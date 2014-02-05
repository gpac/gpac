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
#elif defined (GPAC_USE_OGL_ES)
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
	TX_MUST_SCALE = (1<<3),
	TX_IS_POW2 = (1<<4),
	TX_IS_RECT = (1<<5),
	TX_EMULE_POW2 = (1<<6),
	TX_EMULE_FIRST_LOAD = (1<<7),

	TX_IS_FLIPPED = (1<<8),
};


struct __texture_wrapper
{
	u32 flags;

	/*2D texturing*/
	GF_STENCIL tx_raster;

	/*3D texturing*/
#ifndef GPAC_DISABLE_3D
	/*opengl texture id*/
	u32 id;
	u32 blend_mode;
	u32 rescale_width, rescale_height;
	char *scale_data;
	char *conv_data;
	Fixed conv_wscale, conv_hscale;
	u32 conv_format, conv_w, conv_h;

	/*gl textures vars (gl_type: 2D texture or rectangle (NV ext) )*/
	u32 nb_comp, gl_format, gl_type, gl_dtype;
	Bool yuv_shader;
	u32 v_id, u_id;
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

void gf_sc_texture_release(GF_TextureHandler *txh)
{
	if (txh->vout_udta && txh->compositor->video_out->ReleaseTexture) {
		txh->compositor->video_out->ReleaseTexture(txh->compositor->video_out, txh);
		txh->vout_udta = NULL;
	}

	if (!txh->tx_io) return;
	if (txh->tx_io->tx_raster) {
		txh->compositor->rasterizer->stencil_delete(txh->tx_io->tx_raster);
		txh->tx_io->tx_raster = NULL;
	}

#ifndef GPAC_DISABLE_3D
	if (txh->tx_io->id) glDeleteTextures(1, &txh->tx_io->id);
	if (txh->tx_io->u_id) glDeleteTextures(1, &txh->tx_io->u_id);
	if (txh->tx_io->v_id) glDeleteTextures(1, &txh->tx_io->v_id);
	if (txh->tx_io->scale_data) gf_free(txh->tx_io->scale_data);
	if (txh->tx_io->conv_data) gf_free(txh->tx_io->conv_data);
#endif

#ifdef GF_SR_USE_DEPTH
	if (txh->tx_io->depth_data) gf_free(txh->tx_io->depth_data);
#endif

	gf_free(txh->tx_io);
	txh->tx_io = NULL;
}


GF_Err gf_sc_texture_set_data(GF_TextureHandler *txh)
{
	txh->tx_io->flags |= TX_NEEDS_RASTER_LOAD | TX_NEEDS_HW_LOAD;
	return GF_OK;
}

void gf_sc_texture_reset(GF_TextureHandler *txh)
{
#ifndef GPAC_DISABLE_3D
	if (txh->tx_io->id) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Texturing] Releasing OpenGL texture %d\n", txh->tx_io->id));
		glDeleteTextures(1, &txh->tx_io->id);
		txh->tx_io->id = 0;
		if (txh->tx_io->u_id) {
			glDeleteTextures(1, &txh->tx_io->u_id);
			glDeleteTextures(1, &txh->tx_io->v_id);
			txh->tx_io->u_id = txh->tx_io->v_id = 0;
		}
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
#ifdef GPAC_USE_OGL_ES
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

#ifndef GPAC_USE_OGL_ES 
		if (txh->tx_io->yuv_shader) {
			glUseProgram(0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(txh->tx_io->gl_type, 0);
		}
#endif
		glDisable(txh->tx_io->gl_type);
		if (txh->transparent) glDisable(GL_BLEND);
	}
}


