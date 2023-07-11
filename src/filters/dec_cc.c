/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2023
 *					All rights reserved
 *
 *  This file is part of GPAC / Closed captions decode filter
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
#include <gpac/tools.h>
#include <gpac/avparse.h>
#include <gpac/internal/media_dev.h>

#define GF_ITU_T35_PROVIDER_ATSC	49

#ifdef GPAC_HAS_LIBCAPTION
#include <caption/cea708.h>

#if defined(WIN32) || defined(_WIN32_WCE)
#if !defined(__GNUC__)
#pragma comment(lib, "libcaption")
#endif
#endif

enum
{
	CCTYPE_UNK=0,
	CCTYPE_M2V,
	CCTYPE_M4V,
	CCTYPE_OBU,
	CCTYPE_AVC,
	CCTYPE_HEVC,
	CCTYPE_VVC,
};

typedef struct
{
	u8 *data;
	u32 size;
	Bool is_m2v;
	u64 timestamp;
} CCItem;

typedef struct
{
	GF_FilterPid *ipid;
	GF_FilterPid *opid;
	u32 cctype;
	u32 nalu_size_len;
	GF_List *cc_queue;

	u32 timescale;
#ifdef GPAC_HAS_LIBCAPTION
	caption_frame_t *ccframe;
	u32 cc_last_crc;
	u64 last_ts_plus_one;
#endif
	GF_BitStream *bs;
} CCDecCtx;


