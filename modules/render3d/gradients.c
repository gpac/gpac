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


#define GRAD_TEXTURE_SIZE	128
#define GRAD_TEXTURE_HSIZE	64

/*linear/radial gradient*/
typedef struct
{
	GF_TextureHandler txh;
	char *tx_data;
	Bool transparent;
	Bool no_rgb_support;
} GradientStack;

void GradientGetMatrix(GF_Node *transform, GF_Matrix2D *mat)
{
	gf_mx2d_init(*mat);
	if (transform) {
		switch (gf_node_get_tag(transform) ) {
		case TAG_MPEG4_Transform2D:
		{
			M_Transform2D *tr = (M_Transform2D *)transform;
			gf_mx2d_add_scale_at(mat, 0, 0, tr->scale.x, tr->scale.y, tr->scaleOrientation);
			gf_mx2d_add_rotation(mat, tr->center.x, tr->center.y, tr->rotationAngle);
			gf_mx2d_add_translation(mat, tr->translation.x, tr->translation.y);
		}
			break;
		case TAG_MPEG4_TransformMatrix2D:
		{
			M_TransformMatrix2D *tm = (M_TransformMatrix2D*)transform;
			gf_mx2d_init(*mat);
			mat->m[0] = tm->mxx; mat->m[1] = tm->mxy; mat->m[2] = tm->tx;
			mat->m[3] = tm->myx; mat->m[4] = tm->myy; mat->m[5] = tm->ty;
		}
			break;
		default:
			break;
		}
	}
}

static void DestroyLinearGradient(GF_Node *node)
{
	GradientStack *st = (GradientStack *) gf_node_get_private(node);
	gf_sr_texture_destroy(&st->txh);
	if (st->tx_data) free(st->tx_data);
	free(st);
}

