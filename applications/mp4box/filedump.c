/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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


#include <gpac/scene_manager.h>
#include <gpac/nodes_x3d.h>
#include <gpac/internal/bifs_dev.h>
#include <gpac/constants.h>
#include <gpac/avparse.h>

extern u32 swf_flags;
extern Float swf_flatten_angle;
extern u32 get_file_type_by_ext(char *inName);

void dump_file_text(char *file, char *inName, u32 dump_mode, Bool do_log)
{
	GF_Err e;
	GF_SceneManager *ctx;
	GF_SceneGraph *sg;
	GF_SceneLoader load;
	e = GF_OK;

	sg = gf_sg_new();
	ctx = gf_sm_new(sg);
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = file;
	load.ctx = ctx;
	load.OnProgress = gf_cbk_on_progress;
	load.cbk = "Parsing";
	load.swf_import_flags = swf_flags;
	load.swf_flatten_limit = swf_flatten_angle;

	if (get_file_type_by_ext(file) == 1) {
		load.isom = gf_isom_open(file, GF_ISOM_OPEN_READ, NULL);
		if (!load.isom) {
			fprintf(stdout, "Cannot open file: %s\n", gf_error_to_string(gf_isom_last_error(NULL)));
			gf_sm_del(ctx);
			gf_sg_del(sg);
			return;
		}
	}
	if (do_log) load.flags = GF_SM_LOAD_DUMP_BINARY;
	e = gf_sm_load_init(&load);
	if (!e) e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);
	if (!e) {
		fprintf(stdout, "Scene loaded - dumping %d systems streams\n", gf_list_count(ctx->streams));
		e = gf_sm_dump(ctx, inName, dump_mode);
	}

	gf_sm_del(ctx);
	gf_sg_del(sg);
	if (e) fprintf(stdout, "Error loading scene: %s\n", gf_error_to_string(e));
	if (load.isom) gf_isom_delete(load.isom);
}


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
		}
		fprintf(dump, "<Instanciation NbObjects=\"%d\" NbUse=\"%d\" NbDestroy=\"%d\"/>\n", ptr->nb_created, ptr->nb_used, ptr->nb_del);
		count += ptr->nb_created + ptr->nb_used;
		deleted += ptr->nb_del;
		created += ptr->nb_created;
		fprintf(dump, "</NodeStat>\n");
	}
	if (i) {
		fprintf(dump, "<CumulatedStat TotalNumberOfNodes=\"%d\" ReallyAllocatedNodes=\"%d\" DeletedNodes=\"%d\"/>\n", count, created, deleted);
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
	fprintf(dump, "<FieldStatistic FieldType=\"MFVec2f\">\n");
	fprintf(dump, "<ParsingInfo NumParsed=\"%d\" NumRemoved=\"%d\"/>", stats->count_2d, stats->rem_2d);
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

	load.cbk = "Parsing";
	load.OnProgress = gf_cbk_on_progress;
	e = gf_sm_load_init(&load);
	if (!e) e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);
	if (e) goto exit;

	if (inName) {
		strcpy(szBuf, inName);
		strcat(szBuf, "_stat.xml");
		dump = fopen(szBuf, "wt");
		close = 1;
	} else {
		dump = stdout;
		close = 0;
	}

	fprintf(stdout, "Analysing Scene\n");

	fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
	fprintf(dump, "<!-- Scene Graph Statistics Generated by MP4Box - GPAC " GPAC_VERSION" -->\n");
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
			fprintf(dump, "<AUStatistics StreamID=\"%d\" AUTime=\"%d\">\n", au->owner->ESID, (u32) au->timing);
		} else {
			fprintf(dump, "<GraphStatistics StreamID=\"%d\" AUTime=\"%d\">\n", au->owner->ESID, (u32) au->timing);
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

		gf_cbk_on_progress("Analysing AU", i+1, count);
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

void PrintFixed(Fixed val, Bool add_space)
{
	if (add_space) fprintf(stdout, " ");
	if (val==FIX_MIN) fprintf(stdout, "-I");
	else if (val==FIX_MAX) fprintf(stdout, "+I");
	else fprintf(stdout, "%g", FIX2FLT(val));
}

void PrintNodeSFField(u32 type, void *far_ptr)
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
void PrintNode(const char *name, Bool x3d_node)
{
	const char *nname, *std_name;
	GF_Node *node;
	GF_SceneGraph *sg;
	u32 tag, nbF, i;
	GF_FieldInfo f;
	u8 qt, at;
	Fixed bmin, bmax;
	u32 nbBits;

	if (x3d_node) {
		tag = gf_node_x3d_type_by_class_name(name);
		std_name = "X3D";
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

	fprintf(stdout, "Node Syntax:\n%s {\n", nname);

	for (i=0; i<nbF; i++) {
		gf_node_get_field(node, i, &f);
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
		fprintf(stdout, "\n");
	}
	fprintf(stdout, "}\n\n");

	gf_node_unregister(node, NULL);
	gf_sg_del(sg);
}

void PrintBuiltInNodes(Bool x3d_node)
{
	GF_Node *node;
	GF_SceneGraph *sg;
	u32 i, nb_in, nb_not_in, start_tag, end_tag;

	if (x3d_node) {
		start_tag = GF_NODE_RANGE_FIRST_X3D;
		end_tag = TAG_LastImplementedX3D;
	} else {
		start_tag = GF_NODE_RANGE_FIRST_MPEG4;
		end_tag = TAG_LastImplementedMPEG4;
	}
	nb_in = nb_not_in = 0;
	sg = gf_sg_new();
	if (x3d_node) {
		fprintf(stdout, "Available X3D nodes in this build (dumping):\n");
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
			nb_not_in++;
		}
	}
	gf_sg_del(sg);
	fprintf(stdout, "\n%d nodes supported - %d nodes not supported\n", nb_in, nb_not_in);
}

void dump_file_mp4(GF_ISOFile *file, char *inName)
{
	FILE *dump;
	char szBuf[1024];

	if (inName) {
		strcpy(szBuf, inName);
		strcat(szBuf, "_info.xml");
		dump = fopen(szBuf, "wt");
		gf_isom_dump(file, dump);
		fclose(dump);
	} else {
		gf_isom_dump(file, stdout);
	}
}

void dump_file_rtp(GF_ISOFile *file, char *inName)
{
	u32 i, j;
	FILE *dump;
	char szBuf[1024];

	if (inName) {
		strcpy(szBuf, inName);
		strcat(szBuf, "_rtp.xml");
		dump = fopen(szBuf, "wt");
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

#define TS_DIV 1000
#define TS_MUL 24

void dump_file_ts(GF_ISOFile *file, char *inName)
{
	u32 i, j, k, count;
	Bool has_error;
	FILE *dump;
	char szBuf[1024];

	if (inName) {
		strcpy(szBuf, inName);
		strcat(szBuf, "_ts.txt");
		dump = fopen(szBuf, "wt");
	} else {
		dump = stdout;
	}

	has_error = 0;
	for (i=0; i<gf_isom_get_track_count(file); i++) {	
		Bool has_cts_offset = gf_isom_has_time_offset(file, i+1);

		fprintf(dump, "#dumping track ID %d timing\n", gf_isom_get_track_id(file, i+1));
		count = gf_isom_get_sample_count(file, i+1);
		for (j=0; j<count; j++) {
			u64 dts, cts;
			GF_ISOSample *samp = gf_isom_get_sample_info(file, i+1, j+1, NULL, NULL);
			dts = samp->DTS;
			cts = dts + (s32) samp->CTS_Offset;
			gf_isom_sample_del(&samp);
//			dts /= TS_DIV; cts /= TS_DIV;
//			dts *= TS_MUL; cts *= TS_MUL;

			fprintf(dump, "Sample %d - DTS %d - CTS %d", j+1, (u32)dts, (u32) cts);
			if (cts<dts) { fprintf(dump, " #NEGATIVE CTS OFFSET!!!"); has_error = 1;}
		
			if (has_cts_offset) {
				for (k=0; k<count; k++) {
					u64 adts, acts;
					if (k==j) continue;
					samp = gf_isom_get_sample_info(file, i+1, k+1, NULL, NULL);
					adts = samp->DTS;
					acts = adts + (s32) samp->CTS_Offset;
//					adts /= TS_DIV; acts /= TS_DIV;
//					adts *= TS_MUL; acts *= TS_MUL;

					if (adts==dts) { fprintf(dump, " #SAME DTS USED!!!"); has_error = 1; }
					if (acts==cts) { fprintf(dump, " #SAME CTS USED!!! "); has_error = 1; }

					gf_isom_sample_del(&samp);
				}
			}

			fprintf(dump, "\n");
			gf_cbk_on_progress("Analysing Track Timing", j+1, count);
		}
		fprintf(dump, "\n\n");
		gf_cbk_on_progress("Analysing Track Timing", count, count);
	}
	if (inName) fclose(dump);
	if (has_error) fprintf(stdout, "\tFile has CTTS table errors\n");
}

void dump_file_ismacryp(GF_ISOFile *file, char *inName)
{
	u32 i, j;
	FILE *dump;
	char szBuf[1024];

	if (inName) {
		strcpy(szBuf, inName);
		strcat(szBuf, "_ismacryp.xml");
		dump = fopen(szBuf, "wt");
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


void dump_timed_text_track(GF_ISOFile *file, u32 trackID, char *inName, Bool is_convert, Bool to_srt)
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

	if (gf_isom_get_media_type(file, track) != GF_ISOM_MEDIA_TEXT) {
		fprintf(stdout, "Track ID %d is not a 3GPP text track\n", trackID);
		return;
	}

	if (inName) {
		if (is_convert)	
			sprintf(szBuf, "%s.%s", inName, to_srt ? "srt" : "ttxt");
		else
			sprintf(szBuf, "%s_%d_text.%s", inName, trackID, to_srt ? "srt" : "ttxt");
		dump = fopen(szBuf, "wt");
	} else {
		dump = stdout;
	}
	e = gf_isom_text_dump(file, track, dump, to_srt, gf_cbk_on_progress, "Converting");
	if (inName) fclose(dump);

	if (e) fprintf(stdout, "Conversion failed (%s)\n", gf_error_to_string(e));
	else fprintf(stdout, "Conversion done\n");
}


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
		dump = fopen(szBuf, "wt");
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

static char *format_duration(u64 dur, u32 timescale, char *szDur)
{
	u32 h, m, s, ms;
	dur = (u64) (( ((Double) (s64) dur)/timescale)*1000);
	h = (u32) (dur / 3600000);
	m = (u32) (dur/ 60000) - h*60;
	s = (u32) (dur/1000) - h*3600 - m*60;
	ms = (u32) (dur) - h*3600000 - m*60000 - s*1000;
	sprintf(szDur, "%02d:%02d:%02d.%03d", h, m, s, ms);
	return szDur;
}


static void DumpMetaItem(GF_ISOFile *file, Bool root_meta, u32 tk_num, char *name)
{
	u32 i, count, brand;
	brand = gf_isom_get_meta_type(file, root_meta, tk_num);
	if (!brand) return;

	count = gf_isom_get_meta_item_count(file, root_meta, tk_num);
	fprintf(stdout, "%s type: \"%s\" - %d resource item(s)\n", name, gf_4cc_to_str(brand), count);
	switch (gf_isom_has_meta_xml(file, root_meta, tk_num)) {
	case 1: fprintf(stdout, "Meta has XML resource\n"); break;
	case 2: fprintf(stdout, "Meta has BinaryXML resource\n"); break;
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
	u32 trackNum, j, size, max_rate, rate, ts, mtype, msub_type, timescale, sr, nb_ch, count;
	u64 time_slice, dur;
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
	gf_isom_get_media_language(file, trackNum, sType);
	fprintf(stdout, "Media Info: Language \"%s\" - ", sType);
	mtype = gf_isom_get_media_type(file, trackNum);
	fprintf(stdout, "Type \"%s\" - ", gf_4cc_to_str(mtype));
	msub_type = gf_isom_get_mpeg4_subtype(file, trackNum, 1);
	if (!msub_type) msub_type = gf_isom_get_media_subtype(file, trackNum, 1);
	fprintf(stdout, "Sub Type \"%s\" - %d samples\n", gf_4cc_to_str(msub_type), gf_isom_get_sample_count(file, trackNum));
	
	if (full_dump) {
		const char *handler_name;
		gf_isom_get_handler_name(file, trackNum, &handler_name);
		fprintf(stdout, "Handler name: %s\n", handler_name);
	}

	gf_isom_get_audio_info(file, trackNum, 1, &sr, &nb_ch, &bps);
	
	msub_type = gf_isom_get_media_subtype(file, trackNum, 1);
	if ((msub_type==GF_ISOM_SUBTYPE_MPEG4) || (msub_type==GF_ISOM_SUBTYPE_MPEG4_CRYP) || (msub_type==GF_ISOM_SUBTYPE_AVC_H264))  {
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
			if (esd->decoderConfig->streamType==GF_STREAM_VISUAL) {
				u32 w, h;
				w = h = 0;
				if (esd->decoderConfig->objectTypeIndication==0x20) {
					GF_M4VDecSpecInfo dsi;
					gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
					if (full_dump) fprintf(stdout, "\t");
					w = dsi.width;
					h = dsi.height;
					if (w && h) {
						fprintf(stdout, "MPEG-4 Visual Size %d x %d - %s\n", dsi.width, dsi.height, gf_m4v_get_profile_name(dsi.VideoPL));
						if (dsi.par_den && dsi.par_num) {
							u32 tw, th;
							gf_isom_get_track_layout_info(file, trackNum, &tw, &th, NULL, NULL, NULL);
							fprintf(stdout, "Pixel Aspect Ratio %d:%d - Indicated track size %d x %d\n", dsi.par_num, dsi.par_den, tw, th);
						}
					}
				} else if (esd->decoderConfig->objectTypeIndication==0x21) {
					GF_AVCConfig *avccfg;
					GF_AVCConfigSlot *slc;
					s32 par_n, par_d;

					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					if (full_dump) fprintf(stdout, "\t");
					fprintf(stdout, "AVC/H264 Video - Visual Size %d x %d - ", w, h);
					avccfg = gf_isom_avc_config_get(file, trackNum, 1);
					fprintf(stdout, "Profile %s @ Level %g\n", gf_avc_get_profile_name(avccfg->AVCProfileIndication), ((Double)avccfg->AVCLevelIndication)/10.0 );
					slc = gf_list_get(avccfg->sequenceParameterSets, 0);
					gf_avc_get_sps_info(slc->data, slc->size, NULL, NULL, &par_n, &par_d);
					if ((par_n>0) && (par_d>0)) {
						u32 tw, th;
						gf_isom_get_track_layout_info(file, trackNum, &tw, &th, NULL, NULL, NULL);
						fprintf(stdout, "Pixel Aspect Ratio %d:%d - Indicated track size %d x %d\n", par_n, par_d, tw, th);
					}
					gf_odf_avc_cfg_del(avccfg);
				} 
				/*OGG media*/
				else if (esd->decoderConfig->objectTypeIndication==GPAC_OGG_MEDIA_OTI) {
					char *szName;
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					if (full_dump) fprintf(stdout, "\t");
					if (!strnicmp(&esd->decoderConfig->decoderSpecificInfo->data[1], "theora", 6)) szName = "Theora";
					else szName = "Unknown";
					fprintf(stdout, "Ogg/%s video / GPAC Mux  - Visual Size %d x %d\n", szName, w, h);
				}
				if (!w || !h) {
					gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
					if (full_dump) fprintf(stdout, "\t");
					fprintf(stdout, "Visual Size %d x %d\n", w, h);
				}
			} else if (esd->decoderConfig->streamType==GF_STREAM_AUDIO) {
				GF_M4ADecSpecInfo a_cfg;
				GF_Err e;
				u32 oti, is_mp2 = 0;
				switch (esd->decoderConfig->objectTypeIndication) {
				case 0x66:
				case 0x67:
				case 0x68:
					is_mp2 = 1;
				case 0x40:
					e = gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &a_cfg);
					if (full_dump) fprintf(stdout, "\t");
					if (e) fprintf(stdout, "Corrupted AAC Config\n");
					else {
						fprintf(stdout, "MPEG-%d Audio %s - %d Channel(s) - SampleRate %d", is_mp2 ? 2 : 4, gf_m4a_object_type_name(a_cfg.base_object_type), a_cfg.nb_chan, a_cfg.base_sr);
						if (a_cfg.has_sbr) fprintf(stdout, " - SBR SampleRate %d", a_cfg.sbr_sr);
						fprintf(stdout, "\n");
					}
					break;
				case 0x69:
				case 0x6B:
					if (msub_type == GF_ISOM_SUBTYPE_MPEG4_CRYP) {
						fprintf(stdout, "MPEG-1/2 Audio - %d Channels - SampleRate %d\n", nb_ch, sr);
					} else {
						GF_ISOSample *samp = gf_isom_get_sample(file, trackNum, 1, &oti);
						oti = GF_FOUR_CHAR_INT((u8)samp->data[0], (u8)samp->data[1], (u8)samp->data[2], (u8)samp->data[3]);
						if (full_dump) fprintf(stdout, "\t");
						fprintf(stdout, "%s Audio - %d Channel(s) - SampleRate %d - Layer %d\n",
							gf_mp3_version_name(oti),
							gf_mp3_num_channels(oti), 
							gf_mp3_sampling_rate(oti), 
							gf_mp3_layer(oti)
						);
						gf_isom_sample_del(&samp);
					}
					break;
				/*OGG media*/
				case GPAC_OGG_MEDIA_OTI:
				{
					char *szName;
					if (full_dump) fprintf(stdout, "\t");
					if (!strnicmp(&esd->decoderConfig->decoderSpecificInfo->data[1], "vorbis", 6)) szName = "Vorbis";
					else if (!strnicmp(&esd->decoderConfig->decoderSpecificInfo->data[0], "Speex", 5)) szName = "Speex";
					else if (!strnicmp(&esd->decoderConfig->decoderSpecificInfo->data[0], "Flac", 4)) szName = "Flac";
					else szName = "Unknown";
					fprintf(stdout, "Ogg/%s audio / GPAC Mux - Sample Rate %d - %d channel(s)\n", szName, sr, nb_ch);
				}
					break;
				case 0xA0: fprintf(stdout, "EVRC Audio - Sample Rate 8000 - 1 channel\n"); break;
				case 0xA1: fprintf(stdout, "SMV Audio - Sample Rate 8000 - 1 channel\n"); break;
				case 0xE1: fprintf(stdout, "QCELP Audio - Sample Rate 8000 - 1 channel\n"); break;
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
				if (esd->decoderConfig->objectTypeIndication<=6) {
					GF_BIFSConfig *b_cfg = gf_odf_get_bifs_config(esd->decoderConfig->decoderSpecificInfo, esd->decoderConfig->objectTypeIndication);
					fprintf(stdout, "BIFS Scene description - %s stream\n", b_cfg->elementaryMasks ? "Animation" : "Command"); 
					if (full_dump && !b_cfg->elementaryMasks) {
						fprintf(stdout, "\tWidth %d Height %d Pixel Metrics %s\n", b_cfg->pixelWidth, b_cfg->pixelHeight, b_cfg->pixelMetrics ? "yes" : "no"); 
					}
					gf_odf_desc_del((GF_Descriptor *)b_cfg);
				} else if (esd->decoderConfig->objectTypeIndication==0x09) {
					GF_LASERConfig l_cfg;
					gf_odf_get_laser_config(esd->decoderConfig->decoderSpecificInfo, &l_cfg);
					fprintf(stdout, "LASER Stream - %s\n", l_cfg.append ? "Scene Segment" : "Full Scene"); 
				}
			}

			/*sync is only valid if we open all tracks to take care of default MP4 sync..*/
			if (!full_dump) {
				if (!esd->OCRESID || (esd->OCRESID == esd->ESID))
					fprintf(stdout, "Self-synchronized\n");
				else
					fprintf(stdout, "Synchronized on stream %d\n", esd->OCRESID);
			} else {
				fprintf(stdout, "\tDecoding Buffer size %d - Average bitrate %d kbps - Max Bitrate %d kbps\n", esd->decoderConfig->bufferSizeDB, esd->decoderConfig->avgBitrate/1024, esd->decoderConfig->maxBitrate/1024);
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
				u8 IV_size;
				Bool use_sel_enc;
				gf_isom_get_ismacryp_info(file, trackNum, 1, NULL, &scheme_type, &version, &scheme_URI, &KMS_URI, &use_sel_enc, &IV_size, NULL);

				if (gf_isom_is_ismacryp_media(file, trackNum, 1)) {
					fprintf(stdout, "\n*Encrypted stream - ISMA scheme %s (version %d)\n", gf_4cc_to_str(scheme_type), version);
					if (scheme_URI) fprintf(stdout, "scheme location: %s\n", scheme_URI);
					if (KMS_URI) {
						if (!strnicmp(KMS_URI, "(key)", 5)) fprintf(stdout, "KMS location: key in file\n");
						else fprintf(stdout, "KMS location: %s\n", KMS_URI);
					}
					fprintf(stdout, "ISMA Config: IV length: %d - Selective Encryption: %s\n", IV_size, use_sel_enc ? "Yes" : "No");
				} else {
					fprintf(stdout, "\n*Encrypted stream - unknown scheme %s\n", gf_4cc_to_str(scheme_type));
					if (scheme_URI) fprintf(stdout, "scheme location: %s\n", scheme_URI);
				}
			}

		}
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_H263) {
		u32 w, h;
		gf_isom_get_visual_info(file, trackNum, 1, &w, &h);
		fprintf(stdout, "\t3GPP H263 stream - Resolution %d x %d\n", w, h);
	} else if ((msub_type == GF_ISOM_SUBTYPE_3GP_AMR) || (msub_type == GF_ISOM_SUBTYPE_3GP_AMR_WB)) {
		fprintf(stdout, "\t3GPP AMR%s stream - Sample Rate %d - %d channel(s) %d bits per samples\n", (msub_type == GF_ISOM_SUBTYPE_3GP_AMR_WB) ? " Wide Band" : "", sr, nb_ch, (u32) bps);
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_EVRC) {
		fprintf(stdout, "\t3GPP EVRC stream - Sample Rate %d - %d channel(s) %d bits per samples\n", sr, nb_ch, (u32) bps);
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_QCELP) {
		fprintf(stdout, "\t3GPP QCELP stream - Sample Rate %d - %d channel(s) %d bits per samples\n", sr, nb_ch, (u32) bps);
	} else if (msub_type == GF_ISOM_SUBTYPE_3GP_SMV) {
		fprintf(stdout, "\t3GPP SMV stream - Sample Rate %d - %d channel(s) %d bits per samples\n", sr, nb_ch, (u32) bps);
	} else if (mtype==GF_ISOM_MEDIA_HINT) {
		u32 refTrack;
		s32 i, refCount = gf_isom_get_reference_count(file, trackNum, GF_ISOM_REF_HINT);
		if (refCount) {
			fprintf(stdout, "\tStreaming Hint Track for track%s ", (refCount>1) ? "s" :"");
			for (i=0; i<refCount; i++) {
				gf_isom_get_reference(file, trackNum, GF_ISOM_REF_HINT, i+1, &refTrack);
				if (i) fprintf(stdout, " - ");
				fprintf(stdout, "ID %d", gf_isom_get_track_id(file, refTrack));
			}
			fprintf(stdout, "\n");
		} else {
			fprintf(stdout, "\tStreaming Hint Track (no refs)\n");
		}
	} else if (mtype==GF_ISOM_MEDIA_FLASH) {
		fprintf(stdout, "Macromedia Flash Movie\n");
	} else if (mtype==GF_ISOM_MEDIA_TEXT) {
		u32 w, h;
		s16 l;
		s32 tx, ty;
		gf_isom_get_track_layout_info(file, trackNum, &w, &h, &tx, &ty, &l);
		fprintf(stdout, "3GPP/MPEG-4 Timed Text - Size %d x %d - Translation X=%d Y=%d - Layer %d\n", w, h, tx, ty, l);
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
				free(udesc->extension_buf);
			}
			free(udesc);
		} else {
			fprintf(stdout, "Unknown track type\n");
		}
	}

	DumpMetaItem(file, 0, trackNum, "Track Meta");
	if (!full_dump) {
		fprintf(stdout, "\n");
		return;
	}

	dur = size = 0;
	max_rate = rate = 0;
	time_slice = 0;
	ts = gf_isom_get_media_timescale(file, trackNum);
	for (j=0; j<gf_isom_get_sample_count(file, trackNum); j++) {
		GF_ISOSample *samp = gf_isom_get_sample_info(file, trackNum, j+1, NULL, NULL);
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
	fprintf(stdout, "\tTotal size %d bytes - Total samples duration %d ms\n", size, (u32) dur);
	if (!dur) {
		fprintf(stdout, "\n");
		return;
	}
	dur /= 1000;
	rate = (u32) (size * 8 / dur);
	max_rate *= 8;
	if (rate >= 1500) {
		rate /= 1024;
		max_rate /= 1024;
		fprintf(stdout, "\tAverage rate %d kbps - Max Rate %d kbps\n", rate, max_rate);
	} else {
		fprintf(stdout, "\tAverage rate %d bps - Max Rate %d bps\n", rate, max_rate);
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

void DumpMovieInfo(GF_ISOFile *file)
{
	GF_InitialObjectDescriptor *iod;
	u32 i, brand, min, timescale, count;
	char szDur[50];
	
	DumpMetaItem(file, 1, 0, "Root Meta");
	if (!gf_isom_has_movie(file)) {
		fprintf(stdout, "File has no movie (moov) - static data container\n");
		return;
	}

	timescale = gf_isom_get_timescale(file);
	fprintf(stdout, "* Movie Info *\n\tTimescale %d - Duration %s\n\tFragmented File %s - %d track(s)\n",
		timescale, format_duration(gf_isom_get_duration(file), timescale, szDur), gf_isom_is_fragmented(file) ? "yes" : "no", gf_isom_get_track_count(file));

	if (gf_isom_get_brand_info(file, &brand, &min, NULL) == GF_OK) {
		fprintf(stdout, "\tFile Brand %s - version %d\n", gf_4cc_to_str(brand), min);
	}
	fprintf(stdout, "\n");

	DumpMetaItem(file, 0, 0, "Moov Meta");

	iod = (GF_InitialObjectDescriptor *) gf_isom_get_root_od(file);
	if (iod) {
		if (iod->tag == GF_ODF_IOD_TAG) {
			fprintf(stdout, "File has root IOD\n");
			fprintf(stdout, "Scene PL 0x%02x - Graphics PL 0x%02x - OD PL 0x%02x\n", iod->scene_profileAndLevel, iod->graphics_profileAndLevel, iod->OD_profileAndLevel);
			fprintf(stdout, "Visual PL: %s (0x%02x)\n", gf_m4v_get_profile_name(iod->visual_profileAndLevel), iod->visual_profileAndLevel);
			fprintf(stdout, "Audio PL: %s (0x%02x)\n", gf_m4a_get_profile_name(iod->audio_profileAndLevel), iod->audio_profileAndLevel);
			//fprintf(stdout, "inline profiles included %s\n", iod->inlineProfileFlag ? "yes" : "no");
		} else {
			fprintf(stdout, "File has root OD\n");
		}
		if (!gf_list_count(iod->ESDescriptors)) fprintf(stdout, "No streams included in root OD\n");
		gf_odf_desc_del((GF_Descriptor *) iod);
	} else {
		fprintf(stdout, "File has no MPEG4 IOD/OD\n");
	}

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

	fprintf(stdout, "\n");
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		DumpTrackInfo(file, gf_isom_get_track_id(file, i+1), 0);
	}
}

