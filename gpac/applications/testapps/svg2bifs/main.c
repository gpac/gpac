/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril COncolato
 *			Copyright (c) Telecom ParisTech 2006-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / svg2bifs application
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

#include <gpac/scene_manager.h>
#include <gpac/xml.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_svg.h>
#include <gpac/nodes_mpeg4.h>

typedef struct {
	GF_SAXParser *sax_parser;

	GF_SceneGraph *svg_sg;
	GF_Node *svg_parent;
	SVGAllAttributes all_atts;
	SVGPropertiesPointers svg_props;

	GF_SceneGraph *bifs_sg;
	GF_Node *bifs_parent;
	GF_Node *bifs_text_node;

	Bool force_transform;

} SVG2BIFS_Converter;


typedef struct {
	/* Stage of the resolving:
	    0: resolving attributes which depends on the target: from, to, by, values, type
		1: resolving begin times
		2: resolving end times */
	u32 resolve_stage;
	/* Animation element being deferred */
	SVG_Element *animation_elt;
	/* anim parent*/
	SVG_Element *anim_parent;
	/* target animated element*/
	SVG_Element *target;
	/* id of the target element when unresolved*/
	char *target_id;

	/* attributes which cannot be parsed until the type of the target attribute is known */
	char *type; /* only for animateTransform */
	char *to;
	char *from;
	char *by;
	char *values;
} SVG_DeferredAnimation;


static GF_Node *create_appearance(SVGPropertiesPointers *svg_props, GF_SceneGraph *sg)
{
	M_Appearance *app;
	M_Material2D *mat;
	M_XLineProperties *xlp;
/*	M_RadialGradient *rg;
	M_LinearGradient *lg;
*/
	app = (M_Appearance *)gf_node_new(sg, TAG_MPEG4_Appearance);

	app->material = gf_node_new(sg, TAG_MPEG4_Material2D);
	mat = (M_Material2D *)app->material;
	gf_node_register((GF_Node*)mat, (GF_Node*)app);

	if (svg_props->fill->type == SVG_PAINT_NONE) {
		mat->filled = 0;
	} else {
		mat->filled = 1;
		if (svg_props->fill->type == SVG_PAINT_COLOR) {
			if (svg_props->fill->color.type == SVG_COLOR_RGBCOLOR) {
				mat->emissiveColor.red = svg_props->fill->color.red;
				mat->emissiveColor.green = svg_props->fill->color.green;
				mat->emissiveColor.blue = svg_props->fill->color.blue;
			} else if (svg_props->fill->color.type == SVG_COLOR_CURRENTCOLOR) {
				mat->emissiveColor.red = svg_props->color->color.red;
				mat->emissiveColor.green = svg_props->color->color.green;
				mat->emissiveColor.blue = svg_props->color->color.blue;
			} else {
				/* WARNING */
				mat->emissiveColor.red = 0;
				mat->emissiveColor.green = 0;
				mat->emissiveColor.blue = 0;
			}
		} else { // SVG_PAINT_URI
			/* TODO: gradient or solidcolor */
		}
	}

	mat->transparency = FIX_ONE - svg_props->fill_opacity->value;

	if (svg_props->stroke->type != SVG_PAINT_NONE &&
	        svg_props->stroke_width->value != 0) {
		mat->lineProps = gf_node_new(sg, TAG_MPEG4_XLineProperties);
		xlp = (M_XLineProperties *)mat->lineProps;
		gf_node_register((GF_Node*)xlp, (GF_Node*)mat);

		xlp->width = svg_props->stroke_width->value;

		if (svg_props->stroke->type == SVG_PAINT_COLOR) {
			if (svg_props->stroke->color.type == SVG_COLOR_RGBCOLOR) {
				xlp->lineColor.red = svg_props->stroke->color.red;
				xlp->lineColor.green = svg_props->stroke->color.green;
				xlp->lineColor.blue = svg_props->stroke->color.blue;
			} else if (svg_props->stroke->color.type == SVG_COLOR_CURRENTCOLOR) {
				xlp->lineColor.red = svg_props->color->color.red;
				xlp->lineColor.green = svg_props->color->color.green;
				xlp->lineColor.blue = svg_props->color->color.blue;
			} else {
				/* WARNING */
				xlp->lineColor.red = 0;
				xlp->lineColor.green = 0;
				xlp->lineColor.blue = 0;
			}
		} else { // SVG_PAINT_URI
			/* TODO: xlp->texture =  ... */
		}
		xlp->transparency = FIX_ONE - svg_props->stroke_opacity->value;
		xlp->lineCap = *svg_props->stroke_linecap;
		xlp->lineJoin = *svg_props->stroke_linejoin;
		if (svg_props->stroke_dasharray->type == SVG_STROKEDASHARRAY_ARRAY) {
			u32 i;
			xlp->lineStyle = 6;
			gf_sg_vrml_mf_alloc(&xlp->dashes, GF_SG_VRML_MFFLOAT, svg_props->stroke_dasharray->array.count);
			for (i = 0; i < svg_props->stroke_dasharray->array.count; i++) {
				xlp->dashes.vals[i] = svg_props->stroke_dasharray->array.vals[i] / svg_props->stroke_width->value;
			}
		}
		xlp->miterLimit = svg_props->stroke_miterlimit->value;
	}

	return (GF_Node*)app;
}



