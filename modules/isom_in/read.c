/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / IsoMedia reader module
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
#include <gpac/ismacryp.h>

ISOMChannel *isor_get_channel(ISOMReader *reader, LPNETCHANNEL channel)
{
	u32 i=0;
	ISOMChannel *ch;
	while ((ch = (ISOMChannel *)gf_list_enum(reader->channels, &i))) {
		if (ch->channel == channel) return ch;
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
			free(ch);
			gf_list_rem(reader->channels, i-1);
			return;
		}
	}
}

static GFINLINE Bool isor_is_local(const char *url)
{
	if (!strnicmp(url, "file://", 7)) return 1;
	if (strstr(url, "://")) return 0;
	/*the rest is local (mounted on FS)*/
	return 1;
}

Bool ISOR_CanHandleURL(GF_InputService *plug, const char *url)
{
	char *ext;
	if (!strnicmp(url, "rtsp://", 7)) return 0;
	ext = strrchr(url, '.');
	if (!ext) return 0;

	if (gf_term_check_extension(plug, "video/mp4", "mp4 mpg4", "MPEG-4 Movies", ext)) return 1;
	if (gf_term_check_extension(plug, "audio/mp4", "m4a mp4 mpg4", "MPEG-4 Music", ext)) return 1;
	if (gf_term_check_extension(plug, "application/mp4", "mp4 mpg4", "MPEG-4 Applications", ext)) return 1;
	if (gf_term_check_extension(plug, "video/3gpp", "3gp 3gpp", "3GPP/MMS Movies", ext)) return 1;
	if (gf_term_check_extension(plug, "audio/3gpp", "3gp 3gpp", "3GPP/MMS Music",ext)) return 1;
	if (gf_term_check_extension(plug, "video/3gpp2", "3g2 3gp2", "3GPP2/MMS Movies",ext)) return 1;
	if (gf_term_check_extension(plug, "audio/3gpp2", "3g2 3gp2", "3GPP2/MMS Music",ext)) return 1;

	if (gf_isom_probe_file(url)) {
		gf_term_check_extension(plug, "application/x-isomedia", ext+1, "IsoMedia Files", ext);
		return 1;
	}
	return 0;
}



void isor_net_io(void *cbk, GF_NETIO_Parameter *param)
{
	GF_Err e;
	u32 size = 0;
	char *local_name;
	ISOMReader *read = (ISOMReader *) cbk;
	
	/*handle service message*/
	gf_term_download_update_stats(read->dnload);

	if (param->msg_type==GF_NETIO_DATA_TRANSFERED) {
		e = GF_EOS;
	} else if (param->msg_type==GF_NETIO_DATA_EXCHANGE) {
		e = GF_OK;
		size = param->size;
	} else {
		e = param->error;
	}

	if (e<GF_OK) {
		/*error opening service*/
		if (!read->mov) gf_term_on_connect(read->service, NULL, e);
		return;
	}

	/*open file if not done yet (bad interleaving)*/
	if (e==GF_EOS) {
		const char *local_name;
		if (read->mov) return;
		local_name = gf_dm_sess_get_cache_name(read->dnload);
		if (!local_name) {
			gf_term_on_connect(read->service, NULL, GF_SERVICE_ERROR);
			return;
		}
		e = GF_OK;
		read->mov = gf_isom_open(local_name, GF_ISOM_OPEN_READ, NULL);
		if (!read->mov) e = gf_isom_last_error(NULL);
		else read->time_scale = gf_isom_get_timescale(read->mov);
		gf_term_on_connect(read->service, NULL, GF_OK);
		if (read->no_service_desc) isor_declare_objects(read);
	}
	
	if (!size) return;

	/*service is opened, nothing to do*/
	if (read->mov) return;

	/*try to open the service*/
	local_name = (char *)gf_dm_sess_get_cache_name(read->dnload);
	if (!local_name) {
		gf_term_on_connect(read->service, NULL, GF_SERVICE_ERROR);
		return;
	}

	/*not enogh data yet*/
	if (read->missing_bytes && (read->missing_bytes > size) ) {
		read->missing_bytes -= size;
		return;
	}
	
	e = gf_isom_open_progressive(local_name, &read->mov, &read->missing_bytes);
	switch (e) {
	case GF_ISOM_INCOMPLETE_FILE:
		return;
	case GF_OK:
		break;
	default:
		gf_term_on_connect(read->service, NULL, e);
		return;
	}
	
	/*ok let's go*/
	read->time_scale = gf_isom_get_timescale(read->mov);
	gf_term_on_connect(read->service, NULL, GF_OK);
	if (read->no_service_desc) isor_declare_objects(read);
}


