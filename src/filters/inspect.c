/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / inspection filter
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
#include <gpac/list.h>

typedef struct
{
	GF_FilterPid *src_pid;
	FILE *tmp;
	u64 pck_num;
	u32 idx;
	u8 dump_pid; //0: no, 1: configure/reconfig, 2: info update
	u8 init_pid_config_done;
	u64 pck_for_config;
	u64 prev_dts, prev_cts, init_ts;
} PidCtx;

enum
{
	INSPECT_MODE_PCK=0,
	INSPECT_MODE_BLOCK,
	INSPECT_MODE_REFRAME,
	INSPECT_MODE_RAW
};

enum
{
	INSPECT_TEST_NO=0,
	INSPECT_TEST_NETWORK,
	INSPECT_TEST_ENCODE,
};

typedef struct
{
	u32 mode;
	Bool interleave;
	Bool dump_data;
	Bool deep;
	char *log;
	char *fmt;
	Bool props, hdr, all, info, pcr;
	Double speed, start;
	u32 test;
	GF_Fraction dur;

	FILE *dump;

	GF_List *src_pids;

	Bool is_prober, probe_done, hdr_done, dump_pck;
} GF_InspectCtx;

static void inspect_finalize(GF_Filter *filter)
{
	char szLine[1025];

	Bool concat=GF_FALSE;
	GF_InspectCtx *ctx = (GF_InspectCtx *) gf_filter_get_udta(filter);

	if (ctx->dump) {
		if ((ctx->dump!=stderr) && (ctx->dump!=stdout)) concat=GF_TRUE;
		else if (!ctx->interleave) concat=GF_TRUE;
	}
	while (gf_list_count(ctx->src_pids)) {
		PidCtx *pctx = gf_list_pop_front(ctx->src_pids);

		if (!ctx->interleave && pctx->tmp) {
			if (concat) {
				gf_fseek(pctx->tmp, 0, SEEK_SET);
				while (!feof(pctx->tmp)) {
					u32 read = (u32) fread(szLine, 1, 1024, pctx->tmp);
					fwrite(szLine, 1, read, ctx->dump);
				}
			}
			gf_fclose(pctx->tmp);
		}
		gf_free(pctx);
	}
	gf_list_del(ctx->src_pids);

	if (ctx->dump && (ctx->dump!=stderr) && (ctx->dump!=stdout)) gf_fclose(ctx->dump);
}

static void inspect_dump_property(GF_InspectCtx *ctx, FILE *dump, u32 p4cc, const char *pname, const GF_PropertyValue *att)
{
	char szDump[GF_PROP_DUMP_ARG_SIZE];
	if (!pname) pname = gf_props_4cc_get_name(p4cc);

	if (gf_sys_is_test_mode() || ctx->test) {
		switch (p4cc) {
		case GF_PROP_PID_FILEPATH:
		case GF_PROP_PID_URL:
			return;
		case GF_PROP_PID_FILE_CACHED:
		case GF_PROP_PID_DURATION:
			if (ctx->test==INSPECT_TEST_NETWORK)
				return;
			break;
		case GF_PROP_PID_DECODER_CONFIG:
		case GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT:
		case GF_PROP_PID_DOWN_SIZE:
			if (ctx->test==INSPECT_TEST_ENCODE)
				return;
			break;
		default:
			if (gf_sys_is_test_mode() && (att->type==GF_PROP_POINTER) )
				return;
			break;
		}
	}

	fprintf(dump, "\t%s: ", pname ? pname : gf_4cc_to_str(p4cc));
	fprintf(dump, "%s", gf_prop_dump(p4cc, att, szDump, ctx->dump_data) );

	fprintf(dump, "\n");
}

