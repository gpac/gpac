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
		if (hmux_sess->close)
			hmux_sess->close(hmux_sess);

#ifdef GPAC_HAS_SSL
		if (sess->ssl) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[Downloader] shut down SSL context\n"));
			SSL_shutdown(sess->ssl);
			SSL_free(sess->ssl);
			sess->ssl = NULL;
		}
#endif
		dm_sess_sk_del(sess);
		if (hmux_sess->destroy)
			hmux_sess->destroy(hmux_sess);

		gf_list_del(hmux_sess->sessions);
		gf_mx_v(hmux_sess->mx);
		gf_mx_del(sess->mx);
		gf_free(hmux_sess);
	} else {
		GF_DownloadSession *asess = gf_list_get(hmux_sess->sessions, 0);
		gf_assert(asess->hmux_sess == hmux_sess);

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
	GF_HMUX_Session *hmux_sess = (GF_HMUX_Session *)user_data;

	nb_sess = gf_list_count(hmux_sess->sessions);
	for (i=0;i<nb_sess; i++) {
		GF_DownloadSession *s = gf_list_get(hmux_sess->sessions, i);
		if (s->hmux_stream_id == stream_id)
			return s;

		if (s->server_mode && (s->hmux_stream_id<0) && !first_not_assigned) {
			first_not_assigned = s;
		}
	}
	if (can_reassign && first_not_assigned) {
		first_not_assigned->hmux_stream_id = stream_id;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[%s] reassigning old server session to new stream %d\n", hmux_sess->net_sess->log_name, stream_id));
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

void hmux_flush_internal_data(GF_DownloadSession *sess, Bool store_in_init)
{
	gf_dm_data_received(sess, (u8 *) sess->hmux_buf.data, sess->hmux_buf.size, store_in_init, NULL, NULL);
	sess->hmux_buf.size = 0;
}

void hmux_fetch_data(GF_DownloadSession *sess, u8 *obuffer, u32 size, u32 *nb_bytes)
{
	u32 copy, nb_b_pck;
	u8 *data;
	*nb_bytes = 0;
	if (!sess->hmux_buf.size) {
		if (sess->hmux_is_eos) {
			if (!sess->total_size) {
				sess->total_size = sess->bytes_done;
				gf_dm_data_received(sess, obuffer, 0, GF_FALSE, NULL, NULL);
			}

			if (sess->status==GF_NETIO_DATA_EXCHANGE)
				sess->status = GF_NETIO_DATA_TRANSFERED;
		}
		return;
	}

	gf_assert(sess->hmux_buf.offset<=sess->hmux_buf.size);

	nb_b_pck = sess->hmux_buf.size - sess->hmux_buf.offset;
	if (nb_b_pck > size)
		copy = size;
	else
		copy = nb_b_pck;

	data = sess->hmux_buf.data + sess->hmux_buf.offset;
	memcpy(obuffer, data, copy);
	*nb_bytes = copy;
	//signal we received data (for event triggering / user callbacks)
	gf_dm_data_received(sess, (u8 *) data, copy, GF_FALSE, NULL, NULL);

	if (copy < nb_b_pck) {
		sess->hmux_buf.offset += copy;
	} else {
		sess->hmux_buf.size = sess->hmux_buf.offset = 0;
	}
	gf_assert(sess->hmux_buf.offset<=sess->hmux_buf.size);
}

GF_Err hmux_send_payload(GF_DownloadSession *sess, u8 *data, u32 size)
{
	GF_Err e = GF_OK;
	if (sess->hmux_send_data) {
		e = gf_sk_probe(sess->sock);
		if (e) return e;
		return GF_SERVICE_ERROR;
	}

	if (sess->hmux_stream_id<0)
		return GF_URL_REMOVED;

	gf_mx_p(sess->mx);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[%s] Sending %d bytes on stream_id "LLD"\n", sess->log_name, size, sess->hmux_stream_id));

	assert(sess->hmux_send_data==NULL);
	sess->hmux_send_data = data;
	sess->hmux_send_data_len = size;
	if (sess->hmux_data_paused) {
		e = sess->hmux_sess->resume(sess);
		if (e) {
			gf_mx_v(sess->mx);
			return e;
		}
	}
	//if no data, signal end of stream, otherwise regular send
	if (!data || !size) {
		if (sess->local_buf_len) {
			gf_mx_v(sess->mx);
			if (sess->hmux_is_eos) {
				return GF_IP_NETWORK_EMPTY;
			}
			sess->hmux_is_eos = 1;
			sess->hmux_sess->write(sess);
			return GF_OK;
		}
		sess->hmux_is_eos = 1;
		sess->hmux_sess->write(sess);
		//stream_id is not yet 0 in case of PUT/PUSH, stream is closed once we get reply from server
	} else {
		sess->hmux_is_eos = 0;
		//send the data
		e = sess->hmux_sess->send_pending_data(sess);
		if (e && (e!=GF_IP_NETWORK_EMPTY))
			return e;
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
	if ((sess->hmux_stream_id<0) && sess->hmux_send_data)
		return GF_URL_REMOVED;
	return GF_OK;
}

#endif //GPAC_HTTPMUX



void gf_dm_sess_close_hmux(GF_DownloadSession *sess)
{
#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess)
		sess->hmux_sess->close_session(sess);
#endif
}

GF_DownloadSession *gf_dm_sess_new_subsession(GF_DownloadSession *sess, s64 stream_id, void *usr_cbk, GF_Err *e)
{
#ifdef GPAC_HTTPMUX
	GF_DownloadSession *sub_sess;
	if (!sess->hmux_sess) return NULL;
	gf_mx_p(sess->mx);
	sub_sess = gf_dm_sess_new_internal(NULL, NULL, 0, sess->user_proc, usr_cbk, sess->sock, GF_TRUE, e);
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

