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

/*stack used by CompositeTexture2D and CompositeTexture3D*/
typedef struct _composite_texture
{
	GF_TextureHandler txh;
	/*the surface object handling the texture*/
	struct _visual_surface *surface;
	Bool is_pm, first;
	/*for 2D composite texture*/
	Fixed sx, sy;
} CompositeTextureStack;


static void DestroyCompositeTexture(GF_Node *node)
{
	CompositeTextureStack *st = (CompositeTextureStack *) gf_node_get_private(node);
	assert(!st->txh.data);
	gf_sr_texture_destroy(&st->txh);
	VS_Delete(st->surface);
	free(st);
}

static void Composite_GetPixelSize(GF_Node *n, s32 *pw, s32 *ph)
{
	*pw = *ph = 0;
	if (!n) return;
	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_CompositeTexture3D:
		*pw = ((M_CompositeTexture3D*)n)->pixelWidth;
		*ph = ((M_CompositeTexture3D*)n)->pixelHeight;
		break;
	case TAG_MPEG4_CompositeTexture2D:
		*pw = ((M_CompositeTexture2D*)n)->pixelWidth;
		*ph = ((M_CompositeTexture2D*)n)->pixelHeight;
		break;
	}
	if (*pw<0) *pw = 0;
	if (*ph<0) *ph = 0;
}

static void Composite_SetPixelSize(GF_Node *n, s32 pw, s32 ph)
{
	if (!n) return;
	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_CompositeTexture3D:
		((M_CompositeTexture3D*)n)->pixelWidth = pw;
		((M_CompositeTexture3D*)n)->pixelHeight = ph;
		break;
	case TAG_MPEG4_CompositeTexture2D:
		((M_CompositeTexture2D*)n)->pixelWidth = pw;
		((M_CompositeTexture2D*)n)->pixelHeight = ph;
		break;
	}
}

static Bool Composite_CheckBindables(GF_Node *n, RenderEffect3D *eff, Bool force_check)
{
	GF_Node *btop;
	Bool ret = 0;
	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_CompositeTexture3D: 
	{
		M_CompositeTexture3D*c3d = (M_CompositeTexture3D*)n;
		if (force_check || gf_node_dirty_get(c3d->background)) { gf_node_render(c3d->background, eff); ret = 1; }
		btop = gf_list_get(eff->backgrounds, 0);
		if (btop != c3d->background) { 
			gf_node_unregister(c3d->background, n);
			gf_node_register(btop, n); 
			c3d->background = btop;
			gf_node_event_out_str(n, "background");
			ret = 1; 
		}
		if (force_check || gf_node_dirty_get(c3d->viewpoint)) { gf_node_render(c3d->viewpoint, eff); ret = 1; }
		btop = gf_list_get(eff->viewpoints, 0);
		if (btop != c3d->viewpoint) { 
			gf_node_unregister(c3d->viewpoint, n);
			gf_node_register(btop, n); 
			c3d->viewpoint = btop; 
			gf_node_event_out_str(n, "viewpoint");
			ret = 1; 
		}
		
		if (force_check || gf_node_dirty_get(c3d->fog)) { gf_node_render(c3d->fog, eff); ret = 1; }
		btop = gf_list_get(eff->fogs, 0);
		if (btop != c3d->fog) {
			gf_node_unregister(c3d->fog, n);
			gf_node_register(btop, n); 
			c3d->fog = btop;
			gf_node_event_out_str(n, "fog");
			ret = 1;
		}
		
		if (force_check || gf_node_dirty_get(c3d->navigationInfo)) { gf_node_render(c3d->navigationInfo, eff); ret = 1; }
		btop = gf_list_get(eff->navigations, 0);
		if (btop != c3d->navigationInfo) {
			gf_node_unregister(c3d->navigationInfo, n);
			gf_node_register(btop, n); 
			c3d->navigationInfo = btop;
			gf_node_event_out_str(n, "navigationInfo");
			ret = 1;
		}
		return ret;
	}
	case TAG_MPEG4_CompositeTexture2D: 
	{
		M_CompositeTexture2D *c2d = (M_CompositeTexture2D*)n;
		if (force_check || gf_node_dirty_get(c2d->background)) { gf_node_render(c2d->background, eff); ret = 1; }
		btop = gf_list_get(eff->backgrounds, 0);
		if (btop != c2d->background) {
			gf_node_unregister(c2d->background, n);
			gf_node_register(btop, n); 
			c2d->background = btop;
			gf_node_event_out_str(n, "background");
			ret = 1;
		}

		if (force_check || gf_node_dirty_get(c2d->viewport)) { gf_node_render(c2d->viewport, eff); ret = 1; }
		btop = gf_list_get(eff->viewpoints, 0);
		if (btop != c2d->viewport) { 
			gf_node_unregister(c2d->viewport, n);
			gf_node_register(btop, n); 
			c2d->viewport = btop;
			gf_node_event_out_str(n, "viewport");
			ret = 1;
		}
		
		return ret;
	}
	}
	return 0;
}



