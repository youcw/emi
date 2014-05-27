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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <errno.h>
#include "emi.h"
#include "emisocket.h"
#include "shmem.h"
#include "list.h"
#include "emi_semaphore.h"
#include "emi_crypt.h"
#include "emi_config.h"

#include "debug.h"
#ifdef DEBUG
#else
#define emiprt(a)
#endif


#ifdef BLUETOOTH
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#endif

struct emi_global{
	int shm_id;
	int sem_id;
	int urandom_fd;
}emi_global;

struct func_list{
	struct list_head list;
	emi_func func;
	eu32 msg;
	void *data;
};

LIST_HEAD(__func_list);


struct emi_addr *emi_addr_alloc(){
	struct emi_addr *addr;
	if((addr=(struct emi_addr *)malloc(sizeof(struct emi_addr)))==NULL){
		goto error;
	}
	memset(addr,0,sizeof(struct emi_addr));

	return addr;
error:
	return NULL;
}

void emi_addr_free(struct emi_addr *addr){

	free(addr);
	return;
}



struct emi_msg *emi_msg_alloc(eu32 size){

	struct emi_msg *msg;
	if((msg=(struct emi_msg *)malloc(sizeof(struct emi_msg)+size))==NULL){
		goto error;
	}
	memset(msg,0,sizeof(struct emi_msg));
	msg->size=size;
	/*
	if(size){
		if((msg->data=(void *)malloc(size))==NULL){
			goto freemsg;
		}
	}
	*/
/*
 * we can use the create parameter to alloc data just append after the emi_msg struct.
	if(create){
		if((msg->dest_addr=emi_addr_alloc())==NULL){
			goto freemsg;
		}
		msg->flag|=EMI_MSG_FLAG_CREATE;
	}
*/
	return msg;

//freemsg:
//	free(msg);
error:
	return NULL;
}

void emi_msg_free(struct emi_msg *msg){
/*
	if(msg->flag&EMI_MSG_FLAG_CREATE)
		emi_addr_free(msg->dest_addr);
*/
/*
	if(msg->size)
		free(msg->data);
*/
	free(msg);
	return;
}

int emi_fill_addr(struct emi_addr *addr,char *ip){
	if(((addr)->ipv4.sin_addr.s_addr=inet_addr(ip))==-1)
		return -1;
//	if(strcmp(ip,"127.0.0.1")||strcmp(ip,"localhost"))
//		(addr)->ipv4.sin_port=htons(GET_PORT);
//	else
		(addr)->ipv4.sin_port=htons(emi_config.emi_port);
	(addr)->ipv4.sin_family=AF_INET;
	(addr)->pid=getpid();
	return 0;
}

struct emi_msg *emi_obtain_msg(char *dest_ip,void *data,eu32 size,eu32 cmd,eu32 defined_msg,eu32 flag){
	struct emi_msg *msg;

	if((msg=emi_msg_alloc(size))==NULL){
		emiprt("emi_obtain_msg error\n");
		return NULL;
	}
	if(emi_fill_addr(&(msg->src_addr),"127.0.0.1"))
		return NULL;
	if(dest_ip!=NULL){
		if(emi_fill_addr(&(msg->dest_addr),dest_ip))
			return NULL;
	}
	if(data!=NULL){
		memcpy(msg->data,data,msg->size);
		(msg)->flag|=EMI_MSG_TYPE_DATA;
	}
	if(cmd){
		(msg)->cmd=cmd;
		(msg)->flag|=EMI_MSG_TYPE_CMD;
	}
	if(defined_msg)
		(msg)->msg=defined_msg;

	(msg)->flag|=flag;

	return msg;
};


int emi_fill_msg(struct emi_msg *msg,char *dest_ip,void *data,eu32 cmd,eu32 defined_msg,eu32 flag){
	struct emi_addr *p=&(msg)->src_addr;
	if(emi_fill_addr(p,"127.0.0.1"))
		return -1;
	if(dest_ip!=NULL){
		if(emi_fill_addr(&(msg->dest_addr),dest_ip))
			return -2;
	}
	if(data!=NULL){
//the massage could be zero,do not deem it as the end of the massage.
//		eu32 n=strlen(data);
//		n=(n<msg->size)?n:msg->size;
		memcpy((msg)->data,data,msg->size);
	}
	if(cmd)
		(msg)->cmd=cmd;
	if(defined_msg)
		(msg)->msg=defined_msg;

	(msg)->flag|=flag;
	return 0;
}

