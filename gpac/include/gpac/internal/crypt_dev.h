/*
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Jean Le Feuvre
*			Copyright (c) Telecom ParisTech 2000-2012
*					All rights reserved
*
*  This file is part of GPAC / Crypto Tools sub-project
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

#ifndef _GF_CRYPT_DEV_H_
#define _GF_CRYPT_DEV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/crypt.h>

/*private - do not use*/
struct _gf_crypt_context
{
	GF_CRYPTO_ALGO algo; //single value for now
	GF_CRYPTO_MODE mode; //CBC or CTR

	/* Internal context for openSSL or tiny AES*/
	void *context;

	//ptr to encryption function
	GF_Err(*_init_crypt) (GF_Crypt *ctx, void*, const void*);
	void(*_deinit_crypt) (GF_Crypt *ctx);
	void(*_end_crypt) (GF_Crypt *ctx);
	void(*_set_key)(GF_Crypt *ctx, void*);
	GF_Err(*_crypt) (GF_Crypt *ctx, u8 *buffer, u32 size);
	GF_Err(*_decrypt) (GF_Crypt*, u8 *buffer, u32 size);
	GF_Err(*_set_state) (GF_Crypt*, const u8 *IV, u32 IV_size);
	GF_Err(*_get_state) (GF_Crypt*, u8 *IV, u32 *IV_size);
};

#ifdef GPAC_HAS_SSL
GF_Err gf_crypt_open_open_openssl(GF_Crypt* td, GF_CRYPTO_MODE mode);
#else
GF_Err gf_crypt_open_open_tinyaes(GF_Crypt* td, GF_CRYPTO_MODE mode);
#endif


#ifdef __cplusplus
}
#endif

#endif	/*_GF_CRYPT_DEV_H_*/
