/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Compositor sub-project
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


#include "texturing.h"
#include "visual_manager.h"
#include "nodes_stacks.h"
#include "gl_inc.h"



/*tx flags*/
enum
{
	/*signal video data must be sent to 2D raster*/
	TX_NEEDS_RASTER_LOAD = (1<<1),
	/*signal video data must be sent to 3D hw*/
	TX_NEEDS_HW_LOAD = (1<<2),

	/*OpenGL texturing flags*/

	/*texture is rect*/
	TX_IS_RECT = (1<<10),
	/*texture emulates power of two through larger buffer and upscaling*/
	TX_EMULE_POW2 = (1<<11),
	/*texture is flipped*/
	TX_IS_FLIPPED = (1<<12),
	/*texture must be converted*/
	TX_CONV_8BITS = (1<<13),

	TX_FIRST_UPLOAD_FREEZE = (1<<14),
	TX_FIRST_UPLOAD_FREEZE_DONE = (1<<15),
};


struct __texture_wrapper
{
	u32 flags;

	/*2D texturing*/
	GF_EVGStencil * tx_raster;
	u8 *conv_data;
	GF_Matrix texcoordmatrix;

	//format of converted texture for 10->8 bit or non RGB->RGB if no shader support
	u32 conv_format;
	//size info of converted data
	u32 conv_w, conv_h, conv_stride;

	/*3D OpenGL texturing*/
#ifndef GPAC_DISABLE_3D
	/*opengl texture id*/
	u32 blend_mode;
	//scaling factor when emulating pow2
	Fixed conv_wscale, conv_hscale;

	//texture wrapper object
	GF_GLTextureWrapper tx;

	/* gl_format: GL_RGB, GL_RGBA, GL_LUMINANCE or GL_LUMINANCE_ALPHA*/
	u32 gl_format;
	/* gl_type: 2D texture or rectangle*/
	u32 gl_type;

	/*FBO framebuffer ID*/
	u32 fbo_id;
	/*FBO texture attachment ID*/
	u32 fbo_txid;
	/*FBO depth buffer attacghment ID*/
	u32 depth_id;
#endif

#ifdef GF_SR_USE_DEPTH
	char *depth_data;
#endif
};

GF_Err gf_sc_texture_allocate(GF_TextureHandler *txh)
{
	if (txh->tx_io) return GF_OK;
	GF_SAFEALLOC(txh->tx_io, struct __texture_wrapper);
	if (!txh->tx_io) return GF_OUT_OF_MEM;
	return GF_OK;
}

GF_Err gf_sc_texture_configure_conversion(GF_TextureHandler *txh)
{
	txh->flags &= ~TX_CONV_8BITS;
	if (txh->compositor->out8b) {
		u32 osize;
		if (txh->pixelformat == GF_PIXEL_YUV_10) {
			txh->tx_io->conv_format = GF_PIXEL_YUV;
		}
		else if (txh->pixelformat == GF_PIXEL_NV12_10) {
			txh->tx_io->conv_format = GF_PIXEL_NV12;
		}
		else if (txh->pixelformat == GF_PIXEL_NV21_10) {
			txh->tx_io->conv_format = GF_PIXEL_NV21;
		}
		else if (txh->pixelformat == GF_PIXEL_YUV422_10) {
			txh->tx_io->conv_format = GF_PIXEL_YUV422;
		}
		else if (txh->pixelformat == GF_PIXEL_YUV444_10) {
			txh->tx_io->conv_format = GF_PIXEL_YUV444;
		} else {
			return GF_OK;
		}
		osize = 0;
		txh->tx_io->conv_stride = 0;
		gf_pixel_get_size_info(txh->tx_io->conv_format, txh->width, txh->height, &osize, &txh->tx_io->conv_stride, NULL, NULL, NULL);
		txh->tx_io->conv_data = (char*)gf_realloc(txh->tx_io->conv_data, osize);
		txh->flags |= TX_CONV_8BITS;
	}
	return GF_OK;
}



static void release_txio(struct __texture_wrapper *tx_io)
{

#ifndef GPAC_DISABLE_3D
	if (tx_io->fbo_id) {
		compositor_3d_delete_fbo(&tx_io->fbo_id, &tx_io->fbo_txid, &tx_io->depth_id, GF_FALSE);
	} else {
		gf_gl_txw_reset(&tx_io->tx);
	}
#endif

	if (tx_io->conv_data) gf_free(tx_io->conv_data);

#ifdef GF_SR_USE_DEPTH
	if (tx_io->depth_data) gf_free(tx_io->depth_data);
#endif

	gf_free(tx_io);
}

void gf_sc_texture_release(GF_TextureHandler *txh)
{
	if (txh->vout_udta && txh->compositor->video_out->ReleaseTexture) {
		txh->compositor->video_out->ReleaseTexture(txh->compositor->video_out, txh);
		txh->vout_udta = NULL;
	}

	if (txh->tx_io) {
		gf_sc_lock(txh->compositor, 1);

		if (txh->tx_io->tx_raster) {
			gf_evg_stencil_delete(txh->tx_io->tx_raster);
			txh->tx_io->tx_raster = NULL;
		}

		if (gf_th_id()==txh->compositor->video_th_id) {
			release_txio(txh->tx_io);
		} else {
			gf_list_add(txh->compositor->textures_gc, txh->tx_io);
		}
		txh->tx_io = NULL;

		gf_sc_lock(txh->compositor, 0);
	}
}

void gf_sc_texture_cleanup_hw(GF_Compositor *compositor)
{
	while (gf_list_count(compositor->textures_gc)) {
		struct __texture_wrapper *tx_io = (struct __texture_wrapper *) gf_list_last(compositor->textures_gc);
		gf_list_rem_last(compositor->textures_gc);

		release_txio(tx_io);
	}
}



