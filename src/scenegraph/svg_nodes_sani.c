/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean Le Feuvre
 *    Copyright (c)2004-200X ENST - All rights reserved
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


/*
	DO NOT MOFIFY - File generated on GMT Mon Apr 23 13:10:06 2007

	BY SVGGen for GPAC Version 0.4.3-DEV
*/

#include <gpac/nodes_svg_sani.h>

#ifndef GPAC_DISABLE_SVG

#ifdef GPAC_ENABLE_SVG_SANI

#include <gpac/internal/scenegraph_dev.h>

void *gf_svg_sani_new_a()
{
	SVG_SANI_aElement *p;
	GF_SAFEALLOC(p, SVG_SANI_aElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_a);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "a";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_a_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_a_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	return p;
}

static void gf_svg_sani_a_del(GF_Node *node)
{
	SVG_SANI_aElement *p = (SVG_SANI_aElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->target) free(p->target);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_a_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 20:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 21:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 22:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 23:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 24:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 25:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 26:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 27:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 28:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 29:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 30:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 31:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 32:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 33:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_aElement *)node)->display;
			return GF_OK;
		case 34:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_aElement *)node)->visibility;
			return GF_OK;
		case 35:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = & ((SVG_SANI_aElement *)node)->pointer_events;
			return GF_OK;
		case 36:
			info->name = "target";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVG_SANI_aElement *)node)->target;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_animate()
{
	SVG_SANI_animateElement *p;
	GF_SAFEALLOC(p, SVG_SANI_animateElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_animate);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animate";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_animate_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_animate_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	gf_svg_sani_init_timing((SVG_SANI_Element *)p);
	gf_svg_sani_init_anim((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_animate_del(GF_Node *node)
{
	SVG_SANI_animateElement *p = (SVG_SANI_animateElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_animate_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->attributeName;
			return GF_OK;
		case 24:
			info->name = "attributeType";
			info->fieldType = SMIL_AttributeType_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->attributeType;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->to;
			return GF_OK;
		case 26:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->from;
			return GF_OK;
		case 27:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->by;
			return GF_OK;
		case 28:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->values;
			return GF_OK;
		case 29:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->calcMode;
			return GF_OK;
		case 30:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->keySplines;
			return GF_OK;
		case 31:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->keyTimes;
			return GF_OK;
		case 32:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->accumulate;
			return GF_OK;
		case 33:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->additive;
			return GF_OK;
		case 34:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->lsr_enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_animateColor()
{
	SVG_SANI_animateColorElement *p;
	GF_SAFEALLOC(p, SVG_SANI_animateColorElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_animateColor);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animateColor";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_animateColor_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_animateColor_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	gf_svg_sani_init_timing((SVG_SANI_Element *)p);
	gf_svg_sani_init_anim((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_animateColor_del(GF_Node *node)
{
	SVG_SANI_animateColorElement *p = (SVG_SANI_animateColorElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_animateColor_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->attributeName;
			return GF_OK;
		case 24:
			info->name = "attributeType";
			info->fieldType = SMIL_AttributeType_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->attributeType;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->to;
			return GF_OK;
		case 26:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->from;
			return GF_OK;
		case 27:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->by;
			return GF_OK;
		case 28:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->values;
			return GF_OK;
		case 29:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->calcMode;
			return GF_OK;
		case 30:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->keySplines;
			return GF_OK;
		case 31:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->keyTimes;
			return GF_OK;
		case 32:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->accumulate;
			return GF_OK;
		case 33:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->additive;
			return GF_OK;
		case 34:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->lsr_enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_animateMotion()
{
	SVG_SANI_animateMotionElement *p;
	GF_SAFEALLOC(p, SVG_SANI_animateMotionElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_animateMotion);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animateMotion";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_animateMotion_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_animateMotion_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	gf_svg_sani_init_timing((SVG_SANI_Element *)p);
	gf_svg_sani_init_anim((SVG_SANI_Element *)p);
#ifdef USE_GF_PATH
	gf_path_reset(&p->path);
#else
	p->path.commands = gf_list_new();
	p->path.points = gf_list_new();
#endif
	p->keyPoints = gf_list_new();
	return p;
}

static void gf_svg_sani_animateMotion_del(GF_Node *node)
{
	SVG_SANI_animateMotionElement *p = (SVG_SANI_animateMotionElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_svg_reset_path(p->path);
	gf_smil_delete_key_types(p->keyPoints);
	free(p->origin);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_animateMotion_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->to;
			return GF_OK;
		case 24:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->from;
			return GF_OK;
		case 25:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->by;
			return GF_OK;
		case 26:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->values;
			return GF_OK;
		case 27:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->calcMode;
			return GF_OK;
		case 28:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->keySplines;
			return GF_OK;
		case 29:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->keyTimes;
			return GF_OK;
		case 30:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->accumulate;
			return GF_OK;
		case 31:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->additive;
			return GF_OK;
		case 32:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->lsr_enabled;
			return GF_OK;
		case 33:
			info->name = "path";
			info->fieldType = SVG_PathData_datatype;
			info->far_ptr = & ((SVG_SANI_animateMotionElement *)node)->path;
			return GF_OK;
		case 34:
			info->name = "keyPoints";
			info->fieldType = SMIL_KeyPoints_datatype;
			info->far_ptr = & ((SVG_SANI_animateMotionElement *)node)->keyPoints;
			return GF_OK;
		case 35:
			info->name = "rotate";
			info->fieldType = SVG_Rotate_datatype;
			info->far_ptr = & ((SVG_SANI_animateMotionElement *)node)->rotate;
			return GF_OK;
		case 36:
			info->name = "origin";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_animateMotionElement *)node)->origin;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_animateTransform()
{
	SVG_SANI_animateTransformElement *p;
	GF_SAFEALLOC(p, SVG_SANI_animateTransformElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_animateTransform);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animateTransform";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_animateTransform_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_animateTransform_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	gf_svg_sani_init_timing((SVG_SANI_Element *)p);
	gf_svg_sani_init_anim((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_animateTransform_del(GF_Node *node)
{
	SVG_SANI_animateTransformElement *p = (SVG_SANI_animateTransformElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_animateTransform_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->attributeName;
			return GF_OK;
		case 24:
			info->name = "attributeType";
			info->fieldType = SMIL_AttributeType_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->attributeType;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->to;
			return GF_OK;
		case 26:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->from;
			return GF_OK;
		case 27:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->by;
			return GF_OK;
		case 28:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->values;
			return GF_OK;
		case 29:
			info->name = "type";
			info->fieldType = SVG_TransformType_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->type;
			return GF_OK;
		case 30:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->calcMode;
			return GF_OK;
		case 31:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->keySplines;
			return GF_OK;
		case 32:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->keyTimes;
			return GF_OK;
		case 33:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->accumulate;
			return GF_OK;
		case 34:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->additive;
			return GF_OK;
		case 35:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->lsr_enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_animation()
{
	SVG_SANI_animationElement *p;
	GF_SAFEALLOC(p, SVG_SANI_animationElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_animation);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animation";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_animation_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_animation_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	gf_svg_sani_init_timing((SVG_SANI_Element *)p);
	gf_svg_sani_init_sync((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->audio_level.type = SVG_NUMBER_VALUE;
	p->audio_level.value = FIX_ONE;
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	p->viewport_fill.type = SVG_PAINT_NONE;
	p->viewport_fill_opacity.type = SVG_NUMBER_VALUE;
	p->viewport_fill_opacity.value = FIX_ONE;
	p->timing->dur.type = SMIL_DURATION_MEDIA;
	return p;
}

static void gf_svg_sani_animation_del(GF_Node *node)
{
	SVG_SANI_animationElement *p = (SVG_SANI_animationElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_animation_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 20:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 21:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 22:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 23:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 24:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 25:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 26:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->begin;
			return GF_OK;
		case 27:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->end;
			return GF_OK;
		case 28:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->dur;
			return GF_OK;
		case 29:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 30:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 31:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->restart;
			return GF_OK;
		case 32:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->min;
			return GF_OK;
		case 33:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->max;
			return GF_OK;
		case 34:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->fill;
			return GF_OK;
		case 35:
			info->name = "clipBegin";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->clipBegin;
			return GF_OK;
		case 36:
			info->name = "clipEnd";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->clipEnd;
			return GF_OK;
		case 37:
			info->name = "syncBehavior";
			info->fieldType = SMIL_SyncBehavior_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->sync->syncBehavior;
			return GF_OK;
		case 38:
			info->name = "syncTolerance";
			info->fieldType = SMIL_SyncTolerance_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->sync->syncTolerance;
			return GF_OK;
		case 39:
			info->name = "syncMaster";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->sync->syncMaster;
			return GF_OK;
		case 40:
			info->name = "syncReference";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->sync->syncReference;
			return GF_OK;
		case 41:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 42:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 43:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 44:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 45:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 46:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 47:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 48:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_animationElement *)node)->x;
			return GF_OK;
		case 49:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_animationElement *)node)->y;
			return GF_OK;
		case 50:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_animationElement *)node)->width;
			return GF_OK;
		case 51:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_animationElement *)node)->height;
			return GF_OK;
		case 52:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVG_SANI_animationElement *)node)->preserveAspectRatio;
			return GF_OK;
		case 53:
			info->name = "initialVisibility";
			info->fieldType = SVG_InitialVisibility_datatype;
			info->far_ptr = & ((SVG_SANI_animationElement *)node)->initialVisibility;
			return GF_OK;
		case 54:
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_animationElement *)node)->audio_level;
			return GF_OK;
		case 55:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_animationElement *)node)->display;
			return GF_OK;
		case 56:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_animationElement *)node)->visibility;
			return GF_OK;
		case 57:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_animationElement *)node)->viewport_fill;
			return GF_OK;
		case 58:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_animationElement *)node)->viewport_fill_opacity;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_audio()
{
	SVG_SANI_audioElement *p;
	GF_SAFEALLOC(p, SVG_SANI_audioElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_audio);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "audio";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_audio_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_audio_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	gf_svg_sani_init_timing((SVG_SANI_Element *)p);
	gf_svg_sani_init_sync((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	p->audio_level.type = SVG_NUMBER_VALUE;
	p->audio_level.value = FIX_ONE;
	p->display = SVG_DISPLAY_INLINE;
	p->timing->dur.type = SMIL_DURATION_MEDIA;
	return p;
}

static void gf_svg_sani_audio_del(GF_Node *node)
{
	SVG_SANI_audioElement *p = (SVG_SANI_audioElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	free(p->type);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_audio_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "clipBegin";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->clipBegin;
			return GF_OK;
		case 24:
			info->name = "clipEnd";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->clipEnd;
			return GF_OK;
		case 25:
			info->name = "syncBehavior";
			info->fieldType = SMIL_SyncBehavior_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->sync->syncBehavior;
			return GF_OK;
		case 26:
			info->name = "syncTolerance";
			info->fieldType = SMIL_SyncTolerance_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->sync->syncTolerance;
			return GF_OK;
		case 27:
			info->name = "syncMaster";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->sync->syncMaster;
			return GF_OK;
		case 28:
			info->name = "syncReference";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->sync->syncReference;
			return GF_OK;
		case 29:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 30:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 31:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 32:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 33:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 34:
			info->name = "type";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVG_SANI_audioElement *)node)->type;
			return GF_OK;
		case 35:
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_audioElement *)node)->audio_level;
			return GF_OK;
		case 36:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_audioElement *)node)->display;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_circle()
{
	SVG_SANI_circleElement *p;
	GF_SAFEALLOC(p, SVG_SANI_circleElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_circle);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "circle";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_circle_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_circle_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	p->fill_opacity.type = SVG_NUMBER_VALUE;
	p->fill_opacity.value = FIX_ONE;
	p->stroke_opacity.type = SVG_NUMBER_VALUE;
	p->stroke_opacity.value = FIX_ONE;
	p->fill.type = SVG_PAINT_COLOR;
	p->fill.color.type = SVG_COLOR_RGBCOLOR;
	p->fill_rule = SVG_FILLRULE_NONZERO;
	p->stroke.type = SVG_PAINT_NONE;
	p->stroke.color.type = SVG_COLOR_RGBCOLOR;
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_NONE;
	p->stroke_dashoffset.type = SVG_NUMBER_VALUE;
	p->stroke_linecap = SVG_STROKELINECAP_BUTT;
	p->stroke_linejoin = SVG_STROKELINEJOIN_MITER;
	p->stroke_miterlimit.type = SVG_NUMBER_VALUE;
	p->stroke_miterlimit.value = 4*FIX_ONE;
	p->stroke_width.type = SVG_NUMBER_VALUE;
	p->stroke_width.value = FIX_ONE;
	p->vector_effect = SVG_VECTOREFFECT_NONE;
	return p;
}

