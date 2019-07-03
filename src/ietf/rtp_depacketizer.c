/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / RTP input module
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

#include <gpac/internal/ietf_dev.h>

#ifndef GPAC_DISABLE_STREAMING

#include <gpac/base_coding.h>
#include <gpac/constants.h>
#include <gpac/isomedia.h>
#include <gpac/mpeg4_odf.h>
#include <gpac/avparse.h>

static void gf_rtp_parse_pass_through(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, u8 *payload, u32 size)
{
	if (!rtp) return;

	rtp->sl_hdr.accessUnitStartFlag = 1;
	if (rtp->sl_hdr.compositionTimeStamp != hdr->TimeStamp)
		rtp->sl_hdr.accessUnitStartFlag = 1;

	rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	rtp->sl_hdr.decodingTimeStamp = hdr->TimeStamp;
	rtp->sl_hdr.accessUnitEndFlag = hdr->Marker;
	rtp->sl_hdr.randomAccessPointFlag = 1;
	rtp->on_sl_packet(rtp->udta, payload, size, &rtp->sl_hdr, GF_OK);
}

static void gf_rtp_parse_mpeg4(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, u8 *payload, u32 size)
{
	u32 aux_size, first_idx, au_hdr_size, num_au;
	u64 pay_start, au_size;
	s32 au_idx;
	GF_BitStream *hdr_bs, *aux_bs;

	hdr_bs = gf_bs_new(payload, size, GF_BITSTREAM_READ);
	aux_bs = gf_bs_new(payload, size, GF_BITSTREAM_READ);

//	fprintf(stderr, "parsing packet %d size %d ts %d M %d\n", hdr->SequenceNumber, size, hdr->TimeStamp, hdr->Marker);

	/*global AU header len*/
	au_hdr_size = 0;
	if (rtp->sl_map.auh_first_min_len) {
		au_hdr_size = gf_bs_read_u16(hdr_bs);
		gf_bs_read_u16(aux_bs);
	}

	/*jump to aux section, skip it and get payload start*/
	gf_bs_read_int(aux_bs, au_hdr_size);
	gf_bs_align(aux_bs);
	if (rtp->sl_map.AuxiliaryDataSizeLength) {
		aux_size = gf_bs_read_int(aux_bs, rtp->sl_map.AuxiliaryDataSizeLength);
		gf_bs_read_int(aux_bs, aux_size);
		gf_bs_align(aux_bs);
	}
	pay_start = gf_bs_get_position(aux_bs);
	gf_bs_del(aux_bs);

	first_idx = 0;
	au_idx = 0;

	rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	rtp->sl_hdr.decodingTimeStamp = hdr->TimeStamp;

	num_au = 0;

	rtp->sl_hdr.accessUnitEndFlag = hdr->Marker;
	/*override some defaults for RFC 3016*/
	if (rtp->flags & GF_RTP_NEW_AU) {
		rtp->sl_hdr.accessUnitStartFlag = 1;
	} else {
		rtp->sl_hdr.accessUnitStartFlag = 0;
	}
	rtp->sl_hdr.randomAccessPointFlag = 0;

	while (1) {
		/*get default AU size*/
		au_size = rtp->sl_map.ConstantSize;
		/*not signaled, assume max one AU per packet*/
		if (!au_size) au_size = size - pay_start;

		if ((!num_au && rtp->sl_map.auh_first_min_len) || (num_au && rtp->sl_map.auh_min_len)) {
			/*ISMACryp*/
			if (rtp->flags & GF_RTP_HAS_ISMACRYP) {
				u32 nbbits;
				rtp->sl_hdr.isma_encrypted = 1;
				if (rtp->flags & GF_RTP_ISMA_SEL_ENC) {
					rtp->sl_hdr.isma_encrypted = gf_bs_read_int(hdr_bs, 1);
					gf_bs_read_int(hdr_bs, 7);
					au_hdr_size -= 8;
				}
				/*Note: ISMACryp ALWAYS indicates IV (BSO) and KEYIDX, even when sample is not encrypted.
				This is quite a waste when using selective encryption....*/
				if (!num_au) {
					nbbits = 8*rtp->sl_map.IV_length;
					if (nbbits) {
						rtp->sl_hdr.isma_BSO = gf_bs_read_int(hdr_bs, nbbits);
						au_hdr_size -= nbbits;
					}
				}
				/*NOT SUPPORTED YET*/
				else if (rtp->sl_map.IV_delta_length) {
					nbbits = 8*rtp->sl_map.IV_delta_length;
					if (nbbits) {
						rtp->sl_hdr.isma_BSO += gf_bs_read_int(hdr_bs, nbbits);
						au_hdr_size -= nbbits;
					}
				}
				if (rtp->sl_map.KI_length) {
					/*NOT SUPPORTED YET*/
					if (!num_au || !(rtp->flags & GF_RTP_ISMA_HAS_KEY_IDX) ) {
						nbbits = 8*rtp->sl_map.KI_length;
						if (nbbits) {
							gf_bs_read_int(hdr_bs, nbbits);
							au_hdr_size -= nbbits;
						}
					}
				}
			}

			/*AU size*/
			if (rtp->sl_map.SizeLength) {
				au_size = gf_bs_read_int(hdr_bs, rtp->sl_map.SizeLength);
				if (au_size > size - pay_start) au_size = size - pay_start;
				au_hdr_size -= rtp->sl_map.SizeLength;
			}
			/*AU index*/
			if (! num_au) {
				au_idx = first_idx = gf_bs_read_int(hdr_bs, rtp->sl_map.IndexLength);
				au_hdr_size -= rtp->sl_map.IndexLength;
			} else {
				au_idx += 1 + (u32) gf_bs_read_int(hdr_bs, rtp->sl_map.IndexDeltaLength);
				au_hdr_size -= rtp->sl_map.IndexDeltaLength;
			}
			/*CTS flag*/
			if (rtp->sl_map.CTSDeltaLength) {
				rtp->sl_hdr.compositionTimeStampFlag = gf_bs_read_int(hdr_bs, 1);
				au_hdr_size -= 1;
			} else {
				/*get CTS from IDX*/
				if (rtp->sl_map.ConstantDuration) {
					rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp + (au_idx - first_idx) * rtp->sl_map.ConstantDuration;
				} else {
					rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp + (au_idx - first_idx) * rtp->sl_hdr.au_duration;
				}
			}

			/*CTS in-band*/
			if (rtp->sl_hdr.compositionTimeStampFlag) {
				rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp + (u32) gf_bs_read_int(hdr_bs, rtp->sl_map.CTSDeltaLength);
				au_hdr_size -= rtp->sl_map.CTSDeltaLength;
			}
			/*DTS flag is always present (needed for reconstruction of TSs in case of packet loss)*/
			if (rtp->sl_map.DTSDeltaLength) {
				rtp->sl_hdr.decodingTimeStampFlag = gf_bs_read_int(hdr_bs, 1);
				au_hdr_size -= 1;
			} else {
				/*NO DTS otherwise*/
				rtp->sl_hdr.decodingTimeStampFlag = 0;
			}
			if (rtp->sl_hdr.decodingTimeStampFlag) {
				u32 ts_off = gf_bs_read_int(hdr_bs, rtp->sl_map.DTSDeltaLength);
				/*TODO FIXME may not be true in case of TS wrapping*/
				if (hdr->TimeStamp > ts_off) rtp->sl_hdr.decodingTimeStamp = hdr->TimeStamp - ts_off;
				au_hdr_size -= rtp->sl_map.DTSDeltaLength;
			}
			/*RAP flag*/
			if (rtp->sl_map.RandomAccessIndication) {
				rtp->sl_hdr.randomAccessPointFlag = gf_bs_read_int(hdr_bs, 1);
				au_hdr_size -= 1;
				if (rtp->sl_hdr.randomAccessPointFlag)
					rtp->sl_hdr.randomAccessPointFlag=1;
			} else {
				rtp->sl_hdr.randomAccessPointFlag=1;
			}
			/*stream state - map directly to seqNum*/
			if (rtp->sl_map.StreamStateIndication) {
				rtp->sl_hdr.AU_sequenceNumber = gf_bs_read_int(hdr_bs, rtp->sl_map.StreamStateIndication);
				au_hdr_size -= rtp->sl_map.StreamStateIndication;
			}
		}
		/*no header present, update CTS/DTS - note we're sure there's no interleaving*/
		else {
			if (num_au) {
				rtp->sl_hdr.compositionTimeStamp += rtp->sl_map.ConstantDuration;
				rtp->sl_hdr.decodingTimeStamp += rtp->sl_map.ConstantDuration;
			}
		}
		/*we cannot map RTP SN to SL SN since an RTP packet may carry several SL ones - only inc by 1 seq nums*/
		rtp->sl_hdr.packetSequenceNumber += 1;

		/*force indication of CTS whenever we have a new AU*/

		rtp->sl_hdr.compositionTimeStampFlag = (rtp->flags & GF_RTP_NEW_AU) ? 1 : 0;

		/*locate VOP start code*/
		if (rtp->sl_hdr.accessUnitStartFlag && (rtp->flags & GF_RTP_M4V_CHECK_RAP)) {
			u32 i;
			Bool is_rap = GF_FALSE;
			unsigned char *pay = (unsigned char *) payload + pay_start;
			i=0;
			while (i<au_size-4) {
				if (!pay[i] && !pay[i+1] && (pay[i+2]==1) && (pay[i+3]==0xB6)) {
					is_rap = ((pay[i+4] & 0xC0)==0) ? GF_TRUE : GF_FALSE;
					break;
				}
				i++;
			}
			rtp->sl_hdr.randomAccessPointFlag = is_rap ? 1 : 0;
		}

		rtp->on_sl_packet(rtp->udta, payload + pay_start, (u32) au_size, &rtp->sl_hdr, GF_OK);

		rtp->sl_hdr.compositionTimeStampFlag = 0;

		if (rtp->flags & GF_RTP_HAS_ISMACRYP) rtp->sl_hdr.isma_BSO += au_size;

		if (au_hdr_size < rtp->sl_map.auh_min_len) break;
		pay_start += au_size;
		if (pay_start >= size) break;
		num_au ++;
	}
//	assert(!au_hdr_size);

	if (hdr->Marker)
		rtp->flags |= GF_RTP_NEW_AU;
	else
		rtp->flags &= ~GF_RTP_NEW_AU;

	gf_bs_del(hdr_bs);
}

#ifndef GPAC_DISABLE_AV_PARSERS

