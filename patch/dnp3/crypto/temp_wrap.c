/*
 * $Id: wrap.c 4 2007-04-10 22:55:27Z sparky1194 $
 *
 * Copyright (C) 2007 Turner Technolgoies Inc. http://www.turner.ca
 *
 * Permission is hereby granted, free of charge, to any person 
 * obtaining a copy of this software and associated documentation 
 * files (the "Software"), to deal in the Software without 
 * restriction, including without limitation the rights to use, 
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following 
 * conditions:
 *      
 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software. 
 *      
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
//#include <sys/stat.h>
//#include <sys/types.h>
//#include <fcntl.h>
//#include <errno.h>
//#include <unistd.h>
//#include <stdlib.h>
//#include <string.h>
//#include <assert.h>

#include "aes.h"
#include "sha1.h"
#include "sha2.h"

#define TESTFILE "testdata.bin"
#define SHAKEYFILE "sha1-key.bin"
#define SHA2KEYFILE "sha2-key.bin"
#define SESSION_KEY_FILE "session_key"
#define UPDATE_KEY_FILE "update_key"
#define MAXLINE  1024

#define BITS_PER_BYTE  8
#define SHA1_HMAC_SIZE (128 / BITS_PER_BYTE)
#define SHA2_HMAC_SIZE (256 / BITS_PER_BYTE)

#define DNP_HMAC_SIZE_SERIAL 4
#define DNP_HMAC_SIZE_TCP    8

#include "wrap.h"

void print_bytes(unsigned char * buf, int size)
{
    int i;
    for (i=0; i<size; i++)
    {
	printf("%02X ", buf[i]);
	if (i%16 == 7)
	    printf("- ");
	if (i%16 == 15)
	    printf("\n");
    }
    printf("\n");
}

int read_key(const char* file_name, unsigned char* key, int key_len)
{
#if 0
    int fd, ret;
    unsigned char inbuf[MAXLINE]; /* file input */

    if (file_name == NULL || key == NULL) return -1;

    if((fd = open(file_name, O_RDONLY)) < 0)
    {
	printf("Could not open file %s For reading: %d\n", 
	       file_name, errno);
	perror(NULL);
	exit(1);
    }

    if((ret = read(fd, inbuf, MAXLINE)) < 0)
    {
	printf("Could not read from file %s: %d\n",
	       file_name, errno);
	perror(NULL);
	close(fd);
	exit(1);
    }
        
    close(fd);

    if (ret != key_len)
    {
	printf("From %s: Expected %d bytes, read %d bytes\n", 
	       file_name, key_len, ret);
	return -2;
    }

    memcpy(key, inbuf, key_len);
#endif
    return 0;
}

/**
 * Wrap AES data.
 * @param  key    Key encryption key.
 * @param  n      Number of 64-bit blocks (size of 'in')
 *                asserts((n * 5 + n) < 255)
 * @param  in     Data to be wrapped.  Not modified afterwards.
 *                length of n * 8 bytes.
 * @param  out    Cipher data.  Modified during function.
 *                length of (n + 1) * 8 bytes.
 * @return 0      on success.
 */
int aes_wrap(const unsigned char* key, int n, const unsigned char* in,
	     unsigned char* out)
{
#if 0
    unsigned char *a, *r;
    unsigned char b[16];
    aes_context   context;
    int           j, i;

    assert((n * 5 + n) < 255); /* needed because of: a[7] ^= n*j + i; */

    a = out;
    r = out + 8;

    /* Init variables */
    memset(a, 0xa6, 8);   /* A */
    memcpy(r, in, 8 * n); /* R */

    aes_set_key(&context, key, 128);
    
    /* Calculate Intermediate Values */
    for(j=0; j<6; j++)
    {
	r = out + 8; /* first register */
	for(i = 1; i <= n; i++)
	{
	    memcpy(b, a, 8); /* B = AES(K, A | R[i]) */
	    memcpy(b + 8, r, 8);
	    aes_encrypt(&context, b, b);
	    memcpy(a, b, 8); /* A = MSB64(B) ^ t where t = (n*j) + i */
	    a[7] ^= (n * j) + i;
	    memcpy(r, b + 8, 8); /* R[i] = LSB64(B) */
	    r += 8; /* next register */
	}
    }
#endif
    return 0;
}

#define DNP_IV "\xA6\xA6\xA6\xA6\xA6\xA6\xA6\xA6"

