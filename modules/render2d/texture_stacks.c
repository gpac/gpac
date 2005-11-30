/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D rendering module
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



#include "stacks2d.h"
#include "visualsurface2d.h"

typedef struct _composite_2D
{
	GF_TextureHandler txh;
	u32 width, height;
	/*the surface object handling the texture*/
	struct _visual_surface_2D *surf;
	GF_List *sensors;
	Bool first;
} Composite2DStack;

static void DestroyComposite2D(GF_Node *node)
{
	Composite2DStack *st = (Composite2DStack *) gf_node_get_private(node);
	/*unregister surface*/
	R2D_UnregisterSurface(st->surf->render, st->surf);
	DeleteVisualSurface2D(st->surf);
	gf_list_del(st->sensors);
	/*destroy texture*/
	gf_sr_texture_destroy(&st->txh);
	free(st);
}


static Bool Composite_CheckBindables(GF_Node *n, RenderEffect2D *eff, Bool force_check)
{
	GF_Node *btop;
	Bool ret = 0;
	M_CompositeTexture2D *c2d = (M_CompositeTexture2D *)n;
	if (force_check || gf_node_dirty_get(c2d->background)) { gf_node_render(c2d->background, eff); ret = 1; }
	btop = gf_list_get(eff->back_stack, 0);
	if (btop != c2d->background) {
		gf_node_unregister(c2d->background, n);
		gf_node_register(btop, n); 
		c2d->background = btop;
		gf_node_event_out_str(n, "background");
		ret = 1;
	}

	if (force_check || gf_node_dirty_get(c2d->viewport)) { gf_node_render(c2d->viewport, eff); ret = 1; }
	btop = gf_list_get(eff->view_stack, 0);
	if (btop != c2d->viewport) { 
		gf_node_unregister(c2d->viewport, n);
		gf_node_register(btop, n); 
		c2d->viewport = btop;
		gf_node_event_out_str(n, "viewport");
		ret = 1;
	}
	return ret;
}

