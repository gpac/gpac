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

#include "visual_surface.h"
#include "render3d_nodes.h"
#include <gpac/options.h>

VisualSurface *VS_New()
{
	VisualSurface *tmp = malloc(sizeof(VisualSurface));
	memset(tmp, 0, sizeof(VisualSurface));
	tmp->back_stack = gf_list_new();
	tmp->view_stack = gf_list_new();
	tmp->alpha_nodes_to_draw = gf_list_new();
	tmp->fog_stack = gf_list_new();
	tmp->navigation_stack = gf_list_new();
	return tmp;
}

void VS_Delete(VisualSurface *surf)
{
	BindableStackDelete(surf->back_stack);
	BindableStackDelete(surf->view_stack);
	BindableStackDelete(surf->fog_stack);
	BindableStackDelete(surf->navigation_stack);
	gf_list_del(surf->alpha_nodes_to_draw);
	free(surf);
}

DrawableStack *new_drawable(GF_Node *owner, GF_Renderer *compositor)
{
	DrawableStack *tmp = malloc(sizeof(DrawableStack));
	if (tmp) stack_setup(tmp, owner, compositor);
	return tmp;
}
void delete_drawable(DrawableStack*d)
{
	drawablestack_predestroy(d);
	free(d);
}

void drawable_Node_PreDestroy(GF_Node *n)
{
	DrawableStack *d = gf_node_get_private(n);
	if (d) delete_drawable(d);
}

DrawableStack *BaseDrawableStack(GF_Renderer *sr, GF_Node *node)
{
	DrawableStack *st = new_drawable(node, sr);
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, drawable_Node_PreDestroy);
	return st;
}


void delete_strikeinfo(StrikeInfo *info)
{
	if (info->outline) mesh_free(info->outline);
	free(info);
}

stack2D *new_stack2D(GF_Node *owner, GF_Renderer *compositor)
{
	stack2D *tmp = malloc(sizeof(stack2D));
	if (tmp) {
		stack_setup(tmp, owner, compositor);
		tmp->path = gf_path_new();
		tmp->strike_list = gf_list_new();
	}
	return tmp;
}

void stack2D_predestroy(stack2D *d)
{
	Render3D *sr = (Render3D *)d->compositor->visual_renderer->user_priv;
	if (d->mesh) mesh_free(d->mesh);
	assert(d->path);
	gf_path_del(d->path);

	while (gf_list_count(d->strike_list)) {
		StrikeInfo *si = gf_list_get(d->strike_list, 0);
		gf_list_rem(d->strike_list, 0);
		/*remove from main strike list*/
		gf_list_del_item(sr->strike_bank, si);
		delete_strikeinfo(si);
	}
	gf_list_del(d->strike_list);
}

void delete_stack2D(stack2D *d)
{
	stack2D_predestroy(d);
	free(d);
}
void stack2D_node_predestroy(GF_Node *n)
{
	stack2D *d = gf_node_get_private(n);
	if (d) delete_stack2D(d);
}

stack2D *BaseStack2D(GF_Renderer *sr, GF_Node *node)
{
	stack2D *st = new_stack2D(node, sr);
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, stack2D_node_predestroy);
	return st;
}

void stack2D_setup(stack2D *st, GF_Renderer *sr, GF_Node *node)
{
	stack_setup(st, node, sr);
	st->path = gf_path_new();
	st->strike_list = gf_list_new();
}

void stack2D_reset(stack2D *d)
{
	Render3D *sr = (Render3D *)d->compositor->visual_renderer->user_priv;
	if (d->path) gf_path_reset(d->path);
	while (gf_list_count(d->strike_list)) {
		StrikeInfo *si = gf_list_get(d->strike_list, 0);
		gf_list_rem(d->strike_list, 0);
		gf_list_del_item(sr->strike_bank, si);
		delete_strikeinfo(si);
	}
}

Bool VS_GetAspect2D(RenderEffect3D *eff, Aspect2D *asp)
{
	M_Material2D *mat;
	Bool has_mat;
	/*default values*/
	memset(asp, 0, sizeof(Aspect2D));
	asp->fill_color.red = asp->fill_color.green = asp->fill_color.blue = FLT2FIX(0.8f);
	/*this is a bug in the spec: by default line width is 1.0, but in meterMetrics this means half of the screen :)*/
	if (eff->is_pixel_metrics) 
		asp->pen_props.width = FIX_ONE;
	else
		asp->pen_props.width = gf_invfix(2*eff->min_hsize);

	asp->line_color = asp->fill_color;
	asp->pen_props.cap = GF_LINE_CAP_FLAT;
	asp->pen_props.join = GF_LINE_JOIN_MITER;
	asp->pen_props.miterLimit = INT2FIX(4);
	asp->line_alpha = asp->alpha = FIX_ONE;
	has_mat = 1;


	if (!eff->appear) goto exit;
	mat = (M_Material2D *) ((M_Appearance *)eff->appear)->material;
	/*according to the spec only Material2D shall be used with 2D primitives*/
	if (!mat) goto exit;
	switch (gf_node_get_tag((GF_Node *)mat)) {
	/*we need to indicate to text if it shall use 2D or 3D material...*/
	case TAG_MPEG4_Material:
	case TAG_X3D_Material:
		has_mat = 0;
		asp->filled = 1;
		asp->pen_props.width = 0;
		asp->fill_color = ((M_Material*)mat)->diffuseColor;
		goto exit;
	case TAG_MPEG4_MaterialKey:
		goto exit;
	}

	asp->line_color = asp->fill_color = mat->emissiveColor;
	asp->line_alpha = asp->alpha = FIX_ONE - mat->transparency;
	asp->filled = mat->filled;

	if (mat->lineProps == NULL) {
		if (asp->filled || (asp->alpha==0)) asp->pen_props.width =0;
	} else {
		asp->lp = mat->lineProps;
		switch (gf_node_get_tag(mat->lineProps)) {
		case TAG_MPEG4_LineProperties:
		{
			M_LineProperties *lp = (M_LineProperties *)mat->lineProps;
			asp->pen_props.width = lp->width;
			asp->pen_props.dash = lp->lineStyle;
			asp->line_color = lp->lineColor;
		}
			break;
		case TAG_MPEG4_XLineProperties:
		{
			M_XLineProperties *xlp = (M_XLineProperties *)mat->lineProps;
			asp->pen_props.dash = xlp->lineStyle;
			asp->line_color = xlp->lineColor;
			asp->line_alpha = FIX_ONE - xlp->transparency;
			asp->pen_props.width = xlp->width;
			asp->is_scalable = xlp->isScalable;
			asp->pen_props.align = xlp->isCenterAligned ? GF_PATH_LINE_CENTER : GF_PATH_LINE_INSIDE;
			asp->pen_props.cap = xlp->lineCap;
			asp->pen_props.join = xlp->lineJoin;
			asp->pen_props.miterLimit = xlp->miterLimit;
			asp->pen_props.dash_offset = xlp->dashOffset;
			/*dash settings strutc is the same as MFFloat from XLP, typecast without storing*/
			if (xlp->dashes.count) {
				asp->pen_props.dash_set = (GF_DashSettings *) &xlp->dashes;
			} else {
				asp->pen_props.dash_set = NULL;
			}
			asp->txh = R3D_GetTextureHandler(xlp->texture);
			asp->tx_trans = xlp->textureTransform;
		}
			break;
		}
	}

exit:
	if (!eff->color_mat.identity) 
		gf_cmx_apply_fixed(&eff->color_mat, &asp->alpha, &asp->fill_color.red, &asp->fill_color.green, &asp->fill_color.blue);
	
	/*update line width*/
	if (asp->pen_props.width) {
		/*if pen is not scalable, apply user/viewport transform so that original aspect is kept*/
		if (!asp->is_scalable) {
			GF_Rect rc;
			rc.x = rc.y = 0;
			rc.width = rc.height = FIX_ONE;
			gf_mx_apply_rect(&eff->model_matrix, &rc);
			asp->line_scale = MAX(rc.width, rc.height);
		} else {
			asp->line_scale = FIX_ONE;
		}
		if (!eff->color_mat.identity) 
			gf_cmx_apply_fixed(&eff->color_mat, &asp->line_alpha, &asp->line_color.red, &asp->line_color.green, &asp->line_color.blue);
	}
	return has_mat;
}