GF_Err gf_sc_texture_set_data(GF_TextureHandler *txh)
{
#ifndef GPAC_DISABLE_3D
	u8 *data=NULL;
	GF_FilterFrameInterface *fifce = txh->frame_ifce;
#endif
	txh->tx_io->flags |= TX_NEEDS_RASTER_LOAD | TX_NEEDS_HW_LOAD;

#ifndef GPAC_DISABLE_3D
	data = txh->data;
	//10->8 bit conversion
	if (txh->flags & TX_CONV_8BITS) {
		GF_VideoSurface dst, src;
		GF_Err e;
		memset(&dst, 0, sizeof(GF_VideoSurface));
		memset(&src, 0, sizeof(GF_VideoSurface));
		dst.width = txh->width;
		dst.height = txh->height;
		dst.pitch_y = txh->tx_io->conv_stride;
		dst.video_buffer = txh->tx_io->conv_data;
		dst.pixel_format = txh->tx_io->conv_format;

		src.width = txh->width;
		src.height = txh->height;
		src.pitch_y = txh->stride;
		src.video_buffer = txh->data;
		src.pixel_format = txh->pixelformat;

		if (txh->frame_ifce) {
			u32 st_y, st_u, st_v;
			txh->frame_ifce->get_plane(txh->frame_ifce, 0, (const u8 **) &src.video_buffer, &st_y);
			txh->frame_ifce->get_plane(txh->frame_ifce, 1, (const u8 **) &src.u_ptr, &st_u);
			txh->frame_ifce->get_plane(txh->frame_ifce, 2, (const u8 **) &src.v_ptr, &st_v);
			if (src.video_buffer) src.pitch_y = st_y;
		}

		e = gf_stretch_bits(&dst, &src, NULL, NULL, 0xFF, GF_FALSE, NULL, NULL);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[V3D] Failed to convert 10-bits to 8-bit texture: %s\n", gf_error_to_string(e) ));
			return e;
		}
		fifce = NULL;
		data = dst.video_buffer;
	}
#endif


#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
	//PBO mode: start pushing the texture
	if (data && !fifce && (txh->tx_io->tx.pbo_state!=GF_GL_PBO_NONE)) {
		txh->tx_io->tx.pbo_state = GF_GL_PBO_PUSH;
		gf_gl_txw_upload(&txh->tx_io->tx, data, fifce);

		//we just pushed our texture to the GPU, release
		if (txh->frame_ifce) {
			gf_sc_texture_release_stream(txh);
		}
	}
#endif
	return GF_OK;
}

void gf_sc_texture_reset(GF_TextureHandler *txh)
{
#ifndef GPAC_DISABLE_3D
	if (txh->tx_io->tx.nb_textures) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Texturing] Releasing OpenGL texture\n"));
		gf_gl_txw_reset(&txh->tx_io->tx);
	}
	if (txh->tx_io->fbo_id) {
		compositor_3d_delete_fbo(&txh->tx_io->fbo_id, &txh->tx_io->fbo_txid, &txh->tx_io->depth_id, GF_FALSE);
	}
	txh->tx_io->flags |= TX_NEEDS_HW_LOAD;
#endif

#ifdef GF_SR_USE_DEPTH
	if (txh->tx_io->depth_data) {
		gf_free(txh->tx_io->depth_data);
		txh->tx_io->depth_data = NULL;
	}
#endif

	if (txh->vout_udta && txh->compositor->video_out->ReleaseTexture) {
		txh->compositor->video_out->ReleaseTexture(txh->compositor->video_out, txh);
		txh->vout_udta = NULL;
	}
}

#ifndef GPAC_DISABLE_3D

void gf_sc_texture_set_blend_mode(GF_TextureHandler *txh, u32 mode)
{
	if (txh->tx_io) txh->tx_io->blend_mode = mode;
}

void tx_bind_with_mode(GF_TextureHandler *txh, Bool transparent, u32 blend_mode, Bool no_bind, u32 glsl_prog_id)
{
	if (!txh->tx_io->gl_type)
		return;
#ifndef GPAC_USE_GLES2
	if (!no_bind)
		glEnable(txh->tx_io->gl_type);

	switch (blend_mode) {
	case TX_BLEND:
		if (txh->transparent) glEnable(GL_BLEND);
		GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
		break;
	case TX_REPLACE:
		if (txh->transparent) glEnable(GL_BLEND);
		GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		break;
	case TX_MODULATE:
		if (txh->transparent) glEnable(GL_BLEND);
#ifdef GPAC_USE_GLES1X
		GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
#else
		GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif
		break;
	case TX_DECAL:
	default:
		if (((txh->tx_io->gl_format==GL_LUMINANCE)) || (txh->tx_io->gl_format==GL_LUMINANCE_ALPHA)) {
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND/*GL_MODULATE*/);
		} else {
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		}
		break;
	}
#else
	if (txh->transparent) glEnable(GL_BLEND);
	//blend_mode for ES2.0 can be implemented inside the fragment shader if desired
#endif

	if (!no_bind) {
		gf_gl_txw_bind(&txh->tx_io->tx, "maintx", glsl_prog_id, 0);
	}
}

void tx_bind(GF_TextureHandler *txh)
{
	if (txh->tx_io )
		tx_bind_with_mode(txh, txh->transparent, txh->tx_io->blend_mode, 0, 0);
}

void gf_sc_texture_disable(GF_TextureHandler *txh)
{
	if (!txh || !txh->tx_io)
		return;

#ifndef GPAC_USE_GLES1X
	glBindTexture(txh->tx_io->gl_type, 0);
#endif

#if !defined(GPAC_USE_GLES2)
	glDisable(txh->tx_io->gl_type);
#endif

	if (txh->transparent)
		glDisable(GL_BLEND);

	gf_sc_texture_check_pause_on_first_load(txh, GF_FALSE);
	txh->compositor->visual->bound_tx_pix_fmt = 0;
}


Bool tx_can_use_rect_ext(GF_Compositor *compositor, GF_TextureHandler *txh)
{
#ifndef GPAC_DISABLE_VRML
	u32 i, j, count;
#endif

//	compositor->gl_caps.yuv_texture = 0;
	if (!compositor->gl_caps.rect_texture) return 0;
	if (compositor->rext) return 1;
	/*this happens ONLY with text texturing*/
	if (!txh->owner) return 0;

#ifndef GPAC_DISABLE_VRML
	count = gf_node_get_parent_count(txh->owner);

	/*background 2D can use RECT ext without pb*/
	if (gf_node_get_tag(txh->owner)==TAG_MPEG4_Background2D) return 1;
	/*if a bitmap is using the texture force using RECT*/
	for (i=0; i<count; i++) {
		GF_Node *n = gf_node_get_parent(txh->owner, i);
		if (gf_node_get_tag(n)==TAG_MPEG4_Appearance) {
			count = gf_node_get_parent_count(n);
			for (j=0; j<count; j++) {
				M_Shape *s = (M_Shape *) gf_node_get_parent(n, j);
				if (s->geometry && (gf_node_get_tag((GF_Node *)s)==TAG_MPEG4_Shape) && (gf_node_get_tag(s->geometry)==TAG_MPEG4_Bitmap)) return 1;
			}
		}
	}
#endif /*GPAC_DISABLE_VRML*/
	return 0;
}


