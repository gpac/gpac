/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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
#include <gpac/network.h>
#include <gpac/avparse.h>


void isor_reset_reader(ISOMChannel *ch)
{
	ch->last_state = GF_OK;
	isor_reader_release_sample(ch);

	if (ch->static_sample) {
		ch->static_sample->dataLength = ch->static_sample->alloc_size;
		gf_isom_sample_del(&ch->static_sample);
	}
	ch->sample = NULL;
	ch->sample_num = 0;
	ch->speed = 1.0;
	ch->start = ch->end = 0;
	ch->to_init = 1;
	ch->playing = GF_FALSE;
	if (ch->sai_buffer) gf_free(ch->sai_buffer);
	ch->sai_buffer = NULL;
	ch->sai_alloc_size = 0;
	ch->dts = ch->cts = 0;
	ch->seek_flag = 0;
}

void isor_check_producer_ref_time(ISOMReader *read)
{
	GF_ISOTrackID trackID;
	u64 ntp;
	u64 timestamp;

	if (gf_sys_is_test_mode()) {
		return;
	}

	if (gf_isom_get_last_producer_time_box(read->mov, &trackID, &ntp, &timestamp, GF_TRUE)) {
#if !defined(_WIN32_WCE) && !defined(GPAC_DISABLE_LOG)

		if (gf_log_tool_level_on(GF_LOG_DASH, GF_LOG_DEBUG)) {
			time_t secs;
			struct tm t;

			s32 diff = gf_net_get_ntp_diff_ms(ntp);

			secs = (ntp>>32) - GF_NTP_SEC_1900_TO_1970;
			t = *gf_gmtime(&secs);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] TrackID %d: Timestamp "LLU" matches sender NTP time %d-%02d-%02dT%02d:%02d:%02dZ - NTP clock diff (local - remote): %d ms\n", trackID, timestamp, 1900+t.tm_year, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, diff));
		}
#endif
		read->last_sender_ntp = ntp;
		read->cts_for_last_sender_ntp = timestamp;
		read->ntp_at_last_sender_ntp = gf_net_get_ntp_ts();
	}
}


static void init_reader(ISOMChannel *ch)
{
	u32 sample_desc_index=0;

	ch->au_seq_num = 1;

	assert(ch->sample==NULL);
	if (!ch->static_sample) {
		ch->static_sample = gf_isom_sample_new();
	}

	if (ch->streamType==GF_STREAM_OCR) {
		assert(!ch->sample);
		ch->sample = gf_isom_sample_new();
		ch->sample->IsRAP = RAP;
		ch->sample->DTS = ch->start;
		ch->last_state=GF_OK;
	} else if (ch->sample_num) {
		ch->sample = gf_isom_get_sample_ex(ch->owner->mov, ch->track, ch->sample_num, &sample_desc_index, ch->static_sample, &ch->sample_data_offset);
		ch->disable_seek = GF_TRUE;
		ch->au_seq_num = ch->sample_num;
	} else {
		//if seek is disabled, get the next closest sample for this time; otherwise, get the previous RAP sample for this time
		u32 mode = ch->disable_seek ? GF_ISOM_SEARCH_BACKWARD : GF_ISOM_SEARCH_SYNC_BACKWARD;

		/*take care of seeking out of the track range*/
		if (!ch->owner->frag_type && (ch->duration<=ch->start)) {
			ch->last_state = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->duration-1, &sample_desc_index, mode, &ch->static_sample, &ch->sample_num, &ch->sample_data_offset);
		} else if (ch->start || ch->has_edit_list) {
			ch->last_state = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->start, &sample_desc_index, mode, &ch->static_sample, &ch->sample_num, &ch->sample_data_offset);
		} else {
			ch->sample_num = 1;
			if (ch->owner->nodata) {
				ch->sample = gf_isom_get_sample_info_ex(ch->owner->mov, ch->track, ch->sample_num, &sample_desc_index, &ch->sample_data_offset, ch->static_sample);
			} else {
				ch->sample = gf_isom_get_sample_ex(ch->owner->mov, ch->track, ch->sample_num, &sample_desc_index, ch->static_sample, &ch->sample_data_offset);
			}
			if (!ch->sample) ch->last_state = GF_EOS;
		}
		if (ch->last_state) {
			ch->sample = NULL;
			ch->last_state = GF_OK;
		} else {
			ch->sample = ch->static_sample;
		}

		if (ch->has_rap && ch->has_edit_list) {
			ch->edit_sync_frame = ch->sample_num;
		}

		if (ch->sample && !ch->sample->data && ch->owner->frag_type && !ch->has_edit_list) {
			ch->sample = NULL;
			ch->sample_num = 1;
			ch->sample = gf_isom_get_sample_ex(ch->owner->mov, ch->track, ch->sample_num, &sample_desc_index, ch->static_sample, &ch->sample_data_offset);
		}
	}


	/*no sample means we're not in the track range - stop*/
	if (!ch->sample) {
		/*incomplete file - check if we're still downloading or not*/
		if (gf_isom_get_missing_bytes(ch->owner->mov, ch->track)) {
			if (!ch->owner->input_loaded) {
				ch->last_state = GF_OK;
				return;
			}
			ch->last_state = GF_ISOM_INCOMPLETE_FILE;
		} else if (ch->sample_num) {
			ch->last_state = (ch->owner->frag_type==1) ? GF_OK : GF_EOS;
			ch->to_init = 0;
		}
		return;
	}

	ch->sample_time = ch->sample->DTS;

	ch->to_init = GF_FALSE;

	ch->seek_flag = 0;
	if (ch->disable_seek) {
		ch->dts = ch->sample->DTS;
		ch->cts = ch->sample->DTS + ch->sample->CTS_Offset;
		ch->start = 0;
	} else {
		s64 cts;
		ch->dts = ch->start;
		ch->cts = ch->start;

		cts = ch->sample->DTS + ch->sample->CTS_Offset;
		if (ch->ts_offset<0)
			cts += ch->ts_offset;

		//TODO - we need to notify scene decoder how many secs elapsed between RAP and seek point
		if (ch->cts != cts) {
			ch->seek_flag = 1;
		}
	}
	if (!sample_desc_index) sample_desc_index = 1;
	ch->last_sample_desc_index = sample_desc_index;
	ch->owner->no_order_check = ch->speed < 0 ? GF_TRUE : GF_FALSE;
}


