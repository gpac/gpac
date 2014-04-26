/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Dummy input module
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
/*for GF_STREAM_PRIVATE_SCENE definition*/
#include <gpac/constants.h>
#include <gpac/download.h>

//#define TEST_LIBPLAYER

#ifndef TEST_LIBPLAYER

#ifdef WIN32
typedef u32 uint32_t;
typedef u8 uint8_t;
typedef s8 int8_t;
typedef s32 off_t;
#endif

#include "player.h"

#endif

// variable used to detect the number of libplayer instances created - up to 2 instances
static int libplayer_id = 0;
//! unfortunately, saving data would not be a good solution for Mosaic Mode in ESG Application since the position changes everytime user
//! uses the navigation button
// value for input output windows, when save_data_instance1 equals to 1 <=> the second instance is already created with the correct size,
// save these values in in_instance1 and out_instance1 so that we do not need to recalculate every time
//~ static video_rect_t in_instance1, out_instance1;
//~ static save_data_instance1 = 0;

// variable use to detect whether a dvb instance has already started for instance 1
static int start_dvb = 0;

enum
{
	PLAYER_FILE = 0,
	PLAYER_DVB = 1
};


typedef struct
{
	/*the service we're responsible for*/
	GF_ClientService *service;
	u32 init;
	u32 state;
	u32 player_id;
	u32 player_type;
	u32 width;
	u32 height;
	char *url;
#ifndef TEST_LIBPLAYER
	player_t *player;
#endif

} LibPlayerIn;


static const char * LIBPLAYER_MIME_TYPES[] = {
	"video/x-mpeg", "mpg mpeg mp2 mpa mpe mpv2 ts", "MPEG 1/2 Movies",
	"video/x-mpeg-systems", "mpg mpeg mp2 mpa mpe mpv2", "MPEG 1/2 Movies",
	"audio/basic", "snd au", "Basic Audio",
	"audio/x-wav", "wav", "WAV Audio",
	"audio/vnd.wave", "wav", "WAV Audio",
	"video/x-ms-asf", "asf wma wmv asx asr", "WindowsMedia Movies",
	"video/x-ms-wmv", "asf wma wmv asx asr", "WindowsMedia Movies",
	"video/x-msvideo", "avi", "AVI Movies",
	"video/x-ms-video", "avi", "AVI Movies",
	"video/avi", "avi", "AVI Movies",
	"video/vnd.avi", "avi", "AVI Movies",
	"video/H263", "h263 263", "H263 Video",
	"video/H264", "h264 264", "H264 Video",
	"video/MPEG4", "cmp", "MPEG-4 Video",
	"video/mp4", "mp4", "MPEG-4 Movie",
	"video/quicktime", "mov qt", "QuickTime Movies",
	"video/webm", "webm", "Google WebM Movies",
	"audio/webm", "webm", "Google WebM Music",
	NULL
};


static u32 LIBPLAYER_RegisterMimeTypes(const GF_InputService *plug) {
	u32 i;
	if (!plug)
		return 0;
	for (i = 0 ; LIBPLAYER_MIME_TYPES[i] ; i+=3)
		gf_term_register_mime_type(plug, LIBPLAYER_MIME_TYPES[i], LIBPLAYER_MIME_TYPES[i+1], LIBPLAYER_MIME_TYPES[i+2]);
	return i / 3;
}

Bool LIBPLAYER_CanHandleURL(GF_InputService *plug, const char *url)
{
	char *sExt = strrchr(url, '.');
	if (sExt) {
		u32 i;
		Bool ok = 0;
		char *cgi_par;
		// case dvb
		if (!strnicmp(url, "dvb://", 6)) return 1;
		if (!strnicmp(sExt, ".gz", 3)) sExt = strrchr(sExt, '.');
		if (!strnicmp(url, "rtsp://", 7)) return 0;
		sExt++;

		cgi_par = strchr(sExt, '?');
		if (cgi_par) cgi_par[0] = 0;

		for (i = 0 ; LIBPLAYER_MIME_TYPES[i] ; i+=3) {
			if (strstr(LIBPLAYER_MIME_TYPES[i+1], sExt)) {
				ok=1;
				break;
			}
		}
		if (cgi_par) cgi_par[0] = '?';
		if (ok) return 1;
	}

	return 0;
}

#ifndef TEST_LIBPLAYER

