/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom Paris 2022-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / restamper filter
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

#ifndef GPAC_DISABLE_RESTAMP

typedef struct
{
	GF_FilterPid *ipid, *opid;
	s64 ts_offset, ts_shift_plus_one;
	Bool raw_vid_copy;
	Bool is_audio, is_video;
	s64 last_min_dts;
	u32 timescale;

	u64 last_ts;
	u32 min_dur;
	u32 rescale;
	u64 nb_frames;
	GF_FilterPacket *pck_ref;
	u32 keep_prev_state;

	u64 last_ts_ref_plus_one, last_original_ts;
	u32 ts_rescale, min_ref_dur;
	//offset added to reference TS of a packet, to add to the cts
	s64 ref_ts_diff;

	GF_List *packets;
} RestampPid;

enum
{
	RESTAMP_RAWV_NO=0,
	RESTAMP_RAWV_FORCE,
	RESTAMP_RAWV_DYN,
};

typedef struct
{
	GF_Fraction fps, delay, delay_v, delay_a, delay_t, delay_o;
	GF_Fraction64 tsinit;
	u32 rawv;
	u32 align;
	Bool reorder;

	GF_List *pids;
	Bool reconfigure;
	Bool config_timing;
	u32 config_retry;
} RestampCtx;


static GF_Err restamp_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	GF_Fraction64 fps_scaler;
	RestampCtx *ctx = (RestampCtx *) gf_filter_get_udta(filter);
	RestampPid *pctx = gf_filter_pid_get_udta(pid);

	//disconnect of src pid (not yet supported)
	if (is_remove) {
		if (pctx) {
			if (pctx->opid) {
				gf_filter_pid_remove(pctx->opid);
			}
			gf_filter_pid_set_udta(pid, NULL);
			gf_list_del_item(ctx->pids, pctx);
			while (gf_list_count(pctx->packets)) {
				GF_FilterPacket *pck = gf_list_pop_front(pctx->packets);
				gf_filter_pck_unref(pck);
			}
			gf_list_del(pctx->packets);

			gf_free(pctx);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!pctx) {
		GF_SAFEALLOC(pctx, RestampPid);
		if (!pctx) return GF_OUT_OF_MEM;
		pctx->ipid = pid;
		gf_list_add(ctx->pids, pctx);
		gf_filter_pid_set_udta(pid, pctx);
		pctx->opid = gf_filter_pid_new(filter);
		if (!pctx->opid) return GF_OUT_OF_MEM;
		ctx->config_timing = GF_TRUE;
		if (ctx->reorder)
			pctx->packets = gf_list_new();
	}

	pctx->raw_vid_copy = GF_FALSE;
	pctx->is_audio = GF_FALSE;
	pctx->rescale = GF_FALSE;
	pctx->is_video = GF_FALSE;
	GF_Fraction *delay = NULL;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	gf_fatal_assert(prop);
	switch (prop->value.uint) {
	case GF_STREAM_VISUAL:
		pctx->is_video = GF_TRUE;
		delay = &ctx->delay_v;
		if (ctx->rawv && (ctx->fps.num>0)) {
			prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
			if (prop && (prop->value.uint==GF_CODECID_RAW)) {
				pctx->raw_vid_copy = GF_TRUE;
			} else {
				prop = gf_filter_pid_get_property(pid, GF_PROP_PID_HAS_SYNC);
				if (prop && !prop->value.boolean) {
					pctx->raw_vid_copy = GF_TRUE;
				} else {
					gf_filter_pid_negociate_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
					return GF_OK;
				}
			}
		}
		break;
	case GF_STREAM_AUDIO:
		delay = &ctx->delay_a;
		pctx->is_audio = GF_TRUE;
		break;
	case GF_STREAM_TEXT:
		delay = &ctx->delay_t;
		break;
	default:
		delay = &ctx->delay_o;
		break;
	}
	gf_filter_pid_copy_properties(pctx->opid, pctx->ipid);

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	u32 timescale = prop ? prop->value.uint : 1000;
	//global delay
	s64 ts_offset = gf_timestamp_rescale_signed(ctx->delay.num, ctx->delay.den, timescale);
	//per media-type delay
	if (delay)
		ts_offset += gf_timestamp_rescale_signed(delay->num, delay->den, timescale);

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	if (prop) {
		ts_offset += prop->value.longsint;
	}
	if (ts_offset<0) {
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DELAY, &PROP_LONGSINT(ts_offset));
		ts_offset = 0;
	} else {
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DELAY, NULL);
	}
	pctx->ts_offset = ts_offset;
	if (timescale && (timescale!=pctx->timescale))
		pctx->last_min_dts = 0;
	pctx->timescale = timescale;

	ctx->reconfigure = GF_FALSE;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	pctx->ts_rescale = pctx->timescale;

	if (pctx->is_audio) return GF_OK;

	if (!ctx->fps.num) return GF_OK;

	gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_MAX_TS_DELTA, NULL);
	gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_CONSTANT_DURATION, NULL);

	fps_scaler.num = 0;
	fps_scaler.den = 1;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
	if (prop) {
		if (ctx->fps.num>0) {
			fps_scaler.num = prop->value.frac.num;
			fps_scaler.den = prop->value.frac.den;
			fps_scaler.num *= ctx->fps.den;
			fps_scaler.den *= ctx->fps.num;
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_FPS, &PROP_FRAC_INT(ctx->fps.num, ctx->fps.den));
		} else {
			GF_Fraction f = prop->value.frac;
			f.num *= -ctx->fps.num;
			f.den /= ctx->fps.den;
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_FPS, &PROP_FRAC(f));
		}
	}
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DURATION);
	if ((ctx->fps.num) && prop->value.lfrac.den) {
		GF_Fraction64 ndur = prop->value.lfrac;
		if (ctx->fps.num>0) {
			if (fps_scaler.num) {
				ndur.num *= fps_scaler.num;
				ndur.num /= fps_scaler.den;
				gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DURATION, &PROP_FRAC64(ndur));
			} else {
				gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DURATION, NULL);
			}
		} else {
			ndur.num /= -ctx->fps.num;
			ndur.num *= ctx->fps.den;
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DURATION, &PROP_FRAC64(ndur));
		}
	}
	if ((ctx->fps.num>0) && (timescale % ctx->fps.num)) {
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->fps.num));
		pctx->rescale = GF_TRUE;
		pctx->ts_rescale = ctx->fps.num;
	}
	return GF_OK;
}

