/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / generic FILE input filter
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

typedef struct
{
	//options
	char *src;
	char *ext, mime;
	u32 block_size;
	GF_Fraction64 range;

	//only one output pid declared
	GF_FilterPid *pid;

	FILE *file;
	u64 file_size;
	u64 file_pos, end_pos;
	Bool is_end, pck_out;
	Bool is_null;
	Bool full_file_only;
	Bool do_reconfigure;
	char *block;
} GF_FileInCtx;


GF_Err filein_declare_pid(GF_Filter *filter, GF_FilterPid **the_pid, const char *url, const char *local_file, const char *mime_type, const char *fext, char *probe_data, u32 probe_size)
{
	char *sep;
	GF_FilterPid *pid = (*the_pid);
	//declare a single PID carrying FILE data pid
	if (!pid) {
		pid = gf_filter_pid_new(filter);
		(*the_pid) = pid;
		if (!pid) return GF_OUT_OF_MEM;
	}

	if (local_file)
		gf_filter_pid_set_property(pid, GF_PROP_PID_FILEPATH, &PROP_STRING(local_file));

	gf_filter_pid_set_property(pid, GF_PROP_PID_URL, &PROP_STRING(url));
	gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );

	sep = strrchr(url, '/');
	if (!sep) sep = strrchr(url, '\\');
	if (!sep) sep = (char *) url;
	else sep++;
	gf_filter_pid_set_name(pid, sep);

	if (fext) {
		gf_filter_pid_set_property(pid, GF_PROP_PID_FILE_EXT, &PROP_STRING(fext));
	} else {
		char *ext = strrchr(url, '.');
		if (ext && !stricmp(ext, ".gz")) {
			char *anext;
			ext[0] = 0;
			anext = strrchr(url, '.');
			ext[0] = '.';
			ext = anext;
		}
		if (ext) ext++;

		if (ext) {
			char *s = strchr(ext, '#');
			if (s) s[0] = 0;

			gf_filter_pid_set_property(pid, GF_PROP_PID_FILE_EXT, &PROP_STRING(ext));
			if (s) s[0] = '#';
		}
	}
	//probe data
	if (!mime_type && probe_data) {
		mime_type = gf_filter_probe_mime(filter, probe_data, probe_size);
	}
	if (mime_type)
		gf_filter_pid_set_property(pid, GF_PROP_PID_MIME, &PROP_STRING( mime_type));

	return GF_OK;
}


static GF_Err filein_initialize(GF_Filter *filter)
{
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);
	char *frag_par = NULL;
	char *cgi_par = NULL;
	char *src;

	if (!ctx || !ctx->src) return GF_BAD_PARAM;

	if (!strcmp(ctx->src, "null")) {
		ctx->pid = gf_filter_pid_new(filter);
		gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE));
		gf_filter_pid_set_eos(ctx->pid);
		ctx->is_end = GF_TRUE;
		return GF_OK;
	}

	if (strnicmp(ctx->src, "file:/", 6) && strstr(ctx->src, "://"))  {
		gf_filter_setup_failure(filter, GF_NOT_SUPPORTED);
		return GF_NOT_SUPPORTED;
	}

	//local file

	//strip any fragment identifer
	frag_par = strchr(ctx->src, '#');
	if (frag_par) frag_par[0] = 0;
	cgi_par = strchr(ctx->src, '?');
	if (cgi_par) cgi_par[0] = 0;

	src = (char *) ctx->src;
	if (!strnicmp(ctx->src, "file://", 7)) src += 7;
	else if (!strnicmp(ctx->src, "file:", 5)) src += 5;

	if (!ctx->file)
		ctx->file = gf_fopen(src, "rb");

	if (!ctx->file) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FileIn] Failed to open %s\n", src));

		if (frag_par) frag_par[0] = '#';
		if (cgi_par) cgi_par[0] = '?';

		gf_filter_setup_failure(filter, GF_URL_ERROR);
		return GF_URL_ERROR;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("[FileIn] opening %s\n", src));
	gf_fseek(ctx->file, 0, SEEK_END);
	ctx->file_size = gf_ftell(ctx->file);
	ctx->file_pos = ctx->range.num;
	gf_fseek(ctx->file, ctx->file_pos, SEEK_SET);
	if (ctx->range.den) {
		ctx->end_pos = ctx->range.den;
		if (ctx->end_pos>ctx->file_size) {
			ctx->range.den = ctx->end_pos = ctx->file_size;
		}
	}
	ctx->is_end = GF_FALSE;

	if (frag_par) frag_par[0] = '#';
	if (cgi_par) cgi_par[0] = '?';

	if (!ctx->block)
		ctx->block = gf_malloc(ctx->block_size +1);

	return GF_OK;
}

