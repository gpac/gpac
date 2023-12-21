/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2022-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / compressed bitstream splitter filter
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

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_BSSPLIT)

typedef struct
{
	GF_FilterPid *opid;

	u32 max_layer_id, max_temporal_id;

	u32 min_lid_plus_one, min_tid_plus_one;

	Bool is_base, is_init;
	u32 id, dep_id, width, height;

	GF_FilterPacket *pck;

	GF_AVCConfig *svcc;
} BSSplitOut;

typedef struct _bs_split_ctx BSSplitCtx;
typedef struct _bs_split_pid BSSplitIn;

struct _bs_split_pid
{
	u32 nalu_size_length;
	u32 codec_id;

	u64 first_ts_plus_one;

	GF_Err (*rewrite_pid_config)(BSSplitCtx *ctx, BSSplitIn *pctx);
	GF_Err (*split_packet)(BSSplitCtx *ctx, BSSplitIn *pctx, GF_FilterPacket *pck);

	GF_FilterPid *ipid;

	GF_List *opids;

	GF_AVCConfig *avcc;
	GF_AVCConfig *svcc;
	AVCState *avc_state;
	GF_BitStream *r_bs;

	u32 max_layers, max_sublayers;
};

struct _bs_split_ctx
{
	//options
	GF_PropStringList ltid;
	Bool sig_ltid, svcqid;

	//internal
	GF_Filter *filter;
	GF_List *pids;
};


static Bool match_substream(BSSplitOut *c_opid, u32 layer_id, u32 temporal_id)
{
	if (layer_id > c_opid->max_layer_id) return GF_FALSE;

	if (temporal_id > c_opid->max_temporal_id) return GF_FALSE;

	if (c_opid->min_lid_plus_one) {
		if (layer_id < c_opid->min_lid_plus_one-1)
			return GF_FALSE;
		if (layer_id > c_opid->min_lid_plus_one-1)
			return GF_TRUE;
	}
	if (c_opid->min_tid_plus_one) {
		if (temporal_id <= c_opid->min_tid_plus_one-1)
			return GF_FALSE;
	}

	return GF_TRUE;
}

//reinsert param set by ID order
static void bs_split_svc_add_param(GF_List *ps_list, GF_NALUFFParam *sl)
{
	u32 i, count=gf_list_count(ps_list);
	for (i=0; i<count; i++) {
		GF_NALUFFParam *p = gf_list_get(ps_list, i);
		if (p->id > sl->id) {
			gf_list_insert(ps_list, sl, i);
			return;
		}
	}
	gf_list_add(ps_list, sl);
}

static void bs_split_check_svc_config(BSSplitIn *pctx, BSSplitOut *c_opid)
{
	u32 i;
	Bool sps_found=GF_FALSE, pps_found=GF_FALSE;
	u32 avc_pps_id = pctx->avc_state->s_info.pps->id;
	//warning the sps ID internally stored in svcc is shifted by GF_SVC_SSPS_ID_SHIFT

	u32 avc_sps_id;
	if (pctx->avc_state->last_nal_type_parsed == GF_AVC_NALU_SVC_SLICE)
		avc_sps_id = pctx->avc_state->s_info.pps->sps_id + GF_SVC_SSPS_ID_SHIFT;
	else
		avc_sps_id = pctx->avc_state->s_info.pps->sps_id;

	if (!c_opid->svcc) {
		GF_SAFEALLOC(c_opid->svcc, GF_AVCConfig);
		memcpy(c_opid->svcc, pctx->svcc, sizeof(GF_AVCConfig));
		c_opid->svcc->sequenceParameterSets = gf_list_new();
		c_opid->svcc->pictureParameterSets = gf_list_new();
		c_opid->svcc->complete_representation = 0;
	}

	if (pctx->avc_state->sps[avc_sps_id].width > c_opid->width) {
		c_opid->width = pctx->avc_state->sps[avc_sps_id].width;
		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_WIDTH, &PROP_UINT(c_opid->width));
		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_WIDTH_MAX, &PROP_UINT(c_opid->width));
	}
	if (pctx->avc_state->sps[avc_sps_id].height > c_opid->height) {
		c_opid->height = pctx->avc_state->sps[avc_sps_id].height;
		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(c_opid->height));
		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_HEIGHT_MAX, &PROP_UINT(c_opid->height));
	}

#define CHECK_PARAM(_name, _id, _res)\
	for (i=0; i<gf_list_count(_name); i++) {\
		GF_NALUFFParam *sl = gf_list_get(_name, i);\
		if (sl->id == _id) { _res = GF_TRUE; break; }\
	}

	//check sps, first in our config
	CHECK_PARAM(c_opid->svcc->sequenceParameterSets, avc_sps_id, sps_found)
	if (!sps_found) {
		//sps is in base config
		CHECK_PARAM(pctx->avcc->sequenceParameterSets, avc_sps_id, sps_found)
		if (!sps_found && (avc_sps_id<GF_SVC_SSPS_ID_SHIFT)) {
			CHECK_PARAM(c_opid->svcc->sequenceParameterSets, avc_sps_id+GF_SVC_SSPS_ID_SHIFT, sps_found)
		}
	}

	//check pps, first in our config
	CHECK_PARAM(c_opid->svcc->pictureParameterSets, avc_pps_id, pps_found)
	if (!pps_found) {
		//pps is in base config
		CHECK_PARAM(pctx->avcc->pictureParameterSets, avc_pps_id, pps_found)
	}
	if (pps_found && sps_found) return;

	if (!sps_found) {
rebrowse_sps:
		for (i=0; i<gf_list_count(pctx->svcc->sequenceParameterSets); i++) {
			GF_NALUFFParam *sl = gf_list_get(pctx->svcc->sequenceParameterSets, i);
			if (sl->id == avc_sps_id) {
				sps_found = GF_TRUE;
				if (gf_list_find(c_opid->svcc->sequenceParameterSets, sl)<0)
					bs_split_svc_add_param(c_opid->svcc->sequenceParameterSets, sl);
				break;
			}
		}
		if (!sps_found && (avc_sps_id<GF_SVC_SSPS_ID_SHIFT)) {
			avc_sps_id+=GF_SVC_SSPS_ID_SHIFT;
			goto rebrowse_sps;
		}
	}
	if (!pps_found) {
		for (i=0; i<gf_list_count(pctx->svcc->pictureParameterSets); i++) {
			GF_NALUFFParam *sl = gf_list_get(pctx->svcc->pictureParameterSets, i);
			if (sl->id == avc_pps_id) {
				pps_found = GF_TRUE;
				if (gf_list_find(c_opid->svcc->pictureParameterSets, sl)<0)
					bs_split_svc_add_param(c_opid->svcc->pictureParameterSets, sl);
				break;
			}
		}
	}
	if (!sps_found) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[BSSplit] Missing sequence parameter set ID %d in input\n", (avc_sps_id>GF_SVC_SSPS_ID_SHIFT) ? (avc_sps_id-GF_SVC_SSPS_ID_SHIFT) : avc_sps_id));
	}
	if (!pps_found) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[BSSplit] Missing picture parameter set ID %d in input\n", avc_pps_id));
	}
	if (!pps_found && !sps_found) return;

	u8 *dsi;
	u32 dsi_size;
	gf_odf_avc_cfg_write(c_opid->svcc, &dsi, &dsi_size);
	gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, &PROP_DATA_NO_COPY(dsi, dsi_size) );

}

