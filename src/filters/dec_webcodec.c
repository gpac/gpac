/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2023
 *					All rights reserved
 *
 *  This file is part of GPAC / WebCodec decoder filter
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
#include <gpac/constants.h>

#if defined(GPAC_CONFIG_EMSCRIPTEN)

typedef struct
{
	u32 queued;

	GF_Filter *filter;
	u32 codecid, cfg_crc, timescale;
	u32 width, height;
	Bool codec_init;
	u32 pf, out_size, in_flush;

	u32 af, sample_rate, num_channels, bytes_per_sample;

	GF_FilterPid *ipid, *opid;
	GF_List *src_pcks;
	GF_Err error, dec_error;
	u32 pending_frames, nb_frames;
	Bool playing;
	char szCodec[RFC6381_CODEC_NAME_SIZE_MAX];
} GF_WCDecCtx;

GF_EXPORT
void wcdec_on_error(GF_WCDecCtx *ctx, int state, char *msg)
{
	if (!ctx) return;
	//decode error
	if (state > 1)
		ctx->dec_error = GF_BAD_PARAM;
	else if (state==1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[WebDec] Failed to initialize for codec %s: %s\n", ctx->szCodec, msg ? msg : ""));
		ctx->error = GF_PROFILE_NOT_SUPPORTED;
	}
	else {
		ctx->error = GF_OK;
		ctx->codec_init = GF_TRUE;
	}
}

EM_JS(int, wcdec_init, (int wc_ctx, int _codec_str, int width, int height, int sample_rate, int num_channels, int dsi, int dsi_size), {
	let codec_str = _codec_str ? libgpac.UTF8ToString(_codec_str) : null;
	let config = {};
	config.codec = codec_str;
	let dec_class = null;
	if (width) {
		config.codedWidth = width;
		config.codedHeight = height;
		dec_class = VideoDecoder;
	} else {
		config.sampleRate = sample_rate;
		config.numberOfChannels = num_channels;
		dec_class = AudioDecoder;
	}
	if (dsi_size) {
		config.description = new Uint8Array(libgpac.HEAPU8.buffer, dsi, dsi_size);
	}

	if (typeof libgpac._to_webdec != 'function') {
		libgpac._web_decs = [];
		libgpac._to_webdec = (ctx) => {
          for (let i=0; i<libgpac._web_decs.length; i++) {
            if (libgpac._web_decs[i]._wc_ctx==ctx) return libgpac._web_decs[i];
          }
          return null;
		};
		libgpac._on_wcdec_error = libgpac.cwrap('wcdec_on_error', null, ['number', 'number', 'string']);
		libgpac._on_wcdec_frame = libgpac.cwrap('wcdec_on_video', null, ['number', 'bigint', 'string', 'number', 'number']);
		libgpac._on_wcdec_audio = libgpac.cwrap('wcdec_on_audio', null, ['number', 'bigint', 'string', 'number', 'number', 'number']);
		libgpac._on_wcdec_flush = libgpac.cwrap('wcdec_on_flush', null, ['number']);
		libgpac._on_wcdec_frame_copy = libgpac.cwrap('wcdec_on_frame_copy', null, ['number', 'number', 'number']);
	}
	let c = libgpac._to_webdec(wc_ctx);
	if (!c) {
		c = {_wc_ctx: wc_ctx, dec: null, _frame: null};
		libgpac._web_decs.push(c);
	}
	dec_class.isConfigSupported(config).then( supported => {
		if (supported.supported) {
			if (!c.dec) {
				let init_info = {
					error: (e) => {
						libgpac._on_wcdec_error(c._wc_ctx, 1, ""+e);
					}
				};
				if (width && height) {
					init_info.output = (frame) => {
						c._frame = frame;
						libgpac._on_wcdec_frame(c._wc_ctx, BigInt(frame.timestamp), frame.format, frame.codedWidth, frame.codedHeight);
						if (c._frame) frame.close();
					};
				} else {
					init_info.output = (frame) => {
						c._frame = frame;
						libgpac._on_wcdec_audio(c._wc_ctx, BigInt(frame.timestamp), frame.format, frame.numberOfFrames, frame.numberOfChannels, frame.sampleRate);
						if (c._frame) frame.close();
					};
				}

				c.dec = new dec_class(init_info);
			}
			c.dec.configure(config);
			libgpac._on_wcdec_error(c._wc_ctx, 0, null);
		} else {
			libgpac._on_wcdec_error(c._wc_ctx, 1, null);
		}
	}).catch( (e) => {
		libgpac._on_wcdec_error(c._wc_ctx, 1, ""+e);
	});
})

