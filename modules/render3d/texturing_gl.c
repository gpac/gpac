/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 3D rendering module
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


#include "render3d_nodes.h"
#include "gl_inc.h"

/*tx flags*/
enum
{
	/*these 4 are exclusives*/
	TX_MUST_SCALE = (1<<1),
	TX_IS_POW2 = (1<<2),
	TX_IS_RECT = (1<<3),
	TX_EMULE_POW2 = (1<<4),
	/*signal video data must be sent to hw*/
	TX_NEEDS_HW_LOAD = (1<<5),
};

typedef struct
{
	/*opengl texture id*/
	u32 id;
	/*tx type*/
	u32 tx_flags;
	u32 blend_mode;
	Bool first_load;
	u32 rescale_width, rescale_height;
	char *scale_data;
	char *conv_data;
	Fixed conv_wscale, conv_hscale;
	u32 conv_format, conv_w, conv_h;

	/*gl textures vars (gl_type: 2D texture or rectangle (NV ext) )*/
	u32 nb_comp, gl_format, gl_type;
} GLTexture;

GF_Err tx_allocate(GF_TextureHandler *txh)
{
	GLTexture *gltx;
	if (txh->hwtx) return GF_OK;
	gltx = malloc(sizeof(GLTexture));
	if (!gltx) return GF_OUT_OF_MEM;
	txh->hwtx = gltx;
	memset(gltx, 0, sizeof(GLTexture));
	glGenTextures(1, &gltx->id);
	if (!gltx->id) return GF_IO_ERR;
	gltx->first_load = 1;
	return GF_OK;
}

void tx_delete(GF_TextureHandler *txh)
{
	if (txh->hwtx) {
		GLTexture *gltx = (GLTexture *) txh->hwtx;
		if (gltx->id) glDeleteTextures(1, &gltx->id);
		if (gltx->scale_data) free(gltx->scale_data);
		if (gltx->conv_data) free(gltx->conv_data);
		free(gltx);
		txh->hwtx = NULL;
	}
}

void tx_set_blend_mode(GF_TextureHandler *txh, u32 mode)
{
	if (txh->hwtx) {
		GLTexture *gltx = (GLTexture *) txh->hwtx;
		if (gltx) gltx->blend_mode = mode;
	}
}

void tx_bind(GF_TextureHandler *txh)
{
	GLTexture *gltx = (GLTexture *) txh->hwtx;
	if (!gltx->id || !gltx->gl_type) return;
	glEnable(gltx->gl_type);

	switch (gltx->blend_mode) {
	case TX_BLEND:
		if (txh->transparent) glEnable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
		break;
	case TX_REPLACE:
		if (txh->transparent) glEnable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		break;
	case TX_MODULATE:
		if (txh->transparent) glEnable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		break;
	case TX_DECAL:
	default:
		if ((gltx->gl_format==GL_LUMINANCE) || (gltx->gl_format==GL_LUMINANCE_ALPHA)) {
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		} else {
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		}
		break;
	}
	glBindTexture(gltx->gl_type, gltx->id);
}

