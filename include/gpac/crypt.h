/*
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Jean Le Feuvre
*			Copyright (c) Telecom ParisTech 2000-2018
*					All rights reserved
*
*  This file is part of GPAC / Crypto Tools sub-project
*
*  GPAC is free software; you can redistribute it and/or modify
*  it under the terms of the GNU Lesser General Public License as published by
*  the Free Software Foundation; either version 2, or (at your option)
*  any later version.
*
*  GPAC is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU Lesser General Public License for more details.
*
*  You should have received a copy of the GNU Lesser General Public
*  License along with this library; see the file COPYING.  If not, write to
*  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*
*/

/*
The GPAC crypto lib uses either openSSL 1.x or tiny-AES (https://github.com/kokke/tiny-AES-c) when not available
The crypto lib only supports AES 128 bits in CBC and CTR modes
*/


#ifndef _GF_CRYPT_H_
#define _GF_CRYPT_H_


#ifdef __cplusplus
extern "C" {
#endif

/*!
*	\file <gpac/crypt.h>
*	\brief Utility tools for encryption and decryption.
*/

/*!
*	\addtogroup crypt_grp Cryptography
*	\ingroup media_grp
*	\brief Utility tools for encryption and decryption.
*
*This section documents the encryption and decryption routines used by GPAC, mostly AES 128 in CBC or CTR modes.
*	@{
*/

#include <gpac/tools.h>

/*crypto lib handler*/
typedef struct _gf_crypt_context GF_Crypt;

#define GF_AES_128_KEYSIZE 16

typedef enum {
	GF_CBC = 0,
	GF_CTR = 1
} GF_CRYPTO_MODE;

typedef enum {
	GF_AES_128 = 0
} GF_CRYPTO_ALGO;


/*opens crypto context*/
GF_Crypt *gf_crypt_open(GF_CRYPTO_ALGO algorithm, GF_CRYPTO_MODE mode);
/*close crypto context*/
void gf_crypt_close(GF_Crypt *gfc);

/*
This function initializes all buffers for the specified context
@key: Key used. MUST be 16 bytes long
@iv: Init Vector. It needs to be random and unique (but not secret). The same IV must be used
for encryption/decryption. MUST be 16 bytes long
After calling this function you can use the descriptor for encryption or decryption (not both).
*/
GF_Err gf_crypt_init(GF_Crypt *gfc, void *key, const void *iv);

/*changes key, does not touch IV - used for key roll*/
GF_Err gf_crypt_set_key(GF_Crypt *gfc, void *key);

/* sets the IV for the algorithm. Used for ISMA and CENC CTR.
*/
GF_Err gf_crypt_set_IV(GF_Crypt *gfc, const void *iv, u32 size);

/*gets the IV of the algorithm. The size will hold the size of the state and the state must have enough bytes to hold it (17 is enough). In CTR mode, the first byte will be set to the counter value (number of bytes consummed in last block), or 0 if all bytes were consummed
*/
GF_Err gf_crypt_get_IV(GF_Crypt *gfc, void *iv, u32 *size);

/*
main encryption function.
@Plaintext, @len: plaintext to encrypt - len should be  k*algorithms_block_size if used in a mode
which operated in blocks (cbc, ecb, nofb), or whatever when used in cfb or ofb which operate in streams.
The plaintext is replaced by the ciphertext.
*/
GF_Err gf_crypt_encrypt(GF_Crypt *gfc, void *plaintext, u32 len);
/*decryption function. It is almost the same with gf_crypt_generic.*/
GF_Err gf_crypt_decrypt(GF_Crypt *gfc, void *ciphertext, u32 len);


/*! @} */


#ifdef __cplusplus
}
#endif

#endif	/*_GF_CRYPT_H_*/
