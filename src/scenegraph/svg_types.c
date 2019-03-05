/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato, Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2004-2012
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
#include <gpac/nodes_svg.h>

GF_EXPORT
Bool gf_svg_is_animation_tag(u32 tag)
{
	return (tag == TAG_SVG_set ||
	        tag == TAG_SVG_animate ||
	        tag == TAG_SVG_animateColor ||
	        tag == TAG_SVG_animateTransform ||
	        tag == TAG_SVG_animateMotion ||
	        tag == TAG_SVG_discard
	       ) ? 1 : 0;
}

Bool gf_svg_is_timing_tag(u32 tag)
{
	if (gf_svg_is_animation_tag(tag)) return 1;
	else return (tag == TAG_SVG_animation ||
		             tag == TAG_SVG_audio ||
		             tag == TAG_LSR_conditional ||
		             tag == TAG_LSR_updates ||
		             tag == TAG_SVG_video)? GF_TRUE : GF_FALSE;
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

void gf_svg_node_del(GF_Node *node)
{
	SVG_Element *p = (SVG_Element *)node;

	if (p->sgprivate->interact && p->sgprivate->interact->animations) {
		gf_smil_anim_delete_animations((GF_Node *)p);
	}
	if (p->sgprivate->tag==TAG_SVG_listener) {
		/*remove from target's listener list*/
		gf_dom_event_remove_listener_from_parent((GF_DOMEventTarget *)node->sgprivate->UserPrivate, (GF_Node *)p);
	}
	/*if this is a handler with a UserPrivate, this is a handler with an implicit listener
	(eg handler with ev:event=""). Destroy the associated listener*/
	if (p->sgprivate->tag==TAG_SVG_handler) {
		GF_Node *listener = p->sgprivate->UserPrivate;
		if (listener && (listener->sgprivate->tag==TAG_SVG_listener)) {
			GF_FieldInfo info;
			if (gf_node_get_attribute_by_tag(listener, TAG_XMLEV_ATT_handler, 0, 0, &info) == GF_OK) {
				XMLRI *iri = (XMLRI *)info.far_ptr;
				if (iri->target) {
					assert(iri->target==p);
					iri->target = NULL;
				}
			}
			gf_node_unregister(listener, NULL);
//			gf_svg_node_del(listener);
		}
	}
	/*remove this node from associated listeners*/
	if (node->sgprivate->interact && node->sgprivate->interact->dom_evt) {
		u32 i, count;
		count = gf_dom_listener_count(node);
		for (i=0; i<count; i++) {
			GF_Node *listener = (GF_Node *)gf_list_get(node->sgprivate->interact->dom_evt->listeners, i);
			listener->sgprivate->UserPrivate = NULL;
		}
	}

	if (gf_svg_is_timing_tag(node->sgprivate->tag)) {
		SVGTimedAnimBaseElement *tap = (SVGTimedAnimBaseElement *)node;
		if (tap->animp) {
			gf_free(tap->animp);
			gf_smil_anim_remove_from_target((GF_Node *)tap, (GF_Node *)tap->xlinkp->href->target);
		}
		if (tap->timingp)		{
			gf_smil_timing_delete_runtime_info((GF_Node *)tap, tap->timingp->runtime);
			gf_free(tap->timingp);
		}
		if (tap->xlinkp)	gf_free(tap->xlinkp);
	}

	gf_node_delete_attributes(node);
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

	case TAG_SVG_handler:
		if (node->sgprivate->scenegraph->script_load)
			node->sgprivate->scenegraph->script_load(node);
		if (node->sgprivate->scenegraph->script_action)
			((SVG_handlerElement*)node)->handle_event = gf_sg_handle_dom_event;
		return 1;
	case TAG_LSR_conditional:
		gf_smil_timing_init_runtime_info(node);
		gf_smil_setup_events(node);
		return 1;
	case TAG_SVG_animateMotion:
	case TAG_SVG_set:
	case TAG_SVG_animate:
	case TAG_SVG_animateColor:
	case TAG_SVG_animateTransform:
		gf_smil_anim_init_node(node);
		gf_smil_setup_events(node);
		/*we may get called several times depending on xlink:href resoling for events*/
		return (node->sgprivate->UserPrivate || node->sgprivate->UserCallback) ? 1 : 0;
	case TAG_SVG_audio:
	case TAG_SVG_video:
	case TAG_LSR_updates:
		gf_smil_timing_init_runtime_info(node);
		gf_smil_setup_events(node);
		/*we may get called several times depending on xlink:href resoling for events*/
		return (node->sgprivate->UserPrivate || node->sgprivate->UserCallback) ? 1 : 0;
	case TAG_SVG_animation:
		gf_smil_timing_init_runtime_info(node);
		gf_smil_setup_events(node);
		return 0;
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
	case TAG_SVG_discard:
	case TAG_SVG_set:
	case TAG_SVG_animate:
	case TAG_SVG_animateColor:
	case TAG_SVG_animateTransform:
	case TAG_LSR_conditional:
		gf_smil_timing_modified(node, field);
		return 1;
	case TAG_SVG_animation:
	case TAG_SVG_audio:
	case TAG_SVG_video:
	case TAG_LSR_updates:
		gf_smil_timing_modified(node, field);
		/*used by compositors*/
		return 0;
	}
	return 0;
}


