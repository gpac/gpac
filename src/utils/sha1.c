
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include <gpac/tools.h>

#ifndef PUT_UINT32_BE
#define PUT_UINT32_BE(n,b,i)                            \
{                                                       \
    (b)[(i)    ] = (u8) ( (n) >> 24 );       \
    (b)[(i) + 1] = (u8) ( (n) >> 16 );       \
    (b)[(i) + 2] = (u8) ( (n) >>  8 );       \
    (b)[(i) + 3] = (u8) ( (n)       );       \
}
#endif

#if 0
/*
 *  FIPS-180-1 compliant SHA-1 implementation
 *
 *  Copyright (C) 2003-2006  Christophe Devine
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License, version 2.1 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA
 */
/*
 *  The SHA-1 standard was published by NIST in 1993.
 *
 *  http://www.itl.nist.gov/fipspubs/fip180-1.htm
 */


struct __sha1_context
{
	u32 total[2];
	u32 state[5];
	u8 buffer[64];
};

/*
 * 32-bit integer manipulation macros (big endian)
 */
#ifndef GET_UINT32_BE
#define GET_UINT32_BE(n,b,i)                            \
{                                                       \
    (n) = ( (u32) (b)[(i)    ] << 24 )        \
        | ( (u32) (b)[(i) + 1] << 16 )        \
        | ( (u32) (b)[(i) + 2] <<  8 )        \
        | ( (u32) (b)[(i) + 3]       );       \
}
#endif

/*
 * SHA-1 context setup
 */
GF_SHA1Context *gf_sha1_starts()
{
	GF_SHA1Context *ctx;
	GF_SAFEALLOC(ctx, GF_SHA1Context);
	if (!ctx) return NULL;
	ctx->total[0] = 0;
	ctx->total[1] = 0;

	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xEFCDAB89;
	ctx->state[2] = 0x98BADCFE;
	ctx->state[3] = 0x10325476;
	ctx->state[4] = 0xC3D2E1F0;
	return ctx;
}