static void gf_svg_sani_circle_del(GF_Node *node)
{
	SVG_SANI_circleElement *p = (SVG_SANI_circleElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_circle_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 26:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->display;
			return GF_OK;
		case 27:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->visibility;
			return GF_OK;
		case 28:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->pointer_events;
			return GF_OK;
		case 29:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->fill_opacity;
			return GF_OK;
		case 30:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->stroke_opacity;
			return GF_OK;
		case 31:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->fill;
			return GF_OK;
		case 32:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->fill_rule;
			return GF_OK;
		case 33:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->stroke;
			return GF_OK;
		case 34:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->stroke_dasharray;
			return GF_OK;
		case 35:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 36:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->stroke_linecap;
			return GF_OK;
		case 37:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->stroke_linejoin;
			return GF_OK;
		case 38:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 39:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->stroke_width;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->vector_effect;
			return GF_OK;
		case 41:
			info->name = "cx";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->cx;
			return GF_OK;
		case 42:
			info->name = "cy";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->cy;
			return GF_OK;
		case 43:
			info->name = "r";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_circleElement *)node)->r;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_conditional()
{
	SVG_SANI_conditionalElement *p;
	GF_SAFEALLOC(p, SVG_SANI_conditionalElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_conditional);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "conditional";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_conditional_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_conditional_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_timing((SVG_SANI_Element *)p);
	gf_svg_sa_init_lsr_conditional(&p->updates);
	gf_svg_sani_init_timing((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_conditional_del(GF_Node *node)
{
	SVG_SANI_conditionalElement *p = (SVG_SANI_conditionalElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_svg_sa_reset_lsr_conditional(&p->updates);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_conditional_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->begin;
			return GF_OK;
		case 8:
			info->name = "enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVG_SANI_conditionalElement *)node)->enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_cursorManager()
{
	SVG_SANI_cursorManagerElement *p;
	GF_SAFEALLOC(p, SVG_SANI_cursorManagerElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_cursorManager);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "cursorManager";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_cursorManager_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_cursorManager_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_cursorManager_del(GF_Node *node)
{
	SVG_SANI_cursorManagerElement *p = (SVG_SANI_cursorManagerElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_cursorManager_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "x";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_cursorManagerElement *)node)->x;
			return GF_OK;
		case 15:
			info->name = "y";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_cursorManagerElement *)node)->y;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_defs()
{
	SVG_SANI_defsElement *p;
	GF_SAFEALLOC(p, SVG_SANI_defsElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_defs);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "defs";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_defs_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_defs_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_defs_del(GF_Node *node)
{
	SVG_SANI_defsElement *p = (SVG_SANI_defsElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_defs_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_desc()
{
	SVG_SANI_descElement *p;
	GF_SAFEALLOC(p, SVG_SANI_descElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_desc);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "desc";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_desc_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_desc_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_desc_del(GF_Node *node)
{
	SVG_SANI_descElement *p = (SVG_SANI_descElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_desc_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_discard()
{
	SVG_SANI_discardElement *p;
	GF_SAFEALLOC(p, SVG_SANI_discardElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_discard);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "discard";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_discard_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_discard_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	gf_svg_sani_init_timing((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_discard_del(GF_Node *node)
{
	SVG_SANI_discardElement *p = (SVG_SANI_discardElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_discard_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 16:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 17:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 18:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 19:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_ellipse()
{
	SVG_SANI_ellipseElement *p;
	GF_SAFEALLOC(p, SVG_SANI_ellipseElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_ellipse);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "ellipse";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_ellipse_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_ellipse_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	p->fill_opacity.type = SVG_NUMBER_VALUE;
	p->fill_opacity.value = FIX_ONE;
	p->stroke_opacity.type = SVG_NUMBER_VALUE;
	p->stroke_opacity.value = FIX_ONE;
	p->fill.type = SVG_PAINT_COLOR;
	p->fill.color.type = SVG_COLOR_RGBCOLOR;
	p->fill_rule = SVG_FILLRULE_NONZERO;
	p->stroke.type = SVG_PAINT_NONE;
	p->stroke.color.type = SVG_COLOR_RGBCOLOR;
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_NONE;
	p->stroke_dashoffset.type = SVG_NUMBER_VALUE;
	p->stroke_linecap = SVG_STROKELINECAP_BUTT;
	p->stroke_linejoin = SVG_STROKELINEJOIN_MITER;
	p->stroke_miterlimit.type = SVG_NUMBER_VALUE;
	p->stroke_miterlimit.value = 4*FIX_ONE;
	p->stroke_width.type = SVG_NUMBER_VALUE;
	p->stroke_width.value = FIX_ONE;
	p->vector_effect = SVG_VECTOREFFECT_NONE;
	return p;
}

static void gf_svg_sani_ellipse_del(GF_Node *node)
{
	SVG_SANI_ellipseElement *p = (SVG_SANI_ellipseElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_ellipse_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 26:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->display;
			return GF_OK;
		case 27:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->visibility;
			return GF_OK;
		case 28:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->pointer_events;
			return GF_OK;
		case 29:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->fill_opacity;
			return GF_OK;
		case 30:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->stroke_opacity;
			return GF_OK;
		case 31:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->fill;
			return GF_OK;
		case 32:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->fill_rule;
			return GF_OK;
		case 33:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->stroke;
			return GF_OK;
		case 34:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->stroke_dasharray;
			return GF_OK;
		case 35:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 36:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->stroke_linecap;
			return GF_OK;
		case 37:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->stroke_linejoin;
			return GF_OK;
		case 38:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 39:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->stroke_width;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->vector_effect;
			return GF_OK;
		case 41:
			info->name = "rx";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->rx;
			return GF_OK;
		case 42:
			info->name = "ry";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->ry;
			return GF_OK;
		case 43:
			info->name = "cx";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->cx;
			return GF_OK;
		case 44:
			info->name = "cy";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_ellipseElement *)node)->cy;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_font()
{
	SVG_SANI_fontElement *p;
	GF_SAFEALLOC(p, SVG_SANI_fontElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_font);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "font";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_font_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_font_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_font_del(GF_Node *node)
{
	SVG_SANI_fontElement *p = (SVG_SANI_fontElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_font_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "horiz-adv-x";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_fontElement *)node)->horiz_adv_x;
			return GF_OK;
		case 8:
			info->name = "horiz-origin-x";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_fontElement *)node)->horiz_origin_x;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_font_face()
{
	SVG_SANI_font_faceElement *p;
	GF_SAFEALLOC(p, SVG_SANI_font_faceElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_font_face);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "font_face";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_font_face_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_font_face_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	p->font_family.type = SVG_FONTFAMILY_VALUE;
	p->font_family.value = strdup("Arial");
	p->font_style = SVG_FONTSTYLE_NORMAL;
	p->font_weight = SVG_FONTWEIGHT_NORMAL;
	p->font_variant = SVG_FONTVARIANT_NORMAL;
	return p;
}

static void gf_svg_sani_font_face_del(GF_Node *node)
{
	SVG_SANI_font_faceElement *p = (SVG_SANI_font_faceElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->font_family.value) free(p->font_family.value);
	free(p->font_stretch);
	free(p->unicode_range);
	free(p->panose_1);
	free(p->widths);
	free(p->bbox);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_font_face_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->font_family;
			return GF_OK;
		case 8:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->font_style;
			return GF_OK;
		case 9:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->font_weight;
			return GF_OK;
		case 10:
			info->name = "font-variant";
			info->fieldType = SVG_FontVariant_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->font_variant;
			return GF_OK;
		case 11:
			info->name = "font-stretch";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->font_stretch;
			return GF_OK;
		case 12:
			info->name = "unicode-range";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->unicode_range;
			return GF_OK;
		case 13:
			info->name = "panose-1";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->panose_1;
			return GF_OK;
		case 14:
			info->name = "widths";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->widths;
			return GF_OK;
		case 15:
			info->name = "bbox";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->bbox;
			return GF_OK;
		case 16:
			info->name = "units-per-em";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->units_per_em;
			return GF_OK;
		case 17:
			info->name = "stemv";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->stemv;
			return GF_OK;
		case 18:
			info->name = "stemh";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->stemh;
			return GF_OK;
		case 19:
			info->name = "slope";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->slope;
			return GF_OK;
		case 20:
			info->name = "cap-height";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->cap_height;
			return GF_OK;
		case 21:
			info->name = "x-height";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->x_height;
			return GF_OK;
		case 22:
			info->name = "accent-height";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->accent_height;
			return GF_OK;
		case 23:
			info->name = "ascent";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->ascent;
			return GF_OK;
		case 24:
			info->name = "descent";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->descent;
			return GF_OK;
		case 25:
			info->name = "ideographic";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->ideographic;
			return GF_OK;
		case 26:
			info->name = "alphabetic";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->alphabetic;
			return GF_OK;
		case 27:
			info->name = "mathematical";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->mathematical;
			return GF_OK;
		case 28:
			info->name = "hanging";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->hanging;
			return GF_OK;
		case 29:
			info->name = "underline-position";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->underline_position;
			return GF_OK;
		case 30:
			info->name = "underline-thickness";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->underline_thickness;
			return GF_OK;
		case 31:
			info->name = "strikethrough-position";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->strikethrough_position;
			return GF_OK;
		case 32:
			info->name = "strikethrough-thickness";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->strikethrough_thickness;
			return GF_OK;
		case 33:
			info->name = "overline-position";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->overline_position;
			return GF_OK;
		case 34:
			info->name = "overline-thickness";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_font_faceElement *)node)->overline_thickness;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_font_face_src()
{
	SVG_SANI_font_face_srcElement *p;
	GF_SAFEALLOC(p, SVG_SANI_font_face_srcElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_font_face_src);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "font_face_src";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_font_face_src_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_font_face_src_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_font_face_src_del(GF_Node *node)
{
	SVG_SANI_font_face_srcElement *p = (SVG_SANI_font_face_srcElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_font_face_src_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_font_face_uri()
{
	SVG_SANI_font_face_uriElement *p;
	GF_SAFEALLOC(p, SVG_SANI_font_face_uriElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_font_face_uri);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "font_face_uri";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_font_face_uri_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_font_face_uri_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_font_face_uri_del(GF_Node *node)
{
	SVG_SANI_font_face_uriElement *p = (SVG_SANI_font_face_uriElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_font_face_uri_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_foreignObject()
{
	SVG_SANI_foreignObjectElement *p;
	GF_SAFEALLOC(p, SVG_SANI_foreignObjectElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_foreignObject);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "foreignObject";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_foreignObject_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_foreignObject_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	return p;
}

static void gf_svg_sani_foreignObject_del(GF_Node *node)
{
	SVG_SANI_foreignObjectElement *p = (SVG_SANI_foreignObjectElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_foreignObject_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 20:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 21:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 22:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 23:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 24:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 25:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 26:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 27:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 28:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 29:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 30:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 31:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 32:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 33:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_foreignObjectElement *)node)->x;
			return GF_OK;
		case 34:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_foreignObjectElement *)node)->y;
			return GF_OK;
		case 35:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_foreignObjectElement *)node)->width;
			return GF_OK;
		case 36:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_foreignObjectElement *)node)->height;
			return GF_OK;
		case 37:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_foreignObjectElement *)node)->display;
			return GF_OK;
		case 38:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_foreignObjectElement *)node)->visibility;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_g()
{
	SVG_SANI_gElement *p;
	GF_SAFEALLOC(p, SVG_SANI_gElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_g);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "g";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_g_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_g_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	return p;
}

static void gf_svg_sani_g_del(GF_Node *node)
{
	SVG_SANI_gElement *p = (SVG_SANI_gElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_g_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 26:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_gElement *)node)->display;
			return GF_OK;
		case 27:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_gElement *)node)->visibility;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_glyph()
{
	SVG_SANI_glyphElement *p;
	GF_SAFEALLOC(p, SVG_SANI_glyphElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_glyph);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "glyph";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_glyph_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_glyph_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
#ifdef USE_GF_PATH
	gf_path_reset(&p->d);
#else
	p->d.commands = gf_list_new();
	p->d.points = gf_list_new();
#endif
	return p;
}

static void gf_svg_sani_glyph_del(GF_Node *node)
{
	SVG_SANI_glyphElement *p = (SVG_SANI_glyphElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_svg_reset_path(p->d);
	free(p->unicode);
	free(p->glyph_name);
	free(p->arabic_form);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_glyph_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "horiz-adv-x";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_glyphElement *)node)->horiz_adv_x;
			return GF_OK;
		case 8:
			info->name = "d";
			info->fieldType = SVG_PathData_datatype;
			info->far_ptr = & ((SVG_SANI_glyphElement *)node)->d;
			return GF_OK;
		case 9:
			info->name = "unicode";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_glyphElement *)node)->unicode;
			return GF_OK;
		case 10:
			info->name = "glyph-name";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_glyphElement *)node)->glyph_name;
			return GF_OK;
		case 11:
			info->name = "arabic-form";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_glyphElement *)node)->arabic_form;
			return GF_OK;
		case 12:
			info->name = "lang";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVG_SANI_glyphElement *)node)->lang;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_handler()
{
	SVG_SANI_handlerElement *p;
	GF_SAFEALLOC(p, SVG_SANI_handlerElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_handler);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "handler";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_handler_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_handler_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_handler_del(GF_Node *node)
{
	SVG_SANI_handlerElement *p = (SVG_SANI_handlerElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	free(p->type);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_handler_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "type";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVG_SANI_handlerElement *)node)->type;
			return GF_OK;
		case 8:
			info->name = "ev:event";
			info->fieldType = XMLEV_Event_datatype;
			info->far_ptr = & ((SVG_SANI_handlerElement *)node)->ev_event;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_hkern()
{
	SVG_SANI_hkernElement *p;
	GF_SAFEALLOC(p, SVG_SANI_hkernElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_hkern);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "hkern";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_hkern_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_hkern_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_hkern_del(GF_Node *node)
{
	SVG_SANI_hkernElement *p = (SVG_SANI_hkernElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	free(p->u1);
	free(p->g1);
	free(p->u2);
	free(p->g2);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_hkern_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "u1";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_hkernElement *)node)->u1;
			return GF_OK;
		case 8:
			info->name = "g1";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_hkernElement *)node)->g1;
			return GF_OK;
		case 9:
			info->name = "u2";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_hkernElement *)node)->u2;
			return GF_OK;
		case 10:
			info->name = "g2";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_hkernElement *)node)->g2;
			return GF_OK;
		case 11:
			info->name = "k";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_hkernElement *)node)->k;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_image()
{
	SVG_SANI_imageElement *p;
	GF_SAFEALLOC(p, SVG_SANI_imageElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_image);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "image";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_image_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_image_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	return p;
}

