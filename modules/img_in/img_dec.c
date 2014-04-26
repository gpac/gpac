/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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


static u32 DEC_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
{
	if (StreamType != GF_STREAM_VISUAL) return GF_CODEC_NOT_SUPPORTED;
	/*media type query*/
	if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;

	switch (esd->decoderConfig->objectTypeIndication) {
#ifdef GPAC_HAS_PNG
	case GPAC_OTI_IMAGE_PNG:
		if (NewPNGDec(dec)) return GF_CODEC_SUPPORTED;
		return GF_CODEC_NOT_SUPPORTED;
#endif
#ifdef GPAC_HAS_JPEG
	case GPAC_OTI_IMAGE_JPEG:
		if (NewJPEGDec(dec)) return GF_CODEC_SUPPORTED;
		return GF_CODEC_NOT_SUPPORTED;
#endif
#ifdef GPAC_HAS_JP2
	case GPAC_OTI_IMAGE_JPEG_2000:
		if (NewJP2Dec(dec)) return GF_CODEC_SUPPORTED;
		return GF_CODEC_NOT_SUPPORTED;
#endif
	case GPAC_BMP_OTI:
		if (NewBMPDec(dec)) return GF_CODEC_SUPPORTED;
		return GF_CODEC_NOT_SUPPORTED;

	default:
#ifdef GPAC_HAS_JP2
	{
		char *dsi = esd->decoderConfig->decoderSpecificInfo ? esd->decoderConfig->decoderSpecificInfo->data : NULL;
		if (dsi && (dsi[0]=='m') && (dsi[1]=='j') && (dsi[2]=='p') && (dsi[3]=='2'))
			if (NewJP2Dec(dec)) return GF_CODEC_SUPPORTED;
		return GF_CODEC_NOT_SUPPORTED;
	}
#endif
	return GF_CODEC_NOT_SUPPORTED;
	}
	return GF_CODEC_NOT_SUPPORTED;
}


GF_BaseDecoder *NewBaseDecoder()
{
	GF_MediaDecoder *ifce;
	IMGDec *wrap;
	GF_SAFEALLOC(ifce, GF_MediaDecoder);
	if (!ifce) return NULL;
	GF_SAFEALLOC(wrap, IMGDec);
	if (!wrap) {
		gf_free(ifce);
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
	IMGDec *wrap;
	if (!ifcd)
		return;
	wrap = (IMGDec *)ifcd->privateStack;
	if (!wrap)
		return;
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
	gf_free(wrap);
	ifcd->privateStack = NULL;
	gf_free(ifcd);
}

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_MEDIA_DECODER_INTERFACE,
		GF_NET_CLIENT_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
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

GPAC_MODULE_EXPORT
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

GPAC_MODULE_STATIC_DELARATION( img_in )
