/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2020
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
	EXTRACT_NONE=0,
	EXTRACT_RANGE,
	EXTRACT_SAP,
	EXTRACT_SIZE,
	EXTRACT_DUR,
};

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

	u64 ts_at_range_start_plus_one;
	u64 ts_at_range_end;
	s64 cts_init;

	GF_List *pck_queue;
	//0: not comuted, 1: computed and valid TS, 2: end of stream on pid
	u32 range_start_computed;
	u64 range_end_reached_ts;
	u64 prev_sap_ts;
	u32 prev_sap_frame_idx;
	u32 nb_frames_range;
	u64 sap_ts_plus_one;
	Bool first_pck_sent;

	u32 tk_delay;
	Bool in_eos;
	u32 split_start;
	u32 split_end;

	GF_FilterPacket *split_pck;
	GF_FilterPacket *resinsert_single_pck;
	Bool is_playing;
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
	Bool raw;
	GF_List *xs, *xe;
	Bool nosap, splitrange, xadjust;
	u32 xround;
	Double seeksafe;

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
	GF_Fraction64 cur_start, cur_end;
	u64 start_frame_idx_plus_one, end_frame_idx_plus_one;

	Bool timing_initialized;
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
	u32 gop_depth;

	u32 wait_video_range_adjust;
	Bool is_eos, has_seen_eos;
	u32 nb_non_saps;
} GF_ReframerCtx;

