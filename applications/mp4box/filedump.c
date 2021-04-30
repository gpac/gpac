/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2021
 *					All rights reserved
 *
 *  This file is part of GPAC / mp4box application
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

#include "mp4box.h"

#if defined(GPAC_DISABLE_ISOM) || defined(GPAC_DISABLE_ISOM_WRITE)

#error "Cannot compile MP4Box if GPAC is not built with ISO File Format support"

#else

#ifndef GPAC_DISABLE_X3D
#include <gpac/nodes_x3d.h>
#endif
#ifndef GPAC_DISABLE_BIFS
#include <gpac/internal/bifs_dev.h>
#endif
#ifndef GPAC_DISABLE_VRML
#include <gpac/nodes_mpeg4.h>
#endif
#include <gpac/constants.h>
#include <gpac/avparse.h>
#include <gpac/internal/media_dev.h>
/*ISO 639 languages*/
#include <gpac/iso639.h>
#include <gpac/mpegts.h>

#ifndef GPAC_DISABLE_SMGR
#include <gpac/scene_manager.h>
#endif
#include <gpac/internal/media_dev.h>
#include <gpac/media_tools.h>
/*for built-in box printing*/
#include <gpac/internal/isomedia_dev.h>

extern u32 swf_flags;
extern Float swf_flatten_angle;
extern GF_FileType get_file_type_by_ext(char *inName);
extern u32 fs_dump_flags;

void scene_coding_log(void *cbk, GF_LOG_Level log_level, GF_LOG_Tool log_tool, const char *fmt, va_list vlist);


#ifdef GPAC_DISABLE_LOG
void mp4box_log(const char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	vfprintf(stderr, fmt, vlist);
	fflush(stderr);
	va_end(vl);
}
#endif


u32 PrintLanguages(char *val, u32 opt)
{
	u32 i=0, count = gf_lang_get_count();
	fprintf(stderr, "Supported ISO 639 languages and codes:\n\n");
	for (i=0; i<count; i++) {
		if (gf_lang_get_2cc(i)) {
			fprintf(stderr, "%s (%s - %s)\n", gf_lang_get_name(i), gf_lang_get_3cc(i), gf_lang_get_2cc(i));
		}
	}
	return 1;
}

static const char *GetLanguage(char *lcode)
{
	s32 idx = gf_lang_find(lcode);
	if (idx>=0) return gf_lang_get_name(idx);
	return lcode;
}

GF_Err dump_isom_cover_art(GF_ISOFile *file, char *inName, Bool is_final_name)
{
	const u8 *tag;
	FILE *t;
	u32 tag_len;
	GF_Err e = gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_COVER_ART, &tag, &tag_len);
	if (e!=GF_OK) {
		if (e==GF_URL_ERROR) {
			M4_LOG(GF_LOG_WARNING, ("No cover art found\n"));
			return GF_OK;
		}
		return e;
	}

	if (inName) {
		char szName[1024];
		if (is_final_name) {
			strcpy(szName, inName);
		} else {
			sprintf(szName, "%s.%s", inName, (tag_len>>31) ? "png" : "jpg");
		}
		t = gf_fopen(szName, "wb");
		if (!t) {
			M4_LOG(GF_LOG_ERROR, ("Failed to open %s for dumping\n", szName));
			return GF_IO_ERR;
		}
	} else {
		t = stdout;
	}
	gf_fwrite(tag, tag_len & 0x7FFFFFFF, t);

	if (inName) gf_fclose(t);
	return GF_OK;
}

#ifndef GPAC_DISABLE_SCENE_DUMP

GF_Err dump_isom_scene(char *file, char *inName, Bool is_final_name, GF_SceneDumpFormat dump_mode, Bool do_log, Bool no_odf_conv)
{
	GF_Err e;
	GF_SceneManager *ctx;
	GF_SceneGraph *sg;
	GF_SceneLoader load;
	GF_FileType ftype;
	gf_log_cbk prev_logs = NULL;
	FILE *logs = NULL;

	sg = gf_sg_new();
	ctx = gf_sm_new(sg);
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = file;
	load.ctx = ctx;
	load.swf_import_flags = swf_flags;
	if (dump_mode == GF_SM_DUMP_SVG) {
		load.swf_import_flags |= GF_SM_SWF_USE_SVG;
		load.svgOutFile = inName;
	}
	load.swf_flatten_limit = swf_flatten_angle;

	ftype = get_file_type_by_ext(file);
	if (ftype == GF_FILE_TYPE_ISO_MEDIA) {
		load.isom = gf_isom_open(file, GF_ISOM_OPEN_READ, NULL);
		if (!load.isom) {
			e = gf_isom_last_error(NULL);
			M4_LOG(GF_LOG_ERROR, ("Error opening file: %s\n", gf_error_to_string(e)));
			gf_sm_del(ctx);
			gf_sg_del(sg);
			return e;
		}
		if (no_odf_conv)
			gf_isom_disable_odf_conversion(load.isom, GF_TRUE);

	} else if (ftype==GF_FILE_TYPE_LSR_SAF) {
		load.isom = gf_isom_open("saf_conv", GF_ISOM_WRITE_EDIT, NULL);
#ifndef GPAC_DISABLE_MEDIA_IMPORT
		if (load.isom) {
			GF_Fraction _frac = {0,0};
			e = import_file(load.isom, file, 0, _frac, 0, NULL, NULL, 0);
		} else
#else
		M4_LOG(GF_LOG_WARNING, ("Warning: GPAC was compiled without Media Import support\n"));
#endif
			e = gf_isom_last_error(NULL);

		if (e) {
			M4_LOG(GF_LOG_ERROR, ("Error importing file: %s\n", gf_error_to_string(e)));
			gf_sm_del(ctx);
			gf_sg_del(sg);
			if (load.isom) gf_isom_delete(load.isom);
			return e;
		}
	}

	if (do_log) {
		char szLog[GF_MAX_PATH];
		sprintf(szLog, "%s_dec.logs", inName);
		logs = gf_fopen(szLog, "wt");

		gf_log_set_tool_level(GF_LOG_CODING, GF_LOG_DEBUG);
		prev_logs = gf_log_set_callback(logs, scene_coding_log);
	}
	e = gf_sm_load_init(&load);
	if (!e) e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);
	if (logs) {
		gf_log_set_tool_level(GF_LOG_CODING, GF_LOG_ERROR);
		gf_log_set_callback(NULL, prev_logs);
		gf_fclose(logs);
	}
	if (!e && dump_mode != GF_SM_DUMP_SVG) {
		u32 count = gf_list_count(ctx->streams);
		if (count)
			fprintf(stderr, "Scene loaded - dumping %d systems streams\n", count);
		else
			fprintf(stderr, "Scene loaded - dumping root scene\n");

		e = gf_sm_dump(ctx, inName, is_final_name, dump_mode);
	}

	gf_sm_del(ctx);
	gf_sg_del(sg);
	if (e) M4_LOG(GF_LOG_ERROR, ("Error loading scene: %s\n", gf_error_to_string(e)));
	if (load.isom) gf_isom_delete(load.isom);
	return e;
}
#endif

#ifndef GPAC_DISABLE_SCENE_STATS

static void dump_stats(FILE *dump, const GF_SceneStatistics *stats)
{
	u32 i;
	s32 created, count, draw_created, draw_count, deleted, draw_deleted;
	created = count = draw_created = draw_count = deleted = draw_deleted = 0;

	fprintf(dump, "<NodeStatistics>\n");
	fprintf(dump, "<General NumberOfNodeTypes=\"%d\"/>\n", gf_list_count(stats->node_stats));
	for (i=0; i<gf_list_count(stats->node_stats); i++) {
		GF_NodeStats *ptr = gf_list_get(stats->node_stats, i);
		fprintf(dump, "<NodeStat NodeName=\"%s\">\n", ptr->name);

		switch (ptr->tag) {
#ifndef GPAC_DISABLE_VRML
		case TAG_MPEG4_Bitmap:
		case TAG_MPEG4_Background2D:
		case TAG_MPEG4_Background:
		case TAG_MPEG4_Box:
		case TAG_MPEG4_Circle:
		case TAG_MPEG4_CompositeTexture2D:
		case TAG_MPEG4_CompositeTexture3D:
		case TAG_MPEG4_Cylinder:
		case TAG_MPEG4_Cone:
		case TAG_MPEG4_Curve2D:
		case TAG_MPEG4_Extrusion:
		case TAG_MPEG4_ElevationGrid:
		case TAG_MPEG4_IndexedFaceSet2D:
		case TAG_MPEG4_IndexedFaceSet:
		case TAG_MPEG4_IndexedLineSet2D:
		case TAG_MPEG4_IndexedLineSet:
		case TAG_MPEG4_PointSet2D:
		case TAG_MPEG4_PointSet:
		case TAG_MPEG4_Rectangle:
		case TAG_MPEG4_Sphere:
		case TAG_MPEG4_Text:
		case TAG_MPEG4_Ellipse:
		case TAG_MPEG4_XCurve2D:
			draw_count += ptr->nb_created + ptr->nb_used - ptr->nb_del;
			draw_deleted += ptr->nb_del;
			draw_created += ptr->nb_created;
			break;
#endif /*GPAC_DISABLE_VRML*/
		}
		fprintf(dump, "<Instanciation NbObjects=\"%d\" NbUse=\"%d\" NbDestroy=\"%d\"/>\n", ptr->nb_created, ptr->nb_used, ptr->nb_del);
		count += ptr->nb_created + ptr->nb_used;
		deleted += ptr->nb_del;
		created += ptr->nb_created;
		fprintf(dump, "</NodeStat>\n");
	}
	if (i) {
		fprintf(dump, "<CumulatedStat TotalNumberOfNodes=\"%d\" ReallyAllocatedNodes=\"%d\" DeletedNodes=\"%d\" NumberOfAttributes=\"%d\"/>\n", count, created, deleted, stats->nb_svg_attributes);
		fprintf(dump, "<DrawableNodesCumulatedStat TotalNumberOfNodes=\"%d\" ReallyAllocatedNodes=\"%d\" DeletedNodes=\"%d\"/>\n", draw_count, draw_created, draw_deleted);
	}
	fprintf(dump, "</NodeStatistics>\n");

	created = count = deleted = 0;
	if (gf_list_count(stats->proto_stats)) {
		fprintf(dump, "<ProtoStatistics NumberOfProtoUsed=\"%d\">\n", gf_list_count(stats->proto_stats));
		for (i=0; i<gf_list_count(stats->proto_stats); i++) {
			GF_NodeStats *ptr = gf_list_get(stats->proto_stats, i);
			fprintf(dump, "<ProtoStat ProtoName=\"%s\">\n", ptr->name);
			fprintf(dump, "<Instanciation NbObjects=\"%d\" NbUse=\"%d\" NbDestroy=\"%d\"/>\n", ptr->nb_created, ptr->nb_used, ptr->nb_del);
			count += ptr->nb_created + ptr->nb_used;
			deleted += ptr->nb_del;
			created += ptr->nb_created;
			fprintf(dump, "</ProtoStat>\n");
		}
		if (i) fprintf(dump, "<CumulatedStat TotalNumberOfProtos=\"%d\" ReallyAllocatedProtos=\"%d\" DeletedProtos=\"%d\"/>\n", count, created, deleted);
		fprintf(dump, "</ProtoStatistics>\n");
	}
	fprintf(dump, "<FixedValues min=\"%f\" max=\"%f\">\n", FIX2FLT( stats->min_fixed) , FIX2FLT( stats->max_fixed ));
	fprintf(dump, "<Resolutions scaleIntegerPart=\"%d\" scaleFracPart=\"%d\" coordIntegerPart=\"%d\" coordFracPart=\"%d\"/>\n", stats->scale_int_res_2d, stats->scale_frac_res_2d, stats->int_res_2d, stats->frac_res_2d);
	fprintf(dump, "</FixedValues>\n");
	fprintf(dump, "<FieldStatistic FieldType=\"MFVec2f\">\n");
	fprintf(dump, "<ParsingInfo NumParsed=\"%d\" NumRemoved=\"%d\"/>\n", stats->count_2d, stats->rem_2d);
	if (stats->count_2d) {
		fprintf(dump, "<ExtendInfo MinVec2f=\"%f %f\" MaxVec2f=\"%f %f\"/>\n", FIX2FLT( stats->min_2d.x) , FIX2FLT( stats->min_2d.y ), FIX2FLT( stats->max_2d.x ), FIX2FLT( stats->max_2d.y ) );
	}
	fprintf(dump, "</FieldStatistic>\n");

	fprintf(dump, "<FieldStatistic FieldType=\"MFVec3f\">\n");
	fprintf(dump, "<ParsingInfo NumParsed=\"%d\" NumRemoved=\"%d\"/>", stats->count_3d, stats->rem_3d);
	if (stats->count_3d) {
		fprintf(dump, "<ExtendInfo MinVec3f=\"%f %f %f\" MaxVec3f=\"%f %f %f\"/>\n", FIX2FLT( stats->min_3d.x ), FIX2FLT( stats->min_3d.y ), FIX2FLT( stats->min_3d.z ), FIX2FLT( stats->max_3d.x ), FIX2FLT( stats->max_3d.y ), FIX2FLT( stats->max_3d.z ) );
	}
	fprintf(dump, "</FieldStatistic>\n");

	fprintf(dump, "<FieldStatistic FieldType=\"MF/SFColor\">\n");
	fprintf(dump, "<ParsingInfo NumParsed=\"%d\" NumRemoved=\"%d\"/>", stats->count_color, stats->rem_color);
	fprintf(dump, "</FieldStatistic>\n");

	fprintf(dump, "<FieldStatistic FieldType=\"MF/SFFloat\">\n");
	fprintf(dump, "<ParsingInfo NumParsed=\"%d\" NumRemoved=\"%d\"/>", stats->count_float, stats->rem_float);
	fprintf(dump, "</FieldStatistic>\n");

	fprintf(dump, "<FieldStatistic FieldType=\"SFVec2f\">\n");
	fprintf(dump, "<ParsingInfo NumParsed=\"%d\"/>", stats->count_2f);
	fprintf(dump, "</FieldStatistic>\n");
	fprintf(dump, "<FieldStatistic FieldType=\"SFVec3f\">\n");
	fprintf(dump, "<ParsingInfo NumParsed=\"%d\"/>", stats->count_3f);
	fprintf(dump, "</FieldStatistic>\n");
}


static void ReorderAU(GF_List *sample_list, GF_AUContext *au)
{
	u32 i;
	for (i=0; i<gf_list_count(sample_list); i++) {
		GF_AUContext *ptr = gf_list_get(sample_list, i);
		if (
		    /*time ordered*/
		    (ptr->timing_sec > au->timing_sec)
		    /*set bifs first*/
		    || ((ptr->timing_sec == au->timing_sec) && (ptr->owner->streamType < au->owner->streamType))
		) {
			gf_list_insert(sample_list, au, i);
			return;
		}
	}
	gf_list_add(sample_list, au);
}

void dump_isom_scene_stats(char *file, char *inName, Bool is_final_name, u32 stat_level)
{
	GF_Err e;
	FILE *dump;
	Bool close;
	u32 i, j, count;
	char szBuf[1024];
	GF_SceneManager *ctx;
	GF_SceneLoader load;
	GF_StatManager *sm;
	GF_List *sample_list;
	GF_SceneGraph *scene_graph;

	dump = NULL;
	sm = NULL;
	sample_list = NULL;

	close = 0;

	scene_graph = gf_sg_new();
	ctx = gf_sm_new(scene_graph);
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = file;
	load.ctx = ctx;

	if (get_file_type_by_ext(file) == 1) {
		load.isom = gf_isom_open(file, GF_ISOM_OPEN_READ, NULL);
		if (!load.isom) {
			M4_LOG(GF_LOG_ERROR, ("Cannot open file: %s\n", gf_error_to_string(gf_isom_last_error(NULL))));
			gf_sm_del(ctx);
			gf_sg_del(scene_graph);
			return;
		}
	}

	e = gf_sm_load_init(&load);
	if (!e) e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);
	if (e<0) goto exit;

	if (inName) {
		strcpy(szBuf, inName);
		if (!is_final_name) strcat(szBuf, "_stat.xml");
		dump = gf_fopen(szBuf, "wt");
		if (!dump) {
			M4_LOG(GF_LOG_ERROR, ("Failed to open %s for dumping\n", szBuf));
			return;
		}
		close = 1;
	} else {
		dump = stdout;
		close = 0;
	}

	fprintf(stderr, "Analysing Scene\n");

	fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
	fprintf(dump, "<!-- Scene Graph Statistics Generated by MP4Box - GPAC ");
	if (! gf_sys_is_test_mode())
		fprintf(dump, "%s ",  gf_gpac_version());
	fprintf(dump, "-->\n");

	fprintf(dump, "<SceneStatistics file=\"%s\" DumpType=\"%s\">\n", gf_file_basename(file), (stat_level==1) ? "full scene" : ((stat_level==2) ? "AccessUnit based" : "SceneGraph after each AU"));

	sm = gf_sm_stats_new();

	/*stat level 1: complete scene stat*/
	if (stat_level == 1) {
		e = gf_sm_stats_for_scene(sm, ctx);
		if (!e) dump_stats(dump, gf_sm_stats_get(sm) );
		goto exit;
	}
	/*re_order all BIFS-AUs*/
	sample_list = gf_list_new();
	/*configure all systems streams we're dumping*/
	for (i=0; i<gf_list_count(ctx->streams); i++) {
		GF_StreamContext *sc = gf_list_get(ctx->streams, i);
		if (sc->streamType != GF_STREAM_SCENE) continue;
		for (j=0; j<gf_list_count(sc->AUs); j++) {
			GF_AUContext *au = gf_list_get(sc->AUs, j);
			ReorderAU(sample_list, au);
		}
	}

	count = gf_list_count(sample_list);
	for (i=0; i<count; i++) {
		GF_AUContext *au = gf_list_get(sample_list, i);

		for (j=0; j<gf_list_count(au->commands); j++) {
			GF_Command *com = gf_list_get(au->commands, j);
			/*stat level 2 - get command stats*/
			if (stat_level==2) {
				e = gf_sm_stats_for_command(sm, com);
				if (e) goto exit;
			}
			/*stat level 3 - apply command*/
			if (stat_level==3) gf_sg_command_apply(scene_graph, com, 0);
		}
		/*stat level 3: get graph stat*/
		if (stat_level==3) {
			e = gf_sm_stats_for_graph(sm, scene_graph);
			if (e) goto exit;
		}
		if (stat_level==2) {
			fprintf(dump, "<AUStatistics StreamID=\"%d\" AUTime=\""LLD"\">\n", au->owner->ESID, au->timing);
		} else {
			fprintf(dump, "<GraphStatistics StreamID=\"%d\" AUTime=\""LLD"\">\n", au->owner->ESID, au->timing);
		}
		/*dump stats*/
		dump_stats(dump, gf_sm_stats_get(sm) );
		/*reset stats*/
		gf_sm_stats_reset(sm);
		if (stat_level==2) {
			fprintf(dump, "</AUStatistics>\n");
		} else {
			fprintf(dump, "</GraphStatistics>\n");
		}

		gf_set_progress("Analysing AU", i+1, count);
	}


exit:
	if (sample_list) gf_list_del(sample_list);
	if (sm) gf_sm_stats_del(sm);
	gf_sm_del(ctx);
	gf_sg_del(scene_graph);
	if (load.isom) gf_isom_delete(load.isom);
	if (e) {
		M4_LOG(GF_LOG_ERROR, ("Stats error: %s\n", gf_error_to_string(e)));
	} else {
		fprintf(dump, "</SceneStatistics>\n");
	}
	if (dump && close) gf_fclose(dump);
	fprintf(stderr, "done\n");
}
#endif /*GPAC_DISABLE_SCENE_STATS*/



#ifndef GPAC_DISABLE_VRML

static void PrintFixed(Fixed val, Bool add_space)
{
	if (add_space) fprintf(stderr, " ");
	if (val==FIX_MIN) fprintf(stderr, "-I");
	else if (val==FIX_MAX) fprintf(stderr, "+I");
	else fprintf(stderr, "%g", FIX2FLT(val));
}

static void PrintNodeSFField(u32 type, void *far_ptr)
{
	if (!far_ptr) return;
	switch (type) {
	case GF_SG_VRML_SFBOOL:
		fprintf(stderr, "%s", (*(SFBool *)far_ptr) ? "TRUE" : "FALSE");
		break;
	case GF_SG_VRML_SFINT32:
		fprintf(stderr, "%d", (*(SFInt32 *)far_ptr));
		break;
	case GF_SG_VRML_SFFLOAT:
		PrintFixed((*(SFFloat *)far_ptr), 0);
		break;
	case GF_SG_VRML_SFTIME:
		fprintf(stderr, "%g", (*(SFTime *)far_ptr));
		break;
	case GF_SG_VRML_SFVEC2F:
		PrintFixed(((SFVec2f *)far_ptr)->x, 0);
		PrintFixed(((SFVec2f *)far_ptr)->y, 1);
		break;
	case GF_SG_VRML_SFVEC3F:
		PrintFixed(((SFVec3f *)far_ptr)->x, 0);
		PrintFixed(((SFVec3f *)far_ptr)->y, 1);
		PrintFixed(((SFVec3f *)far_ptr)->z, 1);
		break;
	case GF_SG_VRML_SFROTATION:
		PrintFixed(((SFRotation *)far_ptr)->x, 0);
		PrintFixed(((SFRotation *)far_ptr)->y, 1);
		PrintFixed(((SFRotation *)far_ptr)->z, 1);
		PrintFixed(((SFRotation *)far_ptr)->q, 1);
		break;
	case GF_SG_VRML_SFCOLOR:
		PrintFixed(((SFColor *)far_ptr)->red, 0);
		PrintFixed(((SFColor *)far_ptr)->green, 1);
		PrintFixed(((SFColor *)far_ptr)->blue, 1);
		break;
	case GF_SG_VRML_SFSTRING:
		if (((SFString*)far_ptr)->buffer)
			fprintf(stderr, "\"%s\"", ((SFString*)far_ptr)->buffer);
		else
			fprintf(stderr, "NULL");
		break;
	}
}
#endif

