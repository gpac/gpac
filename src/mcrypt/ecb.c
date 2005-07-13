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

#ifndef GPAC_CRYPT_ISMA_ONLY

static GF_Err _init_mcrypt( void* ign, void *key, int lenofkey, void *IV, int size)
{
	return 0;

}

static GF_Err _mcrypt_set_state( void* buf, void *IV, int size) { return GF_BAD_PARAM; }
static GF_Err _mcrypt_get_state( void* buf, void *IV, int *size) { return GF_BAD_PARAM; }

static void _end_mcrypt (void* buf) {}

static GF_Err _mcrypt( void* ign, void *plaintext, int len, int blocksize, void* akey, void (*func)(void*,void*), void (*func2)(void*,void*))
{
	int j;
	char *plain = plaintext;
	void (*_mcrypt_block_encrypt) (void *, void *);

	_mcrypt_block_encrypt = func;
	
	for (j = 0; j < len / blocksize; j++) {
		_mcrypt_block_encrypt(akey, &plain[j * blocksize]);
	}
	if (j==0 && len!=0) return GF_BAD_PARAM; /* no blocks were encrypted */
	return GF_OK;
}



static GF_Err _mdecrypt( void* ign, void *ciphertext, int len, int blocksize, void* akey, void (*func)(void*,void*), void (*func2)(void*,void*))
{
	int j;
	char *cipher = ciphertext;
	void (*_mcrypt_block_decrypt) (void *, void *);

	_mcrypt_block_decrypt = func2;
	
	for (j = 0; j < len / blocksize; j++) {
		_mcrypt_block_decrypt(akey, &cipher[j * blocksize]);
	}
	if (j==0 && len!=0) return GF_BAD_PARAM; /* no blocks were encrypted */
	return GF_OK;
}

void gf_crypt_register_ecb(GF_Crypt *td)
{
	td->mode_name = "ECB";
	td->_init_mcrypt = _init_mcrypt;
	td->_end_mcrypt = _end_mcrypt;
	td->_mcrypt = _mcrypt;
	td->_mdecrypt = _mdecrypt;
	td->_mcrypt_get_state = _mcrypt_get_state;
	td->_mcrypt_set_state = _mcrypt_set_state;

	td->has_IV = 0;
	td->is_block_mode = 1;
	td->is_block_algo_mode = 1;
	td->mode_size = 0;
	td->mode_version = 20010801;
}

#endif

