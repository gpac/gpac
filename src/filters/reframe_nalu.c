/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / NALU (AVC & HEVC)  reframer filter
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

#include <gpac/avparse.h>
#include <gpac/constants.h>
#include <gpac/filters.h>
#include <gpac/internal/media_dev.h>
//for oinf stuff
#include <gpac/internal/isomedia_dev.h>

#define CTS_POC_OFFSET_SAFETY	1000

//storage for nal header + slice header when not fully included in an input buffer - this needs to be big enough to hold a slice header
//note that we only copy the entire input packet in an internal buffer when the current NAL needs to be fully available for parsing (VPS, SPS, PPS),
//or for temporary storage (SEI)
//otherwise we copy remaining bytes in the hdr store if a startcode may be split across two input packets
#define SAFETY_NAL_STORE	50

typedef struct
{
	u64 pos;
	Double duration;
} NALUIdx;


typedef struct
{
	u32 layer_id_plus_one;
	u32 min_temporal_id, max_temporal_id;
} LHVCLayerInfo;

typedef struct
{
	//filter args
	GF_Fraction fps;
	Double index;
	Bool explicit, force_sync, strict_poc, nosei, importer, subsamples, nosvc, novpsext, deps, seirw, audelim, analyze;
	u32 nal_length;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	//read bitstream for AVC/HEVC parsing
	GF_BitStream *bs_r;
	//write bitstream for nalus size length rewrite
	GF_BitStream *bs_w;
	//current CTS/DTS of the stream, may be overriden by input packet if not file (eg TS PES)
	u64 cts, dts, prev_dts;
	u32 pck_duration;
	//basic config stored here: with, height CRC of base and enh layer decoder config, sample aspect ratio
	//when changing, a new pid config will be emitted
	u32 width, height;
	u32 crc_cfg, crc_cfg_enh;
	GF_Fraction sar;
	GF_Fraction cur_fps;

	//duration of the file if known
	GF_Fraction duration;
	//playback start range
	Double start_range;
	//indicates we are in seek, packets before start range should be marked
	Bool in_seek;
	//set once we play something
	Bool is_playing;
	//is a file, is a file fully loaded on disk (local or download done)
	Bool is_file, file_loaded;
	//initial PLAY command found
	Bool initial_play_done;

	//list of RAP entry points
	NALUIdx *indexes;
	u32 index_alloc_size, index_size;


	//timescale of the input pid if any, 0 otherwise
	u32 timescale;
	//framing flag of input packet when input pid has timing (eg is not a file)
	Bool input_is_au_start;

	GF_FilterPacket *src_pck;

	Bool full_au_source;

	//total delay in frames between decode and presentation
	s32 max_total_delay;
	//max size codable with our nal_length setting
	u32 max_nalu_size_allowed;

	//position in input packet from which we resume parsing
	u32 resume_from;
	//prevents message about possible NAL size optimizaion at finalization
	Bool nal_adjusted;

	//avc/hevc switch
	Bool is_hevc;
	//name of the logger
	const char *log_name;

	//list of packet (in decode order !!) not yet dispatched.
	//Dispatch depends on the mode:
	//strict_poc=0: we wait after each IDR until we find a stable poc diff between pictures, controled by poc_probe_done
	//strict_poc=1: we dispatch only after IDR or at the end (huge delay)
	GF_List *pck_queue;
	//dts of the last IDR found
	u64 dts_last_IDR;
	//max size of NALUs in the bitstream
	u32 max_nalu_size;

	//we store a few bytes here to make sure we have at least start code and NAL type
	//we also store here NALUs we must completely parse (xPS, SEIs)
	u32 bytes_in_header;
	char *hdr_store;
	u32 hdr_store_size, hdr_store_alloc;

	//list of param sets found
	GF_List *sps, *pps, *vps, *sps_ext, *pps_svc;
	//set to true if one of the PS has been modified, will potentially trigger a PID reconfigure
	Bool ps_modified;

	//stats
	u32 nb_idr, nb_i, nb_p, nb_b, nb_sp, nb_si, nb_sei, nb_nalus, nb_aud;

	//frame has intra slice
	Bool has_islice;
	//AU is rap
	GF_FilterSAPType au_sap;
	//frame first slice
	Bool first_slice_in_au;
	//paff used - NEED FURTHER CHECKING
	Bool is_paff;
	Bool bottom_field_flag;
	//SEI recovery count - if 0 and I slice only frame, openGOP detection (avc)
	s32 sei_recovery_frame_count;
	u32 use_opengop_gdr;
	//poc compute variables
	s32 last_poc, max_last_poc, max_last_b_poc, poc_diff, prev_last_poc, min_poc, poc_shift;
	//set to TRUE once 3 frames with same min poc diff are found, enabling dispacth of the frames
	Bool poc_probe_done;
	//pointer to the first packet of the current frame (the one holding timing info)
	//this packet is in the packet queue
	GF_FilterPacket *first_pck_in_au;
	//frame has slices used as reference
	Bool has_ref_slices;
	//frame has redundant coding
	Bool has_redundant;

	Bool next_nal_end_skip;

	Bool last_frame_is_idr;

	//buffer to store SEI messages
	//for AVC: we have to rewrite the SEI to remove some of the messages according to the spec
	//for HEVC: we store prefix SEI here and dispatch them once the first VCL is found
	char *sei_buffer;
	u32 sei_buffer_size, sei_buffer_alloc;

	//subsample buffer, only used for SVC for now
	u32 subsamp_buffer_alloc, subsamp_buffer_size, subs_mapped_bytes;
	char *subsamp_buffer;

	//AVC specific
	//avc bitstream state
	AVCState *avc_state;

	//SVC specific
	char *svc_prefix_buffer;
	u32 svc_prefix_buffer_size, svc_prefix_buffer_alloc;
	u32 svc_nalu_prefix_reserved;
	u8 svc_nalu_prefix_priority;

	//HEVC specific
	HEVCState *hevc_state;
	//shvc stats
	u32 nb_e_idr, nb_e_i, nb_e_p, nb_e_b;

	LHVCLayerInfo linf[64];
	u8 max_temporal_id[64];
	u8 min_layer_id;

	Bool has_initial_aud;
	char init_aud[3];
} GF_NALUDmxCtx;


GF_Err naludmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_NALUDmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		if (ctx->opid) gf_filter_pid_remove(ctx->opid);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) {
		ctx->timescale = p->value.uint;
		//if we have a FPS prop, use it
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
		if (p) {
			ctx->cur_fps = p->value.frac;
		} else {
			ctx->cur_fps.den = 0;
			ctx->cur_fps.num = ctx->timescale;
		}
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (p) {
		switch (p->value.uint) {
		case GF_CODECID_HEVC:
		case GF_CODECID_LHVC:
			ctx->is_hevc = GF_TRUE;
			break;
		}
	}
	else {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_MIME);
		if (p && p->value.string && (
			strstr(p->value.string, "hvc")
			|| strstr(p->value.string, "hevc")
			|| strstr(p->value.string, "265")
			|| strstr(p->value.string, "shvc")
			|| strstr(p->value.string, "mhvc")
			|| strstr(p->value.string, "lhvc")
		) ) ctx->is_hevc = GF_TRUE;
		else {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_FILE_EXT);
			if (p && p->value.string && (
				 strstr(p->value.string, "hvc")
				 || strstr(p->value.string, "hevc")
				 || strstr(p->value.string, "265")
				 || strstr(p->value.string, "shvc")
				 || strstr(p->value.string, "mhvc")
				 || strstr(p->value.string, "lhvc")
			 ) ) ctx->is_hevc = GF_TRUE;
		}
	}
	if (ctx->is_hevc) {
		ctx->log_name = "HEVC";
		if (ctx->avc_state) gf_free(ctx->avc_state);
		if (!ctx->hevc_state) GF_SAFEALLOC(ctx->hevc_state, HEVCState);
		ctx->min_layer_id = 0xFF;
	} else {
		ctx->log_name = "AVC|H264";
		if (ctx->hevc_state) gf_free(ctx->hevc_state);
		if (!ctx->avc_state) GF_SAFEALLOC(ctx->avc_state, AVCState);
	}
	if (ctx->timescale && !ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		ctx->first_slice_in_au = GF_TRUE;
	}
	ctx->full_au_source = GF_FALSE;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_UNFRAMED_FULL_AU);
	if (p && p->value.boolean)
		ctx->full_au_source = GF_TRUE;

	//copy properties at init or reconfig
	if (ctx->opid) {
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT(GF_STREAM_VISUAL));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(ctx->is_hevc ? GF_CODECID_HEVC : GF_CODECID_AVC));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED_FULL_AU, NULL);
		if (!gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_ID))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ID, &PROP_UINT(1));

		ctx->ps_modified = GF_TRUE;
		ctx->crc_cfg = ctx->crc_cfg_enh = 0;
	}
	
	return GF_OK;
}


static void naludmx_check_dur(GF_Filter *filter, GF_NALUDmxCtx *ctx)
{
	FILE *stream;
	GF_BitStream *bs;
	u64 duration, cur_dur, nal_start, start_code_pos;
	AVCState *avc_state = NULL;
	HEVCState *hevc_state = NULL;
	Bool first_slice_in_pic = GF_TRUE;
	const GF_PropertyValue *p;
	const char *filepath = NULL;
	if (!ctx->opid || ctx->timescale || ctx->file_loaded) return;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string) {
		ctx->is_file = GF_FALSE;
		ctx->file_loaded = GF_TRUE;
		return;
	}
	filepath = p->value.string;
	ctx->is_file = GF_TRUE;

	if (ctx->index<0) {
		p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_DOWN_SIZE);
		if (!p || (p->value.longuint > 100000000)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("[%s] Source file larger than 100M, skipping indexing\n", ctx->log_name));
		} else {
			ctx->index = -ctx->index;
		}
	}
	if (ctx->index<=0) {
		ctx->duration.num = 1;
		ctx->file_loaded = GF_TRUE;
		return;
	}

	stream = gf_fopen(filepath, "rb");
	if (!stream) return;

	ctx->index_size = 0;
	duration = 0;
	cur_dur = 0;

	bs = gf_bs_from_file(stream, GF_BITSTREAM_READ);
	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);

	start_code_pos = gf_bs_get_position(bs);
	if (!gf_media_nalu_is_start_code(bs)) {
		gf_bs_del(bs);
		gf_fclose(stream);
		ctx->duration.num = 1;
		ctx->file_loaded = GF_TRUE;
		return;
	}

	nal_start = gf_bs_get_position(bs);
	if (ctx->is_hevc) {
		GF_SAFEALLOC(hevc_state, HEVCState);
	} else {
		GF_SAFEALLOC(avc_state, AVCState);
	}

	while (gf_bs_available(bs)) {
		u32 nal_size;
		s32 res;
		Bool is_rap = GF_FALSE;
		Bool is_slice = GF_FALSE;
		nal_size = gf_media_nalu_next_start_code_bs(bs);

		gf_bs_seek(bs, nal_start);
		if (hevc_state) {
			u8 temporal_id, layer_id, nal_type;

			res = gf_media_hevc_parse_nalu_bs(bs, hevc_state, &nal_type, &temporal_id, &layer_id);
			if (res>0) first_slice_in_pic = GF_TRUE;
			switch (nal_type) {
			case GF_HEVC_NALU_SLICE_IDR_N_LP:
			case GF_HEVC_NALU_SLICE_IDR_W_DLP:
			case GF_HEVC_NALU_SLICE_CRA:
			case GF_HEVC_NALU_SLICE_BLA_N_LP:
			case GF_HEVC_NALU_SLICE_BLA_W_LP:
			case GF_HEVC_NALU_SLICE_BLA_W_DLP:
				is_rap = GF_TRUE;
				is_slice = GF_TRUE;
				break;
			case GF_HEVC_NALU_SLICE_STSA_N:
			case GF_HEVC_NALU_SLICE_STSA_R:
			case GF_HEVC_NALU_SLICE_RADL_R:
			case GF_HEVC_NALU_SLICE_RASL_R:
			case GF_HEVC_NALU_SLICE_RADL_N:
			case GF_HEVC_NALU_SLICE_RASL_N:
			case GF_HEVC_NALU_SLICE_TRAIL_N:
			case GF_HEVC_NALU_SLICE_TRAIL_R:
			case GF_HEVC_NALU_SLICE_TSA_N:
			case GF_HEVC_NALU_SLICE_TSA_R:
				is_slice = GF_TRUE;
				break;
			}
		} else {
			u32 nal_type;
			u64 pos = gf_bs_get_position(bs);
			res = gf_media_avc_parse_nalu(bs, avc_state);
			if (res>0) first_slice_in_pic = GF_TRUE;

			nal_type = avc_state->last_nal_type_parsed;

			switch (nal_type) {
			case GF_AVC_NALU_SEQ_PARAM:
				gf_bs_seek(bs, pos);
				gf_media_avc_read_sps_bs(bs, avc_state, GF_FALSE, NULL);
				break;
			case GF_AVC_NALU_PIC_PARAM:
				gf_bs_seek(bs, pos);
				gf_media_avc_read_pps_bs(bs, avc_state);
				break;
			case GF_AVC_NALU_IDR_SLICE:
				is_rap = GF_TRUE;
				is_slice = GF_TRUE;
				break;
			case GF_AVC_NALU_NON_IDR_SLICE:
			case GF_AVC_NALU_DP_A_SLICE:
			case GF_AVC_NALU_DP_B_SLICE:
			case GF_AVC_NALU_DP_C_SLICE:
				is_slice = GF_TRUE;
				break;
			}
		}

		if (is_rap && first_slice_in_pic && (cur_dur >= ctx->index * ctx->cur_fps.num) ) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(NALUIdx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = start_code_pos;
			ctx->indexes[ctx->index_size].duration = (Double) duration;
			ctx->indexes[ctx->index_size].duration /= ctx->cur_fps.num;
			ctx->index_size ++;
			cur_dur = 0;
		}

		if (is_slice && first_slice_in_pic) {
			duration += ctx->cur_fps.den;
			cur_dur += ctx->cur_fps.den;
			first_slice_in_pic = GF_FALSE;
		}

		gf_bs_seek(bs, nal_start + nal_size);
/*		nal_start = gf_media_nalu_next_start_code_bs(bs);
		if (nal_start) gf_bs_skip_bytes(bs, nal_start);
*/
		if (gf_bs_available(bs)<4)
			break;

		start_code_pos = gf_bs_get_position(bs);
		nal_start = gf_media_nalu_is_start_code(bs);
		if (!nal_start) {
			break;
		}
		nal_start = gf_bs_get_position(bs);
	}

	gf_bs_del(bs);
	gf_fclose(stream);
	if (hevc_state) gf_free(hevc_state);
	if (avc_state) gf_free(avc_state);

	if (!ctx->duration.num || (ctx->duration.num  * ctx->cur_fps.num != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = ctx->cur_fps.num;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );
}


