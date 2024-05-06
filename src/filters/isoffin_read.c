/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_MP4DMX)

#include <gpac/crypt_tools.h>
#include <gpac/media_tools.h>

enum
{
	EDITS_AUTO=0,
	EDITS_NO,
	EDITS_STRICT
};

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


static GF_Err isoffin_setup(GF_Filter *filter, ISOMReader *read, Bool input_is_eos)
{
	char *url;
	char *tmp, *src;
	GF_Err e;
	const GF_PropertyValue *prop;
	if (!read) return GF_SERVICE_ERROR;

	if (read->pid) {
		prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILEPATH);
		gf_fatal_assert(prop);
		src = prop->value.string;
	} else {
		src = read->src;
	}
	if (!src) return GF_SERVICE_ERROR;

	//if source is a fileIO, check if it is tagged for main thread
	//if so, force main thread for this filter and configure at next process
	if (!strncmp(src, "gfio://", 7) && !read->gfio_probe) {
		read->gfio_probe = GF_TRUE;
		if (gf_fileio_is_main_thread(src)) {
			read->moov_not_loaded = 1;
			gf_filter_force_main_thread(filter, GF_TRUE);
			gf_filter_post_process_task(filter);
			return GF_OK;
		}
	}

	read->src_crc = gf_crc_32(src, (u32) strlen(src));

	url = gf_strdup(src);
	if (!url) return GF_OUT_OF_MEM;
	tmp = gf_file_ext_start(url);
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
			} else if (!strnicmp(tmp, "#itemID=", 8)) {
				read->play_only_track_id = atoi(tmp+8);
			} else if (!strnicmp(tmp, "#ID=", 4)) {
				read->play_only_track_id = atoi(tmp+4);
			} else {
				read->play_only_track_id = atoi(tmp+1);
			}
			if (truncate) tmp[0] = 0;
		}
	}

	if (! isor_is_local(url)) {
		gf_free(url);
		return GF_NOT_SUPPORTED;
	}
	read->start_range = read->end_range = 0;
	prop = read->pid ? gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILE_RANGE) : NULL;
	if (prop) {
		read->start_range = prop->value.lfrac.num;
		read->end_range = prop->value.lfrac.den;
	}

	read->missing_bytes = 0;
	e = gf_isom_open_progressive(url, read->start_range, read->end_range, read->sigfrag, &read->mov, &read->missing_bytes);

	if (e == GF_ISOM_INCOMPLETE_FILE) {
		if (input_is_eos) {
			e = GF_ISOM_INVALID_FILE;
		} else {
			gf_free(url);
			read->moov_not_loaded = 1;
			return GF_OK;
		}
	}

	read->input_loaded = GF_TRUE;
	//if missing bytes is set, file is incomplete, check if cache is complete
	if (read->missing_bytes) {
		read->input_loaded = GF_FALSE;
		prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILE_CACHED);
		if (prop && prop->value.boolean)
			read->input_loaded = GF_TRUE;
	}

	if (e != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] error while opening %s, error=%s\n", url,gf_error_to_string(e)));
		gf_filter_setup_failure(filter, e);
		gf_free(url);
		return e;
	}
	read->frag_type = gf_isom_is_fragmented(read->mov) ? 1 : 0;

	read->timescale = gf_isom_get_timescale(read->mov);
	if (!read->input_loaded && read->frag_type)
		read->refresh_fragmented = GF_TRUE;

	if (read->strtxt)
		gf_isom_text_set_streaming_mode(read->mov, GF_TRUE);

	gf_free(url);
	e = isor_declare_objects(read);
	if (e && (e!= GF_ISOM_INCOMPLETE_FILE)) {
		gf_filter_setup_failure(filter, e);
		e = GF_FILTER_NOT_SUPPORTED;
	}
	return e;
}

static void isoffin_delete_channel(ISOMChannel *ch)
{
	isor_reset_reader(ch);
	if (ch->avcc) gf_odf_avc_cfg_del(ch->avcc);
	if (ch->hvcc) gf_odf_hevc_cfg_del(ch->hvcc);
	if (ch->vvcc) gf_odf_vvc_cfg_del(ch->vvcc);
	gf_free(ch);
}

static void isoffin_disconnect(ISOMReader *read)
{
	read->disconnected = GF_TRUE;
	while (gf_list_count(read->channels)) {
		ISOMChannel *ch = (ISOMChannel *)gf_list_get(read->channels, 0);
		gf_list_rem(read->channels, 0);
		if (ch->pid)
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
//	GF_ISOTrackID trackID;
	GF_ISOSegOpenMode flags=0;
	GF_Err e;

	prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILE_CACHED);
	if (prop && prop->value.boolean)
		read->input_loaded = GF_TRUE;

	read->refresh_fragmented = GF_FALSE;
	read->full_segment_flush = GF_TRUE;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] reconfigure triggered, URL %s\n", next_url));

	//no need to lock blob if next_url is a blob, all parsing and probing functions below will lock the blob if any

	switch (gf_isom_probe_file_range(next_url, read->start_range, read->end_range)) {
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	//this is a fragment
	case 3:
		gf_isom_release_segment(read->mov, 1);
		gf_isom_reset_fragment_info(read->mov, GF_TRUE);

		if (read->no_order_check) flags |= GF_ISOM_SEGMENT_NO_ORDER_FLAG;

		//no longer used in filters
#if 0
		if (scalable_segment) flags |= GF_ISOM_SEGMENT_SCALABLE_FLAG;
#endif
		e = gf_isom_open_segment(read->mov, next_url, read->start_range, read->end_range, flags);
		if (!read->input_loaded && (e==GF_ISOM_INCOMPLETE_FILE)) {
			e = GF_OK;
		}
		//always refresh fragmented files, since we could have a full moof+mdat in buffer (not incomplete file)
		//but still further fragments to be pushed
		if (!read->start_range && !read->end_range)
			read->refresh_fragmented = GF_TRUE;

		for (i=0; i<gf_list_count(read->channels); i++) {
			ISOMChannel *ch = gf_list_get(read->channels, i);
			if (ch->last_state==GF_EOS)
				ch->last_state=GF_OK;

			//if we have an init seg set, set sample count
			if (read->initseg) {
				u32 nb_samples = gf_isom_get_sample_count(read->mov, ch->track);
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_NB_FRAMES, &PROP_UINT(nb_samples));
			}
		}

