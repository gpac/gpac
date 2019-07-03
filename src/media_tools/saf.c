/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / LASeR codec sub-project
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

#include <gpac/internal/media_dev.h>
#include <gpac/bitstream.h>
#include <gpac/thread.h>
#include <gpac/list.h>

enum
{
	SAF_STREAM_HEADER = 1,
	SAF_STREAM_HEADER_PERMANENT = 2,
	SAF_END_OF_STREAM = 3,
	SAF_ACCESS_UNIT = 4,
	SAF_END_OF_SESSION = 5,
	SAF_CACHE_UNIT = 6,
	SAF_REMOTE_STREAM_HEADER = 7,
};


typedef struct
{
	char *data;
	u32 data_size;
	Bool is_rap;
	u32 ts;
} GF_SAFSample;

typedef struct
{
	u32 stream_id;
	u32 ts_resolution;
	u32 buffersize_db;
	u8 stream_type, object_type;
	char *mime_type;
	char *remote_url;

	char *dsi;
	u32 dsi_len;

	GF_List *aus;
	/*0: not declared yet; 1: declared; (1<<1) : done but end of stream not sent yet*/
	u32 state;
	u32 last_au_sn, last_au_ts;
} GF_SAFStream;

struct __saf_muxer
{
	GF_List *streams;
	/*0: nothing to do, 1: should regenerate, (1<<1): end of session has been sent*/
	u32 state;
	GF_Mutex *mx;
};

GF_SAFMuxer *gf_saf_mux_new()
{
	GF_SAFMuxer *mux;
	GF_SAFEALLOC(mux, GF_SAFMuxer);
	if (!mux) return NULL;
	mux->mx = gf_mx_new("SAF");
	mux->streams = gf_list_new();
	return mux;
}

static void saf_stream_del(GF_SAFStream *str)
{
	if (str->mime_type) gf_free(str->mime_type);
	if (str->remote_url) gf_free(str->remote_url);
	if (str->dsi) gf_free(str->dsi);

	while (gf_list_count(str->aus)) {
		GF_SAFSample *au = (GF_SAFSample *)gf_list_last(str->aus);
		gf_list_rem_last(str->aus);
		if (au->data) gf_free(au->data);
		gf_free(au);
	}
	gf_list_del(str->aus);
	gf_free(str);
}

void gf_saf_mux_del(GF_SAFMuxer *mux)
{
	while (gf_list_count(mux->streams)) {
		GF_SAFStream *str = (GF_SAFStream *)gf_list_last(mux->streams);
		gf_list_rem_last(mux->streams);
		saf_stream_del(str);
	}
	gf_list_del(mux->streams);
	gf_mx_del(mux->mx);
	gf_free(mux);
}

static GFINLINE GF_SAFStream *saf_get_stream(GF_SAFMuxer *mux, u32 stream_id)
{
	GF_SAFStream *str;
	u32 i=0;
	while ( (str = (GF_SAFStream *)gf_list_enum(mux->streams, &i)) ) {
		if (str->stream_id==stream_id) return str;
	}
	return NULL;
}

GF_Err gf_saf_mux_stream_add(GF_SAFMuxer *mux, u32 stream_id, u32 ts_res, u32 buffersize_db, u8 stream_type, u8 object_type, char *mime_type, char *dsi, u32 dsi_len, char *remote_url)
{
	GF_SAFStream *str = saf_get_stream(mux, stream_id);
	if (str) return GF_BAD_PARAM;

	if (mux->state == 2) return GF_BAD_PARAM;

	gf_mx_p(mux->mx);

	GF_SAFEALLOC(str, GF_SAFStream);
	if (!str) return GF_OUT_OF_MEM;
	str->stream_id = stream_id;
	str->ts_resolution = ts_res;
	str->buffersize_db = buffersize_db;
	str->stream_type = stream_type;
	str->object_type = object_type;
	if (mime_type) {
		str->mime_type = gf_strdup(mime_type);
		str->stream_type = str->object_type = 0xFF;
	}
	str->dsi_len = dsi_len;
	if (dsi_len) {
		str->dsi = (char *) gf_malloc(sizeof(char)*dsi_len);
		memcpy(str->dsi, dsi, sizeof(char)*dsi_len);
	}
	if (remote_url) str->remote_url = gf_strdup(remote_url);
	str->aus = gf_list_new();
	mux->state = 0;
	gf_list_add(mux->streams, str);
	gf_mx_v(mux->mx);
	return GF_OK;
}

#if 0 //unused
/*!
 Removes a stream from the SAF multiplex
 \param mux the SAF multiplexer object
 \param stream_id ID of the SAF stream to remove
 \return error if any
 */
GF_Err gf_saf_mux_stream_rem(GF_SAFMuxer *mux, u32 stream_id)
{
	GF_SAFStream *str = saf_get_stream(mux, stream_id);
	if (!str) return GF_BAD_PARAM;
	if (mux->state == 2) return GF_BAD_PARAM;

	gf_mx_p(mux->mx);
	str->state |= 2;
	mux->state = 1;
	gf_mx_v(mux->mx);
	return GF_OK;
}
#endif


