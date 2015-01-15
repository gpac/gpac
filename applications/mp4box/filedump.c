/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#include <gpac/tools.h>

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
/*for asctime and gmtime*/
#include <time.h>
/*ISO 639 languages*/
#include <gpac/iso639.h>
#include <gpac/mpegts.h>

#ifndef GPAC_DISABLE_SMGR
#include <gpac/scene_manager.h>
#endif
#include <gpac/internal/media_dev.h>

extern u32 swf_flags;
extern Float swf_flatten_angle;
extern GF_FileType get_file_type_by_ext(char *inName);

void scene_coding_log(void *cbk, u32 log_level, u32 log_tool, const char *fmt, va_list vlist);

#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
GF_Err import_file(GF_ISOFile *dest, char *inName, u32 import_flags, Double force_fps, u32 frames_per_sample);
#endif

void PrintLanguages()
{
	u32 i=0, count = gf_lang_get_count();
	fprintf(stderr, "Supported ISO 639 languages and codes:\n\n");
	for (i=0; i<count; i++) {
		if (gf_lang_get_2cc(i)) {
			fprintf(stderr, "%s (%s - %s)\n", gf_lang_get_name(i), gf_lang_get_3cc(i), gf_lang_get_2cc(i));
		}
	}
}

static const char *GetLanguage(char *lcode)
{
	s32 idx = gf_lang_find(lcode);
	if (idx>=0) return gf_lang_get_name(idx);
	return lcode;
}

GF_Err dump_cover_art(GF_ISOFile *file, char *inName)
{
	const char *tag;
	char szName[1024];
	FILE *t;
	u32 tag_len;
	GF_Err e = gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_COVER_ART, &tag, &tag_len);
	if (e!=GF_OK) {
		if (e==GF_URL_ERROR) {
			fprintf(stderr, "No cover art found\n");
			return GF_OK;
		}
		return e;
	}

	sprintf(szName, "%s.%s", inName, (tag_len>>31) ? "png" : "jpg");
	t = gf_f64_open(szName, "wb");
	gf_fwrite(tag, tag_len & 0x7FFFFFFF, 1, t);

	fclose(t);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err set_cover_art(GF_ISOFile *file, char *inName)
{
	GF_Err e;
	char *tag, *ext;
	FILE *t;
	u32 tag_len;
	t = gf_f64_open(inName, "rb");
	gf_f64_seek(t, 0, SEEK_END);
	tag_len = (u32) gf_f64_tell(t);
	gf_f64_seek(t, 0, SEEK_SET);
	tag = gf_malloc(sizeof(char) * tag_len);
	tag_len = (u32) fread(tag, sizeof(char), tag_len, t);
	fclose(t);

	ext = strrchr(inName, '.');
	if (!stricmp(ext, ".png")) tag_len |= 0x80000000;
	e = gf_isom_apple_set_tag(file, GF_ISOM_ITUNE_COVER_ART, tag, tag_len);
	gf_free(tag);
	return e;
}

#endif

#ifndef GPAC_DISABLE_SCENE_DUMP

GF_Err dump_file_text(char *file, char *inName, GF_SceneDumpFormat dump_mode, Bool do_log)
{
	GF_Err e;
	GF_SceneManager *ctx;
	GF_SceneGraph *sg;
	GF_SceneLoader load;
	GF_FileType ftype;
	gf_log_cbk prev_logs = NULL;
	FILE *logs = NULL;
	e = GF_OK;

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
			fprintf(stderr, "Error opening file: %s\n", gf_error_to_string(e));
			gf_sm_del(ctx);
			gf_sg_del(sg);
			return e;
		}
	} else if (ftype==GF_FILE_TYPE_LSR_SAF) {
		load.isom = gf_isom_open("saf_conv", GF_ISOM_WRITE_EDIT, NULL);
#ifndef GPAC_DISABLE_MEDIA_IMPORT
		if (load.isom)
			e = import_file(load.isom, file, 0, 0, 0);
		else
#else
		fprintf(stderr, "Warning: GPAC was compiled without Media Import support\n");
#endif
			e = gf_isom_last_error(NULL);

		if (e) {
			fprintf(stderr, "Error importing file: %s\n", gf_error_to_string(e));
			gf_sm_del(ctx);
			gf_sg_del(sg);
			if (load.isom) gf_isom_delete(load.isom);
			return e;
		}
	}

	if (do_log) {
		char szLog[GF_MAX_PATH];
		sprintf(szLog, "%s_dec.logs", inName);
		logs = gf_f64_open(szLog, "wt");

		gf_log_set_tool_level(GF_LOG_CODING, GF_LOG_DEBUG);
		prev_logs = gf_log_set_callback(logs, scene_coding_log);
	}
	e = gf_sm_load_init(&load);
	if (!e) e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);
	if (logs) {
		gf_log_set_tool_level(GF_LOG_CODING, GF_LOG_ERROR);
		gf_log_set_callback(NULL, prev_logs);
		fclose(logs);
	}
	if (!e && dump_mode != GF_SM_DUMP_SVG) {
		u32 count = gf_list_count(ctx->streams);
		if (count)
			fprintf(stderr, "Scene loaded - dumping %d systems streams\n", count);
		else
			fprintf(stderr, "Scene loaded - dumping root scene\n");

		e = gf_sm_dump(ctx, inName, dump_mode);
	}

	gf_sm_del(ctx);
	gf_sg_del(sg);
	if (e) fprintf(stderr, "Error loading scene: %s\n", gf_error_to_string(e));
	if (load.isom) gf_isom_delete(load.isom);
	return e;
}
#endif

#ifndef GPAC_DISABLE_SCENE_STATS

static void dump_stats(FILE *dump, GF_SceneStatistics *stats)
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

