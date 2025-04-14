/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2025
 *					All rights reserved
 *
 *  This file is part of GPAC / SEI loader
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
#include <gpac/internal/media_dev.h>
#include <gpac/mpeg4_odf.h>

#ifndef GPAC_DISABLE_AV_PARSERS

enum
{
	COD_TYPE_AVC = 1,
	COD_TYPE_HEVC,
	COD_TYPE_VVC,
	COD_TYPE_AV1,
};

struct _gf_sei_loader
{
	Bool is_identity, external;
	u32 codec_type, nalu_size;

	AVCState *avc_state;
	HEVCState *hevc_state;
	VVCState *vvc_state;
	AV1State *av1_state;
	GF_BitStream *full_bs, *bs;
};



static GF_Err seiloader_set_type(GF_SEILoader *sei, u32 type)
{
	if (type==sei->codec_type) return GF_OK;
	if (sei->external) {
		sei->external = GF_FALSE;
		sei->avc_state = NULL;
		sei->hevc_state = NULL;
		sei->vvc_state = NULL;
	}
	else if (type==sei->codec_type)
		return GF_OK;

	sei->codec_type = 0;
	if (type==COD_TYPE_AVC) {
		if (sei->hevc_state) { gf_free(sei->hevc_state); sei->hevc_state = NULL; }
		if (sei->vvc_state) { gf_free(sei->vvc_state); sei->vvc_state = NULL; }
		if (sei->av1_state) { gf_av1_reset_state(sei->av1_state, GF_TRUE); gf_free(sei->av1_state); sei->vvc_state = NULL; }
		GF_SAFEALLOC(sei->avc_state, AVCState);
		if (!sei->avc_state) return GF_OUT_OF_MEM;
	}
	if (type==COD_TYPE_HEVC) {
		if (sei->avc_state) { gf_free(sei->avc_state); sei->avc_state = NULL; }
		if (sei->vvc_state) { gf_free(sei->vvc_state); sei->vvc_state = NULL; }
		if (sei->av1_state) { gf_av1_reset_state(sei->av1_state, GF_TRUE); gf_free(sei->av1_state); sei->vvc_state = NULL; }
		GF_SAFEALLOC(sei->hevc_state, HEVCState);
		if (!sei->hevc_state) return GF_OUT_OF_MEM;
	}
	if (type==COD_TYPE_VVC) {
		if (sei->hevc_state) { gf_free(sei->hevc_state); sei->hevc_state = NULL; }
		if (sei->avc_state) { gf_free(sei->avc_state); sei->avc_state = NULL; }
		if (sei->av1_state) { gf_av1_reset_state(sei->av1_state, GF_TRUE); gf_free(sei->av1_state); sei->vvc_state = NULL; }
		GF_SAFEALLOC(sei->vvc_state, VVCState);
		if (!sei->vvc_state) return GF_OUT_OF_MEM;
	}
	if (type==COD_TYPE_AV1) {
		if (sei->avc_state) { gf_free(sei->avc_state); sei->avc_state = NULL; }
		if (sei->hevc_state) { gf_free(sei->hevc_state); sei->hevc_state = NULL; }
		if (sei->vvc_state) { gf_free(sei->vvc_state); sei->vvc_state = NULL; }
		GF_SAFEALLOC(sei->av1_state, AV1State);
		if (!sei->av1_state) return GF_OUT_OF_MEM;
		gf_av1_init_state(sei->av1_state);
	}
	if (type==0) {
		if (sei->avc_state) { gf_free(sei->avc_state); sei->avc_state = NULL; }
		if (sei->hevc_state) { gf_free(sei->hevc_state); sei->hevc_state = NULL; }
		if (sei->vvc_state) { gf_free(sei->vvc_state); sei->vvc_state = NULL; }
		if (sei->av1_state) { gf_av1_reset_state(sei->av1_state, GF_TRUE); gf_free(sei->av1_state); sei->vvc_state = NULL; }
	}
	sei->codec_type = type;
	return GF_OK;
}

