/*
 * Copyright (C) 1998,1999,2000,2001,2002 Nikos Mavroyanopoulos
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

#ifndef GPAC_CRYPT_ISMA_ONLY

typedef struct cfb_buf {
	u8* s_register;
	u8* enc_s_register;
	int blocksize;
} CFB_BUFFER;

/* CFB MODE */

static GF_Err _init_mcrypt( CFB_BUFFER* buf, void *key, int lenofkey, void *IV, int size)
{

    buf->s_register = buf->enc_s_register = NULL;
    
    buf->blocksize = size;
/* For cfb */
	buf->s_register=malloc( size);
    if (buf->s_register==NULL) goto freeall;

	buf->enc_s_register=malloc( size);
    if (buf->enc_s_register==NULL) goto freeall;

	if (IV!=NULL) {
		memcpy(buf->s_register, IV, size);
	} else {
		memset(buf->s_register, 0, size);	
	}
/* End cfb */
	return GF_OK;

	freeall:
		if (buf->s_register) free(buf->s_register);
		if (buf->enc_s_register) free(buf->enc_s_register);
		return GF_OUT_OF_MEM;
}


static GF_Err _mcrypt_set_state( CFB_BUFFER* buf, void *IV, int size)
{
	memcpy(buf->enc_s_register, IV, size);
	memcpy(buf->s_register, IV, size);

	return GF_OK;
}

static GF_Err _mcrypt_get_state( CFB_BUFFER* buf, u8 *IV, int *size)
{
	if (*size < buf->blocksize) {
		*size = buf->blocksize;
		return GF_BAD_PARAM;
	}
	*size = buf->blocksize;

	memcpy( IV, buf->s_register, buf->blocksize);

	return GF_OK;
}


static void _end_mcrypt( CFB_BUFFER* buf) {
	free(buf->s_register);
	free(buf->enc_s_register);
}

static GF_Err _mcrypt( CFB_BUFFER* buf, void *plaintext, int len, int blocksize, void* akey, void (*func)(void*,void*), void (*func2)(void*,void*))
{				/* plaintext is 1 u8 (8bit cfb) */
	char *plain = plaintext;
	int i, j;
	void (*_mcrypt_block_encrypt) (void *, void *);

	_mcrypt_block_encrypt = func;

	for (j = 0; j < len; j++) {

		memcpy(buf->enc_s_register, buf->s_register, blocksize);

		_mcrypt_block_encrypt(akey, buf->enc_s_register);

		plain[j] ^= buf->enc_s_register[0];

/* Shift the register */
		for (i = 0; i < (blocksize - 1); i++)
			buf->s_register[i] = buf->s_register[i + 1];

		buf->s_register[blocksize - 1] = plain[j];
	}

	return GF_OK;

}


static GF_Err _mdecrypt( CFB_BUFFER* buf, void *plaintext, int len, int blocksize, void* akey, void (*func)(void*,void*), void (*func2)(void*,void*))
{				/* plaintext is 1 u8 (8bit ofb) */
	char *plain = plaintext;
	int i, j;
	void (*_mcrypt_block_encrypt) (void *, void *);

	_mcrypt_block_encrypt = func;

	for (j = 0; j < len; j++) {

		memcpy(buf->enc_s_register, buf->s_register, blocksize);

		_mcrypt_block_encrypt(akey, buf->enc_s_register);

/* Shift the register */
		for (i = 0; i < (blocksize - 1); i++)
			buf->s_register[i] = buf->s_register[i + 1];

		buf->s_register[blocksize - 1] = plain[j];

		plain[j] ^= buf->enc_s_register[0];

	}

	return GF_OK;
}

void gf_crypt_register_cfb(GF_Crypt *td)
{
	td->mode_name = "CFB";
	td->_init_mcrypt = _init_mcrypt;
	td->_end_mcrypt = _end_mcrypt;
	td->_mcrypt = _mcrypt;
	td->_mdecrypt = _mdecrypt;
	td->_mcrypt_get_state = _mcrypt_get_state;
	td->_mcrypt_set_state = _mcrypt_set_state;

	td->has_IV = 1;
	td->is_block_mode = 0;
	td->is_block_algo_mode = 1;
	td->mode_size = sizeof(CFB_BUFFER);
	td->mode_version = 20020310;
}

#endif

