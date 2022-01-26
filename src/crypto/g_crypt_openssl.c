/*
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Rodolphe Fouquet, Jean Le Feuvre
*			Copyright (c) Motion Spell 2016
*                     (c) Telecom Paris 2022
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

#include <gpac/internal/crypt_dev.h>

#ifdef GPAC_HAS_SSL
#include <openssl/aes.h>
#include <openssl/modes.h>

#include <math.h>

typedef struct {
	AES_KEY enc_key, dec_key;

	u8 block[AES_BLOCK_SIZE];
	u8 padded_input[AES_BLOCK_SIZE]; // use only when the input length is inferior to the algo block size
	u8 previous_ciphertext[AES_BLOCK_SIZE];
} Openssl_ctx_cbc;

/** CBC STUFF **/

GF_Err gf_crypt_init_openssl_cbc(GF_Crypt* td, void *key, const void *iv)
{
	Openssl_ctx_cbc *ctx = (Openssl_ctx_cbc*)td->context;
	if (!ctx) {
		GF_SAFEALLOC(ctx, Openssl_ctx_cbc);
		if (ctx == NULL) return GF_OUT_OF_MEM;
		td->context = ctx;
	}
	
	if (iv != NULL) {
		memcpy(ctx->previous_ciphertext, iv, AES_BLOCK_SIZE);
	}
	return GF_OK;
}

void gf_crypt_deinit_openssl_cbc(GF_Crypt* td)
{
}

void gf_set_key_openssl_cbc(GF_Crypt* td, void *key)
{
	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;
	AES_set_encrypt_key(key, 128, &(ctx->enc_key));
	AES_set_decrypt_key(key, 128, &(ctx->dec_key));
}

GF_Err gf_crypt_set_IV_openssl_cbc(GF_Crypt* td, const u8 *iv, u32 iv_size)
{
	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;
	memcpy(ctx->previous_ciphertext, iv, iv_size);
	return GF_OK;
}

GF_Err gf_crypt_get_IV_openssl_cbc(GF_Crypt* td, u8 *iv, u32 *iv_size)
{
	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;
	*iv_size = AES_BLOCK_SIZE;
	memcpy(iv, ctx->previous_ciphertext, AES_BLOCK_SIZE);
	return GF_OK;
}

GF_Err gf_crypt_crypt_openssl_cbc(GF_Crypt* td, u8 *plaintext, u32 len, u32 aes_crypt_type) {
	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;
	u32 iteration;
	AES_KEY *key = aes_crypt_type ? &ctx->enc_key : &ctx->dec_key;
	u32 numberOfIterations = len / AES_BLOCK_SIZE;
	if (numberOfIterations * AES_BLOCK_SIZE < len) numberOfIterations++;

	for (iteration = 0; iteration < numberOfIterations; ++iteration) {
		if (len - iteration*AES_BLOCK_SIZE < AES_BLOCK_SIZE) {
			memset(ctx->padded_input, 0, AES_BLOCK_SIZE);
			memcpy(ctx->padded_input, plaintext, len - iteration*AES_BLOCK_SIZE);
			AES_cbc_encrypt(plaintext + iteration*AES_BLOCK_SIZE, ctx->block, AES_BLOCK_SIZE, key, ctx->previous_ciphertext, aes_crypt_type);
			memcpy(plaintext + iteration*AES_BLOCK_SIZE, ctx->block, len - iteration*AES_BLOCK_SIZE);
		} else {
			AES_cbc_encrypt(plaintext + iteration*AES_BLOCK_SIZE, plaintext + iteration*AES_BLOCK_SIZE, AES_BLOCK_SIZE, key, ctx->previous_ciphertext, aes_crypt_type);
		}
	}
	return GF_OK;
}

GF_Err gf_crypt_encrypt_openssl_cbc(GF_Crypt* td, u8 *plaintext, u32 len)
{
	return gf_crypt_crypt_openssl_cbc(td, plaintext, len, AES_ENCRYPT);
}

