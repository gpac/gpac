/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / IETF RTP/RTSP/SDP sub-project
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

#include <gpac/constants.h>

static void rtp_amr_flush(GP_RTPPacketizer *builder)
{
	u8 *hdr;
	u32 hdr_size;
	if (!builder->bytesInPacket) return;
	gf_bs_get_content(builder->pck_hdr, &hdr, &hdr_size);
	gf_bs_del(builder->pck_hdr);
	builder->pck_hdr = NULL;
	/*overwrite last frame F bit*/
	hdr[builder->last_au_sn] &= 0x7F;
	builder->OnData(builder->cbk_obj, hdr, hdr_size, GF_TRUE);
	gf_free(hdr);
	builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
	builder->bytesInPacket = 0;
	builder->last_au_sn = 0;
}

GF_Err gp_rtp_builder_do_amr(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize)
{
	u32 offset, rtp_ts, block_size;

	if (!data) {
		rtp_amr_flush(builder);
		return GF_OK;
	}

	rtp_ts = (u32) builder->sl_header.compositionTimeStamp;

	offset = 0;
	while (data_size>offset) {
		u8 ft = (data[offset] & 0x78) >> 3;
		u8 size;

		if (builder->rtp_payt == GF_RTP_PAYT_AMR_WB) {
			size = (u32)GF_AMR_WB_FRAME_SIZE[ft];
			block_size = 320;
		} else {
			size = (u32)GF_AMR_FRAME_SIZE[ft];
			block_size = 160;
		}

		/*packet full or too long*/
		if (builder->bytesInPacket + 1 + size > builder->Path_MTU)
			rtp_amr_flush(builder);

		/*need new*/
		if (!builder->bytesInPacket) {
			builder->rtp_header.TimeStamp = rtp_ts;
			builder->rtp_header.Marker = 0;	/*never set*/
			builder->rtp_header.SequenceNumber += 1;
			builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
			assert(builder->pck_hdr==NULL);

			/*always have header and TOC*/
			builder->pck_hdr = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			/*CMR + res (all 0, no interleaving)*/
			gf_bs_write_int(builder->pck_hdr, ft, 4);
			gf_bs_write_int(builder->pck_hdr, 0, 4);
			builder->bytesInPacket = 1;
			/*no interleaving*/
		}

		/*F always to 1*/
		gf_bs_write_int(builder->pck_hdr, 1, 1);
		gf_bs_write_int(builder->pck_hdr, ft, 4);
		/*Q*/
		gf_bs_write_int(builder->pck_hdr, (data[offset] & 0x4) ? 1 : 0, 1);
		gf_bs_write_int(builder->pck_hdr, 0, 2);
		builder->bytesInPacket ++;

		/*remove frame type byte*/
		offset++;

		/*add frame data without rate_type byte header*/
		if (builder->OnDataReference) {
			builder->OnDataReference(builder->cbk_obj, size, offset);
		} else {
			builder->OnData(builder->cbk_obj, data+offset, size, GF_FALSE);
		}
		builder->last_au_sn++;
		builder->bytesInPacket += size;
		offset += size;
		rtp_ts += block_size;
		assert(builder->bytesInPacket<=builder->Path_MTU);
		/*take care of aggregation, flush if needed*/
		if (builder->last_au_sn==builder->auh_size) rtp_amr_flush(builder);
	}
	return GF_OK;
}

static GFINLINE u8 qes_get_rate_size(u32 idx, const unsigned int *rates, const unsigned int nb_rates)
{
	u32 i;
	for (i=0; i<nb_rates; i++) {
		if (rates[2*i]==idx) return rates[2*i+1];
	}
	return 0;
}