static u64 restamp_get_timestamp(RestampCtx *ctx, RestampPid *pctx, u64 ots, Bool is_ref_ts, GF_FilterPacket *pck)
{
	if (ots == GF_FILTER_NO_TS) return ots;
	u64 ts;

	if (pctx->ts_shift_plus_one) {
		ots = ots + (pctx->ts_shift_plus_one-1);
	}
	//ots + pctx->ts_offset<0
	if ((pctx->ts_offset<0) && (ots < (u64) -pctx->ts_offset)) {
		pctx->ts_offset += (s64) ots + pctx->ts_offset;
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DELAY, &PROP_LONGSINT(pctx->ts_offset) );
		ts = 0;
	}
	else {
		ts = ots + pctx->ts_offset;
	}

	if (pctx->is_audio || !ctx->fps.num || !pctx->is_video) goto ts_done;

	if (!pctx->last_min_dts) {
		pctx->last_min_dts = 1 + ts;
		pctx->last_ts = 1 + ts;
		if (pctx->raw_vid_copy)
			pctx->keep_prev_state = 1;
		goto ts_done;
	}
	s64 diff = ts - (pctx->last_min_dts-1);
	if (ctx->fps.num<0) {
		diff *= ctx->fps.den;
		diff /= -ctx->fps.num;
		ts = pctx->last_min_dts-1 + diff;
		goto ts_done;
	}

	if (ots + 1 <= pctx->last_ts) goto ts_done;
	u64 dur = ots - (pctx->last_ts-1);
	if (!pctx->min_dur || (dur < pctx->min_dur)) pctx->min_dur = (u32) dur;


	if (pctx->raw_vid_copy) {
		u64 new_ts = pctx->nb_frames * ctx->fps.den;
		new_ts *= pctx->timescale;
		new_ts /= ctx->fps.num;

		pctx->keep_prev_state = 0;
		if (new_ts + pctx->min_dur/2 <= dur) {
			pctx->keep_prev_state = 1;
		}
		else if (dur + pctx->min_dur/2 <= new_ts) {
			pctx->keep_prev_state = 2;
			ts = 0;
			goto ts_done;
		}

		if (pctx->rescale) {
			new_ts = pctx->nb_frames * ctx->fps.den;
		}
		ts = new_ts + pctx->last_min_dts-1;
		goto ts_done;
	}


	u64 nb_frames = diff / pctx->min_dur;
	if (!pctx->rescale) {
		nb_frames *= ((u64) pctx->min_dur * ctx->fps.num) / pctx->timescale;
	}
	nb_frames *= ctx->fps.den;
	ts = pctx->last_min_dts-1 + nb_frames;