void isor_setup_download(GF_InputService *plug, const char *url)
{
	ISOMReader *read = (ISOMReader *) plug->priv;
	read->dnload = gf_term_download_new(read->service, url, 0, isor_net_io, read);
	if (!read->dnload) gf_term_on_connect(read->service, NULL, GF_NOT_SUPPORTED);
	/*service confirm is done once IOD can be fetched*/
}



GF_Err ISOR_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	char szURL[2048];
	char *tmp;
	ISOMReader *read;
	if (!plug || !plug->priv || !serv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;

	read->service = serv;

	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;

	read->base_track_id = 0;
	strcpy(szURL, url);
	tmp = strrchr(szURL, '.');
	if (tmp) {
		tmp = strchr(tmp, '#');
		if (tmp) {
			if (!strnicmp(tmp, "#trackID=", 9)) {
				read->base_track_id = atoi(tmp+9);
			} else {
				read->base_track_id = atoi(tmp+1);
			}
			tmp[0] = 0;
		}
	}

	if (isor_is_local(szURL)) {
		if (!read->mov) read->mov = gf_isom_open(szURL, GF_ISOM_OPEN_READ, NULL);
		if (!read->mov) {
			gf_term_on_connect(serv, NULL, gf_isom_last_error(NULL));
			return GF_OK;
		}
		read->time_scale = gf_isom_get_timescale(read->mov);
		/*reply to user*/
		gf_term_on_connect(serv, NULL, GF_OK);
		if (read->no_service_desc) isor_declare_objects(read);
	} else {
		/*setup downloader*/
		isor_setup_download(plug, szURL);
	}
	return GF_OK;
}

GF_Err ISOR_CloseService(GF_InputService *plug)
{
	GF_Err reply;
	ISOMReader *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;
	reply = GF_OK;

	if (read->mov) gf_isom_close(read->mov);
	read->mov = NULL;

	while (gf_list_count(read->channels)) {
		ISOMChannel *ch = (ISOMChannel *)gf_list_get(read->channels, 0);
		gf_list_rem(read->channels, 0);
		isor_delete_channel(read, ch);
	}

	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;

	gf_term_on_disconnect(read->service, NULL, reply);
	return GF_OK;
}