static Bool tx_setup_format(GF_TextureHandler *txh)
{
	u32 npow_w, npow_h;
	Bool is_pow2, use_rect, flip;
	GF_Compositor *compositor = (GF_Compositor *)txh->compositor;

	/*first setup, this will force recompute bounds in case used with bitmap - we could refine and only
	invalidate for bitmaps only*/
	if (txh->owner)
		gf_node_dirty_set(txh->owner, 0, 1);

	npow_w = gf_get_next_pow2(txh->width);
	npow_h = gf_get_next_pow2(txh->height);

	flip = (txh->tx_io->flags & TX_IS_FLIPPED);
	is_pow2 = ((npow_w==txh->width) && (npow_h==txh->height)) ? 1 : 0;
	txh->tx_io->flags = 0;
	txh->tx_io->gl_type = GL_TEXTURE_2D;

	/* all textures can be used in GLES2 */
#ifdef GPAC_USE_GLES2
	use_rect = GF_TRUE;
#else

#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
	if (!compositor->shader_mode_disabled || compositor->gl_caps.npot || compositor->gl_caps.has_shaders) {
		use_rect = GF_TRUE;
	} else
#endif
	{
		use_rect = tx_can_use_rect_ext(compositor, txh);

		if (!is_pow2 && use_rect) {
#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
			txh->tx_io->gl_type = GL_TEXTURE_RECTANGLE_EXT;
#endif
			txh->tx_io->flags = TX_IS_RECT;
		}
	}
#endif

	txh->tx_io->gl_format = 0;
	switch (txh->pixelformat) {
	case GF_PIXEL_GREYSCALE:
		txh->tx_io->gl_format = GL_LUMINANCE;
		break;
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_GREYALPHA:
		txh->tx_io->gl_format = GL_LUMINANCE_ALPHA;
		break;
	case GF_PIXEL_RGB:
		txh->tx_io->gl_format = GL_RGB;
		break;
	case GF_PIXEL_BGR:
		txh->tx_io->gl_format = GL_RGB;
		break;
	case GF_PIXEL_BGRX:
		txh->tx_io->gl_format = GL_RGBA;
		break;
	case GF_PIXEL_RGBX:
	case GF_PIXEL_RGBA:
		txh->tx_io->gl_format = GL_RGBA;
		break;
#ifndef GPAC_USE_GLES1X
	case GF_PIXEL_ARGB:
		if (!compositor->gl_caps.bgra_texture) return 0;
		txh->tx_io->gl_format = GL_BGRA_EXT;
		break;
#endif
	case GF_PIXEL_YUV:
	case GF_PIXEL_YVU:
    case GF_PIXEL_YUV_10:
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_YUV444:		
	case GF_PIXEL_YUV444_10:
	case GF_PIXEL_NV21:
	case GF_PIXEL_NV21_10:
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_VYUY:
	case GF_PIXEL_UYVY:
	case GF_PIXEL_GL_EXTERNAL:
	case GF_PIXEL_YUV444_10_PACK:
	case GF_PIXEL_V210:
#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
		if (!compositor->visual->compositor->shader_mode_disabled) {
			break;
		}
#endif
		if (!use_rect && compositor->epow2) txh->tx_io->flags = TX_EMULE_POW2;
		txh->tx_io->gl_format = GL_RGB;
		break;

	//fallthrough
	case GF_PIXEL_YUVA:
	case GF_PIXEL_YUVA444:
	case GF_PIXEL_YUVD:
#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
		if (!compositor->visual->compositor->shader_mode_disabled) {
			break;
		}
#endif
		if (!use_rect && compositor->epow2) txh->tx_io->flags = TX_EMULE_POW2;
		txh->tx_io->gl_format = GL_RGBA;
		break;

	case GF_PIXEL_RGBD:
	case GF_PIXEL_RGB_DEPTH:
		if (!use_rect && compositor->epow2) txh->tx_io->flags = TX_EMULE_POW2;
		txh->tx_io->gl_format = GL_RGB;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[V3D:GLSL] Unknown pixel format %s\n", gf_4cc_to_str(txh->pixelformat) ));
		return 0;
	}

	if (flip) txh->tx_io->flags |= TX_IS_FLIPPED;

	/*in case the ID has been lost, resetup*/
	if (!txh->tx_io->tx.nb_textures) {
		u32 pfmt = txh->pixelformat;
		u32 stride = txh->stride;
		Bool full_range = GF_FALSE;
		s32 cmx = GF_CICP_MX_UNSPECIFIED;
		if (txh->stream && txh->stream->odm && txh->stream->odm->pid) {
			const GF_PropertyValue *p;
			p = gf_filter_pid_get_property(txh->stream->odm->pid, GF_PROP_PID_COLR_RANGE);
			if (p && p->value.boolean) full_range = GF_TRUE;
			p = gf_filter_pid_get_property(txh->stream->odm->pid, GF_PROP_PID_COLR_MX);
			if (p) cmx = p->value.uint;
		}
		
		txh->tx_io->tx.pbo_state = (txh->compositor->gl_caps.pbo && txh->compositor->pbo) ? GF_GL_PBO_BOTH : GF_GL_PBO_NONE;
		if (txh->tx_io->conv_format) {
			stride = txh->tx_io->conv_stride;
			pfmt = txh->tx_io->conv_format;
		}
		gf_gl_txw_setup(&txh->tx_io->tx, pfmt, txh->width, txh->height, stride, 0, GF_TRUE, txh->frame_ifce, full_range, cmx);
	}
	return 1;
}

