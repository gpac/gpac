/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2022
 *					All rights reserved
 *
 *  This file is part of GPAC / compressed split bitstream aggregator filter
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

#define MAX_LID	64
#define MAX_TID	7

typedef struct
{
	u8 *data;
	u32 size, alloc_size, lid, tid;
} NALStore;

typedef struct _bs_agg_ctx BSAggCtx;
typedef struct _bs_agg_pid BSAggOut;

struct _bs_agg_pid
{
	u32 nalu_size_length;
	u32 codec_id;

	GF_Err (*rewrite_pid_config)(BSAggCtx *ctx, BSAggOut *pctx);
	GF_Err (*agg_process)(BSAggCtx *ctx, BSAggOut *pctx);

	GF_FilterPid *opid;

	GF_FilterPacket *pck;
	GF_List *nal_stores;

	GF_List *ipids;
};

struct _bs_agg_ctx
{
	//options
	Bool svcqid;

	//internal
	GF_Filter *filter;
	GF_List *pids;
};

static GF_Err bsagg_transfer_avc_params(GF_AVCConfig *cfg_out, GF_AVCConfig *cfg_in)
{
	if (!cfg_out) return GF_OUT_OF_MEM;
	for (u32 i=0; i<3; i++) {
		GF_List *l_out, *l_in;
		if (i==0) {
			if (!cfg_in->sequenceParameterSets) continue;
			l_in = cfg_in->sequenceParameterSets;
			l_out = cfg_out->sequenceParameterSets;
		}
		else if (i==2) {
			if (!cfg_in->pictureParameterSets) continue;
			l_in = cfg_in->pictureParameterSets;
			l_out = cfg_out->pictureParameterSets;
		}
		else {
			if (!cfg_in->sequenceParameterSetExtensions) continue;
			l_in = cfg_in->pictureParameterSets;
			if (!cfg_out->sequenceParameterSetExtensions)
				cfg_out->sequenceParameterSetExtensions = gf_list_new();
			l_out = cfg_out->sequenceParameterSetExtensions;
			if (!l_out) return GF_OUT_OF_MEM;
		}
		while (gf_list_count(l_in)) {
			u32 j, count_out = gf_list_count(l_out);
			GF_NALUFFParam *sl_in = gf_list_pop_front(l_in);
			GF_NALUFFParam *sl_out=NULL;
			for (j=0; j<count_out; j++) {
				sl_out = gf_list_get(l_out, j);
				if ((sl_out->size == sl_in->size) && !memcmp(sl_in->data, sl_out->data, sl_in->size))
					break;
				sl_out = NULL;
			}
			if (sl_out) {
				gf_free(sl_in->data);
				gf_free(sl_in);
			} else {
				gf_list_add(l_out, sl_in);
			}
		}
	}
	return GF_OK;
}

