/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / ISOBMFF reader filter
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

#include "isoffin.h"

#ifndef GPAC_DISABLE_ISOM

#include <gpac/ismacryp.h>

ISOMChannel *isor_get_channel(ISOMReader *reader, GF_FilterPid *pid)
{
	u32 i=0;
	ISOMChannel *ch;
	while ((ch = (ISOMChannel *)gf_list_enum(reader->channels, &i))) {
		if (ch->pid == pid) return ch;
	}
	return NULL;
}

static void isor_delete_channel(ISOMReader *reader, ISOMChannel *ch)
{
	u32 i=0;
	ISOMChannel *ch2;
	while ((ch2 = (ISOMChannel *)gf_list_enum(reader->channels, &i))) {
		if (ch2 == ch) {
			isor_reset_reader(ch);
			gf_free(ch);
			gf_list_rem(reader->channels, i-1);
			return;
		}
	}
}

static GFINLINE Bool isor_is_local(const char *url)
{
	if (!strnicmp(url, "file://", 7)) return GF_TRUE;
	if (!strnicmp(url, "gmem://", 7)) return GF_TRUE;
	if (!strnicmp(url, "isobmff://", 10)) return GF_TRUE;
	if (strstr(url, "://")) return GF_FALSE;
	/*the rest is local (mounted on FS)*/
	return GF_TRUE;
}


static GF_Err isoffin_setup(GF_Filter *filter, ISOMReader *read)
{
	char szURL[2048];
	char *tmp, *src;
	GF_Err e;
	u64 start_range, end_range;
	if (!read) return GF_SERVICE_ERROR;

	if (read->pid) {
		const GF_PropertyValue *prop;
		prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILEPATH);
		assert(prop);
		src = prop->value.string;
	} else {
		src = read->src;
	}
	if (!src)  return GF_SERVICE_ERROR;

	read->filter = filter;
	read->channels = gf_list_new();

	strcpy(szURL, src);
	tmp = strrchr(szURL, '.');
	if (tmp) {
		tmp = strchr(tmp, '#');
		if (tmp) {
			if (!strnicmp(tmp, "#trackID=", 9)) {
				read->play_only_track_id = atoi(tmp+9);
			} else {
				read->play_only_track_id = atoi(tmp+1);
			}
			tmp[0] = 0;
		}
	}

	if (! isor_is_local(szURL)) {
		return GF_NOT_SUPPORTED;
	}
	start_range = end_range = 0;
#ifdef FILTER_FIXME
		if (plug->query_proxy) {
			GF_NetworkCommand param;
			param.command_type = GF_NET_SERVICE_QUERY_INIT_RANGE;
			if (read->input->query_proxy(read->input, &param)==GF_OK) {
				start_range = param.url_query.start_range;
				end_range = param.url_query.end_range;
			}
		}
#endif
	e = gf_isom_open_progressive(szURL, start_range, end_range, &read->mov, &read->missing_bytes);

	if (e == GF_ISOM_INCOMPLETE_FILE) {
		read->moov_not_loaded = GF_TRUE;
		return GF_OK;
	}

	if (e != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[IsoMedia] error while opening %s, error=%s\n", szURL,gf_error_to_string(e)));
		gf_filter_setup_failure(filter, e);
		return e;
	}
	read->frag_type = gf_isom_is_fragmented(read->mov) ? 1 : 0;

	read->time_scale = gf_isom_get_timescale(read->mov);

	isor_declare_objects(read);

	return GF_OK;
}

static void isoffin_disconnect(ISOMReader *read)
{
	read->disconnected = GF_TRUE;
	while (gf_list_count(read->channels)) {
		ISOMChannel *ch = (ISOMChannel *)gf_list_get(read->channels, 0);
		gf_list_rem(read->channels, 0);
		isor_reset_reader(ch);
		gf_free(ch);
	}

	if (read->mov) gf_isom_close(read->mov);
	read->mov = NULL;

	read->pid = NULL;
}

