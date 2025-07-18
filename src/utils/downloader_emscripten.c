/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2025
 *					All rights reserved
 *
 *  This file is part of GPAC / downloader sub-project
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

#include <gpac/tools.h>
#if defined(GPAC_CONFIG_EMSCRIPTEN)
#include <gpac/download.h>
#include <gpac/list.h>
#include <gpac/thread.h>
#include <gpac/filters.h>

struct __gf_download_manager
{
	GF_FilterSession *fsess;
	GF_List *all_sessions;
	GF_Mutex *mx;
};

GF_DownloadManager *gf_dm_new(GF_DownloadFilterSession *fsess)
{
	GF_DownloadManager *tmp;
	GF_SAFEALLOC(tmp, GF_DownloadManager);
	if (!tmp) return NULL;
	tmp->fsess = fsess;
	tmp->all_sessions = gf_list_new();
	tmp->mx = gf_mx_new("Downloader");
	return tmp;
}
void gf_dm_del(GF_DownloadManager *dm)
{
	if (!dm) return;
	gf_list_del(dm->all_sessions);
	gf_mx_del(dm->mx);
	gf_free(dm);
}

u32 gf_dm_get_global_rate(GF_DownloadManager *dm)
{
	return 0;
}
void gf_dm_set_data_rate(GF_DownloadManager *dm, u32 rate_in_bits_per_sec)
{
}

typedef struct __cache_blob
{
	GF_Blob blob;
	char *url, *cache_name;
	char *mime;
	u64 start_range, end_range;
	Bool persistent;
} GF_CacheBlob;

struct __gf_download_session
{
	GF_DownloadManager *dm;
	gf_dm_user_io user_io;
	void *usr_cbk;
	char method[100];

	char *req_url;
	char *rsp_url;
	/*request headers*/
	char **req_hdrs;
	u32 nb_req_hdrs;
	u32 req_body_size;
	char *req_body;
	u64 start_range, end_range;

	/*response headers*/
	char **rsp_hdrs;
	u32 nb_rsp_hdrs;
	char *mime; //pointer to content-type header or blob

	u32 state, bps;
	u64 start_time, start_time_utc;
	u64 total_size, bytes_done;
	GF_Err last_error;
	GF_NetIOStatus netio_status;
	u32 dl_flags;
	Bool reuse_cache;
	u32 ftask;
	GF_List *cached_blobs;

	GF_CacheBlob *active_cache;

	Bool destroy, in_callback;

};

EM_JS(int, dm_fetch_cancel, (int sess), {
  let fetcher = libgpac._get_fetcher(sess);
  if (!fetcher) return -1;
  fetcher._controller.abort();
  libgpac._del_fetcher(fetcher);
  return 0;
});

static void clear_headers(char ***_hdrs, u32 *nb_hdrs)
{
	char **hdrs = *_hdrs;
	if (hdrs) {
		u32 i=0;
		for (i=0; i<*nb_hdrs; i++) {
			if (hdrs[i]) gf_free(hdrs[i]);
		}
		gf_free(hdrs);
		*_hdrs = NULL;
		*nb_hdrs = 0;
	}
}

void cache_blob_del(GF_CacheBlob *b)
{
	if (!b) return;
	GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[Downloader] Removing cache entry of %s (%s - range "LLU"-"LLU")\n", b->url, b->cache_name, b->start_range, b->end_range));
	gf_blob_unregister(&b->blob);
	if (b->blob.data) gf_free(b->blob.data);
	if (b->url) gf_free(b->url);
	if (b->cache_name) gf_free(b->cache_name);
	if (b->mime) gf_free(b->mime);
	gf_free(b);
}

static void gf_dm_sess_reset(GF_DownloadSession *sess)
{
	if (!sess) return;
	clear_headers(&sess->req_hdrs, &sess->nb_req_hdrs);
	clear_headers(&sess->rsp_hdrs, &sess->nb_rsp_hdrs);
	if (sess->req_body) gf_free(sess->req_body);
	sess->req_body = NULL;

	if (sess->state==1) {
		dm_fetch_cancel(EM_CAST_PTR sess);
	}
	if (sess->req_url) gf_free(sess->req_url);
	sess->req_url = NULL;
	if (sess->rsp_url) gf_free(sess->rsp_url);
	sess->rsp_url = NULL;
	sess->mime = NULL;
	sess->state = 0;
	sess->total_size = 0;
	sess->bytes_done = 0;
	sess->start_time = 0;
	sess->start_range = 0;
	sess->end_range = 0;
	sess->bps = 0;
	sess->last_error = GF_OK;
	sess->reuse_cache = GF_FALSE;
	sess->netio_status = GF_NETIO_SETUP;
}


