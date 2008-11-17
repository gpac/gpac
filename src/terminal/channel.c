/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / Media terminal sub-project
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


#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/sync_layer.h>
#include <gpac/constants.h>

#include "media_memory.h"
#include "media_control.h"

static void ch_buffer_off(GF_Channel *ch)
{
	/*just in case*/
	if (ch->BufferOn) {
		ch->BufferOn = 0;
		gf_clock_buffer_off(ch->clock);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d: buffering off at %d (nb buffering on clock: %d)\n", ch->esd->ESID, gf_term_get_time(ch->odm->term), ch->clock->Buffering));
	}
}


static void ch_buffer_on(GF_Channel *ch)
{
	/*don't buffer on an already running clock*/
	if (ch->clock->no_time_ctrl && ch->clock->clock_init && (ch->esd->ESID!=ch->clock->clockID) ) return;
	if (ch->dispatch_after_db) return;

	/*just in case*/
	if (!ch->BufferOn) {
		ch->BufferOn = 1;
		gf_clock_buffer_on(ch->clock);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d: buffering on at %d (nb buffering on clock: %d)\n", ch->esd->ESID, gf_term_get_time(ch->odm->term), ch->clock->Buffering));
	}
}

/*reset channel*/
static void Channel_Reset(GF_Channel *ch, Bool for_start)
{
	gf_es_lock(ch, 1);

	ch->IsClockInit = 0;
	ch->au_sn = 0;
	ch->pck_sn = 0;
	ch->stream_state = 1;
	ch->IsRap = 0;
	ch->IsEndOfStream = 0;
	ch->skip_carousel_au = 0;

	if (for_start) {
		gf_es_lock(ch, 0);
		return;
	}

	ch->ts_offset = 0;
	ch->seed_ts = 0;

	ch_buffer_off(ch);

	if (ch->buffer) free(ch->buffer);
	ch->buffer = NULL;
	ch->len = ch->allocSize = 0;

	gf_db_unit_del(ch->AU_buffer_first);
	ch->AU_buffer_first = ch->AU_buffer_last = NULL;
	ch->AU_Count = 0;
	ch->BufferTime = 0;
	ch->NextIsAUStart = 1;

	ch->first_au_fetched = 0;
	if (ch->AU_buffer_pull) {
		ch->AU_buffer_pull->data = NULL;
		gf_db_unit_del(ch->AU_buffer_pull);
		ch->AU_buffer_pull = NULL;
	}
	gf_es_lock(ch, 0);
}

GF_Channel *gf_es_new(GF_ESD *esd)
{
	u32 nbBits;
	GF_Channel *tmp;
	GF_SAFEALLOC(tmp, GF_Channel);
	if (!tmp) return NULL;

	tmp->mx = gf_mx_new("Channel");
	tmp->esd = esd;
	tmp->chan_id = (u32) tmp;
	tmp->es_state = GF_ESM_ES_SETUP;

	nbBits = sizeof(u32) * 8 - esd->slConfig->AUSeqNumLength;
	tmp->max_au_sn = 0xFFFFFFFF >> nbBits;
	nbBits = sizeof(u32) * 8 - esd->slConfig->packetSeqNumLength;
	tmp->max_pck_sn = 0xFFFFFFFF >> nbBits;

	tmp->skip_sl = (esd->slConfig->predefined == SLPredef_SkipSL) ? 1 : 0;

	/*take care of dummy streams*/
	if (!esd->slConfig->timestampResolution) esd->slConfig->timestampResolution = esd->slConfig->timeScale ? esd->slConfig->timeScale : 1000;
	if (!esd->slConfig->OCRResolution) esd->slConfig->OCRResolution = esd->slConfig->timestampResolution;

	tmp->ts_res = esd->slConfig->timestampResolution;

	tmp->ocr_scale = 0;
	if (esd->slConfig->OCRResolution) {
		tmp->ocr_scale = 1000;
		tmp->ocr_scale /= esd->slConfig->OCRResolution;
	}


	Channel_Reset(tmp, 0);
	return tmp;
}


/*reconfig SL settings for this channel - this is needed by some net services*/
void gf_es_reconfig_sl(GF_Channel *ch, GF_SLConfig *slc)
{
	u32 nbBits;
	
	memcpy(ch->esd->slConfig, slc, sizeof(GF_SLConfig));

	nbBits = sizeof(u32) * 8 - ch->esd->slConfig->AUSeqNumLength;
	ch->max_au_sn = 0xFFFFFFFF >> nbBits;
	nbBits = sizeof(u32) * 8 - ch->esd->slConfig->packetSeqNumLength;
	ch->max_pck_sn = 0xFFFFFFFF >> nbBits;

	ch->skip_sl = (slc->predefined == SLPredef_SkipSL) ? 1 : 0;

	/*take care of dummy streams*/
	if (!ch->esd->slConfig->timestampResolution) ch->esd->slConfig->timestampResolution = 1000;
	if (!ch->esd->slConfig->OCRResolution) ch->esd->slConfig->OCRResolution = ch->esd->slConfig->timestampResolution;

	ch->ts_res = ch->esd->slConfig->timestampResolution;
	ch->ocr_scale = 0;
	if (ch->esd->slConfig->OCRResolution) {
		ch->ocr_scale = 1000;
		ch->ocr_scale /= ch->esd->slConfig->OCRResolution;
	}
}


/*destroy channel*/
void gf_es_del(GF_Channel *ch)
{
	Channel_Reset(ch, 0);
	if (ch->AU_buffer_pull) {
		ch->AU_buffer_pull->data = NULL;
		gf_db_unit_del(ch->AU_buffer_pull);
	}
	if (ch->ipmp_tool)
		gf_modules_close_interface((GF_BaseInterface *) ch->ipmp_tool);

	if (ch->mx) gf_mx_del(ch->mx);
	free(ch);
}

Bool gf_es_owns_clock(GF_Channel *ch)
{
	/*if the clock is not in the same namespace (used with dynamic scenes), it's not ours*/
	if (gf_list_find(ch->odm->net_service->Clocks, ch->clock)<0) return 0;
	return (ch->esd->ESID==ch->clock->clockID) ? 1 : 0;
}

