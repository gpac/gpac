/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2022
 *					All rights reserved
 *
 *  This file is part of GPAC / OGG muxer filter
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

#ifndef GPAC_DISABLE_OGG

#include <gpac/bitstream.h>
#include <gpac/internal/ogg.h>

typedef struct
{
	GF_FilterPid *ipid;
	Bool ready;
	u32 timescale;
	u64 init_ts, last_ts;
	u32 theora_kgs;
	u32 codec_id;
	u32 nb_i, nb_p;
	Bool inject_cfg;

	GF_FilterPacket *dangling_ref;

	ogg_stream_state os;
	ogg_packet op;
	ogg_page og;
} OGGMuxStream;

typedef struct
{
	GF_Fraction cdur;
	GF_Fraction rcfg;

	GF_FilterPid *opid;
	GF_List *streams;
	u64 nb_pck;
	Bool is_eos, is_playing;
	Double start_range;
	u64 last_reconf;

	u32 page_id;
	GF_Fraction64 ts_regulate;
	u32 dash_mode;

	u32 seg_num, next_seg_num;
	Bool wait_dash, copy_props;
	u64 seg_start, seg_size;
} OGGMuxCtx;

static GF_Err oggmux_send_page(OGGMuxCtx *ctx, OGGMuxStream *pctx, GF_FilterPacket *in_pck)
{
	if (pctx->og.header_len+pctx->og.body_len == 0) return GF_OK;

	u8 *output;
	GF_FilterPacket *pck = gf_filter_pck_new_alloc(ctx->opid, pctx->og.header_len+pctx->og.body_len, &output);
	if (!pck) return GF_OUT_OF_MEM;

	memcpy(output, pctx->og.header, pctx->og.header_len);
	memcpy(output + pctx->og.header_len, pctx->og.body, pctx->og.body_len);
	Bool start = ctx->nb_pck ? GF_FALSE : GF_TRUE;
	if (in_pck && ctx->copy_props) {
		const GF_PropertyValue *p = gf_filter_pck_get_property(in_pck, GF_PROP_PCK_FILENUM);
		if (p) {
			gf_filter_pck_set_property(pck, GF_PROP_PCK_FILENUM, p);

			p = gf_filter_pck_get_property(in_pck, GF_PROP_PCK_FILENAME);
			if (p) {
				gf_filter_pck_set_property(pck, GF_PROP_PCK_FILENAME, p);
				start = GF_TRUE;
			}
		}
		ctx->copy_props = GF_FALSE;
	}
	gf_filter_pck_set_framing(pck, start, GF_FALSE);
	gf_filter_pck_send(pck);
	ctx->nb_pck++;
	ctx->seg_size += pctx->og.header_len+pctx->og.body_len;
	return GF_OK;
}

static GF_Err oggmux_send_config(OGGMuxCtx *ctx, OGGMuxStream *pctx, GF_FilterPacket *in_pck)
{
	const GF_PropertyValue *p = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DECODER_CONFIG);
	if (!p) return GF_NON_COMPLIANT_BITSTREAM;

	pctx->os.b_o_s = 0;
	pctx->os.e_o_s = 0;

	if (pctx->codec_id==GF_CODECID_OPUS) {
		GF_BitStream *bs_out = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_data(bs_out, "OpusHead", 8);
		gf_bs_write_data(bs_out, p->value.data.ptr, p->value.data.size);
		gf_bs_get_content(bs_out, &pctx->op.packet, &pctx->op.bytes);
		gf_bs_del(bs_out);
		ogg_stream_packetin(&pctx->os, &pctx->op);
		gf_free(pctx->op.packet);
		pctx->op.packetno ++;

		//opus tags is mandatory
		bs_out = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_data(bs_out, "OpusTags", 8);
		const char *vendor = gf_sys_is_test_mode() ? "GPAC" : gf_gpac_version();
		u32 vlen = (u32) strlen(vendor);
		gf_bs_write_u32_le(bs_out, vlen);
		gf_bs_write_data(bs_out, vendor, vlen);
		gf_bs_write_u32_le(bs_out, 0);
		gf_bs_get_content(bs_out, &pctx->op.packet, &pctx->op.bytes);
		gf_bs_del(bs_out);
		ogg_stream_packetin(&pctx->os, &pctx->op);
		gf_free(pctx->op.packet);
		pctx->op.packetno ++;
	} else if (pctx->codec_id==GF_CODECID_FLAC) {
		GF_BitStream *bs_out = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_data(bs_out, "fLaC", 4);
		gf_bs_write_data(bs_out, p->value.data.ptr, p->value.data.size);
		gf_bs_get_content(bs_out, &pctx->op.packet, &pctx->op.bytes);
		gf_bs_del(bs_out);
		ogg_stream_packetin(&pctx->os, &pctx->op);
		gf_free(pctx->op.packet);
		pctx->op.packetno ++;
	} else if ((pctx->codec_id==GF_CODECID_VORBIS) || (pctx->codec_id==GF_CODECID_THEORA) || (pctx->codec_id==GF_CODECID_SPEEX)) {
		GF_BitStream *bs = gf_bs_new(p->value.data.ptr, p->value.data.size, GF_BITSTREAM_READ);
		while (gf_bs_available(bs)) {
			pctx->op.bytes = gf_bs_read_u16(bs);
			u32 pos = (u32) gf_bs_get_position(bs);
			pctx->op.packet = p->value.data.ptr + pos;
			gf_bs_skip_bytes(bs, pctx->op.bytes);
			ogg_stream_packetin(&pctx->os, &pctx->op);

			if ((pctx->codec_id==GF_CODECID_THEORA) && (pctx->op.packet[0]==0x80)){
				u32 kff;
				GF_BitStream *vbs = gf_bs_new((char*)pctx->op.packet, pctx->op.bytes, GF_BITSTREAM_READ);
				gf_bs_skip_bytes(vbs, 40);
				gf_bs_read_int(vbs, 6); /* quality */
				kff = 1 << gf_bs_read_int(vbs, 5);
				gf_bs_del(vbs);

				pctx->theora_kgs = 0;
				kff--;
				while (kff) {
					pctx->theora_kgs ++;
					kff >>= 1;
				}
			}
			pctx->op.packetno ++;
		}
		gf_bs_del(bs);

		if ((pctx->codec_id==GF_CODECID_THEORA) && !pctx->theora_kgs) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[OGGMux] Missing theora configuration\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	} else {
		return GF_NOT_SUPPORTED;
	}

	while (ogg_stream_pageout(&pctx->os, &pctx->og)>0) {
		GF_Err e = oggmux_send_page(ctx, pctx, in_pck);
		if (e) return e;
	}
	return GF_OK;
}