/*
*/

void func_sterotype(int no_use){
	int id,nth,pid;
	struct emi_msg *shmsg,*baseshmsg;
	struct list_head *lh;

	id=emi_global.shm_id;

	if((baseshmsg=(struct emi_msg *)shmat(id,(const void *)0,0))==(void *)-1){
		emiprt("shmat error\n");
		exit(-1);		//error!!!!!!!!!!
	}

	pid=getpid();
	nth=*((eu32 *)baseshmsg+pid);

	shmsg=(struct emi_msg *)((char *)baseshmsg+nth);

	//this address is the pid_num in emi_core.it should be changed as soon as possable.emi_core will wait the zero to make sure the this process recieved the signal ,and send the same signal to other process that registered the same massage.
	*(eu32 *)((eu32 *)baseshmsg+pid)=0;


	list_for_each(lh,&__func_list){
		struct func_list *fl;
		fl=container_of(lh,struct func_list,list);
		if(fl->msg==shmsg->msg){

			if(shmsg->flag&EMI_MSG_FLAG_BLOCK_RETURN){
				int ret;
				ret=(fl->func)(shmsg);
//in BLOCK_RETURN mode, if function return with error,then msg->flag must be set to FAILED to ensure the emi_core return failed result to the sender.
				if(ret){
					shmsg->flag&=~EMI_MSG_CMD_SUCCEEDED;
				}else{
					shmsg->flag|=EMI_MSG_CMD_SUCCEEDED;
				}
			//should break here in case user use a emi_msg_register function to register a blockreturn message.
				break;
			}else{
				(fl->func)(shmsg);
			}

#ifdef	EXCLUSIVE_MSG
			break;
#endif
		}
	}


	//FIXME:lock
	shmsg->count--;
	//FIXME:unlock


	if(shmdt(baseshmsg)){
		emiprt("shmdt error\n");
		return;
	}

	return;
}


int emi_msg_send(struct emi_msg *msg){
	struct sk_dpr *sd;
	int ret=0;
	if((!((msg->flag)&EMI_MSG_TYPE_DATA))&&(!((msg->flag)&EMI_MSG_TYPE_CMD))){
		dbg("the flag must either match DATA or CMD or both\n");
		return -1;
	}
	if((msg->flag&EMI_MSG_TYPE_DATA)&&(msg->data==NULL)){
		dbg("flag tells the msg includes data,but msg->data pointer is NULL\n");
		return -1;
	}
	if((sd=emi_open(AF_INET))==NULL){
		dbg("emi_open error\n");
		return -1;
	}
	if((ret=emi_connect(sd,&(msg->dest_addr),1))<0){
		dbg("remote connected error\n");
		emi_close(sd);
		return -1;
	}
	
	if((ret=emi_write(sd,(void *)msg,sizeof(struct emi_msg)))<sizeof(struct emi_msg)){
		dbg("block mode:error when emi_write to remote daemon\n");
		emi_close(sd);
		return -1;
	}

	if(msg->flag&EMI_MSG_TYPE_DATA){
		if((ret=emi_write(sd,msg->data,msg->size))<msg->size){
			dbg("nonblock mode:DATA:emi_write to remote prcess local data with error\n");
			ret=-1;
		}else{
			ret=0;
		}
	}else if(msg->flag&EMI_MSG_TYPE_CMD){
		ret=0;
	}

		//waiting for optimizing
	if(msg->flag&EMI_MSG_FLAG_BLOCK){
		struct emi_msg cmd={
								.flag=0,
							};
		if((ret=emi_read(sd,&cmd,sizeof(struct emi_msg)))<sizeof(struct emi_msg)){
			dbg("block mode:CMD:emi_read from remote process error\n");
			ret=-1;
		}
		if(cmd.flag&EMI_MSG_CMD_SUCCEEDED){
			ret=0;
		}else{
			dbg("block mode:CMD:remote process returned with error in block mode\n");
			ret=-1;
		}

	}else if(msg->flag&EMI_MSG_FLAG_BLOCK_RETURN){

		if((ret=emi_read(sd,msg,sizeof(struct emi_msg)))<sizeof(struct emi_msg)){
			dbg("block mode:CMD:emi_read from remote process error\n");
			ret=-1;
		}
		if(msg->flag&EMI_MSG_CMD_SUCCEEDED){
			if(msg->size){
				if((ret=emi_read(sd,msg->data,msg->size))<msg->size){
					dbg("block mode:CMD:emi_read from remote process error\n");
					ret=-1;
				}
			}
			ret=0;
		}else{
			dbg("block mode:CMD:remote process returned with error in block return node\n");
			ret=-1;
		}
	}

	emi_close(sd);
	return ret;
}

