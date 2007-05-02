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

Bool gf_svg_is_animation_tag(u32 tag)
{
	return (tag == TAG_SVG_set ||
			tag == TAG_SVG_animate ||
			tag == TAG_SVG_animateColor ||
			tag == TAG_SVG_animateTransform ||
			tag == TAG_SVG_animateMotion || 
			tag == TAG_SVG_discard)?1:0;
}

static Bool gf_svg_is_timing_tag(u32 tag)
{
	if (gf_svg_is_animation_tag(tag)) return 1;
	else return (tag == TAG_SVG_animation ||
			tag == TAG_SVG_audio ||
			tag == TAG_SVG_conditional ||
			tag == TAG_SVG_video)?1:0;
}

SVG_Element *gf_svg_create_node(u32 ElementTag)
{
	SVG_Element *p;
	if (gf_svg_is_timing_tag(ElementTag)) {
		SVGTimedAnimBaseElement *tap;
		GF_SAFEALLOC(tap, SVGTimedAnimBaseElement); 
		p = (SVG_Element *)tap;
	} else if (ElementTag == TAG_SVG_handler) {
		SVG_handlerElement *hdl;
		GF_SAFEALLOC(hdl, SVG_handlerElement); 
		p = (SVG_Element *)hdl;
	} else {
		GF_SAFEALLOC(p, SVG_Element);
	}
	gf_node_setup((GF_Node *)p, ElementTag);
	gf_sg_parent_setup((GF_Node *) p);
	return p;
}

static void gf_svg_delete_attributes(GF_SceneGraph *sg, SVGAttribute *attributes)
{
	SVGAttribute *tmp;
	SVGAttribute *att = attributes;
	while(att) {
		gf_svg_delete_attribute_value(att->data_type, att->data, sg);
		tmp = att;
		att = att->next;
		free(tmp);
	}
}

