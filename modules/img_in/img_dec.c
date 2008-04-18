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
	if (StreamType != GF_STREAM_VISUAL) return 0;

	switch (ObjectType) {
	case 0x6D: return NewPNGDec(dec);
	case 0x6C: return NewJPEGDec(dec);
#ifdef GPAC_HAS_JP2
	case 0x6E: return NewJP2Dec(dec);
#endif
	case GPAC_BMP_OTI:
		return NewBMPDec(dec);
	case 0:
		return 1;/*query for types*/
 	default: 
#ifdef GPAC_HAS_JP2
		if ((decSpecInfoSize>=4) && (decSpecInfo[0]=='m') && (decSpecInfo[1]=='j') && (decSpecInfo[2]=='p') && (decSpecInfo[3]=='2')) 
			return NewJP2Dec(dec);
#endif
		return 0;
	}
	return 0;
}


GF_BaseDecoder *NewBaseDecoder()
{
	GF_MediaDecoder *ifce;
	IMGDec *wrap;
	GF_SAFEALLOC(ifce, GF_MediaDecoder);
	if (!ifce) return NULL;
	GF_SAFEALLOC(wrap, IMGDec);
	if (!wrap) {
		free(ifce);
		return NULL;
	}
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
	case DEC_PNG:
		DeletePNGDec(ifcd);
		break;
	case DEC_JPEG:
		DeleteJPEGDec(ifcd);
		break;
#ifdef GPAC_HAS_JP2
	case DEC_JP2:
		DeleteJP2Dec(ifcd);
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

GF_EXPORT
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

GF_EXPORT
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

GF_EXPORT
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