char *gf_sc_texture_get_data(GF_TextureHandler *txh, u32 *pix_format)
{
	if (txh->tx_io->conv_data) {
		*pix_format = txh->tx_io->conv_format;
		return txh->tx_io->conv_data;
	}
	*pix_format = txh->pixelformat;
	return txh->data;
}

/*note about conversion: we consider that a texture without a stream attached is generated by the compositor
hence is never flipped. Otherwise all textures attached to stream are flipped in order to match uv coords*/
Bool gf_sc_texture_convert(GF_TextureHandler *txh)
{
	GF_VideoSurface src, dst;
	u32 out_stride, i, j, bpp;
	GF_Compositor *compositor = (GF_Compositor *)txh->compositor;

	if (!txh->needs_refresh) return 1;

#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
	if (!txh->compositor->shader_mode_disabled) {
		txh->tx_io->flags |= TX_NEEDS_HW_LOAD;
		return 1;
	}
#endif

	switch (txh->pixelformat) {
	case GF_PIXEL_ARGB:
		if (!compositor->gl_caps.bgra_texture) return 0;
		goto common;

	case GF_PIXEL_RGBD:
		if (txh->compositor->depth_gl_type==GF_SC_DEPTH_GL_NONE) {
			bpp = 4;
			break;
		}
	case GF_PIXEL_BGR:
		bpp = 3;
		break;
	case GF_PIXEL_BGRX:
		bpp = 4;
		break;
	case GF_PIXEL_GREYSCALE:
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_GREYALPHA:
	case GF_PIXEL_RGB:
	case GF_PIXEL_RGBX:
	case GF_PIXEL_RGBA:
common:
		txh->tx_io->conv_format = txh->pixelformat;
		txh->tx_io->flags |= TX_NEEDS_HW_LOAD;

		if (!(txh->tx_io->flags & TX_IS_RECT)) return 1;
		if (txh->flags & GF_SR_TEXTURE_NO_GL_FLIP) return 1;
		//FIXME - we really want to go for shaders on RGB as well to avoid this copy on rect ext ...

		if (!txh->tx_io->conv_data) {
			txh->tx_io->conv_data = gf_malloc(sizeof(char)*txh->stride*txh->height);
			txh->tx_io->conv_format = txh->pixelformat;
		}
		assert(txh->tx_io->conv_data );
		assert(txh->data );
		/*if texture is using RECT extension, flip image manually because
				texture transforms are not supported in this case ...*/
		for (i=0; i<txh->height; i++) {
			memcpy(txh->tx_io->conv_data + (txh->height - 1 - i) * txh->stride, txh->data + i*txh->stride, txh->stride);
		}

		txh->tx_io->flags |= TX_IS_FLIPPED;
		return 1;
	case GF_PIXEL_YUV:
	case GF_PIXEL_YVU:
	case GF_PIXEL_YUV_10:
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_YUV444:		
    case GF_PIXEL_YUV444_10:
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_NV21:
	case GF_PIXEL_NV21_10:
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
		bpp = 3;
		break;
	case GF_PIXEL_YUVD:
	case GF_PIXEL_YUVA:
	case GF_PIXEL_YUVA444:
		bpp = 4;
		break;
	default:
		txh->tx_io->conv_format = 0;
		return 0;
	}
	if (!txh->tx_io->conv_data) {
		if (txh->tx_io->flags & TX_EMULE_POW2) {
			/*convert video to a po of 2 WITHOUT SCALING VIDEO*/
			txh->tx_io->conv_w = gf_get_next_pow2(txh->width);
			txh->tx_io->conv_h = gf_get_next_pow2(txh->height);
			txh->tx_io->conv_data = (char*)gf_malloc(sizeof(char) * bpp * txh->tx_io->conv_w * txh->tx_io->conv_h);
			memset(txh->tx_io->conv_data , 0, sizeof(char) * bpp * txh->tx_io->conv_w * txh->tx_io->conv_h);
			txh->tx_io->conv_wscale = INT2FIX(txh->width) / txh->tx_io->conv_w;
			txh->tx_io->conv_hscale = INT2FIX(txh->height) / txh->tx_io->conv_h;
		} else {
			txh->tx_io->conv_data = (char*)gf_malloc(sizeof(char) * bpp * txh->width * txh->height);
			memset(txh->tx_io->conv_data, 0, sizeof(char) * bpp * txh->width * txh->height);
		}
	}
	out_stride = bpp * ((txh->tx_io->flags & TX_EMULE_POW2) ? txh->tx_io->conv_w : txh->width);
	txh->tx_io->conv_stride = out_stride;


	memset(&src, 0, sizeof(GF_VideoSurface));
	memset(&dst, 0, sizeof(GF_VideoSurface));
	dst.width = src.width = txh->width;
	dst.height = src.height = txh->height;
	dst.is_hardware_memory = src.is_hardware_memory = 0;

	src.pitch_x = 0;
	src.pitch_y = txh->stride;
	src.pixel_format = txh->pixelformat;
	src.video_buffer = txh->data;

	dst.pitch_x = 0;
	dst.pitch_y = out_stride;

	dst.video_buffer = txh->tx_io->conv_data;
	switch (txh->pixelformat) {
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YUV:
	case GF_PIXEL_YVU:
	case GF_PIXEL_YUV_10:
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_YUV444:		
    case GF_PIXEL_YUV444_10:
	case GF_PIXEL_NV21:
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_BGR:
	case GF_PIXEL_BGRX:
		txh->tx_io->conv_format = dst.pixel_format = GF_PIXEL_RGB;
		/*stretch and flip*/
		gf_stretch_bits(&dst, &src, NULL, NULL, 0xFF, !txh->is_flipped, NULL, NULL);
		if ( !txh->is_flipped)
			txh->flags |= GF_SR_TEXTURE_NO_GL_FLIP;
		break;
	case GF_PIXEL_YUVD:
		if (txh->compositor->depth_gl_type==GF_SC_DEPTH_GL_NONE) {
			src.pixel_format = GF_PIXEL_YUV;
			txh->tx_io->conv_format = GF_PIXEL_RGB_DEPTH;
			dst.pixel_format = GF_PIXEL_RGB;
			dst.pitch_y = 3*txh->width;
			/*stretch YUV->RGB*/
			gf_stretch_bits(&dst, &src, NULL, NULL, 0xFF, 1, NULL, NULL);
			/*copy over Depth plane*/
			memcpy(dst.video_buffer + 3*txh->width*txh->height, txh->data + 3*txh->stride*txh->height/2, txh->width*txh->height);
		} else {
			txh->tx_io->conv_format = GF_PIXEL_RGBD;
			dst.pixel_format = GF_PIXEL_RGBD;
			/*stretch*/
			gf_stretch_bits(&dst, &src, NULL, NULL, 0xFF, 0, NULL, NULL);
		}
		txh->flags |= GF_SR_TEXTURE_NO_GL_FLIP;
		break;
	case GF_PIXEL_RGBD:
		if (txh->compositor->depth_gl_type==GF_SC_DEPTH_GL_NONE) {
			dst.pitch_y = 3*txh->width;
			txh->tx_io->conv_format = GF_PIXEL_RGB_DEPTH;
			dst.pixel_format = GF_PIXEL_RGB;

			for (j=0; j<txh->height; j++) {
				u8 *src_p = (u8 *) txh->data + (txh->height-j-1)*txh->stride;
				u8 *dst_p = (u8 *) txh->tx_io->conv_data + j*3*txh->width;
				u8 *dst_d = (u8 *) txh->tx_io->conv_data + txh->height*3*txh->width + j*txh->width;

				for (i=0; i<txh->width; i++) {
					*dst_p++ = src_p[i*4];
					*dst_p++ = src_p[i*4 + 1];
					*dst_p++ = src_p[i*4 + 2];
					*dst_d++ = src_p[i*4 + 3];
				}
			}
			txh->flags |= GF_SR_TEXTURE_NO_GL_FLIP;
		}
		break;
	}
	txh->tx_io->flags |= TX_NEEDS_HW_LOAD;
	return 1;
}

