/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2021
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

enum
{
	UFNAL_AVC,
	UFNAL_HEVC,
	UFNAL_VVC,
};

typedef struct
{
	//opts
	Bool rcfg, delim, pps_inband;
	u32 extract;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	u32 vtype;

	u32 nal_hdr_size, crc, crc_enh;
	u8 *dsi, *dsi_non_rap;
	u32 dsi_size, dsi_non_rap_size;

	GF_BitStream *bs_w, *bs_r;
	u32 nb_nalu, nb_nalu_in_hdr, nb_nalu_in_hdr_non_rap;
	u32 width, height;
} GF_NALUMxCtx;




static void nalumx_write_ps_list(GF_NALUMxCtx *ctx, GF_BitStream *bs, GF_List *list, Bool for_non_rap)
{
	u32 i, count = list ? gf_list_count(list) : 0;
	for (i=0; i<count; i++) {
		GF_NALUFFParam *sl = gf_list_get(list, i);
		gf_bs_write_u32(bs, 1);
		gf_bs_write_data(bs, sl->data, sl->size);
		if (for_non_rap)
			ctx->nb_nalu_in_hdr_non_rap++;
		else
			ctx->nb_nalu_in_hdr++;
	}
}

static GF_List *nalumx_get_hevc_ps(GF_HEVCConfig *cfg, u8 type)
{
	u32 i, count = gf_list_count(cfg->param_array);
	for (i=0; i<count; i++) {
		GF_NALUFFParamArray *pa = gf_list_get(cfg->param_array, i);
		if (pa->type == type) return pa->nalus;
	}
	return NULL;
}
static GF_List *nalumx_get_vvc_ps(GF_VVCConfig *cfg, u8 type)
{
	u32 i, count = gf_list_count(cfg->param_array);
	for (i=0; i<count; i++) {
		GF_NALUFFParamArray *pa = gf_list_get(cfg->param_array, i);
		if (pa->type == type) return pa->nalus;
	}
	return NULL;
}