static void sha1_process(GF_SHA1Context *ctx, u8 data[64] )
{
	u32 temp, W[16], A, B, C, D, E;

	GET_UINT32_BE( W[0],  data,  0 );
	GET_UINT32_BE( W[1],  data,  4 );
	GET_UINT32_BE( W[2],  data,  8 );
	GET_UINT32_BE( W[3],  data, 12 );
	GET_UINT32_BE( W[4],  data, 16 );
	GET_UINT32_BE( W[5],  data, 20 );
	GET_UINT32_BE( W[6],  data, 24 );
	GET_UINT32_BE( W[7],  data, 28 );
	GET_UINT32_BE( W[8],  data, 32 );
	GET_UINT32_BE( W[9],  data, 36 );
	GET_UINT32_BE( W[10], data, 40 );
	GET_UINT32_BE( W[11], data, 44 );
	GET_UINT32_BE( W[12], data, 48 );
	GET_UINT32_BE( W[13], data, 52 );
	GET_UINT32_BE( W[14], data, 56 );
	GET_UINT32_BE( W[15], data, 60 );

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

#define R(t)                                            \
(                                                       \
    temp = W[(t -  3) & 0x0F] ^ W[(t - 8) & 0x0F] ^     \
           W[(t - 14) & 0x0F] ^ W[ t      & 0x0F],      \
    ( W[t & 0x0F] = S(temp,1) )                         \
)

#define P(a,b,c,d,e,x)                                  \
{                                                       \
    e += S(a,5) + F(b,c,d) + K + x; b = S(b,30);        \
}

	A = ctx->state[0];
	B = ctx->state[1];
	C = ctx->state[2];
	D = ctx->state[3];
	E = ctx->state[4];

#define F(x,y,z) (z ^ (x & (y ^ z)))
#define K 0x5A827999

	P( A, B, C, D, E, W[0]  );
	P( E, A, B, C, D, W[1]  );
	P( D, E, A, B, C, W[2]  );
	P( C, D, E, A, B, W[3]  );
	P( B, C, D, E, A, W[4]  );
	P( A, B, C, D, E, W[5]  );
	P( E, A, B, C, D, W[6]  );
	P( D, E, A, B, C, W[7]  );
	P( C, D, E, A, B, W[8]  );
	P( B, C, D, E, A, W[9]  );
	P( A, B, C, D, E, W[10] );
	P( E, A, B, C, D, W[11] );
	P( D, E, A, B, C, W[12] );
	P( C, D, E, A, B, W[13] );
	P( B, C, D, E, A, W[14] );
	P( A, B, C, D, E, W[15] );
	P( E, A, B, C, D, R(16) );
	P( D, E, A, B, C, R(17) );
	P( C, D, E, A, B, R(18) );
	P( B, C, D, E, A, R(19) );

#undef K
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define K 0x6ED9EBA1

	P( A, B, C, D, E, R(20) );
	P( E, A, B, C, D, R(21) );
	P( D, E, A, B, C, R(22) );
	P( C, D, E, A, B, R(23) );
	P( B, C, D, E, A, R(24) );
	P( A, B, C, D, E, R(25) );
	P( E, A, B, C, D, R(26) );
	P( D, E, A, B, C, R(27) );
	P( C, D, E, A, B, R(28) );
	P( B, C, D, E, A, R(29) );
	P( A, B, C, D, E, R(30) );
	P( E, A, B, C, D, R(31) );
	P( D, E, A, B, C, R(32) );
	P( C, D, E, A, B, R(33) );
	P( B, C, D, E, A, R(34) );
	P( A, B, C, D, E, R(35) );
	P( E, A, B, C, D, R(36) );
	P( D, E, A, B, C, R(37) );
	P( C, D, E, A, B, R(38) );
	P( B, C, D, E, A, R(39) );

#undef K
#undef F

#define F(x,y,z) ((x & y) | (z & (x | y)))
#define K 0x8F1BBCDC

	P( A, B, C, D, E, R(40) );
	P( E, A, B, C, D, R(41) );
	P( D, E, A, B, C, R(42) );
	P( C, D, E, A, B, R(43) );
	P( B, C, D, E, A, R(44) );
	P( A, B, C, D, E, R(45) );
	P( E, A, B, C, D, R(46) );
	P( D, E, A, B, C, R(47) );
	P( C, D, E, A, B, R(48) );
	P( B, C, D, E, A, R(49) );
	P( A, B, C, D, E, R(50) );
	P( E, A, B, C, D, R(51) );
	P( D, E, A, B, C, R(52) );
	P( C, D, E, A, B, R(53) );
	P( B, C, D, E, A, R(54) );
	P( A, B, C, D, E, R(55) );
	P( E, A, B, C, D, R(56) );
	P( D, E, A, B, C, R(57) );
	P( C, D, E, A, B, R(58) );
	P( B, C, D, E, A, R(59) );

#undef K
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define K 0xCA62C1D6

	P( A, B, C, D, E, R(60) );
	P( E, A, B, C, D, R(61) );
	P( D, E, A, B, C, R(62) );
	P( C, D, E, A, B, R(63) );
	P( B, C, D, E, A, R(64) );
	P( A, B, C, D, E, R(65) );
	P( E, A, B, C, D, R(66) );
	P( D, E, A, B, C, R(67) );
	P( C, D, E, A, B, R(68) );
	P( B, C, D, E, A, R(69) );
	P( A, B, C, D, E, R(70) );
	P( E, A, B, C, D, R(71) );
	P( D, E, A, B, C, R(72) );
	P( C, D, E, A, B, R(73) );
	P( B, C, D, E, A, R(74) );
	P( A, B, C, D, E, R(75) );
	P( E, A, B, C, D, R(76) );
	P( D, E, A, B, C, R(77) );
	P( C, D, E, A, B, R(78) );
	P( B, C, D, E, A, R(79) );

#undef K
#undef F

	ctx->state[0] += A;
	ctx->state[1] += B;
	ctx->state[2] += C;
	ctx->state[3] += D;
	ctx->state[4] += E;
}