static void bs_split_update_hevc_linf(BSSplitIn *pctx, BSSplitOut *c_opid)
{
	u32 i;
	u32 max_lid = c_opid->max_layer_id;
	u32 max_tid = c_opid->max_temporal_id;
	u32 min_lid = 0;
	u32 min_tid = 0;
	for (i=0; i<gf_list_count(pctx->opids); i++) {
		BSSplitOut *an_out = gf_list_get(pctx->opids, i);
		if (an_out==c_opid) continue;
		if (max_lid < an_out->max_layer_id) continue;
		if (max_lid == an_out->max_layer_id) {
			if (min_tid<an_out->max_temporal_id) min_tid = an_out->max_temporal_id;
			continue;
		}
		if (min_lid < an_out->max_layer_id) min_lid = an_out->max_layer_id;
	}
	if (pctx->max_layers) {
		if (max_lid >= pctx->max_layers) max_lid = pctx->max_layers-1;
	} else {
		if (max_lid >= 63) max_lid = 63;
	}

	if (pctx->max_sublayers) {
		if (max_tid > pctx->max_sublayers) max_tid = pctx->max_sublayers;
	} else {
		if (max_tid > 7) max_tid = 7;
	}

	u32 nb_layers = (max_lid - min_lid);
	if (!nb_layers) {
		nb_layers = 1;
		min_lid = 0;
	} else {
		min_lid++;
	}
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	gf_bs_write_int(bs, 0, 2);
	gf_bs_write_int(bs, nb_layers, 6);
	for (i=0; i<nb_layers; i++) {
		gf_bs_write_int(bs, 0, 4);
		gf_bs_write_int(bs, min_lid+i, 6);
		gf_bs_write_int(bs, min_tid+1, 3);
		gf_bs_write_int(bs, max_tid, 3);
		gf_bs_write_int(bs, 0, 1);
		gf_bs_write_int(bs, 0xFF, 7);
	}
	u8 *data;
	u32 data_size;
	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);
	gf_filter_pid_set_info_str(c_opid->opid, "hevc:linf", &PROP_DATA_NO_COPY(data, data_size) );
}

static void bs_split_copy_props_base(BSSplitCtx *ctx, BSSplitOut *c_opid, GF_FilterPid *pid, u32 svc_base_id)
{
	gf_filter_pid_copy_properties(c_opid->opid, pid);
	gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_ID, &PROP_UINT(c_opid->id));
	gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_ISOM_STSD_TEMPLATE, NULL);
	gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_ISOM_STSD_ALL_TEMPLATES, NULL);
	gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_ISOM_SUBTYPE, NULL);

	if (!c_opid->is_base) {
		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_ISOM_TRACK_TEMPLATE, NULL);
	} else {
		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_DOLBY_VISION, NULL);
	}
	gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_BITRATE, NULL);
	//we must force negctts on output MP4 otherwise the mux will adjust timings for some tracks (P frames)
	//but not for tracks containing only I or B frames
	gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_ISOM_FORCE_NEGCTTS, &PROP_BOOL(GF_TRUE));

	if (c_opid->width && c_opid->height) {
		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_WIDTH, &PROP_UINT(c_opid->width));
		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(c_opid->height));
		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_WIDTH_MAX, &PROP_UINT(c_opid->width));
		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_HEIGHT_MAX, &PROP_UINT(c_opid->height));
	}
	if (c_opid->dep_id)
		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_DEPENDENCY_ID, &PROP_UINT(c_opid->dep_id));

	if (ctx->sig_ltid) {
		gf_filter_pid_set_property_str(c_opid->opid, "max_layer_id", &PROP_UINT(c_opid->max_layer_id));
		gf_filter_pid_set_property_str(c_opid->opid, "max_temporal_id", &PROP_UINT(c_opid->max_temporal_id));
	}

	if (svc_base_id) {
		GF_PropertyValue prop_sbas;
		prop_sbas.type = GF_PROP_UINT_LIST;
		prop_sbas.value.uint_list.nb_items = 1;
		prop_sbas.value.uint_list.vals = &svc_base_id;
		//inject sbas reference
		gf_filter_pid_set_property_str(c_opid->opid, "isom:sbas", &prop_sbas);
	}

	//the rest: codecID, decoder config and codec-specific props are done elsewhere
}