#ifndef GPAC_DISABLE_LOG
		if (e<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[IsoMedia] Error opening new segment %s at UTC "LLU": %s\n", next_url, gf_net_get_utc(), gf_error_to_string(e) ));
		} else if (read->end_range) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Playing new range in %s: "LLU"-"LLU"\n", next_url, read->start_range, read->end_range));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] playing new segment %s\n", next_url));
		}
#endif
		break;
#endif // GPAC_DISABLE_ISOM_FRAGMENTS
	//this is a movie, reload
	case 2:
	case 1:
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		//get tfdt of next segment (cumulated sample dur since moov load)
		//if the next segment has a tfdt or a tfrx, this will be ignored
		//otherwise this value will be used as base tfdt for next segment
		tfdt = gf_isom_get_smooth_next_tfdt(read->mov, 1);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Switching between files - opening new init segment %s (time offset="LLU") - range "LLU"-"LLU"\n", next_url, tfdt, read->start_range, read->end_range));

		if (gf_isom_is_smooth_streaming_moov(read->mov)) {
			char *tfdt_val = strstr(next_url, "tfdt=");
			//smooth addressing, replace tfdt=0000000000000000 with proper value
			if (tfdt_val) {
				sprintf(tfdt_val+5, LLX, tfdt);
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[IsoMedia] Error finding init time for init segment %s at UTC "LLU"\n", next_url, gf_net_get_utc() ));
			}
		}
#endif
		if (read->mov) gf_isom_close(read->mov);
		e = gf_isom_open_progressive(next_url, read->start_range, read->end_range, read->sigfrag, &read->mov, &read->missing_bytes);

		//init seg not completely downloaded, retry at next packet
		if (!read->input_loaded && (e==GF_ISOM_INCOMPLETE_FILE)) {
			read->src_crc = 0;
			read->moov_not_loaded = 2;
			return GF_OK;
		}

		read->moov_not_loaded = 0;
		if (e < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[IsoMedia] Error opening init segment %s at UTC "LLU": %s\n", next_url, gf_net_get_utc(), gf_error_to_string(e) ));
		}
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		if (read->sigfrag)
			gf_isom_enable_traf_map_templates(read->mov);
