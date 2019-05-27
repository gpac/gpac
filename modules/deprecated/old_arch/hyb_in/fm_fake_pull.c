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

/*hybrid media interface implementation generating fake audio consisting in beeps every second in pull mode*/

#include <gpac/thread.h>
#include "hyb_in.h"

/**********************************************************************************************************************/

#define EXT_MEDIA_LOAD_THREADED

/**********************************************************************************************************************/

#define FM_FAKE_PULL_AUDIO_FREQ 44100
#define FM_FAKE_PULL_CHAN_NUM	2
#define FM_FAKE_PULL_BITS		16
#define FM_FAKE_PULL_TYPE		s16
#define FM_FAKE_PULL_FRAME_DUR	60		/*in ms*/
#define FM_FAKE_PULL_FRAME_LEN	((FM_FAKE_PULL_FRAME_DUR*FM_FAKE_PULL_CHAN_NUM*FM_FAKE_PULL_BITS*FM_FAKE_PULL_AUDIO_FREQ)/(1000*8))		/*in bytes*/

/**********************************************************************************************************************/

typedef struct s_FM_FAKE_PULL {
	u64 PTS;
	unsigned char buffer10[FM_FAKE_PULL_FRAME_LEN]; /*played 10 percent of time*/
	unsigned char buffer90[FM_FAKE_PULL_FRAME_LEN]; /*played 90 percent of time*/

#ifdef EXT_MEDIA_LOAD_THREADED
	GF_Thread *media_th;
#endif
} FM_FAKE_PULL;
FM_FAKE_PULL FM_FAKE_PULL_private_data;

/**********************************************************************************************************************/

static Bool FM_FAKE_PULL_CanHandleURL(const char *url);
static GF_ObjectDescriptor* FM_FAKE_PULL_GetOD(void);
static GF_Err FM_FAKE_PULL_Connect(GF_HYBMEDIA *self, GF_ClientService *service, const char *url);
static GF_Err FM_FAKE_PULL_Disconnect(GF_HYBMEDIA *self);
static GF_Err FM_FAKE_PULL_GetData(GF_HYBMEDIA *self, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr);
static GF_Err FM_FAKE_PULL_ReleaseData(GF_HYBMEDIA *self);

GF_HYBMEDIA master_fm_fake_pull = {
	"Fake FM (pull mode)",		/*name*/
	FM_FAKE_PULL_CanHandleURL,	/*CanHandleURL()*/
	FM_FAKE_PULL_GetOD,			/*GetOD()*/
	FM_FAKE_PULL_Connect,		/*Connect()*/
	FM_FAKE_PULL_Disconnect,	/*Disconnect()*/
	NULL,						/*SetState()*/
	FM_FAKE_PULL_GetData,		/*GetData()*/
	FM_FAKE_PULL_ReleaseData,	/*ReleaseData()*/
	HYB_PULL,					/*data_mode*/
	&FM_FAKE_PULL_private_data	/*private_data*/
};

/**********************************************************************************************************************/

static Bool FM_FAKE_PULL_CanHandleURL(const char *url)
{
	if (!strnicmp(url, "fm://fake_pull", 14))
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
	esd->slConfig->timestampResolution = FM_FAKE_PULL_AUDIO_FREQ;

	/*Decoder Specific Info for raw media*/
	gf_bs_write_u32(dsi, FM_FAKE_PULL_AUDIO_FREQ);	/*u32 sample_rate*/
	gf_bs_write_u16(dsi, FM_FAKE_PULL_CHAN_NUM);	/*u16 nb_channels*/
	gf_bs_write_u16(dsi, FM_FAKE_PULL_BITS);		/*u16 nb_bits_per_sample*/
	gf_bs_write_u32(dsi, FM_FAKE_PULL_FRAME_LEN);	/*u32 frame_size*/
	gf_bs_write_u32(dsi, 0);						/*u32 channel_config*/
	gf_bs_get_content(dsi, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
	gf_bs_del(dsi);

	return esd;
}

/**********************************************************************************************************************/

static GF_ObjectDescriptor* FM_FAKE_PULL_GetOD(void)
{
	/*declare object to terminal*/
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor*)gf_odf_desc_new(GF_ODF_OD_TAG);
	GF_ESD *esd = get_esd();
	od->objectDescriptorID = 1;
	gf_list_add(od->ESDescriptors, esd);
	return od;
}

