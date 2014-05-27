#include <stdlib.h>
#include <string.h>
#include "emi_aes.h"

#define ulong32 unsigned long
//typedef unsigned long ulong32


#define RORc(x,y) \
	(((((unsigned long)(x)&0xFFFFFFFFUL)>> \
	(unsigned long)((y)&31)) | \
	((unsigned long)(x)<< \
	(unsigned long)(32-((y)&31))))&0xFFFFFFFFUL)


#define byte(x,n)	(((x)>>(8*(n)))&255)

#define STORE32H(x, y)	{\
					(y)[0] = (unsigned char)(((x)>>24)&255);	\
					(y)[1] = (unsigned char)(((x)>>16)&255);	\
	        		(y)[2] = (unsigned char)(((x)>>8)&255);		\
					(y)[3] = (unsigned char)((x)&255);\
					}

#define LOAD32H(x, y)	{\
					x = ((unsigned long)((y)[0] & 255)<<24) | 	\
						((unsigned long)((y)[1] & 255)<<16) | 	\
						((unsigned long)((y)[2] & 255)<<8)  | 	\
						((unsigned long)((y)[3] & 255)); 	\
						}


unsigned long setup_mix(unsigned long temp){
   return	(Te4_3[byte(temp, 2)]) ^
   			(Te4_2[byte(temp, 1)]) ^
			(Te4_1[byte(temp, 0)]) ^
			(Te4_0[byte(temp, 3)]);
}



static int schedule_key(const unsigned char *key, int keylen, ulong32 *ekey,ulong32 *dkey){
    int i, j, nr;
    ulong32  temp, *rk;

    ulong32  *rrk;
  
    if (keylen != 16 && keylen != 24 && keylen != 32) {
       return -1;
    }
    
    
    nr = 10 + ((keylen/8)-2)*2;


    /* setup the forward key */
    i                 = 0;
    rk                = ekey;

    LOAD32H(rk[0], key     );
    LOAD32H(rk[1], key +  4);
    LOAD32H(rk[2], key +  8);
    LOAD32H(rk[3], key + 12);

    if (keylen == 16) {
        j = 44;
        for (;;) {
            temp  = rk[3];
            rk[4] = rk[0] ^ setup_mix(temp) ^ rcon[i];
            rk[5] = rk[1] ^ rk[4];
            rk[6] = rk[2] ^ rk[5];
            rk[7] = rk[3] ^ rk[6];
            if (++i == 10) {
               break;
            }
            rk += 4;
        }
    } else if (keylen == 24) {
        j = 52;   
        LOAD32H(rk[4], key + 16);
        LOAD32H(rk[5], key + 20);
        for (;;) {

            temp = rk[5];

            rk[ 6] = rk[ 0] ^ setup_mix(temp) ^ rcon[i];
            rk[ 7] = rk[ 1] ^ rk[ 6];
            rk[ 8] = rk[ 2] ^ rk[ 7];
            rk[ 9] = rk[ 3] ^ rk[ 8];
            if (++i == 8) {
                break;
            }
            rk[10] = rk[ 4] ^ rk[ 9];
            rk[11] = rk[ 5] ^ rk[10];
            rk += 6;
        }
    } else if (keylen == 32) {
        j = 60;
        LOAD32H(rk[4], key + 16);
        LOAD32H(rk[5], key + 20);
        LOAD32H(rk[6], key + 24);
        LOAD32H(rk[7], key + 28);
        for (;;) {

            temp = rk[7];

            rk[ 8] = rk[ 0] ^ setup_mix(temp) ^ rcon[i];
            rk[ 9] = rk[ 1] ^ rk[ 8];
            rk[10] = rk[ 2] ^ rk[ 9];
            rk[11] = rk[ 3] ^ rk[10];
            if (++i == 7) {
                break;
            }
            temp = rk[11];
            rk[12] = rk[ 4] ^ setup_mix(RORc(temp, 8));
            rk[13] = rk[ 5] ^ rk[12];
            rk[14] = rk[ 6] ^ rk[13];
            rk[15] = rk[ 7] ^ rk[14];
            rk += 8;
        }
    } else {
       /* this can't happen */
       return -1;
    }

if(dkey!=NULL){
    /* setup the inverse key now */
    rk   = dkey;
    rrk  = ekey + j - 4; 
    
    /* apply the inverse MixColumn transform to all round keys but the first and the last: */
    /* copy first */
    *rk++ = *rrk++;
    *rk++ = *rrk++;
    *rk++ = *rrk++;
    *rk   = *rrk;
    rk -= 3; rrk -= 3;
    
    for (i = 1; i < nr; i++) {
        rrk -= 4;
        rk  += 4;

        temp = rrk[0];
        rk[0] =
            Tks0[byte(temp, 3)] ^
            Tks1[byte(temp, 2)] ^
            Tks2[byte(temp, 1)] ^
            Tks3[byte(temp, 0)];
        temp = rrk[1];
        rk[1] =
            Tks0[byte(temp, 3)] ^
            Tks1[byte(temp, 2)] ^
            Tks2[byte(temp, 1)] ^
            Tks3[byte(temp, 0)];
        temp = rrk[2];
        rk[2] =
            Tks0[byte(temp, 3)] ^
            Tks1[byte(temp, 2)] ^
            Tks2[byte(temp, 1)] ^
            Tks3[byte(temp, 0)];
        temp = rrk[3];
        rk[3] =
            Tks0[byte(temp, 3)] ^
            Tks1[byte(temp, 2)] ^
            Tks2[byte(temp, 1)] ^
            Tks3[byte(temp, 0)];
     
    }

    /* copy last */
    rrk -= 4;
    rk  += 4;
    *rk++ = *rrk++;
    *rk++ = *rrk++;
    *rk++ = *rrk++;
    *rk   = *rrk;
}
    return 0;
}