GF_Err gf_saf_mux_add_au(GF_SAFMuxer *mux, u32 stream_id, u32 CTS, char *data, u32 data_len, Bool is_rap)
{
	GF_SAFSample *au;
	GF_SAFStream *str = saf_get_stream(mux, stream_id);
	if (!str) return GF_BAD_PARAM;
	if (mux->state == 2) return GF_BAD_PARAM;

	gf_mx_p(mux->mx);

	GF_SAFEALLOC(au, GF_SAFSample);
	if (!au) return GF_OUT_OF_MEM;
	au->data = data;
	au->data_size = data_len;
	au->is_rap = is_rap;
	au->ts = CTS;
	mux->state = 1;

	gf_list_add(str->aus, au);
	gf_mx_v(mux->mx);
	return GF_OK;
}


GF_Err gf_saf_mux_for_time(GF_SAFMuxer *mux, u32 time_ms, Bool force_end_of_session, u8 **out_data, u32 *out_size)
{
	u32 i, count, dlen;
	u8 *data;
	GF_SAFStream *str;
	GF_SAFSample*au;
	GF_BitStream *bs, *payload;

	*out_data = NULL;
	*out_size = 0;

	gf_mx_p(mux->mx);
	if (!force_end_of_session && (mux->state!=1)) {
		gf_mx_v(mux->mx);
		return GF_OK;
	}

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	count = gf_list_count(mux->streams);

	/*1: write all stream headers*/
	for (i=0; i<count; i++) {
		str = (GF_SAFStream *)gf_list_get(mux->streams, i);
		if (str->state & 1) continue;

		au = (GF_SAFSample *)gf_list_get(str->aus, 0);

		/*write stream declaration*/
		payload = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_int(payload, str->remote_url ? SAF_REMOTE_STREAM_HEADER : SAF_STREAM_HEADER, 4);
		gf_bs_write_int(payload, str->stream_id, 12);

		gf_bs_write_u8(payload, str->object_type);
		gf_bs_write_u8(payload, str->stream_type);
		gf_bs_write_int(payload, str->ts_resolution, 24);
		gf_bs_write_u16(payload, str->buffersize_db);
		if (str->mime_type) {
			u32 len = (u32) strlen(str->mime_type);
			gf_bs_write_u16(payload, len);
			gf_bs_write_data(payload, str->mime_type, len);
		}
		if (str->remote_url) {
			u32 len = (u32) strlen(str->remote_url);
			gf_bs_write_u16(payload, len);
			gf_bs_write_data(payload, str->remote_url, len);
		}
		if (str->dsi) {
			gf_bs_write_data(payload, str->dsi, str->dsi_len);
		}

		gf_bs_get_content(payload, &data, &dlen);
		gf_bs_del(payload);

		/*write SAF packet header*/
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_int(bs, 0, 15);
		gf_bs_write_int(bs, 0, 1);
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_int(bs, au ? au->ts : 0, 30);
		gf_bs_write_int(bs, dlen, 16);
		gf_bs_write_data(bs, data, dlen);
		gf_free(data);

		/*mark as signaled*/
		str->state |= 1;
	}

	/*write all pending AUs*/
	while (1) {
		GF_SAFStream *src = NULL;
		u32 mux_time = time_ms;

		for (i=0; i<count; i++) {
			str = (GF_SAFStream*)gf_list_get(mux->streams, i);
			au = (GF_SAFSample*)gf_list_get(str->aus, 0);
			if (au && (au->ts*1000 < mux_time*str->ts_resolution)) {
				mux_time = 1000*au->ts/str->ts_resolution;
				src = str;
			}
		}

		if (!src) break;

		au = (GF_SAFSample*)gf_list_get(src->aus, 0);
		gf_list_rem(src->aus, 0);

		/*write stream declaration*/
		gf_bs_write_int(bs, au->is_rap ? 1 : 0, 1);
		gf_bs_write_int(bs, src->last_au_sn, 15);
		gf_bs_write_int(bs, 0, 1);
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_int(bs, au->ts, 30);
		gf_bs_write_u16(bs, 2+au->data_size);
		gf_bs_write_int(bs, SAF_ACCESS_UNIT, 4);
		gf_bs_write_int(bs, src->stream_id, 12);
		gf_bs_write_data(bs, au->data, au->data_size);

		src->last_au_sn ++;
		src->last_au_ts = au->ts;
		gf_free(au->data);
		gf_free(au);
	}

	/*3: write all end of stream*/
	for (i=0; i<count; i++) {
		str = (GF_SAFStream*)gf_list_get(mux->streams, i);
		/*mark as signaled*/
		if (!(str->state & 2)) continue;
		if (gf_list_count(str->aus)) continue;

		/*write stream declaration*/
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_int(bs, str->last_au_sn, 15);
		gf_bs_write_int(bs, 0, 1);
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_int(bs, str->last_au_ts, 30);
		gf_bs_write_int(bs, 2, 16);
		gf_bs_write_int(bs, SAF_END_OF_STREAM, 4);
		gf_bs_write_int(bs, str->stream_id, 12);

		/*remove stream*/
		gf_list_rem(mux->streams, i);
		i--;
		count--;
		saf_stream_del(str);
	}
	mux->state = 0;
	if (force_end_of_session) {
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_int(bs, 0, 15);
		gf_bs_write_int(bs, 0, 1);
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_int(bs, 0, 30);
		gf_bs_write_int(bs, 2, 16);
		gf_bs_write_int(bs, SAF_END_OF_SESSION, 4);
		gf_bs_write_int(bs, 0, 12);
		mux->state = 2;
	}
	gf_bs_get_content(bs, out_data, out_size);
	gf_bs_del(bs);
	gf_mx_v(mux->mx);
	return GF_OK;
}
