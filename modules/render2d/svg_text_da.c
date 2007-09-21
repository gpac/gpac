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

/*@styles indicates font styles (PLAIN, BOLD, ITALIC, BOLDITALIC and UNDERLINED, STRIKEOUT)*/
static void set_font_style(RenderEffect2D * eff, char *styles)
{
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

	switch(*eff->svg_props->font_weight) {
	case SVG_FONTWEIGHT_100:
	case SVG_FONTWEIGHT_LIGHTER:
	case SVG_FONTWEIGHT_200:
	case SVG_FONTWEIGHT_300:
		break;

	case SVG_FONTWEIGHT_400:
	case SVG_FONTWEIGHT_NORMAL:
		break;
	
	case SVG_FONTWEIGHT_500:
	case SVG_FONTWEIGHT_600:
	case SVG_FONTWEIGHT_700:
	case SVG_FONTWEIGHT_BOLD:
	case SVG_FONTWEIGHT_800:
	case SVG_FONTWEIGHT_900:
	case SVG_FONTWEIGHT_BOLDER:
		if (!strcmp(styles, "PLAIN")) sprintf(styles, "%s", "BOLD");
		else sprintf(styles, "%s", "BOLDITALIC");
		break;
	}
/*
	switch(*eff->svg_props->text_decoration) {
	}
*/
}

static GF_Err svg_set_font(RenderEffect2D * eff, GF_FontRaster * ft_dr, char * styles){
	char *a_font;
	Bool font_set;
	font_set = 0;

	set_font_style(eff, styles);

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
		return ft_dr->set_font(ft_dr, NULL, styles);
	}
	return GF_OK;
}

static void apply_text_anchor(RenderEffect2D * eff, Fixed * lw){
	if (eff->svg_props->text_anchor) {
		switch(*eff->svg_props->text_anchor) {
		case SVG_TEXTANCHOR_MIDDLE:
			(*lw) = -(*lw)/2;
			break;
		case SVG_TEXTANCHOR_END:
			(*lw) = -(*lw);
			break;
		case SVG_TEXTANCHOR_START:
		default:
			(*lw) = 0;
		}
	} else {
		(*lw) = 0;
	}
}

static u16 *char2unicode(char * str, u32 * len)
{
	u16 wcTemp[5000];
	u16 *wcText;
	*len = gf_utf8_mbstowcs(wcTemp, 5000, (const char **) &str);
	if (*len == (u32) -1) return NULL;

	wcText = (u16*)malloc(sizeof(u16) * (*len+1));
	memcpy(wcText, wcTemp, sizeof(u16) * (*len+1));
	wcText[*len] = 0;

	return wcText;
}

void svg_render_domtext(GF_Node *node, SVGAllAttributes atts, RenderEffect2D *eff, GF_FontRaster *ft_dr, GF_Path *path);

void get_domtext_width(GF_Node *node, RenderEffect2D *eff);
void get_tspan_width(GF_Node *node, void *rs);

void textArea_DOMText_render(GF_Node *node, RenderEffect2D *eff, GF_Path *path);
void textArea_tspan_render(GF_Node *node, RenderEffect2D *eff, Bool is_destroy);

