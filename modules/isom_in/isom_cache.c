/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / MP4 cache module
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

#include "isom_in.h"

#ifndef GPAC_DISABLE_ISOM_WRITE

static GF_Err ISOW_Open(GF_StreamingCache *mc, GF_ClientService *serv, const char *location_and_name, Bool keep_existing_files)
{
	char szRoot[GF_MAX_PATH], szPath[GF_MAX_PATH], *ext;
	ISOMReader *cache = (ISOMReader *)mc->priv;
	if (cache->mov || cache->service) return GF_BAD_PARAM;

	strcpy(szRoot, location_and_name);
	ext = strrchr(szRoot, '.');
	if (ext) ext[0] = 0;
	strcpy(szPath, szRoot);
	strcat(szPath, ".mp4");
	if (keep_existing_files) {
		FILE *f = gf_f64_open(szPath, "rb");
		if (f) {
			u32 i=0;
			fclose(f);
			while (1) {
				sprintf(szPath, "%s_%04d.mp4", szRoot, i);
				f = gf_f64_open(szPath, "rb");
				if (!f) break;
				fclose(f);
				i++;
			}
		}
	}

	/*create a new movie in write mode (eg no editing)*/
	cache->mov = gf_isom_open(szPath, GF_ISOM_OPEN_WRITE, NULL);
	if (!cache->mov) return gf_isom_last_error(NULL);
	cache->service = serv;
	return GF_OK;
}