static void gf_rtp_parse_mpeg12_audio(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, u8 *payload, u32 size)
{
	u16 offset;
	u32 mp3hdr, ts;
	GF_BitStream *bs;

	rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	rtp->sl_hdr.decodingTimeStamp = hdr->TimeStamp;

	rtp->sl_hdr.accessUnitStartFlag = rtp->sl_hdr.accessUnitEndFlag ? 1 : 0;
	if (rtp->flags & GF_RTP_NEW_AU) rtp->sl_hdr.accessUnitStartFlag = 1;

	/*get frag header*/
	bs = gf_bs_new(payload, size, GF_BITSTREAM_READ);
	gf_bs_read_u16(bs);
	offset = gf_bs_read_u16(bs);
	gf_bs_del(bs);
	payload += 4;
	size -= 4;
	mp3hdr = 0;
	while (1) {

		/*frame start if no offset*/
		rtp->sl_hdr.accessUnitStartFlag = offset ? 0 : 1;
		rtp->sl_hdr.randomAccessPointFlag = 1;

		/*new frame, store size*/
		rtp->sl_hdr.compositionTimeStampFlag = 0;
		if (rtp->sl_hdr.accessUnitStartFlag) {
			mp3hdr = GF_4CC((u8) payload[0], (u8) payload[1], (u8) payload[2], (u8) payload[3]);
			rtp->sl_hdr.accessUnitLength = gf_mp3_frame_size(mp3hdr);
			rtp->sl_hdr.channels = gf_mp3_num_channels(mp3hdr);
			rtp->sl_hdr.samplerate = gf_mp3_sampling_rate(mp3hdr);
			if (rtp->sl_hdr.samplerate) {
				rtp->sl_hdr.au_duration = gf_mp3_window_size(mp3hdr);
				rtp->sl_hdr.au_duration *= rtp->clock_rate;
				rtp->sl_hdr.au_duration /= rtp->sl_hdr.samplerate;
			}
			rtp->sl_hdr.compositionTimeStampFlag = 1;
		}
		if (!rtp->sl_hdr.accessUnitLength) break;
		/*fragmented frame*/
		if (rtp->sl_hdr.accessUnitLength>size) {
			rtp->on_sl_packet(rtp->udta, payload, rtp->sl_hdr.accessUnitLength, &rtp->sl_hdr, GF_OK);
			rtp->sl_hdr.accessUnitLength -= size;
			rtp->sl_hdr.accessUnitStartFlag = rtp->sl_hdr.accessUnitEndFlag = 0;
			return;
		}
		/*complete frame*/
		rtp->sl_hdr.accessUnitEndFlag = 1;
		rtp->on_sl_packet(rtp->udta, payload, rtp->sl_hdr.accessUnitLength, &rtp->sl_hdr, GF_OK);
		payload += rtp->sl_hdr.accessUnitLength;
		size -= rtp->sl_hdr.accessUnitLength;
		rtp->sl_hdr.accessUnitLength = 0;

		/*if fragmented there shall not be other frames in the packet*/
		if (!rtp->sl_hdr.accessUnitStartFlag) return;
		if (!size) break;
		offset = 0;
		/*get ts*/
		ts = gf_mp3_window_size(mp3hdr);
		rtp->sl_hdr.compositionTimeStamp += ts;
		rtp->sl_hdr.decodingTimeStamp += ts;
	}
	rtp->flags |= GF_RTP_NEW_AU;
}

#endif

static void gf_rtp_parse_mpeg12_video(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, u8 *payload, u32 size)
{
	u8 pic_type;

	rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	rtp->sl_hdr.decodingTimeStamp = hdr->TimeStamp;


	pic_type = payload[2] & 0x7;
	payload += 4;
	size -= 4;

	/*missed something*/
	if (rtp->sl_hdr.compositionTimeStamp != hdr->TimeStamp) rtp->flags |= GF_RTP_NEW_AU;

	rtp->sl_hdr.accessUnitStartFlag = (rtp->flags & GF_RTP_NEW_AU) ? 1 : 0;
	rtp->sl_hdr.accessUnitEndFlag = hdr->Marker ? 1 : 0;
	rtp->sl_hdr.randomAccessPointFlag = (pic_type==1) ? 1 : 0;

	if (rtp->sl_hdr.accessUnitStartFlag) {
		rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
		rtp->sl_hdr.compositionTimeStampFlag = 1;
	} else {
		rtp->sl_hdr.compositionTimeStampFlag = 0;
	}
	rtp->on_sl_packet(rtp->udta, payload, size, &rtp->sl_hdr, GF_OK);
	if (hdr->Marker) {
		rtp->flags |= GF_RTP_NEW_AU;
	} else {
		rtp->flags &= ~GF_RTP_NEW_AU;
	}
}

static void gf_rtp_parse_amr(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, u8 *payload, u32 size)
{
	unsigned char c, type;
	char *data;
	/*we support max 30 frames in one RTP packet...*/
	u32 nbFrame, i, frame_size;
	/*not supported yet*/
	if (!(rtp->flags & GF_RTP_AMR_ALIGN) ) return;

	/*process toc and locate start of payload data*/
	nbFrame = 0;
	while (1) {
		c = payload[nbFrame + 1];
		nbFrame++;
		if (!(c & 0x80)) break;
	}
	data = payload + nbFrame + 1;
	rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	/*then each frame*/
	for (i=0; i<nbFrame; i++) {
		c = payload[i + 1];
		type = ((c & 0x78) >> 3);
		if (rtp->payt==GF_RTP_PAYT_AMR) {
			frame_size = (u32)GF_AMR_FRAME_SIZE[type];
		} else {
			frame_size = (u32)GF_AMR_WB_FRAME_SIZE[type];
		}

		rtp->sl_hdr.compositionTimeStampFlag = 1;
		rtp->sl_hdr.accessUnitStartFlag = 1;
		rtp->sl_hdr.accessUnitEndFlag = 0;
		rtp->sl_hdr.randomAccessPointFlag = 1;
		/*send TOC*/
		rtp->on_sl_packet(rtp->udta, &payload[i+1], 1, &rtp->sl_hdr, GF_OK);
		rtp->sl_hdr.packetSequenceNumber ++;
		rtp->sl_hdr.compositionTimeStampFlag = 0;
		rtp->sl_hdr.accessUnitStartFlag = 0;
		rtp->sl_hdr.accessUnitEndFlag = 1;
		/*send payload*/
		rtp->on_sl_packet(rtp->udta, data, frame_size, &rtp->sl_hdr, GF_OK);
		data += frame_size;
		rtp->sl_hdr.compositionTimeStamp += 160;
	}
}


static void gf_rtp_parse_h263(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, u8 *payload, u32 size)
{
	GF_BitStream *bs;
	Bool P_bit, V_bit;
	u32 plen;
	u64 offset;
	char blank[2];

	bs = gf_bs_new(payload, size, GF_BITSTREAM_READ);
	/*reserved*/
	gf_bs_read_int(bs, 5);
	P_bit = (Bool)gf_bs_read_int(bs, 1);
	V_bit = (Bool)gf_bs_read_int(bs, 1);
	plen = gf_bs_read_int(bs, 6);
	/*plen_bits = */gf_bs_read_int(bs, 3);

	/*VRC not supported yet*/
	if (V_bit) {
		gf_bs_read_u8(bs);
	}
	/*extra picture header not supported yet*/
	if (plen) {
		gf_bs_skip_bytes(bs, plen);
	}
	offset = gf_bs_get_position(bs);
	gf_bs_del(bs);

	blank[0] = blank[1] = 0;
	/*start*/
	if (P_bit) {
		rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
		rtp->sl_hdr.compositionTimeStampFlag = 1;
		rtp->sl_hdr.accessUnitStartFlag = 1;
		rtp->sl_hdr.accessUnitEndFlag = 0;

		if (rtp->sl_hdr.accessUnitStartFlag) {
			/*the first 16 bytes are NOT sent on the wire*/
			rtp->sl_hdr.randomAccessPointFlag = (payload[offset+2]&0x02) ? 0 : 1;
		}
		/*send missing start code*/
		rtp->on_sl_packet(rtp->udta, (char *) blank, 2, &rtp->sl_hdr, GF_OK);
		/*send payload*/
		rtp->sl_hdr.compositionTimeStampFlag = 0;
		rtp->sl_hdr.accessUnitStartFlag = 0;
		rtp->sl_hdr.randomAccessPointFlag = 0;

		/*if M bit set, end of frame*/
		rtp->sl_hdr.accessUnitEndFlag = hdr->Marker;
		rtp->on_sl_packet(rtp->udta, payload + offset, (u32) (size - offset), &rtp->sl_hdr, GF_OK);
	} else {
		/*middle/end of frames - if M bit set, end of frame*/
		rtp->sl_hdr.accessUnitEndFlag = hdr->Marker;
		rtp->on_sl_packet(rtp->udta, payload + offset, (u32) (size - offset), &rtp->sl_hdr, GF_OK);
	}
}

static void gf_rtp_ttxt_flush(GF_RTPDepacketizer *rtp, u32 ts)
{
	GF_BitStream *bs;
	u8 *data;
	u32 data_size;
	if (!rtp->inter_bs) return;

	rtp->sl_hdr.compositionTimeStamp = ts;
	rtp->sl_hdr.compositionTimeStampFlag = 1;
	rtp->sl_hdr.accessUnitStartFlag = 1;
	rtp->sl_hdr.accessUnitEndFlag = 0;
	rtp->sl_hdr.randomAccessPointFlag = 1;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_int(bs, rtp->sl_hdr.idleFlag, 1);
	rtp->sl_hdr.idleFlag = 0;
	gf_bs_write_int(bs, 0, 4);
	gf_bs_write_int(bs, 1, 3);
	gf_bs_write_u16(bs, 8 + (u16) gf_bs_get_position(rtp->inter_bs));
	gf_bs_write_u8(bs, rtp->sidx);
	gf_bs_write_u24(bs, rtp->sl_hdr.au_duration);
	gf_bs_write_u16(bs, rtp->txt_len);
	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);

	rtp->on_sl_packet(rtp->udta, data, data_size, &rtp->sl_hdr, GF_OK);
	gf_free(data);
	rtp->sl_hdr.accessUnitStartFlag = 0;
	rtp->sl_hdr.accessUnitEndFlag = 1;
	gf_bs_get_content(rtp->inter_bs, &data, &data_size);
	rtp->on_sl_packet(rtp->udta, data, data_size, &rtp->sl_hdr, GF_OK);
	gf_free(data);

	gf_bs_del(rtp->inter_bs);
	rtp->inter_bs = NULL;
	rtp->nb_txt_frag = rtp->cur_txt_frag = rtp->sidx = rtp->txt_len = rtp->nb_mod_frag = 0;
}

