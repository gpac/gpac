/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / ffmpeg video rescaler filter
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

#include "ff_common.h"

#include <gpac/setup.h>

#ifdef GPAC_HAS_FFMPEG

typedef struct
{
	//options
	GF_PropVec2i osize;
	u32 ofmt, scale;
	Double p1, p2;

	//internal data
	Bool initialized;

	GF_FilterPid *ipid, *opid;
	u32 w, h, stride, s_pfmt;
	GF_Fraction ar;
	Bool passthrough;

	struct SwsContext *swscaler;

	u32 dst_stride[5];
	u32 src_stride[5];
	u32 nb_planes, nb_src_planes, out_size, out_src_size, src_uv_height, dst_uv_height, ow, oh;

	u32 swap_idx_1, swap_idx_2;

} GF_FFSWScaleCtx;

static GF_Err ffsws_process(GF_Filter *filter)
{
	const char *data;
	u8 *output;
	u32 osize;
	s32 res;
	u8 *src_planes[5];
	u8 *dst_planes[5];
	GF_FilterPacket *dst_pck;
	GF_FilterFrameInterface *frame_ifce;
	GF_FFSWScaleCtx *ctx = gf_filter_get_udta(filter);
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
	if (!ctx->ofmt && !ctx->ow && !ctx->oh)
		return GF_OK;

	if (!ctx->swscaler) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NOT_SUPPORTED;
	}

	data = gf_filter_pck_get_data(pck, &osize);
	frame_ifce = gf_filter_pck_get_frame_interface(pck);
	//we may have biffer input (padding) but shall not have smaller
	if (osize && (ctx->out_src_size > osize) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFSWS] Mismatched in source osize, expected %d got %d - stride issue ?\n", ctx->out_src_size, osize));
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NOT_SUPPORTED;
	}

	memset(src_planes, 0, sizeof(src_planes));
	memset(dst_planes, 0, sizeof(dst_planes));
	dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->out_size, &output);
	if (!dst_pck) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OUT_OF_MEM;
	}
	gf_filter_pck_merge_properties(pck, dst_pck);

	if (data) {
		src_planes[0] = (u8 *) data;

		if (ctx->nb_src_planes==1) {
		} else if (ctx->nb_src_planes==2) {
			src_planes[1] = src_planes[0] + ctx->src_stride[0]*ctx->h;
		} else if (ctx->nb_src_planes==3) {
			src_planes[1] = src_planes[0] + ctx->src_stride[0] * ctx->h;
			src_planes[2] = src_planes[1] + ctx->src_stride[1] * ctx->src_uv_height;
		} else if (ctx->nb_src_planes==4) {
			src_planes[1] = src_planes[0] + ctx->src_stride[0] * ctx->h;
			src_planes[2] = src_planes[1] + ctx->src_stride[1] * ctx->src_uv_height;
			src_planes[3] = src_planes[2] + ctx->src_stride[2] * ctx->src_uv_height;
		}
	} else if (frame_ifce && frame_ifce->get_plane) {
		u32 i=0;
		for (i=0; i<ctx->nb_src_planes; i++) {
			if (frame_ifce->get_plane(frame_ifce, i, (const u8 **) &src_planes[i], &ctx->src_stride[i])!=GF_OK)
				break;
		}
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFSWS] No data associated with packet, not supported\n"));
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NOT_SUPPORTED;
	}
	dst_planes[0] = output;
	if (ctx->nb_planes==1) {
	} else if (ctx->nb_planes==2) {
		dst_planes[1] = output + ctx->dst_stride[0] * ctx->oh;
	} else if (ctx->nb_planes==3) {
		dst_planes[1] = output + ctx->dst_stride[0] * ctx->oh;
		dst_planes[2] = dst_planes[1] + ctx->dst_stride[1]*ctx->dst_uv_height;
	} else if (ctx->nb_planes==4) {
		dst_planes[1] = output + ctx->dst_stride[0] * ctx->oh;
		dst_planes[2] = dst_planes[1] + ctx->dst_stride[1]*ctx->dst_uv_height;
		dst_planes[3] = dst_planes[2] + ctx->dst_stride[2]*ctx->dst_uv_height;
	}

	//rescale the cropped frame
	res = sws_scale(ctx->swscaler, (const u8**) src_planes, ctx->src_stride, 0, ctx->h, dst_planes, ctx->dst_stride);
	if (res != ctx->oh) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFSWS] Error during scale, expected height %d got %d\n", ctx->oh, res));
		gf_filter_pid_drop_packet(ctx->ipid);
		gf_filter_pck_discard(dst_pck);
		return GF_NOT_SUPPORTED;
	}

	if (ctx->swap_idx_1 || ctx->swap_idx_2) {
		u32 i, j;
		for (i=0; i<ctx->h; i++) {
			u8 *dst = output + ctx->dst_stride[0]*i;
			for (j=0; j<ctx->dst_stride[0]; j+=4) {
				u8 tmp = dst[ctx->swap_idx_1];
				dst[ctx->swap_idx_1] = dst[ctx->swap_idx_2];
				dst[ctx->swap_idx_2] = tmp;
				dst += 4;
			}
		}
	}

	gf_filter_pck_send(dst_pck);
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static u32 get_sws_mode(u32 mode, u32 *has_param)
{
	switch (mode) {
	case 0: return SWS_FAST_BILINEAR;
	case 1: return SWS_BILINEAR;
	case 2: *has_param = 2; return SWS_BICUBIC;
	case 3: return SWS_X;
	case 4: return SWS_POINT;
	case 5: return SWS_AREA;
	case 6: return SWS_BICUBLIN;
	case 7: *has_param = 1; return SWS_GAUSS;
	case 8: return SWS_SINC;
	case 9: *has_param = 1; return SWS_LANCZOS;
	case 10: return SWS_SPLINE;
	default: break;
	}
	*has_param = 2;
	return SWS_BICUBIC;
}

