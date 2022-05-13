/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / node and input sensor decoders modules interfaces
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



#ifndef _GF_MODULE_CODEC_H_
#define _GF_MODULE_CODEC_H_


#include <gpac/module.h>
#include <gpac/mpeg4_odf.h>
#include <gpac/color.h>

#ifdef __cplusplus
extern "C" {
#endif



/* Generic interface used by input sensor decoders (to be deprecated)
@AttachStream:
Add a Stream to the codec. If DependsOnESID is NULL, the stream is a base layer
UpStream means that the decoder should send feedback on this channel.
WARNING: Feedback format is not standardized by MPEG
the same API is used for both encoder and decoder (decSpecInfo is ignored
for an encoder)
@DetachStream:
Remove stream
@GetCapabilities:
Get the desired capability given its code
@SetCapabilities
Set the desired capability given its code if possible
if the codec does not support the request capability, return GF_NOT_SUPPORTED
@CanHandleStream
Can module handle this codec? Return one of GF_CODEC_NOT_SUPPORTED, GF_CODEC_MAYBE_SUPPORTED or GF_CODEC_SUPPORTED
esd is provided for more advanced inspection ( eg MPEG4 audio/visual where a bunch of codecs are defined with same objectType). If esd is NULL, only
decoder type is checked (audio or video), not codec type
@GetDecoderName
returns codec name - only called once the stream is successfully attached
@privateStack
user defined.
*/

#define GF_CODEC_BASE_INTERFACE(IFCE_NAME)		\
	GF_DECL_MODULE_INTERFACE	\
	GF_Err (*AttachStream)(IFCE_NAME, GF_ESD *esd);\
	GF_Err (*DetachStream)(IFCE_NAME, u16 ES_ID);\
	u32 (*CanHandleStream)(IFCE_NAME, u32 StreamType, GF_ESD *esd, u8 ProfileLevelIndication);\
	const char *(*GetName)(IFCE_NAME);\
	void *privateStack;	\


typedef struct _basedecoder
{
	GF_CODEC_BASE_INTERFACE(struct _basedecoder *)
} GF_BaseDecoder;


/*interface name and version for scene decoder */
#define GF_INPUT_DEVICE_INTERFACE		GF_4CC('G', 'I', 'D', '1')

typedef struct __input_device
{
	/* interface declaration*/
	GF_DECL_MODULE_INTERFACE

	Bool (*RegisterDevice)(struct __input_device *, const char *urn, const char *dsi, u32 dsi_size, void (*AddField)(struct __input_device *_this, u32 fieldType, const char *name));
	void (*Start)(struct __input_device *);
	void (*Stop)(struct __input_device *);

	void *udta;

	/*this is set upon loading and shall not be modified*/
	void *input_stream_context;
	void (*DispatchFrame)(struct __input_device *, const u8 *data, u32 data_len);
} GF_InputSensorDevice;


#ifdef __cplusplus
}
#endif

#endif	/*_GF_MODULE_CODEC_H_*/

