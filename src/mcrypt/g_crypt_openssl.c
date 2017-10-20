/*
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Rodolphe Fouquet
*			Copyright (c) Motion Spell 2016
*					All rights reserved
*
*  This file is part of GPAC / crypto lib sub-project
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

#include "g_crypt_openssl.h"
#include <openssl/aes.h>
#include <openssl/modes.h>
#include <math.h>

typedef struct {
	AES_KEY enc_key;
	AES_KEY dec_key;
	u8* block;
	u8* padded_input; // use only when the input length is inferior to the algo block size
	u8* previous_ciphertext;
} Openssl_ctx_cbc;

/** CBC STUFF **/

GF_Err gf_crypt_init_openssl_cbc(GF_Crypt* td, void *key, const void *iv)
{
	GF_SAFEALLOC(td->context, Openssl_ctx_cbc);
	if (td->context == NULL) goto freeall;


	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;

	ctx->previous_ciphertext = gf_malloc(td->algo_block_size);
	ctx->block = gf_malloc(td->algo_block_size);
	if (ctx->previous_ciphertext == NULL) goto freeall;


	ctx->padded_input = gf_malloc(td->algo_block_size);
	if (ctx->padded_input == NULL) goto freeall;

	if (iv != NULL) {
		memcpy(ctx->previous_ciphertext, iv, td->algo_block_size);
	}
	else {
		memset(ctx->previous_ciphertext, 0, td->algo_block_size);
	}

	return GF_OK;

freeall:
	gf_free(ctx->previous_ciphertext);
	gf_free(ctx->block);
	gf_free(ctx->padded_input);
	return GF_OUT_OF_MEM;
}

void gf_crypt_deinit_openssl_cbc(GF_Crypt* td)
{
	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;
	gf_free(ctx->previous_ciphertext);
	gf_free(ctx->block);
	gf_free(ctx->padded_input);
}

void gf_set_key_openssl_cbc(GF_Crypt* td)
{
	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;
	AES_set_encrypt_key(td->keyword_given, td->key_size * 8, &(ctx->enc_key));
	AES_set_decrypt_key(td->keyword_given, td->key_size * 8, &(ctx->dec_key));
}

GF_Err gf_crypt_set_state_openssl_cbc(GF_Crypt* td, const void *iv, int iv_size)
{
	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;
	memcpy(ctx->previous_ciphertext, iv, iv_size);
	return GF_OK;
}

GF_Err gf_crypt_get_state_openssl_cbc(GF_Crypt* td, void *iv, int *iv_size)
{

	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;
	*iv_size = td->algo_block_size;

	memcpy(iv, ctx->previous_ciphertext, td->algo_block_size);

	return GF_OK;
}

GF_Err gf_crypt_crypt_openssl_cbc(GF_Crypt* td, u8 *plaintext, int len, int aes_crypt_type) {
	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;
	int iteration;
	int numberOfIterations = ((float)len / (float)td->algo_block_size);
	AES_KEY *key;
	if (aes_crypt_type == AES_ENCRYPT) {
		key = &ctx->enc_key;
	}
	else {
		key = &ctx->dec_key;
	}
	for (iteration = 0; iteration < numberOfIterations; ++iteration) {
		if (len - iteration*td->algo_block_size < td->algo_block_size) {
			memset(ctx->padded_input, 0, td->algo_block_size);
			memcpy(ctx->padded_input, plaintext, len - iteration*td->algo_block_size);
			AES_cbc_encrypt(plaintext + iteration*td->algo_block_size, ctx->block, td->algo_block_size, key, ctx->previous_ciphertext, aes_crypt_type);
			memcpy(plaintext, ctx->block, len - iteration*td->algo_block_size);
		}
		else {
			AES_cbc_encrypt(plaintext + iteration*td->algo_block_size, ctx->block, td->algo_block_size, key, ctx->previous_ciphertext, aes_crypt_type);
			memcpy((u8*)plaintext + iteration*td->algo_block_size, ctx->block, td->algo_block_size);
		}

	}

	return GF_OK;
}

