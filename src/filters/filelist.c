/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2021
 *					All rights reserved
 *
 *  This file is part of GPAC / file concatenator filter
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
#include <gpac/thread.h>
#include <gpac/list.h>
#include <gpac/bitstream.h>
#include <gpac/isomedia.h>
#include <gpac/network.h>

#ifndef GPAC_DISABLE_AV_PARSERS
#include <gpac/avparse.h>
#endif

enum
{
	FLIST_STATE_STOP=0,
	FLIST_STATE_PLAY,
	FLIST_STATE_WAIT_PLAY,
};

typedef struct
{
	Bool is_raw, planar;
	u32 nb_ch, abps, sample_rate;
} RawAudioInfo;


typedef struct
{
	GF_FilterPid *ipid;
	GF_FilterPid *opid;
	GF_FilterPid *opid_aux;
	u32 stream_type;
	//in current timescale
	u64 max_cts, max_dts;
	u32 o_timescale, timescale;
	//in output timescale
	u64 cts_o, dts_o;
	Bool single_frame;
	Bool is_eos;
	u64 dts_sub;
	u64 first_dts_plus_one;

	Bool skip_dts_init;
	u32 play_state;

	Bool send_cue;
	s32 delay, initial_delay;

	RawAudioInfo ra_info, splice_ra_info;

	s32 audio_samples_to_keep;

	GF_FilterPid *splice_ipid;
	Bool splice_ready;

	u64 cts_o_splice, dts_o_splice;
	u64 dts_sub_splice;
	u32 timescale_splice;
	s32 splice_delay;
	Bool wait_rap;
} FileListPid;

typedef struct
{
	char *file_name;
	u64 last_mod_time;
	u64 file_size;
} FileListEntry;

enum
{
	FL_SORT_NONE=0,
	FL_SORT_NAME,
	FL_SORT_SIZE,
	FL_SORT_DATE,
	FL_SORT_DATEX,
};

enum
{
	//no splicing
	FL_SPLICE_NONE=0,
	//waiting for splice to be active, content is main content
	FL_SPLICE_BEFORE,
	//splice is active, content is spliced content
	FL_SPLICE_ACTIVE,
	//splice no longer active, content is main content
	FL_SPLICE_AFTER,
};

enum
{
	FL_RAW_AV=0,
	FL_RAW_AUDIO,
	FL_RAW_VIDEO,
	FL_RAW_NO
};


enum
{
	FL_SPLICE_DELTA = 1,
	FL_SPLICE_FRAME = 1<<1,
};

typedef struct
{
	//opts
	Bool revert, sigcues, fdel;
	u32 raw;
	s32 floop;
	u32 fsort;
	u32 ka;
	u64 timeout;
	GF_PropStringList srcs;
	GF_Fraction fdur;
	u32 timescale;

	GF_FilterPid *file_pid;
	char *file_path;
	u32 last_url_crc;
	u32 last_url_lineno;
	u32 last_splice_crc;
	u32 last_splice_lineno;
	Bool load_next;

	GF_List *filter_srcs;
	GF_List *io_pids;
	Bool is_eos;

	GF_Fraction64 cts_offset, dts_offset, wait_dts_plus_one, dts_sub_plus_one;

	u32 nb_repeat;
	Double start, stop;
	Bool do_cat, do_del, src_error;
	u64 start_range, end_range;
	GF_List *file_list;
	s32 file_list_idx;

	u64 current_file_dur;
	Bool last_is_isom;

	u32 wait_update_start;
	u64 last_file_modif_time;

	Bool skip_sync;
	Bool first_loaded;
	char *frag_url;

	char *unknown_params;
	char *pid_props;

	GF_Fraction64 splice_start, splice_end;
	u32 flags_splice_start, flags_splice_end;
	u32 splice_state;
	FileListPid *splice_ctrl;

	//splice out, in and current time in **output** timeline
	u64 splice_end_cts, splice_start_cts, spliced_current_cts;
	Bool wait_splice_start;
	Bool wait_splice_end;
	Bool wait_source;
	GF_Fraction64 cts_offset_at_splice, dts_offset_at_splice, dts_sub_plus_one_at_splice;

	GF_List *splice_srcs;
	char *dyn_period_id;
	u32 cur_splice_index;
	u32 splice_nb_repeat;
	Bool keep_splice, mark_only, was_kept;
	char *splice_props;
	char *splice_pid_props;

	GF_Fraction64 init_splice_start, init_splice_end;
	u32 init_flags_splice_start, init_flags_splice_end;
	Double init_start, init_stop;
	Bool force_splice_resume;
} GF_FileListCtx;

static const GF_FilterCapability FileListCapsSrc[] =
{
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
};

static const GF_FilterCapability FileListCapsSrc_RAW_AV[] =
{
	//raw audio and video only
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	//no restriction for media other than audio and video - cf regular caps for comments
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_IN_OUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_UNFRAMED, GF_TRUE),
};

static const GF_FilterCapability FileListCapsSrc_RAW_A[] =
{
	//raw audio only
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	//no restriction for media other than audio - cf regular caps for comments
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_IN_OUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_UNFRAMED, GF_TRUE),
};

static const GF_FilterCapability FileListCapsSrc_RAW_V[] =
{
	//raw video only
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,  GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	//no restriction for media other than video - cf regular caps for comments
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_IN_OUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_UNFRAMED, GF_TRUE),
};

static void filelist_start_ipid(GF_FileListCtx *ctx, FileListPid *iopid, u32 prev_timescale, Bool is_reassign)
{
	iopid->is_eos = GF_FALSE;

	if (is_reassign && !ctx->do_cat) {
		GF_FilterEvent evt;
		//if we reattached the input, we must send a play request
		gf_filter_pid_init_play_event(iopid->ipid, &evt, ctx->start, 1.0, "FileList");
		gf_filter_pid_send_event(iopid->ipid, &evt);
        iopid->skip_dts_init = GF_FALSE;
        if (iopid->play_state==FLIST_STATE_STOP) {
            GF_FEVT_INIT(evt, GF_FEVT_STOP, iopid->ipid)
            gf_filter_pid_send_event(iopid->ipid, &evt);
            iopid->skip_dts_init = GF_TRUE;
        }
	}

	//and convert back cts/dts offsets to output timescale
	if (ctx->dts_offset.num && ctx->dts_offset.den) {
		iopid->dts_o = gf_timestamp_rescale(ctx->dts_offset.num, ctx->dts_offset.den, iopid->o_timescale);
	} else {
		iopid->dts_o = 0;
	}
	if (ctx->cts_offset.num && ctx->cts_offset.den) {
		iopid->cts_o = gf_timestamp_rescale(ctx->cts_offset.num, ctx->cts_offset.den, iopid->o_timescale);
	} else {
		iopid->cts_o = 0;
	}
	
	if (is_reassign && prev_timescale) {
		u64 dts, cts;

		//in input timescale
		dts = iopid->max_dts - iopid->dts_sub;
		cts = iopid->max_cts - iopid->dts_sub;
		//convert to output timescale
		if (prev_timescale != iopid->o_timescale) {
			dts = gf_timestamp_rescale(dts, prev_timescale, iopid->o_timescale);
			cts = gf_timestamp_rescale(cts, prev_timescale, iopid->o_timescale);
		}
		if (
			//skip sync mode, do not adjust timestamps
			ctx->skip_sync
			//in case of rounding
			|| ((dts > iopid->dts_o) && (cts > iopid->cts_o))
		) {
			iopid->dts_o = dts;
			iopid->cts_o = cts;
		}
	}
	iopid->max_cts = iopid->max_dts = 0;

	iopid->first_dts_plus_one = 0;
}

static Bool filelist_merge_prop(void *cbk, u32 prop_4cc, const char *prop_name, const GF_PropertyValue *src_prop)
{
	const GF_PropertyValue *p;
	GF_FilterPid *pid = (GF_FilterPid *) cbk;

	if (prop_4cc) p = gf_filter_pid_get_property(pid, prop_4cc);
	else p = gf_filter_pid_get_property_str(pid, prop_name);
	if (p) return GF_FALSE;
	return GF_TRUE;
}

static void filelist_push_period_id(GF_FileListCtx *ctx, GF_FilterPid *opid)
{
	char szID[200];
	char *dyn_period = NULL;

	gf_dynstrcat(&dyn_period, ctx->dyn_period_id, NULL);
	sprintf(szID, "_%d", ctx->cur_splice_index+1);
	gf_dynstrcat(&dyn_period, szID, NULL);
	gf_filter_pid_set_property(opid, GF_PROP_PID_PERIOD_ID, &PROP_STRING_NO_COPY(dyn_period));
}

static void filelist_override_caps(GF_Filter *filter, GF_FileListCtx *ctx)
{
	switch (ctx->raw) {
	case FL_RAW_AV:
		gf_filter_override_caps(filter, FileListCapsSrc_RAW_AV, GF_ARRAY_LENGTH(FileListCapsSrc_RAW_AV) );
		break;
	case FL_RAW_AUDIO:
		gf_filter_override_caps(filter, FileListCapsSrc_RAW_A, GF_ARRAY_LENGTH(FileListCapsSrc_RAW_A) );
		break;
	case FL_RAW_VIDEO:
		gf_filter_override_caps(filter, FileListCapsSrc_RAW_V, GF_ARRAY_LENGTH(FileListCapsSrc_RAW_V) );
		break;
	default:
		gf_filter_override_caps(filter, FileListCapsSrc, 2);
	}
}

