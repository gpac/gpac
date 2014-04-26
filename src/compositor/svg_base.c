/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
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

#ifndef GPAC_DISABLE_SVG
#include "nodes_stacks.h"


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
	return (props->display && (*(props->display) == SVG_DISPLAY_NONE)) ? 1 : 0;
}


void compositor_svg_apply_local_transformation(GF_TraverseState *tr_state, SVGAllAttributes *atts, GF_Matrix2D *backup_matrix_2d, GF_Matrix *backup_matrix)
{
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d && backup_matrix) {
		Bool is_draw = (tr_state->traversing_mode==TRAVERSE_SORT) ? 1 : 0;
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
	{ "Animation", 1},
	{ "AnimationEventsAttribute", 1},
	{ "BasicClip", 0},
	{ "BasicFilter", 0},
	{ "BasicFont", 1},
	{ "BasicGraphicsAttribute", 1},
	{ "BasicPaintAttribute", 1},
	{ "BasicStructure", 1},
	{ "BasicText", 1},
	{ "Clip", 0},
	{ "ColorProfile", 0},
	{ "ConditionalProcessing", 1},
	{ "ContainerAttribute", 1},
	{ "CoreAttribute", 1},
	{ "Cursor", 0},
	{ "DocumentEventsAttribute", 1},
	{ "Extensibility", 1},
	{ "ExternalResourcesRequired", 0},
	{ "Font", 1},
	{ "Filter", 0},
	{ "Gradient", 1},
	{ "GraphicalEventsAttribute", 1},
	{ "GraphicsAttribute", 1},
	{ "Hyperlinking", 1},
	{ "Image", 1},
	{ "Marker", 0},
	{ "Mask", 0},
	{ "OpacityAttribute", 1},
	{ "PaintAttribute", 1},
	{ "Pattern", 0},
	{ "Script", 1},
	{ "Scripting", 1},
	{ "Shape", 1},
	{ "View", 0},	/*no support for <view> element, the rest is OK ...*/
	{ "ViewportAttribute", 1},
	{ "Structure", 1},
	{ "Style", 0},
	{ "Text", 1},
	{ "View", 1},
	{ "XlinkAttribute", 1},

	{ "SVG", 1},
	{ "SVG-animation", 1},
	{ "SVG-dynamic", 1},
	{ "SVG-static", 1},
	/*we don't support SVG DOM, only uDOM*/
	{ "SVGDOM", 0},
	{ "SVGDOM-animation", 0},
	{ "SVGDOM-dynamic", 0},
	{ "SVGDOM-static", 0},
};
static const struct svg_12_feature {
	const char *name;
	Bool supported;
} svg12_features[] =
{
	{ "CoreAttribute", 1},
	{ "NavigationAttribute", 1},
	{ "Structure", 1},
	{ "ConditionalProcessing", 1},
	{ "ConditionalProcessingAttribute", 1},
	{ "Image", 1},
	{ "Prefetch", 1},
	{ "Discard", 1},
	{ "Shape", 1},
	{ "Text", 1},
	{ "PaintAttribute", 1},
	{ "OpacityAttribute", 1},
	{ "GraphicsAttribute", 1},
	{ "Gradient", 1},
	{ "SolidColor", 1},
	{ "Hyperlinking", 1},
	{ "XlinkAttribute", 1},
	{ "ExternalResourcesRequired", 1},
	{ "Scripting", 1},
	{ "Handler", 1},
	{ "Listener", 1},
	{ "TimedAnimation", 1},
	{ "Animation", 1},
	{ "Audio", 1},
	{ "Video", 1},
	{ "Font", 1},
	{ "Extensibility", 1},
	{ "MediaAttribute", 1},
	{ "TextFlow", 1},
	{ "TransformedVideo", 1},
	{ "ComposedVideo", 1},
	{ "EditableTextAttribute", 1},

	{ "SVG-static", 1},
	{ "SVG-static-DOM", 1},
	{ "SVG-animated", 1},
	{ "SVG-all", 1},
	{ "SVG-interactive", 1},
};