static GF_Err oggmux_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	u32 codec_id;
	OGGMuxCtx *ctx = (OGGMuxCtx *) gf_filter_get_udta(filter);

	OGGMuxStream *pctx = gf_filter_pid_get_udta(pid);
	if (is_remove) {
		if (pctx) {
			gf_list_del_item(ctx->streams, pctx);
			if (pctx->dangling_ref) gf_filter_pck_discard(pctx->dangling_ref);
			ogg_stream_clear(&pctx->os);
			gf_free(pctx);
		}

		if (!gf_list_count(ctx->streams) && ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_BAD_PARAM;
	codec_id = p->value.uint;
	switch (codec_id) {
	case GF_CODECID_OPUS:
	case GF_CODECID_VORBIS:
	case GF_CODECID_FLAC:
	case GF_CODECID_THEORA:
	case GF_CODECID_DIRAC:
	case GF_CODECID_SPEEX:
		break;
	default:
		return GF_FILTER_NOT_SUPPORTED;
	}

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_name(ctx->opid, "ogg_mux");

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("*") );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("*") );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DASH_MODE, NULL);

	if (!pctx) {
		GF_SAFEALLOC(pctx, OGGMuxStream);
		if (!pctx) return GF_OUT_OF_MEM;
		gf_list_add(ctx->streams, pctx);
		gf_filter_pid_set_udta(pid, pctx);
		pctx->ipid = pid;
		ctx->page_id++;
		ogg_stream_init(&pctx->os, ctx->page_id);
		pctx->codec_id = codec_id;

		if (ctx->is_playing) {
			GF_FilterEvent evt;
			GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
			evt.play.start_range = ctx->start_range;
			gf_filter_pid_send_event(pid, &evt);
		}
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	pctx->timescale = p ? p->value.uint : 1000;

	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DASH_MODE);
	if (p && p->value.uint) {
		if (!ctx->dash_mode) ctx->copy_props = GF_TRUE;
		ctx->dash_mode = p->value.uint;
		if (ctx->dash_mode == 2) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[OGGMux] DASH On-demand profile not specified for OGG segments\n"));
			return GF_FILTER_NOT_SUPPORTED;
		}
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!p) {
		pctx->ready = GF_FALSE;
		return GF_OK;
	}
	pctx->ready = GF_TRUE;
	if (ctx->dash_mode) {
		pctx->inject_cfg = GF_TRUE;
		return GF_OK;
	}

	return oggmux_send_config(ctx, pctx, NULL);
}