static void naludmx_enqueue_or_dispatch(GF_NALUDmxCtx *ctx, GF_FilterPacket *n_pck, Bool flush_ref)
{
	//TODO: we are dispacthing frames in "negctts mode", ie we may have DTS>CTS
	//need to signal this for consumers using DTS (eg MPEG-2 TS)
	if (flush_ref && ctx->pck_queue && ctx->poc_diff) {
		//send all reference packet queued

		while (gf_list_count(ctx->pck_queue) ) {
			u64 dts;
			GF_FilterPacket *q_pck = gf_list_get(ctx->pck_queue, 0);

			if (q_pck == ctx->first_pck_in_au) break;

			dts = gf_filter_pck_get_dts(q_pck);
			if (dts != GF_FILTER_NO_TS) {
				s32 poc;
				u64 poc_ts, cts;
				u8 carousel_info = gf_filter_pck_get_carousel_version(q_pck);

				//we reused timing from source packets
				if (!carousel_info) {
					assert(ctx->timescale);
					gf_list_rem(ctx->pck_queue, 0);
					gf_filter_pck_send(q_pck);
					continue;
				}
				gf_filter_pck_set_carousel_version(q_pck, 0);

				poc_ts = gf_filter_pck_get_cts(q_pck);
				assert(poc_ts != GF_FILTER_NO_TS);
				poc = (s32) ((s64) poc_ts - CTS_POC_OFFSET_SAFETY);
				//poc is stored as diff since last IDR which has min_poc
				cts = ( (ctx->min_poc + (s32) poc) * ctx->cur_fps.den ) / ctx->poc_diff + ctx->dts_last_IDR;

				/*old importer code, seems wrong so commented for now*/
#if 0
				/*if PAFF, 2 pictures (eg poc) <=> 1 aggregated frame (eg sample), divide by 2*/
				if (ctx->is_paff) {
					cts /= 2;
					/*in some cases the poc is not on the top field - if that is the case, round up*/
					if (cts % ctx->cur_fps.den) {
						cts = ((cts/ctx->cur_fps.den)+1) * ctx->cur_fps.den;
					}
				}
#endif
				gf_filter_pck_set_cts(q_pck, cts);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[%s] Frame timestamps computed dts "LLU" cts "LLU" (poc %d min poc %d poc_diff %d last IDR DTS "LLU")\n", ctx->log_name, dts, cts, poc, ctx->min_poc, ctx->poc_diff, ctx->dts_last_IDR));

				if (ctx->importer) {
					poc = (s32) ( (s64) cts - (s64) dts);
					if (poc<0) poc = -poc;
					poc /= ctx->cur_fps.den;
					if (poc > ctx->max_total_delay)
						ctx->max_total_delay = poc;
				}
			}
			gf_list_rem(ctx->pck_queue, 0);
			gf_filter_pck_send(q_pck);
		}
	}
	if (!n_pck) return;

	if (!ctx->pck_queue) ctx->pck_queue = gf_list_new();
	gf_list_add(ctx->pck_queue, n_pck);
}

static void naludmx_hevc_add_param(GF_HEVCConfig *cfg, GF_AVCConfigSlot *sl, u8 nal_type)
{
	GF_HEVCParamArray *pa = NULL;
	u32 i, count;
	count = gf_list_count(cfg->param_array);
	for (i=0; i<count; i++) {
		pa = gf_list_get(cfg->param_array, i);
		if (pa->type == nal_type) break;
		pa = NULL;
	}
	if (!pa) {
		GF_SAFEALLOC(pa, GF_HEVCParamArray);
		pa->array_completeness = 1;
		pa->type = nal_type;
		pa->nalus = gf_list_new();
		gf_list_add(cfg->param_array, pa);
	}
	gf_list_add(pa->nalus, sl);
}

static void naludmx_hevc_set_parall_type(GF_NALUDmxCtx *ctx, GF_HEVCConfig *hevc_cfg)
{
	u32 use_tiles, use_wpp, nb_pps, i, count;
	HEVCState hevc;

	count = gf_list_count(ctx->pps);

	memset(&hevc, 0, sizeof(HEVCState));
	hevc.sps_active_idx = -1;

	use_tiles = 0;
	use_wpp = 0;
	nb_pps = 0;

	for (i=0; i<count; i++) {
		HEVC_PPS *pps;
		GF_AVCConfigSlot *slc = (GF_AVCConfigSlot*)gf_list_get(ctx->pps, i);
		s32 idx = gf_media_hevc_read_pps(slc->data, slc->size, &hevc);

		if (idx>=0) {
			nb_pps++;
			pps = &hevc.pps[idx];
			if (!pps->entropy_coding_sync_enabled_flag && pps->tiles_enabled_flag)
				use_tiles++;
			else if (pps->entropy_coding_sync_enabled_flag && !pps->tiles_enabled_flag)
				use_wpp++;
		}
	}
	if (!use_tiles && !use_wpp) hevc_cfg->parallelismType = 1;
	else if (!use_wpp && (use_tiles==nb_pps) ) hevc_cfg->parallelismType = 2;
	else if (!use_tiles && (use_wpp==nb_pps) ) hevc_cfg->parallelismType = 3;
	else hevc_cfg->parallelismType = 0;
}

GF_Err naludmx_set_hevc_oinf(GF_NALUDmxCtx *ctx, u8 *max_temporal_id)
{
	GF_OperatingPointsInformation *oinf;
	GF_BitStream *bs;
	u8 *data;
	u32 data_size;
	u32 i;
	HEVC_VPS *vps;
	GF_AVCConfigSlot *vps_sl = gf_list_get(ctx->vps, 0);
	if (!vps_sl) return GF_SERVICE_ERROR;

	vps = &ctx->hevc_state->vps[vps_sl->id];
	
	if (!vps->vps_extension_found) return GF_OK;
	if (vps->max_layers<2) return GF_OK;

	oinf = gf_isom_oinf_new_entry();
	if (!oinf) return GF_OUT_OF_MEM;

	oinf->scalability_mask = 0;
	for (i = 0; i < 16; i++) {
		if (vps->scalability_mask[i])
			oinf->scalability_mask |= 1 << i;
	}

	for (i = 0; i < vps->num_profile_tier_level; i++) {
		HEVC_ProfileTierLevel ptl = (i == 0) ? vps->ptl : vps->ext_ptl[i-1];
		LHEVC_ProfileTierLevel *lhevc_ptl;
		GF_SAFEALLOC(lhevc_ptl, LHEVC_ProfileTierLevel);
		lhevc_ptl->general_profile_space = ptl.profile_space;
		lhevc_ptl->general_tier_flag = ptl.tier_flag;
		lhevc_ptl->general_profile_idc = ptl.profile_idc;
		lhevc_ptl->general_profile_compatibility_flags = ptl.profile_compatibility_flag;
		lhevc_ptl->general_constraint_indicator_flags = 0;
		if (ptl.general_progressive_source_flag)
			lhevc_ptl->general_constraint_indicator_flags |= ((u64)1) << 47;
		if (ptl.general_interlaced_source_flag)
			lhevc_ptl->general_constraint_indicator_flags |= ((u64)1) << 46;
		if (ptl.general_non_packed_constraint_flag)
			lhevc_ptl->general_constraint_indicator_flags |= ((u64)1) << 45;
		if (ptl.general_frame_only_constraint_flag)
			lhevc_ptl->general_constraint_indicator_flags |= ((u64)1) << 44;
		lhevc_ptl->general_constraint_indicator_flags |= ptl.general_reserved_44bits;
		lhevc_ptl->general_level_idc = ptl.level_idc;
		gf_list_add(oinf->profile_tier_levels, lhevc_ptl);
	}

	for (i = 0; i < vps->num_output_layer_sets; i++) {
		LHEVC_OperatingPoint *op;
		u32 j;
		u16 minPicWidth, minPicHeight, maxPicWidth, maxPicHeight;
		u8 maxChromaFormat, maxBitDepth;
		u8 maxTemporalId;
		GF_SAFEALLOC(op, LHEVC_OperatingPoint);
		op->output_layer_set_idx = i;
		op->layer_count = vps->num_necessary_layers[i];
		minPicWidth = minPicHeight = maxPicWidth = maxPicHeight = maxTemporalId = 0;
		maxChromaFormat = maxBitDepth = 0;
		for (j = 0; j < op->layer_count; j++) {
			u32 format_idx;
			u32 bitDepth;
			op->layers_info[j].ptl_idx = vps->profile_tier_level_idx[i][j];
			op->layers_info[j].layer_id = j;
			op->layers_info[j].is_outputlayer = vps->output_layer_flag[i][j];
			//FIXME: we consider that this flag is never set
			op->layers_info[j].is_alternate_outputlayer = GF_FALSE;

			if (max_temporal_id) {
				if (!maxTemporalId || (maxTemporalId < max_temporal_id[op->layers_info[j].layer_id]))
					maxTemporalId = max_temporal_id[op->layers_info[j].layer_id];
			} else {
				maxTemporalId = vps->max_sub_layers;
			}

			format_idx = vps->rep_format_idx[op->layers_info[j].layer_id];
			if (!minPicWidth || (minPicWidth > vps->rep_formats[format_idx].pic_width_luma_samples))
				minPicWidth = vps->rep_formats[format_idx].pic_width_luma_samples;
			if (!minPicHeight || (minPicHeight > vps->rep_formats[format_idx].pic_height_luma_samples))
				minPicHeight = vps->rep_formats[format_idx].pic_height_luma_samples;
			if (!maxPicWidth || (maxPicWidth < vps->rep_formats[format_idx].pic_width_luma_samples))
				maxPicWidth = vps->rep_formats[format_idx].pic_width_luma_samples;
			if (!maxPicHeight || (maxPicHeight < vps->rep_formats[format_idx].pic_height_luma_samples))
				maxPicHeight = vps->rep_formats[format_idx].pic_height_luma_samples;
			if (!maxChromaFormat || (maxChromaFormat < vps->rep_formats[format_idx].chroma_format_idc))
				maxChromaFormat = vps->rep_formats[format_idx].chroma_format_idc;
			bitDepth = vps->rep_formats[format_idx].bit_depth_chroma > vps->rep_formats[format_idx].bit_depth_luma ? vps->rep_formats[format_idx].bit_depth_chroma : vps->rep_formats[format_idx].bit_depth_luma;
			if (!maxChromaFormat || (maxChromaFormat < bitDepth))
				maxChromaFormat = bitDepth;
		}
		op->max_temporal_id = maxTemporalId;
		op->minPicWidth = minPicWidth;
		op->minPicHeight = minPicHeight;
		op->maxPicWidth = maxPicWidth;
		op->maxPicHeight = maxPicHeight;
		op->maxChromaFormat = maxChromaFormat;
		op->maxBitDepth = maxBitDepth;
		op->frame_rate_info_flag = GF_FALSE; //FIXME: should fetch this info from VUI
		op->bit_rate_info_flag = GF_FALSE; //we don't use it
		gf_list_add(oinf->operating_points, op);
	}

	for (i = 0; i < vps->max_layers; i++) {
		LHEVC_DependentLayer *dep;
		u32 j, k;
		GF_SAFEALLOC(dep, LHEVC_DependentLayer);
		dep->dependent_layerID = vps->layer_id_in_nuh[i];
		for (j = 0; j < vps->max_layers; j++) {
			if (vps->direct_dependency_flag[dep->dependent_layerID][j]) {
				dep->dependent_on_layerID[dep->num_layers_dependent_on] = j;
				dep->num_layers_dependent_on ++;
			}
		}
		k = 0;
		for (j = 0; j < 16; j++) {
			if (oinf->scalability_mask & (1 << j)) {
				dep->dimension_identifier[j] = vps->dimension_id[i][k];
				k++;
			}
		}
		gf_list_add(oinf->dependency_layers, dep);
	}

	//write Operating Points Information Sample Group
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_isom_oinf_write_entry(oinf, bs);
	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);
	gf_isom_oinf_del_entry(oinf);

	gf_filter_pid_set_info_str(ctx->opid, "hevc:oinf", &PROP_DATA_NO_COPY(data, data_size) );
	return GF_OK;
}

static void naludmx_set_hevc_linf(GF_NALUDmxCtx *ctx)
{
	u32 i, nb_layers=0, nb_sublayers=0;
	u8 *data;
	u32 data_size;
	GF_BitStream *bs;

	for (i=0; i<64; i++) {
		if (ctx->linf[i].layer_id_plus_one) nb_layers++;
		if (ctx->linf[i].min_temporal_id != ctx->linf[i].max_temporal_id) nb_sublayers++;
	}
	if (!nb_layers && !nb_sublayers)
		return;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	
	gf_bs_write_int(bs, 0, 2);
	gf_bs_write_int(bs, nb_layers, 6);
	for (i=0; i<nb_layers; i++) {
		if (! ctx->linf[i].layer_id_plus_one) continue;
		gf_bs_write_int(bs, 0, 4);
		gf_bs_write_int(bs, ctx->linf[i].layer_id_plus_one - 1, 6);
		gf_bs_write_int(bs, ctx->linf[i].min_temporal_id, 3);
		gf_bs_write_int(bs, ctx->linf[i].max_temporal_id, 3);
		gf_bs_write_int(bs, 0, 1);
		gf_bs_write_int(bs, 0xFF, 7);

	}
	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);
	gf_filter_pid_set_info_str(ctx->opid, "hevc:linf", &PROP_DATA_NO_COPY(data, data_size) );
}