static void gf_rtp_parse_ttxt(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, u8 *payload, u32 size)
{
	Bool is_utf_16;
	u32 type, ttu_len, duration, ts, sidx, txt_size;
	u32 nb_frag, cur_frag;
	u64 pay_start;
	GF_BitStream *bs;

	ts = hdr->TimeStamp;

	bs = gf_bs_new(payload, size, GF_BITSTREAM_READ);
	while (gf_bs_available(bs)) {
		pay_start = gf_bs_get_position(bs);
		is_utf_16 = (Bool)gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 4);
		type = gf_bs_read_int(bs, 3);
		ttu_len = gf_bs_read_u16(bs);
		if (ttu_len<2) break;

		if (type==1) {
			/*flush any existing packet*/
			gf_rtp_ttxt_flush(rtp, (u32) rtp->sl_hdr.compositionTimeStamp);

			/*bad ttu(1)*/
			if (ttu_len<8) break;
			rtp->sl_hdr.compositionTimeStamp = ts;
			rtp->sl_hdr.compositionTimeStampFlag = 1;
			rtp->sl_hdr.accessUnitStartFlag = 1;
			rtp->sl_hdr.accessUnitEndFlag = 1;
			rtp->sl_hdr.randomAccessPointFlag = 1;
			gf_bs_read_u8(bs);
			rtp->sl_hdr.au_duration = gf_bs_read_u24(bs);
			rtp->on_sl_packet(rtp->udta, payload + pay_start, ttu_len + 1, &rtp->sl_hdr, GF_OK);
			gf_bs_skip_bytes(bs, ttu_len - 6);
			ts += rtp->sl_hdr.au_duration;
		}
		/*text segment*/
		else if (type==2) {
			/*TS changed, flush packet*/
			if (rtp->sl_hdr.compositionTimeStamp < ts) {
				gf_rtp_ttxt_flush(rtp, (u32) rtp->sl_hdr.compositionTimeStamp);
			}
			if (ttu_len<9) break;
			rtp->sl_hdr.compositionTimeStamp = ts;
			rtp->sl_hdr.idleFlag = is_utf_16;
			nb_frag = gf_bs_read_int(bs, 4);
			cur_frag = gf_bs_read_int(bs, 4);
			duration = gf_bs_read_u24(bs);
			sidx = gf_bs_read_u8(bs);
			gf_bs_read_u16(bs);/*complete text sample size, ignored*/
			txt_size = size - 10;

			/*init - 3GPP/MPEG-4 splitting is IMHO stupid:
				- nb frag & cur frags are not needed: rtp reordering insures packet are in order, and
			!!!we assume fragments are sent in order!!!
				- any other TTU suffices to indicate end of text string (modifiers or != RTP TS)
				- replacing these 8bits field with a 16 bit absolute character offset would add error recovery
			*/
			if (!rtp->nb_txt_frag) {
				rtp->nb_txt_frag = nb_frag;
				rtp->cur_txt_frag = 0;
				rtp->sidx = sidx;
			}
			/*flush prev if any mismatch*/
			if ((nb_frag != rtp->nb_txt_frag) || (rtp->cur_txt_frag > cur_frag)) {
				gf_rtp_ttxt_flush(rtp, (u32) rtp->sl_hdr.compositionTimeStamp);
				rtp->nb_txt_frag = nb_frag;
				rtp->sidx = sidx;
			}
			if (!rtp->inter_bs) rtp->inter_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

			/*we don't reorder - RTP reordering is done at lower level, if this is out of order too bad*/
			rtp->cur_txt_frag = cur_frag;
			gf_bs_write_data(rtp->inter_bs, payload+10, txt_size);
			gf_bs_skip_bytes(bs, txt_size);

			rtp->sl_hdr.au_duration = duration;
			/*done*/
			if (hdr->Marker) {
				assert(gf_bs_get_position(rtp->inter_bs) < 1<<7);
				rtp->txt_len = (u8) gf_bs_get_position(rtp->inter_bs);
				gf_rtp_ttxt_flush(rtp, ts);
			}
		} else if ((type==3) || (type==4)) {
			if (!rtp->inter_bs) rtp->inter_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			/*first modifier, store effective written text*/
			if (type==3) {
				assert(gf_bs_get_position(rtp->inter_bs) < 1<<7);
				rtp->txt_len = (u8) gf_bs_get_position(rtp->inter_bs);
			}
			if (ttu_len<6) break;

			nb_frag = gf_bs_read_int(bs, 4);
			if (!rtp->nb_mod_frag) rtp->nb_mod_frag = nb_frag;
			else if (rtp->nb_mod_frag != nb_frag) {
				gf_rtp_ttxt_flush(rtp, (u32) rtp->sl_hdr.compositionTimeStamp);
				rtp->nb_mod_frag = nb_frag;
			}
			gf_bs_read_int(bs, 4);  /*cur_frag, ignore*/
			rtp->sl_hdr.au_duration = gf_bs_read_u24(bs);
			gf_bs_write_data(rtp->inter_bs, payload+7, ttu_len-6);
			gf_bs_skip_bytes(bs, ttu_len-6);

			/*done*/
			if (hdr->Marker) gf_rtp_ttxt_flush(rtp, ts);
		}
	}
	gf_bs_del(bs);
}

static void gf_rtp_h264_flush(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, Bool missed_end)
{
	u8 *data;
	u32 data_size, nal_s;
	if (!rtp->inter_bs) return;

	data = NULL;
	data_size = 0;
	gf_bs_get_content(rtp->inter_bs, &data, &data_size);
	gf_bs_del(rtp->inter_bs);
	rtp->inter_bs = NULL;
	nal_s = data_size-4;

	if (rtp->flags & GF_RTP_AVC_USE_ANNEX_B) {
		data[0] = data[1] = data[2] = 0;
		data[3] = 1;
	} else {
		data[0] = nal_s>>24;
		data[1] = nal_s>>16;
		data[2] = nal_s>>8;
		data[3] = nal_s&0xFF;
	}
	/*set F-bit since nal is corrupted*/
//	if (missed_end) data[4] |= 0x80;

	rtp->sl_hdr.accessUnitEndFlag = hdr->Marker;
	rtp->sl_hdr.compositionTimeStampFlag = 1;
	rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	rtp->sl_hdr.decodingTimeStampFlag = 0;

	rtp->on_sl_packet(rtp->udta, data, data_size, &rtp->sl_hdr, GF_OK);
	rtp->sl_hdr.accessUnitStartFlag = 0;
	rtp->sl_hdr.randomAccessPointFlag = 0;
	gf_free(data);
}

void gf_rtp_parse_h264(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, u8 *payload, u32 size)
{
	char nalhdr[4];
	u32 nal_type;
	if (rtp->h264_pck_mode==2) return;

	nal_type = payload[0] & 0x1F;

	/*set start*/
	if (rtp->sl_hdr.compositionTimeStamp != hdr->TimeStamp) {
		rtp->sl_hdr.accessUnitEndFlag = 0;
		rtp->sl_hdr.accessUnitStartFlag = 1;
		rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
		rtp->sl_hdr.compositionTimeStampFlag = 1;
		rtp->sl_hdr.decodingTimeStampFlag = 0;
		rtp->sl_hdr.randomAccessPointFlag = 0;
	}

	/*single NALU*/
	if (nal_type<23) {
		if (nal_type==GF_AVC_NALU_IDR_SLICE) {
			rtp->sl_hdr.randomAccessPointFlag = 1;
			rtp->flags &= ~GF_RTP_AVC_WAIT_RAP;
		}
		else if (rtp->flags & GF_RTP_AVC_WAIT_RAP)
			return;

		rtp->sl_hdr.accessUnitEndFlag = 0;

		if (rtp->flags & GF_RTP_AVC_USE_ANNEX_B) {
			nalhdr[0] = 0;
			nalhdr[1] = 0;
			nalhdr[2] = 0;
			nalhdr[3] = 1;
		} else {
			/*signal NALU size on 4 bytes*/
			nalhdr[0] = size>>24;
			nalhdr[1] = size>>16;
			nalhdr[2] = size>>8;
			nalhdr[3] = size&0xFF;
		}
		rtp->on_sl_packet(rtp->udta, nalhdr, 4, &rtp->sl_hdr, GF_OK);

		rtp->sl_hdr.accessUnitStartFlag = 0;
		rtp->sl_hdr.compositionTimeStampFlag = 1;
		rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
		rtp->sl_hdr.accessUnitEndFlag = hdr->Marker;

		/*send NAL payload*/
		rtp->on_sl_packet(rtp->udta, payload, size, &rtp->sl_hdr, GF_OK);
	}
	/*STAP-A NALU*/
	else if (nal_type==24) {
		u32 offset = 1;
		while (offset<size) {
			Bool send = GF_TRUE;
			u32 nal_size = (u8) payload[offset];
			nal_size<<=8;
			nal_size |= (u8) payload[offset+1];
			offset += 2;
			if ((payload[offset] & 0x1F) == GF_AVC_NALU_IDR_SLICE) {
				rtp->sl_hdr.randomAccessPointFlag = 1;
				rtp->flags &= ~GF_RTP_AVC_WAIT_RAP;
			}
			if (rtp->flags & GF_RTP_AVC_WAIT_RAP) send = GF_FALSE;

			if (send) {
				/*signal NALU size on 4 bytes*/
				if (rtp->flags & GF_RTP_AVC_USE_ANNEX_B) {
					nalhdr[0] = 0;
					nalhdr[1] = 0;
					nalhdr[2] = 0;
					nalhdr[3] = 1;
				} else {
					nalhdr[0] = nal_size>>24;
					nalhdr[1] = nal_size>>16;
					nalhdr[2] = nal_size>>8;
					nalhdr[3] = nal_size&0xFF;
				}
				rtp->on_sl_packet(rtp->udta, nalhdr, 4, &rtp->sl_hdr, GF_OK);
				rtp->sl_hdr.accessUnitStartFlag = 0;
				rtp->sl_hdr.compositionTimeStampFlag = 0;
			}
			rtp->sl_hdr.accessUnitEndFlag = (hdr->Marker && (offset+nal_size==size)) ? 1 : 0;
			if (send) rtp->on_sl_packet(rtp->udta, payload+offset, nal_size, &rtp->sl_hdr, GF_OK);
			offset += nal_size;
		}
	}
	/*FU-A NALU*/
	else if (nal_type==28) {
		Bool is_start = payload[1] & 0x80;
		Bool is_end = payload[1] & 0x40;
		/*flush*/
		if (is_start) gf_rtp_h264_flush(rtp, hdr, GF_TRUE);

		if ((payload[1] & 0x1F) == GF_AVC_NALU_IDR_SLICE) {
			rtp->flags &= ~GF_RTP_AVC_WAIT_RAP;
			rtp->sl_hdr.randomAccessPointFlag = 1;
		} else if (rtp->flags & GF_RTP_AVC_WAIT_RAP)
			return;

		/*setup*/
		if (!rtp->inter_bs) {
			u8 nal_hdr;
			rtp->inter_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			/*copy F and NRI*/
			nal_hdr = payload[0] & 0xE0;
			/*start bit not set, signal corrupted data (we missed start packet)*/
//			if (!is_start) nal_hdr |= 0x80;
			/*copy NALU type*/
			nal_hdr |= (payload[1] & 0x1F);
			/*dummy size field*/
			gf_bs_write_u32(rtp->inter_bs, 0);
			gf_bs_write_u8(rtp->inter_bs, nal_hdr);
		}
		gf_bs_write_data(rtp->inter_bs, payload+2, size-2);
		if (is_end || hdr->Marker) gf_rtp_h264_flush(rtp, hdr, GF_FALSE);
	}
}