static void reframer_reset_stream(GF_ReframerCtx *ctx, RTStream *st)
{
	if (st->pck_queue) {
		while (gf_list_count(st->pck_queue)) {
			GF_FilterPacket *pck = gf_list_pop_front(st->pck_queue);
			gf_filter_pck_unref(pck);
		}
		gf_list_del(st->pck_queue);
	}
	if (st->split_pck) gf_filter_pck_unref(st->split_pck);
	if (st->resinsert_single_pck) gf_filter_pck_unref(st->resinsert_single_pck);
	gf_free(st);
}
GF_Err reframer_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 i;
	const GF_PropertyValue *p;
	GF_ReframerCtx *ctx = gf_filter_get_udta(filter);
	RTStream *st = gf_filter_pid_get_udta(pid);

	if (is_remove) {
		if (st) {
			gf_filter_pid_remove(st->opid);
			gf_list_del_item(ctx->streams, st);
			reframer_reset_stream(ctx, st);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (st) {
		gf_filter_pid_reset_properties(st->opid);
	} else {
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
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(st->opid, pid);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) st->timescale = p->value.uint;
	else st->timescale = 1000;

	if (!st->all_saps) {
		ctx->nb_non_saps--;
		st->all_saps = GF_TRUE;
	}
	st->can_split = GF_FALSE;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (p) {
		switch (p->value.uint) {
		case GF_STREAM_TEXT:
			st->can_split = GF_TRUE;
			break;
		}
	}
	st->needs_adjust = ctx->xadjust ? GF_TRUE : GF_FALSE;

	st->tk_delay = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	if (p) {
		//delay negative is skip: this is CTS adjustment for B-frames: we keep that notif in the stream
		if (p->value.sint<0) {
			st->tk_delay = 0;
		}
		//delay positive is delay, we keep the value for RT regulation and range
		else {
			st->tk_delay = (u32) p->value.sint;
			//if range processing, we drop frames not in the target playback range so do not forward delay
			if (ctx->range_type) {
				gf_filter_pid_set_property(st->opid, GF_PROP_PID_DELAY, NULL);
			}
		}
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_PLAYBACK_MODE);
	if (!p || (p->value.uint < GF_PLAYBACK_MODE_SEEK))
		ctx->seekable = GF_FALSE;


	ctx->filter_sap1 = ctx->filter_sap2 = ctx->filter_sap3 = ctx->filter_sap4 = ctx->filter_sap_none = GF_FALSE;
	for (i=0; i<ctx->saps.nb_items; i++) {
		switch (ctx->saps.vals[i]) {
		case 1: ctx->filter_sap1 = GF_TRUE; break;
		case 2: ctx->filter_sap2 = GF_TRUE; break;
		case 3: ctx->filter_sap3 = GF_TRUE; break;
		case 4: ctx->filter_sap4 = GF_TRUE; break;
		default: ctx->filter_sap_none = GF_TRUE; break;
		}
	}
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	return GF_OK;
}

static Bool reframer_parse_date(char *date, GF_Fraction64 *value, u64 *frame_idx_plus_one, u32 *extract_mode)
{
	u64 v;
	value->num  =0;
	value->den = 0;

	if (extract_mode)
		*extract_mode = EXTRACT_RANGE;

	if (date[0] == 'T') {
		u32 h=0, m=0, s=0, ms=0;
		if (strchr(date, '.')) {
			if (sscanf(date, "T%u:%u:%u.%u", &h, &m, &s, &ms) != 4) {
				if (sscanf(date, "T%u:%u.%u", &m, &s, &ms) != 3) {
					goto exit;
				}
			}
			if (ms>=1000) ms=0;
		} else {
			if (sscanf(date, "T%u:%u:%u", &h, &m, &s) != 3) {
				goto exit;
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
	if (sscanf(date, LLD"/"LLU, &value->num, &value->den)==2) {
		return GF_TRUE;
	}
	if (sscanf(date, LLU, &v)==1) {
		value->num = v;
		value->den = 1000;
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
		if (sscanf(date+1, LLD"/"LLU, &value->num, &value->den)==2) {
			return GF_TRUE;
		}
		if (sscanf(date+1, LLU, &v)==1) {
			value->num = v;
			value->den = 1000;
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

exit:
	GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] Unrecognized date format %s, expecting TXX:XX:XX[.XX], INT or FRAC\n", date));
	if (extract_mode)
		*extract_mode = EXTRACT_NONE;
	return GF_FALSE;
}

static void reframer_load_range(GF_ReframerCtx *ctx)
{
	u32 i, count;
	Bool do_seek = ctx->seekable;
	u64 prev_frame = ctx->start_frame_idx_plus_one;
	GF_Fraction64 prev_end;
	char *start_date=NULL, *end_date=NULL;

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

	count = gf_list_count(ctx->xs);
	if (!count) {
		if (ctx->range_type) goto range_done;
		return;
	}
	if (ctx->cur_range_idx>=count) {
		goto range_done;
	} else {
		start_date = gf_list_get(ctx->xs, ctx->cur_range_idx);
		end_date = gf_list_get(ctx->xe, ctx->cur_range_idx);
	}
	if (!start_date)
		goto range_done;

	ctx->cur_range_idx++;
	if (!end_date) ctx->range_type = RANGE_OPEN;
	else ctx->range_type = RANGE_CLOSED;

	if (!reframer_parse_date(start_date, &ctx->cur_start, &ctx->start_frame_idx_plus_one, &ctx->extract_mode)) {
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
		if (!reframer_parse_date(end_date, &ctx->cur_end, &ctx->end_frame_idx_plus_one, NULL)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] cannot parse end date, assuming open range\n"));
			ctx->range_type = RANGE_OPEN;
		}
	}

	//reset realtime range and issue seek requests
	if (ctx->rt || do_seek) {
		Double start_range = 0;
		if (do_seek) {
			start_range = (Double) ctx->cur_start.num;
			start_range /= ctx->cur_start.den;
			if (start_range > ctx->seeksafe)
				start_range -= ctx->seeksafe;
			else
				start_range = 0;
			ctx->has_seen_eos = GF_FALSE;
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
			}
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

static void reframer_update_ts_shift(GF_ReframerCtx *ctx, s64 diff_ts, u32 timescale)
{
	u32 i, count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		s64 ts_shift;
		RTStream *st = gf_list_get(ctx->streams, i);
		ts_shift = diff_ts;
		ts_shift *= st->timescale;
		ts_shift /= timescale;
		st->cts_init -= ts_shift;
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
			u64 clock = ctx->clock_val;
			cts_us += st->tk_delay;

			cts_us *= 1000000;
			cts_us /= st->timescale;
			if (ctx->rt==REFRAME_RT_SYNC) {
				if (!ctx->clock) ctx->clock = st;

				st = ctx->clock;
			}
			if (!st->sys_clock_at_init) {
				st->cts_us_at_init = cts_us;
				st->sys_clock_at_init = clock;
				do_send = GF_TRUE;
			} else if (cts_us < st->cts_us_at_init) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] CTS less than CTS used to initialize clock, not delaying\n"));
				do_send = GF_TRUE;
			} else {
				u64 diff = cts_us - st->cts_us_at_init;
				if (ctx->speed>0) diff = (u64) ( diff / ctx->speed);
				else if (ctx->speed<0) diff = (u64) ( diff / -ctx->speed);

				clock -= st->sys_clock_at_init;
				if (clock + 1000 >= diff) {
					do_send = GF_TRUE;
					GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Reframer] Sending packet "LLU" us too late (clock diff "LLU" - CTS diff "LLU")\n", 1000+clock - diff, clock, diff));
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
		s64 ts;
		GF_FilterPacket *new_pck = gf_filter_pck_new_ref(st->opid, NULL, 0, pck);
		gf_filter_pck_merge_properties(pck, new_pck);

		//signal chunk start boundary
		if (!st->first_pck_sent) {
			u32 i, len;
			char *file_suf_name = NULL;
			char *start = gf_list_get(ctx->xs, ctx->cur_range_idx-1);
			char *end = NULL;
			if (ctx->range_type==1) end = gf_list_get(ctx->xe, ctx->cur_range_idx-1);
			st->first_pck_sent = GF_TRUE;

			if (ctx->extract_mode==EXTRACT_RANGE) {
				gf_filter_pck_set_property(new_pck, GF_PROP_PCK_FILENUM, &PROP_UINT(ctx->cur_range_idx) );
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
			} else {
				u64 start_t, end_t;
				char szFileSuf[1000];
				start_t = ctx->cur_start.num * 1000;
				start_t /= ctx->cur_start.den;
				end_t = ctx->cur_end.num * 1000;
				end_t /= ctx->cur_end.den;

				gf_filter_pck_set_property(new_pck, GF_PROP_PCK_FILENUM, &PROP_UINT(ctx->file_idx) );
				sprintf(szFileSuf, LLU"-"LLU, start_t, end_t);
				gf_filter_pck_set_property(new_pck, GF_PROP_PCK_FILESUF, &PROP_STRING(szFileSuf) );
			}
		}

		//rewrite timestamps
		ts = gf_filter_pck_get_cts(pck);

		if (ts != GF_FILTER_NO_TS) {
			ts += st->tk_delay;
			ts += st->ts_at_range_end;
			ts -= st->ts_at_range_start_plus_one - 1;
			ts -= st->cts_init;
			if (ts<0) {
				reframer_update_ts_shift(ctx, -ts, st->timescale);
				ts = 0;
			}

			gf_filter_pck_set_cts(new_pck, (u64) ts);
			if (ctx->raw) {
				gf_filter_pck_set_dts(new_pck, ts);
			}
		}
		if (!ctx->raw) {
			ts = gf_filter_pck_get_dts(pck);
			if (ts != GF_FILTER_NO_TS) {
				ts += st->tk_delay;
				ts -= st->ts_at_range_start_plus_one - 1;
				ts += st->ts_at_range_end;
				gf_filter_pck_set_dts(new_pck, (u64) ts);
			}
		}
		if (st->split_start) {
			u32 dur = gf_filter_pck_get_duration(pck);
			assert(dur>st->split_start);
			dur -= st->split_start;
			gf_filter_pck_set_duration(new_pck, dur);
			st->ts_at_range_start_plus_one += st->split_start;
			st->split_start = 0;
		}
		//last packet and forced duration
		if (st->split_end && (gf_list_count(st->pck_queue)==1)) {
			gf_filter_pck_set_duration(new_pck, st->split_end);
			st->split_end = 0;
		}

		gf_filter_pck_send(new_pck);
	} else {
		gf_filter_pck_forward(pck, st->opid);
	}

	reframer_drop_packet(ctx, st, pck, pck_is_ref);
	st->nb_frames++;
	return GF_TRUE;
}

static Bool reframer_init_timing(GF_Filter *filter, GF_ReframerCtx *ctx)
{
	u32 i, count = gf_filter_get_ipid_count(filter);
	u64 min_dts_plus_one = 0;
	u32 pid_scale=0;
	for (i=0; i<count; i++) {
		u64 ts;
		GF_FilterPacket *pck;
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		RTStream *st = ipid ? gf_filter_pid_get_udta(ipid) : NULL;
		if (!st) continue;
		
		pck = gf_filter_pid_get_packet(ipid);
		if (!pck) {
			//not ready yet
			if (st->is_playing && !gf_filter_pid_is_eos(ipid))
				return GF_FALSE;
			//not playing or eos
			continue;
		}
		ts = gf_filter_pck_get_dts(pck);
		ts += st->tk_delay;

		if (!min_dts_plus_one) {
			min_dts_plus_one = 1 + ts;
			pid_scale = st->timescale;
		} else if (ts * pid_scale < (min_dts_plus_one-1) * st->timescale) {
			min_dts_plus_one = 1 + ts;
			pid_scale = st->timescale;
		}
	}
	for (i=0; i<count; i++) {
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		RTStream *st = ipid ? gf_filter_pid_get_udta(ipid) : NULL;
		if (!st) continue;

		if (!pid_scale) pid_scale = st->timescale;

		st->cts_init = min_dts_plus_one-1;
		if (st->timescale != pid_scale) {
			st->cts_init *= st->timescale;
			st->cts_init /= pid_scale;
		}
	}
	ctx->timing_initialized = GF_TRUE;
	return GF_TRUE;
}

static u32 reframer_check_pck_range(GF_ReframerCtx *ctx, RTStream *st, u64 ts, u32 dur, u32 frame_idx)
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
	} else {
		Bool before = GF_FALSE;
		Bool after = GF_FALSE;

		//ts not after our range start
		if ((s64) (ts * ctx->cur_start.den) < ctx->cur_start.num * st->timescale) {
			before = GF_TRUE;
		}
		//closed range, check
		if ((ctx->range_type!=RANGE_OPEN) && ((s64) ((ts+dur) * ctx->cur_end.den) >= ctx->cur_end.num * st->timescale)) {
			after = GF_TRUE;
		}
		if (before) {
			if (!after)
				return 0;
			//long duration samples (typically text) can both start before and end after the target range
			else
				return 2;
		}
		if (after) return 2;
		return 1;
	}
	return 0;
}

void reframer_purge_queues(GF_ReframerCtx *ctx, u64 ts, u32 timescale)
{
	u32 i, count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		RTStream *st = gf_list_get(ctx->streams, i);
		u64 ts_rescale = ts;
		if (st->resinsert_single_pck)
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
		for (i=0; i<count; i++) {
			u32 j, nb_pck, nb_sap;
			u64 last_sap_ts=0;
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
				GF_FilterPacket *pck = gf_list_get(st->pck_queue, j);
				if (!ctx->raw && !gf_filter_pck_get_sap(pck) ) {
					continue;
				}
				ts = gf_filter_pck_get_dts(pck);
				if (ts==GF_FILTER_NO_TS)
					ts = gf_filter_pck_get_cts(pck);
				ts += st->tk_delay;

				nb_sap++;
				if (nb_sap <= 1 + ctx->gop_depth) {
					continue;
				}

				last_sap_ts = ts;
				break;
			}
			//in SAP split, flush as soon as we no longer have 2 consecutive saps
			if (!last_sap_ts) {
				if (st->in_eos && !flush_all && !st->resinsert_single_pck) {
					flush_all = GF_TRUE;
				} else if (!st->all_saps) {
					wait_for_sap = GF_TRUE;
				}
			}

			if (st->all_saps) {
				if (!min_ts_a || (last_sap_ts * min_timescale_a < min_ts_a * st->timescale) ) {
					min_ts_a = last_sap_ts;
					min_timescale_a = st->timescale;
				}
			} else {
				if (!min_ts || (last_sap_ts * min_timescale < min_ts * st->timescale) ) {
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
				if (!min_ts || (ts * min_timescale > min_ts * st->timescale) ) {
					min_ts = ts;
					min_timescale = st->timescale;
				}
			}
		}

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
			if (st->resinsert_single_pck) continue;
			pck = gf_list_last(st->pck_queue);
			assert(pck);
			ts = gf_filter_pck_get_dts(pck);
			if (ts==GF_FILTER_NO_TS)
				ts = gf_filter_pck_get_cts(pck);
			ts += st->tk_delay;

			if (ts * ctx->min_ts_scale < ctx->min_ts_computed * st->timescale) {
				return;
			}
		}
	}

	//check condition
	if (ctx->extract_mode==EXTRACT_SIZE) {
		u32 nb_stop_at_min_ts = 0;
		u64 cumulated_size = 0;
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

				if (ts * ctx->min_ts_scale >= ctx->min_ts_computed * st->timescale) {
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
		//not done yet (estimated size less than target split)
		if (
			(cumulated_size < ctx->split_size)
			&& ctx->min_ts_scale
			//do this only if first time we estimate this chunk size, or if previous estimated min_ts is not the same as current min_ts
			&& (!ctx->prev_min_ts_computed || (ctx->prev_min_ts_computed < ctx->min_ts_computed))
		) {
			if ((nb_stop_at_min_ts + nb_eos) == count) {
				ctx->est_file_size = cumulated_size;
				ctx->prev_min_ts_computed = ctx->min_ts_computed;
				ctx->prev_min_ts_scale = ctx->min_ts_scale;
				ctx->min_ts_computed = 0;
				ctx->min_ts_scale = 0;
				ctx->gop_depth++;
			}
			return;
		}

		//decide which split size we use
		if (ctx->xround==REFRAME_ROUND_BEFORE) {
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
	}

	//good to go
	ctx->in_range = GF_TRUE;
	ctx->gop_depth = 0;
	for (i=0; i<count; i++) {
		u64 ts;
		RTStream *st = gf_list_get(ctx->streams, i);
		GF_FilterPacket *pck = gf_list_get(st->pck_queue, 0);
		st->range_end_reached_ts = (ctx->min_ts_computed * st->timescale);
		if (ctx->min_ts_scale)
			st->range_end_reached_ts /= ctx->min_ts_scale;

		st->range_end_reached_ts += 1;
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

}


GF_Err reframer_process(GF_Filter *filter)
{
	GF_ReframerCtx *ctx = gf_filter_get_udta(filter);
	u32 i, nb_eos, nb_end_of_range, count = gf_filter_get_ipid_count(filter);

	if (ctx->is_eos) {
		return GF_EOS;
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
				- check if we start from this SAP or from previous SAP (possibly before start) according to cround
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
		Bool check_split = GF_FALSE;
		//init timing
		if (!ctx->timing_initialized) {
			if (!reframer_init_timing(filter, ctx)) return GF_OK;
		}

		//fetch input packets
		for (i=0; i<count; i++) {
			u64 ts;
			u32 pck_in_range, dur;
			Bool is_sap;
			Bool drop_input = GF_TRUE;
			GF_FilterPacket *pck;
			GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
			RTStream *st = gf_filter_pid_get_udta(ipid);

			if (!st->is_playing) {
				nb_start_range_reached++;
				continue;
			}

			if (st->range_start_computed && !ctx->wait_video_range_adjust) {
				nb_start_range_reached++;
				continue;
			}
			//if eos is marked we are flushing so don't check range_end
			if (!ctx->has_seen_eos && st->range_end_reached_ts) continue;

			if (st->split_pck) {
				pck = st->split_pck;
				drop_input = GF_FALSE;
			} else {
				pck = gf_filter_pid_get_packet(ipid);
			}
			if (!pck) {
				if (gf_filter_pid_is_eos(ipid)) {
					//special case for PIDs with a single packet, we reinsert them at the begining of each extracted range
					//this allows dealing with BIFS/OD/JPEG/PNG tracks
					if (st->resinsert_single_pck) {
						if (!ctx->in_range && !st->range_start_computed) {
							st->range_start_computed = 3;
							if (!gf_list_count(st->pck_queue)) {
								pck = st->resinsert_single_pck;
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
						}
					}
					//force flush in case of extract dur to avoid creating file with only a few samples of one track only
					if (st->is_playing && (ctx->extract_mode==EXTRACT_DUR)) {
						ctx->has_seen_eos = GF_TRUE;
						ctx->in_range = 1;
					}
				}
				continue;
			}
			st->nb_frames_range++;

			ts = gf_filter_pck_get_dts(pck);
			if (ts==GF_FILTER_NO_TS)
				ts = gf_filter_pck_get_cts(pck);
			ts += st->tk_delay;

			//if nosap is set, consider all packet SAPs
			is_sap = (ctx->nosap || ctx->raw || gf_filter_pck_get_sap(pck)) ? GF_TRUE : GF_FALSE;

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
				//add packet
				gf_filter_pck_ref(&pck);
				gf_filter_pid_drop_packet(st->ipid);
				gf_list_add(st->pck_queue, pck);
				check_split = GF_TRUE;
				//keep ref to first packet until we see a second one
				if (st->nb_frames_range==1) {
					gf_filter_pck_ref(&pck);
					st->resinsert_single_pck = pck;
				} else if (st->resinsert_single_pck) {
					gf_filter_pck_unref(st->resinsert_single_pck);
					st->resinsert_single_pck = NULL;
				}
				continue;
			}
			dur = gf_filter_pck_get_duration(pck);

			//dur split or range extraction but we wait for video end range to be adjusted, don't enqueue packet
			if (ctx->wait_video_range_adjust && !st->needs_adjust)
				continue;

			//check if packet is in our range
			pck_in_range = reframer_check_pck_range(ctx, st, ts, dur, st->nb_frames_range);


			//SAP packet, decide if we cut here or at previous SAP
			if (is_sap) {
				//if streamtype is video or we have only one pid, purge all packets in all streams before this time
				//
				//for more complex cases we keep packets because we don't know if we will need SAP packets before the final
				//decided start range
				if (!pck_in_range && ((count==1) || !st->all_saps) ) {
					reframer_purge_queues(ctx, ts, st->timescale);
				}

				//packet in range and global context not yet in range, mark which SAP will be the begining of our range
				if (!ctx->in_range && (pck_in_range==1)) {
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
							u64 start_range_ts = ctx->cur_start.num;
							start_range_ts *= st->timescale;
							start_range_ts /= ctx->cur_start.den;

							diff_prev = diff_cur = start_range_ts;
							diff_prev -= st->prev_sap_ts;
							diff_cur -= ts;
							if (ABS(diff_cur) < ABS(diff_prev)) cur_closer = GF_TRUE;
						}
						if (cur_closer) {
							st->sap_ts_plus_one = ts+1;
						} else {
							st->sap_ts_plus_one = st->prev_sap_ts + 1;
						}
					} else if (ctx->xround==REFRAME_ROUND_BEFORE) {
						st->sap_ts_plus_one = st->prev_sap_ts+1;
					} else {
						st->sap_ts_plus_one = ts+1;
					}
					st->range_start_computed = 1;
					nb_start_range_reached++;
				}
				//remember prev sap time
				if (pck_in_range!=2) {
					st->prev_sap_ts = ts;
					st->prev_sap_frame_idx = st->nb_frames_range;
				}
				//video stream start and xadjust set, prevent all other streams from being processed until wee determine the end of the video range
				//and re-enable other streams processing
				if (!ctx->wait_video_range_adjust && ctx->xadjust && st->needs_adjust) {
					ctx->wait_video_range_adjust = GF_TRUE;
				}
			}

			if ((ctx->extract_mode==EXTRACT_DUR) && ctx->has_seen_eos && (pck_in_range==2))
				pck_in_range = 1;

			//after range: whether SAP or not, mark end of range reached
			if (pck_in_range==2) {
				if (!ctx->xadjust || is_sap) {
					Bool enqueue = GF_FALSE;
					st->split_end = 0;
					if (!st->range_start_computed) {
						st->sap_ts_plus_one = st->prev_sap_ts + 1;
						st->range_start_computed = 1;
						nb_start_range_reached++;
						if (st->prev_sap_ts == ts)
							enqueue = GF_TRUE;
					}
					//remember the timestamp of first packet after range
					st->range_end_reached_ts = ts + 1;

					//time-based extraction or dur split, try to clone packet
					if (st->can_split && !ctx->start_frame_idx_plus_one) {
						if ((s64) (ts * ctx->cur_end.den) < ctx->cur_end.num * st->timescale) {
							//force enqueing this packet
							enqueue = GF_TRUE;
							st->split_end = (u32) ( (ctx->cur_end.num * st->timescale) / ctx->cur_end.den - ts);
							st->range_end_reached_ts += st->split_end;
							//and remember it for next chunk - note that we dequeue the input to get proper eos notification
							gf_filter_pck_ref(&pck);
							st->split_pck = pck;
						}
					}
					//video stream end detected and xadjust set, adjust cur_end to match the video stream end range
					//and re-enable other streams processing
					if (ctx->wait_video_range_adjust && ctx->xadjust && st->needs_adjust) {
						ctx->cur_end.num = st->range_end_reached_ts-1;
						ctx->cur_end.den = st->timescale;
						ctx->wait_video_range_adjust = GF_FALSE;
					}

					//do NOT enqueue packet
					if (!enqueue)
						break;
				}
			}

			//add packet
			gf_filter_pck_ref(&pck);
			gf_list_add(st->pck_queue, pck);
			if (drop_input) {
				gf_filter_pid_drop_packet(st->ipid);
				//keep ref to first packet until we see a second one
				if (st->nb_frames_range==1) {
					gf_filter_pck_ref(&pck);
					st->resinsert_single_pck = pck;
				} else if (st->resinsert_single_pck) {
					gf_filter_pck_unref(st->resinsert_single_pck);
					st->resinsert_single_pck = NULL;
				}
			} else {
				assert(pck == st->split_pck);
				gf_filter_pck_unref(st->split_pck);
				st->split_pck = NULL;
			}
		}

		if (check_split) {
			check_gop_split(ctx);
		}

		//all streams reached the start range, compute min ts
		if (!ctx->in_range
			&& (nb_start_range_reached==count)
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
					if (!min_ts_split || ((st->sap_ts_plus_one-1) * min_timescale_split < min_ts_split * st->timescale) ) {
						min_ts_split = st->sap_ts_plus_one;
						min_timescale_split = st->timescale;
					}
				}
				else if (st->all_saps) {
					if (!min_ts_a || ((st->sap_ts_plus_one-1) * min_timescale_a < min_ts_a * st->timescale) ) {
						min_ts_a = st->sap_ts_plus_one;
						min_timescale_a = st->timescale;
					}
				} else {
					if (!min_ts || ((st->sap_ts_plus_one-1) * min_timescale < min_ts * st->timescale) ) {
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
				ctx->is_eos = GF_TRUE;
			} else {
				min_ts -= 1;
			}
			//purge everything before min ts
			for (i=0; i<count; i++) {
				GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
				RTStream *st = gf_filter_pid_get_udta(ipid);

				while (gf_list_count(st->pck_queue)) {
					GF_FilterPacket *pck = gf_list_get(st->pck_queue, 0);
					if (!purge_all) {
						u32 is_start = 0;
						u64 ts, ots;
						u64 dur;
						ts = gf_filter_pck_get_dts(pck);
						if (ts==GF_FILTER_NO_TS)
							ts = gf_filter_pck_get_cts(pck);
						ts += st->tk_delay;
						dur = (u64) gf_filter_pck_get_duration(pck);
						if (!dur) dur=1;
						ots = ts;
						if (min_timescale != st->timescale) {
							ts *= min_timescale;
							ts /= st->timescale;
							dur *= min_timescale;
							dur /= st->timescale;
						}

						if (ts >= min_ts) {
							is_start = 1;
						}
						else if (st->can_split && (ts+dur >= min_ts)) {
							is_start = 2;
						}
						else if (st->range_start_computed==3) {
							is_start = 1;
						}

						if (is_start) {
							//remember TS at range start
							s64 orig = min_ts;
							if (st->timescale != min_timescale) {
								orig *= st->timescale;
								orig /= min_timescale;
							}
							st->split_start = 0;
							if (is_start==2) {
								st->split_start = (u32) (min_ts - ts);
								if (min_timescale != st->timescale) {
									st->split_start *= st->timescale;
									st->split_start /= min_timescale;
								}
							}
							st->ts_at_range_start_plus_one = ots + 1;

							if ((st->range_start_computed==1)
								&& (orig < (s64) ots)
								&& ctx->splitrange
								&& (ctx->cur_range_idx>1)
							) {
								s32 delay = (s32) ((s64) ots - (s64) orig);
								gf_filter_pid_set_property(st->opid, GF_PROP_PID_DELAY, &PROP_SINT(delay) );
							}
							break;
						}
					}
					gf_list_rem(st->pck_queue, 0);
					gf_filter_pck_unref(pck);
					st->nb_frames++;
				}
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
			}
			if (purge_all) {
				if (ctx->extract_mode!=EXTRACT_RANGE)
					return GF_EOS;

				goto load_next_range;
			}

			//we are in the range
			ctx->in_range = GF_TRUE;
		}
		if (!ctx->in_range)
			return GF_OK;
	}

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
			if (st->resinsert_single_pck) {
				u64 start = ctx->cur_start.num;
				start *= st->timescale;
				start /= ctx->cur_start.den;
				st->ts_at_range_end = ctx->cur_end.num;
				st->ts_at_range_end *= st->timescale;
				st->ts_at_range_end /= ctx->cur_end.den;
				st->ts_at_range_end -= start;
			} else {
				st->ts_at_range_end = (st->range_end_reached_ts - 1)  - (st->ts_at_range_start_plus_one - 1);
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
		if (ctx->reschedule_in>2000) {
			gf_filter_ask_rt_reschedule(filter, (u32) (ctx->reschedule_in - 2000));
		} else if (ctx->reschedule_in>1000) {
			gf_filter_ask_rt_reschedule(filter, (u32) (ctx->reschedule_in / 2));
		}
	}

	return GF_OK;
}

static const GF_FilterCapability ReframerRAWCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_CODECID, GF_CODECID_RAW)
};

