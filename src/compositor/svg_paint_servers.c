/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
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

#include "visual_manager.h"

#ifndef GPAC_DISABLE_SVG
#include "nodes_stacks.h"
#include "texturing.h"


enum
{
	GF_SR_TEXTURE_GRAD_REGISTERED = 1<<6,
	GF_SR_TEXTURE_GRAD_NO_RGB = 1<<7,
};

typedef struct
{
	GF_TextureHandler txh;
	Bool linear;
	Bool animated;
	Fixed *keys;
	u32 *cols;
	u32 current_frame;
} SVG_GradientStack;


static void SVG_DestroyPaintServer(GF_Node *node)
{
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(node);
	if (st) {
		if (st->cols) gf_free(st->cols);
		if (st->keys) gf_free(st->keys);
		gf_sc_texture_destroy(&st->txh);
		gf_free(st);
	}
}


static GF_Node *svg_copy_gradient_attributes_from(GF_Node *node, SVGAllAttributes *all_atts)
{
	GF_Node *href_node;
	SVGAllAttributes all_href_atts;
	GF_FieldInfo info;

	/*check gradient redirection ...*/
	href_node = node;
	while (href_node && gf_node_get_attribute_by_tag(href_node, TAG_XLINK_ATT_href, GF_FALSE, GF_FALSE, &info) == GF_OK) {
		XMLRI*iri = (XMLRI*)info.far_ptr;

		if (iri->type != XMLRI_ELEMENTID) {
			GF_SceneGraph *sg = gf_node_get_graph(node);
			GF_Node *n = gf_sg_find_node_by_name(sg, &(iri->string[1]));
			if (n) {
				iri->type = XMLRI_ELEMENTID;
				iri->target = n;
				gf_node_register_iri(sg, iri);
				gf_free(iri->string);
				iri->string = NULL;
			} else {
				break;
			}
		}
		href_node = (GF_Node*)((XMLRI*)info.far_ptr)->target;
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

static void svg_gradient_traverse(GF_Node *node, GF_TraverseState *tr_state, Bool real_traverse)
{
	u32 count, nb_col;
	Bool is_dirty, all_dirty;
	Fixed alpha, max_offset;
	SVGAllAttributes all_atts;
	SVGPropertiesPointers backup_props_1;
	u32 backup_flags_1;
	GF_Node *href_node;
	GF_ChildNodeItem *children;
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(node);

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	href_node = svg_copy_gradient_attributes_from(node, &all_atts);
	compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props_1, &backup_flags_1);

	if (real_traverse &&
	        ! (tr_state->svg_flags & (GF_SG_SVG_STOPCOLOR_OR_OPACITY_DIRTY|GF_SG_SVG_COLOR_DIRTY))
	        && !gf_node_dirty_get(node)
	        && !st->txh.needs_refresh)
	{
		memcpy(tr_state->svg_props, &backup_props_1, sizeof(SVGPropertiesPointers));
		tr_state->svg_flags = backup_flags_1;
		return;
	}

	/*for gradients we must traverse the gradient stops to trigger animations, even if the
	gradient is not marked as dirty*/
	all_dirty = tr_state->svg_flags & (GF_SG_SVG_STOPCOLOR_OR_OPACITY_DIRTY|GF_SG_SVG_COLOR_DIRTY);
	is_dirty = GF_FALSE;
	if (gf_node_dirty_get(node)) {
		is_dirty = all_dirty = GF_TRUE;
		gf_node_dirty_clear(node, 0);
		if (st->cols) gf_free(st->cols);
		st->cols = NULL;
		if (st->keys) gf_free(st->keys);
		st->keys = NULL;

		st->animated = gf_node_animation_count(node) ? GF_TRUE : GF_FALSE;
	}

	children = ((SVG_Element *)node)->children;
	if (!children && href_node) {
		children = ((SVG_Element *)href_node)->children;
	}

	if (!st->cols) {
		count = gf_node_list_get_count(children);
		st->cols = (u32*)gf_malloc(sizeof(u32)*count);
		st->keys = (Fixed*)gf_malloc(sizeof(Fixed)*count);
	}
	nb_col = 0;
	max_offset = 0;
	while (children) {
		SVGPropertiesPointers backup_props_2;
		u32 backup_flags_2;
		Fixed key;
		GF_Node *stop = children->node;
		children = children->next;
		if (gf_node_get_tag((GF_Node *)stop) != TAG_SVG_stop) continue;

		gf_svg_flatten_attributes((SVG_Element*)stop, &all_atts);
		compositor_svg_traverse_base(stop, &all_atts, tr_state, &backup_props_2, &backup_flags_2);

		if (gf_node_animation_count(stop))
			st->animated = GF_TRUE;

		if (all_dirty || gf_node_dirty_get(stop)) {
			is_dirty = GF_TRUE;
			gf_node_dirty_clear(stop, 0);

			alpha = FIX_ONE;
			if (tr_state->svg_props->stop_opacity && (tr_state->svg_props->stop_opacity->type==SVG_NUMBER_VALUE) )
				alpha = tr_state->svg_props->stop_opacity->value;

			if (tr_state->svg_props->stop_color) {
				if (tr_state->svg_props->stop_color->color.type == SVG_COLOR_CURRENTCOLOR) {
					st->cols[nb_col] = GF_COL_ARGB_FIXED(alpha, tr_state->svg_props->color->color.red, tr_state->svg_props->color->color.green, tr_state->svg_props->color->color.blue);
				} else {
					st->cols[nb_col] = GF_COL_ARGB_FIXED(alpha, tr_state->svg_props->stop_color->color.red, tr_state->svg_props->stop_color->color.green, tr_state->svg_props->stop_color->color.blue);
				}
			} else {
				st->cols[nb_col] = GF_COL_ARGB_FIXED(alpha, 0, 0, 0);
			}

			if (all_atts.offset) {
				key = all_atts.offset->value;
				if (all_atts.offset->type==SVG_NUMBER_PERCENTAGE) key/=100;
			} else {
				key=0;
			}
			if (key>max_offset) max_offset=key;
			else key = max_offset;
			st->keys[nb_col] = key;
		} else {
			if (st->keys[nb_col]>max_offset) max_offset = st->keys[nb_col];
		}

		nb_col++;

		memcpy(tr_state->svg_props, &backup_props_2, sizeof(SVGPropertiesPointers));
		tr_state->svg_flags = backup_flags_2;
	}

	if (is_dirty) {
		u32 i;
		GF_EVGStencil * stencil;

		if (!st->txh.tx_io) gf_sc_texture_allocate(&st->txh);
		stencil = gf_sc_texture_get_stencil(&st->txh);
		if (!stencil) stencil = gf_evg_stencil_new(st->linear ? GF_STENCIL_LINEAR_GRADIENT : GF_STENCIL_RADIAL_GRADIENT);
		/*set stencil even if assigned, this invalidates the associated bitmap state in 3D*/
		gf_sc_texture_set_stencil(&st->txh, stencil);

		st->txh.transparent = GF_FALSE;
		for (i=0; i<nb_col; i++) {
			if (GF_COL_A(st->cols[i]) != 0xFF) {
				st->txh.transparent = GF_TRUE;
				break;
			}
		}

		gf_evg_stencil_set_gradient_interpolation(stencil, st->keys, st->cols, nb_col);
		gf_evg_stencil_set_gradient_mode(stencil, /*lg->spreadMethod*/ GF_GRADIENT_MODE_PAD);

		st->txh.needs_refresh = GF_TRUE;
	} else {
		st->txh.needs_refresh = GF_FALSE;
	}

	memcpy(tr_state->svg_props, &backup_props_1, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags_1;
}


static void svg_update_gradient(SVG_GradientStack *st, GF_ChildNodeItem *children, Bool linear)
{
	SVGPropertiesPointers *svgp;
	GF_Node *node = st->txh.owner;
	GF_TraverseState *tr_state = st->txh.compositor->traverse_state;

	if (!gf_node_dirty_get(node)) {
		if (st->current_frame==st->txh.compositor->current_frame) return;
		st->current_frame = st->txh.compositor->current_frame;
		st->txh.needs_refresh = GF_FALSE;
//		if (!st->animated) return;
	}

	if (!tr_state->svg_props) {
		GF_SAFEALLOC(svgp, SVGPropertiesPointers);
		if (svgp) {
			gf_svg_properties_init_pointers(svgp);
			tr_state->svg_props = svgp;

			svg_gradient_traverse(node, tr_state, GF_FALSE);

			gf_svg_properties_reset_pointers(svgp);
			gf_free(svgp);
		}
		tr_state->svg_props = NULL;
	} else {
		svg_gradient_traverse(node, tr_state, GF_FALSE);
	}
}


static void svg_traverse_gradient(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		SVG_DestroyPaintServer(node);
		return;
	}
	if (tr_state->traversing_mode != TRAVERSE_SORT) return;
	svg_gradient_traverse(node, tr_state, GF_TRUE);
}

#define GRAD_TEXTURE_SIZE	128
#define GRAD_TEXTURE_HSIZE	64

static GF_Rect compositor_svg_get_gradient_bounds(GF_TextureHandler *txh, SVGAllAttributes *all_atts)
{
	GF_Rect rc;
	if (gf_node_get_tag(txh->owner)==TAG_SVG_radialGradient) {
		rc.x = rc.y = rc.width = FIX_ONE/2;
		if (all_atts->r) {
			rc.width = 2*all_atts->r->value;
			if (all_atts->r->type==SVG_NUMBER_PERCENTAGE) rc.width /= 100;
		}
		if (all_atts->cx) {
			rc.x = all_atts->cx->value;
			if (all_atts->cx->type==SVG_NUMBER_PERCENTAGE) rc.x /= 100;
		}
		if (all_atts->cy) {
			rc.y = all_atts->cy->value;
			if (all_atts->cy->type==SVG_NUMBER_PERCENTAGE) rc.y /= 100;
		}
		rc.height = rc.width;
		rc.x -= rc.width/2;
		rc.y -= rc.height/2;
	} else {
		rc.x = rc.y = rc.height = 0;
		rc.width = FIX_ONE;
		if (all_atts->x1) {
			rc.x = all_atts->x1->value;
			if (all_atts->x1->type==SVG_NUMBER_PERCENTAGE) rc.x /= 100;
		}
		if (all_atts->y1) {
			rc.y = all_atts->y1->value;
			if (all_atts->y1->type==SVG_NUMBER_PERCENTAGE) rc.y /= 100;
		}
		if (all_atts->x2) {
			rc.width = all_atts->x2->value;
			if (all_atts->x2->type==SVG_NUMBER_PERCENTAGE) rc.width/= 100;
		}
		rc.width -= rc.x;

		if (all_atts->y2) {
			rc.height = all_atts->y2->value;
			if (all_atts->y2->type==SVG_NUMBER_PERCENTAGE) rc.height /= 100;
		}
		rc.height -= rc.y;

		if (!rc.width) rc.width = rc.height;
		if (!rc.height) rc.height = rc.width;
	}
	rc.y += rc.height;
	return rc;
}

void compositor_svg_build_gradient_texture(GF_TextureHandler *txh)
{
	u32 i;
	Fixed size;
	GF_Matrix2D mat;
	GF_EVGStencil * stencil;
	GF_EVGSurface *surface;
	GF_EVGStencil * texture2D;
	GF_Path *path;
	GF_Err e;
	Bool transparent;
	SVGAllAttributes all_atts;
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(txh->owner);

	if (!txh->tx_io) return;

	if (!(txh->flags & GF_SR_TEXTURE_GRAD_REGISTERED)) {
		txh->flags |= GF_SR_TEXTURE_GRAD_REGISTERED;
		if (gf_list_find(txh->compositor->textures, txh)<0)
			gf_list_insert(txh->compositor->textures, txh, 0);
	}

	if (txh->data) {
		gf_free(txh->data);
		txh->data = NULL;
	}
	stencil = gf_sc_texture_get_stencil(txh);
	if (!stencil) return;

	/*init our 2D graphics stuff*/
	texture2D = gf_evg_stencil_new(GF_STENCIL_TEXTURE);
	if (!texture2D) return;
	surface = gf_evg_surface_new(GF_TRUE);
	if (!surface) {
		gf_evg_stencil_delete(texture2D);
		return;
	}

	transparent = st->txh.transparent;
	if (st->txh.flags & GF_SR_TEXTURE_GRAD_NO_RGB) transparent = GF_TRUE;

	if (transparent) {
		if (!txh->data) {
			txh->data = (char *) gf_malloc(sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*4);
		} else {
			memset(txh->data, 0, sizeof(char)*txh->stride*txh->height);
		}
		e = gf_evg_stencil_set_texture(texture2D, txh->data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 4*GRAD_TEXTURE_SIZE, GF_PIXEL_ARGB);
	} else {
		if (!txh->data) {
			txh->data = (char *) gf_malloc(sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*3);
		}
		e = gf_evg_stencil_set_texture(texture2D, txh->data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 3*GRAD_TEXTURE_SIZE, GF_PIXEL_RGB);
		/*try with ARGB (it actually is needed for GDIplus module since GDIplus cannot handle native RGB texture (it works in BGR)*/
		if (e) {
			/*remember for later use*/
			st->txh.flags |= GF_SR_TEXTURE_GRAD_NO_RGB;
			transparent = GF_TRUE;
			gf_free(txh->data);
			txh->data = (char *) gf_malloc(sizeof(char)*GRAD_TEXTURE_SIZE*GRAD_TEXTURE_SIZE*4);
			e = gf_evg_stencil_set_texture(texture2D, txh->data, GRAD_TEXTURE_SIZE, GRAD_TEXTURE_SIZE, 4*GRAD_TEXTURE_SIZE, GF_PIXEL_ARGB);
		}
	}

	if (e) {
		gf_free(txh->data);
		txh->data = NULL;
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

	size = INT2FIX(GRAD_TEXTURE_HSIZE);
	/*fill surface*/
	path = gf_path_new();
	gf_path_add_move_to(path, -size, -size);
	gf_path_add_line_to(path, size, -size);
	gf_path_add_line_to(path, size, size);
	gf_path_add_line_to(path, -size, size);
	gf_path_close(path);

	gf_mx2d_init(mat);
	txh->compute_gradient_matrix(txh, NULL, &mat, GF_FALSE);

	gf_svg_flatten_attributes((SVG_Element*)txh->owner, &all_atts);

	if (all_atts.gradientUnits && (*(SVG_GradientUnit*)all_atts.gradientUnits==SVG_GRADIENTUNITS_OBJECT) ) {
		if (all_atts.gradientTransform)
			gf_mx2d_copy(mat, all_atts.gradientTransform->mat);

		gf_mx2d_add_scale(&mat, 2*size, 2*size);
		gf_mx2d_add_translation(&mat, -size, -size);
	} else {
		GF_Rect rc = compositor_svg_get_gradient_bounds(txh, &all_atts);
		/*recenter the gradient to use full texture*/
		gf_mx2d_add_translation(&mat, -rc.x, rc.height-rc.y);
		gf_mx2d_add_scale(&mat, gf_divfix(2*size, rc.width), gf_divfix(2*size , rc.height));
		gf_mx2d_add_translation(&mat, -size, -size);
	}

	gf_evg_stencil_set_matrix(stencil, &mat);
	gf_evg_surface_set_raster_level(surface, GF_RASTER_HIGH_QUALITY);
	gf_evg_surface_set_path(surface, path);
	gf_evg_surface_fill(surface, stencil);
	gf_evg_surface_delete(surface);
	gf_evg_stencil_delete(texture2D);
	gf_path_del(path);

	txh->width = GRAD_TEXTURE_SIZE;
	txh->height = GRAD_TEXTURE_SIZE;
	txh->transparent = transparent;
	txh->flags |= GF_SR_TEXTURE_NO_GL_FLIP;

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
		txh->pixelformat = GF_PIXEL_RGB;
	}
	gf_sc_texture_set_data(txh);
	return;
}


/* linear gradient */

static void SVG_UpdateLinearGradient(GF_TextureHandler *txh)
{
	SVG_Element *lg = (SVG_Element *) txh->owner;
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(txh->owner);

	svg_update_gradient(st, lg->children, GF_TRUE);
}


static void SVG_LG_ComputeMatrix(GF_TextureHandler *txh, GF_Rect *bounds, GF_Matrix2D *mat, Bool for_3d)
{
	GF_EVGStencil * stencil;
	SFVec2f start, end;
	SVGAllAttributes all_atts;
	SVG_Element *lg = (SVG_Element *) txh->owner;
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(txh->owner);

	if (!txh->tx_io) return;
	stencil = gf_sc_texture_get_stencil(txh);
	if (!stencil) return;

	svg_update_gradient(st, lg->children, GF_TRUE);

	gf_svg_flatten_attributes((SVG_Element*)txh->owner, &all_atts);

	/*get "transfered" attributed from xlink:href if any*/
	svg_copy_gradient_attributes_from(txh->owner, &all_atts);

	gf_mx2d_init(*mat);

	/*gradient is a texture, only update the bounds*/
	if (for_3d) {
		GF_Rect rc;
		if (!all_atts.gradientUnits || (*(SVG_GradientUnit*)all_atts.gradientUnits==SVG_GRADIENTUNITS_OBJECT))
			return;

		/*get gradient bounds in local coord system*/
		rc = compositor_svg_get_gradient_bounds(txh, &all_atts);
		gf_mx2d_add_scale(mat, gf_divfix(rc.width, bounds->width), gf_divfix(rc.height, bounds->height));

		return;
	}

	if (all_atts.gradientTransform)
		gf_mx2d_copy(*mat, all_atts.gradientTransform->mat );

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
		end.x = FIX_ONE;
	}
	if (all_atts.y2) {
		end.y = all_atts.y2->value;
		if (all_atts.y2->type==SVG_NUMBER_PERCENTAGE) end.y /= 100;
	} else {
		end.y = 0;
	}

	gf_evg_stencil_set_gradient_mode(stencil, (GF_GradientMode) (all_atts.spreadMethod ? *(SVG_SpreadMethod*)all_atts.spreadMethod : 0) );


	if (bounds && (!all_atts.gradientUnits || (*(SVG_GradientUnit*)all_atts.gradientUnits==SVG_GRADIENTUNITS_OBJECT)) ) {
		/*move to local coord system - cf SVG spec*/
		gf_mx2d_add_scale(mat, bounds->width, bounds->height);
		gf_mx2d_add_translation(mat, bounds->x, bounds->y  - bounds->height);
	}
	gf_evg_stencil_set_linear_gradient(stencil, start.x, start.y, end.x, end.y);
}

