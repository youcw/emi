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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "emi.h"
#include "msg_table.h"


struct msgandnum{
	char msg[128];
	eu32 num;
};

struct msgandnum msgtonum[]={
	[0]={
		.msg="EMI_MSG_TYPE_CMD",
		.num=0x00000001,
		},
	
	[1]={
		.msg="EMI_MSG_TYPE_DATA",
		.num=0x00000002,
		},
	[2]={.msg="EMI_MSG_TYPE_BIN",
		.num=0x00000004,
		},
	[3]={
		.msg="EMI_MSG_FLAG_UNIDIRECTION",
		.num=0x00000010,
		},
	[4]={
		.msg="EMI_MSG_FLAG_BIDIRECTION",
		.num=0x00000020,
		},
	[5]={
		.msg="EMI_MSG_FLAG_LOCAL",
		.num=0x00000040,
		},
	[6]={
		.msg="EMI_MSG_FLAG_REMOTE",
		.num=0x00000080,
		},
	[7]={
		.msg="EMI_MSG_FLAG_BLOCK",
		.num=0x00000100,
		},
	[8]={
		.msg="EMI_MSG_FLAG_BLOCK_RETURN",
		.num=0x00000200,
		},
	[9]={
		.msg="EMI_MSG_FLAG_NONBLOCK",
		.num=0x00000400,
		},
	[10]={
		.msg="EMI_MSG_FLAG_CREATE",
		.num=0x00002000,
		},
	[11]={
		.msg="EMICORE_CMD_REGISTER",
		.num=0x00004000,
		},
	[12]={
		.msg="EMICORE_CMD_GET",
		.num=0x00008000,
		},
	[13]={
		.msg="EMI_MSG_CMD_SUCCEEDED",
		.num=0x00010000,
		},
	[14]={
		.msg="EMI_MSG_CMD_FAILED",
		.num=0x00020000,
		},
};




#define ARRAY_SIZE(array)	(sizeof(array)/sizeof(array[0]))

void debug_flag(eu32 flag){
	int i,j;
	for(i=0,j=0;i<ARRAY_SIZE(msgtonum);i++){
		if(flag&(msgtonum[i].num)){
			j++;
			printf("		flag include %s,%X\n",msgtonum[i].msg,msgtonum[i].num);
		}
	}
	if(!j)
		printf("no flag matched\n");
	
}


void debug_addr(struct emi_addr *addr,char *p){
	printf("	%s debuging:	",p);
	printf("address=%lX, ",(long)addr);
	printf("ip=%s, ",inet_ntoa(addr->ipv4.sin_addr));
	printf("port=%d, ",ntohs(addr->ipv4.sin_port));
	printf("pid=%d\n",addr->pid);

}


void debug_msg(struct emi_msg *msg,int more){
	printf("emi_msg debuging\n");
	printf(" emi_msg address is %lX\n",(long)msg);

	if(!more)
		return;

	debug_addr(&msg->src_addr,"src_addr");
//	printf("%s\n",inet_ntoa((msg->dest_addr->ipv4.sin_addr)));
//	if(msg->dest_addr!=NULL)
//		debug_addr(msg->dest_addr,"dest_addr");

	printf("msg->size is %d\n",msg->size);
	if(msg->size){
		if(!(msg->flag&EMI_MSG_TYPE_DATA))
			printf("warning:!!!!!!!!!!!msg flag unmatched\n");
		printf(" date size is %d,content is %s:\n",msg->size,msg->data);
		int i;

		for(i=0;i<msg->size;i++){
			printf("%d ",msg->data[i]);
		}
	}
	if(msg->cmd){
		if(!msg->flag&EMI_MSG_TYPE_CMD)
			printf("warning:!!!!!!!!!!!msg flag unmatched\n");
		printf(" emi_msg cmd is %X\n",msg->cmd);
	}
	printf(" msg is %X\n",msg->msg);
	printf(" flag=%X\n",msg->flag);

	debug_flag(msg->flag);
	return;
}

void debug_single_map(struct msg_map *map){
	if(map!=NULL)
		printf("	single_map debuging:msg=%X,pid=%d,next=%lX\n",map->msg,map->pid,(long)map->next);
	else
		printf("	single_map is NULL\n");
	return;
}
void debug_msg_map(struct msg_map **table,struct msg_map *map){
	eu32 i;
	struct msg_map *tmp;
	printf("msg_map debugging:\n");
	i=emi_hash(map);
	printf("	hash value is %d\n",i);
	if((tmp=emi_hsearch(table,map))==NULL){
		printf("	map is not in the table\n");
	}else{
		debug_single_map(tmp);
	}
}

void debug_msg_chain(struct msg_map **table,struct msg_map *map){
	printf("\n");
	int j;
	struct msg_map *tmp;
	printf("msg_chain debugging:\n");
	for(j=0,tmp=(*(table+emi_hash(map)));tmp!=NULL;tmp=tmp->next,j++){
		printf("	chan %d: ",j);
		debug_single_map(tmp);
	}
	printf("\n");
}

void debug_msg_table(struct msg_map **table){
	int i,j;
	struct msg_map *tmp;
	
	printf("printing msg_table\n");
	for(i=0;i<EMI_HASH_MASK;i++){
			printf("  line %d",i);
			for(j=0,tmp=(*(table+i));tmp!=NULL;tmp=tmp->next,j++){
				printf("\n		chan %d: ",j);
				debug_single_map(tmp);
			}
			printf("\n");
	}
	return;
}

void debug_msg_full_table(struct msg_map **table){
	int i,j;
	struct msg_map *tmp;
	
	printf("printing full msg_table\n");
	for(i=0;i<EMI_HASH_MASK;i++){
		if(*(table+i)!=NULL){
			printf("  line %d",i);
			for(j=0,tmp=(*(table+i));tmp!=NULL;tmp=tmp->next,j++){
				printf("\n		chan %d: ",j);
				debug_single_map(tmp);
			}
			printf("\n");
		}else{
			continue;
		}
	}
	return;
}

