/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D+3D Rendering sub-project
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

#include "visual_manager.h"

#ifndef GPAC_DISABLE_SVG
#include "node_stacks.h"
#include "texturing.h"



typedef struct
{
	GF_TextureHandler txh;
	char *tx_data;
	Bool no_rgb_support;
/*	u32 *cols;
	Fixed *keys;
	u32 nb_col;
*/
} SVG_GradientStack;


static void SVG_DestroyPaintServer(GF_Node *node)
{
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(node);
	if (st) {
		gf_sr_texture_destroy(&st->txh);
		free(st);
	}
}


static GF_Node *svg_copy_gradient_attributes_from(GF_Node *node, SVGAllAttributes *all_atts)
{
	GF_Node *href_node;
	SVGAllAttributes all_href_atts;
	GF_FieldInfo info;

	/*check gradient redirection ...*/
	href_node = node;
	while (href_node && gf_svg_get_attribute_by_tag(href_node, TAG_SVG_ATT_xlink_href, 0, 0, &info)==GF_OK) {
		href_node = ((XMLRI*)info.far_ptr)->target;
		if (href_node == node) href_node = NULL;
	}
	if (href_node == node) href_node = NULL;

	if (href_node) {
		gf_svg_flatten_attributes((SVG_Element *)href_node, &all_href_atts);
		if (!all_atts->gradientUnits) all_atts->gradientUnits = all_href_atts.gradientUnits;
		if (!all_atts->gradientTransform) all_atts->gradientTransform = all_href_atts.gradientTransform;
		if (!all_atts->cx) all_atts->cx = all_href_atts.cx;
		if (!all_atts->cy) all_atts->cy = all_href_atts.cy;
		if (!all_atts->r) all_atts->r = all_href_atts.r;
		if (!all_atts->fx) all_atts->fx = all_href_atts.fx;
		if (!all_atts->fy) all_atts->fy = all_href_atts.fy;
		if (!all_atts->spreadMethod) all_atts->spreadMethod = all_href_atts.spreadMethod;
		if (!all_atts->x1) all_atts->x1 = all_href_atts.x1;
		if (!all_atts->x2) all_atts->x2 = all_href_atts.x2;
		if (!all_atts->y1) all_atts->y1 = all_href_atts.y1;
		if (!all_atts->y2) all_atts->y2 = all_href_atts.y2;
	}

	return href_node;
}

