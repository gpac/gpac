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

#ifndef GPAC_DISABLE_ISOM


void isor_reset_reader(ISOMChannel *ch)
{
	memset(&ch->current_slh, 0, sizeof(GF_SLHeader));
	ch->last_state = GF_OK;
	if (ch->sample) gf_isom_sample_del(&ch->sample);
	ch->sample = NULL;
	ch->sample_num = 0;
	ch->speed = 1.0;
	ch->start = ch->end = 0;
	ch->to_init = 1;
	ch->is_playing = 0;
}

void isor_segment_switch_or_refresh(ISOMReader *read, u32 progressive_refresh)
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
	param.url_query.dependent_representation_index = 0;

	count = gf_list_count(read->channels);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Refresh seg: progressive %d - seg opened %d\n", progressive_refresh , read->seg_opened));

	if (progressive_refresh && !read->seg_opened)
		progressive_refresh = 0;

	if ((read->seg_opened==1) && !progressive_refresh) {
		gf_mx_v(read->segment_mutex);
		return;
	}

	//if first time trying to fetch next segment, check if we have to discard it
	if (!progressive_refresh && (read->seg_opened==2)) {
		for (i=0; i<count; i++) {
			ISOMChannel *ch = gf_list_get(read->channels, i);
			/*check all playing channels are waiting for next segment*/
			if (ch->is_playing && !ch->wait_for_segment_switch) {
				gf_mx_v(read->segment_mutex);
				return;
			}
		}

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
		u32 trackID = 0;
		if (param.url_query.next_url){

			//previously loaded file has been aborted, reload segment !
			if (progressive_refresh && param.url_query.discontinuity_type) {
				gf_isom_release_segment(read->mov, 1);
				gf_isom_reset_fragment_info(read->mov, 1);
				progressive_refresh = 0;
			}

			//refresh file
			if (progressive_refresh) {
				//the url is the current download or we just finished downloaded it, refresh the parsing.
				if ((param.url_query.is_current_download || (read->seg_opened==1)) && param.url_query.has_new_data) {
					u64 bytesMissing;
					e = gf_isom_refresh_fragmented(read->mov, &bytesMissing, read->use_memory ? param.url_query.next_url : NULL);

					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] LowLatency mode: Reparsing segment %s boxes at UTC "LLU" - "LLU" bytes still missing\n", param.url_query.next_url, gf_net_get_utc(), bytesMissing ));
					for (i=0; i<count; i++) {
						ISOMChannel *ch = gf_list_get(read->channels, i);
						GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] refresh track %d fragment - cur sample %d - new sample count %d\n", ch->track, ch->sample_num, gf_isom_get_sample_count(ch->owner->mov, ch->track) ));
					}
				}
				//we did the last refresh and the segment is downloaded, move to fully parsed mode
				if (! param.url_query.is_current_download) {
					read->seg_opened = 2;
				}
				gf_mx_v(read->segment_mutex);
				return;
			}

			//we have to open the file

			if (param.url_query.discontinuity_type==2)
				gf_isom_reset_fragment_info(read->mov, 0);

			if (param.url_query.next_url_init_or_switch_segment) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Switching between files - opening new init segment %s\n", param.url_query.next_url_init_or_switch_segment));
				if (read->mov) gf_isom_close(read->mov);
				e = gf_isom_open_progressive(param.url_query.next_url_init_or_switch_segment, param.url_query.switch_start_range, param.url_query.switch_end_range, &read->mov, &read->missing_bytes);
			}

			e = gf_isom_open_segment(read->mov, param.url_query.next_url, param.url_query.start_range, param.url_query.end_range, scalable_segment);

			if (param.url_query.is_current_download  && (e==GF_ISOM_INCOMPLETE_FILE)) {
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

			if (e<0) {
				gf_isom_release_segment(read->mov, 1);
				gf_isom_reset_fragment_info(read->mov, 1);
				//cannot open file, don't change our state
				gf_mx_v(read->segment_mutex);
				return;
			}

			//segment is the first in our cache, we may need a refresh
			if (param.url_query.is_current_download ) {
				read->seg_opened = 1;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Opening current segment in progressive mode (download in progress)\n"));
			} else {
				read->seg_opened = 2;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Opening current segment in npn-progressive mode (completely downloaded)\n"));
			}

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
				/*rewrite all upcoming SPS/PPS into the samples*/
				gf_isom_set_nalu_extract_mode(read->mov, ch->track, GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG);
				ch->last_state = GF_OK;
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
	} else if (e==GF_BUFFER_TOO_SMALL){
		if (progressive_refresh==0) {
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
	if (ch->wait_for_segment_switch) {
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
		ch->sample->IsRAP = 1;
		ch->sample->DTS = ch->start;
		ch->last_state=GF_OK;
	} else {
		/*take care of seeking out of the track range*/
		if (!ch->owner->frag_type && (ch->duration<ch->start)) {
			ch->last_state = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->duration, &ivar, GF_ISOM_SEARCH_SYNC_BACKWARD, &ch->sample, &ch->sample_num);
		} else {
			ch->last_state = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->start, &ivar, GF_ISOM_SEARCH_SYNC_BACKWARD, &ch->sample, &ch->sample_num);
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
			u32 net_status;
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
	} else {
		ch->sample_time = ch->sample->DTS;
		ch->to_init = 0;
	}
	ch->current_slh.decodingTimeStamp = ch->start;
	ch->current_slh.compositionTimeStamp = ch->start;
	ch->current_slh.randomAccessPointFlag = ch->sample ? ch->sample->IsRAP : 0;

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

	if (ch->owner->seg_opened==1) {
		isor_segment_switch_or_refresh(ch->owner, 1);
	}

	if (ch->to_init) {
		init_reader(ch);
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
fetch_next:
		ch->sample = gf_isom_get_sample(ch->owner->mov, ch->track, ch->sample_num, &ivar);
		
		/*if sync shadow / carousel RAP skip*/
		if (ch->sample && (ch->sample->IsRAP==2)) {
			gf_isom_sample_del(&ch->sample);
			ch->sample_num++;
			goto fetch_next;
		}
		if (ch->sample && ch->sample->IsRAP && ch->next_track) {
			ch->track = ch->next_track;
			ch->next_track = 0;
			gf_isom_sample_del(&ch->sample);
			goto fetch_next;
		}
		if (ch->sample && ch->dts_offset) {
			if ( (ch->dts_offset<0) && (ch->sample->DTS < (u64) -ch->dts_offset)) {
				ch->sample->DTS = 0;
			} else {
				ch->sample->DTS += ch->dts_offset;
			}
		}
	}
	if (!ch->sample) {
		/*incomplete file - check if we're still downloading or not*/
		if (gf_isom_get_missing_bytes(ch->owner->mov, ch->track)) {
			ch->last_state = GF_ISOM_INCOMPLETE_FILE;
			if (ch->owner->dnload) {
				u32 net_status;
				gf_dm_sess_get_stats(ch->owner->dnload, NULL, NULL, NULL, NULL, NULL, &net_status);
				if (net_status == GF_NETIO_DATA_EXCHANGE) {
					ch->last_state = GF_OK;
				}
			}
		} else if (!ch->sample_num || (ch->sample_num >= gf_isom_get_sample_count(ch->owner->mov, ch->track))) {
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
		if (ch->wait_for_segment_switch)
			isor_segment_switch_or_refresh(ch->owner, 0);
		return;
	}
	ch->last_state = GF_OK;
	ch->current_slh.accessUnitLength = ch->sample->dataLength;
	/*still seeking or not ?*/
	if (ch->start <= ch->sample->DTS + ch->sample->CTS_Offset) {
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
		}
	}
}

void isor_reader_release_sample(ISOMChannel *ch)
{
	if (ch->sample) gf_isom_sample_del(&ch->sample);
	ch->sample = NULL;
	ch->current_slh.AU_sequenceNumber++;
	ch->current_slh.packetSequenceNumber++;
}



#endif /*GPAC_DISABLE_ISOM*/