void dump_scene_stats(char *file, char *inName, u32 stat_level)
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
			fprintf(stderr, "Cannot open file: %s\n", gf_error_to_string(gf_isom_last_error(NULL)));
			gf_sm_del(ctx);
			gf_sg_del(scene_graph);
			return;
		}
	}

	e = gf_sm_load_init(&load);
	if (!e) e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);
	if (e) goto exit;

	if (inName) {
		strcpy(szBuf, inName);
		strcat(szBuf, "_stat.xml");
		dump = gf_f64_open(szBuf, "wt");
		close = 1;
	} else {
		dump = stderr;
		close = 0;
	}

	fprintf(stderr, "Analysing Scene\n");

	fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
	fprintf(dump, "<!-- Scene Graph Statistics Generated by MP4Box - GPAC " GPAC_FULL_VERSION" -->\n");
	fprintf(dump, "<SceneStatistics file=\"%s\" DumpType=\"%s\">\n", file, (stat_level==1) ? "full scene" : ((stat_level==2) ? "AccessUnit based" : "SceneGraph after each AU"));

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
			fprintf(dump, "<AUStatistics StreamID=\"%d\" AUTime=\""LLD"\">\n", au->owner->ESID, LLD_CAST au->timing);
		} else {
			fprintf(dump, "<GraphStatistics StreamID=\"%d\" AUTime=\""LLD"\">\n", au->owner->ESID, LLD_CAST au->timing);
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
	if (e) {
		fprintf(stderr, "%s\n", gf_error_to_string(e));
	} else {
		fprintf(dump, "</SceneStatistics>\n");
	}
	if (dump && close) fclose(dump);
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

void PrintNode(const char *name, u32 graph_type)
{
#ifdef GPAC_DISABLE_VRML
	fprintf(stderr, "VRML/MPEG-4/X3D scene graph is disabled in this build of GPAC\n");
	return;
#else
	const char *nname, *std_name;
	char szField[1024];
	GF_Node *node;
	GF_SceneGraph *sg;
	u32 tag, nbF, i;
	GF_FieldInfo f;
#ifndef GPAC_DISABLE_BIFS
	u8 qt, at;
	Fixed bmin, bmax;
	u32 nbBits;
#endif /*GPAC_DISABLE_BIFS*/
	Bool is_nodefield = 0;

	char *sep = strchr(name, '.');
	if (sep) {
		strcpy(szField, sep+1);
		sep[0] = 0;
		is_nodefield = 1;
	}

	tag = 0;
	if (graph_type==2) {
		fprintf(stderr, "SVG node printing is not supported\n");
		return;
	} else if (graph_type==1) {
#ifndef GPAC_DISABLE_X3D
		tag = gf_node_x3d_type_by_class_name(name);
		std_name = "X3D";
#else
		fprintf(stderr, "X3D node printing is not supported (X3D support disabled)\n");
		return;
#endif
	} else {
		tag = gf_node_mpeg4_type_by_class_name(name);
		std_name = "MPEG4";
	}
	if (!tag) {
		fprintf(stderr, "Unknown %s node %s\n", std_name, name);
		return;
	}

	sg = gf_sg_new();
	node = gf_node_new(sg, tag);
	gf_node_register(node, NULL);
	nname = gf_node_get_class_name(node);
	if (!node) {
		fprintf(stderr, "Node %s not supported in current built\n", nname);
		return;
	}
	nbF = gf_node_get_field_count(node);

	if (is_nodefield) {
		u32 tfirst, tlast;
		if (gf_node_get_field_by_name(node, szField, &f) != GF_OK) {
			fprintf(stderr, "Field %s is not a member of node %s\n", szField, name);
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

	fprintf(stderr, "Node Syntax:\n%s {\n", nname);

	for (i=0; i<nbF; i++) {
		gf_node_get_field(node, i, &f);
		if (graph_type==2) {
			fprintf(stderr, "\t%s=\"...\"\n", f.name);
			continue;
		}

		fprintf(stderr, "\t%s %s %s", gf_sg_vrml_get_event_type_name(f.eventType, 0), gf_sg_vrml_get_field_type_by_name(f.fieldType), f.name);
		if (f.fieldType==GF_SG_VRML_SFNODE) fprintf(stderr, " NULL");
		else if (f.fieldType==GF_SG_VRML_MFNODE) fprintf(stderr, " []");
		else if (gf_sg_vrml_is_sf_field(f.fieldType)) {
			fprintf(stderr, " ");
			PrintNodeSFField(f.fieldType, f.far_ptr);
		} else {
			void *ptr;
			u32 i, sftype;
			GenMFField *mffield = (GenMFField *) f.far_ptr;
			fprintf(stderr, " [");
			sftype = gf_sg_vrml_get_sf_type(f.fieldType);
			for (i=0; i<mffield->count; i++) {
				if (i) fprintf(stderr, " ");
				gf_sg_vrml_mf_get_item(f.far_ptr, f.fieldType, &ptr, i);
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
	}
	fprintf(stderr, "}\n\n");

	gf_node_unregister(node, NULL);
	gf_sg_del(sg);
#endif /*GPAC_DISABLE_VRML*/
}

void PrintBuiltInNodes(u32 graph_type)
{
#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_X3D) && !defined(GPAC_DISABLE_SVG)
	GF_Node *node;
	GF_SceneGraph *sg;
	u32 i, nb_in, nb_not_in, start_tag, end_tag;

	if (graph_type==1) {
#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_X3D)
		start_tag = GF_NODE_RANGE_FIRST_X3D;
		end_tag = TAG_LastImplementedX3D;
#else
		fprintf(stderr, "X3D scene graph disabled in this build of GPAC\n");
		return;
#endif
	} else if (graph_type==2) {
#ifdef GPAC_DISABLE_SVG
		fprintf(stderr, "SVG scene graph disabled in this build of GPAC\n");
		return;
#else
		start_tag = GF_NODE_RANGE_FIRST_SVG;
		end_tag = GF_NODE_RANGE_LAST_SVG;
#endif
	} else {
#ifdef GPAC_DISABLE_VRML
		fprintf(stderr, "VRML/MPEG-4 scene graph disabled in this build of GPAC\n");
		return;
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
		node = gf_node_new(sg, i);
		if (node) {
			gf_node_register(node, NULL);
			fprintf(stderr, " %s\n", gf_node_get_class_name(node));
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
#else
	fprintf(stderr, "\nNo scene graph enabled in this MP4Box build\n");
#endif
}

#ifndef GPAC_DISABLE_ISOM_DUMP

void dump_isom_xml(GF_ISOFile *file, char *inName)
{
	FILE *dump;
	char szBuf[1024];

	if (inName) {
		strcpy(szBuf, inName);
		strcat(szBuf, "_info.xml");
		dump = gf_f64_open(szBuf, "wt");
		gf_isom_dump(file, dump);
		fclose(dump);
	} else {
		gf_isom_dump(file, stderr);
	}
}
#endif


#if !defined(GPAC_DISABLE_ISOM_HINTING) && !defined(GPAC_DISABLE_ISOM_DUMP)

void dump_file_rtp(GF_ISOFile *file, char *inName)
{
	u32 i, j, size;
	FILE *dump;
	const char *sdp;
	char szBuf[1024];

	if (inName) {
		strcpy(szBuf, inName);
		strcat(szBuf, "_rtp.xml");
		dump = gf_f64_open(szBuf, "wt");
	} else {
		dump = stderr;
	}

	fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
	fprintf(dump, "<!-- MP4Box RTP trace -->\n");
	fprintf(dump, "<RTPFile>\n");

	for (i=0; i<gf_isom_get_track_count(file); i++) {
		if (gf_isom_get_media_type(file, i+1) != GF_ISOM_MEDIA_HINT) continue;

		fprintf(dump, "<RTPHintTrack trackID=\"%d\">\n", gf_isom_get_track_id(file, i+1));
		gf_isom_sdp_track_get(file, i+1, &sdp, &size);
		fprintf(dump, "<SDPInfo>%s</SDPInfo>", sdp);

		for (j=0; j<gf_isom_get_sample_count(file, i+1); j++) {
			gf_isom_dump_hint_sample(file, i+1, j+1, dump);
		}
		fprintf(dump, "</RTPHintTrack>\n");
	}
	fprintf(dump, "</RTPFile>\n");
	if (inName) fclose(dump);
}
#endif

void dump_file_timestamps(GF_ISOFile *file, char *inName)
{
	u32 i, j, k, count;
	Bool has_error;
	FILE *dump;
	char szBuf[1024];

	if (inName) {
		strcpy(szBuf, inName);
		strcat(szBuf, "_ts.txt");
		dump = gf_f64_open(szBuf, "wt");
	} else {
		dump = stderr;
	}

	has_error = 0;
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		u32 has_cts_offset = gf_isom_has_time_offset(file, i+1);

		fprintf(dump, "#dumping track ID %d timing: Num DTS CTS Size RAP Offset isLeading DependsOn DependedOn Redundant RAP-SampleGroup Roll-SampleGroup Roll-Distance\n", gf_isom_get_track_id(file, i+1));
		count = gf_isom_get_sample_count(file, i+1);

		for (j=0; j<count; j++) {
			u64 dts, cts, offset;
			u32 isLeading, dependsOn, dependedOn, redundant;
			Bool is_rap, has_roll;
			s32 roll_distance;
			u32 index;
			GF_ISOSample *samp = gf_isom_get_sample_info(file, i+1, j+1, &index, &offset);
			gf_isom_get_sample_flags(file, i+1, j+1, &isLeading, &dependsOn, &dependedOn, &redundant);
			gf_isom_get_sample_rap_roll_info(file, i+1, j+1, &is_rap, &has_roll, &roll_distance);
			dts = samp->DTS;
			cts = dts + (s32) samp->CTS_Offset;
			fprintf(dump, "Sample %d\tDTS "LLD"\tCTS "LLD"\t%d\t%d\t"LLD"\t%d\t%d\t%d\t%d\t%d\t%d\t%d", j+1, LLD_CAST dts, LLD_CAST cts, samp->dataLength, samp->IsRAP, offset, isLeading, dependsOn, dependedOn, redundant, is_rap, has_roll, roll_distance);
			if (cts<dts) {
				fprintf(dump, " #NEGATIVE CTS OFFSET!!!");
				has_error = 1;
			}

			gf_isom_sample_del(&samp);

			if (has_cts_offset) {
				for (k=0; k<count; k++) {
					u64 adts, acts;
					if (k==j) continue;
					samp = gf_isom_get_sample_info(file, i+1, k+1, NULL, NULL);
					adts = samp->DTS;
					acts = adts + (s32) samp->CTS_Offset;

					if (adts==dts) {
						fprintf(dump, " #SAME DTS USED!!!");
						has_error = 1;
					}
					if (acts==cts) {
						fprintf(dump, " #SAME CTS USED!!! ");
						has_error = 1;
					}

					gf_isom_sample_del(&samp);
				}
			}

			fprintf(dump, "\n");
			gf_set_progress("Analysing Track Timing", j+1, count);
		}
		fprintf(dump, "\n\n");
		gf_set_progress("Analysing Track Timing", count, count);
	}
	if (inName) fclose(dump);
	if (has_error) fprintf(stderr, "\tFile has CTTS table errors\n");
}


#ifndef GPAC_DISABLE_AV_PARSERS

static u32 read_nal_size_hdr(char *ptr, u32 nalh_size)
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

static void dump_nalu(FILE *dump, char *ptr, u32 ptr_size, Bool is_svc, Bool is_hevc, AVCState *avc, u32 nalh_size)
{
	s32 res;
	u8 type;
	u8 dependency_id, quality_id, temporal_id;
	u8 track_ref_index;
	s8 sample_offset;
	u32 data_offset, idx, data_size;
	GF_BitStream *bs;

	if (is_hevc) {
#ifndef GPAC_DISABLE_HEVC
		type = (ptr[0]  & 0x7E) >> 1;
		fprintf(dump, "code=\"%d\" type=\"", type);
		switch (type) {
		case GF_HEVC_NALU_SLICE_TRAIL_N:
			fputs("TRAIL_N slice segment", dump);
			break;
		case GF_HEVC_NALU_SLICE_TRAIL_R:
			fputs("TRAIL_R slice segment", dump);
			break;
		case GF_HEVC_NALU_SLICE_TSA_N:
			fputs("TSA_N slice segment", dump);
			break;
		case GF_HEVC_NALU_SLICE_TSA_R:
			fputs("TSA_R slice segment", dump);
			break;
		case GF_HEVC_NALU_SLICE_STSA_N:
			fputs("STSA_N slice segment", dump);
			break;
		case GF_HEVC_NALU_SLICE_STSA_R:
			fputs("STSA_R slice segment", dump);
			break;
		case GF_HEVC_NALU_SLICE_RADL_N:
			fputs("RADL_N slice segment", dump);
			break;
		case GF_HEVC_NALU_SLICE_RADL_R:
			fputs("RADL_R slice segment", dump);
			break;
		case GF_HEVC_NALU_SLICE_RASL_N:
			fputs("RASL_N slice segment", dump);
			break;
		case GF_HEVC_NALU_SLICE_RASL_R:
			fputs("RASL_R slice segment", dump);
			break;
		case GF_HEVC_NALU_SLICE_BLA_W_LP:
			fputs("Broken link access slice (W LP)", dump);
			break;
		case GF_HEVC_NALU_SLICE_BLA_W_DLP:
			fputs("Broken link access slice (W DLP)", dump);
			break;
		case GF_HEVC_NALU_SLICE_BLA_N_LP:
			fputs("Broken link access slice (N LP)", dump);
			break;
		case GF_HEVC_NALU_SLICE_IDR_W_DLP:
			fputs("IDR slice (W DLP)", dump);
			break;
		case GF_HEVC_NALU_SLICE_IDR_N_LP:
			fputs("IDR slice (N LP)", dump);
			break;
		case GF_HEVC_NALU_SLICE_CRA:
			fputs("CRA slice", dump);
			break;

		case GF_HEVC_NALU_VID_PARAM:
			fputs("Video Parameter Set", dump);
			break;
		case GF_HEVC_NALU_SEQ_PARAM:
			fputs("Sequence Parameter Set", dump);
			break;
		case GF_HEVC_NALU_PIC_PARAM:
			fputs("Picture Parameter Set", dump);
			break;
		case GF_HEVC_NALU_ACCESS_UNIT:
			fputs("AU Delimiter", dump);
			break;
		case GF_HEVC_NALU_END_OF_SEQ:
			fputs("End of Sequence", dump);
			break;
		case GF_HEVC_NALU_END_OF_STREAM:
			fputs("End of Stream", dump);
			break;
		case GF_HEVC_NALU_FILLER_DATA:
			fputs("Filler Data", dump);
			break;
		case GF_HEVC_NALU_SEI_PREFIX:
			fputs("SEI Prefix", dump);
			break;
		case GF_HEVC_NALU_SEI_SUFFIX:
			fputs("SEI Suffix", dump);
			break;
		case 48:
			fputs("HEVCAggregator", dump);
			break;
		case 49:
			fputs("HEVCExtractor", dump);
			track_ref_index = (u8) ptr[2];
			sample_offset = (s8) ptr[3];
			data_offset = read_nal_size_hdr(&ptr[4], nalh_size);
			data_size = read_nal_size_hdr(&ptr[4+nalh_size], nalh_size);
			fprintf(dump, "\" track_ref_index=\"%d\" sample_offset=\"%d\" data_offset=\"%d\" data_size=\"%d", track_ref_index, sample_offset, data_offset, data_size);
			break;
		default:
			fputs("UNKNOWN", dump);
			break;
		}
		fputs("\"", dump);
		fprintf(dump, " layer_id=\"%d\" temporal_id=\"%d\"", ((ptr[0] & 0x1) << 5) | (ptr[1]>>3), (ptr[1] & 0x7) );
#endif //GPAC_DISABLE_HEVC
		return;
	}

	bs = gf_bs_new(ptr, ptr_size, GF_BITSTREAM_READ);
	type = ptr[0] & 0x1F;
	fprintf(dump, "code=\"%d\" type=\"", type);
	res = 0;
	switch (type) {
	case GF_AVC_NALU_NON_IDR_SLICE:
		res = gf_media_avc_parse_nalu(bs, ptr[0], avc);
		fputs("Non IDR slice", dump);

		if (res>=0)
			fprintf(dump, "\" poc=\"%d", avc->s_info.poc);
		break;
	case GF_AVC_NALU_DP_A_SLICE:
		fputs("DP Type A slice", dump);
		break;
	case GF_AVC_NALU_DP_B_SLICE:
		fputs("DP Type B slice", dump);
		break;
	case GF_AVC_NALU_DP_C_SLICE:
		fputs("DP Type C slice", dump);
		break;
	case GF_AVC_NALU_IDR_SLICE:
		res = gf_media_avc_parse_nalu(bs, ptr[0], avc);
		fputs("IDR slice", dump);
		if (res>=0)
			fprintf(dump, "\" poc=\"%d", avc->s_info.poc);
		break;
	case GF_AVC_NALU_SEI:
		fputs("SEI Message", dump);
		break;
	case GF_AVC_NALU_SEQ_PARAM:
		fputs("SequenceParameterSet", dump);
		idx = gf_media_avc_read_sps(ptr, ptr_size, avc, 0, NULL);
		assert (idx >= 0);
		fprintf(dump, "\" sps_id=\"%d", idx);
		break;
	case GF_AVC_NALU_PIC_PARAM:
		fputs("PictureParameterSet", dump);
		idx = gf_media_avc_read_pps(ptr, ptr_size, avc);
		assert (idx >= 0);
		fprintf(dump, "\" pps_id=\"%d\" sps_id=\"%d", idx, avc->pps[idx].sps_id);
		break;
	case GF_AVC_NALU_ACCESS_UNIT:
		fputs("AccessUnit delimiter", dump);
		break;
	case GF_AVC_NALU_END_OF_SEQ:
		fputs("EndOfSequence", dump);
		break;
	case GF_AVC_NALU_END_OF_STREAM:
		fputs("EndOfStream", dump);
		break;
	case GF_AVC_NALU_FILLER_DATA:
		fputs("Filler data", dump);
		break;
	case GF_AVC_NALU_SEQ_PARAM_EXT:
		fputs("SequenceParameterSetExtension", dump);
		break;
	case GF_AVC_NALU_SVC_PREFIX_NALU:
		fputs("SVCPrefix", dump);
		break;
	case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
		fputs("SVCSubsequenceParameterSet", dump);
		idx = gf_media_avc_read_sps(ptr, ptr_size, avc, 1, NULL);
		assert (idx >= 0);
		fprintf(dump, "\" sps_id=\"%d", idx - GF_SVC_SSPS_ID_SHIFT);
		break;
	case GF_AVC_NALU_SLICE_AUX:
		fputs("Auxiliary Slice", dump);
		break;

	case GF_AVC_NALU_SVC_SLICE:
		gf_media_avc_parse_nalu(bs, ptr[0], avc);
		fputs(is_svc ? "SVCSlice" : "CodedSliceExtension", dump);
		dependency_id = (ptr[2] & 0x70) >> 4;
		quality_id = (ptr[2] & 0x0F);
		temporal_id = (ptr[3] & 0xE0) >> 5;
		fprintf(dump, "\" dependency_id=\"%d\" quality_id=\"%d\" temporal_id=\"%d", dependency_id, quality_id, temporal_id);
		fprintf(dump, "\" poc=\"%d", avc->s_info.poc);
		break;
	case 30:
		fputs("SVCAggregator", dump);
		break;
	case 31:
		fputs("SVCExtractor", dump);
		track_ref_index = (u8) ptr[4];
		sample_offset = (s8) ptr[5];
		data_offset = read_nal_size_hdr(&ptr[6], nalh_size);
		data_size = read_nal_size_hdr(&ptr[6+nalh_size], nalh_size);
		fprintf(dump, "\" track_ref_index=\"%d\" sample_offset=\"%d\" data_offset=\"%d\" data_size=\"%d\"", track_ref_index, sample_offset, data_offset, data_size);
		break;

	default:
		fputs("UNKNOWN", dump);
		break;
	}
	fputs("\"", dump);
	if (res<0)
		fprintf(dump, " status=\"error decoding slice\"");

	if (bs) gf_bs_del(bs);
}
#endif

void dump_file_nal(GF_ISOFile *file, u32 trackID, char *inName)
{
	u32 i, count, track, nalh_size, timescale, cur_extract_mode;
	FILE *dump;
	s32 countRef;
	Bool is_adobe_protection = GF_FALSE;
#ifndef GPAC_DISABLE_AV_PARSERS
	Bool is_hevc = GF_FALSE;
	AVCState avc;
	GF_AVCConfig *avccfg, *svccfg;
	GF_HEVCConfig *hevccfg, *shvccfg;
	GF_AVCConfigSlot *slc;

	memset(&avc, 0, sizeof(AVCState));
#endif

	if (inName) {
		char szBuf[GF_MAX_PATH];
		strcpy(szBuf, inName);
		sprintf(szBuf, "%s_%d_nalu.xml", inName, trackID);
		dump = gf_f64_open(szBuf, "wt");
	} else {
		dump = stderr;
	}
	track = gf_isom_get_track_by_id(file, trackID);

	count = gf_isom_get_sample_count(file, track);

	timescale = gf_isom_get_media_timescale(file, track);

	cur_extract_mode = gf_isom_get_nalu_extract_mode(file, track);

	fprintf(dump, "<NALUTrack trackID=\"%d\" SampleCount=\"%d\" TimeScale=\"%d\">\n", trackID, count, timescale);

#ifndef GPAC_DISABLE_AV_PARSERS
	avccfg = gf_isom_avc_config_get(file, track, 1);
	svccfg = gf_isom_svc_config_get(file, track, 1);
	hevccfg = gf_isom_hevc_config_get(file, track, 1);
	shvccfg = gf_isom_shvc_config_get(file, track, 1);
	//for tile tracks the hvcC is stored in the 'tbas' track
	if (!hevccfg && gf_isom_get_reference_count(file, track, GF_4CC('t','b','a','s'))) {
		u32 tk = 0;
		gf_isom_get_reference(file, track, GF_4CC('t','b','a','s'), 1, &tk);
		hevccfg = gf_isom_hevc_config_get(file, tk, 1);
	}
	fprintf(dump, " <NALUConfig>\n");

#define DUMP_ARRAY(arr, name)\
	if (arr) {\
		for (i=0; i<gf_list_count(arr); i++) {\
			slc = gf_list_get(arr, i);\
			fprintf(dump, "  <%s number=\"%d\" size=\"%d\" ", name, i+1, slc->size);\
			dump_nalu(dump, slc->data, slc->size, svccfg ? 1 : 0, is_hevc, &avc, nalh_size);\
			fprintf(dump, "/>\n");\
		}\
	}\
 
	nalh_size = 0;

	if (avccfg) {
		nalh_size = avccfg->nal_unit_size;

		DUMP_ARRAY(avccfg->sequenceParameterSets, "AVCSPSArray")
		DUMP_ARRAY(avccfg->pictureParameterSets, "AVCPPSArray")
		DUMP_ARRAY(avccfg->sequenceParameterSetExtensions, "AVCSPSExArray")
	}
	if (svccfg) {
		if (!nalh_size) nalh_size = svccfg->nal_unit_size;
		DUMP_ARRAY(svccfg->sequenceParameterSets, "SVCSPSArray")
		DUMP_ARRAY(svccfg->pictureParameterSets, "SVCPPSArray")
	}
	if (hevccfg) {
#ifndef GPAC_DISABLE_HEVC
		u32 idx;
		nalh_size = hevccfg->nal_unit_size;
		is_hevc = 1;
		for (idx=0; idx<gf_list_count(hevccfg->param_array); idx++) {
			GF_HEVCParamArray *ar = gf_list_get(hevccfg->param_array, idx);
			if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
				DUMP_ARRAY(ar->nalus, "HEVCSPSArray")
			} else if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
				DUMP_ARRAY(ar->nalus, "HEVCPPSArray")
			} else if (ar->type==GF_HEVC_NALU_VID_PARAM) {
				DUMP_ARRAY(ar->nalus, "HEVCVPSArray")
			} else {
				DUMP_ARRAY(ar->nalus, "HEVCUnknownPSArray")
			}
		}
#endif
	}
	if (shvccfg) {
#ifndef GPAC_DISABLE_HEVC
		u32 idx;
		nalh_size = shvccfg->nal_unit_size;
		is_hevc = 1;
		for (idx=0; idx<gf_list_count(shvccfg->param_array); idx++) {
			GF_HEVCParamArray *ar = gf_list_get(shvccfg->param_array, idx);
			if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
				DUMP_ARRAY(ar->nalus, "HEVCSPSArray")
			} else if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
				DUMP_ARRAY(ar->nalus, "HEVCPPSArray")
			} else if (ar->type==GF_HEVC_NALU_VID_PARAM) {
				DUMP_ARRAY(ar->nalus, "HEVCVPSArray")
			} else {
				DUMP_ARRAY(ar->nalus, "HEVCUnknownPSArray")
			}
		}
#endif
	}
#endif
	fprintf(dump, " </NALUConfig>\n");

	/*fixme: for dumping encrypted track: we don't have neither avccfg nor svccfg*/
	if (!nalh_size) nalh_size = 4;

	/*for testing dependency*/
	countRef = gf_isom_get_reference_count(file, track, GF_ISOM_REF_SCAL);
	if (countRef > 0)
	{
		u32 refTrackID;
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
	is_adobe_protection = gf_isom_is_adobe_protection_media(file, track, 1);
	for (i=0; i<count; i++) {
		u64 dts, cts;
		u32 size, nal_size, idx;
		char *ptr;
		GF_ISOSample *samp = gf_isom_get_sample(file, track, i+1, NULL);
		dts = samp->DTS;
		cts = dts + (s32) samp->CTS_Offset;

		fprintf(dump, "  <Sample number=\"%d\" DTS=\""LLD"\" CTS=\""LLD"\" size=\"%d\" RAP=\"%d\" >\n", i+1, dts, cts, samp->dataLength, samp->IsRAP);
		if (cts<dts) fprintf(dump, "<!-- NEGATIVE CTS OFFSET! -->\n");

		idx = 1;
		ptr = samp->data;
		size = samp->dataLength;
		if (is_adobe_protection) {
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

			if (nalh_size + nal_size > size) {
				fprintf(dump, "   <!-- NALU number %d is corrupted: size is %d but only %d remains -->\n", idx, nal_size, size);
				break;
			} else {
				fprintf(dump, "   <NALU number=\"%d\" size=\"%d\" ", idx, nal_size);
#ifndef GPAC_DISABLE_AV_PARSERS
				dump_nalu(dump, ptr, nal_size, svccfg ? 1 : 0, is_hevc, &avc, nalh_size);
#endif
				fprintf(dump, "/>\n");
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

	if (inName) fclose(dump);
#ifndef GPAC_DISABLE_AV_PARSERS
	if (avccfg) gf_odf_avc_cfg_del(avccfg);
	if (svccfg) gf_odf_avc_cfg_del(svccfg);
#ifndef GPAC_DISABLE_HEVC
	if (hevccfg) gf_odf_hevc_cfg_del(hevccfg);
	if (shvccfg) gf_odf_hevc_cfg_del(shvccfg);
#endif

#endif
	gf_isom_set_nalu_extract_mode(file, track, cur_extract_mode);
}

#ifndef GPAC_DISABLE_ISOM_DUMP

void dump_file_ismacryp(GF_ISOFile *file, char *inName)
{
	u32 i, j;
	FILE *dump;
	char szBuf[1024];

	if (inName) {
		strcpy(szBuf, inName);
		strcat(szBuf, "_ismacryp.xml");
		dump = gf_f64_open(szBuf, "wt");
	} else {
		dump = stderr;
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
	if (inName) fclose(dump);
}


void dump_timed_text_track(GF_ISOFile *file, u32 trackID, char *inName, Bool is_convert, GF_TextDumpType dump_type)
{
	FILE *dump;
	GF_Err e;
	u32 track;
	char szBuf[1024];

	track = gf_isom_get_track_by_id(file, trackID);
	if (!track) {
		fprintf(stderr, "Cannot find track ID %d\n", trackID);
		return;
	}

	switch (gf_isom_get_media_type(file, track)) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		fprintf(stderr, "Track ID %d is not a 3GPP text track\n", trackID);
		return;
	}

	if (inName) {
		char *ext;
		ext = ((dump_type==GF_TEXTDUMPTYPE_SVG) ? "svg" : ((dump_type==GF_TEXTDUMPTYPE_SRT) ? "srt" : "ttxt"));
		if (is_convert)
			sprintf(szBuf, "%s.%s", inName, ext) ;
		else
			sprintf(szBuf, "%s_%d_text.%s", inName, trackID, ext);
		dump = gf_f64_open(szBuf, "wt");
	} else {
		dump = stdout;
	}
	e = gf_isom_text_dump(file, track, dump, dump_type);
	if (inName) fclose(dump);

	if (e) fprintf(stderr, "Conversion failed (%s)\n", gf_error_to_string(e));
	else fprintf(stderr, "Conversion done\n");
}

#endif /*GPAC_DISABLE_ISOM_DUMP*/

#ifndef GPAC_DISABLE_ISOM_HINTING

void DumpSDP(GF_ISOFile *file, char *inName)
{
	const char *sdp;
	u32 size, i;
	FILE *dump;
	char szBuf[1024];

	if (inName) {
		char *ext;
		strcpy(szBuf, inName);
		ext = strchr(szBuf, '.');
		if (ext) ext[0] = 0;
		strcat(szBuf, "_sdp.txt");
		dump = gf_f64_open(szBuf, "wt");
	} else {
		dump = stderr;
		fprintf(dump, "* File SDP content *\n\n");
	}
	//get the movie SDP
	gf_isom_sdp_get(file, &sdp, &size);
	fprintf(dump, "%s", sdp);
	fprintf(dump, "\r\n");

	//then tracks
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		if (gf_isom_get_media_type(file, i+1) != GF_ISOM_MEDIA_HINT) continue;
		gf_isom_sdp_track_get(file, i+1, &sdp, &size);
		fprintf(dump, "%s", sdp);
	}
	fprintf(dump, "\n\n");
	if (inName) fclose(dump);
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
		sprintf(szTime, "GMT %s", asctime(gmtime(&now)) );
	}
	return szTime;
}

void print_udta(GF_ISOFile *file, u32 track_number)
{
	u32 i, count;

	count =  gf_isom_get_udta_count(file, track_number);
	if (!count) return;

	fprintf(stderr, "%d UDTA types: ", count);

	for (i=0; i<count; i++) {
		u32 type;
		bin128 uuid;
		gf_isom_get_udta_type(file, track_number, i+1, &type, &uuid);
		fprintf(stderr, "%s (%d) ", gf_4cc_to_str(type), gf_isom_get_user_data_count(file, track_number, type, uuid) );
	}
	fprintf(stderr, "\n");
}

GF_Err dump_udta(GF_ISOFile *file, char *inName, u32 dump_udta_type, u32 dump_udta_track)
{
	char szName[1024], *data;
	FILE *t;
	bin128 uuid;
	u32 count, res;
	GF_Err e;

	memset(uuid, 0, 16);
	count = gf_isom_get_user_data_count(file, dump_udta_track, dump_udta_type, uuid);
	if (!count) {
		fprintf(stderr, "No UDTA for type %s found\n", gf_4cc_to_str(dump_udta_type) );
		return GF_OK;
	}

	data = NULL;
	count = 0;
	e = gf_isom_get_user_data(file, dump_udta_track, dump_udta_type, uuid, 0, &data, &count);
	if (e) {
		fprintf(stderr, "Error dumping UDTA %s: %s\n", gf_4cc_to_str(dump_udta_type), gf_error_to_string(e) );
		return e;
	} 
	sprintf(szName, "%s_%s.udta", inName, gf_4cc_to_str(dump_udta_type) );
	t = fopen(szName, "wb");
	if (!t) {
		gf_free(data);
		fprintf(stderr, "Cannot open file %s\n", szName );
		return GF_IO_ERR;
	}
	res = (u32) fwrite(data, 1, count, t);
	fclose(t);
	gf_free(data);
	if (count != res) {
		fprintf(stderr, "Error writing udta to file\n");
		gf_free(data);
		return GF_IO_ERR;
	}
	return GF_OK;
}


GF_Err dump_chapters(GF_ISOFile *file, char *inName, Bool dump_ogg)
{
	char szName[1024];
	FILE *t;
	u32 i, count;
	count = gf_isom_get_chapter_count(file, 0);
	strcpy(szName, inName);
	if (dump_ogg) {
		strcat(szName, ".txt");
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Extracting OGG chapters to %s\n", szName));
	}
	else {
		strcat(szName, ".chap");
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Extracting chapters to %s\n", szName));
	}

	t = gf_f64_open(szName, "wt");
	if (!t) return GF_IO_ERR;

	for (i=0; i<count; i++) {
		u64 chapter_time;
		const char *name;
		char szDur[20];
		gf_isom_get_chapter(file, 0, i+1, &chapter_time, &name);
		if (dump_ogg) {
			fprintf(t, "CHAPTER%02d=%s\n", i+1, format_duration(chapter_time, 1000, szDur));
			fprintf(t, "CHAPTER%02dNAME=%s\n", i+1, name);
		} else {
			chapter_time /= 1000;
			fprintf(t, "AddChapterBySecond("LLD",%s)\n", chapter_time, name);
		}
	}
	fclose(t);
	return GF_OK;
}


static void DumpMetaItem(GF_ISOFile *file, Bool root_meta, u32 tk_num, char *name)
{
	u32 i, count, brand, primary_id;
	brand = gf_isom_get_meta_type(file, root_meta, tk_num);
	if (!brand) return;

	count = gf_isom_get_meta_item_count(file, root_meta, tk_num);
	primary_id = gf_isom_get_meta_primary_item_id(file, root_meta, tk_num);
	fprintf(stderr, "%s type: \"%s\" - %d resource item(s)\n", name, gf_4cc_to_str(brand), (count+(primary_id>0)));
	switch (gf_isom_has_meta_xml(file, root_meta, tk_num)) {
	case 1:
		fprintf(stderr, "Meta has XML resource\n");
		break;
	case 2:
		fprintf(stderr, "Meta has BinaryXML resource\n");
		break;
	}
	if (primary_id) {
		fprintf(stderr, "Primary Item - ID %d\n", primary_id);
	}
	for (i=0; i<count; i++) {
		const char *it_name, *mime, *enc, *url, *urn;
		Bool self_ref;
		u32 ID;
		gf_isom_get_meta_item_info(file, root_meta, tk_num, i+1, &ID, NULL, &self_ref, &it_name, &mime, &enc, &url, &urn);
		fprintf(stderr, "Item #%d - ID %d", i+1, ID);
		if (self_ref) fprintf(stderr, " - Self-Reference");
		else if (it_name) fprintf(stderr, " - Name: %s", it_name);
		if (mime) fprintf(stderr, " - MimeType: %s", mime);
		if (enc) fprintf(stderr, " - ContentEncoding: %s", enc);
		fprintf(stderr, "\n");
		if (url) fprintf(stderr, "URL: %s\n", url);
		if (urn) fprintf(stderr, "URN: %s\n", urn);
	}
}

#ifndef GPAC_DISABLE_HEVC
void dump_hevc_track_info(GF_ISOFile *file, u32 trackNum, GF_HEVCConfig *hevccfg, HEVCState *hevc_state)
{
	u32 k, idx;
	fprintf(stderr, "\t%s Info: Profile %s @ Level %g - Chroma Format %d\n", hevccfg->is_shvc ? "SHVC" : "HEVC", gf_hevc_get_profile_name(hevccfg->profile_idc), ((Double)hevccfg->level_idc) / 30.0, hevccfg->chromaFormat);
	fprintf(stderr, "\tNAL Unit length bits: %d - general profile compatibility 0x%08X\n", 8*hevccfg->nal_unit_size, hevccfg->general_profile_compatibility_flags);
	fprintf(stderr, "\tParameter Sets: ");
	for (k=0; k<gf_list_count(hevccfg->param_array); k++) {
		GF_HEVCParamArray *ar=gf_list_get(hevccfg->param_array, k);
		if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
			fprintf(stderr, "%d SPS ", gf_list_count(ar->nalus));
		}
		if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
			fprintf(stderr, "%d PPS ", gf_list_count(ar->nalus));
		}
		if (ar->type==GF_HEVC_NALU_VID_PARAM) {
			fprintf(stderr, "%d VPS ", gf_list_count(ar->nalus));

			for (idx=0; idx<gf_list_count(ar->nalus); idx++) {
				GF_AVCConfigSlot *vps = gf_list_get(ar->nalus, idx);
				gf_media_hevc_read_vps(vps->data, vps->size, hevc_state);
			}
		}
	}

	fprintf(stderr, "\n");
	for (k=0; k<gf_list_count(hevccfg->param_array); k++) {
		GF_HEVCParamArray *ar=gf_list_get(hevccfg->param_array, k);
		u32 width, height;
		s32 par_n, par_d;

		if (ar->type !=GF_HEVC_NALU_SEQ_PARAM) continue;
		for (idx=0; idx<gf_list_count(ar->nalus); idx++) {
			GF_AVCConfigSlot *sps = gf_list_get(ar->nalus, idx);
			par_n = par_d = -1;
			gf_hevc_get_sps_info_with_state(hevc_state, sps->data, sps->size, NULL, &width, &height, &par_n, &par_d);
			fprintf(stderr, "\tSPS resolution %dx%d", width, height);
			if ((par_n>0) && (par_d>0)) {
				u32 tw, th;
				gf_isom_get_track_layout_info(file, trackNum, &tw, &th, NULL, NULL, NULL);
				fprintf(stderr, " - Pixel Aspect Ratio %d:%d - Indicated track size %d x %d", par_n, par_d, tw, th);
			}
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "\tBit Depth luma %d - Chroma %d - %d temporal layers\n", hevccfg->luma_bit_depth, hevccfg->chroma_bit_depth, hevccfg->numTemporalLayers);
	if (hevccfg->is_shvc) {
		fprintf(stderr, "\t%sNum Layers: %d (scalability mask 0x%02X)%s\n", hevccfg->non_hevc_base_layer ? "Non-HEVC base layer - " : "", hevccfg->num_layers, hevccfg->scalability_mask, hevccfg->complete_representation ? "" : " - no VCL data");
	}
}
#endif

void DumpTrackInfo(GF_ISOFile *file, u32 trackID, Bool full_dump)
{
	Float scale;
	Bool is_od_track = 0;
	u32 trackNum, i, j, max_rate, rate, ts, mtype, msub_type, timescale, sr, nb_ch, count, alt_group, nb_groups, nb_edits;
	u64 time_slice, dur, size;
	u8 bps;
	GF_ESD *esd;
	char szDur[50];
	char *lang;

	trackNum = gf_isom_get_track_by_id(file, trackID);
	if (!trackNum) {
		fprintf(stderr, "No track with ID %d found\n", trackID);
		return;
	}

	timescale = gf_isom_get_media_timescale(file, trackNum);
	fprintf(stderr, "Track # %d Info - TrackID %d - TimeScale %d - Media Duration %s\n", trackNum, trackID, timescale, format_duration(gf_isom_get_media_duration(file, trackNum), timescale, szDur));
	nb_edits = gf_isom_get_edit_segment_count(file, trackNum);
	if (nb_edits)
		fprintf(stderr, "Track has %d edit lists: track duration is %s\n", nb_edits, format_duration(gf_isom_get_track_duration(file, trackNum), gf_isom_get_timescale(file), szDur));

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

	count = gf_isom_get_track_kind_count(file, trackNum);
	for (i = 0; i < count; i++) {
		char *kind_scheme, *kind_value;
		gf_isom_get_track_kind(file, trackNum, i, &kind_scheme, &kind_value);
		fprintf(stderr, "Kind: %s - %s\n", kind_scheme, kind_value);
	}

	if (gf_isom_is_track_fragmented(file, trackID) ) {
		u32 frag_samples;
		u64 frag_duration;
		gf_isom_get_fragmented_samples_info(file, trackID, &frag_samples, &frag_duration);
		fprintf(stderr, "Fragmented track: %d samples - Media Duration %s\n", frag_samples, format_duration(frag_duration, timescale, szDur));
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

	print_udta(file, trackNum);

	if (mtype==GF_ISOM_MEDIA_VISUAL) {
		s32 tx, ty;
		u32 w, h;
		gf_isom_get_track_layout_info(file, trackNum, &w, &h, &tx, &ty, NULL);
		fprintf(stderr, "Visual Track layout: x=%d y=%d width=%d height=%d\n", tx, ty, w, h);
	}

	gf_isom_get_audio_info(file, trackNum, 1, &sr, &nb_ch, &bps);
	gf_isom_set_nalu_extract_mode(file, trackNum, GF_ISOM_NALU_EXTRACT_INSPECT);

	msub_type = gf_isom_get_media_subtype(file, trackNum, 1);
	if ((msub_type==GF_ISOM_SUBTYPE_MPEG4)
	        || (msub_type==GF_ISOM_SUBTYPE_MPEG4_CRYP)
	        || (msub_type==GF_ISOM_SUBTYPE_AVC_H264)
	        || (msub_type==GF_ISOM_SUBTYPE_AVC2_H264)
	        || (msub_type==GF_ISOM_SUBTYPE_AVC3_H264)
	        || (msub_type==GF_ISOM_SUBTYPE_AVC4_H264)
	        || (msub_type==GF_ISOM_SUBTYPE_SVC_H264)
	        || (msub_type==GF_ISOM_SUBTYPE_LSR1)
	        || (msub_type==GF_ISOM_SUBTYPE_HVC1)
	        || (msub_type==GF_ISOM_SUBTYPE_HEV1)
	        || (msub_type==GF_ISOM_SUBTYPE_SHV1)
	        || (msub_type==GF_ISOM_SUBTYPE_SHC1)
	        || (msub_type==GF_ISOM_SUBTYPE_HVT1)
	   )  {
		esd = gf_isom_get_esd(file, trackNum, 1);
		if (!esd) {
			fprintf(stderr, "WARNING: Broken MPEG-4 Track\n");
		} else {
			const char *st = gf_odf_stream_type_name(esd->decoderConfig->streamType);
			if (st) {
				fprintf(stderr, "MPEG-4 Config%s%s Stream - ObjectTypeIndication 0x%02x\n",
				        full_dump ? "\n\t" : ": ", st, esd->decoderConfig->objectTypeIndication);
			} else {
				fprintf(stderr, "MPEG-4 Config%sStream Type 0x%02x - ObjectTypeIndication 0x%02x\n",
				        full_dump ? "\n\t" : ": ", esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
			}
			if (esd->decoderConfig->streamType==GF_STREAM_OD)
				is_od_track=1;

			if (esd->decoderConfig->streamType==GF_STREAM_VISUAL) {
				u32 w, h;
				u16 rvc_predef;
				w = h = 0;
				if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_MPEG4_PART2) {
#ifndef GPAC_DISABLE_AV_PARSERS
					if (!esd->decoderConfig->decoderSpecificInfo) {
#else
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					fprintf(stderr, "MPEG-4 Visual Size %d x %d\n", w, h);
#endif
						fprintf(stderr, "\tNon-compliant MPEG-4 Visual track: video_object_layer infos not found in sample description\n");
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
				} else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_AVC) {
#ifndef GPAC_DISABLE_AV_PARSERS
					GF_AVCConfig *avccfg, *svccfg;
					GF_AVCConfigSlot *slc;
					s32 par_n, par_d;
#endif

					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					if (full_dump) fprintf(stderr, "\t");
					fprintf(stderr, "AVC/H264 Video - Visual Size %d x %d\n", w, h);
#ifndef GPAC_DISABLE_AV_PARSERS
					avccfg = gf_isom_avc_config_get(file, trackNum, 1);
					svccfg = gf_isom_svc_config_get(file, trackNum, 1);
					if (!avccfg && !svccfg) {
						fprintf(stderr, "\n\n\tNon-compliant AVC track: SPS/PPS not found in sample description\n");
					} else if (avccfg) {
						fprintf(stderr, "\tAVC Info: %d SPS - %d PPS", gf_list_count(avccfg->sequenceParameterSets) , gf_list_count(avccfg->pictureParameterSets) );
						fprintf(stderr, " - Profile %s @ Level %g\n", gf_avc_get_profile_name(avccfg->AVCProfileIndication), ((Double)avccfg->AVCLevelIndication)/10.0 );
						fprintf(stderr, "\tNAL Unit length bits: %d\n", 8*avccfg->nal_unit_size);
						slc = gf_list_get(avccfg->sequenceParameterSets, 0);
						if (slc) {
							gf_avc_get_sps_info(slc->data, slc->size, NULL, NULL, NULL, &par_n, &par_d);
							if ((par_n>0) && (par_d>0)) {
								u32 tw, th;
								gf_isom_get_track_layout_info(file, trackNum, &tw, &th, NULL, NULL, NULL);
								fprintf(stderr, "\tPixel Aspect Ratio %d:%d - Indicated track size %d x %d\n", par_n, par_d, tw, th);
							}
						}
						if (avccfg->chroma_bit_depth) {
							fprintf(stderr, "\tChroma format %d - Luma bit depth %d - chroma bit depth %d\n", avccfg->chroma_format, avccfg->luma_bit_depth, avccfg->chroma_bit_depth);
						}
						gf_odf_avc_cfg_del(avccfg);
					}
					if (svccfg) {
						fprintf(stderr, "\n\tSVC Info: %d SPS - %d PPS - Profile %s @ Level %g\n", gf_list_count(svccfg->sequenceParameterSets) , gf_list_count(svccfg->pictureParameterSets), gf_avc_get_profile_name(svccfg->AVCProfileIndication), ((Double)svccfg->AVCLevelIndication)/10.0 );
						fprintf(stderr, "\tSVC NAL Unit length bits: %d\n", 8*svccfg->nal_unit_size);
						for (i=0; i<gf_list_count(svccfg->sequenceParameterSets); i++) {
							slc = gf_list_get(svccfg->sequenceParameterSets, i);
							if (slc) {
								u32 s_w, s_h, sps_id;
								gf_avc_get_sps_info(slc->data, slc->size, &sps_id, &s_w, &s_h, &par_n, &par_d);
								fprintf(stderr, "\t\tSSPS ID %d - Visual Size %d x %d\n", sps_id, s_w, s_h);
								if ((par_n>0) && (par_d>0)) {
									u32 tw, th;
									gf_isom_get_track_layout_info(file, trackNum, &tw, &th, NULL, NULL, NULL);
									fprintf(stderr, "\tPixel Aspect Ratio %d:%d - Indicated track size %d x %d\n", par_n, par_d, tw, th);
								}
							}
						}
						gf_odf_avc_cfg_del(svccfg);
					}
#endif /*GPAC_DISABLE_AV_PARSERS*/

				} else if ((esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_HEVC)
				           || (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_SHVC)
				          ) {
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_HEVC)
					HEVCState hevc_state;
					GF_HEVCConfig *hevccfg, *shvccfg;
					memset(&hevc_state, 0, sizeof(HEVCState));
					hevc_state.sps_active_idx = -1;
#endif

					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					if (full_dump) fprintf(stderr, "\t");
					fprintf(stderr, "HEVC Video - Visual Size %d x %d\n", w, h);
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_HEVC)
					hevccfg = gf_isom_hevc_config_get(file, trackNum, 1);
					shvccfg = gf_isom_shvc_config_get(file, trackNum, 1);

					if (msub_type==GF_ISOM_SUBTYPE_HVT1) {
						const char *data;
						u32 size;
						u32  is_default, x,y,w,h, id, independent;
						Bool full_frame;
						if (gf_isom_get_tile_info(file, trackNum, 1, &is_default, &id, &independent, &full_frame, &x, &y, &w, &h)) {
							fprintf(stderr, "\tHEVC Tile - ID %d independent %d (x,y,w,h)=%d,%d,%d,%d \n", id, independent, x, y, w, h);
						} else if (gf_isom_get_sample_group_info(file, trackNum, 1, GF_4CC('t','r','i','f'), &is_default, &data, &size)) {
							fprintf(stderr, "\tHEVC Tile track containing a tile set\n");
						} else {
							fprintf(stderr, "\tHEVC Tile track without tiling info\n");
						}
					} else if (!hevccfg && !shvccfg) {
						fprintf(stderr, "\n\n\tNon-compliant HEVC track: No hvcC or shcC found in sample description\n");
					}
					if (hevccfg) {
						dump_hevc_track_info(file, trackNum, hevccfg, &hevc_state);
						gf_odf_hevc_cfg_del(hevccfg);
						fprintf(stderr, "\n");
					}
					if (shvccfg) {
						dump_hevc_track_info(file, trackNum, shvccfg, &hevc_state);
						gf_odf_hevc_cfg_del(shvccfg);
					}
#endif /*GPAC_DISABLE_AV_PARSERS  && defined(GPAC_DISABLE_HEVC)*/
				}

				/*OGG media*/
				else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_MEDIA_OGG) {
					char *szName;
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					if (full_dump) fprintf(stderr, "\t");
					if (!strnicmp(&esd->decoderConfig->decoderSpecificInfo->data[3], "theora", 6)) szName = "Theora";
					else szName = "Unknown";
					fprintf(stderr, "Ogg/%s video / GPAC Mux  - Visual Size %d x %d\n", szName, w, h);
				}
				else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_IMAGE_JPEG) {
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					fprintf(stderr, "JPEG Stream - Visual Size %d x %d\n", w, h);
				}
				else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_IMAGE_PNG) {
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					fprintf(stderr, "PNG Stream - Visual Size %d x %d\n", w, h);
				}
				else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_IMAGE_JPEG_2000) {
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					fprintf(stderr, "JPEG2000 Stream - Visual Size %d x %d\n", w, h);
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
				u32 is_mp2 = 0;
				switch (esd->decoderConfig->objectTypeIndication) {
				case GPAC_OTI_AUDIO_AAC_MPEG2_MP:
				case GPAC_OTI_AUDIO_AAC_MPEG2_LCP:
				case GPAC_OTI_AUDIO_AAC_MPEG2_SSRP:
					is_mp2 = 1;
				case GPAC_OTI_AUDIO_AAC_MPEG4:
#ifndef GPAC_DISABLE_AV_PARSERS
					if (!esd->decoderConfig->decoderSpecificInfo)
						e = GF_NON_COMPLIANT_BITSTREAM;
					else
						e = gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &a_cfg);
					if (full_dump) fprintf(stderr, "\t");
					if (e) fprintf(stderr, "Corrupted AAC Config\n");
					else {
						fprintf(stderr, "%s - %d Channel(s) - SampleRate %d", gf_m4a_object_type_name(a_cfg.base_object_type), a_cfg.nb_chan, a_cfg.base_sr);
						if (is_mp2) fprintf(stderr, " (MPEG-2 Signaling)");
						if (a_cfg.has_sbr) fprintf(stderr, " - SBR SampleRate %d", a_cfg.sbr_sr);
						if (a_cfg.has_ps) fprintf(stderr, " - PS");
						fprintf(stderr, "\n");
					}
#else
					fprintf(stderr, "MPEG-2/4 Audio - %d Channels - SampleRate %d\n", nb_ch, sr);
#endif
					break;
				case GPAC_OTI_AUDIO_MPEG2_PART3:
				case GPAC_OTI_AUDIO_MPEG1:
					if (msub_type == GF_ISOM_SUBTYPE_MPEG4_CRYP) {
						fprintf(stderr, "MPEG-1/2 Audio - %d Channels - SampleRate %d\n", nb_ch, sr);
					} else {
#ifndef GPAC_DISABLE_AV_PARSERS
						GF_ISOSample *samp = gf_isom_get_sample(file, trackNum, 1, &oti);
						if (samp) {
							oti = GF_4CC((u8)samp->data[0], (u8)samp->data[1], (u8)samp->data[2], (u8)samp->data[3]);
							if (full_dump) fprintf(stderr, "\t");
							fprintf(stderr, "%s Audio - %d Channel(s) - SampleRate %d - Layer %d\n",
							        gf_mp3_version_name(oti),
							        gf_mp3_num_channels(oti),
							        gf_mp3_sampling_rate(oti),
							        gf_mp3_layer(oti)
							       );
							gf_isom_sample_del(&samp);
						} else {
							fprintf(stderr, "\n\tError fetching sample: %s\n", gf_error_to_string(gf_isom_last_error(file)) );
						}
#else
						fprintf(stderr, "MPEG-1/2 Audio - %d Channels - SampleRate %d\n", nb_ch, sr);
#endif
					}
					break;
				/*OGG media*/
				case GPAC_OTI_MEDIA_OGG:
				{
					char *szName;
					if (full_dump) fprintf(stderr, "\t");
					if (!strnicmp(&esd->decoderConfig->decoderSpecificInfo->data[3], "vorbis", 6)) szName = "Vorbis";
					else if (!strnicmp(&esd->decoderConfig->decoderSpecificInfo->data[2], "Speex", 5)) szName = "Speex";
					else if (!strnicmp(&esd->decoderConfig->decoderSpecificInfo->data[2], "Flac", 4)) szName = "Flac";
					else szName = "Unknown";
					fprintf(stderr, "Ogg/%s audio / GPAC Mux - Sample Rate %d - %d channel(s)\n", szName, sr, nb_ch);
				}
				break;
				case GPAC_OTI_AUDIO_EVRC_VOICE:
					fprintf(stderr, "EVRC Audio - Sample Rate 8000 - 1 channel\n");
					break;
				case GPAC_OTI_AUDIO_SMV_VOICE:
					fprintf(stderr, "SMV Audio - Sample Rate 8000 - 1 channel\n");
					break;
				case GPAC_OTI_AUDIO_13K_VOICE:
					fprintf(stderr, "QCELP Audio - Sample Rate 8000 - 1 channel\n");
					break;
				/*packetVideo hack for EVRC...*/
				case 0xD1:
					if (esd->decoderConfig->decoderSpecificInfo && (esd->decoderConfig->decoderSpecificInfo->dataLength==8)
					        && !strnicmp(esd->decoderConfig->decoderSpecificInfo->data, "pvmm", 4)) {
						if (full_dump) fprintf(stderr, "\t");
						fprintf(stderr, "EVRC Audio (PacketVideo Mux) - Sample Rate 8000 - 1 channel\n");
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
				} else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_SCENE_AFX) {
					u8 tag = esd->decoderConfig->decoderSpecificInfo ? esd->decoderConfig->decoderSpecificInfo->data[0] : 0xFF;
					const char *afxtype = gf_afx_get_type_description(tag);
					fprintf(stderr, "AFX Stream - type %s (%d)\n", afxtype, tag);
				} else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_FONT) {
					fprintf(stderr, "Font Data stream\n");
				} else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_SCENE_LASER) {
					GF_LASERConfig l_cfg;
					gf_odf_get_laser_config(esd->decoderConfig->decoderSpecificInfo, &l_cfg);
					fprintf(stderr, "LASER Stream - %s\n", l_cfg.newSceneIndicator ? "Full Scene" : "Scene Segment");
				} else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_TEXT_MPEG4) {
					fprintf(stderr, "MPEG-4 Streaming Text stream\n");
				} else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_SCENE_SYNTHESIZED_TEXTURE) {
					fprintf(stderr, "Synthetized Texture stream stream\n");
				} else {
					fprintf(stderr, "Unknown Systems stream OTI %d\n", esd->decoderConfig->objectTypeIndication);
				}
			}

			/*sync is only valid if we open all tracks to take care of default MP4 sync..*/
			if (!full_dump) {
				if (!esd->OCRESID || (esd->OCRESID == esd->ESID))
					fprintf(stderr, "Self-synchronized\n");
				else
					fprintf(stderr, "Synchronized on stream %d\n", esd->OCRESID);
			} else {
				fprintf(stderr, "\tDecoding Buffer size %d - Average bitrate %d kbps - Max Bitrate %d kbps\n", esd->decoderConfig->bufferSizeDB, esd->decoderConfig->avgBitrate/1000, esd->decoderConfig->maxBitrate/1000);
				if (esd->dependsOnESID)
					fprintf(stderr, "\tDepends on stream %d for decoding\n", esd->dependsOnESID);
				else
					fprintf(stderr, "\tNo stream dependencies for decoding\n");

				fprintf(stderr, "\tStreamPriority %d\n", esd->streamPriority);
				if (esd->URLString) fprintf(stderr, "\tRemote Data Source %s\n", esd->URLString);
			}
			gf_odf_desc_del((GF_Descriptor *) esd);

			/*ISMACryp*/
			if (msub_type == GF_ISOM_SUBTYPE_MPEG4_CRYP) {
				const char *scheme_URI, *KMS_URI;
				u32 scheme_type, version;
				u32 IV_size;
				Bool use_sel_enc;

				if (gf_isom_is_ismacryp_media(file, trackNum, 1)) {
					gf_isom_get_ismacryp_info(file, trackNum, 1, NULL, &scheme_type, &version, &scheme_URI, &KMS_URI, &use_sel_enc, &IV_size, NULL);
					fprintf(stderr, "\n*Encrypted stream - ISMA scheme %s (version %d)\n", gf_4cc_to_str(scheme_type), version);
					if (scheme_URI) fprintf(stderr, "scheme location: %s\n", scheme_URI);
					if (KMS_URI) {
						if (!strnicmp(KMS_URI, "(key)", 5)) fprintf(stderr, "KMS location: key in file\n");
						else fprintf(stderr, "KMS location: %s\n", KMS_URI);
					}
					fprintf(stderr, "Selective Encryption: %s\n", use_sel_enc ? "Yes" : "No");
					if (IV_size) fprintf(stderr, "Initialization Vector size: %d bits\n", IV_size*8);
				} else if (gf_isom_is_omadrm_media(file, trackNum, 1)) {
					const char *textHdrs;
					u32 enc_type, hdr_len;
					u64 orig_len;
					fprintf(stderr, "\n*Encrypted stream - OMA DRM\n");
					gf_isom_get_omadrm_info(file, trackNum, 1, NULL, NULL, NULL, &scheme_URI, &KMS_URI, &textHdrs, &hdr_len, &orig_len, &enc_type, &use_sel_enc, &IV_size, NULL);
					fprintf(stderr, "Rights Issuer: %s\n", KMS_URI);
					fprintf(stderr, "Content ID: %s\n", scheme_URI);
					if (textHdrs) {
						u32 i, offset;
						const char *start = textHdrs;
						fprintf(stderr, "OMA Textual Headers:\n");
						i=offset=0;
						while (i<hdr_len) {
							if (start[i]==0) {
								fprintf(stderr, "\t%s\n", start+offset);
								offset=i+1;
							}
							i++;
						}
						fprintf(stderr, "\t%s\n", start+offset);
					}
					if (orig_len) fprintf(stderr, "Original media size "LLD"\n", LLD_CAST orig_len);
					fprintf(stderr, "Encryption algorithm %s\n", (enc_type==1) ? "AEA 128 CBC" : (enc_type ? "AEA 128 CTR" : "None"));


					fprintf(stderr, "Selective Encryption: %s\n", use_sel_enc ? "Yes" : "No");
					if (IV_size) fprintf(stderr, "Initialization Vector size: %d bits\n", IV_size*8);
				} else if(gf_isom_is_cenc_media(file, trackNum, 1)) {
					gf_isom_get_cenc_info(file, trackNum, 1, NULL, &scheme_type, &version, &IV_size);
					fprintf(stderr, "\n*Encrypted stream - CENC scheme %s (version %d)\n", gf_4cc_to_str(scheme_type), version);
					if (IV_size) fprintf(stderr, "Initialization Vector size: %d bits\n", IV_size*8);
				} else if(gf_isom_is_adobe_protection_media(file, trackNum, 1)) {
					gf_isom_get_adobe_protection_info(file, trackNum, 1, NULL, &scheme_type, &version);
					fprintf(stderr, "\n*Encrypted stream - Adobe protection scheme %s (version %d)\n", gf_4cc_to_str(scheme_type), version);
				} else {
					fprintf(stderr, "\n*Encrypted stream - unknown scheme %s\n", gf_4cc_to_str(gf_isom_is_media_encrypted(file, trackNum, 1) ));
				}
			}

		}
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_H263) {
		u32 w, h;
		gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
		fprintf(stderr, "\t3GPP H263 stream - Resolution %d x %d\n", w, h);
	} else if (msub_type == GF_4CC('m','j','p','2')) {
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
	} else if (msub_type == GF_ISOM_SUBTYPE_AC3) {
		u32 br = 0;
		Bool lfe = 0;
		Bool is_ec3 = 0;
#ifndef GPAC_DISABLE_AV_PARSERS
		GF_AC3Config *ac3 = gf_isom_ac3_config_get(file, trackNum, 1);
		if (ac3) {
			int i;
			nb_ch = gf_ac3_get_channels(ac3->streams[0].acmod);
			for (i=0; i<ac3->streams[0].nb_dep_sub; ++i) {
				assert(ac3->streams[0].nb_dep_sub == 1);
				nb_ch += gf_ac3_get_channels(ac3->streams[0].chan_loc);
			}
			lfe = ac3->streams[0].lfon;
			br = ac3->is_ec3 ? ac3->brcode : gf_ac3_get_bitrate(ac3->brcode);
			is_ec3 = ac3->is_ec3;
			gf_free(ac3);
		}
#endif
		fprintf(stderr, "\t%s stream - Sample Rate %d - %d%s channel(s) - bitrate %d\n", is_ec3 ? "EC-3" : "AC-3", sr, nb_ch, lfe ? ".1" : "", br);
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
		s32 i, refCount = gf_isom_get_reference_count(file, trackNum, GF_ISOM_REF_HINT);
		if (refCount) {
			fprintf(stderr, "Streaming Hint Track for track%s ", (refCount>1) ? "s" :"");
			for (i=0; i<refCount; i++) {
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
		for (i=0; i<refCount; i++) {
			const char *name = gf_isom_get_payt_info(file, trackNum, i+1, &refTrack);
			fprintf(stderr, "\tPayload ID %d: type %s\n", refTrack, name);
		}
#endif
	} else if (mtype==GF_ISOM_MEDIA_FLASH) {
		fprintf(stderr, "Macromedia Flash Movie\n");
	} else if ((mtype==GF_ISOM_MEDIA_TEXT) || (mtype==GF_ISOM_MEDIA_SUBT) || (mtype==GF_ISOM_MEDIA_MPEG_SUBT)) {
		u32 w, h;
		s16 l;
		s32 tx, ty;
		gf_isom_get_track_layout_info(file, trackNum, &w, &h, &tx, &ty, &l);
		fprintf(stderr, "Timed Text - Size %d x %d - Translation X=%d Y=%d - Layer %d\n", w, h, tx, ty, l);
	} else if (mtype == GF_ISOM_MEDIA_META) {
		Bool is_xml = 0;
		const char *mime_or_namespace = NULL;
		const char *content_encoding = NULL;
		const char *schema_loc = NULL;
		gf_isom_get_timed_meta_data_info(file, trackNum, 1, &is_xml, &mime_or_namespace, &content_encoding, &schema_loc);
		fprintf(stderr, "%s Metadata stream\n\t%s %s\n\tencoding %s", is_xml ? "Xml" : "Text", is_xml ? "namespace" : "mime-type", mime_or_namespace, content_encoding);
		if (is_xml && schema_loc != NULL)
			fprintf(stderr, "\n\tschema %s\n", schema_loc);
		fprintf(stderr, "\n");
	} else {
		GF_GenericSampleDescription *udesc = gf_isom_get_generic_sample_description(file, trackNum, 1);
		if (udesc) {
			if (mtype==GF_ISOM_MEDIA_VISUAL) {
				fprintf(stderr, "Visual Track - Compressor \"%s\" - Resolution %d x %d\n", udesc->compressor_name, udesc->width, udesc->height);
			} else if (mtype==GF_ISOM_MEDIA_AUDIO) {
				fprintf(stderr, "Audio Track - Sample Rate %d - %d channel(s)\n", udesc->samplerate, udesc->nb_channels);
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

	DumpMetaItem(file, 0, trackNum, "Track Meta");

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

	if (!full_dump) {
		fprintf(stderr, "\n");
		return;
	}

	dur = size = 0;
	max_rate = rate = 0;
	time_slice = 0;
	ts = gf_isom_get_media_timescale(file, trackNum);
	for (j=0; j<gf_isom_get_sample_count(file, trackNum); j++) {
		GF_ISOSample *samp;
		if (is_od_track) {
			samp = gf_isom_get_sample(file, trackNum, j+1, NULL);
		} else {
			samp = gf_isom_get_sample_info(file, trackNum, j+1, NULL, NULL);
		}
		dur = samp->DTS+samp->CTS_Offset;
		size += samp->dataLength;
		rate += samp->dataLength;
		if (samp->DTS - time_slice>ts) {
			if (max_rate < rate) max_rate = rate;
			rate = 0;
			time_slice = samp->DTS;
		}
		gf_isom_sample_del(&samp);
	}
	fprintf(stderr, "\nComputed info from media:\n");
	scale = 1000;
	scale /= ts;
	dur = (u64) (scale * (s64)dur);
	fprintf(stderr, "\tTotal size "LLU" bytes - Total samples duration "LLU" ms\n", size, dur);
	if (!dur) {
		fprintf(stderr, "\n");
		return;
	}
	/*rate in byte, dur is in ms*/
	rate = (u32) ((size * 8 * 1000) / dur);
	max_rate *= 8;
	if (rate >= 1500) {
		rate /= 1000;
		max_rate /= 1000;
		fprintf(stderr, "\tAverage rate %d kbps - Max Rate %d kbps\n", rate, max_rate);
	} else {
		fprintf(stderr, "\tAverage rate %d bps - Max Rate %d bps\n", rate, max_rate);
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
		char szDur[20];
		const char *name;
		u64 time;
		fprintf(stderr, "\nChapters:\n");
		for (j=0; j<count; j++) {
			gf_isom_get_chapter(file, trackNum, j+1, &time, &name);
			fprintf(stderr, "\tChapter #%d - %s - \"%s\"\n", j+1, format_duration(time, 1000, szDur), name);
		}
	}
}

static const char* ID3v1Genres[] = {
	"Blues", "Classic Rock", "Country", "Dance", "Disco",
	"Funk", "Grunge", "Hip-Hop", "Jazz", "Metal",
	"New Age", "Oldies", "Other", "Pop", "R&B",
	"Rap", "Reggae", "Rock", "Techno", "Industrial",
	"Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack",
	"Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk",
	"Fusion", "Trance", "Classical", "Instrumental", "Acid",
	"House", "Game", "Sound Clip", "Gospel", "Noise",
	"AlternRock", "Bass", "Soul", "Punk", "Space",
	"Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic",
	"Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance",
	"Dream", "Southern Rock", "Comedy", "Cult", "Gangsta",
	"Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native American",
	"Cabaret", "New Wave", "Psychadelic", "Rave", "Showtunes",
	"Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz",
	"Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock",
	"Folk", "Folk/Rock", "National Folk", "Swing",
};
static const char *id3_get_genre(u32 tag)
{
	if ((tag>0) && (tag <= (sizeof(ID3v1Genres)/sizeof(const char *)) )) {
		return ID3v1Genres[tag-1];
	}
	return "Unknown";
}
u32 id3_get_genre_tag(const char *name)
{
	u32 i, count = sizeof(ID3v1Genres)/sizeof(const char *);
	if (!name) return 0;
	for (i=0; i<count; i++) {
		if (!stricmp(ID3v1Genres[i], name)) return i+1;
	}
	return 0;
}

void DumpMovieInfo(GF_ISOFile *file)
{
	GF_InitialObjectDescriptor *iod;
	u32 i, brand, min, timescale, count, tag_len;
	const char *tag;
	u64 create, modif;
	char szDur[50];

	DumpMetaItem(file, 1, 0, "Root Meta");
	if (!gf_isom_has_movie(file)) {
		if (gf_isom_has_segment(file, &brand, &min)) {
			u32 j, count;
			count = gf_isom_segment_get_fragment_count(file);
			fprintf(stderr, "File is a segment - %d movie fragments - Brand %s (version %d):\n", count, gf_4cc_to_str(brand), min);
			for (i=0; i<count; i++) {
				u32 traf_count = gf_isom_segment_get_track_fragment_count(file, i+1);
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
	fprintf(stderr, "* Movie Info *\n\tTimescale %d - Duration %s\n\t%d track(s)\n",
	        timescale, format_duration(gf_isom_get_duration(file), timescale, szDur), gf_isom_get_track_count(file));

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (gf_isom_is_fragmented(file)) {
		fprintf(stderr, "\tFragmented File: yes - duration %s\n%d fragments - %d SegmentIndexes\n", format_duration(gf_isom_get_fragmented_duration(file), timescale, szDur), gf_isom_get_fragments_count(file, 0) , gf_isom_get_fragments_count(file, 1) );
	} else {
		fprintf(stderr, "\tFragmented File: no\n");
	}
#endif

	if (gf_isom_moov_first(file))
		fprintf(stderr, "\tFile suitable for progressive download (moov before mdat)\n");

	if (gf_isom_get_brand_info(file, &brand, &min, NULL) == GF_OK) {
		fprintf(stderr, "\tFile Brand %s - version %d\n", gf_4cc_to_str(brand), min);
	}
	gf_isom_get_creation_time(file, &create, &modif);
	fprintf(stderr, "\tCreated: %s", format_date(create, szDur));
	fprintf(stderr, "\tModified: %s", format_date(modif, szDur));
	fprintf(stderr, "\n");

	DumpMetaItem(file, 0, 0, "Moov Meta");

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
		if (!gf_list_count(iod->ESDescriptors)) fprintf(stderr, "No streams included in root OD\n");
		gf_odf_desc_del((GF_Descriptor *) iod);
	} else {
		fprintf(stderr, "File has no MPEG4 IOD/OD\n");
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
		char szDur[20];
		const char *name;
		u64 time;
		fprintf(stderr, "\nChapters:\n");
		for (i=0; i<count; i++) {
			gf_isom_get_chapter(file, 0, i+1, &time, &name);
			fprintf(stderr, "\tChapter #%d - %s - \"%s\"\n", i+1, format_duration(time, 1000, szDur), name);
		}
	}

	if (gf_isom_apple_get_tag(file, 0, &tag, &tag_len) == GF_OK) {
		fprintf(stderr, "\niTunes Info:\n");
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_NAME, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tName: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_ARTIST, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tArtist: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_ALBUM, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tAlbum: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_COMMENT, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tComment: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_TRACK, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tTrack: %d / %d\n", tag[3], tag[5]);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_COMPOSER, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tComposer: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_WRITER, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tWriter: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_ALBUM_ARTIST, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tAlbum Artist: %s\n", tag);

		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_GENRE, &tag, &tag_len)==GF_OK) {
			if (tag[0]) {
				fprintf(stderr, "\tGenre: %s\n", tag);
			} else {
				fprintf(stderr, "\tGenre: %s\n", id3_get_genre(((u8*)tag)[1]));
			}
		}
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_COMPILATION, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tCompilation: %s\n", tag[0] ? "Yes" : "No");
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_GAPLESS, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tGapless album: %s\n", tag[0] ? "Yes" : "No");

		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_CREATED, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tCreated: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_DISK, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tDisk: %d / %d\n", tag[3], tag[5]);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_TOOL, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tEncoder Software: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_ENCODER, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tEncoded by: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_TEMPO, &tag, &tag_len)==GF_OK) {
			if (tag[0]) {
				fprintf(stderr, "\tTempo (BPM): %s\n", tag);
			} else {
				fprintf(stderr, "\tTempo (BPM): %d\n", tag[1]);
			}
		}
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_TRACKNUMBER, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tTrackNumber: %d / %d\n", (0xff00 & (tag[2]<<8)) | (0xff & tag[3]), (0xff00 & (tag[4]<<8)) | (0xff & tag[5]));
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_TRACK, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tTrack: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_GROUP, &tag, &tag_len)==GF_OK) fprintf(stderr, "\tGroup: %s\n", tag);

		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_COVER_ART, &tag, &tag_len)==GF_OK) {
			if (tag_len>>31) fprintf(stderr, "\tCover Art: PNG File\n");
			else fprintf(stderr, "\tCover Art: JPEG File\n");
		}
	}

	print_udta(file, 0);
	fprintf(stderr, "\n");
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		DumpTrackInfo(file, gf_isom_get_track_id(file, i+1), 0);
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
		count = gf_list_count(ts->SDTs) ;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Program Description found - %d desc:\n", count));
		for (i=0; i<count; i++) {
			GF_M2TS_SDT *sdt = gf_list_get(ts->SDTs, i);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("\tServiceID %d - Provider %s - Name %s\n", sdt->service_id, sdt->provider, sdt->service));
		}
		break;
	case GF_M2TS_EVT_SDT_UPDATE:
		count = gf_list_count(ts->SDTs) ;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Program Description updated - %d desc\n", count));
		for (i=0; i<count; i++) {
			GF_M2TS_SDT *sdt = gf_list_get(ts->SDTs, i);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("\tServiceID %d - Provider %s - Name %s\n", sdt->service_id, sdt->provider, sdt->service));
		}
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
				fprintf(dumper->timestamps_info_file, "\t%f\t%d\t%d", pck->PTS / 90000.0, (pck->flags & GF_M2TS_PES_PCK_RAP ? 1 : 0), (pck->flags & GF_M2TS_PES_PCK_DISCONTINUITY ? 1 : 0));
				if (interpolated_pcr_value) {
					diff = (pck->DTS ? pck->DTS : pck->PTS) / 90000.0;
					diff -= pes->last_pcr_value / (300.0 * 90000);
					fprintf(dumper->timestamps_info_file, "\t%f\n", diff);
					if (diff<0) fprintf(stderr, "Warning: detected PTS/DTS value less than current PCR of %g sec\n", diff);
				} else {
					fprintf(dumper->timestamps_info_file, "\t\n");
				}
			}
		}

		if (dumper->has_seen_pat && dumper->pes_out && (dumper->dump_pid == pck->stream->pid)) {
			gf_fwrite(pck->data, pck->data_len, 1, dumper->pes_out);
		}
		break;
	case GF_M2TS_EVT_PES_PCR:
		pck = par;
		if (gf_list_count(ts->programs)>1 && pck->stream->program->number != dumper->prog_number)
			break;
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\t%f\t\t\t\t%d\n", pck->stream->program->last_pcr_value_pck_number, pck->stream->pid, pck->PTS / (300*90000.0), (pck->flags & GF_M2TS_PES_PCK_DISCONTINUITY ? 1 : 0));
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
					if (esd->decoderConfig->decoderSpecificInfo) gf_fwrite(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, 1, dumper->pes_out_info);
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
				gf_fwrite(sl_pck->data+header_len, sl_pck->data_len-header_len, 1, dumper->pes_out);
				fprintf(dumper->pes_out_nhml, "<NHNTSample DTS=\""LLD"\" dataLength=\"%d\" isRAP=\"%s\"/>\n", LLD_CAST header.decodingTimeStamp, sl_pck->data_len-header_len, (header.randomAccessPointFlag?"yes":"no"));
			}
		}
	}
