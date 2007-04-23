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


#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_svg_da.h>

Bool is_svg3_animation_tag(u32 tag)
{
	return (tag == TAG_SVG3_set ||
			tag == TAG_SVG3_animate ||
			tag == TAG_SVG3_animateColor ||
			tag == TAG_SVG3_animateTransform ||
			tag == TAG_SVG3_animateMotion || 
			tag == TAG_SVG3_discard)?1:0;
}

Bool is_svg3_timing_tag(u32 tag)
{
	if (is_svg3_animation_tag(tag)) return 1;
	else return (tag == TAG_SVG3_animation ||
			tag == TAG_SVG3_audio ||
			tag == TAG_SVG3_video)?1:0;
	/* Note: lsr:Conditional is also a timed element */
}

SVG3Element *gf_svg3_create_node(u32 ElementTag)
{
	SVG3Element *p;
	if (is_svg3_timing_tag(ElementTag)) {
		SVG3TimedAnimBaseElement *tap;
		GF_SAFEALLOC(tap, SVG3TimedAnimBaseElement); 
		p = (SVG3Element *)tap;
	} else if (ElementTag == TAG_SVG3_handler) {
		SVG3handlerElement *hdl;
		GF_SAFEALLOC(hdl, SVG3handlerElement); 
		p = (SVG3Element *)hdl;
	} else {
		GF_SAFEALLOC(p, SVG3Element);
	}
	gf_node_setup((GF_Node *)p, ElementTag);
	gf_sg_parent_setup((GF_Node *) p);
	return p;
}

static void gf_svg3_delete_attributes(GF_SceneGraph *sg, SVG3Attribute *attributes)
{
	u32 i = 0;
	SVG3Attribute *tmp;
	SVG3Attribute *att = attributes;
	while(att) {
		gf_svg_delete_attribute_value(att->data_type, att->data, sg);
		tmp = att;
		att = att->next;
		free(tmp);
	}
}

void gf_svg3_node_del(GF_Node *node)
{
	SVG3Element *p = (SVG3Element *)node;

	if (p->sgprivate->interact && p->sgprivate->interact->animations) {
		gf_smil_anim_delete_animations((GF_Node *)p);
	}

	if (is_svg3_timing_tag(node->sgprivate->tag)) {
		SVG3TimedAnimBaseElement *tap = (SVG3TimedAnimBaseElement *)node;
		if (tap->animp) {
			free(tap->animp);
			gf_smil_anim_remove_from_target((GF_Node *)tap, (GF_Node *)tap->xlinkp->href->target);
		}
		if (tap->timingp)		{
			gf_smil_timing_delete_runtime_info((GF_Node *)tap, tap->timingp->runtime);
			free(tap->timingp);
		}	
		if (tap->xlinkp)	free(tap->xlinkp);
	}

	gf_svg3_delete_attributes(node->sgprivate->scenegraph, p->attributes);
	gf_sg_parent_reset(node);
	gf_node_free(node);
}

Bool gf_sg_svg3_node_init(GF_Node *node)
{
	switch (node->sgprivate->tag) {
	case TAG_SVG3_script:
		if (node->sgprivate->scenegraph->script_load) 
			node->sgprivate->scenegraph->script_load(node);
		return 1;
/*
	case TAG_SVG3_conditional:
		gf_smil_timing_init_runtime_info(node);
		((SVG3Element *)node)->timingp->runtime->evaluate = lsr_conditional_evaluate;
		gf_smil_setup_events(node);
		return 1;
	case TAG_SVG3_handler:
		if (node->sgprivate->scenegraph->script_load) 
			node->sgprivate->scenegraph->script_load(node);
		if (node->sgprivate->scenegraph->js_ifce)
			((SVGhandlerElement *)node)->handle_event = gf_sg_handle_dom_event;
		return 1;
		*/
	case TAG_SVG3_animateMotion:
	case TAG_SVG3_set: 
	case TAG_SVG3_animate: 
	case TAG_SVG3_animateColor: 
	case TAG_SVG3_animateTransform: 
		gf_smil_anim_init_node(node);
	case TAG_SVG3_audio: 
	case TAG_SVG3_video: 
		gf_smil_setup_events(node);
		/*we may get called several times depending on xlink:href resoling for events*/
		return (node->sgprivate->UserPrivate || node->sgprivate->UserCallback) ? 1 : 0;
	/*discard is implemented as a special animation element */
	case TAG_SVG3_discard: 
		gf_smil_anim_init_discard(node);
		gf_smil_setup_events(node);
		return 1;
	default:
		return 0;
	}
	return 0;
}

