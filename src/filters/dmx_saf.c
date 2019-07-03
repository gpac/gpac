/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / SAF demuxer filter
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
#include <gpac/thread.h>
#include <gpac/list.h>
#include <gpac/bitstream.h>

typedef struct
{
	GF_FilterPid *opid;
	u32 au_sn, stream_id, ts_res, buffer_min;
} GF_SAFStream;

enum
{
	SAF_FILE_LOCAL,
	SAF_FILE_REMOTE,
	SAF_LIVE_STREAM
};

typedef struct
{
	GF_FilterPid *ipid;


	GF_List *streams;

	u32 saf_type;

	Double start_range, end_range;
	u32 nb_playing;
	Bool is_file, file_loaded;
	GF_Fraction duration;
	u64 file_pos, file_size;

	Bool initial_play_done;

	char *saf_data;
	u32 alloc_size, saf_size;
} GF_SAFDmxCtx;


static GFINLINE GF_SAFStream *saf_get_channel(GF_SAFDmxCtx *saf, u32 stream_id)
{
	GF_SAFStream *st;
	u32 i=0;
	while ((st = (GF_SAFStream *)gf_list_enum(saf->streams, &i))) {
		if (st->stream_id==stream_id) return st;
	}
	return NULL;
}

static void safdmx_demux(GF_Filter *filter, GF_SAFDmxCtx *ctx, char *data, u32 data_size)
{
	Bool is_rap, go;
	GF_SAFStream *st;
	u32 cts, au_size, type, i, stream_id;
	u64 bs_pos;
	GF_BitStream *bs;

	if (ctx->alloc_size < ctx->saf_size + data_size) {
		ctx->saf_data = (char*)gf_realloc(ctx->saf_data, sizeof(char)*(ctx->saf_size + data_size) );
		ctx->alloc_size = ctx->saf_size + data_size;
	}
	//we could avoid a full copy of the buffer, but given how much SAF is used that's not very urgent ...
	memcpy(ctx->saf_data + ctx->saf_size, data, sizeof(char)*data_size);
	ctx->saf_size += data_size;

	/*first AU not complete yet*/
	if (ctx->saf_size<10) return;

	bs = gf_bs_new(ctx->saf_data, ctx->saf_size, GF_BITSTREAM_READ);
	bs_pos = 0;

	go = GF_TRUE;
	while (go) {
		u64 avail = gf_bs_available(bs);
		bs_pos = gf_bs_get_position(bs);

		if (avail<10) break;

		is_rap = (Bool)gf_bs_read_int(bs, 1);
		/*au_sn = */gf_bs_read_int(bs, 15);
		gf_bs_read_int(bs, 2);
		cts = gf_bs_read_int(bs, 30);
		au_size = gf_bs_read_int(bs, 16);
		avail-=8;

		if (au_size > avail) break;
		assert(au_size>=2);

		type = gf_bs_read_int(bs, 4);
		stream_id = gf_bs_read_int(bs, 12);
		au_size -= 2;

		st = saf_get_channel(ctx, stream_id);
		switch (type) {
		case 1:
		case 2:
		case 7:
			if (st) {
				gf_bs_skip_bytes(bs, au_size);
			} else {
				u32 oti, stype;
				GF_SAFStream *first = (GF_SAFStream *)gf_list_get(ctx->streams, 0);
				GF_SAFEALLOC(st, GF_SAFStream);
				if (!st) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[SAF] Failed to allocate SAF channel"));
					gf_bs_del(bs);
					return;
				}
				st->stream_id = stream_id;
				st->opid = gf_filter_pid_new(filter);
				gf_filter_pid_set_property(st->opid, GF_PROP_PID_ID, &PROP_UINT(stream_id));
				gf_filter_pid_set_property(st->opid, GF_PROP_PID_ESID, &PROP_UINT(stream_id));
				gf_filter_pid_set_property(st->opid, GF_PROP_PID_CLOCK_ID, &PROP_UINT(first ? first->stream_id : stream_id));
				if (!first) {
					gf_filter_pid_set_property(st->opid, GF_PROP_PID_IN_IOD, &PROP_BOOL(GF_TRUE));
				}
				oti = gf_bs_read_u8(bs);
				gf_filter_pid_set_property(st->opid, GF_PROP_PID_CODECID, &PROP_UINT(oti));
				stype = gf_bs_read_u8(bs);
				gf_filter_pid_set_property(st->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(stype));
				st->ts_res = gf_bs_read_u24(bs);
				gf_filter_pid_set_property(st->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(st->ts_res));
				if (ctx->duration.num)
					gf_filter_pid_set_property(st->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));

				/*bufferSizeDB = */gf_bs_read_u16(bs);
				au_size -= 7;
				if ((oti == 0xFF) && (stype == 0xFF) ) {
					u16 mimeLen = gf_bs_read_u16(bs);
					gf_bs_skip_bytes(bs, mimeLen);
					au_size -= mimeLen+2;
				}
				if (type==7) {
					u16 urlLen = gf_bs_read_u16(bs);
					char *url_string = (char*)gf_malloc(sizeof(char)*(urlLen+1));
					gf_bs_read_data(bs, url_string, urlLen);
					url_string[urlLen] = 0;
					au_size -= urlLen+2;
					gf_filter_pid_set_property(st->opid, GF_PROP_PID_REMOTE_URL, &PROP_NAME(url_string));
				}
				if (au_size) {
					char *dsi = (char*)gf_malloc(sizeof(char)*au_size);
					gf_bs_read_data(bs, dsi, au_size);
					gf_filter_pid_set_property(st->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, au_size) );
				}
				gf_list_add(ctx->streams, st);
			}
			break;
		case 4:
			if (st) {
				GF_FilterPacket *pck;
				u8 *pck_data;
				bs_pos = gf_bs_get_position(bs);
				pck = gf_filter_pck_new_alloc(st->opid, au_size, &pck_data);
				memcpy(pck_data, ctx->saf_data+bs_pos, au_size);
				//TODO: map byte range pos ?
				//TODO: map AU SN  ?

				gf_filter_pck_set_cts(pck, cts);
				if (is_rap)
					gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);

				if (ctx->start_range && (ctx->start_range * st->ts_res > cts*1000)) {
					gf_filter_pck_set_seek_flag(pck, GF_TRUE);
				}
				gf_filter_pck_send(pck);
			}
			gf_bs_skip_bytes(bs, au_size);
			break;
		case 3:
			if (st && st->opid) gf_filter_pid_set_eos(st->opid);
			break;
		case 5:
			i=0;
			while ((st = (GF_SAFStream *)gf_list_enum(ctx->streams, &i))) {
				if (st && st->opid) gf_filter_pid_set_eos(st->opid);
			}
			break;
		}
	}

	gf_bs_del(bs);
	if (bs_pos) {
		u32 remain = (u32) (ctx->saf_size - bs_pos);
		if (remain) memmove(ctx->saf_data, ctx->saf_data+bs_pos, sizeof(char)*remain);
		ctx->saf_size = remain;
	}
}


