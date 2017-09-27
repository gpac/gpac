/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / HTTP input filter using GPAC http stack
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
#include <gpac/download.h>

typedef struct
{
	//options
	const char *src;
	u32 block_size;

	GF_DownloadManager *dm;

	//only one output pid declared
	GF_FilterPid *pid;

	GF_DownloadSession *sess;

	char *block;
	Bool pck_out, is_end;
	u32 nb_read, file_size;
	FILE *cached;
} GF_HTTPInCtx;

GF_FilterPid * filein_declare_pid(GF_Filter *filter, const char *url, const char *local_file, const char *mime_type, char *probe_data, u32 probe_size);

GF_Err httpin_initialize(GF_Filter *filter)
{
	GF_HTTPInCtx *ctx = (GF_HTTPInCtx *) gf_filter_get_udta(filter);
	GF_Err e;
	u32 flags = 0;
	char *frag_par = NULL;
	char *cgi_par = NULL;

	if (!ctx || !ctx->src) return GF_BAD_PARAM;
	ctx->dm = gf_filter_get_download_manager(filter);
	if (!ctx->dm) return GF_SERVICE_ERROR;

	ctx->block = gf_malloc(ctx->block_size +1);

	flags = GF_NETIO_SESSION_NOT_THREADED | GF_NETIO_SESSION_PERSISTENT;
	ctx->sess = gf_dm_sess_new(ctx->dm, ctx->src, flags, NULL, NULL, &e);
	if (e) {
		gf_filter_setup_failure(filter, e);
		return e;
	}

	return GF_OK;
}

void httpin_finalize(GF_Filter *filter)
{
	GF_HTTPInCtx *ctx = (GF_HTTPInCtx *) gf_filter_get_udta(filter);

	if (ctx->sess) gf_dm_sess_del(ctx->sess);

	if (ctx->block) gf_free(ctx->block);
	if (ctx->cached) gf_fclose(ctx->cached);
}

GF_FilterProbeScore httpin_probe_url(const char *url, const char *mime_type)
{
	if (!strnicmp(url, "http://", 7) ) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "https://", 8) ) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

static void httpin_rel_pck(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_HTTPInCtx *ctx = (GF_HTTPInCtx *) gf_filter_get_udta(filter);
	ctx->pck_out = GF_FALSE;
}

static Bool httpin_process_event(GF_Filter *filter, GF_FilterEvent *evt)
{
	GF_FilterPacket *pck;
	GF_HTTPInCtx *ctx = (GF_HTTPInCtx *) gf_filter_get_udta(filter);

	if (!evt->base.on_pid) return GF_FALSE;
	if (evt->base.on_pid != ctx->pid) return GF_FALSE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		ctx->is_end = GF_FALSE;
		return GF_TRUE;
	case GF_FEVT_STOP:
		if (!ctx->is_end) {
			//abort session
			ctx->is_end = GF_TRUE;
			gf_dm_sess_abort(ctx->sess);
		}
		return GF_TRUE;
	case GF_FEVT_SOURCE_SEEK:
		if (evt->seek.start_offset < ctx->file_size) {
			ctx->is_end = GF_FALSE;
			//open cache if needed
			if (!ctx->cached && ctx->file_size && (ctx->nb_read==ctx->file_size) ) {
				const char *cached = gf_dm_sess_get_cache_name(ctx->sess);
				if (cached) ctx->cached = gf_fopen(cached, "rb");
			}
			ctx->nb_read = evt->seek.start_offset;

			if (ctx->cached) {
				gf_fseek(ctx->cached, ctx->nb_read, SEEK_SET);
			} else {
				gf_dm_sess_abort(ctx->sess);
				gf_dm_sess_set_range(ctx->sess, ctx->nb_read, 0, GF_TRUE);
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTPIn] Requested seek outside file range !\n") );
			ctx->is_end = GF_TRUE;
			gf_filter_pid_set_eos(ctx->pid);
		}
		return GF_TRUE;
	default:
		break;
	}
	return GF_FALSE;
}