static GF_Err wcdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p, *dsi;
	u32 codecid, dsi_crc;
	u32 streamtype;
    GF_WCDecCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid)) {
		return GF_NOT_SUPPORTED;
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[WebDec] Missing codecid, cannot initialize\n"));
		return GF_NOT_SUPPORTED;
	}
	codecid = p->value.uint;


	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!p) return GF_NOT_SUPPORTED;
	streamtype = p->value.uint;

	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	dsi_crc = dsi ? gf_crc_32(dsi->value.data.ptr, dsi->value.data.size) : 0;
	if (ctx->codec_init && (codecid==ctx->codecid) && (dsi_crc == ctx->cfg_crc)) return GF_OK;

	ctx->in_flush = 0;
	ctx->cfg_crc = dsi_crc;
	ctx->ipid = pid;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	}

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL );

	switch (codecid) {
	//codecs for which we will need a reframer (DSI rebuild, DTS recompute and metadata extraction)
	case GF_CODECID_AVC:
	case GF_CODECID_HEVC:
	case GF_CODECID_VVC:
	case GF_CODECID_AV1:
	case GF_CODECID_VP8:
	case GF_CODECID_VP9:
	case GF_CODECID_AAC_MPEG2_MP:
	case GF_CODECID_AAC_MPEG2_LCP:
	case GF_CODECID_AAC_MPEG2_SSRP:
	case GF_CODECID_AAC_MPEG4:
		//check decoder config is ready
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (!p) return GF_OK;
		break;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	ctx->timescale = p ? p->value.uint : 1000;
	ctx->codecid = codecid;
	ctx->codec_init = GF_FALSE;

	if (streamtype==GF_STREAM_VISUAL) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
		if (!p) return GF_BAD_PARAM;
		ctx->width = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
		if (!p) return GF_BAD_PARAM;
		ctx->height = p->value.uint;

		//default to nv12 for most codecs, will be reconfigured when fetching first frame
		ctx->pf = GF_PIXEL_NV12;
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->pf) );
	} else {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
		if (!p) return GF_BAD_PARAM;
		ctx->sample_rate = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
		ctx->num_channels = p ? p->value.uint : 1;

		//default to 32-bit float for most codecs, will be reconfigured when fetching first frame
		ctx->af = GF_AUDIO_FMT_FLT;
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(ctx->af));
	}
	if (codecid==GF_CODECID_OPUS) {
		strcpy(ctx->szCodec, "opus");
		dsi = NULL; //if description is set, WebCodec assumes ogg+opus
	} else {
		gf_filter_pid_get_rfc_6381_codec_string(pid, ctx->szCodec, GF_FALSE, GF_FALSE, NULL, NULL);
		strlwr(ctx->szCodec);
	}

	//create decoder
	wcdec_init(EM_CAST_PTR ctx,
		EM_CAST_PTR ctx->szCodec,
		ctx->width,
		ctx->height,
		ctx->sample_rate,
		ctx->num_channels,
		dsi ? EM_CAST_PTR dsi->value.data.ptr : 0,
		dsi ? dsi->value.data.size : 0);
	return GF_OK;
}



