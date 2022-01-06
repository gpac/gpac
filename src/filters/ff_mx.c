/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom Paris 2019-2020
 *					All rights reserved
 *
 *  This file is part of GPAC / ffmpeg muxer filter
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

#include <gpac/setup.h>

#ifdef GPAC_HAS_FFMPEG

#include "ff_common.h"
#include <gpac/network.h>
#include <gpac/bitstream.h>

#if (LIBAVFORMAT_VERSION_MAJOR <= 56)
#undef GPAC_HAS_FFMPEG
#endif

#endif

#ifdef GPAC_HAS_FFMPEG

typedef struct
{
	AVStream *stream;
	Bool ts_rescale;
	AVRational in_scale;
	Bool in_seg_flush;
	u32 cts_shift;

	Bool suspended;
	Bool reconfig_stream;
} GF_FFMuxStream;

enum{
	FFMX_STATE_ALLOC=0,
	FFMX_STATE_AVIO_OPEN,
	FFMX_STATE_HDR_DONE,
	FFMX_STATE_TRAILER_DONE,
	FFMX_STATE_EOS,
	FFMX_STATE_ERROR
};

enum
{
	FFMX_INJECT_DSI = 1,
	FFMX_INJECT_VID_INFO = 1<<1,
	FFMX_INJECT_AUD_INFO = 1<<2
};

typedef struct
{
	//options
	char *dst, *mime, *ffmt;
	Double start, speed;
	u32 block_size;
	Bool interleave, nodisc, ffiles, noinit;

	AVFormatContext *muxer;
	//decode options
	AVDictionary *options;

	GF_List *streams;

	//0 not created, 1 created, header written, 2 finalized, trailer written, 3: end of stream
	u32 status;

	Bool dash_mode, init_done;
	u32 nb_pck_in_seg;
	u32 dash_seg_num;
	u64 offset_at_seg_start;

	u8 *avio_ctx_buffer;
	AVIOContext *avio_ctx;
	FILE *gfio;

	u32 cur_file_idx_plus_one;

#if (LIBAVCODEC_VERSION_MAJOR < 59)
	AVPacket pkt;
#else
	AVPacket *pkt;
#endif
} GF_FFMuxCtx;

static GF_Err ffmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove);

static GF_Err ffmx_init_mux(GF_Filter *filter, GF_FFMuxCtx *ctx)
{
	u32 i, nb_pids;
	int res;

	assert(ctx->status==FFMX_STATE_AVIO_OPEN);

	ctx->status = FFMX_STATE_HDR_DONE;

	AVDictionary *options = NULL;
	av_dict_copy(&options, ctx->options, 0);

	res = avformat_init_output(ctx->muxer, &options);

	if (res<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Fail to open %s - error %s\n", ctx->dst, av_err2str(res) ));
		ctx->status = FFMX_STATE_ERROR;
		if (options) av_dict_free(&options);
		return GF_NOT_SUPPORTED;
	}

	nb_pids = gf_filter_get_ipid_count(filter);
	for (i=0; i<nb_pids; i++) {
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		GF_FFMuxStream *st = gf_filter_pid_get_udta(ipid);
		if (!st) continue;

		if ((st->in_scale.den == st->stream->time_base.den) && (st->in_scale.num == st->stream->time_base.num)) {
			st->ts_rescale = GF_FALSE;
		} else {
			st->ts_rescale = GF_TRUE;
		}
	}

	res = avformat_write_header(ctx->muxer, &options);
	if (res<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Fail to write header for %s - error %s\n", ctx->dst, av_err2str(res) ));
		ctx->status = FFMX_STATE_ERROR;
		if (options) av_dict_free(&options);
		return GF_SERVICE_ERROR;
	}

	ffmpeg_report_options(filter, options, ctx->options);

	return GF_OK;
}


static int ffavio_write_packet(void *opaque, uint8_t *buf, int buf_size)
{
	GF_FFMuxCtx *ctx = (GF_FFMuxCtx *)opaque;
	return (int) gf_fwrite(buf, buf_size, ctx->gfio);
}

static int64_t ffavio_seek(void *opaque, int64_t offset, int whence)
{
	GF_FFMuxCtx *ctx = (GF_FFMuxCtx *)opaque;
	if (whence==AVSEEK_SIZE) {
		u64 pos = gf_ftell(ctx->gfio);
		u64 size = gf_fsize(ctx->gfio);
		gf_fseek(ctx->gfio, pos, SEEK_SET);
		return size;
	}
	return (int64_t) gf_fseek(ctx->gfio, offset, whence);
}