GF_Err gf_es_start(GF_Channel *ch)
{
	if (!ch) return GF_BAD_PARAM;

	switch (ch->es_state) {
	case GF_ESM_ES_UNAVAILABLE:
	case GF_ESM_ES_SETUP:
		return GF_BAD_PARAM;
	/*if the channel is already running, don't reset its settings. This only happens in the case of broadcast 
	objects started several times by the scene but not stoped at the ODManager level (cf gf_odm_stop)*/
	case GF_ESM_ES_RUNNING:
		return GF_OK;
	default:
		break;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] Starting ES %d\n", ch->esd->ESID));

	/*reset clock if we own it*/
	if (gf_es_owns_clock(ch) && !ch->clock->no_time_ctrl) 
		gf_clock_reset(ch->clock);

	/*reset channel*/
	Channel_Reset(ch, 1);
	/*create pull buffer if needed*/
	if (ch->is_pulling && !ch->AU_buffer_pull) ch->AU_buffer_pull = gf_db_unit_new();

	/*and start buffering - pull channels always turn off buffering immediately, otherwise 
	buffering size is setup by the network service - except InputSensor*/
	if ((ch->esd->decoderConfig->streamType != GF_STREAM_INTERACT) || ch->esd->URLString) {
		ch_buffer_on(ch);
	}
	ch->last_au_time = gf_term_get_time(ch->odm->term);
	ch->es_state = GF_ESM_ES_RUNNING;
	return GF_OK;
}

GF_Err gf_es_stop(GF_Channel *ch)
{
	if (!ch) return GF_BAD_PARAM;

	switch (ch->es_state) {
	case GF_ESM_ES_UNAVAILABLE:
	case GF_ESM_ES_SETUP:
		return GF_BAD_PARAM;
	default:
		break;
	}

	ch_buffer_off(ch);

	ch->es_state = GF_ESM_ES_CONNECTED;
	Channel_Reset(ch, 0);
	return GF_OK;
}


void Channel_WaitRAP(GF_Channel *ch)
{
	ch->pck_sn = 0;

	/*if using RAP signal and codec not resilient, wait for rap. If RAP isn't signaled DON'T wait for it :)*/
	if (!ch->codec_resilient) 
		ch->stream_state = 2;
	if (ch->buffer) free(ch->buffer);
	ch->buffer = NULL;
	ch->AULength = 0;
	ch->au_sn = 0;
}

void gf_es_map_time(GF_Channel *ch, Bool reset)
{
	gf_mx_p(ch->mx);
	if (ch->buffer) free(ch->buffer);
	ch->buffer = NULL;
	ch->len = ch->allocSize = 0;

	if (reset) {
		gf_db_unit_del(ch->AU_buffer_first);
		ch->AU_buffer_first = ch->AU_buffer_last = NULL;
		ch->AU_Count = 0;
	} else {
		GF_DBUnit *au = ch->AU_buffer_first;
		while (au) {
			au->DTS = au->CTS = ch->ts_offset;
			au = au->next;
		}
	}
	ch->BufferTime = 0;
	gf_mx_v(ch->mx);
}

static Bool Channel_NeedsBuffering(GF_Channel *ch, u32 ForRebuffering)
{
	if (!ch->MaxBuffer || ch->IsEndOfStream) return 0;

	/*for rebuffering, check we're not below min buffer*/
	if (ForRebuffering) {
		if (ch->MinBuffer && (ch->BufferTime <= (s32) ch->MinBuffer)) {
			return 1;
		}
		return 0;
	}
	/*we're in a broadcast scenario and one of the stream running on this clock has completed its buffering: 
	abort all buffering*/
	if (ch->clock->no_time_ctrl == 2) return 0;

	/*nothing received, buffer needed*/
	if (!ch->first_au_fetched && !ch->AU_buffer_first) {
		u32 now = gf_term_get_time(ch->odm->term);
		/*data timeout (no data sent)*/
		if (now > ch->last_au_time + ch->odm->term->net_data_timeout) {
			gf_term_message(ch->odm->term, ch->service->url, "Data timeout - aborting buffering", GF_OK); 
			ch->MinBuffer = ch->MaxBuffer = 0;
			ch->au_duration = 0;
			gf_inline_buffering_info(ch->odm->parentscene ? ch->odm->parentscene : ch->odm->subscene);
			return 0;
		} else {
			now = ch->odm->term->net_data_timeout + ch->last_au_time - now;
			now /= 1000;
			if (now != ch->au_duration) {
				char szMsg[500];
				ch->au_duration = now;
				sprintf(szMsg, "Buffering - Waiting for data (%d s)", now);
				gf_term_message(ch->odm->term, ch->service->url, szMsg, GF_OK); 
			}
			return 1;
		}
	}

	/*buffer not filled yet*/
	if (ch->BufferTime < (s32) ch->MaxBuffer) {
		/*check last AU time*/
		u32 now = gf_term_get_time(ch->odm->term);
		/*if more than half sec since last AU don't buffer and prevent rebuffering on short streams
		this will also work for channels ignoring timing*/
		if (now>ch->last_au_time + MAX(ch->BufferTime, 500) ) {
			/*this can be safely seen as a stream with very few updates (likely only one)*/
			if (!ch->AU_buffer_first && ch->first_au_fetched) ch->MinBuffer = 0;
			return 0;
		}
		return 1;
	}
	return 0;
}

static void Channel_UpdateBuffering(GF_Channel *ch, Bool update_info)
{
	if (update_info && ch->MaxBuffer) gf_inline_buffering_info(ch->odm->parentscene ? ch->odm->parentscene : ch->odm->subscene);

	gf_term_service_media_event(ch->odm, GF_EVENT_MEDIA_DATA_PROGRESS);
	
	if (!Channel_NeedsBuffering(ch, 0)) {
		ch_buffer_off(ch);
		if (ch->MaxBuffer && update_info) gf_inline_buffering_info(ch->odm->parentscene ? ch->odm->parentscene : ch->odm->subscene);
		if (ch->clock->no_time_ctrl) ch->clock->no_time_ctrl = 2;

		gf_term_service_media_event(ch->odm, GF_EVENT_MEDIA_PLAYABLE);
	}
}

