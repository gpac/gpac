/*
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Rodolphe Fouquet, Jean Le Feuvre
*			Copyright (c) Motion Spell 2016
*                     (c) Telecom Paris 2022-2026
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
#include <openssl/evp.h>
#include <openssl/modes.h>

#include <math.h>

static void gf_crypt_ctr128_inc(u8 *counter)
{
	s32 i;
	for (i = AES_BLOCK_SIZE - 1; i >= 0; i--) {
		counter[i]++;
		if (counter[i])
			break;
	}
}

typedef struct {
	AES_KEY ossl_key;
	Bool use_evp, push_key;
	u8 key[16];
	EVP_CIPHER_CTX *ossl_evp;

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
		ctx->use_evp = gf_opts_get_bool("core", "no-evp") ? GF_FALSE : GF_TRUE;
		td->context = ctx;
	}
	
	if (iv != NULL) {
		memcpy(ctx->previous_ciphertext, iv, AES_BLOCK_SIZE);
	}
	return GF_OK;
}

void gf_crypt_deinit_openssl_cbc(GF_Crypt* td)
{
	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;
	if (!ctx) return;
	if (ctx->ossl_evp) {
		EVP_CIPHER_CTX_free(ctx->ossl_evp);
		ctx->ossl_evp = NULL;
	}
}

void gf_set_key_openssl_cbc(GF_Crypt* td, void *key)
{
	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;
	memcpy(ctx->key, key, 16);
	ctx->push_key = GF_TRUE;
	if (ctx->use_evp && !ctx->ossl_evp)
		ctx->ossl_evp = EVP_CIPHER_CTX_new();
}

GF_Err gf_crypt_set_IV_openssl_cbc(GF_Crypt* td, const u8 *iv, u32 iv_size)
{
	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;
	if (iv_size > AES_BLOCK_SIZE) return GF_BAD_PARAM;
	memcpy(ctx->previous_ciphertext, iv, iv_size);
	if (iv_size < AES_BLOCK_SIZE)
		memset(ctx->previous_ciphertext + iv_size, 0, AES_BLOCK_SIZE - iv_size);
	return GF_OK;
}

GF_Err gf_crypt_get_IV_openssl_cbc(GF_Crypt* td, u8 *iv, u32 *iv_size)
{
	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;
	*iv_size = AES_BLOCK_SIZE;
	memcpy(iv, ctx->previous_ciphertext, AES_BLOCK_SIZE);
	return GF_OK;
}

GF_Err gf_crypt_crypt_openssl_cbc(GF_Crypt* td, u8 *plaintext, u32 len, u32 aes_crypt_type)
{
	Openssl_ctx_cbc* ctx = (Openssl_ctx_cbc*)td->context;
	if (ctx->use_evp) {
		u32 full_len = (len / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
		u32 rem_len = len - full_len;
		int out_len = 0;
		if (!ctx->ossl_evp) return GF_IO_ERR;
		if (!len) return GF_OK;

		if (!EVP_CipherInit_ex(ctx->ossl_evp, EVP_aes_128_cbc(), NULL, ctx->key, ctx->previous_ciphertext, (aes_crypt_type == AES_ENCRYPT) ? 1 : 0))
			return GF_IO_ERR;

		if (!EVP_CIPHER_CTX_set_padding(ctx->ossl_evp, 0))
			return GF_IO_ERR;

		if (full_len) {
			if (aes_crypt_type == AES_DECRYPT) {
				u8 last_cblock_in[AES_BLOCK_SIZE];
				memcpy(last_cblock_in, plaintext + full_len - AES_BLOCK_SIZE, AES_BLOCK_SIZE);
				if (!EVP_CipherUpdate(ctx->ossl_evp, plaintext, &out_len, plaintext, (int)full_len))
					return GF_IO_ERR;
				memcpy(ctx->previous_ciphertext, last_cblock_in, AES_BLOCK_SIZE);
			} else {
				if (!EVP_CipherUpdate(ctx->ossl_evp, plaintext, &out_len, plaintext, (int)full_len))
					return GF_IO_ERR;
				memcpy(ctx->previous_ciphertext, plaintext + full_len - AES_BLOCK_SIZE, AES_BLOCK_SIZE);
			}
		}

		if (rem_len) {
			u8 tmp_out[AES_BLOCK_SIZE];
			memset(ctx->padded_input, 0, AES_BLOCK_SIZE);
			memcpy(ctx->padded_input, plaintext + full_len, rem_len);
			if (!EVP_CipherUpdate(ctx->ossl_evp, tmp_out, &out_len, ctx->padded_input, AES_BLOCK_SIZE))
				return GF_IO_ERR;
			memcpy(plaintext + full_len, tmp_out, rem_len);
			if (aes_crypt_type == AES_ENCRYPT)
				memcpy(ctx->previous_ciphertext, tmp_out, AES_BLOCK_SIZE);
			else
				memcpy(ctx->previous_ciphertext, ctx->padded_input, AES_BLOCK_SIZE);
		}
		return GF_OK;
	}

	if (ctx->push_key) {
		if (aes_crypt_type)
			AES_set_encrypt_key(ctx->key, 128, &(ctx->ossl_key));
		else
			AES_set_decrypt_key(ctx->key, 128, &(ctx->ossl_key));
		ctx->push_key = GF_FALSE;
	}
	u32 full_len = (len / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
	u32 rem_len = len - full_len;

	if (full_len) {
		AES_cbc_encrypt(plaintext, plaintext, full_len, &ctx->ossl_key, ctx->previous_ciphertext, aes_crypt_type);
	}
	if (rem_len) {
		memset(ctx->padded_input, 0, AES_BLOCK_SIZE);
		memcpy(ctx->padded_input, plaintext + full_len, rem_len);
		AES_cbc_encrypt(ctx->padded_input, ctx->block, AES_BLOCK_SIZE, &ctx->ossl_key, ctx->previous_ciphertext, aes_crypt_type);
		memcpy(plaintext + full_len, ctx->block, rem_len);
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
	AES_KEY ossl_key;
	Bool use_evp;
	u8 raw_key[16];
	EVP_CIPHER_CTX *ossl_evp;

	u8 cyphered_iv[16];
	u8 iv[16];
	unsigned int c_counter_pos;
} Openssl_ctx_ctr;


/** CTR STUFF **/