static GF_Err filelist_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	FileListPid *iopid;
	GF_FilterPid *opid = NULL;
	const GF_PropertyValue *p;
	u32 i, count;
	Bool reassign = GF_FALSE;
	Bool first_config = GF_FALSE;
	u32 force_bitrate = 0;
	u32 prev_timescale = 0;
	char *src_url = NULL;
	GF_FileListCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		if (pid==ctx->file_pid)
			ctx->file_pid = NULL;
		else {
			iopid = gf_filter_pid_get_udta(pid);
			if (iopid)
				iopid->ipid = NULL;
		}
		return GF_OK;
	}

	if (!ctx->file_pid && !ctx->file_list) {
		if (! gf_filter_pid_check_caps(pid))
			return GF_NOT_SUPPORTED;
		ctx->file_pid = pid;

		//we will declare pids later

		//from now on we only accept the above caps
		filelist_override_caps(filter, ctx);
	}
	if (ctx->file_pid == pid) return GF_OK;

	iopid = NULL;
	count = gf_list_count(ctx->io_pids);
	for (i=0; i<count; i++) {
		iopid = gf_list_get(ctx->io_pids, i);
		if (iopid->ipid==pid) break;
		//check matching stream types if out pit not connected, and reuse if matching
		if (!iopid->ipid) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
			assert(p);
			if (p->value.uint == iopid->stream_type) {
				iopid->ipid = pid;
				prev_timescale = iopid->timescale;
				iopid->timescale = gf_filter_pid_get_timescale(pid);
				reassign = GF_TRUE;
				break;
			}
		}
		iopid = NULL;
	}

	if (!iopid) {
		GF_SAFEALLOC(iopid, FileListPid);
		if (!iopid) return GF_OUT_OF_MEM;
		iopid->ipid = pid;
		if (ctx->timescale) iopid->o_timescale = ctx->timescale;
		else {
			iopid->o_timescale = gf_filter_pid_get_timescale(pid);
			if (!iopid->o_timescale) iopid->o_timescale = 1000;
		}
		gf_list_add(ctx->io_pids, iopid);
		iopid->send_cue = ctx->sigcues;
		iopid->play_state = FLIST_STATE_WAIT_PLAY;
		first_config = GF_TRUE;
	}
	gf_filter_pid_set_udta(pid, iopid);

	if (!iopid->opid) {
		iopid->opid = gf_filter_pid_new(filter);
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
		assert(p);
		iopid->stream_type = p->value.uint;
	}
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	if (ctx->sigcues) {
		p = gf_filter_pid_get_property(iopid->opid, GF_PROP_PID_BITRATE);
		if (!p) p = gf_filter_pid_get_property(iopid->ipid, GF_PROP_PID_BITRATE);
		if (p) force_bitrate = p->value.uint;

		p = gf_filter_pid_get_property(iopid->ipid, GF_PROP_PID_URL);
		if (p && p->value.string) src_url = gf_strdup(p->value.string);

	}
	opid = iopid->opid;

	if (ctx->keep_splice && (ctx->splice_state==FL_SPLICE_ACTIVE) && iopid->splice_ipid) {
		assert(!iopid->opid_aux);
		iopid->opid_aux = gf_filter_pid_new(filter);
		opid = iopid->opid_aux;

		gf_filter_pid_set_property_str(iopid->opid, "period_switch", &PROP_BOOL(GF_TRUE));
		gf_filter_pid_set_property_str(iopid->opid, "period_resume", &PROP_STRING(ctx->dyn_period_id ? ctx->dyn_period_id : "") );

		if (ctx->splice_props)
			gf_filter_pid_push_properties(iopid->opid, ctx->splice_props, GF_TRUE, GF_TRUE);

	} else {
		gf_filter_pid_set_property_str(iopid->opid, "period_switch", NULL);
	}

	//copy properties at init or reconfig:
	//reset (no more props)
	gf_filter_pid_reset_properties(opid);

	gf_filter_pid_copy_properties(opid, iopid->ipid);

	//if file pid is defined, merge its properties. This will allow forwarding user-defined properties,
	// eg -i list.txt:#MyProp=toto to all PIDs coming from the sources
	if (ctx->file_pid) {
		gf_filter_pid_merge_properties(opid, ctx->file_pid, filelist_merge_prop, iopid->ipid);

		p = gf_filter_pid_get_property(opid, GF_PROP_PID_MIME);
		if (p && p->value.string && !strcmp(p->value.string, "application/x-gpac-playlist"))
			gf_filter_pid_set_property(opid, GF_PROP_PID_MIME, NULL);
	}

	//we could further optimize by querying the duration of all sources in the list
	gf_filter_pid_set_property(opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_NONE) );
	gf_filter_pid_set_property(opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(iopid->o_timescale) );
	gf_filter_pid_set_property(opid, GF_PROP_PID_NB_FRAMES, NULL );

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NB_FRAMES);
	iopid->single_frame = (p && (p->value.uint==1)) ? GF_TRUE : GF_FALSE ;
	iopid->timescale = gf_filter_pid_get_timescale(pid);
	if (!iopid->timescale) iopid->timescale = 1000;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (p && (p->value.uint==GF_CODECID_RAW) && (iopid->stream_type==GF_STREAM_AUDIO)) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
		iopid->ra_info.nb_ch = p ? p->value.uint : 1;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
		iopid->ra_info.sample_rate = p ? p->value.uint : 0;
		iopid->ra_info.abps = 0;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
		if (p) {
			iopid->ra_info.abps = gf_audio_fmt_bit_depth(p->value.uint) / 8;
			iopid->ra_info.planar = (p->value.uint > GF_AUDIO_FMT_LAST_PACKED) ? 1 : 0;
		}
		iopid->ra_info.abps *= iopid->ra_info.nb_ch;
		iopid->ra_info.is_raw = GF_TRUE;
	} else {
		iopid->ra_info.abps = iopid->ra_info.nb_ch = iopid->ra_info.sample_rate = 0;
		iopid->ra_info.is_raw = GF_FALSE;
	}

	if (ctx->frag_url)
		gf_filter_pid_set_property(opid, GF_PROP_PID_ORIG_FRAG_URL, &PROP_NAME(ctx->frag_url) );

	if (ctx->sigcues) {
		if (!src_url) {
			gf_filter_pid_set_property(opid, GF_PROP_PID_URL, &PROP_STRING("mysource") );
			gf_filter_pid_set_property(opid, GF_PROP_PID_FILEPATH, &PROP_STRING("mysource") );
		} else {
			gf_filter_pid_set_property(opid, GF_PROP_PID_URL, &PROP_STRING(src_url) );
			gf_filter_pid_set_property(opid, GF_PROP_PID_FILEPATH, &PROP_STRING_NO_COPY(src_url));
		}
		if (force_bitrate)
			gf_filter_pid_set_property(opid, GF_PROP_PID_BITRATE, &PROP_UINT(force_bitrate) );
		else
			gf_filter_pid_set_property(opid, GF_PROP_PID_BITRATE, NULL );

		gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_CUE, &PROP_STRING("inband") );
	} else {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_URL);
		if (p)
			gf_filter_pid_set_property(opid, GF_PROP_PID_URL, p);
	}

	if (ctx->pid_props) {
		gf_filter_pid_push_properties(opid, ctx->pid_props, GF_TRUE, GF_TRUE);
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	iopid->delay = p ? (s32) p->value.longsint : 0;
	if (first_config) {
		iopid->initial_delay = iopid->delay;
	} else {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
		if (p && (p->value.uint==GF_CODECID_RAW) && (iopid->delay<0)) {
			gf_filter_pid_set_property(opid, GF_PROP_PID_DELAY, NULL);
		} else {
			gf_filter_pid_set_property(opid, GF_PROP_PID_DELAY, iopid->initial_delay ? &PROP_LONGSINT(iopid->initial_delay) : NULL);
		}
	}

	if (ctx->splice_state==FL_SPLICE_BEFORE) {
		if (!ctx->splice_ctrl) ctx->splice_ctrl = iopid;
		else if (iopid->stream_type==GF_STREAM_VISUAL) ctx->splice_ctrl = iopid;
		iopid->timescale_splice = iopid->timescale;
		iopid->splice_delay = iopid->delay;

		if (ctx->dyn_period_id) gf_free(ctx->dyn_period_id);
		p = gf_filter_pid_get_property(opid, GF_PROP_PID_PERIOD_ID);
		if (p && p->value.string) {
			ctx->dyn_period_id = gf_strdup(p->value.string);
		} else {
			ctx->dyn_period_id = NULL;
		}
	}
	if (ctx->dyn_period_id) {
		if ((ctx->splice_state==FL_SPLICE_BEFORE) || (ctx->splice_state==FL_SPLICE_AFTER)) {
			filelist_push_period_id(ctx, opid);
		}
		//reset period ID if we keep the splice, and copy it from spliced content if any
		else if ((ctx->splice_state==FL_SPLICE_ACTIVE) && ctx->keep_splice) {
			gf_filter_pid_set_property(iopid->opid, GF_PROP_PID_PERIOD_ID, NULL);
			p = gf_filter_pid_get_property(opid, GF_PROP_PID_PERIOD_ID);
			if (p && p->value.string)
				gf_filter_pid_set_property(iopid->opid, GF_PROP_PID_PERIOD_ID, p);
		}
	}
	gf_filter_pid_set_property_str(opid, "period_resume", NULL);

	//if we reattached the input, we must send a play request
	if (reassign) {
		filelist_start_ipid(ctx, iopid, prev_timescale, GF_TRUE);
	}
	//new pid and no a splicing operation, we must adjust dts and cts offset
	else if (ctx->splice_state == FL_SPLICE_NONE) {
		filelist_start_ipid(ctx, iopid, prev_timescale, GF_FALSE);
	}
	return GF_OK;
}

static Bool filelist_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i, count;
	FileListPid *iopid;
	GF_FilterEvent fevt;
	GF_FileListCtx *ctx = gf_filter_get_udta(filter);

	//manually forward event to all input, except our file in pid
	memcpy(&fevt, evt, sizeof(GF_FilterEvent));
	count = gf_list_count(ctx->io_pids);
	for (i=0; i<count; i++) {
		iopid = gf_list_get(ctx->io_pids, i);
		if (!iopid->ipid) continue;

        //only send on non connected inputs or on the one matching the pid event
        if (iopid->opid && (iopid->opid != evt->base.on_pid))
            continue;

		fevt.base.on_pid = iopid->ipid;
		if (evt->base.type==GF_FEVT_PLAY) {
			gf_filter_pid_init_play_event(iopid->ipid, &fevt, ctx->start, 1.0, "FileList");
			iopid->play_state = FLIST_STATE_PLAY;
			iopid->is_eos = GF_FALSE;
		} else if (evt->base.type==GF_FEVT_STOP) {
			iopid->play_state = FLIST_STATE_STOP;
			iopid->is_eos = GF_TRUE;
		}
		gf_filter_pid_send_event(iopid->ipid, &fevt);
	}
	//and cancel
	return GF_TRUE;
}


static void filelist_check_implicit_cat(GF_FileListCtx *ctx, char *szURL)
{
	char *res_url = NULL;
	char *sep;
	if (ctx->file_path) {
		res_url = gf_url_concatenate(ctx->file_path, szURL);
		szURL = res_url;
	}
	//we use default session separator set in filelist
	sep = gf_url_colon_suffix(szURL, '=');
	if (sep) sep[0] = 0;

	switch (gf_isom_probe_file(szURL)) {
	//this is a fragment
	case 3:
		if (ctx->last_is_isom) {
			ctx->do_cat = GF_TRUE;
		}
		break;
	//this is a movie, reload
	case 2:
	case 1:
		ctx->do_cat = GF_FALSE;
		ctx->last_is_isom = GF_TRUE;
		break;
	default:
		ctx->do_cat = GF_FALSE;
		ctx->last_is_isom = GF_FALSE;
	}
	if (sep) sep[0] = ':';
	if (res_url)
		gf_free(res_url);
}

static void filelist_parse_splice_time(char *aval, GF_Fraction64 *frac, u32 *flags)
{
	*flags = 0;
	if (!aval || !strlen(aval)) {
		frac->num = -1;
		frac->den = 1;
		return;
	}
	frac->num = 0;
	frac->den = 0;

	if (!stricmp(aval, "now")) {
		frac->num = -2;
		frac->den = 1;
		return;
	}
	if (strchr(aval, ':')) {
		frac->den = gf_net_parse_date(aval);
		frac->num = -3;
	}

	if (aval[0]=='+') {
		aval ++;
		*flags |= FL_SPLICE_DELTA;
	}
	if (aval[0]=='F') {
		aval ++;
		*flags |= FL_SPLICE_FRAME;
	}

	gf_parse_lfrac(aval, frac);
}

static Bool filelist_next_url(GF_Filter *filter, GF_FileListCtx *ctx, char szURL[GF_MAX_PATH], Bool is_splice_update)
{
	u32 len;
	u32 url_crc = 0;
	Bool last_found = GF_FALSE;
	FILE *f;
	u32 nb_repeat=0, lineno=0;
	u64 start_range=0, end_range=0;
	Double start=0, stop=0;
	GF_Fraction64 splice_start, splice_end;
	Bool do_cat=0;
	Bool do_del=0;
	Bool is_end=0;
	Bool keep_splice=0;
	Bool mark_only=0;
	Bool no_sync=0;
	u32 start_flags=0;
	u32 end_flags=0;

	if (ctx->file_list) {
		FileListEntry *fentry, *next;

		if (ctx->revert) {
			if (!ctx->file_list_idx) {
				if (!ctx->floop) return GF_FALSE;
				if (ctx->floop>0) ctx->floop--;
				ctx->file_list_idx = gf_list_count(ctx->file_list);
			}
			ctx->file_list_idx --;
		} else {
			ctx->file_list_idx ++;
			if (ctx->file_list_idx >= (s32) gf_list_count(ctx->file_list)) {
				if (!ctx->floop) return GF_FALSE;
				if (ctx->floop>0) ctx->floop--;
				ctx->file_list_idx = 0;
			}
		}
		fentry = gf_list_get(ctx->file_list, ctx->file_list_idx);
		strncpy(szURL, fentry->file_name, sizeof(char)*(GF_MAX_PATH-1) );
		szURL[GF_MAX_PATH-1] = 0;
		filelist_check_implicit_cat(ctx, szURL);
		next = gf_list_get(ctx->file_list, ctx->file_list_idx + 1);
		if (next)
			ctx->current_file_dur = next->last_mod_time - fentry->last_mod_time;
		return GF_TRUE;
	}

	if (ctx->wait_update_start || is_splice_update) {
		u64 last_modif_time = gf_file_modification_time(ctx->file_path);
		if (ctx->last_file_modif_time >= last_modif_time) {
			if (!is_splice_update) {
				u64 diff = gf_sys_clock() - ctx->wait_update_start;
				if (diff > ctx->timeout) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[FileList] Timeout refreshing playlist after %d ms, triggering eos\n", diff));
					ctx->ka = 0;
				}
			}
			return GF_FALSE;
		}
		ctx->wait_update_start = 0;
		ctx->last_file_modif_time = last_modif_time;
	}

	splice_start.num = splice_end.num = 0;
	splice_start.den = splice_end.den = 0;

	f = gf_fopen(ctx->file_path, "rt");
	while (f) {
		char *l = gf_fgets(szURL, GF_MAX_PATH, f);
		url_crc = 0;
		if (!l || (gf_feof(f) && !szURL[0]) ) {
			if (ctx->floop != 0) {
				gf_fseek(f, 0, SEEK_SET);
				//load first line
				last_found = GF_TRUE;
				continue;
			}
			gf_fclose(f);
			if (is_end) {
				ctx->ka = 0;
			} else if (ctx->ka) {
				ctx->wait_update_start = gf_sys_clock();
				ctx->last_file_modif_time = gf_file_modification_time(ctx->file_path);
			}
			return GF_FALSE;
		}

		len = (u32) strlen(szURL);
		//in keep-alive mode, each line shall end with \n, of not consider the file is not yet ready
		if (ctx->ka && !is_end && len && (szURL[len-1]!='\n')) {
			gf_fclose(f);
			ctx->wait_update_start = gf_sys_clock();
			ctx->last_file_modif_time = gf_file_modification_time(ctx->file_path);
			return GF_FALSE;
		}
		//strip all \r, \n, \t at the end
		while (len && strchr("\n\r\t ", szURL[len-1])) {
			szURL[len-1] = 0;
			len--;
		}
		if (!len) continue;

		//comment
		if (szURL[0] == '#') {
			//ignored line
			if (szURL[1] == '#')
				continue;

			char *args = szURL+1;
			nb_repeat=0;
			start=stop=0;
			do_cat = 0;
			start_range=end_range=0;
			if (ctx->pid_props) {
				gf_free(ctx->pid_props);
				ctx->pid_props = NULL;
			}

			while (args) {
				char c;
				char *sep, *aval = NULL;
				while (args[0]==' ') args++;

				sep = strchr(args, ' ');
				if (strncmp(args, "props", 5))
					aval = strchr(args, ',');

				if (sep && aval && (aval < sep))
					sep = aval;
				else if (aval)
					sep = aval;

				if (sep) {
					c = sep[0];
					sep[0] = 0;
				}
				aval = strchr(args, '=');
				if (aval) {
					aval[0] = 0;
					aval++;
				}

				if (!strcmp(args, "repeat")) {
					if (aval)
						nb_repeat = atoi(aval);
				} else if (!strcmp(args, "start")) {
					if (aval)
						start = atof(aval);
				} else if (!strcmp(args, "stop")) {
					if (aval)
						stop = atof(aval);
				} else if (!strcmp(args, "cat")) {
					do_cat = GF_TRUE;
				} else if (!strcmp(args, "del")) {
					do_del = GF_TRUE;
				} else if (do_cat && !strcmp(args, "srange")) {
					if (aval)
						sscanf(aval, LLU, &start_range);
				} else if (do_cat && !strcmp(args, "send")) {
					if (aval)
						sscanf(aval, LLU, &end_range);
				} else if (!strcmp(args, "end")) {
					if (ctx->ka)
						is_end = GF_TRUE;
				} else if (!strcmp(args, "ka")) {
					sscanf(aval, "%u", &ctx->ka);
				} else if (!strcmp(args, "raw")) {
					u32 raw_type = 0;
					if (aval) {
						if (!stricmp(aval, "av")) raw_type = FL_RAW_AV;
						else if (!stricmp(aval, "a")) raw_type = FL_RAW_AUDIO;
						else if (!stricmp(aval, "v")) raw_type = FL_RAW_VIDEO;
					}
					if (filter && (ctx->raw != raw_type) ) {
						ctx->raw = raw_type;
						filelist_override_caps(filter, ctx);
					}
				} else if (!strcmp(args, "floop")) {
					ctx->floop = aval ? atoi(aval) : 0;
				} else if (!strcmp(args, "props")) {
					if (ctx->pid_props) gf_free(ctx->pid_props);
					ctx->pid_props = aval ? gf_strdup(aval) : NULL;
				} else if (!strcmp(args, "out")) {
					filelist_parse_splice_time(aval, &splice_start, &start_flags);
				} else if (!strcmp(args, "in")) {
					filelist_parse_splice_time(aval, &splice_end, &end_flags);
				} else if (!strcmp(args, "keep")) {
					keep_splice = GF_TRUE;
				} else if (!strcmp(args, "mark")) {
					mark_only = GF_TRUE;
				} else if (!strcmp(args, "nosync")) {
					no_sync = GF_TRUE;
				} else if (!strcmp(args, "sprops")) {
					if (ctx->splice_props) gf_free(ctx->splice_props);
					ctx->splice_props = aval ? gf_strdup(aval) : NULL;
				} else {
					if (!ctx->unknown_params || !strstr(ctx->unknown_params, args)) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[FileList] Unrecognized directive %s, ignoring\n", args));
						gf_dynstrcat(&ctx->unknown_params, args, ",");
					}
				}

				if (aval) aval[0] = '=';

				if (!sep) break;
				sep[0] = c;
				args = sep+1;
			}
			continue;
		}
		//increase line num only for non-comment lines
		lineno++;

		url_crc = gf_crc_32(szURL, (u32) strlen(szURL) );
		if (!ctx->last_url_crc) {
			ctx->last_url_crc = url_crc;
			ctx->last_url_lineno = lineno;
			break;
		}
		if ( ((ctx->last_url_crc == url_crc) && (ctx->last_url_lineno == lineno))
			|| (is_splice_update && (ctx->last_splice_crc==url_crc))
		) {
			last_found = GF_TRUE;
			nb_repeat=0;
			start=stop=0;
			do_cat=0;
			start_range=end_range=0;
			if (is_splice_update)
				break;
			splice_start.den = splice_end.den = 0;
			keep_splice = 0;
			mark_only = 0;
			start_flags=0;
			end_flags=0;
			no_sync=0;
			continue;
		}
		if (last_found) {
			ctx->last_url_crc = url_crc;
			ctx->last_url_lineno = lineno;
			break;
		}
		nb_repeat=0;
		start=stop=0;
		do_cat=0;
		do_del=0;
		start_range=end_range=0;
		splice_start.den = splice_end.den = 0;
		keep_splice = 0;
		mark_only = 0;
		start_flags=0;
		end_flags=0;
		no_sync=0;
	}
	gf_fclose(f);

	if (is_splice_update && !last_found)
		return GF_FALSE;

	//not in active splicing and we had a splice start set, trigger a new splice
	if ((ctx->splice_state != FL_SPLICE_ACTIVE) && splice_start.den) {
		//activate if:
		if (
			//new new splice start is undefined (prepare for new splice)
			(splice_start.num<0)
			// or new splice start is defined and prev splice start was undefined
			|| ((splice_start.num>=0) && ((ctx->splice_start.num < 0) || !ctx->splice_start.den) )
			// or new splice time is greater than previous one
			|| (ctx->splice_start.num * splice_start.den < splice_start.num * ctx->splice_start.den)
		) {

			ctx->init_splice_start = ctx->splice_start = splice_start;
			if (!splice_end.den) {
				ctx->splice_end.num = -1;
				ctx->splice_end.den = 1;
			} else {
				ctx->splice_end = splice_end;
			}
			ctx->init_splice_end = ctx->splice_end;

			ctx->splice_state = FL_SPLICE_BEFORE;
			//items to be spliced are the ones after the spliced content
			ctx->last_splice_crc = ctx->last_url_crc = url_crc;
			ctx->last_url_lineno = lineno;
			ctx->last_splice_lineno = lineno;
			ctx->last_file_modif_time = gf_file_modification_time(ctx->file_path);
			ctx->keep_splice = keep_splice;
			ctx->mark_only = mark_only;
			ctx->init_flags_splice_start = ctx->flags_splice_start = start_flags;
			ctx->init_flags_splice_end = ctx->flags_splice_end = end_flags;

			if (!ctx->first_loaded) {
				if (ctx->splice_start.num == -2)
					ctx->splice_start.num = -1;
				if (ctx->splice_end.num == -2)
					ctx->splice_end.num = -1;
			}
		}
	}
	//in active splicing and we hava a splice end set, update splice value if previous one was undefined or not set
	else if ((ctx->splice_state == FL_SPLICE_ACTIVE)  && splice_end.den && (!ctx->splice_end.den || (ctx->splice_end.num<0))) {
		ctx->init_splice_end = ctx->splice_end = splice_end;
		ctx->init_flags_splice_end = ctx->flags_splice_end = end_flags;
	}

	if (is_splice_update)
		return GF_FALSE;

	if ((ctx->splice_state == FL_SPLICE_ACTIVE) && (splice_start.den || splice_end.den)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] URL %s is a main splice content but expecting a regular content in active splice\n", szURL));
		return GF_FALSE;
	}


	len = (u32) strlen(szURL);
	while (len && strchr("\r\n", szURL[len-1])) {
		szURL[len-1] = 0;
		len--;
	}
	ctx->nb_repeat = nb_repeat;
	ctx->start = start;
	ctx->stop = stop;
	ctx->do_cat = do_cat;
	ctx->skip_sync = no_sync;

	if (!ctx->floop)
		ctx->do_del = (do_del || ctx->fdel) ? GF_TRUE : GF_FALSE;
	ctx->start_range = start_range;
	ctx->end_range = end_range;
	filelist_check_implicit_cat(ctx, szURL);
	return GF_TRUE;
}