GF_Err isoffin_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	ISOMReader *read = gf_filter_get_udta(filter);
	if (is_remove) {
		isoffin_disconnect(read);
		return GF_OK;
	}
	//TODO, needed for dash multilayer
	if (read->pid) return GF_NOT_SUPPORTED;

	//we must have a file path for now
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
	if (!prop || ! prop->value.string) {
		return GF_NOT_SUPPORTED;
	}

	read->pid = pid;
	return isoffin_setup(filter, read);
}

GF_Err isoffin_initialize(GF_Filter *filter)
{
	char szURL[2048];
	char *tmp;
	ISOMReader *read = gf_filter_get_udta(filter);
	if (read && read->src)
		return isoffin_setup(filter, read);

	return GF_OK;
}


static GF_Err isoffin_finalize(GF_Filter *filter)
{
	GF_Err reply;
	ISOMReader *read = (ISOMReader *) gf_filter_get_udta(filter);

	read->disconnected = GF_TRUE;

	while (gf_list_count(read->channels)) {
		ISOMChannel *ch = (ISOMChannel *)gf_list_get(read->channels, 0);
		gf_list_rem(read->channels, 0);
		isor_reset_reader(ch);
		gf_free(ch);
	}
	gf_list_del(read->channels);

#ifdef FILTER_FIXME
	if (read->dnload) gf_service_download_del(read->dnload);
	read->dnload = NULL;
#endif

	if (read->mov) gf_isom_close(read->mov);
	read->mov = NULL;

#ifdef FILTER_FIXME
	if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
		send_proxy_command(read, GF_TRUE, GF_FALSE, reply, NULL, NULL);
	} else {
		gf_service_disconnect_ack(read->service, NULL, reply);
	}
#endif

}

static Bool check_mpeg4_systems(GF_Filter *filter, GF_ISOFile *mov)
{
	char *opt, *bname, *br, *next;
	u32 i, count, brand;
	Bool has_mpeg4;
	GF_Err e;
	e = gf_isom_get_brand_info(mov, &brand, &i, &count);
	/*no brand == MP4 v1*/
	if (e || !brand) return GF_TRUE;

	has_mpeg4 = GF_FALSE;
	if ((brand==GF_ISOM_BRAND_MP41) || (brand==GF_ISOM_BRAND_MP42)) has_mpeg4 = GF_TRUE;

#ifdef FILTER_FIXME
	opt = (char*) gf_modules_get_option((GF_BaseInterface *)plug, "ISOReader", "IgnoreMPEG-4ForBrands");
	if (!opt) {
		gf_modules_set_option((GF_BaseInterface *)plug, "ISOReader", "IgnoreMPEG-4ForBrands", "nd*");
		opt = (char*) gf_modules_get_option((GF_BaseInterface *)plug, "ISOReader", "IgnoreMPEG-4ForBrands");
	}
#endif

	for (i=0; i<count; i++) {
		e = gf_isom_get_alternate_brand(mov, i+1, &brand);
		if (e) return GF_FALSE;
		if ((brand==GF_ISOM_BRAND_MP41) || (brand==GF_ISOM_BRAND_MP42)) {
			has_mpeg4 = GF_TRUE;
			continue;
		}
		bname = (char*)gf_4cc_to_str(brand);
		br = opt;
		while (br) {
			Bool ignore = GF_FALSE;
			u32 orig_len, len;
			next = strchr(br, ' ');
			if (next) next[0] = 0;
			len = orig_len = (u32) strlen(br);

			while (len) {
				if (br[len-1]=='*') {
					br[len-1]=0;
					len--;
				} else {
					break;
				}
			}
			/*ignor all brands*/
			if (!len) ignore = GF_TRUE;
			else if (!strncmp(bname, br, len)) ignore = GF_TRUE;

			while (len<orig_len) {
				br[len] = '*';
				len++;
			}
			if (next) {
				next[0] = ' ';
				br = next + 1;
			} else {
				br = NULL;
			}
			if (ignore) return GF_FALSE;
		}
	}
	return has_mpeg4;
}

