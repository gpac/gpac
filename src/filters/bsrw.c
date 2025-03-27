/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2020-2024
 *					All rights reserved
 *
 *  This file is part of GPAC / compressed bitstream metadata rewrite filter
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

#ifndef GPAC_DISABLE_BSRW

typedef struct _bsrw_pid_ctx BSRWPid;
typedef struct _bsrw_ctx GF_BSRWCtx;

GF_OPT_ENUM (BsrwTimecodeMode,
	BSRW_TC_NONE=0,
	BSRW_TC_REMOVE,
	BSRW_TC_INSERT,
	BSRW_TC_SHIFT,
	BSRW_TC_CONSTANT
);

struct _bsrw_pid_ctx
{
	GF_FilterPid *ipid, *opid;
	u32 codec_id;
	Bool reconfigure;
	GF_Err (*rewrite_pid_config)(GF_BSRWCtx *ctx, BSRWPid *pctx);
	GF_Err (*rewrite_packet)(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck);

	s32 prev_cprim, prev_ctfc, prev_cmx, prev_sar;
	GF_Fraction fps;

	u32 nalu_size_length;

#ifndef GPAC_DISABLE_AV_PARSERS
	GF_VUIInfo vui;
	AVCState *avc;
#endif
	Bool rewrite_vui;
};

struct _bsrw_ctx
{
	GF_Fraction sar;
	s32 m4vpl, prof, lev, pcomp, pidc, pspace, gpcflags;
	s32 cprim, ctfc, cmx, vidfmt;
	Bool rmsei, fullrange, novsi, novuitiming;
	GF_PropUIntList seis;

	GF_List *pids;
	Bool reconfigure;

	Bool tcsc_inferred;
	char *tcxs, *tcxe, *tcsc;
	GF_TimeCode tcxs_val, tcxe_val, tcsc_val;
	BsrwTimecodeMode tc;
};

static GF_Err none_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck)
{
	return gf_filter_pck_forward(pck, pctx->opid);
}

static GF_Err m4v_rewrite_pid_config(GF_BSRWCtx *ctx, BSRWPid *pctx)
{
	GF_Err e;
	u8 *dsi;
	u32 dsi_size;
	const GF_PropertyValue *prop;

	prop = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DECODER_CONFIG);
	if (!prop) return GF_OK;

	pctx->reconfigure = GF_FALSE;


	dsi_size = prop->value.data.size;
	dsi = gf_malloc(sizeof(u8) * dsi_size);
	memcpy(dsi, prop->value.data.ptr, sizeof(u8) * dsi_size);

	if (ctx->sar.num && ctx->sar.den) {
#ifndef GPAC_DISABLE_AV_PARSERS
		e = gf_m4v_rewrite_par(&dsi, &dsi_size, ctx->sar.num, ctx->sar.den);
#else
		e = GF_NOT_SUPPORTED;
#endif
		if (e) {
			gf_free(dsi);
			return e;
		}
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_SAR, &PROP_FRAC(ctx->sar) );
	}
	if (ctx->m4vpl>=0) {
#ifndef GPAC_DISABLE_AV_PARSERS
		gf_m4v_rewrite_pl(&dsi, &dsi_size, (u32) ctx->m4vpl);
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_PROFILE_LEVEL, &PROP_UINT(ctx->m4vpl) );
#else
		return GF_NOT_SUPPORTED;
#endif
	}

	gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );

	return GF_OK;
}

static Bool bsrw_manipulate_tc(GF_FilterPacket *pck, GF_BSRWCtx *ctx, BSRWPid *pctx, GF_TimeCode *tc_in, GF_TimeCode *tc_out)
{
	gf_assert(tc_out);
	if (ctx->tc == BSRW_TC_NONE) return GF_FALSE;

	//check if we are infering `tcsc` from the first timecode
	if (ctx->tcsc && strstr(ctx->tcsc, "first") && !ctx->tcsc_inferred) {
		if (!tc_in) return GF_FALSE;
		memcpy(&ctx->tcsc_val, tc_in, sizeof(GF_TimeCode));
		ctx->tcsc_inferred = GF_TRUE;
	}

	//get the current timecode components
	u64 cts = gf_timestamp_rescale(gf_filter_pck_get_cts(pck), pctx->fps.den * gf_filter_pck_get_timescale(pck), pctx->fps.num);
	u16 n_frames = (cts * pctx->fps.den) % pctx->fps.num;
	n_frames /= pctx->fps.den;
	cts = cts * pctx->fps.den / pctx->fps.num;
	u8 seconds = cts % 60;
	cts /= 60;
	u8 minutes = cts % 60;
	cts /= 60;
	u8 hours = (u8) cts;
	u32 as_timestamp = hours*3600 + minutes*60 + seconds;
	as_timestamp *= 1000;
	as_timestamp += n_frames;

	//get the current timecode
	GF_TimeCode now = {0};
	if (!tc_in) {
		now.n_frames = n_frames;
		now.seconds = seconds;
		now.minutes = minutes;
		now.hours = hours;
		now.as_timestamp = as_timestamp;
	} else {
		memcpy(&now, tc_in, sizeof(GF_TimeCode));
	}

	//check if we are within the timecode manipulation range
	Bool tc_change = GF_TRUE;
	if (ctx->tcxs && gf_timecode_less(&now, &ctx->tcxs_val))
		tc_change = GF_FALSE;
	if (ctx->tcxe && gf_timecode_greater(&now, &ctx->tcxe_val))
		tc_change = GF_FALSE;
	if (!tc_change) return GF_FALSE;

	//check few more constraints
	if (ctx->tc == BSRW_TC_SHIFT || ctx->tc == BSRW_TC_CONSTANT) {
		if (!tc_in) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[BSRW] Cannot shift timecode without input timecode\n"));
			return GF_FALSE;
		}
	}

	//apply the timecode manipulation
	switch (ctx->tc)
	{
	case BSRW_TC_REMOVE:
		memset(tc_out, 0, sizeof(GF_TimeCode));
		break;
	case BSRW_TC_INSERT:
		if (!ctx->tcsc_inferred) {
			tc_out->n_frames = n_frames;
			tc_out->seconds = seconds;
			tc_out->minutes = minutes;
			tc_out->hours = hours;
			tc_out->as_timestamp = as_timestamp;
			break;
		} else {
			//reset now, we will overwrite what we have
			now.n_frames = n_frames;
			now.seconds = seconds;
			now.minutes = minutes;
			now.hours = hours;
			now.as_timestamp = as_timestamp;
		}
		//fallthrough
	case BSRW_TC_SHIFT: {
		// Handle rollover for frames first
		s32 frame_adjustment = ctx->tcsc_val.negative ? -ctx->tcsc_val.n_frames : ctx->tcsc_val.n_frames;
		s32 total_frames = now.n_frames + frame_adjustment;
		tc_out->n_frames = (total_frames + pctx->fps.num) % pctx->fps.num;
		s32 second_carry = total_frames / pctx->fps.num;
		if (total_frames < 0 && total_frames % pctx->fps.num != 0) {
			second_carry--;
		}

		// Handle rollover for seconds
		s32 second_adjustment = ctx->tcsc_val.negative ? -ctx->tcsc_val.seconds : ctx->tcsc_val.seconds;
		s32 total_seconds = now.seconds + second_adjustment + second_carry;
		tc_out->seconds = (total_seconds + 60) % 60;
		s32 minute_carry = total_seconds / 60;
		if (total_seconds < 0 && total_seconds % 60 != 0) {
			minute_carry--;
		}

		// Handle rollover for minutes
		s32 minute_adjustment = ctx->tcsc_val.negative ? -ctx->tcsc_val.minutes : ctx->tcsc_val.minutes;
		s32 total_minutes = now.minutes + minute_adjustment + minute_carry;
		tc_out->minutes = (total_minutes + 60) % 60;
		s32 hour_carry = total_minutes / 60;
		if (total_minutes < 0 && total_minutes % 60 != 0) {
			hour_carry--;
		}

		// Handle rollover for hours (assuming 24-hour format)
		s32 hour_adjustment = ctx->tcsc_val.negative ? -ctx->tcsc_val.hours : ctx->tcsc_val.hours;
		s32 total_hours = now.hours + hour_adjustment + hour_carry;
		tc_out->hours = (total_hours + 24) % 24;

		// Calculate the timestamp
		tc_out->as_timestamp = (tc_out->hours * 3600 + tc_out->minutes * 60 + tc_out->seconds) * 1000 + tc_out->n_frames;
		break;
	}
	case BSRW_TC_CONSTANT:
		tc_out->n_frames = ctx->tcsc_val.n_frames;
		tc_out->seconds = ctx->tcsc_val.seconds;
		tc_out->minutes = ctx->tcsc_val.minutes;
		tc_out->hours = ctx->tcsc_val.hours;
		tc_out->as_timestamp = ctx->tcsc_val.as_timestamp;
		break;

	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[BSRW] Unsupported timecode mode\n"));
		return GF_FALSE;
	}

	return GF_TRUE;
}

