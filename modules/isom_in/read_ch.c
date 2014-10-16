/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / MP4 reader module
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
#include <gpac/network.h>
#include <time.h>

#ifndef GPAC_DISABLE_ISOM


void isor_reset_reader(ISOMChannel *ch)
{
	ch->last_state = GF_OK;
	isor_reader_release_sample(ch);

	ch->sample = NULL;
	ch->sample_num = 0;
	ch->speed = 1.0;
	ch->start = ch->end = 0;
	ch->to_init = 1;
	ch->is_playing = 0;
	memset(&ch->current_slh, 0, sizeof(GF_SLHeader));
}

/*
	refresh type:
		0: not progressive
		1: progressive
		2: not progressive and don't check current download
*/
void isor_segment_switch_or_refresh(ISOMReader *read, u32 refresh_type)
{
	GF_NetworkCommand param;
	u32 i, count;
	Bool scalable_segment = 0;
	GF_Err e;

	/*access to the segment switching must be protected in case several decoders are threaded on the file using GetSLPacket */
	gf_mx_p(read->segment_mutex);

	if (!read->frag_type || !read->input->query_proxy ) {
		gf_mx_v(read->segment_mutex);
		return;
	}

	memset(&param, 0, sizeof(GF_NetworkCommand));
	param.command_type = GF_NET_SERVICE_QUERY_NEXT;
	param.url_query.current_download = (refresh_type==2) ? 0 : 1;
	if (refresh_type==2)
		refresh_type = 0;

	count = gf_list_count(read->channels);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Refresh seg: refresh_type %d - seg opened %d\n", refresh_type , read->seg_opened));

	if (refresh_type && !read->seg_opened)
		refresh_type = 0;

	if ((read->seg_opened==1) && !refresh_type) {
		gf_mx_v(read->segment_mutex);
		return;
	}

	if (read->drop_next_segment) {
		read->drop_next_segment = 0;
		param.url_query.drop_first_segment = 1;
	}

	//if first time trying to fetch next segment, check if we have to discard it
	if (!refresh_type && (read->seg_opened==2)) {
#ifdef DASH_USE_PULL
		for (i=0; i<count; i++) {
			ISOMChannel *ch = gf_list_get(read->channels, i);
			/*check all playing channels are waiting for next segment*/
			if (ch->is_playing && !ch->wait_for_segment_switch) {
				gf_mx_v(read->segment_mutex);
				return;
			}
		}
#endif

		/*close current segment*/
		gf_isom_release_segment(read->mov, 1);
		read->seg_opened = 0;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Done playing segment - querying new one\n"));

		/*drop this segment*/
		param.url_query.drop_first_segment = 1;
	}

next_segment:
	/*update current fragment if any*/
	e = read->input->query_proxy(read->input, &param);
	if (e == GF_OK) {
		u64 timestamp, ntp;
		u32 trackID = 0;
		if (param.url_query.next_url) {
			u32 flags;

			//previously loaded file has been aborted, reload segment !
			if (refresh_type && param.url_query.discontinuity_type) {
				gf_isom_release_segment(read->mov, 1);
				gf_isom_reset_fragment_info(read->mov, 1);
				refresh_type = 0;
			}

			if (read->reset_frag_state) {
				read->reset_frag_state = 0;
				gf_isom_reset_fragment_info(read->mov, 0);
			}

			//refresh file
			if (refresh_type) {
				//the url is the current download or we just finished downloaded it, refresh the parsing.
				if ((param.url_query.current_download || (read->seg_opened==1)) && param.url_query.has_new_data) {
					u64 bytesMissing=0;
					e = gf_isom_refresh_fragmented(read->mov, &bytesMissing, read->use_memory ? param.url_query.next_url : NULL);

					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[IsoMedia] LowLatency mode: Reparsing segment %s boxes at UTC "LLU" - "LLU" bytes still missing\n", param.url_query.next_url, gf_net_get_utc(), bytesMissing ));
					for (i=0; i<count; i++) {
						ISOMChannel *ch = gf_list_get(read->channels, i);
						GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[IsoMedia] refresh track %d fragment - cur sample %d - new sample count %d\n", ch->track, ch->sample_num, gf_isom_get_sample_count(ch->owner->mov, ch->track) ));
					}
				}
				//we did the last refresh and the segment is downloaded, move to fully parsed mode
				if (! param.url_query.current_download) {
					read->seg_opened = 2;
				}
				gf_mx_v(read->segment_mutex);
				return;
			}

			//we have to open the file

			if (param.url_query.discontinuity_type==2) {
				gf_isom_reset_fragment_info(read->mov, 0);
				read->clock_discontinuity = 1;
			}
			e = GF_OK;
			if (param.url_query.next_url_init_or_switch_segment) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Switching between files - opening new init segment %s\n", param.url_query.next_url_init_or_switch_segment));
				if (read->mov) gf_isom_close(read->mov);
				e = gf_isom_open_progressive(param.url_query.next_url_init_or_switch_segment, param.url_query.switch_start_range, param.url_query.switch_end_range, &read->mov, &read->missing_bytes);
				if (e<0) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[IsoMedia] Error opening init segment %s at UTC "LLU": %s\n", param.url_query.next_url_init_or_switch_segment, gf_net_get_utc(), gf_error_to_string(e) ));
				}
			}
			if (!e) {
				flags = 0;
				if (read->no_order_check) flags |= GF_ISOM_SEGMENT_NO_ORDER_FLAG;
				if (scalable_segment) flags |= GF_ISOM_SEGMENT_SCALABLE_FLAG;
				e = gf_isom_open_segment(read->mov, param.url_query.next_url, param.url_query.start_range, param.url_query.end_range, flags);

				if (param.url_query.current_download  && (e==GF_ISOM_INCOMPLETE_FILE)) {
					e = GF_OK;
				}

#ifndef GPAC_DISABLE_LOG
				if (e<0) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[IsoMedia] Error opening new segment %s at UTC "LLU": %s\n", param.url_query.next_url, gf_net_get_utc(), gf_error_to_string(e) ));
				} else if (param.url_query.end_range) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Playing new range in %s: "LLU"-"LLU"\n", param.url_query.next_url, param.url_query.start_range, param.url_query.end_range ));
				} else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] playing new segment %s\n", param.url_query.next_url));
				}