#endif
		is_new_mov = GF_TRUE;
		break;
	//empty file
	case 4:
		return GF_OK;
	default:
		if (!read->mov) {
            return GF_NOT_SUPPORTED;
		}
        e = GF_ISOM_INVALID_FILE;
        break;
	}

	gf_filter_post_process_task(filter);

	count = gf_list_count(read->channels);

	if (e<0) {
		count = gf_list_count(read->channels);
        read->invalid_segment = GF_TRUE;
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		gf_isom_release_segment(read->mov, 1);
		//error opening the segment, reset everything ...
		gf_isom_reset_fragment_info(read->mov, GF_FALSE);
#endif
		for (i=0; i<count; i++) {
			ISOMChannel *ch = gf_list_get(read->channels, i);
            if (ch) {
                ch->sample_num = 0;
                ch->eos_sent = 0;
            }
		}
        GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[IsoMedia] Error opening current segment %s: %s\n", next_url, gf_error_to_string(e) ));
		return GF_OK;
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
		ch->eos_sent = 0;

		//old code from master, currently no longer used
		//in filters we don't use extractors for the time being, we only do implicit reconstruction at the decoder side
#if 0
		if (ch->base_track) {
			if (scalable_segment)
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
#endif

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
	if (read->initseg) {
		read->pid = pid;
	}
	//check if we have a file path; if not, this is a pure stream of boxes (no local file cache)
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
	if (!prop || !prop->value.string) {
		if (!read->mem_load_mode)
			read->mem_load_mode = 1;
		if (!read->pid) read->pid = pid;
		read->input_loaded = GF_FALSE;
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
	return isoffin_setup(filter, read, GF_FALSE);
}

GF_Err isoffin_initialize(GF_Filter *filter)
{
	ISOMReader *read = gf_filter_get_udta(filter);
	GF_Err e = GF_OK;
	read->filter = filter;
	read->channels = gf_list_new();

	if (read->xps_check==MP4DMX_XPS_AUTO) {
		read->xps_check = (read->smode==MP4DMX_SPLIT_EXTRACTORS) ? MP4DMX_XPS_KEEP : MP4DMX_XPS_REMOVE;
	}

	if (read->src) {
		read->input_loaded = GF_TRUE;
		return isoffin_setup(filter, read, GF_TRUE);
	}
	else if (read->mov) {
		read->extern_mov = GF_TRUE;
		read->input_loaded = GF_TRUE;
		read->frag_type = gf_isom_is_fragmented(read->mov) ? 1 : 0;
		read->timescale = gf_isom_get_timescale(read->mov);

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		if (read->sigfrag) {
			gf_isom_enable_traf_map_templates(read->mov);
		}
		if (read->catseg) {
			e = gf_isom_open_segment(read->mov, read->catseg, 0, 0, 0);
		}
#endif
		if (!e)
			e = isor_declare_objects(read);

		gf_filter_post_process_task(filter);
	}
	else if (read->initseg) {
		read->src = read->initseg;
		e = isoffin_setup(filter, read, GF_TRUE);
		read->src = NULL;
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
	if (read->mem_url) {
		gf_blob_unregister(&read->mem_blob);
		gf_free(read->mem_url);
	}
}

void isor_declare_pssh(ISOMChannel *ch)
{
	u32 i, PSSH_count;
	u8 *psshd;
	GF_BitStream *pssh_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	u32 s;

	PSSH_count = gf_isom_get_pssh_count(ch->owner->mov);
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
		if (version) {
			gf_bs_write_u32(pssh_bs, KID_count);
			for (s=0; s<KID_count; s++) {
				gf_bs_write_data(pssh_bs, KIDs[s], 16);
			}
		}
		gf_bs_write_u32(pssh_bs, private_data_size);
		gf_bs_write_data(pssh_bs, private_data, private_data_size);
	}
	gf_bs_get_content(pssh_bs, &psshd, &s);
	gf_bs_del(pssh_bs);
	gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_PSSH, & PROP_DATA_NO_COPY(psshd, s) );
}

void isor_set_crypt_config(ISOMChannel *ch)
{
	GF_ISOFile *mov = ch->owner->mov;
	u32 track = ch->track;
	u32 scheme_type, scheme_version, i, count;
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
		ch->is_cenc = 1;

		gf_isom_get_cenc_info(ch->owner->mov, ch->track, stsd_idx, NULL, &scheme_type, &scheme_version);

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

	gf_filter_pid_set_property(ch->pid, GF_PROP_PID_PROTECTION_SCHEME_TYPE, &PROP_4CC(scheme_type) );
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
		const u8 *key_info;
		u32 key_info_size;
		u32 container_type;

		isor_declare_pssh(ch);

		gf_isom_cenc_get_default_info(ch->owner->mov, ch->track, stsd_idx, &container_type, &ch->pck_encrypted, &ch->crypt_byte_block, &ch->skip_byte_block, &key_info, &key_info_size);

		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_STORE, &PROP_4CC(container_type) );

		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ENCRYPTED, &PROP_BOOL(ch->pck_encrypted) );

		if (ch->skip_byte_block || ch->crypt_byte_block) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_PATTERN, &PROP_FRAC_INT(ch->skip_byte_block, ch->crypt_byte_block) );
		}
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_KEY_INFO, &PROP_DATA((u8 *)key_info, key_info_size) );
		ch->key_info_crc = gf_crc_32(key_info, key_info_size);
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
	ch->to_init = 1;
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
	if (read->edits==EDITS_NO) {
		ch->has_edit_list = 0;
	} else if (read->edits==EDITS_AUTO) {
		ch->ts_offset = 0;
		ch->has_edit_list = gf_isom_get_edit_list_type(ch->owner->mov, ch->track, &ch->ts_offset) ? 1 : 0;
		if (!ch->has_edit_list && ch->ts_offset) {
			//if >0 this is a hold, we signal positive delay
			//if <0 this is a skip, we signal negative delay
			gf_filter_pid_set_property(pid, GF_PROP_PID_DELAY, &PROP_LONGSINT( ch->ts_offset) );
		}
	} else {
		if (gf_isom_get_edits_count(ch->owner->mov, ch->track))
			ch->has_edit_list = 1;
		else
			ch->has_edit_list = 0;
	}

	ch->has_rap = (gf_isom_has_sync_points(ch->owner->mov, ch->track)==1) ? 1 : 0;
	gf_filter_pid_set_property(pid, GF_PROP_PID_HAS_SYNC, &PROP_BOOL(ch->has_rap) );
	//some fragmented files do not advertize a sync sample table (legal) so we need to update as soon as we fetch a fragment
	//to see if we are all-intra (as detected here) or not
	if (!ch->has_rap && ch->owner->frag_type)
		ch->check_has_rap = 1;
	ch->timescale = gf_isom_get_media_timescale(ch->owner->mov, ch->track);

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
		ch->is_encrypted = 0;
		return ch;
	}
	ch->is_encrypted = 1;
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
		if (!next_track) {
			ch->playing = 1;
			ref_count = gf_isom_get_reference_count(the_file, ch->track, GF_ISOM_REF_BASE);
			trackID = 0;
			if (ref_count) {
				gf_isom_get_reference(the_file, ch->track, GF_ISOM_REF_BASE, 1, &trackID);
				for (i=0; i<gf_list_count(ch->owner->channels) && trackID; i++) {
					ISOMChannel *base = gf_list_get(ch->owner->channels, i);
					if (base->track_id==trackID) {
						u32 sample_desc_index;
						u64 resume_at;
						GF_Err e;
						//try to locate sync after current time in base
						resume_at = base->static_sample ? gf_timestamp_rescale(base->static_sample->DTS, base->timescale, ch->timescale) : 0;
						e = gf_isom_get_sample_for_media_time(ch->owner->mov, ch->track, resume_at, &sample_desc_index, GF_ISOM_SEARCH_SYNC_FORWARD, &ch->static_sample, &ch->sample_num, &ch->sample_data_offset);
						//found, rewind so that next fetch is the sync
						if (e==GF_OK) {
							ch->sample = NULL;
						}
						//no further sync found, realign with base timescale
						else if (e==GF_EOS) {
							e = gf_isom_get_sample_for_media_time(ch->owner->mov, ch->track, resume_at, &sample_desc_index, GF_ISOM_SEARCH_FORWARD, &ch->static_sample, &ch->sample_num, &ch->sample_data_offset);
						}
						//unknown state, realign sample num with base
						if (e<0) {
							ch->sample_num = base->sample_num;
						}
						break;
					}
				}
			}
			return cur_track;
		}
	} else {
		if (cur_track == ch->base_track)
			return cur_track;
		ref_count = gf_isom_get_reference_count(the_file, cur_track, GF_ISOM_REF_SCAL);
		if (ref_count <= 0)
			return cur_track;
		gf_isom_get_reference(the_file, cur_track, GF_ISOM_REF_SCAL, ref_count, &next_track);
		if (!next_track)
			return cur_track;

		if (ch->track != next_track) {
			ch->playing = 0;
			ch->eos_sent = 1;
			gf_filter_pid_set_eos(ch->pid);
		}
	}

	/*in scalable mode add SPS/PPS in-band*/
	if (ch->owner->smode)
		gf_isom_set_nalu_extract_mode(the_file, next_track, ch->nalu_extract_mode);

	return next_track;
}