static void UpdateComposite2D(GF_TextureHandler *txh)
{
	GF_Err e;
	u32 count;
	u32 i;
	SensorHandler *hsens;
	GF_Node *child;
	RenderEffect2D *eff;

	M_CompositeTexture2D *ct2D = (M_CompositeTexture2D *)txh->owner;
	Composite2DStack *st = (Composite2DStack *) gf_node_get_private(txh->owner);
	GF_Raster2D *r2d = st->surf->render->compositor->r2d;

	/*rebuild stencil*/
	if (!st->surf->the_surface || !txh->hwtx || ((s32) st->width != ct2D->pixelWidth) || ( (s32) st->height != ct2D->pixelHeight) ) {
		if (txh->hwtx) r2d->stencil_delete(txh->hwtx);
		txh->hwtx = NULL;

		if (ct2D->pixelWidth<=0) return;
		if (ct2D->pixelHeight<=0) return;
		st->width = ct2D->pixelWidth;
		st->height = ct2D->pixelHeight;

		txh->hwtx = r2d->stencil_new(r2d, GF_STENCIL_TEXTURE);
		e = r2d->stencil_create_texture(txh->hwtx, st->width, st->height, GF_PIXEL_ARGB);
		if (e) {
			if (txh->hwtx) r2d->stencil_delete(txh->hwtx);
			txh->hwtx = NULL;
		}
	}
	if (!txh->hwtx) return;

	eff = malloc(sizeof(RenderEffect2D));
	memset(eff, 0, sizeof(RenderEffect2D));
	eff->sensors = gf_list_new();
	eff->surface = st->surf;

	if (st->surf->render->top_effect->trav_flags & TF_RENDER_DIRECT) {
		eff->trav_flags = TF_RENDER_DIRECT;
	}


	gf_mx2d_init(eff->transform);
	gf_cmx_init(&eff->color_mat);
	st->surf->width = st->width;
	st->surf->height = st->height;
	eff->back_stack = st->surf->back_stack;
	eff->view_stack = st->surf->view_stack;
	eff->is_pixel_metrics = gf_sg_use_pixel_metrics(gf_node_get_graph(st->txh.owner));
	eff->min_hsize = INT2FIX( MIN(st->width, st->height) ) / 2;

	Composite_CheckBindables(st->txh.owner, eff, st->first);
	st->first = 0;
	
	VS2D_InitDraw(st->surf, eff);

	/*render children*/
	count = gf_list_count(ct2D->children);
	if (gf_node_dirty_get(st->txh.owner) & GF_SG_NODE_DIRTY) {
		/*rebuild sensor list */
		if (gf_list_count(st->sensors)) {
			gf_list_del(st->sensors);
			st->sensors = gf_list_new();
		}
		for (i=0; i<count; i++) {
			child = gf_list_get(ct2D->children, i);
			if (!child || !is_sensor_node(child) ) continue;
			hsens = get_sensor_handler(child);
			if (hsens) gf_list_add(st->sensors, hsens);
		}

		/*if we have an active sensor at this level discard all sensors in current render context (cf VRML)*/
		if (gf_list_count(st->sensors)) {
			effect_reset_sensors(eff);
		}
	}

	/*add sensor to effects*/	
	for (i=0; i <gf_list_count(st->sensors); i++) {
		SensorHandler *hsens = gf_list_get(st->sensors, i);
		effect_add_sensor(eff, hsens, &eff->transform);
	}

	gf_node_dirty_clear(st->txh.owner, 0);

	/*render*/
	gf_node_render_children(st->txh.owner, eff);

	/*finalize draw*/
	txh->needs_refresh = VS2D_TerminateDraw(st->surf, eff);
	st->txh.transparent = st->surf->last_had_back ? 0 : 1;
/*
	st->txh.active_window.x = 0;
	st->txh.active_window.y = 0;
	st->txh.active_window.width = st->width;
	st->txh.active_window.height = st->height;
*/
	st->txh.width = st->width;
	st->txh.height = st->height;

	/*set active viewport in image coordinates top-left=(0, 0), not in BIFS*/
	if (gf_list_count(st->surf->view_stack)) {
		M_Viewport *vp = gf_list_get(st->surf->view_stack, 0);

		if (vp->isBound) {
			SFVec2f size = vp->size;
			if (size.x >=0 && size.y>=0) {
/*
				st->txh.active_window.width = size.x;
				st->txh.active_window.height = size.y;
				st->txh.active_window.x = (st->width - size.x) / 2;
				st->txh.active_window.y = (st->height - size.y) / 2;
*/
				/*FIXME - we need tracking of VP changes*/
				txh->needs_refresh = 1;
			}
		}
	} 

	if (txh->needs_refresh && r2d->stencil_gf_sr_texture_modified) r2d->stencil_gf_sr_texture_modified(st->txh.hwtx); 
	/*always invalidate to make sure the frame counter is incremented, otherwise we will
	break bounds tracking*/
	gf_sr_invalidate(st->txh.compositor, NULL);
	effect_delete(eff);
}

static GF_Err C2D_GetSurfaceAccess(VisualSurface2D *surf)
{
	GF_Err e;
	if (!surf->composite->txh.hwtx || !surf->the_surface) return GF_BAD_PARAM;
	e = surf->render->compositor->r2d->surface_attach_to_texture(surf->the_surface, surf->composite->txh.hwtx);
	if (!e) surf->is_attached = 1;
	return e;
}

static void C2D_ReleaseSurfaceAccess(VisualSurface2D *surf)
{
	surf->render->compositor->r2d->surface_detach(surf->the_surface);
}

void R2D_InitCompositeTexture2D(Render2D *sr, GF_Node *node)
{
	Composite2DStack *st = malloc(sizeof(Composite2DStack));
	memset(st, 0, sizeof(Composite2DStack));
	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = UpdateComposite2D;

	st->txh.flags = GF_SR_TEXTURE_COMPOSITE;
	//st->txh.flags |= GF_SR_TEXTURE_REPEAT_S | GF_SR_TEXTURE_REPEAT_T;

	/*create composite surface*/
	st->surf = NewVisualSurface2D();
	st->surf->composite = st;
	st->surf->GetSurfaceAccess = C2D_GetSurfaceAccess;
	st->surf->ReleaseSurfaceAccess = C2D_ReleaseSurfaceAccess;

	/*Bitmap drawn with brush, not hardware since we don't know how the graphics driver handles the texture bytes*/
	st->surf->DrawBitmap = NULL;
	st->surf->SupportsFormat = NULL;
	st->first = 1;
	st->surf->render = sr;
	st->sensors = gf_list_new();
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyComposite2D);
	R2D_RegisterSurface(sr, st->surf);

}

