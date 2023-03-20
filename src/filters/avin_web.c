/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2023
 *					All rights reserved
 *
 *  This file is part of GPAC / Camera/Mic/Canvas grabber filter
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
#include <gpac/network.h>

typedef struct
{
	char *src;
	GF_PropVec2i vsize;
	Bool back, ntp, alpha;
	GF_Fraction fps;

	GF_Err init_err;
	Bool a_playing, v_playing;
	GF_Filter *filter;
	GF_FilterPid *vpid, *apid;
	u32 w, h, pfmt, out_size;
	u32 samplerate, channels, afmt;
	GF_FilterPacket *pck_v;

	Bool init_play_done;
	Bool is_canvas;
	u32 bytes_per_sample;
	u64 canvas_last_ts, canvas_init_ts;
	u32 nb_frames;
	u64 next_time;

	GF_List *video_pcks;
	GF_List *audio_pcks;
} GF_WebGrab;

EM_JS(int, webgrab_next_video, (int wg_ctx),
{
	let c = libgpac._to_webgrab(wg_ctx);
	if (!c) {
		libgpac._on_wgrab_error(c._wg_ctx, 2, "Invalid source");
		return;
	}
	if (c.canvas) {
		try {
			c._frame = new VideoFrame(c.canvas, {timestamp: 0, alpha: c.keep_alpha ? "keep" : "discard"} );
			libgpac._on_wgrab_video_frame(c._wg_ctx, c._frame.codedWidth, c._frame.codedHeight, c._frame.format, BigInt(c._frame.timestamp));
		} catch (e) {
			libgpac._on_wgrab_error(wg_ctx, 2, ""+e);
		}
		return;
	}
	if (c.video_reader==null) return;

	c.video_reader.read().then( result => {
		if (result.done) {
			libgpac._on_wgrab_error(c._wg_ctx, 3, null);
			return;
		}
		c._frame = result.value;
		libgpac._on_wgrab_video_frame(c._wg_ctx, c._frame.codedWidth, c._frame.codedHeight, c._frame.format, BigInt(c._frame.timestamp));
	}).catch(e => {
		libgpac._on_wgrab_error(wg_ctx, 2, ""+e);
	});
})

EM_JS(int, webgrab_next_audio, (int wg_ctx),
{
	let c = libgpac._to_webgrab(wg_ctx);
	if (!c) {
		libgpac._on_wgrab_error(c._wg_ctx, 2, "Invalid source");
		return;
	}
	if (c.audio_reader==null) return;

	c.audio_reader.read().then( result => {
		if (result.done) {
			libgpac._on_wgrab_error(c._wg_ctx, 3, null);
			return;
		}
		c._frame = result.value;
		libgpac._on_wgrab_audio_data(c._wg_ctx, c._frame.sampleRate, c._frame.numberOfChannels, c._frame.format, c._frame.numberOfFrames, BigInt(c._frame.timestamp));
		c._frame.close();
		c._frame = null;

	}).catch(e => {
		libgpac._on_wgrab_error(wg_ctx, 2, ""+e);
	});
})


static GF_Err webgrab_process(GF_Filter *filter)
{
    GF_WebGrab *ctx = gf_filter_get_udta(filter);
	if (ctx->init_err) return ctx->init_err;
	if (!ctx->init_play_done) return GF_OK;

	if (!ctx->v_playing && !ctx->a_playing && ctx->init_play_done) return GF_EOS;

	while (gf_list_count(ctx->video_pcks)) {
		GF_FilterPacket *pck = gf_list_pop_front(ctx->video_pcks);
		gf_filter_pck_send(pck);

		if (ctx->is_canvas && (ctx->fps.num<=0)) {
			gf_filter_pid_set_eos(ctx->vpid);
		}
	}

	while (gf_list_count(ctx->audio_pcks)) {
		GF_FilterPacket *pck = gf_list_pop_front(ctx->audio_pcks);
		gf_filter_pck_send(pck);
	}

	if (ctx->is_canvas && ctx->next_time) {
		u64 now = gf_sys_clock_high_res();
		if (now > ctx->next_time) {
			ctx->next_time = 0;
			webgrab_next_video(EM_CAST_PTR ctx);
		}
	}
	return GF_OK;
}


