/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2020
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

#include <gpac/crypt_tools.h>
#include <gpac/media_tools.h>

ISOMChannel *isor_get_channel(ISOMReader *reader, GF_FilterPid *pid)
{
	u32 i=0;
	ISOMChannel *ch;
	while ((ch = (ISOMChannel *)gf_list_enum(reader->channels, &i))) {
		if (ch->pid == pid) return ch;
	}
	return NULL;
}


static GFINLINE Bool isor_is_local(const char *url)
{
	if (!strnicmp(url, "file://", 7)) return GF_TRUE;
	if (!strnicmp(url, "gmem://", 7)) return GF_TRUE;
	if (!strnicmp(url, "gfio://", 7)) return GF_TRUE;
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
	if (!read) return GF_SERVICE_ERROR;

	if (read->pid) {
		prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILEPATH);
		assert(prop);
		src = prop->value.string;
	} else {
		src = read->src;
	}
	if (!src)  return GF_SERVICE_ERROR;

	read->src_crc = gf_crc_32(src, (u32) strlen(src));

	strcpy(szURL, src);
	tmp = gf_file_ext_start(szURL);
	if (tmp) {
		Bool truncate = GF_TRUE;
		tmp = strchr(tmp, '#');
		if (!tmp && read->pid) {
			prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_URL);
			if (prop && prop->value.string) {
				tmp = gf_file_ext_start(prop->value.string);
				if (tmp) tmp = strchr(tmp, '#');
				truncate = GF_FALSE;
			}
		}
		if (tmp) {
			if (!strnicmp(tmp, "#audio", 6)) {
				read->play_only_first_media = GF_ISOM_MEDIA_AUDIO;
			} else if (!strnicmp(tmp, "#video", 6)) {
				read->play_only_first_media = GF_ISOM_MEDIA_VISUAL;
			} else if (!strnicmp(tmp, "#auxv", 5)) {
				read->play_only_first_media = GF_ISOM_MEDIA_AUXV;
			} else if (!strnicmp(tmp, "#pict", 5)) {
				read->play_only_first_media = GF_ISOM_MEDIA_PICT;
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
	read->start_range = read->end_range = 0;
	prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILE_RANGE);
	if (prop) {
		read->start_range = prop->value.lfrac.num;
		read->end_range = prop->value.lfrac.den;
	}

	e = gf_isom_open_progressive(szURL, read->start_range, read->end_range, read->sigfrag, &read->mov, &read->missing_bytes);

	if (e == GF_ISOM_INCOMPLETE_FILE) {
		read->moov_not_loaded = GF_TRUE;
		return GF_OK;
	}

	if (e != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] error while opening %s, error=%s\n", szURL,gf_error_to_string(e)));
		gf_filter_setup_failure(filter, e);
		return e;
	}
	read->frag_type = gf_isom_is_fragmented(read->mov) ? 1 : 0;
    if (!read->frag_type && read->sigfrag) {
        e = GF_BAD_PARAM;
        GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] sigfrag requested but file %s is not fragmented\n", szURL));
        gf_filter_setup_failure(filter, e);
        return e;
    }
    
	read->time_scale = gf_isom_get_timescale(read->mov);
	if (!read->input_loaded && read->frag_type)
		read->refresh_fragmented = GF_TRUE;

	if (read->strtxt)
		gf_isom_text_set_streaming_mode(read->mov, GF_TRUE);

	return isor_declare_objects(read);
}

static void isoffin_delete_channel(ISOMChannel *ch)
{
	isor_reset_reader(ch);
	if (ch->nal_bs) gf_bs_del(ch->nal_bs);
	if (ch->avcc) gf_odf_avc_cfg_del(ch->avcc);
	if (ch->hvcc) gf_odf_hevc_cfg_del(ch->hvcc);
	gf_free(ch);
}

static void isoffin_disconnect(ISOMReader *read)
{
	read->disconnected = GF_TRUE;
	while (gf_list_count(read->channels)) {
		ISOMChannel *ch = (ISOMChannel *)gf_list_get(read->channels, 0);
		gf_list_rem(read->channels, 0);
		gf_filter_pid_remove(ch->pid);
		isoffin_delete_channel(ch);
	}

	if (read->mov) gf_isom_close(read->mov);
	read->mov = NULL;

	read->pid = NULL;
}

static GF_Err isoffin_reconfigure(GF_Filter *filter, ISOMReader *read, const char *next_url)
{
	const GF_PropertyValue *prop;
	u32 i, count;
	Bool is_new_mov = GF_FALSE;
	u64 tfdt;
	GF_ISOTrackID trackID;
	GF_ISOSegOpenMode flags=0;
	GF_Err e;

	prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILE_CACHED);
	if (prop && prop->value.boolean)
		read->input_loaded = GF_TRUE;
	read->refresh_fragmented = GF_FALSE;
	read->full_segment_flush = GF_TRUE;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] reconfigure triggered, URL %s\n", next_url));

	switch (gf_isom_probe_file_range(next_url, read->start_range, read->end_range)) {
	//this is a fragment
	case 3:
		gf_isom_release_segment(read->mov, 1);
		gf_isom_reset_fragment_info(read->mov, GF_TRUE);

		if (read->no_order_check) flags |= GF_ISOM_SEGMENT_NO_ORDER_FLAG;
#ifdef FILTER_FIXME
		if (scalable_segment) flags |= GF_ISOM_SEGMENT_SCALABLE_FLAG;
#endif
		e = gf_isom_open_segment(read->mov, next_url, read->start_range, read->end_range, flags);
		if (!read->input_loaded && (e==GF_ISOM_INCOMPLETE_FILE)) {
			read->refresh_fragmented = GF_TRUE;
			e = GF_OK;
		}
		read->seg_name_changed = GF_TRUE;

		for (i=0; i<gf_list_count(read->channels); i++) {
			ISOMChannel *ch = gf_list_get(read->channels, i);
			if (ch->last_state==GF_EOS)
				ch->last_state=GF_OK;
		}

#ifndef GPAC_DISABLE_LOG
		if (e<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[IsoMedia] Error opening new segment %s at UTC "LLU": %s\n", next_url, gf_net_get_utc(), gf_error_to_string(e) ));
		} else if (read->end_range) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Playing new range in %s: "LLU"-"LLU"\n", next_url, read->start_range, read->end_range));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] playing new segment %s (has next dep %d TODO)\n", next_url, 0));
		}