Bool gf_sg_svg3_node_changed(GF_Node *node, GF_FieldInfo *field)
{
	switch (node->sgprivate->tag) {
	case TAG_SVG3_animateMotion:
	case TAG_SVG3_set: 
	case TAG_SVG3_animate: 
	case TAG_SVG3_animateColor: 
	case TAG_SVG3_animateTransform: 
	case TAG_SVG3_conditional: 
		gf_smil_timing_modified(node, field);
		return 1;
	case TAG_SVG3_audio: 
	case TAG_SVG3_video: 
		gf_smil_timing_modified(node, field);
		/*used by renderers*/
		return 0;
	}
	return 0;
}

SVG3Attribute *gf_svg3_create_attribute_from_datatype(u32 data_type, u32 attribute_tag)
{
	SVG3Attribute *att;
	if (!data_type) return NULL;

	GF_SAFEALLOC(att, SVG3Attribute);
	att->data_type = (u16) data_type;
	att->tag = (u16) attribute_tag;
	att->data = gf_svg_create_attribute_value(att->data_type);
	return att;
}


void gf_svg3_attributes_set_default_value(u32 node_tag, SVG3Attribute *att)
{
	switch (att->tag) {
	case TAG_SVG3_ATT_width:
	case TAG_SVG3_ATT_height:
		if (node_tag == TAG_SVG3_svg) {
			SVG_Length *len = att->data;
			len->type = SVG_NUMBER_PERCENTAGE;
			len->value = INT2FIX(100);
		}
		break;
	case TAG_SVG3_ATT_x2:
		if (node_tag == TAG_SVG3_linearGradient) {
			((SVG_Number *)att->data)->value = FIX_ONE;
		}
		break;
	case TAG_SVG3_ATT_cx:
	case TAG_SVG3_ATT_cy:
	case TAG_SVG3_ATT_fx:
	case TAG_SVG3_ATT_fy:
	case TAG_SVG3_ATT_r:
		if (node_tag == TAG_SVG3_radialGradient) {
			((SVG_Number *)att->data)->value = FIX_ONE/2;
		}
		break;
	case TAG_SVG3_ATT_dur:
		if (node_tag == TAG_SVG3_video || 
			node_tag == TAG_SVG3_audio ||
			node_tag == TAG_SVG3_animation)
		{
			((SMIL_Duration *)att->data)->type = SMIL_DURATION_MEDIA;
		}
		break;
	case TAG_SVG3_ATT_min:
		((SMIL_Duration *)att->data)->type = SMIL_DURATION_DEFINED;
		break;
	case TAG_SVG3_ATT_repeatDur:
		((SMIL_Duration *)att->data)->type = SMIL_DURATION_INDEFINITE;
		break;
	case TAG_SVG3_ATT_calcMode:
		if (node_tag == TAG_SVG3_animateMotion)
			*((SMIL_CalcMode *)att->data) = SMIL_CALCMODE_PACED;
		break;
	case TAG_SVG3_ATT_color:
		((SVG_Paint *)att->data)->type = SVG_PAINT_COLOR;
		((SVG_Paint *)att->data)->color.type = SVG_COLOR_INHERIT;
		break;
	case TAG_SVG3_ATT_solid_color:
	case TAG_SVG3_ATT_stop_color:
		((SVG_Paint *)att->data)->type = SVG_PAINT_COLOR;
		((SVG_Paint *)att->data)->color.type = SVG_COLOR_RGBCOLOR;
		break;
	case TAG_SVG3_ATT_opacity:
	case TAG_SVG3_ATT_solid_opacity:
	case TAG_SVG3_ATT_stop_opacity:
	case TAG_SVG3_ATT_audio_level:
	case TAG_SVG3_ATT_viewport_fill_opacity:
		((SVG_Number *)att->data)->value = FIX_ONE;
		break;
	case TAG_SVG3_ATT_display:
		*((SVG_Display *)att->data) = SVG_DISPLAY_INLINE;
		break;
	case TAG_SVG3_ATT_fill:
	case TAG_SVG3_ATT_stroke:
		((SVG_Paint *)att->data)->type = SVG_PAINT_INHERIT;
		break;
	case TAG_SVG3_ATT_stroke_dasharray:
		((SVG_StrokeDashArray *)att->data)->type = SVG_STROKEDASHARRAY_INHERIT;
		break;
	case TAG_SVG3_ATT_fill_opacity:
	case TAG_SVG3_ATT_stroke_opacity:
	case TAG_SVG3_ATT_stroke_width:
	case TAG_SVG3_ATT_font_size:
	case TAG_SVG3_ATT_line_increment:
	case TAG_SVG3_ATT_stroke_dashoffset:
	case TAG_SVG3_ATT_stroke_miterlimit:
		((SVG_Number *)att->data)->type = SVG_NUMBER_INHERIT;
		break;
	case TAG_SVG3_ATT_vector_effect:
		*((SVG_VectorEffect *)att->data) = SVG_VECTOREFFECT_NONE;
		break;
	case TAG_SVG3_ATT_fill_rule:
		*((SVG_FillRule *)att->data) = SVG_FILLRULE_INHERIT;
		break;
	case TAG_SVG3_ATT_font_weight:
		*((SVG_FontWeight *)att->data) = SVG_FONTWEIGHT_INHERIT;
		break;
	case TAG_SVG3_ATT_visibility:
		*((SVG_Visibility *)att->data) = SVG_VISIBILITY_INHERIT;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[Scene] Cannot create default value for SVG attribute %s\n", gf_svg3_get_attribute_name_from_tag(att->tag)));
	}
}

