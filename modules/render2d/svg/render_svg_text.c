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
#include "../visualsurface2d.h"

/* TODO: implement all the missing features: horizontal/vertical, ltr/rtl, tspan, tref ... */

static void SVG_Render_text(GF_Node *node, void *rs)
{
	GF_Matrix2D backup_matrix;
	SVG_Transform *tr;
	DrawableContext *ctx;
	Drawable *cs = gf_node_get_private(node);
	RenderEffect2D *eff = rs;
	SVGtextElement *text = (SVGtextElement *)node;
	GF_FontRaster *ft_dr = eff->surface->render->compositor->font_engine;
  
	SVGStylingProperties backup_props;
	u32 styling_size = sizeof(SVGStylingProperties);

	memcpy(&backup_props, eff->svg_props, styling_size);
	SVGApplyProperties(eff->svg_props, text->properties);

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, styling_size);
		return;
	}

	gf_mx2d_copy(backup_matrix, eff->transform);

	tr = gf_list_get(text->transform, 0);
	if (tr) {
		gf_mx2d_copy(eff->transform, tr->matrix);
		gf_mx2d_add_matrix(&eff->transform, &backup_matrix);
	}

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		Fixed lw, lh, start_x, start_y;
		unsigned short wcTemp[5000];
		char styles[1000];
		char *str = text->textContent;
		/* todo : place each character one by one */
		Fixed x = (gf_list_count(text->x) ? *(Fixed *)gf_list_get(text->x, 0) : 0);
		Fixed y = (gf_list_count(text->y) ? *(Fixed *)gf_list_get(text->y, 0) : 0);
		unsigned short *wcText;
		Fixed ascent, descent, font_height;
		GF_Rect rc;
		u32 len;

//		fprintf(stdout, "Rebuilding text\n");
		drawable_reset_path(cs);
		if (str) {
			len = gf_utf8_mbstowcs(wcTemp, 5000, (const char **) &str);
			if (len == (u32) -1) return;

			wcText = malloc(sizeof(unsigned short) * (len+1));
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
	}
	ctx = SVG_drawable_init_context(cs, eff);
	if (ctx) {
		drawctx_store_original_bounds(ctx);
		drawable_finalize_render(ctx, eff);
	}
	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, styling_size);
}

void SVG_Init_text(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_render_function(node, SVG_Render_text);
}


#endif
