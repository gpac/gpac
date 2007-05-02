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
#include <gpac/internal/scenegraph_dev.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/renderer_dev.h>

#ifdef GPAC_ENABLE_SVG_SA
#include <gpac/nodes_svg_sa.h>
#endif
#ifdef GPAC_ENABLE_SVG_SANI
#include <gpac/nodes_svg_sani.h>
#endif

/*common tool sfor SVG SA and SANI*/
#ifdef GPAC_ENABLE_SVG_SA_BASE


static void svg_delete_string_list(GF_List *l) 
{
	while (gf_list_count(l)) {
		char *str = (char *)gf_list_last(l);
		gf_list_rem_last(l);
		free(str);
	}
	gf_list_del(l);
}

void gf_svg_sa_delete_conditional(SVGConditionalAttributes *p)
{
	while (gf_list_count(p->requiredFeatures)) {
		XMLRI *iri = (XMLRI *)gf_list_last(p->requiredFeatures);
		gf_list_rem_last(p->requiredFeatures);
		if (iri->string) free(iri->string);
		free(iri);
	}
	gf_list_del(p->requiredFeatures);

	while (gf_list_count(p->requiredExtensions)) {
		XMLRI *iri = (XMLRI *)gf_list_last(p->requiredExtensions);
		gf_list_rem_last(p->requiredExtensions);
		if (iri->string) free(iri->string);
		free(iri);
	}
	gf_list_del(p->requiredExtensions);

	svg_delete_string_list(p->requiredFonts);
	svg_delete_string_list(p->requiredFormats);
	svg_delete_string_list(p->systemLanguage);
	free(p);		
} 

void gf_svg_sa_delete_sync(SMILSyncAttributes *p)
{
	if (p->syncReference) free(p->syncReference);
	free(p);
}

void gf_svg_sa_delete_anim(SMILAnimationAttributes *p, GF_SceneGraph *sg)
{
	gf_smil_delete_key_types(p->keySplines);
	gf_smil_delete_key_types(p->keyTimes);
	gf_svg_reset_animate_value(p->from, sg);
	gf_svg_reset_animate_value(p->by, sg);
	gf_svg_reset_animate_value(p->to, sg);
	gf_svg_reset_animate_values(p->values, sg);
	free(p);		
} 
void gf_svg_sa_delete_timing(SMILTimingAttributes *p)
{
	gf_smil_delete_times(p->begin);
	gf_smil_delete_times(p->end);
	free(p);		
}


