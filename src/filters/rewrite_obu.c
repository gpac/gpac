/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / AV1 OBU rewrite filter
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

#include <gpac/filters.h>
#include <gpac/constants.h>
#include <gpac/bitstream.h>
#include <gpac/internal/media_dev.h>

#ifndef GPAC_DISABLE_AV_PARSERS

typedef enum {
	FRAMING_OBU 	= 0,
	FRAMING_AV1B 	= 1,
	FRAMING_IVF 	= 2,
	FRAMING_AV1TS	= 3
} OBUFramingMode;

typedef struct
{
	//opts
	Bool rcfg, tsep;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	u32 crc;

	Bool ivf_hdr;
	OBUFramingMode mode;
	GF_BitStream *bs_w;
	GF_BitStream *bs_r;
	u32 w, h;
	GF_Fraction fps;
	GF_AV1Config *av1c;
	u32 av1b_cfg_size;
	u32 codec_id;
} GF_OBUMxCtx;

GF_Err obumx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 crc;
	const GF_PropertyValue *p, *dcd;
	GF_OBUMxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	dcd = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!dcd) return GF_NON_COMPLIANT_BITSTREAM;

	crc = gf_crc_32(dcd->value.data.ptr, dcd->value.data.size);
	if (ctx->crc == crc) return GF_OK;
	ctx->crc = crc;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, pid);

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );

	ctx->ipid = pid;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_CODECID);
	ctx->codec_id = p ? p->value.uint : 0;
	switch (ctx->codec_id) {
	case GF_CODECID_AV1:
		//check output type OBU vs av1b
		p = gf_filter_pid_caps_query(ctx->opid, GF_PROP_PID_FILE_EXT);
		if (p) {
			if (!strcmp(p->value.string, "obu")) ctx->mode = FRAMING_OBU;
			else if (!strcmp(p->value.string, "av1b") || !strcmp(p->value.string, "av1")) ctx->mode = FRAMING_AV1B;
			//we might want to add a generic IVF read/write at some point
			else if (!strcmp(p->value.string, "ivf")) {
				ctx->mode = FRAMING_IVF;
				ctx->ivf_hdr = 1;
			} else if (!strcmp(p->value.string, "ts")) {
				ctx->mode = FRAMING_AV1TS;
			}
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[OBUWrite] Couldn't guess desired output format type, assuming plain OBU\n"));
		}
		break;
	case GF_CODECID_VP8:
	case GF_CODECID_VP9:
	case GF_CODECID_VP10:
		ctx->mode = FRAMING_IVF;
		ctx->ivf_hdr = 1;
		break;
	}

	if (ctx->av1c) gf_odf_av1_cfg_del(ctx->av1c);
	ctx->av1c = NULL;
	if (ctx->mode==FRAMING_AV1B) {
		u32 i=0;
		GF_AV1_OBUArrayEntry *obu;
		ctx->av1c = gf_odf_av1_cfg_read(dcd->value.data.ptr, dcd->value.data.size);
		if (!ctx->av1c) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[OBUWrite] Invalid av1 config\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		ctx->av1b_cfg_size = 0;

		while ((obu = gf_list_enum(ctx->av1c->obu_array, &i))) {
			//we don't output sequence header since it shall be present in sync sample
			//this avoids creating duplicate of the seqeunce header in the output stream
			if (obu->obu_type==OBU_SEQUENCE_HEADER) {
				i--;
				gf_list_rem(ctx->av1c->obu_array, i);
				gf_free(obu->obu);
				gf_free(obu);
				continue;
			}
			ctx->av1b_cfg_size += (u32) obu->obu_length;
			ctx->av1b_cfg_size += gf_av1_leb128_size(obu->obu_length);
		}
	}


	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_WIDTH);
	if (p) ctx->w = p->value.uint;
	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_HEIGHT);
	if (p) ctx->h = p->value.uint;
	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FPS);
	if (p) ctx->fps = p->value.frac;
	if (!ctx->fps.num || !ctx->fps.den) {
		ctx->fps.num = 25;
		ctx->fps.den = 1;
	}
	gf_filter_pid_set_property_str(ctx->opid, "obu:mode", &PROP_UINT(ctx->mode) );
	gf_filter_pid_set_framing_mode(ctx->ipid, GF_TRUE);

	return GF_OK;
}

