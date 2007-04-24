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
	DO NOT MOFIFY - File generated on GMT Mon Apr 23 13:13:53 2007

	BY SVGGen for GPAC Version 0.4.3-DEV
*/

#include <gpac/nodes_svg_sa.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/scenegraph_dev.h>

#ifdef GPAC_ENABLE_SVG_SA

void *gf_svg_sa_new_a()
{
	SVG_SA_aElement *p;
	GF_SAFEALLOC(p, SVG_SA_aElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_a);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "a";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_a_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_a_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sa_a_del(GF_Node *node)
{
	SVG_SA_aElement *p = (SVG_SA_aElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->target) free(p->target);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_a_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 55:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 56:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 57:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 58:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 59:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 60:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 61:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 62:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 63:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 64:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 65:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 66:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 67:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 68:
			info->name = "target";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVG_SA_aElement *)node)->target;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_a_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("xlink:href", name)) return 54;
	if(!strcmp("xlink:show", name)) return 55;
	if(!strcmp("xlink:title", name)) return 56;
	if(!strcmp("xlink:actuate", name)) return 57;
	if(!strcmp("xlink:role", name)) return 58;
	if(!strcmp("xlink:arcrole", name)) return 59;
	if(!strcmp("xlink:type", name)) return 60;
	if(!strcmp("requiredExtensions", name)) return 61;
	if(!strcmp("requiredFeatures", name)) return 62;
	if(!strcmp("requiredFonts", name)) return 63;
	if(!strcmp("requiredFormats", name)) return 64;
	if(!strcmp("systemLanguage", name)) return 65;
	if(!strcmp("transform", name)) return 66;
	if(!strcmp("motionTransform", name)) return 67;
	if(!strcmp("target", name)) return 68;
	return -1;
}

void *gf_svg_sa_new_animate()
{
	SVG_SA_animateElement *p;
	GF_SAFEALLOC(p, SVG_SA_animateElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_animate);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animate";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_animate_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_animate_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	gf_svg_sa_init_timing((SVG_SA_Element *)p);
	gf_svg_sa_init_anim((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_animate_del(GF_Node *node)
{
	SVG_SA_animateElement *p = (SVG_SA_animateElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_animate_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->attributeName;
			return GF_OK;
		case 24:
			info->name = "attributeType";
			info->fieldType = SMIL_AttributeType_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->attributeType;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->to;
			return GF_OK;
		case 26:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->from;
			return GF_OK;
		case 27:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->by;
			return GF_OK;
		case 28:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->values;
			return GF_OK;
		case 29:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->calcMode;
			return GF_OK;
		case 30:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->keySplines;
			return GF_OK;
		case 31:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->keyTimes;
			return GF_OK;
		case 32:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->accumulate;
			return GF_OK;
		case 33:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->additive;
			return GF_OK;
		case 34:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->lsr_enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_animate_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("xlink:href", name)) return 7;
	if(!strcmp("xlink:show", name)) return 8;
	if(!strcmp("xlink:title", name)) return 9;
	if(!strcmp("xlink:actuate", name)) return 10;
	if(!strcmp("xlink:role", name)) return 11;
	if(!strcmp("xlink:arcrole", name)) return 12;
	if(!strcmp("xlink:type", name)) return 13;
	if(!strcmp("begin", name)) return 14;
	if(!strcmp("end", name)) return 15;
	if(!strcmp("dur", name)) return 16;
	if(!strcmp("repeatCount", name)) return 17;
	if(!strcmp("repeatDur", name)) return 18;
	if(!strcmp("restart", name)) return 19;
	if(!strcmp("min", name)) return 20;
	if(!strcmp("max", name)) return 21;
	if(!strcmp("fill", name)) return 22;
	if(!strcmp("attributeName", name)) return 23;
	if(!strcmp("attributeType", name)) return 24;
	if(!strcmp("to", name)) return 25;
	if(!strcmp("from", name)) return 26;
	if(!strcmp("by", name)) return 27;
	if(!strcmp("values", name)) return 28;
	if(!strcmp("calcMode", name)) return 29;
	if(!strcmp("keySplines", name)) return 30;
	if(!strcmp("keyTimes", name)) return 31;
	if(!strcmp("accumulate", name)) return 32;
	if(!strcmp("additive", name)) return 33;
	if(!strcmp("lsr:enabled", name)) return 34;
	return -1;
}

void *gf_svg_sa_new_animateColor()
{
	SVG_SA_animateColorElement *p;
	GF_SAFEALLOC(p, SVG_SA_animateColorElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_animateColor);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animateColor";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_animateColor_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_animateColor_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	gf_svg_sa_init_timing((SVG_SA_Element *)p);
	gf_svg_sa_init_anim((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_animateColor_del(GF_Node *node)
{
	SVG_SA_animateColorElement *p = (SVG_SA_animateColorElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_animateColor_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->attributeName;
			return GF_OK;
		case 24:
			info->name = "attributeType";
			info->fieldType = SMIL_AttributeType_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->attributeType;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->to;
			return GF_OK;
		case 26:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->from;
			return GF_OK;
		case 27:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->by;
			return GF_OK;
		case 28:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->values;
			return GF_OK;
		case 29:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->calcMode;
			return GF_OK;
		case 30:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->keySplines;
			return GF_OK;
		case 31:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->keyTimes;
			return GF_OK;
		case 32:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->accumulate;
			return GF_OK;
		case 33:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->additive;
			return GF_OK;
		case 34:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->lsr_enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_animateColor_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("xlink:href", name)) return 7;
	if(!strcmp("xlink:show", name)) return 8;
	if(!strcmp("xlink:title", name)) return 9;
	if(!strcmp("xlink:actuate", name)) return 10;
	if(!strcmp("xlink:role", name)) return 11;
	if(!strcmp("xlink:arcrole", name)) return 12;
	if(!strcmp("xlink:type", name)) return 13;
	if(!strcmp("begin", name)) return 14;
	if(!strcmp("end", name)) return 15;
	if(!strcmp("dur", name)) return 16;
	if(!strcmp("repeatCount", name)) return 17;
	if(!strcmp("repeatDur", name)) return 18;
	if(!strcmp("restart", name)) return 19;
	if(!strcmp("min", name)) return 20;
	if(!strcmp("max", name)) return 21;
	if(!strcmp("fill", name)) return 22;
	if(!strcmp("attributeName", name)) return 23;
	if(!strcmp("attributeType", name)) return 24;
	if(!strcmp("to", name)) return 25;
	if(!strcmp("from", name)) return 26;
	if(!strcmp("by", name)) return 27;
	if(!strcmp("values", name)) return 28;
	if(!strcmp("calcMode", name)) return 29;
	if(!strcmp("keySplines", name)) return 30;
	if(!strcmp("keyTimes", name)) return 31;
	if(!strcmp("accumulate", name)) return 32;
	if(!strcmp("additive", name)) return 33;
	if(!strcmp("lsr:enabled", name)) return 34;
	return -1;
}

void *gf_svg_sa_new_animateMotion()
{
	SVG_SA_animateMotionElement *p;
	GF_SAFEALLOC(p, SVG_SA_animateMotionElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_animateMotion);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animateMotion";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_animateMotion_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_animateMotion_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	gf_svg_sa_init_timing((SVG_SA_Element *)p);
	gf_svg_sa_init_anim((SVG_SA_Element *)p);
#ifdef USE_GF_PATH
	gf_path_reset(&p->path);
#else
	p->path.commands = gf_list_new();
	p->path.points = gf_list_new();
#endif
	p->keyPoints = gf_list_new();
	return p;
}

static void gf_svg_sa_animateMotion_del(GF_Node *node)
{
	SVG_SA_animateMotionElement *p = (SVG_SA_animateMotionElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_svg_reset_path(p->path);
	gf_smil_delete_key_types(p->keyPoints);
	free(p->origin);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_animateMotion_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->to;
			return GF_OK;
		case 24:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->from;
			return GF_OK;
		case 25:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->by;
			return GF_OK;
		case 26:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->values;
			return GF_OK;
		case 27:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->calcMode;
			return GF_OK;
		case 28:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->keySplines;
			return GF_OK;
		case 29:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->keyTimes;
			return GF_OK;
		case 30:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->accumulate;
			return GF_OK;
		case 31:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->additive;
			return GF_OK;
		case 32:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->lsr_enabled;
			return GF_OK;
		case 33:
			info->name = "path";
			info->fieldType = SVG_PathData_datatype;
			info->far_ptr = & ((SVG_SA_animateMotionElement *)node)->path;
			return GF_OK;
		case 34:
			info->name = "keyPoints";
			info->fieldType = SMIL_KeyPoints_datatype;
			info->far_ptr = & ((SVG_SA_animateMotionElement *)node)->keyPoints;
			return GF_OK;
		case 35:
			info->name = "rotate";
			info->fieldType = SVG_Rotate_datatype;
			info->far_ptr = & ((SVG_SA_animateMotionElement *)node)->rotate;
			return GF_OK;
		case 36:
			info->name = "origin";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_animateMotionElement *)node)->origin;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_animateMotion_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("xlink:href", name)) return 7;
	if(!strcmp("xlink:show", name)) return 8;
	if(!strcmp("xlink:title", name)) return 9;
	if(!strcmp("xlink:actuate", name)) return 10;
	if(!strcmp("xlink:role", name)) return 11;
	if(!strcmp("xlink:arcrole", name)) return 12;
	if(!strcmp("xlink:type", name)) return 13;
	if(!strcmp("begin", name)) return 14;
	if(!strcmp("end", name)) return 15;
	if(!strcmp("dur", name)) return 16;
	if(!strcmp("repeatCount", name)) return 17;
	if(!strcmp("repeatDur", name)) return 18;
	if(!strcmp("restart", name)) return 19;
	if(!strcmp("min", name)) return 20;
	if(!strcmp("max", name)) return 21;
	if(!strcmp("fill", name)) return 22;
	if(!strcmp("to", name)) return 23;
	if(!strcmp("from", name)) return 24;
	if(!strcmp("by", name)) return 25;
	if(!strcmp("values", name)) return 26;
	if(!strcmp("calcMode", name)) return 27;
	if(!strcmp("keySplines", name)) return 28;
	if(!strcmp("keyTimes", name)) return 29;
	if(!strcmp("accumulate", name)) return 30;
	if(!strcmp("additive", name)) return 31;
	if(!strcmp("lsr:enabled", name)) return 32;
	if(!strcmp("path", name)) return 33;
	if(!strcmp("keyPoints", name)) return 34;
	if(!strcmp("rotate", name)) return 35;
	if(!strcmp("origin", name)) return 36;
	return -1;
}

void *gf_svg_sa_new_animateTransform()
{
	SVG_SA_animateTransformElement *p;
	GF_SAFEALLOC(p, SVG_SA_animateTransformElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_animateTransform);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animateTransform";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_animateTransform_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_animateTransform_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	gf_svg_sa_init_timing((SVG_SA_Element *)p);
	gf_svg_sa_init_anim((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_animateTransform_del(GF_Node *node)
{
	SVG_SA_animateTransformElement *p = (SVG_SA_animateTransformElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_animateTransform_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->attributeName;
			return GF_OK;
		case 24:
			info->name = "attributeType";
			info->fieldType = SMIL_AttributeType_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->attributeType;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->to;
			return GF_OK;
		case 26:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->from;
			return GF_OK;
		case 27:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->by;
			return GF_OK;
		case 28:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->values;
			return GF_OK;
		case 29:
			info->name = "type";
			info->fieldType = SVG_TransformType_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->type;
			return GF_OK;
		case 30:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->calcMode;
			return GF_OK;
		case 31:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->keySplines;
			return GF_OK;
		case 32:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->keyTimes;
			return GF_OK;
		case 33:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->accumulate;
			return GF_OK;
		case 34:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->additive;
			return GF_OK;
		case 35:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->lsr_enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_animateTransform_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("xlink:href", name)) return 7;
	if(!strcmp("xlink:show", name)) return 8;
	if(!strcmp("xlink:title", name)) return 9;
	if(!strcmp("xlink:actuate", name)) return 10;
	if(!strcmp("xlink:role", name)) return 11;
	if(!strcmp("xlink:arcrole", name)) return 12;
	if(!strcmp("xlink:type", name)) return 13;
	if(!strcmp("begin", name)) return 14;
	if(!strcmp("end", name)) return 15;
	if(!strcmp("dur", name)) return 16;
	if(!strcmp("repeatCount", name)) return 17;
	if(!strcmp("repeatDur", name)) return 18;
	if(!strcmp("restart", name)) return 19;
	if(!strcmp("min", name)) return 20;
	if(!strcmp("max", name)) return 21;
	if(!strcmp("fill", name)) return 22;
	if(!strcmp("attributeName", name)) return 23;
	if(!strcmp("attributeType", name)) return 24;
	if(!strcmp("to", name)) return 25;
	if(!strcmp("from", name)) return 26;
	if(!strcmp("by", name)) return 27;
	if(!strcmp("values", name)) return 28;
	if(!strcmp("type", name)) return 29;
	if(!strcmp("calcMode", name)) return 30;
	if(!strcmp("keySplines", name)) return 31;
	if(!strcmp("keyTimes", name)) return 32;
	if(!strcmp("accumulate", name)) return 33;
	if(!strcmp("additive", name)) return 34;
	if(!strcmp("lsr:enabled", name)) return 35;
	return -1;
}

void *gf_svg_sa_new_animation()
{
	SVG_SA_animationElement *p;
	GF_SAFEALLOC(p, SVG_SA_animationElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_animation);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animation";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_animation_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_animation_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	gf_svg_sa_init_timing((SVG_SA_Element *)p);
	gf_svg_sa_init_sync((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->timing->dur.type = SMIL_DURATION_MEDIA;
	return p;
}

static void gf_svg_sa_animation_del(GF_Node *node)
{
	SVG_SA_animationElement *p = (SVG_SA_animationElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_animation_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 17:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 18:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 19:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 20:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 21:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 22:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 23:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 24:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 25:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 26:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 27:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 28:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 29:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 30:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 31:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 32:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 33:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 34:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 35:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->begin;
			return GF_OK;
		case 36:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->end;
			return GF_OK;
		case 37:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->dur;
			return GF_OK;
		case 38:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 39:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 40:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->restart;
			return GF_OK;
		case 41:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->min;
			return GF_OK;
		case 42:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->max;
			return GF_OK;
		case 43:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->fill;
			return GF_OK;
		case 44:
			info->name = "clipBegin";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->clipBegin;
			return GF_OK;
		case 45:
			info->name = "clipEnd";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->clipEnd;
			return GF_OK;
		case 46:
			info->name = "syncBehavior";
			info->fieldType = SMIL_SyncBehavior_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->sync->syncBehavior;
			return GF_OK;
		case 47:
			info->name = "syncTolerance";
			info->fieldType = SMIL_SyncTolerance_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->sync->syncTolerance;
			return GF_OK;
		case 48:
			info->name = "syncMaster";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->sync->syncMaster;
			return GF_OK;
		case 49:
			info->name = "syncReference";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->sync->syncReference;
			return GF_OK;
		case 50:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 51:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 52:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 53:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 54:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 55:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 56:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 57:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_animationElement *)node)->x;
			return GF_OK;
		case 58:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_animationElement *)node)->y;
			return GF_OK;
		case 59:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_animationElement *)node)->width;
			return GF_OK;
		case 60:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_animationElement *)node)->height;
			return GF_OK;
		case 61:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVG_SA_animationElement *)node)->preserveAspectRatio;
			return GF_OK;
		case 62:
			info->name = "initialVisibility";
			info->fieldType = SVG_InitialVisibility_datatype;
			info->far_ptr = & ((SVG_SA_animationElement *)node)->initialVisibility;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_animation_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("focusHighlight", name)) return 16;
	if(!strcmp("focusable", name)) return 17;
	if(!strcmp("nav-down", name)) return 18;
	if(!strcmp("nav-down-left", name)) return 19;
	if(!strcmp("nav-down-right", name)) return 20;
	if(!strcmp("nav-left", name)) return 21;
	if(!strcmp("nav-next", name)) return 22;
	if(!strcmp("nav-prev", name)) return 23;
	if(!strcmp("nav-right", name)) return 24;
	if(!strcmp("nav-up", name)) return 25;
	if(!strcmp("nav-up-left", name)) return 26;
	if(!strcmp("nav-up-right", name)) return 27;
	if(!strcmp("xlink:href", name)) return 28;
	if(!strcmp("xlink:show", name)) return 29;
	if(!strcmp("xlink:title", name)) return 30;
	if(!strcmp("xlink:actuate", name)) return 31;
	if(!strcmp("xlink:role", name)) return 32;
	if(!strcmp("xlink:arcrole", name)) return 33;
	if(!strcmp("xlink:type", name)) return 34;
	if(!strcmp("begin", name)) return 35;
	if(!strcmp("end", name)) return 36;
	if(!strcmp("dur", name)) return 37;
	if(!strcmp("repeatCount", name)) return 38;
	if(!strcmp("repeatDur", name)) return 39;
	if(!strcmp("restart", name)) return 40;
	if(!strcmp("min", name)) return 41;
	if(!strcmp("max", name)) return 42;
	if(!strcmp("fill", name)) return 43;
	if(!strcmp("clipBegin", name)) return 44;
	if(!strcmp("clipEnd", name)) return 45;
	if(!strcmp("syncBehavior", name)) return 46;
	if(!strcmp("syncTolerance", name)) return 47;
	if(!strcmp("syncMaster", name)) return 48;
	if(!strcmp("syncReference", name)) return 49;
	if(!strcmp("requiredExtensions", name)) return 50;
	if(!strcmp("requiredFeatures", name)) return 51;
	if(!strcmp("requiredFonts", name)) return 52;
	if(!strcmp("requiredFormats", name)) return 53;
	if(!strcmp("systemLanguage", name)) return 54;
	if(!strcmp("transform", name)) return 55;
	if(!strcmp("motionTransform", name)) return 56;
	if(!strcmp("x", name)) return 57;
	if(!strcmp("y", name)) return 58;
	if(!strcmp("width", name)) return 59;
	if(!strcmp("height", name)) return 60;
	if(!strcmp("preserveAspectRatio", name)) return 61;
	if(!strcmp("initialVisibility", name)) return 62;
	return -1;
}

