/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Ivica Arsov, Jean Le Feuvre
 *			Copyright (c) Mines-Telecom 2009-
 *					All rights reserved
 *
 *  This file is part of GPAC / iOS camera module
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

#include <gpac/terminal.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/modules/codec.h>
#include <gpac/constants.h>
#include <gpac/modules/service.h>
#include <gpac/thread.h>
#include <gpac/media_tools.h>

#include "cam_wrap.h"


#define CAM_PIXEL_FORMAT GF_PIXEL_BGR_32
//GF_PIXEL_RGB_32
#define CAM_PIXEL_SIZE 4.f
//4
#define CAM_WIDTH 640
#define CAM_HEIGHT 480

//GF_PIXEL_RGB_24;

typedef struct
{
	GF_InputService *input;
	/*the service we're responsible for*/
	GF_ClientService *service;
	LPNETCHANNEL channel;

	/*input file*/
	u32 time_scale;

	u32 base_track_id;

	struct _tag_terminal *term;

	u32 cntr;

	u32 width;
	u32 height;

	Bool started;

	void* camInst;

} IOSCamCtx;

IOSCamCtx* globReader = NULL;

void camStartCamera(IOSCamCtx *read);
void camStopCamera(IOSCamCtx *read);
void processFrameBuf( unsigned char* data, unsigned int dataSize);

Bool CAM_CanHandleURL(GF_InputService *plug, const char *url)
{
	if (!strnicmp(url, "hw://camera", 11)) return 1;

	return 0;
}

GF_Err CAM_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	IOSCamCtx *read;
	if (!plug || !plug->priv || !serv) return GF_SERVICE_ERROR;
	read = (IOSCamCtx *) plug->priv;

	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[ANDROID_CAMERA] CAM_ConnectService: %d\n", gf_th_id()));

	globReader = read;

	read->input = plug;
	read->service = serv;
	read->base_track_id = 1;
	read->time_scale = 1000;

	read->term = serv->term;

	read->camInst = CAM_CreateInstance();
	CAM_SetCallback(read->camInst, processFrameBuf);

	/*reply to user*/
	gf_service_connect_ack(serv, NULL, GF_OK);
	//if (read->no_service_desc) isor_declare_objects(read);

	return GF_OK;
}

GF_Err CAM_CloseService(GF_InputService *plug)
{
	GF_Err reply;
	IOSCamCtx *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (IOSCamCtx *) plug->priv;

	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[ANDROID_CAMERA] CAM_CloseService: %d\n", gf_th_id()));

	reply = GF_OK;

	CAM_DestroyInstance(&read->camInst);

	gf_service_disconnect_ack(read->service, NULL, reply);
	return GF_OK;
}

u32 getWidth(IOSCamCtx *read);
u32 getHeight(IOSCamCtx *read);

static GF_Descriptor *CAM_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	u32 trackID;
	IOSCamCtx *read;
	char *buf;
	u32 buf_size;
	s32 color;
	s32 stride;
	if (!plug || !plug->priv) return NULL;
	read = (IOSCamCtx *) plug->priv;

	trackID = read->base_track_id;
	read->base_track_id = 0;

	if (trackID && (expect_type==GF_MEDIA_OBJECT_VIDEO) ) {
		GF_ESD *esd;
		GF_ObjectDescriptor *od;
		GF_BitStream *bs;
		od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		od->objectDescriptorID = 1;

		esd = gf_odf_desc_esd_new(0);
		esd->slConfig->timestampResolution = 1000;
		esd->decoderConfig->streamType = GF_STREAM_VISUAL;
		esd->ESID = 1;
		esd->decoderConfig->objectTypeIndication = GPAC_OTI_RAW_MEDIA_STREAM;

		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

		CAM_GetCurrentFormat(read->camInst, &read->width, &read->height, &color, &stride);

		gf_bs_write_u32(bs, CAM_PIXEL_FORMAT); // fourcc
		gf_bs_write_u16(bs, read->width); // width
		gf_bs_write_u16(bs, read->height); // height
		gf_bs_write_u32(bs, read->height * stride); // framesize
		gf_bs_write_u32(bs, stride); // stride
		gf_bs_write_u8(bs, 1); // is_flipped

		gf_bs_align(bs);
		gf_bs_get_content(bs, &buf, &buf_size);
		gf_bs_del(bs);

		esd->decoderConfig->decoderSpecificInfo->data = buf;
		esd->decoderConfig->decoderSpecificInfo->dataLength = buf_size;

		gf_list_add(od->ESDescriptors, esd);
		return (GF_Descriptor *) od;
	}

	return NULL;
}