GF_Err gf_svg3_get_attribute_by_tag(GF_Node *node, u16 attribute_tag, Bool create_if_not_found, Bool set_default, GF_FieldInfo *field)
{
	SVG3Element *elt = (SVG3Element *)node;
	SVG3Attribute *att = elt->attributes;
	SVG3Attribute *last_att = NULL;

	while (att) {
		if (att->tag == attribute_tag) {
			field->fieldIndex = att->tag;
			field->fieldType = att->data_type;
			field->far_ptr   = att->data;
			return GF_OK;
		}
		last_att = att;
		att = att->next;
	}

	if (create_if_not_found) {
		/* field not found create one */
		att = gf_svg3_create_attribute(node, attribute_tag);
		if (att) {
			if (!elt->attributes) elt->attributes = att;
			else last_att->next = att;
			field->far_ptr = att->data;
			field->fieldType = att->data_type;
			field->fieldIndex = att->tag;
			/* attribute name should not be called, if needed use gf_svg3_get_attribute_name_from_tag(att->tag);*/
			field->name = NULL; 
			if (set_default) gf_svg3_attributes_set_default_value(node->sgprivate->tag, att);
			return GF_OK;
		}
	}

	return GF_NOT_SUPPORTED;
}

GF_Err gf_svg3_get_attribute_by_name(GF_Node *node, char *name, Bool create_if_not_found, Bool set_default, GF_FieldInfo *field)
{
	SVG3Element *elt = (SVG3Element *)node;

	u16 attribute_tag = gf_svg3_get_attribute_tag(node->sgprivate->tag, name);
	if (attribute_tag == TAG_SVG3_ATT_Unknown) {
		memset(field, 0, sizeof(GF_FieldInfo));
		return GF_NOT_SUPPORTED;
	}
	return gf_svg3_get_attribute_by_tag(node, attribute_tag, create_if_not_found, set_default, field);
}