StrikeInfo *VS_GetStrikeInfo(stack2D *st, Aspect2D *asp, RenderEffect3D *eff)
{
	StrikeInfo *si;
	u32 now, i;
	Render3D *sr = (Render3D *)st->compositor->visual_renderer->user_priv;
	Bool vect_outline = !sr->raster_outlines;
	if (!asp->pen_props.width || !st->path) return NULL;

	si = NULL;
	i=0;
	while ((si = gf_list_enum(st->strike_list, &i))) {
		/*note this includes default LP (NULL)*/
		if (si->lineProps == asp->lp) break;
		si = NULL;
	}
	/*not found, add*/
	if (!si) {
		si = malloc(sizeof(StrikeInfo));
		memset(si, 0, sizeof(StrikeInfo));
		si->lineProps = asp->lp;
		si->node2D = st->owner;
		gf_list_add(st->strike_list, si);
		gf_list_add(sr->strike_bank, si);
	}

	if (vect_outline != si->is_vectorial) {
		if (si->outline) mesh_free(si->outline);
		si->outline = NULL;
	}

	/*node changed or outline not build*/
	now = asp->lp ? (1 + R3D_LP_GetLastUpdateTime(asp->lp)) : si->last_update_time;
	if (!si->outline 
		|| (si->is_vectorial && ((now!=si->last_update_time) || (si->line_scale != asp->line_scale) )) ) {
		si->last_update_time = now;
		si->line_scale = asp->line_scale;
		if (si->outline) mesh_free(si->outline);
		si->outline = new_mesh();
		si->is_vectorial = vect_outline;
#ifndef GPAC_USE_OGL_ES
		if (vect_outline) {
			u32 i;
			GF_Path *outline_path;
			Fixed dash_o = asp->pen_props.dash_offset;
			Fixed w = asp->pen_props.width;
			asp->pen_props.width = gf_divfix(asp->pen_props.width, asp->line_scale);
			asp->pen_props.dash_offset = gf_mulfix(asp->pen_props.dash_offset, asp->pen_props.width);
			if (asp->pen_props.dash_set) {
				for(i=0; i<asp->pen_props.dash_set->num_dash; i++) {
					asp->pen_props.dash_set->dashes[i] = gf_mulfix(asp->pen_props.dash_set->dashes[i], asp->line_scale);
				}
			}
			
			outline_path = gf_path_get_outline(st->path, asp->pen_props);
			/*restore*/
			asp->pen_props.width = w;
			asp->pen_props.dash_offset = dash_o;
			if (asp->pen_props.dash_set) {
				for(i=0; i<asp->pen_props.dash_set->num_dash; i++) {
					asp->pen_props.dash_set->dashes[i] = gf_divfix(asp->pen_props.dash_set->dashes[i], asp->line_scale);
				}
			}

			TesselatePath(si->outline, outline_path, asp->txh ? 2 : 1);
			gf_path_del(outline_path);
		} else
#endif
			mesh_get_outline(si->outline, st->path);
	}
	return si;
}


StrikeInfo *VS_GetStrikeInfoIFS(stack2D *st, Aspect2D *asp, RenderEffect3D *eff)
{
	StrikeInfo *si;
	u32 now, i;
	Render3D *sr = (Render3D *)st->compositor->visual_renderer->user_priv;
	if (!asp->pen_props.width || !st->path) return NULL;

	si = NULL;
	i=0;
	while ((si = gf_list_enum(st->strike_list, &i))) {
		/*note this includes default LP (NULL)*/
		if (si->lineProps == asp->lp) break;
		si = NULL;
	}
	/*not found, add*/
	if (!si) {
		si = malloc(sizeof(StrikeInfo));
		memset(si, 0, sizeof(StrikeInfo));
		si->lineProps = asp->lp;
		si->node2D = st->owner;
		gf_list_add(st->strike_list, si);
		gf_list_add(sr->strike_bank, si);
	}

	/*for this func the strike is always raster*/
	if (si->is_vectorial) {
		if (si->outline) mesh_free(si->outline);
		si->outline = NULL;
	}

	/*node changed or outline not build*/
	now = asp->lp ? R3D_LP_GetLastUpdateTime(asp->lp) : si->last_update_time;
	if ((now!=si->last_update_time) || (si->line_scale != asp->line_scale) ) {
		si->last_update_time = now;
		si->line_scale = asp->line_scale;
		if (si->outline) mesh_free(si->outline);
		si->outline = NULL;
		si->is_vectorial = 0;
	}
	return si;
}


Fixed Aspect_GetLineWidth(Aspect2D *asp)
{
	/*for raster outlines are already set to the proper width - note this may not work depending on GL...*/
	Fixed width = asp->pen_props.width;
	if (asp->is_scalable) 
		width = gf_mulfix(width, asp->line_scale);
	return width;
}

void VS_Set2DStrikeAspect(VisualSurface *surf, Aspect2D *asp)
{
	if (asp->txh && tx_enable(asp->txh, asp->tx_trans)) return;
	/*no texture or not ready, use color*/
	VS3D_SetMaterial2D(surf, asp->line_color, asp->line_alpha);
}

GF_TextureHandler *VS_setup_texture_2d(RenderEffect3D *eff, Aspect2D *asp)
{
	GF_TextureHandler *txh;
	if (!eff->appear) return NULL;
	txh = R3D_GetTextureHandler(((M_Appearance *)eff->appear)->texture);
	if (!txh) return NULL;
	if (!asp->filled) {
		if (asp->alpha!=FIX_ONE) {
			VS3D_SetMaterial2D(eff->surface, asp->fill_color, asp->alpha);
			tx_set_blend_mode(txh, TX_MODULATE);
		} else {
			VS3D_SetState(eff->surface, F3D_BLEND, 0);
			tx_set_blend_mode(txh, TX_REPLACE);
		}
	}
	eff->mesh_has_texture = tx_enable(txh, ((M_Appearance *)eff->appear)->textureTransform);
	if (eff->mesh_has_texture) return txh;
	return NULL;
}

void stack2D_draw(stack2D *st, RenderEffect3D *eff)
{
	Aspect2D asp;
	StrikeInfo *si;
	GF_TextureHandler *fill_txh;

	VS_GetAspect2D(eff, &asp);

	if (!asp.alpha) {
		fill_txh = NULL;
	} else {
		fill_txh = VS_setup_texture_2d(eff, &asp);
	}
	/*fill path*/
	if (fill_txh || (asp.alpha && asp.filled) ) {
		if (asp.filled) VS3D_SetMaterial2D(eff->surface, asp.fill_color, asp.alpha);
		VS3D_DrawMesh(eff, st->mesh);
		/*reset texturing in case of line texture*/
		if (eff->mesh_has_texture) {
			tx_disable(fill_txh);
			eff->mesh_has_texture = 0;
		}
	}

	/*strike path*/
	if (!asp.line_alpha) return;
	si = VS_GetStrikeInfo(st, &asp, eff);
	if (si) {
		VS_Set2DStrikeAspect(eff->surface, &asp);
		if (asp.txh) eff->mesh_has_texture = 1;
		if (!si->is_vectorial) {
			VS3D_StrikeMesh(eff, si->outline, Aspect_GetLineWidth(&asp), asp.pen_props.dash);
		} else {
			VS3D_DrawMesh(eff, si->outline);
		}
		if (asp.txh) {
			tx_disable(asp.txh);
			eff->mesh_has_texture = 0;
		}		
	}
}