void gf_svg_reset_path(SVG_PathData d)
{
#if USE_GF_PATH
	gf_path_reset(&d);
#else
	u32 i, count;
	count = gf_list_count(d.commands);
	for (i = 0; i < count; i++) {
		u8 *command = (u8 *)gf_list_get(d.commands, i);
		gf_free(command);
	}
	gf_list_del(d.commands);
	count = gf_list_count(d.points);
	for (i = 0; i < count; i++) {
		SVG_Point *pt = (SVG_Point *)gf_list_get(d.points, i);
		gf_free(pt);
	}
	gf_list_del(d.points);
#endif
}

/* TODO: update for elliptical arcs */
GF_EXPORT
void gf_svg_path_build(GF_Path *path, GF_List *commands, GF_List *points)
{
	u32 i, j, command_count;
	SVG_Point orig, ct_orig, ct_end, end, *tmp;
	command_count = gf_list_count(commands);
	orig.x = orig.y = ct_orig.x = ct_orig.y = 0;

	for (i=0, j=0; i<command_count; i++) {
		u8 *command = (u8 *)gf_list_get(commands, i);
		switch (*command) {
		case SVG_PATHCOMMAND_M: /* Move To */
			tmp = (SVG_Point*)gf_list_get(points, j);
			orig = *tmp;
			gf_path_add_move_to(path, orig.x, orig.y);
			j++;
			/*provision for nextCurveTo when no curve is specified:
				"If there is no previous command or if the previous command was not an C, c, S or s,
				assume the first control point is coincident with the current point.
			*/
			ct_orig = orig;
			break;
		case SVG_PATHCOMMAND_L: /* Line To */
			tmp = (SVG_Point*)gf_list_get(points, j);
			end = *tmp;

			gf_path_add_line_to(path, end.x, end.y);
			j++;
			orig = end;
			/*cf above*/
			ct_orig = orig;
			break;
		case SVG_PATHCOMMAND_C: /* Curve To */
			tmp = (SVG_Point*)gf_list_get(points, j);
			ct_orig = *tmp;
			tmp = (SVG_Point*)gf_list_get(points, j+1);
			ct_end = *tmp;
			tmp = (SVG_Point*)gf_list_get(points, j+2);
			end = *tmp;
			gf_path_add_cubic_to(path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
			ct_orig = ct_end;
			orig = end;
			j+=3;
			break;
		case SVG_PATHCOMMAND_S: /* Next Curve To */
			ct_orig.x = 2*orig.x - ct_orig.x;
			ct_orig.y = 2*orig.y - ct_orig.y;
			tmp = (SVG_Point*)gf_list_get(points, j);
			ct_end = *tmp;
			tmp = (SVG_Point*)gf_list_get(points, j+1);
			end = *tmp;
			gf_path_add_cubic_to(path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
			ct_orig = ct_end;
			orig = end;
			j+=2;
			break;
		case SVG_PATHCOMMAND_Q: /* Quadratic Curve To */
			tmp = (SVG_Point*)gf_list_get(points, j);
			ct_orig = *tmp;
			tmp = (SVG_Point*)gf_list_get(points, j+1);
			end = *tmp;
			gf_path_add_quadratic_to(path, ct_orig.x, ct_orig.y, end.x, end.y);
			orig = end;
			j+=2;
			break;
		case SVG_PATHCOMMAND_T: /* Next Quadratic Curve To */
			ct_orig.x = 2*orig.x - ct_orig.x;
			ct_orig.y = 2*orig.y - ct_orig.y;
			tmp = (SVG_Point*)gf_list_get(points, j);
			end = *tmp;
			gf_path_add_quadratic_to(path, ct_orig.x, ct_orig.y, end.x, end.y);
			orig = end;
			j++;
			break;
		case SVG_PATHCOMMAND_Z: /* Close */
			gf_path_close(path);
			break;
		}
	}
}


void gf_smil_delete_times(GF_List *list)
{
	u32 i, count;
	count = gf_list_count(list);
	for (i = 0; i < count; i++) {
		SMIL_Time *v = (SMIL_Time *)gf_list_get(list, i);
		if (v->element_id) gf_free(v->element_id);
		gf_free(v);
	}
	gf_list_del(list);
}

void gf_svg_delete_points(GF_List *list)
{
	u32 i, count = gf_list_count(list);
	for (i = 0; i < count; i++) {
		SVG_Point *p = (SVG_Point *)gf_list_get(list, i);
		gf_free(p);
	}
	gf_list_del(list);
}

void gf_svg_delete_coordinates(GF_List *list)
{
	u32 i, count = gf_list_count(list);
	for (i = 0; i < count; i++) {
		SVG_Coordinate *c = (SVG_Coordinate *)gf_list_get(list, i);
		gf_free(c);
	}
	gf_list_del(list);
}

void gf_svg_reset_iri(GF_SceneGraph *sg, XMLRI *iri)
{
	if (!iri) return;
	if (iri->string) gf_free(iri->string);
	gf_node_unregister_iri(sg, iri);
}

void gf_svg_delete_paint(GF_SceneGraph *sg, SVG_Paint *paint)
{
	if (!paint) return;
	//always free since we may allocate the iri to ""
	if (sg) gf_svg_reset_iri(sg, &paint->iri);
	gf_free(paint);
}

static void svg_delete_one_anim_value(u8 anim_datatype, void *anim_value, GF_SceneGraph *sg)
{
	/* TODO: handle specific animation types : Motion, else ? */
	gf_svg_delete_attribute_value(anim_datatype, anim_value, sg);
}

void gf_svg_reset_animate_values(SMIL_AnimateValues anim_values, GF_SceneGraph *sg)
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

void gf_svg_reset_animate_value(SMIL_AnimateValue anim_value, GF_SceneGraph *sg)
{
	svg_delete_one_anim_value(anim_value.type, anim_value.value, sg);
	anim_value.value = NULL;
}

void gf_svg_delete_attribute_value(u32 type, void *value, GF_SceneGraph *sg)
{
	GF_List *l;
	switch (type) {
	case SVG_Paint_datatype:
		gf_svg_delete_paint(sg, (SVG_Paint *)value);
		break;
	case XMLRI_datatype:
	case XML_IDREF_datatype:
		gf_svg_reset_iri(sg, (XMLRI *)value);
		gf_free(value);
		break;
	case SVG_Focus_datatype:
		gf_svg_reset_iri(sg, & ((SVG_Focus*)value)->target);
		gf_free(value);
		break;
	case SVG_PathData_datatype:
#if USE_GF_PATH
		gf_path_del((GF_Path *)value);
#else
		gf_free(value);
#endif
		break;
	case SVG_ID_datatype:
	case DOM_String_datatype:
	case SVG_ContentType_datatype:
	case SVG_LanguageID_datatype:
		if (*(SVG_String *)value) gf_free(*(SVG_String *)value);
		gf_free(value);
		break;
	case SVG_StrokeDashArray_datatype:
		if (((SVG_StrokeDashArray*)value)->array.vals) gf_free(((SVG_StrokeDashArray*)value)->array.vals);
		if (((SVG_StrokeDashArray*)value)->array.units) gf_free(((SVG_StrokeDashArray*)value)->array.units);
		gf_free(value);
		break;
	case SMIL_KeyTimes_datatype:
	case SMIL_KeyPoints_datatype:
	case SMIL_KeySplines_datatype:
	case SVG_Numbers_datatype:
	case SVG_Coordinates_datatype:
	case SVG_Points_datatype:
		l = *(GF_List**)value;
		while (gf_list_count(l)) {
			void *n = gf_list_last(l);
			gf_list_rem_last(l);
			gf_free(n);
		}
		gf_list_del(l);
		gf_free(value);
		break;
	case SVG_FontFamily_datatype:
	{
		SVG_FontFamily *ff = (SVG_FontFamily *)value;
		if (ff->value) gf_free(ff->value);
		gf_free(value);
	}
	break;
	case SMIL_AttributeName_datatype:
	{
		SMIL_AttributeName *an = (SMIL_AttributeName *)value;
		if (an->name) gf_free(an->name);
		gf_free(value);
	}
	break;
	case SMIL_Times_datatype:
		gf_smil_delete_times(*(SMIL_Times *)value);
		gf_free(value);
		break;
	case SMIL_AnimateValue_datatype:
		svg_delete_one_anim_value(((SMIL_AnimateValue *)value)->type, ((SMIL_AnimateValue *)value)->value, sg);
		gf_free(value);
		break;
	case SMIL_AnimateValues_datatype:
		gf_svg_reset_animate_values(*((SMIL_AnimateValues *)value), sg);
		gf_free(value);
		break;
	case DOM_StringList_datatype:
		l = *(GF_List**)value;
		while (gf_list_count(l)) {
			char *n = gf_list_last(l);
			gf_list_rem_last(l);
			gf_free(n);
		}
		gf_list_del(l);
		gf_free(value);
		break;
	case XMLRI_List_datatype:
		l = *(GF_List**)value;
		while (gf_list_count(l)) {
			XMLRI *r = gf_list_last(l);
			gf_list_rem_last(l);
			if (r->string) gf_free(r->string);
			gf_free(r);
		}
		gf_list_del(l);
		gf_free(value);
		break;

	case SMIL_RepeatCount_datatype:
	case SMIL_Duration_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	case SVG_Visibility_datatype:
	case SVG_Display_datatype:
	default:
		gf_free(value);
	}
}


void gf_smil_delete_key_types(GF_List *l)
{
	while (gf_list_count(l)) {
		Fixed *t = (Fixed *)gf_list_get(l, 0);
		gf_list_rem(l, 0);
		gf_free(t);
	}
	gf_list_del(l);
}


#endif /*GPAC_DISABLE_SVG*/
