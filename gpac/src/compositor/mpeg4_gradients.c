/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

#include "nodes_stacks.h"
#include "texturing.h"

#if !defined(GPAC_DISABLE_COMPOSITOR)


#ifndef GPAC_DISABLE_VRML


#define GRAD_TEXTURE_SIZE	128
#define GRAD_TEXTURE_HSIZE	64

enum
{
	GF_SR_TEXTURE_GRAD_REGISTERED = 1<<6,
	GF_SR_TEXTURE_GRAD_NO_RGB = 1<<7,
};

/*linear/radial gradient*/
typedef struct
{
	GF_TextureHandler txh;
	char *tx_data;
//	Bool no_rgb_support;
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
			mat->m[0] = tm->mxx;
			mat->m[1] = tm->mxy;
			mat->m[2] = tm->tx;
			mat->m[3] = tm->myx;
			mat->m[4] = tm->myy;
			mat->m[5] = tm->ty;
		}
		break;
		default:
			break;
		}
	}
}

static void DestroyGradient(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		GradientStack *st = (GradientStack *) gf_node_get_private(node);
		gf_sc_texture_destroy(&st->txh);
		if (st->tx_data) gf_free(st->tx_data);
		gf_free(st);
	}
}

static void UpdateLinearGradient(GF_TextureHandler *txh)
{
	u32 i, *cols;
	Fixed a;
	Bool const_a;
	GF_EVGStencil *stencil;
	M_LinearGradient *lg = (M_LinearGradient *) txh->owner;
	GradientStack *st = (GradientStack *) gf_node_get_private(txh->owner);

	if (!gf_node_dirty_get(txh->owner)) {
		txh->needs_refresh = 0;
		return;
	}
	if (lg->key.count > lg->keyValue.count) return;

	if (!txh->tx_io) {
		gf_node_dirty_set(gf_node_get_parent(txh->owner, 0), 0, 1);
		gf_node_dirty_set(txh->owner, 0, 1);
		gf_sc_texture_allocate(txh);
	}

	stencil = gf_sc_texture_get_stencil(txh);
	if (!stencil) stencil = gf_evg_stencil_new(GF_STENCIL_LINEAR_GRADIENT);
	/*set stencil even if assigned, this invalidates the associated bitmap state in 3D*/
	gf_sc_texture_set_stencil(txh, stencil);

	gf_node_dirty_clear(txh->owner, 0);
	txh->needs_refresh = 1;

	st->txh.transparent = 0;
	const_a = (lg->opacity.count == 1) ? 1 : 0;
	cols = (u32*)gf_malloc(sizeof(u32) * lg->key.count);
	for (i=0; i<lg->key.count; i++) {
		a = (const_a ? lg->opacity.vals[0] : lg->opacity.vals[i]);
		cols[i] = GF_COL_ARGB_FIXED(a, lg->keyValue.vals[i].red, lg->keyValue.vals[i].green, lg->keyValue.vals[i].blue);
		if (a != FIX_ONE) txh->transparent = 1;
	}
	gf_evg_stencil_set_gradient_interpolation(stencil, lg->key.vals, cols, lg->key.count);
	gf_free(cols);
	gf_evg_stencil_set_gradient_mode(stencil, (GF_GradientMode) lg->spreadMethod);
}


static void LG_ComputeMatrix(GF_TextureHandler *txh, GF_Rect *bounds, GF_Matrix2D *mat, Bool for_3d)
{
	GF_EVGStencil *stencil;
	M_LinearGradient *lg = (M_LinearGradient *) txh->owner;

	stencil = gf_sc_texture_get_stencil(txh);
	if (!stencil) return;

	if (lg->key.count<2) return;
	if (lg->key.count != lg->keyValue.count) return;

	/*create gradient brush if needed*/
	if (!txh->tx_io) return;

	GradientGetMatrix((GF_Node *) lg->transform, mat);

	/*translate to the center of the bounds*/
	gf_mx2d_add_translation(mat, gf_divfix(bounds->x, bounds->width), gf_divfix(bounds->y - bounds->height, bounds->height));
	/*scale back to object coordinates - the gradient is still specified in texture coordinates
	in order to avoid overflows in fixed point*/
	gf_mx2d_add_scale(mat, bounds->width, bounds->height);

	gf_evg_stencil_set_linear_gradient(stencil, lg->startPoint.x, lg->startPoint.y, lg->endPoint.x, lg->endPoint.y);
}

//TODO - replace this with shader-based code ...