static void inspect_dump_packet_fmt(GF_InspectCtx *ctx, FILE *dump, GF_FilterPacket *pck, PidCtx *pctx, u64 pck_num)
{
	char szDump[GF_PROP_DUMP_ARG_SIZE];
	u32 size=0;
	const char *data=NULL;
	char *str = ctx->fmt;
	assert(str);

	if (pck)
		data = gf_filter_pck_get_data(pck, &size);

	while (str) {
		char csep;
		char *sep, *key;
		if (!str[0]) break;

		if ((str[0] != '$') && (str[0] != '%') && (str[0] != '@')) {
			fprintf(dump, "%c", str[0]);
			str = str+1;
			continue;
		}
		csep = str[0];
		if (str[1] == csep) {
			fprintf(dump, "%c", str[0]);
			str = str+2;
			continue;
		}
		sep = strchr(str+1, csep);
		if (!sep) {
			fprintf(dump, "%c", str[0]);
			str = str+1;
			continue;
		}
		sep[0] = 0;
		key = str+1;

		if (!pck) {
			if (!strcmp(key, "lf")) fprintf(dump, "\n" );
			else if (!strcmp(key, "cr")) fprintf(dump, "\r" );
			else if (!strncmp(key, "pid.", 4)) fprintf(dump, "%s", key+4);
			else fprintf(dump, "%s", key);
			sep[0] = csep;
			str = sep+1;
			continue;
		}

		if (!strcmp(key, "pn")) fprintf(dump, LLU, pck_num);
		else if (!strcmp(key, "dts")) {
			u64 ts = gf_filter_pck_get_dts(pck);
			if (ts==GF_FILTER_NO_TS) fprintf(dump, "N/A");
			else fprintf(dump, LLU, ts );
		}
		else if (!strcmp(key, "cts")) {
			u64 ts = gf_filter_pck_get_cts(pck);
			if (ts==GF_FILTER_NO_TS) fprintf(dump, "N/A");
			else fprintf(dump, LLU, ts );
		}
		else if (!strcmp(key, "ddts")) {
			u64 ts = gf_filter_pck_get_dts(pck);
			if ((ts==GF_FILTER_NO_TS) || (pctx->prev_dts==GF_FILTER_NO_TS)) fprintf(dump, "N/A");
			else {
				s64 diff = ts;
				diff -= pctx->prev_dts;
				fprintf(dump, LLD, diff);
			}
			pctx->prev_dts = ts;
		}
		else if (!strcmp(key, "dcts")) {
			u64 ts = gf_filter_pck_get_cts(pck);
			if ((ts==GF_FILTER_NO_TS) || (pctx->prev_cts==GF_FILTER_NO_TS)) fprintf(dump, "N/A");
			else {
				s64 diff = ts;
				diff -= pctx->prev_cts;
				fprintf(dump, LLD, diff);
			}
			pctx->prev_cts = ts;
		}
		else if (!strcmp(key, "ctso")) {
			u64 dts = gf_filter_pck_get_dts(pck);
			u64 cts = gf_filter_pck_get_cts(pck);
			if (dts==GF_FILTER_NO_TS) dts = cts;
			if (cts==GF_FILTER_NO_TS) fprintf(dump, "N/A");
			else fprintf(dump, LLD, ((s64)cts) - ((s64)dts) );
		}

		else if (!strcmp(key, "dur")) fprintf(dump, "%u", gf_filter_pck_get_duration(pck) );
		else if (!strcmp(key, "frame")) {
			Bool start, end;
			gf_filter_pck_get_framing(pck, &start, &end);
			if (start && end) fprintf(dump, "frame_full");
			else if (start) fprintf(dump, "frame_start");
			else if (end) fprintf(dump, "frame_end");
			else fprintf(dump, "frame_cont");
		}
		else if (!strcmp(key, "sap") || !strcmp(key, "rap")) fprintf(dump, "%u", gf_filter_pck_get_sap(pck) );
		else if (!strcmp(key, "ilace")) fprintf(dump, "%d", gf_filter_pck_get_interlaced(pck) );
		else if (!strcmp(key, "corr")) fprintf(dump, "%d", gf_filter_pck_get_corrupted(pck) );
		else if (!strcmp(key, "seek")) fprintf(dump, "%d", gf_filter_pck_get_seek_flag(pck) );
		else if (!strcmp(key, "bo")) {
			u64 bo = gf_filter_pck_get_byte_offset(pck);
			if (bo==GF_FILTER_NO_BO) fprintf(dump, "N/A");
			else fprintf(dump, LLU, bo);
		}
		else if (!strcmp(key, "roll")) fprintf(dump, "%d", gf_filter_pck_get_roll_info(pck) );
		else if (!strcmp(key, "crypt")) fprintf(dump, "%d", gf_filter_pck_get_crypt_flags(pck) );
		else if (!strcmp(key, "vers")) fprintf(dump, "%d", gf_filter_pck_get_carousel_version(pck) );
		else if (!strcmp(key, "size")) fprintf(dump, "%d", size );
		else if (!strcmp(key, "crc")) fprintf(dump, "0x%08X", gf_crc_32(data, size) );
		else if (!strcmp(key, "lf")) fprintf(dump, "\n" );
		else if (!strcmp(key, "cr")) fprintf(dump, "\r" );
		else if (!strcmp(key, "data")) {
			u32 i;
			for (i=0; i<size; i++) {
				fprintf(dump, "%02X", (unsigned char) data[i]);
			}
		}
		else if (!strcmp(key, "lp")) {
			u8 flags = gf_filter_pck_get_dependency_flags(pck);
			flags >>= 6;
			fprintf(dump, "%u", flags);
		}
		else if (!strcmp(key, "depo")) {
			u8 flags = gf_filter_pck_get_dependency_flags(pck);
			flags >>= 4;
			flags &= 0x3;
			fprintf(dump, "%u", flags);
		}
		else if (!strcmp(key, "depf")) {
			u8 flags = gf_filter_pck_get_dependency_flags(pck);
			flags >>= 2;
			flags &= 0x3;
			fprintf(dump, "%u", flags);
		}
		else if (!strcmp(key, "red")) {
			u8 flags = gf_filter_pck_get_dependency_flags(pck);
			flags &= 0x3;
			fprintf(dump, "%u", flags);
		}
		else if (!strcmp(key, "ck")) fprintf(dump, "%d", gf_filter_pck_get_clock_type(pck) );
		else if (!strncmp(key, "pid.", 4)) {
			const GF_PropertyValue *prop = NULL;
			u32 prop_4cc=0;
			const char *pkey = key+4;
			prop_4cc = gf_props_get_id(pkey);
			if (!prop_4cc && strlen(pkey)==4) prop_4cc = GF_4CC(pkey[0],pkey[1],pkey[2],pkey[3]);

			if (prop_4cc) {
				prop = gf_filter_pid_get_property(pctx->src_pid, prop_4cc);
			}
			if (!prop) prop = gf_filter_pid_get_property_str(pctx->src_pid, key);

			if (prop) {
				fprintf(dump, "%s", gf_prop_dump(prop_4cc, prop, szDump, ctx->dump_data) );
			}
		}
		else {
			const GF_PropertyValue *prop = NULL;
			u32 prop_4cc = gf_props_get_id(key);
			if (!prop_4cc && strlen(key)==4) prop_4cc = GF_4CC(key[0],key[1],key[2],key[3]);

			if (prop_4cc) {
				prop = gf_filter_pck_get_property(pck, prop_4cc);
			}
			if (!prop) prop = gf_filter_pck_get_property_str(pck, key);

			if (prop) {
				fprintf(dump, "%s", gf_prop_dump(prop_4cc, prop, szDump, ctx->dump_data) );
			}
		}

		sep[0] = csep;
		str = sep+1;
	}
}

