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



#ifndef NODE_STACKS_H
#define NODE_STACKS_H

#include "grouping.h"

void R2D_InitAnchor(Render2D *sr, GF_Node *node);
SensorHandler *r2d_anchor_get_handler(GF_Node *n);

void R2D_InitBackground2D(Render2D *sr, GF_Node *node);
void R2D_Background2DModified(GF_Node *node);
DrawableContext *b2D_GetContext(M_Background2D *ptr, GF_List *from_stack);

void R2D_InitBitmap(Render2D *sr, GF_Node *node);


/*CompositeTexture2D*/
void R2D_InitCompositeTexture2D(Render2D *sr, GF_Node *node);
GF_TextureHandler *r2d_ct2D_get_texture(GF_Node *node);
Bool CT2D_has_sensors(GF_TextureHandler *txh);
DrawableContext *CT2D_FindNode(GF_TextureHandler *txh, DrawableContext *ctx, Fixed x, Fixed y);
void Composite2D_ResetGraphics(GF_TextureHandler *txh);
/* this is to use carefully: picks a node based on the PREVIOUS frame state (no traversing)*/
GF_Node *CT2D_PickNode(GF_TextureHandler *txh, DrawableContext *ctx, Fixed x, Fixed y);


void R2D_InitDiscSensor(Render2D *sr, GF_Node *node);
SensorHandler *r2d_ds_get_handler(GF_Node *n);
void R2D_InitForm(Render2D *sr, GF_Node *node);
void R2D_InitLayer2D(Render2D *sr, GF_Node *node);
void R2D_InitLayout(Render2D *sr, GF_Node *node);
void R2D_LayoutModified(GF_Node *node);

void R2D_InitMatteTexture(Render2D *sr, GF_Node *node);
void R2D_MatteTextureModified(GF_Node *node);
GF_TextureHandler *r2d_matte_get_texture(GF_Node *node);
void R2D_InitOrderedGroup(Render2D *sr, GF_Node *node);
void R2D_InitPlaneSensor2D(Render2D *sr, GF_Node *node);
SensorHandler *r2d_ps2D_get_handler(GF_Node *n);
void R2D_InitProximitySensor2D(Render2D *sr, GF_Node *node);
SensorHandler *r2d_prox2D_get_handler(GF_Node *n);
void R2D_InitSwitch(Render2D *sr, GF_Node *node);

typedef struct
{
	Render2D *sr;
	u32 last_mod_time;
} LinePropStack;
u32 R2D_LP_GetLastUpdateTime(GF_Node *node);


/*exported to access the strike list ...*/
typedef struct
{
	Drawable *graph;
	Fixed ascent, descent;
	GF_List *text_lines;
	GF_Rect bounds;
	Bool gf_sr_texture_text_flag;
} TextStack2D;

void R2D_InitText(Render2D *sr, GF_Node *node);
void R2D_InitTextureText(Render2D *sr, GF_Node *node);

void R2D_InitTouchSensor(Render2D *sr, GF_Node *node);
SensorHandler *r2d_touch_sensor_get_handler(GF_Node *n);

/*viewport*/
void R2D_InitViewport(Render2D *sr, GF_Node *node);
void vp_setup(GF_Node *n, RenderEffect2D *eff, GF_Rect *surf_clip);

/*used by laser so exported*/
typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GROUPINGNODESTACK2D
	GF_Matrix2D mat;
	Bool is_identity;
} Transform2DStack;

void R2D_InitLinearGradient(Render2D *sr, GF_Node *node);
GF_TextureHandler *r2d_lg_get_texture(GF_Node *node);
void R2D_InitRadialGradient(Render2D *sr, GF_Node *node);
GF_TextureHandler *r2d_rg_get_texture(GF_Node *node);
void R2D_InitColorTransform(Render2D *sr, GF_Node *node);
void R2D_InitCircle(Render2D *sr, GF_Node *node);
void R2D_InitCurve2D(Render2D *sr, GF_Node *node);
void R2D_InitEllipse(Render2D *sr, GF_Node *node);
void R2D_InitGroup(Render2D *sr, GF_Node *node);
void R2D_InitIFS2D(Render2D *sr, GF_Node *node);
void R2D_InitILS2D(Render2D *sr, GF_Node *node);
void R2D_InitPointSet2D(Render2D *sr, GF_Node *node);
void R2D_InitRectangle(Render2D *sr, GF_Node *node);
void R2D_InitShape(Render2D *sr, GF_Node *node);
void R2D_InitTransform2D(Render2D *sr, GF_Node *node);
void R2D_InitTransformMatrix2D(Render2D *sr, GF_Node *node);
void TM2D_GetMatrix(GF_Node *n, GF_Matrix2D *mat);
void R2D_InitPathLayout(Render2D *sr, GF_Node *node);
void R2D_InitSound2D(Render2D *sr, GF_Node *node);

#endif


