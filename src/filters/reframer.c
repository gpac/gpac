/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / force reframer filter
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

enum
{
	REFRAME_RT_OFF = 0,
	REFRAME_RT_ON,
	REFRAME_RT_SYNC,
};

enum
{
	REFRAME_ROUND_BEFORE=0,
	REFRAME_ROUND_SEEK,
	REFRAME_ROUND_AFTER,
	REFRAME_ROUND_CLOSEST,
};

enum
{
	RANGE_NONE=0,
	RANGE_CLOSED,
	RANGE_OPEN,
	RANGE_DONE
};

enum
{
	UTCREF_LOCAL=0,
	UTCREF_ANY,
	UTCREF_MEDIA,
};


enum
{
	EXTRACT_NONE=0,
	EXTRACT_RANGE,
	EXTRACT_SAP,
	EXTRACT_SIZE,
	EXTRACT_DUR,
};

enum
{
	RAW_AV=0,
	RAW_AUDIO,
	RAW_VIDEO,
	RAW_NONE,
};

#define RT_PRECISION_US	2000

typedef struct
{
	GF_FilterPid *ipid, *opid;
	u32 timescale;
	u64 cts_us_at_init;
	u64 sys_clock_at_init;
	u32 nb_frames;
	Bool can_split;
	Bool all_saps;
	Bool needs_adjust;
	Bool use_blocking_refs;

	u64 ts_at_range_start_plus_one;
	u64 ts_at_range_end;

	GF_List *pck_queue;
	//0: not computed, 1: computed and valid TS, 2: end of stream on pid
	u32 range_start_computed;
	u64 range_end_reached_ts;
	u64 prev_sap_ts;
	u32 prev_sap_frame_idx;
	u32 nb_frames_range;
	u64 sap_ts_plus_one;
	Bool first_pck_sent;

	//only positive delay here
	u64 tk_delay;
	//only media skip (video, audio priming)
	u32 ts_sub;
	Bool in_eos;
	u32 split_start;
	u32 split_end;

	GF_FilterPacket *split_pck;
	GF_FilterPacket *reinsert_single_pck;
	Bool is_playing;

	Bool is_raw;
	u32 codec_id, stream_type;
	u32 nb_ch, sample_rate, abps;
	Bool audio_planar;
	//for uncompressed audio, number of audio samples to keep from split frame / trash from next split frame
	//for seek mode, duration of media to keep from split frame / trash from next split frame
	u32 audio_samples_to_keep;

	u32 nb_frames_until_start;

	Bool seek_mode;
	u64 probe_ref_frame_ts;

	Bool fetch_done;

	u64 last_utc_ref, last_utc_ref_ts;

} RTStream;

typedef struct
{
	//args
	Bool exporter;
	GF_PropUIntList saps;
	GF_PropUIntList frames;
	Bool refs;
	u32 rt;
	Double speed;
	u32 raw;
	GF_PropStringList xs, xe;
	Bool nosap, splitrange, xadjust, tcmdrw, no_audio_seek, probe_ref, xots;
	u32 xround, utc_ref, utc_probe;
	Double seeksafe;
	GF_PropStringList props;

	//internal
	Bool filter_sap1;
	Bool filter_sap2;
	Bool filter_sap3;
	Bool filter_sap4;
	Bool filter_sap_none;

	GF_List *streams;
	RTStream *clock;

	u64 reschedule_in;
	u64 clock_val;

	u32 range_type;
	u32 cur_range_idx;
	//if cur_start.den is 0, cur_start.num is UTC start time
	//if cur_end.den is 0, cur_start.num is UTC stop time, only if cur_start uses UTC
	GF_Fraction64 cur_start, cur_end;
	u64 start_frame_idx_plus_one, end_frame_idx_plus_one;

	Bool in_range;

	Bool seekable;

	GF_Fraction64 extract_dur;
	u32 extract_mode;
	Bool is_range_extraction;
	u32 file_idx;

	u64 min_ts_computed;
	u32 min_ts_scale;
	u64 split_size;
	u64 est_file_size;
	u64 prev_min_ts_computed;
	u32 prev_min_ts_scale;

	u32 wait_video_range_adjust;
	Bool has_seen_eos;
	u32 eos_state;
	u32 nb_non_saps;

	u32 nb_video_frames_since_start_at_range_start;
	u32 nb_video_frames_since_start;

	Bool flush_samples;
	u64 cumulated_size;
	u64 last_ts;

	u64 flush_max_ts;
	u32 flush_max_ts_scale;

	u64 last_utc_time_s;
	u32 last_clock_probe;
} GF_ReframerCtx;

static void reframer_reset_stream(GF_ReframerCtx *ctx, RTStream *st, Bool do_free)
{
	if (st->pck_queue) {
		while (gf_list_count(st->pck_queue)) {
			GF_FilterPacket *pck = gf_list_pop_front(st->pck_queue);
			gf_filter_pck_unref(pck);
		}
		if (do_free)
			gf_list_del(st->pck_queue);
	}
	if (st->split_pck) gf_filter_pck_unref(st->split_pck);
	st->split_pck = NULL;
	if (st->reinsert_single_pck) gf_filter_pck_unref(st->reinsert_single_pck);
	st->reinsert_single_pck = NULL;
	if (do_free)
		gf_free(st);
}

static void reframer_push_props(GF_ReframerCtx *ctx, RTStream *st)
{
	gf_filter_pid_reset_properties(st->opid);
	gf_filter_pid_copy_properties(st->opid, st->ipid);
	//if range processing, we drop frames not in the target playback range so do not forward delay
	if (ctx->range_type && ((st->tk_delay>0) || (!st->tk_delay && !st->ts_sub)) ){
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_DELAY, NULL);
	}
	if (ctx->filter_sap1 || ctx->filter_sap2)
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_HAS_SYNC, &PROP_BOOL(GF_FALSE)); //false: all samples are sync

	//seek mode, signal we have sample-accurate seek info for the pid
	if (st->seek_mode)
		gf_filter_pid_set_property(st->opid, GF_PROP_PCK_SKIP_BEGIN, &PROP_UINT(1));

	//for old arch compat, signal we must remove edits
	if (gf_sys_old_arch_compat()) {
		gf_filter_pid_set_property_str(st->opid, "reframer_rem_edits", &PROP_BOOL(GF_TRUE) );
	}
}

GF_Err reframer_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 i;
	const GF_PropertyValue *p;
	GF_ReframerCtx *ctx = gf_filter_get_udta(filter);
	RTStream *st = gf_filter_pid_get_udta(pid);

	if (is_remove) {
		if (st) {
			if (st->opid)
				gf_filter_pid_remove(st->opid);
			gf_list_del_item(ctx->streams, st);
			reframer_reset_stream(ctx, st, GF_TRUE);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!st) {
		GF_SAFEALLOC(st, RTStream);
		if (!st) return GF_OUT_OF_MEM;
		
		gf_list_add(ctx->streams, st);
		st->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_udta(pid, st);
		gf_filter_pid_set_udta(st->opid, st);
		st->ipid = pid;
		st->pck_queue = gf_list_new();
		st->all_saps = GF_TRUE;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) st->timescale = p->value.uint;
	else st->timescale = 1000;

	if (!st->all_saps) {
		ctx->nb_non_saps--;
		st->all_saps = GF_TRUE;
	}
	st->can_split = GF_FALSE;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	st->stream_type = p ? p->value.uint : 0;
	switch (st->stream_type) {
	case GF_STREAM_TEXT:
	case GF_STREAM_METADATA:
		st->can_split = GF_TRUE;
		break;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	st->codec_id = p ? p->value.uint : 0;
	st->nb_ch = st->abps = st->sample_rate = 0;
	st->audio_planar = GF_FALSE;
	st->is_raw = (st->codec_id==GF_CODECID_RAW) ? GF_TRUE : GF_FALSE;

	if (st->is_raw && (st->stream_type==GF_STREAM_AUDIO)) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
		if (p) st->abps = gf_audio_fmt_bit_depth(p->value.uint) / 8;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
		if (p) st->nb_ch = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
		st->sample_rate = p ? p->value.uint : st->timescale;
		st->abps *= st->nb_ch;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
		if (p && (p->value.uint>GF_AUDIO_FMT_LAST_PACKED))
			st->audio_planar = GF_TRUE;
	}


	//if non SAP1 detected and xadjust, move directly to needs_adjust=TRUE
	//otherwise consider we don't need to probe for next sap, this will be updated
	//if one packet is not sap
	st->needs_adjust = (ctx->xadjust && !st->all_saps) ? GF_TRUE : GF_FALSE;

	st->tk_delay = 0;
	st->ts_sub = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	if (p) {
		//delay negative is skip: this is CTS adjustment for B-frames: we keep that notif in the stream
		if (p->value.longsint<=0) {
			st->tk_delay = 0;
			st->ts_sub = (u32) -p->value.longsint;
		}
		//delay positive is delay, we keep the value for RT regulation and range
		else {
			st->tk_delay = (u64) p->value.longsint;
		}
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_PLAYBACK_MODE);
	if (!p || (p->value.uint < GF_PLAYBACK_MODE_FASTFORWARD))
		ctx->seekable = GF_FALSE;


	ctx->filter_sap1 = ctx->filter_sap2 = ctx->filter_sap3 = ctx->filter_sap4 = ctx->filter_sap_none = GF_FALSE;
	for (i=0; i<ctx->saps.nb_items; i++) {
		switch (ctx->saps.vals[i]) {
		case 1:
			ctx->filter_sap1 = GF_TRUE;
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_HAS_SYNC, &PROP_BOOL(GF_FALSE)); //false: all samples are sync
			break;
		case 2:
			ctx->filter_sap2 = GF_TRUE;
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_HAS_SYNC, &PROP_BOOL(GF_FALSE)); //false: all samples are sync
			break;
		case 3: ctx->filter_sap3 = GF_TRUE; break;
		case 4: ctx->filter_sap4 = GF_TRUE; break;
		default: ctx->filter_sap_none = GF_TRUE; break;
		}
	}
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	//NEVER use seek mode for uncompressed audio, packets are splitted anyway
	if (ctx->extract_mode==EXTRACT_RANGE) {
		if (st->abps) {
			st->seek_mode = GF_FALSE;
		}
		//use seek if seek is forced
		else if (ctx->xround==REFRAME_ROUND_SEEK) {
			st->seek_mode = GF_TRUE;
		}
		//use seek mode for compressed audio with or without priming
		else if (st->stream_type==GF_STREAM_AUDIO) {
			st->seek_mode = ctx->no_audio_seek ? GF_FALSE : GF_TRUE;
		} else {
			st->seek_mode = GF_FALSE;
		}
	} else {
		st->seek_mode = GF_FALSE;
	}
	reframer_push_props(ctx, st);

	if (ctx->cur_range_idx && (ctx->cur_range_idx <= ctx->props.nb_items)) {
		gf_filter_pid_push_properties(st->opid, ctx->props.vals[ctx->cur_range_idx-1], GF_FALSE, GF_FALSE);
	}

	return GF_OK;
}

static Bool reframer_parse_date(char *date, GF_Fraction64 *value, u64 *frame_idx_plus_one, u32 *extract_mode, Bool *is_dur)
{
	u64 v;
	value->num  =0;
	value->den = 0;

	if (extract_mode)
		*extract_mode = EXTRACT_RANGE;
	if (is_dur)
		*is_dur = GF_FALSE;

	if (date[0] == 'T') {
		u32 h=0, m=0, s=0, ms=0;
		if (strchr(date, '.')) {
			if (sscanf(date, "T%u:%u:%u.%u", &h, &m, &s, &ms) != 4) {
				h = 0;
				if (sscanf(date, "T%u:%u.%u", &m, &s, &ms) != 3) {
					m = 0;
					if (sscanf(date, "T%u.%u", &s, &ms) != 2) {
						goto exit;
					}
				}
			}
			if (ms>=1000) ms=0;
		} else {
			if (sscanf(date, "T%u:%u:%u", &h, &m, &s) != 3) {
				h = 0;
				if (sscanf(date, "T%u:%u", &m, &s) != 2) {
					goto exit;
				}
			}
		}
		v = h*3600 + m*60 + s;
		v *= 1000;
		v += ms;
		value->num = v;
		value->den = 1000;
		return GF_TRUE;
	}
	if ((date[0]=='F') || (date[0]=='f')) {
		*frame_idx_plus_one = 1 + atoi(date+1);
		return GF_TRUE;
	}
	if (!strcmp(date, "RAP") || !strcmp(date, "SAP")) {
		if (extract_mode)
			*extract_mode = EXTRACT_SAP;
		value->num = 0;
		value->den = 1000;
		return GF_TRUE;
	}
	if ((date[0]=='D') || (date[0]=='d')) {
		if (extract_mode)
			*extract_mode = EXTRACT_DUR;
		if (is_dur)
			*is_dur = GF_TRUE;

		if (sscanf(date+1, LLD"/"LLU, &value->num, &value->den)==2) {
			return GF_TRUE;
		}
		if (strchr(date+1, '.')) {
			Double res = atof(date+1);
			value->den = 1000000;
			value->num = (s64) (res * value->den);
			return GF_TRUE;
		}
		if (sscanf(date+1, LLU, &v)==1) {
			value->num = v;
			value->den = 1;
			return GF_TRUE;
		}
	}
	if ((date[0]=='S') || (date[0]=='s')) {
		GF_PropertyValue p;
		if (extract_mode)
			*extract_mode = EXTRACT_SIZE;
		p = gf_props_parse_value(GF_PROP_LUINT, "size", date+1, NULL, ',');
		if (p.type==GF_PROP_LUINT) {
			value->den = p.value.longuint;
			return GF_TRUE;
		}
	}
	if (strchr(date, 'T')) {
		value->num = gf_net_parse_date(date);
		value->den = 0;
		return GF_TRUE;
	}

	if (gf_parse_lfrac(date, value)) {
		return GF_TRUE;
	}

exit:
	GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] Unrecognized date format %s, expecting THH:MM:SS[.ms], TMM:SS[.ms], TSS[.ms], INT or FRAC\n", date));
	if (extract_mode)
		*extract_mode = EXTRACT_NONE;
	return GF_FALSE;
}