u32 gf_svg3_get_rendering_flag_if_modified(SVG3Element *n, GF_FieldInfo *info)
{
//	return 0xFFFFFFFF;
	switch (info->fieldType) {
	case SVG_Paint_datatype: 
		if (info->fieldIndex == TAG_SVG3_ATT_fill)	return GF_SG_SVG_FILL_DIRTY;
		if (info->fieldIndex == TAG_SVG3_ATT_stroke) return GF_SG_SVG_STROKE_DIRTY;
		if (info->fieldIndex == TAG_SVG3_ATT_solid_color) return GF_SG_SVG_SOLIDCOLOR_DIRTY;
		if (info->fieldIndex == TAG_SVG3_ATT_stop_color) return GF_SG_SVG_STOPCOLOR_DIRTY;
		break;
	case SVG_Number_datatype:
		if (info->fieldIndex == TAG_SVG3_ATT_opacity) return GF_SG_SVG_OPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG3_ATT_fill_opacity) return GF_SG_SVG_FILLOPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG3_ATT_stroke_opacity) return GF_SG_SVG_STROKEOPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG3_ATT_solid_opacity) return GF_SG_SVG_SOLIDOPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG3_ATT_stop_opacity) return GF_SG_SVG_STOPOPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG3_ATT_line_increment) return GF_SG_SVG_LINEINCREMENT_DIRTY;
		if (info->fieldIndex == TAG_SVG3_ATT_stroke_miterlimit) return GF_SG_SVG_STROKEMITERLIMIT_DIRTY;
		break;
	case SVG_Length_datatype:
		if (info->fieldIndex == TAG_SVG3_ATT_stroke_dashoffset) return GF_SG_SVG_STROKEDASHOFFSET_DIRTY;
		if (info->fieldIndex == TAG_SVG3_ATT_stroke_width) return GF_SG_SVG_STROKEWIDTH_DIRTY;
		break;
	case SVG_DisplayAlign_datatype: 
		return GF_SG_SVG_DISPLAYALIGN_DIRTY;		
	case SVG_FillRule_datatype:
		return GF_SG_SVG_FILLRULE_DIRTY;
	case SVG_FontFamily_datatype:
		return GF_SG_SVG_FONTFAMILY_DIRTY;
	case SVG_FontSize_datatype:
		return GF_SG_SVG_FONTSIZE_DIRTY;
	case SVG_FontStyle_datatype:
		return GF_SG_SVG_FONTSTYLE_DIRTY;
	case SVG_FontVariant_datatype:
		return GF_SG_SVG_FONTVARIANT_DIRTY;
	case SVG_FontWeight_datatype:
		return GF_SG_SVG_FONTWEIGHT_DIRTY;
	case SVG_StrokeDashArray_datatype:
		return GF_SG_SVG_STROKEDASHARRAY_DIRTY;
	case SVG_StrokeLineCap_datatype:
		return GF_SG_SVG_STROKELINECAP_DIRTY;
	case SVG_StrokeLineJoin_datatype:
		return GF_SG_SVG_STROKELINEJOIN_DIRTY;
	case SVG_TextAlign_datatype:
		return GF_SG_SVG_TEXTALIGN_DIRTY;
	case SVG_TextAnchor_datatype:
		return GF_SG_SVG_TEXTANCHOR_DIRTY;
	case SVG_VectorEffect_datatype:
		return GF_SG_SVG_VECTOREFFECT_DIRTY;
	}

	/* this is not a property but a regular attribute, the animatable attributes are at the moment:
		focusable, focusHighlight, gradientUnits, nav-*, target, xlink:href, xlink:type, 


		the following affect the geometry of the element (some affect the positioning):
		cx, cy, d, height, offset, pathLength, points, r, rx, ry, width, x, x1, x2, y, y1, y2, rotate

		the following affect the positioning and are computed at each frame:
		transform, viewBox, preserveAspectRatio
	*/
	switch (info->fieldType) {
		case SVG_Number_datatype:
		case SVG_Length_datatype:
		case SVG_Coordinate_datatype:
		case SVG_Numbers_datatype:
		case SVG_Points_datatype:
		case SVG_Coordinates_datatype:
		case SVG_PathData_datatype:
		case SVG_Rotate_datatype:
			return GF_SG_SVG_GEOMETRY_DIRTY;

		case SVG_IRI_datatype:
			return GF_SG_NODE_DIRTY;

		//case SVG_Matrix_datatype:
		//case SVG_Motion_datatype:
		//case SVG_ViewBox_datatype:

		default:
			return 0;
	}
}

