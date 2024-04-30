/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2024
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

#ifdef GPAC_USE_DOWNLOADER

#include <gpac/constants.h>
#include <gpac/download.h>

typedef enum
{
	GF_HTTPIN_STORE_AUTO=0,
	GF_HTTPIN_STORE_DISK,
	GF_HTTPIN_STORE_DISK_KEEP,
	GF_HTTPIN_STORE_MEM,
	GF_HTTPIN_STORE_MEM_KEEP,
	GF_HTTPIN_STORE_NONE,
	GF_HTTPIN_STORE_NONE_KEEP,
} GF_HTTPInStoreMode;

enum
{
	HTTP_PCK_NONE=0,
	HTTP_PCK_OUT=1,
	HTTP_PCK_OUT_EOS=2,
};

typedef struct
{
	//options
	char *src;
	u32 block_size;
	GF_HTTPInStoreMode cache;
	GF_Fraction64 range;
	char *ext;
	char *mime;
	Bool blockio;


	//internal
	Bool initial_ack_done;
	GF_DownloadManager *dm;

	//only one output pid declared
	GF_FilterPid *pid;

	GF_DownloadSession *sess;

	char *block;
	u32 pck_out;
	Bool is_end;
	u64 nb_read, file_size;
	FILE *cached;
	u32 blob_size;

	Bool do_reconfigure;
	Bool full_file_only;
	GF_Err last_state;
	Bool is_source_switch;
	Bool prev_was_init_segment;
} GF_HTTPInCtx;

static void httpin_notify_error(GF_Filter *filter, GF_HTTPInCtx *ctx, GF_Err e)
{
	if (filter && (ctx->last_state == GF_OK)) {
		if (!ctx->initial_ack_done) {
			gf_filter_setup_failure(filter, e);
			ctx->initial_ack_done = GF_TRUE;
		} else {
			gf_filter_notification_failure(filter, e, GF_FALSE);
		}
		ctx->last_state = e;
	}
}

static GF_Err httpin_initialize(GF_Filter *filter)
{
	GF_HTTPInCtx *ctx = (GF_HTTPInCtx *) gf_filter_get_udta(filter);
	GF_Err e;
	char *server;
	u32 flags = 0;

	if (!ctx || !ctx->src) return GF_BAD_PARAM;
	ctx->dm = gf_filter_get_download_manager(filter);
	if (!ctx->dm) return GF_SERVICE_ERROR;

	ctx->block = gf_malloc(ctx->block_size +1);

	flags = GF_NETIO_SESSION_NOT_THREADED | GF_NETIO_SESSION_PERSISTENT;
	if (ctx->cache==GF_HTTPIN_STORE_MEM)
		flags |= GF_NETIO_SESSION_MEMORY_CACHE;
	else if (ctx->cache==GF_HTTPIN_STORE_NONE)
		flags |= GF_NETIO_SESSION_NOT_CACHED;
	else if (ctx->cache==GF_HTTPIN_STORE_DISK_KEEP)
		flags |= GF_NETIO_SESSION_KEEP_CACHE;
	else if (ctx->cache==GF_HTTPIN_STORE_AUTO)
		flags |= GF_NETIO_SESSION_AUTO_CACHE;
	else if (ctx->cache==GF_HTTPIN_STORE_MEM_KEEP) {
		flags |= GF_NETIO_SESSION_MEMORY_CACHE|GF_NETIO_SESSION_KEEP_FIRST_CACHE;
		ctx->cache = GF_HTTPIN_STORE_MEM;
	}
	else if (ctx->cache==GF_HTTPIN_STORE_NONE_KEEP) {
		flags |= GF_NETIO_SESSION_NOT_CACHED|GF_NETIO_SESSION_MEMORY_CACHE|GF_NETIO_SESSION_KEEP_FIRST_CACHE;
		ctx->cache = GF_HTTPIN_STORE_NONE;
	}
	gf_filter_set_blocking(filter, GF_TRUE);

	if (!ctx->blockio)
		flags |= GF_NETIO_SESSION_NO_BLOCK;

	server = strstr(ctx->src, "://");
	if (server) server += 3;
	if (server && strncmp(ctx->src, "http://groute", 13) && strstr(server, "://")) {
		ctx->is_end = GF_TRUE;
		return gf_filter_pid_raw_new(filter, server, server, NULL, NULL, NULL, 0, GF_FALSE, &ctx->pid);
	}

	ctx->sess = gf_dm_sess_new(ctx->dm, ctx->src, flags, NULL, NULL, &e);
	if (e) {
		gf_filter_setup_failure(filter, e);
		ctx->initial_ack_done = GF_TRUE;
		return e;
	}
	gf_dm_sess_set_netcap_id(ctx->sess, gf_filter_get_netcap_id(filter));
	if (ctx->range.num || ctx->range.den) {
		gf_dm_sess_set_range(ctx->sess, ctx->range.num, ctx->range.den, GF_TRUE);
	}

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode())
		httpin_notify_error(NULL, NULL, GF_OK);