static GFINLINE Bool VS_setup_material(RenderEffect3D *eff, u32 mesh_type)
{
	SFColor def;
	GF_Node *__mat;
	def.red = def.green = def.blue = FIX_ONE;
	/*temp storage of diffuse alpha*/
	eff->ray.orig.x = FIX_ONE;

	if (!eff->appear) {
		/*use material2D to disable lighting*/
		VS3D_SetMaterial2D(eff->surface, def, FIX_ONE);
		return 1;
	}

	if (gf_node_get_tag(eff->appear)==TAG_X3D_Appearance) {
		X_FillProperties *fp = (X_FillProperties *) ((X_Appearance*)eff->appear)->fillProperties;
		if (fp && !fp->filled) return 0;
	}

	__mat = ((M_Appearance *)eff->appear)->material;
	if (!__mat) {
		/*use material2D to disable lighting (cf VRML specs)*/
		VS3D_SetMaterial2D(eff->surface, def, FIX_ONE);
		return 1;
	} 
	
	switch (gf_node_get_tag((GF_Node *)__mat)) {
	case TAG_MPEG4_Material:
	case TAG_X3D_Material:
	{
		SFColor diff, spec, emi;
		Fixed diff_a, spec_a, emi_a;
		Fixed vec[4];
		Bool has_alpha;
		u32 flag = F3D_LIGHT /*| F3D_COLOR*/;
		M_Material *mat = (M_Material *)__mat;

		diff = mat->diffuseColor;
		diff_a = FIX_ONE - mat->transparency;

		/*if drawing in 2D context or special meshes (lines, points) disable lighting*/
		if (mesh_type || !eff->camera->is_3D) {
			if (eff->camera->is_3D) diff = mat->emissiveColor;
			if (!eff->color_mat.identity) gf_cmx_apply_fixed(&eff->color_mat, &diff_a, &diff.red, &diff.green, &diff.blue);
			VS3D_SetMaterial2D(eff->surface, diff, diff_a);
			return 1;
		}

		spec = mat->specularColor;
		emi = mat->emissiveColor;
		spec_a = emi_a = FIX_ONE - mat->transparency;
		if (!eff->color_mat.identity) {
			gf_cmx_apply_fixed(&eff->color_mat, &diff_a, &diff.red, &diff.green, &diff.blue);
			gf_cmx_apply_fixed(&eff->color_mat, &spec_a, &spec.red, &spec.green, &spec.blue);
			gf_cmx_apply_fixed(&eff->color_mat, &emi_a, &emi.red, &emi.green, &emi.blue);

			if ((diff_a+FIX_EPSILON<FIX_ONE) || (spec_a+FIX_EPSILON<FIX_ONE) || (emi_a+FIX_EPSILON<FIX_ONE )) {
				has_alpha = 1;
			} else {
				has_alpha = 0;
			}
		} else {
			has_alpha = (mat->transparency>FIX_EPSILON) ? 1 : 0;
			/*100% transparent DON'T DRAW*/
			if (mat->transparency+FIX_EPSILON>=FIX_ONE) return 0;
		}

		/*using antialiasing with alpha usually gives bad results (non-edge face segments are visible)*/
		VS3D_SetAntiAlias(eff->surface, !has_alpha);
		if (has_alpha) {
			flag |= F3D_BLEND;
			eff->mesh_is_transparent = 1;
		}
		VS3D_SetState(eff->surface, flag, 1);

		vec[0] = gf_mulfix(diff.red, mat->ambientIntensity);
		vec[1] = gf_mulfix(diff.green, mat->ambientIntensity);
		vec[2] = gf_mulfix(diff.blue, mat->ambientIntensity);
		vec[3] = diff_a;
		VS3D_SetMaterial(eff->surface, MATERIAL_AMBIENT, vec);

		vec[0] = diff.red;
		vec[1] = diff.green;
		vec[2] = diff.blue;
		vec[3] = diff_a;
		VS3D_SetMaterial(eff->surface, MATERIAL_DIFFUSE, vec);

		vec[0] = spec.red;
		vec[1] = spec.green;
		vec[2] = spec.blue;
		vec[3] = spec_a;
		VS3D_SetMaterial(eff->surface, MATERIAL_SPECULAR, vec);

		
		vec[0] = emi.red;
		vec[1] = emi.green;
		vec[2] = emi.blue;
		vec[3] = emi_a;
		VS3D_SetMaterial(eff->surface, MATERIAL_EMISSIVE, vec);

		VS3D_SetShininess(eff->surface, mat->shininess);
		eff->ray.orig.x = diff_a;
	}
		break;
	case TAG_MPEG4_Material2D:
	{
		SFColor emi;
		Fixed emi_a;
		M_Material2D *mat = (M_Material2D *)__mat;

		emi = mat->emissiveColor;
		emi_a = FIX_ONE - mat->transparency;
		if (!eff->color_mat.identity) gf_cmx_apply_fixed(&eff->color_mat, &emi_a, &emi.red, &emi.green, &emi.blue);
		/*100% transparent DON'T DRAW*/
		if (emi_a<FIX_EPSILON) return 0;
		else if (emi_a+FIX_EPSILON<FIX_ONE) VS3D_SetState(eff->surface, F3D_BLEND, 1);


		/*this is an extra feature: if material2D.filled is FALSE on 3D objects, switch to TX_REPLACE mode
		and enable lighting*/
		if (!mat->filled) {
			GF_TextureHandler *txh = R3D_GetTextureHandler(((M_Appearance *)eff->appear)->texture);
			if (txh) {
				tx_set_blend_mode(txh, TX_REPLACE);
				VS3D_SetState(eff->surface, F3D_COLOR, 0);
				VS3D_SetState(eff->surface, F3D_LIGHT, 1);
				return 1;
			}
		}
		/*regular mat 2D*/
		VS3D_SetState(eff->surface, F3D_LIGHT | F3D_COLOR, 0);
		VS3D_SetMaterial2D(eff->surface, emi, emi_a);
	}	
		break;
	default:
		break;
	}
	return 1;
}

Bool VS_setup_texture(RenderEffect3D *eff)
{
	GF_TextureHandler *txh;
	eff->mesh_has_texture = 0;
	if (!eff->appear) return 0;
	txh = R3D_GetTextureHandler(((M_Appearance *)eff->appear)->texture);
	if (txh) {
		tx_set_blend_mode(txh, tx_is_transparent(txh) ? TX_MODULATE : TX_REPLACE);
		eff->mesh_has_texture = tx_enable(txh, ((M_Appearance *)eff->appear)->textureTransform);
		if (eff->mesh_has_texture) {
			Fixed v[4];
			switch (txh->pixelformat) {
			/*override diffuse color with full intensity, but keep material alpha (cf VRML lighting)*/
			case GF_PIXEL_RGB_24:
				v[0] = v[1] = v[2] = 1; v[3] = eff->ray.orig.x;
				VS3D_SetMaterial(eff->surface, MATERIAL_DIFFUSE, v);
				break;
			/*override diffuse color AND material alpha (cf VRML lighting)*/
			case GF_PIXEL_RGBA:
				v[0] = v[1] = v[2] = v[3] = 1;
				VS3D_SetMaterial(eff->surface, MATERIAL_DIFFUSE, v);
				break;
			case GF_PIXEL_GREYSCALE:
				eff->mesh_has_texture = 2;
				break;
			}
		}
		return eff->mesh_has_texture;
	}
	return 0;
}

void VS_disable_texture(RenderEffect3D *eff)
{
	if (eff->mesh_has_texture) {
		tx_disable(R3D_GetTextureHandler(((M_Appearance *)eff->appear)->texture) );
		eff->mesh_has_texture = 0;
	}
}

Bool VS_SetupAppearance(RenderEffect3D *eff)
{
	/*setup material and check if 100% transparent - in which case don't draw*/
	if (!VS_setup_material(eff, 0)) return 0;
	/*setup texture*/
	VS_setup_texture(eff);
	return 1;
}

void VS_DrawMesh(RenderEffect3D *eff, GF_Mesh *mesh)
{
	if (mesh->mesh_type) {
		if (VS_setup_material(eff, mesh->mesh_type)) {
			VS3D_DrawMesh(eff, mesh);
		}
	} else if (VS_SetupAppearance(eff)) {
		VS3D_DrawMesh(eff, mesh);
		VS_disable_texture(eff);
	
#ifndef GPAC_USE_OGL_ES
		if (eff->appear && gf_node_get_tag(eff->appear)==TAG_X3D_Appearance) {
			X_Appearance *ap = (X_Appearance *)eff->appear;
			X_FillProperties *fp = ap->fillProperties ? (X_FillProperties *) ap->fillProperties : NULL;
			if (fp && fp->hatched) VS3D_HatchMesh(eff, mesh, fp->hatchStyle, fp->hatchColor);
		}
#endif
	}
}



/*uncomment to disable frustum cull*/
//#define DISABLE_VIEW_CULL

#ifndef GPAC_DISABLE_LOG
static const char *szPlaneNames [] = 
{
	"Near", "Far", "Left", "Right", "Bottom", "Top"
};
#endif