static void reframer_load_range(GF_ReframerCtx *ctx)
{
	u32 i, count;
	Bool do_seek = ctx->seekable;
	Bool reset_asplit = GF_TRUE;
	u64 prev_frame = ctx->start_frame_idx_plus_one;
	GF_Fraction64 prev_end;
	char *start_date=NULL, *end_date=NULL;

	ctx->nb_video_frames_since_start_at_range_start = ctx->nb_video_frames_since_start;

	if (ctx->extract_mode==EXTRACT_DUR) {
		ctx->cur_start.num += (ctx->extract_dur.num * ctx->cur_start.den) / ctx->extract_dur.den;
		ctx->cur_end.num += (ctx->extract_dur.num * ctx->cur_end.den) / ctx->extract_dur.den;
		ctx->file_idx++;
		return;
	}
	if ((ctx->extract_mode==EXTRACT_SAP) || (ctx->extract_mode==EXTRACT_SIZE)) {
		ctx->cur_start = ctx->cur_end;
		ctx->min_ts_computed = 0;
		ctx->min_ts_scale = 0;
		ctx->file_idx++;
		return;
	}
	prev_end = ctx->cur_end;
	ctx->start_frame_idx_plus_one = 0;
	ctx->end_frame_idx_plus_one = 0;
	ctx->cur_start.num = 0;
	ctx->cur_start.den = 0;
	ctx->cur_end.num = 0;
	ctx->cur_end.den = 0;

	count = ctx->xs.nb_items;
	if (!count) {
		if (ctx->range_type) goto range_done;
		return;
	}
	if (ctx->cur_range_idx>=count) {
		goto range_done;
	} else {
		start_date = ctx->xs.vals[ctx->cur_range_idx];
		end_date = NULL;
		if (ctx->cur_range_idx < ctx->xe.nb_items)
			end_date = ctx->xe.vals[ctx->cur_range_idx];
		else if (ctx->cur_range_idx + 1 < ctx->xs.nb_items)
			end_date = ctx->xs.vals[ctx->cur_range_idx+1];

		if (end_date && !end_date[0]) end_date = NULL;
	}
	if (!start_date)
		goto range_done;

	ctx->cur_range_idx++;
	if (!end_date) ctx->range_type = RANGE_OPEN;
	else ctx->range_type = RANGE_CLOSED;

	if (!reframer_parse_date(start_date, &ctx->cur_start, &ctx->start_frame_idx_plus_one, &ctx->extract_mode, NULL)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] cannot parse start date, assuming end of ranges\n"));
		//done
		ctx->range_type = RANGE_DONE;
		return;
	}

	//range in frame
	if (ctx->start_frame_idx_plus_one) {
		//either range is before or prev range was not frame-based
		if (ctx->start_frame_idx_plus_one > prev_frame)
			do_seek = GF_TRUE;
	}
	//range is time based, prev was frame-based, seek
	else if (!prev_end.den) {
		do_seek = GF_TRUE;
	} else {
		//cur start is before previous end, need to seek
		if (ctx->cur_start.num * prev_end.den < prev_end.num * ctx->cur_start.den) {
			do_seek = GF_TRUE;
		}
		//cur start is less than our seek safety from previous end, do not seek
		if (ctx->cur_start.num * prev_end.den < (prev_end.num + ctx->seeksafe*prev_end.den) * ctx->cur_start.den)
			do_seek = GF_FALSE;
	}
	//do not issue seek on first range, done when catching play requests
	if (ctx->cur_range_idx==1) {
		do_seek = GF_FALSE;
	}
	if (!ctx->cur_start.den)
		do_seek = GF_FALSE;

	if (!ctx->seekable && do_seek) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Reframer] ranges not in order and input not seekable, aborting extraction\n"));
		goto range_done;
	}

	ctx->is_range_extraction = ((ctx->extract_mode==EXTRACT_RANGE) || (ctx->extract_mode==EXTRACT_DUR)) ? GF_TRUE : GF_FALSE;

	if (ctx->extract_mode != EXTRACT_RANGE) {
		end_date = NULL;
		if (ctx->extract_mode==EXTRACT_DUR) {
			ctx->extract_dur = ctx->cur_start;
			ctx->cur_start.num = 0;
			ctx->cur_start.den = ctx->extract_dur.den;
			ctx->cur_end = ctx->extract_dur;
			ctx->range_type = RANGE_CLOSED;
			ctx->file_idx = 1;
			ctx->splitrange = GF_TRUE;
			ctx->xadjust = GF_TRUE;
		}
		else if (ctx->extract_mode==EXTRACT_SIZE) {
			ctx->splitrange = GF_TRUE;
			ctx->split_size = ctx->cur_start.den;
			if (!ctx->split_size) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] invalid split size %d\n", ctx->split_size));
				goto range_done;
			}
			ctx->file_idx = 1;
		}
		else if (ctx->extract_mode==EXTRACT_SAP) {
			ctx->splitrange = GF_TRUE;
		}
	}
	if (end_date) {
		Bool is_dur = GF_FALSE;
		if (!reframer_parse_date(end_date, &ctx->cur_end, &ctx->end_frame_idx_plus_one, NULL, &is_dur)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] cannot parse end date, assuming open range\n"));
			ctx->range_type = RANGE_OPEN;
		} else {
			if (is_dur) {
				ctx->cur_end.num = gf_timestamp_rescale(ctx->cur_end.num, ctx->cur_end.den, ctx->cur_start.den);
				ctx->cur_end.den = ctx->cur_start.den;
				ctx->cur_end.num += ctx->cur_start.num;
			}
			if (gf_timestamp_greater_or_equal(ctx->cur_start.num, ctx->cur_start.den, ctx->cur_end.num, ctx->cur_end.den) ) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] End range before start range, assuming open range\n"));
				ctx->range_type = RANGE_OPEN;
			}
		}
	}

	if (prev_end.den && (prev_end.num * ctx->cur_start.den == prev_end.den * ctx->cur_start.num))
		reset_asplit = GF_FALSE;

	//reset realtime range and issue seek requests
	if (ctx->rt || do_seek || reset_asplit) {
		Double start_range = 0;
		if (do_seek) {
			start_range = (Double) ctx->cur_start.num;
			start_range /= ctx->cur_start.den;
			if (start_range > ctx->seeksafe)
				start_range -= ctx->seeksafe;
			else
				start_range = 0;
			ctx->has_seen_eos = GF_FALSE;
			ctx->nb_video_frames_since_start_at_range_start = 0;
			ctx->nb_video_frames_since_start = 0;
		}
		count = gf_list_count(ctx->streams);
		for (i=0; i<count; i++) {
			RTStream *st = gf_list_get(ctx->streams, i);
			if (ctx->rt) {
				st->cts_us_at_init = 0;
				st->sys_clock_at_init = 0;
			}
			if (do_seek) {
				GF_FilterEvent evt;
				GF_FEVT_INIT(evt, GF_FEVT_STOP, st->ipid);
				gf_filter_pid_send_event(st->ipid, &evt);
				GF_FEVT_INIT(evt, GF_FEVT_PLAY, st->ipid);
				evt.play.start_range = start_range;
				evt.play.speed = 1;
				gf_filter_pid_send_event(st->ipid, &evt);

				st->nb_frames = 0;
			}
			if (reset_asplit) {
				st->audio_samples_to_keep = 0;
			}
		}
	}

	if (ctx->cur_range_idx && (ctx->cur_range_idx <= ctx->props.nb_items)) {
		count = gf_list_count(ctx->streams);
		for (i=0; i<count; i++) {
			RTStream *st = gf_list_get(ctx->streams, i);

			reframer_push_props(ctx, st);
			gf_filter_pid_push_properties(st->opid, ctx->props.vals[ctx->cur_range_idx-1], GF_FALSE, GF_FALSE);
			gf_filter_pid_set_property_str(st->opid, "period_resume", &PROP_STRING("") );
		}
	}

	return;

range_done:
	//done
	ctx->range_type = RANGE_DONE;
	count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		GF_FilterEvent evt;
		RTStream *st = gf_list_get(ctx->streams, i);
		gf_filter_pid_set_discard(st->ipid, GF_TRUE);
		GF_FEVT_INIT(evt, GF_FEVT_STOP, st->ipid);
		gf_filter_pid_send_event(st->ipid, &evt);
		gf_filter_pid_set_eos(st->opid);
	}

}

void reframer_drop_packet(GF_ReframerCtx *ctx, RTStream *st, GF_FilterPacket *pck, Bool pck_is_ref)
{
	if (pck_is_ref) {
		gf_list_rem(st->pck_queue, 0);
		gf_filter_pck_unref(pck);
	} else {
		gf_filter_pid_drop_packet(st->ipid);
	}
}

void reframer_copy_raw_audio(RTStream *st, const u8 *src, u32 src_size, u32 offset, u8 *dst, u32 nb_samp)
{
	if (st->audio_planar) {
		u32 i, bps, stride;
		stride = src_size / st->nb_ch;
		bps = st->abps / st->nb_ch;
		for (i=0; i<st->nb_ch; i++) {
			memcpy(dst + i*bps*nb_samp, src + i*stride + offset * bps, nb_samp * bps);
		}
	} else {
		memcpy(dst, src + offset * st->abps, nb_samp * st->abps);
	}
}

