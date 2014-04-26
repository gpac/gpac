/* Rijndael Cipher

   Written by Mike Scott 21st April 1999
   Copyright (c) 1999 Mike Scott
   See rijndael documentation

   Permission for free direct or derivative use is granted subject
   to compliance with any conditions that the originators of the
   algorithm place on its exploitation.

   Inspiration from Brian Gladman's implementation is acknowledged.

   Written for clarity, rather than speed.
   Full implementation.
   Endian indifferent.
*/

/* modified in order to use the libmcrypt API by Nikos Mavroyanopoulos
 * All modifications are placed under the license of libmcrypt.
 */

/* $Id: rijndael-128.c,v 1.3 2009-07-31 07:37:43 jeanlf Exp $ */

#include <gpac/internal/crypt_dev.h>

#if !defined(GPAC_DISABLE_MCRYPT)

typedef struct rijndael_instance {
	int Nk,Nb,Nr;
	u8 fi[24],ri[24];
	u32 fkey[120];
	u32 rkey[120];
} RI;

/* rotates x one bit to the left */

#define ROTL(x) (((x)>>7)|((x)<<1))

/* Rotates 32-bit word left by 1, 2 or 3 u8  */

#define ROTL8(x) (((x)<<8)|((x)>>24))
#define ROTL16(x) (((x)<<16)|((x)>>16))
#define ROTL24(x) (((x)<<24)|((x)>>8))

/* Fixed Data */

static u8 InCo[4] = { 0xB, 0xD, 0x9, 0xE };	/* Inverse Coefficients */

static u8 fbsub[256];
static u8 rbsub[256];
static u8 ptab[256], ltab[256];
static u32 ftable[256];
static u32 rtable[256];
static u32 rco[30];
static int tables_ok = 0;
/* Parameter-dependent data */

/* in "rijndael.h" */

static u32 pack(u8 * b)
{	/* pack bytes into a 32-bit Word */
	return ((u32) b[3] << 24) | ((u32) b[2] << 16) | ((u32)
	        b[1] << 8)
	       | (u32) b[0];
}

static void unpack(u32 a, u8 * b)
{	/* unpack bytes from a word */
	b[0] = (u8) a;
	b[1] = (u8) (a >> 8);
	b[2] = (u8) (a >> 16);
	b[3] = (u8) (a >> 24);
}


static u8 xtime(u8 a)
{
	u8 b;
	if (a & 0x80)
		b = 0x1B;
	else
		b = 0;
	a <<= 1;
	a ^= b;
	return a;
}

static u8 bmul(u8 x, u8 y)
{	/* x.y= AntiLog(Log(x) + Log(y)) */
	if (x && y)
		return ptab[(ltab[x] + ltab[y]) % 255];
	else
		return 0;
}

static u32 SubByte(u32 a)
{
	u8 b[4];
	unpack(a, b);
	b[0] = fbsub[b[0]];
	b[1] = fbsub[b[1]];
	b[2] = fbsub[b[2]];
	b[3] = fbsub[b[3]];
	return pack(b);
}

static u8 product(u32 x, u32 y)
{	/* dot product of two 4-u8 arrays */
	u8 xb[4], yb[4];
	unpack(x, xb);
	unpack(y, yb);
	return bmul(xb[0], yb[0]) ^ bmul(xb[1], yb[1]) ^ bmul(xb[2],
	        yb[2]) ^
	       bmul(xb[3], yb[3]);
}

static u32 InvMixCol(u32 x)
{	/* matrix Multiplication */
	u32 y, m;
	u8 b[4];

	m = pack(InCo);
	b[3] = product(m, x);
	m = ROTL24(m);
	b[2] = product(m, x);
	m = ROTL24(m);
	b[1] = product(m, x);
	m = ROTL24(m);
	b[0] = product(m, x);
	y = pack(b);
	return y;
}

static u8 ByteSub(u8 x)
{
	u8 y = ptab[255 - ltab[x]];	/* multiplicative inverse */
	x = y;
	x = ROTL(x);
	y ^= x;
	x = ROTL(x);
	y ^= x;
	x = ROTL(x);
	y ^= x;
	x = ROTL(x);
	y ^= x;
	y ^= 0x63;
	return y;
}

