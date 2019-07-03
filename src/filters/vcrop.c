/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / video cropping filter
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
#include <gpac/list.h>
#include <gpac/constants.h>
#include <gpac/network.h>

typedef struct
{
	//options
	const char *wnd;
	Bool copy;
	u32 round;

	//internal data
	Bool initialized;

	GF_FilterPid *ipid, *opid;
	u32 w, h, stride, s_pfmt;
	GF_Fraction ar;
	Bool passthrough;

	u32 dst_stride[5];
	u32 src_stride[5];
	u32 nb_planes, nb_src_planes, out_size, out_src_size, src_uv_height, dst_uv_height;

	Bool use_reference;
	u32 dst_width, dst_height;
	s32 src_x, src_y;
	Bool packed_422;

	GF_List *frames, *frames_res;
} GF_VCropCtx;

typedef struct
{
	GF_FilterFrameInterface frame_ifce;

	//reference to the source packet
	GF_FilterPacket *pck;
	u8 *planes[5];
	u32 stride[5];

	GF_VCropCtx *ctx;
} GF_VCropFrame;


GF_Err vcrop_frame_get_plane(GF_FilterFrameInterface *frame, u32 plane_idx, const u8 **outPlane, u32 *outStride)
{
	GF_VCropFrame *vframe = frame->user_data;

	if (plane_idx>=vframe->ctx->nb_planes) return GF_BAD_PARAM;
	if (outPlane) *outPlane = vframe->planes[plane_idx];
	if (outStride) *outStride = vframe->stride[plane_idx];
	return GF_OK;
}

void vcrop_packet_destruct(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_VCropFrame *vframe;
	GF_FilterFrameInterface *frame_ifce = gf_filter_pck_get_frame_interface(pck);
	if (!frame_ifce) return;
	vframe = frame_ifce->user_data;
	assert(vframe->pck);
	gf_filter_pck_unref(vframe->pck);
	vframe->pck = NULL;
	gf_list_add(vframe->ctx->frames_res, vframe);
}