void *gf_svg_sa_new_audio()
{
	SVG_SA_audioElement *p;
	GF_SAFEALLOC(p, SVG_SA_audioElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_audio);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "audio";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_audio_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_audio_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	gf_svg_sa_init_timing((SVG_SA_Element *)p);
	gf_svg_sa_init_sync((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	p->timing->dur.type = SMIL_DURATION_MEDIA;
	return p;
}

static void gf_svg_sa_audio_del(GF_Node *node)
{
	SVG_SA_audioElement *p = (SVG_SA_audioElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	free(p->type);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_audio_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 17:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 18:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 19:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 20:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 21:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 22:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 23:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->begin;
			return GF_OK;
		case 24:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->end;
			return GF_OK;
		case 25:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->dur;
			return GF_OK;
		case 26:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 27:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 28:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->restart;
			return GF_OK;
		case 29:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->min;
			return GF_OK;
		case 30:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->max;
			return GF_OK;
		case 31:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->fill;
			return GF_OK;
		case 32:
			info->name = "clipBegin";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->clipBegin;
			return GF_OK;
		case 33:
			info->name = "clipEnd";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->clipEnd;
			return GF_OK;
		case 34:
			info->name = "syncBehavior";
			info->fieldType = SMIL_SyncBehavior_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->sync->syncBehavior;
			return GF_OK;
		case 35:
			info->name = "syncTolerance";
			info->fieldType = SMIL_SyncTolerance_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->sync->syncTolerance;
			return GF_OK;
		case 36:
			info->name = "syncMaster";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->sync->syncMaster;
			return GF_OK;
		case 37:
			info->name = "syncReference";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->sync->syncReference;
			return GF_OK;
		case 38:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 39:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 40:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 41:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 42:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 43:
			info->name = "type";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVG_SA_audioElement *)node)->type;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_audio_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("xlink:href", name)) return 16;
	if(!strcmp("xlink:show", name)) return 17;
	if(!strcmp("xlink:title", name)) return 18;
	if(!strcmp("xlink:actuate", name)) return 19;
	if(!strcmp("xlink:role", name)) return 20;
	if(!strcmp("xlink:arcrole", name)) return 21;
	if(!strcmp("xlink:type", name)) return 22;
	if(!strcmp("begin", name)) return 23;
	if(!strcmp("end", name)) return 24;
	if(!strcmp("dur", name)) return 25;
	if(!strcmp("repeatCount", name)) return 26;
	if(!strcmp("repeatDur", name)) return 27;
	if(!strcmp("restart", name)) return 28;
	if(!strcmp("min", name)) return 29;
	if(!strcmp("max", name)) return 30;
	if(!strcmp("fill", name)) return 31;
	if(!strcmp("clipBegin", name)) return 32;
	if(!strcmp("clipEnd", name)) return 33;
	if(!strcmp("syncBehavior", name)) return 34;
	if(!strcmp("syncTolerance", name)) return 35;
	if(!strcmp("syncMaster", name)) return 36;
	if(!strcmp("syncReference", name)) return 37;
	if(!strcmp("requiredExtensions", name)) return 38;
	if(!strcmp("requiredFeatures", name)) return 39;
	if(!strcmp("requiredFonts", name)) return 40;
	if(!strcmp("requiredFormats", name)) return 41;
	if(!strcmp("systemLanguage", name)) return 42;
	if(!strcmp("type", name)) return 43;
	return -1;
}

void *gf_svg_sa_new_circle()
{
	SVG_SA_circleElement *p;
	GF_SAFEALLOC(p, SVG_SA_circleElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_circle);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "circle";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_circle_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_circle_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sa_circle_del(GF_Node *node)
{
	SVG_SA_circleElement *p = (SVG_SA_circleElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_circle_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 59:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 60:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 61:
			info->name = "cx";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_circleElement *)node)->cx;
			return GF_OK;
		case 62:
			info->name = "cy";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_circleElement *)node)->cy;
			return GF_OK;
		case 63:
			info->name = "r";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_circleElement *)node)->r;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_circle_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	if(!strcmp("transform", name)) return 59;
	if(!strcmp("motionTransform", name)) return 60;
	if(!strcmp("cx", name)) return 61;
	if(!strcmp("cy", name)) return 62;
	if(!strcmp("r", name)) return 63;
	return -1;
}

void *gf_svg_sa_new_conditional()
{
	SVG_SA_conditionalElement *p;
	GF_SAFEALLOC(p, SVG_SA_conditionalElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_conditional);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "conditional";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_conditional_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_conditional_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_timing((SVG_SA_Element *)p);
	gf_svg_sa_init_lsr_conditional(&p->updates);
	gf_svg_sa_init_timing((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_conditional_del(GF_Node *node)
{
	SVG_SA_conditionalElement *p = (SVG_SA_conditionalElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_svg_sa_reset_lsr_conditional(&p->updates);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_conditional_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = &((SVG_SA_Element *)node)->timing->begin;
			return GF_OK;
		case 8:
			info->name = "enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVG_SA_conditionalElement *)node)->enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_conditional_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("begin", name)) return 7;
	if(!strcmp("enabled", name)) return 8;
	return -1;
}

void *gf_svg_sa_new_cursorManager()
{
	SVG_SA_cursorManagerElement *p;
	GF_SAFEALLOC(p, SVG_SA_cursorManagerElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_cursorManager);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "cursorManager";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_cursorManager_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_cursorManager_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_cursorManager_del(GF_Node *node)
{
	SVG_SA_cursorManagerElement *p = (SVG_SA_cursorManagerElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_cursorManager_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "x";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_cursorManagerElement *)node)->x;
			return GF_OK;
		case 15:
			info->name = "y";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_cursorManagerElement *)node)->y;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_cursorManager_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("xlink:href", name)) return 7;
	if(!strcmp("xlink:show", name)) return 8;
	if(!strcmp("xlink:title", name)) return 9;
	if(!strcmp("xlink:actuate", name)) return 10;
	if(!strcmp("xlink:role", name)) return 11;
	if(!strcmp("xlink:arcrole", name)) return 12;
	if(!strcmp("xlink:type", name)) return 13;
	if(!strcmp("x", name)) return 14;
	if(!strcmp("y", name)) return 15;
	return -1;
}

void *gf_svg_sa_new_defs()
{
	SVG_SA_defsElement *p;
	GF_SAFEALLOC(p, SVG_SA_defsElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_defs);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "defs";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_defs_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_defs_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_defs_del(GF_Node *node)
{
	SVG_SA_defsElement *p = (SVG_SA_defsElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_defs_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_defs_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	return -1;
}

void *gf_svg_sa_new_desc()
{
	SVG_SA_descElement *p;
	GF_SAFEALLOC(p, SVG_SA_descElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_desc);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "desc";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_desc_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_desc_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_desc_del(GF_Node *node)
{
	SVG_SA_descElement *p = (SVG_SA_descElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_desc_get_attribute(GF_Node *node, GF_FieldInfo *info)
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

s32 gf_svg_sa_desc_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	return -1;
}

void *gf_svg_sa_new_discard()
{
	SVG_SA_discardElement *p;
	GF_SAFEALLOC(p, SVG_SA_discardElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_discard);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "discard";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_discard_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_discard_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	gf_svg_sa_init_timing((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_discard_del(GF_Node *node)
{
	SVG_SA_discardElement *p = (SVG_SA_discardElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_discard_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 16:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 17:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 18:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 19:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_discard_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("xlink:href", name)) return 7;
	if(!strcmp("xlink:show", name)) return 8;
	if(!strcmp("xlink:title", name)) return 9;
	if(!strcmp("xlink:actuate", name)) return 10;
	if(!strcmp("xlink:role", name)) return 11;
	if(!strcmp("xlink:arcrole", name)) return 12;
	if(!strcmp("xlink:type", name)) return 13;
	if(!strcmp("begin", name)) return 14;
	if(!strcmp("requiredExtensions", name)) return 15;
	if(!strcmp("requiredFeatures", name)) return 16;
	if(!strcmp("requiredFonts", name)) return 17;
	if(!strcmp("requiredFormats", name)) return 18;
	if(!strcmp("systemLanguage", name)) return 19;
	return -1;
}

void *gf_svg_sa_new_ellipse()
{
	SVG_SA_ellipseElement *p;
	GF_SAFEALLOC(p, SVG_SA_ellipseElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_ellipse);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "ellipse";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_ellipse_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_ellipse_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sa_ellipse_del(GF_Node *node)
{
	SVG_SA_ellipseElement *p = (SVG_SA_ellipseElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_ellipse_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 59:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 60:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 61:
			info->name = "rx";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_ellipseElement *)node)->rx;
			return GF_OK;
		case 62:
			info->name = "ry";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_ellipseElement *)node)->ry;
			return GF_OK;
		case 63:
			info->name = "cx";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_ellipseElement *)node)->cx;
			return GF_OK;
		case 64:
			info->name = "cy";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_ellipseElement *)node)->cy;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_ellipse_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	if(!strcmp("transform", name)) return 59;
	if(!strcmp("motionTransform", name)) return 60;
	if(!strcmp("rx", name)) return 61;
	if(!strcmp("ry", name)) return 62;
	if(!strcmp("cx", name)) return 63;
	if(!strcmp("cy", name)) return 64;
	return -1;
}

void *gf_svg_sa_new_font()
{
	SVG_SA_fontElement *p;
	GF_SAFEALLOC(p, SVG_SA_fontElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_font);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "font";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_font_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_font_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_font_del(GF_Node *node)
{
	SVG_SA_fontElement *p = (SVG_SA_fontElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_font_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = & ((SVG_SA_fontElement *)node)->horiz_adv_x;
			return GF_OK;
		case 8:
			info->name = "horiz-origin-x";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_fontElement *)node)->horiz_origin_x;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_font_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("horiz-adv-x", name)) return 7;
	if(!strcmp("horiz-origin-x", name)) return 8;
	return -1;
}

void *gf_svg_sa_new_font_face()
{
	SVG_SA_font_faceElement *p;
	GF_SAFEALLOC(p, SVG_SA_font_faceElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_font_face);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "font_face";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_font_face_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_font_face_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_font_face_del(GF_Node *node)
{
	SVG_SA_font_faceElement *p = (SVG_SA_font_faceElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->font_family.value) free(p->font_family.value);
	free(p->font_stretch);
	free(p->unicode_range);
	free(p->panose_1);
	free(p->widths);
	free(p->bbox);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_font_face_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->font_family;
			return GF_OK;
		case 8:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->font_style;
			return GF_OK;
		case 9:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->font_weight;
			return GF_OK;
		case 10:
			info->name = "font-variant";
			info->fieldType = SVG_FontVariant_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->font_variant;
			return GF_OK;
		case 11:
			info->name = "font-stretch";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->font_stretch;
			return GF_OK;
		case 12:
			info->name = "unicode-range";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->unicode_range;
			return GF_OK;
		case 13:
			info->name = "panose-1";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->panose_1;
			return GF_OK;
		case 14:
			info->name = "widths";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->widths;
			return GF_OK;
		case 15:
			info->name = "bbox";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->bbox;
			return GF_OK;
		case 16:
			info->name = "units-per-em";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->units_per_em;
			return GF_OK;
		case 17:
			info->name = "stemv";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->stemv;
			return GF_OK;
		case 18:
			info->name = "stemh";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->stemh;
			return GF_OK;
		case 19:
			info->name = "slope";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->slope;
			return GF_OK;
		case 20:
			info->name = "cap-height";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->cap_height;
			return GF_OK;
		case 21:
			info->name = "x-height";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->x_height;
			return GF_OK;
		case 22:
			info->name = "accent-height";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->accent_height;
			return GF_OK;
		case 23:
			info->name = "ascent";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->ascent;
			return GF_OK;
		case 24:
			info->name = "descent";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->descent;
			return GF_OK;
		case 25:
			info->name = "ideographic";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->ideographic;
			return GF_OK;
		case 26:
			info->name = "alphabetic";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->alphabetic;
			return GF_OK;
		case 27:
			info->name = "mathematical";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->mathematical;
			return GF_OK;
		case 28:
			info->name = "hanging";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->hanging;
			return GF_OK;
		case 29:
			info->name = "underline-position";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->underline_position;
			return GF_OK;
		case 30:
			info->name = "underline-thickness";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->underline_thickness;
			return GF_OK;
		case 31:
			info->name = "strikethrough-position";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->strikethrough_position;
			return GF_OK;
		case 32:
			info->name = "strikethrough-thickness";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->strikethrough_thickness;
			return GF_OK;
		case 33:
			info->name = "overline-position";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->overline_position;
			return GF_OK;
		case 34:
			info->name = "overline-thickness";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_font_faceElement *)node)->overline_thickness;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_font_face_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("font-family", name)) return 7;
	if(!strcmp("font-style", name)) return 8;
	if(!strcmp("font-weight", name)) return 9;
	if(!strcmp("font-variant", name)) return 10;
	if(!strcmp("font-stretch", name)) return 11;
	if(!strcmp("unicode-range", name)) return 12;
	if(!strcmp("panose-1", name)) return 13;
	if(!strcmp("widths", name)) return 14;
	if(!strcmp("bbox", name)) return 15;
	if(!strcmp("units-per-em", name)) return 16;
	if(!strcmp("stemv", name)) return 17;
	if(!strcmp("stemh", name)) return 18;
	if(!strcmp("slope", name)) return 19;
	if(!strcmp("cap-height", name)) return 20;
	if(!strcmp("x-height", name)) return 21;
	if(!strcmp("accent-height", name)) return 22;
	if(!strcmp("ascent", name)) return 23;
	if(!strcmp("descent", name)) return 24;
	if(!strcmp("ideographic", name)) return 25;
	if(!strcmp("alphabetic", name)) return 26;
	if(!strcmp("mathematical", name)) return 27;
	if(!strcmp("hanging", name)) return 28;
	if(!strcmp("underline-position", name)) return 29;
	if(!strcmp("underline-thickness", name)) return 30;
	if(!strcmp("strikethrough-position", name)) return 31;
	if(!strcmp("strikethrough-thickness", name)) return 32;
	if(!strcmp("overline-position", name)) return 33;
	if(!strcmp("overline-thickness", name)) return 34;
	return -1;
}

