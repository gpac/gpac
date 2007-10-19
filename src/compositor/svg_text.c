/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Compositor sub-project
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
#include "visual_manager.h"
#include "nodes_stacks.h"

typedef struct 
{
	Drawable *draw;
	Fixed prev_size;
	u32 prev_flags;
	u32 prev_anchor;
} SVG_TextStack;

static void svg_finalize_sort(DrawableContext *ctx, GF_TraverseState * tr_state)
{
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		if (!ctx->drawable->mesh) {
			ctx->drawable->mesh = new_mesh();
			mesh_from_path(ctx->drawable->mesh, ctx->drawable->path);
		}
		visual_3d_draw_from_context(ctx, tr_state);
		ctx->drawable = NULL;
	} else 
#endif
	{
		drawable_finalize_sort(ctx, tr_state, NULL);
	}
}

/*@styles indicates font styles (PLAIN, BOLD, ITALIC, BOLDITALIC and UNDERLINED, STRIKEOUT)*/
static void set_font_style(GF_TraverseState * tr_state, char *styles)
{
	switch(*tr_state->svg_props->font_style) {
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

	switch(*tr_state->svg_props->font_weight) {
	case SVG_FONTWEIGHT_100:
	case SVG_FONTWEIGHT_LIGHTER:
	case SVG_FONTWEIGHT_200:
	case SVG_FONTWEIGHT_300:
	case SVG_FONTWEIGHT_400:
		break;

	case SVG_FONTWEIGHT_NORMAL:
	case SVG_FONTWEIGHT_500:
		break;
	
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
	switch(*tr_state->svg_props->text_decoration) {
	}
*/
}

static GF_Err svg_set_font(GF_TraverseState * tr_state, GF_FontRaster * ft_dr, char * styles){
	char *a_font;
	Bool font_set;
	font_set = 0;

	set_font_style(tr_state, styles);

	a_font = tr_state->svg_props->font_family->value;
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

static void apply_text_anchor(GF_TraverseState * tr_state, Fixed * lw){
	if (tr_state->svg_props->text_anchor) {
		switch(*tr_state->svg_props->text_anchor) {
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

	if (*len+1 == 10)
		*len = 9;
	wcText = (u16*)malloc(sizeof(u16) * (*len+1));
	memcpy(wcText, wcTemp, sizeof(u16) * (*len+1));
	wcText[*len] = 0;

	return wcText;
}

void svg_traverse_domtext(GF_Node *node, SVGAllAttributes atts, GF_TraverseState *tr_state, GF_FontRaster *ft_dr, GF_Path *path);

void get_domtext_width(GF_Node *node, GF_TraverseState *tr_state);
void get_tspan_width(GF_Node *node, void *rs);

void textArea_DOMText_traverse(GF_Node *node, GF_TraverseState *tr_state, GF_Path *path);
void textArea_tspan_traverse(GF_Node *node, GF_TraverseState *tr_state, Bool is_destroy);

/* TODO: implement all the missing features: horizontal/vertical, ltr/rtl, tspan, tref ... */
static void svg_traverse_text(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_Matrix2D backup_matrix;
	GF_Matrix mx3d;
	DrawableContext *ctx;
	SVG_TextStack *st = (SVG_TextStack *)gf_node_get_private(node);
	Drawable *cs = st->draw;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
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

	assert(tr_state->traversing_mode!=TRAVERSE_DRAW_2D);

	if (tr_state->traversing_mode==TRAVERSE_PICK) {
		svg_drawable_pick(node, cs, tr_state);
		return;
	}

	ft_dr = tr_state->visual->compositor->font_engine;
	if (!ft_dr) return;

	gf_svg_flatten_attributes(text, &atts);
	compositor_svg_traverse_base(node, &atts, tr_state, &backup_props, &backup_flags);

	if (compositor_svg_is_display_off(tr_state->svg_props) ||
		*(tr_state->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		tr_state->svg_flags = backup_flags;
		return;
	}
	
	compositor_svg_apply_local_transformation(tr_state, &atts, &backup_matrix, &mx3d);

	if ( (st->prev_size != tr_state->svg_props->font_size->value) || 
		 (st->prev_flags != *tr_state->svg_props->font_style) || 
		 (st->prev_anchor != *tr_state->svg_props->text_anchor) ||
		 (gf_node_dirty_get(node) & (GF_SG_SVG_GEOMETRY_DIRTY | GF_SG_CHILD_DIRTY) ) 
	) {
		GF_ChildNodeItem *child = ((GF_ParentNode *) text)->children;

		tr_state->text_end_x = 0;
		tr_state->text_end_y = 0;

		//initialisation des positions de caractères 
		if (atts.text_x) tr_state->count_x = gf_list_count(*atts.text_x);
		else tr_state->count_x=0;
		if (atts.text_y) tr_state->count_y = gf_list_count(*atts.text_y);
		else tr_state->count_y=0;

		//initialisation des décalages des éléments de texte
		tr_state->x_anchors = gf_list_new();

		//calcul des longueurs des fragments de texte définis par text_x et text_y
		while (child) {
			switch  (gf_node_get_tag(child->node)) {
			case TAG_DOMText:
				get_domtext_width(child->node, tr_state); break;
			case TAG_SVG_tspan:
				get_tspan_width(child->node, tr_state); break;
			default:
				break;
			}
			child=child->next;
		}

		//calcul des décalages pour chacun des éléments (anchor)
		imax=gf_list_count(tr_state->x_anchors);
		for (i=0;i<imax;i++){
			lw=gf_list_get(tr_state->x_anchors, i);
			apply_text_anchor(tr_state, lw);
		}

		//réinitialisation des compteurs pour le rendu
		if (atts.text_x) tr_state->count_x = gf_list_count(*atts.text_x);
		else tr_state->count_x=0;
		if (atts.text_y) tr_state->count_y = gf_list_count(*atts.text_y);
		else tr_state->count_y=0;
		
		tr_state->chunk_index = 0;

		//initialisation de la position courante du texte
		if (!tr_state->text_end_x){
			SVG_Coordinate *xc = (atts.text_x ? (SVG_Coordinate *) gf_list_get(*atts.text_x, 0) : NULL);
			tr_state->text_end_x = (xc ? xc->value : 0);
		}
		if (!tr_state->text_end_y){
			SVG_Coordinate *yc = (atts.text_y ? (SVG_Coordinate *) gf_list_get(*atts.text_y, 0) : NULL);
			tr_state->text_end_y = (yc ? yc->value : 0);
		}

		//copie de text_x et text_y pour transmettre aux sous-éléments (tspan ...)
		tr_state->text_x = atts.text_x;
		tr_state->text_y = atts.text_y;
		
		drawable_reset_path(cs);
		child = ((GF_ParentNode *) text)->children;
		while (child) {
			switch  (gf_node_get_tag(child->node)) {
			case TAG_DOMText:
				svg_traverse_domtext(child->node, atts, tr_state, ft_dr, cs->path); break;
			case TAG_SVG_tspan:
				gf_node_traverse(child->node, tr_state); break;
			default:
				break;
			}
			child = child->next;
		}
		gf_node_dirty_clear(node, 0);
		drawable_mark_modified(cs, tr_state);
		st->prev_size = tr_state->svg_props->font_size->value;
		st->prev_flags = *tr_state->svg_props->font_style;
		st->prev_anchor = *tr_state->svg_props->text_anchor;
	} else {
		if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
			if (!compositor_svg_is_display_off(tr_state->svg_props))
				gf_path_get_bounds(cs->path, &tr_state->bounds);
			goto end;
		} else {
			GF_ChildNodeItem *child = ((GF_ParentNode *) text)->children;
			while (child) {
				switch  (gf_node_get_tag(child->node)) {
				case TAG_DOMText:
					break;
				case TAG_SVG_tspan:
					gf_node_traverse(child->node, tr_state); break;
				default:
					break;
				}
				child = child->next;
			}
		}
	}
	ctx = drawable_init_context_svg(cs, tr_state);
	if (ctx) svg_finalize_sort(ctx, tr_state);


end:
	compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx3d);
	while (gf_list_count(tr_state->x_anchors)) {
		Fixed *f = gf_list_last(tr_state->x_anchors);
		gf_list_rem_last(tr_state->x_anchors);
		free(f);
	}
	gf_list_del(tr_state->x_anchors);
	tr_state->x_anchors = NULL;
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}

static void svg_traverse_tspan(GF_Node *node, void *rs, Bool is_destroy)
{	
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_Matrix2D backup_matrix;
	GF_Matrix mx3d;
	DrawableContext *ctx;
	SVG_TextStack *st = (SVG_TextStack *)gf_node_get_private(node);
	Drawable *cs = st->draw;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	SVG_Element *tspan = (SVG_Element *)node;
	GF_FontRaster *ft_dr;
	SVGAllAttributes atts;

	if (is_destroy) {
		drawable_del(st->draw);
		free(st);
		return;
	}

	assert(tr_state->traversing_mode!=TRAVERSE_DRAW_2D);

	if (tr_state->traversing_mode==TRAVERSE_PICK) {
		/*!! FIXME heritance needed in 3D mode !!*/
		svg_drawable_pick(node, cs, tr_state);
		return;
	}

	ft_dr = tr_state->visual->compositor->font_engine;
	if (!ft_dr) return;

	gf_svg_flatten_attributes(tspan, &atts);
	compositor_svg_traverse_base(node, &atts, tr_state, &backup_props, &backup_flags);

	if (compositor_svg_is_display_off(tr_state->svg_props) ||
		*(tr_state->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		tr_state->svg_flags = backup_flags;
		return;
	}
	
	compositor_svg_apply_local_transformation(tr_state, &atts, &backup_matrix, &mx3d);

	if ( (st->prev_size != tr_state->svg_props->font_size->value) || 
		 (st->prev_flags != *tr_state->svg_props->font_style) || 
		 (st->prev_anchor != *tr_state->svg_props->text_anchor) ||
		 (gf_node_dirty_get(node) & (GF_SG_SVG_GEOMETRY_DIRTY | GF_SG_CHILD_DIRTY) ) 
	) {
		GF_ChildNodeItem *child = ((GF_ParentNode *) tspan)->children;

		while (child) {
			switch  (gf_node_get_tag(child->node)) {
			case TAG_DOMText:
				svg_traverse_domtext(child->node, atts, tr_state, ft_dr, cs->path); break;
			case TAG_SVG_tspan:
				gf_node_traverse(child->node, tr_state); break;
			default:
				break;
			}
			child = child->next;
		}
		gf_node_dirty_clear(node, 0);
		drawable_mark_modified(cs, tr_state);
		st->prev_size = tr_state->svg_props->font_size->value;
		st->prev_flags = *tr_state->svg_props->font_style;
		st->prev_anchor = *tr_state->svg_props->text_anchor;
	} else {
		if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
			if (!compositor_svg_is_display_off(tr_state->svg_props))
				gf_path_get_bounds(cs->path, &tr_state->bounds);
			goto end;
		} else {
			GF_ChildNodeItem *child = ((GF_ParentNode *) tspan)->children;
			while (child) {
				switch  (gf_node_get_tag(child->node)) {
				case TAG_DOMText:
					break;
				case TAG_SVG_tspan:
					gf_node_traverse(child->node, tr_state); break;
				default:
					break;
				}
				child = child->next;
			}
		}
	}

	ctx = drawable_init_context_svg(cs, tr_state);
	if (ctx) svg_finalize_sort(ctx, tr_state);

end:
	compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx3d);
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
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

void svg_traverse_domtext(GF_Node *node, SVGAllAttributes atts, GF_TraverseState *tr_state, GF_FontRaster *ft_dr, GF_Path *path){
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

		if (svg_set_font(tr_state, ft_dr, styles) != GF_OK) return;
		ft_dr->set_font_size(ft_dr, tr_state->svg_props->font_size->value);
		ft_dr->get_font_metrics(ft_dr, &ascent, &descent, &font_height);

		temp_txt = dup_text = apply_space_preserve(0, dom_text->textContent);

		/*
		si la position des caractères est donnée dans les attributs (x, y), on prendra celle là. 
		Sinon on se place à la suite du texte: tr_state->text_end_x.
		*/		
		while ((temp_txt[0] != 0)&&(((tr_state->count_x)>1)||((tr_state->count_y>1)))){
			u16 *wcText;

			//ici on teste la position à prendre en compte (attribut ou défaut ou position en cours)
			if (tr_state->count_x==0) {
				x = tr_state->text_end_x;
			} else {
				SVG_Coordinate *xc = (SVG_Coordinate *) gf_list_get(*tr_state->text_x, tr_state->chunk_index);
				x = xc->value;
				(tr_state->count_x)--;
			}
			if (tr_state->count_y==0) {
				y = tr_state->text_end_y;
			} else {
				SVG_Coordinate *yc = (SVG_Coordinate *) gf_list_get(*tr_state->text_y, tr_state->chunk_index);
				y = yc->value;
				(tr_state->count_y)--;
			}
			
			//ici on passe tous les caractères uns à uns
			temp_char[0] = temp_txt[0];
			temp_char[1] = 0;
			//encodage
			wcText=char2unicode(temp_char,&len);
			if (len == (u32) -1) {
				free(dup_text);
				return;
			}

			//récupération des dimensions
			ft_dr->get_text_size(ft_dr, wcText, &lw, &lh);

			//récupération des décalages
			x_anchor = *(Fixed *)gf_list_get(tr_state->x_anchors, tr_state->chunk_index);
			//mise à jour du path
			ft_dr->add_text_to_path(ft_dr, path, 0, wcText, x_anchor + x, y, FIX_ONE, FIX_ONE, ascent, &rc);
			free(wcText);

			//mise à jour de la position du dernier caractère et des positions en cours dans les listes
			tr_state->text_end_x=x+lw;
			tr_state->text_end_y=y;
			(tr_state->chunk_index)++;
			temp_txt++;
		}

		/* quand les positions données par les attributs sont épuisées (ou s'il s'agit de la dernière) 
		en x comme en y, s'il reste du texte à écrire, on le place à la suite (ou à la dernière) */
		if (temp_txt) {
			u16 *wcText;
			//si on va épuiser les positions, on prend la dernière valeur
			if ((tr_state->count_x==1) && atts.text_x) {
				SVG_Coordinate *xc = (SVG_Coordinate *) gf_list_get(*atts.text_x, tr_state->chunk_index);
				tr_state->text_end_x = xc->value;
				(tr_state->count_x)--;
			}
			if ((tr_state->count_y==1) && atts.text_y) {
				SVG_Coordinate *yc = (SVG_Coordinate *) gf_list_get(*atts.text_y, tr_state->chunk_index);
				tr_state->text_end_y = yc->value;
				(tr_state->count_y)--;
			}
			//sinon, on va à la suite du texte
			x = tr_state->text_end_x;
			y = tr_state->text_end_y;

			//conversion du texte
			wcText=char2unicode(temp_txt,&len);
			if (len == (u32) -1) {
				free(dup_text);
				return;
			}

			//récupération des dimensions
			ft_dr->get_text_size(ft_dr, wcText, &lw, &lh);

			//récupération des décalages
			x_anchor = *(Fixed *)gf_list_get(tr_state->x_anchors, tr_state->chunk_index);

			//ajout du texte au path
			ft_dr->add_text_to_path(ft_dr, path, 0, wcText, x_anchor + x, y, FIX_ONE, FIX_ONE, ascent, &rc);
			//vidange
			free(wcText);
			//mise à jour de la fin de texte
			tr_state->text_end_x+=lw;
		}

		free(dup_text);
	}
}


void compositor_init_svg_text(GF_Compositor *compositor, GF_Node *node)
{
	SVG_TextStack *stack;
	GF_SAFEALLOC(stack, SVG_TextStack);
	stack->draw = drawable_new();
	stack->draw->node = node;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_traverse_text);
}

void compositor_init_svg_tspan(GF_Compositor *compositor, GF_Node *node)
{
	SVG_TextStack *stack;
	GF_SAFEALLOC(stack, SVG_TextStack);
	stack->draw = drawable_new();
	stack->draw->node = node;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_traverse_tspan);
}


void get_domtext_width(GF_Node *node, GF_TraverseState *tr_state){
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

		ft_dr = tr_state->visual->compositor->font_engine;
		if (!ft_dr) return;

		if (svg_set_font(tr_state, ft_dr, styles) != GF_OK) return;
		ft_dr->set_font_size(ft_dr, tr_state->svg_props->font_size->value);

		temp_txt = dup_text = apply_space_preserve(0, dom_text->textContent);

		//count_x donne le nombre de positions de caractères à venir données par les attributs
		//ainsi tant que ce n'est pas la dernière attribution de position, on prendra les lettres une par une
		while ((temp_txt[0] != 0)&&(((tr_state->count_x)>1)||((tr_state->count_y>1)))){
			u16 *wcText;

			//ici on passe un caractère à la fois
			temp_char[0] = temp_txt[0];
			temp_char[1] = 0;
			//encodage
			wcText=char2unicode(temp_char,&len);
			if (len == (u32) -1) {
				free(dup_text);
				return;
			}
			//récupération des dimensions
			ft_dr->get_text_size(ft_dr, wcText, &lw, &lh);
			//vidange mémoire
			free(wcText);

			//ici on remplit la case de tr_state->x_anchors correspondant à la lettre
			entry = (Fixed*)malloc(sizeof(Fixed));
			*entry = lw;
			gf_list_add(tr_state->x_anchors, entry);

			//tant qu'il reste des positions, on devra passer à la suivante
			if (tr_state->count_x>0) {
				(tr_state->count_x)--;
			}
			if (tr_state->count_y>0) {
				(tr_state->count_y)--;
			}
			
			//passage à suite
			temp_txt++;
		}

		//quand il ne reste plus qu'une redéfinition de position, s'il reste du texte à écrire, on le traite en un bloc
		if (temp_txt) {
			u16 *wcText;
			wcText=char2unicode(temp_txt,&len);
			if (len == (u32) -1) {
				free(dup_text);
				return;
			}
			ft_dr->get_text_size(ft_dr, wcText, &lw, &lh);
			free(wcText);
			//ici on remplit la case suivante de tr_state->x_anchors
			//si on vient de donner la dernière position, on crée un nouvel élément
			if ((tr_state->count_x==1)||(tr_state->count_y==1)){
				entry = (Fixed*)malloc(sizeof(Fixed));
				*entry = lw;
				gf_list_add(tr_state->x_anchors, entry);
			} else { // (count_x == 0 && count_y == 0) sinon on incrémente le dernier
				u32 i=gf_list_count(tr_state->x_anchors);
				if (i>0) {
					Fixed * prec_lw=gf_list_get(tr_state->x_anchors, i-1);
					(*prec_lw) += lw;
				}
				else {
					entry = (Fixed*)malloc(sizeof(Fixed));
					*entry = lw;
					gf_list_add(tr_state->x_anchors, entry);
				}
			}
			//on passe les compteurs à zéro si besoin, pour se souvenir qu'il faudra par la suite
			//incrémenter le dernier élément
			if (tr_state->count_x==1) {
				(tr_state->count_x)--;
			}
			if (tr_state->count_y==1) {
				(tr_state->count_y)--;
			}

		}

		free(dup_text);
	}
}