EM_JS(int, fs_fetch_setup, (), {
	if ((typeof libgpac.gpac_fetch == 'boolean') && !libgpac.gpac_fetch) return 2;
	if (typeof libgpac._fetcher_set_header == 'function') return 1;
	try {
		libgpac._fetchers = [];
		libgpac._get_fetcher = (sess) => {
          for (let i=0; i<libgpac._fetchers.length; i++) {
            if (libgpac._fetchers[i].sess==sess) return libgpac._fetchers[i];
          }
          return null;
		};
		libgpac._del_fetcher = (fetcher) => {
          let i = libgpac._fetchers.indexOf(fetcher);
          if (i>=0) libgpac._fetchers.splice(i, 1);
		};
        libgpac._fetcher_set_header = cwrap('gf_dm_sess_push_header', null, ['number', 'string', 'string']);
        libgpac._fetcher_set_reply = cwrap('gf_dm_sess_async_reply', null, ['number', 'number', 'string']);
	} catch (e) {
		return 0;
	}
	return 1;
});

static GF_Err gf_dm_setup_cache(GF_DownloadSession *sess)
{
	if (sess->active_cache) {
		if (!sess->cached_blobs) {
			cache_blob_del(sess->active_cache);
		}
		else if (sess->active_cache->blob.flags & GF_BLOB_CORRUPTED) {
			gf_list_del_item(sess->cached_blobs, sess->active_cache);
			cache_blob_del(sess->active_cache);
		}
		//remove if not persistent
		else if (!sess->active_cache->persistent) {
			gf_list_del_item(sess->cached_blobs, sess->active_cache);
			cache_blob_del(sess->active_cache);
		}
		sess->active_cache = NULL;
	}
	//look in session for cache
	if (sess->cached_blobs) {
		u32 i, count = gf_list_count(sess->cached_blobs);
		for (i=0; i<count; i++) {
			GF_CacheBlob *cb = gf_list_get(sess->cached_blobs, i);
			if (strcmp(cb->url, sess->req_url)) continue;
			if (cb->start_range != sess->start_range) continue;
			if (cb->end_range != sess->end_range) continue;
			if (!cb->blob.size) continue;
			sess->active_cache = cb;
			sess->reuse_cache = GF_TRUE;
			sess->total_size = cb->blob.size;
			sess->bytes_done = 0;
			sess->netio_status = GF_NETIO_DATA_EXCHANGE;
			sess->last_error = GF_OK;
			sess->mime = cb->mime;
			sess->dl_flags &= ~GF_NETIO_SESSION_KEEP_FIRST_CACHE;
			return GF_OK;
		}
	}
	if (!(sess->dl_flags & GF_NETIO_SESSION_MEMORY_CACHE)) {
		return GF_OK;
	}

	GF_SAFEALLOC(sess->active_cache, GF_CacheBlob);
	if (!sess->active_cache) return GF_OUT_OF_MEM;
	if (!sess->cached_blobs) sess->cached_blobs = gf_list_new();

	gf_list_add(sess->cached_blobs, sess->active_cache);
	sess->active_cache->cache_name = gf_blob_register(&sess->active_cache->blob);

	sess->active_cache->blob.flags = GF_BLOB_IN_TRANSFER;
	sess->active_cache->url = gf_strdup(sess->req_url);
	sess->active_cache->start_range = sess->start_range;
	sess->active_cache->end_range = sess->end_range;
	sess->reuse_cache = GF_FALSE;
	//only files marked with GF_NETIO_SESSION_KEEP_FIRST_CACHE are kept in our local cache
	//all other cache settings are ignored, we rely on browser cache for this
	if (sess->dl_flags & GF_NETIO_SESSION_KEEP_FIRST_CACHE) {
		sess->active_cache->persistent = GF_TRUE;
		sess->dl_flags &= ~GF_NETIO_SESSION_KEEP_FIRST_CACHE;
	} else {
		sess->active_cache->persistent = GF_FALSE;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[Downloader] Setting up memory cache for %s - persistent %d - name %s\n", sess->req_url, sess->active_cache->persistent, sess->active_cache->cache_name));
	return GF_OK;
}

GF_EXPORT
GF_DownloadSession *gf_dm_sess_new(GF_DownloadManager *dm, const char *url, u32 dl_flags,
                                   gf_dm_user_io user_io,
                                   void *usr_cbk,
                                   GF_Err *e)
{
	GF_NETIO_Parameter par;
	GF_DownloadSession *sess;

	int fetch_init = fs_fetch_setup();
	if (fetch_init==2) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[Downloader] fecth() disabled\n"));
		if (e) *e = GF_NOT_SUPPORTED;
		return NULL;
	} else if (fetch_init==0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[Downloader] Failed to initialize fetch\n"));
		if (e) *e = GF_SERVICE_ERROR;
		return NULL;
	}

	GF_SAFEALLOC(sess, GF_DownloadSession);
	if (!sess) {
		if (e) *e = GF_OUT_OF_MEM;
		return NULL;
	}
	sess->dm = dm;
	sess->req_url = gf_strdup(url);
	if (!sess->req_url) {
		gf_free(sess);
		if (e) *e = GF_OUT_OF_MEM;
		return NULL;
	}
	sess->user_io = user_io;
	sess->usr_cbk = usr_cbk;

	strcpy(sess->method, "GET");

	sess->dl_flags = dl_flags;

	if (dl_flags & GF_NETIO_SESSION_MEMORY_CACHE) {
		sess->cached_blobs = gf_list_new();
	}

	if (user_io) {
		memset(&par, 0, sizeof(GF_NETIO_Parameter));
		par.msg_type = GF_NETIO_GET_METHOD;
		sess->user_io(sess->usr_cbk, &par);
		if (par.name) strcpy(sess->method, par.name);

		sess->nb_req_hdrs=0;
		while (1) {
			par.msg_type = GF_NETIO_GET_HEADER;
			par.value = NULL;
			sess->user_io(sess->usr_cbk, &par);
			if (!par.value) break;

			sess->req_hdrs = gf_realloc(sess->req_hdrs, sizeof(char*) * (sess->nb_req_hdrs+2));
			if (!sess->req_hdrs) {
				if (e) *e = GF_OUT_OF_MEM;
				gf_free(sess);
				return NULL;
			}
			sess->req_hdrs[sess->nb_req_hdrs] = gf_strdup(par.name);
			sess->req_hdrs[sess->nb_req_hdrs+1] = gf_strdup(par.value);
			sess->nb_req_hdrs+=2;
		}

		memset(&par, 0, sizeof(GF_NETIO_Parameter));
		par.msg_type = GF_NETIO_GET_CONTENT;
		sess->user_io(sess->usr_cbk, &par);

		sess->req_body_size = par.size;
		if (par.size && par.data) {
			sess->req_body = gf_malloc(sizeof(u8)*par.size);
			if (!sess->req_body) {
				gf_dm_sess_reset(sess);
				gf_list_del(sess->cached_blobs);
				if (e) *e = GF_OUT_OF_MEM;
				gf_free(sess);
				return NULL;
			}
			memcpy(sess->req_body, par.data, par.size);
		}
	}
	if (e) *e = GF_OK;
	*e = gf_dm_setup_cache(sess);
	if (*e) {
		gf_dm_sess_del(sess);
		return NULL;
	}
	if (dm) {
		gf_mx_p(dm->mx);
		gf_list_add(dm->all_sessions, sess);
		gf_mx_v(dm->mx);
	}

	sess->user_io = user_io;
	sess->usr_cbk = usr_cbk;

	return sess;
}