static Bool check_mpeg4_systems(GF_InputService *plug, GF_ISOFile *mov)
{
	char *opt, *bname, *br, *next;
	u32 i, count, brand, has_mpeg4;
	GF_Err e;
	e = gf_isom_get_brand_info(mov, &brand, &i, &count);
	/*no brand == MP4 v1*/
	if (e || !brand) return 1;

	has_mpeg4 = 0;
	if ((brand==GF_ISOM_BRAND_MP41) || (brand==GF_ISOM_BRAND_MP42)) has_mpeg4 = 1;

	opt = (char*) gf_modules_get_option((GF_BaseInterface *)plug, "ISOReader", "IgnoreMPEG-4ForBrands");
	if (!opt) {
		gf_modules_set_option((GF_BaseInterface *)plug, "ISOReader", "IgnoreMPEG-4ForBrands", "nd*");
		opt = (char*) gf_modules_get_option((GF_BaseInterface *)plug, "ISOReader", "IgnoreMPEG-4ForBrands");
	}

	for (i=0; i<count; i++) {
		e = gf_isom_get_alternate_brand(mov, i+1, &brand);
		if (e) return 0;
		if ((brand==GF_ISOM_BRAND_MP41) || (brand==GF_ISOM_BRAND_MP42)) {
			has_mpeg4 = 1;
			continue;
		}
		bname = (char*)gf_4cc_to_str(brand);
		br = opt;
		while (br) {
			Bool ignore = 0;
			u32 orig_len, len;
			next = strchr(br, ' ');
			if (next) next[0] = 0;
			len = orig_len = strlen(br);

			while (len) {
				if (br[len-1]=='*') {
					br[len-1]=0;
					len--;
				} else {
					break;
				}
			}
			/*ignor all brands*/
			if (!len) ignore = 1;
			else if (!strncmp(bname, br, len)) ignore = 1;

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
			if (ignore) return 0;
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
	return 0;
}

/*fixme, this doesn't work properly with respect to @expect_type*/
static GF_Descriptor *ISOR_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	u32 count, nb_st, i, trackID;
	GF_ESD *esd;
	ISOMReader *read;
	GF_InitialObjectDescriptor *iod;
	if (!plug || !plug->priv) return NULL;
	read = (ISOMReader *) plug->priv;
	if (!read->mov) return NULL;

	/*no matter what always read text as TTUs*/
	gf_isom_text_set_streaming_mode(read->mov, 1);

	trackID = 0;
	if (!sub_url) {
		trackID = read->base_track_id;
		read->base_track_id = 0;
	} else {
		char *ext = strrchr(sub_url, '#');
		if (!ext) {
			trackID = 0;
		} else {
			if (!strnicmp(ext, "#trackID=", 9)) trackID = atoi(ext+9);
			else if (!stricmp(ext, "#video")) trackID = get_track_id(read->mov, GF_ISOM_MEDIA_VISUAL, 0);
			else if (!strnicmp(ext, "#video", 6)) {
				trackID = atoi(ext+6);
				trackID = get_track_id(read->mov, GF_ISOM_MEDIA_VISUAL, trackID);
			}
			else if (!stricmp(ext, "#audio")) trackID = get_track_id(read->mov, GF_ISOM_MEDIA_AUDIO, 0);
			else if (!strnicmp(ext, "#audio", 6)) {
				trackID = atoi(ext+6);
				trackID = get_track_id(read->mov, GF_ISOM_MEDIA_AUDIO, trackID);
			}
			else trackID = atoi(ext+1);

			/*if trackID is 0, assume this is a fragment identifier*/
		}
	}

	if (!trackID && (expect_type!=GF_MEDIA_OBJECT_SCENE) && (expect_type!=GF_MEDIA_OBJECT_UNDEF)) {
		for (i=0; i<gf_isom_get_track_count(read->mov); i++) {
			u32 type = gf_isom_get_media_type(read->mov, i+1);
			if (
				((type==GF_ISOM_MEDIA_VISUAL) && (expect_type==GF_MEDIA_OBJECT_VIDEO)) 
				|| ((type==GF_ISOM_MEDIA_AUDIO) && (expect_type==GF_MEDIA_OBJECT_AUDIO)) ) {
				trackID = gf_isom_get_track_id(read->mov, i+1);
				break;
			}

		}
	}
	if (trackID && (expect_type!=GF_MEDIA_OBJECT_SCENE) ) {
		u32 track = gf_isom_get_track_by_id(read->mov, trackID);
		if (!track) return NULL;
		esd = gf_media_map_esd(read->mov, track);
		esd->OCRESID = 0;
		iod = (GF_InitialObjectDescriptor *) gf_isom_get_root_od(read->mov);
		if (!iod) {
			iod = (GF_InitialObjectDescriptor *) gf_odf_desc_new(GF_ODF_IOD_TAG);
			iod->OD_profileAndLevel = iod->audio_profileAndLevel = iod->graphics_profileAndLevel = iod->scene_profileAndLevel = iod->visual_profileAndLevel = 0xFE;
		} else {
			while (gf_list_count(iod->ESDescriptors)) {
				GF_ESD *old = (GF_ESD *)gf_list_get(iod->ESDescriptors, 0);
				gf_odf_desc_del((GF_Descriptor *) old);
				gf_list_rem(iod->ESDescriptors, 0);
			}
		}
		gf_list_add(iod->ESDescriptors, esd);
		isor_emulate_chapters(read->mov, iod);
		return (GF_Descriptor *) iod;
	}

	iod = NULL;
	if (check_mpeg4_systems(plug, read->mov)) {
		iod = (GF_InitialObjectDescriptor *) gf_isom_get_root_od(read->mov);
		if (!iod) {
#ifndef GPAC_DISABLE_LOG
			GF_Err e = gf_isom_last_error(read->mov);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[IsoMedia] Cannot fetch MPEG-4 IOD (error %s) - generating one\n", gf_error_to_string(e) ));
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[IsoMedia] No MPEG-4 IOD found in file - generating one\n"));
			}
#endif
		}
	}
	if (!iod) return isor_emulate_iod(read);

	count = gf_list_count(iod->ESDescriptors);
	if (!count) {
		gf_odf_desc_del((GF_Descriptor*) iod);
		return isor_emulate_iod(read);
	}
	if (count==1) {
		esd = (GF_ESD *)gf_list_get(iod->ESDescriptors, 0);
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_SCENE:
		case GF_STREAM_PRIVATE_SCENE:
			break;
		case GF_STREAM_VISUAL:
			if (expect_type!=GF_MEDIA_OBJECT_VIDEO) {
				gf_odf_desc_del((GF_Descriptor*) iod);
				return isor_emulate_iod(read);
			}
			break;
		case GF_STREAM_AUDIO:
			/*we need a fake scene graph*/
			if (expect_type!=GF_MEDIA_OBJECT_AUDIO) {
				gf_odf_desc_del((GF_Descriptor*) iod);
				return isor_emulate_iod(read);
			}
			break;
		default:
			gf_odf_desc_del((GF_Descriptor*) iod);
			return NULL;
		}
	}
	/*check IOD is not badly formed (eg mixing audio, video or text streams)*/
	nb_st = 0;
	for (i=0; i<count; i++) {
		esd = (GF_ESD *)gf_list_get(iod->ESDescriptors, i);
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_VISUAL: nb_st |= 1; break;
		case GF_STREAM_AUDIO: nb_st |= 2; break;
		case GF_STREAM_TEXT: nb_st |= 4; break;
		}
	}
	if ( (nb_st & 1) + (nb_st & 2) + (nb_st & 4) > 1) {
		gf_odf_desc_del((GF_Descriptor*) iod);
		return isor_emulate_iod(read);
	}

	isor_emulate_chapters(read->mov, iod);
	return (GF_Descriptor *) iod;
}