void gf_set_key_openssl_ctr(GF_Crypt* td, void *key)
{
	Openssl_ctx_ctr* ctx = (Openssl_ctx_ctr*)td->context;
	memcpy(ctx->raw_key, key, 16);
	AES_set_encrypt_key(key, 128, &ctx->ossl_key);
	if (ctx->use_evp && !ctx->ossl_evp)
		ctx->ossl_evp = EVP_CIPHER_CTX_new();
}

GF_Err gf_crypt_set_IV_openssl_ctr(GF_Crypt* td, const u8 *iv, u32 iv_size)
{
	Openssl_ctx_ctr* ctx = (Openssl_ctx_ctr*)td->context;
	if (!iv_size || (iv_size > AES_BLOCK_SIZE + 1)) return GF_BAD_PARAM;

	ctx->c_counter_pos = ((u8*)iv)[0];
	if (ctx->c_counter_pos >= AES_BLOCK_SIZE)
		return GF_BAD_PARAM;
	memcpy(ctx->iv, &((u8*)iv)[1], iv_size - 1);
	if (iv_size < AES_BLOCK_SIZE + 1)
		memset(ctx->iv + iv_size - 1, 0, AES_BLOCK_SIZE + 1 - iv_size);
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
		ctx->use_evp = gf_opts_get_bool("core", "no-evp") ? GF_FALSE : GF_TRUE;
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
	Openssl_ctx_ctr* ctx = (Openssl_ctx_ctr*)td->context;
	if (!ctx) return;
	if (ctx->ossl_evp) {
		EVP_CIPHER_CTX_free(ctx->ossl_evp);
		ctx->ossl_evp = NULL;
	}
}