Bool node_cull(RenderEffect3D *eff, GF_BBox *bbox, Bool skip_near) 
{
	GF_BBox b;
	Fixed irad, rad;
	GF_Camera *cam;
	Bool do_sphere;
	u32 i, p_idx;
	SFVec3f cdiff, vertices[8];

	if (eff->cull_flag == CULL_INSIDE) return 1;
	assert(eff->cull_flag != CULL_OUTSIDE);

#ifdef DISABLE_VIEW_CULL
	eff->cull_flag = CULL_INSIDE;
	return 1;
#endif

	/*empty bounds*/
	if (!bbox->is_set) {
		eff->cull_flag = CULL_OUTSIDE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Culling: Node out (bbox not set)\n"));
		return 0;
	}

	/*get bbox in world space*/
	b = *bbox;
	gf_mx_apply_bbox(&eff->model_matrix, &b);
	cam = eff->camera;

	/*if camera is inside bbox consider we intersect*/
	if (gf_bbox_point_inside(&b, &cam->position)) {
		eff->cull_flag = CULL_INTERSECTS;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Culling: Node intersect (camera in box test)\n"));
		return 1;
	}
	/*first check: sphere vs frustum sphere intersection, this will discard far objects quite fast*/
	gf_vec_diff(cdiff, cam->center, b.center);
	rad = b.radius + cam->radius;
	if (gf_vec_len(cdiff) > rad) {
		eff->cull_flag = CULL_OUTSIDE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Culling: Node out (sphere-sphere test)\n"));
		return 0;
	}

	/*second check: sphere vs frustum planes intersection, if any intersection is detected switch 
	to n/p vertex check.*/
	rad = b.radius;
	irad = -b.radius;
	do_sphere = 1;

	/*skip near/far tests in ortho mode, and near in 3D*/
	i = (eff->camera->is_3D) ? (skip_near ? 1 : 0) : 2;
	for (; i<6; i++) {
		if (do_sphere) {
			Fixed d = gf_plane_get_distance(&cam->planes[i], &b.center);
			if (d<irad) {
				eff->cull_flag = CULL_OUTSIDE;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Culling: Node out (sphere-planes test) plane %s\n", szPlaneNames[i]));
				return 0;
			}
			/*intersect, move to n-p vertex test*/
			if (d<rad) {
				/*get box vertices*/
				gf_bbox_get_vertices(b.min_edge, b.max_edge, vertices);
				do_sphere = 0;
			} else {
				continue;
			}
		}
		p_idx = cam->p_idx[i];
		/*check p-vertex: if not in plane, we're out (since p-vertex is the closest point to the plane)*/
		if (gf_plane_get_distance(&cam->planes[i], &vertices[p_idx])<0) {
			eff->cull_flag = CULL_OUTSIDE;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Culling: Node out (p-vertex test) plane %s\n", szPlaneNames[i]));
			return 0;
		}
		/*check n-vertex: if not in plane, we're intersecting*/
		if (gf_plane_get_distance(&cam->planes[i], &vertices[7-p_idx])<0) {
			eff->cull_flag = CULL_INTERSECTS;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Culling: Node intersect (n-vertex test) plane %s\n", szPlaneNames[i]));
			return 1;
		}
	}

	eff->cull_flag = CULL_INSIDE;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Culling: Node inside (%s test)\n", do_sphere ? "sphere-planes" : "n-p vertex"));
	return 1;
}

void VS_SetupProjection(RenderEffect3D *eff)
{
	GF_Node *bindable;
	u32 mode = eff->traversing_mode;
	eff->traversing_mode = TRAVERSE_RENDER_BINDABLE;

	/*setup viewpoint (this directly modifies the frustum)*/
	bindable = gf_list_get(eff->viewpoints, 0);
	if (Bindable_GetIsBound(bindable)) {
		gf_node_render(bindable, eff);
		eff->camera->had_viewpoint = 1;
	} else if (eff->camera->had_viewpoint) {
		if (eff->camera->is_3D) {
			SFVec3f pos, center;
			SFRotation r;
			Fixed fov = GF_PI/4;
			/*default viewpoint*/
			pos.x = pos.y = 0; pos.z = 10 * FIX_ONE;
			center.x = center.y = center.z = 0;
			r.q = r.x = r.z = 0; r.y = FIX_ONE;
			/*this takes care of pixelMetrics*/
			VS_ViewpointChange(eff, NULL, 0, fov, pos, r, center);
			/*initial vp compute, don't animate*/
			if (eff->camera->had_viewpoint == 2) {
				camera_stop_anim(eff->camera);
				camera_reset_viewpoint(eff->camera, 0);
			}
		} else {
			eff->camera->zoom = FIX_ONE;
			eff->camera->trans.x = eff->camera->trans.y = eff->camera->rot.x = eff->camera->rot.y = 0;
			eff->camera->flags &= ~CAM_HAS_VIEWPORT;
			eff->camera->flags |= CAM_IS_DIRTY;
		}
		eff->camera->had_viewpoint = 0;
	}

	camera_update(eff->camera);

	/*setup projection/modelview*/
	VS3D_SetMatrixMode(eff->surface, MAT_PROJECTION);
	VS3D_LoadMatrix(eff->surface, eff->camera->projection.m);
	VS3D_SetMatrixMode(eff->surface, MAT_MODELVIEW);
	VS3D_LoadMatrix(eff->surface, eff->camera->modelview.m);
	gf_mx_init(eff->model_matrix);

	eff->traversing_mode = mode;
}

void VS_InitRender(RenderEffect3D *eff)
{
	Bool in_layer;
	u32 mode;
	GF_Node *bindable;

	in_layer = (eff->backgrounds != eff->surface->back_stack) ? 1 : 0;

	/*if not in layer, render navigation
	FIXME: we should update the nav info according to the world transform at the current viewpoint (vrml)*/
	eff->traversing_mode = TRAVERSE_RENDER_BINDABLE;
	bindable = eff->navigations ? gf_list_get(eff->navigations, 0) : NULL;
	if (Bindable_GetIsBound(bindable)) {
		gf_node_render(bindable, eff);
		eff->camera->had_nav_info = 1;
	} else if (eff->camera->had_nav_info) {
		/*if no navigation specified, use default VRML one*/
		eff->camera->avatar_size.x = FLT2FIX(0.25f); eff->camera->avatar_size.y = FLT2FIX(1.6f); eff->camera->avatar_size.z = FLT2FIX(0.75f);
		eff->camera->visibility = 0;
		eff->camera->speed = FIX_ONE;
		/*not specified in the spec, but by default we forbid navigation in layer*/
		if (in_layer) {
			eff->camera->navigation_flags = NAV_HEADLIGHT;
			eff->camera->navigate_mode = GF_NAVIGATE_NONE;
		} else {
			eff->camera->navigation_flags = NAV_ANY | NAV_HEADLIGHT;
			if (eff->camera->is_3D) {
				/*X3D is by default examine, VRML/MPEG4 is WALK*/
				eff->camera->navigate_mode = (eff->surface->render->root_is_3D==2) ? GF_NAVIGATE_EXAMINE : GF_NAVIGATE_WALK;
			} else {
				eff->camera->navigate_mode = GF_NAVIGATE_NONE;
			}
		}
		eff->camera->had_nav_info = 0;

		if (eff->is_pixel_metrics) {
			eff->camera->visibility = gf_mulfix(eff->camera->visibility, eff->min_hsize);
			eff->camera->avatar_size.x = gf_mulfix(eff->camera->avatar_size.x, eff->min_hsize);
			eff->camera->avatar_size.y = gf_mulfix(eff->camera->avatar_size.y, eff->min_hsize);
			eff->camera->avatar_size.z = gf_mulfix(eff->camera->avatar_size.z, eff->min_hsize);
		}
	}

	/*animate current camera - if returns TRUE draw next frame*/
	if (camera_animate(eff->camera)) 
		gf_sr_invalidate(eff->surface->render->compositor, NULL);

	VS3D_SetViewport(eff->surface, eff->camera->vp);

	/*setup projection*/
	VS_SetupProjection(eff);

	/*turn off depth buffer in 2D*/
	VS3D_SetDepthBuffer(eff->surface, eff->camera->is_3D);

	/*set headlight if any*/
	VS3D_SetHeadlight(eff->surface, (eff->camera->navigation_flags & NAV_HEADLIGHT) ? 1 : 0, eff->camera);

	/*setup background*/
	mode = eff->traversing_mode;
	eff->traversing_mode = TRAVERSE_RENDER_BINDABLE;
	bindable = gf_list_get(eff->backgrounds, 0);

	/*if in layer clear z buffer (even if background)*/
	if (in_layer) VS3D_ClearDepth(eff->surface);

	if (Bindable_GetIsBound(bindable)) {
		gf_node_render(bindable, eff);
	}
	/*clear if not in layer*/
	else if (!in_layer) {
		SFColor col;
		col.red = col.green = col.blue = 0;
		/*if composite surface, clear with alpha = 0*/
		VS3D_ClearSurface(eff->surface, col, (eff->surface==eff->surface->render->surface) ? FIX_ONE : 0);
	}
	eff->traversing_mode = mode;
}

