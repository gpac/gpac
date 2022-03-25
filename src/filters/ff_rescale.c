/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2022
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

#include <gpac/setup.h>

#ifdef GPAC_HAS_FFMPEG

#include "ff_common.h"
#include <gpac/evg.h>

enum
{
	FFSWS_KEEPAR_OFF=0,
	FFSWS_KEEPAR_FULL,
	FFSWS_KEEPAR_NOSRC,
};

typedef struct
{
	//options
	GF_PropVec2i osize;
	u32 ofmt, scale;
	Double p1, p2;
	Bool ofr;
	u32 brightness, saturation, contrast;
	GF_PropIntList otable, itable;
	//internal data
	Bool initialized;
	char *padclr;
	u32 keepar;
	GF_Fraction osar;

	GF_FilterPid *ipid, *opid;
	u32 w, h, stride, s_pfmt;
	GF_Fraction ar;
	Bool passthrough;

	struct SwsContext *swscaler;

	u32 dst_stride[5];
	u32 src_stride[5];
	u32 nb_planes, nb_src_planes, out_size, out_src_size, src_uv_height, dst_uv_height, ow, oh;

	u32 swap_idx_1, swap_idx_2;
	Bool fullrange;

	u32 o_bpp, offset_w, offset_h;
	GF_EVGSurface *surf;

	Bool unpack_v410, repack_v410, unpack_v210, repack_v210;
	u32 orig_in_stride, repack_stride;
	u8 *unpack_buf, *repack_buf;
} GF_FFSWScaleCtx;

//#include <gpac/bitstream.h>
#include <libavutil/intreadwrite.h>