static GF_Err ffmx_open_url(GF_FFMuxCtx *ctx, char *final_name)
{
	const char *dst;
	Bool use_gfio=GF_FALSE;
	const AVOutputFormat *ofmt = ctx->muxer->oformat;

	dst = final_name ? final_name : ctx->dst;
	if (!strncmp(dst, "gfio://", 7)) {
		use_gfio = GF_TRUE;
	}

	if (ofmt) {
		//handle local urls some_dir/some_other_dir/file.ext, ffmpeg doesn't create the dirs for us
		if (! (ofmt->flags & AVFMT_NOFILE) && gf_url_is_local(dst)) {
			FILE *f = gf_fopen(dst, "wb");
			gf_fclose(f);
		}
	}

	if (use_gfio) {
		ctx->gfio = gf_fopen(dst, "wb");
		if (!ctx->gfio) return GF_URL_ERROR;
		ctx->avio_ctx_buffer = av_malloc(ctx->block_size);
		if (!ctx->avio_ctx_buffer) return GF_OUT_OF_MEM;

		ctx->avio_ctx = avio_alloc_context(ctx->avio_ctx_buffer, ctx->block_size,
									  1, ctx, NULL, &ffavio_write_packet, &ffavio_seek);

		if (!ctx->avio_ctx) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Failed to create AVIO context for %s\n", dst));
			return GF_OUT_OF_MEM;
		}
		ctx->muxer->pb = ctx->avio_ctx;
	} else if (!ofmt || !(ofmt->flags & AVFMT_NOFILE) ) {
		int res = avio_open2(&ctx->muxer->pb, dst, AVIO_FLAG_WRITE, NULL, &ctx->options);
		if (res<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Fail to open AVIO context for %s - error %s\n", dst, av_err2str(res) ));
			return GF_FILTER_NOT_SUPPORTED;
		}
	}
	ctx->status = FFMX_STATE_AVIO_OPEN;
	return GF_OK;
}
static GF_Err ffmx_initialize_ex(GF_Filter *filter, Bool use_templates)
{
	const char *url, *sep;
	const AVOutputFormat *ofmt;
	GF_FFMuxCtx *ctx = (GF_FFMuxCtx *) gf_filter_get_udta(filter);

	if (!ctx->dst) return GF_BAD_PARAM;

	ffmpeg_setup_logs(GF_LOG_CONTAINER);

#if (LIBAVCODEC_VERSION_MAJOR >= 59)
	if (!ctx->pkt)
		ctx->pkt = av_packet_alloc();
#endif

	ctx->muxer = avformat_alloc_context();
	if (!ctx->muxer) return GF_OUT_OF_MEM;

	if (!ctx->streams)
		ctx->streams = gf_list_new();

	url = ctx->dst;

	if (!strncmp(ctx->dst, "gfio://", 7)) {
		url = gf_fileio_translate_url(ctx->dst);
	}
	sep = strchr(url, '$');
	if (sep && strchr(sep+1, '$'))
		use_templates = GF_TRUE;

	ofmt = av_guess_format(ctx->ffmt, url, ctx->mime);
	//if protocol is present, we may fail at guessing the format
	if (!ofmt && !ctx->ffmt) {
		u32 len;
		char szProto[20];
		char *proto = strstr(url, "://");
		if (!proto)
			return GF_FILTER_NOT_SUPPORTED;
		szProto[19] = 0;
		len = (u32) (proto - url);
		if (len>19) len=19;
		strncpy(szProto, url, len);
		szProto[len] = 0;
		ofmt = av_guess_format(szProto, url, ctx->mime);
	}
	if (!ofmt) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Failed to guess output format for %s, cannot run\n", ctx->dst));
		if (!ctx->ffmt) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Try specifying the format through :ffmt option\n"));
		}
		return GF_SERVICE_ERROR;
	}
	ctx->muxer->oformat = FF_OFMT_CAST ofmt;

	ctx->status = FFMX_STATE_ALLOC;
	//templates are used, we need to postpone opening the url until we have a PID and a first packet
	if (use_templates)
		return GF_OK;

	return ffmx_open_url(ctx, NULL);
}