void VS_SetupEffects(VisualSurface *surface, RenderEffect3D *eff)
{
	eff->surface = surface;
	eff->camera = &surface->camera;
	eff->backgrounds = surface->back_stack;
	eff->viewpoints = surface->view_stack;
	eff->fogs = surface->fog_stack;
	eff->navigations = surface->navigation_stack;
	eff->color_mat.identity = 1;
	eff->camera->vp.x = eff->camera->vp.y = 0;
	eff->min_hsize = INT2FIX(MIN(surface->width, surface->height) / 2);
	if (!eff->min_hsize) eff->min_hsize = FIX_ONE;


	/*main surface, set AR*/
	if (surface->render->surface==surface) {
		if (eff->surface->render->compositor->has_size_info) {
			eff->camera->vp.x = INT2FIX(eff->surface->render->out_x); 
			eff->camera->vp.y = INT2FIX(eff->surface->render->out_y);
			eff->camera->vp.width = INT2FIX(eff->surface->render->out_width);
			eff->camera->vp.height = INT2FIX(eff->surface->render->out_height);
			eff->camera->width = INT2FIX(eff->surface->width);
			eff->camera->height = INT2FIX(eff->surface->height);
		} else {
			Fixed sw, sh;
			sw = INT2FIX(eff->surface->render->out_width);
			sh = INT2FIX(eff->surface->render->out_height);
			/*AR changed, rebuild camera*/
			if ((sw!=eff->camera->vp.width) || (sh!=eff->camera->vp.height)) { 
				eff->camera->width = eff->camera->vp.width = INT2FIX(eff->surface->render->out_width);
				eff->camera->height = eff->camera->vp.height = INT2FIX(eff->surface->render->out_height);
				eff->camera->flags |= CAM_IS_DIRTY;
			}
		}
	}
	/*composite surface, no AR*/
	else {
		eff->camera->vp.width = eff->camera->width = INT2FIX(surface->width);
		eff->camera->vp.height = eff->camera->height = INT2FIX(surface->height);
	}

	if (!eff->is_pixel_metrics) {
		if (eff->camera->height > eff->camera->width) {
			eff->camera->height = 2*gf_divfix(eff->camera->height , eff->camera->width);
			eff->camera->width = 2*FIX_ONE;
		} else {
			eff->camera->width = 2 * gf_divfix(eff->camera->width, eff->camera->height);
			eff->camera->height = 2 * FIX_ONE;
		}
	}
	/*setup surface bounds*/
	eff->bbox.max_edge.x = eff->camera->width / 2;
	eff->bbox.min_edge.x = -eff->bbox.max_edge.x;
	eff->bbox.max_edge.y = eff->camera->height / 2;
	eff->bbox.min_edge.y = -eff->bbox.max_edge.y;
	eff->bbox.max_edge.z = eff->bbox.min_edge.z = 0;
	eff->bbox.is_set = 1;
}


void VS_NodeRender(RenderEffect3D *eff, GF_Node *root_node)
{
	GF_Node *fog;
	if (!eff->camera || !eff->surface) return;

	VS_InitRender(eff);

	/*main surface, handle collisions*/
	if ((eff->surface==eff->surface->render->surface) && eff->camera->is_3D) 
		VS_DoCollisions(eff, NULL);
	

	/*setup fog*/
	fog = gf_list_get(eff->surface->fog_stack, 0);
	eff->traversing_mode = TRAVERSE_RENDER_BINDABLE;
	if (Bindable_GetIsBound(fog)) gf_node_render(fog, eff);

	/*turn global lights on*/
	eff->traversing_mode = TRAVERSE_LIGHTING;
	gf_node_render(root_node, eff);
	
	/*sort graph*/
	eff->traversing_mode = TRAVERSE_SORT;
	gf_node_render(root_node, eff);

	/*and draw*/
	VS_FlushContexts(eff->surface, eff);

	/*and turn off lights*/
	VS3D_ClearAllLights(eff->surface);
}


void VS_RootRenderChildren(RenderEffect3D *eff, GF_List *children)
{
	u32 i, count;
	GF_Matrix mx;
	GF_Node *child;

	if (!eff->camera || !eff->surface) return;

	/*don't reset given matrix (used by compositeTexture2D when rescaling to POW2 sizes)*/
	gf_mx_copy(mx, eff->model_matrix);

	VS_InitRender(eff);
	
	/**/
	gf_mx_copy(eff->model_matrix, mx);
	VS3D_MultMatrix(eff->surface, mx.m);

	count = gf_list_count(children);

	eff->traversing_mode = TRAVERSE_LIGHTING;
	for (i=0; i<count; i++) {
		child = gf_list_get(children, i);
		gf_node_render(child, eff);
	}

	eff->traversing_mode = TRAVERSE_SORT;
	for (i=0; i<count; i++) {
		child = gf_list_get(children, i);
		gf_node_render(child, eff);
	}
	/*setup fog*/
	child = gf_list_get(eff->fogs, 0);
	eff->traversing_mode = TRAVERSE_RENDER_BINDABLE;
	if (Bindable_GetIsBound(child)) gf_node_render(child, eff);

	/*and draw*/
	VS_FlushContexts(eff->surface, eff);

	/*and turn off lights*/
	VS3D_ClearAllLights(eff->surface);
}

static GFINLINE Bool appear_has_alpha(RenderEffect3D *eff, GF_Node *node_to_draw)
{
	u32 tag;
	Bool is_mat3D;
	DrawableStack *stack;
	GF_Node *mat = eff->appear ? ((M_Appearance *)eff->appear)->material : NULL;

	is_mat3D = 0;
	if (mat) {
		tag = gf_node_get_tag(mat);
		switch (tag) {
		/*for M2D: if filled & transparent we're transparent - otherwise we must check texture*/
		case TAG_MPEG4_Material2D:
			if (((M_Material2D *)mat)->filled && ((M_Material2D *)mat)->transparency) return 1;
			break;
		case TAG_MPEG4_Material:
		case TAG_X3D_Material:
			is_mat3D = 1;
			if ( ((M_Material *)mat)->transparency) return 1;
			break;
		case TAG_MPEG4_MaterialKey:
			return 1;
			break;
		}
	} else if (eff->camera->is_3D && eff->appear) {
		GF_TextureHandler *txh = R3D_GetTextureHandler(((M_Appearance *)eff->appear)->texture);
		if (txh && txh->transparent) return 1;
	}

	/*check alpha texture in3D or with bitmap*/
	if (is_mat3D || (gf_node_get_tag(((M_Shape *)node_to_draw)->geometry)==TAG_MPEG4_Bitmap)) {
		GF_TextureHandler *txh = R3D_GetTextureHandler(((M_Appearance *)eff->appear)->texture);
		if (txh && txh->transparent) return 1;
	}
	/*TODO - FIXME check alpha only...*/
	if (!eff->color_mat.identity) return 1;

	stack = gf_node_get_private(((M_Shape *)node_to_draw)->geometry);
	if (stack && (stack->mesh->flags & MESH_HAS_ALPHA)) return 1;
	return 0;
}


void VS_RegisterContext(RenderEffect3D *eff, GF_Node *node_to_draw, GF_BBox *bounds, Bool is_shape)
{
	u32 i, count;
	GF_BBox b;
	DLightContext *nl, *ol;
	Draw3DContext *ctx;

	assert(eff->traversing_mode == TRAVERSE_SORT);

	/*if 2D draw in declared order. Otherwise, if no alpha or node is a layer, draw directly*/
	if (!eff->camera->is_3D || !is_shape || !appear_has_alpha(eff, node_to_draw)) {
		eff->traversing_mode = TRAVERSE_RENDER;
		/*layout/form clipper, set it in world coords only*/
		if (eff->has_clip) {
			VS3D_PushMatrix(eff->surface);
			VS3D_ResetMatrix(eff->surface);
			VS3D_SetClipper2D(eff->surface, eff->clipper);
			VS3D_PopMatrix(eff->surface);
		}
		
		gf_node_render(node_to_draw, eff);
	
		/*back to SORT*/
		eff->traversing_mode = TRAVERSE_SORT;

		if (eff->has_clip) VS3D_ResetClipper2D(eff->surface);
		return;
	}


	GF_SAFEALLOC(ctx, sizeof(Draw3DContext));
	ctx->directional_lights = gf_list_new();
	ctx->node_to_draw = node_to_draw;

	memcpy(&ctx->model_matrix, &eff->model_matrix, sizeof(GF_Matrix));
	ctx->color_mat.identity = eff->color_mat.identity;
	if (!eff->color_mat.identity) memcpy(&ctx->color_mat, &eff->color_mat, sizeof(GF_ColorMatrix));

	ctx->is_pixel_metrics = eff->is_pixel_metrics;
	ctx->split_text_idx = eff->split_text_idx;
	
	i=0;
	while ((ol = gf_list_enum(eff->local_lights, &i))) {
		nl = malloc(sizeof(DLightContext));
		memcpy(nl, ol, sizeof(DLightContext));
		gf_list_add(ctx->directional_lights, nl);
	}
	ctx->clipper = eff->clipper;
	ctx->has_clipper = eff->has_clip;
	ctx->cull_flag = eff->cull_flag;

	if ((ctx->num_clip_planes = eff->num_clip_planes))
		memcpy(ctx->clip_planes, eff->clip_planes, sizeof(GF_Plane)*MAX_USER_CLIP_PLANES);

	/*and insert from further to closest*/
	b = *bounds;
	gf_mx_apply_bbox(&ctx->model_matrix, &b);
	gf_mx_apply_bbox(&eff->camera->modelview, &b);
	ctx->zmax = b.max_edge.z;

	/*we don't need an exact sorting, as long as we keep transparent nodes above  -note that for
	speed purposes we store in reverse-z transparent nodes*/
	count = gf_list_count(eff->surface->alpha_nodes_to_draw);
	for (i=0; i<count; i++) {
		Draw3DContext *next = gf_list_get(eff->surface->alpha_nodes_to_draw, i);
		if (next->zmax>ctx->zmax) {
			gf_list_insert(eff->surface->alpha_nodes_to_draw, ctx, i);
			return;
		}
	}
	gf_list_add(eff->surface->alpha_nodes_to_draw, ctx);
}

