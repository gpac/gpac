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

#include <gpac/nodes_svg_sani.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/scenegraph_dev.h>

void gf_svg2_init_core(SVG2Element *p) 
{
	GF_SAFEALLOC(p->core, XMLCoreAttributes)
}

void gf_svg2_init_focus(SVG2Element *p)
{
	GF_SAFEALLOC(p->focus, SVGFocusAttributes)
}

void gf_svg2_init_xlink(SVG2Element *p)
{
	GF_SAFEALLOC(p->xlink, XLinkAttributes)
}

void gf_svg2_init_timing(SVG2Element *p)
{
	GF_SAFEALLOC(p->timing, SMILTimingAttributes)
	p->timing->begin = gf_list_new();
	p->timing->end = gf_list_new();
	p->timing->min.type = SMIL_DURATION_DEFINED;
	p->timing->repeatDur.type = SMIL_DURATION_INDEFINITE;
}

void gf_svg2_init_sync(SVG2Element *p)
{
	GF_SAFEALLOC(p->sync, SMILSyncAttributes)
}

void gf_svg2_init_anim(SVG2Element *p)
{
	GF_SAFEALLOC(p->anim, SMILAnimationAttributes)
	p->anim->lsr_enabled = 1;
	p->anim->keySplines = gf_list_new();
	p->anim->keyTimes = gf_list_new();
	p->anim->values.values = gf_list_new();
	if (gf_node_get_tag((GF_Node *)p) == TAG_SVG2_animateMotion)
		p->anim->calcMode = SMIL_CALCMODE_PACED;
}

void gf_svg2_init_conditional(SVG2Element *p)
{
	GF_SAFEALLOC(p->conditional, SVGConditionalAttributes)
	p->conditional->requiredExtensions = gf_list_new();
	p->conditional->requiredFeatures = gf_list_new();
	p->conditional->requiredFonts = gf_list_new();
	p->conditional->requiredFormats = gf_list_new();
	p->conditional->systemLanguage = gf_list_new();
}

void gf_svg2_delete_core(SVG2Element *elt, XMLCoreAttributes *p) 
{
	gf_svg_reset_iri(elt->sgprivate->scenegraph, &p->base);
	if (p->lang) free(p->lang);
	if (p->_class) free(p->_class);
	free(p);
}

static void svg2_reset_focus(SVG2Element *elt, SVG_Focus *focus) 
{
	if (focus->target.target) {
		gf_svg_unregister_iri(elt->sgprivate->scenegraph, &focus->target);
	}
	if (focus->target.iri) free(focus->target.iri);
}

void gf_svg2_delete_focus(SVG2Element *elt, SVGFocusAttributes *p) 
{
	svg2_reset_focus(elt, &p->nav_next);
	svg2_reset_focus(elt, &p->nav_prev);
	svg2_reset_focus(elt, &p->nav_down);
	svg2_reset_focus(elt, &p->nav_down_left);
	svg2_reset_focus(elt, &p->nav_down_right);
	svg2_reset_focus(elt, &p->nav_left);
	svg2_reset_focus(elt, &p->nav_right);
	svg2_reset_focus(elt, &p->nav_up);
	svg2_reset_focus(elt, &p->nav_up_left);
	svg2_reset_focus(elt, &p->nav_up_right);
	free(p);		
}

void gf_svg2_delete_xlink(SVG2Element *elt, XLinkAttributes *p)
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

void gf_svg2_reset_base_element(SVG2Element *p)
{
	if (p->textContent) free(p->textContent);
	if (p->core)		gf_svg2_delete_core(p, p->core);
	if (p->focus)		gf_svg2_delete_focus(p, p->focus);
	if (p->conditional) gf_svg_delete_conditional(p->conditional);
	if (p->sync)		gf_svg_delete_sync(p->sync);

	if (p->sgprivate->interact && p->sgprivate->interact->animations) gf_smil_anim_delete_animations((GF_Node *)p);
	if (p->anim)		{
		gf_svg_delete_anim(p->anim, p->sgprivate->scenegraph);
		gf_smil_anim_remove_from_target((GF_Node *)p, (GF_Node *)p->xlink->href.target);
	}
	
	if (p->timing)		{
		gf_smil_timing_delete_runtime_info((GF_Node *)p, p->timing->runtime);
		gf_svg_delete_timing(p->timing);
	}
	
	if (p->xlink)		gf_svg2_delete_xlink(p, p->xlink);
}

