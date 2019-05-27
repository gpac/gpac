/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau
 *			Copyright (c) Telecom ParisTech 2010-20xx
 *					All rights reserved
 *
 *  This file is part of GPAC / Hybrid Media input module
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

/*hybrid media interface implementation generating fake audio consisting in beeps every second in push mode*/

#include <gpac/thread.h>
#include <gpac/modules/service.h>
#include <time.h>
#include "hyb_in.h"

/**********************************************************************************************************************/

#define EXT_MEDIA_LOAD_THREADED

/**********************************************************************************************************************/

#define FM_FAKE_PUSH_AUDIO_FREQ 22050
#define FM_FAKE_PUSH_CHAN_NUM	2
#define FM_FAKE_PUSH_BITS		16
#define FM_FAKE_PUSH_TYPE		s16
#define FM_FAKE_PUSH_FRAME_DUR	60		/*in ms*/
#define FM_FAKE_PUSH_FRAME_LEN	((FM_FAKE_PUSH_FRAME_DUR*FM_FAKE_PUSH_CHAN_NUM*FM_FAKE_PUSH_BITS*FM_FAKE_PUSH_AUDIO_FREQ)/(1000*8))		/*in bytes*/

/**********************************************************************************************************************/

typedef struct s_FM_FAKE_PUSH {
	u64 PTS;
	unsigned char buffer10[FM_FAKE_PUSH_FRAME_LEN]; /*played 10 percent of time*/
	unsigned char buffer90[FM_FAKE_PUSH_FRAME_LEN]; /*played 90 percent of time*/

	GF_Thread *th;
#ifdef EXT_MEDIA_LOAD_THREADED
	GF_Thread *media_th;
#endif
} FM_FAKE_PUSH;
FM_FAKE_PUSH FM_FAKE_PUSH_private_data;

/**********************************************************************************************************************/

static Bool FM_FAKE_PUSH_CanHandleURL(const char *url);
static GF_ObjectDescriptor* FM_FAKE_PUSH_GetOD(void);
static GF_Err FM_FAKE_PUSH_Connect(GF_HYBMEDIA *self, GF_ClientService *service, const char *url);
static GF_Err FM_FAKE_PUSH_Disconnect(GF_HYBMEDIA *self);
static GF_Err FM_FAKE_PUSH_SetState(GF_HYBMEDIA *self, const GF_NET_CHAN_CMD state);

GF_HYBMEDIA master_fm_fake_push = {
	"Fake FM (push mode)",		/*name*/
	FM_FAKE_PUSH_CanHandleURL,	/*CanHandleURL()*/
	FM_FAKE_PUSH_GetOD,			/*GetOD()*/
	FM_FAKE_PUSH_Connect,		/*Connect()*/
	FM_FAKE_PUSH_Disconnect,	/*Disconnect()*/
	FM_FAKE_PUSH_SetState,		/*SetState()*/
	NULL,						/*GetData()*/
	NULL,						/*ReleaseData()*/
	HYB_PUSH,					/*data_mode*/
	&FM_FAKE_PUSH_private_data	/*private_data*/
};

/**********************************************************************************************************************/

static Bool FM_FAKE_PUSH_CanHandleURL(const char *url)
{
	if (!strnicmp(url, "fm://fake_push", 14))
		return 1;

	return 0;
}

/**********************************************************************************************************************/

static GF_ESD* get_esd()
{
	GF_BitStream *dsi = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	GF_ESD *esd = gf_odf_desc_esd_new(0);

	esd->ESID = 1;
	esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	esd->decoderConfig->objectTypeIndication = GPAC_OTI_RAW_MEDIA_STREAM;
	esd->decoderConfig->avgBitrate = esd->decoderConfig->maxBitrate = 0;
	esd->slConfig->timestampResolution = FM_FAKE_PUSH_AUDIO_FREQ;

	/*Decoder Specific Info for raw media*/
	gf_bs_write_u32(dsi, FM_FAKE_PUSH_AUDIO_FREQ);	/*u32 sample_rate*/
	gf_bs_write_u16(dsi, FM_FAKE_PUSH_CHAN_NUM);	/*u16 nb_channels*/
	gf_bs_write_u16(dsi, FM_FAKE_PUSH_BITS);		/*u16 nb_bits_per_sample*/
	gf_bs_write_u32(dsi, FM_FAKE_PUSH_FRAME_LEN);	/*u32 frame_size*/
	gf_bs_write_u32(dsi, 0);						/*u32 channel_config*/
	gf_bs_get_content(dsi, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
	gf_bs_del(dsi);

	return esd;
}

/**********************************************************************************************************************/

static GF_ObjectDescriptor* FM_FAKE_PUSH_GetOD(void)
{
	/*declare object to terminal*/
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor*)gf_odf_desc_new(GF_ODF_OD_TAG);
	GF_ESD *esd = get_esd();
	od->objectDescriptorID = 1;
	gf_list_add(od->ESDescriptors, esd);
	return od;
}