static Bool isoffin_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 count, i;
	Bool is_byte_range;
	Bool cancel_event = GF_TRUE;
	Double start_range, speed;
	ISOMChannel *ch, *ref_ch;
	ISOMReader *read = gf_filter_get_udta(filter);

	if (!read || read->disconnected) return GF_FALSE;

	if (evt->base.type == GF_FEVT_QUALITY_SWITCH) {
		count = gf_list_count(read->channels);
		for (i = 0; i < count; i++) {
			ch = (ISOMChannel *)gf_list_get(read->channels, i);
			if (ch->base_track && gf_isom_needs_layer_reconstruction(read->mov)) {
				/*ch->next_track = */ //old code, see not in isoffin_reconfigure
				isoffin_channel_switch_quality(ch, read->mov, evt->quality_switch.up);
			}
		}
		return GF_TRUE;
	}

	if (!evt->base.on_pid) return GF_FALSE;

	ch = isor_get_channel(read, evt->base.on_pid);
	if (!ch)
		return GF_FALSE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (ch->skip_next_play) {
			ch->skip_next_play = 0;
			return GF_TRUE;
		}
		is_byte_range = (evt->play.hint_start_offset || evt->play.hint_end_offset) ? GF_TRUE : GF_FALSE;

		isor_reset_reader(ch);
		ch->eos_sent = 0;
		ch->speed = is_byte_range ? 1 : evt->play.speed;
		ch->initial_play_seen = 1;
		read->reset_frag_state = 1;
		//it can happen that input_is_stop is still TRUE because we did not get called back after the stop - reset to FALSE since we now play
		read->input_is_stop = GF_FALSE;
		if (read->frag_type)
			read->frag_type = 1;

		ch->start = ch->end = 0;
		start_range = is_byte_range ? 0 : evt->play.start_range;
		speed = ch->speed;
		//compute closest range if channel was disconnected
		if (read->nb_playing && ch->midrun_tune) {
			ref_ch = NULL;
			count = gf_list_count(read->channels);
			for (i = 0; i < count; i++) {
				ISOMChannel *ach = (ISOMChannel *)gf_list_get(read->channels, i);
				if (!ach->playing) continue;
				//check sync ID if multiple timelines
				if (ach->clock_id != ch->clock_id) continue;
				ref_ch = ach;
				break;
			}
			//we have a ref, if computed last sample clock is 1s greater than start range of channel, use current time
			if (ref_ch && ref_ch->timescale) {
				Double diff, orig_range = ref_ch->orig_start;
				speed = ref_ch->speed;
				if (ref_ch->has_edit_list) {
					start_range = (Double) ref_ch->sample_time;
				} else {
					start_range = (Double) ref_ch->cts;
				}
				start_range /= ref_ch->timescale;
				diff = orig_range - start_range;
				if (ABS(diff)<1.0)
					start_range = orig_range;
			}
		}
		if (!is_byte_range) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[IsoMedia] channel start is %f - requested %f\n", start_range, evt->play.start_range));

			if (speed>=0) {
				Double t;
				if (start_range>=0) {
					t = start_range;
					t *= ch->timescale;
					ch->start = (u64) t;
				}
				if (evt->play.end_range >= start_range) {
					ch->end = (u64) -1;
					if (evt->play.end_range<FLT_MAX) {
						t = evt->play.end_range;
						t *= ch->timescale;
						ch->end = (u64) t;
					}
				}
			} else {
				Double end = evt->play.end_range;
				if (end==-1) end = 0;
				ch->start = (u64) (s64) (start_range * ch->timescale);
				if (end <= start_range)
					ch->end = (u64) (s64) (end  * ch->timescale);
			}
			ch->sample_num = evt->play.from_pck;
			ch->sample_last = evt->play.to_pck;
			ch->sap_only = evt->play.drop_non_ref ? 1 : 0;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[IsoMedia] Starting channel playback "LLD" to "LLD" (%g to %g)\n", ch->start, ch->end, start_range, evt->play.end_range));
		} else {
			ch->end = 0;
			ch->sample_num = 0;
			ch->sample_last = 0;
			ch->sap_only = 0;
		}
		ch->orig_start = start_range;
		ch->playing = 1;

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		if (!read->nb_playing)
			gf_isom_reset_seq_num(read->mov);
#endif
		if (read->is_partial_download) read->input_loaded = GF_FALSE;

		if (is_byte_range) {
			if (!read->frag_type) {
				return GF_TRUE;
			}
			if (read->mem_load_mode) {
				GF_FilterEvent fevt;
				//new segment will be loaded, reset
				gf_isom_reset_tables(read->mov, GF_TRUE);
				gf_isom_reset_data_offset(read->mov, NULL);
				read->refresh_fragmented = GF_TRUE;
				read->mem_blob.size = 0;

				read->bytes_removed = evt->play.hint_start_offset;

				//send a seek request
				read->is_partial_download = GF_TRUE;
				read->wait_for_source = GF_TRUE;
				read->refresh_fragmented = GF_TRUE;

				GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, read->pid);
				fevt.seek.start_offset = read->bytes_removed;
				gf_filter_pid_send_event(read->pid, &fevt);

				gf_isom_set_removed_bytes(read->mov, read->bytes_removed);
				gf_isom_set_byte_offset(read->mov, 0);
			} else {
				GF_Err gf_isom_load_fragments(GF_ISOFile *movie, u64 start_range, u64 end_range, u64 *BytesMissing);
				gf_isom_load_fragments(read->mov, evt->play.hint_start_offset, evt->play.hint_end_offset, &read->missing_bytes);
				ch->hint_first_tfdt = evt->play.hint_first_dts;
			}
		} else if (evt->play.no_byterange_forward) {
			//new segment will be loaded, reset
			gf_isom_reset_tables(read->mov, GF_TRUE);
			gf_isom_reset_data_offset(read->mov, NULL);
			read->refresh_fragmented = GF_TRUE;
			read->mem_blob.size = 0;
			//send play event
			cancel_event = GF_FALSE;
		} else if (!read->nb_playing && read->pid && !read->input_loaded) {
			GF_FilterEvent fevt;
			Bool is_sidx_seek = GF_FALSE;
			u64 max_offset = GF_FILTER_NO_BO;
			count = gf_list_count(read->channels);

			//try sidx
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
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
						dur = gf_timestamp_rescale(dur, ts, read->timescale);
						if (ch->duration != dur) {
							ch->duration = dur;
							gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DURATION, &PROP_FRAC64_INT(ch->duration, read->timescale));
						}
					}
				}
			}