/**
 * Unwrap AES data.
 * @param  key    Key encryption key.
 * @param  n      Number of 64-bit blocks (size of 'in')
 *                asserts((n * 5 + n) < 255)
 * @param  in     Data to be unwrapped.  Not modified afterwards.
 *                length of (n + 1) * 8 bytes.
 * @param  out    Unwrapped data.  Modified during function.
 *                length of n * 8 bytes.
 * @return 0      on success.
 * @return -1     on IV mismatch.
 */
int aes_unwrap(const unsigned char* key, int n, const unsigned char* in, 
	       unsigned char* out)
{
#if 0
    unsigned char *r;
    unsigned char a[8], b[16];
    int           j, i;
    aes_context   context;

    assert(n * 5 + n < 255); /* needed because of: a[7] ^= n*j + i; */

    aes_set_key(&context, key, 128);

    /* Init variables */
    memcpy(a, in, 8);         /* A = C0 */
    r = out;
    memcpy(r, in + 8, n * 8); /* Ri = Ci */

    /* Compute intermediate values */
    for (j = 5; j >= 0; j--)
    {
	for (i = n; i > 0; i--)
	{
	    /* B = AES-1((A ^ t) | Ri), where t = (n * j) + i */
	    a[7] ^= (n * j) + i;
	    memcpy(b, a, 8);
	    memcpy(b + 8, r + 8 * (i-1), 8);
	    aes_decrypt(&context, b, b);

	    /* A = MSB64(B) */
	    memcpy(a, b, 8);

	    /* Ri = LSB64(B) */
	    memcpy(r + 8 * (i-1), b + 8, 8);
	}
    }

    /* verify IV: */
    if(memcmp(a, DNP_IV, sizeof(DNP_IV) - 1) != 0)
	return -1;
#endif
    return 0;
}