static Bool filelist_on_filter_setup_error(GF_Filter *failed_filter, void *udta, GF_Err err)
{
	u32 i, count;
	GF_Filter *filter = (GF_Filter *)udta;
	GF_FileListCtx *ctx = gf_filter_get_udta(filter);

	GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] Failed to load URL %s: %s\n", gf_filter_get_src_args(failed_filter), gf_error_to_string(err) ));

	count = gf_list_count(ctx->io_pids);
	for (i=0; i<count; i++) {
		FileListPid *iopid = gf_list_get(ctx->io_pids, i);
		if (!iopid->ipid) continue;

		if (gf_filter_pid_is_filter_in_parents(iopid->ipid, failed_filter))
			iopid->ipid = NULL;
	}
	ctx->src_error = GF_TRUE;
	gf_list_del_item(ctx->filter_srcs, failed_filter);
	gf_filter_post_process_task(filter);
	return GF_FALSE;
}


static GF_Err filelist_load_next(GF_Filter *filter, GF_FileListCtx *ctx)
{
	GF_Filter *fsrc;
	FileListPid *iopid;
	u32 i, count;
	GF_Filter *prev_filter;
	GF_List *filters = NULL;
	char *link_args = NULL;
	s32 link_idx = -1;
	Bool is_filter_chain = GF_FALSE;
	Bool do_del = ctx->do_del;
	GF_Err e;
	u32 s_idx;
	char *url;
	char szURL[GF_MAX_PATH];
	Bool next_url_ok;

	next_url_ok = filelist_next_url(filter, ctx, szURL, GF_FALSE);

	if (!next_url_ok && ctx->ka) {
		gf_filter_ask_rt_reschedule(filter, ctx->ka*1000);
		return GF_OK;
	}
	count = gf_list_count(ctx->io_pids);

	if (ctx->wait_splice_start) {
		if (!next_url_ok) {
			ctx->wait_splice_start = GF_FALSE;
			ctx->wait_source = GF_FALSE;
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] No URL for splice, ignoring splice\n"));
			ctx->splice_state = FL_SPLICE_AFTER;
			ctx->splice_end_cts = ctx->splice_start_cts;
			ctx->cts_offset = ctx->cts_offset_at_splice;
			ctx->dts_offset = ctx->dts_offset_at_splice;
			ctx->dts_sub_plus_one = ctx->dts_sub_plus_one_at_splice;
			return GF_OK;
		}
		do_del = GF_FALSE;
		assert (!ctx->splice_srcs);
		ctx->splice_srcs = ctx->filter_srcs;
		ctx->filter_srcs = gf_list_new();
	}

	if (do_del) {
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_FILE_DELETE, NULL);
		evt.file_del.url = "__gpac_self__";
		for (i=0; i<gf_list_count(ctx->filter_srcs); i++) {
			fsrc = gf_list_get(ctx->filter_srcs, i);
			gf_filter_send_event(fsrc, &evt, GF_FALSE);
		}
	}
	if (!ctx->do_cat
		//only reset if not last entry, so that we keep the last graph setup at the end
		&& next_url_ok
	) {
		while (gf_list_count(ctx->filter_srcs)) {
			fsrc = gf_list_pop_back(ctx->filter_srcs);
			gf_filter_remove_src(filter, fsrc);
		}
	}


	ctx->load_next = GF_FALSE;

	if (! next_url_ok) {
		if (ctx->splice_state==FL_SPLICE_ACTIVE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] No next URL for splice but splice period still active, resuming splice with possible broken coding dependencies!\n"));
			ctx->wait_splice_end = GF_TRUE;
			ctx->splice_state = FL_SPLICE_AFTER;
			ctx->dts_sub_plus_one.num = 1;
			ctx->dts_sub_plus_one.den = 1;
			for (i=0; i<count; i++) {
				FileListPid *iopid = gf_list_get(ctx->io_pids, i);
				iopid->splice_ready = GF_TRUE;
			}
			return GF_OK;
		}

		for (i=0; i<count; i++) {
			iopid = gf_list_get(ctx->io_pids, i);
			gf_filter_pid_set_eos(iopid->opid);
			if (iopid->opid_aux)
				gf_filter_pid_set_eos(iopid->opid_aux);
			iopid->ipid = NULL;
		}
		ctx->is_eos = GF_TRUE;
		return GF_EOS;
	}
	for (i=0; i<gf_list_count(ctx->io_pids); i++) {
		FileListPid *an_iopid = gf_list_get(ctx->io_pids, i);
		if (ctx->do_cat) {
			gf_filter_pid_clear_eos(an_iopid->ipid, GF_TRUE);
			filelist_start_ipid(ctx, an_iopid, an_iopid->timescale, GF_TRUE);
		} else {
			if (ctx->wait_splice_start && !an_iopid->splice_ipid) {
				an_iopid->splice_ipid = an_iopid->ipid;
				an_iopid->dts_o_splice = an_iopid->dts_o;
				an_iopid->cts_o_splice = an_iopid->cts_o;
				an_iopid->dts_sub_splice = an_iopid->dts_sub;
				an_iopid->splice_ra_info = an_iopid->ra_info;
			}
			an_iopid->ipid = NULL;
			an_iopid->ra_info.is_raw = GF_FALSE;
		}
	}

	//lock all filters while loading up chain, to avoid PID init from other threads to resolve again this filter
	//while we setup sourceID
	gf_filter_lock_all(filter, GF_TRUE);
	//reset all our source_ids
	gf_filter_reset_source(filter);

	fsrc = NULL;
	prev_filter = NULL;
	s_idx = 0;
	url = szURL;
	while (url) {
		char c = 0;
		char *sep, *lsep;
		char *sep_f = strstr(url, " @");

		sep = strstr(url, " && ");
		if (!sep && ctx->srcs.nb_items)
			sep = strstr(url, "&&");

		if (sep_f && ctx->do_cat) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] Cannot use filter directives in cat mode\n"));
			gf_filter_lock_all(filter, GF_FALSE);
			return GF_BAD_PARAM;
		}

		if (sep && sep_f) {
			if (sep_f < sep)
				sep = NULL;
			else
				sep_f = NULL;
		}

		if (sep) {
			c = sep[0];
			sep[0] = 0;
		}
		else if (sep_f) {
			sep_f[0] = 0;
		}
		//skip empty
		while (url[0] == ' ')
			url++;