static GF_Err ffsws_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	u32 w, h, stride, ofmt;
	GF_Fraction sar;
	Double par[2], *par_p=NULL;
	GF_FFSWScaleCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
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

	//if nothing is set we, consider we run as an adaptation filter, wait for caps to be set to declare output
	if (!ctx->ofmt && !ctx->osize.x && !ctx->osize.y)
		return GF_OK;



	w = h = ofmt = stride = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (p) w = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (p) h = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE);
	if (p) stride = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
	if (p) ofmt = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAR);
	if (p) sar = p->value.frac;
	else sar.den = sar.num = 1;

	//ctx->ofmt may be 0 if the filter is instantiated dynamically, we haven't yet been called for reconfigure
	if (!w || !h || !ofmt) {
		return GF_OK;
	}
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);

	if (!ctx->ofmt)
		ctx->ofmt = ofmt;

	ctx->passthrough = GF_FALSE;

	ctx->ow = ctx->osize.x ? ctx->osize.x : w;
	ctx->oh = ctx->osize.y ? ctx->osize.y : h;
	if ((ctx->w == w) && (ctx->h == h) && (ctx->s_pfmt == ofmt) && (ctx->stride == stride)) {
		//nothing to reconfigure
	}
	//passthrough mode
	else if ((ctx->ow == w) && (ctx->oh == h) && (ctx->s_pfmt == ofmt) && (ofmt==ctx->ofmt) ) {
		memset(ctx->dst_stride, 0, sizeof(ctx->dst_stride));
		gf_pixel_get_size_info(ctx->ofmt, ctx->ow, ctx->oh, &ctx->out_size, &ctx->dst_stride[0], &ctx->dst_stride[1], &ctx->nb_planes, &ctx->dst_uv_height);
		ctx->passthrough = GF_TRUE;
	} else {
		u32 nb_par = 0;
		nb_par = 0;
		Bool res;
		u32 mode = get_sws_mode(ctx->scale, &nb_par);
		u32 ff_src_pfmt = ffmpeg_pixfmt_from_gpac(ofmt);
		u32 ff_dst_pfmt = ffmpeg_pixfmt_from_gpac(ctx->ofmt);

		//get layout info for source
		memset(ctx->src_stride, 0, sizeof(ctx->src_stride));
		if (ctx->stride) ctx->src_stride[0] = ctx->stride;


		res = gf_pixel_get_size_info(ofmt, w, h, &ctx->out_src_size, &ctx->src_stride[0], &ctx->src_stride[1], &ctx->nb_src_planes, &ctx->src_uv_height);
		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFSWS] Failed to query source pixel format characteristics\n"));
			return GF_NOT_SUPPORTED;
		}
		if (ctx->nb_src_planes==3) ctx->src_stride[2] = ctx->src_stride[1];
		if (ctx->nb_src_planes==4) ctx->src_stride[3] = ctx->src_stride[0];

		//get layout info for dest
		memset(ctx->dst_stride, 0, sizeof(ctx->dst_stride));
		res = gf_pixel_get_size_info(ctx->ofmt, ctx->ow, ctx->oh, &ctx->out_size, &ctx->dst_stride[0], &ctx->dst_stride[1], &ctx->nb_planes, &ctx->dst_uv_height);
		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFSWS] Failed to query output pixel format characteristics\n"));
			return GF_NOT_SUPPORTED;
		}
		if (ctx->nb_planes==3) ctx->dst_stride[2] = ctx->dst_stride[1];
		if (ctx->nb_planes==4) ctx->dst_stride[3] = ctx->dst_stride[0];


		if (nb_par) {
			if ((nb_par==1) && (ctx->p1!=GF_MAX_DOUBLE) ) {
				par[0] = ctx->p1;
				par_p = (Double *)par;
			} else if ((nb_par==2) && (ctx->p1!=GF_MAX_DOUBLE) && (ctx->p2!=GF_MAX_DOUBLE)) {
				par[0] = ctx->p1;
				par[1] = ctx->p2;
				par_p = (Double *)par;
			}
		}
		//create/get a swscale context
		ctx->swscaler = sws_getCachedContext(ctx->swscaler, w, h, ff_src_pfmt, ctx->ow, ctx->oh, ff_dst_pfmt, mode, NULL, NULL, par_p);

		if (!ctx->swscaler) {
			Bool in_ok = sws_isSupportedInput(ff_src_pfmt);
			Bool out_ok = sws_isSupportedInput(ff_dst_pfmt);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFSWS] Cannot allocate context for required format - input %s output %s\n", in_ok ? "OK" : "not supported" , out_ok ? "OK" : "not supported"));
			return GF_NOT_SUPPORTED;
		}
		ctx->w = w;
		ctx->h = h;
		ctx->s_pfmt = ofmt;
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[FFSWS] Setup rescaler from %dx%d fmt %s to %dx%d fmt %s\n", w, h, gf_pixel_fmt_name(ofmt), ctx->ow, ctx->oh, gf_pixel_fmt_name(ctx->ofmt)));

		ctx->swap_idx_1 = ctx->swap_idx_2 = 0;
		//if same source / dest pixel format, don'( swap UV components
		if (ctx->s_pfmt != ctx->ofmt) {
			if (ctx->ofmt==GF_PIXEL_VYUY) {
				ctx->swap_idx_1 = 0;
				ctx->swap_idx_2 = 2;
			}
			else if (ctx->ofmt==GF_PIXEL_YVYU) {
				ctx->swap_idx_1 = 1;
				ctx->swap_idx_2 = 3;
			}
		}
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->ow));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->oh));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->dst_stride[0]));
	if (ctx->nb_planes>1)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE_UV, &PROP_UINT(ctx->dst_stride[1]));

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->ofmt));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, &PROP_FRAC(sar) );
	return GF_OK;
}