static void naludmx_create_hevc_decoder_config(GF_NALUDmxCtx *ctx, u8 **dsi, u32 *dsi_size, u8 **dsi_enh, u32 *dsi_enh_size, u32 *max_width, u32 *max_height, u32 *max_enh_width, u32 *max_enh_height, GF_Fraction *sar, Bool *has_hevc_base)
{
	u32 i, count;
	u8 layer_id;
	Bool first = GF_TRUE;
	Bool first_lhvc = GF_TRUE;
	GF_HEVCConfig *cfg;
	GF_HEVCConfig *hvcc;
	GF_HEVCConfig *lvcc;
	u32 max_w, max_h, max_ew, max_eh;

	*has_hevc_base = GF_FALSE;


	max_w = max_h = 0;
	max_ew = max_eh = 0;
	sar->num = sar->den = 1;

	hvcc = gf_odf_hevc_cfg_new();
	lvcc = gf_odf_hevc_cfg_new();
	hvcc->nal_unit_size = ctx->nal_length;
	lvcc->nal_unit_size = ctx->nal_length;
	lvcc->is_lhvc = GF_TRUE;
	//check we have one pps or sps in base layer
	count = gf_list_count(ctx->sps);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = gf_list_get(ctx->sps, i);
		layer_id = ((sl->data[0] & 0x1) << 5) | (sl->data[1] >> 3);
		if (!layer_id) {
			*has_hevc_base = GF_TRUE;
			break;
		}
	}
	count = gf_list_count(ctx->pps);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = gf_list_get(ctx->pps, i);
		layer_id = ((sl->data[0] & 0x1) << 5) | (sl->data[1] >> 3);
		if (!layer_id) {
			*has_hevc_base = GF_TRUE;
			break;
		}
	}
	//assign vps first so that they are serialized first
	count = gf_list_count(ctx->vps);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = gf_list_get(ctx->vps, i);
		HEVC_VPS *vps = &ctx->hevc_state->vps[sl->id];

		if (!i) {
			hvcc->avgFrameRate = lvcc->avgFrameRate = vps->rates[0].avg_pic_rate;
			hvcc->constantFrameRate = lvcc->constantFrameRate = vps->rates[0].constand_pic_rate_idc;
			hvcc->numTemporalLayers = lvcc->numTemporalLayers = vps->max_sub_layers;
			hvcc->temporalIdNested = lvcc->temporalIdNested = vps->temporal_id_nesting;
		}
		//TODO set scalability mask
		if (!ctx->analyze)
			naludmx_hevc_add_param((ctx->explicit || ! (*has_hevc_base) ) ? lvcc : hvcc, sl, GF_HEVC_NALU_VID_PARAM);
	}

	count = gf_list_count(ctx->sps);
	for (i=0; i<count; i++) {
		Bool is_lhvc = GF_FALSE;
		GF_AVCConfigSlot *sl = gf_list_get(ctx->sps, i);
		HEVC_SPS *sps = &ctx->hevc_state->sps[sl->id];
		layer_id = ((sl->data[0] & 0x1) << 5) | (sl->data[1] >> 3);
		if (!layer_id) *has_hevc_base = GF_TRUE;

		if (ctx->explicit || layer_id) {
			cfg = lvcc;
			is_lhvc = GF_TRUE;
		} else {
			cfg = hvcc;
		}

		if (first || (is_lhvc && first_lhvc) ) {
			cfg->configurationVersion = 1;
			cfg->profile_space = sps->ptl.profile_space;
			cfg->tier_flag = sps->ptl.tier_flag;
			cfg->profile_idc = sps->ptl.profile_idc;
			cfg->general_profile_compatibility_flags = sps->ptl.profile_compatibility_flag;
			cfg->progressive_source_flag = sps->ptl.general_progressive_source_flag;
			cfg->interlaced_source_flag = sps->ptl.general_interlaced_source_flag;
			cfg->non_packed_constraint_flag = sps->ptl.general_non_packed_constraint_flag;
			cfg->frame_only_constraint_flag = sps->ptl.general_frame_only_constraint_flag;
			cfg->constraint_indicator_flags = sps->ptl.general_reserved_44bits;
			cfg->level_idc = sps->ptl.level_idc;
			cfg->chromaFormat = sps->chroma_format_idc;
			cfg->luma_bit_depth = sps->bit_depth_luma;
			cfg->chroma_bit_depth = sps->bit_depth_chroma;

			if (sps->aspect_ratio_info_present_flag && sps->sar_width && sps->sar_width) {
				sar->num = sps->sar_width;
				sar->den = sps->sar_height;
			}

			/*disable frame rate scan, most bitstreams have wrong values there*/
			if (!ctx->timescale && first && !ctx->fps.num*ctx->fps.den && sps->has_timing_info
				/*if detected FPS is greater than 1000, assume wrong timing info*/
				&& (sps->time_scale <= 1000*sps->num_units_in_tick)
			) {
				ctx->cur_fps.num = sps->time_scale;
				ctx->cur_fps.den = sps->num_units_in_tick;

				if (!ctx->fps.num && ctx->dts==ctx->fps.den)
					ctx->dts = ctx->cur_fps.den;
			}
			ctx->fps = ctx->cur_fps;
		}
		first = GF_FALSE;
		if (is_lhvc) {
			first_lhvc = GF_FALSE;
			if (sps->width > max_ew) max_ew = sps->width;
			if (sps->height > max_eh) max_eh = sps->height;
		} else {
			if (sps->width > max_w) max_w = sps->width;
			if (sps->height > max_h) max_h = sps->height;
		}
		if (!ctx->analyze)
			naludmx_hevc_add_param(cfg, sl, GF_HEVC_NALU_SEQ_PARAM);
	}

	cfg = ctx->explicit ? lvcc : hvcc;
	count = gf_list_count(ctx->pps);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = gf_list_get(ctx->pps, i);
		layer_id = ((sl->data[0] & 0x1) << 5) | (sl->data[1] >> 3);
		if (!layer_id) *has_hevc_base = GF_TRUE;
		if (!ctx->analyze)
			naludmx_hevc_add_param(layer_id ? lvcc : cfg, sl, GF_HEVC_NALU_PIC_PARAM);
	}

	*dsi = *dsi_enh = NULL;
	*dsi_size = *dsi_enh_size = 0;

	if (ctx->explicit || ! (*has_hevc_base) ) {
		naludmx_hevc_set_parall_type(ctx, lvcc);
		gf_odf_hevc_cfg_write(lvcc, dsi, dsi_size);
		*max_width = *max_enh_width = max_ew;
		*max_height = *max_enh_height = max_eh;
	} else {
		naludmx_hevc_set_parall_type(ctx, hvcc);
		gf_odf_hevc_cfg_write(hvcc, dsi, dsi_size);
		if (gf_list_count(lvcc->param_array) ) {
			naludmx_hevc_set_parall_type(ctx, lvcc);
			gf_odf_hevc_cfg_write(lvcc, dsi_enh, dsi_enh_size);
		}
		*max_width = max_w;
		*max_height = max_h;
		*max_enh_width = max_ew;
		*max_enh_height = max_eh;
	}
	count = gf_list_count(hvcc->param_array);
	for (i=0; i<count; i++) {
		GF_HEVCParamArray *pa = gf_list_get(hvcc->param_array, i);
		gf_list_reset(pa->nalus);
	}
	count = gf_list_count(lvcc->param_array);
	for (i=0; i<count; i++) {
		GF_HEVCParamArray *pa = gf_list_get(lvcc->param_array, i);
		gf_list_reset(pa->nalus);
	}
	gf_odf_hevc_cfg_del(hvcc);
	gf_odf_hevc_cfg_del(lvcc);
}

void naludmx_create_avc_decoder_config(GF_NALUDmxCtx *ctx, u8 **dsi, u32 *dsi_size, u8 **dsi_enh, u32 *dsi_enh_size, u32 *max_width, u32 *max_height, u32 *max_enh_width, u32 *max_enh_height, GF_Fraction *sar)
{
	u32 i, count;
	Bool first = GF_TRUE;
	Bool first_svc = GF_TRUE;
	GF_AVCConfig *cfg;
	GF_AVCConfig *avcc;
	GF_AVCConfig *svcc;
	u32 max_w, max_h, max_ew, max_eh;


	max_w = max_h = max_ew = max_eh = 0;
	sar->num = sar->den = 1;

	avcc = gf_odf_avc_cfg_new();
	svcc = gf_odf_avc_cfg_new();
	avcc->nal_unit_size = ctx->nal_length;
	svcc->nal_unit_size = ctx->nal_length;

	count = gf_list_count(ctx->sps);
	for (i=0; i<count; i++) {
		Bool is_svc = GF_FALSE;
		GF_AVCConfigSlot *sl = gf_list_get(ctx->sps, i);
		AVC_SPS *sps = &ctx->avc_state->sps[sl->id];
		u32 nal_type = sl->data[0] & 0x1F;

		if (ctx->explicit) {
			cfg = svcc;
		} else if (nal_type == GF_AVC_NALU_SVC_SUBSEQ_PARAM) {
			cfg = svcc;
			is_svc = GF_TRUE;
		} else {
			cfg = avcc;
		}

		if (first || (is_svc && first_svc) ) {
			cfg->configurationVersion = 1;
			cfg->profile_compatibility = sps->prof_compat;
			cfg->AVCProfileIndication = sps->profile_idc;
			cfg->AVCLevelIndication = sps->level_idc;
			cfg->chroma_format = sps->chroma_format;
			cfg->luma_bit_depth = 8 + sps->luma_bit_depth_m8;
			cfg->chroma_bit_depth = 8 + sps->chroma_bit_depth_m8;
			/*try to patch ?*/
			if (!gf_avc_is_rext_profile(cfg->AVCProfileIndication)
				&& ((cfg->chroma_format>1) || (cfg->luma_bit_depth>8) || (cfg->chroma_bit_depth>8))
			) {
				if ((cfg->luma_bit_depth>8) || (cfg->chroma_bit_depth>8)) {
					cfg->AVCProfileIndication = 110;
				} else {
					cfg->AVCProfileIndication = (cfg->chroma_format==3) ? 244 : 122;
				}
			}
			if (sps->vui_parameters_present_flag && sps->vui.par_num && sps->vui.par_den) {
				sar->num = sps->vui.par_num;
				sar->den = sps->vui.par_den;
			}

			/*disable frame rate scan, most bitstreams have wrong values there*/
			if (first && !ctx->fps.num*ctx->fps.den && sps->vui.timing_info_present_flag
				/*if detected FPS is greater than 1000, assume wrong timing info*/
				&& (sps->vui.time_scale <= 1000*sps->vui.num_units_in_tick)
			) {
				/*ISO/IEC 14496-10 n11084 Table E-6*/
				/* not used :				u8 DeltaTfiDivisorTable[] = {1,1,1,2,2,2,2,3,3,4,6}; */
				u8 DeltaTfiDivisorIdx;
				if (!sps->vui.pic_struct_present_flag) {
					DeltaTfiDivisorIdx = 1 + (1 - ctx->avc_state->s_info.field_pic_flag);
				} else {
					if (!ctx->avc_state->sei.pic_timing.pic_struct)
						DeltaTfiDivisorIdx = 2;
					else if (ctx->avc_state->sei.pic_timing.pic_struct == 8)
						DeltaTfiDivisorIdx = 6;
					else
						DeltaTfiDivisorIdx = (ctx->avc_state->sei.pic_timing.pic_struct+1) / 2;
				}
				if (!ctx->timescale) {
					ctx->cur_fps.num = 2 * sps->vui.time_scale;
					ctx->cur_fps.den =  2 * sps->vui.num_units_in_tick * DeltaTfiDivisorIdx;

					if (!ctx->fps.num && ctx->dts==ctx->fps.den)
						ctx->dts = ctx->cur_fps.den;
				}
				if (! sps->vui.fixed_frame_rate_flag)
					GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("[%s] Possible Variable Frame Rate: VUI \"fixed_frame_rate_flag\" absent\n", ctx->log_name));
			}
			ctx->fps = ctx->cur_fps;
		}
		first = GF_FALSE;
		if (is_svc) {
			first_svc = GF_FALSE;
			if (sps->width > max_ew) max_ew = sps->width;
			if (sps->height > max_eh) max_eh = sps->height;
		} else {
			if (sps->width > max_w) max_w = sps->width;
			if (sps->height > max_h) max_h = sps->height;
		}
		if (!ctx->analyze)
			gf_list_add(cfg->sequenceParameterSets, sl);
	}

	cfg = ctx->explicit ? svcc : avcc;
	count = gf_list_count(ctx->sps_ext);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = gf_list_get(ctx->sps_ext, i);
		if (!cfg->sequenceParameterSetExtensions) cfg->sequenceParameterSetExtensions = gf_list_new();
		if (!ctx->analyze)
			gf_list_add(cfg->sequenceParameterSetExtensions, sl);
	}

	cfg = ctx->explicit ? svcc : avcc;
	count = gf_list_count(ctx->pps);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = gf_list_get(ctx->pps, i);
		if (!ctx->analyze)
			gf_list_add(cfg->pictureParameterSets, sl);
	}

	cfg = svcc;
	count = gf_list_count(ctx->pps_svc);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = gf_list_get(ctx->pps_svc, i);
		if (!ctx->analyze)
			gf_list_add(cfg->pictureParameterSets, sl);
	}

	*dsi = *dsi_enh = NULL;
	*dsi_size = *dsi_enh_size = 0;

	if (ctx->explicit) {
		gf_odf_avc_cfg_write(svcc, dsi, dsi_size);
	} else {
		gf_odf_avc_cfg_write(avcc, dsi, dsi_size);
		if (gf_list_count(svcc->sequenceParameterSets) || svcc->sequenceParameterSetExtensions) {
			gf_odf_avc_cfg_write(svcc, dsi_enh, dsi_enh_size);
		}
	}
	gf_list_reset(avcc->sequenceParameterSets);
	gf_list_reset(avcc->sequenceParameterSetExtensions);
	gf_list_reset(avcc->pictureParameterSets);
	gf_list_reset(svcc->sequenceParameterSets);
	gf_list_reset(svcc->sequenceParameterSetExtensions);
	gf_list_reset(svcc->pictureParameterSets);
	gf_odf_avc_cfg_del(avcc);
	gf_odf_avc_cfg_del(svcc);
	*max_width = max_w;
	*max_height = max_h;
	*max_enh_width = max_ew;
	*max_enh_height = max_eh;
}

static void naludmx_check_pid(GF_Filter *filter, GF_NALUDmxCtx *ctx)
{
	u32 w, h, ew, eh;
	u8 *dsi, *dsi_enh;
	u32 dsi_size, dsi_enh_size;
	u32 crc_cfg, crc_cfg_enh;
	GF_Fraction sar;
	Bool has_hevc_base = GF_TRUE;
	Bool has_colr_info = GF_FALSE;

	if (ctx->analyze) {
		if (ctx->opid && !ctx->ps_modified) return;
	} else {
		if (!ctx->ps_modified) return;
	}
	ctx->ps_modified = GF_FALSE;

	dsi = dsi_enh = NULL;

	if (!ctx->timescale) {
		ctx->cur_fps = ctx->fps;
		if (!ctx->cur_fps.num*ctx->cur_fps.den) {
			ctx->cur_fps.num = 25000;
			ctx->cur_fps.den = 1000;
		}
	}

	if (ctx->is_hevc) {
		naludmx_create_hevc_decoder_config(ctx, &dsi, &dsi_size, &dsi_enh, &dsi_enh_size, &w, &h, &ew, &eh, &sar, &has_hevc_base);
	} else {
		naludmx_create_avc_decoder_config(ctx, &dsi, &dsi_size, &dsi_enh, &dsi_enh_size, &w, &h, &ew, &eh, &sar);
	}
	crc_cfg = crc_cfg_enh = 0;
	if (dsi) crc_cfg = gf_crc_32(dsi, dsi_size);
	if (dsi_enh) crc_cfg_enh = gf_crc_32(dsi_enh, dsi_enh_size);

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);

		naludmx_check_dur(filter, ctx);
		ctx->first_slice_in_au = GF_TRUE;
	}

	if ((ctx->crc_cfg == crc_cfg) && (ctx->crc_cfg_enh == crc_cfg_enh)
		&& (ctx->width==w) && (ctx->height==h)
		&& (ctx->sar.num * sar.den == ctx->sar.den * sar.num)
	) {
		if (dsi) gf_free(dsi);
		if (dsi_enh) gf_free(dsi_enh);
		return;
	}

	naludmx_enqueue_or_dispatch(ctx, NULL, GF_TRUE);
	if (!ctx->analyze && gf_list_count(ctx->pck_queue)) {
		GF_LOG(dsi_enh ? GF_LOG_DEBUG : GF_LOG_ERROR, GF_LOG_PARSER, ("[%s] xPS changed but could not flush frames before signaling state change %s\n", ctx->log_name, dsi_enh ? "- likely scalable xPS update" : "!"));
	}

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT(GF_STREAM_VISUAL));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	if (!gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_ID))
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ID, &PROP_UINT(1));

	ctx->width = w;
	ctx->height = h;
	ctx->sar = sar;
	ctx->crc_cfg = crc_cfg;
	ctx->crc_cfg_enh = crc_cfg_enh;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, & PROP_UINT( ctx->width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, & PROP_UINT( ctx->height));
	if (ew && eh) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH_MAX, & PROP_UINT( ew ));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT_MAX, & PROP_UINT( eh ));
	}
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, & PROP_FRAC(ctx->sar));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FPS, & PROP_FRAC(ctx->cur_fps));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->timescale ? ctx->timescale : ctx->cur_fps.num));

	if (ctx->explicit || !has_hevc_base) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(ctx->is_hevc ? GF_CODECID_LHVC : GF_CODECID_SVC));
		if (dsi) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );
	} else {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(ctx->is_hevc ? GF_CODECID_HEVC : GF_CODECID_AVC));
		if (dsi) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );
		if (dsi_enh) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, &PROP_DATA_NO_COPY(dsi_enh, dsi_enh_size) );
	}

	if (ctx->is_hevc && gf_list_count(ctx->vps) ) {
		GF_Err e = naludmx_set_hevc_oinf(ctx, NULL);
		if (e) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[%s] Failed to create OINF chunk\n", ctx->log_name));
		}
		naludmx_set_hevc_linf(ctx);
	}
	if (ctx->duration.num)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));

	if (ctx->is_file /* && ctx->index*/) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, & PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD) );
	}

	if (ctx->is_hevc) {
		HEVC_SPS *sps = &ctx->hevc_state->sps[ctx->hevc_state->sps_active_idx];
		if (sps->colour_description_present_flag) {
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_PRIMARIES, & PROP_UINT(sps->colour_primaries) );
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_TRANSFER, & PROP_UINT(sps->transfer_characteristic) );
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_MX, & PROP_UINT(sps->matrix_coeffs) );
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_RANGE, & PROP_BOOL(sps->video_full_range_flag) );
			has_colr_info = GF_TRUE;
		}
	} else {
		/*use the last active SPS*/
		if (ctx->avc_state->sps[ctx->avc_state->sps_active_idx].vui_parameters_present_flag
		&& ctx->avc_state->sps[ctx->avc_state->sps_active_idx].vui.colour_description_present_flag) {
			AVC_VUI *vui = &ctx->avc_state->sps[ctx->avc_state->sps_active_idx].vui;

			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_PRIMARIES, & PROP_UINT(vui->colour_primaries) );
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_TRANSFER, & PROP_UINT(vui->transfer_characteristics) );
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_MX, & PROP_UINT(vui->matrix_coefficients) );
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_RANGE, & PROP_BOOL(vui->video_full_range_flag) );
			has_colr_info = GF_TRUE;
		}
	}

	if (!has_colr_info) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_PRIMARIES, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_TRANSFER, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_MX, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_RANGE, NULL);
	}
}