Bool reframer_send_packet(GF_Filter *filter, GF_ReframerCtx *ctx, RTStream *st, GF_FilterPacket *pck, Bool pck_is_ref)
{
	Bool do_send = GF_FALSE;


	if (!ctx->rt) {
		do_send = GF_TRUE;
	} else {
		u64 cts_us = gf_filter_pck_get_dts(pck);
		if (cts_us==GF_FILTER_NO_TS)
			cts_us = gf_filter_pck_get_cts(pck);

		if (cts_us==GF_FILTER_NO_TS) {
			do_send = GF_TRUE;
		} else {
			RTStream *st_clock = st;
			u64 clock = ctx->clock_val;
			cts_us += st->tk_delay;

			cts_us = gf_timestamp_rescale(cts_us, st->timescale, 1000000);
			if (ctx->rt==REFRAME_RT_SYNC) {
				if (!ctx->clock) ctx->clock = st;
				st_clock = ctx->clock;
			}
			if (!st_clock->sys_clock_at_init) {
				st_clock->cts_us_at_init = cts_us;
				st_clock->sys_clock_at_init = clock;
				do_send = GF_TRUE;
			} else if (cts_us < st_clock->cts_us_at_init) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] CTS less than CTS used to initialize clock by %d ms, not delaying\n", (u32) (st_clock->cts_us_at_init - cts_us)/1000));
				do_send = GF_TRUE;
			} else {
				u64 diff = cts_us - st_clock->cts_us_at_init;
				if (ctx->speed>0) diff = (u64) ( diff / ctx->speed);
				else if (ctx->speed<0) diff = (u64) ( diff / -ctx->speed);

				clock -= st_clock->sys_clock_at_init;
				if (clock + RT_PRECISION_US >= diff) {
					do_send = GF_TRUE;
					if (clock > diff) {
						GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Reframer] Sending packet "LLU" us too late (clock diff "LLU" - CTS diff "LLU")\n", 1000+clock - diff, clock, diff));
					}
				} else {
					diff -= clock;
					if (!ctx->reschedule_in)
						ctx->reschedule_in = diff;
					else if (ctx->reschedule_in > diff)
						ctx->reschedule_in = diff;
				}
			}
		}
	}

	if (!ctx->range_type && ctx->frames.nb_items) {
		u32 i;
		Bool found=GF_FALSE;
		for (i=0; i<ctx->frames.nb_items; i++) {
			if (ctx->frames.vals[i] == st->nb_frames + 1) {
				found=GF_TRUE;
				break;
			}
		}
		if (!found) {
			//drop
			gf_filter_pid_drop_packet(st->ipid);
			st->nb_frames++;
			return GF_TRUE;
		}
	}

	if (!do_send)
		return GF_FALSE;

	//range processing
	if (st->ts_at_range_start_plus_one) {
		Bool is_split = GF_FALSE;
		s64 ts;
		s32 ts_adj = 0;
		u32 cts_offset=0;
		u32 dur=0;
		GF_FilterPacket *new_pck;

		//tmcd, rewrite sample
		if (ctx->tcmdrw && (st->codec_id==GF_CODECID_TMCD) && st->split_start && ctx->nb_video_frames_since_start_at_range_start) {
			GF_BitStream *bs;
			u32 nb_frames;
			u8 *tcmd_data = NULL;
			new_pck = gf_filter_pck_new_copy(st->opid, pck, &tcmd_data);
			if (!new_pck) return GF_FALSE;

			bs = gf_bs_new(tcmd_data, 4, GF_BITSTREAM_READ);
			nb_frames = gf_bs_read_u32(bs);
			gf_bs_del(bs);
			bs = gf_bs_new(tcmd_data, 4, GF_BITSTREAM_WRITE);
			gf_bs_seek(bs, 0);
			gf_bs_write_u32(bs, nb_frames+ctx->nb_video_frames_since_start_at_range_start);
			gf_bs_del(bs);
			dur = gf_filter_pck_get_duration(pck);
			if (dur > st->split_start)
				dur -= st->split_start;
			if (st->split_end && (dur > st->split_end - st->split_start))
				dur = st->split_end - st->split_start;
			ts_adj = st->split_start;

		} else if ((pck == st->split_pck) && st->audio_samples_to_keep) {
			const u8 *data;
			u32 pck_size;
			data = gf_filter_pck_get_data(pck, &pck_size);
			if (st->abps) {
				if (st->audio_samples_to_keep * st->abps <= pck_size) {
					u8 *output;
					new_pck = gf_filter_pck_new_alloc(st->opid, st->audio_samples_to_keep * st->abps, &output);
					if (!new_pck) return GF_FALSE;
					reframer_copy_raw_audio(st, data, pck_size, 0, output, st->audio_samples_to_keep);
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] Broken raw audio frame size/duration: %d samples to keep but packet is smaller (%d samples)\n", st->audio_samples_to_keep, pck_size/st->abps));
					st->audio_samples_to_keep = 0;
					new_pck = gf_filter_pck_new_ref(st->opid, 0, 0, pck);
					if (!new_pck) return GF_FALSE;
				}
			} else {
				new_pck = gf_filter_pck_new_ref(st->opid, 0, 0, pck);
				if (!new_pck) return GF_FALSE;
			}
			dur = st->audio_samples_to_keep;
		} else if (st->audio_samples_to_keep) {
			u8 *output;
			const u8 *data;
			u32 pck_size;
			data = gf_filter_pck_get_data(pck, &pck_size);
			if (st->abps) {
				if (pck_size < st->audio_samples_to_keep * st->abps) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] Broken raw audio frame size/duration: %d samples to keep but packet is smaller (%d samples)\n", st->audio_samples_to_keep, pck_size/st->abps));
					st->audio_samples_to_keep = 0;
				}
				new_pck = gf_filter_pck_new_alloc(st->opid, pck_size - st->audio_samples_to_keep * st->abps, &output);
				if (!new_pck) return GF_FALSE;

				reframer_copy_raw_audio(st, data, pck_size, st->audio_samples_to_keep, output, pck_size/st->abps - st->audio_samples_to_keep);

				dur = pck_size/st->abps - st->audio_samples_to_keep;

				cts_offset = st->audio_samples_to_keep;
				//if first range, add CTS offset to ts at range start
				if (ctx->cur_range_idx==1)
					st->ts_at_range_start_plus_one += cts_offset;
			} else {
				new_pck = gf_filter_pck_new_ref(st->opid, 0, 0, pck);
				if (!new_pck) return GF_FALSE;

				dur = gf_filter_pck_get_duration(pck);
				gf_filter_pck_set_property(new_pck, GF_PROP_PCK_SKIP_BEGIN, &PROP_UINT( st->audio_samples_to_keep ) );
			}
			st->audio_samples_to_keep = 0;
		} else {
			new_pck = gf_filter_pck_new_ref(st->opid, 0, 0, pck);
			if (!new_pck) return GF_FALSE;
		}
		gf_filter_pck_merge_properties(pck, new_pck);

		if (cts_offset || dur) {
			if (st->abps && (st->timescale!=st->sample_rate)) {
				cts_offset = (u32) gf_timestamp_rescale(cts_offset, st->sample_rate, st->timescale);
				dur *= st->timescale;
				dur /= st->sample_rate;
			}
			gf_filter_pck_set_duration(new_pck, dur);
		}

		//signal chunk start boundary
		if (!st->first_pck_sent) {
			u64 start_t, end_t;
			char szFileSuf[1000];
			u32 i, len;
			char *file_suf_name = NULL;
			char *start = ctx->xs.vals[ctx->cur_range_idx-1];
			char *end = NULL;
			if ((ctx->range_type==1) && (ctx->cur_range_idx<ctx->xe.nb_items+1)) {
				end = ctx->xe.vals[ctx->cur_range_idx-1];
			}
			st->first_pck_sent = GF_TRUE;

			if (ctx->extract_mode==EXTRACT_RANGE) {

				gf_filter_pck_set_property(new_pck, GF_PROP_PCK_FILENUM, &PROP_UINT(ctx->cur_range_idx) );

				if (strchr(start, '/')) {
					start_t = ctx->cur_start.num;
					start_t /= ctx->cur_start.den;
					if (ctx->cur_end.den) {
						end_t = ctx->cur_end.num;
						end_t /= ctx->cur_end.den;
						sprintf(szFileSuf, LLU"-"LLU, start_t, end_t);
					} else {
						sprintf(szFileSuf, LLU, start_t);
					}
					gf_filter_pck_set_property(new_pck, GF_PROP_PCK_FILESUF, &PROP_STRING(szFileSuf) );
				} else {

					gf_dynstrcat(&file_suf_name, start, NULL);
					if (end)
						gf_dynstrcat(&file_suf_name, end, "_");

					len = (u32) strlen(file_suf_name);
					//replace : and / characters
					for (i=0; i<len; i++) {
						switch (file_suf_name[i]) {
						case ':':
						case '/':
							file_suf_name[i] = '.';
							break;
						}
					}
					gf_filter_pck_set_property(new_pck, GF_PROP_PCK_FILESUF, &PROP_STRING_NO_COPY(file_suf_name) );
				}
			} else {
				gf_filter_pck_set_property(new_pck, GF_PROP_PCK_FILENUM, &PROP_UINT(ctx->file_idx) );

				start_t = ctx->cur_start.num * 1000;
				start_t /= ctx->cur_start.den;
				if (ctx->cur_end.den) {
					end_t = ctx->cur_end.num * 1000;
					end_t /= ctx->cur_end.den;
					sprintf(szFileSuf, LLU"-"LLU, start_t, end_t);
				} else {
					sprintf(szFileSuf, LLU, start_t);
				}
				gf_filter_pck_set_property(new_pck, GF_PROP_PCK_FILESUF, &PROP_STRING(szFileSuf) );
			}
			if (st->nb_frames) {
				const GF_PropertyValue *p = gf_filter_pid_get_property(st->ipid, GF_PROP_PID_NB_FRAMES);
				if (p) {
					gf_filter_pid_set_property(st->opid, GF_PROP_PID_FRAME_OFFSET, &PROP_UINT(st->nb_frames) );
				}
			}

			if (ctx->file_idx>1) {
				gf_filter_pid_set_property(st->opid, GF_PROP_PID_DELAY, NULL);
			}
		}

		//rewrite timestamps
		ts = gf_filter_pck_get_cts(pck) + cts_offset;

		if (ts != GF_FILTER_NO_TS) {
			if (ctx->is_range_extraction
				&& st->seek_mode
				&& gf_timestamp_less(ts + ts_adj - st->ts_sub, st->timescale, ctx->cur_start.num, ctx->cur_start.den)
			) {
				gf_filter_pck_set_seek_flag(new_pck, GF_TRUE);
				gf_filter_pck_set_property(new_pck, GF_PROP_PCK_SKIP_BEGIN, NULL);
				if (st->stream_type!=GF_STREAM_VISUAL) {
					u32 pck_dur = gf_filter_pck_get_duration(new_pck);
					if (gf_timestamp_greater(ts + ts_adj + pck_dur - st->ts_sub, st->timescale, ctx->cur_start.num, ctx->cur_start.den)) {
						u32 ts_diff = (u32) ( gf_timestamp_rescale(ctx->cur_start.num, ctx->cur_start.den, st->timescale) - (ts + ts_adj - st->ts_sub));
						gf_filter_pck_set_property(new_pck, GF_PROP_PCK_SKIP_BEGIN, &PROP_UINT(ts_diff));
						gf_filter_pck_set_seek_flag(new_pck, GF_FALSE);
					}
				}
			}
			if (!ctx->xots) {
				ts += st->tk_delay;
				ts += st->ts_at_range_end;
				ts -= st->ts_at_range_start_plus_one - 1;

				if (ts<0) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] Negative TS while splitting, something went wrong during range estimation, forcing to 0\n"));
					ts = 0;
				}
			}

			if (st->probe_ref_frame_ts && (gf_filter_pck_get_dts(pck) + st->tk_delay + 1 == st->probe_ref_frame_ts)) {
				gf_filter_pck_set_property(new_pck, GF_PROP_PCK_SKIP_PRES, &PROP_BOOL(GF_TRUE));
			}

			gf_filter_pck_set_cts(new_pck, (u64) ts);
			if (st->is_raw) {
				gf_filter_pck_set_dts(new_pck, ts);
			}
		}
		if (!st->is_raw && !ctx->xots) {
			ts = gf_filter_pck_get_dts(pck) + cts_offset;
			if (ts != GF_FILTER_NO_TS) {
				ts += st->tk_delay;
				ts -= st->ts_at_range_start_plus_one - 1;
				ts += st->ts_at_range_end;
				gf_filter_pck_set_dts(new_pck, (u64) ts);
			}
		}
		//packet was split or was re-inserted
		if (st->split_start) {
			if (!dur) {
				u32 pck_dur = gf_filter_pck_get_duration(pck);
				//can happen if source packet is less than split period duration, we just copy with no timing adjustment
				if (pck_dur > st->split_start)
					pck_dur -= st->split_start;
				gf_filter_pck_set_duration(new_pck, pck_dur);
			}
			st->ts_at_range_start_plus_one += st->split_start;
			st->split_start = 0;
			is_split = GF_TRUE;
		}
		//last packet and forced duration
		if (st->split_end && (gf_list_count(st->pck_queue)==1)) {
			if (!dur) {
				gf_filter_pck_set_duration(new_pck, st->split_end);
			}
			st->split_end = 0;
			is_split = GF_TRUE;
		}
		//packet reinserted (not split), adjust duration and store offset in split start
		if (!st->can_split && !is_split && st->reinsert_single_pck) {
			dur = gf_filter_pck_get_duration(pck);
			//only for closed range
			if (st->range_end_reached_ts) {
				u64 ndur = st->range_end_reached_ts;
				ndur -= st->ts_at_range_start_plus_one-1;
				if (ndur && (ndur < dur))
					gf_filter_pck_set_duration(new_pck, (u32) ndur);
				st->split_start = (u32) ndur;
			}
		}

		gf_filter_pck_send(new_pck);

	} else {
		gf_filter_pck_forward(pck, st->opid);
	}


	reframer_drop_packet(ctx, st, pck, pck_is_ref);
	st->nb_frames++;

	if (st->stream_type==GF_STREAM_VISUAL) {
		if (st->nb_frames > ctx->nb_video_frames_since_start) {
			ctx->nb_video_frames_since_start = st->nb_frames;
		}
	}

	return GF_TRUE;
}