#ifndef GPAC_DISABLE_VRML
static void do_print_node(GF_Node *node, GF_SceneGraph *sg, const char *name, u32 graph_type, Bool is_nodefield, Bool do_cov)
{
	u32 nbF, i;
	GF_FieldInfo f;
#ifndef GPAC_DISABLE_BIFS
	u8 qt, at;
	Fixed bmin, bmax;
	u32 nbBits;
#endif /*GPAC_DISABLE_BIFS*/

	nbF = gf_node_get_field_count(node);

	if (is_nodefield) {
		char szField[1024];
		u32 tfirst, tlast;
		if (gf_node_get_field_by_name(node, szField, &f) != GF_OK) {
			M4_LOG(GF_LOG_ERROR, ("Field %s is not a member of node %s\n", szField, name));
			return;
		}
		fprintf(stderr, "Allowed nodes in %s.%s:\n", name, szField);
		if (graph_type==1) {
			tfirst = GF_NODE_RANGE_FIRST_X3D;
			tlast = GF_NODE_RANGE_LAST_X3D;
		} else {
			tfirst = GF_NODE_RANGE_FIRST_MPEG4;
			tlast = GF_NODE_RANGE_LAST_MPEG4;
		}
		for (i=tfirst; i<tlast; i++) {
			GF_Node *tmp = gf_node_new(sg, i);
			gf_node_register(tmp, NULL);
			if (gf_node_in_table_by_tag(i, f.NDTtype)) {
				const char *nname = gf_node_get_class_name(tmp);
				if (nname && strcmp(nname, "Unknown Node")) {
					fprintf(stderr, "\t%s\n", nname);
				}
			}
			gf_node_unregister(tmp, NULL);
		}
		return;
	}
	if (do_cov) {
		u32 ndt;
		if (graph_type==0) {
			u32 all;
			gf_node_mpeg4_type_by_class_name(name);
			gf_bifs_get_child_table(node);
			all = gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_ALL);
			for (i=0; i<all; i++) {
				u32 res;
				gf_sg_script_get_field_index(node, i, GF_SG_FIELD_CODING_ALL, &res);
			}

			gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_DEF);
			gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_IN);
			gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_OUT);
			gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_DYN);
		}
		else if (graph_type==1) gf_node_x3d_type_by_class_name(name);
		for (ndt=NDT_SFWorldNode; ndt<NDT_LAST; ndt++) {
			gf_node_in_table_by_tag(gf_node_get_tag(node), ndt);
		}
	}
	fprintf(stderr, "%s {\n", name);

	for (i=0; i<nbF; i++) {
		gf_node_get_field(node, i, &f);
		if (graph_type==2) {
			fprintf(stderr, "\t%s=\"...\"\n", f.name);
			continue;
		}

		fprintf(stderr, "\t%s %s %s", gf_sg_vrml_get_event_type_name(f.eventType, 0), gf_sg_vrml_get_field_type_name(f.fieldType), f.name);
		if (f.fieldType==GF_SG_VRML_SFNODE) fprintf(stderr, " NULL");
		else if (f.fieldType==GF_SG_VRML_MFNODE) fprintf(stderr, " []");
		else if (gf_sg_vrml_is_sf_field(f.fieldType)) {
			fprintf(stderr, " ");
			PrintNodeSFField(f.fieldType, f.far_ptr);
		} else {
			void *ptr;
			u32 j, sftype;
			GenMFField *mffield = (GenMFField *) f.far_ptr;
			fprintf(stderr, " [");
			sftype = gf_sg_vrml_get_sf_type(f.fieldType);
			for (j=0; j<mffield->count; j++) {
				if (j) fprintf(stderr, " ");
				gf_sg_vrml_mf_get_item(f.far_ptr, f.fieldType, &ptr, j);
				PrintNodeSFField(sftype, ptr);
			}
			fprintf(stderr, "]");
		}
#ifndef GPAC_DISABLE_BIFS
		if (gf_bifs_get_aq_info(node, i, &qt, &at, &bmin, &bmax, &nbBits)) {
			if (qt) {
				fprintf(stderr, " #QP=%d", qt);
				if (qt==13) fprintf(stderr, " NbBits=%d", nbBits);
				if (bmin && bmax) {
					fprintf(stderr, " Bounds=[");
					PrintFixed(bmin, 0);
					fprintf(stderr, ",");
					PrintFixed(bmax, 0);
					fprintf(stderr, "]");
				}
			}
		}
#endif /*GPAC_DISABLE_BIFS*/
		fprintf(stderr, "\n");

		if (do_cov) {
			gf_node_get_field_by_name(node, (char *) f.name, &f);
		}
	}
	fprintf(stderr, "}\n\n");

}
#endif


u32 PrintNode(const char *name, u32 graph_type)
{
#ifdef GPAC_DISABLE_VRML
	M4_LOG(GF_LOG_ERROR, ("VRML/MPEG-4/X3D scene graph is disabled in this build of GPAC\n"));
	return 2;
#else
	const char *std_name;
	GF_Node *node;
	GF_SceneGraph *sg;
	u32 tag;
#ifndef GPAC_DISABLE_BIFS
#endif /*GPAC_DISABLE_BIFS*/
	Bool is_nodefield = 0;

	char *sep = strchr(name, '.');
	if (sep) {
		sep[0] = 0;
		is_nodefield = 1;
	}

	if (graph_type==1) {
#ifndef GPAC_DISABLE_X3D
		tag = gf_node_x3d_type_by_class_name(name);
		std_name = "X3D";
#else
		M4_LOG(GF_LOG_ERROR, ("X3D node printing is not supported (X3D support disabled)\n"));
		return 2;
#endif
	} else {
		tag = gf_node_mpeg4_type_by_class_name(name);
		std_name = "MPEG4";
	}
	if (!tag) {
		M4_LOG(GF_LOG_ERROR, ("Unknown %s node %s\n", std_name, name));
		return 2;
	}

	sg = gf_sg_new();
	node = gf_node_new(sg, tag);
	gf_node_register(node, NULL);
	name = gf_node_get_class_name(node);
	if (!node) {
		M4_LOG(GF_LOG_ERROR, ("Node %s not supported in current built\n", name));
		return 2;
	}
	do_print_node(node, sg, name, graph_type, is_nodefield, GF_FALSE);

	gf_node_unregister(node, NULL);
	gf_sg_del(sg);
#endif /*GPAC_DISABLE_VRML*/
	return 1;
}

u32 PrintBuiltInNodes(char *arg_val, u32 dump_type)
{
#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_X3D) && !defined(GPAC_DISABLE_SVG)
	GF_SceneGraph *sg;
	u32 i, nb_in, nb_not_in, start_tag, end_tag;
	u32 graph_type;
	Bool dump_nodes = ((dump_type==1) || (dump_type==3)) ? 1 : 0;

	if (dump_type==4) graph_type = 2;
	else if ((dump_type==2) || (dump_type==3)) graph_type = 1;
	else graph_type = 0;

	if (graph_type==1) {
#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_X3D)
		start_tag = GF_NODE_RANGE_FIRST_X3D;
		end_tag = TAG_LastImplementedX3D;
#else
		M4_LOG(GF_LOG_ERROR, ("X3D scene graph disabled in this build of GPAC\n"));
		return 2;
#endif
	} else if (graph_type==2) {
#ifdef GPAC_DISABLE_SVG
		M4_LOG(GF_LOG_ERROR, ("SVG scene graph disabled in this build of GPAC\n"));
		return 2;
#else
		start_tag = GF_NODE_RANGE_FIRST_SVG;
		end_tag = GF_NODE_RANGE_LAST_SVG;
#endif
	} else {
#ifdef GPAC_DISABLE_VRML
		M4_LOG(GF_LOG_ERROR, ("VRML/MPEG-4 scene graph disabled in this build of GPAC\n"));
		return 2;
#else
		start_tag = GF_NODE_RANGE_FIRST_MPEG4;
		end_tag = TAG_LastImplementedMPEG4;
#endif
	}
	nb_in = nb_not_in = 0;
	sg = gf_sg_new();

	if (graph_type==1) {
		fprintf(stderr, "Available X3D nodes in this build (dumping):\n");
	} else if (graph_type==2) {
		fprintf(stderr, "Available SVG nodes in this build (dumping and LASeR coding):\n");
	} else {
		fprintf(stderr, "Available MPEG-4 nodes in this build (encoding/decoding/dumping):\n");
	}
	for (i=start_tag; i<end_tag; i++) {
		GF_Node *node = gf_node_new(sg, i);
		if (node) {
			gf_node_register(node, NULL);
			if (dump_nodes) {
				do_print_node(node, sg, gf_node_get_class_name(node), graph_type, GF_FALSE, GF_TRUE);
			} else {
				fprintf(stderr, " %s\n", gf_node_get_class_name(node));
			}
			gf_node_unregister(node, NULL);
			nb_in++;
		} else {
			if (graph_type==2)
				break;
			nb_not_in++;
		}
	}
	gf_sg_del(sg);
	if (graph_type==2) {
		fprintf(stderr, "\n%d nodes supported\n", nb_in);
	} else {
		fprintf(stderr, "\n%d nodes supported - %d nodes not supported\n", nb_in, nb_not_in);
	}
	//coverage
	if (dump_nodes) {
		for (i=GF_SG_VRML_SFBOOL; i<GF_SG_VRML_SCRIPT_FUNCTION; i++) {
			void *fp = gf_sg_vrml_field_pointer_new(i);
			if (fp) {
				if (i==GF_SG_VRML_SFSCRIPT) gf_free(fp);
				else gf_sg_vrml_field_pointer_del(fp, i);
			}
		}

	}
#else
	M4_LOG(GF_LOG_ERROR, ("No scene graph enabled in this MP4Box build\n"));
#endif
	return 1;
}


u32 PrintBuiltInBoxes(char *argval, u32 do_cov)
{
	u32 i, count=gf_isom_get_num_supported_boxes();
	
	fprintf(stdout, "<Boxes>\n");
	//index 0 is our internal unknown box handler
	for (i=1; i<count; i++) {
		gf_isom_dump_supported_box(i, stdout);
        if (do_cov) {
			u32 btype = gf_isom_get_supported_box_type(i);
            GF_Box *b=gf_isom_box_new(btype);
            if (b) {
                GF_Box *c=NULL;
                gf_isom_clone_box(b, &c);
                if (c) gf_isom_box_del(c);
                gf_isom_box_del(b);
            }
        }
	}
	fprintf(stdout, "</Boxes>\n");
	return 1;
}

#if !defined(GPAC_DISABLE_ISOM_HINTING) && !defined(GPAC_DISABLE_ISOM_DUMP)

void dump_isom_rtp(GF_ISOFile *file, char *inName, Bool is_final_name)
{
	u32 i, j, size;
	FILE *dump;
	const char *sdp;

	if (inName) {
		char szBuf[1024];
		strcpy(szBuf, inName);
		if (!is_final_name) strcat(szBuf, "_rtp.xml");
		dump = gf_fopen(szBuf, "wt");
		if (!dump) {
			M4_LOG(GF_LOG_ERROR, ("Failed to open %s\n", szBuf));
			return;
		}
	} else {
		dump = stdout;
	}

	fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
	fprintf(dump, "<!-- MP4Box RTP trace -->\n");
	fprintf(dump, "<RTPFile>\n");

	for (i=0; i<gf_isom_get_track_count(file); i++) {
		if (gf_isom_get_media_type(file, i+1) != GF_ISOM_MEDIA_HINT) continue;

		fprintf(dump, "<RTPHintTrack trackID=\"%d\">\n", gf_isom_get_track_id(file, i+1));
		gf_isom_sdp_track_get(file, i+1, &sdp, &size);
		fprintf(dump, "<SDPInfo>%s</SDPInfo>", sdp);

#ifndef GPAC_DISABLE_ISOM_HINTING
		for (j=0; j<gf_isom_get_sample_count(file, i+1); j++) {
			gf_isom_dump_hint_sample(file, i+1, j+1, dump);
		}
#endif
		fprintf(dump, "</RTPHintTrack>\n");
	}
	fprintf(dump, "</RTPFile>\n");
	if (inName) gf_fclose(dump);
}
#endif


void dump_isom_timestamps(GF_ISOFile *file, char *inName, Bool is_final_name, u32 dump_mode)
{
	u32 i, j, k, count;
	Bool has_ctts_error, is_fragmented=GF_FALSE;
	FILE *dump;
	Bool skip_offset = ((dump_mode==2) || (dump_mode==4)) ? GF_TRUE : GF_FALSE;
	Bool check_ts = ((dump_mode==3) || (dump_mode==4)) ? GF_TRUE : GF_FALSE;
	struct _ts_info {
		u64 dts;
		s64 cts;
	};
	struct _ts_info *timings = NULL;
	u32 nb_timings=0, nb_timings_alloc = 0;


	if (inName) {
		char szBuf[1024];
		strcpy(szBuf, inName);
		if (!is_final_name) strcat(szBuf, "_ts.txt");
		dump = gf_fopen(szBuf, "wt");
		if (!dump) {
			M4_LOG(GF_LOG_ERROR, ("Failed to open %s\n", szBuf));
			return;
		}
	} else {
		dump = stdout;
	}
	if (gf_isom_is_fragmented(file))
		is_fragmented = GF_TRUE;

	has_ctts_error = GF_FALSE;
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		s64 cts_dts_shift = gf_isom_get_cts_to_dts_shift(file, i+1);
		u32 has_cts_offset = gf_isom_has_time_offset(file, i+1);

		fprintf(dump, "#dumping track ID %d timing:\n", gf_isom_get_track_id(file, i + 1));
		fprintf(dump, "Num\tDTS\tCTS\tSize\tRAP%s\tisLeading\tDependsOn\tDependedOn\tRedundant\tRAP-SampleGroup\tRoll-SampleGroup\tRoll-Distance", skip_offset ? "" : "\tOffset");
		if (is_fragmented) {
			fprintf(dump, "\tfrag_start");
		}
		fprintf(dump, "\n");


		count = gf_isom_get_sample_count(file, i+1);
		if (has_cts_offset && check_ts) {
			if (nb_timings_alloc<count) {
				nb_timings_alloc = count;
				timings = gf_realloc(timings, sizeof (struct _ts_info) * count);
			}
			nb_timings = 0;
		}

		for (j=0; j<count; j++) {
			s64 cts;
			u64 dts, offset;
			u32 isLeading, dependsOn, dependedOn, redundant;
			Bool is_rap;
			GF_ISOSampleRollType roll_type;
			s32 roll_distance;
			u32 index;
			GF_ISOSample *samp = gf_isom_get_sample_info(file, i+1, j+1, &index, &offset);
			if (!samp) {
				fprintf(dump, " SAMPLE #%d IN TRACK #%d NOT THERE !!!\n", j+1, i+1);
				continue;
			}
			gf_isom_get_sample_flags(file, i+1, j+1, &isLeading, &dependsOn, &dependedOn, &redundant);
			gf_isom_get_sample_rap_roll_info(file, i+1, j+1, &is_rap, &roll_type, &roll_distance);
			dts = samp->DTS;
			cts = dts + (s32) samp->CTS_Offset;
			fprintf(dump, "Sample %d\tDTS "LLU"\tCTS "LLD"\t%d\t%d", j+1, dts, cts, samp->dataLength, samp->IsRAP);

			if (!skip_offset)
				fprintf(dump, "\t"LLU, offset);

			fprintf(dump, "\t%d\t%d\t%d\t%d\t%d\t%d\t%d", isLeading, dependsOn, dependedOn, redundant, is_rap, roll_type, roll_distance);

			if (cts< (s64) dts) {
				if (has_cts_offset==2) {
					if (cts_dts_shift && (cts+cts_dts_shift < (s64) dts)) {
						fprintf(dump, " #NEGATIVE CTS OFFSET!!!");
						has_ctts_error = 1;
					} else if (!cts_dts_shift) {
						fprintf(dump, " #possible negative CTS offset (no cslg in file)");
					}
				} else {
					fprintf(dump, " #NEGATIVE CTS OFFSET!!!");
					has_ctts_error = 1;
				}
			}
			if (has_cts_offset && check_ts) {
				for (k=0; k<nb_timings; k++) {

					if (timings[k].dts==dts) {
						fprintf(dump, " #SAME DTS USED!!!");
						has_ctts_error = 1;
					}
					if (timings[k].cts==cts) {
						fprintf(dump, " #SAME CTS USED!!! ");
						has_ctts_error = 1;
					}
				}
				timings[nb_timings].dts = dts;
				timings[nb_timings].cts = cts;
				nb_timings++;
			}

			gf_isom_sample_del(&samp);

			if (is_fragmented) {
				fprintf(dump, "\t%d", gf_isom_sample_is_fragment_start(file, i+1, j+1, NULL) );
			}
			fprintf(dump, "\n");
			gf_set_progress("Dumping track timing", j+1, count);
		}
		fprintf(dump, "\n\n");
		gf_set_progress("Dumping track timing", count, count);
	}
	if (timings) gf_free(timings);

	if (inName) gf_fclose(dump);
	if (has_ctts_error) {
		M4_LOG(GF_LOG_ERROR, ("\tFile has CTTS table errors\n"));
	}
}



static u32 read_nal_size_hdr(u8 *ptr, u32 nalh_size)
{
	u32 nal_size=0;
	u32 v = nalh_size;
	while (v) {
		nal_size |= (u8) *ptr;
		ptr++;
		v-=1;
		if (v) nal_size <<= 8;
	}
	return nal_size;
}

#ifndef GPAC_DISABLE_AV_PARSERS
void gf_inspect_dump_nalu(FILE *dump, u8 *ptr, u32 ptr_size, Bool is_svc, HEVCState *hevc, AVCState *avc, VVCState *vvc, u32 nalh_size, Bool dump_crc, Bool is_encrypted);
#endif


static void dump_isom_nal_ex(GF_ISOFile *file, GF_ISOTrackID trackID, FILE *dump, u32 dump_flags)
{
	u32 i, j, count, nb_descs, track, nalh_size, timescale, cur_extract_mode;
	s32 countRef;
	Bool is_adobe_protected = GF_FALSE;
	Bool is_cenc_protected = GF_FALSE;
	Bool is_hevc = GF_FALSE;
	Bool is_vvc = GF_FALSE;
#ifndef GPAC_DISABLE_AV_PARSERS
	AVCState *avc_state = NULL;
	HEVCState *hevc_state = NULL;
	VVCState *vvc_state = NULL;
#endif
	GF_AVCConfig *avccfg, *svccfg;
	GF_HEVCConfig *hevccfg, *lhvccfg;
	GF_VVCConfig *vvccfg;
	GF_NALUFFParam *slc;
	Bool has_svcc = GF_FALSE;

	track = gf_isom_get_track_by_id(file, trackID);

	count = gf_isom_get_sample_count(file, track);

	timescale = gf_isom_get_media_timescale(file, track);

	cur_extract_mode = gf_isom_get_nalu_extract_mode(file, track);

	nb_descs = gf_isom_get_sample_description_count(file, track);
	if (!nb_descs) {
		M4_LOG(GF_LOG_ERROR, ("Error: Track #%d has no sample description so is likely not NALU-based!\n", trackID));
		return;
	}

	fprintf(dump, "<NALUTrack trackID=\"%d\" SampleCount=\"%d\" TimeScale=\"%d\">\n", trackID, count, timescale);

#ifndef GPAC_DISABLE_AV_PARSERS

#define DUMP_ARRAY(arr, name, loc, _is_svc)\
	if (arr) {\
		fprintf(dump, "  <%sArray location=\"%s\">\n", name, loc);\
		for (i=0; i<gf_list_count(arr); i++) {\
			slc = gf_list_get(arr, i);\
			fprintf(dump, "   <NALU size=\"%d\" ", slc->size);\
			gf_inspect_dump_nalu(dump, (u8 *) slc->data, slc->size, _is_svc, is_hevc ? hevc_state : NULL, avc_state, is_vvc ? vvc_state : NULL, nalh_size, (dump_flags&1) ? GF_TRUE : GF_FALSE, GF_FALSE);\
		}\
		fprintf(dump, "  </%sArray>\n", name);\
	}\

#else

#define DUMP_ARRAY(arr, name, loc, _is_svc)\
	if (arr) {\
		fprintf(dump, "  <%sArray location=\"%s\">\n", name, loc);\
		for (i=0; i<gf_list_count(arr); i++) {\
			slc = gf_list_get(arr, i);\
			fprintf(dump, "   <NALU size=\"%d\" ", slc->size);\
			fprintf(dump, "/>\n");\
		}\
		fprintf(dump, "  </%sArray>\n", name);\
	}\

#endif

	nalh_size = 0;

	for (j=0; j<nb_descs; j++) {
		GF_AVCConfig *mvccfg;
		Bool is_svc;

		avccfg = gf_isom_avc_config_get(file, track, j+1);
		svccfg = gf_isom_svc_config_get(file, track, j+1);
		mvccfg = gf_isom_mvc_config_get(file, track, j+1);
		hevccfg = gf_isom_hevc_config_get(file, track, j+1);
		lhvccfg = gf_isom_lhvc_config_get(file, track, j+1);
		vvccfg = gf_isom_vvc_config_get(file, track, j+1);

		is_svc = (svccfg!=NULL) ? 1:0;

		if (hevccfg || lhvccfg) {
			is_hevc = 1;
#ifndef GPAC_DISABLE_AV_PARSERS
			GF_SAFEALLOC(hevc_state, HEVCState)
#endif
		} else if (vvccfg) {
			is_vvc = 1;
#ifndef GPAC_DISABLE_AV_PARSERS
			GF_SAFEALLOC(vvc_state, VVCState)
#endif
		} else if (avccfg || svccfg || mvccfg) {
#ifndef GPAC_DISABLE_AV_PARSERS
			GF_SAFEALLOC(avc_state, AVCState)
#endif
		}


		//for tile tracks the hvcC is stored in the 'tbas' track
		if (!hevccfg && gf_isom_get_reference_count(file, track, GF_ISOM_REF_TBAS)) {
			u32 tk = 0;
			gf_isom_get_reference(file, track, GF_ISOM_REF_TBAS, 1, &tk);
			hevccfg = gf_isom_hevc_config_get(file, tk, 1);
		}

		fprintf(dump, " <NALUConfig>\n");

		if (!avccfg && !svccfg && !hevccfg && !lhvccfg && !vvccfg) {
			M4_LOG(GF_LOG_ERROR, ("Error: Track #%d is not NALU or OBU based!\n", trackID));
			return;
		}

		if (avccfg) {
			nalh_size = avccfg->nal_unit_size;

			DUMP_ARRAY(avccfg->sequenceParameterSets, "AVCSPS", "avcC", is_svc);
			DUMP_ARRAY(avccfg->pictureParameterSets, "AVCPPS", "avcC", is_svc)
			DUMP_ARRAY(avccfg->sequenceParameterSetExtensions, "AVCSPSEx", "avcC", is_svc)
		}
		if (is_svc) {
			if (!nalh_size) nalh_size = svccfg->nal_unit_size;
			DUMP_ARRAY(svccfg->sequenceParameterSets, "SVCSPS", "svcC", is_svc)
			DUMP_ARRAY(svccfg->pictureParameterSets, "SVCPPS", "svcC", is_svc)
		}
		if (mvccfg) {
			if (!nalh_size) nalh_size = mvccfg->nal_unit_size;
			DUMP_ARRAY(mvccfg->sequenceParameterSets, "SVCSPS", "mvcC", is_svc)
			DUMP_ARRAY(mvccfg->pictureParameterSets, "SVCPPS", "mvcC", is_svc)
		}
		if (hevccfg) {
			u32 idx;
			nalh_size = hevccfg->nal_unit_size;
			for (idx=0; idx<gf_list_count(hevccfg->param_array); idx++) {
				GF_NALUFFParamArray *ar = gf_list_get(hevccfg->param_array, idx);
				if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
					DUMP_ARRAY(ar->nalus, "HEVCSPS", "hvcC", 0)
				} else if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
					DUMP_ARRAY(ar->nalus, "HEVCPPS", "hvcC", 0)
				} else if (ar->type==GF_HEVC_NALU_VID_PARAM) {
					DUMP_ARRAY(ar->nalus, "HEVCVPS", "hvcC", 0)
				} else {
					DUMP_ARRAY(ar->nalus, "HEVCUnknownPS", "hvcC", 0)
				}
			}
		}
		if (vvccfg) {
			u32 idx;
			nalh_size = vvccfg->nal_unit_size;
			for (idx=0; idx<gf_list_count(vvccfg->param_array); idx++) {
				GF_NALUFFParamArray *ar = gf_list_get(vvccfg->param_array, idx);
				if (ar->type==GF_VVC_NALU_SEQ_PARAM) {
					DUMP_ARRAY(ar->nalus, "VVCSPS", "vvcC", 0)
				} else if (ar->type==GF_VVC_NALU_PIC_PARAM) {
					DUMP_ARRAY(ar->nalus, "VVCPPS", "vvcC", 0)
				} else if (ar->type==GF_VVC_NALU_VID_PARAM) {
					DUMP_ARRAY(ar->nalus, "VVCVPS", "vvcC", 0)
				} else {
					DUMP_ARRAY(ar->nalus, "VVCUnknownPS", "vvcC", 0)
				}
			}
		}
		if (lhvccfg) {
			u32 idx;
			nalh_size = lhvccfg->nal_unit_size;
			for (idx=0; idx<gf_list_count(lhvccfg->param_array); idx++) {
				GF_NALUFFParamArray *ar = gf_list_get(lhvccfg->param_array, idx);
				if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
					DUMP_ARRAY(ar->nalus, "HEVCSPS", "lhcC", 0)
				} else if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
					DUMP_ARRAY(ar->nalus, "HEVCPPS", "lhcC", 0)
				} else if (ar->type==GF_HEVC_NALU_VID_PARAM) {
					DUMP_ARRAY(ar->nalus, "HEVCVPS", "lhcC", 0)
				} else {
					DUMP_ARRAY(ar->nalus, "HEVCUnknownPS", "lhcC", 0)
				}
			}
		}
		fprintf(dump, " </NALUConfig>\n");

		if (avccfg) gf_odf_avc_cfg_del(avccfg);
		if (svccfg) {
			gf_odf_avc_cfg_del(svccfg);
			has_svcc = GF_TRUE;
		}
		if (hevccfg) gf_odf_hevc_cfg_del(hevccfg);
		if (vvccfg) gf_odf_vvc_cfg_del(vvccfg);
		if (lhvccfg) gf_odf_hevc_cfg_del(lhvccfg);
	}

	/*fixme: for dumping encrypted track: we don't have neither avccfg nor svccfg*/
	if (!nalh_size) nalh_size = 4;

	/*for testing dependency*/
	countRef = gf_isom_get_reference_count(file, track, GF_ISOM_REF_SCAL);
	if (countRef > 0)
	{
		GF_ISOTrackID refTrackID;
		fprintf(dump, " <SCALReferences>\n");
		for (i = 1; i <= (u32) countRef; i++)
		{
			gf_isom_get_reference_ID(file, track, GF_ISOM_REF_SCAL, i, &refTrackID);
			fprintf(dump, "  <SCALReference number=\"%d\" refTrackID=\"%d\"/>\n", i, refTrackID);
		}

		fprintf(dump, " </SCALReferences>\n");
	}

	fprintf(dump, " <NALUSamples>\n");
	gf_isom_set_nalu_extract_mode(file, track, GF_ISOM_NALU_EXTRACT_INSPECT);
	is_adobe_protected = gf_isom_is_adobe_protection_media(file, track, 1);
	is_cenc_protected = gf_isom_is_cenc_media(file, track, 1);
	for (i=0; i<count; i++) {
		u64 dts, cts;
		Bool is_rap;
		u32 size, nal_size, idx, di;
		u8 *ptr;
		GF_ISOSample *samp = gf_isom_get_sample(file, track, i+1, &di);
		if (!samp) {
			fprintf(dump, "<!-- Unable to fetch sample %d -->\n", i+1);
			continue;
		}
		dts = samp->DTS;
		cts = dts + (s32) samp->CTS_Offset;
		is_rap = samp->IsRAP;
		if (!is_rap) gf_isom_get_sample_rap_roll_info(file, track, i+1, &is_rap, NULL, NULL);

		if (dump_flags&2) {
			fprintf(dump, "  <Sample size=\"%d\" RAP=\"%d\"", samp->dataLength, is_rap);
		} else {
			fprintf(dump, "  <Sample DTS=\""LLD"\" CTS=\""LLD"\" size=\"%d\" RAP=\"%d\"", dts, cts, samp->dataLength, is_rap);
		}
		if (nb_descs>1)
			fprintf(dump, " sample_description=\"%d\"", di);
		fprintf(dump, " >\n");

		if (cts<dts) fprintf(dump, "<!-- NEGATIVE CTS OFFSET! -->\n");

		idx = 1;
		ptr = samp->data;
		size = samp->dataLength;
		if (is_adobe_protected) {
			u8 encrypted_au = ptr[0];
			if (encrypted_au) {
				fprintf(dump, "   <!-- Sample number %d is an Adobe's protected sample: can not be dumped -->\n", i+1);
				fprintf(dump, "  </Sample>\n\n");
				continue;
			}
			else {
				ptr++;
				size--;
			}
		}
		while (size) {
			nal_size = read_nal_size_hdr(ptr, nalh_size);
			ptr += nalh_size;

			if (nal_size >= UINT_MAX-nalh_size || nalh_size + nal_size > size) {
				fprintf(dump, "   <!-- NALU number %d is corrupted: size is %d but only %d remains -->\n", idx, nal_size, size);
				break;
			} else {
				fprintf(dump, "   <NALU size=\"%d\" ", nal_size);
#ifndef GPAC_DISABLE_AV_PARSERS
				Bool is_encrypted = 0;
				if (is_cenc_protected) {
					GF_Err e = gf_isom_get_sample_cenc_info(file, track, i + 1, &is_encrypted, NULL, NULL, NULL, NULL);
					if (e != GF_OK) {
						fprintf(dump, "dump_msg=\"Error %s while fetching encryption info for sample, assuming sample is encrypted\" ", gf_error_to_string(e) );
						is_encrypted = GF_TRUE;
					}
				}
				gf_inspect_dump_nalu(dump, ptr, nal_size, has_svcc ? 1 : 0, hevc_state, avc_state, vvc_state, nalh_size, dump_flags, is_encrypted);
#else
				fprintf(dump, "/>\n");
#endif
			}
			idx++;
			ptr+=nal_size;
			size -= nal_size + nalh_size;
		}
		fprintf(dump, "  </Sample>\n");
		gf_isom_sample_del(&samp);

		fprintf(dump, "\n");
		gf_set_progress("Analysing Track NALUs", i+1, count);
	}
	fprintf(dump, " </NALUSamples>\n");
	fprintf(dump, "</NALUTrack>\n");

	gf_isom_set_nalu_extract_mode(file, track, cur_extract_mode);