static Bool naludmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	u64 file_pos = 0;
	GF_FilterEvent fevt;
	GF_NALUDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
			ctx->cts = ctx->dts = 0;
			ctx->bytes_in_header = 0;
		}
		if (! ctx->is_file) {
			if (!ctx->initial_play_done) {
				ctx->initial_play_done = GF_TRUE;
				if (evt->play.start_range<0.1)
					return GF_FALSE;
			}
			ctx->resume_from = 0;
			return GF_FALSE;
		}
		if (ctx->start_range && (ctx->index<0)) {
			ctx->index = -ctx->index;
			ctx->file_loaded = GF_FALSE;
			ctx->duration.den = ctx->duration.num = 0;
			GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("[%s] Play request from %d, building index\n", ctx->log_name, ctx->start_range));
			naludmx_check_dur(filter, ctx);
		}
		ctx->start_range = evt->play.start_range;
		ctx->in_seek = GF_TRUE;

		if (ctx->start_range) {
			for (i=1; i<ctx->index_size; i++) {
				if (ctx->indexes[i].duration>ctx->start_range) {
					ctx->cts = ctx->dts = (u64) (ctx->indexes[i-1].duration * ctx->cur_fps.num);
					file_pos = ctx->indexes[i-1].pos;
					break;
				}
			}
		}
		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (!file_pos)
				return GF_TRUE;
		}
		ctx->resume_from = 0;
		
		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = file_pos;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		//don't cancel event
		ctx->is_playing = GF_FALSE;
		return GF_FALSE;

	case GF_FEVT_SET_SPEED:
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

static GFINLINE void naludmx_update_time(GF_NALUDmxCtx *ctx)
{
	assert(ctx->cur_fps.num);

	if (ctx->timescale) {
		//very first frame, no dts diff, assume 3000/90k. It should only hurt if we have several frames packet in the first packet sent
		u64 dts_inc = ctx->cur_fps.den ? ctx->cur_fps.den : 3000;
		ctx->cts += dts_inc;
		ctx->dts += dts_inc;
	} else {
		assert(ctx->cur_fps.den);
		ctx->cts += ctx->cur_fps.den;
		ctx->dts += ctx->cur_fps.den;
	}
}

static void naludmx_queue_param_set(GF_NALUDmxCtx *ctx, char *data, u32 size, u32 ps_type, s32 ps_id)
{
	GF_List *list = NULL, *alt_list = NULL;
	GF_AVCConfigSlot *sl;
	u32 i, count;
	u32 crc = gf_crc_32(data, size);

	if (ctx->is_hevc) {
		switch (ps_type) {
		case GF_HEVC_NALU_VID_PARAM:
			if (!ctx->vps) ctx->vps = gf_list_new();
			list = ctx->vps;
			break;
		case GF_HEVC_NALU_SEQ_PARAM:
			list = ctx->sps;
			break;
		case GF_HEVC_NALU_PIC_PARAM:
			list = ctx->pps;
			break;
		default:
			assert(0);
			return;
		}
	} else {
		switch (ps_type) {
		case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
		case GF_AVC_NALU_SEQ_PARAM:
			list = ctx->sps;
			break;
		case GF_AVC_NALU_PIC_PARAM:
			list = ctx->pps;
			alt_list = ctx->pps_svc;
			break;
		case GF_AVC_NALU_SEQ_PARAM_EXT:
			if (!ctx->sps_ext) ctx->sps_ext = gf_list_new();
			list = ctx->sps_ext;
			break;
		default:
			assert(0);
			return;
		}
	}
	sl = NULL;
	count = gf_list_count(list);
	for (i=0; i<count; i++) {
		sl = gf_list_get(list, i);
		if (sl->id != ps_id) {
			sl = NULL;
			continue;
		}
		//same ID, same CRC, we don't change our state
		if (sl->crc == crc) return;
		break;
	}
	//handle alt PPS list for SVC
	if (!sl && alt_list) {
		count = gf_list_count(alt_list);
		for (i=0; i<count; i++) {
			sl = gf_list_get(alt_list, i);
			if (sl->id != ps_id) {
				sl = NULL;
				continue;
			}
			//same ID, same CRC, we don't change our state
			if (sl->crc == crc) return;
			break;
		}
	}

	if (sl) {
		//otherwise we keep this new param set
		sl->data = gf_realloc(sl->data, size);
		memcpy(sl->data, data, size);
		sl->size = size;
		sl->crc = crc;
		ctx->ps_modified = GF_TRUE;
		return;
	}
	//TODO we might want to purge the list after a while !!

	GF_SAFEALLOC(sl, GF_AVCConfigSlot);
	sl->data = gf_malloc(sizeof(char) * size);
	memcpy(sl->data, data, size);
	sl->size = size;
	sl->id = ps_id;
	sl->crc = crc;

	ctx->ps_modified = GF_TRUE;
	gf_list_add(list, sl);
}

void naludmx_finalize_au_flags(GF_NALUDmxCtx *ctx)
{
	u64 ts;
	Bool is_rap = GF_FALSE;

	if (!ctx->first_pck_in_au)
		return;
	if (ctx->au_sap) {
		gf_filter_pck_set_sap(ctx->first_pck_in_au, ctx->au_sap);
		if (ctx->au_sap == GF_FILTER_SAP_1) {
			ctx->dts_last_IDR = gf_filter_pck_get_dts(ctx->first_pck_in_au);
			if (ctx->is_paff) ctx->dts_last_IDR *= 2;
		}
		if (ctx->au_sap <= GF_FILTER_SAP_3) {
			is_rap = GF_TRUE;
		}
	}
	else if (ctx->has_islice && ctx->force_sync && (ctx->sei_recovery_frame_count==0)) {
		gf_filter_pck_set_sap(ctx->first_pck_in_au, GF_FILTER_SAP_1);
		if (!ctx->use_opengop_gdr) {
			ctx->use_opengop_gdr = 1;
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[%s] Forcing non-IDR samples with I slices to be marked as sync points - resulting file will not be ISOBMFF conformant\n", ctx->log_name));
		}
		is_rap = GF_TRUE;
	}
	/*set roll info sampleGroups info*/
	else if (!ctx->au_sap && ( (ctx->sei_recovery_frame_count >= 0) || ctx->has_islice) ) {
		/*generic GDR*/
		if (ctx->sei_recovery_frame_count > 0) {
			if (!ctx->use_opengop_gdr) ctx->use_opengop_gdr = 1;
			gf_filter_pck_set_sap(ctx->first_pck_in_au, GF_FILTER_SAP_4);
			gf_filter_pck_set_roll_info(ctx->first_pck_in_au, ctx->sei_recovery_frame_count);
		}
		/*open-GOP*/
		else if ((ctx->sei_recovery_frame_count == 0) && ctx->has_islice) {
			if (!ctx->use_opengop_gdr) ctx->use_opengop_gdr = 2;
			gf_filter_pck_set_sap(ctx->first_pck_in_au, GF_FILTER_SAP_3);
			is_rap = GF_TRUE;
		}
	}
	if (ctx->is_paff) {
		gf_filter_pck_set_interlaced(ctx->first_pck_in_au, ctx->bottom_field_flag ? 2 : 1);
	}

	//if TS is set, the packet was the first in AU in the input timed packet (eg PES), we reuse the input timing
	ts = gf_filter_pck_get_cts(ctx->first_pck_in_au);
	if (ts == GF_FILTER_NO_TS) {
		/*we store the POC (last POC minus the poc shift) as the CTS offset and re-update the CTS when dispatching*/
		assert(ctx->last_poc >= ctx->poc_shift);
		gf_filter_pck_set_cts(ctx->first_pck_in_au, CTS_POC_OFFSET_SAFETY + ctx->last_poc - ctx->poc_shift);
		//we use the carrousel flag temporarly to indicate the cts must be recomputed
		gf_filter_pck_set_carousel_version(ctx->first_pck_in_au, 1);
	}

	if (ctx->subsamp_buffer_size) {
		gf_filter_pck_set_property(ctx->first_pck_in_au, GF_PROP_PCK_SUBS, &PROP_DATA(ctx->subsamp_buffer, ctx->subsamp_buffer_size) );
		ctx->subsamp_buffer_size = 0;
		ctx->subs_mapped_bytes = 0;
	}
	if (ctx->deps) {
		u8 flags = 0;
		//dependsOn
		flags = (is_rap) ? 2 : 1;
		flags <<= 2;
		//dependedOn
	 	flags |= ctx->has_ref_slices ? 1 : 2;
		flags <<= 2;
		//hasRedundant
	 	flags |= ctx->has_redundant ? 1 : 2;
	 	gf_filter_pck_set_dependency_flags(ctx->first_pck_in_au, flags);
	}
	ctx->has_ref_slices = GF_FALSE;
	ctx->has_redundant = GF_FALSE;

	//if we reuse input packets timing, we can dispatch asap.
	//otherwise if poc probe is done (we know the min_poc_diff between images) and we are not in strict mode, dispatch asap
	//otherwise we will need to wait for the next ref frame to make sure we know all pocs ...
	if (ctx->timescale || (!ctx->strict_poc && ctx->poc_probe_done) )
		naludmx_enqueue_or_dispatch(ctx, NULL, GF_TRUE);

	ctx->first_pck_in_au = NULL;
}

static void naludmx_update_nalu_maxsize(GF_NALUDmxCtx *ctx, u32 size)
{
	if (ctx->max_nalu_size < size) {
		ctx->max_nalu_size = size;
		if (size > ctx->max_nalu_size_allowed) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[%s] nal size %d larger than max allowed size %d - change import settings\n", ctx->log_name, size, ctx->max_nalu_size_allowed ));
		}
	}
}


GF_Err naludmx_realloc_last_pck(GF_NALUDmxCtx *ctx, u32 nb_bytes_to_add, u8 **data_ptr)
{
	GF_Err e;
	u8 *pck_data;
	u32 full_size;
	GF_FilterPacket *pck = gf_list_last(ctx->pck_queue);
	*data_ptr = NULL;
	if (!pck) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[%s] attempt to reallocate a non-existing packet!\n", ctx->log_name));
		return GF_SERVICE_ERROR;
	}
	e = gf_filter_pck_expand(pck, nb_bytes_to_add, &pck_data, data_ptr, &full_size);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[%s] Failed to reallocate packet buffer: %s\n", ctx->log_name, gf_error_to_string(e) ));
		return e;
	}
	assert(ctx->bs_w);
	//rewrite NALU size length
	full_size -= ctx->nal_length;
	gf_bs_reassign_buffer(ctx->bs_w, pck_data, ctx->nal_length);
	gf_bs_write_int(ctx->bs_w, full_size, 8*ctx->nal_length);
	naludmx_update_nalu_maxsize(ctx, full_size);
	//rewrite subsample size
	if (ctx->subsamples) {
		assert(ctx->subsamp_buffer_size>=14);
		//reassign to begining of size field (after first u32 flags)
		gf_bs_reassign_buffer(ctx->bs_w, ctx->subsamp_buffer + ctx->subsamp_buffer_size-14 + 4, 14 - 4);
		gf_bs_write_u32(ctx->bs_w, full_size + ctx->nal_length);
	}
	return GF_OK;
}

GF_FilterPacket *naludmx_start_nalu(GF_NALUDmxCtx *ctx, u32 nal_size, Bool skip_nal_field, Bool *au_start, u8 **pck_data)
{
	GF_FilterPacket *dst_pck = gf_filter_pck_new_alloc(ctx->opid, nal_size + (skip_nal_field ? 0 : ctx->nal_length), pck_data);

	if (!skip_nal_field) {
		if (!ctx->bs_w) ctx->bs_w = gf_bs_new(*pck_data, ctx->nal_length, GF_BITSTREAM_WRITE);
		else gf_bs_reassign_buffer(ctx->bs_w, *pck_data, ctx->nal_length);
		gf_bs_write_int(ctx->bs_w, nal_size, 8*ctx->nal_length);
	}

	if (*au_start) {
		ctx->first_pck_in_au = dst_pck;
		if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);

		gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_FALSE);
		//we reuse the timing of the input packet for the first nal of the first frame starting in this packet
		if (ctx->input_is_au_start) {
			ctx->input_is_au_start = GF_FALSE;
			gf_filter_pck_set_dts(dst_pck, ctx->dts);
			gf_filter_pck_set_cts(dst_pck, ctx->cts);
		} else {
			//we don't set the CTS, it will be set once we detect frame end
			gf_filter_pck_set_dts(dst_pck, ctx->dts);
		}
		//we use the carrousel flag temporarly to indicate the cts must be recomputed
		gf_filter_pck_set_carousel_version(dst_pck, ctx->timescale ? 0 : 1);

		gf_filter_pck_set_duration(dst_pck, ctx->pck_duration ? ctx->pck_duration : ctx->cur_fps.den);
		if (ctx->in_seek) gf_filter_pck_set_seek_flag(dst_pck, GF_TRUE);

		naludmx_update_time(ctx);
		*au_start = GF_FALSE;
	} else {
		gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
	}
	naludmx_update_nalu_maxsize(ctx, nal_size);

	naludmx_enqueue_or_dispatch(ctx, dst_pck, GF_FALSE);

	return dst_pck;
}

void naludmx_add_subsample(GF_NALUDmxCtx *ctx, u32 subs_size, u8 subs_priority, u32 subs_reserved)
{
	if (ctx->subsamp_buffer_alloc < ctx->subsamp_buffer_size+14 ) {
		ctx->subsamp_buffer_alloc = ctx->subsamp_buffer_size+14;
		ctx->subsamp_buffer = gf_realloc(ctx->subsamp_buffer, ctx->subsamp_buffer_alloc);
	}
	assert(ctx->subsamp_buffer);
	gf_bs_reassign_buffer(ctx->bs_w, ctx->subsamp_buffer + ctx->subsamp_buffer_size, 14);
	gf_bs_write_u32(ctx->bs_w, 0); //flags
	gf_bs_write_u32(ctx->bs_w, subs_size + ctx->nal_length);
	gf_bs_write_u32(ctx->bs_w, subs_reserved); //reserved
	gf_bs_write_u8(ctx->bs_w, subs_priority); //priority
	gf_bs_write_u8(ctx->bs_w, 0); //discardable - todo
	ctx->subsamp_buffer_size += 14;
	ctx->subs_mapped_bytes += subs_size + ctx->nal_length;
}