#endif
			if (!is_sidx_seek) {
				for (i=0; i< count; i++) {
					u32 mode, sample_desc_index, sample_num;
					u64 data_offset;
					GF_Err e;
					u64 time;
					ch = gf_list_get(read->channels, i);
					mode = ch->disable_seek ? GF_ISOM_SEARCH_BACKWARD : GF_ISOM_SEARCH_SYNC_BACKWARD;
					time = (u64) (evt->play.start_range * ch->timescale);

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

				if (read->mem_load_mode) {
					read->mem_blob.size = 0;
					read->bytes_removed = max_offset;
					gf_isom_set_removed_bytes(read->mov, read->bytes_removed);
					gf_isom_set_byte_offset(read->mov, 0);
				} else {
					gf_isom_set_byte_offset(read->mov, is_sidx_seek ? 0 : max_offset);
				}
			}
		}


		read->nb_playing++;
		//trigger play on all "disconnected" channels
		if ((read->nb_playing>1) && !ch->midrun_tune) {
			count = gf_list_count(read->channels);
			for (i=0; i<count;i++) {
				ISOMChannel *ach = gf_list_get(read->channels, i);
				//locate disconnected channels playing
				if (ach == ch) continue;
				if (ach->clock_id != ch->clock_id) continue;
				if (!ach->playing || !ach->timescale || !ach->midrun_tune) continue;
				isor_reset_reader(ach);
				ach->playing = 1;
				ach->sample_num = 0;
				ach->start = gf_timestamp_rescale(ch->start, ch->timescale, ach->timescale);
				ach->orig_start = ch->orig_start;
				ach->start = (u64) (ach->orig_start * ach->timescale);
				ach->eos_sent = 0;
				ach->speed = is_byte_range ? 1 : evt->play.speed;
				ach->initial_play_seen = 1;
				ach->skip_next_play = 1;
				ach->set_disc = 1;
			}
		}

		//always request a process task upon a play
		gf_filter_post_process_task(read->filter);
		//cancel event unless dash mode
		return cancel_event;

	case GF_FEVT_STOP:
 		if (read->nb_playing) read->nb_playing--;
		isor_reset_reader(ch);

		//stop is due to a deconnection, mark channel as not active
		if (evt->play.initial_broadcast_play==2)
			ch->midrun_tune = 1;
		else
			ch->midrun_tune = 0;

		//don't send a stop if some of our channels are still waiting for initial play
		for (i=0; i<gf_list_count(read->channels); i++) {
			ISOMChannel *a_ch = gf_list_get(read->channels, i);
			if (ch==a_ch) continue;
			if (!a_ch->initial_play_seen) return GF_TRUE;
		}
		ch->skip_next_play = 0;
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
			ch->sap_only = evt->play.drop_non_ref ? 1 : 0;
		}
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event
	return GF_FALSE;
}

static void isoffin_push_buffer(GF_Filter *filter, ISOMReader *read, const u8 *pck_data, u32 data_size)
{
	u64 bytes_missing;
	GF_Err e;

	if (!read->mem_url) {
		read->mem_url = gf_blob_register(&read->mem_blob);
	}
	if (read->mem_blob_alloc < read->mem_blob.size + data_size) {
		read->mem_blob.data = gf_realloc(read->mem_blob.data, read->mem_blob.size + data_size);
		read->mem_blob_alloc = read->mem_blob.size + data_size;
	}
	memcpy(read->mem_blob.data + read->mem_blob.size, pck_data, data_size);
	read->mem_blob.size += data_size;

	if (read->mem_load_mode==1) {
		u32 box_type;
		e = gf_isom_open_progressive_ex(read->mem_url, 0, 0, read->sigfrag, &read->mov, &bytes_missing, &box_type);

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
				read->moov_not_loaded = 1;
				break;
			}
			return;
		}

		read->frag_type = gf_isom_is_fragmented(read->mov) ? 1 : 0;
		read->timescale = gf_isom_get_timescale(read->mov);
		isor_declare_objects(read);
		read->mem_load_mode = 2;
		read->moov_not_loaded = 0;
		return;
	}
	//refresh file
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	e = gf_isom_refresh_fragmented(read->mov, &bytes_missing, read->mem_url);
	if (e && (e!=GF_ISOM_INCOMPLETE_FILE)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] Failed to refresh fragmented file after buffer push: %s\n", gf_error_to_string(e)));
	}
#endif
	if ((read->mem_load_mode==2) && bytes_missing)
		read->force_fetch = GF_TRUE;

}

static void isoffin_purge_mem(ISOMReader *read, u64 min_offset)
{
	u32 i, count;
	u64 top_offset;
	u32 nb_bytes_to_purge;
	u64 bytes_missing;

	//purge every mstore_purge bytes
	if (read->mstore_purge && (min_offset - read->last_min_offset < read->mstore_purge))
		return;

	if (read->frag_type) {
		//get position of current box being parsed - if new offset is greater than this box we cannot remove
		//bytes (we would trash the top-level box header)
		gf_isom_get_current_top_box_offset(read->mov, &top_offset);
		if (top_offset<min_offset) {
			//force loading more data - we usually get here when mdat is not completely loaded
			read->force_fetch = GF_TRUE;
			return;
		}
	}
	read->last_min_offset = min_offset;

	gf_assert(min_offset>=read->bytes_removed);
	//min_offset is given in absolute file position
	nb_bytes_to_purge = (u32) (min_offset - read->bytes_removed);
	gf_assert(nb_bytes_to_purge<=read->mem_blob.size);
	if (!nb_bytes_to_purge) {
		read->force_fetch = GF_TRUE;
		return;
	}
	memmove(read->mem_blob.data, read->mem_blob.data+nb_bytes_to_purge, read->mem_blob.size - nb_bytes_to_purge);
	read->mem_blob.size -= nb_bytes_to_purge;
	read->bytes_removed += nb_bytes_to_purge;
	gf_isom_set_removed_bytes(read->mov, read->bytes_removed);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[IsoMedia] mem mode %d bytes in mem, "LLU" bytes trashed since start\n", read->mem_blob.size, read->bytes_removed));

	//force a refresh
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	gf_isom_refresh_fragmented(read->mov, &bytes_missing, read->mem_url);
#endif
	if (!read->frag_type)
		return;

	//fragmented file, cleanup sample tables
	count = gf_list_count(read->channels);
	for (i=0; i<count; i++) {
		ISOMChannel *ch = gf_list_get(read->channels, i);
		u32 num_samples;
		u32 prev_samples = gf_isom_get_sample_count(read->mov, ch->track);
		//don't run this too often
		if (ch->sample_num<=1+read->mstore_samples) continue;

		num_samples = ch->sample_num-1;
		if (num_samples>=prev_samples) continue;

		if (gf_isom_purge_samples(read->mov, ch->track, num_samples) == GF_OK)
			ch->sample_num = 1;

		num_samples = gf_isom_get_sample_count(read->mov, ch->track);
		gf_assert(ch->sample_num<=num_samples);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[IsoMedia] mem mode %d samples now in track %d (prev %d)\n", num_samples, ch->track_id, prev_samples));
	}
}