#endif
		break;
	//this is a movie, reload
	case 2:
	case 1:
		tfdt = gf_isom_get_current_tfdt(read->mov, 1);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Switching between files - opening new init segment %s (time offset="LLU") - range "LLU"-"LLU"\n", next_url, tfdt, read->start_range, read->end_range));

		if (gf_isom_is_smooth_streaming_moov(read->mov)) {
			char *tfdt_val = strstr(next_url, "tfdt=");
			//smooth addressing, replace tfdt=0000000000000000000 with proper value
			if (tfdt_val) {
				sprintf(tfdt_val+5, LLU, tfdt);
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[IsoMedia] Error finding init time for init segment %s at UTC "LLU"\n", next_url, gf_net_get_utc() ));
			}
		}

		if (read->mov) gf_isom_close(read->mov);
		e = gf_isom_open_progressive(next_url, read->start_range, read->end_range, read->sigfrag, &read->mov, &read->missing_bytes);
		if (e < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[IsoMedia] Error opening init segment %s at UTC "LLU": %s\n", next_url, gf_net_get_utc(), gf_error_to_string(e) ));
		}
		if (read->sigfrag)
			gf_isom_enable_traf_map_templates(read->mov);

		is_new_mov = GF_TRUE;
		break;
	//empty file
	case 4:
		return GF_OK;
	default:
		return GF_NOT_SUPPORTED;
	}

	gf_filter_post_process_task(filter);

	count = gf_list_count(read->channels);
	
	if (e<0) {
		count = gf_list_count(read->channels);
		gf_isom_release_segment(read->mov, 1);
		//gf_isom_reset_fragment_info(read->mov, GF_TRUE);
		read->drop_next_segment = 1;
		//error opening the segment, reset everything ...
		gf_isom_reset_fragment_info(read->mov, GF_FALSE);
		for (i=0; i<count; i++) {
			ISOMChannel *ch = gf_list_get(read->channels, i);
			if (ch)
				ch->sample_num = 0;
		}
		return e;
	}
	//segment is the first in our cache, we may need a refresh
	if (!read->input_loaded) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Opening current segment in progressive mode (download in progress)\n"));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Opening current segment in non-progressive mode (completely downloaded)\n"));
	}

	isor_check_producer_ref_time(read);

	for (i=0; i<count; i++) {
		ISOMChannel *ch = gf_list_get(read->channels, i);
		ch->last_state = GF_OK;
		ch->eos_sent = GF_FALSE;

		if (ch->base_track) {
#ifdef FILTER_FIXME
			if (scalable_segment)
#endif
			if ((0)) {
				trackID = gf_isom_get_highest_track_in_scalable_segment(read->mov, ch->base_track);
				if (trackID) {
					ch->track_id = trackID;
					ch->track = gf_isom_get_track_by_id(read->mov, ch->track_id);
				}
			} else {
				ch->track = ch->base_track;
				ch->track_id = gf_isom_get_track_id(read->mov, ch->track);
			}
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Track %d - cur sample %d - new sample count %d\n", ch->track, ch->sample_num, gf_isom_get_sample_count(ch->owner->mov, ch->track) ));

		//TODO: signal all discontinuities here
		if (is_new_mov) {
			ch->track = gf_isom_get_track_by_id(read->mov, ch->track_id);
			if (!ch->track) {
				if (gf_isom_get_track_count(read->mov)==1) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Mismatch between track IDs of different representations\n"));
					ch->track = 1;
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[IsoMedia] Mismatch between track IDs of different representations\n"));
				}
			}

			/*we changed our moov structure, sample_num now starts from 0*/
			ch->sample_num = 0;
			//this may happen if we reload moov before initializing the channel
			if (!ch->last_sample_desc_index)
				ch->last_sample_desc_index = 1;
			//and update channel config
			isor_update_channel_config(ch);

			/*restore NAL extraction mode*/
			gf_isom_set_nalu_extract_mode(read->mov, ch->track, ch->nalu_extract_mode);

			if (ch->is_cenc) {
				isor_set_crypt_config(ch);
			}
		}

		ch->last_state = GF_OK;
	}
	return e;
}

GF_Err isoffin_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	ISOMReader *read = gf_filter_get_udta(filter);

	if (is_remove) {
		isoffin_disconnect(read);
		return GF_OK;
	}
	//we must have a file path for now, if not this is a pure stream of boxes (no local file cache)
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
	if (!prop || !prop->value.string) {
		read->mem_load_mode = 1;
		if (!read->pid) read->pid = pid;
		return GF_OK;
	}

	if (read->pid && prop->value.string) {
		const char *next_url = prop->value.string;
		u64 sr, er;
		u32 crc = gf_crc_32(next_url, (u32) strlen(next_url) );

		sr = er = 0;
		prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILE_RANGE);
		if (prop) {
			sr = prop->value.lfrac.num;
			er = prop->value.lfrac.den;
		}

		//if eos is signaled, don't check for crc since we might have the same blob address (same alloc)
		if (!read->eos_signaled && (read->src_crc == crc) && (read->start_range==sr) && (read->end_range==er)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] same URL crc and range for %s, skipping reconfigure\n", next_url));
			return GF_OK;
		}
		read->src_crc = crc;
		read->start_range = sr;
		read->end_range = er;
		read->input_loaded = GF_FALSE;
		read->eos_signaled = GF_FALSE;
		
		//we need to reconfigure
		return isoffin_reconfigure(filter, read, next_url);
	}

	read->pid = pid;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_FILE_CACHED);
	if (prop && prop->value.boolean) {
		GF_FilterEvent evt;
		read->input_loaded = GF_TRUE;
		GF_FEVT_INIT(evt, GF_FEVT_PLAY_HINT, pid);
		evt.play.full_file_only=1;
		gf_filter_pid_send_event(pid, &evt);
	}
	return isoffin_setup(filter, read);
}

GF_Err isoffin_initialize(GF_Filter *filter)
{
	ISOMReader *read = gf_filter_get_udta(filter);
	GF_Err e = GF_OK;
	read->filter = filter;
	read->channels = gf_list_new();

	if (read->src) {
		read->input_loaded = GF_TRUE;
		return isoffin_setup(filter, read);
	}
	else if (read->mov) {
		read->extern_mov = GF_TRUE;
		read->input_loaded = GF_TRUE;
		read->frag_type = gf_isom_is_fragmented(read->mov) ? 1 : 0;
		read->time_scale = gf_isom_get_timescale(read->mov);

		if (read->sigfrag) {
			gf_isom_enable_traf_map_templates(read->mov);
		}

		if (read->catseg) {
			e = gf_isom_open_segment(read->mov, read->catseg, 0, 0, 0);
		}
		if (!e)
			e = isor_declare_objects(read);

		gf_filter_post_process_task(filter);
	}
	return e;
}


static void isoffin_finalize(GF_Filter *filter)
{
	ISOMReader *read = (ISOMReader *) gf_filter_get_udta(filter);

	read->disconnected = GF_TRUE;

	while (gf_list_count(read->channels)) {
		ISOMChannel *ch = (ISOMChannel *)gf_list_get(read->channels, 0);
		gf_list_rem(read->channels, 0);
		isoffin_delete_channel(ch);
	}
	gf_list_del(read->channels);

	if (!read->extern_mov && read->mov) gf_isom_close(read->mov);
	read->mov = NULL;

	if (read->mem_blob.data) gf_free(read->mem_blob.data);
	if (read->mem_url) gf_free(read->mem_url);
}

