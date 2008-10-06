/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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
#endif
#ifdef GPAC_TRISCOPE_MODE
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
	if (!txh->tx_io) return;
	if (txh->tx_io->tx_raster) {
		txh->compositor->rasterizer->stencil_delete(txh->tx_io->tx_raster);
		txh->tx_io->tx_raster = NULL;
	}

#ifndef GPAC_DISABLE_3D
	if (txh->tx_io->id) glDeleteTextures(1, &txh->tx_io->id);
	if (txh->tx_io->scale_data) free(txh->tx_io->scale_data);
	if (txh->tx_io->conv_data) free(txh->tx_io->conv_data);
#endif

#ifdef GPAC_TRISCOPE_MODE
	if (txh->tx_io->depth_data) free(txh->tx_io->depth_data);
#endif
	free(txh->tx_io);
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
	}
	txh->tx_io->flags |= TX_NEEDS_HW_LOAD;
#endif
#ifdef GPAC_TRISCOPE_MODE
	/* remove renoir texture from the display list */
	if (txh->RenoirObject) {
		DestroyRenoirObject(txh->RenoirObject, txh->compositor->RenoirHandler);
		if (txh->tx_io->depth_data) {
			free(txh->tx_io->depth_data);
			txh->tx_io->depth_data = NULL;
		}
	    txh->RenoirObject = NULL;
	}
	
#endif
	
}

#ifndef GPAC_DISABLE_3D

void gf_sc_texture_set_blend_mode(GF_TextureHandler *txh, u32 mode)
{
	if (txh->tx_io) txh->tx_io->blend_mode = mode;
}


void tx_bind_with_mode(GF_TextureHandler *txh, Bool transparent, u32 blend_mode)
{
	if (!txh->tx_io || !txh->tx_io->id || !txh->tx_io->gl_type) return;
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
		if ((txh->tx_io->gl_format==GL_LUMINANCE) || (txh->tx_io->gl_format==GL_LUMINANCE_ALPHA)) {
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND/*GL_MODULATE*/);
		} else {
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		}
		break;
	}
	glBindTexture(txh->tx_io->gl_type, txh->tx_io->id);
}

void tx_bind(GF_TextureHandler *txh)
{
	if (txh->tx_io )
		tx_bind_with_mode(txh, txh->transparent, txh->tx_io->blend_mode);
}

void gf_sc_texture_disable(GF_TextureHandler *txh)
{
	if (txh && txh->tx_io) {
		glDisable(txh->tx_io->gl_type);
		if (txh->transparent) glDisable(GL_BLEND);
	}
}


Bool tx_can_use_rect_ext(GF_Compositor *compositor, GF_TextureHandler *txh)
{
	u32 i, count;

//	compositor->gl_caps.yuv_texture = 0;
	if (!compositor->gl_caps.rect_texture) return 0;
	if (!compositor->disable_rect_ext) return 1;
	/*this happens ONLY with text texturing*/
	if (!txh->owner) return 0;

	count = gf_node_get_parent_count(txh->owner);

	/*background 2D can use RECT ext without pb*/
	if (gf_node_get_tag(txh->owner)==TAG_MPEG4_Background2D) return 1;
	/*if a bitmap is using the texture force using RECT*/
	for (i=0; i<count; i++) {
		GF_Node *n = gf_node_get_parent(txh->owner, 0);
		if (gf_node_get_tag(n)==TAG_MPEG4_Appearance) {
			count = gf_node_get_parent_count(n);
			for (i=0; i<count; i++) {
				M_Shape *s = (M_Shape *) gf_node_get_parent(n, 0);
				if (s->geometry && (gf_node_get_tag((GF_Node *)s)==TAG_MPEG4_Shape) && (gf_node_get_tag(s->geometry)==TAG_MPEG4_Bitmap)) return 1;
			}
		}
	}
	return 0;
}


