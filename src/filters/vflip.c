/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Samir Mustapha - Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019
 *					All rights reserved
 *
 *  This file is part of GPAC / video flip filter
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
	u32 mode;

	//internal data
	Bool initialized;

	GF_FilterPid *ipid, *opid;
	u32 w, h, stride, s_pfmt;
	u32 bps;
	GF_Fraction ar;
	Bool passthrough;
	u32 dst_width, dst_height;

	u32 dst_stride[5];
	u32 src_stride[5];
	u32 nb_planes, nb_src_planes, out_size, out_src_size, src_uv_height, dst_uv_height;

	Bool use_reference;
	Bool packed_422;


	char *line_buffer_vf; //vertical flip
	char *line_buffer_hf; //horizontal flip

} GF_VFlipCtx;

enum
{
	VFLIP_OFF = 0,
	VFLIP_VERT,
	VFLIP_HORIZ,
	VFLIP_BOTH,
};



static void swap_2Ys_YUVpixel(GF_VFlipCtx *ctx, u8 *line_src, u8 *line_dst, u32 FourBytes_start_index)
{
	u32 isFirstY_indexOne;
	switch (ctx->s_pfmt) {
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
		isFirstY_indexOne= (u32) 0;
		break;
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
		isFirstY_indexOne= (u32) 1;
		break;
	default:
		return;
	}

	//Y2_dst = Y1_src
	line_dst[FourBytes_start_index + 2 + isFirstY_indexOne]=line_src[FourBytes_start_index + 0 + isFirstY_indexOne];

	//Y1_dst = Y2_src
	line_dst[FourBytes_start_index + 0 + isFirstY_indexOne]=line_src[FourBytes_start_index + 2 + isFirstY_indexOne];
}

static void horizontal_flip_per_line(GF_VFlipCtx *ctx, u8 *line_src, u8 *line_dst, u32 plane_idx, u32 wiB)
{
	u32 line_size = wiB;

	if( ctx->s_pfmt == GF_PIXEL_RGB || ctx->s_pfmt == GF_PIXEL_BGR || ctx->s_pfmt == GF_PIXEL_XRGB || ctx->s_pfmt == GF_PIXEL_RGBX || ctx->s_pfmt == GF_PIXEL_XBGR || ctx->s_pfmt == GF_PIXEL_BGRX){
		//to avoid "3*j > line_size - 3*j - 3" or "4*j > line_size - 4*j - 4"
		//jmax=line_size/(2*3) or jmax=line_size/(2*4)
		for (u32 j = 0; j < line_size/(2*ctx->bps); j++) {
			u8 pix[4];
			memcpy(pix, line_src + line_size - ctx->bps*(j+1), ctx->bps);
			memcpy(line_dst + line_size - ctx->bps*(j+1), line_src + ctx->bps*j, ctx->bps);
			memcpy(line_dst + ctx->bps*j, pix, ctx->bps);
		}

	}else if (ctx->packed_422) {

		line_size = wiB/2;

		//If the source data is assigned to the output packet during the destination pack allocation
		//i.e dst_planes[0]= src_planes[0], line_src is going to change while reading it as far as writing on line_dst=line_src
		//To avoid this situation, ctx->line_buffer_hf keeps the values of line_src
		memcpy(ctx->line_buffer_hf, line_src, wiB);

		//reversing of 4-bytes sequences
		u32 fourBytesSize = wiB/4;
		for (u32 j = 0; j < fourBytesSize; j++) {
			//buffer = last 4 columns
			u32 last_4bytes_index = wiB-4-(4*j);
			u32 first_4bytes_index = 4*j;
			for (u32 p = 0; p < 4; p++) {
				line_dst[first_4bytes_index+p] = ctx->line_buffer_hf[last_4bytes_index+p];
			}
			//exchanging of Ys within a yuv pixel
			swap_2Ys_YUVpixel(ctx, line_dst, line_dst, first_4bytes_index);
		}
	}else {
		//nv12/21
		//second plane is U-plane={u1,v1, u2,v2...}
		if (ctx->nb_planes==2 && plane_idx==1){
			if (ctx->bps==1) {
				//to avoid "line_size - 2*j - 2 > 2*j", jmax=line_size/4
				for (u32 j = 0; j < line_size/4; j++) {
					u8 u_comp, v_comp;
					u_comp = line_src[line_size - 2*j - 2];
					v_comp = line_src[line_size - 2*j - 1];

					line_dst[line_size - 2*j - 2] = line_src[2*j];
					line_dst[line_size - 2*j - 1] = line_src[2*j + 1];

					line_dst[2*j] = u_comp;
					line_dst[2*j + 1] = v_comp;
				}
			}
			else{
				for (u32 j = 0; j < line_size/4; j++) {
					u16 u_comp, v_comp;
					u_comp = line_src[line_size - 2*j - 2];
					v_comp = line_src[line_size - 2*j - 1];

					((u16 *)line_dst)[line_size - 2*j - 2] = ((u16 *)line_src)[2*j];
					((u16 *)line_dst)[line_size - 2*j - 1] = ((u16 *)line_src)[2*j + 1];

					((u16 *)line_dst)[2*j] = u_comp;
					((u16 *)line_dst)[2*j + 1] = v_comp;
				}
			}
		} else if (ctx->bps==1) {
			u32 wx = line_size/2;
			u8 tmp;
			for (u32 j = 0; j < wx; j++) {
				//tmp = last column
				tmp = line_src[line_size-1-j];

				//last column = first column
				line_dst[line_size-1-j] = line_src[j];

				//first column = tmp
				line_dst[j]=tmp;
			}
		} else {
			line_size /= 2;
			u32 wx = line_size/2;
			u16 tmp;
			for (u32 j = 0; j < wx; j++) {
				//tmp = last column
				tmp = (u16) ( ((u16 *)line_src) [line_size-1-j] );

				//last column = first column
				((u16 *)line_dst) [line_size-1-j] = ((u16*)line_src)[j];

				//first column = tmp
				((u16 *)line_dst) [j]=tmp;
			}
		}
	}
}