#if !defined(GPAC_DISABLE_HEVC) && !defined(GPAC_DISABLE_AV_PARSERS)

static void gf_rtp_hevc_flush(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, Bool missed_end)
{
	u8 *data;
	u32 data_size, nal_s;
	if (!rtp->inter_bs) return;

	data = NULL;
	data_size = 0;
	gf_bs_get_content(rtp->inter_bs, &data, &data_size);
	gf_bs_del(rtp->inter_bs);
	rtp->inter_bs = NULL;
	nal_s = data_size-4;

	data[0] = nal_s>>24;
	data[1] = nal_s>>16;
	data[2] = nal_s>>8;
	data[3] = nal_s&0xFF;
	/*set F-bit since nal is corrupted*/
	if (missed_end) data[4] |= 0x80;

	rtp->sl_hdr.accessUnitEndFlag = hdr->Marker;
	rtp->sl_hdr.compositionTimeStampFlag = 1;
	rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	rtp->sl_hdr.decodingTimeStampFlag = 0;
	rtp->on_sl_packet(rtp->udta, data, data_size, &rtp->sl_hdr, GF_OK);
	rtp->sl_hdr.accessUnitStartFlag = 0;
	rtp->sl_hdr.randomAccessPointFlag = 0;
	gf_free(data);
}

static void gf_rtp_parse_hevc(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, u8 *payload, u32 size)
{
	u32 nal_type;
	char nalu_size[4];

	nal_type = (payload[0] & 0x7E) >> 1;

	/*set start*/
	if (rtp->sl_hdr.compositionTimeStamp != hdr->TimeStamp) {
		rtp->sl_hdr.accessUnitEndFlag = 0;
		rtp->sl_hdr.accessUnitStartFlag = 1;
		rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
		rtp->sl_hdr.compositionTimeStampFlag = 1;
		rtp->sl_hdr.decodingTimeStampFlag = 0;
		rtp->sl_hdr.randomAccessPointFlag = 0;
	}

	/*Single NALU*/
	if (nal_type <= 40) {
		/*FIXME: strict condition for randomAccessPointFlag because of decoder's issue*/
		if ((nal_type==GF_HEVC_NALU_SLICE_IDR_W_DLP) || (nal_type==GF_HEVC_NALU_SLICE_IDR_N_LP)) {
			//if ((nal_type>=GF_HEVC_NALU_SLICE_BLA_W_LP) && (nal_type<=GF_HEVC_NALU_SLICE_CRA)) {
			rtp->sl_hdr.randomAccessPointFlag = 1;
		}

		rtp->sl_hdr.accessUnitEndFlag = 0;

		/*signal NALU size on 4 bytes*/
		nalu_size[0] = size>>24;
		nalu_size[1] = size>>16;
		nalu_size[2] = size>>8;
		nalu_size[3] = size&0xFF;
		rtp->on_sl_packet(rtp->udta, nalu_size, 4, &rtp->sl_hdr, GF_OK);

		rtp->sl_hdr.accessUnitStartFlag = 0;
		rtp->sl_hdr.compositionTimeStampFlag = 1;
		rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
		rtp->sl_hdr.accessUnitEndFlag = hdr->Marker;

		/*send NAL payload*/
		rtp->on_sl_packet(rtp->udta, payload, size, &rtp->sl_hdr, GF_OK);
	}
	/*AP NALU*/
	else if (nal_type == 48) {
		u32 offset = 2;
		while (offset<size) {
			u32 nal_size = (u8) payload[offset];
			nal_size<<=8;
			nal_size |= (u8) payload[offset+1];
			offset += 2;
			nal_type = (payload[offset] & 0x7E) >> 1;
			/*FIXME: strict condition for randomAccessPointFlag because of decoder's issue*/
			if ((nal_type==GF_HEVC_NALU_SLICE_IDR_W_DLP) || (nal_type==GF_HEVC_NALU_SLICE_IDR_N_LP)) {
				//if ((nal_type>=GF_HEVC_NALU_SLICE_BLA_W_LP) && (nal_type<=GF_HEVC_NALU_SLICE_CRA)) {
				rtp->sl_hdr.randomAccessPointFlag = 1;
			}

			/*signal NALU size on 4 bytes*/
			nalu_size[0] = nal_size>>24;
			nalu_size[1] = nal_size>>16;
			nalu_size[2] = nal_size>>8;
			nalu_size[3] = nal_size&0xFF;
			rtp->on_sl_packet(rtp->udta, nalu_size, 4, &rtp->sl_hdr, GF_OK);

			rtp->sl_hdr.accessUnitStartFlag = 0;
			rtp->sl_hdr.compositionTimeStampFlag = 0;
			rtp->sl_hdr.accessUnitEndFlag = (hdr->Marker && (offset+nal_size==size)) ? 1 : 0;
			rtp->on_sl_packet(rtp->udta, payload+offset, nal_size, &rtp->sl_hdr, GF_OK);
			offset += nal_size;
		}
	}
	/*FU NALU*/
	else if (nal_type == 49) {
		Bool is_start = payload[2] & 0x80;
		Bool is_end = payload[2] & 0x40;
		/*flush*/
		if (is_start) gf_rtp_hevc_flush(rtp, hdr, GF_TRUE);

		nal_type = payload[2] & 0x3F;
		/*FIXME: strict condition for randomAccessPointFlag because of decoder's issue*/
		if ((nal_type==GF_HEVC_NALU_SLICE_IDR_W_DLP) || (nal_type==GF_HEVC_NALU_SLICE_IDR_N_LP)) {
			//if ((nal_type>=GF_HEVC_NALU_SLICE_BLA_W_LP) && (nal_type<=GF_HEVC_NALU_SLICE_CRA)) {
			rtp->sl_hdr.randomAccessPointFlag = 1;
		}

		/*setup*/
		if (!rtp->inter_bs) {
			char nal_hdr[2];
			rtp->inter_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			/*coypy F bit highest bit of LayerId*/
			nal_hdr[0] = payload[0] & 0x81;
			/*assign NAL type*/
			nal_hdr[0] |= (payload[2] & 0x3F) << 1;
			/*copy LayerId and TID*/
			nal_hdr[1] = payload[1];
			/*dummy size field*/
			gf_bs_write_u32(rtp->inter_bs, 0);
			gf_bs_write_data(rtp->inter_bs, nal_hdr, 2);
		}
		gf_bs_write_data(rtp->inter_bs, payload+3, size-3);
		if (is_end || hdr->Marker) gf_rtp_hevc_flush(rtp, hdr, GF_FALSE);
	}
}
#endif

#ifndef GPAC_DISABLE_AV_PARSERS

static void gf_rtp_parse_latm(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, u8 *payload, u32 size)
{
	u32 remain, latm_hdr_size, latm_size;

	rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	rtp->sl_hdr.compositionTimeStampFlag = 1;
	rtp->sl_hdr.accessUnitStartFlag = rtp->sl_hdr.accessUnitEndFlag = 1;
	rtp->sl_hdr.randomAccessPointFlag = 1;

	remain = size;
	while (remain) {
		latm_hdr_size = latm_size = 0;
		while (1) {
			u8 c = *payload;
			latm_hdr_size += 1;
			latm_size += c;
			payload ++;
			if (c < 0xFF) break;
		}

		rtp->on_sl_packet(rtp->udta, (char *) payload, latm_size, &rtp->sl_hdr, GF_OK);
		payload += latm_size;
		remain -= (latm_size+latm_hdr_size);
		rtp->sl_hdr.compositionTimeStamp += rtp->sl_hdr.au_duration;
	}
}
#endif

#if GPAC_ENABLE_3GPP_DIMS_RTP
static void gf_rtp_parse_3gpp_dims(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, char *payload, u32 size)
{
	u32 du_size, offset, dsize, hdr_size;
	char *data, dhdr[6];

	u32 frag_state = ((payload[0]>>3) & 0x7);

	rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	rtp->sl_hdr.compositionTimeStampFlag = 1;

	if (rtp->flags & GF_RTP_NEW_AU) {
		rtp->flags &= ~GF_RTP_NEW_AU;
		rtp->sl_hdr.accessUnitStartFlag = 1;
	}
	rtp->sl_hdr.accessUnitEndFlag = 0;
	if (hdr->Marker) rtp->flags |= GF_RTP_NEW_AU;

	rtp->sl_hdr.randomAccessPointFlag = (payload[0] & 0x40);
	rtp->sl_hdr.AU_sequenceNumber = (payload[0] & 0x7);

	offset = 1;
	while (offset < size) {
		switch (frag_state) {
		case 0:
		{
			GF_BitStream *bs;
			bs = gf_bs_new(payload+offset, 2, GF_BITSTREAM_READ);
			du_size = 2 + gf_bs_read_u16(bs);
			gf_bs_del(bs);
			if (hdr->Marker && offset+du_size>=size) {
				rtp->sl_hdr.accessUnitEndFlag = 1;
			}
			rtp->on_sl_packet(rtp->udta, payload + offset, du_size, &rtp->sl_hdr, GF_OK);
			rtp->sl_hdr.accessUnitStartFlag = 0;
			offset += du_size;
		}
		break;
		case 1:
			if (rtp->inter_bs) gf_bs_del(rtp->inter_bs);

			rtp->inter_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_data(rtp->inter_bs, payload+offset, size-offset);
			return;
		case 2:
			if (!rtp->inter_bs) return;
			gf_bs_write_data(rtp->inter_bs, payload+offset, size-offset);
			return;
		case 3:
			if (!rtp->inter_bs) return;
			gf_bs_write_data(rtp->inter_bs, payload+offset, size-offset);
			gf_bs_get_content(rtp->inter_bs, &data, &dsize);
			gf_bs_del(rtp->inter_bs);

			/*send unit header - if dims size is >0xFFFF, use our internal hack for large units*/
			rtp->inter_bs = gf_bs_new(dhdr, 6, GF_BITSTREAM_WRITE);
			if (dsize<=0xFFFF) {
				gf_bs_write_u16(rtp->inter_bs, dsize);
				hdr_size = 2;
			} else {
				gf_bs_write_u16(rtp->inter_bs, 0);
				gf_bs_write_u32(rtp->inter_bs, dsize);
				hdr_size = 6;
			}
			gf_bs_del(rtp->inter_bs);
			rtp->inter_bs = NULL;

			rtp->on_sl_packet(rtp->udta, dhdr, hdr_size, &rtp->sl_hdr, GF_OK);
			rtp->sl_hdr.accessUnitStartFlag = 0;

			rtp->sl_hdr.accessUnitEndFlag = hdr->Marker;
			rtp->on_sl_packet(rtp->udta, data, dsize, &rtp->sl_hdr, GF_OK);
			gf_free(data);
			return;
		}
	}

}
#endif