GF_Err gf_crypt_decrypt_openssl_cbc(GF_Crypt* td, u8 *ciphertext, u32 len)
{
	return gf_crypt_crypt_openssl_cbc(td, ciphertext, len, AES_DECRYPT);
}

typedef struct {
	AES_KEY key;

	u8 cyphered_iv[16];
	u8 iv[16];
	unsigned int c_counter_pos;
} Openssl_ctx_ctr;


/** CTR STUFF **/

void gf_set_key_openssl_ctr(GF_Crypt* td, void *key)
{
	Openssl_ctx_ctr* ctx = (Openssl_ctx_ctr*)td->context;
	AES_set_encrypt_key(key, 128, &(ctx->key));
}

GF_Err gf_crypt_set_IV_openssl_ctr(GF_Crypt* td, const u8 *iv, u32 iv_size)
{
	Openssl_ctx_ctr* ctx = (Openssl_ctx_ctr*)td->context;

	ctx->c_counter_pos = ((u8*)iv)[0];
	memcpy(ctx->iv, &((u8*)iv)[1], iv_size - 1);
	memset(ctx->cyphered_iv, 0, 16);
	return GF_OK;
}

GF_Err gf_crypt_get_IV_openssl_ctr(GF_Crypt* td, u8 *iv, u32 *iv_size)
{
	Openssl_ctx_ctr* ctx = (Openssl_ctx_ctr*)td->context;

	*iv_size = AES_BLOCK_SIZE + 1;

	((u8 *)iv)[0] = ctx->c_counter_pos;
	memcpy(&((u8 *)iv)[1], ctx->iv, AES_BLOCK_SIZE);
	return GF_OK;
}

GF_Err gf_crypt_init_openssl_ctr(GF_Crypt* td, void *key, const void *iv)
{
	Openssl_ctx_ctr *ctx = (Openssl_ctx_ctr*)td->context;
	if (!ctx) {
		GF_SAFEALLOC(ctx, Openssl_ctx_ctr);
		if (!ctx) return GF_OUT_OF_MEM;

		td->context = ctx;
	}
	ctx->c_counter_pos = 0;
	if (iv != NULL) {
		memcpy(ctx->iv, &((u8*)iv)[0], AES_BLOCK_SIZE);
	}

	return GF_OK;
}

void gf_crypt_deinit_openssl_ctr(GF_Crypt* td)
{
}

GF_Err gf_crypt_crypt_openssl_ctr(GF_Crypt* td, u8 *plaintext, u32 len)
{
	Openssl_ctx_ctr* ctx = (Openssl_ctx_ctr*)td->context;

	CRYPTO_ctr128_encrypt(plaintext, plaintext, len, &ctx->key, ctx->iv, ctx->cyphered_iv, &ctx->c_counter_pos, (block128_f)AES_encrypt);

	return GF_OK;
}

GF_Err gf_crypt_encrypt_openssl_ctr(GF_Crypt* td, u8 *plaintext, u32 len)
{
	return gf_crypt_crypt_openssl_ctr(td, plaintext, len);
}

GF_Err gf_crypt_decrypt_openssl_ctr(GF_Crypt* td, u8 *ciphertext, u32 len)
{
	return gf_crypt_crypt_openssl_ctr(td, ciphertext, len);
}

/* ECB */
typedef struct {
	AES_KEY key;
} Openssl_ctx_ecb;

/** CBC STUFF **/

GF_Err gf_crypt_init_openssl_ecb(GF_Crypt* td, void *key, const void *iv)
{
	Openssl_ctx_ecb *ctx = (Openssl_ctx_ecb*)td->context;
	if (!ctx) {
		GF_SAFEALLOC(ctx, Openssl_ctx_ecb);
		if (ctx == NULL) return GF_OUT_OF_MEM;
		td->context = ctx;
	}
	return GF_OK;
}

void gf_crypt_deinit_openssl_ecb(GF_Crypt* td)
{
}