static u32 get_track_id(GF_ISOFile *mov, u32 media_type, u32 idx)
{
	u32 i, count, cur;
	cur=0;
	count = gf_isom_get_track_count(mov);
	for (i=0; i<count; i++) {
		if (gf_isom_get_media_type(mov, i+1) != media_type) continue;
		if (!idx) return gf_isom_get_track_id(mov, i+1);
		cur++;
		if (cur==idx) return gf_isom_get_track_id(mov, i+1);
	}
	return GF_FALSE;
}


void isor_send_cenc_config(ISOMChannel *ch)
{
#ifdef FILTER_FIXME
	GF_NetworkCommand com;
	u32 i;

	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.base.on_channel = ch->channel;
	com.command_type = GF_NET_CHAN_DRM_CFG;
	ch->is_encrypted = GF_TRUE;

	gf_isom_get_cenc_info(ch->owner->mov, ch->track, 1, NULL, &com.drm_cfg.scheme_type, &com.drm_cfg.scheme_version, NULL);

	com.drm_cfg.PSSH_count = gf_isom_get_pssh_count(ch->owner->mov);
	com.drm_cfg.PSSHs = gf_malloc(sizeof(GF_NetComDRMConfigPSSH)*(com.drm_cfg.PSSH_count) );

	/*fill PSSH in the structure. We will free it in CENC_Setup*/
	for (i=0; i<com.drm_cfg.PSSH_count; i++) {
		GF_NetComDRMConfigPSSH *pssh = &com.drm_cfg.PSSHs[i];
		gf_isom_get_pssh_info(ch->owner->mov, i+1, pssh->SystemID, &pssh->KID_count, (const bin128 **) & pssh->KIDs, (const u8 **) &pssh->private_data, &pssh->private_data_size);
	}
	//fixme - check MSE and EME
#if 0
	if (read->input->query_proxy && read->input->proxy_udta) {
		read->input->query_proxy(read->input, &com);
	} else
#endif
		gf_service_command(ch->owner->service, &com, GF_OK);
	//free our PSSH
	if (com.drm_cfg.PSSHs) gf_free(com.drm_cfg.PSSHs);
#endif
}