#endif
			}

			if (e<0) {
				gf_isom_release_segment(read->mov, 1);
				//gf_isom_reset_fragment_info(read->mov, 1);
				read->drop_next_segment = 1;
				//error opening the segment, reset everything ...
				gf_isom_reset_fragment_info(read->mov, 0);
				for (i=0; i<count; i++) {
					ISOMChannel *ch = gf_list_get(read->channels, i);
					if (ch)
						ch->sample_num = 0;
				}

				//cannot open file, don't change our state
				gf_mx_v(read->segment_mutex);
				return;
			}

			//segment is the first in our cache, we may need a refresh
			if (param.url_query.current_download ) {
				read->seg_opened = 1;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Opening current segment in progressive mode (download in progress)\n"));
			} else {
				read->seg_opened = 2;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Opening current segment in non-progressive mode (completely downloaded)\n"));
			}

#ifndef _WIN32_WCE
			if (gf_isom_get_last_producer_time_box(read->mov, &trackID, &ntp, &timestamp, 1)) {
				u32 remote_s, remote_f, local_s, local_f;
				s64 diff_s, diff_f;
				time_t secs;
				struct tm t;
				remote_s = (ntp>>32);
				remote_f = (u32) (ntp & 0xFFFFFFFFULL);
				gf_net_get_ntp(&local_s, &local_f);
				diff_s = local_s;
				diff_s -= remote_s;
				diff_s *= 1000;
				diff_f = local_f;
				diff_f -= remote_f;
				diff_f *= 1000;
				diff_f /= 0xFFFFFFFFULL;
				diff_s += diff_f;

				secs = remote_s - GF_NTP_SEC_1900_TO_1970;
				t = *gmtime(&secs);

				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] TrackID %d: Timestamp %d matches sender NTP time %d-%02d-%02dT%02d:%02d:%02dZ - NTP clock diff (local - remote): %d ms\n", trackID, (u32) timestamp, 1900+t.tm_year, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, diff_s ));
			}