#endif

	return GF_OK;
}


static void httpin_set_eos(GF_HTTPInCtx *ctx)
{
	//no pending packets, signal eos right away
	if (!ctx->pck_out) {
		gf_filter_pid_set_eos(ctx->pid);
	}
	else
		ctx->pck_out = HTTP_PCK_OUT_EOS;
}


void httpin_finalize(GF_Filter *filter)
{
	GF_HTTPInCtx *ctx = (GF_HTTPInCtx *) gf_filter_get_udta(filter);

	if (ctx->sess) gf_dm_sess_del(ctx->sess);

	if (ctx->block) gf_free(ctx->block);
	if (ctx->cached) gf_fclose(ctx->cached);
}

static GF_FilterProbeScore httpin_probe_url(const char *url, const char *mime_type)
{
	if (!strnicmp(url, "http://", 7) ) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "https://", 8) ) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "gmem://", 7) ) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

static void httpin_rel_pck(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_HTTPInCtx *ctx = (GF_HTTPInCtx *) gf_filter_get_udta(filter);

	if (ctx->pck_out==HTTP_PCK_OUT_EOS) {
		gf_filter_pid_set_eos(ctx->pid);
	}
	ctx->pck_out = HTTP_PCK_NONE;
	//ready to process again
	gf_filter_post_process_task(filter);
}