/**********************************************************************************************************************/

static GF_Err GetData(const GF_HYBMEDIA *self, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr)
{
	u64 delta_pts = (FM_FAKE_PUSH_FRAME_DUR*FM_FAKE_PUSH_AUDIO_FREQ)/1000;
	assert(!(FM_FAKE_PUSH_FRAME_DUR*FM_FAKE_PUSH_AUDIO_FREQ%1000));

	/*write SL header*/
	memset(out_sl_hdr, 0, sizeof(GF_SLHeader));
	out_sl_hdr->compositionTimeStampFlag = 1;
	out_sl_hdr->compositionTimeStamp = ((FM_FAKE_PUSH*)self->private_data)->PTS;
	out_sl_hdr->accessUnitStartFlag = 1;

	/*write audio data*/
	if ((((FM_FAKE_PUSH*)self->private_data)->PTS%(10*delta_pts))) {
		*out_data_ptr = ((FM_FAKE_PUSH*)self->private_data)->buffer90;
	} else {
		*out_data_ptr = ((FM_FAKE_PUSH*)self->private_data)->buffer10;
	}
	*out_data_size = FM_FAKE_PUSH_FRAME_LEN;
	((FM_FAKE_PUSH*)self->private_data)->PTS += delta_pts;

	return GF_OK;
}

/**********************************************************************************************************************/

u32 ext_media_load_th(void *par) {
	GF_HYBMEDIA *self = (GF_HYBMEDIA*) par;
	/*declare object to terminal*/
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor*)gf_odf_desc_new(GF_ODF_OD_TAG);
	od->URLString = gf_strdup("http://gpac.sourceforge.net/screenshots/lion.jpg");
	od->objectDescriptorID = 0;
	gf_sleep(2000); //TODO: remove the sleep
	gf_service_declare_media(self->owner, (GF_Descriptor*)od, 0);
	return 0;
}

static u32 audio_gen_th(void *par)
{
	GF_Err e;
	char *data;
	u32 data_size, init_time;
	volatile s32 time_to_wait = 0;
	GF_SLHeader slh;
	GF_HYBMEDIA *self = (GF_HYBMEDIA*) par;
	FM_FAKE_PUSH *fm_fake_push = (FM_FAKE_PUSH*)self->private_data;
	memset(&slh, 0, sizeof(GF_SLHeader));

	{
		/*for hybrid scenarios: add an external media*/
		if (1) {
#ifdef EXT_MEDIA_LOAD_THREADED
			GF_Thread **th = &((FM_FAKE_PUSH*)self->private_data)->media_th;
			assert(*th == NULL);	//once at a time
			*th = gf_th_new("HYB-FM fake external media load thread");
			gf_th_run(*th, ext_media_load_th, par);
#else
			ext_media_load_th(par);
#endif
			gf_sleep(2000); //TODO: remove the sleep
		}

		if (0) {
			time_t now;
			struct tm *now_tm;
			time(&now);
			now_tm = gmtime(&now);
			{
				GF_NetworkCommand com;
				memset(&com, 0, sizeof(com));
				com.command_type = GF_NET_CHAN_MAP_TIME;

				com.map_time.media_time = now_tm->tm_hour*3600+now_tm->tm_min*60+now_tm->tm_sec;
				com.map_time.timestamp = slh.compositionTimeStamp;
				com.map_time.reset_buffers = 0;
				com.base.on_channel = self->channel;
				gf_service_command(self->owner, &com, GF_OK);
				GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[HYB In] Mapping WC  Time %04d/%02d/%02d %02d:%02d:%02d and Hyb time "LLD"\n",
				                                   (now_tm->tm_year + 1900), (now_tm->tm_mon + 1), now_tm->tm_mday, now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec,
				                                   com.map_time.timestamp));
			}
		}

		/*initialize clock*/
		init_time = gf_sys_clock();
	}

	while (self->state >= 0) /*pause or play*/
	{
		if (self->state == HYB_STATE_PAUSE) {
			gf_sleep(10);
			init_time = (u32)(gf_sys_clock() - ((u64)slh.compositionTimeStamp*1000)/FM_FAKE_PUSH_AUDIO_FREQ - 50/*50ms delay*/);	/*pause: won't wait at resume*/
			continue;
		}

#if 0
		time_to_wait = (s32)(init_time + ((u64)slh.compositionTimeStamp*1000)/FM_FAKE_PUSH_AUDIO_FREQ - gf_sys_clock());
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[HYB_IN] FM_FAKE_PUSH - %d ms before next AU\n", time_to_wait));
		if (time_to_wait > 0) {
			if (time_to_wait > 1000) /*TODO: understand the big shifts when playing icecasts contents*/
				GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[HYB_IN] FM_FAKE_PUSH - audio asked to sleep for %d ms\n", time_to_wait));
			gf_sleep(time_to_wait);
		}
#endif

		e = GetData(self, &data, &data_size, &slh);
		gf_service_send_packet(self->owner, self->channel, data, data_size, &slh, e);
	}

	self->state = HYB_STATE_STOPPED;

	return 0;
}