static GF_Err isoffin_process(GF_Filter *filter)
{
	ISOMReader *read = gf_filter_get_udta(filter);
	u32 i, count = gf_list_count(read->channels);
	Bool is_active = GF_FALSE;
	Bool in_is_eos = GF_FALSE;
	Bool in_is_flush = GF_FALSE;
	Bool check_forced_end = GF_FALSE;
	Bool has_new_data = GF_FALSE;
	u64 min_offset_plus_one = 0;
	u32 nb_forced_end=0;
	if (read->in_error)
		return read->in_error;

	if (read->pid) {
		Bool fetch_input = GF_TRUE;

		//we failed at loading the init segment during a dash switch, retry
		if (!read->is_partial_download && !read->mem_load_mode && (read->moov_not_loaded==2) ) {
			isoffin_configure_pid(filter, read->pid, GF_FALSE);
			if (read->moov_not_loaded) return GF_OK;
		}
		if (read->mem_load_mode==2) {
			if (!read->force_fetch && (read->mem_blob.size > read->mstore_size)) {
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
			//we just had a switch but init seg is not completely done: input packet is only a part of the init, drop it
			else if (read->moov_not_loaded==2) {
				gf_filter_pid_drop_packet(read->pid);
				return GF_OK;
			}
			gf_filter_pid_drop_packet(read->pid);
			has_new_data = GF_TRUE;
			if (read->in_error)
				return read->in_error;
		}
		if (gf_filter_pid_is_eos(read->pid)) {
			if (!gf_filter_pid_is_flush_eos(read->pid)) {
				read->input_loaded = GF_TRUE;
				in_is_eos = GF_TRUE;
			} else {
				in_is_flush = GF_TRUE;
			}
		}
		if (read->input_is_stop) {
			read->input_loaded = GF_TRUE;
			in_is_eos = GF_TRUE;
			read->input_is_stop = GF_FALSE;
		}
		if (!read->frag_type && read->input_loaded) {
			in_is_eos = GF_TRUE;
		}
        //segment is invalid, wait for eos on input an send eos on all channels
        if (read->invalid_segment) {
            if (!in_is_eos) return GF_OK;
            read->invalid_segment = GF_FALSE;

            for (i=0; i<count; i++) {
                ISOMChannel *ch = gf_list_get(read->channels, i);
                if (!ch->playing) {
                    continue;
                }
                if (!ch->eos_sent) {
                    ch->eos_sent = 1;
                    gf_filter_pid_set_eos(ch->pid);
                }
            }
            read->eos_signaled = GF_TRUE;
            return GF_EOS;
        }
	} else if (read->extern_mov) {
		in_is_eos = GF_TRUE;
		read->input_loaded = GF_TRUE;
	}
	if (read->moov_not_loaded==1) {
		if (read->mem_load_mode)
			return GF_OK;
		read->moov_not_loaded = GF_FALSE;
		return isoffin_setup(filter, read, in_is_eos);
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

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
			e = gf_isom_refresh_fragmented(read->mov, &bytesMissing, new_url);

			if (e && (e != GF_ISOM_INCOMPLETE_FILE)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[IsoMedia] Failed to refresh current segment: %s\n", gf_error_to_string(e) ));
				read->refresh_fragmented = GF_FALSE;
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Refreshing current segment at UTC "LLU" - "LLU" bytes still missing - input is EOS %d\n", gf_net_get_utc(), bytesMissing, in_is_eos));
			}
#endif

			if (!read->refresh_fragmented && (e==GF_ISOM_INCOMPLETE_FILE)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[IsoMedia] Incomplete Segment received - "LLU" bytes missing but EOF found\n", bytesMissing ));
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
			nb_forced_end++;
			continue;
		}
		//eos not sent on this channel, we are active
		if (!ch->eos_sent)
			is_active = GF_TRUE;

		while (nb_pck) {
			ch->sample_data_offset = 0;
			if (!in_is_flush && !read->full_segment_flush && gf_filter_pid_would_block(ch->pid) )
				break;

			if (ch->item_id) {
				isor_reader_get_sample_from_item(ch);
			} else {
				isor_reader_get_sample(ch);
			}
			if (!ch->sample && ch->pck) {
				gf_filter_pck_discard(ch->pck);
				ch->pck = NULL;
				if (ch->static_sample) {
					ch->static_sample->data = NULL;
					ch->static_sample->alloc_size=0;
				}
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
					ch->needs_pid_reconfig = 0;
				}

				//we have at least two samples, update GF_PROP_PID_HAS_SYNC if needed
				if (ch->check_has_rap && (gf_isom_get_sample_count(ch->owner->mov, ch->track)>1) && (gf_isom_has_sync_points(ch->owner->mov, ch->track)==1)) {
					ch->check_has_rap = 0;
					ch->has_rap = 1;
					gf_filter_pid_set_property(ch->pid, GF_PROP_PID_HAS_SYNC, &PROP_BOOL(ch->has_rap) );
				}

				//strip param sets from payload, trigger reconfig if needed
				isor_reader_check_config(ch);

				if (ch->pck) {
					pck = ch->pck;
					ch->pck = NULL;
					gf_filter_pck_check_realloc(pck, ch->sample->data, ch->sample->dataLength);
					ch->static_sample->data = NULL;
					ch->static_sample->dataLength = 0;
					ch->static_sample->alloc_size=0;
				}
				else if (read->nodata) {
					if (read->nodata==1)
						pck = gf_filter_pck_new_shared(ch->pid, NULL, ch->sample->dataLength, NULL);
					else
						pck = gf_filter_pck_new_alloc(ch->pid, ch->sample->dataLength, &data);

					if (!pck) return GF_OUT_OF_MEM;
				} else {
					pck = gf_filter_pck_new_alloc(ch->pid, ch->sample->dataLength, &data);
					if (!pck) return GF_OUT_OF_MEM;

					if (ch->sample->data)
						memcpy(data, ch->sample->data, ch->sample->dataLength);
				}
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

				sample_dur = ch->sample->duration;
				if (ch->sample->nb_pack)
					sample_dur *= ch->sample->nb_pack;
				gf_filter_pck_set_duration(pck, sample_dur);
				gf_filter_pck_set_seek_flag(pck, ch->seek_flag);

				//for now we only signal xPS mask for non-sap
				if (ch->xps_mask && !gf_filter_pck_get_sap(pck) ) {
					gf_filter_pck_set_property(pck, GF_PROP_PCK_XPS_MASK, &PROP_UINT(ch->xps_mask) );
				}

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

				if (ch->sai_buffer && ch->sai_buffer_size && ch->pck_encrypted) {
					gf_filter_pck_set_property(pck, GF_PROP_PCK_CENC_SAI, &PROP_DATA(ch->sai_buffer, ch->sai_buffer_size) );
				}

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
				if (read->sigfrag) {
					GF_ISOFragmentBoundaryInfo finfo;
					if (gf_isom_sample_is_fragment_start(read->mov, ch->track, ch->sample_num, &finfo) ) {
						u64 start=0;
						u32 traf_start = finfo.seg_start_plus_one ? 2 : 1;

						if (finfo.seg_start_plus_one)
							gf_filter_pck_set_property(pck, GF_PROP_PCK_CUE_START, &PROP_BOOL(GF_TRUE));

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

						gf_filter_pck_set_property(pck, GF_PROP_PCK_FRAG_TFDT, &PROP_LONGUINT(finfo.first_dts));
					}
				}
#endif
				if (ch->sender_ntp) {
					gf_filter_pck_set_property(pck, GF_PROP_PCK_SENDER_NTP, &PROP_LONGUINT(ch->sender_ntp));
					if (ch->ntp_at_server_ntp) {
						gf_filter_pck_set_property(pck, GF_PROP_PCK_RECEIVER_NTP, &PROP_LONGUINT(ch->ntp_at_server_ntp));
					}
				}
				ch->eos_sent = 0;
				ch->nb_empty_retry = 0;

				//this might not be the true end of stream
				if ((ch->streamType==GF_STREAM_AUDIO) && (ch->sample_num == gf_isom_get_sample_count(read->mov, ch->track))) {
					gf_filter_pck_set_property(pck, GF_PROP_PCK_END_RANGE, &PROP_BOOL(GF_TRUE));
				}

				if (!ch->item_id) {
					isor_set_sample_groups_and_aux_data(read, ch, pck);
				}
				if (ch->sample_data_offset && !gf_sys_is_test_mode())
					gf_filter_pck_set_byte_offset(pck, ch->sample_data_offset);

				if (ch->set_disc) {
					ch->set_disc = 0;
					gf_filter_pck_set_clock_type(pck, GF_FILTER_CLOCK_PCR_DISC);
				}
				gf_filter_pck_send(pck);
				isor_reader_release_sample(ch);

				ch->last_valid_sample_data_offset = ch->sample_data_offset;
				if (!in_is_flush)
					nb_pck--;
			} else if (ch->last_state==GF_EOS) {
				if (in_is_flush) {
					gf_filter_pid_send_flush(ch->pid);
				}
				else if (ch->playing == 2) {
					if (in_is_eos) {
						ch->playing = 0;
					} else {
						nb_forced_end++;
						check_forced_end = GF_TRUE;
					}
				}
				ch->nb_empty_retry++;
				if (in_is_eos && !ch->eos_sent) {
					void *tfrf;
					const void *gf_isom_get_tfrf(GF_ISOFile *movie, u32 trackNumber);

					ch->eos_sent = 1;
					read->eos_signaled = GF_TRUE;

					tfrf = (void *) gf_isom_get_tfrf(read->mov, ch->track);
					if (tfrf) {
						gf_filter_pid_set_info_str(ch->pid, "smooth_tfrf", &PROP_POINTER(tfrf) );
						ch->last_has_tfrf = 1;
					} else if (ch->last_has_tfrf) {
						gf_filter_pid_set_info_str(ch->pid, "smooth_tfrf", NULL);
						ch->last_has_tfrf = 0;
					}

					gf_filter_pid_set_eos(ch->pid);
				}
				break;
			} else if (ch->last_state==GF_ISOM_INVALID_FILE) {
				ch->nb_empty_retry++;
				if (!ch->eos_sent) {
					ch->eos_sent = 1;
					read->eos_signaled = GF_TRUE;
					gf_filter_pid_set_eos(ch->pid);
				}
				return ch->last_state;
			} else {
				if ((ch->last_state==GF_OK) && ch->sap_only)
					gf_filter_ask_rt_reschedule(filter, 1);

				read->force_fetch = GF_TRUE;
				ch->nb_empty_retry++;
				break;
			}
		}
		//if no sample fetched for 100 calls, consider no sample for this track and don't use it for memory purge
		//this is typically needed when some tracks are declared in fragmented mode but not present in the stream (at all or for a long time):
		//for these tracks, min_offset_plus_one is always 1 (no samples) or a much smaller value than for active tracks
		// hence forever growing mem storage until stuck at max size...
		if ((ch->nb_empty_retry<100)
			&& (!min_offset_plus_one || (min_offset_plus_one - 1 > ch->last_valid_sample_data_offset))
		) {
			min_offset_plus_one = 1 + ch->last_valid_sample_data_offset;
		}
	}
	if (read->mem_load_mode && min_offset_plus_one) {
		isoffin_purge_mem(read, min_offset_plus_one-1);
	}

	//we reached end of playback due to play range request, we must send eos - however for safety reason with DASH, we first need to cancel the input
	if (read->pid && check_forced_end && (nb_forced_end==count)) {
		//abort input
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_STOP, read->pid);
		gf_filter_pid_send_event(read->pid, &evt);
	}

	if (!is_active) {
		return GF_EOS;
	}

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
	{ OFFS(src), "local file name of source content (only used when explicitly loading the filter)", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(allt), "load all tracks even if unknown media type", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(edits), "do not use edit lists\n"
		"- auto: track delay and no edit list when possible\n"
		"- no: ignore edit list\n"
		"- strict: use edit list even if only signaling a delay", GF_PROP_UINT, "auto", "auto|no|strict", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(itt), "convert all items of root meta into a single PID", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(itemid), "keep item IDs in PID properties", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(smode), "load mode for scalable/tile tracks\n"
	"- split: each track is declared, extractors are removed\n"
	"- splitx: each track is declared, extractors are kept\n"
	"- single: a single track is declared (highest level for scalable, tile base for tiling)", GF_PROP_UINT, "split", "split|splitx|single", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(alltk), "declare disabled tracks", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(frame_size), "frame size for raw audio samples (dispatches frame_size samples per packet)", GF_PROP_UINT, "1024", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(expart), "expose cover art as a dedicated video PID", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(sigfrag), "signal fragment and segment boundaries of source on output packets, fails if source is not fragmented", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(tkid), "declare only track based on given param\n"
	"- integer value: declares track with the given ID\n"
	"- audio: declares first audio track\n"
	"- video: declares first video track\n"
	"- 4CC: declares first track with matching 4CC for handler type", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(stsd), "only extract sample mapped to the given sample description index (0 means extract all)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mov), "pointer to a read/edit ISOBMF file used internally by importers and exporters", GF_PROP_POINTER, NULL, NULL, GF_FS_ARG_HINT_HIDE},
	{ OFFS(analyze), "skip reformat of decoder config and SEI and dispatch all NAL in input order - shall only be used with inspect filter analyze mode!", GF_PROP_UINT, "off", "off|on|bs|full", GF_FS_ARG_HINT_HIDE},
	{ OFFS(catseg), "append the given segment to the movie at init time (only local file supported)", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_HIDE},
	{ OFFS(nocrypt), "signal encrypted tracks as non encrypted (mostly used for export)", GF_PROP_BOOL, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(mstore_size), "target buffer size in bytes when reading from memory stream (pipe etc...)", GF_PROP_UINT, "1000000", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mstore_purge), "minimum size in bytes between memory purges when reading from memory stream, 0 means purge as soon as possible", GF_PROP_UINT, "50000", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mstore_samples), "minimum number of samples to be present before purging sample tables when reading from memory stream (pipe etc...), 0 means purge as soon as possible", GF_PROP_UINT, "50", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(strtxt), "load text tracks (apple/tx3g) as MPEG-4 streaming text tracks", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(xps_check), "parameter sets extraction mode from AVC/HEVC/VVC samples\n"
	"- keep: do not inspect sample (assumes input file is compliant when generating DASH/HLS/CMAF)\n"
	"- rem: removes all inband xPS and notify configuration changes accordingly\n"
	"- auto: resolves to `keep` for `smode=splitx` (dasher mode), `rem` otherwise"
	, GF_PROP_UINT, "auto", "auto|keep|rem", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(nodata), "control sample data loading\n"
	"- no: regular load\n"
	"- yes: skip data loading\n"
	"- fake: allocate sample but no data copy", GF_PROP_UINT, "no", "no|yes|fake", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(lightp), "load minimal set of properties", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(initseg), "local init segment name when input is a single ISOBMFF segment", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};

