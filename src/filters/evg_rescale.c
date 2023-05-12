/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2023
 *					All rights reserved
 *
 *  This file is part of GPAC / EVG rescaler filter
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

#ifndef GPAC_DISABLE_EVG
#include <gpac/evg.h>

enum
{
	EVGS_KEEPAR_OFF=0,
	EVGS_KEEPAR_FULL,
	EVGS_KEEPAR_NOSRC,
};

typedef struct
{
	//options
	GF_PropVec2i osize;
	u32 ofmt, nbth;
	Bool ofr, hq;
	char *padclr;
	u32 keepar;
	GF_Fraction osar;

	//internal data
	GF_FilterPid *ipid;
	u32 i_w, i_h, i_stride, i_stride_uv, i_pfmt, i_planes, i_size;
	Bool i_has_alpha;

	GF_FilterPid *opid;
	u32 o_size, o_w, o_h, o_stride, o_stride_uv;

	GF_Fraction ar;
	Bool passthrough, fullrange;
	u32 offset_w, offset_h;

	GF_EVGSurface *surf;
	GF_EVGStencil *tx;
	GF_Path *path;
} EVGScaleCtx;

u32 gf_evg_stencil_get_pixel_fast(GF_EVGStencil *st, s32 x, s32 y);
u64 gf_evg_stencil_get_pixel_wide_fast(GF_EVGStencil *st, s32 x, s32 y);

static GF_Err evgs_process(GF_Filter *filter)
{
	const char *data;
	u8 *output;
	u32 isize;
	GF_FilterPacket *dst_pck;
	GF_FilterFrameInterface *frame_ifce;
	EVGScaleCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;

	pck = gf_filter_pid_get_packet(ctx->ipid);

	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	if (ctx->passthrough) {
		gf_filter_pck_forward(pck, ctx->opid);
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}
	//not yet configured
	if (!ctx->ofmt && !ctx->o_w && !ctx->o_h)
		return GF_OK;

	if (!ctx->surf) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NOT_SUPPORTED;
	}

	data = gf_filter_pck_get_data(pck, &isize);
	frame_ifce = gf_filter_pck_get_frame_interface(pck);
	//we may have buffer input (padding) but shall not have smaller
	if (isize && (ctx->i_size > isize) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[EVGS] Mismatch in source osize, expected %d got %d - stride issue ?\n", ctx->i_size, isize));
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NOT_SUPPORTED;
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->o_size, &output);
	if (!dst_pck) return GF_OUT_OF_MEM;

	gf_filter_pck_merge_properties(pck, dst_pck);

#define CHK_EXIT(_msg) if (e) {\
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[EVGS] "_msg": %s\n", gf_error_to_string(e) )); \
			gf_filter_pck_discard(dst_pck); \
			gf_filter_pid_drop_packet(ctx->ipid); \
			return e;\
		}

	GF_Err e;
	e = gf_evg_surface_attach_to_buffer(ctx->surf, output, ctx->o_w, ctx->o_h, 0, ctx->o_stride, ctx->ofmt);
	CHK_EXIT("Failed to create output surface");
	//threading must be enabled once the surface is configured
	gf_evg_enable_threading(ctx->surf, ctx->nbth);

	if (ctx->offset_w || ctx->offset_h) {
		u32 color = ctx->padclr ? gf_color_parse(ctx->padclr) : 0xFF000000;
		e = gf_evg_surface_clear(ctx->surf, NULL, color);
		CHK_EXIT("Failed to clear surface");
	}

	if (data) {
		e = gf_evg_stencil_set_texture_planes(ctx->tx, ctx->i_w, ctx->i_h, ctx->i_pfmt, data, ctx->i_stride, NULL, NULL, ctx->i_stride_uv, NULL, 0);
	} else if (frame_ifce && frame_ifce->get_plane) {
		u32 i, stride_y=0, stride_uv=0, stride_a=0;
		const u8 *pY=NULL, *pU=NULL, *pV=NULL, *pA=NULL;
		for (i=0; i<ctx->i_planes; i++) {
			const u8 *ptr;
			u32 stride;
			e = frame_ifce->get_plane(frame_ifce, i, (const u8 **) &ptr, &stride);
			if (e) break;
			if (!i) {
				pY = ptr;
				stride_y = stride;
			} else if (ctx->i_has_alpha && (i+1==ctx->i_planes) ) {
				pA = ptr;
				stride_a = stride;
			}
			else if (i==1) {
				pU = ptr;
				stride_uv = stride;
			} else {
				pV = ptr;
			}
		}
		if (!e)
			e = gf_evg_stencil_set_texture_planes(ctx->tx, ctx->i_w, ctx->i_h, ctx->i_pfmt, pY, stride_y, pU, pV, stride_uv, pA, stride_a);
	} else {
		e = GF_NOT_SUPPORTED;
	}
	CHK_EXIT("Failed setup stencil from input packet");

	e = gf_evg_surface_set_path(ctx->surf, ctx->path);
	CHK_EXIT("Failed to set surface path");
	e = gf_evg_surface_fill(ctx->surf, ctx->tx);
	CHK_EXIT("Failed to fill surface");

	gf_filter_pck_send(dst_pck);
	gf_filter_pid_drop_packet(ctx->ipid);
	return e;
}