static void gf_svg_sani_image_del(GF_Node *node)
{
	SVG_SANI_imageElement *p = (SVG_SANI_imageElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	free(p->type);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_image_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 20:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 21:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 22:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 23:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 24:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 25:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 26:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 27:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 28:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 29:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 30:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 31:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 32:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 33:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_imageElement *)node)->x;
			return GF_OK;
		case 34:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_imageElement *)node)->y;
			return GF_OK;
		case 35:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_imageElement *)node)->width;
			return GF_OK;
		case 36:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_imageElement *)node)->height;
			return GF_OK;
		case 37:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVG_SANI_imageElement *)node)->preserveAspectRatio;
			return GF_OK;
		case 38:
			info->name = "type";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVG_SANI_imageElement *)node)->type;
			return GF_OK;
		case 39:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_imageElement *)node)->display;
			return GF_OK;
		case 40:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_imageElement *)node)->visibility;
			return GF_OK;
		case 41:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = & ((SVG_SANI_imageElement *)node)->image_rendering;
			return GF_OK;
		case 42:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = & ((SVG_SANI_imageElement *)node)->pointer_events;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_line()
{
	SVG_SANI_lineElement *p;
	GF_SAFEALLOC(p, SVG_SANI_lineElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_line);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "line";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_line_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_line_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	p->fill_opacity.type = SVG_NUMBER_VALUE;
	p->fill_opacity.value = FIX_ONE;
	p->stroke_opacity.type = SVG_NUMBER_VALUE;
	p->stroke_opacity.value = FIX_ONE;
	p->fill.type = SVG_PAINT_COLOR;
	p->fill.color.type = SVG_COLOR_RGBCOLOR;
	p->fill_rule = SVG_FILLRULE_NONZERO;
	p->stroke.type = SVG_PAINT_NONE;
	p->stroke.color.type = SVG_COLOR_RGBCOLOR;
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_NONE;
	p->stroke_dashoffset.type = SVG_NUMBER_VALUE;
	p->stroke_linecap = SVG_STROKELINECAP_BUTT;
	p->stroke_linejoin = SVG_STROKELINEJOIN_MITER;
	p->stroke_miterlimit.type = SVG_NUMBER_VALUE;
	p->stroke_miterlimit.value = 4*FIX_ONE;
	p->stroke_width.type = SVG_NUMBER_VALUE;
	p->stroke_width.value = FIX_ONE;
	p->vector_effect = SVG_VECTOREFFECT_NONE;
	return p;
}