EM_JS(int, wcdec_push_frame, (int wc_ctx, int buf, int buf_size, int key, u64 ts), {
	let c = libgpac._to_webdec(wc_ctx);
	if (!c || !c.dec) return;
	const chunk = new EncodedVideoChunk({
		timestamp: Number(ts),
		type: key ? "key" : "delta",
		data: new Uint8Array(libgpac.HEAPU8.buffer, buf, buf_size)
  });
  c.dec.decode(chunk);
})

EM_JS(int, wcdec_push_audio, (int wc_ctx, int buf, int buf_size, int key, u64 ts), {
	let c = libgpac._to_webdec(wc_ctx);
	if (!c || !c.dec) return;
	const chunk = new EncodedAudioChunk({
		timestamp: Number(ts),
		type: key ? "key" : "delta",
		data: new Uint8Array(libgpac.HEAPU8.buffer, buf, buf_size)
  });
  c.dec.decode(chunk);
})

EM_JS(int, wcdec_copy_frame, (int wc_ctx, int dst_pck, int buf, int buf_size), {
	let c = libgpac._to_webdec(wc_ctx);
	if (!c || !c._frame || !dst_pck) return;

	//setup dst
	let ab = new Uint8Array(libgpac.HEAPU8.buffer, buf, buf_size);
	let frame = c._frame;
	c._frame = null;
	frame.copyTo(ab).then( layout => {
		libgpac._on_wcdec_frame_copy(c._wc_ctx, dst_pck, 1);
		frame.close();
	}).catch( e => {
		libgpac._on_wcdec_frame_copy(c._wc_ctx, dst_pck, 0);
		frame.close();
	});

})

GF_EXPORT
void wcdec_on_frame_copy(GF_WCDecCtx *ctx, GF_FilterPacket *pck, int res_ok)
{
	if (!ctx) {
		return;
	}
	ctx->pending_frames--;
	if (res_ok)
		gf_filter_pck_send(pck);
	else
		gf_filter_pck_discard(pck);

	if (!ctx->pending_frames && (ctx->in_flush==2))
		gf_filter_pid_set_eos(ctx->opid);

}

u32 webcodec_pixfmt_to_gpac(char *format)
{
	if (!stricmp(format, "I420")) return GF_PIXEL_YUV;
	else if (!stricmp(format, "I420A")) return GF_PIXEL_YUVA;
	else if (!stricmp(format, "I422")) return GF_PIXEL_YUV422;
	else if (!stricmp(format, "I444")) return GF_PIXEL_YUV444;
	else if (!stricmp(format, "NV12")) return GF_PIXEL_NV12;
	else if (!stricmp(format, "RGBA")) return GF_PIXEL_RGBA;
	else if (!stricmp(format, "RGBX")) return GF_PIXEL_RGBX;
	else if (!stricmp(format, "BGRA")) return GF_PIXEL_BGRA;
	else if (!stricmp(format, "BGRX")) return GF_PIXEL_BGRX;
	return 0;
}
GF_EXPORT
void wcdec_on_video(GF_WCDecCtx *ctx, u64 timestamp, char *format, u32 width, u32 height)
{
	u32 pf = 0;
	u8 *output;
	GF_FilterPacket *dst;
	if (!ctx) return;

	pf = webcodec_pixfmt_to_gpac(format);
	if (!pf) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[WebDec] Unrecognized pixel format %s\n", format));
		return;
	}
	if ((pf != ctx->pf) || (width != ctx->width) || (height != ctx->height) || !ctx->out_size) {
		ctx->pf = pf;
		ctx->width = width;
		ctx->height = height;
		gf_pixel_get_size_info(pf, width, height, &ctx->out_size, NULL, NULL, NULL, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(width));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(height));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(pf));
	}

	s64 cts = gf_timestamp_rescale(timestamp, 1000000, ctx->timescale);
	GF_FilterPacket *src_pck = NULL;
	u32 i, count = gf_list_count(ctx->src_pcks);
	for (i=0; i<count; i++) {
		src_pck = gf_list_get(ctx->src_pcks, i);
		if (!src_pck) {
			continue;
		}
		s64 ts = gf_filter_pck_get_cts(src_pck);
		if (ABS(ts - cts) <= 1) {
			gf_list_rem(ctx->src_pcks, i);
			break;
		}
		src_pck = NULL;
	}

	dst = gf_filter_pck_new_alloc(ctx->opid, ctx->out_size, &output);
	if (!dst) {
		if (src_pck) gf_filter_pck_unref(src_pck);
		return;
	}
	gf_filter_pck_set_cts(dst, cts);
	if (src_pck) {
		gf_filter_pck_merge_properties(src_pck, dst);
		gf_filter_pck_unref(src_pck);
	}

	ctx->pending_frames++;
	ctx->nb_frames++;
	wcdec_copy_frame(EM_CAST_PTR ctx,
		EM_CAST_PTR dst,
		EM_CAST_PTR output, ctx->out_size);
}