void gf_dm_sess_abort(GF_DownloadSession * sess)
{
	if (sess->state==1) {
		dm_fetch_cancel(EM_CAST_PTR sess);
		sess->state = 0;
		sess->netio_status = GF_NETIO_DISCONNECTED;
		sess->last_error = GF_IP_CONNECTION_CLOSED;
		if (sess->active_cache && !sess->reuse_cache && (sess->active_cache->blob.flags & GF_BLOB_IN_TRANSFER)) {
			sess->active_cache->blob.flags = GF_BLOB_CORRUPTED;
		}
	}
	clear_headers(&sess->rsp_hdrs, &sess->nb_rsp_hdrs);
	if (sess->ftask) sess->ftask = 2;
}

void gf_dm_sess_del(GF_DownloadSession *sess)
{
	if (sess->in_callback) {
		sess->destroy = GF_TRUE;
		return;
	}
	gf_dm_sess_reset(sess);
	while (gf_list_count(sess->cached_blobs)) {
		cache_blob_del( gf_list_pop_back(sess->cached_blobs) );
	}
	gf_list_del(sess->cached_blobs);
	if (sess->dm) {
		gf_mx_p(sess->dm->mx);
		gf_list_del_item(sess->dm->all_sessions, sess);
		gf_mx_v(sess->dm->mx);
	}
	gf_free(sess);
}

