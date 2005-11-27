/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Scene Graph sub-project
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

#include <gpac/nodes_svg.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/scenegraph_dev.h>

void gf_svg_reset_path(SVG_PathData d) 
{
	u32 i;
	for (i = 0; i < gf_list_count(d.commands); i++) {
		u8 *command = gf_list_get(d.commands, i);
		free(command);
	}
	gf_list_del(d.commands);
	for (i = 0; i < gf_list_count(d.points); i++) {
		SVG_Point *pt = gf_list_get(d.points, i);
		free(pt);
	}
	gf_list_del(d.points);
}

void gf_smil_delete_times(GF_List *list)
{
	u32 i;
	for (i = 0; i < gf_list_count(list); i++) {
		SMIL_Time *v=gf_list_get(list, i);
		free(v);
	}
	gf_list_del(list);
}

void gf_svg_delete_points(GF_List *list)
{
	u32 i;
	for (i = 0; i < gf_list_count(list); i++) {
		SVG_Point *p=gf_list_get(list, i);
		free(p);
	}
	gf_list_del(list);
}

void gf_svg_delete_coordinates(GF_List *list)
{
	u32 i;
	for (i = 0; i < gf_list_count(list); i++) {
		SVG_Coordinate *c=gf_list_get(list, i);
		free(c);
	}
	gf_list_del(list);
}

void gf_svg_delete_paint(SVG_Paint *paint) 
{
	if (!paint) return;
	free(paint);
}

void gf_svg_reset_iri(SVG_IRI *iri) 
{
	if (!iri) return;
	if (iri->iri) free(iri->iri);
	//if (iri->target) gf_node_unregister((GF_Node *)iri->target, (GF_Node *)iri->iri_owner);
}

void gf_svg_delete_attribute_value(u32 type, void *value)
{
	switch (type) {
	case SVG_Paint_datatype:
		gf_svg_delete_paint((SVG_Paint *)value);
		break;
	case SVG_IRI_datatype:
		gf_svg_reset_iri((SVG_IRI *)value);
		free(value);
		break;
	case SVG_String_datatype:
		if (*(SVG_String *)value) free(*(SVG_String *)value);
		free(value);
		break;
	case SVG_StrokeWidth_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	case SVG_Visibility_datatype:
	case SVG_Display_datatype:
	default:
		free(value);
	} 
}

void gf_svg_delete_one_anim_value(u8 anim_datatype, void *anim_value)
{
	/* TODO: handle specific animation types : Motion, else ? */
	gf_svg_delete_attribute_value(anim_datatype, anim_value);
}

void gf_svg_reset_animate_values(SMIL_AnimateValues anim_values)
{
	u32 i, count;
	count = gf_list_count(anim_values.values);
	for (i = 0; i < count; i++) {
		void *value = gf_list_get(anim_values.values, i);
		gf_svg_delete_one_anim_value(anim_values.type, value);
	}
	gf_list_del(anim_values.values);
	anim_values.values = NULL;
}

void gf_svg_reset_animate_value(SMIL_AnimateValue anim_value)
{
	gf_svg_delete_one_anim_value(anim_value.type, anim_value.value);
	anim_value.value = NULL;
}


void gf_smil_delete_key_types(GF_List *l)
{
	while (gf_list_count(l)) {
		Fixed *t = gf_list_get(l, 0);
		gf_list_rem(l, 0);
		free(t);
	}
	gf_list_del(l);
}

void gf_svg_init_core(SVGElement *p) 
{
	GF_SAFEALLOC(p->core, sizeof(XMLCoreAttributes))
}

void gf_svg_init_properties(SVGElement *p) 
{
	GF_SAFEALLOC(p->properties, sizeof(SVGProperties))
	p->properties->fill.type = SVG_PAINT_INHERIT;
	
	p->properties->color.type = SVG_PAINT_COLOR;
	p->properties->color.color.type = SVG_COLOR_INHERIT;

	p->properties->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties->fill_opacity.type = SVG_NUMBER_INHERIT;
	p->properties->stroke.type = SVG_PAINT_INHERIT;
	p->properties->viewport_fill.type = SVG_PAINT_INHERIT;
	p->properties->stop_color.type = SVG_PAINT_INHERIT;
	p->properties->stroke_opacity.type = SVG_NUMBER_INHERIT;
	p->properties->stroke_width.type = SVG_NUMBER_INHERIT;
	p->properties->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties->stroke_miterlimit.type = SVG_NUMBER_INHERIT;
	p->properties->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties->stroke_dashoffset.type = SVG_NUMBER_INHERIT;
	p->properties->font_size.type = SVG_NUMBER_INHERIT;
	p->properties->text_anchor = SVG_TEXTANCHOR_INHERIT;
}