#endif

Bool gf_sc_texture_push_image(GF_TextureHandler *txh, Bool generate_mipmaps, Bool for2d)
{
#ifndef GPAC_DISABLE_3D
	char *data;
	u32 pixel_format;
	u32 push_time;
#endif

	if (for2d) {
		Bool load_tx = 0;
		if (!txh->data) return 0;
		if (!txh->tx_io) {
			GF_SAFEALLOC(txh->tx_io, struct __texture_wrapper);
			if (!txh->tx_io) return 0;
		}
		if (!txh->tx_io->tx_raster) {
			txh->tx_io->tx_raster = gf_evg_stencil_new(GF_STENCIL_TEXTURE);
			if (!txh->tx_io->tx_raster) return 0;
			load_tx = 1;
		}
		if (txh->tx_io->flags & TX_NEEDS_RASTER_LOAD) {
			load_tx = 1;
			txh->tx_io->flags &= ~TX_NEEDS_RASTER_LOAD;
		}
		if (load_tx) {
			const u8 *pData = txh->data;
			const u8 *pU=NULL, *pV=NULL, *pA=NULL;
			u32 stride = txh->stride;
			u32 stride_uv=0;
			u32 stride_alpha=0;
			GF_Err e=GF_OK;
			if (txh->frame_ifce) {
				pData=NULL;
				if (txh->frame_ifce->get_plane) {
					e = txh->frame_ifce->get_plane(txh->frame_ifce, 0, &pData, &stride);
					if (!e && txh->nb_planes>1)
						e = txh->frame_ifce->get_plane(txh->frame_ifce, 1, &pU, &stride_uv);
					if (!e && txh->nb_planes>2)
						e = txh->frame_ifce->get_plane(txh->frame_ifce, 2, &pV, &stride_uv);
					if (!e && txh->nb_planes>3)
						e = txh->frame_ifce->get_plane(txh->frame_ifce, 3, &pA, &stride_alpha);
				}
			}
			if (!pData) {
				if (!txh->compositor->last_error)
					txh->compositor->last_error = GF_NOT_SUPPORTED;
				return 0;
			}

			if (!e)
				e = gf_evg_stencil_set_texture_planes(txh->tx_io->tx_raster, txh->width, txh->height, (GF_PixelFormat) txh->pixelformat, pData, txh->stride, pU, pV, stride_uv, pA, stride_alpha);

			if (e != GF_OK) {
				if (!txh->compositor->last_error)
					txh->compositor->last_error = e;
				return 0;
			}
			txh->compositor->last_error = GF_OK;
		}
		return 1;
	}

#ifndef GPAC_DISABLE_3D

	if (! (txh->tx_io->flags & TX_NEEDS_HW_LOAD) ) return 1;

	if (txh->tx_io->fbo_id) {
		return 1;
	}
	GL_CHECK_ERR()

	if (txh->data) {
		//reconfig from GL textures to non-GL output
		if (!txh->frame_ifce && !txh->tx_io->tx.internal_textures) {
			gf_gl_txw_reset(&txh->tx_io->tx);
		}
		//reconfig from non-GL output to GL textures
		else if (txh->tx_io->tx.internal_textures
			&& txh->frame_ifce
			&& txh->frame_ifce->get_gl_texture
		) {
			gf_gl_txw_reset(&txh->tx_io->tx);
		}
	}
	if (txh->data) {
		/*convert image*/
		gf_sc_texture_convert(txh);
	}

	txh->tx_io->tx.frame_ifce = txh->frame_ifce;

	/*in case the ID has been lost, resetup*/
	if (!txh->tx_io->tx.nb_textures) {
		/*force setup of image*/
		txh->needs_refresh = 1;
		tx_setup_format(txh);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Texturing] Allocating OpenGL texture\n"));
	}
	if (!txh->tx_io->gl_type)
		return 0;

	GL_CHECK_ERR()
	tx_bind(txh);
	GL_CHECK_ERR()

	txh->tx_io->flags &= ~TX_NEEDS_HW_LOAD;
	data = gf_sc_texture_get_data(txh, &pixel_format);
	if (!data) return 0;

	push_time = gf_sys_clock();

	gf_sc_texture_check_pause_on_first_load(txh, GF_TRUE);

#ifdef GPAC_USE_TINYGL
	glTexImage2D(txh->tx_io->gl_type, 0, txh->tx_io->gl_format, w, h, 0, txh->tx_io->gl_format, GL_UNSIGNED_BYTE, (unsigned char *) data);