/* TODO: implement all the missing features: horizontal/vertical, ltr/rtl, tspan, tref ... */
static void svg_render_text(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_Matrix2D backup_matrix;
	DrawableContext *ctx;
	SVG_TextStack *st = (SVG_TextStack *)gf_node_get_private(node);
	Drawable *cs = st->draw;
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVG_Element *text = (SVG_Element *)node;
	GF_FontRaster *ft_dr;
	SVGAllAttributes atts;
	u32 i,imax;
	Fixed * lw;

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

	gf_svg_flatten_attributes(text, &atts);
	svg_render_base(node, &atts, eff, &backup_props, &backup_flags);

	if (svg_is_display_off(eff->svg_props) ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		eff->svg_flags = backup_flags;
		return;
	}
	
	svg_apply_local_transformation(eff, &atts, &backup_matrix);

	if ( (st->prev_size != eff->svg_props->font_size->value) || 
		 (st->prev_flags != *eff->svg_props->font_style) || 
		 (st->prev_anchor != *eff->svg_props->text_anchor) ||
		 (gf_node_dirty_get(node) & (GF_SG_SVG_GEOMETRY_DIRTY | GF_SG_CHILD_DIRTY) ) 
	) {
		GF_ChildNodeItem *child = ((GF_ParentNode *) text)->children;

		eff->text_end_x = 0;
		eff->text_end_y = 0;

 		//initialisation des positions de caractères 
		if (atts.text_x) eff->count_x = gf_list_count(*atts.text_x);
		else eff->count_x=0;
		if (atts.text_y) eff->count_y = gf_list_count(*atts.text_y);
		else eff->count_y=0;

		//initialisation des décalages des éléments de texte
		eff->x_anchors = gf_list_new();

		//calcul des longueurs des fragments de texte définis par text_x et text_y
		while (child) {
			switch  (gf_node_get_tag(child->node)) {
			case TAG_DOMText:
				get_domtext_width(child->node, eff); break;
			case TAG_SVG_tspan:
				get_tspan_width(child->node, eff); break;
			default:
				break;
			}
			child=child->next;
		}

		//calcul des décalages pour chacun des éléments (anchor)
		imax=gf_list_count(eff->x_anchors);
		for (i=0;i<imax;i++){
			lw=gf_list_get(eff->x_anchors, i);
			apply_text_anchor(eff, lw);
		}

		//réinitialisation des compteurs pour le rendu
		if (atts.text_x) eff->count_x = gf_list_count(*atts.text_x);
		else eff->count_x=0;
		if (atts.text_y) eff->count_y = gf_list_count(*atts.text_y);
		else eff->count_y=0;
		
		eff->chunk_index = 0;

		//initialisation de la position courante du texte
		if (!eff->text_end_x){
			SVG_Coordinate *xc = (atts.text_x ? (SVG_Coordinate *) gf_list_get(*atts.text_x, 0) : NULL);
			eff->text_end_x = (xc ? xc->value : 0);
		}
		if (!eff->text_end_y){
			SVG_Coordinate *yc = (atts.text_y ? (SVG_Coordinate *) gf_list_get(*atts.text_y, 0) : NULL);
			eff->text_end_y = (yc ? yc->value : 0);
		}

		//copie de text_x et text_y pour transmettre aux sous-éléments (tspan ...)
		eff->text_x = atts.text_x;
		eff->text_y = atts.text_y;
		
		drawable_reset_path(cs);
		child = ((GF_ParentNode *) text)->children;
		while (child) {
			switch  (gf_node_get_tag(child->node)) {
			case TAG_DOMText:
				svg_render_domtext(child->node, atts, eff, ft_dr, cs->path); break;
			case TAG_SVG_tspan:
				gf_node_render(child->node, eff); break;
			default:
				break;
			}
			child = child->next;
		}
		gf_node_dirty_clear(node, 0);
		cs->flags |= DRAWABLE_HAS_CHANGED;
		st->prev_size = eff->svg_props->font_size->value;
		st->prev_flags = *eff->svg_props->font_style;
		st->prev_anchor = *eff->svg_props->text_anchor;
	} else {
		if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
			if (!svg_is_display_off(eff->svg_props))
				gf_path_get_bounds(cs->path, &eff->bounds);
			goto end;
		} else {
			GF_ChildNodeItem *child = ((GF_ParentNode *) text)->children;
			while (child) {
				switch  (gf_node_get_tag(child->node)) {
				case TAG_DOMText:
					break;
				case TAG_SVG_tspan:
					gf_node_render(child->node, eff); break;
				default:
					break;
				}
				child = child->next;
			}
		}
	}
	ctx = SVG_drawable_init_context(cs, eff);
	if (ctx) drawable_finalize_render(ctx, eff, NULL);