static void Channel_UpdateBufferTime(GF_Channel *ch)
{
	if (!ch->AU_buffer_first) {
		ch->BufferTime = 0;
	}
	else if (ch->skip_sl) {
		GF_DBUnit *au;
		/*compute buffer size from avg bitrate*/
		u32 avg_rate = ch->esd->decoderConfig->avgBitrate;
		if (!avg_rate && ch->odm->codec) avg_rate = ch->odm->codec->avg_bit_rate;
		if (avg_rate) {
			u32 bsize = 0;
			au = ch->AU_buffer_first;
			while (1) {
				bsize += au->dataLength*8;
				if (!au->next) break;
				au = au->next;
			}
			ch->BufferTime = 1000*bsize/avg_rate;
		} else {
			/*we're in the dark, so don't buffer too much (assume 50ms per unit) so that we start decoding asap*/
			ch->BufferTime = 50*ch->AU_Count;
		}
	} else {
		s32 bt = ch->AU_buffer_last->DTS - gf_clock_time(ch->clock);
		ch->BufferTime = 0;
		if (bt>0) ch->BufferTime = (u32) bt;
	}
	ch->BufferTime += ch->au_duration;
}

/*dispatch the AU in the DB*/
static void Channel_DispatchAU(GF_Channel *ch, u32 duration)
{
	u32 time;
	GF_DBUnit *au;

	if (!ch->buffer || !ch->len) {
		if (ch->buffer) {
			free(ch->buffer);
			ch->buffer = NULL;
		}
		return;
	}

	au = gf_db_unit_new();
	if (!au) {
		free(ch->buffer);
		ch->buffer = NULL;
		ch->len = 0;
		return;
	}

	au->CTS = ch->CTS;
	au->DTS = ch->DTS;
	au->RAP = ch->IsRap;
	au->data = ch->buffer;
	au->dataLength = ch->len;
	au->PaddingBits = ch->padingBits;

	ch->IsRap = 0;
	ch->padingBits = 0;
	au->next = NULL;
	ch->buffer = NULL;

	if (ch->len + ch->media_padding_bytes != ch->allocSize) {
		au->data = (char*)realloc(au->data, sizeof(char) * (au->dataLength + ch->media_padding_bytes));
	}
	if (ch->media_padding_bytes) memset(au->data + au->dataLength, 0, sizeof(char)*ch->media_padding_bytes);
	
	ch->len = ch->allocSize = 0;

	gf_es_lock(ch, 1);

	if (ch->service && ch->service->cache) {
		GF_SLHeader slh;
		memset(&slh, 0, sizeof(GF_SLHeader));
		slh.accessUnitEndFlag = slh.accessUnitStartFlag = 1;
		slh.compositionTimeStampFlag = slh.decodingTimeStampFlag = 1;
		slh.decodingTimeStamp = ch->net_dts;
		slh.compositionTimeStamp = ch->net_cts;
		slh.randomAccessPointFlag = au->RAP;
		ch->service->cache->Write(ch->service->cache, ch, au->data, au->dataLength, &slh);
	}

	if (!ch->AU_buffer_first) {
		ch->AU_buffer_first = au;
		ch->AU_buffer_last = au;
		ch->AU_Count = 1;
	} else {
		if (ch->AU_buffer_last->DTS<=au->DTS) {
			ch->AU_buffer_last->next = au;
			ch->AU_buffer_last = ch->AU_buffer_last->next;
		}
		/*enable deinterleaving only for audio channels (some video transport may not be able to compute DTS, cf MPEG1-2/RTP)
		HOWEVER, we must recompute a monotone increasing DTS in case the decoder does perform frame reordering
		in which case the DTS is used for presentation time!!*/
		else if (ch->odm->codec && (ch->odm->codec->type!=GF_STREAM_AUDIO)) {
			GF_DBUnit *au_prev, *ins_au;
			u32 DTS;

			/*append AU*/
			ch->AU_buffer_last->next = au;
			ch->AU_buffer_last = ch->AU_buffer_last->next;

			DTS = au->DTS;
			au_prev = ch->AU_buffer_first;
			/*locate first AU in buffer with DTS greater than new unit CTS*/
			while (au_prev->next && (au_prev->DTS < DTS) ) au_prev = au_prev->next;
			/*remember insertion point*/
			ins_au = au_prev;
			/*shift all following frames DTS*/
			while (au_prev->next) {
				au_prev->next->DTS = au_prev->DTS;
				au_prev = au_prev->next;
			}
			/*and apply*/
			ins_au->DTS = DTS;
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] Audio deinterleaving OD %d ch %d\n", ch->esd->ESID, ch->odm->OD->objectDescriptorID));
			/*de-interleaving of AUs*/
			if (ch->AU_buffer_first->DTS > au->DTS) {
				au->next = ch->AU_buffer_first;
				ch->AU_buffer_first = au;
			} else {
				GF_DBUnit *au_prev = ch->AU_buffer_first;
				while (au_prev->next && au_prev->next->DTS<au->DTS) {
					au_prev = au_prev->next;
				}
				assert(au_prev);
				if (au_prev->next->DTS==au->DTS) {
					free(au->data);
					free(au);
				} else {
					au->next = au_prev->next;
					au_prev->next = au;
				}
			}
		}
		ch->AU_Count += 1;
	}

	Channel_UpdateBufferTime(ch);
	ch->au_duration = 0;
	if (duration) ch->au_duration = (u32) ((u64)1000 * duration / ch->ts_res);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d - Dispatch AU CTS %d time %d Buffer %d Nb AUs %d\n", ch->esd->ESID, au->CTS, gf_clock_time(ch->clock), ch->BufferTime, ch->AU_Count));

	/*little optimisation: if direct dispatching is possible, try to decode the AU
	we must lock the media scheduler to avoid deadlocks with other codecs accessing the scene or 
	media resources*/
	if (ch->dispatch_after_db) {
		u32 retry = 100;
		u32 current_frame;
		GF_Terminal *term = ch->odm->term;
		ch_buffer_off(ch);
		if (gf_mx_try_lock(term->mm_mx)) {
			switch (ch->esd->decoderConfig->streamType) {
			case GF_STREAM_OD:
				gf_codec_process(ch->odm->subscene->od_codec, 100);
				break;
			case GF_STREAM_SCENE:
				if (ch->odm->codec) 
					gf_codec_process(ch->odm->codec, 100);
				else
					gf_codec_process(ch->odm->subscene->scene_codec, 100);
				break;
			}
			gf_mx_v(term->mm_mx);
		}

		current_frame = term->compositor->frame_number;
		/*wait for initial setup to complete before giving back the hand to the caller service*/
		while (retry) {
			/*Scene bootstrap: if the scene is attached, wait for first frame to complete so that initial PLAY on
			objects can be evaluated*/
			if (term->compositor->scene && (term->compositor->frame_number==current_frame) ) {
				retry--;
				gf_sleep(1);
				continue;
			}
			/*Media bootstrap: wait for all pending requests on media objects are processed*/
			if (gf_list_count(term->media_queue)) {
				retry--;
				gf_sleep(1);
				continue;
			}
			break;
		}
	}

	time = gf_term_get_time(ch->odm->term);
	if (ch->BufferOn) {
		ch->last_au_time = time;
		Channel_UpdateBuffering(ch, 1);
	} else {
		/*trigger the data progress every 500 ms*/
		if (ch->last_au_time + 500 > time) {
			gf_term_service_media_event(ch->odm, GF_EVENT_MEDIA_DATA_PROGRESS);
			ch->last_au_time = time;
		}
	}

	gf_es_lock(ch, 0);
}

