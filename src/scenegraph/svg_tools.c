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
#include <gpac/internal/scenegraph_dev.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/nodes_svg.h>
#include <gpac/internal/renderer_dev.h>

Bool is_svg_animation_tag(u32 tag)
{
	return (tag == TAG_SVG_set ||
			tag == TAG_SVG_animate ||
			tag == TAG_SVG_animateColor ||
			tag == TAG_SVG_animateTransform ||
			tag == TAG_SVG_animateMotion || 
			tag == TAG_SVG_discard)?1:0;
}

static void lsr_conditional_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_simple_time, u32 status)
{	
	if (status==SMIL_TIMING_EVAL_UPDATE) {
		SVGconditionalElement *cond = (SVGconditionalElement *)rti->timed_elt;
		if (cond->updates.data) {
			cond->updates.exec_command_list(cond);
		} else if (gf_list_count(cond->updates.com_list)) {
			gf_sg_command_apply_list(cond->sgprivate->scenegraph, cond->updates.com_list, gf_node_get_scene_time((GF_Node*)cond) );
		}
	}
}

Bool gf_sg_svg_node_init(GF_Node *node)
{
	switch (node->sgprivate->tag) {
	case TAG_SVG_script:
		if (node->sgprivate->scenegraph->script_load) 
			node->sgprivate->scenegraph->script_load(node);
		return 1;
	case TAG_SVG_conditional:
		gf_smil_timing_init_runtime_info(node);
		((SVGElement *)node)->timing->runtime->evaluate = lsr_conditional_evaluate;
		gf_smil_setup_events(node);
		return 1;
	case TAG_SVG_handler:
		if (node->sgprivate->scenegraph->script_load) 
			node->sgprivate->scenegraph->script_load(node);
		if (node->sgprivate->scenegraph->js_ifce)
			((SVGhandlerElement *)node)->handle_event = gf_sg_handle_dom_event;
		return 1;
	case TAG_SVG_animateMotion:
	case TAG_SVG_set: 
	case TAG_SVG_animate: 
	case TAG_SVG_animateColor: 
	case TAG_SVG_animateTransform: 
		gf_smil_anim_init_node(node);
	case TAG_SVG_audio: 
	case TAG_SVG_video: 
		gf_smil_setup_events(node);
		/*we may get called several times depending on xlink:href resoling for events*/
		return (node->sgprivate->UserPrivate || node->sgprivate->UserCallback) ? 1 : 0;
	/*discard is implemented as a special animation element */
	case TAG_SVG_discard: 
		gf_smil_anim_init_discard(node);
		gf_smil_setup_events(node);
		return 1;
	default:
		return 0;
	}
	return 0;
}

Bool gf_sg_svg_node_changed(GF_Node *node, GF_FieldInfo *field)
{
	switch (node->sgprivate->tag) {
	case TAG_SVG_animateMotion:
	case TAG_SVG_set: 
	case TAG_SVG_animate: 
	case TAG_SVG_animateColor: 
	case TAG_SVG_animateTransform: 
	case TAG_SVG_conditional: 
		gf_smil_timing_modified(node, field);
		return 1;
	case TAG_SVG_audio: 
	case TAG_SVG_video: 
		gf_smil_timing_modified(node, field);
		/*used by renderers*/
		return 0;
	}
	return 0;
}

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


/*TODO FIXME, this is ugly, add proper cache system*/
#include <gpac/base_coding.h>

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



void gf_svg_init_lsr_conditional(SVGCommandBuffer *script)
{
	memset(script, 0, sizeof(SVGCommandBuffer));
	script->com_list = gf_list_new();
}

void gf_svg_reset_lsr_conditional(SVGCommandBuffer *script)
{
	u32 i;
	if (script->data) free(script->data);
	for (i=gf_list_count(script->com_list); i>0; i--) {
		GF_Command *com = (GF_Command *)gf_list_get(script->com_list, i-1);
		gf_sg_command_del(com);
	}
	gf_list_del(script->com_list);
}


#else
/*these ones are only needed for W32 libgpac_dll build in order not to modify export def file*/
u32 gf_node_svg_type_by_class_name(const char *element_name)
{
	return 0;
}
u32 gf_svg_get_attribute_count(GF_Node *n)
{
	return 0;
}
GF_Err gf_svg_get_attribute_info(GF_Node *node, GF_FieldInfo *info)
{
	return GF_NOT_SUPPORTED;
}

GF_Node *gf_svg_create_node(GF_SceneGraph *inScene, u32 tag)
{
	return NULL;
}
const char *gf_svg_get_element_name(u32 tag)
{
	return "Unsupported";
}
Bool gf_sg_svg_node_init(GF_Node *node)
{
	return 0;
}

Bool gf_svg_conditional_eval(SVGElement *node)
{
}

#endif	//GPAC_DISABLE_SVG