static void parse_param_list(GF_SEILoader *sei, GF_List *params)
{
	u32 i, count = gf_list_count(params);
	for (i=0; i<count; i++) {
		GF_NALUFFParam *p = gf_list_get(params, i);
		gf_bs_reassign_buffer(sei->bs, p->data, p->size);
		if (sei->avc_state) {
			gf_avc_parse_nalu(sei->bs, sei->avc_state);
		} else if (sei->hevc_state) {
			u8 nal_unit_type, temporal_id, layer_id;
			gf_hevc_parse_nalu_bs(sei->bs, sei->hevc_state, &nal_unit_type, &temporal_id, &layer_id);
		}
		else if (sei->vvc_state) {
			u8 nal_unit_type, temporal_id, layer_id;
			gf_vvc_parse_nalu_bs(sei->bs, sei->vvc_state, &nal_unit_type, &temporal_id, &layer_id);
		}
	}
}

GF_Err gf_sei_init_from_pid(GF_SEILoader *sei, GF_FilterPid *pid)
{
	u32 i, count;
	sei->is_identity = GF_TRUE;
	const GF_PropertyValue *p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_OK;
	u32 codec_id = p->value.uint;
	Bool is_enh = GF_FALSE;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!p) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);
		if (p) is_enh = GF_TRUE;
	}
	if (!p) return GF_OK;

	sei->nalu_size = 0;

	switch (codec_id) {
	case GF_CODECID_AVC:
	{
		GF_AVCConfig *avcc = gf_odf_avc_cfg_read(p->value.data.ptr, p->value.data.size);
		if (avcc) {
			seiloader_set_type(sei, COD_TYPE_AVC);
			sei->nalu_size = avcc->nal_unit_size;
			parse_param_list(sei, avcc->sequenceParameterSets);
			parse_param_list(sei, avcc->sequenceParameterSetExtensions);
			parse_param_list(sei, avcc->pictureParameterSets);
			gf_odf_avc_cfg_del(avcc);
		}
		sei->is_identity = GF_FALSE;
	}
		break;
	case GF_CODECID_HEVC:
	{
		GF_HEVCConfig *hvcc = gf_odf_hevc_cfg_read(p->value.data.ptr, p->value.data.size, is_enh);
		if (hvcc) {
			seiloader_set_type(sei, COD_TYPE_HEVC);
			sei->nalu_size = hvcc->nal_unit_size;
			count = gf_list_count(hvcc->param_array);
			for (i=0; i<count; i++) {
				GF_NALUFFParamArray *pa = gf_list_get(hvcc->param_array, i);
				parse_param_list(sei, pa->nalus);
			}
			gf_odf_hevc_cfg_del(hvcc);
		}
		sei->is_identity = GF_FALSE;
	}
		break;
	case GF_CODECID_VVC:
	{
		GF_VVCConfig *vvcc = gf_odf_vvc_cfg_read(p->value.data.ptr, p->value.data.size);
		if (vvcc) {
			seiloader_set_type(sei, COD_TYPE_VVC);
			sei->nalu_size = vvcc->nal_unit_size;
			count = gf_list_count(vvcc->param_array);
			for (i=0; i<count; i++) {
				GF_NALUFFParamArray *pa = gf_list_get(vvcc->param_array, i);
				parse_param_list(sei, pa->nalus);
			}
			gf_odf_vvc_cfg_del(vvcc);
		}
	}
		sei->is_identity = GF_FALSE;
		break;
	case GF_CODECID_AV1:
	{
		GF_AV1Config *av1c = gf_odf_av1_cfg_read(p->value.data.ptr, p->value.data.size);
		if (av1c) {
			seiloader_set_type(sei, COD_TYPE_AV1);
			sei->av1_state->config = av1c;
			//for now no need to load the OBUs
		}
		gf_odf_av1_cfg_del(av1c);
	}
		sei->is_identity = GF_FALSE;
		break;

	}

	return GF_OK;
}