end:
	while (gf_list_count(eff->x_anchors)) {
		Fixed *f = gf_list_last(eff->x_anchors);
		gf_list_rem_last(eff->x_anchors);
		free(f);
	}
	gf_list_del(eff->x_anchors);
	eff->x_anchors = NULL;
	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = backup_flags;
}

static void svg_render_tspan(GF_Node *node, void *rs, Bool is_destroy)
{	
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_Matrix2D backup_matrix;
	DrawableContext *ctx;
	SVG_TextStack *st = (SVG_TextStack *)gf_node_get_private(node);
	Drawable *cs = st->draw;
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVG_Element *tspan = (SVG_Element *)node;
	GF_FontRaster *ft_dr;
	SVGAllAttributes atts;

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

	gf_svg_flatten_attributes(tspan, &atts);
	svg_render_base(node, &atts, eff, &backup_props, &backup_flags);

	if (svg_is_display_off(eff->svg_props) ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		eff->svg_flags = backup_flags;
		return;
	}
	
	svg_apply_local_transformation(eff, &atts, &backup_matrix);

	if ( (st->prev_size != eff->svg_props->font_size->value) || 
		 (st->prev_flags != *eff->svg_props->font_style) || 
		 (st->prev_anchor != *eff->svg_props->text_anchor) ||
		 (gf_node_dirty_get(node) & (GF_SG_SVG_GEOMETRY_DIRTY | GF_SG_CHILD_DIRTY) ) 
	) {
		GF_ChildNodeItem *child = ((GF_ParentNode *) tspan)->children;

		while (child) {
			switch  (gf_node_get_tag(child->node)) {
			case TAG_DOMText:
				svg_render_domtext(child->node, atts, eff, ft_dr, cs->path); break;
			case TAG_SVG_tspan:
				gf_node_render(child->node, eff); break;
			default:
				break;
			}
			child = child->next;
		}
		gf_node_dirty_clear(node, 0);
		cs->flags |= DRAWABLE_HAS_CHANGED;
		st->prev_size = eff->svg_props->font_size->value;
		st->prev_flags = *eff->svg_props->font_style;
		st->prev_anchor = *eff->svg_props->text_anchor;
	} else {
		if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
			if (!svg_is_display_off(eff->svg_props))
				gf_path_get_bounds(cs->path, &eff->bounds);
			goto end;
		} else {
			GF_ChildNodeItem *child = ((GF_ParentNode *) tspan)->children;
			while (child) {
				switch  (gf_node_get_tag(child->node)) {
				case TAG_DOMText:
					break;
				case TAG_SVG_tspan:
					gf_node_render(child->node, eff); break;
				default:
					break;
				}
				child = child->next;
			}
		}
	}

	ctx = SVG_drawable_init_context(cs, eff);
	if (ctx) drawable_finalize_render(ctx, eff, NULL);

end:
	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = backup_flags;
}

static char *apply_space_preserve(Bool preserve, char *textContent) 
{
	char *dup_text;
	u32 i, j, len;
	char prev;

	len = strlen(textContent);
	dup_text = malloc(len+1);
	prev = ' ';
	for (i = 0, j = 0; i < len; i++) {
		if (textContent[i] == ' ') {
			if (prev == ' ' && !preserve) { 
				/* ignore space */
			} else {
				dup_text[j] = textContent[i];
				prev = dup_text[j];
				j++;
			}
		} else if ((textContent[i] == '\n') ||
				   (textContent[i] == '\r') ||
				   (textContent[i] == '\t')){
			if (prev == ' ' && !preserve) { 
				/* ignore space */
			} else {
				dup_text[j] = ' ';
				prev = dup_text[j];
				j++;
			}
		} else {
			dup_text[j] = textContent[i];
			prev = dup_text[j];
			j++;
		}
	}
	dup_text[j] = 0;

	return dup_text;
}

