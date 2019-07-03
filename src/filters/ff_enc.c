/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / ffmpeg encode filter
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
#include <gpac/bitstream.h>
#include <gpac/avparse.h>

#ifdef GPAC_HAS_FFMPEG

#include "ff_common.h"

#define ENC_BUF_ALLOC_SAFE	10000

typedef struct _gf_ffenc_ctx
{
	//opts
	Bool all_intra;
	char *c, *ffc;
	Bool ls;
	u32 pfmt;
	GF_Fraction fintra;

	//internal data
	Bool initialized;

	u32 gop_size;

	AVCodecContext *encoder;
	//decode options
	AVDictionary *options;

	GF_FilterPid *in_pid, *out_pid;
	//media type
	u32 type;
	u32 timescale;

	u32 nb_frames_out, nb_frames_in;
	u64 time_spent;

	Bool low_delay;

	GF_Err (*process)(GF_Filter *filter, struct _gf_ffenc_ctx *ctx);
	//gpac one
	u32 codecid;
	//done flushing encoder (eg sent NULL frames)
	u32 flush_done;
	//frame used by both video and audio encoder
	AVFrame *frame;

	//encoding buffer - we allocate ENC_BUF_ALLOC_SAFE+WxH for the video (some image codecs in ffmpeg require more than WxH for headers), ENC_BUF_ALLOC_SAFE+nb_ch*samplerate for the audio
	//this should be enough to hold any lossless compression formats
	char *enc_buffer;
	u32 enc_buffer_size;

	Bool init_cts_setup;

	//video state
	u32 width, height, stride, stride_uv, nb_planes, uv_height;
	//ffmpeg one
	enum AVPixelFormat pixel_fmt;
	u64 cts_first_frame_plus_one;

	//audio state
	u32 channels, sample_rate, channel_layout, bytes_per_sample;
	//ffmpeg one
	u32 sample_fmt;
	//we store input audio frame in this buffer untill we have enough data for one encoder frame
	//we also store the remaining of a consumed frame here, so that input packet is realeased ASAP
	char *audio_buffer;
	u32 audio_buffer_size;
	u32 samples_in_audio_buffer;
	//cts of first byte in frame
	u64 first_byte_cts;
	Bool planar_audio;

	//shift of TS - ffmpeg may give pkt-> PTS < frame->PTS to indicate discard samples
	//we convert back to frame PTS but signal discard samples at the PID level
	s32 ts_shift;

	GF_List *src_packets;

	GF_BitStream *sdbs;

	Bool reconfig_pending;
	Bool infmt_negociate;
	Bool remap_ts;

	u32 dsi_crc;

	u32 gpac_pixel_fmt;
	u32 gpac_audio_fmt;

	Bool fintra_setup;
	u64 orig_ts;
	u32 nb_forced;

} GF_FFEncodeCtx;

static GF_Err ffenc_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove);

static GF_Err ffenc_initialize(GF_Filter *filter)
{
	GF_FFEncodeCtx *ctx = (GF_FFEncodeCtx *) gf_filter_get_udta(filter);
	ctx->initialized = GF_TRUE;
	ctx->src_packets = gf_list_new();
	ctx->sdbs = gf_bs_new((u8*)ctx, 1, GF_BITSTREAM_READ);

	ffmpeg_setup_logs(GF_LOG_CODEC);
	return GF_OK;
}

static void ffenc_finalize(GF_Filter *filter)
{
	GF_FFEncodeCtx *ctx = (GF_FFEncodeCtx *) gf_filter_get_udta(filter);
	if (ctx->options) av_dict_free(&ctx->options);
	if (ctx->frame) av_frame_free(&ctx->frame);
	if (ctx->enc_buffer) gf_free(ctx->enc_buffer);
	if (ctx->audio_buffer) gf_free(ctx->audio_buffer);

	while (gf_list_count(ctx->src_packets)) {
		GF_FilterPacket *pck = gf_list_pop_back(ctx->src_packets);
		gf_filter_pck_unref(pck);
	}
	gf_list_del(ctx->src_packets);

	if (ctx->encoder) {
		avcodec_close(ctx->encoder);
	}
	if (ctx->sdbs) gf_bs_del(ctx->sdbs);
	return;
}

//TODO add more feedback
static void ffenc_log_video(GF_Filter *filter, struct _gf_ffenc_ctx *ctx, AVPacket *pkt, Bool do_reporting)
{
	Double fps=0;
	u64 errors[10];
	u32 i;
	s32 q=-1;
	u8 pictype=0;
	u8 nb_errors = 0;
	const char *ptype;

	if (!ctx->ls && !do_reporting) return;


#if LIBAVCODEC_VERSION_MAJOR >= 58
	u32 sq_size;
	u8 *side_q = av_packet_get_side_data(pkt, AV_PKT_DATA_QUALITY_STATS, &sq_size);
	if (side_q) {
		gf_bs_reassign_buffer(ctx->sdbs, side_q, sq_size);
		q = gf_bs_read_u32_le(ctx->sdbs);
		pictype = gf_bs_read_u8(ctx->sdbs);
		nb_errors = gf_bs_read_u8(ctx->sdbs);
		/*res*/gf_bs_read_u16(ctx->sdbs);
		if (nb_errors>10) nb_errors = 10;
		for (i=0; i<nb_errors; i++) {
			errors[i] = gf_bs_read_u64_le(ctx->sdbs);
		}
	}
#endif
	if (ctx->time_spent) {
		fps = ctx->nb_frames_out;
		fps *= 1000000;
		fps /= ctx->time_spent;
	}
	switch (pictype) {
	case AV_PICTURE_TYPE_I: ptype = "I"; break;
	case AV_PICTURE_TYPE_P: ptype = "P"; break;
	case AV_PICTURE_TYPE_S: ptype = "S"; break;
	case AV_PICTURE_TYPE_SP: ptype = "SP"; break;
	case AV_PICTURE_TYPE_B: ptype = "B"; break;
	case AV_PICTURE_TYPE_BI: ptype = "B"; break;
	default: ptype = "U"; break;
	}

	if (ctx->ls) {
		fprintf(stderr, "[FFEnc] FPS %.02f F %d DTS "LLD" CTS "LLD" Q %02.02f PT %s (F_in %d)", fps, ctx->nb_frames_out, pkt->dts+ctx->ts_shift, pkt->pts+ctx->ts_shift, ((Double)q) /  FF_QP2LAMBDA, ptype, ctx->nb_frames_in);
		if (nb_errors) {
			fprintf(stderr, "PSNR");
			for (i=0; i<nb_errors; i++) {
				Double psnr = (Double) errors[i];
				psnr /= ctx->width * ctx->height * 255.0 * 255.0;
				fprintf(stderr, " %02.02f", psnr);
			}
		}
		fprintf(stderr, "\r");
	}

	if (do_reporting) {
		char szStatus[1024];
		sprintf(szStatus, "[FFEnc] FPS %.02f F %d DTS "LLD" CTS "LLD" Q %02.02f PT %s (F_in %d)", fps, ctx->nb_frames_out, pkt->dts+ctx->ts_shift, pkt->pts+ctx->ts_shift, ((Double)q) /  FF_QP2LAMBDA, ptype, ctx->nb_frames_in);
		gf_filter_update_status(filter, -1, szStatus);
	}
}