const char *gf_dm_sess_get_cache_name(GF_DownloadSession *sess)
{
	if (!sess || !sess->active_cache) return NULL;
	return sess->active_cache->cache_name;
}

void gf_dm_sess_force_memory_mode(GF_DownloadSession *sess, u32 force_keep)
{
	if (!sess) return;
	sess->dl_flags |= GF_NETIO_SESSION_MEMORY_CACHE;
	if (force_keep==1) {
		//cache is handled by the browser if any, or forced by fetch headers provied by users
		//sess->dl_flags |= GF_NETIO_SESSION_KEEP_CACHE;
	} else if (force_keep==2) {
		sess->dl_flags |= GF_NETIO_SESSION_KEEP_FIRST_CACHE;
	}
}

GF_Err gf_dm_sess_setup_from_url(GF_DownloadSession *sess, const char *url, Bool allow_direct_reuse)
{
	gf_dm_sess_reset(sess);
	sess->req_url = gf_strdup(url);
	if (!sess->req_url) {
		sess->state=2;
		return GF_OUT_OF_MEM;
	}
	return gf_dm_setup_cache(sess);
}

GF_Err gf_dm_sess_set_range(GF_DownloadSession *sess, u64 start_range, u64 end_range, Bool discontinue_cache)
{
	if (!sess) return GF_BAD_PARAM;
	if (sess->state==1) return GF_BAD_PARAM;
	sess->start_range = start_range;
	sess->end_range = end_range;
	Bool cache_reset = GF_FALSE;
	if ((sess->state!=2) || discontinue_cache) {
		cache_reset = GF_TRUE;
	}
	else if (sess->active_cache) {
		if (sess->active_cache->start_range + sess->active_cache->end_range + 1 == start_range) {
			sess->active_cache->end_range = end_range;
			sess->active_cache->blob.flags |= GF_BLOB_IN_TRANSFER;
		} else {
			cache_reset = GF_TRUE;
		}
	}
	//set range called before starting session, just adjust cache
	if (sess->active_cache && cache_reset && !sess->state) {
		sess->active_cache->start_range = sess->start_range;
		sess->active_cache->end_range = sess->end_range;
		cache_reset=GF_FALSE;
	}
	if (cache_reset) {
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[Downloader] Cache discontinuity, reset\n", sess->req_url, sess->active_cache->persistent, sess->active_cache->cache_name));
		GF_Err e = gf_dm_setup_cache(sess);
		if (e) return e;
		sess->last_error = GF_OK;
	}
	sess->netio_status = GF_NETIO_CONNECTED;
	sess->state = 0;
	return GF_OK;
}