void get_tspan_width(GF_Node *node, void *rs){
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	SVG_TextStack *st = (SVG_TextStack *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	SVG_Element *tspan = (SVG_Element *)node;
	SVGAllAttributes atts;
	GF_ChildNodeItem *child;

	gf_svg_flatten_attributes(tspan, &atts);
	compositor_svg_traverse_base(node, &atts, tr_state, &backup_props, &backup_flags);

	child = ((GF_ParentNode *) tspan)->children;
	while (child) {
		switch  (gf_node_get_tag(child->node)) {
			case TAG_DOMText:
				get_domtext_width(child->node, tr_state); break;
			case TAG_SVG_tspan:
				get_tspan_width(child->node, tr_state); break;
			default:
				break;
		}
		child=child->next;
	}

	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}

static void svg_traverse_textArea(GF_Node *node, void *rs, Bool is_destroy){	
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_Matrix mx3d;
	GF_Matrix2D backup_matrix;
	DrawableContext *ctx;
	SVG_TextStack *st = (SVG_TextStack *)gf_node_get_private(node);
	Drawable *cs = st->draw;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	SVG_Element *text = (SVG_Element *)node;
	GF_FontRaster *ft_dr;
	SVGAllAttributes atts;

	if (is_destroy) {
		drawable_del(st->draw);
		free(st);
		return;
	}

	assert(tr_state->traversing_mode!=TRAVERSE_DRAW_2D);
	
	if (tr_state->traversing_mode==TRAVERSE_PICK) {
		/*!! FIXME heritance needed in 3D mode !!*/
		svg_drawable_pick(node, cs, tr_state);
		return;
	}

	ft_dr = tr_state->visual->compositor->font_engine;
	if (!ft_dr) return;

	gf_svg_flatten_attributes(text, &atts);
	compositor_svg_traverse_base(node, &atts, tr_state, &backup_props, &backup_flags);

	if (compositor_svg_is_display_off(tr_state->svg_props) ||
		*(tr_state->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		tr_state->svg_flags = backup_flags;
		return;
	}
	
	compositor_svg_apply_local_transformation(tr_state, &atts, &backup_matrix, &mx3d);

	if ( (st->prev_size != tr_state->svg_props->font_size->value) || 
		 (st->prev_flags != *tr_state->svg_props->font_style) || 
		 (st->prev_anchor != *tr_state->svg_props->text_anchor) ||
		 (gf_node_dirty_get(node) & (GF_SG_SVG_GEOMETRY_DIRTY | GF_SG_CHILD_DIRTY) ) 
	) {
		GF_ChildNodeItem *child = ((GF_ParentNode *) text)->children;
		
		drawable_reset_path(cs);
		tr_state->max_length = (atts.width ? (atts.width->type == SVG_NUMBER_AUTO ? FIX_MAX : atts.width->value) : FIX_MAX);
		tr_state->max_height = (atts.height ? (atts.height->type == SVG_NUMBER_AUTO ? FIX_MAX : atts.height->value) : FIX_MAX);
		tr_state->base_x = (atts.x ? atts.x->value : 0);
		tr_state->base_y = (atts.y ? atts.y->value : 0);
		tr_state->y_step = tr_state->svg_props->font_size->value;
		tr_state->text_end_x = 0;
		tr_state->text_end_y = (tr_state->svg_props->line_increment->type == SVG_NUMBER_AUTO ? tr_state->y_step*FLT2FIX(1.1) : tr_state->svg_props->line_increment->value);
		child = ((GF_ParentNode *) text)->children;
		while (child) {
			switch  (gf_node_get_tag(child->node)) {
			case TAG_DOMText:
				textArea_DOMText_traverse(child->node, tr_state, cs->path); break;
			case TAG_SVG_tspan:
				textArea_tspan_traverse(child->node, tr_state, is_destroy); break;
			case TAG_SVG_tbreak:
				tr_state->text_end_y += (tr_state->svg_props->line_increment->type == SVG_NUMBER_AUTO ? tr_state->y_step*FLT2FIX(1.1) : tr_state->svg_props->line_increment->value);
				tr_state->text_end_x = 0;
				break;
			default:
				break;
			}
			child=child->next;
		}

		//effacement mémoire, mise à jour des données-test de changement
		gf_node_dirty_clear(node, 0);
		drawable_mark_modified(cs, tr_state);
		st->prev_size = tr_state->svg_props->font_size->value;
		st->prev_flags = *tr_state->svg_props->font_style;
		st->prev_anchor = *tr_state->svg_props->text_anchor;
	} else {		
		if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
			GF_ChildNodeItem *child = ((GF_ParentNode *) text)->children;
			if (!compositor_svg_is_display_off(tr_state->svg_props))
				gf_path_get_bounds(cs->path, &tr_state->bounds);
			goto end;

		} else {
			GF_ChildNodeItem *child = ((GF_ParentNode *) text)->children;
			while (child) {
				switch  (gf_node_get_tag(child->node)) {
				case TAG_DOMText:
					break;
				case TAG_SVG_tspan:
					textArea_tspan_traverse(child->node, tr_state, is_destroy); break;
				default:
					break;
				}
				child = child->next;
			}
		}
	}
	ctx = drawable_init_context_svg(cs, tr_state);
	if (ctx) svg_finalize_sort(ctx, tr_state);


end:
	while (gf_list_count(tr_state->x_anchors)) {
		Fixed *f = gf_list_last(tr_state->x_anchors);
		gf_list_rem_last(tr_state->x_anchors);
		free(f);
	}
	gf_list_del(tr_state->x_anchors);
	tr_state->x_anchors = NULL;
	compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx3d);
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}

