/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean le Feuvre - Jean-Claude Moissinac
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Loader module
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

#include <gpac/scenegraph_svg.h>
#include <gpac/internal/scenegraph_dev.h>
 
 /* Sets all SVG Properties to their initial value 
   The properties are then updated when going down the tree using svg_properties_apply
   TODO: Check that all properties are there */
void gf_svg_properties_init_pointers(SVGPropertiesPointers *svg_props) 
{
	if (!svg_props) return;

	GF_SAFEALLOC(svg_props->fill, sizeof(SVG_Paint));
	svg_props->fill->type = SVG_PAINT_COLOR;
	svg_props->fill->color.type = SVG_COLOR_RGBCOLOR;
	svg_props->fill->color.red = 0;
	svg_props->fill->color.green = 0;
	svg_props->fill->color.blue = 0;

	GF_SAFEALLOC(svg_props->fill_rule, sizeof(SVG_FillRule));
	*svg_props->fill_rule = SVG_FILLRULE_NONZERO;

	GF_SAFEALLOC(svg_props->fill_opacity, sizeof(SVG_Opacity));
	svg_props->fill_opacity->type = SVG_NUMBER_VALUE;
	svg_props->fill_opacity->value = FIX_ONE;
	
	GF_SAFEALLOC(svg_props->stroke, sizeof(SVG_Paint));
	svg_props->stroke->type = SVG_PAINT_NONE;
	svg_props->stroke->color.type = SVG_COLOR_RGBCOLOR;

	GF_SAFEALLOC(svg_props->stroke_opacity, sizeof(SVG_Opacity));
	svg_props->stroke_opacity->type = SVG_NUMBER_VALUE;
	svg_props->stroke_opacity->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->stroke_width, sizeof(SVG_StrokeWidth));
	svg_props->stroke_width->type = SVG_NUMBER_VALUE;
	svg_props->stroke_width->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->stroke_linecap, sizeof(SVG_StrokeLineCap));
	*(svg_props->stroke_linecap) = SVG_STROKELINECAP_BUTT;
	GF_SAFEALLOC(svg_props->stroke_linejoin, sizeof(SVG_StrokeLineJoin));
	*(svg_props->stroke_linejoin) = SVG_STROKELINEJOIN_MITER;

	GF_SAFEALLOC(svg_props->stroke_miterlimit, sizeof(SVG_StrokeMiterLimit));
	svg_props->stroke_miterlimit->type = SVG_NUMBER_VALUE;
	svg_props->stroke_miterlimit->value = 4*FIX_ONE;

	GF_SAFEALLOC(svg_props->stroke_dashoffset , sizeof(SVG_StrokeDashOffset));
	svg_props->stroke_dashoffset->type = SVG_NUMBER_VALUE;
	svg_props->stroke_dashoffset->value = 0;

	GF_SAFEALLOC(svg_props->stroke_dasharray, sizeof(SVG_StrokeDashArray));
	svg_props->stroke_dasharray->type = SVG_STROKEDASHARRAY_NONE;

	GF_SAFEALLOC(svg_props->font_family, sizeof(SVG_FontFamily));
	svg_props->font_family->type = SVG_FONTFAMILY_VALUE;
	svg_props->font_family->value = strdup("Arial");

	GF_SAFEALLOC(svg_props->font_size, sizeof(SVG_FontSize));
	svg_props->font_size->type = SVG_NUMBER_VALUE;
	svg_props->font_size->value = 12*FIX_ONE;

	GF_SAFEALLOC(svg_props->font_style, sizeof(SVG_FontStyle));
	*(svg_props->font_style) = SVG_FONTSTYLE_NORMAL;

	GF_SAFEALLOC(svg_props->color, sizeof(SVG_Paint));
	svg_props->color->type = SVG_PAINT_COLOR;
	svg_props->color->color.type = SVG_COLOR_RGBCOLOR;
	/* svg_props->color->red, green, blue set to zero, so initial value for color property is black */

	GF_SAFEALLOC(svg_props->text_anchor, sizeof(SVG_TextAnchor));
	*svg_props->text_anchor = SVG_TEXTANCHOR_START;

	GF_SAFEALLOC(svg_props->visibility, sizeof(SVG_Visibility));
	*svg_props->visibility = SVG_VISIBILITY_VISIBLE;

	GF_SAFEALLOC(svg_props->display, sizeof(SVG_Display));
	*svg_props->display = SVG_DISPLAY_INLINE;

	GF_SAFEALLOC(svg_props->stop_opacity, sizeof(SVG_Opacity));
	svg_props->stop_opacity->type = SVG_NUMBER_VALUE;
	svg_props->stop_opacity->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->opacity, sizeof(SVG_Opacity));
	svg_props->opacity->type = SVG_NUMBER_VALUE;
	svg_props->opacity->value = FIX_ONE;
}