typedef struct
{
	u32 stream_id;
	u32 ts_res;
} StreamInfo;

static void safdmx_check_dur(GF_SAFDmxCtx *ctx)
{
	u32 nb_streams, i, cts, au_size, au_type, stream_id, ts_res;
	GF_BitStream *bs;
	GF_Fraction dur;
	const GF_PropertyValue *p;
	StreamInfo si[1024];
	FILE *stream;


	if (ctx->duration.num) return;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string) {
		ctx->is_file = GF_FALSE;
		ctx->duration.num=1;
		return;
	}
	ctx->is_file = GF_TRUE;
	if (!ctx->file_loaded) return;

	stream = gf_fopen(p->value.string, "rb");
	if (!stream) return;

	bs = gf_bs_from_file(stream, GF_BITSTREAM_READ);
	ctx->file_size = gf_bs_get_size(bs);

	dur.num = 0;
	dur.den = 1000;
	nb_streams=0;
	while (gf_bs_available(bs)) {
		gf_bs_read_u16(bs);
		gf_bs_read_int(bs, 2);
		cts = gf_bs_read_int(bs, 30);
		au_size = gf_bs_read_int(bs, 16);
		au_type = gf_bs_read_int(bs, 4);
		stream_id = gf_bs_read_int(bs, 12);
		au_size-=2;
		ts_res = 0;
		for (i=0; i<nb_streams; i++) {
			if (si[i].stream_id==stream_id) ts_res = si[i].ts_res;
		}
		if (!ts_res) {
			if ((au_type==1) || (au_type==2) || (au_type==7)) {
				gf_bs_read_u16(bs);
				ts_res = gf_bs_read_u24(bs);
				au_size -= 5;
				si[nb_streams].stream_id = stream_id;
				si[nb_streams].ts_res = ts_res;
				nb_streams++;
			}
		}
		if (ts_res && (au_type==4)) {
			Double ts = cts;
			ts /= ts_res;
			if (ts * 1000 > dur.num) dur.num = (u32) (ts * 1000);
		}
		gf_bs_skip_bytes(bs, au_size);
	}
	gf_bs_del(bs);
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num * dur.den != dur.num * ctx->duration.den)) {
		u32 i=0;
		GF_SAFStream *st;
		ctx->duration = dur;
		while ( (st = gf_list_enum(ctx->streams, &i)) ) {
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
		}
	}
}