/**********************************************************************************************************************/

static u32 ext_media_load_th(void *par) {
	GF_HYBMEDIA *self = (GF_HYBMEDIA*) par;
	/*declare object to terminal*/
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor*)gf_odf_desc_new(GF_ODF_OD_TAG);
	od->URLString = gf_strdup("http://gpac.sourceforge.net/screenshots/lion.jpg");
	od->objectDescriptorID = 0;
	gf_sleep(2000); //TODO: remove the sleep
	gf_service_declare_media(self->owner, (GF_Descriptor*)od, 0);
	return 0;
}

static GF_Err FM_FAKE_PULL_Connect(GF_HYBMEDIA *self, GF_ClientService *service, const char *url)
{
	u32 i;

	if (!self)
		return GF_BAD_PARAM;

	if (!service)
		return GF_BAD_PARAM;

	self->owner = service;

	/*set audio preloaded data*/
	assert(self->private_data);
	memset(self->private_data, 0, sizeof(FM_FAKE_PULL));
	for (i=0; i<(FM_FAKE_PULL_FRAME_LEN*8)/FM_FAKE_PULL_BITS; i++) {
		if (((2*i)/(FM_FAKE_PULL_CHAN_NUM*100))%2) /*100Hz*/
			*((FM_FAKE_PULL_TYPE*)((FM_FAKE_PULL*)self->private_data)->buffer10+i) = 1 << (FM_FAKE_PULL_BITS-1);
	}

	/*for hybrid scenarios: add an external media*/
	if (1) {
#ifdef EXT_MEDIA_LOAD_THREADED
		GF_Thread **th = &((FM_FAKE_PULL*)self->private_data)->media_th;
		assert(*th == NULL);	//once at a time
		*th = gf_th_new("HYB-FM fake external media load thread");
		gf_th_run(*th, ext_media_load_th, self);
#else
		ext_media_load_th(self);
#endif
		//wait for video to begin as late video creates desynchro.
		//gf_sleep(5000);
	}

	return GF_OK;
}

/**********************************************************************************************************************/

static GF_Err FM_FAKE_PULL_Disconnect(GF_HYBMEDIA *self)
{
	FM_FAKE_PULL *priv = (FM_FAKE_PULL*)self->private_data;
	self->owner = NULL;
#ifdef EXT_MEDIA_LOAD_THREADED
	gf_th_del(priv->media_th);
#endif
	return GF_OK;
}

/**********************************************************************************************************************/

static GF_Err FM_FAKE_PULL_GetData(GF_HYBMEDIA *self, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr)
{
	u64 delta_pts = (FM_FAKE_PULL_FRAME_DUR*FM_FAKE_PULL_AUDIO_FREQ)/1000;
	assert(!(FM_FAKE_PULL_FRAME_DUR*FM_FAKE_PULL_AUDIO_FREQ%1000));

	/*write SL header*/
	memset(out_sl_hdr, 0, sizeof(GF_SLHeader));
	out_sl_hdr->compositionTimeStampFlag = 1;
	out_sl_hdr->compositionTimeStamp = ((FM_FAKE_PULL*)self->private_data)->PTS;
	out_sl_hdr->accessUnitStartFlag = 1;

	/*write audio data*/
	if ((((FM_FAKE_PULL*)self->private_data)->PTS%(10*delta_pts))) {
		*out_data_ptr = ((FM_FAKE_PULL*)self->private_data)->buffer90;
	} else {
		*out_data_ptr = ((FM_FAKE_PULL*)self->private_data)->buffer10;
	}
	*out_data_size = FM_FAKE_PULL_FRAME_LEN;
	((FM_FAKE_PULL*)self->private_data)->PTS += delta_pts;

	return GF_OK;
}

/**********************************************************************************************************************/

static GF_Err FM_FAKE_PULL_ReleaseData(GF_HYBMEDIA *self)
{
	return GF_OK;
}

/**********************************************************************************************************************/