void tx_disable(GF_TextureHandler *txh)
{
	GLTexture *gltx;
	if (txh && txh->hwtx) {
		gltx = (GLTexture *) txh->hwtx;
		glDisable(gltx->gl_type);
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

Bool tx_can_use_rect_ext(Render3D *sr, GF_TextureHandler *txh)
{
	u32 i, count;
	if (!sr->hw_caps.rect_texture) return 0;
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

Bool tx_setup_format(GF_TextureHandler *txh)
{
	Bool is_pow2, use_rect;
	Render3D *sr = (Render3D *)txh->compositor->visual_renderer->user_priv;
	GLTexture *gltx = (GLTexture *) txh->hwtx;

	/*first setup, this will force recompute bounds in case used with bitmap - we could refine and only 
	invalidate for bitmaps only*/
	if (txh->owner && (!gltx->rescale_width || !gltx->rescale_height)) 
		gf_node_dirty_set(txh->owner, 0, 1);

	gltx->rescale_width = get_pow2(txh->width);
	gltx->rescale_height = get_pow2(txh->height);

	is_pow2 = ((gltx->rescale_width==txh->width) && (gltx->rescale_height==txh->height)) ? 1 : 0;
	gltx->tx_flags = TX_IS_POW2;
	gltx->gl_type = GL_TEXTURE_2D;
	use_rect = tx_can_use_rect_ext(sr, txh);
	if (!is_pow2 && use_rect) {
		gltx->gl_type = GL_TEXTURE_RECTANGLE_EXT;
		gltx->tx_flags = TX_IS_RECT;
	}
	if (!use_rect && !sr->hw_caps.npot_texture && !is_pow2) gltx->tx_flags = TX_MUST_SCALE;

	gltx->nb_comp = gltx->gl_format = 0;
	switch (txh->pixelformat) {
	case GF_PIXEL_GREYSCALE:
		gltx->gl_format = GL_LUMINANCE;
		gltx->nb_comp = 1;
		gltx->gl_type = GL_TEXTURE_2D;
		if (!is_pow2) gltx->tx_flags = TX_MUST_SCALE;
		break;
	case GF_PIXEL_ALPHAGREY:
		gltx->gl_format = GL_LUMINANCE_ALPHA;
		gltx->nb_comp = 2;
		gltx->gl_type = GL_TEXTURE_2D;
		if (!is_pow2) gltx->tx_flags = TX_MUST_SCALE;
		break;
	case GF_PIXEL_RGB_24:
		gltx->gl_format = GL_RGB;
		gltx->nb_comp = 3;
		break;
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_RGBA:
		gltx->gl_format = GL_RGBA;
		gltx->nb_comp = 4;
		break;
	case GF_PIXEL_ARGB:
		if (!sr->hw_caps.bgra_texture) return 0;
		gltx->gl_format = GL_BGRA_EXT;
		gltx->nb_comp = 4;
		break;
	case GF_PIXEL_YV12:
		if (!use_rect && sr->emul_pow2) gltx->tx_flags = TX_EMULE_POW2;
		gltx->gl_format = GL_RGB;
		gltx->nb_comp = 3;
		break;
	default:
		return 0;
	}
	/*note we don't free the data if existing, since this only happen when re-setting up after context loss (same size)*/
	if ((gltx->tx_flags == TX_MUST_SCALE) & !gltx->scale_data) gltx->scale_data = malloc(sizeof(char) * gltx->nb_comp*gltx->rescale_width*gltx->rescale_height);

	glEnable(gltx->gl_type);
	glBindTexture(gltx->gl_type, gltx->id);

#ifdef GPAC_USE_OGL_ES
	glTexParameterx(gltx->gl_type, GL_TEXTURE_WRAP_S, (txh->flags & GF_SR_TEXTURE_REPEAT_S) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
	glTexParameterx(gltx->gl_type, GL_TEXTURE_WRAP_T, (txh->flags & GF_SR_TEXTURE_REPEAT_T) ? GL_REPEAT : GL_CLAMP_TO_EDGE);
	if (gltx->gl_type == GL_TEXTURE_2D) {
		glTexParameterx(gltx->gl_type, GL_TEXTURE_MAG_FILTER, txh->compositor->high_speed ? GL_NEAREST : GL_LINEAR);
		glTexParameterx(gltx->gl_type, GL_TEXTURE_MIN_FILTER, txh->compositor->high_speed ? GL_NEAREST : GL_LINEAR);
	} else {
		glTexParameterx(gltx->gl_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterx(gltx->gl_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
#else
	glTexParameteri(gltx->gl_type, GL_TEXTURE_WRAP_S, (txh->flags & GF_SR_TEXTURE_REPEAT_S) ? GL_REPEAT : GL_CLAMP);
	glTexParameteri(gltx->gl_type, GL_TEXTURE_WRAP_T, (txh->flags & GF_SR_TEXTURE_REPEAT_T) ? GL_REPEAT : GL_CLAMP);
	if (gltx->gl_type == GL_TEXTURE_2D) {
		glTexParameteri(gltx->gl_type, GL_TEXTURE_MAG_FILTER, txh->compositor->high_speed ? GL_NEAREST : GL_LINEAR);
		glTexParameteri(gltx->gl_type, GL_TEXTURE_MIN_FILTER, txh->compositor->high_speed ? GL_NEAREST : GL_LINEAR);
	} else {
		glTexParameteri(gltx->gl_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(gltx->gl_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
#endif
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glDisable(gltx->gl_type);
	gltx->first_load = 1;
	return 1;
}

char *tx_get_data(GF_TextureHandler *txh, u32 *pix_format)
{
	GLTexture *gltx = (GLTexture *) txh->hwtx;
	char *data = gltx->conv_data;
	*pix_format = gltx->conv_format;
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
	GLTexture *gltx = (GLTexture *) txh->hwtx;
	Render3D *sr = (Render3D *)txh->compositor->visual_renderer->user_priv;


	switch (txh->pixelformat) {
	case GF_PIXEL_ARGB:
		if (!sr->hw_caps.bgra_texture) return 0;
	case GF_PIXEL_GREYSCALE:
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_RGBA:
		if (txh->stream && !(txh->stream->mo_flags & GF_MO_IS_FLIP) ) {
			u32 i, hy;
			char *tmp;
			/*flip image*/
			tmp = malloc(sizeof(char)*txh->stride);
			hy = txh->height/2;
			for (i=0; i<hy; i++) {
				memcpy(tmp, txh->data + i*txh->stride, txh->stride);
				memcpy(txh->data + i*txh->stride, txh->data + (txh->height - 1 - i) * txh->stride, txh->stride);
				memcpy(txh->data + (txh->height - 1 - i) * txh->stride, tmp, txh->stride);
			}
			free(tmp);
			txh->stream->mo_flags |= GF_MO_IS_FLIP;
		}
		gltx->conv_format = txh->pixelformat;
		gltx->tx_flags |= TX_NEEDS_HW_LOAD;
		return 1;
	case GF_PIXEL_YV12:
		break;
	default:
		gltx->conv_format = 0;
		return 0;
	}
	if (!gltx->conv_data) {
		if (gltx->tx_flags == TX_EMULE_POW2) {
			/*convert video to a po of 2 WITHOUT SCALING VIDEO*/
			gltx->conv_w = get_next_pow2(txh->width);
			gltx->conv_h = get_next_pow2(txh->height);
			gltx->conv_data = malloc(sizeof(char) * 3 * gltx->conv_w * gltx->conv_h);
			memset(gltx->conv_data , 0, sizeof(char) * 3 * gltx->conv_w * gltx->conv_h);
			gltx->conv_wscale = INT2FIX(txh->width) / gltx->conv_w;
			gltx->conv_hscale = INT2FIX(txh->height) / gltx->conv_h;
		} else {
			gltx->conv_data = malloc(sizeof(char) * 3 * txh->width * txh->height);
		}
	}
	out_stride = 3 * ((gltx->tx_flags & TX_EMULE_POW2) ? gltx->conv_w : txh->width);

	dst.width = src.width = txh->width;
	dst.height = src.height = txh->height;
	dst.is_hardware_memory = src.is_hardware_memory = 0;

	src.pitch = txh->stride;
	src.pixel_format = txh->pixelformat;
	src.video_buffer = txh->data;

	dst.pitch = out_stride;
	gltx->conv_format = dst.pixel_format = GF_PIXEL_RGB_24;
	dst.video_buffer = gltx->conv_data;

	/*stretch and flip*/
	gf_stretch_bits(&dst, &src, NULL, NULL, 0, 0xFF, 1, NULL, NULL);
	gltx->tx_flags |= TX_NEEDS_HW_LOAD;
	return 1;
}

Bool tx_set_image(GF_TextureHandler *txh, Bool generate_mipmaps)
{
	char *data;
	GLint tx_mode;
	u32 pixel_format, w, h;
	GLTexture *gltx = (GLTexture *) txh->hwtx;
	if (! (gltx->tx_flags & TX_NEEDS_HW_LOAD) ) return 1;
	if (!gltx->gl_type) return 0;

	/*in case the ID has been lost, resetup*/
	if (!gltx->id) {
		glGenTextures(1, &gltx->id);
		tx_setup_format(txh);
	}
	tx_bind(txh);

	gltx->tx_flags &= ~TX_NEEDS_HW_LOAD;
	data = tx_get_data(txh, &pixel_format);
	if (!data) return 0;
	if (gltx->tx_flags & TX_EMULE_POW2) {
		w = gltx->conv_w;
		h = gltx->conv_h;
	} else {
		w = txh->width;
		h = txh->height;
	}
	tx_mode = gltx->nb_comp;
#ifdef GPAC_USE_OGL_ES
	tx_mode = gltx->gl_format;
#endif
	/*pow2 texture or hardware support*/
	if (! (gltx->tx_flags & TX_MUST_SCALE) ) {
		if (gltx->first_load) {
			gltx->first_load = 0;
			glTexImage2D(gltx->gl_type, 0, tx_mode, w, h, 0, gltx->gl_format, GL_UNSIGNED_BYTE, (unsigned char *) data);
		} else {
			glTexSubImage2D(gltx->gl_type, 0, 0, 0, w, h, gltx->gl_format, GL_UNSIGNED_BYTE, (unsigned char *) data);
		}
	} else {
#ifdef GPAC_USE_OGL_ES
		GF_VideoSurface src, dst;
		src.width = txh->width;
		src.height = txh->height;
		src.pitch = txh->stride;
		src.pixel_format = txh->pixelformat;
		src.video_buffer = txh->data;

		dst.width = gltx->rescale_width;
		dst.height = gltx->rescale_height;
		dst.pitch = gltx->rescale_width*gltx->nb_comp;
		dst.pixel_format = txh->pixelformat;
		dst.video_buffer = gltx->scale_data;

		gf_stretch_bits(&dst, &src, NULL, NULL, 0, 0xFF, 0, NULL, NULL);
#else
		gluScaleImage(gltx->gl_format, txh->width, txh->height, GL_UNSIGNED_BYTE, data, gltx->rescale_width, gltx->rescale_height, GL_UNSIGNED_BYTE, gltx->scale_data);
#endif

		if (gltx->first_load) {
			gltx->first_load = 0;
			glTexImage2D(gltx->gl_type, 0, tx_mode, gltx->rescale_width, gltx->rescale_height, 0, gltx->gl_format, GL_UNSIGNED_BYTE, gltx->scale_data);
		} else {
			glTexSubImage2D(gltx->gl_type, 0, 0, 0, gltx->rescale_width, gltx->rescale_height, gltx->gl_format, GL_UNSIGNED_BYTE, gltx->scale_data);
		}
	}
	return 1;
}

void tx_copy_to_texture(GF_TextureHandler *txh)
{
	GLTexture *gltx = (GLTexture *) txh->hwtx;
	tx_bind(txh);
	glCopyTexImage2D(gltx->gl_type, 0, gltx->gl_format, 0, 0, txh->width, txh->height, 0);
	glDisable(gltx->gl_type);
}


Bool tx_get_transform(GF_TextureHandler *txh, GF_Node *tx_transform, GF_Matrix *mx)
{
	GF_Matrix tmp;
	Bool ret = 0;
	GLTexture *gltx = (GLTexture *)txh->hwtx;
	gf_mx_init(*mx);

	/*WATCHOUT: this may be GL specific (GL_TEXTURE-RECTANGLE coords are w, h not 1.0, 1.0)*/
	if (gltx->tx_flags & TX_IS_RECT) {
		gf_mx_add_scale(mx, INT2FIX(txh->width), INT2FIX(txh->height), FIX_ONE);
		/*disable any texture transforms when using RECT textures (no repeat) ??*/
		/*tx_transform = NULL;*/
		ret = 1;
	} 
	else if (gltx->tx_flags & TX_EMULE_POW2) {
		gf_mx_add_scale(mx, gltx->conv_wscale, gltx->conv_hscale, FIX_ONE);
		/*disable any texture transforms when emulating pow2 textures*/
		tx_transform = NULL;
		ret = 1;
	}

	if (tx_transform) {
		switch (gf_node_get_tag(tx_transform)) {
		case TAG_MPEG4_TextureTransform:
		case TAG_X3D_TextureTransform:
		{
			GF_Matrix2D mat;
			M_TextureTransform *tt = (M_TextureTransform *)tx_transform;
			gf_mx2d_init(mat);
			gf_mx2d_add_translation(&mat, -tt->center.x, -tt->center.y);
			gf_mx2d_add_scale(&mat, tt->scale.x, tt->scale.y);
			if (fabs(tt->rotation) > GF_EPSILON_FLOAT) gf_mx2d_add_rotation(&mat, 0, 0, tt->rotation);
			gf_mx2d_add_translation(&mat, tt->translation.x + tt->center.x, tt->translation.y + tt->center.y);
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

Bool tx_enable(GF_TextureHandler *txh, GF_Node *tx_transform)
{
	GF_Matrix mx;
	Render3D *sr;
	GLTexture *gltx;
	if (!txh || !txh->hwtx) return 0;
	tx_set_image(txh, 0);
	sr = (Render3D *)txh->compositor->visual_renderer->user_priv;
	gltx = (GLTexture *)txh->hwtx;

	VS3D_SetMatrixMode(sr->surface, MAT_TEXTURE);
	if (tx_get_transform(txh, tx_transform, &mx)) 
		VS3D_LoadMatrix(sr->surface, mx.m);
	else
		VS3D_ResetMatrix(sr->surface);
		
	VS3D_SetMatrixMode(sr->surface, MAT_MODELVIEW);

	tx_bind(txh);
	return 1;
}

GF_Err R3D_SetTextureData(GF_TextureHandler *hdl)
{
	GLTexture *gltx = (GLTexture *)hdl->hwtx;

	/*init if needed*/
	if (!gltx->gl_type) {
		if (!tx_setup_format(hdl)) return GF_NOT_SUPPORTED;
	}
	/*convert image - don't push it to HW until used*/
	tx_convert(hdl);
	return GF_OK;
}

void R3D_TextureHWReset(GF_TextureHandler *hdl)
{
	GLTexture *gltx = (GLTexture *)hdl->hwtx;

	if (gltx->id) {
		glDeleteTextures(1, &gltx->id);
		gltx->id = 0;
	}
	gltx->tx_flags |= TX_NEEDS_HW_LOAD;
}

Bool tx_needs_reload(GF_TextureHandler *hdl)
{
	return ( ((GLTexture *)hdl->hwtx)->tx_flags & TX_NEEDS_HW_LOAD ) ? 1 : 0;
}