GF_Err gf_crypt_encrypt_openssl_cbc(GF_Crypt* td, u8 *plaintext, int len)
{
	return gf_crypt_crypt_openssl_cbc(td, plaintext, len, AES_ENCRYPT);
}

GF_Err gf_crypt_decrypt_openssl_cbc(GF_Crypt* td, u8 *ciphertext, int len)
{
	return gf_crypt_crypt_openssl_cbc(td, ciphertext, len, AES_DECRYPT);
}

typedef struct {
	AES_KEY enc_key;
	AES_KEY dec_key;
	u8* block;
	u8* padded_input; // use only when the input length is inferior to the algo block size
	u8* enc_counter;
	u8* c_counter;
	unsigned int c_counter_pos;
} Openssl_ctx_ctr;


/** CTR STUFF **/

void gf_set_key_openssl_ctr(GF_Crypt* td)
{
	Openssl_ctx_ctr* ctx = (Openssl_ctx_ctr*)td->context;

	AES_set_encrypt_key(td->keyword_given, td->key_size * 8, &(ctx->enc_key));
	AES_set_decrypt_key(td->keyword_given, td->key_size * 8, &(ctx->dec_key));
}

GF_Err gf_crypt_set_state_openssl_ctr(GF_Crypt* td, const void *iv, int iv_size)
{
	Openssl_ctx_ctr* ctx = (Openssl_ctx_ctr*)td->context;

	ctx->c_counter_pos = ((u8*)iv)[0];
	memcpy(ctx->c_counter, &((u8*)iv)[1], iv_size - 1);
	memcpy(ctx->enc_counter, &((u8*)iv)[1], iv_size - 1);
	return GF_OK;
}

GF_Err gf_crypt_get_state_openssl_ctr(GF_Crypt* td, void *iv, int *iv_size)
{
	Openssl_ctx_ctr* ctx = (Openssl_ctx_ctr*)td->context;

	*iv_size = td->algo_block_size + 1;

	((u8 *)iv)[0] = ctx->c_counter_pos;
	memcpy(&((u8 *)iv)[1], ctx->c_counter, td->algo_block_size);
	return GF_OK;
}

GF_Err gf_crypt_init_openssl_ctr(GF_Crypt* td, void *key, const void *iv)
{
	GF_SAFEALLOC(td->context, Openssl_ctx_ctr);
	Openssl_ctx_ctr* ctx = (Openssl_ctx_ctr*)td->context;

	/* For ctr */
	ctx->c_counter_pos = 0;

	ctx->c_counter = gf_malloc(td->algo_block_size);
	if (ctx->c_counter == NULL) goto freeall;

	ctx->enc_counter = gf_malloc(td->algo_block_size);
	if (ctx->enc_counter == NULL) goto freeall;

	ctx->block = gf_malloc(td->algo_block_size);
	if (ctx->block == NULL) goto freeall;

	ctx->padded_input = gf_malloc(td->algo_block_size);
	if (ctx->padded_input == NULL) goto freeall;

	if (iv != NULL) {

		memcpy(ctx->c_counter, &((u8*)iv)[0], td->algo_block_size);
		memcpy(ctx->enc_counter, &((u8*)iv)[0], td->algo_block_size);
	}
	/* End ctr */

	return GF_OK;

freeall:
	gf_free(ctx->c_counter);
	gf_free(ctx->enc_counter);
	gf_free(ctx->padded_input);
	gf_free(ctx->block);
	return GF_OUT_OF_MEM;
}

void gf_crypt_deinit_openssl_ctr(GF_Crypt* td)
{
	Openssl_ctx_ctr* ctx = (Openssl_ctx_ctr*)td->context;
	gf_free(ctx->c_counter);
	gf_free(ctx->enc_counter);
	gf_free(ctx->padded_input);
	gf_free(ctx->block);
}

enum CRYPT_MODE {
	CRYPT = AES_ENCRYPT,
	DECRYPT = AES_DECRYPT
};