static Bool httpin_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_Err e;
	GF_HTTPInCtx *ctx = (GF_HTTPInCtx *) gf_filter_get_udta(filter);

	if (evt->base.on_pid && (evt->base.on_pid != ctx->pid)) return GF_FALSE;

	switch (evt->base.type) {
	//we only check PLAY for full_file_only hint
	case GF_FEVT_PLAY:
		ctx->full_file_only = evt->play.full_file_only;
		//do NOT reset is_end to false, restarting the session is always done via a source_seek event
		return GF_TRUE;
	case GF_FEVT_STOP:
		if (!ctx->is_end) {
			ctx->is_end = GF_TRUE;
			//abort session
			if (ctx->sess) {
				GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPIn] Stop requested, aborting download %s (pck out %d) this %p\n", ctx->src, ctx->pck_out, ctx) );
				gf_dm_sess_abort(ctx->sess);
				gf_dm_sess_del(ctx->sess);
				ctx->sess = NULL;
			}
			httpin_set_eos(ctx);
		}
		return GF_TRUE;
	case GF_FEVT_SOURCE_SEEK:
		if (evt->seek.start_offset < ctx->file_size) {
			ctx->is_end = GF_FALSE;
			//open cache if needed
			if (!ctx->cached && ctx->file_size && (ctx->nb_read==ctx->file_size) && ctx->sess) {
				const char *cached = gf_dm_sess_get_cache_name(ctx->sess);
				if (cached) ctx->cached = gf_fopen(cached, "rb");
			}
			ctx->nb_read = evt->seek.start_offset;

			if (ctx->cached) {
				gf_fseek(ctx->cached, ctx->nb_read, SEEK_SET);
			} else if (ctx->sess) {
				gf_dm_sess_abort(ctx->sess);
				gf_dm_sess_set_range(ctx->sess, ctx->nb_read, 0, GF_TRUE);
			}
			ctx->range.den = 0;
			ctx->range.num = ctx->nb_read;
			ctx->last_state = GF_OK;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPIn] Requested seek outside file range !\n") );
			ctx->is_end = GF_TRUE;

			httpin_set_eos(ctx);
		}
		return GF_TRUE;
	case GF_FEVT_SOURCE_SWITCH:
		if (evt->seek.source_switch) {
			gf_fatal_assert(ctx->is_end);
			gf_fatal_assert(!ctx->pck_out);
			if (ctx->src && ctx->sess && (ctx->cache!=GF_HTTPIN_STORE_DISK_KEEP) && !ctx->prev_was_init_segment) {
				gf_dm_delete_cached_file_entry_session(ctx->sess, ctx->src, GF_FALSE);
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPIn] Switch from %s to %s\n", gf_file_basename(ctx->src), gf_file_basename(evt->seek.source_switch) ));
			if (ctx->src) gf_free(ctx->src);
			ctx->src = gf_strdup(evt->seek.source_switch);
		} else {
			if (!ctx->is_end) {
				gf_filter_pid_set_eos(ctx->pid);
				ctx->is_end = GF_TRUE;
				if (ctx->sess) {
					gf_dm_sess_abort(ctx->sess);
					gf_dm_sess_del(ctx->sess);
					ctx->sess = NULL;
				}
			}
			if (ctx->src) gf_free(ctx->src);
			ctx->src = NULL;
			return GF_TRUE;
		}
		if (ctx->cached) gf_fclose(ctx->cached);
		ctx->cached = NULL;
		ctx->blob_size = 0;
		ctx->is_source_switch = GF_TRUE;
		ctx->prev_was_init_segment = GF_FALSE;

		//handle isobmff:// url
		if (!strncmp(ctx->src, "isobmff://", 10)) {
			GF_FilterPacket *pck;
			gf_filter_pid_raw_new(filter, ctx->src, ctx->src, NULL, NULL, NULL, 0, GF_FALSE, &ctx->pid);
			ctx->is_end = GF_TRUE;
			ctx->prev_was_init_segment = GF_TRUE;
			pck = gf_filter_pck_new_shared(ctx->pid, ctx->block, 0, httpin_rel_pck);
			if (!pck) return GF_TRUE;
			gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);

			ctx->pck_out = GF_TRUE;
			gf_filter_pck_send(pck);

			gf_filter_pid_set_eos(ctx->pid);
			return GF_TRUE;
		}
		ctx->prev_was_init_segment = evt->seek.is_init_segment;

		//abort type
		if (evt->seek.start_offset == (u64) -1) {
			if (!ctx->is_end) {
				if (ctx->sess)
					gf_dm_sess_abort(ctx->sess);
				ctx->is_end = GF_TRUE;
				httpin_set_eos(ctx);
			}
			ctx->nb_read = 0;
			ctx->last_state = GF_OK;
			return GF_TRUE;
		}
		ctx->last_state = GF_OK;
		if (ctx->sess) {
			if ((ctx->cache==GF_HTTPIN_STORE_MEM) && evt->seek.is_init_segment)
				gf_dm_sess_force_memory_mode(ctx->sess, 2);
			e = gf_dm_sess_setup_from_url(ctx->sess, ctx->src, evt->seek.skip_cache_expiration);
		} else {
			u32 flags;

			flags = GF_NETIO_SESSION_NOT_THREADED | GF_NETIO_SESSION_PERSISTENT;
			if (ctx->cache==GF_HTTPIN_STORE_MEM) {
				flags |= GF_NETIO_SESSION_MEMORY_CACHE;
				if (evt->seek.is_init_segment)
					flags |= GF_NETIO_SESSION_KEEP_FIRST_CACHE;
			}
			else if (ctx->cache==GF_HTTPIN_STORE_NONE) flags |= GF_NETIO_SESSION_NOT_CACHED;

			ctx->sess = gf_dm_sess_new(ctx->dm, ctx->src, flags, NULL, NULL, &e);
			if (ctx->sess) gf_dm_sess_set_netcap_id(ctx->sess, gf_filter_get_netcap_id(filter));
		}

		if (!e && (evt->seek.start_offset || evt->seek.end_offset))
            e = gf_dm_sess_set_range(ctx->sess, evt->seek.start_offset, evt->seek.end_offset, GF_TRUE);
		
        if (e) {
			//use info and not error, as source switch is done by dashin and can be scheduled too early in live cases
			//but recovered later, so we let DASH report the error
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPIn] Cannot resetup session from URL %s: %s\n", ctx->src, gf_error_to_string(e) ) );
			httpin_notify_error(filter, ctx, e);
			ctx->is_end = GF_TRUE;
			if (ctx->src) gf_free(ctx->src);
			ctx->src = NULL;
			return GF_TRUE;
		}
		ctx->nb_read = ctx->file_size = 0;
		ctx->do_reconfigure = GF_TRUE;
		ctx->is_end = GF_FALSE;
		ctx->last_state = GF_OK;
		gf_filter_post_process_task(filter);
		gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_FILE_CACHED, &PROP_BOOL(GF_FALSE) );
		return GF_TRUE;
	default:
		break;
	}
	return GF_TRUE;
}