static GF_Err nalu_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck, u32 codec_type)
{
	Bool is_sei = GF_FALSE;
	GF_TimeCode *tc_in = NULL;
	u8 *output;
	u32 pck_size;
	const u8 *data = gf_filter_pck_get_data(pck, &pck_size);
	if (!data)
		return gf_filter_pck_forward(pck, pctx->opid);

	GF_BitStream *bs = gf_bs_new(data, pck_size, GF_BITSTREAM_READ);
	if (!bs) return GF_OUT_OF_MEM;

	while (gf_bs_available(bs)) {
		u8 nal_type=0;
		u32 nal_size = gf_bs_read_int(bs, 8*pctx->nalu_size_length);
		u64 pos = gf_bs_get_position(bs);
		//AVC
		if (codec_type==0) {
			//populate the avc state
			GF_BitStream *bs_nal = gf_bs_new(data + pos, nal_size, GF_BITSTREAM_READ);
			s32 res = gf_avc_parse_nalu(bs_nal, pctx->avc);
			gf_bs_del(bs_nal);
			if (res < 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[BSRW] failed to parse AVC NALU\n"));
				gf_bs_del(bs);
				return GF_NON_COMPLIANT_BITSTREAM;
			}

			// Check if the NALU is a SEI
			gf_bs_seek(bs, pos);
			nal_type = gf_bs_read_u8(bs) & 0x1F;
			if (nal_type == GF_AVC_NALU_SEI)
				is_sei = GF_TRUE;
			else goto skip;

			//parse the sei
			u8* nal = gf_malloc(nal_size);
			gf_bs_seek(bs, pos);
			gf_bs_read_data(bs, nal, nal_size);
			memset(&pctx->avc->sei.pic_timing, 0, sizeof(AVCSeiPicTiming));
			gf_avc_reformat_sei(nal, nal_size, GF_FALSE, pctx->avc, NULL);
			gf_free(nal);

			AVCSeiPicTimingTimecode *avc_tc = &pctx->avc->sei.pic_timing.timecodes[0];
			if (avc_tc->clock_timestamp_flag) {
				if (tc_in) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[BSRW] Multiple timecodes in the same AU, ignoring\n"));
					goto skip;
				}

				GF_SAFEALLOC(tc_in, GF_TimeCode);
				tc_in->hours = avc_tc->hours;
				tc_in->minutes = avc_tc->minutes;
				tc_in->seconds = avc_tc->seconds;
				tc_in->n_frames = avc_tc->n_frames;
			}
		}
		//HEVC
		else if (codec_type==1) {
			nal_type = (gf_bs_read_u8(bs) & 0x7E) >> 1;
			if ((nal_type == GF_HEVC_NALU_SEI_PREFIX) || (nal_type == GF_HEVC_NALU_SEI_SUFFIX))
				is_sei = GF_TRUE;
			else goto skip;

			//parse the sei
			HEVCState *hevc;
			GF_SAFEALLOC(hevc, HEVCState);
			u8* nal = gf_malloc(nal_size);
			gf_bs_seek(bs, pos);
			gf_bs_read_data(bs, nal, nal_size);
			gf_hevc_parse_sei(nal, nal_size, hevc);
			gf_free(nal);

			AVCSeiPicTimingTimecode *hevc_tc = &hevc->sei.pic_timing.timecodes[0];
			if (hevc_tc->clock_timestamp_flag) {
				if (tc_in) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[BSRW] Multiple timecodes in the same AU, ignoring\n"));
					gf_free(hevc);
					goto skip;
				}

				GF_SAFEALLOC(tc_in, GF_TimeCode);
				tc_in->hours = hevc_tc->hours;
				tc_in->minutes = hevc_tc->minutes;
				tc_in->seconds = hevc_tc->seconds;
				tc_in->n_frames = hevc_tc->n_frames;
				gf_free(hevc);
			}
		}
		//VVC
		else if (codec_type==2) {
			gf_bs_skip_bytes(bs, 1);
			nal_type = gf_bs_read_u8(bs) >> 3;
			if ((nal_type == GF_VVC_NALU_SEI_PREFIX) || (nal_type == GF_VVC_NALU_SEI_SUFFIX))
				is_sei = GF_TRUE;
		}

	skip:
		gf_bs_seek(bs, pos + nal_size);
	}
	if (!is_sei && ctx->tc <= BSRW_TC_REMOVE) {
		gf_bs_del(bs);
		return gf_filter_pck_forward(pck, pctx->opid);
	}

#ifdef GPAC_DISABLE_AV_PARSERS
	gf_bs_del(bs);
	return GF_NOT_SUPPORTED;