GF_Err ISOR_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ESID;
	ISOMChannel *ch;
	GF_NetworkCommand com;
	u32 track;
	Bool is_esd_url;
	GF_Err e;
	ISOMReader *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;

	track = 0;
	ch = NULL;
	is_esd_url = 0;
	e = GF_OK;
	if (upstream) {
		e = GF_ISOM_INVALID_FILE;
		goto exit;
	}
	if (!read->mov) return GF_SERVICE_ERROR;

	if (strstr(url, "ES_ID")) {
		sscanf(url, "ES_ID=%d", &ESID);
	} else {
		/*handle url like mypath/myfile.mp4#trackID*/
		char *track_id = strrchr(url, '.');
		if (track_id) {
			track_id = strchr(url, '#');
			if (track_id) track_id ++;
		}
		is_esd_url = 1;

		ESID = 0;
		/*if only one track ok*/
		if (gf_isom_get_track_count(read->mov)==1) ESID = gf_isom_get_track_id(read->mov, 1);
		else if (track_id) {
			ESID = atoi(track_id);
			track = gf_isom_get_track_by_id(read->mov, (u32) ESID);
			if (!track) ESID = 0;
		}

	}
	if (!ESID) {
		e = GF_NOT_SUPPORTED;
		goto exit;
	}

	/*a channel cannot be open twice, it has to be closed before - NOTE a track is NOT a channel and the user can open
	several times the same track as long as a dedicated channel is used*/
	ch = isor_get_channel(read, channel);
	if (ch) {
		e = GF_SERVICE_ERROR;
		goto exit;
	}
	track = gf_isom_get_track_by_id(read->mov, (u32) ESID);
	if (!track) {
		e = GF_STREAM_NOT_FOUND;
		goto exit;
	}

	GF_SAFEALLOC(ch, ISOMChannel);
	ch->owner = read;
	ch->channel = channel;
	gf_list_add(read->channels, ch);
	ch->track = track;
	switch (gf_isom_get_media_type(ch->owner->mov, ch->track)) {
	case GF_ISOM_MEDIA_OCR:
		ch->streamType = GF_STREAM_OCR;
		break;
	case GF_ISOM_MEDIA_SCENE:
		ch->streamType = GF_STREAM_SCENE;
		break;
	}

	ch->has_edit_list = gf_isom_get_edit_segment_count(ch->owner->mov, ch->track) ? 1 : 0;
	ch->has_rap = (gf_isom_has_sync_points(ch->owner->mov, ch->track)==1) ? 1 : 0;
	ch->time_scale = gf_isom_get_media_timescale(ch->owner->mov, ch->track);