static void isor_update_cenc_info(ISOMChannel *ch, Bool for_item)
{
	GF_Err e;
	Bool Is_Encrypted;
	u32 out_size;
	u8 crypt_byte_block, skip_byte_block;
	u8 piff_info[20];
	u8 *key_info = NULL;
	u32 key_info_size = 0;
	u8 item_mkey = 0;

	//this will be skipped anyways, don't fectch ...
	if (ch->owner->stsd && (ch->last_sample_desc_index != ch->owner->stsd) && ch->sample) {
		return;
	}


	out_size = ch->sai_alloc_size;
	if (for_item) {
		u32 aux_info_param=0;
		e = gf_isom_extract_meta_item_get_cenc_info(ch->owner->mov, GF_TRUE, 0, ch->item_id, &Is_Encrypted, &skip_byte_block, &crypt_byte_block, (const u8 **) &key_info, &key_info_size, &aux_info_param, &ch->sai_buffer, &out_size, &ch->sai_alloc_size);

		/*The ienc property is always exposed as a multiple key info in GPAC
		However the type of SAI may be single-key (aux_info_param==0) or multiple-key (aux_info_param==1) for the same ienc used
		We therefore temporary force the key info type to single key if v0 SAI CENC are used
		Note that this is thread safe as this filter is the only one using the opened file
		*/
		if (aux_info_param==0) {
			item_mkey = key_info[0];
		}
	} else {
		e = gf_isom_get_sample_cenc_info(ch->owner->mov, ch->track, ch->sample_num, &Is_Encrypted, &crypt_byte_block, &skip_byte_block, (const u8 **) &key_info, &key_info_size);
	}
	if (!key_info) {
		piff_info[0] = 0;
		piff_info[1] = 0;
		piff_info[2] = 0;
		piff_info[3] = key_info_size;
		memset(piff_info + 4, 0, 16);
		key_info_size = 20;
		key_info = (u8 *) piff_info;
	}


	if (!for_item && (e==GF_OK) && Is_Encrypted) {
		e = gf_isom_cenc_get_sample_aux_info(ch->owner->mov, ch->track, ch->sample_num, ch->last_sample_desc_index, NULL, &ch->sai_buffer, &out_size);
	}

	if (out_size > ch->sai_alloc_size) ch->sai_alloc_size = out_size;
	ch->sai_buffer_size = out_size;

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] Failed to fetch CENC auxiliary info for %s %d: %s\n", for_item ? "item" : "track", for_item ? ch->item_id : ch->track, gf_error_to_string(e) ));
		return;
	}

	ch->pck_encrypted = Is_Encrypted;
	ch->cenc_ki = NULL;

	/*notify change of IV/KID only when packet is encrypted
	1- these info are ignored when packet is not encrypted
	2- this allows us to define the initial CENC state for multi-stsd cases*/
	if (Is_Encrypted) {
		u32 ki_crc;

		if ((ch->crypt_byte_block != crypt_byte_block) || (ch->skip_byte_block != skip_byte_block)) {
			ch->crypt_byte_block = crypt_byte_block;
			ch->skip_byte_block = skip_byte_block;

			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_PATTERN, &PROP_FRAC_INT(ch->skip_byte_block, ch->crypt_byte_block) );
		}
		if (item_mkey)
			key_info[0] = 0;

		ki_crc = gf_crc_32(key_info, key_info_size);
		if (ch->key_info_crc != ki_crc) {
			ch->key_info_crc = ki_crc;
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CENC_KEY_INFO, &PROP_DATA((u8 *)key_info, key_info_size) );
		}

		if (item_mkey)
			key_info[0] = item_mkey;

		ch->cenc_ki = gf_filter_pid_get_property(ch->pid, GF_PROP_PID_CENC_KEY_INFO);
	}
}