static GF_Err evgs_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	u32 w, h, stride, stride_uv, ofmt;
	GF_Fraction sar;
	Bool fullrange=GF_FALSE;
	EVGScaleCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}

	if (!ctx->ipid) {
		ctx->ipid = pid;
	}

	//if nothing is set we, consider we run as an adaptation filter, wait for reconfiguration to be called to declare output format
	if (!ctx->ofmt && !ctx->osize.x && !ctx->osize.y) {
		//we were explicitly loaded, act as a passthrough filter until we get a reconfig
		//we must do so for cases where the declared properties match the consuming format (so reconfiguration will never be called)
		if (!gf_filter_is_dynamic(filter)) {
			gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
			//make sure we init at some default values as filters down the chain will check for w/h/pfmt
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
			if (!p) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(128));
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
			if (!p) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(128));
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
			if (!p) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_RGB));

			ctx->passthrough = GF_TRUE;
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[EVGS] Running in passthrough mode\n"));
		}
		return GF_OK;
	}



	w = h = ofmt = stride = stride_uv = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (p) w = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (p) h = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE);
	if (p) stride = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE_UV);
	if (p) stride_uv = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
	if (p) ofmt = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAR);
	if (p) sar = p->value.frac;
	else sar.den = sar.num = 1;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_COLR_RANGE);
	if (p) fullrange = p->value.boolean;

	//ctx->ofmt may be 0 if the filter is instantiated dynamically, we haven't yet been called for reconfigure
	if (!w || !h || !ofmt) {
		return GF_OK;
	}
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);

	if (!ctx->ofmt)
		ctx->ofmt = ofmt;

	ctx->passthrough = GF_FALSE;

	Bool downsample_w=GF_FALSE, downsample_h=GF_FALSE;
	switch (ofmt) {
	case GF_PIXEL_YUV:
	case GF_PIXEL_YVU:
	case GF_PIXEL_YUV_10:
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV21:
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_NV21_10:
	case GF_PIXEL_YUVA:
	case GF_PIXEL_YUVD:
		downsample_h=GF_TRUE;
		//fallthrough
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_UYVY_10:
	case GF_PIXEL_VYUY_10:
	case GF_PIXEL_YUYV_10:
	case GF_PIXEL_YVYU_10:
		downsample_w = GF_TRUE;
		break;

	default:
		break;
	}

	u32 scale_w = w;
	if ((ctx->keepar == EVGS_KEEPAR_FULL) && (sar.num > (s32) sar.den)) {
		scale_w = w * sar.num / sar.den;
		sar.num=0;
	}

	if (!ctx->osize.x && !ctx->osize.y) {
		ctx->o_w = w;
		ctx->o_h = h;
		scale_w = w;
	}
	else if (!ctx->osize.x) {
		ctx->o_h = ctx->osize.y;
		ctx->o_w = ctx->osize.y * scale_w / h;
	}
	else if (!ctx->osize.y) {
		ctx->o_w = ctx->osize.x;
		ctx->o_h = ctx->osize.x * h / scale_w;
	} else {
		ctx->o_w = ctx->osize.x;
		ctx->o_h = ctx->osize.y;
	}
	ctx->offset_w = ctx->offset_h = 0;

	u32 final_w = ctx->o_w;
	u32 final_h = ctx->o_h;
	if (ctx->keepar!=EVGS_KEEPAR_OFF) {
		if (ctx->o_w * h < scale_w * ctx->o_h) {
			final_h = ctx->o_w * h / scale_w;
		} else if (ctx->o_w * h > scale_w * ctx->o_h) {
			final_w = ctx->o_h * scale_w / h;
		}
	}
	if (ctx->osar.num > (s32) ctx->osar.den) {
		ctx->o_w = ctx->o_w * ctx->osar.den / ctx->osar.num;
		final_w = final_w * ctx->osar.den / ctx->osar.num;
		if (downsample_w) {
			if (ctx->o_w % 2) ctx->o_w -= 1;
			if (final_w % 2) final_w -= 1;
		}
	}
	ctx->offset_w = (ctx->o_w - final_w) / 2;
	ctx->offset_h = (ctx->o_h - final_h) / 2;

	if (downsample_w && (ctx->offset_w % 2)) ctx->offset_w -= 1;
	if (downsample_h && (ctx->offset_h % 2)) ctx->offset_h -= 1;

	if ((ctx->i_w == w) && (ctx->i_h == h) && (ctx->i_pfmt == ofmt)
		&& (ctx->i_stride == stride)
		&& (ctx->i_stride_uv == stride_uv)
		&& (ctx->fullrange==fullrange)
	) {
		//nothing to reconfigure
	}
	//passthrough mode
	else if ((ctx->o_w == w) && (ctx->o_h == h) && (ofmt==ctx->ofmt) && (ctx->ofr == fullrange) ) {
		ctx->o_stride = 0;
		ctx->o_stride_uv = 0;
		gf_pixel_get_size_info(ctx->ofmt, ctx->o_w, ctx->o_h, &ctx->o_size, &ctx->o_stride, &ctx->o_stride_uv, NULL, NULL);
		ctx->passthrough = GF_TRUE;
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[EVGS] Running in passthrough mode\n"));
	} else {
		Bool res;
		ctx->i_stride = stride;
		ctx->i_stride_uv = stride_uv;

		res = gf_evg_texture_format_ok(ofmt);
		if (res)
			res = gf_pixel_get_size_info(ofmt, w, h, &ctx->i_size, &ctx->i_stride, &ctx->i_stride_uv, &ctx->i_planes, NULL);
		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[EVGS] Source pixel format %s not supported\n", gf_pixel_fmt_name(ofmt) ));
			return GF_NOT_SUPPORTED;
		}
		ctx->i_has_alpha = gf_pixel_fmt_is_transparent(ofmt);

		ctx->o_stride = 0;
		ctx->o_stride_uv = 0;
		res = gf_evg_surface_format_ok(ctx->ofmt);
		if (res)
			res = gf_pixel_get_size_info(ctx->ofmt, ctx->o_w, ctx->o_h, &ctx->o_size, &ctx->o_stride, NULL, NULL, NULL);

		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[EVGS] Output pixel format %s not supported\n", gf_pixel_fmt_name(ctx->ofmt)));
			return GF_NOT_SUPPORTED;
		}

		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[EVGS] Converting from %ux%u@%s to %ux%u@%s\n",
			w, h, gf_pixel_fmt_name(ofmt),
			ctx->o_w, ctx->o_h, gf_pixel_fmt_name(ctx->ofmt)
		));


		gf_path_reset(ctx->path);
		gf_path_add_rect_center(ctx->path, 0, 0, INT2FIX(ctx->o_w - 2*ctx->offset_w), INT2FIX(ctx->o_h - 2*ctx->offset_h));

		//set colorspace - TODO

		ctx->i_w = w;
		ctx->i_h = h;
		ctx->i_pfmt = ofmt;
		ctx->fullrange = fullrange;
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[EVGS] Setup rescaler from %dx%d fmt %s to %dx%d fmt %s\n", w, h, gf_pixel_fmt_name(ofmt), ctx->o_w, ctx->o_h, gf_pixel_fmt_name(ctx->ofmt)));
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->o_w));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->o_h));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->o_stride));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE_UV, ctx->o_stride_uv ? &PROP_UINT(ctx->o_stride_uv) : NULL );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->ofmt));

	if (ctx->osar.num >= (s32) ctx->osar.den) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, &PROP_FRAC(ctx->osar) );
	} else if (sar.num) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, &PROP_FRAC(sar) );
	} else {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, NULL );
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CLAP_X, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CLAP_Y, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CLAP_W, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CLAP_H, NULL);

	return GF_OK;
}