static int aes_encrypt(const unsigned char *pt, unsigned char *ct, ulong32 *ekey,int nr)
{
    ulong32 s0, s1, s2, s3, t0, t1, t2, t3, *rk;
    int  r;
   
    
    rk = ekey;
    
    /*
     * map byte array block to cipher state
     * and add initial round key:
     */

    LOAD32H(s0, pt      ); s0 ^= rk[0];
    LOAD32H(s1, pt  +  4); s1 ^= rk[1];
    LOAD32H(s2, pt  +  8); s2 ^= rk[2];
    LOAD32H(s3, pt  + 12); s3 ^= rk[3];

    /*
     * Nr - 1 full rounds:
     */
    r = nr >> 1;
    for (;;) {
        t0 =
            Te0(byte(s0, 3)) ^
            Te1(byte(s1, 2)) ^
            Te2(byte(s2, 1)) ^
            Te3(byte(s3, 0)) ^
            rk[4];
        t1 =
            Te0(byte(s1, 3)) ^
            Te1(byte(s2, 2)) ^
            Te2(byte(s3, 1)) ^
            Te3(byte(s0, 0)) ^
            rk[5];
        t2 =
            Te0(byte(s2, 3)) ^
            Te1(byte(s3, 2)) ^
            Te2(byte(s0, 1)) ^
            Te3(byte(s1, 0)) ^
            rk[6];
        t3 =
            Te0(byte(s3, 3)) ^
            Te1(byte(s0, 2)) ^
            Te2(byte(s1, 1)) ^
            Te3(byte(s2, 0)) ^
            rk[7];

        rk += 8;
        if (--r == 0) {
            break;
        }

        s0 =
            Te0(byte(t0, 3)) ^
            Te1(byte(t1, 2)) ^
            Te2(byte(t2, 1)) ^
            Te3(byte(t3, 0)) ^
            rk[0];
        s1 =
            Te0(byte(t1, 3)) ^
            Te1(byte(t2, 2)) ^
            Te2(byte(t3, 1)) ^
            Te3(byte(t0, 0)) ^
            rk[1];
        s2 =
            Te0(byte(t2, 3)) ^
            Te1(byte(t3, 2)) ^
            Te2(byte(t0, 1)) ^
            Te3(byte(t1, 0)) ^
            rk[2];
        s3 =
            Te0(byte(t3, 3)) ^
            Te1(byte(t0, 2)) ^
            Te2(byte(t1, 1)) ^
            Te3(byte(t2, 0)) ^
            rk[3];
    }

    /*
     * apply last round and
     * map cipher state to byte array block:
     */
    s0 =
        (Te4_3[byte(t0, 3)]) ^
        (Te4_2[byte(t1, 2)]) ^
        (Te4_1[byte(t2, 1)]) ^
        (Te4_0[byte(t3, 0)]) ^
        rk[0];
    STORE32H(s0, ct);
    s1 =
        (Te4_3[byte(t1, 3)]) ^
        (Te4_2[byte(t2, 2)]) ^
        (Te4_1[byte(t3, 1)]) ^
        (Te4_0[byte(t0, 0)]) ^
        rk[1];
    STORE32H(s1, ct+4);
    s2 =
        (Te4_3[byte(t2, 3)]) ^
        (Te4_2[byte(t3, 2)]) ^
        (Te4_1[byte(t0, 1)]) ^
        (Te4_0[byte(t1, 0)]) ^
        rk[2];
    STORE32H(s2, ct+8);
    s3 =
        (Te4_3[byte(t3, 3)]) ^
        (Te4_2[byte(t0, 2)]) ^
        (Te4_1[byte(t1, 1)]) ^
        (Te4_0[byte(t2, 0)]) ^ 
        rk[3];
    STORE32H(s3, ct+12);

    return 0;
}