#ifndef GPAC_DISABLE_AV_PARSERS
	if (hevc_state) gf_free(hevc_state);
	if (vvc_state) gf_free(vvc_state);
	if (avc_state) gf_free(avc_state);
#endif
}

static void dump_isom_obu(GF_ISOFile *file, GF_ISOTrackID trackID, FILE *dump, Bool dump_crc);
static void dump_qt_prores(GF_ISOFile *file, GF_ISOTrackID trackID, FILE *dump, Bool dump_crc);

void dump_isom_nal(GF_ISOFile *file, GF_ISOTrackID trackID, char *inName, Bool is_final_name, u32 dump_flags)
{
	Bool is_av1 = GF_FALSE;
	Bool is_prores = GF_FALSE;

	FILE *dump;
	if (inName) {
		GF_ESD* esd;
		char szBuf[GF_MAX_PATH];

		strcpy(szBuf, inName);

		u32 track = gf_isom_get_track_by_id(file, trackID);
		esd = gf_isom_get_esd(file, track, 1);

		if (!esd || !esd->decoderConfig) {
			switch (gf_isom_get_media_subtype(file, track, 1)) {
			case GF_ISOM_SUBTYPE_AV01:
				is_av1 = GF_TRUE;
				break;
			case GF_QT_SUBTYPE_APCH:
			case GF_QT_SUBTYPE_APCO:
			case GF_QT_SUBTYPE_APCN:
			case GF_QT_SUBTYPE_APCS:
			case GF_QT_SUBTYPE_AP4X:
			case GF_QT_SUBTYPE_AP4H:
				is_prores = GF_TRUE;
				break;
			}
		}
		else if (esd->decoderConfig->objectTypeIndication == GF_CODECID_AV1) {
			is_av1 = GF_TRUE;
		}
		if (esd) gf_odf_desc_del((GF_Descriptor*)esd);

		if (!is_final_name) sprintf(szBuf, "%s_%d_%s.xml", inName, trackID, is_av1 ? "obu" : "nalu");
		dump = gf_fopen(szBuf, "wt");
		if (!dump) {
			M4_LOG(GF_LOG_ERROR, ("Failed to open %s for dumping\n", szBuf));
			return;
		}
	} else {
		dump = stdout;
	}

	if (is_av1)
		dump_isom_obu(file, trackID, dump, dump_flags);
	else if (is_prores)
		dump_qt_prores(file, trackID, dump, dump_flags);
	else
		dump_isom_nal_ex(file, trackID, dump, dump_flags);

	if (inName) gf_fclose(dump);
}

#ifndef GPAC_DISABLE_AV_PARSERS
void gf_inspect_dump_obu(FILE *dump, AV1State *av1, u8 *obu, u64 obu_length, ObuType obu_type, u64 obu_size, u32 hdr_size, Bool dump_crc);
#endif

static void dump_isom_obu(GF_ISOFile *file, GF_ISOTrackID trackID, FILE *dump, Bool dump_crc)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	u32 i, count, track, timescale;
	AV1State av1;
	ObuType obu_type;
	u64 obu_size;
	u32 hdr_size;
	GF_BitStream *bs;
	u32 idx;

	track = gf_isom_get_track_by_id(file, trackID);

	gf_av1_init_state(&av1);
	av1.config = gf_isom_av1_config_get(file, track, 1);
	if (!av1.config) {
		M4_LOG(GF_LOG_ERROR, ("Error: Track #%d is not AV1!\n", trackID));
		return;
	}

	count = gf_isom_get_sample_count(file, track);
	timescale = gf_isom_get_media_timescale(file, track);

	fprintf(dump, "<OBUTrack trackID=\"%d\" SampleCount=\"%d\" TimeScale=\"%d\">\n", trackID, count, timescale);

	fprintf(dump, " <OBUConfig>\n");

	for (i=0; i<gf_list_count(av1.config->obu_array); i++) {
		GF_AV1_OBUArrayEntry *obu = gf_list_get(av1.config->obu_array, i);
		bs = gf_bs_new(obu->obu, (u32) obu->obu_length, GF_BITSTREAM_READ);
		gf_av1_parse_obu(bs, &obu_type, &obu_size, &hdr_size, &av1);
		gf_inspect_dump_obu(dump, &av1, obu->obu, obu->obu_length, obu_type, obu_size, hdr_size, dump_crc);
		gf_bs_del(bs);
	}
	fprintf(dump, " </OBUConfig>\n");

	fprintf(dump, " <OBUSamples>\n");

	for (i=0; i<count; i++) {
		u64 dts, cts;
		u32 size;
		u8 *ptr;
		GF_ISOSample *samp = gf_isom_get_sample(file, track, i+1, NULL);
		if (!samp) {
			fprintf(dump, "<!-- Unable to fetch sample %d -->\n", i+1);
			continue;
		}
		dts = samp->DTS;
		cts = dts + (s32) samp->CTS_Offset;

		fprintf(dump, "  <Sample number=\"%d\" DTS=\""LLD"\" CTS=\""LLD"\" size=\"%d\" RAP=\"%d\" >\n", i+1, dts, cts, samp->dataLength, samp->IsRAP);
		if (cts<dts) fprintf(dump, "<!-- NEGATIVE CTS OFFSET! -->\n");

		idx = 1;
		ptr = samp->data;
		size = samp->dataLength;

		bs = gf_bs_new(ptr, size, GF_BITSTREAM_READ);
		while (size) {
			gf_av1_parse_obu(bs, &obu_type, &obu_size, &hdr_size, &av1);
			if (obu_size > size) {
				fprintf(dump, "   <!-- OBU number %d is corrupted: size is %d but only %d remains -->\n", idx, (u32) obu_size, size);
				break;
			}
			gf_inspect_dump_obu(dump, &av1, ptr, obu_size, obu_type, obu_size, hdr_size, dump_crc);
			ptr += obu_size;
			size -= (u32)obu_size;
			idx++;
		}
		gf_bs_del(bs);
		fprintf(dump, "  </Sample>\n");
		gf_isom_sample_del(&samp);

		fprintf(dump, "\n");
		gf_set_progress("Analysing Track OBUs", i+1, count);
	}
	fprintf(dump, " </OBUSamples>\n");
	fprintf(dump, "</OBUTrack>\n");

	if (av1.config) gf_odf_av1_cfg_del(av1.config);
	gf_av1_reset_state(&av1, GF_TRUE);
#endif
}

static void dump_qt_prores(GF_ISOFile *file, u32 trackID, FILE *dump, Bool dump_crc)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	u32 i, count, track, timescale;

	track = gf_isom_get_track_by_id(file, trackID);

	count = gf_isom_get_sample_count(file, track);
	timescale = gf_isom_get_media_timescale(file, track);

	fprintf(dump, "<ProResTrack trackID=\"%d\" SampleCount=\"%d\" TimeScale=\"%d\">\n", trackID, count, timescale);

	for (i=0; i<count; i++) {
		void gf_inspect_dump_prores(FILE *dump, u8 *ptr, u64 frame_size, Bool dump_crc);
		u64 dts, cts;
		GF_ISOSample *samp = gf_isom_get_sample(file, track, i+1, NULL);
		if (!samp) {
			fprintf(dump, "<!-- Unable to fetch sample %d -->\n", i+1);
			continue;
		}
		dts = samp->DTS;
		cts = dts + (s32) samp->CTS_Offset;

		if (cts!=dts) fprintf(dump, "<!-- Wrong timing info (CTS "LLD" vs DTS "LLD") ! -->\n", cts, dts);
		if (!samp->IsRAP) fprintf(dump, "<!-- Wrong sync sample info, sample is not SAP1 ! -->\n");

		fprintf(dump, "  <Sample number=\"%d\" CTS=\""LLD"\" size=\"%d\">\n", i+1, cts, samp->dataLength);

		gf_inspect_dump_prores(dump, samp->data, samp->dataLength, dump_crc);
		fprintf(dump, "  </Sample>\n");

		gf_isom_sample_del(&samp);

		fprintf(dump, "\n");
		gf_set_progress("Analysing ProRes Track", i+1, count);
	}
	fprintf(dump, "</ProResTrack>\n");
#endif
}

void dump_isom_saps(GF_ISOFile *file, GF_ISOTrackID trackID, u32 dump_saps_mode, char *inName, Bool is_final_name)
{
	FILE *dump;
	u32 i, count;
	s64 media_offset=0;
	u32 track = gf_isom_get_track_by_id(file, trackID);
	if (inName) {
		char szBuf[GF_MAX_PATH];
		strcpy(szBuf, inName);

		if (!is_final_name) sprintf(szBuf, "%s_%d_cues.xml", inName, trackID);
		dump = gf_fopen(szBuf, "wt");
		if (!dump) {
			M4_LOG(GF_LOG_ERROR, ("Failed to open %s for dumping\n", szBuf));
			return;
		}
	} else {
		dump = stdout;
	}

	fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(dump, "<DASHCues xmlns=\"urn:gpac:dash:schema:cues:2018\">\n");
	fprintf(dump, "<Stream id=\"%d\" timescale=\"%d\"", trackID, gf_isom_get_media_timescale(file, track) );
	if (dump_saps_mode==4) {
		fprintf(dump, " mode=\"edit\"");
		gf_isom_get_edit_list_type(file, track, &media_offset);
	}
	fprintf(dump, ">\n");

	count = gf_isom_get_sample_count(file, track);
	for (i=0; i<count; i++) {
		s64 cts, dts;
		u32 di;
		Bool traf_start = 0;
		u32 sap_type = 0;
		u64 doffset;
		
		GF_ISOSample *samp = gf_isom_get_sample_info(file, track, i+1, &di, &doffset);

		traf_start = gf_isom_sample_is_fragment_start(file, track, i+1, NULL);

		sap_type = samp->IsRAP;
		if (!sap_type) {
			Bool is_rap;
			GF_ISOSampleRollType roll_type;
			s32 roll_dist;
			gf_isom_get_sample_rap_roll_info(file, track, i+1, &is_rap, &roll_type, &roll_dist);
			if (roll_type) sap_type = SAP_TYPE_4;
			else if (is_rap)  sap_type = SAP_TYPE_3;
		}

		if (!sap_type) {
			gf_isom_sample_del(&samp);
			continue;
		}

		dts = cts = samp->DTS;
		cts += samp->CTS_Offset;
		fprintf(dump, "<Cue sap=\"%d\"", sap_type);
		if (dump_saps_mode==4) {
			cts += media_offset;
			fprintf(dump, " cts=\""LLD"\"", cts);
		} else {
			if (!dump_saps_mode || (dump_saps_mode==1)) fprintf(dump, " sample=\"%d\"", i+1);
			if (!dump_saps_mode || (dump_saps_mode==2)) fprintf(dump, " cts=\""LLD"\"", cts);
			if (!dump_saps_mode || (dump_saps_mode==3)) fprintf(dump, " dts=\""LLD"\"", dts);
		}

		if (traf_start)
			fprintf(dump, " wasFragStart=\"yes\"");

		fprintf(dump, "/>\n");

		gf_isom_sample_del(&samp);
	}
	fprintf(dump, "</Stream>\n");
	fprintf(dump, "</DASHCues>\n");
	if (inName) gf_fclose(dump);
}

#ifndef GPAC_DISABLE_ISOM_DUMP

void dump_isom_ismacryp(GF_ISOFile *file, char *inName, Bool is_final_name)
{
	u32 i, j;
	FILE *dump;

	if (inName) {
		char szBuf[1024];
		strcpy(szBuf, inName);
		if (!is_final_name) strcat(szBuf, "_ismacryp.xml");
		dump = gf_fopen(szBuf, "wt");
		if (!dump) {
			M4_LOG(GF_LOG_ERROR, ("Failed to open %s for dumping\n", szBuf));
			return;
		}
	} else {
		dump = stdout;
	}

	fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
	fprintf(dump, "<!-- MP4Box ISMACryp trace -->\n");
	fprintf(dump, "<ISMACrypFile>\n");


	for (i=0; i<gf_isom_get_track_count(file); i++) {
		if (gf_isom_get_media_subtype(file, i+1, 1) != GF_ISOM_SUBTYPE_MPEG4_CRYP) continue;

		gf_isom_dump_ismacryp_protection(file, i+1, dump);

		fprintf(dump, "<ISMACrypTrack trackID=\"%d\">\n", gf_isom_get_track_id(file, i+1));
		for (j=0; j<gf_isom_get_sample_count(file, i+1); j++) {
			gf_isom_dump_ismacryp_sample(file, i+1, j+1, dump);
		}
		fprintf(dump, "</ISMACrypTrack >\n");
	}
	fprintf(dump, "</ISMACrypFile>\n");
	if (inName) gf_fclose(dump);
}


void dump_isom_timed_text(GF_ISOFile *file, GF_ISOTrackID trackID, char *inName, Bool is_final_name, Bool is_convert, GF_TextDumpType dump_type)
{
	FILE *dump;
	GF_Err e;
	u32 track;

	track = gf_isom_get_track_by_id(file, trackID);
	if (!track) {
		M4_LOG(GF_LOG_ERROR, ("Cannot find track ID %d\n", trackID));
		return;
	}

	switch (gf_isom_get_media_type(file, track)) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		M4_LOG(GF_LOG_ERROR, ("Track ID %d is not a 3GPP text track\n", trackID));
		return;
	}

	if (inName) {
		char szBuf[1024];
		char *ext;
		ext = ((dump_type==GF_TEXTDUMPTYPE_SVG) ? "svg" : ((dump_type==GF_TEXTDUMPTYPE_SRT) ? "srt" : "ttxt"));
		if (is_final_name) {
			strcpy(szBuf, inName) ;
		} else if (is_convert)
			sprintf(szBuf, "%s.%s", inName, ext) ;
		else
			sprintf(szBuf, "%s_%d_text.%s", inName, trackID, ext);

		dump = gf_fopen(szBuf, "wt");
		if (!dump) {
			M4_LOG(GF_LOG_ERROR, ("Failed to open %s for dumping\n", szBuf));
			return;
		}
	} else {
		dump = stdout;
	}
	e = gf_isom_text_dump(file, track, dump, dump_type);
	if (inName) gf_fclose(dump);

	if (e) {
		M4_LOG(GF_LOG_ERROR, ("Conversion failed (%s)\n", gf_error_to_string(e)));
	} else {
		fprintf(stderr, "Conversion done\n");
	}
}

#endif /*GPAC_DISABLE_ISOM_DUMP*/

#ifndef GPAC_DISABLE_ISOM_HINTING

void dump_isom_sdp(GF_ISOFile *file, char *inName, Bool is_final_name)
{
	const char *sdp;
	u32 size, i;
	FILE *dump;

	if (inName) {
		char szBuf[1024];
		strcpy(szBuf, inName);
		if (!is_final_name) {
			char *ext = strchr(szBuf, '.');
			if (ext) ext[0] = 0;
			strcat(szBuf, "_sdp.txt");
		}
		dump = gf_fopen(szBuf, "wt");
		if (!dump) {
			M4_LOG(GF_LOG_ERROR, ("Failed to open %s for dumping\n", szBuf));
			return;
		}
	} else {
		dump = stdout;
		fprintf(dump, "# File SDP content \n\n");
	}
	//get the movie SDP
	gf_isom_sdp_get(file, &sdp, &size);
	if (sdp && size)
		fprintf(dump, "%s", sdp);
	fprintf(dump, "\r\n");

	//then tracks
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		if (gf_isom_get_media_type(file, i+1) != GF_ISOM_MEDIA_HINT) continue;
		gf_isom_sdp_track_get(file, i+1, &sdp, &size);
		fprintf(dump, "%s", sdp);
	}
	fprintf(dump, "\n\n");
	if (inName) gf_fclose(dump);
}

#endif


#ifndef GPAC_DISABLE_ISOM_DUMP