void *gf_svg_sa_new_font_face_src()
{
	SVG_SA_font_face_srcElement *p;
	GF_SAFEALLOC(p, SVG_SA_font_face_srcElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_font_face_src);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "font_face_src";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_font_face_src_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_font_face_src_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_font_face_src_del(GF_Node *node)
{
	SVG_SA_font_face_srcElement *p = (SVG_SA_font_face_srcElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_font_face_src_get_attribute(GF_Node *node, GF_FieldInfo *info)
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

s32 gf_svg_sa_font_face_src_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	return -1;
}

void *gf_svg_sa_new_font_face_uri()
{
	SVG_SA_font_face_uriElement *p;
	GF_SAFEALLOC(p, SVG_SA_font_face_uriElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_font_face_uri);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "font_face_uri";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_font_face_uri_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_font_face_uri_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_font_face_uri_del(GF_Node *node)
{
	SVG_SA_font_face_uriElement *p = (SVG_SA_font_face_uriElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_font_face_uri_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_font_face_uri_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("xlink:href", name)) return 7;
	if(!strcmp("xlink:show", name)) return 8;
	if(!strcmp("xlink:title", name)) return 9;
	if(!strcmp("xlink:actuate", name)) return 10;
	if(!strcmp("xlink:role", name)) return 11;
	if(!strcmp("xlink:arcrole", name)) return 12;
	if(!strcmp("xlink:type", name)) return 13;
	return -1;
}

void *gf_svg_sa_new_foreignObject()
{
	SVG_SA_foreignObjectElement *p;
	GF_SAFEALLOC(p, SVG_SA_foreignObjectElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_foreignObject);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "foreignObject";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_foreignObject_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_foreignObject_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sa_foreignObject_del(GF_Node *node)
{
	SVG_SA_foreignObjectElement *p = (SVG_SA_foreignObjectElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_foreignObject_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 55:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 56:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 57:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 58:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 59:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 60:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 61:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 62:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 63:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 64:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 65:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 66:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 67:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 68:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_foreignObjectElement *)node)->x;
			return GF_OK;
		case 69:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_foreignObjectElement *)node)->y;
			return GF_OK;
		case 70:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_foreignObjectElement *)node)->width;
			return GF_OK;
		case 71:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_foreignObjectElement *)node)->height;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_foreignObject_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("xlink:href", name)) return 54;
	if(!strcmp("xlink:show", name)) return 55;
	if(!strcmp("xlink:title", name)) return 56;
	if(!strcmp("xlink:actuate", name)) return 57;
	if(!strcmp("xlink:role", name)) return 58;
	if(!strcmp("xlink:arcrole", name)) return 59;
	if(!strcmp("xlink:type", name)) return 60;
	if(!strcmp("requiredExtensions", name)) return 61;
	if(!strcmp("requiredFeatures", name)) return 62;
	if(!strcmp("requiredFonts", name)) return 63;
	if(!strcmp("requiredFormats", name)) return 64;
	if(!strcmp("systemLanguage", name)) return 65;
	if(!strcmp("transform", name)) return 66;
	if(!strcmp("motionTransform", name)) return 67;
	if(!strcmp("x", name)) return 68;
	if(!strcmp("y", name)) return 69;
	if(!strcmp("width", name)) return 70;
	if(!strcmp("height", name)) return 71;
	return -1;
}

void *gf_svg_sa_new_g()
{
	SVG_SA_gElement *p;
	GF_SAFEALLOC(p, SVG_SA_gElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_g);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "g";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_g_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_g_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sa_g_del(GF_Node *node)
{
	SVG_SA_gElement *p = (SVG_SA_gElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_g_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 59:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 60:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_g_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	if(!strcmp("transform", name)) return 59;
	if(!strcmp("motionTransform", name)) return 60;
	return -1;
}

void *gf_svg_sa_new_glyph()
{
	SVG_SA_glyphElement *p;
	GF_SAFEALLOC(p, SVG_SA_glyphElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_glyph);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "glyph";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_glyph_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_glyph_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
#ifdef USE_GF_PATH
	gf_path_reset(&p->d);
#else
	p->d.commands = gf_list_new();
	p->d.points = gf_list_new();
#endif
	return p;
}

static void gf_svg_sa_glyph_del(GF_Node *node)
{
	SVG_SA_glyphElement *p = (SVG_SA_glyphElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_svg_reset_path(p->d);
	free(p->unicode);
	free(p->glyph_name);
	free(p->arabic_form);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_glyph_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = & ((SVG_SA_glyphElement *)node)->horiz_adv_x;
			return GF_OK;
		case 8:
			info->name = "d";
			info->fieldType = SVG_PathData_datatype;
			info->far_ptr = & ((SVG_SA_glyphElement *)node)->d;
			return GF_OK;
		case 9:
			info->name = "unicode";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_glyphElement *)node)->unicode;
			return GF_OK;
		case 10:
			info->name = "glyph-name";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_glyphElement *)node)->glyph_name;
			return GF_OK;
		case 11:
			info->name = "arabic-form";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_glyphElement *)node)->arabic_form;
			return GF_OK;
		case 12:
			info->name = "lang";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVG_SA_glyphElement *)node)->lang;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_glyph_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("horiz-adv-x", name)) return 7;
	if(!strcmp("d", name)) return 8;
	if(!strcmp("unicode", name)) return 9;
	if(!strcmp("glyph-name", name)) return 10;
	if(!strcmp("arabic-form", name)) return 11;
	if(!strcmp("lang", name)) return 12;
	return -1;
}

void *gf_svg_sa_new_handler()
{
	SVG_SA_handlerElement *p;
	GF_SAFEALLOC(p, SVG_SA_handlerElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_handler);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "handler";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_handler_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_handler_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_handler_del(GF_Node *node)
{
	SVG_SA_handlerElement *p = (SVG_SA_handlerElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	free(p->type);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_handler_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = & ((SVG_SA_handlerElement *)node)->type;
			return GF_OK;
		case 8:
			info->name = "ev:event";
			info->fieldType = XMLEV_Event_datatype;
			info->far_ptr = & ((SVG_SA_handlerElement *)node)->ev_event;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_handler_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("type", name)) return 7;
	if(!strcmp("ev:event", name)) return 8;
	return -1;
}

void *gf_svg_sa_new_hkern()
{
	SVG_SA_hkernElement *p;
	GF_SAFEALLOC(p, SVG_SA_hkernElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_hkern);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "hkern";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_hkern_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_hkern_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_hkern_del(GF_Node *node)
{
	SVG_SA_hkernElement *p = (SVG_SA_hkernElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	free(p->u1);
	free(p->g1);
	free(p->u2);
	free(p->g2);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_hkern_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = & ((SVG_SA_hkernElement *)node)->u1;
			return GF_OK;
		case 8:
			info->name = "g1";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_hkernElement *)node)->g1;
			return GF_OK;
		case 9:
			info->name = "u2";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_hkernElement *)node)->u2;
			return GF_OK;
		case 10:
			info->name = "g2";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_hkernElement *)node)->g2;
			return GF_OK;
		case 11:
			info->name = "k";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_hkernElement *)node)->k;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_hkern_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("u1", name)) return 7;
	if(!strcmp("g1", name)) return 8;
	if(!strcmp("u2", name)) return 9;
	if(!strcmp("g2", name)) return 10;
	if(!strcmp("k", name)) return 11;
	return -1;
}

void *gf_svg_sa_new_image()
{
	SVG_SA_imageElement *p;
	GF_SAFEALLOC(p, SVG_SA_imageElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_image);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "image";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_image_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_image_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sa_image_del(GF_Node *node)
{
	SVG_SA_imageElement *p = (SVG_SA_imageElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	free(p->type);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_image_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->opacity;
			return GF_OK;
		case 17:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 18:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 19:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 20:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 21:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 22:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 23:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 24:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 25:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 26:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 27:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 28:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 29:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 30:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 31:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 32:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 33:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 34:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 35:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 36:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 37:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 38:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 39:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 40:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 41:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 42:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 43:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_imageElement *)node)->x;
			return GF_OK;
		case 44:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_imageElement *)node)->y;
			return GF_OK;
		case 45:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_imageElement *)node)->width;
			return GF_OK;
		case 46:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_imageElement *)node)->height;
			return GF_OK;
		case 47:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVG_SA_imageElement *)node)->preserveAspectRatio;
			return GF_OK;
		case 48:
			info->name = "type";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVG_SA_imageElement *)node)->type;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_image_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("opacity", name)) return 16;
	if(!strcmp("focusHighlight", name)) return 17;
	if(!strcmp("focusable", name)) return 18;
	if(!strcmp("nav-down", name)) return 19;
	if(!strcmp("nav-down-left", name)) return 20;
	if(!strcmp("nav-down-right", name)) return 21;
	if(!strcmp("nav-left", name)) return 22;
	if(!strcmp("nav-next", name)) return 23;
	if(!strcmp("nav-prev", name)) return 24;
	if(!strcmp("nav-right", name)) return 25;
	if(!strcmp("nav-up", name)) return 26;
	if(!strcmp("nav-up-left", name)) return 27;
	if(!strcmp("nav-up-right", name)) return 28;
	if(!strcmp("xlink:href", name)) return 29;
	if(!strcmp("xlink:show", name)) return 30;
	if(!strcmp("xlink:title", name)) return 31;
	if(!strcmp("xlink:actuate", name)) return 32;
	if(!strcmp("xlink:role", name)) return 33;
	if(!strcmp("xlink:arcrole", name)) return 34;
	if(!strcmp("xlink:type", name)) return 35;
	if(!strcmp("requiredExtensions", name)) return 36;
	if(!strcmp("requiredFeatures", name)) return 37;
	if(!strcmp("requiredFonts", name)) return 38;
	if(!strcmp("requiredFormats", name)) return 39;
	if(!strcmp("systemLanguage", name)) return 40;
	if(!strcmp("transform", name)) return 41;
	if(!strcmp("motionTransform", name)) return 42;
	if(!strcmp("x", name)) return 43;
	if(!strcmp("y", name)) return 44;
	if(!strcmp("width", name)) return 45;
	if(!strcmp("height", name)) return 46;
	if(!strcmp("preserveAspectRatio", name)) return 47;
	if(!strcmp("type", name)) return 48;
	return -1;
}

void *gf_svg_sa_new_line()
{
	SVG_SA_lineElement *p;
	GF_SAFEALLOC(p, SVG_SA_lineElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_line);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "line";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_line_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_line_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sa_line_del(GF_Node *node)
{
	SVG_SA_lineElement *p = (SVG_SA_lineElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_line_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 59:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 60:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 61:
			info->name = "x1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_lineElement *)node)->x1;
			return GF_OK;
		case 62:
			info->name = "y1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_lineElement *)node)->y1;
			return GF_OK;
		case 63:
			info->name = "x2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_lineElement *)node)->x2;
			return GF_OK;
		case 64:
			info->name = "y2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_lineElement *)node)->y2;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_line_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	if(!strcmp("transform", name)) return 59;
	if(!strcmp("motionTransform", name)) return 60;
	if(!strcmp("x1", name)) return 61;
	if(!strcmp("y1", name)) return 62;
	if(!strcmp("x2", name)) return 63;
	if(!strcmp("y2", name)) return 64;
	return -1;
}

void *gf_svg_sa_new_linearGradient()
{
	SVG_SA_linearGradientElement *p;
	GF_SAFEALLOC(p, SVG_SA_linearGradientElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_linearGradient);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "linearGradient";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_linearGradient_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_linearGradient_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	p->x2.value = FIX_ONE;
	gf_mx2d_init(p->gradientTransform.mat);
	return p;
}

static void gf_svg_sa_linearGradient_del(GF_Node *node)
{
	SVG_SA_linearGradientElement *p = (SVG_SA_linearGradientElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_linearGradient_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 43:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 44:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 45:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 46:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 47:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 48:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 49:
			info->name = "gradientUnits";
			info->fieldType = SVG_GradientUnit_datatype;
			info->far_ptr = & ((SVG_SA_linearGradientElement *)node)->gradientUnits;
			return GF_OK;
		case 50:
			info->name = "spreadMethod";
			info->fieldType = SVG_SpreadMethod_datatype;
			info->far_ptr = & ((SVG_SA_linearGradientElement *)node)->spreadMethod;
			return GF_OK;
		case 51:
			info->name = "gradientTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = & ((SVG_SA_linearGradientElement *)node)->gradientTransform;
			return GF_OK;
		case 52:
			info->name = "x1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_linearGradientElement *)node)->x1;
			return GF_OK;
		case 53:
			info->name = "y1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_linearGradientElement *)node)->y1;
			return GF_OK;
		case 54:
			info->name = "x2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_linearGradientElement *)node)->x2;
			return GF_OK;
		case 55:
			info->name = "y2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_linearGradientElement *)node)->y2;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_linearGradient_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("xlink:href", name)) return 42;
	if(!strcmp("xlink:show", name)) return 43;
	if(!strcmp("xlink:title", name)) return 44;
	if(!strcmp("xlink:actuate", name)) return 45;
	if(!strcmp("xlink:role", name)) return 46;
	if(!strcmp("xlink:arcrole", name)) return 47;
	if(!strcmp("xlink:type", name)) return 48;
	if(!strcmp("gradientUnits", name)) return 49;
	if(!strcmp("spreadMethod", name)) return 50;
	if(!strcmp("gradientTransform", name)) return 51;
	if(!strcmp("x1", name)) return 52;
	if(!strcmp("y1", name)) return 53;
	if(!strcmp("x2", name)) return 54;
	if(!strcmp("y2", name)) return 55;
	return -1;
}

void *gf_svg_sa_new_listener()
{
	SVG_SA_listenerElement *p;
	GF_SAFEALLOC(p, SVG_SA_listenerElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_listener);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "listener";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_listener_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_listener_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_listener_del(GF_Node *node)
{
	SVG_SA_listenerElement *p = (SVG_SA_listenerElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_svg_reset_iri(node->sgprivate->scenegraph, &p->observer);
	gf_svg_reset_iri(node->sgprivate->scenegraph, &p->target);
	gf_svg_reset_iri(node->sgprivate->scenegraph, &p->handler);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_listener_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = & ((SVG_SA_listenerElement *)node)->event;
			return GF_OK;
		case 8:
			info->name = "phase";
			info->fieldType = XMLEV_Phase_datatype;
			info->far_ptr = & ((SVG_SA_listenerElement *)node)->phase;
			return GF_OK;
		case 9:
			info->name = "propagate";
			info->fieldType = XMLEV_Propagate_datatype;
			info->far_ptr = & ((SVG_SA_listenerElement *)node)->propagate;
			return GF_OK;
		case 10:
			info->name = "defaultAction";
			info->fieldType = XMLEV_DefaultAction_datatype;
			info->far_ptr = & ((SVG_SA_listenerElement *)node)->defaultAction;
			return GF_OK;
		case 11:
			info->name = "observer";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVG_SA_listenerElement *)node)->observer;
			return GF_OK;
		case 12:
			info->name = "target";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVG_SA_listenerElement *)node)->target;
			return GF_OK;
		case 13:
			info->name = "handler";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVG_SA_listenerElement *)node)->handler;
			return GF_OK;
		case 14:
			info->name = "enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVG_SA_listenerElement *)node)->enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_listener_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("event", name)) return 7;
	if(!strcmp("phase", name)) return 8;
	if(!strcmp("propagate", name)) return 9;
	if(!strcmp("defaultAction", name)) return 10;
	if(!strcmp("observer", name)) return 11;
	if(!strcmp("target", name)) return 12;
	if(!strcmp("handler", name)) return 13;
	if(!strcmp("enabled", name)) return 14;
	return -1;
}

