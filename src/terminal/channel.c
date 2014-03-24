/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

void gf_es_buffer_off(GF_Channel *ch)
{
	/*just in case*/
	if (ch->BufferOn) {
		ch->BufferOn = 0;
		gf_clock_buffer_off(ch->clock);
		GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: buffering off at STB %d (OTB %d) (nb buffering on clock: %d)\n", ch->esd->ESID, gf_term_get_time(ch->odm->term), gf_clock_time(ch->clock), ch->clock->Buffering));
	}
}


void gf_es_buffer_on(GF_Channel *ch)
{
	/*don't buffer on an already running clock*/
	if (ch->clock->no_time_ctrl && ch->clock->clock_init && (ch->esd->ESID!=ch->clock->clockID) ) return;
	if (ch->dispatch_after_db) return;

	/*just in case*/
	if (!ch->BufferOn) {
		ch->BufferOn = 1;
		gf_clock_buffer_on(ch->clock);
		GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: buffering on at %d (nb buffering on clock: %d)\n", ch->esd->ESID, gf_term_get_time(ch->odm->term), ch->clock->Buffering));
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
	ch->min_ts_inc = 0;
	ch->min_computed_cts = 0;
	gf_es_buffer_off(ch);

	if (ch->buffer) gf_free(ch->buffer);
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
	tmp->recompute_dts = esd->slConfig->no_dts_signaling;

	tmp->ocr_scale = 0;
	if (esd->slConfig->OCRResolution) {
		tmp->ocr_scale = 1000;
		tmp->ocr_scale /= esd->slConfig->OCRResolution;
	}

	Channel_Reset(tmp, 0);
	return tmp;
}


/*reconfig SL settings for this channel - this is needed by some net services*/
void gf_es_reconfig_sl(GF_Channel *ch, GF_SLConfig *slc, Bool use_m2ts_sections)
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
	ch->recompute_dts = ch->esd->slConfig->no_dts_signaling;
	ch->ocr_scale = 0;
	if (ch->esd->slConfig->OCRResolution) {
		ch->ocr_scale = 1000;
		ch->ocr_scale /= ch->esd->slConfig->OCRResolution;
	}
	ch->carousel_type = GF_ESM_CAROUSEL_NONE;
	if (use_m2ts_sections) {
		ch->carousel_type = GF_ESM_CAROUSEL_MPEG2;
	} else {
		switch (ch->esd->decoderConfig->streamType) {
		case GF_STREAM_OD:
		case GF_STREAM_SCENE:
			ch->carousel_type = ch->esd->slConfig->AUSeqNumLength ? GF_ESM_CAROUSEL_MPEG4 : GF_ESM_CAROUSEL_NONE;
			break;
		}
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
	gf_free(ch);
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
	objects started several times by the scene but not stopped at the ODManager level (cf gf_odm_stop)*/
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
		if (! ch->is_pulling)
			gf_es_buffer_on(ch);
	}
	ch->last_au_time = gf_term_get_time(ch->odm->term);
	ch->es_state = GF_ESM_ES_RUNNING;

	ch->resync_drift = 0;
	if (ch->clock->clockID==ch->esd->ESID) {
		const char *opt = gf_cfg_get_key(ch->clock->term->user->config, "Systems", "ResyncLateClock");
		if (opt) ch->resync_drift = atoi(opt);
	}
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

	gf_es_buffer_off(ch);

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
	if (ch->buffer) gf_free(ch->buffer);
	ch->buffer = NULL;
	ch->AULength = 0;
	ch->au_sn = 0;
}

void gf_es_reset_buffers(GF_Channel *ch)
{
	gf_mx_p(ch->mx);

	if (ch->buffer) gf_free(ch->buffer);
	ch->buffer = NULL;
	ch->len = ch->allocSize = 0;

	gf_db_unit_del(ch->AU_buffer_first);
	ch->AU_buffer_first = ch->AU_buffer_last = NULL;
	ch->AU_Count = 0;

	ch->BufferTime = 0;
	gf_mx_v(ch->mx);

	if (ch->odm->codec && ch->odm->codec->CB) 
		gf_cm_reset(ch->odm->codec->CB);

}