void compositor_init_svg_textarea(GF_Compositor *compositor, GF_Node *node)
{
	SVG_TextStack *stack;
	GF_SAFEALLOC(stack, SVG_TextStack);
	stack->draw = drawable_new();
	stack->draw->node = node;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_traverse_textArea);
}

void textArea_DOMText_traverse(GF_Node *node, GF_TraverseState *tr_state, GF_Path *path)
{
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
	u16 *wcText = NULL;
	GF_DOMText *dom_text = (GF_DOMText *)node;

	if (!dom_text->textContent) return;

	ft_dr = tr_state->visual->compositor->font_engine;
	if (!ft_dr) return;

	if (svg_set_font(tr_state, ft_dr, styles) != GF_OK) return;
	ft_dr->set_font_size(ft_dr, tr_state->svg_props->font_size->value);

	word_start = dup_text = apply_space_preserve(0, dom_text->textContent);

	/* boucle principale: mot par mot */
	while (word_start[0] != 0){
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
		if (wcLen == (u32) -1) goto exit;

		word_start[len] = tmp;

		//récupération des dimensions
		ft_dr->get_font_metrics(ft_dr, &ascent, &descent, &font_height);
		ft_dr->get_text_size(ft_dr, wcText, &lw, &lh);

		if ((tr_state->text_end_x + lw)>tr_state->max_length) {
			/* si le mot commence par un espace on recalcule sa longueur sans l'espace */
			if (word_start[0] == ' ') {
				word_start++;
				tmp = word_start[len-1];
				word_start[len-1] = 0;
				free(wcText);
				wcText=char2unicode(word_start,&wcLen);
				word_start[len-1] = tmp;
				if (wcLen == (u32) -1) goto exit;
				
				//récupération des dimensions
				ft_dr->get_font_metrics(ft_dr, &ascent, &descent, &font_height);
				ft_dr->get_text_size(ft_dr, wcText, &lw, &lh);
			}

				/* cas particulier:
				   le premier mot de la ligne ne tient pas sur la ligne */
			if (lw > tr_state->max_length) goto exit;

			tr_state->text_end_y += (tr_state->svg_props->line_increment->type == SVG_NUMBER_AUTO ? tr_state->y_step*FLT2FIX(1.1) : tr_state->svg_props->line_increment->value);			
			/* on ne rend plus les mots qui dépassent du cadre */
			if (tr_state->text_end_y>tr_state->max_height) goto exit;

			/* le retour à la ligne est possible en x et en y*/
			tr_state->text_end_x = 0;
			//tr_state->y_step = tr_state->svg_props->font_size->value;
		} else {
			/* cas où le premier mot ne tient pas en hauteur */
			if (tr_state->text_end_x == 0 && (tr_state->text_end_y+lh > tr_state->max_height))
				goto exit;

			/* on reste sur la même ligne */
		}

		ft_dr->add_text_to_path(ft_dr, path, 0, wcText, tr_state->base_x + tr_state->text_end_x, 
							                            tr_state->base_y + tr_state->text_end_y, 
														FIX_ONE, FIX_ONE, ascent, &rc);
		tr_state->text_end_x+=lw;
		if (tr_state->y_step<lh) tr_state->y_step=lh;

		free(wcText);
		wcText = NULL;

		word_start=word_end;
/*
		while ((word_start[0] != 0)&&((word_start[0]=='\n')||(word_start[0]=='\r')||(word_start[0]=='\t'))){
			word_start++;
			shift=1;
		}
		*/
	}

exit:
	if (wcText) free(wcText);
	free(dup_text);
}