exit:
	gf_term_on_connect(read->service, channel, e);
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
		gf_term_on_command(read->service, &com, GF_OK);
	}
	if (!e && track && gf_isom_is_track_encrypted(read->mov, track)) {
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.on_channel = channel;
		com.command_type = GF_NET_CHAN_DRM_CFG;
		ch->is_encrypted = 1;
		if (gf_isom_is_ismacryp_media(read->mov, track, 1)) {
			gf_isom_get_ismacryp_info(read->mov, track, 1, NULL, &com.drm_cfg.scheme_type, &com.drm_cfg.scheme_version, &com.drm_cfg.scheme_uri, &com.drm_cfg.kms_uri, NULL, NULL, NULL);
			gf_term_on_command(read->service, &com, GF_OK);
		} else if (gf_isom_is_omadrm_media(read->mov, track, 1)) {
			gf_isom_get_omadrm_info(read->mov, track, 1, NULL, &com.drm_cfg.scheme_type, &com.drm_cfg.scheme_version, &com.drm_cfg.contentID, &com.drm_cfg.kms_uri, &com.drm_cfg.oma_drm_textual_headers, &com.drm_cfg.oma_drm_textual_headers_len, NULL, &com.drm_cfg.oma_drm_crypt_type, NULL, NULL, NULL);

			gf_media_get_file_hash(gf_isom_get_filename(read->mov), com.drm_cfg.hash);
			gf_term_on_command(read->service, &com, GF_OK);

		}
	}
	return e;
}

GF_Err ISOR_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	ISOMChannel *ch;
	GF_Err e;
	ISOMReader *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;
	if (!read->mov) return GF_SERVICE_ERROR;

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
	gf_term_on_disconnect(read->service, channel, e);
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
	*sl_compressed = 0;
	*out_reception_status = GF_OK;
	ch = isor_get_channel(read, channel);
	if (!ch) return GF_STREAM_NOT_FOUND;
	if (!ch->is_playing) return GF_OK;

	*is_new_data = 0;
	if (!ch->sample) {
		/*get sample*/
		isor_reader_get_sample(ch);
		*is_new_data = 1;
	}

	if (ch->sample) {
		*out_data_ptr = ch->sample->data;
		*out_data_size = ch->sample->dataLength;
		*out_sl_hdr = ch->current_slh;
	}
	*out_reception_status = ch->last_state;
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

static u64 check_round(ISOMChannel *ch, u64 val_ts, Double val_range, Bool make_greater)
{
	Double round_check = (Double) (s64) val_ts;
	round_check /= ch->time_scale;
//	if (round_check != val_range) val_ts += make_greater ? 1 : -1;
	return val_ts;
}