static GF_Node *add_transform_matrix(SVG2BIFS_Converter *converter, GF_Node *node)
{
	M_TransformMatrix2D *tr = (M_TransformMatrix2D*)gf_node_new(converter->bifs_sg, TAG_MPEG4_TransformMatrix2D);
	gf_node_register((GF_Node *)tr, node);
	gf_node_list_add_child(&((GF_ParentNode*)node)->children, (GF_Node *)tr);
	if (converter->all_atts.transform) {
		SVG_Transform *svg_tr = converter->all_atts.transform;
		tr->mxx = svg_tr->mat.m[0];
		tr->mxy = svg_tr->mat.m[1];
		tr->tx  = svg_tr->mat.m[2];
		tr->myx = svg_tr->mat.m[3];
		tr->myy = svg_tr->mat.m[4];
		tr->ty  = svg_tr->mat.m[5];
	}
	return (GF_Node *)tr;

}

static GF_Node *add_transform2d(SVG2BIFS_Converter *converter, GF_Node *node)
{
	M_Transform2D *tr = (M_Transform2D*)gf_node_new(converter->bifs_sg, TAG_MPEG4_Transform2D);
	gf_node_register((GF_Node *)tr, node);
	gf_node_list_add_child(&((GF_ParentNode*)node)->children, (GF_Node *)tr);
	return (GF_Node *)tr;
}

static void svg_parse_animation(GF_SceneGraph *sg, SVG_DeferredAnimation *anim)
{
	GF_FieldInfo info;
	u32 tag;
	u8 anim_value_type = 0;

	if (anim->resolve_stage==0) {
		/* Stage 0: parsing the animation attribute values
					for that we need to resolve the target first */
		if (!anim->target)
			anim->target = (SVG_Element *) gf_sg_find_node_by_name(sg, anim->target_id + 1);

		if (!anim->target) {
			/* the target is still not known stay in stage 0 */
			return;
		} else {
			XMLRI *iri;
			gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_XLINK_ATT_href, 1, 0, &info);
			iri = (XMLRI *)info.far_ptr;
			iri->type = XMLRI_ELEMENTID;
			iri->target = anim->target;
			gf_node_register_iri(sg, iri);
		}

		tag = gf_node_get_tag((GF_Node *)anim->animation_elt);
		/* get the attribute name attribute if specified */
		if (anim->type && (tag== TAG_SVG_animateTransform) ) {
			gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_transform_type, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->type, 0);
			switch(*(SVG_TransformType *) info.far_ptr) {
			case SVG_TRANSFORM_TRANSLATE:
				anim_value_type = SVG_Transform_Translate_datatype;
				break;
			case SVG_TRANSFORM_SCALE:
				anim_value_type = SVG_Transform_Scale_datatype;
				break;
			case SVG_TRANSFORM_ROTATE:
				anim_value_type = SVG_Transform_Rotate_datatype;
				break;
			case SVG_TRANSFORM_SKEWX:
				anim_value_type = SVG_Transform_SkewX_datatype;
				break;
			case SVG_TRANSFORM_SKEWY:
				anim_value_type = SVG_Transform_SkewY_datatype;
				break;
			case SVG_TRANSFORM_MATRIX:
				anim_value_type = SVG_Transform_datatype;
				break;
			default:
				fprintf(stdout, "unknown datatype for animate transform");
				return;
			}
		}
		else if (gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_attributeName, 0, 0, &info) == GF_OK) {
			gf_node_get_attribute_by_name((GF_Node *)anim->target, ((SMIL_AttributeName *)info.far_ptr)->name, 0, 1, 1, &info);
			anim_value_type = info.fieldType;
		} else {
			if (tag == TAG_SVG_animateMotion) {
				anim_value_type = SVG_Motion_datatype;
			} else if (tag == TAG_SVG_discard) {
				/* there is no value to parse in discard, we can jump to the next stage */
				anim->resolve_stage = 1;
				svg_parse_animation(sg, anim);
				return;
			} else {
				fprintf(stdout, "Missing attributeName attribute on %s", gf_node_get_name((GF_Node *)anim->animation_elt));
				return;
			}
		}

		if (anim->to) {
			gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_to, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->to, anim_value_type);
		}
		if (anim->from) {
			gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_from, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->from, anim_value_type);
		}
		if (anim->by) {
			gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_by, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->by, anim_value_type);
		}
		if (anim->values) {
			gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_values, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->values, anim_value_type);
		}
		anim->resolve_stage = 1;
	}
}


