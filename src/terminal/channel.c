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

#include <gpac/ismacryp.h>
#include <gpac/base_coding.h>
#include <gpac/constants.h>

#include "media_memory.h"

/*reset channel*/
static void Channel_Reset(GF_Channel *ch)
{
	gf_es_lock(ch, 1);

	ch->IsClockInit = 0;
	ch->au_sn = 0;
	ch->pck_sn = 0;
	ch->NeedRap = 1;
	ch->IsRap = 0;
	ch->IsEndOfStream = 0;
	ch->ts_offset = 0;
	ch->seed_ts = 0;

	/*just in case*/
	if (ch->BufferOn) {
		gf_clock_buffer_off(ch->clock);
	}
	ch->BufferOn = 0;

	if (ch->buffer) free(ch->buffer);
	ch->buffer = NULL;
	ch->len = ch->allocSize = 0;

	DB_Delete(ch->AU_buffer_first);
	ch->AU_buffer_first = ch->AU_buffer_last = NULL;
	ch->AU_Count = 0;
	ch->BufferTime = 0;
	ch->NextIsAUStart = 1;

	ch->first_au_fetched = 0;
	ch->last_IV = 0;
	if (ch->AU_buffer_pull) {
		ch->AU_buffer_pull->data = NULL;
		DB_Delete(ch->AU_buffer_pull);
		ch->AU_buffer_pull = NULL;
	}
	gf_es_lock(ch, 0);
}

GF_Channel *gf_es_new(GF_ESD *esd)
{
	u32 nbBits;
	GF_Channel *tmp = malloc(sizeof(GF_Channel));
	if (!tmp) return NULL;
	memset((void *)tmp, 0, sizeof(GF_Channel));

	tmp->mx = gf_mx_new();

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


	Channel_Reset(tmp);
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
	Channel_Reset(ch);
	if (ch->AU_buffer_pull) {
		ch->AU_buffer_pull->data = NULL;
		DB_Delete(ch->AU_buffer_pull);
	}
	if (ch->crypt) gf_crypt_close(ch->crypt);
	if (ch->mx) gf_mx_del(ch->mx);
	free(ch);
}

Bool Channel_OwnsClock(GF_Channel *ch)
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
	default:
		break;
	}

	/*reset clock if we own it*/
	if (Channel_OwnsClock(ch)) gf_clock_reset(ch->clock);

	/*reset channel*/
	Channel_Reset(ch);
	/*create pull buffer if needed*/
	if (ch->is_pulling && !ch->AU_buffer_pull) ch->AU_buffer_pull = DB_New();

	/*and start buffering - pull channels always turn off buffering immediately, otherwise 
	buffering size is setup by the network service - except InputSensor*/
	if ((ch->esd->decoderConfig->streamType != GF_STREAM_INTERACT) || ch->esd->URLString) {
		ch->BufferOn = 1;
		gf_clock_buffer_on(ch->clock);
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

	if (ch->BufferOn) {
		gf_clock_buffer_off(ch->clock);
		ch->BufferOn = 0;
	}
	ch->es_state = GF_ESM_ES_CONNECTED;
	if (ch->buffer) free(ch->buffer);
	ch->buffer = NULL;
	ch->len = 0;
	return GF_OK;
}