void isor_reader_get_sample_from_item(ISOMChannel *ch)
{
	if (ch->au_seq_num) {
		if (!ch->owner->itt || !isor_declare_item_properties(ch->owner, ch, 1+ch->au_seq_num)) {
			ch->last_state = GF_EOS;
			return;
		}
	}
	ch->sample_time = 0;
	ch->last_state = GF_OK;
	if (!ch->static_sample) {
		ch->static_sample = gf_isom_sample_new();
	}

	ch->sample = ch->static_sample;
	ch->sample->IsRAP = RAP;
	ch->au_duration = 1000;
	ch->dts = ch->cts = 1000 * ch->au_seq_num;
	gf_isom_extract_meta_item_mem(ch->owner->mov, GF_TRUE, 0, ch->item_id, &ch->sample->data, &ch->sample->dataLength, &ch->static_sample->alloc_size, NULL, GF_FALSE);

	if (ch->is_encrypted && ch->is_cenc) {
		isor_update_cenc_info(ch, GF_TRUE);
	}
}

void isor_reader_get_sample(ISOMChannel *ch)
{
	GF_Err e;
	u32 sample_desc_index;
	if (ch->sample) return;

	if (ch->next_track) {
		ch->track = ch->next_track;
		ch->next_track = 0;
	}

	if (ch->to_init) {
		init_reader(ch);
		sample_desc_index = ch->last_sample_desc_index;
	} else if (ch->speed < 0) {
		if (ch->last_state == GF_EOS) {
			ch->sample = NULL;
			return;
		}

		if (ch->static_sample->IsRAP) {
			ch->last_rap_sample_time = ch->sample_time;
		}

		e = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->sample_time + 1, &sample_desc_index, GF_ISOM_SEARCH_FORWARD, &ch->static_sample, &ch->sample_num, NULL);

		if ((e==GF_EOS) || (ch->static_sample->IsRAP)) {
			if (!ch->last_rap_sample_time) {
				e = GF_EOS;
			} else {
				e = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->last_rap_sample_time - 1, &sample_desc_index, GF_ISOM_SEARCH_SYNC_BACKWARD, &ch->static_sample, &ch->sample_num, NULL);
			}
		}

		if (e) {
			if ((e==GF_EOS) && !ch->owner->frag_type) {
				ch->last_state = GF_EOS;
			}
			ch->sample = NULL;
			return;
		}
		ch->sample = ch->static_sample;

		if (ch->sample->DTS == ch->sample_time) {
			if (!ch->owner->frag_type) {
				ch->last_state = GF_EOS;
			}
		}
		if (ch->sample) {
			ch->sample_time = ch->sample->DTS;
		}

	} else if (ch->has_edit_list) {
		u32 prev_sample = ch->sample_num;
		e = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->sample_time + 1, &sample_desc_index, GF_ISOM_SEARCH_FORWARD, &ch->static_sample, &ch->sample_num, &ch->sample_data_offset);

		if (e == GF_OK) {
			ch->sample = ch->static_sample;

			/*we are in forced seek mode: fetch all samples before the one matching the sample time*/
			if (ch->edit_sync_frame) {
				ch->edit_sync_frame++;
				if (ch->edit_sync_frame < ch->sample_num) {
					ch->sample = gf_isom_get_sample_ex(ch->owner->mov, ch->track, ch->edit_sync_frame, &sample_desc_index, ch->static_sample, &ch->sample_data_offset);
					if (ch->sample) {
						ch->sample->DTS = ch->sample_time;
						ch->sample->CTS_Offset = 0;
					}
				} else {
					ch->edit_sync_frame = 0;
					if (ch->sample) ch->sample_time = ch->sample->DTS;
				}
			} else {
				/*if we get the same sample, figure out next interesting time (current sample + DTS gap to next sample should be a good bet)*/
				if (prev_sample == ch->sample_num) {
					if (ch->owner->frag_type && (ch->sample_num==gf_isom_get_sample_count(ch->owner->mov, ch->track))) {
						ch->sample = NULL;
					} else {
						u32 sample_num = ch->sample_num ? ch->sample_num : 1;

						if (sample_num >= gf_isom_get_sample_count(ch->owner->mov, ch->track) ) {
							//e = GF_EOS;
						} else {
							u32 time_diff = gf_isom_get_sample_duration(ch->owner->mov, ch->track, sample_num);
							e = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->sample_time + time_diff, &sample_desc_index, GF_ISOM_SEARCH_FORWARD, &ch->static_sample, &ch->sample_num, &ch->sample_data_offset);
							if (e==GF_OK) {
								if (ch->sample_num == prev_sample) {
									ch->sample_time += time_diff;
									ch->sample = NULL;
									return;
								} else {
									ch->sample = ch->static_sample;
								}
							}
						}
					}
				}

				/*we jumped to another segment - if RAP is needed look for closest rap in decoding order and
				force seek mode*/
				if (ch->sample && !ch->sample->IsRAP && ch->has_rap && (ch->sample_num != prev_sample+1)) {
					GF_ISOSample *found = ch->static_sample;
					u32 samp_num = ch->sample_num;
					ch->sample = NULL;
					e = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->sample_time + 1, &sample_desc_index, GF_ISOM_SEARCH_SYNC_BACKWARD, &ch->static_sample, &ch->sample_num, &ch->sample_data_offset);

					ch->sample = (e == GF_OK) ? ch->static_sample : NULL;

					/*if no sync point in the past, use the first non-sync for the given time*/
					if (!ch->sample || !ch->sample->data) {
						ch->sample = ch->static_sample = found;
						ch->sample_time = ch->sample->DTS;
						ch->sample_num = samp_num;
					} else {
						ch->sample = ch->static_sample;
						ch->edit_sync_frame = ch->sample_num;
						ch->sample->DTS = ch->sample_time;
						ch->sample->CTS_Offset = 0;
					}
				} else {
					if (ch->sample) ch->sample_time = ch->sample->DTS;
				}
			}
		}
	} else {
		Bool do_fetch = GF_TRUE;
		ch->sample_num++;

		if (ch->sap_only) {
			Bool is_rap = gf_isom_get_sample_sync(ch->owner->mov, ch->track, ch->sample_num);
			if (!is_rap) {
				GF_ISOSampleRollType roll_type;
				gf_isom_get_sample_rap_roll_info(ch->owner->mov, ch->track, ch->sample_num, &is_rap, &roll_type, NULL);
				if (roll_type) is_rap = GF_TRUE;
			}

			if (!is_rap) {
				do_fetch = GF_FALSE;
			} else if (ch->sap_only==2) {
				ch->sap_only = 0;
			}
		}
		if (do_fetch) {
			if (ch->owner->nodata) {
				ch->sample = gf_isom_get_sample_info_ex(ch->owner->mov, ch->track, ch->sample_num, &sample_desc_index, &ch->sample_data_offset, ch->static_sample);
			} else {
				ch->sample = gf_isom_get_sample_ex(ch->owner->mov, ch->track, ch->sample_num, &sample_desc_index, ch->static_sample, &ch->sample_data_offset);
			}
			/*if sync shadow / carousel RAP skip*/
			if (ch->sample && (ch->sample->IsRAP==RAP_REDUNDANT)) {
				ch->sample = NULL;
				ch->sample_num++;
				isor_reader_get_sample(ch);
				return;
			}
		}
	}

	//check scalable track change
	if (ch->sample && ch->sample->IsRAP && ch->next_track) {
		ch->track = ch->next_track;
		ch->next_track = 0;
		ch->sample = NULL;
		isor_reader_get_sample(ch);
		return;
	}

	if (!ch->sample) {
		u32 sample_count = gf_isom_get_sample_count(ch->owner->mov, ch->track);
		ch->sample_data_offset = 0;
		/*incomplete file - check if we're still downloading or not*/
		if (gf_isom_get_missing_bytes(ch->owner->mov, ch->track)) {
			ch->last_state = GF_ISOM_INCOMPLETE_FILE;
			if (ch->owner->mem_load_mode==2)
				ch->owner->force_fetch = GF_TRUE;

			if (!ch->owner->input_loaded) {
				ch->last_state = GF_OK;
				if (!ch->has_edit_list && ch->sample_num)
					ch->sample_num--;
			} else {
				if (ch->to_init && ch->sample_num) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] Failed to fetch initial sample %d for track %d\n", ch->sample_num, ch->track));
					ch->last_state = GF_ISOM_INVALID_FILE;
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] File truncated, aborting read for track %d\n", ch->track));
					ch->last_state = GF_EOS;
				}
			}
		}
		else if (!ch->sample_num
		         || ((ch->speed >= 0) && (ch->sample_num >= sample_count))
		         || ((ch->speed < 0) && (ch->sample_time == gf_isom_get_current_tfdt(ch->owner->mov, ch->track) ))
		        ) {

			if (ch->owner->frag_type==1) {
				/*if sample cannot be found and file is fragmented, rewind sample*/
				if (ch->sample_num) ch->sample_num--;
				ch->last_state = GF_EOS;
			} else if (ch->last_state != GF_EOS) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[IsoMedia] Track #%d end of stream reached\n", ch->track));
				ch->last_state = GF_EOS;
				if (ch->sample_num>sample_count) ch->sample_num = sample_count;
			} else {
				if (ch->sample_num>sample_count) ch->sample_num = sample_count;
			}
		} else {
			e = gf_isom_last_error(ch->owner->mov);
			GF_LOG((e==GF_ISOM_INCOMPLETE_FILE) ? GF_LOG_DEBUG : GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track #%d fail to fetch sample %d / %d: %s\n", ch->track, ch->sample_num, gf_isom_get_sample_count(ch->owner->mov, ch->track), gf_error_to_string(e) ));

			if ((e<GF_OK) && (e!=GF_ISOM_INCOMPLETE_FILE)) {
				ch->last_state = GF_EOS;
			}
		}
		return;
	}

	if (sample_desc_index != ch->last_sample_desc_index) {
		if (!ch->owner->stsd) {
			//we used sample entry 1 by default to setup, if no active prev sample (edit list might trigger this)
			//and new sample desc is 1, do not reconfigure
			if (!ch->last_sample_desc_index && (sample_desc_index==1)) {

			} else {
				ch->needs_pid_reconfig = GF_TRUE;
			}
		}
		ch->last_sample_desc_index = sample_desc_index;
	}

	ch->last_state = GF_OK;
	ch->au_duration = gf_isom_get_sample_duration(ch->owner->mov, ch->track, ch->sample_num);

	ch->sap_3 = GF_FALSE;
	ch->sap_4_type = 0;
	ch->roll = 0;
	ch->set_disc = ch->owner->clock_discontinuity ? 2 : 0;
	ch->owner->clock_discontinuity = 0;

	if (ch->sample) {
		gf_isom_get_sample_rap_roll_info(ch->owner->mov, ch->track, ch->sample_num, &ch->sap_3, &ch->sap_4_type, &ch->roll);

		/*still seeking or not ?
		 1- when speed is negative, the RAP found is "after" the seek point in playback order since we used backward RAP search: nothing to do
		 2- otherwise set DTS+CTS to start value
		 */
		if ((ch->speed < 0) || (ch->start <= ch->sample->DTS + ch->sample->CTS_Offset)) {
			ch->dts = ch->sample->DTS;
			ch->cts = ch->sample->DTS + ch->sample->CTS_Offset;
			ch->seek_flag = 0;
		} else {
			ch->cts = ch->start;
			ch->seek_flag = 1;
			ch->dts = ch->start;
		}

		if (ch->end && (ch->end < ch->sample->DTS + ch->sample->CTS_Offset + ch->au_duration)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[IsoMedia] End of Channel "LLD" (CTS "LLD")\n", ch->end, ch->sample->DTS + ch->sample->CTS_Offset));
			ch->sample = NULL;
			ch->last_state = GF_EOS;
			ch->playing = 2;
			return;
		}
	}

	if (ch->owner->last_sender_ntp && ch->cts==ch->owner->cts_for_last_sender_ntp) {
		ch->sender_ntp = ch->owner->last_sender_ntp;
		ch->ntp_at_server_ntp = ch->owner->ntp_at_last_sender_ntp;
	} else if (ch->owner->last_sender_ntp && ch->dts==ch->owner->cts_for_last_sender_ntp) {
		ch->sender_ntp = ch->owner->last_sender_ntp;
		ch->ntp_at_server_ntp = ch->owner->ntp_at_last_sender_ntp;
	} else {
		ch->sender_ntp = ch->ntp_at_server_ntp = 0;
	}

	if (!ch->sample_num) return;

	gf_isom_get_sample_flags(ch->owner->mov, ch->track, ch->sample_num, &ch->isLeading, &ch->dependsOn, &ch->dependedOn, &ch->redundant);

	if (ch->is_encrypted) {
		/*in case of CENC: we write sample auxiliary information to slh->sai; its size is in saiz*/
		if (gf_isom_is_cenc_media(ch->owner->mov, ch->track, ch->last_sample_desc_index)) {
			isor_update_cenc_info(ch, GF_FALSE);

		} else if (gf_isom_is_media_encrypted(ch->owner->mov, ch->track, ch->last_sample_desc_index)) {
			ch->pck_encrypted = GF_TRUE;
		} else {
			ch->pck_encrypted = GF_FALSE;
		}
	}
	if (ch->sample && ch->sample->nb_pack)
		ch->sample_num += ch->sample->nb_pack-1;
}

