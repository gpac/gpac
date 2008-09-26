/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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

/* The GPAC crypto lib is a simplified version of libmcrypt. */

#include <gpac/internal/crypt_dev.h>


static Bool gf_crypt_assign_algo(GF_Crypt *td, const char *algorithm)
{
	if (!stricmp(algorithm, "AES-128") || !stricmp(algorithm, "Rijndael-128")) { gf_crypt_register_rijndael_128(td); return 1; }
#ifndef GPAC_CRYPT_ISMA_ONLY
	else if (!stricmp(algorithm, "AES-192") || !stricmp(algorithm, "Rijndael-192")) { gf_crypt_register_rijndael_192(td); return 1; }
	else if (!stricmp(algorithm, "AES-256") || !stricmp(algorithm, "Rijndael-256")) { gf_crypt_register_rijndael_256(td); return 1; }
	else if (!stricmp(algorithm, "DES")) { gf_crypt_register_des(td); return 1; }
	else if (!stricmp(algorithm, "3DES")) { gf_crypt_register_3des(td); return 1; }
#endif
	return 0;
}
static Bool gf_crypt_assign_mode(GF_Crypt *td, const char *mode)
{
	if (!stricmp(mode, "CTR")) { gf_crypt_register_ctr(td); return 1; }
#ifndef GPAC_CRYPT_ISMA_ONLY
	else if (!stricmp(mode, "CBC")) { gf_crypt_register_cbc(td); return 1; }
	else if (!stricmp(mode, "CFB")) { gf_crypt_register_cfb(td); return 1; }
	else if (!stricmp(mode, "ECB")) { gf_crypt_register_ecb(td); return 1; }
	else if (!stricmp(mode, "nCFB")) { gf_crypt_register_ncfb(td); return 1; }
	else if (!stricmp(mode, "nOFB")) { gf_crypt_register_nofb(td); return 1; }
	else if (!stricmp(mode, "OFB")) { gf_crypt_register_ofb(td); return 1; }
	else if (!stricmp(mode, "STREAM")) { gf_crypt_register_stream(td); return 1; }
#endif
	return 0;
}

static GF_Crypt *gf_crypt_open_intern(const char *algorithm, const char *mode, Bool is_check)
{
	GF_Crypt *td;
	if ((!algorithm || !mode) && !is_check) return NULL;
	
	GF_SAFEALLOC(td, GF_Crypt);
	if (td==NULL) return NULL;

	
	if (algorithm && !gf_crypt_assign_algo(td, algorithm)) {
		free(td);
		return NULL;
	}
	if (mode && !gf_crypt_assign_mode(td, mode)) {
		free(td);
		return NULL;
	}
	if (is_check) return td;

	if (td->is_block_algo != td->is_block_algo_mode) {
		free(td);
		return NULL;
	}
	if (!td->_mcrypt || !td->_mdecrypt || !td->_mcrypt_set_state
		|| !td->a_decrypt || !td->a_encrypt || !td->a_set_key)
	{
		free(td);
		return NULL;
	}
	return td;
}

static void internal_end_mcrypt(GF_Crypt *td)
{
	if (!td || !td->keyword_given) return;
	free(td->keyword_given);
	td->keyword_given = NULL;

	if (td->akey) {
		free(td->akey);
		td->akey = NULL;
	}
	if (td->abuf) {
		td->_end_mcrypt(td->abuf);
		free(td->abuf);
		td->abuf = NULL;
	}
}


GF_EXPORT
GF_Crypt *gf_crypt_open(const char *algorithm, const char *mode)
{
	return gf_crypt_open_intern(algorithm, mode, 0);
}

GF_EXPORT
void gf_crypt_close(GF_Crypt *td)
{
	internal_end_mcrypt(td);
	free(td);
}