void isor_set_crypt_config(ISOMChannel *ch)
{
	GF_ISOFile *mov = ch->owner->mov;
	u32 track = ch->track;
	u32 scheme_type, scheme_version, PSSH_count=0, i, count;
	const char *kms_uri, *scheme_uri;
	Bool selectiveEncryption=0;
	u32 IVLength=0;
	u32 KeyIndicationLength=0;
	const char *txtHdr=NULL;
	const char *contentID=NULL;
	u32 txtHdrLen=0;
	u64 plainTextLen=0;
	u32 crypt_type=0;
	u32 stsd_idx = ch->owner->stsd ? ch->owner->stsd : 1;

	if (!ch->is_encrypted) return;

	scheme_type = scheme_version = 0;
	kms_uri = scheme_uri = NULL;

	/*ugly fix to detect when an stsd uses both clear and encrypted sample descriptions*/
	count = gf_isom_get_sample_description_count(ch->owner->mov, ch->track);
	if (count>1) {
		u32 first_crypted_stsd = 0;
		u32 nb_same_mtype = 1;
		u32 nb_clear=0, nb_encrypted=0;
		u32 base_subtype = 0;
		Bool first_is_clear = GF_FALSE;
		for (i=0; i<count; i++) {
			u32 mtype = gf_isom_get_media_subtype(ch->owner->mov, ch->track, i+1);
			if ( gf_isom_is_media_encrypted(ch->owner->mov, track, i+1)) {
				gf_isom_get_original_format_type(ch->owner->mov, ch->track, i+1, &mtype);
				nb_encrypted++;
				if (!first_crypted_stsd) first_crypted_stsd = i+1;
			} else {
				nb_clear++;
				if (!i) first_is_clear = GF_TRUE;
			}
			if (!i) base_subtype = mtype;
			else if (base_subtype==mtype) {
				nb_same_mtype++;
			}
		}
		if ((nb_same_mtype==count) && (nb_clear==nb_encrypted)) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_STSD_MODE, &PROP_UINT(first_is_clear ? 1 : 2) );
			stsd_idx = first_crypted_stsd;
		}

	}

	if (gf_isom_is_ismacryp_media(mov, track, stsd_idx)) {
		gf_isom_get_ismacryp_info(mov, track, stsd_idx, NULL, &scheme_type, &scheme_version, &scheme_uri, &kms_uri, &selectiveEncryption, &IVLength, &KeyIndicationLength);
	} else if (gf_isom_is_omadrm_media(mov, track, stsd_idx)) {
		//u8 hash[20];
		gf_isom_get_omadrm_info(mov, track, stsd_idx, NULL, &scheme_type, &scheme_version, &contentID, &kms_uri, &txtHdr, &txtHdrLen, &plainTextLen, &crypt_type, &selectiveEncryption, &IVLength, &KeyIndicationLength);

		//gf_media_get_file_hash(gf_isom_get_filename(mov), hash);
	} else if (gf_isom_is_cenc_media(mov, track, stsd_idx)) {
		ch->is_cenc = GF_TRUE;

		gf_isom_get_cenc_info(ch->owner->mov, ch->track, stsd_idx, NULL, &scheme_type, &scheme_version, NULL);

		PSSH_count = gf_isom_get_pssh_count(ch->owner->mov);
		//if no PSSH declared, DO update the properties (PSSH is not mandatory)
	} else if (gf_isom_is_adobe_protection_media(mov, track, stsd_idx)) {
		u32 ofmt;
		scheme_version = 1;
		scheme_type = GF_ISOM_ADOBE_SCHEME;
		const char *metadata = NULL;

		gf_isom_get_adobe_protection_info(mov, track, stsd_idx, &ofmt, &scheme_type, &scheme_version, &metadata);
		if (metadata)
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ADOBE_CRYPT_META, &PROP_DATA((char *)metadata, (u32) strlen(metadata) ) );
	}

	gf_filter_pid_set_property(ch->pid, GF_PROP_PID_PROTECTION_SCHEME_TYPE, &PROP_UINT(scheme_type) );
	gf_filter_pid_set_property(ch->pid, GF_PROP_PID_PROTECTION_SCHEME_VERSION, &PROP_UINT(scheme_version) );
	if (scheme_uri) gf_filter_pid_set_property(ch->pid, GF_PROP_PID_PROTECTION_SCHEME_URI, &PROP_STRING((char*) scheme_uri) );
	if (kms_uri) gf_filter_pid_set_property(ch->pid, GF_PROP_PID_PROTECTION_KMS_URI, &PROP_STRING((char*) kms_uri) );

	if (selectiveEncryption) gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISMA_SELECTIVE_ENC, &PROP_BOOL(GF_TRUE) );
	if (IVLength) gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISMA_IV_LENGTH, &PROP_UINT(IVLength) );
	if (KeyIndicationLength) gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISMA_KI_LENGTH, &PROP_UINT(KeyIndicationLength) );
	if (crypt_type) gf_filter_pid_set_property(ch->pid, GF_PROP_PID_OMA_CRYPT_TYPE, &PROP_UINT(crypt_type) );
	if (contentID) gf_filter_pid_set_property(ch->pid, GF_PROP_PID_OMA_CID, &PROP_STRING(contentID) );
	if (txtHdr) gf_filter_pid_set_property(ch->pid, GF_PROP_PID_OMA_TXT_HDR, &PROP_STRING(txtHdr) );
	if (plainTextLen) gf_filter_pid_set_property(ch->pid, GF_PROP_PID_OMA_CLEAR_LEN, &PROP_LONGUINT(plainTextLen) );

	if (ch->is_cenc) {
		u8 *psshd;
		GF_BitStream *pssh_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		u32 s, container_type;

		gf_bs_write_u32(pssh_bs, PSSH_count);

		/*fill PSSH in the structure. We will free it in CENC_Setup*/
		for (i=0; i<PSSH_count; i++) {
			bin128 SystemID;
			u32 version;
			u32 KID_count;
			bin128 *KIDs;
			u32 private_data_size;
			u8 *private_data;
			gf_isom_get_pssh_info(ch->owner->mov, i+1, SystemID, &version, &KID_count, (const bin128 **) & KIDs, (const u8 **) &private_data, &private_data_size);

			gf_bs_write_data(pssh_bs, SystemID, 16);
			gf_bs_write_u32(pssh_bs, version);
			gf_bs_write_u32(pssh_bs, KID_count);
			for (s=0; s<KID_count; s++) {
				gf_bs_write_data(pssh_bs, KIDs[s], 16);
			}
			gf_bs_write_u32(pssh_bs, private_data_size);
			gf_bs_write_data(pssh_bs, private_data, private_data_size);
		}
		gf_bs_get_content(pssh_bs, &psshd, &s);
		gf_bs_del(pssh_bs);
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_PSSH, & PROP_DATA_NO_COPY(psshd, s) );

		gf_isom_cenc_get_default_info(ch->owner->mov, ch->track, stsd_idx, &container_type, &ch->pck_encrypted, &ch->IV_size, &ch->KID, &ch->constant_IV_size, &ch->constant_IV, &ch->crypt_byte_block, &ch->skip_byte_block);

		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_STORE, &PROP_UINT(container_type) );

		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ENCRYPTED, &PROP_BOOL(ch->pck_encrypted) );

		if (ch->skip_byte_block || ch->crypt_byte_block) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_PATTERN, &PROP_FRAC_INT(ch->skip_byte_block, ch->crypt_byte_block) );
		}

		if (ch->IV_size) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_IV_SIZE, &PROP_UINT(ch->IV_size) );
		}
		if (ch->constant_IV_size) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_IV_CONST, &PROP_DATA(ch->constant_IV, ch->constant_IV_size) );
		}
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_KID, &PROP_DATA(ch->KID, sizeof(bin128) ) );
	}
}