static BSSplitOut *bs_split_get_out_stream(BSSplitCtx *ctx, BSSplitIn *pctx, Bool is_config, u32 layer_id, u32 temporal_id, Bool force_dv)
{
	u32 i, count;
	Bool update_linf=GF_FALSE;
	BSSplitOut *c_opid = NULL;
	BSSplitOut *max_c_opid = NULL;
	if (!layer_id && (temporal_id==1)) {
		c_opid = gf_list_get(pctx->opids, 0);
		c_opid->is_base = GF_TRUE;
	} else {
		count = gf_list_count(pctx->opids);
		for (i=0; i<count; i++) {
			c_opid = gf_list_get(pctx->opids, i);
			if ((c_opid->max_layer_id==99) && (c_opid->max_temporal_id==99))
				max_c_opid = c_opid;

			if (match_substream(c_opid, layer_id, temporal_id))
				break;

			c_opid = NULL;
		}
	}

	//assign config based on settings
	u32 min_lid_last = 0;
	u32 min_tid_last = 0;
	u32 max_lid = 0;
	u32 max_tid = 0;

	//no output or max TID not assigned, parse config
	if (!c_opid || !c_opid->max_temporal_id) {
		//if no config, assume layer split
		if (!ctx->ltid.nb_items) {
			max_lid = layer_id;
			max_tid = 99;
		} else {
			Bool found=GF_FALSE;
			for (i=0; i<ctx->ltid.nb_items; i++) {
				u32 max_layer_id = 0;
				u32 max_temporal_id = 0;
				char *ltid = ctx->ltid.vals[i];

				if (ltid[0] == '.') {
					max_layer_id = 99;
					sscanf(ltid, ".%u", &max_temporal_id);
				} else if (!strcmp(ltid, "all")) {
					max_layer_id = layer_id;
					max_temporal_id = temporal_id;
				} else if (strchr(ltid, '.')) {
					sscanf(ltid, "%u.%u", &max_layer_id, &max_temporal_id);
				} else {
					sscanf(ltid, "%u", &max_layer_id);
					max_temporal_id=99;
				}
				if (!max_temporal_id) max_temporal_id = 1;

				if (max_layer_id!=99) {
					if (min_lid_last < max_layer_id+1) min_lid_last = max_layer_id+1;
				}
				if (max_temporal_id!=99) {
					if (min_tid_last < max_temporal_id+1) min_tid_last = max_temporal_id+1;
				}

				//layer not specified or we match our layer, check temporal
				if ((max_layer_id==99) || (max_layer_id == layer_id)) {
					if (max_temporal_id >= temporal_id) {
						max_lid = max_layer_id;
						max_tid = max_temporal_id;
						found = GF_TRUE;
						break;
					}
				}
				else if ((max_layer_id > layer_id) && (max_temporal_id >= temporal_id)) {
					max_lid = max_layer_id;
					max_tid = max_temporal_id;
					found = GF_TRUE;
					break;
				}
			}
			//no stream found (e.g. max stream), use max stream if existing
			if (!c_opid && !found && max_c_opid) {
				c_opid = max_c_opid;
			}
			else if (!found) {
				//if we get here, last layer specified, put everything higher in this stream
				max_lid = 99;
				max_tid = 99;
			} else {
				min_lid_last = 0;
				min_tid_last = 0;
			}
		}
	}


	if (!c_opid) {
		const GF_PropertyValue *p;
		BSSplitOut *base_out = gf_list_last(pctx->opids);
		u32 base_id;
		GF_SAFEALLOC(c_opid, BSSplitOut);
		if (!c_opid) return NULL;
		gf_list_add(pctx->opids, c_opid);
		c_opid->opid = gf_filter_pid_new(ctx->filter);

		p = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_ID);
		base_id = p ? p->value.uint : 1;
		c_opid->id = base_id + 100 + gf_list_count(pctx->opids)-2;

		//check dependencies, first by layers, then by tid within layer
		//note that this is not sufficient: if we split L0(T1,T2)+L1(T1,T2) in 4 streams
		//L1T2 depends on both L1T1 and L0T2
		BSSplitOut *dep=NULL;
		count = gf_list_count(pctx->opids);
		for (i=count; i>1; i--) {
			dep = gf_list_get(pctx->opids, i-2);
			if ((dep->max_layer_id==layer_id) && (dep->max_temporal_id < temporal_id)) {
				break;
			}
			dep=NULL;
		}
		if (!dep) {
			for (i=count; i>1; i--) {
				dep = gf_list_get(pctx->opids, i-2);
				if (dep->max_layer_id<layer_id) {
					break;
				}
				dep = NULL;
			}
		}
		if (!dep)
			dep = gf_list_get(pctx->opids, 0);

		c_opid->dep_id = dep->id;

		//get first stream with same layer ID and set width/height
		for (i=0; i<count-1; i++) {
			dep = gf_list_get(pctx->opids, i);
			if (dep->width && dep->height && (dep->max_layer_id == layer_id)) {
				c_opid->width = dep->width;
				c_opid->height = dep->height;
				break;
			}
		}

		//ready to copy properties
		bs_split_copy_props_base(ctx, c_opid, pctx->ipid, (pctx->codec_id==GF_CODECID_AVC) ? base_id : 0);

		//not called during config, we must recreate decoder config and set codec IDs
		//this happens for:
		//- temporal sublayers which are usually not detected at configure
		//- SVC for which we need frames to detect LID TID
		//- DolbyVision RPU/EL
		if (!is_config) {
			Bool base_is_enh=GF_FALSE;
			//always remove decoder config from enhancement
			gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_DECODER_CONFIG, NULL);

			p = gf_filter_pid_get_property(base_out->opid, GF_PROP_PID_DECODER_CONFIG);
			if (!p) {
				base_is_enh = GF_TRUE;
				p = gf_filter_pid_get_property(base_out->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);
			}

			//DV split
			if (force_dv) {
				u8 dvcfg[24];
				//set dsi
				gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_DECODER_CONFIG, p);

				//set dv
				p = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DOLBY_VISION);
				if (p && (p->value.data.size==24)) memcpy(dvcfg, p->value.data.ptr, 24);
				else {
					memset(dvcfg, 0, 24);
					dvcfg[0] = 1;
					dvcfg[0] = 1;
				}
				//remove bl_present_flag, last lsb bit of byte 4
				dvcfg[3] &= ~1;

				gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_DOLBY_VISION, &PROP_DATA(dvcfg, 24));
				if ((pctx->codec_id==GF_CODECID_HEVC) || (pctx->codec_id==GF_CODECID_LHVC)) {
					gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_HEVC) );
				}
				else if (pctx->codec_id==GF_CODECID_AVC) {
					gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_AVC) );
				}
				//VVC, not defined yet
				else {
					gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_VVC) );
				}
			}
			//L-HEVC split, build decoder config(s)
			else if ((pctx->codec_id==GF_CODECID_HEVC) || (pctx->codec_id==GF_CODECID_LHVC)) {
				gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_LHVC) );

				if (p) {
					u8 *dsi;
					u32 dsi_size;
					GF_HEVCConfig *hvcc = gf_odf_hevc_cfg_read(p->value.data.ptr, p->value.data.size, (base_is_enh ? GF_TRUE : GF_FALSE) );
					hvcc->is_lhvc = GF_TRUE;
					gf_odf_hevc_cfg_write(hvcc, &dsi, &dsi_size);
					gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, &PROP_DATA_NO_COPY(dsi, dsi_size) );
					gf_odf_hevc_cfg_del(hvcc);
				}
				update_linf = GF_TRUE;
			} else {
				gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, p);
				if (pctx->codec_id==GF_CODECID_AVC) {
					gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_SVC) );
				}
			}
		}
	}
	if (!is_config && pctx->avc_state && (layer_id || (temporal_id>1)))
		bs_split_check_svc_config(pctx, c_opid);

	if (c_opid->max_temporal_id)
		return c_opid;

	c_opid->max_layer_id = max_lid;
	c_opid->max_temporal_id = max_tid;

	//keep track of min lid/tid defined in config so we don't put NALs on highest stream when intermediate stream
	//is not yet defined (temporal scalable)
	if (min_lid_last) c_opid->min_lid_plus_one = min_lid_last;
	if (min_tid_last) c_opid->min_tid_plus_one = min_tid_last;

	if (ctx->sig_ltid) {
		gf_filter_pid_set_property_str(c_opid->opid, "max_layer_id", &PROP_UINT(c_opid->max_layer_id));
		gf_filter_pid_set_property_str(c_opid->opid, "max_temporal_id", &PROP_UINT(c_opid->max_temporal_id));
	}
	if (update_linf) bs_split_update_hevc_linf(pctx, c_opid);
	return c_opid;
}

static void bs_split_svcc_del(GF_AVCConfig *svcc)
{
	if (!svcc) return;

	gf_list_reset(svcc->sequenceParameterSets);
	gf_list_reset(svcc->pictureParameterSets);
	gf_odf_avc_cfg_del(svcc);
}

static GF_Err avc_rewrite_pid_config(BSSplitCtx *ctx, BSSplitIn *pctx)
{
	GF_AVCConfig *avcc;
	BSSplitOut *c_opid;
	u8 *dsi;
	u32 dsi_size, i, count, base_id;
	u32 width=0, height=0;
	const GF_PropertyValue *prop;

	prop = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DECODER_CONFIG);
	if (!prop) return GF_OK;

	avcc = gf_odf_avc_cfg_read(prop->value.data.ptr, prop->value.data.size);
	if (!avcc) return GF_NON_COMPLIANT_BITSTREAM;

	if (!pctx->avc_state) pctx->avc_state = (AVCState *) gf_malloc(sizeof(AVCState));
	memset(pctx->avc_state, 0, sizeof(AVCState));
	if (!pctx->r_bs)
		pctx->r_bs = gf_bs_new((u8*)pctx->avc_state, 1, GF_BITSTREAM_READ);

	gf_odf_avc_cfg_write(avcc, &dsi, &dsi_size);
	pctx->nalu_size_length = avcc->nal_unit_size;

	//cleanup all svcc, they contain pointers to pctx->avcc andf pctx->svcc we are about to free
	for (i=1; i<gf_list_count(pctx->opids); i++) {
		c_opid = gf_list_get(pctx->opids, i);
		if (c_opid->svcc) {
			bs_split_svcc_del(c_opid->svcc);
			c_opid->svcc = NULL;
		}
	}

	if (pctx->avcc) gf_odf_avc_cfg_del(pctx->avcc);
	pctx->avcc = avcc;