static int on_libplayer_event(player_event_t e, void *data)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerEvent] Received event %d\n", e));

	switch (e) {
	case PLAYER_EVENT_PLAYBACK_FINISHED:
		player_playback_stop(data);
		player_playback_start(data);
		break;
	case PLAYER_EVENT_FE_HAS_LOCK:
		break;
	case PLAYER_EVENT_FE_TIMEDOUT:
		break;
	case PLAYER_EVENT_VIDEO_PICTURE:
		break;
	default:
		break;
	}

	return 0;
}



#endif

GF_Err LIBPLAYER_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	LibPlayerIn *read = (LibPlayerIn *) plug->priv;
#ifndef TEST_LIBPLAYER
	mrl_t *mrl = NULL;
#endif

	GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[LibPlayerIN] Connecting\n"));

	if (!read || !serv || !url) return GF_BAD_PARAM;

	if (!strnicmp(url, "libplayer://", 12)) url+=12;

	if (!read->init) {
		read->init=1;
		/* libplayer init with default width/height */
		read->width = 80;
		read->height = 20;
		read->url = url;
		read->player_id = libplayer_id;
		read->player_type = PLAYER_FILE;

#ifndef TEST_LIBPLAYER
		read->player = player_init(PLAYER_TYPE_DUMMY, PLAYER_AO_AUTO, PLAYER_VO_AUTO, PLAYER_MSG_INFO, read->player_id, on_libplayer_event);

		if (!read->player) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[LibPlayerIN] Failed to instanciate libplayer instance %d\n", read->player_id));
			gf_term_on_connect(serv, NULL, GF_REMOTE_SERVICE_ERROR);
			return GF_OK;
		}
#endif
		libplayer_id++;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerIN] Opening URL %s for Player instance %d\n", url, read->player_id));
	}

#ifndef TEST_LIBPLAYER
	mrl = NULL;

	// dvb
	if (!strnicmp(url, "dvb://", 6)) {
		read->player_type = PLAYER_DVB;
		mrl_resource_dvb_args_t *mrl_dvb_args;
		mrl_dvb_args = calloc(1, sizeof(mrl_resource_dvb_args_t));
		char *frequency;

		// fetch frequency
		if (frequency = strchr(url+6, '@')) {
			char *enc, *pid;
			mrl_dvb_args->frequency = atoi(frequency+1);

			/*
			 * video
			 */
			// video codec
			if (enc = strstr(url+6, "mpeg2")) {
				mrl_dvb_args->video_enc = PLAYER_VIDEO_MPEG2;
			}
			else if (enc = strstr(url+6, "h264")) {
				mrl_dvb_args->video_enc = PLAYER_VIDEO_H264;
			}
			else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[LibPlayerIN] Unknown video encoding\n"));
				mrl_dvb_args->video_enc = PLAYER_VIDEO_UNKNOWN;
			}

			// video PID
			if (mrl_dvb_args->video_enc != PLAYER_VIDEO_UNKNOWN) {
				pid = strchr(enc, ':');
				mrl_dvb_args->video_pid = atoi(pid+1);
			}

			/*
			 * audio
			 */
			// audio codec
			if (enc = strstr(url+6, "mp2")) {
				mrl_dvb_args->audio_enc = PLAYER_AUDIO_MP2;
			}
			else if (enc = strstr(url+6, "ac3")) {
				mrl_dvb_args->audio_enc = PLAYER_AUDIO_AC3;
			}
			else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[LibPlayerIN] Unknown audio encoding\n"));
				mrl_dvb_args->audio_enc = PLAYER_AUDIO_UNKNOWN;
			}

			// audio PID
			if (mrl_dvb_args->audio_enc != PLAYER_AUDIO_UNKNOWN) {
				pid = strchr(enc, ':');
				mrl_dvb_args->audio_pid = atoi(pid+1);
			}

			if (mrl_dvb_args->video_enc == PLAYER_VIDEO_UNKNOWN && mrl_dvb_args->audio_enc == PLAYER_AUDIO_UNKNOWN) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[LibPlayerIN] Unknown video and audio encoding\n"));
				free(mrl_dvb_args);
				return GF_BAD_PARAM;
			}
		}
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[LibPlayerIN] Unknown frequency\n"));
			free(mrl_dvb_args);
			return GF_BAD_PARAM;
		}

		// player instance 1 has not starter yet in dvb case <=> player instance 1 is not created yet
		if (start_dvb == 0) {
			mrl = mrl_new(read->player, MRL_RESOURCE_DVB, mrl_dvb_args);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerIN] MRL created for DVB\n"));

			// player has already started, zapping case, make sure the player is not recreated
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerIN] Starting DVB PES filtering\n"));
			player_dvb_pes_filter_start(read->player, mrl_dvb_args->video_enc, mrl_dvb_args->video_pid, mrl_dvb_args->audio_enc, mrl_dvb_args->audio_pid);
		}

	}
	else if (!strnicmp(url, "file://", 7) || !strstr(url, "://")) {
		mrl_resource_local_args_t *mrl_args;
		mrl_args = calloc(1, sizeof(mrl_resource_local_args_t));
		if (!strnicmp(url, "file://", 7)) {
			mrl_args->location = strdup(url + 7);
		}  else {
			mrl_args->location = strdup(url);
		}
		mrl = mrl_new (read->player, MRL_RESOURCE_FILE, mrl_args);

	}

	// only for DVB case to make sure player is only set once
	if (start_dvb == 0) {
		if (!mrl) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[LibPlayerIN] Failed to create MRL for url %s\n", url));
			gf_term_on_connect(serv, NULL, GF_URL_ERROR);
			return GF_OK;
		}

		player_mrl_set(read->player, mrl);
	}