static void SVG_UpdateGradient(SVG_GradientStack *st, GF_ChildNodeItem *children, Bool linear)
{
	GF_STENCIL stencil;
	u32 count;
	Fixed alpha, max_offset;
	SVGAllAttributes all_atts;
	u32 *cols;
	Fixed *keys;
	u32 nb_col;
	SVGPropertiesPointers backup_props_1;
	u32 backup_flags_1;
	SVGPropertiesPointers *svgp;
	GF_Node *node, *href_node;
	GF_TraverseState *tr_state = ((Render *)st->txh.compositor->visual_renderer->user_priv)->traverse_state;
	node = st->txh.owner;

	if (!gf_node_dirty_get(node)) {
		st->txh.needs_refresh = 0;
		return;
	}
	gf_node_dirty_clear(st->txh.owner, 0);
	st->txh.needs_refresh = 1;
	st->txh.transparent = 0;

	if (!st->txh.hwtx) render_texture_allocate(&st->txh);

	stencil = render_texture_get_stencil(&st->txh);
	if (!stencil) stencil = st->txh.compositor->r2d->stencil_new(st->txh.compositor->r2d, linear ? GF_STENCIL_LINEAR_GRADIENT : GF_STENCIL_RADIAL_GRADIENT);
	/*set stencil even if assigned, this invalidates the associated bitmap state in 3D*/
	render_texture_set_stencil(&st->txh, stencil);


	GF_SAFEALLOC(svgp, SVGPropertiesPointers);
	gf_svg_properties_init_pointers(svgp);
	tr_state->svg_props = svgp;

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	href_node = svg_copy_gradient_attributes_from(node, &all_atts);

	render_svg_render_base(node, &all_atts, tr_state, &backup_props_1, &backup_flags_1);

	if (!children && href_node) {
		children = ((SVG_Element *)href_node)->children;
	}

	count = gf_node_list_get_count(children);
	nb_col = 0;
	cols = (u32*)malloc(sizeof(u32)*count);
	keys = (Fixed*)malloc(sizeof(Fixed)*count);


	max_offset = 0;
	while (children) {
		SVGPropertiesPointers backup_props_2;
		u32 backup_flags_2;
		Fixed key;
		GF_Node *stop = children->node;
		children = children->next;
		if (gf_node_get_tag((GF_Node *)stop) != TAG_SVG_stop) continue;

		gf_svg_flatten_attributes((SVG_Element*)stop, &all_atts);
		render_svg_render_base(stop, &all_atts, tr_state, &backup_props_2, &backup_flags_2);

		alpha = FIX_ONE;
		if (tr_state->svg_props->stop_opacity && (tr_state->svg_props->stop_opacity->type==SVG_NUMBER_VALUE) )
			alpha = tr_state->svg_props->stop_opacity->value;

		if (tr_state->svg_props->stop_color) {
			if (tr_state->svg_props->stop_color->color.type == SVG_COLOR_CURRENTCOLOR) {
				cols[nb_col] = GF_COL_ARGB_FIXED(alpha, tr_state->svg_props->color->color.red, tr_state->svg_props->color->color.green, tr_state->svg_props->color->color.blue);
			} else {
				cols[nb_col] = GF_COL_ARGB_FIXED(alpha, tr_state->svg_props->stop_color->color.red, tr_state->svg_props->stop_color->color.green, tr_state->svg_props->stop_color->color.blue);
			}
		}

		if (all_atts.offset) {
			key = all_atts.offset->value;
			if (all_atts.offset->type==SVG_NUMBER_PERCENTAGE) key/=100; 
		} else {
			key=0;
		}
		if (key>max_offset) max_offset=key;
		else key = max_offset;
		keys[nb_col] = key;

		nb_col++;
		if (alpha!=FIX_ONE) st->txh.transparent = 1;

		memcpy(tr_state->svg_props, &backup_props_2, sizeof(SVGPropertiesPointers));
		tr_state->svg_flags = backup_flags_2;
	}
	st->txh.compositor->r2d->stencil_set_gradient_interpolation(stencil, keys, cols, nb_col);
	st->txh.compositor->r2d->stencil_set_gradient_mode(stencil, /*lg->spreadMethod*/ GF_GRADIENT_MODE_PAD);
	free(keys);
	free(cols);

	memcpy(tr_state->svg_props, &backup_props_1, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags_1;

	gf_svg_properties_reset_pointers(svgp);
	free(svgp);
	tr_state->svg_props = NULL;
}

static void SVG_Render_PaintServer(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	SVGAllAttributes all_atts;
	u32 backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	SVG_Element *elt = (SVG_Element *)node;
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		SVG_DestroyPaintServer(node);
		return;
	}

	gf_svg_flatten_attributes(elt, &all_atts);
	render_svg_render_base(node, &all_atts, tr_state, &backup_props, &backup_flags);
	
	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		return;
	} else {
		render_svg_node_list(elt->children, tr_state);
	}
	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
}

#define GRAD_TEXTURE_SIZE	128
#define GRAD_TEXTURE_HSIZE	64

