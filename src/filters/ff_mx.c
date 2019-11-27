/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom Paris 2019
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

typedef struct
{
	AVStream *stream;
	Bool ts_rescale;
	AVRational in_scale;
	Bool in_seg_flush;
} GF_FFMuxStream;

typedef struct
{
	//options
	char *dst;
	Double start, speed;
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
} GF_FFMuxCtx;

static GF_Err ffmx_init_mux(GF_Filter *filter, GF_FFMuxCtx *ctx)
{
	u32 i, nb_pids;
	int res;

	ctx->status = 1;
	res = avformat_init_output(ctx->muxer, &ctx->options);

	if (res<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Fail to open %s - error %s\n", ctx->dst, av_err2str(res) ));
		ctx->status=4;
		return GF_NOT_SUPPORTED;
	}

	AVDictionaryEntry *prev_e = NULL;
	while (1) {
		prev_e = av_dict_get(ctx->options, "", prev_e, AV_DICT_IGNORE_SUFFIX);
		if (!prev_e) break;
		gf_filter_report_unused_meta_option(filter, prev_e->key);
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

	res = avformat_write_header(ctx->muxer, NULL);
	if (res<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Fail to write %s header - error %s\n", ctx->dst, av_err2str(res) ));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	return GF_OK;
}

static GF_Err ffmx_initialize(GF_Filter *filter)
{
	AVOutputFormat *ofmt;
	GF_FFMuxCtx *ctx = (GF_FFMuxCtx *) gf_filter_get_udta(filter);

	ffmpeg_setup_logs(GF_LOG_CONTAINER);

	ctx->muxer = avformat_alloc_context();
	if (!ctx->muxer) return GF_OUT_OF_MEM;

	ofmt = av_guess_format(NULL, ctx->dst, gf_filter_get_target_mime(filter));
	if (!ofmt) return GF_FILTER_NOT_SUPPORTED;

	ctx->muxer->oformat = ofmt;
	if (!(ofmt->flags & AVFMT_NOFILE) ) {
		//handle local urls some_dir/some_other_dir/file.ext, ffmpeg doesn't create the dirs for us
		if (gf_url_is_local(ctx->dst)) {
			FILE *f = gf_fopen(ctx->dst, "wb");
			gf_fclose(f);
		}
		avio_open(&ctx->muxer->pb, ctx->dst, AVIO_FLAG_WRITE);
	}
	ctx->streams = gf_list_new();
	return GF_OK;
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

		for (i=0; i< ctx->muxer->nb_streams; i++) {
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

			if (ctx->muxer->streams[i]->codecpar->codec_tag == MKTAG('t','m','c','d'))
				st->codec->time_base = ctx->muxer->streams[i]->codec->time_base;

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

	av_freep(&ctx->muxer->url);
	ctx->muxer->url = av_strdup(seg_name);
	strncpy(ctx->muxer->filename, seg_name, 1023);
	ctx->offset_at_seg_start = 0;

	if (!(ctx->muxer->oformat->flags & AVFMT_NOFILE)) {
		//handle local urls some_dir/some_other_dir/file.ext, ffmpeg doesn't create the dirs for us
		if (gf_url_is_local(seg_name)) {
			FILE *f = gf_fopen(seg_name, "wb");
			gf_fclose(f);
		}

		res = ctx->muxer->io_open(ctx->muxer, &ctx->muxer->pb, ctx->muxer->url, AVIO_FLAG_WRITE, NULL);
		if (res < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Fail to open new segment %s - error %s\n", ctx->muxer->url, av_err2str(res) ));
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
        av_dict_free(&options);
        if (res < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Fail to configure segment %s - error %s\n", ctx->muxer->url, av_err2str(res) ));
			return GF_IO_ERR;
		}
    }
	ctx->status = 1;
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
			if (ctx->status==1) {
				res = av_write_trailer(ctx->muxer);
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Invalid state %d for segment %s close\n", ctx->status, ctx->muxer->url));
				return GF_SERVICE_ERROR;
			}
		}

		if (res<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Fail to flush segment %s - error %s\n", ctx->muxer->url, av_err2str(res) ));
			return GF_SERVICE_ERROR;
		}
		ctx->status = 2;
	}
	ctx->nb_pck_in_seg = 0;

	pid = gf_filter_get_ipid(filter, 0);
	GF_FEVT_INIT(evt, GF_FEVT_SEGMENT_SIZE, pid);
	evt.seg_size.seg_url = NULL;

	//init seg
	if (!ctx->init_done) {
		evt.seg_size.is_init = GF_TRUE;
		ctx->init_done = GF_TRUE;
	} else {
		evt.seg_size.is_init = GF_FALSE;
	}
	evt.seg_size.media_range_start = ctx->offset_at_seg_start;
	evt.seg_size.media_range_end = ctx->muxer->pb ? ctx->muxer->pb->written : 0;
	ctx->offset_at_seg_start = evt.seg_size.media_range_end;

	gf_filter_pid_send_event(pid, &evt);
	return GF_OK;
}

static GF_Err ffmx_process(GF_Filter *filter)
{
	GF_Err e = GF_OK;
	GF_FFMuxCtx *ctx = (GF_FFMuxCtx *) gf_filter_get_udta(filter);
	u32 nb_done, nb_segs_done, i, nb_pids = gf_filter_get_ipid_count(filter);

	if (!ctx->status) {
		for (i=0; i<nb_pids; i++) {
			GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
			GF_FFMuxStream *st = gf_filter_pid_get_udta(ipid);
			GF_FilterPacket *ipck = gf_filter_pid_get_packet(ipid);
			if (!ipck) return GF_OK;

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
		//good to go, finalize mux
		ffmx_init_mux(filter, ctx);
	}
	if (ctx->status==3) return GF_EOS;
	else if (ctx->status==4) return GF_SERVICE_ERROR;

	nb_segs_done = 0;
	nb_done = 0;
	for (i=0; i<nb_pids; i++) {
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		GF_FFMuxStream *st = gf_filter_pid_get_udta(ipid);
		if (!st) continue;
		if (st->in_seg_flush) {
			nb_segs_done ++;
			continue;
		}

		while (1) {
			u64 cts;
			int res;
			const GF_PropertyValue *p;
			u32 sap;
			AVPacket ffpck;
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

			if (ctx->dash_mode) {
				p = gf_filter_pck_get_property(ipck, GF_PROP_PCK_FILENUM);
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
			}

			av_init_packet(&ffpck);
			ffpck.stream_index = st->stream->index;
			ffpck.dts = gf_filter_pck_get_dts(ipck);
			ffpck.pts = gf_filter_pck_get_cts(ipck);
			ffpck.duration = gf_filter_pck_get_duration(ipck);
			sap = gf_filter_pck_get_sap(ipck);
			if (sap==GF_FILTER_SAP_1) ffpck.flags = AV_PKT_FLAG_KEY;
			ffpck.data = (u8 *) gf_filter_pck_get_data(ipck, &ffpck.size);

			if (st->ts_rescale) {
				av_packet_rescale_ts(&ffpck, st->in_scale, st->stream->time_base);
			}

			if (ctx->interleave) {
				res = av_interleaved_write_frame(ctx->muxer, &ffpck);
			} else {
				res = av_write_frame(ctx->muxer, &ffpck);
			}
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Fail to write packet to %s - error %s\n", ctx->muxer->url, av_err2str(res) ));
				e = GF_IO_ERR;
			}

			gf_filter_pid_drop_packet(ipid);
			ctx->nb_pck_in_seg++;
		}
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
		if (ctx->status==1) {
			if (ctx->dash_mode) {
				ffmx_close_seg(filter, ctx, GF_FALSE);
			} else {
				av_write_trailer(ctx->muxer);
			}
			ctx->status = 3;
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
		if (ctx->status)
			check_disc = GF_TRUE;
	} else {
		if (ctx->status) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Cannot dynamically add new stream to %s, not supported\n", ctx->dst ));
			return GF_NOT_SUPPORTED;
		}
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!p) return GF_NOT_SUPPORTED;
	streamtype = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_NOT_SUPPORTED;
	codec_id = p->value.uint;

	ff_st = ffmpeg_stream_type_from_gpac(streamtype);
	ff_codec_id = ffmpeg_codecid_from_gpac(codec_id, &ff_codec_tag);


	if (ctx->muxer->oformat->query_codec) {
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
		st->stream = avformat_new_stream(ctx->muxer, c);
		if (!st->stream) return GF_NOT_SUPPORTED;
		gf_filter_pid_set_udta(pid, st);
		gf_list_add(ctx->streams, st);

		gf_filter_pid_init_play_event(pid, &evt, ctx->start, ctx->speed, "FFMux");
		gf_filter_pid_send_event(pid, &evt);
	}
	avst = st->stream;

	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);

	if (check_disc) {
		u32 dsi_crc, old_dsi_crc;
		Bool is_ok = GF_TRUE;
		if (avst->codecpar->codec_id != ff_codec_id) is_ok = GF_FALSE;
		else if (avst->codecpar->codec_type != ff_st) is_ok = GF_FALSE;

		dsi_crc = old_dsi_crc = 0;
		if (avst->codecpar->extradata) old_dsi_crc = gf_crc_32(avst->codecpar->extradata, avst->codecpar->extradata_size);
		if (dsi) dsi_crc = gf_crc_32(dsi->value.data.ptr, dsi->value.data.size);

		if (dsi_crc != old_dsi_crc) is_ok = GF_FALSE;
		if (!is_ok) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMux] Dynamic stream update in mux %s not supported\n", ctx->dst));
			return GF_NOT_SUPPORTED;
		}
	}

	avst->codecpar->codec_id = ff_codec_id;
	avst->codecpar->codec_type = ff_st;
	avst->codecpar->codec_tag = ff_codec_tag;

	if (dsi) {
		if (avst->codecpar->extradata) av_free(avst->codecpar->extradata);
		avst->codecpar->extradata_size = dsi->value.data.size;
		avst->codecpar->extradata = av_malloc(avst->codecpar->extradata_size+ AV_INPUT_BUFFER_PADDING_SIZE);
		memcpy(avst->codecpar->extradata, dsi->value.data.ptr, avst->codecpar->extradata_size);
		memset(avst->codecpar->extradata + dsi->value.data.size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
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
	if (p) {
		avst->duration = p->value.lfrac.num;
		avst->duration *= avst->time_base.den;
		avst->duration /= p->value.lfrac.den;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_BITRATE);
	if (p) avst->codecpar->bit_rate = p->value.uint;

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
			if (p) avst->codecpar->format =  ffmpeg_pixfmt_from_gpac(p->value.uint);
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
			avst->codecpar->video_delay = p->value.sint;
			avst->codecpar->video_delay *= avst->r_frame_rate.num;
			avst->codecpar->video_delay /= avst->r_frame_rate.den;
		}

	}
	else if (streamtype==GF_STREAM_AUDIO) {
		u32 ch_layout, samplerate=0;

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
			ch_layout = p->value.uint;
		else if (avst->codecpar->channels==2)
			ch_layout = AV_CH_LAYOUT_STEREO;
		avst->codecpar->channel_layout = ffmpeg_channel_layout_from_gpac(ch_layout);

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
		if (p && (p->value.sint<0) && samplerate) {
			avst->codecpar->initial_padding = p->value.sint;
			if (st->in_scale.den != samplerate) {
				avst->codecpar->initial_padding *= samplerate;
				avst->codecpar->initial_padding /= st->in_scale.den;
			}
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

	if (ctx->status==1) {
		if (ctx->dash_mode) {
			ffmx_close_seg(filter, ctx, GF_FALSE);
		} else {
			av_write_trailer(ctx->muxer);
		}
		ctx->status = 2;
	}

	if (ctx->options) av_dict_free(&ctx->options);
	if (ctx->muxer)	avformat_free_context(ctx->muxer);
	while (gf_list_count(ctx->streams)) {
		GF_FFMuxStream *st = gf_list_pop_back(ctx->streams);
		gf_free(st);
	}
	gf_list_del(ctx->streams);
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
	AVOutputFormat *ofmt = av_guess_format(NULL, url, mime);
	if (!ofmt) return GF_FPROBE_NOT_SUPPORTED;
	return GF_FPROBE_SUPPORTED;
}

static const GF_FilterCapability FFMuxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),

	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_STATIC_OPT, 	GF_PROP_PID_DASH_MODE, 0),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),