GF_Err gp_rtp_builder_do_qcelp(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize)
{
	u32 offset, rtp_ts;
	u8 hdr;

	if (!data) {
		if (builder->bytesInPacket) builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
		builder->bytesInPacket = 0;
		builder->last_au_sn = 0;
		return GF_OK;
	}

	rtp_ts = (u32) builder->sl_header.compositionTimeStamp;


	offset = 0;
	while (data_size>offset) {
		u8 frame_type = data[offset];
		u8 size = qes_get_rate_size(frame_type, GF_QCELP_RATE_TO_SIZE, GF_QCELP_RATE_TO_SIZE_NB);
		/*reserved, not sent)*/
		if (frame_type>=5) {
			offset += size;
			continue;
		}
		/*packet full or too long*/
		if (builder->bytesInPacket + size > builder->Path_MTU) {
			builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
			builder->bytesInPacket = 0;
			builder->last_au_sn = 0;
		}

		/*need new*/
		if (!builder->bytesInPacket) {
			builder->rtp_header.TimeStamp = rtp_ts;
			builder->rtp_header.Marker = 0;	/*never set*/
			builder->rtp_header.SequenceNumber += 1;
			builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
			hdr = 0;/*no interleaving*/
			builder->OnData(builder->cbk_obj, (char*)&hdr, 1, GF_FALSE);
			builder->bytesInPacket = 1;
		}
		if (builder->OnDataReference) {
			builder->OnDataReference(builder->cbk_obj, size, offset);
		} else {
			builder->OnData(builder->cbk_obj, data+offset, size, GF_FALSE);
		}
		builder->bytesInPacket += size;
		offset += size;
		rtp_ts += 160;
		assert(builder->bytesInPacket<=builder->Path_MTU);

		/*take care of aggregation, flush if needed*/
		builder->last_au_sn++;
		if (builder->last_au_sn==builder->auh_size) {
			builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
			builder->bytesInPacket = 0;
			builder->last_au_sn = 0;
		}
	}
	return GF_OK;
}


static void rtp_evrc_smv_flush(GP_RTPPacketizer *builder)
{
	if (!builder->bytesInPacket) return;
	if (builder->auh_size>1) {
		u8 *hdr;
		u32 hdr_size;
		/*padding*/
		if (builder->last_au_sn % 2) gf_bs_write_int(builder->pck_hdr, 0, 4);
		gf_bs_get_content(builder->pck_hdr, &hdr, &hdr_size);
		gf_bs_del(builder->pck_hdr);
		builder->pck_hdr = NULL;
		/*overwrite count*/
		hdr[0] = 0;
		hdr[1] = builder->last_au_sn-1;/*MMM + frameCount-1*/
		builder->OnData(builder->cbk_obj, hdr, hdr_size, GF_TRUE);
		gf_free(hdr);
	}
	builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
	builder->bytesInPacket = 0;
	builder->last_au_sn = 0;
}

GF_Err gp_rtp_builder_do_smv(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize)
{
	u32 offset, rtp_ts;

	if (!data) {
		rtp_evrc_smv_flush(builder);
		return GF_OK;
	}

	rtp_ts = (u32) builder->sl_header.compositionTimeStamp;

	offset = 0;
	while (data_size>offset) {
		u8 frame_type = data[offset];
		u8 size = qes_get_rate_size(frame_type, GF_SMV_EVRC_RATE_TO_SIZE, GF_SMV_EVRC_RATE_TO_SIZE_NB);

		/*reserved, not sent)*/
		if (frame_type>=5) {
			offset += size;
			continue;
		}
		/*packet full or too long*/
		if (builder->bytesInPacket + size > builder->Path_MTU)
			rtp_evrc_smv_flush(builder);

		/*need new*/
		if (!builder->bytesInPacket) {
			builder->rtp_header.TimeStamp = rtp_ts;
			builder->rtp_header.Marker = 0;	/*never set*/
			builder->rtp_header.SequenceNumber += 1;
			builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
			assert(builder->pck_hdr==NULL);

			if (builder->auh_size>1) {
				builder->pck_hdr = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
				/*RRLLLNNN (all 0, no interleaving)*/
				gf_bs_write_u8(builder->pck_hdr, 0);
				/*MMM + count-1 : overriden when flushing*/
				gf_bs_write_u8(builder->pck_hdr, 0);
				builder->bytesInPacket = 2;
			}
		}

		/*bundle mode: cat rate byte to TOC, on 4 bits*/
		if (builder->auh_size>1) {
			gf_bs_write_int(builder->pck_hdr, data[offset], 4);
			if (!(builder->last_au_sn % 2)) builder->bytesInPacket += 1;
		}
		/*note that EVEN in header-free format the rate_type byte is removed*/
		offset++;
		size--;

		/*add frame data without rate_type byte header*/
		if (builder->OnDataReference) {
			builder->OnDataReference(builder->cbk_obj, size, offset);
		} else {
			builder->OnData(builder->cbk_obj, data+offset, size, GF_FALSE);
		}
		builder->last_au_sn++;
		builder->bytesInPacket += size;
		offset += size;
		rtp_ts += 160;
		assert(builder->bytesInPacket<=builder->Path_MTU);
		/*take care of aggregation, flush if needed*/
		if (builder->last_au_sn==builder->auh_size) rtp_evrc_smv_flush(builder);
	}
	return GF_OK;
}