EM_JS(int, wcdec_copy_audio, (int wc_ctx, int buf, int buf_size, int plane_index), {
	let c = libgpac._to_webdec(wc_ctx);
	if (!c || !c._frame) return;

	//setup dst
	let ab = new Uint8Array(libgpac.HEAPU8.buffer, buf, buf_size);
	c._frame.copyTo(ab, { planeIndex: plane_index });
})


u32 webcodec_audiofmt_to_gpac(char *format)
{
	if (!stricmp(format, "u8")) return GF_AUDIO_FMT_U8;
	else if (!stricmp(format, "s16")) return GF_AUDIO_FMT_S16;
	else if (!stricmp(format, "s32")) return GF_AUDIO_FMT_S32;
	else if (!stricmp(format, "f32")) return GF_AUDIO_FMT_FLT;
	else if (!stricmp(format, "u8-planar")) return GF_AUDIO_FMT_U8P;
	else if (!stricmp(format, "s16-planar")) return GF_AUDIO_FMT_S16P;
	else if (!stricmp(format, "s32-planar")) return GF_AUDIO_FMT_S32P;
	else if (!stricmp(format, "f32-planar")) return GF_AUDIO_FMT_FLTP;
	return 0;
}

GF_EXPORT
void wcdec_on_audio(GF_WCDecCtx *ctx, u64 timestamp, char *format, u32 num_frames, u32 num_channels, u32 sample_rate)
{
	u32 af = 0;
	u8 *output;
	GF_FilterPacket *dst;
	if (!ctx) return;

	af = webcodec_audiofmt_to_gpac(format);
	if (!af) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[WebDec] Unrecognized pixel format %s\n", format));
		return;
	}
	if ((sample_rate != ctx->sample_rate) || (af != ctx->af) || (num_channels != ctx->num_channels)) {
		ctx->sample_rate = sample_rate;
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(sample_rate));
		ctx->af = af;
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(af));
		ctx->num_channels = num_channels;
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(num_channels));
		ctx->bytes_per_sample = gf_audio_fmt_bit_depth(af) / 8;
	}

	s64 cts = gf_timestamp_rescale(timestamp, 1000000, ctx->timescale);

	GF_FilterPacket *src_pck = NULL;
	u32 i, count = gf_list_count(ctx->src_pcks);
	for (i=0; i<count; i++) {
		src_pck = gf_list_get(ctx->src_pcks, i);
		if (!src_pck) {
			continue;
		}
		s64 ts = gf_filter_pck_get_cts(src_pck);
		if (ABS(ts - cts) <= 1) {
			gf_list_rem(ctx->src_pcks, i);
			break;
		}
		src_pck = NULL;
	}
	u32 output_size = ctx->bytes_per_sample*num_frames*num_channels;
	dst = gf_filter_pck_new_alloc(ctx->opid, output_size, &output);
	if (!dst) {
		if (src_pck) gf_filter_pck_unref(src_pck);
		return;
	}
	gf_filter_pck_set_cts(dst, cts);
	if (src_pck) {
		gf_filter_pck_merge_properties(src_pck, dst);
		gf_filter_pck_unref(src_pck);
	}

	//audioData copyTo is sync
	u32 num_planes, plane_size;
	if (gf_audio_fmt_is_planar(af)) {
		num_planes = num_channels;
		plane_size = ctx->bytes_per_sample*num_frames;
	} else {
		num_planes = 1;
		plane_size = ctx->bytes_per_sample*num_frames*num_channels;
	}

	for (i=0; i<num_planes; i++) {
		u8 *buf = output + i * plane_size;
		wcdec_copy_audio(EM_CAST_PTR ctx, EM_CAST_PTR buf, plane_size, i);
	}
	gf_filter_pck_send(dst);
}