ts_done:
	if (ctx->align) {
		if (is_ref_ts) {
			//compute duration
			u32 dur = gf_filter_pck_get_duration(pck);
			if (!dur || ((pctx->last_original_ts < ots) && (dur > ots - pctx->last_original_ts)))
				dur = (u32) (ots - pctx->last_original_ts);

			pctx->last_original_ts = ots;
			if (dur & pctx->rescale)
				dur = (u32) gf_timestamp_rescale(dur, pctx->timescale, pctx->ts_rescale);

			if (dur && (!pctx->min_ref_dur || (dur < pctx->min_ref_dur)))
				pctx->min_ref_dur = dur;

			//we have sent a previous packet, check the diff between new TS and last one
			if (pctx->last_ts_ref_plus_one) {
				s64 diff = ts;
				u64 pred_ts = pctx->last_ts_ref_plus_one-1 + pctx->min_ref_dur;
				pctx->ref_ts_diff = 0;
				diff -= pred_ts;
				diff = gf_timestamp_rescale_signed(diff, pctx->ts_rescale, 1000);
				if (ABS(diff)>ctx->align) {
					u64 new_ts = pred_ts;
					pctx->ref_ts_diff = new_ts;
					pctx->ref_ts_diff -= ts;
					ts = new_ts;
				}
			}
			pctx->last_ts_ref_plus_one = ts + 1;
		} else {
			ts = (s64) ts + pctx->ref_ts_diff;
		}
	}
	return ts;
}

