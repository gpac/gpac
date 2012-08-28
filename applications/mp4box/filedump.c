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
#include <gpac/media_tools.h>
/*for asctime and gmtime*/
#include <time.h>
/*ISO 639 languages*/
#include <gpac/iso639.h>
#include <gpac/mpegts.h>

#ifndef GPAC_DISABLE_SMGR
#include <gpac/scene_manager.h>
#endif

extern u32 swf_flags;
extern Float swf_flatten_angle;
extern u32 get_file_type_by_ext(char *inName);

void scene_coding_log(void *cbk, u32 log_level, u32 log_tool, const char *fmt, va_list vlist);

#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
GF_Err import_file(GF_ISOFile *dest, char *inName, u32 import_flags, Double force_fps, u32 frames_per_sample);
#endif

void PrintLanguages()
{
	u32 i=0;
	fprintf(stdout, "Supported ISO 639 languages and codes:\n\n");
	while (GF_ISO639_Lang[i]) {
		if (!GF_ISO639_Lang[i+2][0]) {
			i+=3;
			continue;
		}
		fprintf(stdout, "%s (%s - %s)\n", GF_ISO639_Lang[i], GF_ISO639_Lang[i+1], GF_ISO639_Lang[i+2]);
		i+=3;
	}
}

static const char *GetLanguage(char *lcode)
{
	u32 i=0;
	if ((lcode[0]=='u') && (lcode[1]=='n') && (lcode[2]=='d')) return "Undetermined";
	while (GF_ISO639_Lang[i]) {
		if (GF_ISO639_Lang[i+2][0] && strstr(GF_ISO639_Lang[i+1], lcode)) return GF_ISO639_Lang[i];
		i+=3;
	}
	return "Unknown";
}

const char *GetLanguageCode(char *lang)
{
	u32 i;
	Bool check_2cc = 0;
	i = strlen(lang);
	if (i==3) return lang;
	if (i==2) check_2cc = 1;

	i=0;
	while (GF_ISO639_Lang[i]) {
		if (GF_ISO639_Lang[i+2][0]) {
			if (check_2cc) {
				if (!stricmp(GF_ISO639_Lang[i+2], lang) ) return GF_ISO639_Lang[i+1];
			} else if (!stricmp(GF_ISO639_Lang[i], lang)) return GF_ISO639_Lang[i+1];
		}
		i+=3;
	}
	return "und";
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
			fprintf(stdout, "No cover art found\n");
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
	tag_len = fread(tag, sizeof(char), tag_len, t);
	fclose(t);
	
	ext = strrchr(inName, '.');
	if (!stricmp(ext, ".png")) tag_len |= 0x80000000;
	e = gf_isom_apple_set_tag(file, GF_ISOM_ITUNE_COVER_ART, tag, tag_len);
	gf_free(tag);
	return e;
}

#endif

#ifndef GPAC_DISABLE_SCENE_DUMP

GF_Err dump_file_text(char *file, char *inName, u32 dump_mode, Bool do_log)
{
	GF_Err e;
	GF_SceneManager *ctx;
	GF_SceneGraph *sg;
	GF_SceneLoader load;
	u32 ftype;
	gf_log_cbk prev_logs = NULL;
	FILE *logs = NULL;
	e = GF_OK;

	sg = gf_sg_new();
	ctx = gf_sm_new(sg);
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = file;
	load.ctx = ctx;
	load.swf_import_flags = swf_flags;
	load.swf_flatten_limit = swf_flatten_angle;

	ftype = get_file_type_by_ext(file);
	if (ftype == 1) {
		load.isom = gf_isom_open(file, GF_ISOM_OPEN_READ, NULL);
		if (!load.isom) {
			e = gf_isom_last_error(NULL);
			fprintf(stdout, "Error opening file: %s\n", gf_error_to_string(e));
			gf_sm_del(ctx);
			gf_sg_del(sg);
			return e;
		}
	}
	/*SAF*/
	else if (ftype==6) {
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
			fprintf(stdout, "Error importing file: %s\n", gf_error_to_string(e));
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
	if (!e) {
		u32 count = gf_list_count(ctx->streams);
		if (count)
			fprintf(stdout, "Scene loaded - dumping %d systems streams\n", count);
		else
			fprintf(stdout, "Scene loaded - dumping root scene\n");

		e = gf_sm_dump(ctx, inName, dump_mode);
	}

	gf_sm_del(ctx);
	gf_sg_del(sg);
	if (e) fprintf(stdout, "Error loading scene: %s\n", gf_error_to_string(e));
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
			fprintf(stdout, "Cannot open file: %s\n", gf_error_to_string(gf_isom_last_error(NULL)));
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
		dump = stdout;
		close = 0;
	}

	fprintf(stdout, "Analysing Scene\n");

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
		fprintf(stdout, "%s\n", gf_error_to_string(e));
	} else {
		fprintf(dump, "</SceneStatistics>\n");
	}
	if (dump && close) fclose(dump);
	fprintf(stdout, "done\n");
}
#endif /*GPAC_DISABLE_SCENE_STATS*/



#ifndef GPAC_DISABLE_VRML

static void PrintFixed(Fixed val, Bool add_space)
{
	if (add_space) fprintf(stdout, " ");
	if (val==FIX_MIN) fprintf(stdout, "-I");
	else if (val==FIX_MAX) fprintf(stdout, "+I");
	else fprintf(stdout, "%g", FIX2FLT(val));
}

static void PrintNodeSFField(u32 type, void *far_ptr)
{
	if (!far_ptr) return;
	switch (type) {
	case GF_SG_VRML_SFBOOL:
		fprintf(stdout, "%s", (*(SFBool *)far_ptr) ? "TRUE" : "FALSE");
		break;
	case GF_SG_VRML_SFINT32:
		fprintf(stdout, "%d", (*(SFInt32 *)far_ptr));
		break;
	case GF_SG_VRML_SFFLOAT:
		PrintFixed((*(SFFloat *)far_ptr), 0);
		break;
	case GF_SG_VRML_SFTIME:
		fprintf(stdout, "%g", (*(SFTime *)far_ptr));
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
			fprintf(stdout, "\"%s\"", ((SFString*)far_ptr)->buffer);
		else
			fprintf(stdout, "NULL");
		break;
	}
}
#endif

void PrintNode(const char *name, u32 graph_type)
{
#ifdef GPAC_DISABLE_VRML
	fprintf(stdout, "VRML/MPEG-4/X3D scene graph is disabled in this build of GPAC\n");
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
		fprintf(stdout, "SVG node printing is not supported\n");
		return;
	} else if (graph_type==1) {
#ifndef GPAC_DISABLE_X3D
		tag = gf_node_x3d_type_by_class_name(name);
		std_name = "X3D";
#else
                fprintf(stdout, "X3D node printing is not supported (X3D support disabled)\n");
                return;
#endif
	} else {
		tag = gf_node_mpeg4_type_by_class_name(name);
		std_name = "MPEG4";
	}
	if (!tag) {
		fprintf(stdout, "Unknown %s node %s\n", std_name, name);
		return;
	}

	sg = gf_sg_new();
	node = gf_node_new(sg, tag);
	gf_node_register(node, NULL);
	nname = gf_node_get_class_name(node);
	if (!node) {
		fprintf(stdout, "Node %s not supported in current built\n", nname);
		return;
	}
	nbF = gf_node_get_field_count(node);

	if (is_nodefield) {
		u32 tfirst, tlast;
		if (gf_node_get_field_by_name(node, szField, &f) != GF_OK) {
			fprintf(stdout, "Field %s is not a member of node %s\n", szField, name);
			return;
		}
		fprintf(stdout, "Allowed nodes in %s.%s:\n", name, szField);
		if (graph_type==1) {
			tfirst = GF_NODE_RANGE_FIRST_X3D;
			tlast = GF_NODE_RANGE_LAST_X3D;
		} else {
			tfirst = GF_NODE_RANGE_FIRST_MPEG4;
			tlast = GF_NODE_RANGE_LAST_MPEG4;
		}
		for (i=tfirst; i<tlast;i++) {
			GF_Node *tmp = gf_node_new(sg, i);
			gf_node_register(tmp, NULL);
			if (gf_node_in_table_by_tag(i, f.NDTtype)) {
				const char *nname = gf_node_get_class_name(tmp);
				if (nname && strcmp(nname, "Unknown Node")) {
					fprintf(stdout, "\t%s\n", nname); 
				}
			}
			gf_node_unregister(tmp, NULL);
		}
		return;
	}

	fprintf(stdout, "Node Syntax:\n%s {\n", nname);

	for (i=0; i<nbF; i++) {
		gf_node_get_field(node, i, &f);
		if (graph_type==2) {
			fprintf(stdout, "\t%s=\"...\"\n", f.name);
			continue;
		}

		fprintf(stdout, "\t%s %s %s", gf_sg_vrml_get_event_type_name(f.eventType, 0), gf_sg_vrml_get_field_type_by_name(f.fieldType), f.name);
		if (f.fieldType==GF_SG_VRML_SFNODE) fprintf(stdout, " NULL");
		else if (f.fieldType==GF_SG_VRML_MFNODE) fprintf(stdout, " []");
		else if (gf_sg_vrml_is_sf_field(f.fieldType)) {
			fprintf(stdout, " ");
			PrintNodeSFField(f.fieldType, f.far_ptr);
		} else {
			void *ptr;
			u32 i, sftype;
			GenMFField *mffield = (GenMFField *) f.far_ptr;
			fprintf(stdout, " [");
			sftype = gf_sg_vrml_get_sf_type(f.fieldType);
			for (i=0; i<mffield->count; i++) {
				if (i) fprintf(stdout, " ");
				gf_sg_vrml_mf_get_item(f.far_ptr, f.fieldType, &ptr, i);
				PrintNodeSFField(sftype, ptr);
			}
			fprintf(stdout, "]");
		}
#ifndef GPAC_DISABLE_BIFS
		if (gf_bifs_get_aq_info(node, i, &qt, &at, &bmin, &bmax, &nbBits)) {
			if (qt) {
				fprintf(stdout, " #QP=%d", qt);
				if (qt==13) fprintf(stdout, " NbBits=%d", nbBits);
				if (bmin && bmax) {
					fprintf(stdout, " Bounds=[");
					PrintFixed(bmin, 0);
					fprintf(stdout, ",");
					PrintFixed(bmax, 0);
					fprintf(stdout, "]");
				}
			}
		}
#endif /*GPAC_DISABLE_BIFS*/
		fprintf(stdout, "\n");
	}
	fprintf(stdout, "}\n\n");

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
		fprintf(stdout, "X3D scene graph disabled in this build of GPAC\n");
		return;