GF_EXPORT
void wcdec_on_flush(GF_WCDecCtx *ctx)
{
	ctx->in_flush = 2;
	if (!ctx->pending_frames)
		gf_filter_pid_set_eos(ctx->opid);
}

EM_JS(int, wcdec_flush, (int wc_ctx), {
	let c = libgpac._to_webdec(wc_ctx);
	if (!c || !c.dec) return;
	c.dec.flush().then( () => { libgpac._on_wcdec_flush(c._wc_ctx); }).catch ( (e) => { libgpac._on_wcdec_flush(c._wc_ctx); } );
})


static GF_Err wcdec_process(GF_Filter *filter)
{
	u64 cts=0;
	GF_Err e;
	const u8 *in_buffer;
	u32 in_buffer_size;
	GF_FilterSAPType sap;
	GF_WCDecCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;

	if (ctx->error)
		return ctx->error;

	if (!ctx->codec_init) {
		//force fetch first packet if not init, in case decoder config was not ready at setup
		gf_filter_pid_get_packet(ctx->ipid);
		return GF_OK;
	}

	if (!ctx->playing) {
		if (ctx->pending_frames) {
			gf_filter_post_process_task(filter);
		}
		return GF_OK;
	}

	while (!ctx->dec_error) {
		pck = gf_filter_pid_get_packet(ctx->ipid);
		if (!pck) {
			if (gf_filter_pid_is_eos(ctx->ipid)) {
				if (!ctx->in_flush) {
					ctx->in_flush = 1;
					wcdec_flush(EM_CAST_PTR ctx);
				}
				if ((ctx->in_flush==1) || ((ctx->in_flush==2) && ctx->pending_frames)) {
					gf_filter_post_process_task(filter);
					return GF_OK;
				}
				return GF_EOS;
			}
			//keep filter alive while we have pending frames to make sure session is not destroyed while waiting for video frame copy promise to complete
			else if (ctx->pending_frames) {
				gf_filter_post_process_task(filter);
			}
			return GF_OK;
		}
		ctx->in_flush = 0;
		//don't push too fast
		if (gf_list_count(ctx->src_pcks) > ctx->queued) {
			//ask for later processing to trigger breaking of scheduler loop in case of threading
			gf_filter_ask_rt_reschedule(filter, 500);
			return GF_OK;
		}
		in_buffer = (u8 *) gf_filter_pck_get_data(pck, &in_buffer_size);
		cts = gf_timestamp_rescale( gf_filter_pck_get_cts(pck), ctx->timescale, 1000000);
		sap = gf_filter_pck_get_sap(pck);
		//queue input
		if (ctx->width) {
			wcdec_push_frame(EM_CAST_PTR ctx, EM_CAST_PTR in_buffer, in_buffer_size, sap ? 1 : 0, cts);
		} else {
			wcdec_push_audio(EM_CAST_PTR ctx, EM_CAST_PTR in_buffer, in_buffer_size, sap ? 1 : 0, cts);
		}

		gf_filter_pck_ref_props(&pck);
		gf_list_add(ctx->src_pcks, pck);
		gf_filter_pid_drop_packet(ctx->ipid);
	}
	e = ctx->dec_error;
	ctx->dec_error = GF_OK;
	return e;
}