#ifndef GPAC_DISABLE_AV_PARSERS

static void gf_rtp_parse_ac3(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, u8 *payload, u32 size)
{
	u8 ft;

	rtp->sl_hdr.compositionTimeStampFlag = 1;
	rtp->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	rtp->sl_hdr.randomAccessPointFlag = 1;
	ft = payload[0];
	/*nb_pck = payload[1];*/
	payload += 2;
	size -= 2;

	if (!ft) {
		GF_AC3Header hdr;
		memset(&hdr, 0, sizeof(GF_AC3Header));
		rtp->sl_hdr.accessUnitStartFlag = rtp->sl_hdr.accessUnitEndFlag = 1;
		while (size) {
			u32 offset;
			if (!gf_ac3_parser((u8*)payload, size, &offset, &hdr, GF_FALSE)) {
				return;
			}
			if (offset) {
				if (offset>size) return;
				payload+=offset;
				size-=offset;
			}
			rtp->on_sl_packet(rtp->udta, payload, hdr.framesize, &rtp->sl_hdr, GF_OK);
			if (size < hdr.framesize) return;
			size -= hdr.framesize;
			payload += hdr.framesize;
			rtp->sl_hdr.compositionTimeStamp += 1536;
		}
		rtp->flags |= GF_RTP_NEW_AU;
	} else if (ft==3) {
		rtp->sl_hdr.accessUnitStartFlag = 0;
		rtp->sl_hdr.accessUnitEndFlag = hdr->Marker ? 1 : 0;
		rtp->on_sl_packet(rtp->udta, payload, size, &rtp->sl_hdr, GF_OK);
	} else {
		rtp->sl_hdr.accessUnitStartFlag = 1;
		rtp->sl_hdr.accessUnitEndFlag = 0;
		rtp->on_sl_packet(rtp->udta, payload, size, &rtp->sl_hdr, GF_OK);
	}
}
#endif /*GPAC_DISABLE_AV_PARSERS*/

static u32 gf_rtp_get_payload_type(GF_RTPMap *map, GF_SDPMedia *media)
{
	u32 i, j;

	if (!stricmp(map->payload_name, "MP4V-ES") ) return GF_RTP_PAYT_MPEG4;
	else if (!stricmp(map->payload_name, "mpeg4-generic")) return GF_RTP_PAYT_MPEG4;
	else if (!stricmp(map->payload_name, "enc-mpeg4-generic")) return GF_RTP_PAYT_MPEG4;
	/*optibase mm400 card hack*/
	else if (!stricmp(map->payload_name, "enc-generic-mp4") ) {
		gf_free(map->payload_name);
		map->payload_name = gf_strdup("enc-mpeg4-generic");
		return GF_RTP_PAYT_MPEG4;
	}

	/*LATM: only without multiplexing (not tested but should be straight AUs)*/
	else if (!stricmp(map->payload_name, "MP4A-LATM")) {
		GF_SDP_FMTP *fmtp;
		i=0;
		while ((fmtp = (GF_SDP_FMTP *) gf_list_enum(media->FMTP, &i))) {
			GF_X_Attribute *att;
			if (fmtp->PayloadType != map->PayloadType) continue;
			//this is our payload. check cpresent is 0
			j=0;
			while ((att = (GF_X_Attribute *)gf_list_enum(fmtp->Attributes, &j))) {
				if (!stricmp(att->Name, "cpresent") && atoi(att->Value)) return 0;
			}
		}
		return GF_RTP_PAYT_LATM;
	}
	else if (!stricmp(map->payload_name, "MPA")) return GF_RTP_PAYT_MPEG12_AUDIO;
	else if (!stricmp(map->payload_name, "MPV")) return GF_RTP_PAYT_MPEG12_VIDEO;
	else if (!stricmp(map->payload_name, "H263-1998") || !stricmp(map->payload_name, "H263-2000")) return GF_RTP_PAYT_H263;
	else if (!stricmp(map->payload_name, "AMR")) return GF_RTP_PAYT_AMR;
	else if (!stricmp(map->payload_name, "AMR-WB")) return GF_RTP_PAYT_AMR_WB;
	else if (!stricmp(map->payload_name, "3gpp-tt")) return GF_RTP_PAYT_3GPP_TEXT;
	else if (!stricmp(map->payload_name, "H264")) return GF_RTP_PAYT_H264_AVC;
#if GPAC_ENABLE_3GPP_DIMS_RTP
	else if (!stricmp(map->payload_name, "richmedia+xml")) return GF_RTP_PAYT_3GPP_DIMS;
#endif
	else if (!stricmp(map->payload_name, "ac3")) return GF_RTP_PAYT_AC3;
	else if (!stricmp(map->payload_name, "H264-SVC")) return GF_RTP_PAYT_H264_SVC;
	else if (!stricmp(map->payload_name, "H265")) return GF_RTP_PAYT_HEVC;
	else if (!stricmp(map->payload_name, "H265-SHVC")) return GF_RTP_PAYT_LHVC;
	else return 0;
}


static GF_Err payt_set_param(GF_RTPDepacketizer *rtp, char *param_name, char *param_val)
{
	u32 i, val;
	char valS[3];
	GF_BitStream *bs;

	if (!rtp || !param_name) return GF_BAD_PARAM;

	/*1 - mpeg4-generic / RFC 3016 payload type items*/

	/*PL (not needed when IOD is here)*/
	if (!stricmp(param_name, "Profile-level-id")) {
		if (rtp->payt == GF_RTP_PAYT_H264_AVC || rtp->payt == GF_RTP_PAYT_H264_SVC) {
			sscanf(param_val, "%x", &rtp->sl_map.PL_ID);
		} else {
			rtp->sl_map.PL_ID = atoi(param_val);
		}
	}
	/*decoder specific info (not needed when IOD is here)*/
	else if (!stricmp(param_name, "config")) {
		u32 len = (u32) strlen(param_val);
		//decode the buffer - the string buffer is MSB hexadecimal
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		valS[2] = 0;
		for (i=0; i<len; i+=2) {
			valS[0] = param_val[i];
			valS[1] = param_val[i+1];
			sscanf(valS, "%x", &val);
			gf_bs_write_u8(bs, val);
		}
		if (rtp->sl_map.config) gf_free(rtp->sl_map.config);
		rtp->sl_map.config = NULL;
		gf_bs_get_content(bs, &rtp->sl_map.config, &rtp->sl_map.configSize);
		gf_bs_del(bs);
	}
	/*mpeg4-generic payload type items required*/

	/*constant size (size of all AUs) */
	else if (!stricmp(param_name, "ConstantSize")) {
		rtp->sl_map.ConstantSize = atoi(param_val);
	}
	/*constant size (size of all AUs) */
	else if (!stricmp(param_name, "ConstantDuration")) {
		rtp->sl_map.ConstantDuration = atoi(param_val);
	}
	/*object type indication (not needed when IOD is here)*/
	else if (!stricmp(param_name, "ObjectType")) {
		rtp->sl_map.CodecID = atoi(param_val);
	}
	else if (!stricmp(param_name, "StreamType"))
		rtp->sl_map.StreamType = atoi(param_val);
	else if (!stricmp(param_name, "mode")) {
		strcpy(rtp->sl_map.mode, param_val);
		/*in case no IOD and no streamType/OTI in the file*/
		if (!stricmp(param_val, "AAC-hbr") || !stricmp(param_val, "AAC-lbr") || !stricmp(param_val, "CELP-vbr") || !stricmp(param_val, "CELP-cbr")) {
			rtp->sl_map.StreamType = GF_STREAM_AUDIO;
			rtp->sl_map.CodecID = GF_CODECID_AAC_MPEG4;
		}
		/*in case no IOD and no streamType/OTI in the file*/
		else if (!stricmp(param_val, "avc-video") ) {
			rtp->sl_map.StreamType = GF_STREAM_VISUAL;
			rtp->sl_map.CodecID = GF_CODECID_AVC;
		}
	}

	else if (!stricmp(param_name, "DTSDeltaLength")) rtp->sl_map.DTSDeltaLength = atoi(param_val);
	else if (!stricmp(param_name, "CTSDeltaLength")) rtp->sl_map.CTSDeltaLength = atoi(param_val);
	else if (!stricmp(param_name, "SizeLength")) rtp->sl_map.SizeLength = atoi(param_val);
	else if (!stricmp(param_name, "IndexLength")) rtp->sl_map.IndexLength = atoi(param_val);
	else if (!stricmp(param_name, "IndexDeltaLength")) rtp->sl_map.IndexDeltaLength = atoi(param_val);
	else if (!stricmp(param_name, "RandomAccessIndication")) rtp->sl_map.RandomAccessIndication = atoi(param_val);
	else if (!stricmp(param_name, "StreamStateIndication")) rtp->sl_map.StreamStateIndication = atoi(param_val);
	else if (!stricmp(param_name, "AuxiliaryDataSizeLength")) rtp->sl_map.AuxiliaryDataSizeLength = atoi(param_val);

	/*H264/AVC config - we only handle mode 0 and 1*/
	else if (!stricmp(param_name, "packetization-mode"))
		rtp->h264_pck_mode = 1;
	/*AMR config*/
	else if (!stricmp(param_name, "octet-align")) {
		if (!stricmp(param_val, "1"))
			rtp->flags |= GF_RTP_AMR_ALIGN;
	} /*ISMACryp config*/
	else if (!stricmp(param_name, "ISMACrypCryptoSuite")) {
		if (!stricmp(param_val, "AES_CTR_128"))
			rtp->isma_scheme = GF_ISOM_ISMACRYP_SCHEME;
		else
			rtp->isma_scheme = 0;
	}
	else if (!stricmp(param_name, "ISMACrypSelectiveEncryption")) {
		if (!stricmp(param_val, "1") || !stricmp(param_val, "true"))
			rtp->flags |= GF_RTP_ISMA_SEL_ENC;
		else
			rtp->flags &= ~GF_RTP_ISMA_SEL_ENC;
	}
	else if (!stricmp(param_name, "ISMACrypIVLength"))
		rtp->sl_map.IV_length = atoi(param_val);
	else if (!stricmp(param_name, "ISMACrypDeltaIVLength"))
		rtp->sl_map.IV_delta_length = atoi(param_val);
	else if (!stricmp(param_name, "ISMACrypKeyIndicatorLength"))
		rtp->sl_map.KI_length = atoi(param_val);
	else if (!stricmp(param_name, "ISMACrypKeyIndicatorPerAU")) {
		if (!stricmp(param_val, "1") || !stricmp(param_val, "true"))
			rtp->flags |= GF_RTP_ISMA_HAS_KEY_IDX;
		else
			rtp->flags &= ~GF_RTP_ISMA_HAS_KEY_IDX;
	} else if (!stricmp(param_name, "ISMACrypKey")) {
		rtp->key = gf_strdup(param_val);
	}
	return GF_OK;
}