static void ffsws_finalize(GF_Filter *filter)
{
	GF_FFSWScaleCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->swscaler) sws_freeContext(ctx->swscaler);
	return;
}


static GF_Err ffsws_reconfigure_output(GF_Filter *filter, GF_FilterPid *pid)
{
	const GF_PropertyValue *p;
	GF_FFSWScaleCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->opid != pid) return GF_BAD_PARAM;

	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_WIDTH);
	if (p) ctx->osize.x = p->value.uint;

	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_HEIGHT);
	if (p) ctx->osize.y = p->value.uint;

	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_PIXFMT);
	if (p) ctx->ofmt = p->value.uint;
	return ffsws_configure_pid(filter, ctx->ipid, GF_FALSE);
}


#define OFFS(_n)	#_n, offsetof(GF_FFSWScaleCtx, _n)
static GF_FilterArgs FFSWSArgs[] =
{
	{ OFFS(osize), "osize of output video. When not set, input osize is used", GF_PROP_VEC2I, NULL, NULL, 0},
	{ OFFS(ofmt), "pixel format for output video. When not set, input format is used", GF_PROP_PIXFMT, "none", NULL, 0},
	{ OFFS(scale), "scaling mode - see filter info", GF_PROP_UINT, "bicubic", "fastbilinear|bilinear|bicubic|X|point|area|bicublin|gauss|sinc|lanzcos|spline", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(p1), "scaling algo param1 - see filter info", GF_PROP_DOUBLE, "+I", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(p2), "scaling algo param2 - see filter info", GF_PROP_DOUBLE, "+I", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};

static const GF_FilterCapability FFSWSCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW)
};


GF_FilterRegister FFSWSRegister = {
	.name = "ffsws",
	.version=LIBSWSCALE_IDENT,
	GF_FS_SET_DESCRIPTION("FFMPEG video rescaler")
	GF_FS_SET_HELP("For bicubic, to tune the shape of the basis function, [-p1]() tunes f(1) and [-p2]() f´(1)\n"\
				"For gauss [-p1]() tunes the exponent and thus cutoff frequency\n"\
				"For lanczos [-p1]() tunes the width of the window function"\
				"\nSee FFMPEG documentation (https://ffmpeg.org/documentation.html) for more detailed info on rescaler options")

	.private_size = sizeof(GF_FFSWScaleCtx),
	.args = FFSWSArgs,
	.configure_pid = ffsws_configure_pid,
	SETCAPS(FFSWSCaps),
	.finalize = ffsws_finalize,
	.process = ffsws_process,
	.reconfigure_output = ffsws_reconfigure_output,
};

#endif

const GF_FilterRegister *ffsws_register(GF_FilterSession *session)
{
#ifdef GPAC_HAS_FFMPEG
	FFSWSArgs[1].min_max_enum = gf_pixel_fmt_all_names();
	return &FFSWSRegister;
#else
	return NULL;
#endif
}