static GF_List *Composite_GetChildren(GF_Node *n)
{
	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_CompositeTexture3D: return ((M_CompositeTexture3D*)n)->children;
	case TAG_MPEG4_CompositeTexture2D: return ((M_CompositeTexture2D*)n)->children;
	default: return NULL;
	}
}

static void UpdateCompositeTexture(GF_TextureHandler *txh)
{
	s32 w, h;
	GF_BBox box;
	u32 i, count;
	GF_Node *child;
	RenderEffect3D *eff;
	GF_List *children;
	Bool is_dirty;

	CompositeTextureStack *st = (CompositeTextureStack *) gf_node_get_private(txh->owner);
	Render3D *sr = (Render3D *)txh->compositor->visual_renderer->user_priv;

	if (st->txh.hwtx && st->txh.compositor->reset_graphics) tx_delete(&st->txh);

	Composite_GetPixelSize(st->txh.owner, &w, &h);
	if (!w || !h) return;

	if (!st->txh.hwtx || ((s32) st->txh.width != w) || ( (s32) st->txh.height != h) ) {
		tx_delete(&st->txh);

		/*we don't use rect ext because of no support for texture transforms*/
		if (sr->hw_caps.npot_texture ) {
			st->txh.width = w;
			st->txh.height = h;
		} else {
			st->txh.width = 2;
			while (st->txh.width<(u32)w) st->txh.width*=2;
			st->txh.height = 2;
			while (st->txh.height<(u32)h) st->txh.height*=2;

			st->sx = INT2FIX(st->txh.width) / w;
			st->sy = INT2FIX(st->txh.height) / h;

			Composite_SetPixelSize(st->txh.owner, st->txh.width, st->txh.height);
		}

		/*generate texture*/
#if 1
		st->txh.pixelformat = GF_PIXEL_RGBA;
		st->txh.stride = st->txh.width*4;
		st->txh.transparent = 1;
#else
		st->txh.pixelformat = GF_PIXEL_RGB_24;
		st->txh.stride = st->txh.width*3;
#endif
		st->surface->width = st->txh.width;
		st->surface->height = st->txh.height;

		tx_allocate(&st->txh);
		
		st->txh.data = malloc(sizeof(unsigned char) * st->txh.stride * st->txh.height);
		tx_setup_format(&st->txh);
		tx_set_image(&st->txh, 0);
		free(st->txh.data);
		st->txh.data = NULL;
	}

	children = Composite_GetChildren(st->txh.owner);
	/*note: we never clear the dirty state of the composite texture, so that any child invalidated
	doesn't invalidate the parent graph*/
	is_dirty = 0;
	count = gf_list_count(children);
	for (i=0; i<count; i++) {
		child = gf_list_get(children, i);
		if (gf_node_dirty_get(child)) {
			is_dirty = 1;
			break;
		}
	}
	if (st->first) st->is_pm = gf_sg_use_pixel_metrics(gf_node_get_graph(txh->owner));

	eff = effect3d_new();
	eff->surface = st->surface;
	gf_mx_init(eff->model_matrix);
	gf_cmx_init(&eff->color_mat);
	eff->is_pixel_metrics = st->is_pm;
	VS_SetupEffects(st->surface, eff);
	eff->traversing_mode = TRAVERSE_GET_BOUNDS;

	/*children dirty, get bounds*/
	if (is_dirty) {
		/*store bbox for background2D*/
		box = eff->bbox;
		for (i=0; i<count; i++) {
			child = gf_list_get(children, i);
			gf_node_render(child, eff);
		}
		eff->bbox = box;
	}

	if (!st->surface->camera.is_3D) gf_mx_add_scale(&eff->model_matrix, st->sx, st->sy, 1);

	/*check bindables*/
	if (Composite_CheckBindables(st->txh.owner, eff, st->first)) is_dirty = 1;
	st->first = 0;

	if (!is_dirty) {
		effect3d_delete(eff);
		return;
	}

	/*setup GL*/
	VS3D_Setup(st->surface);

	/*render*/
	VS_RootRenderChildren(eff, children);

	tx_copy_to_texture(&st->txh);
	effect3d_delete(eff);
}

GF_TextureHandler *r3d_composite_get_texture(GF_Node *node)
{
	CompositeTextureStack *st = (CompositeTextureStack *) gf_node_get_private(node);
	return &st->txh;
}

Bool r3d_has_composite_texture(GF_Node *appear)
{
	u32 tag;
	if (!appear) return 0;
	tag = gf_node_get_tag(appear);
	if ((tag==TAG_MPEG4_Appearance) || (tag==TAG_X3D_Appearance)) {
		M_Appearance *ap = (M_Appearance *)appear;
		if (!ap->texture) return 0;
		switch (gf_node_get_tag(((M_Appearance *)appear)->texture)) {
		case TAG_MPEG4_CompositeTexture2D:
		case TAG_MPEG4_CompositeTexture3D:
			return 1;
		}
	}
	return 0;
}

