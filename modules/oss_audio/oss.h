/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / linux_oss audio render module
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


#ifndef __LINUX_AUDIO_OSS_H_
#define __LINUX_AUDIO_OSS_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <gpac/module/av_hw.h>

#define OSS_AUDIO_DEVICE	"/dev/dsp"

typedef struct 
{
	int audio_device;
	int buf_size;
	char *wav_buf;
} OSSContext;


#ifdef __cplusplus
}
#endif

#endif
