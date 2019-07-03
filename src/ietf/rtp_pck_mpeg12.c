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
#include <gpac/avparse.h>

static void mpa12_do_flush(GP_RTPPacketizer *builder, Bool start_new)
{
	u8 *tmp;
	u32 tmp_size;
	/*flush*/
	if (builder->pck_hdr) {
		gf_bs_get_content(builder->pck_hdr, &tmp, &tmp_size);
		builder->OnData(builder->cbk_obj, tmp, tmp_size, GF_TRUE);
		gf_free(tmp);

		if (gf_bs_get_size(builder->payload)) {
			gf_bs_get_content(builder->payload, &tmp, &tmp_size);
			builder->OnData(builder->cbk_obj, tmp, tmp_size, GF_FALSE);
			gf_free(tmp);
		}

		builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
		gf_bs_del(builder->pck_hdr);
		gf_bs_del(builder->payload);
		builder->pck_hdr = NULL;
		builder->payload = NULL;
		builder->bytesInPacket = 0;
	}
	if (!start_new) return;

	builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;
	builder->pck_hdr = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	builder->payload = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	/*create new RTP Packet */
	builder->rtp_header.SequenceNumber += 1;
	builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
	builder->first_sl_in_rtp = GF_TRUE;
	builder->bytesInPacket = 0;
}

GF_Err gp_rtp_builder_do_mpeg12_audio(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize)
{
	u32 pck_size;
	u16 offset;

	/*if no data flush, if nothing start if not enough space restart*/
	if (!data || !builder->bytesInPacket || (builder->bytesInPacket + data_size > builder->Path_MTU)) {
		mpa12_do_flush(builder, data ? GF_TRUE : GF_FALSE);
		if (!data) return GF_OK;
	}

	offset = 0;
	while (data_size) {
		if (data_size + 4 < builder->Path_MTU) {
			pck_size = data_size;
		} else {
			pck_size = builder->Path_MTU - 4;
		}
		if (builder->first_sl_in_rtp) {
			gf_bs_write_u16(builder->pck_hdr, 0);
			gf_bs_write_u16(builder->pck_hdr, offset);
			builder->first_sl_in_rtp = GF_FALSE;
			builder->bytesInPacket = 2;
		}
		/*add reference*/
		if (builder->OnDataReference)
			builder->OnDataReference(builder->cbk_obj, pck_size, offset);
		else
			gf_bs_write_data(builder->payload, data + offset, pck_size);

		data_size -= pck_size;
		builder->bytesInPacket += pck_size;
		/*start new packet if fragmenting*/
		if (data_size) {
			offset += (u16) pck_size;
			mpa12_do_flush(builder, GF_TRUE);
		}
	}
	/*if offset or no aggregation*/
	if (offset || !(builder->flags & GP_RTP_PCK_USE_MULTI) ) mpa12_do_flush(builder, GF_FALSE);

	return GF_OK;
}

#ifndef GPAC_DISABLE_AV_PARSERS

#define MPEG12_PICTURE_START_CODE         0x00000100
#define MPEG12_SEQUENCE_START_CODE        0x000001b3

GF_Err gp_rtp_builder_do_mpeg12_video(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize)
{
	u32 startcode, pic_type, max_pck_size, offset, prev_slice, next_slice;
	Bool start_with_slice, slices_done, got_slice, first_slice, have_seq;
	char mpv_hdr[4];
	char *payload, *buffer;

	/*no flsuh (no aggregation)*/
	if (!data) return GF_OK;

	offset = 0;
	have_seq = GF_FALSE;

	while (1) {
		u32 oldoffset;
		oldoffset = offset;
		if (gf_mv12_next_start_code((unsigned char *) data + offset, data_size - offset, &offset, &startcode) < 0)
			break;

		offset += oldoffset;
		if (startcode == MPEG12_SEQUENCE_START_CODE) have_seq = GF_TRUE;
		offset += 4;

		if (startcode == MPEG12_PICTURE_START_CODE) break;
	}

	max_pck_size = builder->Path_MTU - 4;

	payload = data + offset;
	pic_type = (payload[1] >> 3) & 0x7;
	/*first 6 bits (MBZ and T bit) not used*/
	/*temp ref on 10 bits*/
	mpv_hdr[0] = (payload[0] >> 6) & 0x3;
	mpv_hdr[1] = (payload[0] << 2) | ((payload[1] >> 6) & 0x3);
	mpv_hdr[2] = pic_type;
	mpv_hdr[3] = 0;

	if ((pic_type==2) || (pic_type== 3)) {
		mpv_hdr[3] = (((u32)payload[3]) << 5) & 0xf;
		if ((payload[4] & 0x80) != 0) mpv_hdr[3] |= 0x10;
		if (pic_type == 3) mpv_hdr[3] |= (payload[4] >> 3) & 0xf;
	}

	/*start packet*/
	builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;
	builder->rtp_header.Marker = 1;
	builder->rtp_header.SequenceNumber += 1;
	builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);

	buffer = data;
	prev_slice = 0;
	start_with_slice = (gf_mv12_next_slice_start((unsigned char *)buffer, offset, data_size, &next_slice) >= 0) ? GF_TRUE : GF_FALSE;
	offset = 0;
	slices_done = GF_FALSE;
	got_slice = start_with_slice;
	first_slice = GF_TRUE;

	while (data_size > 0) {
		Bool last_pck;
		u32 len_to_write;

		if (data_size <= max_pck_size) {
			len_to_write = data_size;
			last_pck = GF_TRUE;
//			prev_slice = 0;
		} else {
			got_slice = (!first_slice && !slices_done && (next_slice <= max_pck_size)) ? GF_TRUE : GF_FALSE;
			first_slice = GF_FALSE;
			last_pck = GF_FALSE;

			while (!slices_done && (next_slice <= max_pck_size)) {
				prev_slice = next_slice;
				if (gf_mv12_next_slice_start((unsigned char *)buffer, next_slice + 4, data_size, &next_slice) >= 0) {
					got_slice = GF_TRUE;
				} else {
					slices_done = GF_TRUE;
				}
			}
			if (got_slice) len_to_write = prev_slice;
			else len_to_write = MIN(max_pck_size, data_size);
		}

		mpv_hdr[2] = pic_type;

		if (have_seq) {
			mpv_hdr[2] |= 0x20;
			have_seq = GF_FALSE;
		}
		if (first_slice) mpv_hdr[2] |= 0x10;

		if (got_slice || last_pck) {
			mpv_hdr[2] |= 0x08;
//			start_with_slice = GF_TRUE;
		} else {
//			start_with_slice = GF_FALSE;
		}

		builder->OnData(builder->cbk_obj, mpv_hdr, 4, GF_FALSE);
		if (builder->OnDataReference) {
			builder->OnDataReference(builder->cbk_obj, len_to_write, offset);
		} else {
			builder->OnData(builder->cbk_obj, data + offset, len_to_write, GF_FALSE);
		}

		builder->rtp_header.Marker = last_pck ? 1 : 0;
		builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);

		offset += len_to_write;
		data_size -= len_to_write;
		prev_slice = 0;
		next_slice -= len_to_write;
		buffer += len_to_write;

		if (!last_pck) {
			builder->rtp_header.Marker = 0;
			builder->rtp_header.SequenceNumber += 1;
			builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
		}
	}
	return GF_OK;
}

#endif

#endif /*GPAC_DISABLE_STREAMING*/