void textArea_tspan_traverse(GF_Node *node, GF_TraverseState *tr_state, Bool is_destroy){
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_Matrix2D backup_matrix;
	GF_Matrix mx3d;
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

	assert(tr_state->traversing_mode!=TRAVERSE_DRAW_2D);

	if (tr_state->traversing_mode==TRAVERSE_PICK) {
		/*!! FIXME heritance needed in 3D mode !!*/
		svg_drawable_pick(node, cs, tr_state);
		return;
	}

	//chargement du font engine (on pourrait s'en passer...)
	ft_dr = tr_state->visual->compositor->font_engine;
	if (!ft_dr) return;

	//extraction des attributs
	gf_svg_flatten_attributes(tspan, &atts);
	compositor_svg_traverse_base(node, &atts, tr_state, &backup_props, &backup_flags);

	//cas où on n'affiche pas
	if (compositor_svg_is_display_off(tr_state->svg_props) ||
		*(tr_state->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		tr_state->svg_flags = backup_flags;
		return;
	}
	
	compositor_svg_apply_local_transformation(tr_state, &atts, &backup_matrix, &mx3d);
	if ( (st->prev_size != tr_state->svg_props->font_size->value) || 
		 (st->prev_flags != *tr_state->svg_props->font_style) || 
		 (st->prev_anchor != *tr_state->svg_props->text_anchor) ||
		 (gf_node_dirty_get(node) & (GF_SG_SVG_GEOMETRY_DIRTY | GF_SG_CHILD_DIRTY) ) 
	) {

		GF_ChildNodeItem *child = ((GF_ParentNode *) tspan)->children;

		//distinction des cas (domtext ou sous-tspan)
		while (child) {
			switch  (gf_node_get_tag(child->node)) {
				case TAG_DOMText:
					textArea_DOMText_traverse(child->node, tr_state, cs->path); break;
				case TAG_SVG_tspan:
					textArea_tspan_traverse(child->node, tr_state, is_destroy); break;
				case TAG_SVG_tbreak:
					tr_state->text_end_y += (tr_state->svg_props->line_increment->type == SVG_NUMBER_AUTO ? tr_state->y_step*FLT2FIX(1.1) : tr_state->svg_props->line_increment->value);
					tr_state->text_end_x = 0;
					break;
				default:
					break;
			}
			child = child->next;
		}
		gf_node_dirty_clear(node, 0);
		drawable_mark_modified(cs, tr_state);
		st->prev_size = tr_state->svg_props->font_size->value;
		st->prev_flags = *tr_state->svg_props->font_style;
		st->prev_anchor = *tr_state->svg_props->text_anchor;
	} else {
		if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
			if (!compositor_svg_is_display_off(tr_state->svg_props))
				gf_path_get_bounds(cs->path, &tr_state->bounds);
			goto end;
		} else {
			GF_ChildNodeItem *child = ((GF_ParentNode *) tspan)->children;
			while (child) {
				switch  (gf_node_get_tag(child->node)) {
					case TAG_DOMText:
						break;
					case TAG_SVG_tspan:
						textArea_tspan_traverse(child->node, tr_state, is_destroy); break;
					default:
						break;
				}
				child = child->next;
			}
		}
	}

	ctx = drawable_init_context_svg(cs, tr_state);
	if (ctx) svg_finalize_sort(ctx, tr_state);

end:
	gf_mx2d_copy(tr_state->transform, backup_matrix);  
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}
#endif