#define AVC_READ_LIST(_list, _is_sps) \
	if (_list) {\
		count = gf_list_count(_list);\
		for (i=0; i<count; i++) {\
			GF_NALUFFParam *sl = gf_list_get(_list, i);\
			gf_bs_reassign_buffer(pctx->r_bs, sl->data, sl->size);\
			gf_bs_enable_emulation_byte_removal(pctx->r_bs, GF_TRUE);\
			s32 res = gf_avc_parse_nalu(pctx->r_bs, pctx->avc_state);\
			if (res>=0) {\
				sl->id = pctx->avc_state->last_ps_idx;\
				if (_is_sps) {\
					width = pctx->avc_state->sps[sl->id].width;\
					height = pctx->avc_state->sps[sl->id].height;\
				}\
			}\
		}\
	}\

	AVC_READ_LIST(pctx->avcc->sequenceParameterSets, GF_TRUE)
	AVC_READ_LIST(pctx->avcc->pictureParameterSets, GF_FALSE)
	AVC_READ_LIST(pctx->avcc->sequenceParameterSetExtensions, GF_FALSE)

	if (pctx->svcc) gf_odf_avc_cfg_del(pctx->svcc);
	pctx->svcc = NULL;
	prop = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);
	if (prop) {
		pctx->svcc = gf_odf_avc_cfg_read(prop->value.data.ptr, prop->value.data.size);
		if (!pctx->svcc) return GF_NON_COMPLIANT_BITSTREAM;
	}

	//rewrite config of base
	c_opid = gf_list_get(pctx->opids, 0);
	if (width && height) {
		c_opid->width = width;
		c_opid->height = height;
	}

	bs_split_copy_props_base(ctx, c_opid, pctx->ipid, 0);
	gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );
	gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);

	base_id = c_opid->id;

	//for SVC/MVC we need slices to figure out what shoud go where, which is why we keep a copy of avcC and svcC

	if (pctx->svcc) {
		AVC_READ_LIST(pctx->svcc->sequenceParameterSets, GF_FALSE)
		AVC_READ_LIST(pctx->svcc->pictureParameterSets, GF_FALSE)
		AVC_READ_LIST(pctx->svcc->sequenceParameterSetExtensions, GF_FALSE)
	}

	//copy props on all defined pids
	for (i=1; i<gf_list_count(pctx->opids); i++) {
		c_opid = gf_list_get(pctx->opids, i);

		bs_split_copy_props_base(ctx, c_opid, pctx->ipid, base_id);

		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_SVC) );

		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
		//reset decoder config, it will be recreated when parsing SVC slices
		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);
	}
	return GF_OK;
}


static GF_Err vvc_hevc_rewrite_pid_config(BSSplitCtx *ctx, BSSplitIn *pctx)
{
	GF_HEVCConfig *hvcc=NULL;
	GF_HEVCConfig *hvcc_out = NULL;
#ifndef GPAC_DISABLE_AV_PARSERS
	HEVCState *hvcc_state = NULL;
	VVCState *vvcc_state = NULL;
#else
	void *hvcc_state = NULL;
	void *vvcc_state = NULL;
#endif
	Bool is_vvc = GF_FALSE;
	GF_VVCConfig *vvcc=NULL;
	GF_VVCConfig *vvcc_out = NULL;
	u8 *dsi;
	u32 dsi_size, i, j, count;
	GF_List *param_array;
	const GF_PropertyValue *cfg, *cfg_enh;

	cfg = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DECODER_CONFIG);
	cfg_enh = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);
	if (!cfg && !cfg_enh) return GF_OK;

	if (pctx->codec_id==GF_CODECID_VVC) is_vvc = GF_TRUE;

	if (cfg) {
		if (is_vvc) {
			vvcc = gf_odf_vvc_cfg_read(cfg->value.data.ptr, cfg->value.data.size);
		} else {
			hvcc = gf_odf_hevc_cfg_read(cfg->value.data.ptr, cfg->value.data.size, (!cfg_enh && (pctx->codec_id==GF_CODECID_LHVC)) ? GF_TRUE : GF_FALSE);
		}
	} else {
		if (is_vvc) {
			vvcc = gf_odf_vvc_cfg_read(cfg_enh->value.data.ptr, cfg_enh->value.data.size);
		} else {
			hvcc = gf_odf_hevc_cfg_read(cfg_enh->value.data.ptr, cfg_enh->value.data.size, GF_TRUE);
		}
	}
	if (vvcc)
		pctx->nalu_size_length = vvcc->nal_unit_size;
	else if (hvcc)
		pctx->nalu_size_length = hvcc->nal_unit_size;
	else
		return GF_NON_COMPLIANT_BITSTREAM;


	//gather all param sets