static GF_Err ffsws_process(GF_Filter *filter)
{
	const char *data;
	u8 *output, *pck_data;
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
	//we may have buffer input (padding) but shall not have smaller
	if (osize && (ctx->out_src_size > osize) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFSWS] Mismatched in source osize, expected %d got %d - stride issue ?\n", ctx->out_src_size, osize));
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NOT_SUPPORTED;
	}

	memset(src_planes, 0, sizeof(src_planes));
	memset(dst_planes, 0, sizeof(dst_planes));
	dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->out_size, &output);
	if (!dst_pck) return GF_OUT_OF_MEM;

	gf_filter_pck_merge_properties(pck, dst_pck);

	if (data) {
		src_planes[0] = (u8 *) data;

		if (ctx->unpack_v410) {
			u32 i, j;
			for (j=0; j<ctx->h; j++) {
				const u8 *p_src = data + j * ctx->orig_in_stride;
				u8 *p_y = ctx->unpack_buf + j * ctx->w * 2;
				u8 *p_u = p_y + ctx->h * ctx->w * 2;
				u8 *p_v = p_u + ctx->h * ctx->w * 2;

				for (i=0; i<ctx->w; i++) {
					u32 val = AV_RL32(p_src);
					*(u16*) p_u = (val>>2) & 0x3FF;
					*(u16*) p_y = (val>>12) & 0x3FF;
					*(u16*) p_v = (val>>22);

					p_u+=2;
					p_v+=2;
					p_y+=2;
					p_src+=4;
				}
			}
			src_planes[0] = ctx->unpack_buf;
			src_planes[1] = src_planes[0] + ctx->src_stride[0]*ctx->h;
			src_planes[2] = src_planes[1] + ctx->src_stride[1]*ctx->h;
		} else if (ctx->unpack_v210) {
			u32 i, j;
			for (j=0; j<ctx->h; j++) {
				u32 cur_w = 0;
				const u8 *p_src = data + j * ctx->orig_in_stride;
				u8 *p_y = ctx->unpack_buf + j * ctx->w * 2;
				u8 *p_u = ctx->unpack_buf + j * ctx->w + ctx->h * ctx->w * 2;
				u8 *p_v = p_u + ctx->h * ctx->w;

				for (i=0; i<ctx->w; i+=6) {
					u32 val;
					u16 u, v, y;
					//first word is xx Cr0 Y0 Cb0
					val = AV_RL32(p_src);
					u = (val) & 0x3FF;
					*(u16*) p_y = (val>>10) & 0x3FF;
					v = (val>>20) & 0x3FF;
					*(u16*) p_u = u;
					*(u16*) p_v = v;
					p_u+=2;
					p_v+=2;
					p_y+=2;
					p_src+=4;
					cur_w++;
					if (cur_w>=ctx->w) break;

					//second word is xx Y2 Cb1 Y1
					val = AV_RL32(p_src);
					y = (val) & 0x3FF;

					*(u16*) p_y = y;
					p_y+=2;
					cur_w++;
					if (cur_w>=ctx->w) break;

					u = (val>>10) & 0x3FF;
					y = (val>>20) & 0x3FF;
					p_src+=4;

					//third word is xx Cb2 Y3 Cr1
					val = AV_RL32(p_src);
					v = (val) & 0x3FF;
					*(u16*) p_y = y;
					*(u16*) p_u = u;
					*(u16*) p_v = v;
					p_u+=2;
					p_v+=2;
					p_y+=2;
					cur_w++;
					if (cur_w>=ctx->w) break;

					y = (val>>10) & 0x3FF;
					*(u16*) p_y = y;
					p_y+=2;
					cur_w++;
					if (cur_w>=ctx->w) break;

					u = (val>>20) & 0x3FF;
					p_src+=4;

					//fourth word is xx Y5 Cr2 Y4
					val = AV_RL32(p_src);
					y = (val) & 0x3FF;
					v = (val>>10) & 0x3FF;

					*(u16*) p_y = y;
					*(u16*) p_u = u;
					*(u16*) p_v = v;
					p_u+=2;
					p_v+=2;
					p_y+=2;
					cur_w++;
					if (cur_w>=ctx->w) break;

					y = (val>>20) & 0x3FF;
					*(u16*) p_y = y;
					p_y+=2;
					p_src+=4;
					cur_w++;
					if (cur_w>=ctx->w) break;
				}
			}
			src_planes[0] = ctx->unpack_buf;
			src_planes[1] = src_planes[0] + ctx->src_stride[0]*ctx->h;
			src_planes[2] = src_planes[1] + ctx->src_stride[1]*ctx->h;
		} else {
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

	pck_data = output;
	if (ctx->repack_v410 || ctx->repack_v210) {
		output = ctx->repack_buf;
	}

	dst_planes[0] = output;
	dst_planes[0] += ctx->dst_stride[0] * ctx->offset_h + ctx->offset_w*ctx->o_bpp;

	if (ctx->nb_planes==1) {
	} else if (ctx->nb_planes==2) {
		dst_planes[1] = output + ctx->dst_stride[0] * ctx->oh;

		dst_planes[1] += ctx->dst_stride[1] * ctx->offset_h * ctx->dst_uv_height / ctx->oh;
		dst_planes[1] += ctx->offset_w*ctx->o_bpp*ctx->dst_stride[1]/ctx->dst_stride[0];
	} else if (ctx->nb_planes==3) {
		dst_planes[1] = output + ctx->dst_stride[0] * ctx->oh;
		dst_planes[2] = dst_planes[1] + ctx->dst_stride[1]*ctx->dst_uv_height;

		dst_planes[1] += ctx->dst_stride[1] * ctx->offset_h * ctx->dst_uv_height / ctx->oh;
		dst_planes[1] += ctx->offset_w*ctx->o_bpp*ctx->dst_stride[1]/ctx->dst_stride[0];

		dst_planes[2] += ctx->dst_stride[2] * ctx->offset_h * ctx->dst_uv_height / ctx->oh;
		dst_planes[2] += ctx->offset_w*ctx->o_bpp*ctx->dst_stride[2]/ctx->dst_stride[0];
	} else if (ctx->nb_planes==4) {
		dst_planes[1] = output + ctx->dst_stride[0] * ctx->oh;
		dst_planes[2] = dst_planes[1] + ctx->dst_stride[1]*ctx->dst_uv_height;
		dst_planes[3] = dst_planes[2] + ctx->dst_stride[2]*ctx->dst_uv_height;

		dst_planes[1] += ctx->dst_stride[1] * ctx->offset_h * ctx->dst_uv_height / ctx->oh;
		dst_planes[1] += ctx->offset_w*ctx->o_bpp*ctx->dst_stride[1]/ctx->dst_stride[0];

		dst_planes[2] += ctx->dst_stride[2] * ctx->offset_h * ctx->dst_uv_height / ctx->oh;
		dst_planes[2] += ctx->offset_w*ctx->o_bpp*ctx->dst_stride[2]/ctx->dst_stride[0];

		dst_planes[3] += ctx->dst_stride[3] * ctx->offset_h + ctx->offset_w*ctx->o_bpp;
	}
	if (ctx->offset_w || ctx->offset_h) {
		u32 color = ctx->padclr ? gf_color_parse(ctx->padclr) : 0xFF000000;
		gf_evg_surface_attach_to_buffer(ctx->surf, output, ctx->ow, ctx->oh, 0, ctx->dst_stride[0], ctx->ofmt);
		gf_evg_surface_clear(ctx->surf, NULL, color);
	}

	//rescale the cropped frame
	res = sws_scale(ctx->swscaler, (const u8**) src_planes, ctx->src_stride, 0, ctx->h, dst_planes, ctx->dst_stride);
	if (res + 2*ctx->offset_h != ctx->oh) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFSWS] Error during scale, expected height %d got %d\n", ctx->oh, res));
		gf_filter_pid_drop_packet(ctx->ipid);
		gf_filter_pck_discard(dst_pck);
		return GF_NOT_SUPPORTED;
	}


	if (ctx->repack_v410) {
		u32 i, j;
		output = pck_data;
		for (j=0; j<ctx->oh; j++) {
			u8 *p_dst = output + j * ctx->repack_stride;
			u8 *p_y = ctx->repack_buf + j * ctx->ow * 2;
			u8 *p_u = p_y + ctx->oh * ctx->ow * 2;
			u8 *p_v = p_u + ctx->oh * ctx->ow * 2;

			for (i=0; i<ctx->ow; i++) {
				u32 val = 0;
				val = *(u16 *) p_v; val<<=10;
				val |= *(u16 *) p_y; val<<=10;
				val |= *(u16 *) p_u; val<<=2;

				p_dst[0] = val & 0xFF;
				p_dst[1] = (val>>8) & 0xFF;
				p_dst[2] = (val>>16) & 0xFF;
				p_dst[3] = (val>>24) & 0xFF;

				p_u+=2;
				p_v+=2;
				p_y+=2;
				p_dst+=4;
			}
		}
	}
	else if (ctx->repack_v210) {
		u32 i, j;
		output = pck_data;
		for (j=0; j<ctx->oh; j++) {
			u8 *p_dst = output + j * ctx->repack_stride;
			u8 *p_y = ctx->repack_buf + j * ctx->ow * 2;
			u8 *p_u = ctx->repack_buf + ctx->oh * ctx->ow * 2 + j * ctx->ow;
			u8 *p_v = p_u + ctx->oh * ctx->ow;
			u32 cur_uv_w=0;

			for (i=0; i<ctx->ow; i+=6) {
				u32 val = 0;
				val = *(u16 *) p_u;
				p_u+=2;
				val |= ((u32) *(u16 *) p_y) << 10;
				p_y+=2;
				val |= ((u32) *(u16 *) p_v) << 20;
				p_v+=2;

#define PUT_WORD_LE \
				p_dst[0] = val & 0xFF;\
				p_dst[1] = (val>>8) & 0xFF;\
				p_dst[2] = (val>>16) & 0xFF;\
				p_dst[3] = (val>>24) & 0xFF;\
				p_dst+=4;\

				PUT_WORD_LE

				val = *(u16 *) p_y;
				p_y+=2;

				cur_uv_w+=2;
				if (cur_uv_w>=ctx->ow) {
					PUT_WORD_LE
					break;
				}
				val |= ((u32) *(u16 *) p_u) << 10;
				p_u+=2;
				val |= ((u32) *(u16 *) p_y) << 20;
				p_y+=2;

				PUT_WORD_LE

				val = *(u16 *) p_v;
				p_v+=2;
				val |= ((u32) *(u16 *) p_y) << 10;
				p_y+=2;

				cur_uv_w+=2;
				if (cur_uv_w>=ctx->ow) {
					PUT_WORD_LE
					break;
				}

				val |= ((u32) *(u16 *) p_u) << 20;
				p_u+=2;

				PUT_WORD_LE

				val = *(u16 *) p_y;
				p_y+=2;
				val |= ((u32) *(u16 *) p_v) << 10;
				p_v+=2;
				val |= ((u32) *(u16 *) p_y) << 20;
				p_y+=2;

				PUT_WORD_LE

				cur_uv_w+=2;
				if (cur_uv_w>=ctx->ow)
					break;
			}
		}
	}	else if (ctx->swap_idx_1 || ctx->swap_idx_2) {
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
	Bool fullrange=GF_FALSE;
	Double par[2], *par_p=NULL;
	GF_FFSWScaleCtx *ctx = gf_filter_get_udta(filter);

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
		}
		return GF_OK;
	}



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
	if ((ctx->keepar == FFSWS_KEEPAR_FULL) && (sar.num > (s32) sar.den)) {
		scale_w = w * sar.num / sar.den;
		sar.num=0;
	}

	if (!ctx->osize.x && !ctx->osize.y) {
		ctx->ow = w;
		ctx->oh = h;
		scale_w = w;
	}
	else if (!ctx->osize.x) {
		ctx->oh = ctx->osize.y;
		ctx->ow = ctx->osize.y * scale_w / h;
	}
	else if (!ctx->osize.y) {
		ctx->ow = ctx->osize.x;
		ctx->oh = ctx->osize.x * h / scale_w;
	} else {
		ctx->ow = ctx->osize.x;
		ctx->oh = ctx->osize.y;
	}
	ctx->offset_w = ctx->offset_h = 0;

	u32 final_w = ctx->ow;
	u32 final_h = ctx->oh;
	if (ctx->keepar!=FFSWS_KEEPAR_OFF) {
		if (ctx->ow * h < scale_w * ctx->oh) {
			final_h = ctx->ow * h / scale_w;
		} else if (ctx->ow * h > scale_w * ctx->oh) {
			final_w = ctx->oh * scale_w / h;
		}
	}
	if (ctx->osar.num > (s32) ctx->osar.den) {
		ctx->ow = ctx->ow * ctx->osar.den / ctx->osar.num;
		final_w = final_w * ctx->osar.den / ctx->osar.num;
		if (downsample_w) {
			if (ctx->ow % 2) ctx->ow -= 1;
			if (final_w % 2) final_w -= 1;
		}
	}
	ctx->offset_w = (ctx->ow - final_w) / 2;
	ctx->offset_h = (ctx->oh - final_h) / 2;

	if (downsample_w && (ctx->offset_w % 2)) ctx->offset_w -= 1;
	if (downsample_h && (ctx->offset_h % 2)) ctx->offset_h -= 1;

	if ((ctx->w == w) && (ctx->h == h) && (ctx->s_pfmt == ofmt) && (ctx->stride == stride) && (ctx->fullrange==fullrange)) {
		//nothing to reconfigure
	}
	//passthrough mode
	else if ((ctx->ow == w) && (ctx->oh == h) && (ofmt==ctx->ofmt) && (ctx->ofr == fullrange)
		&& !ctx->brightness && !ctx->saturation && !ctx->contrast && (ctx->otable.nb_items!=4) && (ctx->itable.nb_items!=4)
	) {
		memset(ctx->dst_stride, 0, sizeof(ctx->dst_stride));
		gf_pixel_get_size_info(ctx->ofmt, ctx->ow, ctx->oh, &ctx->out_size, &ctx->dst_stride[0], &ctx->dst_stride[1], &ctx->nb_planes, &ctx->dst_uv_height);
		ctx->passthrough = GF_TRUE;
	} else {
		u32 nb_par = 0;
		nb_par = 0;
		Bool res;
		u32 mode = get_sws_mode(ctx->scale, &nb_par);

		u32 ff_src_pfmt, ff_dst_pfmt;
		ctx->unpack_v410 = ctx->repack_v410 = GF_FALSE;
		ctx->unpack_v210 = ctx->repack_v210 = GF_FALSE;
		//v410 and v210 as treated as codecs in ffmpeg, we deal with them as pixel formats
		if (ofmt == GF_PIXEL_YUV444_10_PACK) {
			ctx->unpack_v410 = GF_TRUE;
			ff_src_pfmt = ffmpeg_pixfmt_from_gpac(GF_PIXEL_YUV444_10);
		} else if (ofmt == GF_PIXEL_V210) {
			ctx->unpack_v210 = GF_TRUE;
			ff_src_pfmt = ffmpeg_pixfmt_from_gpac(GF_PIXEL_YUV422_10);
		} else {
			ff_src_pfmt = ffmpeg_pixfmt_from_gpac(ofmt);
		}
		if (ctx->ofmt == GF_PIXEL_YUV444_10_PACK) {
			ctx->repack_v410 = GF_TRUE;
			ff_dst_pfmt = ffmpeg_pixfmt_from_gpac(GF_PIXEL_YUV444_10);
		} else if (ctx->ofmt == GF_PIXEL_V210) {
			ctx->repack_v210 = GF_TRUE;
			ff_dst_pfmt = ffmpeg_pixfmt_from_gpac(GF_PIXEL_YUV422_10);
		} else {
			ff_dst_pfmt = ffmpeg_pixfmt_from_gpac(ctx->ofmt);
		}

		if ((ff_src_pfmt==AV_PIX_FMT_NONE) || (ff_dst_pfmt==AV_PIX_FMT_NONE))
			return GF_NOT_SUPPORTED;

		//get layout info for source
		memset(ctx->src_stride, 0, sizeof(ctx->src_stride));
		if (ctx->stride) ctx->src_stride[0] = ctx->stride;


		res = gf_pixel_get_size_info(ofmt, w, h, &ctx->out_src_size, &ctx->src_stride[0], &ctx->src_stride[1], &ctx->nb_src_planes, &ctx->src_uv_height);
		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFSWS] Failed to query source pixel format characteristics\n"));
			return GF_NOT_SUPPORTED;
		}
		if (ctx->unpack_v410) {
			ctx->unpack_buf = gf_realloc(ctx->unpack_buf, w*h*3*2);
			ctx->nb_src_planes = 3;
			ctx->orig_in_stride = ctx->src_stride[0];
			ctx->src_stride[2] = ctx->src_stride[1] = ctx->src_stride[0] = 2*w;
			ctx->src_uv_height = h;
		} else if (ctx->unpack_v210) {
			ctx->unpack_buf = gf_realloc(ctx->unpack_buf, w*h*2*2);
			ctx->nb_src_planes = 3;
			ctx->orig_in_stride = ctx->src_stride[0];
			ctx->src_stride[0] = 2*w;
			ctx->src_stride[2] = ctx->src_stride[1] = w;
			ctx->src_uv_height = h;
		} else {
			if (ctx->nb_src_planes==3) ctx->src_stride[2] = ctx->src_stride[1];
			if (ctx->nb_src_planes==4) ctx->src_stride[3] = ctx->src_stride[0];
		}

		ctx->o_bpp = gf_pixel_get_bytes_per_pixel(ofmt);
		//get layout info for dest
		memset(ctx->dst_stride, 0, sizeof(ctx->dst_stride));
		res = gf_pixel_get_size_info(ctx->ofmt, ctx->ow, ctx->oh, &ctx->out_size, &ctx->dst_stride[0], &ctx->dst_stride[1], &ctx->nb_planes, &ctx->dst_uv_height);
		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFSWS] Failed to query output pixel format characteristics\n"));
			return GF_NOT_SUPPORTED;
		}
		if (ctx->nb_planes==3) ctx->dst_stride[2] = ctx->dst_stride[1];
		if (ctx->nb_planes==4) ctx->dst_stride[3] = ctx->dst_stride[0];

		if (ctx->repack_v410) {
			ctx->nb_planes = 3;
			ctx->repack_stride = ctx->dst_stride[0];
			ctx->repack_buf = gf_realloc(ctx->repack_buf, ctx->ow * ctx->oh * 3 * 2);
			ctx->dst_stride[2] = ctx->dst_stride[1] = ctx->dst_stride[0] = 2 * ctx->ow;
			ctx->dst_uv_height = ctx->oh;
			ctx->o_bpp = 2;
		}
		else if (ctx->repack_v210) {
			ctx->nb_planes = 3;
			ctx->repack_stride = ctx->dst_stride[0];
			ctx->repack_buf = gf_realloc(ctx->repack_buf, ctx->ow * ctx->oh * 2 * 2);
			ctx->dst_stride[0] = 2 * ctx->ow;
			ctx->dst_stride[2] = ctx->dst_stride[1] = ctx->ow;
			ctx->dst_uv_height = ctx->oh;
			ctx->o_bpp = 2;
		}
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
		ctx->swscaler = sws_getCachedContext(ctx->swscaler, w, h, ff_src_pfmt, ctx->ow-2*ctx->offset_w, ctx->oh-2*ctx->offset_h, ff_dst_pfmt, mode, NULL, NULL, par_p);

		if (!ctx->swscaler) {
#ifndef GPAC_DISABLE_LOG
			Bool in_ok = sws_isSupportedInput(ff_src_pfmt);
			Bool out_ok = sws_isSupportedInput(ff_dst_pfmt);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFSWS] Cannot allocate context for required format - input %s output %s\n", in_ok ? "OK" : "not supported" , out_ok ? "OK" : "not supported"));