static GF_Err ffmx_initialize(GF_Filter *filter)
{
	return ffmx_initialize_ex(filter, GF_FALSE);
}
static GF_Err ffmx_start_seg(GF_Filter *filter, GF_FFMuxCtx *ctx, const char *seg_name)
{
	int i;
	int res;

	/*create a new output context, clone all streams from current one and swap output context*/
	if (ctx->ffiles) {
		AVFormatContext *segmux;

		res = avformat_alloc_output_context2(&segmux, ctx->muxer->oformat, NULL, NULL);
		if (res<0) {
			return GF_IO_ERR;
		}
		segmux->interrupt_callback = ctx->muxer->interrupt_callback;
		segmux->max_delay = ctx->muxer->max_delay;
		av_dict_copy(&segmux->metadata, ctx->muxer->metadata, 0);
		segmux->opaque = ctx->muxer->opaque;
		segmux->io_close = ctx->muxer->io_close;
		segmux->io_open = ctx->muxer->io_open;
		segmux->flags = ctx->muxer->flags;

		for (i=0; i< (s32) ctx->muxer->nb_streams; i++) {
			u32 j;
			AVStream *st;
			AVCodecParameters *ipar, *opar;

			if (!(st = avformat_new_stream(segmux, NULL)))
				return GF_OUT_OF_MEM;
			ipar = ctx->muxer->streams[i]->codecpar;
			opar = st->codecpar;
			avcodec_parameters_copy(opar, ipar);
			if (!segmux->oformat->codec_tag ||
				av_codec_get_id (segmux->oformat->codec_tag, ipar->codec_tag) == opar->codec_id ||
				av_codec_get_tag(segmux->oformat->codec_tag, ipar->codec_id) <= 0) {
				opar->codec_tag = ipar->codec_tag;
			} else {
				opar->codec_tag = 0;
			}
			st->sample_aspect_ratio = ctx->muxer->streams[i]->sample_aspect_ratio;
			st->time_base = ctx->muxer->streams[i]->time_base;
			st->avg_frame_rate = ctx->muxer->streams[i]->avg_frame_rate;

#if (LIBAVFORMAT_VERSION_MAJOR < 59)
			if (ctx->muxer->streams[i]->codecpar->codec_tag == MKTAG('t','m','c','d'))
				st->codec->time_base = ctx->muxer->streams[i]->codec->time_base;
#endif

			av_dict_copy(&st->metadata, ctx->muxer->streams[i]->metadata, 0);

			/*reassign stream*/
			for (j=0; j<gf_list_count(ctx->streams); j++) {
				GF_FFMuxStream *ast = gf_list_get(ctx->streams, j);
				if (ast->stream == ctx->muxer->streams[i]) {
					ast->stream = st;
					break;
				}
			}
		}
		//swap context
		avformat_free_context(ctx->muxer);
		ctx->muxer = segmux;
	} else {
		ctx->muxer->io_close(ctx->muxer, ctx->muxer->pb);
		ctx->muxer->pb = NULL;
	}

#if (LIBAVFORMAT_VERSION_MAJOR < 59)
	strncpy(ctx->muxer->filename, seg_name, 1023);
	ctx->muxer->filename[1023]=0;
#else
	av_freep(&ctx->muxer->url);
	ctx->muxer->url = av_strdup(seg_name);
#endif
	ctx->offset_at_seg_start = 0;

	if (!(ctx->muxer->oformat->flags & AVFMT_NOFILE)) {
		//handle local urls some_dir/some_other_dir/file.ext, ffmpeg doesn't create the dirs for us
		if (gf_url_is_local(seg_name)) {
			FILE *f = gf_fopen(seg_name, "wb");
			gf_fclose(f);
		}

		res = ctx->muxer->io_open(ctx->muxer, &ctx->muxer->pb, /*ctx->muxer->url*/seg_name, AVIO_FLAG_WRITE, NULL);
		if (res < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Fail to open new segment %s - error %s\n", seg_name, av_err2str(res) ));
			return GF_IO_ERR;
		}
	}
	ctx->muxer->pb->seekable = 0;

    if (ctx->muxer->oformat->priv_class && ctx->muxer->priv_data)
        av_opt_set(ctx->muxer->priv_data, "mpegts_flags", "+resend_headers", 0);

    if (ctx->ffiles) {
        AVDictionary *options = NULL;
        av_dict_copy(&options, ctx->options, 0);
        av_dict_set(&options, "fflags", "-autobsf", 0);
        res = avformat_write_header(ctx->muxer, &options);
		if (options) av_dict_free(&options);
        if (res < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Fail to configure segment %s - error %s\n", seg_name, av_err2str(res) ));
			return GF_IO_ERR;
		}
    }
	ctx->status = FFMX_STATE_HDR_DONE;
	return GF_OK;
}


static GF_Err ffmx_close_seg(GF_Filter *filter, GF_FFMuxCtx *ctx, Bool send_evt_only)
{
	GF_FilterEvent evt;
	GF_FilterPid *pid;

	av_write_frame(ctx->muxer, NULL);

	if (!send_evt_only) {
		int res=0;

		if (ctx->ffiles) {
			if (ctx->status==FFMX_STATE_HDR_DONE) {
				res = av_write_trailer(ctx->muxer);
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Invalid state %d for segment %s close\n", ctx->status, AVFMT_URL(ctx->muxer) ));
				return GF_SERVICE_ERROR;
			}
		}

		if (res<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Fail to flush segment %s - error %s\n", AVFMT_URL(ctx->muxer), av_err2str(res) ));
			return GF_SERVICE_ERROR;
		}
		ctx->status = FFMX_STATE_TRAILER_DONE;
	}
	ctx->nb_pck_in_seg = 0;

	pid = gf_filter_get_ipid(filter, 0);
	GF_FEVT_INIT(evt, GF_FEVT_SEGMENT_SIZE, pid);
	evt.seg_size.seg_url = NULL;

	//init seg
	if (!ctx->init_done) {
		evt.seg_size.is_init = 1;
		ctx->init_done = GF_TRUE;
	} else {
		evt.seg_size.is_init = 0;
	}
	evt.seg_size.media_range_start = ctx->offset_at_seg_start;
	evt.seg_size.media_range_end = ctx->muxer->pb ? ctx->muxer->pb->written-1 : 0;
	ctx->offset_at_seg_start = evt.seg_size.media_range_end;

	gf_filter_pid_send_event(pid, &evt);
	return GF_OK;
}

