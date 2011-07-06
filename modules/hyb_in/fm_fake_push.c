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

#include "hyb_in.h"


#define FM_FAKE_PUSH_AUDIO_FREQ 44100
#define FM_FAKE_PUSH_CHAN_NUM	2
#define FM_FAKE_PUSH_BITS		16
#define FM_FAKE_PUSH_TYPE		s16
#define FM_FAKE_PUSH_FRAME_DUR	60	/*in ms*/
#define FM_FAKE_PUSH_FRAME_LEN	((FM_FAKE_PUSH_FRAME_DUR*FM_FAKE_PUSH_CHAN_NUM*FM_FAKE_PUSH_BITS*FM_FAKE_PUSH_AUDIO_FREQ)/(1000*8))		/*in bytes*/
	

typedef struct s_FM_FAKE_PUSH {
	u64 PTS;
	unsigned char buffer10[FM_FAKE_PUSH_FRAME_LEN]; /*played 10 percent of time*/
	unsigned char buffer90[FM_FAKE_PUSH_FRAME_LEN]; /*played 90 percent of time*/

	GF_Thread *th;
	volatile u32 th_state;

} FM_FAKE_PUSH;
FM_FAKE_PUSH FM_FAKE_PUSH_private_data;

/**********************************************************************************************************************/
/*  Declare FM_FAKE_PUSH interfaces  */
/**********************************************************************************************************************/
Bool FM_FAKE_PUSH_CanHandleURL(const char *url);
GF_ESD* FM_FAKE_PUSH_GetESD();
GF_ObjectDescriptor* FM_FAKE_PUSH_GetOD(GF_HYBMEDIA *self);
GF_Err FM_FAKE_PUSH_Connect(GF_HYBMEDIA *self, GF_ClientService *service, const char *url);
GF_Err FM_FAKE_PUSH_Disconnect(GF_HYBMEDIA *self, GF_ClientService *service);
GF_Err FM_FAKE_PUSH_AddMedia(GF_HYBMEDIA *self);
GF_Err FM_FAKE_PUSH_GetData(GF_HYBMEDIA *self, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr);
GF_Err FM_FAKE_PUSH_ReleaseData(GF_HYBMEDIA *self);

GF_HYBMEDIA master_fm_fake_push = {
	FM_FAKE_PUSH_CanHandleURL,
	FM_FAKE_PUSH_GetOD,
	FM_FAKE_PUSH_Connect,
	FM_FAKE_PUSH_Disconnect,
	FM_FAKE_PUSH_AddMedia,
	NULL,
	NULL,
	HYB_PUSH,
	NULL,
	NULL,
	&FM_FAKE_PUSH_private_data
};
/**********************************************************************************************************************/

static Bool FM_FAKE_PUSH_CanHandleURL(const char *url)
{
	if (!strnicmp(url, "fm://fake_push", 14))
		return 1;

	return 0;
}

static GF_ESD* FM_FAKE_PUSH_GetESD()
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

static GF_ObjectDescriptor* FM_FAKE_PUSH_GetOD(GF_HYBMEDIA *self)
{
	/*declare object to terminal*/
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor*)gf_odf_desc_new(GF_ODF_OD_TAG);
	GF_ESD *esd = FM_FAKE_PUSH_GetESD();
	od->objectDescriptorID = 1;
	od->service_ifce = self->owner;
	gf_list_add(od->ESDescriptors, esd);
	return od;
}

static u32 audio_gen_th(void *par) 
{
	GF_Err e;
	char *data;
	u32 data_size, init_time = gf_sys_clock();
	s32 time_to_wait = 0;
	GF_SLHeader slh;
	GF_HYBMEDIA *self = (GF_HYBMEDIA*) par;
	FM_FAKE_PUSH *fm_fake_push = (FM_FAKE_PUSH*)self->private_data;
	memset(&slh, 0, sizeof(GF_SLHeader));

	while (!fm_fake_push->th_state) /*FM_MMBTOOLS_STATE_RUNNING*/
	{
		if (!time_to_wait)
			init_time = gf_sys_clock();

		time_to_wait = (u32)(init_time + ((u64)slh.compositionTimeStamp*1000)/FM_FAKE_PUSH_AUDIO_FREQ - gf_sys_clock());
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[HYB_IN] FM_FAKE_PUSH - %d ms before next AU\n", time_to_wait));
		if (time_to_wait > 0)
			gf_sleep(time_to_wait);

		e = FM_FAKE_PUSH_GetData(self, &data, &data_size, &slh);
		gf_term_on_sl_packet(self->owner, self->channel, data, data_size, &slh, e);
	}

	fm_fake_push->th_state = HYB_STATE_STOPPED;

	return 0;
}

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

	priv->th = gf_th_new("HYB-FM fake audio generation thread");
	gf_th_run(priv->th, audio_gen_th, self);

	return GF_OK;
}

static GF_Err FM_FAKE_PUSH_Disconnect(GF_HYBMEDIA *self, GF_ClientService *service)
{
	FM_FAKE_PUSH *fm_fake_push = (FM_FAKE_PUSH*)self->private_data;

	/*stop the audio_generation thread*/
	fm_fake_push->th_state = HYB_STATE_STOPPING;
	while (fm_fake_push->th_state != HYB_STATE_STOPPED)
		gf_sleep(10);

	self->owner = NULL;
	return GF_OK;
}

static GF_Err FM_FAKE_PUSH_AddMedia(GF_HYBMEDIA *self)
{
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor*)gf_odf_desc_new(GF_ODF_OD_TAG);
	//not implemented
	assert(0);
	gf_term_add_media(self->owner, (GF_Descriptor*)od, 0);
	return GF_OK;
}

static GF_Err FM_FAKE_PUSH_GetData(GF_HYBMEDIA *self, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr)
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

static GF_Err FM_FAKE_PUSH_ReleaseData(GF_HYBMEDIA *self)
{
	return GF_OK;
}