void VS_FlushContexts(VisualSurface *surf, RenderEffect3D *eff)
{
	u32 i, idx, count;

	eff->traversing_mode = TRAVERSE_RENDER;

	count = gf_list_count(surf->alpha_nodes_to_draw);
	for (idx=0; idx<count; idx++) {
		DLightContext *dl;
		Draw3DContext *ctx = gf_list_get(surf->alpha_nodes_to_draw, idx);

		VS3D_PushMatrix(surf);

		/*apply directional lights*/
		eff->local_light_on = 1;
		i=0;
		while ((dl = gf_list_enum(ctx->directional_lights, &i))) {
			VS3D_PushMatrix(surf);
			VS3D_MultMatrix(surf, dl->light_matrix.m);
			gf_node_render(dl->dlight, eff);
			VS3D_PopMatrix(surf);
		}
		
		/*clipper, set it in world coords only*/
		if (ctx->has_clipper) {
			VS3D_PushMatrix(surf);
			VS3D_ResetMatrix(surf);
			VS3D_SetClipper2D(surf, ctx->clipper);
			VS3D_PopMatrix(surf);
		}

		/*clip planes, set it in world coords only*/
		for (i=0; i<ctx->num_clip_planes; i++)
			VS3D_SetClipPlane(surf, ctx->clip_planes[i]);

		/*restore effect*/
		VS3D_MultMatrix(surf, ctx->model_matrix.m);
		memcpy(&eff->model_matrix, &ctx->model_matrix, sizeof(GF_Matrix));
		eff->color_mat.identity = ctx->color_mat.identity;
		if (!eff->color_mat.identity) memcpy(&eff->color_mat, &ctx->color_mat, sizeof(GF_ColorMatrix));
		eff->split_text_idx = ctx->split_text_idx;
		eff->is_pixel_metrics = ctx->is_pixel_metrics;
		/*restore cull flag in case we're completely inside (avoids final frustum/AABB tree culling)*/
		eff->cull_flag = ctx->cull_flag;

		gf_node_render(ctx->node_to_draw, eff);
		
		/*reset directional lights*/
		eff->local_light_on = 0;
		for (i=gf_list_count(ctx->directional_lights); i>0; i--) {
			DLightContext *dl = gf_list_get(ctx->directional_lights, i-1);
			gf_node_render(dl->dlight, eff);
			free(dl);
		}

		if (ctx->has_clipper) VS3D_ResetClipper2D(surf);
		for (i=0; i<ctx->num_clip_planes; i++) VS3D_ResetClipPlane(surf);

		VS3D_PopMatrix(surf);

		/*and destroy*/
		gf_list_del(ctx->directional_lights);
		free(ctx);
	}
	gf_list_reset(eff->surface->alpha_nodes_to_draw);
}


static void reset_collide_cursor(Render3D *sr)
{
	if (sr->last_cursor == GF_CURSOR_COLLIDE) {
		GF_Event evt;
		sr->last_cursor = evt.cursor.cursor_type = GF_CURSOR_NORMAL;
		evt.type = GF_EVT_SET_CURSOR;
		sr->compositor->video_out->ProcessEvent(sr->compositor->video_out, &evt);
	}
}

Bool VS_ExecuteEvent(VisualSurface *surf, RenderEffect3D *eff, GF_UserEvent *ev, GF_List *node_list)
{
	Fixed x, y;
	SFVec3f start, end;
	SensorHandler *hs, *hs_grabbed;
	SFVec4f res;
	Fixed in_x, in_y;
	GF_List *tmp;
	u32 i, count, stype;
	Render3D *sr = surf->render;

	count = 0;
	if ((ev->event_type > GF_EVT_LEFTUP) || sr->nav_is_grabbed) return 0;

	eff->camera = &surf->camera;
	eff->surface = surf;
	eff->backgrounds = surf->back_stack;
	eff->viewpoints = surf->view_stack;
	eff->fogs = surf->fog_stack;
	eff->navigations = surf->navigation_stack;
	eff->min_hsize = INT2FIX(MIN(surf->width, surf->height)) / 2;
	/*setup projection*/
	VS_SetupProjection(eff);

	eff->traversing_mode = TRAVERSE_PICK;
	eff->collect_layer = NULL;
	sr->hit_info.appear = NULL;

	x = INT2FIX(ev->mouse.x); y = INT2FIX(ev->mouse.y);
	/*main surface with AR*/
	if ((sr->surface == surf) && sr->compositor->has_size_info) {
		Fixed scale = gf_divfix(INT2FIX(surf->width), INT2FIX(sr->out_width));
		x = gf_mulfix(x, scale);
		scale = gf_divfix(INT2FIX(surf->height), INT2FIX(sr->out_height));
		y = gf_mulfix(y, scale);
	}

	start.z = surf->camera.z_near;
	end.z = surf->camera.z_far;
	if (!eff->camera->is_3D && !eff->is_pixel_metrics) {
		start.x = end.x = gf_divfix(x, eff->min_hsize); 
		start.y = end.y = gf_divfix(y, eff->min_hsize);
	} else {
		start.x = end.x = x;
		start.y = end.y = y;
	}

	/*unproject to world coords*/
	in_x = 2*x/ (s32) surf->width;
	in_y = 2*y/ (s32) surf->height;
	
	res.x = in_x; res.y = in_y; res.z = -FIX_ONE; res.q = FIX_ONE;
	gf_mx_apply_vec_4x4(&surf->camera.unprojection, &res);
	if (!res.q) return 0;
	start.x = gf_divfix(res.x, res.q); start.y = gf_divfix(res.y, res.q); start.z = gf_divfix(res.z, res.q);

	res.x = in_x; res.y = in_y; res.z = FIX_ONE; res.q = FIX_ONE;
	gf_mx_apply_vec_4x4(&surf->camera.unprojection, &res);
	if (!res.q) return 0;
	end.x = gf_divfix(res.x, res.q); end.y = gf_divfix(res.y, res.q); end.z = gf_divfix(res.z, res.q);

	eff->ray = gf_ray(start, end);
	/*also update hit info world ray in case we have a grabbed sensor with mouse off*/
	sr->hit_info.world_ray = eff->ray;

#ifndef GPAC_DISABLE_LOG
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] cast ray\n\tOrigin %.4f %.4f %.4f - End %.4f %.4f %.4f\n\tDir %.4f %.4f %.4f\n", 
		FIX2FLT(eff->ray.orig.x), FIX2FLT(eff->ray.orig.y), FIX2FLT(eff->ray.orig.z),
		FIX2FLT(end.x), FIX2FLT(end.y), FIX2FLT(end.z),
		FIX2FLT(eff->ray.dir.x), FIX2FLT(eff->ray.dir.y), FIX2FLT(eff->ray.dir.z)));