static GF_Err ffenc_process_video(GF_Filter *filter, struct _gf_ffenc_ctx *ctx)
{
	AVPacket pkt;
	s32 gotpck;
	const char *data = NULL;
	u32 size=0, i, count, offset, to_copy;
	s32 res;
	u64 now;
	u8 *output;
	Bool insert_jp2c = GF_FALSE;
	GF_FilterPacket *dst_pck, *src_pck;
	GF_FilterPacket *pck;

	if (!ctx->in_pid) return GF_EOS;

	pck = gf_filter_pid_get_packet(ctx->in_pid);

	if (!ctx->encoder) {
		if (ctx->infmt_negociate) return GF_OK;

		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFEnc] encoder reconfiguration failed, aborting stream\n"));
		gf_filter_pid_set_eos(ctx->out_pid);
		return GF_EOS;
	}

	if (!pck) {
		if (! gf_filter_pid_is_eos(ctx->in_pid)) return GF_OK;
		if (ctx->flush_done) return GF_OK;
	}

	if (ctx->reconfig_pending) pck = NULL;

	if (pck) data = gf_filter_pck_get_data(pck, &size);

	av_init_packet(&pkt);
	pkt.data = (uint8_t*)ctx->enc_buffer;
	pkt.size = ctx->enc_buffer_size;

	ctx->frame->pict_type = 0;
	ctx->frame->width = ctx->width;
	ctx->frame->height = ctx->height;
	ctx->frame->format = ctx->pixel_fmt;
	//force picture type
	if (ctx->all_intra)
		ctx->frame->pict_type = AV_PICTURE_TYPE_I;

	now = gf_sys_clock_high_res();
	gotpck = 0;
	if (pck) {
		u32 ilaced;
		if (data) {
			ctx->frame->data[0] = (u8 *) data;
			ctx->frame->linesize[0] = ctx->stride;
			if (ctx->nb_planes>1) {
				ctx->frame->data[1] = (u8 *) data + ctx->stride * ctx->height;
				ctx->frame->linesize[1] = ctx->stride_uv ? ctx->stride_uv : ctx->stride/2;
				if (ctx->nb_planes>2) {
					ctx->frame->data[2] = (u8 *) ctx->frame->data[1] + ctx->stride_uv * ctx->height/2;
					ctx->frame->linesize[2] = ctx->frame->linesize[1];
				} else {
					ctx->frame->linesize[2] = 0;
				}
			} else {
				ctx->frame->linesize[1] = 0;
			}
		} else {
			GF_Err e=GF_NOT_SUPPORTED;
			GF_FilterFrameInterface *frame_ifce = gf_filter_pck_get_frame_interface(pck);
			if (frame_ifce && frame_ifce->get_plane) {
				e = frame_ifce->get_plane(frame_ifce, 0, (const u8 **) &ctx->frame->data[0], &ctx->frame->linesize[0]);
				if (!e && (ctx->nb_planes>1)) {
					e = frame_ifce->get_plane(frame_ifce, 1, (const u8 **) &ctx->frame->data[1], &ctx->frame->linesize[1]);
					if (!e && (ctx->nb_planes>2)) {
						e = frame_ifce->get_plane(frame_ifce, 1, (const u8 **) &ctx->frame->data[2], &ctx->frame->linesize[2]);
					}
				}
			}
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFEnc] Failed to fetch %sframe data: %s\n", frame_ifce ? "hardware " : "", gf_error_to_string(e) ));
				gf_filter_pid_drop_packet(ctx->in_pid);
				return e;
			}
		}

		ilaced = gf_filter_pck_get_interlaced(pck);
		if (!ilaced) {
			ctx->frame->interlaced_frame = 0;
		} else {
			ctx->frame->interlaced_frame = 1;
			ctx->frame->top_field_first = (ilaced==2) ? 1 : 0;
		}
		ctx->frame->pts = gf_filter_pck_get_cts(pck);
		ctx->frame->pkt_duration = gf_filter_pck_get_duration(pck);