#endif
	read->state = 0;
	read->service = serv;

	/*ACK connection is OK*/
	gf_term_on_connect(serv, NULL, GF_OK);


	/*setup LIBPLAYER object descriptor*/
	{
		GF_ESD *esd;
		GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		od->objectDescriptorID = 1+read->player_id;

		esd = gf_odf_desc_esd_new(0);
		esd->ESID = 1+read->player_id;
		esd->slConfig->timestampResolution = 1000;
		esd->decoderConfig->streamType = GF_STREAM_PRIVATE_MEDIA;
		esd->decoderConfig->objectTypeIndication = GPAC_OTI_PRIVATE_MEDIA_LIBPLAYER;
#ifndef TEST_LIBPLAYER
		esd->decoderConfig->decoderSpecificInfo->data = read;
#endif

		gf_list_add(od->ESDescriptors, esd);
		gf_term_add_media(read->service, (GF_Descriptor*)od, 0);
	}

	return GF_OK;
}

GF_Err LIBPLAYER_CloseService(GF_InputService *plug)
{
	LibPlayerIn *read = (LibPlayerIn *) plug->priv;

#ifndef TEST_LIBPLAYER
	// only disconnect if
	if (read->player_type == PLAYER_FILE) {
		player_playback_stop(read->player);
		fprintf(stderr, "[LibPlayerIN]player_playback_stop for instance %d\n", read->player_id);
		player_uninit(read->player);
		fprintf(stderr, "[LibPlayerIN]player_uninit for instance %d\n", read->player_id);
		read->player = NULL;
		libplayer_id--;

		read->state = 0;

		gf_term_on_disconnect(read->service, NULL, GF_OK);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerIn] Closing libplayer instance %d\n", read->player_id));



		// channel zapping dvb case, don't disconnect service
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerIn] Don't close service libplayer instance %d, use this instance for channel zapping\n", read->player_id));
	}

	return GF_OK;
#endif
}

/*Dummy input just send a file name, no multitrack to handle so we don't need to check sub_url nor expected type*/
static GF_Descriptor *LIBPLAYER_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	return NULL;
}


GF_Err LIBPLAYER_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	LibPlayerIn *read = (LibPlayerIn *) plug->priv;
	unsigned long prop = 0;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerIN] ServiceCommand for instance %d, read->state=%d\n", read->player_id, read->state));
	if (!com->base.on_channel) return GF_NOT_SUPPORTED;

	if (com->command_type==GF_NET_SERVICE_HAS_AUDIO) return GF_NOT_SUPPORTED;

	switch (com->command_type) {
	case GF_NET_CHAN_SET_PULL:
		return GF_NOT_SUPPORTED;
	case GF_NET_CHAN_INTERACTIVE:
		return GF_OK;
	/*since data is file-based, no padding is needed (decoder plugin will handle it itself)*/
	case GF_NET_CHAN_SET_PADDING:
		return GF_OK;
	case GF_NET_CHAN_BUFFER:
		return GF_OK;
		com->buffer.max = com->buffer.min = 0;
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		/*this module is not made for updates, use undefined duration*/
		com->duration.duration = -1;
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		if (read->state==0) {
#ifndef TEST_LIBPLAYER
			if ((read->player_id == 0) && (read->player_type == PLAYER_DVB) && (start_dvb == 1)) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerIN] Instance %d has already started, zapping mode\n", read->player_id));

			} else {
				player_playback_start(read->player);
				if ((read->player_id == 0) && (read->player_type == PLAYER_DVB)) {
					start_dvb = 1;
				}

				read->state = 1;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerIN] Starting playback for instance %d\n", read->player_id));
			}