static GF_Err ISOW_Close(GF_StreamingCache *mc, Bool delete_cache)
{
	GF_Err e;
	ISOMReader *cache = (ISOMReader *)mc->priv;
	if (!cache->mov || !cache->service) return GF_BAD_PARAM;

	while (gf_list_count(cache->channels)) {
		ISOMChannel *ch = (ISOMChannel *)gf_list_get(cache->channels, 0);
		gf_list_rem(cache->channels, 0);
		if (ch->cache_sample) {
			gf_isom_add_sample(cache->mov, ch->track, 1, ch->cache_sample);
			gf_isom_sample_del(&ch->cache_sample);
		}
		gf_free(ch);
	}
	if (delete_cache) {
		gf_isom_delete(cache->mov);
		e = GF_OK;
	} else {
		e = gf_isom_close(cache->mov);
	}
	cache->mov = NULL;
	cache->service = NULL;
	return e;
}
static GF_Err ISOW_Write(GF_StreamingCache *mc, LPNETCHANNEL ch, char *data, u32 data_size, GF_SLHeader *sl_hdr)
{
	ISOMChannel *mch;
	GF_ESD *esd;
	u32 di, mtype;
	u64 DTS, CTS;
	ISOMReader *cache = (ISOMReader *)mc->priv;
	if (!cache->mov || !cache->service) return GF_BAD_PARAM;

	mch = isor_get_channel(cache, ch);
	if (!mch) {
		Bool mapped;
		GF_NetworkCommand com;
		com.base.on_channel = ch;
		com.base.command_type = GF_NET_CHAN_GET_ESD;
		gf_service_command(cache->service, &com, GF_OK);
		if (!com.cache_esd.esd) return GF_SERVICE_ERROR;

		esd = (GF_ESD *)com.cache_esd.esd;
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_OD:
			mtype = GF_ISOM_MEDIA_OD;
			break;
		case GF_STREAM_SCENE:
			mtype = GF_ISOM_MEDIA_SCENE;
			break;
		case GF_STREAM_VISUAL:
			mtype = GF_ISOM_MEDIA_VISUAL;
			break;
		case GF_STREAM_AUDIO:
			mtype = GF_ISOM_MEDIA_AUDIO;
			break;
		case GF_STREAM_MPEG7:
			mtype = GF_ISOM_MEDIA_MPEG7;
			break;
		case GF_STREAM_OCI:
			mtype = GF_ISOM_MEDIA_OCI;
			break;
		case GF_STREAM_IPMP:
			mtype = GF_ISOM_MEDIA_IPMP;
			break;
		case GF_STREAM_MPEGJ:
			mtype = GF_ISOM_MEDIA_MPEGJ;
			break;
		case GF_STREAM_TEXT:
			mtype = GF_ISOM_MEDIA_TEXT;
			break;
		default:
			return GF_NOT_SUPPORTED;
		}
		GF_SAFEALLOC(mch, ISOMChannel);
		mch->time_scale = esd->slConfig->timestampResolution;
		mch->streamType = esd->decoderConfig->streamType;
		mch->track = gf_isom_new_track(cache->mov, com.cache_esd.esd->ESID, mtype, mch->time_scale);
		mch->is_playing = 1;
		mch->channel = ch;
		mch->owner = cache;
		gf_isom_set_track_enabled(cache->mov, mch->track, 1);
		/*translate 3GP streams to MP4*/
		mapped = 0;
		if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_MEDIA_GENERIC) {
			char szCode[5];
			strncpy(szCode, esd->decoderConfig->decoderSpecificInfo->data, 4);
			szCode[4]=0;
			if (!stricmp(szCode, "samr") || !stricmp(szCode, "amr ") || !stricmp(szCode, "sawb")) {
				GF_3GPConfig amrc;
				mapped = 1;
				memset(&amrc, 0, sizeof(GF_3GPConfig));

				amrc.frames_per_sample = (u32) esd->decoderConfig->decoderSpecificInfo->data[13];
				amrc.type = (!stricmp(szCode, "sawb")) ? GF_ISOM_SUBTYPE_3GP_AMR_WB : GF_ISOM_SUBTYPE_3GP_AMR;
				amrc.vendor = GF_4CC('G','P','A','C');
				gf_isom_3gp_config_new(cache->mov, mch->track, &amrc, NULL, NULL, &di);
			} else if (!stricmp(szCode, "h263")) {
				GF_3GPConfig h263c;
				memset(&h263c, 0, sizeof(GF_3GPConfig));
				h263c.type = GF_ISOM_SUBTYPE_3GP_H263;
				h263c.vendor = GF_4CC('G','P','A','C');
				gf_isom_3gp_config_new(cache->mov, mch->track, &h263c, NULL, NULL, &di);
				mapped = 1;
			}
		}
		if (!mapped) gf_isom_new_mpeg4_description(cache->mov, mch->track, esd, NULL, NULL, &di);
		if (com.cache_esd.is_iod_stream) gf_isom_add_track_to_root_od(cache->mov, mch->track);
		gf_list_add(cache->channels, mch);
	}

	/*first sample, cache it*/
	if (!mch->cache_sample) {
		mch->cache_seed_ts = sl_hdr->decodingTimeStamp;
		mch->cache_sample = gf_isom_sample_new();
		mch->cache_sample->IsRAP = sl_hdr->randomAccessPointFlag;
		mch->cache_sample->dataLength = data_size;
		mch->cache_sample->data = (char*)gf_malloc(sizeof(char)*data_size);
		memcpy(mch->cache_sample->data, data, sizeof(char)*data_size);
		return GF_OK;
	}

	/*adjust DTS/CTS*/
	DTS = sl_hdr->decodingTimeStamp - mch->cache_seed_ts;

	if ((mch->streamType==GF_STREAM_VISUAL) && (DTS<=mch->cache_sample->DTS)) {
		assert(DTS>mch->prev_dts);
		CTS = mch->cache_sample->DTS + mch->cache_sample->CTS_Offset;
		mch->cache_sample->CTS_Offset = 0;

		/*first time, shift all CTS*/
		if (!mch->frame_cts_offset) {
			u32 i, count = gf_isom_get_sample_count(cache->mov, mch->track);
			mch->frame_cts_offset = (u32) (DTS-mch->prev_dts);
			for (i=0; i<count; i++) {
				gf_isom_modify_cts_offset(cache->mov, mch->track, i+1, mch->frame_cts_offset);
			}
			mch->cache_sample->CTS_Offset += mch->frame_cts_offset;
		}
		mch->cache_sample->DTS = mch->prev_dts + mch->frame_cts_offset;
		mch->cache_sample->CTS_Offset += (u32) (CTS-mch->cache_sample->DTS);
	}
	/*deal with reference picture insertion: if no CTS offset and biggest CTS until now, this is
	a reference insertion - we must check that in order to make sure we have strictly increasing DTSs*/
	if (mch->max_cts && !mch->cache_sample->CTS_Offset && (mch->cache_sample->DTS+mch->cache_sample->CTS_Offset > mch->max_cts)) {
		assert(mch->cache_sample->DTS > mch->prev_dts + mch->frame_cts_offset);
		CTS = mch->cache_sample->DTS + mch->cache_sample->CTS_Offset;
		mch->cache_sample->DTS = mch->prev_dts + mch->frame_cts_offset;
		mch->cache_sample->CTS_Offset = (u32) (CTS-mch->cache_sample->DTS);
	}
	if (mch->cache_sample->CTS_Offset)
		mch->max_cts = mch->cache_sample->DTS+mch->cache_sample->CTS_Offset;

	/*add cache*/
	gf_isom_add_sample(cache->mov, mch->track, 1, mch->cache_sample);
	assert(!mch->prev_dts || (mch->prev_dts < mch->cache_sample->DTS));
	mch->prev_dts = mch->cache_sample->DTS;
	mch->duration = MAX(mch->max_cts, mch->prev_dts);
	gf_isom_sample_del(&mch->cache_sample);

	/*store sample*/
	mch->cache_sample = gf_isom_sample_new();
	mch->cache_sample->IsRAP = sl_hdr->randomAccessPointFlag;
	mch->cache_sample->DTS = DTS + mch->frame_cts_offset;
	mch->cache_sample->CTS_Offset = (u32) (sl_hdr->compositionTimeStamp - mch->cache_seed_ts - DTS);
	mch->cache_sample->dataLength = data_size;
	mch->cache_sample->data = (char*)gf_malloc(sizeof(char)*data_size);
	memcpy(mch->cache_sample->data, data, sizeof(char)*data_size);
	return GF_OK;
}
static GF_Err ISOW_ServiceCommand(GF_StreamingCache *mc, GF_NetworkCommand *com)
{
	ISOMReader *cache = (ISOMReader *)mc->priv;
	if (!cache->mov || !cache->service) return GF_BAD_PARAM;

	return GF_OK;
}
static GF_Err ISOW_ChannelGetSLP(GF_StreamingCache *mc, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	ISOMReader *cache = (ISOMReader *)mc->priv;
	if (!cache->mov || !cache->service) return GF_BAD_PARAM;

	return GF_OK;
}
static GF_Err ISOW_ChannelReleaseSLP(GF_StreamingCache *mc, LPNETCHANNEL channel)
{
	ISOMReader *cache = (ISOMReader *)mc->priv;
	if (!cache->mov || !cache->service) return GF_BAD_PARAM;

	return GF_OK;
}

GF_BaseInterface *isow_load_cache()
{
	ISOMReader *cache;
	GF_StreamingCache *plug;
	GF_SAFEALLOC(plug, GF_StreamingCache);
	GF_REGISTER_MODULE_INTERFACE(plug, GF_STREAMING_MEDIA_CACHE, "GPAC IsoMedia Cache", "gpac distribution")

	plug->Open = ISOW_Open;
	plug->Close = ISOW_Close;
	plug->Write = ISOW_Write;
	plug->ChannelGetSLP = ISOW_ChannelGetSLP;
	plug->ChannelReleaseSLP = ISOW_ChannelReleaseSLP;
	plug->ServiceCommand = ISOW_ServiceCommand;

	GF_SAFEALLOC(cache, ISOMReader);
	cache->channels = gf_list_new();
	plug->priv = cache;
	return (GF_BaseInterface *) plug;
}

void isow_delete_cache(GF_BaseInterface *bi)
{
	GF_StreamingCache *mc = (GF_StreamingCache*) bi;
	ISOMReader *cache = (ISOMReader *)mc->priv;
	gf_list_del(cache->channels);
	gf_free(cache);
	gf_free(bi);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