void svg_render_domtext(GF_Node *node, SVGAllAttributes atts, RenderEffect2D *eff, GF_FontRaster *ft_dr, GF_Path *path){
	GF_DOMText *dom_text = (GF_DOMText *)node;

	if (dom_text->textContent) {
		Fixed ascent, descent, font_height;
		char styles[1000];
		Fixed x, y;
		u32 len;
		Fixed lw, lh;
		Fixed x_anchor;
		GF_Rect rc;
		char *temp_txt;
		char temp_char[2];
		char *dup_text;

		temp_txt = dup_text = apply_space_preserve(0, dom_text->textContent);
		if (!strlen(temp_txt)) return;

		if (svg_set_font(eff, ft_dr, styles) != GF_OK) return;
		ft_dr->set_font_size(ft_dr, eff->svg_props->font_size->value);
		ft_dr->get_font_metrics(ft_dr, &ascent, &descent, &font_height);

		/*
		si la position des caractères est donnée dans les attributs (x, y), on prendra celle là. 
		Sinon on se place à la suite du texte: eff->text_end_x.
		*/		
		while ((temp_txt[0] != 0)&&(((eff->count_x)>1)||((eff->count_y>1)))){
			u16 *wcText;

			//ici on teste la position à prendre en compte (attribut ou défaut ou position en cours)
			if (eff->count_x==0) {
				x = eff->text_end_x;
			} else {
				SVG_Coordinate *xc = (SVG_Coordinate *) gf_list_get(*eff->text_x, eff->chunk_index);
				x = xc->value;
				(eff->count_x)--;
			}
			if (eff->count_y==0) {
				y = eff->text_end_y;
			} else {
				SVG_Coordinate *yc = (SVG_Coordinate *) gf_list_get(*eff->text_y, eff->chunk_index);
				y = yc->value;
				(eff->count_y)--;
			}
			
			//ici on passe tous les caractères uns à uns
			temp_char[0] = temp_txt[0];
			temp_char[1] = 0;
			//encodage
			wcText=char2unicode(temp_char,&len);
			if (len == (u32) -1) return;

			//récupération des dimensions
			ft_dr->get_text_size(ft_dr, wcText, &lw, &lh);

			//récupération des décalages
			x_anchor = *(Fixed *)gf_list_get(eff->x_anchors, eff->chunk_index);
			//mise à jour du path
			ft_dr->add_text_to_path(ft_dr, path, 0, wcText, x_anchor + x, y, FIX_ONE, FIX_ONE, ascent, &rc);
			free(wcText);

			//mise à jour de la position du dernier caractère et des positions en cours dans les listes
			eff->text_end_x=x+lw;
			eff->text_end_y=y;
			(eff->chunk_index)++;
			temp_txt++;
		}

		/* quand les positions données par les attributs sont épuisées (ou s'il s'agit de la dernière) 
		en x comme en y, s'il reste du texte à écrire, on le place à la suite (ou à la dernière) */
		if (temp_txt) {
			u16 *wcText;
			//si on va épuiser les positions, on prend la dernière valeur
			if (atts.text_x && eff->count_x==1) {
				SVG_Coordinate *xc = (SVG_Coordinate *) gf_list_get(*atts.text_x, eff->chunk_index);
				eff->text_end_x = xc->value;
				(eff->count_x)--;
			}
			if (atts.text_y && eff->count_y==1) {
				SVG_Coordinate *yc = (SVG_Coordinate *) gf_list_get(*atts.text_y, eff->chunk_index);
				eff->text_end_y = yc->value;
				(eff->count_y)--;
			}
			//sinon, on va à la suite du texte
			x = eff->text_end_x;
			y = eff->text_end_y;

			//conversion du texte
			wcText=char2unicode(temp_txt,&len);
			if (len == (u32) -1 || !len) {
				free(wcText);
				return;
			}

			//récupération des dimensions
			ft_dr->get_text_size(ft_dr, wcText, &lw, &lh);

			//récupération des décalages
			x_anchor = *(Fixed *)gf_list_get(eff->x_anchors, eff->chunk_index);

			//ajout du texte au path
			ft_dr->add_text_to_path(ft_dr, path, 0, wcText, x_anchor + x, y, FIX_ONE, FIX_ONE, ascent, &rc);
			//vidange
			free(wcText);
			//mise à jour de la fin de texte
			eff->text_end_x+=lw;
		}

		free(dup_text);
	}
}


