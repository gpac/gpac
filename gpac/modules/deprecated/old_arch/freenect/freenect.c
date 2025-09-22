/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2011-20XX
 *					All rights reserved
 *
 *  This file is part of GPAC / Freenect video input module
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


#include <gpac/modules/service.h>
#include <gpac/modules/codec.h>
#include <gpac/constants.h>
#include <gpac/download.h>

#ifdef FREENECT_FLAT_HEADERS
#include <libfreenect.h>
#else
#include <libfreenect/libfreenect.h>
#endif

#include <gpac/thread.h>


#if !defined(FREENECT_DEVICE_CAMERA) && defined(FREENECT_FRAME_W)
#define FREENECT_MINIMAL
#endif


typedef struct
{
	/*the service we're responsible for*/
	GF_ClientService *service;

	freenect_context *f_ctx;
	freenect_device *f_dev;

	u32 width, height, fps, out_depth_size, out_color_size, color_stride, depth_stride, color_pixel_format, depth_pixel_format;
	u32 depth_format;

	u8 *vid_buf;
	u8 *depth_buf;
	u16 gamma[2048];

	GF_SLHeader depth_sl_header, color_sl_header;
	LPNETCHANNEL depth_channel, color_channel;

	GF_Thread *th;
	u32 nb_running;
	Bool done;

} FreenectIn;


void Freenect_DepthCallback_RGBD(freenect_device *dev, void *v_depth, uint32_t timestamp)
{
	FreenectIn *vcap = freenect_get_user(dev);
	if (vcap->depth_channel) {
		u32 i, j;
		u16 *depth = (u16*)v_depth;

		for (i=0; i<vcap->height; i++) {
			for (j=0; j<vcap->width; j++) {
				int idx_col = 3 * (j + i*vcap->width) ;
				int idx_depth = 4*(j + i*vcap->width) ;
				int pval = depth[i*vcap->width + j];
				pval = 255 - (255*pval) / 2048;

				vcap->depth_buf[idx_depth ] = vcap->vid_buf[idx_col];
				vcap->depth_buf[idx_depth + 1] = vcap->vid_buf[idx_col+1];
				vcap->depth_buf[idx_depth + 2] = vcap->vid_buf[idx_col+2];
				vcap->depth_buf[idx_depth + 3] = pval;
			}
		}
		vcap->depth_sl_header.compositionTimeStamp = timestamp;
		gf_service_send_packet(vcap->service, vcap->depth_channel, (char *) vcap->depth_buf, vcap->out_depth_size, &vcap->depth_sl_header, GF_OK);
	}
}


void Freenect_DepthCallback_GREY16(freenect_device *dev, void *v_depth, uint32_t timestamp)
{
	FreenectIn *vcap = freenect_get_user(dev);
	if (vcap->depth_channel) {
		memcpy(vcap->depth_buf, v_depth, vcap->out_depth_size);

		vcap->depth_sl_header.compositionTimeStamp = timestamp;
		gf_service_send_packet(vcap->service, vcap->depth_channel, (char *) vcap->depth_buf, vcap->out_depth_size, &vcap->depth_sl_header, GF_OK);
	}
}

void Freenect_DepthCallback_GREY8(freenect_device *dev, void *v_depth, uint32_t timestamp)
{
	FreenectIn *vcap = freenect_get_user(dev);
	if (vcap->depth_channel) {
		u32 i, j;
		u16 *depth = (u16*)v_depth;

		for (i=0; i<vcap->height; i++) {
			for (j=0; j<vcap->width; j++) {
				int pval = depth[j + i*vcap->width];
				pval = (255*pval) / 2048;
				vcap->depth_buf[j + i*vcap->width] = pval;
			}
		}
//		vcap->depth_sl_header.compositionTimeStamp = timestamp;
		vcap->depth_sl_header.compositionTimeStamp ++;
		gf_service_send_packet(vcap->service, vcap->depth_channel, (char *) vcap->depth_buf, vcap->out_depth_size, &vcap->depth_sl_header, GF_OK);
	}
}