static void BuildLinearGradientTexture(GF_TextureHandler *txh)
{
	u32 i;
	SFVec2f start, end;
	u32 *cols;
	Fixed a;
	Bool const_a;
	GF_Matrix2D mat;
	GF_EVGStencil * stenc;
	GF_EVGSurface *surface;
	GF_EVGStencil * texture2D;
	GF_Path *path;
	GF_Err e;
	Bool transparent;
	u32 pix_fmt = 0;
	M_LinearGradient *lg = (M_LinearGradient *) txh->owner;
	GradientStack *st = (GradientStack *) gf_node_get_private(txh->owner);

	if (!txh->tx_io) return;

	if (!(txh->flags & GF_SR_TEXTURE_GRAD_REGISTERED)) {
		txh->flags |= GF_SR_TEXTURE_GRAD_REGISTERED;
		if (gf_list_find(txh->compositor->textures, txh)<0)
			gf_list_insert(txh->compositor->textures, txh, 0);
	}

	if (lg->key.count<2) return;
	if (lg->key.count != lg->keyValue.count) return;

	start = lg->startPoint;
	end = lg->endPoint;

	transparent = (lg->opacity.count==1) ? (lg->opacity.vals[0]!=FIX_ONE) : 1;

	/*init our 2D graphics stuff*/
	texture2D = gf_evg_stencil_new(GF_STENCIL_TEXTURE);
	if (!texture2D) return;
	surface = gf_evg_surface_new(1);
	if (!surface) {
		gf_evg_stencil_delete(texture2D);
		return;
	}

	if (st->txh.flags & GF_SR_TEXTURE_GRAD_NO_RGB) transparent = 1;

	if (st->tx_data && (st->txh.transparent != transparent)) {
		gf_free(st->tx_data);
		st->tx_data = NULL;
	}

	if (transparent) {
		if (!st->tx_data) {
			st->tx_data = (char *) gf_malloc(sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*4);
		}
		memset(st->tx_data, 0, sizeof(char)*txh->stride*txh->height);

		pix_fmt = GF_PIXEL_RGBA;
		e = gf_evg_stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 4*GRAD_TEXTURE_SIZE, pix_fmt);
		if (e) {
			pix_fmt = GF_PIXEL_ARGB;
			e = gf_evg_stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 4*GRAD_TEXTURE_SIZE, pix_fmt);
		}
	} else {
		if (!st->tx_data) {
			st->tx_data = (char *) gf_malloc(sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*3);
		}
		e = gf_evg_stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 3*GRAD_TEXTURE_SIZE, GF_PIXEL_RGB);
		/*try with ARGB (it actually is needed for GDIplus module since GDIplus cannot handle native RGB texture (it works in BGR)*/
		if (e) {
			/*remember for later use*/
			st->txh.flags |= GF_SR_TEXTURE_GRAD_NO_RGB;
			transparent = 1;
			gf_free(st->tx_data);
			st->tx_data = (char *) gf_malloc(sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*4);
			e = gf_evg_stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 4*GRAD_TEXTURE_SIZE, GF_PIXEL_ARGB);
		}
	}
	st->txh.transparent = transparent;

	if (e) {
		gf_free(st->tx_data);
		gf_evg_stencil_delete(texture2D);
		gf_evg_surface_delete(surface);
		return;
	}
	e = gf_evg_surface_attach_to_texture(surface, texture2D);
	if (e) {
		gf_evg_stencil_delete(texture2D);
		gf_evg_surface_delete(surface);
		return;
	}

	/*create & setup gradient*/
	stenc = gf_evg_stencil_new(GF_STENCIL_LINEAR_GRADIENT);
	if (!stenc) {
		gf_evg_stencil_delete(texture2D);
		gf_evg_surface_delete(surface);
		return;
	}
	/*move line to object space*/
	start.x *= GRAD_TEXTURE_SIZE;
	end.x *= GRAD_TEXTURE_SIZE;
	start.y *= GRAD_TEXTURE_SIZE;
	end.y *= GRAD_TEXTURE_SIZE;
	gf_evg_stencil_set_linear_gradient(stenc, start.x, start.y, end.x, end.y);
	const_a = (lg->opacity.count == 1) ? 1 : 0;
	cols = (u32*)gf_malloc(sizeof(u32) * lg->key.count);
	for (i=0; i<lg->key.count; i++) {
		a = (const_a ? lg->opacity.vals[0] : lg->opacity.vals[i]);
		cols[i] = GF_COL_ARGB_FIXED(a, lg->keyValue.vals[i].red, lg->keyValue.vals[i].green, lg->keyValue.vals[i].blue);
	}
	gf_evg_stencil_set_gradient_interpolation(stenc, lg->key.vals, cols, lg->key.count);
	gf_free(cols);
	gf_evg_stencil_set_gradient_mode(stenc, (GF_GradientMode)lg->spreadMethod);

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
	gf_evg_stencil_set_matrix(stenc, &mat);
	gf_evg_stencil_set_auto_matrix(stenc, GF_FALSE);

	gf_evg_surface_set_raster_level(surface, GF_RASTER_HIGH_QUALITY);
	gf_evg_surface_set_path(surface, path);
	gf_evg_surface_fill(surface, stenc);
	gf_evg_stencil_delete(stenc);
	gf_evg_surface_delete(surface);
	gf_evg_stencil_delete(texture2D);
	gf_path_del(path);

	txh->data = st->tx_data;
	txh->width = GRAD_TEXTURE_SIZE;
	txh->height = GRAD_TEXTURE_SIZE;
	txh->transparent = transparent;
	if (transparent) {
		u32 j;
		txh->stride = GRAD_TEXTURE_SIZE*4;
		txh->pixelformat = GF_PIXEL_RGBA;

		/*back to RGBA texturing*/
		if (pix_fmt != GF_PIXEL_RGBA) {
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
		}
	} else {
		txh->stride = GRAD_TEXTURE_SIZE*3;
		txh->pixelformat = GF_PIXEL_RGB;
	}
	txh->flags |= GF_SR_TEXTURE_NO_GL_FLIP;
	gf_sc_texture_set_data(txh);
}