static u32 reframer_check_pck_range(GF_Filter *filter, GF_ReframerCtx *ctx, RTStream *st, GF_FilterPacket *pck, u64 ts, u32 dur, u32 frame_idx, u32 *nb_audio_samples_to_keep)
{
	if (ctx->start_frame_idx_plus_one) {
		//frame not after our range start
		if (frame_idx<ctx->start_frame_idx_plus_one) {
			return 0;
		} else {
			//closed range, check
			if ((ctx->range_type!=RANGE_OPEN) && (frame_idx>=ctx->end_frame_idx_plus_one)) {
				return 2;
			}
			return 1;
		}
		return 0;
	}

	Bool before = GF_FALSE;
	Bool after = GF_FALSE;

	if (!ctx->cur_start.den) {
		u64 now=0;
		if (ctx->utc_ref) {
			//check sender NTP on packet
			const GF_PropertyValue *date = gf_filter_pck_get_property(pck, GF_PROP_PCK_SENDER_NTP);
			if (date) {
				now = gf_net_ntp_to_utc(date->value.longuint);
				st->last_utc_ref = now;
				st->last_utc_ref_ts = ts;
			}
			//otherwise check UTC date mapping
			else {
				date = gf_filter_pck_get_property(pck, GF_PROP_PCK_UTC_TIME);
				if (date) {
					ctx->last_clock_probe=0;
					st->last_utc_ref = date->value.longuint;
					st->last_utc_ref_ts = ts;
				}
				//otherwise interpolate from last mapping
				else if (st->last_utc_ref && st->last_utc_ref_ts) {
					s64 diff_ms = (s64) ts;
					diff_ms -= (s64) st->last_utc_ref_ts;
					diff_ms = gf_timestamp_rescale_signed(diff_ms, st->timescale, 1000);
					now = st->last_utc_ref;
					now += diff_ms;
				}
			}

			if (!now && !st->last_utc_ref) {
				if (!ctx->last_clock_probe) ctx->last_clock_probe = gf_sys_clock();
				else if (gf_sys_clock() - ctx->last_clock_probe > ctx->utc_probe) {
					if (ctx->utc_ref==UTCREF_ANY) {
						now = gf_net_get_utc();
						ctx->utc_ref = UTCREF_LOCAL;
						GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("[Reframer] Failed to acquire UTC mapping from media, will use local host\n"));
					} else {
						GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("[Reframer] Failed to acquire UTC mapping from media, aborting\n"));
						ctx->cur_range_idx = ctx->xs.nb_items;
						return 2;
					}
				}
			}
			//time not yet known, consider we are before
			if (!now)
				return 0;
		} else {
			now = gf_net_get_utc();
		}

		if (now < (u64) ctx->cur_start.num) {
			before = GF_TRUE;
			u64 time = (ctx->cur_start.num - now) / 1000;

			if (!ctx->last_utc_time_s || (ctx->last_utc_time_s-2>time)) {
				u32 h, m, s;
				char szStatus[100];
				ctx->last_utc_time_s = time+2;
				h = (u32) (time/3600);
				m = (u32) (time/60 - h*60);
				s = (u32) (time - m*60 - h*3660);
				if (h>24) {
					u32 days = h/24;
					sprintf(szStatus, "Next range start in %d days", days);
				} else {
					sprintf(szStatus, "Next range start in %02d:%02d:%02d", h, m, s);
				}
				if (gf_filter_reporting_enabled(filter)) {
					gf_filter_update_status(filter, 0, szStatus);
				} else {
					GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("[Reframer] %s\r", szStatus));
				}
			}
		} else if (ctx->last_utc_time_s!=1) {
			ctx->last_utc_time_s = 1;
			if (gf_filter_reporting_enabled(filter)) {
				gf_filter_update_status(filter, 0, "");
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("                                                     \r"));
			}
		}

		if (ctx->cur_end.num) {
			u64 end;
			if (ctx->cur_end.den)
				end = ctx->cur_start.num + (ctx->cur_end.num*1000) / ctx->cur_end.den;
			else
				end = ctx->cur_end.num;
			if (now>end)
				after = GF_TRUE;
		}
	} else {
		//check ts not after our range start:
		//if round_seek mode, check TS+dur is less than or equal to desired start, and we will notify the duration of data to skip from the packet
		//otherwise, check TS is strictly less than desired start
		if (st->seek_mode) {
			if (gf_timestamp_less_or_equal(ts+dur, st->timescale, ctx->cur_start.num, ctx->cur_start.den)) {
				before = GF_TRUE;
				if ((st->stream_type==GF_STREAM_AUDIO) && st->ts_sub) {
					if (gf_timestamp_greater(ts+st->ts_sub, st->timescale, ctx->cur_start.num, ctx->cur_start.den)) {
						before = GF_FALSE;
					}
				}
			}
		}
		else if (gf_timestamp_less(ts, st->timescale, ctx->cur_start.num, ctx->cur_start.den)) {
			before = GF_TRUE;
			if (st->abps && gf_timestamp_greater(ts+dur, st->timescale, ctx->cur_start.num, ctx->cur_start.den)) {
				u64 nb_samp = gf_timestamp_rescale(ctx->cur_start.num, ctx->cur_start.den, st->timescale) - ts;
				if (st->timescale != st->sample_rate) {
					nb_samp = gf_timestamp_rescale(nb_samp, st->timescale, st->sample_rate);
				}
				*nb_audio_samples_to_keep = (u32) nb_samp;
				before = GF_FALSE;
			}
		}
		//consider after if time+duration is STRICTLY greater than cut point
		if ((ctx->range_type!=RANGE_OPEN) && gf_timestamp_greater(ts+dur, st->timescale, ctx->cur_end.num, ctx->cur_end.den)) {
			if ((st->abps || st->seek_mode )
				&& gf_timestamp_less(ts, st->timescale, ctx->cur_end.num, ctx->cur_end.den)
			) {
				u64 nb_samp = gf_timestamp_rescale(ctx->cur_end.num, ctx->cur_end.den, st->timescale) - ts;
				if (st->abps && (st->timescale != st->sample_rate)) {
					nb_samp = gf_timestamp_rescale(nb_samp, st->timescale, st->sample_rate);
				}
				*nb_audio_samples_to_keep = (u32)nb_samp;
			}
			after = GF_TRUE;
		}
	}

	if (before) {
		if (!after)
			return 0;
		//long duration samples (typically text) can both start before and end after the target range
		else
			return 2;
	}
	if (after)
		return 2;
	return 1;
}

void reframer_purge_queues(GF_ReframerCtx *ctx, u64 ts, u32 timescale)
{
	u32 i, count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		RTStream *st = gf_list_get(ctx->streams, i);
		u64 ts_rescale = ts;
		if (st->reinsert_single_pck)
			continue;

		if (st->timescale != timescale) {
			ts_rescale *= st->timescale;
			ts_rescale /= timescale;
		}
		while (1) {
			GF_FilterPacket *pck = gf_list_get(st->pck_queue, 0);
			if (!pck) break;
			u64 dts = gf_filter_pck_get_dts(pck);
			if (dts==GF_FILTER_NO_TS)
				dts = gf_filter_pck_get_cts(pck);

			dts += gf_filter_pck_get_duration(pck);
			if (dts >= ts_rescale) break;
			gf_list_rem(st->pck_queue, 0);
			gf_filter_pck_unref(pck);
			st->nb_frames++;
		}
	}
}

//#define DEBUG_RF_MEM_USAGE

#ifdef DEBUG_RF_MEM_USAGE
static void reframer_dump_mem(GF_ReframerCtx *ctx, char *status)
{
	u32 i, count = gf_list_count(ctx->streams);
	u32 mem_pck = 0;
	u64 mem_size = 0;
	u32 mem_saps = 0;

	//check all streams have reached min ts
	for (i=0; i<count; i++) {
		u32 j, nb_pck;
		RTStream *st = gf_list_get(ctx->streams, i);
		nb_pck = gf_list_count(st->pck_queue);

		for (j=0; j<nb_pck; j++) {
			u32 size;
			GF_FilterPacket *pck = gf_list_get(st->pck_queue, j);
			gf_filter_pck_get_data(pck, &size);
			mem_size += size;
			if (gf_filter_pck_get_sap(pck)) mem_saps++;
			mem_pck++;
		}
	}
	fprintf(stderr, "%s, mem state: %d pck "LLU" KB %d saps\n", status, mem_pck, mem_size/1000, mem_saps);
}
#endif