static void filein_finalize(GF_Filter *filter)
{
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);

	if (ctx->file) gf_fclose(ctx->file);
	if (ctx->block) gf_free(ctx->block);

}

static GF_FilterProbeScore filein_probe_url(const char *url, const char *mime_type)
{
	char *frag_par = NULL;
	char *cgi_par = NULL;
	char *src = (char *) url;
	Bool res;
	if (!strnicmp(url, "file://", 7)) src += 7;
	else if (!strnicmp(url, "file:", 5)) src += 5;

	if (!strcmp(url, "null")) return GF_FPROBE_SUPPORTED;

	//strip any fragment identifer
	frag_par = strchr(url, '#');
	if (frag_par) frag_par[0] = 0;
	cgi_par = strchr(url, '?');
	if (cgi_par) cgi_par[0] = 0;

	res = gf_file_exists(src);

	if (frag_par) frag_par[0] = '#';
	if (cgi_par) cgi_par[0] = '?';

	return res ? GF_FPROBE_SUPPORTED : GF_FPROBE_NOT_SUPPORTED;
}

static Bool filein_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);

	if (evt->base.on_pid && (evt->base.on_pid != ctx->pid))
		return GF_FALSE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		ctx->full_file_only = evt->play.full_file_only;
		return GF_TRUE;
	case GF_FEVT_STOP:
		//stop sending data
		ctx->is_end = GF_TRUE;
		return GF_TRUE;
	case GF_FEVT_SOURCE_SEEK:
		if (evt->seek.start_offset < ctx->file_size) {
			ctx->is_end = GF_FALSE;
			gf_fseek(ctx->file, evt->seek.start_offset, SEEK_SET);
			ctx->file_pos = evt->seek.start_offset;
			ctx->end_pos = evt->seek.end_offset;
			if (ctx->end_pos>ctx->file_size) ctx->end_pos = ctx->file_size;
			ctx->range.num = evt->seek.start_offset;
			ctx->range.den = ctx->end_pos;
			if (evt->seek.hint_block_size > ctx->block_size) {
				ctx->block_size = evt->seek.hint_block_size;
				ctx->block = gf_realloc(ctx->block, ctx->block_size);
			}
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("[FileIn] Seek request outside of file %s range ("LLU" vs size "LLU")\n", ctx->src, evt->seek.start_offset, ctx->file_size));

		}
		return GF_TRUE;
	case GF_FEVT_SOURCE_SWITCH:
		assert(ctx->is_end);
		ctx->range.num = evt->seek.start_offset;
		ctx->range.den = evt->seek.end_offset;
		if (evt->seek.source_switch) {
			if (strcmp(evt->seek.source_switch, ctx->src)) {
				gf_free(ctx->src);
				ctx->src = gf_strdup(evt->seek.source_switch);
				if (ctx->file) gf_fclose(ctx->file);
				ctx->file = NULL;
			}
			ctx->do_reconfigure = GF_TRUE;
		}
		filein_initialize(filter);
		gf_filter_post_process_task(filter);
		break;
	default:
		break;
	}
	return GF_FALSE;
}


static void filein_pck_destructor(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);
	ctx->pck_out = GF_FALSE;
	//ready to process again
	gf_filter_post_process_task(filter);
}