#define SCALE_TS(_ts) if (_ts != GF_FILTER_NO_TS) { _ts *= ctx->encoder->time_base.den; _ts /= ctx->encoder->time_base.num; _ts /= ctx->timescale; }
#define UNSCALE_TS(_ts) if (_ts != AV_NOPTS_VALUE)  { _ts *= ctx->encoder->time_base.num; _ts *= ctx->timescale; _ts /= ctx->encoder->time_base.den; }
#define UNSCALE_DUR(_ts) { _ts *= ctx->encoder->time_base.num; _ts *= ctx->timescale; _ts /= ctx->encoder->time_base.den; }

		//store first frame CTS before rescaling, we use it after rescaling the output packet timing to compute CTS-DTS
		if (!ctx->cts_first_frame_plus_one) {
			ctx->cts_first_frame_plus_one = 1 + ctx->frame->pts;
		}

		if (ctx->remap_ts) {
			SCALE_TS(ctx->frame->pts);

			SCALE_TS(ctx->frame->pkt_duration);
		}

		ctx->frame->pkt_dts = ctx->frame->pkt_pts = ctx->frame->pts;

		ctx->frame->pict_type = AV_PICTURE_TYPE_NONE;

		if (ctx->fintra.den && ctx->fintra.num) {
			u64 cts = gf_filter_pck_get_cts(pck);
			if (!ctx->fintra_setup) {
				ctx->fintra_setup = GF_TRUE;
				ctx->orig_ts = cts;
				ctx->frame->pict_type = AV_PICTURE_TYPE_I;
				ctx->nb_forced=1;
			} else if (cts < ctx->orig_ts) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("[FFEnc] timestamps not increasing monotonuously, reseting forced intra state !\n"));
				ctx->orig_ts = cts;
				ctx->frame->pict_type = AV_PICTURE_TYPE_I;
				ctx->nb_forced=1;
			} else {
				u64 ts_diff = cts - ctx->orig_ts;
				if (ts_diff * ctx->fintra.den >= ctx->nb_forced * ctx->fintra.num * ctx->timescale) {
					ctx->frame->pict_type = AV_PICTURE_TYPE_I;
					ctx->nb_forced++;
					GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("[FFEnc] Forcing IDR at frame %d (CTS %d / %d)\n", ctx->nb_frames_in, cts, ctx->timescale));
				}
			}
		}

		res = avcodec_encode_video2(ctx->encoder, &pkt, ctx->frame, &gotpck);
		ctx->nb_frames_in++;

		//keep ref to ource properties
		gf_filter_pck_ref_props(&pck);
		gf_list_add(ctx->src_packets, pck);

		gf_filter_pid_drop_packet(ctx->in_pid);

		if (ctx->remap_ts) {
			UNSCALE_TS(ctx->frame->pts);
			UNSCALE_TS(ctx->frame->pkt_duration);

			UNSCALE_TS(pkt.dts);
			UNSCALE_TS(pkt.pts);
			UNSCALE_DUR(pkt.duration);
		}
	} else {
		res = avcodec_encode_video2(ctx->encoder, &pkt, NULL, &gotpck);
		if (!gotpck) {
			//done flushing encoder while reconfiguring
			if (ctx->reconfig_pending) {
				ctx->reconfig_pending = GF_FALSE;
				avcodec_close(ctx->encoder);
				ctx->encoder = NULL;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("[FFEnc] codec flush done, triggering reconfiguration\n"));
				return ffenc_configure_pid(filter, ctx->in_pid, GF_FALSE);
			}
			ctx->flush_done = GF_TRUE;
			gf_filter_pid_set_eos(ctx->out_pid);
			return GF_EOS;
		}
		if (ctx->remap_ts) {
			UNSCALE_TS(pkt.dts);
			UNSCALE_TS(pkt.pts);
			UNSCALE_DUR(pkt.duration);
		}
	}
	now = gf_sys_clock_high_res() - now;
	ctx->time_spent += now;

	if (res<0) {
		ctx->nb_frames_out++;
		return GF_SERVICE_ERROR;
	}

	if (!gotpck) {
		return GF_OK;
	}

	ctx->nb_frames_out++;
	if (ctx->init_cts_setup) {
		ctx->init_cts_setup = GF_FALSE;
		if (ctx->frame->pts != pkt.pts) {
			//check shift in PTS
			ctx->ts_shift = (s32) ( (s64) ctx->cts_first_frame_plus_one - 1 - (s64) pkt.pts );

			//check shift in DTS
			ctx->ts_shift += (s32) ( (s64) ctx->cts_first_frame_plus_one - 1 - (s64) pkt.dts );
		}
		if (ctx->ts_shift) {
			s64 shift = ctx->ts_shift;
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_DELAY, &PROP_SINT((s32) shift) );
		}
	}

	src_pck = NULL;
	count = gf_list_count(ctx->src_packets);
	for (i=0; i<count; i++) {
		src_pck = gf_list_get(ctx->src_packets, i);
		if (gf_filter_pck_get_cts(src_pck) == pkt.pts) break;
		src_pck = NULL;
	}

	offset = 0;
	to_copy = size = pkt.size;


	if (ctx->codecid == GF_CODECID_J2K) {
		u32 b4cc = GF_4CC(pkt.data[4], pkt.data[5], pkt.data[6], pkt.data[7]);
		if (b4cc == GF_4CC('j','P',' ',' ')) {
			u32 jp2h_offset = 0;
			offset = 12;
			while (offset+8 < (u32) pkt.size) {
				b4cc = GF_4CC(pkt.data[offset+4], pkt.data[offset+5], pkt.data[offset+6], pkt.data[offset+7]);
				if (b4cc == GF_4CC('j','p','2','c')) {
					break;
				}
				if (b4cc == GF_4CC('j','p','2','h')) {
					jp2h_offset = offset;
				}
				offset++;
			}
			if (jp2h_offset) {
				u32 len = pkt.data[jp2h_offset];
				len <<= 8;
				len |= pkt.data[jp2h_offset+1];
				len <<= 8;
				len |= pkt.data[jp2h_offset+2];
				len <<= 8;
				len |= pkt.data[jp2h_offset+3];

				u32 dsi_crc = gf_crc_32(pkt.data + jp2h_offset + 8, len-8);
				if (dsi_crc != ctx->dsi_crc) {
					ctx->dsi_crc = dsi_crc;
					gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(pkt.data + jp2h_offset + 8, len-8) );
				}
			}
			size -= offset;
			to_copy -= offset;
		} else {
			size += 8;

			if (!ctx->dsi_crc) {
				u8 *dsi;
				u32 dsi_len;
				GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
				gf_bs_write_u32(bs, 14+8);
				gf_bs_write_u32(bs, GF_4CC('i','h','d','r'));
				gf_bs_write_u32(bs, ctx->height);
				gf_bs_write_u32(bs, ctx->width);
				gf_bs_write_u16(bs, ctx->nb_planes);
				gf_bs_write_u8(bs, gf_pixel_get_bytes_per_pixel(ctx->gpac_pixel_fmt));
				gf_bs_write_u8(bs, 7); //COMP
				gf_bs_write_u8(bs, 0);
				gf_bs_write_u8(bs, 0);
				gf_bs_get_content(bs, &dsi, &dsi_len);
				gf_bs_del(bs);
				gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_len) );
				ctx->dsi_crc = 1;
			}
		}
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->out_pid, size, &output);
	if (insert_jp2c) {
		u32 bsize = pkt.size + 8;
		output[0] = (bsize >> 24) & 0xFF;
		output[1] = (bsize >> 16) & 0xFF;
		output[2] = (bsize >> 8) & 0xFF;
		output[3] = (bsize) & 0xFF;
		output[4] = 'j';
		output[5] = 'p';
		output[6] = '2';
		output[7] = 'c';
		output += 8;
	}
	memcpy(output, pkt.data + offset, to_copy);

	if (src_pck) {
		gf_filter_pck_merge_properties(src_pck, dst_pck);
		gf_list_del_item(ctx->src_packets, src_pck);
		gf_filter_pck_unref(src_pck);
	} else {
		if (pkt.duration) {
			gf_filter_pck_set_duration(dst_pck, (u32) pkt.duration);
		} else {
			gf_filter_pck_set_duration(dst_pck, (u32) ctx->frame->pkt_duration);
		}
	}

	ffenc_log_video(filter, ctx, &pkt, gf_filter_reporting_enabled(filter));

	gf_filter_pck_set_cts(dst_pck, pkt.pts + ctx->ts_shift);
	gf_filter_pck_set_dts(dst_pck, pkt.dts + ctx->ts_shift);

	//this is not 100% correct since we don't have any clue if this is SAP1/2/3/4 ...
	//since we send the output to our reframers we should be fine
	if (pkt.flags & AV_PKT_FLAG_KEY) {
		gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[FFEnc] frame %d is SAP\n", ctx->nb_frames_out));
	}
	else
		gf_filter_pck_set_sap(dst_pck, 0);

#if LIBAVCODEC_VERSION_MAJOR >= 58
	if (pkt.flags & AV_PKT_FLAG_DISPOSABLE) {
		gf_filter_pck_set_dependency_flags(dst_pck, 0x8);
	}
#endif
	gf_filter_pck_send(dst_pck);

	//we're in final flush, request a process task until all frames flushe
	//we could recursiveley call ourselves, same result
	if (!pck) {
		gf_filter_post_process_task(filter);
	}
	return GF_OK;
}

static void ffenc_audio_discard_samples(struct _gf_ffenc_ctx *ctx, u32 nb_samples)
{
	u32 i, bytes_per_chan, len;
	if (!ctx->planar_audio) {
		u32 offset = nb_samples * ctx->bytes_per_sample;
		u32 len = (ctx->samples_in_audio_buffer - nb_samples) * ctx->bytes_per_sample;
		assert(ctx->samples_in_audio_buffer >= nb_samples);
		memmove(ctx->audio_buffer, ctx->audio_buffer + offset, sizeof(char)*len);
		ctx->samples_in_audio_buffer = ctx->samples_in_audio_buffer - nb_samples;
		return;
	}

	bytes_per_chan = ctx->bytes_per_sample / ctx->channels;
	assert(ctx->samples_in_audio_buffer >= nb_samples);

	len = ctx->samples_in_audio_buffer - nb_samples;
	if (len) {
		for (i=0; i<ctx->channels; i++) {
			u8 *dst = ctx->audio_buffer + i*ctx->encoder->frame_size + bytes_per_chan * ctx->samples_in_audio_buffer;
			memmove(dst, dst+nb_samples, len*bytes_per_chan);
		}
		ctx->samples_in_audio_buffer -= nb_samples;
	} else {
		ctx->samples_in_audio_buffer = 0;
	}
}