static void restamp_config_timing(GF_Filter *filter, RestampCtx *ctx)
{
	Bool not_ready=GF_FALSE;
	Bool has_blocking=GF_FALSE;
	u64 min_ts=0;
	u32 min_timescale=0;

	if ((ctx->tsinit.num<0) || !ctx->tsinit.den) {
		ctx->config_timing = GF_FALSE;
		return;
	}


	u32 i, count = gf_list_count(ctx->pids);
	for (i=0; i<count; i++) {
		RestampPid *pctx = gf_list_get(ctx->pids, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(pctx->ipid);
		if (!pck) {
			if (!gf_filter_pid_is_eos(pctx->ipid))
				not_ready = GF_TRUE;
			continue;
		}
		if (gf_filter_pck_is_blocking_ref(pck))
			has_blocking=GF_TRUE;

		u64 ts = gf_filter_pck_get_dts(pck);
		if (ts==GF_FILTER_NO_TS)
			ts = gf_filter_pck_get_cts(pck);

		if (!min_timescale || (min_ts < gf_timestamp_rescale(ts, pctx->timescale, min_timescale))) {
			min_timescale = pctx->timescale;
			min_ts = ts;
		}
	}
	if (not_ready && !has_blocking) {
		ctx->config_retry++;
		if (ctx->config_retry<10000)
			return;
	}
	ctx->config_retry=0;

	for (i=0; i<count; i++) {
		RestampPid *pctx = gf_list_get(ctx->pids, i);

		pctx->ts_shift_plus_one = - (s64) gf_timestamp_rescale_signed(min_ts, min_timescale, pctx->timescale);
		pctx->ts_shift_plus_one += gf_timestamp_rescale(ctx->tsinit.num, ctx->tsinit.den, pctx->timescale);
		pctx->ts_shift_plus_one += 1;
	}
	ctx->config_timing = GF_FALSE;
}


static GF_Err restamp_process(GF_Filter *filter)
{
	u32 i, count;
	RestampCtx *ctx = (RestampCtx *) gf_filter_get_udta(filter);

	if (ctx->config_timing) {
		restamp_config_timing(filter, ctx);
		if (ctx->config_timing) return GF_OK;
	}

	count = gf_list_count(ctx->pids);
	for (i=0; i<count; i++) {
		RestampPid *pctx = gf_list_get(ctx->pids, i);
		if (!pctx) continue;
		if (ctx->reconfigure) {
			restamp_configure_pid(filter, pctx->ipid, GF_FALSE);
		}

		while (1) {
			s64 ts;
			Bool is_ref = GF_FALSE;
			GF_FilterPacket *pck = gf_filter_pid_get_packet(pctx->ipid);

			if (!pck) {
				if (gf_filter_pid_is_eos(pctx->ipid)) {
					if (pctx->packets) {
						pck = gf_list_pop_front(pctx->packets);
						is_ref = GF_TRUE;
					}
					if (!pck) {
						gf_filter_pid_set_eos(pctx->opid);
						if (pctx->pck_ref) {
							gf_filter_pck_unref(pctx->pck_ref);
							pctx->pck_ref = NULL;
						}
					}
				}
				if (!pck)
					break;
			}

			if (ctx->reorder && !is_ref) {
				u64 ts = gf_filter_pck_get_cts(pck);
				u32 j;
				Bool can_flush=GF_FALSE;
				for (j=0; j<gf_list_count(pctx->packets); j++) {
					GF_FilterPacket *tmp = gf_list_get(pctx->packets, j);
					u64 ats = gf_filter_pck_get_cts(tmp);
					if (ats>ts) {
						gf_filter_pck_ref(&pck);
						gf_list_insert(pctx->packets, pck, j);
						pck=NULL;
						//we insert after first frame, we can flush the first frame
						if (j) can_flush = GF_TRUE;
						break;
					}
				}
				if (pck) {
					gf_filter_pck_ref(&pck);
					gf_list_add(pctx->packets, pck);
				}
				gf_filter_pid_drop_packet(pctx->ipid);
				if (!can_flush)
					break;
				pck = gf_list_pop_front(pctx->packets);
				is_ref = GF_TRUE;
			}

			GF_FilterPacket *opck;

			if (!pctx->raw_vid_copy) {
				Bool ts_is_ref = GF_TRUE;
				opck = gf_filter_pck_new_ref(pctx->opid, 0, 0, pck);
				if (!opck) return GF_OUT_OF_MEM;
				gf_filter_pck_merge_properties(pck, opck);
				ts = restamp_get_timestamp(ctx, pctx, gf_filter_pck_get_dts(pck), ts_is_ref, pck);
				if (ts != GF_FILTER_NO_TS) {
					ts_is_ref = GF_FALSE;
					gf_filter_pck_set_dts(opck, ts);
				}

				ts = restamp_get_timestamp(ctx, pctx, gf_filter_pck_get_cts(pck), ts_is_ref, pck);
				if (ts != GF_FILTER_NO_TS)
					gf_filter_pck_set_cts(opck, ts);

				if (!pctx->is_audio) {
					u32 dur = gf_filter_pck_get_duration(pck);
					if (ctx->fps.num<0) {
						dur *= ctx->fps.den;
						dur /= -ctx->fps.num;
						gf_filter_pck_set_duration(opck, dur);
					}
				}
				gf_filter_pck_send(opck);
				if (is_ref)
					gf_filter_pck_unref(pck);
				else
					gf_filter_pid_drop_packet(pctx->ipid);
			} else {
				u64 ots = gf_filter_pck_get_cts(pck) ;
 				Bool discard = GF_FALSE;
				pctx->keep_prev_state = 0;
				ts = restamp_get_timestamp(ctx, pctx, ots, GF_TRUE, pck);

				if (pctx->keep_prev_state==2) {
					if (pctx->pck_ref) {
						gf_filter_pck_unref(pctx->pck_ref);
						pctx->pck_ref = NULL;
					}
					if (is_ref)
						gf_filter_pck_unref(pck);
					else
						gf_filter_pid_drop_packet(pctx->ipid);

					GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Restamp] Droping frame TS "LLU"/%u\n", ots, pctx->timescale));
					continue;
				}

				if (!pctx->keep_prev_state && pctx->pck_ref) {
					gf_filter_pck_unref(pctx->pck_ref);
					pctx->pck_ref = NULL;
					GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Restamp] Moving to next frame for output TS "LLU"\n", ts));
				} else if (pctx->pck_ref) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Restamp] Keeping prev frame for output ts "LLU"\n", ts));
				}
				if (!pctx->pck_ref) {
					pctx->pck_ref = pck;
					gf_filter_pck_ref(&pctx->pck_ref);
					discard = GF_TRUE;
				}
				gf_assert(pctx->pck_ref);
				opck = gf_filter_pck_new_ref(pctx->opid, 0, 0, pctx->pck_ref);
				if (!opck) return GF_OUT_OF_MEM;
				gf_filter_pck_merge_properties(pck, opck);
				if (ts != GF_FILTER_NO_TS)
					gf_filter_pck_set_cts(opck, ts);
				pctx->nb_frames++;

				gf_filter_pck_send(opck);

				if (discard) {
					if (!is_ref)
						gf_filter_pid_drop_packet(pctx->ipid);
				} else {
					if (is_ref)
						gf_list_insert(pctx->packets, pck, 0);
				}
			}
		}
	}
	return GF_OK;
}