#endif

	u32 tc_sei_type = codec_type == 0 ? 1 : 136;
	SEI_Filter sei_filter = {
		.is_whitelist = !ctx->rmsei,
		.seis = ctx->seis,
		.extra_filter = 0
	};

	GF_BitStream *bs_w = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (!bs_w) {
		gf_bs_del(bs);
		return GF_OUT_OF_MEM;
	}

	GF_TimeCode tc_out;
	Bool tc_change = bsrw_manipulate_tc(pck, ctx, pctx, tc_in, &tc_out);
	sei_filter.extra_filter = tc_change ? -tc_sei_type : 0;
	if (tc_change && ctx->tc > BSRW_TC_REMOVE) {
		gf_bs_write_int(bs_w, 0, 8*pctx->nalu_size_length);

		if (codec_type == 0) {
			gf_bs_write_int(bs_w, 0, 1);
			gf_bs_write_int(bs_w, 0, 2);
			gf_bs_write_int(bs_w, GF_AVC_NALU_SEI, 5);
		} else if (codec_type == 1) {
			gf_bs_write_int(bs_w, 0, 1);
			gf_bs_write_int(bs_w, GF_HEVC_NALU_SEI_PREFIX, 6);
			gf_bs_write_int(bs_w, 0, 6);
			gf_bs_write_int(bs_w, 1, 3);
		}

		//write SEI type
		gf_bs_write_int(bs_w, tc_sei_type, 8);

		//save position for size
		u64 size_pos = gf_bs_get_position(bs_w);
		gf_bs_write_int(bs_w, 0, 8);

		if (codec_type == 0) {
			int sps_id = pctx->avc->sps_active_idx;
			AVC_SPS *sps = &pctx->avc->sps[sps_id];
			if (sps->vui.nal_hrd_parameters_present_flag || sps->vui.vcl_hrd_parameters_present_flag) {
				gf_bs_write_int(bs_w, 0, 1 + sps->vui.hrd.cpb_removal_delay_length_minus1);
				gf_bs_write_int(bs_w, 0, 1 + sps->vui.hrd.dpb_output_delay_length_minus1);
			}
			gf_bs_write_int(bs_w, 0/*pic_struct*/, 4);
			gf_bs_write_int(bs_w, 1/*clock_timestamp_flag*/, 1);
			gf_bs_write_int(bs_w, 0/*ct_type*/, 2);
			gf_bs_write_int(bs_w, 0/*nuit_field_based_flag*/, 1);
			gf_bs_write_int(bs_w, 0/*counting_type*/, 5);
			gf_bs_write_int(bs_w, 1/*full_timestamp_flag*/, 1);
			gf_bs_write_int(bs_w, 0/*discontinuity_flag*/, 1);
			gf_bs_write_int(bs_w, 0/*cnt_dropped_flag*/, 1);
			gf_bs_write_int(bs_w, tc_out.n_frames, 8);
			gf_bs_write_int(bs_w, tc_out.seconds, 6);
			gf_bs_write_int(bs_w, tc_out.minutes, 6);
			gf_bs_write_int(bs_w, tc_out.hours, 5);
		} else {
			gf_bs_write_int(bs_w, 1/*num_clock_ts*/, 2);
			gf_bs_write_int(bs_w, 1/*clock_timestamp_flag*/, 1);
			gf_bs_write_int(bs_w, 0/*units_field_based_flag*/, 1);
			gf_bs_write_int(bs_w, 0/*counting_type*/, 5);
			gf_bs_write_int(bs_w, 1/*full_timestamp_flag*/, 1);
			gf_bs_write_int(bs_w, 0/*discontinuity_flag*/, 1);
			gf_bs_write_int(bs_w, 0/*cnt_dropped_flag*/, 1);
			gf_bs_write_int(bs_w, tc_out.n_frames, 9);
			gf_bs_write_int(bs_w, tc_out.seconds, 6);
			gf_bs_write_int(bs_w, tc_out.minutes, 6);
			gf_bs_write_int(bs_w, tc_out.hours, 5);
			gf_bs_write_int(bs_w, 0/*time_offset_length*/, 5);
		}

		gf_bs_write_int(bs_w, 0x80, 8);
		gf_bs_align(bs_w);

		//write the SEI size
		u64 pos = gf_bs_get_position(bs_w);
		u32 sei_size = pos - size_pos - 1;
		gf_bs_seek(bs_w, size_pos);
		gf_bs_write_int(bs_w, sei_size, 8);

		//write the NAL size
		u32 nal_size = pos - pctx->nalu_size_length;
		gf_bs_seek(bs_w, 0);
		gf_bs_write_int(bs_w, nal_size, 8*pctx->nalu_size_length);
		gf_bs_seek(bs_w, pos);
	}

	u32 rw_sei_size = 0;
	u8 *rw_sei_payload = NULL;
	gf_bs_seek(bs, 0);
	while (gf_bs_available(bs)) {
		u8 nal_type=0;
		u32 nal_size = gf_bs_read_int(bs, 8*pctx->nalu_size_length);
		u64 payload_pos = gf_bs_get_position(bs);
		is_sei = GF_FALSE;

		//AVC
		if (codec_type==0) {
			nal_type = gf_bs_read_u8(bs) & 0x1F;
			if (nal_type == GF_AVC_NALU_SEI) is_sei = GF_TRUE;
		}
		//HEVC
		else if (codec_type==1) {
			nal_type = (gf_bs_read_u8(bs) & 0x7E) >> 1;
			if ((nal_type == GF_HEVC_NALU_SEI_PREFIX) || (nal_type == GF_HEVC_NALU_SEI_SUFFIX))
				is_sei = GF_TRUE;
		}
		//VVC
		else if (codec_type==2) {
			gf_bs_skip_bytes(bs, 1);
			nal_type = gf_bs_read_u8(bs) >> 3;
			if ((nal_type == GF_VVC_NALU_SEI_PREFIX) || (nal_type == GF_VVC_NALU_SEI_SUFFIX))
				is_sei = GF_TRUE;
		}

		//allocate the temporary storage
		rw_sei_size = nal_size;
		if (rw_sei_payload) rw_sei_payload = gf_realloc(rw_sei_payload, rw_sei_size);
		else rw_sei_payload = gf_malloc(rw_sei_size);

		//copy the NAL payload
		gf_bs_seek(bs, payload_pos);
		gf_bs_read_data(bs, rw_sei_payload, rw_sei_size);

		//reformat the SEI
		if (is_sei) {
			switch (codec_type)
			{
			case 0:
				rw_sei_size = gf_avc_reformat_sei(rw_sei_payload, rw_sei_size, GF_TRUE, NULL, &sei_filter);
				break;
			case 1:
				rw_sei_size = gf_hevc_reformat_sei(rw_sei_payload, rw_sei_size, GF_TRUE, &sei_filter);
				break;
			case 2:
				rw_sei_size = gf_vvc_reformat_sei(rw_sei_payload, rw_sei_size, GF_TRUE, &sei_filter);
				break;
			default:
				break;
			}
		}

		// write the new NAL
		if (rw_sei_size) {
			gf_bs_write_int(bs_w, rw_sei_size, 8*pctx->nalu_size_length);
			gf_bs_write_data(bs_w, rw_sei_payload, rw_sei_size);
		}
	}

	pck_size = gf_bs_get_position(bs_w);
	GF_FilterPacket *dst = gf_filter_pck_new_alloc(pctx->opid, pck_size, &output);
	if (!dst) return GF_OUT_OF_MEM;
	gf_filter_pck_merge_properties(pck, dst);

	if (tc_change) {
		if (ctx->tc == BSRW_TC_REMOVE)
			gf_filter_pck_set_property(dst, GF_PROP_PCK_TIMECODES, NULL);
		else
			gf_filter_pck_set_property(dst, GF_PROP_PCK_TIMECODES, &PROP_DATA((u8*)&tc_out, sizeof(GF_TimeCode)));
	}

	//copy the new data
	gf_bs_seek(bs_w, 0);
	gf_bs_read_data(bs_w, output, pck_size);

	//cleanup
	gf_free(rw_sei_payload);
	gf_bs_del(bs_w);
	gf_bs_del(bs);

	return gf_filter_pck_send(dst);
}