static void gf_svg_sani_line_del(GF_Node *node)
{
	SVG_SANI_lineElement *p = (SVG_SANI_lineElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_line_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 26:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->display;
			return GF_OK;
		case 27:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->visibility;
			return GF_OK;
		case 28:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->pointer_events;
			return GF_OK;
		case 29:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->fill_opacity;
			return GF_OK;
		case 30:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->stroke_opacity;
			return GF_OK;
		case 31:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->fill;
			return GF_OK;
		case 32:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->fill_rule;
			return GF_OK;
		case 33:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->stroke;
			return GF_OK;
		case 34:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->stroke_dasharray;
			return GF_OK;
		case 35:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 36:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->stroke_linecap;
			return GF_OK;
		case 37:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->stroke_linejoin;
			return GF_OK;
		case 38:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 39:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->stroke_width;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->vector_effect;
			return GF_OK;
		case 41:
			info->name = "x1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->x1;
			return GF_OK;
		case 42:
			info->name = "y1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->y1;
			return GF_OK;
		case 43:
			info->name = "x2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->x2;
			return GF_OK;
		case 44:
			info->name = "y2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_lineElement *)node)->y2;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_linearGradient()
{
	SVG_SANI_linearGradientElement *p;
	GF_SAFEALLOC(p, SVG_SANI_linearGradientElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_linearGradient);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "linearGradient";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_linearGradient_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_linearGradient_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	p->x2.value = FIX_ONE;
	gf_mx2d_init(p->gradientTransform.mat);
	return p;
}

static void gf_svg_sani_linearGradient_del(GF_Node *node)
{
	SVG_SANI_linearGradientElement *p = (SVG_SANI_linearGradientElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_linearGradient_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "gradientUnits";
			info->fieldType = SVG_GradientUnit_datatype;
			info->far_ptr = & ((SVG_SANI_linearGradientElement *)node)->gradientUnits;
			return GF_OK;
		case 15:
			info->name = "spreadMethod";
			info->fieldType = SVG_SpreadMethod_datatype;
			info->far_ptr = & ((SVG_SANI_linearGradientElement *)node)->spreadMethod;
			return GF_OK;
		case 16:
			info->name = "gradientTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = & ((SVG_SANI_linearGradientElement *)node)->gradientTransform;
			return GF_OK;
		case 17:
			info->name = "x1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_linearGradientElement *)node)->x1;
			return GF_OK;
		case 18:
			info->name = "y1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_linearGradientElement *)node)->y1;
			return GF_OK;
		case 19:
			info->name = "x2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_linearGradientElement *)node)->x2;
			return GF_OK;
		case 20:
			info->name = "y2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_linearGradientElement *)node)->y2;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_listener()
{
	SVG_SANI_listenerElement *p;
	GF_SAFEALLOC(p, SVG_SANI_listenerElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_listener);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "listener";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_listener_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_listener_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_listener_del(GF_Node *node)
{
	SVG_SANI_listenerElement *p = (SVG_SANI_listenerElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_svg_reset_iri(node->sgprivate->scenegraph, &p->observer);
	gf_svg_reset_iri(node->sgprivate->scenegraph, &p->target);
	gf_svg_reset_iri(node->sgprivate->scenegraph, &p->handler);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_listener_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "event";
			info->fieldType = XMLEV_Event_datatype;
			info->far_ptr = & ((SVG_SANI_listenerElement *)node)->event;
			return GF_OK;
		case 8:
			info->name = "phase";
			info->fieldType = XMLEV_Phase_datatype;
			info->far_ptr = & ((SVG_SANI_listenerElement *)node)->phase;
			return GF_OK;
		case 9:
			info->name = "propagate";
			info->fieldType = XMLEV_Propagate_datatype;
			info->far_ptr = & ((SVG_SANI_listenerElement *)node)->propagate;
			return GF_OK;
		case 10:
			info->name = "defaultAction";
			info->fieldType = XMLEV_DefaultAction_datatype;
			info->far_ptr = & ((SVG_SANI_listenerElement *)node)->defaultAction;
			return GF_OK;
		case 11:
			info->name = "observer";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVG_SANI_listenerElement *)node)->observer;
			return GF_OK;
		case 12:
			info->name = "target";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVG_SANI_listenerElement *)node)->target;
			return GF_OK;
		case 13:
			info->name = "handler";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVG_SANI_listenerElement *)node)->handler;
			return GF_OK;
		case 14:
			info->name = "enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVG_SANI_listenerElement *)node)->enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_metadata()
{
	SVG_SANI_metadataElement *p;
	GF_SAFEALLOC(p, SVG_SANI_metadataElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_metadata);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "metadata";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_metadata_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_metadata_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_metadata_del(GF_Node *node)
{
	SVG_SANI_metadataElement *p = (SVG_SANI_metadataElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_metadata_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_missing_glyph()
{
	SVG_SANI_missing_glyphElement *p;
	GF_SAFEALLOC(p, SVG_SANI_missing_glyphElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_missing_glyph);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "missing_glyph";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_missing_glyph_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_missing_glyph_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
#ifdef USE_GF_PATH
	gf_path_reset(&p->d);
#else
	p->d.commands = gf_list_new();
	p->d.points = gf_list_new();
#endif
	return p;
}

static void gf_svg_sani_missing_glyph_del(GF_Node *node)
{
	SVG_SANI_missing_glyphElement *p = (SVG_SANI_missing_glyphElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_svg_reset_path(p->d);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_missing_glyph_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "horiz-adv-x";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_missing_glyphElement *)node)->horiz_adv_x;
			return GF_OK;
		case 8:
			info->name = "d";
			info->fieldType = SVG_PathData_datatype;
			info->far_ptr = & ((SVG_SANI_missing_glyphElement *)node)->d;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_mpath()
{
	SVG_SANI_mpathElement *p;
	GF_SAFEALLOC(p, SVG_SANI_mpathElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_mpath);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "mpath";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_mpath_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_mpath_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_mpath_del(GF_Node *node)
{
	SVG_SANI_mpathElement *p = (SVG_SANI_mpathElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_mpath_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_path()
{
	SVG_SANI_pathElement *p;
	GF_SAFEALLOC(p, SVG_SANI_pathElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_path);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "path";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_path_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_path_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	p->fill_opacity.type = SVG_NUMBER_VALUE;
	p->fill_opacity.value = FIX_ONE;
	p->stroke_opacity.type = SVG_NUMBER_VALUE;
	p->stroke_opacity.value = FIX_ONE;
	p->fill.type = SVG_PAINT_COLOR;
	p->fill.color.type = SVG_COLOR_RGBCOLOR;
	p->fill_rule = SVG_FILLRULE_NONZERO;
	p->stroke.type = SVG_PAINT_NONE;
	p->stroke.color.type = SVG_COLOR_RGBCOLOR;
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_NONE;
	p->stroke_dashoffset.type = SVG_NUMBER_VALUE;
	p->stroke_linecap = SVG_STROKELINECAP_BUTT;
	p->stroke_linejoin = SVG_STROKELINEJOIN_MITER;
	p->stroke_miterlimit.type = SVG_NUMBER_VALUE;
	p->stroke_miterlimit.value = 4*FIX_ONE;
	p->stroke_width.type = SVG_NUMBER_VALUE;
	p->stroke_width.value = FIX_ONE;
	p->vector_effect = SVG_VECTOREFFECT_NONE;
#ifdef USE_GF_PATH
	gf_path_reset(&p->d);
#else
	p->d.commands = gf_list_new();
	p->d.points = gf_list_new();
#endif
	return p;
}

static void gf_svg_sani_path_del(GF_Node *node)
{
	SVG_SANI_pathElement *p = (SVG_SANI_pathElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_svg_reset_path(p->d);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_path_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 26:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->display;
			return GF_OK;
		case 27:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->visibility;
			return GF_OK;
		case 28:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->pointer_events;
			return GF_OK;
		case 29:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->fill_opacity;
			return GF_OK;
		case 30:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->stroke_opacity;
			return GF_OK;
		case 31:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->fill;
			return GF_OK;
		case 32:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->fill_rule;
			return GF_OK;
		case 33:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->stroke;
			return GF_OK;
		case 34:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->stroke_dasharray;
			return GF_OK;
		case 35:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 36:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->stroke_linecap;
			return GF_OK;
		case 37:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->stroke_linejoin;
			return GF_OK;
		case 38:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 39:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->stroke_width;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->vector_effect;
			return GF_OK;
		case 41:
			info->name = "d";
			info->fieldType = SVG_PathData_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->d;
			return GF_OK;
		case 42:
			info->name = "pathLength";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_pathElement *)node)->pathLength;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_polygon()
{
	SVG_SANI_polygonElement *p;
	GF_SAFEALLOC(p, SVG_SANI_polygonElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_polygon);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "polygon";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_polygon_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_polygon_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	p->fill_opacity.type = SVG_NUMBER_VALUE;
	p->fill_opacity.value = FIX_ONE;
	p->stroke_opacity.type = SVG_NUMBER_VALUE;
	p->stroke_opacity.value = FIX_ONE;
	p->fill.type = SVG_PAINT_COLOR;
	p->fill.color.type = SVG_COLOR_RGBCOLOR;
	p->fill_rule = SVG_FILLRULE_NONZERO;
	p->stroke.type = SVG_PAINT_NONE;
	p->stroke.color.type = SVG_COLOR_RGBCOLOR;
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_NONE;
	p->stroke_dashoffset.type = SVG_NUMBER_VALUE;
	p->stroke_linecap = SVG_STROKELINECAP_BUTT;
	p->stroke_linejoin = SVG_STROKELINEJOIN_MITER;
	p->stroke_miterlimit.type = SVG_NUMBER_VALUE;
	p->stroke_miterlimit.value = 4*FIX_ONE;
	p->stroke_width.type = SVG_NUMBER_VALUE;
	p->stroke_width.value = FIX_ONE;
	p->vector_effect = SVG_VECTOREFFECT_NONE;
	p->points = gf_list_new();
	return p;
}

static void gf_svg_sani_polygon_del(GF_Node *node)
{
	SVG_SANI_polygonElement *p = (SVG_SANI_polygonElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_svg_delete_points(p->points);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_polygon_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 26:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->display;
			return GF_OK;
		case 27:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->visibility;
			return GF_OK;
		case 28:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->pointer_events;
			return GF_OK;
		case 29:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->fill_opacity;
			return GF_OK;
		case 30:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->stroke_opacity;
			return GF_OK;
		case 31:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->fill;
			return GF_OK;
		case 32:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->fill_rule;
			return GF_OK;
		case 33:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->stroke;
			return GF_OK;
		case 34:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->stroke_dasharray;
			return GF_OK;
		case 35:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 36:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->stroke_linecap;
			return GF_OK;
		case 37:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->stroke_linejoin;
			return GF_OK;
		case 38:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 39:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->stroke_width;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->vector_effect;
			return GF_OK;
		case 41:
			info->name = "points";
			info->fieldType = SVG_Points_datatype;
			info->far_ptr = & ((SVG_SANI_polygonElement *)node)->points;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_polyline()
{
	SVG_SANI_polylineElement *p;
	GF_SAFEALLOC(p, SVG_SANI_polylineElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_polyline);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "polyline";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_polyline_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_polyline_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	p->fill_opacity.type = SVG_NUMBER_VALUE;
	p->fill_opacity.value = FIX_ONE;
	p->stroke_opacity.type = SVG_NUMBER_VALUE;
	p->stroke_opacity.value = FIX_ONE;
	p->fill.type = SVG_PAINT_COLOR;
	p->fill.color.type = SVG_COLOR_RGBCOLOR;
	p->fill_rule = SVG_FILLRULE_NONZERO;
	p->stroke.type = SVG_PAINT_NONE;
	p->stroke.color.type = SVG_COLOR_RGBCOLOR;
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_NONE;
	p->stroke_dashoffset.type = SVG_NUMBER_VALUE;
	p->stroke_linecap = SVG_STROKELINECAP_BUTT;
	p->stroke_linejoin = SVG_STROKELINEJOIN_MITER;
	p->stroke_miterlimit.type = SVG_NUMBER_VALUE;
	p->stroke_miterlimit.value = 4*FIX_ONE;
	p->stroke_width.type = SVG_NUMBER_VALUE;
	p->stroke_width.value = FIX_ONE;
	p->vector_effect = SVG_VECTOREFFECT_NONE;
	p->points = gf_list_new();
	return p;
}

static void gf_svg_sani_polyline_del(GF_Node *node)
{
	SVG_SANI_polylineElement *p = (SVG_SANI_polylineElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_svg_delete_points(p->points);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_polyline_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 26:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->display;
			return GF_OK;
		case 27:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->visibility;
			return GF_OK;
		case 28:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->pointer_events;
			return GF_OK;
		case 29:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->fill_opacity;
			return GF_OK;
		case 30:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->stroke_opacity;
			return GF_OK;
		case 31:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->fill;
			return GF_OK;
		case 32:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->fill_rule;
			return GF_OK;
		case 33:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->stroke;
			return GF_OK;
		case 34:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->stroke_dasharray;
			return GF_OK;
		case 35:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 36:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->stroke_linecap;
			return GF_OK;
		case 37:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->stroke_linejoin;
			return GF_OK;
		case 38:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 39:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->stroke_width;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->vector_effect;
			return GF_OK;
		case 41:
			info->name = "points";
			info->fieldType = SVG_Points_datatype;
			info->far_ptr = & ((SVG_SANI_polylineElement *)node)->points;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_prefetch()
{
	SVG_SANI_prefetchElement *p;
	GF_SAFEALLOC(p, SVG_SANI_prefetchElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_prefetch);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "prefetch";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_prefetch_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_prefetch_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_prefetch_del(GF_Node *node)
{
	SVG_SANI_prefetchElement *p = (SVG_SANI_prefetchElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	free(p->mediaTime);
	free(p->mediaCharacterEncoding);
	free(p->mediaContentEncodings);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_prefetch_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "mediaSize";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_prefetchElement *)node)->mediaSize;
			return GF_OK;
		case 15:
			info->name = "mediaTime";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_prefetchElement *)node)->mediaTime;
			return GF_OK;
		case 16:
			info->name = "mediaCharacterEncoding";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_prefetchElement *)node)->mediaCharacterEncoding;
			return GF_OK;
		case 17:
			info->name = "mediaContentEncodings";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_prefetchElement *)node)->mediaContentEncodings;
			return GF_OK;
		case 18:
			info->name = "bandwidth";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_prefetchElement *)node)->bandwidth;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_radialGradient()
{
	SVG_SANI_radialGradientElement *p;
	GF_SAFEALLOC(p, SVG_SANI_radialGradientElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_radialGradient);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "radialGradient";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_radialGradient_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_radialGradient_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	p->cx.value = FIX_ONE/2;
	p->cy.value = FIX_ONE/2;
	p->r.value = FIX_ONE/2;
	gf_mx2d_init(p->gradientTransform.mat);
	p->fx.value = FIX_ONE/2;
	p->fy.value = FIX_ONE/2;
	return p;
}

static void gf_svg_sani_radialGradient_del(GF_Node *node)
{
	SVG_SANI_radialGradientElement *p = (SVG_SANI_radialGradientElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_radialGradient_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "gradientUnits";
			info->fieldType = SVG_GradientUnit_datatype;
			info->far_ptr = & ((SVG_SANI_radialGradientElement *)node)->gradientUnits;
			return GF_OK;
		case 15:
			info->name = "spreadMethod";
			info->fieldType = SVG_SpreadMethod_datatype;
			info->far_ptr = & ((SVG_SANI_radialGradientElement *)node)->spreadMethod;
			return GF_OK;
		case 16:
			info->name = "gradientTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = & ((SVG_SANI_radialGradientElement *)node)->gradientTransform;
			return GF_OK;
		case 17:
			info->name = "cx";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_radialGradientElement *)node)->cx;
			return GF_OK;
		case 18:
			info->name = "cy";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_radialGradientElement *)node)->cy;
			return GF_OK;
		case 19:
			info->name = "r";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_radialGradientElement *)node)->r;
			return GF_OK;
		case 20:
			info->name = "fx";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_radialGradientElement *)node)->fx;
			return GF_OK;
		case 21:
			info->name = "fy";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_radialGradientElement *)node)->fy;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_rect()
{
	SVG_SANI_rectElement *p;
	GF_SAFEALLOC(p, SVG_SANI_rectElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_rect);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "rect";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_rect_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_rect_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	p->fill_opacity.type = SVG_NUMBER_VALUE;
	p->fill_opacity.value = FIX_ONE;
	p->stroke_opacity.type = SVG_NUMBER_VALUE;
	p->stroke_opacity.value = FIX_ONE;
	p->fill.type = SVG_PAINT_COLOR;
	p->fill.color.type = SVG_COLOR_RGBCOLOR;
	p->fill_rule = SVG_FILLRULE_NONZERO;
	p->stroke.type = SVG_PAINT_NONE;
	p->stroke.color.type = SVG_COLOR_RGBCOLOR;
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_NONE;
	p->stroke_dashoffset.type = SVG_NUMBER_VALUE;
	p->stroke_linecap = SVG_STROKELINECAP_BUTT;
	p->stroke_linejoin = SVG_STROKELINEJOIN_MITER;
	p->stroke_miterlimit.type = SVG_NUMBER_VALUE;
	p->stroke_miterlimit.value = 4*FIX_ONE;
	p->stroke_width.type = SVG_NUMBER_VALUE;
	p->stroke_width.value = FIX_ONE;
	p->vector_effect = SVG_VECTOREFFECT_NONE;
	return p;
}

static void gf_svg_sani_rect_del(GF_Node *node)
{
	SVG_SANI_rectElement *p = (SVG_SANI_rectElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_rect_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 26:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->display;
			return GF_OK;
		case 27:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->visibility;
			return GF_OK;
		case 28:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->pointer_events;
			return GF_OK;
		case 29:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->fill_opacity;
			return GF_OK;
		case 30:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->stroke_opacity;
			return GF_OK;
		case 31:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->fill;
			return GF_OK;
		case 32:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->fill_rule;
			return GF_OK;
		case 33:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->stroke;
			return GF_OK;
		case 34:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->stroke_dasharray;
			return GF_OK;
		case 35:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 36:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->stroke_linecap;
			return GF_OK;
		case 37:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->stroke_linejoin;
			return GF_OK;
		case 38:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 39:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->stroke_width;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->vector_effect;
			return GF_OK;
		case 41:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->x;
			return GF_OK;
		case 42:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->y;
			return GF_OK;
		case 43:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->width;
			return GF_OK;
		case 44:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->height;
			return GF_OK;
		case 45:
			info->name = "rx";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->rx;
			return GF_OK;
		case 46:
			info->name = "ry";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_rectElement *)node)->ry;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_rectClip()
{
	SVG_SANI_rectClipElement *p;
	GF_SAFEALLOC(p, SVG_SANI_rectClipElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_rectClip);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "rectClip";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_rectClip_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_rectClip_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sani_rectClip_del(GF_Node *node)
{
	SVG_SANI_rectClipElement *p = (SVG_SANI_rectClipElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_rectClip_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 26:
			info->name = "size";
			info->fieldType = LASeR_Size_datatype;
			info->far_ptr = & ((SVG_SANI_rectClipElement *)node)->size;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_script()
{
	SVG_SANI_scriptElement *p;
	GF_SAFEALLOC(p, SVG_SANI_scriptElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_script);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "script";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_script_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_script_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_script_del(GF_Node *node)
{
	SVG_SANI_scriptElement *p = (SVG_SANI_scriptElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	free(p->type);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_script_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "type";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVG_SANI_scriptElement *)node)->type;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_selector()
{
	SVG_SANI_selectorElement *p;
	GF_SAFEALLOC(p, SVG_SANI_selectorElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_selector);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "selector";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_selector_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_selector_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	return p;
}

static void gf_svg_sani_selector_del(GF_Node *node)
{
	SVG_SANI_selectorElement *p = (SVG_SANI_selectorElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_selector_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 26:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_selectorElement *)node)->display;
			return GF_OK;
		case 27:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_selectorElement *)node)->visibility;
			return GF_OK;
		case 28:
			info->name = "choice";
			info->fieldType = LASeR_Choice_datatype;
			info->far_ptr = & ((SVG_SANI_selectorElement *)node)->choice;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_set()
{
	SVG_SANI_setElement *p;
	GF_SAFEALLOC(p, SVG_SANI_setElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_set);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "set";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_set_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_set_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	gf_svg_sani_init_timing((SVG_SANI_Element *)p);
	gf_svg_sani_init_anim((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_set_del(GF_Node *node)
{
	SVG_SANI_setElement *p = (SVG_SANI_setElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_set_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->attributeName;
			return GF_OK;
		case 24:
			info->name = "attributeType";
			info->fieldType = SMIL_AttributeType_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->attributeType;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->to;
			return GF_OK;
		case 26:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->anim->lsr_enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_simpleLayout()
{
	SVG_SANI_simpleLayoutElement *p;
	GF_SAFEALLOC(p, SVG_SANI_simpleLayoutElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_simpleLayout);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "simpleLayout";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_simpleLayout_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_simpleLayout_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	return p;
}

static void gf_svg_sani_simpleLayout_del(GF_Node *node)
{
	SVG_SANI_simpleLayoutElement *p = (SVG_SANI_simpleLayoutElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_simpleLayout_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 26:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_simpleLayoutElement *)node)->display;
			return GF_OK;
		case 27:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_simpleLayoutElement *)node)->visibility;
			return GF_OK;
		case 28:
			info->name = "delta";
			info->fieldType = LASeR_Size_datatype;
			info->far_ptr = & ((SVG_SANI_simpleLayoutElement *)node)->delta;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_solidColor()
{
	SVG_SANI_solidColorElement *p;
	GF_SAFEALLOC(p, SVG_SANI_solidColorElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_solidColor);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "solidColor";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_solidColor_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_solidColor_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	p->solid_color.type = SVG_PAINT_COLOR;
	p->solid_color.color.type = SVG_COLOR_RGBCOLOR;
	p->solid_opacity.type = SVG_NUMBER_VALUE;
	p->solid_opacity.value = FIX_ONE;
	return p;
}

static void gf_svg_sani_solidColor_del(GF_Node *node)
{
	SVG_SANI_solidColorElement *p = (SVG_SANI_solidColorElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_solidColor_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVG_SANI_solidColorElement *)node)->solid_color;
			return GF_OK;
		case 8:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_solidColorElement *)node)->solid_opacity;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_stop()
{
	SVG_SANI_stopElement *p;
	GF_SAFEALLOC(p, SVG_SANI_stopElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_stop);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "stop";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_stop_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_stop_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	p->stop_opacity.type = SVG_NUMBER_VALUE;
	p->stop_opacity.value = FIX_ONE;
	return p;
}

static void gf_svg_sani_stop_del(GF_Node *node)
{
	SVG_SANI_stopElement *p = (SVG_SANI_stopElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_stop_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = & ((SVG_SANI_stopElement *)node)->color_rendering;
			return GF_OK;
		case 8:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVG_SANI_stopElement *)node)->stop_color;
			return GF_OK;
		case 9:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_stopElement *)node)->stop_opacity;
			return GF_OK;
		case 10:
			info->name = "offset";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_stopElement *)node)->offset;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_svg()
{
	SVG_SANI_svgElement *p;
	GF_SAFEALLOC(p, SVG_SANI_svgElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_svg);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "svg";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_svg_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_svg_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_sync((SVG_SANI_Element *)p);
	p->viewport_fill.type = SVG_PAINT_NONE;
	p->viewport_fill_opacity.type = SVG_NUMBER_VALUE;
	p->viewport_fill_opacity.value = FIX_ONE;
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	p->width.type = SVG_NUMBER_PERCENTAGE;
	p->width.value = INT2FIX(100);
	p->height.type = SVG_NUMBER_PERCENTAGE;
	p->height.value = INT2FIX(100);
	return p;
}