#else

	if (txh->frame_ifce && txh->frame_ifce->get_gl_texture ) {
		if (txh->tx_io->tx.internal_textures) {
			glDeleteTextures(txh->tx_io->tx.nb_textures, txh->tx_io->tx.textures);
			txh->tx_io->tx.internal_textures = GF_FALSE;
		}
	}
	if (txh->tx_io->tx.pbo_state == GF_GL_PBO_PUSH)
		txh->tx_io->tx.pbo_state = GF_GL_PBO_TEXIMG;

	GL_CHECK_ERR()
	gf_gl_txw_upload(&txh->tx_io->tx, data, txh->frame_ifce);
	GL_CHECK_ERR()
	
#endif

	push_time = gf_sys_clock() - push_time;

	txh->nb_frames ++;
	txh->upload_time += push_time;

#ifndef GPAC_DISABLE_LOG
	if (txh->stream) {
		u32 ck;
		gf_mo_get_object_time(txh->stream, &ck);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[GL Texture] Texture (CTS %u) %d ms after due date - Pushed %s in %d ms - average push time %d ms (PBO enabled %s)\n", txh->last_frame_time, ck - txh->last_frame_time, txh->tx_io->tx.is_yuv ? "YUV textures" : "texture", push_time, txh->upload_time / txh->nb_frames, txh->tx_io->tx.pbo_state ? "yes" : "no"));
	}
#endif
	return 1;

#endif
	return 0;
}


#ifndef GPAC_DISABLE_3D

u32 gf_sc_texture_get_gl_id(GF_TextureHandler *txh)
{
    return (txh->tx_io && txh->tx_io->tx.nb_textures) ? txh->tx_io->tx.textures[0] : 0;
}

#ifndef GPAC_USE_TINYGL
void gf_sc_copy_to_texture(GF_TextureHandler *txh)
{
	/*in case the ID has been lost, resetup*/
	if (!txh->tx_io->tx.nb_textures) {
		tx_setup_format(txh);
	}

	GL_CHECK_ERR()

	tx_bind(txh);

	GL_CHECK_ERR()

#ifdef GPAC_USE_GLES2
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#endif
	GL_CHECK_ERR()

	if (txh->compositor->fbo_id) compositor_3d_enable_fbo(txh->compositor, GF_TRUE);
	glCopyTexImage2D(txh->tx_io->gl_type, 0, txh->tx_io->gl_format, 0, 0, txh->width, txh->height, 0);
	if (txh->compositor->fbo_id) compositor_3d_enable_fbo(txh->compositor, GF_FALSE);

#ifndef GPAC_USE_GLES2
	glDisable(txh->tx_io->gl_type);
#endif
	GL_CHECK_ERR()
}

GF_Err gf_sc_texture_setup_fbo(GF_TextureHandler *txh)
{
	txh->tx_io->gl_type = GL_TEXTURE_2D;
	return compositor_3d_setup_fbo(txh->width, txh->height, &txh->tx_io->fbo_id, &txh->tx_io->fbo_txid, &txh->tx_io->depth_id);
}

void gf_sc_texture_enable_fbo(GF_TextureHandler *txh, Bool enable)
{
#ifndef GPAC_USE_GLES1X
	if (txh->tx_io && txh->tx_io->fbo_id)
	glBindFramebuffer(GL_FRAMEBUFFER, enable ? txh->tx_io->fbo_id : 0);
	GL_CHECK_ERR()
	if (!enable)
		glBindTexture(GL_TEXTURE_2D, 0);
#endif
}


#endif

#ifndef GPAC_USE_TINYGL

void gf_sc_copy_to_stencil(GF_TextureHandler *txh)
{
	u32 i, hy;
	char *tmp=NULL;

	/*in case the ID has been lost, resetup*/
	if (!txh->data || !txh->tx_io->tx_raster)
		return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[GL Texture] Copying GL backbuffer %dx%d@PF=%s to systems memory\n", txh->width, txh->height, gf_4cc_to_str(txh->pixelformat) ));

	if (txh->compositor->fbo_id) compositor_3d_enable_fbo(txh->compositor, GF_TRUE);

	if (txh->pixelformat==GF_PIXEL_RGBA) {
		glReadPixels(0, 0, txh->width, txh->height, GL_RGBA, GL_UNSIGNED_BYTE, txh->data);
	} else if (txh->pixelformat==GF_PIXEL_RGB) {
		glReadPixels(0, 0, txh->width, txh->height, GL_RGB, GL_UNSIGNED_BYTE, txh->data);
	}
#ifdef GF_SR_USE_DEPTH
	else if (txh->pixelformat==GF_PIXEL_RGBDS) {
		/*we'll work with one alpha bit (=shape). we'll take the heaviest weighted as this threshold*/
		glReadPixels(0, 0, txh->width, txh->height, GL_RGBA, GL_UNSIGNED_BYTE, txh->data);

		/*NOTES on OpenGL's z-buffer perspective inversion:
		 * option 1: extract float depth buffer, undoing depth perspective transform PIXEL per PIXEL and then
		 * convert to byte (computationally costly)
		 *
		 * option 2: use gain and offset to make up an approximation of the linear z-buffer (the original)
		 * it can be achieved by scaling the interval where the inflection point is located
		 * i.e. z' = G*z - (G - 1), the offset so that z still belongs to [0..1]*
		 */

		//glPixelTransferf(GL_DEPTH_SCALE, txh->compositor->OGLDepthGain);
		//glPixelTransferf(GL_DEPTH_BIAS, txh->compositor->OGLDepthOffset);

#ifndef GPAC_USE_GLES1X
		/*obtain depthmap*/
		if (!txh->tx_io->depth_data) txh->tx_io->depth_data = (char*)gf_malloc(sizeof(char)*txh->width*txh->height);
		glReadPixels(0, 0, txh->width, txh->height, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, txh->tx_io->depth_data);
		/*	depth = alpha & 0xfe
		    shape = plan alpha & 0x01 */

		/*this corresponds to the RGBDS ordering*/
		for (i=0; i<txh->height*txh->width; i++) {
			u8 alpha;
			//inversion
			u8 ds = (char) (255 - (int)txh->tx_io->depth_data[i]);
			/*get alpha*/
			alpha = (txh->data[i*4 + 3]);

			/* if heaviest-weighted alpha bit is set (>128) , turn on shape bit*/
			//if (ds & 0x80) depth |= 0x01;
			if (alpha & 0x80) ds = (ds >> 1) | 0x80;
			else ds = 0x0;
			txh->data[i*4+3] = ds; /*insert depth onto alpha*/
		}
#endif

	}
#endif /*GF_SR_USE_DEPTH*/

	if (txh->compositor->fbo_id) compositor_3d_enable_fbo(txh->compositor, GF_FALSE);

	/*flip image because of OpenGL*/
	tmp = (char*)gf_malloc(sizeof(char)*txh->stride);
	hy = txh->height/2;
	for (i=0; i<hy; i++) {
		memcpy(tmp, txh->data + i*txh->stride, txh->stride);
		memcpy(txh->data + i*txh->stride, txh->data + (txh->height - 1 - i) * txh->stride, txh->stride);
		memcpy(txh->data + (txh->height - 1 - i) * txh->stride, tmp, txh->stride);
	}
	gf_free(tmp);
	//dump depth and rgbds texture

}
#else

