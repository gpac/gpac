/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2023
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

#include "visual_manager.h"

#if !defined(GPAC_DISABLE_SVG) &&  !defined(GPAC_DISABLE_COMPOSITOR)

#include "nodes_stacks.h"
#include <gpac/iso639.h>


/*
	This is the generic routine for children traversing
*/
void compositor_svg_traverse_children(GF_ChildNodeItem *children, GF_TraverseState *tr_state)
{
	while (children) {
		gf_node_traverse(children->node, tr_state);
		children = children->next;
	}
}

Bool compositor_svg_is_display_off(SVGPropertiesPointers *props)
{
	return (props->display && (*(props->display) == SVG_DISPLAY_NONE)) ? GF_TRUE : GF_FALSE;
}


void compositor_svg_apply_local_transformation(GF_TraverseState *tr_state, SVGAllAttributes *atts, GF_Matrix2D *backup_matrix_2d, GF_Matrix *backup_matrix)
{
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d && backup_matrix) {
		Bool is_draw = (tr_state->traversing_mode==TRAVERSE_SORT) ? GF_TRUE : GF_FALSE;
		gf_mx_copy(*backup_matrix, tr_state->model_matrix);

		if (atts->transform && atts->transform->is_ref) {
			gf_mx_from_mx2d(&tr_state->model_matrix, &tr_state->vb_transform);
			if (is_draw) {
				gf_mx_init(tr_state->model_matrix);
				gf_mx_add_translation(&tr_state->model_matrix, -tr_state->camera->width/2, tr_state->camera->height/2, 0);
				gf_mx_add_scale(&tr_state->model_matrix, FIX_ONE, -FIX_ONE, FIX_ONE);
				gf_mx_add_matrix_2d(&tr_state->model_matrix, &tr_state->vb_transform);
			}
		}

		if (atts->motionTransform) {
			gf_mx_add_matrix_2d(&tr_state->model_matrix, atts->motionTransform);
		}

		if (atts->transform) {
			gf_mx_add_matrix_2d(&tr_state->model_matrix, &atts->transform->mat);
		}
		return;
	}
#endif
	gf_mx2d_copy(*backup_matrix_2d, tr_state->transform);

	if (atts->transform && atts->transform->is_ref)
		gf_mx2d_copy(tr_state->transform, tr_state->vb_transform);

	if (atts->motionTransform)
		gf_mx2d_pre_multiply(&tr_state->transform, atts->motionTransform);

	if (atts->transform)
		gf_mx2d_pre_multiply(&tr_state->transform, &atts->transform->mat);

}

void compositor_svg_restore_parent_transformation(GF_TraverseState *tr_state, GF_Matrix2D *backup_matrix_2d, GF_Matrix *backup_matrix)
{
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d && backup_matrix) {
		gf_mx_copy(tr_state->model_matrix, *backup_matrix);
		return;
	}
#endif
	gf_mx2d_copy(tr_state->transform, *backup_matrix_2d);
}

#ifdef GPAC_UNUSED_FUNC
static void gf_svg_apply_inheritance_no_inheritance(SVGAllAttributes *all_atts, SVGPropertiesPointers *render_svg_props)
{
#define CHECK_PROP(a, b) if (b) a = b;

	render_svg_props->audio_level = NULL;
	CHECK_PROP(render_svg_props->display, all_atts->display);
	CHECK_PROP(render_svg_props->fill, all_atts->fill);
	CHECK_PROP(render_svg_props->fill_opacity, all_atts->fill_opacity);
	CHECK_PROP(render_svg_props->fill_rule, all_atts->fill_rule);
	CHECK_PROP(render_svg_props->solid_color, all_atts->solid_color);
	CHECK_PROP(render_svg_props->solid_opacity, all_atts->solid_opacity);
	CHECK_PROP(render_svg_props->stop_color, all_atts->stop_color);
	CHECK_PROP(render_svg_props->stop_opacity, all_atts->stop_opacity);
	CHECK_PROP(render_svg_props->stroke, all_atts->stroke);
	CHECK_PROP(render_svg_props->stroke_dasharray, all_atts->stroke_dasharray);
	CHECK_PROP(render_svg_props->stroke_dashoffset, all_atts->stroke_dashoffset);
	CHECK_PROP(render_svg_props->stroke_linecap, all_atts->stroke_linecap);
	CHECK_PROP(render_svg_props->stroke_linejoin, all_atts->stroke_linejoin);
	CHECK_PROP(render_svg_props->stroke_miterlimit, all_atts->stroke_miterlimit);
	CHECK_PROP(render_svg_props->stroke_opacity, all_atts->stroke_opacity);
	CHECK_PROP(render_svg_props->stroke_width, all_atts->stroke_width);
	CHECK_PROP(render_svg_props->visibility, all_atts->visibility);
}
#endif /*GPAC_UNUSED_FUNC*/