void svg_init_text(Render2D *sr, GF_Node *node)
{
	SVG_TextStack *stack;
	GF_SAFEALLOC(stack, SVG_TextStack);
	stack->draw = drawable_new();
	stack->draw->node = node;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_render_text);
}

void svg_init_tspan(Render2D *sr, GF_Node *node)
{
	SVG_TextStack *stack;
	GF_SAFEALLOC(stack, SVG_TextStack);
	stack->draw = drawable_new();
	stack->draw->node = node;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_render_tspan);
}


void get_domtext_width(GF_Node *node, RenderEffect2D *eff){
	GF_DOMText *dom_text = (GF_DOMText *)node;

	if (dom_text->textContent) {
		char styles[1000];
		u32 len;
		Fixed lw, lh;
		Fixed * entry;
		char * temp_txt = dom_text->textContent;
		char temp_char[2];
		GF_FontRaster *ft_dr;
		char *dup_text;

		temp_txt = dup_text = apply_space_preserve(0, dom_text->textContent);
		if (!strlen(temp_txt)) return;

		ft_dr = eff->surface->render->compositor->font_engine;
		if (!ft_dr) return;

		if (svg_set_font(eff, ft_dr, styles) != GF_OK) return;
		ft_dr->set_font_size(ft_dr, eff->svg_props->font_size->value);

		//count_x donne le nombre de positions de caractères à venir données par les attributs
		//ainsi tant que ce n'est pas la dernière attribution de position, on prendra les lettres une par une
		while ((temp_txt[0] != 0)&&(((eff->count_x)>1)||((eff->count_y>1)))){
			u16 *wcText;

			//ici on passe un caractère à la fois
			temp_char[0] = temp_txt[0];
			temp_char[1] = 0;
			//encodage
			wcText=char2unicode(temp_char,&len);
			if (len == (u32) -1) return;
			//récupération des dimensions
			ft_dr->get_text_size(ft_dr, wcText, &lw, &lh);
			//vidange mémoire
			free(wcText);

			//ici on remplit la case de eff->x_anchors correspondant à la lettre
			entry = (Fixed*)malloc(sizeof(Fixed));
			*entry = lw;
			gf_list_add(eff->x_anchors, entry);

			//tant qu'il reste des positions, on devra passer à la suivante
			if (eff->count_x>0) {
				(eff->count_x)--;
			}
			if (eff->count_y>0) {
				(eff->count_y)--;
			}
			
			//passage à suite
			temp_txt++;
		}

		//quand il ne reste plus qu'une redéfinition de position, s'il reste du texte à écrire, on le traite en un bloc
		if (temp_txt) {
			u16 *wcText;
			wcText=char2unicode(temp_txt,&len);
			if (len == (u32) -1 || !len) {
				free(wcText);
				return;
			}
			ft_dr->get_text_size(ft_dr, wcText, &lw, &lh);
			free(wcText);
			//ici on remplit la case suivante de eff->x_anchors
			//si on vient de donner la dernière position, on crée un nouvel élément
			if ((eff->count_x==1)||(eff->count_y==1)){
				entry = (Fixed*)malloc(sizeof(Fixed));
				*entry = lw;
				gf_list_add(eff->x_anchors, entry);
			} else { // (count_x == 0 && count_y == 0) sinon on incrémente le dernier
				u32 i=gf_list_count(eff->x_anchors);
				if (i>0) {
					Fixed * prec_lw=gf_list_get(eff->x_anchors, i-1);
					(*prec_lw) += lw;
				}
				else {
					entry = (Fixed*)malloc(sizeof(Fixed));
					*entry = lw;
					gf_list_add(eff->x_anchors, entry);
				}
			}
			//on passe les compteurs à zéro si besoin, pour se souvenir qu'il faudra par la suite
			//incrémenter le dernier élément
			if (eff->count_x==1) {
				(eff->count_x)--;
			}
			if (eff->count_y==1) {
				(eff->count_y)--;
			}

		}

		free(dup_text);
	}
}