static void _mcrypt_rijndael_gentables(void)
{	/* generate tables */
	int i;
	u8 y, b[4];

	/* use 3 as primitive root to generate power and log tables */

	ltab[0] = 0;
	ptab[0] = 1;
	ltab[1] = 0;
	ptab[1] = 3;
	ltab[3] = 1;
	for (i = 2; i < 256; i++) {
		ptab[i] = ptab[i - 1] ^ xtime(ptab[i - 1]);
		ltab[ptab[i]] = i;
	}

	/* affine transformation:- each bit is xored with itself shifted one bit */

	fbsub[0] = 0x63;
	rbsub[0x63] = 0;
	for (i = 1; i < 256; i++) {
		y = ByteSub((u8) i);
		fbsub[i] = y;
		rbsub[y] = i;
	}

	for (i = 0, y = 1; i < 30; i++) {
		rco[i] = y;
		y = xtime(y);
	}

	/* calculate forward and reverse tables */
	for (i = 0; i < 256; i++) {
		y = fbsub[i];
		b[3] = y ^ xtime(y);
		b[2] = y;
		b[1] = y;
		b[0] = xtime(y);
		ftable[i] = pack(b);

		y = rbsub[i];
		b[3] = bmul(InCo[0], y);
		b[2] = bmul(InCo[1], y);
		b[1] = bmul(InCo[2], y);
		b[0] = bmul(InCo[3], y);
		rtable[i] = pack(b);
	}
}

static int _mcrypt_set_key(RI * rinst, u8 * key, int nk)
{	/* blocksize=32*nb bits. Key=32*nk bits */
	/* currently nb,bk = 4, 6 or 8          */
	/* key comes as 4*rinst->Nk bytes              */
	/* Key Scheduler. Create expanded encryption key */
	int nb = 4;		/* 128 block size */
	int i, j, k, m, N;
	int C1, C2, C3;
	u32 CipherKey[8];

	nk /= 4;
	if (nk < 4)
		nk = 4;

	if (tables_ok == 0) {
		_mcrypt_rijndael_gentables();
		tables_ok = 1;
	}

	rinst->Nb = nb;
	rinst->Nk = nk;

	/* rinst->Nr is number of rounds */
	if (rinst->Nb >= rinst->Nk)
		rinst->Nr = 6 + rinst->Nb;
	else
		rinst->Nr = 6 + rinst->Nk;

	C1 = 1;
	if (rinst->Nb < 8) {
		C2 = 2;
		C3 = 3;
	} else {
		C2 = 3;
		C3 = 4;
	}

	/* pre-calculate forward and reverse increments */
	for (m = j = 0; j < nb; j++, m += 3) {
		rinst->fi[m] = (j + C1) % nb;
		rinst->fi[m + 1] = (j + C2) % nb;
		rinst->fi[m + 2] = (j + C3) % nb;
		rinst->ri[m] = (nb + j - C1) % nb;
		rinst->ri[m + 1] = (nb + j - C2) % nb;
		rinst->ri[m + 2] = (nb + j - C3) % nb;
	}

	N = rinst->Nb * (rinst->Nr + 1);

	for (i = j = 0; i < rinst->Nk; i++, j += 4) {
		CipherKey[i] = pack(&key[j]);
	}
	for (i = 0; i < rinst->Nk; i++)
		rinst->fkey[i] = CipherKey[i];
	for (j = rinst->Nk, k = 0; j < N; j += rinst->Nk, k++) {
		rinst->fkey[j] =
		    rinst->fkey[j -
		                rinst->Nk] ^ SubByte(ROTL24(rinst->
		                                     fkey[j -
		                                             1])) ^
		    rco[k];
		if (rinst->Nk <= 6) {
			for (i = 1; i < rinst->Nk && (i + j) < N; i++)
				rinst->fkey[i + j] =
				    rinst->fkey[i + j -
				                rinst->Nk] ^ rinst->
				    fkey[i + j - 1];
		} else {
			for (i = 1; i < 4 && (i + j) < N; i++)
				rinst->fkey[i + j] =
				    rinst->fkey[i + j -
				                rinst->Nk] ^ rinst->
				    fkey[i + j - 1];
			if ((j + 4) < N)
				rinst->fkey[j + 4] =
				    rinst->fkey[j + 4 -
				                rinst->
				                Nk] ^ SubByte(rinst->
				                              fkey[j + 3]);
			for (i = 5; i < rinst->Nk && (i + j) < N; i++)
				rinst->fkey[i + j] =
				    rinst->fkey[i + j -
				                rinst->Nk] ^ rinst->
				    fkey[i + j - 1];
		}

	}

	/* now for the expanded decrypt key in reverse order */

	for (j = 0; j < rinst->Nb; j++)
		rinst->rkey[j + N - rinst->Nb] = rinst->fkey[j];
	for (i = rinst->Nb; i < N - rinst->Nb; i += rinst->Nb) {
		k = N - rinst->Nb - i;
		for (j = 0; j < rinst->Nb; j++)
			rinst->rkey[k + j] = InvMixCol(rinst->fkey[i + j]);
	}
	for (j = N - rinst->Nb; j < N; j++)
		rinst->rkey[j - N + rinst->Nb] = rinst->fkey[j];
	return 0;
}


/* There is an obvious time/space trade-off possible here.     *
 * Instead of just one ftable[], I could have 4, the other     *
 * 3 pre-rotated to save the ROTL8, ROTL16 and ROTL24 overhead */