GF_Err dump_isom_xml(GF_ISOFile *file, char *inName, Bool is_final_name, Bool do_track_dump, Bool merge_vtt_cues, Bool skip_init, Bool skip_samples)
{
	GF_Err e;
	FILE *dump = stdout;
	Bool do_close=GF_FALSE;
	if (!file) return GF_ISOM_INVALID_FILE;

	if (inName) {
		char szBuf[1024];
		strcpy(szBuf, inName);
		if (!is_final_name) {
			strcat(szBuf, do_track_dump ? "_dump.xml" : "_info.xml");
		}
		dump = gf_fopen(szBuf, "wt");
		if (!dump) {
			M4_LOG(GF_LOG_ERROR, ("Failed to open %s\n", szBuf));
			return GF_IO_ERR;
		}
		do_close=GF_TRUE;
	}

	fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	if (do_track_dump) {
		fprintf(dump, "<ISOBaseMediaFileTrace>\n");
	}
	e = gf_isom_dump(file, dump, skip_init, skip_samples);
	if (e) {
		M4_LOG(GF_LOG_ERROR, ("Error dumping ISO structure\n"));
	}

	if (do_track_dump) {
#ifndef GPAC_DISABLE_MEDIA_EXPORT
		u32 i;
		//because of dump mode we need to reopen in regular read mode to avoid mem leaks
		GF_ISOFile *the_file = gf_isom_open(gf_isom_get_filename(file), GF_ISOM_OPEN_READ, NULL);
		u32 tcount = gf_isom_get_track_count(the_file);
		fprintf(dump, "<Tracks>\n");

		for (i=0; i<tcount; i++) {
			GF_MediaExporter dumper;
			GF_ISOTrackID trackID = gf_isom_get_track_id(the_file, i+1);
			u32 mtype = gf_isom_get_media_type(the_file, i+1);
			u32 msubtype = gf_isom_get_media_subtype(the_file, i+1, 1);
			Bool fmt_handled = GF_FALSE;
			memset(&dumper, 0, sizeof(GF_MediaExporter));
			dumper.file = the_file;
			dumper.trackID = trackID;
			dumper.dump_file = dump;

			if (mtype == GF_ISOM_MEDIA_HINT) {
#ifndef GPAC_DISABLE_ISOM_HINTING
				char *name=NULL;
				if (msubtype==GF_ISOM_SUBTYPE_RTP) name = "RTPHintTrack";
				else if (msubtype==GF_ISOM_SUBTYPE_SRTP) name = "SRTPHintTrack";
				else if (msubtype==GF_ISOM_SUBTYPE_RRTP) name = "RTPReceptionHintTrack";
				else if (msubtype==GF_ISOM_SUBTYPE_RTCP) name = "RTCPReceptionHintTrack";
				else if (msubtype==GF_ISOM_SUBTYPE_FLUTE) name = "FLUTEReceptionHintTrack";
				else name = "UnknownHintTrack";

				fprintf(dump, "<%s trackID=\"%d\">\n", name, trackID);

#ifndef GPAC_DISABLE_ISOM_HINTING
				u32 j, scount=gf_isom_get_sample_count(the_file, i+1);
				for (j=0; j<scount; j++) {
					gf_isom_dump_hint_sample(the_file, i+1, j+1, dump);
				}
#endif
				fprintf(dump, "</%s>\n", name);
				fmt_handled = GF_TRUE;
#endif /*GPAC_DISABLE_ISOM_HINTING*/
			}
			else if (gf_isom_get_avc_svc_type(the_file, i+1, 1) || gf_isom_get_hevc_lhvc_type(the_file, i+1, 1)) {
				dump_isom_nal_ex(the_file, trackID, dump, GF_FALSE);
				fmt_handled = GF_TRUE;
			} else if ((mtype==GF_ISOM_MEDIA_TEXT) || (mtype==GF_ISOM_MEDIA_SUBT) ) {

				if (msubtype==GF_ISOM_SUBTYPE_WVTT) {
					gf_webvtt_dump_iso_track(&dumper, i+1, merge_vtt_cues, GF_TRUE);
					fmt_handled = GF_TRUE;
				} else if ((msubtype==GF_ISOM_SUBTYPE_TX3G) || (msubtype==GF_ISOM_SUBTYPE_TEXT)) {
					gf_isom_text_dump(the_file, i+1, dump, GF_TEXTDUMPTYPE_TTXT_BOXES);
					fmt_handled = GF_TRUE;
				}
			}

			if (!fmt_handled) {
				dumper.flags = GF_EXPORT_NHML | GF_EXPORT_NHML_FULL;
				dumper.print_stats_graph = fs_dump_flags;
				gf_media_export(&dumper);
			}
		}
#else
		return GF_NOT_SUPPORTED;
#endif /*GPAC_DISABLE_MEDIA_EXPORT*/
		gf_isom_delete(the_file);
		fprintf(dump, "</Tracks>\n");
		fprintf(dump, "</ISOBaseMediaFileTrace>\n");
	}
	if (do_close) gf_fclose(dump);
	return e;
}
#endif


static char *format_duration(u64 dur, u32 timescale, char *szDur)
{
	u32 h, m, s, ms;
	if ((dur==(u64) -1) || (dur==(u32) -1))  {
		strcpy(szDur, "Unknown");
		return szDur;
	}
	dur = (u64) (( ((Double) (s64) dur)/timescale)*1000);
	h = (u32) (dur / 3600000);
	m = (u32) (dur/ 60000) - h*60;
	s = (u32) (dur/1000) - h*3600 - m*60;
	ms = (u32) (dur) - h*3600000 - m*60000 - s*1000;
	if (h<=24) {
		sprintf(szDur, "%02d:%02d:%02d.%03d", h, m, s, ms);
	} else {
		u32 d = (u32) (dur / 3600000 / 24);
		h = (u32) (dur/3600000)-24*d;
		if (d<=365) {
			sprintf(szDur, "%d Days, %02d:%02d:%02d.%03d", d, h, m, s, ms);
		} else {
			u32 y=0;
			while (d>365) {
				y++;
				d-=365;
				if (y%4) d--;
			}
			sprintf(szDur, "%d Years %d Days, %02d:%02d:%02d.%03d", y, d, h, m, s, ms);
		}

	}
	return szDur;
}

static char *format_date(u64 time, char *szTime)
{
	time_t now;
	if (!time) {
		strcpy(szTime, "UNKNOWN DATE");
	} else {
		time -= 2082844800;
		now = (u32) time;
		sprintf(szTime, "GMT %s", asctime(gf_gmtime(&now)) );
	}
	return szTime;
}

void print_udta(GF_ISOFile *file, u32 track_number, Bool has_itags)
{
	u32 i, count;

	count =  gf_isom_get_udta_count(file, track_number);
	if (!count) return;

	if (has_itags) {
		for (i=0; i<count; i++) {
			u32 type;
			bin128 uuid;
			gf_isom_get_udta_type(file, track_number, i+1, &type, &uuid);
			if (type == GF_ISOM_BOX_TYPE_META) {
				count--;
				break;
			}
		}
		if (!count) return;
	}

	fprintf(stderr, "%d UDTA types: ", count);

	for (i=0; i<count; i++) {
		u32 j, type, nb_items, first=GF_TRUE;
		bin128 uuid;
		gf_isom_get_udta_type(file, track_number, i+1, &type, &uuid);
		nb_items = gf_isom_get_user_data_count(file, track_number, type, uuid);
		fprintf(stderr, "%s (%d) ", gf_4cc_to_str(type), nb_items);
		for (j=0; j<nb_items; j++) {
			u8 *udta=NULL;
			u32 udta_size;
			gf_isom_get_user_data(file, track_number, type, uuid, j+1, &udta, &udta_size);
			if (!udta) continue;
			if (udta_size && gf_utf8_is_legal(udta, udta_size)) {
				u32 idx;
				if (first) {
					fprintf(stderr, "\n");
					first = GF_FALSE;
				}
				fprintf(stderr, "\t");
				for (idx=0; idx<udta_size; idx++) {
					if (!udta[idx]) break;
					fprintf(stderr, "%c", udta[idx]);
				}
				fprintf(stderr, "\n");
			}
			gf_free(udta);
		}
	}
	fprintf(stderr, "\n");
}

GF_Err dump_isom_udta(GF_ISOFile *file, char *inName, Bool is_final_name, u32 dump_udta_type, u32 dump_udta_track)
{
	u8 *data;
	FILE *t;
	bin128 uuid;
	u32 count, res;
	GF_Err e;

	memset(uuid, 0, 16);
	count = gf_isom_get_user_data_count(file, dump_udta_track, dump_udta_type, uuid);
	if (!count) {
		M4_LOG(GF_LOG_ERROR, ("No UDTA for type %s found\n", gf_4cc_to_str(dump_udta_type) ));
		return GF_NOT_FOUND;
	}

	data = NULL;
	count = 0;
	e = gf_isom_get_user_data(file, dump_udta_track, dump_udta_type, uuid, 0, &data, &count);
	if (e) {
		M4_LOG(GF_LOG_ERROR, ("Error dumping UDTA %s: %s\n", gf_4cc_to_str(dump_udta_type), gf_error_to_string(e) ));
		return e;
	}
	if (inName) {
		char szName[1024];
		if (is_final_name)
			strcpy(szName, inName);
		else
			sprintf(szName, "%s_%s.udta", inName, gf_4cc_to_str(dump_udta_type) );

		t = gf_fopen(szName, "wb");
		if (!t) {
			gf_free(data);
			M4_LOG(GF_LOG_ERROR, ("Cannot open file %s\n", szName ));
			return GF_IO_ERR;
		}
	} else {
		t = stdout;
	}
	res = (u32) gf_fwrite(data+8, count-8, t);
	if (inName) gf_fclose(t);
	gf_free(data);
	if (count-8 != res) {
		M4_LOG(GF_LOG_ERROR, ("Error writing udta to file\n"));
		return GF_IO_ERR;
	}
	return GF_OK;
}


GF_Err dump_isom_chapters(GF_ISOFile *file, char *inName, Bool is_final_name, u32 dump_mode)
{
	FILE *t;
	u32 i, count;
	u32 chap_tk = 0;
	count = gf_isom_get_chapter_count(file, 0);

	if (dump_mode==2) dump_mode = GF_TEXTDUMPTYPE_OGG_CHAP;
	else if (dump_mode==3) dump_mode = GF_TEXTDUMPTYPE_ZOOM_CHAP;
	else dump_mode = GF_TEXTDUMPTYPE_TTXT_CHAP;

	if (!count) {
		for (i=0; i<gf_isom_get_track_count(file); i++) {
			if (gf_isom_get_reference_count(file, i+1, GF_ISOM_REF_CHAP)) {
				GF_Err e = gf_isom_get_reference(file, i+1, GF_ISOM_REF_CHAP, 1, &chap_tk);
				if (!e) break;
			}
		}
		if (!chap_tk) {
			M4_LOG(GF_LOG_WARNING, ("No chapters or chapters track found in file\n"));
			return GF_OK;
		}

		fprintf(stderr, "Dumping chapter track %d\n", chap_tk);
		dump_isom_timed_text(file, gf_isom_get_track_id(file, chap_tk), inName, is_final_name, GF_FALSE, dump_mode);
		return GF_OK;

	}
	if (inName) {
		char szName[1024];
		strcpy(szName, inName);
		if (!is_final_name) {
			if (dump_mode==GF_TEXTDUMPTYPE_OGG_CHAP) {
				strcat(szName, ".txt");
			} else if (dump_mode==GF_TEXTDUMPTYPE_ZOOM_CHAP) {
				strcat(szName, ".txt");
			} else {
				strcat(szName, ".ttxt");
			}
		}
		t = gf_fopen(szName, "wt");
		if (!t) return GF_IO_ERR;
	} else {
		t = stdout;
	}

	if (dump_mode==GF_TEXTDUMPTYPE_TTXT_CHAP) {
		fprintf(t, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
		fprintf(t, "<TextStream version=\"1.1\">\n");
		fprintf(t, "<TextStreamHeader width=\"0\" height=\"0\" layer=\"0\" translation_x=\"0\" translation_y=\"0\">\n");
		fprintf(t, "<TextSampleDescription horizontalJustification=\"left\" backColor=\"0 0 0\" scroll=\"None\"/>\n");
		fprintf(t, "</TextStreamHeader>\n");
	}

	for (i=0; i<count; i++) {
		char szDur[20];
		u64 chapter_time;
		const char *name;
		gf_isom_get_chapter(file, 0, i+1, &chapter_time, &name);
		if (dump_mode==GF_TEXTDUMPTYPE_OGG_CHAP) {
			fprintf(t, "CHAPTER%02d=%s\n", i+1, format_duration(chapter_time, 1000, szDur));
			fprintf(t, "CHAPTER%02dNAME=%s\n", i+1, name);
		} else if (dump_mode==GF_TEXTDUMPTYPE_ZOOM_CHAP) {
			chapter_time /= 1000;
			fprintf(t, "AddChapterBySecond("LLD",%s)\n", chapter_time, name);
		} else {
			fprintf(t, "<TextSample sampleTime=\"%s\" sampleDescriptionIndex=\"1\" xml:space=\"preserve\">%s</TextSample>\n"
				, format_duration(chapter_time, 1000, szDur), name);
		}
	}
	if (dump_mode==GF_TEXTDUMPTYPE_TTXT_CHAP) {
		fprintf(t, "</TextStream>\n");
	}
	if (inName) gf_fclose(t);
	return GF_OK;
}


static void dump_key_info(const u8 *key_info, u32 key_info_size, Bool is_protected)
{
	if (!key_info) return;
	u32 j, k, kpos=3;
	u32 nb_keys = 1;
	if (key_info[0]) {
		nb_keys = key_info[1];
		nb_keys <<= 8;
		nb_keys |= key_info[2];
	}
	for (k=0; k<nb_keys; k++) {
		u8 constant_iv_size=0;
		u8 iv_size=key_info[kpos+1];
		fprintf(stderr, "\t\tKID");
		if (nb_keys>1)
			fprintf(stderr, "%d", k+1);
		fprintf(stderr, " ");
		for (j=0; j<16; j++) fprintf(stderr, "%02X", key_info[kpos+1+j]);
		kpos+=17;
		if (!iv_size && is_protected) {
			constant_iv_size = key_info[1];
			kpos += 1 + constant_iv_size;
		}
		fprintf(stderr, " - %sIV size %d \n", constant_iv_size ? "const " : "", constant_iv_size ? constant_iv_size : iv_size);
	}
}

static void DumpMetaItem(GF_ISOFile *file, Bool root_meta, u32 tk_num, char *name)
{
	char szInd[2];
	u32 i, count, primary_id;
	u32 meta_type = gf_isom_get_meta_type(file, root_meta, tk_num);
	if (name[0]=='\t') {
		szInd[0] = '\t';
		szInd[1] = 0;
	} else {
		szInd[0] = 0;
	}

	count = gf_isom_get_meta_item_count(file, root_meta, tk_num);
	primary_id = gf_isom_get_meta_primary_item_id(file, root_meta, tk_num);
	fprintf(stderr, "%s type: \"%s\" - %d resource item(s)\n", name, meta_type ? gf_4cc_to_str(meta_type) : "undefined", (count+(primary_id>0)));
	switch (gf_isom_has_meta_xml(file, root_meta, tk_num)) {
	case 1:
		fprintf(stderr, "%sMeta has XML resource\n", szInd);
		break;
	case 2:
		fprintf(stderr, "%sMeta has BinaryXML resource\n", szInd);
		break;
	}
	if (primary_id) {
		fprintf(stderr, "%sPrimary Item - ID %d\n", szInd, primary_id);
	}
	for (i=0; i<count; i++) {
		const char *it_name, *mime, *enc, *url, *urn;
		Bool self_ref;
		u32 ID;
		u32 it_type, cenc_scheme, cenc_version;
		GF_Err e = gf_isom_get_meta_item_info(file, root_meta, tk_num, i+1, &ID, &it_type, &cenc_scheme, &cenc_version, &self_ref, &it_name, &mime, &enc, &url, &urn);
		if (e) {
			fprintf(stderr, "%sItem #%d fetch info error: %s\n", szInd, i+1, gf_error_to_string(e) );
			continue;
		}
		fprintf(stderr, "%sItem #%d: ID %d type %s", szInd, i+1, ID, gf_4cc_to_str(it_type));
		if (self_ref) fprintf(stderr, " Self-Reference");
		else if (it_name && it_name[0]) fprintf(stderr, " Name \"%s\"", it_name);
		if (mime) fprintf(stderr, " MIME: \"%s\"", mime);
		if (enc) fprintf(stderr, " ContentEncoding: \"%s\"", enc);

		if (meta_type == GF_META_ITEM_TYPE_PICT) {
			GF_ImageItemProperties img_props;
			e = gf_isom_get_meta_image_props(file, root_meta, tk_num, ID, &img_props);
			if (e) {
				fprintf(stderr, " invalid image properties !");
			} else {
				u32 j;
				Bool chan_diff = 0;
				if (img_props.width && img_props.height) {
					fprintf(stderr, " size %ux%u", img_props.width, img_props.height);
				}
				if (img_props.hSpacing && img_props.vSpacing) {
					fprintf(stderr, " SAR %u/%u", img_props.hSpacing, img_props.vSpacing);
				}
				if (img_props.num_channels) {
					fprintf(stderr, " %d channel%s (", img_props.num_channels, (img_props.num_channels>1) ? "s" : "");
					for (j=1; j<img_props.num_channels; j++) {
						if (img_props.bits_per_channel[0] != img_props.bits_per_channel[j])
							chan_diff = 1;
					}
					if (chan_diff) {
						for (j=0; j<img_props.num_channels; j++) {
							if (j) fprintf(stderr, ",");
							fprintf(stderr, "%d", img_props.bits_per_channel[j]);
						}
					} else {
						fprintf(stderr, "%d", img_props.bits_per_channel[0]);
					}
					fprintf(stderr, " bpc)");
				}
				if (img_props.hOffset || img_props.vOffset)
					fprintf(stderr, " Offset %ux%u", img_props.hOffset, img_props.vOffset);
				if (img_props.alpha) fprintf(stderr, " Alpha");
				if (img_props.hidden) fprintf(stderr, " Hidden");
				if (img_props.angle) fprintf(stderr, " Rotate %d", img_props.angle);
				if (img_props.mirror) fprintf(stderr, " Mirror %d", img_props.mirror);
				if (img_props.clap_hden || img_props.clap_wden)
					fprintf(stderr, " Clap %d/%d,%d/%d,%d/%d,%d/%d", img_props.clap_wnum, img_props.clap_wden, img_props.clap_hnum, img_props.clap_hden, img_props.clap_honum, img_props.clap_hoden, img_props.clap_vonum, img_props.clap_voden);
			}
		}

		if (cenc_scheme) {
			Bool is_protected;
			u8 skip_byte_block, crypt_byte_block;
			const u8 *key_info;
			u32 key_info_size;
			fprintf(stderr, " - Protection scheme: %s v0x%08X", gf_4cc_to_str(cenc_scheme), cenc_version);

			gf_isom_extract_meta_item_get_cenc_info(file, root_meta, tk_num, ID, &is_protected, &skip_byte_block, &crypt_byte_block, &key_info, &key_info_size, NULL, NULL, NULL, NULL);
			if (skip_byte_block && crypt_byte_block)
				fprintf(stderr, " - Pattern %d:%d", skip_byte_block, crypt_byte_block);
			fprintf(stderr, "\n");
			dump_key_info(key_info, key_info_size, is_protected);
		}
		fprintf(stderr, "\n");
		if (url) fprintf(stderr, "%sURL: %s\n", szInd, url);
		if (urn) fprintf(stderr, "%sURN: %s\n", szInd, urn);

	}
}


static void print_config_hash(GF_List *xps_array, char *szName)
{
	u32 i, j;
	u8 hash[20];
	for (i=0; i<gf_list_count(xps_array); i++) {
		GF_NALUFFParam *slc = gf_list_get(xps_array, i);
		gf_sha1_csum((u8 *) slc->data, slc->size, hash);
		fprintf(stderr, "\t%s#%d hash: ", szName, i+1);
		for (j=0; j<20; j++) fprintf(stderr, "%02X", hash[j]);
		fprintf(stderr, "\n");
	}
}

void dump_hevc_track_info(GF_ISOFile *file, u32 trackNum, GF_HEVCConfig *hevccfg
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_HEVC)
	, HEVCState *hevc_state
#endif /*GPAC_DISABLE_AV_PARSERS  && defined(GPAC_DISABLE_HEVC)*/
	)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_HEVC)
	u32 idx;
#endif
	u32 k;
	Bool non_hevc_base_layer=GF_FALSE;
	fprintf(stderr, "\t%s Info:", hevccfg->is_lhvc ? "LHVC" : "HEVC");
	if (!hevccfg->is_lhvc)
		fprintf(stderr, " Profile %s @ Level %g - Chroma Format %s\n", gf_hevc_get_profile_name(hevccfg->profile_idc), ((Double)hevccfg->level_idc) / 30.0, gf_avc_hevc_get_chroma_format_name(hevccfg->chromaFormat));
	fprintf(stderr, "\n");
	fprintf(stderr, "\tNAL Unit length bits: %d", 8*hevccfg->nal_unit_size);
	if (!hevccfg->is_lhvc)
		fprintf(stderr, " - general profile compatibility 0x%08X\n", hevccfg->general_profile_compatibility_flags);
	fprintf(stderr, "\n");
	fprintf(stderr, "\tParameter Sets: ");
	for (k=0; k<gf_list_count(hevccfg->param_array); k++) {
		GF_NALUFFParamArray *ar=gf_list_get(hevccfg->param_array, k);
		if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
			fprintf(stderr, "%d SPS ", gf_list_count(ar->nalus));
		}
		if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
			fprintf(stderr, "%d PPS ", gf_list_count(ar->nalus));
		}
		if (ar->type==GF_HEVC_NALU_VID_PARAM) {
			fprintf(stderr, "%d VPS ", gf_list_count(ar->nalus));

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_HEVC)
			for (idx=0; idx<gf_list_count(ar->nalus); idx++) {
				GF_NALUFFParam *vps = gf_list_get(ar->nalus, idx);
				s32 ps_idx=gf_hevc_read_vps(vps->data, vps->size, hevc_state);
				if (hevccfg->is_lhvc && (ps_idx>=0)) {
					non_hevc_base_layer = ! hevc_state->vps[ps_idx].base_layer_internal_flag;
				}
			}
#endif

		}
	}

	fprintf(stderr, "\n");
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_HEVC)
	for (k=0; k<gf_list_count(hevccfg->param_array); k++) {
		GF_NALUFFParamArray *ar=gf_list_get(hevccfg->param_array, k);
		u32 width, height;
		s32 par_n, par_d;

		if (ar->type !=GF_HEVC_NALU_SEQ_PARAM) continue;
		for (idx=0; idx<gf_list_count(ar->nalus); idx++) {
			GF_Err e;
			GF_NALUFFParam *sps = gf_list_get(ar->nalus, idx);
			par_n = par_d = -1;
			e = gf_hevc_get_sps_info_with_state(hevc_state, sps->data, sps->size, NULL, &width, &height, &par_n, &par_d);
			if (e==GF_OK) {
				fprintf(stderr, "\tSPS resolution %dx%d", width, height);
				if ((par_n>0) && (par_d>0)) {
					u32 tw, th;
					gf_isom_get_track_layout_info(file, trackNum, &tw, &th, NULL, NULL, NULL);
					fprintf(stderr, " - Pixel Aspect Ratio %d:%d - Indicated track size %d x %d", par_n, par_d, tw, th);
				}
				fprintf(stderr, "\n");
			} else {
				M4_LOG(GF_LOG_ERROR, ("Failed to read SPS: %s\n\n", gf_error_to_string(e) ));
			}
		}
	}
#endif
	if (!hevccfg->is_lhvc)
		fprintf(stderr, "\tBit Depth luma %d - Chroma %d - %d temporal layers\n", hevccfg->luma_bit_depth, hevccfg->chroma_bit_depth, hevccfg->numTemporalLayers);
	else
		fprintf(stderr, "\t%d temporal layers\n", hevccfg->numTemporalLayers);
	if (hevccfg->is_lhvc) {
		fprintf(stderr, "\t%sHEVC base layer - Complete representation %d\n", non_hevc_base_layer ? "Non-" : "", hevccfg->complete_representation);
	}

	for (k=0; k<gf_list_count(hevccfg->param_array); k++) {
		GF_NALUFFParamArray *ar=gf_list_get(hevccfg->param_array, k);
		if (ar->type==GF_HEVC_NALU_SEQ_PARAM) print_config_hash(ar->nalus, "SPS");
		else if (ar->type==GF_HEVC_NALU_PIC_PARAM) print_config_hash(ar->nalus, "PPS");
		else if (ar->type==GF_HEVC_NALU_VID_PARAM) print_config_hash(ar->nalus, "VPS");
	}
}

