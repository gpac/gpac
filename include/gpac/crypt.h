/*
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Jean Le Feuvre
*			Copyright (c) Telecom ParisTech 2000-2022
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
\file <gpac/crypt.h>
\brief Utility tools for encryption and decryption.
*/

/*!
\addtogroup crypt_grp Cryptography
\ingroup media_grp
\brief Utility tools for encryption and decryption.

This section documents the encryption and decryption routines used by GPAC, mostly AES 128 in CBC or CTR modes.

@{
*/

#include <gpac/tools.h>

/*! cryptographic context object*/
typedef struct _gf_crypt_context GF_Crypt;

/*! Key size in bytes for AES 128*/
#define GF_AES_128_KEYSIZE 16

/*! Chaining mode of AES*/
typedef enum {
	/*! CBC chaining mode*/
	GF_CBC = 0,
	/*! CTR chaining mode*/
	GF_CTR = 1,
	/*! ECB (no chaining), payload must be a multiple of 16-bytes blocks*/
	GF_ECB = 2,
} GF_CRYPTO_MODE;

/*! Algorithm mode to use*/
typedef enum {
	/*! AES 128 bit encryption*/
	GF_AES_128 = 0
} GF_CRYPTO_ALGO;


/*! opens crypto context
\param algorithm the algorithm to use
\param mode the chaining mode of the algorithm
\return a new crypto context
*/
GF_Crypt *gf_crypt_open(GF_CRYPTO_ALGO algorithm, GF_CRYPTO_MODE mode);
/*! destroys a crypto context
\param gfc the target crytpo context
*/
void gf_crypt_close(GF_Crypt *gfc);

/*! initializes all buffers for the specified context
After calling this function you can use the crypto context for encryption or decryption (not both).
\param gfc the target crytpo context
\param key the key to use. MUST be 16 bytes long
\param iv the seed/initialization vector to use. It needs to be random and unique (but not secret). The same IV must be used
for encryption/decryption. MUST be 16 bytes long
\return error if any
*/
GF_Err gf_crypt_init(GF_Crypt *gfc, void *key, const void *iv);

/*! changes key, does not touch IV. This is used for key rolling
\param gfc the target crytpo context
\param key the new key to use. MUST be 16 bytes long
\return error if any
*/
GF_Err gf_crypt_set_key(GF_Crypt *gfc, void *key);

/*! sets the IV for the algorithm. Used for ISMA and CENC CTR.
\param gfc the target crytpo context
\param iv the new eed/initialization vector to use
\param size the size of the IV. Must be 16 or 17 for AES. If 17 (AES CTR only), the first byte shall contain the counter position
\return error if any
*/
GF_Err gf_crypt_set_IV(GF_Crypt *gfc, const void *iv, u32 size);

/*! gets the IV of the algorithm.
The size will hold the size of the state and the state must have enough bytes to hold it (17 is enough for AES 128).
In CTR mode, the first byte will be set to the counter value (number of bytes consumed in last block), or 0 if all bytes were consumed
\param gfc the target crytpo context
\param iv filled with the current IV
\param size will be set to the IV size (16 for AES CBC? 17 for AES CTR)
\return error if any
*/
GF_Err gf_crypt_get_IV(GF_Crypt *gfc, void *iv, u32 *size);

/*! encrypts a payload. The encryption is done inplace (plaintext is replaced by the ciphertext).
 The buffer size should be k*algorithms_block_size if used in a mode which operated in blocks (CBC) or whatever when used in CTR which operate in streams.

\param gfc the target crytpo context
\param plaintext the clear buffer
\param size the size of the clear buffer
\return error if any
*/
GF_Err gf_crypt_encrypt(GF_Crypt *gfc, void *plaintext, u32 size);

/*! decrypts a payload. The decryption is done inplace (ciphertext is replaced by the plaintext).
 The buffer size should be k*algorithms_block_size if used in a mode which operated in blocks (CBC) or whatever when used in CTR which operate in streams.

\param gfc the target crytpo context
\param ciphertext the encrypted buffer
\param size the size of the encrypted buffer
\return error if any
*/
GF_Err gf_crypt_decrypt(GF_Crypt *gfc, void *ciphertext, u32 size);


/*! @} */


#ifdef __cplusplus
}
#endif

#endif	/*_GF_CRYPT_H_*/