static void horizontal_flip(GF_VFlipCtx *ctx, u8 *src_plane, u8 *dst_plane, u32 height, u32 plane_idx, u32 wiB, u32 *src_stride)
{
	for (u32 i=0; i<height; i++) {
		u8 *src_first_line = src_plane + i * src_stride[plane_idx];
		u8 *dst_first_line = dst_plane + i * ctx->dst_stride[plane_idx];

		horizontal_flip_per_line(ctx, src_first_line, dst_first_line, plane_idx, wiB);
	}
}

static void vertical_flip(GF_VFlipCtx *ctx, u8 *src_plane, u8 *dst_plane, u32 height, u32 plane_idx, u32 wiB){
	u32 hy, i;
	hy = height/2;
	for (i=0; i<hy; i++) {
		u8 *src_first_line = src_plane+ i*ctx->src_stride[plane_idx];
		u8 *src_last_line  = src_plane+ (height  - 1 - i) * ctx->src_stride[plane_idx];

		u8 *dst_first_line = dst_plane+ i*ctx->dst_stride[plane_idx];
		u8 *dst_last_line  = dst_plane+ (height  - 1 - i) * ctx->dst_stride[plane_idx];

		memcpy(ctx->line_buffer_vf, src_last_line, wiB);
		memcpy(dst_last_line, src_first_line, wiB);
		memcpy(dst_first_line, ctx->line_buffer_vf, wiB);
	}
}

static GF_Err vflip_process(GF_Filter *filter)
{
	const char *data;
	u8 *output;
	u32 size;
	u32 i;
	u32 wiB, height; //wiB: width in Bytes of a plane
	u8 *src_planes[5];
	u8 *dst_planes[5];
	GF_FilterPacket *dst_pck;
	GF_FilterFrameInterface *frame_ifce;
	GF_VFlipCtx *ctx = gf_filter_get_udta(filter);
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
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[VFlip] No data associated with packet, not supported\n"));
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NOT_SUPPORTED;
	}

	ctx->bps = gf_pixel_get_bytes_per_pixel(ctx->s_pfmt);


	if (frame_ifce){
		dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->out_size, &output);
		gf_filter_pck_merge_properties(pck, dst_pck);
	} else {
		dst_pck = gf_filter_pck_new_clone(ctx->opid, pck, &output);
	}

	if (!dst_pck) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OUT_OF_MEM;
	}


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

	//computing of height and wiB
	//YUYV variations need *2 on horizontal dimension
	for (i=0; i<ctx->nb_planes; i++) {
		if (i==0) {
			if (ctx->packed_422) {
				wiB = ctx->bps * ctx->dst_width *2;
			} else {
				wiB = ctx->bps * ctx->dst_width;
			}
			height = ctx->h;
		}else {
			//nv12/21
			if (i==1 && ctx->nb_planes==2) {
				//half vertical res (/2)
				//half horizontal res (/2) but two chroma packed per pixel (*2)
				wiB = ctx->bps * ctx->dst_width;
				height = ctx->h / 2;
			} else if (ctx->nb_planes>=3) {
				u32 div_x, div_y;
				//alpha/depth/other plane, treat as luma plane
				if (i==3 && ctx->nb_planes==4) {
					wiB = ctx->bps * ctx->dst_width;
					height = ctx->h;
				}else if (i==1 || i==2) {
					div_x = (ctx->src_stride[1]==ctx->src_stride[0]) ? 1 : 2;
					div_y = (ctx->src_uv_height==ctx->h) ? 1 : 2;
					height = ctx->dst_height;
					height /= div_y;
					wiB = ctx->bps * ctx->dst_width;
					wiB /= div_x;
				}
			}
		}

		//processing according selected mode
		if (ctx->mode==VFLIP_VERT){
			vertical_flip(ctx, src_planes[i], dst_planes[i], height, i, wiB);
		}else if (ctx->mode==VFLIP_HORIZ){
			horizontal_flip(ctx, src_planes[i], dst_planes[i], height, i, wiB, ctx->src_stride);
		}else if (ctx->mode==VFLIP_BOTH){
			vertical_flip(ctx, src_planes[i], dst_planes[i], height, i, wiB);
			horizontal_flip(ctx, dst_planes[i], dst_planes[i], height, i, wiB, ctx->dst_stride);
		}
	}

	gf_filter_pck_send(dst_pck);
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}