void gf_svg_properties_reset_pointers(SVGPropertiesPointers *svg_props)
{
	if (!svg_props) return;
	gf_svg_delete_paint(svg_props->fill);
	if(svg_props->fill_rule) free(svg_props->fill_rule);
	if(svg_props->fill_opacity) free(svg_props->fill_opacity);
	if(svg_props->opacity) free(svg_props->opacity);
	gf_svg_delete_paint(svg_props->stroke);
	if(svg_props->stroke_opacity) free(svg_props->stroke_opacity);
	if(svg_props->stroke_width) free(svg_props->stroke_width);
	if(svg_props->stroke_linecap) free(svg_props->stroke_linecap);
	if(svg_props->stroke_linejoin) free(svg_props->stroke_linejoin);
	if(svg_props->stroke_miterlimit) free(svg_props->stroke_miterlimit);
	if(svg_props->stroke_dashoffset) free(svg_props->stroke_dashoffset);
	if(svg_props->stroke_dasharray) {
		if (svg_props->stroke_dasharray->array.count) free(svg_props->stroke_dasharray->array.vals);
		free(svg_props->stroke_dasharray);
	}
	if(svg_props->font_family) {
		if (svg_props->font_family->value) free(svg_props->font_family->value);
		free(svg_props->font_family);
	}
	if(svg_props->font_size) free(svg_props->font_size);
	if(svg_props->font_style) free(svg_props->font_style);
	gf_svg_delete_paint(svg_props->color);
	if(svg_props->text_anchor) free(svg_props->text_anchor);
	if(svg_props->visibility) free(svg_props->visibility);
	if(svg_props->display) free(svg_props->display);
	if(svg_props->stop_opacity) free(svg_props->stop_opacity);
	memset(svg_props, 0, sizeof(SVGPropertiesPointers));
}