#endif

			trackID = 0;
			for (i=0; i<count; i++) {
				ISOMChannel *ch = gf_list_get(read->channels, i);
				ch->wait_for_segment_switch = 0;

				if (scalable_segment) {
					trackID = gf_isom_get_highest_track_in_scalable_segment(read->mov, ch->base_track);
					if (trackID) {
						ch->track_id = trackID;
						ch->track = gf_isom_get_track_by_id(read->mov, ch->track_id);
					}
				}
				else if (ch->base_track) {
					ch->track = ch->base_track;
					ch->track_id = gf_isom_get_track_id(read->mov, ch->track);
				}

				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Track %d - cur sample %d - new sample count %d\n", ch->track, ch->sample_num, gf_isom_get_sample_count(ch->owner->mov, ch->track) ));
				if (param.url_query.next_url_init_or_switch_segment) {
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
				}
				//a loop was detected, our timing is no longer reliable if we use edit lists - just reset the sample time to tfdt ...
				else if (param.url_query.discontinuity_type==2) {
					ch->sample_num = 0;
					if (ch->has_edit_list) {
						ch->sample_time = gf_isom_get_current_tfdt(read->mov, ch->track);
						//next read will query sample for ch->sample_time + 1
						if (ch->sample_time) ch->sample_time--;
					}
				}
				/*rewrite all upcoming SPS/PPS into the samples*/
				gf_isom_set_nalu_extract_mode(read->mov, ch->track, ch->nalu_extract_mode);
				ch->last_state = GF_OK;


				if (ch->is_cenc) {
					isor_send_cenc_config(ch);
				}

			}

			read->use_memory = !strncmp(param.url_query.next_url, "gmem://", 7) ? GF_TRUE : GF_FALSE;
		}
		if (param.url_query.has_next) {
			param.url_query.drop_first_segment = GF_FALSE;
			param.url_query.dependent_representation_index++;
			scalable_segment = 1;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Enhancement layer available in cache - refreshing it\n"));
			goto next_segment;
		}
		read->waiting_for_data = GF_FALSE;
	} else if (e==GF_EOS) {
		/*consider we are done*/
		read->frag_type = 2;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] No more segments - done playing file\n"));
	} else if (e==GF_BUFFER_TOO_SMALL) {
		if (refresh_type==0) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Next segment is not yet available\n"));
			read->waiting_for_data = GF_TRUE;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[IsoMedia] Corrupted state in low latency mode !!\n"));
		}
	} else {
		/*consider we are done*/
		read->frag_type = 2;
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[IsoMedia] Error fetching next DASH segment: no more segments\n"));
	}
	gf_mx_v(read->segment_mutex);
}