static GF_Err vcrop_process(GF_Filter *filter)
{
	const u8 *data;
	u8 *output;
	u32 size;
	u32 bps;
	u32 s_off_x, s_off_y, d_off_x, d_off_y, i, copy_w, copy_h;
	u8 *src, *dst;
	u8 *src_planes[5];
	u8 *dst_planes[5];
	Bool do_memset = GF_FALSE;
	GF_FilterPacket *dst_pck;
	GF_FilterFrameInterface *frame_ifce;
	GF_VCropCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->ipid);

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
	data = gf_filter_pck_get_data(pck, &size);
	frame_ifce = gf_filter_pck_get_frame_interface(pck);

	memset(src_planes, 0, sizeof(src_planes));
	memset(dst_planes, 0, sizeof(dst_planes));

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
		for (i=0; i<4; i++) {
		if (frame_ifce->get_plane(frame_ifce, i, (const u8 **) &src_planes[i], &ctx->src_stride[i])!=GF_OK)
			break;
		}
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[VCrop] No data associated with packet, not supported\n"));
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NOT_SUPPORTED;
	}

	bps = gf_pixel_get_bytes_per_pixel(ctx->s_pfmt);

	copy_w = ctx->dst_width;
	copy_h = ctx->dst_height;
	s_off_x = s_off_y = d_off_x = d_off_y = 0;
	if (ctx->src_x>=0) {
		s_off_x = ctx->src_x;
	} else {
		d_off_x = (u32) (-ctx->src_x);
		copy_w = (s32) ctx->dst_width + ctx->src_x;
		do_memset = GF_TRUE;
	}
	if (s_off_x + copy_w > ctx->w) {
		copy_w = ctx->w - s_off_x;
		do_memset = GF_TRUE;
	}
	if (ctx->src_y>=0) {
		s_off_y = ctx->src_y;
	} else {
		d_off_y = (u32) (-ctx->src_y);
		copy_h = (s32) ctx->dst_height + ctx->src_y;
		do_memset = GF_TRUE;
	}
	if (s_off_y + copy_h > ctx->h) {
		copy_h = ctx->h - s_off_y;
		do_memset = GF_TRUE;
	}

	if (ctx->use_reference) {
		GF_VCropFrame *vframe = gf_list_pop_back(ctx->frames_res);
		if (!vframe) {
			GF_SAFEALLOC(vframe, GF_VCropFrame);
		}
		vframe->ctx = ctx;
		memcpy(vframe->stride, ctx->src_stride, sizeof(vframe->stride));
		if (ctx->packed_422) {
			vframe->planes[0] = src_planes[0] + s_off_x * bps * 2 + ctx->src_stride[0] * s_off_y;
		} else {
			vframe->planes[0] = src_planes[0] + s_off_x * bps + ctx->src_stride[0] * s_off_y;
		}
		//nv12/21
		if (ctx->nb_planes==2) {
			vframe->planes[1] = src_planes[1] + s_off_x * bps + ctx->src_stride[1] * s_off_y/2;
		} else if (ctx->nb_planes>=3) {
			u32 div_x, div_y;
			//alpha/depth/other plane, treat as luma plane
			if (ctx->nb_planes==4) {
				vframe->planes[3] = src_planes[3] + s_off_x * bps + ctx->src_stride[3] * s_off_y;
			}
			div_x = (ctx->src_stride[1]==ctx->src_stride[0]) ? 1 : 2;
			div_y = (ctx->src_uv_height==ctx->h) ? 1 : 2;

			vframe->planes[1] = src_planes[1] + s_off_x * bps / div_x + ctx->src_stride[1] * s_off_y / div_y;
			vframe->planes[2] =src_planes[2] + s_off_x * bps / div_x + ctx->src_stride[2] * s_off_y / div_y;
		}
		vframe->frame_ifce.user_data = vframe;
		vframe->frame_ifce.get_plane = vcrop_frame_get_plane;
		dst_pck = gf_filter_pck_new_frame_interface(ctx->opid, &vframe->frame_ifce, vcrop_packet_destruct);
		//keep a ref to input packet
		vframe->pck = pck;
		gf_filter_pck_ref(&vframe->pck);
		gf_filter_pck_merge_properties(pck, dst_pck);
		gf_filter_pck_send(dst_pck);
		//remove input packet
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}


	dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->out_size, &output);
	if (!dst_pck) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OUT_OF_MEM;
	}
	gf_filter_pck_merge_properties(pck, dst_pck);
	dst_planes[0] = output;
	if (ctx->nb_planes==1) {
	} else if (ctx->nb_planes==2) {
		dst_planes[1] = output + ctx->dst_stride[0] * ctx->dst_height;
	} else if (ctx->nb_planes==3) {
		dst_planes[1] = output + ctx->dst_stride[0] * ctx->dst_height;
		dst_planes[2] = dst_planes[1] + ctx->dst_stride[1]*ctx->dst_uv_height;
	} else if (ctx->nb_planes==4) {
		dst_planes[1] = output + ctx->dst_stride[0] * ctx->dst_height;
		dst_planes[2] = dst_planes[1] + ctx->dst_stride[1]*ctx->dst_uv_height;
		dst_planes[3] = dst_planes[2] + ctx->dst_stride[2]*ctx->dst_uv_height;
	}

	if (do_memset) {
		memset(output, 0x00, sizeof(char)*ctx->out_size);
	}

	//YUYV variations need *2 on horizontal dimension
	if (ctx->packed_422) {
		src = src_planes[0] + s_off_x * bps * 2 + ctx->src_stride[0] * s_off_y;
		dst = dst_planes[0] + d_off_x * bps * 2 + ctx->dst_stride[0] * d_off_y;
		for (i=0; i<copy_h; i++) {
			memcpy(dst, src, bps * copy_w * 2);
			src += ctx->src_stride[0];
			dst += ctx->dst_stride[0];
		}
	} else {
		//copy first plane
		src = src_planes[0] + s_off_x * bps + ctx->src_stride[0] * s_off_y;
		dst = dst_planes[0] + d_off_x * bps + ctx->dst_stride[0] * d_off_y;
		for (i=0; i<copy_h; i++) {
			memcpy(dst, src, bps * copy_w);
			src += ctx->src_stride[0];
			dst += ctx->dst_stride[0];
		}
	}

	//nv12/21
	if (ctx->nb_planes==2) {
		src = src_planes[1] + s_off_x * bps + ctx->src_stride[1] * s_off_y/2;
		dst = dst_planes[1] + d_off_x * bps + ctx->dst_stride[1] * d_off_y/2;
		//half vertical res (/2)
		for (i=0; i<copy_h/2; i++) {
			//half horizontal res (/2) but two chroma packed per pixel (*2)
			memcpy(dst, src, bps * copy_w);
			src += ctx->src_stride[1];
			dst += ctx->dst_stride[1];
		}
	} else if (ctx->nb_planes>=3) {
		u32 div_x, div_y;
		//alpha/depth/other plane, treat as luma plane
		if (ctx->nb_planes==4) {
			src = src_planes[3] + s_off_x * bps + ctx->src_stride[3] * s_off_y;
			dst = dst_planes[3] + d_off_x * bps + ctx->dst_stride[3] * d_off_y;
			for (i=0; i<copy_h; i++) {
				memcpy(dst, src, bps * copy_w);
				src += ctx->src_stride[3];
				dst += ctx->dst_stride[3];
			}
		}

		div_x = (ctx->src_stride[1]==ctx->src_stride[0]) ? 1 : 2;
		div_y = (ctx->src_uv_height==ctx->h) ? 1 : 2;

		src = src_planes[1] + s_off_x * bps / div_x + ctx->src_stride[1] * s_off_y / div_y;
		dst = dst_planes[1] + d_off_x * bps / div_x + ctx->dst_stride[1] * d_off_y / div_y;
		copy_h /= div_y;
		copy_w /= div_x;

		for (i=0; i<copy_h; i++) {
			memcpy(dst, src, bps * copy_w);
			src += ctx->src_stride[1];
			dst += ctx->dst_stride[1];
		}

		src = src_planes[2] + s_off_x * bps / div_x + ctx->src_stride[2] * s_off_y / div_y;
		dst = dst_planes[2] + d_off_x * bps / div_x + ctx->dst_stride[2] * d_off_y / div_y;
		for (i=0; i<copy_h; i++) {
			memcpy(dst, src, bps * copy_w);
			src += ctx->src_stride[1];
			dst += ctx->dst_stride[1];
		}
	}

	gf_filter_pck_send(dst_pck);
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static Bool parse_crop_props(GF_VCropCtx *ctx, u32 src_w, u32 src_h, GF_PixelFormat pfmt)
{
	s32 dim[4];
	s32 dim_off[4];
	Bool dim_pc[4];
	u32 nb_dim=0;
	char *args = (char *)ctx->wnd;

	memset(dim, 0, sizeof(dim));
	memset(dim_pc, 0, sizeof(dim_pc));
	memset(dim_off, 0, sizeof(dim_off));

	while (args) {
		char *sep_offset=NULL, *sep_pc, sign;
		char *sep = strchr(args, 'x');
		if (sep) sep[0] = 0;

		sep_pc = strchr(args, '%');
		if (sep_pc) {
			sep_pc[0] = 0;
			dim[nb_dim] = atoi(args);
			sep_pc[0] = '%';
			dim_pc[nb_dim] = GF_TRUE;

			sep_offset = strchr(sep_pc, '+');
			if (sep_offset) {
				sign = sep_offset[0];
				dim_off[nb_dim] = atoi(sep_offset+1);
				sep_offset[0] = 0;
			} else {
				sep_offset = strchr(args, '-');
				if (sep_offset) {
					sign = sep_offset[0];
					dim_off[nb_dim] = - atoi(sep_offset+1);
					sep_offset[0] = 0;
				}
			}

		} else {
			dim[nb_dim] = atoi(args);
		}

		if (sep_offset) sep_offset[0] = sign;
		nb_dim++;
		if (!sep) break;
		sep[0] = 'x';
		args = sep+1;
		if (nb_dim==4) break;
	}

#define GET_DIM(_dst, _i, _s) if (dim_pc[_i]) \
								_dst = (dim[_i] * (s32) _s / 100) + dim_off[_i]; \
							else \
								_dst = dim[_i] + dim_off[_i]; \


	if (nb_dim==2) {
		ctx->src_x = 0;
		ctx->src_y = 0;
		GET_DIM(ctx->dst_width, 0, src_w)
		GET_DIM(ctx->dst_height, 1, src_h)
	} else if (nb_dim==4) {
		GET_DIM(ctx->src_x, 0, src_w)
		GET_DIM(ctx->src_y, 1, src_h)
		GET_DIM(ctx->dst_width, 2, src_w)
		GET_DIM(ctx->dst_height, 3, src_h)
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[VCrop] Cannot parse crop parameters, expected 2 or 4 numbers, got %d\n", nb_dim));
		return GF_FALSE;
	}
	if ((s32) ctx->dst_width < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[VCrop] Negative destination width %d !\n", (s32) ctx->dst_width));
		return GF_FALSE;
	}
	if ((s32) ctx->dst_height < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[VCrop] Negative destination height %d !\n", (s32) ctx->dst_height));
		return GF_FALSE;
	}

#define ROUND_IT(_a) { if ((ctx->round==0) || (ctx->round==2)) { (_a)++; } else { (_a)--; } }

	//for YUV 420, adjust to multiple of 2 on both dim
	ctx->packed_422 = GF_FALSE;
	switch (pfmt) {
	case GF_PIXEL_YUV:
	case GF_PIXEL_NV21:
	case GF_PIXEL_NV21_10:
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_YUVA:
	case GF_PIXEL_YUVD:
	case GF_PIXEL_YUV_10:
		if (ctx->src_x % 2) ROUND_IT(ctx->src_x);
		if (ctx->src_y % 2) ROUND_IT(ctx->src_y);
		if ((ctx->src_x + ctx->dst_width) % 2) ROUND_IT(ctx->dst_width);
		if ((ctx->src_y + ctx->dst_height) % 2) ROUND_IT(ctx->dst_height);
		break;
	//for YUV 422, adjust to multiple of 2 on horizontal dim
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
		ctx->packed_422 = GF_TRUE;
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV422_10:
		if (ctx->src_x % 2) ROUND_IT(ctx->src_x);
		if ((ctx->src_x + ctx->dst_width) % 2) ROUND_IT(ctx->dst_width);
		if (ctx->round>1) {
			if (ctx->src_y % 2) ROUND_IT(ctx->src_y);
			if ((ctx->src_y + ctx->dst_height) % 2) ROUND_IT(ctx->dst_height);
		}
		break;
	default:
		if (ctx->round>1) {
			if (ctx->src_x % 2) ROUND_IT(ctx->src_x);
			if (ctx->src_y % 2) ROUND_IT(ctx->src_y);
			if ((ctx->src_x + ctx->dst_width) % 2) ROUND_IT(ctx->dst_width);
			if ((ctx->src_y + ctx->dst_height) % 2) ROUND_IT(ctx->dst_height);
		}
		break;
	}
	return GF_TRUE;
}