static Bool webgrab_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
    GF_WebGrab *ctx = gf_filter_get_udta(filter);
	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (evt->base.on_pid==ctx->vpid) {
			if (!ctx->v_playing) webgrab_next_video(EM_CAST_PTR ctx);
			ctx->v_playing = GF_TRUE;
		} else {
			if (!ctx->a_playing) webgrab_next_audio(EM_CAST_PTR ctx);
			ctx->a_playing = GF_TRUE;
		}
		ctx->init_play_done = GF_TRUE;
		break;
	case GF_FEVT_STOP:
		if (evt->base.on_pid==ctx->vpid) {
			ctx->v_playing = GF_FALSE;
		} else {
			ctx->a_playing = GF_FALSE;
		}
		break;
	default:
		break;
	}
	return GF_TRUE;
}
GF_EXPORT
void webgrab_on_error(GF_WebGrab *ctx, int res_code, char *msg)
{
	if (res_code) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[WebGrab] Failed to %s: %s\n", (res_code==1) ? "initialize" : "grab frame", msg ? msg : "unknown error"));
		if (res_code==1) {
			ctx->init_err = GF_SERVICE_ERROR;
			gf_filter_setup_failure(ctx->filter, GF_SERVICE_ERROR);
		}
	}
}

u32 webcodec_pixfmt_to_gpac(char *format);

EM_JS(int, wgrab_copy_frame, (int wg_ctx, int dst_pck, int buf, int buf_size), {
	let c = libgpac._to_webgrab(wg_ctx);
	if (!c || !c._frame || !dst_pck) return;

	//setup dst
	let ab = new Uint8Array(libgpac.HEAPU8.buffer, buf, buf_size);
	let frame = c._frame;
	c._frame = null;
	frame.copyTo(ab).then( layout => {
		libgpac._on_wgrab_frame_copy(c._wg_ctx, dst_pck, 1);
		frame.close();
	}).catch( e => {
		libgpac._on_wgrab_frame_copy(c._wg_ctx, dst_pck, 0);
		frame.close();
	});
})

GF_EXPORT
void webgrab_on_frame_copy(GF_WebGrab *ctx, GF_FilterPacket *pck, int res_ok)
{
	if (!ctx) return;
	if (!res_ok) gf_filter_pck_discard(ctx->pck_v);
	else gf_list_add(ctx->video_pcks, ctx->pck_v);

	gf_filter_post_process_task(ctx->filter);
	if (ctx->v_playing && !ctx->is_canvas)
		webgrab_next_video(EM_CAST_PTR ctx);
}

GF_EXPORT
void webgrab_on_video_frame(GF_WebGrab *ctx, u32 width, u32 height, char * pixfmt, u64 ts)
{
	Bool first=GF_FALSE;

	u32 pf = webcodec_pixfmt_to_gpac(pixfmt);
	if (!ctx->vpid) {
		ctx->vpid = gf_filter_pid_new(ctx->filter);
		first = GF_TRUE;
		gf_filter_pid_set_property(ctx->vpid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL));
		gf_filter_pid_set_property(ctx->vpid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));
		gf_filter_pid_set_property(ctx->vpid, GF_PROP_PID_TIMESCALE, &PROP_UINT(1000000));
		gf_filter_pid_set_property(ctx->vpid, GF_PROP_PID_RAWGRAB, &PROP_UINT(ctx->back ? 2 : 1) );
	}
	if ((width != ctx->w) || (height != ctx->h) || (pf != ctx->pfmt)) {
		gf_filter_pid_set_property(ctx->vpid, GF_PROP_PID_WIDTH, &PROP_UINT(width));
		gf_filter_pid_set_property(ctx->vpid, GF_PROP_PID_HEIGHT, &PROP_UINT(height));
		gf_filter_pid_set_property(ctx->vpid, GF_PROP_PID_PIXFMT, &PROP_UINT(pf));
		ctx->w = width;
		ctx->h = height;
		ctx->pfmt = pf;
		gf_pixel_get_size_info(pf, width, height, &ctx->out_size, NULL, NULL, NULL, NULL);
	}
	if (first) {
		return;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[WebGrab] Got frame ts "LLU"\n", ts));

	u8 *output;
	ctx->pck_v = gf_filter_pck_new_alloc(ctx->vpid, ctx->out_size, &output);
	if (!ctx->pck_v) return;

	if (ctx->is_canvas) {
		u64 now = gf_sys_clock_high_res();
		if (!ctx->canvas_init_ts) ctx->canvas_init_ts = now;
		ctx->canvas_last_ts = now;
		ts = now - ctx->canvas_init_ts;
		ctx->nb_frames++;
		if ((ctx->fps.num>0) && (ctx->fps.den>0)) {
			ctx->next_time = ctx->canvas_init_ts + gf_timestamp_rescale(ctx->nb_frames*ctx->fps.den, ctx->fps.num, 1000000);
		}
	}

	gf_filter_pck_set_cts(ctx->pck_v, ts);
	if (ctx->ntp) {
		u64 ntp = gf_net_get_ntp_ts();
		gf_filter_pck_set_property(ctx->pck_v, GF_PROP_PCK_SENDER_NTP, &PROP_LONGUINT(ntp) );
	}

	wgrab_copy_frame(EM_CAST_PTR ctx,
		EM_CAST_PTR ctx->pck_v,
		EM_CAST_PTR output, ctx->out_size);

}