GF_Err safdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 i;
	GF_SAFDmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		GF_SAFStream *st;
		ctx->ipid = NULL;

		while ((st = (GF_SAFStream*)gf_list_enum(ctx->streams, &i))) {
			if (st->opid)
				gf_filter_pid_remove(st->opid);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	return GF_OK;
}

static Bool safdmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_FilterEvent fevt;
	GF_SAFDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (ctx->nb_playing && (ctx->start_range == evt->play.start_range)) {
			return GF_TRUE;
		}
		if (! ctx->is_file) {
			return GF_FALSE;
		}
		safdmx_check_dur(ctx);

		ctx->nb_playing++;

		ctx->start_range = evt->play.start_range;
		ctx->file_pos = 0;
		if (ctx->duration.num) {
			ctx->file_pos = (u64) (ctx->file_size * ctx->start_range);
			ctx->file_pos *= ctx->duration.den;
			ctx->file_pos /= ctx->duration.num;
			if (ctx->file_pos>ctx->file_size) return GF_TRUE;
		}

		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (!ctx->file_pos)
				return GF_TRUE;
		}
		//post a seek to 0 - we would need to build an index of AUs to find the next place to seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = 0;
		ctx->saf_size = 0;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		ctx->nb_playing--;
		//don't cancel event
		return GF_FALSE;

	case GF_FEVT_SET_SPEED:
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

GF_Err safdmx_process(GF_Filter *filter)
{
	GF_SAFDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	GF_SAFStream *st;
	u32 i=0, pkt_size;
	const char *data;
	u32 would_block = 0;

	//update duration
	safdmx_check_dur(ctx);

	//check if all the streams are in block state, if so return.
	//we need to check for all output since one pid could still be buffering
	while ((st = gf_list_enum(ctx->streams, &i))) {
		if (st->opid && gf_filter_pid_would_block(st->opid))
			would_block++;
	}
	if (would_block && (would_block+1==i))
		return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
//		if (gf_filter_pid_is_eos(ctx->ipid)) oggdmx_signal_eos(ctx);
		return GF_OK;
	}
	data = gf_filter_pck_get_data(pck, &pkt_size);
	safdmx_demux(filter, ctx, (char *) data, pkt_size);
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

GF_Err safdmx_initialize(GF_Filter *filter)
{
	GF_SAFDmxCtx *ctx = gf_filter_get_udta(filter);
	ctx->streams = gf_list_new();
	return GF_OK;
}

void safdmx_finalize(GF_Filter *filter)
{
	GF_SAFDmxCtx *ctx = gf_filter_get_udta(filter);

	while (gf_list_count(ctx->streams)) {
		GF_SAFStream *st = (GF_SAFStream *)gf_list_last(ctx->streams);
		gf_list_rem_last(ctx->streams);
		gf_free(st);
	}
	if (ctx->saf_data) gf_free(ctx->saf_data);
	gf_list_del(ctx->streams);
}

static const char *safdmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	*score = GF_FPROBE_EXT_MATCH;
	return "saf|lsr";
}

static const GF_FilterCapability SAFDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/x-saf|application/saf"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "saf|lsr"),
};

GF_FilterRegister SAFDmxRegister = {
	.name = "safdmx",
	GF_FS_SET_DESCRIPTION("SAF demuxer")
	GF_FS_SET_HELP("This filter demultiplexes SAF (MPEG-4 Simple Aggregation Format for LASeR) files/data into a set of media PIDs and frames.")
	.private_size = sizeof(GF_SAFDmxCtx),
	.initialize = safdmx_initialize,
	.finalize = safdmx_finalize,
	SETCAPS(SAFDmxCaps),
	.configure_pid = safdmx_configure_pid,
	.process = safdmx_process,
	.process_event = safdmx_process_event,
	.probe_data = safdmx_probe_data
};

const GF_FilterRegister *safdmx_register(GF_FilterSession *session)
{
	return &SAFDmxRegister;
}