static GF_Err vcrop_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_PropVec2i vec;
	const GF_PropertyValue *p;
	u32 w, h, stride, pfmt;
	GF_Fraction sar;
	GF_VCropCtx *ctx = gf_filter_get_udta(filter);

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
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, pid);

	if (!ctx->ipid) {
		ctx->ipid = pid;
	}
	w = h = pfmt = stride = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (p) w = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (p) h = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE);
	if (p) stride = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
	if (p) pfmt = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAR);
	if (p) sar = p->value.frac;
	else sar.den = sar.num = 1;

	if (!w || !h || !pfmt) {
		ctx->passthrough = GF_TRUE;
		return GF_OK;
	}
	ctx->passthrough = GF_FALSE;

	if ((ctx->w == w) && (ctx->h == h) && (ctx->s_pfmt == pfmt) && (ctx->stride == stride)) {
		//nothing to reconfigure
	} else {
		Bool res;
		if (! parse_crop_props(ctx, w, h, pfmt)) {
			return GF_BAD_PARAM;
		}

		if (ctx->src_x<0) ctx->use_reference = GF_FALSE;
		else if (ctx->src_y<0) ctx->use_reference = GF_FALSE;
		else if (ctx->src_x + ctx->dst_width > w) ctx->use_reference = GF_FALSE;
		else if (ctx->src_y + ctx->dst_height > h) ctx->use_reference = GF_FALSE;
		else ctx->use_reference = ctx->copy ? GF_FALSE : GF_TRUE;

		if (ctx->use_reference) {
			if (!ctx->frames) ctx->frames = gf_list_new();
			if (!ctx->frames_res) ctx->frames_res = gf_list_new();
		}

		//get layout info for source
		memset(ctx->src_stride, 0, sizeof(ctx->src_stride));
		if (ctx->stride) ctx->src_stride[0] = ctx->stride;

		res = gf_pixel_get_size_info(pfmt, w, h, &ctx->out_src_size, &ctx->src_stride[0], &ctx->src_stride[1], &ctx->nb_src_planes, &ctx->src_uv_height);
		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[VCrop] Failed to query source pixel format characteristics\n"));
			return GF_NOT_SUPPORTED;
		}
		if (ctx->nb_src_planes==3) ctx->src_stride[2] = ctx->src_stride[1];
		if (ctx->nb_src_planes==4) ctx->src_stride[3] = ctx->src_stride[0];


		//get layout info for dest
		memset(ctx->dst_stride, 0, sizeof(ctx->dst_stride));
		res = gf_pixel_get_size_info(pfmt, ctx->dst_width, ctx->dst_height, &ctx->out_size, &ctx->dst_stride[0], &ctx->dst_stride[1], &ctx->nb_planes, &ctx->dst_uv_height);
		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[VCrop] Failed to query output pixel format characteristics\n"));
			return GF_NOT_SUPPORTED;
		}
		if (ctx->nb_planes==3) ctx->dst_stride[2] = ctx->dst_stride[1];
		if (ctx->nb_planes==4) ctx->dst_stride[3] = ctx->dst_stride[0];


		ctx->w = w;
		ctx->h = h;
		ctx->s_pfmt = pfmt;

		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[VCrop] Configured output window to crop %dx%dx%dx%d from full frame size %dx%d\n", ctx->src_x, ctx->src_y, ctx->dst_width, ctx->dst_height, ctx->w, ctx->h));

		if (!ctx->src_x && !ctx->src_y && (ctx->dst_width==ctx->w) &&  (ctx->dst_height==ctx->h) ) {
			ctx->passthrough = GF_TRUE;
		}
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->dst_width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->dst_height));
	if (ctx->use_reference) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->src_stride[0] ));
		if (ctx->nb_planes>1)
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE_UV, &PROP_UINT(ctx->src_stride[1]));
	} else {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->dst_stride[0] ));
		if (ctx->nb_planes>1)
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE_UV, &PROP_UINT(ctx->dst_stride[1]));
	}
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, &PROP_FRAC(sar) );
	vec.x = ctx->src_x;
	vec.y = ctx->src_y;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CROP_POS, &PROP_VEC2I(vec) );
	vec.x = ctx->w;
	vec.y = ctx->h;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ORIG_SIZE, &PROP_VEC2I(vec) );

	return GF_OK;
}