static void ffenc_audio_append_samples(struct _gf_ffenc_ctx *ctx, const u8 *data, u32 size, u32 sample_offset, u32 nb_samples)
{
	u32 i, bytes_per_chan, src_frame_size;
	if (!ctx->planar_audio) {
		u32 offset_src = sample_offset * ctx->bytes_per_sample;
		u32 offset_dst = ctx->samples_in_audio_buffer * ctx->bytes_per_sample;
		u32 len = nb_samples * ctx->bytes_per_sample;
		memcpy(ctx->audio_buffer + offset_dst, data + offset_src, sizeof(char)*len);
		ctx->samples_in_audio_buffer += nb_samples;
		return;
	}

	bytes_per_chan = ctx->bytes_per_sample / ctx->channels;
	src_frame_size = size / ctx->bytes_per_sample;
	assert(ctx->samples_in_audio_buffer + nb_samples <= (u32) ctx->encoder->frame_size);

	for (i=0; i<ctx->channels; i++) {
		u8 *dst = ctx->audio_buffer + (i*ctx->encoder->frame_size + ctx->samples_in_audio_buffer) * bytes_per_chan;
		const u8 *src = data + (i*src_frame_size + sample_offset) * bytes_per_chan;
		memcpy(dst, src, nb_samples * bytes_per_chan);
	}
	ctx->samples_in_audio_buffer += nb_samples;
}

static GF_Err ffenc_process_audio(GF_Filter *filter, struct _gf_ffenc_ctx *ctx)
{
	AVPacket pkt;
	s32 gotpck;
	const char *data = NULL;
	u32 size=0, nb_copy=0, i, count;
	Bool from_buffer_only = GF_FALSE;
	s32 res;
	u32 nb_samples=0;
	u8 *output;
	GF_FilterPacket *dst_pck, *src_pck;
	GF_FilterPacket *pck;

	if (!ctx->in_pid) return GF_EOS;

	pck = gf_filter_pid_get_packet(ctx->in_pid);

	if (!ctx->encoder) {
		if (ctx->infmt_negociate) return GF_OK;
		
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFEnc] encoder reconfiguration failed, aborting stream\n"));
		gf_filter_pid_set_eos(ctx->out_pid);
		return GF_EOS;
	}

	if (!pck) {
		if (! gf_filter_pid_is_eos(ctx->in_pid)) return GF_OK;
		if (ctx->flush_done) return GF_EOS;
	}

	if (ctx->reconfig_pending) pck = NULL;

	if (ctx->encoder->frame_size && (ctx->encoder->frame_size <= (s32) ctx->samples_in_audio_buffer)) {
		res = avcodec_fill_audio_frame(ctx->frame, ctx->channels, ctx->sample_fmt, ctx->audio_buffer, ctx->bytes_per_sample * ctx->encoder->frame_size, 0);
		ffenc_audio_discard_samples(ctx, ctx->encoder->frame_size);
		from_buffer_only = GF_TRUE;
	} else if (pck) {
		data = gf_filter_pck_get_data(pck, &size);
		if (!data) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[FFEnc] Packet without associated data\n"));
			gf_filter_pid_drop_packet(ctx->in_pid);
			return GF_OK;
		}

		if (!ctx->samples_in_audio_buffer) {
			ctx->first_byte_cts = gf_filter_pck_get_cts(pck);
		}

		src_pck = pck;
		gf_filter_pck_ref_props(&src_pck);
		gf_list_add(ctx->src_packets, src_pck);

		nb_samples = size / ctx->bytes_per_sample;
		if (ctx->encoder->frame_size && (nb_samples + ctx->samples_in_audio_buffer < (u32) ctx->encoder->frame_size)) {
			ffenc_audio_append_samples(ctx, data, size, 0, nb_samples);
			gf_filter_pid_drop_packet(ctx->in_pid);
			return GF_OK;
		}

		if (ctx->encoder->frame_size) {
			nb_copy = ctx->encoder->frame_size - ctx->samples_in_audio_buffer;
			ffenc_audio_append_samples(ctx, data, size, 0, nb_copy);

			ctx->frame->nb_samples = ctx->encoder->frame_size;

			res = avcodec_fill_audio_frame(ctx->frame, ctx->channels, ctx->sample_fmt, ctx->audio_buffer, ctx->encoder->frame_size*ctx->bytes_per_sample, 0);
		} else {
			ctx->frame->nb_samples = size / ctx->bytes_per_sample;
			res = avcodec_fill_audio_frame(ctx->frame, ctx->channels, ctx->sample_fmt, data, size, 0);
			data = NULL;
			size = 0;
		}
		if (res<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFEnc] Error filling raw audio frame: %s\n", av_err2str(res) ));
			//discard
			ctx->samples_in_audio_buffer = 0;
			if (nb_samples > nb_copy) {
				u64 ts_diff = nb_copy;
				ffenc_audio_append_samples(ctx, data, size, nb_copy, nb_samples - nb_copy);
				ts_diff *= ctx->timescale;
				ts_diff /= ctx->sample_rate;
				ctx->first_byte_cts = gf_filter_pck_get_cts(pck) + ts_diff;
			}
			gf_filter_pid_drop_packet(ctx->in_pid);
			return GF_SERVICE_ERROR;
		}
	}

	av_init_packet(&pkt);
	pkt.data = (uint8_t*)ctx->enc_buffer;
	pkt.size = ctx->enc_buffer_size;

	gotpck = 0;
	if (pck) {
		ctx->frame->pkt_dts = ctx->frame->pkt_pts = ctx->frame->pts = ctx->first_byte_cts;

		res = avcodec_encode_audio2(ctx->encoder, &pkt, ctx->frame, &gotpck);
	} else {
		res = avcodec_encode_audio2(ctx->encoder, &pkt, NULL, &gotpck);
		if (!gotpck) {
			//done flushing encoder while reconfiguring
			if (ctx->reconfig_pending) {
				ctx->reconfig_pending = GF_FALSE;
				avcodec_close(ctx->encoder);
				ctx->encoder = NULL;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("[FFEnc] codec flush done, triggering reconfiguration\n"));
				return ffenc_configure_pid(filter, ctx->in_pid, GF_FALSE);
			}
			ctx->flush_done = GF_TRUE;
			gf_filter_pid_set_eos(ctx->out_pid);
			return GF_EOS;
		}
	}

	if (pck && !from_buffer_only) {
		ctx->samples_in_audio_buffer = 0;
		if (nb_samples > nb_copy) {
			u64 ts_diff = nb_copy;
			ts_diff *= ctx->timescale;
			ts_diff /= ctx->sample_rate;
			ctx->first_byte_cts = gf_filter_pck_get_cts(pck) + ts_diff;
			ffenc_audio_append_samples(ctx, data, size, nb_copy, nb_samples - nb_copy);
		}
		gf_filter_pid_drop_packet(ctx->in_pid);
	}

	if (res<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFEnc] Error encoding frame: %s\n", av_err2str(res) ));
		return GF_SERVICE_ERROR;
	}
	if (!gotpck) {
		return GF_OK;
	}
	dst_pck = gf_filter_pck_new_alloc(ctx->out_pid, pkt.size, &output);
	memcpy(output, pkt.data, pkt.size);

	if (ctx->init_cts_setup) {
		ctx->init_cts_setup = GF_FALSE;
		if (ctx->frame->pts != pkt.pts) {
			ctx->ts_shift = (s32) ( (s64) ctx->frame->pts - (s64) pkt.pts );
		}
//		if (ctx->ts_shift) {
//			s64 shift = ctx->ts_shift;
//			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_DELAY, &PROP_SINT((s32) -shift) );
//		}
	}

	//try to locate first source packet with cts greater than this packet cts and use it as source for properties
	//this is not optimal because we dont produce N for N because of different window coding sizes
	src_pck = NULL;
	count = gf_list_count(ctx->src_packets);
	for (i=0; i<count; i++) {
		u64 acts;
		u32 adur;
		src_pck = gf_list_get(ctx->src_packets, i);
		acts = gf_filter_pck_get_cts(src_pck);
		adur = gf_filter_pck_get_duration(src_pck);

		if ((s64) acts >= pkt.pts) {
			break;
		}

		if (acts + adur <= (u64) ( pkt.pts + ctx->ts_shift) ) {
			gf_list_rem(ctx->src_packets, i);
			gf_filter_pck_unref(src_pck);
			i--;
			count--;
		}
		src_pck = NULL;
	}
	if (src_pck) {
		gf_filter_pck_merge_properties(src_pck, dst_pck);
		gf_list_del_item(ctx->src_packets, src_pck);
		gf_filter_pck_unref(src_pck);
	}
	gf_filter_pck_set_cts(dst_pck, pkt.pts + ctx->ts_shift);
	gf_filter_pck_set_dts(dst_pck, pkt.dts + ctx->ts_shift);
	//this is not 100% correct since we don't have any clue if this is SAP1/4 (roll info missing)
	if (pkt.flags & AV_PKT_FLAG_KEY)
		gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
	else
		gf_filter_pck_set_sap(dst_pck, 0);

	gf_filter_pck_set_duration(dst_pck, (u32) pkt.duration);

	gf_filter_pck_send(dst_pck);

	//we're in final flush, request a process task until all frames flushe
	//we could recursiveley call ourselves, same result
	if (!pck) {
		gf_filter_post_process_task(filter);
	}
	return GF_OK;
}