static void gf_svg_sani_svg_del(GF_Node *node)
{
	SVG_SANI_svgElement *p = (SVG_SANI_svgElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	free(p->version);
	free(p->baseProfile);
	free(p->contentScriptType);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_svg_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "syncBehaviorDefault";
			info->fieldType = SMIL_SyncBehavior_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->sync->syncBehaviorDefault;
			return GF_OK;
		case 20:
			info->name = "syncToleranceDefault";
			info->fieldType = SMIL_SyncTolerance_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->sync->syncToleranceDefault;
			return GF_OK;
		case 21:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->x;
			return GF_OK;
		case 22:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->y;
			return GF_OK;
		case 23:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->width;
			return GF_OK;
		case 24:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->height;
			return GF_OK;
		case 25:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->preserveAspectRatio;
			return GF_OK;
		case 26:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->viewport_fill;
			return GF_OK;
		case 27:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 28:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->display;
			return GF_OK;
		case 29:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->visibility;
			return GF_OK;
		case 30:
			info->name = "viewBox";
			info->fieldType = SVG_ViewBox_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->viewBox;
			return GF_OK;
		case 31:
			info->name = "zoomAndPan";
			info->fieldType = SVG_ZoomAndPan_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->zoomAndPan;
			return GF_OK;
		case 32:
			info->name = "version";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->version;
			return GF_OK;
		case 33:
			info->name = "baseProfile";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->baseProfile;
			return GF_OK;
		case 34:
			info->name = "contentScriptType";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->contentScriptType;
			return GF_OK;
		case 35:
			info->name = "snapshotTime";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->snapshotTime;
			return GF_OK;
		case 36:
			info->name = "timelineBegin";
			info->fieldType = SVG_TimelineBegin_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->timelineBegin;
			return GF_OK;
		case 37:
			info->name = "playbackOrder";
			info->fieldType = SVG_PlaybackOrder_datatype;
			info->far_ptr = & ((SVG_SANI_svgElement *)node)->playbackOrder;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_switch()
{
	SVG_SANI_switchElement *p;
	GF_SAFEALLOC(p, SVG_SANI_switchElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_switch);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "switch";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_switch_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_switch_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	return p;
}

static void gf_svg_sani_switch_del(GF_Node *node)
{
	SVG_SANI_switchElement *p = (SVG_SANI_switchElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_switch_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 26:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_switchElement *)node)->display;
			return GF_OK;
		case 27:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_switchElement *)node)->visibility;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_tbreak()
{
	SVG_SANI_tbreakElement *p;
	GF_SAFEALLOC(p, SVG_SANI_tbreakElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_tbreak);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "tbreak";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_tbreak_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_tbreak_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_tbreak_del(GF_Node *node)
{
	SVG_SANI_tbreakElement *p = (SVG_SANI_tbreakElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_tbreak_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_text()
{
	SVG_SANI_textElement *p;
	GF_SAFEALLOC(p, SVG_SANI_textElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_text);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "text";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_text_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_text_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	p->font_family.type = SVG_FONTFAMILY_VALUE;
	p->font_family.value = strdup("Arial");
	p->font_size.type = SVG_NUMBER_VALUE;
	p->font_size.value = 12*FIX_ONE;
	p->font_style = SVG_FONTSTYLE_NORMAL;
	p->font_variant = SVG_FONTVARIANT_NORMAL;
	p->font_weight = SVG_FONTWEIGHT_NORMAL;
	p->text_anchor = SVG_TEXTANCHOR_START;
	p->text_align = SVG_TEXTALIGN_START;
	p->vector_effect = SVG_VECTOREFFECT_NONE;
	p->fill_opacity.type = SVG_NUMBER_VALUE;
	p->fill_opacity.value = FIX_ONE;
	p->stroke_opacity.type = SVG_NUMBER_VALUE;
	p->stroke_opacity.value = FIX_ONE;
	p->fill.type = SVG_PAINT_COLOR;
	p->fill.color.type = SVG_COLOR_RGBCOLOR;
	p->fill_rule = SVG_FILLRULE_NONZERO;
	p->stroke.type = SVG_PAINT_NONE;
	p->stroke.color.type = SVG_COLOR_RGBCOLOR;
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_NONE;
	p->stroke_dashoffset.type = SVG_NUMBER_VALUE;
	p->stroke_linecap = SVG_STROKELINECAP_BUTT;
	p->stroke_linejoin = SVG_STROKELINEJOIN_MITER;
	p->stroke_miterlimit.type = SVG_NUMBER_VALUE;
	p->stroke_miterlimit.value = 4*FIX_ONE;
	p->stroke_width.type = SVG_NUMBER_VALUE;
	p->stroke_width.value = FIX_ONE;
	p->x = gf_list_new();
	p->y = gf_list_new();
	return p;
}

static void gf_svg_sani_text_del(GF_Node *node)
{
	SVG_SANI_textElement *p = (SVG_SANI_textElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->font_family.value) free(p->font_family.value);
	gf_svg_delete_coordinates(p->x);
	gf_svg_delete_coordinates(p->y);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_text_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 26:
			info->name = "editable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->editable;
			return GF_OK;
		case 27:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->display;
			return GF_OK;
		case 28:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->visibility;
			return GF_OK;
		case 29:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->font_family;
			return GF_OK;
		case 30:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->font_size;
			return GF_OK;
		case 31:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->font_style;
			return GF_OK;
		case 32:
			info->name = "font-variant";
			info->fieldType = SVG_FontVariant_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->font_variant;
			return GF_OK;
		case 33:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->font_weight;
			return GF_OK;
		case 34:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->text_anchor;
			return GF_OK;
		case 35:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->text_align;
			return GF_OK;
		case 36:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->vector_effect;
			return GF_OK;
		case 37:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->pointer_events;
			return GF_OK;
		case 38:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->fill_opacity;
			return GF_OK;
		case 39:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->stroke_opacity;
			return GF_OK;
		case 40:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->fill;
			return GF_OK;
		case 41:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->fill_rule;
			return GF_OK;
		case 42:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->stroke;
			return GF_OK;
		case 43:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->stroke_dasharray;
			return GF_OK;
		case 44:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 45:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->stroke_linecap;
			return GF_OK;
		case 46:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->stroke_linejoin;
			return GF_OK;
		case 47:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 48:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->stroke_width;
			return GF_OK;
		case 49:
			info->name = "x";
			info->fieldType = SVG_Coordinates_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->x;
			return GF_OK;
		case 50:
			info->name = "y";
			info->fieldType = SVG_Coordinates_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->y;
			return GF_OK;
		case 51:
			info->name = "rotate";
			info->fieldType = SVG_Numbers_datatype;
			info->far_ptr = & ((SVG_SANI_textElement *)node)->rotate;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_textArea()
{
	SVG_SANI_textAreaElement *p;
	GF_SAFEALLOC(p, SVG_SANI_textAreaElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_textArea);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "textArea";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_textArea_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_textArea_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	p->fill_opacity.type = SVG_NUMBER_VALUE;
	p->fill_opacity.value = FIX_ONE;
	p->stroke_opacity.type = SVG_NUMBER_VALUE;
	p->stroke_opacity.value = FIX_ONE;
	p->fill.type = SVG_PAINT_COLOR;
	p->fill.color.type = SVG_COLOR_RGBCOLOR;
	p->fill_rule = SVG_FILLRULE_NONZERO;
	p->stroke.type = SVG_PAINT_NONE;
	p->stroke.color.type = SVG_COLOR_RGBCOLOR;
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_NONE;
	p->stroke_dashoffset.type = SVG_NUMBER_VALUE;
	p->stroke_linecap = SVG_STROKELINECAP_BUTT;
	p->stroke_linejoin = SVG_STROKELINEJOIN_MITER;
	p->stroke_miterlimit.type = SVG_NUMBER_VALUE;
	p->stroke_miterlimit.value = 4*FIX_ONE;
	p->stroke_width.type = SVG_NUMBER_VALUE;
	p->stroke_width.value = FIX_ONE;
	p->font_family.type = SVG_FONTFAMILY_VALUE;
	p->font_family.value = strdup("Arial");
	p->font_size.type = SVG_NUMBER_VALUE;
	p->font_size.value = 12*FIX_ONE;
	p->font_style = SVG_FONTSTYLE_NORMAL;
	p->font_variant = SVG_FONTVARIANT_NORMAL;
	p->font_weight = SVG_FONTWEIGHT_NORMAL;
	p->text_anchor = SVG_TEXTANCHOR_START;
	p->text_align = SVG_TEXTALIGN_START;
	p->display_align = SVG_DISPLAYALIGN_AUTO;
	p->line_increment.type = SVG_NUMBER_AUTO;
	p->line_increment.value = FIX_ONE;
	p->vector_effect = SVG_VECTOREFFECT_NONE;
	return p;
}

static void gf_svg_sani_textArea_del(GF_Node *node)
{
	SVG_SANI_textAreaElement *p = (SVG_SANI_textAreaElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->font_family.value) free(p->font_family.value);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_textArea_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 26:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->x;
			return GF_OK;
		case 27:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->y;
			return GF_OK;
		case 28:
			info->name = "editable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->editable;
			return GF_OK;
		case 29:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->display;
			return GF_OK;
		case 30:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->visibility;
			return GF_OK;
		case 31:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->pointer_events;
			return GF_OK;
		case 32:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->text_rendering;
			return GF_OK;
		case 33:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->fill_opacity;
			return GF_OK;
		case 34:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->stroke_opacity;
			return GF_OK;
		case 35:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->fill;
			return GF_OK;
		case 36:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->fill_rule;
			return GF_OK;
		case 37:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->stroke;
			return GF_OK;
		case 38:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->stroke_dasharray;
			return GF_OK;
		case 39:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 40:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->stroke_linecap;
			return GF_OK;
		case 41:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->stroke_linejoin;
			return GF_OK;
		case 42:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 43:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->stroke_width;
			return GF_OK;
		case 44:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->color_rendering;
			return GF_OK;
		case 45:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->font_family;
			return GF_OK;
		case 46:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->font_size;
			return GF_OK;
		case 47:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->font_style;
			return GF_OK;
		case 48:
			info->name = "font-variant";
			info->fieldType = SVG_FontVariant_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->font_variant;
			return GF_OK;
		case 49:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->font_weight;
			return GF_OK;
		case 50:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->text_anchor;
			return GF_OK;
		case 51:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->text_align;
			return GF_OK;
		case 52:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->display_align;
			return GF_OK;
		case 53:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->line_increment;
			return GF_OK;
		case 54:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->vector_effect;
			return GF_OK;
		case 55:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->width;
			return GF_OK;
		case 56:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_textAreaElement *)node)->height;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_title()
{
	SVG_SANI_titleElement *p;
	GF_SAFEALLOC(p, SVG_SANI_titleElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_title);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "title";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_title_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_title_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_title_del(GF_Node *node)
{
	SVG_SANI_titleElement *p = (SVG_SANI_titleElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_title_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_tspan()
{
	SVG_SANI_tspanElement *p;
	GF_SAFEALLOC(p, SVG_SANI_tspanElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_tspan);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "tspan";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_tspan_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_tspan_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	return p;
}

static void gf_svg_sani_tspan_del(GF_Node *node)
{
	SVG_SANI_tspanElement *p = (SVG_SANI_tspanElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_tspan_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 20:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 21:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 22:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 23:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_use()
{
	SVG_SANI_useElement *p;
	GF_SAFEALLOC(p, SVG_SANI_useElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_use);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "use";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_use_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_use_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	return p;
}

static void gf_svg_sani_use_del(GF_Node *node)
{
	SVG_SANI_useElement *p = (SVG_SANI_useElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_use_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 20:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 21:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 22:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 23:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 24:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 25:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 26:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 27:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 28:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 29:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 30:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 31:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 32:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 33:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_useElement *)node)->x;
			return GF_OK;
		case 34:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_useElement *)node)->y;
			return GF_OK;
		case 35:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_useElement *)node)->display;
			return GF_OK;
		case 36:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_useElement *)node)->visibility;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_sani_new_video()
{
	SVG_SANI_videoElement *p;
	GF_SAFEALLOC(p, SVG_SANI_videoElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SANI_video);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "video";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sani_video_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sani_video_get_attribute;
#endif
	gf_svg_sani_init_core((SVG_SANI_Element *)p);
	gf_svg_sani_init_focus((SVG_SANI_Element *)p);
	gf_svg_sani_init_xlink((SVG_SANI_Element *)p);
	gf_svg_sani_init_timing((SVG_SANI_Element *)p);
	gf_svg_sani_init_sync((SVG_SANI_Element *)p);
	gf_svg_sani_init_conditional((SVG_SANI_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->audio_level.type = SVG_NUMBER_VALUE;
	p->audio_level.value = FIX_ONE;
	p->display = SVG_DISPLAY_INLINE;
	p->visibility = SVG_VISIBILITY_VISIBLE;
	p->timing->dur.type = SMIL_DURATION_MEDIA;
	return p;
}

static void gf_svg_sani_video_del(GF_Node *node)
{
	SVG_SANI_videoElement *p = (SVG_SANI_videoElement *)node;
	gf_svg_sani_reset_base_element((SVG_SANI_Element *)p);
	free(p->type);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sani_video_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = gf_node_get_name_address(node);
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 8:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->focusable;
			return GF_OK;
		case 9:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down;
			return GF_OK;
		case 10:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 11:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 12:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_left;
			return GF_OK;
		case 13:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_next;
			return GF_OK;
		case 14:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 15:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_right;
			return GF_OK;
		case 16:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up;
			return GF_OK;
		case 17:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 18:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 19:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->href;
			return GF_OK;
		case 20:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->show;
			return GF_OK;
		case 21:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->title;
			return GF_OK;
		case 22:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->actuate;
			return GF_OK;
		case 23:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->role;
			return GF_OK;
		case 24:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 25:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->xlink->type;
			return GF_OK;
		case 26:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->begin;
			return GF_OK;
		case 27:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->end;
			return GF_OK;
		case 28:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->dur;
			return GF_OK;
		case 29:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 30:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 31:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->restart;
			return GF_OK;
		case 32:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->min;
			return GF_OK;
		case 33:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->max;
			return GF_OK;
		case 34:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->fill;
			return GF_OK;
		case 35:
			info->name = "clipBegin";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->clipBegin;
			return GF_OK;
		case 36:
			info->name = "clipEnd";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->timing->clipEnd;
			return GF_OK;
		case 37:
			info->name = "syncBehavior";
			info->fieldType = SMIL_SyncBehavior_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->sync->syncBehavior;
			return GF_OK;
		case 38:
			info->name = "syncTolerance";
			info->fieldType = SMIL_SyncTolerance_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->sync->syncTolerance;
			return GF_OK;
		case 39:
			info->name = "syncMaster";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->sync->syncMaster;
			return GF_OK;
		case 40:
			info->name = "syncReference";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->sync->syncReference;
			return GF_OK;
		case 41:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 42:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 43:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 44:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 45:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SANI_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 46:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;
			return GF_OK;
		case 47:
			info->name = "motionTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;
			return GF_OK;
		case 48:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_videoElement *)node)->x;
			return GF_OK;
		case 49:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SANI_videoElement *)node)->y;
			return GF_OK;
		case 50:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_videoElement *)node)->width;
			return GF_OK;
		case 51:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SANI_videoElement *)node)->height;
			return GF_OK;
		case 52:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVG_SANI_videoElement *)node)->preserveAspectRatio;
			return GF_OK;
		case 53:
			info->name = "type";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVG_SANI_videoElement *)node)->type;
			return GF_OK;
		case 54:
			info->name = "initialVisibility";
			info->fieldType = SVG_InitialVisibility_datatype;
			info->far_ptr = & ((SVG_SANI_videoElement *)node)->initialVisibility;
			return GF_OK;
		case 55:
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SANI_videoElement *)node)->audio_level;
			return GF_OK;
		case 56:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVG_SANI_videoElement *)node)->display;
			return GF_OK;
		case 57:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVG_SANI_videoElement *)node)->visibility;
			return GF_OK;
		case 58:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = & ((SVG_SANI_videoElement *)node)->pointer_events;
			return GF_OK;
		case 59:
			info->name = "transformBehavior";
			info->fieldType = SVG_TransformBehavior_datatype;
			info->far_ptr = & ((SVG_SANI_videoElement *)node)->transformBehavior;
			return GF_OK;
		case 60:
			info->name = "overlay";
			info->fieldType = SVG_Overlay_datatype;
			info->far_ptr = & ((SVG_SANI_videoElement *)node)->overlay;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