void vcrop_finalize(GF_Filter *filter)
{
	GF_VCropCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->frames) {
		while (gf_list_count(ctx->frames)) {
			GF_VCropFrame *vframe = gf_list_pop_back(ctx->frames);
			gf_free(vframe);
		}
		gf_list_del(ctx->frames);
	}
	if (ctx->frames_res) {
		while (gf_list_count(ctx->frames_res)) {
			GF_VCropFrame *vframe = gf_list_pop_back(ctx->frames_res);
			gf_free(vframe);
		}
		gf_list_del(ctx->frames_res);
	}
}


#define OFFS(_n)	#_n, offsetof(GF_VCropCtx, _n)
static GF_FilterArgs VCropArgs[] =
{
	{ OFFS(wnd), "size of output to crop, indicated as TxLxWxH. If % is indicated after a number, the value is in percent of the source width (for L and W) or height (for T and H). An absolute offset (+x, -x) can be added after percent", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(copy), "copy the source pixels. By default the filter will try to forward crop frames by adjusting offsets and strides of the source if possible (window contained in frame)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(round), "adjust dimension to be a multiple of 2.\n"
	"- up: up rounding\n"
	"- down: down rounding\n"
	"- allup: up rounding on formats that do not require it (RGB, YUV444)\n"
	"- alldown: down rounding on formats that do not require it (RGB, YUV444)", GF_PROP_UINT, "up", "up|down|allup|alldown", GF_FS_ARG_HINT_ADVANCED},
	{0}
};

static const GF_FilterCapability VCropCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister VCropRegister = {
	.name = "vcrop",
	GF_FS_SET_DESCRIPTION("Video crop")
	GF_FS_SET_HELP("This filter is used to crop raw video data.")
	.private_size = sizeof(GF_VCropCtx),
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	.args = VCropArgs,
	.configure_pid = vcrop_configure_pid,
	SETCAPS(VCropCaps),
	.process = vcrop_process,
	.finalize = vcrop_finalize,
};



const GF_FilterRegister *vcrop_register(GF_FilterSession *session)
{
	return &VCropRegister;
}