GF_Err gp_rtp_builder_do_h263(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize)
{
	GF_BitStream *bs;
	u8 hdr[2];
	Bool Pbit;
	u32 offset, size, max_size;

	builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;

	/*the H263 hinter doesn't perform inter-sample concatenation*/
	if (!data) return GF_OK;

	Pbit = GF_TRUE;

	/*skip 16 0'ed bits of start code*/
	offset = 2;
	data_size -= 2;
	max_size = builder->Path_MTU - 2;

	while(data_size > 0) {
		if(data_size > max_size) {
			size = max_size;
			builder->rtp_header.Marker = 0;
		} else {
			size = data_size;
			builder->rtp_header.Marker = 1;
		}

		data_size -= size;

		/*create new RTP Packet */
		builder->rtp_header.SequenceNumber += 1;
		builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);

		bs = gf_bs_new(hdr, 2, GF_BITSTREAM_WRITE);
		gf_bs_write_int(bs, 0, 5);
		gf_bs_write_int(bs, Pbit, 1);
		gf_bs_write_int(bs, 0, 10);
		gf_bs_del(bs);

		/*add header*/
		builder->OnData(builder->cbk_obj, (char*) hdr, 2, GF_TRUE);
		/*add payload*/
		if (builder->OnDataReference)
			builder->OnDataReference(builder->cbk_obj, size, offset);
		else
			builder->OnData(builder->cbk_obj, data + offset, size, GF_FALSE);

		builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);

		offset += size;
		Pbit = GF_FALSE;
	}
	return GF_OK;
}

