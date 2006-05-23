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

typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GF_Mesh *mesh;
	Bool (*IntersectWithRay)(GF_Node *owner, GF_Ray *ray, SFVec3f *outPoint, SFVec3f *outNormal, SFVec2f *outTexCoords);
	Bool (*ClosestFace)(GF_Node *owner, SFVec3f user_pos, Fixed min_dist, SFVec3f *outPoint);
	SFVec2f size;
} BitmapStack;


static Bool BitmapIntersectWithRay(GF_Node *owner, GF_Ray *ray, SFVec3f *outPoint, SFVec3f *outNormal, SFVec2f *outTexCoords)
{
	Bool inside;
	BitmapStack *st = (BitmapStack *)gf_node_get_private(owner);

	if (!R3D_Get2DPlaneIntersection(ray, outPoint)) return 0;

	/*note this is broken when a single bitmap is used with # texture*/
	inside = ( (outPoint->x >= -st->size.x/2) && (outPoint->y >= -st->size.y/2) 
			&& (outPoint->x <= st->size.x/2) && (outPoint->y <= st->size.y/2) 
			) ? 1 : 0;

	if (!inside) return 0;

	if (outNormal) { outNormal->x = outNormal->y = 0; outNormal->z = 1; }
	if (outTexCoords) { 
		outTexCoords->x = gf_divfix(outPoint->x, st->size.x) + FIX_ONE/2;
		outTexCoords->y = gf_divfix(outPoint->y, st->size.y) + FIX_ONE/2;
	}
	return 1;
}


static void RenderBitmap(GF_Node *node, void *rs)
{
	Aspect2D asp;
#ifndef GPAC_USE_OGL_ES
	Fixed x, y;
	char *data;
	u32 format;
#endif
	GF_TextureHandler *txh;
	Fixed sx, sy;
	SFVec2f size;
	BitmapStack *st = (BitmapStack *)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	M_Bitmap *bmp = (M_Bitmap *)st->owner;
	Render3D *sr = (Render3D*)st->compositor->visual_renderer->user_priv;

	if (!eff->appear) return;
	if (! ((M_Appearance *)eff->appear)->texture) return;
	txh = R3D_GetTextureHandler(((M_Appearance *)eff->appear)->texture);
	if (!txh || !txh->hwtx || !txh->width || !txh->height) return;
	
	sx = bmp->scale.x; if (sx<0) sx = FIX_ONE;
	sy = bmp->scale.y; if (sy<0) sy = FIX_ONE;

	R3D_CompositeAdjustScale(txh->owner, &sx, &sy);

	/*check size change*/
	size.x = txh->width*sx;
	size.y = txh->height*sy;
	/*if we have a PAR update it!!*/
	if (txh->pixel_ar) {
		u32 n = (txh->pixel_ar>>16) & 0xFFFF;
		u32 d = (txh->pixel_ar) & 0xFFFF;
		size.x = ( (txh->width * n) / d) * sx;
	}


	/*we're in meter metrics*/
	if (!eff->is_pixel_metrics) {
		size.x = gf_divfix(size.x, eff->min_hsize);
		size.y = gf_divfix(size.y, eff->min_hsize);
	}
	if ((st->size.x != size.x) || (st->size.y != size.y) /*|| (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY)*/) {
		st->size = size;
		mesh_new_rectangle(st->mesh, size);
		if (eff->traversing_mode!=TRAVERSE_GET_BOUNDS) gf_node_dirty_set(node, 0, 1);
		gf_node_dirty_clear(node, 0);
	}

	if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
		return;
	}

	VS_GetAspect2D(eff, &asp);

	/*texture is available in hw, use it - if blending, force using texture*/
	if (!tx_needs_reload(txh) || (asp.alpha!=FIX_ONE) || !sr->bitmap_use_pixels) {
		if (tx_set_image(txh, 0)) {
			VS3D_SetState(eff->surface, F3D_LIGHT, 0);
			VS3D_SetAntiAlias(eff->surface, 0);
			if (asp.alpha != FIX_ONE) {
				VS3D_SetMaterial2D(eff->surface, asp.fill_color, asp.alpha);
				tx_set_blend_mode(txh, TX_MODULATE);
			} else if (tx_is_transparent(txh)) {
				tx_set_blend_mode(txh, TX_REPLACE);
			} else {
				VS3D_SetState(eff->surface, F3D_BLEND, 0);
			}
			/*ignore texture transform for bitmap*/
 			eff->mesh_has_texture = tx_enable(txh, NULL);
			if (eff->mesh_has_texture) {
				VS3D_DrawMesh(eff, st->mesh);
 				tx_disable(txh);
				eff->mesh_has_texture = 0;
			}
			return;
		}
	}

	/*otherwise use glDrawPixels*/
#ifndef GPAC_USE_OGL_ES
	data = tx_get_data(txh, &format);
	if (!data) return;

	x = INT2FIX(txh->width) / -2;
	y = INT2FIX(txh->height) / 2;

	{
	Fixed g[16];

	/*add top level scale if any*/
	sx = gf_mulfix(sx, sr->scale_x);
	sy = gf_mulfix(sy, sr->scale_y);

	/*get & apply current transform scale*/
	VS3D_GetMatrix(eff->surface, MAT_MODELVIEW, g);

	if (g[0]<0) g[0] *= -FIX_ONE;
	if (g[5]<0) g[5] *= -FIX_ONE;
	sx = gf_mulfix(sx, g[0]);
	sy = gf_mulfix(sy, g[5]);
	x = gf_mulfix(x, sx);
	y = gf_mulfix(y, sy);

	}
	VS3D_DrawImage(eff->surface, x, y, txh->width, txh->height, format, data, sx, sy);
#endif
}


static void Bitmap_PreDestroy(GF_Node *n)
{
	BitmapStack *st = (BitmapStack *)gf_node_get_private(n);
	mesh_free(st->mesh);
	free(st);
}

void R3D_InitBitmap(Render3D *sr, GF_Node *node)
{
	BitmapStack *st = GF_SAFEALLOC(st, sizeof(BitmapStack));
	stack_setup(st, node, sr->compositor);
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, Bitmap_PreDestroy);
	gf_node_set_render_function(node, RenderBitmap);
	st->IntersectWithRay = BitmapIntersectWithRay;
}