void Channel_WaitRAP(GF_Channel *ch)
{
	ch->pck_sn = 0;

	/*if using RAP signal and codec not resilient, wait for rap. If RAP isn't signaled DON'T wait for it :)*/
	if (ch->esd->slConfig->useRandomAccessPointFlag && !ch->codec_resilient) ch->NeedRap = 1;
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
		DB_Delete(ch->AU_buffer_first);
		ch->AU_buffer_first = ch->AU_buffer_last = NULL;
		ch->AU_Count = 0;
	} else {
		LPAUBUFFER au = ch->AU_buffer_first;
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
	/*nothing received, buffer needed*/
	if (!ch->first_au_fetched && !ch->AU_buffer_first) {
		u32 now = gf_term_get_time(ch->odm->term);
		/*data timeout (no data sent)*/
		if (now > ch->last_au_time + ch->odm->term->net_data_timeout) {
			gf_term_message(ch->odm->term, ch->service->url, "Data timeout - aborting buffering", GF_OK); 
			ch->MinBuffer = ch->MaxBuffer = 0;
			ch->au_duration = 0;
			gf_is_buffering_info(ch->odm->parentscene ? ch->odm->parentscene : ch->odm->subscene);
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
	if (update_info && ch->MaxBuffer) gf_is_buffering_info(ch->odm->parentscene ? ch->odm->parentscene : ch->odm->subscene);
	if (!Channel_NeedsBuffering(ch, 0)) {
		ch->BufferOn = 0;
		gf_clock_buffer_off(ch->clock);
		if (ch->MaxBuffer) gf_is_buffering_info(ch->odm->parentscene ? ch->odm->parentscene : ch->odm->subscene);
	}
}

static void Channel_UpdateBufferTime(GF_Channel *ch)
{
	if (!ch->AU_buffer_first) {
		ch->BufferTime = 0;
	}
	else if (ch->skip_sl) {
		LPAUBUFFER au;
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
	LPAUBUFFER au;
	if (!ch->buffer || !ch->len) {
		if (ch->buffer) {
			free(ch->buffer);
			ch->buffer = NULL;
		}
		return;
	}

	au = DB_New();
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
		au->data = realloc(au->data, sizeof(char) * (au->dataLength + ch->media_padding_bytes));
	}
	if (ch->media_padding_bytes) memset(au->data + au->dataLength, 0, sizeof(char)*ch->media_padding_bytes);
	
	ch->len = ch->allocSize = 0;
	if (ch->NeedRap && au->RAP) ch->NeedRap = 0;

	gf_es_lock(ch, 1);

	if (ch->service->cache) {
		SLHeader slh;
		memset(&slh, 0, sizeof(SLHeader));
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
			LPAUBUFFER au_prev, ins_au;
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

#if 0
			au_prev = ch->AU_buffer_first;
			fprintf(stdout, "\nDTS ");
			while (au_prev->next) {
				assert(au_prev->DTS <= au_prev->next->DTS);
				fprintf(stdout, "%d ", au_prev->DTS);
				au_prev = au_prev->next;
			}
			fprintf(stdout, "%d\n", au_prev->DTS);
#endif
		} else {
			fprintf(stdout, "Audio deinterleaving OD %d ch %d\n", ch->esd->ESID, ch->odm->OD->objectDescriptorID);
			/*de-interleaving of AUs*/
			if (ch->AU_buffer_first->DTS > au->DTS) {
				au->next = ch->AU_buffer_first;
				ch->AU_buffer_first = au;
			} else {
				LPAUBUFFER au_prev = ch->AU_buffer_first;
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

	//fprintf(stdout, "CH %d - Dispatch AU CTS %d time %d Buffer %d Nb AUs %d\n", ch->esd->ESID, au->CTS, gf_clock_time(ch->clock), ch->BufferTime, ch->AU_Count);

	if (ch->BufferOn) {
		ch->last_au_time = gf_term_get_time(ch->odm->term);
		Channel_UpdateBuffering(ch, 1);
	}
	gf_es_lock(ch, 0);
	return;
}

void Channel_RecieveSkipSL(GF_ClientService *serv, GF_Channel *ch, char *StreamBuf, u32 StreamLength)
{
	LPAUBUFFER au;
	if (!StreamLength) return;

	gf_es_lock(ch, 1);
	au = DB_New();
	au->RAP = 1;
	au->DTS = gf_clock_time(ch->clock);
	au->data = malloc(sizeof(char) * (ch->media_padding_bytes + StreamLength));
	memcpy(au->data, StreamBuf, sizeof(char) * StreamLength);
	if (ch->media_padding_bytes) memset(au->data + StreamLength, 0, sizeof(char)*ch->media_padding_bytes);
	au->dataLength = StreamLength;
	au->next = NULL;

	/*if channel owns the clock, start it*/
	if (ch->clock && !ch->IsClockInit) {
		if (Channel_OwnsClock(ch)) {
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

void Channel_DecryptISMA(GF_Channel *ch, char *data, u32 dataLength, SLHeader *slh)
{
	if (!ch->crypt) return;
	/*resync IV*/
	if (!ch->last_IV || (ch->last_IV!=slh->isma_BSO)) {
		char IV[17];
		u64 count;
		u32 remain;
		GF_BitStream *bs;
		count = slh->isma_BSO / 16;
		remain = (u32) (slh->isma_BSO % 16);

		/*format IV to begin of counter*/
		bs = gf_bs_new(IV, 17, GF_BITSTREAM_WRITE);
		gf_bs_write_u8(bs, 0);	/*begin of counter*/
		gf_bs_write_data(bs, ch->salt, 8);
		gf_bs_write_u64(bs, (s64) count);
		gf_bs_del(bs);
		gf_crypt_set_state(ch->crypt, IV, 17);

		/*decrypt remain bytes*/
		if (remain) {
			char dummy[20];
			gf_crypt_decrypt(ch->crypt, dummy, remain);
		}
		ch->last_IV = slh->isma_BSO;
	}
	/*decrypt*/
	gf_crypt_decrypt(ch->crypt, data, dataLength);
	ch->last_IV += dataLength;
}

/*handles reception of an SL-PDU, logical or physical*/
void gf_es_receive_sl_packet(GF_ClientService *serv, GF_Channel *ch, char *StreamBuf, u32 StreamLength, SLHeader *header, GF_Err reception_status)
{
	SLHeader hdr;
	u32 nbAU, OldLength, size, AUSeqNum, SLHdrLen;
	Bool EndAU, NewAU;
	char *payload;

	if (ch->skip_sl) {
		Channel_RecieveSkipSL(serv, ch, StreamBuf, StreamLength);
		return;
	}

	/*physical SL-PDU - depacketize*/
	if (!header) {
		if (!StreamLength) return;
		gf_sl_depacketize(ch->esd->slConfig, &hdr, StreamBuf, StreamLength, &SLHdrLen);
		/*FIXME: this assumes the start of the payload is byte-aligned, nothing forces that in systems*/
		StreamLength -= SLHdrLen;
	} else {
		hdr = *header;
		SLHdrLen = 0;
	}
	payload = StreamBuf + SLHdrLen;

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
	} else if (!ch->esd->slConfig->useRandomAccessPointFlag) {
		ch->NeedRap = 0;
	}

	if (ch->esd->slConfig->packetSeqNumLength) {
		if (ch->pck_sn && hdr.packetSequenceNumber) {
			/*repeated -> drop*/
			if (ch->pck_sn == hdr.packetSequenceNumber) return;
			/*if codec has no resiliency check packet drops*/
			if (!ch->codec_resilient && !hdr.accessUnitStartFlag) {
				if (ch->pck_sn == (u32) (1<<ch->esd->slConfig->packetSeqNumLength) ) {
					if (hdr.packetSequenceNumber) {
						Channel_WaitRAP(ch);
						return;
					}
				} else if (ch->pck_sn + 1 != hdr.packetSequenceNumber) {
					Channel_WaitRAP(ch);
					return;
				}
			}
		}
		ch->pck_sn = hdr.packetSequenceNumber;
	}

	/*if idle or empty, skip the packet*/
	if (hdr.idleFlag || (hdr.paddingFlag && !hdr.paddingBits)) return;


	NewAU = 0;
	if (hdr.accessUnitStartFlag) {
		NewAU = 1;
		ch->NextIsAUStart = 0;

		/*if we have a pending AU, add it*/
		if (ch->buffer) {
			fprintf(stdout, "MISSED END OF AU\n");
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
			if (!ch->IsClockInit && (ch->net_dts < ch->seed_ts)) ch->seed_ts = ch->net_dts;

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

		/*if channel owns the clock, start it*/
		if (!ch->IsClockInit) {
			if (Channel_OwnsClock(ch)) {
				gf_clock_set_time(ch->clock, ch->DTS);
				ch->IsClockInit = 1;
			}
			if (ch->clock->clock_init) ch->IsClockInit = 1;
		}
		/*if the AU Length is carried in SL, get its size*/
		if (ch->esd->slConfig->AULength > 0) {
			ch->AULength = hdr.accessUnitLength;
		} else {
			ch->AULength = 0;
		}

		/*skip the AU if we're waiting for a RAP.*/
		if (ch->NeedRap && !hdr.randomAccessPointFlag) {
			return;
		}

		/*TO DO - caroussel for repeated AUs*/
		if (ch->esd->slConfig->AUSeqNumLength) {
			if (AUSeqNum == ch->au_sn) return;
			ch->au_sn = AUSeqNum;
		}
	}
	/*update the RAP marker on a packet base (to cope with AVC/H264 NALU->AU reconstruction)*/
	if (hdr.randomAccessPointFlag) ch->IsRap = 1;

	/*check OCR*/
	if (hdr.OCRflag) {
		s64 OCR_TS = (s64) (((s64) hdr.objectClockReference) * ch->ocr_scale);
		gf_clock_set_time(ch->clock, (u32) OCR_TS);
		ch->IsClockInit = 1;
	}

	/*get AU end state*/	
	OldLength = ch->buffer ? ch->len : 0;
	EndAU = hdr.accessUnitEndFlag;
	if (ch->AULength == OldLength + StreamLength) EndAU = 1;
	if (EndAU) ch->NextIsAUStart = 1;

	if (!StreamLength && EndAU && ch->buffer) {
		Channel_DispatchAU(ch, 0);
		return;
	}
	if (!StreamLength) return;

	/*missed begining, unusable*/
	if (!ch->buffer && !NewAU) {
		fprintf(stdout, "MISSED BEGIN OF AU\n");
		return;
	}

	/*Write the Packet payload to the buffer*/
	if (NewAU) {
		/*we should NEVER have a bitstream at this stage*/
		assert(!ch->buffer);
		/*ignore length fields*/
		size = StreamLength + ch->media_padding_bytes;
		ch->buffer = malloc(sizeof(char) * size);
		if (!ch->buffer) return;

		ch->allocSize = size;
		memset(ch->buffer, 0, sizeof(char) * size);
		ch->len = 0;
	}
	if (!ch->esd->slConfig->usePaddingFlag) hdr.paddingFlag = 0;

	/*if no bitstream, we missed the AU Start packet. Unusable ...*/
	if (!ch->buffer) return;
	
	if (ch->crypt) {
		if (hdr.isma_encrypted) Channel_DecryptISMA(ch, payload, StreamLength, &hdr);
		else ch->last_IV = 0;
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
			ch->buffer = realloc(ch->buffer, sizeof(char) * size);
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
	if (ch->BufferOn) {
		ch->BufferOn = 0;
		gf_clock_buffer_off(ch->clock);
	}
	ch->clock->has_seen_eos = 1;
	gf_odm_on_eos(ch->odm, ch);
}



LPAUBUFFER gf_es_get_au(GF_Channel *ch)
{
	Bool comp, is_new_data;
	GF_Err e, state;
	SLHeader slh;

	if (ch->es_state != GF_ESM_ES_RUNNING) return NULL;

	if (!ch->is_pulling) {
		/*we must update buffering before fetching in order to stop buffering for streams with very few
		updates (especially streams with one update, like most of OD streams)*/
		if (ch->BufferOn) Channel_UpdateBuffering(ch, 0);
		if (ch->first_au_fetched && ch->BufferOn) return NULL;
		return ch->AU_buffer_first;
	}

	/*pull from stream - resume clock if needed*/
	if (ch->BufferOn) {
		ch->BufferOn = 0;
		gf_clock_buffer_off(ch->clock);
	}

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
		if (ch->crypt) {
			if (slh.isma_encrypted) Channel_DecryptISMA(ch, ch->AU_buffer_pull->data, ch->AU_buffer_pull->dataLength, &slh);
			/*otherwise RESET IV to force resync on next AU*/
			else ch->last_IV = 0;
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
	SLHeader slh;
	Bool comp, is_new_data;
	GF_Err e, state;
	if (!ch->is_pulling) return;
	/*pull from stream - resume clock if needed*/
	if (ch->BufferOn) {
		ch->BufferOn = 0;
		gf_clock_buffer_off(ch->clock);
	}
	e = gf_term_channel_get_sl_packet(ch->service, ch, (char **) &ch->AU_buffer_pull->data, &ch->AU_buffer_pull->dataLength, &slh, &comp, &state, &is_new_data);
	if (e) state = e;
	if ((state==GF_OK) && is_new_data) gf_es_receive_sl_packet(ch->service, ch, NULL, 0, &slh, GF_OK);
	gf_term_channel_release_sl_packet(ch->service, ch);
}

void gf_es_drop_au(GF_Channel *ch)
{
	LPAUBUFFER au;

	if (ch->is_pulling) {
		gf_term_channel_release_sl_packet(ch->service, ch);
		ch->AU_buffer_pull->data = NULL;
		ch->AU_buffer_pull->dataLength = 0;
		ch->first_au_fetched = 1;
		return;
	}

	if (!ch->AU_buffer_first) return;

	/*lock the channel before touching the queue*/
	gf_es_lock(ch, 1);
	ch->first_au_fetched = 1;

	au = ch->AU_buffer_first;
	ch->AU_buffer_first = au->next;
	au->next = NULL;
	DB_Delete(au);
	ch->AU_Count -= 1;

	if (!ch->AU_Count && ch->AU_buffer_first) {
		ch->AU_buffer_first = NULL;
	}
	if (!ch->AU_buffer_first) ch->AU_buffer_last = NULL;

	Channel_UpdateBufferTime(ch);

	/*if we get under our limit, rebuffer EXCEPT WHEN EOS is signaled*/
	if (!ch->IsEndOfStream && !ch->BufferOn && Channel_NeedsBuffering(ch, 1)) {
		ch->BufferOn = 1;
		gf_clock_buffer_on(ch->clock);
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

/*performs final setup upon connection confirm*/
void gf_es_on_connect(GF_Channel *ch)
{
	Bool can_buffer;
	GF_NetworkCommand com;
	GF_CodecCapability cap;

	/*config channel*/
	com.command_type = GF_NET_CHAN_CONFIG;
	com.base.on_channel = ch;

	com.cfg.priority = ch->esd->streamPriority;
	com.cfg.sync_id = (u32) ch->clock;
	memcpy(&com.cfg.sl_config, ch->esd->slConfig, sizeof(GF_SLConfig));
	com.cfg.frame_duration = 0;

	/*get the frame duration if audio (used by some network stack)*/
	if (ch->odm->codec && (ch->odm->codec->type==GF_STREAM_AUDIO) ) {
		cap.CapCode = GF_CODEC_SAMPLERATE;
		gf_codec_get_capability(ch->odm->codec, &cap);
		com.cfg.sample_rate = cap.cap.valueInt;
		cap.CapCode = GF_CODEC_CU_DURATION;
		gf_codec_get_capability(ch->odm->codec, &cap);
		com.cfg.frame_duration = cap.cap.valueInt;
	} 
	gf_term_service_command(ch->service, &com);

	/*check whether we can work in pull mode or not*/
	can_buffer = 1;
	/*if local interaction streams no buffer nor pull*/
	if ((ch->esd->decoderConfig->streamType == GF_STREAM_INTERACT) && !ch->esd->URLString) can_buffer = 0;

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
		ch->odm->no_time_ctrl = 1;
		gf_odm_refresh_uninteractives(ch->odm);
	}

	/*signal channel state*/
	ch->es_state = GF_ESM_ES_CONNECTED;
	/*signal only once connected to prevent PLAY trigger on connection callback*/
	ch->odm->pending_channels--;

	/*remember channels connected on service*/
	if (ch->esd->URLString) ch->service->nb_ch_users++;

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

	/*get duration*/
	com.command_type = GF_NET_CHAN_DURATION;
	com.base.on_channel = ch;
	if (gf_term_service_command(ch->service, &com) == GF_OK)
		gf_odm_set_duration(ch->odm, ch, (u32) (1000*com.duration.duration));
}

static void KMS_OnData(void *cbck, char *data, u32 size, u32 state, GF_Err e)
{
}

GF_Err Channel_GetGPAC_KMS(GF_Channel *ch, char *kms_url)
{
	GF_Err e;
	FILE *t;
	GF_DownloadSession * sess;
	if (!strnicmp(kms_url, "(ipmp)", 6)) return GF_NOT_SUPPORTED;
	else if (!strnicmp(kms_url, "(uri)", 5)) kms_url += 5;
	else if (!strnicmp(kms_url, "file://", 7)) kms_url += 7;

	e = GF_OK;
	/*try local*/
	t = (strstr(kms_url, "://") == NULL) ? fopen(kms_url, "r") : NULL;
	if (t) {
		fclose(t);
		return gf_ismacryp_gpac_get_info(ch->esd->ESID, kms_url, ch->key, ch->salt);
	}
	/*note that gpac doesn't have TLS support -> not really usefull. As a general remark, ISMACryp
	is supported as a proof of concept, crypto and IPMP being the last priority on gpac...*/
	gf_term_message(ch->odm->term, kms_url, "Fetching ISMACryp key", GF_OK);
	sess = gf_term_download_new(ch->service, kms_url, 0, KMS_OnData, ch);
	if (!sess) goto err_exit;

	while (1) {
		e = gf_dm_sess_get_stats(sess, NULL, NULL, NULL, NULL, NULL, NULL);
		if (e) break;
	}
	if (e!= GF_EOS) goto err_exit;
	e = gf_ismacryp_gpac_get_info(ch->esd->ESID, (char *) gf_dm_sess_get_cache_name(sess), ch->key, ch->salt);

err_exit:
	gf_term_download_del(sess);
	return e;
}

void gf_es_config_ismacryp(GF_Channel *ch, GF_NetComISMACryp *isma_cryp)
{
	char IV[16];
	GF_Err e;

	/*always buffer when fetching keys*/
	gf_clock_buffer_on(ch->clock);

	ch->is_protected = 1;
	e = GF_OK;
	if ((isma_cryp->scheme_version != 1) || (isma_cryp->scheme_type != GF_FOUR_CHAR_INT('i','A','E','C')) ) {
		gf_term_message(ch->odm->term, ch->service->url, "Unknown ISMACryp scheme and version", GF_NOT_SUPPORTED);
		goto exit;
	}
	if (!isma_cryp->kms_uri) {
		gf_term_message(ch->odm->term, ch->service->url, "ISMACryp: Missing URI for KMS", GF_NON_COMPLIANT_BITSTREAM);
		goto exit;
	}
	if (ch->crypt) gf_crypt_close(ch->crypt);
	ch->crypt = gf_crypt_open("AES-128", "CTR");
	if (!ch->crypt) {
		gf_term_message(ch->odm->term, ch->service->url, "ISMACryp: cannot open AES-128 CTR decryption", GF_IO_ERR);
		goto exit;
	}
	/*fetch keys*/
	if (!strnicmp(isma_cryp->kms_uri, "(key)", 5)) {
		char data[100];
		gf_base64_decode((unsigned char *) isma_cryp->kms_uri+5, strlen(isma_cryp->kms_uri)-5, data, 100);
		memcpy(ch->key, data, sizeof(char)*16);
		memcpy(ch->salt, data+16, sizeof(char)*8);
	}
	/*MPEG4-IP KMS*/
	else if (!stricmp(isma_cryp->kms_uri, "AudioKey") || !stricmp(isma_cryp->kms_uri, "VideoKey")) {
		if (!gf_ismacryp_mpeg4ip_get_info(ch->esd->ESID, (char *) isma_cryp->kms_uri, ch->key, ch->salt)) {
			gf_term_message(ch->odm->term, ch->service->url, "ISMACryp: Unable to retrieve keys from ~./kms_data file", GF_BAD_PARAM);
			goto exit;
		}
	}
	/*gpac default scheme is used, fetch file from KMS and load keys*/
	else if (isma_cryp->scheme_uri && !stricmp(isma_cryp->scheme_uri, "urn:gpac:isma:encryption_scheme")) {
		e = Channel_GetGPAC_KMS(ch, (char *) isma_cryp->kms_uri);
		if (e) {
			if (ch->crypt) {
				gf_crypt_close(ch->crypt);
				ch->crypt = NULL;
			}
			gf_term_message(ch->odm->term, isma_cryp->kms_uri, "ISMACryp: cannot load KMS file", e);
			goto exit;
		}
	}

	/*init decrypter*/
	memset(IV, 0, sizeof(char)*16);
	memcpy(IV, ch->salt, sizeof(char)*8);
	e = gf_crypt_init(ch->crypt, ch->key, 16, IV);
	if (e) {
		gf_term_message(ch->odm->term, ch->service->url, "ISMACryp: cannot initialize AES-128 CTR decryption", e);
		goto exit;
	}

exit:
	gf_clock_buffer_off(ch->clock);
	if (e && ch->crypt) {
		gf_crypt_close(ch->crypt);
		ch->crypt = NULL;
	}
}

void gf_es_reinit_clock(GF_Channel *ch, u32 fromTS)
{
	if (Channel_OwnsClock(ch)) gf_clock_set_time(ch->clock, fromTS);
	ch->IsClockInit = 1;
	if (ch->BufferOn) {
		ch->BufferOn = 0;
		gf_clock_buffer_off(ch->clock);
	}
}