void render_svg_build_gradient_texture(GF_TextureHandler *txh)
{
	u32 i;
	Fixed size;
	GF_Matrix2D mat;
	GF_STENCIL stencil;
	GF_SURFACE surf;
	GF_STENCIL texture2D;
	GF_Path *path;
	GF_Err e;
	Bool transparent;
	SVGAllAttributes all_atts;
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(txh->owner);
	GF_Raster2D *raster = txh->compositor->r2d;


	if (!txh->hwtx) return;

	if (st->tx_data) {
		free(st->tx_data);
		st->tx_data = NULL;
	}
	stencil = render_texture_get_stencil(txh);
	if (!stencil) return;
	
	/*init our 2D graphics stuff*/
	texture2D = raster->stencil_new(raster, GF_STENCIL_TEXTURE);
	if (!texture2D) return;
	surf = raster->surface_new(raster, 1);
	if (!surf) {
		raster->stencil_delete(texture2D);
		return;
	}

	transparent = st->txh.transparent;
	if (st->no_rgb_support) transparent = 1;
	
	if (transparent) {
		if (!st->tx_data) {
			st->tx_data = (char *) malloc(sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*4);
		} else {
			memset(st->tx_data, 0, sizeof(char)*txh->stride*txh->height);
		}
		e = raster->stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 4*GRAD_TEXTURE_SIZE, GF_PIXEL_ARGB, GF_PIXEL_ARGB, 1);
	} else {
		if (!st->tx_data) {
			st->tx_data = (char *) malloc(sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*3);
		}
		e = raster->stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 3*GRAD_TEXTURE_SIZE, GF_PIXEL_RGB_24, GF_PIXEL_RGB_24, 1);
		/*try with ARGB (it actually is needed for GDIplus module since GDIplus cannot handle native RGB texture (it works in BGR)*/
		if (e) {
			/*remember for later use*/
			st->no_rgb_support = 1;
			transparent = 1;
			free(st->tx_data);
			st->tx_data = (char *) malloc(sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*4);
			e = raster->stencil_set_texture(texture2D, st->tx_data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 4*GRAD_TEXTURE_SIZE, GF_PIXEL_ARGB, GF_PIXEL_ARGB, 1);
		}
	}

	if (e) {
		free(st->tx_data);
		raster->stencil_delete(texture2D);
		raster->surface_delete(surf);
		return;
	}
	e = raster->surface_attach_to_texture(surf, texture2D);
	if (e) {
		raster->stencil_delete(texture2D);
		raster->surface_delete(surf);
		return;
	}

	size = INT2FIX(GRAD_TEXTURE_HSIZE);
	/*fill surface*/
	path = gf_path_new();
	gf_path_add_move_to(path, -size, -size);
	gf_path_add_line_to(path, size, -size);
	gf_path_add_line_to(path, size, size);
	gf_path_add_line_to(path, -size, size);
	gf_path_close(path);


	gf_svg_flatten_attributes((SVG_Element*)txh->owner, &all_atts);
	gf_mx2d_init(mat);

	if (all_atts.gradientUnits && (*(SVG_GradientUnit*)all_atts.gradientUnits==SVG_GRADIENTUNITS_OBJECT) ) {
		if (all_atts.gradientTransform) {
			gf_mx2d_copy(mat, all_atts.gradientTransform->mat);
		}
		gf_mx2d_add_scale(&mat, 2*size, 2*size);
		gf_mx2d_add_translation(&mat, -size, -size);
	} else {
		/*FIXME - UserSpace is broken*/
		if (all_atts.gradientTransform) {
			GF_Matrix2D mx;
			gf_mx2d_copy(mx, all_atts.gradientTransform->mat);
			mx.m[2] = 0;
			mx.m[5] = 0;
			gf_mx2d_add_matrix(&mat, &mx);
		}

		gf_mx2d_add_scale(&mat, 2*size, 2*size);
		gf_mx2d_add_translation(&mat, -size, -size);
	}

	raster->stencil_set_matrix(stencil, &mat);
	raster->surface_set_raster_level(surf, GF_RASTER_HIGH_QUALITY);
	raster->surface_set_path(surf, path);
	raster->surface_fill(surf, stencil);
	raster->surface_delete(surf);
	raster->stencil_delete(texture2D);
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
	render_texture_set_data(txh);
	return;
}


/* linear gradient */

static void SVG_UpdateLinearGradient(GF_TextureHandler *txh)
{
	SVG_Element *lg = (SVG_Element *) txh->owner;
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(txh->owner);

	SVG_UpdateGradient(st, lg->children, 1);
}


static void SVG_LG_ComputeMatrix(GF_TextureHandler *txh, GF_Rect *bounds, GF_Matrix2D *mat)
{
	GF_STENCIL stencil;
	SFVec2f start, end;
	SVGAllAttributes all_atts;

	if (!txh->hwtx) return;
	stencil = render_texture_get_stencil(txh);
	if (!stencil) return;

	gf_svg_flatten_attributes((SVG_Element*)txh->owner, &all_atts);
	/*TODO get "transfered" attributed from xlink:href if any*/

	if (all_atts.x1) {
		start.x = all_atts.x1->value;
		if (all_atts.x1->type==SVG_NUMBER_PERCENTAGE) start.x /= 100;
	} else {
		start.x = 0;
	}
	if (all_atts.y1) {
		start.y = all_atts.y1->value;
		if (all_atts.y1->type==SVG_NUMBER_PERCENTAGE) start.y /= 100;
	} else {
		start.y = 0;
	}
	if (all_atts.x2) {
		end.x = all_atts.x2->value;
		if (all_atts.x2->type==SVG_NUMBER_PERCENTAGE) end.x /= 100;
	} else {
		end.x = 1;
	}
	if (all_atts.y2) {
		end.y = all_atts.y2->value;
		if (all_atts.y2->type==SVG_NUMBER_PERCENTAGE) end.y /= 100;
	} else {
		end.y = 0;
	}
	txh->compositor->r2d->stencil_set_gradient_mode(stencil, (GF_GradientMode) all_atts.spreadMethod ? *(SVG_SpreadMethod*)all_atts.spreadMethod : 0);

	if (all_atts.gradientTransform) {
		gf_mx2d_copy(*mat, all_atts.gradientTransform->mat );
	} else {
		gf_mx2d_init(*mat);
	}

	if (bounds && (!all_atts.gradientUnits || (*(SVG_GradientUnit*)all_atts.gradientUnits==SVG_GRADIENTUNITS_OBJECT)) ) {
		/*move to local coord system - cf SVG spec*/
		gf_mx2d_add_scale(mat, bounds->width, bounds->height);
		gf_mx2d_add_translation(mat, bounds->x - 1, bounds->y  - bounds->height - 1);
	}
	txh->compositor->r2d->stencil_set_linear_gradient(stencil, start.x, start.y, end.x, end.y);
}