void gf_get_tinygl_depth(GF_TextureHandler *txh)
{
	glReadPixels(0, 0, txh->width, txh->height, GL_RGBDS, GL_UNSIGNED_BYTE, txh->data);
}
#endif

#endif

Bool gf_sc_texture_get_transform(GF_TextureHandler *txh, GF_Node *tx_transform, GF_Matrix *mx, Bool for_picking)
{
#ifndef GPAC_DISABLE_3D
	u32 nb_views=1;
#endif
	Bool ret = 0;
	gf_mx_init(*mx);

#ifndef GPAC_DISABLE_3D

	if (txh->stream && txh->stream->c_w && txh->stream->c_h) {
		Float c_x, c_y;
		//clean aperture center in pixel coords
		c_x = txh->width / 2 + txh->stream->c_x;
		c_y = txh->height / 2 + txh->stream->c_y;
		//left/top of clean aperture zone, in pixel coordinates
		c_x -= txh->stream->c_w / 2;
		c_y -= txh->stream->c_h / 2;

		gf_mx_add_translation(mx, c_x / txh->width, c_y / txh->height, 0);
		gf_mx_add_scale(mx, txh->stream->c_w / txh->width, txh->stream->c_h / txh->height, 1);
		ret = 1;
	}

	if (!txh->compositor->dbgpack)
		gf_mo_get_nb_views(txh->stream, &nb_views);

#ifdef GPAC_CONFIG_ANDROID
	if (txh->stream && txh->tx_io->gl_type == GL_TEXTURE_EXTERNAL_OES) {
		gf_mx_copy(*mx, txh->tx_io->texcoordmatrix);
		ret = 1;
	}
#endif // GPAC_CONFIG_ANDROID
	if (nb_views>1 && !txh->frame_ifce) {
		if (txh->compositor->visual->current_view%2 != 0 && !txh->compositor->multiview_mode){
			gf_mx_add_translation(mx, 0, 0.5f, 0);
		}
		gf_mx_add_scale(mx, FIX_ONE, 0.5f, FIX_ONE);
		ret = 1;
	}

	if (txh->stream && (txh->compositor->fpack==GF_3D_STEREO_TOP)) {
		if ((txh->compositor->visual->current_view % 2 != 0) && !txh->compositor->multiview_mode) {
			gf_mx_add_translation(mx, 0, 0.5f, 0);
		}
		gf_mx_add_scale(mx, FIX_ONE, 0.5f, FIX_ONE);
		ret = 1;
	}
#endif

	/*flip image if requested*/
	if (! (txh->flags & GF_SR_TEXTURE_NO_GL_FLIP) && !(txh->tx_io->flags & TX_IS_FLIPPED) && !for_picking) {
		/*flip it*/
		gf_mx_add_scale(mx, FIX_ONE, -FIX_ONE, FIX_ONE);
		/*and translate it to handle repeatS/repeatT*/
		gf_mx_add_translation(mx, 0, -FIX_ONE, 0);
		ret = 1;
	}

	/*WATCHOUT: GL_TEXTURE_RECTANGLE coords are w, h not 1.0, 1.0 (but not with shaders, we do the txcoord conversion in the fragment shader*/
	if (txh->tx_io->flags & TX_IS_RECT) {
#ifndef GPAC_DISABLE_3D
		if (!for_picking && !txh->tx_io->tx.is_yuv) {
			gf_mx_add_scale(mx, INT2FIX(txh->width), INT2FIX(txh->height), FIX_ONE);
			ret = 1;
		}
#endif
	}
	else if (txh->tx_io->flags & TX_EMULE_POW2) {
#ifndef GPAC_DISABLE_3D
		gf_mx_add_scale(mx, txh->tx_io->conv_wscale, txh->tx_io->conv_hscale, FIX_ONE);
		/*disable any texture transforms when emulating pow2 textures*/
		tx_transform = NULL;
		ret = 1;
#endif
	}

#ifndef GPAC_DISABLE_X3D
	if (tx_transform) {
		GF_Matrix tmp;
		switch (gf_node_get_tag(tx_transform)) {
		case TAG_MPEG4_TextureTransform:
		case TAG_X3D_TextureTransform:
		{
			GF_Matrix2D mat;
			M_TextureTransform *tt = (M_TextureTransform *)tx_transform;
			gf_mx2d_init(mat);

			/*cf VRML spec:  Tc' = -C \D7 S \D7 R \D7 C \D7 T \D7 Tc*/
			gf_mx2d_add_translation(&mat, -tt->center.x, -tt->center.y);
			gf_mx2d_add_scale(&mat, tt->scale.x, tt->scale.y);
			if (fabs(tt->rotation) > FIX_EPSILON) gf_mx2d_add_rotation(&mat, tt->center.x, tt->center.y, tt->rotation);
			gf_mx2d_add_translation(&mat, tt->center.x, tt->center.y);

			gf_mx2d_add_translation(&mat, tt->translation.x, tt->translation.y);

			if (ret) {
				gf_mx_from_mx2d(&tmp, &mat);
				gf_mx_add_matrix(mx, &tmp);
			} else {
				gf_mx_from_mx2d(mx, &mat);
			}
			ret = 1;
		}
		break;
		case TAG_MPEG4_TransformMatrix2D:
		{
			M_TransformMatrix2D *tm = (M_TransformMatrix2D *)tx_transform;
			memset(tmp.m, 0, sizeof(Fixed)*16);
			tmp.m[0] = tm->mxx;
			tmp.m[4] = tm->mxy; /*0*/ tmp.m[12] = tm->tx;
			tmp.m[1] = tm->myx;
			tmp.m[5] = tm->myy; /*0*/ tmp.m[13] = tm->ty;
			/*rest is all 0 excep both diag*/
			tmp.m[10] = tmp.m[15] = FIX_ONE;

			if (ret) {
				gf_mx_add_matrix(mx, &tmp);
			} else {
				gf_mx_copy(*mx, tmp);
			}
			ret = 1;
		}
		break;
		default:
			break;
		}
	}
#endif /*GPAC_DISABLE_X3D*/

	return ret;
}