static GF_Err nalumx_make_inband_header(GF_NALUMxCtx *ctx, char *dsi, u32 dsi_len, char *dsi_enh, u32 dsi_enh_len, Bool for_non_rap)
{
	GF_BitStream *bs;
	GF_AVCConfig *avcc = NULL;
	GF_AVCConfig *svcc = NULL;
	GF_HEVCConfig *hvcc = NULL;
	GF_HEVCConfig *lvcc = NULL;
	GF_VVCConfig *vvcc = NULL;
	GF_VVCConfig *s_vvcc = NULL;

	if (ctx->vtype==UFNAL_HEVC) {
		if (dsi) hvcc = gf_odf_hevc_cfg_read(dsi, dsi_len, GF_FALSE);
		if (dsi_enh) lvcc = gf_odf_hevc_cfg_read(dsi_enh, dsi_enh_len, GF_TRUE);
		if (!hvcc && !lvcc) return GF_NON_COMPLIANT_BITSTREAM;
		ctx->nal_hdr_size = hvcc ? hvcc->nal_unit_size : lvcc->nal_unit_size;
	} else if (ctx->vtype==UFNAL_VVC) {
		if (dsi) vvcc = gf_odf_vvc_cfg_read(dsi, dsi_len);
		if (dsi_enh) s_vvcc = gf_odf_vvc_cfg_read(dsi_enh, dsi_enh_len);
		if (!vvcc && !s_vvcc) return GF_NON_COMPLIANT_BITSTREAM;
		ctx->nal_hdr_size = vvcc ? vvcc->nal_unit_size : s_vvcc->nal_unit_size;
	} else {
		if (dsi) avcc = gf_odf_avc_cfg_read(dsi, dsi_len);
		if (dsi_enh) svcc = gf_odf_avc_cfg_read(dsi_enh, dsi_enh_len);
		if (!avcc && !svcc) return GF_NON_COMPLIANT_BITSTREAM;
		ctx->nal_hdr_size = avcc ? avcc->nal_unit_size : svcc->nal_unit_size;
	}
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (avcc || svcc) {
		if (avcc && !for_non_rap) {
			nalumx_write_ps_list(ctx, bs, avcc->sequenceParameterSets, for_non_rap);
		}

		if (svcc && !for_non_rap)
			nalumx_write_ps_list(ctx, bs, svcc->sequenceParameterSets, for_non_rap);

		if (avcc && avcc->sequenceParameterSetExtensions && !for_non_rap)
			nalumx_write_ps_list(ctx, bs, avcc->sequenceParameterSetExtensions, for_non_rap);

		if (svcc && svcc->sequenceParameterSetExtensions && !for_non_rap)
			nalumx_write_ps_list(ctx, bs, svcc->sequenceParameterSetExtensions, for_non_rap);

		if (avcc)
			nalumx_write_ps_list(ctx, bs, avcc->pictureParameterSets, for_non_rap);

		if (svcc)
			nalumx_write_ps_list(ctx, bs, svcc->pictureParameterSets, for_non_rap);
	}
	if (hvcc || lvcc) {
		if (hvcc && !for_non_rap)
			nalumx_write_ps_list(ctx, bs, nalumx_get_hevc_ps(hvcc, GF_HEVC_NALU_VID_PARAM), for_non_rap);
		if (lvcc && !for_non_rap)
			nalumx_write_ps_list(ctx, bs, nalumx_get_hevc_ps(lvcc, GF_HEVC_NALU_VID_PARAM), for_non_rap);
		if (hvcc && !for_non_rap)
			nalumx_write_ps_list(ctx, bs, nalumx_get_hevc_ps(hvcc, GF_HEVC_NALU_SEQ_PARAM), for_non_rap);
		if (lvcc && !for_non_rap)
			nalumx_write_ps_list(ctx, bs, nalumx_get_hevc_ps(lvcc, GF_HEVC_NALU_SEQ_PARAM), for_non_rap);
		if (hvcc)
			nalumx_write_ps_list(ctx, bs, nalumx_get_hevc_ps(hvcc, GF_HEVC_NALU_PIC_PARAM), for_non_rap);
		if (lvcc)
			nalumx_write_ps_list(ctx, bs, nalumx_get_hevc_ps(lvcc, GF_HEVC_NALU_PIC_PARAM), for_non_rap);
	}

	if (vvcc || s_vvcc) {
		u32 dump_types[] = {GF_VVC_NALU_VID_PARAM, GF_VVC_NALU_DEC_PARAM, GF_VVC_NALU_SEQ_PARAM, GF_VVC_NALU_PIC_PARAM, GF_VVC_NALU_APS_PREFIX, GF_VVC_NALU_SEI_PREFIX};
		u32 i = for_non_rap ? 3 : 0;

		for (; i< GF_ARRAY_LENGTH(dump_types); i++) {
			if (vvcc)
				nalumx_write_ps_list(ctx, bs, nalumx_get_vvc_ps(vvcc, dump_types[i]), for_non_rap);
			if (s_vvcc)
				nalumx_write_ps_list(ctx, bs, nalumx_get_vvc_ps(s_vvcc, dump_types[i]), for_non_rap);
		}
	}

	if (for_non_rap) {
		if (ctx->dsi_non_rap) gf_free(ctx->dsi_non_rap);
		gf_bs_get_content(bs, &ctx->dsi_non_rap, &ctx->dsi_non_rap_size);
	} else {
		if (ctx->dsi) gf_free(ctx->dsi);
		gf_bs_get_content(bs, &ctx->dsi, &ctx->dsi_size);
	}
	gf_bs_del(bs);

	if (avcc) gf_odf_avc_cfg_del(avcc);
	if (svcc) gf_odf_avc_cfg_del(svcc);
	if (hvcc) gf_odf_hevc_cfg_del(hvcc);
	if (lvcc) gf_odf_hevc_cfg_del(lvcc);
	if (vvcc) gf_odf_vvc_cfg_del(vvcc);
	if (s_vvcc) gf_odf_vvc_cfg_del(s_vvcc);

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);

	return GF_OK;
}

