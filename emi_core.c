/*
EMI:	embedded message interface
Copyright (C) 2009  david leels <davidontech@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#include <syslog.h>
#include <string.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include "emi.h"
#include "msg_table.h"
#include "shmem.h"
#include "emisocket.h"
#include "emi_semaphore.h"
#include "debug.h"
#include "emi_config.h"


#define coreprt(a) emiprt(a)

#define DAEMONIZE	0x00000001
#define SINGLIZE	0x00000002

struct clone_args{
	struct sk_dpr *sd;
	struct sk_dpr *client_sd;
	void *base;
	struct msg_map **msg_table;
};


int core_shmid=-1;
struct sk_dpr *sd=NULL;
struct sk_dpr *client_sd=NULL;
int lock_fd=-1;
void *emi_base_addr=NULL;
struct msg_map *msg_table[EMI_MAX_MSG];


int emi_recieve_operation(void *args);

void sig_handler(pid_t pid){
	return;
}
void sig_release(pid_t pid){
	if(lock_fd>=0)
		close(lock_fd);
	emi_close(sd);
	emi_close(client_sd);
	if(core_shmid>=0){
		shmctl(core_shmid,IPC_RMID,NULL);
	}
	exit(0);
}

int runing_check(char *name){
	int fd;
	char buf[16];
	struct flock fl;
	if((fd=open(name,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH))<0){
		coreprt("lock file open error\n");
		return -1;
	}
	lock_fd=fd;
	fl.l_type=F_WRLCK;
	fl.l_start=0;
	fl.l_whence=SEEK_SET;
	fl.l_len=0;
	if(fcntl(fd,F_SETLK,&fl)<0){
		coreprt("can not lock\n");
		close(fd);
		return -1;
	}
	if(ftruncate(fd,0)){
		coreprt("ftruncate error\n");
		close(fd);
		return -1;
	}
	sprintf(buf,"%ld",(long)getpid());
	if(write(fd,buf,strlen(buf)+1)<0){
		coreprt("write error\n");
		close(fd);
		return -1;
	}
	return 0;
}

int init_msg_table(struct msg_map *table[]){
	int i;
	for(i=0;i<EMI_MAX_MSG;i++)
		table[i]=NULL;
	return 0;
}



int int_global_shm_space(int pid_max){
	if((core_shmid=shmget(emi_config.emi_key,pid_max*sizeof(int)+sizeof(struct emi_msg)*(EMI_MAX_MSG)+(emi_config.emi_data_size_per_msg)*(EMI_MAX_MSG),IPC_CREAT|IPC_EXCL|0666))<0){
		coreprt("shmget error\n");
		return -1;
	}
	if((emi_base_addr=(void *)shmat(core_shmid,(const void *)0,0))==(void *)-1){
		coreprt("shmat error\n");
		shmctl(core_shmid,IPC_RMID,NULL);
		return -1;
	}
	return 0;
}

eu32 get_pid_max(void){
	int fd,i;
	char buf[8]={0};
	if((fd=open("/proc/sys/kernel/pid_max",O_RDONLY))<0){
		goto error;
	}
	if(read(fd,buf,sizeof(buf))<0){
		close(fd);
		goto error;
	}
	close(fd);
	i=atoi(buf);
	return i;

error:
	perror("dangerous!it seems your system does not mount the proc filesystem yet,so can not get your pid_max number.emi_core would use default ,but this may be different with the value in your system,as a result,may cause incorrect transmission");
	return 32768;
}


void print_usage(void){
	printf("usage:emi_core [-d/-s]\n");
	printf("	by default,emi_core will work at exclusive mode,whitch request root priority.\n\n");
	printf("	-d	run as a daemon\n");
	printf("	-s	run singly,which means different user can run independently without interacting with each other.note that this mode should just be used locally,if you mean to transfer message between machines,run exclusively.meanwhile,this may affect connecting efficiency slightly\n\n");
}


int main(int argc,char **argv){
	struct sigaction sa;
	char lock_name[128];

	int option=0,opt;
	eu32 pid_max;
	int ret;


	while(*++argv!=NULL&&**argv=='-'){
		while((opt=*++*argv)!='\0'){
			switch(opt){
				case 'd':
					option|=DAEMONIZE;
					break;
				case 's':
					option|=SINGLIZE;
					break;
				default:
					option=0;
					print_usage();
					return 0;
			}
		}
	}

	if((!(option&SINGLIZE))&&getuid()){
		print_usage();
		return 0;
	}

	if(init_msg_table(msg_table)){
		coreprt("init msg table error\n");
		return -1;
	}

/*if emi.conf file exists in the directories,get_config will return custom result,or it will return default one*/
	ret=get_config(&emi_config);


	if(option&SINGLIZE){
		if(ret<0){
			/*because normal user can not use the default emi_port(361),so have to get one from emi.conf*/
			coreprt("you choose single mode,but no emi.conf assigned ,this is not permitted.");
			return -1;
		}

		emi_port=emi_config.emi_port;		//for single mode, emi_port could also get from emi_config,the only thing you should matter is emi_config.emi_port should bigger than 1024 for normal user.
		construct_local_lock_name(lock_name);
		if(runing_check(lock_name)){
			coreprt("emi_core may already running\n");
			return -1;
		}
	}else{
		emi_port=emi_config.emi_port;
		if(runing_check(LOCK)){
			coreprt("emi_core may already running\n");
			return -1;
		}
	}

	pid_max=get_pid_max();

	umask(0);

	if(option&DAEMONIZE){
		pid_t pid;
		if((pid=fork())<0){
			coreprt("process fork error\n");
			return -1;
		}else if(pid!=0){
			return 0;
		}
	}

	setsid();



	sa.sa_handler=sig_release;
	sa.sa_flags=0;
	sigfillset(&sa.sa_mask);
	if(sigaction(SIGTERM,&sa,NULL)<0){
		coreprt("sigaction error\n");
	}

	sa.sa_handler=sig_release;
	sa.sa_flags=0;
	sigfillset(&sa.sa_mask);
	if(sigaction(SIGINT,&sa,NULL)<0){
		coreprt("sigaction error\n");
	}


	if(chdir("/")<0){
		coreprt("can not changed dir to /\n");
	}