static void oggmux_send_seg_info(OGGMuxCtx *ctx)
{
	if (ctx->seg_size) {
		GF_FilterEvent evt;
		OGGMuxStream *pctx = gf_list_get(ctx->streams, 0);
		GF_FEVT_INIT(evt, GF_FEVT_SEGMENT_SIZE, pctx->ipid);
		evt.seg_size.media_range_start = ctx->seg_start;
		evt.seg_size.media_range_end = ctx->seg_start + ctx->seg_size - 1;
		gf_filter_pid_send_event(pctx->ipid, &evt);

		ctx->seg_start += ctx->seg_size;
		ctx->seg_size = 0;
	}
}
static GF_Err oggmux_process(GF_Filter *filter)
{
	u32 nb_skip=0;
	OGGMuxCtx *ctx = (OGGMuxCtx *) gf_filter_get_udta(filter);
	u32 i, nb_done = 0, count = gf_list_count(ctx->streams), nb_dash_done = 0;;

	for (i=0; i<count; i++) {
		const u8 *data=NULL;
		u32 size;
		OGGMuxStream *pctx = gf_list_get(ctx->streams, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(pctx->ipid);

		if (!pctx->ready) {
			continue;
		}
		if (!pck) {
			if (gf_filter_pid_is_eos(pctx->ipid)) nb_done++;
			continue;
		}

		if (gf_filter_pck_get_frame_interface(pck)!=NULL) {
			pctx->dangling_ref = gf_filter_pck_dangling_copy(pck, pctx->dangling_ref);
			if (pctx->dangling_ref) data = gf_filter_pck_get_data(pctx->dangling_ref, &size);
		} else {
			data = gf_filter_pck_get_data(pck, &size);
		}
		if (!data) {
			continue;
		}

		if (ctx->dash_mode) {
			const GF_PropertyValue *p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
			if (p && (p->value.uint>ctx->seg_num)) {
				if (!ctx->wait_dash) {
					ctx->next_seg_num = p->value.uint;
				}
				ctx->wait_dash = GF_TRUE;
				nb_dash_done ++;
				continue;
			}
		} else if (count>1) {
			u64 ts = gf_filter_pck_get_dts(pck);
			if (ts==GF_FILTER_NO_TS)
				ts = gf_filter_pck_get_cts(pck);

			if (ts!=GF_FILTER_NO_TS) {
				if (!pctx->init_ts) pctx->init_ts = ts+1;
				ts -= pctx->init_ts-1;

				//ahead of our interleaving window, don't write yet
				if (gf_timestamp_greater(ts, pctx->timescale, ctx->ts_regulate.num, ctx->ts_regulate.den)) {
					nb_skip++;
					continue;
				}
			}
		}

		if (pctx->inject_cfg) {
			if (ctx->dash_mode || gf_filter_pck_get_sap(pck)) {
				//reinit stream
				u32 pageno = pctx->os.serialno;

				ogg_stream_clear(&pctx->os);
				ogg_stream_init(&pctx->os, pageno);

				GF_Err e = oggmux_send_config(ctx, pctx, pck);
				if (e) return e;
				pctx->inject_cfg = GF_FALSE;
			}
		}

		ctx->is_eos = GF_FALSE;
		pctx->op.bytes = size;
		pctx->op.packet = (unsigned char*)data;
		pctx->op.packetno ++;

		if (pctx->theora_kgs) {
            Bool is_sap = (data[0] & 0x40) ? 0 : 1;
			if (is_sap) {
				pctx->nb_i += pctx->nb_p;
				pctx->nb_p = 0;
			} else {
				pctx->nb_p++;
			}
			if (pctx->nb_p >= (u32) (1<<pctx->theora_kgs)) {
				pctx->nb_i += pctx->nb_p;
				pctx->nb_p = 0;
			}
			pctx->op.granulepos = (pctx->nb_i << pctx->theora_kgs) | pctx->nb_p;
		} else {
			u64 cts = gf_filter_pck_get_cts(pck);
			if (cts == GF_FILTER_NO_TS) cts = pctx->last_ts;
			else pctx->last_ts = cts;

			pctx->op.granulepos = cts + gf_filter_pck_get_duration(pck);
		}

		if (gf_filter_pid_is_eos(pctx->ipid))
			pctx->op.e_o_s = 1;

		ogg_stream_packetin(&pctx->os, &pctx->op);

		while (ogg_stream_pageout(&pctx->os, &pctx->og)>0) {
			oggmux_send_page(ctx, pctx, pck);
		}

		gf_filter_pid_drop_packet(pctx->ipid);
	}

	if (nb_done == count) {
		if (ctx->is_eos) return GF_EOS;
		ctx->is_eos = GF_TRUE;
		for (i=0; i<count; i++) {
			OGGMuxStream *pctx = gf_list_get(ctx->streams, i);

			while (ogg_stream_flush(&pctx->os, &pctx->og)>0) {
				oggmux_send_page(ctx, pctx, NULL);
			}
			ogg_stream_clear(&pctx->os);
		}
		gf_filter_pid_set_eos(ctx->opid);
		oggmux_send_seg_info(ctx);
		return GF_EOS;
	}
	else if (ctx->dash_mode) {
		if (ctx->wait_dash) {
			OGGMuxStream *pctx;
			if (nb_dash_done < count) return GF_OK;
			ctx->wait_dash = GF_FALSE;
			ctx->seg_num = ctx->next_seg_num;
			ctx->copy_props = GF_TRUE;
			for (i=0; i<count; i++) {
				pctx = gf_list_get(ctx->streams, i);
				pctx->inject_cfg = GF_TRUE;

				if (ogg_stream_flush(&pctx->os, &pctx->og)>0)
					oggmux_send_page(ctx, pctx, NULL);
			}
			oggmux_send_seg_info(ctx);
		}
	}
	else if ((count>1) && (nb_skip == count)) {
		ctx->ts_regulate.num += ctx->cdur.num;

		if (ctx->rcfg.num &&ctx->rcfg.den) {
			if (gf_timestamp_greater_or_equal(ctx->ts_regulate.num, ctx->ts_regulate.den, ctx->last_reconf, ctx->rcfg.den)) {
				ctx->last_reconf += ctx->rcfg.num;
				for (i=0; i<count; i++) {
					OGGMuxStream *pctx = gf_list_get(ctx->streams, i);
					pctx->inject_cfg = GF_TRUE;
				}
			}
		}
	}

	return GF_OK;
}

static Bool oggmux_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	OGGMuxCtx *ctx = (OGGMuxCtx *) gf_filter_get_udta(filter);
	if (evt->base.type == GF_FEVT_PLAY) {
		ctx->is_playing = GF_TRUE;
		ctx->start_range = evt->play.start_range;
	} else if (evt->base.type == GF_FEVT_STOP)
		ctx->is_playing = GF_FALSE;

	return GF_FALSE;
}

