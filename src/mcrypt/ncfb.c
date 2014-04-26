/*
 * Copyright (C) 1998,1999,2000,2001 Nikos Mavroyanopoulos
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

#include <gpac/internal/crypt_dev.h>

#if !defined(GPAC_CRYPT_ISMA_ONLY) && !defined(GPAC_DISABLE_MCRYPT)

typedef struct ncfb_buf {
	u8*   enc_s_register;
	u8* s_register;
	int   s_register_pos;
	int   blocksize;
} nCFB_BUFFER;

/* nCFB MODE */

static GF_Err _init_mcrypt( nCFB_BUFFER* buf, void *key, int lenofkey, void *IV, int size)
{
	buf->enc_s_register = buf->s_register = NULL;
	buf->s_register_pos = 0;

	buf->blocksize = size;

	/* For cfb */
	buf->enc_s_register=gf_malloc( size);
	if (buf->enc_s_register==NULL) goto freeall;

	buf->s_register=gf_malloc( size);
	if (buf->s_register==NULL) goto freeall;

	if (IV!=NULL) {
		memcpy(buf->enc_s_register, IV, size);
		memcpy(buf->s_register, IV, size);
	} else {
		memset(buf->enc_s_register, 0, size);
		memset(buf->s_register, 0, size);
	}

	/* End ncfb */

	return GF_OK;
freeall:
	if (buf->enc_s_register) gf_free(buf->enc_s_register);
	if (buf->s_register) gf_free(buf->s_register);
	return GF_BAD_PARAM;
}

static GF_Err _mcrypt_set_state( nCFB_BUFFER* buf, u8 *IV, int size)
{
	buf->s_register_pos = IV[0];
	memcpy(buf->enc_s_register, &IV[1], size-1);
	memcpy(buf->s_register, &IV[1], size-1);

	return GF_OK;
}

static GF_Err _mcrypt_get_state( nCFB_BUFFER* buf, u8 *IV, int *size)
{
	if (*size < buf->blocksize + 1) {
		*size = buf->blocksize + 1;
		return GF_BAD_PARAM;
	}
	*size = buf->blocksize + 1;

	IV[0] = buf->s_register_pos;
	memcpy( &IV[1], buf->s_register, buf->blocksize);

	return GF_OK;
}

static void _end_mcrypt( nCFB_BUFFER* buf) {
	gf_free(buf->enc_s_register);
	gf_free(buf->s_register);
}

GFINLINE static
void xor_stuff_en( nCFB_BUFFER *buf, void* akey, void (*func)(void*,void*), u8* plain,  int blocksize, int xor_size)
{
	void (*_mcrypt_block_encrypt) (void *, void *);

	_mcrypt_block_encrypt = func;

	if (xor_size == blocksize) {
		if (buf->s_register_pos == 0) {

			memcpy(buf->enc_s_register, buf->s_register, blocksize);

			_mcrypt_block_encrypt(akey, buf->enc_s_register);

			memxor( plain, buf->enc_s_register, blocksize);

			memcpy(buf->s_register, plain, blocksize);

		} else {
			int size = blocksize - buf->s_register_pos;

			memxor( plain, &buf->enc_s_register[buf->s_register_pos],
			        size);

			memcpy(buf->enc_s_register, buf->s_register, blocksize);

			_mcrypt_block_encrypt(akey, buf->enc_s_register);

			memxor( &plain[size], buf->enc_s_register,
			        buf->s_register_pos);

			memcpy( &buf->s_register[size],
			        plain, buf->s_register_pos);

			/* buf->s_register_pos remains the same */
		}
	} else { /* xor_size != blocksize */
		if (buf->s_register_pos == 0) {
			memcpy(buf->enc_s_register, buf->s_register, blocksize);

			_mcrypt_block_encrypt(akey, buf->enc_s_register);

			memxor( plain, buf->enc_s_register, xor_size);

			memcpy(buf->s_register, plain, xor_size);

			buf->s_register_pos = xor_size;
		} else {
			int size = blocksize - buf->s_register_pos;
			int min_size =  size < xor_size ? size: xor_size;

			memxor( plain, &buf->enc_s_register[buf->s_register_pos],
			        min_size);

			memcpy( &buf->s_register[buf->s_register_pos], plain, min_size);

			buf->s_register_pos += min_size;

			if (min_size >= xor_size)
				return;

			memcpy(buf->enc_s_register, buf->s_register, blocksize);

			_mcrypt_block_encrypt(akey, buf->enc_s_register);

			memxor( &plain[min_size], buf->s_register,
			        xor_size - min_size);

			buf->s_register_pos = xor_size - min_size;

			memcpy(buf->s_register, plain, xor_size - min_size);
		}

	}
	return;
}

