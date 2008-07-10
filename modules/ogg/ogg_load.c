/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / XIPH.org module
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

#include "ogg_in.h"


static Bool OGG_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, u32 ObjectType, char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	/*video decs*/
	if (StreamType == GF_STREAM_VISUAL) {
		switch (ObjectType) {
#ifdef GPAC_HAS_THEORA
		case GPAC_OTI_MEDIA_OGG: 
			if (decSpecInfo && (decSpecInfoSize>=9)  && !strncmp((char *) &decSpecInfo[3], "theora", 6)) {
				return NewTheoraDecoder(dec);
			}
			return 0;
		case 0: return 1;/*query for types*/
#endif
		default: return 0;
		}
	}
	/*audio decs*/	
	if (StreamType == GF_STREAM_AUDIO) {
		switch (ObjectType) {
#ifdef GPAC_HAS_VORBIS
		case GPAC_OTI_MEDIA_OGG: 
			if (decSpecInfo && (decSpecInfoSize>=9)  && !strncmp((char *) &decSpecInfo[3], "vorbis", 6)) {
				return NewVorbisDecoder(dec);
			}
			return 0;
		case 0: return 1;
#endif
		default: return 0;
		}
	}
	return 0;
}


GF_BaseDecoder *OGG_LoadDecoder()
{
	GF_MediaDecoder *ifce;
	OGGWraper *wrap;
	GF_SAFEALLOC(ifce, GF_MediaDecoder);
	GF_SAFEALLOC(wrap, OGGWraper);
	ifce->privateStack = wrap;
	ifce->CanHandleStream = OGG_CanHandleStream;
	GF_REGISTER_MODULE_INTERFACE(ifce, GF_MEDIA_DECODER_INTERFACE, "GPAC XIPH.org package", "gpac distribution")

	/*other interfaces will be setup at run time*/
	return (GF_BaseDecoder *)ifce;
}

void DeleteOGGDecoder(GF_BaseDecoder *ifcd)
{
	OGGWraper *wrap = (OGGWraper *)ifcd->privateStack;
	switch (wrap->type) {
#ifdef GPAC_HAS_VORBIS
	case OGG_VORBIS: DeleteVorbisDecoder(ifcd); break;
#endif
#ifdef GPAC_HAS_THEORA
	case OGG_THEORA: DeleteTheoraDecoder(ifcd); break;
#endif
	default:
		break;
	}
	free(wrap);
	free(ifcd);
}


Bool QueryInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return 1;
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return 1;
	return 0;
}

GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return (GF_BaseInterface *)OGG_LoadDemux();
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *)OGG_LoadDecoder();
	return NULL;
}

void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_NET_CLIENT_INTERFACE:
		OGG_DeleteDemux(ifce);
		break;
	case GF_MEDIA_DECODER_INTERFACE:
		DeleteOGGDecoder((GF_BaseDecoder *) ifce);
		break;
	}
}