static const GF_FilterCapability ISOFFInCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "mp4|mpg4|m4a|m4i|3gp|3gpp|3g2|3gp2|iso|m4s|iff|heif|heic|avif|avci|mj2|ismv|mov|qt"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "video/mp4|audio/mp4|application/mp4|video/3gpp|audio/3gpp|video/3gp2|audio/3gp2|video/iso.segment|audio/iso.segment|image/heif|image/heic|image/avci|video/jp2|video/quicktime"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_METADATA),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),

	CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	//also declare generic file output for embedded files (cover art & co), but explicit to skip this cap in chain resolution
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_OUTPUT | GF_CAPFLAG_LOADED_FILTER ,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE)
};

GF_FilterRegister ISOFFInRegister = {
	.name = "mp4dmx",
	GF_FS_SET_DESCRIPTION("ISOBMFF/QT demultiplexer")
	GF_FS_SET_HELP("This filter demultiplexes ISOBMF and QT files.\n"
		"Input ISOBMFF/QT can be regular or fragmented, and available as files or as raw bytestream.\n"
		"# Track Selection\n"
		"The filter can use fragment identifiers of source to select a single track for playback. The allowed fragments are:\n"
		" - #audio: only use the first audio track\n"
		" - #video: only use the first video track\n"
		" - #auxv: only use the first auxiliary video track\n"
		" - #pict: only use the first picture track\n"
		" - #text: only use the first text track\n"
		" - #trackID=VAL: only use the track with given ID\n"
		" - #itemID=VAL: only use the item with given ID\n"
		" - #ID=VAL: only use the track/item with given ID\n"
		" - #VAL: only use the track/item with given ID\n"
		"\n"
		"# Scalable Tracks\n"
		"When scalable tracks are present in a file, the reader can operate in 3 modes using [-smode]() option:\n"
		"- smode=single: resolves all extractors to extract a single bitstream from a scalable set. The highest level is used\n"
		"In this mode, there is no enhancement decoder config, only a base one resulting from the merge of the layers configurations\n"
		"- smode=split: all extractors are removed and every track of the scalable set is declared. In this mode, each enhancement track has no base decoder config\n"
		"and an enhancement decoder config.\n"
		"- smode=splitx: extractors are kept in the bitstream, and every track of the scalable set is declared. In this mode, each enhancement track has a base decoder config\n"
		" (copied from base) and an enhancement decoder config. This is mostly used for DASHing content.\n"
		"Warning: smode=splitx will result in extractor NAL units still present in the output bitstream, which shall only be true if the output is ISOBMFF based\n"
	 	)
	.private_size = sizeof(ISOMReader),
	.flags = GF_FS_REG_USE_SYNC_READ,
	.args = ISOFFInArgs,
	.initialize = isoffin_initialize,
	.finalize = isoffin_finalize,
	.process = isoffin_process,
	.configure_pid = isoffin_configure_pid,
	SETCAPS(ISOFFInCaps),
	.process_event = isoffin_process_event,
	.probe_data = isoffin_probe_data
};

const GF_FilterRegister *mp4dmx_register(GF_FilterSession *session)
{
	return &ISOFFInRegister;
}
#else
const GF_FilterRegister *mp4dmx_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_MP4DMX)