ISOMChannel *ISOR_CreateChannel(ISOMReader *read, GF_FilterPid *pid, u32 track)
{
	ISOMChannel *ch;
	Bool is_esd_url;
	GF_Err e;


	if (!read->mov) return NULL;

	GF_SAFEALLOC(ch, ISOMChannel);
	if (!ch) {
		return NULL;
	}
	ch->owner = read;
	ch->pid = pid;
//	ch->is_playing = GF_TRUE;
	ch->to_init = GF_TRUE;
	gf_list_add(read->channels, ch);
	ch->track = track;
	ch->track_id = gf_isom_get_track_id(read->mov, ch->track);
	switch (gf_isom_get_media_type(ch->owner->mov, ch->track)) {
	case GF_ISOM_MEDIA_OCR:
		ch->streamType = GF_STREAM_OCR;
		break;
	case GF_ISOM_MEDIA_SCENE:
		ch->streamType = GF_STREAM_SCENE;
		break;
	case GF_ISOM_MEDIA_VISUAL:
		gf_isom_get_reference(ch->owner->mov, ch->track, GF_ISOM_REF_BASE, 1, &ch->base_track);
		//use base track only if avc/svc or hevc/lhvc. If avc+lhvc we need different rules
		if ( gf_isom_get_avc_svc_type(ch->owner->mov, ch->base_track, 1) == GF_ISOM_AVCTYPE_AVC_ONLY) {
			if ( gf_isom_get_hevc_lhvc_type(ch->owner->mov, ch->track, 1) >= GF_ISOM_HEVCTYPE_HEVC_ONLY) {
				ch->base_track=0;
			}
		}
		
		ch->next_track = 0;
		/*in scalable mode add SPS/PPS in-band*/
		ch->nalu_extract_mode = GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG /*| GF_ISOM_NALU_EXTRACT_ANNEXB_FLAG*/;
		gf_isom_set_nalu_extract_mode(ch->owner->mov, ch->track, ch->nalu_extract_mode);
		break;
	}

	ch->has_edit_list = gf_isom_get_edit_list_type(ch->owner->mov, ch->track, &ch->dts_offset) ? GF_TRUE : GF_FALSE;
	ch->has_rap = (gf_isom_has_sync_points(ch->owner->mov, ch->track)==1) ? GF_TRUE : GF_FALSE;
	ch->time_scale = gf_isom_get_media_timescale(ch->owner->mov, ch->track);

	return ch;

#ifdef FILTER_FIXME

exit:
	if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
		send_proxy_command(read, GF_FALSE, GF_FALSE, e, NULL, channel);
	} else {
		gf_service_connect_ack(read->service, channel, e);
	}
	/*if esd url reconfig SL layer*/
	if (!e && is_esd_url) {
		GF_ESD *esd;
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.on_channel = channel;
		com.command_type = GF_NET_CHAN_RECONFIG;
		esd = gf_isom_get_esd(read->mov, ch->track, 1);
		if (esd) {
			memcpy(&com.cfg.sl_config, esd->slConfig, sizeof(GF_SLConfig));
			gf_odf_desc_del((GF_Descriptor *)esd);
		} else {
			com.cfg.sl_config.tag = GF_ODF_SLC_TAG;
			com.cfg.sl_config.timestampLength = 32;
			com.cfg.sl_config.timestampResolution = ch->time_scale;
			com.cfg.sl_config.useRandomAccessPointFlag = 1;
		}
		if (read->input->query_proxy && read->input->proxy_udta) {
			read->input->query_proxy(read->input, &com);
		} else {
			gf_service_command(read->service, &com, GF_OK);
		}
	}
	if (!e && track && gf_isom_is_track_encrypted(read->mov, track)) {
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.on_channel = channel;
		com.command_type = GF_NET_CHAN_DRM_CFG;
		ch->is_encrypted = GF_TRUE;
		if (gf_isom_is_ismacryp_media(read->mov, track, 1)) {
			gf_isom_get_ismacryp_info(read->mov, track, 1, NULL, &com.drm_cfg.scheme_type, &com.drm_cfg.scheme_version, &com.drm_cfg.scheme_uri, &com.drm_cfg.kms_uri, NULL, NULL, NULL);
			if (read->input->query_proxy && read->input->proxy_udta) {
				read->input->query_proxy(read->input, &com);
			} else {
				gf_service_command(read->service, &com, GF_OK);
			}
		} else if (gf_isom_is_omadrm_media(read->mov, track, 1)) {
			gf_isom_get_omadrm_info(read->mov, track, 1, NULL, &com.drm_cfg.scheme_type, &com.drm_cfg.scheme_version, &com.drm_cfg.contentID, &com.drm_cfg.kms_uri, &com.drm_cfg.oma_drm_textual_headers, &com.drm_cfg.oma_drm_textual_headers_len, NULL, &com.drm_cfg.oma_drm_crypt_type, NULL, NULL, NULL);

			gf_media_get_file_hash(gf_isom_get_filename(read->mov), com.drm_cfg.hash);
			if (read->input->query_proxy && read->input->proxy_udta) {
				read->input->query_proxy(read->input, &com);
			} else {
				gf_service_command(read->service, &com, GF_OK);
			}
		} else if (gf_isom_is_cenc_media(read->mov, track, 1)) {
			ch->is_cenc = GF_TRUE;
			isor_send_cenc_config(ch);
		}
	}
	return e;
