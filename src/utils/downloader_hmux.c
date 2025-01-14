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

#include "downloader.h"

#ifndef GPAC_DISABLE_NETWORK

#ifdef GPAC_HTTPMUX

//detach session from HTTP2 session - the session mutex SHALL be grabbed before calling this
void hmux_detach_session(GF_HMUX_Session *hmux_sess, GF_DownloadSession *sess)
{
	if (!hmux_sess || !sess) return;
	gf_assert(sess->hmux_sess == hmux_sess);
	gf_assert(sess->mx);

	gf_list_del_item(hmux_sess->sessions, sess);
	if (!gf_list_count(hmux_sess->sessions)) {
		dm_sess_sk_del(sess);

#ifdef GPAC_HAS_SSL
		if (sess->ssl) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[Downloader] shut down SSL context\n"));
			SSL_shutdown(sess->ssl);
			SSL_free(sess->ssl);
			sess->ssl = NULL;
		}
#endif

		if (hmux_sess->destroy) hmux_sess->destroy(hmux_sess);

		gf_list_del(hmux_sess->sessions);
		gf_mx_v(hmux_sess->mx);
		gf_mx_del(sess->mx);
		gf_free(hmux_sess);
	} else {
		GF_DownloadSession *asess = gf_list_get(hmux_sess->sessions, 0);
		gf_assert(asess->hmux_sess == hmux_sess);
		gf_assert(asess->sock);

		hmux_sess->net_sess = asess;
		//swap async buf if any to new active session
		if (sess->async_buf_alloc) {
			gf_assert(!asess->async_buf);
			asess->async_buf = sess->async_buf;
			asess->async_buf_alloc = sess->async_buf_alloc;
			asess->async_buf_size = sess->async_buf_size;
			sess->async_buf_alloc = sess->async_buf_size = 0;
			sess->async_buf = NULL;
		}
		sess->sock = NULL;
#ifdef GPAC_HAS_SSL
		sess->ssl = NULL;
#endif
		gf_mx_v(sess->mx);
	}

	if (sess->hmux_buf.data) {
		gf_free(sess->hmux_buf.data);
		memset(&sess->hmux_buf, 0, sizeof(hmux_reagg_buffer));
	}
	sess->hmux_sess = NULL;
	sess->mx = NULL;
}


GF_DownloadSession *hmux_get_session(void *user_data, s64 stream_id, Bool can_reassign)
{
	u32 i, nb_sess;
	GF_DownloadSession *first_not_assigned = NULL;
	GF_HMUX_Session *h2sess = (GF_HMUX_Session *)user_data;

	nb_sess = gf_list_count(h2sess->sessions);
	for (i=0;i<nb_sess; i++) {
		GF_DownloadSession *s = gf_list_get(h2sess->sessions, i);
		if (s->hmux_stream_id == stream_id)
			return s;

		if (s->server_mode && !s->hmux_stream_id && !first_not_assigned) {
			first_not_assigned = s;
		}
	}
	if (can_reassign && first_not_assigned) {
		first_not_assigned->hmux_stream_id = stream_id;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] reassigning old server session to new stream %d\n", stream_id));
		first_not_assigned->status = GF_NETIO_CONNECTED;
		first_not_assigned->total_size = first_not_assigned->bytes_done = 0;
		first_not_assigned->hmux_data_done = 0;
		if (first_not_assigned->remote_path) gf_free(first_not_assigned->remote_path);
		first_not_assigned->remote_path = NULL;
		//reset internal reaggregation buffer
		first_not_assigned->hmux_buf.size = 0;

		gf_dm_sess_clear_headers(first_not_assigned);
		return first_not_assigned;
	}
	return NULL;
}