#endif
	break;
	}
}

void dump_mpeg2_ts(char *mpeg2ts_file, char *out_name, Bool prog_num)
{
	char data[188];
	GF_M2TS_Dump dumper;

	u32 size;
	u64 fsize, fdone;
	GF_M2TS_Demuxer *ts;
	FILE *src;

	src = gf_f64_open(mpeg2ts_file, "rb");
	if (!src) {
		fprintf(stderr, "Cannot open %s: no such file\n", mpeg2ts_file);
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
			dumper.pes_out = gf_f64_open(dumper.dump, "wb");
#if 0
			sprintf(dumper.nhml, "%s_%d.nhml", pes_out_name, dumper.dump_pid);
			dumper.pes_out_nhml = gf_f64_open(dumper.nhml, "wt");
			sprintf(dumper.info, "%s_%d.info", pes_out_name, dumper.dump_pid);
			dumper.pes_out_info = gf_f64_open(dumper.info, "wb");
#endif
			pid[0] = '#';
		}
	}


	gf_f64_seek(src, 0, SEEK_END);
	fsize = gf_f64_tell(src);
	gf_f64_seek(src, 0, SEEK_SET);
	fdone = 0;

	/* first loop to process all packets between two PAT, and assume all signaling was found between these 2 PATs */
	while (!feof(src)) {
		size = (u32) fread(data, 1, 188, src);
		if (size<188) break;

		gf_m2ts_process_data(ts, data, size);
		if (dumper.has_seen_pat) break;
	}
	dumper.has_seen_pat = 1;

	if (prog_num) {
		sprintf(dumper.timestamps_info_name, "%s_prog_%d_timestamps.txt", mpeg2ts_file, prog_num/*, mpeg2ts_file*/);
		dumper.timestamps_info_file = gf_f64_open(dumper.timestamps_info_name, "wt");
		if (!dumper.timestamps_info_file) {
			fprintf(stderr, "Cannot open file %s\n", dumper.timestamps_info_name);
			return;
		}
		fprintf(dumper.timestamps_info_file, "PCK#\tPID\tPCR\tDTS\tPTS\tRAP\tDiscontinuity\tDTS-PCR Diff\n");
	}

	gf_m2ts_reset_parsers(ts);
	gf_f64_seek(src, 0, SEEK_SET);
	fdone = 0;


	while (!feof(src)) {
		size = (u32) fread(data, 1, 188, src);
		if (size<188) break;

		gf_m2ts_process_data(ts, data, size);

		fdone += size;
		gf_set_progress("MPEG-2 TS Parsing", fdone, fsize);
	}


	fclose(src);
	gf_m2ts_demux_del(ts);
	if (dumper.pes_out) fclose(dumper.pes_out);
#if 0
	if (dumper.pes_out_nhml) {
		if (dumper.is_info_dumped) fprintf(dumper.pes_out_nhml, "</NHNTStream>\n");
		fclose(dumper.pes_out_nhml);
		fclose(dumper.pes_out_info);
	}
#endif
	if (dumper.timestamps_info_file) fclose(dumper.timestamps_info_file);

}


#endif /*GPAC_DISABLE_MPEG2TS*/