void compositor_init_linear_gradient(GF_Compositor *compositor, GF_Node *node)
{
	GradientStack *st;
	GF_SAFEALLOC(st, GradientStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate gradient stack\n"));
		return;
	}

	/*!!! Gradients are textures but are not registered as textures with the compositor in order to avoid updating
	too many textures each frame - gradients are only registered with the compositor when they are used in OpenGL, in order
	to release associated HW resource when no longer used*/
	st->txh.owner = node;
	st->txh.compositor = compositor;
	st->txh.update_texture_fcnt = UpdateLinearGradient;
	st->txh.compute_gradient_matrix = LG_ComputeMatrix;

	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, DestroyGradient);
}


static void BuildRadialGradientTexture(GF_TextureHandler *txh)
{
	u32 i;
	SFVec2f center, focal;
	u32 *cols;
	Fixed a, radius;
	Bool const_a;
	GF_Matrix2D mat;
	GF_EVGStencil * stenc;
	GF_EVGSurface *surface;
	GF_EVGStencil * texture2D;
	GF_Path *path;
	GF_Err e;
	u32 pix_fmt = 0;
	Bool transparent;
	M_RadialGradient *rg = (M_RadialGradient*) txh->owner;
	GradientStack *st = (GradientStack *) gf_node_get_private(txh->owner);

	if (!txh->tx_io) return;

	if (!(txh->flags & GF_SR_TEXTURE_GRAD_REGISTERED)) {
		txh->flags |= GF_SR_TEXTURE_GRAD_REGISTERED;
		if (gf_list_find(txh->compositor->textures, txh)<0)
			gf_list_insert(txh->compositor->textures, txh, 0);
	}

	if (rg->key.count<2) return;
	if (rg->key.count != rg->keyValue.count) return;

	transparent = (rg->opacity.count==1) ? ((rg->opacity.vals[0]!=FIX_ONE) ? 1 : 0) : 1;

	/*init our 2D graphics stuff*/
	texture2D = gf_evg_stencil_new(GF_STENCIL_TEXTURE);
	if (!texture2D) return;
	surface = gf_evg_surface_new(1);
	if (!surface) {
		gf_evg_stencil_delete(texture2D);
		return;
	}

	if (st->txh.flags & GF_SR_TEXTURE_GRAD_NO_RGB) transparent = 1;

	if (st->tx_data && (st->txh.transparent != transparent)) {
		gf_free(st->tx_data);
		st->tx_data = NULL;
	}

	if (transparent) {
		if (!st->tx_data) {
			st->tx_data = (char *) gf_malloc(sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*4);
		}
		memset(st->tx_data, 0, sizeof(char)*txh->stride*txh->height);

		pix_fmt = GF_PIXEL_RGBA;

		e = gf_evg_stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 4*GRAD_TEXTURE_SIZE, pix_fmt);

		if (e) {
			pix_fmt = GF_PIXEL_ARGB;
			e = gf_evg_stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 4*GRAD_TEXTURE_SIZE, pix_fmt);
		}
	} else {
		if (!st->tx_data) {
			st->tx_data = (char *) gf_malloc(sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*3);
		}
		e = gf_evg_stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 3*GRAD_TEXTURE_SIZE, GF_PIXEL_RGB);
		/*try with ARGB (it actually is needed for GDIplus module since GDIplus cannot handle native RGB texture (it works in BGR)*/
		if (e) {
			/*remember for later use*/
			st->txh.flags |= GF_SR_TEXTURE_GRAD_NO_RGB;
			transparent = 1;
			gf_free(st->tx_data);
			st->tx_data = (char *) gf_malloc(sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*4);
			e = gf_evg_stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 4*GRAD_TEXTURE_SIZE, GF_PIXEL_ARGB);
		}
	}
	st->txh.transparent = transparent;

	if (e) {
		gf_free(st->tx_data);
		gf_evg_stencil_delete(texture2D);
		gf_evg_surface_delete(surface);
		return;
	}
	e = gf_evg_surface_attach_to_texture(surface, texture2D);
	if (e) {
		gf_evg_stencil_delete(texture2D);
		gf_evg_surface_delete(surface);
		return;
	}

	/*create & setup gradient*/
	stenc = gf_evg_stencil_new(GF_STENCIL_RADIAL_GRADIENT);
	if (!stenc) {
		gf_evg_stencil_delete(texture2D);
		gf_evg_surface_delete(surface);
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

	gf_evg_stencil_set_radial_gradient(stenc, center.x, center.y, focal.x, focal.y, radius, radius);

	const_a = (rg->opacity.count == 1) ? 1 : 0;
	cols = (u32*) gf_malloc(sizeof(u32) * rg->key.count);
	for (i=0; i<rg->key.count; i++) {
		a = (const_a ? rg->opacity.vals[0] : rg->opacity.vals[i]);
		cols[i] = GF_COL_ARGB_FIXED(a, rg->keyValue.vals[i].red, rg->keyValue.vals[i].green, rg->keyValue.vals[i].blue);
	}
	gf_evg_stencil_set_gradient_interpolation(stenc, rg->key.vals, cols, rg->key.count);
	gf_free(cols);
	gf_evg_stencil_set_gradient_mode(stenc, (GF_GradientMode)rg->spreadMethod);

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
	gf_evg_stencil_set_matrix(stenc, &mat);
	gf_evg_stencil_set_auto_matrix(stenc, GF_FALSE);

	gf_evg_surface_set_raster_level(surface, GF_RASTER_HIGH_QUALITY);
	gf_evg_surface_set_path(surface, path);
	gf_evg_surface_fill(surface, stenc);
	gf_evg_stencil_delete(stenc);
	gf_evg_surface_delete(surface);
	gf_evg_stencil_delete(texture2D);
	gf_path_del(path);

	txh->data = st->tx_data;
	txh->width = GRAD_TEXTURE_SIZE;
	txh->height = GRAD_TEXTURE_SIZE;
	txh->transparent = transparent;
	if (transparent) {
		u32 j;
		txh->stride = GRAD_TEXTURE_SIZE*4;
		txh->pixelformat = GF_PIXEL_RGBA;

		/*back to RGBA texturing*/
		if (pix_fmt == GF_PIXEL_ARGB) {
			for (i=0; i<txh->height; i++) {
				char *data = txh->data + i*txh->stride;
				for (j=0; j<txh->width; j++) {
					u8 pa, pr, pg, pb;
					pa = data[4*j];
					pr = data[4*j+1];
					pg = data[4*j+2];
					pb = data[4*j+3];
					data[4*j] = pr;
					data[4*j+1] = pg;
					data[4*j+2] = pb;
					data[4*j+3] = pa;
				}
			}
		}
	} else {
		txh->stride = GRAD_TEXTURE_SIZE*3;
		txh->pixelformat = GF_PIXEL_RGB;
	}
//	tx_set_blend_enable(txh, 1);
	txh->flags |= GF_SR_TEXTURE_NO_GL_FLIP;
	gf_sc_texture_set_data(txh);
	return;
}