void get_tspan_width(GF_Node *node, void *rs){
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	SVG_TextStack *st = (SVG_TextStack *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVG_Element *tspan = (SVG_Element *)node;
	SVGAllAttributes atts;
	GF_ChildNodeItem *child;

	gf_svg_flatten_attributes(tspan, &atts);
	svg_render_base(node, &atts, eff, &backup_props, &backup_flags);

	child = ((GF_ParentNode *) tspan)->children;
	while (child) {
		switch  (gf_node_get_tag(child->node)) {
			case TAG_DOMText:
				get_domtext_width(child->node, eff); break;
			case TAG_SVG_tspan:
				get_tspan_width(child->node, eff); break;
			default:
				break;
		}
		child=child->next;
	}

	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = backup_flags;
}

static void svg_render_textArea(GF_Node *node, void *rs, Bool is_destroy){	
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_Matrix2D backup_matrix;
	DrawableContext *ctx;
	SVG_TextStack *st = (SVG_TextStack *)gf_node_get_private(node);
	Drawable *cs = st->draw;
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVG_Element *text = (SVG_Element *)node;
	GF_FontRaster *ft_dr;
	SVGAllAttributes atts;

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

	gf_svg_flatten_attributes(text, &atts);
	svg_render_base(node, &atts, eff, &backup_props, &backup_flags);

	if (svg_is_display_off(eff->svg_props) ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		eff->svg_flags = backup_flags;
		return;
	}
	
	svg_apply_local_transformation(eff, &atts, &backup_matrix);

	if ( (st->prev_size != eff->svg_props->font_size->value) || 
		 (st->prev_flags != *eff->svg_props->font_style) || 
		 (st->prev_anchor != *eff->svg_props->text_anchor) ||
		 (gf_node_dirty_get(node) & (GF_SG_SVG_GEOMETRY_DIRTY | GF_SG_CHILD_DIRTY) ) 
	) {
		GF_ChildNodeItem *child = ((GF_ParentNode *) text)->children;
		
		drawable_reset_path(cs);
		eff->max_length = (atts.width ? (atts.width->type == SVG_NUMBER_AUTO ? FLT_MAX : atts.width->value) : FLT_MAX);
		eff->max_height = (atts.height ? (atts.height->type == SVG_NUMBER_AUTO ? FLT_MAX : atts.height->value) : FLT_MAX);
		eff->base_x = (atts.x ? atts.x->value : 0);
		eff->base_y = (atts.y ? atts.y->value : 0);
		eff->y_step = eff->svg_props->font_size->value;
		eff->text_end_x = 0;
		eff->text_end_y = (eff->svg_props->line_increment->type == SVG_NUMBER_AUTO ? eff->y_step*FLT2FIX(1.1) : eff->svg_props->line_increment->value);
		child = ((GF_ParentNode *) text)->children;
		while (child) {
			switch  (gf_node_get_tag(child->node)) {
			case TAG_DOMText:
				textArea_DOMText_render(child->node, eff, cs->path); break;
			case TAG_SVG_tspan:
				textArea_tspan_render(child->node, eff, is_destroy); break;
			case TAG_SVG_tbreak:
				eff->text_end_y += (eff->svg_props->line_increment->type == SVG_NUMBER_AUTO ? eff->y_step*FLT2FIX(1.1) : eff->svg_props->line_increment->value);
				eff->text_end_x = 0;
				break;
			default:
				break;
			}
			child=child->next;
		}

		//effacement mémoire, mise à jour des données-test de changement
		gf_node_dirty_clear(node, 0);
		cs->flags |= DRAWABLE_HAS_CHANGED;
		st->prev_size = eff->svg_props->font_size->value;
		st->prev_flags = *eff->svg_props->font_style;
		st->prev_anchor = *eff->svg_props->text_anchor;
	} else {		
		if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
			GF_ChildNodeItem *child = ((GF_ParentNode *) text)->children;
			if (!svg_is_display_off(eff->svg_props))
				gf_path_get_bounds(cs->path, &eff->bounds);
			goto end;

		} else {
			GF_ChildNodeItem *child = ((GF_ParentNode *) text)->children;
			while (child) {
				switch  (gf_node_get_tag(child->node)) {
				case TAG_DOMText:
					break;
				case TAG_SVG_tspan:
					textArea_tspan_render(child->node, eff, is_destroy); break;
				default:
					break;
				}
				child = child->next;
			}
		}
	}
	ctx = SVG_drawable_init_context(cs, eff);
	if (ctx) drawable_finalize_render(ctx, eff, NULL);


end:
	while (gf_list_count(eff->x_anchors)) {
		Fixed *f = gf_list_last(eff->x_anchors);
		gf_list_rem_last(eff->x_anchors);
		free(f);
	}
	gf_list_del(eff->x_anchors);
	eff->x_anchors = NULL;
	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = backup_flags;
}