static void check_gop_split(GF_ReframerCtx *ctx)
{
	u32 i, count = gf_list_count(ctx->streams);
	Bool flush_all = GF_FALSE;

	if (!ctx->min_ts_scale) {
		u64 min_ts = 0;
		u32 min_timescale=0;
		u64 min_ts_a = 0;
		u32 min_timescale_a=0;
		u32 nb_eos = 0;
		Bool has_empty_streams = GF_FALSE;
		Bool wait_for_sap = GF_FALSE;
		Bool size_split = (ctx->extract_mode==EXTRACT_SIZE) ? GF_TRUE : GF_FALSE;

		ctx->flush_max_ts = 0;
		for (i=0; i<count; i++) {
			u32 j, nb_pck, nb_sap;
			u64 last_sap_ts=0;
			u64 before_last_sap_ts=0;
			RTStream *st = gf_list_get(ctx->streams, i);
			nb_pck = gf_list_count(st->pck_queue);
			nb_sap = 0;
			if (st->in_eos) {
				nb_eos++;
				if (!nb_pck) {
					has_empty_streams = GF_TRUE;
					continue;
				}
			}

			for (j=0; j<nb_pck; j++) {
				u64 ts;
				GF_FilterPacket *pck;
				//size split, reverse walk the packet list
				if (size_split) {
					pck = gf_list_get(st->pck_queue, nb_pck - j - 1);
				} else {
					pck = gf_list_get(st->pck_queue, j);
				}
				if (!st->is_raw && !gf_filter_pck_get_sap(pck) ) {
					continue;
				}
				ts = gf_filter_pck_get_dts(pck);
				if (ts==GF_FILTER_NO_TS)
					ts = gf_filter_pck_get_cts(pck);
				ts += st->tk_delay;

				nb_sap++;
				//size split, get the last and before last SAP times
				if (size_split) {
					if (nb_sap == 1) {
						last_sap_ts = ts;
					}
					else if (nb_sap == 2) {
						before_last_sap_ts = ts;
						break;
					}
				}
				//regular split, get the second SAP time
				else {
					if (nb_sap <= 1) {
						continue;
					}
					last_sap_ts = ts;
					break;
				}
			}
			//size split, if only one SAP in queue, don't take any decision yet
			if (size_split && !before_last_sap_ts)
				last_sap_ts = 0;

			//in SAP split, flush as soon as we no longer have 2 consecutive saps
			if (!last_sap_ts) {
				if (st->in_eos && !flush_all && !st->reinsert_single_pck) {
					flush_all = GF_TRUE;
				} else if (!st->all_saps) {
					wait_for_sap = GF_TRUE;
				}
			}

			if (before_last_sap_ts) {
				if (!ctx->flush_max_ts || gf_timestamp_less(before_last_sap_ts, st->timescale, ctx->flush_max_ts, ctx->flush_max_ts_scale)) {
					ctx->flush_max_ts = before_last_sap_ts;
					ctx->flush_max_ts_scale = st->timescale;
				}
			}

			if (st->all_saps) {
				if (!min_ts_a || gf_timestamp_less(last_sap_ts, st->timescale, min_ts_a, min_timescale_a) ) {
					min_ts_a = last_sap_ts;
					min_timescale_a = st->timescale;
				}
			} else {
				if (!min_ts || gf_timestamp_less(last_sap_ts, st->timescale, min_ts, min_timescale) ) {
					min_ts = last_sap_ts;
					min_timescale = st->timescale;
				}
			}
		}

		//in size split, flush as soon as one stream is in eos
		if (nb_eos && has_empty_streams) {
			flush_all = GF_TRUE;
		}

		//if flush, get timestamp + dur of last packet in each stream and use this as final end time
		if (flush_all) {
			for (i=0; i<count; i++) {
				u64 ts;
				GF_FilterPacket *pck;
				RTStream *st = gf_list_get(ctx->streams, i);
				if (!st->in_eos)
					return;

				pck = gf_list_last(st->pck_queue);
				if (!pck) continue;
				u32 dur = gf_filter_pck_get_duration(pck);
				if (!dur) dur=1;
				ts = gf_filter_pck_get_dts(pck);
				if (ts==GF_FILTER_NO_TS)
					ts = gf_filter_pck_get_cts(pck);
				ts += st->tk_delay;
				ts += dur;
				if (!min_ts || gf_timestamp_greater(ts, st->timescale, min_ts, min_timescale) ) {
					min_ts = ts;
					min_timescale = st->timescale;
				}
			}
		}

		ctx->flush_samples = GF_FALSE;
		if (!min_ts) {
			//video not ready, need more input
			if (wait_for_sap)
				return;
			min_ts = min_ts_a;
			min_timescale = min_timescale_a;
		}
		if (!min_ts) {
			//other streams not ready, need more input
			if (nb_eos<count)
				return;
		} else {
			ctx->min_ts_scale = min_timescale;
			ctx->min_ts_computed = min_ts;
		}
	}
	//check all streams have reached min ts unless we are in final flush
	if (!flush_all) {
		for (i=0; i<count; i++) {
			u64 ts;
			GF_FilterPacket *pck;
			RTStream *st = gf_list_get(ctx->streams, i);
			if (st->range_start_computed==2) continue;
			if (st->reinsert_single_pck) continue;
			pck = gf_list_last(st->pck_queue);
			if (!pck) continue;
			ts = gf_filter_pck_get_dts(pck);
			if (ts==GF_FILTER_NO_TS)
				ts = gf_filter_pck_get_cts(pck);
			ts += st->tk_delay;

			if (gf_timestamp_less(ts, st->timescale, ctx->min_ts_computed, ctx->min_ts_scale)) {
				return;
			}
		}
	}

	//check condition
	if (ctx->extract_mode==EXTRACT_SIZE) {
		u32 nb_stop_at_min_ts = 0;
		u64 cumulated_size = ctx->cumulated_size;
		Bool use_prev = GF_FALSE;
		u32 nb_eos = 0;

		//check all streams have reached min ts
		for (i=0; i<count; i++) {
			u32 j, nb_pck;
			Bool found=GF_FALSE;
			RTStream *st = gf_list_get(ctx->streams, i);
			nb_pck = gf_list_count(st->pck_queue);

			for (j=0; j<nb_pck; j++) {
				u64 ts;
				u32 size;
				GF_FilterPacket *pck = gf_list_get(st->pck_queue, j);

				ts = gf_filter_pck_get_dts(pck);
				if (ts==GF_FILTER_NO_TS)
					ts = gf_filter_pck_get_cts(pck);
				ts += st->tk_delay;

				if ((ctx->prev_min_ts_computed < ctx->min_ts_computed)
					&& gf_timestamp_greater_or_equal(ts, st->timescale, ctx->min_ts_computed, ctx->min_ts_scale)
				) {
					nb_stop_at_min_ts ++;
					found = GF_TRUE;
					break;
				}
				gf_filter_pck_get_data(pck, &size);
				cumulated_size += size;
			}

			if ((j==nb_pck) && st->in_eos && !found) {
				nb_eos++;
			}
		}

		if ((nb_stop_at_min_ts + nb_eos) < count) {
			ctx->flush_samples = GF_FALSE;
		}

		//not done yet (estimated size less than target split)
		if (
			(cumulated_size < ctx->split_size)
			&& ctx->min_ts_scale
			//do this only if first time we estimate this chunk size, or if previous estimated min_ts is not the same as current min_ts
			&& (!ctx->prev_min_ts_computed || (ctx->prev_min_ts_computed < ctx->min_ts_computed))
		) {
			if ((nb_stop_at_min_ts + nb_eos) == count) {
				ctx->est_file_size = cumulated_size;
				//flush everything until prev_min_ts_computed
				if (ctx->flush_max_ts) {
#ifdef DEBUG_RF_MEM_USAGE
					reframer_dump_mem(ctx, "Requesting sample flush");
#endif
					ctx->flush_samples = GF_TRUE;
					//if first flush, setup streams
					if (!ctx->cumulated_size) {
						for (i=0; i<count; i++) {
							u64 ts;
							RTStream *st = gf_list_get(ctx->streams, i);
							GF_FilterPacket *pck = gf_list_get(st->pck_queue, 0);
							st->range_end_reached_ts = 0;
							st->first_pck_sent = GF_FALSE;

							if (pck) {
								ts = gf_filter_pck_get_dts(pck);
								if (ts==GF_FILTER_NO_TS)
									ts = gf_filter_pck_get_cts(pck);
								ts += st->tk_delay;
								st->ts_at_range_start_plus_one = ts + 1;
							}
						}
					}
				}
				ctx->prev_min_ts_computed = ctx->min_ts_computed;
				ctx->prev_min_ts_scale = ctx->min_ts_scale;
				ctx->min_ts_computed = 0;
				ctx->min_ts_scale = 0;
			}
			return;
		}

		if (ctx->min_ts_computed && (ctx->prev_min_ts_computed == ctx->min_ts_computed)) {
			//not end of stream, if size still not reached continue
			if (!nb_eos) {
				if (cumulated_size < ctx->split_size) {
					ctx->min_ts_computed = 0;
					ctx->min_ts_scale = 0;
					return;
				}
			} else if (nb_eos == count) {
				//end of stream, force final flush
				ctx->in_range = GF_TRUE;
				return;
			} else if (ctx->min_ts_computed == ctx->prev_min_ts_computed) {
				ctx->in_range = GF_TRUE;
				return;
			}
		}

		//decide which split size we use
		if (ctx->xround<=REFRAME_ROUND_SEEK) {
			use_prev = GF_TRUE;
		} else if (ctx->xround==REFRAME_ROUND_AFTER) {
			use_prev = GF_FALSE;
		} else {
			s64 diff_prev = (s64) ctx->split_size;
			s64 diff_cur = (s64) ctx->split_size;
			diff_prev -= (s64) ctx->est_file_size;
			diff_cur -= (s64) cumulated_size;
			if (ABS(diff_cur)<ABS(diff_prev))
				use_prev = GF_FALSE;
			else
				use_prev = GF_TRUE;
		}
		if (!ctx->prev_min_ts_scale)
			use_prev = GF_FALSE;

		if (use_prev) {
			//ctx->est_file_size = ctx->est_file_size;
			ctx->min_ts_computed = ctx->prev_min_ts_computed;
			ctx->min_ts_scale = ctx->prev_min_ts_scale;
		} else {
			ctx->est_file_size = cumulated_size;
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[Reframer] split computed using %s estimation of file size ("LLU")\n", use_prev ? "previous" : "current", ctx->est_file_size));
		ctx->prev_min_ts_computed = 0;
		ctx->prev_min_ts_scale = 0;
		ctx->flush_samples = GF_FALSE;
	}

	//good to go
	ctx->in_range = GF_TRUE;
	for (i=0; i<count; i++) {
		u64 ts;
		RTStream *st = gf_list_get(ctx->streams, i);
		GF_FilterPacket *pck = gf_list_get(st->pck_queue, 0);
		st->range_end_reached_ts = (ctx->min_ts_computed * st->timescale);
		if (ctx->min_ts_scale)
			st->range_end_reached_ts /= ctx->min_ts_scale;

		st->range_end_reached_ts += 1;

		if ((ctx->extract_mode==EXTRACT_SIZE) && ctx->cumulated_size)
			continue;

		st->first_pck_sent = GF_FALSE;
		if (pck) {
			ts = gf_filter_pck_get_dts(pck);
			if (ts==GF_FILTER_NO_TS)
				ts = gf_filter_pck_get_cts(pck);
			ts += st->tk_delay;
			st->ts_at_range_start_plus_one = ts + 1;
		} else {
			//this will be a eos signal
			st->range_end_reached_ts = 0;
			assert(st->range_start_computed==2);
		}
	}
	ctx->cur_end.num = ctx->min_ts_computed;
	ctx->cur_end.den = ctx->min_ts_scale;
	ctx->cumulated_size = 0;
}



GF_Err reframer_process(GF_Filter *filter)
{
	GF_ReframerCtx *ctx = gf_filter_get_udta(filter);
	u32 i, nb_eos, nb_end_of_range, count = gf_filter_get_ipid_count(filter);

	if (ctx->eos_state) {
		return (ctx->eos_state==2) ? GF_NOT_SUPPORTED : GF_EOS;
	}
	if (ctx->rt) {
		ctx->reschedule_in = 0;
		ctx->clock_val = gf_sys_clock_high_res();
	}

	/*active range, process as follows:
		- if stream is marked as "start reached" or "end reached" do nothing
		- queue up packets until we reach start range:
		- if packet is in range:
			- queue it (ref &nd detach from pid)
			- if pck is SAP and first SAP after start and context is not yet marked "in range":
				- check if we start from this SAP or from previous SAP (possibly before start) according to xround
				- and mark stream as "start ready"
				- if stream is video and xadjust is set, prevent all other stream processing
		- if packet is out of range
			- do NOT enqueue packet
			- if stream was not marked as "start ready" (no SAP in active range), use previous SAP before start and mark as active
			- mark as end of range reached
			- if stream is video and xadjust is set, re-enable all other stream processing

		Once all streams are marked as "start ready"
			- compute min time at which we will adjust the start range for all streams
			- purge all packets before this time
			- mark global context as "in range"

		The regular (non-range) process is then adjusted as follows:
			- if context is "in range" get packet from internal queue
			- if no more packets in internal queue, mark stream as "range done"

		Once all streams are marked as "range done"
			- adjust next_ts of each stream
			- mark each stream as not "start ready" and not "range done"
			- mark context as not "in range"
			- load next range and let the algo loop
	*/
	if (ctx->range_type && (ctx->range_type!=RANGE_DONE)) {
		u32 nb_start_range_reached = 0;
		u32 nb_not_playing = 0;
		Bool check_split = GF_FALSE;
		Bool check_sync = GF_TRUE;
		u32 nb_streams_fetched=0;

		for (i=0; i<count; i++) {
			GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
			RTStream *st = gf_filter_pid_get_udta(ipid);
			st->fetch_done = GF_FALSE;
		}

refetch_streams:
		//fetch input packets
		for (i=0; i<count; i++) {
			u64 ts, check_ts;
			u32 nb_audio_samples_to_keep = 0;
			u32 pck_in_range, dur;
			Bool is_sap;
			Bool drop_input = GF_TRUE;
			GF_FilterPacket *pck;
			GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
			RTStream *st = gf_filter_pid_get_udta(ipid);

			if (st->fetch_done) continue;

			if (!st->is_playing) {
				nb_start_range_reached++;
				nb_not_playing++;
				st->fetch_done = GF_TRUE;
				continue;
			}

			if (st->range_start_computed && !ctx->wait_video_range_adjust) {
				nb_start_range_reached++;
				st->fetch_done = GF_TRUE;
				continue;
			}
			//if eos is marked we are flushing so don't check range_end
			if (!ctx->has_seen_eos && st->range_end_reached_ts) {
				st->fetch_done = GF_TRUE;
				continue;
			}

			if (st->split_pck) {
				pck = st->split_pck;
				drop_input = GF_FALSE;
			} else {
				pck = gf_filter_pid_get_packet(ipid);
			}
			if (!pck) {
				if (gf_filter_pid_is_eos(ipid)) {
					//special case for PIDs with a single packet, we reinsert them at the beginning of each extracted range
					//this allows dealing with BIFS/OD/JPEG/PNG tracks
					if (st->reinsert_single_pck) {
						if (!ctx->in_range && !st->range_start_computed) {
							st->range_start_computed = 3;
							if (!gf_list_count(st->pck_queue)) {
								pck = st->reinsert_single_pck;
								gf_filter_pck_ref(&pck);
								gf_list_add(st->pck_queue, pck);
								if (!ctx->is_range_extraction) {
									check_split = GF_TRUE;
								}
							}
						}
						if (st->range_start_computed) {
							nb_start_range_reached++;
						}
						if (!ctx->is_range_extraction) {
							st->in_eos = GF_TRUE;
						}
						continue;
					}

					if (!ctx->is_range_extraction) {
						check_split = GF_TRUE;
						st->in_eos = GF_TRUE;
					} else {
						st->range_start_computed = 2;
						if (ctx->wait_video_range_adjust && ctx->xadjust && st->needs_adjust) {
							ctx->wait_video_range_adjust = GF_FALSE;
							ctx->flush_max_ts_scale = 0;
						}
					}
					if (st->range_start_computed) {
						nb_start_range_reached++;
					}
					//force flush in case of extract dur to avoid creating file with only a few samples of one track only
					if (st->is_playing && (ctx->extract_mode==EXTRACT_DUR)) {
						ctx->has_seen_eos = GF_TRUE;
					}
				}
				st->fetch_done = GF_TRUE;
				continue;
			}

			ts = gf_filter_pck_get_dts(pck);
			if (ts==GF_FILTER_NO_TS)
				ts = gf_filter_pck_get_cts(pck);
			ts += st->tk_delay;

			if (gf_timestamp_greater(ts, st->timescale, ctx->last_ts, 1000)) {
				if (check_sync) {
					if (st->range_start_computed) nb_start_range_reached++;
					continue;
				} else {
					ctx->last_ts = gf_timestamp_rescale(ts, st->timescale, 1000);
				}
			} else {
				st->fetch_done = GF_TRUE;
			}

			st->nb_frames_range++;
			//in range extraction we target the presentation time, use CTS and apply delay
			if (ctx->is_range_extraction) {
				check_ts = gf_filter_pck_get_cts(pck) + st->tk_delay;
				if (check_ts > st->ts_sub) check_ts -= st->ts_sub;
				else check_ts = 0;
			} else {
				check_ts = ts;
			}

			//if nosap is set, consider all packet SAPs
			if (ctx->nosap || st->is_raw) {
				is_sap = GF_TRUE;
			} else {
				GF_FilterSAPType sap = gf_filter_pck_get_sap(pck);
				if ((sap==GF_FILTER_SAP_1) || (sap==GF_FILTER_SAP_2) || (sap==GF_FILTER_SAP_3)) {
					is_sap = GF_TRUE;
				} else if ((st->stream_type==GF_STREAM_AUDIO) && (sap==GF_FILTER_SAP_4)) {
					is_sap = GF_TRUE;
				} else {
					is_sap = GF_FALSE;
				}
			}

			if (!is_sap) {
				if (st->all_saps) {
					st->all_saps = GF_FALSE;
					ctx->nb_non_saps++;
					if (ctx->nb_non_saps>1) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] %d streams using predictive coding, results may be undefined or broken when aligning SAP, consider remuxing the source\n", ctx->nb_non_saps));
					}

					if (ctx->xadjust) {
						st->needs_adjust = GF_TRUE;
						if (st->range_start_computed==1) {
							if (ctx->is_range_extraction) {
								ctx->wait_video_range_adjust = GF_TRUE;
							}
						}
					}
				}
			}

			//SAP or size split, push packet in queue and ask for gop split check
			if (!ctx->is_range_extraction) {
				if (gf_filter_pck_is_blocking_ref(pck)) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Reframer] cannot perform size/duration extraction with an input using blocking packet references (PID %s)\n\tCheck filter `%s` settings to allow for data copy\n", gf_filter_pid_get_name(st->ipid), gf_filter_pid_get_source_filter_name(st->ipid) ));
					ctx->eos_state = 2;
					return GF_NOT_SUPPORTED;
				}
				//add packet
				gf_filter_pck_ref(&pck);
				gf_filter_pid_drop_packet(st->ipid);
				gf_list_add(st->pck_queue, pck);
				check_split = GF_TRUE;
				//keep ref to first packet until we see a second one, except if blocking ref
				//if blocking ref we assume the source is sending enough packets and we won't reinsert any
				if (!gf_filter_pck_is_blocking_ref(pck) && (st->nb_frames_range==1)) {
					gf_filter_pck_ref(&pck);
					st->reinsert_single_pck = pck;
				} else if (st->reinsert_single_pck) {
					gf_filter_pck_unref(st->reinsert_single_pck);
					st->reinsert_single_pck = NULL;
				}
				nb_streams_fetched++;
				continue;
			}
			dur = gf_filter_pck_get_duration(pck);

			//dur split or range extraction but we wait for video end range to be adjusted, don't enqueue packet
			if (ctx->wait_video_range_adjust && !st->needs_adjust) {
				if (!ctx->flush_max_ts_scale
					|| gf_timestamp_greater_or_equal(ts, st->timescale, ctx->flush_max_ts, ctx->flush_max_ts_scale)
				) {
					if (st->range_start_computed) nb_start_range_reached++;
					continue;
				}
			}

			//check if packet is in our range
			pck_in_range = reframer_check_pck_range(filter, ctx, st, pck, check_ts, dur, st->nb_frames_range, &nb_audio_samples_to_keep);


			//SAP packet, decide if we cut here or at previous SAP
			if (is_sap) {
				//if streamtype is video or we have only one pid, purge all packets in all streams before this time
				//
				//for more complex cases we keep packets because we don't know if we will need SAP packets before the final
				//decided start range
				if (!pck_in_range && ((count==1) || !st->all_saps) ) {
					reframer_purge_queues(ctx, ts, st->timescale);
				}

				//packet in range and global context not yet in range, mark which SAP will be the beginning of our range
				if (!ctx->in_range && (pck_in_range==1)) {
					if (!st->range_start_computed) {
						u32 ts_adj = nb_audio_samples_to_keep;
						if (ts_adj && (st->sample_rate!=st->timescale)) {
							ts_adj *= st->timescale;
							ts_adj /= st->sample_rate;
						}

						if (ctx->xround==REFRAME_ROUND_CLOSEST) {
							Bool cur_closer = GF_FALSE;
							//check which frame is closer
							if (ctx->start_frame_idx_plus_one) {
								s64 diff_prev = ctx->start_frame_idx_plus_one-1;
								s64 diff_cur = ctx->start_frame_idx_plus_one-1;
								diff_prev -= st->prev_sap_frame_idx;
								diff_cur -= st->nb_frames_range;
								if (ABS(diff_cur) < ABS(diff_prev)) cur_closer = GF_TRUE;
							} else {
								s64 diff_prev, diff_cur;
								u64 start_range_ts = gf_timestamp_rescale(ctx->cur_start.num, ctx->cur_start.den, st->timescale);

								diff_prev = diff_cur = start_range_ts;
								diff_prev -= st->prev_sap_ts;
								diff_cur -= ts+ts_adj;
								if (ABS(diff_cur) < ABS(diff_prev)) cur_closer = GF_TRUE;
							}
							if (cur_closer) {
								st->sap_ts_plus_one = ts+ts_adj+1;
								st->nb_frames_until_start = 0;
							} else {
								st->sap_ts_plus_one = st->prev_sap_ts + 1;
							}
						} else if (ctx->xround<=REFRAME_ROUND_SEEK) {
							st->sap_ts_plus_one = st->prev_sap_ts+1;

							if ((ctx->extract_mode==EXTRACT_RANGE) && !ctx->start_frame_idx_plus_one) {
								u64 start_range_ts = gf_timestamp_rescale(ctx->cur_start.num, ctx->cur_start.den, st->timescale);
								if (ts + ts_adj == start_range_ts) {
									st->sap_ts_plus_one = ts+ts_adj+1;
									st->nb_frames_until_start = 0;
								}
							}
						} else {
							st->sap_ts_plus_one = ts+ts_adj+1;
							st->nb_frames_until_start = 0;
						}
						st->range_start_computed = 1;
					}
					nb_start_range_reached++;
				}
				//remember prev sap time
				ctx->flush_max_ts_scale = 0;
				if (pck_in_range!=2) {
					u64 before_prev_sap_ts = st->prev_sap_ts;
					st->prev_sap_ts = ts;
					st->prev_sap_frame_idx = st->nb_frames_range;
					if (!st->range_start_computed && (ctx->xround==REFRAME_ROUND_SEEK) )
						st->nb_frames_until_start = 1;

					if (ctx->wait_video_range_adjust && before_prev_sap_ts && !st->all_saps) {
						ctx->flush_max_ts = before_prev_sap_ts;
						ctx->flush_max_ts_scale = st->timescale;
					}
				}
				//video stream start and xadjust set, prevent all other streams from being processed until we determine the end of the video range
				//and re-enable other streams processing
				if (!ctx->wait_video_range_adjust && ctx->xadjust && st->needs_adjust && !st->all_saps) {
					ctx->wait_video_range_adjust = GF_TRUE;
				}
			} else if (st->range_start_computed) {
				nb_start_range_reached++;
			}

			if ((ctx->extract_mode==EXTRACT_DUR) && ctx->has_seen_eos && (pck_in_range==2))
				pck_in_range = 1;

			if (!pck_in_range && st->nb_frames_until_start)
				st->nb_frames_until_start++;

			//video stream and not adjusting to next SAP:, packet was not sap, probe for P/B reference frame :
			//we include this frame in the output but mark it as skip
			if (ctx->probe_ref && !ctx->xadjust && (pck_in_range==2) && (st->stream_type==GF_STREAM_VISUAL)) {
				if (!st->probe_ref_frame_ts) {
					pck_in_range = 1;
					st->probe_ref_frame_ts = ts+1;
				} else {
					ts = st->probe_ref_frame_ts - 1;
					st->probe_ref_frame_ts = 0;
				}
			}

			//after range: whether SAP or not, mark end of range reached
			if (pck_in_range==2) {
				if (!ctx->xadjust || is_sap) {
					Bool enqueue = GF_FALSE;
					st->split_end = 0;
					if (!st->range_start_computed) {
						st->sap_ts_plus_one = st->prev_sap_ts + 1;
						st->range_start_computed = 1;
						nb_start_range_reached++;
						//for UTC case, if range is empty discard everything
						if (!ctx->cur_start.den) {
							reframer_reset_stream(ctx, st, GF_FALSE);
						} else if (st->prev_sap_ts == ts) {
							enqueue = GF_TRUE;
						}
					}
					//remember the timestamp of first packet after range
					st->range_end_reached_ts = ts + 1;

					//time-based extraction or dur split, try to clone packet
					if (st->can_split && !ctx->start_frame_idx_plus_one) {
						if (gf_timestamp_less(ts, st->timescale, ctx->cur_end.num, ctx->cur_end.den)) {
							//force enqueing this packet
							enqueue = GF_TRUE;
							st->split_end = (u32) ( (ctx->cur_end.num * st->timescale) / ctx->cur_end.den - ts);
							st->range_end_reached_ts += st->split_end;
							//and remember it for next chunk - note that we dequeue the input to get proper eos notification
							gf_filter_pck_ref(&pck);
							st->split_pck = pck;
						}
					}
					else if (nb_audio_samples_to_keep && !ctx->start_frame_idx_plus_one) {
						enqueue = GF_TRUE;
						gf_filter_pck_ref(&pck);
						st->split_pck = pck;
						st->audio_samples_to_keep = nb_audio_samples_to_keep;
					}

					//video stream end detected and xadjust set, adjust cur_end to match the video stream end range
					//and re-enable other streams processing
					if (ctx->wait_video_range_adjust && ctx->xadjust && st->needs_adjust) {
						ctx->cur_end.num = st->range_end_reached_ts-1;
						ctx->cur_end.den = st->timescale;
						ctx->wait_video_range_adjust = GF_FALSE;
						ctx->flush_max_ts_scale = 0;
					}

					//do NOT enqueue packet
					if (!enqueue)
						break;
				}
			}
			nb_streams_fetched++;

			//add packet unless blocking ref
			if (gf_filter_pck_is_blocking_ref(pck) && !pck_in_range) {
				st->use_blocking_refs = GF_TRUE;
				if (drop_input)
					gf_filter_pid_drop_packet(st->ipid);
				continue;
			}

			gf_filter_pck_ref(&pck);
			gf_list_add(st->pck_queue, pck);
			if (drop_input) {
				gf_filter_pid_drop_packet(st->ipid);
				//keep ref to first packet until we see a second one, except if blocking ref
				//if blocking ref we assume the source is sending enough packets and we won't reinsert any
				if (!gf_filter_pck_is_blocking_ref(pck) && (st->nb_frames_range==1)) {
					gf_filter_pck_ref(&pck);
					st->reinsert_single_pck = pck;
				} else if (st->reinsert_single_pck) {
					gf_filter_pck_unref(st->reinsert_single_pck);
					st->reinsert_single_pck = NULL;
				}
			} else {
				assert(pck == st->split_pck);
				gf_filter_pck_unref(st->split_pck);
				st->split_pck = NULL;
			}
		}

		if (check_sync && !nb_streams_fetched && (!nb_start_range_reached || (nb_start_range_reached<count))) {
			check_sync = GF_FALSE;
			nb_start_range_reached=0;
			goto refetch_streams;
		}


		if (check_split) {
			check_gop_split(ctx);
		}

		//all streams reached the start range, compute min ts
		if (!ctx->in_range
			&& (nb_start_range_reached==count)
			&& (nb_not_playing<count)
			&& ctx->is_range_extraction
		) {
			u64 min_ts = 0;
			u32 min_timescale=0;
			u64 min_ts_a = 0;
			u32 min_timescale_a=0;
			u64 min_ts_split = 0;
			u32 min_timescale_split=0;
			Bool purge_all = GF_FALSE;
			for (i=0; i<count; i++) {
				GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
				RTStream *st = gf_filter_pid_get_udta(ipid);
				if (!st->is_playing) continue;
				assert(st->range_start_computed);
				//eos
				if (st->range_start_computed==2) {
					continue;
				}
				//packet will be reinserted at cut time, do not check its timestamp
				if (st->range_start_computed==3)
					continue;

				if (st->can_split) {
					if (!min_ts_split || gf_timestamp_less(st->sap_ts_plus_one-1, st->timescale, min_ts_split, min_timescale_split) ) {
						min_ts_split = st->sap_ts_plus_one;
						min_timescale_split = st->timescale;
					}
				}
				else if (st->all_saps) {
					if (!min_ts_a || gf_timestamp_less(st->sap_ts_plus_one-1, st->timescale, min_ts_a, min_timescale_a) ) {
						min_ts_a = st->sap_ts_plus_one;
						min_timescale_a = st->timescale;
					}
				} else {
					if (!min_ts || gf_timestamp_less(st->sap_ts_plus_one-1, st->timescale, min_ts, min_timescale) ) {
						min_ts = st->sap_ts_plus_one;
						min_timescale = st->timescale;
					}
				}
			}
			if (!min_ts) {
				min_ts = min_ts_a;
				min_timescale = min_timescale_a;
				if (!min_ts && min_ts_split) {
					if (ctx->start_frame_idx_plus_one) {
						min_ts = min_ts_split;
						min_timescale = min_timescale_split;
					} else {
						min_ts = ctx->cur_start.num+1;
						min_timescale = (u32) ctx->cur_start.den;
					}
				}
			}
			if (!min_ts) {
				purge_all = GF_TRUE;
				if (ctx->extract_mode==EXTRACT_RANGE) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] All streams in end of stream for desired start range "LLD"/"LLU"\n", ctx->cur_start.num, ctx->cur_start.den));
				}
				ctx->eos_state = 1;
			} else {
				min_ts -= 1;

				if (ctx->cur_start.den && (ctx->extract_mode==EXTRACT_RANGE) && (ctx->xround!=REFRAME_ROUND_SEEK)) {
					ctx->cur_start.num = min_ts;
					ctx->cur_start.den = min_timescale;
				}
			}
			//purge everything before min ts
			for (i=0; i<count; i++) {
				Bool start_found = GF_FALSE;
				GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
				RTStream *st = gf_filter_pid_get_udta(ipid);
				if (!st->is_playing) continue;

				while (gf_list_count(st->pck_queue)) {
					GF_FilterPacket *pck = gf_list_get(st->pck_queue, 0);
					if (!purge_all) {
						u32 is_start = 0;
						u64 ts, ots, check_ts;
						u64 dur, odur;
						Bool check_priming = GF_FALSE;
						ts = gf_filter_pck_get_dts(pck);
						if (ts==GF_FILTER_NO_TS)
							ts = gf_filter_pck_get_cts(pck);
						ts += st->tk_delay;
						odur = dur = (u64) gf_filter_pck_get_duration(pck);
						if (!dur) dur=1;

						check_ts = ots = ts;
						if (st->seek_mode && (st->stream_type==GF_STREAM_AUDIO) && st->ts_sub) {
							check_priming = GF_TRUE;
							check_ts += st->ts_sub;
						}

						if (min_timescale != st->timescale) {
							ts = gf_timestamp_rescale(ts, st->timescale, min_timescale);
							if (check_priming) {
								check_ts = gf_timestamp_rescale(check_ts, st->timescale, min_timescale);
							}
							dur = gf_timestamp_rescale(dur, st->timescale, min_timescale);
						}

						if (ts >= min_ts) {
							is_start = 1;
						}
						else if (st->can_split && (ts+dur > min_ts)) {
							is_start = 2;
						}
						else if (check_priming && (check_ts >= min_ts)) {
							is_start = 1;
						}
						else if ((st->abps || st->seek_mode) && (ts+dur >= min_ts) && (st->stream_type==GF_STREAM_AUDIO)) {
							u64 diff;
							diff = gf_timestamp_rescale(min_ts, min_timescale, st->timescale);
							diff -= ots;
							if (st->abps) {
								if (st->sample_rate != st->timescale) {
									diff = gf_timestamp_rescale(diff, st->timescale, st->sample_rate);
								}
							}
							if (diff < odur) {
								st->audio_samples_to_keep = (u32) diff;
								is_start = 1;
							}
						}
						else if (st->range_start_computed==3) {
							is_start = 1;
						}

						if (is_start) {
							//remember TS at range start
							s64 orig = min_ts;
							if (st->timescale != min_timescale) {
								orig = gf_timestamp_rescale(orig, min_timescale, st->timescale);
							}
							st->split_start = 0;
							if (is_start==2) {
								st->split_start = (u32) (min_ts - ts);
								if (min_timescale != st->timescale) {
									st->split_start = (u32) gf_timestamp_rescale(st->split_start, min_timescale, st->timescale);
								}
							}

							st->ts_at_range_start_plus_one = ots + 1;

							if ((st->range_start_computed==1)
								&& (orig < (s64) ots)
								&& ctx->splitrange
								&& (ctx->cur_range_idx>1)
							) {
								s64 delay = (s64) ots - (s64) orig;
								gf_filter_pid_set_property(st->opid, GF_PROP_PID_DELAY, &PROP_LONGSINT(delay) );
							}
							start_found = GF_TRUE;
							break;
						}
					}
					gf_list_rem(st->pck_queue, 0);
					gf_filter_pck_unref(pck);
					st->nb_frames++;
				}
				//we couldn't find a sample with dts >= to our min_ts - this happens when the min_ts
				//is located a few seconds AFTER the target split point
				//so force stream to reevaluate and enqueue more packets
				if (ctx->cur_start.den && !start_found && !st->use_blocking_refs) {
					st->range_start_computed = 0;
					return GF_OK;
				}
			}

			//OK every stream has now packets starting at the min_ts, ready to go
			const GF_PropertyValue *chap_times=NULL, *chap_names=NULL;
			ctx->nb_video_frames_since_start = 0;
			for (i=0; i<count; i++) {
				GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
				RTStream *st = gf_filter_pid_get_udta(ipid);
				//reset start range computed
				st->range_start_computed = 0;

				if (ctx->extract_mode==EXTRACT_DUR) {
					st->first_pck_sent = GF_FALSE;
				} else {
					st->first_pck_sent = ctx->splitrange ? GF_FALSE : GF_TRUE;
				}

				if (purge_all && (ctx->extract_mode!=EXTRACT_RANGE)) {
					gf_filter_pid_get_packet(st->ipid);
					gf_filter_pid_set_eos(st->opid);
				}

				if ((st->stream_type==GF_STREAM_VISUAL) && (st->nb_frames > ctx->nb_video_frames_since_start)) {
					ctx->nb_video_frames_since_start = st->nb_frames;
					if (st->nb_frames_until_start) {
						ctx->nb_video_frames_since_start += st->nb_frames_until_start-1;
						st->nb_frames_until_start = 0;
					}
				}

				if (!chap_times && ctx->cur_start.den) {
					chap_times = gf_filter_pid_get_property(st->ipid, GF_PROP_PID_CHAP_TIMES);
					chap_names = gf_filter_pid_get_property(st->ipid, GF_PROP_PID_CHAP_NAMES);
					if (!chap_times || !chap_names || (chap_times->value.uint_list.nb_items!=chap_names->value.string_list.nb_items))
						chap_times = chap_names = NULL;
				}
			}
			ctx->nb_video_frames_since_start_at_range_start = ctx->nb_video_frames_since_start;

			if (purge_all) {
				if (ctx->extract_mode!=EXTRACT_RANGE)
					return GF_EOS;

				goto load_next_range;
			}

			//we are in the range
			ctx->in_range = GF_TRUE;