GF_Err ISOR_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	Double track_dur, media_dur;
	ISOMChannel *ch;
	ISOMReader *read;
	if (!plug || !plug->priv || !com) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;

	if (com->command_type==GF_NET_SERVICE_INFO) {
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
	if (com->command_type==GF_NET_SERVICE_HAS_AUDIO) {
		u32 i, count;
		count = gf_isom_get_track_count(read->mov);
		for (i=0; i<count; i++) {
			if (gf_isom_get_media_type(read->mov, i+1) == GF_ISOM_MEDIA_AUDIO) return GF_OK;
		}
		return GF_NOT_SUPPORTED;
	}

	if (!com->base.on_channel) return GF_NOT_SUPPORTED;

	ch = isor_get_channel(read, com->base.on_channel);
	if (!ch) return GF_STREAM_NOT_FOUND;

	switch (com->command_type) {
	case GF_NET_CHAN_SET_PADDING:
		if (!ch->track) return GF_OK;
		gf_isom_set_sample_padding(read->mov, ch->track, com->pad.padding_bytes);
		return GF_OK;
	case GF_NET_CHAN_SET_PULL:
		ch->is_pulling = 1;
		return GF_OK;
	case GF_NET_CHAN_INTERACTIVE:
		return GF_OK;
	case GF_NET_CHAN_BUFFER:
		com->buffer.max = com->buffer.min = 0;
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		if (!ch->track) {
			com->duration.duration = 0;
			return GF_OK;
		}
		ch->duration = gf_isom_get_track_duration(read->mov, ch->track);
		track_dur = (Double) (s64) ch->duration;
		track_dur /= read->time_scale;
		if (gf_isom_get_edit_segment_count(read->mov, ch->track)) {
			com->duration.duration = (Double) track_dur;
			ch->duration = (u32) (track_dur * ch->time_scale);
		} else {
			/*some file indicate a wrong TrackDuration, get the longest*/
			ch->duration = gf_isom_get_media_duration(read->mov, ch->track);
			media_dur = (Double) (s64) ch->duration;
			media_dur /= ch->time_scale;
			com->duration.duration = MAX(track_dur, media_dur);
		}
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		if (!ch->is_pulling) return GF_NOT_SUPPORTED;
		assert(!ch->is_playing);
		isor_reset_reader(ch);
		ch->speed = com->play.speed;
		ch->start = ch->end = 0;
		if (com->play.speed>0) {
			if (com->play.start_range>=0) {
				ch->start = (u64) (s64) (com->play.start_range * ch->time_scale);
				ch->start = check_round(ch, ch->start, com->play.start_range, 1);
			}
			if (com->play.end_range >= com->play.start_range) {
				ch->end = (u64) (s64) (com->play.end_range*ch->time_scale);
				ch->end = check_round(ch, ch->end, com->play.end_range, 0);
			}
		} else if (com->play.speed<0) {
			if (com->play.end_range>=com->play.start_range) ch->start = (u64) (s64) (com->play.start_range * ch->time_scale);
			if (com->play.end_range >= 0) ch->end = (u64) (s64) (com->play.end_range*ch->time_scale);
		}
		ch->is_playing = 1;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[IsoMedia] Starting channel playback "LLD" to "LLD" (%g to %g)\n", ch->start, ch->end, com->play.start_range, com->play.end_range));
		return GF_OK;
	case GF_NET_CHAN_STOP:
		isor_reset_reader(ch);
		return GF_OK;

	/*nothing to do on MP4 for channel config*/
	case GF_NET_CHAN_CONFIG:
		return GF_OK;
	case GF_NET_CHAN_GET_PIXEL_AR:
		return gf_isom_get_pixel_aspect_ratio(read->mov, ch->track, 1, &com->par.hSpacing, &com->par.vSpacing);
	case GF_NET_CHAN_GET_DSI:
	{
		/*it may happen that there are conflicting config when using ESD URLs...*/
		GF_DecoderConfig *dcd = gf_isom_get_decoder_config(read->mov, ch->track, 1);
		com->get_dsi.dsi = NULL;
		com->get_dsi.dsi_len = 0;
		if (dcd) {
			if (dcd->decoderSpecificInfo) {
				com->get_dsi.dsi = dcd->decoderSpecificInfo->data;
				com->get_dsi.dsi_len = dcd->decoderSpecificInfo->dataLength;
				dcd->decoderSpecificInfo->data = NULL;
			}
			gf_odf_desc_del((GF_Descriptor *) dcd);
		}
	}
		return GF_OK;
	}
	return GF_NOT_SUPPORTED;
}

static Bool ISOR_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	char szURL[2048], *sep;
	ISOMReader *read = (ISOMReader *)plug->priv;
	const char *this_url = gf_term_get_service_url(read->service);
	if (!this_url || !url) return 0;

	if (!strcmp(this_url, url)) return 1;

	strcpy(szURL, this_url);
	sep = strrchr(szURL, '#');
	if (sep) sep[0] = 0;

	/*direct addressing in service*/
	if (url[0] == '#') return 1;
	if (strnicmp(szURL, url, sizeof(char)*strlen(szURL))) return 0;
	return 1;
}

GF_InputService *isor_client_load()
{
	ISOMReader *reader;
	GF_InputService *plug;
	GF_SAFEALLOC(plug, GF_InputService);
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC IsoMedia Reader", "gpac distribution")

	plug->CanHandleURL = ISOR_CanHandleURL;
	plug->ConnectService = ISOR_ConnectService;
	plug->CloseService = ISOR_CloseService;
	plug->GetServiceDescriptor = ISOR_GetServiceDesc;
	plug->ConnectChannel = ISOR_ConnectChannel;
	plug->DisconnectChannel = ISOR_DisconnectChannel;
	plug->ServiceCommand = ISOR_ServiceCommand;
	plug->CanHandleURLInService = ISOR_CanHandleURLInService;
	/*we do support pull mode*/
	plug->ChannelGetSLP = ISOR_ChannelGetSLP;
	plug->ChannelReleaseSLP = ISOR_ChannelReleaseSLP;

	GF_SAFEALLOC(reader, ISOMReader);
	reader->channels = gf_list_new();
	plug->priv = reader;
	return plug;
}

void isor_client_del(GF_BaseInterface *bi)
{
	GF_InputService *plug = (GF_InputService *) bi;
	ISOMReader *read = (ISOMReader *)plug->priv;

	gf_list_del(read->channels);
	free(read);
	free(bi);
}