// Create a new packet from the Bitstream
// Set initial properties from source packet
// Add packet to the list
static GF_Err obumx_add_packet(GF_FilterPid *opid,
							   GF_BitStream *bs,
							   GF_FilterPacket *src_pck,
							   GF_List *pcks)
{
	u8 *output = NULL;
	GF_FilterPacket *pck = NULL;
	u8 *pck_input_data;
	u32 pck_input_data_size;

	if (!pcks) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[OBUWrite] Packet list is null!!\n"));
		return GF_BAD_PARAM;
	}
	gf_bs_get_content(bs, &pck_input_data, &pck_input_data_size);
	if (!pck_input_data || !pck_input_data_size) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[OBUWrite] Trying to create an empty packet!!\n"));
		return GF_BAD_PARAM;
	}
	pck = gf_filter_pck_new_alloc(opid, pck_input_data_size, &output);
	if (!pck || !output) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[OBUWrite] Problem allocating new packet!!\n"));
		return GF_OUT_OF_MEM;
	}
	memcpy(output, pck_input_data, pck_input_data_size);
	if (src_pck) {
		gf_filter_pck_merge_properties(src_pck, pck);
	}
	gf_list_add(pcks, pck);
	return GF_OK;
}

// Generate MPEG-TS formatted OBU:
// - add start code
// - add emulation prevention bytes
// NOTE: size fields are not modified
static GF_Err format_obu_mpeg2ts(u8* in_data, u32 in_size, u8 **out_data, u32 *out_size) {
	const u8 START_CODE_SIZE = 3;
	u8 *pck_data_epb = NULL;
	u32 pck_size_epb = 0;

	if (!in_data || !in_size || !out_data || !out_size) {
		return GF_BAD_PARAM;
	}
	pck_size_epb = START_CODE_SIZE + in_size + gf_media_nalu_emulation_bytes_add_count(in_data, in_size);
	pck_data_epb = gf_malloc(pck_size_epb);
	if (!pck_data_epb) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[OBUWrite] Could not allocate EPB buffer!!\n"));
		return GF_OUT_OF_MEM;
	}
	pck_data_epb[0] = 0;
	pck_data_epb[1] = 0;
	pck_data_epb[2] = 1;
	gf_media_nalu_add_emulation_bytes(in_data, pck_data_epb+3, in_size);
	*out_data = pck_data_epb;
	*out_size = pck_size_epb;
	return GF_OK;
}