static Bool tx_setup_format(GF_TextureHandler *txh)
{
	Bool is_pow2, use_rect;
	GF_Compositor *compositor = (GF_Compositor *)txh->compositor;

	/*in case the ID has been lost, resetup*/
	if (!txh->tx_io->id) {
		glGenTextures(1, &txh->tx_io->id);
		txh->tx_io->flags |= TX_EMULE_FIRST_LOAD;
	}

	/*first setup, this will force recompute bounds in case used with bitmap - we could refine and only 
	invalidate for bitmaps only*/
	if (txh->owner && (!txh->tx_io->rescale_width || !txh->tx_io->rescale_height)) 
		gf_node_dirty_set(txh->owner, 0, 1);

	txh->tx_io->rescale_width = gf_get_next_pow2(txh->width);
	txh->tx_io->rescale_height = gf_get_next_pow2(txh->height);

	is_pow2 = ((txh->tx_io->rescale_width==txh->width) && (txh->tx_io->rescale_height==txh->height)) ? 1 : 0;
	txh->tx_io->flags = TX_IS_POW2;
	txh->tx_io->gl_type = GL_TEXTURE_2D;
	use_rect = tx_can_use_rect_ext(compositor, txh);
	if (!is_pow2 && use_rect) {
#ifndef GPAC_USE_TINYGL
		txh->tx_io->gl_type = GL_TEXTURE_RECTANGLE_EXT;
#endif
		txh->tx_io->flags = TX_IS_RECT;
	}
	if (!use_rect && !compositor->gl_caps.npot_texture && !is_pow2) txh->tx_io->flags = TX_MUST_SCALE;

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
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_RGBA:
		txh->tx_io->gl_format = GL_RGBA;
		txh->tx_io->nb_comp = 4;
		break;
	case GF_PIXEL_ARGB:
		if (!compositor->gl_caps.bgra_texture) return 0;
		txh->tx_io->gl_format = GL_BGRA_EXT;
		txh->tx_io->nb_comp = 4;
		break;
	case GF_PIXEL_YV12:
		if (!compositor->disable_yuvgl && compositor->gl_caps.yuv_texture && !(txh->tx_io->flags & TX_MUST_SCALE) ) {
			txh->tx_io->gl_format = compositor->gl_caps.yuv_texture;
			txh->tx_io->nb_comp = 3;
			txh->tx_io->gl_dtype = UNSIGNED_SHORT_8_8_MESA;
		} else {
			if (!use_rect && compositor->emul_pow2) txh->tx_io->flags = TX_EMULE_POW2;
			txh->tx_io->gl_format = GL_RGB;
			txh->tx_io->nb_comp = 3;
		}
		break;
	default:
		return 0;
	}
	/*note we don't free the data if existing, since this only happen when re-setting up after context loss (same size)*/
	if ((txh->tx_io->flags == TX_MUST_SCALE) & !txh->tx_io->scale_data) {
		txh->tx_io->scale_data = (char*)malloc(sizeof(char) * txh->tx_io->nb_comp*txh->tx_io->rescale_width*txh->tx_io->rescale_height);
		memset(txh->tx_io->scale_data , 0, sizeof(char) * txh->tx_io->nb_comp*txh->tx_io->rescale_width*txh->tx_io->rescale_height);
	}

	glEnable(txh->tx_io->gl_type);
	glBindTexture(txh->tx_io->gl_type, txh->tx_io->id);

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
		GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_S, GL_REPEAT);
		GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	if (txh->tx_io->gl_type == GL_TEXTURE_2D) {
		GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MAG_FILTER, txh->compositor->high_speed ? GL_NEAREST : GL_LINEAR);
		GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MIN_FILTER, txh->compositor->high_speed ? GL_NEAREST : GL_LINEAR);
	} else {
		GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		GLTEXPARAM(txh->tx_io->gl_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
#endif
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glDisable(txh->tx_io->gl_type);
	return 1;
}

char *gf_sc_texture_get_data(GF_TextureHandler *txh, u32 *pix_format)
{
	char *data = txh->tx_io->conv_data;
	*pix_format = txh->tx_io->conv_format;
	if (*pix_format == txh->pixelformat) return txh->data;
	return data;
}

void txh_unpack_yuv(GF_TextureHandler *txh)
{
	u32 i, j;
	u8 *dst, *y, *u, *v, *p_y, *p_u, *p_v;
	if (!txh->tx_io->conv_data) {
		txh->tx_io->conv_data = (char*)malloc(sizeof(char) * 2 * txh->width * txh->height);
	}
	p_y = txh->data;
	p_u = txh->data + txh->stride*txh->height;
	p_v = txh->data + 5*txh->stride*txh->height/4;

	/*convert to UYVY and flip texture*/
	for (i=0; i<txh->height; i++) {
		u32 idx = txh->height-i-1;
		y = p_y + idx*txh->stride;
		u = p_u + (idx/2) * txh->stride/2;
		v = p_v + (idx/2) * txh->stride/2;
		dst = txh->tx_io->conv_data + 2*i*txh->stride;

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
	txh->flags |= GF_SR_TEXTURE_NO_GL_FLIP;
}

/*note about conversion: we consider that a texture without a stream attached is generated by the compositor
hence is never flipped. Otherwise all textures attached to stream are flipped in order to match uv coords*/
Bool tx_convert(GF_TextureHandler *txh)
{
	GF_VideoSurface src, dst;
	u32 out_stride;
	GF_Compositor *compositor = (GF_Compositor *)txh->compositor;

	switch (txh->pixelformat) {
	case GF_PIXEL_ARGB:
		if (!compositor->gl_caps.bgra_texture) return 0;
	case GF_PIXEL_GREYSCALE:
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_RGBA:
		txh->tx_io->conv_format = txh->pixelformat;
		txh->tx_io->flags |= TX_NEEDS_HW_LOAD;
		return 1;
	case GF_PIXEL_YV12:
		if (txh->tx_io->gl_format == compositor->gl_caps.yuv_texture) {
			txh->tx_io->conv_format = GF_PIXEL_YVYU;
			txh->tx_io->flags |= TX_NEEDS_HW_LOAD;
			txh_unpack_yuv(txh);
			return 1;
		}
		break;
	default:
		txh->tx_io->conv_format = 0;
		return 0;
	}
	if (!txh->tx_io->conv_data) {
		if (txh->tx_io->flags == TX_EMULE_POW2) {
			/*convert video to a po of 2 WITHOUT SCALING VIDEO*/
			txh->tx_io->conv_w = gf_get_next_pow2(txh->width);
			txh->tx_io->conv_h = gf_get_next_pow2(txh->height);
			txh->tx_io->conv_data = (char*)malloc(sizeof(char) * 3 * txh->tx_io->conv_w * txh->tx_io->conv_h);
			memset(txh->tx_io->conv_data , 0, sizeof(char) * 3 * txh->tx_io->conv_w * txh->tx_io->conv_h);
			txh->tx_io->conv_wscale = INT2FIX(txh->width) / txh->tx_io->conv_w;
			txh->tx_io->conv_hscale = INT2FIX(txh->height) / txh->tx_io->conv_h;
		} else {
			txh->tx_io->conv_data = (char*)malloc(sizeof(char) * 3 * txh->width * txh->height);
		}
	}
	out_stride = 3 * ((txh->tx_io->flags & TX_EMULE_POW2) ? txh->tx_io->conv_w : txh->width);

	dst.width = src.width = txh->width;
	dst.height = src.height = txh->height;
	dst.is_hardware_memory = src.is_hardware_memory = 0;

	src.pitch = txh->stride;
	src.pixel_format = txh->pixelformat;
	src.video_buffer = txh->data;

	dst.pitch = out_stride;
	txh->tx_io->conv_format = dst.pixel_format = GF_PIXEL_RGB_24;
	dst.video_buffer = txh->tx_io->conv_data;

	/*stretch and flip*/
	gf_stretch_bits(&dst, &src, NULL, NULL, 0, 0xFF, 1, NULL, NULL);
	txh->tx_io->flags |= TX_NEEDS_HW_LOAD;
	txh->flags |= GF_SR_TEXTURE_NO_GL_FLIP;
	return 1;
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
		tx_setup_format(txh);
		first_load = 1;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Texturing] Allocating OpenGL texture %d\n", txh->tx_io->id));
	}
	if (!txh->tx_io->gl_type) return 0;
	
	if (txh->tx_io->flags & TX_EMULE_FIRST_LOAD) {
		txh->tx_io->flags &= ~TX_EMULE_FIRST_LOAD;
		first_load = 1;
	}

	/*convert image*/
	tx_convert(txh);

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
		if (first_load) {
			glTexImage2D(txh->tx_io->gl_type, 0, tx_mode, w, h, 0, txh->tx_io->gl_format, txh->tx_io->gl_dtype, (unsigned char *) data);

		} else {
			glTexSubImage2D(txh->tx_io->gl_type, 0, 0, 0, w, h, txh->tx_io->gl_format, txh->tx_io->gl_dtype, (unsigned char *) data);
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
			src.width = txh->width;
			src.height = txh->height;
			src.pitch = txh->stride;
			src.pixel_format = txh->pixelformat;
			src.video_buffer = txh->data;

			dst.width = txh->tx_io->rescale_width;
			dst.height = txh->tx_io->rescale_height;
			dst.pitch = txh->tx_io->rescale_width*txh->tx_io->nb_comp;
			dst.pixel_format = txh->pixelformat;
			dst.video_buffer = txh->tx_io->scale_data;

			gf_stretch_bits(&dst, &src, NULL, NULL, 0, 0xFF, 0, NULL, NULL);
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
#ifdef GPAC_TRISCOPE_MODE
	const char *sOpt;
	float OGL_depthGain;
#endif

	/*in case the ID has been lost, resetup*/
	if (!txh->data || !txh->tx_io->tx_raster) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[GL Texture] Copying GL backbuffer %dx%d@PF=%s to systems memory\n", txh->width, txh->height, gf_4cc_to_str(txh->pixelformat) ));

	if (txh->pixelformat==GF_PIXEL_RGBA) {
		glReadPixels(0, 0, txh->width, txh->height, GL_RGBA, GL_UNSIGNED_BYTE, txh->data);
	} else if (txh->pixelformat==GF_PIXEL_RGB_24) {
		glReadPixels(0, 0, txh->width, txh->height, GL_RGB, GL_UNSIGNED_BYTE, txh->data);

#ifdef GPAC_TRISCOPE_MODE
	} else if (txh->pixelformat==GF_PIXEL_RGBDS) {
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
		
		glPixelTransferf(GL_DEPTH_SCALE, txh->compositor->OGLDepthGain); 
		glPixelTransferf(GL_DEPTH_BIAS, txh->compositor->OGLDepthOffset); 
		
		/*obtain depthmap*/ /*NOTE: could txt width, height change once allocated?*/
		if (!txh->tx_io->depth_data) txh->tx_io->depth_data = (char*)malloc(sizeof(char)*txh->width*txh->height);
		glReadPixels(0, 0, txh->width, txh->height, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, txh->tx_io->depth_data);
	    /*	depth = alpha & 0xfe 
		    shape = plan alpha & 0x01 */
		/*at the moment RENOIR WORKS WITH DSRGB - need to recombine components*/

#if 1		/*this corresponds to the RGBDS ordering*/
		for (i=0; i<txh->height*txh->width; i++) {
			u8 ds;
			/* erase lowest-weighted depth bit */
			u8 depth = txh->tx_io->depth_data[i] & 0xfe; 
			/*get alpha*/
			ds = (txh->data[i*4 + 3]);
			/* if heaviest-weighted alpha bit is set (>128) , turn on shape bit*/
			if (ds & 0x80) depth |= 0x01;
			txh->data[i*4+3] = depth; /*insert depth onto alpha*/ 
		}

#endif		
#endif
	}
	/*flip image because of openGL*/
	tmp = (char*)malloc(sizeof(char)*txh->stride);
	hy = txh->height/2;
	for (i=0; i<hy; i++) {
		memcpy(tmp, txh->data + i*txh->stride, txh->stride);
		memcpy(txh->data + i*txh->stride, txh->data + (txh->height - 1 - i) * txh->stride, txh->stride);
		memcpy(txh->data + (txh->height - 1 - i) * txh->stride, tmp, txh->stride);
	}
	free(tmp);

}
#else

void gf_get_tinygl_depth(GF_TextureHandler *txh) {
	
	glReadPixels(0, 0, txh->width, txh->height, GL_RGBDS, GL_UNSIGNED_BYTE, txh->data);

}
#endif

#endif

Bool gf_sc_texture_get_transform(GF_TextureHandler *txh, GF_Node *tx_transform, GF_Matrix *mx)
{
	GF_Matrix tmp;
	Bool ret = 0;
	gf_mx_init(*mx);

	/*flip image if requested*/
	if (! (txh->flags & GF_SR_TEXTURE_NO_GL_FLIP) ) {
		/*flip it*/
		gf_mx_add_scale(mx, FIX_ONE, -FIX_ONE, FIX_ONE);
		/*and translate it to handle repeatS/repeatT*/
		gf_mx_add_translation(mx, 0, -FIX_ONE, 0);
		ret = 1;
	}

	/*WATCHOUT: GL_TEXTURE_RECTANGLE coords are w, h not 1.0, 1.0*/
	if (txh->tx_io->flags & TX_IS_RECT) {

		gf_mx_add_scale(mx, INT2FIX(txh->width), INT2FIX(txh->height), FIX_ONE);
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

	if (tx_transform) {
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
	return ret;
}

static Bool gf_sc_texture_enable_matte_texture(GF_Node *n)
{
#ifdef GPAC_DISABLE_3D
	return 0;
#else
	GF_TextureHandler *b_surf;
#ifndef GPAC_USE_TINYGL
	GF_TextureHandler *matte_hdl;
	GF_TextureHandler *a_surf;
	GF_TextureHandler *alpha_surf;
	char * action ;
	int tmp;
	u8 texture[4];
	MFFloat coefficients ;
	GF_Compositor *compositor;
#endif
	M_MatteTexture *matte = (M_MatteTexture *)n;
	

	b_surf = gf_sc_texture_get_handler(matte->surfaceB);
	
	if (!b_surf || !b_surf->tx_io) return 0;
	glEnable(GL_BLEND);	
	tx_set_image(b_surf, 0);

#ifdef GPAC_USE_TINYGL
	tx_bind(b_surf);
	return 1;
#else

	compositor = b_surf->compositor;
	if (!compositor->gl_caps.glActiveTextureARB) {
		tx_bind(b_surf);
		return 1;
	}
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

		compositor->gl_caps.glActiveTextureARB(GL_TEXTURE0_ARB);
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
		if (!strcmp(action,"BIAS")) operand = GL_ADD_SIGNED_ARB;

		compositor->gl_caps.glActiveTextureARB(GL_TEXTURE1_ARB);
		tx_bind(b_surf);
		GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,		COMBINE_ARB);
		GLTEXENV(GL_TEXTURE_ENV, COMBINE_RGB_ARB,			operand);
		GLTEXENV(GL_TEXTURE_ENV, SOURCE0_RGB_ARB ,			GL_TEXTURE1_ARB);
		GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB,		GL_SRC_COLOR);
		GLTEXENV(GL_TEXTURE_ENV, SOURCE1_RGB_ARB,			GL_TEXTURE0_ARB);
		GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB,		GL_SRC_COLOR);
		GLTEXENV(GL_TEXTURE_ENV, COMBINE_ALPHA_ARB,		GL_REPLACE );
		GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB,		GL_TEXTURE1_ARB  );
		GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB,	GL_SRC_ALPHA);
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
		
		compositor->gl_caps.glActiveTextureARB(GL_TEXTURE0_ARB);
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
		
		/* fin de la génération de la texture donnée par la fraction ! */
		
		/* mélange effectif des textures ! } */
		compositor->gl_caps.glActiveTextureARB(GL_TEXTURE1_ARB);
		tx_bind(b_surf);

		if (a_surf) {
			compositor->gl_caps.glActiveTextureARB(GL_TEXTURE2_ARB);
			tx_set_image(a_surf, 0);
			tx_bind(a_surf);
			GLTEXENV(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,COMBINE_ARB);
			GLTEXENV(GL_TEXTURE_ENV,COMBINE_RGB_ARB,INTERPOLATE_ARB );
			GLTEXENV(GL_TEXTURE_ENV, SOURCE0_RGB_ARB , GL_TEXTURE1_ARB  );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND0_RGB_ARB,GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, SOURCE1_RGB_ARB   , GL_TEXTURE2_ARB  );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND1_RGB_ARB,GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV,SOURCE2_RGB_ARB, GL_TEXTURE0_ARB  );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND2_RGB_ARB,GL_SRC_COLOR);
		}	
		return 3;
	}	
	/* end CROSS_FADE */

	/*REVEAL */
	if (!strcmp(action,"REVEAL")) {
		compositor->gl_caps.glActiveTextureARB(GL_TEXTURE0_ARB);
		tx_bind(b_surf);

		if (alpha_surf) {
			tx_set_image(alpha_surf, 0);
			compositor->gl_caps.glActiveTextureARB(GL_TEXTURE1_ARB);
			tx_bind(alpha_surf);
		}
		if (a_surf) {
			compositor->gl_caps.glActiveTextureARB(GL_TEXTURE2_ARB);
			tx_set_image(a_surf, 0);
			tx_bind(a_surf);
			
			GLTEXENV(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,COMBINE_ARB);
			GLTEXENV(GL_TEXTURE_ENV,COMBINE_RGB_ARB,INTERPOLATE_ARB );
			GLTEXENV(GL_TEXTURE_ENV, SOURCE0_RGB_ARB , GL_TEXTURE0_ARB  );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND0_RGB_ARB,GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, SOURCE1_RGB_ARB   , GL_TEXTURE2_ARB  );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND1_RGB_ARB,GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV,SOURCE2_RGB_ARB, GL_TEXTURE1_ARB );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND2_RGB_ARB,GL_SRC_ALPHA);
			
		}
		return 3;
	}
	/* end REVEAL */

	/*INVERT */
	if (! strcmp(action,"INVERT")) {
		compositor->gl_caps.glActiveTextureARB(GL_TEXTURE0_ARB);
		tx_bind(b_surf);
		GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,	COMBINE_ARB);
		GLTEXENV(GL_TEXTURE_ENV, COMBINE_RGB_ARB,		GL_REPLACE );
		GLTEXENV(GL_TEXTURE_ENV, SOURCE0_RGB_ARB,		GL_TEXTURE0_ARB  );
		GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB,	GL_ONE_MINUS_SRC_COLOR);

		if (matte->parameter.count && matte->parameter.vals[0]) {
			GLTEXENV(GL_TEXTURE_ENV, COMBINE_ALPHA_ARB,		GL_REPLACE );
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB,		GL_TEXTURE0_ARB  );
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB,	GL_ONE_MINUS_SRC_ALPHA);
		}
		return 1;
	}
	/* end INVERT */

	/* opération REPLACE_ALPHA */
	if (!strcmp(action,"REPLACE_ALPHA")) {
		compositor->gl_caps.glActiveTextureARB(GL_TEXTURE0_ARB);
		tx_bind(b_surf);
		
		if (alpha_surf) {
			glEnable(GL_BLEND);
			compositor->gl_caps.glActiveTextureARB(GL_TEXTURE1_ARB);
			tx_set_image(alpha_surf, 0);
			tx_bind(alpha_surf);
			GLTEXENV(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,COMBINE_ARB);
			GLTEXENV(GL_TEXTURE_ENV,COMBINE_RGB_ARB,GL_REPLACE );
			GLTEXENV(GL_TEXTURE_ENV, SOURCE0_RGB_ARB   , GL_TEXTURE0_ARB  );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND0_RGB_ARB,GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV,COMBINE_ALPHA_ARB,GL_REPLACE );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA_ARB,GL_SRC_ALPHA);
		}
		return 2;
	}
	/* end REPLACE_ALPHA */

	/* MULTIPLY_ALPHA */
	if (!strcmp(action,"MULTIPLY_ALPHA")) {
		compositor->gl_caps.glActiveTextureARB(GL_TEXTURE0_ARB);
		tx_bind(b_surf);
		GLTEXENV(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);

		if (alpha_surf) {
			compositor->gl_caps.glActiveTextureARB(GL_TEXTURE1_ARB);
			tx_set_image(alpha_surf, 0);
			tx_bind(alpha_surf);
			GLTEXENV(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,COMBINE_ARB);
			GLTEXENV(GL_TEXTURE_ENV,COMBINE_RGB_ARB,GL_REPLACE );
			GLTEXENV(GL_TEXTURE_ENV, SOURCE0_RGB_ARB   , GL_TEXTURE0_ARB  );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND0_RGB_ARB,GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV,COMBINE_ALPHA_ARB,GL_MODULATE );
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB   , GL_TEXTURE1_ARB  );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND1_ALPHA_ARB,GL_SRC_ALPHA);
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB   , GL_TEXTURE0_ARB  );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA_ARB,GL_SRC_ALPHA);
		}
		return 2;
	}
	/* end MULTIPLY_ALPHA */

	/*ADD */
	if (! strcmp(action,"ADD")) {
		compositor->gl_caps.glActiveTextureARB(GL_TEXTURE0_ARB);
		tx_bind(b_surf);
		if (a_surf) {
			compositor->gl_caps.glActiveTextureARB(GL_TEXTURE1_ARB);
			tx_set_image(a_surf, 0);
			tx_bind(a_surf);
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,		COMBINE_ARB);
			GLTEXENV(GL_TEXTURE_ENV, COMBINE_RGB_ARB,			GL_ADD_SIGNED_ARB );
			GLTEXENV(GL_TEXTURE_ENV, SOURCE0_RGB_ARB,			GL_TEXTURE0_ARB );
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB,		GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, SOURCE1_RGB_ARB,			GL_TEXTURE1_ARB );
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB,		GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, COMBINE_ALPHA_ARB,		GL_REPLACE );
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB,		GL_TEXTURE0_ARB  );
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB,	GL_SRC_ALPHA);
		}
		return 2;
	}
	/*end ADD*/

	/* ADD_SIGNED*/
	if (! strcmp(action,"ADD_SIGNED")) {
		compositor->gl_caps.glActiveTextureARB(GL_TEXTURE0_ARB);
		tx_bind(b_surf);
		if (a_surf) {
			compositor->gl_caps.glActiveTextureARB(GL_TEXTURE1_ARB);
			tx_set_image(a_surf, 0);
			tx_bind(a_surf);
			GLTEXENV(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,COMBINE_ARB);
			GLTEXENV(GL_TEXTURE_ENV,COMBINE_RGB_ARB,GL_ADD );
			GLTEXENV(GL_TEXTURE_ENV, SOURCE0_RGB_ARB   , GL_TEXTURE0_ARB );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND0_RGB_ARB,GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV,SOURCE1_RGB_ARB,GL_TEXTURE1_ARB );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND1_RGB_ARB,GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV,COMBINE_ALPHA_ARB,GL_REPLACE );
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB   , GL_TEXTURE0_ARB  );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA_ARB,GL_SRC_ALPHA);
		}
		return 2;
	}
	/* end ADD_SIGNED*/

	/*SUBSTRACT*/
	if (! strcmp(action,"SUBSTRACT")) {
		compositor->gl_caps.glActiveTextureARB(GL_TEXTURE0_ARB);
		tx_bind(b_surf);
		if (a_surf) {
			compositor->gl_caps.glActiveTextureARB(GL_TEXTURE1_ARB);
			tx_set_image(a_surf, 0);
			tx_bind(a_surf);
			GLTEXENV(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,COMBINE_ARB);
			GLTEXENV(GL_TEXTURE_ENV,COMBINE_RGB_ARB,GL_SUBTRACT_ARB);
			GLTEXENV(GL_TEXTURE_ENV, SOURCE0_RGB_ARB   , GL_TEXTURE0_ARB );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND0_RGB_ARB,GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV,SOURCE1_RGB_ARB,GL_TEXTURE1_ARB );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND1_RGB_ARB,GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV,COMBINE_ALPHA_ARB,GL_REPLACE );
			GLTEXENV(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB   , GL_TEXTURE0_ARB  );
			GLTEXENV(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA_ARB,GL_SRC_ALPHA);
		}
		return 2;
	}
	/* end SUBTRACT*/

	/*BLEND*/
	if (! strcmp(action,"BLEND")) {
		compositor->gl_caps.glActiveTextureARB(GL_TEXTURE0_ARB);
		tx_bind(b_surf);
		if (a_surf) {
			GLTEXENV(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
			compositor->gl_caps.glActiveTextureARB(GL_TEXTURE1_ARB);
			tx_set_image(a_surf, 0);
			tx_bind(a_surf);
			GLTEXENV(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_DECAL);
			GLTEXENV(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,		COMBINE_ARB);
			GLTEXENV(GL_TEXTURE_ENV, COMBINE_RGB_ARB,			GL_MODULATE);
			GLTEXENV(GL_TEXTURE_ENV, SOURCE0_RGB_ARB,			GL_TEXTURE0_ARB );
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB,		GL_SRC_COLOR);
			GLTEXENV(GL_TEXTURE_ENV, SOURCE1_RGB_ARB,			GL_TEXTURE1_ARB );
			GLTEXENV(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB,		GL_SRC_COLOR);
		}
		return 2;
	}
	/*end BLEND */

	tx_bind(b_surf);
	return 1;