void dump_vvc_track_info(GF_ISOFile *file, u32 trackNum, GF_VVCConfig *vvccfg
#if !defined(GPAC_DISABLE_AV_PARSERS)
	, VVCState *vvc_state
#endif /*GPAC_DISABLE_AV_PARSERS  && defined(GPAC_DISABLE_HEVC)*/
	)
{
#if !defined(GPAC_DISABLE_AV_PARSERS)
	u32 idx;
#endif
	u32 k;
	fprintf(stderr, "\tVVC Info:");

	fprintf(stderr, " Profile %d @ Level %d - Chroma Format %s\n", vvccfg->general_profile_idc, vvccfg->general_level_idc, vvccfg->chromaformat_plus_one ? gf_avc_hevc_get_chroma_format_name(vvccfg->chromaformat_plus_one-1) : "n/a");
	fprintf(stderr, "\n");
	fprintf(stderr, "\tNAL Unit length bits: %d", 8*vvccfg->nal_unit_size);
	if (vvccfg->general_constraint_info && vvccfg->num_constraint_info && vvccfg->general_constraint_info[0]) {
		fprintf(stderr, " - general constraint info 0x");
		for (idx=0; idx<vvccfg->num_constraint_info; idx++) {
			fprintf(stderr, "%02X", vvccfg->general_constraint_info[idx]);
		}
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "\tParameter Sets: ");
	for (k=0; k<gf_list_count(vvccfg->param_array); k++) {
		GF_NALUFFParamArray *ar=gf_list_get(vvccfg->param_array, k);
		if (ar->type==GF_VVC_NALU_SEQ_PARAM) {
			fprintf(stderr, "%d SPS ", gf_list_count(ar->nalus));
		}
		if (ar->type==GF_VVC_NALU_PIC_PARAM) {
			fprintf(stderr, "%d PPS ", gf_list_count(ar->nalus));
		}
		if (ar->type==GF_VVC_NALU_VID_PARAM) {
			fprintf(stderr, "%d VPS ", gf_list_count(ar->nalus));

#if !defined(GPAC_DISABLE_AV_PARSERS) && 0 //TODO
			for (idx=0; idx<gf_list_count(ar->nalus); idx++) {
				GF_NALUFFParam *vps = gf_list_get(ar->nalus, idx);
				s32 ps_idx=gf_hevc_read_vps(vps->data, vps->size, hevc_state);
				if (hevccfg->is_lhvc && (ps_idx>=0)) {
					non_hevc_base_layer = ! hevc_state->vps[ps_idx].base_layer_internal_flag;
				}
			}
#endif

		}
	}

	fprintf(stderr, "\n");
#if !defined(GPAC_DISABLE_AV_PARSERS) && 0 //TODO
	for (k=0; k<gf_list_count(vvccfg->param_array); k++) {
		GF_NALUFFParamArray *ar=gf_list_get(vvccfg->param_array, k);
		u32 width, height;
		s32 par_n, par_d;

		if (ar->type !=GF_VVC_NALU_SEQ_PARAM) continue;
		for (idx=0; idx<gf_list_count(ar->nalus); idx++) {
			GF_Err e;
			GF_NALUFFParam *sps = gf_list_get(ar->nalus, idx);
			par_n = par_d = -1;
			e = gf_vvc_get_sps_info_with_state(vvc_state, sps->data, sps->size, NULL, &width, &height, &par_n, &par_d);
			if (e==GF_OK) {
				fprintf(stderr, "\tSPS resolution %dx%d", width, height);
				if ((par_n>0) && (par_d>0)) {
					u32 tw, th;
					gf_isom_get_track_layout_info(file, trackNum, &tw, &th, NULL, NULL, NULL);
					fprintf(stderr, " - Pixel Aspect Ratio %d:%d - Indicated track size %d x %d", par_n, par_d, tw, th);
				}
				fprintf(stderr, "\n");
			} else {
				M4_LOG(GF_LOG_ERROR, ("\nFailed to read SPS: %s\n\n", gf_error_to_string(e) ));
			}
		}
	}
#endif
	fprintf(stderr, "\tBit Depth %d - %d temporal layers\n", vvccfg->bit_depth_plus_one-1, vvccfg->numTemporalLayers);

	for (k=0; k<gf_list_count(vvccfg->param_array); k++) {
		GF_NALUFFParamArray *ar=gf_list_get(vvccfg->param_array, k);
		if (ar->type==GF_VVC_NALU_SEQ_PARAM) print_config_hash(ar->nalus, "SPS");
		else if (ar->type==GF_VVC_NALU_PIC_PARAM) print_config_hash(ar->nalus, "PPS");
		else if (ar->type==GF_VVC_NALU_VID_PARAM) print_config_hash(ar->nalus, "VPS");
	}
}

void gf_inspect_format_timecode(const u8 *data, u32 size, u32 tmcd_flags, u32 tc_num, u32 tc_den, u32 tmcd_fpt, char szFmt[100]);