u32 webcodec_audiofmt_to_gpac(char *format);

EM_JS(int, wgrab_copy_audio, (int wg_ctx, int buf, int buf_size, int plane_index), {
	let c = libgpac._to_webgrab(wg_ctx);
	if (!c || !c._frame) return;

	//setup dst
	let ab = new Uint8Array(libgpac.HEAPU8.buffer, buf, buf_size);
	c._frame.copyTo(ab, { planeIndex: plane_index });
})

GF_EXPORT
void webgrab_on_audio_data(GF_WebGrab *ctx, u32 sr, u32 channels, char * afmt, u32 num_frames, u64 ts)
{
	Bool first=GF_FALSE;

	u32 af = webcodec_audiofmt_to_gpac(afmt);
	if (!ctx->apid) {
		ctx->apid = gf_filter_pid_new(ctx->filter);
		first = GF_TRUE;
		gf_filter_pid_set_property(ctx->apid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO));
		gf_filter_pid_set_property(ctx->apid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));
		gf_filter_pid_set_property(ctx->apid, GF_PROP_PID_TIMESCALE, &PROP_UINT(1000000));
		gf_filter_pid_set_property(ctx->apid, GF_PROP_PID_RAWGRAB, &PROP_UINT(1) );
	}
	if ((sr != ctx->samplerate) || (channels != ctx->channels) || (af != ctx->afmt)) {
		gf_filter_pid_set_property(ctx->apid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(sr));
		gf_filter_pid_set_property(ctx->apid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(channels));
		gf_filter_pid_set_property(ctx->apid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(af));
		ctx->samplerate = sr;
		ctx->channels = channels;
		ctx->afmt = af;
		ctx->bytes_per_sample = gf_audio_fmt_bit_depth(af) / 8;
	}
	if (first) {
		return;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[WebGrab] Got audio frame ts "LLU"\n", ts));

	u32 i, output_size = ctx->bytes_per_sample*num_frames*channels;
	u8 *output;
	GF_FilterPacket *dst = gf_filter_pck_new_alloc(ctx->apid, output_size, &output);
	if (!dst) return;

	gf_filter_pck_set_cts(dst, ts);
	if (ctx->ntp) {
		u64 ntp = gf_net_get_ntp_ts();
		gf_filter_pck_set_property(dst, GF_PROP_PCK_SENDER_NTP, &PROP_LONGUINT(ntp) );
	}

	//audioData copyTo is sync
	u32 num_planes, plane_size;
	if (gf_audio_fmt_is_planar(af)) {
		num_planes = channels;
		plane_size = ctx->bytes_per_sample*num_frames;
	} else {
		num_planes = 1;
		plane_size = ctx->bytes_per_sample*num_frames*channels;
	}

	for (i=0; i<num_planes; i++) {
		u8 *buf = output + i * plane_size;
		wgrab_copy_audio(EM_CAST_PTR ctx, EM_CAST_PTR buf, plane_size, i);
	}

	gf_list_add(ctx->audio_pcks, dst);
	gf_filter_post_process_task(ctx->filter);
	if (ctx->a_playing)
		webgrab_next_audio(EM_CAST_PTR ctx);
}