static GF_Err reframer_initialize(GF_Filter *filter)
{
	GF_ReframerCtx *ctx = gf_filter_get_udta(filter);

	ctx->streams = gf_list_new();
	ctx->seekable = GF_TRUE;
	reframer_load_range(ctx);

	if (ctx->raw) {
		gf_filter_override_caps(filter, ReframerRAWCaps, sizeof(ReframerRAWCaps) / sizeof(GF_FilterCapability) );
	}

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		reframer_update_ts_shift(ctx, -1, 1000);
	}
#endif
	return GF_OK;
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
		if (ctx->range_type && !ctx->start_frame_idx_plus_one) {
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
		ctx->is_eos = GF_FALSE;
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
		reframer_reset_stream(ctx, st);
	}
	gf_list_del(ctx->streams);
}

static const GF_FilterCapability ReframerCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	//we do accept everything, including raw streams 
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_UNFRAMED, GF_TRUE),
	//we don't accept files as input so don't output them
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	//we don't produce RAW streams during dynamic chain resolution - this will avoid loading the filter for compositor/other raw access
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	//but we may produce raw streams when filter is explicitly loaded (media exporter)
	CAP_UINT(GF_CAPS_OUTPUT_LOADED_FILTER, GF_PROP_PID_CODECID, GF_CODECID_RAW)
};