void compositor_init_svg_linearGradient(GF_Compositor *compositor, GF_Node *node)
{
	SVG_GradientStack *st;
	GF_SAFEALLOC(st, SVG_GradientStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate svg gradient stack\n"));
		return;
	}

	/*!!! Gradients are textures but are not registered as textures with the compositor in order to avoid updating
	too many textures each frame - gradients are only registered with the compositor when they are used in OpenGL, in order
	to release associated HW resource when no longer used*/
	st->txh.owner = node;
	st->txh.compositor = compositor;
	st->txh.update_texture_fcnt = SVG_UpdateLinearGradient;
	st->txh.flags = GF_SR_TEXTURE_SVG;

	st->txh.compute_gradient_matrix = SVG_LG_ComputeMatrix;
	st->linear = GF_TRUE;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, svg_traverse_gradient);
}

/* radial gradient */

static void SVG_UpdateRadialGradient(GF_TextureHandler *txh)
{
	SVG_Element *rg = (SVG_Element *) txh->owner;
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(txh->owner);

	svg_update_gradient(st, rg->children, GF_FALSE);
}

static void SVG_RG_ComputeMatrix(GF_TextureHandler *txh, GF_Rect *bounds, GF_Matrix2D *mat, Bool for_3d)
{
	GF_EVGStencil * stencil;
	SFVec2f center, focal;
	Fixed radius;
	SVGAllAttributes all_atts;
	SVG_Element *rg = (SVG_Element *) txh->owner;
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(txh->owner);


	/*create gradient brush if needed*/
	if (!txh->tx_io) return;
	stencil = gf_sc_texture_get_stencil(txh);
	if (!stencil) return;

	svg_update_gradient(st, rg->children, GF_FALSE);

	gf_svg_flatten_attributes((SVG_Element*)txh->owner, &all_atts);

	/*get transfered attributed from xlink:href if any*/
	svg_copy_gradient_attributes_from(txh->owner, &all_atts);

	gf_mx2d_init(*mat);

	/*gradient is a texture, only update the bounds*/
	if (for_3d && bounds) {
		GF_Rect rc;
		if (!all_atts.gradientUnits || (*(SVG_GradientUnit*)all_atts.gradientUnits==SVG_GRADIENTUNITS_OBJECT))
			return;

		/*get gradient bounds in local coord system*/
		rc = compositor_svg_get_gradient_bounds(txh, &all_atts);
		gf_mx2d_add_translation(mat, gf_divfix(rc.x-bounds->x, rc.width), gf_divfix(bounds->y - rc.y, rc.height) );
		gf_mx2d_add_scale(mat, gf_divfix(rc.width, bounds->width), gf_divfix(rc.height, bounds->height));

		gf_mx2d_inverse(mat);
		return;
	}

	if (all_atts.gradientTransform)
		gf_mx2d_copy(*mat, all_atts.gradientTransform->mat);

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

	gf_evg_stencil_set_gradient_mode(stencil, (GF_GradientMode) (all_atts.spreadMethod ? *(SVG_SpreadMethod*)all_atts.spreadMethod : 0));

	if (all_atts.fx) {
		focal.x = all_atts.fx->value;
		if (all_atts.fx->type==SVG_NUMBER_PERCENTAGE) focal.x /= 100;
	} else {
		focal.x = center.x;
	}
	if (all_atts.fy) {
		focal.y = all_atts.fy->value;
		if (all_atts.fy->type==SVG_NUMBER_PERCENTAGE) focal.y /= 100;
	} else {
		focal.y = center.y;
	}

	/* clamping fx/fy according to:
	http://www.w3.org/TR/SVG11/pservers.html#RadialGradients
	If the point defined by fx and fy lies outside the circle defined by cx, cy and r,
	then the user agent shall set the focal point to the intersection of the line from (cx, cy)
	to (fx, fy) with the circle defined by cx, cy and r.*/
	{
		Fixed norm = gf_v2d_distance(&focal, &center);
		if (norm > radius) {
			Fixed xdelta = gf_muldiv(radius, focal.x-center.x, norm);
			Fixed ydelta = gf_muldiv(radius, focal.y-center.y, norm);
			focal.x = center.x + xdelta;
			focal.y = center.y + ydelta;
		}
	}

	if (bounds && (!all_atts.gradientUnits || (*(SVG_GradientUnit*)all_atts.gradientUnits==SVG_GRADIENTUNITS_OBJECT)) ) {
		/*move to local coord system - cf SVG spec*/
		gf_mx2d_add_scale(mat, bounds->width, bounds->height);
		gf_mx2d_add_translation(mat, bounds->x, bounds->y  - bounds->height);
	}
	gf_evg_stencil_set_radial_gradient(stencil, center.x, center.y, focal.x, focal.y, radius, radius);
}

