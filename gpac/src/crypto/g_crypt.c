/*
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Jean Le Feuvre
*			Copyright (c) Telecom ParisTech 2000-2018
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

GF_EXPORT
GF_Crypt *gf_crypt_open(GF_CRYPTO_ALGO algorithm, GF_CRYPTO_MODE mode)
{
	GF_Crypt *td;
	GF_Err e;

	GF_SAFEALLOC(td, GF_Crypt);
	if (td == NULL) return NULL;

#ifdef GPAC_HAS_SSL
	e = gf_crypt_open_open_openssl(td, mode);
#else
	e = gf_crypt_open_open_tinyaes(td, mode);
#endif

	if (e != GF_OK) {
		gf_free(td);
		return NULL;
	}
	return td;
}

GF_EXPORT
void gf_crypt_close(GF_Crypt *td)
{
	if (!td) return;
	td->_deinit_crypt(td);
	gf_free(td->context);
	gf_free(td);
}

GF_EXPORT
GF_Err gf_crypt_set_key(GF_Crypt *td, void *key)
{
	td->_set_key(td, key);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_crypt_set_IV(GF_Crypt *td, const void *iv, u32 size)
{
	if (!td) return GF_BAD_PARAM;
	return td->_set_state(td, (void *)iv, size);
}

GF_EXPORT
GF_Err gf_crypt_get_IV(GF_Crypt *td, void *iv, u32 *size)
{
	if (!td) return GF_BAD_PARAM;
	return td->_get_state(td, iv, size);
}


GF_EXPORT
GF_Err gf_crypt_init(GF_Crypt *td, void *key, const void *IV)
{
	GF_Err e;

	e = td->_init_crypt(td, key, IV);
	if (e != GF_OK) gf_crypt_close(td);
	//need for openssl we have 2 passes init
	td->_set_key(td, key);
	return e;
}

GF_EXPORT
GF_Err gf_crypt_encrypt(GF_Crypt *td, void *plaintext, u32 len)
{
	if (!td) return GF_BAD_PARAM;
	return td->_crypt(td, plaintext, len);
}

GF_EXPORT
GF_Err gf_crypt_decrypt(GF_Crypt *td, void *ciphertext, u32 len)
{
	if (!td) return GF_BAD_PARAM;
	if (!len) return GF_OK;
	return td->_decrypt(td, ciphertext, len);
}