static GF_Err httpin_process(GF_Filter *filter)
{
	Bool is_start;
	u32 nb_read=0, total_size;
	GF_FilterPacket *pck;
	char *pck_data;
	GF_Err e;
	u32 bytes_done, bytes_per_sec;
	GF_NetIOStatus net_status;
	GF_HTTPInCtx *ctx = (GF_HTTPInCtx *) gf_filter_get_udta(filter);

	if (ctx->pck_out)
		return GF_OK;

	if (ctx->is_end)
		return GF_EOS;

	if (!ctx->pid) {
		if (ctx->nb_read) return GF_SERVICE_ERROR;
	} else {
		//TODO: go on fetching data to cache even when not consuming, and reread from cache
		if (gf_filter_pid_would_block(ctx->pid))
			return GF_OK;
	}

	is_start = ctx->nb_read ? GF_FALSE : GF_TRUE;
	ctx->is_end = GF_FALSE;

	//we read from cache file
	if (ctx->cached) {
		u32 to_read = ctx->file_size - ctx->nb_read;
		if (to_read>ctx->block_size) to_read = ctx->block_size;

		nb_read = fread(ctx->block, 1, to_read, ctx->cached);

	}
	//we read from network
	else {

		e = gf_dm_sess_fetch_data(ctx->sess, ctx->block, ctx->block_size, &nb_read);
		if (e<0) {
			if (! ctx->nb_read)
				gf_filter_setup_failure(filter, e);
			return e;
		}
		gf_dm_sess_get_stats(ctx->sess, NULL, NULL, &total_size, &bytes_done, &bytes_per_sec, &net_status);

		//wait until we have some data to declare the pid
		if ((e!= GF_EOS) && !nb_read) return GF_OK;

		if (!ctx->pid) {
			u32 idx;
			const char *hname, *hval;
			const char *cached = gf_dm_sess_get_cache_name(ctx->sess);
			if ((e==GF_EOS) && cached) {
				ctx->cached = gf_fopen(cached, "rb");
				if (ctx->cached) {
					nb_read = fread(ctx->block, 1, ctx->block_size, ctx->cached);
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[HTTPIn] Failed to open cached file %s\n", cached));
				}
			}
			ctx->file_size = total_size;
			ctx->block[nb_read] = 0;
			ctx->pid = filein_declare_pid(filter, ctx->src, cached, gf_dm_sess_mime_type(ctx->sess), ctx->block, nb_read);
			if (!ctx->pid) return GF_SERVICE_ERROR;

			if ((e==GF_EOS) && cached)
				gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_FILE_CACHED, &PROP_BOOL(GF_TRUE) );

			idx = 0;
			while (gf_dm_sess_enum_headers(ctx->sess, &idx, &hname, &hval) == GF_OK) {
				if (!strcmp(hname, "icy-name")) {
					gf_filter_pid_set_info_str(ctx->pid, "icy-name", & PROP_STRING(hval));
				} else if (!strcmp(hname, "icy-genre")) {
					gf_filter_pid_set_info_str(ctx->pid, "icy-genre", & PROP_STRING(hval));
				} else if (!strcmp(hname, "icy-meta")) {
					gf_filter_pid_set_info_str(ctx->pid, "icy-meta", & PROP_STRING(hval));
				}
			}
		}

		gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_RATE, &PROP_UINT(8*bytes_per_sec) );
		gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_BYTES, &PROP_UINT(bytes_done) );
		gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_SIZE, &PROP_UINT(ctx->file_size) );
	}

	ctx->nb_read += nb_read;
	if (ctx->file_size && (ctx->nb_read==ctx->file_size)) {
		ctx->is_end = GF_TRUE;
	} else if (e==GF_EOS) {
		ctx->is_end = GF_TRUE;
	}
	assert(!ctx->file_size || (ctx->nb_read <= ctx->file_size));

	pck = gf_filter_pck_new_shared(ctx->pid, ctx->block, nb_read, httpin_rel_pck);
	if (!pck) return GF_OK;

	gf_filter_pck_set_cts(pck, 0);

	gf_filter_pck_set_framing(pck, is_start, ctx->is_end);
	gf_filter_pck_set_sap(pck, 1);
	gf_filter_pck_set_byte_offset(pck, ctx->nb_read);

	//mark packet out BEFORE sending, since the call to send() may destroy the packet if cloned
	ctx->pck_out = GF_TRUE;
	gf_filter_pck_send(pck);

	if (ctx->is_end) {
		gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_FILE_CACHED, &PROP_BOOL(GF_TRUE) );
		gf_filter_pid_set_eos(ctx->pid);
		return GF_EOS;
	}

	return GF_OK;
}



#define OFFS(_n)	#_n, offsetof(GF_HTTPInCtx, _n)

static const GF_FilterArgs HTTPInArgs[] =
{
	{ OFFS(src), "location of source content", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{ OFFS(block_size), "block size used to read file", GF_PROP_UINT, "1000000", NULL, GF_FALSE},
	{}
};

static const GF_FilterCapability HTTPInOutputs[] =
{
	{.code=GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_FILE)},
	{}
};

GF_FilterRegister HTTPInRegister = {
	.name = "http",
	.description = "HTTP Input",
	.private_size = sizeof(GF_HTTPInCtx),
	.args = HTTPInArgs,
	.output_caps = HTTPInOutputs,
	.initialize = httpin_initialize,
	.finalize = httpin_finalize,
	.process = httpin_process,
	.update_arg = NULL,
	.process_event = httpin_process_event,
	.probe_url = httpin_probe_url
};


const GF_FilterRegister *httpin_register(GF_FilterSession *session)
{
	return &HTTPInRegister;
}