void compositor_init_svg_radialGradient(GF_Compositor *compositor, GF_Node *node)
{
	SVG_GradientStack *st;
	GF_SAFEALLOC(st, SVG_GradientStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate svg gradient stack\n"));
		return;
	}

	/*!!! Gradients are textures but are not registered as textures with the compositor in order to avoid updating
	too many textures each frame - gradients are only registered with the compositor when they are used in OpenGL, in order
	to release associated HW resource when no longer used*/
	st->txh.owner = node;
	st->txh.compositor = compositor;
	st->txh.flags = GF_SR_TEXTURE_SVG;

	st->txh.update_texture_fcnt = SVG_UpdateRadialGradient;
	st->txh.compute_gradient_matrix = SVG_RG_ComputeMatrix;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, svg_traverse_gradient);
}


static void svg_traverse_PaintServer(GF_Node *node, void *rs, Bool is_destroy, Bool is_solid_color)
{
	SVGPropertiesPointers backup_props;
	SVGAllAttributes all_atts;
	u32 backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	SVG_Element *elt = (SVG_Element *)node;
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		if (!is_solid_color)
			SVG_DestroyPaintServer(node);
		return;
	}

	gf_svg_flatten_attributes(elt, &all_atts);
	compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags);

	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		return;
	} else {
		compositor_svg_traverse_children(elt->children, tr_state);
	}
	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
}