Bool tx_can_use_rect_ext(GF_Compositor *compositor, GF_TextureHandler *txh)
{
#ifndef GPAC_DISABLE_VRML
	u32 i, j, count;
#endif

//	compositor->gl_caps.yuv_texture = 0;
	if (!compositor->gl_caps.rect_texture) return 0;
	if (!compositor->disable_rect_ext) return 1;
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
	use_rect = tx_can_use_rect_ext(compositor, txh);
	if (!is_pow2 && use_rect) {
#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_OGL_ES)
		txh->tx_io->gl_type = GL_TEXTURE_RECTANGLE_EXT;
#endif
		txh->tx_io->flags = TX_IS_RECT;
	}
	if (!use_rect && !compositor->gl_caps.npot_texture && !is_pow2) txh->tx_io->flags = TX_MUST_SCALE;

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
		txh->tx_io->gl_format = GL_LUMINANCE_ALPHA;
		txh->tx_io->nb_comp = 2;
		txh->tx_io->gl_type = GL_TEXTURE_2D;
		if (!is_pow2) txh->tx_io->flags = TX_MUST_SCALE;
		break;
	case GF_PIXEL_RGB_24:
		txh->tx_io->gl_format = GL_RGB;
		txh->tx_io->nb_comp = 3;
		break;
	case GF_PIXEL_BGR_24:
		txh->tx_io->gl_format = GL_RGB;
		txh->tx_io->nb_comp = 3;
		break;
    case GF_PIXEL_BGR_32:
        txh->tx_io->gl_format = GL_RGBA;
        txh->tx_io->nb_comp = 4;
        break;
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_RGBA:
		txh->tx_io->gl_format = GL_RGBA;
		txh->tx_io->nb_comp = 4;
		break;
#ifndef GPAC_USE_OGL_ES 
	case GF_PIXEL_ARGB:
		if (!compositor->gl_caps.bgra_texture) return 0;
		txh->tx_io->gl_format = GL_BGRA_EXT;
		txh->tx_io->nb_comp = 4;
		break;
#endif
	case GF_PIXEL_YV12:
	case GF_PIXEL_YV12_10:
	case GF_PIXEL_NV21:
#ifndef GPAC_USE_OGL_ES 
        if (compositor->gl_caps.has_shaders && (is_pow2 || compositor->visual->yuv_rect_glsl_program) ) {
            use_yuv_shaders = 1;
            break;
        } else if (!compositor->disable_yuvgl && compositor->gl_caps.yuv_texture && !(txh->tx_io->flags & TX_MUST_SCALE) ) {
			txh->tx_io->gl_format = compositor->gl_caps.yuv_texture;
			txh->tx_io->nb_comp = 3;
			txh->tx_io->gl_dtype = UNSIGNED_SHORT_8_8_MESA;
			break;
		}
#endif
		//fallthrough
	case GF_PIXEL_YUY2:
	case GF_PIXEL_YUVD:
		if (compositor->gl_caps.has_shaders && (is_pow2 || compositor->visual->yuv_rect_glsl_program) ) {
			use_yuv_shaders = 1;
		} else {
			if (!use_rect && compositor->emul_pow2) txh->tx_io->flags = TX_EMULE_POW2;
			txh->tx_io->gl_format = GL_RGB;
			txh->tx_io->nb_comp = 3;
		}
		break;

	case GF_PIXEL_RGBD:
	case GF_PIXEL_RGB_24_DEPTH:
		if (!use_rect && compositor->emul_pow2) txh->tx_io->flags = TX_EMULE_POW2;
		txh->tx_io->gl_format = GL_RGB;
		txh->tx_io->nb_comp = 3;
		break;
	default:
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

#ifndef GPAC_USE_OGL_ES
	if (use_yuv_shaders && !txh->tx_io->u_id) {
		glGenTextures(1, &txh->tx_io->u_id);
		glGenTextures(1, &txh->tx_io->v_id);
		tx_id[1] = txh->tx_io->u_id;
		tx_id[2] = txh->tx_io->v_id;
		nb_tx = 3;


		if (txh->tx_io->flags & TX_IS_RECT) {
			GLint loc;
			glUseProgram(compositor->visual->yuv_rect_glsl_program);
			loc = glGetUniformLocation(compositor->visual->yuv_rect_glsl_program, "width");
			if (loc == -1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to locate uniform \"width\" in YUV shader\n"));
			} else {
				GLfloat w = (GLfloat) txh->width;
				glUniform1f(loc, w);
			}
			loc = glGetUniformLocation(compositor->visual->yuv_rect_glsl_program, "height");
			if (loc == -1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to locate uniform \"width\" in YUV shader\n"));
			} else {
				GLfloat h = (GLfloat) txh->height;
				glUniform1f(loc, h);
			}
			glUseProgram(0);
		}
	}
