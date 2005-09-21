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

#include <gpac/nodes_svg.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/scenegraph_dev.h>

void SVG_DeletePath(SVG_PathData *d) 
{
	u32 i;
	for (i = 0; i < gf_list_count(d->commands); i++) {
		u8 *command = gf_list_get(d->commands, i);
		free(command);
	}
	gf_list_del(d->commands);
	for (i = 0; i < gf_list_count(d->points); i++) {
		SVG_Point *pt = gf_list_get(d->points, i);
		free(pt);
	}
	gf_list_del(d->points);
}

void SVG_DeleteTransformList(GF_List *tr)
{
	u32 i;
	for (i = 0; i < gf_list_count(tr); i++) {
		SVG_Matrix *m=gf_list_get(tr, i);
		free(m);
	}
	gf_list_del(tr);
}

void SMIL_DeleteTimes(GF_List *list)
{
	u32 i;
	for (i = 0; i < gf_list_count(list); i++) {
		SMIL_Time *v=gf_list_get(list, i);
		free(v);
	}
	gf_list_del(list);
}

void SVG_DeletePoints(GF_List *list)
{
	u32 i;
	for (i = 0; i < gf_list_count(list); i++) {
		SVG_Point *p=gf_list_get(list, i);
		free(p);
	}
	gf_list_del(list);
}

void SVG_DeleteCoordinates(GF_List *list)
{
	u32 i;
	for (i = 0; i < gf_list_count(list); i++) {
		SVG_Coordinate *c=gf_list_get(list, i);
		free(c);
	}
	gf_list_del(list);
}

void SVG_DeletePaint(SVG_Paint *paint) 
{
	if (!paint) return;
	free(paint->color);
	free(paint);
}

void SVG_DeleteOneAnimValue(u8 anim_datatype, void *anim_value)
{
	switch (anim_datatype) {
	case SVG_Paint_datatype:
		SVG_DeletePaint((SVG_Paint *)anim_value);
		break;
	case SVG_StrokeWidth_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	case SVG_Visibility_datatype:
	case SVG_Display_datatype:
	default:
		free(anim_value);
	} 
}

void SMIL_DeleteAnimateValues(SMIL_AnimateValues *anim_values)
{
	u32 i, count;
	count = gf_list_count(anim_values->values);
	for (i = 0; i < count; i++) {
		void *value = gf_list_get(anim_values->values, i);
		SVG_DeleteOneAnimValue(anim_values->datatype, value);
	}
	gf_list_del(anim_values->values);
	anim_values->values = NULL;
}

void SMIL_DeleteAnimateValue(SMIL_AnimateValue *anim_value)
{
	SVG_DeleteOneAnimValue(anim_value->datatype, anim_value->value);
	anim_value->value = NULL;
}

#endif /*GPAC_DISABLE_SVG*/