static GF_Err avc_rewrite_pid_config(BSAggCtx *ctx, BSAggOut *pctx)
{
	GF_Err e;
	u32 i, count;
	GF_AVCConfig *avcc_out = NULL;
	GF_AVCConfig *svcc_out = NULL;
	GF_FilterPid *base = NULL;

	count = gf_list_count(pctx->ipids);
	for (i=0; i<count; i++) {
		const GF_PropertyValue *dsi, *dsi_enh, *prop;
		GF_FilterPid *ipid = gf_list_get(pctx->ipids, i);
		prop = gf_filter_pid_get_property(ipid, GF_PROP_PID_DEPENDENCY_ID);
		if (!prop || !prop->value.uint) base = ipid;

		dsi = gf_filter_pid_get_property(ipid, GF_PROP_PID_DECODER_CONFIG);
		dsi_enh = gf_filter_pid_get_property(ipid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);


		GF_AVCConfig *avcc = NULL;

		if (dsi) {
			avcc = gf_odf_avc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
			if (!avcc) return GF_NON_COMPLIANT_BITSTREAM;
		}
		else if (dsi_enh) {
			avcc = gf_odf_avc_cfg_read(dsi_enh->value.data.ptr, dsi_enh->value.data.size);
			if (!avcc) return GF_NON_COMPLIANT_BITSTREAM;
		}

		gf_filter_pid_set_udta_flags(ipid, avcc->nal_unit_size);
		if (dsi && (base==ipid) && !avcc_out) {
			avcc_out = avcc;
			avcc = NULL;
		}

reparse:
		if (avcc) {
			if (!svcc_out) {
				GF_SAFEALLOC(svcc_out, GF_AVCConfig);
				if (!svcc_out) {
					gf_odf_avc_cfg_del(avcc);
					return GF_OUT_OF_MEM;
				}
				memcpy(svcc_out, avcc, sizeof(GF_AVCConfig));
				svcc_out->sequenceParameterSets = gf_list_new();
				svcc_out->pictureParameterSets = gf_list_new();
				svcc_out->sequenceParameterSetExtensions = NULL;
			}

			e = bsagg_transfer_avc_params(svcc_out, avcc);
			if (e) return e;

			gf_odf_avc_cfg_del(avcc);
		}

		if (dsi && dsi_enh) {

			avcc = gf_odf_avc_cfg_read(dsi_enh->value.data.ptr, dsi_enh->value.data.size);
			if (!avcc) return GF_NON_COMPLIANT_BITSTREAM;
			dsi_enh = NULL;
			goto reparse;
		}
	}

	if (base) {
		gf_filter_pid_copy_properties(pctx->opid, base);

		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_AVC));
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_ISOM_STSD_TEMPLATE, NULL);
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_BITRATE, NULL);

		u8 *data;
		u32 size;
		avcc_out->nal_unit_size = 4;
		gf_odf_avc_cfg_write(avcc_out, &data, &size);
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(data, size) );
		if (svcc_out) {
			svcc_out->nal_unit_size = 4;
			gf_odf_avc_cfg_write(svcc_out, &data, &size);
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, &PROP_DATA_NO_COPY(data, size) );
		} else {
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);
		}
	}
	if (avcc_out) gf_odf_avc_cfg_del(avcc_out);
	if (svcc_out) gf_odf_avc_cfg_del(svcc_out);
	return GF_OK;
}

static GF_Err bsagg_transfer_param_array(GF_List *array_out, GF_List *array_in)
{
	while (gf_list_count(array_in)) {
		u32 i, count = gf_list_count(array_out);
		GF_NALUFFParamArray *pa_in = gf_list_pop_front(array_in);
		GF_NALUFFParamArray *pa_out = NULL;
		for (i=0; i<count; i++) {
			pa_out = gf_list_get(array_out, i);
			if (pa_out->type == pa_in->type) break;
			pa_out = NULL;
		}
		if (!pa_out) {
			GF_SAFEALLOC(pa_out, GF_NALUFFParamArray);
			if (!pa_out) return GF_OUT_OF_MEM;

			pa_out->type = pa_in->type;
			pa_out->array_completeness = pa_in->array_completeness;
			pa_out->nalus = gf_list_new();
			if (!pa_out->nalus) return GF_OUT_OF_MEM;
			gf_list_add(array_out, pa_out);
		}
		while (gf_list_count(pa_in->nalus)) {
			u32 j, count2 = gf_list_count(pa_out->nalus);
			GF_NALUFFParam *sl_in = gf_list_pop_front(pa_in->nalus);
			GF_NALUFFParam *sl_out = NULL;
			for (j=0; j<count2; j++) {
				sl_out = gf_list_get(pa_out->nalus, j);
				if ((sl_out->size == sl_in->size) && !memcmp(sl_out->data, sl_in->data, sl_in->size))
					break;
				sl_out = NULL;
			}
			if (!sl_out) {
				gf_list_add(pa_out->nalus, sl_in);
			}
			else {
				gf_free(sl_in->data);
				gf_free(sl_in);
			}
		}

		gf_list_del(pa_in->nalus);
		gf_free(pa_in);
	}

	return GF_OK;
}