/* 
	Initialization of properties at the element level 
	  -	If a property is inheritable by default ('inherit: yes' in the property definition), 
		the default value should be inherit.
	  -	If a property is not inheritable by default ('inherit: no' in the property definition), 
		it should have the initial value.
*/
void gf_svg_sa_init_properties(SVG_SA_Element *p) 
{
	GF_SAFEALLOC(p->properties, SVGProperties)

	p->properties->audio_level.type = SVG_NUMBER_VALUE;
	p->properties->audio_level.value = FIX_ONE;
	
	p->properties->color.type = SVG_PAINT_COLOR;
	p->properties->color.color.type = SVG_COLOR_INHERIT;

	p->properties->color_rendering = SVG_RENDERINGHINT_INHERIT;

	p->properties->display = SVG_DISPLAY_INLINE;

	p->properties->display_align = SVG_DISPLAYALIGN_INHERIT;

	p->properties->fill.type = SVG_PAINT_INHERIT;

	p->properties->fill_opacity.type = SVG_NUMBER_INHERIT;
	
	p->properties->fill_rule = SVG_FILLRULE_INHERIT;

	p->properties->font_family.type = SVG_FONTFAMILY_INHERIT;

	p->properties->font_size.type = SVG_NUMBER_INHERIT;

	p->properties->font_style = SVG_FONTSTYLE_INHERIT;

	p->properties->font_variant = SVG_FONTVARIANT_INHERIT;

	p->properties->font_weight = SVG_FONTWEIGHT_INHERIT;

	p->properties->image_rendering = SVG_RENDERINGHINT_INHERIT;

	p->properties->line_increment.type = SVG_NUMBER_INHERIT;

	p->properties->opacity.type = SVG_NUMBER_VALUE;
	p->properties->opacity.value = FIX_ONE;

	p->properties->pointer_events = SVG_POINTEREVENTS_INHERIT;

	p->properties->shape_rendering = SVG_RENDERINGHINT_INHERIT;

	p->properties->solid_color.type = SVG_PAINT_COLOR;
	p->properties->solid_color.color.type = SVG_COLOR_RGBCOLOR;
	/*
	p->properties->solid_color.color.red = 0;
	p->properties->solid_color.color.green = 0;
	p->properties->solid_color.color.blue = 0;
	*/

	p->properties->solid_opacity.type = SVG_NUMBER_VALUE;
	p->properties->solid_opacity.value = FIX_ONE;

	p->properties->stop_color.type = SVG_PAINT_COLOR;
	p->properties->stop_color.color.type = SVG_COLOR_RGBCOLOR;
	/*
	p->properties->stop_color.color.red = 0;
	p->properties->stop_color.color.green = 0;
	p->properties->stop_color.color.blue = 0;
	*/

	p->properties->stop_opacity.type = SVG_NUMBER_VALUE;
	p->properties->stop_opacity.value = FIX_ONE;

	p->properties->stroke.type = SVG_PAINT_INHERIT;
	
	p->properties->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	
	p->properties->stroke_dashoffset.type = SVG_NUMBER_INHERIT;

	p->properties->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	
	p->properties->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	
	p->properties->stroke_miterlimit.type = SVG_NUMBER_INHERIT;
	
	p->properties->stroke_opacity.type = SVG_NUMBER_INHERIT;
	
	p->properties->stroke_width.type = SVG_NUMBER_INHERIT;
	
	p->properties->text_align = SVG_TEXTALIGN_INHERIT;
	
	p->properties->text_anchor = SVG_TEXTANCHOR_INHERIT;
	
	p->properties->text_rendering = SVG_RENDERINGHINT_INHERIT;

	p->properties->vector_effect = SVG_VECTOREFFECT_NONE;

	p->properties->viewport_fill.type = SVG_PAINT_NONE;
	
	p->properties->viewport_fill_opacity.type = SVG_NUMBER_VALUE;
	p->properties->viewport_fill_opacity.value = FIX_ONE;
	
	p->properties->visibility = SVG_VISIBILITY_INHERIT;

/*
	This is the list of attributes which either inherit a value from the parent when not specified, 
	or explicitely use an inherit value, even though they are not CSS properties:
	contentScriptType
	externalResourcesRequired
	syncBehaviorDefault
	syncToleranceDefault
	xml:base
	xml:lang
	xml:space
  
*/
}


void gf_svg_sa_init_lsr_conditional(SVGCommandBuffer *script)
{
	memset(script, 0, sizeof(SVGCommandBuffer));
	script->com_list = gf_list_new();
}

void gf_svg_sa_reset_lsr_conditional(SVGCommandBuffer *script)
{
	u32 i;
	if (script->data) free(script->data);
	for (i=gf_list_count(script->com_list); i>0; i--) {
		GF_Command *com = (GF_Command *)gf_list_get(script->com_list, i-1);
		gf_sg_command_del(com);
	}
	gf_list_del(script->com_list);
}

Bool gf_svg_sa_is_animation_tag(u32 tag)
{
	switch (tag) {
#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_set:
	case TAG_SVG_SA_animate:
	case TAG_SVG_SA_animateColor:
	case TAG_SVG_SA_animateTransform:
	case TAG_SVG_SA_animateMotion:
	case TAG_SVG_SA_discard:
		return 1;
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	case TAG_SVG_SANI_set:
	case TAG_SVG_SANI_animate:
	case TAG_SVG_SANI_animateColor:
	case TAG_SVG_SANI_animateTransform:
	case TAG_SVG_SANI_animateMotion:
	case TAG_SVG_SANI_discard:
		return 1;
#endif
	default:
		return 0;
	}
}

#endif	/*GPAC_ENABLE_SVG_SA_BASE*/


#ifdef GPAC_ENABLE_SVG_SA

void gf_svg_sa_init_core(SVG_SA_Element *p) 
{
	GF_SAFEALLOC(p->core, XMLCoreAttributes)
}

