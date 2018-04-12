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
} PidCtx;

typedef struct
{
	Bool framing;
	Bool interleave;
	Bool dump_data;
	Bool pid_only;
	const char *logfile;

	FILE *dump;

	GF_List *src_pids;

	Bool is_prober, probe_done;
} GF_InspectCtx;

static void inspect_finalize(GF_Filter *filter)
{
	char szLine[1025];

	Bool concat=GF_FALSE;
	GF_InspectCtx *ctx = (GF_InspectCtx *) gf_filter_get_udta(filter);

	if ((ctx->dump!=stderr) && (ctx->dump!=stdout)) concat=GF_TRUE;
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
			fclose(pctx->tmp);
		}
		gf_free(pctx);
	}
	gf_list_del(ctx->src_pids);

	if ((ctx->dump!=stderr) && (ctx->dump!=stdout)) gf_fclose(ctx->dump);
}

static void inspect_dump_property(GF_InspectCtx *ctx, FILE *dump, u32 p4cc, const char *pname, const GF_PropertyValue *att)
{
	char szDump[100];
	if (!pname) pname = gf_props_4cc_get_name(p4cc);

	fprintf(dump, "\t%s: ", pname ? pname : gf_4cc_to_str(p4cc));
	fprintf(dump, "%s", gf_prop_dump_val(att, szDump, ctx->dump_data) );
	fprintf(dump, "\n");
}

static void inspect_dump_packet(GF_InspectCtx *ctx, FILE *dump, GF_FilterPacket *pck, u32 pid_idx, u64 pck_num)
{
	u32 idx=0, size;
	Bool start, end;
	const char *data;

	if (ctx->pid_only) return;

	data = gf_filter_pck_get_data(pck, &size);
	gf_filter_pck_get_framing(pck, &start, &end);

	fprintf(dump, "PID %d PCK "LLU" - ", pid_idx, pck_num);
	if (start && end) fprintf(dump, "full frame");
	else if (start) fprintf(dump, "frame start");
	else if (end) fprintf(dump, "frame end");
	else fprintf(dump, "frame continuation");
	if (ctx->dump_data) {
		u32 i;
		fprintf(dump, " size %d data:\n", size);
		for (i=0; i<size; i++) {
			fprintf(dump, "%02X", (unsigned char) data[i]);
		}
		fprintf(dump, "\nproperties:\n");
	} else {
		fprintf(dump, " size %d CRC32 0x%08X - properties:\n", size, gf_crc_32(data, size) );
	}

	while (1) {
		u32 prop_4cc;
		const char *prop_name;
		const GF_PropertyValue * p = gf_filter_pck_enum_properties(pck, &idx, &prop_4cc, &prop_name);
		if (!p) break;
		inspect_dump_property(ctx, dump, prop_4cc, prop_name, p);
	}
}