static GF_Err vvc_hevc_rewrite_pid_config(BSAggCtx *ctx, BSAggOut *pctx)
{
	GF_Err e;
	u32 i, count;
	Bool is_vvc = GF_FALSE;
#ifndef GPAC_DISABLE_HEVC
	GF_HEVCConfig *hvcc_out = NULL;
	GF_HEVCConfig *hvcc_enh_out = NULL;
#endif
	GF_VVCConfig *vvcc_out = NULL;
	GF_FilterPid *base = NULL;
	const GF_PropertyValue *dv_cfg = NULL;
	if (pctx->codec_id == GF_CODECID_VVC) is_vvc = GF_TRUE;

	if (is_vvc) {
		vvcc_out = gf_odf_vvc_cfg_new();
	} else {
#ifndef GPAC_DISABLE_HEVC
		hvcc_out = gf_odf_hevc_cfg_new();
#endif
	}

	count = gf_list_count(pctx->ipids);
	for (i=0; i<count; i++) {
		const GF_PropertyValue *dsi, *dsi_enh, *prop;
		GF_FilterPid *ipid = gf_list_get(pctx->ipids, i);
		prop = gf_filter_pid_get_property(ipid, GF_PROP_PID_DEPENDENCY_ID);
		if (!prop || !prop->value.uint) base = ipid;

		dsi = gf_filter_pid_get_property(ipid, GF_PROP_PID_DECODER_CONFIG);
		dsi_enh = gf_filter_pid_get_property(ipid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);

		if (!dv_cfg)
			dv_cfg = gf_filter_pid_get_property(ipid, GF_PROP_PID_DOLBY_VISION);

		if (is_vvc) {
			GF_VVCConfig *vvcc = NULL;

			if (dsi) {
				vvcc = gf_odf_vvc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
				if (!vvcc) return GF_NON_COMPLIANT_BITSTREAM;
			}
			else if (dsi_enh) {
				vvcc = gf_odf_vvc_cfg_read(dsi_enh->value.data.ptr, dsi_enh->value.data.size);
				if (!vvcc) return GF_NON_COMPLIANT_BITSTREAM;
			}
			if (dsi) {
				GF_List *bck = vvcc_out->param_array;
				memcpy(vvcc_out, vvcc, sizeof(GF_VVCConfig));
				vvcc_out->param_array = bck;
			}

			gf_filter_pid_set_udta_flags(ipid, vvcc->nal_unit_size);
			e = bsagg_transfer_param_array(vvcc_out->param_array, vvcc->param_array);
			if (e) return e;

			if (vvcc)
				gf_odf_vvc_cfg_del(vvcc);

			if (dsi && dsi_enh) {
				vvcc = gf_odf_vvc_cfg_read(dsi_enh->value.data.ptr, dsi_enh->value.data.size);
				if (!vvcc) return GF_NON_COMPLIANT_BITSTREAM;

				e = bsagg_transfer_param_array(vvcc_out->param_array, vvcc->param_array);
				gf_odf_vvc_cfg_del(vvcc);
				if (e) return e;
			}
		} else {
#ifndef GPAC_DISABLE_HEVC
			GF_HEVCConfig *hvcc = NULL;
			Bool dsi_lhvc=GF_FALSE;
			if ((pctx->codec_id==GF_CODECID_LHVC) && !dsi_enh) dsi_lhvc=GF_TRUE;

			if (dsi) {
				hvcc = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size, dsi_lhvc);
				if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;
			}
			else if (dsi_enh) {
				hvcc = gf_odf_hevc_cfg_read(dsi_enh->value.data.ptr, dsi_enh->value.data.size, GF_TRUE);
				if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;
			}
			if (dsi) {
				GF_List *bck = hvcc_out->param_array;
				memcpy(hvcc_out, hvcc, sizeof(GF_HEVCConfig));
				hvcc_out->param_array = bck;
			}

			gf_filter_pid_set_udta_flags(ipid, hvcc->nal_unit_size);
			if (dsi) {
				e = bsagg_transfer_param_array(hvcc_out->param_array, hvcc->param_array);
				if (e) return e;
			} else {
				if (!hvcc_enh_out) {
					hvcc_enh_out = hvcc;
					hvcc = NULL;
				} else {
					e = bsagg_transfer_param_array(hvcc_enh_out->param_array, hvcc->param_array);
					if (e) return e;
				}
			}
			if (hvcc)
				gf_odf_hevc_cfg_del(hvcc);

			if (dsi && dsi_enh) {
				hvcc = gf_odf_hevc_cfg_read(dsi_enh->value.data.ptr, dsi_enh->value.data.size, GF_TRUE);
				if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;

				e = bsagg_transfer_param_array(hvcc_out->param_array, hvcc->param_array);
				gf_odf_hevc_cfg_del(hvcc);
				if (e) return e;
			}
#endif
		}
	}

	if (base) {

		gf_filter_pid_copy_properties(pctx->opid, base);

		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_HEVC));
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_ISOM_STSD_TEMPLATE, NULL);
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_BITRATE, NULL);

		if (dv_cfg)
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DOLBY_VISION, dv_cfg);


		u8 *data;
		u32 size;
		if (vvcc_out) {
			vvcc_out->nal_unit_size = 4;
			gf_odf_vvc_cfg_write(vvcc_out, &data, &size);
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(data, size) );
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);
		}