static GF_Err httpin_process(GF_Filter *filter)
{
	Bool is_start;
	u32 nb_read=0;
	GF_FilterPacket *pck;
	GF_Err e=GF_OK;
	u32 bytes_per_sec=0;
	u64 bytes_done=0, total_size, byte_offset;
	GF_NetIOStatus net_status = GF_NETIO_DATA_EXCHANGE;
	GF_HTTPInCtx *ctx = (GF_HTTPInCtx *) gf_filter_get_udta(filter);

	//until packet is released we return EOS (no processing), and ask for processing again upon release
	if (ctx->pck_out)
		return GF_EOS;

	if (ctx->is_end)
		return GF_EOS;

	if (!ctx->sess)
		return GF_EOS;

	if (!ctx->pid) {
		if (ctx->nb_read)
            return GF_SERVICE_ERROR;
	} else {
		//TODO: go on fetching data to cache even when not consuming, and reread from cache
		if (gf_filter_pid_would_block(ctx->pid))
			return GF_OK;
	}

	is_start = ctx->nb_read ? GF_FALSE : GF_TRUE;
	ctx->is_end = GF_FALSE;

	//we read from cache file
	if (ctx->cached) {
		u32 to_read;
		u64 lto_read = ctx->file_size - ctx->nb_read;

		if (lto_read > (u64) ctx->block_size)
			to_read = (u64) ctx->block_size;
		else
			to_read = (u32) lto_read;

		if (ctx->full_file_only) {
			pck = gf_filter_pck_new_shared(ctx->pid, ctx->block, 0, httpin_rel_pck);
			if (!pck) return GF_OUT_OF_MEM;
			ctx->is_end = GF_TRUE;
			gf_filter_pck_set_framing(pck, is_start, ctx->is_end);

			//mark packet out BEFORE sending, since the call to send() may destroy the packet if cloned
			ctx->pck_out = HTTP_PCK_OUT;
			gf_filter_pck_send(pck);

			httpin_set_eos(ctx);
			return GF_EOS;
		}
		nb_read = (u32) gf_fread(ctx->block, to_read, ctx->cached);
		bytes_per_sec = 0;
		if (!nb_read && (ctx->nb_read == ctx->file_size)) {
			e = GF_EOS;
			net_status = GF_NETIO_DATA_TRANSFERED;
		}
	}
	else if (ctx->blob_size) {
		u8 *b_data;
		u32 b_size;
		const char *cached = gf_dm_sess_get_cache_name(ctx->sess);
		gf_assert(cached);

		gf_blob_get(cached, &b_data, &b_size, NULL);

		//we should NEVER have this case (file size less than at last call), abort download
		if (ctx->nb_read > b_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPIn] Error fetching %s, corrupted blob (URL %s prev size %d new size %d)\n", ctx->src, cached, ctx->blob_size, b_size ) );
			nb_read = 0;
			ctx->blob_size = b_size;
			ctx->nb_read = ctx->file_size = b_size;
			net_status = GF_NETIO_DATA_TRANSFERED;
			e = GF_EOS;
		} else {
			nb_read = b_size - (u32) ctx->nb_read;
			if (nb_read>ctx->block_size)
				nb_read = ctx->block_size;

			if (nb_read) {
				memcpy(ctx->block, b_data + ctx->nb_read, nb_read);
				e = GF_OK;
				gf_filter_ask_rt_reschedule(filter, 1);
			} else {
				if (b_size == ctx->blob_size) {
					e = gf_dm_sess_fetch_data(ctx->sess, ctx->block, ctx->block_size, &nb_read);
					if (e==GF_EOS) {
						net_status = GF_NETIO_DATA_TRANSFERED;
					}
				} else {
					ctx->blob_size = b_size;
				}
			}
		}
        gf_blob_release(cached);
	}
	//we read from network
	else {

		e = gf_dm_sess_fetch_data(ctx->sess, ctx->block, ctx->block_size, &nb_read);
		if (e<0) {
			if (e==GF_IP_NETWORK_EMPTY) {
				if (ctx->pid) {
					gf_dm_sess_get_stats(ctx->sess, NULL, NULL, NULL, NULL, &bytes_per_sec, NULL);
					gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_RATE, &PROP_UINT(8*bytes_per_sec) );
				}
				gf_filter_ask_rt_reschedule(filter, 1000);
				return GF_OK;
			}
			if (! ctx->nb_read)
				httpin_notify_error(filter, ctx, e);

			ctx->is_end = GF_TRUE;

			//do not return an error if first fetch after source switch fails with removed or 404, this happens in DASH dynamic
			//and the error might be absorbed by the dash demux later
			//also do not signal eos in that case, this could trigger the consuming filter (dashdmx, filein) to consider the stream is in regular EOS
			//and not wait for notify failure
			if (ctx->is_source_switch && !ctx->nb_read && ((e==GF_URL_REMOVED) || (e==GF_URL_ERROR)))
				return GF_OK;

			//no packet out, we can signal eos, except for source switch failures
			if (ctx->pid) {
				gf_filter_pid_set_eos(ctx->pid);
			}

			gf_dm_sess_abort(ctx->sess);
			ctx->is_source_switch = GF_FALSE;
			return e;
		}
		gf_dm_sess_get_stats(ctx->sess, NULL, NULL, &total_size, &bytes_done, &bytes_per_sec, &net_status);

		//wait until we have some data to declare the pid
        if ((e!= GF_EOS) && !nb_read) {
            gf_filter_ask_rt_reschedule(filter, 1000);
            return GF_OK;
        }

		if (!ctx->pid || ctx->do_reconfigure) {
			u32 idx;
			GF_Err cfg_e;
			const char *hname, *hval;
			const char *cached = gf_dm_sess_get_cache_name(ctx->sess);

			ctx->do_reconfigure = GF_FALSE;

			if ((e==GF_EOS) && cached) {
				if (!strnicmp(cached, "gmem://", 7)) {
					u8 *b_data;
					u32 b_size, b_flags;
					gf_blob_get(cached, &b_data, &b_size, &b_flags);
					if (b_size>ctx->block_size) {
						ctx->blob_size = b_size;
						b_size = ctx->block_size;
						e = GF_OK;
					}
					gf_assert(! (b_flags&GF_BLOB_IN_TRANSFER));
					memcpy(ctx->block, b_data, b_size);
					nb_read = b_size;
					gf_blob_release(cached);
				} else {
					ctx->cached = gf_fopen(cached, "rb");
					if (ctx->cached) {
						nb_read = (u32) gf_fread(ctx->block, ctx->block_size, ctx->cached);
						if (nb_read) e = GF_OK;
					} else {
						GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPIn] Failed to open cached file %s\n", cached));
					}
				}
			}
			ctx->block[nb_read] = 0;
			cfg_e = gf_filter_pid_raw_new(filter, ctx->src, cached, ctx->mime ? ctx->mime : gf_dm_sess_mime_type(ctx->sess), ctx->ext, ctx->block, nb_read, ctx->mime ? GF_TRUE : GF_FALSE, &ctx->pid);
			if (cfg_e) return cfg_e;

			gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_FILE_CACHED, &PROP_BOOL(ctx->cached ? GF_TRUE : GF_FALSE) );

			if (!ctx->initial_ack_done) {
				ctx->initial_ack_done = GF_TRUE;
				gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_DOWNLOAD_SESSION, &PROP_POINTER( (void*)ctx->sess ) );
			}

			/*in test mode don't expose http headers (they contain date/version/etc)*/
			if (! gf_sys_is_test_mode()) {
				idx = 0;
				while (gf_dm_sess_enum_headers(ctx->sess, &idx, &hname, &hval) == GF_OK) {
					gf_filter_pid_set_property_dyn(ctx->pid, (char *) hname, & PROP_STRING(hval));
				}
			}
		}
		//update file size at each call to get_stats, since we may use a dynamic blob in case of route
		ctx->file_size = total_size;

		gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_RATE, &PROP_UINT(8*bytes_per_sec) );
		if (ctx->range.num && ctx->file_size) {
			gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_BYTES, &PROP_LONGUINT(bytes_done + ctx->range.num) );
			gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_SIZE, &PROP_LONGUINT(ctx->file_size) );
		} else {
			gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_BYTES, &PROP_LONGUINT(bytes_done) );
			gf_filter_pid_set_info(ctx->pid, GF_PROP_PID_DOWN_SIZE, &PROP_LONGUINT(ctx->file_size ? ctx->file_size : bytes_done) );
		}
	}

	byte_offset = ctx->nb_read;

	ctx->nb_read += nb_read;
	if (ctx->file_size && (ctx->nb_read==ctx->file_size)) {
		if (net_status!=GF_NETIO_DATA_EXCHANGE)
			ctx->is_end = GF_TRUE;
	} else if (e==GF_EOS) {
		ctx->is_end = GF_TRUE;
	}

	pck = gf_filter_pck_new_shared(ctx->pid, ctx->block, nb_read, httpin_rel_pck);
	if (!pck) return GF_OUT_OF_MEM;

	gf_filter_pck_set_cts(pck, 0);

	gf_filter_pck_set_framing(pck, is_start, ctx->is_end);
	gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
	gf_filter_pck_set_byte_offset(pck, byte_offset);

	//mark packet out BEFORE sending, since the call to send() may destroy the packet if cloned
	ctx->pck_out = HTTP_PCK_OUT;
	gf_filter_pck_send(pck);

	if (ctx->file_size && gf_filter_reporting_enabled(filter)) {
		char szStatus[1024], *szSrc;
		szSrc = gf_file_basename(ctx->src);

		sprintf(szStatus, "%s: % 16"LLD_SUF" /% 16"LLD_SUF" (%02.02f) % 8d kbps", szSrc, (s64) bytes_done, (s64) ctx->file_size, ((Double)bytes_done*100.0)/ctx->file_size, bytes_per_sec*8/1000);
		gf_filter_update_status(filter, (u32) (bytes_done*10000/ctx->file_size), szStatus);
	}

	if (ctx->is_end) {
		const char *cached = gf_dm_sess_get_cache_name(ctx->sess);
		if (cached)
			gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_FILE_CACHED, &PROP_BOOL(GF_TRUE) );

		httpin_set_eos(ctx);
		return GF_EOS;
	}

	return ctx->pck_out ? GF_EOS : GF_OK;
}