#endif
    
	if (use_yuv_shaders) {
		txh->tx_io->gl_format = GL_RED;
		txh->tx_io->nb_comp = 1;
		txh->tx_io->yuv_shader = 1;
		if (txh->pixelformat==GF_PIXEL_YV12_10) {
			txh->tx_io->gl_dtype = GL_UNSIGNED_SHORT;
		}
	}

	/*note we don't free the data if existing, since this only happen when re-setting up after context loss (same size)*/
	if ((txh->tx_io->flags == TX_MUST_SCALE) & !txh->tx_io->scale_data) {
		txh->tx_io->scale_data = (char*)gf_malloc(sizeof(char) * txh->tx_io->nb_comp*txh->tx_io->rescale_width*txh->tx_io->rescale_height);
		memset(txh->tx_io->scale_data , 0, sizeof(char) * txh->tx_io->nb_comp*txh->tx_io->rescale_width*txh->tx_io->rescale_height);
	}

	//setup all textures
	for (i=0; i<nb_tx; i++) {
		glEnable(txh->tx_io->gl_type);
		glBindTexture(txh->tx_io->gl_type, tx_id[i] );

#ifdef GPAC_USE_OGL_ES
		GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_S, (txh->flags & GF_SR_TEXTURE_REPEAT_S) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
		GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_T, (txh->flags & GF_SR_TEXTURE_REPEAT_T) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
		if (txh->tx_io->gl_type == GL_TEXTURE_2D) {
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MAG_FILTER, txh->compositor->high_speed ? GL_NEAREST : GL_LINEAR);
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MIN_FILTER, txh->compositor->high_speed ? GL_NEAREST : GL_LINEAR);
		} else {
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
#else

#ifndef GPAC_USE_TINYGL
		if (txh->tx_io->gl_type == GL_TEXTURE_2D) { 
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_S, (txh->flags & GF_SR_TEXTURE_REPEAT_S) ? GL_REPEAT : GL_CLAMP);
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_T, (txh->flags & GF_SR_TEXTURE_REPEAT_T) ? GL_REPEAT : GL_CLAMP);
		} else