void gf_svg_sa_init_focus(SVG_SA_Element *p)
{
	GF_SAFEALLOC(p->focus, SVGFocusAttributes)
}

void gf_svg_sa_init_xlink(SVG_SA_Element *p)
{
	GF_SAFEALLOC(p->xlink, XLinkAttributes)
}

void gf_svg_sa_init_timing(SVG_SA_Element *p)
{
	GF_SAFEALLOC(p->timing, SMILTimingAttributes)
	p->timing->begin = gf_list_new();
	p->timing->end = gf_list_new();
	p->timing->min.type = SMIL_DURATION_DEFINED;
	p->timing->repeatDur.type = SMIL_DURATION_INDEFINITE;
}

void gf_svg_sa_init_sync(SVG_SA_Element *p)
{
	GF_SAFEALLOC(p->sync, SMILSyncAttributes)
}

void gf_svg_sa_init_anim(SVG_SA_Element *p)
{
	GF_SAFEALLOC(p->anim, SMILAnimationAttributes)
	p->anim->lsr_enabled = 1;
	p->anim->keySplines = gf_list_new();
	p->anim->keyTimes = gf_list_new();
	p->anim->values.values = gf_list_new();
	if (gf_node_get_tag((GF_Node *)p) == TAG_SVG_SA_animateMotion)
		p->anim->calcMode = SMIL_CALCMODE_PACED;
}

void gf_svg_sa_init_conditional(SVG_SA_Element *p)
{
	GF_SAFEALLOC(p->conditional, SVGConditionalAttributes)
	p->conditional->requiredExtensions = gf_list_new();
	p->conditional->requiredFeatures = gf_list_new();
	p->conditional->requiredFonts = gf_list_new();
	p->conditional->requiredFormats = gf_list_new();
	p->conditional->systemLanguage = gf_list_new();
}

static void gf_svg_sa_delete_core(SVG_SA_Element *elt, XMLCoreAttributes *p) 
{
	gf_svg_reset_iri(elt->sgprivate->scenegraph, &p->base);
	if (p->lang) free(p->lang);
	if (p->_class) free(p->_class);
	free(p);
}

void gf_svg_delete_properties(SVG_SA_Element *elt,SVGProperties *p) 
{
	free(p->font_family.value);
	gf_svg_reset_iri(elt->sgprivate->scenegraph, &p->fill.iri);
	gf_svg_reset_iri(elt->sgprivate->scenegraph, &p->stroke.iri);
	free(p->stroke_dasharray.array.vals);
	free(p);
}

static void svg_reset_focus(SVG_SA_Element *elt, SVG_Focus *focus) 
{
	if (focus->target.target) {
		gf_svg_unregister_iri(elt->sgprivate->scenegraph, &focus->target);
	}
	if (focus->target.string) free(focus->target.string);
}

void gf_svg_delete_focus(SVG_SA_Element *elt, SVGFocusAttributes *p) 
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

void gf_svg_sa_delete_xlink(SVG_SA_Element *elt, XLinkAttributes *p)
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

void gf_svg_sa_reset_base_element(SVG_SA_Element *p)
{
	if (p->textContent) free(p->textContent);
	if (p->core)		gf_svg_sa_delete_core(p, p->core);
	if (p->properties)	gf_svg_delete_properties(p, p->properties);
	if (p->focus)		gf_svg_delete_focus(p, p->focus);
	if (p->conditional) gf_svg_sa_delete_conditional(p->conditional);
	if (p->sync)		gf_svg_sa_delete_sync(p->sync);

	if (p->sgprivate->interact && p->sgprivate->interact->animations) gf_smil_anim_delete_animations((GF_Node*)p);
	if (p->anim)		{
		gf_svg_sa_delete_anim(p->anim, p->sgprivate->scenegraph);
		gf_smil_anim_remove_from_target((GF_Node *)p, (GF_Node *)p->xlink->href.target);
	}
	
	if (p->timing)		{
		gf_smil_timing_delete_runtime_info((GF_Node *)p, p->timing->runtime);
		p->timing->runtime = NULL;
		gf_svg_sa_delete_timing(p->timing);
	}
	
	if (p->xlink)		gf_svg_sa_delete_xlink(p, p->xlink);
}