#endif
	} else if (graph_type==2) {
#ifdef GPAC_DISABLE_SVG
		fprintf(stdout, "SVG scene graph disabled in this build of GPAC\n");
		return;
#else
		start_tag = GF_NODE_RANGE_FIRST_SVG;
		end_tag = GF_NODE_RANGE_LAST_SVG;
#endif
	} else {
#ifdef GPAC_DISABLE_VRML
		fprintf(stdout, "VRML/MPEG-4 scene graph disabled in this build of GPAC\n");
		return;
#else
		start_tag = GF_NODE_RANGE_FIRST_MPEG4;
		end_tag = TAG_LastImplementedMPEG4;
#endif
	}
	nb_in = nb_not_in = 0;
	sg = gf_sg_new();
	
	if (graph_type==1) {
		fprintf(stdout, "Available X3D nodes in this build (dumping):\n");
	} else if (graph_type==2) {
		fprintf(stdout, "Available SVG nodes in this build (dumping and LASeR coding):\n");
	} else {
		fprintf(stdout, "Available MPEG-4 nodes in this build (encoding/decoding/dumping):\n");
	}
	for (i=start_tag; i<end_tag; i++) {
		node = gf_node_new(sg, i);
		if (node) {
			gf_node_register(node, NULL);
			fprintf(stdout, " %s\n", gf_node_get_class_name(node));
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
		fprintf(stdout, "\n%d nodes supported\n", nb_in);
	} else {
		fprintf(stdout, "\n%d nodes supported - %d nodes not supported\n", nb_in, nb_not_in);
	}
#else
	fprintf(stdout, "\nNo scene graph enabled in this MP4Box build\n");
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
		gf_isom_dump(file, stdout);
	}
}
#endif


#if !defined(GPAC_DISABLE_ISOM_HINTING) && !defined(GPAC_DISABLE_ISOM_DUMP)

void dump_file_rtp(GF_ISOFile *file, char *inName)
{
	u32 i, j;
	FILE *dump;
	char szBuf[1024];

	if (inName) {
		strcpy(szBuf, inName);
		strcat(szBuf, "_rtp.xml");
		dump = gf_f64_open(szBuf, "wt");
	} else {
		dump = stdout;
	}

	fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
	fprintf(dump, "<!-- MP4Box RTP trace -->\n");
	fprintf(dump, "<RTPFile>\n");

	for (i=0; i<gf_isom_get_track_count(file); i++) {
		if (gf_isom_get_media_type(file, i+1) != GF_ISOM_MEDIA_HINT) continue;

		fprintf(dump, "<RTPHintTrack trackID=\"%d\">\n", gf_isom_get_track_id(file, i+1));
		for (j=0; j<gf_isom_get_sample_count(file, i+1); j++) {
			gf_isom_dump_hint_sample(file, i+1, j+1, dump);
		}
		fprintf(dump, "</RTPHintTrack>\n");
	}
	fprintf(dump, "</RTPFile>\n");
	if (inName) fclose(dump);
}
#endif