static void inspect_dump_packet(GF_InspectCtx *ctx, FILE *dump, GF_FilterPacket *pck, u32 pid_idx, u64 pck_num)
{
	u32 idx=0, size;
	u64 ts;
	u8 dflags = 0;
	GF_FilterClockType ck_type;
	Bool start, end;
	const char *data;

	if (!ctx->deep && !ctx->fmt) return;

	data = gf_filter_pck_get_data(pck, &size);
	gf_filter_pck_get_framing(pck, &start, &end);

	ck_type = ctx->pcr ? gf_filter_pck_get_clock_type(pck) : 0;
	if (!size && !ck_type) return;

	fprintf(dump, "PID %d PCK "LLU" - ", pid_idx, pck_num);
	if (ck_type) {
		ts = gf_filter_pck_get_cts(pck);
		if (ts==GF_FILTER_NO_TS) fprintf(dump, " PCR N/A");
		else fprintf(dump, " PCR%s "LLU"\n", (ck_type==GF_FILTER_CLOCK_PCR) ? "" : " discontinuity", ts );
		return;
	}

	if (start && end) fprintf(dump, "full frame");
	else if (start) fprintf(dump, "frame start");
	else if (end) fprintf(dump, "frame end");
	else fprintf(dump, "frame continuation");

	ts = gf_filter_pck_get_dts(pck);
	if (ts==GF_FILTER_NO_TS) fprintf(dump, " dts N/A");
	else fprintf(dump, " dts "LLU"", ts );

	ts = gf_filter_pck_get_cts(pck);
	if (ts==GF_FILTER_NO_TS) fprintf(dump, " cts N/A");
	else fprintf(dump, " cts "LLU"", ts );

	fprintf(dump, " dur %u", gf_filter_pck_get_duration(pck) );
	fprintf(dump, " sap %u", gf_filter_pck_get_sap(pck) );
	fprintf(dump, " ilace %d", gf_filter_pck_get_interlaced(pck) );
	fprintf(dump, " corr %d", gf_filter_pck_get_corrupted(pck) );
	fprintf(dump, " seek %d", gf_filter_pck_get_seek_flag(pck) );

	ts = gf_filter_pck_get_byte_offset(pck);
	if (ts==GF_FILTER_NO_BO) fprintf(dump, " bo N/A");
	else fprintf(dump, " bo "LLU"", ts );

	fprintf(dump, " roll %u", gf_filter_pck_get_roll_info(pck) );
	fprintf(dump, " crypt %u", gf_filter_pck_get_crypt_flags(pck) );
	fprintf(dump, " vers %u", gf_filter_pck_get_carousel_version(pck) );

	if (!ck_type) {
		fprintf(dump, " size %d", size );
	}
	dflags = gf_filter_pck_get_dependency_flags(pck);
	fprintf(dump, " lp %u", (dflags>>6) & 0x3 );
	fprintf(dump, " depo %u", (dflags>>4) & 0x3 );
	fprintf(dump, " depf %u", (dflags>>2) & 0x3 );
	fprintf(dump, " red %u", (dflags) & 0x3 );

	if (ctx->dump_data) {
		u32 i;
		fprintf(dump, " data ");
		for (i=0; i<size; i++) {
			fprintf(dump, "%02X", (unsigned char) data[i]);
		}
		fprintf(dump, "\n");
	} else {
		fprintf(dump, " CRC32 0x%08X\n", gf_crc_32(data, size) );
	}
	if (!ctx->props) return;
	while (1) {
		u32 prop_4cc;
		const char *prop_name;
		const GF_PropertyValue * p = gf_filter_pck_enum_properties(pck, &idx, &prop_4cc, &prop_name);
		if (!p) break;
		if (idx==0) fprintf(dump, "properties:\n");

		inspect_dump_property(ctx, dump, prop_4cc, prop_name, p);
	}
}