static void lsr_conditional_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_simple_time, u32 status)
{	
	if (status==SMIL_TIMING_EVAL_UPDATE) {
		SVG_SA_conditionalElement *cond = (SVG_SA_conditionalElement *)rti->timed_elt;
		if (cond->updates.data) {
			cond->updates.exec_command_list(cond);
		} else if (gf_list_count(cond->updates.com_list)) {
			gf_sg_command_apply_list(cond->sgprivate->scenegraph, cond->updates.com_list, gf_node_get_scene_time((GF_Node*)cond) );
		}
	}
}

Bool gf_svg_sa_node_init(GF_Node *node)
{
	switch (node->sgprivate->tag) {
	case TAG_SVG_SA_script:
		if (node->sgprivate->scenegraph->script_load) 
			node->sgprivate->scenegraph->script_load(node);
		return 1;
	case TAG_SVG_SA_conditional:
		gf_smil_timing_init_runtime_info(node);
		((SVG_SA_Element *)node)->timing->runtime->evaluate = lsr_conditional_evaluate;
		gf_smil_setup_events(node);
		return 1;
	case TAG_SVG_SA_handler:
		if (node->sgprivate->scenegraph->script_load) 
			node->sgprivate->scenegraph->script_load(node);
		if (node->sgprivate->scenegraph->js_ifce)
			((SVG_SA_handlerElement *)node)->handle_event = gf_sg_handle_dom_event;
		return 1;
	case TAG_SVG_SA_animateMotion:
	case TAG_SVG_SA_set: 
	case TAG_SVG_SA_animate: 
	case TAG_SVG_SA_animateColor: 
	case TAG_SVG_SA_animateTransform: 
		gf_smil_anim_init_node(node);
	case TAG_SVG_SA_audio: 
	case TAG_SVG_SA_video: 
		gf_smil_setup_events(node);
		/*we may get called several times depending on xlink:href resoling for events*/
		return (node->sgprivate->UserPrivate || node->sgprivate->UserCallback) ? 1 : 0;
	/*discard is implemented as a special animation element */
	case TAG_SVG_SA_discard: 
		gf_smil_anim_init_discard(node);
		gf_smil_setup_events(node);
		return 1;
	default:
		return 0;
	}
	return 0;
}

Bool gf_svg_sa_node_changed(GF_Node *node, GF_FieldInfo *field)
{
	switch (node->sgprivate->tag) {
	case TAG_SVG_SA_animateMotion:
	case TAG_SVG_SA_set: 
	case TAG_SVG_SA_animate: 
	case TAG_SVG_SA_animateColor: 
	case TAG_SVG_SA_animateTransform: 
	case TAG_SVG_SA_conditional: 
		gf_smil_timing_modified(node, field);
		return 1;
	case TAG_SVG_SA_audio: 
	case TAG_SVG_SA_video: 
		gf_smil_timing_modified(node, field);
		/*used by renderers*/
		return 0;
	}
	return 0;
}



#else
u32 gf_svg_sa_node_type_by_class_name(const char *element_name)
{
	return 0;
}
s32 gf_svg_sa_get_attribute_index_by_name(GF_Node *node, char *name)
{
	return -1;
}

#endif /*GPAC_ENABLE_SVG_SA*/




#else

/*these ones are only needed for W32 libgpac_dll build in order not to modify export def file*/
u32 gf_svg_sa_node_type_by_class_name(const char *element_name)
{
	return 0;
}
u32 gf_svg_sa_get_attribute_count(GF_Node *n)
{
	return 0;
}
GF_Err gf_svg_sa_get_attribute_info(GF_Node *node, GF_FieldInfo *info)
{
	return GF_NOT_SUPPORTED;
}

GF_Node *gf_svg_sa_create_node(GF_SceneGraph *inScene, u32 tag)
{
	return NULL;
}
const char *gf_svg_sa_get_element_name(u32 tag)
{
	return "Unsupported";
}
Bool gf_svg_sa_node_init(GF_Node *node)
{
	return 0;
}

Bool gf_svg_conditional_eval(SVG_SA_Element *node)
{
}




#endif	//GPAC_DISABLE_SVG