GF_Err nalumx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_Err e;
	u32 crc, crc_enh, codecid;
	const GF_PropertyValue *p, *dcd, *dcd_enh;
	GF_NALUMxCtx *ctx = gf_filter_get_udta(filter);

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
		ctx->vtype = UFNAL_HEVC;
	} else if (codecid==GF_CODECID_VVC) {
		ctx->vtype = UFNAL_VVC;
	} else {
		ctx->vtype = UFNAL_AVC;
	}

	ctx->width = ctx->height = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (p) ctx->width = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (p) ctx->height = p->value.uint;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL );

	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(ctx->ipid, GF_TRUE);
	if (!dcd && !dcd_enh)
		return GF_OK;

	e = nalumx_make_inband_header(ctx, dcd ? dcd->value.data.ptr : NULL, dcd ? dcd->value.data.size : 0, dcd_enh ? dcd_enh->value.data.ptr : NULL, dcd_enh ? dcd_enh->value.data.size : 0, GF_FALSE);
	if (e) return e;

	if (!ctx->pps_inband || !ctx->rcfg) return GF_OK;
	return nalumx_make_inband_header(ctx, dcd ? dcd->value.data.ptr : NULL, dcd ? dcd->value.data.size : 0, dcd_enh ? dcd_enh->value.data.ptr : NULL, dcd_enh ? dcd_enh->value.data.size : 0, GF_TRUE);
}