void isor_reader_release_sample(ISOMChannel *ch)
{
	if (ch->sample)
		ch->au_seq_num++;
	ch->sample = NULL;
	ch->sai_buffer_size = 0;
}

static void isor_reset_seq_list(GF_List *list)
{
	while (gf_list_count(list)) {
		GF_NALUFFParam *sl = gf_list_pop_back(list);
		gf_free(sl->data);
		gf_free(sl);
	}
}

enum
{
	RESET_STATE_VPS=1,
	RESET_STATE_SPS=1<<1,
	RESET_STATE_PPS=1<<2,
	RESET_STATE_SPS_EXT=1<<3,
	RESET_STATE_DCI=1<<4,
};

static void isor_replace_nal(ISOMChannel *ch, u8 *data, u32 size, u8 nal_type, u32 *reset_state)
{
	u32 i, count, state=0;
	GF_NALUFFParam *sl;
	GF_List *list=NULL;
	if (ch->avcc) {
		if (nal_type==GF_AVC_NALU_PIC_PARAM) {
			list = ch->avcc->pictureParameterSets;
			state=RESET_STATE_PPS;
		} else if (nal_type==GF_AVC_NALU_SEQ_PARAM) {
			list = ch->avcc->sequenceParameterSets;
			state=RESET_STATE_SPS;
		} else if (nal_type==GF_AVC_NALU_SEQ_PARAM_EXT) {
			list = ch->avcc->sequenceParameterSetExtensions;
			state=RESET_STATE_SPS_EXT;
		} else return;
	}
	else if (ch->hvcc) {
		GF_NALUFFParamArray *hvca=NULL;
		count = gf_list_count(ch->hvcc->param_array);
		for (i=0; i<count; i++) {
			hvca = gf_list_get(ch->hvcc->param_array, i);
			if (hvca->type==nal_type) {
				list = hvca->nalus;
				break;
			}
			hvca = NULL;
		}
		if (!hvca) {
			GF_SAFEALLOC(hvca, GF_NALUFFParamArray);
			if (hvca) {
				list = hvca->nalus = gf_list_new();
				hvca->type = nal_type;
				gf_list_add(ch->hvcc->param_array, hvca);
			}
		}
		switch (nal_type) {
		case GF_HEVC_NALU_VID_PARAM:
			state = RESET_STATE_VPS;
			break;
		case GF_HEVC_NALU_SEQ_PARAM:
			state = RESET_STATE_SPS;
			break;
		case GF_HEVC_NALU_PIC_PARAM:
			state = RESET_STATE_PPS;
			break;
		}
	}
	else if (ch->vvcc) {
		GF_NALUFFParamArray *vvca=NULL;
		count = gf_list_count(ch->vvcc->param_array);
		for (i=0; i<count; i++) {
			vvca = gf_list_get(ch->vvcc->param_array, i);
			if (vvca->type==nal_type) {
				list = vvca->nalus;
				break;
			}
			vvca = NULL;
		}
		if (!vvca) {
			GF_SAFEALLOC(vvca, GF_NALUFFParamArray);
			if (vvca) {
				list = vvca->nalus = gf_list_new();
				vvca->type = nal_type;
				gf_list_add(ch->vvcc->param_array, vvca);
			}
		}
		switch (nal_type) {
		case GF_VVC_NALU_VID_PARAM:
			state = RESET_STATE_VPS;
			break;
		case GF_VVC_NALU_SEQ_PARAM:
			state = RESET_STATE_SPS;
			break;
		case GF_VVC_NALU_PIC_PARAM:
			state = RESET_STATE_PPS;
			break;
		case GF_VVC_NALU_DEC_PARAM:
			state = RESET_STATE_DCI;
			break;
		}
	}

	ch->xps_mask |= state;

	count = gf_list_count(list);
	for (i=0; i<count; i++) {
		sl = gf_list_get(list, i);
		if ((sl->size==size) && !memcmp(sl->data, data, size)) return;
	}
	if (! (*reset_state & state))  {
		isor_reset_seq_list(list);
		*reset_state |= state;
	}
	GF_SAFEALLOC(sl, GF_NALUFFParam);
	if (!sl) return;
	sl->data = gf_malloc(sizeof(char)*size);
	memcpy(sl->data, data, size);
	sl->size = size;
	gf_list_add(list, sl);
}

