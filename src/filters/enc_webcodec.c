/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2023
 *					All rights reserved
 *
 *  This file is part of GPAC / WebCodec encoder filter
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
	char *c;
	GF_Fraction fintra;
	Bool all_intra;
	u32 b;
	u32 queued;

	GF_Filter *filter;
	u32 codecid, dsi_crc, timescale;
	u32 width, height, pfmt, stride, stride_uv, nb_planes, uv_height;
	Bool codec_init;
	u32 pf, out_size, in_flush;

	u32 afmt, sample_rate, num_channels, bytes_per_sample;

	GF_FilterPid *ipid, *opid;
	GF_List *src_pcks;
	GF_Err error, enc_error;

	Bool fintra_setup;
	u64 orig_ts;
	u32 nb_forced, nb_frames_in;
	char *pname;
} GF_WCEncCtx;

GF_EXPORT
void wcenc_on_error(GF_WCEncCtx *ctx, int state, char *msg)
{
	//decode error
	if (state > 1)
		ctx->enc_error = GF_BAD_PARAM;
	else if (state==1)
		ctx->error = GF_PROFILE_NOT_SUPPORTED;
	else {
		ctx->error = GF_OK;
		ctx->codec_init = GF_TRUE;
	}
}

EM_JS(int, wcenc_init, (int wc_ctx, int _codec_str, int bitrate, int width, int height, double FPS, int realtime, int sample_rate, int num_channels), {
	let codec_str = _codec_str ? libgpac.UTF8ToString(_codec_str) : null;
	let config = {};
	config.codec = codec_str;
	let enc_class = null;
	if (width) {
		config.width = width;
		config.height = height;
		config.framerate = FPS;
		config.latency = realtime ?  "realtime" : "quality";
		enc_class = VideoEncoder;
		if (codec_str.startsWith("avc")) {
			config.avc = { format: "annexb" };
		}
		else if (codec_str.startsWith("hvc1") || codec_str.startsWith("hev1")) {
			config.hevc = { format: "annexb" };
		}
	} else {
		config.sampleRate = sample_rate;
		config.numberOfChannels = num_channels;
		enc_class = AudioEncoder;
	}
	config.bitrate = bitrate;

	if (typeof libgpac._to_webenc != 'function') {
		libgpac._web_encs = [];
		libgpac._to_webenc = (ctx) => {
          for (let i=0; i<libgpac._web_encs.length; i++) {
            if (libgpac._web_encs[i]._wc_ctx==ctx) return libgpac._web_encs[i];
          }
          return null;
		};
		libgpac._on_wcenc_error = libgpac.cwrap('wcenc_on_error', null, ['number', 'number', 'string']);
		libgpac._on_wcenc_config = libgpac.cwrap('wcenc_on_config', null, ['number', 'number']);
		libgpac._on_wcenc_frame = libgpac.cwrap('wcenc_on_frame', null, ['number', 'bigint', 'number', 'number', 'number']);
		libgpac._on_wcenc_flush = libgpac.cwrap('wcenc_on_flush', null, ['number']);
	}
	enc_class.isConfigSupported(config).then( supported => {
		if (supported.supported) {
			let c = libgpac._to_webenc(wc_ctx);
			if (!c) {
				c = {_wc_ctx: wc_ctx, enc: null, _frame: null};
				libgpac._web_encs.push(c);
			}
			if (!c.enc) {
				let init_info = {
					error: (e) => {
						console.log(e.message);
					}
				};
				init_info.output = (chunk, metadata) => {
					let dsize = metadata.decoderConfig || null;
					if (dsize) dsize = dsize.description || null;
					if (dsize) dsize = dsize.byteLength || 0;
					if (dsize) {
						c.decoderConfig = metadata.decoderConfig.description;
						libgpac._on_wcenc_config(c._wc_ctx, c.decoderConfig.byteLength);
						c.decoderConfig = null;
					}
					c.chunk = chunk;
					let sap = 1;
					if (typeof chunk.type != 'undefined') sap = (chunk.type=="key") ? 1 : 0;
					libgpac._on_wcenc_frame(c._wc_ctx, BigInt(chunk.timestamp), chunk.duration, chunk.byteLength, sap);
					c.chunk = null;
				};
				c.enc = new enc_class(init_info);
			}
			c.enc.configure(config);
			libgpac._on_wcenc_error(wc_ctx, 0, null);
		} else {
			libgpac._on_wcenc_error(wc_ctx, 1, "Not Supported");
		}
	}).catch( (e) => {
		libgpac._on_wcenc_error(wc_ctx, 1, ""+e);
	});
})