static void inspect_dump_pid(GF_InspectCtx *ctx, FILE *dump, GF_FilterPid *pid, u32 pid_idx, Bool is_connect, Bool is_remove, u64 pck_for_config, Bool is_info)
{
	u32 idx=0;

	//disconnect of src pid (not yet supported)
	if (is_info) {
		fprintf(dump, "PID %d name %s info update\n", pid_idx, gf_filter_pid_get_name(pid) );
	} else if (is_remove) {
		fprintf(dump, "PID %d name %s delete\n", pid_idx, gf_filter_pid_get_name(pid) );
	} else {
		fprintf(dump, "PID %d name %s %sonfigure", pid_idx, gf_filter_pid_get_name(pid), is_connect ? "C" : "Rec" );
		if (pck_for_config)
			fprintf(dump, " after "LLU" packets", pck_for_config);
		fprintf(dump, " - properties:\n");
	}
	if (!is_info) {
		while (1) {
			u32 prop_4cc;
			const char *prop_name;
			const GF_PropertyValue * p = gf_filter_pid_enum_properties(pid, &idx, &prop_4cc, &prop_name);
			if (!p) break;
			inspect_dump_property(ctx, dump, prop_4cc, prop_name, p);
		}
	} else {
		while (ctx->info) {
			u32 prop_4cc;
			const char *prop_name;
			const GF_PropertyValue * p = gf_filter_pid_enum_info(pid, &idx, &prop_4cc, &prop_name);
			if (!p) break;
			inspect_dump_property(ctx, dump, prop_4cc, prop_name, p);
		}
	}
}