#define SET_SOURCE(_filter, _from) \
			if (link_idx>0) { \
				prev_filter = gf_list_get(filters, (u32) link_idx); \
				if (!prev_filter) { \
					if (filters) gf_list_del(filters); \
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] Invalid link directive, filter index %d does not point to a valid filter\n")); \
					gf_filter_lock_all(filter, GF_FALSE);\
					return GF_SERVICE_ERROR; \
				} \
			}\
			lsep = link_args ? strchr(link_args, ' ') : NULL; \
			if (lsep) lsep[0] = 0; \
			gf_filter_set_source(_filter, _from, link_args); \
			if (lsep) lsep[0] = ' '; \
			link_args = NULL;\
			link_idx=-1;


		if (!url[0]) {
			//last link directive before new source specifier
			if (is_filter_chain && prev_filter && (link_idx>=0)) {
				SET_SOURCE(filter, prev_filter);
				prev_filter = NULL;
				gf_list_reset(filters);
			}
		} else if (ctx->do_cat) {
			char *f_url;
			GF_FilterEvent evt;
			fsrc = gf_list_get(ctx->filter_srcs, s_idx);
			if (!fsrc) {
				if (sep) sep[0] = c;
				else if (sep_f) sep_f[0] = ' ';

				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] More URL to cat than opened service!\n"));
				gf_filter_lock_all(filter, GF_FALSE);
				return GF_BAD_PARAM;
			}
			f_url = gf_url_concatenate(ctx->file_path, url);
			memset(&evt, 0, sizeof(GF_FilterEvent));
			evt.base.type = GF_FEVT_SOURCE_SWITCH;
			evt.seek.source_switch = f_url ? f_url : url;
			evt.seek.start_offset = ctx->start_range;
			evt.seek.end_offset = ctx->end_range;
			gf_filter_send_event(fsrc, &evt, GF_FALSE);
			if (f_url)
				gf_free(f_url);
		} else {
			GF_Filter *f = NULL;
			if (is_filter_chain) {
				f = gf_filter_load_filter(filter, url, &e);
				if (f) gf_filter_require_source_id(f);
			} else {
				fsrc = gf_filter_connect_source(filter, url, ctx->file_path, GF_FALSE, &e);

				if (fsrc) {
					gf_filter_set_setup_failure_callback(filter, fsrc, filelist_on_filter_setup_error, filter);
					gf_filter_require_source_id(fsrc);
				}
			}

			if (e) {
				if (filters) gf_list_del(filters);
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] Failed to open file %s: %s\n", szURL, gf_error_to_string(e) ));

				if (sep) sep[0] = c;
				else if (sep_f) sep_f[0] = ' ';
				if (!ctx->splice_srcs)
					ctx->splice_state = FL_SPLICE_NONE;
				gf_filter_lock_all(filter, GF_FALSE);
				return GF_SERVICE_ERROR;
			}
			if (is_filter_chain) {

				if (!filters) filters = gf_list_new();
				if (!gf_list_count(filters)) gf_list_add(filters, fsrc);

				SET_SOURCE(f, prev_filter);

				//insert at beginning, so that link_idx=0 <=> last declared filter
				gf_list_insert(filters, f, 0);
				prev_filter = f;
			} else {
				gf_list_add(ctx->filter_srcs, fsrc);
			}
		}
		if (!sep && !sep_f) {
			//if prev filter was not set (simple entry), use fsrc
			if (!prev_filter) prev_filter = fsrc;
			break;
		}

		if (sep) {
			sep[0] = c;
			url = (sep[0]==' ') ? sep+4 : sep+2;
			is_filter_chain = GF_FALSE;
			if (prev_filter) {
				gf_filter_set_source(filter, prev_filter, NULL);
				prev_filter = NULL;
			}
			if (filters) gf_list_reset(filters);
			link_idx = -1;
		} else {
			sep_f[0] = ' ';

			if (sep_f[2] != ' ') link_args = sep_f+2;
			else link_args = NULL;

			link_idx = 0;
			if (link_args) {
				if (link_args[0] != '#') {
					sep = strchr(link_args, '#');
					if (sep) {
						sep[0] = 0;
						link_idx = atoi(link_args);
						sep[0] = '#';
						link_args = sep+1;
					} else {
						link_idx = atoi(link_args);
						link_args = NULL;
					}
					// " @-1" directive restart chain from source
					if (link_idx<0) {
						if (prev_filter) {
							gf_filter_set_source(filter, prev_filter, NULL);
							prev_filter = NULL;
						}
						if (filters) gf_list_reset(filters);
					}

				} else {
					link_args++;
				}
			}

			url = strchr(sep_f+2, ' ');
			if (url && (url[1]!='&'))
				url += 1;

			is_filter_chain = GF_TRUE;
			if (!prev_filter) {
				if (!fsrc) {
					if (filters) gf_list_del(filters);
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] Missing source declaration before filter directive\n"));
					gf_filter_lock_all(filter, GF_FALSE);
					return GF_BAD_PARAM;
				}
				prev_filter = fsrc;
			}

			//last empty link
			if (!url && prev_filter) {
				SET_SOURCE(filter, prev_filter);
				prev_filter = NULL;
			}
		}
	}
	//assign last defined filter as source for ourselves
	if (prev_filter) {
		gf_filter_set_source(filter, prev_filter, NULL);
		prev_filter = NULL;
	}

	gf_filter_lock_all(filter, GF_FALSE);

	if (filters) gf_list_del(filters);

	//wait for PIDs to connect
	GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[FileList] Switching to file %s\n", szURL));

	ctx->wait_splice_start = GF_FALSE;
	return GF_OK;
}

static s64 filelist_translate_splice_cts(FileListPid *iopid, u64 cts)
{
	if (iopid->timescale_splice != iopid->o_timescale) {
		cts = gf_timestamp_rescale(cts, iopid->timescale_splice, iopid->o_timescale);
	}
	return iopid->cts_o_splice + cts - iopid->dts_sub_splice;
}

static Bool filelist_check_splice(GF_FileListCtx *ctx)
{
	u64 cts;
	u32 i, count;
	GF_FilterPacket *pck;
	GF_FilterSAPType sap;
	GF_FilterPid *ipid;
	Bool is_raw_audio;
	assert(ctx->splice_ctrl);
	assert(ctx->splice_state);

	ipid = ctx->splice_ctrl->splice_ipid ? ctx->splice_ctrl->splice_ipid : ctx->splice_ctrl->ipid;
	is_raw_audio = ctx->splice_ctrl->splice_ipid ? ctx->splice_ctrl->splice_ra_info.is_raw : ctx->splice_ctrl->ra_info.is_raw;

	pck = gf_filter_pid_get_packet(ipid);
	if (!pck) {
		if (!gf_filter_pid_is_eos(ipid))
			return GF_FALSE;

		if (ctx->splice_state==FL_SPLICE_BEFORE)
			ctx->splice_state = FL_SPLICE_AFTER;

		//if we are in end of stream, abort
		if (ctx->splice_state==FL_SPLICE_ACTIVE) {
			ctx->splice_state = FL_SPLICE_AFTER;
			ctx->splice_end_cts = ctx->spliced_current_cts;

			count = gf_list_count(ctx->io_pids);
			for (i=0; i<count; i++) {
				FileListPid *iopid = gf_list_get(ctx->io_pids, i);
				iopid->splice_ready = GF_FALSE;
			}
			ctx->wait_splice_end = GF_TRUE;
		}
		return GF_TRUE;
	}
	cts = gf_filter_pck_get_cts(pck);
	if (cts==GF_FILTER_NO_TS)
		return GF_TRUE;

	if (ctx->splice_ctrl->splice_delay>=0) {
		cts += ctx->splice_ctrl->splice_delay;
	} else if (cts > - ctx->splice_ctrl->splice_delay) {
		cts += ctx->splice_ctrl->splice_delay;
	} else {
		cts = 0;
	}

	sap = is_raw_audio ? GF_FILTER_SAP_1 : gf_filter_pck_get_sap(pck);
	if (ctx->splice_state==FL_SPLICE_BEFORE) {
		ctx->spliced_current_cts = filelist_translate_splice_cts(ctx->splice_ctrl, cts);
		if (sap && (sap <= GF_FILTER_SAP_3)) {
			u64 check_ts = cts;
			if (is_raw_audio) {
				check_ts += gf_filter_pck_get_duration(pck);
				check_ts -= 1;
			}

			if (ctx->splice_start.num==-1) {
				char szURL[GF_MAX_PATH];
				filelist_next_url(NULL, ctx, szURL, GF_TRUE);
			} else if (ctx->splice_start.num==-2) {
				ctx->splice_start.num = cts;
				ctx->splice_start.den = ctx->splice_ctrl->timescale_splice;
			} else if (ctx->splice_start.num==-3) {
				u64 now = gf_net_get_utc();
				if (now >= ctx->splice_start.den) {
					ctx->splice_start.num = cts;
					ctx->splice_start.den = ctx->splice_ctrl->timescale_splice;
				}
			}

			//we're entering the splice period
			if ((ctx->splice_start.num >= 0)
				&& gf_timestamp_greater_or_equal(check_ts, ctx->splice_ctrl->timescale_splice, ctx->splice_start.num, ctx->splice_start.den)
			) {
				//cts larger than splice end, move directly to splice_in state
				if ((ctx->splice_end.num >= 0)
					&& !ctx->flags_splice_end
					&& gf_timestamp_greater_or_equal(cts, ctx->splice_ctrl->timescale_splice, ctx->splice_end.num, ctx->splice_end.den)
				) {
					ctx->splice_state = FL_SPLICE_AFTER;
					ctx->splice_end_cts = filelist_translate_splice_cts(ctx->splice_ctrl, cts);
					return GF_TRUE;
				}
				if (is_raw_audio) {
					u64 ts_diff = gf_timestamp_rescale(ctx->splice_ctrl->timescale_splice, ctx->splice_start.den, ctx->splice_start.num);
					if (ts_diff >= cts) {
						ts_diff -= cts;
						cts += ts_diff;
					}
				}
				ctx->splice_state = FL_SPLICE_ACTIVE;
				ctx->splice_start_cts = filelist_translate_splice_cts(ctx->splice_ctrl, cts);
				//we just activate splice, iopid->dts_sub_splice is not set yet !!
				assert(!ctx->splice_ctrl->dts_sub_splice);
				ctx->splice_start_cts -= ctx->splice_ctrl->dts_sub;

				if (ctx->flags_splice_end & FL_SPLICE_DELTA) {
					ctx->splice_start.num = cts;
					ctx->splice_start.den = ctx->splice_ctrl->timescale_splice;
				}

				count = gf_list_count(ctx->io_pids);
				for (i=0; i<count; i++) {
					FileListPid *iopid = gf_list_get(ctx->io_pids, i);
					iopid->splice_ready = GF_FALSE;
				}
				//signal on controler that we are ready for splicing
				ctx->splice_ctrl->splice_ready = GF_TRUE;
				//but wait for all other streams to be ready too
				ctx->wait_splice_start = GF_TRUE;
			}
		}
	}

	if (ctx->splice_state==FL_SPLICE_ACTIVE) {
		u64 check_ts = cts;
		if (is_raw_audio) {
			check_ts += gf_filter_pck_get_duration(pck);
			check_ts -= 1;
		}

		if (ctx->flags_splice_end & FL_SPLICE_DELTA) {
			GF_Fraction64 s_end = ctx->splice_start;
			u64 diff = ctx->splice_end.num;
			diff *= ctx->splice_start.den;
			diff /= ctx->splice_end.den;
			s_end.num += diff;
			ctx->splice_end = s_end;
			ctx->flags_splice_end = 0;
		}
		else if (ctx->splice_end.num==-1) {
			char szURL[GF_MAX_PATH];
			filelist_next_url(NULL, ctx, szURL, GF_TRUE);
		} else if (sap && (sap <= GF_FILTER_SAP_3)) {
			if (ctx->splice_end.num==-2) {
				ctx->splice_end.num = cts;
				ctx->splice_end.den = ctx->splice_ctrl->timescale_splice;
			} else if (ctx->splice_end.num==-3) {
				u64 now = gf_net_get_utc();
				if (now >= ctx->splice_start.den) {
					ctx->splice_end.num = cts;
					ctx->splice_end.den = ctx->splice_ctrl->timescale_splice;
				}
			}
		}

		if (sap && (sap <= GF_FILTER_SAP_3)
			&& (ctx->splice_end.num >= 0)
			&& gf_timestamp_greater_or_equal(check_ts, ctx->splice_ctrl->timescale_splice, ctx->splice_end.num, ctx->splice_end.den)
		) {
			ctx->splice_state = FL_SPLICE_AFTER;

			if (is_raw_audio) {
				u64 ts_diff = gf_timestamp_rescale(ctx->splice_ctrl->timescale_splice, ctx->splice_end.den, ctx->splice_end.num);
				if (ts_diff >= cts) {
					ts_diff -= cts;
					cts += ts_diff;
				}
			}


			ctx->splice_end_cts = filelist_translate_splice_cts(ctx->splice_ctrl, cts);

			count = gf_list_count(ctx->io_pids);
			for (i=0; i<count; i++) {
				FileListPid *iopid = gf_list_get(ctx->io_pids, i);
				iopid->splice_ready = GF_FALSE;
			}
			//do not signal splice ready on controler yet, we need to wait for the first frame in the splice content to have cts >= splice end
			ctx->wait_splice_end = GF_TRUE;
			return GF_TRUE;
		}

		ctx->spliced_current_cts = filelist_translate_splice_cts(ctx->splice_ctrl, cts);
	}

	return GF_TRUE;
}

void filelist_copy_raw_audio(FileListPid *iopid, const u8 *src, u32 src_size, u32 offset, u8 *dst, u32 nb_samp, RawAudioInfo *ra)
{
	if (iopid->ra_info.planar) {
		u32 i, bps, stride;
		stride = src_size / ra->nb_ch;
		bps = ra->abps / ra->nb_ch;
		for (i=0; i<ra->nb_ch; i++) {
			memcpy(dst + i*bps*nb_samp, src + i*stride + offset * bps, nb_samp * bps);
		}
	} else {
		memcpy(dst, src + offset * ra->abps, nb_samp * ra->abps);
	}
}

static void filelist_forward_splice_pck(FileListPid *iopid, GF_FilterPacket *pck)
{
	if (iopid->audio_samples_to_keep) {
		u32 nb_samp, dur;
		u64 cts;
		u8 *output;
		const u8 *data;
		GF_FilterPacket *dst_pck;
		u32 pck_size, osize, offset=0;

		cts = gf_filter_pck_get_cts(pck);

		data = gf_filter_pck_get_data(pck, &pck_size);
		if (iopid->audio_samples_to_keep>0) {
			nb_samp = iopid->audio_samples_to_keep;
			iopid->audio_samples_to_keep = -iopid->audio_samples_to_keep;
		} else {
			nb_samp = pck_size / iopid->splice_ra_info.abps + iopid->audio_samples_to_keep;
			offset = -iopid->audio_samples_to_keep;
			iopid->audio_samples_to_keep = 0;
		}
		osize = nb_samp * iopid->splice_ra_info.abps;
		dst_pck = gf_filter_pck_new_alloc(iopid->opid, osize, &output);
		if (!dst_pck) return;

		filelist_copy_raw_audio(iopid, data, pck_size, offset, output, nb_samp, &iopid->splice_ra_info);

		dur = nb_samp;
		if (iopid->timescale_splice != iopid->splice_ra_info.sample_rate) {
			dur = (u32) gf_timestamp_rescale(dur, iopid->splice_ra_info.sample_rate, iopid->timescale_splice);
			offset = (u32) gf_timestamp_rescale(offset, iopid->splice_ra_info.sample_rate, iopid->timescale_splice);
		}
		cts += offset;
		gf_filter_pck_set_cts(dst_pck, cts);
		gf_filter_pck_set_dts(dst_pck, cts);
		gf_filter_pck_set_duration(dst_pck, dur);

		gf_filter_pck_send(dst_pck);
	} else {
		gf_filter_pck_forward(pck, iopid->opid);
	}
}