void Freenect_DepthCallback_ColorGradient(freenect_device *dev, void *v_depth, uint32_t timestamp)
{
	FreenectIn *vcap = freenect_get_user(dev);
	if (vcap->depth_channel) {
		u32 i;
		u16 *depth = (u16*)v_depth;
		/*remap to color RGB using freenect gamma*/
		for (i=0; i<vcap->width*vcap->height; i++) {
			int pval = vcap->gamma[depth[i]];
			int lb = pval & 0xff;
			switch (pval>>8) {
			case 0:
				vcap->depth_buf[3*i+0] = 255;
				vcap->depth_buf[3*i+1] = 255-lb;
				vcap->depth_buf[3*i+2] = 255-lb;
				break;
			case 1:
				vcap->depth_buf[3*i+0] = 255;
				vcap->depth_buf[3*i+1] = lb;
				vcap->depth_buf[3*i+2] = 0;
				break;
			case 2:
				vcap->depth_buf[3*i+0] = 255-lb;
				vcap->depth_buf[3*i+1] = 255;
				vcap->depth_buf[3*i+2] = 0;
				break;
			case 3:
				vcap->depth_buf[3*i+0] = 0;
				vcap->depth_buf[3*i+1] = 255;
				vcap->depth_buf[3*i+2] = lb;
				break;
			case 4:
				vcap->depth_buf[3*i+0] = 0;
				vcap->depth_buf[3*i+1] = 255-lb;
				vcap->depth_buf[3*i+2] = 255;
				break;
			case 5:
				vcap->depth_buf[3*i+0] = 0;
				vcap->depth_buf[3*i+1] = 0;
				vcap->depth_buf[3*i+2] = 255-lb;
				break;
			default:
				vcap->depth_buf[3*i+0] = 0;
				vcap->depth_buf[3*i+1] = 0;
				vcap->depth_buf[3*i+2] = 0;
				break;
			}
		}
		vcap->depth_sl_header.compositionTimeStamp = timestamp;
		gf_service_send_packet(vcap->service, vcap->depth_channel, (char *) vcap->depth_buf, vcap->out_depth_size, &vcap->depth_sl_header, GF_OK);
	}
}

void Freenect_RGBCallback(freenect_device *dev, void *rgb, uint32_t timestamp)
{
	FreenectIn *vcap = freenect_get_user(dev);
	if (vcap->color_channel) {
		vcap->color_sl_header.compositionTimeStamp = timestamp;
		gf_service_send_packet(vcap->service, vcap->color_channel, (char *) rgb, vcap->out_color_size, &vcap->color_sl_header, GF_OK);
	}
}


u32 FreenectRun(void *par)
{
	FreenectIn *vcap = par;

	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[Freenect] Starting device thread\n"));
	freenect_start_depth(vcap->f_dev);
	freenect_start_video(vcap->f_dev);
	vcap->done = 0;
	while (vcap->nb_running && (freenect_process_events(vcap->f_ctx)>=0) ) {
		gf_sleep(0);
	}
	freenect_stop_depth(vcap->f_dev);
	freenect_stop_video(vcap->f_dev);
	vcap->done = 1;
	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[Freenect] stopping device thread\n"));
	return 0;
}

Bool Freenect_CanHandleURL(GF_InputService *plug, const char *url)
{
	if (!strnicmp(url, "camera://", 9)) return 1;
	if (!strnicmp(url, "video://", 8)) return 1;
	if (!strnicmp(url, "kinect://", 8)) return 1;
	return 0;
}

void Freenect_Logs(freenect_context *dev, freenect_loglevel level, const char *msg)
{
	switch (level) {
	case FREENECT_LOG_ERROR:
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Freenect] %s", msg));
		break;
	case FREENECT_LOG_WARNING:
		GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[Freenect] %s", msg));
		break;
	case FREENECT_LOG_NOTICE:
	case FREENECT_LOG_INFO:
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[Freenect] %s", msg));
		break;
	case FREENECT_LOG_DEBUG:
	case FREENECT_LOG_SPEW:
	case FREENECT_LOG_FLOOD:
	default:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Freenect] %s", msg));
		break;
	}
}