restart:

	param_array = vvcc ? vvcc->param_array : hvcc->param_array;
	count = gf_list_count(param_array);
	for (i=0; i<count; i++) {
		GF_NALUFFParamArray *pa = gf_list_get(param_array, i);

		u32 count2 = gf_list_count(pa->nalus);
		for (j=0; j<count2; j++) {
			GF_List *pa_out_list;
			u32 layer_id=0, temporal_id=0;
			GF_NALUFFParamArray *pa_out = NULL;
			GF_NALUFFParam *nal = gf_list_get(pa->nalus, j);

			if (is_vvc) {
				layer_id = nal->data[0] & 0x3F;
				temporal_id = (nal->data[1] & 0x7);
			} else {
				layer_id = nal->data[0] & 1;
				layer_id <<= 5;
				layer_id |= (nal->data[1] >> 3) & 0x1F;
				temporal_id = (nal->data[1] & 0x7);
			}

			BSSplitOut *c_opid = bs_split_get_out_stream(ctx, pctx, GF_TRUE, layer_id, temporal_id, GF_FALSE);
			if (!c_opid) return GF_OUT_OF_MEM;

			if (is_vvc) {
				if (!vvcc_out) {
					GF_SAFEALLOC(vvcc_out, GF_VVCConfig);
					if (!vvcc_out) {
						gf_odf_vvc_cfg_del(vvcc);
						return GF_OUT_OF_MEM;
					}
					memcpy(vvcc_out, vvcc, sizeof(GF_VVCConfig));
					vvcc_out->general_constraint_info = gf_malloc(sizeof(u8)*vvcc_out->num_constraint_info);
					if (!vvcc_out->general_constraint_info) {
						gf_odf_vvc_cfg_del(vvcc);
						return GF_OUT_OF_MEM;
					}
					memcpy(vvcc_out->general_constraint_info, vvcc->general_constraint_info, sizeof(u8)*vvcc_out->num_constraint_info);
					vvcc_out->param_array = gf_list_new();
					if (!vvcc_out->param_array) {
						gf_odf_vvc_cfg_del(vvcc);
						gf_odf_vvc_cfg_del(vvcc_out);
						return GF_OUT_OF_MEM;
					}
				}
				pa_out_list = vvcc_out->param_array;
			} else {
				if (!hvcc_out) {
					GF_SAFEALLOC(hvcc_out, GF_HEVCConfig);
					if (!hvcc_out) {
						gf_odf_hevc_cfg_del(hvcc);
						return GF_OUT_OF_MEM;
					}
					memcpy(hvcc_out, hvcc, sizeof(GF_HEVCConfig));
					hvcc_out->param_array = gf_list_new();
					if (!hvcc_out->param_array) {
						gf_odf_hevc_cfg_del(hvcc);
						gf_odf_hevc_cfg_del(hvcc_out);
						return GF_OUT_OF_MEM;
					}
				}
				pa_out_list = hvcc_out->param_array;
			}

			for (u32 k=0; k<gf_list_count(pa_out_list); k++) {
				pa_out = gf_list_get(pa_out_list, k);
				if (pa_out->type == pa->type) break;
				pa_out = NULL;
			}
			if (pa_out == NULL) {
				GF_SAFEALLOC(pa_out, GF_NALUFFParamArray);
				if (pa_out) pa_out->nalus = gf_list_new();
				if (!pa_out || !pa_out->nalus) {
					if (vvcc) gf_odf_vvc_cfg_del(vvcc);
					if (vvcc_out) gf_odf_vvc_cfg_del(vvcc_out);
					if (hvcc) gf_odf_hevc_cfg_del(hvcc);
					if (hvcc_out) gf_odf_hevc_cfg_del(hvcc_out);
					if (pa_out) gf_free(pa_out);
					return GF_OUT_OF_MEM;
				}
				pa_out->type = pa->type;
				pa_out->array_completeness = 1;
				gf_list_add(pa_out_list, pa_out);
			}
			GF_NALUFFParam *nal_out;
			GF_SAFEALLOC(nal_out, GF_NALUFFParam);
			nal_out->size = nal->size;
			nal_out->data = gf_malloc(sizeof(u8) * nal->size);
			memcpy(nal_out->data, nal->data, sizeof(u8) * nal->size);
			gf_list_add(pa_out->nalus, nal_out);
		}
	}

	if (vvcc) {
		gf_odf_vvc_cfg_del(vvcc);
		vvcc = NULL;
	}
	if (hvcc) {
		gf_odf_hevc_cfg_del(hvcc);
		hvcc = NULL;
	}

	//for hevc we might get l-hevc config on main stream
	if (cfg_enh && cfg && !is_vvc) {
		hvcc = gf_odf_hevc_cfg_read(cfg_enh->value.data.ptr, cfg_enh->value.data.size, GF_TRUE);
		if (!hvcc) {
			if (hvcc_out) gf_odf_hevc_cfg_del(hvcc_out);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		cfg_enh = NULL;
		goto restart;
	}

	GF_Err e = GF_OK;
	vvcc = vvcc_out;
	hvcc = hvcc_out;

#ifndef GPAC_DISABLE_AV_PARSERS
	if (hvcc) GF_SAFEALLOC(hvcc_state, HEVCState);
	if (vvcc) GF_SAFEALLOC(vvcc_state, VVCState);
#endif


	count = gf_list_count(pctx->opids);
	for (i=0; i<count; i++) {
		u32 max_w=0, max_h=0;
		GF_List *orig_pa_list, *out_param_array;
		BSSplitOut *c_opid = gf_list_get(pctx->opids, i);


		if (vvcc) {
			orig_pa_list = vvcc->param_array;
			vvcc->param_array = gf_list_new();
			if (!vvcc->param_array) {
				vvcc->param_array = orig_pa_list;
				e = GF_OUT_OF_MEM;
				break;
			}
			out_param_array = vvcc->param_array;
		}
		else if (hvcc) {
			orig_pa_list = hvcc->param_array;
			hvcc->is_lhvc = !c_opid->is_base ? GF_TRUE : GF_FALSE;
			hvcc->param_array = gf_list_new();
			if (!hvcc->param_array) {
				hvcc->param_array = orig_pa_list;
				e = GF_OUT_OF_MEM;
				break;
			}
			out_param_array = hvcc->param_array;
		} else {
			e = GF_SERVICE_ERROR;
			break;
		}

		u32 count2 = gf_list_count(orig_pa_list);
		for (j=0; j<count2; j++) {
			GF_NALUFFParamArray *pa = gf_list_get(orig_pa_list, j);
			GF_NALUFFParamArray *pa_out = NULL;
			for (u32 k=0; k<gf_list_count(pa->nalus); k++) {
				Bool do_add = GF_FALSE;
				u32 layer_id=0, temporal_id=0;
				GF_NALUFFParam *nal = gf_list_get(pa->nalus, k);
				if (vvcc) {
					layer_id = nal->data[0] & 0x3F;
					temporal_id = (nal->data[1] & 0x7);
				} else {
					//u32 nal_type = (nal->data[0] & 0x7E) >> 1;
					layer_id = nal->data[0] & 1;
					layer_id <<= 5;
					layer_id |= (nal->data[1] >> 3) & 0x1F;
					temporal_id = (nal->data[1] & 0x7);

					if (pa->type == GF_HEVC_NALU_VID_PARAM) do_add = GF_TRUE;
				}
				if (match_substream(c_opid, layer_id, temporal_id))
					do_add = GF_TRUE;

				if (!do_add) continue;

#ifndef GPAC_DISABLE_AV_PARSERS
				if (hvcc_state) {
					u8 ntype, tid, lid;
					s32 res = gf_hevc_parse_nalu(nal->data, nal->size, hvcc_state, &ntype, &tid, &lid);
					if ((res>=0) && (pa->type == GF_HEVC_NALU_SEQ_PARAM)) {
						if (max_w < hvcc_state->sps[hvcc_state->last_parsed_sps_id].width)
							max_w = hvcc_state->sps[hvcc_state->last_parsed_sps_id].width;
						if (max_h < hvcc_state->sps[hvcc_state->last_parsed_sps_id].height)
							max_h = hvcc_state->sps[hvcc_state->last_parsed_sps_id].height;
					}
					if ((res>=0) && (pa->type==GF_HEVC_NALU_VID_PARAM)) {
						pctx->max_layers = hvcc_state->vps[hvcc_state->last_parsed_vps_id].max_layers;
						pctx->max_sublayers = hvcc_state->vps[hvcc_state->last_parsed_vps_id].max_sub_layers;
					}
				}
				if (vvcc_state) {
					u8 ntype, tid, lid;
					s32 res = gf_vvc_parse_nalu(nal->data, nal->size, vvcc_state, &ntype, &tid, &lid);
					if ((res>=0) && (pa->type == GF_VVC_NALU_SEQ_PARAM)) {
						if (max_w < vvcc_state->sps[vvcc_state->last_parsed_sps_id].width)
							max_w = vvcc_state->sps[vvcc_state->last_parsed_sps_id].width;
						if (max_h < vvcc_state->sps[vvcc_state->last_parsed_sps_id].height)
							max_h = vvcc_state->sps[vvcc_state->last_parsed_sps_id].height;
					}
					if ((res>=0) && (pa->type==GF_VVC_NALU_VID_PARAM)) {
						pctx->max_layers = vvcc_state->vps[vvcc_state->last_parsed_vps_id].max_layers;
						pctx->max_sublayers = vvcc_state->vps[vvcc_state->last_parsed_vps_id].max_sub_layers;
					}
				}
#endif

				if (pa_out == NULL) {
					GF_SAFEALLOC(pa_out, GF_NALUFFParamArray);
					if (pa_out) pa_out->nalus = gf_list_new();
					if (!pa_out || !pa_out->nalus) {
						if (pa_out) gf_free(pa_out);
						e = GF_OUT_OF_MEM;
						break;
					}
					pa_out->type = pa->type;
					pa_out->array_completeness = pa->array_completeness;
					gf_list_add(out_param_array, pa_out);
				}
				gf_list_add(pa_out->nalus, nal);
			}
			if (e) break;
		}
		if (e) {
			if (vvcc) vvcc->param_array = orig_pa_list;
			else hvcc->param_array = orig_pa_list;
			break;
		}
		if (vvcc)
			gf_odf_vvc_cfg_write(vvcc, &dsi, &dsi_size);
		else
			gf_odf_hevc_cfg_write(hvcc, &dsi, &dsi_size);

		if (max_w && max_h) {
			c_opid->width = max_w;
			c_opid->height = max_h;
		}


		bs_split_copy_props_base(ctx, c_opid, pctx->ipid, 0);

		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );
		gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL );

		if (is_vvc) {

		} else {
			if (hvcc->is_lhvc) {
				gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_LHVC) );
			} else {
				gf_filter_pid_set_property(c_opid->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_HEVC) );
			}
			bs_split_update_hevc_linf(pctx, c_opid);
		}

		count2 = gf_list_count(out_param_array);
		for (j=0; j<count2; j++) {
			GF_NALUFFParamArray *pa = gf_list_get(out_param_array, j);
			gf_list_del(pa->nalus);
			gf_free(pa);
		}
		gf_list_del(out_param_array);

		if (vvcc) vvcc->param_array = orig_pa_list;
		else if (hvcc) hvcc->param_array = orig_pa_list;
	}

