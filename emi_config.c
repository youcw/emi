#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "emi.h"
#include "emi_config.h"
#include "debug.h"


/*global emi_config using default value,
 */
/**
 *some parameters may not suit for user configuration using config files.for example,emi_port should use a unique value especially for remote connection.another example is emi_data_size_per_msg,this parameter defines the max data size in each massage,if two sides use different parameter,it may cause data transmit error.so be causion with these parameters.

the obsolete parameter emi_max_data is defined for max data ,because new design append massage data space for each massage, so it will be not used again.

 */
struct emi_config emi_config={
	.emi_port=EMI_PORT,
	.emi_data_size_per_msg=EMI_DATA_SIZE_PER_MSG,
	.emi_key=EMI_KEY,
};

#ifdef DEBUG
void debug_config(struct emi_config *config){
	printf("config->emi_port=%d\n",config->emi_port);
//	printf("config->emi_max_msg=%x\n",config->emi_max_msg);
	printf("config->emi_data_size_per_msg=%d\n",config->emi_data_size_per_msg);
	printf("config->emi_key=%x\n",config->emi_key);
}
#endif

void set_default_config(struct emi_config *config){
	config->emi_port=EMI_PORT;
//	config->emi_max_msg=EMI_MAX_MSG;
	config->emi_data_size_per_msg=EMI_DATA_SIZE_PER_MSG;
	config->emi_key=EMI_KEY;
		return;
}

int get_config(struct emi_config *config){
	int fd;
	int i,j;
	char *p;
	char stack[256];
	struct stat sb;
	char *addr;

	char *terms[]={"EMI_PORT",
					"EMI_DATA_SIZE_PER_MSG",
					"EMI_KEY"};
	char *name[]={"emi.conf","$HOME/.emi.conf","$HOME/emi.conf","/etc/emi.conf"};

	set_default_config(config);

	for(i=0;i<sizeof(name)/sizeof(char *);i++){
		if((fd=open(name[i],O_RDONLY))>0)
			break;
	}

	if(fd<0||(fstat(fd,&sb)<0)){
		return -1;
	}

	if((addr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0))==NULL){
		return -1;
	}


	for(p=addr;p-addr<sb.st_size;p++){
		if(*p=='#'||*p=='\n'){
			for(;*p!='\n';p++);
		}
		for(i=0;i<sizeof(terms)/sizeof(char *);i++){
			if(!strncmp(terms[i],p,strlen(terms[i]))){
				p=p+strlen(terms[i]);
				memset(stack,0,sizeof(stack));
				for(j=0;*p!='#'&&*p!='\n';p=(*p=='='?p+1:p),stack[j++]=*p++);
					*((int *)(config)+i)=atoi(stack);
			}
		}
	}
	munmap(addr,sb.st_size);
	close(fd);

	return 0;
}
