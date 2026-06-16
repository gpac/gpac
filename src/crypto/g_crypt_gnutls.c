/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Telecom ParisTech 2018-2026
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

#ifdef GPAC_HAS_GNUTLS

#include <nettle/aes.h>
#include <nettle/cbc.h>
#include <nettle/ctr.h>
#include <nettle/macros.h>

typedef struct {

    struct aes_ctx enc_ctx;
    struct aes_ctx dec_ctx;

    u8 iv[AES_BLOCK_SIZE];
    u32 key_len;

    u8 buffer[AES_BLOCK_SIZE];
	u32 pos;
} NettleCryptoCtx;



void gf_crypt_set_key_gnutls(GF_Crypt* td, void* key) {

    NettleCryptoCtx* ctx = (NettleCryptoCtx*)td->context;
    if (!ctx || !key)
        return;

    aes_set_encrypt_key(&ctx->enc_ctx, ctx->key_len, key);
    aes_set_decrypt_key(&ctx->dec_ctx, ctx->key_len, key);

    ctx->pos = 0;
    memset(ctx->buffer, 0, AES_BLOCK_SIZE);

}


GF_Err gf_crypt_init_gnutls(GF_Crypt* td, void* key, const void* iv) {

    NettleCryptoCtx* ctx = (NettleCryptoCtx*)td->context;
    if (!ctx) {
        GF_SAFEALLOC(ctx, NettleCryptoCtx);
        if (ctx == NULL)
            return GF_OUT_OF_MEM;
        td->context = ctx;
    }
    ctx->key_len = AES128_KEY_SIZE;

    if (iv) {
        memcpy(ctx->iv, iv, AES_BLOCK_SIZE);
    }

    gf_crypt_set_key_gnutls(td, key);

    return GF_OK;
}

void gf_crypt_deinit_gnutls(GF_Crypt* td) {
}


//////////////// CBC ////////////////

GF_Err gf_crypt_get_IV_gnutls_cbc(GF_Crypt* td, u8* iv, u32* size) {

    NettleCryptoCtx* ctx = (NettleCryptoCtx*)td->context;
    if (!ctx || !iv || !size)
        return GF_BAD_PARAM;

    *size = AES_BLOCK_SIZE;
    memcpy(iv, ctx->iv, AES_BLOCK_SIZE);
    return GF_OK;
}


GF_Err gf_crypt_set_IV_gnutls_cbc(GF_Crypt* td, const u8* iv, u32 size) {

    NettleCryptoCtx* ctx = (NettleCryptoCtx*)td->context;
    if (!ctx || !iv)
        return GF_BAD_PARAM;
    if (size < AES_BLOCK_SIZE)
        return GF_BAD_PARAM;

    memcpy(ctx->iv, iv, AES_BLOCK_SIZE);

    return GF_OK;
}


GF_Err gf_crypt_encrypt_gnutls_cbc(GF_Crypt* td, u8* buffer, u32 size) {

    NettleCryptoCtx* ctx = (NettleCryptoCtx*)td->context;
    if (!ctx || !buffer)
        return GF_BAD_PARAM;
    if (size % AES_BLOCK_SIZE)
        return GF_BAD_PARAM;

    cbc_encrypt(&ctx->enc_ctx, (nettle_cipher_func*)aes_encrypt, AES_BLOCK_SIZE, ctx->iv, size, buffer, buffer);
    return GF_OK;
}

GF_Err gf_crypt_decrypt_gnutls_cbc(GF_Crypt* td, u8* buffer, u32 size) {

    NettleCryptoCtx* ctx = (NettleCryptoCtx*)td->context;
    if (!ctx || !buffer)
        return GF_BAD_PARAM;
    if (size % AES_BLOCK_SIZE)
        return GF_BAD_PARAM;

    cbc_decrypt(&ctx->dec_ctx, (nettle_cipher_func*)aes_decrypt, AES_BLOCK_SIZE, ctx->iv, size, buffer, buffer);
    return GF_OK;
}

/////////////////////////////////////

//////////////// CTR ////////////////

GF_Err gf_crypt_get_IV_gnutls_ctr(GF_Crypt* td, u8* iv, u32* size) {

    NettleCryptoCtx* ctx = (NettleCryptoCtx*)td->context;
    if (!ctx || !iv || !size)
        return GF_BAD_PARAM;

    *size = AES_BLOCK_SIZE+1;
    iv[0] = ctx->pos;
    memcpy(iv+1, ctx->iv, AES_BLOCK_SIZE);
    return GF_OK;
}

GF_Err gf_crypt_set_IV_gnutls_ctr(GF_Crypt* td, const u8* iv, u32 size) {

    NettleCryptoCtx* ctx = (NettleCryptoCtx*)td->context;
    if (!ctx || !iv)
        return GF_BAD_PARAM;
    if (size < AES_BLOCK_SIZE)
        return GF_BAD_PARAM;
    if (td->mode == GF_ECB)
        return GF_OK;

    memset(ctx->buffer, 0, AES_BLOCK_SIZE);

    if (size > AES_BLOCK_SIZE) {
        ctx->pos = ((u8*)iv)[0];
        memcpy(ctx->iv, &((u8*)iv)[1], AES_BLOCK_SIZE);
    } else {
        ctx->pos = 0;
        memcpy(ctx->iv, iv, AES_BLOCK_SIZE);
    }

    return GF_OK;
}


