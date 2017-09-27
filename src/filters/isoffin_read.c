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
	const GF_PropertyValue *prop;
	u64 start_range, end_range;
	if (!read) return GF_SERVICE_ERROR;

	if (read->pid) {
		prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILEPATH);
		assert(prop);
		src = prop->value.string;
	} else {
		src = read->src;
	}
	if (!src)  return GF_SERVICE_ERROR;

	read->src_crc = gf_crc_32(src, strlen(src));

	strcpy(szURL, src);
	tmp = strrchr(szURL, '.');
	if (tmp) {
		Bool truncate = GF_TRUE;
		tmp = strchr(tmp, '#');
		if (!tmp && read->pid) {
			prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_URL);
			if (prop && prop->value.string) {
				tmp = strrchr(prop->value.string, '.');
				if (tmp) tmp = strchr(tmp, '#');
				truncate = GF_FALSE;
			}
		}
		if (tmp) {
			if (!strnicmp(tmp, "#audio", 6)) {
				read->play_only_first_media = GF_ISOM_MEDIA_AUDIO;
			} else if (!strnicmp(tmp, "#video", 6)) {
				read->play_only_first_media = GF_ISOM_MEDIA_VISUAL;
			} else if (!strnicmp(tmp, "#text", 5)) {
				read->play_only_first_media = GF_ISOM_MEDIA_TEXT;
			} else if (!strnicmp(tmp, "#trackID=", 9)) {
				read->play_only_track_id = atoi(tmp+9);
			} else if (!strnicmp(tmp, "#ID=", 4)) {
				read->play_only_track_id = atoi(tmp+4);
			} else {
				read->play_only_track_id = atoi(tmp+1);
			}
			if (truncate) tmp[0] = 0;
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
	//we must have a file path for now
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
	if (!prop || ! prop->value.string) {
		return GF_NOT_SUPPORTED;
	}
	if (read->pid) {
		//TODO, will be needed for dash multilayer or playlists
		if (read->src_crc != gf_crc_32(prop->value.string, strlen(prop->value.string)))
			return GF_NOT_SUPPORTED;

		return GF_OK;
	}

	read->pid = pid;

	prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILE_CACHED);
	if (prop && prop->value.boolean) read->input_loaded = GF_TRUE;

	return isoffin_setup(filter, read);
}

GF_Err isoffin_initialize(GF_Filter *filter)
{
	ISOMReader *read = gf_filter_get_udta(filter);

	read->filter = filter;
	read->channels = gf_list_new();

	if (read->src) {
		read->input_loaded = GF_TRUE;
		return isoffin_setup(filter, read);
	}

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

	if (read->mov) gf_isom_close(read->mov);
	read->mov = NULL;
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

		if (!read->nb_playing)
			gf_isom_reset_seq_num(read->mov);

		if (read->is_partial_download) read->input_loaded = GF_FALSE;

		if (!read->nb_playing && read->pid && !read->input_loaded) {
			GF_FilterEvent fevt;
			u64 max_offset = GF_FILTER_NO_BO;
			u32 i, count = gf_list_count(read->channels);
			for (i=0; i< count; i++) {
				u32 mode, sample_desc_index, sample_num;
				u64 data_offset;
				GF_ISOSample *sample;
				GF_Err e;
				u64 time;
				ch = gf_list_get(read->channels, i);
				mode = ch->disable_seek ? GF_ISOM_SEARCH_BACKWARD : GF_ISOM_SEARCH_SYNC_BACKWARD;
				time = com->play.start_range;
				time *= ch->time_scale;

				/*take care of seeking out of the track range*/
				if (!read->frag_type && (ch->duration < time)) {
					e = gf_isom_get_sample_for_movie_time(read->mov, ch->track, ch->duration, 	&sample_desc_index, mode, NULL, &sample_num, &data_offset);
				} else {
					e = gf_isom_get_sample_for_movie_time(read->mov, ch->track, time, &sample_desc_index, mode, NULL, &sample_num, &data_offset);
				}
				if ((e == GF_OK) && (data_offset<max_offset))
					max_offset = data_offset;
			}

			if ((com->play.start_range || read->is_partial_download)  && (max_offset != GF_FILTER_NO_BO) ) {

				//send a seek request
				read->is_partial_download = GF_TRUE;
				read->wait_for_source = GF_TRUE;

				//post a seek from 0 - TODO we could build a map of byte offsets
				GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, read->pid);
				fevt.seek.start_offset = max_offset;
				gf_filter_pid_send_event(read->pid, &fevt);
				gf_isom_set_byte_offset(read->mov, max_offset);

			}
		}
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

static GF_Err isoffin_process(GF_Filter *filter)
{
	ISOMReader *read = gf_filter_get_udta(filter);
	u32 i, count = gf_list_count(read->channels);
	Bool is_active = GF_FALSE;

	if (read->pid) {
		while (1) {
			GF_FilterPacket *pck = gf_filter_pid_get_packet(read->pid);
			if (!pck) {
				//we issued a seek, wait for the first packet to be received before fetching channels
				//otherwise we could end up reading from the wrong cache
				if (read->wait_for_source) {
					//something went wrong during the seek request
					if (gf_filter_pid_is_eos(read->pid))
						return GF_EOS;
					return GF_OK;
				}
				break;
			}
			read->wait_for_source = GF_FALSE;
			gf_filter_pid_drop_packet(read->pid);
		}
		if (gf_filter_pid_is_eos(read->pid))
			read->input_loaded = GF_TRUE;
	}
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
				assert(pck);
				
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
	{}
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

