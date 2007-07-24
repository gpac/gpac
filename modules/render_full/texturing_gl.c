/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D+3D rendering module
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
#include "gl_inc.h"

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


typedef struct
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
	u32 nb_comp, gl_format, gl_type;
#endif
} TXWrapper;

GF_Err render_texture_allocate(GF_TextureHandler *txh)
{
	TXWrapper *tx_wrap;
	if (txh->hwtx) return GF_OK;
	GF_SAFEALLOC(tx_wrap, TXWrapper);
	if (!tx_wrap) return GF_OUT_OF_MEM;
	txh->hwtx = tx_wrap;

	return GF_OK;
}

void render_texture_release(GF_TextureHandler *txh)
{
	TXWrapper *tx_wrap;
	if (!txh->hwtx) return;

	tx_wrap = (TXWrapper *) txh->hwtx;
	txh->hwtx = NULL;

	if (tx_wrap->tx_raster) {
		txh->compositor->r2d->stencil_delete(tx_wrap->tx_raster);
		tx_wrap->tx_raster = NULL;
	}

#ifndef GPAC_DISABLE_3D
	if (tx_wrap->id) glDeleteTextures(1, &tx_wrap->id);
	if (tx_wrap->scale_data) free(tx_wrap->scale_data);
	if (tx_wrap->conv_data) free(tx_wrap->conv_data);
#endif
	free(tx_wrap);
}


GF_Err render_texture_set_data(GF_TextureHandler *hdl)
{
	TXWrapper *tx_wrap = (TXWrapper *)hdl->hwtx;
	tx_wrap->flags |= TX_NEEDS_RASTER_LOAD | TX_NEEDS_HW_LOAD;
	return GF_OK;
}

void render_texture_reset(GF_TextureHandler *hdl)
{
	TXWrapper *tx_wrap = (TXWrapper *)hdl->hwtx;

#ifndef GPAC_DISABLE_3D
	if (tx_wrap->id) {
		glDeleteTextures(1, &tx_wrap->id);
		tx_wrap->id = 0;
	}
	tx_wrap->flags |= TX_NEEDS_HW_LOAD;
#endif
}

#ifndef GPAC_DISABLE_3D

void tx_set_blend_mode(GF_TextureHandler *txh, u32 mode)
{
	if (txh->hwtx) {
		TXWrapper *tx_wrap = (TXWrapper *) txh->hwtx;
		if (tx_wrap) tx_wrap->blend_mode = mode;
	}
}

void tx_bind_with_mode(GF_TextureHandler *txh, Bool transparent, u32 blend_mode)
{
	TXWrapper *tx_wrap = (TXWrapper *) txh->hwtx;
	if (!tx_wrap->id || !tx_wrap->gl_type) return;
	glEnable(tx_wrap->gl_type);

	switch (blend_mode) {
	case TX_BLEND:
		if (txh->transparent) glEnable(GL_BLEND);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
		break;
	case TX_REPLACE:
		if (txh->transparent) glEnable(GL_BLEND);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		break;
	case TX_MODULATE:
		if (txh->transparent) glEnable(GL_BLEND);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		break;
	case TX_DECAL:
	default:
		if ((tx_wrap->gl_format==GL_LUMINANCE) || (tx_wrap->gl_format==GL_LUMINANCE_ALPHA)) {
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND/*GL_MODULATE*/);
		} else {
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		}
		break;
	}
	glBindTexture(tx_wrap->gl_type, tx_wrap->id);
}

void tx_bind(GF_TextureHandler *txh)
{
	TXWrapper *tx_wrap = (TXWrapper *) txh->hwtx;
	tx_bind_with_mode(txh, txh->transparent, tx_wrap->blend_mode);
}

void tx_disable(GF_TextureHandler *txh)
{
	TXWrapper *tx_wrap;
	if (txh && txh->hwtx) {
		tx_wrap = (TXWrapper *) txh->hwtx;
		glDisable(tx_wrap->gl_type);
		if (txh->transparent) glDisable(GL_BLEND);
	}
}


