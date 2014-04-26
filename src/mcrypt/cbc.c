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

#if !defined(GPAC_DISABLE_MCRYPT)

typedef struct cbc_buf {
	u32 *previous_ciphertext;
	u32 *previous_cipher;
	int blocksize;
} CBC_BUFFER;

/* CBC MODE */


static GF_Err _init_mcrypt( void* _buf,void *key, int lenofkey, void *IV, int size)
{
	CBC_BUFFER* buf = (CBC_BUFFER* )_buf;
	/* For cbc */
	buf->previous_ciphertext =
	    buf->previous_cipher = NULL;

	buf->blocksize = size;

	buf->previous_ciphertext = gf_malloc( size);
	buf->previous_cipher = gf_malloc( size);

	if (buf->previous_ciphertext==NULL ||
	        buf->previous_cipher==NULL) goto freeall;

	if (IV!=NULL) {
		memcpy(buf->previous_ciphertext, IV, size);
	} else {
		memset(buf->previous_ciphertext, 0, size);
	}

	return GF_OK;

freeall:
	if (buf->previous_ciphertext) gf_free(buf->previous_ciphertext);
	if (buf->previous_cipher) gf_free(buf->previous_cipher);
	return GF_OUT_OF_MEM;
}

static GF_Err _mcrypt_set_state( void* _buf, void *IV, int size)
{
	/* For cbc */
	CBC_BUFFER* buf = (CBC_BUFFER* )_buf;

	memcpy(buf->previous_ciphertext, IV, size);
	memcpy(buf->previous_cipher, IV, size);

	return GF_OK;
}

static GF_Err _mcrypt_get_state( void* _buf, void *IV, int *size)
{
	CBC_BUFFER* buf = (CBC_BUFFER* )_buf;
	if (*size < buf->blocksize) {
		*size = buf->blocksize;
		return GF_BAD_PARAM;
	}
	*size = buf->blocksize;

	memcpy( IV, buf->previous_ciphertext, buf->blocksize);

	return GF_OK;
}


static void _end_mcrypt( void* _buf) {
	CBC_BUFFER* buf = (CBC_BUFFER* )_buf;
	gf_free(buf->previous_ciphertext);
	gf_free(buf->previous_cipher);
}

static GF_Err _mcrypt( void* _buf, void *plaintext, int len, int blocksize, void* akey, void (*func)(void*,void*), void (*func2)(void*,void*))
{
	CBC_BUFFER* buf = (CBC_BUFFER* )_buf;
	u32 *fplain = plaintext;
	u32 *plain;
	int dblock, dlen, i, j;
	void (*_mcrypt_block_encrypt) (void *, void *);

	_mcrypt_block_encrypt = func;

	dblock = blocksize / sizeof(u32);
	dlen = len / blocksize;
	for (j = 0; j < dlen ; j++) {

		plain = &fplain[j * dblock];

		for (i = 0; i < dblock; i++) {
			plain[i] ^= buf->previous_ciphertext[i];
		}

		_mcrypt_block_encrypt(akey, plain);

		/* Copy the ciphertext to prev_ciphertext */
		memcpy(buf->previous_ciphertext, plain, blocksize);
	}
	if (j==0 && len!=0) return GF_BAD_PARAM;
	return GF_OK;
}



static GF_Err _mdecrypt( void* _buf, void *ciphertext, int len, int blocksize,void* akey, void (*func)(void*,void*), void (*func2)(void*,void*))
{
	CBC_BUFFER* buf = (CBC_BUFFER* )_buf;
	u32 *cipher;
	u32 *fcipher = ciphertext;
	int i, j, dlen, dblock;
	void (*_mcrypt_block_decrypt) (void *, void *);

	_mcrypt_block_decrypt = func2;


	dblock = blocksize / sizeof(u32);
	dlen = len / blocksize;
	for (j = 0; j < dlen; j++) {

		cipher = &fcipher[j * dblock];
		memcpy(buf->previous_cipher, cipher, blocksize);

		_mcrypt_block_decrypt(akey, cipher);
		for (i = 0; i < dblock; i++) {
			cipher[i] ^= buf->previous_ciphertext[i];
		}

		/* Copy the ciphertext to prev_cipher */
		memcpy(buf->previous_ciphertext, buf->previous_cipher, blocksize);

	}
	if (j==0 && len!=0) return GF_BAD_PARAM;
	return GF_OK;
}

void gf_crypt_register_cbc(GF_Crypt *td)
{
	td->mode_name = "CBC";
	td->_init_mcrypt = _init_mcrypt;
	td->_end_mcrypt = _end_mcrypt;
	td->_mcrypt = _mcrypt;
	td->_mdecrypt = _mdecrypt;
	td->_mcrypt_get_state = _mcrypt_get_state;
	td->_mcrypt_set_state = _mcrypt_set_state;

	td->has_IV = 1;
	td->is_block_mode = 1;
	td->is_block_algo_mode = 1;
	td->mode_size = sizeof(CBC_BUFFER);
	td->mode_version = 20010801;
}

#endif /*!defined(GPAC_CRYPT_ISMA_ONLY) */