// Generate one or more output packet(s) from an input packet that contains a AV1 TU
// One output packet is created for each frame (frame_header only or full frame)
// All OBUs are transformed to add start code and emulation prevention bytes
static GF_Err obumx_process_mpeg2au(GF_OBUMxCtx *ctx, GF_FilterPacket *src_pck, u8 *data, u32 src_pck_size) {
	Bool first_frame_found = GF_FALSE;
	u32 pck_count = 0;
	Bool first_packet = GF_TRUE;
	GF_List *pcks = gf_list_new();
	u64 cts = gf_filter_pck_get_cts(src_pck);
	u32 duration = gf_filter_pck_get_duration(src_pck);
	u32 out_duration;
	u64 out_cts;

	if (!ctx->bs_r) ctx->bs_r = gf_bs_new(data, src_pck_size, GF_BITSTREAM_READ);
	else gf_bs_reassign_buffer(ctx->bs_r, data, src_pck_size);

	if (ctx->bs_w) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[OBUWrite] Write bitstream should be null, deleting... !!\n"));
		gf_bs_del(ctx->bs_w);
	}
	ctx->bs_w = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	while (gf_bs_available(ctx->bs_r)) {
		ObuType obu_type;
		u32 obu_size;
		u32 read_size;
		u8 *obu_data;
		u8 *out_obu_data = NULL;
		u32 out_obu_size = 0;
		Bool obu_extension_flag, obu_has_size_field;
		u8 temporal_id, spatial_id;
		u64 obu_start = (u32) gf_bs_get_position(ctx->bs_r);
		u32 obu_hdr_size;

		gf_av1_parse_obu_header(ctx->bs_r, &obu_type, &obu_extension_flag, &obu_has_size_field, &temporal_id, &spatial_id);
		obu_hdr_size = gf_bs_get_position(ctx->bs_r) - obu_start;

		if (obu_has_size_field) {
			obu_size = (u32)gf_av1_leb128_read(ctx->bs_r, NULL);
			obu_hdr_size = gf_bs_get_position(ctx->bs_r) - obu_start;
		} else {
			obu_size = src_pck_size - (u32) gf_bs_get_position(ctx->bs_r);
		}
		obu_size += obu_hdr_size;
		gf_bs_seek(ctx->bs_r, obu_start);

		if (obu_type == OBU_FRAME || obu_type == OBU_FRAME_HEADER) {
			// start creating packet only after the first frame is found
			if (first_frame_found) {
				obumx_add_packet(ctx->opid, ctx->bs_w, src_pck, pcks);
				gf_bs_del(ctx->bs_w);
				ctx->bs_w = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			}
			first_frame_found = GF_TRUE;
		}
		obu_data = gf_malloc(obu_size);
		read_size = gf_bs_read_data(ctx->bs_r, obu_data, obu_size);
		if (read_size != obu_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[OBUWrite] Could not read entire OBU %d vs %d !!\n", read_size, obu_size));
		}
		GF_Err e = format_obu_mpeg2ts(obu_data, read_size, &out_obu_data, &out_obu_size);
		if (e != GF_OK || out_obu_data == NULL || out_obu_size == 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[OBUWrite] Problem formatting OBU for MPEG-2 TS!!\n"));
		} else {
			gf_bs_write_data(ctx->bs_w, out_obu_data, out_obu_size);
		}
	}
	// create last packet from any pending data
	obumx_add_packet(ctx->opid,ctx->bs_w, src_pck, pcks);
	gf_bs_del(ctx->bs_w);
	ctx->bs_w = NULL;

	// Adjust CTS, flags and send
	pck_count = gf_list_count(pcks);
	out_duration = duration / pck_count;
	out_cts = cts;
	while (gf_list_count(pcks)) {
		GF_FilterPacket *pck = (GF_FilterPacket *)gf_list_get(pcks, 0);
		gf_list_rem(pcks, 0);
		gf_filter_pck_set_cts(pck, out_cts);
		out_cts += out_duration;
		if (gf_list_count(pcks) != 0) {
			gf_filter_pck_set_duration(pck, out_duration);
		} else {
			// adjust the duration of the last packet for rounding issues
			gf_filter_pck_set_duration(pck, duration - (u32)(out_cts-cts));
		}
		if (first_packet) {
			// The first output packet gets the timing, flags and sap type of the input packet
			u8 sap_type = gf_filter_pck_get_sap(src_pck);
			u8 flags = gf_filter_pck_get_dependency_flags(src_pck);
			gf_filter_pck_set_sap(pck, sap_type);
			gf_filter_pck_set_dependency_flags(pck, flags);
			first_packet = GF_FALSE;
		} else {
			// all other output packets get no flags
			gf_filter_pck_set_sap(pck, 0);
			gf_filter_pck_set_dependency_flags(pck, 0);
		}
		gf_filter_pck_send(pck);
	}
	gf_list_del(pcks);

	// we are done with the input packet
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