static GF_Err ffenc_process(GF_Filter *filter)
{
	GF_FFEncodeCtx *ctx = (GF_FFEncodeCtx *) gf_filter_get_udta(filter);
	if (!ctx->out_pid || gf_filter_pid_would_block(ctx->out_pid))
		return GF_OK;
	return ctx->process(filter, ctx);
}

static void ffenc_copy_pid_props(GF_FFEncodeCtx *ctx)
{
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->out_pid, ctx->in_pid);
	gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_DECODER_CONFIG, NULL);
	gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_CODECID, &PROP_UINT(ctx->codecid) );

	switch (ctx->codecid) {
	case GF_CODECID_AVC:
	case GF_CODECID_HEVC:
	case GF_CODECID_MPEG4_PART2:
		gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );
		gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_UNFRAMED_FULL_AU, &PROP_BOOL(GF_TRUE) );
		break;
	default:
		if (ctx->encoder && ctx->encoder->extradata_size && ctx->encoder->extradata) {
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(ctx->encoder->extradata, ctx->encoder->extradata_size) );
		}
		break;
	}
}

static GF_Err ffenc_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	s32 res;
	u32 type=0, fftype, ff_codectag=0;
	u32 i=0;
	u32 change_input_fmt = 0;
	const GF_PropertyValue *prop;
	const AVCodec *codec=NULL;
	const AVCodec *desired_codec=NULL;
	u32 codec_id, pfmt, afmt;
	GF_FFEncodeCtx *ctx = (GF_FFEncodeCtx *) gf_filter_get_udta(filter);

	//disconnect of src pid (not yet supported)
	if (is_remove) {
		ctx->in_pid = NULL;
		//one in one out, this is simple
		if (ctx->out_pid) gf_filter_pid_remove(ctx->out_pid);
		return GF_OK;
	}
	//check our PID: streamtype and codecid
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!prop) return GF_NOT_SUPPORTED;

	type = prop->value.uint;
	switch (type) {
	case GF_STREAM_AUDIO:
	case GF_STREAM_VISUAL:
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!prop || prop->value.uint!=GF_CODECID_RAW) return GF_NOT_SUPPORTED;

	//figure out if output was preconfigured during filter chain setup
	prop = gf_filter_pid_caps_query(pid, GF_PROP_PID_CODECID);
	if (prop) {
		ctx->codecid = prop->value.uint;
	} else if (!ctx->codecid && ctx->c) {
		ctx->codecid = gf_codec_parse(ctx->c);
		if (!ctx->codecid) {
			codec = avcodec_find_encoder_by_name(ctx->c);
			if (codec)
				ctx->codecid = ffmpeg_codecid_to_gpac(codec->id);
		}
	}
	//if the codec was set using ffc, get it
	if (ctx->ffc) {
		desired_codec = avcodec_find_encoder_by_name(ctx->ffc);
	}

	if (!ctx->codecid && !desired_codec) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFEnc] No codecid specified\n" ));
		return GF_BAD_PARAM;
	}

	//initial config or update
	if (!ctx->in_pid || (ctx->in_pid==pid)) {
		ctx->in_pid = pid;
		if (!ctx->type) ctx->type = type;
		//no support for dynamic changes of stream types
		else if (ctx->type != type) {
			return GF_NOT_SUPPORTED;
		}
	} else {
		//only one input pid in ctx
		if (ctx->in_pid) return GF_REQUIRES_NEW_INSTANCE;
	}

	codec_id = 0;
	if (ctx->codecid) {
		codec_id = ffmpeg_codecid_from_gpac(ctx->codecid, &ff_codectag);
		if (codec_id) {
			if (desired_codec && desired_codec->id==codec_id)
				codec = desired_codec;
			else
				codec = avcodec_find_encoder(codec_id);
		}
	} else {
		codec = desired_codec;
	}
	if (!codec) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFEnc] Cannot find encoder for codec %s\n", gf_codecid_name(ctx->codecid) ));
		return GF_NOT_SUPPORTED;
	}
	codec_id = codec->id;
	if (!ctx->codecid)
		ctx->codecid = ffmpeg_codecid_to_gpac(codec->id);

	fftype = ffmpeg_stream_type_to_gpac(codec->type);
	if (fftype != type) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFEnc] Mismatch between stream type, codec indicates %s but source type is %s\n", gf_stream_type_name(fftype), gf_stream_type_name(type) ));
		return GF_NOT_SUPPORTED;
	}

	//declare our output pid to make sure we connect the chain
	ctx->in_pid = pid;
	if (!ctx->out_pid) {
		char szCodecName[1000];
		ctx->out_pid = gf_filter_pid_new(filter);

		//to change once we implement on-the-fly codec change
		sprintf(szCodecName, "ffenc:%s", codec->name ? codec->name : "unknown");
		gf_filter_set_name(filter, szCodecName);
		gf_filter_pid_set_framing_mode(ctx->in_pid, GF_TRUE);
	}
	if (type==GF_STREAM_AUDIO) {
		ctx->process = ffenc_process_audio;
	} else {
		ctx->process = ffenc_process_video;
	}

	ffenc_copy_pid_props(ctx);