#endif
		}

		return GF_OK;
	case GF_NET_CHAN_STOP:
		if (read->state==1) {
#ifndef TEST_LIBPLAYER
			// channel zapping, don't stop channel
			if ((read->player_id == 0) && (read->player_type == PLAYER_DVB) && (start_dvb = 1)) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerIN] Instance %d is in zapping mode, don't stop channel\n", read->player_id));
			} else {
				player_playback_stop(read->player);
				read->state = 0;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerIN] Stopping playback for instance %d\n", read->player_id));
			}
#endif
		}
		return GF_OK;
	case GF_NET_CHAN_CONFIG:
		return GF_OK;
	case GF_NET_CHAN_GET_DSI:
		com->get_dsi.dsi = NULL;
		com->get_dsi.dsi_len = 0;
		return GF_OK;
	}
	return GF_OK;
}

GF_Err LIBPLAYER_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ESID;
	LibPlayerIn *read = (LibPlayerIn *) plug->priv;
	sscanf(url, "ES_ID=%ud", &ESID);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerIN] instance %d connect channel %d\n", read->player_id, ESID));
	if (ESID != 1+read->player_id) {
		gf_term_on_connect(read->service, channel, GF_STREAM_NOT_FOUND);
	} else {
		gf_term_on_connect(read->service, channel, GF_OK);
	}
	return GF_OK;
}

GF_Err LIBPLAYER_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	LibPlayerIn *read = (LibPlayerIn *) plug->priv;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerIN] instance %d disconnect channel\n", read->player_id));
	gf_term_on_disconnect(read->service, channel, GF_OK);
	return GF_OK;
}



Bool LIBPLAYER_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	return 0;
	//return 1;
}

static GF_Err LIBPLAYER_AttachStream(GF_BaseDecoder *dec, GF_ESD *esd)
{
	LibPlayerIn *read;

	if (dec->privateStack) return GF_BAD_PARAM;
	if (!esd->decoderConfig->decoderSpecificInfo) return GF_BAD_PARAM;
	if (!esd->decoderConfig->decoderSpecificInfo->data) return GF_BAD_PARAM;
	read = (LibPlayerIn *) esd->decoderConfig->decoderSpecificInfo->data;
	if (esd->ESID!=1+read->player_id) return GF_BAD_PARAM;
	dec->privateStack = read;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerDEC] AttachStream for instance %d\n", read->player_id));
	esd->decoderConfig->decoderSpecificInfo->data = NULL;
	return GF_OK;
}

static GF_Err LIBPLAYER_DetachStream(GF_BaseDecoder *dec, u16 ES_ID)
{
	LibPlayerIn *player = dec->privateStack;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerDEC] DetachStream for instance %d\n", player ? player->player_id : -1));
	dec->privateStack = NULL;
	return GF_OK;
}
static GF_Err LIBPLAYER_GetCapabilities(GF_BaseDecoder *dec, GF_CodecCapability *capability)
{
	LibPlayerIn *read = dec->privateStack;
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerDEC] GetCapabilities\n"));
	switch (capability->CapCode) {
	case GF_CODEC_WIDTH:
		capability->cap.valueInt = read->width;
		break;
	case GF_CODEC_HEIGHT:
		capability->cap.valueInt = read->height;
		break;
	}
	return GF_OK;
}
static GF_Err LIBPLAYER_SetCapabilities(GF_BaseDecoder *dec, GF_CodecCapability capability)
{
	return GF_NOT_SUPPORTED;
}
static u32 LIBPLAYER_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerDEC] CanHandleStream\n"));
	if (StreamType!=GF_STREAM_PRIVATE_MEDIA) return GF_CODEC_NOT_SUPPORTED;
	/*don't reply to media type queries*/
	if (!esd) return GF_CODEC_NOT_SUPPORTED;
	if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_PRIVATE_MEDIA_LIBPLAYER) return GF_CODEC_SUPPORTED;
	return GF_CODEC_NOT_SUPPORTED;
}
static const char *LIBPLAYER_GetName(GF_BaseDecoder *dec)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerDEC] GetName\n"));
	return "LibPlayer decoder";
}