static GF_Err avc_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck)
{
	if (ctx->tc == BSRW_TC_INSERT) {
	#ifdef GPAC_DISABLE_AV_PARSERS
		return GF_NOT_SUPPORTED;
	#endif

		//parse the sps/pps
		if (pctx->avc)
			goto finish;

		const GF_PropertyValue *prop = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DECODER_CONFIG);
		if (!prop) return GF_NOT_SUPPORTED;

		GF_AVCConfig *avcc = gf_odf_avc_cfg_read(prop->value.data.ptr, prop->value.data.size);
		if (!avcc) return GF_NOT_SUPPORTED;

		GF_SAFEALLOC(pctx->avc, AVCState);
		for (u32 i=0; i<gf_list_count(avcc->sequenceParameterSets); ++i) {
			GF_NALUFFParam *slc = gf_list_get(avcc->sequenceParameterSets, i);
			s32 idx = gf_avc_read_sps(slc->data, slc->size, pctx->avc, 0, NULL);
			if (idx < 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[BSRW] failed to parse AVC SPS\n"));
				gf_odf_avc_cfg_del(avcc);
				return GF_NOT_SUPPORTED;
			}
		}
		for (u32 i=0; i<gf_list_count(avcc->pictureParameterSets); ++i) {
			GF_NALUFFParam *slc = gf_list_get(avcc->pictureParameterSets, i);
			s32 idx = gf_avc_read_pps(slc->data, slc->size, pctx->avc);
			if (idx < 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[BSRW] failed to parse AVC PPS\n"));
				gf_odf_avc_cfg_del(avcc);
				return GF_NOT_SUPPORTED;
			}
		}

		gf_odf_avc_cfg_del(avcc);
	}

finish:
	return nalu_rewrite_packet(ctx, pctx, pck, 0);
}
static GF_Err hevc_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck)
{
	return nalu_rewrite_packet(ctx, pctx, pck, 1);
}
static GF_Err vvc_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck)
{
	return nalu_rewrite_packet(ctx, pctx, pck, 2);
}

static GF_Err av1_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck)
{
	u32 pck_size;
	const u8 *data = gf_filter_pck_get_data(pck, &pck_size);

	//for the time being, we only support timecode manipulation
	if (!data || !ctx->tc)
		return gf_filter_pck_forward(pck, pctx->opid);

#ifdef GPAC_DISABLE_AV_PARSERS
	return GF_NOT_SUPPORTED;
#endif

	GF_BitStream *bs = gf_bs_new(data, pck_size, GF_BITSTREAM_READ);
	if (!bs) return GF_OUT_OF_MEM;

	//probe data
	if (gf_media_probe_iamf(bs) || gf_media_probe_ivf(bs) || gf_media_aom_probe_annexb(bs)) {
		gf_bs_del(bs);
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[BSRW] Timecode manipulation is only supported for AV1 Section 5 bitstreams\n"));
		return gf_filter_pck_forward(pck, pctx->opid);
	}

	//look for existing timecode
	GF_TimeCode *tc_in = NULL;
	while (gf_bs_available(bs)) {
		u64 pos = gf_bs_get_position(bs);

		//read header
		ObuType obu_type = OBU_RESERVED_0;
		Bool obu_extension_flag = GF_FALSE, obu_has_size_field = GF_FALSE;
		u8 tid = 0, sid = 0;
		GF_Err e = gf_av1_parse_obu_header(bs, &obu_type, &obu_extension_flag, &obu_has_size_field, &tid, &sid);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[BSRW] Error parsing AV1 OBU header, forwarding packet\n"));
			gf_bs_del(bs);
			return gf_filter_pck_forward(pck, pctx->opid);
		}

		// read size
		u64 obu_size = pck_size;
		if (obu_has_size_field) {
			obu_size = gf_av1_leb128_read(bs, NULL);
			if (obu_size > pck_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[BSRW] OBU size exceeds packet size, forwarding packet\n"));
				gf_bs_del(bs);
				return gf_filter_pck_forward(pck, pctx->opid);
			}
		}
		u32 hdr_size = (u32)(gf_bs_get_position(bs) - pos);

		//check if timecode metadata
		Bool is_metadata = obu_type == OBU_METADATA;
		if (!is_metadata) goto skip;
		u64 metadata_type = gf_av1_leb128_read(bs, NULL);
		if (metadata_type != OBU_METADATA_TYPE_TIMECODE) goto skip;

		//allocate the timecode
		if (tc_in) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[BSRW] Multiple timecodes in the same TU, ignoring\n"));
			goto skip;
		}
		GF_SAFEALLOC(tc_in, GF_TimeCode);

		//parse timecode metadata
		gf_bs_read_int(bs, 5); //counting_type
		Bool full_timestamp_flag = gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 1); //discontinuity_flag
		gf_bs_read_int(bs, 1); //cnt_dropped_flag
		tc_in->n_frames = gf_bs_read_int(bs, 9);

		if (full_timestamp_flag) {
			tc_in->seconds = gf_bs_read_int(bs, 6);
			tc_in->minutes = gf_bs_read_int(bs, 6);
			tc_in->hours = gf_bs_read_int(bs, 5);
		} else {
			Bool seconds_flag = gf_bs_read_int(bs, 1);
			if (seconds_flag) {
				tc_in->seconds = gf_bs_read_int(bs, 6);
				Bool minutes_flag = gf_bs_read_int(bs, 1);
				if (minutes_flag) {
					tc_in->minutes = gf_bs_read_int(bs, 6);
					Bool hours_flag = gf_bs_read_int(bs, 1);
					if (hours_flag) {
						tc_in->hours = gf_bs_read_int(bs, 5);
					}
				}
			}
		}
		//skip rest of the metadata

	skip:
		gf_bs_seek(bs, pos + hdr_size + obu_size);
	}

	GF_TimeCode tc_out;
	Bool tc_change = bsrw_manipulate_tc(pck, ctx, pctx, tc_in, &tc_out);
	if (!tc_change) {
		gf_bs_del(bs);
		return gf_filter_pck_forward(pck, pctx->opid);
	}

	GF_BitStream *bs_w = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (!bs_w) {
		gf_bs_del(bs);
		return GF_OUT_OF_MEM;
	}

	//insert timecode metadata
	if (ctx->tc > BSRW_TC_REMOVE) {
		gf_bs_write_u8(bs_w, 0x2a);
		gf_av1_leb128_write(bs_w, 6/*8+39 bits*/);
		gf_av1_leb128_write(bs_w, OBU_METADATA_TYPE_TIMECODE);
		gf_bs_write_int(bs_w, 0/*counting_type*/, 5);
		gf_bs_write_int(bs_w, 1/*full_timestamp_flag*/, 1);
		gf_bs_write_int(bs_w, 0/*discontinuity_flag*/, 1);
		gf_bs_write_int(bs_w, 0/*cnt_dropped_flag*/, 1);
		gf_bs_write_int(bs_w, tc_out.n_frames, 9);
		gf_bs_write_int(bs_w, tc_out.seconds, 6);
		gf_bs_write_int(bs_w, tc_out.minutes, 6);
		gf_bs_write_int(bs_w, tc_out.hours, 5);
		gf_bs_write_int(bs_w, 0/*time_offset_length*/, 5);
		gf_bs_align(bs_w);
	}

	//remove existing timecode metadata
	gf_bs_seek(bs, 0);
	while (gf_bs_available(bs)) {
		u64 to_copy;
		u64 pos = gf_bs_get_position(bs);

		//read header
		ObuType obu_type = OBU_RESERVED_0;
		Bool obu_extension_flag = GF_FALSE, obu_has_size_field = GF_FALSE;
		u8 tid = 0, sid = 0;
		GF_Err e = gf_av1_parse_obu_header(bs, &obu_type, &obu_extension_flag, &obu_has_size_field, &tid, &sid);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[BSRW] Error parsing AV1 OBU header, forwarding packet\n"));
			gf_bs_del(bs_w);
			gf_bs_del(bs);
			return gf_filter_pck_forward(pck, pctx->opid);
		}

		// read size
		u64 obu_size = pck_size;
		if (obu_has_size_field) {
			obu_size = gf_av1_leb128_read(bs, NULL);
			if (obu_size > pck_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[BSRW] OBU size exceeds packet size, forwarding packet\n"));
				gf_bs_del(bs_w);
				gf_bs_del(bs);
				return gf_filter_pck_forward(pck, pctx->opid);
			}
		}
		u32 hdr_size = (u32)(gf_bs_get_position(bs) - pos);

		//check if timecode metadata
		Bool is_metadata = obu_type == OBU_METADATA;
		if (!is_metadata) goto transfer;
		u64 metadata_type = gf_av1_leb128_read(bs, NULL);
		if (metadata_type != OBU_METADATA_TYPE_TIMECODE) goto transfer;

		//skip timecode metadata
		gf_bs_seek(bs, pos + hdr_size + obu_size);
		continue;

	transfer:
		gf_bs_seek(bs, pos);
		to_copy = hdr_size + obu_size;
		while (to_copy--) {
			u32 byte = gf_bs_read_u8(bs);
			gf_bs_write_u8(bs_w, byte);
		}
	}

	//send packet
	u8* output = NULL;
	pck_size = gf_bs_get_position(bs_w);
	GF_FilterPacket *dst = gf_filter_pck_new_alloc(pctx->opid, pck_size, &output);
	gf_filter_pck_merge_properties(pck, dst);

	if (ctx->tc == BSRW_TC_REMOVE)
		gf_filter_pck_set_property(dst, GF_PROP_PCK_TIMECODES, NULL);
	else
		gf_filter_pck_set_property(dst, GF_PROP_PCK_TIMECODES, &PROP_DATA((u8*)&tc_out, sizeof(GF_TimeCode)));

	//copy the new data
	gf_bs_seek(bs_w, 0);
	gf_bs_read_data(bs_w, output, pck_size);

	//cleanup
	gf_bs_del(bs_w);
	gf_bs_del(bs);

	return gf_filter_pck_send(dst);
}