#define GET_PROP(_a, _code, _name) \
	prop = gf_filter_pid_get_property(pid, _code); \
	if (!prop) {\
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[FFEnc] Input %s unknown, waiting for reconfigure\n", _name)); \
		return GF_OK; \
	}\
	_a  =prop->value.uint;

	pfmt = afmt = 0;
	if (type==GF_STREAM_VISUAL) {
		GET_PROP(ctx->width, GF_PROP_PID_WIDTH, "width")
		GET_PROP(ctx->height, GF_PROP_PID_HEIGHT, "height")
		GET_PROP(pfmt, GF_PROP_PID_PIXFMT, "pixel format")

		prop = gf_filter_pid_caps_query(pid, GF_PROP_PID_STRIDE);
		//keep stride and stride_uv to 0 i fnot set, and recompute from pixel format
		if (prop) ctx->stride = prop->value.uint;
		prop = gf_filter_pid_caps_query(pid, GF_PROP_PID_STRIDE_UV);
		if (prop) ctx->stride_uv = prop->value.uint;
	} else {
		GET_PROP(ctx->sample_rate, GF_PROP_PID_SAMPLE_RATE, "sample rate")
		GET_PROP(ctx->channels, GF_PROP_PID_NUM_CHANNELS, "nb channels")
		GET_PROP(afmt, GF_PROP_PID_AUDIO_FORMAT, "audio format")
	}

	if (ctx->encoder) {
		u32 codec_id = ffmpeg_codecid_from_gpac(ctx->codecid, &ff_codectag);

		if (type==GF_STREAM_AUDIO) {
			if ((ctx->encoder->codec->id==codec_id) && (ctx->encoder->sample_rate==ctx->sample_rate) && (ctx->encoder->channels==ctx->channels) && (ctx->gpac_audio_fmt == afmt ) ) {
				return GF_OK;
			}
		} else {
			if ((ctx->encoder->codec->id==codec_id) && (ctx->encoder->width==ctx->width) && (ctx->encoder->height==ctx->height) && (ctx->gpac_pixel_fmt == pfmt ) ) {
				return GF_OK;
			}
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("[FFEnc] codec reconfiguration, begining flush\n"));
		ctx->reconfig_pending = GF_TRUE;
		return GF_OK;
	}

	if (type==GF_STREAM_VISUAL) {
		u32 force_pfmt = AV_PIX_FMT_NONE;
		if (ctx->pfmt) {
			u32 ff_pfmt = ffmpeg_pixfmt_from_gpac(ctx->pfmt);
			i=0;
			while (codec->pix_fmts) {
				if (codec->pix_fmts[i] == AV_PIX_FMT_NONE) break;
				if (codec->pix_fmts[i] == ff_pfmt) {
					force_pfmt = ff_pfmt;
					break;
				}
				//handle pixel formats aliases
				if (ffmpeg_pixfmt_to_gpac(codec->pix_fmts[i]) == ctx->pfmt) {
					force_pfmt = ctx->pixel_fmt;
					break;
				}
				i++;
			}
			if (force_pfmt == AV_PIX_FMT_NONE) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("[FFEnc] Requested source format %s not supported by codec, using default one\n", gf_pixel_fmt_name(ctx->pfmt) ));
			} else {
				change_input_fmt = force_pfmt;
			}
		}
		ctx->pixel_fmt = ffmpeg_pixfmt_from_gpac(pfmt);
		//check pixel format
		if (force_pfmt == AV_PIX_FMT_NONE) {
			change_input_fmt = AV_PIX_FMT_NONE;
			i=0;
			while (codec->pix_fmts) {
				if (codec->pix_fmts[i] == AV_PIX_FMT_NONE) break;
				if (codec->pix_fmts[i] == ctx->pixel_fmt) {
					change_input_fmt = ctx->pixel_fmt;
					break;
				}
				//handle pixel formats aliases
				if (ffmpeg_pixfmt_to_gpac(codec->pix_fmts[i]) == pfmt) {
					ctx->pixel_fmt = change_input_fmt = codec->pix_fmts[i];
					break;
				}
				i++;
			}
			if (!ctx->ffc && (change_input_fmt == AV_PIX_FMT_NONE)) {
#if (LIBAVCODEC_VERSION_MAJOR >= 58) && (LIBAVCODEC_VERSION_MINOR>=20)
				void *ff_opaque=NULL;
#else
				AVCodec *codec_alt = NULL;
#endif
				while (1) {
#if (LIBAVCODEC_VERSION_MAJOR >= 58) && (LIBAVCODEC_VERSION_MINOR>=20)
					const AVCodec *codec_alt = av_codec_iterate(&ff_opaque);
#else
					codec_alt = av_codec_next(codec_alt);
#endif
					if (!codec_alt) break;
					if (codec_alt==codec) continue;
					if (codec_alt->id == codec_id) {
						i=0;
						while (codec_alt->pix_fmts) {
							if (codec_alt->pix_fmts[i] == AV_PIX_FMT_NONE) break;
							if (codec_alt->pix_fmts[i] == ctx->pixel_fmt) {
								change_input_fmt = ctx->pixel_fmt;
								GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("[FFEnc] Reassigning codec from %s to %s to match pixel format\n", codec->name, codec_alt->name ));
								codec = codec_alt;
								break;
							}
							i++;
						}
					}
				}
			}
		}

		if (ctx->pixel_fmt != change_input_fmt) {
			u32 ff_pmft = ctx->pixel_fmt;

			if (force_pfmt == AV_PIX_FMT_NONE) {
				ff_pmft = AV_PIX_FMT_NONE;
				i=0;
				//find a mapped pixel format
				while (codec->pix_fmts) {
					if (codec->pix_fmts[i] == AV_PIX_FMT_NONE) break;
					if (ffmpeg_pixfmt_to_gpac(codec->pix_fmts[i])) {
						ff_pmft = codec->pix_fmts[i];
						break;
					}
					i++;
				}
				if (ff_pmft == AV_PIX_FMT_NONE) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFEnc] Could not find a matching GPAC pixel format for encoder %s\n", codec->name ));
					return GF_NOT_SUPPORTED;
				}
			} else if (ctx->pfmt) {
				ff_pmft = ffmpeg_pixfmt_from_gpac(ctx->pfmt);
			}
			pfmt = ffmpeg_pixfmt_to_gpac(ff_pmft);
			gf_filter_pid_negociate_property(ctx->in_pid, GF_PROP_PID_PIXFMT, &PROP_UINT(pfmt) );
			ctx->infmt_negociate = GF_TRUE;
		} else {
			ctx->infmt_negociate = GF_FALSE;
		}
	} else {
		//check audio format
		ctx->sample_fmt = ffmpeg_audio_fmt_from_gpac(afmt);
		change_input_fmt = 0;
		while (codec->sample_fmts) {
			if (codec->sample_fmts[i] == AV_SAMPLE_FMT_NONE) break;
			if (codec->sample_fmts[i] == ctx->sample_fmt) {
				change_input_fmt = ctx->sample_fmt;
				break;
			}
			i++;
		}
		if (ctx->sample_fmt != change_input_fmt) {
			ctx->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_S16;
			afmt = ffmpeg_audio_fmt_to_gpac(ctx->sample_fmt);
			gf_filter_pid_negociate_property(ctx->in_pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(afmt) );
			ctx->infmt_negociate = GF_TRUE;
		} else {
			ctx->infmt_negociate = GF_FALSE;
		}
	}

	//renegociate input, wait for reconfig call
	if (ctx->infmt_negociate) return GF_OK;

	ctx->gpac_pixel_fmt = pfmt;
	ctx->gpac_audio_fmt = afmt;
	ctx->dsi_crc = 0;

	ctx->encoder = avcodec_alloc_context3(codec);
	if (! ctx->encoder) return GF_OUT_OF_MEM;

	ctx->encoder->codec_tag = ff_codectag;
	if (type==GF_STREAM_VISUAL) {
		ctx->encoder->width = ctx->width;
		ctx->encoder->height = ctx->height;
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_SAR);
		if (prop) {
			ctx->encoder->sample_aspect_ratio.num = prop->value.frac.num;
			ctx->timescale = ctx->encoder->sample_aspect_ratio.den = prop->value.frac.den;
		} else {
			ctx->encoder->sample_aspect_ratio.num = 1;
			ctx->encoder->sample_aspect_ratio.den = 1;
		}
		//CHECKME: do we need to use 1/FPS ?
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
		if (prop) {
			ctx->encoder->time_base.num = 1;
			ctx->timescale = ctx->encoder->time_base.den = prop->value.uint;
		}
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
		if (prop) {
			ctx->encoder->gop_size = 1;
			ctx->encoder->gop_size = prop->value.frac.num / prop->value.frac.den;
			ctx->encoder->time_base.num = prop->value.frac.den;
			ctx->encoder->time_base.den = prop->value.frac.num;
		}

		gf_media_get_reduced_frame_rate(&ctx->encoder->time_base.den, &ctx->encoder->time_base.num);

		if (ctx->low_delay) {
			av_dict_set(&ctx->options, "vprofile", "baseline", 0);
			av_dict_set(&ctx->options, "preset", "ultrafast", 0);
			av_dict_set(&ctx->options, "tune", "zerolatency", 0);
			if (ctx->codecid==GF_CODECID_AVC) {
				av_dict_set(&ctx->options, "x264opts", "no-mbtree:sliced-threads:sync-lookahead=0", 0);
			}
#if LIBAVCODEC_VERSION_MAJOR >= 58
			ctx->encoder->flags |= AV_CODEC_FLAG_LOW_DELAY;
#endif
		}

		if (ctx->fintra.den && ctx->fintra.num) {
			av_dict_set(&ctx->options, "forced-idr", "1", 0);
		}

		//we don't use out of band headers, since x264 in ffmpeg (and likely other) do not output in MP4 format but
		//in annexB (extradata only contains SPS/PPS/etc in annexB)
		//so we indicate unframed for these codecs and use our own filter for annexB->MP4

		if (!ctx->frame)
			ctx->frame = av_frame_alloc();

		ctx->enc_buffer_size = ctx->width*ctx->height + ENC_BUF_ALLOC_SAFE;
		ctx->enc_buffer = gf_realloc(ctx->enc_buffer, sizeof(char)*ctx->enc_buffer_size);

		gf_pixel_get_size_info(pfmt, ctx->width, ctx->height, NULL, &ctx->stride, &ctx->stride_uv, &ctx->nb_planes, &ctx->uv_height);

		ctx->encoder->pix_fmt = ctx->pixel_fmt;
		ctx->init_cts_setup = GF_TRUE;
	} else if (type==GF_STREAM_AUDIO) {
		ctx->process = ffenc_process_audio;

		ctx->encoder->sample_rate = ctx->sample_rate;
		ctx->encoder->channels = ctx->channels;

		//TODO
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CHANNEL_LAYOUT);
		if (prop) {
			ctx->encoder->channel_layout = ffmpeg_channel_layout_from_gpac(prop->value.uint);
		} else if (ctx->channels==1) {
			ctx->encoder->channel_layout = AV_CH_LAYOUT_MONO;
		} else if (ctx->channels==2) {
			ctx->encoder->channel_layout = AV_CH_LAYOUT_STEREO;
		}

		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
		if (prop) {
			ctx->encoder->time_base.num = 1;
			ctx->encoder->time_base.den = prop->value.uint;
			ctx->timescale = prop->value.uint;
		} else {
			ctx->encoder->time_base.num = 1;
			ctx->encoder->time_base.den = ctx->sample_rate;
			ctx->timescale = ctx->sample_rate;
		}

		//for aac
		switch (ctx->codecid) {
		case GF_CODECID_AAC_MPEG4:
		case GF_CODECID_AAC_MPEG2_MP:
		case GF_CODECID_AAC_MPEG2_LCP:
		case GF_CODECID_AAC_MPEG2_SSRP:
			av_dict_set(&ctx->options, "strict", "experimental", 0);
			break;
		}

		if (!ctx->frame)
			ctx->frame = av_frame_alloc();

		ctx->enc_buffer_size = ctx->channels*ctx->sample_rate + ENC_BUF_ALLOC_SAFE;
		ctx->enc_buffer = gf_realloc(ctx->enc_buffer, sizeof(char) * ctx->enc_buffer_size);

		ctx->encoder->sample_fmt = ctx->sample_fmt;
		ctx->planar_audio = gf_audio_fmt_is_planar(afmt);

		ctx->audio_buffer_size = ctx->sample_rate;
		ctx->audio_buffer = gf_realloc(ctx->audio_buffer, sizeof(char) * ctx->enc_buffer_size);
		ctx->bytes_per_sample = ctx->channels * gf_audio_fmt_bit_depth(afmt) / 8;
		ctx->init_cts_setup = GF_TRUE;

		switch (ctx->codecid) {
		case GF_CODECID_AAC_MPEG4:
		case GF_CODECID_AAC_MPEG2_MP:
		case GF_CODECID_AAC_MPEG2_LCP:
		case GF_CODECID_AAC_MPEG2_SSRP:
		{
			GF_M4ADecSpecInfo acfg;
			u8 *dsi;
			u32 dsi_len;
			memset(&acfg, 0, sizeof(GF_M4ADecSpecInfo));
			acfg.base_object_type = GF_M4A_AAC_LC;
			acfg.base_sr = ctx->sample_rate;
			acfg.nb_chan = ctx->channels;
			acfg.sbr_object_type = 0;
			acfg.audioPL = gf_m4a_get_profile(&acfg);

			gf_m4a_write_config(&acfg, &dsi, &dsi_len);
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_len) );
		}
			break;
		}
	}

	ffmpeg_set_enc_dec_flags(ctx->options, ctx->encoder);
	if (ctx->gop_size) ctx->encoder->gop_size = ctx->gop_size;

	res = avcodec_open2(ctx->encoder, codec, &ctx->options );
	if (res < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFEnc] PID %s failed to open codec context: %s\n", gf_filter_pid_get_name(pid), av_err2str(res) ));
		return GF_BAD_PARAM;
	}
	ctx->remap_ts = (ctx->encoder->time_base.den != ctx->timescale) ? GF_TRUE : GF_FALSE;


	AVDictionaryEntry *prev_e = NULL;
	while (1) {
		prev_e = av_dict_get(ctx->options, "", prev_e, AV_DICT_IGNORE_SUFFIX);
		if (!prev_e) break;
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[FFENC] meta-filter option %s=%s set but not used, see gpac -h ffenc and gpac -h ffenc:%s for allowed options\n", prev_e->key, prev_e->value, ctx->encoder->codec->priv_class->class_name));
		gf_filter_report_unused_meta_option(filter, prev_e->key);
	}

	ffenc_copy_pid_props(ctx);
	return GF_OK;
}


