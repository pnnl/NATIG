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


#define MAXLINE  1024

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
    return 0;
}


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

    return 0;
}

int aes_key_wrap_test()
{

    return 0;
}

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

}

// END Random data ////////////////////////////////////////////////////////////


int main2()
{
    return 0;
}