void gf_es_reset_timing(GF_Channel *ch)
{
	struct _decoding_buffer *au = ch->AU_buffer_first;
	gf_mx_p(ch->mx);

	if (ch->buffer) gf_free(ch->buffer);
	ch->buffer = NULL;
	ch->len = ch->allocSize = 0;

	while (au) {
		au->CTS = au->DTS = 0;
		au = au->next;
	}
	ch->BufferTime = 0;
	ch->IsClockInit = 0;
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

	/*nothing received, buffer needed*/
	if (!ch->first_au_fetched && !ch->AU_buffer_first) {
		u32 now = gf_term_get_time(ch->odm->term);
		/*data timeout (no data sent)*/
		if (now > ch->last_au_time + ch->clock->data_timeout) {
			gf_term_message(ch->odm->term, ch->service->url, "Data timeout - aborting buffering", GF_OK); 
			ch->MinBuffer = ch->MaxBuffer = 0;
			ch->au_duration = 0;
			gf_scene_buffering_info(ch->odm->parentscene ? ch->odm->parentscene : ch->odm->subscene);
			return 0;
		} else {
			now = ch->clock->data_timeout + ch->last_au_time - now;
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
		/*if more than MaxBuffer sec since last AU don't buffer and prevent rebuffering on short streams
		this will also work for channels ignoring timing
		we use MaxBuffer as some transport protocols (HTTP streaming, DVB-H) will work in burst modes of MaxBuffer 
		*/
		if (now > ch->last_au_time + 2*ch->MaxBuffer ) {
			/*this can be safely seen as a stream with very few updates (likely only one)*/
			if (!ch->AU_buffer_first && ch->first_au_fetched) 
				ch->MinBuffer = 0;
			return 0;
		}
		return 1;
	}
	return 0;
}

static void Channel_UpdateBuffering(GF_Channel *ch, Bool update_info)
{
	if (update_info && ch->MaxBuffer) gf_scene_buffering_info(ch->odm->parentscene ? ch->odm->parentscene : ch->odm->subscene);

	gf_term_service_media_event(ch->odm, GF_EVENT_MEDIA_PROGRESS);
	gf_term_service_media_event(ch->odm, GF_EVENT_MEDIA_TIME_UPDATE);
	
	if (!Channel_NeedsBuffering(ch, 0)) {
		gf_es_buffer_off(ch);
		if (ch->MaxBuffer && update_info) gf_scene_buffering_info(ch->odm->parentscene ? ch->odm->parentscene : ch->odm->subscene);

		gf_term_service_media_event(ch->odm, GF_EVENT_MEDIA_PLAYING);
	}
}

static void Channel_UpdateBufferTime(GF_Channel *ch)
{
	if (!ch->AU_buffer_first || !ch->IsClockInit) {
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
		if (bt>0) {
			ch->BufferTime = (u32) bt;
			if (ch->clock->speed != FIX_ONE) {
				ch->BufferTime = FIX2INT( gf_divfix( INT2FIX(ch->AU_buffer_last->DTS - ch->AU_buffer_first->DTS) , ch->clock->speed)) ;
			}
		} else {	
			ch->BufferTime = 0;
		}
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
			gf_free(ch->buffer);
			ch->buffer = NULL;
		}
		return;
	}

	au = gf_db_unit_new();
	if (!au) {
		gf_free(ch->buffer);
		ch->buffer = NULL;
		ch->len = 0;
		return;
	}

	au->CTS = ch->CTS;
	au->DTS = ch->DTS;
	if (ch->IsRap) au->flags |= GF_DB_AU_RAP;
	if (ch->CTS_past_offset) {
		au->CTS = ch->CTS_past_offset;
		au->flags |= GF_DB_AU_CTS_IN_PAST;
		ch->CTS_past_offset = 0;
	}
	if (ch->no_timestamps) {
		au->flags |= GF_DB_AU_NO_TIMESTAMPS;
		ch->no_timestamps=0;
	}
	au->data = ch->buffer;
	au->dataLength = ch->len;
	au->PaddingBits = ch->padingBits;

	ch->IsRap = 0;
	ch->padingBits = 0;
	au->next = NULL;
	ch->buffer = NULL;

	if (ch->len + ch->media_padding_bytes != ch->allocSize) {
		au->data = (char*)gf_realloc(au->data, sizeof(char) * (au->dataLength + ch->media_padding_bytes));
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
		slh.randomAccessPointFlag = (au->flags & GF_DB_AU_RAP) ? 1 : 0;
		ch->service->cache->Write(ch->service->cache, ch, au->data, au->dataLength, &slh);
	}

	if (!ch->AU_buffer_first) {
		ch->AU_buffer_first = au;
		ch->AU_buffer_last = au;
		ch->AU_Count = 1;
	} else {
		if (!ch->recompute_dts && (ch->AU_buffer_last->DTS<=au->DTS)) {
			ch->AU_buffer_last->next = au;
			ch->AU_buffer_last = ch->AU_buffer_last->next;
		}
		/*enable deinterleaving only for audio channels (some video transport may not be able to compute DTS, cf MPEG1-2/RTP)
		HOWEVER, we must recompute a monotone increasing DTS in case the decoder does perform frame reordering
		in which case the DTS is used for presentation time!!*/
		else if (ch->esd->decoderConfig->streamType!=GF_STREAM_AUDIO) {
			/*append AU*/
			ch->AU_buffer_last->next = au;
			ch->AU_buffer_last = ch->AU_buffer_last->next;

			if (ch->recompute_dts) {
#if 0
				if (au->DTS > au->CTS)
					au->DTS = au->CTS;

#else
				au->CTS = au->DTS = ch->CTS;

#if 0
				GF_DBUnit *au_prev, *ins_au;
				u32 DTS, dts_shift;
				u32 minDIFF = (u32) -1;
				u32 minCTS = (u32) -1;
				DTS = (u32) -1;


				/*compute min CTS and min CTS diff */
				au_prev = ch->AU_buffer_first;
				while (au_prev->next) {
					s32 TS_diff;
					if (au_prev->CTS > au_prev->next->CTS) {
						TS_diff = au_prev->CTS - au_prev->next->CTS;
					} else {
						TS_diff = au_prev->next->CTS - au_prev->CTS;
					}
					if (TS_diff && (TS_diff<minDIFF)) 
						minDIFF = TS_diff;
					
					if (au_prev->DTS < minCTS)
						minCTS = au_prev->CTS;

					au_prev = au_prev->next;
				}
				ch->min_ts_inc = minDIFF;
				if (!ch->min_computed_cts) {
					ch->min_computed_cts = au->CTS;
				}


				GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] Media deinterleaving OD %d ch %d\n", ch->odm->OD->objectDescriptorID, ch->esd->ESID));

				dts_shift = 0;
				DTS = au->DTS;
				au_prev = ch->AU_buffer_first;
				/*locate first AU in buffer with DTS greater than new unit CTS*/
				while (au_prev->next && (au_prev->DTS < DTS) ) {
					if (au_prev->DTS > au_prev->CTS) {
						u32 au_dts_shift = au_prev->DTS - au_prev->CTS;
						if (au_dts_shift > dts_shift) dts_shift = au_dts_shift;
					}
					au_prev = au_prev->next;
				}
				/*remember insertion point*/
				ins_au = au_prev;
				/*shift all following frames DTS*/
				while (au_prev->next) {
					au_prev->next->DTS = au_prev->DTS;
					if (au_prev->next->DTS > au_prev->next->CTS) {
						u32 au_dts_shift = au_prev->next->DTS - au_prev->next->CTS;
						if (au_dts_shift > dts_shift ) dts_shift = au_dts_shift;
					}
					au_prev = au_prev->next;
				}
				/*and apply*/
				ins_au->DTS = DTS;
				if (ins_au->DTS > ins_au->CTS) {
					u32 au_dts_shift = ins_au->DTS - ins_au->CTS;
					if (au_dts_shift > dts_shift ) dts_shift = au_dts_shift;
				}
				
				if (dts_shift) {
					au_prev = ch->AU_buffer_first;
					/*locate first AU in buffer with DTS greater than new unit CTS*/
					while (au_prev ) {
						if (au_prev->DTS > dts_shift) {
							au_prev->DTS -= dts_shift;
						} else {
							au_prev->DTS = 0;
						}
						/*FIXME - there is still a bug in DTS recomputation ...*/
//						assert(au_prev->DTS<= au_prev->CTS);
						if (au_prev->DTS> au_prev->CTS)
							au_prev->DTS = au_prev->CTS;
						au_prev = au_prev->next;
					}
				}
#endif

#endif

			} else {
				au->DTS = 0;
			}
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
					gf_free(au->data);
					gf_free(au);
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

	GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d (%s) - Dispatch AU DTS %d - CTS %d - RAP %d - size %d time %d Buffer %d Nb AUs %d - First AU relative timing %d\n", ch->esd->ESID, ch->odm->net_service->url, au->DTS, au->CTS, au->flags&1, au->dataLength, gf_clock_real_time(ch->clock), ch->BufferTime, ch->AU_Count, ch->AU_buffer_first ? ch->AU_buffer_first->DTS - gf_clock_time(ch->clock) : 0 ));

	/*little optimisation: if direct dispatching is possible, try to decode the AU
	we must lock the media scheduler to avoid deadlocks with other codecs accessing the scene or 
	media resources*/
	if (ch->dispatch_after_db) {
		u32 retry = 100;
		u32 current_frame;
		GF_Terminal *term = ch->odm->term;
		gf_es_buffer_off(ch);

		gf_es_lock(ch, 0);
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
		gf_es_lock(ch, 1);

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
			gf_term_service_media_event(ch->odm, GF_EVENT_MEDIA_PROGRESS);
			ch->last_au_time = time;
		}
	}

	gf_es_lock(ch, 0);
}