static int aes_decrypt(const unsigned char *ct, unsigned char *pt, ulong32 *dkey, int nr)
//symmetric_key *skey)
{
    ulong32 s0, s1, s2, s3, t0, t1, t2, t3, *rk;
    int r;

     rk = dkey;


    /*
     * map byte array block to cipher state
     * and add initial round key:
     */
    LOAD32H(s0, ct      ); s0 ^= rk[0];
    LOAD32H(s1, ct  +  4); s1 ^= rk[1];
    LOAD32H(s2, ct  +  8); s2 ^= rk[2];
    LOAD32H(s3, ct  + 12); s3 ^= rk[3];

    /*
     * Nr - 1 full rounds:
     */
    r = nr >> 1;
    for (;;) {

        t0 =
            Td0(byte(s0, 3)) ^
            Td1(byte(s3, 2)) ^
            Td2(byte(s2, 1)) ^
            Td3(byte(s1, 0)) ^
            rk[4];
        t1 =
            Td0(byte(s1, 3)) ^
            Td1(byte(s0, 2)) ^
            Td2(byte(s3, 1)) ^
            Td3(byte(s2, 0)) ^
            rk[5];
        t2 =
            Td0(byte(s2, 3)) ^
            Td1(byte(s1, 2)) ^
            Td2(byte(s0, 1)) ^
            Td3(byte(s3, 0)) ^
            rk[6];
        t3 =
            Td0(byte(s3, 3)) ^
            Td1(byte(s2, 2)) ^
            Td2(byte(s1, 1)) ^
            Td3(byte(s0, 0)) ^
            rk[7];

        rk += 8;
        if (--r == 0) {
            break;
        }


        s0 =
            Td0(byte(t0, 3)) ^
            Td1(byte(t3, 2)) ^
            Td2(byte(t2, 1)) ^
            Td3(byte(t1, 0)) ^
            rk[0];
        s1 =
            Td0(byte(t1, 3)) ^
            Td1(byte(t0, 2)) ^
            Td2(byte(t3, 1)) ^
            Td3(byte(t2, 0)) ^
            rk[1];
        s2 =
            Td0(byte(t2, 3)) ^
            Td1(byte(t1, 2)) ^
            Td2(byte(t0, 1)) ^
            Td3(byte(t3, 0)) ^
            rk[2];
        s3 =
            Td0(byte(t3, 3)) ^
            Td1(byte(t2, 2)) ^
            Td2(byte(t1, 1)) ^
            Td3(byte(t0, 0)) ^
            rk[3];
    }

    /*
     * apply last round and
     * map cipher state to byte array block:
     */
    s0 =
        (Td4[byte(t0, 3)] & 0xff000000) ^
        (Td4[byte(t3, 2)] & 0x00ff0000) ^
        (Td4[byte(t2, 1)] & 0x0000ff00) ^
        (Td4[byte(t1, 0)] & 0x000000ff) ^
        rk[0];
    STORE32H(s0, pt);
    s1 =
        (Td4[byte(t1, 3)] & 0xff000000) ^
        (Td4[byte(t0, 2)] & 0x00ff0000) ^
        (Td4[byte(t3, 1)] & 0x0000ff00) ^
        (Td4[byte(t2, 0)] & 0x000000ff) ^
        rk[1];
    STORE32H(s1, pt+4);
    s2 =
        (Td4[byte(t2, 3)] & 0xff000000) ^
        (Td4[byte(t1, 2)] & 0x00ff0000) ^
        (Td4[byte(t0, 1)] & 0x0000ff00) ^
        (Td4[byte(t3, 0)] & 0x000000ff) ^
        rk[2];
    STORE32H(s2, pt+8);
    s3 =
        (Td4[byte(t3, 3)] & 0xff000000) ^
        (Td4[byte(t2, 2)] & 0x00ff0000) ^
        (Td4[byte(t1, 1)] & 0x0000ff00) ^
        (Td4[byte(t0, 0)] & 0x000000ff) ^
        rk[3];
    STORE32H(s3, pt+12);

    return 0;
}