GF_Err gf_crypt_encrypt_gnutls_ctr(GF_Crypt* td, u8 *buffer, u32 size) {
    NettleCryptoCtx *ctx = (NettleCryptoCtx *)td->context;
    if (!ctx || !buffer || ctx->pos >= AES_BLOCK_SIZE) return GF_BAD_PARAM;

    u32 i = 0;

    // consume remainder of cached keystream block if pos > 0
    if (ctx->pos > 0) {
        u32 avail = AES_BLOCK_SIZE - ctx->pos;
        u32 take = (size < avail) ? size : avail;
        for (u32 j = 0; j < take; j++)
            buffer[j] ^= ctx->buffer[ctx->pos + j];
        ctx->pos += take;
        if (ctx->pos == AES_BLOCK_SIZE) ctx->pos = 0;
        i += take;
        size -= take;
    }

    // bulk: whole blocks via ctr_crypt
    if (size >= AES_BLOCK_SIZE) {
        u32 bulk = size & ~(AES_BLOCK_SIZE - 1);
        ctr_crypt(&ctx->enc_ctx, (nettle_cipher_func*)aes_encrypt, AES_BLOCK_SIZE, ctx->iv, bulk, buffer + i, buffer + i);
        i += bulk;
        size -= bulk;
    }

    // trailing partial block
    if (size > 0) {
        aes_encrypt(&ctx->enc_ctx, AES_BLOCK_SIZE, ctx->buffer, ctx->iv);
        INCREMENT(AES_BLOCK_SIZE, ctx->iv);
        for (u32 j = 0; j < size; j++)
            buffer[i + j] ^= ctx->buffer[j];
        ctx->pos = size;
    }

    return GF_OK;
}


/////////////////////////////////////

//////////////// ECB ////////////////


GF_Err gf_crypt_get_IV_gnutls_ecb(GF_Crypt* td, u8* iv, u32* size) {

    NettleCryptoCtx* ctx = (NettleCryptoCtx*)td->context;
    if (!ctx || !iv || !size)
        return GF_BAD_PARAM;

    *size = AES_BLOCK_SIZE;
    memset(iv, 0, AES_BLOCK_SIZE);
    return GF_OK;
}

GF_Err gf_crypt_set_IV_gnutls_ecb(GF_Crypt* td, const u8* iv, u32 size) {
    return GF_OK;
}

GF_Err gf_crypt_encrypt_gnutls_ecb(GF_Crypt* td, u8 *buffer, u32 size) {

	NettleCryptoCtx *ctx = (NettleCryptoCtx *)td->context;
	u32 i;

    if (!ctx || !buffer) return GF_BAD_PARAM;
	if (size % AES_BLOCK_SIZE) return GF_BAD_PARAM;

	for (i = 0; i < size; i += AES_BLOCK_SIZE) {
		aes_encrypt(&ctx->enc_ctx, AES_BLOCK_SIZE, buffer + i, buffer + i);
	}
	return GF_OK;
}

GF_Err gf_crypt_decrypt_gnutls_ecb(GF_Crypt* td, u8 *buffer, u32 size) {

	NettleCryptoCtx *ctx = (NettleCryptoCtx *)td->context;
	u32 i;

    if (!ctx || !buffer) return GF_BAD_PARAM;
	if (size % AES_BLOCK_SIZE) return GF_BAD_PARAM;

	for (i = 0; i < size; i += AES_BLOCK_SIZE) {
		aes_decrypt(&ctx->dec_ctx, AES_BLOCK_SIZE, buffer + i, buffer + i);
	}
	return GF_OK;
}

/////////////////////////////////////



GF_Err gf_crypt_open_gnutls(GF_Crypt* td, GF_CRYPTO_MODE mode) {

    td->mode = mode;
    td->algo = GF_AES_128;

    td->_init_crypt = gf_crypt_init_gnutls;
    td->_deinit_crypt = gf_crypt_deinit_gnutls;
    td->_set_key = gf_crypt_set_key_gnutls;

    switch (td->mode) {
    case GF_CBC:
        td->_crypt = gf_crypt_encrypt_gnutls_cbc;
        td->_decrypt = gf_crypt_decrypt_gnutls_cbc;
        td->_get_state = gf_crypt_get_IV_gnutls_cbc;
        td->_set_state = gf_crypt_set_IV_gnutls_cbc;
        break;
    case GF_CTR:
        td->_crypt = gf_crypt_encrypt_gnutls_ctr;
        td->_decrypt = gf_crypt_encrypt_gnutls_ctr;
        td->_get_state = gf_crypt_get_IV_gnutls_ctr;
        td->_set_state = gf_crypt_set_IV_gnutls_ctr;
        break;
    case GF_ECB:
        td->_crypt = gf_crypt_encrypt_gnutls_ecb;
        td->_decrypt = gf_crypt_decrypt_gnutls_ecb;
        td->_get_state = gf_crypt_get_IV_gnutls_ecb;
        td->_set_state = gf_crypt_set_IV_gnutls_ecb;
        break;
    default:
        return GF_BAD_PARAM;
        break;
    }

    return GF_OK;
}

#endif