void DumpTrackInfo(GF_ISOFile *file, GF_ISOTrackID trackID, Bool full_dump, Bool is_track_num, Bool dump_m4sys)
{
	char szCodec[RFC6381_CODEC_NAME_SIZE_MAX];
	Double scale, max_rate, rate;
	Bool is_od_track = 0;
	u32 trackNum, i, j, ts, mtype, msub_type, timescale, sr, nb_ch, count, alt_group, nb_groups, nb_edits, cdur, csize, bps, pfmt, codecid;
	u64 time_slice, dur, size;
	s32 cts_shift;
	GF_ESD *esd;
	char szDur[50];
	char *lang;

	if (!is_track_num) {
		trackNum = gf_isom_get_track_by_id(file, trackID);
	} else {
		trackNum = trackID;
		trackID = gf_isom_get_track_id(file, trackNum);
	}
	if (!trackNum) {
		M4_LOG(GF_LOG_ERROR, ("No track with ID %d found\n", trackID));
		return;
	}

	timescale = gf_isom_get_media_timescale(file, trackNum);
	fprintf(stderr, "# Track %d Info - ID %d - TimeScale %d\n", trackNum, trackID, timescale);

	dur = gf_isom_get_media_original_duration(file, trackNum);
	size = gf_isom_get_media_duration(file, trackNum);
	fprintf(stderr, "Media Duration %s ", format_duration(dur, timescale, szDur));
	if (dur != size)
		fprintf(stderr, " (recomputed %s)", format_duration(size, timescale, szDur));
	fprintf(stderr, "\n");

	if (gf_isom_check_data_reference(file, trackNum, 1) != GF_OK) {
		M4_LOG(GF_LOG_WARNING, ("Track uses external data reference not supported by GPAC!\n"));
	}

	nb_edits = gf_isom_get_edits_count(file, trackNum);
	if (nb_edits)
		fprintf(stderr, "Track has %d edits: track duration is %s\n", nb_edits, format_duration(gf_isom_get_track_duration(file, trackNum), gf_isom_get_timescale(file), szDur));

	cts_shift = gf_isom_get_composition_offset_shift(file, trackNum);
	if (cts_shift)
		fprintf(stderr, "Track composition offset shift (negative CTS offset): %d\n", cts_shift);

	if (gf_isom_is_track_in_root_od(file, trackNum) ) fprintf(stderr, "Track is present in Root OD\n");
	if (!gf_isom_is_track_enabled(file, trackNum))  fprintf(stderr, "Track is disabled\n");
	gf_isom_get_media_language(file, trackNum, &lang);
	fprintf(stderr, "Media Info: Language \"%s (%s)\" - ", GetLanguage(lang), lang );
	gf_free(lang);
	mtype = gf_isom_get_media_type(file, trackNum);
	fprintf(stderr, "Type \"%s:", gf_4cc_to_str(mtype));
	msub_type = gf_isom_get_mpeg4_subtype(file, trackNum, 1);
	if (!msub_type) msub_type = gf_isom_get_media_subtype(file, trackNum, 1);
	fprintf(stderr, "%s\" - %d samples\n", gf_4cc_to_str(msub_type), gf_isom_get_sample_count(file, trackNum));

	pfmt = gf_pixel_fmt_from_qt_type(msub_type);
	codecid = gf_codec_id_from_isobmf(msub_type);

	count = gf_isom_get_track_kind_count(file, trackNum);
	for (i = 0; i < count; i++) {
		char *kind_scheme, *kind_value;
		gf_isom_get_track_kind(file, trackNum, i, &kind_scheme, &kind_value);
		fprintf(stderr, "Kind: %s - %s\n", kind_scheme ? kind_scheme : "null", kind_value ? kind_value : "null");
		if (kind_scheme) gf_free(kind_scheme);
		if (kind_value) gf_free(kind_value);
	}

	if (gf_isom_is_track_fragmented(file, trackID) ) {
		u32 defaultDuration, defaultSize, defaultDescriptionIndex, defaultRandomAccess;
		u8 defaultPadding;
		u16 defaultDegradationPriority;
		u32 frag_samples;
		u64 frag_duration;
		gf_isom_get_fragmented_samples_info(file, trackID, &frag_samples, &frag_duration);
		fprintf(stderr, "Fragmented track: %d samples - Media Duration %s\n", frag_samples, format_duration(frag_duration, timescale, szDur));

		gf_isom_get_fragment_defaults(file, trackNum, &defaultDuration, &defaultSize, &defaultDescriptionIndex, &defaultRandomAccess, &defaultPadding, &defaultDegradationPriority);

		fprintf(stderr, "Fragment sample defaults: duration %d size %d stsd %d sync %d padding %d degradation_priority %d\n",
				defaultDuration, defaultSize, defaultDescriptionIndex, defaultRandomAccess,
				(u32) defaultPadding, (u32) defaultDegradationPriority
		);
	}

	if (!gf_isom_is_self_contained(file, trackNum, 1)) {
		const char *url, *urn;
		gf_isom_get_data_reference(file, trackNum, 1, &url, &urn);
		fprintf(stderr, "Media Data Location: %s\n", url ? url : urn);
	}

	if (full_dump) {
		const char *handler_name;
		gf_isom_get_handler_name(file, trackNum, &handler_name);
		fprintf(stderr, "Handler name: %s\n", handler_name);
	}

	print_udta(file, trackNum, GF_FALSE);

	if (gf_isom_is_video_handler_type(mtype) ) {
		s32 tx, ty;
		u32 w, h;
		u16 bit_depth;

		gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
		gf_isom_get_visual_bit_depth(file, trackNum, 1, &bit_depth);
		fprintf(stderr, "Visual Sample Entry Info: width=%d height=%d (depth=%d bits)\n", w, h, (int)bit_depth);

		gf_isom_get_track_layout_info(file, trackNum, &w, &h, &tx, &ty, NULL);
		fprintf(stderr, "Visual Track layout: x=%d y=%d width=%d height=%d\n", tx, ty, w, h);
	}

	gf_isom_get_audio_info(file, trackNum, 1, &sr, &nb_ch, &bps);
	gf_isom_set_nalu_extract_mode(file, trackNum, GF_ISOM_NALU_EXTRACT_INSPECT);

	msub_type = gf_isom_get_media_subtype(file, trackNum, 1);
	if (msub_type==GF_ISOM_SUBTYPE_MPEG4_CRYP)
		gf_isom_get_original_format_type(file, trackNum, 1, &msub_type);

	if ((msub_type==GF_ISOM_SUBTYPE_MPEG4)
	        || (msub_type==GF_ISOM_SUBTYPE_AVC_H264)
	        || (msub_type==GF_ISOM_SUBTYPE_AVC2_H264)
	        || (msub_type==GF_ISOM_SUBTYPE_AVC3_H264)
	        || (msub_type==GF_ISOM_SUBTYPE_AVC4_H264)
	        || (msub_type==GF_ISOM_SUBTYPE_SVC_H264)
	        || (msub_type==GF_ISOM_SUBTYPE_MVC_H264)
	        || (msub_type==GF_ISOM_SUBTYPE_LSR1)
	        || (msub_type==GF_ISOM_SUBTYPE_HVC1)
	        || (msub_type==GF_ISOM_SUBTYPE_HEV1)
	        || (msub_type==GF_ISOM_SUBTYPE_HVC2)
	        || (msub_type==GF_ISOM_SUBTYPE_HEV2)
	        || (msub_type==GF_ISOM_SUBTYPE_LHV1)
	        || (msub_type==GF_ISOM_SUBTYPE_LHE1)
	        || (msub_type==GF_ISOM_SUBTYPE_HVT1)
	   )  {
		esd = gf_isom_get_esd(file, trackNum, 1);
		if (!esd || !esd->decoderConfig) {
			M4_LOG(GF_LOG_WARNING, ("WARNING: Broken MPEG-4 Track\n"));
			if (esd) gf_odf_desc_del((GF_Descriptor *)esd);
		} else {
			const char *st = gf_stream_type_name(esd->decoderConfig->streamType);
			if (dump_m4sys) {
				if (st) {
					fprintf(stderr, "MPEG-4 Config%s%s Stream - ObjectTypeIndication 0x%02x\n",
							full_dump ? "\n\t" : ": ", st, esd->decoderConfig->objectTypeIndication);
				} else {
					fprintf(stderr, "MPEG-4 Config%sStream Type 0x%02x - ObjectTypeIndication 0x%02x\n",
							full_dump ? "\n\t" : ": ", esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
				}
			}

			if (esd->decoderConfig->streamType==GF_STREAM_OD)
				is_od_track=1;

			if (esd->decoderConfig->streamType==GF_STREAM_VISUAL) {
				u32 w, h;
				u16 rvc_predef;
				w = h = 0;
				if (esd->decoderConfig->objectTypeIndication==GF_CODECID_MPEG4_PART2) {
#ifndef GPAC_DISABLE_AV_PARSERS
					if (!esd->decoderConfig->decoderSpecificInfo) {
#else
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					fprintf(stderr, "MPEG-4 Visual Size %d x %d\n", w, h);
#endif
						M4_LOG(GF_LOG_WARNING, ("Non-compliant MPEG-4 Visual track: video_object_layer infos not found in sample description\n"));
#ifndef GPAC_DISABLE_AV_PARSERS
					} else {
						GF_M4VDecSpecInfo dsi;
						gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
						if (full_dump) fprintf(stderr, "\t");
						w = dsi.width;
						h = dsi.height;
						fprintf(stderr, "MPEG-4 Visual Size %d x %d - %s\n", w, h, gf_m4v_get_profile_name(dsi.VideoPL));
						if (dsi.par_den && dsi.par_num) {
							u32 tw, th;
							gf_isom_get_track_layout_info(file, trackNum, &tw, &th, NULL, NULL, NULL);
							fprintf(stderr, "Pixel Aspect Ratio %d:%d - Indicated track size %d x %d\n", dsi.par_num, dsi.par_den, tw, th);
						}
					}
#endif
				} else if (gf_isom_get_avc_svc_type(file, trackNum, 1) != GF_ISOM_AVCTYPE_NONE) {
					GF_AVCConfig *avccfg, *svccfg, *mvccfg;

					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					if (full_dump) fprintf(stderr, "\t");
					fprintf(stderr, "AVC/H264 Video - Visual Size %d x %d\n", w, h);

					avccfg = gf_isom_avc_config_get(file, trackNum, 1);
					svccfg = gf_isom_svc_config_get(file, trackNum, 1);
					mvccfg = gf_isom_mvc_config_get(file, trackNum, 1);
					if (!avccfg && !svccfg && !mvccfg) {
						M4_LOG(GF_LOG_ERROR, ("\tNon-compliant AVC track: SPS/PPS not found in sample description\n"));
					} else if (avccfg) {
						fprintf(stderr, "\tAVC Info: %d SPS - %d PPS", gf_list_count(avccfg->sequenceParameterSets) , gf_list_count(avccfg->pictureParameterSets) );
						fprintf(stderr, " - Profile %s @ Level %g\n", gf_avc_get_profile_name(avccfg->AVCProfileIndication), ((Double)avccfg->AVCLevelIndication)/10.0 );
						fprintf(stderr, "\tNAL Unit length bits: %d\n", 8*avccfg->nal_unit_size);

#ifndef GPAC_DISABLE_AV_PARSERS
						for (i=0; i<gf_list_count(avccfg->sequenceParameterSets); i++) {
							s32 par_n, par_d;
							GF_NALUFFParam *slc = gf_list_get(avccfg->sequenceParameterSets, i);
							gf_avc_get_sps_info(slc->data, slc->size, NULL, NULL, NULL, &par_n, &par_d);
							if ((par_n>0) && (par_d>0)) {
								u32 tw, th;
								gf_isom_get_track_layout_info(file, trackNum, &tw, &th, NULL, NULL, NULL);
								fprintf(stderr, "\tPixel Aspect Ratio %d:%d - Indicated track size %d x %d\n", par_n, par_d, tw, th);
							}
							if (!full_dump) break;
						}
#endif

						if (avccfg->chroma_bit_depth) {
							fprintf(stderr, "\tChroma format %s - Luma bit depth %d - chroma bit depth %d\n", gf_avc_hevc_get_chroma_format_name(avccfg->chroma_format), avccfg->luma_bit_depth, avccfg->chroma_bit_depth);
						}

						print_config_hash(avccfg->sequenceParameterSets, "SPS");
						print_config_hash(avccfg->pictureParameterSets, "PPS");

						gf_odf_avc_cfg_del(avccfg);
					}
					if (svccfg) {
						fprintf(stderr, "\n\tSVC Info: %d SPS - %d PPS - Profile %s @ Level %g\n", gf_list_count(svccfg->sequenceParameterSets) , gf_list_count(svccfg->pictureParameterSets), gf_avc_get_profile_name(svccfg->AVCProfileIndication), ((Double)svccfg->AVCLevelIndication)/10.0 );
						fprintf(stderr, "\tSVC NAL Unit length bits: %d\n", 8*svccfg->nal_unit_size);
#ifndef GPAC_DISABLE_AV_PARSERS
						for (i=0; i<gf_list_count(svccfg->sequenceParameterSets); i++) {
							GF_NALUFFParam *slc = gf_list_get(svccfg->sequenceParameterSets, i);
							if (slc) {
								s32 par_n, par_d;
								u32 s_w, s_h, sps_id;
								gf_avc_get_sps_info(slc->data, slc->size, &sps_id, &s_w, &s_h, &par_n, &par_d);
								fprintf(stderr, "\t\tSPS ID %d - Visual Size %d x %d\n", sps_id, s_w, s_h);
								if ((par_n>0) && (par_d>0)) {
									u32 tw, th;
									gf_isom_get_track_layout_info(file, trackNum, &tw, &th, NULL, NULL, NULL);
									fprintf(stderr, "\tPixel Aspect Ratio %d:%d - Indicated track size %d x %d\n", par_n, par_d, tw, th);
								}
							}
						}
#endif
						print_config_hash(svccfg->sequenceParameterSets, "SPS");
						print_config_hash(svccfg->pictureParameterSets, "PPS");
						print_config_hash(svccfg->sequenceParameterSetExtensions, "SPSEx");

						gf_odf_avc_cfg_del(svccfg);
					}

					if (mvccfg) {
						fprintf(stderr, "\n\tMVC Info: %d SPS - %d PPS - Profile %s @ Level %g\n", gf_list_count(mvccfg->sequenceParameterSets) , gf_list_count(mvccfg->pictureParameterSets), gf_avc_get_profile_name(mvccfg->AVCProfileIndication), ((Double)mvccfg->AVCLevelIndication)/10.0 );
						fprintf(stderr, "\tMVC NAL Unit length bits: %d\n", 8*mvccfg->nal_unit_size);
#ifndef GPAC_DISABLE_AV_PARSERS
						for (i=0; i<gf_list_count(mvccfg->sequenceParameterSets); i++) {
							GF_NALUFFParam *slc = gf_list_get(mvccfg->sequenceParameterSets, i);
							if (slc) {
								u32 s_w, s_h, sps_id;
								s32 par_n, par_d;
								gf_avc_get_sps_info(slc->data, slc->size, &sps_id, &s_w, &s_h, &par_n, &par_d);
								fprintf(stderr, "\t\tSPS ID %d - Visual Size %d x %d\n", sps_id, s_w, s_h);
								if ((par_n>0) && (par_d>0)) {
									u32 tw, th;
									gf_isom_get_track_layout_info(file, trackNum, &tw, &th, NULL, NULL, NULL);
									fprintf(stderr, "\tPixel Aspect Ratio %d:%d - Indicated track size %d x %d\n", par_n, par_d, tw, th);
								}
							}
						}
#endif
						print_config_hash(mvccfg->sequenceParameterSets, "SPS");
						print_config_hash(mvccfg->pictureParameterSets, "PPS");
						gf_odf_avc_cfg_del(mvccfg);
					}

				} else if ((esd->decoderConfig->objectTypeIndication==GF_CODECID_HEVC)
				           || (esd->decoderConfig->objectTypeIndication==GF_CODECID_LHVC)
				          ) {
					GF_HEVCConfig *hevccfg, *lhvccfg;
					GF_OperatingPointsInformation *oinf;
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_HEVC)
					HEVCState hevc_state;
					memset(&hevc_state, 0, sizeof(HEVCState));
					hevc_state.sps_active_idx = -1;
#endif

					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					if (full_dump) fprintf(stderr, "\t");
					fprintf(stderr, "HEVC Video - Visual Size %d x %d\n", w, h);
					hevccfg = gf_isom_hevc_config_get(file, trackNum, 1);
					lhvccfg = gf_isom_lhvc_config_get(file, trackNum, 1);

					if (msub_type==GF_ISOM_SUBTYPE_HVT1) {
						const u8 *data;
						u32 tsize;
						u32  is_default, tx,ty,tw,th, id, independent;
						Bool full_frame;
						if (gf_isom_get_tile_info(file, trackNum, 1, &is_default, &id, &independent, &full_frame, &tx, &ty, &tw, &th)) {
							fprintf(stderr, "\tHEVC Tile - ID %d independent %d (x,y,w,h)=%d,%d,%d,%d \n", id, independent, tx, ty, tw, th);
						} else if (gf_isom_get_sample_group_info(file, trackNum, 1, GF_ISOM_SAMPLE_GROUP_TRIF, &is_default, &data, &tsize)) {
							fprintf(stderr, "\tHEVC Tile track containing a tile set\n");
						} else {
							fprintf(stderr, "\tHEVC Tile track without tiling info\n");
						}
					} else if (!hevccfg && !lhvccfg) {
						M4_LOG(GF_LOG_ERROR, ("\tNon-compliant HEVC track: No hvcC or shcC found in sample description\n"));
					}

					if (gf_isom_get_reference_count(file, trackNum, GF_ISOM_REF_SABT)) {
						fprintf(stderr, "\tHEVC Tile base track\n");
					}
					if (hevccfg) {
						dump_hevc_track_info(file, trackNum, hevccfg
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_HEVC)
							, &hevc_state
#endif
						);
						gf_odf_hevc_cfg_del(hevccfg);
						fprintf(stderr, "\n");
					}
					if (lhvccfg) {
						dump_hevc_track_info(file, trackNum, lhvccfg
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_HEVC)
							, &hevc_state
#endif
						);
						gf_odf_hevc_cfg_del(lhvccfg);
					}

					if (gf_isom_get_oinf_info(file, trackNum, &oinf)) {
						fprintf(stderr, "\n\tOperating Points Information -");
						fprintf(stderr, " scalability_mask %d (", oinf->scalability_mask);
						switch (oinf->scalability_mask) {
						case 2:
							fprintf(stderr, "Multiview");
							break;
						case 4:
							fprintf(stderr, "Spatial scalability");
							break;
						case 8:
							fprintf(stderr, "Auxilary");
							break;
						default:
							fprintf(stderr, "unknown");
						}
						//TODO: need to dump more info ?
						fprintf(stderr, ") num_profile_tier_level %d ", gf_list_count(oinf->profile_tier_levels) );
						fprintf(stderr, " num_operating_points %d dependency layers %d \n", gf_list_count(oinf->operating_points), gf_list_count(oinf->dependency_layers) );
					}
				}

				/*OGG media*/
				else if (esd->decoderConfig->objectTypeIndication==GF_CODECID_THEORA) {
					char *szName;
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					if (full_dump) fprintf(stderr, "\t");
					if (!strnicmp((char *) &esd->decoderConfig->decoderSpecificInfo->data[3], "theora", 6)) szName = "Theora";
					else szName = "Unknown";
					fprintf(stderr, "Ogg/%s video / GPAC Mux  - Visual Size %d x %d\n", szName, w, h);
				}
				else {
					//check if we know this codec from its OTI
					u32 codec_id = gf_codecid_from_oti(GF_STREAM_VISUAL, esd->decoderConfig->objectTypeIndication);
					if (codec_id) {
						gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
						fprintf(stderr, "%s - Visual Size %d x %d\n", gf_codecid_name(codec_id), w, h);
					}
				}
				if (!w || !h) {
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					if (full_dump) fprintf(stderr, "\t");
					fprintf(stderr, "Visual Size %d x %d\n", w, h);
				}
				if (gf_isom_get_rvc_config(file, trackNum, 1, &rvc_predef, NULL, NULL, NULL)==GF_OK) {
					fprintf(stderr, "Has RVC signaled - Predefined configuration %d\n", rvc_predef);
				}

			} else if (esd->decoderConfig->streamType==GF_STREAM_AUDIO) {
#ifndef GPAC_DISABLE_AV_PARSERS
				GF_M4ADecSpecInfo a_cfg;
				GF_Err e;
				u32 oti;
#endif
				u32 codec_id;
				Bool is_mp2 = GF_FALSE;
				switch (esd->decoderConfig->objectTypeIndication) {
				case GF_CODECID_AAC_MPEG2_MP:
				case GF_CODECID_AAC_MPEG2_LCP:
				case GF_CODECID_AAC_MPEG2_SSRP:
					is_mp2 = GF_TRUE;
				case GF_CODECID_AAC_MPEG4:
#ifndef GPAC_DISABLE_AV_PARSERS
					if (!esd->decoderConfig->decoderSpecificInfo)
						e = GF_NON_COMPLIANT_BITSTREAM;
					else
						e = gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &a_cfg);
					if (full_dump) fprintf(stderr, "\t");
					if (e) {
						M4_LOG(GF_LOG_ERROR, ("Corrupted AAC Config\n"));
					} else {
						char *signaling = "implicit";
						char *heaac = "";
						if (!is_mp2 && a_cfg.has_sbr) {
							if (a_cfg.has_ps) heaac = "(HE-AAC v2) ";
							else heaac = "(HE-AAC v1) ";
						}
						if (a_cfg.base_object_type==2) {
							if (a_cfg.has_ps || a_cfg.has_sbr)
								signaling = "backward compatible";
						} else {
							signaling = "hierarchical";
						}
						fprintf(stderr, "%s (AOT=%d %s) %s- %d Channel(s) - SampleRate %d", gf_m4a_object_type_name(a_cfg.base_object_type), a_cfg.base_object_type, signaling, heaac, a_cfg.nb_chan, a_cfg.base_sr);
						if (is_mp2) fprintf(stderr, " (MPEG-2 Signaling)");
						if (a_cfg.has_sbr) fprintf(stderr, " - SBR: SampleRate %d Type %s", a_cfg.sbr_sr, gf_m4a_object_type_name(a_cfg.sbr_object_type));
						if (a_cfg.has_ps) fprintf(stderr, " - PS");
						fprintf(stderr, "\n");
					}
#else
					fprintf(stderr, "MPEG-2/4 Audio - %d Channels - SampleRate %d\n", nb_ch, sr);
#endif
					break;
				case GF_CODECID_MPEG2_PART3:
				case GF_CODECID_MPEG_AUDIO:
					if (msub_type == GF_ISOM_SUBTYPE_MPEG4_CRYP) {
						fprintf(stderr, "MPEG-1/2 Audio - %d Channels - SampleRate %d\n", nb_ch, sr);
					} else {
#ifndef GPAC_DISABLE_AV_PARSERS
						GF_ISOSample *samp = gf_isom_get_sample(file, trackNum, 1, &oti);
						if (samp) {
							u32 mhdr = GF_4CC((u8)samp->data[0], (u8)samp->data[1], (u8)samp->data[2], (u8)samp->data[3]);
							if (full_dump) fprintf(stderr, "\t");
							fprintf(stderr, "%s Audio - %d Channel(s) - SampleRate %d - Layer %d\n",
							        gf_mp3_version_name(mhdr),
							        gf_mp3_num_channels(mhdr),
							        gf_mp3_sampling_rate(mhdr),
							        gf_mp3_layer(mhdr)
							       );
							gf_isom_sample_del(&samp);
						} else {
							M4_LOG(GF_LOG_ERROR, ("Error fetching sample: %s\n", gf_error_to_string(gf_isom_last_error(file)) ));
						}
#else
						fprintf(stderr, "MPEG-1/2 Audio - %d Channels - SampleRate %d\n", nb_ch, sr);
#endif
					}
					break;
				case GF_CODECID_EVRC:
					fprintf(stderr, "EVRC Audio - Sample Rate 8000 - 1 channel\n");
					break;
				case GF_CODECID_SMV:
					fprintf(stderr, "SMV Audio - Sample Rate 8000 - 1 channel\n");
					break;
				case GF_CODECID_QCELP:
					fprintf(stderr, "QCELP Audio - Sample Rate 8000 - 1 channel\n");
					break;
				/*packetVideo hack for EVRC...*/
				case GF_CODECID_EVRC_PV:
					if (esd->decoderConfig->decoderSpecificInfo && (esd->decoderConfig->decoderSpecificInfo->dataLength==8)
					        && !strnicmp((char *)esd->decoderConfig->decoderSpecificInfo->data, "pvmm", 4)) {
						if (full_dump) fprintf(stderr, "\t");
						fprintf(stderr, "EVRC Audio (PacketVideo Mux) - Sample Rate 8000 - 1 channel\n");
					}
					break;
				default:
					codec_id = gf_codecid_from_oti(GF_STREAM_AUDIO, esd->decoderConfig->objectTypeIndication);
					if (codec_id) {
						fprintf(stderr, "%s - Sample Rate %d - %d channel(s)\n", gf_codecid_name(codec_id), sr, nb_ch);
					}
					break;
				}
			}
			else if (esd->decoderConfig->streamType==GF_STREAM_SCENE) {
				if (esd->decoderConfig->objectTypeIndication<=4) {
					GF_BIFSConfig *b_cfg = gf_odf_get_bifs_config(esd->decoderConfig->decoderSpecificInfo, esd->decoderConfig->objectTypeIndication);
					fprintf(stderr, "BIFS Scene description - %s stream\n", b_cfg->elementaryMasks ? "Animation" : "Command");
					if (full_dump && !b_cfg->elementaryMasks) {
						fprintf(stderr, "\tWidth %d Height %d Pixel Metrics %s\n", b_cfg->pixelWidth, b_cfg->pixelHeight, b_cfg->pixelMetrics ? "yes" : "no");
					}
					gf_odf_desc_del((GF_Descriptor *)b_cfg);
				} else if (esd->decoderConfig->objectTypeIndication==GF_CODECID_AFX) {
					u8 tag = esd->decoderConfig->decoderSpecificInfo ? esd->decoderConfig->decoderSpecificInfo->data[0] : 0xFF;
					const char *afxtype = gf_stream_type_afx_name(tag);
					fprintf(stderr, "AFX Stream - type %s (%d)\n", afxtype, tag);
				} else if (esd->decoderConfig->objectTypeIndication==GF_CODECID_FONT) {
					fprintf(stderr, "Font Data stream\n");
				} else if (esd->decoderConfig->objectTypeIndication==GF_CODECID_LASER) {
					GF_LASERConfig l_cfg;
					gf_odf_get_laser_config(esd->decoderConfig->decoderSpecificInfo, &l_cfg);
					fprintf(stderr, "LASER Stream - %s\n", l_cfg.newSceneIndicator ? "Full Scene" : "Scene Segment");
				} else if (esd->decoderConfig->objectTypeIndication==GF_CODECID_TEXT_MPEG4) {
					fprintf(stderr, "MPEG-4 Streaming Text stream\n");
				} else if (esd->decoderConfig->objectTypeIndication==GF_CODECID_SYNTHESIZED_TEXTURE) {
					fprintf(stderr, "Synthetized Texture stream stream\n");
				} else {
					M4_LOG(GF_LOG_WARNING, ("Unknown Systems stream OTI %d\n", esd->decoderConfig->objectTypeIndication));
				}
			}

			/*sync is only valid if we open all tracks to take care of default MP4 sync..*/
			if (!full_dump) {
				if (dump_m4sys) {
					if (!esd->OCRESID || (esd->OCRESID == esd->ESID))
						fprintf(stderr, "Self-synchronized\n");
					else
						fprintf(stderr, "Synchronized on stream %d\n", esd->OCRESID);
				}
			} else {
				fprintf(stderr, "\tDecoding Buffer size %d - Bitrate: avg %d - max %d kbps\n", esd->decoderConfig->bufferSizeDB, esd->decoderConfig->avgBitrate/1000, esd->decoderConfig->maxBitrate/1000);
				if (esd->dependsOnESID)
					fprintf(stderr, "\tDepends on stream %d for decoding\n", esd->dependsOnESID);
				else
					fprintf(stderr, "\tNo stream dependencies for decoding\n");

				fprintf(stderr, "\tStreamPriority %d\n", esd->streamPriority);
				if (esd->URLString) fprintf(stderr, "\tRemote Data Source %s\n", esd->URLString);
			}
			gf_odf_desc_del((GF_Descriptor *) esd);
		}
	} else if (msub_type == GF_ISOM_SUBTYPE_AV01) {
		GF_AV1Config *av1c;
		u32 w, h;
		gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
		fprintf(stderr, "\tAOM AV1 stream - Resolution %d x %d\n", w, h);

		av1c = gf_isom_av1_config_get(file, trackNum, 1);
		fprintf(stderr, "\tversion=%u, profile=%u, level_idx0=%u, tier=%u\n", (u32)av1c->version, (u32)av1c->seq_profile, (u32)av1c->seq_level_idx_0, (u32)av1c->seq_tier_0);
		fprintf(stderr, "\thigh_bitdepth=%u, twelve_bit=%u, monochrome=%u\n", (u32)av1c->high_bitdepth, (u32)av1c->twelve_bit, (u32)av1c->monochrome);
		fprintf(stderr, "\tchroma: subsampling_x=%u, subsampling_y=%u, sample_position=%u\n", (u32)av1c->chroma_subsampling_x, (u32)av1c->chroma_subsampling_y, (u32)av1c->chroma_sample_position);

		if (av1c->initial_presentation_delay_present)
			fprintf(stderr, "\tInitial presentation delay %u\n", (u32) av1c->initial_presentation_delay_minus_one+1);

		count = gf_list_count(av1c->obu_array);
		for (i=0; i<count; i++) {
			u8 hash[20];
			GF_AV1_OBUArrayEntry *obu = gf_list_get(av1c->obu_array, i);
			gf_sha1_csum((u8*)obu->obu, (u32)obu->obu_length, hash);
			fprintf(stderr, "\tOBU#%d %s hash: ", i+1, gf_av1_get_obu_name(obu->obu_type) );
			for (j=0; j<20; j++) fprintf(stderr, "%02X", hash[j]);
			fprintf(stderr, "\n");
		}
		gf_odf_av1_cfg_del(av1c);
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_H263) {
		u32 w, h;
		gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
		fprintf(stderr, "\t3GPP H263 stream - Resolution %d x %d\n", w, h);
	} else if (msub_type == GF_ISOM_SUBTYPE_MJP2) {
		u32 w, h;
		gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
		fprintf(stderr, "\tMotionJPEG2000 stream - Resolution %d x %d\n", w, h);
	} else if ((msub_type == GF_ISOM_SUBTYPE_3GP_AMR) || (msub_type == GF_ISOM_SUBTYPE_3GP_AMR_WB)) {
		fprintf(stderr, "\t3GPP AMR%s stream - Sample Rate %d - %d channel(s) %d bps\n", (msub_type == GF_ISOM_SUBTYPE_3GP_AMR_WB) ? " Wide Band" : "", sr, nb_ch, (u32) bps);
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_EVRC) {
		fprintf(stderr, "\t3GPP EVRC stream - Sample Rate %d - %d channel(s) %d bps\n", sr, nb_ch, (u32) bps);
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_QCELP) {
		fprintf(stderr, "\t3GPP QCELP stream - Sample Rate %d - %d channel(s) %d bps\n", sr, nb_ch, (u32) bps);
	} else if (msub_type == GF_ISOM_SUBTYPE_MP3) {
		fprintf(stderr, "\tMPEG 1/2 Audio stream - Sample Rate %d - %d channel(s) %d bps\n", sr, nb_ch, (u32) bps);
	} else if ((msub_type == GF_ISOM_SUBTYPE_AC3) || (msub_type == GF_ISOM_SUBTYPE_EC3)) {
		u32 br = 0;
		const char *lfe = "";
		Bool is_ec3 = (msub_type == GF_ISOM_SUBTYPE_EC3) ? GF_TRUE : GF_FALSE;
#ifndef GPAC_DISABLE_AV_PARSERS
		GF_AC3Config *ac3 = gf_isom_ac3_config_get(file, trackNum, 1);
		if (ac3) {
			nb_ch = gf_ac3_get_channels(ac3->streams[0].acmod);
			for (i=0; i<ac3->streams[0].nb_dep_sub; ++i) {
				assert(ac3->streams[0].nb_dep_sub == 1);
				nb_ch += gf_ac3_get_channels(ac3->streams[0].chan_loc);
			}
			if (ac3->streams[0].lfon) lfe = ".1";
			br = ac3->is_ec3 ? ac3->brcode : gf_ac3_get_bitrate(ac3->brcode);
			is_ec3 = ac3->is_ec3;
			gf_free(ac3);
		}
#endif
		fprintf(stderr, "\t%s stream - Sample Rate %d - %d%s channel(s) - bitrate %d\n", is_ec3 ? "EC-3" : "AC-3", sr, nb_ch, lfe, br);
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_SMV) {
		fprintf(stderr, "\t3GPP SMV stream - Sample Rate %d - %d channel(s) %d bits per samples\n", sr, nb_ch, (u32) bps);
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_DIMS) {
		u32 w, h;
		GF_DIMSDescription dims;
		gf_isom_get_visual_info(file, trackNum, 1, &w, &h);

		gf_isom_get_dims_description(file, trackNum, 1, &dims);
		fprintf(stderr, "\t3GPP DIMS stream - size %d x %d - Profile %d - Level %d\n", w, h, dims.profile, dims.level);
		fprintf(stderr, "\tpathComponents: %d - useFullRequestHost: %s\n", dims.pathComponents, dims.fullRequestHost ? "yes" : "no");
		fprintf(stderr, "\tstream type: %s - redundant: %s\n", dims.streamType ? "primary" : "secondary", (dims.containsRedundant==1) ? "main" : ((dims.containsRedundant==2) ? "redundant" : "main+redundant") );
		if (dims.textEncoding[0]) fprintf(stderr, "\ttext encoding %s\n", dims.textEncoding);
		if (dims.contentEncoding[0]) fprintf(stderr, "\tcontent encoding %s\n", dims.contentEncoding);
		if (dims.content_script_types) fprintf(stderr, "\tscript languages %s\n", dims.content_script_types);
	} else if (mtype==GF_ISOM_MEDIA_HINT) {
		u32 refTrack;
		s32 refCount = gf_isom_get_reference_count(file, trackNum, GF_ISOM_REF_HINT);
		if (refCount>0) {
			fprintf(stderr, "Streaming Hint Track for track%s ", (refCount>1) ? "s" :"");
			for (i=0; i<(u32) refCount; i++) {
				gf_isom_get_reference(file, trackNum, GF_ISOM_REF_HINT, i+1, &refTrack);
				if (i) fprintf(stderr, " - ");
				fprintf(stderr, "ID %d", gf_isom_get_track_id(file, refTrack));
			}
			fprintf(stderr, "\n");
		} else {
			fprintf(stderr, "Streaming Hint Track (no refs)\n");
		}
#ifndef GPAC_DISABLE_ISOM_HINTING
		refCount = gf_isom_get_payt_count(file, trackNum);
		if (refCount>0) {
			for (i=0; i<(u32) refCount; i++) {
				const char *name = gf_isom_get_payt_info(file, trackNum, i+1, &refTrack);
				fprintf(stderr, "\tPayload ID %d: type %s\n", refTrack, name);
			}
		}
#endif
	} else if (mtype==GF_ISOM_MEDIA_FLASH) {
		fprintf(stderr, "Macromedia Flash Movie\n");
	} else if ((mtype==GF_ISOM_MEDIA_TEXT) || (mtype==GF_ISOM_MEDIA_SUBT) || (mtype==GF_ISOM_MEDIA_MPEG_SUBT)) {
		u32 w, h;
		s16 l;
		s32 tx, ty;
		const char *content_encoding = NULL;
		const char *mime = NULL;
		const char *config  = NULL;
		const char *_namespace = NULL;
		const char *schema_loc = NULL;
		const char *auxiliary_mimes = NULL;
		gf_isom_get_track_layout_info(file, trackNum, &w, &h, &tx, &ty, &l);
		if (msub_type == GF_ISOM_SUBTYPE_SBTT) {
			gf_isom_stxt_get_description(file, trackNum, 1, &mime, &content_encoding, &config);
			fprintf(stderr, "Textual Subtitle Stream ");
			fprintf(stderr, "- mime %s", mime);
			if (content_encoding != NULL) {
				fprintf(stderr, " - encoding %s", content_encoding);
			}
			if (config != NULL) {
				fprintf(stderr, " - %d bytes config", (u32) strlen(config));
			}
		} else if (msub_type == GF_ISOM_SUBTYPE_STXT) {
			gf_isom_stxt_get_description(file, trackNum, 1, &mime, &content_encoding, &config);
			fprintf(stderr, "Simple Timed Text Stream ");
			fprintf(stderr, "- mime %s", mime);
			if (content_encoding != NULL) {
				fprintf(stderr, " - encoding %s", content_encoding);
			}
			if (config != NULL) {
				fprintf(stderr, " - %d bytes config", (u32) strlen(config));
			}
		} else if (msub_type == GF_ISOM_SUBTYPE_STPP) {
			gf_isom_xml_subtitle_get_description(file, trackNum, 1, &_namespace, &schema_loc, &auxiliary_mimes);
			fprintf(stderr, "XML Subtitle Stream ");
			fprintf(stderr, "- namespace %s", _namespace);
			if (schema_loc != NULL) {
				fprintf(stderr, " - schema-location %s", schema_loc);
			}
			if (auxiliary_mimes != NULL) {
				fprintf(stderr, " - auxiliary-mime-types %s", auxiliary_mimes);
			}
		} else {
			fprintf(stderr, "Unknown Text Stream");
		}
		fprintf(stderr, "\n Size %d x %d - Translation X=%d Y=%d - Layer %d\n", w, h, tx, ty, l);
	} else if (mtype == GF_ISOM_MEDIA_META) {
		const char *content_encoding = NULL;
		if (msub_type == GF_ISOM_SUBTYPE_METT) {
			const char *mime = NULL;
			const char *config  = NULL;
			gf_isom_stxt_get_description(file, trackNum, 1, &mime, &content_encoding, &config);
			fprintf(stderr, "Textual Metadata Stream - mime %s", mime);
			if (content_encoding != NULL) {
				fprintf(stderr, " - encoding %s", content_encoding);
			}
			if (config != NULL) {
				fprintf(stderr, " - %d bytes config", (u32) strlen(config));
			}
			fprintf(stderr, "\n");
		} else if (msub_type == GF_ISOM_SUBTYPE_METX) {
			const char *_namespace = NULL;
			const char *schema_loc = NULL;
			gf_isom_get_xml_metadata_description(file, trackNum, 1, &_namespace, &schema_loc, &content_encoding);
			fprintf(stderr, "XML Metadata Stream - namespace %s", _namespace);
			if (content_encoding != NULL) {
				fprintf(stderr, " - encoding %s", content_encoding);
			}
			if (schema_loc != NULL) {
				fprintf(stderr, " - schema-location %s", schema_loc);
			}
			fprintf(stderr, "\n");
		} else {
			fprintf(stderr, "Unknown Metadata Stream\n");
		}
	} else if ((msub_type==GF_ISOM_SUBTYPE_VVC1) || (msub_type==GF_ISOM_SUBTYPE_VVI1)) {
		GF_VVCConfig *vvccfg;
		u32 w, h;
#if !defined(GPAC_DISABLE_AV_PARSERS)
		VVCState *vvc_state;
		GF_SAFEALLOC(vvc_state, VVCState);
		if (vvc_state) vvc_state->sps_active_idx = -1;
#endif

		gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
		if (full_dump) fprintf(stderr, "\t");
		fprintf(stderr, "VVC Video - Visual Size %d x %d\n", w, h);
		vvccfg = gf_isom_vvc_config_get(file, trackNum, 1);

		if (!vvccfg) {
			M4_LOG(GF_LOG_ERROR, ("Non-compliant VVC track: No vvcC found in sample description\n"));
		} else {
			dump_vvc_track_info(file, trackNum, vvccfg
#if !defined(GPAC_DISABLE_AV_PARSERS)
				, vvc_state
#endif
			);
			gf_odf_vvc_cfg_del(vvccfg);
			fprintf(stderr, "\n");
		}
#if !defined(GPAC_DISABLE_AV_PARSERS)
		if (vvc_state) gf_free(vvc_state);
#endif
	} else if ((msub_type == GF_ISOM_SUBTYPE_MH3D_MHA1) || (msub_type == GF_ISOM_SUBTYPE_MH3D_MHA2)
			|| (msub_type == GF_ISOM_SUBTYPE_MH3D_MHM1) || (msub_type == GF_ISOM_SUBTYPE_MH3D_MHM2)
	) {
		const u8 *compat_profiles;
		u32 nb_compat_profiles;
		Bool valid = GF_FALSE;
		Bool allow_inband = GF_FALSE;
		if ( (msub_type == GF_ISOM_SUBTYPE_MH3D_MHM1) || (msub_type == GF_ISOM_SUBTYPE_MH3D_MHM2))
			allow_inband = GF_TRUE;

		fprintf(stderr, "\tMPEG-H Audio stream - Sample Rate %d\n", sr);

		esd = gf_media_map_esd(file, trackNum, 1);
		if (!esd || !esd->decoderConfig || !esd->decoderConfig->decoderSpecificInfo
			|| !esd->decoderConfig->decoderSpecificInfo->data
		) {
			if (allow_inband) {
				GF_ISOSample *samp = gf_isom_get_sample(file, trackNum, 1, NULL);
				if (samp) {
					u64 ch_layout=0;
					s32 PL = gf_mpegh_get_mhas_pl(samp->data, samp->dataLength, &ch_layout);
					if (PL>=0) {
						fprintf(stderr, "\tProfileLevelIndication: 0x%02X", PL);
						if (ch_layout)
							fprintf(stderr, " - Reference Channel Layout %s", gf_audio_fmt_get_layout_name(ch_layout) );
						fprintf(stderr, "\n");
					}
					gf_isom_sample_del(&samp);
				}
				valid = GF_TRUE;
			}
		} else if (esd->decoderConfig->decoderSpecificInfo->dataLength>=5) {
			fprintf(stderr, "\tProfileLevelIndication: 0x%02X - Reference Channel Layout %s\n", esd->decoderConfig->decoderSpecificInfo->data[1]
				, gf_audio_fmt_get_layout_name_from_cicp(esd->decoderConfig->decoderSpecificInfo->data[2])
			);
			valid = GF_TRUE;
		}
		if (!valid) {
			M4_LOG(GF_LOG_ERROR, ("Invalid MPEG-H audio config\n"));
		}
		if (esd) gf_odf_desc_del((GF_Descriptor *)esd);
		compat_profiles = gf_isom_get_mpegh_compatible_profiles(file, trackNum, 1, &nb_compat_profiles);
		for (i=0; i<nb_compat_profiles; i++) {
			if (!i)
				fprintf(stderr, "\tCompatible profiles:");
			fprintf(stderr, " 0x%02X", compat_profiles[i]);
		}
		if (i) fprintf(stderr, "\n");
	} else if (msub_type==GF_ISOM_SUBTYPE_MLPA) {
		u32 fmt, prate;
		if (gf_isom_truehd_config_get(file, trackNum, 1, &fmt, &prate) != GF_OK) {
			fprintf(stderr, "\tInvalid TrueHD audio config\n");
		}
		fprintf(stderr, "TrueHD Audio stream - Sample Rate %u - channels %u - format %u peak rate %u\n", sr, nb_ch, fmt, prate);
	} else if (codecid) {
		if (gf_isom_is_video_handler_type(mtype) ) {
			u32 w, h;
			gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
			fprintf(stderr, "%s - Resolution %d x %d\n", gf_codecid_name(codecid), w, h);
		} else if (mtype==GF_ISOM_MEDIA_AUDIO) {
			gf_isom_get_audio_info(file, trackNum, 1, &sr, &nb_ch, NULL);
			fprintf(stderr, "%s - Sample Rate %d - %d channel(s)\n", gf_codecid_name(codecid), sr, nb_ch);
		} else {
			fprintf(stderr, "%s\n", gf_codecid_name(codecid) );
		}
	} else if (pfmt) {
		u32 w, h;
		gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
		fprintf(stderr, "Raw video %s - Resolution %d x %d\n", gf_pixel_fmt_name(pfmt), w, h);
	} else if (msub_type==GF_QT_SUBTYPE_TMCD) {
		u32 stsd_idx;
		GF_ISOSample *sample = gf_isom_get_sample(file, trackNum, 1, &stsd_idx);
		fprintf(stderr, "Time Code stream\n");
		if (sample) {
			char szTimecode[100];
			u32 tmcd_flags, tmcd_num, tmcd_den, tmcd_fpt;

			gf_isom_get_tmcd_config(file, trackNum, stsd_idx, &tmcd_flags, &tmcd_num, &tmcd_den, &tmcd_fpt);

			gf_inspect_format_timecode(sample->data, sample->dataLength, tmcd_flags, tmcd_num, tmcd_den, tmcd_fpt, szTimecode);

			gf_isom_sample_del(&sample);
			fprintf(stderr, "\tFirst timecode: %s\n", szTimecode);
		}
	} else {
		GF_GenericSampleDescription *udesc;

		udesc = gf_isom_get_generic_sample_description(file, trackNum, 1);
		if (udesc) {
			if (gf_isom_is_video_handler_type(mtype) ) {
                fprintf(stderr, "%s - Compressor \"%s\" - Resolution %d x %d\n",
						( (mtype == GF_ISOM_MEDIA_VISUAL ? "Visual" : "Auxiliary Video") ),
                        udesc->compressor_name, udesc->width, udesc->height);
			} else if (mtype==GF_ISOM_MEDIA_AUDIO) {
				fprintf(stderr, "Audio - Sample Rate %d - %d channel(s)\n", udesc->samplerate, udesc->nb_channels);
			} else {
				fprintf(stderr, "Unknown media type\n");
			}
			if (udesc->vendor_code)
				fprintf(stderr, "\tVendor code \"%s\" - Version %d - revision %d\n", gf_4cc_to_str(udesc->vendor_code), udesc->version, udesc->revision);

			if (udesc->extension_buf) {
				fprintf(stderr, "\tCodec configuration data size: %d bytes\n", udesc->extension_buf_size);
				gf_free(udesc->extension_buf);
			}
			gf_free(udesc);
		} else {
			fprintf(stderr, "Unknown track type\n");
		}
	}


	/*Crypto info*/
	if (gf_isom_is_track_encrypted(file, trackNum)) {
		const char *scheme_URI, *KMS_URI;
		u32 scheme_type, version;
		u32 IV_size;
		Bool use_sel_enc;

		if (gf_isom_is_ismacryp_media(file, trackNum, 1)) {
			gf_isom_get_ismacryp_info(file, trackNum, 1, NULL, &scheme_type, &version, &scheme_URI, &KMS_URI, &use_sel_enc, &IV_size, NULL);
			fprintf(stderr, "\n\tProtected by ISMA E&A scheme %s (version %d)\n", gf_4cc_to_str(scheme_type), version);
			if (scheme_URI) fprintf(stderr, "scheme location: %s\n", scheme_URI);
			if (KMS_URI) {
				if (!strnicmp(KMS_URI, "(key)", 5)) fprintf(stderr, "\tKMS location: key in file\n");
				else fprintf(stderr, "\tKMS location: %s\n", KMS_URI);
			}
			fprintf(stderr, "\tSelective Encryption: %s\n", use_sel_enc ? "Yes" : "No");
			if (IV_size) fprintf(stderr, "\tInitialization Vector size: %d bits\n", IV_size*8);
		} else if (gf_isom_is_omadrm_media(file, trackNum, 1)) {
			const char *textHdrs;
			u32 enc_type, hdr_len;
			u64 orig_len;
			gf_isom_get_omadrm_info(file, trackNum, 1, NULL, &scheme_type, &version, &scheme_URI, &KMS_URI, &textHdrs, &hdr_len, &orig_len, &enc_type, &use_sel_enc, &IV_size, NULL);
			fprintf(stderr, "\n\tProtected by OMA DRM scheme %s (version %d)\n", gf_4cc_to_str(scheme_type), version);
			fprintf(stderr, "\tRights Issuer: %s\n", KMS_URI);
			fprintf(stderr, "\tContent ID: %s\n", scheme_URI);
			if (textHdrs) {
				u32 offset;
				const char *start = textHdrs;
				fprintf(stderr, "\tOMA Textual Headers:\n");
				i=0;
				offset=0;
				while (i<hdr_len) {
					if (start[i]==0) {
						fprintf(stderr, "\t\t%s\n", start+offset);
						offset=i+1;
					}
					i++;
				}
				fprintf(stderr, "\\tt%s\n", start+offset);
			}
			if (orig_len) fprintf(stderr, "\tOriginal media size "LLD"\n", orig_len);
			fprintf(stderr, "\tEncryption algorithm %s\n", (enc_type==1) ? "AEA 128 CBC" : (enc_type ? "AEA 128 CTR" : "None"));
			fprintf(stderr, "\tSelective Encryption: %s\n", use_sel_enc ? "Yes" : "No");
			if (IV_size) fprintf(stderr, "\tInitialization Vector size: %d bits\n", IV_size*8);
		} else if(gf_isom_is_cenc_media(file, trackNum, 1)) {
			const u8 *def_key;
			u32 def_key_size;
			Bool IsEncrypted;
			u8 crypt_byte_block, skip_byte_block;
			IV_size = 0;
			gf_isom_get_cenc_info(file, trackNum, 1, NULL, &scheme_type, &version);

			gf_isom_cenc_get_default_info(file, trackNum, 1, NULL, &IsEncrypted, &crypt_byte_block, &skip_byte_block, &def_key, &def_key_size);

			fprintf(stderr, "\n\tProtected by CENC scheme %s version 0x%08X", gf_4cc_to_str(scheme_type), version);

			if (crypt_byte_block && skip_byte_block)
				fprintf(stderr, " - Pattern %d:%d", (u32) skip_byte_block, (u32) crypt_byte_block);
			if (def_key && def_key[0])
				fprintf(stderr, " - MultiKey");

			fprintf(stderr, "\n");
			dump_key_info(def_key, def_key_size, IsEncrypted);

		} else if(gf_isom_is_adobe_protection_media(file, trackNum, 1)) {
			gf_isom_get_adobe_protection_info(file, trackNum, 1, NULL, &scheme_type, &version, NULL);
			fprintf(stderr, "\nProtected by Adobe scheme %s (version %d)\n", gf_4cc_to_str(scheme_type), version);
		} else {
			fprintf(stderr, "\nProtected by unknown scheme %s\n", gf_4cc_to_str(gf_isom_is_media_encrypted(file, trackNum, 0) ));
		}
		fprintf(stderr, "\n");
	}

	if ( gf_media_get_rfc_6381_codec_name(file, trackNum, szCodec, GF_FALSE, GF_FALSE) == GF_OK) {
		fprintf(stderr, "\tRFC6381 Codec Parameters: %s\n", szCodec);
	}


	DumpMetaItem(file, 0, trackNum, "\tTrack Meta");

	gf_isom_get_track_switch_group_count(file, trackNum, &alt_group, &nb_groups);
	if (alt_group) {
		fprintf(stderr, "Alternate Group ID %d\n", alt_group);
		for (i=0; i<nb_groups; i++) {
			u32 nb_crit, switchGroupID;
			const u32 *criterias = gf_isom_get_track_switch_parameter(file, trackNum, i+1, &switchGroupID, &nb_crit);
			if (!nb_crit) {
				fprintf(stderr, "\tNo criteria in %s group\n", switchGroupID ? "switch" : "alternate");
			} else {
				if (switchGroupID) {
					fprintf(stderr, "\tSwitchGroup ID %d criterias: ", switchGroupID);
				} else {
					fprintf(stderr, "\tAlternate Group criterias: ");
				}
				for (j=0; j<nb_crit; j++) {
					if (j) fprintf(stderr, " ");
					fprintf(stderr, "%s", gf_4cc_to_str(criterias[j]) );
				}
				fprintf(stderr, "\n");
			}
		}
	}

	switch (gf_isom_has_sync_points(file, trackNum)) {
	case 0:
		fprintf(stderr, "\tAll samples are sync\n");
		break;
	case 1:
	{
		u32 nb_sync = gf_isom_get_sync_point_count(file, trackNum) - 1;
		if (! nb_sync) {
			fprintf(stderr, "\tOnly one sync sample\n");
		} else {
			fprintf(stderr, "\tAverage GOP length: %d samples\n", gf_isom_get_sample_count(file, trackNum) / nb_sync);
		}
	}
	break;
	case 2:
		fprintf(stderr, "\tNo sync sample found\n");
		break;
	}
	fprintf(stderr, "\tMax sample duration: %d / %d\n", gf_isom_get_max_sample_delta(file, trackNum), timescale);

	if (!full_dump) {
		fprintf(stderr, "\n");
		return;
	}

	dur = size = 0;
	max_rate = rate = 0;
	time_slice = 0;
	ts = gf_isom_get_media_timescale(file, trackNum);
	csize = gf_isom_get_constant_sample_size(file, trackNum);
	cdur = gf_isom_get_constant_sample_duration(file, trackNum);
	count = gf_isom_get_sample_count(file, trackNum);
	if (csize && cdur) {
		size = count * csize;
		dur = cdur * count;
	} else {

		for (j=0; j<count; j++) {
			GF_ISOSample *samp;
			if (is_od_track) {
				samp = gf_isom_get_sample(file, trackNum, j+1, NULL);
			} else {
				samp = gf_isom_get_sample_info(file, trackNum, j+1, NULL, NULL);
			}
			if (!samp) {
				M4_LOG(GF_LOG_ERROR, ("Failed to fetch sample %d\n", j+1));
				return;
			}
			dur = samp->DTS+samp->CTS_Offset;
			size += samp->dataLength;
			rate += samp->dataLength;
			if (samp->DTS - time_slice > ts) {
				Double max_tmp = rate * ts / (samp->DTS - time_slice);
				if (max_rate < max_tmp )
					max_rate = max_tmp;

				rate = 0;
				time_slice = samp->DTS;
			}
			gf_isom_sample_del(&samp);
		}
	}
	fprintf(stderr, "\nComputed info from media:\n");
	if (csize && cdur) {
		fprintf(stderr, "\tConstant sample size %d bytes and dur %d / %d\n", csize, cdur, ts);
	}
	scale = 1000.0 / ts;
	dur = (u64) (scale * dur);
	fprintf(stderr, "\tTotal size "LLU" bytes - Total samples duration "LLU" ms\n", size, dur);
	if (!dur) {
		fprintf(stderr, "\n");
		return;
	}
	/*rate in byte, dur is in ms*/
	rate = 8000.0 * size / dur;

	if (!max_rate)
		max_rate = rate;
	else
		max_rate *= 8.0;

	if (rate >= 1500) {
		fprintf(stderr, "\tAverage rate %.2f kbps - Max Rate %.2f kbps\n", rate/1000, max_rate/1000);
	} else {
		fprintf(stderr, "\tAverage rate %.2f bps - Max Rate %.2f bps\n", rate, max_rate);
	}

	{
		u32 dmin, dmax, davg, smin, smax, savg;
		gf_isom_get_chunks_infos(file, trackNum, &dmin, &davg, &dmax, &smin, &savg, &smax);
		fprintf(stderr, "\tChunk durations: min %d ms - max %d ms - average %d ms\n", (1000*dmin)/ts, (1000*dmax)/ts, (1000*davg)/ts);
		fprintf(stderr, "\tChunk sizes (bytes): min %d - max %d - average %d\n", smin, smax, savg);
	}
	fprintf(stderr, "\n");

	count = gf_isom_get_chapter_count(file, trackNum);
	if (count) {
		const char *name;
		u64 time;
		fprintf(stderr, "\nChapters:\n");
		for (j=0; j<count; j++) {
			gf_isom_get_chapter(file, trackNum, j+1, &time, &name);
			fprintf(stderr, "\tChapter #%d - %s - \"%s\"\n", j+1, format_duration(time, 1000, szDur), name);
		}
	}
}