#ifndef GPAC_DISABLE_AV_PARSERS
	if (hvcc_state) gf_free(hvcc_state);
	if (vvcc_state) gf_free(vvcc_state);
#endif

	if (vvcc) gf_odf_vvc_cfg_del(vvcc);
	if (hvcc) gf_odf_hevc_cfg_del(hvcc);
	return GF_OK;
}


static GF_Err none_rewrite_pid_config(BSSplitCtx *ctx, BSSplitIn *pctx)
{
	u32 i, count;

	count = gf_list_count(pctx->opids);
	for (i=0; i<count; i++) {
		BSSplitOut *c_opid = gf_list_get(pctx->opids, i);
		gf_filter_pid_copy_properties(c_opid->opid, pctx->ipid);
	}
	return GF_OK;
}

static GF_Err none_split_packet(BSSplitCtx *ctx, BSSplitIn *pctx, GF_FilterPacket *pck)
{
	BSSplitOut *c_opid = gf_list_get(pctx->opids, 0);
	if (c_opid)
		return gf_filter_pck_forward(pck, c_opid->opid);
	return GF_OK;
}

static GF_Err nalu_split_packet(BSSplitCtx *ctx, BSSplitIn *pctx, GF_FilterPacket *pck, u32 codec_type)
{
	u32 size, pck_size, min_nal_size;
	GF_Err e;
	u64 pck_ts;
	Bool has_svc_prefix = GF_FALSE;
	const u8 *data = gf_filter_pck_get_data(pck, &pck_size);
	if (!data) {
		BSSplitOut *c_opid = gf_list_get(pctx->opids, 0);
		if (c_opid)
			return gf_filter_pck_forward(pck, c_opid->opid);
		return GF_OK;
	}

	pck_ts = gf_filter_pck_get_dts(pck);
	if (pck_ts == GF_FILTER_NO_TS)
		pck_ts = gf_filter_pck_get_cts(pck);
	if (pck_ts == GF_FILTER_NO_TS)
		pck_ts = 0;
	if (!pctx->first_ts_plus_one) {
		pctx->first_ts_plus_one = pck_ts+1;
	}
	min_nal_size = codec_type ? 2 : 1;

	size=0;
	while (size<pck_size) {
		Bool force_dv = GF_FALSE;
		u32 nal_type=0;
		u32 layer_id = 0;
		u32 temporal_id = 0;
		u32 nal_hdr = pctx->nalu_size_length;
		u32 nal_size = 0;
		while (nal_hdr) {
			nal_size |= data[size];
			size++;
			nal_hdr--;
			if (!nal_hdr) break;
			nal_size<<=8;
		}
		if (size + nal_size > pck_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[BSSplit] Invalid NAL size %d but remain %d\n", nal_size, pck_size-size));
			e = GF_NON_COMPLIANT_BITSTREAM;
			break;
		}
		if (nal_size < min_nal_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[BSSplit] Invalid NAL size %d but mn size %d\n", nal_size, min_nal_size));
			e = GF_NON_COMPLIANT_BITSTREAM;
			break;
		}

		//AVC
		if (codec_type==0) {
			Bool get_dqid=GF_FALSE;
			gf_bs_reassign_buffer(pctx->r_bs, data+size, nal_size);
			gf_avc_parse_nalu(pctx->r_bs, pctx->avc_state);
			nal_type = pctx->avc_state->last_nal_type_parsed;
			temporal_id = 1;
			//remember we had an svc prefix
			if (nal_type == GF_AVC_NALU_SVC_PREFIX_NALU) {
				has_svc_prefix = GF_TRUE;
			}
			//if slice is scalable extension, get dqid
			else if ((nal_type == GF_AVC_NALU_SVC_SLICE) && pctx->avc_state->s_info.pps) {
				//svc prefix is no longer valid
				has_svc_prefix = GF_FALSE;
				get_dqid = GF_TRUE;
			}
			//don't forward extractors
			else if (nal_type == GF_AVC_NALU_FF_EXTRACTOR) {
				size += nal_size;
				continue;
			}
			//todo, find a way to differentiate between FF AVC aggregator and DV EL ...


			//if we had an svc prefix, get dqid
			if (has_svc_prefix) {
				get_dqid = GF_TRUE;
			}

			if (get_dqid) {
				if (ctx->svcqid)
					layer_id = pctx->avc_state->s_info.svc_nalhdr.quality_id;
				else
					layer_id = pctx->avc_state->s_info.svc_nalhdr.dependency_id;

				temporal_id = 1+pctx->avc_state->s_info.svc_nalhdr.temporal_id;
			}

			//force layerID 100 for DV
			if ((nal_type == GF_AVC_NALU_DV_RPU) || (nal_type == GF_AVC_NALU_DV_EL)) {
				layer_id = 100;
				force_dv = GF_TRUE;
			}

		}
		//HEVC
		else if (codec_type==1) {
			nal_type = (data[size] & 0x7E) >> 1;

			layer_id = data[size] & 1;
			layer_id <<= 5;
			layer_id |= (data[size+1] >> 3) & 0x1F;
			temporal_id = (data[size+1] & 0x7);

			//force layerID 100 for DV
			if ((nal_type == GF_HEVC_NALU_DV_RPU) || (nal_type == GF_HEVC_NALU_DV_EL)) {
				layer_id = 100;
				force_dv = GF_TRUE;
			}
			//don't forward extractors nor aggregators
			else if ((nal_type == GF_HEVC_NALU_FF_EXTRACTOR) || (nal_type == GF_HEVC_NALU_FF_AGGREGATOR)) {
				size += nal_size;
				continue;
			}
		}
		//VVC
		else if (codec_type==2) {
			layer_id = data[size] & 0x3F;
			temporal_id = data[size+1] & 0x7;
			nal_type = data[size+1] >> 3;
		}

		BSSplitOut *c_opid = bs_split_get_out_stream(ctx, pctx, GF_FALSE, layer_id, temporal_id, force_dv);
		if (!c_opid) {
			e = GF_OUT_OF_MEM;
			break;
		}

		u8 *out_data;

		if (!c_opid->is_init) {
			u64 ts_diff = pck_ts - (pctx->first_ts_plus_one-1);
			c_opid->is_init = GF_TRUE;

			//tsdiff not 0, we are starting a new split stream on a temporal sublayer, inject empty AU with a single NALU AU delim
			//we do so because we need to make sure the muxer will not screw up timing:
			//for mp4 not fragmented, not injecting will result in this sample (being the first) having dts=0 plus edit
			//which will unalign the tracks in terms of dts/ctts, which is what implicit reconstruction uses....
			if (ts_diff) {
				u32 isize;
				u8 *idata;

				if (codec_type==1) isize=3;
				else if (codec_type==2) isize=3;
				else isize=2;
				GF_FilterPacket *inject_pck = gf_filter_pck_new_alloc(c_opid->opid, isize+pctx->nalu_size_length, &idata);
				if (!inject_pck) {
					e = GF_OUT_OF_MEM;
					break;
				}
				GF_BitStream *aud_bs = gf_bs_new(idata, isize+pctx->nalu_size_length, GF_BITSTREAM_WRITE);
				if (!aud_bs) {
					e = GF_OUT_OF_MEM;
					break;
				}

				gf_bs_write_int(aud_bs, isize, 8*pctx->nalu_size_length);
				if (codec_type==1) {
					gf_bs_write_int(aud_bs, 0, 1);
					gf_bs_write_int(aud_bs, GF_HEVC_NALU_ACCESS_UNIT, 6);
					gf_bs_write_int(aud_bs, layer_id, 6);
					gf_bs_write_int(aud_bs, temporal_id, 3);
					/*pic-type - by default we signal all slice types possible*/
					gf_bs_write_int(aud_bs, 2, 3);
					gf_bs_write_int(aud_bs, 1, 1); //stop bit
					gf_bs_write_int(aud_bs, 0, 4); //4 bits to 0
				} else if (codec_type==2) {
					gf_bs_write_int(aud_bs, 0, 1);
					gf_bs_write_int(aud_bs, 0, 1);
					gf_bs_write_int(aud_bs, layer_id, 6);
					gf_bs_write_int(aud_bs, GF_VVC_NALU_ACCESS_UNIT, 5);
					gf_bs_write_int(aud_bs, temporal_id, 3);
					gf_bs_write_int(aud_bs, 0, 1);
					/*pic-type - by default we signal all slice types possible*/
					gf_bs_write_int(aud_bs, 2, 3);
					gf_bs_write_int(aud_bs, 1, 1); //stop bit
					gf_bs_write_int(aud_bs, 0, 3); //3 bits to 0
				} else {
					gf_bs_write_int(aud_bs, (data[size] & 0x60) | GF_AVC_NALU_ACCESS_UNIT, 8);
					gf_bs_write_int(aud_bs, 0xF0 , 8); /*7 "all supported NALUs" (=111) + rbsp trailing (10000)*/;
				}
				gf_bs_del(aud_bs);

				gf_filter_pck_set_dts(inject_pck, pctx->first_ts_plus_one-1);
				gf_filter_pck_set_cts(inject_pck, pctx->first_ts_plus_one-1);
				gf_filter_pck_set_duration(inject_pck, (u32) ts_diff);
				gf_filter_pck_set_framing(inject_pck, GF_TRUE, GF_TRUE);
				gf_filter_pck_send(inject_pck);
			}
		}

		if (!c_opid->pck) {
			c_opid->pck = gf_filter_pck_new_alloc(c_opid->opid, nal_size+pctx->nalu_size_length, &out_data);
			if (!c_opid->pck) {
				e = GF_OUT_OF_MEM;
				break;
			}
			gf_filter_pck_merge_properties(pck, c_opid->pck);

			if (!c_opid->is_base)
				gf_filter_pck_set_sap(c_opid->pck, GF_FILTER_SAP_NONE);
		} else {
			e = gf_filter_pck_expand(c_opid->pck, nal_size+pctx->nalu_size_length, NULL, &out_data, NULL);
			if (e) break;
		}
		memcpy(out_data, data + size - pctx->nalu_size_length, nal_size+pctx->nalu_size_length);

		size += nal_size;
	}

	u32 i, count = gf_list_count(pctx->opids);
	for (i=0; i<count; i++) {
		BSSplitOut *c_opid = gf_list_get(pctx->opids, i);
		if (c_opid->pck) {
			gf_filter_pck_send(c_opid->pck);
			c_opid->pck = NULL;
		}
	}
	return e;
}