#ifdef DEBUG_RF_MEM_USAGE
			reframer_dump_mem(ctx, "Starting new range");
#endif

			if (chap_times) {
				GF_PropertyValue ch;
				GF_PropUIntList new_times;
				GF_PropStringList new_names;
				u32 first_chap_idx=0, last_chap_idx=chap_times->value.uint_list.nb_items;
				u32 cut_time = (u32) gf_timestamp_rescale(min_ts, min_timescale, 1000);
				u32 out_time = 0;
				if (ctx->range_type == RANGE_CLOSED) {
					out_time = (u32) gf_timestamp_rescale(ctx->cur_end.num, ctx->cur_end.den, 1000);
				}

				for (i=0; i<chap_times->value.uint_list.nb_items; i++) {
					if (chap_times->value.uint_list.vals[i] <= cut_time)
						first_chap_idx = i;
					if (!out_time) continue;

					if (chap_times->value.uint_list.vals[i] == out_time)
						break;

					if (chap_times->value.uint_list.vals[i] > out_time) {
						last_chap_idx = i+1;
						break;
					}
				}
				new_times.nb_items = last_chap_idx - first_chap_idx;
				new_times.vals = gf_malloc(sizeof(u32)*new_times.nb_items);
				new_names.nb_items = new_times.nb_items;
				new_names.vals = gf_malloc(sizeof(char *)*new_names.nb_items);
				u32 k=0;
				for (i=first_chap_idx; i<last_chap_idx; i++) {
					s64 time = chap_times->value.uint_list.vals[i];
					time -= cut_time;
					if (time<0) time = 0;
					new_times.vals[k] = (u32) time;
					new_names.vals[k] = gf_strdup(chap_names->value.string_list.vals[i]);
					k++;
				}

				//set chapter properties
				for (i=0; i<count; i++) {
					GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
					RTStream *st = gf_filter_pid_get_udta(ipid);

					ch.type = GF_PROP_UINT_LIST;
					ch.value.uint_list = new_times;
					gf_filter_pid_set_property(st->opid, GF_PROP_PID_CHAP_TIMES, &ch);

					ch.type = GF_PROP_STRING_LIST_COPY;
					ch.value.string_list = new_names;
					gf_filter_pid_set_property(st->opid, GF_PROP_PID_CHAP_NAMES, &ch);
				}
				gf_free(new_times.vals);
				ch.type = GF_PROP_STRING_LIST;
				ch.value.string_list = new_names;
				gf_props_reset_single(&ch);
			}
		}
		if (!ctx->in_range && !ctx->flush_samples)
			return GF_OK;
	}