static GF_Err evgs_initialize(GF_Filter *filter)
{
	EVGScaleCtx *ctx = gf_filter_get_udta(filter);

	ctx->surf = gf_evg_surface_new(GF_TRUE);
	if (!ctx->surf) return GF_OUT_OF_MEM;

	ctx->tx = gf_evg_stencil_new(GF_STENCIL_TEXTURE);
	if (!ctx->tx) return GF_OUT_OF_MEM;
	ctx->path = gf_path_new();
	if (!ctx->path) return GF_OUT_OF_MEM;

	if (ctx->osar.num &&
	 ((ctx->osar.num<0) || (ctx->osar.num < (s32) ctx->osar.den))
	) {
		ctx->osar.num = 0;
		ctx->osar.den = 1;
	}
	if (ctx->hq)
		gf_evg_stencil_set_filter(ctx->tx, GF_TEXTURE_FILTER_HIGH_QUALITY );

	return GF_OK;
}
static void evgs_finalize(GF_Filter *filter)
{
	EVGScaleCtx *ctx = gf_filter_get_udta(filter);
	gf_evg_surface_delete(ctx->surf);
	gf_evg_stencil_delete(ctx->tx);
	gf_path_del(ctx->path);
	return;
}


static GF_Err evgs_reconfigure_output(GF_Filter *filter, GF_FilterPid *pid)
{
	const GF_PropertyValue *p;
	EVGScaleCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->opid != pid) return GF_BAD_PARAM;

	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_WIDTH);
	if (p) ctx->osize.x = p->value.uint;

	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_HEIGHT);
	if (p) ctx->osize.y = p->value.uint;

	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_PIXFMT);
	if (p) ctx->ofmt = p->value.uint;
	return evgs_configure_pid(filter, ctx->ipid, GF_FALSE);
}