static GF_Err wcenc_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	u32 streamtype;
	char *codec_par=NULL;
    GF_WCEncCtx *ctx = gf_filter_get_udta(filter);

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
	if (!p || (p->value.uint!=GF_CODECID_RAW)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[WebEnc] Missing codecid, cannot initialize\n"));
		return GF_NOT_SUPPORTED;
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!p) return GF_NOT_SUPPORTED;
	streamtype = p->value.uint;

	ctx->in_flush = 0;
	ctx->ipid = pid;

	ctx->codecid = gf_codecid_parse(ctx->c);
	if (!ctx->codecid) {
		char *sep = strchr(ctx->c, '.');
		if (sep) {
			sep[0] = 0;
			ctx->codecid = gf_codecid_parse(ctx->c);
			codec_par = sep;
			sep[0] = '.';
		}
	}
	if (!ctx->codecid) {
		return GF_NOT_SUPPORTED;
	}

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	}

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(ctx->codecid) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ISOM_STSD_TEMPLATE, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ISOM_SUBTYPE, NULL);

	switch (ctx->codecid) {
	//codecs for whoch we will need a reframer (DSI rebuild, DTS recompute and metadata extraction)
	case GF_CODECID_AVC:
	case GF_CODECID_HEVC:
	case GF_CODECID_VVC:
	case GF_CODECID_AV1:
	case GF_CODECID_VP8:
	case GF_CODECID_VP9:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED_FULL_AU, &PROP_BOOL(GF_TRUE) );
		break;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	ctx->timescale = p ? p->value.uint : 1000;
	Double fps=0;
	if (streamtype==GF_STREAM_VISUAL) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
		if (!p) return GF_BAD_PARAM;
		if (ctx->width != p->value.uint) {
			ctx->width = p->value.uint;
			ctx->codec_init = GF_FALSE;
		}
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
		if (!p) return GF_BAD_PARAM;
		if (ctx->height != p->value.uint) {
			ctx->height = p->value.uint;
			ctx->codec_init = GF_FALSE;
		}

		ctx->stride = ctx->stride_uv = 0;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE);
		if (p) ctx->stride = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE_UV);
		if (p) ctx->stride_uv = p->value.uint;

		ctx->nb_planes = 0;
		ctx->uv_height = 0;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
		if (p) {
			u32 ifmt = p->value.uint;
			ctx->pfmt = ifmt;
			ctx->pname = NULL;
			switch (ifmt) {
			case GF_PIXEL_YUV: ctx->pname = "I420"; break;
			case GF_PIXEL_YUVA: ctx->pname = "I420A"; break;
			case GF_PIXEL_YUV422: ctx->pname = "I422"; break;
			case GF_PIXEL_YUV444: ctx->pname = "I444"; break;
			case GF_PIXEL_NV12: ctx->pname = "NV12"; break;
			case GF_PIXEL_RGBA: ctx->pname = "RGBA"; break;
			case GF_PIXEL_RGBX: ctx->pname = "RGBX"; break;
			case GF_PIXEL_BGRA: ctx->pname = "BGRA"; break;
			case GF_PIXEL_BGRX: ctx->pname = "BGRX"; break;
			default:
				if (gf_pixel_fmt_is_yuv(ifmt)) {
					ctx->pfmt = gf_pixel_fmt_is_transparent(ifmt) ? GF_PIXEL_YUVA : GF_PIXEL_YUV;
				} else {
					ctx->pfmt = gf_pixel_fmt_is_transparent(ifmt) ? GF_PIXEL_RGBA : GF_PIXEL_RGBX;
				}
				break;
			}
			if (ctx->pfmt != ifmt) {
				gf_filter_pid_negociate_property(ctx->ipid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->pfmt) );
				return GF_OK;
			}

			gf_pixel_get_size_info(ctx->pfmt, ctx->width, ctx->height, NULL, &ctx->stride, &ctx->stride_uv, &ctx->nb_planes, &ctx->uv_height);
		}

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
		if (p) {
			fps = p->value.frac.num;
			fps /= p->value.frac.den;
		}
	} else {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
		if (!p) return GF_BAD_PARAM;
		if (ctx->sample_rate != p->value.uint) {
			ctx->sample_rate = p->value.uint;
			ctx->codec_init = GF_FALSE;
		}
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
		u32 ch = p ? p->value.uint : 1;
		if (ctx->num_channels != ch) {
			ctx->num_channels = ch;
			ctx->codec_init = GF_FALSE;
		}
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
		if (p) {
			u32 ifmt = p->value.uint;
			ctx->afmt = ifmt;
			ctx->pname = NULL;
			switch (ifmt) {
			case GF_AUDIO_FMT_U8: ctx->pname = "u8"; break;
			case GF_AUDIO_FMT_S16: ctx->pname = "s16"; break;
			case GF_AUDIO_FMT_S32: ctx->pname = "s32"; break;
			case GF_AUDIO_FMT_FLT: ctx->pname = "f32"; break;
			case GF_AUDIO_FMT_U8P: ctx->pname = "u8-planar"; break;
			case GF_AUDIO_FMT_S16P: ctx->pname = "s16-planar"; break;
			case GF_AUDIO_FMT_S32P: ctx->pname = "s32-planar"; break;
			case GF_AUDIO_FMT_FLTP: ctx->pname = "f32-planar"; break;
			default:
				ctx->afmt = GF_AUDIO_FMT_S32;
				break;
			}
			if (ctx->afmt != ifmt) {
				gf_filter_pid_negociate_property(ctx->ipid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->afmt) );
				return GF_OK;
			}
			ctx->bytes_per_sample = gf_audio_fmt_bit_depth(ctx->afmt) * ctx->num_channels / 8;
		}
	}
	if (ctx->codec_init) return GF_OK;


	char szCodec[RFC6381_CODEC_NAME_SIZE_MAX];
	strncpy(szCodec, ctx->c, RFC6381_CODEC_NAME_SIZE_MAX);
	u32 ctype = gf_codecid_4cc_type(ctx->codecid);
	if (ctype) {
		strcpy(szCodec, gf_4cc_to_str(ctype));

		if (codec_par) {
			strncat(szCodec, codec_par, RFC6381_CODEC_NAME_SIZE_MAX);
		} else {
			switch (ctx->codecid) {
			case GF_CODECID_AVC:
				strcpy(szCodec, "avc1.640028");
				break;
			case GF_CODECID_HEVC:
				strcpy(szCodec, "hvc1.1.6.L153.0");
				break;
			case GF_CODECID_VVC:
				strcpy(szCodec, "vvc1.1.H102.CQA");
				break;
			case GF_CODECID_AV1:
				strcpy(szCodec, "av01.0.08M.08");
				break;
			case GF_CODECID_AAC_MPEG2_MP:
			case GF_CODECID_AAC_MPEG2_LCP:
			case GF_CODECID_AAC_MPEG2_SSRP:
			case GF_CODECID_AAC_MPEG4:
				strcpy(szCodec, "mp4a.40.2");
				break;
			}
		}
		strlwr(szCodec);
	}

	ctx->error = GF_OK;
	//create encoder
	wcenc_init(EM_CAST_PTR ctx,
		EM_CAST_PTR szCodec,
		ctx->b ? ctx->b : (ctx->width ? 2000000 : 124000),
		ctx->width,
		ctx->height,
		fps,
		0,
		ctx->sample_rate,
		ctx->num_channels);

	return ctx->error;
}