static Bool wcdec_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
    GF_WCDecCtx *ctx = gf_filter_get_udta(filter);
	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		ctx->playing = GF_TRUE;
		break;
	case GF_FEVT_STOP:
		ctx->playing = GF_FALSE;
		break;
	default:
		break;
	}
	return GF_FALSE;
}

GF_Err wcdec_initialize(GF_Filter *filter)
{
    GF_WCDecCtx *ctx = gf_filter_get_udta(filter);
    ctx->filter = filter;
	ctx->src_pcks = gf_list_new();
    return GF_OK;
}

EM_JS(int, wcdec_del, (int wc_ctx), {
	if (! Array.isArray(libgpac._web_decs) ) return;
	for (let i=0; i<libgpac._web_decs.length; i++) {
		if (libgpac._web_decs[i]._wc_ctx == wc_ctx) {
			libgpac._web_decs[i]._wc_ctx = null;
			libgpac._web_decs.splice(i, 1);
			return;
		}
	}
})


void wcdec_finalize(GF_Filter *filter)
{
    GF_WCDecCtx *ctx = gf_filter_get_udta(filter);
    wcdec_del(EM_CAST_PTR ctx);

    while (gf_list_count(ctx->src_pcks)) {
		GF_FilterPacket *pck = gf_list_pop_back(ctx->src_pcks);
		gf_filter_pck_unref(pck);
	}
	gf_list_del(ctx->src_pcks);
}

static GF_FilterCapability WCDecCapsV[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

static GF_FilterCapability WCDecCapsA[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

static GF_FilterCapability WCDecCapsAV[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW)
};

#define OFFS(_n)	#_n, offsetof(GF_WCDecCtx, _n)

static const GF_FilterArgs WCDecArgs[] =
{
	{ OFFS(queued), "Maximum number of packets to queue in webcodec instance", GF_PROP_UINT, "10", NULL, 0},
	{0}
};

GF_FilterRegister GF_WCDecCtxRegister = {
	.name = "wcdec",
	GF_FS_SET_DESCRIPTION("WebCodec decoder")
	GF_FS_SET_HELP("This filter decodes video streams using WebCodec decoder of the browser")
	.args = WCDecArgs,
	SETCAPS(WCDecCapsAV),
	.flags = GF_FS_REG_SINGLE_THREAD|GF_FS_REG_ASYNC_BLOCK,
	.private_size = sizeof(GF_WCDecCtx),
	.initialize = wcdec_initialize,
	.finalize = wcdec_finalize,
	.configure_pid = wcdec_configure_pid,
	.process = wcdec_process,
	.process_event = wcdec_process_event,
};


const GF_FilterRegister *wcdec_register(GF_FilterSession *session)
{
	int has_webv_decode = EM_ASM_INT({
		if (typeof VideoDecoder == 'undefined') return 0;
		return 1;
	});
	int has_weba_decode = EM_ASM_INT({
		if (typeof AudioDecoder == 'undefined') return 0;
		return 1;
	});

	if (!has_webv_decode && !has_weba_decode) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[WebDec] No WebDecoder support\n"));
		return NULL;
	}
	if (!has_webv_decode) {
		GF_WCDecCtxRegister.caps = WCDecCapsA;
		GF_WCDecCtxRegister.nb_caps = sizeof(WCDecCapsA)/sizeof(GF_FilterCapability);
	} else if (!has_weba_decode) {
		GF_WCDecCtxRegister.caps = WCDecCapsV;
		GF_WCDecCtxRegister.nb_caps = sizeof(WCDecCapsV)/sizeof(GF_FilterCapability);
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[WebDec] AudioDecoder %d - VideoDecoder %d\n", has_weba_decode, has_webv_decode));
	return &GF_WCDecCtxRegister;
}
#endif