GF_Err ccdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	CCDecCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPid *out_pid;
	const GF_PropertyValue *prop;

	if (is_remove) {
		out_pid = gf_filter_pid_get_udta(pid);
		if (out_pid==ctx->opid)
			ctx->opid = NULL;
		if (out_pid)
			gf_filter_pid_remove(out_pid);
		return GF_OK;
	}

	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		if (!ctx->opid) return GF_OUT_OF_MEM;
		gf_filter_pid_set_udta(pid, ctx->opid);
	}
	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_SIMPLE_TEXT) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	ctx->timescale = prop ? prop->value.uint : 1000;

	ctx->cctype = CCTYPE_UNK;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!prop) return GF_BAD_PARAM;
	switch (prop->value.uint) {
#ifndef GPAC_DISABLE_AV_PARSERS
	case GF_CODECID_MPEG2_422:
	case GF_CODECID_MPEG2_SNR:
	case GF_CODECID_MPEG2_HIGH:
	case GF_CODECID_MPEG2_MAIN:
	case GF_CODECID_MPEG2_SIMPLE:
	case GF_CODECID_MPEG2_SPATIAL:
		ctx->cctype = CCTYPE_M2V;
		break;
	case GF_CODECID_MPEG4_PART2:
		ctx->cctype = CCTYPE_M4V;
		break;
#endif
	case GF_CODECID_AV1:
		ctx->cctype = CCTYPE_OBU;
		break;
	case GF_CODECID_AVC:
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (prop) {
			GF_AVCConfig *avcc = gf_odf_avc_cfg_read(prop->value.data.ptr, prop->value.data.size);
			if (avcc) {
				ctx->nalu_size_len = avcc->nal_unit_size;
				gf_odf_avc_cfg_del(avcc);
				ctx->cctype = CCTYPE_AVC;
			} else {
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		} else {
			return GF_OK;
		}
		break;
	case GF_CODECID_HEVC:
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (prop) {
			GF_HEVCConfig *hvcc = gf_odf_hevc_cfg_read(prop->value.data.ptr, prop->value.data.size, GF_FALSE);
			if (hvcc) {
				ctx->nalu_size_len = hvcc->nal_unit_size;
				gf_odf_hevc_cfg_del(hvcc);
				ctx->cctype = CCTYPE_HEVC;
			} else {
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		} else {
			return GF_OK;
		}
		break;
	case GF_CODECID_VVC:
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (prop) {
			GF_VVCConfig *vvcc = gf_odf_vvc_cfg_read(prop->value.data.ptr, prop->value.data.size);
			if (vvcc) {
				ctx->nalu_size_len = vvcc->nal_unit_size;
				gf_odf_vvc_cfg_del(vvcc);
				ctx->cctype = CCTYPE_VVC;
			} else {
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		} else {
			return GF_OK;
		}
		break;
	}
	if (ctx->cctype == CCTYPE_UNK) return GF_NOT_SUPPORTED;

	return GF_OK;
}

u32 gf_m4v_parser_get_obj_type(GF_M4VParser *m4v);
void gf_m4v_parser_set_inspect(GF_M4VParser *m4v);

static GF_Err ccdec_flush_queue(CCDecCtx *ctx)
{
	CCItem *cc = gf_list_pop_front(ctx->cc_queue);
	if (!cc) return GF_EOS;

	cea708_t scc;
	cea708_init(&scc, 0);
	if (cc->is_m2v)
		cea708_parse_h262(cc->data, cc->size, &scc);
	else
		cea708_parse_h264(cc->data, cc->size, &scc);

	u64 ts = cc->timestamp;
	u32 i, count = cea708_cc_count(&scc.user_data);
	libcaption_stauts_t status = LIBCAPTION_OK;
	if (!ctx->ccframe) {
		GF_SAFEALLOC(ctx->ccframe, caption_frame_t);
		caption_frame_init(ctx->ccframe);
	}
	Double timestamp = (Double) cc->timestamp;
	timestamp /= ctx->timescale;
	Bool dump_frame = GF_FALSE;
	if (!ctx->last_ts_plus_one)
		ctx->last_ts_plus_one = ts+1;

	for (i = 0; i < count; ++i) {
		int valid;
		cea708_cc_type_t type;
		uint16_t cc_data = cea708_cc_data(&scc.user_data, i, &valid, &type);

		if (valid
			&& ((cc_type_ntsc_cc_field_1 == type) || (cc_type_ntsc_cc_field_2 == type))
		) {
			status = libcaption_status_update(status, caption_frame_decode(ctx->ccframe, cc_data, timestamp));
			if (status == LIBCAPTION_READY) {
				dump_frame = GF_TRUE;
			}
		}
	}
	gf_free(cc->data);
	gf_free(cc);

	if (!dump_frame) return GF_OK;

	u8 txtdata[CAPTION_FRAME_TEXT_BYTES+1];
	u32 size = (u32) caption_frame_to_text(ctx->ccframe, txtdata);
	u32 crc = gf_crc_32(txtdata, size);
	if (crc!=ctx->cc_last_crc) {
		ctx->cc_last_crc = crc;
	} else {
		size=0;
	}
	if (!size) return GF_OK;

	u8 *output;
	GF_FilterPacket *pck = gf_filter_pck_new_alloc(ctx->opid, size+1, &output);
	if (!pck) return GF_OUT_OF_MEM;
	memcpy(output, txtdata, size);
	output[size] = 0;
	gf_filter_pck_truncate(pck, size);
	gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);
	gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
	gf_filter_pck_set_cts(pck, ctx->last_ts_plus_one-1);
	gf_filter_pck_set_duration(pck, (u32) (ts - ctx->last_ts_plus_one+1));
	ctx->last_ts_plus_one = 0;

	gf_filter_pck_send(pck);
	return GF_OK;
}

static GF_Err ccdec_queue_data(CCDecCtx *ctx, u64 ts, u8 *data, u32 max_size, Bool m2v, Bool keep_data)
{
	//for m2v, check if the udta is indeed a CC
	if (m2v) {
		if (max_size<7) {
			if (keep_data) gf_free(data);
			return GF_OK;
		}
		u32 udta_id = GF_4CC(data[0], data[1], data[2], data[3]);
		u32 udta_code = 0;
		if (udta_id==GF_4CC('G','A','9','4'))
			udta_code = data[4];
		else if (udta_id==GF_4CC('D','T','G','1'))
			udta_code = data[4];

		if (udta_code == 3) {
			u32 cc_count = (data[5] & 0x1F);
			cc_count *= 3;
			if (max_size<7+cc_count) {
				if (keep_data) gf_free(data);
				return GF_OK;
			}
			max_size = 7+cc_count;
		}
	}
	//otherwise check was done in sei / OBU Metadata parsing

	CCItem *it;
	GF_SAFEALLOC(it, CCItem);
	if (!it) {
		if (keep_data) gf_free(data);
		return GF_OUT_OF_MEM;
	}
	if (!keep_data) {
		it->data = gf_malloc(sizeof(u8)*max_size);
		if (!it->data) return GF_OUT_OF_MEM;
		memcpy(it->data, data, sizeof(u8)*max_size);
	} else {
		it->data = data;
	}
	it->size = max_size;
	it->timestamp = ts;
	it->is_m2v = m2v;
	Bool can_flush = GF_FALSE;
	u32 i, count=gf_list_count(ctx->cc_queue);
	for (i=0; i<count; i++) {
		CCItem *an_it = gf_list_get(ctx->cc_queue, i);
		if (an_it->timestamp > it->timestamp) {
			gf_list_insert(ctx->cc_queue, it, i);
			if (i)
				can_flush = GF_TRUE;
			it=NULL;
			break;
		}
	}
	if (it) gf_list_add(ctx->cc_queue, it);

	while (can_flush) {
		it = gf_list_get(ctx->cc_queue, 0);
		if (it->timestamp>=ts) break;
		ccdec_flush_queue(ctx);
	}
	return GF_OK;
}


GF_Err ccdec_process(GF_Filter *filter)
{
	GF_Err e;
	u64 ts;
	CCDecCtx *ctx = gf_filter_get_udta(filter);
	const u8 *data;
	u32 size;
	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			while (gf_list_count(ctx->cc_queue)) {
				ccdec_flush_queue(ctx);
			}
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	data = gf_filter_pck_get_data(pck, &size);
	if (!data || !size) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}
	ts = gf_filter_pck_get_cts(pck);

#ifndef GPAC_DISABLE_AV_PARSERS
	if (ctx->cctype<=CCTYPE_M4V) {
		GF_M4VParser *m4v = gf_m4v_parser_new((u8*)data, size, (ctx->cctype==CCTYPE_M2V) ? GF_TRUE : GF_FALSE);
		gf_m4v_parser_set_inspect(m4v);

		while (1) {
			u8 ftype = 0;
			u32 tinc, o_type;
			u64 fsize;
			u64 start;
			GF_M4VDecSpecInfo dsi;
			Bool is_coded = GF_FALSE;
			e = gf_m4v_parse_frame(m4v, &dsi, &ftype, &tinc, &fsize, &start, &is_coded);
			if (e<0)
				break;

			o_type = gf_m4v_parser_get_obj_type(m4v);
			if (ctx->cctype==CCTYPE_M4V) {
				switch (o_type) {
				case M4V_UDTA_START_CODE:
					ccdec_queue_data(ctx, ts, (u8*) data + start+4, (u32) (size-start-4), GF_TRUE, GF_FALSE);
					break;
				default:
					break;
				}
			} else {
				switch (o_type) {
				case M2V_UDTA_START_CODE:
					start = gf_m4v_get_object_start(m4v);
					ccdec_queue_data(ctx, ts, (u8*) data + start+4, (u32) (size-start-4), GF_TRUE, GF_FALSE);
					break;
				}
			}
			if (e)
				break;
		}
		gf_m4v_parser_del(m4v);
	} else if (ctx->cctype==CCTYPE_OBU) {
		if (!ctx->bs) ctx->bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
		else gf_bs_reassign_buffer(ctx->bs, data, size);

		while (size) {
			ObuType obu_type = 0;
			u32 obu_hdr_size;
			u8 temporal_id, spatial_id;
			u64 obu_size = 0;
			Bool obu_extension_flag, obu_has_size_field;
			u64 obu_start = (u32) gf_bs_get_position(ctx->bs);

			gf_av1_parse_obu_header(ctx->bs, &obu_type, &obu_extension_flag, &obu_has_size_field, &temporal_id, &spatial_id);
			obu_hdr_size = (u32) (gf_bs_get_position(ctx->bs) - obu_start);

			if (obu_has_size_field) {
				obu_size = (u32)gf_av1_leb128_read(ctx->bs, NULL);
				obu_hdr_size = (u32) (gf_bs_get_position(ctx->bs) - obu_start);
			} else {
				obu_size = size - (u32) gf_bs_get_position(ctx->bs);
			}
			gf_bs_seek(ctx->bs, obu_start);

			if (obu_type==OBU_METADATA) {
				gf_bs_skip_bytes(ctx->bs, obu_hdr_size);
				u32 country_code = gf_bs_read_u8(ctx->bs);
				if (country_code == 0xFF) {
					/*u32 country_code_extension = */gf_bs_read_u8(ctx->bs);
				}
				u32 terminal_provider_code = gf_bs_read_u16(ctx->bs);
				if (terminal_provider_code==GF_ITU_T35_PROVIDER_ATSC) {
					ccdec_queue_data(ctx, ts, (u8*) data + obu_start + obu_hdr_size, (u32) obu_size, GF_FALSE, GF_FALSE);
				}
				gf_bs_seek(ctx->bs, obu_start);
			}
			obu_size += obu_hdr_size;
			if (size < obu_size) break;
			gf_bs_skip_bytes(ctx->bs, obu_size);
			size -= (u32) ( obu_size + obu_hdr_size);
		}
	} else
#endif // GPAC_DISABLE_AV_PARSERS

	//NALU-based: look for SEI messages
	{
		while (size) {
			u32 sei_h_size=0;
			u8 nal_type;
			u32 nal_size = 0;
			u32 v = ctx->nalu_size_len;
			while (v) {
				nal_size |= (u8) *data;
				data++;
				v -= 1;
				if (v) nal_size <<= 8;
			}
			if (ctx->nalu_size_len + nal_size > size) {
				break;
			}
			if (ctx->cctype==CCTYPE_AVC) {
				nal_type = data[0] & 0x1F;
				if (nal_type==GF_AVC_NALU_SEI) sei_h_size = 1;
			} else if (ctx->cctype==CCTYPE_HEVC) {
				nal_type = (data[0] & 0x7E) >> 1;
				if (nal_type==GF_HEVC_NALU_SEI_PREFIX) sei_h_size = 2;
				else if (nal_type==GF_HEVC_NALU_SEI_SUFFIX) sei_h_size = 2;
			} else {
				nal_type = data[1]>>3;
				if (nal_type==GF_VVC_NALU_SEI_PREFIX) sei_h_size = 2;
				else if (nal_type==GF_VVC_NALU_SEI_SUFFIX) sei_h_size = 2;
			}
			//we have an sei message, parse it using EBP removal
			if (sei_h_size) {
				if (!ctx->bs) ctx->bs = gf_bs_new(data, nal_size, GF_BITSTREAM_READ);
				else gf_bs_reassign_buffer(ctx->bs, data, nal_size);
				gf_bs_enable_emulation_byte_removal(ctx->bs, GF_TRUE);

				//skip nal header
				gf_bs_read_int(ctx->bs, sei_h_size*8);

				while (gf_bs_available(ctx->bs) ) {
					u32 i;
					u32 sei_type = 0;
					u32 sei_size = 0;
					while (gf_bs_peek_bits(ctx->bs, 8, 0) == 0xFF) {
						sei_type += 255;
						gf_bs_read_int(ctx->bs, 8);
					}
					sei_type += gf_bs_read_int(ctx->bs, 8);
					while (gf_bs_peek_bits(ctx->bs, 8, 0) == 0xFF) {
						sei_size += 255;
						gf_bs_read_int(ctx->bs, 8);
					}
					sei_size += gf_bs_read_int(ctx->bs, 8);

					i=0;
					if (sei_type == 4) {
						//queuue
						u32 pos = (u32) gf_bs_get_position(ctx->bs);
						u32 country_code = gf_bs_read_u8(ctx->bs);
						i++;
						if (country_code == 0xFF) {
							/*u32 country_code_extension = */gf_bs_read_u8(ctx->bs);
							i++;
						}
						u32 terminal_provider_code = gf_bs_read_u16(ctx->bs);
						i+=2;
						if (terminal_provider_code==GF_ITU_T35_PROVIDER_ATSC) {
							u8 *data = gf_malloc(sizeof(u8)*sei_size);
							gf_bs_seek(ctx->bs, pos);
							i=0;
							while (i<sei_size) {
								data[i] = gf_bs_read_u8(ctx->bs);
								i++;
							}
							ccdec_queue_data(ctx, ts, (u8*) data, sei_size, GF_FALSE, GF_TRUE);
						}
					}
					while (i < sei_size) {
						gf_bs_read_u8(ctx->bs);
						i++;
					}
					if (gf_bs_peek_bits(ctx->bs, 8, 0) == 0x80) {
						break;
					}
				}
			}
			data += nal_size;
			size -= nal_size + ctx->nalu_size_len;
		}
	}

	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static GF_Err ccdec_initialize(GF_Filter *filter)
{
	CCDecCtx *ctx = gf_filter_get_udta(filter);
	ctx->cc_queue = gf_list_new();
	if (!ctx->cc_queue) return GF_OUT_OF_MEM;
	return GF_OK;
}

static void ccdec_finalize(GF_Filter *filter)
{
	CCDecCtx *ctx = gf_filter_get_udta(filter);
	while (gf_list_count(ctx->cc_queue)) {
		CCItem *cc = gf_list_pop_back(ctx->cc_queue);
		gf_free(cc->data);
		gf_free(cc);
	}
	gf_list_del(ctx->cc_queue);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->ccframe) gf_free(ctx->ccframe);
}