static GF_Err restamp_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *new_val)
{
	RestampCtx *ctx = (RestampCtx *) gf_filter_get_udta(filter);
	ctx->reconfigure = GF_TRUE;
	return GF_OK;
}

static const GF_FilterCapability RestampCapsRawVid[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
};

static GF_Err restamp_initialize(GF_Filter *filter)
{
	RestampCtx *ctx = (RestampCtx *) gf_filter_get_udta(filter);
	ctx->pids = gf_list_new();

#ifdef GPAC_ENABLE_COVERAGE
	restamp_update_arg(filter, NULL, NULL);
#endif

	if ((u32) ctx->fps.num==ctx->fps.den) ctx->fps.num = 0;
	if ((u32) -ctx->fps.num==ctx->fps.den) ctx->fps.num = 0;

	if ((ctx->rawv==RESTAMP_RAWV_FORCE) && (ctx->fps.num>0)) {
		gf_filter_override_caps(filter, RestampCapsRawVid, GF_ARRAY_LENGTH(RestampCapsRawVid) );
	}

	return GF_OK;
}
static void restamp_finalize(GF_Filter *filter)
{
	RestampCtx *ctx = (RestampCtx *) gf_filter_get_udta(filter);
	while (gf_list_count(ctx->pids)) {
		RestampPid *pctx = gf_list_pop_back(ctx->pids);
		if (pctx->pck_ref) gf_filter_pck_unref(pctx->pck_ref);
		while (gf_list_count(pctx->packets)) {
			GF_FilterPacket *pck = gf_list_pop_front(pctx->packets);
			gf_filter_pck_unref(pck);
		}
		gf_list_del(pctx->packets);
		gf_free(pctx);
	}
	gf_list_del(ctx->pids);
}