static GFINLINE GF_SEIInfo *get_sei_info(GF_SEILoader *sei)
{
	if (sei->avc_state) return &sei->avc_state->sei;
	if (sei->hevc_state) return &sei->hevc_state->sei;
	if (sei->vvc_state) return &sei->vvc_state->sei;
	if (sei->av1_state) return &sei->av1_state->sei;
	return NULL;
}

static GF_Err gf_sei_load_from_state_internal(GF_SEILoader *ctx, GF_FilterPacket *pck, Bool skip_check)
{
	GF_SEIInfo *sei = get_sei_info(ctx);
	if (!sei) return GF_OK;
	if (!skip_check && gf_filter_pck_get_property(pck, GF_PROP_PCK_SEI_LOADED))
		return GF_OK;

	skip_check = GF_FALSE;
	if (sei->clli_valid) {
		gf_filter_pck_set_property(pck, GF_PROP_PCK_CONTENT_LIGHT_LEVEL, &PROP_DATA(sei->clli_data, 4));
		sei->clli_valid = 0;
		skip_check = GF_TRUE;
	}
	if (sei->mdcv_valid) {
		if (ctx->av1_state) {
			u8 rw_mdcv[24];
			gf_av1_format_mdcv_to_mpeg(sei->mdcv_data, rw_mdcv);
			gf_filter_pck_set_property(pck, GF_PROP_PCK_MASTER_DISPLAY_COLOUR, &PROP_DATA(rw_mdcv, 24));
		} else {
			gf_filter_pck_set_property(pck, GF_PROP_PCK_MASTER_DISPLAY_COLOUR, &PROP_DATA(sei->mdcv_data, 24));
			sei->mdcv_valid = 0;
		}
		skip_check = GF_TRUE;
	}
	if (sei->pic_timing.num_clock_ts && sei->pic_timing.timecodes[0].clock_timestamp_flag) {
		AVCSeiPicTimingTimecode *a_tc = &sei->pic_timing.timecodes[0];

		GF_TimeCode tc_dst = {0};
		tc_dst.hours = a_tc->hours;
		tc_dst.minutes = a_tc->minutes;
		tc_dst.seconds = a_tc->seconds;
		tc_dst.n_frames = a_tc->n_frames;
		tc_dst.max_fps = a_tc->max_fps;
		tc_dst.drop_frame = (a_tc->counting_type==4 && a_tc->cnt_dropped_flag) ? 1 : 0;
		tc_dst.counting_type = a_tc->counting_type;
		gf_filter_pck_set_property(pck, GF_PROP_PCK_TIMECODE, &PROP_DATA((u8*)&tc_dst, sizeof(GF_TimeCode)));

		sei->pic_timing.num_clock_ts = 0;
		skip_check = GF_TRUE;
	}

	if (skip_check)
		gf_filter_pck_set_property(pck, GF_PROP_PCK_SEI_LOADED, &PROP_BOOL(GF_TRUE));

	return GF_OK;
}
GF_Err gf_sei_load_from_state(GF_SEILoader *ctx, GF_FilterPacket *pck)
{
	return gf_sei_load_from_state_internal(ctx, pck, GF_FALSE);
}