void hmux_flush_send(GF_DownloadSession *sess, Bool flush_local_buf)
{
	char h2_flush[1024];
	u32 res;

	while (sess->hmux_send_data) {
		GF_Err e;
		u32 nb_bytes = sess->hmux_send_data_len;

		if (!sess->local_buf_len || flush_local_buf) {
			//read any frame pending from remote peer (window update and co)
			e = gf_dm_read_data(sess, h2_flush, 1023, &res);
			if ((e<0) && (e != GF_IP_NETWORK_EMPTY)) {
				sess->status = GF_NETIO_STATE_ERROR;
				SET_LAST_ERR(sess->last_error)
				break;
			}

			sess->hmux_sess->send(sess);

			//error or regular eos
			if (!sess->hmux_stream_id)
				break;
			if (sess->status==GF_NETIO_STATE_ERROR)
				break;
		}

		if (nb_bytes > sess->hmux_send_data_len) continue;
		if (!(sess->flags & GF_NETIO_SESSION_NO_BLOCK)) continue;

		if (flush_local_buf) return;

		//stream will now block, no choice but do a local copy...
		if (sess->local_buf_len + nb_bytes > sess->local_buf_alloc) {
			sess->local_buf_alloc = sess->local_buf_len + nb_bytes;
			sess->local_buf = gf_realloc(sess->local_buf, sess->local_buf_alloc);
			if (!sess->local_buf) {
				sess->status = GF_NETIO_STATE_ERROR;
				SET_LAST_ERR(sess->last_error)
				break;
			}
		}
		if (nb_bytes) {
			memcpy(sess->local_buf + sess->local_buf_len, sess->hmux_send_data, nb_bytes);
			sess->local_buf_len+=nb_bytes;
		}
		sess->hmux_send_data = NULL;
		sess->hmux_send_data_len = 0;
	}
}

void hmux_flush_internal_data(GF_DownloadSession *sess, Bool store_in_init)
{
	gf_dm_data_received(sess, (u8 *) sess->hmux_buf.data, sess->hmux_buf.size, store_in_init, NULL, NULL);
	sess->hmux_buf.size = 0;
}

void hmux_fetch_flush_data(GF_DownloadSession *sess, u8 *obuffer, u32 size, u32 *nb_bytes)
{
	u32 copy, nb_b_pck;
	u8 *data;
	*nb_bytes = 0;
	if (!sess->hmux_buf.size)
		return;

	gf_assert(sess->hmux_buf.offset<=sess->hmux_buf.size);

	nb_b_pck = sess->hmux_buf.size - sess->hmux_buf.offset;
	if (nb_b_pck > size)
		copy = size;
	else
		copy = nb_b_pck;

	data = sess->hmux_buf.data + sess->hmux_buf.offset;
	memcpy(obuffer, data, copy);
	*nb_bytes = copy;
	gf_dm_data_received(sess, (u8 *) data, copy, GF_FALSE, NULL, NULL);

	if (copy < nb_b_pck) {
		sess->hmux_buf.offset += copy;
	} else {
		sess->hmux_buf.size = sess->hmux_buf.offset = 0;
	}
	gf_assert(sess->hmux_buf.offset<=sess->hmux_buf.size);
}

GF_Err hmux_data_received(GF_DownloadSession *sess, const u8 *data, u32 nb_bytes)
{
	if (nb_bytes) {
		GF_Err e = sess->hmux_sess->data_received(sess, data, nb_bytes);
		if (e) return e;
	}
	/* send pending frames - hmux_sess may be NULL at this point if the connection was reset during processing of the above
		this typically happens if we have a refused stream
	*/
	if (sess->hmux_sess)
		sess->hmux_sess->send(sess);

	return GF_OK;
}

GF_Err hmux_async_flush(GF_DownloadSession *sess)
{
	if (!sess->local_buf_len && !sess->hmux_is_eos) return GF_OK;
	GF_Err ret = GF_OK;
	u8 was_eos = sess->hmux_is_eos;
	gf_assert(sess->hmux_send_data==NULL);
	sess->hmux_send_data = sess->local_buf;
	sess->hmux_send_data_len = sess->local_buf_len;
	gf_mx_p(sess->mx);
	if (sess->hmux_data_paused) {
		sess->hmux_data_paused = 0;
		sess->hmux_sess->resume(sess);
	}
	hmux_flush_send(sess, GF_TRUE);
	gf_mx_v(sess->mx);
	if (sess->hmux_send_data_len) {
		//we will return empty to avoid pushing data too fast, but we also need to flush the async buffer
		ret = GF_IP_NETWORK_EMPTY;
		if (sess->hmux_send_data_len<sess->local_buf_len) {
			memmove(sess->local_buf, sess->hmux_send_data, sess->hmux_send_data_len);
			sess->local_buf_len = sess->hmux_send_data_len;
		} else {
			//nothing sent, check socket
			ret = gf_sk_probe(sess->sock);
		}
		sess->hmux_send_data = NULL;
		sess->hmux_send_data_len = 0;
		sess->hmux_is_eos = was_eos;
	} else {
		sess->local_buf_len = 0;
		sess->hmux_send_data = NULL;
		sess->hmux_send_data_len = 0;
		if (was_eos && !sess->hmux_is_eos) {
			if (sess->put_state==1) {
				sess->put_state = 2;
				sess->status = GF_NETIO_WAIT_FOR_REPLY;
			}
		}
	}
	return ret;
}