static  void _mcrypt_encrypt(RI * rinst, u8 * buff)
{
	int i, j, k, m;
	u32 a[8], b[8], *x, *y, *t;

	for (i = j = 0; i < rinst->Nb; i++, j += 4) {
		a[i] = pack(&buff[j]);
		a[i] ^= rinst->fkey[i];
	}
	k = rinst->Nb;
	x = a;
	y = b;

	/* State alternates between a and b */
	for (i = 1; i < rinst->Nr; i++) {	/* rinst->Nr is number of rounds. May be odd. */

		/* if rinst->Nb is fixed - unroll this next
		   loop and hard-code in the values of fi[]  */

		for (m = j = 0; j < rinst->Nb; j++, m += 3) {	/* deal with each 32-bit element of the State */
			/* This is the time-critical bit */
			y[j] = rinst->fkey[k++] ^ ftable[(u8) x[j]] ^
			       ROTL8(ftable[(u8) (x[rinst->fi[m]] >> 8)]) ^
			       ROTL16(ftable
			              [(u8) (x[rinst->fi[m + 1]] >> 16)]) ^
			       ROTL24(ftable[x[rinst->fi[m + 2]] >> 24]);
		}
		t = x;
		x = y;
		y = t;		/* swap pointers */
	}

	/* Last Round - unroll if possible */
	for (m = j = 0; j < rinst->Nb; j++, m += 3) {
		y[j] = rinst->fkey[k++] ^ (u32) fbsub[(u8) x[j]] ^
		       ROTL8((u32) fbsub[(u8) (x[rinst->fi[m]] >> 8)]) ^
		       ROTL16((u32)
		              fbsub[(u8) (x[rinst->fi[m + 1]] >> 16)]) ^
		       ROTL24((u32) fbsub[x[rinst->fi[m + 2]] >> 24]);
	}
	for (i = j = 0; i < rinst->Nb; i++, j += 4) {
		unpack(y[i], &buff[j]);
		x[i] = y[i] = 0;	/* clean up stack */
	}
	return;
}

static  void _mcrypt_decrypt(RI * rinst, u8 * buff)
{
	int i, j, k, m;
	u32 a[8], b[8], *x, *y, *t;

	for (i = j = 0; i < rinst->Nb; i++, j += 4) {
		a[i] = pack(&buff[j]);
		a[i] ^= rinst->rkey[i];
	}
	k = rinst->Nb;
	x = a;
	y = b;

	/* State alternates between a and b */
	for (i = 1; i < rinst->Nr; i++) {	/* rinst->Nr is number of rounds. May be odd. */

		/* if rinst->Nb is fixed - unroll this next
		   loop and hard-code in the values of ri[]  */

		for (m = j = 0; j < rinst->Nb; j++, m += 3) {	/* This is the time-critical bit */
			y[j] = rinst->rkey[k++] ^ rtable[(u8) x[j]] ^
			       ROTL8(rtable[(u8) (x[rinst->ri[m]] >> 8)]) ^
			       ROTL16(rtable
			              [(u8) (x[rinst->ri[m + 1]] >> 16)]) ^
			       ROTL24(rtable[x[rinst->ri[m + 2]] >> 24]);
		}
		t = x;
		x = y;
		y = t;		/* swap pointers */
	}

	/* Last Round - unroll if possible */
	for (m = j = 0; j < rinst->Nb; j++, m += 3) {
		y[j] = rinst->rkey[k++] ^ (u32) rbsub[(u8) x[j]] ^
		       ROTL8((u32) rbsub[(u8) (x[rinst->ri[m]] >> 8)]) ^
		       ROTL16((u32)
		              rbsub[(u8) (x[rinst->ri[m + 1]] >> 16)]) ^
		       ROTL24((u32) rbsub[x[rinst->ri[m + 2]] >> 24]);
	}
	for (i = j = 0; i < rinst->Nb; i++, j += 4) {
		unpack(y[i], &buff[j]);
		x[i] = y[i] = 0;	/* clean up stack */
	}
	return;
}

void gf_crypt_register_rijndael_128(GF_Crypt *td)
{
	td->a_encrypt = (void *)_mcrypt_encrypt;
	td->a_decrypt = (void *)_mcrypt_decrypt;
	td->a_set_key = (void *)_mcrypt_set_key;
	td->algo_name = "Rijndael-128";
	td->algo_version = 20010801;
	td->num_key_sizes = 3;
	td->key_sizes[0] = 16;
	td->key_sizes[1] = 24;
	td->key_sizes[2] = 32;
	td->key_size = 32;
	td->is_block_algo = 1;
	td->algo_block_size = 16;
	td->algo_size = sizeof(RI);
}

#endif /*!defined(GPAC_DISABLE_MCRYPT)*/
