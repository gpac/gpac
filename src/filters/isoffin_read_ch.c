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

#include <time.h>

#ifndef GPAC_DISABLE_ISOM


void isor_reset_reader(ISOMChannel *ch)
{
	ch->last_state = GF_OK;
	isor_reader_release_sample(ch);

	if (ch->static_sample) gf_isom_sample_del(&ch->static_sample);
	ch->sample = NULL;
	ch->sample_num = 0;
	ch->speed = 1.0;
	ch->start = ch->end = 0;
	ch->to_init = 1;
	ch->play_state = 0;
	if (ch->sai_buffer) gf_free(ch->sai_buffer);
	ch->sai_buffer = NULL;
	ch->sai_alloc_size = 0;
	memset(&ch->current_slh, 0, sizeof(GF_SLHeader));
}

void isor_check_producer_ref_time(ISOMReader *read)
{
	u32 trackID;
	u64 ntp;
	u64 timestamp;

	if (gf_isom_get_last_producer_time_box(read->mov, &trackID, &ntp, &timestamp, GF_TRUE)) {
#if !defined(_WIN32_WCE) && !defined(GPAC_DISABLE_LOG)

		if (gf_log_tool_level_on(GF_LOG_DASH, GF_LOG_DEBUG)) {
#ifdef FILTER_FIXME
			time_t secs;
			struct tm t;

			s32 diff = gf_net_get_ntp_diff_ms(ntp);

			if (read->input->query_proxy) {
				GF_NetworkCommand param;
				GF_Err e;
				memset(&param, 0, sizeof(GF_NetworkCommand));
				param.command_type = GF_NET_SERVICE_QUERY_UTC_DELAY;
				e = read->input->query_proxy(read->input, &param);
				if (e == GF_OK) {
					diff -= param.utc_delay.delay;
				}
			}

			secs = (ntp>>32) - GF_NTP_SEC_1900_TO_1970;
			t = *gmtime(&secs);


			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] TrackID %d: Timestamp "LLU" matches sender NTP time %d-%02d-%02dT%02d:%02d:%02dZ - NTP clock diff (local - remote): %d ms\n", trackID, timestamp, 1900+t.tm_year, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, diff));

#endif
		}
#endif

		read->last_sender_ntp = ntp;
		read->cts_for_last_sender_ntp = timestamp;
	}
}


static void init_reader(ISOMChannel *ch)
{
	u32 sample_desc_index=0;

	ch->current_slh.accessUnitEndFlag = 1;
	ch->current_slh.accessUnitStartFlag = 1;
	ch->current_slh.AU_sequenceNumber = 1;
	ch->current_slh.compositionTimeStampFlag = 1;
	ch->current_slh.decodingTimeStampFlag = 1;
	ch->current_slh.packetSequenceNumber = 1;
	ch->current_slh.randomAccessPointFlag = 0;

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
	} else {
		//if seek is disabled, get the next closest sample for this time; otherwose, get the previous RAP sample for this time
		u32 mode = ch->disable_seek ? GF_ISOM_SEARCH_BACKWARD : GF_ISOM_SEARCH_SYNC_BACKWARD;

		/*take care of seeking out of the track range*/
		if (!ch->owner->frag_type && (ch->duration<ch->start)) {
			ch->last_state = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->duration, &sample_desc_index, mode, &ch->static_sample, &ch->sample_num, NULL);
		} else {
			ch->last_state = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->start, &sample_desc_index, mode, &ch->static_sample, &ch->sample_num, NULL);
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

	if (ch->has_edit_list) {
		ch->sample_time = ch->sample->DTS;
	} else {
		//store movie time in media timescale in the sample time, eg no edit list is used but we may have a shift (dts_offset) between
		//movie and media timelines

		if ((ch->dts_offset<0) && (ch->sample->DTS  < (u64) -ch->dts_offset)) {
			ch->sample_time = 0;
			ch->do_dts_shift_test = GF_TRUE;
		} else {
			ch->sample_time = ch->sample->DTS + ch->dts_offset;
		}
	}
	ch->to_init = GF_FALSE;

	ch->current_slh.seekFlag = 0;
	if (ch->disable_seek) {
		ch->current_slh.decodingTimeStamp = ch->sample->DTS;
		ch->current_slh.compositionTimeStamp = ch->sample->DTS + ch->sample->CTS_Offset;
		ch->start = 0;
	} else {
		ch->current_slh.decodingTimeStamp = ch->start;
		ch->current_slh.compositionTimeStamp = ch->start;

		//TODO - we need to notify scene decoder how many secs elapsed between RAP and seek point
		if (ch->current_slh.compositionTimeStamp != ch->sample->DTS + ch->sample->CTS_Offset) {
			ch->current_slh.seekFlag = 1;
		}
	}
	ch->current_slh.randomAccessPointFlag = ch->sample ? ch->sample->IsRAP : 0;
	if (!sample_desc_index) sample_desc_index = 1;
	ch->last_sample_desc_index = sample_desc_index;
	ch->owner->no_order_check = ch->speed < 0 ? GF_TRUE : GF_FALSE;
}