#define OFFS(_n)	#_n, offsetof(GF_HTTPInCtx, _n)

static const GF_FilterArgs HTTPInArgs[] =
{
	{ OFFS(src), "URL of source content", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(block_size), "block size used to read file", GF_PROP_UINT, "100000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(cache), "set cache mode\n"
	"- auto: cache to disk if content length is known, no cache otherwise\n"
	"- disk: cache to disk,  discard once session is no longer used\n"
	"- keep: cache to disk and keep\n"
	"- mem: stores to memory, discard once session is no longer used\n"
	"- mem_keep: stores to memory, keep after session is reassigned but move to `mem` after first download\n"
	"- none: no cache\n"
	"- none_keep: stores to memory, keep after session is reassigned but move to `none` after first download"
	, GF_PROP_UINT, "none", "auto|disk|keep|mem|mem_keep|none|none_keep", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(range), "set byte range, as fraction", GF_PROP_FRACTION64, "0-0", NULL, 0},
	{ OFFS(ext), "override file extension", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(mime), "set file mime type", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(blockio), "use blocking IO", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};

static const GF_FilterCapability HTTPInCaps[] =
{
	CAP_UINT(GF_CAPS_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};

GF_FilterRegister HTTPInRegister = {
	.name = "httpin",
	GF_FS_SET_DESCRIPTION("HTTP input")
	GF_FS_SET_HELP("This filter dispatch raw blocks from a remote HTTP resource into a filter chain.\n"
	"Block size can be adjusted using [-block_size](), and disk caching policies can be adjusted.\n"
	"Content format can be forced through [-mime]() and file extension can be changed through [-ext]().\n"
	"\n"
	"The filter supports both http and https schemes, and will attempt reconnecting as TLS if TCP connection fails.\n"
	"\n"
	"Note: Unless disabled at session level (see [-no-probe](CORE) ), file extensions are usually ignored and format probing is done on the first data block.")
	.private_size = sizeof(GF_HTTPInCtx),
#ifdef GPAC_CONFIG_EMSCRIPTEN
	.flags = GF_FS_REG_MAIN_THREAD,
#endif
	.args = HTTPInArgs,
	SETCAPS(HTTPInCaps),
	.initialize = httpin_initialize,
	.finalize = httpin_finalize,
	.process = httpin_process,
	.process_event = httpin_process_event,
	.probe_url = httpin_probe_url
};


const GF_FilterRegister *httpin_register(GF_FilterSession *session)
{
	if (gf_opts_get_bool("temp", "get_proto_schemes")) {
		gf_opts_set_key("temp_in_proto", HTTPInRegister.name, "http,https,gmem");
	}
	return &HTTPInRegister;
}
#else
const GF_FilterRegister *httpin_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_USE_DOWNLOADER