static GF_Err avc_split_packet(BSSplitCtx *ctx, BSSplitIn *pctx, GF_FilterPacket *pck)
{
	return nalu_split_packet(ctx, pctx, pck, 0);
}
static GF_Err hevc_split_packet(BSSplitCtx *ctx, BSSplitIn *pctx, GF_FilterPacket *pck)
{
	return nalu_split_packet(ctx, pctx, pck, 1);
}
static GF_Err vvc_split_packet(BSSplitCtx *ctx, BSSplitIn *pctx, GF_FilterPacket *pck)
{
	return nalu_split_packet(ctx, pctx, pck, 2);
}

static void bs_split_reset_stream(GF_Filter *filter, BSSplitCtx *ctx, BSSplitIn *pctx, Bool is_finalize, Bool is_reconf)
{
	if (!pctx) return;

	while (gf_list_count(pctx->opids)) {
		BSSplitOut *c_opid = gf_list_pop_back(pctx->opids);
		if (!is_finalize)
			gf_filter_pid_remove(c_opid->opid);
		bs_split_svcc_del(c_opid->svcc);
		gf_free(c_opid);
	}
	if (pctx->avcc) {
		gf_odf_avc_cfg_del(pctx->avcc);
		pctx->avcc = NULL;
	}
	if (pctx->svcc) {
		gf_odf_avc_cfg_del(pctx->svcc);
		pctx->svcc = NULL;
	}
	if (pctx->avc_state) {
		gf_free(pctx->avc_state);
		pctx->avc_state = NULL;
	}
	if (pctx->r_bs) {
		gf_bs_del(pctx->r_bs);
		pctx->r_bs = NULL;
	}
	if (is_reconf)
		return;

	gf_list_del_item(ctx->pids, pctx);
	gf_list_del(pctx->opids);
	gf_free(pctx);

}