void Channel_ReceiveSkipSL(GF_ClientService *serv, GF_Channel *ch, char *StreamBuf, u32 StreamLength)
{
	GF_DBUnit *au;
	if (!StreamLength) return;

	gf_es_lock(ch, 1);
	au = gf_db_unit_new();
	au->RAP = 1;
	au->DTS = gf_clock_time(ch->clock);
	au->data = (char*)malloc(sizeof(char) * (ch->media_padding_bytes + StreamLength));
	memcpy(au->data, StreamBuf, sizeof(char) * StreamLength);
	if (ch->media_padding_bytes) memset(au->data + StreamLength, 0, sizeof(char)*ch->media_padding_bytes);
	au->dataLength = StreamLength;
	au->next = NULL;

	/*if channel owns the clock, start it*/
	if (ch->clock && !ch->IsClockInit) {
		if (gf_es_owns_clock(ch)) {
			gf_clock_set_time(ch->clock, 0);
			ch->IsClockInit = 1;
			ch->seed_ts = 0;
		}
		if (ch->clock->clock_init && !ch->IsClockInit) {
			ch->IsClockInit = 1;
			ch->seed_ts = gf_clock_time(ch->clock);
		}
	}

	if (!ch->AU_buffer_first) {
		ch->AU_buffer_first = au;
		ch->AU_buffer_last = au;
		ch->AU_Count = 1;
	} else {
		ch->AU_buffer_last->next = au;
		ch->AU_buffer_last = ch->AU_buffer_last->next;
		ch->AU_Count += 1;
	}

	Channel_UpdateBufferTime(ch);

	if (ch->BufferOn) {
		ch->last_au_time = gf_term_get_time(ch->odm->term);
		Channel_UpdateBuffering(ch, 1);
	}
	gf_es_lock(ch, 0);
}


