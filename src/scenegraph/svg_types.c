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

#include <gpac/nodes_svg_sa.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/scenegraph_dev.h>

void gf_svg_reset_path(SVG_PathData d) 
{
#if USE_GF_PATH
	gf_path_reset(&d);
#else
	u32 i, count;
	count = gf_list_count(d.commands);
	for (i = 0; i < count; i++) {
		u8 *command = (u8 *)gf_list_get(d.commands, i);
		free(command);
	}
	gf_list_del(d.commands);
	count = gf_list_count(d.points);
	for (i = 0; i < count; i++) {
		SVG_Point *pt = (SVG_Point *)gf_list_get(d.points, i);
		free(pt);
	}
	gf_list_del(d.points);
#endif
}

void gf_smil_delete_times(GF_List *list)
{
	u32 i, count;
	count = gf_list_count(list);
	for (i = 0; i < count; i++) {
		SMIL_Time *v = (SMIL_Time *)gf_list_get(list, i);
		if (v->element_id) free(v->element_id);
		free(v);
	}
	gf_list_del(list);
}

void gf_svg_delete_points(GF_List *list)
{
	u32 i, count = gf_list_count(list);
	for (i = 0; i < count; i++) {
		SVG_Point *p = (SVG_Point *)gf_list_get(list, i);
		free(p);
	}
	gf_list_del(list);
}

void gf_svg_delete_coordinates(GF_List *list)
{
	u32 i, count = gf_list_count(list);
	for (i = 0; i < count; i++) {
		SVG_Coordinate *c = (SVG_Coordinate *)gf_list_get(list, i);
		free(c);
	}
	gf_list_del(list);
}

void gf_svg_reset_iri(GF_SceneGraph *sg, SVG_IRI *iri) 
{
	if (!iri) return;
	if (iri->iri) free(iri->iri);
	gf_svg_unregister_iri(sg, iri);
}

void gf_svg_delete_paint(GF_SceneGraph *sg, SVG_Paint *paint) 
{
	if (!paint) return;
	if (paint->type == SVG_PAINT_URI && sg) gf_svg_reset_iri(sg, &paint->iri);
	free(paint);
}

static void svg_delete_one_anim_value(u8 anim_datatype, void *anim_value, GF_SceneGraph *sg)
{
	/* TODO: handle specific animation types : Motion, else ? */
	gf_svg_delete_attribute_value(anim_datatype, anim_value, sg);
}

void svg_reset_animate_values(SMIL_AnimateValues anim_values, GF_SceneGraph *sg)
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

void svg_reset_animate_value(SMIL_AnimateValue anim_value, GF_SceneGraph *sg)
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
	case SVG_IRI_datatype:
		gf_svg_reset_iri(sg, (SVG_IRI *)value);
		free(value);
		break;
	case SVG_PathData_datatype:
#if USE_GF_PATH
		gf_path_del((GF_Path *)value);
#else
		free(value);
#endif
		break;
	case SVG_String_datatype:
		if (*(SVG_String *)value) free(*(SVG_String *)value);
		free(value);
		break;
	case SVG_StrokeDashArray_datatype:
		if (((SVG_StrokeDashArray*)value)->array.vals) free(((SVG_StrokeDashArray*)value)->array.vals);
		free(value);
		break;
	case SVG_Coordinates_datatype:
	case SVG_Points_datatype:
		l = *(GF_List**)value;
		while (gf_list_count(l)) {
			void *n = gf_list_last(l);
			gf_list_rem_last(l);
			free(n);
		}
		gf_list_del(l);
		free(value);
		break;
	case SVG_FontFamily_datatype:
		{
			SVG_FontFamily *ff = (SVG_FontFamily *)value;
			if (ff->value) free(ff->value);
			free(value);
		}
		break;
	case SMIL_AttributeName_datatype:
		{
			SMIL_AttributeName *an = (SMIL_AttributeName *)value;
			if (an->name) free(an->name);
			free(value);
		}
		break;
	case SMIL_Times_datatype:
		gf_smil_delete_times(*(SMIL_Times *)value);
		free(value);
		break;
	case SMIL_AnimateValue_datatype:
		svg_delete_one_anim_value(((SMIL_AnimateValue *)value)->type, ((SMIL_AnimateValue *)value)->value, sg);
		free(value);
		break;
	case SMIL_AnimateValues_datatype:
		svg_reset_animate_values(*((SMIL_AnimateValues *)value), sg);
		free(value);
		break;
	case SMIL_RepeatCount_datatype:
	case SMIL_Duration_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	case SVG_Visibility_datatype:
	case SVG_Display_datatype:
	default:
		free(value);
	} 
}


void gf_smil_delete_key_types(GF_List *l)
{
	while (gf_list_count(l)) {
		Fixed *t = (Fixed *)gf_list_get(l, 0);
		gf_list_rem(l, 0);
		free(t);
	}
	gf_list_del(l);
}


#endif /*GPAC_DISABLE_SVG*/