SVG_SANI_Element *gf_svg_sani_create_node(u32 ElementTag)
{
	switch (ElementTag) {
		case TAG_SVG_SANI_a: return (SVG_SANI_Element*) gf_svg_sani_new_a();
		case TAG_SVG_SANI_animate: return (SVG_SANI_Element*) gf_svg_sani_new_animate();
		case TAG_SVG_SANI_animateColor: return (SVG_SANI_Element*) gf_svg_sani_new_animateColor();
		case TAG_SVG_SANI_animateMotion: return (SVG_SANI_Element*) gf_svg_sani_new_animateMotion();
		case TAG_SVG_SANI_animateTransform: return (SVG_SANI_Element*) gf_svg_sani_new_animateTransform();
		case TAG_SVG_SANI_animation: return (SVG_SANI_Element*) gf_svg_sani_new_animation();
		case TAG_SVG_SANI_audio: return (SVG_SANI_Element*) gf_svg_sani_new_audio();
		case TAG_SVG_SANI_circle: return (SVG_SANI_Element*) gf_svg_sani_new_circle();
		case TAG_SVG_SANI_conditional: return (SVG_SANI_Element*) gf_svg_sani_new_conditional();
		case TAG_SVG_SANI_cursorManager: return (SVG_SANI_Element*) gf_svg_sani_new_cursorManager();
		case TAG_SVG_SANI_defs: return (SVG_SANI_Element*) gf_svg_sani_new_defs();
		case TAG_SVG_SANI_desc: return (SVG_SANI_Element*) gf_svg_sani_new_desc();
		case TAG_SVG_SANI_discard: return (SVG_SANI_Element*) gf_svg_sani_new_discard();
		case TAG_SVG_SANI_ellipse: return (SVG_SANI_Element*) gf_svg_sani_new_ellipse();
		case TAG_SVG_SANI_font: return (SVG_SANI_Element*) gf_svg_sani_new_font();
		case TAG_SVG_SANI_font_face: return (SVG_SANI_Element*) gf_svg_sani_new_font_face();
		case TAG_SVG_SANI_font_face_src: return (SVG_SANI_Element*) gf_svg_sani_new_font_face_src();
		case TAG_SVG_SANI_font_face_uri: return (SVG_SANI_Element*) gf_svg_sani_new_font_face_uri();
		case TAG_SVG_SANI_foreignObject: return (SVG_SANI_Element*) gf_svg_sani_new_foreignObject();
		case TAG_SVG_SANI_g: return (SVG_SANI_Element*) gf_svg_sani_new_g();
		case TAG_SVG_SANI_glyph: return (SVG_SANI_Element*) gf_svg_sani_new_glyph();
		case TAG_SVG_SANI_handler: return (SVG_SANI_Element*) gf_svg_sani_new_handler();
		case TAG_SVG_SANI_hkern: return (SVG_SANI_Element*) gf_svg_sani_new_hkern();
		case TAG_SVG_SANI_image: return (SVG_SANI_Element*) gf_svg_sani_new_image();
		case TAG_SVG_SANI_line: return (SVG_SANI_Element*) gf_svg_sani_new_line();
		case TAG_SVG_SANI_linearGradient: return (SVG_SANI_Element*) gf_svg_sani_new_linearGradient();
		case TAG_SVG_SANI_listener: return (SVG_SANI_Element*) gf_svg_sani_new_listener();
		case TAG_SVG_SANI_metadata: return (SVG_SANI_Element*) gf_svg_sani_new_metadata();
		case TAG_SVG_SANI_missing_glyph: return (SVG_SANI_Element*) gf_svg_sani_new_missing_glyph();
		case TAG_SVG_SANI_mpath: return (SVG_SANI_Element*) gf_svg_sani_new_mpath();
		case TAG_SVG_SANI_path: return (SVG_SANI_Element*) gf_svg_sani_new_path();
		case TAG_SVG_SANI_polygon: return (SVG_SANI_Element*) gf_svg_sani_new_polygon();
		case TAG_SVG_SANI_polyline: return (SVG_SANI_Element*) gf_svg_sani_new_polyline();
		case TAG_SVG_SANI_prefetch: return (SVG_SANI_Element*) gf_svg_sani_new_prefetch();
		case TAG_SVG_SANI_radialGradient: return (SVG_SANI_Element*) gf_svg_sani_new_radialGradient();
		case TAG_SVG_SANI_rect: return (SVG_SANI_Element*) gf_svg_sani_new_rect();
		case TAG_SVG_SANI_rectClip: return (SVG_SANI_Element*) gf_svg_sani_new_rectClip();
		case TAG_SVG_SANI_script: return (SVG_SANI_Element*) gf_svg_sani_new_script();
		case TAG_SVG_SANI_selector: return (SVG_SANI_Element*) gf_svg_sani_new_selector();
		case TAG_SVG_SANI_set: return (SVG_SANI_Element*) gf_svg_sani_new_set();
		case TAG_SVG_SANI_simpleLayout: return (SVG_SANI_Element*) gf_svg_sani_new_simpleLayout();
		case TAG_SVG_SANI_solidColor: return (SVG_SANI_Element*) gf_svg_sani_new_solidColor();
		case TAG_SVG_SANI_stop: return (SVG_SANI_Element*) gf_svg_sani_new_stop();
		case TAG_SVG_SANI_svg: return (SVG_SANI_Element*) gf_svg_sani_new_svg();
		case TAG_SVG_SANI_switch: return (SVG_SANI_Element*) gf_svg_sani_new_switch();
		case TAG_SVG_SANI_tbreak: return (SVG_SANI_Element*) gf_svg_sani_new_tbreak();
		case TAG_SVG_SANI_text: return (SVG_SANI_Element*) gf_svg_sani_new_text();
		case TAG_SVG_SANI_textArea: return (SVG_SANI_Element*) gf_svg_sani_new_textArea();
		case TAG_SVG_SANI_title: return (SVG_SANI_Element*) gf_svg_sani_new_title();
		case TAG_SVG_SANI_tspan: return (SVG_SANI_Element*) gf_svg_sani_new_tspan();
		case TAG_SVG_SANI_use: return (SVG_SANI_Element*) gf_svg_sani_new_use();
		case TAG_SVG_SANI_video: return (SVG_SANI_Element*) gf_svg_sani_new_video();
		default: return NULL;
	}
}