#define OFFS(_n)	#_n, offsetof(EVGScaleCtx, _n)
static GF_FilterArgs EVGSArgs[] =
{
	{ OFFS(osize), "osize of output video", GF_PROP_VEC2I, NULL, NULL, 0},
	{ OFFS(ofmt), "pixel format for output video. When not set, input format is used", GF_PROP_PIXFMT, "none", NULL, 0},
	{ OFFS(ofr), "force output full range", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(keepar), "keep aspect ratio\n"
	"- off: ignore aspect ratio\n"
	"- full: respect aspect ratio, applying input sample aspect ratio info\n"
	"- nosrc: respect aspect ratio but ignore input sample aspect ratio"
	, GF_PROP_UINT, "off", "off|full|nosrc", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(padclr), "clear color when aspect ration preservation is used", GF_PROP_STRING, "black", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(osar), "force output pixel aspect ratio", GF_PROP_FRACTION, "0/1", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(nbth), "number of threads to use, -1 means all cores", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(hq), "use bilinear interpolation instead of closest pixel", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},

	{0}
};

static const GF_FilterCapability EVGSCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW)
};


GF_FilterRegister EVGSRegister = {
	.name = "evgs",
	GF_FS_SET_DESCRIPTION("EVG video rescaler")
	GF_FS_SET_HELP("This filter rescales raw video data using GPAC's EVG library to the specified size and pixel format.\n"
	"## Output size assignment\n"
	"If [-osize]() is {0,0}, the output dimensions will be set to the input size, and input aspect ratio will be ignored.\n"
	"\n"
	"If [-osize]() is {0,H} (resp. {W,0}), the output width (resp. height) will be set to respect input aspect ratio. If [-keepar=nosrc](), input sample aspect ratio is ignored.\n"
	"## Aspect Ratio and Sample Aspect Ratio\n"
	"When output sample aspect ratio is set, the output dimensions are divided by the output sample aspect ratio.\n"
	"EX evgs:osize=288x240:osar=3/2\n"
	"The output dimensions will be 192x240.\n"
	"\n"
	"When aspect ratio is not kept ([-keepar=off]()):\n"
	"- source is resampled to desired dimensions\n"
	"- if output aspect ratio is not set, output will use source sample aspect ratio\n"
	"\n"
	"When aspect ratio is partially kept ([-keepar=nosrc]()):\n"
	"- resampling is done on the input data without taking input sample aspect ratio into account\n"
	"- if output sample aspect ratio is not set ([-osar=0/N]()), source aspect ratio is forwarded to output.\n"
	"\n"
	"When aspect ratio is fully kept ([-keepar=full]()), output aspect ratio is force to 1/1 if not set.\n"
	"\n"
	"When sample aspect ratio is kept, the filter will:\n"
	"- center the rescaled input frame on the output frame\n"
	"- fill extra pixels with [-padclr]()\n"
	)
	.private_size = sizeof(EVGScaleCtx),
	.args = EVGSArgs,
	.configure_pid = evgs_configure_pid,
	SETCAPS(EVGSCaps),
	.flags = GF_FS_REG_ALLOW_CYCLIC,
	.initialize = evgs_initialize,
	.finalize = evgs_finalize,
	.process = evgs_process,
	.reconfigure_output = evgs_reconfigure_output,
	//use low priority in case we have other scalers (ffsws is faster)
	.priority = 128,
};

#endif //GPAC_DISABLE_EVG

const GF_FilterRegister *evgs_register(GF_FilterSession *session)
{
#ifndef GPAC_DISABLE_EVG
	return &EVGSRegister;
#else
	return NULL;
#endif
}