static void UpdateRadialGradient(GF_TextureHandler *txh)
{
	Bool const_a;
	GF_EVGStencil * stencil;
	u32 i, *cols;
	Fixed a;
	M_RadialGradient *rg = (M_RadialGradient*) txh->owner;
	GradientStack *st = (GradientStack *) gf_node_get_private(txh->owner);

	if (!gf_node_dirty_get(txh->owner)) {
		txh->needs_refresh = 0;
		return;
	}
	if (rg->key.count > rg->keyValue.count) return;

	if (!txh->tx_io) gf_sc_texture_allocate(txh);

	stencil = gf_sc_texture_get_stencil(txh);
	if (!stencil) stencil = gf_evg_stencil_new(GF_STENCIL_RADIAL_GRADIENT);
	/*set stencil even if assigned, this invalidates the associated bitmap state in 3D*/
	gf_sc_texture_set_stencil(txh, stencil);

	gf_node_dirty_clear(txh->owner, 0);
	txh->needs_refresh = 1;

	st->txh.transparent = 0;
	for (i=0; i<rg->opacity.count; i++) {
		if (rg->opacity.vals[i] != FIX_ONE) {
			st->txh.transparent = 1;
			break;
		}
	}

	const_a = (rg->opacity.count == 1) ? 1 : 0;
	cols = (u32*)gf_malloc(sizeof(u32) * rg->key.count);
	for (i=0; i<rg->key.count; i++) {
		a = (const_a ? rg->opacity.vals[0] : rg->opacity.vals[i]);
		cols[i] = GF_COL_ARGB_FIXED(a, rg->keyValue.vals[i].red, rg->keyValue.vals[i].green, rg->keyValue.vals[i].blue);
	}
	gf_evg_stencil_set_gradient_interpolation(stencil, rg->key.vals, cols, rg->key.count);
	gf_free(cols);

	gf_evg_stencil_set_gradient_mode(stencil, (GF_GradientMode) rg->spreadMethod);

}