void gf_svg_sani_element_del(SVG_SANI_Element *elt)
{
	GF_Node *node = (GF_Node *)elt;
	switch (node->sgprivate->tag) {
		case TAG_SVG_SANI_a: gf_svg_sani_a_del(node); return;
		case TAG_SVG_SANI_animate: gf_svg_sani_animate_del(node); return;
		case TAG_SVG_SANI_animateColor: gf_svg_sani_animateColor_del(node); return;
		case TAG_SVG_SANI_animateMotion: gf_svg_sani_animateMotion_del(node); return;
		case TAG_SVG_SANI_animateTransform: gf_svg_sani_animateTransform_del(node); return;
		case TAG_SVG_SANI_animation: gf_svg_sani_animation_del(node); return;
		case TAG_SVG_SANI_audio: gf_svg_sani_audio_del(node); return;
		case TAG_SVG_SANI_circle: gf_svg_sani_circle_del(node); return;
		case TAG_SVG_SANI_conditional: gf_svg_sani_conditional_del(node); return;
		case TAG_SVG_SANI_cursorManager: gf_svg_sani_cursorManager_del(node); return;
		case TAG_SVG_SANI_defs: gf_svg_sani_defs_del(node); return;
		case TAG_SVG_SANI_desc: gf_svg_sani_desc_del(node); return;
		case TAG_SVG_SANI_discard: gf_svg_sani_discard_del(node); return;
		case TAG_SVG_SANI_ellipse: gf_svg_sani_ellipse_del(node); return;
		case TAG_SVG_SANI_font: gf_svg_sani_font_del(node); return;
		case TAG_SVG_SANI_font_face: gf_svg_sani_font_face_del(node); return;
		case TAG_SVG_SANI_font_face_src: gf_svg_sani_font_face_src_del(node); return;
		case TAG_SVG_SANI_font_face_uri: gf_svg_sani_font_face_uri_del(node); return;
		case TAG_SVG_SANI_foreignObject: gf_svg_sani_foreignObject_del(node); return;
		case TAG_SVG_SANI_g: gf_svg_sani_g_del(node); return;
		case TAG_SVG_SANI_glyph: gf_svg_sani_glyph_del(node); return;
		case TAG_SVG_SANI_handler: gf_svg_sani_handler_del(node); return;
		case TAG_SVG_SANI_hkern: gf_svg_sani_hkern_del(node); return;
		case TAG_SVG_SANI_image: gf_svg_sani_image_del(node); return;
		case TAG_SVG_SANI_line: gf_svg_sani_line_del(node); return;
		case TAG_SVG_SANI_linearGradient: gf_svg_sani_linearGradient_del(node); return;
		case TAG_SVG_SANI_listener: gf_svg_sani_listener_del(node); return;
		case TAG_SVG_SANI_metadata: gf_svg_sani_metadata_del(node); return;
		case TAG_SVG_SANI_missing_glyph: gf_svg_sani_missing_glyph_del(node); return;
		case TAG_SVG_SANI_mpath: gf_svg_sani_mpath_del(node); return;
		case TAG_SVG_SANI_path: gf_svg_sani_path_del(node); return;
		case TAG_SVG_SANI_polygon: gf_svg_sani_polygon_del(node); return;
		case TAG_SVG_SANI_polyline: gf_svg_sani_polyline_del(node); return;
		case TAG_SVG_SANI_prefetch: gf_svg_sani_prefetch_del(node); return;
		case TAG_SVG_SANI_radialGradient: gf_svg_sani_radialGradient_del(node); return;
		case TAG_SVG_SANI_rect: gf_svg_sani_rect_del(node); return;
		case TAG_SVG_SANI_rectClip: gf_svg_sani_rectClip_del(node); return;
		case TAG_SVG_SANI_script: gf_svg_sani_script_del(node); return;
		case TAG_SVG_SANI_selector: gf_svg_sani_selector_del(node); return;
		case TAG_SVG_SANI_set: gf_svg_sani_set_del(node); return;
		case TAG_SVG_SANI_simpleLayout: gf_svg_sani_simpleLayout_del(node); return;
		case TAG_SVG_SANI_solidColor: gf_svg_sani_solidColor_del(node); return;
		case TAG_SVG_SANI_stop: gf_svg_sani_stop_del(node); return;
		case TAG_SVG_SANI_svg: gf_svg_sani_svg_del(node); return;
		case TAG_SVG_SANI_switch: gf_svg_sani_switch_del(node); return;
		case TAG_SVG_SANI_tbreak: gf_svg_sani_tbreak_del(node); return;
		case TAG_SVG_SANI_text: gf_svg_sani_text_del(node); return;
		case TAG_SVG_SANI_textArea: gf_svg_sani_textArea_del(node); return;
		case TAG_SVG_SANI_title: gf_svg_sani_title_del(node); return;
		case TAG_SVG_SANI_tspan: gf_svg_sani_tspan_del(node); return;
		case TAG_SVG_SANI_use: gf_svg_sani_use_del(node); return;
		case TAG_SVG_SANI_video: gf_svg_sani_video_del(node); return;
		default: return;
	}
}

u32 gf_svg_sani_get_attribute_count(GF_Node *node)
{
	switch (node->sgprivate->tag) {
		case TAG_SVG_SANI_a: return 37;
		case TAG_SVG_SANI_animate: return 35;
		case TAG_SVG_SANI_animateColor: return 35;
		case TAG_SVG_SANI_animateMotion: return 37;
		case TAG_SVG_SANI_animateTransform: return 36;
		case TAG_SVG_SANI_animation: return 59;
		case TAG_SVG_SANI_audio: return 37;
		case TAG_SVG_SANI_circle: return 44;
		case TAG_SVG_SANI_conditional: return 9;
		case TAG_SVG_SANI_cursorManager: return 16;
		case TAG_SVG_SANI_defs: return 7;
		case TAG_SVG_SANI_desc: return 7;
		case TAG_SVG_SANI_discard: return 20;
		case TAG_SVG_SANI_ellipse: return 45;
		case TAG_SVG_SANI_font: return 9;
		case TAG_SVG_SANI_font_face: return 35;
		case TAG_SVG_SANI_font_face_src: return 7;
		case TAG_SVG_SANI_font_face_uri: return 14;
		case TAG_SVG_SANI_foreignObject: return 39;
		case TAG_SVG_SANI_g: return 28;
		case TAG_SVG_SANI_glyph: return 13;
		case TAG_SVG_SANI_handler: return 9;
		case TAG_SVG_SANI_hkern: return 12;
		case TAG_SVG_SANI_image: return 43;
		case TAG_SVG_SANI_line: return 45;
		case TAG_SVG_SANI_linearGradient: return 21;
		case TAG_SVG_SANI_listener: return 15;
		case TAG_SVG_SANI_metadata: return 7;
		case TAG_SVG_SANI_missing_glyph: return 9;
		case TAG_SVG_SANI_mpath: return 14;
		case TAG_SVG_SANI_path: return 43;
		case TAG_SVG_SANI_polygon: return 42;
		case TAG_SVG_SANI_polyline: return 42;
		case TAG_SVG_SANI_prefetch: return 19;
		case TAG_SVG_SANI_radialGradient: return 22;
		case TAG_SVG_SANI_rect: return 47;
		case TAG_SVG_SANI_rectClip: return 27;
		case TAG_SVG_SANI_script: return 8;
		case TAG_SVG_SANI_selector: return 29;
		case TAG_SVG_SANI_set: return 27;
		case TAG_SVG_SANI_simpleLayout: return 29;
		case TAG_SVG_SANI_solidColor: return 9;
		case TAG_SVG_SANI_stop: return 11;
		case TAG_SVG_SANI_svg: return 38;
		case TAG_SVG_SANI_switch: return 28;
		case TAG_SVG_SANI_tbreak: return 7;
		case TAG_SVG_SANI_text: return 52;
		case TAG_SVG_SANI_textArea: return 57;
		case TAG_SVG_SANI_title: return 7;
		case TAG_SVG_SANI_tspan: return 24;
		case TAG_SVG_SANI_use: return 37;
		case TAG_SVG_SANI_video: return 61;
		default: return 0;
	}
}

GF_Err gf_svg_sani_get_attribute_info(GF_Node *node, GF_FieldInfo *info)
{
	switch (node->sgprivate->tag) {
		case TAG_SVG_SANI_a: return gf_svg_sani_a_get_attribute(node, info);
		case TAG_SVG_SANI_animate: return gf_svg_sani_animate_get_attribute(node, info);
		case TAG_SVG_SANI_animateColor: return gf_svg_sani_animateColor_get_attribute(node, info);
		case TAG_SVG_SANI_animateMotion: return gf_svg_sani_animateMotion_get_attribute(node, info);
		case TAG_SVG_SANI_animateTransform: return gf_svg_sani_animateTransform_get_attribute(node, info);
		case TAG_SVG_SANI_animation: return gf_svg_sani_animation_get_attribute(node, info);
		case TAG_SVG_SANI_audio: return gf_svg_sani_audio_get_attribute(node, info);
		case TAG_SVG_SANI_circle: return gf_svg_sani_circle_get_attribute(node, info);
		case TAG_SVG_SANI_conditional: return gf_svg_sani_conditional_get_attribute(node, info);
		case TAG_SVG_SANI_cursorManager: return gf_svg_sani_cursorManager_get_attribute(node, info);
		case TAG_SVG_SANI_defs: return gf_svg_sani_defs_get_attribute(node, info);
		case TAG_SVG_SANI_desc: return gf_svg_sani_desc_get_attribute(node, info);
		case TAG_SVG_SANI_discard: return gf_svg_sani_discard_get_attribute(node, info);
		case TAG_SVG_SANI_ellipse: return gf_svg_sani_ellipse_get_attribute(node, info);
		case TAG_SVG_SANI_font: return gf_svg_sani_font_get_attribute(node, info);
		case TAG_SVG_SANI_font_face: return gf_svg_sani_font_face_get_attribute(node, info);
		case TAG_SVG_SANI_font_face_src: return gf_svg_sani_font_face_src_get_attribute(node, info);
		case TAG_SVG_SANI_font_face_uri: return gf_svg_sani_font_face_uri_get_attribute(node, info);
		case TAG_SVG_SANI_foreignObject: return gf_svg_sani_foreignObject_get_attribute(node, info);
		case TAG_SVG_SANI_g: return gf_svg_sani_g_get_attribute(node, info);
		case TAG_SVG_SANI_glyph: return gf_svg_sani_glyph_get_attribute(node, info);
		case TAG_SVG_SANI_handler: return gf_svg_sani_handler_get_attribute(node, info);
		case TAG_SVG_SANI_hkern: return gf_svg_sani_hkern_get_attribute(node, info);
		case TAG_SVG_SANI_image: return gf_svg_sani_image_get_attribute(node, info);
		case TAG_SVG_SANI_line: return gf_svg_sani_line_get_attribute(node, info);
		case TAG_SVG_SANI_linearGradient: return gf_svg_sani_linearGradient_get_attribute(node, info);
		case TAG_SVG_SANI_listener: return gf_svg_sani_listener_get_attribute(node, info);
		case TAG_SVG_SANI_metadata: return gf_svg_sani_metadata_get_attribute(node, info);
		case TAG_SVG_SANI_missing_glyph: return gf_svg_sani_missing_glyph_get_attribute(node, info);
		case TAG_SVG_SANI_mpath: return gf_svg_sani_mpath_get_attribute(node, info);
		case TAG_SVG_SANI_path: return gf_svg_sani_path_get_attribute(node, info);
		case TAG_SVG_SANI_polygon: return gf_svg_sani_polygon_get_attribute(node, info);
		case TAG_SVG_SANI_polyline: return gf_svg_sani_polyline_get_attribute(node, info);
		case TAG_SVG_SANI_prefetch: return gf_svg_sani_prefetch_get_attribute(node, info);
		case TAG_SVG_SANI_radialGradient: return gf_svg_sani_radialGradient_get_attribute(node, info);
		case TAG_SVG_SANI_rect: return gf_svg_sani_rect_get_attribute(node, info);
		case TAG_SVG_SANI_rectClip: return gf_svg_sani_rectClip_get_attribute(node, info);
		case TAG_SVG_SANI_script: return gf_svg_sani_script_get_attribute(node, info);
		case TAG_SVG_SANI_selector: return gf_svg_sani_selector_get_attribute(node, info);
		case TAG_SVG_SANI_set: return gf_svg_sani_set_get_attribute(node, info);
		case TAG_SVG_SANI_simpleLayout: return gf_svg_sani_simpleLayout_get_attribute(node, info);
		case TAG_SVG_SANI_solidColor: return gf_svg_sani_solidColor_get_attribute(node, info);
		case TAG_SVG_SANI_stop: return gf_svg_sani_stop_get_attribute(node, info);
		case TAG_SVG_SANI_svg: return gf_svg_sani_svg_get_attribute(node, info);
		case TAG_SVG_SANI_switch: return gf_svg_sani_switch_get_attribute(node, info);
		case TAG_SVG_SANI_tbreak: return gf_svg_sani_tbreak_get_attribute(node, info);
		case TAG_SVG_SANI_text: return gf_svg_sani_text_get_attribute(node, info);
		case TAG_SVG_SANI_textArea: return gf_svg_sani_textArea_get_attribute(node, info);
		case TAG_SVG_SANI_title: return gf_svg_sani_title_get_attribute(node, info);
		case TAG_SVG_SANI_tspan: return gf_svg_sani_tspan_get_attribute(node, info);
		case TAG_SVG_SANI_use: return gf_svg_sani_use_get_attribute(node, info);
		case TAG_SVG_SANI_video: return gf_svg_sani_video_get_attribute(node, info);
		default: return GF_BAD_PARAM;
	}
}