EM_JS(int, wcenc_encode_frame, (int wc_ctx, u32 w, u32 h, u32 uv_h, int _format, u64 ts, u32 dur, u32 planes, u32 stride1, u32 stride2, u32 sap, int buf, u32 buf_size), {
	let c = libgpac._to_webenc(wc_ctx);
	if (!c || !buf || !_format) return;
	let format = libgpac.UTF8ToString(_format);

	//setup source frame
	let ab = new Uint8Array(libgpac.HEAPU8.buffer, buf, buf_size);
	let vbinit = {
		format: format,
		layout: [],
		codedWidth: w,
		codedHeight: h,
		timestamp: Number(ts),
		duration: dur,
	};
	let offset=0;
	for (let i=0; i<planes; i++) {
		let l={};
		let p_h=h;
		l.offset = offset;
		l.stride = stride1;

		if (i==4) l.stride = stride1;
		else if (i) {
			l.stride = stride2;
			p_h = uv_h;
		}
		offset += l.stride * p_h;
		vbinit.layout.push(l);
	}
	let vframe = new VideoFrame(ab, vbinit);
	c.enc.encode(vframe, {keyFrame: sap ? true : false} );
	vframe.close();
})

EM_JS(int, wcenc_encode_audio, (int wc_ctx, u32 sr, u32 ch, u32 frames, int _format, u64 ts, int buf, u32 buf_size), {
	let c = libgpac._to_webenc(wc_ctx);
	if (!c || !buf || !_format) return;
	let format = libgpac.UTF8ToString(_format);

	//setup source frame
	let adinit = {
		format: format,
		sampleRate: sr,
		numberOfChannels: ch,
		numberOfFrames: frames,
		timestamp: Number(ts),
		data: new Uint8Array(libgpac.HEAPU8.buffer, buf, buf_size)
	};
	let adata = new AudioData(adinit);
	c.enc.encode(adata);
	adata.close();
})


