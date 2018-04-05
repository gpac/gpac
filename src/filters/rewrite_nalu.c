/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / NALU video AnnexB write filter
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

typedef struct
{
	//opts
	Bool rcfg;
	u32 extract;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	Bool is_hevc;

	u32 nal_hdr_size, crc, crc_enh;
	char *dsi;
	u32 dsi_size;

	GF_BitStream *bs_w, *bs_r;
} GF_NALUMxCtx;




static void nalumx_write_ps_list(GF_BitStream *bs, GF_List *list)
{
	u32 i, count = list ? gf_list_count(list) : 0;
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = gf_list_get(list, i);
		gf_bs_write_u32(bs, 1);
		gf_bs_write_data(bs, sl->data, sl->size);
	}
}

static GF_List *nalumx_get_hevc_ps(GF_HEVCConfig *cfg, u8 type)
{
	u32 i, count = gf_list_count(cfg->param_array);
	for (i=0; i<count; i++) {
		GF_HEVCParamArray *pa = gf_list_get(cfg->param_array, i);
		if (pa->type == type) return pa->nalus;
	}
	return NULL;
}

static GF_Err nalumx_make_inband_header(GF_NALUMxCtx *ctx, char *dsi, u32 dsi_len, char *dsi_enh, u32 dsi_enh_len)
{
	GF_BitStream *bs;
	GF_AVCConfig *avcc = NULL;
	GF_AVCConfig *svcc = NULL;
	GF_HEVCConfig *hvcc = NULL;
	GF_HEVCConfig *lvcc = NULL;

	if (ctx->is_hevc) {
		hvcc = gf_odf_hevc_cfg_read(dsi, dsi_len, GF_FALSE);
		if (dsi_enh) lvcc = gf_odf_hevc_cfg_read(dsi_enh, dsi_enh_len, GF_TRUE);
		if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;
		ctx->nal_hdr_size = hvcc->nal_unit_size;
	} else {
		avcc = gf_odf_avc_cfg_read(dsi, dsi_len);
		if (dsi_enh) svcc = gf_odf_avc_cfg_read(dsi_enh, dsi_enh_len);
		if (!avcc) return GF_NON_COMPLIANT_BITSTREAM;
		ctx->nal_hdr_size = avcc->nal_unit_size;
	}

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (avcc || svcc) {
		if (avcc)
			nalumx_write_ps_list(bs, avcc->sequenceParameterSets);

		if (svcc)
			nalumx_write_ps_list(bs, svcc->sequenceParameterSets);

		if (avcc && avcc->sequenceParameterSetExtensions)
			nalumx_write_ps_list(bs, avcc->sequenceParameterSetExtensions);

		if (svcc && svcc->sequenceParameterSetExtensions)
			nalumx_write_ps_list(bs, svcc->sequenceParameterSetExtensions);

		if (avcc)
			nalumx_write_ps_list(bs, avcc->pictureParameterSets);

		if (svcc)
			nalumx_write_ps_list(bs, svcc->pictureParameterSets);
	}
	if (hvcc || lvcc) {
		if (hvcc)
			nalumx_write_ps_list(bs, nalumx_get_hevc_ps(hvcc, GF_HEVC_NALU_VID_PARAM));
		if (lvcc)
			nalumx_write_ps_list(bs, nalumx_get_hevc_ps(lvcc, GF_HEVC_NALU_VID_PARAM));
		if (hvcc)
			nalumx_write_ps_list(bs, nalumx_get_hevc_ps(hvcc, GF_HEVC_NALU_SEQ_PARAM));
		if (lvcc)
			nalumx_write_ps_list(bs, nalumx_get_hevc_ps(lvcc, GF_HEVC_NALU_SEQ_PARAM));
		if (hvcc)
			nalumx_write_ps_list(bs, nalumx_get_hevc_ps(hvcc, GF_HEVC_NALU_PIC_PARAM));
		if (lvcc)
			nalumx_write_ps_list(bs, nalumx_get_hevc_ps(lvcc, GF_HEVC_NALU_PIC_PARAM));
	}
	if (ctx->dsi) gf_free(ctx->dsi);
	gf_bs_get_content(bs, &ctx->dsi, &ctx->dsi_size);
	gf_bs_del(bs);

	if (avcc) gf_odf_avc_cfg_del(avcc);
	if (svcc) gf_odf_avc_cfg_del(svcc);
	if (hvcc) gf_odf_hevc_cfg_del(hvcc);
	if (lvcc) gf_odf_hevc_cfg_del(lvcc);

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);

	return GF_OK;
}

GF_Err nalumx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 crc, crc_enh, codecid;
	const GF_PropertyValue *p, *dcd, *dcd_enh;
	GF_NALUMxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		gf_filter_pid_remove(ctx->opid);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_NOT_SUPPORTED;
	codecid = p->value.uint;

	//it may happen we don't have anything yet ...
	dcd = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!dcd) {
		crc = -1;
	} else {
		crc = gf_crc_32(dcd->value.data.ptr, dcd->value.data.size);
	}
	dcd_enh = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);

	crc_enh = dcd_enh ? gf_crc_32(dcd_enh->value.data.ptr, dcd_enh->value.data.size) : 0;
	if ((ctx->crc == crc) && (ctx->crc_enh == crc_enh)) return GF_OK;
	ctx->crc = crc;
	ctx->crc_enh = crc_enh;

	if ((codecid==GF_CODECID_HEVC) || (codecid==GF_CODECID_LHVC)) {
		ctx->is_hevc = GF_TRUE;
	} else {
		ctx->is_hevc = GF_FALSE;
	}

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL );

	ctx->ipid = pid;
	if (!dcd)
		return GF_OK;

	return nalumx_make_inband_header(ctx, dcd->value.data.ptr, dcd->value.data.size, dcd_enh ? dcd_enh->value.data.ptr : NULL, dcd_enh ? dcd_enh->value.data.size : 0);
}