ISOMChannel *isor_create_channel(ISOMReader *read, GF_FilterPid *pid, u32 track, u32 item_id, Bool force_no_extractors)
{
	ISOMChannel *ch;
	const GF_PropertyValue *p;
	s64 ts_shift;
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
	ch->item_id = item_id;

	ch->nalu_extract_mode = 0;
	ch->track_id = gf_isom_get_track_id(read->mov, ch->track);
	switch (gf_isom_get_media_type(ch->owner->mov, ch->track)) {
	case GF_ISOM_MEDIA_OCR:
		ch->streamType = GF_STREAM_OCR;
		break;
	case GF_ISOM_MEDIA_SCENE:
		ch->streamType = GF_STREAM_SCENE;
		break;
	case GF_ISOM_MEDIA_VISUAL:
	case GF_ISOM_MEDIA_AUXV:
	case GF_ISOM_MEDIA_PICT:
		gf_isom_get_reference(ch->owner->mov, ch->track, GF_ISOM_REF_BASE, 1, &ch->base_track);
		//use base track only if avc/svc or hevc/lhvc. If avc+lhvc we need different rules
		if ( gf_isom_get_avc_svc_type(ch->owner->mov, ch->base_track, 1) == GF_ISOM_AVCTYPE_AVC_ONLY) {
			if ( gf_isom_get_hevc_lhvc_type(ch->owner->mov, ch->track, 1) >= GF_ISOM_HEVCTYPE_HEVC_ONLY) {
				ch->base_track=0;
			}
		}
		ch->next_track = 0;
		/*in scalable mode add SPS/PPS in-band*/
		if (ch->base_track)
			ch->nalu_extract_mode = GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG /*| GF_ISOM_NALU_EXTRACT_ANNEXB_FLAG*/;
		break;
	}
	if (!read->noedit) {
		ch->ts_offset = 0;
		ch->has_edit_list = gf_isom_get_edit_list_type(ch->owner->mov, ch->track, &ch->ts_offset) ? GF_TRUE : GF_FALSE;
		if (!ch->has_edit_list && ch->ts_offset) {
			//if >0 this is a hold, we signal positive delay
			//if <0 this is a skip, we signal negative delay
			gf_filter_pid_set_property(pid, GF_PROP_PID_DELAY, &PROP_SINT((s32) ch->ts_offset) );
		}
	} else
		ch->has_edit_list = GF_FALSE;

	ch->has_rap = (gf_isom_has_sync_points(ch->owner->mov, ch->track)==1) ? GF_TRUE : GF_FALSE;
	gf_filter_pid_set_property(pid, GF_PROP_PID_HAS_SYNC, &PROP_BOOL(ch->has_rap) );
	ch->time_scale = gf_isom_get_media_timescale(ch->owner->mov, ch->track);

	ts_shift = gf_isom_get_cts_to_dts_shift(ch->owner->mov, ch->track);
	if (ts_shift) {
		gf_filter_pid_set_property(pid, GF_PROP_PID_CTS_SHIFT, &PROP_UINT((u32) ts_shift) );
	}

	if (!track || !gf_isom_is_track_encrypted(read->mov, track)) {
		if (force_no_extractors) {
			ch->nalu_extract_mode = GF_ISOM_NALU_EXTRACT_LAYER_ONLY;
		} else {
			switch (read->smode) {
			case MP4DMX_SPLIT_EXTRACTORS:
				ch->nalu_extract_mode = GF_ISOM_NALU_EXTRACT_INSPECT | GF_ISOM_NALU_EXTRACT_TILE_ONLY;
				break;
			case MP4DMX_SPLIT:
				ch->nalu_extract_mode = GF_ISOM_NALU_EXTRACT_LAYER_ONLY | GF_ISOM_NALU_EXTRACT_TILE_ONLY;
				break;
			default:
				break;
			}
		}

		if (ch->nalu_extract_mode) {
			gf_isom_set_nalu_extract_mode(ch->owner->mov, ch->track, ch->nalu_extract_mode);
		}
		return ch;
	}
	if (ch->owner->nocrypt) {
		ch->is_encrypted = GF_FALSE;
		return ch;
	}
	ch->is_encrypted = GF_TRUE;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (p) gf_filter_pid_set_property(pid, GF_PROP_PID_ORIG_STREAM_TYPE, &PROP_UINT(p->value.uint) );

	gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_ENCRYPTED) );

	isor_set_crypt_config(ch);

	if (ch->nalu_extract_mode) {
		if (ch->is_encrypted) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] using sample NAL rewrite with encryption is not yet supported, patch welcome\n"));
		} else {
			gf_isom_set_nalu_extract_mode(ch->owner->mov, ch->track, ch->nalu_extract_mode);
		}
	}
	return ch;
}