static Bool nalumx_is_nal_skip(GF_NALUMxCtx *ctx, u8 *data, u32 pos, Bool *has_nal_delim, u32 *out_temporal_id, u32 *out_layer_id, u8 *avc_hdr)
{
	Bool is_layer = GF_FALSE;
	if (ctx->vtype==UFNAL_HEVC) {
		u8 nal_type = (data[pos] & 0x7E) >> 1;
		u8 temporal_id = data[pos+1] & 0x7;
		u8 layer_id = data[pos] & 1;
		layer_id <<= 5;
		layer_id |= (data[pos+1]>>3) & 0x1F;
		if (temporal_id > *out_temporal_id) *out_temporal_id = temporal_id;
		if (! (*out_layer_id) ) *out_layer_id = 1+layer_id;

		switch (nal_type) {
		case GF_HEVC_NALU_VID_PARAM:
			break;
		case GF_HEVC_NALU_ACCESS_UNIT:
			*has_nal_delim = GF_TRUE;
			break;
		default:
			if (layer_id) is_layer = GF_TRUE;
			break;
		}
	} else if (ctx->vtype==UFNAL_VVC) {
		u8 nal_type = data[pos+1] >> 3;
		u8 temporal_id = (data[pos+1] & 0x7) - 1;
		u8 layer_id = data[pos] & 0x3f;
		if (temporal_id > *out_temporal_id) *out_temporal_id = temporal_id;
		if (! (*out_layer_id) ) *out_layer_id = layer_id;

		switch (nal_type) {
		case GF_VVC_NALU_VID_PARAM:
			break;
		case GF_VVC_NALU_ACCESS_UNIT:
			*has_nal_delim = GF_TRUE;
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
		case GF_AVC_NALU_ACCESS_UNIT:
			*has_nal_delim = GF_TRUE;
			break;
		default:
			if (! (*avc_hdr))
				(*avc_hdr) = data[pos];
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
	u8 *data, *output;
	u32 pck_size, size, sap=0, temporal_id, layer_id;
	u8 avc_hdr;
	u8 *dsi_buf = NULL;
	u32 dsi_buf_size = 0, dsi_nb_nal = 0;

	Bool has_nalu_delim = GF_FALSE;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (!ctx->nb_nalu && ctx->dsi) {
				dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->dsi_size, &output);
				if (!dst_pck) return GF_OUT_OF_MEM;

				memcpy(output, ctx->dsi, ctx->dsi_size);
				gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
				gf_filter_pck_send(dst_pck);
				ctx->nb_nalu += ctx->nb_nalu_in_hdr;
			}
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	if (!ctx->nal_hdr_size) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[NALWrite] no NAL size length field set, assuming 4\n"));
		ctx->nal_hdr_size = 4;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);
	if (!pck_size || !data) {
		//if output and packet properties, forward - this is required for sinks using packets for state signaling
		//such as TS muxer in dash mode looking for EODS property
		if (ctx->opid && gf_filter_pck_has_properties(pck))
			gf_filter_pck_forward(pck, ctx->opid);

		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}
	if (!ctx->bs_r) ctx->bs_r = gf_bs_new(data, pck_size, GF_BITSTREAM_READ);
	else gf_bs_reassign_buffer(ctx->bs_r, data, pck_size);

	size = 0;
	temporal_id = layer_id = 0;
	avc_hdr = 0;

	while (gf_bs_available((ctx->bs_r))) {
		Bool skip_nal = GF_FALSE;
		Bool is_nalu_delim = GF_FALSE;
		u32 pos;
		u32 nal_size = gf_bs_read_int(ctx->bs_r, 8*ctx->nal_hdr_size);
		if (nal_size > gf_bs_available(ctx->bs_r) ) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		pos = (u32) gf_bs_get_position(ctx->bs_r);
		//even if not filtering, parse to check for AU delim
		skip_nal = nalumx_is_nal_skip(ctx, data, pos, &is_nalu_delim, &layer_id, &temporal_id, &avc_hdr);
		if (!ctx->extract) {
			skip_nal = GF_FALSE;
		}
		if (is_nalu_delim) {
			has_nalu_delim = GF_TRUE;
			if (!ctx->delim)
				skip_nal = GF_TRUE;
		}
		if (!skip_nal) {
			size += nal_size + 4;
		}
		gf_bs_skip_bytes(ctx->bs_r, nal_size);
	}
	gf_bs_seek(ctx->bs_r, 0);

	if (!ctx->delim)
		has_nalu_delim = GF_TRUE;

	//we need to insert NALU delim (2 bytes for avc, 3 for hevc or vvc)
	if (!has_nalu_delim) {
		size += (ctx->vtype != UFNAL_AVC) ? 3 : 2;
		size += 4;
	}

	if (ctx->dsi) {
		Bool insert_dsi = GF_FALSE;
		sap = gf_filter_pck_get_sap(pck);
		if (sap && (sap <= GF_FILTER_SAP_3) ) {
			insert_dsi = GF_TRUE;
		}
		if (!insert_dsi) {
			u8 flags = gf_filter_pck_get_dependency_flags(pck);
			//get dependsOn
			if (flags) {
				flags>>=4;
				flags &= 0x3;
				if (flags==2) insert_dsi = GF_TRUE; //could be SAP 1, 2 or 3
			}
		}

		if (insert_dsi) {
			dsi_buf = ctx->dsi;
			dsi_buf_size = ctx->dsi_size;
			dsi_nb_nal = ctx->nb_nalu_in_hdr;
		} else if (ctx->dsi_non_rap) {
			dsi_buf = ctx->dsi_non_rap;
			dsi_buf_size = ctx->dsi_non_rap_size;
			dsi_nb_nal = ctx->nb_nalu_in_hdr_non_rap;
		}
		size += dsi_buf_size;
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
	if (!dst_pck) return GF_OUT_OF_MEM;

	if (!ctx->bs_w) ctx->bs_w = gf_bs_new(output, size, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs_w, output, size);

	// nalu delimiter is not present, write it first
	if (!has_nalu_delim) {
		gf_bs_write_u32(ctx->bs_w, 1);
		if (ctx->vtype==UFNAL_HEVC) {
			if (!layer_id)
				layer_id=1;
			if (!temporal_id)
				temporal_id=1;
#ifndef GPAC_DISABLE_HEVC
			gf_bs_write_int(ctx->bs_w, 0, 1);
			gf_bs_write_int(ctx->bs_w, GF_HEVC_NALU_ACCESS_UNIT, 6);
			gf_bs_write_int(ctx->bs_w, layer_id-1, 6);
			gf_bs_write_int(ctx->bs_w, temporal_id, 3);
			/*pic-type - by default we signal all slice types possible*/
			gf_bs_write_int(ctx->bs_w, 2, 3);
			gf_bs_write_int(ctx->bs_w, 1, 1); //stop bit
			gf_bs_write_int(ctx->bs_w, 0, 4); //4 bits to 0
#endif
		} else if (ctx->vtype==UFNAL_VVC) {
			gf_bs_write_int(ctx->bs_w, 0, 1);
			gf_bs_write_int(ctx->bs_w, 0, 1);
			gf_bs_write_int(ctx->bs_w, layer_id, 6);
			gf_bs_write_int(ctx->bs_w, GF_VVC_NALU_ACCESS_UNIT, 5);
			gf_bs_write_int(ctx->bs_w, temporal_id+1, 3);
			gf_bs_write_int(ctx->bs_w, sap ? 1 : 0, 1);
			/*pic-type - by default we signal all slice types possible*/
			gf_bs_write_int(ctx->bs_w, 2, 3);

			gf_bs_write_int(ctx->bs_w, 1, 1); //stop bit
			gf_bs_write_int(ctx->bs_w, 0, 3); //3 bits to 0
		} else {
			gf_bs_write_int(ctx->bs_w, (avc_hdr & 0x60) | GF_AVC_NALU_ACCESS_UNIT, 8);
			gf_bs_write_int(ctx->bs_w, 0xF0 , 8); /*7 "all supported NALUs" (=111) + rbsp trailing (10000)*/;
		}
		ctx->nb_nalu++;
	}

	while (gf_bs_available((ctx->bs_r))) {
		u32 pos;
		Bool skip_nal = GF_FALSE;
		Bool is_nalu_delim = GF_FALSE;
		u32 nal_size = gf_bs_read_int(ctx->bs_r, 8*ctx->nal_hdr_size);
		if (nal_size > gf_bs_available(ctx->bs_r) ) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		pos = (u32) gf_bs_get_position(ctx->bs_r);

		skip_nal = nalumx_is_nal_skip(ctx, data, pos, &is_nalu_delim, &layer_id, &temporal_id, &avc_hdr);
		if (!ctx->extract) {
			skip_nal = GF_FALSE;
		}
		//we don't serialize nalu delimiter, skip nal
		else if (!ctx->delim && is_nalu_delim) {
			skip_nal = GF_TRUE;
		}


		if (skip_nal) {
			gf_bs_skip_bytes(ctx->bs_r, nal_size);
			continue;
		}

		//insert dsi only after NALUD if any
		if (dsi_buf && !is_nalu_delim) {
			gf_bs_write_data(ctx->bs_w, dsi_buf, dsi_buf_size);
			ctx->nb_nalu += dsi_nb_nal;
			dsi_buf = NULL;

			if (!ctx->rcfg) {
				gf_free(ctx->dsi);
				ctx->dsi = NULL;
				ctx->dsi_size = 0;
			}
		}

		gf_bs_write_u32(ctx->bs_w, 1);
		gf_bs_write_data(ctx->bs_w, data+pos, nal_size);

		gf_bs_skip_bytes(ctx->bs_r, nal_size);
		ctx->nb_nalu++;
	}
	gf_filter_pck_merge_properties(pck, dst_pck);
	gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);

	gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
	gf_filter_pck_send(dst_pck);

	gf_filter_pid_drop_packet(ctx->ipid);

	if (gf_filter_reporting_enabled(filter)) {
		char szStatus[1024];

		sprintf(szStatus, "%s Annex-B %dx%d % 10d NALU", (ctx->vtype==UFNAL_HEVC) ? "HEVC" : ((ctx->vtype==UFNAL_VVC) ? "VVC" : "AVC|H264"), ctx->width, ctx->height, ctx->nb_nalu);
		gf_filter_update_status(filter, -1, szStatus);

	}
	return GF_OK;
}