#ifdef DEBUG_RF_MEM_USAGE
	reframer_dump_mem(ctx, "Processing range");
#endif

	nb_eos = 0;
	nb_end_of_range = 0;
	for (i=0; i<count; i++) {
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		RTStream *st = gf_filter_pid_get_udta(ipid);

		while (1) {
			Bool forward = GF_TRUE;
			Bool pck_is_ref = GF_FALSE;
			GF_FilterPacket *pck;

			//dequeue packet
			if (ctx->range_type && (ctx->range_type!=RANGE_DONE) ) {
				pck = gf_list_get(st->pck_queue, 0);
				pck_is_ref = GF_TRUE;

				if (pck && !ctx->is_range_extraction && st->range_end_reached_ts) {
					u64 ts;
					ts = gf_filter_pck_get_dts(pck);
					if (ts==GF_FILTER_NO_TS)
						ts = gf_filter_pck_get_cts(pck);
					ts += st->tk_delay;
					if (ts>= st->range_end_reached_ts-1) {
						nb_end_of_range++;
						break;
					}
				}
			} else {
				pck = gf_filter_pid_get_packet(ipid);
			}

			if (!pck) {
				if (st->range_end_reached_ts) {
					nb_end_of_range++;
					break;
				}

				if (!st->is_playing) {
					nb_eos++;
				} else {
					//force a eos check if this was a split pid
					if (st->can_split)
						gf_filter_pid_get_packet(st->ipid);

					if (gf_filter_pid_is_eos(ipid)) {
						gf_filter_pid_set_eos(st->opid);
						nb_eos++;
					}
				}
				break;
			}
			//size split, flushing
			if (ctx->flush_samples) {
				u32 size;
				u64 cts;


				cts = gf_filter_pck_get_dts(pck);
				if (cts==GF_FILTER_NO_TS)
					cts = gf_filter_pck_get_cts(pck);
				cts += st->tk_delay;

				if (gf_timestamp_greater_or_equal(cts, st->timescale, ctx->flush_max_ts, ctx->flush_max_ts_scale)) {
					break;
				}
				gf_filter_pck_get_data(pck, &size);
				ctx->cumulated_size += size;
			}

			if (ctx->refs) {
				u8 deps = gf_filter_pck_get_dependency_flags(pck);
				deps >>= 2;
				deps &= 0x3;
				//not used as reference, don't forward
				if (deps==2)
					forward = GF_FALSE;
			}
			if (ctx->saps.nb_items) {
				u32 sap = gf_filter_pck_get_sap(pck);
				switch (sap) {
				case GF_FILTER_SAP_1:
					if (!ctx->filter_sap1) forward = GF_FALSE;
					break;
				case GF_FILTER_SAP_2:
					if (!ctx->filter_sap2) forward = GF_FALSE;
					break;
				case GF_FILTER_SAP_3:
					if (!ctx->filter_sap3) forward = GF_FALSE;
					break;
				case GF_FILTER_SAP_4:
				case GF_FILTER_SAP_4_PROL:
					if (!ctx->filter_sap4) forward = GF_FALSE;
					break;
				default:
					if (!ctx->filter_sap_none) forward = GF_FALSE;
					break;
				}
			}
			if (ctx->range_type==RANGE_DONE)
				forward = GF_FALSE;

			if (!forward) {
				reframer_drop_packet(ctx, st, pck, pck_is_ref);
				st->nb_frames++;
				continue;
			}

			if (! reframer_send_packet(filter, ctx, st, pck, pck_is_ref))
				break;

		}
	}

	//end of range
	if (nb_end_of_range + nb_eos == count) {
load_next_range:
		nb_end_of_range = 0;
		nb_eos=0;
		for (i=0; i<count; i++) {
			GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
			RTStream *st = gf_filter_pid_get_udta(ipid);
			//we reinsert the same PCK, so the ts_at_range_start_plus is always the packet cts
			//we therefore need to compute the ts at and as the target end time minus the target start time
			if (st->reinsert_single_pck && ctx->cur_start.den) {
				u64 start = gf_timestamp_rescale(ctx->cur_start.num, ctx->cur_start.den, st->timescale);
				//closed range, compute TS at range end
				if (ctx->cur_end.num && ctx->cur_end.den) {
					st->ts_at_range_end = gf_timestamp_rescale(ctx->cur_end.num, ctx->cur_end.den, st->timescale);
					st->ts_at_range_end -= start;
				}
			} else {
				st->ts_at_range_end += (st->range_end_reached_ts - 1)  - (st->ts_at_range_start_plus_one - 1);
			}
			st->ts_at_range_start_plus_one = 0;
			st->range_end_reached_ts = 0;
			st->range_start_computed = 0;
			if (st->in_eos) {
				if (gf_list_count(st->pck_queue)) {
					nb_end_of_range++;
				} else {
					gf_filter_pid_set_eos(st->opid);
					nb_eos++;
				}
			} else if (st->split_pck) {
				nb_end_of_range++;
			}
		}
		//and load next range
		ctx->in_range = GF_FALSE;
		reframer_load_range(ctx);
		if (nb_end_of_range)
			gf_filter_post_process_task(filter);
	}

	if (nb_eos==count) return GF_EOS;

	if (ctx->rt) {
		//while technically correct this increases the CPU load by shuffling the task around and querying gf_sys_clock_high_res too often
		//needs more investigation
		//using a simple callback every RT_PRECISION_US is a good workaround
#if 0
		u32 rsus = 0;
		if (ctx->reschedule_in > RT_PRECISION_US) {
			rsus = (u32) (ctx->reschedule_in - RT_PRECISION_US);
			if (rsus<RT_PRECISION_US) rsus = RT_PRECISION_US;
		} else if (ctx->reschedule_in>1000) {
			rsus = (u32) (ctx->reschedule_in / 2);
		}
		if (rsus) {
			gf_filter_ask_rt_reschedule(filter, rsus);
		}
#else
		if (ctx->reschedule_in) {
			gf_filter_ask_rt_reschedule(filter, RT_PRECISION_US);
		}
#endif
	}

	return GF_OK;
}

static const GF_FilterCapability ReframerCaps_RAW_AV[] =
{
	//raw audio and video only
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	//no restriction for media other than audio and video - cf regular caps for comments
	CAP_UINT(GF_CAPS_IN_OUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_IN_OUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_IN_OUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_IN_OUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT_LOADED_FILTER, GF_PROP_PID_CODECID, GF_CODECID_RAW)
};


static const GF_FilterCapability ReframerCaps_RAW_A[] =
{
	//raw audio only
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	//no restriction for media other than audio - cf regular caps for comments
	CAP_UINT(GF_CAPS_IN_OUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_IN_OUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_IN_OUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT_LOADED_FILTER, GF_PROP_PID_CODECID, GF_CODECID_RAW)
};


static const GF_FilterCapability ReframerCaps_RAW_V[] =
{
	//raw video only
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	//no restriction for media other than video - cf regular caps for comments
	CAP_UINT(GF_CAPS_IN_OUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_IN_OUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_IN_OUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT_LOADED_FILTER, GF_PROP_PID_CODECID, GF_CODECID_RAW)
};

static GF_Err reframer_initialize(GF_Filter *filter)
{
	GF_Err e;
	GF_ReframerCtx *ctx = gf_filter_get_udta(filter);

	ctx->streams = gf_list_new();
	ctx->seekable = GF_TRUE;
	if ((ctx->xs.nb_items>1) && (ctx->xround==REFRAME_ROUND_SEEK)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Reframer] `xround=seek` can only be used for single range extraction\n"));
		return GF_BAD_PARAM;
	}


	reframer_load_range(ctx);

	switch (ctx->raw) {
	case RAW_AV:
		e = gf_filter_override_caps(filter, ReframerCaps_RAW_AV, GF_ARRAY_LENGTH(ReframerCaps_RAW_AV));
		break;
	case RAW_VIDEO:
		e = gf_filter_override_caps(filter, ReframerCaps_RAW_V, GF_ARRAY_LENGTH(ReframerCaps_RAW_V));
		break;
	case RAW_AUDIO:
		e = gf_filter_override_caps(filter, ReframerCaps_RAW_A, GF_ARRAY_LENGTH(ReframerCaps_RAW_A));
		break;
	default:
		e = GF_OK;
	}
	return e;
}