#ifndef GPAC_DISABLE_AV_PARSERS
static void update_props(BSRWPid *pctx, GF_VUIInfo *vui)
{
	if ((vui->ar_num>0) && (vui->ar_den>0))
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_SAR, &PROP_FRAC_INT(vui->ar_num, vui->ar_den) );

	if (vui->fullrange>0)
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_RANGE, &PROP_BOOL(vui->fullrange) );

	if (!vui->remove_video_info) {
		if (vui->color_prim>=0)
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_PRIMARIES, &PROP_UINT(vui->color_prim) );
		if (vui->color_tfc>=0)
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_TRANSFER, &PROP_UINT(vui->color_tfc) );
		if (vui->color_matrix>=0)
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_MX, &PROP_UINT(vui->color_matrix) );
	}
}
#endif /*GPAC_DISABLE_AV_PARSERS*/

static GF_Err avc_rewrite_pid_config(GF_BSRWCtx *ctx, BSRWPid *pctx)
{
	GF_AVCConfig *avcc;
	GF_Err e = GF_OK;
	u8 *dsi;
	u32 dsi_size;
	const GF_PropertyValue *prop;

	prop = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DECODER_CONFIG);
	if (!prop) return GF_OK;
	pctx->reconfigure = GF_FALSE;

	avcc = gf_odf_avc_cfg_read(prop->value.data.ptr, prop->value.data.size);

	if (pctx->rewrite_vui) {
#ifndef GPAC_DISABLE_AV_PARSERS
		GF_VUIInfo tmp_vui = pctx->vui;
		tmp_vui.update = GF_TRUE;
		e = gf_avc_change_vui(avcc, &tmp_vui);
		if (!e) {
			update_props(pctx, &tmp_vui);
		}
#else
		return GF_NOT_SUPPORTED;
#endif /*GPAC_DISABLE_AV_PARSERS*/
	}

	if ((ctx->lev>=0) || (ctx->prof>=0) || (ctx->pcomp>=0)) {
		u32 i, count = gf_list_count(avcc->sequenceParameterSets);
		for (i=0; i<count; i++) {
			GF_NALUFFParam *sps = gf_list_get(avcc->sequenceParameterSets, i);
			//first byte is nalu header, then profile_idc (8bits), prof_comp (8bits), and level_idc (8bits)
			if (ctx->prof>=0) {
				sps->data[1] = (u8) ctx->prof;
			}
			if (ctx->pcomp>=0) {
				sps->data[2] = (u8) ctx->pcomp;
			}
			if (ctx->lev>=0) {
				sps->data[3] = (u8) ctx->lev;
			}
		}
		if (ctx->lev>=0) avcc->AVCLevelIndication = ctx->lev;
		if (ctx->prof>=0) avcc->AVCProfileIndication = ctx->prof;
		if (ctx->pcomp>=0) avcc->profile_compatibility = ctx->pcomp;
	}

	gf_odf_avc_cfg_write(avcc, &dsi, &dsi_size);
	pctx->nalu_size_length = avcc->nal_unit_size;

	gf_odf_avc_cfg_del(avcc);
	if (e) return e;

	gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );

	if (ctx->rmsei || ctx->seis.nb_items || ctx->tc) {
		pctx->rewrite_packet = avc_rewrite_packet;
	} else {
		pctx->rewrite_packet = none_rewrite_packet;
	}
	return GF_OK;
}

static GF_Err hevc_rewrite_pid_config(GF_BSRWCtx *ctx, BSRWPid *pctx)
{
	GF_HEVCConfig *hvcc;
	GF_Err e = GF_OK;
	u8 *dsi;
	u32 dsi_size;
	const GF_PropertyValue *prop;

	prop = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DECODER_CONFIG);
	if (!prop) return GF_OK;
	pctx->reconfigure = GF_FALSE;

	hvcc = gf_odf_hevc_cfg_read(prop->value.data.ptr, prop->value.data.size, (pctx->codec_id==GF_CODECID_LHVC) ? GF_TRUE : GF_FALSE);
	if (pctx->rewrite_vui) {
#ifndef GPAC_DISABLE_AV_PARSERS
		GF_VUIInfo tmp_vui = pctx->vui;
		tmp_vui.update = GF_TRUE;
		e = gf_hevc_change_vui(hvcc, &tmp_vui);
		if (!e) {
			update_props(pctx, &tmp_vui);
		}
#else
		return GF_NOT_SUPPORTED;
#endif /*GPAC_DISABLE_AV_PARSERS*/
	}

	if (ctx->pidc>=0) hvcc->profile_idc = ctx->pidc;
	if (ctx->pspace>=0) hvcc->profile_space = ctx->pspace;
	if (ctx->gpcflags>=0) hvcc->general_profile_compatibility_flags = ctx->gpcflags;


	gf_odf_hevc_cfg_write(hvcc, &dsi, &dsi_size);
	pctx->nalu_size_length = hvcc->nal_unit_size;
	gf_odf_hevc_cfg_del(hvcc);
	if (e) return e;

	gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );

	if (ctx->rmsei || ctx->seis.nb_items || ctx->tc) {
		pctx->rewrite_packet = hevc_rewrite_packet;
	} else {
		pctx->rewrite_packet = none_rewrite_packet;
	}
	return GF_OK;
}

