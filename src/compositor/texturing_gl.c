/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#ifdef GPAC_USE_TINYGL
# define GLTEXENV	glTexEnvi
# define GLTEXPARAM	glTexParameteri
# define TexEnvType u32
#elif defined (GPAC_USE_GLES1X)
# define GLTEXENV	glTexEnvx
# define GLTEXPARAM	glTexParameterx
# define TexEnvType Fixed
#else
# define GLTEXENV	glTexEnvf
# define GLTEXPARAM	glTexParameteri
# define TexEnvType Float
#endif


/*tx flags*/
enum
{
	/*signal video data must be sent to 2D raster*/
	TX_NEEDS_RASTER_LOAD = (1<<1),
	/*signal video data must be sent to 3D hw*/
	TX_NEEDS_HW_LOAD = (1<<2),

	/*OpenGL texturing flags*/

	/*these 4 are exclusives*/
	TX_MUST_SCALE = (1<<10),
	TX_IS_POW2 = (1<<11),
	TX_IS_RECT = (1<<12),
	TX_EMULE_POW2 = (1<<13),
	TX_EMULE_FIRST_LOAD = (1<<14),

	TX_IS_FLIPPED = (1<<15),
};


struct __texture_wrapper
{
	u32 flags;

	/*2D texturing*/
	GF_EVGStencil * tx_raster;
	//0: not paused, 1: paused, 2: initial pause has been done
	u32 init_pause_status;
	Bool conv_to_8bit;
	u8 *conv_data;
	GF_Matrix texcoordmatrix;

	/*3D texturing*/
#ifndef GPAC_DISABLE_3D
	/*opengl texture id*/
	u32 id;
	u32 blend_mode;
	u32 rescale_width, rescale_height;
	char *scale_data;
	Fixed conv_wscale, conv_hscale;
	u32 conv_format, conv_w, conv_h;

	Bool use_external_textures;
	
	/*gl textures vars (gl_type: 2D texture or rectangle (NV ext) )*/
	u32 nb_comp, gl_format, gl_type, gl_dtype;
	Bool yuv_shader;
	u32 v_id, u_id;
	u32 pbo_id, u_pbo_id, v_pbo_id;
	Bool pbo_pushed;
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
	if (txh->compositor->out8b) {

		if (txh->pixelformat == GF_PIXEL_YUV_10) {
			txh->stride /= 2;
			txh->tx_io->conv_to_8bit = GF_TRUE;
			txh->pixelformat = GF_PIXEL_YUV;
			if(txh->frame_ifce)
				txh->tx_io->conv_data = (char*)gf_realloc(txh->tx_io->conv_data, 3 * sizeof(char)* txh->stride * txh->height / 2);
		}
		else if (txh->pixelformat == GF_PIXEL_NV12_10) {
			txh->stride /= 2;
			txh->tx_io->conv_to_8bit = GF_TRUE;
			txh->pixelformat = GF_PIXEL_NV12;
			if(txh->frame_ifce)
				txh->tx_io->conv_data = (char*)gf_realloc(txh->tx_io->conv_data, 3 * sizeof(char)* txh->stride * txh->height / 2);
		}
		else if (txh->pixelformat == GF_PIXEL_YUV422_10) {
			txh->stride /= 2;
			txh->tx_io->conv_to_8bit = GF_TRUE;
			txh->pixelformat = GF_PIXEL_YUV422;
			
			if (txh->frame_ifce)
				txh->tx_io->conv_data = (char*)gf_realloc(txh->tx_io->conv_data, 2 * sizeof(char)* txh->stride * txh->height);
		}
		else if (txh->pixelformat == GF_PIXEL_YUV444_10) {
			txh->stride /= 2;
			txh->tx_io->conv_to_8bit = GF_TRUE;
			txh->pixelformat = GF_PIXEL_YUV444;
			if (txh->frame_ifce)
				txh->tx_io->conv_data = (char*)gf_realloc(txh->tx_io->conv_data, 3 * sizeof(char)* txh->stride * txh->height);
		}
	}
	return GF_OK;
}