#ifndef GPAC_DISABLE_HEVC
		if (hvcc_out) {
			hvcc_out->nal_unit_size = 4;
			hvcc_out->is_lhvc = GF_FALSE;
			gf_odf_hevc_cfg_write(hvcc_out, &data, &size);
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(data, size) );
			if (hvcc_enh_out) {
				hvcc_enh_out->is_lhvc = GF_TRUE;
				hvcc_enh_out->nal_unit_size = 4;
				gf_odf_hevc_cfg_write(hvcc_enh_out, &data, &size);
				gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, &PROP_DATA_NO_COPY(data, size) );
			} else {
				gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);
			}
		}
#endif
	}

#ifndef GPAC_DISABLE_HEVC
	if (hvcc_out) gf_odf_hevc_cfg_del(hvcc_out);
	if (hvcc_enh_out) gf_odf_hevc_cfg_del(hvcc_enh_out);
#endif
	if (vvcc_out) gf_odf_vvc_cfg_del(vvcc_out);
	return GF_OK;
}


static GF_Err none_rewrite_pid_config(BSAggCtx *ctx, BSAggOut *c_opid)
{
	GF_FilterPid *ipid = gf_list_get(c_opid->ipids, 0);
	gf_filter_pid_copy_properties(c_opid->opid, ipid);
	return GF_OK;
}

static GF_Err none_process(BSAggCtx *ctx, BSAggOut *c_opid)
{
	GF_FilterPid *ipid = gf_list_get(c_opid->ipids, 0);
	while (1) {
		GF_Err e;
		GF_FilterPacket *pck = gf_filter_pid_get_packet(ipid);
		if (!pck) {
			if (gf_filter_pid_is_eos(ipid)) {
				gf_filter_pid_set_eos(c_opid->opid);
				return GF_EOS;
			}
			return GF_OK;
		}
		e = gf_filter_pck_forward(pck, c_opid->opid);
		gf_filter_pid_drop_packet(ipid);
		if (e) return e;
	}
	return GF_OK;
}

