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

#include "../render2d.h"

#ifndef GPAC_DISABLE_SVG
#include <gpac/nodes_svg.h>

/* Common stack for Textures */

/* Reusing generic 2D stacks for rendering */
#include "../stacks2d.h"

/* Importing SMIL stacks for animation */
#include "../smil/smil_stacks.h"

/* Creates a rendering context and Translates the SVG Styling properties into a context
   that the renderer can understand */
DrawableContext *SVG_drawable_init_context(Drawable *node, RenderEffect2D *eff);

/* Updates the SVG Styling Properties of the renderer (render_svg_props) with the properties
   of the current SVG element (current_svg_props). Only the properties in current_svg_props 
   with a value different than inherit are updated.
   This function implements inheritance. */
void SVGApplyProperties(SVGStylingProperties *render_svg_props, SVGStylingProperties current_svg_props);

/* Basic shapes rendering functions */
void SVG_Init_svg(Render2D *sr, GF_Node *node);
void SVG_Init_g(Render2D *sr, GF_Node *node);
void SVG_Init_rect(Render2D *sr, GF_Node *node);
void SVG_Init_circle(Render2D *sr, GF_Node *node);
void SVG_Init_ellipse(Render2D *sr, GF_Node *node);
void SVG_Init_line(Render2D *sr, GF_Node *node);
void SVG_Init_polyline(Render2D *sr, GF_Node *node);
void SVG_Init_polygon(Render2D *sr, GF_Node *node);
void SVG_Init_path(Render2D *sr, GF_Node *node);

void SVG_Init_use(Render2D *se, GF_Node *node);

/* Text rendering functions */
void SVG_Init_text(Render2D *sr, GF_Node *node);

/* SVG Specific Animation functions */
void SVG_Init_set(Render2D *sr, GF_Node *node);
void SVG_Init_animate(Render2D *sr, GF_Node *node);
void SVG_Init_animateTransform(Render2D *sr, GF_Node *node);
void SVG_Init_animateMotion(Render2D *sr, GF_Node *node);
void SVG_Init_animateColor(Render2D *sr, GF_Node *node);
void SVG_Init_discard(Render2D *sr, GF_Node *node);

/* Interactive functions */
SensorHandler *SVG_GetHandler_a(GF_Node *n);
void SVG_Init_a(Render2D *se, GF_Node *node);

/* Media rendering functions */
typedef struct {
	GF_TextureHandler txh;
	Drawable *graph;
	MFURL txurl;
} SVG_image_stack;

void SVG_Init_image(Render2D *se, GF_Node *node);

typedef struct
{
	GF_TextureHandler txh;
	Drawable *graph;
	GF_TimeNode time_handle;
	Bool fetch_first_frame, first_frame_fetched;
	Double start_time;
	Bool isActive;
	MFURL txurl;
} SVG_video_stack;
void SVG_Init_video(Render2D *se, GF_Node *node);

typedef struct
{
	GF_AudioInput input;
	GF_TimeNode time_handle;
	Bool is_active;
	Double start_time;
	MFURL aurl;
} SVG_audio_stack;
void SVG_Init_audio(Render2D *se, GF_Node *node);

#endif

#endif