static void UpdateLinearGradient(GF_TextureHandler *txh)
{
	u32 i;
	SFVec2f start, end;
	u32 *cols;
	Fixed a;
	Bool const_a;
	GF_Matrix2D mat;
	GF_STENCIL stenc;
	GF_SURFACE surf;
	GF_STENCIL texture2D;
	GF_Path *path;
	GF_Err e;
	Bool transparent;
	M_LinearGradient *lg = (M_LinearGradient *) txh->owner;
	GradientStack *st = (GradientStack *) gf_node_get_private(txh->owner);
	GF_Raster2D *r2d = txh->compositor->r2d;

	if (!txh->hwtx) gf_node_dirty_set(txh->owner, 0, 0);
	if (!gf_node_dirty_get(txh->owner)) return;
	gf_node_dirty_clear(txh->owner, 0);

	if (!txh->hwtx) tx_allocate(txh);
	if (st->tx_data) {
		free(st->tx_data);
		st->tx_data = NULL;
	}

	if (lg->key.count<2) return;
	if (lg->key.count != lg->keyValue.count) return;

	start = lg->startPoint;
	end = lg->endPoint;

	transparent = (lg->opacity.count==1) ? (lg->opacity.vals[0]!=FIX_ONE) : 1;
	
	/*init our 2D graphics stuff*/
	texture2D = r2d->stencil_new(r2d, GF_STENCIL_TEXTURE);
	if (!texture2D) return;
	surf = r2d->surface_new(r2d, 1);
	if (!surf) {
		r2d->stencil_delete(texture2D);
		return;
	}

	if (st->no_rgb_support) transparent = 1;
	if (st->tx_data && (st->transparent != transparent)) {
		free(st->tx_data);
		st->tx_data = NULL;
	}
	
	if (transparent) {
		if (!st->tx_data) {
			SAFEALLOC(st->tx_data, sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*4);
		} else {
			memset(st->tx_data, 0, sizeof(char)*txh->stride*txh->height);
		}
		e = r2d->stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 4*GRAD_TEXTURE_SIZE, GF_PIXEL_ARGB, GF_PIXEL_ARGB, 1);
	} else {
		if (!st->tx_data) {
			SAFEALLOC(st->tx_data, sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*3);
		}
		e = r2d->stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 3*GRAD_TEXTURE_SIZE, GF_PIXEL_RGB_24, GF_PIXEL_RGB_24, 1);
		/*try with ARGB (it actually is needed for GDIplus module since GDIplus cannot handle native RGB texture (it works in BGR)*/
		if (e) {
			/*remember for later use*/
			st->no_rgb_support = 1;
			transparent = 1;
			free(st->tx_data);
			SAFEALLOC(st->tx_data, sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*4);
			e = r2d->stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 4*GRAD_TEXTURE_SIZE, GF_PIXEL_ARGB, GF_PIXEL_ARGB, 1);
		}
	}
	st->transparent = transparent;

	if (e) {
		free(st->tx_data);
		r2d->stencil_delete(texture2D);
		r2d->surface_delete(surf);
		return;
	}
	e = r2d->surface_attach_to_texture(surf, texture2D);
	if (e) {
		r2d->stencil_delete(texture2D);
		r2d->surface_delete(surf);
		return;
	}

	/*create & setup gradient*/
	stenc = r2d->stencil_new(r2d, GF_STENCIL_LINEAR_GRADIENT);
	if (!stenc) {
		r2d->stencil_delete(texture2D);
		r2d->surface_delete(surf);
	}
	/*move line to object space*/
	start.x *= GRAD_TEXTURE_SIZE;
	end.x *= GRAD_TEXTURE_SIZE;
	start.y *= GRAD_TEXTURE_SIZE;
	end.y *= GRAD_TEXTURE_SIZE;
	r2d->stencil_set_linear_gradient(stenc, start.x, start.y, end.x, end.y, 0xFFFF0000, 0xFFFF00FF);
	const_a = (lg->opacity.count == 1) ? 1 : 0;
	cols = malloc(sizeof(u32) * lg->key.count);
	for (i=0; i<lg->key.count; i++) {
		a = (const_a ? lg->opacity.vals[0] : lg->opacity.vals[i]);
		cols[i] = GF_COL_ARGB_FIXED(a, lg->keyValue.vals[i].red, lg->keyValue.vals[i].green, lg->keyValue.vals[i].blue);
	}
	r2d->stencil_set_gradient_interpolation(stenc, lg->key.vals, cols, lg->key.count);
	free(cols);
	r2d->stencil_set_gradient_mode(stenc, lg->spreadMethod);

	/*fill surface*/
	path = gf_path_new();
	gf_path_add_move_to(path, -INT2FIX(GRAD_TEXTURE_HSIZE), -INT2FIX(GRAD_TEXTURE_HSIZE));
	gf_path_add_line_to(path, INT2FIX(GRAD_TEXTURE_HSIZE), -INT2FIX(GRAD_TEXTURE_HSIZE));
	gf_path_add_line_to(path, INT2FIX(GRAD_TEXTURE_HSIZE), INT2FIX(GRAD_TEXTURE_HSIZE));
	gf_path_add_line_to(path, -INT2FIX(GRAD_TEXTURE_HSIZE), INT2FIX(GRAD_TEXTURE_HSIZE));
	gf_path_close(path);

	/*add gradient transform*/
	GradientGetMatrix(lg->transform, &mat);
	/*move transform to object space*/
	mat.m[2] *= GRAD_TEXTURE_SIZE;
	mat.m[5] *= GRAD_TEXTURE_SIZE;
	/*translate to the center of the bounds*/
	gf_mx2d_add_translation(&mat, -INT2FIX(GRAD_TEXTURE_HSIZE), -INT2FIX(GRAD_TEXTURE_HSIZE));
	/*back to GL bottom->up order*/
	gf_mx2d_add_scale(&mat, FIX_ONE, -FIX_ONE);
	r2d->stencil_set_matrix(stenc, &mat);

	r2d->surface_set_raster_level(surf, GF_RASTER_HIGH_QUALITY);
	r2d->surface_set_path(surf, path);
	r2d->surface_fill(surf, stenc);
	r2d->stencil_delete(stenc);
	r2d->surface_delete(surf);
	r2d->stencil_delete(texture2D);
	gf_path_del(path);

	txh->data = st->tx_data;
	txh->width = GRAD_TEXTURE_SIZE;
	txh->height = GRAD_TEXTURE_SIZE;
	txh->transparent = transparent;
	if (transparent) {
		u32 j;
		txh->stride = GRAD_TEXTURE_SIZE*4;

		/*back to RGBA texturing*/
		txh->pixelformat = GF_PIXEL_RGBA;
		for (i=0; i<txh->height; i++) {
			char *data = txh->data + i*txh->stride;
			for (j=0; j<txh->width; j++) {
				u32 val = *(u32 *) &data[4*j];
				data[4*j] = (val>>16) & 0xFF;
				data[4*j+1] = (val>>8) & 0xFF;
				data[4*j+2] = (val) & 0xFF;
				data[4*j+3] = (val>>24) & 0xFF;
			}
		}
	} else {
		txh->stride = GRAD_TEXTURE_SIZE*3;
		txh->pixelformat = GF_PIXEL_RGB_24;
	}
	R3D_SetTextureData(txh);
}