void Channel_ReceiveSkipSL(GF_ClientService *serv, GF_Channel *ch, const char *StreamBuf, u32 StreamLength)
{
	GF_DBUnit *au;
	if (!StreamLength) return;

	gf_es_lock(ch, 1);
	au = gf_db_unit_new();
	au->flags = GF_DB_AU_RAP;
	au->DTS = gf_clock_time(ch->clock);
	au->data = (char*)gf_malloc(sizeof(char) * (ch->media_padding_bytes + StreamLength));
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



static void gf_es_check_timing(GF_Channel *ch)
{
	/*the first data received inits the clock - this is needed to handle clock dependencies on non-initialized 
	streams (eg, bifs/od depends on audio/video clock)*/
	if (!ch->clock->clock_init) {
		if (!ch->clock->use_ocr) {
			gf_clock_set_time(ch->clock, ch->CTS);
			GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: Forcing clock initialization at STB %d - AU DTS %d CTS %d\n", ch->esd->ESID, gf_term_get_time(ch->odm->term), ch->DTS, ch->CTS));
			ch->IsClockInit = 1;
		} 
	}
	/*channel is the OCR, force a re-init of the clock since we cannot assume the AU used to init the clock was
	not sent ahead of time*/
	else if (gf_es_owns_clock(ch)) {
		if (!ch->clock->use_ocr) {
			ch->clock->clock_init = 0;
			gf_clock_set_time(ch->clock, ch->DTS);
			GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: initializing clock at STB %d - AU DTS %d - %d buffering - OTB %d\n", ch->esd->ESID, gf_term_get_time(ch->odm->term), ch->DTS, ch->clock->Buffering, gf_clock_time(ch->clock) ));
			ch->IsClockInit = 1;
		}
	}
	/*if channel is not the OCR, shift all time stamps to match the current time at clock init*/
	else if (!ch->IsClockInit ) {
//		ch->ts_offset += gf_clock_real_time(ch->clock);
		if (ch->clock->clock_init) {
			ch->IsClockInit = 1;
			if (ch->odm->flags & GF_ODM_INHERIT_TIMELINE) {
//				ch->ts_offset += gf_clock_real_time(ch->clock) - ch->CTS;
			}
		}
	}
	/*deal with some broken DMB streams were the timestamps on BIFS/OD are not set (0) or completely out of sync
	of the OCR clock (usually audio). If the audio codec (BSAC ...) is not found, we force re-initializing of the clock
	so that video can play back correctly*/
	else if (gf_clock_time(ch->clock) * 1000 < ch->DTS) {
		ch->clock->clock_init = 0;
		gf_clock_set_time(ch->clock, ch->DTS);
		GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: re-initializing clock at STB %d - AU DTS %d - %d buffering\n", ch->esd->ESID, gf_term_get_time(ch->odm->term), ch->DTS, ch->clock->Buffering));
		ch->IsClockInit = 1;
	}
}


void gf_es_dispatch_raw_media_au(GF_Channel *ch, char *payload, u32 payload_size, u32 cts)
{
	u32 now;
	GF_CompositionMemory *cb;
	GF_CMUnit *cu;
	if (!payload || !ch->odm->codec->CB) return;
	if (!ch->odm->codec->CB->no_allocation) return;

	now = gf_clock_real_time(ch->clock);
	if (cts + ch->MinBuffer < now) {
		if (ch->MinBuffer && (ch->is_raw_channel==2)) {
			ch->clock->clock_init = 0;
			gf_clock_set_time(ch->clock, cts);
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM%d] Raw Frame dispatched at OTB %u but frame TS is %u ms - adjusting clock\n", ch->odm->OD->objectDescriptorID, now, cts));
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM%d] Raw Frame dispatched at OTB %u but frame TS is %u ms - DROPPING\n", ch->odm->OD->objectDescriptorID, now, cts));
		}
		return;
	}

	cb = ch->odm->codec->CB;
	cu = gf_cm_lock_input(cb, cts, 1);
	if (cu) {
		u32 size = 0;
		assert(cu->RenderedLength==0);
		if (cb->UnitSize >= payload_size) {
			cu->data = payload;
			size = payload_size;
			cu->TS = cts;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] Raw Frame dispatched to CB - TS %d ms - OTB %d ms - OTB_drift %d ms\n", ch->odm->OD->objectDescriptorID, cu->TS, gf_clock_real_time(ch->clock), gf_clock_time(ch->clock) ));
		}
		gf_cm_unlock_input(cb, cu, size, 1);

		if (ch->BufferOn) {
			ch->BufferOn = 0;
			gf_clock_buffer_off(ch->clock);
			gf_cm_abort_buffering(cb);
		}
		/*since the CB is a simple pointer to the input frame, wait until it is released before getting 
		back to the caller module*/
		if (size) {
			gf_sema_wait(ch->odm->raw_frame_sema);
			assert(cb->output->dataLength == 0);
		}
	}
}