static void svg2bifs_node_start(void *sax_cbck, const char *name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	u32 i;
	SVG2BIFS_Converter *converter = (SVG2BIFS_Converter *)sax_cbck;
	SVGPropertiesPointers *backup_props;
	char *id_string = NULL;
	u32	tag;
	SVG_Element *elt;
	SVG_DeferredAnimation *anim = NULL;

	tag = gf_xml_get_element_tag(name, 0);
	elt = (SVG_Element*)gf_node_new(converter->svg_sg, tag);
	if (!gf_sg_get_root_node(converter->svg_sg)) {
		gf_node_register((GF_Node *)elt, NULL);
		gf_sg_set_root_node(converter->svg_sg, (GF_Node *)elt);
	} else {
		gf_node_register((GF_Node *)elt, converter->svg_parent);
		//gf_node_list_add_child(&((GF_ParentNode*)converter->svg_parent)->children, (GF_Node *)elt);
	}

//	fprintf(stdout, "Converting %s\n", gf_node_get_class_name((GF_Node *)elt));
//	if (converter->bifs_parent) fprintf(stdout, "%s\n", gf_node_get_class_name(converter->bifs_parent));

	if (gf_svg_is_animation_tag(tag)) {
		GF_SAFEALLOC(anim, SVG_DeferredAnimation);
		/*default anim target is parent node*/
		anim->animation_elt = elt;
		if (converter->svg_parent) {
			anim->target = anim->anim_parent = (SVG_Element*) converter->svg_parent;
		}
	}

	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
		if (!att->value || !strlen(att->value)) continue;

		if (!stricmp(att->name, "style")) {
			gf_svg_parse_style((GF_Node *)elt, att->value);
		} else if (!stricmp(att->name, "id") || !stricmp(att->name, "xml:id")) {
			gf_svg_parse_element_id((GF_Node *)elt, att->value, 0);
			id_string = att->value;
		} else if (anim && !stricmp(att->name, "to")) {
			anim->to = gf_strdup(att->value);
		} else if (anim && !stricmp(att->name, "from")) {
			anim->from = gf_strdup(att->value);
		} else if (anim && !stricmp(att->name, "by")) {
			anim->by = gf_strdup(att->value);
		} else if (anim && !stricmp(att->name, "values")) {
			anim->values = gf_strdup(att->value);
		} else if (anim && (tag == TAG_SVG_animateTransform) && !stricmp(att->name, "type")) {
			anim->type = gf_strdup(att->value);
		} else {
			GF_FieldInfo info;
			if (gf_node_get_field_by_name((GF_Node *)elt, att->name, &info)==GF_OK) {
				gf_svg_parse_attribute((GF_Node *)elt, &info, att->value, 0);
			} else {
				fprintf(stdout, "Skipping attribute %s\n", att->name);
			}
		}
	}

	if (anim) {
		svg_parse_animation(converter->svg_sg, anim);
	}

	memset(&converter->all_atts, 0, sizeof(SVGAllAttributes));
	gf_svg_flatten_attributes(elt, &converter->all_atts);

	backup_props = gf_malloc(sizeof(SVGPropertiesPointers));
	memcpy(backup_props, &converter->svg_props, sizeof(SVGPropertiesPointers));
	gf_node_set_private((GF_Node *)elt, backup_props);

	gf_svg_apply_inheritance(&converter->all_atts, &converter->svg_props);

	fprintf(stdout, "START\t%s\t%s\t%s", converter->svg_parent ? gf_node_get_class_name(converter->svg_parent) : "none", converter->bifs_parent ? gf_node_get_class_name(converter->bifs_parent) : "none", name);
	converter->svg_parent = (GF_Node *)elt;
	if (!gf_sg_get_root_node(converter->bifs_sg)) {
		if (tag == TAG_SVG_svg) {
			GF_Node *node, *child;

			converter->bifs_sg->usePixelMetrics = 1;
			if (converter->all_atts.width && converter->all_atts.width->type == SVG_NUMBER_VALUE) {
				converter->bifs_sg->width = FIX2INT(converter->all_atts.width->value);
			} else {
				converter->bifs_sg->width = 320;
			}
			if (converter->all_atts.height && converter->all_atts.height->type == SVG_NUMBER_VALUE) {
				converter->bifs_sg->height = FIX2INT(converter->all_atts.height->value);
			} else {
				converter->bifs_sg->height = 200;
			}

			node = gf_node_new(converter->bifs_sg, TAG_MPEG4_OrderedGroup);
			gf_node_register(node, NULL);
			gf_sg_set_root_node(converter->bifs_sg, node);

			child = gf_node_new(converter->bifs_sg, TAG_MPEG4_QuantizationParameter);
			gf_node_register(child, node);
			gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
			{
				M_QuantizationParameter *qp = (M_QuantizationParameter *)child;
				qp->useEfficientCoding = 1;
			}

			/* SVG to BIFS coordinate transformation */
			child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Viewport);
			gf_node_register(child, node);
			gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
			{
				M_Viewport *vp = (M_Viewport*)child;
				if (converter->all_atts.viewBox) {
					vp->size.x = converter->all_atts.viewBox->width;
					vp->size.y = converter->all_atts.viewBox->height;
					vp->position.x = converter->all_atts.viewBox->x+converter->all_atts.viewBox->width/2;
					vp->position.y = -(converter->all_atts.viewBox->y+converter->all_atts.viewBox->height/2);
				} else {
					vp->size.x = INT2FIX(converter->bifs_sg->width);
					vp->size.y = INT2FIX(converter->bifs_sg->height);
					vp->position.x = INT2FIX(converter->bifs_sg->width)/2;
					vp->position.y = -INT2FIX(converter->bifs_sg->height)/2;
				}
			}

			child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Background2D);
			gf_node_register(child, node);
			gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
			{
				M_Background2D *b = (M_Background2D *)child;
				b->backColor.red = FIX_ONE;
				b->backColor.green = FIX_ONE;
				b->backColor.blue = FIX_ONE;
			}

			child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Transform2D);
			gf_node_register(child, node);
			gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
			node = child;
			child = NULL;
			{
				M_Transform2D *tr = (M_Transform2D *)node;
				tr->scale.y = -FIX_ONE;
			}
			converter->bifs_parent = node;
		}
	} else {
		GF_Node *node, *child;

		node = converter->bifs_parent;

		switch(tag) {
		case TAG_SVG_g:
		{
			if (converter->all_atts.transform) {
				node = add_transform_matrix(converter, node);
				converter->bifs_parent = node;
			} else {
				M_Group *g = (M_Group*)gf_node_new(converter->bifs_sg, TAG_MPEG4_Group);
				gf_node_register((GF_Node *)g, node);
				gf_node_list_add_child(&((GF_ParentNode*)node)->children, (GF_Node *)g);
				node = (GF_Node *)g;
				converter->bifs_parent = node;
			}
		}
		break;
		case TAG_SVG_rect:
		{
			Bool is_parent_set = 0;
			if (converter->all_atts.transform) {
				node = add_transform_matrix(converter, node);
				converter->bifs_parent = node;
				is_parent_set = 1;
			}
			if (converter->force_transform) {
				node = add_transform2d(converter, node);
				if (!is_parent_set) {
					converter->bifs_parent = node;
					is_parent_set = 1;
				}
			}
			if (converter->all_atts.x || converter->all_atts.y) {
				child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Transform2D);
				gf_node_register(child, node);
				gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
				node = child;
				child = NULL;
				if (!is_parent_set) {
					converter->bifs_parent = node;
					is_parent_set = 1;
				}
				{
					M_Transform2D *tr = (M_Transform2D *)node;
					if (converter->all_atts.x) tr->translation.x = converter->all_atts.x->value + (converter->all_atts.width?converter->all_atts.width->value/2:0);
					if (converter->all_atts.y) tr->translation.y = converter->all_atts.y->value + (converter->all_atts.height?converter->all_atts.height->value/2:0);
				}
			}
			child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Shape);
			gf_node_register(child, node);
			gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
			node = child;
			child = NULL;
			if (!is_parent_set) converter->bifs_parent = node;
			{
				M_Shape *shape = (M_Shape *)node;
				shape->geometry = gf_node_new(converter->bifs_sg, TAG_MPEG4_Rectangle);
				gf_node_register(shape->geometry, (GF_Node *)shape);
				{
					M_Rectangle *rect = (M_Rectangle *)shape->geometry;
					if (converter->all_atts.width) rect->size.x = converter->all_atts.width->value;
					if (converter->all_atts.height) rect->size.y = converter->all_atts.height->value;
				}

				shape->appearance = create_appearance(&converter->svg_props, converter->bifs_sg);
				gf_node_register(shape->appearance, (GF_Node *)shape);
			}
		}
		break;
		case TAG_SVG_path:
		{
			Bool is_parent_set = 0;
			if (converter->all_atts.transform) {
				node = add_transform_matrix(converter, node);
				converter->bifs_parent = node;
				is_parent_set = 1;
			}
			if (converter->force_transform) {
				node = add_transform2d(converter, node);
				if (!is_parent_set) {
					converter->bifs_parent = node;
					is_parent_set = 1;
				}
			}
			if (converter->all_atts.x || converter->all_atts.y) {
				child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Transform2D);
				gf_node_register(child, node);
				gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
				node = child;
				child = NULL;
				if (!is_parent_set) {
					converter->bifs_parent = node;
					is_parent_set = 1;
				}
				{
					M_Transform2D *tr = (M_Transform2D *)node;
					if (converter->all_atts.x) tr->translation.x = converter->all_atts.x->value;
					if (converter->all_atts.y) tr->translation.y = converter->all_atts.y->value;
				}
			}
			child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Shape);
			gf_node_register(child, node);
			gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
			node = child;
			child = NULL;
			if (!is_parent_set) converter->bifs_parent = node;
			{
				M_Shape *shape = (M_Shape *)node;
				shape->geometry = gf_node_new(converter->bifs_sg, TAG_MPEG4_XCurve2D);
				gf_node_register(shape->geometry, (GF_Node *)shape);
				if (converter->all_atts.d) {
					M_Coordinate2D *c2d;
					M_XCurve2D *xc = (M_XCurve2D *)shape->geometry;
					u32 j, c, k;

					xc->point = gf_node_new(converter->bifs_sg, TAG_MPEG4_Coordinate2D);
					c2d = (M_Coordinate2D *)xc->point;
					gf_node_register(xc->point, (GF_Node *)xc);

					gf_sg_vrml_mf_alloc(&c2d->point, GF_SG_VRML_MFVEC2F, converter->all_atts.d->n_points);
					gf_sg_vrml_mf_alloc(&xc->type, GF_SG_VRML_MFINT32, converter->all_atts.d->n_points);

					c = 0;
					k = 0;
					j = 0;
					c2d->point.vals[k] = converter->all_atts.d->points[0];
					k++;
					xc->type.vals[0] = 0;
					for (i = 1; i < converter->all_atts.d->n_points; ) {
						switch(converter->all_atts.d->tags[i]) {
						case GF_PATH_CURVE_ON:
							c2d->point.vals[k] = converter->all_atts.d->points[i];
							k++;

							if (i-1 == converter->all_atts.d->contours[c]) {
								xc->type.vals[j] = 0;
								c++;
							} else {
								xc->type.vals[j] = 1;
							}
							i++;
							break;
						case GF_PATH_CURVE_CUBIC:
							c2d->point.vals[k] = converter->all_atts.d->points[i];
							c2d->point.vals[k+1] = converter->all_atts.d->points[i+1];
							c2d->point.vals[k+2] = converter->all_atts.d->points[i+2];
							k+=3;

							xc->type.vals[j] = 2;
							if (converter->all_atts.d->tags[i+2]==GF_PATH_CLOSE)  {
								j++;
								xc->type.vals[j] = 6;
							}
							i+=3;
							break;
						case GF_PATH_CLOSE:
							xc->type.vals[j] = 6;
							i++;
							break;
						case GF_PATH_CURVE_CONIC:
							c2d->point.vals[k] = converter->all_atts.d->points[i];
							c2d->point.vals[k+1] = converter->all_atts.d->points[i+1];
							k+=2;

							xc->type.vals[j] = 7;
							if (converter->all_atts.d->tags[i+1]==GF_PATH_CLOSE)  {
								j++;
								xc->type.vals[j] = 6;
							}
							i+=2;
							break;
						}
						j++;
					}
					xc->type.count = j;
					c2d->point.count = k;
				}

				shape->appearance = create_appearance(&converter->svg_props, converter->bifs_sg);
				gf_node_register(shape->appearance, (GF_Node *)shape);
			}
		}
		break;
		case TAG_SVG_polyline:
		{
			Bool is_parent_set = 0;
			if (converter->all_atts.transform) {
				node = add_transform_matrix(converter, node);
				converter->bifs_parent = node;
				is_parent_set = 1;
			}
			if (converter->force_transform) {
				node = add_transform2d(converter, node);
				if (!is_parent_set) {
					converter->bifs_parent = node;
					is_parent_set = 1;
				}
			}
			child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Shape);
			gf_node_register(child, node);
			gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
			node = child;
			child = NULL;
			if (!is_parent_set) converter->bifs_parent = node;
			{
				M_Shape *shape = (M_Shape *)node;
				shape->geometry = gf_node_new(converter->bifs_sg, TAG_MPEG4_IndexedFaceSet2D);
				gf_node_register(shape->geometry, (GF_Node *)shape);
				if (converter->all_atts.points) {
					M_Coordinate2D *c2d;
					M_IndexedFaceSet2D *ifs = (M_IndexedFaceSet2D *)shape->geometry;

					ifs->coord = gf_node_new(converter->bifs_sg, TAG_MPEG4_Coordinate2D);
					c2d = (M_Coordinate2D *)ifs->coord;
					gf_node_register(ifs->coord, (GF_Node *)ifs);

					gf_sg_vrml_mf_alloc(&c2d->point, GF_SG_VRML_MFVEC2F, gf_list_count(*converter->all_atts.points));
					for (i = 0; i < gf_list_count(*converter->all_atts.points); i++) {
						SVG_Point *p = (SVG_Point *)gf_list_get(*converter->all_atts.points, i);
						c2d->point.vals[i].x = p->x;
						c2d->point.vals[i].y = p->y;
					}
				}

				shape->appearance = create_appearance(&converter->svg_props, converter->bifs_sg);
				gf_node_register(shape->appearance, (GF_Node *)shape);
			}
		}
		break;
		case TAG_SVG_text:
		{
			Bool is_parent_set = 0;
			if (converter->all_atts.transform) {
				node = add_transform_matrix(converter, node);
				converter->bifs_parent = node;
				is_parent_set = 1;
			}
			if (converter->force_transform) {
				node = add_transform2d(converter, node);
				if (!is_parent_set) {
					converter->bifs_parent = node;
					is_parent_set = 1;
				}
			}

			child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Transform2D);
			gf_node_register(child, node);
			gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
			{
				M_Transform2D *tr = (M_Transform2D *)child;
				if (converter->all_atts.text_x) tr->translation.x = ((SVG_Coordinate *)gf_list_get(*converter->all_atts.text_x, 0))->value;
				if (converter->all_atts.text_y) tr->translation.y = ((SVG_Coordinate *)gf_list_get(*converter->all_atts.text_y, 0))->value;
				tr->scale.y = -FIX_ONE;
			}
			node = child;
			child = NULL;
			if (!is_parent_set) {
				converter->bifs_parent = node;
				is_parent_set = 1;
			}

			child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Shape);
			gf_node_register(child, node);
			gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
			node = child;
			child = NULL;
			if (!is_parent_set) converter->bifs_parent = node;
			{
				M_FontStyle *fs;
				M_Text *text;
				M_Shape *shape = (M_Shape *)node;
				text = (M_Text *)gf_node_new(converter->bifs_sg, TAG_MPEG4_Text);
				shape->geometry = (GF_Node *)text;
				converter->bifs_text_node = shape->geometry;
				gf_node_register(shape->geometry, (GF_Node *)shape);

				fs = (M_FontStyle *)gf_node_new(converter->bifs_sg, TAG_MPEG4_XFontStyle);
				gf_node_register((GF_Node *)fs, (GF_Node*)text);
				text->fontStyle = (GF_Node *)fs;

				gf_sg_vrml_mf_alloc(&fs->family, GF_SG_VRML_MFSTRING, 1);
				fs->family.vals[0] = gf_strdup(converter->svg_props.font_family->value);
				fs->size = converter->svg_props.font_size->value;

				shape->appearance = create_appearance(&converter->svg_props, converter->bifs_sg);
				gf_node_register(shape->appearance, (GF_Node *)shape);
			}
		}
		break;
		case TAG_SVG_ellipse:
		case TAG_SVG_circle:
		{
			Bool is_parent_set = 0;
			if (converter->all_atts.transform) {
				node = add_transform_matrix(converter, node);
				converter->bifs_parent = node;
				is_parent_set = 1;
			}
			if (converter->force_transform) {
				node = add_transform2d(converter, node);
				if (!is_parent_set) {
					converter->bifs_parent = node;
					is_parent_set = 1;
				}
			}
			if (converter->all_atts.cx || converter->all_atts.cy) {
				child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Transform2D);
				gf_node_register(child, node);
				gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
				{
					M_Transform2D *tr = (M_Transform2D *)child;
					if (converter->all_atts.cx) tr->translation.x = converter->all_atts.cx->value;
					if (converter->all_atts.cy) tr->translation.y = converter->all_atts.cy->value;
				}
				node = child;
				child = NULL;
				if (!is_parent_set) {
					converter->bifs_parent = node;
					is_parent_set = 1;
				}
			}
			child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Shape);
			gf_node_register(child, node);
			gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
			node = child;
			child = NULL;
			if (!is_parent_set) converter->bifs_parent = node;
			{
				M_Shape *shape = (M_Shape *)node;
				if (tag == TAG_SVG_ellipse) {
					M_Ellipse *e = (M_Ellipse *)gf_node_new(converter->bifs_sg, TAG_MPEG4_Ellipse);
					shape->geometry = (GF_Node *)e;
					e->radius.x = converter->all_atts.rx->value;
					e->radius.y = converter->all_atts.ry->value;
				} else {
					M_Circle *c = (M_Circle *)gf_node_new(converter->bifs_sg, TAG_MPEG4_Circle);
					shape->geometry = (GF_Node *)c;
					c->radius = converter->all_atts.r->value;
				}
				gf_node_register(shape->geometry, (GF_Node *)shape);

				shape->appearance = create_appearance(&converter->svg_props, converter->bifs_sg);
				gf_node_register(shape->appearance, (GF_Node *)shape);
			}
		}
		break;

		case TAG_SVG_defs:
		{
			child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Switch);
			gf_node_register(child, node);
			gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
			node = child;
			child = NULL;
			{
				M_Switch *sw = (M_Switch *)node;
				sw->whichChoice = -1;
			}
			converter->bifs_parent = node;
		}
		break;
		case TAG_SVG_solidColor:
		{
			child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Shape);
			gf_node_register(child, node);
			gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
			node = child;
			child = NULL;
			converter->bifs_parent = node;
		}
		break;
		case TAG_SVG_animateTransform:
		{
			GF_Node *child_ts;
			if (!gf_node_get_id(node)) {
				gf_node_set_id(node, gf_sg_get_next_available_node_id(converter->bifs_sg), NULL);
			}

			child_ts = gf_node_new(converter->bifs_sg, TAG_MPEG4_TimeSensor);
			if (!gf_node_get_id(child_ts)) {
				gf_node_set_id(child_ts, gf_sg_get_next_available_node_id(converter->bifs_sg), NULL);
			}
			gf_node_register(child_ts, node);
			gf_node_list_add_child(&((GF_ParentNode *)node)->children, child_ts);
			{
				M_TimeSensor *ts = (M_TimeSensor *)child_ts;
				if (converter->all_atts.dur) {
					ts->cycleInterval = converter->all_atts.dur->clock_value;
				}
				if (converter->all_atts.repeatCount && converter->all_atts.repeatCount->type == SMIL_REPEATCOUNT_INDEFINITE) {
					ts->loop = 1;
				}
			}

			if (converter->all_atts.transform_type) {
				GF_FieldInfo fromField, toField;

				switch (*converter->all_atts.transform_type) {
				case SVG_TRANSFORM_ROTATE:
					child = gf_node_new(converter->bifs_sg, TAG_MPEG4_PositionInterpolator2D);
					if (!gf_node_get_id(child)) {
						gf_node_set_id(child, gf_sg_get_next_available_node_id(converter->bifs_sg), NULL);
					}
					gf_node_register(child, node);
					gf_node_list_add_child(&((GF_ParentNode *)node)->children, child);

					gf_node_get_field_by_name(child_ts, "fraction_changed", &fromField);
					gf_node_get_field_by_name(child, "set_fraction", &toField);
					gf_sg_route_new(converter->bifs_sg, child_ts, fromField.fieldIndex, child, toField.fieldIndex);

					gf_node_get_field_by_name(child, "value_changed", &fromField);
					gf_node_get_field_by_name(node, "rotationAngle", &toField);
					gf_sg_route_new(converter->bifs_sg, child, fromField.fieldIndex, node, toField.fieldIndex);
					{
						M_PositionInterpolator2D *pi2d = (M_PositionInterpolator2D *)child;
						if (converter->all_atts.keyTimes) {
							SFFloat *g;
							u32 count;
							count = gf_list_count(*converter->all_atts.keyTimes);
							for (i = 0; i < count; i++) {
								Fixed *f = gf_list_get(*converter->all_atts.keyTimes, i);
								gf_sg_vrml_mf_append(&pi2d->key, GF_SG_VRML_MFFLOAT, &g);
								*g = *f;
							}
						}
						if (converter->all_atts.values) {
							SFVec2f *g;
							u32 count;
							count = gf_list_count(converter->all_atts.values->values);
							for (i = 0; i < count; i++) {
								SVG_Point_Angle *p;
								p = gf_list_get(converter->all_atts.values->values, i);
								gf_sg_vrml_mf_append(&pi2d->keyValue, GF_SG_VRML_MFVEC2F, &g);
								g->x = p->x;
								g->y = p->y;
							}
						}
					}


					child = gf_node_new(converter->bifs_sg, TAG_MPEG4_ScalarInterpolator);
					if (!gf_node_get_id(child)) {
						gf_node_set_id(child, gf_sg_get_next_available_node_id(converter->bifs_sg), NULL);
					}
					gf_node_register(child, node);
					gf_node_list_add_child(&((GF_ParentNode *)node)->children, child);

					gf_node_get_field_by_name(child_ts, "fraction_changed", &fromField);
					gf_node_get_field_by_name(child, "set_fraction", &toField);
					gf_sg_route_new(converter->bifs_sg, child_ts, fromField.fieldIndex, child, toField.fieldIndex);

					gf_node_get_field_by_name(child, "value_changed", &fromField);
					gf_node_get_field_by_name(node, "center", &toField);
					gf_sg_route_new(converter->bifs_sg, child, fromField.fieldIndex, node, toField.fieldIndex);

					{
						M_ScalarInterpolator *si = (M_ScalarInterpolator *)child;
						if (converter->all_atts.keyTimes) {
							SFFloat *g;
							u32 count;
							count = gf_list_count(*converter->all_atts.keyTimes);
							for (i = 0; i < count; i++) {
								Fixed *f = gf_list_get(*converter->all_atts.keyTimes, i);
								gf_sg_vrml_mf_append(&si->key, GF_SG_VRML_MFFLOAT, &g);
								*g = *f;
							}
						}
						if (converter->all_atts.values) {
							SFFloat *g;
							u32 count;
							count = gf_list_count(converter->all_atts.values->values);
							for (i = 0; i < count; i++) {
								SVG_Point_Angle *p;
								p = gf_list_get(converter->all_atts.values->values, i);
								gf_sg_vrml_mf_append(&si->keyValue, GF_SG_VRML_MFFLOAT, &g);
								*g = p->angle;
							}
						}
					}

					break;

				case SVG_TRANSFORM_SCALE:
				case SVG_TRANSFORM_TRANSLATE:
					child = gf_node_new(converter->bifs_sg, TAG_MPEG4_PositionInterpolator2D);
					if (!gf_node_get_id(child)) {
						gf_node_set_id(child, gf_sg_get_next_available_node_id(converter->bifs_sg), NULL);
					}
					gf_node_register(child, node);
					gf_node_list_add_child(&((GF_ParentNode *)node)->children, child);

					gf_node_get_field_by_name(child_ts, "fraction_changed", &fromField);
					gf_node_get_field_by_name(child, "set_fraction", &toField);
					gf_sg_route_new(converter->bifs_sg, child_ts, fromField.fieldIndex, child, toField.fieldIndex);

					gf_node_get_field_by_name(child, "value_changed", &fromField);
					if (*converter->all_atts.transform_type == SVG_TRANSFORM_SCALE)
						gf_node_get_field_by_name(node, "scale", &toField);
					else
						gf_node_get_field_by_name(node, "translation", &toField);

					gf_sg_route_new(converter->bifs_sg, child, fromField.fieldIndex, node, toField.fieldIndex);
					{
						M_PositionInterpolator2D *pi2d = (M_PositionInterpolator2D *)child;
						if (converter->all_atts.keyTimes) {
							SFFloat *g;
							u32 count;
							count = gf_list_count(*converter->all_atts.keyTimes);
							for (i = 0; i < count; i++) {
								Fixed *f = gf_list_get(*converter->all_atts.keyTimes, i);
								gf_sg_vrml_mf_append(&pi2d->key, GF_SG_VRML_MFFLOAT, &g);
								*g = *f;
							}
						}
						if (converter->all_atts.values) {
							SFVec2f *g;
							u32 count;
							count = gf_list_count(converter->all_atts.values->values);
							for (i = 0; i < count; i++) {
								SVG_Point *p;
								p = gf_list_get(converter->all_atts.values->values, i);
								gf_sg_vrml_mf_append(&pi2d->keyValue, GF_SG_VRML_MFVEC2F, &g);
								g->x = p->x;
								g->y = p->y;
							}
						}
					}
					break;
				default:
					fprintf(stdout, "Warning: transformation type not supported \n");
				}
			}
			//converter->bifs_parent = node;
		}
		break;
		default:
		{
			fprintf(stdout, "Warning: element %s not supported \n", gf_node_get_class_name((GF_Node *)elt));
			child = gf_node_new(converter->bifs_sg, TAG_MPEG4_Transform2D);
			gf_node_register(child, node);
			//gf_node_list_add_child(&((GF_ParentNode*)node)->children, child);
			node = child;
			child = NULL;
			converter->bifs_parent = node;
		}
		break;
		}

		if (id_string)
			gf_node_set_id(converter->bifs_parent, gf_sg_get_next_available_node_id(converter->bifs_sg), NULL);//gf_node_get_name((GF_Node *)elt));

	}
	fprintf(stdout, "\t%s\n", converter->bifs_parent ? gf_node_get_class_name(converter->bifs_parent) : "none");
}