static GF_Err filein_process(GF_Filter *filter)
{
	GF_Err e;
	u32 nb_read, to_read;
	GF_FilterPacket *pck;
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);

	if (ctx->is_end)
		return GF_EOS;

	//until packet is released we return EOS (no processing), and ask for processing again upon release
	if (ctx->pck_out)
		return GF_EOS;
	if (ctx->pid && gf_filter_pid_would_block(ctx->pid)) {
		assert(0);
		return GF_OK;
	}

	if (ctx->full_file_only && ctx->pid && !ctx->do_reconfigure) {
		ctx->is_end = GF_TRUE;
		pck = gf_filter_pck_new_shared(ctx->pid, ctx->block, 0, filein_pck_destructor);
		gf_filter_pck_set_framing(pck, ctx->file_pos ? GF_FALSE : GF_TRUE, ctx->is_end);
		gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
		ctx->pck_out = GF_TRUE;
		gf_filter_pck_send(pck);
		gf_filter_pid_set_eos(ctx->pid);
		return GF_OK;
	}

	if (ctx->end_pos > ctx->file_pos)
		to_read = (u32) (ctx->end_pos - ctx->file_pos);
	else
		to_read = (u32) (ctx->file_size - ctx->file_pos);

	if (to_read > ctx->block_size)
		to_read = ctx->block_size;

	nb_read = (u32) fread(ctx->block, 1, to_read, ctx->file);

	ctx->block[nb_read] = 0;
	if (!ctx->pid || ctx->do_reconfigure) {
		ctx->do_reconfigure = GF_FALSE;
		e = filein_declare_pid(filter, &ctx->pid, ctx->src, ctx->src, ctx->mime, ctx->ext, ctx->block, nb_read);
		if (e) return e;
		gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_FILE_CACHED, &PROP_BOOL(GF_TRUE) );
		gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_SIZE, &PROP_LONGUINT(ctx->file_size) );
		if (ctx->range.num || ctx->range.den)
			gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_FILE_RANGE, &PROP_FRAC64(ctx->range) );
	}
	pck = gf_filter_pck_new_shared(ctx->pid, ctx->block, nb_read, filein_pck_destructor);
	if (!pck)
		return GF_OK;

	gf_filter_pck_set_byte_offset(pck, ctx->file_pos);

	if (ctx->file_pos + nb_read == ctx->file_size)
		ctx->is_end = GF_TRUE;
	else if (nb_read < to_read) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("[FileIn] Asked to read %d but got only %d\n", to_read, nb_read));
		if (feof(ctx->file)) {
			ctx->is_end = GF_TRUE;
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FileIn] IO error EOF found after reading %d bytes but file %s size is %d\n", ctx->file_pos+nb_read, ctx->src, ctx->file_size));
		}
	}
	gf_filter_pck_set_framing(pck, ctx->file_pos ? GF_FALSE : GF_TRUE, ctx->is_end);
	gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
	ctx->file_pos += nb_read;

	ctx->pck_out = GF_TRUE;
	gf_filter_pck_send(pck);

	if (ctx->is_end) {
		gf_filter_pid_set_eos(ctx->pid);
		return GF_EOS;
	}
	return ctx->pck_out ? GF_EOS : GF_OK;
}



#define OFFS(_n)	#_n, offsetof(GF_FileInCtx, _n)

static const GF_FilterArgs FileInArgs[] =
{
	{ OFFS(src), "location of source content", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{ OFFS(block_size), "block size used to read file", GF_PROP_UINT, "5000", NULL, GF_FALSE},
	{ OFFS(range), "byte range", GF_PROP_FRACTION, "0-0", NULL, GF_FALSE},
	{ OFFS(ext), "overrides file extension", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{ OFFS(mime), "sets file mime type", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{0}
};

static const GF_FilterCapability FileInCaps[] =
{
	CAP_UINT(GF_CAPS_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};

GF_FilterRegister FileInRegister = {
	.name = "fin",
	.description = "Generic file input",
	.private_size = sizeof(GF_FileInCtx),
	.args = FileInArgs,
	.initialize = filein_initialize,
	SETCAPS(FileInCaps),
	.finalize = filein_finalize,
	.process = filein_process,
	.process_event = filein_process_event,
	.probe_url = filein_probe_url
};


const GF_FilterRegister *filein_register(GF_FilterSession *session)
{
	return &FileInRegister;
}