u8 key_info_get_iv_size(const u8 *key_info, u32 nb_keys, u32 idx, u8 *const_iv_size, const u8 **const_iv);

void isor_sai_bytes_removed(ISOMChannel *ch, u32 pos, u32 removed)
{
	u32 offset = 0;
	u8 *sai;
	u32 sai_size, cur_pos;
	u32 sub_count_size = 0;
	u32 i, subs_count = 0;

	if (!ch->cenc_ki || !ch->sai_buffer) return;

	sai = ch->sai_buffer;
	sai_size = ch->sai_buffer_size;

	//multikey
	if (ch->cenc_ki->value.data.ptr[0]) {
		u32 remain;
		u32 j, nb_iv_init = sai[0];
		nb_iv_init <<= 8;
		nb_iv_init |= sai[1];
		u8 *sai_p = sai + 2;
		remain = sai_size-2;

		for (j=0; j<nb_iv_init; j++) {
			u32 mk_iv_size;
			u32 idx = sai_p[0];
			idx<<=8;
			idx |= sai_p[1];

			mk_iv_size = key_info_get_iv_size(ch->cenc_ki->value.data.ptr, ch->cenc_ki->value.data.size, idx, NULL, NULL);
			mk_iv_size += 2; //idx
			if (mk_iv_size > remain) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MP4Mux] Invalid multi-key CENC SAI, cannot modify first subsample !\n"));
				return;
			}
			sai_p += mk_iv_size;
			remain -= mk_iv_size;
		}
		offset = (u32) (sai_p - sai);
		sub_count_size = 4; //32bit sub count

	} else {
		offset = key_info_get_iv_size(ch->cenc_ki->value.data.ptr, ch->cenc_ki->value.data.size, 1, NULL, NULL);
		sub_count_size = 2; //16bit sub count
	}
	if (sai_size < offset + sub_count_size) return;

	sai += offset;
	if (sub_count_size==2) {
		subs_count = ((u32) sai[0]) << 8 | sai[1];
	} else {
		subs_count = GF_4CC(sai[0], sai[1], sai[2], sai[3]);
	}
	sai += sub_count_size;
	sai_size -= offset + sub_count_size;
	cur_pos = 0;
	for (i=0; i<subs_count; i++) {
		if (sai_size<6)
			return;
		u32 clear = ((u32) sai[0]) << 8 | sai[1];
		u32 crypt = GF_4CC(sai[2], sai[3], sai[4], sai[5]);
		if (cur_pos + clear > pos) {
			clear -= removed;
			sai[0] = (clear>>8) & 0xFF;
			sai[1] = (clear) & 0xFF;
			return;
		}
		cur_pos += clear + crypt;
		sai += 6;
		sai_size-=6;
	}
}