GF_Err Freenect_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	GF_ESD *esd;
	GF_BitStream *bs;
	GF_ObjectDescriptor *od;
	FreenectIn *vcap = (FreenectIn *) plug->priv;

	if (!vcap || !serv || !url) return GF_BAD_PARAM;

	vcap->service = serv;

	if (!vcap->f_ctx) {
		int i;
#ifndef FREENECT_MINIMAL
		freenect_frame_mode frame_mode;
		freenect_resolution frame_format = FREENECT_RESOLUTION_MEDIUM;
#endif
		char *name, *params;
		int res = freenect_init(&vcap->f_ctx, NULL);
		if (res < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("freenect_init() failed - ret code %d\n", res));
			return GF_IO_ERR;
		}
		freenect_set_log_level(vcap->f_ctx, FREENECT_LOG_DEBUG);
		freenect_set_log_callback(vcap->f_ctx, Freenect_Logs);
#ifndef FREENECT_MINIMAL
		freenect_select_subdevices(vcap->f_ctx, FREENECT_DEVICE_CAMERA);
#endif
		res = freenect_num_devices (vcap->f_ctx);
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[Freenect] %d devices found\n", res));
		if (res<1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Freenect] No device found\n"));
			return GF_URL_ERROR;
		}

		res = freenect_open_device(vcap->f_ctx, &vcap->f_dev, 0);
		if (res < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Freenect] Could not open Kinect - error %d\n", res));
			return GF_SERVICE_ERROR;
		}


		params = (char *) strchr(url, '?');
		if (params) params[0] = 0;
		name = (char *) strstr(url, "://");
		if (name) name += 3;

		if (!stricmp(name, "color")) {
		}

		if (params) {
			params[0] = '?';
			params ++;
		}
		while (params) {
			char *sep = (char *) strchr(params, '&');
			if (sep) sep[0] = 0;

			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[VideoCapture] Set camera option %s\n", params));

			if (!strnicmp(params, "resolution=", 11)) {
#ifndef FREENECT_MINIMAL
				u32 w, h;
				if (sscanf(params+11, "%dx%d", &w, &h)==2) {
					if ((w<=320) || (h<=240)) frame_format = FREENECT_RESOLUTION_LOW;
					else if ((w<=640) || (h<=480)) frame_format = FREENECT_RESOLUTION_MEDIUM;
					else frame_format = FREENECT_RESOLUTION_HIGH;
				}
#endif
			}
			else if (!strnicmp(params, "format=", 7)) {
				if (!stricmp(params+7, "standard")) vcap->depth_format = 0;
				else if (!stricmp(params+7, "grey")) vcap->depth_format = 1;
				else if (!stricmp(params+7, "rgbd")) vcap->depth_format = 2;
				else if (!stricmp(params+7, "grey16")) vcap->depth_format = 3;
				else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[VideoCapture] Unrecognized value %s for parameter \"format\"\n", params+7));
				}
			}
			else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[VideoCapture] Unrecognized parameter %s\n", params));
			}

			if (!sep) break;
			sep[0] = '&';
			params = sep+1;
		}

#ifndef FREENECT_MINIMAL
		frame_mode = freenect_find_video_mode(frame_format, FREENECT_VIDEO_RGB);
		res = freenect_set_video_mode(vcap->f_dev, frame_mode);
		res = freenect_set_depth_mode(vcap->f_dev, freenect_find_depth_mode(frame_format, FREENECT_DEPTH_11BIT));
		vcap->width = frame_mode.width;
		vcap->height = frame_mode.height;
		vcap->fps = frame_mode.framerate;
#else
		freenect_set_video_format(vcap->f_dev, FREENECT_VIDEO_RGB);
		res = freenect_set_depth_format(vcap->f_dev, FREENECT_DEPTH_11BIT);
		vcap->width = FREENECT_FRAME_W;
		vcap->height = FREENECT_FRAME_H;
		vcap->fps = 30;