GF_Err gp_rtp_builder_do_tx3g(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize, u32 duration, u8 descIndex)
{
	GF_BitStream *bs;
	u8 *hdr;
	u32 samp_size, txt_size, pay_start, hdr_size, txt_done, cur_frag, nb_frag;
	Bool is_utf_16 = GF_FALSE;

	if (!data) {
		/*flush packet*/
		if (builder->bytesInPacket) {
			builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
			builder->bytesInPacket = 0;
		}
		return GF_OK;
	}
	/*cfg packet*/
	txt_size = data[0];
	txt_size <<= 8;
	txt_size |= (unsigned char) data[1];
	/*remove BOM*/
	pay_start = 2;
	if (txt_size>2) {
		/*seems 3GP only accepts BE UTF-16 (no LE, no UTF32)*/
		if (((u8) data[2]==(u8) 0xFE) && ((u8) data[3]==(u8) 0xFF)) {
			is_utf_16 = GF_TRUE;
			pay_start = 4;
			txt_size -= 2;
		}
	}
	samp_size = data_size - pay_start;

	/*if TTU does not fit in packet flush packet*/
	if (builder->bytesInPacket && (builder->bytesInPacket + 3 + 6 + samp_size > builder->Path_MTU)) {
		builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
		builder->bytesInPacket = 0;
	}
	//we only deal with static SIDX
	descIndex += GF_RTP_TX3G_SIDX_OFFSET;

	/*first TTU in packet*/
	if (!builder->bytesInPacket) {
		builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;
		builder->rtp_header.Marker = 1;
		builder->rtp_header.SequenceNumber += 1;
		builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
	}
	/*fits entirely*/
	if (builder->bytesInPacket + 3 + 6 + samp_size <= builder->Path_MTU) {
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_int(bs, is_utf_16, 1);
		gf_bs_write_int(bs, 0, 4);
		gf_bs_write_int(bs, 1, 3);
		gf_bs_write_u16(bs, 8 + samp_size);
		gf_bs_write_u8(bs, descIndex);
		gf_bs_write_u24(bs, duration);
		gf_bs_write_u16(bs, txt_size);
		gf_bs_get_content(bs, &hdr, &hdr_size);
		gf_bs_del(bs);
		builder->OnData(builder->cbk_obj, (char *) hdr, hdr_size, GF_FALSE);
		builder->bytesInPacket += hdr_size;
		gf_free(hdr);

		if (txt_size) {
			if (builder->OnDataReference) {
				builder->OnDataReference(builder->cbk_obj, samp_size, pay_start);
			} else {
				builder->OnData(builder->cbk_obj, data + pay_start, samp_size, GF_FALSE);
			}
			builder->bytesInPacket += samp_size;
		}
		/*disable aggregation*/
		if (!(builder->flags & GP_RTP_PCK_USE_MULTI)) {
			builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
			builder->bytesInPacket = 0;
		}
		return GF_OK;
	}
	/*doesn't fit and already data, flush packet*/
	if (builder->bytesInPacket) {
		builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
		builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;
		/*split unit*/
		builder->rtp_header.Marker = 0;
		builder->rtp_header.SequenceNumber += 1;
		builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
		builder->bytesInPacket = 0;
	}
	/*write all type2 units (text only) - FIXME: split at char boundaries, NOT SUPPORTED YET*/
	txt_done = 0;
	nb_frag = 1;
	/*all fragments needed for Type2 units*/
	while (txt_done + (builder->Path_MTU-10) < txt_size) {
		txt_done += (builder->Path_MTU-10);
		nb_frag += 1;
	}
	/*all fragments needed for Type3/4 units*/
	txt_done = txt_size;
	while (txt_done + (builder->Path_MTU-7) < samp_size) {
		txt_done += (builder->Path_MTU-7);
		nb_frag += 1;
	}


	cur_frag = 0;
	txt_done = 0;
	while (txt_done<txt_size) {
		u32 size;
		if (txt_done + (builder->Path_MTU-10) < txt_size) {
			size = builder->Path_MTU-10;
		} else {
			size = txt_size - txt_done;
		}

		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_int(bs, is_utf_16, 1);
		gf_bs_write_int(bs, 0, 4);
		gf_bs_write_int(bs, 2, 3);
		gf_bs_write_u16(bs, 9 + size);
		gf_bs_write_int(bs, nb_frag, 4);
		gf_bs_write_int(bs, cur_frag, 4);
		gf_bs_write_u24(bs, duration);
		gf_bs_write_u8(bs, descIndex);
		/*SLEN is the full original length minus text len and BOM (put here for buffer allocation purposes)*/
		gf_bs_write_u16(bs, samp_size);
		gf_bs_get_content(bs, &hdr, &hdr_size);
		gf_bs_del(bs);
		builder->OnData(builder->cbk_obj, (char *) hdr, hdr_size, GF_FALSE);
		builder->bytesInPacket += hdr_size;
		gf_free(hdr);

		if (builder->OnDataReference) {
			builder->OnDataReference(builder->cbk_obj, size, pay_start + txt_done);
		} else {
			builder->OnData(builder->cbk_obj, data + pay_start + txt_done, size, GF_FALSE);
		}
		builder->bytesInPacket += size;
		cur_frag++;

		/*flush packet*/
		if (cur_frag == nb_frag) {
			txt_done = txt_size;
			if (pay_start + txt_done == data_size) {
				builder->rtp_header.Marker = 1;
				builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
				builder->bytesInPacket = 0;
			}
		} else {
			txt_done += size;
			builder->rtp_header.Marker = 0;
			builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
			builder->rtp_header.SequenceNumber += 1;
			builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
			builder->bytesInPacket = 0;
		}
	}

	txt_done = txt_size;

	/*write all modifiers - OPT: split at modifiers boundaries*/
	while (txt_done<samp_size) {
		u32 size, type;
		type = (txt_done == txt_size) ? 3 : 4;

		if (txt_done + (builder->Path_MTU-7) < samp_size) {
			size = builder->Path_MTU-10;
		} else {
			size = samp_size - txt_done;
		}

		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_int(bs, is_utf_16, 1);
		gf_bs_write_int(bs, 0, 4);
		gf_bs_write_int(bs, type, 3);
		gf_bs_write_u16(bs, 6 + size);
		gf_bs_write_int(bs, nb_frag, 4);
		gf_bs_write_int(bs, cur_frag, 4);
		gf_bs_write_u24(bs, duration);

		gf_bs_get_content(bs, &hdr, &hdr_size);
		gf_bs_del(bs);
		builder->OnData(builder->cbk_obj, (char *) hdr, hdr_size, GF_FALSE);
		builder->bytesInPacket += hdr_size;
		gf_free(hdr);

		if (builder->OnDataReference) {
			builder->OnDataReference(builder->cbk_obj, size, pay_start + txt_done);
		} else {
			builder->OnData(builder->cbk_obj, data + pay_start + txt_done, size, GF_FALSE);
		}
		builder->bytesInPacket += size;
		cur_frag++;
		if (cur_frag==nb_frag) {
			builder->rtp_header.Marker = 1;
			builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
			builder->bytesInPacket = 0;
		} else {
			builder->rtp_header.Marker = 0;
			builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
			builder->rtp_header.SequenceNumber += 1;
			builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
			builder->bytesInPacket = 0;
		}
		txt_done += size;
	}
	return GF_OK;
}


