/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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


#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)

#if defined(_WIN32_WCE)
#pragma comment(lib, "toolhelp")
#pragma comment(lib, "winsock")
#endif

#define _TOSTR(_val) #_val
#define TOSTR(_val) _TOSTR(_val)

#pragma comment(lib, "avcodec-"TOSTR(LIBAVCODEC_VERSION_MAJOR) )
#pragma comment(lib, "avformat-"TOSTR(LIBAVFORMAT_VERSION_MAJOR) )
#pragma comment(lib, "avutil-"TOSTR(LIBAVUTIL_VERSION_MAJOR) )
#pragma comment(lib, "swscale-"TOSTR(LIBSWSCALE_VERSION_MAJOR) )

#endif


GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_MEDIA_DECODER_INTERFACE,
#ifndef DISABLE_FFMPEG_DEMUX
		GF_NET_CLIENT_INTERFACE,
#endif
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return FFDEC_Load();
#ifndef DISABLE_FFMPEG_DEMUX
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return New_FFMPEG_Demux();
#endif
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_MEDIA_DECODER_INTERFACE:
		FFDEC_Delete(ifce);
		break;
#ifndef DISABLE_FFMPEG_DEMUX
	case GF_NET_CLIENT_INTERFACE:
		Delete_FFMPEG_Demux(ifce);
		break;
#endif
	}
}


GPAC_MODULE_STATIC_DELARATION( ffmpeg )