/**********************************************************************************************************************/

static void audio_gen_stop(GF_HYBMEDIA *self)
{
	if (self->state >= 0) { /*only when playing*/
		self->state = HYB_STATE_STOP_REQ;
		while (self->state != HYB_STATE_STOPPED)
			gf_sleep(10);
	}
}

/**********************************************************************************************************************/

static GF_Err FM_FAKE_PUSH_Connect(GF_HYBMEDIA *self, GF_ClientService *service, const char *url)
{
	u32 i;
	FM_FAKE_PUSH *priv;

	if (!self)
		return GF_BAD_PARAM;

	if (!service)
		return GF_BAD_PARAM;

	priv = (FM_FAKE_PUSH*)self->private_data;
	if (!priv)
		return GF_BAD_PARAM;

	self->owner = service;

	/*set audio preloaded data*/
	memset(self->private_data, 0, sizeof(FM_FAKE_PUSH));
	for (i=0; i<(FM_FAKE_PUSH_FRAME_LEN*8)/FM_FAKE_PUSH_BITS; i++) {
		if (((2*i)/(FM_FAKE_PUSH_CHAN_NUM*100))%2) /*100Hz*/
			*((FM_FAKE_PUSH_TYPE*)((FM_FAKE_PUSH*)self->private_data)->buffer10+i) = 1 << (FM_FAKE_PUSH_BITS-1);
	}

	assert(!priv->th);
	priv->th = gf_th_new("HYB-FM fake audio generation thread");
	self->state = HYB_STATE_PAUSE;
	gf_th_run(priv->th, audio_gen_th, self);

	return GF_OK;
}

/**********************************************************************************************************************/

static GF_Err FM_FAKE_PUSH_Disconnect(GF_HYBMEDIA *self)
{
	FM_FAKE_PUSH *priv = (FM_FAKE_PUSH*)self->private_data;

	audio_gen_stop(self);
	gf_th_del(priv->th);
#ifdef EXT_MEDIA_LOAD_THREADED
	gf_th_del(priv->media_th);
#endif
	priv->th = NULL;

	self->owner = NULL;
	return GF_OK;
}

/**********************************************************************************************************************/

static GF_Err FM_FAKE_PUSH_SetState(GF_HYBMEDIA *self, const GF_NET_CHAN_CMD state)
{
	switch(state) {
	case GF_NET_CHAN_PLAY:
		self->state = HYB_STATE_PLAYING;
		break;
	case GF_NET_CHAN_STOP:
		audio_gen_stop(self);
		break;
	case GF_NET_CHAN_PAUSE:
		self->state = HYB_STATE_PAUSE;
		break;
	case GF_NET_CHAN_RESUME:
		self->state = HYB_STATE_PLAYING;
		break;
	default:
		return GF_BAD_PARAM;
	}

	return GF_OK;
}

/**********************************************************************************************************************/