void ffmx_inject_config(GF_FilterPid *pid, GF_FFMuxStream *st, AVPacket *pkt)
{
	const GF_PropertyValue *p;
	u8 *data;
	if (st->reconfig_stream & FFMX_INJECT_DSI) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (p && (p->type==GF_PROP_DATA) && p->value.data.ptr) {
			data = av_packet_new_side_data(pkt, AV_PKT_DATA_NEW_EXTRADATA, p->value.data.size);
			if (data)
				memcpy(data, p->value.data.ptr, p->value.data.size);
		}
	}
	if (st->reconfig_stream & FFMX_INJECT_VID_INFO) {
		u32 w, h;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
		w = p ? p->value.uint : st->stream->codecpar->width;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
		h = p ? p->value.uint : st->stream->codecpar->height;

		data = av_packet_new_side_data(pkt, AV_PKT_DATA_PARAM_CHANGE, 12);
		if (data) {
			GF_BitStream *bs = gf_bs_new(data, 12, GF_BITSTREAM_WRITE);
			gf_bs_write_u32_le(bs, AV_SIDE_DATA_PARAM_CHANGE_DIMENSIONS);
			gf_bs_write_u32_le(bs, w);
			gf_bs_write_u32_le(bs, h);
			gf_bs_del(bs);
		}
	}
	if (st->reconfig_stream & FFMX_INJECT_AUD_INFO) {
		u32 sr, ch, size=12;
		u64 ch_layout;
		u32 flags = AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_COUNT | AV_SIDE_DATA_PARAM_CHANGE_SAMPLE_RATE;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
		sr = p ? p->value.uint : st->stream->codecpar->sample_rate;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
		ch = p ? p->value.uint : st->stream->codecpar->channels;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_CHANNEL_LAYOUT);
		if (p) {
			ch_layout = ffmpeg_channel_layout_from_gpac(p->value.uint);
		} else {
			ch_layout = st->stream->codecpar->channel_layout;
		}
		if (ch_layout) {
			size += 8;
			flags |= AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_LAYOUT;
		}

		data = av_packet_new_side_data(pkt, AV_PKT_DATA_PARAM_CHANGE, size);
		if (data) {
			GF_BitStream *bs = gf_bs_new(data, 12, GF_BITSTREAM_WRITE);
			gf_bs_write_u32_le(bs, flags);
			gf_bs_write_u32_le(bs, ch);
			if (flags & AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_LAYOUT)
				gf_bs_write_u64_le(bs, ch_layout);
			gf_bs_write_u32_le(bs, sr);
			gf_bs_del(bs);
		}
	}
}

static GF_Err ffmx_process(GF_Filter *filter)
{
	GF_Err e = GF_OK;
	GF_FFMuxCtx *ctx = (GF_FFMuxCtx *) gf_filter_get_udta(filter);
	u32 nb_done, nb_segs_done, nb_suspended, i, nb_pids = gf_filter_get_ipid_count(filter);

	if (ctx->status<FFMX_STATE_HDR_DONE) {
		Bool all_ready = GF_TRUE;
		Bool needs_reinit = (ctx->status==FFMX_STATE_ALLOC) ? GF_TRUE : GF_FALSE;

		for (i=0; i<nb_pids; i++) {
			GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
			GF_FFMuxStream *st = gf_filter_pid_get_udta(ipid);
			GF_FilterPacket *ipck = gf_filter_pid_get_packet(ipid);

			if (needs_reinit) {
				e = ffmx_configure_pid(filter, ipid, GF_FALSE);
				if (e) return e;
			}
			if (!ipck) {
				all_ready = GF_FALSE;
				continue;
			}

			if (st->stream->codecpar->codec_type==AVMEDIA_TYPE_VIDEO) {
				u8 ilced = gf_filter_pck_get_interlaced(ipck);
				if (ilced==2) st->stream->codecpar->field_order = AV_FIELD_BB;
				else if (ilced==1) st->stream->codecpar->field_order = AV_FIELD_TT;
				else st->stream->codecpar->field_order = AV_FIELD_PROGRESSIVE;
			}
			else if (st->stream->codecpar->codec_type==AVMEDIA_TYPE_AUDIO) {
				st->stream->codecpar->seek_preroll = gf_filter_pck_get_roll_info(ipck);
			}
		}
		if (!all_ready || (ctx->status==FFMX_STATE_ALLOC)) return GF_OK;

		//good to go, finalize mux
		ffmx_init_mux(filter, ctx);
	}
	if (ctx->status==FFMX_STATE_EOS) return GF_EOS;
	else if (ctx->status==FFMX_STATE_ERROR) return GF_SERVICE_ERROR;

	nb_segs_done = 0;
	nb_done = 0;
	nb_suspended = 0;
	for (i=0; i<nb_pids; i++) {
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		GF_FFMuxStream *st = gf_filter_pid_get_udta(ipid);
		if (!st) continue;
		if (st->in_seg_flush) {
			nb_segs_done ++;
			continue;
		}
		if (st->suspended) {
			nb_suspended++;
			continue;
		}

		while (1) {
			u64 cts;
			int res;
			const GF_PropertyValue *p;
			u32 sap;
			AVPacket *pkt;
			GF_FilterPacket *ipck = gf_filter_pid_get_packet(ipid);
			if (!ipck) {
				if (gf_filter_pid_is_eos(ipid)) {
					nb_done++;
				}
				break;
			}
			cts = gf_filter_pck_get_cts(ipck);

			if (cts == GF_FILTER_NO_TS) {
				p = gf_filter_pck_get_property(ipck, GF_PROP_PCK_EODS);
				if (p && p->value.boolean) {
					st->in_seg_flush = GF_TRUE;
					nb_segs_done ++;
					nb_done++;
					gf_filter_pid_drop_packet(ipid);
					break;
				}
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Packet with no CTS assigned, cannot store to track, ignoring\n"));
				gf_filter_pid_drop_packet(ipid);
				continue;
			}

			p = gf_filter_pck_get_property(ipck, GF_PROP_PCK_FILENUM);
			if (ctx->dash_mode) {
				if (p) {
					//flush segment
					if (ctx->dash_seg_num && (p->value.uint != ctx->dash_seg_num)) {
						st->in_seg_flush = GF_TRUE;
						nb_segs_done++;
						break;
					}
					ctx->dash_seg_num = p->value.uint;
					//segment done, check if we switch file
					p = gf_filter_pck_get_property(ipck, GF_PROP_PCK_FILENAME);
					if (p && p->value.string) {
						ffmx_close_seg(filter, ctx, GF_FALSE);
						ffmx_start_seg(filter, ctx, p->value.string);
					}
					//otherwise close segment for event processing only
					else
						ffmx_close_seg(filter, ctx, GF_TRUE);
				}
			} else if (p) {
				if (ctx->cur_file_idx_plus_one == p->value.uint+1) {
				} else if (!st->suspended) {
					st->suspended = GF_TRUE;
					nb_suspended++;
					break;
				}
			}

			FF_INIT_PCK(ctx, pkt)

			pkt->stream_index = st->stream->index;
			pkt->dts = gf_filter_pck_get_dts(ipck);
			pkt->pts = gf_filter_pck_get_cts(ipck);
			if (st->cts_shift) pkt->pts += st->cts_shift;

			if (pkt->dts > pkt->pts) {
				st->cts_shift += (u32) (pkt->dts - pkt->pts);
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[FFMux] Negative CTS offset -%d found, adjusting offset\n", st->cts_shift));
				pkt->pts = pkt->dts;
			}
			pkt->duration = gf_filter_pck_get_duration(ipck);
			sap = gf_filter_pck_get_sap(ipck);
			if (sap==GF_FILTER_SAP_1) pkt->flags = AV_PKT_FLAG_KEY;
			pkt->data = (u8 *) gf_filter_pck_get_data(ipck, &pkt->size);

			if (st->ts_rescale) {
				av_packet_rescale_ts(pkt, st->in_scale, st->stream->time_base);
			}

			if (st->reconfig_stream) {
				ffmx_inject_config(ipid, st, pkt);
			}

			if (ctx->interleave) {
				res = av_interleaved_write_frame(ctx->muxer, pkt);
			} else {
				res = av_write_frame(ctx->muxer, pkt);
			}
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Fail to write packet %sto %s - error %s\n", st->reconfig_stream ? "with reconfig side data " : "", AVFMT_URL(ctx->muxer), av_err2str(res) ));
				e = GF_IO_ERR;
			}
			if (st->reconfig_stream) {
				av_packet_free_side_data(pkt);
				st->reconfig_stream = 0;
			}

			gf_filter_pid_drop_packet(ipid);
			ctx->nb_pck_in_seg++;
			FF_RELEASE_PCK(pkt)
		}
	}

	//done writing file
	if (nb_suspended && (nb_suspended==nb_pids)) {
		av_write_trailer(ctx->muxer);
		if (ctx->muxer)	avformat_free_context(ctx->muxer);
		ctx->muxer = NULL;
		ctx->status = FFMX_STATE_ALLOC;
		ffmx_initialize_ex(filter, GF_TRUE);
		for (i=0; i<nb_pids; i++) {
			GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
			GF_FFMuxStream *st = gf_filter_pid_get_udta(ipid);
			if (!st) continue;
			st->suspended = GF_FALSE;
			st->stream = 0;
			ffmx_configure_pid(filter, ipid, GF_FALSE);
		}
		nb_done = 0;
	}

	//done writing segment
	if (nb_segs_done==nb_pids) {
		ctx->dash_seg_num = 0;
		for (i=0; i<nb_pids; i++) {
			GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
			GF_FFMuxStream *st = gf_filter_pid_get_udta(ipid);
			if (!st) continue;
			st->in_seg_flush = GF_FALSE;
		}
	}

	if (e) return e;

	if (nb_done==nb_pids) {
		if (ctx->status==FFMX_STATE_HDR_DONE) {
			if (ctx->dash_mode) {
				ffmx_close_seg(filter, ctx, GF_FALSE);
			} else {
				av_write_trailer(ctx->muxer);
			}
			ctx->status = FFMX_STATE_EOS;
		}
		return GF_EOS;
	}
	return GF_OK;
}

