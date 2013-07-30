
 /*
   -----------------------------------------------------------------------
   Copyright (c) 2001 Dr Brian Gladman <brg@gladman.uk.net>, Worcester, UK

   TERMS

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   This software is provided 'as is' with no guarantees of correctness or
   fitness for purpose.
   -----------------------------------------------------------------------
 */

/* Example of the use of the AES (Rijndael) algorithm for file  */
/* encryption.  Note that this is an example application, it is */
/* not intended for real operational use.  The Command line is: */
/*                                                              */
/* aesxam input_file_name output_file_name [D|E] hexadecimalkey */
/*                                                              */
/* where E gives encryption and D decryption of the input file  */
/* into the output file using the given hexadecimal key string  */
/* The later is a hexadecimal sequence of 32, 48 or 64 digits   */
/* Examples to encrypt or decrypt aes.c into aes.enc are:       */
/*                                                              */
/* aesxam file.c file.enc E 0123456789abcdeffedcba9876543210    */
/*                                                              */
/* aesxam file.enc file2.c D 0123456789abcdeffedcba9876543210   */
/*                                                              */
/* which should return a file 'file2.c' identical to 'file.c'   */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "aes.h"

#include "platformcode.h"

/* A Pseudo Random Number Generator (PRNG) used for the     */
/* Initialisation Vector. The PRNG is George Marsaglia's    */
/* Multiply-With-Carry (MWC) PRNG that concatenates two     */
/* 16-bit MWC generators:                                   */
/*     x(n)=36969 * x(n-1) + carry mod 2^16                 */
/*     y(n)=18000 * y(n-1) + carry mod 2^16                 */
/* to produce a combined PRNG with a period of about 2^60.  */
/* The Pentium cycle counter is used to initialise it. This */
/* is crude but the IV does not need to be secret.          */

/* void cycles(unsigned long *rtn)     */
/* {                           // read the Pentium Time Stamp Counter */
/*     __asm */
/*     { */
/*     _emit   0x0f            // complete pending operations */
/*     _emit   0xa2 */
/*     _emit   0x0f            // read time stamp counter */
/*     _emit   0x31 */
/*     mov     ebx,rtn */
/*     mov     [ebx],eax */
/*     mov     [ebx+4],edx */
/*     _emit   0x0f            // complete pending operations */
/*     _emit   0xa2 */
/*     } */
/* } */

#define RAND(a,b) (((a = 36969 * (a & 65535) + (a >> 16)) << 16) + (b = 18000 * (b & 65535) + (b >> 16))  )

void fillrand(char *buf, int len)
{   static unsigned long a[2], mt = 1, count = 4;
    static char          r[4];
    int                  i;

    if(mt) {
	 mt = 0;
	 /*cycles(a);*/
      a[0]=0xeaf3;
	 a[1]=0x35fe;
    }

    for(i = 0; i < len; ++i)
    {
        if(count == 4)
        {
            *(unsigned long*)r = RAND(a[0], a[1]);
            count = 0;
        }

        buf[i] = r[count++];
    }
}

int encfile(aes *ctx)
{   char            inbuf[16], outbuf[16];
    fpos_t          flen;
    unsigned long   i=0, l=0, j, k;

    fillrand(outbuf, 16);           /* set an IV for CBC mode           */
    flen = 4096;
    fillrand(inbuf, 1);             /* make top 4 bits of a byte random */
    l = 15;                         /* and store the length of the last */
                                    /* block in the lower 4 bits        */
    inbuf[0] = ((char)flen & 15) | (inbuf[0] & ~15);

    for(j = 0; j <256; j++)
    {                               /* input 1st 16 bytes to buf[1..16] */
        for(k = 0; k < 16; ++k)
            inbuf[k] = jrand();

        for(i = 0; i < 16; ++i)         /* xor in previous cipher text  */
            inbuf[i] ^= outbuf[i];

        encrypt(inbuf, outbuf, ctx);    /* and do the encryption        */
                                    /* in all but first round read 16   */
        l = 16;                     /* bytes into the buffer            */
    }

    return 0;
}

int decfile(aes *ctx)
{   char    inbuf1[16], inbuf2[16], outbuf[16], *bp1, *bp2, *tp;
    int     i,j, l, flen, k;

    fillrand(inbuf1, 16);           /* set an IV for CBC mode           */

    for(k = 0; k < 16; ++k)
        inbuf2[k] = jrand();

    decrypt(inbuf2, outbuf, ctx);   /* decrypt it                       */

    for(i = 0; i < 16; ++i)         /* xor with previous input          */
        outbuf[i] ^= inbuf1[i];

    flen = 0;  /* recover length of the last block and set */
    l = 15;                 /* the count of valid bytes in block to 15  */
    bp1 = inbuf1;           /* set up pointers to two input buffers     */
    bp2 = inbuf2;

    for(j = 0; j < 256; ++j)
    {
        for(k = 0; k < 16; ++k)
            bp1[k] = jrand();

        decrypt(bp1, outbuf, ctx);  /* decrypt the new input block and  */

        for(i = 0; i < 16; ++i)     /* xor it with previous input block */
            outbuf[i] ^= bp2[i];

        /* set byte count to 16 and swap buffer pointers                */

        l = i; tp = bp1, bp1 = bp2, bp2 = tp;
    }

    return 0;
}

char *presetkey="ABCDEF1234567890ABCDEF1234567890";

int main(int argc, char *argv[])
{
    char    *cp, ch, key[32];
    int     i=0, by=0, key_len=0, err = 0, n;

/*    for(i = 0; i < 16; ++i)
//    {
        presetkey[i+6] = '0' + i;
        presetkey[i+22]= '0' + i;
        if(i < 6)
        {
            presetkey[i] = 'A' + i;
            presetkey[i+16] = 'A' + i;
        }
    }
*/
    initialise_trigger();
    start_trigger();
    for(n = 0; n < REPEAT_FACTOR>>9; n++)
    {
        aes     ctx[1];
        by=0; key_len=0; err = 0;
        cp = presetkey;   /* this is a pointer to the hexadecimal key digits  */
        i = 0;          /* this is a count for the input digits processed   */

        while(i < 64 && *cp)    /* the maximum key length is 32 bytes and   */
        {                       /* hence at most 64 hexadecimal digits      */
            ch = *cp++;            /* process a hexadecimal digit  */
            if(ch >= '0' && ch <= '9')
                by = (by << 4) + ch - '0';
            else if(ch >= 'A' && ch <= 'F')
                by = (by << 4) + ch - 'A' + 10;
            else                            /* error if not hexadecimal     */
            {
                err = -2; goto exit;
            }

            /* store a key byte for each pair of hexadecimal digits         */
            if(i++ & 1)
                key[i / 2 - 1] = by & 0xff;
        }

        if(*cp)
        {
            err = -3; goto exit;
        }
        else if(i < 32 || (i & 15))
        {
            err = -4; goto exit;
        }

        key_len = i / 2;

        set_key(key, key_len, enc, ctx);

        err = encfile(ctx);

        set_key(key, key_len, dec, ctx);

        err = decfile(ctx);

    }
exit:
    stop_trigger();
    return err;
}