void dump_file_ts(GF_ISOFile *file, char *inName)
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
		dump = stdout;
	}

	has_error = 0;
	for (i=0; i<gf_isom_get_track_count(file); i++) {	
		Bool has_cts_offset = gf_isom_has_time_offset(file, i+1);

		fprintf(dump, "#dumping track ID %d timing: Num DTS CTS Size RAP\n", gf_isom_get_track_id(file, i+1));
		count = gf_isom_get_sample_count(file, i+1);
		for (j=0; j<count; j++) {
			u64 dts, cts;
			GF_ISOSample *samp = gf_isom_get_sample_info(file, i+1, j+1, NULL, NULL);
			dts = samp->DTS;
			cts = dts + (s32) samp->CTS_Offset;

			fprintf(dump, "Sample %d\tDTS "LLD"\tCTS "LLD"\t%d\t%d", j+1, LLD_CAST dts, LLD_CAST cts, samp->dataLength, samp->IsRAP);
			if (cts<dts) { fprintf(dump, " #NEGATIVE CTS OFFSET!!!"); has_error = 1;}
		
			gf_isom_sample_del(&samp);

			if (has_cts_offset) {
				for (k=0; k<count; k++) {
					u64 adts, acts;
					if (k==j) continue;
					samp = gf_isom_get_sample_info(file, i+1, k+1, NULL, NULL);
					adts = samp->DTS;
					acts = adts + (s32) samp->CTS_Offset;

					if (adts==dts) { fprintf(dump, " #SAME DTS USED!!!"); has_error = 1; }
					if (acts==cts) { fprintf(dump, " #SAME CTS USED!!! "); has_error = 1; }

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
	if (has_error) fprintf(stdout, "\tFile has CTTS table errors\n");
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
	if (inName) fclose(dump);
}


void dump_timed_text_track(GF_ISOFile *file, u32 trackID, char *inName, Bool is_convert, u32 dump_type)
{
	FILE *dump;
	GF_Err e;
	u32 track;
	char szBuf[1024];

	track = gf_isom_get_track_by_id(file, trackID);
	if (!track) {
		fprintf(stdout, "Cannot find track ID %d\n", trackID);
		return;
	}

	switch (gf_isom_get_media_type(file, track)) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		fprintf(stdout, "Track ID %d is not a 3GPP text track\n", trackID);
		return;
	}

	if (inName) {
		if (is_convert)	
			sprintf(szBuf, "%s.%s", inName, (dump_type==2) ? "svg" : ((dump_type==1) ? "srt" : "ttxt") ) ;
		else
			sprintf(szBuf, "%s_%d_text.%s", inName, trackID, (dump_type==2) ? "svg" : ((dump_type==1) ? "srt" : "ttxt") );
		dump = gf_f64_open(szBuf, "wt");
	} else {
		dump = stdout;
	}
	e = gf_isom_text_dump(file, track, dump, dump_type);
	if (inName) fclose(dump);

	if (e) fprintf(stdout, "Conversion failed (%s)\n", gf_error_to_string(e));
	else fprintf(stdout, "Conversion done\n");
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
		dump = stdout;
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




GF_Err dump_chapters(GF_ISOFile *file, char *inName)
{
	char szName[1024];
	FILE *t;
	u32 i, count;
	count = gf_isom_get_chapter_count(file, 0);
	sprintf(szName, "%s.chap", inName);
	GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Extracting chapters to %s\n", szName));

	t = gf_f64_open(szName, "wt");
	if (!t) return GF_IO_ERR;
	
	for (i=0; i<count;i++) {
		u64 chapter_time;
		const char *name;
		gf_isom_get_chapter(file, 0, i+1, &chapter_time, &name);
		chapter_time /= 1000;
		fprintf(t, "AddChapterBySecond("LLD",%s)\n", chapter_time, name);
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
	fprintf(stdout, "%s type: \"%s\" - %d resource item(s)\n", name, gf_4cc_to_str(brand), (count+(primary_id>0)));
	switch (gf_isom_has_meta_xml(file, root_meta, tk_num)) {
	case 1: fprintf(stdout, "Meta has XML resource\n"); break;
	case 2: fprintf(stdout, "Meta has BinaryXML resource\n"); break;
	}
	if (primary_id) {
		fprintf(stdout, "Primary Item - ID %d\n", primary_id); 
	} 
	for (i=0; i<count; i++) {
		const char *it_name, *mime, *enc, *url, *urn;
		Bool self_ref;
		u32 ID;
		gf_isom_get_meta_item_info(file, root_meta, tk_num, i+1, &ID, NULL, &self_ref, &it_name, &mime, &enc, &url, &urn);
		fprintf(stdout, "Item #%d - ID %d", i+1, ID);
		if (self_ref) fprintf(stdout, " - Self-Reference");
		else if (it_name) fprintf(stdout, " - Name: %s", it_name);
		if (mime) fprintf(stdout, " - MimeType: %s", mime);
		if (enc) fprintf(stdout, " - ContentEncoding: %s", enc);
		fprintf(stdout, "\n");
		if (url) fprintf(stdout, "URL: %s\n", url);
		if (urn) fprintf(stdout, "URN: %s\n", urn);
	}
}

void DumpTrackInfo(GF_ISOFile *file, u32 trackID, Bool full_dump)
{
	Float scale;
	Bool is_od_track = 0;
	u32 trackNum, i, j, max_rate, rate, ts, mtype, msub_type, timescale, sr, nb_ch, count, alt_group, nb_groups;
	u64 time_slice, dur, size;
	u8 bps;
	GF_ESD *esd;
	char sType[5], szDur[50];

	trackNum = gf_isom_get_track_by_id(file, trackID);
	if (!trackNum) {
		fprintf(stdout, "No track with ID %d found\n", trackID);
		return;
	}

	timescale = gf_isom_get_media_timescale(file, trackNum);
	fprintf(stdout, "Track # %d Info - TrackID %d - TimeScale %d - Duration %s\n", trackNum, trackID, timescale, format_duration(gf_isom_get_media_duration(file, trackNum), timescale, szDur));
	if (gf_isom_is_track_in_root_od(file, trackNum) ) fprintf(stdout, "Track is present in Root OD\n");
	if (!gf_isom_is_track_enabled(file, trackNum))  fprintf(stdout, "Track is disabled\n");
	gf_isom_get_media_language(file, trackNum, sType);
	fprintf(stdout, "Media Info: Language \"%s\" - ", GetLanguage(sType) );
	mtype = gf_isom_get_media_type(file, trackNum);
	fprintf(stdout, "Type \"%s:", gf_4cc_to_str(mtype));
	msub_type = gf_isom_get_mpeg4_subtype(file, trackNum, 1);
	if (!msub_type) msub_type = gf_isom_get_media_subtype(file, trackNum, 1);
	fprintf(stdout, "%s\" - %d samples\n", gf_4cc_to_str(msub_type), gf_isom_get_sample_count(file, trackNum));
	
	if (!gf_isom_is_self_contained(file, trackNum, 1)) {
		const char *url, *urn;
		gf_isom_get_data_reference(file, trackNum, 1, &url, &urn);
		fprintf(stdout, "Media Data Location: %s\n", url ? url : urn);
	}

	if (full_dump) {
		const char *handler_name;
		gf_isom_get_handler_name(file, trackNum, &handler_name);
		fprintf(stdout, "Handler name: %s\n", handler_name);
	}

	if (mtype==GF_ISOM_MEDIA_VISUAL) {
		s32 tx, ty;
		u32 w, h;
		gf_isom_get_track_layout_info(file, trackNum, &w, &h, &tx, &ty, NULL);
		fprintf(stdout, "Visual Track layout: x=%d y=%d width=%d height=%d\n", tx, ty, w, h);
	}

	gf_isom_get_audio_info(file, trackNum, 1, &sr, &nb_ch, &bps);
	
	msub_type = gf_isom_get_media_subtype(file, trackNum, 1);
	if ((msub_type==GF_ISOM_SUBTYPE_MPEG4) 
		|| (msub_type==GF_ISOM_SUBTYPE_MPEG4_CRYP) 
		|| (msub_type==GF_ISOM_SUBTYPE_AVC_H264)
		|| (msub_type==GF_ISOM_SUBTYPE_AVC2_H264)
		|| (msub_type==GF_ISOM_SUBTYPE_SVC_H264)
		|| (msub_type==GF_ISOM_SUBTYPE_LSR1)
	)  {
		esd = gf_isom_get_esd(file, trackNum, 1);
		if (!esd) {
			fprintf(stdout, "WARNING: Broken MPEG-4 Track\n");
		} else {
			const char *st = gf_odf_stream_type_name(esd->decoderConfig->streamType);
			if (st) {
				fprintf(stdout, "MPEG-4 Config%s%s Stream - ObjectTypeIndication 0x%02x\n",
							full_dump ? "\n\t" : ": ", st, esd->decoderConfig->objectTypeIndication);
			} else {
				fprintf(stdout, "MPEG-4 Config%sStream Type 0x%02x - ObjectTypeIndication 0x%02x\n",
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
					GF_M4VDecSpecInfo dsi;
					gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
					if (full_dump) fprintf(stdout, "\t");
					w = dsi.width;
					h = dsi.height;
					fprintf(stdout, "MPEG-4 Visual Size %d x %d - %s\n", w, h, gf_m4v_get_profile_name(dsi.VideoPL));
					if (dsi.par_den && dsi.par_num) {
						u32 tw, th;
						gf_isom_get_track_layout_info(file, trackNum, &tw, &th, NULL, NULL, NULL);
						fprintf(stdout, "Pixel Aspect Ratio %d:%d - Indicated track size %d x %d\n", dsi.par_num, dsi.par_den, tw, th);
					}
#else
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					fprintf(stdout, "MPEG-4 Visual Size %d x %d\n", w, h);
#endif

				} else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_AVC) {
#ifndef GPAC_DISABLE_AV_PARSERS
					GF_AVCConfig *avccfg, *svccfg;
					GF_AVCConfigSlot *slc;
					s32 par_n, par_d;
#endif

					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					if (full_dump) fprintf(stdout, "\t");
					fprintf(stdout, "AVC/H264 Video - Visual Size %d x %d\n", w, h);
#ifndef GPAC_DISABLE_AV_PARSERS
					avccfg = gf_isom_avc_config_get(file, trackNum, 1);
					svccfg = gf_isom_svc_config_get(file, trackNum, 1);
					if (!avccfg && !svccfg) {
						fprintf(stdout, "\n\n\tNon-compliant AVC track: SPS/PPS not found in sample description\n");
					} else if (avccfg) {
						fprintf(stdout, "\tAVC Info: %d SPS - %d PPS", gf_list_count(avccfg->sequenceParameterSets) , gf_list_count(avccfg->pictureParameterSets) );
						fprintf(stdout, " - Profile %s @ Level %g\n", gf_avc_get_profile_name(avccfg->AVCProfileIndication), ((Double)avccfg->AVCLevelIndication)/10.0 );
						fprintf(stdout, "\tNAL Unit length bits: %d\n", 8*avccfg->nal_unit_size);
						slc = gf_list_get(avccfg->sequenceParameterSets, 0);
						if (slc) {
							gf_avc_get_sps_info(slc->data, slc->size, NULL, NULL, NULL, &par_n, &par_d);
							if ((par_n>0) && (par_d>0)) {
								u32 tw, th;
								gf_isom_get_track_layout_info(file, trackNum, &tw, &th, NULL, NULL, NULL);
								fprintf(stdout, "\tPixel Aspect Ratio %d:%d - Indicated track size %d x %d\n", par_n, par_d, tw, th);
							}
						}
						if (avccfg->chroma_bit_depth) {
							fprintf(stdout, "\tChroma format %d - Luma bit depth %d - chroma bot depth %d\n", avccfg->chroma_format, avccfg->luma_bit_depth, avccfg->chroma_bit_depth);
						}
						gf_odf_avc_cfg_del(avccfg);
					}
					if (svccfg) {
						fprintf(stdout, "\n\tSVC Info: %d SPS - %d PPS - Profile %s @ Level %g\n", gf_list_count(svccfg->sequenceParameterSets) , gf_list_count(svccfg->pictureParameterSets), gf_avc_get_profile_name(svccfg->AVCProfileIndication), ((Double)svccfg->AVCLevelIndication)/10.0 );
						fprintf(stdout, "\tSVC NAL Unit length bits: %d\n", 8*svccfg->nal_unit_size);
						for (i=0; i<gf_list_count(svccfg->sequenceParameterSets); i++) {
							slc = gf_list_get(svccfg->sequenceParameterSets, i);
							if (slc) {
								u32 s_w, s_h, sps_id;
								gf_avc_get_sps_info(slc->data, slc->size, &sps_id, &s_w, &s_h, &par_n, &par_d);
								fprintf(stdout, "\t\tSSPS ID %d - Visual Size %d x %d\n", sps_id, s_w, s_h);
								if ((par_n>0) && (par_d>0)) {
									u32 tw, th;
									gf_isom_get_track_layout_info(file, trackNum, &tw, &th, NULL, NULL, NULL);
									fprintf(stdout, "\tPixel Aspect Ratio %d:%d - Indicated track size %d x %d\n", par_n, par_d, tw, th);
								}
							}
						}
						gf_odf_avc_cfg_del(svccfg);
					}
#endif /*GPAC_DISABLE_AV_PARSERS*/
				} 
				/*OGG media*/
				else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_MEDIA_OGG) {
					char *szName;
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					if (full_dump) fprintf(stdout, "\t");
					if (!strnicmp(&esd->decoderConfig->decoderSpecificInfo->data[3], "theora", 6)) szName = "Theora";
					else szName = "Unknown";
					fprintf(stdout, "Ogg/%s video / GPAC Mux  - Visual Size %d x %d\n", szName, w, h);
				}
				else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_IMAGE_JPEG) {
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					fprintf(stdout, "JPEG Stream - Visual Size %d x %d\n", w, h);
				}
				else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_IMAGE_PNG) {
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					fprintf(stdout, "PNG Stream - Visual Size %d x %d\n", w, h);
				}
				else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_IMAGE_JPEG_2000) {
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					fprintf(stdout, "JPEG2000 Stream - Visual Size %d x %d\n", w, h);
				}
				if (!w || !h) {
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					if (full_dump) fprintf(stdout, "\t");
					fprintf(stdout, "Visual Size %d x %d\n", w, h);
				}
				if (gf_isom_get_rvc_config(file, trackNum, 1, &rvc_predef, NULL, NULL, NULL)==GF_OK) {
					fprintf(stdout, "Has RVC signaled - Predefined configuration %d\n", rvc_predef);
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
					if (full_dump) fprintf(stdout, "\t");
					if (e) fprintf(stdout, "Corrupted AAC Config\n");
					else {
						fprintf(stdout, "MPEG-%d Audio %s - %d Channel(s) - SampleRate %d", is_mp2 ? 2 : 4, gf_m4a_object_type_name(a_cfg.base_object_type), a_cfg.nb_chan, a_cfg.base_sr);
						if (a_cfg.has_sbr) fprintf(stdout, " - SBR SampleRate %d", a_cfg.sbr_sr);
						if (a_cfg.has_ps) fprintf(stdout, " - PS");
						fprintf(stdout, "\n");
					}
#else
					fprintf(stdout, "MPEG-2/4 Audio - %d Channels - SampleRate %d\n", nb_ch, sr);
#endif
					break;
				case GPAC_OTI_AUDIO_MPEG2_PART3:
				case GPAC_OTI_AUDIO_MPEG1:
					if (msub_type == GF_ISOM_SUBTYPE_MPEG4_CRYP) {
						fprintf(stdout, "MPEG-1/2 Audio - %d Channels - SampleRate %d\n", nb_ch, sr);
					} else {
#ifndef GPAC_DISABLE_AV_PARSERS
						GF_ISOSample *samp = gf_isom_get_sample(file, trackNum, 1, &oti);
						if (samp) {
							oti = GF_4CC((u8)samp->data[0], (u8)samp->data[1], (u8)samp->data[2], (u8)samp->data[3]);
							if (full_dump) fprintf(stdout, "\t");
							fprintf(stdout, "%s Audio - %d Channel(s) - SampleRate %d - Layer %d\n",
								gf_mp3_version_name(oti),
								gf_mp3_num_channels(oti), 
								gf_mp3_sampling_rate(oti), 
								gf_mp3_layer(oti)
							);
							gf_isom_sample_del(&samp);
						} else {
							fprintf(stdout, "\n\tError fetching sample: %s\n", gf_error_to_string(gf_isom_last_error(file)) );
						}
#else
						fprintf(stdout, "MPEG-1/2 Audio - %d Channels - SampleRate %d\n", nb_ch, sr);
#endif
					}
					break;
				/*OGG media*/
				case GPAC_OTI_MEDIA_OGG:
				{
					char *szName;
					if (full_dump) fprintf(stdout, "\t");
					if (!strnicmp(&esd->decoderConfig->decoderSpecificInfo->data[3], "vorbis", 6)) szName = "Vorbis";
					else if (!strnicmp(&esd->decoderConfig->decoderSpecificInfo->data[2], "Speex", 5)) szName = "Speex";
					else if (!strnicmp(&esd->decoderConfig->decoderSpecificInfo->data[2], "Flac", 4)) szName = "Flac";
					else szName = "Unknown";
					fprintf(stdout, "Ogg/%s audio / GPAC Mux - Sample Rate %d - %d channel(s)\n", szName, sr, nb_ch);
				}
					break;
				case GPAC_OTI_AUDIO_EVRC_VOICE: fprintf(stdout, "EVRC Audio - Sample Rate 8000 - 1 channel\n"); break;
				case GPAC_OTI_AUDIO_SMV_VOICE: fprintf(stdout, "SMV Audio - Sample Rate 8000 - 1 channel\n"); break;
				case GPAC_OTI_AUDIO_13K_VOICE: fprintf(stdout, "QCELP Audio - Sample Rate 8000 - 1 channel\n"); break;
				/*packetVideo hack for EVRC...*/
				case 0xD1: 
					if (esd->decoderConfig->decoderSpecificInfo && (esd->decoderConfig->decoderSpecificInfo->dataLength==8)
					&& !strnicmp(esd->decoderConfig->decoderSpecificInfo->data, "pvmm", 4)) {
						if (full_dump) fprintf(stdout, "\t");
						fprintf(stdout, "EVRC Audio (PacketVideo Mux) - Sample Rate 8000 - 1 channel\n"); 
					}
					break;
				}
			}
			else if (esd->decoderConfig->streamType==GF_STREAM_SCENE) {
				if (esd->decoderConfig->objectTypeIndication<=4) {
					GF_BIFSConfig *b_cfg = gf_odf_get_bifs_config(esd->decoderConfig->decoderSpecificInfo, esd->decoderConfig->objectTypeIndication);
					fprintf(stdout, "BIFS Scene description - %s stream\n", b_cfg->elementaryMasks ? "Animation" : "Command"); 
					if (full_dump && !b_cfg->elementaryMasks) {
						fprintf(stdout, "\tWidth %d Height %d Pixel Metrics %s\n", b_cfg->pixelWidth, b_cfg->pixelHeight, b_cfg->pixelMetrics ? "yes" : "no"); 
					}
					gf_odf_desc_del((GF_Descriptor *)b_cfg);
				} else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_SCENE_AFX) {
					u8 tag = esd->decoderConfig->decoderSpecificInfo ? esd->decoderConfig->decoderSpecificInfo->data[0] : 0xFF;
					const char *afxtype = gf_afx_get_type_description(tag);
					fprintf(stdout, "AFX Stream - type %s (%d)\n", afxtype, tag); 
				} else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_FONT) {
					fprintf(stdout, "Font Data stream\n"); 
				} else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_SCENE_LASER) {
					GF_LASERConfig l_cfg;
					gf_odf_get_laser_config(esd->decoderConfig->decoderSpecificInfo, &l_cfg);
					fprintf(stdout, "LASER Stream - %s\n", l_cfg.newSceneIndicator ? "Full Scene" : "Scene Segment"); 
				} else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_TEXT_MPEG4) {
					fprintf(stdout, "MPEG-4 Streaming Text stream\n"); 
				} else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_SCENE_SYNTHESIZED_TEXTURE) {
					fprintf(stdout, "Synthetized Texture stream stream\n"); 
				} else {
					fprintf(stdout, "Unknown Systems stream OTI %d\n", esd->decoderConfig->objectTypeIndication); 
				}
			}

			/*sync is only valid if we open all tracks to take care of default MP4 sync..*/
			if (!full_dump) {
				if (!esd->OCRESID || (esd->OCRESID == esd->ESID))
					fprintf(stdout, "Self-synchronized\n");
				else
					fprintf(stdout, "Synchronized on stream %d\n", esd->OCRESID);
			} else {
				fprintf(stdout, "\tDecoding Buffer size %d - Average bitrate %d kbps - Max Bitrate %d kbps\n", esd->decoderConfig->bufferSizeDB, esd->decoderConfig->avgBitrate/1000, esd->decoderConfig->maxBitrate/1000);
				if (esd->dependsOnESID)
					fprintf(stdout, "\tDepends on stream %d for decoding\n", esd->dependsOnESID);
				else
					fprintf(stdout, "\tNo stream dependencies for decoding\n");

				fprintf(stdout, "\tStreamPriority %d\n", esd->streamPriority);
				if (esd->URLString) fprintf(stdout, "\tRemote Data Source %s\n", esd->URLString);
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
					fprintf(stdout, "\n*Encrypted stream - ISMA scheme %s (version %d)\n", gf_4cc_to_str(scheme_type), version);
					if (scheme_URI) fprintf(stdout, "scheme location: %s\n", scheme_URI);
					if (KMS_URI) {
						if (!strnicmp(KMS_URI, "(key)", 5)) fprintf(stdout, "KMS location: key in file\n");
						else fprintf(stdout, "KMS location: %s\n", KMS_URI);
					}
					fprintf(stdout, "Selective Encryption: %s\n", use_sel_enc ? "Yes" : "No");
					if (IV_size) fprintf(stdout, "Initialization Vector size: %d bits\n", IV_size*8);
				} else if (gf_isom_is_omadrm_media(file, trackNum, 1)) {
					const char *textHdrs;
					u32 enc_type, hdr_len;
					u64 orig_len;
					fprintf(stdout, "\n*Encrypted stream - OMA DRM\n");
					gf_isom_get_omadrm_info(file, trackNum, 1, NULL, NULL, NULL, &scheme_URI, &KMS_URI, &textHdrs, &hdr_len, &orig_len, &enc_type, &use_sel_enc, &IV_size, NULL);
					fprintf(stdout, "Rights Issuer: %s\n", KMS_URI);
					fprintf(stdout, "Content ID: %s\n", scheme_URI);
					if (textHdrs) {
						u32 i, offset;
						const char *start = textHdrs;
						fprintf(stdout, "OMA Textual Headers:\n");
						i=offset=0;
						while (i<hdr_len) {
							if (start[i]==0) {
								fprintf(stdout, "\t%s\n", start+offset);
								offset=i+1;
							}
							i++;
						}
						fprintf(stdout, "\t%s\n", start+offset);
					}
					if (orig_len) fprintf(stdout, "Original media size "LLD"\n", LLD_CAST orig_len);
					fprintf(stdout, "Encryption algorithm %s\n", (enc_type==1) ? "AEA 128 CBC" : (enc_type ? "AEA 128 CTR" : "None"));


					fprintf(stdout, "Selective Encryption: %s\n", use_sel_enc ? "Yes" : "No");
					if (IV_size) fprintf(stdout, "Initialization Vector size: %d bits\n", IV_size*8);
				} else {
					fprintf(stdout, "\n*Encrypted stream - unknown scheme %s\n", gf_4cc_to_str(gf_isom_is_media_encrypted(file, trackNum, 1) ));
				}
			}

		}
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_H263) {
		u32 w, h;
		gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
		fprintf(stdout, "\t3GPP H263 stream - Resolution %d x %d\n", w, h);
	} else if (msub_type == GF_4CC('m','j','p','2')) {
		u32 w, h;
		gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
		fprintf(stdout, "\tMotionJPEG2000 stream - Resolution %d x %d\n", w, h);
	} else if ((msub_type == GF_ISOM_SUBTYPE_3GP_AMR) || (msub_type == GF_ISOM_SUBTYPE_3GP_AMR_WB)) {
		fprintf(stdout, "\t3GPP AMR%s stream - Sample Rate %d - %d channel(s) %d bits per samples\n", (msub_type == GF_ISOM_SUBTYPE_3GP_AMR_WB) ? " Wide Band" : "", sr, nb_ch, (u32) bps);
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_EVRC) {
		fprintf(stdout, "\t3GPP EVRC stream - Sample Rate %d - %d channel(s) %d bits per samples\n", sr, nb_ch, (u32) bps);
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_QCELP) {
		fprintf(stdout, "\t3GPP QCELP stream - Sample Rate %d - %d channel(s) %d bits per samples\n", sr, nb_ch, (u32) bps);
	} else if (msub_type == GF_ISOM_SUBTYPE_AC3) {
		u32 br = 0;
		Bool lfe = 0;
#ifndef GPAC_DISABLE_AV_PARSERS
		GF_AC3Config *ac3 = gf_isom_ac3_config_get(file, trackNum, 1);
		if (ac3) {
			nb_ch = gf_ac3_get_channels(ac3->acmod);
			br = gf_ac3_get_bitrate(ac3->brcode);
			lfe = ac3->lfon;
			gf_free(ac3);
		}
#endif
		fprintf(stdout, "\tAC3 stream - Sample Rate %d - %d%s channel(s) - bitrate %d\n", sr, nb_ch, lfe ? ".1" : "", br);
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_SMV) {
		fprintf(stdout, "\t3GPP SMV stream - Sample Rate %d - %d channel(s) %d bits per samples\n", sr, nb_ch, (u32) bps);
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_DIMS) {
		u32 w, h;
		GF_DIMSDescription dims;
		gf_isom_get_visual_info(file, trackNum, 1, &w, &h);

		gf_isom_get_dims_description(file, trackNum, 1, &dims);
		fprintf(stdout, "\t3GPP DIMS stream - size %d x %d - Profile %d - Level %d\n", w, h, dims.profile, dims.level);
		fprintf(stdout, "\tpathComponents: %d - useFullRequestHost: %s\n", dims.pathComponents, dims.fullRequestHost ? "yes" : "no");
		fprintf(stdout, "\tstream type: %s - redundant: %s\n", dims.streamType ? "primary" : "secondary", (dims.containsRedundant==1) ? "main" : ((dims.containsRedundant==2) ? "redundant" : "main+redundant") );
		if (dims.textEncoding[0]) fprintf(stdout, "\ttext encoding %s\n", dims.textEncoding);
		if (dims.contentEncoding[0]) fprintf(stdout, "\tcontent encoding %s\n", dims.contentEncoding);
		if (dims.content_script_types) fprintf(stdout, "\tscript languages %s\n", dims.content_script_types);
	} else if (mtype==GF_ISOM_MEDIA_HINT) {
		u32 refTrack;
		s32 i, refCount = gf_isom_get_reference_count(file, trackNum, GF_ISOM_REF_HINT);
		if (refCount) {
			fprintf(stdout, "Streaming Hint Track for track%s ", (refCount>1) ? "s" :"");
			for (i=0; i<refCount; i++) {
				gf_isom_get_reference(file, trackNum, GF_ISOM_REF_HINT, i+1, &refTrack);
				if (i) fprintf(stdout, " - ");
				fprintf(stdout, "ID %d", gf_isom_get_track_id(file, refTrack));
			}
			fprintf(stdout, "\n");
		} else {
			fprintf(stdout, "Streaming Hint Track (no refs)\n");
		}
#ifndef GPAC_DISABLE_ISOM_HINTING
		refCount = gf_isom_get_payt_count(file, trackNum);
		for (i=0;i<refCount;i++) {
			const char *name = gf_isom_get_payt_info(file, trackNum, i+1, &refTrack);
			fprintf(stdout, "\tPayload ID %d: type %s\n", refTrack, name);
		}
#endif
	} else if (mtype==GF_ISOM_MEDIA_FLASH) {
		fprintf(stdout, "Macromedia Flash Movie\n");
	} else if ((mtype==GF_ISOM_MEDIA_TEXT) || (mtype==GF_ISOM_MEDIA_SUBT)) {
		u32 w, h;
		s16 l;
		s32 tx, ty;
		gf_isom_get_track_layout_info(file, trackNum, &w, &h, &tx, &ty, &l);
		fprintf(stdout, "3GPP/MPEG-4 Timed Text - Size %d x %d - Translation X=%d Y=%d - Layer %d\n", w, h, tx, ty, l);
	} else if (mtype == GF_ISOM_MEDIA_META) {
		Bool is_xml = 0;
		const char *mime_or_namespace = NULL;
		const char *content_encoding = NULL;
		const char *schema_loc = NULL;
		gf_isom_get_timed_meta_data_info(file, trackNum, 1, &is_xml, &mime_or_namespace, &content_encoding, &schema_loc);
		fprintf(stdout, "%s Metadata stream\n\t%s %s\n\tencoding %s", is_xml ? "Xml" : "Text", is_xml ? "namespace" : "mime-type", mime_or_namespace, content_encoding);
		if (is_xml && schema_loc != NULL)
			fprintf(stdout, "\n\tschema %s\n", schema_loc);
		fprintf(stdout, "\n");
	} else {
		GF_GenericSampleDescription *udesc = gf_isom_get_generic_sample_description(file, trackNum, 1);
		if (udesc) {
			if (mtype==GF_ISOM_MEDIA_VISUAL) {
				fprintf(stdout, "Visual Track - Compressor \"%s\" - Resolution %d x %d\n", udesc->compressor_name, udesc->width, udesc->height);
			} else if (mtype==GF_ISOM_MEDIA_AUDIO) {
				fprintf(stdout, "Audio Track - Sample Rate %d - %d channel(s)\n", udesc->samplerate, udesc->nb_channels);
			} else {
				fprintf(stdout, "Unknown media type\n");
			}
			fprintf(stdout, "\tVendor code \"%s\" - Version %d - revision %d\n", gf_4cc_to_str(udesc->vendor_code), udesc->version, udesc->revision);
			if (udesc->extension_buf) {
				fprintf(stdout, "\tCodec configuration data size: %d bytes\n", udesc->extension_buf_size);
				gf_free(udesc->extension_buf);
			}
			gf_free(udesc);
		} else {
			fprintf(stdout, "Unknown track type\n");
		}
	}

	DumpMetaItem(file, 0, trackNum, "Track Meta");

	gf_isom_get_track_switch_group_count(file, trackNum, &alt_group, &nb_groups);
	if (alt_group) {
		fprintf(stdout, "Alternate Group ID %d\n", alt_group);
		for (i=0; i<nb_groups; i++) {
			u32 nb_crit, switchGroupID; 
			const u32 *criterias = gf_isom_get_track_switch_parameter(file, trackNum, i+1, &switchGroupID, &nb_crit);
			if (!nb_crit) {
				fprintf(stdout, "\tNo criteria in %s group\n", switchGroupID ? "switch" : "alternate");
			} else {
				if (switchGroupID) {
					fprintf(stdout, "\tSwitchGroup ID %d criterias: ", switchGroupID);
				} else {
					fprintf(stdout, "\tAlternate Group criterias: ");
				}
				for (j=0; j<nb_crit; j++) {
					if (j) fprintf(stdout, " ");
					fprintf(stdout, "%s", gf_4cc_to_str(criterias[j]) );
				}
				fprintf(stdout, "\n");
			}
		}
	}

	if (!full_dump) {
		fprintf(stdout, "\n");
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
	fprintf(stdout, "\nComputed info from media:\n");
	scale = 1000;
	scale /= ts;
	dur = (u64) (scale * (s64)dur);
	fprintf(stdout, "\tTotal size "LLU" bytes - Total samples duration "LLU" ms\n", size, dur);
	if (!dur) {
		fprintf(stdout, "\n");
		return;
	}
	/*rate in byte, dur is in ms*/
	rate = (u32) ((size * 8 * 1000) / dur);
	max_rate *= 8;
	if (rate >= 1500) {
		rate /= 1000;
		max_rate /= 1000;
		fprintf(stdout, "\tAverage rate %d kbps - Max Rate %d kbps\n", rate, max_rate);
	} else {
		fprintf(stdout, "\tAverage rate %d bps - Max Rate %d bps\n", rate, max_rate);
	}

	{
		u32 dmin, dmax, davg, smin, smax, savg;
		gf_isom_get_chunks_infos(file, trackNum, &dmin, &davg, &dmax, &smin, &savg, &smax);
		fprintf(stdout, "\tChunk durations: min %d ms - max %d ms - average %d ms\n", (1000*dmin)/ts, (1000*dmax)/ts, (1000*davg)/ts);
		fprintf(stdout, "\tChunk sizes (bytes): min %d - max %d - average %d\n", smin, smax, savg);
	}
	fprintf(stdout, "\n");

	count = gf_isom_get_chapter_count(file, trackNum);
	if (count) {
		char szDur[20];
		const char *name;
		u64 time;
		fprintf(stdout, "\nChapters:\n");
		for (j=0; j<count; j++) {
			gf_isom_get_chapter(file, trackNum, j+1, &time, &name);
			fprintf(stdout, "\tChapter #%d - %s - \"%s\"\n", j+1, format_duration(time, 1000, szDur), name);
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
            fprintf(stdout, "File is a segment: \n");
            fprintf(stdout, "\tSegment Brand %s - version %d\n", gf_4cc_to_str(brand), min);
        } else {
		    fprintf(stdout, "File has no movie (moov) - static data container\n");
        }
		return;
	}

	timescale = gf_isom_get_timescale(file);
	fprintf(stdout, "* Movie Info *\n\tTimescale %d - Duration %s\n\t%d track(s)\n",
		timescale, format_duration(gf_isom_get_duration(file), timescale, szDur), gf_isom_get_track_count(file));
	
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (gf_isom_is_fragmented(file)) {
		fprintf(stdout, "\tFragmented File: yes - duration %s\n%d fragments - %d SegmentIndexes\n", format_duration(gf_isom_get_fragmented_duration(file), timescale, szDur), gf_isom_get_fragments_count(file, 0) , gf_isom_get_fragments_count(file, 1) );
	} else {
		fprintf(stdout, "\tFragmented File: no\n");
	}
#endif

	if (gf_isom_moov_first(file))
		fprintf(stdout, "\tFile suitable for progressive download (moov before mdat)\n");

	if (gf_isom_get_brand_info(file, &brand, &min, NULL) == GF_OK) {
		fprintf(stdout, "\tFile Brand %s - version %d\n", gf_4cc_to_str(brand), min);
	}
	gf_isom_get_creation_time(file, &create, &modif);
	fprintf(stdout, "\tCreated: %s", format_date(create, szDur));
	//fprintf(stdout, "\tModified: %s", format_date(modif, szDur));
	fprintf(stdout, "\n");

	DumpMetaItem(file, 0, 0, "Moov Meta");

	iod = (GF_InitialObjectDescriptor *) gf_isom_get_root_od(file);
	if (iod) {
		u32 desc_size = gf_odf_desc_size((GF_Descriptor *)iod);
		if (iod->tag == GF_ODF_IOD_TAG) {
			fprintf(stdout, "File has root IOD (%d bytes)\n", desc_size);
			fprintf(stdout, "Scene PL 0x%02x - Graphics PL 0x%02x - OD PL 0x%02x\n", iod->scene_profileAndLevel, iod->graphics_profileAndLevel, iod->OD_profileAndLevel);
			fprintf(stdout, "Visual PL: %s (0x%02x)\n", gf_m4v_get_profile_name(iod->visual_profileAndLevel), iod->visual_profileAndLevel);
			fprintf(stdout, "Audio PL: %s (0x%02x)\n", gf_m4a_get_profile_name(iod->audio_profileAndLevel), iod->audio_profileAndLevel);
			//fprintf(stdout, "inline profiles included %s\n", iod->inlineProfileFlag ? "yes" : "no");
		} else {
			fprintf(stdout, "File has root OD (%d bytes)\n", desc_size);
		}
		if (!gf_list_count(iod->ESDescriptors)) fprintf(stdout, "No streams included in root OD\n");
		gf_odf_desc_del((GF_Descriptor *) iod);
	} else {
		fprintf(stdout, "File has no MPEG4 IOD/OD\n");
	}
	if (gf_isom_is_JPEG2000(file)) fprintf(stdout, "File is JPEG 2000\n");

	count = gf_isom_get_copyright_count(file);
	if (count) {
		const char *lang, *note;
		fprintf(stdout, "\nCopyrights:\n");
		for (i=0; i<count; i++) {
			gf_isom_get_copyright(file, i+1, &lang, &note);
			fprintf(stdout, "\t(%s) %s\n", lang, note);
		}
	}
	
	count = gf_isom_get_chapter_count(file, 0);
	if (count) {
		char szDur[20];
		const char *name;
		u64 time;
		fprintf(stdout, "\nChapters:\n");
		for (i=0; i<count; i++) {
			gf_isom_get_chapter(file, 0, i+1, &time, &name);
			fprintf(stdout, "\tChapter #%d - %s - \"%s\"\n", i+1, format_duration(time, 1000, szDur), name);
		}
	}

	if (gf_isom_apple_get_tag(file, 0, &tag, &tag_len) == GF_OK) {
		fprintf(stdout, "\niTunes Info:\n");
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_NAME, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tName: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_ARTIST, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tArtist: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_ALBUM, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tAlbum: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_COMMENT, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tComment: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_TRACK, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tTrack: %d / %d\n", tag[3], tag[5]);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_COMPOSER, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tComposer: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_WRITER, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tWriter: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_ALBUM_ARTIST, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tAlbum Artist: %s\n", tag);
		
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_GENRE, &tag, &tag_len)==GF_OK) {
			if (tag[0]) {
				fprintf(stdout, "\tGenre: %s\n", tag);
			} else {
				fprintf(stdout, "\tGenre: %s\n", id3_get_genre(((u8*)tag)[1]));
			}
		}
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_COMPILATION, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tCompilation: %s\n", tag[0] ? "Yes" : "No");
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_GAPLESS, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tGapless album: %s\n", tag[0] ? "Yes" : "No");
		
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_CREATED, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tCreated: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_DISK, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tDisk: %d / %d\n", tag[3], tag[5]);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_TOOL, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tEncoder Software: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_ENCODER, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tEncoded by: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_TEMPO, &tag, &tag_len)==GF_OK) {
			if (tag[0]) {
				fprintf(stdout, "\tTempo (BPM): %s\n", tag);
			} else {
				fprintf(stdout, "\tTempo (BPM): %d\n", tag[1]);
			}
		}
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_TRACKNUMBER, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tTrackNumber: %d / %d\n", tag[3], tag[5]);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_TRACK, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tTrack: %s\n", tag);
		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_GROUP, &tag, &tag_len)==GF_OK) fprintf(stdout, "\tGroup: %s\n", tag);

		if (gf_isom_apple_get_tag(file, GF_ISOM_ITUNE_COVER_ART, &tag, &tag_len)==GF_OK) {
			if (tag_len>>31) fprintf(stdout, "\tCover Art: PNG File\n");
			else fprintf(stdout, "\tCover Art: JPEG File\n");
		}
	}

	fprintf(stdout, "\n");
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		DumpTrackInfo(file, gf_isom_get_track_id(file, i+1), 0);
	}
}