GF_TextureHandler *ct2D_get_texture(GF_Node *node)
{
	Composite2DStack *st = (Composite2DStack*) gf_node_get_private(node);
	return &st->txh;
}


Bool CT2D_has_sensors(GF_TextureHandler *txh)
{
	Composite2DStack *st = (Composite2DStack *) gf_node_get_private(txh->owner);
	assert(st->surf);
	return gf_list_count(st->surf->sensors) ? 1 : 0;
}

void get_gf_sr_texture_transform(GF_Node *__appear, GF_TextureHandler *txh, GF_Matrix2D *mat, Bool line_texture, Fixed final_width, Fixed final_height);

DrawableContext *CT2D_FindNode(GF_TextureHandler *txh, DrawableContext *ctx, Fixed x, Fixed y)
{
	GF_Rect rc;
	GF_Matrix2D mat, tx_trans;
	Fixed width, height;
	Composite2DStack *st = (Composite2DStack *) gf_node_get_private(txh->owner);
	assert(st->surf);

	rc = ctx->original;

	gf_mx2d_init(mat);
	gf_mx2d_add_scale(&mat, rc.width / st->width, rc.height / st->height);
	get_gf_sr_texture_transform(ctx->appear, &st->txh, &tx_trans, (ctx->h_texture==&st->txh) ? 0 : 1, INT2FIX(ctx->original.width), INT2FIX(ctx->original.height));
	gf_mx2d_add_matrix(&mat, &tx_trans);
	gf_mx2d_add_translation(&mat, (rc.x), (rc.y - rc.height));
	gf_mx2d_add_matrix(&mat, &ctx->transform);

	gf_mx2d_inverse(&mat);
	gf_mx2d_apply_coords(&mat, &x, &y);

	width = INT2FIX(st->width);
	height = INT2FIX(st->height);
	while (x>width) x -= width;
	while (x < 0) x += width;
	while (y>height) y -= height;
	while (y < 0) y += height;
	x -= width / 2;
	y -= height / 2;

	return VS2D_PickSensitiveNode(st->surf, x, y);
}


GF_Node *CT2D_PickNode(GF_TextureHandler *txh, DrawableContext *ctx, Fixed x, Fixed y)
{
	GF_Rect rc;
	GF_Matrix2D mat, tx_trans;
	Fixed width, height;
	Composite2DStack *st = (Composite2DStack *) gf_node_get_private(txh->owner);
	assert(st->surf);

	rc = ctx->original;

	gf_mx2d_init(mat);
	gf_mx2d_add_scale(&mat, rc.width / st->width, rc.height / st->height);
	get_gf_sr_texture_transform(ctx->appear, &st->txh, &tx_trans, (ctx->h_texture==&st->txh) ? 0 : 1, INT2FIX(ctx->original.width), INT2FIX(ctx->original.height));
	gf_mx2d_add_matrix(&mat, &tx_trans);
	gf_mx2d_add_translation(&mat, (rc.x), (rc.y - rc.height));
	gf_mx2d_add_matrix(&mat, &ctx->transform);

	gf_mx2d_inverse(&mat);
	gf_mx2d_apply_coords(&mat, &x, &y);

	width = INT2FIX(st->width);
	height = INT2FIX(st->height);
	while (x>width) x -= width;
	while (x < 0) x += width;
	while (y>height) y -= height;
	while (y < 0) y += height;
	x -= width / 2;
	y -= height / 2;

	return VS2D_PickNode(st->surf, x, y);
}


static void GradientGetMatrix(GF_Node *transform, GF_Matrix2D *mat)
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
			TM2D_GetMatrix(transform, mat);
			break;
		default:
			break;
		}
	}
}

typedef struct
{
	GF_TextureHandler txh;
} GradientStack;

/*
		linear gradient
*/

