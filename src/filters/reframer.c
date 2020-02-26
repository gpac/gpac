/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
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

typedef struct
{
	GF_FilterPid *ipid, *opid;
	u32 timescale;
	u64 cts_at_init;
	u64 sys_clock_at_init;
	u32 nb_frames;
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
} GF_ReframerCtx;

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
			gf_free(st);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (st) {
		gf_filter_pid_reset_properties(st->opid);
	} else {
		GF_SAFEALLOC(st, RTStream);
		gf_list_add(ctx->streams, st);
		st->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_udta(pid, st);
		st->ipid = pid;
	}
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(st->opid, pid);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) st->timescale = p->value.uint;
	else st->timescale = 1000;

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


Bool reframer_send_packet(GF_Filter *filter, GF_ReframerCtx *ctx, RTStream *st, GF_FilterPacket *pck)
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
			cts_us *= 1000000;
			cts_us /= st->timescale;
			if (ctx->rt==REFRAME_RT_SYNC) {
				if (!ctx->clock) ctx->clock = st;

				st = ctx->clock;
			}
			if (!st->sys_clock_at_init) {
				st->cts_at_init = cts_us;
				st->sys_clock_at_init = clock;
				do_send = GF_TRUE;
			} else if (cts_us < st->cts_at_init) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Reframer] CTS less than CTS used to initialize clock, not delaying\n"));
				do_send = GF_TRUE;
			} else {
				u64 diff = cts_us - st->cts_at_init;
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

	if (ctx->frames.nb_items) {
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

	if (do_send) {
		gf_filter_pck_forward(pck, st->opid);
		gf_filter_pid_drop_packet(st->ipid);
		st->nb_frames++;
		return GF_TRUE;
	}
	return GF_FALSE;
}


GF_Err reframer_process(GF_Filter *filter)
{
	GF_ReframerCtx *ctx = gf_filter_get_udta(filter);
	u32 i, nb_eos, count = gf_filter_get_ipid_count(filter);

	if (ctx->rt) {
		ctx->reschedule_in = 0;
		ctx->clock_val = gf_sys_clock_high_res();
	}
	nb_eos = 0;
	for (i=0; i<count; i++) {
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		RTStream *st = ipid ? gf_filter_pid_get_udta(ipid) : NULL;
		assert(ipid);
		assert(st);

		while (1) {
			Bool forward = GF_TRUE;
			GF_FilterPacket *pck = gf_filter_pid_get_packet(ipid);
			if (!pck) {
				if (gf_filter_pid_is_eos(ipid)) {
					gf_filter_pid_set_eos(st->opid);
					nb_eos++;
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
					if (!ctx->filter_sap4) forward = GF_FALSE;
					break;
				default:
					if (!ctx->filter_sap_none) forward = GF_FALSE;
					break;
				}
			}

			if (!forward) {
				gf_filter_pid_drop_packet(ipid);
				st->nb_frames++;
				continue;
			}

			if (! reframer_send_packet(filter, ctx, st, pck))
				break;

		}
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

	if (ctx->raw) {
		gf_filter_override_caps(filter, ReframerRAWCaps, sizeof(ReframerRAWCaps) / sizeof(GF_FilterCapability) );
	}
	return GF_OK;
}


static void reframer_finalize(GF_Filter *filter)
{
	GF_ReframerCtx *ctx = gf_filter_get_udta(filter);

	while (gf_list_count(ctx->streams)) {
		RTStream *st = gf_list_pop_back(ctx->streams);
		gf_free(st);
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
	"- sync: enables real-time regulation one clock for all pids", GF_PROP_UINT, "off", "off|on|sync", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(saps), "drop non-SAP packets, off by default. The list gives the SAP types (0,1,2,3,4) to forward. Note that forwarding only sap 0 will break the decoding", GF_PROP_UINT_LIST, NULL, "0|1|2|3|4", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(refs), "forward only frames used as reference frames, if indicated in the input stream", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(speed), "speed for real-time regulation mode - only positive value", GF_PROP_DOUBLE, "1.0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(raw), "force input streams to be in raw format (i.e. forces decoding of input)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(frames), "drop all except listed frames (first being 1), off by default", GF_PROP_UINT_LIST, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};

GF_FilterRegister ReframerRegister = {
	.name = "reframer",
	GF_FS_SET_DESCRIPTION("Media Reframer")
	GF_FS_SET_HELP("Passthrough filter ensuring reframing, and optionally decoding, of inputs\n"
		"This filter forces input pids to be properly framed (1 packet = 1 Access Unit). It is mostly used for file to file operations.\n"\
		"The filter can be used to filter out packets based on SAP types, for example to extract only the key frame (SAP 1,2,3) of a video.\n"\
		"The filter can be used to only keep specific [-frames]() of the source.\n"\
		"The filter can be used to force input media to be decoded.\n"\
		"The filter can be used to add real-time regulation of input packets. For example to simulate a live DASH:\n"\
		"EX \"src=m.mp4 reframer:rt=on dst=live.mpd:dynamic\"\n"\
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
};


const GF_FilterRegister *reframer_register(GF_FilterSession *session)
{
	return &ReframerRegister;
}