static void svg2bifs_node_end(void *sax_cbck, const char *name, const char *name_space)
{
	SVG2BIFS_Converter *converter = (SVG2BIFS_Converter *)sax_cbck;
	GF_Node *parent;

	SVGPropertiesPointers *backup_props = gf_node_get_private(converter->svg_parent);
	memcpy(&converter->svg_props, backup_props, sizeof(SVGPropertiesPointers));
//	gf_free(backup_props);
	gf_node_set_private(converter->svg_parent, NULL);

	if (!(gf_node_get_tag(converter->svg_parent) == TAG_SVG_animateTransform))
		converter->bifs_parent = gf_node_get_parent(converter->bifs_parent, 0);
	parent = gf_node_get_parent(converter->svg_parent, 0);
	gf_node_unregister(converter->svg_parent, parent);
	if (!parent) gf_sg_set_root_node(converter->svg_sg, NULL);
	converter->svg_parent = parent;
	converter->bifs_text_node = NULL;

	fprintf(stdout, "END:\t%s\t%s\n", converter->svg_parent ? gf_node_get_class_name(converter->svg_parent) : "none", converter->bifs_parent ? gf_node_get_class_name(converter->bifs_parent) : "none");
}

static void svg2bifs_text_content(void *sax_cbck, const char *text_content, Bool is_cdata)
{
	SVG2BIFS_Converter *converter = (SVG2BIFS_Converter *)sax_cbck;
	if (converter->bifs_text_node) {
		M_Text *text = (M_Text *)converter->bifs_text_node;
		gf_sg_vrml_mf_alloc(&text->string, GF_SG_VRML_MFSTRING, 1);
		text->string.vals[0] = gf_strdup(text_content);
	}
}