static s32 naludmx_parse_nal_hevc(GF_NALUDmxCtx *ctx, char *data, u32 size, Bool *skip_nal, Bool *is_slice, Bool *is_islice)
{
	s32 ps_idx = 0;
	s32 res;
	u8 nal_unit_type, temporal_id, layer_id;
	*skip_nal = GF_FALSE;

	gf_bs_reassign_buffer(ctx->bs_r, data, size);
	res = gf_media_hevc_parse_nalu_bs(ctx->bs_r, ctx->hevc_state, &nal_unit_type, &temporal_id, &layer_id);
	ctx->nb_nalus++;

	if (res < 0) {
		if (res == -1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[%s] Warning: Error parsing NAL unit\n", ctx->log_name));
		}
		*skip_nal = GF_TRUE;
	}

	if (layer_id && ctx->nosvc) {
		*skip_nal = GF_TRUE;
		return 0;
	}

	switch (nal_unit_type) {
	case GF_HEVC_NALU_VID_PARAM:
		if (ctx->novpsext) {
			//this may modify nal_size, but we don't use it for bitstream reading
			ps_idx = gf_media_hevc_read_vps_ex(data, &size, ctx->hevc_state, GF_TRUE);
		} else {
			ps_idx = ctx->hevc_state->last_parsed_vps_id;
		}
		if (ps_idx<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[%s] Error parsing Video Param Set\n", ctx->log_name));
		} else {
			naludmx_queue_param_set(ctx, data, size, GF_HEVC_NALU_VID_PARAM, ps_idx);
		}
		*skip_nal = GF_TRUE;
		break;
	case GF_HEVC_NALU_SEQ_PARAM:
		ps_idx = ctx->hevc_state->last_parsed_sps_id;
		if (ps_idx<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[%s] Error parsing Sequence Param Set\n", ctx->log_name));
		} else {
			naludmx_queue_param_set(ctx, data, size, GF_HEVC_NALU_SEQ_PARAM, ps_idx);
		}
		*skip_nal = GF_TRUE;
		break;
	case GF_HEVC_NALU_PIC_PARAM:
		ps_idx = ctx->hevc_state->last_parsed_pps_id;
		if (ps_idx<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[%s] Error parsing Picture Param Set\n", ctx->log_name));
		} else {
			naludmx_queue_param_set(ctx, data, size, GF_HEVC_NALU_PIC_PARAM, ps_idx);
		}
		*skip_nal = GF_TRUE;
		break;
	case GF_HEVC_NALU_SEI_PREFIX:
		if (!ctx->nosei) {
			ctx->nb_sei++;

			if (ctx->sei_buffer_alloc < ctx->sei_buffer_size + size + ctx->nal_length) {
				ctx->sei_buffer_alloc = ctx->sei_buffer_size + size + ctx->nal_length;
				ctx->sei_buffer = gf_realloc(ctx->sei_buffer, ctx->sei_buffer_alloc);
			}

			if (!ctx->bs_w) ctx->bs_w = gf_bs_new(ctx->sei_buffer + ctx->sei_buffer_size, ctx->nal_length + size, GF_BITSTREAM_WRITE);
			else gf_bs_reassign_buffer(ctx->bs_w, ctx->sei_buffer + ctx->sei_buffer_size, ctx->nal_length + size);
			gf_bs_write_int(ctx->bs_w, size, 8*ctx->nal_length);
			memcpy(ctx->sei_buffer + ctx->sei_buffer_size + ctx->nal_length, data, size);
			ctx->sei_buffer_size += size + ctx->nal_length;
		} else {
			ctx->nb_nalus--;
		}
		*skip_nal = GF_TRUE;
		break;
	case GF_HEVC_NALU_SEI_SUFFIX:
		if (! ctx->is_playing) return 0;
		if (ctx->nosei) {
			*skip_nal = GF_TRUE;
			ctx->nb_nalus--;
		} else {
			ctx->nb_sei++;
		}
		break;

	/*slice_segment_layer_rbsp*/
	case GF_HEVC_NALU_SLICE_STSA_N:
	case GF_HEVC_NALU_SLICE_STSA_R:
	case GF_HEVC_NALU_SLICE_RADL_R:
	case GF_HEVC_NALU_SLICE_RASL_R:
	case GF_HEVC_NALU_SLICE_RADL_N:
	case GF_HEVC_NALU_SLICE_RASL_N:
	case GF_HEVC_NALU_SLICE_TRAIL_N:
	case GF_HEVC_NALU_SLICE_TRAIL_R:
	case GF_HEVC_NALU_SLICE_TSA_N:
	case GF_HEVC_NALU_SLICE_TSA_R:
	case GF_HEVC_NALU_SLICE_BLA_W_LP:
	case GF_HEVC_NALU_SLICE_BLA_W_DLP:
	case GF_HEVC_NALU_SLICE_BLA_N_LP:
	case GF_HEVC_NALU_SLICE_IDR_W_DLP:
	case GF_HEVC_NALU_SLICE_IDR_N_LP:
	case GF_HEVC_NALU_SLICE_CRA:
		if (! ctx->is_playing) return 0;
		*is_slice = GF_TRUE;
		if (! *skip_nal) {
			switch (ctx->hevc_state->s_info.slice_type) {
			case GF_HEVC_SLICE_TYPE_P:
				if (layer_id) ctx->nb_e_p++;
				else ctx->nb_p++;
				break;
			case GF_HEVC_SLICE_TYPE_I:
				if (layer_id) ctx->nb_e_i++;
				else ctx->nb_i++;
				*is_islice = GF_TRUE;
				break;
			case GF_HEVC_SLICE_TYPE_B:
				if (layer_id) ctx->nb_e_b++;
				else ctx->nb_b++;
				break;
			}
		}
		break;

	case GF_HEVC_NALU_ACCESS_UNIT:
		ctx->nb_aud++;
		if (!ctx->audelim) {
			*skip_nal = GF_TRUE;
		} else if (!ctx->opid) {
			ctx->has_initial_aud = GF_TRUE;
			memcpy(ctx->init_aud, data, 3);
		}
		break;
	/*remove*/
	case GF_HEVC_NALU_FILLER_DATA:
	case GF_HEVC_NALU_END_OF_SEQ:
	case GF_HEVC_NALU_END_OF_STREAM:
		*skip_nal = GF_TRUE;
		break;

	default:
		if (! ctx->is_playing) return 0;
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[%s] NAL Unit type %d not handled - adding\n", ctx->log_name, nal_unit_type));
		break;
	}
	if (*skip_nal) return res;

	ctx->linf[layer_id].layer_id_plus_one = layer_id + 1;
	if (! ctx->linf[layer_id].max_temporal_id ) ctx->linf[layer_id].max_temporal_id = temporal_id;
	else if (ctx->linf[layer_id].max_temporal_id < temporal_id) ctx->linf[layer_id].max_temporal_id = temporal_id;

	if (! ctx->linf[layer_id].min_temporal_id ) ctx->linf[layer_id].min_temporal_id = temporal_id;
	else if (ctx->linf[layer_id].min_temporal_id > temporal_id) ctx->linf[layer_id].min_temporal_id = temporal_id;

	if (ctx->max_temporal_id[layer_id] < temporal_id)
		ctx->max_temporal_id[layer_id] = temporal_id;
	if (ctx->min_layer_id > layer_id) ctx->min_layer_id = layer_id;
	return res;
}

static s32 naludmx_parse_nal_avc(GF_NALUDmxCtx *ctx, char *data, u32 size, u32 nal_type, Bool *skip_nal, Bool *is_slice, Bool *is_islice)
{
	s32 ps_idx = 0;
	s32 res = 0;

	gf_bs_reassign_buffer(ctx->bs_r, data, size);
	*skip_nal = GF_FALSE;
	res = gf_media_avc_parse_nalu(ctx->bs_r, ctx->avc_state);
	if (res < 0) {
		if (res == -1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[%s] Warning: Error parsing NAL unit\n", ctx->log_name));
		}
		*skip_nal = GF_TRUE;
	}
	ctx->nb_nalus++;
	
	switch (nal_type) {
	case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
	case GF_AVC_NALU_SEQ_PARAM:
		ps_idx = ctx->avc_state->last_ps_idx;
		if (ps_idx<0) {
			if (ctx->avc_state->sps[0].profile_idc) {
				GF_LOG(ctx->avc_state->sps[0].profile_idc ? GF_LOG_WARNING : GF_LOG_ERROR, GF_LOG_PARSER, ("[%s] Error parsing Sequence Param Set\n", ctx->log_name));
			}
		} else {
			naludmx_queue_param_set(ctx, data, size, GF_AVC_NALU_SEQ_PARAM, ps_idx);
		}
		*skip_nal = GF_TRUE;
		return 0;

	case GF_AVC_NALU_PIC_PARAM:
		ps_idx = ctx->avc_state->last_ps_idx;
		if (ps_idx<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[%s] Error parsing Picture Param Set\n", ctx->log_name));
		} else {
			naludmx_queue_param_set(ctx, data, size, GF_AVC_NALU_PIC_PARAM, ps_idx);
		}
		*skip_nal = GF_TRUE;
		return 0;

	case GF_AVC_NALU_SEQ_PARAM_EXT:
		ps_idx = ctx->avc_state->last_ps_idx;
		if (ps_idx<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[%s] Error parsing Sequence Param Set Extension\n", ctx->log_name));
		} else {
			naludmx_queue_param_set(ctx, data, size, GF_AVC_NALU_SEQ_PARAM_EXT, ps_idx);
		}
		*skip_nal = GF_TRUE;
		return 0;

	case GF_AVC_NALU_SEI:
		if (ctx->avc_state->sps_active_idx != -1) {
			u32 rw_sei_size, sei_size = size;
			if (ctx->sei_buffer_alloc < ctx->sei_buffer_size + sei_size + ctx->nal_length) {
				ctx->sei_buffer_alloc = ctx->sei_buffer_size + sei_size + ctx->nal_length;
				ctx->sei_buffer = gf_realloc(ctx->sei_buffer, ctx->sei_buffer_alloc);
			}

			if (!ctx->bs_w) ctx->bs_w = gf_bs_new(ctx->sei_buffer + ctx->sei_buffer_size, ctx->nal_length + sei_size, GF_BITSTREAM_WRITE);
			else gf_bs_reassign_buffer(ctx->bs_w, ctx->sei_buffer + ctx->sei_buffer_size, ctx->nal_length + sei_size);
			gf_bs_write_int(ctx->bs_w, sei_size, 8*ctx->nal_length);

			memcpy(ctx->sei_buffer + ctx->sei_buffer_size + ctx->nal_length, data, sei_size);
			rw_sei_size = gf_media_avc_reformat_sei(ctx->sei_buffer + ctx->sei_buffer_size + ctx->nal_length, sei_size, ctx->seirw, ctx->avc_state);
			if (rw_sei_size < sei_size) {
				gf_bs_seek(ctx->bs_w, 0);
				gf_bs_write_int(ctx->bs_w, rw_sei_size, 8*ctx->nal_length);
			}

			*skip_nal = GF_TRUE;
			ctx->sei_buffer_size += rw_sei_size + ctx->nal_length;

			if (ctx->nosei) {
				ctx->sei_buffer_size = 0;
			} else {
				ctx->nb_sei++;
			}
		}
		return 0;

	case GF_AVC_NALU_ACCESS_UNIT:
		ctx->nb_aud++;
		if (!ctx->audelim) {
			*skip_nal = GF_TRUE;
		} else if (!ctx->opid) {
			ctx->has_initial_aud = GF_TRUE;
			memcpy(ctx->init_aud, data, 2);
		}
		return 1;
	/*remove*/
	case GF_AVC_NALU_FILLER_DATA:
	case GF_AVC_NALU_END_OF_SEQ:
	case GF_AVC_NALU_END_OF_STREAM:
		*skip_nal = GF_TRUE;
		return 0;

	//update stats
	case GF_AVC_NALU_NON_IDR_SLICE:
	case GF_AVC_NALU_DP_A_SLICE:
	case GF_AVC_NALU_DP_B_SLICE:
	case GF_AVC_NALU_DP_C_SLICE:
	case GF_AVC_NALU_IDR_SLICE:
		*is_slice = GF_TRUE;
		switch (ctx->avc_state->s_info.slice_type) {
		case GF_AVC_TYPE_P:
		case GF_AVC_TYPE2_P:
			ctx->nb_p++;
			break;
		case GF_AVC_TYPE_I:
		case GF_AVC_TYPE2_I:
			ctx->nb_i++;
			*is_islice = GF_TRUE;
			break;
		case GF_AVC_TYPE_B:
		case GF_AVC_TYPE2_B:
			ctx->nb_b++;
			break;
		case GF_AVC_TYPE_SP:
		case GF_AVC_TYPE2_SP:
			ctx->nb_sp++;
			break;
		case GF_AVC_TYPE_SI:
		case GF_AVC_TYPE2_SI:
			ctx->nb_si++;
			break;
		}
		break;

	case GF_AVC_NALU_SVC_SLICE:
		if (!ctx->explicit) {
			u32 i;
			for (i = 0; i < gf_list_count(ctx->pps); i ++) {
				GF_AVCConfigSlot *slc = (GF_AVCConfigSlot*)gf_list_get(ctx->pps, i);
				if (ctx->avc_state->s_info.pps->id == slc->id) {
					/* This PPS is used by an SVC NAL unit, it should be moved to the SVC Config Record) */
					gf_list_rem(ctx->pps, i);
					i--;
					if (!ctx->pps_svc) ctx->pps_svc = gf_list_new(ctx->pps_svc);
					gf_list_add(ctx->pps_svc, slc);
					ctx->ps_modified = GF_TRUE;
				}
			}
		}
		*is_slice = GF_TRUE;
        if (ctx->avc_state->s_info.sps) {
            switch (ctx->avc_state->s_info.slice_type) {
            case GF_AVC_TYPE_P:
            case GF_AVC_TYPE2_P:
                ctx->avc_state->s_info.sps->nb_ep++;
                break;
            case GF_AVC_TYPE_I:
            case GF_AVC_TYPE2_I:
                ctx->avc_state->s_info.sps->nb_ei++;
                break;
            case GF_AVC_TYPE_B:
            case GF_AVC_TYPE2_B:
                ctx->avc_state->s_info.sps->nb_eb++;
                break;
            }
        }
        break;
	case GF_AVC_NALU_SLICE_AUX:
		*is_slice = GF_TRUE;
		break;
	}
	return res;
}