void gf_svg_node_del(GF_Node *node)
{
	SVG_Element *p = (SVG_Element *)node;

	if (p->sgprivate->interact && p->sgprivate->interact->animations) {
		gf_smil_anim_delete_animations((GF_Node *)p);
	}
	if (p->sgprivate->tag==TAG_SVG_listener) {
		/*remove from all registered parents' listener list*/
		GF_Node *obs = node->sgprivate->UserPrivate;
		node->sgprivate->UserPrivate = NULL;
		if (obs && obs->sgprivate->interact && obs->sgprivate->interact->events) {
			gf_list_del_item(obs->sgprivate->interact->events, node);
		}
	}

	if (gf_svg_is_timing_tag(node->sgprivate->tag)) {
		SVGTimedAnimBaseElement *tap = (SVGTimedAnimBaseElement *)node;
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

	gf_svg_delete_attributes(node->sgprivate->scenegraph, p->attributes);
	gf_sg_parent_reset(node);
	gf_node_free(node);
}

Bool gf_svg_node_init(GF_Node *node)
{
	switch (node->sgprivate->tag) {
	case TAG_SVG_script:
		if (node->sgprivate->scenegraph->script_load) 
			node->sgprivate->scenegraph->script_load(node);
		return 1;
/*
	case TAG_SVG_handler:
		if (node->sgprivate->scenegraph->script_load) 
			node->sgprivate->scenegraph->script_load(node);
		if (node->sgprivate->scenegraph->js_ifce)
			((SVG_SA_handlerElement *)node)->handle_event = gf_sg_handle_dom_event;
		return 1;
		*/
	case TAG_SVG_conditional:
		gf_smil_timing_init_runtime_info(node);
		gf_smil_setup_events(node);
		return 1;
	case TAG_SVG_animateMotion:
	case TAG_SVG_set: 
	case TAG_SVG_animate: 
	case TAG_SVG_animateColor: 
	case TAG_SVG_animateTransform: 
		gf_smil_anim_init_node(node);
	case TAG_SVG_audio: 
	case TAG_SVG_video: 
		gf_smil_setup_events(node);
		/*we may get called several times depending on xlink:href resoling for events*/
		return (node->sgprivate->UserPrivate || node->sgprivate->UserCallback) ? 1 : 0;
	/*discard is implemented as a special animation element */
	case TAG_SVG_discard: 
		gf_smil_anim_init_discard(node);
		gf_smil_setup_events(node);
		return 1;
	default:
		return 0;
	}
	return 0;
}

Bool gf_svg_node_changed(GF_Node *node, GF_FieldInfo *field)
{
	switch (node->sgprivate->tag) {
	case TAG_SVG_animateMotion:
	case TAG_SVG_set: 
	case TAG_SVG_animate: 
	case TAG_SVG_animateColor: 
	case TAG_SVG_animateTransform: 
	case TAG_SVG_conditional: 
		gf_smil_timing_modified(node, field);
		return 1;
	case TAG_SVG_audio: 
	case TAG_SVG_video: 
		gf_smil_timing_modified(node, field);
		/*used by renderers*/
		return 0;
	}
	return 0;
}

SVGAttribute *gf_svg_create_attribute_from_datatype(u32 data_type, u32 attribute_tag)
{
	SVGAttribute *att;
	if (!data_type) return NULL;

	GF_SAFEALLOC(att, SVGAttribute);
	att->data_type = (u16) data_type;
	att->tag = (u16) attribute_tag;
	att->data = gf_svg_create_attribute_value(att->data_type);
	return att;
}


static void gf_svg_attributes_set_default_value(u32 node_tag, SVGAttribute *att)
{
	switch (att->tag) {
	case TAG_SVG_ATT_width:
	case TAG_SVG_ATT_height:
		if (node_tag == TAG_SVG_svg) {
			SVG_Length *len = att->data;
			len->type = SVG_NUMBER_PERCENTAGE;
			len->value = INT2FIX(100);
		}
		break;
	case TAG_SVG_ATT_x2:
		if (node_tag == TAG_SVG_linearGradient) {
			((SVG_Number *)att->data)->value = FIX_ONE;
		}
		break;
	case TAG_SVG_ATT_cx:
	case TAG_SVG_ATT_cy:
	case TAG_SVG_ATT_fx:
	case TAG_SVG_ATT_fy:
	case TAG_SVG_ATT_r:
		if (node_tag == TAG_SVG_radialGradient) {
			((SVG_Number *)att->data)->value = FIX_ONE/2;
		}
		break;
	case TAG_SVG_ATT_dur:
		if (node_tag == TAG_SVG_video || 
			node_tag == TAG_SVG_audio ||
			node_tag == TAG_SVG_animation)
		{
			((SMIL_Duration *)att->data)->type = SMIL_DURATION_MEDIA;
		} else {
			/*is this correct?*/
			((SMIL_Duration *)att->data)->type = SMIL_DURATION_INDEFINITE;
		}
		break;
	case TAG_SVG_ATT_min:
		((SMIL_Duration *)att->data)->type = SMIL_DURATION_DEFINED;
		break;
	case TAG_SVG_ATT_repeatDur:
		((SMIL_Duration *)att->data)->type = SMIL_DURATION_INDEFINITE;
		break;
	case TAG_SVG_ATT_calcMode:
		if (node_tag == TAG_SVG_animateMotion)
			*((SMIL_CalcMode *)att->data) = SMIL_CALCMODE_PACED;
		else
			*((SMIL_CalcMode *)att->data) = SMIL_CALCMODE_LINEAR;
		break;
	case TAG_SVG_ATT_color:
		((SVG_Paint *)att->data)->type = SVG_PAINT_COLOR;
		((SVG_Paint *)att->data)->color.type = SVG_COLOR_INHERIT;
		break;
	case TAG_SVG_ATT_solid_color:
	case TAG_SVG_ATT_stop_color:
		((SVG_Paint *)att->data)->type = SVG_PAINT_COLOR;
		((SVG_Paint *)att->data)->color.type = SVG_COLOR_RGBCOLOR;
		break;
	case TAG_SVG_ATT_opacity:
	case TAG_SVG_ATT_solid_opacity:
	case TAG_SVG_ATT_stop_opacity:
	case TAG_SVG_ATT_audio_level:
	case TAG_SVG_ATT_viewport_fill_opacity:
		((SVG_Number *)att->data)->value = FIX_ONE;
		break;
	case TAG_SVG_ATT_display:
		*((SVG_Display *)att->data) = SVG_DISPLAY_INLINE;
		break;
	case TAG_SVG_ATT_fill:
	case TAG_SVG_ATT_stroke:
		((SVG_Paint *)att->data)->type = SVG_PAINT_INHERIT;
		break;
	case TAG_SVG_ATT_stroke_dasharray:
		((SVG_StrokeDashArray *)att->data)->type = SVG_STROKEDASHARRAY_INHERIT;
		break;
	case TAG_SVG_ATT_fill_opacity:
	case TAG_SVG_ATT_stroke_opacity:
	case TAG_SVG_ATT_stroke_width:
	case TAG_SVG_ATT_font_size:
	case TAG_SVG_ATT_line_increment:
	case TAG_SVG_ATT_stroke_dashoffset:
	case TAG_SVG_ATT_stroke_miterlimit:
		((SVG_Number *)att->data)->type = SVG_NUMBER_INHERIT;
		break;
	case TAG_SVG_ATT_vector_effect:
		*((SVG_VectorEffect *)att->data) = SVG_VECTOREFFECT_NONE;
		break;
	case TAG_SVG_ATT_fill_rule:
		*((SVG_FillRule *)att->data) = SVG_FILLRULE_INHERIT;
		break;
	case TAG_SVG_ATT_font_weight:
		*((SVG_FontWeight *)att->data) = SVG_FONTWEIGHT_INHERIT;
		break;
	case TAG_SVG_ATT_visibility:
		*((SVG_Visibility *)att->data) = SVG_VISIBILITY_INHERIT;
		break;
	case TAG_SVG_ATT_smil_fill:
		*((SMIL_Fill *)att->data) = SMIL_FILL_REMOVE;
		break;
	case TAG_SVG_ATT_defaultAction:
		*((XMLEV_DefaultAction *)att->data) = XMLEVENT_DEFAULTACTION_PERFORM;
		break;
	case TAG_SVG_ATT_zoomAndPan:
		*((SVG_ZoomAndPan *)att->data) = SVG_ZOOMANDPAN_MAGNIFY;
		break;
	case TAG_SVG_ATT_stroke_linecap: 
		*(SVG_StrokeLineCap*)att->data = SVG_STROKELINECAP_INHERIT; 
		break;
	case TAG_SVG_ATT_stroke_linejoin: 
		*(SVG_StrokeLineJoin*)att->data = SVG_STROKELINEJOIN_INHERIT; 
		break;

	case TAG_SVG_ATT_transform: 
		gf_mx2d_init(((SVG_Transform*)att->data)->mat);
		break;

		
	/*all default=0 values (don't need init)*/
	case TAG_SVG_ATT_font_family:
	case TAG_SVG_ATT_font_style:
	case TAG_SVG_ATT_text_anchor:
	case TAG_SVG_ATT_x:
	case TAG_SVG_ATT_y:
		break;

	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[Scene] Cannot create default value for SVG attribute %s\n", gf_svg_get_attribute_name(att->tag)));
	}
}

GF_Err gf_svg_get_attribute_by_tag(GF_Node *node, u32 attribute_tag, Bool create_if_not_found, Bool set_default, GF_FieldInfo *field)
{
	SVG_Element *elt = (SVG_Element *)node;
	SVGAttribute *att = elt->attributes;
	SVGAttribute *last_att = NULL;

	while (att) {
		if ((u32) att->tag == attribute_tag) {
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
		att = gf_svg_create_attribute(node, attribute_tag);
		if (att) {
			if (!elt->attributes) elt->attributes = att;
			else last_att->next = att;
			field->far_ptr = att->data;
			field->fieldType = att->data_type;
			field->fieldIndex = att->tag;
			/* attribute name should not be called, if needed use gf_svg_get_attribute_name(att->tag);*/
			field->name = NULL; 
			if (set_default) gf_svg_attributes_set_default_value(node->sgprivate->tag, att);
			return GF_OK;
		}
	}

	return GF_NOT_SUPPORTED;
}

GF_Err gf_svg_get_attribute_by_name(GF_Node *node, char *name, Bool create_if_not_found, Bool set_default, GF_FieldInfo *field)
{
	u32 attribute_tag = gf_svg_get_attribute_tag(node->sgprivate->tag, name);
	if (attribute_tag == TAG_SVG_ATT_Unknown) {
		memset(field, 0, sizeof(GF_FieldInfo));
		return GF_NOT_SUPPORTED;
	}
	return gf_svg_get_attribute_by_tag(node, attribute_tag, create_if_not_found, set_default, field);
}

u32 gf_svg_get_rendering_flag_if_modified(SVG_Element *n, GF_FieldInfo *info)
{
//	return 0xFFFFFFFF;
	switch (info->fieldType) {
	case SVG_Paint_datatype: 
		if (info->fieldIndex == TAG_SVG_ATT_fill)	return GF_SG_SVG_FILL_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_stroke) return GF_SG_SVG_STROKE_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_solid_color) return GF_SG_SVG_SOLIDCOLOR_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_stop_color) return GF_SG_SVG_STOPCOLOR_DIRTY;
		break;
	case SVG_Number_datatype:
		if (info->fieldIndex == TAG_SVG_ATT_opacity) return GF_SG_SVG_OPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_fill_opacity) return GF_SG_SVG_FILLOPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_stroke_opacity) return GF_SG_SVG_STROKEOPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_solid_opacity) return GF_SG_SVG_SOLIDOPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_stop_opacity) return GF_SG_SVG_STOPOPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_line_increment) return GF_SG_SVG_LINEINCREMENT_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_stroke_miterlimit) return GF_SG_SVG_STROKEMITERLIMIT_DIRTY;
		break;
	case SVG_Length_datatype:
		if (info->fieldIndex == TAG_SVG_ATT_stroke_dashoffset) return GF_SG_SVG_STROKEDASHOFFSET_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_stroke_width) return GF_SG_SVG_STROKEWIDTH_DIRTY;
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

		case XMLRI_datatype:
			return GF_SG_NODE_DIRTY;

		//case SVG_Matrix_datatype:
		//case SVG_Motion_datatype:
		//case SVG_ViewBox_datatype:

		default:
			return 0;
	}
}