void svg_init_textArea(Render2D *sr, GF_Node *node)
{
	SVG_TextStack *stack;
	GF_SAFEALLOC(stack, SVG_TextStack);
	stack->draw = drawable_new();
	stack->draw->node = node;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_render_textArea);
}

void textArea_DOMText_render(GF_Node *node, RenderEffect2D *eff, GF_Path *path){
	GF_DOMText *dom_text = (GF_DOMText *)node;

	if (dom_text->textContent) {
		Fixed ascent, descent, font_height;
		GF_Rect rc;
		char styles[1000];
		u32 len=0;
		Fixed lw, lh;
		char * word_start;
		char * word_end;
		GF_FontRaster *ft_dr;
		Bool shift=0;

		char *dup_text;

		word_start = dup_text = apply_space_preserve(0, dom_text->textContent);

		ft_dr = eff->surface->render->compositor->font_engine;
		if (!ft_dr) return;

		if (svg_set_font(eff, ft_dr, styles) != GF_OK) return;
		ft_dr->set_font_size(ft_dr, eff->svg_props->font_size->value);
		
		/* boucle principale: mot par mot */
		while (word_start[0] != 0){
			u16 *wcText;
			u32 wcLen;
			char tmp;

			word_end = word_start+1;
			len=1;

			//on cherche la fin du mot
			while ((word_end[0] != 0)&&(word_end[0]!=' ')){
				word_end++;
				len++;
			}

			//encodage
			tmp = word_start[len];
			word_start[len] = 0;
			wcText=char2unicode(word_start,&wcLen);
			if (wcLen == (u32) -1) return;
			word_start[len] = tmp;

			//récupération des dimensions
			ft_dr->get_font_metrics(ft_dr, &ascent, &descent, &font_height);
			ft_dr->get_text_size(ft_dr, wcText, &lw, &lh);

			if ((eff->text_end_x + lw)>eff->max_length) {
				/* si le mot commence par un espace on recalcule sa longueur sans l'espace */
				if (word_start[0] == ' ') {
					word_start++;
					tmp = word_start[len-1];
					word_start[len-1] = 0;
					wcText=char2unicode(word_start,&wcLen);
					word_start[len-1] = tmp;
					if (wcLen == (u32) -1) return;
					
					//récupération des dimensions
					ft_dr->get_font_metrics(ft_dr, &ascent, &descent, &font_height);
					ft_dr->get_text_size(ft_dr, wcText, &lw, &lh);
				}

				if (lw > eff->max_length) {
					/* cas particulier:
					   le premier mot de la ligne ne tient pas sur la ligne */
					return;
				} else {
					eff->text_end_y += (eff->svg_props->line_increment->type == SVG_NUMBER_AUTO ? eff->y_step*FLT2FIX(1.1) : eff->svg_props->line_increment->value);			
					if (eff->text_end_y>eff->max_height) {
						/* on ne rend plus les mots qui dépassent du cadre */
						return;
					}
					/* le retour à la ligne est possible en x et en y*/
					eff->text_end_x = 0;
					//eff->y_step = eff->svg_props->font_size->value;
				}
			} else {
				if (eff->text_end_x == 0 && (eff->text_end_y+lh > eff->max_height)) { /* cas où le premier mot ne tient pas en hauteur */
					return;
				}
				/* on reste sur la même ligne */
			}

			ft_dr->add_text_to_path(ft_dr, path, 0, wcText, eff->base_x + eff->text_end_x, 
							                                eff->base_y + eff->text_end_y, 
															FIX_ONE, FIX_ONE, ascent, &rc);
			eff->text_end_x+=lw;
			if (eff->y_step<lh) eff->y_step=lh;

			free(wcText);

			word_start=word_end;
/*
			while ((word_start[0] != 0)&&((word_start[0]=='\n')||(word_start[0]=='\r')||(word_start[0]=='\t'))){
				word_start++;
				shift=1;
			}
			*/
		}

		free(dup_text);
	}
}