void gf_set_key_openssl_ecb(GF_Crypt* td, void *key)
{
	Openssl_ctx_ecb* ctx = (Openssl_ctx_ecb*)td->context;
	AES_set_encrypt_key(key, 128, &(ctx->key));
	AES_set_decrypt_key(key, 128, &(ctx->key));
}

GF_Err gf_crypt_set_IV_openssl_ecb(GF_Crypt* td, const u8 *iv, u32 iv_size)
{
	return GF_OK;
}

GF_Err gf_crypt_get_IV_openssl_ecb(GF_Crypt* td, u8 *iv, u32 *iv_size)
{
	*iv_size = AES_BLOCK_SIZE;
	memset(iv, 0, AES_BLOCK_SIZE);
	return GF_OK;
}

GF_Err gf_crypt_crypt_openssl_ecb(GF_Crypt* td, u8 *plaintext, u32 len, u32 aes_crypt_type)
{
	Openssl_ctx_ecb* ctx = (Openssl_ctx_ecb*)td->context;
	u32 iteration;
	u32 numberOfIterations = len / AES_BLOCK_SIZE;
	if (numberOfIterations * AES_BLOCK_SIZE < len) {
		return GF_BAD_PARAM;
	}

	for (iteration = 0; iteration < numberOfIterations; ++iteration) {
		AES_ecb_encrypt(plaintext + iteration*AES_BLOCK_SIZE, plaintext + iteration*AES_BLOCK_SIZE, &ctx->key, aes_crypt_type);
	}
	return GF_OK;
}

GF_Err gf_crypt_encrypt_openssl_ecb(GF_Crypt* td, u8 *plaintext, u32 len)
{
	return gf_crypt_crypt_openssl_ecb(td, plaintext, len, AES_ENCRYPT);
}

GF_Err gf_crypt_decrypt_openssl_ecb(GF_Crypt* td, u8 *ciphertext, u32 len)
{
	return gf_crypt_crypt_openssl_ecb(td, ciphertext, len, AES_DECRYPT);
}



GF_Err gf_crypt_open_open_openssl(GF_Crypt* td, GF_CRYPTO_MODE mode)
{
	td->mode = mode;
	switch (td->mode) {
	case GF_CBC:
		td->_init_crypt = gf_crypt_init_openssl_cbc;
		td->_deinit_crypt = gf_crypt_deinit_openssl_cbc;
		td->_set_key = gf_set_key_openssl_cbc;
		td->_crypt = gf_crypt_encrypt_openssl_cbc;
		td->_decrypt = gf_crypt_decrypt_openssl_cbc;
		td->_get_state = gf_crypt_get_IV_openssl_cbc;
		td->_set_state = gf_crypt_set_IV_openssl_cbc;
		break;
	case GF_CTR:
		td->_init_crypt = gf_crypt_init_openssl_ctr;
		td->_deinit_crypt = gf_crypt_deinit_openssl_ctr;
		td->_set_key = gf_set_key_openssl_ctr;
		td->_crypt = gf_crypt_encrypt_openssl_ctr;
		td->_decrypt = gf_crypt_decrypt_openssl_ctr;
		td->_get_state = gf_crypt_get_IV_openssl_ctr;
		td->_set_state = gf_crypt_set_IV_openssl_ctr;
		break;
	case GF_ECB:
		td->_init_crypt = gf_crypt_init_openssl_ecb;
		td->_deinit_crypt = gf_crypt_deinit_openssl_ecb;
		td->_set_key = gf_set_key_openssl_ecb;
		td->_crypt = gf_crypt_encrypt_openssl_ecb;
		td->_decrypt = gf_crypt_decrypt_openssl_ecb;
		td->_get_state = gf_crypt_get_IV_openssl_ecb;
		td->_set_state = gf_crypt_set_IV_openssl_ecb;
		break;
	default:
		return GF_BAD_PARAM;
		break;
	}

	td->algo = GF_AES_128;
	return GF_OK;
}

#endif