static GF_Err gf_rtp_payt_setup(GF_RTPDepacketizer *rtp, GF_RTPMap *map, GF_SDPMedia *media)
{
	u32 i, j;
	GF_SDP_FMTP *fmtp;

	/*reset sl map*/
	memset(&rtp->sl_map, 0, sizeof(GP_RTPSLMap));

	if (map && !stricmp(map->payload_name, "enc-mpeg4-generic")) rtp->flags |= GF_RTP_HAS_ISMACRYP;


	/*then process all FMTPs*/
	i=0;
	while ((fmtp = (GF_SDP_FMTP*)gf_list_enum(media->FMTP, &i))) {
		GF_X_Attribute *att;
		//we work with only one PayloadType for now
		if (map && (fmtp->PayloadType != map->PayloadType)) continue;
		j=0;
		while ((att = (GF_X_Attribute *)gf_list_enum(fmtp->Attributes, &j))) {
			payt_set_param(rtp, att->Name, att->Value);
		}
	}
#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
		gf_rtp_parse_pass_through(NULL, NULL, NULL, 0);
	}
#endif

	switch (rtp->payt) {
#ifndef GPAC_DISABLE_AV_PARSERS
	case GF_RTP_PAYT_LATM:
	{
		u32 AudioMuxVersion, AllStreamsSameTime, numSubFrames, numPrograms, numLayers, ch_cfg;
		GF_M4ADecSpecInfo cfg;
		char *latm_dsi = rtp->sl_map.config;
		GF_BitStream *bs = gf_bs_new(latm_dsi, rtp->sl_map.configSize, GF_BITSTREAM_READ);
		AudioMuxVersion = gf_bs_read_int(bs, 1);
		AllStreamsSameTime = gf_bs_read_int(bs, 1);
		numSubFrames = gf_bs_read_int(bs, 6);
		numPrograms = gf_bs_read_int(bs, 4);
		numLayers = gf_bs_read_int(bs, 3);

		if (AudioMuxVersion || !AllStreamsSameTime || numSubFrames || numPrograms || numLayers) {
			gf_bs_del(bs);
			return GF_NOT_SUPPORTED;
		}
		memset(&cfg, 0, sizeof(cfg));
		cfg.base_object_type = gf_bs_read_int(bs, 5);
		cfg.base_sr_index = gf_bs_read_int(bs, 4);
		if (cfg.base_sr_index == 0x0F) {
			cfg.base_sr = gf_bs_read_int(bs, 24);
		} else {
			cfg.base_sr = GF_M4ASampleRates[cfg.base_sr_index];
		}
		ch_cfg = gf_bs_read_int(bs, 4);
		if (cfg.base_object_type==5 || cfg.base_object_type==29) {
			if (cfg.base_object_type==29) {
				cfg.has_ps = 1;
				cfg.nb_chan = 1;
			}
			cfg.has_sbr = 1;
			cfg.sbr_sr_index = gf_bs_read_int(bs, 4);
			if (cfg.sbr_sr_index == 0x0F) {
				cfg.sbr_sr = gf_bs_read_int(bs, 24);
			} else {
				cfg.sbr_sr = GF_M4ASampleRates[cfg.sbr_sr_index];
			}
			cfg.sbr_object_type = gf_bs_read_int(bs, 5);
		}
		gf_bs_del(bs);
		gf_free(rtp->sl_map.config);
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		/*write as regular AAC*/
		gf_bs_write_int(bs, cfg.base_object_type, 5);
		gf_bs_write_int(bs, cfg.base_sr_index, 4);

		gf_bs_write_int(bs, ch_cfg, 4);
		gf_bs_align(bs);
		gf_bs_get_content(bs, &rtp->sl_map.config, &rtp->sl_map.configSize);
		gf_bs_del(bs);
		rtp->sl_map.StreamType = GF_STREAM_AUDIO;
		rtp->sl_map.CodecID = GF_CODECID_AAC_MPEG4;

		/*assign depacketizer*/
		rtp->depacketize = gf_rtp_parse_latm;
	}
	break;
#endif
	case GF_RTP_PAYT_MPEG4:
		/*mark if AU header is present*/
		rtp->sl_map.auh_first_min_len = 0;
		if (rtp->flags & GF_RTP_HAS_ISMACRYP) {
			if (!rtp->isma_scheme) rtp->isma_scheme = GF_ISOM_ISMACRYP_SCHEME;
			if (!rtp->sl_map.IV_length) rtp->sl_map.IV_length = 4;

			if (rtp->flags & GF_RTP_ISMA_SEL_ENC) rtp->sl_map.auh_first_min_len += 8;
			else rtp->sl_map.auh_first_min_len += 8*(rtp->sl_map.IV_length + rtp->sl_map.KI_length);
		}
		rtp->sl_map.auh_first_min_len += rtp->sl_map.CTSDeltaLength;
		rtp->sl_map.auh_first_min_len += rtp->sl_map.DTSDeltaLength;
		rtp->sl_map.auh_first_min_len += rtp->sl_map.SizeLength;
		rtp->sl_map.auh_first_min_len += rtp->sl_map.RandomAccessIndication;
		rtp->sl_map.auh_first_min_len += rtp->sl_map.StreamStateIndication;
		rtp->sl_map.auh_min_len = rtp->sl_map.auh_first_min_len;
		rtp->sl_map.auh_first_min_len += rtp->sl_map.IndexLength;
		rtp->sl_map.auh_min_len += rtp->sl_map.IndexDeltaLength;
		if (!map) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTP] Missing required payload map\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		/*RFC3016 flags*/
		if (!stricmp(map->payload_name, "MP4V-ES")) {
			rtp->sl_map.StreamType = GF_STREAM_VISUAL;
			rtp->sl_map.CodecID = GF_CODECID_MPEG4_PART2;
		}
		else if (!strnicmp(map->payload_name, "AAC", 3)) {
			rtp->sl_map.StreamType = GF_STREAM_AUDIO;
			rtp->sl_map.CodecID = GF_CODECID_AAC_MPEG4;
		}
		else if (!stricmp(map->payload_name, "MP4A-LATM")) {
			rtp->sl_map.StreamType = GF_STREAM_AUDIO;
			rtp->sl_map.CodecID = GF_CODECID_AAC_MPEG4;
		}
		/*MPEG-4 video, check RAPs if not indicated*/
		if ((rtp->sl_map.StreamType == GF_STREAM_VISUAL) && (rtp->sl_map.CodecID == GF_CODECID_MPEG4_PART2) && !rtp->sl_map.RandomAccessIndication) {
			rtp->flags |= GF_RTP_M4V_CHECK_RAP;
		}
#ifndef GPAC_DISABLE_AV_PARSERS
		if ((rtp->sl_map.CodecID == GF_CODECID_AAC_MPEG4) && !rtp->sl_map.config) {
			GF_M4ADecSpecInfo cfg;
			GF_RTPMap*map = (GF_RTPMap*)gf_list_get(media->RTPMaps, 0);

			memset(&cfg, 0, sizeof(GF_M4ADecSpecInfo));
			cfg.audioPL = rtp->sl_map.PL_ID;
			cfg.nb_chan = map->AudioChannels;
			cfg.nb_chan = 1;
			cfg.base_sr = map->ClockRate/2;
			cfg.sbr_sr = map->ClockRate;
			cfg.base_object_type = GF_M4A_AAC_LC;
			cfg.base_object_type = 5;
			cfg.sbr_object_type = GF_M4A_AAC_MAIN;
			gf_m4a_write_config(&cfg, &rtp->sl_map.config, &rtp->sl_map.configSize);
		}
#endif
		/*assign depacketizer*/
		rtp->depacketize = gf_rtp_parse_mpeg4;
		break;
#ifndef GPAC_DISABLE_AV_PARSERS
	case GF_RTP_PAYT_MPEG12_AUDIO:
		rtp->sl_map.StreamType = GF_STREAM_AUDIO;
		rtp->sl_map.CodecID = GF_CODECID_MPEG2_PART3;
		/*assign depacketizer*/
		rtp->depacketize = gf_rtp_parse_mpeg12_audio;
		break;