#endif

}

#ifdef FILTER_FIXME

GF_Err ISOR_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	ISOMChannel *ch;
	GF_Err e;
	ISOMReader *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;
	if (!read->mov) return GF_SERVICE_ERROR;

	gf_mx_p(read->segment_mutex);
	e = GF_OK;
	ch = isor_get_channel(read, channel);
	assert(ch);
	if (!ch) {
		e = GF_STREAM_NOT_FOUND;
		goto exit;
	}
	/*signal the service is broken but still process the delete*/
	isor_delete_channel(read, ch);
	assert(!isor_get_channel(read, channel));

exit:
	if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
		send_proxy_command(read, 1, 0, e, NULL, channel);
	} else {
		gf_service_disconnect_ack(read->service, channel, e);
	}
	gf_mx_v(read->segment_mutex);
	return e;
}

GF_Err ISOR_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	ISOMChannel *ch;
	ISOMReader *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	/*cannot read native SL-PDUs*/
	if (!out_sl_hdr) return GF_NOT_SUPPORTED;
	read = (ISOMReader *) plug->priv;
	if (!read->mov) return GF_SERVICE_ERROR;

	*out_data_ptr = NULL;
	*out_data_size = 0;
	*sl_compressed = GF_FALSE;
	*out_reception_status = GF_OK;
	*is_new_data = GF_FALSE;
	ch = isor_get_channel(read, channel);
	if (!ch) return GF_STREAM_NOT_FOUND;
	if (!ch->is_playing) return GF_OK;

	*is_new_data = GF_FALSE;
	if (!ch->sample) {
		/*get sample*/
		gf_mx_p(read->segment_mutex);
		isor_reader_get_sample(ch);
		gf_mx_v(read->segment_mutex);
		*is_new_data = ch->sample ? GF_TRUE : GF_FALSE;
	}

	if (ch->sample) {
		*out_data_ptr = ch->sample->data;
		*out_data_size = ch->sample->dataLength;
		*out_sl_hdr = ch->current_slh;
	}
	*out_reception_status = ch->last_state;
	if (read->waiting_for_data)
		*out_reception_status = GF_BUFFER_TOO_SMALL;

	return GF_OK;
}

GF_Err ISOR_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	ISOMChannel *ch;
	ISOMReader *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;
	if (!read->mov) return GF_SERVICE_ERROR;
	ch = isor_get_channel(read, channel);
	if (!ch) return GF_STREAM_NOT_FOUND;
	if (!ch->is_playing) return GF_SERVICE_ERROR;

	if (ch->sample) {
		isor_reader_release_sample(ch);
		/*release sample*/
	}
	return GF_OK;
}

/*switch channel quality. Return next channel or current channel if error*/
static
u32 gf_channel_switch_quality(ISOMChannel *ch, GF_ISOFile *the_file, Bool switch_up)
{
	u32 i, count, next_track, trackID, cur_track;
	s32 ref_count;

	cur_track = ch->next_track ? ch->next_track : ch->track;
	count = gf_isom_get_track_count(the_file);
	trackID = gf_isom_get_track_id(the_file, cur_track);
	next_track = 0;

	if (switch_up)
	{
		for (i = 0; i < count; i++)
		{
			ref_count = gf_isom_get_reference_count(the_file, i+1, GF_ISOM_REF_SCAL);
			if (ref_count < 0)
				return cur_track; //error
			else if (ref_count == 0)
				continue;
			/*next track is the one that has the last reference of type GF_ISOM_REF_SCAL refers to this current track*/
			else if ((u32)ref_count == gf_isom_has_track_reference(the_file, i+1, GF_ISOM_REF_SCAL, trackID))
			{
				next_track = i+1;
				break;
			}
		}
		/*this is the highest quality*/
		if (!next_track)
			return cur_track;
	}
	else
	{
		if (cur_track == ch->base_track)
			return cur_track;
		ref_count = gf_isom_get_reference_count(the_file, cur_track, GF_ISOM_REF_SCAL);
		if (ref_count <= 0)
			return cur_track;
		gf_isom_get_reference(the_file, cur_track, GF_ISOM_REF_SCAL, ref_count, &next_track);
		if (!next_track)
			return cur_track;
	}

	/*in scalable mode add SPS/PPS in-band*/
	gf_isom_set_nalu_extract_mode(the_file, next_track, ch->nalu_extract_mode);

	return next_track;
}
#endif