static GF_Err ffenc_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *arg_val)
{
	s32 res;
	const char *arg_val_str;
	GF_FFEncodeCtx *ctx = gf_filter_get_udta(filter);

	if (!strcmp(arg_name, "global_header"))	return GF_OK;
	else if (!strcmp(arg_name, "local_header"))	return GF_OK;
	else if (!strcmp(arg_name, "low_delay"))	ctx->low_delay = GF_TRUE;
	//remap some options
	else if (!strcmp(arg_name, "bitrate") || !strcmp(arg_name, "rate"))	arg_name = "b";
//	else if (!strcmp(arg_name, "gop")) arg_name = "g";
	//disable low delay if these options are set
	else if (!strcmp(arg_name, "x264opts")) ctx->low_delay = GF_FALSE;
	else if (!strcmp(arg_name, "vprofile")) ctx->low_delay = GF_FALSE;
	else if (!strcmp(arg_name, "preset")) ctx->low_delay = GF_FALSE;
	else if (!strcmp(arg_name, "tune")) ctx->low_delay = GF_FALSE;

	if (!strcmp(arg_name, "g") || !strcmp(arg_name, "gop"))
		ctx->gop_size = arg_val->value.string ? atoi(arg_val->value.string) : 25;

	//initial parsing of arguments
	if (!ctx->initialized) {
		switch (arg_val->type) {
		case GF_PROP_STRING:
			arg_val_str = arg_val->value.string;
			if (!arg_val_str) arg_val_str = "1";
			res = av_dict_set(&ctx->options, arg_name, arg_val_str, 0);
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFEnc] Failed to set option %s:%s\n", arg_name, arg_val ));
			}
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFEnc] Failed to set option %s:%s, unrecognized type %d\n", arg_name, arg_val, arg_val->type ));
			return GF_NOT_SUPPORTED;
		}
		return GF_OK;
	}
	//updates of arguments, not supported for ffmpeg decoders
	return GF_NOT_SUPPORTED;
}