#endif
		{ 
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_S, GL_CLAMP);
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_T, GL_CLAMP);
		}

		if (txh->tx_io->gl_type == GL_TEXTURE_2D) { 
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MAG_FILTER, txh->compositor->high_speed ? GL_NEAREST : GL_LINEAR);
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MIN_FILTER, txh->compositor->high_speed ? GL_NEAREST : GL_LINEAR);
		} else {
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
#endif

		if (txh->tx_io->yuv_shader && (txh->pixelformat==GF_PIXEL_YV12_10)) {
		    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
			//we use 10 bits but GL will normalise using 16 bits, so we need to multiply the nomralized result by 2^6
			glPixelTransferi(GL_RED_SCALE, 64);
		} else {
		    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		}
		glDisable(txh->tx_io->gl_type);
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

		for (j=0; j<txh->width/2;j++) {
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
		if ((txh->compositor->depth_gl_type==GF_SC_DEPTH_GL_NONE) || (txh->compositor->depth_gl_type==GF_SC_DEPTH_GL_VBO)) {
			bpp = 4;
			break;
		}
	case GF_PIXEL_BGR_24:
		bpp = 3;
		break;
    case GF_PIXEL_BGR_32:
        bpp = 4;
        break;
	case GF_PIXEL_GREYSCALE:
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_RGBA:
common:
		txh->tx_io->conv_format = txh->pixelformat;
		txh->tx_io->flags |= TX_NEEDS_HW_LOAD;

		if (!(txh->tx_io->flags & TX_IS_RECT)) return 1;
		if (txh->flags & GF_SR_TEXTURE_NO_GL_FLIP) return 1;

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
	case GF_PIXEL_YV12:
	case GF_PIXEL_YV12_10:
	case GF_PIXEL_NV21:
	case GF_PIXEL_I420:
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
	case GF_PIXEL_YUY2:
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
	case GF_PIXEL_YUY2:
	case GF_PIXEL_YV12:
	case GF_PIXEL_YV12_10:
	case GF_PIXEL_NV21:
	case GF_PIXEL_I420:
	case GF_PIXEL_BGR_24:
    case GF_PIXEL_BGR_32:
    	txh->tx_io->conv_format = dst.pixel_format = GF_PIXEL_RGB_24;
		/*stretch and flip*/
		gf_stretch_bits(&dst, &src, NULL, NULL, 0xFF, !txh->is_flipped, NULL, NULL);
        if ( !txh->is_flipped)
            txh->flags |= GF_SR_TEXTURE_NO_GL_FLIP;
		break;
    case GF_PIXEL_YUVD:
		if ((txh->compositor->depth_gl_type==GF_SC_DEPTH_GL_NONE) || (txh->compositor->depth_gl_type==GF_SC_DEPTH_GL_VBO)) {
			src.pixel_format = GF_PIXEL_YV12;
			txh->tx_io->conv_format = GF_PIXEL_RGB_24_DEPTH;
			dst.pixel_format = GF_PIXEL_RGB_24;
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
		if ((txh->compositor->depth_gl_type==GF_SC_DEPTH_GL_NONE) || (txh->compositor->depth_gl_type==GF_SC_DEPTH_GL_VBO)) {
			dst.pitch_y = 3*txh->width;
			txh->tx_io->conv_format = GF_PIXEL_RGB_24_DEPTH;
			dst.pixel_format = GF_PIXEL_RGB_24;

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
static void do_tex_image_2d(GF_TextureHandler *txh, GLint tx_mode, Bool first_load, u8 *data, u32 stride, u32 w, u32 h)
{
    Bool needs_stride;
	if (txh->tx_io->gl_dtype==GL_UNSIGNED_SHORT) {
		needs_stride = (stride != 2*w*txh->tx_io->nb_comp) ? GF_TRUE : GF_FALSE; 
	} else {
		needs_stride = (stride!=w*txh->tx_io->nb_comp) ? GF_TRUE : GF_FALSE; 
	}

#if !defined(GPAC_USE_OGL_ES)
    if (needs_stride)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
#else
    u32 i;
    if (needs_stride) {
#endif        

	if (first_load) {
		glTexImage2D(txh->tx_io->gl_type, 0, tx_mode, w, h, 0, txh->tx_io->gl_format, txh->tx_io->gl_dtype, data);
	} else {
		glTexSubImage2D(txh->tx_io->gl_type, 0, 0, 0, w, h, txh->tx_io->gl_format, txh->tx_io->gl_dtype, data);
	}

#if !defined(GPAC_USE_OGL_ES)
    if (needs_stride)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    return;
#else
	}

    if (!needs_stride)
        return;

    //no GL_UNPACK_ROW_LENGTH on GLES, push line by line ...
    if (first_load) {
        glTexImage2D(txh->tx_io->gl_type, 0, tx_mode, w, h, 0, txh->tx_io->gl_format, txh->tx_io->gl_dtype, NULL);
    }
    for (i=0; i<h; i++) {
        u8 *ptr = data + i*stride;
        glTexSubImage2D(txh->tx_io->gl_type, 0, 0, 0, w, 1, txh->tx_io->gl_format, txh->tx_io->gl_dtype, ptr);
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
#endif

	if (for2d) {
		Bool load_tx = 0;
		if (!txh->data) return 0;
		if (!txh->tx_io->tx_raster) {
			txh->tx_io->tx_raster = txh->compositor->rasterizer->stencil_new(txh->compositor->rasterizer, GF_STENCIL_TEXTURE);
			if (!txh->tx_io->tx_raster) return 0;
			load_tx = 1;
		}
		if (txh->tx_io->flags & TX_NEEDS_RASTER_LOAD) {
			load_tx = 1;
			txh->tx_io->flags &= ~TX_NEEDS_RASTER_LOAD;
		}
		if (load_tx) {
			if (txh->compositor->rasterizer->stencil_set_texture(txh->tx_io->tx_raster, txh->data, txh->width, txh->height, txh->stride, (GF_PixelFormat) txh->pixelformat, (GF_PixelFormat) txh->compositor->video_out->pixel_format, 0) != GF_OK)
				return 0;
		}
		return 1;
	}

#ifndef GPAC_DISABLE_3D

	if (! (txh->tx_io->flags & TX_NEEDS_HW_LOAD) ) return 1;

	/*in case the ID has been lost, resetup*/
	if (!txh->tx_io->id) {
		glGenTextures(1, &txh->tx_io->id);
		/*force setup of image*/
		txh->needs_refresh = 1;
		tx_setup_format(txh);
		first_load = 1;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Texturing] Allocating OpenGL texture %d\n", txh->tx_io->id));
	}
	if (!txh->tx_io->gl_type) return 0;
	
	/*if data not yet ready don't push the texture*/
	if (txh->data) {

		if (txh->tx_io->flags & TX_EMULE_FIRST_LOAD) {
			txh->tx_io->flags &= ~TX_EMULE_FIRST_LOAD;
			first_load = 1;
		}

		/*convert image*/
		gf_sc_texture_convert(txh);
	}

	tx_bind(txh);

	txh->tx_io->flags &= ~TX_NEEDS_HW_LOAD;
	data = gf_sc_texture_get_data(txh, &pixel_format);
	if (!data) return 0;
	if (txh->tx_io->flags & TX_EMULE_POW2) {
		w = txh->tx_io->conv_w;
		h = txh->tx_io->conv_h;
	} else {
		w = txh->width;
		h = txh->height;
	}
#ifdef GPAC_USE_OGL_ES
	tx_mode = txh->tx_io->gl_format;
#else
	tx_mode = txh->tx_io->nb_comp;
	if (txh->tx_io->conv_format==GF_PIXEL_YVYU) {
		tx_mode = txh->tx_io->gl_format;
	}
#endif

#ifdef GPAC_USE_TINYGL
	glTexImage2D(txh->tx_io->gl_type, 0, tx_mode, w, h, 0, txh->tx_io->gl_format, txh->tx_io->gl_dtype, (unsigned char *) data);

#else

	/*pow2 texture or hardware support*/
	if (! (txh->tx_io->flags & TX_MUST_SCALE) ) {
		if (txh->tx_io->yuv_shader) {
			u32 push_time;
			u8 *pY, *pU, *pV;
			u32 ck;
			pY = data;
			if (txh->raw_memory) {
				if (!txh->pU || !txh->pV) return 0;

				pU = txh->pU;
				pV = txh->pV;
			} else {
				pU = pY + txh->height*txh->stride;
				pV = pU + txh->height*txh->stride/4;
			}

			push_time = gf_sys_clock();
            
            do_tex_image_2d(txh, tx_mode, first_load, pY, txh->stride, w, h);

			glBindTexture(txh->tx_io->gl_type, txh->tx_io->u_id);
            do_tex_image_2d(txh, tx_mode, first_load, pU, txh->stride/2, w/2, h/2);
			
            glBindTexture(txh->tx_io->gl_type, txh->tx_io->v_id);
            do_tex_image_2d(txh, tx_mode, first_load, pV, txh->stride/2, w/2, h/2);
            
			push_time = gf_sys_clock() - push_time;

			if (txh->nb_frames==100) {
				txh->nb_frames = 0;
				txh->upload_time = 0;
			}
			txh->nb_frames ++;
			txh->upload_time += push_time;

#ifndef GPAC_DISABLE_LOGS
		    gf_mo_get_object_time(txh->stream, &ck);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[GL Texture] Texure (CTS %d) %d ms after due date - Pushed Y,U,V texures in %d ms - average push time %d ms\n", txh->last_frame_time, ck - txh->last_frame_time, push_time, txh->upload_time / txh->nb_frames));
#endif
			//we just pushed our texture to the GPU, release
			if (txh->raw_memory) {
				gf_sc_texture_release_stream(txh);
			}
		} else {
			if (first_load) {
				glTexImage2D(txh->tx_io->gl_type, 0, tx_mode, w, h, 0, txh->tx_io->gl_format, txh->tx_io->gl_dtype, (unsigned char *) data);
			} else {
				glTexSubImage2D(txh->tx_io->gl_type, 0, 0, 0, w, h, txh->tx_io->gl_format, txh->tx_io->gl_dtype, (unsigned char *) data);
			}
		}
	} else {

#ifdef GPAC_HAS_GLU
		if (!txh->compositor->disable_glu_scale) {
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
	return 1;

#endif
	return 0;
}


#ifndef GPAC_DISABLE_3D

static Bool tx_set_image(GF_TextureHandler *txh, Bool generate_mipmaps)
{
	return gf_sc_texture_push_image(txh, generate_mipmaps, 0);
}

#ifndef GPAC_USE_TINYGL
void gf_sc_copy_to_texture(GF_TextureHandler *txh)
{
	/*in case the ID has been lost, resetup*/
	if (!txh->tx_io->id) {
		glGenTextures(1, &txh->tx_io->id);
		tx_setup_format(txh);
	}
	
	tx_bind(txh);
	glCopyTexImage2D(txh->tx_io->gl_type, 0, txh->tx_io->gl_format, 0, 0, txh->width, txh->height, 0);
	glDisable(txh->tx_io->gl_type);
}
#endif

#ifndef GPAC_USE_TINYGL

void gf_sc_copy_to_stencil(GF_TextureHandler *txh)
{
	u32 i, hy;
	char *tmp=NULL;

	/*in case the ID has been lost, resetup*/
	if (!txh->data || !txh->tx_io->tx_raster) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[GL Texture] Copying GL backbuffer %dx%d@PF=%s to systems memory\n", txh->width, txh->height, gf_4cc_to_str(txh->pixelformat) ));

	if (txh->pixelformat==GF_PIXEL_RGBA) {
		glReadPixels(0, 0, txh->width, txh->height, GL_RGBA, GL_UNSIGNED_BYTE, txh->data);
	} else if (txh->pixelformat==GF_PIXEL_RGB_24) {
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
		
#ifndef GPAC_USE_OGL_ES
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
	Bool ret = 0;
	gf_mx_init(*mx);

	/*flip image if requested*/
	if (! (txh->flags & GF_SR_TEXTURE_NO_GL_FLIP) && !(txh->tx_io->flags & TX_IS_FLIPPED) && !for_picking) {
		/*flip it*/
		gf_mx_add_scale(mx, FIX_ONE, -FIX_ONE, FIX_ONE);
		/*and translate it to handle repeatS/repeatT*/
		gf_mx_add_translation(mx, 0, -FIX_ONE, 0);
		ret = 1;
	}

	/*WATCHOUT: GL_TEXTURE_RECTANGLE coords are w, h not 1.0, 1.0*/
	if (txh->tx_io->flags & TX_IS_RECT) {
		if (!for_picking) {
			gf_mx_add_scale(mx, INT2FIX(txh->width), INT2FIX(txh->height), FIX_ONE);
//			gf_mx_add_translation(mx, 0, INT2FIX(txh->height), 0);
		}
		/*disable any texture transforms when using RECT textures (no repeat) ??*/
		/*tx_transform = NULL;*/
		ret = 1;
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
			/*1- translate*/
			gf_mx2d_add_translation(&mat, tt->translation.x, tt->translation.y);
			/*2- rotate*/
			if (fabs(tt->rotation) > FIX_EPSILON) gf_mx2d_add_rotation(&mat, tt->center.x, tt->center.y, tt->rotation);
			/*3- centered-scale*/
			gf_mx2d_add_translation(&mat, -tt->center.x, -tt->center.y);
			gf_mx2d_add_scale(&mat, tt->scale.x, tt->scale.y);
			gf_mx2d_add_translation(&mat, tt->center.x, tt->center.y);
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
			tmp.m[0] = tm->mxx; tmp.m[4] = tm->mxy; /*0*/ tmp.m[12] = tm->tx;
			tmp.m[1] = tm->myx; tmp.m[5] = tm->myy; /*0*/ tmp.m[13] = tm->ty;
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

#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_DISABLE_VRML)

static Bool gf_sc_texture_enable_matte_texture(GF_Node *n)
{
	GF_TextureHandler *b_surf;
#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_OGL_ES)
	GF_TextureHandler *matte_hdl;
	GF_TextureHandler *a_surf;
	GF_TextureHandler *alpha_surf;
	char * action ;
	int tmp;
	u8 texture[4];
	MFFloat coefficients;
#endif
	M_MatteTexture *matte = (M_MatteTexture *)n;
	

	b_surf = gf_sc_texture_get_handler(matte->surfaceB);
	
	if (!b_surf || !b_surf->tx_io) return 0;
	glEnable(GL_BLEND);	
	tx_set_image(b_surf, 0);

#if defined(GPAC_USE_TINYGL) || defined(GPAC_USE_OGL_ES)
	tx_bind(b_surf);
	return 1;
#else

/*To remove: gcc 4.6 introduces this warning*/
#if __GNUC__ == 4 && __GNUC_MINOR__ >= 6
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
#endif
	if (glActiveTexture==NULL) {
		tx_bind(b_surf);
		return 1;
	}
/*To remove: gcc 4.6 introduces this warning*/
#if __GNUC__ == 4 && __GNUC_MINOR__ == 6
#pragma GCC diagnostic pop
#endif
	matte_hdl = gf_node_get_private(n);
	if (!matte_hdl->tx_io) {
		gf_sc_texture_allocate(matte_hdl);
	}
	a_surf = gf_sc_texture_get_handler(matte->surfaceA);
	if (!a_surf->tx_io) a_surf = NULL;
	alpha_surf = gf_sc_texture_get_handler(matte->alphaSurface);
	if (!alpha_surf->tx_io) alpha_surf = NULL;

	action = (matte->operation).buffer;
	glDisable(GL_TEXTURE_2D);
	
	/* SCALE */
	if (! strcmp(action,"SCALE") || !strcmp(action,"BIAS") ) {
		TexEnvType operand;
		coefficients = (matte->parameter);
		if (coefficients.count < 3) {
			tx_bind(b_surf);
			return 1;
		}
		texture[0] = (u8) FIX2INT( 255 * coefficients.vals[0]);
		texture[1] = (u8) FIX2INT( 255 * coefficients.vals[1]);
		texture[2] = (u8) FIX2INT( 255 * coefficients.vals[2]);
		texture[3] = 255;
		if (coefficients.count >= 4) 
			texture[3] = (u8) FIX2INT( 255 * coefficients.vals[3]);

		glActiveTexture(GL_TEXTURE0);
		if (!matte_hdl->tx_io->id) {
			glGenTextures(1, &matte_hdl->tx_io->id);
		}
		glBindTexture( GL_TEXTURE_2D, matte_hdl->tx_io->id);
		glEnable( GL_DEPTH_TEST );
		glEnable( GL_TEXTURE_2D );
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		GLTEXPARAM( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,GL_LINEAR );
		GLTEXPARAM( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_LINEAR );
		GLTEXPARAM( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,GL_REPEAT );
		GLTEXPARAM( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,GL_REPEAT );
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,texture);

		operand = GL_MODULATE;
		if (!strcmp(action,"BIAS")) operand = GL_ADD_SIGNED;

		glActiveTexture(GL_TEXTURE1);
		tx_bind(b_surf);
		GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,		GL_COMBINE);
		GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_RGB,			operand);
		GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_RGB ,			GL_TEXTURE1);
		GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_RGB,		GL_SRC_COLOR);
		GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE1_RGB,			GL_TEXTURE0);
		GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND1_RGB,		GL_SRC_COLOR);
		GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_ALPHA,		GL_REPLACE );
		GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA,		GL_TEXTURE1  );
		GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA,	GL_SRC_ALPHA);
		return 2;
	}
	/* end SCALE */

	/* CROSS_FADE */
	if (! strcmp(action,"CROSS_FADE")) {
		tmp = FIX2INT(255 * matte->fraction);
		texture[0] = (unsigned char) tmp;  
		texture[1] = (unsigned char) tmp;
		texture[2] = (unsigned char) tmp;
		texture[3] = (unsigned char) 255;					// donne l'alpha de l'image de sortie
		
		glActiveTexture(GL_TEXTURE0);
		if (!matte_hdl->tx_io->id) {
			glGenTextures(1, &matte_hdl->tx_io->id);
		}
		glBindTexture( GL_TEXTURE_2D, matte_hdl->tx_io->id);
		glEnable( GL_TEXTURE_2D );
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		GLTEXPARAM( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,GL_LINEAR );
		GLTEXPARAM( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_LINEAR );
		GLTEXPARAM( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,GL_REPEAT );
		GLTEXPARAM( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,GL_REPEAT );
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,texture);
		
		/* fin de la g\E9n\E9ration de la texture donn\E9e par la fraction ! */
		
		/* m\E9lange effectif des textures ! } */
		glActiveTexture(GL_TEXTURE1);
		tx_bind(b_surf);

		if (a_surf) {
			glActiveTexture(GL_TEXTURE2);
			tx_set_image(a_surf, 0);
			tx_bind(a_surf);
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE );
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE1  );
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE2  );
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_TEXTURE0  );
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
		}	
		return 3;
	}	
	/* end CROSS_FADE */

	/*REVEAL */
	if (!strcmp(action,"REVEAL")) {
		glActiveTexture(GL_TEXTURE0);
		tx_bind(b_surf);

		if (alpha_surf) {
			tx_set_image(alpha_surf, 0);
			glActiveTexture(GL_TEXTURE1);
			tx_bind(alpha_surf);
		}
		if (a_surf) {
			glActiveTexture(GL_TEXTURE2);
			tx_set_image(a_surf, 0);
			tx_bind(a_surf);
			
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_RGB , GL_TEXTURE0);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE2);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_TEXTURE1);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
			
		}
		return 3;
	}
	/* end REVEAL */

	/*INVERT */
	if (! strcmp(action,"INVERT")) {
		glActiveTexture(GL_TEXTURE0);
		tx_bind(b_surf);
		GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
		GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
		GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_ONE_MINUS_SRC_COLOR);

		if (matte->parameter.count && matte->parameter.vals[0]) {
			GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE );
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE0);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA,	GL_ONE_MINUS_SRC_ALPHA);
		}
		return 1;
	}
	/* end INVERT */

	/* op\E9ration REPLACE_ALPHA */
	if (!strcmp(action,"REPLACE_ALPHA")) {
		glActiveTexture(GL_TEXTURE0);
		tx_bind(b_surf);
		
		if (alpha_surf) {
			glEnable(GL_BLEND);
			glActiveTexture(GL_TEXTURE1);
			tx_set_image(alpha_surf, 0);
			tx_bind(alpha_surf);
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		}
		return 2;
	}
	/* end REPLACE_ALPHA */

	/* MULTIPLY_ALPHA */
	if (!strcmp(action,"MULTIPLY_ALPHA")) {
		glActiveTexture(GL_TEXTURE0);
		tx_bind(b_surf);
		GLTEXENV(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);

		if (alpha_surf) {
			glActiveTexture(GL_TEXTURE1);
			tx_set_image(alpha_surf, 0);
			tx_bind(alpha_surf);
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE1);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE0);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		}
		return 2;
	}
	/* end MULTIPLY_ALPHA */

	/*ADD */
	if (! strcmp(action,"ADD")) {
		glActiveTexture(GL_TEXTURE0);
		tx_bind(b_surf);
		if (a_surf) {
			glActiveTexture(GL_TEXTURE1);
			tx_set_image(a_surf, 0);
			tx_bind(a_surf);
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD_SIGNED);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE1);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE0);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA,	GL_SRC_ALPHA);
		}
		return 2;
	}
	/*end ADD*/

	/* ADD_SIGNED*/
	if (! strcmp(action,"ADD_SIGNED")) {
		glActiveTexture(GL_TEXTURE0);
		tx_bind(b_surf);
		if (a_surf) {
			glActiveTexture(GL_TEXTURE1);
			tx_set_image(a_surf, 0);
			tx_bind(a_surf);
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE1);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE0);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		}
		return 2;
	}
	/* end ADD_SIGNED*/

	/*SUBSTRACT*/
	if (! strcmp(action,"SUBSTRACT")) {
		glActiveTexture(GL_TEXTURE0);
		tx_bind(b_surf);
		if (a_surf) {
			glActiveTexture(GL_TEXTURE1);
			tx_set_image(a_surf, 0);
			tx_bind(a_surf);
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_SUBTRACT);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE1);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE0);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		}
		return 2;
	}
	/* end SUBTRACT*/

	/*BLEND*/
	if (! strcmp(action,"BLEND")) {
		glActiveTexture(GL_TEXTURE0);
		tx_bind(b_surf);
		if (a_surf) {
			GLTEXENV(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
			glActiveTexture(GL_TEXTURE1);
			tx_set_image(a_surf, 0);
			tx_bind(a_surf);
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			GLTEXENV(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE1);
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
		}
		return 2;
	}
	/*end BLEND */

	tx_bind(b_surf);
	return 1;

#endif /*GPAC_USE_TINYGL*/

#undef GLTEXPARAM
}
#endif /* !defined(GPAC_DISABLE_3D) && !defined(GPAC_DISABLE_VRML) */


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
	GF_Compositor *compositor = (GF_Compositor *)txh->compositor;