#endif /*defined(GPAC_DISABLE_ISOM) || defined(GPAC_DISABLE_ISOM_WRITE)*/


#ifndef GPAC_DISABLE_MPEG2TS
#include <gpac/internal/isomedia_dev.h>

typedef struct
{	
	Bool start_indexing;

	/* For indexing the TS*/
	Double segment_duration;
	Bool segment_at_rap;
	u32 subsegs_per_segment;
	char *seg_name;
	char *seg_ext;
	Bool use_url_template;
	char *init_seg_name;
	Bool single_segment;

	u32 segment_index;

	FILE *index_file;
	char index_file_name[GF_MAX_PATH];
	GF_BitStream *index_bs;

	char mpd_file_name[GF_MAX_PATH];
	FILE *mpd_file;
	/* temporary file to store the MPD segment description before writing the header */
	FILE *mpd_segs;

	u32 represantation_idx;

	u32 reference_pid;
	GF_M2TS_PES *reference_stream;
	
	u32 nb_pes_in_segment;
	/* earliest presentation time for the whole segment */
	u64 first_PTS;

	/* earliest presentation time for the subsegment being processed */
	u64 base_PTS;
	/* byte offset for the start of subsegment being processed */
	u32 base_offset;
	/* last presentation time for the subsegment being processed (before the next subsegment is started) */
	u64 last_PTS;
	/* last decoding time for the subsegment being processed */
	u64 last_DTS;
	/* byte offset for the last PES packet for the subsegment being processed */
	u32 last_offset;

	/* earliest presentation time for the previous subsegment */
	u64 prev_base_PTS;
	/* byte offset for the start of the previous subsegment */
	u32 prev_base_offset;
	/* last presentation time for the previous subsegment */
	u64 prev_last_PTS;
	/* byte offset for the last PES packet for the previous subsegment */
	u32 prev_last_offset;

	/* indicates if the current subsegment contains a SAP and its SAP type*/
	u32 SAP_type;
	/* indicates if the first PES in the current subsegment is a SAP*/
	Bool first_pes_sap;
	/* Presentation time for the first RAP encountered in the subsegment */
	u64 first_SAP_PTS;
	/* byte offset for the first RAP encountered in the subsegment */
	u32 first_SAP_offset;
	u64 prev_last_SAP_PTS;
	u32 prev_last_SAP_offset;
	u64 last_SAP_PTS;
	u32 last_SAP_offset;

	/*Interpolated PCR value for the pcrb*/
	u64 interpolated_pcr_value;
	u64 last_pcr_value;

	/* information about the first PAT found in the subsegment */
	u32 last_pat_position;
	u32 first_pat_position;
	u32 prev_last_pat_position;
	Bool first_pat_position_valid;
	u32 pat_version;

	/* information about the first CAT found in the subsegment */
	u32 last_cat_position;
	u32 first_cat_position;
	u32 prev_last_cat_position;
	Bool first_cat_position_valid;
	u32 cat_version;

	/* information about the first PMT found in the subsegment */
	u32 last_pmt_position;
	u32 first_pmt_position;
	u32 prev_last_pmt_position;
	Bool first_pmt_position_valid;
	u32 pmt_version;

	/* information about the first PCR found in the subsegment */
	u32 last_pcr_position;
	u32 first_pcr_position;
	Bool first_pcr_position_valid;
	u32 prev_last_pcr_position;

	//GF_List *sidxs;
	GF_SegmentIndexBox *sidx;
	
	//GF_List *pcrbs;
	GF_PcrInfoBox *pcrb;
} GF_M2TS_IndexingInfo;

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

	GF_M2TS_IndexingInfo index_info;

} GF_M2TS_Dump;

