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
	u32 i, count;
	count = gf_list_count(d.commands);
	for (i = 0; i < count; i++) {
		u8 *command = gf_list_get(d.commands, i);
		free(command);
	}
	gf_list_del(d.commands);
	count = gf_list_count(d.points);
	for (i = 0; i < count; i++) {
		SVG_Point *pt = gf_list_get(d.points, i);
		free(pt);
	}
	gf_list_del(d.points);
}

void gf_smil_delete_times(GF_List *list)
{
	u32 i, count;
	count = gf_list_count(list);
	for (i = 0; i < count; i++) {
		SMIL_Time *v=gf_list_get(list, i);
		if (v->element_id) free(v->element_id);
		free(v);
	}
	gf_list_del(list);
}

void gf_svg_delete_points(GF_List *list)
{
	u32 i, count = gf_list_count(list);
	for (i = 0; i < count; i++) {
		SVG_Point *p=gf_list_get(list, i);
		free(p);
	}
	gf_list_del(list);
}

void gf_svg_delete_coordinates(GF_List *list)
{
	u32 i, count = gf_list_count(list);
	for (i = 0; i < count; i++) {
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

void gf_svg_reset_iri(GF_SceneGraph *sg, SVG_IRI *iri) 
{
	if (!iri) return;
	if (iri->iri) free(iri->iri);
	gf_list_del_item(sg->xlink_hrefs, iri);
}

void gf_svg_delete_attribute_value(u32 type, void *value, GF_SceneGraph *sg)
{
	GF_List *l;
	switch (type) {
	case SVG_Paint_datatype:
		gf_svg_delete_paint((SVG_Paint *)value);
		break;
	case SVG_IRI_datatype:
		gf_svg_reset_iri(sg, (SVG_IRI *)value);
		free(value);
		break;
	case SVG_String_datatype:
		if (*(SVG_String *)value) free(*(SVG_String *)value);
		free(value);
		break;
	case SVG_StrokeDashArray_datatype:
		if (((SVG_StrokeDashArray*)value)->array.vals) free(((SVG_StrokeDashArray*)value)->array.vals);
		free(value);
		break;
	case SVG_Coordinates_datatype:
	case SVG_Points_datatype:
		l = *(GF_List**)value;
		while (gf_list_count(l)) {
			void *n = gf_list_last(l);
			gf_list_rem_last(l);
			free(n);
		}
		gf_list_del(l);
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

static void svg_delete_one_anim_value(u8 anim_datatype, void *anim_value, GF_SceneGraph *sg)
{
	/* TODO: handle specific animation types : Motion, else ? */
	gf_svg_delete_attribute_value(anim_datatype, anim_value, sg);
}

static void svg_reset_animate_values(SMIL_AnimateValues anim_values, GF_SceneGraph *sg)
{
	u32 i, count;
	count = gf_list_count(anim_values.values);
	for (i = 0; i < count; i++) {
		void *value = gf_list_get(anim_values.values, i);
		svg_delete_one_anim_value(anim_values.type, value, sg);
	}
	gf_list_del(anim_values.values);
	anim_values.values = NULL;
}

static void svg_reset_animate_value(SMIL_AnimateValue anim_value, GF_SceneGraph *sg)
{
	svg_delete_one_anim_value(anim_value.type, anim_value.value, sg);
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
	p->properties->opacity.type = SVG_NUMBER_INHERIT;
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
	p->properties->font_weight = SVG_FONTWEIGHT_INHERIT;
	p->properties->visibility = SVG_VISIBILITY_INHERIT;

	p->properties->stop_opacity.type = SVG_NUMBER_VALUE;
	p->properties->stop_opacity.value = FIX_ONE;
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
	p->anim->lsr_enabled = 1;
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

void gf_svg_delete_core(SVGElement *elt, XMLCoreAttributes *p) 
{
	gf_svg_reset_iri(elt->sgprivate->scenegraph, &p->base);
	if (p->lang) free(p->lang);
	if (p->_class) free(p->_class);
	free(p);
}

void gf_svg_delete_properties(SVGProperties *p) 
{
	free(p->font_family.value);
	if (p->fill.uri) free(p->fill.uri);
	if (p->stroke.uri) free(p->stroke.uri);
	free(p->stroke_dasharray.array.vals);
	free(p);
}

static void svg_reset_focus(SVGElement *elt, SVG_Focus *focus) 
{
	if (focus->target.target) {
		gf_svg_unregister_iri(elt->sgprivate->scenegraph, &focus->target);
	}
	if (focus->target.iri) free(focus->target.iri);
}

void gf_svg_delete_focus(SVGElement *elt, SVGFocusAttributes *p) 
{
	svg_reset_focus(elt, &p->nav_next);
	svg_reset_focus(elt, &p->nav_prev);
	svg_reset_focus(elt, &p->nav_down);
	svg_reset_focus(elt, &p->nav_down_left);
	svg_reset_focus(elt, &p->nav_down_right);
	svg_reset_focus(elt, &p->nav_left);
	svg_reset_focus(elt, &p->nav_right);
	svg_reset_focus(elt, &p->nav_up);
	svg_reset_focus(elt, &p->nav_up_left);
	svg_reset_focus(elt, &p->nav_up_right);
	free(p);		
}

void gf_svg_delete_xlink(SVGElement *elt, XLinkAttributes *p)
{
	gf_svg_reset_iri(elt->sgprivate->scenegraph, &p->href);
	if (p->type) free(p->type);
	if (p->title) free(p->title);
	gf_svg_reset_iri(elt->sgprivate->scenegraph, &p->arcrole);
	gf_svg_reset_iri(elt->sgprivate->scenegraph, &p->role);
	if (p->show) free(p->show);
	if (p->actuate) free(p->actuate);
	free(p);		
}

void gf_svg_delete_timing(SMILTimingAttributes *p)
{
	gf_smil_delete_times(p->begin);
	gf_smil_delete_times(p->end);
	if (p->runtime) free(p->runtime);
	free(p);		
}

void gf_svg_delete_sync(SMILSyncAttributes *p)
{
	if (p->syncReference) free(p->syncReference);
	free(p);
}

static void gf_svg_delete_anim(SMILAnimationAttributes *p, GF_SceneGraph *sg)
{
	gf_smil_delete_key_types(p->keySplines);
	gf_smil_delete_key_types(p->keyTimes);
	svg_reset_animate_value(p->from, sg);
	svg_reset_animate_value(p->by, sg);
	svg_reset_animate_value(p->to, sg);
	svg_reset_animate_values(p->values, sg);
	free(p);		
} 

static void svg_delete_string_list(GF_List *l) 
{
	while (gf_list_count(l)) {
		char *str = gf_list_last(l);
		gf_list_rem_last(l);
		free(str);
	}
	gf_list_del(l);
}
void gf_svg_delete_conditional(SVGConditionalAttributes *p)
{
	while (gf_list_count(p->requiredFeatures)) {
		SVG_IRI *iri = gf_list_last(p->requiredFeatures);
		gf_list_rem_last(p->requiredFeatures);
		if (iri->iri) free(iri->iri);
		free(iri);
	}
	gf_list_del(p->requiredFeatures);

	while (gf_list_count(p->requiredExtensions)) {
		SVG_IRI *iri = gf_list_last(p->requiredExtensions);
		gf_list_rem_last(p->requiredExtensions);
		if (iri->iri) free(iri->iri);
		free(iri);
	}
	gf_list_del(p->requiredExtensions);

	svg_delete_string_list(p->requiredFonts);
	svg_delete_string_list(p->requiredFormats);
	svg_delete_string_list(p->systemLanguage);
	free(p);		
} 

void gf_svg_reset_base_element(SVGElement *p)
{
	if (p->textContent) free(p->textContent);
	if (p->core)		gf_svg_delete_core(p, p->core);
	if (p->properties)	gf_svg_delete_properties(p->properties);
	if (p->focus)		gf_svg_delete_focus(p, p->focus);
	if (p->conditional) gf_svg_delete_conditional(p->conditional);
	if (p->anim)		gf_svg_delete_anim(p->anim, p->sgprivate->scenegraph);
	if (p->sync)		gf_svg_delete_sync(p->sync);
	if (p->timing)		gf_svg_delete_timing(p->timing);
	if (p->xlink)		gf_svg_delete_xlink(p, p->xlink);
}

#endif /*GPAC_DISABLE_SVG*/