/* Updates the passed SVG Styling Properties Pointers with the properties of the given SVG element:
	1- applies inheritance whenever needed.
	2- applies any running animation on the element

	TODO: Check if all properties are implemented 
*/
void gf_svg_properties_apply(GF_Node *node, SVGPropertiesPointers *render_svg_props)
{
	u32 count_all, count, i,j;
	SVGElement *elt = (SVGElement*)node;

	/*Step 1: perform inheritance*/
	if (render_svg_props && elt->properties) {
		if (elt->properties->color.color.type != SVG_COLOR_INHERIT) {
			render_svg_props->color = &elt->properties->color;
		}
		if (elt->properties->fill.type != SVG_PAINT_INHERIT) {
			render_svg_props->fill = &(elt->properties->fill);
		}
		if (elt->properties->fill_rule != SVG_FILLRULE_INHERIT) {
			render_svg_props->fill_rule = &elt->properties->fill_rule;
		}
		if (elt->properties->fill_opacity.type != SVG_NUMBER_INHERIT) {
			render_svg_props->fill_opacity = &elt->properties->fill_opacity;
		}
		if (elt->properties->stroke.type != SVG_PAINT_INHERIT) {
			render_svg_props->stroke = &elt->properties->stroke;
		}
		if (elt->properties->stroke_opacity.type != SVG_NUMBER_INHERIT) {
			render_svg_props->stroke_opacity = &elt->properties->stroke_opacity;
		}
		if (elt->properties->stroke_width.type != SVG_NUMBER_INHERIT) {
			render_svg_props->stroke_width = &elt->properties->stroke_width;
		}
		if (elt->properties->stroke_miterlimit.type != SVG_NUMBER_INHERIT) {
			render_svg_props->stroke_miterlimit = &elt->properties->stroke_miterlimit;
		}
		if (elt->properties->stroke_linecap != SVG_STROKELINECAP_INHERIT) {
			render_svg_props->stroke_linecap = &elt->properties->stroke_linecap;
		}
		if (elt->properties->stroke_linejoin != SVG_STROKELINEJOIN_INHERIT) {
			render_svg_props->stroke_linejoin = &elt->properties->stroke_linejoin;
		}
		if (elt->properties->stroke_dashoffset.type != SVG_NUMBER_INHERIT) {
			render_svg_props->stroke_dashoffset = &elt->properties->stroke_dashoffset;
		}
		if (elt->properties->stroke_dasharray.type != SVG_STROKEDASHARRAY_INHERIT) {
			render_svg_props->stroke_dasharray = &elt->properties->stroke_dasharray;
		}
		if (elt->properties->font_family.type != SVG_FONTFAMILY_INHERIT) {
			render_svg_props->font_family = &elt->properties->font_family;
		}
		if (elt->properties->font_size.type != SVG_NUMBER_INHERIT) {
			render_svg_props->font_size = &elt->properties->font_size;
		}
		if (elt->properties->font_style != SVG_FONTSTYLE_INHERIT) {
			render_svg_props->font_style = &elt->properties->font_style;
		}
		if (elt->properties->text_anchor != SVG_TEXTANCHOR_INHERIT) {
			render_svg_props->text_anchor = &elt->properties->text_anchor;
		}
		if (elt->properties->visibility != SVG_VISIBILITY_INHERIT) {
			render_svg_props->visibility = &elt->properties->visibility;
		}
		if (elt->properties->display != SVG_DISPLAY_INHERIT) {
			render_svg_props->display = &elt->properties->display;
		}
		if (elt->properties->opacity.type != SVG_NUMBER_INHERIT) {
			render_svg_props->opacity = &elt->properties->opacity;
		}
	}

	/*Step 2: handle all animations*/

	/*TODO FIXME - THIS IS WRONG, we're changing orders of animations which may corrupt the visual result*/
	count_all = gf_node_animation_count(node);
	/* Loop 1: For all animated attributes (target_attribute) */
	for (i = 0; i < count_all; i++) {
		GF_FieldInfo underlying_value;
		SMIL_AttributeAnimations *aa = gf_node_animation_get(node, i);		
		count = gf_list_count(aa->anims);
		if (!count) continue;

		/* initializing the type of the underlying value */
		underlying_value.fieldType = aa->saved_dom_value.fieldType;		
		/* the pointer contains the result of the previous animation cycle,
		   it needs to be reset */
		underlying_value.far_ptr = aa->presentation_value.far_ptr;
		
		/* The resetting is done either based on the inherited value or based on the (saved) DOM value 
		   or in special cases on the inherited color value */
		gf_svg_attributes_copy_computed_value(&underlying_value, &aa->saved_dom_value, (SVGElement*)node, aa->orig_dom_ptr, render_svg_props);

		/* we also need a special handling of current color if used in animation values */
		aa->current_color_value.fieldType = SVG_Paint_datatype;
		aa->current_color_value.far_ptr = render_svg_props->color;

		/* Loop 2: For all animations (anim) */
		for (j = 0; j < count; j++) {
			SMIL_Anim_RTI *rai = gf_list_get(aa->anims, j);			
			gf_smil_timing_notify_time(rai->anim_elt->timing->runtime, gf_node_get_scene_time(node));
		}
		/* end of Loop 2 */

		
		/*TODO FIXME, we need a finer granularity here and we must know if the animated attribute has changed or not (freeze)...*/
		gf_node_dirty_set(node, GF_SG_SVG_GEOMETRY_DIRTY | GF_SG_SVG_APPEARANCE_DIRTY, 0);

		gf_node_changed(node, NULL);
//		gf_sr_invalidate(eff->surface->render->compositor, NULL);
	}
}