static void nalumx_finalize(GF_Filter *filter)
{
	GF_NALUMxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs_r) gf_bs_del(ctx->bs_r);
	if (ctx->bs_w) gf_bs_del(ctx->bs_w);
	if (ctx->dsi) gf_free(ctx->dsi);
	if (ctx->dsi_non_rap) gf_free(ctx->dsi_non_rap);
}

static const GF_FilterCapability NALUMxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT_OPT,GF_PROP_PID_CODECID, GF_CODECID_AVC_PS),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT_OPT,GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT_OPT,GF_PROP_PID_CODECID, GF_CODECID_MVC),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT_OPT,GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_TILE_BASE, GF_TRUE),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_VVC),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_TILE_BASE, GF_TRUE),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	//for HLS-SAES AVC
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_4CC('s','a','e','s') ),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
};


#define OFFS(_n)	#_n, offsetof(GF_NALUMxCtx, _n)
static const GF_FilterArgs NALUMxArgs[] =
{
	{ OFFS(rcfg), "force repeating decoder config at each I-frame", GF_PROP_BOOL, "true", NULL, 0},
	{ OFFS(extract), "layer extraction mode\n"
	"- all: extracts all layers\n"
	"- base: extract base layer only\n"
	"- layer: extract non-base layer(s) only", GF_PROP_UINT, "all", "all|base|layer", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(delim), "insert AU Delimiter NAL", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(pps_inband), "inject PPS at each non SAP frame, ignored if rcfg is not set", GF_PROP_BOOL, "false", NULL, 0},
	{0}
};


GF_FilterRegister NALUMxRegister = {
	.name = "ufnalu",
	GF_FS_SET_DESCRIPTION("AVC/HEVC to AnnexB writer")
	GF_FS_SET_HELP("This filter converts AVC|H264 and HEVC streams into AnnexB format, with inband parameter sets and start codes.")
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