GFINLINE static
void xor_stuff_de( nCFB_BUFFER *buf, void* akey, void (*func)(void*,void*), u8* cipher,  int blocksize, int xor_size)
{
	void (*_mcrypt_block_encrypt) (void *, void *);

	_mcrypt_block_encrypt = func;

	if (xor_size == blocksize) {
		if (buf->s_register_pos == 0) {

			memcpy(buf->enc_s_register, buf->s_register, blocksize);

			_mcrypt_block_encrypt(akey, buf->enc_s_register);

			memcpy(buf->s_register, cipher, blocksize);

			memxor( cipher, buf->enc_s_register, blocksize);


		} else {
			int size = blocksize - buf->s_register_pos;

			memxor( cipher, &buf->enc_s_register[buf->s_register_pos],
			        size);

			memcpy(buf->enc_s_register, buf->s_register, blocksize);

			_mcrypt_block_encrypt(akey, buf->enc_s_register);

			memcpy( &buf->s_register[size],
			        cipher, buf->s_register_pos);

			memxor( &cipher[size], buf->enc_s_register,
			        buf->s_register_pos);


			/* buf->s_register_pos remains the same */
		}
	} else { /* xor_size != blocksize */
		if (buf->s_register_pos == 0) {
			memcpy(buf->enc_s_register, buf->s_register, blocksize);

			_mcrypt_block_encrypt(akey, buf->enc_s_register);

			memcpy(buf->s_register, cipher, xor_size);

			memxor( cipher, buf->enc_s_register, xor_size);


			buf->s_register_pos = xor_size;
		} else {
			int size = blocksize - buf->s_register_pos;
			int min_size =  size < xor_size ? size: xor_size;

			memxor( cipher, &buf->enc_s_register[buf->s_register_pos],
			        min_size);

			memcpy( &buf->s_register[buf->s_register_pos], cipher, min_size);

			buf->s_register_pos += min_size;

			if (min_size >= xor_size)
				return;

			memcpy(buf->enc_s_register, buf->s_register, blocksize);

			_mcrypt_block_encrypt(akey, buf->enc_s_register);

			memcpy(buf->s_register, cipher, xor_size - min_size);

			memxor( &cipher[min_size], buf->s_register,
			        xor_size - min_size);

			buf->s_register_pos = xor_size - min_size;

		}

	}
	return;
}


static GF_Err _mcrypt( nCFB_BUFFER* buf,void *plaintext, int len, int blocksize, void* akey, void (*func)(void*,void*), void (*func2)(void*,void*))
{	/* plaintext is n*blocksize bytes (nbit cfb) */
	u8* plain;
	int dlen, j=0;
	void (*_mcrypt_block_encrypt) (void *, void *);
	int modlen;

	_mcrypt_block_encrypt = func;

	dlen = len / blocksize;
	plain = plaintext;
	for (j = 0; j < dlen; j++) {
		xor_stuff_en( buf, akey, func, plain, blocksize, blocksize);

		plain += blocksize;

	}
	modlen = len % blocksize;
	if (modlen > 0) {
		xor_stuff_en( buf, akey, func, plain, blocksize, modlen);
	}

	return GF_OK;
}


static GF_Err _mdecrypt( nCFB_BUFFER* buf,void *plaintext, int len, int blocksize, void* akey, void (*func)(void*,void*), void (*func2)(void*,void*))
{	/* plaintext is n*blocksize bytes (nbit cfb) */
	u8* plain;
	int dlen, j=0;
	void (*_mcrypt_block_encrypt) (void *, void *);
	int modlen;

	_mcrypt_block_encrypt = func;

	dlen = len / blocksize;
	plain = plaintext;
	for (j = 0; j < dlen; j++) {
		xor_stuff_de( buf, akey, func, plain, blocksize, blocksize);

		plain += blocksize;

	}
	modlen = len % blocksize;
	if (modlen > 0) {
		xor_stuff_de( buf, akey, func, plain, blocksize, modlen);
	}

	return GF_OK;
}


void gf_crypt_register_ncfb(GF_Crypt *td)
{
	td->mode_name = "nCFB";
	td->_init_mcrypt = _init_mcrypt;
	td->_end_mcrypt = _end_mcrypt;
	td->_mcrypt = _mcrypt;
	td->_mdecrypt = _mdecrypt;
	td->_mcrypt_get_state = _mcrypt_get_state;
	td->_mcrypt_set_state = _mcrypt_set_state;

	td->has_IV = 1;
	td->is_block_mode = 0;
	td->is_block_algo_mode = 1;
	td->mode_size = sizeof(nCFB_BUFFER);
	td->mode_version = 20020307;
}

#endif /*!defined(GPAC_CRYPT_ISMA_ONLY) && !defined(GPAC_DISABLE_MCRYPT)*/