#endif

	if (!sr->is_grabbed) sr->hs_grab = NULL;

	sr->sq_dist = 0;
	sr->picked = NULL;
	gf_list_reset(sr->sensors);

	if (!node_list) {
		gf_node_render(gf_sg_get_root_node(sr->compositor->scene), eff);
	} else {
		GF_Node *child;
		i=0;
		while ((child = gf_list_enum(node_list, &i))) {
			gf_node_render(child, eff);
		}
	}
	gf_list_reset(eff->sensors);

	hs_grabbed = NULL;
	stype = GF_CURSOR_NORMAL;
	count = gf_list_count(sr->prev_sensors);
	for (i=0; i<count; i++) {
		hs = gf_list_get(sr->prev_sensors, i);
		if (gf_list_find(sr->sensors, hs) < 0) {
			/*that's a bit ugly but we need coords if pointer out of the shape when sensor grabbed...*/
			if (sr->is_grabbed) {
				hs_grabbed = hs;
				sr->hs_grab = hs;
			} else {
				hs->OnUserEvent(hs, 0, ev->event_type, &sr->hit_info);
			}
		}
	}

	count = gf_list_count(sr->sensors);
	for (i=0; i<count; i++) {
		hs = gf_list_get(sr->sensors, i);
		hs->OnUserEvent(hs, 1, ev->event_type, &sr->hit_info);
		stype = gf_node_get_tag(hs->owner);
		if (hs==hs_grabbed) hs_grabbed = NULL;
	}
	/*switch sensors*/
	tmp = sr->sensors;
	sr->sensors = sr->prev_sensors;
	sr->prev_sensors = tmp;

	/*check if we have a grabbed sensor*/
	if (hs_grabbed) {
		hs_grabbed->OnUserEvent(hs_grabbed, 0, ev->event_type, &sr->hit_info);
		gf_list_reset(sr->prev_sensors);
		gf_list_add(sr->prev_sensors, hs_grabbed);
		stype = gf_node_get_tag(hs_grabbed->owner);
	}

	/*composite texture*/
	if (!stype && sr->hit_info.appear) 
		return r3d_handle_composite_event(sr, ev);

	/*and set cursor*/
	if (sr->last_cursor != GF_CURSOR_COLLIDE) {
		switch (stype) {
		case TAG_MPEG4_Anchor: 
		case TAG_X3D_Anchor: 
			stype = GF_CURSOR_ANCHOR; 
			break;
		case TAG_MPEG4_PlaneSensor2D:
		case TAG_MPEG4_PlaneSensor: 
		case TAG_X3D_PlaneSensor: 
			stype = GF_CURSOR_PLANE; break;
		case TAG_MPEG4_CylinderSensor: 
		case TAG_X3D_CylinderSensor: 
		case TAG_MPEG4_DiscSensor:
		case TAG_MPEG4_SphereSensor:
		case TAG_X3D_SphereSensor:
			stype = GF_CURSOR_ROTATE; break;
		case TAG_MPEG4_ProximitySensor2D:
		case TAG_MPEG4_ProximitySensor:
		case TAG_X3D_ProximitySensor:
			stype = GF_CURSOR_PROXIMITY; break;
		case TAG_MPEG4_TouchSensor: 
		case TAG_X3D_TouchSensor: 
			stype = GF_CURSOR_TOUCH; break;
		default: stype = GF_CURSOR_NORMAL; break;
		}
		if ((stype != GF_CURSOR_NORMAL) || (sr->last_cursor != stype)) {
			GF_Event evt;
			evt.type = GF_EVT_SET_CURSOR;
			evt.cursor.cursor_type = stype;
			sr->compositor->video_out->ProcessEvent(sr->compositor->video_out, &evt);
			sr->last_cursor = stype;
		}
	} else {
		reset_collide_cursor(sr);
	}
	return count ? 1 : 0;
}

void drawable_do_pick(GF_Node *n, RenderEffect3D *eff) 
{
	SFVec3f local_pt, world_pt, vdiff;
	SFVec3f hit_normal;
	SFVec2f text_coords;
	u32 i, count;
	Fixed sqdist;
	Bool node_is_over;
	Render3D *sr;
	GF_Matrix mx;
	GF_Ray r;
	DrawableStack *st = gf_node_get_private(n);
	u32 cull_bckup = eff->cull_flag;
	if (!st) return;

	count = gf_list_count(eff->sensors);

	sr = eff->surface->render;

	node_is_over = 0;
	if (!node_cull(eff, &st->mesh->bounds, 0)) {
		eff->cull_flag = cull_bckup;
		return;
	}
	eff->cull_flag = cull_bckup;
	r = eff->ray;
	gf_mx_copy(mx, eff->model_matrix);
	gf_mx_inverse(&mx);
	gf_mx_apply_ray(&mx, &r);

	/*if we already have a hit point don't check anything below...*/
	if (sr->sq_dist && !sr->hs_grab && !eff->collect_layer) {
		GF_Plane p;
		SFVec3f hit = sr->hit_info.world_point;
		gf_mx_apply_vec(&mx, &hit);
		p.normal = r.dir;
		p.d = -1 * gf_vec_dot(p.normal, hit);
		if (gf_bbox_plane_relation(&st->mesh->bounds, &p) == GF_BBOX_FRONT) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Picking: bounding box of node %s (DEF %s) below current hit point - skipping\n", gf_node_get_class_name(n), gf_node_get_name(n)));
			return;
		}
	}
	if (!st->IntersectWithRay) {
		node_is_over = gf_mesh_intersect_ray(st->mesh, &r, &local_pt, &hit_normal, &text_coords);
	} else {
		node_is_over = st->IntersectWithRay(st->owner, &r, &local_pt, &hit_normal, &text_coords);
	}
	if (!node_is_over) {
		/*store matrices for grabed sensors*/
		if (0 && sr->hs_grab && (gf_list_find(eff->sensors, sr->hs_grab)>=0) ) {
			gf_mx_copy(sr->hit_info.world_to_local, eff->model_matrix);
			gf_mx_copy(sr->hit_info.local_to_world, mx);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Picking: found grabed sensor - storing matrices\n"));
		}
		return;
	}

	/*check distance from user and keep the closest hitpoint*/
	world_pt = local_pt;
	gf_mx_apply_vec(&eff->model_matrix, &world_pt);

	for (i=0; i<eff->num_clip_planes; i++) {
		if (gf_plane_get_distance(&eff->clip_planes[i], &world_pt) < 0) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Picking: node %s (def %s) is not in clipper half space\n", gf_node_get_class_name(n), gf_node_get_name(n)));
			return;
		}
	}

	gf_vec_diff(vdiff, world_pt, eff->ray.orig);
	sqdist = gf_vec_lensq(vdiff);
	if (sr->sq_dist && (sr->sq_dist+FIX_EPSILON<sqdist)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Picking: node %s (def %s) is farther (%g) than current pick (%g)\n", gf_node_get_class_name(n), gf_node_get_name(n), FIX2FLT(sqdist), FIX2FLT(sr->sq_dist)));
		return;
	}

	sr->sq_dist = sqdist;
	gf_list_reset(sr->sensors);
	for (i=0; i<count; i++) {
		gf_list_add(sr->sensors, gf_list_get(eff->sensors, i));
	}

	gf_mx_copy(sr->hit_info.world_to_local, eff->model_matrix);
	gf_mx_copy(sr->hit_info.local_to_world, mx);
	sr->hit_info.local_point = local_pt;
	sr->hit_info.world_point = world_pt;
	sr->hit_info.world_ray = eff->ray;
	sr->hit_info.hit_normal = hit_normal;
	sr->hit_info.hit_texcoords = text_coords;
	if (r3d_has_composite_texture(eff->appear)) {
		sr->hit_info.appear = eff->appear;
	} else {
		sr->hit_info.appear = NULL;
	}
	sr->picked = n;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Picking: node %s (def %s) is under mouse - hit %g %g %g\n", gf_node_get_class_name(n), gf_node_get_name(n),
			FIX2FLT(world_pt.x), FIX2FLT(world_pt.y), FIX2FLT(world_pt.z)));
}

