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

/* TODO: update for elliptical arcs */		
GF_EXPORT
void gf_svg_path_build(GF_Path *path, GF_List *commands, GF_List *points)
{
	u32 i, j, command_count, points_count;
	SVG_Point orig, ct_orig, ct_end, end, *tmp;
	command_count = gf_list_count(commands);
	points_count = gf_list_count(points);
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
		gf_svg_reset_animate_values(*((SMIL_AnimateValues *)value), sg);
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


GF_EXPORT
void gf_svg_register_iri(GF_SceneGraph *sg, SVG_IRI *target)
{
	if (gf_list_find(sg->xlink_hrefs, target)<0) {
		gf_list_add(sg->xlink_hrefs, target);
	}
}
void gf_svg_unregister_iri(GF_SceneGraph *sg, SVG_IRI *target)
{
	gf_list_del_item(sg->xlink_hrefs, target);
}

/*TODO FIXME, this is ugly, add proper cache system*/
#include <gpac/base_coding.h>


static u32 check_existing_file(char *base_file, char *ext, char *data, u32 data_size, u32 idx)
{
	char szFile[GF_MAX_PATH];
	u32 fsize;
	FILE *f;
	
	sprintf(szFile, "%s%04X%s", base_file, idx, ext);
	
	f = fopen(szFile, "rb");
	if (!f) return 0;

	fseek(f, 0, SEEK_END);
	fsize = ftell(f);
	if (fsize==data_size) {
		u32 offset=0;
		char cache[1024];
		fseek(f, 0, SEEK_SET);
		while (fsize) {
			u32 read = fread(cache, 1, 1024, f);
			fsize -= read;
			if (memcmp(cache, data+offset, sizeof(char)*read)) break;
			offset+=read;
		}
		fclose(f);
		/*same file*/
		if (!fsize) return 2;
	}
	fclose(f);
	return 1;
}


GF_EXPORT
Bool gf_svg_store_embedded_data(SVG_IRI *iri, const char *cache_dir, const char *base_filename)
{
	char szFile[GF_MAX_PATH], buf[20], *sep, *data, *ext;
	u32 data_size, idx;
	Bool existing;
	FILE *f;

	if (!cache_dir || !base_filename || !iri || !iri->iri || strncmp(iri->iri, "data:", 5)) return 0;

	/*handle "data:" scheme when cache is specified*/
	strcpy(szFile, cache_dir);
	data_size = strlen(szFile);
	if (szFile[data_size-1] != GF_PATH_SEPARATOR) {
		szFile[data_size] = GF_PATH_SEPARATOR;
		szFile[data_size+1] = 0;
	}
	if (base_filename) {
		sep = strrchr(base_filename, GF_PATH_SEPARATOR);
#ifdef WIN32
		if (!sep) sep = strrchr(base_filename, '/');
#endif
		if (!sep) sep = (char *) base_filename;
		else sep += 1;
		strcat(szFile, sep);
	}
	sep = strrchr(szFile, '.');
	if (sep) sep[0] = 0;
	strcat(szFile, "_img_");

	/*get mime type*/
	sep = (char *)iri->iri + 5;
	if (!strncmp(sep, "image/jpg", 9) || !strncmp(sep, "image/jpeg", 10)) ext = ".jpg";
	else if (!strncmp(sep, "image/png", 9)) ext = ".png";
	else return 0;


	data = NULL;
	sep = strchr(iri->iri, ';');
	if (!strncmp(sep, ";base64,", 8)) {
		sep += 8;
		data_size = 2*strlen(sep);
		data = (char*)malloc(sizeof(char)*data_size);
		if (!data) return 0;
		data_size = gf_base64_decode(sep, strlen(sep), data, data_size);
	}
	else if (!strncmp(sep, ";base16,", 8)) {
		data_size = 2*strlen(sep);
		data = (char*)malloc(sizeof(char)*data_size);
		if (!data) return 0;
		sep += 8;
		data_size = gf_base16_decode(sep, strlen(sep), data, data_size);
	}
	if (!data_size) return 0;
	
	iri->type = SVG_IRI_IRI;
	
	existing = 0;
	idx = 0;
	while (1) {
		u32 res = check_existing_file(szFile, ext, data, data_size, idx);
		if (!res) break;
		if (res==2) {
			existing = 1;
			break;
		}
		idx++;
	}
	sprintf(buf, "%04X", idx);
	strcat(szFile, buf);
	strcat(szFile, ext);

	if (!existing) {
		f = fopen(szFile, "wb");
		if (!f) return 0;
		fwrite(data, data_size, 1, f);
		fclose(f);
	}
	free(data);
	free(iri->iri);
	iri->iri = strdup(szFile);
	return 1;
}

#endif /*GPAC_DISABLE_SVG*/