#ifndef GPAC_DISABLE_VRML
	if (txh->matteTexture) {
		u32 ret = gf_sc_texture_enable_matte_texture(txh->matteTexture);
		if (!ret) return 0;
		visual_3d_set_matrix_mode(compositor->visual, V3D_MATRIX_TEXTURE);
		if (gf_sc_texture_get_transform(txh, tx_transform, &mx, 0)) 
			visual_3d_matrix_load(compositor->visual, mx.m);
		else
			visual_3d_matrix_reset(compositor->visual);
		visual_3d_set_matrix_mode(compositor->visual, V3D_MATRIX_MODELVIEW);
		return ret;
	}
#endif
	if (!txh || !txh->tx_io) return 0;

	if (txh->compute_gradient_matrix && gf_sc_texture_needs_reload(txh) ) {
		compositor_gradient_update(txh);
	}

	tx_set_image(txh, 0);

	visual_3d_set_matrix_mode(compositor->visual, V3D_MATRIX_TEXTURE);
	if (bounds && txh->compute_gradient_matrix) {
		GF_Matrix2D mx2d;
		txh->compute_gradient_matrix(txh, bounds, &mx2d, 1);
		gf_mx_from_mx2d(&mx, &mx2d);
		visual_3d_matrix_load(compositor->visual, mx.m);
	}
	else if (gf_sc_texture_get_transform(txh, tx_transform, &mx, 0)) {
		visual_3d_matrix_load(compositor->visual, mx.m);
	} else {
		visual_3d_matrix_reset(compositor->visual);
	}
	visual_3d_set_matrix_mode(compositor->visual, V3D_MATRIX_MODELVIEW);

	txh->flags |= GF_SR_TEXTURE_USED;

#ifndef GPAC_USE_OGL_ES
	if (txh->tx_io->yuv_shader) {
		/*use our program*/
		Bool is_rect = txh->tx_io->flags & TX_IS_RECT;
		glUseProgram(is_rect ? compositor->visual->yuv_rect_glsl_program : compositor->visual->yuv_glsl_program);

		glEnable(txh->tx_io->gl_type);
		
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(txh->tx_io->gl_type, txh->tx_io->v_id);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(txh->tx_io->gl_type, txh->tx_io->u_id);

		glActiveTexture(GL_TEXTURE0 );
		glBindTexture(txh->tx_io->gl_type, txh->tx_io->id);
		
		tx_bind_with_mode(txh, txh->transparent, txh->tx_io->blend_mode, 1);
		glClientActiveTexture(GL_TEXTURE0);
	} else 
#endif
    {
		tx_bind(txh);
	}
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


GF_STENCIL gf_sc_texture_get_stencil(GF_TextureHandler *txh)
{
	return txh->tx_io->tx_raster;
}

void gf_sc_texture_set_stencil(GF_TextureHandler *txh, GF_STENCIL stencil)
{
	txh->tx_io->tx_raster = stencil;
	txh->tx_io->flags |= TX_NEEDS_HW_LOAD;
}

