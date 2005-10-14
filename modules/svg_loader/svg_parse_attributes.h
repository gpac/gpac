/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
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


#ifndef _SVG_PARSE_ATTRIBUTES_H
#define _SVG_PARSE_ATTRIBUTES_H

#include "svg_parser.h"

#ifdef __cplusplus
extern "C" {
#endif 

u32   svg_parse_length					(SVGParser *parser, SVG_Length *length,			char *attribute_content);
void  svg_parse_color					(SVGParser *parser, SVG_Color *col,				char *attribute_content);
void  svg_parse_transform				(SVGParser *parser, SVG_Matrix *mat,			char *attribute_content);
void  svg_parse_path					(SVGParser *parser, SVG_PathData *d_attribute,	char *attribute_content);
void  svg_parse_paint					(SVGParser *parser, SVG_Paint *paint,			char *attribute_content);
void  svg_parse_visibility				(SVGParser *parser, SVG_Visibility *value,		char *attribute_content);
void  svg_parse_display					(SVGParser *parser, SVG_Display *value,			char *attribute_content);
void  svg_parse_textanchor				(SVGParser *parser, SVG_TextAnchor *value,		char *attribute_content);
void  svg_parse_clipfillrule			(SVGParser *parser, SVG_FillRule *value,		char *attribute_content);
void  svg_parse_strokelinejoin			(SVGParser *parser, SVG_StrokeLineJoin *value,	char *attribute_content);
void  svg_parse_strokelinecap			(SVGParser *parser, SVG_StrokeLineCap *value,	char *attribute_content);
void  svg_parse_fontfamily				(SVGParser *parser, SVG_FontFamily *value,		char *attribute_content);
void  svg_parse_fontstyle				(SVGParser *parser, SVG_FontStyle *value,		char *attribute_content);
void  svg_parse_inheritablefloat		(SVGParser *parser, SVGInheritableFloat *value, char *attribute_content, u8 clamp0to1);
void  svg_parse_boolean					(SVGParser *parser, SVG_Boolean *value,			char *attribute_content);
void  svg_parse_viewbox					(SVGParser *parser, SVG_ViewBox *value,			char *attribute_content);
void  svg_parse_strokedasharray			(SVGParser *parser, SVG_StrokeDashArray *value, char *attribute_content);
void  svg_parse_preserveaspectratio		(SVGParser *parser, SVG_PreserveAspectRatio *p, char *attribute_content);
void  svg_parse_animatetransform_type	(SVGParser *parser, SVG_TransformType *ttype,	char *attribute_content);
void  svg_parse_coordinates				(SVGParser *parser, GF_List *values,			char *attribute_content);
void  svg_parse_points					(SVGParser *parser, GF_List *values,			char *attribute_content);
void  svg_parse_floats					(SVGParser *parser, GF_List *values,			char *attribute_content, Bool is_angle);
void  svg_parse_transformlist			(SVGParser *parser, GF_List *values,			char *attribute_content);
void  svg_parse_iri						(SVGParser *parser, SVGElement *e, SVG_IRI *i,	char *attribute_content);

void *svg_parse_one_anim_value			(SVGParser *parser, SVGElement *elt, char *single_value_string, u8 anim_value_type, u8 transform_anim_datatype);
void  svg_parse_anim_values				(SVGParser *parser, SVGElement *elt, SMIL_AnimateValues *anim_values, char *anim_values_string, u8 anim_value_type, u8 transform_anim_datatype);

void  svg_parse_one_style				(SVGParser *parser, SVGElement *elt, char *one_style);
void  svg_parse_style					(SVGParser *parser, SVGElement *elt, char *style);

void  smil_parse_min_max_dur_repeatdur	(SVGParser *parser, SMIL_Duration *value,			char *value_string);
void  smil_parse_repeatcount			(SVGParser *parser, SMIL_RepeatCount *value,		char *value_string);
void  smil_parse_fill					(SVGParser *parser, SMIL_Fill *value,				char *value_string);
void  smil_parse_restart				(SVGParser *parser, SMIL_Restart *value,			char *value_string);
void  smil_parse_calcmode				(SVGParser *parser, SMIL_CalcMode *value,			char *value_string);
void  smil_parse_additive				(SVGParser *parser, SMIL_Additive *value,			char *value_string);
void  smil_parse_accumulate				(SVGParser *parser, SMIL_Accumulate *value,			char *value_string);
void  smil_parse_time					(SVGParser *parser, SVGElement *e, SMIL_Time *t,	char *value_string);
void  smil_parse_attributename			(SVGParser *parser, SVGElement *e,					char *value_string);
void  smil_parse_time_list				(SVGParser *parser, SVGElement *e, GF_List *values, char *value_string);
		
#ifdef __cplusplus
}
#endif

#endif // _SVG_PARSE_ATTRIBUTES_H