/*handles reception of an SL-PDU, logical or physical*/
void gf_es_receive_sl_packet(GF_ClientService *serv, GF_Channel *ch, char *StreamBuf, u32 StreamLength, GF_SLHeader *header, GF_Err reception_status)
{
	GF_SLHeader hdr;
	u32 nbAU, OldLength, size, AUSeqNum, SLHdrLen;
	Bool EndAU, NewAU;
	char *payload;

	if (ch->bypass_sl_and_db) {
		GF_SceneDecoder *sdec;
		ch->IsClockInit = 1;
		if (ch->odm->subscene) {
			sdec = (GF_SceneDecoder *)ch->odm->subscene->scene_codec->decio;
		} else {
			sdec = (GF_SceneDecoder *)ch->odm->codec->decio;
		}
		gf_mx_p(ch->mx);
		sdec->ProcessData(sdec, StreamBuf, StreamLength, ch->esd->ESID, 0, 0);
		gf_mx_v(ch->mx);
		return;
	}

	if (ch->es_state != GF_ESM_ES_RUNNING) return;

	/*physical SL-PDU - depacketize*/
	if (!header) {
		if (!StreamLength) return;
		gf_sl_depacketize(ch->esd->slConfig, &hdr, StreamBuf, StreamLength, &SLHdrLen);
		StreamLength -= SLHdrLen;
	} else {
		hdr = *header;
		SLHdrLen = 0;
	}
	payload = StreamBuf + SLHdrLen;

	if (ch->skip_sl) {
		Channel_ReceiveSkipSL(serv, ch, payload, StreamLength);
		return;
	}

	/*check state*/
	if (!ch->codec_resilient && (reception_status==GF_CORRUPTED_DATA)) {
		Channel_WaitRAP(ch);
		return;
	}

	if (!ch->esd->slConfig->useAccessUnitStartFlag) {
		/*no AU signaling - each packet is an AU*/
		if (!ch->esd->slConfig->useAccessUnitEndFlag) 
			hdr.accessUnitEndFlag = hdr.accessUnitStartFlag = 1;
		/*otherwise AU are signaled by end of previous packet*/
		else
			hdr.accessUnitStartFlag = ch->NextIsAUStart;
	}

	/*get RAP*/
	if (ch->esd->slConfig->hasRandomAccessUnitsOnlyFlag) {
		hdr.randomAccessPointFlag = 1;
	} else if (!ch->esd->slConfig->useRandomAccessPointFlag || ch->codec_resilient) {
		ch->stream_state = 0;
	}

	if (ch->esd->slConfig->packetSeqNumLength) {
		if (ch->pck_sn && hdr.packetSequenceNumber) {
			/*repeated -> drop*/
			if (ch->pck_sn == hdr.packetSequenceNumber) {
				GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: repeated packet, droping\n", ch->esd->ESID));
				return;
			}
			/*if codec has no resiliency check packet drops*/
			if (!ch->codec_resilient && !hdr.accessUnitStartFlag) {
				if (ch->pck_sn == (u32) (1<<ch->esd->slConfig->packetSeqNumLength) ) {
					if (hdr.packetSequenceNumber) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_SYNC, ("[SyncLayer] ES%d: packet loss, droping & wait RAP\n", ch->esd->ESID));
						Channel_WaitRAP(ch);
						return;
					}
				} else if (ch->pck_sn + 1 != hdr.packetSequenceNumber) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_SYNC, ("[SyncLayer] ES%d: packet loss, droping & wait RAP\n", ch->esd->ESID));
					Channel_WaitRAP(ch);
					return;
				}
			}
		}
		ch->pck_sn = hdr.packetSequenceNumber;
	}

	/*if empty, skip the packet*/
	if (hdr.paddingFlag && !hdr.paddingBits) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d: Empty packet - skipping\n", ch->esd->ESID));
		return;
	}
	/*IDLE stream shall be processed*/


	NewAU = 0;
	if (hdr.accessUnitStartFlag) {
		NewAU = 1;
		ch->NextIsAUStart = 0;
		ch->skip_carousel_au = 0;

		/*if we have a pending AU, add it*/
		if (ch->buffer) {
			if (ch->esd->slConfig->useAccessUnitEndFlag) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_SYNC, ("[SyncLayer] ES%d: missed end of AU (DTS %d)\n", ch->esd->ESID, ch->DTS));
			}
			if (ch->codec_resilient) {
				Channel_DispatchAU(ch, 0);
			} else {
				free(ch->buffer);
				ch->buffer = NULL;
				ch->AULength = 0;
				ch->len = ch->allocSize = 0;
			}
		}
		AUSeqNum = hdr.AU_sequenceNumber;
		/*Get CTS */
		if (hdr.compositionTimeStampFlag) {
			ch->net_dts = ch->net_cts = hdr.compositionTimeStamp;
			/*get DTS */
			if (hdr.decodingTimeStampFlag) ch->net_dts = hdr.decodingTimeStamp;

			/*until clock is not init check seed ts*/
			if (!ch->IsClockInit && (ch->net_dts < ch->seed_ts)) 
				ch->seed_ts = ch->net_dts;

			ch->net_dts -= ch->seed_ts;
			ch->net_cts -= ch->seed_ts;
			/*TS Wraping not tested*/
			ch->CTS = (u32) (ch->ts_offset + (s64) (ch->net_cts) * 1000 / ch->ts_res);
			ch->DTS = (u32) (ch->ts_offset + (s64) (ch->net_dts) * 1000 / ch->ts_res);
		} else {
			/*use CU duration*/
			if (!ch->IsClockInit) ch->DTS = ch->CTS = ch->ts_offset;

			if (!ch->esd->slConfig->AUSeqNumLength) {
				if (!ch->au_sn) {
					ch->CTS = ch->ts_offset;
					ch->au_sn = 1;
				} else {
					ch->CTS += ch->esd->slConfig->CUDuration;
				}
			} else {
				//use the sequence number to get the TS
				if (AUSeqNum < ch->au_sn) {
					nbAU = ( (1<<ch->esd->slConfig->AUSeqNumLength) - ch->au_sn) + AUSeqNum;
				} else {
					nbAU = AUSeqNum - ch->au_sn;
				}
				ch->CTS += nbAU * ch->esd->slConfig->CUDuration;
			}
		}

		if (!ch->IsClockInit && (hdr.compositionTimeStampFlag || !ch->esd->slConfig->useTimestampsFlag) ) {
			/*the first data received inits the clock - this is needed to handle clock dependencies on non-initialized 
			streams (eg, bifs/od depends on audio/video clock)*/
			if (!ch->clock->clock_init) {
				gf_clock_set_time(ch->clock, ch->CTS);
				GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: Forcing clock initialization at STB %d - AU DTS %d\n", ch->esd->ESID, gf_term_get_time(ch->odm->term), ch->DTS));
			} 
			/*channel is the OCR, force a re-init of the clock since we cannot assume the AU used to init the clock was
			not sent ahead of time*/
			else if (gf_es_owns_clock(ch)) {
				ch->clock->clock_init = 0;
				gf_clock_set_time(ch->clock, ch->DTS);
				GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: initializing clock at STB %d - AU DTS %d - %d buffering\n", ch->esd->ESID, gf_term_get_time(ch->odm->term), ch->DTS, ch->clock->Buffering));
			}
			if (ch->clock->clock_init) ch->IsClockInit = 1;
		}
		/*if the AU Length is carried in SL, get its size*/
		if (ch->esd->slConfig->AULength > 0) {
			ch->AULength = hdr.accessUnitLength;
		} else {
			ch->AULength = 0;
		}
		/*carousel for repeated AUs.*/
		if (ch->esd->slConfig->AUSeqNumLength) {
			if (hdr.randomAccessPointFlag) {
				/*initial tune-in*/
				if (ch->stream_state==1) {
					GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: RAP Carousel found - tuning in\n", ch->esd->ESID));
					ch->au_sn = AUSeqNum;
					ch->stream_state = 0;
				}
				/*carousel RAP*/
				else if (AUSeqNum == ch->au_sn) {
					/*error recovery*/
					if (ch->stream_state==2) {
						GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: RAP Carousel found - recovering\n", ch->esd->ESID));
						ch->stream_state = 0;
					} 
					else {
						ch->skip_carousel_au = 1;
						GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: RAP Carousel found - skipping\n", ch->esd->ESID));
						return;
					}
				}
				/*regular RAP*/
				else {
					ch->au_sn = AUSeqNum;
					ch->stream_state = 0;
				}
			} 
			/*regular AU but waiting for RAP*/
			else if (ch->stream_state) {
				ch->skip_carousel_au = 1;
				GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: Waiting for RAP Carousel - skipping\n", ch->esd->ESID));
				return;
			} else {
				ch->au_sn = AUSeqNum;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d: NON-RAP AU received\n", ch->esd->ESID));
			}
		}
		/*no carousel signaling, tune-in at first RAP*/
		else if (hdr.randomAccessPointFlag) {
			ch->stream_state = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d: RAP AU received\n", ch->esd->ESID));
		}
		/*waiting for RAP, return*/
		else if (ch->stream_state) {
			GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: Waiting for RAP - skipping AU (DTS %d)\n", ch->esd->ESID, ch->DTS));
			return;
		}
	}

	/*update the RAP marker on a packet base (to cope with AVC/H264 NALU->AU reconstruction)*/
	if (hdr.randomAccessPointFlag) ch->IsRap = 1;
	/* we need to skip all the packets of the current AU in the carousel scenario */
	if (ch->skip_carousel_au == 1) return;

	/*we ignore OCRs for the moment*/