GF_EXPORT
void wcenc_on_flush(GF_WCEncCtx *ctx)
{
	ctx->in_flush = 2;
	gf_filter_pid_set_eos(ctx->opid);
}

EM_JS(int, wcenc_flush, (int wc_ctx), {
	let c = libgpac._to_webenc(wc_ctx);
	if (!c || !c.enc) return;
	c.enc.flush().then( () => {libgpac._on_wcenc_flush(c._wc_ctx); }).catch ( (e) => { libgpac._on_wcenc_flush(c._wc_ctx); } );
})


static GF_Err wcenc_process(GF_Filter *filter)
{
	u64 cts=0;
	GF_Err e;
	const u8 *in_buffer;
	u32 in_buffer_size;
	GF_WCEncCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;

	if (ctx->error)
		return ctx->error;
	if (!ctx->codec_init) return GF_OK;


	while (!ctx->enc_error) {
		pck = gf_filter_pid_get_packet(ctx->ipid);
		if (!pck) {
			if (gf_filter_pid_is_eos(ctx->ipid)) {
				if (!ctx->in_flush) {
					ctx->in_flush = 1;
					wcenc_flush(EM_CAST_PTR ctx);
				}
				if (ctx->in_flush == 1) {
					gf_filter_post_process_task(filter);
					return GF_OK;
				}
				return GF_EOS;
			}
			return GF_OK;
		}
		ctx->in_flush = 0;
		//don't push too fast
		if (gf_list_count(ctx->src_pcks) > ctx->queued) return GF_OK;

		in_buffer = (u8 *) gf_filter_pck_get_data(pck, &in_buffer_size);
		cts = gf_timestamp_rescale( gf_filter_pck_get_cts(pck), ctx->timescale, 1000000);

		gf_filter_pck_ref_props(&pck);
		gf_list_add(ctx->src_pcks, pck);

		//queue input
		if (ctx->width) {
			int sap = 0;
			const GF_PropertyValue *p;

			//force picture type
			if (ctx->all_intra)
				sap = 1;

			//if PCK_FILENUM is set on input, this is a file boundary, force IDR sync
			if (pck && gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM)) {
				sap = 1;
			}
			//if CUE_START is set on input, this is a segment boundary, force IDR sync
			p = pck ? gf_filter_pck_get_property(pck, GF_PROP_PCK_CUE_START) : NULL;
			if (p && p->value.boolean) {
				sap = 1;
			}

			//check if we need to force a closed gop
			if (pck && ctx->fintra.den && (ctx->fintra.num>0)) {
				if (!ctx->fintra_setup) {
					ctx->fintra_setup = GF_TRUE;
					ctx->orig_ts = cts;
					sap = 1;
					ctx->nb_forced=1;
				} else if (cts < ctx->orig_ts) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[WebEnc] timestamps not increasing monotonuously, resetting forced intra state !\n"));
					ctx->orig_ts = cts;
					sap = 1;
					ctx->nb_forced=1;
				} else {
					u64 ts_diff = cts - ctx->orig_ts;
					if (ts_diff * ctx->fintra.den >= ctx->nb_forced * ctx->fintra.num * 1000000) {
						sap = 1;
						ctx->nb_forced++;
						GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[WebEnc] Forcing IDR at frame %d (CTS %d / 1000000)\n", ctx->nb_frames_in, cts));
					}
				}
			}


			wcenc_encode_frame(EM_CAST_PTR ctx, ctx->width, ctx->height, ctx->uv_height, EM_CAST_PTR ctx->pname, cts, gf_filter_pck_get_duration(pck), ctx->nb_planes, ctx->stride, ctx->stride_uv, sap, EM_CAST_PTR in_buffer, in_buffer_size);
			ctx->nb_frames_in++;
		} else {
			u32 nb_samples = in_buffer_size / ctx->bytes_per_sample;

			wcenc_encode_audio(EM_CAST_PTR ctx, ctx->sample_rate, ctx->num_channels, nb_samples, EM_CAST_PTR ctx->pname, cts, EM_CAST_PTR in_buffer, in_buffer_size);
		}

		gf_filter_pid_drop_packet(ctx->ipid);
	}
	e = ctx->enc_error;
	ctx->enc_error = GF_OK;
	return e;
}