#define OFFS(_n)	#_n, offsetof(GF_ReframerCtx, _n)
static const GF_FilterArgs ReframerArgs[] =
{
	{ OFFS(exporter), "compatibility with old exporter, displays export results", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(rt), "real-time regulation mode of input\n"
	"- off: disables real-time regulation\n"
	"- on: enables real-time regulation, one clock per pid\n"
	"- sync: enables real-time regulation one clock for all pids", GF_PROP_UINT, "off", "off|on|sync", GF_FS_ARG_HINT_NORMAL},
	{ OFFS(saps), "drop non-SAP packets, off by default. The list gives the SAP types (0,1,2,3,4) to forward. Note that forwarding only sap 0 will break the decoding", GF_PROP_UINT_LIST, NULL, "0|1|2|3|4", GF_FS_ARG_HINT_NORMAL},
	{ OFFS(refs), "forward only frames used as reference frames, if indicated in the input stream", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_NORMAL},
	{ OFFS(speed), "speed for real-time regulation mode - only positive value", GF_PROP_DOUBLE, "1.0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(raw), "force input streams to be in raw format (i.e. forces decoding of input)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_NORMAL},
	{ OFFS(frames), "drop all except listed frames (first being 1), off by default", GF_PROP_UINT_LIST, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(xs), "extraction start time(s), see filter help", GF_PROP_STRING_LIST, NULL, NULL, GF_FS_ARG_HINT_NORMAL},
	{ OFFS(xe), "extraction end time(s). If less values than start times, the last time interval extracted is an open range", GF_PROP_STRING_LIST, NULL, NULL, GF_FS_ARG_HINT_NORMAL},
	{ OFFS(xround), "adjustment of extraction start range I-frame\n"
	"- before: use first I-frame preceeding or equal to start range\n"
	"- after: use first I-frame (if any) following or equal to start range\n"
	"- closest: use I-frame closest to start range", GF_PROP_UINT, "before", "before|after|closest", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(xadjust), "adjust end time of extraction range to be before next I-frame", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(nosap), "do not cut at SAP when extracting range (may result in broken streams)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(splitrange), "signal file boundary at each extraction first packet for template-base file generation", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(seeksafe), "rewind play requests by given seconds (to make sur I-frame preceeding start is catched)", GF_PROP_DOUBLE, "10.0", NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};

GF_FilterRegister ReframerRegister = {
	.name = "reframer",
	GF_FS_SET_DESCRIPTION("Media Reframer")
	GF_FS_SET_HELP("This filter provides various compressed domain tools on inputs:\n"
		"- ensure reframing\n"
		"- optionally force decoding\n"
		"- real-time regulation\n"
		"- packet filtering based on SAP types or frame numbers\n"
		"- time-range extraction and spliting\n"
		"This filter forces input pids to be properly framed (1 packet = 1 Access Unit).\n"
		"It is typcially needed to force remultiplexing in file to file operations when source and destination files use the same format.\n"
		"  \n"
		"# SAP filtering\n"
		"The filter can remove packets based on their SAP types using [-saps]() option.\n"
		"For example, this can be used to extract only the key frame (SAP 1,2,3) of a video to create a trick mode version.\n"
		"  \n"
		"# Frame filtering\n"
		"This filter can keep only specific Access Units of the source using [-frames]() option.\n"
		"For example, this can be used to extract only specific key frame of a video to create a HEIF collection.\n"
		"  \n"
		"# Frame decoding\n"
		"This filter can force input media streams to be decoded using the [-raw]() option.\n"
		"EX gpac src=m.mp4 reframer:raw @ [dst]\n"
		"# Real-time Regulation\n"
		"The filter can perform real-time regulation of input packets, based on their timescale and timestamps.\n"
		"For example to simulate a live DASH:\n"
		"EX gpac src=m.mp4 reframer:rt=on @ dst=live.mpd:dynamic\n"
		"  \n"
		"# Range extraction\n"
		"The filter can perform time range extraction of the source using [-xs]() and [-xe]() options.\n"
		"The formats allowed for times specifiers are:\n"
		"- 'T'H:M:S: specify time in hours, minutes, seconds\n"
		"- 'T'H:M:S.MS: specify time in hours, minutes, seconds and milliseconds\n"
		"- INT: specify time in millisecond\n"
		"- NUM/DEN: specify time in seconds as fraction\n"
		"- FNUM: specify time as frame number\n"
		"In this mode, the timestamps are rewritten to form a continuous timeline.\n"
		"When multiple ranges are given, the filter will try to seek if supported by source."
		"\n"
		"EX gpac src=m.mp4 reframer:xs=T00:00:10,T00:01:10,T00:02:00:xs=T00:00:20,T00:01:20 [dst]\n"
		"This will extract the time ranges [10s,20s], [1m10s,1m20s] and all media starting from 2m\n"
		"\n"
		"It is possible to signal range boundaries in output packets using [-splitrange]().\n"
		"This will expose on the first packet of each range in each pid the following properties:\n"
		"- FileNumber: starting at 1 for the first range, to be used as replacement for $num$ in templates\n"
		"- FileSuffix: corresponding to `StartRange_EndRange` or `StartRange` for open ranges, to be used as replacement for $FS$ in templates\n"
		"\n"
		"EX gpac src=m.mp4 reframer:xs=T00:00:10,T00:01:10:xe=T00:00:20:splitrange -o dump_$FS$.264\n"
		"This will create two output files dump_T00.00.10_T00.02.00.264 and dump_T00.01.10.264.\n"
		"Note: The `:` and `/` characters are replaced by `.` in `FileSuffix` property.\n"
		"\n"
		"# Other split actions\n"
		"The filter can perform splitting of the source using [-xs]() option.\n"
		"The additional formats allowed for [-xs]() option are:\n"
		"- 'SAP': split source at each SAP/RAP\n"
		"- 'D'VAL: split source by chunks of VAL ms\n"
		"- 'D'NUM/DEN: split source by chunks of NUM/DEN seconds\n"
		"- 'S'VAL: split source by chunks of estimated size VAL bytes, VAL can use property multipliers\n"
		"\n"
		"Note: In these modes, [-splitrange]() and [-xadjust]() are implicitly set.\n"
	)
	.private_size = sizeof(GF_ReframerCtx),
	.max_extra_pids = (u32) -1,
	.args = ReframerArgs,
	//reframer is explicit only, so we don't load the reframer during resolution process
	.flags = GF_FS_REG_EXPLICIT_ONLY,
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
