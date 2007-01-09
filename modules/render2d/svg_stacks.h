/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Rendering sub-project
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
#ifndef _SVG_STACKS_H
#define _SVG_STACKS_H

#include "render2d.h"

#ifndef GPAC_DISABLE_SVG
#include <gpac/nodes_svg.h>

/* Common stack for Textures */

/* Reusing generic 2D stacks for rendering */
#include "stacks2d.h"

void SVG_Render_base(GF_Node *node, RenderEffect2D *eff, SVGPropertiesPointers *backup_props, u32 *backup_flags);
void svg_render_node(GF_Node *node, RenderEffect2D *eff);
void svg_render_node_list(GF_ChildNodeItem *children, RenderEffect2D *eff);
void svg_get_nodes_bounds(GF_Node *self, GF_ChildNodeItem *children, RenderEffect2D *eff);


/* Creates a rendering context and Translates the SVG Styling properties into a context
   that the renderer can understand */
DrawableContext *SVG_drawable_init_context(Drawable *node, RenderEffect2D *eff);

/* Updates the SVG Styling Properties of the renderer (render_svg_props) with the properties
   of the current SVG element (current_svg_props). Only the properties in current_svg_props 
   with a value different than inherit are updated.
   This function implements inheritance. */
void SVGApplyProperties(SVGPropertiesPointers *render_svg_props, SVGProperties *current_svg_props);

/* Basic shapes rendering functions */
void SVG_Init_svg(Render2D *sr, GF_Node *node);
void SVG_Init_g(Render2D *sr, GF_Node *node);
void SVG_Init_switch(Render2D *sr, GF_Node *node);
void SVG_Init_rect(Render2D *sr, GF_Node *node);
void SVG_Init_circle(Render2D *sr, GF_Node *node);
void SVG_Init_ellipse(Render2D *sr, GF_Node *node);
void SVG_Init_line(Render2D *sr, GF_Node *node);
void SVG_Init_polyline(Render2D *sr, GF_Node *node);
void SVG_Init_polygon(Render2D *sr, GF_Node *node);
void SVG_Init_path(Render2D *sr, GF_Node *node);

/* Text rendering functions */
void SVG_Init_text(Render2D *sr, GF_Node *node);

/* Interactive functions */
void SVG_Init_a(Render2D *se, GF_Node *node);

#define BASE_IMAGE_STACK 	\
	GF_TextureHandler txh; \
	Drawable *graph; \
	MFURL txurl;

typedef struct {
	BASE_IMAGE_STACK
} SVG_image_stack;
void SVG_Init_image(Render2D *se, GF_Node *node);

typedef struct
{
	BASE_IMAGE_STACK
	Bool first_frame_fetched;
} SVG_video_stack;
void SVG_Init_video(Render2D *se, GF_Node *node);


typedef struct
{
	GF_AudioInput input;
	Bool is_active;
	MFURL aurl;
} SVG_audio_stack;
void SVG_Init_audio(Render2D *se, GF_Node *node);

void SVG_Init_linearGradient(Render2D *sr, GF_Node *node);
void SVG_Init_radialGradient(Render2D *sr, GF_Node *node);
GF_TextureHandler *svg_gradient_get_texture(GF_Node *node);


void SVG_Init_solidColor(Render2D *sr, GF_Node *node);
void SVG_Init_stop(Render2D *sr, GF_Node *node);

void gf_svg_apply_local_transformation(RenderEffect2D *eff, GF_Node *node, GF_Matrix2D *backup_matrix);
void gf_svg_restore_parent_transformation(RenderEffect2D *eff, GF_Matrix2D *backup_matrix);

#endif //GPAC_DISABLE_SVG

#endif //_SVG_STACKS_H