GF_Err obumx_process(GF_Filter *filter)
{
	u32 i;
	u32 frame_sizes[128], max_frames;
	GF_OBUMxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u32 pck_size, size, sap_type, hdr_size, av1b_frame_size=0;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);
	if (!pck_size) {
		//if output and packet properties, forward - this is required for sinks using packets for state signaling
		//such as TS muxer in dash mode looking for EODS property
		if (ctx->opid && gf_filter_pck_has_properties(pck))
			gf_filter_pck_forward(pck, ctx->opid);

		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}

	if (ctx->mode == FRAMING_AV1TS) {
		return obumx_process_mpeg2au(ctx, pck, data, pck_size);
	}

	hdr_size = 0;
	size = pck_size;
	//always add temporal delim
	if (ctx->codec_id==GF_CODECID_AV1)
		size += 2;

	sap_type = gf_filter_pck_get_sap(pck);
	if (!sap_type) {
		u8 flags = gf_filter_pck_get_dependency_flags(pck);
		if (flags) {
			//get dependsOn
			flags>>=4;
			flags &= 0x3;
			if (flags==2) sap_type = 3; //could be 1, 2 or 3
		}
	}

	memset(frame_sizes, 0, sizeof(u32)*128);
	if (ctx->mode==FRAMING_IVF) {
		if (ctx->ivf_hdr) hdr_size += 32;
		hdr_size += 12;
	}
	if (ctx->mode==FRAMING_AV1B) {
		u32 obu_sizes=0;
		u32 frame_idx=0;

		obu_sizes = frame_sizes[0] = 3;
		if (sap_type && ctx->av1b_cfg_size) {
			frame_sizes[0] += ctx->av1b_cfg_size;
			obu_sizes += ctx->av1b_cfg_size;
		}

		if (!ctx->bs_r) ctx->bs_r = gf_bs_new(data, pck_size, GF_BITSTREAM_READ);
		else gf_bs_reassign_buffer(ctx->bs_r, data, pck_size);

		while (gf_bs_available(ctx->bs_r)) {
			ObuType obu_type;
			u32 obu_size;
			Bool obu_extension_flag, obu_has_size_field;
			u8 temporal_id, spatial_id;
			u32 obu_hdr_size = (u32) gf_bs_get_position(ctx->bs_r);

			gf_av1_parse_obu_header(ctx->bs_r, &obu_type, &obu_extension_flag, &obu_has_size_field, &temporal_id, &spatial_id);

			if (!obu_has_size_field) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[OBUWrite] OBU without size field, bug in demux filter !!\n"));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			obu_size = (u32)gf_av1_leb128_read(ctx->bs_r, NULL);
			obu_hdr_size = (u32) gf_bs_get_position(ctx->bs_r) - obu_hdr_size;
			gf_bs_skip_bytes(ctx->bs_r, obu_size);

			obu_size += obu_hdr_size;
			obu_sizes += obu_size + gf_av1_leb128_size(obu_size);

			if (obu_type==OBU_FRAME) {
				frame_idx++;
				if (frame_idx==128) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[OBUWrite] more than 128 frames in a temporal unit not supported\n"));
					return GF_NOT_SUPPORTED;
				}
				if (frame_idx>1)
					frame_sizes[frame_idx-1] = 0;
			}
			frame_sizes[frame_idx ? (frame_idx-1) : 0] += obu_size + gf_av1_leb128_size(obu_size);
		}
		max_frames = frame_idx;
		size = obu_sizes;
		//packet without frame
		if (!max_frames) {
			size += gf_av1_leb128_size(frame_sizes[0]);
		} else {
			for (i=0;i<max_frames; i++) {
				size += gf_av1_leb128_size(frame_sizes[i]);
			}
		}
		av1b_frame_size = size;
		size += gf_av1_leb128_size(size);
	} else {
		if (sap_type && ctx->av1b_cfg_size) size += ctx->av1b_cfg_size;
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, hdr_size+size, &output);
	if (!dst_pck) return GF_OUT_OF_MEM;
	
	gf_filter_pck_merge_properties(pck, dst_pck);

	if (!ctx->bs_w) ctx->bs_w = gf_bs_new(output, hdr_size+size, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs_w, output, hdr_size+size);

	if (ctx->mode==FRAMING_AV1B) {
		u32 frame_idx = 0;
		//temporal unit
		gf_av1_leb128_write(ctx->bs_w, av1b_frame_size);
		assert(frame_sizes[0]);
		gf_av1_leb128_write(ctx->bs_w, frame_sizes[0]);

		//write temporal delim with obu size set
		gf_av1_leb128_write(ctx->bs_w, 2);
		gf_bs_write_u8(ctx->bs_w, 0x12);
		gf_bs_write_u8(ctx->bs_w, 0);

		if (sap_type && ctx->av1b_cfg_size) {
			GF_AV1_OBUArrayEntry *obu;
			i=0;
			while ((obu = gf_list_enum(ctx->av1c->obu_array, &i))) {
				gf_av1_leb128_write(ctx->bs_w, obu->obu_length);
				gf_bs_write_data(ctx->bs_w, obu->obu, (u32) obu->obu_length);
			}
		}

		gf_bs_reassign_buffer(ctx->bs_r, data, pck_size);

		while (gf_bs_available(ctx->bs_r)) {
			ObuType obu_type;
			u32 obu_size;
			Bool obu_extension_flag, obu_has_size_field;
			u8 temporal_id, spatial_id;
			u32 obu_hdr_size, start = (u32) gf_bs_get_position(ctx->bs_r);

			gf_av1_parse_obu_header(ctx->bs_r, &obu_type, &obu_extension_flag, &obu_has_size_field, &temporal_id, &spatial_id);
			obu_size = (u32)gf_av1_leb128_read(ctx->bs_r, NULL);

			obu_hdr_size = (u32) gf_bs_get_position(ctx->bs_r) - start;
			gf_bs_skip_bytes(ctx->bs_r, obu_size);

			if (obu_type==OBU_FRAME) {
				frame_idx++;
				if (frame_idx>1) {
					gf_av1_leb128_write(ctx->bs_w, frame_sizes[frame_idx-1] );
				}
			}

			obu_size += obu_hdr_size;
			gf_av1_leb128_write(ctx->bs_w, obu_size);
			gf_bs_write_data(ctx->bs_w, data+start, obu_size);

		}
		assert(gf_bs_get_position(ctx->bs_w) == size);
	} else {

		//write IVF headers
		if (ctx->ivf_hdr) {
			gf_bs_write_u32(ctx->bs_w, GF_4CC('D', 'K', 'I', 'F'));
			gf_bs_write_u16_le(ctx->bs_w, 0);
			gf_bs_write_u16_le(ctx->bs_w, 32);

			//codec_fourcc
			switch (ctx->codec_id) {
			case GF_CODECID_AV1:
				gf_bs_write_u32(ctx->bs_w, GF_4CC('A', 'V', '0', '1') );
				break;
			case GF_CODECID_VP8:
				gf_bs_write_u32(ctx->bs_w, GF_4CC('V', 'P', '8', '0') );
				break;
			case GF_CODECID_VP9:
				gf_bs_write_u32(ctx->bs_w, GF_4CC('V', 'P', '9', '0') );
				break;
			case GF_CODECID_VP10:
				gf_bs_write_u32(ctx->bs_w, GF_4CC('V', 'P', '1', '0') );
				break;
			default:
				gf_bs_write_u32(ctx->bs_w, ctx->codec_id);
				break;
			}

			gf_bs_write_u16_le(ctx->bs_w, ctx->w);
			gf_bs_write_u16_le(ctx->bs_w, ctx->h);
			gf_bs_write_u32_le(ctx->bs_w, ctx->fps.num);
			gf_bs_write_u32_le(ctx->bs_w, ctx->fps.den);
			const GF_PropertyValue *p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_NB_FRAMES);
			//nb frames
			if (p)
				gf_bs_write_u32_le(ctx->bs_w, p->value.uint );
			else
				gf_bs_write_u32_le(ctx->bs_w, 0);
			gf_bs_write_u32_le(ctx->bs_w, 0);
			ctx->ivf_hdr = 0;
		}
		if (ctx->mode==FRAMING_IVF) {
			u64 cts = gf_timestamp_rescale(gf_filter_pck_get_cts(pck), ctx->fps.den * gf_filter_pck_get_timescale(pck), ctx->fps.num);
			gf_bs_write_u32_le(ctx->bs_w, size);
			gf_bs_write_u64_le(ctx->bs_w, cts);
		}
		if (ctx->codec_id==GF_CODECID_AV1) {
			//write temporal delim without obu size set
			gf_bs_write_u8(ctx->bs_w, 0x12);
			gf_bs_write_u8(ctx->bs_w, 0);

			if (sap_type && ctx->av1b_cfg_size) {
				GF_AV1_OBUArrayEntry *obu;
				i=0;
				while ((obu = gf_list_enum(ctx->av1c->obu_array, &i))) {
					gf_av1_leb128_write(ctx->bs_w, obu->obu_length);
					gf_bs_write_data(ctx->bs_w, obu->obu, (u32) obu->obu_length);
				}
			}
		}
		gf_bs_write_data(ctx->bs_w, data, pck_size);
	}

	if (!ctx->rcfg) {
		ctx->av1b_cfg_size = 0;
	}
	gf_filter_pck_send(dst_pck);
	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}