static void init_reader(ISOMChannel *ch)
{
	u32 ivar;
	if (ch->is_pulling && ch->wait_for_segment_switch) {
		isor_segment_switch_or_refresh(ch->owner, 0);
		if (ch->wait_for_segment_switch)
			return;
	}

	ch->current_slh.accessUnitEndFlag = 1;
	ch->current_slh.accessUnitStartFlag = 1;
	ch->current_slh.AU_sequenceNumber = 1;
	ch->current_slh.compositionTimeStampFlag = 1;
	ch->current_slh.decodingTimeStampFlag = 1;
	ch->current_slh.packetSequenceNumber = 1;
	ch->current_slh.randomAccessPointFlag = 0;

	assert(ch->sample==NULL);

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
			ch->last_state = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->duration, &ivar, mode, &ch->sample, &ch->sample_num);
		} else {
			ch->last_state = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->start, &ivar, mode, &ch->sample, &ch->sample_num);
		}
		ch->last_state = GF_OK;

		if (ch->has_rap && ch->has_edit_list) {
			ch->edit_sync_frame = ch->sample_num;
		}
	}

	/*no sample means we're not in the track range - stop*/
	if (!ch->sample) {
		/*incomplete file - check if we're still downloading or not*/
		if (gf_isom_get_missing_bytes(ch->owner->mov, ch->track)) {
			GF_NetIOStatus net_status;
			gf_dm_sess_get_stats(ch->owner->dnload, NULL, NULL, NULL, NULL, NULL, &net_status);
			if (net_status == GF_NETIO_DATA_EXCHANGE) {
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

		if ((ch->dts_offset<0) && (ch->sample->DTS  < (u64) -ch->dts_offset))	//should not happen
			ch->sample_time = 0;
		else
			ch->sample_time = ch->sample->DTS + ch->dts_offset;
	}
	ch->to_init = 0;

	if (ch->disable_seek) {
		ch->current_slh.decodingTimeStamp = ch->sample->DTS;
		ch->current_slh.compositionTimeStamp = ch->sample->DTS + ch->sample->CTS_Offset;
		ch->start = 0;
	} else {
		ch->current_slh.decodingTimeStamp = ch->start;
		ch->current_slh.compositionTimeStamp = ch->start;
	}
	ch->current_slh.randomAccessPointFlag = ch->sample ? ch->sample->IsRAP : 0;

	ch->owner->no_order_check = ch->speed < 0 ? GF_TRUE : GF_FALSE;
}



void isor_reader_get_sample(ISOMChannel *ch)
{
	GF_Err e;
	u32 ivar;
	if (ch->sample) return;

	if (ch->next_track) {
		ch->track = ch->next_track;
		ch->next_track = 0;
	}

	if ((ch->owner->seg_opened==1) && ch->is_pulling) {
		isor_segment_switch_or_refresh(ch->owner, 1);
	}

	if (ch->to_init) {
		init_reader(ch);
	} else if (ch->speed < 0) {
		e = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->sample_time - 1, &ivar, GF_ISOM_SEARCH_SYNC_BACKWARD, &ch->sample, &ch->sample_num);
		if (e) {
			if (e==GF_EOS) {
				ch->last_state = GF_EOS;
				return;
			}
			return;
		}
		if (ch->sample->DTS + ch->dts_offset == ch->sample_time) {
			if (!ch->owner->frag_type) {
				ch->last_state = GF_EOS;
			} else {
				if (ch->sample)
					gf_isom_sample_del(&ch->sample);
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
		e = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->sample_time + 1, &ivar, GF_ISOM_SEARCH_FORWARD, &ch->sample, &ch->sample_num);

		if (e == GF_OK) {

			/*we are in forced seek mode: fetch all samples before the one matching the sample time*/
			if (ch->edit_sync_frame) {
				ch->edit_sync_frame++;
				if (ch->edit_sync_frame < ch->sample_num) {
					gf_isom_sample_del(&ch->sample);
					ch->sample = gf_isom_get_sample(ch->owner->mov, ch->track, ch->edit_sync_frame, &ivar);
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
						if (ch->sample)
							gf_isom_sample_del(&ch->sample);
					} else {
						u32 time_diff = 2;
						u32 sample_num = ch->sample_num ? ch->sample_num : 1;
						GF_ISOSample *s1 = gf_isom_get_sample(ch->owner->mov, ch->track, sample_num, NULL);
						GF_ISOSample *s2 = gf_isom_get_sample(ch->owner->mov, ch->track, sample_num+1, NULL);

						gf_isom_sample_del(&ch->sample);

						if (s2 && s1) {
							assert(s2->DTS >= s1->DTS);
							time_diff = (u32) (s2->DTS - s1->DTS);
							e = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->sample_time + time_diff, &ivar, GF_ISOM_SEARCH_FORWARD, &ch->sample, &ch->sample_num);
						} else if (s1 && !s2) {
							e = GF_EOS;
						}
						gf_isom_sample_del(&s1);
						gf_isom_sample_del(&s2);

					}
				}

				/*we jumped to another segment - if RAP is needed look for closest rap in decoding order and
				force seek mode*/
				if (ch->sample && !ch->sample->IsRAP && ch->has_rap && (ch->sample_num != prev_sample+1)) {
					GF_ISOSample *found = ch->sample;
					u32 samp_num = ch->sample_num;
					ch->sample = NULL;
					e = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->sample_time + 1, &ivar, GF_ISOM_SEARCH_SYNC_BACKWARD, &ch->sample, &ch->sample_num);
					assert (e == GF_OK);
					/*if no sync point in the past, use the first non-sync for the given time*/
					if (!ch->sample || !ch->sample->data) {
						gf_isom_sample_del(&ch->sample);
						ch->sample = found;
						ch->sample_time = ch->sample->DTS;
						ch->sample_num = samp_num;
					} else {
						gf_isom_sample_del(&found);
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

		ch->sample = gf_isom_get_sample(ch->owner->mov, ch->track, ch->sample_num, &ivar);
		/*if sync shadow / carousel RAP skip*/
		if (ch->sample && (ch->sample->IsRAP==RAP_REDUNDANT)) {
			gf_isom_sample_del(&ch->sample);
			ch->sample_num++;
			isor_reader_get_sample(ch);
			return;
		}
	}

	//check scalable track change
	if (ch->sample && ch->sample->IsRAP && ch->next_track) {
		ch->track = ch->next_track;
		ch->next_track = 0;
		gf_isom_sample_del(&ch->sample);
		isor_reader_get_sample(ch);
		return;
	}
	if (ch->sample && ch->dts_offset) {
		if ( (ch->dts_offset<0) && (ch->sample->DTS < (u64) -ch->dts_offset)) {
			ch->sample->DTS = 0;
		} else {
			ch->sample->DTS += ch->dts_offset;
		}
	}

	if (!ch->sample) {
		/*incomplete file - check if we're still downloading or not*/
		if (gf_isom_get_missing_bytes(ch->owner->mov, ch->track)) {
			ch->last_state = GF_ISOM_INCOMPLETE_FILE;
			if (ch->owner->dnload) {
				GF_NetIOStatus net_status;
				gf_dm_sess_get_stats(ch->owner->dnload, NULL, NULL, NULL, NULL, NULL, &net_status);
				if (net_status == GF_NETIO_DATA_EXCHANGE) {
					ch->last_state = GF_OK;
					if (!ch->has_edit_list)
						ch->sample_num--;
				}
			}
			else if (ch->owner->input->query_proxy) {
				ch->last_state = GF_OK;
			}
		}
		else if (!ch->sample_num
		         || ((ch->speed >= 0) && (ch->sample_num >= gf_isom_get_sample_count(ch->owner->mov, ch->track)))
		         || ((ch->speed < 0) && (ch->sample_time == gf_isom_get_current_tfdt(ch->owner->mov, ch->track) + ch->dts_offset))
		        ) {

			if (ch->owner->frag_type==1) {
				//if segment is fully opened and no more data, this track is done, wait for next segment
				if (!ch->wait_for_segment_switch && ch->owner->input->query_proxy && (ch->owner->seg_opened==2) ) {
					ch->wait_for_segment_switch = 1;
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Track #%d end of segment reached - waiting for sample %d - current count %d\n", ch->track, ch->sample_num, gf_isom_get_sample_count(ch->owner->mov, ch->track) ));
				}
				/*if sample cannot be found and file is fragmented, rewind sample*/
				if (ch->sample_num) ch->sample_num--;
				ch->last_state = GF_OK;
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Track #%d end of stream reached\n", ch->track));
				ch->last_state = GF_EOS;
			}
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Track #%d fail to fetch sample %d / %d: %s\n", ch->track, ch->sample_num, gf_isom_get_sample_count(ch->owner->mov, ch->track), gf_error_to_string(gf_isom_last_error(ch->owner->mov)) ));
		}
		if (ch->wait_for_segment_switch && ch->is_pulling) {
			isor_segment_switch_or_refresh(ch->owner, 0);
			if (ch->owner->seg_opened) {
				isor_reader_get_sample(ch);
				return;
			}
		}
		return;
	}
	ch->last_state = GF_OK;
	ch->current_slh.accessUnitEndFlag = ch->current_slh.accessUnitStartFlag = 1;
	ch->current_slh.accessUnitLength = ch->sample->dataLength;
	ch->current_slh.au_duration = gf_isom_get_sample_duration(ch->owner->mov, ch->track, ch->sample_num);

	/*still seeking or not ?
	 1- when speed is negative, the RAP found is "after" the seek point in playback order since we used backward RAP search: nothing to do
	 2- otherwise set DTS+CTS to start value - !! FIXME we should not change the TS but rather signal the frame is a seek frame
	 */
	if ((ch->speed < 0) || (ch->start <= ch->sample->DTS + ch->sample->CTS_Offset)) {
		ch->current_slh.decodingTimeStamp = ch->sample->DTS;
		ch->current_slh.compositionTimeStamp = ch->sample->DTS + ch->sample->CTS_Offset;
	} else {
		ch->current_slh.compositionTimeStamp = ch->start;
		if (ch->streamType==GF_STREAM_SCENE)
			ch->current_slh.decodingTimeStamp = ch->sample->DTS;
		else
			ch->current_slh.decodingTimeStamp = ch->start;
	}
	ch->current_slh.randomAccessPointFlag = ch->sample->IsRAP;
	ch->current_slh.OCRflag = ch->owner->clock_discontinuity ? 2 : 0;
	ch->owner->clock_discontinuity = 0;

	if (ch->end && (ch->end < ch->sample->DTS + ch->sample->CTS_Offset)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] End of Channel "LLD" (CTS "LLD")\n", ch->end, ch->sample->DTS + ch->sample->CTS_Offset));
		ch->last_state = GF_EOS;
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
				GF_CENCSampleAuxInfo *sai;
				u32 Is_Encrypted;
				u8 IV_size;
				bin128 KID;

				gf_isom_get_sample_cenc_info(ch->owner->mov, ch->track, ch->sample_num, &Is_Encrypted, &IV_size, &KID);
				ch->current_slh.IV_size = IV_size;
				if (Is_Encrypted) {
					ch->current_slh.cenc_encrypted = 1;
					sai = NULL;
					gf_isom_cenc_get_sample_aux_info(ch->owner->mov, ch->track, ch->sample_num, &sai, NULL);
					if (sai) {
						GF_BitStream *bs;
						bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
						/*write sample auxiliary information*/
						gf_bs_write_data(bs, (char *)KID, 16);
						gf_bs_write_data(bs, (char *)sai->IV, IV_size);
						gf_bs_write_u16(bs, sai->subsample_count);
						for (ivar = 0; ivar < sai->subsample_count; ivar++) {
							gf_bs_write_u16(bs, sai->subsamples[ivar].bytes_clear_data);
							gf_bs_write_u32(bs, sai->subsamples[ivar].bytes_encrypted_data);
						}
						gf_bs_get_content(bs, &ch->current_slh.sai, &ch->current_slh.saiz);
						gf_bs_del(bs);
						gf_isom_cenc_samp_aux_info_del(sai);
						sai = NULL;
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
	if (ch->current_slh.sai) {
		gf_free(ch->current_slh.sai);
		ch->current_slh.sai = NULL;
	}
	if (ch->sample) gf_isom_sample_del(&ch->sample);
	ch->sample = NULL;
	ch->current_slh.AU_sequenceNumber++;
	ch->current_slh.packetSequenceNumber++;
}


void isor_flush_data(ISOMReader *read, Bool check_buffer_level, Bool is_chunk_flush)
{
	u32 i, count;
	GF_Err e = GF_OK;
	Bool in_progressive_mode;
	GF_NetworkCommand com;
	ISOMChannel *ch;

	if (read->in_data_flush) {
		if (check_buffer_level && !is_chunk_flush) read->has_pending_segments++;
		return;
	}

	//if another thread grabs the mutex at the same time, just return
	if (!gf_mx_try_lock(read->segment_mutex)) {
		if (check_buffer_level && !is_chunk_flush) read->has_pending_segments++;
		return;
	}
	read->in_data_flush = 1;
	count = gf_list_count(read->channels);

	in_progressive_mode = (read->seg_opened==1) ? 1 : 0;

	if (in_progressive_mode) {
		//query from terminal - do nothing if we are not done downloading the segment - otherwise we could access media data while it is reallocated by the downloader
		if (!check_buffer_level) {
			read->in_data_flush = 0;
			gf_mx_v(read->segment_mutex);
			return;
		}
		//otherwise this is new chunk or end of chunk, process
	}
	//this is a new file, check buffer level
	else if (!is_chunk_flush && check_buffer_level) {
		Bool buffer_full = 1;
		for (i=0; i<count; i++) {
			ch = (ISOMChannel *)gf_list_get(read->channels, i);
			/*query buffer level on each channel, don't sleep if too low*/
			memset(&com, 0, sizeof(GF_NetworkCommand));
			com.command_type = GF_NET_BUFFER_QUERY;
			com.base.on_channel = ch->channel;
			gf_service_command(read->service, &com, GF_OK);
			if ((com.buffer.occupancy < MAX(com.buffer.max, 1000) )) {
				buffer_full = 0;
				break;
			}
		}
		if (buffer_full) {
			read->has_pending_segments++;
			read->in_data_flush = 0;
			gf_mx_v(read->segment_mutex);
			if (count) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[IsoMedia] Buffer level %d ms higher than max allowed %d ms - skipping dispatch\n", com.buffer.occupancy,  com.buffer.max));
			}
			return;
		}
	}
	//flush request from terminal: only process if nothing is opened and we have pending segments
	//we have to keep the polling event when no segments are pending, in order to detect period switch - we therefore tolerate a couble of requests even though no segments are pending
	if (!check_buffer_level && !read->seg_opened && !read->has_pending_segments && (read->nb_force_flush > 2) && !read->drop_next_segment) {
		read->in_data_flush = 0;
		gf_mx_v(read->segment_mutex);
		return;
	}

	//if this is a request from terminal to flush pending segments, do not attempt to open the current download one, only open the first available in the cache
	if (!check_buffer_level && !in_progressive_mode)
		in_progressive_mode = 2;

	//update data
	isor_segment_switch_or_refresh(read, in_progressive_mode);

	//for all channels, fetch and send ...
	count = gf_list_count(read->channels);
	for (i=0; i<count; i++) {
		ch = (ISOMChannel *)gf_list_get(read->channels, i);

		while (!ch->sample) {
			isor_reader_get_sample(ch);
			if (!ch->sample) break;
			if (ch->is_playing)
				gf_service_send_packet(read->service, ch->channel, ch->sample->data, ch->sample->dataLength, &ch->current_slh, GF_OK);
			isor_reader_release_sample(ch);
		}
		if (!ch->sample && (ch->last_state==GF_EOS)) {
			gf_service_send_packet(read->service, ch->channel, NULL, 0, NULL, GF_EOS);
		}
	}

	//done playing the file after notification from DASH client:
	if (read->seg_opened==2) {
		GF_NetworkCommand param;
		//drop this segment
		memset(&param, 0, sizeof(GF_NetworkCommand));
		param.command_type = GF_NET_SERVICE_QUERY_NEXT;
		param.url_query.dependent_representation_index = 0;
		param.url_query.drop_first_segment = 1;
		e = read->input->query_proxy(read->input, &param);

		read->nb_force_flush = 0;
		if (read->has_pending_segments) {
			read->has_pending_segments--;
		}

		if (e==GF_EOS) {
			for (i=0; i<count; i++) {
				ch = (ISOMChannel *)gf_list_get(read->channels, i);
				gf_service_send_packet(read->service, ch->channel, NULL, 0, NULL, GF_EOS);
			}
		}
		//not enough data
		else if (param.url_query.in_end_of_period) {
			//act as if we have a pending segment to process at next call until we get to the next period
			if (!read->has_pending_segments)
				read->has_pending_segments++;
		}

		/*close current segment*/
		gf_isom_release_segment(read->mov, 1);
		read->seg_opened = 0;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Done playing segment \n"));
	}
	read->in_data_flush = 0;

	if (!read->has_pending_segments) {
		read->nb_force_flush ++;
	}
	gf_mx_v(read->segment_mutex);
}

#endif /*GPAC_DISABLE_ISOM*/