EM_JS(int, webgrab_start_usermedia, (int wg_ctx, int vid, int aud, int canv_id, int w, int h, int back, int alpha),
{
	if (typeof libgpac._to_webgrab != 'function') {
		libgpac._web_grabs = [];
		libgpac._to_webgrab = (ctx) => {
          for (let i=0; i<libgpac._web_grabs.length; i++) {
            if (libgpac._web_grabs[i]._wg_ctx==ctx) return libgpac._web_grabs[i];
          }
          return null;
		};
		libgpac._on_wgrab_error = libgpac.cwrap('webgrab_on_error', null, ['number', 'number', 'string']);
		libgpac._on_wgrab_video_frame = libgpac.cwrap('webgrab_on_video_frame', null, ['number', 'number', 'number', 'string', 'bigint']);
		libgpac._on_wgrab_audio_data = libgpac.cwrap('webgrab_on_audio_data', null, ['number', 'number', 'number', 'string', 'number', 'bigint']);
		libgpac._on_wgrab_frame_copy = libgpac.cwrap('webgrab_on_frame_copy', null, ['number', 'number', 'number']);
	}

	let c = libgpac._to_webgrab(wg_ctx);
	if (!c) {
		c = {_wg_ctx: wg_ctx, stream: null, canvas: null, video_reader: null, audio_reader: null, _frame: null};
		libgpac._web_grabs.push(c);
	}
	if (canv_id) {
		let canvas_id = libgpac.UTF8ToString(canv_id);
		c.canvas = document.getElementById(canvas_id);
		c.keep_alpha = alpha;
		if (!c.canvas) {
			libgpac._on_wgrab_error(wg_ctx, 1, "No such element "+canvas_id);
			return;
		}
		try {
			let frame = new VideoFrame(c.canvas, {timestamp: 0, alpha: c.keep_alpha ? "keep" : "discard"} );
			libgpac._on_wgrab_video_frame(c._wg_ctx, frame.codedWidth, frame.codedHeight, frame.format, BigInt(frame.timestamp));
			frame.close();
		} catch (e) {
			libgpac._on_wgrab_error(wg_ctx, 1, ""+e);
		}
		return;
	}

	let constraints = {
		audio: aud ? true : false,
		video: vid ? true : false,
	};
	if (w || h || back) {
		constraints.video = { };
		if (w) constraints.video.width = {ideal: w};
		if (h) constraints.video.height = {ideal: h};
		if (back) constraints.video.facingMode = {ideal: 'environment'};
	}

	navigator.mediaDevices.getUserMedia(constraints).then( stream => {
		c.stream = stream;
		stream.getTracks().forEach(track => {
			if (track.kind=='audio') {
				let trackProcessor = new MediaStreamTrackProcessor(track);
				c.audio_reader = trackProcessor.readable.getReader();
				c.audio_reader.read().then( result => {
					if (result.done) return;
					const frame = result.value;
					libgpac._on_wgrab_audio_data(c._wg_ctx, frame.sampleRate, frame.numberOfChannels, frame.format, frame.numberOfFrames, BigInt(frame.timestamp));
					frame.close();
				}).catch(e => {
					libgpac._on_wgrab_error(wg_ctx, 2, ""+e);
				});
			}
			else if (track.kind=='video') {
				let trackProcessor = new MediaStreamTrackProcessor(track);
				c.video_reader = trackProcessor.readable.getReader();
				c.video_reader.read().then( result => {
					if (result.done) return;
					const frame = result.value;
					libgpac._on_wgrab_video_frame(c._wg_ctx, frame.codedWidth, frame.codedHeight, frame.format, BigInt(frame.timestamp));
					frame.close();
				}).catch(e => {
					libgpac._on_wgrab_error(wg_ctx, 2, ""+e);
				});
			}
		});
	}).catch(e => {
		libgpac._on_wgrab_error(wg_ctx, 1, ""+e);

	});

})

GF_Err webgrab_initialize(GF_Filter *filter)
{
	Bool use_video=GF_FALSE, use_audio=GF_FALSE;
	char *canvas_id=NULL;
    GF_WebGrab *ctx = gf_filter_get_udta(filter);
    ctx->filter = filter;

    if (!ctx->src) return GF_BAD_PARAM;

    if (!strcmp(ctx->src, "video://")) use_video = GF_TRUE;
	else if (!strncmp(ctx->src, "video://", 8)) {
		canvas_id = ctx->src+8;
		ctx->is_canvas = GF_TRUE;
    }
    else if (!strcmp(ctx->src, "audio://")) use_audio = GF_TRUE;
    else if (!strcmp(ctx->src, "av://")) use_video = use_audio = GF_TRUE;
    else
		return GF_BAD_PARAM;

	gf_filter_prevent_blocking(filter, GF_TRUE);
	webgrab_start_usermedia(EM_CAST_PTR ctx, use_video, use_audio, EM_CAST_PTR canvas_id, ctx->vsize.x, ctx->vsize.y, ctx->back, ctx->alpha);

	ctx->video_pcks = gf_list_new();
	ctx->audio_pcks = gf_list_new();
    return GF_OK;
}