static GF_Err inspect_process(GF_Filter *filter)
{
	u32 i, count, nb_done=0;
	GF_InspectCtx *ctx = (GF_InspectCtx *) gf_filter_get_udta(filter);

	count = gf_list_count(ctx->src_pids);
	for (i=0; i<count; i++) {
		PidCtx *pctx = gf_list_get(ctx->src_pids, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(pctx->src_pid);

		if (!pck && !gf_filter_pid_is_eos(pctx->src_pid))
			continue;

		if (pctx->dump_pid) {
			inspect_dump_pid(ctx, pctx->tmp, pctx->src_pid, pctx->idx, pctx->init_pid_config_done ? GF_FALSE : GF_TRUE, GF_FALSE, pctx->pck_for_config, (pctx->dump_pid==2) ? GF_TRUE : GF_FALSE);
			pctx->dump_pid = 0;
			pctx->init_pid_config_done = 1;
			pctx->pck_for_config=0;

			if (!ctx->hdr_done) {
				ctx->hdr_done=GF_TRUE;
				if (ctx->hdr && ctx->fmt)
					inspect_dump_packet_fmt(ctx, pctx->tmp, NULL, 0, 0);
			}
		}
		if (!pck) continue;
		
		pctx->pck_for_config++;
		pctx->pck_num++;

		if (ctx->dump_pck) {

			if (ctx->is_prober) {
				nb_done++;
			} else if (ctx->fmt) {
				inspect_dump_packet_fmt(ctx, pctx->tmp, pck, pctx, pctx->pck_num);
			} else {
				inspect_dump_packet(ctx, pctx->tmp, pck, pctx->idx, pctx->pck_num);
			}
		}
		
		if (ctx->dur.num && ctx->dur.den) {
			u64 timescale = gf_filter_pck_get_timescale(pck);
			u64 ts = gf_filter_pck_get_dts(pck);
			if (ts == GF_FILTER_NO_TS) ts = gf_filter_pck_get_cts(pck);

			if (!pctx->init_ts) pctx->init_ts = ts;
			else if (ctx->dur.den * (ts - pctx->init_ts) >= ctx->dur.num * timescale) {
				GF_FilterEvent evt;
				GF_FEVT_INIT(evt, GF_FEVT_STOP, pctx->src_pid);
				gf_filter_pid_send_event(pctx->src_pid, &evt);
			}
		}
		gf_filter_pid_drop_packet(pctx->src_pid);
	}
	if (ctx->is_prober && !ctx->probe_done && (nb_done==count) && !ctx->all) {
		for (i=0; i<count; i++) {
			PidCtx *pctx = gf_list_get(ctx->src_pids, i);
			GF_FilterEvent evt;
			GF_FEVT_INIT(evt, GF_FEVT_STOP, pctx->src_pid);
			gf_filter_pid_send_event(pctx->src_pid, &evt);
		}
		ctx->probe_done = GF_TRUE;
		return GF_EOS;
	}
	return GF_OK;
}

static GF_Err inspect_config_input(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_FilterEvent evt;
	PidCtx *pctx;
	GF_InspectCtx  *ctx = (GF_InspectCtx *) gf_filter_get_udta(filter);

	if (!ctx->src_pids) ctx->src_pids = gf_list_new();

	pctx = gf_filter_pid_get_udta(pid);
	if (pctx) {
		assert(pctx->src_pid == pid);
		if (!ctx->is_prober) pctx->dump_pid = 1;
		return GF_OK;
	}
	GF_SAFEALLOC(pctx, PidCtx);
	pctx->src_pid = pid;
	gf_filter_pid_set_udta(pid, pctx);
	if (! ctx->interleave) {
		const GF_PropertyValue *p;
		pctx->tmp = gf_temp_file_new(NULL);

		//in non interleave mode, log audio first and video after - mostly used for tests where we always need th same output
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
		if (p && (p->value.uint==GF_STREAM_AUDIO)) {
			u32 i;
			gf_list_insert(ctx->src_pids, pctx, 0);

			for (i=0; i<gf_list_count(ctx->src_pids); i++) {
				PidCtx *actx = gf_list_get(ctx->src_pids, i);
				actx->idx = i+1;
			}
		} else {
			gf_list_add(ctx->src_pids, pctx);
		}
	} else {
		pctx->tmp = ctx->dump;
		gf_list_add(ctx->src_pids, pctx);
	}

	pctx->idx = gf_list_find(ctx->src_pids, pctx) + 1;

	switch (ctx->mode) {
	case INSPECT_MODE_PCK:
	case INSPECT_MODE_REFRAME:
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
		break;
	default:
		gf_filter_pid_set_framing_mode(pid, GF_FALSE);
		break;
	}

	if (!ctx->is_prober) {
		pctx->dump_pid = 1;
	}

	gf_filter_pid_init_play_event(pid, &evt, ctx->start, ctx->speed, "Inspect");
	gf_filter_pid_send_event(pid, &evt);

	if (ctx->is_prober || ctx->deep || ctx->fmt) {
		ctx->dump_pck = GF_TRUE;
	} else {
		ctx->dump_pck = GF_FALSE;
	}
	if (ctx->pcr)
		gf_filter_pid_set_clock_mode(pid, GF_TRUE);
	return GF_OK;
}

static const GF_FilterCapability InspecterDemuxedCaps[] =
{
	//accept any stream but files, framed
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_NONE),
	{0},
};