static void RG_ComputeMatrix(GF_TextureHandler *txh, GF_Rect *bounds, GF_Matrix2D *mat, Bool for_3d)
{
	GF_EVGStencil * stencil;
	M_RadialGradient *rg = (M_RadialGradient *) txh->owner;

	if (rg->key.count<2) return;
	if (rg->key.count != rg->keyValue.count) return;

	if (!txh->tx_io) return;

	stencil = gf_sc_texture_get_stencil(txh);
	if (!stencil) return;

	GradientGetMatrix((GF_Node *) rg->transform, mat);

	gf_evg_stencil_set_radial_gradient(stencil, rg->center.x, rg->center.y, rg->focalPoint.x, rg->focalPoint.y, rg->radius, rg->radius);

	/*move to center of bounds*/
	gf_mx2d_add_translation(mat, gf_divfix(bounds->x, bounds->width), gf_divfix(bounds->y - bounds->height, bounds->height));
	/*scale back to object coordinates - the gradient is still specified in texture coordinates
	in order to avoid overflows in fixed point*/
	gf_mx2d_add_scale(mat, bounds->width, bounds->height);
}


void compositor_init_radial_gradient(GF_Compositor *compositor, GF_Node *node)
{
	GradientStack *st;
	GF_SAFEALLOC(st, GradientStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate gradient stack\n"));
		return;
	}

	/*!!! Gradients are textures but are not registered as textures with the compositor in order to avoid updating
	too many textures each frame - gradients are only registered with the compositor when they are used in OpenGL, in order
	to release associated HW resource when no longer used*/
	st->txh.owner = node;
	st->txh.compositor = compositor;
	st->txh.update_texture_fcnt = UpdateRadialGradient;
	st->txh.compute_gradient_matrix = RG_ComputeMatrix;

	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, DestroyGradient);
}

GF_TextureHandler *compositor_mpeg4_get_gradient_texture(GF_Node *node)
{
	GradientStack *st = (GradientStack*) gf_node_get_private(node);
	st->txh.update_texture_fcnt(&st->txh);
	return &st->txh;
}

#endif /*GPAC_DISABLE_VRML*/

#if !defined(GPAC_DISABLE_COMPOSITOR)

void compositor_gradient_update(GF_TextureHandler *txh)
{
	switch (gf_node_get_tag(txh->owner) ) {
#ifndef GPAC_DISABLE_VRML
	case TAG_MPEG4_RadialGradient:
		BuildRadialGradientTexture(txh);
		break;
	case TAG_MPEG4_LinearGradient:
		BuildLinearGradientTexture(txh);
		break;
#endif

#ifndef GPAC_DISABLE_SVG
	case TAG_SVG_linearGradient:
	case TAG_SVG_radialGradient:
		compositor_svg_build_gradient_texture(txh);
		break;
#endif
	}
}

#endif

#endif //!defined(GPAC_DISABLE_COMPOSITOR)