/*initialize all needed locks*/
	emi_init_locks();

	if(int_global_shm_space(pid_max)){
		coreprt("init shm space error\n");
		return -1;
	}

/*
 *
 *the function is called to alloc a listed structures,which is for managing msg sharing area.
 *
 */
	if(emi_init_msg_space(EMI_MAX_MSG,pid_max*sizeof(int))){
		coreprt("init msg space error\n");
		return -1;
	}


	if((sd=emi_open(AF_INET))==NULL){
		coreprt("emi_open error\n");
		shmctl(core_shmid,IPC_RMID,NULL);
		return -1;
	}

	int yes=1;
    if(setsockopt(sd->d, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		coreprt("setsockopt error");
		return -1;
	}


	if(emi_bind(sd,emi_port)<0){
		coreprt("bind error\n");
		emi_close(sd);
		shmctl(core_shmid,IPC_RMID,NULL);
		return -1;
	}

	if(emi_listen(sd)<0){
		coreprt("listen err\n");
		emi_close(sd);
		shmctl(core_shmid,IPC_RMID,NULL);
		return -1;
	}


	while(1){
		if((client_sd=emi_accept(sd,NULL))==NULL){
			coreprt("emi_accept error\n");
			emi_close(client_sd);
			continue;
		}

		pthread_t tid;
		struct clone_args *arg;

		if((arg=(struct clone_args *)malloc(sizeof(struct clone_args)))==NULL){
			coreprt("mem error\n");
			continue;
		}

		arg->client_sd=client_sd;
		arg->base=emi_base_addr;
		arg->msg_table=msg_table;

		if((ret=pthread_create(&tid,NULL,(void *)emi_recieve_operation,arg))){
			coreprt("pthread cancel error\n");
			continue;
		}

		if((ret=pthread_detach(tid))){
			coreprt("pthread_detach error\n");
			continue;
		}
	}
}