int emi_aes_encrypt(unsigned char *pt,unsigned char *ct,unsigned char *key){
	unsigned char k[32]={0x12,0x45,0xaf,0x31,0x5e,0x96,0xe3,0xd6,\
						0x9d,0x7e,0x49,0x19,0x42,0x38,0x48,0x53,\
						0x1a,0x04,0x32,0x94,0xa5,0xd5,0x4e,0x20,\
						0xe8,0x21,0x50,0x06,0x0a,0x4d,0x58,0x8a\
						};

	ulong32 ek[60]={0};
		
	int i=strlen((char *)key);

	if(i>=sizeof(k))
		i=sizeof(k)-1;

	for(;i>0;i--)
		k[i]=*key++;
	
	if(schedule_key(k,16,ek,NULL)){
		return -1;
	}
	if(aes_encrypt(pt,ct,ek,10)){
		return -1;
	}
	return 0;
}

int emi_aes_decrypt(unsigned char *ct, unsigned char *pt,unsigned char *key){
	unsigned char k[32]={0x12,0x45,0xaf,0x31,0x5e,0x96,0xe3,0xd6,\
						0x9d,0x7e,0x49,0x19,0x42,0x38,0x48,0x53,\
						0x1a,0x04,0x32,0x94,0xa5,0xd5,0x4e,0x20,\
						0xe8,0x21,0x50,0x06,0x0a,0x4d,0x58,0x8a\
						};

	ulong32 dk[60]={0},ek[60];
		
	int i=strlen((char *)key);

	if(i>=sizeof(k))
		i=sizeof(k)-1;

	for(;i>0;i--)
		k[i]=*key++;
	
	if(schedule_key(k,16,ek,dk)){
		return -1;
	}
	if(aes_decrypt(ct,pt,dk,10)){
		return -1;
	}
	return 0;
}