static void release_txio(struct __texture_wrapper *tx_io)
{

#ifndef GPAC_DISABLE_3D
	if (!tx_io->use_external_textures) {
		if (tx_io->id) glDeleteTextures(1, &tx_io->id);
		if (tx_io->u_id) glDeleteTextures(1, &tx_io->u_id);
		if (tx_io->v_id) glDeleteTextures(1, &tx_io->v_id);
	}
	if (tx_io->pbo_id) glDeleteBuffers(1, &tx_io->pbo_id);
	if (tx_io->u_pbo_id) glDeleteBuffers(1, &tx_io->u_pbo_id);
	if (tx_io->v_pbo_id) glDeleteBuffers(1, &tx_io->v_pbo_id);

	if (tx_io->scale_data) gf_free(tx_io->scale_data);
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
	u32 y_stride = txh->stride;
#endif
	u8  *p_y, *p_u, *p_v;
	txh->tx_io->flags |= TX_NEEDS_RASTER_LOAD | TX_NEEDS_HW_LOAD;

	p_y = txh->data;
	p_u = p_v = NULL;
	//10->8 bit conversion
	if (txh->tx_io->conv_to_8bit) {
		GF_VideoSurface dst;
		u32 src_stride = txh->stride * 2;
		memset(&dst, 0, sizeof(GF_VideoSurface));
		dst.width = txh->width;
		dst.height = txh->height;
		dst.pitch_y = txh->stride;
		dst.video_buffer = txh->frame_ifce ? txh->tx_io->conv_data : txh->data;

		p_u = p_v = NULL;
		p_y = (u8 *)txh->data;
		if (txh->frame_ifce) {
			u32 st_y, st_u, st_v;
			txh->frame_ifce->get_plane(txh->frame_ifce, 0, (const u8 **) &p_y, &st_y);
			txh->frame_ifce->get_plane(txh->frame_ifce, 1, (const u8 **) &p_u, &st_u);
			txh->frame_ifce->get_plane(txh->frame_ifce, 2, (const u8 **) &p_v, &st_v);
			if (p_y) src_stride = st_y;
		}

		if (txh->pixelformat == GF_PIXEL_YUV) {

			gf_color_write_yv12_10_to_yuv(&dst, p_y, p_u, p_v, src_stride, txh->width, txh->height, NULL, GF_FALSE);

			if (txh->frame_ifce) {
				p_y = dst.video_buffer;
				p_u = (u8*) dst.video_buffer + dst.pitch_y * txh->height;
				p_v = (u8*) dst.video_buffer + 5 * dst.pitch_y * txh->height / 4;
			}
		}
		else if (txh->pixelformat == GF_PIXEL_YUV422) {

			gf_color_write_yuv422_10_to_yuv422(&dst, p_y, p_u, p_v, src_stride, txh->width, txh->height, NULL, GF_FALSE);

			if (txh->frame_ifce) {
				p_y = dst.video_buffer;
				p_u = (u8*) dst.video_buffer + dst.pitch_y * txh->height;
				p_v = (u8*) dst.video_buffer + 3 * dst.pitch_y * txh->height / 2;
			}

		}
		else if (txh->pixelformat == GF_PIXEL_YUV444) {

			gf_color_write_yuv444_10_to_yuv444(&dst, p_y, p_u, p_v, src_stride, txh->width, txh->height, NULL, GF_FALSE);

			if (txh->frame_ifce) {
				p_y = dst.video_buffer;
				p_u = (u8*) dst.video_buffer + dst.pitch_y * txh->height;
				p_v = (u8*) dst.video_buffer + 2 * dst.pitch_y * txh->height;
			}
		}
	}
#ifndef GPAC_DISABLE_3D
	else if (txh->tx_io->pbo_id && txh->frame_ifce) {
		u32 src_stride;
		txh->frame_ifce->get_plane(txh->frame_ifce, 0, (const u8 **) &p_y, &y_stride);
		txh->frame_ifce->get_plane(txh->frame_ifce, 1, (const u8 **) &p_u, &src_stride);
		txh->frame_ifce->get_plane(txh->frame_ifce, 2, (const u8 **) &p_v, &src_stride);
	}
#endif


#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
	//PBO mode: start pushing the texture
	if (p_y && txh->tx_io->pbo_id) {
		u8 *ptr;
		u32 size = y_stride * txh->height;

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, txh->tx_io->pbo_id);
		ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		if (ptr) memcpy(ptr, p_y, size);
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

		if (txh->tx_io->u_pbo_id) {
			if (!p_u) p_u = (u8 *) p_v + size;
			if (!p_v) p_v = (u8 *) p_v + 5*size/4;

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, txh->tx_io->u_pbo_id);
			ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
			if (ptr) memcpy(ptr, p_u, size/4);
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, txh->tx_io->v_pbo_id);
			ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
			if (ptr) memcpy(ptr, p_v, size/4);
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		}

		txh->tx_io->pbo_pushed = 1;

		//we just pushed our texture to the GPU, release
		if (txh->frame_ifce) {
			gf_sc_texture_release_stream(txh);
		}
	}
	return GF_OK;

	//We do not have PBOs in ES2.0
#elif !defined(GPAC_DISABLE_3D) && defined(GPAC_USE_GLES2)
	//PBO mode: start pushing the texture
	if (txh->tx_io->pbo_id) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[V3D:GLSL] PBOs are not implemented in GL ES 2.0\n"));
	}
	return GF_NOT_SUPPORTED;
#else
	return GF_NOT_SUPPORTED;
#endif
}

void gf_sc_texture_reset(GF_TextureHandler *txh)
{
#ifndef GPAC_DISABLE_3D
	if (txh->tx_io->id) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Texturing] Releasing OpenGL texture %d\n", txh->tx_io->id));

		if (txh->tx_io->use_external_textures) {
			glDeleteTextures(1, &txh->tx_io->id);
			if (txh->tx_io->u_id) {
				glDeleteTextures(1, &txh->tx_io->u_id);
				glDeleteTextures(1, &txh->tx_io->v_id);
			}
		}
		txh->tx_io->id = txh->tx_io->u_id = txh->tx_io->v_id = 0;
		
		if (txh->tx_io->pbo_id) glDeleteBuffers(1, &txh->tx_io->pbo_id);
		if (txh->tx_io->u_pbo_id) glDeleteBuffers(1, &txh->tx_io->u_pbo_id);
		if (txh->tx_io->v_pbo_id) glDeleteBuffers(1, &txh->tx_io->v_pbo_id);
		txh->tx_io->pbo_id = txh->tx_io->u_pbo_id = txh->tx_io->v_pbo_id = 0;
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

void tx_bind_with_mode(GF_TextureHandler *txh, Bool transparent, u32 blend_mode, Bool no_bind)
{
	if (!txh->tx_io || !txh->tx_io->id || !txh->tx_io->gl_type) return;

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

	if (!no_bind)
		glBindTexture(txh->tx_io->gl_type, txh->tx_io->id);
}

void tx_bind(GF_TextureHandler *txh)
{
	if (txh->tx_io )
		tx_bind_with_mode(txh, txh->transparent, txh->tx_io->blend_mode, 0);
}

