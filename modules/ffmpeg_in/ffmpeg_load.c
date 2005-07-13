/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / FFMPEG module
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

#include "ffmpeg_in.h"



Bool QueryInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return 1;
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return 1;
	return 0;
}

void *LoadInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return FFDEC_Load();
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return New_FFMPEG_Demux();
	return NULL;
}

void ShutdownInterface(void *ifce)
{
	GF_BaseInterface *ptr = (GF_BaseInterface *)ifce;
	switch (ptr->InterfaceType) {
	case GF_MEDIA_DECODER_INTERFACE:
		FFDEC_Delete(ptr);
		break;
	case GF_NET_CLIENT_INTERFACE:
		Delete_FFMPEG_Demux(ptr);
		break;
	}
}