GF_Err CAM_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	GF_Err e;
	IOSCamCtx *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (IOSCamCtx *) plug->priv;

	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[ANDROID_CAMERA] CAM_ConnectChannel: %d\n", gf_th_id()));

	e = GF_OK;
	if (upstream) {
		e = GF_ISOM_INVALID_FILE;
	}

	read->channel = channel;

	camStartCamera(read);

	gf_service_connect_ack(read->service, channel, e);
	return e;
}

GF_Err CAM_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	GF_Err e;
	IOSCamCtx *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (IOSCamCtx *) plug->priv;

	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[ANDROID_CAMERA] CAM_DisconnectChannel: %d\n", gf_th_id()));

	e = GF_OK;

	camStopCamera(read);

	gf_service_disconnect_ack(read->service, channel, e);
	return e;
}

int* decodeYUV420SP( char* yuv420sp, int width, int height)
{
	int frameSize = width * height;
	int j, yp, i, y, y1192, r, g, b;
	int ti, tj;

	int* rgb = (int*)gf_malloc(width*height*4);
	for (j = 0, yp = 0, tj=height-1; j < height; j++, tj--)
	{
		int uvp = frameSize + (j >> 1) * width, u = 0, v = 0;
		for (i = 0, ti=0; i < width; i++, yp++, ti+=width)
		{
			y = (0xff & ((int) yuv420sp[yp])) - 16;
			if (y < 0) y = 0;
			if ((i & 1) == 0)
			{
				v = (0xff & yuv420sp[uvp++]) - 128;
				u = (0xff & yuv420sp[uvp++]) - 128;
			}

			y1192 = 1192 * y;
			r = (y1192 + 1634 * v);
			g = (y1192 - 833 * v - 400 * u);
			b = (y1192 + 2066 * u);

			if (r < 0)
				r = 0;
			else if (r > 262143)
				r = 262143;
			if (g < 0)
				g = 0;
			else if (g > 262143)
				g = 262143;
			if (b < 0)
				b = 0;
			else if (b > 262143)
				b = 262143;

			rgb[yp] = 0xff000000 | ((r << 6) & 0xff0000)
			          | ((g >> 2) & 0xff00) | ((b >> 10) & 0xff);
			//			rgb[ti+tj] = 0xff000000 | ((r << 6) & 0xff0000)
			//					| ((g >> 2) & 0xff00) | ((b >> 10) & 0xff);
		}
	}
	return rgb;
}

void processFrameBuf( unsigned char* data, unsigned int dataSize)
{
	IOSCamCtx* ctx = globReader;
	GF_SLHeader hdr;
	u32 cts = 0;

	cts = gf_term_get_time(ctx->term);

	memset(&hdr, 0, sizeof(hdr));
	hdr.compositionTimeStampFlag = 1;
	hdr.compositionTimeStamp = cts;
	gf_service_send_packet(ctx->service, ctx->channel, (void*)data, dataSize, &hdr, GF_OK);

}

void camStartCamera(IOSCamCtx *read)
{
	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[ANDROID_CAMERA] startCamera: %d\n", gf_th_id()));

	//CAM_Start(read->camInst);
}

void camStopCamera(IOSCamCtx *read)
{
	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[ANDROID_CAMERA] stopCamera: %d\n", gf_th_id()));

	//CAM_Stop(read->camInst);
}