Bool is_svg2_animation_tag(u32 tag)
{
	return (tag == TAG_SVG2_set ||
			tag == TAG_SVG2_animate ||
			tag == TAG_SVG2_animateColor ||
			tag == TAG_SVG2_animateTransform ||
			tag == TAG_SVG2_animateMotion || 
			tag == TAG_SVG2_discard)?1:0;
}

Bool gf_sg_svg2_node_init(GF_Node *node)
{
	switch (node->sgprivate->tag) {
	case TAG_SVG2_script:
		if (node->sgprivate->scenegraph->script_load) 
			node->sgprivate->scenegraph->script_load(node);
		return 1;
/*	case TAG_SVG2_conditional:
		gf_smil_timing_init_runtime_info(node);
		((SVGElement *)node)->timing->runtime->evaluate = lsr_conditional_evaluate;
		gf_smil_setup_events(node);
		return 1;*/
	case TAG_SVG2_handler:
		if (node->sgprivate->scenegraph->script_load) 
			node->sgprivate->scenegraph->script_load(node);
		if (node->sgprivate->scenegraph->js_ifce)
			((SVG2handlerElement *)node)->handle_event = gf_sg_handle_dom_event;
		return 1;
	case TAG_SVG2_animateMotion:
	case TAG_SVG2_set: 
	case TAG_SVG2_animate: 
	case TAG_SVG2_animateColor: 
	case TAG_SVG2_animateTransform: 
		gf_smil_anim_init_node(node);
	case TAG_SVG2_audio: 
	case TAG_SVG2_video: 
		gf_smil_setup_events(node);
		/*we may get called several times depending on xlink:href resoling for events*/
		return (node->sgprivate->UserPrivate || node->sgprivate->UserCallback) ? 1 : 0;
	/*discard is implemented as a special animation element */
	case TAG_SVG2_discard: 
		gf_smil_anim_init_discard(node);
		gf_smil_setup_events(node);
		return 1;
	default:
		return 0;
	}
	return 0;
}

Bool gf_sg_svg2_node_changed(GF_Node *node, GF_FieldInfo *field)
{
	switch (node->sgprivate->tag) {
	case TAG_SVG2_animateMotion:
	case TAG_SVG2_set: 
	case TAG_SVG2_animate: 
	case TAG_SVG2_animateColor: 
	case TAG_SVG2_animateTransform: 
	case TAG_SVG2_conditional: 
		gf_smil_timing_modified(node, field);
		return 1;
	case TAG_SVG2_audio: 
	case TAG_SVG2_video: 
		gf_smil_timing_modified(node, field);
		/*used by renderers*/
		return 0;
	}
	return 0;
}

u32 gf_svg2_get_rendering_flag_if_modified(SVG2Element *n, GF_FieldInfo *info)
{
//	return 0xFFFFFFFF;
	switch (info->fieldType) {
	case SVG_Paint_datatype: 
		if (!strcmp(info->name, "fill"))	return GF_SG_SVG_FILL_DIRTY;
		if (!strcmp(info->name, "stroke")) return GF_SG_SVG_STROKE_DIRTY;
		if (!strcmp(info->name, "solid-color")) return GF_SG_SVG_SOLIDCOLOR_DIRTY;
		if (!strcmp(info->name, "stop-color")) return GF_SG_SVG_STOPCOLOR_DIRTY;
		break;
	case SVG_Number_datatype:
		if (!strcmp(info->name, "opacity")) return GF_SG_SVG_OPACITY_DIRTY;
		if (!strcmp(info->name, "fill-opacity")) return GF_SG_SVG_FILLOPACITY_DIRTY;
		if (!strcmp(info->name, "stroke-opacity")) return GF_SG_SVG_STROKEOPACITY_DIRTY;
		if (!strcmp(info->name, "solid-opacity")) return GF_SG_SVG_SOLIDOPACITY_DIRTY;
		if (!strcmp(info->name, "stop-opacity")) return GF_SG_SVG_STOPOPACITY_DIRTY;
		if (!strcmp(info->name, "line-increment")) return GF_SG_SVG_LINEINCREMENT_DIRTY;
		if (!strcmp(info->name, "stroke-miterlimit")) return GF_SG_SVG_STROKEMITERLIMIT_DIRTY;
		break;
	case SVG_Length_datatype:
		if (!strcmp(info->name, "stroke-dashoffset")) return GF_SG_SVG_STROKEDASHOFFSET_DIRTY;
		if (!strcmp(info->name, "stroke-width")) return GF_SG_SVG_STROKEWIDTH_DIRTY;
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
#endif /*GPAC_DISABLE_SVG*/