static void filelist_purge_slice(GF_FileListCtx *ctx)
{
	u32 i, count = gf_list_count(ctx->io_pids);
	assert(ctx->splice_ctrl);

	if (ctx->mark_only)
		return;
	assert(ctx->splice_ctrl->splice_ipid);

	for (i=0; i<count; i++) {
		FileListPid *iopid = gf_list_get(ctx->io_pids, i);
		if (iopid == ctx->splice_ctrl) continue;
		if (!iopid->splice_ipid) continue;

		while (1) {
			u64 cts;
			GF_FilterPacket *pck = gf_filter_pid_get_packet(iopid->splice_ipid);
			if (!pck) break;

			cts = gf_filter_pck_get_cts(pck);
			if (cts==GF_FILTER_NO_TS) cts=0;

			cts = filelist_translate_splice_cts(iopid, cts);
			if (gf_timestamp_greater(cts, iopid->o_timescale, ctx->spliced_current_cts, ctx->splice_ctrl->o_timescale))
				break;

			if (ctx->keep_splice) {
				GF_FilterPacket *pck = gf_filter_pid_get_packet(iopid->splice_ipid);
				filelist_forward_splice_pck(iopid, pck);
			}
			gf_filter_pid_drop_packet(iopid->splice_ipid);
		}
	}
	if (ctx->keep_splice) {
		GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->splice_ctrl->splice_ipid);
		filelist_forward_splice_pck(ctx->splice_ctrl, pck);
	}

	gf_filter_pid_drop_packet(ctx->splice_ctrl->splice_ipid);
}

void filelist_send_packet(GF_FileListCtx *ctx, FileListPid *iopid, GF_FilterPacket *pck, Bool is_splice_forced)
{
	GF_FilterPacket *dst_pck;
	u32 dur;
	u64 dts, cts;
	if (iopid->audio_samples_to_keep) {
		u32 nb_samp;
		u8 *output;
		const u8 *data;
		u32 pck_size, osize, offset=0;
		RawAudioInfo *ra = is_splice_forced ? &iopid->splice_ra_info : &iopid->ra_info;

		data = gf_filter_pck_get_data(pck, &pck_size);
		if (iopid->audio_samples_to_keep>0) {
			nb_samp = iopid->audio_samples_to_keep;
		} else {
			nb_samp = pck_size / ra->abps + iopid->audio_samples_to_keep;
			offset = -iopid->audio_samples_to_keep;
		}

		osize = ABS(iopid->audio_samples_to_keep) * ra->abps;
		dst_pck = gf_filter_pck_new_alloc((!is_splice_forced && iopid->opid_aux) ? iopid->opid_aux : iopid->opid, osize, &output);
		if (!dst_pck) return;

		filelist_copy_raw_audio(iopid, data, pck_size, offset, output, nb_samp, ra);
	} else {
		dst_pck = gf_filter_pck_new_ref(iopid->opid_aux ? iopid->opid_aux : iopid->opid, 0, 0, pck);
		if (!dst_pck) return;
	}
	gf_filter_pck_merge_properties(pck, dst_pck);

	dts = gf_filter_pck_get_dts(pck);
	if (dts==GF_FILTER_NO_TS) dts=0;

	cts = gf_filter_pck_get_cts(pck);
	if (cts==GF_FILTER_NO_TS) cts=0;

	if (iopid->single_frame && (ctx->fsort==FL_SORT_DATEX) ) {
		dur = (u32) ctx->current_file_dur;
		//move from second to input pid timescale
		dur *= iopid->timescale;
	} else if (iopid->single_frame && ctx->fdur.num && ctx->fdur.den) {
		s64 pdur = ctx->fdur.num;
		pdur *= iopid->timescale;
		pdur /= ctx->fdur.den;
		dur = (u32) pdur;
	} else if (iopid->audio_samples_to_keep) {
		RawAudioInfo *ra = is_splice_forced ? &iopid->splice_ra_info : &iopid->ra_info;
		if (iopid->audio_samples_to_keep>0) {
			dur = iopid->audio_samples_to_keep;
			if ( (iopid->splice_ipid && (iopid->splice_ipid != iopid->ipid)) || (!ctx->keep_splice && !ctx->mark_only))
				iopid->audio_samples_to_keep = 0;
			else
				iopid->audio_samples_to_keep = -iopid->audio_samples_to_keep;
		} else {
			u32 pck_size;
			gf_filter_pck_get_data(pck, &pck_size);
			dur = pck_size/ra->abps + iopid->audio_samples_to_keep;
			cts += -iopid->audio_samples_to_keep;
			dts += -iopid->audio_samples_to_keep;
			iopid->audio_samples_to_keep = 0;
		}
		if (iopid->timescale != ra->sample_rate) {
			dur = (u32) gf_timestamp_rescale(dur, ra->sample_rate, iopid->timescale);
		}
	} else {
		dur = gf_filter_pck_get_duration(pck);
	}

	if (iopid->timescale == iopid->o_timescale) {
		gf_filter_pck_set_dts(dst_pck, iopid->dts_o + dts - iopid->dts_sub);
		gf_filter_pck_set_cts(dst_pck, iopid->cts_o + cts - iopid->dts_sub);
		gf_filter_pck_set_duration(dst_pck, dur);
	} else {
		u64 ts = gf_timestamp_rescale(dts, iopid->timescale, iopid->o_timescale);
		gf_filter_pck_set_dts(dst_pck, iopid->dts_o + ts - iopid->dts_sub);

		ts = gf_timestamp_rescale(cts, iopid->timescale, iopid->o_timescale);
		gf_filter_pck_set_cts(dst_pck, iopid->cts_o + ts - iopid->dts_sub);

		ts = gf_timestamp_rescale(dur, iopid->timescale, iopid->o_timescale);
		gf_filter_pck_set_duration(dst_pck, (u32) ts);
	}
	dts += dur;
	cts += dur;

	if (iopid->delay>=0) {
		cts += iopid->delay;
	} else if (cts > - iopid->delay) {
		cts += iopid->delay;
	} else {
		cts = 0;
	}

	if (!is_splice_forced) {
		if (dts > iopid->max_dts)
			iopid->max_dts = dts;
		if (cts > iopid->max_cts)
			iopid->max_cts = cts;

		//remember our first DTS
		if (!iopid->first_dts_plus_one) {
			iopid->first_dts_plus_one = dts + 1;
		}

		if (iopid->send_cue) {
			iopid->send_cue = GF_FALSE;
			gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_CUE_START, &PROP_BOOL(GF_TRUE));
		}
	}

	gf_filter_pck_send(dst_pck);

	if (!iopid->audio_samples_to_keep) {
		gf_filter_pid_drop_packet(iopid->ipid);
	}
}

