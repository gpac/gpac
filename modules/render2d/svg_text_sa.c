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


#ifndef GPAC_DISABLE_SVG

#include <gpac/utf.h>
#include "visualsurface2d.h"
#include "svg_stacks.h"

#ifdef GPAC_ENABLE_SVG_SA


/* TODO: implement all the missing features: horizontal/vertical, ltr/rtl, tspan, tref ... */

static void SVG_Render_text(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_Matrix2D backup_matrix;
	DrawableContext *ctx;
	SVG_TextStack *st = (SVG_TextStack *)gf_node_get_private(node);
	Drawable *cs = st->draw;
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVG_SA_textElement *text = (SVG_SA_textElement *)node;
	GF_FontRaster *ft_dr;
	char *a_font;
	Bool font_set;
  
	if (is_destroy) {
		drawable_del(st->draw);
		free(st);
		return;
	}

	if (eff->traversing_mode==TRAVERSE_DRAW) {
		drawable_draw(eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		eff->is_over = 1;
		//drawable_pick(eff);
		return;
	}

	ft_dr = eff->surface->render->compositor->font_engine;
	if (!ft_dr) return;

	svg_sa_render_base(node, eff, &backup_props, &backup_flags);

	if (svg_is_display_off(eff->svg_props) ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		eff->svg_flags = backup_flags;
		return;
	}

	svg_sa_apply_local_transformation(eff, node, &backup_matrix);

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

			font_set = 0;
			a_font = eff->svg_props->font_family->value;
			while (a_font && !font_set) {
				char *sep;
				while (strchr("\t\r\n ", a_font[0])) a_font++;

				sep = strchr(a_font, ',');
				if (sep) sep[0] = 0;

				if (a_font[0] == '\'') {
					char *sep_end = strchr(a_font+1, '\'');
					if (sep_end) sep_end[0] = 0;
					if (ft_dr->set_font(ft_dr, a_font+1, styles) == GF_OK) 
						font_set = 1;
					if (sep_end) sep_end[0] = '\'';
				} else {
					u32 skip, len = strlen(a_font)-1;
					skip = 0;
					while (a_font[len-skip] == ' ') skip++;
					if (skip) a_font[len-skip+1] = 0;
					if (ft_dr->set_font(ft_dr, a_font, styles) == GF_OK) 
						font_set = 1;
					if (skip) a_font[len-skip+1] = ' ';
				}
				
				if (sep) {
					sep[0] = ',';
					a_font = sep+1;
				} else {
					a_font = NULL;
				}
			}
			if (!font_set) {
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
		cs->flags |= DRAWABLE_HAS_CHANGED;
		st->prev_size = eff->svg_props->font_size->value;
		st->prev_flags = *eff->svg_props->font_style;
		st->prev_anchor = *eff->svg_props->text_anchor;
	}
	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		if (!svg_is_display_off(eff->svg_props)) 
			gf_path_get_bounds(cs->path, &eff->bounds);
		goto end;
	}

	ctx = SVG_drawable_init_context(cs, eff);
	if (ctx) drawable_finalize_render(ctx, eff, NULL);

end:
	svg_sa_restore_parent_transformation(eff, &backup_matrix);
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = backup_flags;
}


void svg_sa_init_text(Render2D *sr, GF_Node *node)
{
	SVG_TextStack *stack;
	GF_SAFEALLOC(stack, SVG_TextStack);
	stack->draw = drawable_new();
	stack->draw->node = node;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, SVG_Render_text);
}

#endif

#endif