static GF_Err gf_sei_load_from_packet_nalu(GF_SEILoader *sei, GF_FilterPacket *pck, Bool *needs_load)
{
	u32 data_len;
	u8 *data = (u8*) gf_filter_pck_get_data(pck, &data_len);

	u32 pos=0;
	while (pos<data_len) {
		if (pos + sei->nalu_size >= data_len) break;
		u32 tmp=0, size = 0;
		while (tmp<sei->nalu_size-1) {
			size |= data[pos+tmp];
			tmp++;
			size<<=8;
		}
		size |= data[pos+tmp];
		//we allow nal_size=0 for incomplete files, abort as soon as we see one to avoid parsing thousands of 0 bytes
		if (!size) break;
		if (pos+sei->nalu_size+size > data_len) break;

		gf_bs_reassign_buffer(sei->bs, data+pos+sei->nalu_size, size);
		Bool is_sei = GF_FALSE;

		if (sei->avc_state) {
			u32 nal_type = gf_bs_peek_bits(sei->bs, 8, 0) & 0x1F;
			switch (nal_type) {
			case GF_AVC_NALU_SEI:
				is_sei = GF_TRUE;
				gf_avc_reformat_sei(data+pos+sei->nalu_size, size, GF_FALSE, sei->avc_state, NULL);
				break;
			case GF_AVC_NALU_SEQ_PARAM: //mandatory for pic timing parsing
			case GF_AVC_NALU_SEQ_PARAM_EXT:
				gf_avc_parse_nalu(sei->bs, sei->avc_state);
				break;
			}
		} else if (sei->hevc_state) {
			u8 temporal_id, layer_id;
			u8 nal_type = (gf_bs_peek_bits(sei->bs, 8, 0) & 0x7E) >> 1;
			switch (nal_type) {
			case GF_HEVC_NALU_SEI_PREFIX:
			case GF_HEVC_NALU_SEI_SUFFIX:
				is_sei = GF_TRUE;
			case GF_HEVC_NALU_SEQ_PARAM: //for pic timing full parse we need SPS...
				gf_hevc_parse_nalu_bs(sei->bs, sei->hevc_state, &nal_type, &temporal_id, &layer_id);
				break;
			}
		}
		else if (sei->vvc_state) {
			u8 temporal_id, layer_id;
			u8 nal_unit_type = gf_bs_peek_bits(sei->bs, 8, 1) >> 3;
			switch (nal_unit_type) {
			case GF_HEVC_NALU_SEI_PREFIX:
			case GF_HEVC_NALU_SEI_SUFFIX:
				is_sei = GF_TRUE;
				gf_vvc_parse_nalu_bs(sei->bs, sei->vvc_state, &nal_unit_type, &temporal_id, &layer_id);
				break;
			}
		}

		if (!is_sei) {
			pos += sei->nalu_size + data_len;
			continue;
		}
		*needs_load = GF_TRUE;
#if 0
		//if needed, load SEIs not handled by avparser ?
		gf_bs_seek(sei->bs, 0);
		//load SEIs
		while (gf_bs_available(sei->bs) ) {
			u32 sei_type = 0;
			u32 sei_size = 0;
			u32 sei_pos;
			while (gf_bs_peek_bits(sei->bs, 8, 0) == 0xFF) {
				sei_type += 255;
				gf_bs_read_int(sei->bs, 8);
			}
			sei_type += gf_bs_read_int(sei->bs, 8);
			while (gf_bs_peek_bits(sei->bs, 8, 0) == 0xFF) {
				sei_size += 255;
				gf_bs_read_int(sei->bs, 8);
			}
			sei_size += gf_bs_read_int(sei->bs, 8);
			sei_pos = (u32) gf_bs_get_position(sei->bs);

			//take actions here

			gf_bs_seek(sei->bs, sei_pos);
			while (sei_size && gf_bs_available(sei->bs)) {
				gf_bs_read_u8(sei->bs);
				sei_size--;
			}
			if (gf_bs_peek_bits(sei->bs, 8, 0) == 0x80) {
				break;
			}
		}
#endif

		pos += sei->nalu_size + data_len;
	}
	return GF_OK;
}

static GF_Err gf_sei_load_from_packet_av1(GF_SEILoader *sei, GF_FilterPacket *pck, Bool *needs_load)
{
	u32 data_len;
	u8 *data = (u8*) gf_filter_pck_get_data(pck, &data_len);

	while (data_len) {
		ObuType obu_type = 0;
		u64 obu_size = 0;
		u32 hdr_size = 0;
		gf_bs_reassign_buffer(sei->bs, data, data_len);

		sei->av1_state->parse_metadata_filter = 1;
		GF_Err e = gf_av1_parse_obu(sei->bs, &obu_type, &obu_size, &hdr_size, sei->av1_state);
		if (e) break;
		if (sei->av1_state->parse_metadata_filter == 2)
			*needs_load = GF_TRUE;

		if (!obu_size || (obu_size > data_len)) {
			break;
		}

		data += obu_size;
		data_len -= (u32)obu_size;
	}
	return GF_OK;
}