int emi_recieve_operation(void *args){
	int ret,pid_ret=-1;
	struct emi_msg *msg_pos;

/*
 * get an empty area in the share memory for a recieving msg.
*/
	if((msg_pos=emi_obtain_msg_space(((struct clone_args *)args)->base))==NULL){
		coreprt("emi_obtain_msg_space error\n");
		goto e0;
	}
	
/*
 * read the remote msg into this alloced memory
 */
	if((ret=emi_read(((struct clone_args *)args)->client_sd,msg_pos,sizeof(struct emi_msg)))<(sizeof(struct emi_msg))){
		coreprt("emi_read from client error\n");
		goto e0;
	}

	debug_msg(msg_pos,0);

/*
 *	if it is a register msg ,then:
 */
	if(msg_pos->flag&EMI_MSG_CMD_REGISTER){
		struct msg_map p;
		msg_map_fill(&p,msg_pos->msg,msg_pos->src_addr.pid);
		emi_lock(&msg_map_lock);
		emi_hinsert(((struct clone_args *)args)->msg_table,&p);
		emi_unlock(&msg_map_lock);

/*
 *	for EMI_MSG_FLAG_BLOCK_RETURN mode, we should search the whole msg_table to ensure this msg does not been registered before, 
 */
		if(msg_pos->flag&EMI_MSG_FLAG_BLOCK_RETURN){
			struct msg_map *mp;
			int tmpnum=0;
			mp=__emi_hsearch(((struct clone_args *)args)->msg_table,&p,&tmpnum);
			if(mp!=NULL){
				mp->pid=p.pid;
			}
		}

		msg_pos->flag=EMI_MSG_CMD_SUCCEEDED;
//tell the process the registeration result
		if((ret=emi_write(((struct clone_args *)args)->client_sd,msg_pos,sizeof(struct emi_msg)))<(sizeof(struct emi_msg))){
			coreprt("emi_read from client error\n");
		}

//		debug_msg_full_table(((struct clone_args *)args)->msg_table);
		goto e0;
	}else{

/*
 * otherwise, we should operate it carefully. if the msg is a data msg, we should recieve the date first.
 */
		int num;
		struct msg_map p,*m;
		int nth;
		eu32 *pid_num;

		if(msg_pos->flag&EMI_MSG_TYPE_DATA){

			if(msg_pos->size<emi_config.emi_data_size_per_msg){
				if((ret=emi_read(((struct clone_args *)args)->client_sd,msg_pos->data,msg_pos->size))<msg_pos->size){
					coreprt("emi_read from client error\n");
					goto e0;
				}
			}else{
				//we should do something, for example write back a emi_msg hint that the data size exceeds a default one.
			}
		}

		debug_msg(msg_pos,1);

/*
 *	get the offset of the msg in "msg split" area (see develop.txt). this offset will be writed into the BASE_ADDR+pid address afterward, inform the according process to get it.
 */
		nth=obtain_space_msg_num(((struct clone_args *)args)->base,msg_pos);
		
		msg_map_fill(&p,msg_pos->msg,0);

/*
 *		each cycle would find a msg_map associated with corresponding MSG NUMBER.
 */
		for(num=0;;num+=1){
			emi_lock(&msg_map_lock);
			if((m=__emi_hsearch(((struct clone_args *)args)->msg_table,&p,&num))==NULL){
				emi_unlock(&msg_map_lock);
				break;
			}
			emi_unlock(&msg_map_lock);

/*
 * *pid_num is used for passing the number which tells the recievers 
 * where the transfered msg could be found,at the same time, 
 * as a flag judged by emi_core weather the reciever has finished operating it,
 * which means that it is safe for emi_core to unlock and to reuse it again.
 * so it is quite critial.
 *
 * moreover,pid_num must be identified no less than EMI_MAX_MSG messages,
 * thus may be not enough if defined as a char type sometimes.
 * so here we treat as an int.
 *
 */
			/*get process id address in critical shmem index area*/
			pid_num=(eu32 *)((eu32 *)(((struct clone_args *)args)->base)+m->pid);

			emi_lock(&critical_shmem_lock);//FIXME:this is ugly,all process conpete the only lock.

			/*write emi_msg space offset to the index address.*/
			*pid_num=nth;
			pid_ret=kill(m->pid,SIGUSR2);
			if(pid_ret<0){
				*pid_num=0;
			}else{
				msg_pos->count++;
			}

			while(*pid_num){
				sleep(0);
			};

			emi_unlock(&critical_shmem_lock);

			if(pid_ret<0){
				emi_lock(&msg_map_lock);
				emi_hdelete(((struct clone_args *)args)->msg_table,m);
				emi_unlock(&msg_map_lock);
			}
		}

		//target process need to down this count once when he finishes the operation.
		//remember to lock and unlock when down the count.
		while(msg_pos->count){
			sleep(0);
		}

		if(msg_pos->flag&EMI_MSG_FLAG_NONBLOCK){
			goto e0;
		}else if(msg_pos->flag&EMI_MSG_FLAG_BLOCK){
			struct emi_msg cmd;
			cmd.flag=EMI_MSG_CMD_SUCCEEDED;
			if((ret=emi_write(((struct clone_args *)args)->client_sd,&cmd,sizeof(struct emi_msg)))<sizeof(struct emi_msg)){
				goto e0;
			}
		//EMI_MSG_FLAG_BLOCK_RETURN mode, write back return msg and its data
		}else if(msg_pos->flag&EMI_MSG_FLAG_BLOCK_RETURN){

			if(!(msg_pos->flag&EMI_MSG_CMD_SUCCEEDED)){
//				msg_pos->flag&=~EMI_MSG_CMD_SUCCEEDED;
			}else if(msg_pos->size > emi_config.emi_data_size_per_msg){
				msg_pos->flag&=~EMI_MSG_CMD_SUCCEEDED;
			}else{
				msg_pos->flag=EMI_MSG_CMD_SUCCEEDED;
			}


			if((ret=emi_write(((struct clone_args *)args)->client_sd,msg_pos,sizeof(struct emi_msg)))<sizeof(struct emi_msg)){
				goto e0;
			}

			if(msg_pos->flag&EMI_MSG_CMD_SUCCEEDED&&msg_pos->size){
				if((ret=emi_write(((struct clone_args *)args)->client_sd,msg_pos->data,msg_pos->size))<msg_pos->size){
					goto e0;
				}
			}

		}
	}

e0:
	emi_close(((struct clone_args *)args)->client_sd);
	emi_return_msg_space(msg_pos);
	free(args);
	pthread_exit(NULL);
	return ret;

}