#if 0
	/*Apply OCR*/
	if (!ch->BufferOn && ch->IsClockInit && hdr.OCRflag) {
		u32 OCR_TS = (u32) (s64) (((s64) hdr.objectClockReference) * ch->ocr_scale);
		u32 ck = gf_clock_time(ch->clock);
		OCR_TS += ch->BufferTime;
		fprintf(stdout, "OCR %d OTB %d Diff %d Buffer %d (%d AUs) \n", OCR_TS, ck, OCR_TS - ck, ch->BufferTime, ch->AU_Count);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d: At OTB %d adjusting OCR to %d\n", ch->esd->ESID, gf_clock_real_time(ch->clock), OCR_TS));
		gf_clock_set_time(ch->clock, OCR_TS);
	}
#endif

	/*get AU end state*/	
	OldLength = ch->buffer ? ch->len : 0;
	EndAU = hdr.accessUnitEndFlag;
	if (ch->AULength == OldLength + StreamLength) EndAU = 1;
	if (EndAU) ch->NextIsAUStart = 1;

	if (!StreamLength && EndAU && ch->buffer) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d: Empty packet, flushing buffer\n", ch->esd->ESID));
		Channel_DispatchAU(ch, 0);
		return;
	}
	if (!StreamLength) return;

	/*missed begining, unusable*/
	if (!ch->buffer && !NewAU) {
		if (ch->esd->slConfig->useAccessUnitStartFlag) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SYNC, ("[SyncLayer] ES%d: missed begin of AU\n", ch->esd->ESID));
		}
		if (ch->codec_resilient) NewAU = 1;
		else return;
	}

	/*Write the Packet payload to the buffer*/
	if (NewAU) {
		/*we should NEVER have a bitstream at this stage*/
		assert(!ch->buffer);
		/*ignore length fields*/
		size = StreamLength + ch->media_padding_bytes;
		ch->buffer = (char*)malloc(sizeof(char) * size);
		if (!ch->buffer) {
			assert(0);
			return;
		}

		ch->allocSize = size;
		memset(ch->buffer, 0, sizeof(char) * size);
		ch->len = 0;
	}
	if (!ch->esd->slConfig->usePaddingFlag) hdr.paddingFlag = 0;
	
	if (ch->ipmp_tool) {
		GF_Err e;
		GF_IPMPEvent evt;
		memset(&evt, 0, sizeof(evt));
		evt.event_type=GF_IPMP_TOOL_PROCESS_DATA;
		evt.channel = ch;
		evt.data = payload;
		evt.data_size = StreamLength;
		evt.is_encrypted = hdr.isma_encrypted;
		evt.isma_BSO = hdr.isma_BSO;
		e = ch->ipmp_tool->process(ch->ipmp_tool, &evt);

		/*we discard undecrypted AU*/
		if (e) {
			if (e==GF_EOS) {
				gf_es_on_eos(ch);
				/*restart*/
				if (evt.restart_requested) {
					if (ch->odm->parentscene->is_dynamic_scene) {
						gf_inline_restart_dynamic(ch->odm->parentscene, 0);
					} else {
						MC_Restart(ch->odm);
					}
				}
			}
			return;
		}
	}

	if (hdr.paddingFlag && !EndAU) {	
		/*to do - this shouldn't happen anyway */

	} else {
		/*check if enough space*/
		size = ch->allocSize;
		if (size && (StreamLength + ch->len <= size)) {
			memcpy(ch->buffer+ch->len, payload, StreamLength);
			ch->len += StreamLength;
		} else {
			size = StreamLength + ch->len + ch->media_padding_bytes;
			ch->buffer = (char*)realloc(ch->buffer, sizeof(char) * size);
			memcpy(ch->buffer+ch->len, payload, StreamLength);
			ch->allocSize = size;
			ch->len += StreamLength;
		}
		if (hdr.paddingFlag) ch->padingBits = hdr.paddingBits;
	}

	if (EndAU) Channel_DispatchAU(ch, hdr.au_duration);
}


/*notification of End of stream on this channel*/
void gf_es_on_eos(GF_Channel *ch)
{
	if (!ch || ch->IsEndOfStream) return;
	ch->IsEndOfStream = 1;
	
	/*flush buffer*/
	ch_buffer_off(ch);

	ch->clock->has_seen_eos = 1;
	gf_odm_on_eos(ch->odm, ch);
}



GF_DBUnit *gf_es_get_au(GF_Channel *ch)
{
	Bool comp, is_new_data;
	GF_Err e, state;
	GF_SLHeader slh;

	if (ch->es_state != GF_ESM_ES_RUNNING) return NULL;

	if (!ch->is_pulling) {
		/*we must update buffering before fetching in order to stop buffering for streams with very few
		updates (especially streams with one update, like most of OD streams)*/
		if (ch->BufferOn) Channel_UpdateBuffering(ch, 0);
		if (ch->first_au_fetched && ch->BufferOn) return NULL;
		return ch->AU_buffer_first;
	}

	/*pull from stream - resume clock if needed*/
	ch_buffer_off(ch);

	e = gf_term_channel_get_sl_packet(ch->service, ch, (char **) &ch->AU_buffer_pull->data, &ch->AU_buffer_pull->dataLength, &slh, &comp, &state, &is_new_data);
	if (e) state = e;
	switch (state) {
	case GF_EOS:
		gf_es_on_eos(ch);
		return NULL;
	case GF_OK:
		break;
	default:
		gf_term_message(ch->odm->term, ch->service->url , "Data reception failure", state);
		return NULL;
	}
	assert(!comp);
	/*update timing if new stream data but send no data*/
	if (is_new_data) {
		gf_es_receive_sl_packet(ch->service, ch, NULL, 0, &slh, GF_OK);
		if (ch->ipmp_tool) {
			GF_IPMPEvent evt;
			memset(&evt, 0, sizeof(evt));
			evt.event_type=GF_IPMP_TOOL_PROCESS_DATA;
			evt.data = ch->AU_buffer_pull->data;
			evt.data_size = ch->AU_buffer_pull->dataLength;
			evt.is_encrypted = slh.isma_encrypted;
			evt.isma_BSO = slh.isma_BSO;
			evt.channel = ch;
			e = ch->ipmp_tool->process(ch->ipmp_tool, &evt);

			/*we discard undecrypted AU*/
			if (e) {
				if (e==GF_EOS) {
					gf_es_on_eos(ch);
					/*restart*/
					if (evt.restart_requested) {
						if (ch->odm->parentscene->is_dynamic_scene) {
							gf_inline_restart_dynamic(ch->odm->parentscene, 0);
						} else {
							MC_Restart(ch->odm);
						}
					}
				}
				gf_term_channel_release_sl_packet(ch->service, ch);
				return NULL;
			}
		}
	}

	/*this may happen in file streaming when data has not arrived yet, in which case we discard the AU*/
	if (!ch->AU_buffer_pull->data) {
		gf_term_channel_release_sl_packet(ch->service, ch);
		return NULL;
	}
	ch->AU_buffer_pull->CTS = (u32) ch->CTS;
	ch->AU_buffer_pull->DTS = (u32) ch->DTS;
	ch->AU_buffer_pull->PaddingBits = ch->padingBits;
	ch->AU_buffer_pull->RAP = ch->IsRap;
	return ch->AU_buffer_pull;
}