#endif
		/*currently hardcoded*/
		vcap->color_pixel_format = GF_PIXEL_RGB_24;
		vcap->color_stride = 3*vcap->width;
		vcap->out_color_size = vcap->color_stride * vcap->height;
		vcap->vid_buf = gf_malloc(sizeof(char) * vcap->out_color_size);
		freenect_set_video_callback(vcap->f_dev, Freenect_RGBCallback);

		switch (vcap->depth_format) {
		case 1:
			vcap->depth_pixel_format = GF_PIXEL_GREYSCALE;
			vcap->depth_stride = vcap->width;
			freenect_set_depth_callback(vcap->f_dev, Freenect_DepthCallback_GREY8);
			break;
		case 2:
			vcap->depth_pixel_format = GF_PIXEL_RGBD;
			vcap->depth_stride = 4*vcap->width;
			freenect_set_depth_callback(vcap->f_dev, Freenect_DepthCallback_RGBD);
			break;
		case 3:
			vcap->depth_pixel_format = GF_PIXEL_RGB_565;
			vcap->depth_stride = 2*vcap->width;
			freenect_set_depth_callback(vcap->f_dev, Freenect_DepthCallback_GREY16);
			break;
		default:
			vcap->depth_pixel_format = GF_PIXEL_RGB_24;
			vcap->depth_stride = 3*vcap->width;
			freenect_set_depth_callback(vcap->f_dev, Freenect_DepthCallback_ColorGradient);
		}
		vcap->out_depth_size = vcap->depth_stride * vcap->height;
		vcap->depth_buf = gf_malloc(sizeof(char) * vcap->out_depth_size);


		res = freenect_set_video_buffer(vcap->f_dev, vcap->vid_buf);

		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[Freenect] Device configured - resolution %dx%d - Frame Rate %d - Depth Pixel Format %s\n", vcap->width, vcap->height, vcap->fps, gf_4cc_to_str(vcap->depth_pixel_format) ));


		for (i=0; i<2048; i++) {
			float v = i/2048.0f;
			v = powf(v, 3)* 6;
			vcap->gamma[i] = (u16) (v*6*256);
		}

		freenect_set_user(vcap->f_dev, vcap);

		vcap->th = gf_th_new("Freenect");
	}

	/*ACK connection is OK*/
	gf_service_connect_ack(serv, NULL, GF_OK);


	/*setup object descriptor*/
	od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);

	esd = gf_odf_desc_esd_new(0);
	esd->slConfig->timestampResolution = 1000;
	if (!strnicmp(url, "camera://", 9) || !strnicmp(url, "video://", 8) || !strnicmp(url, "kinect://", 8)) {
		if (strstr(url, "color") || strstr(url, "COLOR")) {
			od->objectDescriptorID = 2;
			esd->ESID = 2;
			esd->decoderConfig->streamType = GF_STREAM_VISUAL;
		} else {
			od->objectDescriptorID = 1;
			esd->ESID = 1;
			esd->decoderConfig->streamType = GF_STREAM_VISUAL;
		}
	} else {
		od->objectDescriptorID = 3;
		esd->ESID = 3;
		esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	}
	esd->decoderConfig->objectTypeIndication = GPAC_OTI_RAW_MEDIA_STREAM;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_u32(bs, (esd->ESID==2) ? vcap->color_pixel_format : vcap->depth_pixel_format);
	gf_bs_write_u16(bs, vcap->width);
	gf_bs_write_u16(bs, vcap->height);
	gf_bs_write_u32(bs, (esd->ESID==2) ? vcap->out_color_size : vcap->out_depth_size);
	gf_bs_write_u32(bs, (esd->ESID==2) ? vcap->color_stride : vcap->depth_stride);
	gf_bs_get_content(bs, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
	gf_bs_del(bs);

	gf_list_add(od->ESDescriptors, esd);
	gf_service_declare_media(vcap->service, (GF_Descriptor*)od, 0);

	return GF_OK;
}

GF_Err Freenect_CloseService(GF_InputService *plug)
{
	FreenectIn *vcap = (FreenectIn *) plug->priv;
	if (vcap->f_dev) freenect_close_device(vcap->f_dev);
	if (vcap->f_ctx) freenect_shutdown(vcap->f_ctx);
	vcap->f_ctx = NULL;
	vcap->f_dev = NULL;
	gf_service_disconnect_ack(vcap->service, NULL, GF_OK);
	return GF_OK;
}

/*Dummy input just send a file name, no multitrack to handle so we don't need to check sub_url nor expected type*/
static GF_Descriptor *Freenect_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	return NULL;
}