void DumpMovieInfo(GF_ISOFile *file)
{
	GF_InitialObjectDescriptor *iod;
	Bool dump_m4sys = GF_FALSE;
	u32 i, brand, min, timescale, count, data_len;
	const u8 *data;
	u64 create, modif;
	Bool has_itags = GF_FALSE;
	char szDur[50];

	DumpMetaItem(file, 1, 0, "# File Meta");
	if (!gf_isom_has_movie(file)) {
		if (gf_isom_has_segment(file, &brand, &min)) {
			count = gf_isom_segment_get_fragment_count(file);
			fprintf(stderr, "File is a segment - %d movie fragments - Brand %s (version %d):\n", count, gf_4cc_to_str(brand), min);
			for (i=0; i<count; i++) {
				u32 j, traf_count = gf_isom_segment_get_track_fragment_count(file, i+1);
				for (j=0; j<traf_count; j++) {
					u32 ID;
					u64 tfdt;
					ID = gf_isom_segment_get_track_fragment_decode_time(file, i+1, j+1, &tfdt);
					fprintf(stderr, "\tFragment #%d Track ID %d - TFDT "LLU"\n", i+1, ID, tfdt);
				}
			}
		} else {
			fprintf(stderr, "File has no movie (moov) - static data container\n");
		}
		return;
	}

	timescale = gf_isom_get_timescale(file);
	i=gf_isom_get_track_count(file);
	fprintf(stderr, "# Movie Info - %d track%s - TimeScale %d\n", i, i>1 ? "s" : "", timescale);

	modif = gf_isom_get_duration(file);
	create = gf_isom_get_original_duration(file);
	fprintf(stderr, "Duration %s", format_duration(create, timescale, szDur));
	if (create!=modif) {
		fprintf(stderr, " (recomputed %s)", format_duration(modif, timescale, szDur));
	}
	fprintf(stderr, "\n");

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (gf_isom_is_fragmented(file)) {
		fprintf(stderr, "Fragmented: yes - duration %s\n%d fragments - %d SegmentIndexes\n", format_duration(gf_isom_get_fragmented_duration(file), timescale, szDur), gf_isom_get_fragments_count(file, 0) , gf_isom_get_fragments_count(file, 1) );
	} else {
		fprintf(stderr, "Fragmented: no\n");
	}
#endif

	if (gf_isom_moov_first(file))
		fprintf(stderr, "Progressive (moov before mdat)\n");

	if (gf_isom_get_brand_info(file, &brand, &min, &count) == GF_OK) {
		fprintf(stderr, "Major Brand %s - version %d - compatible brands:", gf_4cc_to_str(brand), min);
		for (i=0; i<count;i++) {
			if (gf_isom_get_alternate_brand(file, i+1, &brand)==GF_OK)
				fprintf(stderr, " %s", gf_4cc_to_str(brand) );
		}
		fprintf(stderr, "\n");
	}
	gf_isom_get_creation_time(file, &create, &modif);
	fprintf(stderr, "Created: %s", format_date(create, szDur));
	if (create != modif)
		fprintf(stderr, "Modified: %s", format_date(modif, szDur));
	fprintf(stderr, "\n");

	DumpMetaItem(file, 0, 0, "# Movie Meta");

	iod = (GF_InitialObjectDescriptor *) gf_isom_get_root_od(file);
	if (iod) {
		u32 desc_size = gf_odf_desc_size((GF_Descriptor *)iod);
		if (iod->tag == GF_ODF_IOD_TAG) {
			fprintf(stderr, "File has root IOD (%d bytes)\n", desc_size);
			fprintf(stderr, "Scene PL 0x%02x - Graphics PL 0x%02x - OD PL 0x%02x\n", iod->scene_profileAndLevel, iod->graphics_profileAndLevel, iod->OD_profileAndLevel);
			fprintf(stderr, "Visual PL: %s (0x%02x)\n", gf_m4v_get_profile_name(iod->visual_profileAndLevel), iod->visual_profileAndLevel);
			fprintf(stderr, "Audio PL: %s (0x%02x)\n", gf_m4a_get_profile_name(iod->audio_profileAndLevel), iod->audio_profileAndLevel);
			//fprintf(stderr, "inline profiles included %s\n", iod->inlineProfileFlag ? "yes" : "no");
		} else {
			fprintf(stderr, "File has root OD (%d bytes)\n", desc_size);
		}
		if (!gf_list_count(iod->ESDescriptors))
			fprintf(stderr, "No streams included in root OD\n");
		else
			dump_m4sys = GF_TRUE;

		gf_odf_desc_del((GF_Descriptor *) iod);
	}
	if (gf_isom_is_JPEG2000(file)) fprintf(stderr, "File is JPEG 2000\n");

	count = gf_isom_get_copyright_count(file);
	if (count) {
		const char *lang, *note;
		fprintf(stderr, "\nCopyrights:\n");
		for (i=0; i<count; i++) {
			gf_isom_get_copyright(file, i+1, &lang, &note);
			fprintf(stderr, "\t(%s) %s\n", lang, note);
		}
	}

	count = gf_isom_get_chapter_count(file, 0);
	if (count) {
		const char *name;
		u64 time;
		fprintf(stderr, "\nChapters:\n");
		for (i=0; i<count; i++) {
			gf_isom_get_chapter(file, 0, i+1, &time, &name);
			fprintf(stderr, "\tChapter #%d - %s - \"%s\"\n", i+1, format_duration(time, 1000, szDur), name);
		}
	}

	if (gf_isom_apple_get_tag(file, 0, &data, &data_len) == GF_OK) {
		has_itags = GF_TRUE;
		fprintf(stderr, "\niTunes Info:\n");

		i=0;
		while (1) {
			u32 int_val2, flags, itype;
			GF_ISOiTunesTag tag;
			u64 int_val;
			s32 tag_idx;
			GF_Err e = gf_isom_apple_enum_tag(file, i, &tag, &data, &data_len, &int_val, &int_val2, &flags);
			if (e) break;
			i++;

			tag_idx = gf_itags_find_by_itag(tag);
			if (tag_idx<0) {
				fprintf(stderr, "\t%s: %s\n", gf_4cc_to_str(tag), data);
				continue;
			}
			fprintf(stderr, "\t%s: ", gf_itags_get_name(tag_idx) );
			itype = gf_itags_get_type(tag_idx);
			switch (itype) {
			case GF_ITAG_BOOL: fprintf(stderr, int_val ? "yes" : "no"); break;
			case GF_ITAG_INT8:
			case GF_ITAG_INT16:
			case GF_ITAG_INT32:
			case GF_ITAG_INT64:
				fprintf(stderr, LLU, int_val);
				break;
			case GF_ITAG_FRAC6:
			case GF_ITAG_FRAC8:
				fprintf(stderr, LLU" / %u", int_val, int_val2);
				break;
			case GF_ITAG_FILE:
				if (flags==14) fprintf(stderr, "PNG File");
				else if (flags==13) fprintf(stderr, "JPEG File");
				else fprintf(stderr, "unknown (flags %d)", flags);
				break;
			case GF_ITAG_ID3_GENRE:
				if (int_val) {
					fprintf(stderr, "%s", gf_id3_get_genre((u32) int_val) );
					break;
				}
				//fallthrough
			default:
				if (data)
					fprintf(stderr, "%s", data);
				else
					fprintf(stderr, data_len ? "none" : "unknown");
				break;
			}
			fprintf(stderr, "\n");
		}
	}
	i=0;
	while (1) {
		u32 type, version;
		char *wmatag;
		GF_Err e = gf_isom_wma_enum_tag(file, i, &wmatag, &data, &data_len, &version, &type);
		if (e) break;
		if (!i) {
			fprintf(stderr, "\nWMA Info:\n");
		}
		i++;
		fprintf(stderr, "\t%s", wmatag);
		if (version!=1)
			fprintf(stderr, " (version %d)", version);
		fprintf(stderr, ": ");

		if (type) {
			fprintf(stderr, "unknown type %d\n", type);
		} else {
			u16 *src_str = (u16 *) data;
			u32 len = (u32) ( UTF8_MAX_BYTES_PER_CHAR * gf_utf8_wcslen(src_str) );
			char *utf8str = (char *)gf_malloc(len + 1);
			u32 res_len = (u32) gf_utf8_wcstombs(utf8str, len, (const unsigned short **) &src_str);
			utf8str[res_len] = 0;
			fprintf(stderr, "%s\n", utf8str);

			gf_free(utf8str);
		}
	}


	print_udta(file, 0, has_itags);
	fprintf(stderr, "\n");
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		DumpTrackInfo(file, i+1, 0, GF_TRUE, dump_m4sys);
	}
}

#endif /*defined(GPAC_DISABLE_ISOM) || defined(GPAC_DISABLE_ISOM_WRITE)*/


#ifndef GPAC_DISABLE_MPEG2TS