void gf_es_init_dummy(GF_Channel *ch)
{
	GF_SLHeader slh;
	Bool comp, is_new_data;
	GF_Err e, state;
	/*pull from stream - resume clock if needed*/
	ch_buffer_off(ch);

	ch->ts_res = 1000;
	if (ch->is_pulling) {
		e = gf_term_channel_get_sl_packet(ch->service, ch, (char **) &ch->AU_buffer_pull->data, &ch->AU_buffer_pull->dataLength, &slh, &comp, &state, &is_new_data);
		if (e) state = e;
		if ((state==GF_OK) && is_new_data) gf_es_receive_sl_packet(ch->service, ch, NULL, 0, &slh, GF_OK);
		gf_term_channel_release_sl_packet(ch->service, ch);
	} else {
		memset(&slh, 0, sizeof(GF_SLHeader));
		slh.accessUnitStartFlag = 1;
		slh.compositionTimeStampFlag = 1;
		gf_es_receive_sl_packet(ch->service, ch, NULL, 0, &slh, GF_OK);
	}
}

void gf_es_drop_au(GF_Channel *ch)
{
	GF_DBUnit *au;

	if (ch->is_pulling) {
		if (ch->AU_buffer_pull) {
			gf_term_channel_release_sl_packet(ch->service, ch);
			ch->AU_buffer_pull->data = NULL;
			ch->AU_buffer_pull->dataLength = 0;
		}
		ch->first_au_fetched = 1;
		return;
	}

	/*lock the channel before touching the queue*/
	gf_es_lock(ch, 1);
	if (!ch->AU_buffer_first) {
		gf_es_lock(ch, 0);
		return;
	}
	ch->first_au_fetched = 1;

	au = ch->AU_buffer_first;
	ch->AU_buffer_first = au->next;
	au->next = NULL;
	gf_db_unit_del(au);
	ch->AU_Count -= 1;

	if (!ch->AU_Count && ch->AU_buffer_first) {
		ch->AU_buffer_first = NULL;
	}
	if (!ch->AU_buffer_first) ch->AU_buffer_last = NULL;

	Channel_UpdateBufferTime(ch);

	/*if we get under our limit, rebuffer EXCEPT WHEN EOS is signaled*/
	if (!ch->IsEndOfStream && Channel_NeedsBuffering(ch, 1)) {
		ch_buffer_on(ch);
		gf_term_service_media_event(ch->odm, GF_EVENT_MEDIA_NOT_PLAYABLE);
	}

	/*unlock the channel*/
	gf_es_lock(ch, 0);
}

/*(un)locks channel*/
void gf_es_lock(GF_Channel *ch, u32 LockIt)
{
	if (LockIt) {
		gf_mx_p(ch->mx);
	} else {
		gf_mx_v(ch->mx);
	}
}