GF_Err naludmx_process(GF_Filter *filter)
{
	GF_NALUDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	GF_Err e;
	char *data;
	u8 *start;
	u32 pck_size;
	u32 hdr_size_at_resume = 0;
	u32 nalu_before = ctx->nb_nalus;
	s32 remain;
	Bool is_eos = GF_FALSE;

	//always reparse duration
	if (!ctx->file_loaded)
		naludmx_check_dur(filter, ctx);

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (ctx->bytes_in_header || ctx->hdr_store_size) {
				start = data = ctx->hdr_store;
				remain = pck_size = ctx->bytes_in_header ? ctx->bytes_in_header : ctx->hdr_store_size;
				ctx->bytes_in_header = 0;
				ctx->hdr_store_size = 0;
				is_eos = GF_TRUE;
				goto naldmx_flush;
			}
			if (ctx->first_pck_in_au) {
				naludmx_finalize_au_flags(ctx);
			}
			naludmx_enqueue_or_dispatch(ctx, NULL, GF_TRUE);
			if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
			ctx->src_pck = NULL;
			if (!ctx->opid) return GF_EOS;

			gf_filter_pid_set_info(ctx->opid, GF_PROP_PID_MAX_NALU_SIZE, &PROP_UINT(ctx->max_nalu_size) );
			if (ctx->is_hevc) {
				naludmx_set_hevc_oinf(ctx, ctx->max_temporal_id);
				naludmx_set_hevc_linf(ctx);
				gf_filter_pid_set_property_str(ctx->opid, "hevc:min_lid", &PROP_UINT(ctx->min_layer_id) );
			}
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);
	start = data;
	remain = pck_size;

	//input pid sets some timescale - we flushed pending data , update cts
	if (ctx->timescale) {
		u64 ts = gf_filter_pck_get_cts(pck);
		if (ts != GF_FILTER_NO_TS)
			ctx->cts = ts;
		ts = gf_filter_pck_get_dts(pck);
		if (ts != GF_FILTER_NO_TS) {
			ctx->dts = ts;
			if (!ctx->prev_dts) ctx->prev_dts = ts;
			else if (ctx->prev_dts != ts) {
				u64 diff = ts;
				diff -= ctx->prev_dts;
				if (!ctx->cur_fps.den)
					ctx->cur_fps.den = (u32) diff;
				else if (ctx->cur_fps.den > diff)
					ctx->cur_fps.den = (u32) diff;

				ctx->prev_dts = ts;
			}
		}
		ctx->pck_duration = gf_filter_pck_get_duration(pck);
		if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
		ctx->src_pck = pck;
		gf_filter_pck_ref_props(&ctx->src_pck);
		//store framing flags. If input_is_au_start, the first NAL of the first frame begining in this packet will
		//use the DTS/CTS of the inout packet, otherwise we will use our internal POC recompute
		gf_filter_pck_get_framing(pck, &ctx->input_is_au_start, NULL);
	}

	//we stored some data to find the complete vosh, aggregate this packet with current one
	if (!ctx->resume_from && ctx->hdr_store_size) {
		if (ctx->hdr_store_alloc < ctx->hdr_store_size + pck_size) {
			ctx->hdr_store_alloc = ctx->hdr_store_size + pck_size;
			ctx->hdr_store = gf_realloc(ctx->hdr_store, sizeof(char)*ctx->hdr_store_alloc);
		}
		hdr_size_at_resume = ctx->hdr_store_size;
		memcpy(ctx->hdr_store + ctx->hdr_store_size, data, sizeof(char)*pck_size);
		ctx->hdr_store_size += pck_size;
		start = data = ctx->hdr_store;
		remain = pck_size = ctx->hdr_store_size;
	}

	if (ctx->resume_from) {
		if (ctx->opid && gf_filter_pid_would_block(ctx->opid))
			return GF_OK;

		//resume from data copied internally
		if (ctx->hdr_store_size) {
			assert(ctx->resume_from <= ctx->hdr_store_size);
			start = data = ctx->hdr_store + ctx->resume_from;
			remain = pck_size = ctx->hdr_store_size - ctx->resume_from;
		} else {
			assert((s32)ctx->resume_from >0);
            if (ctx->resume_from > (u32) remain) {
                GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[%s] something wrong, resuming parsing at %d bytes but only %d bytes in packet, discarding data\n", ctx->log_name, ctx->resume_from, remain));
            } else {
                start += ctx->resume_from;
                remain -= ctx->resume_from;
            }
		}
		ctx->resume_from = 0;
	}