/* Initializes an SIDX */
static GF_SegmentIndexBox *m2ts_sidx_new(u32 pid, u64 PTS, u64 position)
{						
	GF_SegmentIndexBox *sidx = (GF_SegmentIndexBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_SIDX);
	sidx->reference_ID = pid;
	/* timestamps in MPEG-2 are expressed in 90 kHz timescale */
	sidx->timescale = 90000;
	/* first encountered PTS on the PID for this subsegment */
	sidx->earliest_presentation_time = PTS;
	sidx->first_offset = position;
	return sidx;
}

static void m2ts_sidx_add_entry(GF_SegmentIndexBox *sidx, Bool ref_type, 
								u32 size, u32 duration, Bool first_is_SAP, u32 sap_type, u32 RAP_delta_time)
{
	GF_SIDXReference *ref;
	sidx->nb_refs++;
	sidx->refs = gf_realloc(sidx->refs, sidx->nb_refs*sizeof(GF_SIDXReference));
	ref = &(sidx->refs[sidx->nb_refs-1]);
	ref->reference_type = ref_type;
	ref->reference_size = size;
	ref->subsegment_duration = duration;
	ref->starts_with_SAP = first_is_SAP;
	ref->SAP_type = sap_type;
	ref->SAP_delta_time = (sap_type ? RAP_delta_time: 0);
}