static Bool nalumx_is_nal_skip(GF_NALUMxCtx *ctx, char *data, u32 pos)
{
	Bool is_layer = GF_FALSE;
	if (ctx->is_hevc) {
		u8 nal_type = (data[pos] & 0x7E) >> 1;
//		u8 temporal_id = data[pos+1] & 0x7;
		u8 layer_id = data[pos] & 1;
		layer_id <<= 5;
		layer_id |= (data[pos+1]>>3) & 0x1F;
		switch (nal_type) {
		case GF_HEVC_NALU_VID_PARAM:
			break;
		default:
			if (layer_id) is_layer = GF_TRUE;
			break;
		}
	} else {
		u32 nal_type = data[pos] & 0x1F;
		switch (nal_type) {
		case GF_AVC_NALU_SVC_PREFIX_NALU:
		case GF_AVC_NALU_SVC_SLICE:
		case GF_AVC_NALU_VDRD:
		case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
		case GF_AVC_NALU_SEQ_PARAM:
		case GF_AVC_NALU_SEQ_PARAM_EXT:
		case GF_AVC_NALU_PIC_PARAM:
			is_layer = GF_TRUE;
			break;
		default:
			break;
		}
	}
	if (ctx->extract==1) return is_layer ? GF_TRUE : GF_FALSE;
	else if (ctx->extract==2) return is_layer ? GF_FALSE : GF_TRUE;
	return GF_FALSE;
}

GF_Err nalumx_process(GF_Filter *filter)
{
	GF_NALUMxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	char *data, *output;
	u32 pck_size, size;
	Bool insert_dsi = GF_FALSE;
	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);

	if (!ctx->bs_r) ctx->bs_r = gf_bs_new(data, pck_size, GF_BITSTREAM_READ);
	else gf_bs_reassign_buffer(ctx->bs_r, data, pck_size);

	size = 0;

	while (gf_bs_available((ctx->bs_r))) {
		Bool skip_nal = GF_FALSE;
		u32 nal_size = gf_bs_read_int(ctx->bs_r, 8*ctx->nal_hdr_size);
		if (nal_size > gf_bs_available(ctx->bs_r) ) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (ctx->extract) {
			u32 pos = gf_bs_get_position(ctx->bs_r);
			skip_nal = nalumx_is_nal_skip(ctx, data, pos);
		}
		if (!skip_nal)
			size += nal_size + 4;

		gf_bs_skip_bytes(ctx->bs_r, nal_size);
	}
	gf_bs_seek(ctx->bs_r, 0);

	//TODO check if we need to insert NALU delim

	if ((gf_filter_pck_get_sap(pck) <= GF_FILTER_SAP_3) && ctx->dsi) {
		insert_dsi = GF_TRUE;
		size += ctx->dsi_size;
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);

	if (!ctx->bs_w) ctx->bs_w = gf_bs_new(output, size, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs_w, output, size);

	if (insert_dsi) {
		gf_bs_write_data(ctx->bs_w, ctx->dsi, ctx->dsi_size);

		if (!ctx->rcfg) {
			gf_free(ctx->dsi);
			ctx->dsi = NULL;
			ctx->dsi_size = 0;
		}
	}

	while (gf_bs_available((ctx->bs_r))) {
		u32 pos;
		Bool skip_nal = GF_FALSE;
		u32 nal_size = gf_bs_read_int(ctx->bs_r, 8*ctx->nal_hdr_size);
		if (nal_size > gf_bs_available(ctx->bs_r) ) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		pos = gf_bs_get_position(ctx->bs_r);

		if (ctx->extract) {
			skip_nal = nalumx_is_nal_skip(ctx, data, pos);
		}
		if (!skip_nal) {
			gf_bs_write_u32(ctx->bs_w, 1);
			gf_bs_write_data(ctx->bs_w, data+pos, nal_size);
		}

		gf_bs_skip_bytes(ctx->bs_r, nal_size);
	}
	gf_filter_pck_merge_properties(pck, dst_pck);
	gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);

	gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
	gf_filter_pck_send(dst_pck);

	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static void nalumx_finalize(GF_Filter *filter)
{
	GF_NALUMxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs_r) gf_bs_del(ctx->bs_r);
	if (ctx->bs_w) gf_bs_del(ctx->bs_w);
	if (ctx->dsi) gf_free(ctx->dsi);
}

static const GF_FilterCapability NALUMxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC_PS),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_MVC),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
};


#define OFFS(_n)	#_n, offsetof(GF_NALUMxCtx, _n)
static const GF_FilterArgs NALUMxArgs[] =
{
	{ OFFS(rcfg), "Force repeating decoder config at each I-frame", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(extract), "Extracts full, base or layer only", GF_PROP_UINT, "all", "all|base|layer", GF_FALSE},
	{}
};


GF_FilterRegister NALUMxRegister = {
	.name = "write_nal",
	.description = "ISOBMFF to NALU writer for AVC|H264 and HEVC",
	.private_size = sizeof(GF_NALUMxCtx),
	.args = NALUMxArgs,
	.finalize = nalumx_finalize,
	SETCAPS(NALUMxCaps),
	.configure_pid = nalumx_configure_pid,
	.process = nalumx_process
};


const GF_FilterRegister *nalumx_register(GF_FilterSession *session)
{
	return &NALUMxRegister;
}