static GF_Err vflip_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	u32 w, h, stride, pfmt;
	GF_Fraction sar;
	GF_VFlipCtx *ctx = gf_filter_get_udta(filter);

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
		ctx->passthrough = GF_TRUE;
	} else if (ctx->mode==VFLIP_OFF) {
		ctx->passthrough = GF_TRUE;
	} else {
		Bool res;

		ctx->w = w;
		ctx->h = h;
		ctx->s_pfmt = pfmt;
		ctx->stride = stride;
		ctx->dst_width  = w;
		ctx->dst_height = h;
		ctx->passthrough = GF_FALSE;

		//get layout info for source
		memset(ctx->src_stride, 0, sizeof(ctx->src_stride));
		if (ctx->stride) ctx->src_stride[0] = ctx->stride;

		res = gf_pixel_get_size_info(pfmt, w, h, &ctx->out_src_size, &ctx->src_stride[0], &ctx->src_stride[1], &ctx->nb_src_planes, &ctx->src_uv_height);
		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[VFlip] Failed to query source pixel format characteristics\n"));
			return GF_NOT_SUPPORTED;
		}
		if (ctx->nb_src_planes==3) ctx->src_stride[2] = ctx->src_stride[1];
		if (ctx->nb_src_planes==4) ctx->src_stride[3] = ctx->src_stride[0];


		//get layout info for dest
		memset(ctx->dst_stride, 0, sizeof(ctx->dst_stride));
		res = gf_pixel_get_size_info(pfmt, ctx->dst_width, ctx->dst_height, &ctx->out_size, &ctx->dst_stride[0], &ctx->dst_stride[1], &ctx->nb_planes, &ctx->dst_uv_height);
		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[VFlip] Failed to query output pixel format characteristics\n"));
			return GF_NOT_SUPPORTED;
		}
		if (ctx->nb_planes==3) ctx->dst_stride[2] = ctx->dst_stride[1];
		if (ctx->nb_planes==4) ctx->dst_stride[3] = ctx->dst_stride[0];


		ctx->w = w;
		ctx->h = h;
		ctx->s_pfmt = pfmt;

		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[VFlip] Configured output full frame size %dx%d\n", ctx->w, ctx->h));

		ctx->line_buffer_vf = gf_realloc(ctx->line_buffer_vf, sizeof(char)*ctx->dst_stride[0] );
		ctx->line_buffer_hf = gf_realloc(ctx->line_buffer_hf, sizeof(char)*ctx->src_stride[0] );

		ctx->packed_422 = GF_FALSE;
		switch (pfmt) {
		//for YUV 422, adjust to multiple of 2 on horizontal dim
		case GF_PIXEL_YUYV:
		case GF_PIXEL_YVYU:
		case GF_PIXEL_UYVY:
		case GF_PIXEL_VYUY:
			ctx->packed_422 = GF_TRUE;
			break;
		}
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->dst_width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->dst_height));

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->dst_stride[0] ));
	if (ctx->nb_planes>1)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE_UV, &PROP_UINT(ctx->dst_stride[1]));

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, &PROP_FRAC(sar) );

	//an access unit corresponds to a single packet
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	return GF_OK;
}

void vflip_finalize(GF_Filter *filter)
{
	GF_VFlipCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->line_buffer_vf) gf_free(ctx->line_buffer_vf);
	if (ctx->line_buffer_hf) gf_free(ctx->line_buffer_hf);
}


#define OFFS(_n)	#_n, offsetof(GF_VFlipCtx, _n)
static GF_FilterArgs VFlipArgs[] =
{
		{ OFFS(mode), "flip mode\n"
		"- off: no flipping (passthrough)\n"
		"- vert: vertical flip\n"
		"- horiz: horizontal flip\n"
		"- both: horizontal and vertical flip"
		"", GF_PROP_UINT, "vert", "off|vert|horiz|both", GF_FS_ARG_UPDATE | GF_FS_ARG_HINT_ADVANCED},
		{0}
};

static const GF_FilterCapability VFlipCaps[] =
{
		CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
		CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW)
};

GF_FilterRegister VFlipRegister = {
		.name = "vflip",
		GF_FS_SET_DESCRIPTION("Video flip")
		GF_FS_SET_HELP("Filter used to flip video frames vertically, horizontally, in both directions or no flip")
		.private_size = sizeof(GF_VFlipCtx),
		.flags = GF_FS_REG_EXPLICIT_ONLY,
		.args = VFlipArgs,
		.configure_pid = vflip_configure_pid,
		SETCAPS(VFlipCaps),
		.process = vflip_process,
		.finalize = vflip_finalize,
};



const GF_FilterRegister *vflip_register(GF_FilterSession *session)
{
	return &VFlipRegister;
}