/*handles reception of an SL-PDU, logical or physical*/
void gf_es_receive_sl_packet(GF_ClientService *serv, GF_Channel *ch, char *payload, u32 payload_size, GF_SLHeader *header, GF_Err reception_status)
{
	GF_SLHeader hdr;
	u32 nbAU, OldLength, size;
	Bool EndAU, NewAU;
	Bool init_ts = 0;

	if (ch->bypass_sl_and_db) {
		GF_SceneDecoder *sdec;
		ch->IsClockInit = 1;
		if (ch->odm->subscene) {
			sdec = (GF_SceneDecoder *)ch->odm->subscene->scene_codec->decio;
		} else {
			sdec = (GF_SceneDecoder *)ch->odm->codec->decio;
		}
		gf_mx_p(ch->mx);
		sdec->ProcessData(sdec, payload, payload_size, ch->esd->ESID, 0, 0);
		gf_mx_v(ch->mx);
		return;
	}

	if (ch->es_state != GF_ESM_ES_RUNNING) return;

	if (ch->skip_sl) {
		Channel_ReceiveSkipSL(serv, ch, payload, payload_size);
		return;
	}
	if (ch->is_raw_channel) {
		ch->CTS = ch->DTS = (u32) (ch->ts_offset + (header->compositionTimeStamp - ch->seed_ts) * 1000 / ch->ts_res);
		if (!ch->IsClockInit) {
			gf_es_check_timing(ch);
		}
		if (payload)
			gf_es_dispatch_raw_media_au(ch, payload, payload_size, ch->CTS);
		return;
	}

	/*physical SL-PDU - depacketize*/
	if (!header) {
		u32 SLHdrLen;
		if (!payload_size) return;
		gf_sl_depacketize(ch->esd->slConfig, &hdr, payload, payload_size, &SLHdrLen);
		payload_size -= SLHdrLen;
		payload += SLHdrLen;
	} else {
		hdr = *header;
	}

	/*we ignore OCRs for the moment*/
	if (hdr.OCRflag) {
		if (!ch->IsClockInit) {
			/*channel is the OCR, re-initialize the clock with the proper OCR*/
			if (gf_es_owns_clock(ch)) {
				u32 OCR_TS;

				/*timestamps of PCR stream haven been shifted - shift the OCR as well*/
				if (ch->seed_ts) {
					u64 diff_ts;
					Double scale = hdr.m2ts_pcr ? 27000000 : ch->esd->slConfig->OCRResolution;
					scale /= ch->ts_res;
					diff_ts = (u64) (ch->seed_ts * scale);
					hdr.objectClockReference -= diff_ts;
				}

				/*if SL is mapped from network module(eg not coded), OCR=PCR shall be given in 27Mhz units*/
				if (hdr.m2ts_pcr) {
					OCR_TS = (u32) ( hdr.objectClockReference / 27000);
				} else {
					OCR_TS = (u32) ( (s64) (hdr.objectClockReference) * ch->ocr_scale);
				}
				OCR_TS += ch->ts_offset;
				ch->clock->clock_init = 0;
				ch->prev_pcr_diff = 0;

				gf_clock_set_time(ch->clock, OCR_TS);
				/*many TS streams deployed with HLS have broken PCRs - we will check their consistency
				when receiving the first AU with DTS/CTS on this channel*/
				ch->clock->probe_ocr = 1;
				GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: initializing clock at STB %d from OCR TS %d (original TS "LLD") - %d buffering - OTB %d\n", ch->esd->ESID, gf_term_get_time(ch->odm->term), OCR_TS, hdr.objectClockReference, ch->clock->Buffering, gf_clock_time(ch->clock) ));
				if (ch->clock->clock_init) ch->IsClockInit = 1;

			}
		} else 	if (hdr.OCRflag==2) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_SYNC, ("[SyncLayer] ES%d: At OTB %u clock disctontinuity was signaled\n", ch->esd->ESID, gf_clock_real_time(ch->clock) ));
			gf_clock_discontinuity(ch->clock, ch->odm->parentscene, (hdr.m2ts_pcr==2) ? GF_TRUE : GF_FALSE);

			//and re-init timing
			gf_es_receive_sl_packet(serv, ch, payload, payload_size, header, reception_status);
			return;
		} else {
			u32 ck;
			u32 OCR_TS;
			s32 pcr_diff, pcr_pcrprev_diff;
			if (hdr.m2ts_pcr) {
				OCR_TS = (u32) ( hdr.objectClockReference / 27000);
			} else {
				OCR_TS = (u32) ( (s64) (hdr.objectClockReference) * ch->ocr_scale);
			}
			ck = gf_clock_time(ch->clock);

			pcr_diff = (s32) OCR_TS - (s32) ck;
			pcr_pcrprev_diff = pcr_diff - ch->prev_pcr_diff;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d: At OTB %u got OCR %u (original TS "LLU") - diff %d%s\n", ch->esd->ESID, gf_clock_real_time(ch->clock), OCR_TS, hdr.objectClockReference, pcr_diff, (hdr.m2ts_pcr==2) ? " - PCR Discontinuity flag" : "" ));

			//PCR loop or disc - use 10 sec as a threshold - it may happen that video is sent up to 4 or 5 seconds ahead of the PCR in some systems
			if (ch->prev_pcr_diff && ABS(pcr_pcrprev_diff) > 10000) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_SYNC, ("[SyncLayer] ES%d: At OTB %u detected PCR %s (PCR diff %d - last PCR diff %d)\n", ch->esd->ESID, gf_clock_real_time(ch->clock), (hdr.m2ts_pcr==2) ? "discontinuity" : "looping", pcr_diff, ch->prev_pcr_diff));
				gf_clock_discontinuity(ch->clock, ch->odm->parentscene, (hdr.m2ts_pcr==2) ? GF_TRUE : GF_FALSE);

				//and re-init timing
				gf_es_receive_sl_packet(serv, ch, payload, payload_size, header, reception_status);
				//do not probe OCR after a discontinuity
				ch->clock->probe_ocr = 0;
				return;
			} else {
				ch->prev_pcr_diff = pcr_diff;
			}
		}
		if (!payload_size) return;
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
	} else if ((ch->carousel_type!=GF_ESM_CAROUSEL_MPEG2) && (!ch->esd->slConfig->useRandomAccessPointFlag || ch->codec_resilient) ) {
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
		ch->skip_time_check_for_pending = 0;
		init_ts = 1;
	}
	/*if not first packet but about to force a clock init, do init timestamps*/
	else if (!ch->IsClockInit) {
		/*don't process anything until the clock is initialized*/
		if (ch->esd->dependsOnESID && !gf_es_owns_clock(ch)) 
			return;
		if (hdr.compositionTimeStampFlag)
			init_ts = 1;
	}


	/*if we had a previous buffer, add or discard it, depending on codec resilience*/
	if (hdr.accessUnitStartFlag && ch->buffer) {
		if (ch->esd->slConfig->useAccessUnitEndFlag) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_SYNC, ("[SyncLayer] ES%d: missed end of AU (DTS %d)\n", ch->esd->ESID, ch->DTS));
		}
		if (ch->codec_resilient) {
			if (!ch->IsClockInit && !ch->skip_time_check_for_pending) gf_es_check_timing(ch);
			Channel_DispatchAU(ch, 0);
		} else {
			gf_free(ch->buffer);
			ch->buffer = NULL;
			ch->AULength = 0;
			ch->len = ch->allocSize = 0;
		}
	}

	if (init_ts) {
		/*Get CTS */
		if (ch->esd->slConfig->useTimestampsFlag) {
			if (hdr.compositionTimeStampFlag) {
				ch->net_dts = ch->net_cts = hdr.compositionTimeStamp;
				/*get DTS */
				if (hdr.decodingTimeStampFlag) ch->net_dts = hdr.decodingTimeStamp;

#if 0
				/*until clock is not init check seed ts*/
				if (!ch->IsClockInit && (ch->net_dts < ch->seed_ts)) 
					ch->seed_ts = ch->net_dts;
#endif				

				if (ch->net_cts<ch->seed_ts) {
					u64 diff = ch->seed_ts - ch->net_cts;
					ch->CTS_past_offset = (u32) (diff * 1000 / ch->ts_res) + ch->ts_offset;

					ch->net_dts = ch->net_cts = 0;
					ch->CTS = ch->DTS = gf_clock_time(ch->clock);
				} else {
					if (ch->net_dts>ch->seed_ts) ch->net_dts -= ch->seed_ts;
					else ch->net_dts=0;
					ch->net_cts -= ch->seed_ts;
					ch->CTS_past_offset = 0;

					/*TS Wraping not tested*/
					ch->CTS = (u32) (ch->ts_offset + (s64) (ch->net_cts) * 1000 / ch->ts_res);
					ch->DTS = (u32) (ch->ts_offset + (s64) (ch->net_dts) * 1000 / ch->ts_res);
				}

				if (ch->odm->parentscene && ch->odm->parentscene->root_od->addon) {
					ch->DTS = (u32) gf_scene_adjust_timestamp_for_addon(ch->odm->parentscene, ch->DTS, ch->odm->parentscene->root_od->addon);
					ch->CTS = (u32) gf_scene_adjust_timestamp_for_addon(ch->odm->parentscene, ch->CTS, ch->odm->parentscene->root_od->addon);
				}

				if (ch->clock->probe_ocr && gf_es_owns_clock(ch)) {
					s32 diff_ts = ch->DTS;
					diff_ts -= ch->clock->init_time;
					if (ABS(diff_ts) > 10000) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_SYNC, ("[SyncLayer] ES%d: invalid clock reference detected - DTS %d but OCR %d - using DTS as OCR\n", ch->esd->ESID, ch->DTS, ch->clock->init_time));
						ch->clock->clock_init = 0;
						gf_clock_set_time(ch->clock, ch->DTS-1000);
					}
					ch->clock->probe_ocr = 0;
				}

				ch->no_timestamps = 0;
			} else {
				ch->no_timestamps = 1;
			}
		} else {
			u32 AUSeqNum = hdr.AU_sequenceNumber;
			/*use CU duration*/
			if (!ch->IsClockInit)
				ch->DTS = ch->CTS = ch->ts_offset;

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
	}

	if (hdr.accessUnitStartFlag) {
		u32 AUSeqNum = hdr.AU_sequenceNumber;

		/*if the AU Length is carried in SL, get its size*/
		if (ch->esd->slConfig->AULength > 0) {
			ch->AULength = hdr.accessUnitLength;
		} else {
			ch->AULength = 0;
		}
		/*carousel for repeated AUs.*/
		if (ch->carousel_type) {
/* not used :			Bool use_rap = hdr.randomAccessPointFlag; */

			if (ch->carousel_type==GF_ESM_CAROUSEL_MPEG2) {
				AUSeqNum = hdr.m2ts_version_number_plus_one-1;
				/*mpeg-2 section carrouseling does not take into account the RAP nature of the tables*/


				if (AUSeqNum==ch->au_sn) {
					if (ch->stream_state) {
						ch->stream_state=0;
						GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: MPEG-2 Carousel: tuning in\n", ch->esd->ESID));
					} else {
						ch->skip_carousel_au = 1;
						GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d: MPEG-2 Carousel: repeated AU (TS %d) - skipping\n", ch->esd->ESID, ch->CTS));
						return;
					}
				} else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d: MPEG-2 Carousel: updated AU (TS %d)\n", ch->esd->ESID, ch->CTS));
					ch->stream_state=0;
					ch->au_sn = AUSeqNum;
				}
			} else {
				if (hdr.randomAccessPointFlag) {
					/*initial tune-in*/
					if (ch->stream_state==1) {
						GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: RAP Carousel found (TS %d) - tuning in\n", ch->esd->ESID, ch->CTS));
						ch->au_sn = AUSeqNum;
						ch->stream_state = 0;
					}
					/*carousel RAP*/
					else if (AUSeqNum == ch->au_sn) {
						/*error recovery*/
						if (ch->stream_state==2) {
							GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: RAP Carousel found (TS %d) - recovering\n", ch->esd->ESID, ch->CTS));
							ch->stream_state = 0;
						} 
						else {
							ch->skip_carousel_au = 1;
							GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: RAP Carousel found (TS %d) - skipping\n", ch->esd->ESID, ch->CTS));
							return;
						}
					}
					/*regular RAP*/
					else {
						if (ch->stream_state==2) {
							GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: RAP Carousel found (TS %d) - recovering from previous errors\n", ch->esd->ESID, ch->CTS));
						}
 						ch->au_sn = AUSeqNum;
						ch->stream_state = 0;
					}
				} 
				/*regular AU but waiting for RAP*/
				else if (ch->stream_state) {
#if 0
					ch->skip_carousel_au = 1;
					GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: Waiting for RAP Carousel - skipping\n", ch->esd->ESID));
					return;
#else
					GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: Tuning in before RAP\n", ch->esd->ESID));
#endif
				}
				/*previous packet(s) loss: check for critical or non-critical AUs*/
				else if (reception_status == GF_REMOTE_SERVICE_ERROR) { 
					if (ch->au_sn == AUSeqNum) {
						GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: Lost a non critical packet\n", ch->esd->ESID));
					} 
					/*Packet lost are critical*/
					else {
						ch->stream_state = 2;
						GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: Lost a critical packet - skipping\n", ch->esd->ESID));
						return;
					}
				} else {
					ch->au_sn = AUSeqNum;
					GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d: NON-RAP AU received (TS %d)\n", ch->esd->ESID, ch->DTS));
				}
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

	/*get AU end state*/	
	OldLength = ch->buffer ? ch->len : 0;
	EndAU = hdr.accessUnitEndFlag;
	if (ch->AULength == OldLength + payload_size) EndAU = 1;
	if (EndAU) ch->NextIsAUStart = 1;

	/*init clock if end of AU or if header is valid*/
	if ((EndAU || init_ts) && !ch->IsClockInit && !ch->skip_time_check_for_pending) {
		ch->skip_time_check_for_pending = 0;
		gf_es_check_timing(ch);
	}

	/* we need to skip all the packets of the current AU in the carousel scenario */
	if (ch->skip_carousel_au == 1) return;

	if (!payload_size && EndAU && ch->buffer) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d: Empty packet, flushing buffer\n", ch->esd->ESID));
		Channel_DispatchAU(ch, 0);
		return;
	}
	if (!payload_size) return;

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
		size = payload_size + ch->media_padding_bytes;
		ch->buffer = (char*)gf_malloc(sizeof(char) * size);
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
		evt.data_size = payload_size;
		evt.is_encrypted = (hdr.isma_encrypted || hdr.cenc_encrypted) ? 1 : 0;
		if (hdr.isma_encrypted) {
			evt.isma_BSO = hdr.isma_BSO;
		} else if (hdr.cenc_encrypted) {
			evt.sai = hdr.sai;
			evt.saiz = hdr.saiz;
			evt.IV_size = hdr.IV_size;
		}
		e = ch->ipmp_tool->process(ch->ipmp_tool, &evt);

		/*we discard undecrypted AU*/
		if (e) {
			if (e==GF_EOS) {
				gf_es_on_eos(ch);
				/*restart*/
				if (evt.restart_requested) {
					if (ch->odm->parentscene->is_dynamic_scene) {
						gf_scene_restart_dynamic(ch->odm->parentscene, 0);
					} else {
						mediacontrol_restart(ch->odm);
					}
				}
			}
			return;
		}
	}

	gf_es_lock(ch, 1);

	if (hdr.paddingFlag && !EndAU) {	
		/*to do - this shouldn't happen anyway */

	} else {
		/*check if enough space*/
		size = ch->allocSize;
		if (size && (payload_size + ch->len <= size)) {
			memcpy(ch->buffer+ch->len, payload, payload_size);
			ch->len += payload_size;
		} else {
			size = payload_size + ch->len + ch->media_padding_bytes;
			ch->buffer = (char*)gf_realloc(ch->buffer, sizeof(char) * size);
			memcpy(ch->buffer+ch->len, payload, payload_size);
			ch->allocSize = size;
			ch->len += payload_size;
		}
		if (hdr.paddingFlag) ch->padingBits = hdr.paddingBits;
	}

	if (EndAU) Channel_DispatchAU(ch, hdr.au_duration);

	gf_es_lock(ch, 0);
}