static GF_Err ffmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	int res;
	AVStream *avst;
	Bool check_disc = GF_FALSE;
	u32 streamtype, codec_id;
	u32 ff_codec_id, ff_st, ff_codec_tag;
	const GF_PropertyValue *p, *dsi;
	GF_FFMuxStream *st;
	GF_FFMuxCtx *ctx = (GF_FFMuxCtx *) gf_filter_get_udta(filter);

	//disconnect of src pid (not yet supported)
	if (is_remove) {
		//one in one out, this is simple
		return GF_OK;
	}
	st = gf_filter_pid_get_udta(pid);
	if (st) {
		if (ctx->status >= FFMX_STATE_HDR_DONE)
			check_disc = GF_TRUE;
	} else {
		if (ctx->status>FFMX_STATE_HDR_DONE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Cannot dynamically add new stream to %s, not supported\n", ctx->dst ));
			return GF_NOT_SUPPORTED;
		}
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!p) return GF_NOT_SUPPORTED;
	streamtype = p->value.uint;

	//not supported yet
	if (streamtype==GF_STREAM_ENCRYPTED) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Cannot import protected stream, not supported\n"));
		return GF_NOT_SUPPORTED;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_NOT_SUPPORTED;
	codec_id = p->value.uint;

	ff_st = ffmpeg_stream_type_from_gpac(streamtype);
	if (codec_id==GF_CODECID_RAW) {
		ff_codec_tag = 0;
		if (streamtype==GF_STREAM_VISUAL) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
			if (!p) return GF_NOT_SUPPORTED;
			switch (p->value.uint) {
			default:
				ff_codec_id = AV_CODEC_ID_RAWVIDEO;
				break;
			}
		} else {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
			if (!p) return GF_NOT_SUPPORTED;

			switch (p->value.uint) {
			case GF_AUDIO_FMT_U8:
			case GF_AUDIO_FMT_U8P:
				ff_codec_id = AV_CODEC_ID_PCM_U8;
				break;
			case GF_AUDIO_FMT_S16:
			case GF_AUDIO_FMT_S16P:
				ff_codec_id = AV_CODEC_ID_PCM_S16LE;
				break;
			case GF_AUDIO_FMT_S16_BE:
				ff_codec_id = AV_CODEC_ID_PCM_S16BE;
				break;
			case GF_AUDIO_FMT_S24:
			case GF_AUDIO_FMT_S24P:
				ff_codec_id = AV_CODEC_ID_PCM_S24LE;
				break;
			case GF_AUDIO_FMT_S32:
			case GF_AUDIO_FMT_S32P:
				ff_codec_id = AV_CODEC_ID_PCM_S32LE;
				break;
			case GF_AUDIO_FMT_FLT:
			case GF_AUDIO_FMT_FLTP:
				ff_codec_id = AV_CODEC_ID_PCM_F32LE;
				break;
			default:
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Unmapped raw audio format %s to FFMPEG, patch welcome\n", gf_audio_fmt_name(p->value.uint) ));
				return GF_NOT_SUPPORTED;
			}
		}
	} else {
		ff_codec_id = ffmpeg_codecid_from_gpac(codec_id, &ff_codec_tag);
	}

	if (ctx->muxer->oformat && ctx->muxer->oformat->query_codec) {
		res = ctx->muxer->oformat->query_codec(ff_codec_id, 1);
		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Codec %s not supported in container %s\n", gf_codecid_name(codec_id), ctx->muxer->oformat->name));
			return GF_NOT_SUPPORTED;
		}
	}
	const AVCodec *c = avcodec_find_decoder(ff_codec_id);
	if (!c) return GF_NOT_SUPPORTED;

	if (!st) {
		GF_FilterEvent evt;

		GF_SAFEALLOC(st, GF_FFMuxStream);
		if (!st) return GF_OUT_OF_MEM;
		gf_filter_pid_set_udta(pid, st);
		gf_list_add(ctx->streams, st);

		gf_filter_pid_init_play_event(pid, &evt, ctx->start, ctx->speed, "FFMux");
		gf_filter_pid_send_event(pid, &evt);
	}

	if (ctx->status==FFMX_STATE_ALLOC) {
		char szPath[GF_MAX_PATH];
		const GF_PropertyValue *p;
		const char *file_suffix = NULL;
		GF_FilterPacket *pck = gf_filter_pid_get_packet(pid);
		if (!pck) {
			if (gf_filter_pid_is_eos(pid))
				return GF_SERVICE_ERROR;
			return GF_OK;
		}
		p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
		ctx->cur_file_idx_plus_one = p ? (p->value.uint + 1) : 1;

		p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILESUF);
		if (p && p->value.string) file_suffix = p->value.string;

		gf_filter_pid_resolve_file_template(pid, ctx->dst, szPath, ctx->cur_file_idx_plus_one-1, file_suffix);
		ffmx_open_url(ctx, szPath);
	}


	if (!st->stream) {
		st->stream = avformat_new_stream(ctx->muxer, c);
		if (!st->stream) return GF_NOT_SUPPORTED;
	}
	avst = st->stream;

	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);

	if (check_disc) {
		u32 dsi_crc, old_dsi_crc;
		if ((avst->codecpar->codec_id != ff_codec_id) || (avst->codecpar->codec_type != ff_st)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Dynamic stream update in mux %s not supported\n", ctx->dst));
			return GF_NOT_SUPPORTED;
		}

		dsi_crc = old_dsi_crc = 0;
		if (avst->codecpar->extradata) old_dsi_crc = gf_crc_32(avst->codecpar->extradata, avst->codecpar->extradata_size);
		if (dsi) dsi_crc = gf_crc_32(dsi->value.data.ptr, dsi->value.data.size);

		st->reconfig_stream = 0;
		if (dsi_crc != old_dsi_crc)
			st->reconfig_stream |= FFMX_INJECT_DSI;
		if (streamtype==GF_STREAM_VISUAL) {
			u32 w, h;
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
			w = p ? p->value.uint : 0;
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
			h = p ? p->value.uint : 0;
			if ((w != avst->codecpar->width) || (h != avst->codecpar->height))
				st->reconfig_stream |= FFMX_INJECT_VID_INFO;
		} else if (streamtype==GF_STREAM_AUDIO) {
			u32 sr, ch;
			u64 ch_layout;
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
			sr = p ? p->value.uint : 0;
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
			ch = p ? p->value.uint : 0;
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_CHANNEL_LAYOUT);
			ch_layout = p ? p->value.uint : 0;
			if (ch_layout)
				ch_layout = ffmpeg_channel_layout_from_gpac(ch_layout);
			if ((sr != avst->codecpar->sample_rate) || (ch != avst->codecpar->channels) || (ch_layout!=avst->codecpar->channel_layout))
				st->reconfig_stream |= FFMX_INJECT_AUD_INFO;
		}

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
		if (p) {
			st->in_scale.den = p->value.uint;
			st->in_scale.num = 1;
		}
		return GF_OK;
	}

	avst->codecpar->codec_id = ff_codec_id;
	avst->codecpar->codec_type = ff_st;
	avst->codecpar->codec_tag = ff_codec_tag;

	if (dsi && dsi->value.data.ptr) {
		if (avst->codecpar->extradata) av_free(avst->codecpar->extradata);
		avst->codecpar->extradata_size = dsi->value.data.size;
		avst->codecpar->extradata = av_malloc(avst->codecpar->extradata_size+ AV_INPUT_BUFFER_PADDING_SIZE);
		if (avst->codecpar->extradata) {
			memcpy(avst->codecpar->extradata, dsi->value.data.ptr, avst->codecpar->extradata_size);
			memset(avst->codecpar->extradata + dsi->value.data.size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
		}
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	if (p) avst->id = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) {
		avst->time_base.den = p->value.uint;
		avst->time_base.num = 1;
	}
	st->in_scale = avst->time_base;

	avst->start_time = 0;
	avst->duration = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DURATION);
	if (p && p->value.lfrac.den) {
		avst->duration = p->value.lfrac.num;
		avst->duration *= avst->time_base.den;
		avst->duration /= p->value.lfrac.den;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_BITRATE);
	if (p) avst->codecpar->bit_rate = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CTS_SHIFT);
	if (p) st->cts_shift = p->value.uint;

	if (streamtype==GF_STREAM_VISUAL) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
		if (p) avst->codecpar->width = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
		if (p) avst->codecpar->height = p->value.uint;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
		if (p) {
			avst->r_frame_rate.num = p->value.frac.num;
			avst->r_frame_rate.den = p->value.frac.den;
			avst->avg_frame_rate = avst->r_frame_rate;
		}
		if (codec_id==GF_CODECID_RAW) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
			if (p) {
				avst->codecpar->format = ffmpeg_pixfmt_from_gpac(p->value.uint);
				avst->codecpar->codec_tag = avcodec_pix_fmt_to_codec_tag(avst->codecpar->format);
			}
		}

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAR);
		if (p) {
			avst->codecpar->sample_aspect_ratio.num = p->value.frac.num;
			avst->codecpar->sample_aspect_ratio.den = p->value.frac.den;
		}
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_COLR_PRIMARIES);
		if (p) avst->codecpar->color_primaries = p->value.uint;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_COLR_RANGE);
		if (p) avst->codecpar->color_range = (p->value.uint==1) ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_COLR_TRANSFER);
		if (p) avst->codecpar->color_trc = p->value.uint;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_COLR_MX);
		if (p) avst->codecpar->color_space = p->value.uint;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_COLR_CHROMALOC);
		if (p) avst->codecpar->chroma_location = p->value.uint;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
		if (p && avst->r_frame_rate.num && avst->r_frame_rate.den) {
			s64 delay = p->value.longsint;
			delay *= avst->r_frame_rate.num;
			delay /= avst->r_frame_rate.den;
			avst->codecpar->video_delay = (s32) delay;
		}

	}
	else if (streamtype==GF_STREAM_AUDIO) {
		u64 ch_layout;
		u32 samplerate=0;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
		if (p) avst->codecpar->sample_rate = samplerate = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
		if (p) avst->codecpar->channels = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLES_PER_FRAME);
		if (p) avst->codecpar->frame_size = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_BPS);
		if (p) avst->codecpar->bits_per_raw_sample = p->value.uint;
		if (codec_id==GF_CODECID_RAW) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
			if (p) avst->codecpar->format =  ffmpeg_audio_fmt_from_gpac(p->value.uint);
		}

		ch_layout = AV_CH_LAYOUT_MONO;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_CHANNEL_LAYOUT);
		if (p)
			ch_layout = p->value.longuint;
		else if (avst->codecpar->channels==2)
			ch_layout = AV_CH_LAYOUT_STEREO;
		avst->codecpar->channel_layout = ffmpeg_channel_layout_from_gpac(ch_layout);

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
		if (p && (p->value.sint<0) && samplerate) {
			s64 pad = p->value.longsint;
			if (st->in_scale.den != samplerate) {
				pad *= samplerate;
				pad /= st->in_scale.den;
			}
			avst->codecpar->initial_padding = (s32) pad;
		}
		/*
		//not mapped in gpac
		int trailing_padding;
		*/
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DASH_MODE);
	if (p && (p->value.uint==1)) {
		ctx->dash_mode = GF_TRUE;
	}


	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	return GF_OK;
}