static void DestroyLinearGradient(GF_Node *node)
{
	GradientStack *st = (GradientStack *) gf_node_get_private(node);
	gf_sr_texture_destroy(&st->txh);
	free(st);
}

static void UpdateLinearGradient(GF_TextureHandler *txh)
{
	u32 i, *cols;
	Fixed a;
	Bool const_a;
	M_LinearGradient *lg = (M_LinearGradient *) txh->owner;
	GradientStack *st = (GradientStack *) gf_node_get_private(txh->owner);
	if (!txh->hwtx) txh->hwtx = txh->compositor->r2d->stencil_new(txh->compositor->r2d, GF_STENCIL_LINEAR_GRADIENT);

	if (!gf_node_dirty_get(txh->owner)) return;
	gf_node_dirty_clear(txh->owner, 0);

	txh->needs_refresh = 1;

	st->txh.transparent = 0;
	const_a = (lg->opacity.count == 1) ? 1 : 0;
	cols = malloc(sizeof(u32) * lg->key.count);
	for (i=0; i<lg->key.count; i++) {
		a = (const_a ? lg->opacity.vals[0] : lg->opacity.vals[i]);
		cols[i] = GF_COL_ARGB_FIXED(a, lg->keyValue.vals[i].red, lg->keyValue.vals[i].green, lg->keyValue.vals[i].blue);
		if (a != FIX_ONE) txh->transparent = 1;
	}
	txh->compositor->r2d->stencil_set_gradient_interpolation(txh->hwtx, lg->key.vals, cols, lg->key.count);
	free(cols);
	txh->compositor->r2d->stencil_set_gradient_mode(txh->hwtx, lg->spreadMethod);

}

static void LG_ComputeMatrix(GF_TextureHandler *txh, GF_Rect *bounds, GF_Matrix2D *mat)
{
	SFVec2f start, end;
	M_LinearGradient *lg = (M_LinearGradient *) txh->owner;

	if (lg->key.count<2) return;
	if (lg->key.count != lg->keyValue.count) return;

	start = lg->startPoint;
	end = lg->endPoint;

	/*create gradient brush if needed*/
	if (!txh->hwtx) return;

	GradientGetMatrix((GF_Node *) lg->transform, mat);
	
	/*move line to object space*/
	start.x = gf_mulfix(start.x, bounds->width);
	end.x = gf_mulfix(end.x, bounds->width);
	start.y = gf_mulfix(start.y, bounds->height);
	end.y = gf_mulfix(end.y, bounds->height);

	/*move transform to object space*/
	mat->m[2] = gf_mulfix(mat->m[2], bounds->width);
	mat->m[5] = gf_mulfix(mat->m[5], bounds->height);
	mat->m[1] = gf_muldiv(mat->m[1], bounds->width, bounds->height);
	mat->m[3] = gf_muldiv(mat->m[3], bounds->height, bounds->width);

	/*translate to the center of the bounds*/
	gf_mx2d_add_translation(mat, bounds->x, bounds->y - bounds->height);

	txh->compositor->r2d->stencil_set_linear_gradient(txh->hwtx, start.x, start.y, end.x, end.y);
}

void R2D_InitLinearGradient(Render2D *sr, GF_Node *node)
{
	GradientStack *st = malloc(sizeof(GradientStack));
	memset(st, 0, sizeof(GradientStack));

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = UpdateLinearGradient;

	st->txh.compute_gradient_matrix = LG_ComputeMatrix;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyLinearGradient);
}

GF_TextureHandler *r2d_lg_get_texture(GF_Node *node)
{
	GradientStack *st = (GradientStack*) gf_node_get_private(node);
	return &st->txh;
}


/*
		radial gradient
*/


static void DestroyRadialGradient(GF_Node *node)
{
	GradientStack *st = (GradientStack *) gf_node_get_private(node);
	gf_sr_texture_destroy(&st->txh);
	free(st);
}