void R3D_CompositeAdjustScale(GF_Node *node, Fixed *sx, Fixed *sy)
{
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_CompositeTexture2D:
	case TAG_MPEG4_CompositeTexture3D:
	{
		CompositeTextureStack *st = (CompositeTextureStack *) gf_node_get_private(node);
		(*sx) = gf_divfix(*sx, st->sx);
		(*sy) = gf_divfix(*sy, st->sy);
		break;
	}
	default:
		return;
	}
}


void R3D_InitCompositeTexture3D(Render3D *sr, GF_Node *node)
{
	CompositeTextureStack *st = malloc(sizeof(CompositeTextureStack));
	memset(st, 0, sizeof(CompositeTextureStack));
	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.flags = GF_SR_TEXTURE_REPEAT_S | GF_SR_TEXTURE_REPEAT_T;
	st->first = 1;
	/*create composite surface*/
	st->surface = VS_New();
	st->surface->camera.is_3D = 1;
	camera_invalidate(&st->surface->camera);
	st->surface->render = sr;
	st->txh.update_texture_fcnt = UpdateCompositeTexture;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyCompositeTexture);
}


void R3D_InitCompositeTexture2D(Render3D *sr, GF_Node *node)
{
	CompositeTextureStack *st = malloc(sizeof(CompositeTextureStack));
	memset(st, 0, sizeof(CompositeTextureStack));
	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.flags = GF_SR_TEXTURE_REPEAT_S | GF_SR_TEXTURE_REPEAT_T;
	st->first = 1;
	/*create composite surface*/
	st->surface = VS_New();
	st->surface->camera.is_3D = 0;
	camera_invalidate(&st->surface->camera);
	st->surface->render = sr;
	st->txh.update_texture_fcnt = UpdateCompositeTexture;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyCompositeTexture);
}


Bool r3d_handle_composite_event(Render3D *sr, GF_UserEvent *ev)
{
	CompositeTextureStack *st;
	GF_Matrix mx;
	GF_Node *child;
	RenderEffect3D *eff;
	GF_List *children;
	Bool res;
	u32 i;
	SFVec3f txcoord;
	M_Appearance *ap = (M_Appearance *)sr->hit_info.appear;
	assert(ap && ap->texture);

	if (ev->event_type > GF_EVT_LEFTUP) return 0;

	st = (CompositeTextureStack *) gf_node_get_private(ap->texture);

	txcoord.x = sr->hit_info.hit_texcoords.x;
	txcoord.y = sr->hit_info.hit_texcoords.y;
	txcoord.z = 0;
	if (tx_get_transform(&st->txh, ap->textureTransform, &mx)) {
		/*tx coords are inverted when mapping, thus applying directly the matrix will give us the 
		untransformed coords*/
		gf_mx_apply_vec(&mx, &txcoord);
		while (txcoord.x<0) txcoord.x += FIX_ONE; while (txcoord.x>FIX_ONE) txcoord.x -= FIX_ONE;
		while (txcoord.y<0) txcoord.y += FIX_ONE; while (txcoord.y>FIX_ONE) txcoord.y -= FIX_ONE;
	}
	/*convert to tx space*/
	ev->mouse.x = FIX2INT(txcoord.x - FIX_ONE/2) * st->surface->width;
	ev->mouse.y = FIX2INT(txcoord.y - FIX_ONE/2) * st->surface->height;

	eff = effect3d_new();
	eff->surface = st->surface;
	eff->traversing_mode = TRAVERSE_RENDER;
	gf_mx_init(eff->model_matrix);
	gf_cmx_init(&eff->color_mat);
	eff->is_pixel_metrics = st->is_pm;
	VS_SetupEffects(st->surface, eff);

	children = Composite_GetChildren(st->txh.owner);
	i=0;
	while ((child = gf_list_enum(children, &i))) {
		SensorHandler *hsens = r3d_get_sensor_handler(child);
		if (hsens) gf_list_add(eff->sensors, hsens);
	}
	res = VS_ExecuteEvent(st->surface, eff, ev, children);
	effect3d_delete(eff);
	return res;
}



GF_TextureHandler *R3D_GetTextureHandler(GF_Node *n)
{
	if (!n) return NULL;
	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_CompositeTexture2D: return r3d_composite_get_texture(n);
	case TAG_MPEG4_CompositeTexture3D: return r3d_composite_get_texture(n);
	case TAG_MPEG4_LinearGradient: return r3d_lg_get_texture(n);
	case TAG_MPEG4_RadialGradient: return r3d_rg_get_texture(n);
/*	
	case TAG_MPEG4_MatteTexture: return matte_get_texture(n);
*/
	default: return gf_sr_texture_get_handler(n);
	}
}
