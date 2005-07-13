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

#ifndef _GF_TOKEN_H_
#define _GF_TOKEN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>

/*********************************************************************
					a simple string parser
**********************************************************************/
/*get string component 
	@Buffer: src string
	@start: start offset in src
	@SeparatorSet: separator characters used
	@Container, @ContainerSize: output
*/
s32 gf_token_get(unsigned char *Buffer, s32 Start, unsigned char *SeparatorSet, unsigned char *Container, s32 ContainerSize);
/*line delimeters checked: \r\n, \n and \r*/
s32 gf_token_get_line(unsigned char	*Buffer, u32 Start, u32 Size, unsigned char *LineBuffer, u32 LineBufferSize);
/*locates pattern in buffer*/
s32 gf_token_find(unsigned char *Buffer, u32 Start, u32 Size, unsigned char *Pattern);


#ifdef __cplusplus
}
#endif


#endif		/*_GF_TOKEN_H_*/