/*switch channel quality. Return next channel or current channel if error*/
static
u32 isoffin_channel_switch_quality(ISOMChannel *ch, GF_ISOFile *the_file, Bool switch_up)
{
	u32 i, count, next_track, trackID, cur_track;
	s32 ref_count;

	cur_track = ch->next_track ? ch->next_track : ch->track;
	count = gf_isom_get_track_count(the_file);
	trackID = gf_isom_get_track_id(the_file, cur_track);
	next_track = 0;

	if (switch_up) {
		for (i = 0; i < count; i++) {
			ref_count = gf_isom_get_reference_count(the_file, i+1, GF_ISOM_REF_SCAL);
			if (ref_count < 0)
				return cur_track; //error
			if (ref_count == 0)
				continue;
			/*next track is the one that has the last reference of type GF_ISOM_REF_SCAL refers to this current track*/
			if ((u32)ref_count == gf_isom_has_track_reference(the_file, i+1, GF_ISOM_REF_SCAL, trackID)) {
				next_track = i+1;
				break;
			}
		}
		/*this is the highest quality*/
		if (!next_track)
			return cur_track;
	} else {
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

static Bool isoffin_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 count, i;
	Bool cancel_event = GF_TRUE;
	ISOMChannel *ch;
	ISOMReader *read = gf_filter_get_udta(filter);

	if (!read || read->disconnected) return GF_FALSE;

	if (evt->base.type == GF_FEVT_QUALITY_SWITCH) {
		count = gf_list_count(read->channels);
		for (i = 0; i < count; i++) {
			ch = (ISOMChannel *)gf_list_get(read->channels, i);
			if (ch->base_track && gf_isom_needs_layer_reconstruction(read->mov)) {
				ch->next_track = isoffin_channel_switch_quality(ch, read->mov, evt->quality_switch.up);
			}
#ifdef GPAC_ENABLE_COVERAGE
			else if (gf_sys_is_cov_mode()) {
				isoffin_channel_switch_quality(ch, read->mov, evt->quality_switch.up);
			}
#endif

		}
		return GF_TRUE;
	}

	if (!evt->base.on_pid) return GF_FALSE;

	ch = isor_get_channel(read, evt->base.on_pid);
	if (!ch)
		return GF_FALSE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		isor_reset_reader(ch);
		ch->eos_sent = GF_FALSE;
		ch->speed = evt->play.speed;
		ch->initial_play_seen = GF_TRUE;
		read->reset_frag_state = 1;
		if (read->frag_type)
			read->frag_type = 1;

		ch->start = ch->end = 0;
		if (evt->play.speed>=0) {
			Double t;
			if (evt->play.start_range>=0) {
				t = evt->play.start_range;
				t *= ch->time_scale;
				ch->start = (u64) t;
			}
			if (evt->play.end_range >= evt->play.start_range) {
				ch->end = (u64) -1;
				if (evt->play.end_range<FLT_MAX) {
					t = evt->play.end_range;
					t *= ch->time_scale;
					ch->end = (u64) t;
				}
			}
		} else {
			Double end = evt->play.end_range;
			if (end==-1) end = 0;
			ch->start = (u64) (s64) (evt->play.start_range * ch->time_scale);
			if (end <= evt->play.start_range)
				ch->end = (u64) (s64) (end  * ch->time_scale);
		}
		ch->playing = GF_TRUE;
		ch->sample_num = evt->play.from_pck;

		ch->sap_only = evt->play.drop_non_ref ? GF_TRUE : GF_FALSE;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[IsoMedia] Starting channel playback "LLD" to "LLD" (%g to %g)\n", ch->start, ch->end, evt->play.start_range, evt->play.end_range));

		if (!read->nb_playing)
			gf_isom_reset_seq_num(read->mov);

		if (read->is_partial_download) read->input_loaded = GF_FALSE;

		if (evt->play.no_byterange_forward) {
			//new segment will be loaded, reset
			gf_isom_reset_tables(read->mov, GF_TRUE);
			gf_isom_reset_data_offset(read->mov, NULL);
			read->refresh_fragmented = GF_TRUE;
			//send play event
			cancel_event = GF_FALSE;
		} else if (!read->nb_playing && read->pid && !read->input_loaded) {
			GF_FilterEvent fevt;
			Bool is_sidx_seek = GF_FALSE;
			u64 max_offset = GF_FILTER_NO_BO;
			count = gf_list_count(read->channels);

			//try sidx
			if (read->frag_type) {
				u32 ts;
				u64 dur=0;
				GF_Err e = gf_isom_get_file_offset_for_time(read->mov, evt->play.start_range, &max_offset);
				if (e==GF_OK) {
					if (evt->play.start_range>0)
						gf_isom_reset_tables(read->mov, GF_TRUE);

					is_sidx_seek = GF_TRUE;
					//in case we loaded moov but not sidx, update duration
					if ((gf_isom_get_sidx_duration(read->mov, &dur, &ts)==GF_OK) && dur) {
						dur *= read->time_scale;
						dur /= ts;
						if (ch->duration != dur) {
							ch->duration = dur;
							gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DURATION, &PROP_FRAC64_INT(ch->duration, read->time_scale));
						}
					}
				}
			}

			if (!is_sidx_seek) {
				for (i=0; i< count; i++) {
					u32 mode, sample_desc_index, sample_num;
					u64 data_offset;
					GF_Err e;
					u64 time;
					ch = gf_list_get(read->channels, i);
					mode = ch->disable_seek ? GF_ISOM_SEARCH_BACKWARD : GF_ISOM_SEARCH_SYNC_BACKWARD;
					time = (u64) (evt->play.start_range * ch->time_scale);

					/*take care of seeking out of the track range*/
					if (!read->frag_type && (ch->duration < time)) {
						e = gf_isom_get_sample_for_movie_time(read->mov, ch->track, ch->duration, 	&sample_desc_index, mode, NULL, &sample_num, &data_offset);
					} else {
						e = gf_isom_get_sample_for_movie_time(read->mov, ch->track, time, &sample_desc_index, mode, NULL, &sample_num, &data_offset);
					}
					if ((e == GF_OK) && (data_offset<max_offset))
						max_offset = data_offset;
				}
			}

			if ((evt->play.start_range || read->is_partial_download)  && (max_offset != GF_FILTER_NO_BO) ) {

				//send a seek request
				read->is_partial_download = GF_TRUE;
				read->wait_for_source = GF_TRUE;
				read->refresh_fragmented = GF_TRUE;

				GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, read->pid);
				fevt.seek.start_offset = max_offset;
				gf_filter_pid_send_event(read->pid, &fevt);
				gf_isom_set_byte_offset(read->mov, is_sidx_seek ? 0 : max_offset);

			}
		}
		//always request a process task upon a play
		gf_filter_post_process_task(read->filter);
		read->nb_playing++;
		//cancel event unless dash mode
		return cancel_event;

	case GF_FEVT_STOP:
 		if (read->nb_playing) read->nb_playing--;
		isor_reset_reader(ch);
		//don't send a stop if some of our channels are still waiting for initial play
		for (i=0; i<gf_list_count(read->channels); i++) {
			ISOMChannel *a_ch = gf_list_get(read->channels, i);
			if (ch==a_ch) continue;
			if (!a_ch->initial_play_seen) return GF_TRUE;
		}
		//cancel event if nothing playing
		if (read->nb_playing) return GF_TRUE;
		read->input_is_stop = GF_TRUE;
		return GF_FALSE;

	case GF_FEVT_SET_SPEED:
	case GF_FEVT_RESUME:
		ch->speed = evt->play.speed;
		if (ch->sap_only && !evt->play.drop_non_ref) {
			ch->sap_only = 2;
		} else {
			ch->sap_only = evt->play.drop_non_ref ? GF_TRUE : GF_FALSE;
		}
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