static Bool isoffin_process_event(GF_Filter *filter, GF_FilterEvent *com)
{
	Double track_dur, media_dur;
	u32 count, i;
	ISOMChannel *ch;
	ISOMReader *read = gf_filter_get_udta(filter);

	if (!read || read->disconnected) return GF_FALSE;

#ifdef FILTER_FIXME
	if (com->type==GF_NET_SERVICE_INFO) {
		u32 tag_len;
		const char *tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_NAME, &tag, &tag_len)==GF_OK) com->info.name = tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_ARTIST, &tag, &tag_len)==GF_OK) com->info.artist = tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_ALBUM, &tag, &tag_len)==GF_OK) com->info.album = tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_COMMENT, &tag, &tag_len)==GF_OK) com->info.comment = tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_TRACK, &tag, &tag_len)==GF_OK) {
			com->info.track_info = (((tag[2]<<8)|tag[3]) << 16) | ((tag[4]<<8)|tag[5]);
		}
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_COMPOSER, &tag, &tag_len)==GF_OK) com->info.composer = tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_WRITER, &tag, &tag_len)==GF_OK) com->info.writer = tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_GENRE, &tag, &tag_len)==GF_OK) {
			if (tag[0]) {
				com->info.genre = 0;
			} else {
				com->info.genre = (tag[0]<<8) | tag[1];
			}
		}
		return GF_OK;
	}
	if (com->type==GF_NET_SERVICE_HAS_AUDIO) {
		u32 i, count;
		count = gf_isom_get_track_count(read->mov);
		for (i=0; i<count; i++) {
			if (gf_isom_get_media_type(read->mov, i+1) == GF_ISOM_MEDIA_AUDIO) return GF_OK;
		}
		return GF_NOT_SUPPORTED;
	}

	if (com->type == GF_NET_SERVICE_QUALITY_SWITCH)
	{
		count = gf_list_count(read->channels);
		for (i = 0; i < count; i++)
		{
			ch = (ISOMChannel *)gf_list_get(read->channels, i);
			if (ch->base_track && gf_isom_needs_layer_reconstruction(read->mov)) {
				ch->next_track = gf_channel_switch_quality(ch, read->mov, com->switch_quality.up);
			}
		}
		return GF_OK;
	}
	if (com->type == GF_NET_SERVICE_PROXY_DATA_RECEIVE) {
		isor_flush_data(read, 1, com->proxy_data.is_chunk);
		return GF_OK;
	}
	if (com->command_type == GF_NET_SERVICE_CAN_REVERSE_PLAYBACK)
		return GF_OK;