/* NOTE: Some properties (audio-level, display, opacity, solid*, stop*, vector-effect, viewport*) 
         are inherited only when they are  specified with the value 'inherit'
         otherwise they default to their initial value
		 which for the function below means NULL, the renderer will take care of the rest
 */
u32 gf_svg_apply_inheritance(SVGAllAttributes *all_atts, SVGPropertiesPointers *render_svg_props) 
{
	u32 inherited_flags_mask = GF_SG_NODE_DIRTY | GF_SG_CHILD_DIRTY;
	if(!all_atts || !render_svg_props) return ~inherited_flags_mask;

	if (all_atts->audio_level && all_atts->audio_level->type != SVG_NUMBER_INHERIT) {
		render_svg_props->audio_level = all_atts->audio_level;	
	} else if (!all_atts->audio_level) {
		render_svg_props->audio_level = NULL;
	}
	
	if (all_atts->color && all_atts->color->color.type != SVG_COLOR_INHERIT) {
		render_svg_props->color = all_atts->color;
	} else {
		inherited_flags_mask |= GF_SG_SVG_COLOR_DIRTY;
	}
	if (all_atts->color_rendering && *(all_atts->color_rendering) != SVG_RENDERINGHINT_INHERIT) {
		render_svg_props->color_rendering = all_atts->color_rendering;
	}
	if (all_atts->display && *(all_atts->display) != SVG_DISPLAY_INHERIT) {
		render_svg_props->display = all_atts->display;
	} else if (!all_atts->display) {
		render_svg_props->display = NULL;
	}

	if (all_atts->display_align && *(all_atts->display_align) != SVG_DISPLAYALIGN_INHERIT) {
		render_svg_props->display_align = all_atts->display_align;
	} else {
		inherited_flags_mask |= GF_SG_SVG_DISPLAYALIGN_DIRTY;
	}
	if (all_atts->fill && all_atts->fill->type != SVG_PAINT_INHERIT) {
		render_svg_props->fill = all_atts->fill;
		if (all_atts->fill->type == SVG_PAINT_COLOR && 
			all_atts->fill->color.type == SVG_COLOR_CURRENTCOLOR &&
			(inherited_flags_mask & GF_SG_SVG_COLOR_DIRTY)) {
			inherited_flags_mask |= GF_SG_SVG_FILL_DIRTY;
		}
	} else {
		inherited_flags_mask |= GF_SG_SVG_FILL_DIRTY;
	}
	if (all_atts->fill_opacity && all_atts->fill_opacity->type != SVG_NUMBER_INHERIT) {
		render_svg_props->fill_opacity = all_atts->fill_opacity;
	} else {
		inherited_flags_mask |= GF_SG_SVG_FILLOPACITY_DIRTY;
	}
	if (all_atts->fill_rule && *(all_atts->fill_rule) != SVG_FILLRULE_INHERIT) {
		render_svg_props->fill_rule = all_atts->fill_rule;
	} else {
		inherited_flags_mask |= GF_SG_SVG_FILLRULE_DIRTY;
	}
	if (all_atts->font_family && all_atts->font_family->type != SVG_FONTFAMILY_INHERIT) {
		render_svg_props->font_family = all_atts->font_family;
	} else {
		inherited_flags_mask |= GF_SG_SVG_FONTFAMILY_DIRTY;
	}
	if (all_atts->font_size && all_atts->font_size->type != SVG_NUMBER_INHERIT) {
		render_svg_props->font_size = all_atts->font_size;
	} else {
		inherited_flags_mask |= GF_SG_SVG_FONTSIZE_DIRTY;
	}
	if (all_atts->font_style && *(all_atts->font_style) != SVG_FONTSTYLE_INHERIT) {
		render_svg_props->font_style = all_atts->font_style;
	} else {
		inherited_flags_mask |= GF_SG_SVG_FONTSTYLE_DIRTY;
	}
	if (all_atts->font_variant && *(all_atts->font_variant) != SVG_FONTVARIANT_INHERIT) {
		render_svg_props->font_variant = all_atts->font_variant;
	} else {
		inherited_flags_mask |= GF_SG_SVG_FONTVARIANT_DIRTY;
	}
	if (all_atts->font_weight && *(all_atts->font_weight) != SVG_FONTWEIGHT_INHERIT) {
		render_svg_props->font_weight = all_atts->font_weight;
	} else {
		inherited_flags_mask |= GF_SG_SVG_FONTWEIGHT_DIRTY;
	}
	if (all_atts->image_rendering && *(all_atts->image_rendering) != SVG_RENDERINGHINT_INHERIT) {
		render_svg_props->image_rendering = all_atts->image_rendering;
	}
	if (all_atts->line_increment && all_atts->line_increment->type != SVG_NUMBER_INHERIT) {
		render_svg_props->line_increment = all_atts->line_increment;
	} else {
		inherited_flags_mask |= GF_SG_SVG_LINEINCREMENT_DIRTY;
	}
	if (all_atts->opacity && all_atts->opacity->type != SVG_NUMBER_INHERIT) {
		render_svg_props->opacity = all_atts->opacity;
	} else if (!all_atts->opacity) {
		render_svg_props->opacity = NULL;
	} else {
		inherited_flags_mask |= GF_SG_SVG_OPACITY_DIRTY;
	}

	if (all_atts->pointer_events && *(all_atts->pointer_events) != SVG_POINTEREVENTS_INHERIT) {
		render_svg_props->pointer_events = all_atts->pointer_events;
	}
	if (all_atts->shape_rendering && *(all_atts->shape_rendering) != SVG_RENDERINGHINT_INHERIT) {
		render_svg_props->shape_rendering = all_atts->shape_rendering;
	}
	if (all_atts->solid_color && all_atts->solid_color->type != SVG_PAINT_INHERIT) {
		render_svg_props->solid_color = all_atts->solid_color;		
		if (all_atts->solid_color->type == SVG_PAINT_COLOR && 
			all_atts->solid_color->color.type == SVG_COLOR_CURRENTCOLOR &&
			(inherited_flags_mask & GF_SG_SVG_COLOR_DIRTY)) {
			inherited_flags_mask |= GF_SG_SVG_SOLIDCOLOR_DIRTY;
		}
	} else if (!all_atts->solid_color) {
		render_svg_props->solid_color = NULL;
	} else {
		inherited_flags_mask |= GF_SG_SVG_SOLIDCOLOR_DIRTY;
	}
	if (all_atts->solid_opacity && all_atts->solid_opacity->type != SVG_NUMBER_INHERIT) {
		render_svg_props->solid_opacity = all_atts->solid_opacity;
	} else if (!all_atts->solid_opacity) {
		render_svg_props->solid_opacity = NULL;
	} else {
		inherited_flags_mask |= GF_SG_SVG_SOLIDOPACITY_DIRTY;
	}
	if (all_atts->stop_color && all_atts->stop_color->type != SVG_PAINT_INHERIT) {
		render_svg_props->stop_color = all_atts->stop_color;
		if (all_atts->stop_color->type == SVG_PAINT_COLOR && 
			all_atts->stop_color->color.type == SVG_COLOR_CURRENTCOLOR &&
			(inherited_flags_mask & GF_SG_SVG_COLOR_DIRTY)) {
			inherited_flags_mask |= GF_SG_SVG_STOPCOLOR_DIRTY;
		}
	} else if (!all_atts->stop_color) {
		render_svg_props->stop_color = NULL;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STOPCOLOR_DIRTY;
	}
	if (all_atts->stop_opacity && all_atts->stop_opacity->type != SVG_NUMBER_INHERIT) {
		render_svg_props->stop_opacity = all_atts->stop_opacity;
	} else if (!all_atts->stop_opacity) {
		render_svg_props->stop_opacity = NULL;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STOPOPACITY_DIRTY;
	}
	if (all_atts->stroke && all_atts->stroke->type != SVG_PAINT_INHERIT) {
		render_svg_props->stroke = all_atts->stroke;
		if (all_atts->stroke->type == SVG_PAINT_COLOR && 
			all_atts->stroke->color.type == SVG_COLOR_CURRENTCOLOR &&
			(inherited_flags_mask & GF_SG_SVG_COLOR_DIRTY)) {
			inherited_flags_mask |= GF_SG_SVG_STROKE_DIRTY;
		}
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKE_DIRTY;
	}
	if (all_atts->stroke_dasharray && all_atts->stroke_dasharray->type != SVG_STROKEDASHARRAY_INHERIT) {
		render_svg_props->stroke_dasharray = all_atts->stroke_dasharray;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKEDASHARRAY_DIRTY;
	}
	if (all_atts->stroke_dashoffset && all_atts->stroke_dashoffset->type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_dashoffset = all_atts->stroke_dashoffset;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKEDASHOFFSET_DIRTY;
	}
	if (all_atts->stroke_linecap && *(all_atts->stroke_linecap) != SVG_STROKELINECAP_INHERIT) {
		render_svg_props->stroke_linecap = all_atts->stroke_linecap;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKELINECAP_DIRTY;
	}
	if (all_atts->stroke_linejoin && *(all_atts->stroke_linejoin) != SVG_STROKELINEJOIN_INHERIT) {
		render_svg_props->stroke_linejoin = all_atts->stroke_linejoin;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKELINEJOIN_DIRTY;
	}
	if (all_atts->stroke_miterlimit && all_atts->stroke_miterlimit->type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_miterlimit = all_atts->stroke_miterlimit;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKEMITERLIMIT_DIRTY;
	}
	if (all_atts->stroke_opacity && all_atts->stroke_opacity->type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_opacity = all_atts->stroke_opacity;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKEOPACITY_DIRTY;
	}
	if (all_atts->stroke_width && all_atts->stroke_width->type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_width = all_atts->stroke_width;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKEWIDTH_DIRTY;
	}
	if (all_atts->text_align && *(all_atts->text_align) != SVG_TEXTALIGN_INHERIT) {
		render_svg_props->text_align = all_atts->text_align;
	} else {
		inherited_flags_mask |= GF_SG_SVG_TEXTALIGN_DIRTY;
	}
	if (all_atts->text_anchor && *(all_atts->text_anchor) != SVG_TEXTANCHOR_INHERIT) {
		render_svg_props->text_anchor = all_atts->text_anchor;
	} else {
		inherited_flags_mask |= GF_SG_SVG_TEXTANCHOR_DIRTY;
	}
	if (all_atts->text_rendering && *(all_atts->text_rendering) != SVG_RENDERINGHINT_INHERIT) {
		render_svg_props->text_rendering = all_atts->text_rendering;
	}
	if (all_atts->vector_effect && *(all_atts->vector_effect) != SVG_VECTOREFFECT_INHERIT) {
		render_svg_props->vector_effect = all_atts->vector_effect;
	} else if (!all_atts->vector_effect) {
		render_svg_props->vector_effect = NULL;
	} else {
		inherited_flags_mask |= GF_SG_SVG_VECTOREFFECT_DIRTY;
	}
	if (all_atts->viewport_fill && all_atts->viewport_fill->type != SVG_PAINT_INHERIT) {
		render_svg_props->viewport_fill = all_atts->viewport_fill;		
	} else if (!all_atts->viewport_fill) {
		render_svg_props->viewport_fill = NULL;
	} 
	if (all_atts->viewport_fill_opacity && all_atts->viewport_fill_opacity->type != SVG_NUMBER_INHERIT) {
		render_svg_props->viewport_fill_opacity = all_atts->viewport_fill_opacity;
	} else if (!all_atts->viewport_fill_opacity) {
		render_svg_props->viewport_fill_opacity = NULL;
	}
	if (all_atts->visibility && *(all_atts->visibility) != SVG_VISIBILITY_INHERIT) {
		render_svg_props->visibility = all_atts->visibility;
	}
	return inherited_flags_mask;
}

#endif