static void isoffin_push_buffer(GF_Filter *filter, ISOMReader *read, const u8 *pck_data, u32 data_size)
{
	u64 bytes_missing;
	GF_Err e;

	if (!read->mem_url) {
		char szPath[200];
		sprintf(szPath, "gmem://%p", &read->mem_blob);
		read->mem_url = gf_strdup(szPath);
	}
	read->mem_blob.data = gf_realloc(read->mem_blob.data, read->mem_blob.size + data_size);
	memcpy(read->mem_blob.data + read->mem_blob.size, pck_data, data_size);
	read->mem_blob.size += data_size;

	if (read->mem_load_mode==1) {
		u32 box_type;
		e = gf_isom_open_progressive_ex(read->mem_url, 0, 0, GF_FALSE, &read->mov, &bytes_missing, &box_type);

		if (e && (e != GF_ISOM_INCOMPLETE_FILE)) {
			gf_filter_setup_failure(filter, e);
			read->mem_load_mode = 0;
			read->in_error = e;
			return;
		}
		if (!read->mov) {
			switch (box_type) {
			case GF_4CC('m','d','a','t'):
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] non fragmented ISOBMFF with moof after mdat and no underlying file cache (pipe or other stream input), not supported !\n"));
				gf_filter_setup_failure(filter, GF_NOT_SUPPORTED);
				read->mem_load_mode = 0;
				read->in_error = GF_NOT_SUPPORTED;
				break;
			default:
				read->moov_not_loaded = GF_TRUE;
				break;
			}
			return;
		}

		read->frag_type = gf_isom_is_fragmented(read->mov) ? 1 : 0;
		read->time_scale = gf_isom_get_timescale(read->mov);
		isor_declare_objects(read);
		read->mem_load_mode = 2;
		read->moov_not_loaded = GF_FALSE;
		return;
	}
	//refresh file
	gf_isom_refresh_fragmented(read->mov, &bytes_missing, read->mem_url);
}

static void isoffin_purge_mem(ISOMReader *read, u64 min_offset)
{
	u32 i, count;
	u64 top_offset;
	u32 nb_bytes_to_purge;
	u64 bytes_missing;

	//purge every
	if (read->mstore_purge && (min_offset - read->last_min_offset < read->mstore_purge))
		return;

	if (read->frag_type) {
		//get position of current box being parsed - if new offset is greater than this box we cannot remove
		//bytes (we would trash the top-level box header)
		gf_isom_get_current_top_box_offset(read->mov, &top_offset);
		if (top_offset<min_offset) {
			return;
		}
	}
	read->last_min_offset = min_offset;

	assert(min_offset>=read->bytes_removed);
	//min_offset is given in absolute file position
	nb_bytes_to_purge = (u32) (min_offset - read->bytes_removed);
	assert(nb_bytes_to_purge<=read->mem_blob.size);

	memmove(read->mem_blob.data, read->mem_blob.data+nb_bytes_to_purge, read->mem_blob.size - nb_bytes_to_purge);
	read->mem_blob.size -= nb_bytes_to_purge;
	read->bytes_removed += nb_bytes_to_purge;
	gf_isom_set_removed_bytes(read->mov, read->bytes_removed);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[IsoMedia] mem mode %d bytes in mem, "LLU" bytes trashed since start\n", read->mem_blob.size, read->bytes_removed));

	//force a refresh
	gf_isom_refresh_fragmented(read->mov, &bytes_missing, read->mem_url);

	if (!read->frag_type)
		return;

	//fragmented file, cleanup sample tables
	count = gf_list_count(read->channels);
	for (i=0; i<count; i++) {
		ISOMChannel *ch = gf_list_get(read->channels, i);
		u32 num_samples;
#ifndef GPAC_DISABLE_LOG
		u32 prev_samples = gf_isom_get_sample_count(read->mov, ch->track);
#endif
		//don't run this too often
		if (ch->sample_num<=1+read->mstore_samples) continue;

		if (gf_isom_purge_samples(read->mov, ch->track, ch->sample_num-1) == GF_OK)
			ch->sample_num = 1;

		num_samples = gf_isom_get_sample_count(read->mov, ch->track);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[IsoMedia] mem mode %d samples now in track %d (prev %d)\n", num_samples, ch->track_id, prev_samples));
	}
}

static GF_Err isoffin_process(GF_Filter *filter)
{
	ISOMReader *read = gf_filter_get_udta(filter);
	u32 i, count = gf_list_count(read->channels);
	Bool is_active = GF_FALSE;
	Bool in_is_eos = GF_FALSE;
	Bool has_new_data = GF_FALSE;
	u64 min_offset_plus_one = 0;
	if (read->in_error)
		return read->in_error;

	if (read->pid) {
		Bool fetch_input = GF_TRUE;
		if (read->mem_load_mode==2) {
			if (!read->force_fetch && read->mem_blob.size > read->mstore_size) {
				fetch_input = GF_FALSE;
			}
			read->force_fetch = GF_FALSE;
		}
		while (fetch_input) {
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

			if (read->mem_load_mode) {
				u32 data_size;
				const u8 *pck_data = gf_filter_pck_get_data(pck, &data_size);
				isoffin_push_buffer(filter, read, pck_data, data_size);
			}
			gf_filter_pid_drop_packet(read->pid);
			has_new_data = GF_TRUE;
			if (read->in_error)
				return read->in_error;
		}
		if (gf_filter_pid_is_eos(read->pid)) {
			read->input_loaded = GF_TRUE;
			in_is_eos = GF_TRUE;
		}
		if (read->input_is_stop) {
			read->input_loaded = GF_TRUE;
			in_is_eos = GF_TRUE;
			read->input_is_stop = GF_FALSE;
		}
		if (!read->frag_type && read->input_loaded) {
			in_is_eos = GF_TRUE;
		}
	} else if (read->extern_mov) {
		in_is_eos = GF_TRUE;
		read->input_loaded = GF_TRUE;
	}
	if (read->moov_not_loaded) {
		if (read->mem_load_mode)
			return GF_OK;
		read->moov_not_loaded = GF_FALSE;
		return isoffin_setup(filter, read);
	}

	if (read->refresh_fragmented) {
		const GF_PropertyValue *prop;

		if (in_is_eos) {
			read->refresh_fragmented = GF_FALSE;
		} else {
			prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILE_CACHED);
			if (prop && prop->value.boolean)
				read->refresh_fragmented = GF_FALSE;
		}

		if (has_new_data) {
			u64 bytesMissing=0;
			GF_Err e;
			const char *new_url = NULL;
			prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILEPATH);
			if (prop) new_url = prop->value.string;

			e = gf_isom_refresh_fragmented(read->mov, &bytesMissing, new_url);

			if (e && (e!= GF_ISOM_INCOMPLETE_FILE)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[IsoMedia] Failed to refresh current segment: %s\n", gf_error_to_string(e) ));
				read->refresh_fragmented = GF_FALSE;
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Refreshing current segment at UTC "LLU" - "LLU" bytes still missing - input is EOS %d\n", gf_net_get_utc(), bytesMissing, in_is_eos));
			}

			if (!read->refresh_fragmented && (e==GF_ISOM_INCOMPLETE_FILE)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[IsoMedia] Incomplete Segment received - "LLU" bytes missing but EOF found\n", bytesMissing ));
			}

#ifndef GPAC_DISABLE_LOG
			if (gf_log_tool_level_on(GF_LOG_DASH, GF_LOG_DEBUG)) {
				for (i=0; i<count; i++) {
					ISOMChannel *ch = gf_list_get(read->channels, i);
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] refresh track %d fragment - cur sample %d - new sample count %d\n", ch->track, ch->sample_num, gf_isom_get_sample_count(ch->owner->mov, ch->track) ));
				}
			}