void gf_sc_texture_disable(GF_TextureHandler *txh)
{
	if (txh && txh->tx_io) {

#ifndef GPAC_USE_GLES1X
		if (txh->tx_io->yuv_shader) {
//			glUseProgram(0);
			txh->compositor->visual->current_texture_glsl_program = 0;
//			glActiveTexture(GL_TEXTURE0);
			glBindTexture(txh->tx_io->gl_type, 0);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[GL Texture] Texture drawn (CTS %u)\n", txh->last_frame_time));

		}
#endif
#if !defined(GPAC_USE_GLES2)
		glDisable(txh->tx_io->gl_type);
#endif
		if (txh->transparent) glDisable(GL_BLEND);

		gf_sc_texture_check_pause_on_first_load(txh);
		txh->compositor->visual->glsl_flags &= ~(GF_GL_HAS_TEXTURE | GF_GL_IS_YUV | GF_GL_IS_ExternalOES);
	}
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
	GLint tx_id[3];
	u32 i, nb_tx = 1;
	Bool is_pow2, use_rect, flip, use_yuv_shaders;
	GF_Compositor *compositor = (GF_Compositor *)txh->compositor;

	/*first setup, this will force recompute bounds in case used with bitmap - we could refine and only
	invalidate for bitmaps only*/
	if (txh->owner && (!txh->tx_io->rescale_width || !txh->tx_io->rescale_height))
		gf_node_dirty_set(txh->owner, 0, 1);

	txh->tx_io->rescale_width = gf_get_next_pow2(txh->width);
	txh->tx_io->rescale_height = gf_get_next_pow2(txh->height);

	flip = (txh->tx_io->flags & TX_IS_FLIPPED);
	is_pow2 = ((txh->tx_io->rescale_width==txh->width) && (txh->tx_io->rescale_height==txh->height)) ? 1 : 0;
	txh->tx_io->flags = TX_IS_POW2;
	txh->tx_io->gl_type = GL_TEXTURE_2D;

	/* all textures can be used in GLES2 */
#ifdef GPAC_USE_GLES2
	use_rect = GF_TRUE;
#else

#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
	if (compositor->shader_only_mode) {
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
		if (!use_rect && !compositor->gl_caps.npot_texture && !is_pow2) txh->tx_io->flags = TX_MUST_SCALE;
	}
#endif

	use_yuv_shaders = 0;
	txh->tx_io->nb_comp = txh->tx_io->gl_format = 0;
	txh->tx_io->gl_dtype = GL_UNSIGNED_BYTE;
	switch (txh->pixelformat) {
	case GF_PIXEL_GREYSCALE:
		txh->tx_io->gl_format = GL_LUMINANCE;
		txh->tx_io->nb_comp = 1;
		txh->tx_io->gl_type = GL_TEXTURE_2D;
		if (!is_pow2) txh->tx_io->flags = TX_MUST_SCALE;
		break;
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_GREYALPHA:
		txh->tx_io->gl_format = GL_LUMINANCE_ALPHA;
		txh->tx_io->nb_comp = 2;
		txh->tx_io->gl_type = GL_TEXTURE_2D;
		if (!is_pow2) txh->tx_io->flags = TX_MUST_SCALE;
		break;
	case GF_PIXEL_RGB:
		txh->tx_io->gl_format = GL_RGB;
		txh->tx_io->nb_comp = 3;
		break;
	case GF_PIXEL_BGR:
		txh->tx_io->gl_format = GL_RGB;
		txh->tx_io->nb_comp = 3;
		break;
	case GF_PIXEL_BGRX:
		txh->tx_io->gl_format = GL_RGBA;
		txh->tx_io->nb_comp = 4;
		break;
	case GF_PIXEL_RGBX:
	case GF_PIXEL_RGBA:
		txh->tx_io->gl_format = GL_RGBA;
		txh->tx_io->nb_comp = 4;
		break;
#ifndef GPAC_USE_GLES1X
	case GF_PIXEL_ARGB:
		if (!compositor->gl_caps.bgra_texture) return 0;
		txh->tx_io->gl_format = GL_BGRA_EXT;
		txh->tx_io->nb_comp = 4;
		break;
#endif
	case GF_PIXEL_YUV:
    case GF_PIXEL_YUV_10:
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_YUV444:		
	case GF_PIXEL_YUV444_10:
	case GF_PIXEL_NV21:
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV12_10:
#ifndef GPAC_USE_GLES1X
		if (compositor->gl_caps.has_shaders && (is_pow2 || compositor->visual->compositor->shader_only_mode) ) {
			use_yuv_shaders = 1;
			break;
		}
#ifndef GPAC_USE_GLES2
		else if (compositor->yuvgl && compositor->gl_caps.yuv_texture && !(txh->tx_io->flags & TX_MUST_SCALE) ) {
			txh->tx_io->gl_format = compositor->gl_caps.yuv_texture;
			txh->tx_io->nb_comp = 3;
			txh->tx_io->gl_dtype = UNSIGNED_SHORT_8_8_MESA;
			break;
		}
#endif
#endif

	//fallthrough
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YUVD:
#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
		if (compositor->gl_caps.has_shaders && (is_pow2 || compositor->visual->compositor->shader_only_mode) ) {
			use_yuv_shaders = 1;
		} else
#endif
		{
			if (!use_rect && compositor->epow2) txh->tx_io->flags = TX_EMULE_POW2;
			txh->tx_io->gl_format = GL_RGB;
			txh->tx_io->nb_comp = 3;
		}
		break;

	case GF_PIXEL_RGBD:
	case GF_PIXEL_RGB_DEPTH:
		if (!use_rect && compositor->epow2) txh->tx_io->flags = TX_EMULE_POW2;
		txh->tx_io->gl_format = GL_RGB;
		txh->tx_io->nb_comp = 3;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[V3D:GLSL] Unknown pixel format %s\n", gf_4cc_to_str(txh->pixelformat) ));
		return 0;
	}

	if (flip) txh->tx_io->flags |= TX_IS_FLIPPED;

	/*in case the ID has been lost, resetup*/
	if (!txh->tx_io->id) {
		glGenTextures(1, &txh->tx_io->id);
		txh->tx_io->flags |= TX_EMULE_FIRST_LOAD;
		txh->tx_io->u_id = 0;
		txh->tx_io->v_id = 0;
	}
	tx_id[0] = txh->tx_io->id;

#ifndef GPAC_USE_GLES1X
	if (use_yuv_shaders && !txh->tx_io->u_id) {
		glGenTextures(1, &txh->tx_io->u_id);
		glGenTextures(1, &txh->tx_io->v_id);
		tx_id[1] = txh->tx_io->u_id;
		tx_id[2] = txh->tx_io->v_id;
		nb_tx = 3;
	}
#endif

#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
	if (txh->compositor->gl_caps.pbo && txh->compositor->pbo) {
		u32 size = txh->stride*txh->height;

		if (!txh->tx_io->pbo_id && txh->tx_io->id) {
			glGenBuffers(1, &txh->tx_io->pbo_id);
			if (txh->tx_io->pbo_id) {
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, txh->tx_io->pbo_id);
				glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, size, NULL, GL_DYNAMIC_DRAW_ARB);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
			}
		}
		if (!txh->tx_io->u_pbo_id && txh->tx_io->u_id) {
			glGenBuffers(1, &txh->tx_io->u_pbo_id);
			if (txh->tx_io->u_pbo_id) {
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, txh->tx_io->u_pbo_id);
				glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, size/4, NULL, GL_DYNAMIC_DRAW_ARB);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
			}
		}
		if (!txh->tx_io->v_pbo_id && txh->tx_io->v_id) {
			glGenBuffers(1, &txh->tx_io->v_pbo_id);
			if (txh->tx_io->v_pbo_id) {
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, txh->tx_io->v_pbo_id);
				glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, size/4, NULL, GL_DYNAMIC_DRAW_ARB);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
			}
		}
	}