void isor_reader_check_config(ISOMChannel *ch)
{
	u32 nalu_len, reset_state;
	if (!ch->check_hevc_ps && !ch->check_avc_ps && !ch->check_vvc_ps && !ch->check_mhas_pl) return;

	if (!ch->sample) return;
	ch->xps_mask = 0;

	//we cannot touch the payload if encrypted but not CENC !!
	if (ch->is_encrypted && !ch->is_cenc)
		return;

	if (ch->check_mhas_pl) {
		//we cannot touch the payload if encrypted !!
		if (ch->pck_encrypted) return;
		u64 ch_layout = 0;
		s32 PL = gf_mpegh_get_mhas_pl(ch->sample->data, ch->sample->dataLength, &ch_layout);
		if (PL>0) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_PROFILE_LEVEL, &PROP_UINT((u32) PL));
			ch->check_mhas_pl = GF_FALSE;
			if (ch_layout)
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_LONGUINT(ch_layout));
		}
		return;
	}
	//analyze mode, do not rewrite
	if (ch->owner->analyze) return;

	//we cannot touch the payload if encrypted but no SAI buffer
	if (ch->pck_encrypted && !ch->sai_buffer)
		return;

	nalu_len = 4;
	if (ch->avcc) nalu_len = ch->avcc->nal_unit_size;
	else if (ch->hvcc) nalu_len = ch->hvcc->nal_unit_size;
	else if (ch->vvcc) nalu_len = ch->vvcc->nal_unit_size;

	reset_state = 0;

	if (!ch->nal_bs) ch->nal_bs = gf_bs_new(ch->sample->data, ch->sample->dataLength, GF_BITSTREAM_READ);
	else gf_bs_reassign_buffer(ch->nal_bs, ch->sample->data, ch->sample->dataLength);

	while (1) {
		Bool replace_nal = GF_FALSE;
		u8 nal_type=0;
		u32 pos = (u32) gf_bs_get_position(ch->nal_bs);
		if (pos + nalu_len >= ch->sample->dataLength) break;
		u32 size = gf_bs_read_int(ch->nal_bs, nalu_len*8);
		//this takes care of size + pos + nalu_len > 0 but (s32) size < 0 ...
		if (ch->sample->dataLength < size) break;
		if (ch->sample->dataLength < size + pos + nalu_len) break;
		if (ch->check_avc_ps) {
			u8 hdr = gf_bs_peek_bits(ch->nal_bs, 8, 0);
			nal_type = hdr & 0x1F;
			switch (nal_type) {
			case GF_AVC_NALU_SEQ_PARAM:
			case GF_AVC_NALU_SEQ_PARAM_EXT:
			case GF_AVC_NALU_PIC_PARAM:
				replace_nal = GF_TRUE;
				break;
			}
		}
		else if (ch->check_hevc_ps) {
			u8 hdr = gf_bs_peek_bits(ch->nal_bs, 8, 0);
			nal_type = (hdr & 0x7E) >> 1;
			switch (nal_type) {
			case GF_HEVC_NALU_VID_PARAM:
			case GF_HEVC_NALU_SEQ_PARAM:
			case GF_HEVC_NALU_PIC_PARAM:
				replace_nal = GF_TRUE;
				break;
			}
		}
		else if (ch->check_vvc_ps) {
			u8 hdr = gf_bs_peek_bits(ch->nal_bs, 8, 1);
			nal_type = hdr >> 3;
			switch (nal_type) {
			case GF_VVC_NALU_VID_PARAM:
			case GF_VVC_NALU_SEQ_PARAM:
			case GF_VVC_NALU_PIC_PARAM:
			case GF_VVC_NALU_DEC_PARAM:
				replace_nal = GF_TRUE;
				break;
			}
		}
		gf_bs_skip_bytes(ch->nal_bs, size);

		if (replace_nal) {
			u32 move_size = ch->sample->dataLength - size - pos - nalu_len;
			isor_replace_nal(ch, ch->sample->data + pos + nalu_len, size, nal_type, &reset_state);
			if (move_size)
				memmove(ch->sample->data + pos, ch->sample->data + pos + size + nalu_len, ch->sample->dataLength - size - pos - nalu_len);

			ch->sample->dataLength -= size + nalu_len;
			gf_bs_reassign_buffer(ch->nal_bs, ch->sample->data, ch->sample->dataLength);
			gf_bs_seek(ch->nal_bs, pos);

			//remove nal from clear subsample range
			if (ch->pck_encrypted)
				isor_sai_bytes_removed(ch, pos, nalu_len+size);
		}
	}

	if (reset_state) {
		u8 *dsi=NULL;
		u32 dsi_size=0;
		if (ch->check_avc_ps) {
			gf_odf_avc_cfg_write(ch->avcc, &dsi, &dsi_size);
		}
		else if (ch->check_hevc_ps) {
			gf_odf_hevc_cfg_write(ch->hvcc, &dsi, &dsi_size);
		}
		else if (ch->check_vvc_ps) {
			gf_odf_vvc_cfg_write(ch->vvcc, &dsi, &dsi_size);
		}
		if (dsi && dsi_size) {
			u32 dsi_crc = gf_crc_32(dsi, dsi_size);
			if (ch->dsi_crc == dsi_crc) {
				gf_free(dsi);
			} else {
				ch->dsi_crc = dsi_crc;
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );
			}
		}
	}
}