static Bool reframer_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_ReframerCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterEvent fevt;
	RTStream *st;
	if (!evt->base.on_pid) return GF_FALSE;
	st = gf_filter_pid_get_udta(evt->base.on_pid);
	if (!st) return GF_TRUE;
	//if we have a PID, we always cancel the event and forward the same event to the associated input pid
	fevt = *evt;
	fevt.base.on_pid = st->ipid;

	//if range extraction based on time, adjust start range
	if (evt->base.type==GF_FEVT_PLAY) {
		if (ctx->range_type && !ctx->start_frame_idx_plus_one && ctx->cur_start.den) {
			Double start_range = (Double) ctx->cur_start.num;
			start_range /= ctx->cur_start.den;
			//rewind safety offset
			if (start_range > ctx->seeksafe)
				start_range -= ctx->seeksafe;
			else
				start_range = 0.0;

			fevt.play.start_range = start_range;
		}
		st->in_eos = GF_FALSE;
		st->is_playing = GF_TRUE;
		if (ctx->eos_state==1)
			ctx->eos_state = 0;
	} else if (evt->base.type==GF_FEVT_STOP) {
		st->is_playing = GF_FALSE;
	}

	gf_filter_pid_send_event(st->ipid, &fevt);
	return GF_TRUE;
}

static void reframer_finalize(GF_Filter *filter)
{
	GF_ReframerCtx *ctx = gf_filter_get_udta(filter);

	while (gf_list_count(ctx->streams)) {
		RTStream *st = gf_list_pop_back(ctx->streams);
		reframer_reset_stream(ctx, st, GF_TRUE);
	}
	gf_list_del(ctx->streams);
}

static const GF_FilterCapability ReframerCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	//we do accept everything, including raw streams 
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_UNFRAMED, GF_TRUE),
	//we don't accept files as input so don't output them
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	//we don't produce RAW streams during dynamic chain resolution - this will avoid loading the filter for compositor/other raw access
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	//but we may produce raw streams when filter is explicitly loaded (media exporter)
	CAP_UINT(GF_CAPS_OUTPUT_LOADED_FILTER|GF_CAPFLAG_OPTIONAL, GF_PROP_PID_CODECID, GF_CODECID_RAW)
};


#define OFFS(_n)	#_n, offsetof(GF_ReframerCtx, _n)
static const GF_FilterArgs ReframerArgs[] =
{
	{ OFFS(exporter), "compatibility with old exporter, displays export results", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(rt), "real-time regulation mode of input\n"
	"- off: disables real-time regulation\n"
	"- on: enables real-time regulation, one clock per PID\n"
	"- sync: enables real-time regulation one clock for all PIDs", GF_PROP_UINT, "off", "off|on|sync", GF_FS_ARG_HINT_NORMAL},
	{ OFFS(saps), "list of SAP types (0,1,2,3,4) to forward, other packets are dropped (forwarding only sap 0 will break the decoding)", GF_PROP_UINT_LIST, NULL, "0|1|2|3|4", GF_FS_ARG_HINT_NORMAL},
	{ OFFS(refs), "forward only frames used as reference frames, if indicated in the input stream", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_NORMAL},
	{ OFFS(speed), "speed for real-time regulation mode", GF_PROP_DOUBLE, "1.0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(raw), "force input AV streams to be in raw format\n"
	"- no: do not force decoding of inputs\n"
	"- av: force decoding of audio and video inputs\n"
	"- a: force decoding of audio inputs\n"
	"- v: force decoding of video inputs", GF_PROP_UINT, "no", "av|a|v|no", GF_FS_ARG_HINT_NORMAL},
	{ OFFS(frames), "drop all except listed frames (first being 1)", GF_PROP_UINT_LIST, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(xs), "extraction start time(s)", GF_PROP_STRING_LIST, NULL, NULL, GF_FS_ARG_HINT_NORMAL},
	{ OFFS(xe), "extraction end time(s). If less values than start times, the last time interval extracted is an open range", GF_PROP_STRING_LIST, NULL, NULL, GF_FS_ARG_HINT_NORMAL},
	{ OFFS(xround), "adjust start time of extraction range to I-frame\n"
	"- before: use first I-frame preceding or matching range start\n"
	"- seek: see filter help\n"
	"- after: use first I-frame (if any) following or matching range start\n"
	"- closest: use I-frame closest to range start", GF_PROP_UINT, "before", "before|seek|after|closest", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(xadjust), "adjust end time of extraction range to be before next I-frame", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(xots), "keep original timestamps after extraction", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(nosap), "do not cut at SAP when extracting range (may result in broken streams)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(splitrange), "signal file boundary at each extraction first packet for template-base file generation", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(seeksafe), "rewind play requests by given seconds (to make sure the I-frame preceding start is catched)", GF_PROP_DOUBLE, "10.0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(tcmdrw), "rewrite TCMD samples when splitting", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(props), "extra output PID properties per extraction range", GF_PROP_STRING_LIST, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(no_audio_seek), "disable seek mode on audio streams (no change of priming duration)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(probe_ref), "allow extracted range to be longer in case of B-frames with reference frames presented outside of range", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(utc_ref), "set reference mode for UTC range extraction\n"
	"- local: use UTC of local host\n"
	"- any: use UTC of media, or UTC of local host if not found in media after probing time\n"
	"- media: use UTC of media (abort if none found)", GF_PROP_UINT, "any", "local|any|media", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(utc_probe), "timeout in milliseconds to try to acquire UTC reference from media", GF_PROP_UINT, "5000", NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};

GF_FilterRegister ReframerRegister = {
	.name = "reframer",
	GF_FS_SET_DESCRIPTION("Media Reframer")
	GF_FS_SET_HELP("This filter provides various tools on inputs:\n"
		"- ensure reframing (1 packet = 1 Access Unit)\n"
		"- optionally force decoding\n"
		"- real-time regulation\n"
		"- packet filtering based on SAP types or frame numbers\n"
		"- time-range extraction and splitting\n"
		"  \n"
		"This filter forces input PIDs to be properly framed (1 packet = 1 Access Unit).\n"
		"It is typically needed to force remultiplexing in file to file operations when source and destination files use the same format.\n"
		"  \n"
		"# SAP filtering\n"
		"The filter can remove packets based on their SAP types using [-saps]() option.\n"
		"For example, this can be used to extract only the key frame (SAP 1,2,3) of a video to create a trick mode version.\n"
		"  \n"
		"# Frame filtering\n"
		"This filter can keep only specific Access Units of the source using [-frames]() option.\n"
		"For example, this can be used to extract only specific key pictures of a video to create a HEIF collection.\n"
		"  \n"
		"# Frame decoding\n"
		"This filter can force input media streams to be decoded using the [-raw]() option.\n"
		"EX gpac -i m.mp4 reframer:raw=av [dst]\n"
		"# Real-time Regulation\n"
		"The filter can perform real-time regulation of input packets, based on their timescale and timestamps.\n"
		"For example to simulate a live DASH:\n"
		"EX gpac -i m.mp4 reframer:rt=on -o live.mpd:dynamic\n"
		"  \n"
		"# Range extraction\n"
		"The filter can perform time range extraction of the source using [-xs]() and [-xe]() options.\n"
		"The formats allowed for times specifiers are:\n"
		"- 'T'H:M:S, 'T'M:S: specify time in hours, minutes, seconds\n"
		"- 'T'H:M:S.MS, 'T'M:S.MS, 'T'S.MS: specify time in hours, minutes, seconds and milliseconds\n"
		"- INT, FLOAT, NUM/DEN: specify time in seconds (number or fraction)\n"
		"- 'D'INT, 'D'FLOAT, 'D'NUM/DEN: specify end time as offset to start time in seconds (number or fraction) - only valid for [-xe]()\n"
		"- 'F'NUM: specify time as frame number\n"
		"- XML DateTime: specify absolute UTC time\n"
		"  \n"
		"In this mode, the timestamps are rewritten to form a continuous timeline, unless [-xots]() is set.\n"
		"When multiple ranges are given, the filter will try to seek if needed and supported by source.\n"
		"\n"
		"EX gpac -i m.mp4 reframer:xs=T00:00:10,T00:01:10,T00:02:00:xe=T00:00:20,T00:01:20 [dst]\n"
		"This will extract the time ranges [10s,20s], [1m10s,1m20s] and all media starting from 2m\n"
		"\n"
		"If no end range is found for a given start range:\n"
		"- if a following start range is set, the end range is set to this next start\n"
		"- otherwise, the end range is open\n"
		"\n"
		"EX gpac -i m.mp4 reframer:xs=0,10,25:xe=5,20 [dst]\n"
		"This will extract the time ranges [0s,5s], [10s,20s] and all media starting from 25s\n"
		"EX gpac -i m.mp4 reframer:xs=0,10,25 [dst]\n"
		"This will extract the time ranges [0s,10s], [10s,25s] and all media starting from 25s\n"
		"\n"
		"It is possible to signal range boundaries in output packets using [-splitrange]().\n"
		"This will expose on the first packet of each range in each PID the following properties:\n"
		"- `FileNumber`: starting at 1 for the first range, to be used as replacement for $num$ in templates\n"
		"- `FileSuffix`: corresponding to `StartRange_EndRange` or `StartRange` for open ranges, to be used as replacement for $FS$ in templates\n"
		"\n"
		"EX gpac -i m.mp4 reframer:xs=T00:00:10,T00:01:10:xe=T00:00:20:splitrange -o dump_$FS$.264 [dst]\n"
		"This will create two output files dump_T00.00.10_T00.02.00.264 and dump_T00.01.10.264.\n"
		"Note: The `:` and `/` characters are replaced by `.` in `FileSuffix` property.\n"
		"\n"
		"It is possible to modify PID properties per range using [-props](). Each set of property must be specified using the active separator set.\n"
		"Warning: The option must be escaped using double separators in order to be parsed properly.\n"
		"EX gpac -i m.mp4 reframer:xs=0,30::props=#Period=P1,#Period=P2:#foo=bar [dst]\n"
		"This will assign to output PIDs\n"
		"- during the range [0,30]: property `Period` to `P1`\n"
		"- during the range [30, end]: properties `Period` to `P2` and property `foo` to `bar`\n"
		"\n"
		"For uncompressed audio PIDs, input frame will be split to closest audio sample number.\n"
		"\n"
		"When [-xround]() is set to `seek`, the following applies:\n"
		"- a single range shall be specified\n"
		"- the first I-frame preceding or matching the range start is used as split point\n"
		"- all packets before range start are marked as seek points\n"
		"- packets overlapping range start are forwarded with a `SkipBegin` property set to the amount of media to skip\n"
		"- packets overlapping range end are forwarded with an adjusted duration to match the range end\n"
		"This mode is typically used to extract a range in a frame/sample accurate way, rather than a GOP-aligned way.\n"
		"\n"
		"When [-xround]() is not set to `seek`, compressed audio streams will still use seek mode.\n"
		"Consequently, these streams will have modified edit lists in ISOBMFF which might not be properly handled by players.\n"
		"This can be avoided using [-no_audio_seek](), but this will introduce audio delay.\n"
		"\n"
		"# UTC-based range extraction\n"
		"The filter can perform range extraction based on UTC time rather than media time. In this mode, the end time must be:\n"
		"- a UTC date: range extraction will stop after this date\n"
		"- a time in second: range extraction will stop after the specified duration\n"
		"\n"
		"The UTC reference is specified using [-utc_ref]().\n"
		"If UTC signal from media source is used, the filter will probe for [-utc_probe]() before considering the source has no UTC signal.\n"
		"\n"
		"The properties `SenderNTP` and, if absent, `UTC` of source packets are checked for establishing the UTC reference.\n"
		"# Other split actions\n"
		"The filter can perform splitting of the source using [-xs]() option.\n"
		"The additional formats allowed for [-xs]() option are:\n"
		"- `SAP`: split source at each SAP/RAP\n"
		"- `D`VAL: split source by chunks of `VAL` seconds\n"
		"- `D`NUM/DEN: split source by chunks of `NUM/DEN` seconds\n"
		"- `S`VAL: split source by chunks of estimated size `VAL` bytes (can use property multipliers, e.g. `m`)\n"
		"\n"
		"Note: In these modes, [-splitrange]() and [-xadjust]() are implicitly set.\n"
		"\n"
	)
	.private_size = sizeof(GF_ReframerCtx),
	.max_extra_pids = (u32) -1,
	.args = ReframerArgs,
	//reframer is explicit only, so we don't load the reframer during resolution process
	.flags = GF_FS_REG_EXPLICIT_ONLY|GF_FS_REG_ALLOW_CYCLIC,
	SETCAPS(ReframerCaps),
	.initialize = reframer_initialize,
	.finalize = reframer_finalize,
	.configure_pid = reframer_configure_pid,
	.process = reframer_process,
	.process_event = reframer_process_event,
};


const GF_FilterRegister *reframer_register(GF_FilterSession *session)
{
	return &ReframerRegister;
}