/*notification of End of stream on this channel*/
void gf_es_on_eos(GF_Channel *ch)
{
	if (!ch || ch->IsEndOfStream) return;
	ch->IsEndOfStream = 1;
	
	/*flush buffer*/
	gf_es_buffer_off(ch);
	if (ch->len)
		Channel_DispatchAU(ch, 0);

	gf_odm_on_eos(ch->odm, ch);
}



GF_DBUnit *gf_es_get_au(GF_Channel *ch)
{
	Bool comp, is_new_data;
	GF_Err e, state;
	GF_SLHeader slh;

	if (ch->es_state != GF_ESM_ES_RUNNING) return NULL;

	if (!ch->is_pulling) {
		gf_mx_p(ch->mx);
		
		if (!ch->AU_buffer_first || (ch->BufferTime < (s32) ch->MaxBuffer/2) ) {
			/*query buffer level, don't sleep if too low*/
			GF_NetworkCommand com;
			com.command_type = GF_NET_SERVICE_FLUSH_DATA;
			com.base.on_channel = ch;
			gf_term_service_command(ch->service, &com);
		}
	
		/*we must update buffering before fetching in order to stop buffering for streams with very few
		updates (especially streams with one update, like most of OD streams)*/
		if (ch->BufferOn && ch->AU_buffer_first) Channel_UpdateBuffering(ch, 0);
		gf_mx_v(ch->mx);
		
		if (ch->BufferOn) {
			if (ch->first_au_fetched || !ch->AU_buffer_first || !ch->AU_buffer_first->next)
				return NULL;
		}
		return ch->AU_buffer_first;
	}

	//pull channel is buffering: fetch first frame for systems, or fetch until CB is full
	if (ch->BufferOn) {
		if (ch->odm->codec && ch->odm->codec->CB) {
			if (ch->odm->codec->CB->UnitCount) return NULL;
		} else {
			if (ch->first_au_fetched) return NULL;
		}
	}

	memset(&slh, 0, sizeof(GF_SLHeader));

	e = gf_term_channel_get_sl_packet(ch->service, ch, (char **) &ch->AU_buffer_pull->data, &ch->AU_buffer_pull->dataLength, &slh, &comp, &state, &is_new_data);
	if (e) state = e;
	switch (state) {
	case GF_EOS:
		gf_es_on_eos(ch);
		return NULL;
	case GF_BUFFER_TOO_SMALL:
		if (ch->MinBuffer) {
			//if we have a composition buffer, only trigger buffering when no more data is available in the CB
			if (ch->odm->codec && ch->odm->codec->CB && ch->odm->codec->CB->UnitCount<=1) {
				gf_term_service_media_event(ch->odm, GF_EVENT_MEDIA_PROGRESS);
				gf_es_buffer_on(ch);
				ch->pull_forced_buffer = 1;
			} else if (!ch->odm->codec || !ch->odm->codec->CB) {
				gf_term_service_media_event(ch->odm, GF_EVENT_MEDIA_PROGRESS);
				gf_es_buffer_on(ch);
				ch->pull_forced_buffer = 1;
			}
		}
		return NULL;
	case GF_OK:
		break;
	default:
        {
            char m[100];
            sprintf(m, "Data reception failure on channel %d", ch->esd->ESID);
		    gf_term_message(ch->odm->term, ch->service->url , m, state);
		    return NULL;
        }
	}
	assert(!comp);
	/*update timing if new stream data but send no data. in sase of encrypted data: we always need updating*/
	if (is_new_data) {
		ch->IsRap = 0;
		gf_es_receive_sl_packet(ch->service, ch, NULL, 0, &slh, GF_OK);
		
		if (ch->ipmp_tool) {
			GF_IPMPEvent evt;
			memset(&evt, 0, sizeof(evt));
			evt.event_type=GF_IPMP_TOOL_PROCESS_DATA;
			evt.data = ch->AU_buffer_pull->data;
			evt.data_size = ch->AU_buffer_pull->dataLength;
			evt.is_encrypted = (slh.isma_encrypted || slh.cenc_encrypted) ? 1 : 0;
			if (slh.isma_encrypted) {
				evt.isma_BSO = slh.isma_BSO;
			} else if (slh.cenc_encrypted) {
				evt.sai = slh.sai;
				evt.saiz = slh.saiz;
				evt.IV_size = slh.IV_size;
			}
			evt.channel = ch;
			e = ch->ipmp_tool->process(ch->ipmp_tool, &evt);

			/*we discard undecrypted AU*/
			if (e) {
				if (e==GF_EOS) {
					gf_es_on_eos(ch);
					/*restart*/
					if (evt.restart_requested) {
						if (ch->odm->parentscene->is_dynamic_scene) {
							gf_scene_restart_dynamic(ch->odm->parentscene, 0);
						} else {
							mediacontrol_restart(ch->odm);
						}
					}
				}
				gf_term_channel_release_sl_packet(ch->service, ch);
				return NULL;
			}
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d (%s) - Dispatch Pull AU DTS %d - CTS %d - size %d time %d - UTC "LLU" ms\n", ch->esd->ESID, ch->odm->net_service->url, ch->DTS, ch->CTS, ch->AU_buffer_pull->dataLength, gf_clock_real_time(ch->clock), gf_net_get_utc() ));
	}

	/*this may happen in file streaming when data has not arrived yet, in which case we discard the AU*/
	if (!ch->AU_buffer_pull->data) {
		gf_term_channel_release_sl_packet(ch->service, ch);
		return NULL;
	}
	ch->AU_buffer_pull->CTS = (u32) ch->CTS;
	ch->AU_buffer_pull->DTS = (u32) ch->DTS;
	ch->AU_buffer_pull->PaddingBits = ch->padingBits;
	ch->AU_buffer_pull->flags = 0;
	if (ch->IsRap) ch->AU_buffer_pull->flags |= GF_DB_AU_RAP;

	if (ch->pull_forced_buffer) {
		assert(ch->BufferOn);
		ch->pull_forced_buffer=0;
		gf_es_buffer_off(ch);
		Channel_UpdateBuffering(ch, 1);
	} else if (is_new_data && !ch->first_au_fetched) {
		Channel_UpdateBuffering(ch, 1);
	}

	return ch->AU_buffer_pull;
}

void gf_es_init_dummy(GF_Channel *ch)
{
	GF_SLHeader slh;
	Bool comp, is_new_data;
	GF_Err e, state;
	/*pull from stream - resume clock if needed*/
	gf_es_buffer_off(ch);

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
		if (!ch->esd->dependsOnESID)
			ch->first_au_fetched = 1;
		return;
	}

	/*lock the channel before touching the queue*/
	gf_es_lock(ch, 1);
	if (!ch->AU_buffer_first) {
		gf_es_lock(ch, 0);
		return;
	}
	if (!ch->esd->dependsOnESID)
		ch->first_au_fetched = 1;

	au = ch->AU_buffer_first;
//	ch->min_computed_cts = au->CTS;
	ch->AU_buffer_first = au->next;


	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[ODM%d] ES%d Droping AU CTS %d\n", ch->odm->OD->objectDescriptorID, ch->esd->ESID, au->CTS));

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
		gf_es_buffer_on(ch);
		gf_term_service_media_event(ch->odm, GF_EVENT_MEDIA_WAITING);
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
	GF_Scene *in_scene;

	/*check for inline*/
	in_scene = odm->subscene ? odm->subscene : odm->parentscene;
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(in_scene->root_od->channels, &i)) ) {
		if (ch->clock->no_time_ctrl) {
			in_scene->root_od->flags |= GF_ODM_NO_TIME_CTRL;
			//if (ch->clock->use_ocr && ch->BufferOn) gf_es_buffer_off(ch);
		}
	}

	i=0;
	while ((test_od = (GF_ObjectManager *)gf_list_enum(in_scene->resources, &i)) ) {
		j=0;
		while ((ch = (GF_Channel*)gf_list_enum(test_od->channels, &j)) ) {
			if (ch->clock->no_time_ctrl) {
				test_od->flags |= GF_ODM_NO_TIME_CTRL;
				//if (ch->clock->use_ocr && ch->BufferOn) gf_es_buffer_off(ch);
			}
		}
	}
}
/*performs final setup upon connection confirm*/
void gf_es_on_connect(GF_Channel *ch)
{
	const char *sOpt;
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
	case GPAC_OTI_IMAGE_JPEG:
	case GPAC_OTI_IMAGE_PNG:
		can_buffer = 0;
		break;
	}
	/*buffer setup*/
	ch->MinBuffer = ch->MaxBuffer = 0;

	/*set default values*/
	com.buffer.max = 1000;
	com.buffer.min = 0;
	sOpt = gf_cfg_get_key(ch->odm->term->user->config, "Network", "BufferLength");
	if (sOpt) com.buffer.max = atoi(sOpt);
	com.buffer.min = 0;
	sOpt = gf_cfg_get_key(ch->odm->term->user->config, "Network", "RebufferLength");
	if (sOpt) com.buffer.min = atoi(sOpt);

	//set the buffer command even though the channel is pulling, in order to indicate to the service the prefered values
	com.command_type = GF_NET_CHAN_BUFFER;
	com.base.on_channel = ch;
	if (gf_term_service_command(ch->service, &com) == GF_OK) {
		//only set the values if we can buffer
		if (can_buffer) {
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
	if (gf_term_service_command(ch->service, &com) == GF_OK) {
		if (com.duration.duration>=0)
			gf_odm_set_duration(ch->odm, ch, (u64) (1000*com.duration.duration));
	}
}

void gf_es_config_drm(GF_Channel *ch, GF_NetComDRMConfig *drm_cfg)
{
	GF_Terminal *term = ch->odm->term;
	u32 i, count;
	GF_Err e;
	GF_IPMPEvent evt;
	GF_OMADRM2Config cfg;
	GF_OMADRM2Config isma_cfg;
	GF_CENCConfig cenc_cfg;

	/*always buffer when fetching keys*/
	gf_es_buffer_on(ch);
	ch->is_protected = 1;

	memset(&evt, 0, sizeof(GF_IPMPEvent));
	evt.event_type = GF_IPMP_TOOL_SETUP;
	evt.channel = ch;

	/*push all cfg data*/
	/*CommonEncryption*/
	if ((drm_cfg->scheme_type == GF_4CC('c', 'e', 'n', 'c')) || (drm_cfg->scheme_type == GF_4CC('c','b','c','1'))) {
		evt.config_data_code = drm_cfg->scheme_type;
		memset(&cenc_cfg, 0, sizeof(cenc_cfg));
		cenc_cfg.scheme_version = drm_cfg->scheme_version;
		cenc_cfg.scheme_type = drm_cfg->scheme_type;
		cenc_cfg.PSSH_count = drm_cfg->PSSH_count;
		cenc_cfg.PSSHs = drm_cfg->PSSHs;
		evt.config_data = &cenc_cfg;	
	} else {
	/*ISMA and OMA*/
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
	}

	if (ch->ipmp_tool) {
		e = ch->ipmp_tool->process(ch->ipmp_tool, &evt);
		if (e) gf_term_message(ch->odm->term, ch->service->url, "Error setting up DRM tool", e);
		gf_es_buffer_off(ch);
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
			gf_es_buffer_off(ch);
			return;
		}
		gf_modules_close_interface((GF_BaseInterface *) ch->ipmp_tool);
		ch->ipmp_tool = NULL;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[IPMP] No IPMP tool suitable to handle channel protection scheme %s (KMS URI %s)\n", drm_cfg->scheme_uri, drm_cfg->kms_uri)); 
	gf_es_buffer_off(ch);
}