void VS_DoCollisions(RenderEffect3D *eff, GF_List *node_list)
{
	SFVec3f n, pos, dir;
	Bool go;
	u32 i;
	Fixed diff, pos_diff;

	assert(eff->surface && eff->camera);
	/*don't collide on VP animations or when modes discard collision*/
	if ((eff->camera->anim_len && !eff->camera->jumping) || !eff->surface->render->collide_mode || (eff->camera->navigate_mode>=GF_NAVIGATE_EXAMINE)) {
		/*reset ground flag*/
		eff->camera->last_had_ground = 0;
		/*and avoid reseting move at next collision change*/
		eff->camera->last_pos = eff->camera->position;
		return;
	}
	/*don't collide if not moved*/
	if (gf_vec_equal(eff->camera->position, eff->camera->last_pos)) {
		reset_collide_cursor(eff->surface->render);
		return;
	}
	eff->traversing_mode = TRAVERSE_COLLIDE;
	eff->camera->collide_flags = 0;
	eff->camera->collide_dist = FIX_MAX;
	eff->camera->ground_dist = FIX_MAX;
	if ((eff->camera->navigate_mode==GF_NAVIGATE_WALK) && eff->surface->render->gravity_on) eff->camera->collide_flags |= CF_DO_GRAVITY;

	gf_vec_diff(dir, eff->camera->position, eff->camera->last_pos);
	pos_diff = gf_vec_len(dir);
	gf_vec_norm(&dir);
	pos = eff->camera->position;
	
	diff = 0;
	go = 1;
	eff->camera->last_had_col = 0;
	/*some explanation: the current collision detection algo only checks closest distance
	to objects, but doesn't attempt to track object cross during a move. If we step more than
	the collision detection size, we may cross an object without detecting collision. we thus
	break the move in max collision size moves*/
	while (go) {
		if (pos_diff>eff->camera->avatar_size.x) {
			pos_diff-=eff->camera->avatar_size.x;
			diff += eff->camera->avatar_size.x;
		} else {
			diff += pos_diff;
			go = 0;
		}
		n = gf_vec_scale(dir, diff);	
		gf_vec_add(eff->camera->position, eff->camera->last_pos, n);	
		if (!node_list) {
			gf_node_render(gf_sg_get_root_node(eff->surface->render->compositor->scene), eff);
		} else {
			GF_Node *child;
			i=0;
			while ((child = gf_list_enum(node_list, &i))) {
				gf_node_render(child, eff);
			}
		}
		if (eff->camera->collide_flags & CF_COLLISION) break;
		eff->camera->collide_flags &= ~CF_DO_GRAVITY;
	}

	/*gravity*/
	if (eff->camera->collide_flags & CF_GRAVITY) {
		diff = eff->camera->ground_dist - eff->camera->avatar_size.y;
		if (eff->camera->last_had_ground && (-diff>eff->camera->avatar_size.z)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Collision: Obstacle detected - too high (dist %g)\n", diff));
			eff->camera->position = eff->camera->last_pos;
			eff->camera->flags |= CAM_IS_DIRTY;
		} else {
			if ((eff->camera->jumping && fabs(diff)>eff->camera->dheight) 
				|| (!eff->camera->jumping && (ABS(diff)>FIX_ONE/1000) )) {
				eff->camera->last_had_ground = 1;
				n = gf_vec_scale(eff->camera->up, -diff);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Collision: Ground detected camera position: %g %g %g - offset: %g %g %g (dist %g)\n", eff->camera->position.x, eff->camera->position.y, eff->camera->position.z, n.x, n.y, n.z, diff));

				gf_vec_add(eff->camera->position, eff->camera->position, n);
				gf_vec_add(eff->camera->target, eff->camera->target, n);
				gf_vec_add(eff->camera->last_pos, eff->camera->position, n);
				eff->camera->flags |= CAM_IS_DIRTY;
			}
		}
	}
	/*collsion found*/
	if (eff->camera->collide_flags & CF_COLLISION) {
		if (eff->surface->render->last_cursor != GF_CURSOR_COLLIDE) {
			GF_Event evt;
			eff->camera->last_had_col = 1;
			evt.type = GF_EVT_SET_CURSOR;
			eff->surface->render->last_cursor = evt.cursor.cursor_type = GF_CURSOR_COLLIDE;
			eff->surface->render->compositor->video_out->ProcessEvent(eff->surface->render->compositor->video_out, &evt);
		}

		/*regular collision*/
		if (eff->surface->render->collide_mode==GF_COLLISION_NORMAL) {
			eff->camera->position = eff->camera->last_pos;
			eff->camera->flags |= CAM_IS_DIRTY;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Collision: Collision detected - restoring previous avatar position\n"));
		} else {
			/*camera displacement collision*/
			if (eff->camera->collide_dist) {
				if (eff->camera->collide_dist>=eff->camera->avatar_size.x)
					GF_LOG(GF_LOG_WARNING, GF_LOG_RENDER, ("[Render 3D] Collision: Collision distance %g greater than avatar collide size %g\n", eff->camera->collide_dist, eff->camera->avatar_size.x));

				/*safety check due to precision, always stay below collide dist*/
				if (eff->camera->collide_dist>=eff->camera->avatar_size.x) eff->camera->collide_dist = eff->camera->avatar_size.x;

				gf_vec_diff(n, eff->camera->position, eff->camera->collide_point);
				gf_vec_norm(&n);
				n = gf_vec_scale(n, eff->camera->avatar_size.x - eff->camera->collide_dist);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Collision: offseting camera: position: %g %g %g - offset: %g %g %g\n", eff->camera->position.x, eff->camera->position.y, eff->camera->position.z, n.x, n.y, n.z));
				gf_vec_add(eff->camera->position, eff->camera->position, n);
				gf_vec_add(eff->camera->target, eff->camera->target, n);
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Collision: Collision detected and camera on hit point - restoring previous avatar position\n"));
				eff->camera->position = eff->camera->last_pos;
			}
			eff->camera->last_pos = eff->camera->position;
			eff->camera->flags |= CAM_IS_DIRTY;
		}
	} else {
		reset_collide_cursor(eff->surface->render);
		eff->camera->last_pos = eff->camera->position;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Collision: no collision found\n"));
	}

	if (eff->camera->flags & CAM_IS_DIRTY) VS_SetupProjection(eff);
}

void drawable_do_collide(GF_Node *node, RenderEffect3D *eff)
{
	SFVec3f pos, v1, v2, collide_pt, last_pos;
	Fixed dist, m_dist;
	GF_Matrix mx;
	u32 ntag, cull_backup;
	DrawableStack *st = gf_node_get_private(node);
	if (!st) return;

	/*no collision with lines & points*/
	if (st->mesh->mesh_type != MESH_TRIANGLES) return;
	/*no collision with text (vrml)*/
	ntag = gf_node_get_tag(node);
	if ((ntag==TAG_MPEG4_Text) || (ntag==TAG_X3D_Text)) return;


	/*cull but don't use near plane to detect objects behind us*/
	cull_backup = eff->cull_flag;
	if (!node_cull(eff, &st->mesh->bounds, 1)) {
		eff->cull_flag = cull_backup;
		return;
	}
	eff->cull_flag = cull_backup;

	/*use up & front to get an average size of the collision dist in this space*/
	pos = eff->camera->position;
	last_pos = eff->camera->last_pos;
	v1 = camera_get_target_dir(eff->camera);
	v1 = gf_vec_scale(v1, eff->camera->avatar_size.x);
	gf_vec_add(v1, v1, pos);
	v2 = camera_get_right_dir(eff->camera);
	v2 = gf_vec_scale(v2, eff->camera->avatar_size.x);
	gf_vec_add(v2, v2, pos);

	gf_mx_copy(mx, eff->model_matrix);
	gf_mx_inverse(&mx);

	gf_mx_apply_vec(&mx, &pos);
	gf_mx_apply_vec(&mx, &last_pos);
	gf_mx_apply_vec(&mx, &v1);
	gf_mx_apply_vec(&mx, &v2);

	gf_vec_diff(v1, v1, pos);
	gf_vec_diff(v2, v2, pos);
	dist = gf_vec_len(v1);
	m_dist = gf_vec_len(v2);
	if (dist<m_dist) m_dist = dist;

	/*check for any collisions*/
	if (gf_mesh_closest_face(st->mesh, pos, m_dist, &collide_pt)) {
		/*get transformed hit*/
		gf_mx_apply_vec(&eff->model_matrix, &collide_pt);
		gf_vec_diff(v2, eff->camera->position, collide_pt);
		dist = gf_vec_len(v2);
		if (dist<eff->camera->collide_dist) {
			eff->camera->collide_dist = dist;
			eff->camera->collide_flags |= CF_COLLISION;
			eff->camera->collide_point = collide_pt;

#ifndef GPAC_DISABLE_LOG
			if ((gf_log_get_level() >= GF_LOG_DEBUG) && (gf_log_get_tools() & GF_LOG_RENDER)) { 				
				gf_vec_diff(v1, pos, collide_pt);
				gf_vec_norm(&v1);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Collision: found at %g %g %g (WC) - dist (%g) - local normal %g %g %g\n", 
					eff->camera->collide_point.x, eff->camera->collide_point.y, eff->camera->collide_point.z, 
					dist, 
					v1.x, v1.y, v1.z));
			}
#endif
		} 
		else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Collision: Existing collision (dist %g) closer than current collsion (dist %g)\n", eff->camera->collide_dist, dist));
		}
	}

	if (eff->camera->collide_flags & CF_DO_GRAVITY) {
		GF_Ray r;
		Bool intersect;
		r.orig = eff->camera->position;
		r.dir = gf_vec_scale(eff->camera->up, -FIX_ONE);
		gf_mx_apply_ray(&mx, &r);

		if (!st->IntersectWithRay) {
			intersect = gf_mesh_intersect_ray(st->mesh, &r, &collide_pt, &v1, NULL);
		} else {
			intersect = st->IntersectWithRay(st->owner, &r, &collide_pt, &v1, NULL);
		}

		if (intersect) {
			gf_mx_apply_vec(&eff->model_matrix, &collide_pt);
			gf_vec_diff(v2, eff->camera->position, collide_pt);
			dist = gf_vec_len(v2);
			if (dist<eff->camera->ground_dist) {
				eff->camera->ground_dist = dist;
				eff->camera->collide_flags |= CF_GRAVITY;
				eff->camera->ground_point = collide_pt;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Collision: Ground found at %g %g %g (WC) - dist %g - local normal %g %g %g\n", 
					eff->camera->ground_point.x, eff->camera->ground_point.y, eff->camera->ground_point.z, 
					dist, 
					v1.x, v1.y, v1.z));
			} 
			else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 3D] Collision: Existing ground (dist %g) closer than current (dist %g)\n", eff->camera->ground_dist, dist));
			}
		} 
	}
}