naldmx_flush:
	if (!ctx->bs_r) {
		ctx->bs_r = gf_bs_new(start, remain, GF_BITSTREAM_READ);
	} else {
		gf_bs_reassign_buffer(ctx->bs_r, start, remain);
	}

    assert(remain>=0);
    
	while (remain) {
		u8 *pck_data;
		u8 *hdr_start;
		u32 hdr_avail;
		u8 *pck_start;
		u32 pck_avail;
		s32 current;
		Bool nal_hdr_in_store = GF_FALSE;
		Bool nal_sc_in_store = GF_FALSE;
		u32 nal_bytes_from_store = 0;
		Bool skip_nal = GF_FALSE;
		u64 size=0;
		u32 sc_size, store_sc_size=0;
		u32 bytes_from_store = 0;
		u32 hdr_offset = 0;
		Bool full_nal_required = GF_FALSE;
		u32 nal_type = 0;
		u32 nal_ref_idc = 0;
		s32 next=0;
		u32 next_sc_size=0;
		s32 nal_parse_result;
		Bool slice_is_ref, slice_force_ref;
		Bool is_slice = GF_FALSE;
		Bool is_islice = GF_FALSE;
		Bool bottom_field_flag = GF_FALSE;
		Bool au_start;
		u32 avc_svc_subs_reserved = 0;
		u8 avc_svc_subs_priority = 0;
		Bool recovery_point_valid = GF_FALSE;
		u32 recovery_point_frame_cnt = 0;
		Bool bIntraSlice = GF_FALSE;
		GF_FilterSAPType au_sap_type = GF_FILTER_SAP_NONE;
		Bool slice_is_b = GF_FALSE;
		Bool full_nal = GF_FALSE;
		s32 slice_poc = 0;
		u32 next_size = 0;

		Bool copy_last_bytes = GF_FALSE;

		//not enough bytes to parse start code
		if (!is_eos && (remain<6)) {
			memcpy(ctx->hdr_store, start, remain);
			ctx->bytes_in_header = remain;
			break;
		}
		current = -1;

		//we have some potential bytes of a start code in the store, copy some more bytes and check if valid start code.
		//if not, dispatch these bytes as continuation of the data
		if (ctx->bytes_in_header) {
			Bool split_start_code=GF_FALSE;

			memcpy(ctx->hdr_store + ctx->bytes_in_header, start, SAFETY_NAL_STORE - ctx->bytes_in_header);
			current = gf_media_nalu_next_start_code(ctx->hdr_store, SAFETY_NAL_STORE, &sc_size);
			if (current==SAFETY_NAL_STORE)
				current = -1;

			//no start code in stored buffer
			if (current<0) {
				if (!ctx->next_nal_end_skip) {
					e = naludmx_realloc_last_pck(ctx, ctx->bytes_in_header, &pck_data);
					if (e==GF_OK) {
						memcpy(pck_data, ctx->hdr_store, ctx->bytes_in_header);
					}
				}
				ctx->bytes_in_header = 0;
			} else {
				//if start is greater than stored data, the nal is completely in the input packet (but the start code may not be)
				if (current + sc_size > ctx->bytes_in_header) {
					//we need to dispatch current bytes from the header, whether all these bytes were in the previous
					//packet or some are in the new one does not matter

					bytes_from_store = current;
					//the offset to the NAL first byte in the store is current+sc_size,
					//hence current+sc_pos-ctx->bytes_in_header in the new packet
					hdr_offset = current + sc_size - ctx->bytes_in_header;
					nal_sc_in_store = GF_TRUE;
				} else {
					//this is trickier, nal start is in the store buffer
					u8 lastb2, lastb1, nextb1;

					lastb2 = ctx->hdr_store[ctx->bytes_in_header-2];
					lastb1 = ctx->hdr_store[ctx->bytes_in_header-1];
					nextb1 = start[0];

					if (!lastb1 && !lastb2) { split_start_code=GF_TRUE; }
					else if (!lastb1 && !nextb1) { split_start_code = GF_TRUE; }

					if (split_start_code ) {
						u32 store_remain = SAFETY_NAL_STORE - (current + sc_size);
						u32 next_sc_size;
						s32 next = gf_media_nalu_next_start_code(ctx->hdr_store + current+sc_size, store_remain, &next_sc_size);
						assert(!current);
						if ((next>0) && (next + current+sc_size < ctx->bytes_in_header)) {

							next_size = next - current;
							nal_bytes_from_store = next_size;
							store_sc_size=sc_size;
						} else {
							split_start_code = 0;
						}
					}

					//we still have current bytes to dispatch
					bytes_from_store = current;

					//the offset in the STORE is current+sc_size, and we parse the store, not the packet !!
					hdr_offset = current + sc_size;

					nal_hdr_in_store = GF_TRUE;


					if (!split_start_code) {
						if (sc_size == ctx->bytes_in_header) {
							nal_hdr_in_store = GF_FALSE;
							sc_size = 0;
							hdr_offset=0;
							nal_bytes_from_store = 0;
						} else {
							//we need to copy the nal first bytes from the store
							nal_bytes_from_store = ctx->bytes_in_header - (current + sc_size);
						}
					}

				}
				if (!split_start_code) {
					ctx->bytes_in_header = 0;
				}
			}
		}
		//no starcode in store, look for startcode in packet
		if (current == -1) {
			//locate next start code
			current = gf_media_nalu_next_start_code(start, remain, &sc_size);
			if (current == remain)
				current = -1;

			//no start code, dispatch the block
			if (current<0) {
				u8 b3, b2, b1;
				if (! ctx->first_pck_in_au) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[%s] no start code in block and no frame started, discarding data\n", ctx->log_name));
					break;
				}
				size = remain;
				b3 = start[remain-3];
				b2 = start[remain-2];
				b1 = start[remain-1];
				//we may have a startcode at the end of the packet, store it and don't dispatch the last 3 bytes !
				if (!is_eos && (!b1 || !b2 || !b3)) {
					copy_last_bytes = GF_TRUE;
					assert(size >= 3);
					size -= 3;
					ctx->bytes_in_header = 3;
				}
				if (!ctx->next_nal_end_skip) {
					e = naludmx_realloc_last_pck(ctx, (u32) size, &pck_data);
					if (e==GF_OK)
						memcpy(pck_data, start, (size_t) size);
				}

				if (copy_last_bytes) {
					memcpy(ctx->hdr_store, start+remain-3, 3);
				}
				break;
			}
		}

		assert(current>=0);


		//skip if no output pid
		if (!ctx->opid && current) {
			assert(remain>=current);
            assert((s32) current >= 0);
            
			start += current;
			remain -= current;
			current = 0;
			ctx->next_nal_end_skip = GF_FALSE;
		}

		//dispatch remaining bytes
		if (current>0) {
			e = GF_EOS;
			//flush remaining bytes in NAL
			if (!ctx->next_nal_end_skip) {
				e = naludmx_realloc_last_pck(ctx, current, &pck_data);
			}
			//bytes were partly in store, partly in packet
			if (bytes_from_store) {
				if (bytes_from_store>=(u32) current) {
					//we still have that many bytes from the store to dispatch
//					bytes_from_store -= current;
				} else {
					//we are done, the nal header and start code is completely in the new packet
					u32 shift = current - bytes_from_store;
//					bytes_from_store = 0;
                    assert(remain >= (s32) shift);
                    assert((s32) shift >= 0);
					start += shift;
					remain -= shift;
					nal_sc_in_store = 0;
				}
				if (e==GF_OK) {
					memcpy(pck_data, ctx->hdr_store, current);
				}
			} else {
				if (e==GF_OK) {
					memcpy(pck_data, start, current);
				}
				assert(remain>=current);
                assert((s32) current>=0);
				start += current;
				remain -= current;
				current = 0;
			}
		}
		if (!remain)
			break;

		//nal hdr is in the store, use the store to parse slice header
		if (nal_hdr_in_store) {
			hdr_start = ctx->hdr_store + hdr_offset;
			hdr_avail = SAFETY_NAL_STORE - hdr_offset;
			pck_start = start;
			pck_avail = remain;
			sc_size = 0;
		}
		//nal hdr is in new packet at hdr_offset, use the packet to parse slice header
		else if (nal_sc_in_store) {
			hdr_start = start + hdr_offset;
			hdr_avail = remain - hdr_offset;
			pck_start = hdr_start;
			pck_avail = hdr_avail;
			sc_size = 0;
		}
		//nal hdr is in new packet start + sc_size, use the packet to parse slice header
		else {
			hdr_start = start + sc_size;
			hdr_avail = remain - sc_size;
			pck_start = hdr_start;
			pck_avail = hdr_avail;
		}

		//figure out which nal we need to completely load
		if (ctx->is_hevc) {
			nal_type = hdr_start[0];
			nal_type = (nal_type & 0x7E) >> 1;
			switch (nal_type) {
			case GF_HEVC_NALU_VID_PARAM:
			case GF_HEVC_NALU_SEQ_PARAM:
			case GF_HEVC_NALU_PIC_PARAM:
			case GF_HEVC_NALU_SEI_PREFIX:
			case GF_HEVC_NALU_SEI_SUFFIX:
				if (!ctx->analyze)
					full_nal_required = GF_TRUE;
				break;
			case GF_HEVC_NALU_SLICE_TRAIL_N:
			case GF_HEVC_NALU_SLICE_TSA_N:
			case GF_HEVC_NALU_SLICE_STSA_N:
			case GF_HEVC_NALU_SLICE_RADL_N:
			case GF_HEVC_NALU_SLICE_RASL_N:
			case GF_HEVC_NALU_SLICE_RSV_VCL_N10:
			case GF_HEVC_NALU_SLICE_RSV_VCL_N12:
			case GF_HEVC_NALU_SLICE_RSV_VCL_N14:
				if (ctx->deps) {
					HEVC_VPS *vps;
					u32 temporal_id = hdr_start[1] & 0x7;
					vps = & ctx->hevc_state->vps[ctx->hevc_state->s_info.sps->vps_id];
					if (temporal_id + 1 < vps->max_sub_layers) {
						nal_ref_idc = GF_TRUE;
					}
				}
				break;
			default:
				if (nal_type<GF_HEVC_NALU_VID_PARAM)
					nal_ref_idc = GF_TRUE;
				break;
			}
		} else {
			nal_type = hdr_start[0] & 0x1F;
			switch (nal_type) {
			case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
			case GF_AVC_NALU_SEQ_PARAM:
			case GF_AVC_NALU_PIC_PARAM:
			case GF_AVC_NALU_SVC_PREFIX_NALU:
			//we also need the SEI in AVC since some SEI messages have to be removed
			case GF_AVC_NALU_SEI:
				if (!ctx->analyze)
					full_nal_required = GF_TRUE;
				break;
			default:
				break;
			}
			nal_ref_idc = (hdr_start[0] & 0x60) >> 5;
		}
		if (!next_size) {
			if (full_nal_required && !is_eos) {
				//we need the full nal loaded
				next = gf_media_nalu_next_start_code(pck_start, pck_avail, &next_sc_size);
				if (!ctx->full_au_source && (next==pck_avail))
					next = -1;

				if (next<0) {
					if (sc_size) pck_avail += sc_size;
					if (ctx->hdr_store_alloc < ctx->hdr_store_size + pck_avail) {
						ctx->hdr_store_alloc = ctx->hdr_store_size + pck_avail;
						ctx->hdr_store = gf_realloc(ctx->hdr_store, sizeof(char)*ctx->hdr_store_alloc);
					}
					memcpy(ctx->hdr_store + ctx->hdr_store_size, start, sizeof(char)*pck_avail);
					ctx->hdr_store_size += pck_avail;
					gf_filter_pid_drop_packet(ctx->ipid);
					return GF_OK;
				}
			} else {
				next = gf_media_nalu_next_start_code(pck_start, pck_avail, &next_sc_size);
				if ( (next == pck_avail) && !ctx->full_au_source) {
					if (!is_eos)
						next = -1;
					else if (ctx->is_hevc && pck_avail<2)
						next = -1;
				}
			}

			//ok we have either a full nal, or the start of a NAL we can start to process, parse NAL
			if (next<0) {
				size = pck_avail;
				if (!nal_hdr_in_store) {
					hdr_avail = (u32) size;
				}
			} else {
				full_nal = GF_TRUE;
				size = next;
				if (full_nal_required) {
					if (!nal_bytes_from_store && nal_sc_in_store) {
						hdr_avail = (u32) size;
						hdr_start = pck_start;

					} else {
						size += nal_bytes_from_store;
						//we require full nal but begining of the nal is in the store, the rest in the current packet.
						//We must copy the complete nal in the store before parsing it
						if (nal_bytes_from_store && (size > SAFETY_NAL_STORE)) {
							u32 copy_size = (u32) (size - nal_bytes_from_store);
							if (ctx->hdr_store_alloc<copy_size) {
								ctx->hdr_store_alloc = copy_size;
								ctx->hdr_store = gf_realloc(ctx->hdr_store, ctx->hdr_store_alloc);
							}
							memcpy(ctx->hdr_store + hdr_offset + nal_bytes_from_store, start, copy_size);
						}
						hdr_avail = (u32) size;
					}
				} else if (hdr_avail>(u32) size && !nal_bytes_from_store)
					hdr_avail = (u32) size;
			}
			if (!full_nal && (size < SAFETY_NAL_STORE/2)) {
				assert(!nal_sc_in_store);
				assert(!nal_hdr_in_store);
				assert(remain < SAFETY_NAL_STORE);
				//we may have garbage at the end os stream, if the stream was cancelled in the middle (truncated nal)
				if (!is_eos) {
					memcpy(ctx->hdr_store, start, remain);
					ctx->bytes_in_header = remain;
				}
				break;
			}
		} else {
			full_nal = GF_TRUE;
			size = next_size;
		}

		if (ctx->is_hevc) {
			nal_parse_result = naludmx_parse_nal_hevc(ctx, hdr_start, hdr_avail, &skip_nal, &is_slice, &is_islice);
		} else {
			nal_parse_result = naludmx_parse_nal_avc(ctx, hdr_start, hdr_avail, nal_type, &skip_nal, &is_slice, &is_islice);
		}

		if (ctx->analyze) {
			skip_nal = GF_FALSE;
			ctx->sei_buffer_size = 0;
		}

		//new frame - if no slices, we detected the new frame on AU delimiter, don't flush new frame !
		if ((nal_parse_result>0) && !ctx->first_slice_in_au) {
			//new frame - we flush later on
			naludmx_finalize_au_flags(ctx);

			ctx->has_islice = GF_FALSE;
			ctx->first_slice_in_au = GF_TRUE;
			ctx->sei_recovery_frame_count = -1;
			ctx->au_sap = GF_FILTER_SAP_NONE;
			ctx->bottom_field_flag = GF_FALSE;
		}

		if (skip_nal) {
			assert(remain >= (s32)sc_size+next);
			if (next<0) {
				u8 b3, b2, b1;
				b3 = start[remain-3];
				b2 = start[remain-2];
				b1 = start[remain-1];
				//we may have a startcode at the end of the packet, store it and don't dispatch the last 3 bytes !
				if (!b1 || !b2 || !b3) {
					memcpy(ctx->hdr_store, start+remain-3, 3);
					ctx->bytes_in_header = 3;
				}
				ctx->next_nal_end_skip = GF_TRUE;
				break;
			}
			assert(remain >= next);
			start = pck_start + next;
            assert((u32) (start - (u8*)data) <= pck_size);
			remain = pck_size - (u32) (start - (u8*)data);
			if (nal_hdr_in_store) {
				if (full_nal && ctx->bytes_in_header) {
					memmove(ctx->hdr_store, hdr_start+nal_bytes_from_store, ctx->bytes_in_header - nal_bytes_from_store);
					ctx->bytes_in_header -= next_size+store_sc_size;
				} else {
					ctx->bytes_in_header = 0;
				}
			}
			continue;
		}
		ctx->next_nal_end_skip = GF_FALSE;

		naludmx_check_pid(filter, ctx);
		if (!ctx->opid) {
            u32 skip = sc_size+next;
			assert(remain >= (s32) skip);
            if (skip) {
                start += sc_size+next;
                remain -= sc_size+next;
            } else {
                ctx->bytes_in_header = 0;
            }
			continue;
		}

		//at this point, we no longer reaggregate packets
		ctx->hdr_store_size = 0;


		if (!ctx->is_playing) {
			ctx->resume_from = (u32) ( (char *)start -  (char *)data);
            assert(ctx->resume_from<=pck_size);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[%s] not yet playing\n", ctx->log_name));
			return GF_OK;
		}
		if (ctx->in_seek) {
			u64 nb_frames_at_seek = (u64) (ctx->start_range * ctx->cur_fps.num);
			if (ctx->cts + ctx->cur_fps.den >= nb_frames_at_seek) {
				//u32 samples_to_discard = (ctx->cts + ctx->dts_inc) - nb_samples_at_seek;
				ctx->in_seek = GF_FALSE;
			}
		}

		if (nal_parse_result<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[%s] Error parsing NAL Unit type %d - skipping\n", ctx->log_name,  nal_type));
			assert(remain >= (s32) sc_size+next);
			start += sc_size+next;
			remain -= sc_size+next;
			continue;
		}

		if (is_islice) ctx->has_islice = GF_TRUE;

		//store all variables needed to compute POC/CTS and sample SAP and recovery info
		if (ctx->is_hevc) {

			slice_is_ref = gf_media_hevc_slice_is_IDR(ctx->hevc_state);

			recovery_point_valid = ctx->hevc_state->sei.recovery_point.valid;
			recovery_point_frame_cnt = ctx->hevc_state->sei.recovery_point.frame_cnt;
			bIntraSlice = gf_media_hevc_slice_is_intra(ctx->hevc_state);

			au_sap_type = GF_FILTER_SAP_NONE;
			if (gf_media_hevc_slice_is_IDR(ctx->hevc_state)) {
				au_sap_type = GF_FILTER_SAP_1;
			}
			else {
				switch (ctx->hevc_state->s_info.nal_unit_type) {
				case GF_HEVC_NALU_SLICE_BLA_W_LP:
				case GF_HEVC_NALU_SLICE_BLA_W_DLP:
					au_sap_type = GF_FILTER_SAP_3;
					break;
				case GF_HEVC_NALU_SLICE_BLA_N_LP:
					au_sap_type = GF_FILTER_SAP_1;
					break;
				case GF_HEVC_NALU_SLICE_CRA:
					au_sap_type = GF_FILTER_SAP_3;
					break;
				}
			}

			slice_poc = ctx->hevc_state->s_info.poc;

			/*need to store TS offsets*/
			switch (ctx->hevc_state->s_info.slice_type) {
			case GF_AVC_TYPE_B:
			case GF_AVC_TYPE2_B:
				slice_is_b = GF_TRUE;
				break;
			}

		} else {

			/*fixme - we need finer grain for priority*/
			if ((nal_type==GF_AVC_NALU_SVC_PREFIX_NALU) || (nal_type==GF_AVC_NALU_SVC_SLICE)) {
				unsigned char *p = (unsigned char *) start;
				// RefPicFlag
				avc_svc_subs_reserved |= (p[0] & 0x60) ? 0x80000000 : 0;
				// RedPicFlag TODO: not supported, would require to parse NAL unit payload
				avc_svc_subs_reserved |= (0) ? 0x40000000 : 0;
				// VclNALUnitFlag
				avc_svc_subs_reserved |= (1<=nal_type && nal_type<=5) || (nal_type==GF_AVC_NALU_SVC_PREFIX_NALU) || (nal_type==GF_AVC_NALU_SVC_SLICE) ? 0x20000000 : 0;
				// use values of IdrFlag and PriorityId directly from SVC extension header
				avc_svc_subs_reserved |= p[1] << 16;
				// use values of DependencyId and QualityId directly from SVC extension header
				avc_svc_subs_reserved |= p[2] << 8;
				// use values of TemporalId and UseRefBasePicFlag directly from SVC extension header
				avc_svc_subs_reserved |= p[3] & 0xFC;
				// StoreBaseRepFlag TODO: SVC FF mentions a store_base_rep_flag which cannot be found in SVC spec
				avc_svc_subs_reserved |= (0) ? 0x00000002 : 0;

				// priority_id (6 bits) in SVC has inverse meaning -> lower value means higher priority - invert it and scale it to 8 bits
				avc_svc_subs_priority = (63 - (p[1] & 0x3F)) << 2;

				if (nal_type==GF_AVC_NALU_SVC_PREFIX_NALU) {
                    if (ctx->svc_prefix_buffer_size) {
                        GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[%s] broken bitstream, two consecutive SVC prefix NALU without SVC slice inbetween\n", ctx->log_name));
                        ctx->svc_prefix_buffer_size = 0;
                    }

					/* remember reserved and priority value */
					ctx->svc_nalu_prefix_reserved = avc_svc_subs_reserved;
					ctx->svc_nalu_prefix_priority = avc_svc_subs_priority;

					ctx->svc_prefix_buffer_size = next;
					if (ctx->svc_prefix_buffer_size > ctx->svc_prefix_buffer_alloc) {
						ctx->svc_prefix_buffer_alloc = ctx->svc_prefix_buffer_size;
						ctx->svc_prefix_buffer = gf_realloc(ctx->svc_prefix_buffer, ctx->svc_prefix_buffer_size);
					}
					memcpy(ctx->svc_prefix_buffer, start+sc_size, ctx->svc_prefix_buffer_size);

					assert( remain >= (s32) sc_size+next );
					start += sc_size+next;
					remain -= sc_size+next;
					continue;
				}
			} else if (is_slice) {
				// RefPicFlag
				avc_svc_subs_reserved |= (start[0] & 0x60) ? 0x80000000 : 0;
				// VclNALUnitFlag
				avc_svc_subs_reserved |= (1<=nal_type && nal_type<=5) || (nal_type==GF_AVC_NALU_SVC_PREFIX_NALU) || (nal_type==GF_AVC_NALU_SVC_SLICE) ? 0x20000000 : 0;
				avc_svc_subs_priority = 0;
			}

			if (is_slice && ctx->avc_state->s_info.field_pic_flag) {
				ctx->is_paff = GF_TRUE;
				bottom_field_flag = ctx->avc_state->s_info.bottom_field_flag;
			}

			slice_is_ref = (ctx->avc_state->s_info.nal_unit_type==GF_AVC_NALU_IDR_SLICE) ? GF_TRUE : GF_FALSE;

			recovery_point_valid = ctx->avc_state->sei.recovery_point.valid;
			recovery_point_frame_cnt = ctx->avc_state->sei.recovery_point.frame_cnt;
			bIntraSlice = gf_media_avc_slice_is_intra(ctx->avc_state);

			au_sap_type = GF_FILTER_SAP_NONE;
			if (ctx->avc_state->s_info.nal_unit_type == GF_AVC_NALU_IDR_SLICE)
				au_sap_type = GF_FILTER_SAP_1;

			slice_poc = ctx->avc_state->s_info.poc;
			/*need to store TS offsets*/
			switch (ctx->avc_state->s_info.slice_type) {
			case GF_AVC_TYPE_B:
			case GF_AVC_TYPE2_B:
				slice_is_b = GF_TRUE;
				break;
			}
		}

		if (is_slice) {
			Bool first_in_au = ctx->first_slice_in_au;

			if (slice_is_ref)
				ctx->nb_idr++;
			slice_force_ref = GF_FALSE;

			/*we only indicate TRUE IDRs for sync samples (cf AVC file format spec).
			SEI recovery should be used to build sampleToGroup & RollRecovery tables*/
			if (ctx->first_slice_in_au) {
				ctx->first_slice_in_au = GF_FALSE;
				if (recovery_point_valid) {
//					if (!ctx->is_hevc) assert(ctx->avc_state->s_info.nal_unit_type!=GF_AVC_NALU_IDR_SLICE || bIntraSlice);

					ctx->sei_recovery_frame_count = recovery_point_frame_cnt;

					/*we allow to mark I-frames as sync on open-GOPs (with sei_recovery_frame_count=0) when forcing sync even when the SEI RP is not available*/
					if (!recovery_point_frame_cnt && bIntraSlice) {
						ctx->has_islice = 1;
						if (ctx->use_opengop_gdr == 1) {
							ctx->use_opengop_gdr = 2; /*avoid message flooding*/
							GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[%s] No valid SEI Recovery Point found although needed - forcing\n", ctx->log_name));
						}
					}
					if (ctx->is_hevc) {
						ctx->hevc_state->sei.recovery_point.valid = 0;
					} else {
						ctx->avc_state->sei.recovery_point.valid = 0;
					}
					if (bIntraSlice && ctx->force_sync && (ctx->sei_recovery_frame_count==0))
						slice_force_ref = GF_TRUE;
				}
				ctx->au_sap = au_sap_type;
				ctx->bottom_field_flag = bottom_field_flag;
			}

			if (slice_poc < ctx->poc_shift) {

				u32 i, count = gf_list_count(ctx->pck_queue);
				for (i=0; i<count; i++) {
					u64 dts, cts;
					GF_FilterPacket *q_pck = gf_list_get(ctx->pck_queue, i);
					assert(q_pck);
					dts = gf_filter_pck_get_dts(q_pck);
					if (dts == GF_FILTER_NO_TS) continue;
					cts = gf_filter_pck_get_cts(q_pck);
					cts += ctx->poc_shift;
					cts -= slice_poc;
					gf_filter_pck_set_cts(q_pck, cts);
				}

				ctx->poc_shift = slice_poc;
			}

			/*if #pics, compute smallest POC increase*/
			if (slice_poc != ctx->last_poc) {
				s32 pdiff = ABS(ctx->last_poc - slice_poc);

				if ((slice_poc < 0) && !ctx->last_poc)
					ctx->poc_diff = 0;

				if (!ctx->poc_diff || (ctx->poc_diff > (s32) pdiff ) ) {
					ctx->poc_diff = pdiff;
					ctx->poc_probe_done = GF_FALSE;
				} else if (first_in_au) {
					//second frame with the same poc diff, we should be able to properly recompute CTSs
					ctx->poc_probe_done = GF_TRUE;
				}
			}
			ctx->last_poc = slice_poc;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[%s] POC is %d - min poc diff %d - slice is ref %d\n", ctx->log_name, slice_poc, ctx->poc_diff, slice_is_ref));

			/*ref slice, reset poc*/
			if (slice_is_ref) {
				if (first_in_au) {
					//two consecutive IDRs, force poc_diff to 0 to force frame dispatch
					if (ctx->last_frame_is_idr) ctx->poc_diff=1;
					//new ref frame, dispatch all pending packets
					naludmx_enqueue_or_dispatch(ctx, NULL, GF_TRUE);

					ctx->max_last_poc = ctx->last_poc = ctx->max_last_b_poc = 0;
					ctx->poc_shift = 0;
					//force probing of POC diff, this will prevent dispatching frames with wrong CTS until we have a clue of min poc_diff used
					ctx->poc_probe_done = 0;
					ctx->last_frame_is_idr = GF_TRUE;
				}
			}
			/*forced ref slice*/
			else if (slice_force_ref) {
				ctx->last_frame_is_idr = GF_FALSE;
				if (first_in_au) {
					//new ref frame, dispatch all pending packets
					naludmx_enqueue_or_dispatch(ctx, NULL, GF_TRUE);

					/*adjust POC shift as sample will now be marked as sync, so we must store poc as if IDR (eg POC=0) for our CTS offset computing to be correct*/
					ctx->poc_shift = slice_poc;

					//force probing of POC diff, this will prevent dispatching frames with wrong CTS until we have a clue of min poc_diff used
					ctx->poc_probe_done = 0;
				}
			}
			/*strictly less - this is a new P slice*/
			else if (ctx->max_last_poc < ctx->last_poc) {
				ctx->max_last_b_poc = 0;
				ctx->max_last_poc = ctx->last_poc;
				ctx->last_frame_is_idr = GF_FALSE;
			}
			/*stricly greater*/
			else if (slice_is_b && (ctx->max_last_poc > ctx->last_poc)) {
				ctx->last_frame_is_idr = GF_FALSE;
				if (!ctx->max_last_b_poc) {
					ctx->max_last_b_poc = ctx->last_poc;
				}
				/*if same poc than last max, this is a B-slice*/
				else if (ctx->last_poc > ctx->max_last_b_poc) {
					ctx->max_last_b_poc = ctx->last_poc;
				}
				/*otherwise we had a B-slice reference: do nothing*/
			} else {
				ctx->last_frame_is_idr = GF_FALSE;
			}


			if (ctx->deps) {
				if (nal_ref_idc) {
					ctx->has_ref_slices = GF_TRUE;
				}
				if (!ctx->is_hevc && (ctx->avc_state->s_info.redundant_pic_cnt) ) {
					ctx->has_redundant = GF_TRUE;
				}
			}
		}

		//we skipped bytes already in store + end of start code present in packet, so the size of the first object
		//needs adjustement
		if (nal_hdr_in_store && !next_size && !full_nal_required) {
			size += nal_bytes_from_store;
		}

		if (! full_nal) {
			u8 b3 = start[remain-3];
			u8 b2 = start[remain-2];
			u8 b1 = start[remain-1];

			//we may have a startcode at the end of the packet, store it and don't dispatch the last 3 bytes !
			if (!b1 || !b2 || !b3) {
				copy_last_bytes = GF_TRUE;
				assert(size >= 3);
				size -= 3;
				ctx->bytes_in_header = 3;
			}
		}

		au_start = ctx->first_pck_in_au ? GF_FALSE : GF_TRUE;

		if (ctx->has_initial_aud) {
			u32 audelim_size = ctx->is_hevc ? 3 : 2;
			/*dst_pck = */naludmx_start_nalu(ctx, audelim_size, GF_FALSE, &au_start, &pck_data);
			memcpy(pck_data + ctx->nal_length , ctx->init_aud, audelim_size);
			ctx->has_initial_aud = GF_FALSE;
			if (ctx->subsamples) {
				naludmx_add_subsample(ctx, audelim_size, avc_svc_subs_priority, avc_svc_subs_reserved);
			}
		}
		if (ctx->sei_buffer_size) {
			//sei buffer is already nal size prefixed
			/*dst_pck = */naludmx_start_nalu(ctx, ctx->sei_buffer_size, GF_TRUE, &au_start, &pck_data);
			memcpy(pck_data, ctx->sei_buffer, ctx->sei_buffer_size);
			if (ctx->subsamples) {
				naludmx_add_subsample(ctx, ctx->sei_buffer_size - ctx->nal_length, avc_svc_subs_priority, avc_svc_subs_reserved);
			}
			ctx->sei_buffer_size = 0;
		}

		if (ctx->svc_prefix_buffer_size) {
			/*dst_pck = */naludmx_start_nalu(ctx, ctx->svc_prefix_buffer_size, GF_FALSE, &au_start, &pck_data);
			memcpy(pck_data + ctx->nal_length, ctx->svc_prefix_buffer, ctx->svc_prefix_buffer_size);
			if (ctx->subsamples) {
				naludmx_add_subsample(ctx, ctx->svc_prefix_buffer_size, ctx->svc_nalu_prefix_priority, ctx->svc_nalu_prefix_reserved);
			}
			ctx->svc_prefix_buffer_size = 0;
		}

		//nalu size field
		/*dst_pck = */naludmx_start_nalu(ctx, (u32) size, GF_FALSE, &au_start, &pck_data);
		pck_data += ctx->nal_length;

		//add subsample info before touching the size
		if (ctx->subsamples) {
			naludmx_add_subsample(ctx, (u32) size, avc_svc_subs_priority, avc_svc_subs_reserved);
		}

		//bytes come from both our store and the data packet
		if (nal_hdr_in_store) {
			memcpy(pck_data, hdr_start, nal_bytes_from_store);
			assert(size >= nal_bytes_from_store);
			size -= nal_bytes_from_store;
			if (size)
				memcpy(pck_data + nal_bytes_from_store, pck_start, (size_t) size);

			if (next_size) {
				assert(!size);
				memmove(ctx->hdr_store, hdr_start+nal_bytes_from_store,ctx->bytes_in_header - nal_bytes_from_store);
				ctx->bytes_in_header -= next_size+store_sc_size;
				continue;
			}
		} else {
			//bytes only come from the data packet
			memcpy(pck_data, pck_start, (size_t) size);
		}

		if (! full_nal) {
			if (copy_last_bytes) {
				memcpy(ctx->hdr_store, start+remain-3, 3);
			}
			break;
		}

		assert(remain >= size);
		start = pck_start + size;
        assert((u32) (start - (u8*)data) <= pck_size);
		remain = pck_size - (u32) (start - (u8*)data);


		//don't demux too much of input, abort when we would block. This avoid dispatching
		//a huge number of frames in a single call
		if (remain && gf_filter_pid_would_block(ctx->opid)) {
			ctx->resume_from = (u32) ((char *)start -  (char *)data);
			if (data == ctx->hdr_store) {
				assert(ctx->resume_from > hdr_size_at_resume);
				ctx->resume_from -= hdr_size_at_resume;
            } else {
                assert(ctx->resume_from<=pck_size);
            }
			return GF_OK;
		}
	}
	if (is_eos)
		return naludmx_process(filter);

	if ((ctx->nb_nalus>nalu_before) && gf_filter_reporting_enabled(filter)) {
		char szStatus[1024];

		sprintf(szStatus, "%s %dx%d % 10d NALU % 8d I % 8d P % 8d B % 8d SEI", ctx->is_hevc ? "HEVC":"AVC|H264", ctx->width, ctx->height, ctx->nb_nalus, ctx->nb_i, ctx->nb_p, ctx->nb_b, ctx->nb_sei);
		gf_filter_update_status(filter, -1, szStatus);
	}

	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static GF_Err naludmx_initialize(GF_Filter *filter)
{
	GF_NALUDmxCtx *ctx = gf_filter_get_udta(filter);
	ctx->hdr_store_size = 0;
	ctx->hdr_store_alloc = SAFETY_NAL_STORE;
	ctx->hdr_store = gf_malloc(sizeof(char)*SAFETY_NAL_STORE);
	ctx->sps = gf_list_new();
	ctx->pps = gf_list_new();
	switch (ctx->nal_length) {
	case 1:
		ctx->max_nalu_size_allowed = 0xFF;
		break;
	case 2:
		ctx->max_nalu_size_allowed = 0xFFFF;
		break;
	case 4:
		ctx->max_nalu_size_allowed = 0xFFFFFFFF;
		break;
	case 0:
		ctx->max_nalu_size_allowed = 0xFFFFFFFF;
		ctx->nal_length = 4;
		ctx->nal_adjusted = GF_TRUE;
		break;
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[%s] NAL size length %d is not allowed, defaulting to 4 bytes\n", ctx->log_name));
		ctx->max_nalu_size_allowed = 0xFFFFFFFF;
		ctx->nal_length = 4;
		break;
	}
	return GF_OK;
}