void gf_svg_init_focus(SVGElement *p)
{
	GF_SAFEALLOC(p->focus, sizeof(SVGFocusAttributes))
}

void gf_svg_init_xlink(SVGElement *p)
{
	GF_SAFEALLOC(p->xlink, sizeof(XLinkAttributes))
}

void gf_svg_init_timing(SVGElement *p)
{
	GF_SAFEALLOC(p->timing, sizeof(SMILTimingAttributes))		
	p->timing->begin = gf_list_new();
	p->timing->end = gf_list_new();
	p->timing->min.type = SMIL_DURATION_DEFINED;
	p->timing->repeatDur.type = SMIL_DURATION_UNSPECIFIED;
}

void gf_svg_init_sync(SVGElement *p)
{
	GF_SAFEALLOC(p->sync, sizeof(SMILSyncAttributes))				
}

void gf_svg_init_anim(SVGElement *p)
{
	GF_SAFEALLOC(p->anim, sizeof(SMILAnimationAttributes))				
	p->anim->keySplines = gf_list_new();
	p->anim->keyTimes = gf_list_new();
	p->anim->values.values = gf_list_new();
	if (gf_node_get_tag((GF_Node *)p) == TAG_SVG_animateMotion)
		p->anim->calcMode = SMIL_CALCMODE_PACED;
}

void gf_svg_init_conditional(SVGElement *p)
{
	GF_SAFEALLOC(p->conditional, sizeof(SVGConditionalAttributes))				
	p->conditional->requiredExtensions = gf_list_new();
	p->conditional->requiredFeatures = gf_list_new();
	p->conditional->requiredFonts = gf_list_new();
	p->conditional->requiredFormats = gf_list_new();
	p->conditional->systemLanguage = gf_list_new();
}

void gf_svg_delete_core(XMLCoreAttributes *p) 
{
	gf_svg_reset_iri(&(p->base));
	if (p->lang) free(p->lang);
	if (p->_class) free(p->_class);
	free(p);
}

void gf_svg_delete_properties(SVGProperties *p) 
{
	free(p->font_family.value);
	free(p->stroke_dasharray.array.vals);
	free(p);
}

void gf_svg_reset_focus(SVG_Focus focus) 
{
	return;
}

void gf_svg_delete_focus(SVGFocusAttributes *p) 
{
	gf_svg_reset_focus(p->nav_next);
	gf_svg_reset_focus(p->nav_prev);
	gf_svg_reset_focus(p->nav_down);
	gf_svg_reset_focus(p->nav_down_left);
	gf_svg_reset_focus(p->nav_down_right);
	gf_svg_reset_focus(p->nav_left);
	gf_svg_reset_focus(p->nav_right);
	gf_svg_reset_focus(p->nav_up);
	gf_svg_reset_focus(p->nav_up_left);
	gf_svg_reset_focus(p->nav_up_right);
	free(p);		
}

void gf_svg_delete_xlink(XLinkAttributes *p)
{
	gf_svg_reset_iri(&(p->href));
	if (p->type) free(p->type);
	if (p->title) free(p->title);
	gf_svg_reset_iri(&(p->arcrole));
	gf_svg_reset_iri(&(p->role));
	if (p->show) free(p->show);
	if (p->actuate) free(p->actuate);
	free(p);		
}

void gf_svg_delete_timing(SMILTimingAttributes *p)
{
	gf_smil_delete_times(p->begin);
	gf_smil_delete_times(p->end);
	free(p->runtime);
	free(p);		
}

void gf_svg_delete_sync(SMILSyncAttributes *p)
{
	free(p);
}