void *gf_svg_sa_new_metadata()
{
	SVG_SA_metadataElement *p;
	GF_SAFEALLOC(p, SVG_SA_metadataElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_metadata);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "metadata";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_metadata_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_metadata_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_metadata_del(GF_Node *node)
{
	SVG_SA_metadataElement *p = (SVG_SA_metadataElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_metadata_get_attribute(GF_Node *node, GF_FieldInfo *info)
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

s32 gf_svg_sa_metadata_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	return -1;
}

void *gf_svg_sa_new_missing_glyph()
{
	SVG_SA_missing_glyphElement *p;
	GF_SAFEALLOC(p, SVG_SA_missing_glyphElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_missing_glyph);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "missing_glyph";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_missing_glyph_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_missing_glyph_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
#ifdef USE_GF_PATH
	gf_path_reset(&p->d);
#else
	p->d.commands = gf_list_new();
	p->d.points = gf_list_new();
#endif
	return p;
}

static void gf_svg_sa_missing_glyph_del(GF_Node *node)
{
	SVG_SA_missing_glyphElement *p = (SVG_SA_missing_glyphElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_svg_reset_path(p->d);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_missing_glyph_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = & ((SVG_SA_missing_glyphElement *)node)->horiz_adv_x;
			return GF_OK;
		case 8:
			info->name = "d";
			info->fieldType = SVG_PathData_datatype;
			info->far_ptr = & ((SVG_SA_missing_glyphElement *)node)->d;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_missing_glyph_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("horiz-adv-x", name)) return 7;
	if(!strcmp("d", name)) return 8;
	return -1;
}

void *gf_svg_sa_new_mpath()
{
	SVG_SA_mpathElement *p;
	GF_SAFEALLOC(p, SVG_SA_mpathElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_mpath);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "mpath";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_mpath_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_mpath_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_mpath_del(GF_Node *node)
{
	SVG_SA_mpathElement *p = (SVG_SA_mpathElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_mpath_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_mpath_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("xlink:href", name)) return 7;
	if(!strcmp("xlink:show", name)) return 8;
	if(!strcmp("xlink:title", name)) return 9;
	if(!strcmp("xlink:actuate", name)) return 10;
	if(!strcmp("xlink:role", name)) return 11;
	if(!strcmp("xlink:arcrole", name)) return 12;
	if(!strcmp("xlink:type", name)) return 13;
	return -1;
}

void *gf_svg_sa_new_path()
{
	SVG_SA_pathElement *p;
	GF_SAFEALLOC(p, SVG_SA_pathElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_path);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "path";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_path_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_path_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
#ifdef USE_GF_PATH
	gf_path_reset(&p->d);
#else
	p->d.commands = gf_list_new();
	p->d.points = gf_list_new();
#endif
	return p;
}

static void gf_svg_sa_path_del(GF_Node *node)
{
	SVG_SA_pathElement *p = (SVG_SA_pathElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_svg_reset_path(p->d);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_path_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 59:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 60:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 61:
			info->name = "d";
			info->fieldType = SVG_PathData_datatype;
			info->far_ptr = & ((SVG_SA_pathElement *)node)->d;
			return GF_OK;
		case 62:
			info->name = "pathLength";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_pathElement *)node)->pathLength;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_path_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	if(!strcmp("transform", name)) return 59;
	if(!strcmp("motionTransform", name)) return 60;
	if(!strcmp("d", name)) return 61;
	if(!strcmp("pathLength", name)) return 62;
	return -1;
}

void *gf_svg_sa_new_polygon()
{
	SVG_SA_polygonElement *p;
	GF_SAFEALLOC(p, SVG_SA_polygonElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_polygon);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "polygon";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_polygon_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_polygon_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->points = gf_list_new();
	return p;
}

static void gf_svg_sa_polygon_del(GF_Node *node)
{
	SVG_SA_polygonElement *p = (SVG_SA_polygonElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_svg_delete_points(p->points);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_polygon_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 59:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 60:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 61:
			info->name = "points";
			info->fieldType = SVG_Points_datatype;
			info->far_ptr = & ((SVG_SA_polygonElement *)node)->points;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_polygon_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	if(!strcmp("transform", name)) return 59;
	if(!strcmp("motionTransform", name)) return 60;
	if(!strcmp("points", name)) return 61;
	return -1;
}

void *gf_svg_sa_new_polyline()
{
	SVG_SA_polylineElement *p;
	GF_SAFEALLOC(p, SVG_SA_polylineElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_polyline);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "polyline";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_polyline_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_polyline_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->points = gf_list_new();
	return p;
}

static void gf_svg_sa_polyline_del(GF_Node *node)
{
	SVG_SA_polylineElement *p = (SVG_SA_polylineElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_svg_delete_points(p->points);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_polyline_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 59:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 60:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 61:
			info->name = "points";
			info->fieldType = SVG_Points_datatype;
			info->far_ptr = & ((SVG_SA_polylineElement *)node)->points;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_polyline_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	if(!strcmp("transform", name)) return 59;
	if(!strcmp("motionTransform", name)) return 60;
	if(!strcmp("points", name)) return 61;
	return -1;
}

void *gf_svg_sa_new_prefetch()
{
	SVG_SA_prefetchElement *p;
	GF_SAFEALLOC(p, SVG_SA_prefetchElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_prefetch);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "prefetch";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_prefetch_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_prefetch_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_prefetch_del(GF_Node *node)
{
	SVG_SA_prefetchElement *p = (SVG_SA_prefetchElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	free(p->mediaTime);
	free(p->mediaCharacterEncoding);
	free(p->mediaContentEncodings);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_prefetch_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "mediaSize";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_prefetchElement *)node)->mediaSize;
			return GF_OK;
		case 15:
			info->name = "mediaTime";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_prefetchElement *)node)->mediaTime;
			return GF_OK;
		case 16:
			info->name = "mediaCharacterEncoding";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_prefetchElement *)node)->mediaCharacterEncoding;
			return GF_OK;
		case 17:
			info->name = "mediaContentEncodings";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_prefetchElement *)node)->mediaContentEncodings;
			return GF_OK;
		case 18:
			info->name = "bandwidth";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_prefetchElement *)node)->bandwidth;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_prefetch_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("xlink:href", name)) return 7;
	if(!strcmp("xlink:show", name)) return 8;
	if(!strcmp("xlink:title", name)) return 9;
	if(!strcmp("xlink:actuate", name)) return 10;
	if(!strcmp("xlink:role", name)) return 11;
	if(!strcmp("xlink:arcrole", name)) return 12;
	if(!strcmp("xlink:type", name)) return 13;
	if(!strcmp("mediaSize", name)) return 14;
	if(!strcmp("mediaTime", name)) return 15;
	if(!strcmp("mediaCharacterEncoding", name)) return 16;
	if(!strcmp("mediaContentEncodings", name)) return 17;
	if(!strcmp("bandwidth", name)) return 18;
	return -1;
}

void *gf_svg_sa_new_radialGradient()
{
	SVG_SA_radialGradientElement *p;
	GF_SAFEALLOC(p, SVG_SA_radialGradientElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_radialGradient);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "radialGradient";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_radialGradient_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_radialGradient_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	p->cx.value = FIX_ONE/2;
	p->cy.value = FIX_ONE/2;
	p->r.value = FIX_ONE/2;
	gf_mx2d_init(p->gradientTransform.mat);
	p->fx.value = FIX_ONE/2;
	p->fy.value = FIX_ONE/2;
	return p;
}

static void gf_svg_sa_radialGradient_del(GF_Node *node)
{
	SVG_SA_radialGradientElement *p = (SVG_SA_radialGradientElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_radialGradient_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 43:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 44:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 45:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 46:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 47:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 48:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 49:
			info->name = "gradientUnits";
			info->fieldType = SVG_GradientUnit_datatype;
			info->far_ptr = & ((SVG_SA_radialGradientElement *)node)->gradientUnits;
			return GF_OK;
		case 50:
			info->name = "spreadMethod";
			info->fieldType = SVG_SpreadMethod_datatype;
			info->far_ptr = & ((SVG_SA_radialGradientElement *)node)->spreadMethod;
			return GF_OK;
		case 51:
			info->name = "gradientTransform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = & ((SVG_SA_radialGradientElement *)node)->gradientTransform;
			return GF_OK;
		case 52:
			info->name = "cx";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_radialGradientElement *)node)->cx;
			return GF_OK;
		case 53:
			info->name = "cy";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_radialGradientElement *)node)->cy;
			return GF_OK;
		case 54:
			info->name = "r";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_radialGradientElement *)node)->r;
			return GF_OK;
		case 55:
			info->name = "fx";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_radialGradientElement *)node)->fx;
			return GF_OK;
		case 56:
			info->name = "fy";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_radialGradientElement *)node)->fy;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_radialGradient_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("xlink:href", name)) return 42;
	if(!strcmp("xlink:show", name)) return 43;
	if(!strcmp("xlink:title", name)) return 44;
	if(!strcmp("xlink:actuate", name)) return 45;
	if(!strcmp("xlink:role", name)) return 46;
	if(!strcmp("xlink:arcrole", name)) return 47;
	if(!strcmp("xlink:type", name)) return 48;
	if(!strcmp("gradientUnits", name)) return 49;
	if(!strcmp("spreadMethod", name)) return 50;
	if(!strcmp("gradientTransform", name)) return 51;
	if(!strcmp("cx", name)) return 52;
	if(!strcmp("cy", name)) return 53;
	if(!strcmp("r", name)) return 54;
	if(!strcmp("fx", name)) return 55;
	if(!strcmp("fy", name)) return 56;
	return -1;
}

void *gf_svg_sa_new_rect()
{
	SVG_SA_rectElement *p;
	GF_SAFEALLOC(p, SVG_SA_rectElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_rect);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "rect";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_rect_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_rect_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sa_rect_del(GF_Node *node)
{
	SVG_SA_rectElement *p = (SVG_SA_rectElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_rect_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 59:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 60:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 61:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_rectElement *)node)->x;
			return GF_OK;
		case 62:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_rectElement *)node)->y;
			return GF_OK;
		case 63:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_rectElement *)node)->width;
			return GF_OK;
		case 64:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_rectElement *)node)->height;
			return GF_OK;
		case 65:
			info->name = "rx";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_rectElement *)node)->rx;
			return GF_OK;
		case 66:
			info->name = "ry";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_rectElement *)node)->ry;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_rect_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	if(!strcmp("transform", name)) return 59;
	if(!strcmp("motionTransform", name)) return 60;
	if(!strcmp("x", name)) return 61;
	if(!strcmp("y", name)) return 62;
	if(!strcmp("width", name)) return 63;
	if(!strcmp("height", name)) return 64;
	if(!strcmp("rx", name)) return 65;
	if(!strcmp("ry", name)) return 66;
	return -1;
}

