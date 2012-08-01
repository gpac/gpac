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

static void check_segment_switch(ISOMReader *read)
{
	GF_NetworkCommand param;
	u32 i, count;
	GF_Err e;

	/*access to the segment switching must be protected in case several decoders are threaded on the file using GetSLPacket */
	gf_mx_p(read->segment_mutex);

	if (!read->frag_type || !read->input->query_proxy) {
		gf_mx_v(read->segment_mutex);
		return;
	}

	count = gf_list_count(read->channels);
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
	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[IsoMedia] Done playing segment - querying new one\n"));

	/*update current fragment if any*/
	param.command_type = GF_NET_SERVICE_QUERY_NEXT;
	if ((read->input->query_proxy(read->input, &param)==GF_OK) && param.url_query.next_url){
		if (param.url_query.discontinuity_type==2)
			gf_isom_reset_fragment_info(read->mov);

		if (param.url_query.next_url_init_or_switch_segment) {
			if (read->mov) gf_isom_close(read->mov);
			e = gf_isom_open_progressive(param.url_query.next_url_init_or_switch_segment, param.url_query.switch_start_range, param.url_query.switch_end_range, &read->mov, &read->missing_bytes);
		}

		e = gf_isom_open_segment(read->mov, param.url_query.next_url, param.url_query.start_range, param.url_query.end_range);

#ifndef GPAC_DISABLE_LOG
		if (e<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[IsoMedia] Error opening new segment %s: %s\n", param.url_query.next_url, gf_error_to_string(e) ));
		} else if (param.url_query.end_range) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[IsoMedia] Playing new range in %s: "LLU"-"LLU"\n", param.url_query.next_url, param.url_query.start_range, param.url_query.end_range ));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[IsoMedia] playing new segment %s\n", param.url_query.next_url));
		}
#endif

		for (i=0; i<count; i++) {
			ISOMChannel *ch = gf_list_get(read->channels, i);
			ch->wait_for_segment_switch = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[IsoMedia] Track %d - cur sample %d - new sample count %d\n", ch->track, ch->sample_num, gf_isom_get_sample_count(ch->owner->mov, ch->track) ));
			if (param.url_query.next_url_init_or_switch_segment) {
				ch->needs_codec_update = 1;
			}
		}
	} else {
		/*consider we are done*/
		read->frag_type = 2;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[IsoMedia] No more segments - done playing file\n"));
	}
	gf_mx_v(read->segment_mutex);
}

static void init_reader(ISOMChannel *ch)
{
	u32 ivar;
	if (ch->wait_for_segment_switch) {
		check_segment_switch(ch->owner);
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
		}
	} else {
		ch->sample_time = ch->sample->DTS;
	}
	ch->to_init = 0;
	ch->current_slh.decodingTimeStamp = ch->start;
	ch->current_slh.compositionTimeStamp = ch->start;
	ch->current_slh.randomAccessPointFlag = ch->sample ? ch->sample->IsRAP : 0;

}



void isor_reader_get_sample(ISOMChannel *ch)
{
	GF_Err e;
	u32 ivar;
	if (ch->sample) return;

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
	}
	if (!ch->sample) {
		/*incomplete file - check if we're still downloading or not*/
		if (gf_isom_get_missing_bytes(ch->owner->mov, ch->track)) {
			u32 net_status;
			gf_dm_sess_get_stats(ch->owner->dnload, NULL, NULL, NULL, NULL, NULL, &net_status);
			if (net_status == GF_NETIO_DATA_EXCHANGE) {
				ch->last_state = GF_OK;
			} else {
				ch->last_state = GF_ISOM_INCOMPLETE_FILE;
			}
		} else if (!ch->sample_num || (ch->sample_num >= gf_isom_get_sample_count(ch->owner->mov, ch->track))) {
			if (ch->owner->frag_type==1) {
				if (!ch->wait_for_segment_switch) {
					ch->wait_for_segment_switch = 1;
					GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[IsoMedia] Track #%d end of segment reached - waiting for sample %d - current count %d\n", ch->track, ch->sample_num, gf_isom_get_sample_count(ch->owner->mov, ch->track) ));
				}
				/*if sample cannot be found and file is fragmented, rewind sample*/
				if (ch->sample_num) ch->sample_num--;
				ch->last_state = GF_OK;
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[IsoMedia] Track #%d end of stream reached\n", ch->track));
				ch->last_state = GF_EOS;
			}
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[IsoMedia] Track #%d fail to fetch sample %d / %d: %s\n", ch->track, ch->sample_num, gf_isom_get_sample_count(ch->owner->mov, ch->track), gf_error_to_string(gf_isom_last_error(ch->owner->mov)) ));
		}
		if (ch->wait_for_segment_switch)
			check_segment_switch(ch->owner);
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
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[IsoMedia] End of Channel "LLD" (CTS "LLD")\n", ch->end, ch->sample->DTS + ch->sample->CTS_Offset));
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

	/*this is ugly we need a rearchitecture of the streaming part of GPAC to handle code changes properly !! */
	if (ch->sample && ch->needs_codec_update) {
		GF_AVCConfig *avccfg, *svccfg;
		GF_AVCConfigSlot *slc;
		GF_BitStream *bs;
		u32 i; 
		ch->needs_codec_update = 0;

		switch (gf_isom_get_media_subtype(ch->owner->mov, ch->track, 1)) {
		case GF_ISOM_SUBTYPE_AVC_H264:
		case GF_ISOM_SUBTYPE_AVC2_H264:
		case GF_ISOM_SUBTYPE_SVC_H264:
			avccfg = gf_isom_avc_config_get(ch->owner->mov, ch->track, 1);
			svccfg = gf_isom_avc_config_get(ch->owner->mov, ch->track, 1);

			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			if (avccfg) {
				for (i=0; i<gf_list_count(avccfg->sequenceParameterSets); i++) {
					slc = gf_list_get(avccfg->sequenceParameterSets, i);
					gf_bs_write_int(bs, slc->size, avccfg->nal_unit_size*8);
					gf_bs_write_data(bs, slc->data, slc->size);
				}
			}
			if (svccfg) {
				for (i=0; i<gf_list_count(avccfg->sequenceParameterSets); i++) {
					slc = gf_list_get(avccfg->sequenceParameterSets, i);
					gf_bs_write_int(bs, slc->size, avccfg->nal_unit_size*8);
					gf_bs_write_data(bs, slc->data, slc->size);
				}
			}
			gf_bs_write_data(bs, ch->sample->data, ch->sample->dataLength);
			gf_free(ch->sample->data);
			ch->sample->data = 0;
			gf_bs_get_content(bs, &ch->sample->data, &ch->sample->dataLength);
			gf_bs_del(bs);
			break;
		default:
			break;
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