void textArea_tspan_render(GF_Node *node, RenderEffect2D *eff, Bool is_destroy){
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_Matrix2D backup_matrix;
	DrawableContext *ctx;
	SVG_TextStack *st = (SVG_TextStack *)gf_node_get_private(node);
	Drawable *cs = st->draw;
	SVG_Element *tspan = (SVG_Element *)node;
	GF_FontRaster *ft_dr;
	SVGAllAttributes atts;

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

	//chargement du font engine (on pourrait s'en passer...)
	ft_dr = eff->surface->render->compositor->font_engine;
	if (!ft_dr) return;

	//extraction des attributs
	gf_svg_flatten_attributes(tspan, &atts);
	svg_render_base(node, &atts, eff, &backup_props, &backup_flags);

	//cas où on n'affiche pas
	if (svg_is_display_off(eff->svg_props) ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		eff->svg_flags = backup_flags;
		return;
	}
	
	svg_apply_local_transformation(eff, &atts, &backup_matrix);
	if ( (st->prev_size != eff->svg_props->font_size->value) || 
		 (st->prev_flags != *eff->svg_props->font_style) || 
		 (st->prev_anchor != *eff->svg_props->text_anchor) ||
		 (gf_node_dirty_get(node) & (GF_SG_SVG_GEOMETRY_DIRTY | GF_SG_CHILD_DIRTY) ) 
	) {

		GF_ChildNodeItem *child = ((GF_ParentNode *) tspan)->children;

		//distinction des cas (domtext ou sous-tspan)
		while (child) {
			switch  (gf_node_get_tag(child->node)) {
				case TAG_DOMText:
					textArea_DOMText_render(child->node, eff, cs->path); break;
				case TAG_SVG_tspan:
					textArea_tspan_render(child->node, eff, is_destroy); break;
				case TAG_SVG_tbreak:
					eff->text_end_y += (eff->svg_props->line_increment->type == SVG_NUMBER_AUTO ? eff->y_step*FLT2FIX(1.1) : eff->svg_props->line_increment->value);
					eff->text_end_x = 0;
					break;
				default:
					break;
			}
			child = child->next;
		}
		gf_node_dirty_clear(node, 0);
		cs->flags |= DRAWABLE_HAS_CHANGED;
		st->prev_size = eff->svg_props->font_size->value;
		st->prev_flags = *eff->svg_props->font_style;
		st->prev_anchor = *eff->svg_props->text_anchor;
	} else {
		if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
			if (!svg_is_display_off(eff->svg_props))
				gf_path_get_bounds(cs->path, &eff->bounds);
			goto end;
		} else {
			GF_ChildNodeItem *child = ((GF_ParentNode *) tspan)->children;
			while (child) {
				switch  (gf_node_get_tag(child->node)) {
					case TAG_DOMText:
						break;
					case TAG_SVG_tspan:
						textArea_tspan_render(child->node, eff, is_destroy); break;
					default:
						break;
				}
				child = child->next;
			}
		}
	}

	ctx = SVG_drawable_init_context(cs, eff);
	if (ctx) drawable_finalize_render(ctx, eff, NULL);

end:
	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = backup_flags;
}
#endif