static const struct svg_11_feature {
	const char *name;
	Bool supported;
} svg11_features[] =
{
	{ "Animation", GF_TRUE},
	{ "AnimationEventsAttribute", GF_TRUE},
	{ "BasicClip", GF_FALSE},
	{ "BasicFilter", GF_FALSE},
	{ "BasicFont", GF_TRUE},
	{ "BasicGraphicsAttribute", GF_TRUE},
	{ "BasicPaintAttribute", GF_TRUE},
	{ "BasicStructure", GF_TRUE},
	{ "BasicText", GF_TRUE},
	{ "Clip", GF_FALSE},
	{ "ColorProfile", GF_FALSE},
	{ "ConditionalProcessing", GF_TRUE},
	{ "ContainerAttribute", GF_TRUE},
	{ "CoreAttribute", GF_TRUE},
	{ "Cursor", GF_FALSE},
	{ "DocumentEventsAttribute", GF_TRUE},
	{ "Extensibility", GF_TRUE},
	{ "ExternalResourcesRequired", GF_FALSE},
	{ "Font", GF_TRUE},
	{ "Filter", GF_FALSE},
	{ "Gradient", GF_TRUE},
	{ "GraphicalEventsAttribute", GF_TRUE},
	{ "GraphicsAttribute", GF_TRUE},
	{ "Hyperlinking", GF_TRUE},
	{ "Image", GF_TRUE},
	{ "Marker", GF_FALSE},
	{ "Mask", GF_FALSE},
	{ "OpacityAttribute", GF_TRUE},
	{ "PaintAttribute", GF_TRUE},
	{ "Pattern", GF_FALSE},
	{ "Script", GF_TRUE},
	{ "Scripting", GF_TRUE},
	{ "Shape", GF_TRUE},
	{ "View", GF_FALSE},	/*no support for <view> element, the rest is OK ...*/
	{ "ViewportAttribute", GF_TRUE},
	{ "Structure", GF_TRUE},
	{ "Style", GF_FALSE},
	{ "Text", GF_TRUE},
	{ "View", GF_TRUE},
	{ "XlinkAttribute", GF_TRUE},

	{ "SVG", GF_TRUE},
	{ "SVG-animation", GF_TRUE},
	{ "SVG-dynamic", GF_TRUE},
	{ "SVG-static", GF_TRUE},
	/*we don't support SVG DOM, only uDOM*/
	{ "SVGDOM", GF_FALSE},
	{ "SVGDOM-animation", GF_FALSE},
	{ "SVGDOM-dynamic", GF_FALSE},
	{ "SVGDOM-static", GF_FALSE},
};
static const struct svg_12_feature {
	const char *name;
	Bool supported;
} svg12_features[] =
{
	{ "CoreAttribute", GF_TRUE},
	{ "NavigationAttribute", GF_TRUE},
	{ "Structure", GF_TRUE},
	{ "ConditionalProcessing", GF_TRUE},
	{ "ConditionalProcessingAttribute", GF_TRUE},
	{ "Image", GF_TRUE},
	{ "Prefetch", GF_TRUE},
	{ "Discard", GF_TRUE},
	{ "Shape", GF_TRUE},
	{ "Text", GF_TRUE},
	{ "PaintAttribute", GF_TRUE},
	{ "OpacityAttribute", GF_TRUE},
	{ "GraphicsAttribute", GF_TRUE},
	{ "Gradient", GF_TRUE},
	{ "SolidColor", GF_TRUE},
	{ "Hyperlinking", GF_TRUE},
	{ "XlinkAttribute", GF_TRUE},
	{ "ExternalResourcesRequired", GF_TRUE},
	{ "Scripting", GF_TRUE},
	{ "Handler", GF_TRUE},
	{ "Listener", GF_TRUE},
	{ "TimedAnimation", GF_TRUE},
	{ "Animation", GF_TRUE},
	{ "Audio", GF_TRUE},
	{ "Video", GF_TRUE},
	{ "Font", GF_TRUE},
	{ "Extensibility", GF_TRUE},
	{ "MediaAttribute", GF_TRUE},
	{ "TextFlow", GF_TRUE},
	{ "TransformedVideo", GF_TRUE},
	{ "ComposedVideo", GF_TRUE},
	{ "EditableTextAttribute", GF_TRUE},

	{ "SVG-static", GF_TRUE},
	{ "SVG-static-DOM", GF_TRUE},
	{ "SVG-animated", GF_TRUE},
	{ "SVG-all", GF_TRUE},
	{ "SVG-interactive", GF_TRUE},
};