static GF_Err LIBPLAYER_Control(GF_PrivateMediaDecoder *dec, Bool mute, GF_Window *src, GF_Window *dst)
{
#ifndef TEST_LIBPLAYER
	video_rect_t in, out;
	LibPlayerIn *read = dec->privateStack;
	u32 width, height;

	if (!read) return GF_OK;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerDEC] Control instance %d\n",read->player_id));
	//! unfortunately, saving data would not be a good solution for Mosaic Mode in ESG Application since the position changes everytime user
	//! uses the navigation button
	//~ if (read->player_id == 1 && save_data_instance1 == 1) {
	//~ fprintf(stderr, "in here for save data instance\n");
	//~ player_video_io_windows_set(read->player, &in_instance1, &out_instance1);
	//~
	//~ return GF_OK;
	//~
	//~ } else {
	width = mrl_get_property(read->player, NULL, MRL_PROPERTY_VIDEO_WIDTH);
	height = mrl_get_property(read->player, NULL, MRL_PROPERTY_VIDEO_HEIGHT);
	//~ }



	if((width != read->width) || (height != read->height))	{
		fprintf(stderr, "in here for video size changed\t");
		fprintf(stderr, "width %d read->width %d height %d read->height %d\n", width, read->width, height, read->height);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerDEC] video size changed to width %d - height %d\n", width, height));
		if (width && height) {
			read->width = width;
			read->height = height;
		}
		return GF_BUFFER_TOO_SMALL;
	}

	in.x = src->x;
	in.y = src->y;
	in.w = src->w;
	in.h = src->h;
	out.x = dst->x;
	out.y = dst->y;
	out.w = dst->w;
	out.h = dst->h;
	player_video_io_windows_set(read->player, &in, &out);

	//! unfortunately, saving data would not be a good solution for Mosaic Mode in ESG Application since the position changes everytime user
	//! uses the navigation button
	//~ if (read->player_id == 1) {
	//~ in_instance1 = in;
	//~ //	//~ out_instance1.w = out.w;
	//~ //	//~ out_instance1.h = out.h;
	//~ out_instance1 = out;
	//~ save_data_instance1 = 1;
	//~ }
	//~

#endif

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[LibPlayerDEC] Repositioning video src %d %d %d %d - dest %d %d %d %d\n", src->x, src->y, src->w, src->h, dst->x, dst->y, dst->w, dst->h) );
	return GF_OK;
}


GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_NET_CLIENT_INTERFACE,
		GF_PRIVATE_MEDIA_DECODER_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) {
		LibPlayerIn *read;
		GF_InputService *plug;
		GF_SAFEALLOC(plug, GF_InputService);
		memset(plug, 0, sizeof(GF_InputService));
		GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "LibPlayer Input", "gpac distribution")

		plug->RegisterMimeTypes = LIBPLAYER_RegisterMimeTypes;
		plug->CanHandleURL = LIBPLAYER_CanHandleURL;
		plug->ConnectService = LIBPLAYER_ConnectService;
		plug->CloseService = LIBPLAYER_CloseService;
		plug->GetServiceDescriptor = LIBPLAYER_GetServiceDesc;
		plug->ConnectChannel = LIBPLAYER_ConnectChannel;
		plug->DisconnectChannel = LIBPLAYER_DisconnectChannel;
		plug->ServiceCommand = LIBPLAYER_ServiceCommand;
		plug->CanHandleURLInService = LIBPLAYER_CanHandleURLInService;

		GF_SAFEALLOC(read, LibPlayerIn);
		plug->priv = read;
		return (GF_BaseInterface *)plug;

	} else if (InterfaceType == GF_PRIVATE_MEDIA_DECODER_INTERFACE) {
		GF_PrivateMediaDecoder *ifce;

		GF_SAFEALLOC(ifce, GF_PrivateMediaDecoder);
		GF_REGISTER_MODULE_INTERFACE(ifce, GF_PRIVATE_MEDIA_DECODER_INTERFACE, "LibPlayer Decoder", "gpac distribution")

		/*setup our own interface*/
		ifce->AttachStream = LIBPLAYER_AttachStream;
		ifce->DetachStream = LIBPLAYER_DetachStream;
		ifce->GetCapabilities = LIBPLAYER_GetCapabilities;
		ifce->SetCapabilities = LIBPLAYER_SetCapabilities;
		ifce->Control = LIBPLAYER_Control;
		ifce->CanHandleStream = LIBPLAYER_CanHandleStream;
		ifce->GetName = LIBPLAYER_GetName;
		return (GF_BaseInterface *) ifce;
	}
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *bi)
{
	if (bi->InterfaceType==GF_NET_CLIENT_INTERFACE) {
		GF_InputService *ifcn = (GF_InputService*)bi;
		LibPlayerIn *read = (LibPlayerIn*)ifcn->priv;
		gf_free(read);
		gf_free(bi);
	} else if (bi->InterfaceType == GF_PRIVATE_MEDIA_DECODER_INTERFACE) {
		gf_free(bi);
	}
}

GPAC_MODULE_STATIC_DELARATION( libplayer )