#endif
			return GF_NOT_SUPPORTED;
		}
		//set colorspace
		if (fullrange || ctx->ofr || ctx->brightness || ctx->saturation || ctx->contrast || (ctx->itable.nb_items==4) || (ctx->otable.nb_items==4)) {
			s32 in_full, out_full, brightness, contrast, saturation;
			s32 *inv_table, *table;

			sws_getColorspaceDetails(ctx->swscaler, &inv_table, &in_full, &table, &out_full, &brightness, &contrast, &saturation);
			in_full = fullrange;
			out_full = ctx->ofr;
			if (ctx->brightness) brightness = ctx->brightness;
			if (ctx->saturation) saturation = ctx->saturation;
			if (ctx->contrast) contrast = ctx->contrast;

			if (ctx->itable.nb_items==4)
				inv_table = ctx->itable.vals;
			if (ctx->otable.nb_items==4)
				table = ctx->otable.vals;

			sws_setColorspaceDetails(ctx->swscaler, (const int *) inv_table, in_full, (const int *)table, out_full, brightness, contrast, saturation);
		}
		ctx->w = w;
		ctx->h = h;
		ctx->s_pfmt = ofmt;
		ctx->fullrange = fullrange;
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[FFSWS] Setup rescaler from %dx%d fmt %s to %dx%d fmt %s\n", w, h, gf_pixel_fmt_name(ofmt), ctx->ow, ctx->oh, gf_pixel_fmt_name(ctx->ofmt)));

		ctx->swap_idx_1 = ctx->swap_idx_2 = 0;
		//if same source / dest pixel format, don't swap UV components
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

	if (ctx->repack_v410 || ctx->repack_v210) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->repack_stride));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE_UV, NULL );
	} else {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->dst_stride[0]));
		if (ctx->nb_planes>1)
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE_UV, &PROP_UINT(ctx->dst_stride[1]));
		else
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE_UV, NULL );
	}

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