GF_Err gf_sei_load_from_packet(GF_SEILoader *sei, GF_FilterPacket *pck)
{
	Bool needs_load = GF_FALSE;
	GF_Err e = GF_OK;
	if (sei->is_identity)
		return GF_OK;

	if (sei->nalu_size)
		e = gf_sei_load_from_packet_nalu(sei, pck, &needs_load);
	else if (sei->av1_state)
		e = gf_sei_load_from_packet_av1(sei, pck, &needs_load);

	if (e) return e;
	if (needs_load)
		return gf_sei_load_from_state_internal(sei, pck, GF_TRUE);
	return GF_OK;
}

GF_Err gf_sei_init_from_avc(GF_SEILoader *sei, AVCState *avc)
{
	if (!sei || !avc) return GF_BAD_PARAM;
	seiloader_set_type(sei, 0);
	sei->avc_state = avc;
	sei->external = GF_TRUE;
	return GF_OK;
}
GF_Err gf_sei_init_from_hevc(GF_SEILoader *sei, HEVCState *hevc)
{
	if (!sei || !hevc) return GF_BAD_PARAM;
	seiloader_set_type(sei, 0);
	sei->hevc_state = hevc;
	sei->external = GF_TRUE;
	return GF_OK;
}

GF_Err gf_sei_init_from_vvc(GF_SEILoader *sei, VVCState *vvc)
{
	if (!sei || !vvc) return GF_BAD_PARAM;
	seiloader_set_type(sei, 0);
	sei->vvc_state = vvc;
	sei->external = GF_TRUE;
	return GF_OK;
}
GF_Err gf_sei_init_from_av1(GF_SEILoader *sei, AV1State *av1)
{
	if (!sei || !av1) return GF_BAD_PARAM;
	seiloader_set_type(sei, 0);
	sei->av1_state = av1;
	sei->external = GF_TRUE;
	return GF_OK;
}

GF_SEILoader *gf_sei_loader_new()
{
	GF_SEILoader *sei;

	GF_SAFEALLOC(sei, GF_SEILoader);
	if (!sei) return NULL;
	sei->bs = gf_bs_new((u8*)sei, 1, GF_BITSTREAM_READ);
	if (!sei->bs) {
		gf_free(sei);
		return NULL;
	}
	return sei;
}

void gf_sei_loader_del(GF_SEILoader *sei)
{
	if (!sei) return;
	seiloader_set_type(sei, 0);
	gf_free(sei->bs);
	gf_free(sei);
}

#endif


#ifndef GPAC_DISABLE_SEILOAD

typedef struct
{
	GF_List *loaders;
} SEILoadCtx;