void *gf_svg_sa_new_rectClip()
{
	SVG_SA_rectClipElement *p;
	GF_SAFEALLOC(p, SVG_SA_rectClipElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_rectClip);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "rectClip";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_rectClip_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_rectClip_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sa_rectClip_del(GF_Node *node)
{
	SVG_SA_rectClipElement *p = (SVG_SA_rectClipElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_rectClip_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 59:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 60:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 61:
			info->name = "size";
			info->fieldType = LASeR_Size_datatype;
			info->far_ptr = & ((SVG_SA_rectClipElement *)node)->size;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_rectClip_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	if(!strcmp("transform", name)) return 59;
	if(!strcmp("motionTransform", name)) return 60;
	if(!strcmp("size", name)) return 61;
	return -1;
}

void *gf_svg_sa_new_script()
{
	SVG_SA_scriptElement *p;
	GF_SAFEALLOC(p, SVG_SA_scriptElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_script);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "script";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_script_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_script_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_script_del(GF_Node *node)
{
	SVG_SA_scriptElement *p = (SVG_SA_scriptElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	free(p->type);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_script_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = & ((SVG_SA_scriptElement *)node)->type;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_script_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("type", name)) return 7;
	return -1;
}

void *gf_svg_sa_new_selector()
{
	SVG_SA_selectorElement *p;
	GF_SAFEALLOC(p, SVG_SA_selectorElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_selector);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "selector";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_selector_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_selector_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sa_selector_del(GF_Node *node)
{
	SVG_SA_selectorElement *p = (SVG_SA_selectorElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_selector_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 59:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 60:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 61:
			info->name = "choice";
			info->fieldType = LASeR_Choice_datatype;
			info->far_ptr = & ((SVG_SA_selectorElement *)node)->choice;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_selector_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	if(!strcmp("transform", name)) return 59;
	if(!strcmp("motionTransform", name)) return 60;
	if(!strcmp("choice", name)) return 61;
	return -1;
}

void *gf_svg_sa_new_set()
{
	SVG_SA_setElement *p;
	GF_SAFEALLOC(p, SVG_SA_setElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_set);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "set";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_set_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_set_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	gf_svg_sa_init_timing((SVG_SA_Element *)p);
	gf_svg_sa_init_anim((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_set_del(GF_Node *node)
{
	SVG_SA_setElement *p = (SVG_SA_setElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_set_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->attributeName;
			return GF_OK;
		case 24:
			info->name = "attributeType";
			info->fieldType = SMIL_AttributeType_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->attributeType;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->to;
			return GF_OK;
		case 26:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->anim->lsr_enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_set_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("xlink:href", name)) return 7;
	if(!strcmp("xlink:show", name)) return 8;
	if(!strcmp("xlink:title", name)) return 9;
	if(!strcmp("xlink:actuate", name)) return 10;
	if(!strcmp("xlink:role", name)) return 11;
	if(!strcmp("xlink:arcrole", name)) return 12;
	if(!strcmp("xlink:type", name)) return 13;
	if(!strcmp("begin", name)) return 14;
	if(!strcmp("end", name)) return 15;
	if(!strcmp("dur", name)) return 16;
	if(!strcmp("repeatCount", name)) return 17;
	if(!strcmp("repeatDur", name)) return 18;
	if(!strcmp("restart", name)) return 19;
	if(!strcmp("min", name)) return 20;
	if(!strcmp("max", name)) return 21;
	if(!strcmp("fill", name)) return 22;
	if(!strcmp("attributeName", name)) return 23;
	if(!strcmp("attributeType", name)) return 24;
	if(!strcmp("to", name)) return 25;
	if(!strcmp("lsr:enabled", name)) return 26;
	return -1;
}

void *gf_svg_sa_new_simpleLayout()
{
	SVG_SA_simpleLayoutElement *p;
	GF_SAFEALLOC(p, SVG_SA_simpleLayoutElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_simpleLayout);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "simpleLayout";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_simpleLayout_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_simpleLayout_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sa_simpleLayout_del(GF_Node *node)
{
	SVG_SA_simpleLayoutElement *p = (SVG_SA_simpleLayoutElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_simpleLayout_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 59:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 60:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 61:
			info->name = "delta";
			info->fieldType = LASeR_Size_datatype;
			info->far_ptr = & ((SVG_SA_simpleLayoutElement *)node)->delta;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_simpleLayout_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	if(!strcmp("transform", name)) return 59;
	if(!strcmp("motionTransform", name)) return 60;
	if(!strcmp("delta", name)) return 61;
	return -1;
}

void *gf_svg_sa_new_solidColor()
{
	SVG_SA_solidColorElement *p;
	GF_SAFEALLOC(p, SVG_SA_solidColorElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_solidColor);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "solidColor";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_solidColor_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_solidColor_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	p->properties->solid_opacity.value = FIX_ONE;
	return p;
}

static void gf_svg_sa_solidColor_del(GF_Node *node)
{
	SVG_SA_solidColorElement *p = (SVG_SA_solidColorElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_solidColor_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_solidColor_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	return -1;
}

void *gf_svg_sa_new_stop()
{
	SVG_SA_stopElement *p;
	GF_SAFEALLOC(p, SVG_SA_stopElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_stop);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "stop";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_stop_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_stop_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	p->properties->stop_opacity.value = FIX_ONE;
	return p;
}

static void gf_svg_sa_stop_del(GF_Node *node)
{
	SVG_SA_stopElement *p = (SVG_SA_stopElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_stop_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "offset";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVG_SA_stopElement *)node)->offset;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_stop_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("offset", name)) return 42;
	return -1;
}

void *gf_svg_sa_new_svg()
{
	SVG_SA_svgElement *p;
	GF_SAFEALLOC(p, SVG_SA_svgElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_svg);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "svg";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_svg_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_svg_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_sync((SVG_SA_Element *)p);
	p->width.type = SVG_NUMBER_PERCENTAGE;
	p->width.value = INT2FIX(100);
	p->height.type = SVG_NUMBER_PERCENTAGE;
	p->height.value = INT2FIX(100);
	return p;
}

static void gf_svg_sa_svg_del(GF_Node *node)
{
	SVG_SA_svgElement *p = (SVG_SA_svgElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	free(p->version);
	free(p->baseProfile);
	free(p->contentScriptType);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_svg_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "syncBehaviorDefault";
			info->fieldType = SMIL_SyncBehavior_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->sync->syncBehaviorDefault;
			return GF_OK;
		case 55:
			info->name = "syncToleranceDefault";
			info->fieldType = SMIL_SyncTolerance_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->sync->syncToleranceDefault;
			return GF_OK;
		case 56:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_svgElement *)node)->x;
			return GF_OK;
		case 57:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_svgElement *)node)->y;
			return GF_OK;
		case 58:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_svgElement *)node)->width;
			return GF_OK;
		case 59:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_svgElement *)node)->height;
			return GF_OK;
		case 60:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVG_SA_svgElement *)node)->preserveAspectRatio;
			return GF_OK;
		case 61:
			info->name = "viewBox";
			info->fieldType = SVG_ViewBox_datatype;
			info->far_ptr = & ((SVG_SA_svgElement *)node)->viewBox;
			return GF_OK;
		case 62:
			info->name = "zoomAndPan";
			info->fieldType = SVG_ZoomAndPan_datatype;
			info->far_ptr = & ((SVG_SA_svgElement *)node)->zoomAndPan;
			return GF_OK;
		case 63:
			info->name = "version";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_svgElement *)node)->version;
			return GF_OK;
		case 64:
			info->name = "baseProfile";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVG_SA_svgElement *)node)->baseProfile;
			return GF_OK;
		case 65:
			info->name = "contentScriptType";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVG_SA_svgElement *)node)->contentScriptType;
			return GF_OK;
		case 66:
			info->name = "snapshotTime";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = & ((SVG_SA_svgElement *)node)->snapshotTime;
			return GF_OK;
		case 67:
			info->name = "timelineBegin";
			info->fieldType = SVG_TimelineBegin_datatype;
			info->far_ptr = & ((SVG_SA_svgElement *)node)->timelineBegin;
			return GF_OK;
		case 68:
			info->name = "playbackOrder";
			info->fieldType = SVG_PlaybackOrder_datatype;
			info->far_ptr = & ((SVG_SA_svgElement *)node)->playbackOrder;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_svg_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("syncBehaviorDefault", name)) return 54;
	if(!strcmp("syncToleranceDefault", name)) return 55;
	if(!strcmp("x", name)) return 56;
	if(!strcmp("y", name)) return 57;
	if(!strcmp("width", name)) return 58;
	if(!strcmp("height", name)) return 59;
	if(!strcmp("preserveAspectRatio", name)) return 60;
	if(!strcmp("viewBox", name)) return 61;
	if(!strcmp("zoomAndPan", name)) return 62;
	if(!strcmp("version", name)) return 63;
	if(!strcmp("baseProfile", name)) return 64;
	if(!strcmp("contentScriptType", name)) return 65;
	if(!strcmp("snapshotTime", name)) return 66;
	if(!strcmp("timelineBegin", name)) return 67;
	if(!strcmp("playbackOrder", name)) return 68;
	return -1;
}

void *gf_svg_sa_new_switch()
{
	SVG_SA_switchElement *p;
	GF_SAFEALLOC(p, SVG_SA_switchElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_switch);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "switch";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_switch_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_switch_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sa_switch_del(GF_Node *node)
{
	SVG_SA_switchElement *p = (SVG_SA_switchElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_switch_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 59:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 60:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_switch_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	if(!strcmp("transform", name)) return 59;
	if(!strcmp("motionTransform", name)) return 60;
	return -1;
}

void *gf_svg_sa_new_tbreak()
{
	SVG_SA_tbreakElement *p;
	GF_SAFEALLOC(p, SVG_SA_tbreakElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_tbreak);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "tbreak";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_tbreak_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_tbreak_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_tbreak_del(GF_Node *node)
{
	SVG_SA_tbreakElement *p = (SVG_SA_tbreakElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_tbreak_get_attribute(GF_Node *node, GF_FieldInfo *info)
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

s32 gf_svg_sa_tbreak_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	return -1;
}

void *gf_svg_sa_new_text()
{
	SVG_SA_textElement *p;
	GF_SAFEALLOC(p, SVG_SA_textElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_text);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "text";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_text_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_text_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->x = gf_list_new();
	p->y = gf_list_new();
	return p;
}

static void gf_svg_sa_text_del(GF_Node *node)
{
	SVG_SA_textElement *p = (SVG_SA_textElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_svg_delete_coordinates(p->x);
	gf_svg_delete_coordinates(p->y);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_text_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 59:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 60:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 61:
			info->name = "editable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVG_SA_textElement *)node)->editable;
			return GF_OK;
		case 62:
			info->name = "x";
			info->fieldType = SVG_Coordinates_datatype;
			info->far_ptr = & ((SVG_SA_textElement *)node)->x;
			return GF_OK;
		case 63:
			info->name = "y";
			info->fieldType = SVG_Coordinates_datatype;
			info->far_ptr = & ((SVG_SA_textElement *)node)->y;
			return GF_OK;
		case 64:
			info->name = "rotate";
			info->fieldType = SVG_Numbers_datatype;
			info->far_ptr = & ((SVG_SA_textElement *)node)->rotate;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_text_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	if(!strcmp("transform", name)) return 59;
	if(!strcmp("motionTransform", name)) return 60;
	if(!strcmp("editable", name)) return 61;
	if(!strcmp("x", name)) return 62;
	if(!strcmp("y", name)) return 63;
	if(!strcmp("rotate", name)) return 64;
	return -1;
}

void *gf_svg_sa_new_textArea()
{
	SVG_SA_textAreaElement *p;
	GF_SAFEALLOC(p, SVG_SA_textAreaElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_textArea);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "textArea";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_textArea_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_textArea_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sa_textArea_del(GF_Node *node)
{
	SVG_SA_textAreaElement *p = (SVG_SA_textAreaElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_textArea_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 59:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 60:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 61:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_textAreaElement *)node)->x;
			return GF_OK;
		case 62:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_textAreaElement *)node)->y;
			return GF_OK;
		case 63:
			info->name = "editable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVG_SA_textAreaElement *)node)->editable;
			return GF_OK;
		case 64:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_textAreaElement *)node)->width;
			return GF_OK;
		case 65:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_textAreaElement *)node)->height;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_textArea_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	if(!strcmp("transform", name)) return 59;
	if(!strcmp("motionTransform", name)) return 60;
	if(!strcmp("x", name)) return 61;
	if(!strcmp("y", name)) return 62;
	if(!strcmp("editable", name)) return 63;
	if(!strcmp("width", name)) return 64;
	if(!strcmp("height", name)) return 65;
	return -1;
}

void *gf_svg_sa_new_title()
{
	SVG_SA_titleElement *p;
	GF_SAFEALLOC(p, SVG_SA_titleElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_title);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "title";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_title_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_title_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_title_del(GF_Node *node)
{
	SVG_SA_titleElement *p = (SVG_SA_titleElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_title_get_attribute(GF_Node *node, GF_FieldInfo *info)
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

s32 gf_svg_sa_title_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	return -1;
}

void *gf_svg_sa_new_tspan()
{
	SVG_SA_tspanElement *p;
	GF_SAFEALLOC(p, SVG_SA_tspanElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_tspan);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "tspan";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_tspan_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_tspan_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	return p;
}

static void gf_svg_sa_tspan_del(GF_Node *node)
{
	SVG_SA_tspanElement *p = (SVG_SA_tspanElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_tspan_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 55:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 56:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 57:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 58:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_tspan_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("requiredExtensions", name)) return 54;
	if(!strcmp("requiredFeatures", name)) return 55;
	if(!strcmp("requiredFonts", name)) return 56;
	if(!strcmp("requiredFormats", name)) return 57;
	if(!strcmp("systemLanguage", name)) return 58;
	return -1;
}

void *gf_svg_sa_new_use()
{
	SVG_SA_useElement *p;
	GF_SAFEALLOC(p, SVG_SA_useElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_use);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "use";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_use_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_use_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	return p;
}

static void gf_svg_sa_use_del(GF_Node *node)
{
	SVG_SA_useElement *p = (SVG_SA_useElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_use_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-align";
			info->fieldType = SVG_TextAlign_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_align;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_anchor;
			return GF_OK;
		case 41:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->vector_effect;
			return GF_OK;
		case 42:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 43:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 44:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 45:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 46:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 47:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 48:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 49:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 50:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 51:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 52:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 53:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 54:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 55:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 56:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 57:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 58:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 59:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 60:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 61:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 62:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 63:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 64:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 65:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 66:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 67:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 68:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_useElement *)node)->x;
			return GF_OK;
		case 69:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_useElement *)node)->y;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_use_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("color", name)) return 16;
	if(!strcmp("color-rendering", name)) return 17;
	if(!strcmp("display-align", name)) return 18;
	if(!strcmp("fill", name)) return 19;
	if(!strcmp("fill-opacity", name)) return 20;
	if(!strcmp("fill-rule", name)) return 21;
	if(!strcmp("font-family", name)) return 22;
	if(!strcmp("font-size", name)) return 23;
	if(!strcmp("font-style", name)) return 24;
	if(!strcmp("font-weight", name)) return 25;
	if(!strcmp("line-increment", name)) return 26;
	if(!strcmp("solid-color", name)) return 27;
	if(!strcmp("solid-opacity", name)) return 28;
	if(!strcmp("stop-color", name)) return 29;
	if(!strcmp("stop-opacity", name)) return 30;
	if(!strcmp("stroke", name)) return 31;
	if(!strcmp("stroke-dasharray", name)) return 32;
	if(!strcmp("stroke-dashoffset", name)) return 33;
	if(!strcmp("stroke-linecap", name)) return 34;
	if(!strcmp("stroke-linejoin", name)) return 35;
	if(!strcmp("stroke-miterlimit", name)) return 36;
	if(!strcmp("stroke-opacity", name)) return 37;
	if(!strcmp("stroke-width", name)) return 38;
	if(!strcmp("text-align", name)) return 39;
	if(!strcmp("text-anchor", name)) return 40;
	if(!strcmp("vector-effect", name)) return 41;
	if(!strcmp("focusHighlight", name)) return 42;
	if(!strcmp("focusable", name)) return 43;
	if(!strcmp("nav-down", name)) return 44;
	if(!strcmp("nav-down-left", name)) return 45;
	if(!strcmp("nav-down-right", name)) return 46;
	if(!strcmp("nav-left", name)) return 47;
	if(!strcmp("nav-next", name)) return 48;
	if(!strcmp("nav-prev", name)) return 49;
	if(!strcmp("nav-right", name)) return 50;
	if(!strcmp("nav-up", name)) return 51;
	if(!strcmp("nav-up-left", name)) return 52;
	if(!strcmp("nav-up-right", name)) return 53;
	if(!strcmp("xlink:href", name)) return 54;
	if(!strcmp("xlink:show", name)) return 55;
	if(!strcmp("xlink:title", name)) return 56;
	if(!strcmp("xlink:actuate", name)) return 57;
	if(!strcmp("xlink:role", name)) return 58;
	if(!strcmp("xlink:arcrole", name)) return 59;
	if(!strcmp("xlink:type", name)) return 60;
	if(!strcmp("requiredExtensions", name)) return 61;
	if(!strcmp("requiredFeatures", name)) return 62;
	if(!strcmp("requiredFonts", name)) return 63;
	if(!strcmp("requiredFormats", name)) return 64;
	if(!strcmp("systemLanguage", name)) return 65;
	if(!strcmp("transform", name)) return 66;
	if(!strcmp("motionTransform", name)) return 67;
	if(!strcmp("x", name)) return 68;
	if(!strcmp("y", name)) return 69;
	return -1;
}