Bool gf_sc_texture_is_transparent(GF_TextureHandler *txh)
{
#ifdef GPAC_DISABLE_VRML
	return txh->transparent;
#else
	M_MatteTexture *matte;
	if (!txh->matteTexture) return txh->transparent;
	matte = (M_MatteTexture *)txh->matteTexture;
	if (!matte->operation.buffer) return txh->transparent;
	if (matte->alphaSurface) return 1;
	if (!strcmp(matte->operation.buffer, "COLOR_MATRIX")) return 1;
	return txh->transparent;
#endif
}

#ifndef GPAC_DISABLE_3D

u32 gf_sc_texture_enable_ex(GF_TextureHandler *txh, GF_Node *tx_transform, GF_Rect *bounds)
{
	GF_Matrix mx;
	Bool res;
#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
	GF_GLProgInstance *prog;
#endif
	GF_VisualManager *root_visual;

	if (!txh || !txh->tx_io) return 0;
	root_visual = (GF_VisualManager *) txh->compositor->visual;

	if (txh->stream && !txh->stream->pck) {
		if (txh->tx_io->tx.nb_textures) {
			txh->tx_io->tx.frame_ifce = NULL;
			goto skip_push;
		}
		return 0;
	}
	if (root_visual->has_material_2d) {	// mat2d (hence no lights)
		root_visual->active_glsl_flags &= ~GF_GL_HAS_LIGHT;
	}

#ifndef GPAC_DISABLE_VRML
	if (txh->matteTexture) {
		//not supported yet
		return 0;
	}
#endif

	if (txh->compute_gradient_matrix && gf_sc_texture_needs_reload(txh) ) {
		compositor_gradient_update(txh);
	}
	if (!txh->pixelformat)
		return 0;

	if (!txh->stream || txh->data || txh->frame_ifce) {
		gf_rmt_begin_gl(gf_sc_texture_push_image);
		glGetError();
		res = gf_sc_texture_push_image(txh, 0, 0);
		gf_rmt_end_gl();
		glGetError();
		if (!res) return 0;
	}

skip_push:

	gf_rmt_begin_gl(gf_sc_texture_enable);
	glGetError();

	if (bounds && txh->compute_gradient_matrix) {
		GF_Matrix2D mx2d;
		txh->compute_gradient_matrix(txh, bounds, &mx2d, 1);
		gf_mx_from_mx2d(&mx, &mx2d);
		visual_3d_set_texture_matrix(root_visual, &mx);
	}
	else if (gf_sc_texture_get_transform(txh, tx_transform, &mx, 0)) {
		visual_3d_set_texture_matrix(root_visual, &mx);
	} else {
		visual_3d_set_texture_matrix(root_visual, NULL);
	}

	txh->flags |= GF_SR_TEXTURE_USED;
	root_visual->active_glsl_flags |= GF_GL_HAS_TEXTURE;
	root_visual->bound_tx_pix_fmt = txh->pixelformat;

#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
	prog = visual_3d_check_program_exists(root_visual, root_visual->active_glsl_flags, txh->pixelformat);

	if (prog) {
		glUseProgram(prog->prog);
		GL_CHECK_ERR()
		tx_bind_with_mode(txh, txh->transparent, txh->tx_io->blend_mode, 0, prog->prog);
	} else
#endif
	{
		tx_bind(txh);
	}

	gf_rmt_end_gl();
	glGetError();
	return 1;

}

u32 gf_sc_texture_enable(GF_TextureHandler *txh, GF_Node *tx_transform)
{
	return gf_sc_texture_enable_ex(txh, tx_transform, NULL);
}


#endif

Bool gf_sc_texture_needs_reload(GF_TextureHandler *txh)
{
	return (txh->tx_io->flags & TX_NEEDS_HW_LOAD) ? 1 : 0;
}


GF_EVGStencil * gf_sc_texture_get_stencil(GF_TextureHandler *txh)
{
	return txh->tx_io->tx_raster;
}

void gf_sc_texture_set_stencil(GF_TextureHandler *txh, GF_EVGStencil * stencil)
{
	txh->tx_io->tx_raster = stencil;
	txh->tx_io->flags |= TX_NEEDS_HW_LOAD;
}

void gf_sc_texture_check_pause_on_first_load(GF_TextureHandler *txh, Bool do_freeze)
{
	if (!txh->stream || !txh->tx_io || !txh->stream->odm)
		return;
	if (txh->stream->odm->flags & GF_ODM_IS_SPARSE)
		return;

	if (do_freeze) {
		if (! (txh->tx_io->flags & TX_FIRST_UPLOAD_FREEZE) ) {
			txh->tx_io->flags |= TX_FIRST_UPLOAD_FREEZE;
			gf_sc_ar_control(txh->compositor->audio_renderer, GF_SC_AR_PAUSE);
		}
	} else {
		if (!(txh->tx_io->flags & TX_FIRST_UPLOAD_FREEZE_DONE)) {
			txh->tx_io->flags |= TX_FIRST_UPLOAD_FREEZE_DONE;
			gf_sc_ar_control(txh->compositor->audio_renderer, GF_SC_AR_RESUME);
		}
	}
}

