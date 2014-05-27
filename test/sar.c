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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <emiif.h>


int func1(struct emi_msg *tmp){
	printf("1:msg is %d,while cmd is %d,\n",tmp->msg,tmp->cmd);
	if(tmp->flag&EMI_MSG_TYPE_DATA)
		printf("data is %s\n",tmp->data);
}

int func2(struct emi_msg *tmp){
	printf("blocking mode,sleep 5 seconds \n");
	sleep(5);
	printf("1:msg is %d,while cmd is %d,\n",tmp->msg,tmp->cmd);
	if(tmp->flag&EMI_MSG_TYPE_DATA)
		printf("data is %s\n",tmp->data);
}



int main(int argc,char **argv){

	emi_init();

	char opt;
	int option=0;
	int ret;
	int size=0;
	char *datap;
	int block=0;
	unsigned long cmd,msgr,msgs,flag=0;
	unsigned char addr[32]={0};
	struct emi_msg *msg1;
	if(argc==1){
		printf("usage:sar [-b] [-r registermsg] [-s addr [-m msg]] [-d data]/[-c commond]\n");
		return 0;
	}

	while(*++argv!=NULL){
		while(**argv=='-'){
			while((opt=*++*argv)!='\0'){
				switch(opt){
					case 'b':
						block=1;
						break;
					case 'r':
						option|=0x1;
						msgr=atol(*(argv+1));
						break;
					case 's':
						option|=0x2;
						strcpy(addr,*(argv+1));
						break;
					case 'd':
						option|=0x4;
						flag|=EMI_MSG_TYPE_DATA;
						size=strlen(*(argv+1));
						datap=malloc(size);
						if(datap==NULL)
							return -1;
						strcpy(datap,*(argv+1));
						break;
					case 'c':
						option|=0x8;
						flag|=EMI_MSG_TYPE_CMD;
						cmd=atol(*(argv+1));
						break;
					case 'm':
						option|=0x10;
						msgs=atol(*(argv+1));
						break;
					default:
						option=-1;
				}
				break;
			}
		}

	}
	
//	printf("%d %s %d\n",msgr,addr,flag);
	


	
	if(option&1){
		if(block){
			if((ret=emi_msg_register(msgr,func2))<0){
				printf("emi_msg_register error\n");
				return -1;
			}
		}else{
			if((ret=emi_msg_register(msgr,func1))<0){
				printf("emi_msg_register error\n");
				return -1;
			}
		}
			printf("register msg num is %d\n",msgr);
	}
	if(option&2){
		if(option&4){
			while(1){
				char buf;
				scanf("%c",&buf);
				if(buf!=' ')
					continue;
				if((msg1=emi_msg_alloc(size))==NULL){
					printf("emi_msg_alloc error\n");
					return -1;
				}
				if(block){
					printf("block mode\n");
					if((ret=emi_fill_msg(msg1,addr,datap,cmd,msgs,flag|EMI_MSG_FLAG_BLOCK))<0){
						printf("emi_fill_msg error\n");
						return -1;
					}
				}else{
					if((ret=emi_fill_msg(msg1,addr,datap,cmd,msgs,flag|EMI_MSG_FLAG_NONBLOCK))<0){
						printf("emi_fill_msg error\n");
						return -1;
					}
				}
				if((ret=emi_msg_send(msg1))<0){
					printf("emi_send_msg error\n");
					return -1;
				}
				debug_msg(msg1,1);
				printf("send msg %d to %s with the %s datap\n",msgs,addr,datap);
			}
		}else if(option&8){
			while(1){
				char buf;
				scanf("%c",&buf);
				if(buf!=' ')
					continue;
				if((msg1=emi_msg_alloc(0))==NULL){
					printf("emi_msg_alloc error\n");
					return -1;
				}

				if(block){
					printf("block mode\n");
					if((ret=emi_fill_msg(msg1,addr,NULL,cmd,msgs,flag|EMI_MSG_FLAG_BLOCK))<0){
						printf("emi_fill_msg error\n");
						return -1;
					}
				}else{
					if((ret=emi_fill_msg(msg1,addr,NULL,cmd,msgs,flag|EMI_MSG_FLAG_NONBLOCK))<0){
						printf("emi_fill_msg error\n");
						return -1;
					}
				}

				if((ret=emi_msg_send(msg1))<0){
					printf("emi_send_msg error\n");
					return -1;
				}
				debug_msg(msg1,1);
				printf("send msg %d to %s with the %d cmd\n",msgs,addr,cmd);
			}
		}
	}
	while(1)
		pause();
}
