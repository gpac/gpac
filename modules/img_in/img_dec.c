/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / image format module
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


#include "img_in.h"


static Bool DEC_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, u32 ObjectType, char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	IMGDec *wrap = (IMGDec *)dec->privateStack;

	if (StreamType != GF_STREAM_VISUAL) return 0;

	/*should never happen*/
	assert(wrap->type == DEC_RESERVED);

	switch (ObjectType) {
#ifdef GPAC_HAS_PNG
	case 0x6D: return NewPNGDec(dec);
#endif
#ifdef GPAC_HAS_JPEG
	case 0x6C: return NewJPEGDec(dec);
#endif
	case GPAC_BMP_OTI: 
	  return NewBMPDec(dec);
	case 0: 
	  return 1;/*query for types*/
 	default: return 0;
	}
	return 0;
}


GF_BaseDecoder *NewBaseDecoder()
{
	GF_MediaDecoder *ifce;
	IMGDec *wrap;
	GF_SAFEALLOC(ifce, GF_MediaDecoder);
	wrap = malloc(sizeof(IMGDec));
	memset(wrap, 0, sizeof(IMGDec));
	ifce->privateStack = wrap;
	ifce->CanHandleStream = DEC_CanHandleStream;

	GF_REGISTER_MODULE_INTERFACE(ifce, GF_MEDIA_DECODER_INTERFACE, "GPAC Image Decoder", "gpac distribution")

	/*other interfaces will be setup at run time*/
	return (GF_BaseDecoder *)ifce;
}

void DeleteBaseDecoder(GF_BaseDecoder *ifcd)
{
	IMGDec *wrap = (IMGDec *)ifcd->privateStack;
	switch (wrap->type) {
#ifdef GPAC_HAS_PNG
	case DEC_PNG:
		DeletePNGDec(ifcd);
		break;
#endif

#ifdef GPAC_HAS_JPEG
	case DEC_JPEG:
		DeleteJPEGDec(ifcd);
		break;
#endif
	case DEC_BMP:
		DeleteBMPDec(ifcd);
		break;
	default:
		break;
	}
	free(wrap);
	free(ifcd);
}

Bool QueryInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_MEDIA_DECODER_INTERFACE:
		return 1;
	case GF_NET_CLIENT_INTERFACE:
		return 1;
	default:
		return 0;
	}
}

GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_MEDIA_DECODER_INTERFACE:
		return (GF_BaseInterface *)NewBaseDecoder();
	case GF_NET_CLIENT_INTERFACE:
		return (GF_BaseInterface *)NewLoaderInterface();
	default:
		return NULL;
	}
}

void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_MEDIA_DECODER_INTERFACE:
		DeleteBaseDecoder((GF_BaseDecoder *)ifce);
		break;
	case GF_NET_CLIENT_INTERFACE:
		DeleteLoaderInterface(ifce);
		break;
	}
}