static GF_Err filelist_process(GF_Filter *filter)
{
	Bool start, end, purge_splice = GF_FALSE;
	u32 i, count, nb_done, nb_inactive, nb_stop, nb_ready;
	FileListPid *iopid;
	GF_FileListCtx *ctx = gf_filter_get_udta(filter);


	if (!ctx->file_list) {
		GF_FilterPacket *pck;
		if (!ctx->file_pid) {
			return GF_EOS;
		}
		pck = gf_filter_pid_get_packet(ctx->file_pid);
		if (pck) {
			gf_filter_pck_get_framing(pck, &start, &end);
			gf_filter_pid_drop_packet(ctx->file_pid);

			if (end) {
				const GF_PropertyValue *p;
				Bool is_first = GF_TRUE;
				FILE *f=NULL;
				p = gf_filter_pid_get_property(ctx->file_pid, GF_PROP_PID_FILEPATH);
				if (p) {
					char *frag;
					if (ctx->file_path) {
						gf_free(ctx->file_path);
						is_first = GF_FALSE;
					}
					ctx->file_path = gf_strdup(p->value.string);
					frag = strchr(ctx->file_path, '#');
					if (frag) {
						frag[0] = 0;
						ctx->frag_url = gf_strdup(frag+1);
					}
					f = gf_fopen(ctx->file_path, "rt");
				}
				if (!f) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] Unable to open file %s\n", ctx->file_path ? ctx->file_path : "no source path"));
					return GF_SERVICE_ERROR;
				} else {
					gf_fclose(f);
					ctx->load_next = is_first;
				}
			}
		}
	}
	if (ctx->is_eos)
		return GF_EOS;

	if (ctx->load_next) {
		return filelist_load_next(filter, ctx);
	}


	count = gf_list_count(ctx->io_pids);
	//init first timestamp
	if (!ctx->dts_sub_plus_one.num) {
		u32 nb_eos = 0;

		for (i=0; i<gf_list_count(ctx->filter_srcs); i++) {
			GF_Filter *fsrc = gf_list_get(ctx->filter_srcs, i);
			if (gf_filter_has_pid_connection_pending(fsrc, filter)) {
				return GF_OK;
			}
		}

		for (i=0; i<count; i++) {
			GF_FilterPacket *pck;
			u64 dts;
			iopid = gf_list_get(ctx->io_pids, i);
            if (!iopid->ipid) {
				if (ctx->src_error) {
					nb_eos++;
					continue;
				}

				if (iopid->opid) gf_filter_pid_set_eos(iopid->opid);
				if (iopid->opid_aux) gf_filter_pid_set_eos(iopid->opid_aux);
				return GF_OK;
			}
            if (iopid->skip_dts_init) continue;
            pck = gf_filter_pid_get_packet(iopid->ipid);

			if (!pck) {
				if (gf_filter_pid_is_eos(iopid->ipid) || (iopid->play_state==FLIST_STATE_STOP)) {
					nb_eos++;
					continue;
				}
				ctx->dts_sub_plus_one.num = 0;
				return GF_OK;
			}


			dts = gf_filter_pck_get_dts(pck);
			if (dts==GF_FILTER_NO_TS)
				dts = gf_filter_pck_get_cts(pck);
			if (dts==GF_FILTER_NO_TS)
				dts = 0;

			//make sure we start all streams on a SAP
			if (!iopid->wait_rap && gf_filter_pck_get_seek_flag(pck)) {
				gf_filter_pid_drop_packet(iopid->ipid);
				pck = NULL;
			}
			else if (iopid->wait_rap && !iopid->ra_info.is_raw && !gf_filter_pck_get_sap(pck)) {
				gf_filter_pid_drop_packet(iopid->ipid);
				pck = NULL;
			}
			if (!pck) {
				iopid->wait_rap = GF_TRUE;
				if (!ctx->wait_dts_plus_one.num
					|| gf_timestamp_greater(dts + 1, iopid->timescale, ctx->wait_dts_plus_one.num, ctx->wait_dts_plus_one.den)
				) {
					ctx->wait_dts_plus_one.num = dts + 1;
					ctx->wait_dts_plus_one.den = iopid->timescale;
				}
				ctx->dts_sub_plus_one.num = 0;
				return GF_OK;
			}
			if (ctx->wait_dts_plus_one.num
				&& gf_timestamp_less(dts, iopid->timescale, (ctx->wait_dts_plus_one.num - 1), ctx->wait_dts_plus_one.den)
			) {
				gf_filter_pid_drop_packet(iopid->ipid);
				iopid->wait_rap = GF_TRUE;
				ctx->dts_sub_plus_one.num = 0;
				return GF_OK;
			}
			if (iopid->wait_rap) {
				iopid->wait_rap = GF_FALSE;
				ctx->dts_sub_plus_one.num = 0;
				return GF_OK;
			}
			if (!ctx->dts_sub_plus_one.num
				|| gf_timestamp_less(dts, iopid->timescale, (ctx->dts_sub_plus_one.num - 1), ctx->dts_sub_plus_one.den)
			) {
				ctx->dts_sub_plus_one.num = dts + 1;
				ctx->dts_sub_plus_one.den = iopid->timescale;
			}
	 	}
	 	ctx->src_error = GF_FALSE;
	 	if (nb_eos) {
			if (nb_eos==count) {
				//force load
				ctx->load_next = GF_TRUE;
				return filelist_process(filter);
			}
			return GF_OK;
		}

		//if we start with a splice, set first dst_sub to 0 to keep timestamps untouched
		if (!ctx->splice_state) {
			ctx->first_loaded = GF_TRUE;
			ctx->skip_sync = GF_FALSE;
		} else if (!ctx->first_loaded) {
			ctx->dts_sub_plus_one.num = 1;
			ctx->dts_sub_plus_one.den = 1;
			ctx->first_loaded = 1;
			ctx->skip_sync = GF_FALSE;
		}

		for (i=0; i<count; i++) {
			iopid = gf_list_get(ctx->io_pids, i);
			iopid->dts_sub = gf_timestamp_rescale(ctx->dts_sub_plus_one.num - 1, ctx->dts_sub_plus_one.den, iopid->o_timescale);
		}
		ctx->wait_dts_plus_one.num = 0;
	}

	if (ctx->splice_state) {
		if (!filelist_check_splice(ctx))
			return GF_OK;
	}

	nb_done = nb_inactive = nb_stop = nb_ready = 0;
	for (i=0; i<count; i++) {
		iopid = gf_list_get(ctx->io_pids, i);
		if (!iopid->ipid) {
            iopid->splice_ready = GF_TRUE;
			nb_inactive++;
			continue;
		}
		if (iopid->play_state==FLIST_STATE_WAIT_PLAY)
			continue;
        if (iopid->play_state==FLIST_STATE_STOP) {
			nb_stop++;
            //in case the input still dispatch packets, drop them
            while (1) {
                GF_FilterPacket *pck = gf_filter_pid_get_packet(iopid->ipid);
                if (!pck) break;
                gf_filter_pid_drop_packet(iopid->ipid);
            }
            while (iopid->splice_ipid) {
                GF_FilterPacket *pck = gf_filter_pid_get_packet(iopid->splice_ipid);
                if (!pck) break;
                gf_filter_pid_drop_packet(iopid->splice_ipid);
            }
            nb_done++;
            iopid->splice_ready = GF_TRUE;
            continue;
        }
		if (iopid->is_eos) {
            iopid->splice_ready = GF_TRUE;
			nb_done++;
			continue;
		}

		while (1) {
			u64 cts;
			GF_FilterPacket *pck;

			pck = gf_filter_pid_get_packet(iopid->ipid);
			if (!pck) {
				if (gf_filter_pid_is_eos(iopid->ipid)) {
					if (ctx->wait_splice_end) {
						iopid->splice_ready = GF_TRUE;
					} else if (ctx->wait_splice_start) {
						iopid->splice_ready = GF_TRUE;
					} else {
						iopid->is_eos = GF_TRUE;
						if (ctx->splice_state==FL_SPLICE_ACTIVE)
							purge_splice = GF_TRUE;
					}
				}

				if (iopid->is_eos)
					nb_done++;
				break;
			}

			if (gf_filter_pid_would_block(iopid->opid) && (!iopid->opid_aux || gf_filter_pid_would_block(iopid->opid_aux)))
				break;

			cts = gf_filter_pck_get_cts(pck);
			if (ctx->splice_state && (cts != GF_FILTER_NO_TS)) {
				u32 dur = gf_filter_pck_get_duration(pck);

				if (iopid->delay>=0) {
					cts += iopid->delay;
				} else if (cts > - iopid->delay) {
					cts += iopid->delay;
				} else {
					cts = 0;
				}

				//translate cts in output timescale, and compare with splice
				if (iopid->timescale != iopid->o_timescale) {
					cts = gf_timestamp_rescale(cts, iopid->timescale, iopid->o_timescale);
					dur = (u32) gf_timestamp_rescale(dur, iopid->timescale, iopid->o_timescale);
				}
				if (iopid->cts_o + cts >= iopid->dts_sub)
					cts = iopid->cts_o + cts - iopid->dts_sub;
				else
					cts = 0;

				//about to enter the splice period
				if (ctx->splice_state==FL_SPLICE_BEFORE) {
					//do not dispatch yet if cts is greater than last CTS seen on splice control pid
					if (gf_timestamp_greater(cts, iopid->o_timescale, ctx->spliced_current_cts, ctx->splice_ctrl->o_timescale))
						break;
				}
				//in the splice period
				else if (ctx->splice_state==FL_SPLICE_ACTIVE) {
					u64 check_ts = cts;

					if (iopid->ra_info.is_raw) {
						check_ts += dur;
						check_ts -= 1;
					}

					//packet in splice range
					if (gf_timestamp_greater_or_equal(check_ts, iopid->o_timescale, ctx->splice_start_cts, ctx->splice_ctrl->o_timescale)) {
						Bool keep_pck = GF_FALSE;
						//waiting for all streams to reach splice out point (packet is from main content)
						//don't drop packet yet in case splice content is not ready
						if (ctx->wait_splice_start) {
							if (iopid->ra_info.is_raw && iopid->ra_info.sample_rate && !iopid->audio_samples_to_keep) {
								u64 ts_diff = gf_timestamp_rescale(ctx->splice_start_cts, ctx->splice_ctrl->o_timescale, iopid->o_timescale);
								if (ts_diff >= cts) {
									ts_diff -= cts;
									if (iopid->ra_info.sample_rate != iopid->o_timescale) {
										ts_diff = gf_timestamp_rescale(ts_diff,  iopid->o_timescale, iopid->ra_info.sample_rate);
									}
									iopid->audio_samples_to_keep = (s32) ts_diff;
									if (ts_diff) keep_pck = GF_TRUE;
								}
							}
							if (!keep_pck) {
								iopid->splice_ready = GF_TRUE;
								break;
							}
						}
						if (!keep_pck && (iopid == ctx->splice_ctrl))
							purge_splice = GF_TRUE;

						//do not dispatch yet if cts is greater than last CTS seen on splice control pid
						if (gf_timestamp_greater(cts, iopid->o_timescale, ctx->spliced_current_cts, ctx->splice_ctrl->o_timescale))
							break;

					}
					//should not happen
					else if (!ctx->wait_splice_start) {
						gf_filter_pid_drop_packet(iopid->ipid);
						continue;
					}
				}
				//leaving the splice period
				else if (ctx->splice_state==FL_SPLICE_AFTER) {
					//still in spliced content
					if (ctx->wait_splice_end) {
						u64 check_ts = cts;
						if (iopid->ra_info.is_raw) {
							check_ts += dur;
							check_ts -= 1;
						}

						if (
							//packet is after splice end, drop
							gf_timestamp_greater_or_equal(check_ts, iopid->o_timescale, ctx->splice_end_cts, ctx->splice_ctrl->o_timescale)
							//packet is before splice end but a previous packet was dropped because after splice end (i.e. we dropped a ref), drop
							|| iopid->splice_ready
						) {
							Bool do_break = GF_TRUE;
							if (iopid->ra_info.is_raw && !iopid->audio_samples_to_keep) {
								u64 ts_diff = gf_timestamp_rescale(ctx->splice_end_cts, ctx->splice_ctrl->o_timescale, iopid->o_timescale);
								if (ts_diff >= cts) {
									ts_diff -= cts;
									if (iopid->ra_info.sample_rate != iopid->o_timescale) {
										ts_diff = gf_timestamp_rescale(ts_diff, iopid->o_timescale, iopid->ra_info.sample_rate);
									}
									iopid->audio_samples_to_keep = (s32) ts_diff;
									if (ts_diff)
										do_break = GF_FALSE;
								}
							}
							if (do_break) {
								iopid->splice_ready = GF_TRUE;
								if (!ctx->mark_only)
									gf_filter_pid_drop_packet(iopid->ipid);
								break;
							}
						}
					} else {
						//out of spliced content, packet is from main:
						//drop packet if before CTS of splice end point (open gop or packets from other PIDs not yet discarded during splice)
						u64 check_ts = cts;
						if (iopid->ra_info.is_raw) {
							check_ts += dur;
							check_ts -= 1;
						}
						if (!iopid->audio_samples_to_keep
							&& gf_timestamp_less(check_ts, iopid->o_timescale, ctx->splice_end_cts, ctx->splice_ctrl->o_timescale)
						) {
							//do not drop if not raw audio and we were in keep/mark mode
							if (iopid->ra_info.is_raw || !ctx->was_kept) {
								gf_filter_pid_drop_packet(iopid->ipid);
								break;
							}
						}
						if (ctx->wait_splice_start) {
							iopid->splice_ready = GF_FALSE;
						}
					}
				}
			}

			if (ctx->wait_source) {
				nb_ready++;
				break;
			}

			filelist_send_packet(ctx, iopid, pck, GF_FALSE);

			//if we have an end range, compute max_dts (includes dur) - first_dts
			if (ctx->stop > ctx->start) {
				if ( (ctx->stop - ctx->start) * iopid->timescale <= (iopid->max_dts - iopid->first_dts_plus_one + 1)) {
					GF_FilterEvent evt;
					GF_FEVT_INIT(evt, GF_FEVT_STOP, iopid->ipid)
					gf_filter_pid_send_event(iopid->ipid, &evt);
					gf_filter_pid_set_discard(iopid->ipid, GF_TRUE);
					iopid->is_eos = GF_TRUE;
					nb_done++;
					break;
				}
			}
		}
	}

	if (purge_splice) {
		filelist_purge_slice(ctx);
	}
	if (ctx->wait_source) {
		if (nb_ready + nb_inactive + nb_done == count) {
			ctx->wait_source = GF_FALSE;

			if (ctx->splice_state==FL_SPLICE_AFTER)
				ctx->splice_state = FL_SPLICE_BEFORE;
		}
	}


	if (ctx->wait_splice_end) {
		Bool ready = GF_TRUE;
		for (i=0; i<count; i++) {
			iopid = gf_list_get(ctx->io_pids, i);
			if (!iopid->ipid) continue;
			if (!iopid->splice_ready) {
				ready = GF_FALSE;
				break;
			}
		}
		if (!ready)
			return GF_OK;

		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[FileList] Splice end reached, resuming main content\n"));
		ctx->wait_splice_end = GF_FALSE;
		ctx->nb_repeat = ctx->splice_nb_repeat;
		ctx->splice_nb_repeat = 0;

		//reset props pushed by splice
		if (ctx->pid_props) gf_free(ctx->pid_props);
		ctx->pid_props = ctx->splice_pid_props;
		ctx->splice_pid_props = NULL;
		ctx->skip_sync = GF_FALSE;

		for (i=0; i<count; i++) {
			iopid = gf_list_get(ctx->io_pids, i);
			iopid->splice_ready = GF_FALSE;
			//detach
			if (!ctx->mark_only && iopid->ipid) {
				gf_filter_pid_set_udta(iopid->ipid, NULL);
				gf_filter_pid_set_discard(iopid->ipid, GF_TRUE);
			}

			if (iopid->splice_ipid) {
				GF_FilterPacket *pck;
				iopid->ipid = iopid->splice_ipid;
				iopid->dts_o = iopid->dts_o_splice;
				iopid->cts_o = iopid->cts_o_splice;
				iopid->dts_sub = iopid->dts_sub_splice;
				iopid->dts_sub_splice = 0;
				iopid->dts_o_splice = 0;
				iopid->cts_o_splice = 0;
				iopid->splice_ipid = NULL;
				iopid->ra_info = iopid->splice_ra_info;
				iopid->splice_ra_info.is_raw = GF_FALSE;
				iopid->timescale = iopid->timescale_splice;

				//if spliced media was raw audio, we may need to forward or truncate part of the packet before switching properties
				pck = iopid->ra_info.is_raw ? gf_filter_pid_get_packet(iopid->ipid) : NULL;
				if (pck) {
					u64 cts = gf_filter_pck_get_cts(pck);
					u64 check_ts = cts + gf_filter_pck_get_duration(pck) - 1;

					if (gf_timestamp_less(cts, iopid->timescale, ctx->splice_end_cts, ctx->splice_ctrl->timescale)) {
						if (gf_timestamp_greater(check_ts, iopid->timescale, ctx->splice_end_cts, ctx->splice_ctrl->timescale)) {
							u64 diff_ts = gf_timestamp_rescale(ctx->splice_end_cts, ctx->splice_ctrl->timescale, iopid->timescale);
							diff_ts -= cts;

							if (iopid->timescale != iopid->ra_info.sample_rate) {
								diff_ts = gf_timestamp_rescale(diff_ts, iopid->timescale, iopid->ra_info.sample_rate);
							}
							iopid->audio_samples_to_keep = (s32) diff_ts;
							if (ctx->keep_splice) {
								filelist_send_packet(ctx, iopid, pck, GF_TRUE);
							} else {
								iopid->audio_samples_to_keep = -iopid->audio_samples_to_keep;
							}
						}
					}

				}
			} else if (!ctx->mark_only) {
				iopid->ipid = NULL;
			}
			iopid->is_eos = GF_FALSE;
		}
		if (!ctx->mark_only) {
			while (gf_list_count(ctx->filter_srcs)) {
				GF_Filter *fsrc = gf_list_pop_back(ctx->filter_srcs);
				gf_filter_remove_src(filter, fsrc);
			}
			gf_list_del(ctx->filter_srcs);
			ctx->filter_srcs = ctx->splice_srcs;
			ctx->splice_srcs = NULL;
		} else {
			for (i=0; i<count; i++) {
				iopid = gf_list_get(ctx->io_pids, i);
				if (!iopid->ipid) continue;
				gf_filter_pid_set_property_str(iopid->opid, "period_resume", NULL);
				gf_filter_pid_set_property_str(iopid->opid, "period_resume", &PROP_STRING(ctx->dyn_period_id ? ctx->dyn_period_id : "") );
				gf_filter_pid_set_property_str(iopid->opid, "period_switch", NULL);
				gf_filter_pid_set_property_str(iopid->opid, "period_switch", &PROP_BOOL(GF_TRUE) );
			}
		}
		ctx->was_kept = (ctx->mark_only || ctx->keep_splice) ? GF_TRUE : GF_FALSE;

		ctx->mark_only = GF_FALSE;


		ctx->splice_state = FL_SPLICE_AFTER;
		if (ctx->keep_splice) {
			for (i=0; i<count; i++) {
				iopid = gf_list_get(ctx->io_pids, i);
				if (iopid->opid_aux) {
					gf_filter_pid_set_eos(iopid->opid_aux);
					gf_filter_pid_remove(iopid->opid_aux);
					iopid->opid_aux = NULL;
				}
			}
			ctx->keep_splice = GF_FALSE;

			if (ctx->splice_props) {
				gf_free(ctx->splice_props);
				ctx->splice_props = NULL;
			}
		}

		//spliced media is still active, restore our timeline as it was before splicing
		if (! gf_filter_pid_is_eos(ctx->splice_ctrl->ipid)) {

			ctx->cur_splice_index ++;
			//force a reconfig
			for (i=0; i<count; i++) {
				iopid = gf_list_get(ctx->io_pids, i);
				if (iopid->ipid) {
					filelist_configure_pid(filter, iopid->ipid, GF_FALSE);
					gf_filter_pid_set_property_str(iopid->opid, "period_resume", &PROP_STRING(ctx->dyn_period_id ? ctx->dyn_period_id : "") );
				}
			}

			ctx->cts_offset = ctx->cts_offset_at_splice;
			ctx->dts_offset = ctx->dts_offset_at_splice;
			ctx->dts_sub_plus_one = ctx->dts_sub_plus_one_at_splice;
			//set splice start as undefined to probe for new splice time
			ctx->splice_start.num = -1;
			ctx->splice_start.den = 1;
			ctx->splice_end.num = -1;
			ctx->splice_end.den = 1;
			//make sure we have a packet ready on each input pid before dispatching packets from the main content
			//so that we trigger synchronized reconfig on all pids
			ctx->wait_source = GF_TRUE;
			return GF_OK;
		}

		for (i=0; i<count; i++) {
			iopid = gf_list_get(ctx->io_pids, i);
			if (!iopid->ipid) continue;
			gf_filter_pid_set_udta(iopid->ipid, NULL);
			gf_filter_pid_set_discard(iopid->ipid, GF_TRUE);
		}
		//spliced media is done, load next
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[FileList] Spliced media is over, switching to next item in playlist\n"));
		nb_inactive = 0;
		nb_done = count;
		ctx->cur_splice_index = 0;
		ctx->splice_state = ctx->nb_repeat ? FL_SPLICE_BEFORE : FL_SPLICE_NONE;
	}

	if (ctx->wait_splice_start) {
		Bool ready = GF_TRUE;
		for (i=0; i<count; i++) {
			iopid = gf_list_get(ctx->io_pids, i);
			if (!iopid->ipid) continue;
			if (!iopid->splice_ready) {
				ready = GF_FALSE;
				break;
			}
		}
		if (!ready)
			return GF_OK;

		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[FileList] Splice start reached, loading splice content\n"));
		ctx->splice_nb_repeat = ctx->nb_repeat;
		ctx->nb_repeat = 0;
		ctx->init_start = ctx->start;
		ctx->init_stop = ctx->stop;
		ctx->was_kept = GF_FALSE;
		ctx->stop = ctx->start = 0;
		nb_inactive = 0;
		nb_done = count;

		assert(!ctx->splice_pid_props);
		ctx->splice_pid_props = ctx->pid_props;
		ctx->pid_props = NULL;

		ctx->cts_offset_at_splice = ctx->cts_offset;
		ctx->dts_offset_at_splice = ctx->dts_offset;
		ctx->dts_sub_plus_one_at_splice = ctx->dts_sub_plus_one;

		if (ctx->mark_only) {
			ctx->cur_splice_index ++;
			for (i=0; i<count; i++) {
				iopid = gf_list_get(ctx->io_pids, i);
				if (iopid->ipid) {
					gf_filter_pid_set_property_str(iopid->opid, "period_resume", NULL);
					gf_filter_pid_set_property_str(iopid->opid, "period_resume", &PROP_STRING(ctx->dyn_period_id ? ctx->dyn_period_id : "") );
					gf_filter_pid_set_property_str(iopid->opid, "period_switch", NULL);
					gf_filter_pid_set_property_str(iopid->opid, "period_switch", &PROP_BOOL(GF_TRUE) );
				}
				if (ctx->splice_props)
					gf_filter_pid_push_properties(iopid->opid, ctx->splice_props, GF_TRUE, GF_TRUE);

				if (ctx->dyn_period_id)
					filelist_push_period_id(ctx, iopid->opid);
			}
			if (ctx->splice_props) {
				gf_free(ctx->splice_props);
				ctx->splice_props = NULL;
			}
			ctx->wait_splice_start = GF_FALSE;
			ctx->splice_ctrl->splice_ipid = ctx->splice_ctrl->ipid;
			ctx->splice_ctrl->splice_ra_info = ctx->splice_ctrl->ra_info;
			return GF_OK;
		}
		//make sure we have a packet ready on each input pid before dispatching packets from the splice
		//so that we trigger synchronized reconfig on all pids
		ctx->wait_source = GF_TRUE;
	}



	if ((nb_inactive!=count) && (nb_done+nb_inactive==count)) {
		//compute max cts and dts
		GF_Fraction64 max_cts, max_dts;
		max_cts.num = max_dts.num = 0;
		max_cts.den = max_dts.den = 1;

		if (gf_filter_end_of_session(filter) || (nb_stop + nb_inactive == count) ) {
			for (i=0; i<count; i++) {
				iopid = gf_list_get(ctx->io_pids, i);
				gf_filter_pid_set_eos(iopid->opid);
			}
			ctx->is_eos = GF_TRUE;
			return GF_EOS;
		}
		ctx->dts_sub_plus_one.num = 0;
		for (i=0; i<count; i++) {
			u64 ts;
			iopid = gf_list_get(ctx->io_pids, i);
			iopid->send_cue = ctx->sigcues;
			if (!iopid->ipid) continue;

			ts = iopid->max_cts - iopid->dts_sub;
			if (gf_timestamp_less(max_cts.num, max_cts.den, ts, iopid->timescale)) {
				max_cts.num = ts;
				max_cts.den = iopid->timescale;
			}

			ts = iopid->max_dts - iopid->dts_sub;
			if (gf_timestamp_less(max_dts.num, max_dts.den, ts, iopid->timescale)) {
				max_dts.num = ts;
				max_dts.den = iopid->timescale;
			}
		}
		if (!ctx->cts_offset.num || !ctx->cts_offset.den) {
			ctx->cts_offset = max_dts;
		} else if (ctx->cts_offset.den == max_dts.den) {
			ctx->cts_offset.num += max_dts.num;
		} else if (max_dts.den>ctx->cts_offset.den) {
			ctx->cts_offset.num = gf_timestamp_rescale_signed(ctx->cts_offset.num, ctx->cts_offset.den, max_dts.den);
			ctx->cts_offset.num += max_dts.num;
			ctx->cts_offset.den = max_dts.den;
		} else {
			ctx->cts_offset.num += gf_timestamp_rescale_signed(max_dts.num, max_dts.den, ctx->cts_offset.den);
		}

		if (!ctx->dts_offset.num || !ctx->dts_offset.den) {
			ctx->dts_offset = max_dts;
		} else if (ctx->dts_offset.den == max_dts.den) {
			ctx->dts_offset.num += max_dts.num;
		} else if (max_dts.den > ctx->dts_offset.den) {
			ctx->dts_offset.num = gf_timestamp_rescale_signed(ctx->dts_offset.num, ctx->dts_offset.den, max_dts.den);
			ctx->dts_offset.num += max_dts.num;
			ctx->dts_offset.den = max_dts.den;
		} else {
			ctx->dts_offset.num += gf_timestamp_rescale_signed(max_dts.num, max_dts.den, ctx->dts_offset.den);
		}

		if (ctx->nb_repeat) {
			Bool is_splice_resume = GF_FALSE;
			if ((s32) ctx->nb_repeat>0)
				ctx->nb_repeat--;

			//reload of main content
			if (ctx->splice_state==FL_SPLICE_AFTER) {
				ctx->splice_start = ctx->init_splice_start;
				ctx->splice_end = ctx->init_splice_end;
				ctx->flags_splice_start = ctx->init_flags_splice_start;
				ctx->flags_splice_end = ctx->init_flags_splice_end;
				ctx->splice_state = FL_SPLICE_BEFORE;
				ctx->splice_end_cts = 0;
				ctx->splice_start_cts = 0;
				ctx->spliced_current_cts = 0;
				ctx->dts_sub_plus_one_at_splice.num = 0;
				ctx->last_url_crc = ctx->last_splice_crc;
				ctx->last_url_lineno = ctx->last_splice_lineno;
				ctx->start = ctx->init_start;
				ctx->stop = ctx->init_stop;
				is_splice_resume = GF_TRUE;
			}

			for (i=0; i<count; i++) {
				GF_FilterEvent evt;
				iopid = gf_list_get(ctx->io_pids, i);
				if (!iopid->ipid) continue;

				gf_filter_pid_set_discard(iopid->ipid, GF_FALSE);

				GF_FEVT_INIT(evt, GF_FEVT_STOP, iopid->ipid);
				gf_filter_pid_send_event(iopid->ipid, &evt);

				iopid->is_eos = GF_FALSE;
				filelist_start_ipid(ctx, iopid, iopid->timescale, GF_TRUE);
				if (is_splice_resume) {
					iopid->dts_o_splice = iopid->cts_o;
					iopid->cts_o_splice = iopid->dts_o;
					iopid->splice_ra_info = iopid->ra_info;
					iopid->dts_sub_splice = 0;
				}
			}
		} else {
			if (ctx->splice_state!=FL_SPLICE_ACTIVE) {
				ctx->splice_state = FL_SPLICE_NONE;
				ctx->last_splice_crc = 0;
			}
			//force load
			ctx->load_next = GF_TRUE;
			return filelist_process(filter);
		}
	}
	return GF_OK;
}

