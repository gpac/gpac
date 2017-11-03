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
/*for GF_STREAM_PRIVATE_SCENE definition*/
#include <gpac/constants.h>
#include <gpac/xml.h>

typedef struct
{
	//options
	char *src;
	u32 block_size;
	GF_Fraction range;

	//only one output pid declared
	GF_FilterPid *pid;

	FILE *file;
	u64 file_size;
	u64 file_pos, end_pos;
	Bool is_end, pck_out;

	Bool do_reconfigure;
	char *block;
} GF_FileInCtx;


GF_Err filein_declare_pid(GF_Filter *filter, GF_FilterPid **the_pid, const char *url, const char *local_file, const char *mime_type, char *probe_data, u32 probe_size)
{
	char *ext = NULL;
	char *sep;
	GF_FilterPid *pid = (*the_pid);
	//declare a single PID carrying FILE data pid
	if (!pid) {
		pid = gf_filter_pid_new(filter);
		(*the_pid) = pid;
		if (!pid) return GF_OUT_OF_MEM;
	}

	if (local_file)
		gf_filter_pid_set_property(pid, GF_PROP_PID_FILEPATH, &PROP_STRING((char *)local_file));

	gf_filter_pid_set_property(pid, GF_PROP_PID_URL, &PROP_STRING((char *)url));
	gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );

	sep = strrchr(url, '/');
	if (!sep) sep = strrchr(url, '\\');
	if (!sep) sep = (char *) url;
	else sep++;
	gf_filter_pid_set_name(pid, sep);


	ext = strrchr(url, '.');
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

	//TODO - make this generic
	if (!mime_type && probe_data) {
		if (strstr(probe_data, "<XMT-A") || strstr(probe_data, ":mpeg4:xmta:")) {
			mime_type = "application/x-xmt";
		} else if (strstr(probe_data, "InitialObjectDescriptor")
			|| (strstr(probe_data, "EXTERNPROTO") && strstr(probe_data, "gpac:"))
		) {
			mime_type = "application/x-bt";
		} else if (strstr(probe_data, "#VRML V2.0 utf8")) {
			mime_type = "model/vrml";
		} else if ( strstr(probe_data, "#X3D V3.0")) {
			mime_type = "model/x3d+vrml";
		} else if (strstr(probe_data, "<X3D") || strstr(probe_data, "/x3d-3.0.dtd")) {
			mime_type = "model/x3d+xml";
		} else if (strstr(probe_data, "<saf") || strstr(probe_data, "mpeg4:SAF:2005")
			|| strstr(probe_data, "mpeg4:LASeR:2005")
		) {
			mime_type = "application/x-LASeR+xml";
		} else if (strstr(probe_data, "<svg") || strstr(probe_data, "w3.org/2000/svg") ) {
			mime_type = "image/svg+xml";
		} else if (strstr(probe_data, "<widget")  ) {
			mime_type = "application/widget";
		}
	}
	if (mime_type)
		gf_filter_pid_set_property(pid, GF_PROP_PID_MIME, &PROP_STRING((char *) mime_type));

	return GF_OK;
}


GF_Err filein_initialize(GF_Filter *filter)
{
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);
	char *frag_par = NULL;
	char *cgi_par = NULL;
	char *src;

	if (!ctx || !ctx->src) return GF_BAD_PARAM;

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
		ctx->file = gf_fopen(src, "r");

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

void filein_finalize(GF_Filter *filter)
{
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);

	if (ctx->file) gf_fclose(ctx->file);
	if (ctx->block) gf_free(ctx->block);

}

GF_FilterProbeScore filein_probe_url(const char *url, const char *mime_type)
{
	char *frag_par = NULL;
	char *cgi_par = NULL;
	char *src = (char *) url;
	Bool res;
	if (!strnicmp(url, "file://", 7)) src += 7;
	else if (!strnicmp(url, "file:", 5)) src += 5;

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


void filein_pck_destructor(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
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
	if (ctx->end_pos > ctx->file_pos)
		to_read = ctx->end_pos - ctx->file_pos;
	else
		to_read = ctx->file_size - ctx->file_pos;

	if (to_read > ctx->block_size)
		to_read = ctx->block_size;

	nb_read = fread(ctx->block, 1, to_read, ctx->file);

	ctx->block[nb_read] = 0;
	if (!ctx->pid || ctx->do_reconfigure) {
		ctx->do_reconfigure = GF_FALSE;
		e = filein_declare_pid(filter, &ctx->pid, ctx->src, ctx->src, NULL, ctx->block, nb_read);
		if (e) return e;
		gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_FILE_CACHED, &PROP_BOOL(GF_TRUE) );
		gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_SIZE, &PROP_LONGUINT(ctx->file_size) );
		if (ctx->range.num || ctx->range.den)
			gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_FILE_RANGE, &PROP_FRAC(ctx->range) );
	}
	pck = gf_filter_pck_new_shared(ctx->pid, ctx->block, nb_read, filein_pck_destructor);
	if (!pck)
		return GF_OK;

	gf_filter_pck_set_byte_offset(pck, ctx->file_pos);
	ctx->file_pos += nb_read;
	if (ctx->file_pos == ctx->file_size)
		ctx->is_end = GF_TRUE;
	gf_filter_pck_set_framing(pck, ctx->file_pos ? GF_FALSE : GF_TRUE, ctx->is_end);
	gf_filter_pck_set_sap(pck, 1);

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
	{}
};

GF_FilterRegister FileInRegister = {
	.name = "filein",
	.description = "Generic File Input",
	.private_size = sizeof(GF_FileInCtx),
	.args = FileInArgs,
	.initialize = filein_initialize,
	.finalize = filein_finalize,
	.process = filein_process,
	.process_event = filein_process_event,
	.probe_url = filein_probe_url
};


const GF_FilterRegister *filein_register(GF_FilterSession *session)
{
	return &FileInRegister;
}