#ifdef FF_SUB_SUPPORT
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
#endif
};

GF_FilterRegister FFMuxRegister = {
	.name = "ffmx",
	.version = LIBAVFORMAT_IDENT,
	GF_FS_SET_DESCRIPTION("FFMPEG muxer")
	GF_FS_SET_HELP("See FFMPEG documentation (https://ffmpeg.org/documentation.html) for more detailed info on muxer options")
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

#define BUILTIN_ARGS	7

static const GF_FilterArgs FFMuxArgs[] =
{
	{ OFFS(dst), "location of source content", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(start), "set playback start offset. Negative value means percent of media dur with -1 <=> dur", GF_PROP_DOUBLE, "0.0", NULL, 0},
	{ OFFS(speed), "set playback speed when vsync is on. If speed is negative and start is 0, start is set to -1", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{ OFFS(interleave), "write frame in interleave mode", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(nodisc), "ignore stream configuration changes while muxing, may result in broken streams", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(ffiles), "force complete files to be created for each segment in DASH modes", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ "*", -1, "any possible args defined for AVCodecContext and sub-classes. See `gpac -hx ffdec` and `gpac -hx ffdec:*`", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_META},
	{0}
};

void ffmx_regfree(GF_FilterSession *session, GF_FilterRegister *reg)
{
	ffmpeg_register_free(session, reg, BUILTIN_ARGS-1);
}

const GF_FilterRegister *ffmx_register(GF_FilterSession *session)
{
	GF_FilterArgs *args;
	u32 i=0, idx=0;
	Bool load_meta_filters = session ? GF_TRUE : GF_FALSE;
	AVFormatContext *mx_ctx;
	const struct AVOption *opt;

	ffmpeg_initialize();

	//by default no need to load option descriptions, everything is handled by av_set_opt in update_args
	if (!load_meta_filters) {
		FFMuxRegister.args = FFMuxArgs;
		FFMuxRegister.register_free = NULL;
		return &FFMuxRegister;
	}

	FFMuxRegister.register_free = ffmx_regfree;
	mx_ctx = avformat_alloc_context();

	idx=0;
	while (mx_ctx->av_class->option) {
		opt = &mx_ctx->av_class->option[idx];
		if (!opt || !opt->name) break;
		if (opt->flags & AV_OPT_FLAG_ENCODING_PARAM)
			i++;
		idx++;
	}
	i+=BUILTIN_ARGS;

	args = gf_malloc(sizeof(GF_FilterArgs)*(i+1));
	memset(args, 0, sizeof(GF_FilterArgs)*(i+1));
	FFMuxRegister.args = args;

	args[0] = (GF_FilterArgs){ OFFS(dst), "location of destination content", GF_PROP_STRING, NULL, NULL, 0} ;
	args[1] = (GF_FilterArgs){ OFFS(start), "set playback start offset. Negative value means percent of media dur with -1 <=> dur", GF_PROP_DOUBLE, "0.0", NULL, 0} ;
	args[2] = (GF_FilterArgs) { OFFS(speed), "set playback speed when vsync is on. If speed is negative and start is 0, start is set to -1", GF_PROP_DOUBLE, "1.0", NULL, GF_FS_ARG_HINT_EXPERT} ;


	i=0;
	idx=0;
	while (mx_ctx->av_class->option) {
		opt = &mx_ctx->av_class->option[idx];
		if (!opt || !opt->name) break;
		if (opt->flags & AV_OPT_FLAG_ENCODING_PARAM) {
			args[i + BUILTIN_ARGS - 1] = ffmpeg_arg_translate(opt);
			i++;
		}
		idx++;
	}
	args[i+BUILTIN_ARGS] = (GF_FilterArgs) { "*", -1, "options depend on codec type, check individual filter syntax", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT};

#if (LIBAVCODEC_VERSION_MAJOR >= 58) && (LIBAVCODEC_VERSION_MINOR>=20)
	avformat_free_context(mx_ctx);
#else
	av_free(mx_ctx);
#endif

	ffmpeg_expand_register(session, &FFMuxRegister, FF_REG_TYPE_MUX);

	return &FFMuxRegister;
}

#else
#include <gpac/filters.h>
const GF_FilterRegister *ffmx_register(GF_FilterSession *session)
{
	return NULL;
}
#endif