void *gf_svg_sa_new_video()
{
	SVG_SA_videoElement *p;
	GF_SAFEALLOC(p, SVG_SA_videoElement);
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_SA_video);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "video";
	((GF_Node *p)->sgprivate->node_del = gf_svg_sa_video_del;
	((GF_Node *p)->sgprivate->get_field = gf_svg_sa_video_get_attribute;
#endif
	gf_svg_sa_init_core((SVG_SA_Element *)p);
	gf_svg_sa_init_properties((SVG_SA_Element *)p);
	gf_svg_sa_init_focus((SVG_SA_Element *)p);
	gf_svg_sa_init_xlink((SVG_SA_Element *)p);
	gf_svg_sa_init_timing((SVG_SA_Element *)p);
	gf_svg_sa_init_sync((SVG_SA_Element *)p);
	gf_svg_sa_init_conditional((SVG_SA_Element *)p);
	gf_mx2d_init(p->transform.mat);
	p->timing->dur.type = SMIL_DURATION_MEDIA;
	return p;
}

static void gf_svg_sa_video_del(GF_Node *node)
{
	SVG_SA_videoElement *p = (SVG_SA_videoElement *)node;
	gf_svg_sa_reset_base_element((SVG_SA_Element *)p);
	free(p->type);
	if (p->motionTransform) free(p->motionTransform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_sa_video_get_attribute(GF_Node *node, GF_FieldInfo *info)
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
			info->name = "audio-level";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusHighlight;
			return GF_OK;
		case 17:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->focusable;
			return GF_OK;
		case 18:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down;
			return GF_OK;
		case 19:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_left;
			return GF_OK;
		case 20:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_down_right;
			return GF_OK;
		case 21:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_left;
			return GF_OK;
		case 22:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_next;
			return GF_OK;
		case 23:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_prev;
			return GF_OK;
		case 24:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_right;
			return GF_OK;
		case 25:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up;
			return GF_OK;
		case 26:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_left;
			return GF_OK;
		case 27:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->focus->nav_up_right;
			return GF_OK;
		case 28:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->href;
			return GF_OK;
		case 29:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->show;
			return GF_OK;
		case 30:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->title;
			return GF_OK;
		case 31:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->actuate;
			return GF_OK;
		case 32:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->role;
			return GF_OK;
		case 33:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->arcrole;
			return GF_OK;
		case 34:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->xlink->type;
			return GF_OK;
		case 35:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->begin;
			return GF_OK;
		case 36:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->end;
			return GF_OK;
		case 37:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->dur;
			return GF_OK;
		case 38:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatCount;
			return GF_OK;
		case 39:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->repeatDur;
			return GF_OK;
		case 40:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->restart;
			return GF_OK;
		case 41:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->min;
			return GF_OK;
		case 42:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->max;
			return GF_OK;
		case 43:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->fill;
			return GF_OK;
		case 44:
			info->name = "clipBegin";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->clipBegin;
			return GF_OK;
		case 45:
			info->name = "clipEnd";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->timing->clipEnd;
			return GF_OK;
		case 46:
			info->name = "syncBehavior";
			info->fieldType = SMIL_SyncBehavior_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->sync->syncBehavior;
			return GF_OK;
		case 47:
			info->name = "syncTolerance";
			info->fieldType = SMIL_SyncTolerance_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->sync->syncTolerance;
			return GF_OK;
		case 48:
			info->name = "syncMaster";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->sync->syncMaster;
			return GF_OK;
		case 49:
			info->name = "syncReference";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->sync->syncReference;
			return GF_OK;
		case 50:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 51:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 52:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFonts;
			return GF_OK;
		case 53:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->requiredFormats;
			return GF_OK;
		case 54:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVG_SA_Element *)node)->conditional->systemLanguage;
			return GF_OK;
		case 55:
			info->name = "transform";
			info->fieldType = SVG_Transform_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 56:
			info->name = "motionTransform";
			info->fieldType = SVG_Motion_datatype;
			info->far_ptr = ((SVGTransformableElement *)node)->motionTransform;
			return GF_OK;
		case 57:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_videoElement *)node)->x;
			return GF_OK;
		case 58:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVG_SA_videoElement *)node)->y;
			return GF_OK;
		case 59:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_videoElement *)node)->width;
			return GF_OK;
		case 60:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVG_SA_videoElement *)node)->height;
			return GF_OK;
		case 61:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVG_SA_videoElement *)node)->preserveAspectRatio;
			return GF_OK;
		case 62:
			info->name = "type";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVG_SA_videoElement *)node)->type;
			return GF_OK;
		case 63:
			info->name = "initialVisibility";
			info->fieldType = SVG_InitialVisibility_datatype;
			info->far_ptr = & ((SVG_SA_videoElement *)node)->initialVisibility;
			return GF_OK;
		case 64:
			info->name = "transformBehavior";
			info->fieldType = SVG_TransformBehavior_datatype;
			info->far_ptr = & ((SVG_SA_videoElement *)node)->transformBehavior;
			return GF_OK;
		case 65:
			info->name = "overlay";
			info->fieldType = SVG_Overlay_datatype;
			info->far_ptr = & ((SVG_SA_videoElement *)node)->overlay;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

s32 gf_svg_sa_video_get_attribute_index_from_name(char *name)
{
	if(!strcmp("id", name)) return 0;
	if(!strcmp("xml:id", name)) return 1;
	if(!strcmp("class", name)) return 2;
	if(!strcmp("xml:lang", name)) return 3;
	if(!strcmp("xml:base", name)) return 4;
	if(!strcmp("xml:space", name)) return 5;
	if(!strcmp("externalResourcesRequired", name)) return 6;
	if(!strcmp("audio-level", name)) return 7;
	if(!strcmp("display", name)) return 8;
	if(!strcmp("image-rendering", name)) return 9;
	if(!strcmp("pointer-events", name)) return 10;
	if(!strcmp("shape-rendering", name)) return 11;
	if(!strcmp("text-rendering", name)) return 12;
	if(!strcmp("viewport-fill", name)) return 13;
	if(!strcmp("viewport-fill-opacity", name)) return 14;
	if(!strcmp("visibility", name)) return 15;
	if(!strcmp("focusHighlight", name)) return 16;
	if(!strcmp("focusable", name)) return 17;
	if(!strcmp("nav-down", name)) return 18;
	if(!strcmp("nav-down-left", name)) return 19;
	if(!strcmp("nav-down-right", name)) return 20;
	if(!strcmp("nav-left", name)) return 21;
	if(!strcmp("nav-next", name)) return 22;
	if(!strcmp("nav-prev", name)) return 23;
	if(!strcmp("nav-right", name)) return 24;
	if(!strcmp("nav-up", name)) return 25;
	if(!strcmp("nav-up-left", name)) return 26;
	if(!strcmp("nav-up-right", name)) return 27;
	if(!strcmp("xlink:href", name)) return 28;
	if(!strcmp("xlink:show", name)) return 29;
	if(!strcmp("xlink:title", name)) return 30;
	if(!strcmp("xlink:actuate", name)) return 31;
	if(!strcmp("xlink:role", name)) return 32;
	if(!strcmp("xlink:arcrole", name)) return 33;
	if(!strcmp("xlink:type", name)) return 34;
	if(!strcmp("begin", name)) return 35;
	if(!strcmp("end", name)) return 36;
	if(!strcmp("dur", name)) return 37;
	if(!strcmp("repeatCount", name)) return 38;
	if(!strcmp("repeatDur", name)) return 39;
	if(!strcmp("restart", name)) return 40;
	if(!strcmp("min", name)) return 41;
	if(!strcmp("max", name)) return 42;
	if(!strcmp("fill", name)) return 43;
	if(!strcmp("clipBegin", name)) return 44;
	if(!strcmp("clipEnd", name)) return 45;
	if(!strcmp("syncBehavior", name)) return 46;
	if(!strcmp("syncTolerance", name)) return 47;
	if(!strcmp("syncMaster", name)) return 48;
	if(!strcmp("syncReference", name)) return 49;
	if(!strcmp("requiredExtensions", name)) return 50;
	if(!strcmp("requiredFeatures", name)) return 51;
	if(!strcmp("requiredFonts", name)) return 52;
	if(!strcmp("requiredFormats", name)) return 53;
	if(!strcmp("systemLanguage", name)) return 54;
	if(!strcmp("transform", name)) return 55;
	if(!strcmp("motionTransform", name)) return 56;
	if(!strcmp("x", name)) return 57;
	if(!strcmp("y", name)) return 58;
	if(!strcmp("width", name)) return 59;
	if(!strcmp("height", name)) return 60;
	if(!strcmp("preserveAspectRatio", name)) return 61;
	if(!strcmp("type", name)) return 62;
	if(!strcmp("initialVisibility", name)) return 63;
	if(!strcmp("transformBehavior", name)) return 64;
	if(!strcmp("overlay", name)) return 65;
	return -1;
}

SVG_SA_Element *gf_svg_sa_create_node(u32 ElementTag)
{
	switch (ElementTag) {
		case TAG_SVG_SA_a: return (SVG_SA_Element*) gf_svg_sa_new_a();
		case TAG_SVG_SA_animate: return (SVG_SA_Element*) gf_svg_sa_new_animate();
		case TAG_SVG_SA_animateColor: return (SVG_SA_Element*) gf_svg_sa_new_animateColor();
		case TAG_SVG_SA_animateMotion: return (SVG_SA_Element*) gf_svg_sa_new_animateMotion();
		case TAG_SVG_SA_animateTransform: return (SVG_SA_Element*) gf_svg_sa_new_animateTransform();
		case TAG_SVG_SA_animation: return (SVG_SA_Element*) gf_svg_sa_new_animation();
		case TAG_SVG_SA_audio: return (SVG_SA_Element*) gf_svg_sa_new_audio();
		case TAG_SVG_SA_circle: return (SVG_SA_Element*) gf_svg_sa_new_circle();
		case TAG_SVG_SA_conditional: return (SVG_SA_Element*) gf_svg_sa_new_conditional();
		case TAG_SVG_SA_cursorManager: return (SVG_SA_Element*) gf_svg_sa_new_cursorManager();
		case TAG_SVG_SA_defs: return (SVG_SA_Element*) gf_svg_sa_new_defs();
		case TAG_SVG_SA_desc: return (SVG_SA_Element*) gf_svg_sa_new_desc();
		case TAG_SVG_SA_discard: return (SVG_SA_Element*) gf_svg_sa_new_discard();
		case TAG_SVG_SA_ellipse: return (SVG_SA_Element*) gf_svg_sa_new_ellipse();
		case TAG_SVG_SA_font: return (SVG_SA_Element*) gf_svg_sa_new_font();
		case TAG_SVG_SA_font_face: return (SVG_SA_Element*) gf_svg_sa_new_font_face();
		case TAG_SVG_SA_font_face_src: return (SVG_SA_Element*) gf_svg_sa_new_font_face_src();
		case TAG_SVG_SA_font_face_uri: return (SVG_SA_Element*) gf_svg_sa_new_font_face_uri();
		case TAG_SVG_SA_foreignObject: return (SVG_SA_Element*) gf_svg_sa_new_foreignObject();
		case TAG_SVG_SA_g: return (SVG_SA_Element*) gf_svg_sa_new_g();
		case TAG_SVG_SA_glyph: return (SVG_SA_Element*) gf_svg_sa_new_glyph();
		case TAG_SVG_SA_handler: return (SVG_SA_Element*) gf_svg_sa_new_handler();
		case TAG_SVG_SA_hkern: return (SVG_SA_Element*) gf_svg_sa_new_hkern();
		case TAG_SVG_SA_image: return (SVG_SA_Element*) gf_svg_sa_new_image();
		case TAG_SVG_SA_line: return (SVG_SA_Element*) gf_svg_sa_new_line();
		case TAG_SVG_SA_linearGradient: return (SVG_SA_Element*) gf_svg_sa_new_linearGradient();
		case TAG_SVG_SA_listener: return (SVG_SA_Element*) gf_svg_sa_new_listener();
		case TAG_SVG_SA_metadata: return (SVG_SA_Element*) gf_svg_sa_new_metadata();
		case TAG_SVG_SA_missing_glyph: return (SVG_SA_Element*) gf_svg_sa_new_missing_glyph();
		case TAG_SVG_SA_mpath: return (SVG_SA_Element*) gf_svg_sa_new_mpath();
		case TAG_SVG_SA_path: return (SVG_SA_Element*) gf_svg_sa_new_path();
		case TAG_SVG_SA_polygon: return (SVG_SA_Element*) gf_svg_sa_new_polygon();
		case TAG_SVG_SA_polyline: return (SVG_SA_Element*) gf_svg_sa_new_polyline();
		case TAG_SVG_SA_prefetch: return (SVG_SA_Element*) gf_svg_sa_new_prefetch();
		case TAG_SVG_SA_radialGradient: return (SVG_SA_Element*) gf_svg_sa_new_radialGradient();
		case TAG_SVG_SA_rect: return (SVG_SA_Element*) gf_svg_sa_new_rect();
		case TAG_SVG_SA_rectClip: return (SVG_SA_Element*) gf_svg_sa_new_rectClip();
		case TAG_SVG_SA_script: return (SVG_SA_Element*) gf_svg_sa_new_script();
		case TAG_SVG_SA_selector: return (SVG_SA_Element*) gf_svg_sa_new_selector();
		case TAG_SVG_SA_set: return (SVG_SA_Element*) gf_svg_sa_new_set();
		case TAG_SVG_SA_simpleLayout: return (SVG_SA_Element*) gf_svg_sa_new_simpleLayout();
		case TAG_SVG_SA_solidColor: return (SVG_SA_Element*) gf_svg_sa_new_solidColor();
		case TAG_SVG_SA_stop: return (SVG_SA_Element*) gf_svg_sa_new_stop();
		case TAG_SVG_SA_svg: return (SVG_SA_Element*) gf_svg_sa_new_svg();
		case TAG_SVG_SA_switch: return (SVG_SA_Element*) gf_svg_sa_new_switch();
		case TAG_SVG_SA_tbreak: return (SVG_SA_Element*) gf_svg_sa_new_tbreak();
		case TAG_SVG_SA_text: return (SVG_SA_Element*) gf_svg_sa_new_text();
		case TAG_SVG_SA_textArea: return (SVG_SA_Element*) gf_svg_sa_new_textArea();
		case TAG_SVG_SA_title: return (SVG_SA_Element*) gf_svg_sa_new_title();
		case TAG_SVG_SA_tspan: return (SVG_SA_Element*) gf_svg_sa_new_tspan();
		case TAG_SVG_SA_use: return (SVG_SA_Element*) gf_svg_sa_new_use();
		case TAG_SVG_SA_video: return (SVG_SA_Element*) gf_svg_sa_new_video();
		default: return NULL;
	}
}

