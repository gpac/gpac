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
#include <gpac/constants.h>
#include <gpac/download.h>

enum {
	GF_GPAC_DOWNLOAD_SESSION = GF_4CC('G','H','T','T'),
};

typedef enum
{
	GF_HTTPIN_STORE_DISK=0,
	GF_HTTPIN_STORE_DISK_KEEP,
	GF_HTTPIN_STORE_MEM,
	GF_HTTPIN_STORE_NONE,
} GF_HTTPInStoreMode;

typedef struct
{
	//options
	char *src;
	u32 block_size;
	GF_HTTPInStoreMode cache;
	GF_Fraction range;

	Bool initial_ack_done;
	GF_DownloadManager *dm;

	//only one output pid declared
	GF_FilterPid *pid;

	GF_DownloadSession *sess;

	char *block;
	Bool pck_out, is_end;
	u32 nb_read, file_size;
	FILE *cached;

	Bool do_reconfigure;
	Bool full_file_only;
	GF_Err last_state;
} GF_HTTPInCtx;

GF_Err filein_declare_pid(GF_Filter *filter, GF_FilterPid **pid, const char *url, const char *local_file, const char *mime_type, char *probe_data, u32 probe_size);

GF_Err httpin_initialize(GF_Filter *filter)
{
	GF_HTTPInCtx *ctx = (GF_HTTPInCtx *) gf_filter_get_udta(filter);
	GF_Err e;
	u32 flags = 0;

	if (!ctx || !ctx->src) return GF_BAD_PARAM;
	ctx->dm = gf_filter_get_download_manager(filter);
	if (!ctx->dm) return GF_SERVICE_ERROR;

	ctx->block = gf_malloc(ctx->block_size +1);

	flags = GF_NETIO_SESSION_NOT_THREADED | GF_NETIO_SESSION_PERSISTENT;
	if (ctx->cache==GF_HTTPIN_STORE_MEM) flags |= GF_NETIO_SESSION_MEMORY_CACHE;
	else if (ctx->cache==GF_HTTPIN_STORE_NONE) flags |= GF_NETIO_SESSION_NOT_CACHED;

	ctx->sess = gf_dm_sess_new(ctx->dm, ctx->src, flags, NULL, NULL, &e);
	if (e) {
		gf_filter_setup_failure(filter, e);
		ctx->initial_ack_done = GF_TRUE;
		return e;
	}
	if (ctx->range.den) {
		gf_dm_sess_set_range(ctx->sess, ctx->range.num, ctx->range.den, GF_TRUE);
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
	if (!strnicmp(url, "gmem://", 7) ) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}


static void httpin_notify_error(GF_Filter *filter, GF_HTTPInCtx *ctx, GF_Err e)
{
	if (ctx->last_state == GF_OK) {
		if (!ctx->initial_ack_done) {
			gf_filter_setup_failure(filter, e);
			ctx->initial_ack_done = GF_TRUE;
		} else {
			gf_filter_notification_failure(filter, e, GF_FALSE);
		}
		ctx->last_state = e;
	}
}

static Bool httpin_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_Err e;
	GF_HTTPInCtx *ctx = (GF_HTTPInCtx *) gf_filter_get_udta(filter);

	if (evt->base.on_pid && (evt->base.on_pid != ctx->pid)) return GF_FALSE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		ctx->is_end = GF_FALSE;
		ctx->full_file_only = evt->play.full_file_only;
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
			ctx->last_state = GF_OK;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTPIn] Requested seek outside file range !\n") );
			ctx->is_end = GF_TRUE;
			gf_filter_pid_set_eos(ctx->pid);
		}
		return GF_TRUE;
	case GF_FEVT_SOURCE_SWITCH:
		assert(ctx->sess);
		if (evt->seek.source_switch) {
			if ((ctx->cache!=GF_HTTPIN_STORE_DISK_KEEP) && !evt->seek.previous_is_init_segment) {
				gf_dm_delete_cached_file_entry_session(ctx->sess, ctx->src);
			}
			if (ctx->src) gf_free(ctx->src);
			ctx->src = gf_strdup(evt->seek.source_switch);
		}
		if (ctx->cached) gf_fclose(ctx->cached);
		ctx->cached = NULL;

		//abort type
		if (evt->seek.start_offset == (u64) -1) {
			if (!ctx->is_end) {
				gf_dm_sess_abort(ctx->sess);
				ctx->is_end = GF_TRUE;
				gf_filter_pid_set_eos(ctx->pid);
			}
			ctx->nb_read = 0;
			ctx->last_state = GF_OK;
			return GF_OK;
		}

		assert(ctx->is_end);
		assert(!ctx->pck_out);
		e = gf_dm_sess_setup_from_url(ctx->sess, ctx->src, evt->seek.skip_cache_expiration);
		if (!e) e = gf_dm_sess_set_range(ctx->sess, evt->seek.start_offset, evt->seek.end_offset, GF_TRUE);
		if (e) {
			httpin_notify_error(filter, ctx, e);
		}
		ctx->nb_read = ctx->file_size = 0;
		ctx->do_reconfigure = GF_TRUE;
		ctx->is_end = GF_FALSE;
		ctx->last_state = GF_OK;
		gf_filter_post_process_task(filter);
		return GF_TRUE;
	default:
		break;
	}
	return GF_TRUE;
}

