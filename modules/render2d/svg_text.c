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


#include "svg_stacks.h"

#ifndef GPAC_DISABLE_SVG

#include <gpac/utf.h>
#include "visualsurface2d.h"

typedef struct 
{
	Drawable *draw;
	Fixed prev_size;
	u32 prev_flags;
	u32 prev_anchor;
} SVG_TextStack;

/* TODO: implement all the missing features: horizontal/vertical, ltr/rtl, tspan, tref ... */

static void SVG_Render_text(GF_Node *node, void *rs)
{
	SVGPropertiesPointers backup_props;
	GF_Matrix2D backup_matrix;
	DrawableContext *ctx;
	SVG_TextStack *st = (SVG_TextStack *)gf_node_get_private(node);
	Drawable *cs = st->draw;
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVGtextElement *text = (SVGtextElement *)node;
	GF_FontRaster *ft_dr = eff->surface->render->compositor->font_engine;
  
	if (!ft_dr) return;

	SVG_Render_base(node, eff, &backup_props);

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		return;
	}
	gf_mx2d_copy(backup_matrix, eff->transform);

	if (((SVGTransformableElement *)node)->motionTransform) 
		gf_mx2d_pre_multiply(&eff->transform, ((SVGTransformableElement *)node)->motionTransform);
	gf_mx2d_pre_multiply(&eff->transform, &((SVGTransformableElement *)node)->transform);

	if ( (st->prev_size != eff->svg_props->font_size->value) || 
		 (st->prev_flags != *eff->svg_props->font_style) || 
		 (st->prev_anchor != *eff->svg_props->text_anchor) ||
		 (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) 
	) {
		unsigned short *wcText;
		Fixed ascent, descent, font_height;
		GF_Rect rc;
		u32 len;
		Fixed lw, lh, start_x, start_y;
		unsigned short wcTemp[5000];
		char styles[1000];
		char *str = text->textContent;
		/* todo : place each character one by one */
		SVG_Coordinate *xc = (SVG_Coordinate *) gf_list_get(text->x, 0);
		SVG_Coordinate *yc = (SVG_Coordinate *) gf_list_get(text->y, 0);
		Fixed x, y;
		if (xc) x = xc->value; else x = 0;
		if (yc) y = yc->value; else y = 0;

//		fprintf(stdout, "Rebuilding text\n");
		drawable_reset_path(cs);
		if (str) {
			len = gf_utf8_mbstowcs(wcTemp, 5000, (const char **) &str);
			if (len == (u32) -1) return;

			wcText = (u16*)malloc(sizeof(unsigned short) * (len+1));
			memcpy(wcText, wcTemp, sizeof(unsigned short) * (len+1));
			wcText[len] = 0;

			switch(*eff->svg_props->font_style) {
			case SVG_FONTSTYLE_NORMAL:
				sprintf(styles, "%s", "PLAIN");
				break;
			case SVG_FONTSTYLE_ITALIC:
				sprintf(styles, "%s", "ITALIC");
				break;
			case SVG_FONTSTYLE_OBLIQUE:
				sprintf(styles, "%s", "ITALIC");
				break;
			}

			if (ft_dr->set_font(ft_dr, eff->svg_props->font_family->value, styles) != GF_OK) {
				if (ft_dr->set_font(ft_dr, NULL, styles) != GF_OK) {
					free(wcText);
					return;
				}
			}
			ft_dr->set_font_size(ft_dr, eff->svg_props->font_size->value);
			ft_dr->get_font_metrics(ft_dr, &ascent, &descent, &font_height);


			ft_dr->get_text_size(ft_dr, wcText, &lw, &lh);

			if (eff->svg_props->text_anchor) {
				switch(*eff->svg_props->text_anchor) {
				case SVG_TEXTANCHOR_MIDDLE:
					start_x = -lw/2;
					break;
				case SVG_TEXTANCHOR_END:
					start_x = -lw;
					break;
				case SVG_TEXTANCHOR_START:
				default:
					start_x = 0;
				}
			} else {
				start_x = 0;
			}
			start_y = 0;

			ft_dr->add_text_to_path(ft_dr, cs->path, 0, wcText, start_x + x, start_y + y, FIX_ONE, FIX_ONE, ascent, &rc);
			free(wcText);
		}
		gf_node_dirty_clear(node, 0);
		cs->node_changed = 1;
		st->prev_size = eff->svg_props->font_size->value;
		st->prev_flags = *eff->svg_props->font_style;
		st->prev_anchor = *eff->svg_props->text_anchor;
	}
	ctx = SVG_drawable_init_context(cs, eff);
	if (ctx) {
		drawctx_store_original_bounds(ctx);
		drawable_finalize_render(ctx, eff);
	}
	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
}

Bool SVG_text_PointOver(DrawableContext *ctx, Fixed x, Fixed y, u32 check_type)
{
	/*this is not documented anywhere but it speeds things up*/
	if (!check_type || ctx->aspect.filled) return 1;
	/*FIXME*/
	return 1;
}


void SVG_DestroyText(GF_Node *node)
{
	SVG_TextStack *stack = (SVG_TextStack *) gf_node_get_private(node);
	drawable_del(stack->draw);
	free(stack);
}

void SVG_Init_text(Render2D *sr, GF_Node *node)
{
	SVG_TextStack *stack;
	GF_SAFEALLOC(stack, SVG_TextStack);
	stack->draw = drawable_new();
	stack->draw->IsPointOver = SVG_text_PointOver;
	gf_sr_traversable_setup(stack->draw, node, sr->compositor);
	gf_node_set_private(node, stack);
	gf_node_set_predestroy_function(node, SVG_DestroyText);
	gf_node_set_render_function(node, SVG_Render_text);
}


#endif