static void m2ts_pcrb_add_entry(GF_PcrInfoBox *pcrb, u64 interpolatedPCR)
{
	pcrb->subsegment_count++;
	pcrb->pcr_values = gf_realloc(pcrb->pcr_values, pcrb->subsegment_count*sizeof(u64));
	
	pcrb->pcr_values[pcrb->subsegment_count-1] = interpolatedPCR;
}

static void m2ts_sidx_update_prev_entry_duration(GF_SegmentIndexBox *sidx, u32 duration)
{
	GF_SIDXReference *ref;
	if (sidx->nb_refs == 0) return;
	ref = &(sidx->refs[sidx->nb_refs-1]);
	ref->subsegment_duration = duration;
}

static void m2ts_sidx_finalize_size(GF_M2TS_IndexingInfo *index_info, u64 file_size)
{
	GF_SIDXReference *ref;
	if (index_info->sidx->nb_refs == 0) return;
	ref = &(index_info->sidx->refs[index_info->sidx->nb_refs-1]);
	ref->reference_size = (u32)(file_size - index_info->prev_base_offset);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Subsegment: position-range ajdustment:%d-%d (%d bytes)\n", index_info->prev_base_offset, (u32)file_size, ref->reference_size));
}

static void m2ts_sidx_flush_entry(GF_M2TS_IndexingInfo *index_info) 
{
	u32 size; 
	u32 duration, prev_duration; 
	u32 SAP_delta_time; 
	u32 SAP_offset;

	u32 end_offset;

	if (!index_info->sidx) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Segment: Reference PID: %d, EPTime: "LLU", Start Offset: %d bytes\n", index_info->reference_pid, index_info->base_PTS, index_info->base_offset));
		index_info->sidx = m2ts_sidx_new(index_info->reference_pid, index_info->base_PTS, index_info->base_offset);
	}
	
	if (!index_info->pcrb) {
		index_info->pcrb = (GF_PcrInfoBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_PCRB);
	}

	/* determine the end of the current index */
	if (index_info->segment_at_rap) {
		/*split at PAT*/
		end_offset = index_info->last_pat_position;
	} else {
		/* split at PES header */
		end_offset = index_info->last_offset; 
	}

	/* close the current index */ 
	size = (u32)(end_offset - index_info->base_offset);
	duration = (u32)(index_info->last_PTS - index_info->base_PTS);
	SAP_delta_time= (u32)(index_info->first_SAP_PTS - index_info->base_PTS);
	SAP_offset = (u32)(index_info->first_SAP_offset - index_info->base_offset);
	m2ts_sidx_add_entry(index_info->sidx, 0, size, duration, index_info->first_pes_sap, index_info->SAP_type, SAP_delta_time);
	m2ts_pcrb_add_entry(index_info->pcrb, index_info->interpolated_pcr_value);

	/* adjust the previous index duration */
	if (index_info->sidx->nb_refs > 0 && (index_info->base_PTS < index_info->prev_last_PTS) ) {
		prev_duration = (u32)(index_info->base_PTS-index_info->prev_base_PTS);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("           time-range adj.: %.03f-%.03f / %.03f sec.\n", 		
			(index_info->prev_base_PTS - index_info->first_PTS)/90000.0, 
			(index_info->base_PTS - index_info->first_PTS)/90000.0, prev_duration/90000.0);
		m2ts_sidx_update_prev_entry_duration(index_info->sidx, prev_duration));
	}

	/* Printing result */
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Subsegment:"));
	//time-range:position-range: 
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (" %.03f-%0.3f / %.03f sec., %d-%d / %d bytes, ",
		(index_info->base_PTS - index_info->first_PTS)/90000.0, 
		(index_info->last_PTS - index_info->first_PTS)/90000.0, duration/90000.0,
		index_info->base_offset, end_offset, size));
	if (index_info->SAP_type) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("RAP @ %.03f sec. / %d bytes", (index_info->first_SAP_PTS - index_info->first_PTS)/90000.0, 
			SAP_offset));
	}
	if (index_info->first_pat_position_valid) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", PAT @ %d bytes", (u32)(index_info->first_pat_position - index_info->base_offset)));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", No PAT"));
	}
	if (index_info->first_cat_position_valid) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", CAT @ %d bytes", (u32)(index_info->first_cat_position - index_info->base_offset)));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", No CAT"));
	}
	if (index_info->first_pmt_position_valid) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", PMT @ %d bytes", (u32)(index_info->first_pmt_position - index_info->base_offset)));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", No PMT"));
	}
	if (index_info->first_pcr_position_valid) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", PCR @ %d bytes", (u32)(index_info->first_pcr_position - index_info->base_offset)));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", No PCR"));
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("\n"));

	/* save the current values for later adjustments */
	index_info->prev_last_SAP_PTS = index_info->last_SAP_PTS;
	index_info->prev_last_SAP_offset = index_info->last_SAP_offset;
	index_info->prev_last_PTS = index_info->last_PTS;
	index_info->prev_last_offset = index_info->last_offset;
	index_info->prev_base_PTS = index_info->base_PTS;
	index_info->base_PTS = index_info->last_PTS;
	index_info->prev_base_offset = index_info->base_offset;
	index_info->prev_last_pat_position = index_info->last_pat_position;
	index_info->prev_last_cat_position = index_info->last_cat_position;
	index_info->prev_last_pmt_position = index_info->last_pmt_position;
	index_info->prev_last_pcr_position = index_info->last_pcr_position;
	
	/* update the values for the new index*/
	index_info->base_offset = end_offset;
	index_info->SAP_type = 0;
	index_info->first_SAP_PTS = 0;
	index_info->first_SAP_offset = 0;
	index_info->first_pes_sap = 0;
	index_info->nb_pes_in_segment = 0;
	index_info->last_DTS = 0;

	if (index_info->last_pat_position >= index_info->base_offset) {
		index_info->first_pat_position_valid = 1;
		index_info->first_pat_position = index_info->last_pat_position;
	} else {
		index_info->first_pat_position_valid = 0;
		index_info->first_pat_position = 0;
	}
	if (index_info->last_cat_position >= index_info->base_offset) {
		index_info->first_cat_position_valid = 1;
		index_info->first_cat_position = index_info->last_cat_position;
	} else {
		index_info->first_cat_position_valid = 0;
		index_info->first_cat_position = 0;
	}
	if (index_info->last_pmt_position >= index_info->base_offset) {
		index_info->first_pmt_position_valid = 1;
		index_info->first_pmt_position = index_info->last_pmt_position;
	} else {
		index_info->first_pmt_position_valid = 0;
		index_info->first_pmt_position = 0;
	}
	if (index_info->last_pcr_position >= index_info->base_offset) {
		index_info->first_pcr_position_valid = 1;
		index_info->first_pcr_position = index_info->last_pcr_position;
	} else {
		index_info->first_pcr_position_valid = 0;
		index_info->first_pcr_position = 0;
	}
}

static void m2ts_check_indexing(GF_M2TS_IndexingInfo *index_info)
{
	u32 delta_time = (u32)(index_info->last_PTS - index_info->base_PTS);
	u32 segment_duration = (u32)(index_info->segment_duration*90000);
	/* we need to create an SIDX entry when the duration of the previous entry is too big */
	if (delta_time >= segment_duration) {
		m2ts_sidx_flush_entry(index_info);
	} 
}

static void on_m2ts_dump_event(GF_M2TS_Demuxer *ts, u32 evt_type, void *par) 
{
	u32 i, count;
	GF_M2TS_Program *prog;
	GF_M2TS_PES_PCK *pck;
	GF_M2TS_Dump *dumper = (GF_M2TS_Dump *)ts->user;
	GF_M2TS_IndexingInfo *index_info = &dumper->index_info;

	switch (evt_type) {
	case GF_M2TS_EVT_PAT_FOUND:
		if (index_info->start_indexing) {
			if (!index_info->first_pat_position_valid) {
				index_info->first_pat_position_valid = 1;
				index_info->first_pat_position = (ts->pck_number-1)*188;
			}
			index_info->last_pat_position = (ts->pck_number-1)*188;
		}
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, 0);
		}
		break;
	case GF_M2TS_EVT_PAT_UPDATE:
		if (index_info->start_indexing) {
			if (!index_info->first_pat_position_valid) {
				index_info->first_pat_position_valid = 1;
				index_info->first_pat_position = (ts->pck_number-1)*188;
			}
			index_info->last_pat_position = (ts->pck_number-1)*188;
		}
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, 0);
		}
		break;
	case GF_M2TS_EVT_PAT_REPEAT:
		/* WARNING: We detect the pat on a repetition, probably to ensure that we also have seen all the PMT 
		   To be checked */
		dumper->has_seen_pat = 1;
		if (index_info->start_indexing) {
			if (!index_info->first_pat_position_valid) {
				index_info->first_pat_position_valid = 1;
				index_info->first_pat_position = (ts->pck_number-1)*188;
			}
			index_info->last_pat_position = (ts->pck_number-1)*188;
		}
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, 0);
		}