static const GF_FilterCapability InspecterReframeCaps[] =
{
	//accept any stream but files, framed
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
};

GF_Err inspect_initialize(GF_Filter *filter)
{
	const char *name = gf_filter_get_name(filter);
	GF_InspectCtx  *ctx = (GF_InspectCtx *) gf_filter_get_udta(filter);

	if (name && !strcmp(name, "probe") ) {
		ctx->is_prober = GF_TRUE;
		return GF_OK;
	}

	if (!ctx->log) return GF_BAD_PARAM;
	if (!strcmp(ctx->log, "stderr")) ctx->dump = stderr;
	else if (!strcmp(ctx->log, "stdout")) ctx->dump = stdout;
	else {
		ctx->dump = gf_fopen(ctx->log, "wt");
		if (!ctx->dump) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[Inspec] Failed to open file %s\n", ctx->log));
			return GF_IO_ERR;
		}
	}
	switch (ctx->mode) {
	case INSPECT_MODE_RAW:
		break;
	case INSPECT_MODE_REFRAME:
		gf_filter_override_caps(filter, InspecterReframeCaps,  sizeof(InspecterReframeCaps)/sizeof(GF_FilterCapability) );
		break;
	default:
		gf_filter_override_caps(filter, InspecterDemuxedCaps,  sizeof(InspecterDemuxedCaps)/sizeof(GF_FilterCapability) );
		break;
	}
	return GF_OK;
}

static Bool inspect_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_InspectCtx  *ctx = (GF_InspectCtx *) gf_filter_get_udta(filter);
	if (!ctx->info) return GF_TRUE;
	if (evt->base.type!=GF_FEVT_INFO_UPDATE) return GF_TRUE;
	PidCtx *pctx = gf_filter_pid_get_udta(evt->base.on_pid);
	pctx->dump_pid = 2;
	return GF_TRUE;
}


#define OFFS(_n)	#_n, offsetof(GF_InspectCtx, _n)
static const GF_FilterArgs InspectArgs[] =
{
	{ OFFS(log), "sets inspect log filename", GF_PROP_STRING, "stderr", "fileName or stderr or stdout", 0},
	{ OFFS(mode), "dump mode:\n\tpck: dumps full packet\n\tblk: dumps packets before reconstruction\n\tframe: force reframer\n\traw: dumps source packets without demuxing", GF_PROP_UINT, "pck", "pck|blk|frame|raw", 0},
	{ OFFS(interleave), "dumps packets as they are received on each pid. If false, report per pid is generated", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(deep), "dumps packets along with PID state change - implied when fmt is set", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(props), "dumps packet properties - ignored when fmt is set, see filter help", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(dump_data), "enables full data dump, WARNING heavy - ignored when fmt is set, see filter help", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(fmt), "sets packet dump format - see filter help", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(hdr), "prints a header corresponding to fmt string without \'$ \'or \"pid.\"", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(all), "analyses for the entire duration, rather than stoping when all pids are found", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(info), "monitor PID info changes", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(pcr), "dumps M2TS PCR info", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(speed), "sets playback command speed. If speed is negative and start is 0, start is set to -1", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{ OFFS(start), "sets playback start offset, [-1, 0] means percent of media dur, eg -1 == dur", GF_PROP_DOUBLE, "0.0", NULL, 0},
	{ OFFS(dur), "sets inspect duration", GF_PROP_FRACTION, "0/0", NULL, 0},
	{ OFFS(test), "skips some properties:\n"
		"\tno: no properties skipped\n"
		"\tnetwork: URL/path dump, cache state, file size properties skipped (used for hashing network results)\n"
		"\tencode: same as network plus skip decoder config (used for hashing encoding results)", GF_PROP_UINT, "no", "no|network|encode", GF_FS_ARG_HINT_EXPERT},
	{0}
};

static const GF_FilterCapability InspectCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_UNKNOWN),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
};