void render_init_svg_linearGradient(Render *sr, GF_Node *node)
{
	SVG_GradientStack *st;
	GF_SAFEALLOC(st, SVG_GradientStack);

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = SVG_UpdateLinearGradient;
	st->txh.compute_gradient_matrix = SVG_LG_ComputeMatrix;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, SVG_Render_PaintServer);
}

/* radial gradient */

static void SVG_UpdateRadialGradient(GF_TextureHandler *txh)
{
	SVG_Element *rg = (SVG_Element *) txh->owner;
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(txh->owner);

	SVG_UpdateGradient(st, rg->children, 0);
}

static void SVG_RG_ComputeMatrix(GF_TextureHandler *txh, GF_Rect *bounds, GF_Matrix2D *mat)
{
	GF_STENCIL stencil;
	SFVec2f center, focal;
	Fixed radius;
	SVGAllAttributes all_atts;

	/*create gradient brush if needed*/
	if (!txh->hwtx) return;
	stencil = render_texture_get_stencil(txh);
	if (!stencil) return;

	gf_svg_flatten_attributes((SVG_Element*)txh->owner, &all_atts);
	/*TODO get "transfered" attributed from xlink:href if any*/

	if (all_atts.gradientTransform) 
		gf_mx2d_copy(*mat, all_atts.gradientTransform->mat);
	else
		gf_mx2d_init(*mat);
	
	if (all_atts.r) {
		radius = all_atts.r->value;
		if (all_atts.r->type==SVG_NUMBER_PERCENTAGE) radius /= 100;
	} else {
		radius = FIX_ONE/2;
	}
	if (all_atts.cx) {
		center.x = all_atts.cx->value;
		if (all_atts.cx->type==SVG_NUMBER_PERCENTAGE) center.x /= 100;
	} else {
		center.x = FIX_ONE/2;
	}
	if (all_atts.cy) {
		center.y = all_atts.cy->value;
		if (all_atts.cy->type==SVG_NUMBER_PERCENTAGE) center.y /= 100;
	} else {
		center.y = FIX_ONE/2;
	}

	txh->compositor->r2d->stencil_set_gradient_mode(stencil, (GF_GradientMode) all_atts.spreadMethod ? *(SVG_SpreadMethod*)all_atts.spreadMethod : 0);

	if (all_atts.fx) {
		focal.x = all_atts.fx->value;
		if (all_atts.fx->type==SVG_NUMBER_PERCENTAGE) focal.x /= 100;
	} else {
		focal.x = center.x;
	}
	if (all_atts.fy) {
		focal.y = all_atts.fx->value;
		if (all_atts.fy->type==SVG_NUMBER_PERCENTAGE) focal.y /= 100;
	} else {
		focal.y = center.y;
	}

	if (bounds && (!all_atts.gradientUnits || (*(SVG_GradientUnit*)all_atts.gradientUnits==SVG_GRADIENTUNITS_OBJECT)) ) {
		/*move to local coord system - cf SVG spec*/
		gf_mx2d_add_scale(mat, bounds->width, bounds->height);
		gf_mx2d_add_translation(mat, bounds->x, bounds->y  - bounds->height);
	} 
	txh->compositor->r2d->stencil_set_radial_gradient(stencil, center.x, center.y, focal.x, focal.y, radius, radius);
}

void render_init_svg_radialGradient(Render *sr, GF_Node *node)
{
	SVG_GradientStack *st;
	GF_SAFEALLOC(st, SVG_GradientStack);

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = SVG_UpdateRadialGradient;
	st->txh.compute_gradient_matrix = SVG_RG_ComputeMatrix;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, SVG_Render_PaintServer);
}

void render_init_svg_solidColor(Render *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, SVG_Render_PaintServer);
}

void render_init_svg_stop(Render *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, SVG_Render_PaintServer);
}

GF_TextureHandler *render_svg_get_gradient_texture(GF_Node *node)
{
	GF_FieldInfo info;
	GF_Node *g = NULL;
	SVG_GradientStack *st;
	/*check gradient redirection ...*/
	if (gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_xlink_href, 0, 0, &info)==GF_OK) {
		g = ((XMLRI*)info.far_ptr)->target;
	}
	if (!g) g = node;
	st = (SVG_GradientStack*) gf_node_get_private((GF_Node *)g);
	return &st->txh;
}


#endif