EM_JS(int, dm_fetch_init, (int sess, int _url, int _method, int _headers, int nb_headers, int req_body, int req_body_size), {
  let url = _url ? UTF8ToString(_url) : null;
  let ret = 0; // libgpac.OK

  let fetcher = libgpac._get_fetcher(sess);
  if (!fetcher) {
	fetcher = {};
	fetcher.sess = sess;
	libgpac._fetchers.push(fetcher);
  }

  fetcher._controller = new AbortController();
  let options = {
	signal: fetcher._controller.signal,
	method: _method ? UTF8ToString(_method) : "GET",
	mode: libgpac.gpac_fetch_mode || 'cors',
  };
  let mime_type = 'application/octet-stream';
  options.headers = {};
  if (_headers) {
	for (let i=0; i<nb_headers; i+=2) {
	  let _s = getValue(_headers+i*4, 'i32');
	  let h_name = _s ? UTF8ToString(_s).toLowerCase() : "";
	  _s = getValue(_headers+(i+1)*4, 'i32');
	  let h_val = _s ? UTF8ToString(_s) : "";
	  if (h_name.length && h_val.length) {
		  options.headers[h_name] = h_val;
		  if (h_name=='content-type')
			mime_type = h_val;
	  }
	}
  }
  if (typeof libgpac.gpac_extra_headers == 'object') {
	for (const hdr in libgpac.gpac_extra_headers) {
	  options.headers[hdr] = object[hdr];
	}
  }

  if (req_body) {
	let body_ab = new Uint8Array(HEAPU8.buffer, req_body, req_body_size);
	options.body = new Blob(body_ab, {type: mime_type} );
  }
  fetcher._state = 0;
  fetch(url, options).then( (response) => {
	if (response.ok) {
	  fetcher._state = 1;
	  fetcher._bytes = 0;
	  fetcher._reader = response.body.getReader();

	  let final_url = null;
	  if (response.redirected) final_url = response.url;
	  libgpac._fetcher_set_reply(fetcher.sess, response.status, final_url);

	  response.headers.forEach((value, key) => {
		libgpac._fetcher_set_header(fetcher.sess, key, value);
	  });
      libgpac._fetcher_set_header(fetcher.sess, 0, 0);
	} else {
	  libgpac._fetcher_set_reply(fetcher.sess, response.status, null);
	  fetcher._state = 3;
	}
  })
  .catch( (e) => {
	  if (e.name !== 'AbortError') printErr('fetcher exception ' + e);
	  ret = -14; // libgpac.REMOTE_SERVICE_ERROR
	  libgpac._del_fetcher(fetcher);
  });
  return ret;
});


EM_JS(int, dm_fetch_data, (int sess, int buffer, int buffer_size, int read_size), {
  let f = libgpac._get_fetcher(sess);
  if (!f) return -1;
  if (f._state==0) return -44;
  if (f._state==3) return -12;
  if (f._state==4) return 1;
  if (f._state == 1) {
	f._state = 0;
	f._reader.read().then( (block) => {
		if (block.done) {
			f._state = 4;
		} else {
			f._ab = block.value;
			f._bytes += f._ab.byteLength;
			f._block_pos = 0;
			f._state = 2;
		}
	})
	.catch( e => {
		f._state = 0;
	});
	if (f._state==0) return -44;
  }

  let avail = f._ab.byteLength - f._block_pos;
  if (avail<buffer_size) buffer_size = avail;

  //copy array buffer
  let src = f._ab.subarray(f._block_pos, f._block_pos+buffer_size);
  let dst = new Uint8Array(HEAPU8.buffer, buffer, buffer_size);
  dst.set(src);

  setValue(read_size, buffer_size, 'i32');
  f._block_pos += buffer_size;
  if (f._ab.byteLength == f._block_pos) {
	  f._ab = null;
	  f._state = 1;
  }
  return 0;
});