const char *gf_svg3_get_attribute_name_from_tag(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return "id";
		case TAG_SVG3_ATT__class: return "class";
		case TAG_SVG3_ATT_xml_id: return "xml:id";
		case TAG_SVG3_ATT_xml_base: return "xml:base";
		case TAG_SVG3_ATT_xml_lang: return "xml:lang";
		case TAG_SVG3_ATT_xml_space: return "xml:space";
		case TAG_SVG3_ATT_requiredFeatures: return "requiredFeatures";
		case TAG_SVG3_ATT_requiredExtensions: return "requiredExtensions";
		case TAG_SVG3_ATT_requiredFormats: return "requiredFormats";
		case TAG_SVG3_ATT_requiredFonts: return "requiredFonts";
		case TAG_SVG3_ATT_systemLanguage: return "systemLanguage";
		case TAG_SVG3_ATT_display: return "display";
		case TAG_SVG3_ATT_visibility: return "visibility";
		case TAG_SVG3_ATT_image_rendering: return "image-rendering";
		case TAG_SVG3_ATT_pointer_events: return "pointer-events";
		case TAG_SVG3_ATT_shape_rendering: return "shape-rendering";
		case TAG_SVG3_ATT_text_rendering: return "text-rendering";
		case TAG_SVG3_ATT_audio_level: return "audio-level";
		case TAG_SVG3_ATT_viewport_fill: return "viewport-fill";
		case TAG_SVG3_ATT_viewport_fill_opacity: return "viewport-fill-opacity";
		case TAG_SVG3_ATT_fill_opacity: return "fill-opacity";
		case TAG_SVG3_ATT_stroke_opacity: return "stroke-opacity";
		case TAG_SVG3_ATT_fill: return "fill";
		case TAG_SVG3_ATT_fill_rule: return "fill-rule";
		case TAG_SVG3_ATT_stroke: return "stroke";
		case TAG_SVG3_ATT_stroke_dasharray: return "stroke-dasharray";
		case TAG_SVG3_ATT_stroke_dashoffset: return "stroke-dashoffset";
		case TAG_SVG3_ATT_stroke_linecap: return "stroke-linecap";
		case TAG_SVG3_ATT_stroke_linejoin: return "stroke-linejoin";
		case TAG_SVG3_ATT_stroke_miterlimit: return "stroke-miterlimit";
		case TAG_SVG3_ATT_stroke_width: return "stroke-width";
		case TAG_SVG3_ATT_color: return "color";
		case TAG_SVG3_ATT_color_rendering: return "color-rendering";
		case TAG_SVG3_ATT_vector_effect: return "vector-effect";
		case TAG_SVG3_ATT_solid_color: return "solid-color";
		case TAG_SVG3_ATT_solid_opacity: return "solid-opacity";
		case TAG_SVG3_ATT_display_align: return "display-align";
		case TAG_SVG3_ATT_line_increment: return "line-increment";
		case TAG_SVG3_ATT_stop_color: return "stop-color";
		case TAG_SVG3_ATT_stop_opacity: return "stop-opacity";
		case TAG_SVG3_ATT_font_family: return "font-family";
		case TAG_SVG3_ATT_font_size: return "font-size";
		case TAG_SVG3_ATT_font_style: return "font-style";
		case TAG_SVG3_ATT_font_variant: return "font-variant";
		case TAG_SVG3_ATT_font_weight: return "font-weight";
		case TAG_SVG3_ATT_text_anchor: return "text-anchor";
		case TAG_SVG3_ATT_text_align: return "text-align";
		case TAG_SVG3_ATT_focusHighlight: return "focusHighlight";
		case TAG_SVG3_ATT_externalResourcesRequired: return "externalResourcesRequired";
		case TAG_SVG3_ATT_focusable: return "focusable";
		case TAG_SVG3_ATT_nav_next: return "nav-next";
		case TAG_SVG3_ATT_nav_prev: return "nav-prev";
		case TAG_SVG3_ATT_nav_up: return "nav-up";
		case TAG_SVG3_ATT_nav_up_right: return "nav-up-right";
		case TAG_SVG3_ATT_nav_right: return "nav-right";
		case TAG_SVG3_ATT_nav_down_right: return "nav-down-right";
		case TAG_SVG3_ATT_nav_down: return "nav-down";
		case TAG_SVG3_ATT_nav_down_left: return "nav-down-left";
		case TAG_SVG3_ATT_nav_left: return "nav-left";
		case TAG_SVG3_ATT_nav_up_left: return "nav-up-left";
		case TAG_SVG3_ATT_transform: return "transform";
		case TAG_SVG3_ATT_xlink_type: return "xlink:type";
		case TAG_SVG3_ATT_xlink_role: return "xlink:role";
		case TAG_SVG3_ATT_xlink_arcrole: return "xlink:arcrole";
		case TAG_SVG3_ATT_xlink_title: return "xlink:title";
		case TAG_SVG3_ATT_xlink_href: return "xlink:href";
		case TAG_SVG3_ATT_xlink_show: return "xlink:show";
		case TAG_SVG3_ATT_xlink_actuate: return "xlink:actuate";
		case TAG_SVG3_ATT_target: return "target";
		case TAG_SVG3_ATT_attributeName: return "attributeName";
		case TAG_SVG3_ATT_attributeType: return "attributeType";
		case TAG_SVG3_ATT_begin: return "begin";
		case TAG_SVG3_ATT_lsr_enabled: return "lsr:enabled";
		case TAG_SVG3_ATT_dur: return "dur";
		case TAG_SVG3_ATT_end: return "end";
		case TAG_SVG3_ATT_repeatCount: return "repeatCount";
		case TAG_SVG3_ATT_repeatDur: return "repeatDur";
		case TAG_SVG3_ATT_restart: return "restart";
		case TAG_SVG3_ATT_smil_fill: return "smil:fill";
		case TAG_SVG3_ATT_min: return "min";
		case TAG_SVG3_ATT_max: return "max";
		case TAG_SVG3_ATT_to: return "to";
		case TAG_SVG3_ATT_calcMode: return "calcMode";
		case TAG_SVG3_ATT_values: return "values";
		case TAG_SVG3_ATT_keyTimes: return "keyTimes";
		case TAG_SVG3_ATT_keySplines: return "keySplines";
		case TAG_SVG3_ATT_from: return "from";
		case TAG_SVG3_ATT_by: return "by";
		case TAG_SVG3_ATT_additive: return "additive";
		case TAG_SVG3_ATT_accumulate: return "accumulate";
		case TAG_SVG3_ATT_path: return "path";
		case TAG_SVG3_ATT_keyPoints: return "keyPoints";
		case TAG_SVG3_ATT_rotate: return "rotate";
		case TAG_SVG3_ATT_origin: return "origin";
		case TAG_SVG3_ATT_type: return "type";
		case TAG_SVG3_ATT_clipBegin: return "clipBegin";
		case TAG_SVG3_ATT_clipEnd: return "clipEnd";
		case TAG_SVG3_ATT_syncBehavior: return "syncBehavior";
		case TAG_SVG3_ATT_syncTolerance: return "syncTolerance";
		case TAG_SVG3_ATT_syncMaster: return "syncMaster";
		case TAG_SVG3_ATT_syncReference: return "syncReference";
		case TAG_SVG3_ATT_x: return "x";
		case TAG_SVG3_ATT_y: return "y";
		case TAG_SVG3_ATT_width: return "width";
		case TAG_SVG3_ATT_height: return "height";
		case TAG_SVG3_ATT_preserveAspectRatio: return "preserveAspectRatio";
		case TAG_SVG3_ATT_initialVisibility: return "initialVisibility";
		case TAG_SVG3_ATT_content_type: return "content-type";
		case TAG_SVG3_ATT_cx: return "cx";
		case TAG_SVG3_ATT_cy: return "cy";
		case TAG_SVG3_ATT_r: return "r";
		case TAG_SVG3_ATT_enabled: return "lsr:enabled";
		case TAG_SVG3_ATT_cursorManager_x: return "cursorManager:x";
		case TAG_SVG3_ATT_cursorManager_y: return "cursorManager:y";
		case TAG_SVG3_ATT_rx: return "rx";
		case TAG_SVG3_ATT_ry: return "ry";
		case TAG_SVG3_ATT_horiz_adv_x: return "horiz_adv_x";
		case TAG_SVG3_ATT_horiz_origin_x: return "horiz_origin_x";
		case TAG_SVG3_ATT_font_stretch: return "font_stretch";
		case TAG_SVG3_ATT_unicode_range: return "unicode_range";
		case TAG_SVG3_ATT_panose_1: return "panose_1";
		case TAG_SVG3_ATT_widths: return "widths";
		case TAG_SVG3_ATT_bbox: return "bbox";
		case TAG_SVG3_ATT_units_per_em: return "units_per_em";
		case TAG_SVG3_ATT_stemv: return "stemv";
		case TAG_SVG3_ATT_stemh: return "stemh";
		case TAG_SVG3_ATT_slope: return "slope";
		case TAG_SVG3_ATT_cap_height: return "cap_height";
		case TAG_SVG3_ATT_x_height: return "x_height";
		case TAG_SVG3_ATT_accent_height: return "accent_height";
		case TAG_SVG3_ATT_ascent: return "ascent";
		case TAG_SVG3_ATT_descent: return "descent";
		case TAG_SVG3_ATT_ideographic: return "ideographic";
		case TAG_SVG3_ATT_alphabetic: return "alphabetic";
		case TAG_SVG3_ATT_mathematical: return "mathematical";
		case TAG_SVG3_ATT_hanging: return "hanging";
		case TAG_SVG3_ATT_underline_position: return "underline_position";
		case TAG_SVG3_ATT_underline_thickness: return "underline_thickness";
		case TAG_SVG3_ATT_strikethrough_position: return "strikethrough_position";
		case TAG_SVG3_ATT_strikethrough_thickness: return "strikethrough_thickness";
		case TAG_SVG3_ATT_overline_position: return "overline_position";
		case TAG_SVG3_ATT_overline_thickness: return "overline_thickness";
		case TAG_SVG3_ATT_d: return "d";
		case TAG_SVG3_ATT_unicode: return "unicode";
		case TAG_SVG3_ATT_glyph_name: return "glyph_name";
		case TAG_SVG3_ATT_arabic_form: return "arabic_form";
		case TAG_SVG3_ATT_lang: return "lang";
		case TAG_SVG3_ATT_ev_event: return "ev_event";
		case TAG_SVG3_ATT_u1: return "u1";
		case TAG_SVG3_ATT_g1: return "g1";
		case TAG_SVG3_ATT_u2: return "u2";
		case TAG_SVG3_ATT_g2: return "g2";
		case TAG_SVG3_ATT_k: return "k";
		case TAG_SVG3_ATT_opacity: return "opacity";
		case TAG_SVG3_ATT_x1: return "x1";
		case TAG_SVG3_ATT_y1: return "y1";
		case TAG_SVG3_ATT_x2: return "x2";
		case TAG_SVG3_ATT_y2: return "y2";
		case TAG_SVG3_ATT_gradientUnits: return "gradientUnits";
		case TAG_SVG3_ATT_spreadMethod: return "spreadMethod";
		case TAG_SVG3_ATT_gradientTransform: return "gradientTransform";
		case TAG_SVG3_ATT_event: return "event";
		case TAG_SVG3_ATT_phase: return "phase";
		case TAG_SVG3_ATT_propagate: return "propagate";
		case TAG_SVG3_ATT_defaultAction: return "defaultAction";
		case TAG_SVG3_ATT_observer: return "observer";
		case TAG_SVG3_ATT_listener_target: return "listener_target";
		case TAG_SVG3_ATT_handler: return "handler";
		case TAG_SVG3_ATT_pathLength: return "pathLength";
		case TAG_SVG3_ATT_points: return "points";
		case TAG_SVG3_ATT_mediaSize: return "mediaSize";
		case TAG_SVG3_ATT_mediaTime: return "mediaTime";
		case TAG_SVG3_ATT_mediaCharacterEncoding: return "mediaCharacterEncoding";
		case TAG_SVG3_ATT_mediaContentEncodings: return "mediaContentEncodings";
		case TAG_SVG3_ATT_bandwidth: return "bandwidth";
		case TAG_SVG3_ATT_fx: return "fx";
		case TAG_SVG3_ATT_fy: return "fy";
		case TAG_SVG3_ATT_size: return "lsr:size";
		case TAG_SVG3_ATT_choice: return "lsr:choice";
		case TAG_SVG3_ATT_delta: return "lsr:delta";
		case TAG_SVG3_ATT_offset: return "offset";
		case TAG_SVG3_ATT_syncBehaviorDefault: return "syncBehaviorDefault";
		case TAG_SVG3_ATT_syncToleranceDefault: return "syncToleranceDefault";
		case TAG_SVG3_ATT_viewBox: return "viewBox";
		case TAG_SVG3_ATT_zoomAndPan: return "zoomAndPan";
		case TAG_SVG3_ATT_version: return "version";
		case TAG_SVG3_ATT_baseProfile: return "baseProfile";
		case TAG_SVG3_ATT_snapshotTime: return "snapshotTime";
		case TAG_SVG3_ATT_timelineBegin: return "timelineBegin";
		case TAG_SVG3_ATT_playbackOrder: return "playbackOrder";
		case TAG_SVG3_ATT_editable: return "editable";
		case TAG_SVG3_ATT_text_x: return "x";
		case TAG_SVG3_ATT_text_y: return "y";
		case TAG_SVG3_ATT_text_rotate: return "rotate";
		case TAG_SVG3_ATT_transformBehavior: return "transformBehavior";
		case TAG_SVG3_ATT_overlay: return "overlay";
		case TAG_SVG3_ATT_motionTransform: return "motionTransform";
		default: return "unknown";
	}
}
#endif