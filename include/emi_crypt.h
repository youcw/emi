#ifndef __EMI_CRYPT_H__
#define __EMI_CRYPT_H__

extern int emi_aes_encrypt(const unsigned char *pt,unsigned char *ct,unsigned char *key);
extern int emi_aes_decrypt(const unsigned char *ct, unsigned char *pt,unsigned char *key);
#endif