int aes_key_wrap_test()
{
#if 0
    int ret;
    unsigned char k[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
			   0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };
    unsigned char d[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
			   0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
    unsigned char o[sizeof(d) + 8];
    unsigned char p[sizeof(d)];
    
    if((ret = aes_wrap(k, sizeof(d) / 8, d, o)) < 0)
    {
	printf("aes_wrap error: %d\n", ret);
    }

    printf("key:\n");
    print_bytes(k, sizeof(k));
    printf("data in:\n");
    print_bytes(d, sizeof(d));
    printf("ciphertext:\n");
    print_bytes(o, sizeof(o));

    if((ret = aes_unwrap(k, (sizeof(o) / 8) - 1, o, p)) < 0)
    {
	printf("aes_unwrap error: %d\n", ret);
    }

    printf("key:\n");
    print_bytes(k, sizeof(k));
    printf("ciphertext:\n");
    print_bytes(o, sizeof(o));
    printf("plaintext:\n");
    print_bytes(p, sizeof(p));
#endif
    return 0;
}

// Random data ////////////////////////////////////////////////////////////////
#if 0
#ifndef HTONL
#define HTONL(A) \
{						\
    A = (((A & 0xff000000) >> 24)		\
	 | ((A & 0x00ff0000) >> 8)		\
	 | ((A & 0x0000ff00) << 8)		\
	 | ((A & 0x000000ff) << 24));		\
}
#endif
#endif
/*
 * Generate FIPS 186-2 w/change notice pseudo-random data.
 * @param  seed      The seed value for the random data.
 * @param  seed_len  Size of the random seed.
 * @param  x         Output, filled in as chunks of 40 bytes.
 * @param  xlen      Length of x, if < 40, no data is generated.
 *                   If x is only ever filled in using multiples of 40 bytes.
 *
 * Based off of work done by:
 * wpa_supplicant-0.5.7: sha1.c fips186_2_prf()
 *   Jouni Malinen <jkmaline@cc.hut.fi>   Dist. under GPL or BSD license
 */
void gen_rand(unsigned char *seed, size_t seed_len, unsigned char *x, 
	      size_t xlen)
{
#if 0
    unsigned char xkey[64] = {0};
    int i, j, m, k;
    unsigned char *xpos = x;
    unsigned int carry;

    sha1_context ctx;

    if (seed_len > sizeof(xkey))
	seed_len = sizeof(xkey);
    /* 1. set XKEY */
    memcpy(xkey, seed, seed_len);

    /* 2. let t = 67452301 EFCDAB89 98BADCFE 10325476 C3D2E1F0 */
    /* done in sha1_starts() */

    /* 3. For j = 0 to m - 1 do */
    m = xlen / 40;
    for(j = 0; j < m; j++)
    {
	/* 3.1 XSEEDj is not used. */
	/* 3.2 for i = 0 to 1 do */
	for(i=0;i<2;i++)
	{
	    /* XVAL = XKEY + XSEEDj */

	    /* wi = G(t, XVAL) */
	    sha1_starts(&ctx);
	    sha1_process(&ctx, xkey);

	    HTONL(ctx.state[0]);
	    HTONL(ctx.state[1]);
	    HTONL(ctx.state[2]);
	    HTONL(ctx.state[3]);
	    HTONL(ctx.state[4]);

	    memcpy(xpos, ctx.state, 20);

	    /* XKEY = (1 + XKEY + wi) */
	    carry = 1;
	    for (k=19; k>= 0; k--)
	    {
		carry += xkey[k] + xpos[k];
		xkey[k] = carry & 0xff;
		carry >>= 8;
	    }

	    xpos += 20;
	}
	
    }
   #endif
}

// END Random data ////////////////////////////////////////////////////////////


int main2()
{
#if 0
    int ret;
    int fd;
    unsigned char inbuf[MAXLINE]; /* Tests data buffer */
    int hash_data_len;

    unsigned char session_key[SHA1_HMAC_SIZE];
    unsigned char update_key[AES_128_SIZE];

    unsigned char data_to_hash[MAXLINE];
    unsigned char hmac_buf[20];
    unsigned char hmac2_buf[32];

    //aes_context aes_c;

    /********* SELF TESTS **********/
/*     printf("Running AES Self test:\n"); */
/*     ret = aes_self_test(); */
/*     printf("Self test returned %s\n\n", (ret==0)?"Success":"Failure!"); */

    printf("Running SHA-1 self test:\n");
    ret = sha1_self_test();
    printf("Self test returned %s\n\n", (ret==0)?"Success":"Failure!");

    printf("Running SHA-256 self test:\n");
    ret = sha2_self_test();
    printf("Self test returned %s\n\n", (ret==0)?"Success":"Failure!");

    /** Read in the Session Key **/
    ret = read_key(SESSION_KEY_FILE, session_key, SHA1_HMAC_SIZE);
    if (ret < 0)
    {
	printf("read_key() returned %d, exiting", ret);
	exit(1);
    }

    printf("Read Session Key successfully:\n");
    print_bytes(session_key, SHA1_HMAC_SIZE);


    /** Read in the Update Key ***/
    ret = read_key(UPDATE_KEY_FILE, update_key, AES_128_SIZE);
    if (ret < 0)
    {
	printf("read_key() returned %d, exiting", ret);
	exit(1);
    }

    printf("Read Update Key successfully:\n");
    print_bytes(update_key, AES_128_SIZE);

    /** Encrypt the Session Key **/
/*     aes_set_key(&aes_c, update_key, AES_128_SIZE * 8); */
/*     aes_encrypt(&aes_c, session_key, session_key_enc); */
/*     printf("Encrypted Session Key successfully:\n"); */
/*     print_bytes(session_key_enc, AES_128_SIZE); */

    /** Decrypt the session key **/
/*     aes_decrypt(&aes_c, session_key_enc, session_key_dec); */
/*     printf("Decrypted Session Key successfully:\n"); */
/*     print_bytes(session_key_dec, AES_128_SIZE); */



    /**** Read in the random data ****/
    if((fd = open(TESTFILE,O_RDONLY)) < 0)
    {
	printf("Could not open file " TESTFILE " For reading: %d", 
	       errno);
	perror(NULL);
	exit(1);
    }

    if((ret = read(fd, inbuf, MAXLINE)) < 0)
    {
	printf("Could not read from file " TESTFILE ": %d", 
	       errno);
	perror(NULL);
	close(fd);
	exit(1);
    }
    
    close(fd);

    printf("Read %d bytes:\n", ret);
    print_bytes(inbuf, ret);

    /****** Create the HMAC ******/
    memcpy(data_to_hash, session_key, AES_128_SIZE);
    hash_data_len = AES_128_SIZE;
    memcpy(data_to_hash + hash_data_len, inbuf, ret);
    hash_data_len += ret;

    printf("Data used for HMAC:\n");
    print_bytes(data_to_hash, hash_data_len);
    sha1_hmac(update_key, sizeof(update_key), data_to_hash, 
	      hash_data_len, hmac_buf);

    printf("HMAC-SHA1:\n");
    print_bytes(hmac_buf, 20);



    /*  Create the HMAC-256  */
    sha2_hmac(update_key, sizeof(update_key), data_to_hash, 
	      hash_data_len, hmac2_buf);
    printf("\nHMAC-SHA2:\n");
    print_bytes(hmac2_buf, 32);

    aes_key_wrap_test();
#endif
    return 0;
}