static void obumx_finalize(GF_Filter *filter)
{
	GF_OBUMxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs_r) gf_bs_del(ctx->bs_r);
	if (ctx->bs_w) gf_bs_del(ctx->bs_w);
	if (ctx->av1c) gf_odf_av1_cfg_del(ctx->av1c);
}

static const GF_FilterCapability OBUMxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_AV1),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_VP8),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_VP9),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_VP10),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
};



#define OFFS(_n)	#_n, offsetof(GF_OBUMxCtx, _n)
static const GF_FilterArgs OBUMxArgs[] =
{
	{ OFFS(rcfg), "force repeating decoder config at each I-frame", GF_PROP_BOOL, "true", NULL, 0},
	{0}
};


GF_FilterRegister OBUMxRegister = {
	.name = "ufobu",
	GF_FS_SET_DESCRIPTION("IVF/OBU/annexB writer")
	GF_FS_SET_HELP("This filter rewrites VPx or AV1 bitstreams into a IVF, annexB or OBU sequence.\n"
	"The temporal delimiter OBU is re-inserted in annexB (`.av1` and `.av1b`files, with obu_size set) and OBU sequences (`.obu`files, without obu_size)\n"
	"Note: VP8/9 codecs will only use IVF output (equivalent to file extension `.ivf` or `:ext=ivf` set on output).\n"
	)
	.private_size = sizeof(GF_OBUMxCtx),
	.args = OBUMxArgs,
	SETCAPS(OBUMxCaps),
	.finalize = obumx_finalize,
	.configure_pid = obumx_configure_pid,
	.process = obumx_process
};


const GF_FilterRegister *obumx_register(GF_FilterSession *session)
{
	return &OBUMxRegister;
}

#else
const GF_FilterRegister *obumx_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_AV_PARSERS

