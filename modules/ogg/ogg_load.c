/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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


#if !defined(__GNUC__)
# if (defined(_WIN32_WCE) || defined (WIN32))
#  pragma comment(lib, "libogg_static")
#  pragma comment(lib, "libvorbis_static")
#  pragma comment(lib, "libtheora_static")
# endif
#endif


static u32 OGG_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
{
	/*video decs*/
	if (StreamType == GF_STREAM_VISUAL) {
		char *dsi;
		/*media type query*/
		if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;
		dsi = esd->decoderConfig->decoderSpecificInfo ? esd->decoderConfig->decoderSpecificInfo->data : NULL;

		switch (esd->decoderConfig->objectTypeIndication) {
#ifdef GPAC_HAS_THEORA
		case GPAC_OTI_MEDIA_OGG:
			if (dsi && (esd->decoderConfig->decoderSpecificInfo->dataLength>=9)  && !strncmp((char *) &dsi[3], "theora", 6)) {
				if (NewTheoraDecoder(dec)) return GF_CODEC_SUPPORTED;
			}
			return GF_CODEC_NOT_SUPPORTED;
#endif
		default:
			return GF_CODEC_NOT_SUPPORTED;
		}
	}
	/*audio decs*/
	if (StreamType == GF_STREAM_AUDIO) {
		char *dsi;
		/*media type query*/
		if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;
		dsi = esd->decoderConfig->decoderSpecificInfo ? esd->decoderConfig->decoderSpecificInfo->data : NULL;

		switch (esd->decoderConfig->objectTypeIndication) {
#ifdef GPAC_HAS_VORBIS
		case GPAC_OTI_MEDIA_OGG:
			if (dsi && (esd->decoderConfig->decoderSpecificInfo->dataLength>=9)  && !strncmp((char *) &dsi[3], "vorbis", 6)) {
				if (NewVorbisDecoder(dec)) return GF_CODEC_SUPPORTED;
			}
			return GF_CODEC_NOT_SUPPORTED;
#endif
		default:
			return GF_CODEC_NOT_SUPPORTED;
		}
	}
	return GF_CODEC_NOT_SUPPORTED;
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
	OGGWraper *wrap;
	if (!ifcd)
		return;
	wrap = (OGGWraper *)ifcd->privateStack;
	if (wrap) {
		switch (wrap->type) {
#ifdef GPAC_HAS_VORBIS
		case OGG_VORBIS:
			DeleteVorbisDecoder(ifcd);
			break;
#endif
#ifdef GPAC_HAS_THEORA
		case OGG_THEORA:
			DeleteTheoraDecoder(ifcd);
			break;
#endif
		default:
			break;
		}
		gf_free(wrap);
		ifcd->privateStack = NULL;
	}
	gf_free(ifcd);
}


GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_OGG)
		GF_NET_CLIENT_INTERFACE,
#endif
		GF_MEDIA_DECODER_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_OGG)
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return (GF_BaseInterface *)OGG_LoadDemux();
#endif
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *)OGG_LoadDecoder();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_OGG)
	case GF_NET_CLIENT_INTERFACE:
		OGG_DeleteDemux(ifce);
		break;
#endif
	case GF_MEDIA_DECODER_INTERFACE:
		DeleteOGGDecoder((GF_BaseDecoder *) ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DELARATION( ogg_in )