GF_Err gf_crypt_set_key(GF_Crypt *td, void *key, u32 keysize, const void *IV)
{
	GF_Err (*__mcrypt_set_key_stream) (void *, const void *, int, const void *, int);
	GF_Err (*__mcrypt_set_key_block) (void *, const void *, int);

	if (td->is_block_algo== 0) {
		/* stream */
		__mcrypt_set_key_stream = (mcrypt_setkeystream) td->a_set_key;
		if (__mcrypt_set_key_stream == NULL) return GF_BAD_PARAM;
		return __mcrypt_set_key_stream(td->akey, key, keysize, IV, (IV!=NULL) ? gf_crypt_get_iv_size(td) : 0);
	} else {
		__mcrypt_set_key_block = (mcrypt_setkeyblock) td->a_set_key;
		if (__mcrypt_set_key_block == NULL) return GF_BAD_PARAM;
		return __mcrypt_set_key_block(td->akey, key, keysize);
	}
}

GF_EXPORT
GF_Err gf_crypt_set_state(GF_Crypt *td, const void *iv, int size) 
{
	if (!td) return GF_BAD_PARAM;
	return td->_mcrypt_set_state(td->abuf, (void *) iv, size);
}

GF_Err gf_crypt_get_state(GF_Crypt *td, void *iv, int *size)
{
	if (!td) return GF_BAD_PARAM;
	return td->_mcrypt_get_state(td->abuf, iv, size);
}

u32 gf_crypt_get_block_size(GF_Crypt *td) { return td ? td->algo_block_size : 0; }

u32 gf_crypt_get_iv_size(GF_Crypt *td) 
{
	if (!td) return 0;
	if (td->is_block_algo_mode) return td->algo_block_size;
	return td->algo_IV_size;
}

u32 gf_crypt_get_key_size(GF_Crypt *td) { return td ? td->key_size : 0; }

u32 gf_crypt_get_supported_key_sizes(GF_Crypt *td, u32 *key_sizes)
{
	u32 i;
	if (!td || !td->num_key_sizes) return 0;
	for (i=0; i<td->num_key_sizes; i++) key_sizes[i] = td->key_sizes[i];
	return td->num_key_sizes;
}

Bool gf_crypt_is_block_algorithm(GF_Crypt *td) { return td ? td->is_block_algo : 0; }
const char *gf_crypt_get_algorithm_name(GF_Crypt *td) { return td ? td->algo_name : NULL; }
const char *gf_crypt_get_mode_name(GF_Crypt *td) { return td ? td->mode_name : NULL; }
Bool gf_crypt_is_block_mode(GF_Crypt *td) { return td ? td->is_block_mode : 0; }
Bool gf_crypt_mode_has_iv(GF_Crypt *td) { return td ? td->has_IV : 0; }
Bool gf_crypt_is_block_algorithm_mode(GF_Crypt *td) { return td ? td->is_block_algo_mode : 0; }
u32 gf_crypt_get_algorithm_version(GF_Crypt *td) { return td ? td->algo_version : 0; }
u32 gf_crypt_get_mode_version(GF_Crypt *td) { return td ? td->mode_version : 0; }



GF_EXPORT
GF_Err gf_crypt_init(GF_Crypt *td, void *key, u32 lenofkey, const void *IV)
{
	GF_Err e;
	u32 sizes[MAX_KEY_SIZES];
	u32 i, num_of_sizes, ok = 0;
	u32 key_size = gf_crypt_get_key_size(td);

	if ((lenofkey > key_size) || (lenofkey==0)) return GF_BAD_PARAM;
	num_of_sizes = gf_crypt_get_supported_key_sizes(td, sizes);
	if (num_of_sizes) {
		for (i=0; i < num_of_sizes; i++) {
			if (lenofkey == sizes[i]) {
				ok = 1;
				break;
			}
		}
	} else if (lenofkey <= gf_crypt_get_key_size(td)) {
		ok = 1;
	}

	if (ok == 0) { /* not supported key size */
		key_size = gf_crypt_get_key_size(td);
		if (sizes != NULL) {
			for (i = 0; i < num_of_sizes; i++) {
				if (lenofkey <= sizes[i]) {
					key_size = sizes[i];
					break;
				}
			}
		} else { /* well every key size is supported! */
			key_size = lenofkey;
		}
	} else {
		key_size = lenofkey;
	}

	td->keyword_given = (char*)malloc(sizeof(char)*gf_crypt_get_key_size(td));
	if (td->keyword_given==NULL) return GF_OUT_OF_MEM; 
	
	memmove(td->keyword_given, key, lenofkey);

	td->akey = (char*)malloc(sizeof(char)*td->algo_size);
	if (td->akey==NULL) {
		free(td->keyword_given);
		return GF_OUT_OF_MEM;
	}
	if (td->mode_size > 0) {
		td->abuf = (char*)malloc(sizeof(char)*td->mode_size);
		if (td->abuf==NULL) {
			free(td->keyword_given);
			free(td->akey);
			return GF_OUT_OF_MEM;
		}
	}
	e = td->_init_mcrypt(td->abuf, (void *) key, key_size, (void *) IV, gf_crypt_get_block_size(td));
	if (e!=GF_OK) {
		free(td->keyword_given);
		free(td->akey);
		free(td->abuf);
		return e;
	}

	e = gf_crypt_set_key(td, (void *) td->keyword_given, key_size, IV);

	if (e!=GF_OK) internal_end_mcrypt(td);
	return e;
}