EM_JS(int, wgrab_del, (int wg_ctx), {
	if (! Array.isArray(libgpac._web_grabs) ) return;
	for (let i=0; i<libgpac._web_grabs.length; i++) {
		if (libgpac._web_grabs[i]._wg_ctx == wg_ctx) {
			libgpac._web_grabs[i]._wg_ctx = null;
			if (libgpac._web_grabs[i].stream) {
				libgpac._web_grabs[i].stream.getTracks().forEach(function(track) {
				  track.stop();
				});
			}
			libgpac._web_grabs.splice(i, 1);
			return;
		}
	}
})

void webgrab_finalize(GF_Filter *filter)
{
    GF_WebGrab *ctx = gf_filter_get_udta(filter);
	wgrab_del(EM_CAST_PTR ctx);
	while (gf_list_count(ctx->video_pcks)) {
		GF_FilterPacket *pck = gf_list_pop_front(ctx->video_pcks);
		gf_filter_pck_discard(pck);
	}
	gf_list_del(ctx->video_pcks);
	while (gf_list_count(ctx->audio_pcks)) {
		GF_FilterPacket *pck = gf_list_pop_front(ctx->audio_pcks);
		gf_filter_pck_discard(pck);
	}
	gf_list_del(ctx->audio_pcks);
}

static GF_FilterProbeScore webgrab_probe_url(const char *url, const char *mime)
{
	if (!strncmp(url, "video://", 8)) return GF_FPROBE_MAYBE_SUPPORTED;
	if (!strncmp(url, "audio://", 8)) return GF_FPROBE_MAYBE_SUPPORTED;
	if (!strncmp(url, "av://", 5)) return GF_FPROBE_MAYBE_SUPPORTED;
	if (!strncmp(url, "canvas://", 8)) return GF_FPROBE_MAYBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

static GF_FilterCapability WebGrabCaps[] =
{
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

#define OFFS(_n)	#_n, offsetof(GF_WebGrab, _n)

static const GF_FilterArgs WebGrabArgs[] =
{
	{ OFFS(src), "source url", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(vsize), "desired webcam resolution", GF_PROP_VEC2I, "0x0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(back), "use back camera", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ntp), "mark packets with NTP", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(alpha), "keep alpha when brabbing canvas", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(fps), "framerate to use when grabbing images - 0 FPS means single image", GF_PROP_FRACTION, "0/1", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};

GF_FilterRegister GF_WebGrabRegister = {
	.name = "webgrab",
	GF_FS_SET_DESCRIPTION("Frame grabber for web audio and video")
	GF_FS_SET_HELP("This filter grabs audio and video streams MediaStreamTrackProcessor of the browser\n"
	"\n"
	"Supported URL schemes:\n"
	"- video:// grabs from camera\n"
	"- audio:// grabs from microphone\n"
	"- av:// grabs both audio from microphone and video from camera\n"
	"- video://ELTID grabs from DOM element with ID `ELTID` (the element must be a valid element accepted by `VideoFrame` constructor)\n"
	)
	.args = WebGrabArgs,
	SETCAPS(WebGrabCaps),
	.flags = GF_FS_REG_SINGLE_THREAD,
	.private_size = sizeof(GF_WebGrab),
	.initialize = webgrab_initialize,
	.finalize = webgrab_finalize,
	.process = webgrab_process,
	.process_event = webgrab_process_event,
	.probe_url = webgrab_probe_url,
};


const GF_FilterRegister *webgrab_register(GF_FilterSession *session)
{
	
	int has_media_track_processor = EM_ASM_INT({
		if ('MediaStreamTrackProcessor' in window) return 1;
		return 0;
	});
	if (!has_media_track_processor) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[WebGrab] No MediaStreamTrackProcessor support\n"));
		return NULL;
	}
	return &GF_WebGrabRegister;
}
#endif