static void UpdateRadialGradient(GF_TextureHandler *txh)
{
	u32 i;
	M_RadialGradient *rg = (M_RadialGradient*) txh->owner;
	GradientStack *st = (GradientStack *) gf_node_get_private(txh->owner);
	if (!txh->hwtx) txh->hwtx = txh->compositor->r2d->stencil_new(txh->compositor->r2d, GF_STENCIL_RADIAL_GRADIENT);

	if (!gf_node_dirty_get(txh->owner)) return;
	gf_node_dirty_clear(txh->owner, 0);
	txh->needs_refresh = 1;

	st->txh.transparent = 0;
	for (i=0; i<rg->opacity.count; i++) {
		if (rg->opacity.vals[i] != FIX_ONE) {
			st->txh.transparent = 1;
			break;
		}
	}
}

static void RG_ComputeMatrix(GF_TextureHandler *txh, GF_Rect *bounds, GF_Matrix2D *mat)
{
	SFVec2f center, focal;
	u32 i, *cols;
	Fixed a;
	Bool const_a;
	M_RadialGradient *rg = (M_RadialGradient *) txh->owner;

	if (rg->key.count<2) return;
	if (rg->key.count != rg->keyValue.count) return;

	/*create gradient brush if needed*/
	if (!txh->hwtx) return;

	GradientGetMatrix((GF_Node *) rg->transform, mat);

	center = rg->center;
	focal = rg->focalPoint;

	/*move circle to object space*/
	center.x = gf_mulfix(center.x, bounds->width);
	center.y = gf_mulfix(center.y, bounds->height);
	focal.x = gf_mulfix(focal.x, bounds->width);
	focal.y = gf_mulfix(focal.y, bounds->height);

	/*move transform to object space*/
	mat->m[2] = gf_mulfix(mat->m[2], bounds->width);
	mat->m[5] = gf_mulfix(mat->m[5], bounds->height);
	mat->m[1] = gf_muldiv(mat->m[1], bounds->width, bounds->height);
	mat->m[3] = gf_muldiv(mat->m[3], bounds->height, bounds->width);

	
	txh->compositor->r2d->stencil_set_radial_gradient(txh->hwtx, center.x, center.y, focal.x, focal.y, gf_mulfix(rg->radius, bounds->width), gf_mulfix(rg->radius, bounds->height));

	const_a = (rg->opacity.count == 1) ? 1 : 0;
	cols = malloc(sizeof(u32) * rg->key.count);
	for (i=0; i<rg->key.count; i++) {
		a = (const_a ? rg->opacity.vals[0] : rg->opacity.vals[i]);
		cols[i] = GF_COL_ARGB_FIXED(a, rg->keyValue.vals[i].red, rg->keyValue.vals[i].green, rg->keyValue.vals[i].blue);
	}
	txh->compositor->r2d->stencil_set_gradient_interpolation(txh->hwtx, rg->key.vals, cols, rg->key.count);
	free(cols);

	txh->compositor->r2d->stencil_set_gradient_mode(txh->hwtx, rg->spreadMethod);
	gf_mx2d_add_translation(mat, bounds->x, bounds->y - bounds->height);
}

void R2D_InitRadialGradient(Render2D *sr, GF_Node *node)
{
	GradientStack *st = malloc(sizeof(GradientStack));
	memset(st, 0, sizeof(GradientStack));

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = UpdateRadialGradient;

	st->txh.compute_gradient_matrix = RG_ComputeMatrix;

	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyRadialGradient);
}

GF_TextureHandler *r2d_rg_get_texture(GF_Node *node)
{
	GradientStack *st = (GradientStack*) gf_node_get_private(node);
	return &st->txh;
}


void R2D_InitMatteTexture(Render2D *sr, GF_Node *node)
{
}

GF_TextureHandler *r2d_matte_get_texture(GF_Node *node)
{
	return NULL;
}

void R2D_MatteTextureModified(GF_Node *node)
{
}

GF_TextureHandler *R2D_GetTextureHandler(GF_Node *n)
{
	if (!n) return NULL;
	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_CompositeTexture2D: return ct2D_get_texture(n);
	case TAG_MPEG4_MatteTexture: return r2d_matte_get_texture(n);
	case TAG_MPEG4_LinearGradient: return r2d_lg_get_texture(n);
	case TAG_MPEG4_RadialGradient: return r2d_rg_get_texture(n);
	default: return gf_sr_texture_get_handler(n);
	}
}