void gf_crypt_deinit(GF_Crypt *td)
{
	internal_end_mcrypt(td);
}

GF_EXPORT
GF_Err gf_crypt_encrypt(GF_Crypt *td, void *plaintext, int len)
{
	if (!td) return GF_BAD_PARAM;
	return td->_mcrypt(td->abuf, plaintext, len, gf_crypt_get_block_size(td), td->akey, (mcryptfunc) td->a_encrypt, (mcryptfunc) td->a_decrypt);
}

GF_EXPORT
GF_Err gf_crypt_decrypt(GF_Crypt *td, void *ciphertext, int len)
{
	if (!td) return GF_BAD_PARAM;
	return td->_mdecrypt(td->abuf, ciphertext, len, gf_crypt_get_block_size(td), td->akey, (mcryptfunc) td->a_encrypt, (mcryptfunc) td->a_decrypt);
}


u32 gf_crypt_str_get_algorithm_version(const char *algorithm)
{
	u32 ret;
	GF_Crypt *td = gf_crypt_open_intern(algorithm, NULL, 1);
	ret = td ? td->algo_version : 0;
	gf_crypt_close(td);
	return ret;
}
u32 gf_crypt_str_get_mode_version(const char *mode)
{
	u32 ret;
	GF_Crypt *td = gf_crypt_open_intern(NULL, mode, 1);
	ret = td ? td->mode_version : 0;
	gf_crypt_close(td);
	return ret;
}
Bool gf_crypt_str_is_block_algorithm(const char *algorithm)
{
	Bool ret;
	GF_Crypt *td = gf_crypt_open_intern(algorithm, NULL, 1);
	ret = td ? td->is_block_algo : 0;
	gf_crypt_close(td);
	return ret;
}
Bool gf_crypt_str_is_block_algorithm_mode(const char *algorithm)
{
	Bool ret;
	GF_Crypt *td = gf_crypt_open_intern(algorithm, NULL, 1);
	ret = td ? td->is_block_algo_mode : 0;
	gf_crypt_close(td);
	return ret;
}
Bool gf_crypt_str_is_block_mode(const char *mode)
{
	Bool ret;
	GF_Crypt *td = gf_crypt_open_intern(NULL, mode, 1);
	ret = td ? td->is_block_mode : 0;
	gf_crypt_close(td);
	return ret;
}
u32 gf_crypt_str_module_get_algo_block_size(const char *algorithm)
{
	u32 ret;
	GF_Crypt *td = gf_crypt_open_intern(algorithm, NULL, 1);
	ret = td ? td->algo_block_size : 0;
	gf_crypt_close(td);
	return ret;
}
u32 gf_crypt_str_module_get_algo_key_size(const char *algorithm)
{
	u32 ret;
	GF_Crypt *td = gf_crypt_open_intern(algorithm, NULL, 1);
	ret = td ? td->key_size : 0;
	gf_crypt_close(td);
	return ret;
}
u32 gf_crypt_str_get_algo_supported_key_sizes(const char *algorithm, int *keys)
{
	u32 ret;
	GF_Crypt *td = gf_crypt_open_intern(algorithm, NULL, 1);
	ret = gf_crypt_get_supported_key_sizes(td, (u32 *)keys);
	gf_crypt_close(td);
	return ret;
}

