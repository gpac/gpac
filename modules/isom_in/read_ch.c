/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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


static void init_reader(ISOMChannel *ch)
{
	u32 ivar;

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
		if (ch->duration<ch->start) {
			ch->last_state = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->duration, &ivar, GF_ISOM_SEARCH_SYNC_BACKWARD, &ch->sample, &ch->sample_num);
		} else {
			ch->last_state = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->start, &ivar, GF_ISOM_SEARCH_SYNC_BACKWARD, &ch->sample, &ch->sample_num);
		}
	
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
			ch->last_state = GF_EOS;
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
			/*we jumped to another segment - if RAP is needed look for closest rap in decoding order and
			force seek mode*/
			if (ch->sample && !ch->sample->IsRAP && ch->has_rap && (ch->sample_num != prev_sample+1)) {
				gf_isom_sample_del(&ch->sample);
				e = gf_isom_get_sample_for_movie_time(ch->owner->mov, ch->track, ch->sample_time + 1, &ivar, GF_ISOM_SEARCH_SYNC_BACKWARD, &ch->sample, &ch->sample_num);
				ch->edit_sync_frame = ch->sample_num;
				ch->sample->DTS = ch->sample_time;
				ch->sample->CTS_Offset = 0;
			} else {
				if (ch->sample) ch->sample_time = ch->sample->DTS;
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
		} else if (!ch->sample_num || (ch->sample_num > gf_isom_get_sample_count(ch->owner->mov, ch->track))) {
			ch->last_state = GF_EOS;
		}
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
			free(ch->sample->data);
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