typedef struct
{
	u32 current_frame;
	Bool is_dirty;
} GF_SolidColorStack;

Bool compositor_svg_solid_color_dirty(GF_Compositor *compositor, GF_Node *node)
{
	GF_SolidColorStack *st = (GF_SolidColorStack*)gf_node_get_private(node);
	if (st->current_frame==compositor->current_frame) return st->is_dirty;
	st->current_frame = compositor->current_frame;
	st->is_dirty = gf_node_dirty_get(node) ? GF_TRUE : GF_FALSE;
	gf_node_dirty_clear(node, 0);
	return st->is_dirty;
}

static void svg_traverse_solidColor(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		GF_SolidColorStack *st = (GF_SolidColorStack*)gf_node_get_private(node);
		gf_free(st);
		return;
	}
	svg_traverse_PaintServer(node, rs, is_destroy, GF_TRUE);
}


void compositor_init_svg_solidColor(GF_Compositor *compositor, GF_Node *node)
{
	GF_SolidColorStack *st;
	GF_SAFEALLOC(st, GF_SolidColorStack);
	if (!st) return;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, svg_traverse_solidColor);
}

static void svg_traverse_stop(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_traverse_PaintServer(node, rs, is_destroy, GF_TRUE);
}

void compositor_init_svg_stop(GF_Compositor *compositor, GF_Node *node)
{
	gf_node_set_callback_function(node, svg_traverse_stop);
}

GF_TextureHandler *compositor_svg_get_gradient_texture(GF_Node *node)
{
	SVG_GradientStack *st = (SVG_GradientStack*) gf_node_get_private((GF_Node *)node);
	st->txh.update_texture_fcnt(&st->txh);
	return &st->txh;
}


#endif