static const GF_FilterCapability FFEncodeCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	//some video encoding dumps in unframe mode, we declare the pid property at runtime
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),

};

GF_FilterRegister FFEncodeRegister = {
	.name = "ffenc",
	.version=LIBAVCODEC_IDENT,
	GF_FS_SET_DESCRIPTION("FFMPEG encoder")
	GF_FS_SET_HELP("See FFMPEG documentation (https://ffmpeg.org/documentation.html) for more detailed info on encoder options"
		"\n"
		"Note: if no codec is explicited through [-ffc]() option and no pixel format is given, codecs will be enumerated to find a matching pixel format.\n"

	)
	.private_size = sizeof(GF_FFEncodeCtx),
	SETCAPS(FFEncodeCaps),
	.initialize = ffenc_initialize,
	.finalize = ffenc_finalize,
	.configure_pid = ffenc_configure_pid,
	.process = ffenc_process,
	.update_arg = ffenc_update_arg,
};

#define OFFS(_n)	#_n, offsetof(GF_FFEncodeCtx, _n)
static const GF_FilterArgs FFEncodeArgs[] =
{
	{ OFFS(c), "codec identifier. Can be any supported GPAC ID or ffmpeg ID or filter subclass name", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(pfmt), "pixel format for input video. When not set, input format is used", GF_PROP_PIXFMT, "none", NULL, 0},
	{ OFFS(fintra), "force intra / IDR frames at the given period in sec, eg `fintra=60000/1001` will force an intra every 2 seconds on 29.97 fps video; ignored for audio", GF_PROP_FRACTION, "0", NULL, 0},

	{ OFFS(all_intra), "only produce intra frames", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ls), "output log", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ffc), "ffmpeg codec name. This allows enforcing a given codec if multiple codecs support the codec ID set (eg aac vs vo_aacenc)", GF_PROP_STRING, NULL, NULL, 0},

	{ "*", -1, "any possible args defined for AVCodecContext and sub-classes. see `gpac -hx ffenc` and `gpac -hx ffenc:*`", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_META},
	{0}
};

const int FFENC_STATIC_ARGS = (sizeof (FFEncodeArgs) / sizeof (GF_FilterArgs)) - 1;

void ffenc_regfree(GF_FilterSession *session, GF_FilterRegister *reg)
{
	ffmpeg_register_free(session, reg, FFENC_STATIC_ARGS);
}

const GF_FilterRegister *ffenc_register(GF_FilterSession *session)
{
	GF_FilterArgs *args;
	u32 i=0, idx=0;
	Bool load_meta_filters = session ? GF_TRUE : GF_FALSE;
	AVCodecContext *ctx;
	const struct AVOption *opt;

	ffmpeg_initialize();

	//by default no need to load option descriptions, everything is handled by av_set_opt in update_args
	if (!load_meta_filters) {
		FFEncodeRegister.args = FFEncodeArgs;
		FFEncodeRegister.register_free = NULL;
		return &FFEncodeRegister;
	}

	FFEncodeRegister.register_free = ffenc_regfree;
	ctx = avcodec_alloc_context3(NULL);

	idx=0;
	while (ctx->av_class->option) {
		opt = &ctx->av_class->option[idx];
		if (!opt || !opt->name) break;
		if (opt->flags & AV_OPT_FLAG_ENCODING_PARAM)
			i++;
		idx++;
	}
	i+=FFENC_STATIC_ARGS+1;

	args = gf_malloc(sizeof(GF_FilterArgs)*i);
	memset(args, 0, sizeof(GF_FilterArgs)*i);
	FFEncodeRegister.args = args;
	for (i=0; (s32) i<FFENC_STATIC_ARGS; i++)
		args[i] = FFEncodeArgs[i];

	idx=0;
	while (ctx->av_class->option) {
		opt = &ctx->av_class->option[idx];
		if (!opt || !opt->name) break;
		if (opt->flags & AV_OPT_FLAG_ENCODING_PARAM) {
			args[i] = ffmpeg_arg_translate(opt);
			i++;
		}
		idx++;
	}

#if (LIBAVCODEC_VERSION_MAJOR >= 58) && (LIBAVCODEC_VERSION_MINOR>=20)
	avcodec_free_context(&ctx);
#else
	av_free(ctx);
#endif

	ffmpeg_expand_register(session, &FFEncodeRegister, FF_REG_TYPE_ENCODE);

	return &FFEncodeRegister;
}


#else
#include <gpac/filters.h>
const GF_FilterRegister *ffenc_register(GF_FilterSession *session)
{
	return NULL;
}

#endif //GPAC_HAS_FFMPEG