u32 gf_svg_sani_type_by_class_name(const char *element_name)
{
	if (!element_name) return TAG_UndefinedNode;
	if (!stricmp(element_name, "a")) return TAG_SVG_SANI_a;
	if (!stricmp(element_name, "animate")) return TAG_SVG_SANI_animate;
	if (!stricmp(element_name, "animateColor")) return TAG_SVG_SANI_animateColor;
	if (!stricmp(element_name, "animateMotion")) return TAG_SVG_SANI_animateMotion;
	if (!stricmp(element_name, "animateTransform")) return TAG_SVG_SANI_animateTransform;
	if (!stricmp(element_name, "animation")) return TAG_SVG_SANI_animation;
	if (!stricmp(element_name, "audio")) return TAG_SVG_SANI_audio;
	if (!stricmp(element_name, "circle")) return TAG_SVG_SANI_circle;
	if (!stricmp(element_name, "conditional")) return TAG_SVG_SANI_conditional;
	if (!stricmp(element_name, "cursorManager")) return TAG_SVG_SANI_cursorManager;
	if (!stricmp(element_name, "defs")) return TAG_SVG_SANI_defs;
	if (!stricmp(element_name, "desc")) return TAG_SVG_SANI_desc;
	if (!stricmp(element_name, "discard")) return TAG_SVG_SANI_discard;
	if (!stricmp(element_name, "ellipse")) return TAG_SVG_SANI_ellipse;
	if (!stricmp(element_name, "font")) return TAG_SVG_SANI_font;
	if (!stricmp(element_name, "font-face")) return TAG_SVG_SANI_font_face;
	if (!stricmp(element_name, "font-face-src")) return TAG_SVG_SANI_font_face_src;
	if (!stricmp(element_name, "font-face-uri")) return TAG_SVG_SANI_font_face_uri;
	if (!stricmp(element_name, "foreignObject")) return TAG_SVG_SANI_foreignObject;
	if (!stricmp(element_name, "g")) return TAG_SVG_SANI_g;
	if (!stricmp(element_name, "glyph")) return TAG_SVG_SANI_glyph;
	if (!stricmp(element_name, "handler")) return TAG_SVG_SANI_handler;
	if (!stricmp(element_name, "hkern")) return TAG_SVG_SANI_hkern;
	if (!stricmp(element_name, "image")) return TAG_SVG_SANI_image;
	if (!stricmp(element_name, "line")) return TAG_SVG_SANI_line;
	if (!stricmp(element_name, "linearGradient")) return TAG_SVG_SANI_linearGradient;
	if (!stricmp(element_name, "listener")) return TAG_SVG_SANI_listener;
	if (!stricmp(element_name, "metadata")) return TAG_SVG_SANI_metadata;
	if (!stricmp(element_name, "missing-glyph")) return TAG_SVG_SANI_missing_glyph;
	if (!stricmp(element_name, "mpath")) return TAG_SVG_SANI_mpath;
	if (!stricmp(element_name, "path")) return TAG_SVG_SANI_path;
	if (!stricmp(element_name, "polygon")) return TAG_SVG_SANI_polygon;
	if (!stricmp(element_name, "polyline")) return TAG_SVG_SANI_polyline;
	if (!stricmp(element_name, "prefetch")) return TAG_SVG_SANI_prefetch;
	if (!stricmp(element_name, "radialGradient")) return TAG_SVG_SANI_radialGradient;
	if (!stricmp(element_name, "rect")) return TAG_SVG_SANI_rect;
	if (!stricmp(element_name, "rectClip")) return TAG_SVG_SANI_rectClip;
	if (!stricmp(element_name, "script")) return TAG_SVG_SANI_script;
	if (!stricmp(element_name, "selector")) return TAG_SVG_SANI_selector;
	if (!stricmp(element_name, "set")) return TAG_SVG_SANI_set;
	if (!stricmp(element_name, "simpleLayout")) return TAG_SVG_SANI_simpleLayout;
	if (!stricmp(element_name, "solidColor")) return TAG_SVG_SANI_solidColor;
	if (!stricmp(element_name, "stop")) return TAG_SVG_SANI_stop;
	if (!stricmp(element_name, "svg")) return TAG_SVG_SANI_svg;
	if (!stricmp(element_name, "switch")) return TAG_SVG_SANI_switch;
	if (!stricmp(element_name, "tbreak")) return TAG_SVG_SANI_tbreak;
	if (!stricmp(element_name, "text")) return TAG_SVG_SANI_text;
	if (!stricmp(element_name, "textArea")) return TAG_SVG_SANI_textArea;
	if (!stricmp(element_name, "title")) return TAG_SVG_SANI_title;
	if (!stricmp(element_name, "tspan")) return TAG_SVG_SANI_tspan;
	if (!stricmp(element_name, "use")) return TAG_SVG_SANI_use;
	if (!stricmp(element_name, "video")) return TAG_SVG_SANI_video;
	return TAG_UndefinedNode;
}

const char *gf_svg_sani_get_element_name(u32 tag)
{
	switch(tag) {
	case TAG_SVG_SANI_a: return "a";
	case TAG_SVG_SANI_animate: return "animate";
	case TAG_SVG_SANI_animateColor: return "animateColor";
	case TAG_SVG_SANI_animateMotion: return "animateMotion";
	case TAG_SVG_SANI_animateTransform: return "animateTransform";
	case TAG_SVG_SANI_animation: return "animation";
	case TAG_SVG_SANI_audio: return "audio";
	case TAG_SVG_SANI_circle: return "circle";
	case TAG_SVG_SANI_conditional: return "conditional";
	case TAG_SVG_SANI_cursorManager: return "cursorManager";
	case TAG_SVG_SANI_defs: return "defs";
	case TAG_SVG_SANI_desc: return "desc";
	case TAG_SVG_SANI_discard: return "discard";
	case TAG_SVG_SANI_ellipse: return "ellipse";
	case TAG_SVG_SANI_font: return "font";
	case TAG_SVG_SANI_font_face: return "font-face";
	case TAG_SVG_SANI_font_face_src: return "font-face-src";
	case TAG_SVG_SANI_font_face_uri: return "font-face-uri";
	case TAG_SVG_SANI_foreignObject: return "foreignObject";
	case TAG_SVG_SANI_g: return "g";
	case TAG_SVG_SANI_glyph: return "glyph";
	case TAG_SVG_SANI_handler: return "handler";
	case TAG_SVG_SANI_hkern: return "hkern";
	case TAG_SVG_SANI_image: return "image";
	case TAG_SVG_SANI_line: return "line";
	case TAG_SVG_SANI_linearGradient: return "linearGradient";
	case TAG_SVG_SANI_listener: return "listener";
	case TAG_SVG_SANI_metadata: return "metadata";
	case TAG_SVG_SANI_missing_glyph: return "missing-glyph";
	case TAG_SVG_SANI_mpath: return "mpath";
	case TAG_SVG_SANI_path: return "path";
	case TAG_SVG_SANI_polygon: return "polygon";
	case TAG_SVG_SANI_polyline: return "polyline";
	case TAG_SVG_SANI_prefetch: return "prefetch";
	case TAG_SVG_SANI_radialGradient: return "radialGradient";
	case TAG_SVG_SANI_rect: return "rect";
	case TAG_SVG_SANI_rectClip: return "rectClip";
	case TAG_SVG_SANI_script: return "script";
	case TAG_SVG_SANI_selector: return "selector";
	case TAG_SVG_SANI_set: return "set";
	case TAG_SVG_SANI_simpleLayout: return "simpleLayout";
	case TAG_SVG_SANI_solidColor: return "solidColor";
	case TAG_SVG_SANI_stop: return "stop";
	case TAG_SVG_SANI_svg: return "svg";
	case TAG_SVG_SANI_switch: return "switch";
	case TAG_SVG_SANI_tbreak: return "tbreak";
	case TAG_SVG_SANI_text: return "text";
	case TAG_SVG_SANI_textArea: return "textArea";
	case TAG_SVG_SANI_title: return "title";
	case TAG_SVG_SANI_tspan: return "tspan";
	case TAG_SVG_SANI_use: return "use";
	case TAG_SVG_SANI_video: return "video";
	default: return "UndefinedNode";
	}
}

Bool gf_svg_sani_is_element_transformable(u32 tag)
{
	switch(tag) {
	case TAG_SVG_SANI_a:return 1;
	case TAG_SVG_SANI_animate:return 0;
	case TAG_SVG_SANI_animateColor:return 0;
	case TAG_SVG_SANI_animateMotion:return 0;
	case TAG_SVG_SANI_animateTransform:return 0;
	case TAG_SVG_SANI_animation:return 1;
	case TAG_SVG_SANI_audio:return 0;
	case TAG_SVG_SANI_circle:return 1;
	case TAG_SVG_SANI_conditional:return 0;
	case TAG_SVG_SANI_cursorManager:return 0;
	case TAG_SVG_SANI_defs:return 0;
	case TAG_SVG_SANI_desc:return 0;
	case TAG_SVG_SANI_discard:return 0;
	case TAG_SVG_SANI_ellipse:return 1;
	case TAG_SVG_SANI_font:return 0;
	case TAG_SVG_SANI_font_face:return 0;
	case TAG_SVG_SANI_font_face_src:return 0;
	case TAG_SVG_SANI_font_face_uri:return 0;
	case TAG_SVG_SANI_foreignObject:return 1;
	case TAG_SVG_SANI_g:return 1;
	case TAG_SVG_SANI_glyph:return 0;
	case TAG_SVG_SANI_handler:return 0;
	case TAG_SVG_SANI_hkern:return 0;
	case TAG_SVG_SANI_image:return 1;
	case TAG_SVG_SANI_line:return 1;
	case TAG_SVG_SANI_linearGradient:return 0;
	case TAG_SVG_SANI_listener:return 0;
	case TAG_SVG_SANI_metadata:return 0;
	case TAG_SVG_SANI_missing_glyph:return 0;
	case TAG_SVG_SANI_mpath:return 0;
	case TAG_SVG_SANI_path:return 1;
	case TAG_SVG_SANI_polygon:return 1;
	case TAG_SVG_SANI_polyline:return 1;
	case TAG_SVG_SANI_prefetch:return 0;
	case TAG_SVG_SANI_radialGradient:return 0;
	case TAG_SVG_SANI_rect:return 1;
	case TAG_SVG_SANI_rectClip:return 1;
	case TAG_SVG_SANI_script:return 0;
	case TAG_SVG_SANI_selector:return 1;
	case TAG_SVG_SANI_set:return 0;
	case TAG_SVG_SANI_simpleLayout:return 1;
	case TAG_SVG_SANI_solidColor:return 0;
	case TAG_SVG_SANI_stop:return 0;
	case TAG_SVG_SANI_svg:return 0;
	case TAG_SVG_SANI_switch:return 1;
	case TAG_SVG_SANI_tbreak:return 0;
	case TAG_SVG_SANI_text:return 1;
	case TAG_SVG_SANI_textArea:return 1;
	case TAG_SVG_SANI_title:return 0;
	case TAG_SVG_SANI_tspan:return 0;
	case TAG_SVG_SANI_use:return 1;
	case TAG_SVG_SANI_video:return 1;
	default: return 0;
	}
}

#endif	/*GPAC_ENABLE_SVG_SANI*/
#endif /*GPAC_DISABLE_SVG*/