static GF_Err ffsws_initialize(GF_Filter *filter)
{
	GF_FFSWScaleCtx *ctx = gf_filter_get_udta(filter);
	ffmpeg_setup_logs(GF_LOG_MEDIA);
	ctx->surf = gf_evg_surface_new(GF_FALSE);

	if (ctx->osar.num &&
	 ((ctx->osar.num<0) || (ctx->osar.num < (s32) ctx->osar.den))
	) {
		ctx->osar.num = 0;
		ctx->osar.den = 1;
	}
	return GF_OK;
}
static void ffsws_finalize(GF_Filter *filter)
{
	GF_FFSWScaleCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->swscaler) sws_freeContext(ctx->swscaler);
	if (ctx->unpack_buf) gf_free(ctx->unpack_buf);
	if (ctx->repack_buf) gf_free(ctx->repack_buf);
	gf_evg_surface_delete(ctx->surf);
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
	{ OFFS(osize), "osize of output video", GF_PROP_VEC2I, NULL, NULL, 0},
	{ OFFS(ofmt), "pixel format for output video. When not set, input format is used", GF_PROP_PIXFMT, "none", NULL, 0},
	{ OFFS(scale), "scaling mode (see filter help)", GF_PROP_UINT, "bicubic", "fastbilinear|bilinear|bicubic|X|point|area|bicublin|gauss|sinc|lanzcos|spline", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(p1), "scaling algo param1", GF_PROP_DOUBLE, "+I", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(p2), "scaling algo param2", GF_PROP_DOUBLE, "+I", NULL, GF_FS_ARG_HINT_ADVANCED},

	{ OFFS(ofr), "force output full range", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(brightness), "16.16 fixed point brightness correction, 0 means use default", GF_PROP_BOOL, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(contrast), "16.16 fixed point brightness correction, 0 means use default", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(saturation), "16.16 fixed point brightness correction, 0 means use default", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(otable), "the yuv2rgb coefficients describing the output yuv space, normally ff_yuv2rgb_coeffs[x], use default if not set", GF_PROP_SINT_LIST, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(itable), "the yuv2rgb coefficients describing the input yuv space, normally ff_yuv2rgb_coeffs[x], use default if not set", GF_PROP_SINT_LIST, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(keepar), "keep aspect ratio\n"
	"- off: ignore aspect ratio\n"
	"- full: respect aspect ratio, applying input sample aspect ratio info\n"
	"- nosrc: respect aspect ratio but ignore input sample aspect ratio"
	, GF_PROP_UINT, "off", "off|full|nosrc", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(padclr), "clear color when aspect ration preservation is used", GF_PROP_STRING, "black", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(osar), "force output pixel aspect ratio", GF_PROP_FRACTION, "0/1", NULL, GF_FS_ARG_HINT_EXPERT},
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
	GF_FS_SET_HELP("This filter rescales raw video data using FFMPEG to the specified size and pixel format.\n"
	"## Output size assignment\n"
	"If [-osize]() is {0,0}, the output dimensions will be set to the input size, and input aspect ratio will be ignored.\n"
	"\n"
	"If [-osize]() is {0,H} (resp. {W,0}), the output width (resp. height) will be set to respect input aspect ratio. If [-keepar=nosrc](), input sample aspect ratio is ignored.\n"
	"## Aspect Ratio and Sample Aspect Ratio\n"
	"When output sample aspect ratio is set, the output dimensions are divided by the output sample aspect ratio.\n"
	"EX ffsws:osize=288x240:osar=3/2\n"
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
	"\n"
	"## Algorithms options\n"
	"- for bicubic, to tune the shape of the basis function, [-p1]() tunes f(1) and [-p2]() f´(1)\n"
	"- for gauss [-p1]() tunes the exponent and thus cutoff frequency\n"
	"- for lanczos [-p1]() tunes the width of the window function\n"
	"\n"
	"See FFMPEG documentation (https://ffmpeg.org/documentation.html) for more details")
	.private_size = sizeof(GF_FFSWScaleCtx),
	.args = FFSWSArgs,
	.configure_pid = ffsws_configure_pid,
	SETCAPS(FFSWSCaps),
	.flags = GF_FS_REG_ALLOW_CYCLIC,
	.initialize = ffsws_initialize,
	.finalize = ffsws_finalize,
	.process = ffsws_process,
	.reconfigure_output = ffsws_reconfigure_output,
};

#else
#include <gpac/filters.h>
#endif //GPAC_HAS_FFMPEG

const GF_FilterRegister *ffsws_register(GF_FilterSession *session)
{
#ifdef GPAC_HAS_FFMPEG
	FFSWSArgs[1].min_max_enum = gf_pixel_fmt_all_names();
	return &FFSWSRegister;
#else
	return NULL;
#endif
}