static inline int emi_msg_send_highlevel(char *ipaddr, int msgnum,int send_size,char *send_data, int ret_size,char *ret_data, eu32 cmd,eu32 flags){
	int size=send_size>ret_size?send_size:ret_size;
	struct emi_msg *msg=emi_msg_alloc(size);									
	if(msg==NULL){
		return -1;
	}

	msg->size=send_size;
	emi_fill_msg(msg,ipaddr,send_data,cmd,msgnum,flags);			
	if(emi_msg_send(msg)){
		goto error;												
	}

	if(ret_data&&msg->size)
		memcpy(ret_data,msg->data,msg->size);

	emi_msg_free(msg);												
	return 0;
	error:															
		emi_msg_free(msg);											
	return -1;
}	


int emi_msg_send_highlevel_blockreturn(char *ipaddr, int msgnum,int send_size,void *send_data, int ret_size,void *ret_data, eu32 cmd){
	eu32 flags;
	if(send_data)																		
		flags=EMI_MSG_TYPE_CMD|EMI_MSG_FLAG_BLOCK_RETURN|EMI_MSG_TYPE_DATA;
	else																		
		flags=EMI_MSG_TYPE_CMD|EMI_MSG_FLAG_BLOCK_RETURN;
	
	return emi_msg_send_highlevel(ipaddr,msgnum,send_size,send_data,ret_size,ret_data,cmd,flags);
}

int emi_msg_send_highlevel_block(char *ipaddr, int msgnum,int send_size,void *send_data, eu32 cmd){
	eu32 flags;
	if(send_data)																		
		flags=EMI_MSG_TYPE_CMD|EMI_MSG_FLAG_BLOCK|EMI_MSG_TYPE_DATA;
	else																		
		flags=EMI_MSG_TYPE_CMD|EMI_MSG_FLAG_BLOCK;
	
	return emi_msg_send_highlevel(ipaddr,msgnum,send_size,send_data,0,NULL,cmd,flags);
}
	
int emi_msg_send_highlevel_nonblock(char *ipaddr, int msgnum,int send_size,void *send_data, eu32 cmd){
	eu32 flags;
	if(send_data)																		
		flags=EMI_MSG_TYPE_CMD|EMI_MSG_FLAG_NONBLOCK|EMI_MSG_TYPE_DATA;
	else																		
		flags=EMI_MSG_TYPE_CMD|EMI_MSG_FLAG_NONBLOCK;
	
	return emi_msg_send_highlevel(ipaddr,msgnum,send_size,send_data,0,NULL,cmd,flags);
}


int emi_msg_prepare_return_data(struct emi_msg *msg,void *data,eu32 size){
	if(size>emi_config.emi_data_size_per_msg)
		return -1;
	msg->size=size;
	memcpy(msg->data,data,size);
	return 0;
}


int emi_msg_decode_aes(struct emi_msg *msg,unsigned char *key){
	unsigned char *tmp,c[16]={0};
	int i,j;

	tmp=(unsigned char *)msg->data+msg->size-sizeof(c);
	if(emi_aes_decrypt(tmp,c,key)<0){
		return -1;
	}
	memset(tmp,0,sizeof(c));
	
	tmp=(unsigned char *)msg->data;
	msg->size-=sizeof(c);
	for(i=0,j=0;i<msg->size;i++,j=j<sizeof(c)?(j+1):0){
		*tmp+++=c[j];
	}

	return 0;
}