GF_Err hmux_send_payload(GF_DownloadSession *sess, u8 *data, u32 size)
{
	GF_Err e;
	if (sess->hmux_send_data) {
		e = gf_sk_probe(sess->sock);
		if (e) return e;
		return GF_SERVICE_ERROR;
	}

	if (!sess->hmux_stream_id)
		return GF_URL_REMOVED;

	gf_mx_p(sess->mx);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] Sending %d bytes on stream_id %d\n", size, sess->hmux_stream_id));

	sess->hmux_send_data = data;
	sess->hmux_send_data_len = size;
	if (sess->hmux_data_paused) {
		sess->hmux_data_paused = 0;
		sess->hmux_sess->resume(sess);
	}
	//if no data, signal end of stream, otherwise regular send
	if (!data || !size) {
		if (sess->local_buf_len) {
			gf_mx_v(sess->mx);
			if (sess->hmux_is_eos) {
				return GF_IP_NETWORK_EMPTY;
			}
			sess->hmux_is_eos = GF_TRUE;
			sess->hmux_sess->send(sess);
			return GF_OK;
		}
		sess->hmux_is_eos = GF_TRUE;
		sess->hmux_sess->send(sess);
		//stream_id is not yet 0 in case of PUT/PUSH, stream is closed once we get reply from server
	} else {
		sess->hmux_is_eos = GF_FALSE;
		//send the data
		hmux_flush_send(sess, GF_FALSE);
	}

	gf_mx_v(sess->mx);

	if (!data || !size) {
		if (sess->put_state) {
			//we are done sending
			if (!sess->local_buf_len) {
				sess->put_state = 2;
				sess->status = GF_NETIO_WAIT_FOR_REPLY;
			}
			return GF_OK;
		}
	}
	if (!sess->hmux_stream_id && sess->hmux_send_data)
		return GF_URL_REMOVED;
	return GF_OK;
}

#endif //GPAC_HTTPMUX



void gf_dm_sess_close_hmux(GF_DownloadSession *sess)
{
#ifdef GPAC_HAS_HTTP2
	if (!sess->hmux_sess) return;
	nghttp2_submit_shutdown_notice(sess->hmux_sess->hmux_udta);
	sess->hmux_sess->send(sess);

	//commented out as it seems go_away is never set
#if 1
	u64 in_time;
	in_time = gf_sys_clock_high_res();
	while (nghttp2_session_want_read(sess->hmux_sess->hmux_udta)) {
		u32 res;
		char h2_flush[2024];
		GF_Err e;
		if (gf_sys_clock_high_res() - in_time > 100000)
			break;

		//read any frame pending from remote peer (window update and co)
		e = gf_dm_read_data(sess, h2_flush, 2023, &res);
		if ((e<0) && (e != GF_IP_NETWORK_EMPTY)) {
			if (e!=GF_IP_CONNECTION_CLOSED) {
				SET_LAST_ERR(e)
				sess->status = GF_NETIO_STATE_ERROR;
			}
			break;
		}

		sess->hmux_sess->send(sess);
	}
#else
	sess->status = GF_NETIO_DISCONNECTED;
#endif

#endif
}

GF_DownloadSession *gf_dm_sess_new_subsession(GF_DownloadSession *sess, s64 stream_id, void *usr_cbk, GF_Err *e)
{
#ifdef GPAC_HTTPMUX
	GF_DownloadSession *sub_sess;
	if (!sess->hmux_sess || !stream_id) return NULL;
	gf_mx_p(sess->mx);
	sub_sess = gf_dm_sess_new_internal(NULL, NULL, 0, sess->user_proc, usr_cbk, sess->sock, e);
	if (!sub_sess) {
		gf_mx_v(sess->mx);
		return NULL;
	}
	gf_list_add(sess->hmux_sess->sessions, sub_sess);
#ifdef GPAC_HAS_SSL
	sub_sess->ssl = sess->ssl;
#endif
	sub_sess->hmux_sess = sess->hmux_sess;
	if (sub_sess->mx) gf_mx_del(sub_sess->mx);
	sub_sess->mx = sess->hmux_sess->mx;
	sub_sess->hmux_stream_id = stream_id;
	sub_sess->status = GF_NETIO_CONNECTED;

	sub_sess->hmux_sess->setup_session(sub_sess, GF_FALSE);

	sub_sess->flags = sess->flags;
	gf_mx_v(sess->mx);
	return sub_sess;
#else
	return NULL;
#endif //GPAC_HTTPMUX
}



#endif //GPAC_DISABLE_NETWORK