void pauseCamera(IOSCamCtx *read)
{
	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[ANDROID_CAMERA] pauseCamera: %d\n", gf_th_id()));

	read->started = 0;
	CAM_Stop(read->camInst);
}

void resumeCamera(IOSCamCtx *read)
{
	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[ANDROID_CAMERA] resumeCamera: %d\n", gf_th_id()));

	read->started = 1;
	CAM_Start(read->camInst);
}

GF_Err CAM_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	IOSCamCtx *read;
	char *buf;
	u32 buf_size;
	if (!plug || !plug->priv || !com) return GF_SERVICE_ERROR;
	read = (IOSCamCtx *) plug->priv;

	if (com->command_type==GF_NET_SERVICE_INFO) {
		return GF_OK;
	}
	if (com->command_type==GF_NET_SERVICE_HAS_AUDIO) {
		return GF_NOT_SUPPORTED;
	}
	if (!com->base.on_channel) return GF_NOT_SUPPORTED;

	switch (com->command_type) {
	case GF_NET_CHAN_INTERACTIVE:
		return GF_OK;
	case GF_NET_CHAN_BUFFER:
		com->buffer.max = com->buffer.min = 0;
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		resumeCamera(read);
		return GF_OK;
	case GF_NET_CHAN_STOP:
		pauseCamera(read);
		return GF_OK;
	/*nothing to do on MP4 for channel config*/
	case GF_NET_CHAN_CONFIG:
		return GF_OK;
	case GF_NET_CHAN_GET_PIXEL_AR:
		return 1<<16;//gf_isom_get_pixel_aspect_ratio(read->mov, ch->track, 1, &com->par.hSpacing, &com->par.vSpacing);
	case GF_NET_CHAN_GET_DSI:
	{
		GF_BitStream *bs;
		s32 color;
		s32 stride;
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cam get DSI\n"));
		/*it may happen that there are conflicting config when using ESD URLs...*/
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

		CAM_GetCurrentFormat(read->camInst, &read->width, &read->height, &color, &stride);

		gf_bs_write_u32(bs, CAM_PIXEL_FORMAT); // fourcc
		gf_bs_write_u16(bs, read->width); // width
		gf_bs_write_u16(bs, read->height); // height
		gf_bs_write_u32(bs, read->height * stride); // framesize
		gf_bs_write_u32(bs, stride); // stride

		gf_bs_align(bs);
		gf_bs_get_content(bs, &buf, &buf_size);
		gf_bs_del(bs);

		com->get_dsi.dsi = buf;
		com->get_dsi.dsi_len = buf_size;
		return GF_OK;
	}
	default:
		return GF_NOT_SUPPORTED;
	}
	return GF_NOT_SUPPORTED;
}

GF_InputService *CAM_client_load()
{
	IOSCamCtx *reader;
	GF_InputService *plug;
	GF_SAFEALLOC(plug, GF_InputService);
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC Camera Plugin", "gpac distribution")
	plug->CanHandleURL = CAM_CanHandleURL;
	plug->ConnectService = CAM_ConnectService;
	plug->CloseService = CAM_CloseService;
	plug->GetServiceDescriptor = CAM_GetServiceDesc;
	plug->ConnectChannel = CAM_ConnectChannel;
	plug->DisconnectChannel = CAM_DisconnectChannel;
	plug->ServiceCommand = CAM_ServiceCommand;

	GF_SAFEALLOC(reader, IOSCamCtx);
	plug->priv = reader;
	return plug;
}

void CAM_client_del(GF_BaseInterface *bi)
{
	GF_InputService *plug = (GF_InputService *) bi;
	IOSCamCtx *read = (IOSCamCtx *)plug->priv;

	gf_free(read);
	gf_free(bi);
}

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_NET_CLIENT_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_NET_CLIENT_INTERFACE)
		return (GF_BaseInterface *)CAM_client_load();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_NET_CLIENT_INTERFACE:
		CAM_client_del(ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( ios_cam )