static void ffmx_finalize(GF_Filter *filter)
{
	GF_FFMuxCtx *ctx = (GF_FFMuxCtx *) gf_filter_get_udta(filter);

	if (ctx->status==FFMX_STATE_HDR_DONE) {
		if (ctx->dash_mode) {
			ffmx_close_seg(filter, ctx, GF_FALSE);
		} else {
			av_write_trailer(ctx->muxer);
		}
		ctx->status = FFMX_STATE_TRAILER_DONE;
	} 
	if (!ctx->gfio && ctx->muxer && ctx->muxer->pb) {
		ctx->muxer->io_close(ctx->muxer, ctx->muxer->pb);
	}

#if (LIBAVCODEC_VERSION_MAJOR >= 59)
	av_packet_free(&ctx->pkt);
#endif

	if (ctx->options) av_dict_free(&ctx->options);
	if (ctx->muxer)	avformat_free_context(ctx->muxer);
	while (gf_list_count(ctx->streams)) {
		GF_FFMuxStream *st = gf_list_pop_back(ctx->streams);
		gf_free(st);
	}
	gf_list_del(ctx->streams);
	if (ctx->avio_ctx) {
		if (ctx->avio_ctx->buffer) av_freep(&ctx->avio_ctx->buffer);
		av_freep(&ctx->avio_ctx);
	}
	if (ctx->gfio) gf_fclose(ctx->gfio);
	return;
}