static GF_Err vvc_rewrite_pid_config(GF_BSRWCtx *ctx, BSRWPid *pctx)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	GF_VVCConfig *vvcc;
	GF_Err e = GF_OK;
	u8 *dsi;
	u32 dsi_size;
	const GF_PropertyValue *prop;

	if (ctx->tc) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[BSRW] Timecode manipulation is not supported for VVC\n"));
		return GF_BAD_PARAM;
	}

	prop = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DECODER_CONFIG);
	if (!prop) return GF_OK;
	pctx->reconfigure = GF_FALSE;

	vvcc = gf_odf_vvc_cfg_read(prop->value.data.ptr, prop->value.data.size);
	if (pctx->rewrite_vui) {
		GF_VUIInfo tmp_vui = pctx->vui;
		tmp_vui.update = GF_TRUE;
		e = gf_vvc_change_vui(vvcc, &tmp_vui);
		if (!e) {
			update_props(pctx, &tmp_vui);
		}
	}

	if (ctx->pidc>=0) vvcc->general_profile_idc = ctx->pidc;
	if (ctx->lev>=0) vvcc->general_level_idc = ctx->pidc;


	gf_odf_vvc_cfg_write(vvcc, &dsi, &dsi_size);
	pctx->nalu_size_length = vvcc->nal_unit_size;
	gf_odf_vvc_cfg_del(vvcc);
	if (e) return e;

	gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );

	if (ctx->rmsei || ctx->seis.nb_items || ctx->tc) {
		pctx->rewrite_packet = vvc_rewrite_packet;
	} else {
		pctx->rewrite_packet = none_rewrite_packet;
	}
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif /*GPAC_DISABLE_AV_PARSERS*/
}

static GF_Err none_rewrite_pid_config(GF_BSRWCtx *ctx, BSRWPid *pctx)
{
	pctx->reconfigure = GF_FALSE;
	return GF_OK;
}

static GF_Err rewrite_pid_config(GF_BSRWCtx *ctx, BSRWPid *pctx)
{
	GF_Err e = pctx->rewrite_pid_config(ctx, pctx);
	if (e) return e;
	pctx->prev_cprim = pctx->prev_ctfc = pctx->prev_cmx = pctx->prev_sar = -1;
	return GF_OK;
}

static void init_vui(GF_BSRWCtx *ctx, BSRWPid *pctx)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	pctx->vui.ar_num = ctx->sar.num;
	pctx->vui.ar_den = ctx->sar.den;
	pctx->vui.color_matrix = ctx->cmx;
	pctx->vui.color_prim = ctx->cprim;
	pctx->vui.fullrange = ctx->fullrange;
	pctx->vui.remove_video_info = ctx->novsi;
	pctx->vui.remove_vui_timing_info = ctx->novuitiming;
	pctx->vui.color_tfc = ctx->ctfc;
	pctx->vui.video_format = ctx->vidfmt;
#endif /*GPAC_DISABLE_AV_PARSERS*/

	pctx->rewrite_vui = GF_TRUE;
	if (ctx->tc) {
		if (pctx->codec_id == GF_CODECID_AVC) {
			pctx->vui.enable_pic_struct = ctx->tc != BSRW_TC_REMOVE;
			return;
		}
	}
	if (ctx->sar.num>=0) return;
	if ((s32) ctx->sar.den>=0) return;
	if (ctx->cmx>-1) return;
	if (ctx->cprim>-1) return;
	if (ctx->fullrange) return;
	if (ctx->novsi) return;
	if (ctx->novuitiming) return;
	if (ctx->ctfc>-1) return;
	if (ctx->vidfmt>-1) return;
	//all default
	pctx->rewrite_vui = GF_FALSE;
}

static GF_Err prores_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck)
{
	u8 *output;
	GF_FilterPacket *dst_pck = gf_filter_pck_new_clone(pctx->opid, pck, &output);
	if (!dst_pck) return GF_OUT_OF_MEM;
	
	//starting at offset 20 in frame:
	/*
	prores_frame->chroma_format = gf_bs_read_int(bs, 2);
	gf_bs_read_int(bs, 2);
	prores_frame->interlaced_mode = gf_bs_read_int(bs, 2);
	gf_bs_read_int(bs, 2);
	prores_frame->aspect_ratio_information = gf_bs_read_int(bs, 4);
	prores_frame->framerate_code = gf_bs_read_int(bs, 4);
	prores_frame->color_primaries = gf_bs_read_u8(bs);
	prores_frame->transfer_characteristics = gf_bs_read_u8(bs);
	prores_frame->matrix_coefficients = gf_bs_read_u8(bs);
	gf_bs_read_int(bs, 4);
	prores_frame->alpha_channel_type = gf_bs_read_int(bs, 4);
	*/

	if (ctx->sar.num && ctx->sar.den) {
		u32 framerate_code = output[21] & 0xF;
		u32 new_ar = 0;
		if (ctx->sar.num==ctx->sar.den) new_ar = 1;
		else if (ctx->sar.num * 3 == ctx->sar.den * 4) new_ar = 2;
		else if (ctx->sar.num * 9 == ctx->sar.den * 16) new_ar = 3;
		else {
			if (pctx->prev_sar != new_ar) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[BSRW] Aspect ratio %d/%d not registered in ProRes, using 0 (unknown)\n", ctx->sar.num, ctx->sar.den));
			}
		}
		new_ar <<= 4;
		framerate_code |= new_ar;
		output[21] = (u8) framerate_code;
		if (pctx->prev_sar != new_ar) {
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_SAR, &PROP_FRAC(ctx->sar) );
			pctx->prev_sar = new_ar;
		}
	}
	if (ctx->cprim>=0) {
		output[22] = (u8) ctx->cprim;
		if (pctx->prev_cprim != ctx->cprim) {
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_PRIMARIES, &PROP_UINT(ctx->cprim) );
			pctx->prev_cprim = ctx->cprim;
		}
	}
	if (ctx->ctfc>=0) {
		output[23] = (u8) ctx->ctfc;
		if (ctx->ctfc != pctx->prev_ctfc) {
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_TRANSFER, &PROP_UINT(ctx->ctfc) );
			pctx->prev_ctfc = ctx->ctfc;
		}
	}
	if (ctx->cmx>=0) {
		output[24] = (u8) ctx->cmx;
		if (pctx->prev_cmx != ctx->cmx) {
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_MX, &PROP_UINT(ctx->cmx) );
			pctx->prev_cmx = ctx->cmx;
		}
	}
	return gf_filter_pck_send(dst_pck);
}