GF_Err gf_crypt_crypt_openssl_ctr(GF_Crypt* td, u8 *plaintext, u32 len)
{
	Openssl_ctx_ctr* ctx = (Openssl_ctx_ctr*)td->context;
	if (ctx->use_evp) {
		u8 *p = plaintext;
		int out_len = 0;
		if (!ctx->ossl_evp) return GF_IO_ERR;

		if (!EVP_EncryptInit_ex(ctx->ossl_evp, EVP_aes_128_ecb(), NULL, ctx->raw_key, NULL))
			return GF_IO_ERR;
		if (!EVP_CIPHER_CTX_set_padding(ctx->ossl_evp, 0))
			return GF_IO_ERR;

		while (len) {
			if (!ctx->c_counter_pos) {
				if (!EVP_EncryptUpdate(ctx->ossl_evp, ctx->cyphered_iv, &out_len, ctx->iv, AES_BLOCK_SIZE))
					return GF_IO_ERR;
				if (out_len != AES_BLOCK_SIZE)
					return GF_IO_ERR;
				gf_crypt_ctr128_inc(ctx->iv);
			}
			*p ^= ctx->cyphered_iv[ctx->c_counter_pos];
			p++;
			len--;
			ctx->c_counter_pos = (ctx->c_counter_pos + 1) & (AES_BLOCK_SIZE - 1);
		}
		return GF_OK;
	}

	CRYPTO_ctr128_encrypt(plaintext, plaintext, len, &ctx->ossl_key, ctx->iv, ctx->cyphered_iv, &ctx->c_counter_pos, (block128_f)AES_encrypt);

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
	AES_KEY ossl_key;
	Bool use_evp;
	Bool push_key;
	u8 key[16];
	EVP_CIPHER_CTX *ossl_evp;
} Openssl_ctx_ecb;

GF_Err gf_crypt_init_openssl_ecb(GF_Crypt* td, void *key, const void *iv)
{
	Openssl_ctx_ecb *ctx = (Openssl_ctx_ecb*)td->context;
	if (!ctx) {
		GF_SAFEALLOC(ctx, Openssl_ctx_ecb);
		if (ctx == NULL) return GF_OUT_OF_MEM;
		ctx->use_evp = gf_opts_get_bool("core", "no-evp") ? GF_FALSE : GF_TRUE;
		td->context = ctx;
	}
	return GF_OK;
}

void gf_crypt_deinit_openssl_ecb(GF_Crypt* td)
{
	Openssl_ctx_ecb* ctx = (Openssl_ctx_ecb*)td->context;
	if (!ctx) return;
	if (ctx->ossl_evp) {
		EVP_CIPHER_CTX_free(ctx->ossl_evp);
		ctx->ossl_evp = NULL;
	}
}

void gf_set_key_openssl_ecb(GF_Crypt* td, void *key)
{
	Openssl_ctx_ecb* ctx = (Openssl_ctx_ecb*)td->context;
	memcpy(ctx->key, key, 16);
	ctx->push_key = GF_TRUE;
	if (ctx->use_evp && !ctx->ossl_evp)
		ctx->ossl_evp = EVP_CIPHER_CTX_new();
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
	if (ctx->use_evp) {
		EVP_CIPHER_CTX *evp_ctx = ctx->ossl_evp;
		int out_len = 0;
		if (!evp_ctx) return GF_IO_ERR;
		if (len % AES_BLOCK_SIZE)
			return GF_BAD_PARAM;
		if (!EVP_CipherInit_ex(evp_ctx, EVP_aes_128_ecb(), NULL, ctx->key, NULL, (aes_crypt_type == AES_ENCRYPT) ? 1 : 0))
			return GF_IO_ERR;
		if (!EVP_CIPHER_CTX_set_padding(evp_ctx, 0))
			return GF_IO_ERR;
		if (len && !EVP_CipherUpdate(evp_ctx, plaintext, &out_len, plaintext, (int)len))
			return GF_IO_ERR;
		return GF_OK;
	}

	if (ctx->push_key) {
		if (aes_crypt_type == AES_ENCRYPT)
			AES_set_encrypt_key(ctx->key, 128, &ctx->ossl_key);
		else
			AES_set_decrypt_key(ctx->key, 128, &ctx->ossl_key);
		ctx->push_key = GF_FALSE;
	}

	u32 iteration;
	u32 numberOfIterations = len / AES_BLOCK_SIZE;
	if (numberOfIterations * AES_BLOCK_SIZE < len) {
		return GF_BAD_PARAM;
	}

	for (iteration = 0; iteration < numberOfIterations; ++iteration) {
		AES_ecb_encrypt(plaintext + iteration*AES_BLOCK_SIZE, plaintext + iteration*AES_BLOCK_SIZE, &ctx->ossl_key, aes_crypt_type);
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