static GF_Err ffmx_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *arg_val)
{
	s32 res;
	GF_FFMuxCtx *ctx = gf_filter_get_udta(filter);

	//initial parsing of arguments
	if (!ctx->muxer) {
		switch (arg_val->type) {
		case GF_PROP_STRING:
			res = av_dict_set(&ctx->options, arg_name, arg_val->value.string, 0);
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] Failed to set option %s:%s\n", arg_name, arg_val ));
			}
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] Failed to set option %s:%s, unrecognized type %d\n", arg_name, arg_val, arg_val->type ));
			return GF_NOT_SUPPORTED;
		}
		return GF_OK;
	}
	//updates of arguments, not supported for ffmpeg decoders
	return GF_NOT_SUPPORTED;
}

static GF_FilterProbeScore ffmx_probe_url(const char *url, const char *mime)
{
	const char *proto;
	if (url && !strncmp(url, "gfio://", 7)) {
		url = gf_fileio_translate_url(url);
	}
	if (!url)
		return GF_FPROBE_NOT_SUPPORTED;


	const AVOutputFormat *ofmt = av_guess_format(NULL, url, mime);
	if (!ofmt && mime) ofmt = av_guess_format(NULL, NULL, mime);
	if (!ofmt && url) ofmt = av_guess_format(NULL, url, NULL);

	if (ofmt) return GF_FPROBE_SUPPORTED;

	proto = strstr(url, "://");
	if (!proto)
		return GF_FPROBE_NOT_SUPPORTED;

	proto = avio_find_protocol_name(url);
	if (proto)
		return GF_FPROBE_MAYBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

static const GF_FilterCapability FFMuxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),

	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),