GF_Err Freenect_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	FreenectIn *vcap = (FreenectIn *) plug->priv;

	if (!com->base.on_channel) return GF_NOT_SUPPORTED;

	switch (com->command_type) {
	case GF_NET_CHAN_SET_PULL:
		return GF_NOT_SUPPORTED;
	case GF_NET_CHAN_INTERACTIVE:
		return GF_OK;
	/*since data is file-based, no padding is needed (decoder plugin will handle it itself)*/
	case GF_NET_CHAN_SET_PADDING:
		return GF_OK;
	case GF_NET_CHAN_BUFFER:
		com->buffer.max = com->buffer.min = 500;
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		/*this module is not made for updates, use undefined duration*/
		com->duration.duration = 0;
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		if (!vcap->nb_running) {
			vcap->nb_running++;
			gf_th_run(vcap->th, FreenectRun, vcap);
		}
		return GF_OK;
	case GF_NET_CHAN_STOP:
		if (vcap->nb_running) {
			vcap->nb_running--;
			if (!vcap->nb_running) {
				while (! vcap->done) {
					gf_sleep(10);
				}
			}
		}
		return GF_OK;
	case GF_NET_CHAN_CONFIG:
		return GF_OK;
	case GF_NET_CHAN_GET_DSI:
		com->get_dsi.dsi = NULL;
		com->get_dsi.dsi_len = 0;
		return GF_OK;
	default:
		break;
	}
	return GF_OK;
}

GF_Err Freenect_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ESID;
	FreenectIn *vcap = (FreenectIn *) plug->priv;

	sscanf(url, "ES_ID=%u", &ESID);
	if (ESID == 1) {
		vcap->depth_channel = channel;
		memset(&vcap->depth_sl_header, 0, sizeof(GF_SLHeader));
		vcap->depth_sl_header.compositionTimeStampFlag = 1;
		gf_service_connect_ack(vcap->service, channel, GF_OK);
	} else if (ESID == 2) {
		vcap->color_channel = channel;
		memset(&vcap->color_sl_header, 0, sizeof(GF_SLHeader));
		vcap->color_sl_header.compositionTimeStampFlag = 1;
		gf_service_connect_ack(vcap->service, channel, GF_OK);
	} else {
		/*TODO*/
		gf_service_connect_ack(vcap->service, channel, GF_STREAM_NOT_FOUND);
	}
	return GF_OK;
}

GF_Err Freenect_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	FreenectIn *vcap = (FreenectIn *) plug->priv;
	if (vcap->depth_channel == channel) {
		vcap->depth_channel = NULL;
	}
	else if (vcap->color_channel == channel) {
		vcap->color_channel = NULL;
	}
	gf_service_disconnect_ack(vcap->service, channel, GF_OK);
	return GF_OK;
}

Bool Freenect_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	return 0;
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
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) {
		FreenectIn *vcap;
		GF_InputService *plug;
		GF_SAFEALLOC(plug, GF_InputService);
		memset(plug, 0, sizeof(GF_InputService));
		GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "Video Capture using libfreenect", "gpac distribution")

		plug->RegisterMimeTypes = NULL;
		plug->CanHandleURL = Freenect_CanHandleURL;
		plug->ConnectService = Freenect_ConnectService;
		plug->CloseService = Freenect_CloseService;
		plug->GetServiceDescriptor = Freenect_GetServiceDesc;
		plug->ConnectChannel = Freenect_ConnectChannel;
		plug->DisconnectChannel = Freenect_DisconnectChannel;
		plug->ServiceCommand = Freenect_ServiceCommand;
		plug->CanHandleURLInService = Freenect_CanHandleURLInService;

		GF_SAFEALLOC(vcap, FreenectIn);
		plug->priv = vcap;
		return (GF_BaseInterface *)plug;
	}
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *bi)
{
	if (bi->InterfaceType==GF_NET_CLIENT_INTERFACE) {
		GF_InputService *ifcn = (GF_InputService*)bi;
		FreenectIn *vcap = (FreenectIn*)ifcn->priv;

		if (vcap->vid_buf) gf_free(vcap->vid_buf);
		if (vcap->depth_buf) gf_free(vcap->depth_buf);
		if (vcap->th) gf_th_del(vcap->th);
		gf_free(vcap);
		gf_free(bi);
	}
}

GPAC_MODULE_STATIC_DECLARATION( freenect )