static Bool ccdec_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	CCDecCtx *ctx = gf_filter_get_udta(filter);
	if (evt->base.type==GF_FEVT_PLAY) {
		if (ctx->ccframe) caption_frame_init(ctx->ccframe);
	}
	return GF_FALSE;
}

static const GF_FilterCapability CCDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_422),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SNR),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_HIGH),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_MAIN),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SIMPLE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SPATIAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_VVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AV1),

	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_SIMPLE_TEXT),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
};

GF_FilterRegister CCDecRegister = {
	.name = "ccdec",
	GF_FS_SET_DESCRIPTION("Closed-Caption decoder")
	GF_FS_SET_HELP("This filter decodes Closed Captions to unframed SRT.\n"
	"Supported video media types are MPEG2, AVC, HEVC, VVC and AV1 streams.\n"
	"\nOnly a subset of CEA 608/708 is supported.")
	.private_size = sizeof(CCDecCtx),
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	SETCAPS(CCDecCaps),
	.initialize = ccdec_initialize,
	.finalize = ccdec_finalize,
	.process = ccdec_process,
	.configure_pid = ccdec_configure_pid,
	.process_event = ccdec_process_event,
};

const GF_FilterRegister *ccdec_register(GF_FilterSession *session)
{
	return &CCDecRegister;
}
#else //GPAC_HAS_LIBCAPTION
const GF_FilterRegister *ccdec_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_CCDEC