void gf_svg_delete_anim(SMILAnimationAttributes *p)
{
	gf_smil_delete_key_types(p->keySplines);
	gf_smil_delete_key_types(p->keyTimes);
	gf_svg_reset_animate_value(p->from);
	gf_svg_reset_animate_value(p->by);
	gf_svg_reset_animate_value(p->to);
	gf_svg_reset_animate_values(p->values);
	free(p);		
} 

void gf_svg_delete_conditional(SVGConditionalAttributes *p)
{
	gf_list_del(p->requiredExtensions);
	gf_list_del(p->requiredFeatures);
	gf_list_del(p->requiredFonts);
	gf_list_del(p->requiredFormats);
	gf_list_del(p->systemLanguage);
	free(p);		
} 

void gf_svg_reset_base_element(SVGElement *p)
{
	if (p->textContent) free(p->textContent);
	if (p->core)		gf_svg_delete_core(p->core);
	if (p->properties)	gf_svg_delete_properties(p->properties);
	if (p->focus)		gf_svg_delete_focus(p->focus);
	if (p->conditional) gf_svg_delete_conditional(p->conditional);
	if (p->anim)		gf_svg_delete_anim(p->anim);
	if (p->sync)		gf_svg_delete_sync(p->sync);
	if (p->timing)		gf_svg_delete_timing(p->timing);
	if (p->xlink)		gf_svg_delete_xlink(p->xlink);
}

void *gf_svg_get_property_pointer_by_name(SVGPropertiesPointers *p, const char *name)
{
	if (!p) return NULL;
	if (!strcmp(name, "color")) return p->color;
	else if (!strcmp(name, "fill")) return p->fill;
	else if (!strcmp(name, "stroke")) return p->stroke;
	else if (!strcmp(name, "solid-color")) return p->solid_color;
	else if (!strcmp(name, "stop-color")) return p->stop_color;
	else if (!strcmp(name, "viewport-fill")) return p->viewport_fill;
	else if (!strcmp(name, "fill-opacity")) return p->fill_opacity;
	else if (!strcmp(name, "solid-opacity")) return p->solid_opacity;
	else if (!strcmp(name, "stop-opacity")) return p->stop_opacity;
	else if (!strcmp(name, "stroke-opacity")) return p->stop_opacity;
	else if (!strcmp(name, "viewport-fill-opacity")) return p->viewport_fill_opacity;
	else if (!strcmp(name, "audio-level")) return p->audio_level;
	else if (!strcmp(name, "color-rendering")) return p->color_rendering;
	else if (!strcmp(name, "image-rendering")) return p->image_rendering;
	else if (!strcmp(name, "shape-rendering")) return p->shape_rendering;
	else if (!strcmp(name, "text-rendering")) return p->text_rendering;
	else if (!strcmp(name, "display")) return p->display;
	else if (!strcmp(name, "display-align")) return p->display_align;
	else if (!strcmp(name, "fill-rule")) return p->fill_rule;
	else if (!strcmp(name, "font-family")) return p->font_family;
	else if (!strcmp(name, "font-size")) return p->font_size;
	else if (!strcmp(name, "font-style")) return p->font_style;
	else if (!strcmp(name, "font-weight")) return p->font_weight;
	else if (!strcmp(name, "line-increment")) return p->line_increment;
	else if (!strcmp(name, "pointer-events")) return p->pointer_events;
	else if (!strcmp(name, "stroke-dasharray")) return p->stroke_dasharray;
	else if (!strcmp(name, "stroke-dashoffset")) return p->stroke_dashoffset;
	else if (!strcmp(name, "stroke-linecap")) return p->stroke_linecap;
	else if (!strcmp(name, "stroke-linejoin")) return p->stroke_linejoin;
	else if (!strcmp(name, "stroke-miterlimit")) return p->stroke_miterlimit;
	else if (!strcmp(name, "stroke-width")) return p->stroke_width;
	else if (!strcmp(name, "text-anchor")) return p->text_anchor;
	else if (!strcmp(name, "vector-effect")) return p->vector_effect;
	else if (!strcmp(name, "visibilty")) return p->visibility;
	else if (!strcmp(name, "opacity")) return p->opacity;
	else if (!strcmp(name, "overflow")) return p->overflow;
	else return NULL;
}

#endif /*GPAC_DISABLE_SVG*/