#endif

	if (use_yuv_shaders) {
		//we use LUMINANCE because GL_RED is not defined on android ...
		txh->tx_io->gl_format = GL_LUMINANCE;
		txh->tx_io->nb_comp = 1;
		txh->tx_io->yuv_shader = 1;
		switch (txh->pixelformat) {
		case GF_PIXEL_YUV_10:
		case GF_PIXEL_NV12_10:
		case GF_PIXEL_YUV422_10:
		case GF_PIXEL_YUV444_10:
			txh->tx_io->gl_dtype = GL_UNSIGNED_SHORT;
			break;
		}
		txh->compositor->visual->yuv_pixelformat_type = txh->pixelformat;
	}

	/*note we don't free the data if existing, since this only happen when re-setting up after context loss (same size)*/
	if ((txh->tx_io->flags == TX_MUST_SCALE) & !txh->tx_io->scale_data) {
		txh->tx_io->scale_data = (char*)gf_malloc(sizeof(char) * txh->tx_io->nb_comp*txh->tx_io->rescale_width*txh->tx_io->rescale_height);
		memset(txh->tx_io->scale_data , 0, sizeof(char) * txh->tx_io->nb_comp*txh->tx_io->rescale_width*txh->tx_io->rescale_height);
	}

	//setup all textures
	for (i=0; i<nb_tx; i++) {
#if !defined(GPAC_USE_GLES2)
		glEnable(txh->tx_io->gl_type);
#endif
		glBindTexture(txh->tx_io->gl_type, tx_id[i] );

#if defined(GPAC_USE_GLES1X) || defined(GPAC_USE_GLES2)

#ifdef GPAC_USE_GLES2
		if (!is_pow2) {
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		} else
#endif
		{
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_S, (txh->flags & GF_SR_TEXTURE_REPEAT_S) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_T, (txh->flags & GF_SR_TEXTURE_REPEAT_T) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
		}

		if (is_pow2 && txh->tx_io->gl_type == GL_TEXTURE_2D) {
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MAG_FILTER, txh->compositor->fast ? GL_NEAREST : GL_LINEAR);
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MIN_FILTER, txh->compositor->fast ? GL_NEAREST : GL_LINEAR);
		} else {
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
#else

#ifndef GPAC_USE_TINYGL
		if (txh->tx_io->gl_type == GL_TEXTURE_2D) {
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_S, (txh->flags & GF_SR_TEXTURE_REPEAT_S) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_T, (txh->flags & GF_SR_TEXTURE_REPEAT_T) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
		} else
#endif
			//clamp to edge for NPOT textures
		{
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}

		if (txh->tx_io->gl_type == GL_TEXTURE_2D) {
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MAG_FILTER, txh->compositor->fast ? GL_NEAREST : GL_LINEAR);
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MIN_FILTER, txh->compositor->fast ? GL_NEAREST : GL_LINEAR);
		} else {
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
#endif

		if (txh->tx_io->yuv_shader && ((txh->pixelformat==GF_PIXEL_YUV_10) || (txh->pixelformat==GF_PIXEL_NV12_10) || (txh->pixelformat==GF_PIXEL_YUV422_10) || (txh->pixelformat==GF_PIXEL_YUV444_10)) ) {
			//will never happen on GLES for now since we don't have GLES2 support yet ...
			//FIXME - allow 10bit support in GLES2
#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
			glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
			//we use 10 bits but GL will normalise using 16 bits, so we need to multiply the normalized result by 2^6
			glPixelTransferi(GL_RED_SCALE, 64);
#endif
		} else {
#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
			glPixelTransferi(GL_RED_SCALE, 1);
#endif
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		}
#if !defined(GPAC_USE_GLES2)
		glDisable(txh->tx_io->gl_type);
#endif
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

void txh_unpack_yuv(GF_TextureHandler *txh)
{
	u32 i, j;
	u8 *dst, *y, *u, *v, *p_y, *p_u, *p_v;
	if (!txh->tx_io->conv_data) {
		txh->tx_io->conv_data = (char*)gf_malloc(sizeof(char) * 2 * txh->width * txh->height);
	}
	p_y = (u8 *) txh->data;
	p_u = (u8 *) txh->data + txh->stride*txh->height;
	p_v = (u8 *) txh->data + 5*txh->stride*txh->height/4;

	/*convert to UYVY and flip texture*/
	for (i=0; i<txh->height; i++) {
		u32 idx = txh->height-i-1;
		y = p_y + idx*txh->stride;
		u = p_u + (idx/2) * txh->stride/2;
		v = p_v + (idx/2) * txh->stride/2;
		dst = (u8 *) txh->tx_io->conv_data + 2*i*txh->stride;

		for (j=0; j<txh->width/2; j++) {
			*dst = *u;
			dst++;
			u++;
			*dst = *y;
			dst++;
			y++;
			*dst = *v;
			dst++;
			v++;
			*dst = *y;
			dst++;
			y++;
		}
	}
	/*remove GL texture flip*/
	txh->tx_io->flags |= TX_IS_FLIPPED;
}

/*note about conversion: we consider that a texture without a stream attached is generated by the compositor
hence is never flipped. Otherwise all textures attached to stream are flipped in order to match uv coords*/
Bool gf_sc_texture_convert(GF_TextureHandler *txh)
{
	GF_VideoSurface src, dst;
	u32 out_stride, i, j, bpp;
	GF_Compositor *compositor = (GF_Compositor *)txh->compositor;

	if (!txh->needs_refresh) return 1;

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
	case GF_PIXEL_YUV_10:
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_YUV444:		
    case GF_PIXEL_YUV444_10:
	case GF_PIXEL_NV21:
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV12_10:
		if (txh->tx_io->gl_format == compositor->gl_caps.yuv_texture) {
			txh->tx_io->conv_format = GF_PIXEL_YVYU;
			txh->tx_io->flags |= TX_NEEDS_HW_LOAD;
			txh_unpack_yuv(txh);
			return 1;
		}
		if (txh->tx_io->yuv_shader) {
			txh->tx_io->flags |= TX_NEEDS_HW_LOAD;
			return 1;
		}
	case GF_PIXEL_YUYV:
		bpp = 3;
		break;
	case GF_PIXEL_YUVD:
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
				u8 *src = (u8 *) txh->data + (txh->height-j-1)*txh->stride;
				u8 *dst_p = (u8 *) txh->tx_io->conv_data + j*3*txh->width;
				u8 *dst_d = (u8 *) txh->tx_io->conv_data + txh->height*3*txh->width + j*txh->width;

				for (i=0; i<txh->width; i++) {
					*dst_p++ = src[i*4];
					*dst_p++ = src[i*4 + 1];
					*dst_p++ = src[i*4 + 2];
					*dst_d++ = src[i*4 + 3];
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


#ifndef GPAC_DISABLE_3D
static void do_tex_image_2d(GF_TextureHandler *txh, GLint tx_mode, Bool first_load, u8 *data, u32 stride, u32 w, u32 h, u32 pbo_id)
{
	Bool needs_stride;
	GL_CHECK_ERR

	if (txh->tx_io->gl_dtype==GL_UNSIGNED_SHORT) {
		needs_stride = (stride != 2*w*txh->tx_io->nb_comp) ? GF_TRUE : GF_FALSE;
		if (needs_stride) stride /= 2;
	} else {
		if (tx_mode==GL_LUMINANCE_ALPHA) stride /= 2;
		needs_stride = (stride!=w*txh->tx_io->nb_comp) ? GF_TRUE : GF_FALSE;
	}

#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
	if (needs_stride)
		glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
#else
	if (needs_stride) {
		if (txh->compositor->gl_caps.gles2_unpack) {
			glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[V3D:GLSL] Texture with stride - OpenGL ES2.0 extension \"EXT_unpack_subimage\" is required\n"));
		}
	}
#endif

#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
	if (txh->tx_io->pbo_pushed) {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo_id);
		glTexImage2D(txh->tx_io->gl_type, 0, tx_mode, w, h, 0, txh->tx_io->gl_format, txh->tx_io->gl_dtype, NULL);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	}
	else
#elif defined(GPAC_USE_GLES2)
	if (txh->tx_io->pbo_pushed) {
		glTexImage2D(txh->tx_io->gl_type, 0, tx_mode, w, h, 0, txh->tx_io->gl_format, txh->tx_io->gl_dtype, NULL);
	} else
#endif
		if (first_load) {
			glTexImage2D(txh->tx_io->gl_type, 0, tx_mode, w, h, 0, txh->tx_io->gl_format, txh->tx_io->gl_dtype, data);
		} else {
			glTexSubImage2D(txh->tx_io->gl_type, 0, 0, 0, w, h, txh->tx_io->gl_format, txh->tx_io->gl_dtype, data);
		}

#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
	if (needs_stride)
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	return;
#else


	if (!needs_stride)
		return;

	//no GL_UNPACK_ROW_LENGTH on GLES, push line by line ...
	if (first_load) {
		glTexImage2D(txh->tx_io->gl_type, 0, tx_mode, w, h, 0, txh->tx_io->gl_format, txh->tx_io->gl_dtype, NULL);
	}
	{
		u32 i;
		for (i=0; i<h; i++) {
			u8 *ptr = data + i*stride;
			glTexSubImage2D(txh->tx_io->gl_type, 0, 0, 0, w, 1, txh->tx_io->gl_format, txh->tx_io->gl_dtype, ptr);
		}
	}
#endif
}