#endif
			isor_check_producer_ref_time(read);
			if (!read->frag_type)
				read->refresh_fragmented = GF_FALSE;
		}
	}

	for (i=0; i<count; i++) {
		u8 *data;
		u32 nb_pck=50;
		ISOMChannel *ch;
		ch = gf_list_get(read->channels, i);
		if (!ch->playing) {
			continue;
		}
		//eos not sent on this channel, we are active
		if (!ch->eos_sent)
			is_active = GF_TRUE;

		while (nb_pck) {
			ch->sample_data_offset = 0;
			if (!read->full_segment_flush && gf_filter_pid_would_block(ch->pid) )
				break;

			if (ch->item_id) {
				isor_reader_get_sample_from_item(ch);
			} else {
				isor_reader_get_sample(ch);
			}

			if (read->stsd && (ch->last_sample_desc_index != read->stsd) && ch->sample) {
				isor_reader_release_sample(ch);
				continue;
			}
			if (ch->sample) {
				u32 sample_dur;
				u8 dep_flags;
				u8 *subs_buf;
				u32 subs_buf_size;
				GF_FilterPacket *pck;
				if (ch->needs_pid_reconfig) {
					isor_update_channel_config(ch);
					ch->needs_pid_reconfig = GF_FALSE;
				}

				//strip param sets from payload, trigger reconfig if needed
				isor_reader_check_config(ch);

				pck = gf_filter_pck_new_alloc(ch->pid, ch->sample->dataLength, &data);
				assert(pck);

				memcpy(data, ch->sample->data, ch->sample->dataLength);

				gf_filter_pck_set_dts(pck, ch->dts);
				gf_filter_pck_set_cts(pck, ch->cts);
				if (ch->sample->IsRAP==-1) {
					gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
					ch->redundant = 1;
				} else {
					gf_filter_pck_set_sap(pck, (GF_FilterSAPType) ch->sample->IsRAP);
				}

				if (ch->sap_3)
					gf_filter_pck_set_sap(pck, GF_FILTER_SAP_3);
				else if (ch->sap_4_type) {
					gf_filter_pck_set_sap(pck, (ch->sap_4_type==GF_ISOM_SAMPLE_PREROLL) ? GF_FILTER_SAP_4_PROL : GF_FILTER_SAP_4);
					gf_filter_pck_set_roll_info(pck, ch->roll);
				}

				sample_dur = ch->au_duration;
				if (ch->sample->nb_pack)
					sample_dur *= ch->sample->nb_pack;
				gf_filter_pck_set_duration(pck, sample_dur);
				gf_filter_pck_set_seek_flag(pck, ch->seek_flag);

				dep_flags = ch->isLeading;
				dep_flags <<= 2;
				dep_flags |= ch->dependsOn;
				dep_flags <<= 2;
				dep_flags |= ch->dependedOn;
				dep_flags <<= 2;
				dep_flags |= ch->redundant;

				if (dep_flags)
					gf_filter_pck_set_dependency_flags(pck, dep_flags);

				gf_filter_pck_set_crypt_flags(pck, ch->pck_encrypted ? GF_FILTER_PCK_CRYPT : 0);
				gf_filter_pck_set_seq_num(pck, ch->sample_num);


				subs_buf = gf_isom_sample_get_subsamples_buffer(read->mov, ch->track, ch->sample_num, &subs_buf_size);
				if (subs_buf) {
					gf_filter_pck_set_property(pck, GF_PROP_PCK_SUBS, &PROP_DATA_NO_COPY(subs_buf, subs_buf_size) );
				}

				/**/
				if (ch->cenc_state_changed && ch->pck_encrypted) {
					gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_PATTERN, &PROP_FRAC_INT(ch->skip_byte_block, ch->crypt_byte_block) );

					if (!ch->IV_size) {
						if (ch->constant_IV_size) {
							gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_IV_CONST, &PROP_DATA(ch->constant_IV, ch->constant_IV_size) );
						} else {
							gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_IV_CONST, NULL);
						}
					} else {
						gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_IV_SIZE, &PROP_UINT(ch->IV_size) );
					}

					gf_filter_pid_set_property(ch->pid, GF_PROP_PID_KID, &PROP_DATA(ch->KID, sizeof(bin128) ) );
				}
				if (ch->sai_buffer && ch->pck_encrypted) {
					assert(ch->sai_buffer_size);
					gf_filter_pck_set_property(pck, GF_PROP_PCK_CENC_SAI, &PROP_DATA(ch->sai_buffer, ch->sai_buffer_size) );
				}

				if (read->sigfrag) {
					GF_ISOFragmentBoundaryInfo finfo;
					if (gf_isom_sample_is_fragment_start(read->mov, ch->track, ch->sample_num, &finfo) ) {
						u64 start=0;
						u32 traf_start = finfo.seg_start_plus_one ? 2 : 1;
						gf_filter_pck_set_property(pck, GF_PROP_PCK_FRAG_START, &PROP_UINT(traf_start));

						start = finfo.frag_start;
						if (finfo.seg_start_plus_one) start = finfo.seg_start_plus_one-1;
						gf_filter_pck_set_property(pck, GF_PROP_PCK_FRAG_RANGE, &PROP_FRAC64_INT(start, finfo.mdat_end));
						if (finfo.moof_template) {
							gf_filter_pck_set_property(pck, GF_PROP_PCK_MOOF_TEMPLATE, &PROP_DATA((u8 *)finfo.moof_template, finfo.moof_template_size));
						}
						if (finfo.sidx_end) {
							gf_filter_pck_set_property(pck, GF_PROP_PCK_SIDX_RANGE, &PROP_FRAC64_INT(finfo.sidx_start , finfo.sidx_end));
						}

						if (read->seg_name_changed) {
							const GF_PropertyValue *p = gf_filter_pid_get_property(read->pid, GF_PROP_PID_URL);
							read->seg_name_changed = GF_FALSE;
							if (p && p->value.string) {
								gf_filter_pck_set_property(pck, GF_PROP_PID_URL, &PROP_STRING(p->value.string));
							}
						}
					}
				}
				if (ch->sender_ntp) {
					gf_filter_pck_set_property(pck, GF_PROP_PCK_SENDER_NTP, &PROP_LONGUINT(ch->sender_ntp));
					if (ch->ntp_at_server_ntp) {
						gf_filter_pck_set_property(pck, GF_PROP_PCK_RECEIVER_NTP, &PROP_LONGUINT(ch->ntp_at_server_ntp));
					}
				}
				ch->eos_sent = GF_FALSE;
				gf_filter_pck_send(pck);
				isor_reader_release_sample(ch);

				ch->last_valid_sample_data_offset = ch->sample_data_offset;
				nb_pck--;
			} else if (ch->last_state==GF_EOS) {
				if (in_is_eos && !ch->eos_sent) {
					ch->eos_sent = GF_TRUE;
					read->eos_signaled = GF_TRUE;
					gf_filter_pid_set_eos(ch->pid);
				}
				break;
			} else {
				read->force_fetch = GF_TRUE;
				break;
			}
		}
		if (!min_offset_plus_one || (min_offset_plus_one - 1 > ch->last_valid_sample_data_offset))
			min_offset_plus_one = 1 + ch->last_valid_sample_data_offset;
	}
	if (read->mem_load_mode && min_offset_plus_one) {
		isoffin_purge_mem(read, min_offset_plus_one-1);
	}

	if (!is_active) {
		return GF_EOS;
	}
	//if (in_is_eos)
	gf_filter_ask_rt_reschedule(filter, 1);
	return GF_OK;

}