#if GPAC_ENABLE_3GPP_DIMS_RTP
GF_Err gp_rtp_builder_do_dims(GP_RTPPacketizer *builder, char *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize, u32 duration)
{
	u32 frag_state;
	GF_BitStream *bs;
	u32 offset;
	Bool is_last_du;

	/*the DIMS hinter doesn't perform inter-sample concatenation*/
	if (!data) return GF_OK;

	offset = 0;
	builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;
	bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	while (offset < data_size) {
		u32 du_offset = 0;
		u32 hdr_offset = 0;
		u32 orig_size, du_size;

		orig_size = du_size = 2+gf_bs_read_u16(bs);
		/*if dims size is >0xFFFF, use our internal hack for large units*/
		if (du_size==2) {
			orig_size = du_size = 2+gf_bs_read_u32(bs);
			hdr_offset = 4;
		}
		gf_bs_skip_bytes(bs, du_size-2);

		/*prepare M-bit*/
		is_last_du = (offset+du_size==data_size) ? GF_TRUE : GF_FALSE;

		frag_state = 0;
		while (du_size) {
			u32 size_offset = 0;
			u32 size;
			//size = du_size;

			/*does not fit, flush required*/
			if (builder->bytesInPacket && (du_size + 1 + builder->bytesInPacket > builder->Path_MTU)) {
				builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
				builder->bytesInPacket = 0;
			}

			/*fragmentation required*/
			if (du_size + 1 > builder->Path_MTU) {
				size = builder->Path_MTU - 1;
				/*first fragment*/
				if (!frag_state) {
					/*size field is skipped !!*/
					size_offset = 2 + hdr_offset;
					frag_state = 1;

					while (du_size - size_offset <= size) {
						size--;
					}
				}
				/*any middle fragment*/
				else frag_state = 2;

				builder->rtp_header.Marker = 0;
			}
			/*last fragment*/
			else if (frag_state) {
				size = du_size;
				frag_state = 3;
				builder->rtp_header.Marker = is_last_du;
			} else {
				size = du_size;
				builder->rtp_header.Marker = is_last_du;
			}

			if (frag_state && builder->bytesInPacket) {
				builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
				builder->bytesInPacket = 0;
			}

			/*need a new packet*/
			if (!builder->bytesInPacket) {
				char dims_rtp_hdr[1];

				/*the unit is critical, increase counter (coded on 3 bits)*/
				if (! (data[2+hdr_offset] & GF_DIMS_UNIT_P) && (frag_state <= 1) ) {
					builder->last_au_sn++;
					builder->last_au_sn %= 8;
				}
				/*set CTR value*/
				dims_rtp_hdr[0] = builder->last_au_sn;
				/*if M-bit is set in the dims unit header, replicate it*/
				if (data[2+hdr_offset] & (1<<1) ) dims_rtp_hdr[0] |= (1<<6);
				/*add unit fragmentation type*/
				dims_rtp_hdr[0] |= (frag_state<<3);

				builder->rtp_header.SequenceNumber += 1;
				builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
				builder->OnData(builder->cbk_obj, (char *) dims_rtp_hdr, 1, GF_TRUE);
				builder->bytesInPacket = 1;
			}

			/*add payload*/
			if (builder->OnDataReference)
				builder->OnDataReference(builder->cbk_obj, size, offset+du_offset+size_offset);
			else
				builder->OnData(builder->cbk_obj, data+offset+du_offset+size_offset, size, GF_FALSE);

			/*if fragmentation, force packet flush even on last packet since aggregation unit do not
			use the same packet format*/
			if (frag_state) {
				builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
				builder->bytesInPacket = 0;
			} else {
				builder->bytesInPacket += size;
			}
			du_offset += size+size_offset;
			assert(du_size>= size+size_offset);
			du_size -= size+size_offset;
		}
		offset += orig_size;
	}
	if (builder->bytesInPacket) {
		builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
		builder->bytesInPacket = 0;
	}
	gf_bs_del(bs);
	return GF_OK;
}
#endif