static void filelist_add_entry(GF_FileListCtx *ctx, FileListEntry *fentry)
{
	u32 i, count;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUTHOR, ("[FileList] Adding file %s to list\n", fentry->file_name));
	if (ctx->fsort==FL_SORT_NONE) {
		gf_list_add(ctx->file_list, fentry);
		return;
	}
	count = gf_list_count(ctx->file_list);
	for (i=0; i<count; i++) {
		Bool insert=GF_FALSE;
		FileListEntry *cur = gf_list_get(ctx->file_list, i);
		switch (ctx->fsort) {
		case FL_SORT_SIZE:
			if (cur->file_size>fentry->file_size) insert = GF_TRUE;
			break;
		case FL_SORT_DATE:
		case FL_SORT_DATEX:
			if (cur->last_mod_time>fentry->last_mod_time) insert = GF_TRUE;
			break;
		case FL_SORT_NAME:
			if (strcmp(cur->file_name, fentry->file_name) > 0) insert = GF_TRUE;
			break;
		}
		if (insert) {
			gf_list_insert(ctx->file_list, fentry, i);
			return;
		}
	}
	gf_list_add(ctx->file_list, fentry);
}

static Bool filelist_enum(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	FileListEntry *fentry;
	GF_FileListCtx *ctx = cbck;
	if (file_info->hidden) return GF_FALSE;
	if (file_info->directory) return GF_FALSE;
	if (file_info->drive) return GF_FALSE;
	if (file_info->system) return GF_FALSE;

	GF_SAFEALLOC(fentry, FileListEntry);
	if (!fentry) return GF_TRUE;

	fentry->file_name = gf_strdup(item_path);
	fentry->file_size = file_info->size;
	fentry->last_mod_time = file_info->last_modified;
	filelist_add_entry(ctx, fentry);

	return GF_FALSE;
}

static GF_Err filelist_initialize(GF_Filter *filter)
{
	u32 i, count;
	char *sep_dir, c=0, *dir, *pattern;
	GF_FileListCtx *ctx = gf_filter_get_udta(filter);
	ctx->io_pids = gf_list_new();

	ctx->filter_srcs = gf_list_new();
	if (ctx->ka)
		ctx->floop = 0;


	if (! ctx->srcs.nb_items ) {
		if (! gf_filter_is_dynamic(filter)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[FileList] No inputs\n"));
		}
		return GF_OK;
	}

	ctx->file_list = gf_list_new();
	count = ctx->srcs.nb_items;
	for (i=0; i<count; i++) {
		char *list = ctx->srcs.vals[i];

		if (strchr(list, '*') ) {
			sep_dir = strrchr(list, '/');
			if (!sep_dir) sep_dir = strrchr(list, '\\');
			if (sep_dir) {
				c = sep_dir[0];
				sep_dir[0] = 0;
				dir = list;
				pattern = sep_dir+2;
			} else {
				dir = ".";
				pattern = list;
			}
			gf_enum_directory(dir, GF_FALSE, filelist_enum, ctx, pattern);
			if (c && sep_dir) sep_dir[0] = c;
		} else {
			u32 type = 0;
			if (strstr(list, " && ") || strstr(list, "&&"))
				type = 1;
			else if (gf_file_exists(list))
				type = 2;

			if (type) {
				FileListEntry *fentry;
				GF_SAFEALLOC(fentry, FileListEntry);
				if (fentry) {
					fentry->file_name = gf_strdup(list);
					if (type==2) {
						FILE *fo;
						fentry->last_mod_time = gf_file_modification_time(list);
						fo = gf_fopen(list, "rb");
						if (fo) {
							fentry->file_size = gf_fsize(fo);
							gf_fclose(fo);
						}
					}
					filelist_add_entry(ctx, fentry);
				}
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[FileList] File %s not found, ignoring\n", list));
			}
		}
	}

	if (!gf_list_count(ctx->file_list)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] No files found in list %s\n", ctx->srcs));
		return GF_BAD_PARAM;
	}
	if (ctx->fsort==FL_SORT_DATEX) {
		ctx->revert = GF_FALSE;
		ctx->floop = 0;
	}
	ctx->file_list_idx = ctx->revert ? gf_list_count(ctx->file_list) : -1;
	ctx->load_next = GF_TRUE;
	//from now on we only accept the above caps
	filelist_override_caps(filter, ctx);
	//and we act as a source, request processing
	gf_filter_post_process_task(filter);
	//prevent deconnection of filter when no input
	gf_filter_make_sticky(filter);
	return GF_OK;
}

static void filelist_finalize(GF_Filter *filter)
{
	GF_FileListCtx *ctx = gf_filter_get_udta(filter);
	while (gf_list_count(ctx->io_pids)) {
		FileListPid *iopid = gf_list_pop_back(ctx->io_pids);
		gf_free(iopid);
	}
	if (ctx->file_list) {
		while (gf_list_count(ctx->file_list)) {
			FileListEntry *fentry = gf_list_pop_back(ctx->file_list);
			gf_free(fentry->file_name);
			gf_free(fentry);
		}
		gf_list_del(ctx->file_list);
	}
	gf_list_del(ctx->io_pids);
	gf_list_del(ctx->filter_srcs);
	if (ctx->file_path) gf_free(ctx->file_path);
	if (ctx->frag_url) gf_free(ctx->frag_url);
	if (ctx->unknown_params) gf_free(ctx->unknown_params);
	if (ctx->pid_props) gf_free(ctx->pid_props);
	if (ctx->dyn_period_id) gf_free(ctx->dyn_period_id);
	if (ctx->splice_props) gf_free(ctx->splice_props);
	if (ctx->splice_pid_props) gf_free(ctx->splice_pid_props);
}