#endif

	if (!com->base.on_pid) return GF_FALSE;

	ch = isor_get_channel(read, com->base.on_pid);
	if (!ch)
		return GF_FALSE;

	switch (com->base.type) {
#ifdef FILTER_FIXME
	case GF_NET_CHAN_SET_PADDING:
		if (!ch->track) return GF_OK;
		gf_isom_set_sample_padding(read->mov, ch->track, com->pad.padding_bytes);
		return GF_OK;
	case GF_NET_CHAN_SET_PULL:
		//we don't pull in DASH base services, we flush as soon as we have a complete segment
#ifndef DASH_USE_PULL
		if (read->input->proxy_udta && !read->input->proxy_type)
			return GF_NOT_SUPPORTED;
#endif

		ch->is_pulling = 1;
		return GF_OK;
	case GF_NET_CHAN_INTERACTIVE:
		return GF_OK;
	case GF_NET_CHAN_BUFFER:
		//dash or HTTP, do rebuffer if not disabled
		if (plug->query_proxy) {
		} else if (read->dnload) {
			ch->buffer_min = com->buffer.min;
			ch->buffer_max = com->buffer.max;
		} else {
			com->buffer.max = com->buffer.min = 0;
		}
		return GF_OK;

	case GF_NET_CHAN_NALU_MODE:
		ch->nalu_extract_mode = GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG;

		//when this is set, we work in real scalable (eg N streams reassembled by the player) so only extract the layer. This wll need refinements if we plan to support
		//several scalable layers ...
		if (com->nalu_mode.extract_mode>=1) {
			if (com->nalu_mode.extract_mode==2) {
				ch->disable_seek = 1;
			}
			ch->nalu_extract_mode |= GF_ISOM_NALU_EXTRACT_ANNEXB_FLAG | GF_ISOM_NALU_EXTRACT_VDRD_FLAG | GF_ISOM_NALU_EXTRACT_LAYER_ONLY;
		}
		gf_isom_set_nalu_extract_mode(ch->owner->mov, ch->track, ch->nalu_extract_mode);
		break;
#endif

	case GF_FEVT_PLAY:
		isor_reset_reader(ch);
		ch->speed = com->play.speed;
		read->reset_frag_state = 1;
		if (read->frag_type)
			read->frag_type = 1;

		ch->start = ch->end = 0;
		if (com->play.speed>0) {
			Double t;
			if (com->play.start_range>=0) {
				t = com->play.start_range;
				t *= ch->time_scale;
				ch->start = (u64) t;
			}
			if (com->play.end_range >= com->play.start_range) {
				t = com->play.end_range;
				t *= ch->time_scale;
				ch->end = (u64) t;
			}
		} else if (com->play.speed<0) {
			Double end = com->play.end_range;
			if (end==-1) end = 0;
			ch->start = (u64) (s64) (com->play.start_range * ch->time_scale);
			if (end <= com->play.start_range)
				ch->end = (u64) (s64) (end  * ch->time_scale);
		}
		ch->is_playing = 1;
#ifdef FILTER_FIXME
		if (com->play.dash_segment_switch) ch->wait_for_segment_switch = 1;
#endif
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[IsoMedia] Starting channel playback "LLD" to "LLD" (%g to %g)\n", ch->start, ch->end, com->play.start_range, com->play.end_range));

#ifdef FILTER_FIXME
		//and check buffer level on play request
		isor_check_buffer_level(read);
#endif
		if (!read->nb_playing)
			gf_isom_reset_seq_num(read->mov);

		//always request a process task upon a play
		gf_filter_post_process_task(read->filter);

		read->nb_playing++;
		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		if (read->nb_playing) read->nb_playing--;
		isor_reset_reader(ch);
		//cancel event
		return GF_FALSE;

	case GF_FEVT_SET_SPEED:
		ch->speed = com->play.speed;
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

#ifdef FILTER_FIXME
static Bool ISOR_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	char szURL[2048], *sep;
	ISOMReader *read = (ISOMReader *)plug->priv;
	const char *this_url = gf_service_get_url(read->service);
	if (!this_url || !url) return GF_FALSE;

	if (!strcmp(this_url, url)) return GF_TRUE;

	strcpy(szURL, this_url);
	sep = strrchr(szURL, '#');
	if (sep) sep[0] = 0;

	/*direct addressing in service*/
	if (url[0] == '#') return GF_TRUE;
	if (strnicmp(szURL, url, sizeof(char)*strlen(szURL))) return GF_FALSE;
	return GF_TRUE;
}

#endif