static GF_Err bs_split_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	BSSplitCtx *ctx = (BSSplitCtx *) gf_filter_get_udta(filter);
	BSSplitIn *pctx = gf_filter_pid_get_udta(pid);

	//disconnect of src pid
	if (is_remove) {
		if (pctx) {
			bs_split_reset_stream(filter, ctx, pctx, GF_FALSE, GF_FALSE);
			gf_filter_pid_set_udta(pid, NULL);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!pctx) {
		BSSplitOut *base_opid;
		GF_SAFEALLOC(pctx, BSSplitIn);
		if (!pctx) return GF_OUT_OF_MEM;
		GF_SAFEALLOC(base_opid, BSSplitOut);
		if (!base_opid) {
			gf_free(pctx);
			return GF_OUT_OF_MEM;
		}
		pctx->ipid = pid;
		pctx->opids = gf_list_new();
		gf_list_add(pctx->opids, base_opid);
		gf_filter_pid_set_udta(pid, pctx);
		gf_list_add(ctx->pids, pctx);
		base_opid->opid = gf_filter_pid_new(filter);
		if (!base_opid->opid) return GF_OUT_OF_MEM;

		const GF_PropertyValue *p = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_ID);
		base_opid->id = p ? p->value.uint : 1;
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	gf_fatal_assert(prop);

	switch (prop->value.uint) {
	case GF_CODECID_AVC:
	case GF_CODECID_SVC:
	case GF_CODECID_MVC:
		pctx->rewrite_pid_config = avc_rewrite_pid_config;
		pctx->split_packet = avc_split_packet;
		break;
	case GF_CODECID_HEVC:
	case GF_CODECID_LHVC:
		pctx->rewrite_pid_config = vvc_hevc_rewrite_pid_config;
		pctx->split_packet = hevc_split_packet;
		break;
	case GF_CODECID_VVC:
		pctx->rewrite_pid_config = vvc_hevc_rewrite_pid_config;
		pctx->split_packet = vvc_split_packet;
		break;
	default:
		pctx->rewrite_pid_config = none_rewrite_pid_config;
		pctx->split_packet = none_split_packet;
		break;
	}

	//change of codec, reset output
	if (pctx->codec_id && (pctx->codec_id != prop->value.uint)) {
		bs_split_reset_stream(filter, ctx, pctx, GF_FALSE, GF_TRUE);
	}
	pctx->codec_id = prop->value.uint;

	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	//rewrite asap - waiting for first packet could lead to issues further down the chain, especially for movie fragments
	//were the init segment could have already been flushed at the time we will dispatch the first packet
	return pctx->rewrite_pid_config(ctx, pctx);
}


static GF_Err bs_split_process(GF_Filter *filter)
{
	u32 i, count;
	BSSplitCtx *ctx = (BSSplitCtx *) gf_filter_get_udta(filter);

	count = gf_filter_get_ipid_count(filter);
	for (i=0; i<count; i++) {
		BSSplitIn *pctx;
		GF_FilterPacket *pck;
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		if (!pid) break;
		pctx = gf_filter_pid_get_udta(pid);
		if (!pctx) break;

		while (1) {
			pck = gf_filter_pid_get_packet(pid);
			if (!pck) {
				if (gf_filter_pid_is_eos(pctx->ipid)) {
					for (u32 j=0; j<gf_list_count(pctx->opids); j++) {
						BSSplitOut *c_opid = gf_list_get(pctx->opids, j);
						gf_filter_pid_set_eos(c_opid->opid);
					}
				}
				break;
			}
			pctx->split_packet(ctx, pctx, pck);
			gf_filter_pid_drop_packet(pid);
		}
	}
	return GF_OK;
}

static GF_Err bs_split_initialize(GF_Filter *filter)
{
	BSSplitCtx *ctx = (BSSplitCtx *) gf_filter_get_udta(filter);
	ctx->pids = gf_list_new();

	ctx->filter = filter;
	return GF_OK;
}
static void bs_split_finalize(GF_Filter *filter)
{
	BSSplitCtx *ctx = (BSSplitCtx *) gf_filter_get_udta(filter);
	while (gf_list_count(ctx->pids)) {
		BSSplitIn *pctx = gf_list_pop_back(ctx->pids);

		bs_split_reset_stream(filter, ctx, pctx, GF_TRUE, GF_FALSE);
	}
	gf_list_del(ctx->pids);
}

#define OFFS(_n)	#_n, offsetof(BSSplitCtx, _n)
static GF_FilterArgs BSSplitArgs[] =
{
	{ OFFS(ltid), "temporal and layer ID of output streams", GF_PROP_STRING_LIST, NULL, NULL, 0},
	{ OFFS(svcqid), "use qualityID instead of dependencyID for SVC splitting", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(sig_ltid), "signal maximum temporal (`max_temporal_id`) and layer ID (`max_layer_id`) of output streams (mostly used for debug)", GF_PROP_BOOL, "false", NULL, 0},
	{0}
};

static const GF_FilterCapability BSSplitCaps[] =
{
	//we want the filter to act as passthrough for other media, so we accept everything
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_DEPENDENCY_ID, 0),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
};

GF_FilterRegister BSSplitRegister = {
	.name = "bssplit",
	GF_FS_SET_DESCRIPTION("Compressed layered bitstream splitter")
	GF_FS_SET_HELP("This filter splits input stream by layers and sublayers\n"
	"\n"
	"The filter supports AVC|H264, HEVC and VVC stream splitting and is pass-through for other codec types.\n"
	"\n"
	"Splitting is based on temporalID value (start from 1) and layerID value (start from 0).\n"
	"For AVC|H264, layerID is the dependency value, or quality value if `svcqid` is set.\n"
	"\n"
	"Each input stream is filtered according to the `ltid` option as follows:\n"
	"- no value set: input stream is split by layerID, i.e. each layer creates an output\n"
	"- `all`: input stream is split by layerID and temporalID, i.e. each {layerID,temporalID} creates an output\n"
	"- `lID`: input stream is split according to layer `lID` value, and temporalID is ignored\n"
	"- `.tID`: input stream is split according to temporal sub-layer `tID` value and layerID is ignored\n"
	"- `lID.tID`: input stream is split according to layer `lID` and sub-layer `tID` values\n"
	"\n"
	"Note: A tID value of 0 in `ltid` is equivalent to value 1.\n"
	"\n"
	"Multiple values can be given in `ltid`, in which case each value gives the maximum {layerID,temporalID} values for the current layer.\n"
	"A few examples on an input with 2 layers each with 2 temporal sublayers:\n"
	"- `ltid=0.2`: this will split the stream in:\n"
	"  - one stream with {lID=0,tID=1} and {lID=0,tID=2} NAL units\n"
	"  - one stream with all other layers/substreams\n"
	"- `ltid=0.1,1.1`: this will split the stream in:\n"
	"  - one stream with {lID=0,tID=1} NAL units\n"
	"  - one stream with {lID=0,tID=2}, {lID=1,tID=1} NAL units\n"
	"  - one stream with the rest {lID=0,tID=2}, {lID=1,tID=2} NAL units\n"
	"- `ltid=0.1,0.2`: this will split the stream in:\n"
	"  - one stream with {lID=0,tID=1} NAL units\n"
	"  - one stream with {lID=0,tID=2} NAL units\n"
	"  - one stream with the rest {lID=1,tID=1}, {lID=1,tID=2} NAL units\n"
	"\n"
	"The filter can also be used on AVC and HEVC DolbyVision streams to split base stream and DV RPU/EL.\n"
	"\n"
	"The filter does not create aggregator or extractor NAL units.\n"
	)
	.private_size = sizeof(BSSplitCtx),
	.max_extra_pids = 0xFFFFFFFF,
	.flags = GF_FS_REG_EXPLICIT_ONLY|GF_FS_REG_ALLOW_CYCLIC,
	.args = BSSplitArgs,
	SETCAPS(BSSplitCaps),
	.initialize = bs_split_initialize,
	.finalize = bs_split_finalize,
	.configure_pid = bs_split_configure_pid,
	.process = bs_split_process
};

#endif

const GF_FilterRegister *bssplit_register(GF_FilterSession *session)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_BSSPLIT)
	return (const GF_FilterRegister *) &BSSplitRegister;
#else
	return NULL;
#endif
}