void gf_svg_sa_element_del(SVG_SA_Element *elt)
{
	GF_Node *node = (GF_Node *)elt;
	switch (node->sgprivate->tag) {
		case TAG_SVG_SA_a: gf_svg_sa_a_del(node); return;
		case TAG_SVG_SA_animate: gf_svg_sa_animate_del(node); return;
		case TAG_SVG_SA_animateColor: gf_svg_sa_animateColor_del(node); return;
		case TAG_SVG_SA_animateMotion: gf_svg_sa_animateMotion_del(node); return;
		case TAG_SVG_SA_animateTransform: gf_svg_sa_animateTransform_del(node); return;
		case TAG_SVG_SA_animation: gf_svg_sa_animation_del(node); return;
		case TAG_SVG_SA_audio: gf_svg_sa_audio_del(node); return;
		case TAG_SVG_SA_circle: gf_svg_sa_circle_del(node); return;
		case TAG_SVG_SA_conditional: gf_svg_sa_conditional_del(node); return;
		case TAG_SVG_SA_cursorManager: gf_svg_sa_cursorManager_del(node); return;
		case TAG_SVG_SA_defs: gf_svg_sa_defs_del(node); return;
		case TAG_SVG_SA_desc: gf_svg_sa_desc_del(node); return;
		case TAG_SVG_SA_discard: gf_svg_sa_discard_del(node); return;
		case TAG_SVG_SA_ellipse: gf_svg_sa_ellipse_del(node); return;
		case TAG_SVG_SA_font: gf_svg_sa_font_del(node); return;
		case TAG_SVG_SA_font_face: gf_svg_sa_font_face_del(node); return;
		case TAG_SVG_SA_font_face_src: gf_svg_sa_font_face_src_del(node); return;
		case TAG_SVG_SA_font_face_uri: gf_svg_sa_font_face_uri_del(node); return;
		case TAG_SVG_SA_foreignObject: gf_svg_sa_foreignObject_del(node); return;
		case TAG_SVG_SA_g: gf_svg_sa_g_del(node); return;
		case TAG_SVG_SA_glyph: gf_svg_sa_glyph_del(node); return;
		case TAG_SVG_SA_handler: gf_svg_sa_handler_del(node); return;
		case TAG_SVG_SA_hkern: gf_svg_sa_hkern_del(node); return;
		case TAG_SVG_SA_image: gf_svg_sa_image_del(node); return;
		case TAG_SVG_SA_line: gf_svg_sa_line_del(node); return;
		case TAG_SVG_SA_linearGradient: gf_svg_sa_linearGradient_del(node); return;
		case TAG_SVG_SA_listener: gf_svg_sa_listener_del(node); return;
		case TAG_SVG_SA_metadata: gf_svg_sa_metadata_del(node); return;
		case TAG_SVG_SA_missing_glyph: gf_svg_sa_missing_glyph_del(node); return;
		case TAG_SVG_SA_mpath: gf_svg_sa_mpath_del(node); return;
		case TAG_SVG_SA_path: gf_svg_sa_path_del(node); return;
		case TAG_SVG_SA_polygon: gf_svg_sa_polygon_del(node); return;
		case TAG_SVG_SA_polyline: gf_svg_sa_polyline_del(node); return;
		case TAG_SVG_SA_prefetch: gf_svg_sa_prefetch_del(node); return;
		case TAG_SVG_SA_radialGradient: gf_svg_sa_radialGradient_del(node); return;
		case TAG_SVG_SA_rect: gf_svg_sa_rect_del(node); return;
		case TAG_SVG_SA_rectClip: gf_svg_sa_rectClip_del(node); return;
		case TAG_SVG_SA_script: gf_svg_sa_script_del(node); return;
		case TAG_SVG_SA_selector: gf_svg_sa_selector_del(node); return;
		case TAG_SVG_SA_set: gf_svg_sa_set_del(node); return;
		case TAG_SVG_SA_simpleLayout: gf_svg_sa_simpleLayout_del(node); return;
		case TAG_SVG_SA_solidColor: gf_svg_sa_solidColor_del(node); return;
		case TAG_SVG_SA_stop: gf_svg_sa_stop_del(node); return;
		case TAG_SVG_SA_svg: gf_svg_sa_svg_del(node); return;
		case TAG_SVG_SA_switch: gf_svg_sa_switch_del(node); return;
		case TAG_SVG_SA_tbreak: gf_svg_sa_tbreak_del(node); return;
		case TAG_SVG_SA_text: gf_svg_sa_text_del(node); return;
		case TAG_SVG_SA_textArea: gf_svg_sa_textArea_del(node); return;
		case TAG_SVG_SA_title: gf_svg_sa_title_del(node); return;
		case TAG_SVG_SA_tspan: gf_svg_sa_tspan_del(node); return;
		case TAG_SVG_SA_use: gf_svg_sa_use_del(node); return;
		case TAG_SVG_SA_video: gf_svg_sa_video_del(node); return;
		default: return;
	}
}

u32 gf_svg_sa_get_attribute_count(GF_Node *node)
{
	switch (node->sgprivate->tag) {
		case TAG_SVG_SA_a: return 69;
		case TAG_SVG_SA_animate: return 35;
		case TAG_SVG_SA_animateColor: return 35;
		case TAG_SVG_SA_animateMotion: return 37;
		case TAG_SVG_SA_animateTransform: return 36;
		case TAG_SVG_SA_animation: return 63;
		case TAG_SVG_SA_audio: return 44;
		case TAG_SVG_SA_circle: return 64;
		case TAG_SVG_SA_conditional: return 9;
		case TAG_SVG_SA_cursorManager: return 16;
		case TAG_SVG_SA_defs: return 42;
		case TAG_SVG_SA_desc: return 7;
		case TAG_SVG_SA_discard: return 20;
		case TAG_SVG_SA_ellipse: return 65;
		case TAG_SVG_SA_font: return 9;
		case TAG_SVG_SA_font_face: return 35;
		case TAG_SVG_SA_font_face_src: return 7;
		case TAG_SVG_SA_font_face_uri: return 14;
		case TAG_SVG_SA_foreignObject: return 72;
		case TAG_SVG_SA_g: return 61;
		case TAG_SVG_SA_glyph: return 13;
		case TAG_SVG_SA_handler: return 9;
		case TAG_SVG_SA_hkern: return 12;
		case TAG_SVG_SA_image: return 49;
		case TAG_SVG_SA_line: return 65;
		case TAG_SVG_SA_linearGradient: return 56;
		case TAG_SVG_SA_listener: return 15;
		case TAG_SVG_SA_metadata: return 7;
		case TAG_SVG_SA_missing_glyph: return 9;
		case TAG_SVG_SA_mpath: return 14;
		case TAG_SVG_SA_path: return 63;
		case TAG_SVG_SA_polygon: return 62;
		case TAG_SVG_SA_polyline: return 62;
		case TAG_SVG_SA_prefetch: return 19;
		case TAG_SVG_SA_radialGradient: return 57;
		case TAG_SVG_SA_rect: return 67;
		case TAG_SVG_SA_rectClip: return 62;
		case TAG_SVG_SA_script: return 8;
		case TAG_SVG_SA_selector: return 62;
		case TAG_SVG_SA_set: return 27;
		case TAG_SVG_SA_simpleLayout: return 62;
		case TAG_SVG_SA_solidColor: return 42;
		case TAG_SVG_SA_stop: return 43;
		case TAG_SVG_SA_svg: return 69;
		case TAG_SVG_SA_switch: return 61;
		case TAG_SVG_SA_tbreak: return 7;
		case TAG_SVG_SA_text: return 65;
		case TAG_SVG_SA_textArea: return 66;
		case TAG_SVG_SA_title: return 7;
		case TAG_SVG_SA_tspan: return 59;
		case TAG_SVG_SA_use: return 70;
		case TAG_SVG_SA_video: return 66;
		default: return 0;
	}
}

GF_Err gf_svg_sa_get_attribute_info(GF_Node *node, GF_FieldInfo *info)
{
	switch (node->sgprivate->tag) {
		case TAG_SVG_SA_a: return gf_svg_sa_a_get_attribute(node, info);
		case TAG_SVG_SA_animate: return gf_svg_sa_animate_get_attribute(node, info);
		case TAG_SVG_SA_animateColor: return gf_svg_sa_animateColor_get_attribute(node, info);
		case TAG_SVG_SA_animateMotion: return gf_svg_sa_animateMotion_get_attribute(node, info);
		case TAG_SVG_SA_animateTransform: return gf_svg_sa_animateTransform_get_attribute(node, info);
		case TAG_SVG_SA_animation: return gf_svg_sa_animation_get_attribute(node, info);
		case TAG_SVG_SA_audio: return gf_svg_sa_audio_get_attribute(node, info);
		case TAG_SVG_SA_circle: return gf_svg_sa_circle_get_attribute(node, info);
		case TAG_SVG_SA_conditional: return gf_svg_sa_conditional_get_attribute(node, info);
		case TAG_SVG_SA_cursorManager: return gf_svg_sa_cursorManager_get_attribute(node, info);
		case TAG_SVG_SA_defs: return gf_svg_sa_defs_get_attribute(node, info);
		case TAG_SVG_SA_desc: return gf_svg_sa_desc_get_attribute(node, info);
		case TAG_SVG_SA_discard: return gf_svg_sa_discard_get_attribute(node, info);
		case TAG_SVG_SA_ellipse: return gf_svg_sa_ellipse_get_attribute(node, info);
		case TAG_SVG_SA_font: return gf_svg_sa_font_get_attribute(node, info);
		case TAG_SVG_SA_font_face: return gf_svg_sa_font_face_get_attribute(node, info);
		case TAG_SVG_SA_font_face_src: return gf_svg_sa_font_face_src_get_attribute(node, info);
		case TAG_SVG_SA_font_face_uri: return gf_svg_sa_font_face_uri_get_attribute(node, info);
		case TAG_SVG_SA_foreignObject: return gf_svg_sa_foreignObject_get_attribute(node, info);
		case TAG_SVG_SA_g: return gf_svg_sa_g_get_attribute(node, info);
		case TAG_SVG_SA_glyph: return gf_svg_sa_glyph_get_attribute(node, info);
		case TAG_SVG_SA_handler: return gf_svg_sa_handler_get_attribute(node, info);
		case TAG_SVG_SA_hkern: return gf_svg_sa_hkern_get_attribute(node, info);
		case TAG_SVG_SA_image: return gf_svg_sa_image_get_attribute(node, info);
		case TAG_SVG_SA_line: return gf_svg_sa_line_get_attribute(node, info);
		case TAG_SVG_SA_linearGradient: return gf_svg_sa_linearGradient_get_attribute(node, info);
		case TAG_SVG_SA_listener: return gf_svg_sa_listener_get_attribute(node, info);
		case TAG_SVG_SA_metadata: return gf_svg_sa_metadata_get_attribute(node, info);
		case TAG_SVG_SA_missing_glyph: return gf_svg_sa_missing_glyph_get_attribute(node, info);
		case TAG_SVG_SA_mpath: return gf_svg_sa_mpath_get_attribute(node, info);
		case TAG_SVG_SA_path: return gf_svg_sa_path_get_attribute(node, info);
		case TAG_SVG_SA_polygon: return gf_svg_sa_polygon_get_attribute(node, info);
		case TAG_SVG_SA_polyline: return gf_svg_sa_polyline_get_attribute(node, info);
		case TAG_SVG_SA_prefetch: return gf_svg_sa_prefetch_get_attribute(node, info);
		case TAG_SVG_SA_radialGradient: return gf_svg_sa_radialGradient_get_attribute(node, info);
		case TAG_SVG_SA_rect: return gf_svg_sa_rect_get_attribute(node, info);
		case TAG_SVG_SA_rectClip: return gf_svg_sa_rectClip_get_attribute(node, info);
		case TAG_SVG_SA_script: return gf_svg_sa_script_get_attribute(node, info);
		case TAG_SVG_SA_selector: return gf_svg_sa_selector_get_attribute(node, info);
		case TAG_SVG_SA_set: return gf_svg_sa_set_get_attribute(node, info);
		case TAG_SVG_SA_simpleLayout: return gf_svg_sa_simpleLayout_get_attribute(node, info);
		case TAG_SVG_SA_solidColor: return gf_svg_sa_solidColor_get_attribute(node, info);
		case TAG_SVG_SA_stop: return gf_svg_sa_stop_get_attribute(node, info);
		case TAG_SVG_SA_svg: return gf_svg_sa_svg_get_attribute(node, info);
		case TAG_SVG_SA_switch: return gf_svg_sa_switch_get_attribute(node, info);
		case TAG_SVG_SA_tbreak: return gf_svg_sa_tbreak_get_attribute(node, info);
		case TAG_SVG_SA_text: return gf_svg_sa_text_get_attribute(node, info);
		case TAG_SVG_SA_textArea: return gf_svg_sa_textArea_get_attribute(node, info);
		case TAG_SVG_SA_title: return gf_svg_sa_title_get_attribute(node, info);
		case TAG_SVG_SA_tspan: return gf_svg_sa_tspan_get_attribute(node, info);
		case TAG_SVG_SA_use: return gf_svg_sa_use_get_attribute(node, info);
		case TAG_SVG_SA_video: return gf_svg_sa_video_get_attribute(node, info);
		default: return GF_BAD_PARAM;
	}
}