Bool compositor_svg_evaluate_conditional(GF_Compositor *compositor, SVGAllAttributes *atts)
{
	u32 i, count;
	Bool found;
	s32 lang_idx;

	/*process required features*/
	count = atts->requiredFeatures ? gf_list_count(*atts->requiredFeatures) : 0;
	for (i=0; i<count; i++) {
		char *feat = NULL;
		XMLRI *iri = (XMLRI*)gf_list_get(*atts->requiredFeatures, i);
		if (!iri->string) continue;

		if (!strnicmp(iri->string, "org.w3c.svg", 11)) {
			feat = iri->string+12;
			if (feat) {
				if (!stricmp(feat, "animation")) {}
				else if (!stricmp(feat, "dynamic")) {}
				/*no support for filters, clipping & co - SVG 1.0 featureStrings are badly designed*/
				else return GF_FALSE;
			}
		}
		else if (!strnicmp(iri->string, "http://www.w3.org/TR/SVG11/feature", 34)) {
			feat = iri->string+35;
			if (feat) {
				u32 j, nbf;
				found = GF_FALSE;
				nbf  = sizeof(svg11_features) / sizeof(struct svg_11_feature);
				for (j=0; j<nbf; j++) {
					if (!strcmp(svg11_features[j].name, feat)) {
						found = GF_TRUE;
						if (!svg11_features[j].supported) return GF_FALSE;
						break;
					}
				}
				if (!found) return GF_FALSE;
			}
		}
		else if (!strnicmp(iri->string, "http://www.w3.org/Graphics/SVG/feature/1.2/", 43)) {
			feat = iri->string+44;
			if (feat) {
				u32 j, nbf;
				found = GF_FALSE;
				nbf  = sizeof(svg12_features) / sizeof(struct svg_12_feature);
				for (j=0; j<nbf; j++) {
					if (!strcmp(svg12_features[j].name, feat)) {
						found = GF_TRUE;
						if (!svg12_features[j].supported) return GF_FALSE;
						break;
					}
				}
				if (!found) return GF_FALSE;
			}
		}
		/*unrecognized feature*/
		else {
			return GF_FALSE;
		}
	}

	/*process required extensions*/
	count = atts->requiredExtensions ? gf_list_count(*atts->requiredExtensions) : 0;
	if (count) return GF_FALSE;

	/*process system language*/
	count = atts->systemLanguage ? gf_list_count(*atts->systemLanguage) : 0;
	if (count) {
		found = GF_FALSE;
		const char *lang = gf_opts_get_key("core", "lang");
		lang_idx = lang ? gf_lang_find(lang) : -1;
	} else {
		lang_idx = -1;
		found = GF_TRUE;
	}

	for (i=0; i<count; i++) {
		char *lang = (char*)gf_list_get(*atts->systemLanguage, i);
		s32 pref_lang_idx = lang ? gf_lang_find(lang) : -1;
		if (pref_lang_idx==lang_idx) {
			found = GF_TRUE;
			break;
		}
	}
	if (!found) return GF_FALSE;

	/*process required formats*/
	count = atts->requiredFormats ? gf_list_count(*atts->requiredFormats) : 0;
	if (count) {
		for (i=0; i<count; i++) {
			Bool mime_ok;
			char *mime = (char*)gf_list_get(*atts->requiredFormats, i);
			char *sep = strchr(mime, ';');
			if (sep) sep[0] = 0;
			mime_ok = gf_filter_is_supported_mime(compositor->filter, mime);
			if (sep) sep[0] = ';';
			if (!mime_ok) return GF_FALSE;
		}
	}

	/*process required fonts*/
	count = atts->requiredFonts ? gf_list_count(*atts->requiredFonts) : 0;
	if (count) {
		for (i=0; i<count; i++) {
			char *font = (char*)gf_list_get(*atts->requiredFonts, i);
			if (gf_font_manager_set_font_ex(compositor->font_manager, &font, 1, 0, GF_TRUE)==NULL)
				return GF_FALSE;
		}
	}

	/*OK, we can render this one*/
	return GF_TRUE;
}

Bool compositor_svg_traverse_base(GF_Node *node, SVGAllAttributes *atts, GF_TraverseState *tr_state,
                                  SVGPropertiesPointers *backup_props, u32 *backup_flags)
{
	u32 inherited_flags_mask, flags;

	if (atts->requiredFeatures || atts->requiredExtensions || atts->systemLanguage
	        || atts->requiredFonts || atts->requiredFormats) {
		if (!compositor_svg_evaluate_conditional(tr_state->visual->compositor, atts))
			return GF_FALSE;
	}

	memcpy(backup_props, tr_state->svg_props, sizeof(SVGPropertiesPointers));
	*backup_flags = tr_state->svg_flags;

#if 0
	// applying inheritance and determining which group of properties are being inherited
	inherited_flags_mask = gf_svg_apply_inheritance(atts, tr_state->svg_props);
	gf_svg_apply_animations(node, tr_state->svg_props); // including again inheritance if values are 'inherit'
#else
	/* animation (including possibly inheritance) then full inheritance */
	gf_svg_apply_animations(node, tr_state->svg_props);
	inherited_flags_mask = gf_svg_apply_inheritance(atts, tr_state->svg_props);
//	gf_svg_apply_inheritance_no_inheritance(atts, tr_state->svg_props);
//	inherited_flags_mask = 0xFFFFFFFF;
#endif
	tr_state->svg_flags &= inherited_flags_mask;
	flags = gf_node_dirty_get(node);
	tr_state->svg_flags |= flags;

	return GF_TRUE;
}

#endif //!defined(GPAC_DISABLE_SVG) &&  !defined(GPAC_DISABLE_COMPOSITOR)