static void inspect_dump_pid(GF_InspectCtx *ctx, FILE *dump, GF_FilterPid *pid, u32 pid_idx, Bool is_connect, Bool is_remove)
{
	u32 idx=0;

	//disconnect of src pid (not yet supported)
	if (is_remove) {
		fprintf(dump, "PID %d name %s delete\n", pid_idx, gf_filter_pid_get_name(pid) );
	} else {
		fprintf(dump, "PID %d name %s %sonfigure - properties:\n", pid_idx, gf_filter_pid_get_name(pid), is_connect ? "C" : "Rec" );
	}
	while (1) {
		u32 prop_4cc;
		const char *prop_name;
		const GF_PropertyValue * p = gf_filter_pid_enum_properties(pid, &idx, &prop_4cc, &prop_name);
		if (!p) break;
		inspect_dump_property(ctx, dump, prop_4cc, prop_name, p);
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
		if (!pck) continue;
		pctx->pck_num++;
		if (ctx->is_prober) {
			nb_done++;
		} else {
			inspect_dump_packet(ctx, pctx->tmp, pck, pctx->idx, pctx->pck_num);
		}
		gf_filter_pid_drop_packet(pctx->src_pid);
	}
	if (ctx->is_prober && !ctx->probe_done && (nb_done==count)) {
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
	PidCtx *pctx;
	GF_InspectCtx  *ctx = (GF_InspectCtx *) gf_filter_get_udta(filter);

	if (!ctx->src_pids) ctx->src_pids = gf_list_new();

	pctx = gf_filter_pid_get_udta(pid);
	if (pctx) {
		assert(pctx->src_pid == pid);
		if (!ctx->is_prober)
			inspect_dump_pid(ctx, pctx->tmp, pid, pctx->idx, GF_FALSE, is_remove);
		return GF_OK;
	}
	GF_SAFEALLOC(pctx, PidCtx);
	pctx->src_pid = pid;
	gf_filter_pid_set_udta(pid, pctx);
	if (! ctx->interleave) {
		pctx->tmp = gf_temp_file_new(NULL);
	} else {
		pctx->tmp = ctx->dump;
	}
	gf_list_add(ctx->src_pids, pctx);
	pctx->idx = gf_list_count(ctx->src_pids);

	gf_filter_pid_set_framing_mode(pid, ctx->framing);

	if (ctx->is_prober) {
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
		gf_filter_pid_send_event(pid, &evt);
	} else {
		inspect_dump_pid(ctx, pctx->tmp, pid, pctx->idx, GF_TRUE, GF_FALSE);
	}

	return GF_OK;
}

GF_Err inspect_initialize(GF_Filter *filter)
{
	const char *name = gf_filter_get_name(filter);
	GF_InspectCtx  *ctx = (GF_InspectCtx *) gf_filter_get_udta(filter);

	if (name && !strcmp(name, "probe") ) {
		ctx->is_prober = GF_TRUE;
		return GF_OK;
	}

	if (!ctx->logfile) return GF_BAD_PARAM;
	if (!strcmp(ctx->logfile, "stderr")) ctx->dump = stderr;
	else if (!strcmp(ctx->logfile, "stdout")) ctx->dump = stdout;
	else {
		ctx->dump = gf_fopen(ctx->logfile, "wt");
		if (!ctx->dump) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[Inspec] Failed to open file %s\n", ctx->logfile));
			return GF_IO_ERR;
		}
	}
	return GF_OK;
}




#define OFFS(_n)	#_n, offsetof(GF_InspectCtx, _n)
static const GF_FilterArgs InspectArgs[] =
{
	{ OFFS(logfile), "Sets inspect log filename", GF_PROP_STRING, "stderr", "file|stderr|stdout", GF_FALSE},
	{ OFFS(framing), "Enables full frame/block reconstruction before inspection", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(interleave), "Dumps packets as they are received on each pid. If false, report per pid is generated", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(pid_only), "Only dumps PID state change, not packets", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(dump_data), "Enables full data dump - heavy !", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{0}
};

static const GF_FilterCapability InspectCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_UNKNOWN),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
};

const GF_FilterRegister InspectRegister = {
	.name = "inspect",
	.description = "Inspect packets on pids",
	.private_size = sizeof(GF_InspectCtx),
	.explicit_only = 1,
	.max_extra_pids = (u32) -1,
	.args = InspectArgs,
	SETCAPS(InspectCaps),
	.initialize = inspect_initialize,
	.finalize = inspect_finalize,
	.process = inspect_process,
	.configure_pid = inspect_config_input,
};

static const GF_FilterCapability ProberCaps[] =
{
	//accept any stream but files, framed
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},

	//for scene and OD, we don't want raw codecid (filters modifying a scene graph we don't expose)
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0}
};


const GF_FilterRegister ProbeRegister = {
	.name = "probe",
	.description = "Inspect packets on demux pids (not file)",
	.private_size = sizeof(GF_InspectCtx),
	.explicit_only = 1,
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