void isor_reader_get_sample_from_item(ISOMChannel *ch)
{
	if (ch->current_slh.AU_sequenceNumber) {
		ch->last_state = GF_EOS;
		return;
	}
	ch->sample_time = 0;
	ch->last_state = GF_OK;
	ch->sample = gf_isom_sample_new();
	ch->sample->IsRAP = RAP;
	ch->current_slh.accessUnitEndFlag = ch->current_slh.accessUnitStartFlag = 1;
	ch->current_slh.au_duration = 1000;
	ch->current_slh.randomAccessPointFlag = ch->sample->IsRAP;
	ch->current_slh.compositionTimeStampFlag = 1;
	ch->current_slh.decodingTimeStampFlag = 1;
	gf_isom_extract_meta_item_mem(ch->owner->mov, GF_TRUE, 0, ch->item_id, &ch->sample->data, &ch->sample->dataLength, NULL, GF_FALSE);
	ch->current_slh.accessUnitLength = ch->sample->dataLength;
}

void isor_reader_get_sample(ISOMChannel *ch)
{
	GF_Err e;
	u32 sample_desc_index;
	Bool is_init = GF_FALSE;
	if (ch->sample) return;

	if (ch->next_track) {
		ch->track = ch->next_track;
		ch->next_track = 0;
	}

	if (ch->to_init) {
		init_reader(ch);
		is_init = GF_TRUE;
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

		if (ch->sample->DTS + ch->dts_offset == ch->sample_time) {
			if (!ch->owner->frag_type) {
				ch->last_state = GF_EOS;
			}
		}
		if (ch->sample) {
			if ((ch->dts_offset<0) && (ch->sample->DTS < (u64) -ch->dts_offset))	//should not happen
				ch->sample_time = 0;
			else
				ch->sample_time = ch->sample->DTS + ch->dts_offset;
		}

	} else if (ch->has_edit_list) {
		u32 prev_sample = ch->sample_num;
		e = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->sample_time + 1, &sample_desc_index, GF_ISOM_SEARCH_FORWARD, &ch->static_sample, &ch->sample_num, NULL);

		if (e == GF_OK) {
			ch->sample = ch->static_sample;

			/*we are in forced seek mode: fetch all samples before the one matching the sample time*/
			if (ch->edit_sync_frame) {
				ch->edit_sync_frame++;
				if (ch->edit_sync_frame < ch->sample_num) {
					ch->sample = gf_isom_get_sample_ex(ch->owner->mov, ch->track, ch->edit_sync_frame, &sample_desc_index, ch->static_sample);
					ch->sample->DTS = ch->sample_time;
					ch->sample->CTS_Offset = 0;
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
						u32 time_diff = 2;
						u32 sample_num = ch->sample_num ? ch->sample_num : 1;

						if (sample_num >= gf_isom_get_sample_count(ch->owner->mov, ch->track) ) {
							//e = GF_EOS;
						} else {
							time_diff = gf_isom_get_sample_duration(ch->owner->mov, ch->track, sample_num);
							e = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->sample_time + time_diff, &sample_desc_index, GF_ISOM_SEARCH_FORWARD, &ch->static_sample, &ch->sample_num, NULL);
							if (e==GF_OK) {
								ch->sample = ch->static_sample;
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
					e = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->sample_time + 1, &sample_desc_index, GF_ISOM_SEARCH_SYNC_BACKWARD, &ch->static_sample, &ch->sample_num, NULL);

					if (e == GF_OK) ch->sample = ch->static_sample;

					/*if no sync point in the past, use the first non-sync for the given time*/
					if (!ch->sample || !ch->sample->data) {
						ch->sample = ch->static_sample = found;
						ch->sample_time = ch->sample->DTS;
						ch->sample_num = samp_num;
					} else {
						gf_isom_sample_del(&found);
						ch->static_sample = ch->sample;
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
		ch->sample_num++;

		ch->sample = gf_isom_get_sample_ex(ch->owner->mov, ch->track, ch->sample_num, &sample_desc_index, ch->static_sample);
		/*if sync shadow / carousel RAP skip*/
		if (ch->sample && (ch->sample->IsRAP==RAP_REDUNDANT)) {
			ch->sample = NULL;
			ch->sample_num++;
			isor_reader_get_sample(ch);
			return;
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
		/*incomplete file - check if we're still downloading or not*/
		if (gf_isom_get_missing_bytes(ch->owner->mov, ch->track)) {
			ch->last_state = GF_ISOM_INCOMPLETE_FILE;
			if (!ch->owner->input_loaded) {
				ch->last_state = GF_OK;
				if (!ch->has_edit_list && ch->sample_num)
					ch->sample_num--;
			}
		}
		else if (!ch->sample_num
		         || ((ch->speed >= 0) && (ch->sample_num >= gf_isom_get_sample_count(ch->owner->mov, ch->track)))
		         || ((ch->speed < 0) && (ch->sample_time == gf_isom_get_current_tfdt(ch->owner->mov, ch->track) + ch->dts_offset))
		        ) {

			if (ch->owner->frag_type==1) {
				/*if sample cannot be found and file is fragmented, rewind sample*/
				if (ch->sample_num) ch->sample_num--;
				ch->last_state = GF_EOS;
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Track #%d end of stream reached\n", ch->track));
				ch->last_state = GF_EOS;
			}
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Track #%d fail to fetch sample %d / %d: %s\n", ch->track, ch->sample_num, gf_isom_get_sample_count(ch->owner->mov, ch->track), gf_error_to_string(gf_isom_last_error(ch->owner->mov)) ));
		}
		return;
	}

	if (sample_desc_index != ch->last_sample_desc_index) {
		u32 mtype = gf_isom_get_media_type(ch->owner->mov, ch->track);
		switch (mtype) {
		case GF_ISOM_MEDIA_VISUAL:
			//code is here as a reminder, but by default we use inband param set extraction so no need for it
#if 0
			if ( ! (ch->nalu_extract_mode & GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG) ) {
				u32 extract_mode = ch->nalu_extract_mode | GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG;

				ch->sample = NULL;
				gf_isom_set_nalu_extract_mode(ch->owner->mov, ch->track, extract_mode);
				ch->sample = gf_isom_get_sample_ex(ch->owner->mov, ch->track, ch->sample_num, &ch->last_sample_desc_index, ch->static_sample);

				gf_isom_set_nalu_extract_mode(ch->owner->mov, ch->track, ch->nalu_extract_mode);
			}
#endif
			break;
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBPIC:
		case GF_ISOM_MEDIA_MPEG_SUBT:
			break;
		default:
			//TODO: do we want to support codec changes ?
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] Change of sample description (%d->%d) for media type %s not supported\n", ch->last_sample_desc_index, sample_desc_index, gf_4cc_to_str(mtype) ));
			ch->sample = NULL;
			ch->last_state = GF_NOT_SUPPORTED;
			return;
		}
	}

	ch->last_state = GF_OK;
	ch->current_slh.accessUnitEndFlag = ch->current_slh.accessUnitStartFlag = 1;
	ch->current_slh.accessUnitLength = ch->sample->dataLength;
	ch->current_slh.au_duration = gf_isom_get_sample_duration(ch->owner->mov, ch->track, ch->sample_num);

	//update timestamp when single edit
	if (ch->sample && ch->dts_offset) {
		if (ch->do_dts_shift_test) {
			s64 DTS, CTS;
			DTS = (s64) ch->sample->DTS + ch->dts_offset;
			CTS = (s64) ch->sample->DTS + ch->dts_offset + (s32) ch->sample->CTS_Offset;
			if (DTS<0)
				DTS=0;
			else
				ch->do_dts_shift_test = GF_FALSE;

			if (CTS<0) CTS=0;
			ch->sample->DTS = (u64) DTS;
			ch->sample->CTS_Offset = (s32) (CTS - DTS);
		}
		//if sample is fetched from init(), dts is already shifted
		else if (!is_init) {
			ch->sample->DTS = ch->sample->DTS + ch->dts_offset;
		}
	}

	/*still seeking or not ?
	 1- when speed is negative, the RAP found is "after" the seek point in playback order since we used backward RAP search: nothing to do
	 2- otherwise set DTS+CTS to start value
	 */
	if ((ch->speed < 0) || (ch->start <= ch->sample->DTS + ch->sample->CTS_Offset)) {
		ch->current_slh.decodingTimeStamp = ch->sample->DTS;
		ch->current_slh.compositionTimeStamp = ch->sample->DTS + ch->sample->CTS_Offset;
		ch->current_slh.seekFlag = 0;
	} else {
		ch->current_slh.compositionTimeStamp = ch->start;
		ch->current_slh.seekFlag = 1;
		ch->current_slh.decodingTimeStamp = ch->start;
	}
	ch->current_slh.randomAccessPointFlag = ch->sample->IsRAP;
	ch->current_slh.OCRflag = ch->owner->clock_discontinuity ? 2 : 0;
	ch->owner->clock_discontinuity = 0;

	//handle negative ctts
	if (ch->current_slh.decodingTimeStamp > ch->current_slh.compositionTimeStamp)
		ch->current_slh.decodingTimeStamp = ch->current_slh.compositionTimeStamp;

	if (ch->end && (ch->end < ch->sample->DTS + ch->sample->CTS_Offset)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] End of Channel "LLD" (CTS "LLD")\n", ch->end, ch->sample->DTS + ch->sample->CTS_Offset));
		ch->last_state = GF_EOS;
	}
	if (ch->owner->last_sender_ntp && ch->current_slh.compositionTimeStamp==ch->owner->cts_for_last_sender_ntp) {
		ch->current_slh.sender_ntp = ch->owner->last_sender_ntp;
	} else {
		ch->current_slh.sender_ntp = 0;
	}


	if (ch->is_encrypted) {
		GF_ISMASample *ismasamp = gf_isom_get_ismacryp_sample(ch->owner->mov, ch->track, ch->sample, 1);
		if (ismasamp) {
			gf_free(ch->sample->data);
			ch->sample->data = ismasamp->data;
			ch->sample->dataLength = ismasamp->dataLength;
			ismasamp->data = NULL;
			ismasamp->dataLength = 0;
			ch->current_slh.isma_encrypted = (ismasamp->flags & GF_ISOM_ISMA_IS_ENCRYPTED) ? 1 : 0;
			ch->current_slh.isma_BSO = ismasamp->IV;
			gf_isom_ismacryp_delete_sample(ismasamp);
		} else {
			ch->current_slh.isma_encrypted = 0;
			/*in case of CENC: we write sample auxiliary information to slh->sai; its size is in saiz*/
			if (gf_isom_is_cenc_media(ch->owner->mov, ch->track, 1)) {
				u32 Is_Encrypted;
				u8 IV_size;
				bin128 KID;
				u8 crypt_bytr_block, skip_byte_block;
				u8 constant_IV_size;
				bin128 constant_IV;

				gf_isom_get_sample_cenc_info(ch->owner->mov, ch->track, ch->sample_num, &Is_Encrypted, &IV_size, &KID, &crypt_bytr_block, &skip_byte_block, &constant_IV_size, &constant_IV);
				ch->current_slh.IV_size = IV_size;
				ch->current_slh.crypt_byte_block = crypt_bytr_block;
				ch->current_slh.skip_byte_block = skip_byte_block;
				if (Is_Encrypted && !ch->current_slh.IV_size) {
					ch->current_slh.constant_IV_size = constant_IV_size;
					memmove(ch->current_slh.constant_IV, constant_IV, ch->current_slh.constant_IV_size);
				}
				if (Is_Encrypted) {
					u32 out_size = ch->sai_alloc_size;
					ch->current_slh.cenc_encrypted = 1;

					gf_isom_cenc_get_sample_aux_info_buffer(ch->owner->mov, ch->track, ch->sample_num, NULL, &ch->sai_buffer, &out_size);
					if (out_size > ch->sai_alloc_size) ch->sai_alloc_size = out_size;
					if (out_size) {
						assert(out_size>=16);
						memcpy(ch->sai_buffer, (char *)KID, 16);
						ch->sai_buffer_size = out_size;
					}
				}
				else
					ch->current_slh.cenc_encrypted = 0;
			}
		}
	}
}

void isor_reader_release_sample(ISOMChannel *ch)
{
	ch->sample = NULL;
	ch->sai_buffer_size = 0;
	ch->current_slh.AU_sequenceNumber++;
	ch->current_slh.packetSequenceNumber++;
}


#endif /*GPAC_DISABLE_ISOM*/