GF_Err gf_dm_sess_fetch_data(GF_DownloadSession *sess, char *buffer, u32 buffer_size, u32 *read_size)
{
	if (sess->reuse_cache) {
		u32 remain = sess->active_cache->blob.size - sess->bytes_done;
		if (!remain) {
			*read_size = 0;
			sess->last_error = GF_EOS;
			sess->netio_status = GF_NETIO_DATA_TRANSFERED;
			return GF_EOS;
		}
		if (remain < buffer_size) buffer_size = remain;
		memcpy(buffer, sess->active_cache->blob.data + sess->bytes_done, buffer_size);
		*read_size = buffer_size;
		sess->bytes_done += buffer_size;
		return GF_OK;
	}
	if (sess->state == 0) {
		if (sess->start_range || sess->end_range) {
			char szHdr[100];
			if (sess->end_range)
				sprintf(szHdr, "bytes="LLU"-"LLU, sess->start_range, sess->end_range);
			else
				sprintf(szHdr, "bytes="LLU"-", sess->start_range);

			sess->req_hdrs = gf_realloc(sess->req_hdrs, sizeof(char*) * (sess->nb_req_hdrs+2));
			if (!sess->req_hdrs) {
				sess->state = 2;
				return sess->last_error = GF_OUT_OF_MEM;
			}
			sess->req_hdrs[sess->nb_req_hdrs] = gf_strdup("Range");
			sess->req_hdrs[sess->nb_req_hdrs+1] = gf_strdup(szHdr);
			sess->nb_req_hdrs+=2;
		}

		sess->start_time_utc = gf_net_get_utc();
		sess->start_time = gf_sys_clock();
		sess->netio_status = GF_NETIO_CONNECTED;

		if (sess->start_range || sess->end_range) {
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[Downloader] Starting download of %s (%s - range "LLU"-"LLU")\n", sess->req_url, sess->active_cache ? sess->active_cache->cache_name : "no cache", sess->start_range, sess->end_range));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[Downloader] Starting download of %s (%s)\n", sess->req_url, sess->active_cache ? sess->active_cache->cache_name : "no cache"));
		}
		GF_Err fetch_e = dm_fetch_init(
			EM_CAST_PTR sess,
			EM_CAST_PTR sess->req_url,
			EM_CAST_PTR sess->method,
			EM_CAST_PTR sess->req_hdrs,
			sess->nb_req_hdrs,
			EM_CAST_PTR sess->req_body,
			sess->req_body_size
		);

		if (fetch_e) {
			gf_dm_sess_reset(sess);
			sess->state = 2;
			sess->netio_status = GF_NETIO_DISCONNECTED;
			return sess->last_error = fetch_e;
		}
		sess->state = 1;
		sess->netio_status = GF_NETIO_WAIT_FOR_REPLY;
	}
	if (sess->state==2) return sess->last_error;

	*read_size = 0;
	GF_Err fetch_err = dm_fetch_data(
		EM_CAST_PTR sess,
		EM_CAST_PTR buffer,
		buffer_size,
		EM_CAST_PTR read_size
	);

	if (*read_size) {
		u64 ellapsed = gf_sys_clock() - sess->start_time;
		sess->bytes_done += *read_size;
		if (ellapsed)
			sess->bps = (u32) (sess->bytes_done * 1000 / ellapsed);
		sess->netio_status = GF_NETIO_DATA_EXCHANGE;

		if (sess->active_cache) {
			u32 nb_bytes= *read_size;
			sess->active_cache->blob.data = gf_realloc(sess->active_cache->blob.data, sess->active_cache->blob.size + nb_bytes);
			if (!sess->active_cache->blob.data) {
				sess->active_cache->blob.flags |= GF_BLOB_CORRUPTED;
			} else {
				memcpy(sess->active_cache->blob.data + sess->active_cache->blob.size, buffer, nb_bytes);
				sess->active_cache->blob.size += nb_bytes;
			}
		}
		if (sess->total_size == sess->bytes_done) fetch_err = GF_EOS;

	} else if (!fetch_err) {
		return GF_IP_NETWORK_EMPTY;
	} else if (fetch_err!=GF_IP_NETWORK_EMPTY) {
		sess->state=2;
		sess->last_error = fetch_err;
		sess->netio_status = GF_NETIO_STATE_ERROR;
		if (sess->active_cache)
			sess->active_cache->blob.flags = GF_BLOB_CORRUPTED;
	}

	if (fetch_err==GF_EOS) {
		sess->total_size = sess->bytes_done;
		sess->state=2;
		sess->last_error = GF_EOS;
		sess->netio_status = GF_NETIO_DATA_TRANSFERED;
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[Downloader] Done downloading %s - rate %d bps\n", sess->req_url, sess->bps));
		if (sess->active_cache)
			sess->active_cache->blob.flags &= ~GF_BLOB_IN_TRANSFER;
	}
	return fetch_err;
}