static void gf_rtp_ac3_flush(GP_RTPPacketizer *builder)
{
	char hdr[2];
	if (!builder->bytesInPacket) return;

	hdr[0] = builder->ac3_ft;
	hdr[1] = builder->last_au_sn;
	builder->OnData(builder->cbk_obj, hdr, 2, GF_TRUE);

	builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
	builder->bytesInPacket = 0;
	builder->last_au_sn = 0;
	builder->ac3_ft = 0;
}

GF_Err gp_rtp_builder_do_ac3(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize)
{
	char hdr[2];
	u32 offset, nb_pck;

	/*flush*/
	if (!data) {
		gf_rtp_ac3_flush(builder);
		return GF_OK;
	}

	if (
	    /*AU does not fit*/
	    (builder->bytesInPacket + data_size > builder->Path_MTU)
	    ||
	    /*aggregation is not enabled*/
	    !(builder->flags & GP_RTP_PCK_USE_MULTI)
	    ||
	    /*max ptime is exceeded*/
	    (builder->max_ptime && ( (u32) builder->sl_header.compositionTimeStamp >= builder->rtp_header.TimeStamp + builder->max_ptime) )

	) {
		gf_rtp_ac3_flush(builder);
	}

	/*fits*/
	if (builder->bytesInPacket + data_size < builder->Path_MTU) {
		/*need a new packet*/
		if (!builder->bytesInPacket) {
			builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;
			builder->ac3_ft = 0;
			builder->rtp_header.Marker = 1;
			builder->rtp_header.SequenceNumber += 1;
			builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
			/*2 bytes header*/
			builder->bytesInPacket = 2;
		}

		/*add payload*/
		if (builder->OnDataReference)
			builder->OnDataReference(builder->cbk_obj, data_size, 0);
		else
			builder->OnData(builder->cbk_obj, data, data_size, GF_FALSE);

		builder->bytesInPacket += data_size;
		builder->last_au_sn++;
		return GF_OK;
	}

	/*need fragmentation*/
	assert(!builder->bytesInPacket);
	offset = 0;
	nb_pck = data_size / (builder->Path_MTU-2);
	if (data_size % (builder->Path_MTU-2)) nb_pck++;
	builder->last_au_sn = nb_pck;

	while (offset < data_size) {
		u32 pck_size = MIN(data_size-offset, builder->Path_MTU-2);

		builder->rtp_header.Marker = 0;
		builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;
		builder->rtp_header.SequenceNumber += 1;

		if (!offset) {
			builder->ac3_ft = (pck_size > 5*data_size/8) ? 1 : 2;
		} else {
			builder->ac3_ft = 3;
			if (offset + pck_size == data_size)
				builder->rtp_header.Marker = 1;
		}
		builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);

		hdr[0] = builder->ac3_ft;
		hdr[1] = builder->last_au_sn;
		builder->OnData(builder->cbk_obj, hdr, 2, GF_TRUE);

		/*add payload*/
		if (builder->OnDataReference)
			builder->OnDataReference(builder->cbk_obj, pck_size, offset);
		else
			builder->OnData(builder->cbk_obj, data+offset, pck_size, GF_FALSE);

		builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
		offset += pck_size;
		builder->bytesInPacket = 0;
	}

	return GF_OK;
}

#endif /*GPAC_DISABLE_STREAMING*/