typedef struct
{
	/* when writing to file */
	FILE *pes_out;
	char dump[100];
#if 0
	FILE *pes_out_nhml;
	char nhml[100];
	FILE *pes_out_info;
	char info[100];
#endif
	Bool is_info_dumped;

	u32 prog_number;
	/* For logging timing information (PCR, PTS/DTS) */
	FILE *timestamps_info_file;
	char timestamps_info_name[100];

	/* when dumping TS information */
	u32 dump_pid;
	Bool has_seen_pat;

} GF_M2TS_Dump;



static void on_m2ts_dump_event(GF_M2TS_Demuxer *ts, u32 evt_type, void *par)
{
	u32 i, count;
	GF_M2TS_Program *prog;
	GF_M2TS_PES_PCK *pck;
	GF_M2TS_Dump *dumper = (GF_M2TS_Dump *)ts->user;

	switch (evt_type) {
	case GF_M2TS_EVT_PAT_FOUND:
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, 0);
		}
		break;
	case GF_M2TS_EVT_PAT_UPDATE:
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, 0);
		}
		break;
	case GF_M2TS_EVT_PAT_REPEAT:
		/* WARNING: We detect the pat on a repetition, probably to ensure that we also have seen all the PMT
		   To be checked */
		dumper->has_seen_pat = 1;
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, 0);
		}
//		fprintf(stderr, "Repeated PAT found - %d programs\n", gf_list_count(ts->programs) );
		break;
	case GF_M2TS_EVT_CAT_FOUND:
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, 0);
		}
		break;
	case GF_M2TS_EVT_CAT_UPDATE:
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, 0);
		}
		break;
	case GF_M2TS_EVT_CAT_REPEAT:
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, 0);
		}
		break;
	case GF_M2TS_EVT_PMT_FOUND:
		prog = (GF_M2TS_Program*)par;
		if (gf_list_count(ts->programs)>1 && prog->number!=dumper->prog_number)
			break;

		count = gf_list_count(prog->streams);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Program number %d found - %d streams:\n", prog->number, count));
		for (i=0; i<count; i++) {
			GF_M2TS_ES *es = gf_list_get(prog->streams, i);
			if (es->pid == prog->pmt_pid) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("\tPID %d: Program Map Table\n", es->pid));
			} else {
				GF_M2TS_PES *pes = (GF_M2TS_PES *)es;
				gf_m2ts_set_pes_framing(pes, dumper->pes_out ? GF_M2TS_PES_FRAMING_RAW : GF_M2TS_PES_FRAMING_DEFAULT);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("\tPID %d: %s ", pes->pid, gf_m2ts_get_stream_name(pes->stream_type) ));
				if (pes->mpeg4_es_id) GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (" - MPEG-4 ES ID %d", pes->mpeg4_es_id));
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("\n"));
			}
		}
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, prog->pmt_pid);
		}
		break;
	case GF_M2TS_EVT_PMT_UPDATE:
		prog = (GF_M2TS_Program*)par;
		if (gf_list_count(ts->programs)>1 && prog->number!=dumper->prog_number)
			break;
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, prog->pmt_pid);
		}
		break;
	case GF_M2TS_EVT_PMT_REPEAT:
		prog = (GF_M2TS_Program*)par;
		if (gf_list_count(ts->programs)>1 && prog->number!=dumper->prog_number)
			break;
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, prog->pmt_pid);
		}
		break;
	case GF_M2TS_EVT_SDT_FOUND:
#ifndef GPAC_DISABLE_LOG
		count = gf_list_count(ts->SDTs) ;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Program Description found - %d desc:\n", count));
		for (i=0; i<count; i++) {
			GF_M2TS_SDT *sdt = gf_list_get(ts->SDTs, i);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("\tServiceID %d - Provider %s - Name %s\n", sdt->service_id, sdt->provider, sdt->service));
		}
#endif
		break;
	case GF_M2TS_EVT_SDT_UPDATE:
#ifndef GPAC_DISABLE_LOG
		count = gf_list_count(ts->SDTs) ;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Program Description updated - %d desc\n", count));
		for (i=0; i<count; i++) {
			GF_M2TS_SDT *sdt = gf_list_get(ts->SDTs, i);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("\tServiceID %d - Provider %s - Name %s\n", sdt->service_id, sdt->provider, sdt->service));
		}
#endif
		break;
	case GF_M2TS_EVT_SDT_REPEAT:
		break;
	case GF_M2TS_EVT_PES_TIMING:
		pck = par;
		if (gf_list_count(ts->programs)>1 && pck->stream->program->number != dumper->prog_number)
			break;

		break;
	case GF_M2TS_EVT_PES_PCK:
		pck = par;
		if (gf_list_count(ts->programs)>1 && pck->stream->program->number != dumper->prog_number)
			break;
		if (dumper->has_seen_pat) {

			/*We need the interpolated PCR for the pcrb, hence moved this calculus out, and saving the calculated value in index_info to put it in the pcrb*/
			GF_M2TS_PES *pes = pck->stream;
			/*FIXME : not used GF_M2TS_Program *prog = pes->program; */
			/* Interpolated PCR value for the TS packet containing the PES header start */
			u64 interpolated_pcr_value = 0;
			if (pes->last_pcr_value && pes->before_last_pcr_value_pck_number && pes->last_pcr_value > pes->before_last_pcr_value) {
				u32 delta_pcr_pck_num = pes->last_pcr_value_pck_number - pes->before_last_pcr_value_pck_number;
				u32 delta_pts_pcr_pck_num = pes->pes_start_packet_number - pes->last_pcr_value_pck_number;
				u64 delta_pcr_value = pes->last_pcr_value - pes->before_last_pcr_value;
				if ((pes->pes_start_packet_number > pes->last_pcr_value_pck_number)
				        && (pes->last_pcr_value > pes->before_last_pcr_value)) {

					pes->last_pcr_value = pes->before_last_pcr_value;
				}
				/* we can compute the interpolated pcr value for the packet containing the PES header */
				interpolated_pcr_value = pes->last_pcr_value + (u64)((delta_pcr_value*delta_pts_pcr_pck_num*1.0)/delta_pcr_pck_num);
			}

			if (dumper->timestamps_info_file) {
				Double diff;
				fprintf(dumper->timestamps_info_file, "%u\t%d\t", pck->stream->pes_start_packet_number, pck->stream->pid);
				if (interpolated_pcr_value) fprintf(dumper->timestamps_info_file, "%f", interpolated_pcr_value/(300.0 * 90000));
				fprintf(dumper->timestamps_info_file, "\t");
				if (pck->DTS) fprintf(dumper->timestamps_info_file, "%f", (pck->DTS / 90000.0));
				fprintf(dumper->timestamps_info_file, "\t%f\t%d\t%d", pck->PTS / 90000.0, (pck->flags & GF_M2TS_PES_PCK_RAP) ? 1 : 0, (pck->flags & GF_M2TS_PES_PCK_DISCONTINUITY) ? 1 : 0);
				if (interpolated_pcr_value) {
					diff = (pck->DTS ? pck->DTS : pck->PTS) / 90000.0;
					diff -= pes->last_pcr_value / (300.0 * 90000);
					fprintf(dumper->timestamps_info_file, "\t%f\n", diff);
					if (diff<0) {
						M4_LOG(GF_LOG_WARNING, ("Warning: detected PTS/DTS value less than current PCR of %g sec\n", diff));
					}
				} else {
					fprintf(dumper->timestamps_info_file, "\t\n");
				}
			}
		}

		if (dumper->has_seen_pat && dumper->pes_out && (dumper->dump_pid == pck->stream->pid)) {
			gf_fwrite(pck->data, pck->data_len, dumper->pes_out);
		}
		break;
	case GF_M2TS_EVT_PES_PCR:
		pck = par;
		if (gf_list_count(ts->programs)>1 && pck->stream->program->number != dumper->prog_number)
			break;
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\t%f\t\t\t\t%d\n", pck->stream->program->last_pcr_value_pck_number, pck->stream->pid, pck->PTS / (300*90000.0), (pck->flags & GF_M2TS_PES_PCK_DISCONTINUITY) ? 1 : 0);
		}
		break;
	case GF_M2TS_EVT_SL_PCK:
#if 0
	{
		GF_M2TS_SL_PCK *sl_pck = par;
		if (dumper->pes_out && (dumper->dump_pid == sl_pck->stream->pid)) {
			GF_SLHeader header;
			u32 header_len;
			if (sl_pck->stream->mpeg4_es_id) {
				GF_ESD *esd = ((GF_M2TS_PES*)sl_pck->stream)->esd;
				if (!dumper->is_info_dumped) {
					if (esd->decoderConfig->decoderSpecificInfo) gf_fwrite(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, dumper->pes_out_info);
					dumper->is_info_dumped = 1;
					fprintf(dumper->pes_out_nhml, "<NHNTStream version=\"1.0\" ");
					fprintf(dumper->pes_out_nhml, "timeScale=\"%d\" ", esd->slConfig->timestampResolution);
					fprintf(dumper->pes_out_nhml, "streamType=\"%d\" ", esd->decoderConfig->streamType);
					fprintf(dumper->pes_out_nhml, "objectTypeIndication=\"%d\" ", esd->decoderConfig->objectTypeIndication);
					if (esd->decoderConfig->decoderSpecificInfo) fprintf(dumper->pes_out_nhml, "specificInfoFile=\"%s\" ", dumper->info);
					fprintf(dumper->pes_out_nhml, "baseMediaFile=\"%s\" ", dumper->dump);
					fprintf(dumper->pes_out_nhml, "inRootOD=\"yes\">\n");
				}
				gf_sl_depacketize(esd->slConfig, &header, sl_pck->data, sl_pck->data_len, &header_len);
				gf_fwrite(sl_pck->data+header_len, sl_pck->data_len-header_len, dumper->pes_out);
				fprintf(dumper->pes_out_nhml, "<NHNTSample DTS=\""LLD"\" dataLength=\"%d\" isRAP=\"%s\"/>\n", header.decodingTimeStamp, sl_pck->data_len-header_len, (header.randomAccessPointFlag?"yes":"no"));
			}
		}
	}
#endif
	break;
	}
}

void dump_mpeg2_ts(char *mpeg2ts_file, char *out_name, Bool prog_num)
{
	u8 data[188];
	GF_M2TS_Dump dumper;

	u32 size;
	u64 fsize, fdone;
	GF_M2TS_Demuxer *ts;
	FILE *src;

	if (!prog_num && !out_name) {
		fprintf(stderr, "No program number nor output filename specified. No timestamp file will be generated.");
	}

	src = gf_fopen(mpeg2ts_file, "rb");
	if (!src) {
		M4_LOG(GF_LOG_ERROR, ("Cannot open %s: no such file\n", mpeg2ts_file));
		return;
	}
	ts = gf_m2ts_demux_new();
	ts->on_event = on_m2ts_dump_event;
	ts->notify_pes_timing = 1;
	memset(&dumper, 0, sizeof(GF_M2TS_Dump));
	ts->user = &dumper;
	dumper.prog_number = prog_num;

	/*PES dumping*/
	if (out_name) {
		char *pid = strrchr(out_name, '#');
		if (pid) {
			dumper.dump_pid = atoi(pid+1);
			pid[0] = 0;
			sprintf(dumper.dump, "%s_%d.raw", out_name, dumper.dump_pid);
			dumper.pes_out = gf_fopen(dumper.dump, "wb");
#if 0
			sprintf(dumper.nhml, "%s_%d.nhml", pes_out_name, dumper.dump_pid);
			dumper.pes_out_nhml = gf_fopen(dumper.nhml, "wt");
			sprintf(dumper.info, "%s_%d.info", pes_out_name, dumper.dump_pid);
			dumper.pes_out_info = gf_fopen(dumper.info, "wb");
#endif
			pid[0] = '#';
		}
	}

	gf_fseek(src, 0, SEEK_END);
	fsize = gf_ftell(src);
	gf_fseek(src, 0, SEEK_SET);

	/* first loop to process all packets between two PAT, and assume all signaling was found between these 2 PATs */
	while (!feof(src)) {
		size = (u32) gf_fread(data, 188, src);
		if (size<188) break;

		gf_m2ts_process_data(ts, data, size);
		if (dumper.has_seen_pat) break;
	}
	dumper.has_seen_pat = GF_TRUE;

	if (!prog_num) {
		GF_M2TS_Program *p = gf_list_get(ts->programs, 0);
		if (p) prog_num = p->number;
		fprintf(stderr, "No program number specified, defaulting to first program\n");
	}

	if (!prog_num && !out_name) {
		fprintf(stderr, "No program number nor output filename specified. No timestamp file will be generated\n");
	}

	if (prog_num) {
		sprintf(dumper.timestamps_info_name, "%s_prog_%d_timestamps.txt", mpeg2ts_file, prog_num/*, mpeg2ts_file*/);
		dumper.timestamps_info_file = gf_fopen(dumper.timestamps_info_name, "wt");
		if (!dumper.timestamps_info_file) {
			M4_LOG(GF_LOG_ERROR, ("Cannot open file %s\n", dumper.timestamps_info_name));
			return;
		}
		fprintf(dumper.timestamps_info_file, "PCK#\tPID\tPCR\tDTS\tPTS\tRAP\tDiscontinuity\tDTS-PCR Diff\n");
	}

	gf_m2ts_reset_parsers(ts);
	gf_fseek(src, 0, SEEK_SET);
	fdone = 0;

	while (!feof(src)) {
		size = (u32) gf_fread(data, 188, src);
		if (size<188) break;

		gf_m2ts_process_data(ts, data, size);

		fdone += size;
		gf_set_progress("MPEG-2 TS Parsing", fdone, fsize);
	}

	gf_fclose(src);
	gf_m2ts_demux_del(ts);
	if (dumper.pes_out) gf_fclose(dumper.pes_out);
#if 0
	if (dumper.pes_out_nhml) {
		if (dumper.is_info_dumped) fprintf(dumper.pes_out_nhml, "</NHNTStream>\n");
		gf_fclose(dumper.pes_out_nhml);
		gf_fclose(dumper.pes_out_info);
	}
#endif
	if (dumper.timestamps_info_file) gf_fclose(dumper.timestamps_info_file);
}

#endif /*GPAC_DISABLE_MPEG2TS*/


#include <gpac/download.h>
#include <gpac/mpd.h>

void get_file_callback(void *usr_cbk, GF_NETIO_Parameter *parameter)
{
	if (parameter->msg_type==GF_NETIO_DATA_EXCHANGE) {
		u64 tot_size, done, max;
		u32 bps;
		gf_dm_sess_get_stats(parameter->sess, NULL, NULL, &tot_size, &done, &bps, NULL);
		if (tot_size) {
			max = done;
			max *= 100;
			max /= tot_size;
			fprintf(stderr, "download %02d %% at %05d kpbs\r", (u32) max, bps*8/1000);
		}
	}
}

static GF_DownloadSession *get_file(const char *url, GF_DownloadManager *dm, GF_Err *e)
{
	GF_DownloadSession *sess;
	sess = gf_dm_sess_new(dm, url, GF_NETIO_SESSION_NOT_THREADED, get_file_callback, NULL, e);
	if (!sess) return NULL;
	*e = gf_dm_sess_process(sess);
	if (*e) {
		gf_dm_sess_del(sess);
		return NULL;
	}
	return sess;
}

static void revert_cache_file(char *item_path)
{
	char szPATH[GF_MAX_PATH];
	const char *url;
	GF_Config *cached;

	if (!strstr(item_path, "gpac_cache_")) {
		fprintf(stderr, "%s is not a gpac cache file\n", item_path);
		return;
	}
	if (!strncmp(item_path, "./", 2) || !strncmp(item_path, ".\\", 2))
			item_path += 2;

 	strcpy(szPATH, item_path);
	strcat(szPATH, ".txt");

	cached = gf_cfg_new(NULL, szPATH);
	url = gf_cfg_get_key(cached, "cache", "url");
	if (url) url = strstr(url, "://");
	if (url) {
		u32 i, len, dir_len=0, k=0;
		char *sep;
		char *dst_name;
		sep = strstr(item_path, "gpac_cache_");
		if (sep) {
			sep[0] = 0;
			dir_len = (u32) strlen(item_path);
			sep[0] = 'g';
		}
		url+=3;
		len = (u32) strlen(url);
		dst_name = gf_malloc(len+dir_len+1);
		memset(dst_name, 0, len+dir_len+1);

		strncpy(dst_name, item_path, dir_len);
		k=dir_len;
		for (i=0; i<len; i++) {
			dst_name[k] = url[i];
			if (dst_name[k]==':') dst_name[k]='_';
			else if (dst_name[k]=='/') {
				if (!gf_dir_exists(dst_name))
					gf_mkdir(dst_name);
			}
			k++;
		}
		if (gf_file_exists(item_path)) {
			gf_file_move(item_path, dst_name);
		}

		gf_free(dst_name);
	} else {
		M4_LOG(GF_LOG_ERROR, ("Failed to reverse %s cache file\n", item_path));
	}
	gf_cfg_del(cached);
	gf_file_delete(szPATH);
}

GF_Err rip_mpd(const char *mpd_src, const char *output_dir)
{
	GF_DownloadSession *sess;
	u32 i, connect_time, reply_time, download_time, req_hdr_size, rsp_hdr_size;
	GF_Err e;
	GF_DOMParser *mpd_parser=NULL;
	GF_MPD *mpd=NULL;
	GF_MPD_Period *period;
	GF_MPD_AdaptationSet *as;
	GF_MPD_Representation *rep;
	char szName[GF_MAX_PATH];
	GF_DownloadManager *dm;

	if (output_dir) {
		char *sep;
		strcpy(szName, output_dir);
		sep = gf_file_basename(szName);
		if (sep) sep[0] = 0;
		gf_opts_set_key("temp", "cache", szName);
	} else {
		gf_opts_set_key("temp", "cache", ".");
	}
	gf_opts_set_key("temp", "clean-cache", "true");
	dm = gf_dm_new(NULL);

/*
	char *name = strrchr(mpd_src, '/');
	if (!name) name = strrchr(mpd_src, '\\');
	if (!name) name = "manifest.mpd";
	else name ++;

	if (strchr(name, '?') || strchr(name, '&')) name = "manifest.mpd";
*/

	fprintf(stderr, "Downloading %s\n", mpd_src);
	sess = get_file(mpd_src, dm, &e);
	if (!sess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Error downloading MPD file %s: %s\n", mpd_src, gf_error_to_string(e) ));
		goto err_exit;
	}
	strcpy(szName, gf_dm_sess_get_cache_name(sess) );
	gf_dm_sess_get_header_sizes_and_times(sess, &req_hdr_size, &rsp_hdr_size, &connect_time, &reply_time, &download_time);
	gf_dm_sess_del(sess);

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Error fetching MPD file %s: %s\n", mpd_src, gf_error_to_string(e)));
		goto err_exit;
	}
	else {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Fetched file %s\n", mpd_src));
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_APP, ("GET Header size %d - Reply header size %d\n", req_hdr_size, rsp_hdr_size));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_APP, ("GET time: Connect Time %d - Reply Time %d - Download Time %d\n", connect_time, reply_time, download_time));

	mpd_parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(mpd_parser, szName, NULL, NULL);

	if (e != GF_OK) {
		gf_xml_dom_del(mpd_parser);
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Error parsing MPD %s : %s\n", mpd_src, gf_error_to_string(e)));
		return e;
	}
	mpd = gf_mpd_new();
	e = gf_mpd_init_from_dom(gf_xml_dom_get_root(mpd_parser), mpd, mpd_src);
	gf_xml_dom_del(mpd_parser);
	mpd_parser=NULL;
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Error initializing MPD %s : %s\n", mpd_src, gf_error_to_string(e)));
		goto err_exit;
	}
	else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_APP, ("MPD %s initialized: %s\n", szName, gf_error_to_string(e)));
	}

	revert_cache_file(szName);
	if (mpd->type==GF_MPD_TYPE_DYNAMIC) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("MPD rip is not supported on live sources\n"));
		e = GF_NOT_SUPPORTED;
		goto err_exit;
	}

	i=0;
	while ((period = (GF_MPD_Period *) gf_list_enum(mpd->periods, &i))) {
		char *initTemplate = NULL;
		Bool segment_base = GF_FALSE;
		u32 j=0;

		if (period->segment_base) segment_base=GF_TRUE;

		if (period->segment_template && period->segment_template->initialization) {
			initTemplate = period->segment_template->initialization;
		}

		while ((as = gf_list_enum(period->adaptation_sets, &j))) {
			u32 k=0;
			if (!initTemplate && as->segment_template && as->segment_template->initialization) {
				initTemplate = as->segment_template->initialization;
			}
			if (as->segment_base) segment_base=GF_TRUE;

			while ((rep = gf_list_enum(as->representations, &k))) {
				u64 out_range_start, out_range_end, segment_duration;
				Bool is_in_base_url;
				char *seg_url;
				u32 seg_idx=0;
				if (rep->segment_template && rep->segment_template->initialization) {
					initTemplate = rep->segment_template->initialization;
				} else if (k>1) {
					initTemplate = NULL;
				}
				if (rep->segment_base) segment_base=GF_TRUE;

				e = gf_mpd_resolve_url(mpd, rep, as, period, mpd_src, 0, GF_MPD_RESOLVE_URL_INIT, 0, 0, &seg_url, &out_range_start, &out_range_end, &segment_duration, &is_in_base_url, NULL, NULL, NULL);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Error resolving init segment name : %s\n", gf_error_to_string(e)));
					continue;
				}
				//not a byte range, replace URL
				if (segment_base) {

				} else if (out_range_start || out_range_end || !seg_url) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("byte range rip not yet implemented\n"));
					if (seg_url) gf_free(seg_url);
					e = GF_NOT_SUPPORTED;
					goto err_exit;
				}

				fprintf(stderr, "Downloading %s\n", seg_url);
				sess = get_file(seg_url, dm, &e);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Error downloading init segment %s from MPD %s : %s\n", seg_url, mpd_src, gf_error_to_string(e)));
					goto err_exit;
				}
				revert_cache_file((char *) gf_dm_sess_get_cache_name(sess) );
				gf_free(seg_url);
				gf_dm_sess_del(sess);

				if (segment_base) continue;

				while (1) {
					e = gf_mpd_resolve_url(mpd, rep, as, period, mpd_src, 0, GF_MPD_RESOLVE_URL_MEDIA, seg_idx, 0, &seg_url, &out_range_start, &out_range_end, &segment_duration, NULL, NULL, NULL, NULL);
					if (e) {
						if (e<0) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Error resolving segment name : %s\n", gf_error_to_string(e)));
						}
						break;
					}

					seg_idx++;

					if (out_range_start || out_range_end || !seg_url) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("byte range rip not yet implemented\n"));
						if (seg_url) gf_free(seg_url);
						break;
					}
					fprintf(stderr, "Downloading %s\n", seg_url);
					sess = get_file(seg_url, dm, &e);
					if (e) {
						gf_free(seg_url);
						if (e != GF_URL_ERROR) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Error downloading segment %s: %s\n", seg_url, gf_error_to_string(e)));
						} else {
							//todo, properly detect end of dash representation
							e = GF_OK;
						}
						break;
					}
					revert_cache_file((char *) gf_dm_sess_get_cache_name(sess) );
					gf_free(seg_url);
					gf_dm_sess_del(sess);
				}
			}
		}
	}

err_exit:
	if (mpd) gf_mpd_del(mpd);
	gf_dm_del(dm);
	return e;
}
