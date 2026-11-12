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

#ifndef _DOWNLOADER_EXP_H_
#define _DOWNLOADER_EXP_H_

/*
	Put here all internal downloader functions that are needed by other filters (ssl, session access, etc)
	downloader.h should not be included by any other file than the downloader* files
*/


#include <gpac/download.h>

//socket and SSL context ownership is transfered to the download session object
GF_DownloadSession *gf_dm_sess_new_server(GF_DownloadManager *dm, GF_Socket *server, void *ssl_ctx, gf_dm_user_io user_io, void *usr_cbk, Bool async, GF_Err *e);
GF_DownloadSession *gf_dm_sess_new_subsession(GF_DownloadSession *sess, s64 stream_id, void *usr_cbk, GF_Err *e);
u32 gf_dm_sess_subsession_count(GF_DownloadSession *);

GF_Socket *gf_dm_sess_get_socket(GF_DownloadSession *);
void gf_dm_sess_clear_headers(GF_DownloadSession *sess);
void  gf_dm_sess_set_header(GF_DownloadSession *sess, const char *name, const char *value);
void  gf_dm_sess_set_header_ex(GF_DownloadSession *sess, const char *name, const char *value, Bool allow_overwrite);
GF_Err gf_dm_sess_flush_async(GF_DownloadSession *sess, Bool no_select);
u32 gf_dm_sess_async_pending(GF_DownloadSession *sess);

GF_Err gf_dm_sess_send_reply(GF_DownloadSession *sess, u32 reply_code, const char *response_body, u32 body_len, Bool no_body);
void gf_dm_sess_server_reset(GF_DownloadSession *sess);

void gf_dm_sess_set_timeout(GF_DownloadSession *sess, u32 timeout);

typedef enum
{
	GF_SESS_TYPE_HTTP=0,
	GF_SESS_TYPE_HTTP2,
	GF_SESS_TYPE_HTTP3,
} GF_HTTPSessionType;
GF_HTTPSessionType gf_dm_sess_is_hmux(GF_DownloadSession *sess);
void gf_dm_sess_close_hmux(GF_DownloadSession *sess);

void gf_dm_sess_set_sock_group(GF_DownloadSession *sess, GF_SockGroup *sg);

GF_Err gf_dm_sess_read(GF_DownloadSession *sess, char *data, u32 data_size, u32 *out_read);
GF_Err gf_dm_sess_send(GF_DownloadSession *sess, u8 *data, u32 size);
GF_Err gf_dm_sess_flush_async(GF_DownloadSession *sess, Bool no_select);
GF_Err gf_dm_sess_flush_close(GF_DownloadSession *sess);
Bool gf_dm_sess_use_tls(GF_DownloadSession * sess);

#ifdef GPAC_HAS_SSL

Bool gf_ssl_init_lib();
void *gf_ssl_new(void *ssl_server_ctx, GF_Socket *client_sock, GF_Err *e);
void gf_ssl_shutdown(void *ssl);
void *gf_ssl_server_context_new(const char *cert, const char *key, Bool for_quic);
void gf_ssl_server_context_del(void *ssl_server_ctx);
Bool gf_ssl_check_cert(void * ssl, const char *server_name);

#endif

#ifdef GPAC_HAS_NGTCP2

typedef struct __gf_quic_server GF_QuicServer;
GF_Err gf_dm_quic_server_new(GF_DownloadManager *dm, void *ssl_ctx, GF_QuicServer **oq, const char *ip, u32 port, const char *netcap_id,
	Bool (*accept_conn)(void *udta, GF_DownloadSession *sess, const char *address, u32 port),
	void *udta);

void gf_dm_quic_server_del(GF_QuicServer *qs);
GF_Socket *gf_dm_quic_get_socket(GF_QuicServer *qs);
GF_Err gf_dm_quic_process(GF_QuicServer *qs);
GF_Err gf_dm_quic_verify(GF_QuicServer *qs);
void gf_dm_sess_set_callback(GF_DownloadSession *sess, gf_dm_user_io user_io, void *usr_cbk);
#endif

#endif //_DOWNLOADER_EXP_H_