Bool compositor_svg_evaluate_conditional(GF_Compositor *compositor, SVGAllAttributes *atts)
{
	u32 i, count;
	Bool found;
	const char *lang_3cc, *lang_2cc;

	/*process required features*/
	count = atts->requiredFeatures ? gf_list_count(*atts->requiredFeatures) : 0;
	for (i=0; i<count; i++) {
		char *feat = NULL;
		XMLRI *iri = gf_list_get(*atts->requiredFeatures, i);
		if (!iri->string) continue;

		if (!strnicmp(iri->string, "org.w3c.svg", 11)) {
			feat = iri->string+12;
			if (feat) {
				if (!stricmp(feat, "animation")) {}
				else if (!stricmp(feat, "dynamic")) {}
				/*no support for filters, clipping & co - SVG 1.0 featureStrings are badly designed*/
				else return 0;
			}
		}
		else if (!strnicmp(iri->string, "http://www.w3.org/TR/SVG11/feature", 34)) {
			feat = iri->string+35;
			if (feat) {
				Bool found = 0;
				u32 j, nbf;
				nbf  = sizeof(svg11_features) / sizeof(struct svg_11_feature);
				for (j=0; j<nbf; j++) {
					if (!strcmp(svg11_features[j].name, feat)) {
						found = 1;
						if (!svg11_features[j].supported) return 0;
						break;
					}
				}
				if (!found) return 0;
			}
		}
		else if (!strnicmp(iri->string, "http://www.w3.org/Graphics/SVG/feature/1.2/", 43)) {
			feat = iri->string+44;
			if (feat) {
				Bool found = 0;
				u32 j, nbf;
				nbf  = sizeof(svg12_features) / sizeof(struct svg_12_feature);
				for (j=0; j<nbf; j++) {
					if (!strcmp(svg12_features[j].name, feat)) {
						found = 1;
						if (!svg12_features[j].supported) return 0;
						break;
					}
				}
				if (!found) return 0;
			}
		}
		/*unrecognized feature*/
		else {
			return 0;
		}
	}

	/*process required extensions*/
	count = atts->requiredExtensions ? gf_list_count(*atts->requiredExtensions) : 0;
	if (count) return 0;

	/*process system language*/
	count = atts->systemLanguage ? gf_list_count(*atts->systemLanguage) : 0;
	if (count) {
		found = 0;
		lang_3cc = gf_cfg_get_key(compositor->user->config, "Systems", "Language3CC");
		if (!lang_3cc) lang_3cc = "und";
		lang_2cc = gf_cfg_get_key(compositor->user->config, "Systems", "Language2CC");
		if (!lang_2cc) lang_2cc = "un";
	} else {
		lang_3cc = "und";
		lang_2cc = "un";
		found = 1;
	}

	for (i=0; i<count; i++) {
		char *lang = gf_list_get(*atts->systemLanguage, i);
		/*3 char-code*/
		if (strlen(lang)==3) {
			if (!stricmp(lang, lang_3cc)) {
				found = 1;
				break;
			}
		}
		/*2 char-code, only check first 2 chars - TODO FIXME*/
		else if (!strnicmp(lang, lang_2cc, 2)) {
			found = 1;
			break;
		}
	}
	if (!found) return 0;

	/*process required formats*/
	count = atts->requiredFormats ? gf_list_count(*atts->requiredFormats) : 0;
	if (count) {
		for (i=0; i<count; i++) {
			const char *opt;
			char *mime = gf_list_get(*atts->requiredFormats, i);
			char *sep = strchr(mime, ';');
			if (sep) sep[0] = 0;
			opt = gf_cfg_get_key(compositor->user->config, "MimeTypes", mime);
			if (sep) sep[0] = ';';
			if (!opt) return 0;
		}
	}

	/*process required fonts*/
	count = atts->requiredFonts ? gf_list_count(*atts->requiredFonts) : 0;
	if (count) {
		for (i=0; i<count; i++) {
			char *font = gf_list_get(*atts->requiredFonts, i);
			if (gf_font_manager_set_font_ex(compositor->font_manager, &font, 1, 0, 1)==NULL)
				return 0;
		}
	}

	/*OK, we can render this one*/
	return 1;
}

Bool compositor_svg_traverse_base(GF_Node *node, SVGAllAttributes *atts, GF_TraverseState *tr_state,
                                  SVGPropertiesPointers *backup_props, u32 *backup_flags)
{
	u32 inherited_flags_mask, flags;

	if (atts->requiredFeatures || atts->requiredExtensions || atts->systemLanguage
	        || atts->requiredFonts || atts->requiredFormats) {
		if (!compositor_svg_evaluate_conditional(tr_state->visual->compositor, atts))
			return 0;
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

	return 1;
}
#endif