#ifdef FF_SUB_SUPPORT
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
#endif
};

GF_FilterRegister FFMuxRegister = {
	.name = "ffmx",
	.version = LIBAVFORMAT_IDENT,
	GF_FS_SET_DESCRIPTION("FFMPEG muxer")

	GF_FS_SET_HELP("Mulitiplexes files and open output protocols using FFMPEG.\n"
		"See FFMPEG documentation (https://ffmpeg.org/documentation.html) for more details.\n"
		"To list all supported muxers for your GPAC build, use `gpac -h ffmx:*`."
		"This will list both supported output formats and protocols.\n"
		"Output protocols are listed with `Description: Output protocol`, and the subclass name identitfes the protocol scheme.\n"
		"For example, if `ffmx:rtmp` is listed as output protocol, this means `rtmp://` destination URLs are supported.\n"
		"\n"
		"Some URL formats may not be sufficient to derive the multiplexing format, you must then use [-ffmt]() to specify the desired format.\n"
		"\n"
		"Unlike other multiplexing filters in GPAC, this filter is a sink filter and does not produce any PID to be redirected in the graph.\n"
		"The filter can however use template names for its output, using the first input PID to resolve the final name.\n"
		"The filter watches the property `FileNumber` on incoming packets to create new files.\n"
	)
	.private_size = sizeof(GF_FFMuxCtx),
	SETCAPS(FFMuxCaps),
	.initialize = ffmx_initialize,
	.finalize = ffmx_finalize,
	.configure_pid = ffmx_configure_pid,
	.process = ffmx_process,
	.update_arg = ffmx_update_arg,
	.probe_url = ffmx_probe_url,
	.flags = GF_FS_REG_META,
	.max_extra_pids = (u32) -1,


	//use lowest priorty, so that we still use our default built-in muxers
	.priority = 255
};

#define OFFS(_n)	#_n, offsetof(GF_FFMuxCtx, _n)


static const GF_FilterArgs FFMuxArgs[] =
{
	{ OFFS(dst), "location of destination file or remote URL", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(start), "set playback start offset. Negative value means percent of media duration with -1 equal to duration", GF_PROP_DOUBLE, "0.0", NULL, 0},
	{ OFFS(speed), "set playback speed. If speed is negative and start is 0, start is set to -1", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{ OFFS(interleave), "write frame in interleave mode", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(nodisc), "ignore stream configuration changes while muxing, may result in broken streams", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mime), "set mime type for graph resolution", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(ffiles), "force complete files to be created for each segment in DASH modes", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(ffmt), "force ffmpeg output format for the given URL", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(block_size), "block size used to read file when using avio context", GF_PROP_UINT, "4096", NULL, GF_FS_ARG_HINT_EXPERT},
	{ "*", -1, "any possible options defined for AVFormatContext and sub-classes. See `gpac -hx ffmx` and `gpac -hx ffmx:*`", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_META},
	{0}
};

const int FFMX_STATIC_ARGS = (sizeof (FFMuxArgs) / sizeof (GF_FilterArgs)) - 1;

const GF_FilterRegister *ffmx_register(GF_FilterSession *session)
{
	ffmpeg_build_register(session, &FFMuxRegister, FFMuxArgs, FFMX_STATIC_ARGS, FF_REG_TYPE_MUX);
	return &FFMuxRegister;
}

#else
#include <gpac/filters.h>
const GF_FilterRegister *ffmx_register(GF_FilterSession *session)
{
	return NULL;
}
#endif