EM_JS(int, wcenc_get_config, (int wc_ctx, int buf, int buf_size), {
	let c = libgpac._to_webenc(wc_ctx);
	if (!c || !c.decoderConfig) {
		throw 'Bad Param';
		return;
	}
	//setup dst
	let dst = new Uint8Array(libgpac.HEAPU8.buffer, buf, buf_size);
	dst.set(c.decoderConfig);
})

GF_EXPORT
void wcenc_on_config(GF_WCEncCtx *ctx, int size)
{
	u8 *buf = gf_malloc(size);
	memset(buf, 0, size);
	wcenc_get_config(EM_CAST_PTR ctx, EM_CAST_PTR buf, size);
	u32 dsi_crc = gf_crc_32(buf, size);
	if (dsi_crc != ctx->dsi_crc) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(buf, size));
		ctx->dsi_crc = dsi_crc;
	} else {
		gf_free(buf);
	}
}

EM_JS(int, wcenc_get_frame, (int wc_ctx, int buf, int buf_size), {
	let c = libgpac._to_webenc(wc_ctx);
	if (!c || !c.chunk) {
		throw 'Bad Param';
		return;
	}
	//setup dst
	let dst = new Uint8Array(libgpac.HEAPU8.buffer, buf, buf_size);
	c.chunk.copyTo(dst);
})

GF_EXPORT
void wcenc_on_frame(GF_WCEncCtx *ctx, u64 timestamp, u32 duration, u32 size, int is_key)
{
	u8 *output;
	u32 i, count;
	GF_FilterPacket *src_pck=NULL;

	GF_FilterPacket *dst = gf_filter_pck_new_alloc(ctx->opid, size, &output);
	if (!dst) return;

	wcenc_get_frame(EM_CAST_PTR ctx, EM_CAST_PTR output, size);

	u64 cts = gf_timestamp_rescale(timestamp, 1000000, ctx->timescale);

	count = gf_list_count(ctx->src_pcks);
	for (i=0; i<count; i++) {
		src_pck = gf_list_get(ctx->src_pcks, i);
		s64 ts = gf_filter_pck_get_cts(src_pck);
		if (ABS(ts - cts) <= 1) {
			gf_list_rem(ctx->src_pcks, i);
			break;
		}
		if (ctx->sample_rate && (cts>=ts)) {
			gf_list_rem(ctx->src_pcks, i);
			break;
		}
		src_pck = NULL;
	}

	gf_filter_pck_set_cts(dst, cts);
	if (src_pck) {
		gf_filter_pck_merge_properties(src_pck, dst);
		gf_filter_pck_unref(src_pck);
		gf_filter_pck_set_byte_offset(dst, GF_FILTER_NO_BO);
	}
	gf_filter_pck_set_sap(dst, is_key ? GF_FILTER_SAP_1 : GF_FILTER_SAP_NONE);
	gf_filter_pck_send(dst);

	if (ctx->sample_rate) {
		while (gf_list_count(ctx->src_pcks)) {
			src_pck = gf_list_pop_back(ctx->src_pcks);
			gf_filter_pck_unref(src_pck);
		}
	}
}