void R3D_InitLinearGradient(Render3D *sr, GF_Node *node)
{
	GradientStack *st = malloc(sizeof(GradientStack));
	memset(st, 0, sizeof(GradientStack));
	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_gf_sr_texture_fcnt = UpdateLinearGradient;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyLinearGradient);
}

GF_TextureHandler *r3d_lg_get_texture(GF_Node *node)
{
	GradientStack *st = (GradientStack*) gf_node_get_private(node);
	return &st->txh;
}


static void DestroyRadialGradient(GF_Node *node)
{
	GradientStack *st = (GradientStack *) gf_node_get_private(node);
	gf_sr_texture_destroy(&st->txh);
	if (st->tx_data) free(st->tx_data);
	free(st);
}

static void UpdateRadialGradient(GF_TextureHandler *txh)
{
	u32 i;
	SFVec2f center, focal;
	u32 *cols;
	Fixed a, radius;
	Bool const_a;
	GF_Matrix2D mat;
	GF_STENCIL stenc;
	GF_SURFACE surf;
	GF_STENCIL texture2D;
	GF_Path *path;
	GF_Err e;
	Bool transparent;
	M_RadialGradient *rg = (M_RadialGradient*) txh->owner;
	GradientStack *st = (GradientStack *) gf_node_get_private(txh->owner);
	GF_Raster2D *r2d = txh->compositor->r2d;

	if (!txh->hwtx) gf_node_dirty_set(txh->owner, 0, 0);
	if (!gf_node_dirty_get(txh->owner)) return;
	gf_node_dirty_clear(txh->owner, 0);

	if (!txh->hwtx) tx_allocate(txh);
	if (st->tx_data) {
		free(st->tx_data);
		st->tx_data = NULL;
	}

	if (rg->key.count<2) return;
	if (rg->key.count != rg->keyValue.count) return;

	transparent = (rg->opacity.count==1) ? ((rg->opacity.vals[0]!=FIX_ONE) ? 1 : 0) : 1;
	
	/*init our 2D graphics stuff*/
	texture2D = r2d->stencil_new(r2d, GF_STENCIL_TEXTURE);
	if (!texture2D) return;
	surf = r2d->surface_new(r2d, 1);
	if (!surf) {
		r2d->stencil_delete(texture2D);
		return;
	}

	if (st->no_rgb_support) transparent = 1;
	if (st->tx_data && (st->transparent != transparent)) {
		free(st->tx_data);
		st->tx_data = NULL;
	}
	
	if (transparent) {
		if (!st->tx_data) {
			SAFEALLOC(st->tx_data, sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*4);
		} else {
			memset(st->tx_data, 0, sizeof(char)*txh->stride*txh->height);
		}
		e = r2d->stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 4*GRAD_TEXTURE_SIZE, GF_PIXEL_ARGB, GF_PIXEL_ARGB, 1);
	} else {
		if (!st->tx_data) {
			SAFEALLOC(st->tx_data, sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*3);
		}
		e = r2d->stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 3*GRAD_TEXTURE_SIZE, GF_PIXEL_RGB_24, GF_PIXEL_RGB_24, 1);
		/*try with ARGB (it actually is needed for GDIplus module since GDIplus cannot handle native RGB texture (it works in BGR)*/
		if (e) {
			/*remember for later use*/
			st->no_rgb_support = 1;
			transparent = 1;
			free(st->tx_data);
			SAFEALLOC(st->tx_data, sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*4);
			e = r2d->stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 4*GRAD_TEXTURE_SIZE, GF_PIXEL_ARGB, GF_PIXEL_ARGB, 1);
		}
	}
	st->transparent = transparent;

	if (e) {
		free(st->tx_data);
		r2d->stencil_delete(texture2D);
		r2d->surface_delete(surf);
		return;
	}
	e = r2d->surface_attach_to_texture(surf, texture2D);
	if (e) {
		r2d->stencil_delete(texture2D);
		r2d->surface_delete(surf);
		return;
	}

	/*create & setup gradient*/
	stenc = r2d->stencil_new(r2d, GF_STENCIL_RADIAL_GRADIENT);
	if (!stenc) {
		r2d->stencil_delete(texture2D);
		r2d->surface_delete(surf);
	}

	center = rg->center;
	focal = rg->focalPoint;
	radius = rg->radius;

	/*move circle to object space*/
	center.x *= GRAD_TEXTURE_SIZE;
	center.y *= GRAD_TEXTURE_SIZE;
	focal.x *= GRAD_TEXTURE_SIZE;
	focal.y *= GRAD_TEXTURE_SIZE;
	radius *= GRAD_TEXTURE_SIZE;

	r2d->stencil_set_radial_gradient(stenc, center.x, center.y, focal.x, focal.y, radius, radius);

	const_a = (rg->opacity.count == 1) ? 1 : 0;
	cols = malloc(sizeof(u32) * rg->key.count);
	for (i=0; i<rg->key.count; i++) {
		a = (const_a ? rg->opacity.vals[0] : rg->opacity.vals[i]);
		cols[i] = GF_COL_ARGB_FIXED(a, rg->keyValue.vals[i].red, rg->keyValue.vals[i].green, rg->keyValue.vals[i].blue);
	}
	r2d->stencil_set_gradient_interpolation(stenc, rg->key.vals, cols, rg->key.count);
	free(cols);
	r2d->stencil_set_gradient_mode(stenc, rg->spreadMethod);

	/*fill surface*/
	path = gf_path_new();
	gf_path_add_move_to(path, -INT2FIX(GRAD_TEXTURE_HSIZE), -INT2FIX(GRAD_TEXTURE_HSIZE));
	gf_path_add_line_to(path, INT2FIX(GRAD_TEXTURE_HSIZE), -INT2FIX(GRAD_TEXTURE_HSIZE));
	gf_path_add_line_to(path, INT2FIX(GRAD_TEXTURE_HSIZE), INT2FIX(GRAD_TEXTURE_HSIZE));
	gf_path_add_line_to(path, -INT2FIX(GRAD_TEXTURE_HSIZE), INT2FIX(GRAD_TEXTURE_HSIZE));
	gf_path_close(path);

	/*add gradient transform*/
	GradientGetMatrix(rg->transform, &mat);
	/*move transform to object space*/
	mat.m[2] *= GRAD_TEXTURE_SIZE;
	mat.m[5] *= GRAD_TEXTURE_SIZE;
	/*translate to the center of the bounds*/
	gf_mx2d_add_translation(&mat, -INT2FIX(GRAD_TEXTURE_HSIZE), -INT2FIX(GRAD_TEXTURE_HSIZE));
	/*back to GL bottom->up order*/
	gf_mx2d_add_scale(&mat, FIX_ONE, -FIX_ONE);
	r2d->stencil_set_matrix(stenc, &mat);

	r2d->surface_set_raster_level(surf, GF_RASTER_HIGH_QUALITY);
	r2d->surface_set_path(surf, path);
	r2d->surface_fill(surf, stenc);
	r2d->stencil_delete(stenc);
	r2d->surface_delete(surf);
	r2d->stencil_delete(texture2D);
	gf_path_del(path);

	txh->data = st->tx_data;
	txh->width = GRAD_TEXTURE_SIZE;
	txh->height = GRAD_TEXTURE_SIZE;
	txh->transparent = transparent;
	if (transparent) {
		u32 j;
		txh->stride = GRAD_TEXTURE_SIZE*4;

		/*back to RGBA texturing*/
		txh->pixelformat = GF_PIXEL_RGBA;
		for (i=0; i<txh->height; i++) {
			char *data = txh->data + i*txh->stride;
			for (j=0; j<txh->width; j++) {
				u32 val = *(u32 *) &data[4*j];
				data[4*j] = (val>>16) & 0xFF;
				data[4*j+1] = (val>>8) & 0xFF;
				data[4*j+2] = (val) & 0xFF;
				data[4*j+3] = (val>>24) & 0xFF;
			}
		}
	} else {
		txh->stride = GRAD_TEXTURE_SIZE*3;
		txh->pixelformat = GF_PIXEL_RGB_24;
	}
//	tx_set_blend_enable(txh, 1);
	R3D_SetTextureData(txh);
	return;
}

void R3D_InitRadialGradient(Render3D *sr, GF_Node *node)
{
	GradientStack *st = malloc(sizeof(GradientStack));
	memset(st, 0, sizeof(GradientStack));
	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_gf_sr_texture_fcnt = UpdateRadialGradient;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyRadialGradient);
}

GF_TextureHandler *r3d_rg_get_texture(GF_Node *node)
{
	GradientStack *st = (GradientStack*) gf_node_get_private(node);
	return &st->txh;
}