#endif /*GPAC_DISABLE_AV_PARSERS*/

	case GF_RTP_PAYT_MPEG12_VIDEO:
		/*we signal RAPs*/
		rtp->sl_map.RandomAccessIndication = GF_TRUE;
		rtp->sl_map.StreamType = GF_STREAM_VISUAL;
		/*FIXME: how to differentiate MPEG1 from MPEG2 video before any frame is received??*/
		rtp->sl_map.CodecID = GF_CODECID_MPEG1;
		/*assign depacketizer*/
		rtp->depacketize = gf_rtp_parse_mpeg12_video;
		break;
	case GF_RTP_PAYT_AMR:
	case GF_RTP_PAYT_AMR_WB:
	{
		rtp->sl_map.StreamType = GF_STREAM_AUDIO;
		rtp->sl_map.CodecID = (rtp->payt == GF_RTP_PAYT_AMR) ? GF_CODECID_AMR : GF_CODECID_AMR_WB;
		/*assign depacketizer*/
		rtp->depacketize = gf_rtp_parse_amr;
	}
	break;
	case GF_RTP_PAYT_H263:
	{
		GF_X_Attribute *att;

		j=0;
		while ((att = (GF_X_Attribute *)gf_list_enum(media->Attributes, &j))) {
			if (stricmp(att->Name, "cliprect")) continue;
			/*only get the display area*/
			sscanf(att->Value, "%u,%u,%u,%u", &rtp->y, &rtp->x, &rtp->h, &rtp->w);
		}

		rtp->sl_map.StreamType = GF_STREAM_VISUAL;
		rtp->sl_map.CodecID = GF_CODECID_H263;

		/*we signal RAPs*/
		rtp->sl_map.RandomAccessIndication = GF_TRUE;

		/*assign depacketizer*/
		rtp->depacketize = gf_rtp_parse_h263;
	}
	break;
	case GF_RTP_PAYT_3GPP_TEXT:
	{
		char *tx3g, *a_tx3g;
		GF_BitStream *bs;
		u32 nb_desc;
		GF_SDP_FMTP *fmtp;
		GF_TextConfig tcfg;
		memset(&tcfg, 0, sizeof(GF_TextConfig));
		tcfg.tag = GF_ODF_TEXT_CFG_TAG;
		tcfg.Base3GPPFormat = 0x10;
		tcfg.MPEGExtendedFormat = 0x10;
		tcfg.profileLevel = 0x10;
		if (!map) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTP] Missing required payload map\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		tcfg.timescale = map->ClockRate;
		tcfg.sampleDescriptionFlags = 1;
		tx3g = NULL;

		i=0;
		while ((fmtp = (GF_SDP_FMTP*)gf_list_enum(media->FMTP, &i))) {
			GF_X_Attribute *att;
			if (fmtp->PayloadType != map->PayloadType) continue;
			j=0;
			while ((att = (GF_X_Attribute *)gf_list_enum(fmtp->Attributes, &j))) {

				if (!stricmp(att->Name, "width")) tcfg.text_width = atoi(att->Value);
				else if (!stricmp(att->Name, "height")) tcfg.text_height = atoi(att->Value);
				else if (!stricmp(att->Name, "tx")) tcfg.horiz_offset = atoi(att->Value);
				else if (!stricmp(att->Name, "ty")) tcfg.vert_offset = atoi(att->Value);
				else if (!stricmp(att->Name, "layer")) tcfg.layer = atoi(att->Value);
				else if (!stricmp(att->Name, "max-w")) tcfg.video_width = atoi(att->Value);
				else if (!stricmp(att->Name, "max-h")) tcfg.video_height = atoi(att->Value);
				else if (!stricmp(att->Name, "tx3g")) tx3g = att->Value;
			}
		}
		if (!tx3g) return GF_NON_COMPLIANT_BITSTREAM;

		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_u8(bs, tcfg.Base3GPPFormat);
		gf_bs_write_u8(bs, tcfg.MPEGExtendedFormat); /*MPEGExtendedFormat*/
		gf_bs_write_u8(bs, tcfg.profileLevel); /*profileLevel*/
		gf_bs_write_u24(bs, tcfg.timescale);
		gf_bs_write_int(bs, 0, 1);	/*no alt formats*/
		gf_bs_write_int(bs, tcfg.sampleDescriptionFlags, 2);
		gf_bs_write_int(bs, 1, 1);	/*we will write sample desc*/
		gf_bs_write_int(bs, 1, 1);	/*video info*/
		gf_bs_write_int(bs, 0, 3);	/*reserved, spec doesn't say the values*/
		gf_bs_write_u8(bs, tcfg.layer);
		gf_bs_write_u16(bs, tcfg.text_width);
		gf_bs_write_u16(bs, tcfg.text_height);
		/*get all tx3g (comma separated)*/
		nb_desc = 1;
		a_tx3g = tx3g;
		while ((a_tx3g = strchr(a_tx3g, ',')) ) {
			a_tx3g ++;
			nb_desc ++;
		}
		a_tx3g = tx3g;
		gf_bs_write_u8(bs, nb_desc);
		nb_desc = 1;
		while (1) {
			char *next_tx3g, szOut[1000];
			u32 len, s_len;
			next_tx3g = strchr(a_tx3g, ',');
			if (next_tx3g) s_len = (u32) (next_tx3g - a_tx3g - 1);
			else s_len = (u32) strlen(a_tx3g);

			len = gf_base64_decode(a_tx3g, s_len, szOut, 1000);
			nb_desc++;
			gf_bs_write_data(bs, szOut, len);
			if (!next_tx3g) break;
			a_tx3g = strchr(a_tx3g, ',');
			if (!a_tx3g) break;
			a_tx3g += 1;
			while (a_tx3g[0] == ' ') a_tx3g += 1;
		}

		/*write video cfg*/
		gf_bs_write_u16(bs, tcfg.video_width);
		gf_bs_write_u16(bs, tcfg.video_height);
		gf_bs_write_u16(bs, tcfg.horiz_offset);
		gf_bs_write_u16(bs, tcfg.vert_offset);
		gf_bs_get_content(bs, &rtp->sl_map.config, &rtp->sl_map.configSize);
		rtp->sl_map.StreamType = GF_STREAM_TEXT;
		rtp->sl_map.CodecID = GF_CODECID_TEXT_MPEG4;
		gf_bs_del(bs);
		/*assign depacketizer*/
		rtp->depacketize = gf_rtp_parse_ttxt;
	}
	break;
#ifndef GPAC_DISABLE_AV_PARSERS
	case GF_RTP_PAYT_H264_AVC:
	case GF_RTP_PAYT_H264_SVC:
	{
		GF_SDP_FMTP *fmtp;
		GF_AVCConfig *avcc;
		if (!map) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTP] Missing required payload map\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		avcc = gf_odf_avc_cfg_new();
		avcc->AVCProfileIndication = (rtp->sl_map.PL_ID>>16) & 0xFF;
		avcc->profile_compatibility = (rtp->sl_map.PL_ID>>8) & 0xFF;
		avcc->AVCLevelIndication = rtp->sl_map.PL_ID & 0xFF;
		avcc->configurationVersion = 1;
		avcc->nal_unit_size = 4;
		rtp->sl_map.StreamType = 4;
		rtp->sl_map.CodecID = GF_CODECID_AVC;
		/*we will signal RAPs*/
		rtp->sl_map.RandomAccessIndication = GF_TRUE;
		/*rewrite sps and pps*/
		i=0;
		while ((fmtp = (GF_SDP_FMTP*)gf_list_enum(media->FMTP, &i))) {
			GF_X_Attribute *att;
			if (fmtp->PayloadType != map->PayloadType) continue;
			j=0;
			while ((att = (GF_X_Attribute *)gf_list_enum(fmtp->Attributes, &j))) {
				char *nal_ptr, *sep;
				if (stricmp(att->Name, "sprop-parameter-sets")) continue;

				nal_ptr = att->Value;
				while (nal_ptr) {
					u32 nalt, b64size, ret;
					char *b64_d;

					sep = strchr(nal_ptr, ',');
					if (sep) sep[0] = 0;

					b64size = (u32) strlen(nal_ptr);
					b64_d = (char*)gf_malloc(sizeof(char)*b64size);
					ret = gf_base64_decode(nal_ptr, b64size, b64_d, b64size);
					b64_d[ret] = 0;

					nalt = b64_d[0] & 0x1F;
					if (/*SPS*/(nalt==0x07) || /*PPS*/(nalt==0x08) || /*SSPS*/(nalt==0x0F)) {
						GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_malloc(sizeof(GF_AVCConfigSlot));
						sl->size = ret;
						sl->data = (char*)gf_malloc(sizeof(char)*sl->size);
						memcpy(sl->data, b64_d, sizeof(char)*sl->size);
						if (nalt==0x07 || nalt==0x0F) {
							gf_list_add(avcc->sequenceParameterSets, sl);
						} else {
							gf_list_add(avcc->pictureParameterSets, sl);
						}
					}
					gf_free(b64_d);

					if (sep) {
						sep[0] = ',';
						nal_ptr = sep+1;
					} else {
						break;
					}
				}
			}
		}
		if (gf_list_count(avcc->sequenceParameterSets) && gf_list_count(avcc->pictureParameterSets)) {
			gf_odf_avc_cfg_write(avcc, &rtp->sl_map.config, &rtp->sl_map.configSize);
		} else {
			rtp->flags |= GF_RTP_AVC_USE_ANNEX_B;
		}
		gf_odf_avc_cfg_del(avcc);
	}
		/*assign depacketizer*/
	rtp->depacketize = gf_rtp_parse_h264;
	break;
	case GF_RTP_PAYT_HEVC:
	case GF_RTP_PAYT_LHVC:
#ifndef GPAC_DISABLE_HEVC
	{
		GF_SDP_FMTP *fmtp;
		GF_HEVCConfig *hevcc;
		if (!map) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTP] Missing required payload map\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		hevcc = gf_odf_hevc_cfg_new();
		hevcc->configurationVersion = 1;
		hevcc->nal_unit_size = 4;
		rtp->sl_map.StreamType = 4;
		rtp->sl_map.CodecID = GF_CODECID_HEVC;
		/*we will signal RAPs*/
		rtp->sl_map.RandomAccessIndication = GF_TRUE;
		i=0;
		while ((fmtp = (GF_SDP_FMTP*)gf_list_enum(media->FMTP, &i))) {
			GF_X_Attribute *att;
			if (fmtp->PayloadType != map->PayloadType) continue;
			j=0;
			while ((att = (GF_X_Attribute *)gf_list_enum(fmtp->Attributes, &j))) {
				char *nal_ptr, *sep;
				GF_HEVCParamArray *ar;
				if (!stricmp(att->Name, "sprop-vps")) {
					GF_SAFEALLOC(ar, GF_HEVCParamArray);
					if (!ar) return GF_OUT_OF_MEM;
					ar->nalus = gf_list_new();
					ar->type = GF_HEVC_NALU_VID_PARAM;
				}
				else if (!stricmp(att->Name, "sprop-sps")) {
					GF_SAFEALLOC(ar, GF_HEVCParamArray);
					if (!ar) return GF_OUT_OF_MEM;
					ar->nalus = gf_list_new();
					ar->type = GF_HEVC_NALU_SEQ_PARAM;
				}
				else if (!stricmp(att->Name, "sprop-pps")) {
					GF_SAFEALLOC(ar, GF_HEVCParamArray);
					if (!ar) return GF_OUT_OF_MEM;
					ar->nalus = gf_list_new();
					ar->type = GF_HEVC_NALU_PIC_PARAM;
				}
				else
					continue;
				nal_ptr = att->Value;
				while (nal_ptr) {
					u32 b64size, ret;
					char *b64_d;
					GF_AVCConfigSlot *sl;

					sep = strchr(nal_ptr, ',');
					if (sep) sep[0] = 0;

					b64size = (u32) strlen(nal_ptr);
					b64_d = (char*)gf_malloc(sizeof(char)*b64size);
					ret = gf_base64_decode(nal_ptr, b64size, b64_d, b64size);
					b64_d[ret] = 0;

					sl = (GF_AVCConfigSlot *)gf_malloc(sizeof(GF_AVCConfigSlot));
					sl->size = ret;
					sl->data = (char*)gf_malloc(sizeof(char)*sl->size);
					memcpy(sl->data, b64_d, sizeof(char)*sl->size);
					gf_list_add(ar->nalus, sl);

					gf_free(b64_d);

					if (sep) {
						sep[0] = ',';
						nal_ptr = sep+1;
					} else {
						break;
					}
				}
				if (!hevcc->param_array) hevcc->param_array = gf_list_new();
				gf_list_add(hevcc->param_array, ar);
			}
		}
		gf_odf_hevc_cfg_write(hevcc, &rtp->sl_map.config, &rtp->sl_map.configSize);
		gf_odf_hevc_cfg_del(hevcc);
	}
	rtp->depacketize = gf_rtp_parse_hevc;
#else
	return GF_NOT_SUPPORTED;
#endif
	break;
#endif /*GPAC_DISABLE_AV_PARSERS*/

#if GPAC_ENABLE_3GPP_DIMS_RTP
	/*todo - rewrite DIMS config*/
	case GF_RTP_PAYT_3GPP_DIMS:
		rtp->sl_map.StreamType = GF_STREAM_SCENE;
		rtp->sl_map.CodecID = GF_CODECID_DIMS;
		/*we will signal RAPs*/
		rtp->sl_map.RandomAccessIndication = GF_TRUE;
		/*we map DIMS CTR to AU seq num, hence 3 bits*/
		rtp->sl_map.StreamStateIndication = 3;
		rtp->sl_map.IndexLength = 3;
		/*assign depacketizer*/
		rtp->depacketize = gf_rtp_parse_3gpp_dims;
		break;
#endif

#ifndef GPAC_DISABLE_AV_PARSERS
	case GF_RTP_PAYT_AC3:
		rtp->sl_map.StreamType = GF_STREAM_AUDIO;
		rtp->sl_map.CodecID = GF_CODECID_AC3;
		rtp->sl_map.RandomAccessIndication = GF_TRUE;
		/*assign depacketizer*/
		rtp->depacketize = gf_rtp_parse_ac3;
		break;
#endif /*GPAC_DISABLE_AV_PARSERS*/
	default:
		if (rtp->payt >= GF_RTP_PAYT_LAST_STATIC_DEFINED)
			return GF_NOT_SUPPORTED;
		rtp->depacketize = gf_rtp_parse_pass_through;
		return GF_OK;
	}
	return GF_OK;
}

const GF_RTPStaticMap static_payloads [] =
{
	{ GF_RTP_PAYT_PCMU, 8000, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_GSM, 8000, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_G723, 8000, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_DVI4_8K, 8000, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_DVI4_16K, 16000, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_LPC, 8000, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_PCMA, 8000, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_G722, 8000, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_L16_STEREO, 44100, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_L16_MONO, 44100, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_QCELP_BASIC, 8000, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_CN, 8000, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_MPEG12_AUDIO, 90000, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_G728, 8000, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_DVI4_11K, 11025, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_DVI4_22K, 22050, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_G729, 8000, GF_STREAM_AUDIO},
	{ GF_RTP_PAYT_CelB, 90000, GF_STREAM_VISUAL},
	{ GF_RTP_PAYT_JPEG, 90000, GF_STREAM_VISUAL},
	{ GF_RTP_PAYT_nv, 90000, GF_STREAM_VISUAL},
	{ GF_RTP_PAYT_H261, 90000, GF_STREAM_VISUAL},
	{ GF_RTP_PAYT_MPEG12_VIDEO, 90000, GF_STREAM_VISUAL},
	{ GF_RTP_PAYT_MP2T, 90000, GF_STREAM_FILE, 0, "video/mp2t"},
	{ GF_RTP_PAYT_H263, 90000, GF_STREAM_VISUAL}
};

static const GF_RTPStaticMap *gf_rtp_is_valid_static_payt(u32 payt)
{
	u32 i, count = sizeof(static_payloads) / sizeof(struct rtp_static_payt);
	if (payt>= GF_RTP_PAYT_LAST_STATIC_DEFINED) return NULL;
	for (i=0; i<count; i++) {
		if (static_payloads[i].fmt == payt) {
			return &static_payloads[i];
		}
	}
	return NULL;
}


GF_EXPORT
GF_RTPDepacketizer *gf_rtp_depacketizer_new(GF_SDPMedia *media, void (*sl_packet_cbk)(void *udta, u8 *payload, u32 size, GF_SLHeader *hdr, GF_Err e), void *udta)
{
	GF_Err e;
	GF_RTPMap *map;
	u32 payt;
	GF_RTPDepacketizer *tmp;
	u32 clock_rate;
	const GF_RTPStaticMap *static_map = NULL;

	/*check RTP map. For now we only support 1 RTPMap*/
	if (!sl_packet_cbk || !media || (gf_list_count(media->RTPMaps) > 1)) return NULL;

	/*check payload type*/
	map = (GF_RTPMap *)gf_list_get(media->RTPMaps, 0);
	if (!map) {

		//we deal with at most one format
		if (!media->fmt_list || strchr(media->fmt_list, ' ')) return NULL;

		payt = atoi(media->fmt_list);
		static_map = gf_rtp_is_valid_static_payt(payt);
		if (!static_map) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("RTP: Invalid static payload type %d\n", payt));
			return NULL;
		}
		clock_rate = static_map->clock_rate;
	} else {
		payt = gf_rtp_get_payload_type(map, media);
		if (!payt) return NULL;
		clock_rate = map->ClockRate;
	}

	GF_SAFEALLOC(tmp, GF_RTPDepacketizer);
	if (!tmp) return NULL;
	tmp->payt = payt;
	tmp->static_map = static_map;

	e = gf_rtp_payt_setup(tmp, map, media);
	if (e) {
		gf_free(tmp);
		return NULL;
	}
	assert(tmp->depacketize);
	tmp->clock_rate = clock_rate;
	tmp->on_sl_packet = sl_packet_cbk;
	tmp->udta = udta;
	return tmp;
}

GF_EXPORT
void gf_rtp_depacketizer_reset(GF_RTPDepacketizer *rtp, Bool full_reset)
{
	if (rtp) {
		if (rtp->inter_bs) gf_bs_del(rtp->inter_bs);
		rtp->inter_bs = NULL;
		rtp->flags |= GF_RTP_NEW_AU;
		if (full_reset) {
			u32 dur = rtp->sl_hdr.au_duration;
			memset(&rtp->sl_hdr, 0, sizeof(GF_SLHeader));
			rtp->sl_hdr.au_duration = dur;
		}
	}
}

GF_EXPORT
void gf_rtp_depacketizer_del(GF_RTPDepacketizer *rtp)
{
	if (rtp) {
		gf_rtp_depacketizer_reset(rtp, GF_FALSE);
		if (rtp->sl_map.config) gf_free(rtp->sl_map.config);
		if (rtp->key) gf_free(rtp->key);
		gf_free(rtp);
	}
}

GF_EXPORT
void gf_rtp_depacketizer_process(GF_RTPDepacketizer *rtp, GF_RTPHeader *hdr, u8 *payload, u32 size)
{
	assert(rtp && rtp->depacketize);
	rtp->sl_hdr.sender_ntp = hdr->recomputed_ntp_ts;
	rtp->depacketize(rtp, hdr, payload, size);
}


#if 0 //unused
void gf_rtp_depacketizer_get_slconfig(GF_RTPDepacketizer *rtp, GF_SLConfig *slc)
{
	memset(slc, 0, sizeof(GF_SLConfig));
	slc->tag = GF_ODF_SLC_TAG;

	slc->AULength = rtp->sl_map.ConstantSize;
	if (rtp->sl_map.ConstantDuration) {
		slc->CUDuration = slc->AUDuration = rtp->sl_map.ConstantDuration;
	} else {
		slc->CUDuration = slc->AUDuration = rtp->sl_hdr.au_duration;
	}
	/*AUSeqNum is only signaled if streamState is used (eg for carrouselling); otherwise we ignore it*/
	slc->AUSeqNumLength = rtp->sl_map.StreamStateIndication;
	slc->no_dts_signaling = rtp->sl_map.DTSDeltaLength ? GF_FALSE : GF_TRUE;


	/*RTP SN is on 16 bits*/
	slc->packetSeqNumLength = 0;
	/*RTP TS is on 32 bits*/
	slc->timestampLength = 32;
	slc->timeScale = slc->timestampResolution = rtp->clock_rate;
	slc->useTimestampsFlag = 1;

	/*we override these flags because we emulate the flags through the marker bit */
	slc->useAccessUnitEndFlag = slc->useAccessUnitStartFlag = 1;
	slc->useRandomAccessPointFlag = rtp->sl_map.RandomAccessIndication;
	/*checking RAP for video*/
	if (rtp->flags & GF_RTP_M4V_CHECK_RAP) {
		slc->useRandomAccessPointFlag = 1;
		slc->hasRandomAccessUnitsOnlyFlag = 0;
	}

	/*try to signal caroussel if not set in StreamState*/
	if (!slc->AUSeqNumLength && rtp->sl_map.RandomAccessIndication) {
		switch (rtp->sl_map.StreamType) {
		case GF_STREAM_OD:
		case GF_STREAM_SCENE:
			slc->AUSeqNumLength = rtp->sl_map.IndexLength;
			break;
		}
	}
}
#endif


#endif /*GPAC_DISABLE_STREAMING*/