#endif

Bool gf_sc_texture_push_image(GF_TextureHandler *txh, Bool generate_mipmaps, Bool for2d)
{
#ifndef GPAC_DISABLE_3D
	char *data;
	Bool first_load = 0;
	GLint tx_mode;
	u32 pixel_format, w, h;
	int nb_views = 1, nb_layers = 1, nb_frames = 1;
	u32 push_time;

	if (txh->stream) {
		gf_mo_get_nb_views(txh->stream, &nb_views);
		gf_mo_get_nb_layers(txh->stream, &nb_layers);
	}
	if (txh->frame_ifce || nb_views == 1) nb_frames = 1;
	else if (nb_layers) nb_frames = nb_layers;

#endif


	if (for2d) {
		Bool load_tx = 0;
		if (!txh->data) return 0;
		if (!txh->tx_io) {
			GF_SAFEALLOC(txh->tx_io, struct __texture_wrapper);
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
			GF_Err e;
			if (txh->frame_ifce) {
				pData=NULL;
				if (txh->frame_ifce->get_plane) {
					e = txh->frame_ifce->get_plane(txh->frame_ifce, 0, &pData, &stride);
					if (!e && txh->nb_planes>1)
						e = txh->frame_ifce->get_plane(txh->frame_ifce, 1, &pU, &stride_uv);
					if (!e && txh->nb_planes>2)
						e = txh->frame_ifce->get_plane(txh->frame_ifce, 2, &pV, &stride_uv);
					if (!e && txh->nb_planes>3)
						e = txh->frame_ifce->get_plane(txh->frame_ifce, 3, &pA, &stride_uv);
				}
			}
			if (!pData) {
				if (!txh->compositor->last_error)
					txh->compositor->last_error = GF_NOT_SUPPORTED;
				return 0;
			}

			e = gf_evg_stencil_set_texture_planes(txh->tx_io->tx_raster, txh->width, txh->height, (GF_PixelFormat) txh->pixelformat, pData, txh->stride, pU, pV, stride_uv, pA);

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

	GL_CHECK_ERR

	/*in case the ID has been lost, resetup*/
	if (!txh->tx_io->id && !txh->tx_io->use_external_textures) {
		glGenTextures(1, &txh->tx_io->id);
		/*force setup of image*/
		txh->needs_refresh = 1;
		tx_setup_format(txh);
		txh->tx_io->flags |= TX_EMULE_FIRST_LOAD;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Texturing] Allocating OpenGL texture %d\n", txh->tx_io->id));
	}
	if (!txh->tx_io->gl_type) return 0;

	/*if data not yet ready don't push the texture*/
	if (txh->data) {
		/*convert image*/
		gf_sc_texture_convert(txh);
	}

	tx_bind(txh);

	txh->tx_io->flags &= ~TX_NEEDS_HW_LOAD;
	data = gf_sc_texture_get_data(txh, &pixel_format);
	if (!data) return 0;

	if (txh->tx_io->flags & TX_EMULE_FIRST_LOAD) {
		txh->tx_io->flags &= ~TX_EMULE_FIRST_LOAD;
		first_load = 1;
	}

	if (txh->tx_io->flags & TX_EMULE_POW2) {
		w = txh->tx_io->conv_w;
		h = txh->tx_io->conv_h;
	} else {
		w = txh->width;
		h = txh->height * nb_frames;
	}
#if defined(GPAC_USE_GLES1X) || defined(GPAC_USE_GLES2)
	tx_mode = txh->tx_io->gl_format;
#else
	tx_mode = txh->tx_io->nb_comp;
	if (txh->tx_io->conv_format==GF_PIXEL_YVYU) {
		tx_mode = txh->tx_io->gl_format;
	}
#endif


	push_time = gf_sys_clock();


#ifdef GPAC_USE_TINYGL
	glTexImage2D(txh->tx_io->gl_type, 0, tx_mode, w, h, 0, txh->tx_io->gl_format, txh->tx_io->gl_dtype, (unsigned char *) data);

#else

	gf_sc_texture_check_pause_on_first_load(txh);

	/*pow2 texture or hardware support*/
	if (! (txh->tx_io->flags & TX_MUST_SCALE) ) {
		if (txh->tx_io->yuv_shader) {
			u32 stride_luma = txh->stride;
			u32 stride_chroma = txh->stride_chroma;
			u8 *pY, *pU, *pV;
			
			if (txh->frame_ifce && txh->frame_ifce->get_gl_texture ) {
				u32 gl_format;
				
				if (!txh->tx_io->use_external_textures) {
					glDeleteTextures(1, &txh->tx_io->id);
					glDeleteTextures(1, &txh->tx_io->u_id);
					glDeleteTextures(1, &txh->tx_io->v_id);
					txh->tx_io->id = txh->tx_io->u_id = txh->tx_io->v_id = 0;
					txh->tx_io->use_external_textures = GF_TRUE;
				}
					
				if (txh->frame_ifce->get_gl_texture(txh->frame_ifce, 0, &gl_format, &txh->tx_io->id, &txh->tx_io->texcoordmatrix) != GF_OK) {
					return 0;
				}
				glBindTexture(gl_format, txh->tx_io->id);
				GLTEXPARAM(gl_format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				GLTEXPARAM(gl_format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				GLTEXPARAM(gl_format, GL_TEXTURE_MAG_FILTER, txh->compositor->fast ? GL_NEAREST : GL_LINEAR);
				GLTEXPARAM(gl_format, GL_TEXTURE_MIN_FILTER, txh->compositor->fast ? GL_NEAREST : GL_LINEAR);

#ifdef GPAC_CONFIG_ANDROID
				if ( gl_format == GL_TEXTURE_EXTERNAL_OES) {
					txh->tx_io->flags |= TX_IS_FLIPPED;
					txh->tx_io->gl_type = GL_TEXTURE_EXTERNAL_OES;
					goto push_exit;
				}
#endif // GPAC_CONFIG_ANDROID
					
				if (txh->frame_ifce->get_gl_texture(txh->frame_ifce, 1, &gl_format, &txh->tx_io->u_id, &txh->tx_io->texcoordmatrix) != GF_OK) {
					return 0;
				}

				glBindTexture(gl_format, txh->tx_io->u_id);
				GLTEXPARAM(gl_format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				GLTEXPARAM(gl_format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				GLTEXPARAM(gl_format, GL_TEXTURE_MAG_FILTER, txh->compositor->fast ? GL_NEAREST : GL_LINEAR);
				GLTEXPARAM(gl_format, GL_TEXTURE_MIN_FILTER, txh->compositor->fast ? GL_NEAREST : GL_LINEAR);

				goto push_exit;
			}
			
			
			pY = (u8 *) data;
			pU = pV = NULL;

			if (txh->frame_ifce) {
				GF_Err e;
				e = txh->frame_ifce->get_plane(txh->frame_ifce, 0, (const u8 **) &pY, &stride_luma);
				if (e) goto push_exit;
				e = txh->frame_ifce->get_plane(txh->frame_ifce, 1, (const u8 **) &pU, &stride_chroma);
				if (e) goto push_exit;
				e = txh->frame_ifce->get_plane(txh->frame_ifce, 2, (const u8 **) &pV, &stride_chroma);
				if (e) goto push_exit;
			} else {
				pU = (u8 *) pY + nb_frames * txh->height * txh->stride;
			}
			
			switch (txh->pixelformat) {
			case GF_PIXEL_YUV444_10:
			case GF_PIXEL_YUV444:
				if (!stride_chroma)
					stride_chroma = stride_luma;
				if (!pV)
					pV = (u8 *) pU + txh->height * stride_chroma;
				break;
			case GF_PIXEL_YUV422_10:
			case GF_PIXEL_YUV422:
				if (!stride_chroma)
					stride_chroma = stride_luma/2;
				if (!pV)
					pV = (u8 *) pU + txh->height * stride_chroma;
				break;
			case GF_PIXEL_YUV_10:
			case GF_PIXEL_YUV:
				if (!stride_chroma)
					stride_chroma = stride_luma/2;
				if (!pV)
					pV = (u8 *) pU + txh->height * nb_frames  * stride_chroma / 2;
				break;
			case GF_PIXEL_NV21:
			case GF_PIXEL_NV12:
			case GF_PIXEL_NV12_10:
				if (!stride_chroma)
					stride_chroma = stride_luma/2;
				break;
			default:
				if (!stride_chroma)
					stride_chroma = stride_luma/2;
				pV = NULL;
				break;
			}

#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
		
			switch (txh->pixelformat) {
			case GF_PIXEL_YUV_10:
			case GF_PIXEL_YUV422_10:
			case GF_PIXEL_YUV444_10:
			case GF_PIXEL_NV12_10:
				glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
				//we use 10 bits but GL will normalise using 16 bits, so we need to multiply the nomralized result by 2^6
				glPixelTransferi(GL_RED_SCALE, 64);
				break;
			}
#endif

			do_tex_image_2d(txh, tx_mode, first_load, pY, stride_luma, w, h, txh->tx_io->pbo_id);
			GL_CHECK_ERR

			/*
			 * Note: GF_PIXEL_NV21 is default for Android camera review. First wxh bytes is the Y channel,
			 * the following (wxh)/2 bytes is UV plane.
			 * Reference: http://stackoverflow.com/questions/22456884/how-to-render-androids-yuv-nv21-camera-image-on-the-background-in-libgdx-with-o
			 */
			if ((txh->pixelformat == GF_PIXEL_NV21) || (txh->pixelformat == GF_PIXEL_NV12) || (txh->pixelformat == GF_PIXEL_NV12_10)) {
				u32 fmt = txh->tx_io->gl_format;
				txh->tx_io->gl_format = GL_LUMINANCE_ALPHA;
				glBindTexture(txh->tx_io->gl_type, txh->tx_io->u_id);
				do_tex_image_2d(txh, GL_LUMINANCE_ALPHA, first_load, pU, stride_chroma, w/2, h/2, txh->tx_io->u_pbo_id);
				txh->tx_io->gl_format = fmt;
				GL_CHECK_ERR
			} 
			else if (txh->pixelformat == GF_PIXEL_YUV_10 || txh->pixelformat == GF_PIXEL_YUV ) {
				glBindTexture(txh->tx_io->gl_type, txh->tx_io->u_id);
				do_tex_image_2d(txh, tx_mode, first_load, pU, stride_chroma, w/2, h/2, txh->tx_io->u_pbo_id);
				GL_CHECK_ERR

				glBindTexture(txh->tx_io->gl_type, txh->tx_io->v_id);
				do_tex_image_2d(txh, tx_mode, first_load, pV, stride_chroma, w/2, h/2, txh->tx_io->v_pbo_id);
				GL_CHECK_ERR
			}
			else if (txh->pixelformat == GF_PIXEL_YUV422_10 || txh->pixelformat == GF_PIXEL_YUV422) {
				
				glBindTexture(txh->tx_io->gl_type, txh->tx_io->u_id);
				do_tex_image_2d(txh, tx_mode, first_load, pU, stride_chroma, w/2 , h , txh->tx_io->u_pbo_id);
				GL_CHECK_ERR

				glBindTexture(txh->tx_io->gl_type, txh->tx_io->v_id);
				do_tex_image_2d(txh, tx_mode, first_load, pV, stride_chroma, w/2 , h, txh->tx_io->v_pbo_id);
				GL_CHECK_ERR
			}
			else if (txh->pixelformat == GF_PIXEL_YUV444_10 || txh->pixelformat == GF_PIXEL_YUV444) {
				
				glBindTexture(txh->tx_io->gl_type, txh->tx_io->u_id);
		      	do_tex_image_2d(txh, tx_mode, first_load, pU, stride_chroma, w, h, txh->tx_io->u_pbo_id);
				GL_CHECK_ERR
 
				glBindTexture(txh->tx_io->gl_type, txh->tx_io->v_id);
				do_tex_image_2d(txh, tx_mode, first_load, pV, stride_chroma, w, h, txh->tx_io->v_pbo_id);
				GL_CHECK_ERR
			}

#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
			if (txh->pixelformat==GF_PIXEL_YUV_10 || txh->pixelformat==GF_PIXEL_YUV444_10 || txh->pixelformat==GF_PIXEL_YUV422_10 ) {
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glPixelTransferi(GL_RED_SCALE, 1);
			}
#endif

			txh->tx_io->pbo_pushed = 0;
		} else if (txh->frame_ifce) {
			if (txh->frame_ifce->get_gl_texture) {
				u32 gl_format;
				txh->frame_ifce->get_gl_texture(txh->frame_ifce, 0, &gl_format, &txh->tx_io->id, &txh->tx_io->texcoordmatrix);
				glBindTexture(gl_format, txh->tx_io->id);
				GL_CHECK_ERR
				goto push_exit;

			} else {
				u32 stride;
				txh->frame_ifce->get_plane(txh->frame_ifce, 0, (const u8 **) &data, &stride);
				do_tex_image_2d(txh, tx_mode, first_load, (u8 *) data, txh->stride, w, h, txh->tx_io->pbo_id);
			}
			txh->tx_io->pbo_pushed = 0;
		} else {
			do_tex_image_2d(txh, tx_mode, first_load, (u8 *) data, txh->stride, w, h, txh->tx_io->pbo_id);
			txh->tx_io->pbo_pushed = 0;
		}
	} else {

#ifdef GPAC_HAS_GLU
		if (txh->compositor->glus) {
			gluScaleImage(txh->tx_io->gl_format, txh->width, txh->height, txh->tx_io->gl_dtype, data, txh->tx_io->rescale_width, txh->tx_io->rescale_height, txh->tx_io->gl_dtype, txh->tx_io->scale_data);
		} else
#endif
		{
			/*it appears gluScaleImage is quite slow - use ourt own resampler which is not as nice but a but faster*/
			GF_VideoSurface src, dst;
			memset(&src, 0, sizeof(GF_VideoSurface));
			src.width = txh->width;
			src.height = txh->height;
			src.pitch_x = 0;
			src.pitch_y = txh->stride;
			src.pixel_format = txh->pixelformat;
			src.video_buffer = txh->data;

			memset(&dst, 0, sizeof(GF_VideoSurface));
			dst.width = txh->tx_io->rescale_width;
			dst.height = txh->tx_io->rescale_height;
			dst.pitch_x = 0;
			dst.pitch_y = txh->tx_io->rescale_width*txh->tx_io->nb_comp;
			dst.pixel_format = txh->pixelformat;
			dst.video_buffer = txh->tx_io->scale_data;

			gf_stretch_bits(&dst, &src, NULL, NULL, 0xFF, 0, NULL, NULL);
		}

		if (first_load) {
			glTexImage2D(txh->tx_io->gl_type, 0, tx_mode, txh->tx_io->rescale_width, txh->tx_io->rescale_height, 0, txh->tx_io->gl_format, txh->tx_io->gl_dtype, txh->tx_io->scale_data);
		} else {
			glTexSubImage2D(txh->tx_io->gl_type, 0, 0, 0, txh->tx_io->rescale_width, txh->tx_io->rescale_height, txh->tx_io->gl_format, txh->tx_io->gl_dtype, txh->tx_io->scale_data);
		}
	}
#endif

push_exit:

	push_time = gf_sys_clock() - push_time;

	txh->nb_frames ++;
	txh->upload_time += push_time;

#ifndef GPAC_DISABLE_LOGS
	if (txh->stream) {
		u32 ck;
		gf_mo_get_object_time(txh->stream, &ck);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[GL Texture] Texture (CTS %u) %d ms after due date - Pushed %s in %d ms - average push time %d ms (PBO enabled %s)\n", txh->last_frame_time, ck - txh->last_frame_time, txh->tx_io->yuv_shader ? "YUV textures" : "texture", push_time, txh->upload_time / txh->nb_frames, txh->tx_io->pbo_pushed ? "yes" : "no"));
	}
#endif
	return 1;

#endif
	return 0;
}


#ifndef GPAC_DISABLE_3D

#if 0 //unused
/*refreshes hardware data for given rect (eg glTexSubImage)*/
void gf_sc_texture_refresh_area(GF_TextureHandler *txh, GF_IRect *rc, void *mem)
{
	if (!txh->tx_io) return;

	if (txh->tx_io->flags & TX_NEEDS_HW_LOAD) {
		gf_sc_texture_push_image(txh, 0, 0);
	}

	glTexSubImage2D(txh->tx_io->gl_type, 0, rc->x, rc->y, rc->width, rc->height, txh->tx_io->gl_format, txh->tx_io->gl_dtype, mem);
}
#endif


static Bool tx_set_image(GF_TextureHandler *txh, Bool generate_mipmaps)
{
	return gf_sc_texture_push_image(txh, generate_mipmaps, 0);
}

u32 gf_sc_texture_get_gl_id(GF_TextureHandler *txh)
{
    return txh->tx_io ? txh->tx_io->id : 0;
}


#ifndef GPAC_USE_TINYGL
void gf_sc_copy_to_texture(GF_TextureHandler *txh)
{
	/*in case the ID has been lost, resetup*/
	if (!txh->tx_io->id) {
		glGenTextures(1, &txh->tx_io->id);
		tx_setup_format(txh);
	}

	GL_CHECK_ERR
	tx_bind(txh);
	GL_CHECK_ERR
#ifdef GPAC_USE_GLES2
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#endif
	GL_CHECK_ERR

	if (txh->compositor->fbo_id) compositor_3d_enable_fbo(txh->compositor, GF_TRUE);
	glCopyTexImage2D(txh->tx_io->gl_type, 0, txh->tx_io->gl_format, 0, 0, txh->width, txh->height, 0);
	if (txh->compositor->fbo_id) compositor_3d_enable_fbo(txh->compositor, GF_FALSE);

#ifndef GPAC_USE_GLES2
	glDisable(txh->tx_io->gl_type);
#endif
	GL_CHECK_ERR
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

	/*flip image because of openGL*/
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
	int nb_views=1;
#endif
	Bool ret = 0;
	gf_mx_init(*mx);

#ifndef GPAC_DISABLE_3D
	gf_mo_get_nb_views(txh->stream, &nb_views);

#ifdef GPAC_CONFIG_ANDROID
	if(txh->stream && txh->tx_io->gl_type == GL_TEXTURE_EXTERNAL_OES) {
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
		if (!for_picking && !txh->tx_io->yuv_shader) {
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
	GF_Compositor *compositor = (GF_Compositor *)txh->compositor;
#endif
	GF_VisualManager *root_visual = (GF_VisualManager *) txh->compositor->visual;

	if (root_visual->has_material_2d) {	// mat2d (hence no lights)
		root_visual->glsl_flags &= ~GF_GL_HAS_LIGHT;
	}

#ifndef GPAC_DISABLE_VRML
	if (txh->matteTexture) {
		//not supported yet
		return 0;

/*
		if (gf_sc_texture_get_transform(txh, tx_transform, &mx, 0))
			visual_3d_set_texture_matrix(root_visual, &mx);
		else
			visual_3d_set_texture_matrix(root_visual, NULL);

		return ret;
*/

	}
#endif
	if (!txh || !txh->tx_io)
		return 0;

	if (txh->compute_gradient_matrix && gf_sc_texture_needs_reload(txh) ) {
		compositor_gradient_update(txh);
	}
	if (!txh->pixelformat)
		return 0;

	gf_rmt_begin_gl(gf_sc_texture_push_image);
	glGetError();
	res = tx_set_image(txh, 0);
	gf_rmt_end_gl();
	glGetError();
	if (!res) return 0;


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
	root_visual->glsl_flags |= GF_GL_HAS_TEXTURE;
	root_visual->glsl_flags &= ~(GF_GL_IS_YUV | GF_GL_IS_ExternalOES);	

#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)

	if (txh->tx_io->yuv_shader) {
		u32 active_shader;	//stores current shader (GLES2.0 or the old stuff)
#ifdef GPAC_CONFIG_ANDROID
		root_visual->glsl_flags |= (txh->tx_io->gl_type == GL_TEXTURE_EXTERNAL_OES ) ? GF_GL_IS_ExternalOES : GF_GL_IS_YUV;
#else
		root_visual->glsl_flags |= GF_GL_IS_YUV;
#endif // GPAC_CONFIG_ANDROID
		active_shader = root_visual->glsl_programs[root_visual->glsl_flags];	//Set active

		GL_CHECK_ERR

		glUseProgram(active_shader);
		GL_CHECK_ERR
		
#ifdef GPAC_CONFIG_ANDROID
		if (txh->tx_io->gl_type != GL_TEXTURE_EXTERNAL_OES)
#endif // GPAC_CONFIG_ANDROID
		{
			if (txh->tx_io->v_id) {
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(txh->tx_io->gl_type, txh->tx_io->v_id);
			}
			if (txh->tx_io->u_id) {
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(txh->tx_io->gl_type, txh->tx_io->u_id);
			}
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(txh->tx_io->gl_type, txh->tx_io->id);

		GL_CHECK_ERR
		tx_bind_with_mode(txh, txh->transparent, txh->tx_io->blend_mode, 1);
#ifndef GPAC_USE_GLES2
		glClientActiveTexture(GL_TEXTURE0);
#endif

	}
	else if (compositor->shader_only_mode) {
		GL_CHECK_ERR
		glUseProgram(root_visual->glsl_programs[root_visual->glsl_flags]);
		GL_CHECK_ERR

		glActiveTexture(GL_TEXTURE0);
		GL_CHECK_ERR
		glBindTexture(txh->tx_io->gl_type, txh->tx_io->id);
		GL_CHECK_ERR

		tx_bind(txh);
	} else
#endif
		tx_bind(txh);


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

void gf_sc_texture_check_pause_on_first_load(GF_TextureHandler *txh)
{
	if (txh->stream && txh->tx_io) {
		switch (txh->tx_io->init_pause_status) {
		case 0:
			gf_sc_ar_control(txh->compositor->audio_renderer, GF_SC_AR_PAUSE);
			txh->tx_io->init_pause_status = 1;
			break;
		case 1:
			gf_sc_ar_control(txh->compositor->audio_renderer, GF_SC_AR_RESUME);
			txh->tx_io->init_pause_status = 2;
			break;
		default:
			break;
		}
	}
}