u32 get_pow2(u32 s)
{
	u32 i;
	u32 res = s;
    u32 sizes[] = { 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048 };
    u32 nSizes = sizeof(sizes) / sizeof(int);
    for (i = 0; i < nSizes; ++i) { if (res <= sizes[i]) { res = sizes[i]; break; } }
	return res;
}

Bool tx_can_use_rect_ext(Render *sr, GF_TextureHandler *txh)
{
	u32 i, count;
	if (!sr->gl_caps.rect_texture) return 0;
	if (!sr->disable_rect_ext) return 1;
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
	Render *sr = (Render *)txh->compositor->visual_renderer->user_priv;
	TXWrapper *tx_wrap = (TXWrapper *) txh->hwtx;

	/*in case the ID has been lost, resetup*/
	if (!tx_wrap->id) {
		glGenTextures(1, &tx_wrap->id);
		tx_wrap->flags |= TX_EMULE_FIRST_LOAD;
	}

	/*first setup, this will force recompute bounds in case used with bitmap - we could refine and only 
	invalidate for bitmaps only*/
	if (txh->owner && (!tx_wrap->rescale_width || !tx_wrap->rescale_height)) 
		gf_node_dirty_set(txh->owner, 0, 1);

	tx_wrap->rescale_width = get_pow2(txh->width);
	tx_wrap->rescale_height = get_pow2(txh->height);

	is_pow2 = ((tx_wrap->rescale_width==txh->width) && (tx_wrap->rescale_height==txh->height)) ? 1 : 0;
	tx_wrap->flags = TX_IS_POW2;
	tx_wrap->gl_type = GL_TEXTURE_2D;
	use_rect = tx_can_use_rect_ext(sr, txh);
	if (!is_pow2 && use_rect) {
#ifndef GPAC_USE_TINYGL
		tx_wrap->gl_type = GL_TEXTURE_RECTANGLE_EXT;
#endif
		tx_wrap->flags = TX_IS_RECT;
	}
	if (!use_rect && !sr->gl_caps.npot_texture && !is_pow2) tx_wrap->flags = TX_MUST_SCALE;

	tx_wrap->nb_comp = tx_wrap->gl_format = 0;
	switch (txh->pixelformat) {
	case GF_PIXEL_GREYSCALE:
		tx_wrap->gl_format = GL_LUMINANCE;
		tx_wrap->nb_comp = 1;
		tx_wrap->gl_type = GL_TEXTURE_2D;
		if (!is_pow2) tx_wrap->flags = TX_MUST_SCALE;
		break;
	case GF_PIXEL_ALPHAGREY:
		tx_wrap->gl_format = GL_LUMINANCE_ALPHA;
		tx_wrap->nb_comp = 2;
		tx_wrap->gl_type = GL_TEXTURE_2D;
		if (!is_pow2) tx_wrap->flags = TX_MUST_SCALE;
		break;
	case GF_PIXEL_RGB_24:
		tx_wrap->gl_format = GL_RGB;
		tx_wrap->nb_comp = 3;
		break;
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_RGBA:
		tx_wrap->gl_format = GL_RGBA;
		tx_wrap->nb_comp = 4;
		break;
	case GF_PIXEL_ARGB:
		if (!sr->gl_caps.bgra_texture) return 0;
		tx_wrap->gl_format = GL_BGRA_EXT;
		tx_wrap->nb_comp = 4;
		break;
	case GF_PIXEL_YV12:
		if (!use_rect && sr->emul_pow2) tx_wrap->flags = TX_EMULE_POW2;
		tx_wrap->gl_format = GL_RGB;
		tx_wrap->nb_comp = 3;
		break;
	default:
		return 0;
	}
	/*note we don't free the data if existing, since this only happen when re-setting up after context loss (same size)*/
	if ((tx_wrap->flags == TX_MUST_SCALE) & !tx_wrap->scale_data) tx_wrap->scale_data = (char*)malloc(sizeof(char) * tx_wrap->nb_comp*tx_wrap->rescale_width*tx_wrap->rescale_height);

	glEnable(tx_wrap->gl_type);
	glBindTexture(tx_wrap->gl_type, tx_wrap->id);

#ifdef GPAC_USE_OGL_ES
	glTexParameterx(tx_wrap->gl_type, GL_TEXTURE_WRAP_S, (txh->flags & GF_SR_TEXTURE_REPEAT_S) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
	glTexParameterx(tx_wrap->gl_type, GL_TEXTURE_WRAP_T, (txh->flags & GF_SR_TEXTURE_REPEAT_T) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
	if (tx_wrap->gl_type == GL_TEXTURE_2D) {
		glTexParameterx(tx_wrap->gl_type, GL_TEXTURE_MAG_FILTER, txh->compositor->high_speed ? GL_NEAREST : GL_LINEAR);
		glTexParameterx(tx_wrap->gl_type, GL_TEXTURE_MIN_FILTER, txh->compositor->high_speed ? GL_NEAREST : GL_LINEAR);
	} else {
		glTexParameterx(tx_wrap->gl_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterx(tx_wrap->gl_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
#else
	glTexParameteri(tx_wrap->gl_type, GL_TEXTURE_WRAP_S, (txh->flags & GF_SR_TEXTURE_REPEAT_S) ? GL_REPEAT : GL_CLAMP);
	glTexParameteri(tx_wrap->gl_type, GL_TEXTURE_WRAP_T, (txh->flags & GF_SR_TEXTURE_REPEAT_T) ? GL_REPEAT : GL_CLAMP);
	if (tx_wrap->gl_type == GL_TEXTURE_2D) {
		glTexParameteri(tx_wrap->gl_type, GL_TEXTURE_MAG_FILTER, txh->compositor->high_speed ? GL_NEAREST : GL_LINEAR);
		glTexParameteri(tx_wrap->gl_type, GL_TEXTURE_MIN_FILTER, txh->compositor->high_speed ? GL_NEAREST : GL_LINEAR);
	} else {
		glTexParameteri(tx_wrap->gl_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(tx_wrap->gl_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
#endif
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glDisable(tx_wrap->gl_type);
	return 1;
}

char *tx_get_data(GF_TextureHandler *txh, u32 *pix_format)
{
	TXWrapper *tx_wrap = (TXWrapper *) txh->hwtx;
	char *data = tx_wrap->conv_data;
	*pix_format = tx_wrap->conv_format;
	if (*pix_format == txh->pixelformat) data = txh->data;
	return data;
}

u32 get_next_pow2(u32 s)
{
	u32 i;
	u32 res = s;
    u32 sizes[] = { 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 };
    u32 nSizes = sizeof(sizes) / sizeof(int);
    for (i = 0; i < nSizes; ++i) { if (res <= sizes[i]) { res = sizes[i]; break; } }
	return res;
}

/*note about conversion: we consider that a texture without a stream attached is generated by the renderer
hence is never flipped. Otherwise all textures attached to stream are flipped in order to match uv coords*/
Bool tx_convert(GF_TextureHandler *txh)
{
	GF_VideoSurface src, dst;
	u32 out_stride;
	TXWrapper *tx_wrap = (TXWrapper *) txh->hwtx;
	Render *sr = (Render *)txh->compositor->visual_renderer->user_priv;


	switch (txh->pixelformat) {
	case GF_PIXEL_ARGB:
		if (!sr->gl_caps.bgra_texture) return 0;
	case GF_PIXEL_GREYSCALE:
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_RGBA:
		if (txh->stream && !(gf_mo_get_flags(txh->stream) & GF_MO_IS_FLIP) ) {
			u32 i, hy;
			char *tmp;
			/*flip image*/
			tmp = (char*)malloc(sizeof(char)*txh->stride);
			hy = txh->height/2;
			for (i=0; i<hy; i++) {
				memcpy(tmp, txh->data + i*txh->stride, txh->stride);
				memcpy(txh->data + i*txh->stride, txh->data + (txh->height - 1 - i) * txh->stride, txh->stride);
				memcpy(txh->data + (txh->height - 1 - i) * txh->stride, tmp, txh->stride);
			}
			free(tmp);
			gf_mo_set_flag(txh->stream, GF_MO_IS_FLIP, 1);
		}
		tx_wrap->conv_format = txh->pixelformat;
		tx_wrap->flags |= TX_NEEDS_HW_LOAD;
		return 1;
	case GF_PIXEL_YV12:
		break;
	default:
		tx_wrap->conv_format = 0;
		return 0;
	}
	if (!tx_wrap->conv_data) {
		if (tx_wrap->flags == TX_EMULE_POW2) {
			/*convert video to a po of 2 WITHOUT SCALING VIDEO*/
			tx_wrap->conv_w = get_next_pow2(txh->width);
			tx_wrap->conv_h = get_next_pow2(txh->height);
			tx_wrap->conv_data = (char*)malloc(sizeof(char) * 3 * tx_wrap->conv_w * tx_wrap->conv_h);
			memset(tx_wrap->conv_data , 0, sizeof(char) * 3 * tx_wrap->conv_w * tx_wrap->conv_h);
			tx_wrap->conv_wscale = INT2FIX(txh->width) / tx_wrap->conv_w;
			tx_wrap->conv_hscale = INT2FIX(txh->height) / tx_wrap->conv_h;
		} else {
			tx_wrap->conv_data = (char*)malloc(sizeof(char) * 3 * txh->width * txh->height);
		}
	}
	out_stride = 3 * ((tx_wrap->flags & TX_EMULE_POW2) ? tx_wrap->conv_w : txh->width);

	dst.width = src.width = txh->width;
	dst.height = src.height = txh->height;
	dst.is_hardware_memory = src.is_hardware_memory = 0;

	src.pitch = txh->stride;
	src.pixel_format = txh->pixelformat;
	src.video_buffer = txh->data;

	dst.pitch = out_stride;
	tx_wrap->conv_format = dst.pixel_format = GF_PIXEL_RGB_24;
	dst.video_buffer = tx_wrap->conv_data;

	/*stretch and flip*/
	gf_stretch_bits(&dst, &src, NULL, NULL, 0, 0xFF, 1, NULL, NULL);
	tx_wrap->flags |= TX_NEEDS_HW_LOAD;
	return 1;
}

#endif


Bool render_texture_push_image(GF_TextureHandler *txh, Bool generate_mipmaps, Bool for2d)
{
#ifndef GPAC_DISABLE_3D
	char *data;
	Bool first_load = 0;
	GLint tx_mode;
	u32 pixel_format, w, h;
#endif
	TXWrapper *tx_wrap = (TXWrapper *) txh->hwtx;

	if (for2d) {
		Bool load_tx;
		if (!tx_wrap->tx_raster) {
			tx_wrap->tx_raster = txh->compositor->r2d->stencil_new(txh->compositor->r2d, GF_STENCIL_TEXTURE);
			if (!tx_wrap->tx_raster) return 0;
			load_tx = 1;
		}
		if (tx_wrap->flags & TX_NEEDS_RASTER_LOAD) {
			load_tx = 1;
			tx_wrap->flags &= ~TX_NEEDS_RASTER_LOAD;
		}
		if (load_tx) {
			if (txh->compositor->r2d->stencil_set_texture(tx_wrap->tx_raster, txh->data, txh->width, txh->height, txh->stride, (GF_PixelFormat) txh->pixelformat, (GF_PixelFormat) txh->compositor->video_out->pixel_format, 0) != GF_OK)
				return 0;
		}
		return 1;
	}

#ifndef GPAC_DISABLE_3D

	if (! (tx_wrap->flags & TX_NEEDS_HW_LOAD) ) return 1;

	/*in case the ID has been lost, resetup*/
	if (!tx_wrap->id) {
		glGenTextures(1, &tx_wrap->id);
		tx_setup_format(txh);
		first_load = 1;
	}
	if (!tx_wrap->gl_type) return 0;
	
	if (tx_wrap->flags & TX_EMULE_FIRST_LOAD) {
		tx_wrap->flags &= ~TX_EMULE_FIRST_LOAD;
		first_load = 1;
	}

	/*convert image*/
	tx_convert(txh);

	tx_bind(txh);

	tx_wrap->flags &= ~TX_NEEDS_HW_LOAD;
	data = tx_get_data(txh, &pixel_format);
	if (!data) return 0;
	if (tx_wrap->flags & TX_EMULE_POW2) {
		w = tx_wrap->conv_w;
		h = tx_wrap->conv_h;
	} else {
		w = txh->width;
		h = txh->height;
	}
	tx_mode = tx_wrap->nb_comp;
#ifdef GPAC_USE_OGL_ES
	tx_mode = tx_wrap->gl_format;
#endif

#ifdef GPAC_USE_TINYGL
	glTexImage2D(tx_wrap->gl_type, 0, tx_mode, w, h, 0, tx_wrap->gl_format, GL_UNSIGNED_BYTE, (unsigned char *) data);

#else

	/*pow2 texture or hardware support*/
	if (! (tx_wrap->flags & TX_MUST_SCALE) ) {
		if (first_load) {
			glTexImage2D(tx_wrap->gl_type, 0, tx_mode, w, h, 0, tx_wrap->gl_format, GL_UNSIGNED_BYTE, (unsigned char *) data);
		} else {
			glTexSubImage2D(tx_wrap->gl_type, 0, 0, 0, w, h, tx_wrap->gl_format, GL_UNSIGNED_BYTE, (unsigned char *) data);
		}
	} else {
#ifdef GPAC_USE_OGL_ES
		GF_VideoSurface src, dst;
		src.width = txh->width;
		src.height = txh->height;
		src.pitch = txh->stride;
		src.pixel_format = txh->pixelformat;
		src.video_buffer = txh->data;

		dst.width = tx_wrap->rescale_width;
		dst.height = tx_wrap->rescale_height;
		dst.pitch = tx_wrap->rescale_width*tx_wrap->nb_comp;
		dst.pixel_format = txh->pixelformat;
		dst.video_buffer = tx_wrap->scale_data;

		gf_stretch_bits(&dst, &src, NULL, NULL, 0, 0xFF, 0, NULL, NULL);
#else
		gluScaleImage(tx_wrap->gl_format, txh->width, txh->height, GL_UNSIGNED_BYTE, data, tx_wrap->rescale_width, tx_wrap->rescale_height, GL_UNSIGNED_BYTE, tx_wrap->scale_data);
#endif

		if (first_load) {
			glTexImage2D(tx_wrap->gl_type, 0, tx_mode, tx_wrap->rescale_width, tx_wrap->rescale_height, 0, tx_wrap->gl_format, GL_UNSIGNED_BYTE, tx_wrap->scale_data);
		} else {
			glTexSubImage2D(tx_wrap->gl_type, 0, 0, 0, tx_wrap->rescale_width, tx_wrap->rescale_height, tx_wrap->gl_format, GL_UNSIGNED_BYTE, tx_wrap->scale_data);
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
	return render_texture_push_image(txh, generate_mipmaps, 0);
}

#ifndef GPAC_USE_TINYGL
void tx_copy_to_texture(GF_TextureHandler *txh)
{
	TXWrapper *tx_wrap = (TXWrapper *) txh->hwtx;

	/*in case the ID has been lost, resetup*/
	if (!tx_wrap->id) {
		glGenTextures(1, &tx_wrap->id);
		tx_setup_format(txh);
	}
	
	tx_bind(txh);
	glCopyTexImage2D(tx_wrap->gl_type, 0, tx_wrap->gl_format, 0, 0, txh->width, txh->height, 0);
	glDisable(tx_wrap->gl_type);
}
#endif

#ifndef GPAC_USE_TINYGL

void tx_copy_to_stencil(GF_TextureHandler *txh)
{
	u32 i, hy;
	char *tmp;
	TXWrapper *tx_wrap = (TXWrapper *) txh->hwtx;

	/*in case the ID has been lost, resetup*/
	if (!txh->data || !tx_wrap->tx_raster) return;

	if (txh->pixelformat==GF_PIXEL_RGBA) {
		glReadPixels(0, 0, txh->width, txh->height, GL_RGBA, GL_UNSIGNED_BYTE, txh->data);
	} else if (txh->pixelformat==GF_PIXEL_RGB_24) {
		glReadPixels(0, 0, txh->width, txh->height, GL_RGB, GL_UNSIGNED_BYTE, txh->data);
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
#endif

#endif

Bool render_texture_get_transform(GF_TextureHandler *txh, GF_Node *tx_transform, GF_Matrix *mx)
{
	GF_Matrix tmp;
	Bool ret = 0;
	TXWrapper *tx_wrap = (TXWrapper *)txh->hwtx;
	gf_mx_init(*mx);

	/*WATCHOUT: this may be GL specific (GL_TEXTURE-RECTANGLE coords are w, h not 1.0, 1.0)*/
	if (tx_wrap->flags & TX_IS_RECT) {
		gf_mx_add_scale(mx, INT2FIX(txh->width), INT2FIX(txh->height), FIX_ONE);
		/*disable any texture transforms when using RECT textures (no repeat) ??*/
		/*tx_transform = NULL;*/
		ret = 1;
	} 
	else if (tx_wrap->flags & TX_EMULE_POW2) {
#ifndef GPAC_DISABLE_3D
		gf_mx_add_scale(mx, tx_wrap->conv_wscale, tx_wrap->conv_hscale, FIX_ONE);
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

static Bool tx_enable_matte_texture(GF_Node *n)
{
/*
	GF_TextureHandler *b_surf;
	GF_TextureHandler *a_surf;
	GF_TextureHandler *alpha_surf;
	M_MatteTexture *matte = (M_MatteTexture *)n;
	b_surf = redner_get_texture_handler(matte->surfaceB);
	if (!b_surf || !b_surf->hwtx) return 0;
	tx_set_image(b_surf, 0);
	tx_bind(b_surf);

	a_surf = redner_get_texture_handler(matte->surfaceA);
	alpha_surf = redner_get_texture_handler(matte->alphaSurface);

	if (!alpha_surf || !alpha_surf->hwtx) return 0;
	tx_set_image(alpha_surf, 0);
	tx_bind_with_mode(alpha_surf, render_texture_is_transparent(b_surf), ((TXWrapper *)b_surf->hwtx)->blend_mode);
	return 1;
*/
	return 0;
}

Bool render_texture_is_transparent(GF_TextureHandler *txh)
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

Bool tx_enable(GF_TextureHandler *txh, GF_Node *tx_transform)
{
	GF_Matrix mx;
	Render *sr = (Render *)txh->compositor->visual_renderer->user_priv;

	if (txh->matteTexture) {
		if (!tx_enable_matte_texture(txh->matteTexture)) return 0;
		visual_3d_set_matrix_mode(sr->visual, V3D_MATRIX_TEXTURE);
		if (render_texture_get_transform(txh, tx_transform, &mx)) 
			visual_3d_matrix_load(sr->visual, mx.m);
		else
			visual_3d_matrix_reset(sr->visual);
		visual_3d_set_matrix_mode(sr->visual, V3D_MATRIX_MODELVIEW);
		return 1;
	}
	if (!txh || !txh->hwtx) return 0;

	if (txh->compute_gradient_matrix && tx_needs_reload(txh) ) {
		render_gradient_update(txh);
	}
	tx_set_image(txh, 0);

	visual_3d_set_matrix_mode(sr->visual, V3D_MATRIX_TEXTURE);
	if (render_texture_get_transform(txh, tx_transform, &mx)) 
		visual_3d_matrix_load(sr->visual, mx.m);
	else
		visual_3d_matrix_reset(sr->visual);
	visual_3d_set_matrix_mode(sr->visual, V3D_MATRIX_MODELVIEW);

	tx_bind(txh);
	return 1;
}

#endif

Bool tx_needs_reload(GF_TextureHandler *hdl)
{
	return ( ((TXWrapper *)hdl->hwtx)->flags & TX_NEEDS_HW_LOAD ) ? 1 : 0;
}


GF_STENCIL render_texture_get_stencil(GF_TextureHandler *hdl)
{
	TXWrapper *tx_wrap = (TXWrapper *)hdl->hwtx;
	return tx_wrap->tx_raster;
}

void render_texture_set_stencil(GF_TextureHandler *hdl, GF_STENCIL stencil)
{
	TXWrapper *tx_wrap = (TXWrapper *)hdl->hwtx;
	tx_wrap->tx_raster = stencil;
	tx_wrap->flags |= TX_NEEDS_HW_LOAD;
}

