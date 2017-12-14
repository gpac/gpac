/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / generic FILE output filter
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
#include <gpac/xml.h>

typedef struct
{
	//options
	char *dst;
	Bool append, dynext;

	//only one output pid declared for now
	GF_FilterPid *pid;

	FILE *file;
	Bool is_std;
} GF_FileOutCtx;

static GF_Err fileout_open_close(GF_FileOutCtx *ctx, const char *filename, const char *ext)
{
	if (ctx->file && !ctx->is_std) gf_fclose(ctx->file);
	ctx->file = NULL;
	if (!filename) return GF_OK;

	if (!strcmp(filename, "std")) ctx->is_std = GF_TRUE;
	else if (!strcmp(filename, "stdout")) ctx->is_std = GF_TRUE;
	else ctx->is_std = GF_FALSE;

	if (ctx->is_std) {
		ctx->file = stdout;
	} else {
		if (ctx->dynext && ext) {
			char szName[GF_MAX_PATH];
			strcpy(szName, filename);
			strcat(szName, ".");
			strcat(szName, ext);
			ctx->file = gf_fopen(szName, ctx->append ? "a+" : "w");
		} else {
			ctx->file = gf_fopen(filename, ctx->append ? "a+" : "w");
		}
	}
	if (!ctx->file) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] cannot open output file %s\n", filename));
		return GF_IO_ERR;
	}
	return GF_OK;
}

static GF_Err fileout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);
	if (is_remove) {
		ctx->pid = NULL;
		fileout_open_close(ctx, NULL, NULL);
	}
	gf_filter_pid_check_caps(pid);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_FILE_EXT);
	if (p && p->value.string) {
		fileout_open_close(ctx, ctx->dst, p->value.string);
	}
	ctx->pid = pid;
	return GF_OK;
}

static GF_Err fileout_initialize(GF_Filter *filter)
{
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);

	if (!ctx || !ctx->dst) return GF_OK;

	if (strnicmp(ctx->dst, "file:/", 6) && strstr(ctx->dst, "://"))  {
		gf_filter_setup_failure(filter, GF_NOT_SUPPORTED);
		return GF_NOT_SUPPORTED;
	}
	if (ctx->dynext) return GF_OK;
	fileout_open_close(ctx, ctx->dst, NULL);
	return GF_OK;
}

static void fileout_finalize(GF_Filter *filter)
{
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);
	fileout_open_close(ctx, NULL, NULL);
}

static GF_Err fileout_process(GF_Filter *filter)
{
	GF_FilterPacket *pck;
	const GF_PropertyValue *fname, *fext;
	Bool start, end;
	const char *pck_data;
	u32 pck_size, nb_write;
	GF_FileOutCtx *ctx = (GF_FileOutCtx *) gf_filter_get_udta(filter);

	pck = gf_filter_pid_get_packet(ctx->pid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->pid)) {
			fileout_open_close(ctx, NULL, NULL);
			return GF_EOS;
		}
		return GF_OK;
	}

	gf_filter_pck_get_framing(pck, &start, &end);

	if (start) {
		fname = gf_filter_pck_get_property(pck, GF_PROP_PID_FILEPATH);
		fext = gf_filter_pck_get_property(pck, GF_PROP_PID_FILE_EXT);

		if (fname && fname->value.string) {
			fileout_open_close(ctx, fname->value.string, fext ? fext->value.string : NULL);
		}
	}

	pck_data = gf_filter_pck_get_data(pck, &pck_size);
	if (ctx->file) {
		nb_write = fwrite(pck_data, 1, pck_size, ctx->file);
		if (nb_write!=pck_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] Write error, wrote %d bytes but had %d to write\n", nb_write, pck_size));
		}
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[FileOut] output file handle is not opened, discarding %d bytes\n", pck_size));

	}
	gf_filter_pid_drop_packet(ctx->pid);
	if (end) {
		fileout_open_close(ctx, NULL, NULL);
	}
	return GF_OK;
}



#define OFFS(_n)	#_n, offsetof(GF_FileOutCtx, _n)

static const GF_FilterArgs FileOutArgs[] =
{
	{ OFFS(dst), "location of source content", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{ OFFS(append), "open in append mode", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(dynext), "indicates the file extension is set by filter chain, not dst", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{}
};

static const GF_FilterCapability FileOutOutputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};


GF_FilterRegister FileOutRegister = {
	.name = "fileout",
	.description = "Generic File Output",
	.private_size = sizeof(GF_FileOutCtx),
	.args = FileOutArgs,
	INCAPS(FileOutOutputs),
	.initialize = fileout_initialize,
	.finalize = fileout_finalize,
	.configure_pid = fileout_configure_pid,
	.process = fileout_process
};


const GF_FilterRegister *fileout_register(GF_FilterSession *session)
{
	return &FileOutRegister;
}