int main(int argc, char **argv)
{
	SVG2BIFS_Converter *converter;
	GF_SceneDumper *dump;
	char *tmp;

	gf_sys_init(GF_MemTrackerNone);

	GF_SAFEALLOC(converter, SVG2BIFS_Converter);

	converter->sax_parser = gf_xml_sax_new(svg2bifs_node_start, svg2bifs_node_end, svg2bifs_text_content, converter);
	converter->force_transform = 1;

	converter->svg_sg = gf_sg_new();
	gf_svg_properties_init_pointers(&converter->svg_props);

	converter->bifs_sg = gf_sg_new();

	fprintf(stdout, "Parsing SVG File\n");
	gf_xml_sax_parse_file(converter->sax_parser, argv[1], NULL);

	fprintf(stdout, "Dumping BIFS scenegraph\n");
	tmp = strchr(argv[1], '.');
	tmp[0] = 0;
	dump = gf_sm_dumper_new(converter->bifs_sg, argv[1], ' ', GF_SM_DUMP_BT);
	tmp[0] = '.';

	gf_sm_dump_graph(dump, 1, 0);
	gf_sm_dumper_del(dump);

	gf_svg_properties_reset_pointers(&converter->svg_props);

	gf_sg_del(converter->svg_sg);
//	gf_sg_del(converter->bifs_sg);

	gf_xml_sax_del(converter->sax_parser);

	gf_free(converter);
}