/*
 * SHA-1 process buffer
 */
void gf_sha1_update(GF_SHA1Context *ctx, u8 *input, u32 ilen )
{
	s32 fill;
	u32 left;

	if( ilen <= 0 )
		return;

	left = ctx->total[0] & 0x3F;
	fill = 64 - left;

	ctx->total[0] += ilen;
	ctx->total[0] &= 0xFFFFFFFF;

	if( ctx->total[0] < (u32) ilen )
		ctx->total[1]++;

	if( left && (s32) ilen >= fill )
	{
		memcpy( (void *) (ctx->buffer + left),
		        (void *) input, fill );
		sha1_process( ctx, ctx->buffer );
		input += fill;
		ilen  -= fill;
		left = 0;
	}

	while( ilen >= 64 )
	{
		sha1_process( ctx, input );
		input += 64;
		ilen  -= 64;
	}

	if( ilen > 0 )
	{
		memcpy( (void *) (ctx->buffer + left),
		        (void *) input, ilen );
	}
}

static const u8 sha1_padding[64] =
{
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * SHA-1 final digest
 */
void gf_sha1_finish(GF_SHA1Context *ctx, u8 output[GF_SHA1_DIGEST_SIZE] )
{
	u32 last, padn;
	u32 high, low;
	u8 msglen[8];

	high = ( ctx->total[0] >> 29 )
	       | ( ctx->total[1] <<  3 );
	low  = ( ctx->total[0] <<  3 );

	PUT_UINT32_BE( high, msglen, 0 );
	PUT_UINT32_BE( low,  msglen, 4 );

	last = ctx->total[0] & 0x3F;
	padn = ( last < 56 ) ? ( 56 - last ) : ( 120 - last );

	gf_sha1_update( ctx, (u8 *) sha1_padding, padn );
	gf_sha1_update( ctx, msglen, 8 );

	PUT_UINT32_BE( ctx->state[0], output,  0 );
	PUT_UINT32_BE( ctx->state[1], output,  4 );
	PUT_UINT32_BE( ctx->state[2], output,  8 );
	PUT_UINT32_BE( ctx->state[3], output, 12 );
	PUT_UINT32_BE( ctx->state[4], output, 16 );

	gf_free(ctx);
}
#else

/*
 *  sha1.c
 *
 *  Copyright (C) 1998, 2009
 *  Paul E. Jones <paulej@packetizer.com>
 *  All Rights Reserved
 *
 *  Freeware Public License (FPL)
 *
 *  This software is licensed as "freeware."  Permission to distribute
 *  this software in source and binary forms, including incorporation
 *  into other products, is hereby granted without a fee.  THIS SOFTWARE
 *  IS PROVIDED 'AS IS' AND WITHOUT ANY EXPRESSED OR IMPLIED WARRANTIES,
 *  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 *  AND FITNESS FOR A PARTICULAR PURPOSE.  THE AUTHOR SHALL NOT BE HELD
 *  LIABLE FOR ANY DAMAGES RESULTING FROM THE USE OF THIS SOFTWARE, EITHER
 *  DIRECTLY OR INDIRECTLY, INCLUDING, BUT NOT LIMITED TO, LOSS OF DATA
 *  OR DATA BEING RENDERED INACCURATE.
 *
 *****************************************************************************
 *  $Id: sha1.c 12 2009-06-22 19:34:25Z paulej $
 *****************************************************************************
 *
 *  Description:
 *      This file implements the Secure Hashing Standard as defined
 *      in FIPS PUB 180-1 published April 17, 1995.
 *
 *      The Secure Hashing Standard, which uses the Secure Hashing
 *      Algorithm (SHA), produces a 160-bit message digest for a
 *      given data stream.  In theory, it is highly improbable that
 *      two messages will produce the same message digest.  Therefore,
 *      this algorithm can serve as a means of providing a "fingerprint"
 *      for a message.
 *
 *  Portability Issues:
 *      SHA-1 is defined in terms of 32-bit "words".  This code was
 *      written with the expectation that the processor has at least
 *      a 32-bit machine word size.  If the machine word size is larger,
 *      the code should still function properly.  One caveat to that
 *      is that the input functions taking characters and character
 *      arrays assume that only 8 bits of information are stored in each
 *      character.
 *
 *  Caveats:
 *      SHA-1 is designed to work with messages less than 2^64 bits
 *      long. Although SHA-1 allows a message digest to be generated for
 *      messages of any number of bits less than 2^64, this
 *      implementation only works with messages with a length that is a
 *      multiple of the size of an 8-bit character.
 *
 */

/*
 *  This structure will hold context information for the hashing
 *  operation
 */
struct __sha1_context
{
	unsigned Message_Digest[5]; /* Message Digest (output)          */

	unsigned Length_Low;        /* Message length in bits           */
	unsigned Length_High;       /* Message length in bits           */

	unsigned char Message_Block[64]; /* 512-bit message blocks      */
	int Message_Block_Index;    /* Index into message block array   */

	int Computed;               /* Is the digest computed?          */
	int Corrupted;              /* Is the message digest corruped?  */
};

/*
 *  Define the circular shift macro
 */
#define SHA1CircularShift(bits,word) \
                ((((word) << (bits)) & 0xFFFFFFFF) | \
                ((word) >> (32-(bits))))


/*
 *  SHA1ProcessMessageBlock
 *
 *  Description:
 *      This function will process the next 512 bits of the message
 *      stored in the Message_Block array.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      Many of the variable names in the SHAContext, especially the
 *      single character names, were used because those were the names
 *      used in the publication.
 *
 *
 */
void SHA1ProcessMessageBlock(GF_SHA1Context *context)
{
	const unsigned K[] =            /* Constants defined in SHA-1   */
	{
		0x5A827999,
		0x6ED9EBA1,
		0x8F1BBCDC,
		0xCA62C1D6
	};
	int         t;                  /* Loop counter                 */
	unsigned    temp;               /* Temporary word value         */
	unsigned    W[80];              /* Word sequence                */
	unsigned    A, B, C, D, E;      /* Word buffers                 */

	/*
	 *  Initialize the first 16 words in the array W
	 */
	for(t = 0; t < 16; t++)
	{
		W[t] = ((unsigned) context->Message_Block[t * 4]) << 24;
		W[t] |= ((unsigned) context->Message_Block[t * 4 + 1]) << 16;
		W[t] |= ((unsigned) context->Message_Block[t * 4 + 2]) << 8;
		W[t] |= ((unsigned) context->Message_Block[t * 4 + 3]);
	}

	for(t = 16; t < 80; t++)
	{
		W[t] = SHA1CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
	}

	A = context->Message_Digest[0];
	B = context->Message_Digest[1];
	C = context->Message_Digest[2];
	D = context->Message_Digest[3];
	E = context->Message_Digest[4];

	for(t = 0; t < 20; t++)
	{
		temp =  SHA1CircularShift(5,A) +
		        ((B & C) | ((~B) & D)) + E + W[t] + K[0];
		temp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(t = 20; t < 40; t++)
	{
		temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
		temp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(t = 40; t < 60; t++)
	{
		temp = SHA1CircularShift(5,A) +
		       ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
		temp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(t = 60; t < 80; t++)
	{
		temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
		temp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	context->Message_Digest[0] =
	    (context->Message_Digest[0] + A) & 0xFFFFFFFF;
	context->Message_Digest[1] =
	    (context->Message_Digest[1] + B) & 0xFFFFFFFF;
	context->Message_Digest[2] =
	    (context->Message_Digest[2] + C) & 0xFFFFFFFF;
	context->Message_Digest[3] =
	    (context->Message_Digest[3] + D) & 0xFFFFFFFF;
	context->Message_Digest[4] =
	    (context->Message_Digest[4] + E) & 0xFFFFFFFF;

	context->Message_Block_Index = 0;
}

/*
 *  SHA1PadMessage
 *
 *  Description:
 *      According to the standard, the message must be padded to an even
 *      512 bits.  The first padding bit must be a '1'.  The last 64
 *      bits represent the length of the original message.  All bits in
 *      between should be 0.  This function will pad the message
 *      according to those rules by filling the Message_Block array
 *      accordingly.  It will also call SHA1ProcessMessageBlock()
 *      appropriately.  When it returns, it can be assumed that the
 *      message digest has been computed.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to pad
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *
 */
static void SHA1PadMessage(GF_SHA1Context *context)
{
	/*
	 *  Check to see if the current message block is too small to hold
	 *  the initial padding bits and length.  If so, we will pad the
	 *  block, process it, and then continue padding into a second
	 *  block.
	 */
	if (context->Message_Block_Index > 55)
	{
		context->Message_Block[context->Message_Block_Index++] = 0x80;
		while(context->Message_Block_Index < 64)
		{
			context->Message_Block[context->Message_Block_Index++] = 0;
		}

		SHA1ProcessMessageBlock(context);

		while(context->Message_Block_Index < 56)
		{
			context->Message_Block[context->Message_Block_Index++] = 0;
		}
	}
	else
	{
		context->Message_Block[context->Message_Block_Index++] = 0x80;
		while(context->Message_Block_Index < 56)
		{
			context->Message_Block[context->Message_Block_Index++] = 0;
		}
	}

	/*
	 *  Store the message length as the last 8 octets
	 */
	context->Message_Block[56] = (context->Length_High >> 24) & 0xFF;
	context->Message_Block[57] = (context->Length_High >> 16) & 0xFF;
	context->Message_Block[58] = (context->Length_High >> 8) & 0xFF;
	context->Message_Block[59] = (context->Length_High) & 0xFF;
	context->Message_Block[60] = (context->Length_Low >> 24) & 0xFF;
	context->Message_Block[61] = (context->Length_Low >> 16) & 0xFF;
	context->Message_Block[62] = (context->Length_Low >> 8) & 0xFF;
	context->Message_Block[63] = (context->Length_Low) & 0xFF;

	SHA1ProcessMessageBlock(context);
}
/*
 *  SHA1Reset
 *
 *  Description:
 *      This function will initialize the SHA1Context in preparation
 *      for computing a new message digest.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to reset.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *
 */
GF_SHA1Context *gf_sha1_starts()
{
	GF_SHA1Context *context;
	GF_SAFEALLOC(context, GF_SHA1Context);
	if (!context) return NULL;
	context->Length_Low             = 0;
	context->Length_High            = 0;
	context->Message_Block_Index    = 0;

	context->Message_Digest[0]      = 0x67452301;
	context->Message_Digest[1]      = 0xEFCDAB89;
	context->Message_Digest[2]      = 0x98BADCFE;
	context->Message_Digest[3]      = 0x10325476;
	context->Message_Digest[4]      = 0xC3D2E1F0;

	context->Computed   = 0;
	context->Corrupted  = 0;
	return context;
}

void gf_sha1_update(GF_SHA1Context *context, u8 *message_array, u32 length )
{
	if (!length)
	{
		return;
	}

	if (context->Computed || context->Corrupted)
	{
		context->Corrupted = 1;
		return;
	}

	while(length-- && !context->Corrupted)
	{
		context->Message_Block[context->Message_Block_Index++] =
		    (*message_array & 0xFF);

		context->Length_Low += 8;
		/* Force it to 32 bits */
		context->Length_Low &= 0xFFFFFFFF;
		if (context->Length_Low == 0)
		{
			context->Length_High++;
			/* Force it to 32 bits */
			context->Length_High &= 0xFFFFFFFF;
			if (context->Length_High == 0)
			{
				/* Message is too long */
				context->Corrupted = 1;
			}
		}

		if (context->Message_Block_Index == 64)
		{
			SHA1ProcessMessageBlock(context);
		}

		message_array++;
	}
}
void gf_sha1_finish(GF_SHA1Context *context, u8 output[GF_SHA1_DIGEST_SIZE] )
{
	if (context->Corrupted)
	{
		return;
	}

	if (!context->Computed)
	{
		SHA1PadMessage(context);
		context->Computed = 1;
	}
	PUT_UINT32_BE( context->Message_Digest[0], output,  0 );
	PUT_UINT32_BE( context->Message_Digest[1], output,  4 );
	PUT_UINT32_BE( context->Message_Digest[2], output,  8 );
	PUT_UINT32_BE( context->Message_Digest[3], output, 12 );
	PUT_UINT32_BE( context->Message_Digest[4], output, 16 );

	gf_free(context);
}

#endif

/*
 * Output = SHA-1( file contents )
 */
GF_EXPORT
GF_Err gf_sha1_file_ptr(FILE *f, u8 output[GF_SHA1_DIGEST_SIZE] )
{
	u64 pos = gf_ftell(f);
	size_t n;
	GF_SHA1Context *ctx;
	u8 buf[1024];

	ctx  = gf_sha1_starts();
	gf_fseek(f, 0, SEEK_SET);

	while( ( n = gf_fread( buf, sizeof( buf ), f ) ) > 0 )
		gf_sha1_update(ctx, buf, (s32) n );

	gf_sha1_finish(ctx, output );

	gf_fseek(f, pos, SEEK_SET);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sha1_file( const char *path, u8 output[GF_SHA1_DIGEST_SIZE] )
{
	FILE *f;
	GF_Err e;

	if (!strncmp(path, "gmem://", 7)) {
		u32 size;
		u8 *mem_address;
		e = gf_blob_get(path, &mem_address, &size, NULL);
		if (e) return e;

		gf_sha1_csum(mem_address, size, output);
        gf_blob_release(path);
		return GF_OK;
	}

	if( ( f = gf_fopen( path, "rb" ) ) == NULL )
		return GF_URL_ERROR;

	e = gf_sha1_file_ptr(f, output);
	gf_fclose( f );
	return e;
}

/*
 * Output = SHA-1( input buffer )
 */
GF_EXPORT
void gf_sha1_csum( u8 *input, u32 ilen, u8 output[GF_SHA1_DIGEST_SIZE] )
{
	GF_SHA1Context *ctx;

	memset(output, 0, sizeof(u8)*GF_SHA1_DIGEST_SIZE);
	ctx = gf_sha1_starts();
	if (ctx) {
		gf_sha1_update(ctx, input, ilen );
		gf_sha1_finish(ctx, output );
	}
}

#if 0 //unused
#define GF_SHA1_DIGEST_SIZE_HEXA		41
void gf_sha1_csum_hexa(u8 *buf, u32 buflen, u8 digest[GF_SHA1_DIGEST_SIZE_HEXA]) {
	u8 tmp[GF_SHA1_DIGEST_SIZE];
	gf_sha1_csum (buf, buflen, tmp );
	digest[0] = 0;
	{
		int i;
		for ( i=0; i<GF_SHA1_DIGEST_SIZE; i++ )
		{
			char t[3];
			t[2] = 0;
			sprintf ( t, "%02X", tmp[i] );
			strcat ( (char*)digest, t );
		}
	}
}
#endif