void AES_ctr128_encrypt(const unsigned char *in, unsigned char *out,
	size_t length, const AES_KEY *key,
	unsigned char ivec[AES_BLOCK_SIZE],
	unsigned char ecount_buf[AES_BLOCK_SIZE],
	unsigned int *num) {
	CRYPTO_ctr128_encrypt(in, out, length, key, ivec, ecount_buf, num, (block128_f)AES_encrypt);
}

GF_Err gf_crypt_crypt_openssl_ctr(GF_Crypt* td, u8 *plaintext, int len, int aes_crypt_type) {
	Openssl_ctx_ctr* ctx = (Openssl_ctx_ctr*)td->context;
	int iteration;
	int numberOfIterations = ((float)len / (float)td->algo_block_size);
	AES_KEY *key;
	if (aes_crypt_type == AES_ENCRYPT) {
		key = &ctx->enc_key;
	}
	else {
		key = &ctx->dec_key;
	}
	for (iteration = 0; iteration < numberOfIterations; ++iteration) {
		if (len - iteration*td->algo_block_size < td->algo_block_size) {
			memset(ctx->padded_input, 0, td->algo_block_size);
			memcpy(ctx->padded_input, plaintext, len - iteration*td->algo_block_size);
			AES_ctr128_encrypt(((const u8*)ctx->padded_input), ctx->block, td->algo_block_size, key, ctx->c_counter, ctx->enc_counter, &ctx->c_counter_pos);
			memcpy(plaintext, ctx->block, len - iteration*td->algo_block_size);
		}
		else {
			AES_ctr128_encrypt(((const u8*)plaintext) + iteration*td->algo_block_size, ctx->block, td->algo_block_size, key, ctx->c_counter, ctx->enc_counter, &ctx->c_counter_pos);
			memcpy((u8*)plaintext + iteration*td->algo_block_size, ctx->block, td->algo_block_size);
		}

	}
	return GF_OK;
}

GF_Err gf_crypt_encrypt_openssl_ctr(GF_Crypt* td, u8 *plaintext, int len)
{
	return gf_crypt_crypt_openssl_ctr(td, plaintext, len, CRYPT);
}

GF_Err gf_crypt_decrypt_openssl_ctr(GF_Crypt* td, u8 *ciphertext, int len)
{
	return gf_crypt_crypt_openssl_ctr(td, ciphertext, len, DECRYPT);
}



GF_Err open_openssl(GF_Crypt* td, GF_CRYPTO_MODE mode)
{
	td->mode = mode;
	switch (td->mode) {
	case GF_CBC:
		td->_init_crypt = gf_crypt_init_openssl_cbc;
		td->_deinit_crypt = gf_crypt_deinit_openssl_cbc;
		td->_set_key = gf_set_key_openssl_cbc;
		td->_crypt = gf_crypt_encrypt_openssl_cbc;
		td->_decrypt = gf_crypt_decrypt_openssl_cbc;
		td->_get_state = gf_crypt_get_state_openssl_cbc;
		td->_set_state = gf_crypt_set_state_openssl_cbc;
		break;
	case GF_CTR:
		td->_init_crypt = gf_crypt_init_openssl_ctr;
		td->_deinit_crypt = gf_crypt_deinit_openssl_ctr;
		td->_set_key = gf_set_key_openssl_ctr;
		td->_crypt = gf_crypt_encrypt_openssl_ctr;
		td->_decrypt = gf_crypt_decrypt_openssl_ctr;
		td->_get_state = gf_crypt_get_state_openssl_ctr;
		td->_set_state = gf_crypt_set_state_openssl_ctr;
		break;
	default:
		return GF_BAD_PARAM;
		break;

	}

	td->algo = GF_AES_128;
	td->key_size = 16;
	td->is_block_algo = 1;
	td->algo_block_size = AES_BLOCK_SIZE;

	td->has_IV = 1;
	td->is_block_mode = 1;
	td->is_block_algo_mode = 1;

	return GF_OK;
}