GF_Err wcenc_initialize(GF_Filter *filter)
{
    GF_WCEncCtx *ctx = gf_filter_get_udta(filter);
    ctx->filter = filter;
	ctx->src_pcks = gf_list_new();
    return GF_OK;
}

EM_JS(int, wcenc_del, (int wc_ctx), {
	if (typeof libgpac._web_encs != 'array') return;
	for (let i=0; i<libgpac._web_encs.length; i++) {
		if (libgpac._web_encs[i]._wc_ctx == wc_ctx) {
			libgpac._web_encs[i].enc.close();
			libgpac._web_encs.splice(i, 1);
			return;
		}
	}
})


void wcenc_finalize(GF_Filter *filter)
{
    GF_WCEncCtx *ctx = gf_filter_get_udta(filter);
    wcenc_del(EM_CAST_PTR ctx);

    while (gf_list_count(ctx->src_pcks)) {
		GF_FilterPacket *pck = gf_list_pop_back(ctx->src_pcks);
		gf_filter_pck_unref(pck);
	}
	gf_list_del(ctx->src_pcks);
}

static GF_FilterCapability WCEncCapsV[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_TILE_BASE, GF_TRUE),
};

static GF_FilterCapability WCEncCapsA[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

static GF_FilterCapability WCEncCapsAV[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_TILE_BASE, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

#define OFFS(_n)	#_n, offsetof(GF_WCEncCtx, _n)

static const GF_FilterArgs WCEncArgs[] =
{
	{ OFFS(c), "codec identifier. Can be any supported GPAC codec name or ffmpeg codec name - updated to ffmpeg codec name after initialization", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(fintra), "force intra / IDR frames at the given period in sec, e.g. `fintra=2` will force an intra every 2 seconds and `fintra=1001/1000` will force an intra every 30 frames on 30000/1001=29.97 fps video; ignored for audio", GF_PROP_FRACTION, "-1/1", NULL, 0},

	{ OFFS(all_intra), "only produce intra frames", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(b), "bitrate in bits per seconds, defaults to 2M for video and 124K for audio", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(queued), "maximum number of packets to queue in webcodec instance", GF_PROP_UINT, "10", NULL, 0},
	{0}
};

GF_FilterRegister GF_WCEncCtxRegister = {
	.name = "wcenc",
	GF_FS_SET_DESCRIPTION("WebCodec encoder")
	GF_FS_SET_HELP("This filter encodes video streams using WebCodec encoder of the browser")
	.args = WCEncArgs,
	SETCAPS(WCEncCapsAV),
	.flags = GF_FS_REG_SINGLE_THREAD|GF_FS_REG_ASYNC_BLOCK,
	.private_size = sizeof(GF_WCEncCtx),
	.initialize = wcenc_initialize,
	.finalize = wcenc_finalize,
	.configure_pid = wcenc_configure_pid,
	.process = wcenc_process,
};


const GF_FilterRegister *wcenc_register(GF_FilterSession *session)
{
	
	int has_webv_encode = EM_ASM_INT({
		if (typeof VideoEncoder == 'undefined') return 0;
		return 1;
	});
	int has_weba_encode = EM_ASM_INT({
		if (typeof AudioEncoder == 'undefined') return 0;
		return 1;
	});

	if (!has_webv_encode && !has_weba_encode) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[WebEnc] No WebEncoder support\n"));
		return NULL;
	}
	if (!has_webv_encode) {
		GF_WCEncCtxRegister.caps = WCEncCapsA;
		GF_WCEncCtxRegister.nb_caps = sizeof(WCEncCapsA)/sizeof(GF_FilterCapability);
	} else if (!has_weba_encode) {
		GF_WCEncCtxRegister.caps = WCEncCapsV;
		GF_WCEncCtxRegister.nb_caps = sizeof(WCEncCapsV)/sizeof(GF_FilterCapability);
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[WebEnc] AudioEncoder %d - VideoEncoder %d\n", has_weba_encode, has_webv_encode));
	return &GF_WCEncCtxRegister;
}
#endif