const GF_FilterRegister InspectRegister = {
	.name = "inspect",
	GF_FS_SET_DESCRIPTION("Inspect packets on pids")
	GF_FS_SET_HELP("The inspector filter can be used to dump pid and packets. Te default options load only pid changes.\n"\
				"The packet inspector mode can be configured to dump specific properties of packets using the fmt option.\n"\
	 			"When the option is not present, all properties are dumped. Otherwise, only properties identified by $TOKEN$ are printed use $, @ or % for TOKEN separator. TOKEN can be:\n"\
				"\tpn: packet (frame in framed mode) number\n"\
				"\tdts: decoding time stamp in stream timescale, N/A if not available\n"\
				"\tddts: difference between current and previous packet's decoding time stamp in stream timescale, N/A if not available\n"\
				"\tcts: composition time stamp in stream timescale, N/A if not available\n"\
				"\tdcts: difference between current and previous packet's composition time stamp in stream timescale, N/A if not available\n"\
				"\tctso: difference between composition time stamp and decoding time stamp in stream timescale, N/A if not available\n"\
				"\tdur: duration in stream timescale\n"\
				"\tframe: framing status: frame_full (complete AU), frame_start, , frame_end, frame_cont\n"
				"\tsap or rap: SAP type of the frame\n"\
				"\tilace: interlacing flag (0: progressive, 1: top field, 2: bottom field)\n"\
				"\tcorr: corrupted packet flag\n"\
				"\tseek: seek flag\n"\
				"\tbo: byte offset in source, N/A if not available\n"\
				"\troll: roll info\n"\
				"\tcrypt: crypt flag\n"\
				"\tvers: carrousel version number\n"\
				"\tsize: size of packet\n"\
				"\tcrc: 32 bit CRC of packet\n"\
				"\tlf: insert linefeed\n"\
				"\tcr: insert carriage return\n"\
				"\tdata: hex dump of packet - WARNING, THIS IS BIG !!\n"\
				"\tlp: leading picture flag\n"\
				"\tdepo: depends on other packet flag\n"\
				"\tdepf: is depended on other packet flag\n"\
				"\tred: redundant coding flag\n"\
				"\tck: clock type (used for PCR discontinuities)\n"\
	 			"\tProperty name or 4cc.\n"\
				"\tpid.P4CC: PID property 4CC\n"\
				"\tpid.PropName: PID property name\n"\
	 			"\n"\
	 			"EX: fmt=\"PID $pid.ID$ packet $pn$ DTS $dts$ CTS $cts$ $lf$\" dumps packet number, cts and dts, eg \"PID 1 packet 10 DTS 100 CTS 108 \\n\" \n"\
	 			"\n"\
	 			"An unrecognized keywork or missing property will resolve to an empty string\n"\
	 			"\n"\
	 			"Note: when dumping in interleaved mode, there is no guarantee that the packets will be dumped in their original sequence order\n"\
	 			"since the inspector fetches one packet at a time on each PID\n")
	.private_size = sizeof(GF_InspectCtx),
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	.max_extra_pids = (u32) -1,
	.args = InspectArgs,
	SETCAPS(InspectCaps),
	.initialize = inspect_initialize,
	.finalize = inspect_finalize,
	.process = inspect_process,
	.process_event = inspect_process_event,
	.configure_pid = inspect_config_input,
};

static const GF_FilterCapability ProberCaps[] =
{
	//accept any stream but files, framed
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_INPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_UINT(GF_CAPS_INPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
};


const GF_FilterRegister ProbeRegister = {
	.name = "probe",
	GF_FS_SET_DESCRIPTION("Inspect packets on demux pids (not file)")
	.private_size = sizeof(GF_InspectCtx),
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	.max_extra_pids = (u32) -1,
	.args = InspectArgs,
	.initialize = inspect_initialize,
	SETCAPS(ProberCaps),
	.finalize = inspect_finalize,
	.process = inspect_process,
	.configure_pid = inspect_config_input,
};

const GF_FilterRegister *inspect_register(GF_FilterSession *session)
{
	return &InspectRegister;
}

const GF_FilterRegister *probe_register(GF_FilterSession *session)
{
	return &ProbeRegister;
}