static const char *filelist_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	u32 nb_lines = 0;
	if (!gf_utf8_is_legal(data, size)) {
		return NULL;
	}
	while (data && size) {
		u32 i, line_size;
		Bool is_cr = GF_FALSE;
		char *nl;
		nl = memchr(data, '\r', size);
		if (!nl)
			nl = memchr(data, '\n', size);
		else
			is_cr = GF_TRUE;

		if (nl)
			line_size = (u32) (nl - (char *) data);
		else
			line_size = size-1;

		//line is comment
		if (data[0] != '#') {
			Bool line_empty = GF_TRUE;
			for (i=0;i<line_size; i++) {
				char c = (char) data[i];
				if (!c) return NULL;
				if ( isalnum(c)) continue;
				//valid URL chars plus backslash for win path
				if (strchr("-._~:/?#[]@!$&'()*+,;%=\\", c)) {
					line_empty = GF_FALSE;
					continue;
				}
				//not a valid URL
				return NULL;
			}
			if (!line_empty)
				nb_lines++;
		}
		if (!nl) break;
		size -= (u32) (nl+1 - (char *) data);
		data = nl+1;
		if (is_cr && (data[0]=='\n')) {
			size --;
			data++;
		}
	}
	if (!nb_lines) return NULL;
	*score = GF_FPROBE_MAYBE_SUPPORTED;
	return "application/x-gpac-playlist";
}

#define OFFS(_n)	#_n, offsetof(GF_FileListCtx, _n)
static const GF_FilterArgs GF_FileListArgs[] =
{
	{ OFFS(floop), "loop playlist/list of files, `0` for one time, `n` for n+1 times, `-1` for indefinitely", GF_PROP_SINT, "0", NULL, 0},
	{ OFFS(srcs), "list of files to play - see filter help", GF_PROP_STRING_LIST, NULL, NULL, 0},
	{ OFFS(fdur), "for source files with a single frame, sets frame duration. 0/NaN fraction means reuse source timing which is usually not set!", GF_PROP_FRACTION, "1/25", NULL, 0},
	{ OFFS(revert), "revert list of files (not playlist)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(timescale), "force output timescale on all pids. 0 uses the timescale of the first pid found", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ka), "keep playlist alive (disable loop), waiting the for a new input to be added or `#end` to end playlist. The value specifies the refresh rate in ms", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(timeout), "timeout in ms after which the playlist is considered dead. `-1` means indefinitely", GF_PROP_LUINT, "-1", NULL, GF_FS_ARG_HINT_ADVANCED},

	{ OFFS(fsort), "sort list of files\n"
		"- no: no sorting, use default directory enumeration of OS\n"
		"- name: sort by alphabetical name\n"
		"- size: sort by increasing size\n"
		"- date: sort by increasing modification time\n"
		"- datex: sort by increasing modification time - see filter help"
		, GF_PROP_UINT, "no", "no|name|size|date|datex", 0},

	{ OFFS(sigcues), "inject CueStart property at each source begin (new or repeated) for DASHing", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(fdel), "delete source files after processing in playlist mode (does not delete the playlist)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(raw), "force input AV streams to be in raw format\n"
	"- no: do not force decoding of inputs\n"
	"- av: force decoding of audio and video inputs\n"
	"- a: force decoding of audio inputs\n"
	"- v: force decoding of video inputs", GF_PROP_UINT, "no", "av|a|v|no", GF_FS_ARG_HINT_NORMAL},

	{0}
};


static const GF_FilterCapability FileListCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "txt|m3u|pl"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/x-gpac-playlist"),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};


GF_FilterRegister FileListRegister = {
	.name = "flist",
	GF_FS_SET_DESCRIPTION("Sources concatenator")
	GF_FS_SET_HELP("This filter can be used to play playlist files or a list of sources.\n"
		"\n"
		"The filter loads any source supported by GPAC: remote or local files or streaming sessions (TS, RTP, DASH or other).\n"
		"The filter forces input demultiplex and recomputes the input timestamps into a continuous timeline.\n"
		"At each new source, the filter tries to remap input PIDs to already declared output PIDs of the same type, if any, or declares new output PIDs otherwise. If no input PID matches the type of an output, no packets are send for that PID.\n"
		"\n"
		"# Source list mode\n"
		"The source list mode is activated by using `flist:srcs=f1[,f2]`, where f1 can be a file or a directory to enumerate.\n"
		"The syntax for directory enumeration is:\n"
		"- dir/*: enumerates everything in dir\n"
		"- foo/*.png: enumerates all files with extension png in foo\n"
		"- foo/*.png;*.jpg: enumerates all files with extension png or jpg in foo\n"
		"\n"
		"The resulting file list can be sorted using [-fsort]().\n"
		"If the sort mode is `datex` and source files are images or single frame files, the following applies:\n"
		"- options [-floop](), [-revert]() and [-dur]() are ignored\n"
		"- the files are sorted by modification time\n"
		"- the first frame is assigned a timestamp of 0\n"
		"- each frame (coming from each file) is assigned a duration equal to the difference of modification time between the file and the next file\n"
		"- the last frame is assigned the same duration as the previous one\n"
		"# Playlist mode\n"
		"The playlist mode is activated when opening a playlist file (m3u format, utf-8 encoding, default extensions `m3u`, `txt` or `pl`).\n"
		"In this mode, directives can be given in a comment line, i.e. a line starting with '#' before the line with the file name.\n"
		"Lines stating with `##` are ignored.\n"
		"\n"
		"The playlist file is refreshed whenever the next source has to be reloaded in order to allow for dynamic pushing of sources in the playlist.\n"\
		"If the last URL played cannot be found in the playlist, the first URL in the playlist file will be loaded.\n"
		"\n"
		"When [-ka]() is used to keep refreshing the playlist on regular basis, the playlist must end with a new line.\n"
		"Playlist refreshing will abort:\n"
		"- if the input playlist has a line not ending with a LF `(\\n)` character, in order to avoid asynchronous issues when reading the playlist.\n"
		"- if the input playlist has not been modified for the [-timeout]() option value (infinite by default).\n"
		"## Playlist directives\n"
		"A playlist directive line can contain zero or more directives, separated with space. The following directives are supported:\n"
		"- repeat=N: repeats N times the content (hence played N+1).\n"
		"- start=T: tries to play the file from start time T seconds (double format only). This may not work with some files/formats not supporting seeking.\n"
		"- stop=T: stops source playback after T seconds (double format only). This works on any source (implemented independently from seek support).\n"
		"- cat: specifies that the following entry should be concatenated to the previous source rather than opening a new source. This can optionally specify a byte range if desired, otherwise the full file is concatenated.\n"
		"Note: When sources are ISOBMFF files or segments on local storage or GF_FileIO objects, the concatenation will be automatically detected.\n"
		"- srange=T: when cat is set, indicates the start T (64 bit decimal, default 0) of the byte range from the next entry to concatenate.\n"
		"- send=T: when cat is set, indicates the end T (64 bit decimal, default 0) of the byte range from the next entry to concatenate.\n"
		"- props=STR: assigns properties described in `STR` to all pids coming from the listed sources on next line. `STR` is formatted according to `gpac -h doc` using the default parameter set.\n"
		"- del: specifies that the source file(s) must be deleted once processed, true by default if [-fdel]() is set.\n"
		"- out=V: specifies splicing start time (cf below).\n"
		"- in=V: specifies splicing end time (cf below).\n"
		"- nosync: prevents timestamp adjustments when joining sources (implied if `cat` is set).\n"
		"- keep: keeps spliced period in output (cf below).\n"
		"- mark: only inject marker for the splice period and do not load any replacement content (cf below).\n"
		"- sprops=STR: assigns properties described in `STR` to all pids of the main content during a splice (cf below). `STR` is formatted according to `gpac -h doc` using the default parameter set.\n"
		"\n"
		"The following global options (applying to the filter, not the sources) may also be set in the playlist:\n"
		"- ka=N: force [-ka]() option to `N` millisecond refresh.\n"
		"- floop=N: set [-floop]() option from within playlist.\n"
		"- raw: set [-raw]() option from within playlist.\n"
		"\n"
		"The default behavior when joining sources is to realign the timeline origin of the new source to the maximum time in all pids of the previous sources.\n"
		"This may create gaps in the timeline in case each pid are not of equal duration (quite common with most audio codecs).\n"
		"Using `nosync` directive will disable this realignment and provide a continuous timeline but may introduce synchronization errors depending in the source encoding (use with caution).\n"
		"## Source syntax\n"
		"The source lines follow the usual source syntax, see `gpac -h`.\n"
		"Additional pid properties can be added per source (see `gpac -h doc`), but are valid only for the current source, and reset at next source.\n"
		"The loaded sources do not inherit arguments from the parent playlist filter.\n"
		"\n"
		"The URL given can either be a single URL, or a list of URLs separated by \" && \" to load several sources for the active entry.\n"
		"Warning: There shall not be any other space/tab characters between sources.\n"
		"EX audio.mp4 && video.mp4\n"
		"## Source with filter chains\n"
		"Each URL can be followed by a chain of one or more filters, using the `@` link directive as used in gpac (see `gpac -h doc`).\n"
		"A negative link index (e.g. `@-1`) can be used to setup a new filter chain starting from the last specified source in the line.\n"
		"Warning: There shall be a single character, with value space (' '), before and after each link directive.\n"
		"\n"
		"EX src.mp4 @ reframer:rt=on\n"
		"This will inject a reframer with real-time regulation between source and `flist` filter.\n"
		"EX src.mp4 @ reframer:saps=1 @1 reframer:saps=0,2,3\n"
		"EX src.mp4 @ reframer:saps=1 @-1 reframer:saps=0,2,3\n"
		"This will inject a reframer filtering only SAP1 frames and a reframer filtering only non-SAP1 frames between source and `flist` filter\n"
		"\n"
		"Link options can be specified (see `gpac -h doc`).\n"
		"EX src.mp4 @#video reframer:rt=on\n"
		"This will inject a reframer with real-time regulation between video pid of source and `flist` filter.\n"
		"\n"
		"When using filter chains, the `flist` filter will only accept PIDs from the last declared filter in the chain.\n"
		"In order to accept other PIDs from the source, you must specify a final link directive with no following filter.\n"
		"EX src.mp4 @#video reframer:rt=on @-1#audio\n"
		"This will inject a reframer with real-time regulation between video pid of source and `flist` filter, and will also allow audio pids from source to connect to `flist` filter.\n"
		"\n"
		"The empty link directive can also be used on the last declared filter\n"
		"EX src.mp4 @ reframer:rt=on @#audio\n"
		"This will inject a reframer with real-time regulation between source and `flist` filter and only connect audio pids to `flist` filter.\n"
		"## Splicing\n"
		"The playlist can be used to splice content with other content following a media in the playlist.\n"
		"A source item is declared as main media in a splice operation if and only if it has an `out` directive set (possibly empty).\n"
		"Directive can be used for the main media except concatenation directives.\n"
		"\n"
		"The splicing operations do not alter media frames and do not perform uncompressed domain operations such as cross-fade or mixing.\n"
		"\n"
		"The `out` (resp. `in`) directive specifies the media splice start (resp. end) time. The value can be formatted as follows:\n"
		"- empty: the time is not yet assigned\n"
		"- `now`: the time is resolved to the next SAP point in the media\n"
		"- integer, float or fraction: set time in seconds\n"
		"- `+VAL`: used for `in` only, specify the end point as delta in seconds from the start point (`VAL` can be integer, float or fraction)\n"
		"- DATE: set splice time according to wall clock `DATE`, formatted as an `XSD dateTime`\n"
		"The splice times (except wall clock) are expressed in the source (main media) timing, not the reconstructed output timeline.\n"
		"\n"
		"When a splice begins (`out` time reached), the source items following the main media are played until the end of the splice or the end of the main media.\n"
		"Sources used during the splice period can use directives such as `start`, `dur` or `repeat`.\n"
		"\n"
		"Once a splice is done (`in` time reached), the main media `out` splice time is reset to undefined.\n"
		"\n"
		"When the main media has undefined `out` or `in` splice times, the playlist is reloaded at each new main media packet to check for resolved values.\n"
		"- `out` can only be modified when no splice is active, otherwise it is ignored. If modified, it resets the next source to play to be the one following the modified main media.\n"
		"- `in` can only be modified when a splice is active with an undefined end time, otherwise it is ignored.\n"
		"\n"
		"When the main media is over:\n"
		"- if `repeat` directive is set, the main media is repeated, `in` and `out` set to their initial values and the next splicing content is the one following the main content,\n"
		"- otherwise, the next source queued is the one following the last source played during the last splice period.\n"
		"\n"
		"It is allowed to defined several main media in the playlist, but a main media is not allowed as media for a splice period.\n"
		"\n"
		"The filter will look for the property `Period` on the output PIDs of the main media for multi-period DASH.\n"
		"If found, `_N` is appended to the period ID, with `N` starting from 1 and increased at each main media resume.\n"
		"If no `Period` property is set on main or spliced media, period switch can still be forced using [-pswitch](dasher) DASH option.\n"
		"\n"
		"If `mark` directive is set for a main media, no content replacement is done and the splice boundaries will be signaled in the main media.\n"
		"If `keep` directive is set for a main media, the main media is forwarded along with the replacement content.\n"
		"When `mark` or `keep` directives are set, it is possible to alter the PID properties of the main media using `sprops` directive.\n"
		"\n"
		"EX #out=2 in=4 mark sprops=#xlink=http://foo.bar/\nEX src:#Period=main\n"
		"This will inject property xlink on the output pids in the splice zone (corresponding to period `main_2`) but not in the rest of the main media.\n"
		"\n"
		"Directives `mark`, `keep` and `sprops` are reset at the end of the splice period.\n"
		)
	.private_size = sizeof(GF_FileListCtx),
	.max_extra_pids = -1,
	.flags = GF_FS_REG_ACT_AS_SOURCE | GF_FS_REG_REQUIRES_RESOLVER | GF_FS_REG_DYNAMIC_PIDS,
	.args = GF_FileListArgs,
	.initialize = filelist_initialize,
	.finalize = filelist_finalize,
	SETCAPS(FileListCaps),
	.configure_pid = filelist_configure_pid,
	.process = filelist_process,
	.process_event = filelist_process_event,
	.probe_data = filelist_probe_data
};

const GF_FilterRegister *filelist_register(GF_FilterSession *session)
{
	return &FileListRegister;
}

