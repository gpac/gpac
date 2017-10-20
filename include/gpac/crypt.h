/*
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Jean Le Feuvre
*			Copyright (c) Telecom ParisTech 2000-2012
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
The GPAC crypto lib is a simplified version of libmcrypt - not all algos are included.
Doc here is man mcrypt
Original libmcrypt license
*/

/*
* Copyright (C) 1998,1999,2000 Nikos Mavroyanopoulos
*
* This library is free software; you can redistribute it and/or modify it
* under the terms of the GNU Library General Public License as published
* by the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Library General Public License for more details.
*
* You should have received a copy of the GNU Library General Public
* License along with this library; if not, write to the
* Free Software Foundation, Inc., 59 Temple Place - Suite 330,
* Boston, MA 02111-1307, USA.
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



	/*max number of possible key sizes for all supported modes*/
#define MAX_KEY_SIZES	4

	/*crypto lib handler*/
	typedef struct _tag_crypt_stream GF_Crypt;

#define GF_AES_128_KEYSIZE 16

	typedef enum {
		GF_CBC = 0,
		GF_CTR = 1
	} GF_CRYPTO_MODE;

	typedef enum {
		GF_AES_128 = 0
	} GF_CRYPTO_ALGO;


	/*opens crypto context - algo and mode SHALL NOT be NULL*/
	GF_Crypt *gf_crypt_open(GF_CRYPTO_ALGO algorithm, GF_CRYPTO_MODE mode);
	/*close crypto context*/
	void gf_crypt_close(GF_Crypt *gfc);

	/* sets the state of the algorithm. Can be used only with block algorithms and certain modes like CBC, CFB etc.
	It is useful if you want to restart or start a different encryption quickly.
	*/
	GF_Err gf_crypt_set_state(GF_Crypt *gfc, const void *iv, int size);
	/*gets the state of the algorithm. Can be used only certain modes and algorithms.
	The size will hold the size of the state and the state must have enough bytes to hold it.
	*/
	GF_Err gf_crypt_get_state(GF_Crypt *gfc, void *iv, int *size);

	/*
	This function initializes all buffers for the specified context
	@iv: usually size of the algorithms block size - get it by calling gf_crypt_get_iv_size().
	IV is ignored in ECB. IV MUST exist in CFB, CBC, STREAM, nOFB and OFB modes.
	It needs to be random and unique (but not secret). The same IV must be used
	for encryption/decryption.
	@iv_size: key size in BYTES.
	After calling this function you can use the descriptor for encryption or decryption (not both).
	*/
	GF_Err gf_crypt_init(GF_Crypt *gfc, void *key, const void *iv);
	/*changes key and IV*/
	GF_Err gf_crypt_set_key(GF_Crypt *gfc, void *key, u32 keysize, const void *iv);

	/*
	main encryption function.
	@Plaintext, @len: plaintext to encrypt - len should be  k*algorithms_block_size if used in a mode
	which operated in blocks (cbc, ecb, nofb), or whatever when used in cfb or ofb which operate in streams.
	The plaintext is replaced by the ciphertext.
	*/
	GF_Err gf_crypt_encrypt(GF_Crypt *gfc, void *plaintext, int len);
	/*decryption function. It is almost the same with gf_crypt_generic.*/
	GF_Err gf_crypt_decrypt(GF_Crypt *gfc, void *ciphertext, int len);


	/*! @} */


#ifdef __cplusplus
}
#endif

#endif	/*_GF_CRYPT_H_*/