static GF_Err isoffin_process(GF_Filter *filter)
{
	ISOMReader *read = gf_filter_get_udta(filter);
	u32 i, count = gf_list_count(read->channels);
	Bool is_active = GF_FALSE;

	if (read->moov_not_loaded) {
		read->moov_not_loaded = GF_FALSE;
		return isoffin_setup(filter, read);
	}
	for (i=0; i<count; i++) {
		char *data;
		ISOMChannel *ch;
		ch = gf_list_get(read->channels, i);
		if (!ch->is_playing) continue;
		is_active = GF_TRUE;

		while (! gf_filter_pid_would_block(ch->pid) ) {

			isor_reader_get_sample(ch);
			if (ch->sample) {
				u32 sample_dur;
				GF_FilterPacket *pck;
				pck = gf_filter_pck_new_alloc(ch->pid, ch->sample->dataLength, &data);
				memcpy(data, ch->sample->data, ch->sample->dataLength);

				gf_filter_pck_set_dts(pck, ch->current_slh.decodingTimeStamp);
				gf_filter_pck_set_cts(pck, ch->current_slh.compositionTimeStamp);
				gf_filter_pck_set_sap(pck, ch->sample->IsRAP);
				sample_dur = gf_isom_get_sample_duration(read->mov, ch->track, ch->sample_num);
				gf_filter_pck_set_duration(pck, sample_dur);
				gf_filter_pck_set_seek_flag(pck, ch->current_slh.seekFlag);

				gf_filter_pck_send(pck);
				isor_reader_release_sample(ch);
			} else if (ch->last_state==GF_EOS) {
				gf_filter_pid_set_eos(ch->pid);
				ch->is_playing=0;
				break;
			} else {
				break;
			}
		}
	}
	return is_active ? GF_OK : GF_EOS;
}

GF_FilterProbeScore isoffin_probe_url(const char *url, const char *mime_type)
{
	if (!strnicmp(url, "rtsp://", 7)) return GF_FPROBE_NOT_SUPPORTED;

	if (gf_isom_probe_file(url)) {
		return GF_FPROBE_SUPPORTED;
	}
	return GF_FPROBE_NOT_SUPPORTED;
}


#define OFFS(_n)	#_n, offsetof(ISOMReader, _n)

static const GF_FilterArgs ISOFFInArgs[] =
{
	{ OFFS(src), "location of source content", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{}
};

static const GF_FilterCapability ISOFFInInputs[] =
{
	{.code=GF_PROP_PID_MIME, PROP_STRING("application/x-isomedia"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("application/mp4"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("video/mp4"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("audio/mp4"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("video/3gpp"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("audio/3gpp"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("video/3gp2"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("audio/3gp2"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("video/iso.segment"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("audio/iso.segment"), .start=GF_TRUE},

	{.code=GF_PROP_PID_FILE_EXT, PROP_STRING("mp4|mpg4|m4a|m4i|3gp|3gpp|3g2|3gp2|iso|m4s"), .start=GF_TRUE},
};

static const GF_FilterCapability ISOFFInOutputs[] =
{
	{.code=GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO)},

	{.code=GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_VISUAL), .start=GF_TRUE},
	{}
};

GF_FilterRegister ISOFFInRegister = {
	.name = "mp4in",
	.description = "ISOFF Demuxer",
	.private_size = sizeof(ISOMReader),
	.args = ISOFFInArgs,
	.initialize = isoffin_initialize,
	.finalize = isoffin_finalize,
	.process = isoffin_process,
	.configure_pid = isoffin_configure_pid,
	.update_arg = NULL,
	.input_caps = ISOFFInInputs,
	.output_caps = ISOFFInOutputs,
	.process_event = isoffin_process_event
//	.probe_url = isoffin_probe_url
};


#endif /*GPAC_DISABLE_ISOM*/

const GF_FilterRegister *isoffin_register(GF_FilterSession *session)
{
#ifdef GPAC_DISABLE_ISOM
	return NULL;
#else
	return &ISOFFInRegister;
#endif /*GPAC_DISABLE_ISOM*/
}