int emi_msg_encode_aes(struct emi_msg *msg,unsigned char *key){
	unsigned char p[16],*tmp;//,c[16]={0};
	int i,j;

	if(read(emi_global.urandom_fd,p,sizeof(p))<0){
		emiprt("read random error\n");
		return -1;
	}

	tmp=(unsigned char *)msg->data;
	for(i=0,j=0;i<msg->size;i++,j=j<sizeof(p)?(j+1):0){
		*tmp++-=p[j];
	}

//	msg->size+=sizeof(p);
//	if((msg->data=realloc(msg->data, msg->size))==NULL){
//		return -1;
//	}FIXME
	
	tmp=(unsigned char *)msg->data+msg->size-sizeof(p);
	if(emi_aes_encrypt(p,tmp,key)<0){
		return -1;
	}

	return 0;
}


static int __emi_msg_register(eu32 defined_msg,emi_func func,eu32 flag){
	struct sk_dpr *sd;
	int ret;
	struct sigaction act, old_act;
	struct func_list *fl;
	struct emi_msg cmd;
	cmd.flag=flag;

	if((sd=emi_open(AF_INET))==NULL){
		dbg("emi_open error\n");
		return -1;
	}
	memset(&cmd,0,sizeof(struct emi_msg));
	if((ret=emi_fill_msg(&cmd,NULL,NULL,0,defined_msg,EMI_MSG_CMD_REGISTER))<0){
		emi_close(sd);
		dbg("emi_fill_msg error\n");
		return -1;
	}
	
	if((ret=emi_connect(sd,&cmd.src_addr,1))<0){
		emi_close(sd);
		dbg("emi_connect error\n");
		return -1;
	}
	if((ret=emi_write(sd,(void *)&cmd,sizeof(struct emi_msg)))<sizeof(struct emi_msg)){
		emi_close(sd);
		dbg("emi_write error\n");
		return -1;
	}
	if((ret=emi_read(sd,(void *)&cmd,sizeof(struct emi_msg)))<sizeof(struct emi_msg)){
		emi_close(sd);
		dbg("emi_read register back error\n");
		return -1;
	}
	if(!(cmd.flag&EMI_MSG_CMD_SUCCEEDED)){
		return -1;
	}

#ifdef	EXCLUSIVE_MSG
	struct list_head *lh;
	list_for_each(lh,&__func_list){
		struct func_list *lfl;
		lfl=container_of(lh,struct func_list,list);
		if(defined_msg==lfl->msg){
			lfl->func=func;
			return 1;
		}
	}
#endif

	if((fl=(struct func_list *)malloc(sizeof(struct func_list)))==NULL){
		dbg("malloc error\n");
		return -ENOMEM;
	}
	memset(fl,0,sizeof(struct func_list));
	fl->func=func;
	fl->msg=defined_msg;
//	INIT_LIST_HEAD(&fl->list);
	list_add(&fl->list,&__func_list);

	act.sa_handler=func_sterotype;
	sigfillset(&act.sa_mask);
	sigdelset(&act.sa_mask,SIGUSR2);
//	act.sa_flags=SA_INTERRUPT;
	act.sa_flags= SA_RESTART;
	if((ret=sigaction(SIGUSR2,&act,&old_act))<0){
		dbg("sigaction error\n");
		return ret;
	}
	return 0;
}

int emi_msg_register(eu32 defined_msg,emi_func func){
	return __emi_msg_register(defined_msg,func,0);
}

int emi_msg_register_exclusive(eu32 defined_msg,emi_func func){
	return __emi_msg_register(defined_msg,func,EMI_MSG_FLAG_BLOCK_RETURN);
}

void emi_msg_unregister(eu32 defined_msg,emi_func func){
	struct list_head *lh;
	list_for_each(lh,&__func_list){
		struct func_list *fl;
		fl=container_of(lh,struct func_list,list);
		if(NULL==func){
			if(defined_msg==fl->msg){
				list_del(lh);
				free(fl);
			}
		}else{
			if((defined_msg==fl->msg)&&(func==fl->func)){
				list_del(lh);
				free(fl);
				return;
			}
		}
	}
	return;
	
}

/*
 *emi_init must be used before recieving process.it uses the emi_config struct, which may requre emi config file.
 *
 * */
int emi_init(void){
	get_config(&emi_config);
	if((emi_global.shm_id=shmget(emi_config.emi_key,0,0))<0){
		emiprt("shmget error\n");
		return -1;
	}
	if((emi_global.urandom_fd=open("/dev/urandom",O_RDONLY))<0){
		emiprt("urandom fd open error\n");
		return -1;
	}

	return 0;
}