static void httpin_rel_pck(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_HTTPInCtx *ctx = (GF_HTTPInCtx *) gf_filter_get_udta(filter);
	ctx->pck_out = GF_FALSE;
	//ready to process again
	gf_filter_post_process_task(filter);
}

static GF_Err httpin_process(GF_Filter *filter)
{
	Bool is_start;
	u32 nb_read=0, total_size;
	GF_FilterPacket *pck;
	GF_Err e;
	u32 bytes_done, bytes_per_sec;
	GF_NetIOStatus net_status;
	GF_HTTPInCtx *ctx = (GF_HTTPInCtx *) gf_filter_get_udta(filter);

	//until packet is released we return EOS (no processing), and ask for processing again upon release
	if (ctx->pck_out)
		return GF_EOS;

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

		if (ctx->full_file_only) {
			ctx->is_end = GF_TRUE;
			pck = gf_filter_pck_new_shared(ctx->pid, ctx->block, 0, httpin_rel_pck);
			gf_filter_pck_set_framing(pck, is_start, ctx->is_end);

			//mark packet out BEFORE sending, since the call to send() may destroy the packet if cloned
			ctx->pck_out = GF_TRUE;
			gf_filter_pck_send(pck);

			gf_filter_pid_set_eos(ctx->pid);
			return GF_EOS;
		}
		nb_read = fread(ctx->block, 1, to_read, ctx->cached);

	}
	//we read from network
	else {

		e = gf_dm_sess_fetch_data(ctx->sess, ctx->block, ctx->block_size, &nb_read);
		if (e<0) {
			if (! ctx->nb_read)
				httpin_notify_error(filter, ctx, e);
			return e;
		}
		gf_dm_sess_get_stats(ctx->sess, NULL, NULL, &total_size, &bytes_done, &bytes_per_sec, &net_status);

		//wait until we have some data to declare the pid
		if ((e!= GF_EOS) && !nb_read) return GF_OK;

		if (!ctx->pid || ctx->do_reconfigure) {
			u32 idx;
			const char *hname, *hval;
			const char *cached = gf_dm_sess_get_cache_name(ctx->sess);
			ctx->do_reconfigure = GF_FALSE;
			if ((e==GF_EOS) && cached && strnicmp(cached, "gmem://", 7)) {
				ctx->cached = gf_fopen(cached, "rb");
				if (ctx->cached) {
					nb_read = fread(ctx->block, 1, ctx->block_size, ctx->cached);
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[HTTPIn] Failed to open cached file %s\n", cached));
				}
			}
			ctx->file_size = total_size;
			ctx->block[nb_read] = 0;
			e = filein_declare_pid(filter, &ctx->pid, ctx->src, cached, gf_dm_sess_mime_type(ctx->sess), ctx->block, nb_read);
			if (e) return e;
			if (!ctx->initial_ack_done) {
				ctx->initial_ack_done = GF_TRUE;
				gf_filter_pid_set_property(ctx->pid, GF_GPAC_DOWNLOAD_SESSION, &PROP_POINTER( (void*)ctx->sess ) );
			}

			gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_FILE_CACHED, &PROP_BOOL(((e==GF_EOS) && cached) ? GF_TRUE : GF_FALSE) );

			idx = 0;
			while (gf_dm_sess_enum_headers(ctx->sess, &idx, &hname, &hval) == GF_OK) {
				if (!strcmp(hname, "icy-name")) {
					gf_filter_pid_set_info_str(ctx->pid, "icy-name", & PROP_STRING((char *) hval));
				} else if (!strcmp(hname, "icy-genre")) {
					gf_filter_pid_set_info_str(ctx->pid, "icy-genre", & PROP_STRING((char *) hval));
				} else if (!strcmp(hname, "icy-meta")) {
					gf_filter_pid_set_info_str(ctx->pid, "icy-meta", & PROP_STRING((char *) hval));
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
	gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
	gf_filter_pck_set_byte_offset(pck, ctx->nb_read);

	//mark packet out BEFORE sending, since the call to send() may destroy the packet if cloned
	ctx->pck_out = GF_TRUE;
	gf_filter_pck_send(pck);

	if (ctx->is_end) {
		gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_FILE_CACHED, &PROP_BOOL(GF_TRUE) );
		gf_filter_pid_set_eos(ctx->pid);
		return GF_EOS;
	}

	return ctx->pck_out ? GF_EOS : GF_OK;
}



#define OFFS(_n)	#_n, offsetof(GF_HTTPInCtx, _n)

static const GF_FilterArgs HTTPInArgs[] =
{
	{ OFFS(src), "location of source content", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{ OFFS(block_size), "block size used to read file", GF_PROP_UINT, "1000000", NULL, GF_FALSE},
	{ OFFS(cache), "Sets cache mode: disk, disk without discarding, memory or none", GF_PROP_UINT, "disk", "disk|keep|mem|none", GF_FALSE},
	{ OFFS(range), "Sets byte range, as fraction", GF_PROP_FRACTION, "0-0", NULL, GF_FALSE},
	{}
};

GF_FilterRegister HTTPInRegister = {
	.name = "http",
	.description = "HTTP Input",
	.private_size = sizeof(GF_HTTPInCtx),
	.args = HTTPInArgs,
	.initialize = httpin_initialize,
	.finalize = httpin_finalize,
	.process = httpin_process,
	.process_event = httpin_process_event,
	.probe_url = httpin_probe_url
};


const GF_FilterRegister *httpin_register(GF_FilterSession *session)
{
	return &HTTPInRegister;
}