u32 gf_svg_sa_node_type_by_class_name(const char *element_name)
{
	if (!element_name) return TAG_UndefinedNode;
	if (!stricmp(element_name, "a")) return TAG_SVG_SA_a;
	if (!stricmp(element_name, "animate")) return TAG_SVG_SA_animate;
	if (!stricmp(element_name, "animateColor")) return TAG_SVG_SA_animateColor;
	if (!stricmp(element_name, "animateMotion")) return TAG_SVG_SA_animateMotion;
	if (!stricmp(element_name, "animateTransform")) return TAG_SVG_SA_animateTransform;
	if (!stricmp(element_name, "animation")) return TAG_SVG_SA_animation;
	if (!stricmp(element_name, "audio")) return TAG_SVG_SA_audio;
	if (!stricmp(element_name, "circle")) return TAG_SVG_SA_circle;
	if (!stricmp(element_name, "conditional")) return TAG_SVG_SA_conditional;
	if (!stricmp(element_name, "cursorManager")) return TAG_SVG_SA_cursorManager;
	if (!stricmp(element_name, "defs")) return TAG_SVG_SA_defs;
	if (!stricmp(element_name, "desc")) return TAG_SVG_SA_desc;
	if (!stricmp(element_name, "discard")) return TAG_SVG_SA_discard;
	if (!stricmp(element_name, "ellipse")) return TAG_SVG_SA_ellipse;
	if (!stricmp(element_name, "font")) return TAG_SVG_SA_font;
	if (!stricmp(element_name, "font-face")) return TAG_SVG_SA_font_face;
	if (!stricmp(element_name, "font-face-src")) return TAG_SVG_SA_font_face_src;
	if (!stricmp(element_name, "font-face-uri")) return TAG_SVG_SA_font_face_uri;
	if (!stricmp(element_name, "foreignObject")) return TAG_SVG_SA_foreignObject;
	if (!stricmp(element_name, "g")) return TAG_SVG_SA_g;
	if (!stricmp(element_name, "glyph")) return TAG_SVG_SA_glyph;
	if (!stricmp(element_name, "handler")) return TAG_SVG_SA_handler;
	if (!stricmp(element_name, "hkern")) return TAG_SVG_SA_hkern;
	if (!stricmp(element_name, "image")) return TAG_SVG_SA_image;
	if (!stricmp(element_name, "line")) return TAG_SVG_SA_line;
	if (!stricmp(element_name, "linearGradient")) return TAG_SVG_SA_linearGradient;
	if (!stricmp(element_name, "listener")) return TAG_SVG_SA_listener;
	if (!stricmp(element_name, "metadata")) return TAG_SVG_SA_metadata;
	if (!stricmp(element_name, "missing-glyph")) return TAG_SVG_SA_missing_glyph;
	if (!stricmp(element_name, "mpath")) return TAG_SVG_SA_mpath;
	if (!stricmp(element_name, "path")) return TAG_SVG_SA_path;
	if (!stricmp(element_name, "polygon")) return TAG_SVG_SA_polygon;
	if (!stricmp(element_name, "polyline")) return TAG_SVG_SA_polyline;
	if (!stricmp(element_name, "prefetch")) return TAG_SVG_SA_prefetch;
	if (!stricmp(element_name, "radialGradient")) return TAG_SVG_SA_radialGradient;
	if (!stricmp(element_name, "rect")) return TAG_SVG_SA_rect;
	if (!stricmp(element_name, "rectClip")) return TAG_SVG_SA_rectClip;
	if (!stricmp(element_name, "script")) return TAG_SVG_SA_script;
	if (!stricmp(element_name, "selector")) return TAG_SVG_SA_selector;
	if (!stricmp(element_name, "set")) return TAG_SVG_SA_set;
	if (!stricmp(element_name, "simpleLayout")) return TAG_SVG_SA_simpleLayout;
	if (!stricmp(element_name, "solidColor")) return TAG_SVG_SA_solidColor;
	if (!stricmp(element_name, "stop")) return TAG_SVG_SA_stop;
	if (!stricmp(element_name, "svg")) return TAG_SVG_SA_svg;
	if (!stricmp(element_name, "switch")) return TAG_SVG_SA_switch;
	if (!stricmp(element_name, "tbreak")) return TAG_SVG_SA_tbreak;
	if (!stricmp(element_name, "text")) return TAG_SVG_SA_text;
	if (!stricmp(element_name, "textArea")) return TAG_SVG_SA_textArea;
	if (!stricmp(element_name, "title")) return TAG_SVG_SA_title;
	if (!stricmp(element_name, "tspan")) return TAG_SVG_SA_tspan;
	if (!stricmp(element_name, "use")) return TAG_SVG_SA_use;
	if (!stricmp(element_name, "video")) return TAG_SVG_SA_video;
	return TAG_UndefinedNode;
}

const char *gf_svg_sa_get_element_name(u32 tag)
{
	switch(tag) {
	case TAG_SVG_SA_a: return "a";
	case TAG_SVG_SA_animate: return "animate";
	case TAG_SVG_SA_animateColor: return "animateColor";
	case TAG_SVG_SA_animateMotion: return "animateMotion";
	case TAG_SVG_SA_animateTransform: return "animateTransform";
	case TAG_SVG_SA_animation: return "animation";
	case TAG_SVG_SA_audio: return "audio";
	case TAG_SVG_SA_circle: return "circle";
	case TAG_SVG_SA_conditional: return "conditional";
	case TAG_SVG_SA_cursorManager: return "cursorManager";
	case TAG_SVG_SA_defs: return "defs";
	case TAG_SVG_SA_desc: return "desc";
	case TAG_SVG_SA_discard: return "discard";
	case TAG_SVG_SA_ellipse: return "ellipse";
	case TAG_SVG_SA_font: return "font";
	case TAG_SVG_SA_font_face: return "font-face";
	case TAG_SVG_SA_font_face_src: return "font-face-src";
	case TAG_SVG_SA_font_face_uri: return "font-face-uri";
	case TAG_SVG_SA_foreignObject: return "foreignObject";
	case TAG_SVG_SA_g: return "g";
	case TAG_SVG_SA_glyph: return "glyph";
	case TAG_SVG_SA_handler: return "handler";
	case TAG_SVG_SA_hkern: return "hkern";
	case TAG_SVG_SA_image: return "image";
	case TAG_SVG_SA_line: return "line";
	case TAG_SVG_SA_linearGradient: return "linearGradient";
	case TAG_SVG_SA_listener: return "listener";
	case TAG_SVG_SA_metadata: return "metadata";
	case TAG_SVG_SA_missing_glyph: return "missing-glyph";
	case TAG_SVG_SA_mpath: return "mpath";
	case TAG_SVG_SA_path: return "path";
	case TAG_SVG_SA_polygon: return "polygon";
	case TAG_SVG_SA_polyline: return "polyline";
	case TAG_SVG_SA_prefetch: return "prefetch";
	case TAG_SVG_SA_radialGradient: return "radialGradient";
	case TAG_SVG_SA_rect: return "rect";
	case TAG_SVG_SA_rectClip: return "rectClip";
	case TAG_SVG_SA_script: return "script";
	case TAG_SVG_SA_selector: return "selector";
	case TAG_SVG_SA_set: return "set";
	case TAG_SVG_SA_simpleLayout: return "simpleLayout";
	case TAG_SVG_SA_solidColor: return "solidColor";
	case TAG_SVG_SA_stop: return "stop";
	case TAG_SVG_SA_svg: return "svg";
	case TAG_SVG_SA_switch: return "switch";
	case TAG_SVG_SA_tbreak: return "tbreak";
	case TAG_SVG_SA_text: return "text";
	case TAG_SVG_SA_textArea: return "textArea";
	case TAG_SVG_SA_title: return "title";
	case TAG_SVG_SA_tspan: return "tspan";
	case TAG_SVG_SA_use: return "use";
	case TAG_SVG_SA_video: return "video";
	default: return "UndefinedNode";
	}
}

s32 gf_svg_sa_get_attribute_index_by_name(GF_Node *node, char *name)
{
	switch(node->sgprivate->tag) {
	case TAG_SVG_SA_a: return gf_svg_sa_a_get_attribute_index_from_name(name);
	case TAG_SVG_SA_animate: return gf_svg_sa_animate_get_attribute_index_from_name(name);
	case TAG_SVG_SA_animateColor: return gf_svg_sa_animateColor_get_attribute_index_from_name(name);
	case TAG_SVG_SA_animateMotion: return gf_svg_sa_animateMotion_get_attribute_index_from_name(name);
	case TAG_SVG_SA_animateTransform: return gf_svg_sa_animateTransform_get_attribute_index_from_name(name);
	case TAG_SVG_SA_animation: return gf_svg_sa_animation_get_attribute_index_from_name(name);
	case TAG_SVG_SA_audio: return gf_svg_sa_audio_get_attribute_index_from_name(name);
	case TAG_SVG_SA_circle: return gf_svg_sa_circle_get_attribute_index_from_name(name);
	case TAG_SVG_SA_conditional: return gf_svg_sa_conditional_get_attribute_index_from_name(name);
	case TAG_SVG_SA_cursorManager: return gf_svg_sa_cursorManager_get_attribute_index_from_name(name);
	case TAG_SVG_SA_defs: return gf_svg_sa_defs_get_attribute_index_from_name(name);
	case TAG_SVG_SA_desc: return gf_svg_sa_desc_get_attribute_index_from_name(name);
	case TAG_SVG_SA_discard: return gf_svg_sa_discard_get_attribute_index_from_name(name);
	case TAG_SVG_SA_ellipse: return gf_svg_sa_ellipse_get_attribute_index_from_name(name);
	case TAG_SVG_SA_font: return gf_svg_sa_font_get_attribute_index_from_name(name);
	case TAG_SVG_SA_font_face: return gf_svg_sa_font_face_get_attribute_index_from_name(name);
	case TAG_SVG_SA_font_face_src: return gf_svg_sa_font_face_src_get_attribute_index_from_name(name);
	case TAG_SVG_SA_font_face_uri: return gf_svg_sa_font_face_uri_get_attribute_index_from_name(name);
	case TAG_SVG_SA_foreignObject: return gf_svg_sa_foreignObject_get_attribute_index_from_name(name);
	case TAG_SVG_SA_g: return gf_svg_sa_g_get_attribute_index_from_name(name);
	case TAG_SVG_SA_glyph: return gf_svg_sa_glyph_get_attribute_index_from_name(name);
	case TAG_SVG_SA_handler: return gf_svg_sa_handler_get_attribute_index_from_name(name);
	case TAG_SVG_SA_hkern: return gf_svg_sa_hkern_get_attribute_index_from_name(name);
	case TAG_SVG_SA_image: return gf_svg_sa_image_get_attribute_index_from_name(name);
	case TAG_SVG_SA_line: return gf_svg_sa_line_get_attribute_index_from_name(name);
	case TAG_SVG_SA_linearGradient: return gf_svg_sa_linearGradient_get_attribute_index_from_name(name);
	case TAG_SVG_SA_listener: return gf_svg_sa_listener_get_attribute_index_from_name(name);
	case TAG_SVG_SA_metadata: return gf_svg_sa_metadata_get_attribute_index_from_name(name);
	case TAG_SVG_SA_missing_glyph: return gf_svg_sa_missing_glyph_get_attribute_index_from_name(name);
	case TAG_SVG_SA_mpath: return gf_svg_sa_mpath_get_attribute_index_from_name(name);
	case TAG_SVG_SA_path: return gf_svg_sa_path_get_attribute_index_from_name(name);
	case TAG_SVG_SA_polygon: return gf_svg_sa_polygon_get_attribute_index_from_name(name);
	case TAG_SVG_SA_polyline: return gf_svg_sa_polyline_get_attribute_index_from_name(name);
	case TAG_SVG_SA_prefetch: return gf_svg_sa_prefetch_get_attribute_index_from_name(name);
	case TAG_SVG_SA_radialGradient: return gf_svg_sa_radialGradient_get_attribute_index_from_name(name);
	case TAG_SVG_SA_rect: return gf_svg_sa_rect_get_attribute_index_from_name(name);
	case TAG_SVG_SA_rectClip: return gf_svg_sa_rectClip_get_attribute_index_from_name(name);
	case TAG_SVG_SA_script: return gf_svg_sa_script_get_attribute_index_from_name(name);
	case TAG_SVG_SA_selector: return gf_svg_sa_selector_get_attribute_index_from_name(name);
	case TAG_SVG_SA_set: return gf_svg_sa_set_get_attribute_index_from_name(name);
	case TAG_SVG_SA_simpleLayout: return gf_svg_sa_simpleLayout_get_attribute_index_from_name(name);
	case TAG_SVG_SA_solidColor: return gf_svg_sa_solidColor_get_attribute_index_from_name(name);
	case TAG_SVG_SA_stop: return gf_svg_sa_stop_get_attribute_index_from_name(name);
	case TAG_SVG_SA_svg: return gf_svg_sa_svg_get_attribute_index_from_name(name);
	case TAG_SVG_SA_switch: return gf_svg_sa_switch_get_attribute_index_from_name(name);
	case TAG_SVG_SA_tbreak: return gf_svg_sa_tbreak_get_attribute_index_from_name(name);
	case TAG_SVG_SA_text: return gf_svg_sa_text_get_attribute_index_from_name(name);
	case TAG_SVG_SA_textArea: return gf_svg_sa_textArea_get_attribute_index_from_name(name);
	case TAG_SVG_SA_title: return gf_svg_sa_title_get_attribute_index_from_name(name);
	case TAG_SVG_SA_tspan: return gf_svg_sa_tspan_get_attribute_index_from_name(name);
	case TAG_SVG_SA_use: return gf_svg_sa_use_get_attribute_index_from_name(name);
	case TAG_SVG_SA_video: return gf_svg_sa_video_get_attribute_index_from_name(name);
	default: return -1;
	}
}

Bool gf_svg_sa_is_element_transformable(u32 tag)
{
	switch(tag) {
	case TAG_SVG_SA_a:return 1;
	case TAG_SVG_SA_animate:return 0;
	case TAG_SVG_SA_animateColor:return 0;
	case TAG_SVG_SA_animateMotion:return 0;
	case TAG_SVG_SA_animateTransform:return 0;
	case TAG_SVG_SA_animation:return 1;
	case TAG_SVG_SA_audio:return 0;
	case TAG_SVG_SA_circle:return 1;
	case TAG_SVG_SA_conditional:return 0;
	case TAG_SVG_SA_cursorManager:return 0;
	case TAG_SVG_SA_defs:return 0;
	case TAG_SVG_SA_desc:return 0;
	case TAG_SVG_SA_discard:return 0;
	case TAG_SVG_SA_ellipse:return 1;
	case TAG_SVG_SA_font:return 0;
	case TAG_SVG_SA_font_face:return 0;
	case TAG_SVG_SA_font_face_src:return 0;
	case TAG_SVG_SA_font_face_uri:return 0;
	case TAG_SVG_SA_foreignObject:return 1;
	case TAG_SVG_SA_g:return 1;
	case TAG_SVG_SA_glyph:return 0;
	case TAG_SVG_SA_handler:return 0;
	case TAG_SVG_SA_hkern:return 0;
	case TAG_SVG_SA_image:return 1;
	case TAG_SVG_SA_line:return 1;
	case TAG_SVG_SA_linearGradient:return 0;
	case TAG_SVG_SA_listener:return 0;
	case TAG_SVG_SA_metadata:return 0;
	case TAG_SVG_SA_missing_glyph:return 0;
	case TAG_SVG_SA_mpath:return 0;
	case TAG_SVG_SA_path:return 1;
	case TAG_SVG_SA_polygon:return 1;
	case TAG_SVG_SA_polyline:return 1;
	case TAG_SVG_SA_prefetch:return 0;
	case TAG_SVG_SA_radialGradient:return 0;
	case TAG_SVG_SA_rect:return 1;
	case TAG_SVG_SA_rectClip:return 1;
	case TAG_SVG_SA_script:return 0;
	case TAG_SVG_SA_selector:return 1;
	case TAG_SVG_SA_set:return 0;
	case TAG_SVG_SA_simpleLayout:return 1;
	case TAG_SVG_SA_solidColor:return 0;
	case TAG_SVG_SA_stop:return 0;
	case TAG_SVG_SA_svg:return 0;
	case TAG_SVG_SA_switch:return 1;
	case TAG_SVG_SA_tbreak:return 0;
	case TAG_SVG_SA_text:return 1;
	case TAG_SVG_SA_textArea:return 1;
	case TAG_SVG_SA_title:return 0;
	case TAG_SVG_SA_tspan:return 0;
	case TAG_SVG_SA_use:return 1;
	case TAG_SVG_SA_video:return 1;
	default: return 0;
	}
}
#endif /*GPAC_ENABLE_SVG_SA*/

#endif /*GPAC_DISABLE_SVG*/


