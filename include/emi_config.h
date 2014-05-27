#ifndef __EMI_CONFIG_H__
#define __EMI_CONFIG_H__
#include "msg_table.h"
#include "shmem.h"

#define EMI_MAX_MSG	EMI_HASH_MASK
#define EMI_DATA_SIZE_PER_MSG	(1024)
#define EMI_MAX_DATA	(EMI_MAX_MSG/5)
#define EMI_PORT	361

struct emi_config{
	eu32 emi_port;
	eu32 emi_data_size_per_msg;
	eu32 emi_key;
};

extern struct emi_config emi_config;

extern int get_config(struct emi_config *config);
#endif