#endif /*GPAC_USE_TINYGL*/

#undef GLTEXPARAM
#endif
}


Bool gf_sc_texture_is_transparent(GF_TextureHandler *txh)
{
	M_MatteTexture *matte;
	if (!txh->matteTexture) return txh->transparent;
	matte = (M_MatteTexture *)txh->matteTexture;
	if (!matte->operation.buffer) return txh->transparent;
	if (matte->alphaSurface) return 1;
	if (!strcmp(matte->operation.buffer, "COLOR_MATRIX")) return 1;
	return txh->transparent;
}

#ifndef GPAC_DISABLE_3D

u32 gf_sc_texture_enable_ex(GF_TextureHandler *txh, GF_Node *tx_transform, GF_Rect *bounds)
{
	GF_Matrix mx;
	GF_Compositor *compositor = (GF_Compositor *)txh->compositor;

	if (txh->matteTexture) {
		u32 ret = gf_sc_texture_enable_matte_texture(txh->matteTexture);
		if (!ret) return 0;
		visual_3d_set_matrix_mode(compositor->visual, V3D_MATRIX_TEXTURE);
		if (gf_sc_texture_get_transform(txh, tx_transform, &mx)) 
			visual_3d_matrix_load(compositor->visual, mx.m);
		else
			visual_3d_matrix_reset(compositor->visual);
		visual_3d_set_matrix_mode(compositor->visual, V3D_MATRIX_MODELVIEW);
		return ret;
	}
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
	else if (gf_sc_texture_get_transform(txh, tx_transform, &mx)) {
		visual_3d_matrix_load(compositor->visual, mx.m);
	} else {
		visual_3d_matrix_reset(compositor->visual);
	}
	visual_3d_set_matrix_mode(compositor->visual, V3D_MATRIX_MODELVIEW);

	txh->flags |= GF_SR_TEXTURE_USED;
	tx_bind(txh);
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