void isor_set_sample_groups_and_aux_data(ISOMReader *read, ISOMChannel *ch, GF_FilterPacket *pck)
{
	char szPName[30];

	u32 grp_idx=0;
	while (1) {
		u32 grp_type=0, grp_size=0, grp_parameter=0;
		const u8 *grp_data=NULL;
		GF_Err e = gf_isom_enum_sample_group(read->mov, ch->track, ch->sample_num, &grp_idx, &grp_type, &grp_parameter, &grp_data, &grp_size);
		if (e) continue;
		if (!grp_type) break;
		if (!grp_size || !grp_data) continue;

		if (grp_type == GF_4CC('P','S','S','H')) {
			gf_filter_pck_set_property(pck, GF_PROP_PID_CENC_PSSH, &PROP_DATA((u8*)grp_data, grp_size) );
			continue;
		}
		//all other are mapped to sample groups
		if (grp_parameter) sprintf(szPName, "grp_%s_%d", gf_4cc_to_str(grp_type), grp_parameter);
		else sprintf(szPName, "grp_%s", gf_4cc_to_str(grp_type));

		gf_filter_pck_set_property_dyn(pck, szPName, &PROP_DATA((u8*)grp_data, grp_size) );
	}

	u32 sai_idx=0;
	while (1) {
		u32 sai_type=0, sai_size=0, sai_parameter=0;
		u8 *sai_data=NULL;
		GF_Err e = gf_isom_enum_sample_aux_data(read->mov, ch->track, ch->sample_num, &sai_idx, &sai_type, &sai_parameter, &sai_data, &sai_size);
		if (e) continue;
		if (!sai_type) break;
		if (!sai_size || !sai_data) continue;

		//all other are mapped to sample groups
		if (sai_parameter) sprintf(szPName, "sai_%s_%d", gf_4cc_to_str(sai_type), sai_parameter);
		else sprintf(szPName, "sai_%s", gf_4cc_to_str(sai_type));

		gf_filter_pck_set_property_dyn(pck, szPName, &PROP_DATA_NO_COPY(sai_data, sai_size) );
	}


	while (1) {
		GF_Err gf_isom_pop_emsg(GF_ISOFile *the_file, u8 **emsg_data, u32 *emsg_size);
		u8 *data=NULL;
		u32 size;
		GF_Err e = gf_isom_pop_emsg(read->mov, &data, &size);
		if (e || !data) break;

		gf_filter_pck_set_property_str(pck, "emsg", &PROP_DATA_NO_COPY(data, size));
	}

}


#endif /*GPAC_DISABLE_ISOM*/
