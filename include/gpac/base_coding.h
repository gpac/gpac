/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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

#ifndef _GF_BASE_CODING_H_
#define _GF_BASE_CODING_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>

/* Base64 encoding 
@in_buffer, in_buffer_size: input buffer and size
@out_buffer, @out_buffer_size: output buffer and available size in output

  return value: encoded buffer size
*/
u32 gf_base64_encode(unsigned char *in_buffer, u32 in_buffer_size, unsigned char *out_buffer, u32 out_buffer_size);

/* Base64 decoding 
@in_buffer, in_buffer_size: input buffer and size
@out_buffer, @out_buffer_size: output buffer and available size in output

  return value: decoded buffer size
*/
u32 gf_base64_decode(unsigned char *in_buffer, u32 in_buffer_size, unsigned char *out_buffer, u32 out_buffer_size);

/* Base16 encoding 
@in_buffer, in_buffer_size: input buffer and size
@out_buffer, @out_buffer_size: output buffer and available size in output

  return value: encoded buffer size
*/
u32 gf_base16_encode(unsigned char *in, u32 inSize, unsigned char *out, u32 outSize);

/* Base16 decoding 
@in_buffer, in_buffer_size: input buffer and size
@out_buffer, @out_buffer_size: output buffer and available size in output

  return value: encoded buffer size
*/
u32 gf_base16_decode(unsigned char *in, u32 inSize, unsigned char *out, u32 outSize);


#ifdef __cplusplus
}
#endif


#endif		/*_GF_BASE_CODING_H_*/