//		fprintf(stdout, "Repeated PAT found - %d programs\n", gf_list_count(ts->programs) );
		break;
	case GF_M2TS_EVT_CAT_FOUND:
		if (index_info->start_indexing) {
			if (!index_info->first_cat_position_valid) {
				index_info->first_cat_position_valid = 1;
				index_info->first_cat_position = (ts->pck_number-1)*188;
			}
			index_info->last_cat_position = (ts->pck_number-1)*188;
		}
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, 0);
		}
		break;
	case GF_M2TS_EVT_CAT_UPDATE:
		if (index_info->start_indexing) {
			if (!index_info->first_cat_position_valid) {
				index_info->first_cat_position_valid = 1;
				index_info->first_cat_position = (ts->pck_number-1)*188;
			}
			index_info->last_cat_position = (ts->pck_number-1)*188;
		}
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, 0);
		}
		break;
	case GF_M2TS_EVT_CAT_REPEAT:
		if (index_info->start_indexing) {
			if (!index_info->first_cat_position_valid) {
				index_info->first_cat_position_valid = 1;
				index_info->first_cat_position = (ts->pck_number-1)*188;
			}
			index_info->last_cat_position = (ts->pck_number-1)*188;
		}
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, 0);
		}
		break;
	case GF_M2TS_EVT_PMT_FOUND:
		prog = (GF_M2TS_Program*)par;
		if (gf_list_count(ts->programs)>1 && prog->number!=dumper->prog_number)
			break;
		if (index_info->start_indexing) {
			if (!index_info->first_pmt_position_valid) {
				index_info->first_pmt_position_valid = 1;
				index_info->first_pmt_position = (ts->pck_number-1)*188;
			}
			index_info->last_pmt_position = (ts->pck_number-1)*188;
		}
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
			if (es->pid == prog->pcr_pid) {
				/* we create indexing information on the stream used for carrying the PCR */
				index_info->reference_pid = prog->pcr_pid;
				index_info->reference_stream = (GF_M2TS_PES *)es;
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
		if (index_info->start_indexing) {
			if (!index_info->first_pmt_position_valid) {
				index_info->first_pmt_position_valid = 1;
				index_info->first_pmt_position = (ts->pck_number-1)*188;
			}
			index_info->last_pmt_position = (ts->pck_number-1)*188;
		}
		if (dumper->timestamps_info_file) {
			fprintf(dumper->timestamps_info_file, "%u\t%d\n", ts->pck_number, prog->pmt_pid);
		}
		break;
	case GF_M2TS_EVT_PMT_REPEAT:
		prog = (GF_M2TS_Program*)par;
		if (gf_list_count(ts->programs)>1 && prog->number!=dumper->prog_number)
			break;
		if (index_info->start_indexing) {
			if (!index_info->first_pmt_position_valid) {
				index_info->first_pmt_position_valid = 1;
				index_info->first_pmt_position = (ts->pck_number-1)*188;
			}
			index_info->last_pmt_position = (ts->pck_number-1)*188;
		}
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
			if (pes->last_pcr_value && pes->before_last_pcr_value && pes->last_pcr_value > pes->before_last_pcr_value) {
				u32 delta_pcr_pck_num = pes->last_pcr_value_pck_number - pes->before_last_pcr_value_pck_number;
				u32 delta_pts_pcr_pck_num = pes->pes_start_packet_number - pes->last_pcr_value_pck_number;
				u64 delta_pcr_value = pes->last_pcr_value - pes->before_last_pcr_value; 
				/* we can compute the interpolated pcr value for the packet containing the PES header */
				interpolated_pcr_value = pes->last_pcr_value + (u64)((delta_pcr_value*delta_pts_pcr_pck_num*1.0)/delta_pcr_pck_num);
				index_info->interpolated_pcr_value = interpolated_pcr_value;
				index_info->last_pcr_value = pes->last_pcr_value;
			}
			
			if (dumper->timestamps_info_file) {
				fprintf(dumper->timestamps_info_file, "%u\t%d\t", pck->stream->pes_start_packet_number, pck->stream->pid);
				if (interpolated_pcr_value) fprintf(dumper->timestamps_info_file, "%f", interpolated_pcr_value/(300.0 * 90000));
				fprintf(dumper->timestamps_info_file, "\t");
				if (pck->DTS) fprintf(dumper->timestamps_info_file, "%f", (pck->DTS / 90000.0));
				fprintf(dumper->timestamps_info_file, "\t%f\t%d\t%d\n", pck->PTS / 90000.0, (pck->flags & GF_M2TS_PES_PCK_RAP ? 1 : 0), (pck->flags & GF_M2TS_PES_PCK_DISCONTINUITY ? 1 : 0));
			}
		}
		if (index_info->start_indexing) {
			/* we process packets only for the given PID */
			if (pck->stream->pid != index_info->reference_pid) {
				break;
			} else {
				if (index_info->last_DTS != pck->DTS) {
					index_info->last_DTS = pck->DTS;
					index_info->nb_pes_in_segment++;
				}

				/* we store the fact that there is at least a RAP for the index
				and we store the PTS of the first encountered RAP in the index*/
				if (pck->flags & GF_M2TS_PES_PCK_RAP) {
					index_info->SAP_type = 1;
					if (!index_info->first_SAP_PTS || (index_info->first_SAP_PTS > pck->PTS)) {
						index_info->first_SAP_PTS = pck->PTS;
						index_info->first_SAP_offset = (pck->stream->pes_start_packet_number-1)*188;
					}
					index_info->last_SAP_PTS = pck->PTS;
					index_info->last_SAP_offset = (pck->stream->pes_start_packet_number-1)*188;

					if (index_info->nb_pes_in_segment==1) {
						index_info->first_pes_sap = 1;
					}
				}
				/* we need to know the earliest PTS value (RAP or not) in the index*/
				if (!index_info->base_PTS || (index_info->base_PTS > pck->PTS)) {
					index_info->base_PTS = pck->PTS;
				}
				/* we need to know the earliest PTS value for the whole file (segment) */
				if (!index_info->first_PTS || (index_info->first_PTS > pck->PTS)) {
					index_info->first_PTS = pck->PTS;
				}
				if (pck->PTS > index_info->last_PTS) {
					/* we use the last PTS for first approximation of the duration */
					index_info->last_PTS = pck->PTS;
					index_info->last_offset = (index_info->reference_stream->pes_start_packet_number-1)*188;
				}

				m2ts_check_indexing(index_info);
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
		if (index_info->start_indexing) {
			if (!index_info->first_pcr_position_valid) {
				index_info->first_pcr_position_valid = 1;
				index_info->first_pcr_position = (ts->pck_number-1)*188;
			}
			index_info->last_pcr_position = (ts->pck_number-1)*188;
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

static void mpd_duration(Double duration, char *duration_string)
{
	u32 h, m;
	Double s;

	h = (u32) (duration/3600);
	m = (u32) (duration-h*60)/60;
	s = (duration - h*3600 - m*60);
	if (h) sprintf(duration_string, "PT%dH%dM%.2fS", h, m, s);	
	else if (m) sprintf(duration_string, "PT%dM%.2fS", m, s);	
	else if (s) sprintf(duration_string, "PT%.2fS", s);	
	else sprintf(duration_string, "PT0S");	
}

static void mpd_start(GF_M2TS_IndexingInfo *index_info, Bool on_demand, const char *media_file_name, 
			   Double file_duration, Bool split_seg_at_rap, u64 file_size, Double bufferTime)
{
	char duration_string[100];
	char buffer_string[100];
	u32 bandwidth, i;
#ifndef GPAC_DISABLE_MEDIA_IMPORT
	GF_MediaImporter import;
#endif
	char szCodecs[1000];
	u32 width, height, sample_rate, nb_channels, langCode;
	GF_Err e;
	FILE *mpd = index_info->mpd_file;

	/*get codecs*/	
#ifndef GPAC_DISABLE_MEDIA_IMPORT
	memset(&import, 0, sizeof(GF_MediaImporter));
	import.trackID = 0;
	import.flags = GF_IMPORT_PROBE_ONLY;
	import.in_name = (char *)media_file_name;
	e = gf_media_import(&import);
#else
	fprintf(stdout, "Warning: generating MPD without media import support\n");
#endif
	strcpy(szCodecs, "");
	width = height = sample_rate = nb_channels = langCode = 0;


	bandwidth = (u32) (file_size * 8 / file_duration);	
	mpd_duration(file_duration, duration_string);
	mpd_duration(bufferTime, buffer_string);

	if (!index_info->represantation_idx) {
		fprintf(mpd, "<MPD type=\"%s\" xmlns=\"urn:mpeg:DASH:schema:MPD:2011\" profiles=\"urn:mpeg:dash:profile:mp2t-main:2011\" minBufferTime=\"%s\">\n", (on_demand ? "static": "dynamic"), buffer_string);
		fprintf(mpd, " <ProgramInformation moreInformationURL=\"http://gpac.sourceforge.net\">\n");
		fprintf(mpd, "  <Title>MPD for file %s generated with GPAC %s</Title>\n", media_file_name, GPAC_FULL_VERSION);
		fprintf(mpd, " </ProgramInformation>\n");
		fprintf(mpd, " <Period start=\"PT0S\" duration=\"%s\" minBufferTime=\"%s\">\n", duration_string, buffer_string);	
		fprintf(mpd, "  <AdaptationSet segmentAlignment=\"true\" bitstreamSwitching=\"true\" subsegmentAlignment=\"true\">\n");
	}

	if (!e) {
#ifndef GPAC_DISABLE_MEDIA_IMPORT
		for (i=0; i<import.nb_tracks;i++) {
			if (strlen(import.tk_info[i].szCodecProfile)) {
				if (strlen(szCodecs)) strcat(szCodecs, ",");
				strcat(szCodecs, import.tk_info[i].szCodecProfile);
			}

			if (import.tk_info[i].type==GF_ISOM_MEDIA_VISUAL) {
				if (!width) width = import.tk_info[i].video_info.width;
				if (!height) height = import.tk_info[i].video_info.height;
				fprintf(mpd, "   <ContentComponent id=\"%d\" contentType=\"video\"/>\n", import.tk_info[i].track_num);
			}
			else if (import.tk_info[i].type==GF_ISOM_MEDIA_AUDIO) {
				if (!sample_rate) sample_rate = import.tk_info[i].audio_info.sample_rate;
				if (!nb_channels) nb_channels = import.tk_info[i].audio_info.nb_channels;
				fprintf(mpd, "   <ContentComponent id=\"%d\" contentType=\"audio\"/>\n", import.tk_info[i].track_num);
			}
			if (!langCode && import.tk_info[i].lang) langCode = import.tk_info[i].lang;
		}
#endif
	}

	fprintf(mpd, "   <Representation id=\"%d\" mimeType=\"video/mp2t\" codecs=\"%s\"", index_info->represantation_idx+1, szCodecs);
	if (width && height) fprintf(mpd, " width=\"%d\" height=\"%d\"", width, height);
	if (sample_rate) fprintf(mpd, " audioSamplingRate=\"%d\"", sample_rate);
	if (langCode) fprintf(mpd, " lang=\"%s\"", gf_4cc_to_str(langCode) );
	fprintf(mpd, " startWithRAP=\"%s\"", split_seg_at_rap ? "1" : "false");
	fprintf(mpd, " bandwidth=\"%d\"", bandwidth);
	fprintf(mpd, ">\n");
	if (nb_channels) 
		fprintf(mpd, "    <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"%d\"/>", nb_channels);

}

void mpd_end(FILE *mpd, Bool is_last_rep)
{
    fprintf(mpd, "   </Representation>\n");
	if (is_last_rep) {
		fprintf(mpd, "  </AdaptationSet>\n");
		fprintf(mpd, " </Period>\n");
		fprintf(mpd, "</MPD>");
	}
}

static void write_mpd_segment_info(GF_M2TS_IndexingInfo *index_info, char *media_file_name)
{
	char *sep, SegName[GF_MAX_PATH];
	u32 i;
	u64 start;

	if (!index_info->seg_name) {
		fprintf(index_info->mpd_file, "    <BaseURL>%s</BaseURL>\n", media_file_name);
	}

	if (index_info->single_segment) {
		fprintf(index_info->mpd_file, "    <SegmentBase>\n");
		fprintf(index_info->mpd_file, "     <RepresentationIndex>%s</RepresentationIndex>\n", index_info->index_file_name);
		fprintf(index_info->mpd_file, "    </SegmentBase>\n");
		return;
	} 

	if (index_info->seg_name) {
		char *mfile = strrchr(media_file_name, '/');
		if (!mfile) mfile = strrchr(media_file_name, '\\');
		if (mfile) mfile += 1;
		else mfile = media_file_name;

		if (strstr(index_info->seg_name, "%s")) {
			sprintf(SegName, index_info->seg_name, mfile);
		} else {
			strcpy(SegName, index_info->seg_name);
		}
		sep = strrchr(SegName, '.');
		if (sep) sep[0] = 0;
	}

	if (index_info->seg_name && index_info->use_url_template) {
		fprintf(index_info->mpd_file, "    <SegmentTemplate timescale=\"1000\" duration=\"%d\" startNumber=\"0\" media=\"%s$Number$.%s\" >\n", (u32) (1000*index_info->segment_duration), SegName, index_info->seg_ext); 
	} else {
		fprintf(index_info->mpd_file, "    <SegmentList timescale=\"1000\" duration=\"%d\">\n", (u32) (1000*index_info->segment_duration)); 
	}
	/* add startIndex for live scenarios */
	
	if (index_info->init_seg_name) {
		fprintf(index_info->mpd_file, "     <Initialization sourceURL=\"%s\"/>\n", index_info->init_seg_name);
	}


	fprintf(index_info->mpd_file, "     <RepresentationIndex>%s</RepresentationIndex>\n", index_info->index_file_name); 

	if (!index_info->seg_name) {
		start=index_info->sidx->first_offset;
		for (i=0; i<index_info->sidx->nb_refs; i++) {
			GF_SIDXReference *ref = &index_info->sidx->refs[i];
			fprintf(index_info->mpd_file, "     <SegmentURL mediaRange=\""LLD"-"LLD"\"/>\n", start, start+ref->reference_size-1);
			start += ref->reference_size;
		}
	} else {
		FILE *src, *dst;
		u64 pos, end;
		src= gf_f64_open(media_file_name, "rb");
		start=index_info->sidx->first_offset;
		for (i=0; i<index_info->sidx->nb_refs; i++) {
			char szFile[GF_MAX_PATH], buf[4096];
			GF_SIDXReference *ref = &index_info->sidx->refs[i];

			strcpy(szFile, SegName);
			sprintf(buf, "%d", index_info->segment_index);
			strcat(szFile, buf);
			strcat(szFile, ".ts");

			if (index_info->use_url_template!=2) {
				dst = gf_f64_open(szFile, "wb");

				gf_f64_seek(src, start, SEEK_SET);
				pos = start;
				end = start+ref->reference_size;
				while (pos<end) {
					u32 res;
					u32 to_read = 4096;
					if (pos+4096 >= end) {
						to_read = (u32) (end-pos);
					}
					res = fread(buf, 1, to_read, src);
					if (res==to_read) {
						res = fwrite(buf, 1, to_read, dst);
					}
					if (res!=to_read) {
						fprintf(stderr, "IO error while Extracting segment %03d / %03d\r", i+1, index_info->sidx->nb_refs);
						break;
					}
					pos += res;
				}
				fclose(dst);
			}
			start += ref->reference_size;
			index_info->segment_index++;

			if (!index_info->use_url_template) 
				fprintf(index_info->mpd_file, "     <SegmentURL media=\"%s\"/>\n", szFile);

			fprintf(stdout, "Extracting segment %03d / %03d\r", i+1, index_info->sidx->nb_refs);
		}
		fclose(src);
	}		
//		fprintf(index_info->mpd_file, "     <SegmentURL media=\"%s\" index=\"%s\"/>\n", media_file_name, index_info->index_file_name);
	if (index_info->seg_name && index_info->use_url_template) {
		fprintf(index_info->mpd_file, "    </SegmentTemplate>\n");
	} else {
		fprintf(index_info->mpd_file, "    </SegmentList>\n");
	}
}

void dump_mpeg2_ts(char *mpeg2ts_file, char *out_name, Bool prog_num, 
				   Double dash_duration, Bool seg_at_rap, u32 subseg_per_seg,
				   char *seg_name, char *seg_ext, Bool use_url_template, Bool single_segment, u32 representation_idx, Bool is_last_rep)
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
	memset(&dumper, 0, sizeof(GF_M2TS_Dump));
	ts->user = &dumper;
	dumper.prog_number = prog_num;


	if (dash_duration) {
		char *c, *f, *basename;
		dumper.index_info.segment_duration = dash_duration;
		dumper.index_info.segment_at_rap = seg_at_rap;
		dumper.index_info.subsegs_per_segment = subseg_per_seg;
		dumper.index_info.use_url_template = use_url_template;
		dumper.index_info.init_seg_name = NULL;
		dumper.index_info.single_segment = single_segment;
		dumper.index_info.seg_name = seg_name;
		dumper.index_info.seg_ext = seg_ext ? seg_ext : "ts";
	
		basename = out_name ? out_name : mpeg2ts_file;
		c = strrchr(basename, '.');
		if (c) *c = 0;
		f = NULL;
		if (!out_name) {
			f = strrchr(basename, '/');
			if (!f) f = strrchr(basename, '\\');
		}
		sprintf(dumper.index_info.index_file_name, "%s_index.%s", f ? f+1 : basename, (seg_ext?seg_ext:"didx"));
		if (c) *c = '.';
		
		dumper.index_info.index_file = NULL;
		dumper.index_info.index_bs = NULL;
		if (dumper.index_info.use_url_template!=2) {
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
			dumper.index_info.index_file = gf_f64_open(dumper.index_info.index_file_name, "wb");
			dumper.index_info.index_bs = gf_bs_from_file(dumper.index_info.index_file, GF_BITSTREAM_WRITE);
			{
				GF_SegmentTypeBox *styp = (GF_SegmentTypeBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_STYP);
				styp->majorBrand = GF_4CC('r','i','s','x');
				styp->minorVersion = 0;
				styp->altBrand = (u32*)gf_malloc(sizeof(u32));
				styp->altBrand[0] = styp->majorBrand;
				styp->altCount = 1;
				gf_isom_box_size((GF_Box *)styp);
				gf_isom_box_write((GF_Box *)styp, dumper.index_info.index_bs);
				gf_isom_box_del((GF_Box *)styp);
			}
#endif
		}
	}
	/*PES dumping*/
	else if (out_name) {
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
		size = fread(data, 1, 188, src);
		if (size<188) break;

		gf_m2ts_process_data(ts, data, size);
		if (dumper.has_seen_pat) break;
	}

	if (prog_num && !dash_duration) {
		sprintf(dumper.timestamps_info_name, "%s_prog_%d_timestamps.txt", mpeg2ts_file, prog_num/*, mpeg2ts_file*/);
		dumper.timestamps_info_file = gf_f64_open(dumper.timestamps_info_name, "wt");
		if (!dumper.timestamps_info_file) {
			fprintf(stderr, "Cannot open file %s\n", dumper.timestamps_info_name);
			return;
		}
		fprintf(dumper.timestamps_info_file, "PCK#\tPID\tPCR\tDTS\tPTS\tRAP\tDiscontinuity\n");
	}

	gf_m2ts_reset_parsers(ts);
	gf_f64_seek(src, 0, SEEK_SET);
	fdone = 0;
	if (dumper.index_info.segment_duration) {
		char *sep;
		dumper.index_info.start_indexing = 1;
		fprintf(stderr, "Starting indexing ...\n");
		if (out_name) {
			strcpy(dumper.index_info.mpd_file_name, out_name);
		} else {
			sep = strrchr(mpeg2ts_file, '/');
			if (!sep) sep = strrchr(mpeg2ts_file, '\\');
			strcpy(dumper.index_info.mpd_file_name, sep ? sep+1 : mpeg2ts_file);
			sep = strrchr(dumper.index_info.mpd_file_name, '.');
			if (sep) sep[0] = 0;
			strcat(dumper.index_info.mpd_file_name, ".mpd");
		}
		if (!representation_idx) {
			dumper.index_info.mpd_file = gf_f64_open(dumper.index_info.mpd_file_name, "wt");
		} else {
			dumper.index_info.mpd_file = gf_f64_open(dumper.index_info.mpd_file_name, "a+t");
		}
		if (!dumper.index_info.mpd_file) {
			fprintf(stderr, "Cannot open file %s\n", dumper.index_info.mpd_file_name);
			return;
		}
		dumper.index_info.represantation_idx = representation_idx;
	}
	while (!feof(src)) {
		size = fread(data, 1, 188, src);
		if (size<188) break;

		gf_m2ts_process_data(ts, data, size);

		fdone += size;
		gf_set_progress("MPEG-2 TS Parsing", fdone, fsize);
	}

	if (dumper.index_info.segment_duration) {
		u64 file_size;
		/* flush SIDX entry for the last packets */
		m2ts_sidx_flush_entry(&dumper.index_info);
		gf_f64_seek(src, 0, SEEK_END);
		file_size = gf_f64_tell(src);
		m2ts_sidx_finalize_size(&dumper.index_info, file_size);
		fprintf(stderr, "Indexing done (1 sidx, %d entries).\n", dumper.index_info.sidx->nb_refs);

		mpd_start(&dumper.index_info, 1, mpeg2ts_file, (dumper.index_info.last_PTS-dumper.index_info.first_PTS)/90000.0, dumper.index_info.segment_at_rap, file_size, dumper.index_info.segment_duration/4);
		write_mpd_segment_info(&dumper.index_info, mpeg2ts_file);
		mpd_end(dumper.index_info.mpd_file, is_last_rep);
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
	if (dumper.index_info.sidx) {
		gf_isom_box_size((GF_Box *)dumper.index_info.sidx);
		if (dumper.index_info.index_bs) gf_isom_box_write((GF_Box *)dumper.index_info.sidx, dumper.index_info.index_bs);
		gf_isom_box_del((GF_Box *)dumper.index_info.sidx);
		}
	
		// ToDo: Should be configurable by some switch
		if (dumper.index_info.pcrb) {
		gf_isom_box_size((GF_Box *)dumper.index_info.pcrb);
		if (dumper.index_info.index_bs) gf_isom_box_write((GF_Box *)dumper.index_info.pcrb, dumper.index_info.index_bs);
		gf_isom_box_del((GF_Box *)dumper.index_info.pcrb);

	}
	if (dumper.index_info.mpd_file) fclose(dumper.index_info.mpd_file);
	if (dumper.index_info.index_file) fclose(dumper.index_info.index_file);
	if (dumper.index_info.index_bs) gf_bs_del(dumper.index_info.index_bs);
}

#endif /*GPAC_DISABLE_MPEG2TS*/