/*refresh all ODs when an non-interactive stream is found*/
static void refresh_non_interactive_clocks(GF_ObjectManager *odm)
{
	u32 i, j;
	GF_Channel *ch;
	GF_ObjectManager *test_od;
	GF_InlineScene *in_scene;

	/*check for inline*/
	in_scene = odm->subscene ? odm->subscene : odm->parentscene;
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(in_scene->root_od->channels, &i)) ) {
		if (ch->clock->no_time_ctrl) {
			in_scene->root_od->flags |= GF_ODM_NO_TIME_CTRL;
			//if (ch->clock->use_ocr && ch->BufferOn) ch_buffer_off(ch);
		}
	}

	i=0;
	while ((test_od = (GF_ObjectManager *)gf_list_enum(in_scene->ODlist, &i)) ) {
		j=0;
		while ((ch = (GF_Channel*)gf_list_enum(test_od->channels, &j)) ) {
			if (ch->clock->no_time_ctrl) {
				test_od->flags |= GF_ODM_NO_TIME_CTRL;
				//if (ch->clock->use_ocr && ch->BufferOn) ch_buffer_off(ch);
			}
		}
	}
}
/*performs final setup upon connection confirm*/
void gf_es_on_connect(GF_Channel *ch)
{
	Bool can_buffer;
	GF_NetworkCommand com;

	/*check whether we can work in pull mode or not*/
	can_buffer = 1;
	/*if local interaction streams no buffer nor pull*/
	if ((ch->esd->decoderConfig->streamType == GF_STREAM_INTERACT) && !ch->esd->URLString) can_buffer = 0;

	com.base.on_channel = ch;

	ch->is_pulling = 0;
	if (can_buffer) {
		/*request padding*/
		com.command_type = GF_NET_CHAN_SET_PADDING;
		com.pad.padding_bytes = ch->media_padding_bytes;
		if (!com.pad.padding_bytes || (gf_term_service_command(ch->service, &com) == GF_OK)) {
			/*request pull if possible*/
			if (ch->service->ifce->ChannelGetSLP && ch->service->ifce->ChannelReleaseSLP) {
				com.command_type = GF_NET_CHAN_SET_PULL;
				if (gf_term_service_command(ch->service, &com) == GF_OK) {
					ch->is_pulling = 1;
					can_buffer = 0;
				}
			}
		}
	}
	/*checks whether the stream is interactive or not*/
	com.command_type = GF_NET_CHAN_INTERACTIVE;
	if (gf_term_service_command(ch->service, &com)!=GF_OK) {
		ch->clock->no_time_ctrl = 1;
		ch->odm->flags |= GF_ODM_NO_TIME_CTRL;
		refresh_non_interactive_clocks(ch->odm);
	}

	/*signal channel state*/
	if (ch->es_state == GF_ESM_ES_WAIT_FOR_ACK) 
		ch->es_state = GF_ESM_ES_CONNECTED;
	/*signal only once connected to prevent PLAY trigger on connection callback*/
	ch->odm->pending_channels--;

	/*remember channels connected on service*/
	if (ch->esd->URLString) ch->service->nb_ch_users++;

	/*turn off buffering for JPEG and PNG*/
	switch (ch->esd->decoderConfig->objectTypeIndication) {
	case 0x6C:
	case 0x6D:
		can_buffer = 0;
		break;
	}
	/*buffer setup*/
	ch->MinBuffer = ch->MaxBuffer = 0;
	if (can_buffer) {
		const char *sOpt;
		com.command_type = GF_NET_CHAN_BUFFER;
		com.base.on_channel = ch;

		/*set default values*/
		com.buffer.max = 1000;
		sOpt = gf_cfg_get_key(ch->odm->term->user->config, "Network", "BufferLength");
		if (sOpt) com.buffer.max = atoi(sOpt);
		com.buffer.min = 0;
		sOpt = gf_cfg_get_key(ch->odm->term->user->config, "Network", "RebufferLength");
		if (sOpt) com.buffer.min = atoi(sOpt);

		if (gf_term_service_command(ch->service, &com) == GF_OK) {
			ch->MinBuffer = com.buffer.min;
			ch->MaxBuffer = com.buffer.max;
		}
	}
	if (ch->esd->decoderConfig->streamType == GF_STREAM_PRIVATE_SCENE &&
		ch->esd->decoderConfig->objectTypeIndication == GPAC_OTI_PRIVATE_SCENE_EPG) { 
		ch->bypass_sl_and_db = 1;
	}
	if (ch->clock->no_time_ctrl) {
		switch (ch->esd->decoderConfig->streamType) {
		case GF_STREAM_AUDIO:
		case GF_STREAM_VISUAL:
			break;
		default:
			ch->dispatch_after_db = 1;
			break;
		}
	}

	/*get duration*/
	com.command_type = GF_NET_CHAN_DURATION;
	com.base.on_channel = ch;
	if (gf_term_service_command(ch->service, &com) == GF_OK)
		gf_odm_set_duration(ch->odm, ch, (u64) (1000*com.duration.duration));
}

void gf_es_config_drm(GF_Channel *ch, GF_NetComDRMConfig *drm_cfg)
{
	GF_Terminal *term = ch->odm->term;
	u32 i, count;
	GF_Err e;
	GF_IPMPEvent evt;
	GF_OMADRM2Config cfg;
	GF_OMADRM2Config isma_cfg;

	/*always buffer when fetching keys*/
	ch_buffer_on(ch);
	ch->is_protected = 1;

	memset(&evt, 0, sizeof(GF_IPMPEvent));
	evt.event_type = GF_IPMP_TOOL_SETUP;
	evt.channel = ch;

	/*push all cfg data*/
	if (drm_cfg->contentID) {
		evt.config_data_code = GF_4CC('o','d','r','m');
		memset(&cfg, 0, sizeof(cfg));
		cfg.scheme_version = drm_cfg->scheme_version;
		cfg.scheme_type = drm_cfg->scheme_type;
		cfg.scheme_uri = drm_cfg->scheme_uri;
		cfg.kms_uri = drm_cfg->kms_uri;
		memcpy(cfg.hash, drm_cfg->hash, sizeof(char)*20);
		cfg.contentID = drm_cfg->contentID;
		cfg.oma_drm_crypt_type = drm_cfg->oma_drm_crypt_type;
		cfg.oma_drm_use_pad = drm_cfg->oma_drm_use_pad;
		cfg.oma_drm_use_hdr = drm_cfg->oma_drm_use_hdr;
		cfg.oma_drm_textual_headers = drm_cfg->oma_drm_textual_headers;
		cfg.oma_drm_textual_headers_len = drm_cfg->oma_drm_textual_headers_len;
		evt.config_data = &cfg;
	} else {
		evt.config_data_code = GF_4CC('i','s','m','a');
		memset(&isma_cfg, 0, sizeof(isma_cfg));
		isma_cfg.scheme_version = drm_cfg->scheme_version;
		isma_cfg.scheme_type = drm_cfg->scheme_type;
		isma_cfg.scheme_uri = drm_cfg->scheme_uri;
		isma_cfg.kms_uri = drm_cfg->kms_uri;
		evt.config_data = &isma_cfg;		
	}

	if (ch->ipmp_tool) {
		e = ch->ipmp_tool->process(ch->ipmp_tool, &evt);
		if (e) gf_term_message(ch->odm->term, ch->service->url, "Error setting up DRM tool", e);
		ch_buffer_off(ch);
		return;
	}

	/*browse all available tools*/
	count = gf_modules_get_count(term->user->modules);
	for (i=0; i< count; i++) {
		ch->ipmp_tool = (GF_IPMPTool *) gf_modules_load_interface(term->user->modules, i, GF_IPMP_TOOL_INTERFACE);
		if (!ch->ipmp_tool) continue;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[IPMP] Checking if IPMP tool %s can handle channel protection scheme\n", ch->ipmp_tool->module_name));
		e = ch->ipmp_tool->process(ch->ipmp_tool, &evt);
		if (e==GF_OK) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[IPMP] Associating IPMP tool %s to channel %d\n", ch->ipmp_tool->module_name, ch->esd->ESID));
			ch_buffer_off(ch);
			return;
		}
		gf_modules_close_interface((GF_BaseInterface *) ch->ipmp_tool);
		ch->ipmp_tool = NULL;
	}
	gf_term_message(ch->odm->term, ch->service->url, "No IPMP tool suitable to handle channel protection", GF_NOT_SUPPORTED);
	ch_buffer_off(ch);
}