GF_Err gf_dm_sess_get_stats(GF_DownloadSession * sess, const char **server, const char **path, u64 *total_size, u64 *bytes_done, u32 *bytes_per_sec, GF_NetIOStatus *net_status)
{
	if (!sess) return GF_OUT_OF_MEM;
	if (total_size) *total_size = sess->total_size;
	if (bytes_done) *bytes_done = sess->bytes_done;
	if (bytes_per_sec) *bytes_per_sec = sess->bps;
	if (net_status) *net_status = sess->netio_status;
	return sess->last_error;
}

const char *gf_dm_sess_mime_type(GF_DownloadSession * sess)
{
	return sess->mime;
}

GF_Err gf_dm_sess_enum_headers(GF_DownloadSession *sess, u32 *idx, const char **hdr_name, const char **hdr_val)
{
	if( !sess || !idx || !hdr_name || !hdr_val)
		return GF_BAD_PARAM;
	if (*idx >= sess->nb_rsp_hdrs/2) return GF_EOS;

	u32 i = *idx * 2;
	(*hdr_name) = sess->rsp_hdrs[i];
	(*hdr_val) = sess->rsp_hdrs[i+1];
	(*idx) = (*idx) + 1;
	return GF_OK;
}

void gf_dm_delete_cached_file_entry_session(const GF_DownloadSession * sess, const char * url, Bool force)
{
	//unused we automatically purge cache when setting up session
	if (!sess || !force) return;
	if (sess->state==1) return;
	if (sess->active_cache) {
		gf_list_del_item(sess->cached_blobs, sess->active_cache);
		cache_blob_del(sess->active_cache);
		((GF_DownloadSession *)sess)->active_cache = NULL;
	}
}

GF_Err gf_dm_sess_process_headers(GF_DownloadSession *sess)
{
	//no control of this for fetch
	return GF_OK;
}

GF_EXPORT
void gf_dm_sess_push_header(GF_DownloadSession *sess, const char *hdr, const char *value)
{
	if (!sess) return;
	if (!hdr || !value) return;

	if (!stricmp(hdr, "Content-Length")) {
		sscanf(value, LLU, &sess->total_size);
	}

	sess->rsp_hdrs = gf_realloc(sess->rsp_hdrs, sizeof(char*) * (sess->nb_rsp_hdrs+2));
	if (!sess->rsp_hdrs) return;

	sess->rsp_hdrs[sess->nb_rsp_hdrs] = gf_strdup(hdr);
	sess->rsp_hdrs[sess->nb_rsp_hdrs+1] = gf_strdup(value);
	if (!stricmp(hdr, "Content-Type")) {
		sess->mime = sess->rsp_hdrs[sess->nb_rsp_hdrs+1];
		//keep a copy of mime
		if (sess->active_cache && sess->active_cache->persistent)
			sess->active_cache->mime = gf_strdup(sess->mime);
	}
	sess->nb_rsp_hdrs+=2;
}

GF_EXPORT
void gf_dm_sess_async_reply(GF_DownloadSession *sess, int rsp_code, const char *final_url)
{
	if (!sess) return;
	GF_Err e;
	switch (rsp_code) {
	case 404: e = GF_URL_ERROR; break;
	case 400: e = GF_SERVICE_ERROR; break;
	case 401: e = GF_AUTHENTICATION_FAILURE; break;
	case 405: e = GF_AUTHENTICATION_FAILURE; break;
	case 416: e = GF_SERVICE_ERROR; break; //range error
	default:
		if (rsp_code>=500) e = GF_REMOTE_SERVICE_ERROR;
		else if (rsp_code>=400) e = GF_SERVICE_ERROR;
		else e = GF_OK;
		break;
	}
	sess->last_error = e;

	if (final_url) {
		if (sess->rsp_url) gf_free(sess->rsp_url);
		sess->rsp_url = final_url ? gf_strdup(final_url) : NULL;
	}
}