#define OFFS(_n)	#_n, offsetof(RestampCtx, _n)
static GF_FilterArgs RestampArgs[] =
{
	{ OFFS(fps), "target fps", GF_PROP_FRACTION, "0/1", NULL, 0},
	{ OFFS(delay), "delay to add to all streams", GF_PROP_FRACTION, "0/1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(delay_v), "delay to add to video streams", GF_PROP_FRACTION, "0/1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(delay_a), "delay to add to audio streams", GF_PROP_FRACTION, "0/1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(delay_t), "delay to add to text streams", GF_PROP_FRACTION, "0/1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(delay_o), "delay to add to other streams", GF_PROP_FRACTION, "0/1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(rawv), "copy video frames\n"
		"- no: no raw frame copy/drop\n"
		"- force: force decoding all video streams\n"
		"- dyn: decoding video streams if not all intra"
	, GF_PROP_UINT, "no", "no|force|dyn", 0},
	{ OFFS(tsinit), "initial timestamp to resync to, negative values disables resync", GF_PROP_FRACTION64, "-1/1", NULL, 0},
	{ OFFS(align), "timestamp alignment threshold (0 disables alignment) - see filter help", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(reorder), "reorder input packets by CTS (resulting PID may fail decoding)", GF_PROP_BOOL, "false", NULL, 0},
	{0}
};

static const GF_FilterCapability RestampCaps[] =
{
	//we setup 2 caps for dynamic video reconfig
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	{0},
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
};

const GF_FilterRegister RestampRegister = {
	.name = "restamp",
	GF_FS_SET_DESCRIPTION("Packet timestamp rewriter")
	GF_FS_SET_HELP("This filter rewrites timing (offsets and rate) of packets.\n"
	"\n"
	"The delays (global or per stream class) can be either positive (stream presented later) or negative (stream presented sooner).\n"
	"\n"
	"The specified [-fps]() can be either 0, positive or negative.\n"
	"- if 0 or if the stream is audio, stream rate is not modified.\n"
	"- otherwise if negative, stream rate is multiplied by `-fps.num/fps.den`.\n"
	"- otherwise if positive and the stream is not video, stream rate is not modified.\n"
	"- otherwise (video PID), constant frame rate is assumed and:\n"
	"  - if [-rawv=no](), video frame rate is changed to the specified rate (speed-up or slow-down).\n"
	"  - if [-rawv=force](), input video stream is decoded and video frames are dropped/copied to match the new rate.\n"
	"  - if [-rawv=dyn](), input video stream is decoded if not all-intra and video frames are dropped/copied to match the new rate.\n"
	"\n"
	"Note: frames are simply copied or dropped with no motion compensation.\n"
	"\n"
	"When [-align]() is not 0, if the difference between two consecutive timestamps is greater than the specified threshold, the new timestamp \n"
	"is set to the last computed timestamp plus the minimum packet duration for the stream.\n"
	)
	.private_size = sizeof(RestampCtx),
	.max_extra_pids = 0xFFFFFFFF,
	.flags = GF_FS_REG_EXPLICIT_ONLY|GF_FS_REG_ALLOW_CYCLIC|GF_FS_REG_TEMP_INIT,
	.args = RestampArgs,
	SETCAPS(RestampCaps),
	.initialize = restamp_initialize,
	.finalize = restamp_finalize,
	.configure_pid = restamp_configure_pid,
	.process = restamp_process,
	.update_arg = restamp_update_arg
};

const GF_FilterRegister *restamp_register(GF_FilterSession *session)
{
	return &RestampRegister;
}
#else
const GF_FilterRegister *restamp_register(GF_FilterSession *session)
{
	return NULL;
}
#endif //#ifndef GPAC_DISABLE_RESTAMP