static GF_Err nalu_process(BSAggCtx *ctx, BSAggOut *pctx, u32 codec_type)
{
	u32 size, pck_size, i, count, tot_size=0, nb_done=0;
	u64 min_dts = GF_FILTER_NO_TS;
	u32 min_timescale=0, min_nal_size;
	GF_Err process_error = GF_OK;
	Bool has_svc_prefix = GF_FALSE;

	min_nal_size = codec_type ? 2 : 1;

	count = gf_list_count(pctx->ipids);
	for (i=0; i<count; i++) {
		u64 ts;
		u32 timescale;
		GF_FilterPid *pid = gf_list_get(pctx->ipids, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(pid);
		if (!pck) {
			if (gf_filter_pid_is_eos(pid)) {
				nb_done++;
				continue;
			}
			return GF_OK;
		}
		ts = gf_filter_pck_get_dts(pck);
		if (ts==GF_FILTER_NO_TS)
			ts = gf_filter_pck_get_cts(pck);
		if (ts==GF_FILTER_NO_TS) {
			ts = 0;
		}
		timescale = gf_filter_pck_get_timescale(pck);
		if (min_dts==GF_FILTER_NO_TS) {
			min_dts = ts;
			min_timescale = timescale;
		} else if (gf_timestamp_less(ts, timescale, min_dts, min_timescale)) {
			min_dts = ts;
			min_timescale = timescale;
		}
	}
	if (nb_done==count) {
		gf_filter_pid_set_eos(pctx->opid);
		return GF_EOS;
	}
	if (!min_timescale) return GF_OK;

	for (i=0; i<count; i++) {
		GF_Err e = GF_OK;
		u64 ts;
		u32 timescale;
		u32 svc_layer_id=0, svc_temporal_id=0;
		GF_FilterPid *pid = gf_list_get(pctx->ipids, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(pid);
		if (!pck) continue;

		ts = gf_filter_pck_get_dts(pck);
		if (ts==GF_FILTER_NO_TS) ts = gf_filter_pck_get_cts(pck);
		if (ts==GF_FILTER_NO_TS) ts = 0;

		timescale = gf_filter_pck_get_timescale(pck);
		if (! gf_timestamp_less_or_equal(ts, timescale, min_dts, min_timescale)) continue;

		const u8 *data = gf_filter_pck_get_data(pck, &pck_size);
		if (!data) continue;

		u32 nalu_size_length = gf_filter_pid_get_udta_flags(pid);

		size=0;
		while (size<pck_size) {
			u32 nal_type=0;
			u32 layer_id = 0;
			u32 temporal_id = 0;
			u32 nal_hdr = nalu_size_length;
			u32 nal_size = 0;
			while (nal_hdr) {
				nal_size |= data[size];
				size++;
				nal_hdr--;
				if (!nal_hdr) break;
				nal_size<<=8;
			}
			if (size + nal_size > pck_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[BSAgg] Invalid NAL size %d but remain %d\n", nal_size, pck_size-size));
				e = GF_NON_COMPLIANT_BITSTREAM;
				break;
			}
			if (nal_size < min_nal_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[BSAgg] Invalid NAL size %d but mn size %d\n", nal_size, min_nal_size));
				e = GF_NON_COMPLIANT_BITSTREAM;
				break;
			}

			//AVC
			if (codec_type==0) {
				nal_type = data[size] & 0x1F;
				if ((nal_type == GF_AVC_NALU_SVC_PREFIX_NALU) || (nal_type==GF_AVC_NALU_SVC_SLICE)) {
					if (nal_size < 4) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[BSAgg] Invalid NAL size %d but mn size 4\n", nal_size));
						e = GF_NON_COMPLIANT_BITSTREAM;
						break;
					}
					has_svc_prefix = GF_TRUE;
					//quick parse svc nal header (right after 1-byte nal header)
					//u32 prio_id = (data[size+1]) & 0x3F;
					u32 dep_id = (data[size+2] >> 4) & 0x7;
					u32 qual_id = (data[size+2]) & 0xF;
					u32 temporal_id = (data[size+3]>>5) & 0x7;

					svc_temporal_id = temporal_id;
					svc_layer_id = ctx->svcqid ? qual_id : dep_id;
				}
				if (has_svc_prefix) {
					layer_id = svc_layer_id;
					temporal_id = svc_temporal_id;
				}

				//force layerID 100 for DV
				if ((nal_type == GF_AVC_NALU_DV_RPU) || (nal_type == GF_AVC_NALU_DV_EL)) {
					layer_id = 100;
				}
				//don't forward extractors
				//todo, find a way to differentiate DV EL from naluff aggregator
				else if ((nal_type == GF_AVC_NALU_FF_EXTRACTOR)
					|| (nal_type == GF_AVC_NALU_ACCESS_UNIT)
				) {
					size += nal_size;
					continue;
				}
			}
			//HEVC
			else if (codec_type==1) {
				nal_type = (data[size] & 0x7E) >> 1;

				layer_id = data[size] & 1;
				layer_id <<= 5;
				layer_id |= (data[size+1] >> 3) & 0x1F;
				temporal_id = (data[size+1] & 0x7);

				if (nal_type == GF_HEVC_NALU_ACCESS_UNIT) {
					size += nal_size;
					continue;
				}

				//force layerID 100 for DV
				if ((nal_type == GF_HEVC_NALU_DV_RPU) || (nal_type == GF_HEVC_NALU_DV_EL)) {
					layer_id = 100;
				}
				//don't forward extractors nor aggregators
				else if ((nal_type == GF_HEVC_NALU_FF_EXTRACTOR)
					|| (nal_type == GF_HEVC_NALU_FF_AGGREGATOR)
					|| (nal_type == GF_AVC_NALU_ACCESS_UNIT)
				) {
					size += nal_size;
					continue;
				}
			}
			//VVC
			else if (codec_type==2) {
				layer_id = data[size] & 0x3F;
				temporal_id = data[size+1] & 0x7;
				nal_type = data[size+1] >> 3;

				if (nal_type == GF_VVC_NALU_ACCESS_UNIT) {
					size += nal_size;
					continue;
				}
			}
			//push nal
			NALStore *ns = NULL;
			s32 next_ns_idx = -1;
			u32 nal_count = gf_list_count(pctx->nal_stores);
			for (u32 j=0; j<nal_count; j++) {
				ns = gf_list_get(pctx->nal_stores, j);
				if ((ns->lid == layer_id) && (ns->tid == temporal_id))
					break;

				//same layer, tid greater than ours: insert nal
				if (ns->lid==layer_id) {
					if (ns->tid > temporal_id) {
						next_ns_idx = j;
						ns = NULL;
						break;
					}
				}
				if (ns->lid>layer_id) {
					next_ns_idx = j;
					ns = NULL;
					break;
				}
				ns = NULL;
			}

			if (!ns) {
				GF_SAFEALLOC(ns, NALStore)
				if (!ns) {
					e = GF_OUT_OF_MEM;
					break;
				}
				ns->lid = layer_id;
				ns->tid = temporal_id;
				if (next_ns_idx>=0) {
					gf_list_insert(pctx->nal_stores, ns, next_ns_idx);
				} else {
					gf_list_add(pctx->nal_stores, ns);
				}
			}
			if (ns->alloc_size < ns->size+nal_size+4) {
				ns->alloc_size = ns->size+nal_size+4;
				ns->data = gf_realloc(ns->data, ns->alloc_size);
				if (!ns->data) {
					e = GF_OUT_OF_MEM;
					break;
				}
			}
			u8 *dst = ns->data + ns->size;
			//write nal header
			nal_hdr=4;
			while (nal_hdr) {
				nal_hdr--;
				*dst = (nal_size>>(8*nal_hdr)) & 0xFF;
				dst++;
			}
			//write nal
			memcpy(dst, data + size, nal_size);
			ns->size+= nal_size + 4;
			tot_size += nal_size + 4;

			size += nal_size;
		}
		//if not created, alloc output packet with 4 bytes and copy packet props
		if (size && !pctx->pck) {
			pctx->pck = gf_filter_pck_new_alloc(pctx->opid, 4, NULL);
			if (!pctx->pck) {
				e = GF_OUT_OF_MEM;
				break;
			}
			gf_filter_pck_merge_properties(pck, pctx->pck);
		}
		//drop packet
		gf_filter_pid_drop_packet(pid);

		if (e) process_error = e;
	}

	if (!tot_size) return process_error;

	//realloc packet to total size
	u8 *output=NULL;
	gf_filter_pck_expand(pctx->pck, tot_size-4, &output, NULL, NULL);
	u32 nal_count = gf_list_count(pctx->nal_stores);
	for (u32 j=0; j<nal_count; j++) {
		NALStore *ns = gf_list_get(pctx->nal_stores, j);
		if (!ns->size) continue;

		memcpy(output, ns->data, ns->size);
		output+= ns->size;
		ns->size = 0;
	}

	gf_filter_pck_send(pctx->pck);
	pctx->pck = NULL;
	return GF_OK;
}

static GF_Err avc_process(BSAggCtx *ctx, BSAggOut *c_opid)
{
	return nalu_process(ctx, c_opid, 0);
}
static GF_Err hevc_process(BSAggCtx *ctx, BSAggOut *c_opid)
{
	return nalu_process(ctx, c_opid, 1);
}
static GF_Err vvc_process(BSAggCtx *ctx, BSAggOut *c_opid)
{
	return nalu_process(ctx, c_opid, 2);
}

static Bool bs_agg_is_base(BSAggOut *pctx, u32 codec_id, u32 dep_id, u32 rec_level)
{
	u32 i, count = gf_list_count(pctx->ipids);
	if (rec_level>count) return GF_FALSE;
	for (i=0; i<count; i++) {
		GF_FilterPid *ipid = gf_list_get(pctx->ipids, i);
		const GF_PropertyValue *p = gf_filter_pid_get_property(ipid, GF_PROP_PID_ID);
		if (p && (p->value.uint==dep_id)) return GF_TRUE;
		p = gf_filter_pid_get_property(ipid, GF_PROP_PID_DEPENDENCY_ID);
		if (!p || !p->value.uint) continue;;

		if (bs_agg_is_base(pctx, codec_id, p->value.uint, rec_level+1))
			return GF_TRUE;
	}
	return GF_FALSE;
}

static BSAggOut *bs_agg_get_base(BSAggCtx *ctx, u32 codec_id, u32 dep_id)
{
	u32 i, count = gf_list_count(ctx->pids);
	for (i=0; i<count; i++) {
		BSAggOut *pctx = gf_list_get(ctx->pids, i);
		if (bs_agg_is_base(pctx, codec_id, dep_id, 0))
			return pctx;
	}
	return NULL;
}

static void bs_agg_reset_stream(BSAggOut *pctx)
{
	while (gf_list_count(pctx->nal_stores)) {
		NALStore *ns = gf_list_pop_back(pctx->nal_stores);
		gf_free(ns->data);
		gf_free(ns);
	}
	gf_list_del(pctx->nal_stores);
	gf_list_del(pctx->ipids);
	gf_free(pctx);
}

static GF_Err bs_agg_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	u32 codec_id;
	BSAggCtx *ctx = (BSAggCtx *) gf_filter_get_udta(filter);
	BSAggOut *pctx = gf_filter_pid_get_udta(pid);

	//disconnect of src pid
	if (is_remove) {
		if (pctx) {
			gf_list_del_item(pctx->ipids, pid);
			if (!gf_list_count(pctx->ipids)) {
				gf_filter_pid_remove(pctx->opid);
				gf_list_del_item(ctx->pids, pctx);
				bs_agg_reset_stream(pctx);
			}
 			gf_filter_pid_set_udta(pid, NULL);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!prop) return GF_NOT_SUPPORTED;
	codec_id = prop->value.uint;

	//initial config of pid, check base ID
	if (!pctx) {
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
		if (prop && prop->value.uint)
			pctx = bs_agg_get_base(ctx, codec_id, prop->value.uint);

		if (!pctx) {
			GF_SAFEALLOC(pctx, BSAggOut);
			if (!pctx) return GF_OUT_OF_MEM;
			pctx->ipids = gf_list_new();
			pctx->nal_stores = gf_list_new();
			if (!pctx->ipids || !pctx->nal_stores) return GF_OUT_OF_MEM;
			pctx->opid = gf_filter_pid_new(filter);
			gf_list_add(ctx->pids, pctx);
		}
		//add input
		gf_filter_pid_set_udta(pid, pctx);
		gf_list_add(pctx->ipids, pid);
	}

	switch (codec_id) {
	case GF_CODECID_AVC:
	case GF_CODECID_SVC:
	case GF_CODECID_MVC:
		pctx->rewrite_pid_config = avc_rewrite_pid_config;
		pctx->agg_process = avc_process;
		break;
#ifndef GPAC_DISABLE_HEVC
	case GF_CODECID_HEVC:
	case GF_CODECID_LHVC:
		codec_id = GF_CODECID_HEVC;
		pctx->rewrite_pid_config = vvc_hevc_rewrite_pid_config;
		pctx->agg_process = hevc_process;
		break;
#endif
	case GF_CODECID_VVC:
		pctx->rewrite_pid_config = vvc_hevc_rewrite_pid_config;
		pctx->agg_process = vvc_process;
		break;
	default:
		pctx->rewrite_pid_config = none_rewrite_pid_config;
		pctx->agg_process = none_process;
		break;
	}

	//change of codec, reset output
	if (pctx->codec_id && (pctx->codec_id != codec_id)) {
		while (gf_list_count(pctx->nal_stores)) {
			NALStore *ns = gf_list_pop_back(pctx->nal_stores);
			gf_free(ns->data);
			gf_free(ns);
		}
	}
	pctx->codec_id = codec_id;

	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	//rewrite asap - waiting for first packet could lead to issues further down the chain, especially for movie fragments
	//were the init segment could have already been flushed at the time we will dispatch the first packet
	return pctx->rewrite_pid_config(ctx, pctx);
}


static GF_Err bs_agg_process(GF_Filter *filter)
{
	u32 i, count, done=0;
	BSAggCtx *ctx = (BSAggCtx *) gf_filter_get_udta(filter);

	count = gf_list_count(ctx->pids);
	for (i=0; i<count; i++) {
		BSAggOut *pctx = gf_list_get(ctx->pids, i);
		GF_Err e = pctx->agg_process(ctx, pctx);
		if (e==GF_EOS) done++;
		else if (e) return e;
	}
	if (done==count) return GF_EOS;
	return GF_OK;
}


static GF_Err bs_agg_initialize(GF_Filter *filter)
{
	BSAggCtx *ctx = (BSAggCtx *) gf_filter_get_udta(filter);
	ctx->pids = gf_list_new();

	ctx->filter = filter;
	return GF_OK;
}
static void bs_agg_finalize(GF_Filter *filter)
{
	BSAggCtx *ctx = (BSAggCtx *) gf_filter_get_udta(filter);

	while (gf_list_count(ctx->pids)) {
		BSAggOut *pctx = gf_list_pop_back(ctx->pids);
		bs_agg_reset_stream(pctx);
	}
	gf_list_del(ctx->pids);
}

#define OFFS(_n)	#_n, offsetof(BSAggCtx, _n)
static GF_FilterArgs BSAggArgs[] =
{
	{ OFFS(svcqid), "use qualityID instead of dependencyID for SVC splitting", GF_PROP_BOOL, "false", NULL, 0},
	{0}
};

static const GF_FilterCapability BSAggCaps[] =
{
	//we want the filter to act as passthrough for other media, so we accept everything
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
};

GF_FilterRegister BSAggRegister = {
	.name = "bsagg",
	GF_FS_SET_DESCRIPTION("Compressed layered bitstream aggregator")
	GF_FS_SET_HELP("This filter aggregates layers and sublayers into a single output PID.\n"
	"\n"
	"The filter supports AVC|H264, HEVC and VVC stream reconstruction, and is passthrough for other codec types.\n"
	"\n"
	"Aggregation is based on temporalID value (start from 1) and layerID value (start from 0).\n"
	"For AVC|H264, layerID is the dependency value, or quality value if `svcqid` is set.\n"
	"\n"
	"The filter can also be used on AVC and HEVC DolbyVision dual-streams to aggregate base stream and DV RPU/EL.\n"
	"\n"
	"The filter does not forward aggregator or extractor NAL units.\n"
	)
	.private_size = sizeof(BSAggCtx),
	.max_extra_pids = 0xFFFFFFFF,
	.flags = GF_FS_REG_EXPLICIT_ONLY|GF_FS_REG_ALLOW_CYCLIC,
	.args = BSAggArgs,
	SETCAPS(BSAggCaps),
	.initialize = bs_agg_initialize,
	.finalize = bs_agg_finalize,
	.configure_pid = bs_agg_configure_pid,
	.process = bs_agg_process
};

const GF_FilterRegister *bs_agg_register(GF_FilterSession *session)
{
	return (const GF_FilterRegister *) &BSAggRegister;
}