static GF_Err bsrw_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	GF_BSRWCtx *ctx = (GF_BSRWCtx *) gf_filter_get_udta(filter);
	BSRWPid *pctx = gf_filter_pid_get_udta(pid);

	//disconnect of src pid (not yet supported)
	if (is_remove) {
		if (pctx) {
			if (pctx->opid) {
				gf_filter_pid_remove(pctx->opid);
				pctx->opid = NULL;
			}
			gf_filter_pid_set_udta(pid, NULL);
			gf_list_del_item(ctx->pids, pctx);
 			gf_free(pctx);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	gf_fatal_assert(prop);
	u32 codec_id = prop->value.uint;

	if (!pctx) {
		GF_SAFEALLOC(pctx, BSRWPid);
		if (!pctx) return GF_OUT_OF_MEM;
		pctx->ipid = pid;
		gf_filter_pid_set_udta(pid, pctx);
		pctx->rewrite_pid_config = none_rewrite_pid_config;
		pctx->rewrite_packet = none_rewrite_packet;
		gf_list_add(ctx->pids, pctx);
		pctx->opid = gf_filter_pid_new(filter);
		if (!pctx->opid) return GF_OUT_OF_MEM;
		pctx->codec_id = codec_id;
		init_vui(ctx, pctx);
	}

	switch (codec_id) {
	case GF_CODECID_AVC:
	case GF_CODECID_SVC:
	case GF_CODECID_MVC:
		pctx->rewrite_pid_config = avc_rewrite_pid_config;
		break;
	case GF_CODECID_HEVC:
	case GF_CODECID_HEVC_TILES:
	case GF_CODECID_LHVC:
		pctx->rewrite_pid_config = hevc_rewrite_pid_config;
		break;
	case GF_CODECID_VVC:
	case GF_CODECID_VVC_SUBPIC:
		pctx->rewrite_pid_config = vvc_rewrite_pid_config;
		break;
	case GF_CODECID_MPEG4_PART2:
		pctx->rewrite_pid_config = m4v_rewrite_pid_config;
		break;
	case GF_CODECID_AV1:
		pctx->rewrite_packet = av1_rewrite_packet;
		break;
	case GF_CODECID_AP4H:
	case GF_CODECID_AP4X:
	case GF_CODECID_APCH:
	case GF_CODECID_APCN:
	case GF_CODECID_APCO:
	case GF_CODECID_APCS:
		pctx->rewrite_pid_config = none_rewrite_pid_config;
		pctx->rewrite_packet = prores_rewrite_packet;
		break;

	default:
		pctx->rewrite_pid_config = none_rewrite_pid_config;
		pctx->rewrite_packet = none_rewrite_packet;
		break;
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
	if (prop) pctx->fps = prop->value.frac;
	if (!pctx->fps.num || !pctx->fps.den) {
		pctx->fps.num = 25;
		pctx->fps.den = 1;
	}

	gf_filter_pid_copy_properties(pctx->opid, pctx->ipid);
	pctx->codec_id = codec_id;
	pctx->reconfigure = GF_FALSE;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	//rewrite asap - waiting for first packet could lead to issues further down the chain, especially for movie fragments
	//were the init segment could have already been flushed at the time we will dispatch the first packet
	rewrite_pid_config(ctx, pctx);
	return GF_OK;
}


static GF_Err bsrw_process(GF_Filter *filter)
{
	u32 i, count;
	GF_BSRWCtx *ctx = (GF_BSRWCtx *) gf_filter_get_udta(filter);

	count = gf_filter_get_ipid_count(filter);
	for (i=0; i<count; i++) {
		BSRWPid *pctx;
		GF_FilterPacket *pck;
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		if (!pid) break;
		pctx = gf_filter_pid_get_udta(pid);
		if (!pctx) break;
		if (ctx->reconfigure)
			pctx->reconfigure = GF_TRUE;

		if (pctx->reconfigure) {
			init_vui(ctx, pctx);
			GF_Err e = rewrite_pid_config(ctx, pctx);
			if (e) return e;
		}
		pck = gf_filter_pid_get_packet(pid);
		if (!pck) {
			if (gf_filter_pid_is_eos(pctx->ipid))
				gf_filter_pid_set_eos(pctx->opid);
			continue;
		}
		pctx->rewrite_packet(ctx, pctx, pck);
		gf_filter_pid_drop_packet(pid);
	}
	ctx->reconfigure = GF_FALSE;
	return GF_OK;
}

static GF_Err bsrw_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *new_val)
{
	GF_BSRWCtx *ctx = (GF_BSRWCtx *) gf_filter_get_udta(filter);
	ctx->reconfigure = GF_TRUE;
	return GF_OK;
}

//copied and simplified from reframer to keep the same syntax
static GF_Err bsrw_parse_date(const char *date_in, GF_TimeCode *tc_out)
{
	char* date = (char*) date_in;
	if (!date_in || !tc_out)
		return GF_BAD_PARAM;

	if (date[0] == '-') {
		tc_out->negative = GF_TRUE;
		date++;
	}

	u8 h, m, s;
	u16 n_frames;
	if (sscanf(date, "TC%u:%u:%u:%u", &h, &m, &s, &n_frames) != 4)
		return GF_BAD_PARAM;

	// Express timecode to timestamp
	u64 v = h*3600 + m*60 + s;
	v *= 1000;
	v += n_frames;

	tc_out->hours = h;
	tc_out->minutes = m;
	tc_out->seconds = s;
	tc_out->n_frames = n_frames;
	tc_out->as_timestamp = v;
	return GF_OK;
}

static GF_Err bsrw_initialize(GF_Filter *filter)
{
	GF_BSRWCtx *ctx = (GF_BSRWCtx *) gf_filter_get_udta(filter);
	ctx->pids = gf_list_new();

	GF_Err e = GF_OK;
	if (ctx->tcxs) e |= bsrw_parse_date(ctx->tcxs, &ctx->tcxs_val);
	if (ctx->tcxe) e |= bsrw_parse_date(ctx->tcxe, &ctx->tcxe_val);
	if (ctx->tcsc && !strstr(ctx->tcsc, "first")) {
		e |= bsrw_parse_date(ctx->tcsc, &ctx->tcsc_val);
		ctx->tcsc_inferred = GF_TRUE;
	}
	if (e) return e;

	if (ctx->tc == BSRW_TC_SHIFT || ctx->tc == BSRW_TC_CONSTANT) {
		if (!ctx->tcsc) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[BSRW] Timecode manipulation mode requires `tcsc` to be set\n"));
			return GF_BAD_PARAM;
		}
	}

#ifdef GPAC_ENABLE_COVERAGE
	bsrw_update_arg(filter, NULL, NULL);
#endif
	return GF_OK;
}
static void bsrw_finalize(GF_Filter *filter)
{
	GF_BSRWCtx *ctx = (GF_BSRWCtx *) gf_filter_get_udta(filter);
	while (gf_list_count(ctx->pids)) {
		BSRWPid *pctx = gf_list_pop_back(ctx->pids);
		if (pctx->avc) gf_free(pctx->avc);
		gf_free(pctx);
	}
	gf_list_del(ctx->pids);
}