static GF_Err seiload_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	SEILoadCtx *ctx = (SEILoadCtx *) gf_filter_get_udta(filter);
	GF_FilterPid *opid = gf_filter_pid_get_udta(pid);
	GF_SEILoader *loader = opid ? gf_filter_pid_get_udta(opid) : NULL;
	if (is_remove) {
		if (loader) {
			gf_filter_pid_set_udta(opid, NULL);
			gf_list_del_item(ctx->loaders, loader);
			gf_sei_loader_del(loader);
		}
		if (opid) gf_filter_pid_remove(opid);
		return GF_OK;
	}
	if (!opid) {
		opid = gf_filter_pid_new(filter);
		loader = gf_sei_loader_new();
		gf_filter_pid_set_udta(pid, opid);
		gf_filter_pid_set_udta(opid, loader);
		gf_list_add(ctx->loaders, loader);
	}
	gf_filter_pid_copy_properties(opid, pid);
	gf_filter_pid_set_property(opid, GF_PROP_PID_SEI_LOADED, &PROP_BOOL(GF_TRUE) );
	return gf_sei_init_from_pid(loader, opid);
}
static GF_Err seiload_process(GF_Filter *filter)
{
	u32 i, nb_eos=0, count = gf_filter_get_ipid_count(filter);
	for (i=0; i<count; i++) {
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		GF_FilterPid *opid = gf_filter_pid_get_udta(ipid);
		GF_SEILoader *loader = opid ? gf_filter_pid_get_udta(opid) : NULL;
		while (1) {
			GF_FilterPacket *pck = gf_filter_pid_get_packet(ipid);
			if (!pck) {
				if (gf_filter_pid_is_eos(ipid)) {
					gf_filter_pid_set_eos(opid);
					nb_eos++;
				}
				break;
			}
			if (loader->is_identity
				|| (gf_filter_pck_get_property(pck, GF_PROP_PCK_SEI_LOADED)!=NULL)
			) {
				gf_filter_pck_forward(pck, opid);
			} else {
				GF_FilterPacket *opck = gf_filter_pck_new_ref(opid, 0, 0, pck);
				gf_filter_pck_merge_properties(pck, opck);
				gf_sei_load_from_packet(loader, opck);
				gf_filter_pck_send(opck);
			}
			gf_filter_pid_drop_packet(ipid);
		}
	}
	if (nb_eos==count) return GF_EOS;
	return GF_OK;
}


static GF_Err seiload_reconfigure_output(GF_Filter *filter, GF_FilterPid *opid)
{
	const GF_PropertyValue *p = gf_filter_pid_caps_query(opid, GF_PROP_PID_SEI_LOADED);
	if (p) return GF_OK;
	return GF_NOT_SUPPORTED;
}

static GF_Err seiload_initialize(GF_Filter *filter)
{
	SEILoadCtx *ctx = (SEILoadCtx *) gf_filter_get_udta(filter);
	ctx->loaders = gf_list_new();
	return GF_OK;
}
static void seiload_finalize(GF_Filter *filter)
{
	SEILoadCtx *ctx = (SEILoadCtx *) gf_filter_get_udta(filter);
	while (gf_list_count(ctx->loaders)) {
		GF_SEILoader *pctx = gf_list_pop_back(ctx->loaders);
		gf_sei_loader_del(pctx);
	}
	gf_list_del(ctx->loaders);
}

#define OFFS(_n)	#_n, offsetof(SEILoadCtx, _n)
static GF_FilterArgs SEILoadArgs[] =
{
	{0}
};

static const GF_FilterCapability SEILoadCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	CAP_BOOL(GF_CAPFLAG_RECONFIG, GF_PROP_PID_SEI_LOADED, GF_TRUE),
};

GF_FilterRegister SEILoadRegister = {
	.name = "seiload",
	GF_FS_SET_DESCRIPTION("SEI message loader")
	GF_FS_SET_HELP("This filter loads known inband metadata as packet properties\n"
	)
	.private_size = sizeof(SEILoadCtx),
	.max_extra_pids = 0xFFFFFFFF,
	.args = SEILoadArgs,
	SETCAPS(SEILoadCaps),
	.initialize = seiload_initialize,
	.finalize = seiload_finalize,
	.configure_pid = seiload_configure_pid,
	.process = seiload_process,
	.reconfigure_output = seiload_reconfigure_output,
	//assign lowest priority so that we don't get picked up in graph solving
	//if a better chain is possible
	.priority = 0x7FFF,
	.hint_class_type = GF_FS_CLASS_STREAM
};

const GF_FilterRegister *seiload_register(GF_FilterSession *session)
{
	return (const GF_FilterRegister *) &SEILoadRegister;
}
#else
const GF_FilterRegister *seiload_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_SEILOAD