static void naludmx_del_param_list(GF_List *ps)
{
	if (!ps) return;
	while (gf_list_count(ps)) {
		GF_AVCConfigSlot *sl = gf_list_pop_back(ps);
		if (sl->data) gf_free(sl->data);
		gf_free(sl);
	}
	gf_list_del(ps);
}

static void naludmx_log_stats(GF_NALUDmxCtx *ctx)
{
	u32 i, count;
	u32 nb_frames = (u32) (ctx->dts / ctx->cur_fps.den);

	if (ctx->nb_si || ctx->nb_sp) {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("%s Import results: %d frames (%d NALUs) - Slices: %d I %d P %d B %d SP %d SI - %d SEI - %d IDR\n", ctx->log_name, nb_frames, ctx->nb_nalus, ctx->nb_i, ctx->nb_p, ctx->nb_b, ctx->nb_sp, ctx->nb_si, ctx->nb_sei, ctx->nb_idr ));
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("%s Import results: %d samples (%d NALUs) - Slices: %d I %d P %d B - %d SEI - %d IDR\n",
			                  ctx->log_name, nb_frames, ctx->nb_nalus, ctx->nb_i, ctx->nb_p, ctx->nb_b, ctx->nb_sei, ctx->nb_idr));
	}

	if (!ctx->is_hevc) {
		count = gf_list_count(ctx->sps);
		for (i=0; i<count; i++) {
			AVC_SPS *sps;
			GF_AVCConfigSlot *svcc = (GF_AVCConfigSlot*)gf_list_get(ctx->sps, i);
			sps = & ctx->avc_state->sps[svcc->id];
			if (sps->nb_ei || sps->nb_ep) {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("%s SVC (SSPS ID %d, %dx%d) Import results: Slices: %d I %d P %d B\n", ctx->log_name, svcc->id - GF_SVC_SSPS_ID_SHIFT, sps->width, sps->height, sps->nb_ei, sps->nb_ep, sps->nb_eb ));
			}
		}
	} else if (ctx->nb_e_i || ctx->nb_e_p || ctx->nb_e_b) {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("%s L-HEVC Import results: Slices: %d I %d P %d B\n", ctx->log_name, ctx->nb_e_i, ctx->nb_e_p, ctx->nb_e_b ));
	}

	if (ctx->max_total_delay>1) {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("%s Stream uses forward prediction - stream CTS offset: %d frames\n", ctx->log_name, ctx->max_total_delay));
	}

	if (!ctx->nal_adjusted) {
		if ((ctx->max_nalu_size < 0xFF) && (ctx->nal_length>1) ){
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("%s Max NALU size is %d - stream could be optimized by setting nal_length=1\n", ctx->log_name, ctx->max_nalu_size));
		} else if ((ctx->max_nalu_size < 0xFFFF) && (ctx->nal_length>2) ){
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("%s Max NALU size is %d - stream could be optimized by setting nal_length=2\n", ctx->log_name, ctx->max_nalu_size));
		}
	}
}

static void naludmx_finalize(GF_Filter *filter)
{
	GF_NALUDmxCtx *ctx = gf_filter_get_udta(filter);

	if (ctx->importer) naludmx_log_stats(ctx);

	if (ctx->bs_r) gf_bs_del(ctx->bs_r);
	if (ctx->bs_w) gf_bs_del(ctx->bs_w);
	if (ctx->indexes) gf_free(ctx->indexes);
	if (ctx->hdr_store) gf_free(ctx->hdr_store);
	if (ctx->pck_queue) {
		while (gf_list_count(ctx->pck_queue)) {
			GF_FilterPacket *pck = gf_list_pop_back(ctx->pck_queue);
			gf_filter_pck_discard(pck);
		}
		gf_list_del(ctx->pck_queue);
	}
	if (ctx->sei_buffer) gf_free(ctx->sei_buffer);
	if (ctx->svc_prefix_buffer) gf_free(ctx->svc_prefix_buffer);
	if (ctx->subsamp_buffer) gf_free(ctx->subsamp_buffer);

	naludmx_del_param_list(ctx->sps);
	naludmx_del_param_list(ctx->pps);
	naludmx_del_param_list(ctx->vps);
	naludmx_del_param_list(ctx->sps_ext);
	naludmx_del_param_list(ctx->pps_svc);

	if (ctx->avc_state) gf_free(ctx->avc_state);
	if (ctx->hevc_state) gf_free(ctx->hevc_state);
}


static const char *naludmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	u32 sc, sc_size;
	u32 not_hevc=0;
	u32 not_avc=0;
	u32 nb_hevc=0;
	u32 nb_avc=0;
	u32 nb_nalus=0;
	u32 nb_hevc_zero=0;
	u32 nb_avc_zero=0;
	u32 nb_sps_hevc=0,nb_pps_hevc=0,nb_vps_hevc=0;
	u32 nb_sps_avc=0,nb_pps_avc=0;

	while (size) {
		u32 avc_type=0;
		u32 hevc_type=0;
		sc = gf_media_nalu_next_start_code(data, size, &sc_size);
		if (!sc_size) break;

		data += sc + sc_size;
		if (size <= sc + sc_size) break;
		size -= sc + sc_size;

		nb_nalus++;

		hevc_type = (data[0] & 0x7E) >> 1;
		if (hevc_type<=40) {
			nb_hevc++;
			switch (hevc_type) {
			case GF_HEVC_NALU_PIC_PARAM:
				nb_pps_hevc++;
				break;
			case GF_HEVC_NALU_SEQ_PARAM:
				nb_sps_hevc++;
				break;
			case GF_HEVC_NALU_VID_PARAM:
				nb_vps_hevc++;
				break;
			case 0:
				nb_hevc_zero++;
				break;
			}
		} else {
			not_hevc++;
		}

		avc_type = data[0] & 0x1F;
		if (avc_type && avc_type<=24) {
			nb_avc++;
			switch (avc_type) {
			case GF_AVC_NALU_PIC_PARAM:
				nb_pps_avc++;
				break;
			case GF_AVC_NALU_SEQ_PARAM:
				nb_sps_avc++;
				break;
			case 0:
				nb_avc_zero++;
				break;
			}
		} else {
			not_avc++;
		}
	}
	if (!nb_sps_avc || !nb_pps_avc) nb_avc=0;
	if (!nb_sps_hevc || !nb_pps_hevc || !nb_vps_hevc) nb_hevc=0;
	if (not_avc) nb_avc=0;
	if (not_hevc) nb_hevc=0;

	if (not_avc && not_hevc) return NULL;
	if (nb_avc==nb_avc_zero) nb_avc=0;
	if (nb_hevc==nb_hevc_zero) nb_hevc=0;

	if (!nb_hevc && !nb_avc) return NULL;
	*score = GF_FPROBE_SUPPORTED;
	if (!nb_hevc) return "video/avc";
	if (!nb_avc) return "video/hevc";
	if (nb_hevc>nb_avc) return "video/hevc";
	return "video/avc";
}

static const GF_FilterCapability NALUDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "video/avc|video/h264|video/svc|video/mvc|video/hevc|video/lhvc|video/shvc|video/mhvc"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "264|h264|26L|h26L|h26l|avc|svc|mvc|hevc|hvc|265|h265|shvc|lvhc|mhvc"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
};

#define OFFS(_n)	#_n, offsetof(GF_NALUDmxCtx, _n)
static const GF_FilterArgs NALUDmxArgs[] =
{
	{ OFFS(fps), "import frame rate (0 default to FPS from bitstream or 25 Hz)", GF_PROP_FRACTION, "0/1000", NULL, 0},
	{ OFFS(index), "indexing window length. If 0, bitstream is not probed for duration. A negative value skips the indexing if the source file is larger than 100M (slows down importers) unless a play with start range > 0 is issued, otherwise uses the positive value", GF_PROP_DOUBLE, "-1.0", NULL, 0},
	{ OFFS(explicit), "use explicit layered (SVC/LHVC) import", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(strict_poc), "delay frame output of an entire GOP to ensure CTS info is correct when POC suddenly changes", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(nosei), "remove all sei messages", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(nosvc), "remove all SVC/MVC/LHVC data", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(novpsext), "remove all VPS extensions", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(importer), "compatibility with old importer, displays import results", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(nal_length), "set number of bytes used to code length field: 1, 2 or 4", GF_PROP_UINT, "4", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(subsamples), "import subsamples information", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(deps), "import samples dependencies information", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(seirw), "rewrite AVC sei messages for ISOBMFF constraints", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(audelim), "keep Access Unit delimiter in payload", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(analyze), "skip reformat of decoder config and SEI and dispatch all NAL in input order - shall only be used with inspect filter analyze mode!", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_HIDE},
	{0}
};


GF_FilterRegister NALUDmxRegister = {
	.name = "rfnalu",
	GF_FS_SET_DESCRIPTION("AVC/HEVC reframer")
	GF_FS_SET_HELP("This filter parses AVC|H264 and HEVC files/data and outputs corresponding video PID and frames.\n"
	"This demuxer only produces ISOBMFF-compatible output: start codes are removed, NALU length field added and avcC/hvcC config created.\nNote: The demux uses negative CTS offsets: CTS is corrrect, but some frames may have DTS > CTS.")
	.private_size = sizeof(GF_NALUDmxCtx),
	.args = NALUDmxArgs,
	.initialize = naludmx_initialize,
	.finalize = naludmx_finalize,
	SETCAPS(NALUDmxCaps),
	.configure_pid = naludmx_configure_pid,
	.process = naludmx_process,
	.process_event = naludmx_process_event,
	.probe_data = naludmx_probe_data,
};


const GF_FilterRegister *naludmx_register(GF_FilterSession *session)
{
	return &NALUDmxRegister;
}