GF_Err gf_dm_sess_fetch_once(GF_DownloadSession *sess)
{
	GF_NETIO_Parameter par;
	char buffer[10000];
	u32 read = 0;
	GF_Err e = gf_dm_sess_fetch_data(sess, buffer, 10000, &read);
	if (e==GF_IP_NETWORK_EMPTY) {

	} else if ((e==GF_OK) || (e==GF_EOS)) {
		if (sess->user_io && read) {
			memset(&par, 0, sizeof(GF_NETIO_Parameter));
			par.sess = sess;
			par.error = sess->last_error;
			if (e) {
				par.msg_type = GF_NETIO_DATA_TRANSFERED;
			} else {
				par.msg_type = GF_NETIO_DATA_EXCHANGE;
				par.data = buffer;
				par.size = read;
			}
			sess->in_callback = GF_FALSE;
			sess->user_io(sess->usr_cbk, &par);
			sess->in_callback = GF_FALSE;
		}
	} else {
		if (sess->user_io) {
			memset(&par, 0, sizeof(GF_NETIO_Parameter));
			par.sess = sess;
			par.msg_type = sess->netio_status;
			par.error = sess->last_error;
			sess->in_callback = GF_TRUE;
			sess->user_io(sess->usr_cbk, &par);
			sess->in_callback = GF_FALSE;
		}
	}
	return e;
}

Bool gf_dm_session_task(GF_FilterSession *fsess, void *callback, u32 *reschedule_ms)
{
	Bool ret = GF_TRUE;
	GF_DownloadSession *sess = callback;
	if (!sess) return GF_FALSE;
	gf_assert(sess->ftask);
	if (sess->ftask==2) {
		sess->ftask = 0;
		return GF_FALSE;
	}

	GF_Err e = gf_dm_sess_fetch_once(sess);
	if (e==GF_EOS) ret = GF_FALSE;
	else if (e && (e!=GF_IP_NETWORK_EMPTY)) ret = GF_FALSE;

	if (ret) {
		*reschedule_ms = 1;
		return GF_TRUE;
	}
	sess->ftask = 0;
	if (sess->destroy) {
		sess->destroy = GF_FALSE;
		gf_dm_sess_del(sess);
	}
	return GF_FALSE;
}

GF_Err gf_dm_sess_process(GF_DownloadSession *sess)
{
	if (sess->netio_status == GF_NETIO_DATA_TRANSFERED) {
		return GF_OK;
	}

	if ((sess->dl_flags & GF_NETIO_SESSION_NOT_THREADED) || !sess->dm->fsess) {
		return gf_dm_sess_fetch_once(sess);
	}
	//ignore if task already allocated
	if (sess->ftask) return GF_OK;
	sess->ftask = GF_TRUE;
	return gf_fs_post_user_task(sess->dm->fsess, gf_dm_session_task, sess, "download");
}


const char *gf_dm_sess_get_resource_name(GF_DownloadSession *sess)
{
	if (!sess) return NULL;
	return sess->rsp_url ? sess->rsp_url : sess->req_url;
}

const char *gf_dm_sess_get_header(GF_DownloadSession *sess, const char *name)
{
	u32 i;
	if (!sess || !name) return NULL;
	for (i=0; i<sess->nb_rsp_hdrs; i+=2) {
		char *h = sess->rsp_hdrs[i];
		if (!stricmp(h, name)) return sess->rsp_hdrs[i+1];
	}
	return NULL;
}

u64 gf_dm_sess_get_utc_start(GF_DownloadSession * sess)
{
	if (!sess) return 0;
	return sess->start_time_utc;
}

//only used for dash playing local files
u32 gf_dm_get_data_rate(GF_DownloadManager *dm)
{
	return gf_opts_get_int("core", "maxrate");
}

void gf_dm_sess_detach_async(GF_DownloadSession *sess)
{
	if (!sess) return;
	sess->dl_flags &= ~GF_NETIO_SESSION_NOT_THREADED;
	gf_dm_sess_force_memory_mode(sess, 1);
}
void gf_dm_sess_set_netcap_id(GF_DownloadSession *sess, const char *netcap_id)
{

}

void gf_dm_sess_set_max_rate(GF_DownloadSession *sess, u32 max_rate)
{
}

u32 gf_dm_sess_get_max_rate(GF_DownloadSession *sess)
{
	return 0;
}
Bool gf_dm_sess_is_regulated(GF_DownloadSession *sess)
{
	return GF_FALSE;
}
u32 gf_dm_sess_get_resource_size(GF_DownloadSession * sess)
{
	return sess ? sess->total_size : 0;
}


#endif // GPAC_CONFIG_EMSCRIPTEN