static GF_Err oggmux_initialize(GF_Filter *filter)
{
	OGGMuxCtx *ctx = (OGGMuxCtx *) gf_filter_get_udta(filter);
	ctx->streams = gf_list_new();
	if (ctx->cdur.num * (s32) ctx->cdur.den <= 0) {
		ctx->cdur.num = 1000;
		ctx->cdur.den = 1000;
	}
	ctx->last_reconf = ctx->rcfg.num;
	ctx->ts_regulate.num = ctx->cdur.num;
	ctx->ts_regulate.den = ctx->cdur.den;
	return GF_OK;
}

static void oggmux_finalize(GF_Filter *filter)
{
	OGGMuxCtx *ctx = (OGGMuxCtx *) gf_filter_get_udta(filter);

	while (gf_list_count(ctx->streams)) {
		OGGMuxStream *pctx = gf_list_pop_back(ctx->streams);
		if (pctx->dangling_ref) gf_filter_pck_discard(pctx->dangling_ref);
		ogg_stream_clear(&pctx->os);
		gf_free(pctx);
	}
	gf_list_del(ctx->streams);
}

#define OFFS(_n)	#_n, offsetof(OGGMuxCtx, _n)

static const GF_FilterArgs OGGMuxArgs[] =
{
	{ OFFS(cdur), "stream interleaving duration in seconds", GF_PROP_FRACTION, "1/10", NULL, 0},
	{ OFFS(rcfg), "stream config re-injection frequency in seconds", GF_PROP_FRACTION, "0/1", NULL, 0},
	{0}
};

static const GF_FilterCapability OGGMuxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_THEORA),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_VORBIS),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_OPUS),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_FLAC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SPEEX),
	CAP_UINT(GF_CAPS_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "oga|spx|ogg|ogv|oggm|opus"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/ogg|audio/x-ogg|audio/x-vorbis+ogg|application/ogg|application/x-ogg|video/ogg|video/x-ogg|video/x-ogm+ogg|audio/opus|audio/x-opus"),
	{0}
};



GF_FilterRegister OGGMuxRegister = {
	.name = "oggmx",
	GF_FS_SET_DESCRIPTION("OGG multiplexer")
	GF_FS_SET_HELP("This filter multiplexes audio and video to produce an OGG stream.\n"
		"\n"
		"The [-cdur]() option allows specifiying the interleaving duration (max time difference between consecutive packets of different streams). \n"
	)
	.private_size = sizeof(OGGMuxCtx),
	.max_extra_pids = -1,
	.args = OGGMuxArgs,
	SETCAPS(OGGMuxCaps),
	.initialize = oggmux_initialize,
	.finalize = oggmux_finalize,
	.configure_pid = oggmux_configure_pid,
	.process = oggmux_process,
	.process_event = oggmux_process_event,
};

#endif

const GF_FilterRegister *oggmux_register(GF_FilterSession *session)
{
#ifndef GPAC_DISABLE_OGG
	return &OGGMuxRegister;
#else
	return NULL;
#endif

}