#define OFFS(_n)	#_n, offsetof(GF_BSRWCtx, _n)
static GF_FilterArgs BSRWArgs[] =
{
	///do not change order of the first 3
	{ OFFS(cprim), "color primaries according to ISO/IEC 23001-8 / 23091-2", GF_PROP_CICP_COL_PRIM, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(ctfc), "color transfer characteristics according to ISO/IEC 23001-8 / 23091-2", GF_PROP_CICP_COL_TFC, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(cmx), "color matrix coeficients according to ISO/IEC 23001-8 / 23091-2", GF_PROP_CICP_COL_MX, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(sar), "aspect ratio to rewrite", GF_PROP_FRACTION, "-1/-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(m4vpl), "set ProfileLevel for MPEG-4 video part two", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(fullrange), "video full range flag", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(novsi), "remove video_signal_type from VUI in AVC|H264 and HEVC", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(novuitiming), "remove timing_info from VUI in AVC|H264 and HEVC", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(prof), "profile indication for AVC|H264", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(lev), "level indication for AVC|H264, level_idc for VVC", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(pcomp), "profile compatibility for AVC|H264", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(pidc), "profile IDC for HEVC and VVC", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(pspace), "profile space for HEVC", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(gpcflags), "general compatibility flags for HEVC", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(tcxs), "timecode manipulation start", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_UPDATE},
	{ OFFS(tcxe), "timecode manipulation end", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_UPDATE},
	{ OFFS(tcsc), "timecode constant for use with shift/constant modes", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_UPDATE},
	{ OFFS(tc), "timecode manipulation mode\n"
	"- none: do not change anything\n"
	"- remove: remove timecodes\n"
	"- insert: insert timecodes based on cts or `tcsc` (if provided)\n"
	"- shift: shift timecodes based by `tcsc`\n"
	"- constant: overwrite timecodes with `tcsc`", GF_PROP_UINT, "none", "none|remove|insert|shift|constant", GF_FS_ARG_UPDATE},
	{ OFFS(seis), "list of SEI message types (4,137,144,...). When used with `rmsei`, this serves as a blacklist. If left empty, all SEIs will be removed. Otherwise, it serves as a whitelist", GF_PROP_UINT_LIST, NULL, NULL, GF_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(rmsei), "remove SEI messages from bitstream for AVC|H264, HEVC and VVC", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(vidfmt), "video format for AVC|H264, HEVC and VVC", GF_PROP_SINT, "-1", "component|pal|ntsc|secam|mac|undef", GF_FS_ARG_UPDATE},
	{0}
};

static const GF_FilterCapability BSRWCaps[] =
{
	//this is correct but we want the filter to act as passthrough for other media
#if 0
	CAP_UINT(GF_CAPS_INPUT_STATIC ,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED|GF_CAPFLAG_STATIC , GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_MVC),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC_TILES),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_AP4H),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_AP4X),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_APCH),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_APCN),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_APCO),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_APCS),
#else
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
#endif
};

GF_FilterRegister BSRWRegister = {
	.name = "bsrw",
	GF_FS_SET_DESCRIPTION("Bitstream metadata rewriter")
	GF_FS_SET_HELP("This filter rewrites some metadata of various bitstream formats.\n"
	"The filter can currently modify the following properties in video bitstreams:\n"
	"- MPEG-4 Visual:\n"
	"  - sample aspect ratio\n"
	"  - profile and level\n"
	"- AVC|H264, HEVC and VVC:\n"
	"  - sample aspect ratio\n"
	"  - profile, level, profile compatibility\n"
	"  - video format, video fullrange\n"
	"  - color primaries, transfer characteristics and matrix coefficients (or remove all info)\n"
	"  - (AVC|HEVC) timecode"
	"- AV1:\n"
	"  - timecode\n"
	"- ProRes:\n"
	"  - sample aspect ratio\n"
	"  - color primaries, transfer characteristics and matrix coefficients\n"
	"  \n"
	"Values are by default initialized to -1, implying to keep the related info (present or not) in the bitstream.\n"
	"A [-sar]() value of `0/0` will remove sample aspect ratio info from bitstream if possible.\n"
	"  \n"
	"The filter can currently modify the following properties in the stream configuration but not in the bitstream:\n"
	"- HEVC: profile IDC, profile space, general compatibility flags\n"
	"- VVC: profile IDC, general profile and level indication\n"
	"  \n"
	"The filter will work in passthrough mode for all other codecs and media types.\n"
	"# Timecode Manipulation\n"
	"One can optionally set the [-tcxs]() and [-tcxe]() to define the start and end of timecode manipulation. By default, the filter will process all packets.\n"
	"Some modes require you to define [-tcsc](). This follows the same format as the timecode itself ([-]'TC'HH:MM:SS:FF). The use of negative values is only meaningful in the `shift` mode. It's also possible to set [-tcsc]() to `first` to infer the value from the first timecode when timecode manipulation starts. In this case, unless a timecode is found, the filter will not perform any operation.\n"
	"## Modes\n"
	"Timecode manipulation has four modes and they all have their own operating nuances.\n"
	"### Remove\n"
	"Remove all timecodes from the bitstream.\n"
	"### Insert\n"
	"Insert timecodes based on the CTS. If [-tcsc]() is set, it will be used as timecode offset.\n"
	"This mode will overwrite existing timecodes (if any).\n"
	"### Shift\n"
	"Shift all timecodes by the value defined in [-tcsc]().\n"
	"This mode will only modify timecodes if they exists, no new timecode will be inserted.\n"
	"### Constant\n"
	"Set all timecodes to the value defined in [-tcsc]().\n"
	"Again, this mode wouldn't insert new timecodes.\n"
	"## Examples\n"
	"EX gpac -i in.mp4 bsrw:tc=insert [dst]\n"
	"EX gpac -i in.mp4 bsrw:tc=insert:tcsc=TC00:00:10:00 [dst]\n"
	"EX gpac -i in.mp4 bsrw:tc=shift:tcsc=TC00:00:10:00:tcxs=TC00:01:00:00 [dst]\n"
	)
	.private_size = sizeof(GF_BSRWCtx),
	.max_extra_pids = 0xFFFFFFFF,
	.flags = GF_FS_REG_EXPLICIT_ONLY|GF_FS_REG_ALLOW_CYCLIC,
	.args = BSRWArgs,
	SETCAPS(BSRWCaps),
	.initialize = bsrw_initialize,
	.finalize = bsrw_finalize,
	.configure_pid = bsrw_configure_pid,
	.process = bsrw_process,
	.update_arg = bsrw_update_arg,
	.hint_class_type = GF_FS_CLASS_STREAM
};

const GF_FilterRegister *bsrw_register(GF_FilterSession *session)
{
	//assign runtime caps on first load
	if (gf_opts_get_bool("temp", "helponly")) {
		BSRWArgs[0].min_max_enum = gf_cicp_color_primaries_all_names();
		BSRWArgs[1].min_max_enum = gf_cicp_color_transfer_all_names();
		BSRWArgs[2].min_max_enum = gf_cicp_color_matrix_all_names();
	}
	return (const GF_FilterRegister *) &BSRWRegister;
}
#else
const GF_FilterRegister *bsrw_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_BSRW