static const char *isoffin_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	if (gf_isom_probe_data(data, size)) {
		*score = GF_FPROBE_SUPPORTED;
		return "video/mp4";
	}
	return NULL;
}

#define OFFS(_n)	#_n, offsetof(ISOMReader, _n)

static const GF_FilterArgs ISOFFInArgs[] =
{
	{ OFFS(src), "location of source content", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(allt), "load all tracks even if unknown", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(noedit), "do not use edit lists", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(itt), "convert all items of root meta into a single PID", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(itemid), "keep item IDs in PID properties", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(smode), "load mode for scalable/tile tracks\n"
	"- split: each track is declared, extractors are removed\n"
	"- splitx: each track is declared, extractors are kept\n"
	"- single: a single track is declared (highest level for scalable, tile base for tiling)", GF_PROP_UINT, "split", "split|splitx|single", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(alltk), "declare all tracks even disabled ones", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(frame_size), "frame size for raw audio samples (dispatches frame_size samples per packet)", GF_PROP_UINT, "1024", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(expart), "expose cover art as a dedicated video pid", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(sigfrag), "signal fragment and segment boundaries of source on output packets", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},

	{ OFFS(tkid), "declare only track based on given param"
	"- integer value: declares track with the given ID\n"
	"- audio: declares first audio track\n"
	"- video: declares first video track\n"
	"- 4CC: declares first track with matching 4CC for handler type", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(stsd), "only extract sample mapped to the given sample desciption index. 0 means no filter", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mov), "pointer to a read/edit ISOBMF file used internally by importers and exporters", GF_PROP_POINTER, NULL, NULL, GF_FS_ARG_HINT_HIDE},
	{ OFFS(analyze), "skip reformat of decoder config and SEI and dispatch all NAL in input order - shall only be used with inspect filter analyze mode!", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_HIDE},
	{ OFFS(catseg), "append the given segment to the movie at init time (only local file supported)", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_HIDE},
	{ OFFS(nocrypt), "signal encrypted tracks as non encrypted (mostly used for export)", GF_PROP_BOOL, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(mstore_size), "target buffer size in bytes", GF_PROP_UINT, "1000000", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mstore_purge), "minimum size in bytes between memory purges when reading from memory stream (pipe etc...), 0 means purge as soon as possible", GF_PROP_UINT, "50000", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mstore_samples), "minimum number of samples to be present before purging sample tables when reading from memory stream (pipe etc...), 0 means purge as soon as possible", GF_PROP_UINT, "50", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(strtxt), "load text tracks (apple/tx3g) as MPEG-4 streaming text tracks", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},

	{0}
};

static const GF_FilterCapability ISOFFInCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "mp4|mpg4|m4a|m4i|3gp|3gpp|3g2|3gp2|iso|m4s|heif|heic|avci|mj2|mov|qt"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/x-isomedia|application/mp4|video/mp4|audio/mp4|video/3gpp|audio/3gpp|video/3gp2|audio/3gp2|video/iso.segment|audio/iso.segment|image/heif|image/heic|image/avci|video/quicktime"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_METADATA),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),

	CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	//we don't set output cap for streamtype FILE for now.
};

GF_FilterRegister ISOFFInRegister = {
	.name = "mp4dmx",
	GF_FS_SET_DESCRIPTION("ISOBMFF/QT demuxer")
	GF_FS_SET_HELP("This filter demultiplexes ISOBMF and QT files (regular or fragmented).\n"
		"# Track Selection\n"
		"The filter can use fragment identifiers of source to select a single track for playback. The allowed fragments are:\n"
		" - #audio: only use the first audio track\n"
		" - #video: only use the first video track\n"
		" - #auxv: only use the first auxiliary video track\n"
		" - #pict: only use the first picture track\n"
		" - #text: only use the first text track\n"
		" - #trackID=VAL: only use the track with given ID\n"
		" - #ID=VAL: only use the track with given ID\n"
		" - #VAL: only use the track with given ID\n"
		"\n"
		"# Scalable Tracks\n"
		"When scalable tracks are present in a file, the reader can operate in 3 modes using [-smode]() option:\n"\
	 	"- smode=single: resolves all extractors to extract a single bitstream from a scalable set. The highest level is used\n"\
	 	"In this mode, there is no enhancement decoder config, only a base one resulting from the merge of the configs\n"\
	 	"- smode=split: all extractors are removed and every track of the scalable set is declared. In this mode, each enhancement track has no base decoder config\n"
	 	"and an enhancement decoder config.\n"\
	 	"- smode=splitx: extractors are kept in the bitstream, and every track of the scalable set is declared. In this mode, each enhancement track has a base decoder config\n"
	 	" (copied from base) and an enhancement decoder config. This is mostly used for DASHing content.\n"\
	 	"Warning: smode=splitx will result in extractor NAL units still present in the output bitstream, which shall only be true if the output is ISOBMFF based\n")
	.private_size = sizeof(ISOMReader),
	.args = ISOFFInArgs,
	.initialize = isoffin_initialize,
	.finalize = isoffin_finalize,
	.process = isoffin_process,
	.configure_pid = isoffin_configure_pid,
	SETCAPS(ISOFFInCaps),
	.process_event = isoffin_process_event,
	.probe_data = isoffin_probe_data
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

