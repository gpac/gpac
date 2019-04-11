/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Management sub-project
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
#include <gpac/constants.h>
#include <gpac/utf.h>
#include <gpac/network.h>
#include <gpac/xml.h>
#include <gpac/internal/bifs_dev.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_x3d.h>

#ifndef GPAC_DISABLE_LOADER_XMT

void gf_sm_update_bitwrapper_buffer(GF_Node *node, const char *fileName);

/*for QP types*/
#include "../bifs/quant.h"

typedef struct
{
	GF_Node *node;
	GF_FieldInfo container_field;
	GF_ChildNodeItem *last;
} XMTNodeStack;


/**/
enum
{
	/*document is not yet initialized*/
	XMT_STATE_INIT = 0,
	/*document head is being parsed*/
	XMT_STATE_HEAD = 1,
	/*document body being parsed*/
	XMT_STATE_BODY = 2,
	/*commands are being parsed*/
	XMT_STATE_COMMANDS = 3,
	/*elements are being parsed*/
	XMT_STATE_ELEMENTS = 4,
	/*end of body parsing*/
	XMT_STATE_BODY_END = 5,
	/*end of parsing*/
	XMT_STATE_END = 6,
};


typedef struct
{
	/*1: XMT-A, 2: X3D, 3: XMT-O (not supported yet) */
	u32 doc_type;
	/*0: not init, 1: header, 2: body*/
	u32 state;
	u32 current_node_tag;

	GF_SceneLoader *load;
	GF_Err last_error;
	GF_SAXParser *sax_parser;
	XMTNodeStack *x3d_root;

	/* stack of nodes for SAX parsing*/
	GF_List *nodes;
	/* stack of descriptors for SAX parsing*/
	GF_List *descriptors;

	GF_List *peeked_nodes;
	GF_List *def_nodes;
	GF_List *inserted_routes, *unresolved_routes;

	/* OD and ESD links*/
	GF_List *od_links, *esd_links;
	/*set when parsing proto*/
	GF_Proto *parsing_proto;
	GF_ProtoFieldInterface *proto_field;

	GF_StreamContext *scene_es;
	GF_AUContext *scene_au;
	u32 base_scene_id;
	/*current scene command*/
	GF_Command *command;
	SFCommandBuffer *command_buffer;

	GF_StreamContext *od_es;
	GF_AUContext *od_au;
	u32 base_od_id;
	/*current od command*/
	GF_ODCom *od_command;


	/*current stream ID, AU time and RAP flag*/
	u32 stream_id;
	Double au_time;
	Bool au_is_rap;
	Bool in_com;
	GF_List *script_to_load;
} GF_XMTParser;


typedef struct
{
	char *desc_name;
	u32 ID;
	/*store nodes refering to this URL*/
	GF_List *mf_urls;
	GF_ObjectDescriptor *od;
} XMT_ODLink;

typedef struct
{
	char *desc_name;
	u32 ESID;
	GF_ESD *esd;
	char *OCR_Name;
	char *Depends_Name;
} XMT_ESDLink;


static GF_Err xmt_report(GF_XMTParser *parser, GF_Err e, char *format, ...)
{
#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_PARSER, e ? GF_LOG_ERROR : GF_LOG_WARNING)) {
		char szMsg[2048];
		va_list args;
		va_start(args, format);
		vsnprintf(szMsg, 2048, format, args);
		va_end(args);
		GF_LOG((u32) (e ? GF_LOG_ERROR : GF_LOG_WARNING), GF_LOG_PARSER, ("[XMT Parsing] %s (line %d)\n", szMsg, gf_xml_sax_get_line(parser->sax_parser)) );
	}
#endif
	if (e) parser->last_error = e;
	return e;
}
static void xmt_progress(void *cbk, u64 done, u64 total)
{
	gf_set_progress("XMT Parsing", done, total);
}
static Bool xmt_esid_available(GF_XMTParser *parser, u16 ESID)
{
	u32 i;
	XMT_ESDLink *esdl;
	i=0;
	while ((esdl = (XMT_ESDLink *)gf_list_enum(parser->esd_links, &i))) {
		if (esdl->ESID == ESID) return 0;
	}
	return 1;
}

static char *xmt_get_es_name(GF_XMTParser *parser, u16 ESID)
{
	u32 i;
	XMT_ESDLink *esdl;
	i=0;
	while ((esdl = (XMT_ESDLink *)gf_list_enum(parser->esd_links, &i))) {
		if (esdl->ESID == ESID) return esdl->desc_name;
	}
	return NULL;
}

static void xmt_new_od_link(GF_XMTParser *parser, GF_ObjectDescriptor *od, char *name, u32 ID)
{
	u32 i, j, count;
	XMT_ODLink *odl;

	if (!ID) {
		if (!strnicmp(name, "od", 2)) ID = atoi(name + 2);
		else if (!strnicmp(name, "iod", 3)) ID = atoi(name+ 3);
		/*be carefull, an url like "11-regression-test.mp4" will return 1 on sscanf :)*/
		else if (sscanf(name, "%u", &ID) == 1) {
			char szURL[20];
			sprintf(szURL, "%u", ID);
			if (strcmp(szURL, name)) {
				ID = 0;
			} else {
				name = NULL;
			}
		}
	}

	count = gf_list_count(parser->od_links);
	for (i=0; i<count; i++) {
		odl = (XMT_ODLink*)gf_list_get(parser->od_links, i);
		if ( (ID && (odl->ID == ID))
		        || (odl->od == od)
		        || (odl->desc_name && name && !strcmp(odl->desc_name, name))
		   ) {
			if (!odl->od) odl->od = od;
			if (!odl->desc_name && name) odl->desc_name = gf_strdup(name);
			if (!od->objectDescriptorID) {
				od->objectDescriptorID = ID;
			} else if (ID && (od->objectDescriptorID != ID)) {
				xmt_report(parser, GF_BAD_PARAM, "Conflicting OD IDs %d vs %d\n", ID, od->objectDescriptorID);
			}

			for (j=i+1; j<count; j++) {
				XMT_ODLink *l2 = (XMT_ODLink*)gf_list_get(parser->od_links, j);
				if (l2->od == od) {
					odl->ID = od->objectDescriptorID = odl->od->objectDescriptorID;
					gf_list_rem(parser->od_links, j);
					if (l2->desc_name) gf_free(l2->desc_name);
					gf_list_del(l2->mf_urls);
					gf_free(l2);
					break;
				}
			}
			return;
		}
	}
	GF_SAFEALLOC(odl, XMT_ODLink);
	if (!odl) return;

	odl->mf_urls = gf_list_new();
	odl->od = od;
	if (ID) od->objectDescriptorID = ID;
	if (name) odl->desc_name = gf_strdup(name);
	gf_list_add(parser->od_links, odl);
}

static void xmt_new_od_link_from_node(GF_XMTParser *parser, char *name, MFURL *url)
{
	u32 i, ID;
	XMT_ODLink *odl;

	/*find OD_ID*/
	ID = 0;
	if (!strnicmp(name, "od", 2)) ID = atoi(name + 2);
	else if (!strnicmp(name, "iod", 3)) ID = atoi(name + 3);
	/*be carefull, an url like "11-regression-test.mp4" will return 1 on sscanf :)*/
	else if (sscanf(name, "%u", &ID) == 1) {
		char szURL[20];
		sprintf(szURL, "%u", ID);
		if (strcmp(szURL, name)) {
			ID = 0;
		} else {
			name = NULL;
		}
	}
	else ID = 0;

	/*write OD_ID*/
	assert(url->count);
	i = url->count - 1;
	url->vals[i].OD_ID = 0;
	url->vals->OD_ID = ID;

	i=0;
	while ((odl = (XMT_ODLink*)gf_list_enum(parser->od_links, &i))) {
		if ( (name && odl->desc_name && !strcmp(odl->desc_name, name))
		        || (ID && odl->od && odl->od->objectDescriptorID==ID)
		        || (ID && (odl->ID==ID))
		   ) {
			if (url && (gf_list_find(odl->mf_urls, url)<0) ) gf_list_add(odl->mf_urls, url);
			return;
		}
	}
	GF_SAFEALLOC(odl, XMT_ODLink);
	if (!odl) return;

	odl->mf_urls = gf_list_new();
	if (url) gf_list_add(odl->mf_urls, url);
	if (ID) odl->ID = ID;
	else odl->desc_name = gf_strdup(name);
	gf_list_add(parser->od_links, odl);
}
static void xmt_new_esd_link(GF_XMTParser *parser, GF_ESD *esd, char *desc_name, u32 binID)
{
	u32 i, j;
	XMT_ESDLink *esdl;

	i=0;
	while ((esdl = (XMT_ESDLink *)gf_list_enum(parser->esd_links, &i))) {
		if (esdl->esd  && (esd!=esdl->esd)) continue;
		if (!esdl->esd) {
			if (!esdl->ESID || !desc_name || strcmp(esdl->desc_name, desc_name)) continue;
			esdl->esd = esd;
		}
		if (binID) {
			/*remove temp links*/
			if (esdl->ESID == (u16) ( ( (PTR_TO_U_CAST esdl) >> 16) | ( (PTR_TO_U_CAST esdl) & 0x0000FFFF) ) ) {
				GF_StreamContext *sc;
				j=0;
				while ((sc = (GF_StreamContext *)gf_list_enum(parser->load->ctx->streams, &j))) {
					if (sc->ESID!=esdl->ESID) continue;
					/*reassign*/
					sc->ESID = binID;
					break;
				}
			}
			esdl->ESID = esdl->esd->ESID = binID;
		}
		if (desc_name && !esdl->desc_name) {
			esdl->desc_name = gf_strdup(desc_name);
			if (!esdl->ESID && !strnicmp(desc_name, "es", 2)) esdl->ESID = atoi(&desc_name[2]);
		}
		return;
	}
	GF_SAFEALLOC(esdl, XMT_ESDLink);
	if (!esdl) return;

	esdl->esd = esd;
	esd->ESID = esdl->ESID = binID;
	if (desc_name) {
		if (!esdl->ESID && !strnicmp(desc_name, "es", 2)) esdl->ESID = atoi(&desc_name[2]);
		esdl->desc_name = gf_strdup(desc_name);
	}
	if (!esd->ESID) {
		esd->ESID = 1;
		while (!xmt_esid_available(parser, esd->ESID)) esd->ESID++;
		esdl->ESID = esd->ESID;
	}

	gf_list_add(parser->esd_links, esdl);
}
static Bool xmt_set_depend_id(GF_XMTParser *parser, GF_ESD *desc, char *es_name, Bool is_ocr_dep)
{
	u32 i;
	XMT_ESDLink *esdl;
	if (!desc || !es_name) return 0;

	i=0;
	while ((esdl = (XMT_ESDLink *)gf_list_enum(parser->esd_links, &i))) {
		if (esdl->esd == desc) {
			if (is_ocr_dep)
				esdl->OCR_Name = gf_strdup(es_name);
			else
				esdl->Depends_Name = gf_strdup(es_name);
			return 1;
		}
	}
	return 0;
}

static u32 xmt_get_od_id(GF_XMTParser *parser, char *od_name)
{
	u32 i, ID;
	XMT_ODLink *l;
	if (sscanf(od_name, "%u", &ID)==1) return ID;

	i=0;
	while ((l = (XMT_ODLink*)gf_list_enum(parser->od_links, &i))) {
		if (!l->od) continue;
		if (l->desc_name && !strcmp(l->desc_name, od_name)) return l->od->objectDescriptorID;
	}
	return 0;
}

static u32 xmt_get_esd_id(GF_XMTParser *parser, char *esd_name)
{
	u32 i, ID;
	XMT_ESDLink *l;
	if (sscanf(esd_name, "%u", &ID)==1) return ID;

	i=0;
	while ((l = (XMT_ESDLink *)gf_list_enum(parser->esd_links, &i))) {
		if (!l->esd) continue;
		if (l->desc_name && !strcmp(l->desc_name, esd_name)) return l->esd->ESID;
	}
	return 0;
}
static u32 xmt_locate_stream(GF_XMTParser *parser, char *stream_name)
{
	XMT_ESDLink *esdl;
	u32 i;
	char szN[200];

	i=0;
	while ((esdl = (XMT_ESDLink *)gf_list_enum(parser->esd_links, &i))) {
		if (esdl->desc_name && !strcmp(esdl->desc_name, stream_name)) return esdl->ESID;
		if (esdl->ESID) {
			sprintf(szN, "es%d", esdl->ESID);
			if (!strcmp(szN, stream_name)) return esdl->ESID;
			sprintf(szN, "%d", esdl->ESID);
			if (!strcmp(szN, stream_name)) return esdl->ESID;
		}
	}
	if (parser->load->ctx) {
		GF_StreamContext *sc;
		i=0;
		while ((sc = gf_list_enum(parser->load->ctx->streams, &i))) {
			if (sc->name && !strcmp(sc->name, stream_name)) return sc->ESID;
			sprintf(szN, "%d", sc->ESID);
			if (!strcmp(szN, stream_name)) return sc->ESID;
		}
	}
	/*create a temp one*/
	esdl = (XMT_ESDLink *)gf_malloc(sizeof(XMT_ESDLink));
	memset(esdl, 0, sizeof(XMT_ESDLink));
	esdl->ESID = (u16) ( (PTR_TO_U_CAST esdl) >> 16) | ( (PTR_TO_U_CAST esdl) & 0x0000FFFF);
	if (!strnicmp(stream_name, "es", 2)) esdl->ESID = atoi(&stream_name[2]);
	esdl->desc_name = gf_strdup(stream_name);
	gf_list_add(parser->esd_links, esdl);
	return esdl->ESID;
}
static Bool xmt_odid_available(GF_XMTParser *parser, u16 ODID)
{
	u32 i;
	XMT_ODLink *l;
	i=0;
	while ((l = (XMT_ODLink*)gf_list_enum(parser->od_links, &i))) {
		if (l->ID == ODID) return 0;
		if (l->od && l->od->objectDescriptorID == ODID) return 0;
	}
	return 1;
}

static void xmt_resolve_od_links(GF_XMTParser *parser)
{
	u32 i, j;
	XMT_ESDLink *esdl, *esdl2;
	XMT_ODLink *l;
	char szURL[5000];

	/*fix ESD IDs*/
	i=0;
	while ((esdl = (XMT_ESDLink *)gf_list_enum(parser->esd_links, &i))) {
		if (!esdl->esd) {
			xmt_report(parser, GF_BAD_PARAM, "Stream %s ID %d has no associated ES descriptor\n", esdl->desc_name ? esdl->desc_name : "", esdl->ESID);
			i--;
			gf_list_rem(parser->esd_links, i);
			if (esdl->desc_name) gf_free(esdl->desc_name);
			gf_free(esdl);
			continue;
		}
		if (esdl->ESID) esdl->esd->ESID = esdl->ESID;
		else if (!esdl->esd->ESID) {
			u16 ESID = 1;
			while (!xmt_esid_available(parser, ESID)) ESID++;
			esdl->esd->ESID = ESID;
		}
	}

	/*set OCR es ids*/
	i=0;
	while ((esdl = (XMT_ESDLink *)gf_list_enum(parser->esd_links, &i))) {
		Bool use_old_fmt;
		u16 ocr_id;
		char szTest[50];

		esdl->esd->OCRESID = 0;
		if (!esdl->OCR_Name) continue;

		use_old_fmt = 0;
		ocr_id = atoi(esdl->OCR_Name);
		sprintf(szTest, "%d", ocr_id);
		if (!stricmp(szTest, esdl->OCR_Name)) use_old_fmt = 1;

		j=0;
		while ((esdl2 = (XMT_ESDLink *)gf_list_enum(parser->esd_links, &j))) {
			if (esdl2->desc_name && !strcmp(esdl2->desc_name, esdl->OCR_Name)) {
				esdl->esd->OCRESID = esdl2->esd->ESID;
				break;
			}
			if (use_old_fmt && (esdl2->esd->ESID==ocr_id)) {
				esdl->esd->OCRESID = ocr_id;
				break;
			}
		}
		if (!esdl->esd->OCRESID) {
			xmt_report(parser, GF_OK, "WARNING: Could not find clock reference %s for ES %s - forcing self-synchronization", esdl->OCR_Name, esdl->desc_name);
		}
		gf_free(esdl->OCR_Name);
		esdl->OCR_Name = NULL;
	}

	/*set dependsOn es ids*/
	i=0;
	while ((esdl = (XMT_ESDLink *)gf_list_enum(parser->esd_links, &i))) {
		Bool use_old_fmt;
		u16 dep_id;
		char szTest[50];

		esdl->esd->dependsOnESID = 0;
		if (!esdl->Depends_Name) continue;

		use_old_fmt = 0;
		dep_id = atoi(esdl->Depends_Name);
		sprintf(szTest, "%d", dep_id);
		if (!stricmp(szTest, esdl->Depends_Name)) use_old_fmt = 1;

		j=0;
		while ((esdl2 = (XMT_ESDLink *)gf_list_enum(parser->esd_links, &j))) {
			if (esdl2->desc_name && !strcmp(esdl2->desc_name, esdl->Depends_Name)) {
				esdl->esd->dependsOnESID = esdl2->esd->ESID;
				break;
			}
			if (use_old_fmt && (esdl2->esd->ESID==dep_id)) {
				esdl->esd->dependsOnESID = dep_id;
				break;
			}
		}
		if (!esdl->esd->dependsOnESID) {
			xmt_report(parser, GF_OK, "WARNING: Could not find stream dependance %s for ES %s - forcing self-synchronization", esdl->Depends_Name, esdl->desc_name);
		}
		gf_free(esdl->Depends_Name);
		esdl->Depends_Name = NULL;
	}

	while (gf_list_count(parser->esd_links)) {
		esdl = (XMT_ESDLink *)gf_list_get(parser->esd_links, 0);
		gf_list_rem(parser->esd_links, 0);
		if (esdl->desc_name) gf_free(esdl->desc_name);
		gf_free(esdl);
	}

	i=0;
	while ((l = (XMT_ODLink*)gf_list_enum(parser->od_links, &i))) {
		if (l->od && !l->od->objectDescriptorID) {
			u16 ODID = 1;
			while (!xmt_odid_available(parser, ODID)) ODID++;
			l->od->objectDescriptorID = ODID;
		}
		if (l->od) {
			if (!l->ID) l->ID = l->od->objectDescriptorID;
			assert(l->ID == l->od->objectDescriptorID);
		}
	}

	/*unroll dep in case some URLs reference ODs by their binary IDs not their string ones*/
	i=0;
	while ((l = (XMT_ODLink*)gf_list_enum(parser->od_links, &i))) {
		XMT_ODLink *l2;
		/*not OD URL*/
		if (!l->ID) continue;
		j=i+1;
		while ((l2 = (XMT_ODLink*)gf_list_enum(parser->od_links, &j))) {
			/*not OD URL*/
			if (!l2->ID) continue;
			if (l->ID == l2->ID) {
				while (gf_list_count(l2->mf_urls)) {
					MFURL *url = (MFURL *)gf_list_get(l2->mf_urls, 0);
					gf_list_rem(l2->mf_urls, 0);
					gf_list_add(l->mf_urls, url);
				}
				j--;
				gf_list_rem(parser->od_links, j);
				if (l2->desc_name) gf_free(l2->desc_name);
				gf_list_del(l2->mf_urls);
				gf_free(l2);
			}
		}
	}

	while (gf_list_count(parser->od_links) ) {
		l = (XMT_ODLink*)gf_list_get(parser->od_links, 0);
		if (!l->od) {
			/*if no ID found this is not an OD URL*/
			if (l->ID) {
				if (l->desc_name) {
					xmt_report(parser, GF_OK, "WARNING: OD \"%s\" (ID %d) not assigned", l->desc_name, l->ID);
				} else {
					xmt_report(parser, GF_OK, "WARNING: OD ID %d not assigned", l->ID);
				}
			}
		} else {
			MFURL *the_url;
			j=0;
			while ((the_url = (MFURL *)gf_list_enum(l->mf_urls, &j))) {
				u32 k;
				char *seg = NULL;
				for (k=0; k<the_url->count; k++) {
					SFURL *url = &the_url->vals[k];
					if (url->url) seg = strstr(url->url, "#");
					if (seg) {
						sprintf(szURL, "od:%d#%s", l->od->objectDescriptorID, seg+1);
						gf_free(url->url);
						url->url = gf_strdup(szURL);
					} else {
						if (url->url) gf_free(url->url);
						url->url = NULL;
						url->OD_ID = l->od->objectDescriptorID;
					}
				}
			}
		}

		if (l->desc_name) gf_free(l->desc_name);
		gf_list_del(l->mf_urls);
		gf_free(l);
		gf_list_rem(parser->od_links, 0);
	}
}


static u32 xmt_get_next_node_id(GF_XMTParser *parser)
{
	u32 ID;
	GF_SceneGraph *sc = parser->load->scene_graph;
	if (parser->parsing_proto) sc = gf_sg_proto_get_graph(parser->parsing_proto);
	ID = gf_sg_get_next_available_node_id(sc);
	if (parser->load->ctx && (ID>parser->load->ctx->max_node_id))
		parser->load->ctx->max_node_id = ID;
	return ID;
}
static u32 xmt_get_node_id(GF_XMTParser *parser, char *name)
{
	GF_Node *n = NULL;
	u32 ID = 0;
	if (sscanf(name, "N%u", &ID) == 1) {
		u32 k=1;
		while (name && name[k]) {
			if (strchr("0123456789", name[k])==0) {
				ID = 0;
				break;
			}
			k++;
		}
		if (ID) {
			ID ++;
			n = gf_sg_find_node(parser->load->scene_graph, ID);
			if (!n) {
				if (parser->load->ctx && (parser->load->ctx->max_node_id<ID)) parser->load->ctx->max_node_id=ID;
				return ID;
			}
		}
	}
	ID = xmt_get_next_node_id(parser);
	if (n) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[XMT Parsing] (line %d) Binary ID %s already assigned to %s - keeping internal ID %d\n", gf_xml_sax_get_line(parser->sax_parser), name, gf_node_get_name(n), ID));
	}
	return ID;
}

static u32 xmt_get_node_tag(GF_XMTParser *parser, const char *node_name)
{
	u32 tag;
	/*if VRML and allowing non MPEG4 nodes, use X3D*/
	if ((parser->doc_type==2) && !(parser->load->flags & GF_SM_LOAD_MPEG4_STRICT)) {
#ifndef GPAC_DISABLE_X3D
		tag = gf_node_x3d_type_by_class_name(node_name);
		if (!tag)
#endif
			tag = gf_node_mpeg4_type_by_class_name(node_name);
	} else {
		tag = gf_node_mpeg4_type_by_class_name(node_name);
		/*if allowing non MPEG4 nodes, try X3D*/
#ifndef GPAC_DISABLE_X3D
		if (!tag && !(parser->load->flags & GF_SM_LOAD_MPEG4_STRICT)) tag = gf_node_x3d_type_by_class_name(node_name);
#endif
	}
	return tag;
}

static GF_Node *xmt_find_node(GF_XMTParser *parser, char *ID)
{
	u32 i, count, tag;
	Bool is_proto;
	char *node_class;
	GF_Node *n;
	if (!ID) return NULL;
	n = gf_sg_find_node_by_name(parser->load->scene_graph, ID);
	if (n) return n;

	count = gf_list_count(parser->peeked_nodes);
	for (i=0; i<count; i++) {
		n = (GF_Node*)gf_list_get(parser->peeked_nodes, i);
		if (!strcmp(gf_node_get_name(n), ID)) return n;
	}
	node_class = gf_xml_sax_peek_node(parser->sax_parser, "DEF", ID, "ProtoInstance", "name", "<par", &is_proto);
	if (!node_class) return NULL;

	n = NULL;
	if (is_proto) {
		GF_Proto *p;
		GF_SceneGraph *sg = parser->load->scene_graph;
		while (1) {
			p = gf_sg_find_proto(sg, 0, node_class);
			if (p) break;
			sg = sg->parent_scene;
			if (!sg) break;
		}
		if (!p) {
			xmt_report(parser, GF_BAD_PARAM, "%s: not a valid/supported proto", node_class);
			gf_free(node_class);
			return NULL;
		}
		n = gf_sg_proto_create_instance(parser->load->scene_graph, p);
	} else {
		tag = xmt_get_node_tag(parser, node_class);
		n = gf_node_new(parser->load->scene_graph, tag);
	}
	gf_free(node_class);
	if (n) {
		u32 nID = xmt_get_node_id(parser, ID);
		gf_node_set_id(n, nID, ID);
		if (!parser->parsing_proto) gf_node_init(n);
		gf_list_add(parser->peeked_nodes, n);
	}
	return n;
}

#define XMT_GET_ONE_VAL \
	char value[100];	\
	u32 i;			\
	char *str = a_value;	\
	if (!str) {		\
		xmt_report(parser, GF_BAD_PARAM, "%s: Number expected", name);	\
		return 0;		\
	}		\
	while (str[0] == ' ') str += 1;	\
	i = 0;	\
	while ((str[i] != ' ') && str[i]) {	\
		value[i] = str[i];			\
		i++;				\
	}					\
	value[i] = 0;	\
	while ((str[i] == ' ') && str[i]) i++;

static u32 xmt_parse_int(GF_XMTParser *parser, const char *name, SFInt32 *val, char *a_value)
{
	XMT_GET_ONE_VAL
	*val = atoi(value);
	return i;
}
static u32 xmt_parse_float(GF_XMTParser *parser, const char *name, SFFloat *val, char *a_value)
{
	XMT_GET_ONE_VAL
	*val = FLT2FIX(atof(value));
	return i;
}
static u32 xmt_parse_time(GF_XMTParser *parser, const char *name, SFTime *val, char *a_value)
{
	XMT_GET_ONE_VAL
	*val = atof(value);
	return i;
}
static u32 xmt_parse_bool(GF_XMTParser *parser, const char *name, SFBool *val, char *a_value)
{
	XMT_GET_ONE_VAL
	if (!stricmp(value, "1") || !stricmp(value, "true"))
		*val = 1;
	else
		*val = 0;
	return i;
}

static u32 xmt_parse_string(GF_XMTParser *parser, const char *name, SFString *val, Bool is_mf, char *a_value)
{
	char *value;
	char sep[10];
	u32 len;
	u32 i=0;
	u32 k=0;
	char *str = a_value;
	if (!str) return 0;

	/*SF string, no inspection*/
	if (!is_mf) {
		len = (u32) strlen(str);
		if (val->buffer) gf_free(val->buffer);
		val->buffer = NULL;
		if (len) val->buffer = gf_strdup(str);
		return len+1;
	}

	/*now this is the REAL pain:
		X3D allows '"String1" "String2"' and therefore '"String &quot;test&quot;"'
		XMT allows '&quot;String1&quot; &quot;String2&quot;' and therefore '&quot;String \&quot;test\&quot;&quot;'
	thus translating the string from xml to UTF may screw up the separators !! We need to identify them
	*/

	i = 0;
	while ((str[i]==' ') || (str[i]=='\t')) i++;
	if (!strncmp(&str[i], "&quot;", 6)) strcpy(sep, "&quot;");
	else if (!strncmp(&str[i], "&apos;", 6)) strcpy(sep, "&apos;");
	else if (str[i]=='\'') strcpy(sep, "\'");
	else if (str[i]=='\"') strcpy(sep, "\"");
	/*handle as a single field (old GPAC XMT & any unknown cases...*/
	else {
		len = (u32) strlen(str);
		if (val->buffer) gf_free(val->buffer);
		val->buffer = NULL;
		if (len) val->buffer = gf_strdup(str);
		return len;
	}
	k = 0;
	i += (u32) strlen(sep);

	value = gf_strdup(str);

	if (strncmp(&str[i], sep, strlen(sep))) {

		while (str[i]) {
			if ((str[i] == '\\') && !strncmp(&str[i+1], sep, strlen(sep))) {
				i++;
				continue;
			}
			value[k] = str[i];
			i++;
			k++;
			if (!strncmp(&str[i], sep, strlen(sep)) && (str[i-1] != '\\')) break;
		}
	}
	value[k] = 0;
	len = (u32) strlen(sep) + i;

	if (val->buffer) gf_free(val->buffer);
	val->buffer = NULL;
	if (strlen(value)) val->buffer = gf_strdup(value);
	gf_free(value);
	return len;
}

static u32 xmt_parse_url(GF_XMTParser *parser, const char *name, MFURL *val, GF_Node *owner, Bool is_mf, char *a_value)
{
	SFString sfstr;
	u32 res, idx;
	char value[5000], *tmp;

	/*parse as a string*/
	sfstr.buffer = NULL;
	res = xmt_parse_string(parser, name, &sfstr, is_mf, a_value);
	if (parser->last_error) return res;

	assert(val->count);
	idx = val->count - 1;
	if (val->vals[idx].url) gf_free(val->vals[idx].url);
	val->vals[idx].url = sfstr.buffer;
	val->vals[idx].OD_ID = 0;
	/*empty*/
	if (!val->vals[idx].url) return res;

	/*remove segments & viewpoints info to create OD link*/
	strcpy(value, val->vals[idx].url);
	tmp = strstr(value, "#");
	if (tmp) tmp[0] = 0;

	/*according to XMT-A spec, both 'od:' and 'od://' are tolerated in XMT-A*/
	if (!strnicmp(value, "od://", 5))
		xmt_new_od_link_from_node(parser, value+5, val);
	else if (!strnicmp(value, "od:", 3))
		xmt_new_od_link_from_node(parser, value+3, val);
	else
		xmt_new_od_link_from_node(parser, value, val);
	return res;
}


static u32 xmt_parse_script(GF_XMTParser *parser, const char *name, SFScript *val, Bool is_mf, char *a_value)
{
	SFString sfstr;
	u32 res;

	/*parse as a string*/
	sfstr.buffer = NULL;
	res = xmt_parse_string(parser, name, &sfstr, is_mf, a_value);
	if (parser->last_error) return res;

	if (val->script_text) gf_free(val->script_text);
	val->script_text = (char*)sfstr.buffer;
	return res;
}

static void xmt_offset_time(GF_XMTParser *parser, Double *time)
{
	*time += parser->au_time;
}
static void xmt_check_time_offset(GF_XMTParser *parser, GF_Node *n, GF_FieldInfo *info)
{
	if (!(parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK)) return;
	if (gf_node_get_tag(n) != TAG_ProtoNode) {
		if (!stricmp(info->name, "startTime") || !stricmp(info->name, "stopTime"))
			xmt_offset_time(parser, (Double *)info->far_ptr);
	} else if (gf_sg_proto_field_is_sftime_offset(n, info)) {
		xmt_offset_time(parser, (Double *)info->far_ptr);
	}
}

static u32 xmt_parse_sf_field(GF_XMTParser *parser, GF_FieldInfo *info, GF_Node *n, char *a_value)
{
	u32 res = 0;
	switch (info->fieldType) {
	case GF_SG_VRML_SFINT32:
		res = xmt_parse_int(parser, info->name, (SFInt32 *)info->far_ptr, a_value);
		break;
	case GF_SG_VRML_SFBOOL:
		res = xmt_parse_bool(parser, info->name, (SFBool *)info->far_ptr, a_value);
		break;
	case GF_SG_VRML_SFFLOAT:
		res = xmt_parse_float(parser, info->name, (SFFloat *)info->far_ptr, a_value);
		break;
	case GF_SG_VRML_SFTIME:
		res = xmt_parse_time(parser, info->name, (SFTime *)info->far_ptr, a_value);
		if (n) xmt_check_time_offset(parser, n, info);
		break;
	case GF_SG_VRML_SFCOLOR:
		res = xmt_parse_float(parser, info->name, & ((SFColor *)info->far_ptr)->red, a_value);
		res += xmt_parse_float(parser, info->name, & ((SFColor *)info->far_ptr)->green, a_value + res);
		res += xmt_parse_float(parser, info->name, & ((SFColor *)info->far_ptr)->blue, a_value + res);
		break;
	case GF_SG_VRML_SFVEC2F:
		res = xmt_parse_float(parser, info->name, & ((SFVec2f *)info->far_ptr)->x, a_value);
		res += xmt_parse_float(parser, info->name, & ((SFVec2f *)info->far_ptr)->y, a_value + res);
		break;
	case GF_SG_VRML_SFVEC3F:
		res = xmt_parse_float(parser, info->name, & ((SFVec3f *)info->far_ptr)->x, a_value);
		res += xmt_parse_float(parser, info->name, & ((SFVec3f *)info->far_ptr)->y, a_value + res);
		res += xmt_parse_float(parser, info->name, & ((SFVec3f *)info->far_ptr)->z, a_value + res);
		break;
	case GF_SG_VRML_SFROTATION:
		res = xmt_parse_float(parser, info->name, & ((SFRotation *)info->far_ptr)->x, a_value);
		res += xmt_parse_float(parser, info->name, & ((SFRotation *)info->far_ptr)->y, a_value + res);
		res += xmt_parse_float(parser, info->name, & ((SFRotation *)info->far_ptr)->z, a_value + res);
		res += xmt_parse_float(parser, info->name, & ((SFRotation *)info->far_ptr)->q, a_value + res);
		break;
	case GF_SG_VRML_SFSTRING:
		res = xmt_parse_string(parser, info->name, (SFString*)info->far_ptr, 0, a_value);
		if (n && (n->sgprivate->tag==TAG_MPEG4_BitWrapper)) {
			gf_sm_update_bitwrapper_buffer(n, parser->load->fileName);
		}
		break;
	case GF_SG_VRML_SFSCRIPT:
		res = xmt_parse_script(parser, info->name, (SFScript *)info->far_ptr, 0, a_value);
		break;
	case GF_SG_VRML_SFCOMMANDBUFFER:
	{
		SFCommandBuffer *cb = (SFCommandBuffer *)info->far_ptr;
		if (parser->command_buffer) {
			cb->buffer = (unsigned char*)parser->command_buffer;
		} else {
			cb->buffer = (unsigned char*)parser->command;
		}
		parser->command_buffer = cb;
	}
	break;

	case GF_SG_VRML_SFIMAGE:
	{
		u32 k, size, v;
		SFImage *img = (SFImage *)info->far_ptr;
		res = xmt_parse_int(parser, "width", (SFInt32*)&img->width, a_value);
		if (parser->last_error) return res;
		res += xmt_parse_int(parser, "height", (SFInt32*)&img->height, a_value + res);
		if (parser->last_error) return res;
		res += xmt_parse_int(parser, "nbComp", (SFInt32*)&v, a_value + res);
		if (parser->last_error) return res;
		img->numComponents = v;
		size = img->width * img->height * img->numComponents;
		if (img->pixels) gf_free(img->pixels);
		img->pixels = (unsigned char*)gf_malloc(sizeof(char) * size);
		a_value += res;
		res = 0;
		for (k=0; k<size; k++) {
			char *name = "pixels";
			XMT_GET_ONE_VAL
			if (strstr(value, "0x")) sscanf(value, "%x", &v);
			else sscanf(value, "%u", &v);
			switch (img->numComponents) {
			case 1:
				img->pixels[k] = (char) v;
				break;
			case 2:
				img->pixels[k] = (char) (v>>8)&0xFF;
				img->pixels[k+1] = (char) (v)&0xFF;
				k++;
				break;
			case 3:
				img->pixels[k] = (char) (v>>16)&0xFF;
				img->pixels[k+1] = (char) (v>>8)&0xFF;
				img->pixels[k+2] = (char) (v)&0xFF;
				k+=2;
				break;
			case 4:
				img->pixels[k] = (char) (v>>24)&0xFF;
				img->pixels[k+1] = (char) (v>>16)&0xFF;
				img->pixels[k+2] = (char) (v>>8)&0xFF;
				img->pixels[k+3] = (char) (v)&0xFF;
				k+=3;
				break;
			}
			res += i;
			a_value += i;
		}
	}
	break;

	default:
		parser->last_error = GF_NOT_SUPPORTED;
		break;
	}
	return res;
}

static void xmt_parse_mf_field(GF_XMTParser *parser, GF_FieldInfo *info, GF_Node *n, char *value)
{
	u32 res;
	GF_FieldInfo sfInfo;
	sfInfo.fieldType = gf_sg_vrml_get_sf_type(info->fieldType);
	sfInfo.name = info->name;
	gf_sg_vrml_mf_reset(info->far_ptr, info->fieldType);

	if (!value || !strlen(value)) return;

	while (value[0] && !parser->last_error) {

		while (value[0] && value[0] == ' ') value++;
		if (!value[0]) break;

		gf_sg_vrml_mf_append(info->far_ptr, info->fieldType, &sfInfo.far_ptr);

		/*special case for MF type based on string (MFString, MFURL and MFScript), we need to take care
		of all possible forms of XML multi string encoding*/
		if (sfInfo.fieldType == GF_SG_VRML_SFSTRING) {
			res = xmt_parse_string(parser, info->name, (SFString*)sfInfo.far_ptr, 1, value);
		} else if (sfInfo.fieldType == GF_SG_VRML_SFURL) {
			res = xmt_parse_url(parser, info->name, (MFURL *)info->far_ptr, n, 1, value);
		} else if (sfInfo.fieldType == GF_SG_VRML_SFSCRIPT) {
			res = xmt_parse_script(parser, info->name, (SFScript*)sfInfo.far_ptr, 1, value);
		} else {
			res = xmt_parse_sf_field(parser, &sfInfo, n, value);
		}
		if (res) {
			value += res;
		} else {
			break;
		}
	}
}

static Bool xmt_has_been_def(GF_XMTParser *parser, char *node_name)
{
	u32 i, count;
	count = gf_list_count(parser->def_nodes);
	for (i=0; i<count; i++) {
		GF_Node *n = (GF_Node *)gf_list_get(parser->def_nodes, i);
		if (!strcmp(gf_node_get_name(n), node_name)) return 1;
	}
	return 0;
}

static u32 xmt_get_route(GF_XMTParser *parser, char *name, Bool del_com)
{
	u32 i;
	GF_Command *com;
	GF_Route *r = gf_sg_route_find_by_name(parser->load->scene_graph, name);
	if (r) return r->ID;
	i=0;
	while ((com = (GF_Command *)gf_list_enum(parser->inserted_routes, &i))) {
		if (com->def_name && !strcmp(com->def_name, name)) {
			if (del_com) gf_list_rem(parser->inserted_routes, i);
			return com->RouteID;
		}
	}
	return 0;
}

static Bool xmt_route_id_used(GF_XMTParser *parser, u32 ID)
{
	u32 i;
	GF_Command *com;
	GF_Route *r = gf_sg_route_find(parser->load->scene_graph, ID);
	if (r) return 1;
	i=0;
	while ((com = (GF_Command *)gf_list_enum(parser->inserted_routes, &i))) {
		if (com->RouteID == ID) return 1;
	}
	return 0;
}
static u32 xmt_get_next_route_id(GF_XMTParser *parser)
{
	u32 ID;
	GF_SceneGraph *sc = parser->load->scene_graph;
	if (parser->parsing_proto) sc = gf_sg_proto_get_graph(parser->parsing_proto);

	ID = gf_sg_get_next_available_route_id(sc);
	if (parser->load->ctx && (ID>parser->load->ctx->max_route_id))
		parser->load->ctx->max_route_id = ID;
	return ID;
}

static void xmt_resolve_routes(GF_XMTParser *parser)
{
	GF_Command *com;
	/*resolve all commands*/
	while (1) {
		com = (GF_Command *)gf_list_last(parser->unresolved_routes);
		if (!com) break;
		gf_list_rem_last(parser->unresolved_routes);
		switch (com->tag) {
		case GF_SG_ROUTE_DELETE:
		case GF_SG_ROUTE_REPLACE:
			com->RouteID = xmt_get_route(parser, com->unres_name, 0);
			if (!com->RouteID) {
				xmt_report(parser, GF_BAD_PARAM, "Cannot resolve GF_Route DEF %s", com->unres_name);
			} else {
				com->unresolved = 0;
			}
			gf_free(com->unres_name);
			com->unres_name = NULL;
			break;
		}
	}
	while (gf_list_count(parser->inserted_routes)) gf_list_rem(parser->inserted_routes, 0);
}
static void xmt_parse_route(GF_XMTParser *parser, const GF_XMLAttribute *attributes, u32 nb_attributes, Bool is_insert, GF_Command *com)
{
	GF_Route *r;
	char *toN, *toNF, *fromN, *fromNF, *ID;
	GF_Node *orig, *dest;
	GF_Err e;
	u32 rID, i;
	GF_FieldInfo orig_field, dest_field;

	toN = toNF = fromN = fromNF = ID = NULL;

	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
		if (!att->value || !strlen(att->value)) continue;
		if (!strcmp(att->name, "fromNode")) fromN = att->value;
		else if (!strcmp(att->name, "fromField")) fromNF = att->value;
		else if (!strcmp(att->name, "toNode")) toN = att->value;
		else if (!strcmp(att->name, "toField")) toNF = att->value;
		else if (!strcmp(att->name, "DEF")) ID = att->value;
	}

	orig = xmt_find_node(parser, fromN);
	if (!orig) {
		xmt_report(parser, GF_BAD_PARAM, "ROUTE: Cannot find origin node %s", fromN);
		return;
	}
	e = gf_node_get_field_by_name(orig, fromNF, &orig_field);
	if ((e != GF_OK) && strstr(fromNF, "_changed")) {
		char *sz = strstr(fromNF, "_changed");
		sz[0] = 0;
		e = gf_node_get_field_by_name(orig, fromNF, &orig_field);
	}
	if (e!=GF_OK) {
		xmt_report(parser, GF_BAD_PARAM, "%s is not an attribute of node %s", fromNF, fromN);
		return;
	}
	dest = xmt_find_node(parser, toN);
	if (!dest) {
		xmt_report(parser, GF_BAD_PARAM, "ROUTE: Cannot find destination node %s", toN);
		return;
	}
	e = gf_node_get_field_by_name(dest, toNF, &dest_field);
	if ((e != GF_OK) && toNF && !strnicmp(toNF, "set_", 4)) e = gf_node_get_field_by_name(dest, &toNF[4], &dest_field);
	if (e != GF_OK) {
		xmt_report(parser, GF_BAD_PARAM, "%s is not an attribute of node %s", toNF, toN);
		return;
	}
	rID = 0;
	if (ID && strlen(ID)) {
		rID = xmt_get_route(parser, ID, 0);
		if (!rID && (ID[0]=='R') ) {
			rID = atoi(&ID[1]);
			if (rID) {
				rID++;
				if (xmt_route_id_used(parser, rID)) rID = 0;
			}
		}
		if (!rID) rID = xmt_get_next_route_id(parser);
	}
	if (com) {
		/*for insert command*/
		if (rID) {
			com->RouteID = rID;
			com->def_name = gf_strdup(ID);
			/*whenever not inserting in graph, keep track of max defined ID*/
			gf_sg_set_max_defined_route_id(parser->load->scene_graph, rID);
			if (rID>parser->load->ctx->max_route_id) parser->load->ctx->max_route_id = rID;

		}
		com->fromNodeID = gf_node_get_id(orig);
		com->fromFieldIndex = orig_field.fieldIndex;
		com->toNodeID = gf_node_get_id(dest);
		com->toFieldIndex = dest_field.fieldIndex;
		return;
	}
	r = gf_sg_route_new(parser->load->scene_graph, orig, orig_field.fieldIndex, dest, dest_field.fieldIndex);
	if (rID) {
		gf_sg_route_set_id(r, rID);
		gf_sg_route_set_name(r, ID);
	}
}


static void xmt_update_timenode(GF_XMTParser *parser, GF_Node *node)
{
	if (!(parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK)) return;

	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_AnimationStream:
		xmt_offset_time(parser, & ((M_AnimationStream*)node)->startTime);
		xmt_offset_time(parser, & ((M_AnimationStream*)node)->stopTime);
		break;
	case TAG_MPEG4_AudioBuffer:
		xmt_offset_time(parser, & ((M_AudioBuffer*)node)->startTime);
		xmt_offset_time(parser, & ((M_AudioBuffer*)node)->stopTime);
		break;
	case TAG_MPEG4_AudioClip:
		xmt_offset_time(parser, & ((M_AudioClip*)node)->startTime);
		xmt_offset_time(parser, & ((M_AudioClip*)node)->stopTime);
		break;
	case TAG_MPEG4_AudioSource:
		xmt_offset_time(parser, & ((M_AudioSource*)node)->startTime);
		xmt_offset_time(parser, & ((M_AudioSource*)node)->stopTime);
		break;
	case TAG_MPEG4_MovieTexture:
		xmt_offset_time(parser, & ((M_MovieTexture*)node)->startTime);
		xmt_offset_time(parser, & ((M_MovieTexture*)node)->stopTime);
		break;
	case TAG_MPEG4_TimeSensor:
		xmt_offset_time(parser, & ((M_TimeSensor*)node)->startTime);
		xmt_offset_time(parser, & ((M_TimeSensor*)node)->stopTime);
		break;
	case TAG_ProtoNode:
	{
		u32 i, nbFields;
		GF_FieldInfo inf;
		nbFields = gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_ALL);
		for (i=0; i<nbFields; i++) {
			gf_node_get_field(node, i, &inf);
			if (inf.fieldType != GF_SG_VRML_SFTIME) continue;
			xmt_check_time_offset(parser, node, &inf);
		}
	}
	break;
	}
}

static void xmt_strip_name(const char *in, char *out)
{
	while (in[0]==' ') in++;
	strcpy(out, in);
	while (out[strlen(out)-1] == ' ') out[strlen(out)-1] = 0;
}

static u32 xmt_get_ft_by_name(const char *_name)
{
	char name[1024];
	xmt_strip_name(_name, name);

	if (!strcmp(name, "Boolean") || !strcmp(name, "SFBool")) return GF_SG_VRML_SFBOOL;
	else if (!strcmp(name, "Integer") || !strcmp(name, "SFInt32")) return GF_SG_VRML_SFINT32;
	else if (!strcmp(name, "Color") || !strcmp(name, "SFColor")) return GF_SG_VRML_SFCOLOR;
	else if (!strcmp(name, "Vector2") || !strcmp(name, "SFVec2f")) return GF_SG_VRML_SFVEC2F;
	else if (!strcmp(name, "Image") || !strcmp(name, "SFImage")) return GF_SG_VRML_SFIMAGE;
	else if (!strcmp(name, "Time") || !strcmp(name, "SFTime")) return GF_SG_VRML_SFTIME;
	else if (!strcmp(name, "Float") || !strcmp(name, "SFFloat")) return GF_SG_VRML_SFFLOAT;
	else if (!strcmp(name, "Vector3") || !strcmp(name, "SFVec3f")) return GF_SG_VRML_SFVEC3F;
	else if (!strcmp(name, "Rotation") || !strcmp(name, "SFRotation")) return GF_SG_VRML_SFROTATION;
	else if (!strcmp(name, "String") || !strcmp(name, "SFString")) return GF_SG_VRML_SFSTRING;
	else if (!strcmp(name, "Node") || !strcmp(name, "SFNode")) return GF_SG_VRML_SFNODE;
	else if (!strcmp(name, "Booleans") || !strcmp(name, "MFBool")) return GF_SG_VRML_MFBOOL;
	else if (!strcmp(name, "Integers") || !strcmp(name, "MFInt32")) return GF_SG_VRML_MFINT32;
	else if (!strcmp(name, "Colors") || !strcmp(name, "MFColor")) return GF_SG_VRML_MFCOLOR;
	else if (!strcmp(name, "Vector2s") || !strcmp(name, "Vector2Array") || !strcmp(name, "MFVec2f")) return GF_SG_VRML_MFVEC2F;
	else if (!strcmp(name, "Images") || !strcmp(name, "MFImage")) return GF_SG_VRML_MFIMAGE;
	else if (!strcmp(name, "Times") || !strcmp(name, "MFTime")) return GF_SG_VRML_MFTIME;
	else if (!strcmp(name, "Floats") || !strcmp(name, "MFFloat")) return GF_SG_VRML_MFFLOAT;
	else if (!strcmp(name, "Vector3s") || !strcmp(name, "Vector3Array") || !strcmp(name, "MFVec3f")) return GF_SG_VRML_MFVEC3F;
	else if (!strcmp(name, "Rotations") || !strcmp(name, "MFRotation")) return GF_SG_VRML_MFROTATION;
	else if (!strcmp(name, "Strings") || !strcmp(name, "MFString")) return GF_SG_VRML_MFSTRING;
	else if (!strcmp(name, "Nodes") || !strcmp(name, "MFNode")) return GF_SG_VRML_MFNODE;

	else if (!strcmp(name, "SFColorRGBA")) return GF_SG_VRML_SFCOLORRGBA;
	else if (!strcmp(name, "MFColorRGBA")) return GF_SG_VRML_MFCOLORRGBA;
	else if (!strcmp(name, "SFDouble")) return GF_SG_VRML_SFDOUBLE;
	else if (!strcmp(name, "MFDouble")) return GF_SG_VRML_MFDOUBLE;
	else if (!strcmp(name, "SFVec3d")) return GF_SG_VRML_SFVEC3D;
	else if (!strcmp(name, "MFVec3d")) return GF_SG_VRML_MFVEC3D;
	else if (!strcmp(name, "SFVec2d")) return GF_SG_VRML_SFVEC2D;
	else if (!strcmp(name, "MFVec2d")) return GF_SG_VRML_MFVEC2D;
	else return GF_SG_VRML_UNKNOWN;
}
static u32 xmt_get_script_et_by_name(const char *_name)
{
	char name[1024];
	xmt_strip_name((char *)_name, name);

	if (!strcmp(name, "eventIn") || !strcmp(name, "inputOnly") ) return GF_SG_SCRIPT_TYPE_EVENT_IN;
	else if (!strcmp(name, "eventOut") || !strcmp(name, "outputOnly")) return GF_SG_SCRIPT_TYPE_EVENT_OUT;
	else if (!strcmp(name, "field") || !strcmp(name, "initializeOnly") ) return GF_SG_SCRIPT_TYPE_FIELD;
	else return GF_SG_EVENT_UNKNOWN;
}
static u32 xmt_get_et_by_name(const char *_name)
{
	char name[1024];
	xmt_strip_name((char *)_name, name);

	if (!strcmp(name, "eventIn") || !strcmp(name, "inputOnly") ) return GF_SG_EVENT_IN;
	else if (!strcmp(name, "eventOut") || !strcmp(name, "outputOnly")) return GF_SG_EVENT_OUT;
	else if (!strcmp(name, "field") || !strcmp(name, "initializeOnly") ) return GF_SG_EVENT_FIELD;
	else if (!strcmp(name, "exposedField") || !strcmp(name, "inputOutput")) return GF_SG_EVENT_EXPOSED_FIELD;
	else return GF_SG_EVENT_UNKNOWN;
}

static void xmt_parse_script_field(GF_XMTParser *parser, GF_Node *node, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	GF_ScriptField *scfield;
	GF_FieldInfo field;
	char *val = NULL;
	u32 fieldType, eventType, i;
	char *fieldName = NULL;
	fieldType = eventType = 0;
	val = NULL;
	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
		if (!att->value || !strlen(att->value)) continue;
		if (!strcmp(att->name, "name")) fieldName = att->value;
		else if (!strcmp(att->name, "type")) fieldType = xmt_get_ft_by_name(att->value);
		else if (!strcmp(att->name, "vrml97Hint") || !strcmp(att->name, "accessType")) eventType = xmt_get_script_et_by_name(att->value);
		else if (strstr(att->name, "value") || strstr(att->name, "Value")) val = att->value;
	}
	scfield = gf_sg_script_field_new(node, eventType, fieldType, fieldName);

	if (!scfield) {
		xmt_report(parser, GF_BAD_PARAM, "Cannot create script field - please check syntax");
		return;
	}
	if (val) {
		gf_node_get_field_by_name(node, fieldName, &field);
		if (gf_sg_vrml_is_sf_field(fieldType)) {
			xmt_parse_sf_field(parser, &field, node, val);
		} else {
			xmt_parse_mf_field(parser, &field, node, val);
		}
	}
}

static u32 xmt_get_next_proto_id(GF_XMTParser *parser)
{
	u32 ID;
	GF_SceneGraph *sc = parser->load->scene_graph;
	if (parser->parsing_proto) sc = gf_sg_proto_get_graph(parser->parsing_proto);
	ID = gf_sg_get_next_available_proto_id(sc);
	if (parser->load->ctx && (ID>parser->load->ctx->max_node_id))
		parser->load->ctx->max_proto_id = ID;
	return ID;
}

static void xmt_parse_proto(GF_XMTParser *parser, const GF_XMLAttribute *attributes, u32 nb_attributes, GF_List *proto_list)
{
	GF_FieldInfo info;
	GF_Proto *proto;
	char *szName, *extURL;
	u32 ID, i;

	szName = extURL = NULL;
	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
		if (!att->value || !strlen(att->value)) continue;
		if (!strcmp(att->name, "name")) szName = att->value;
//		else if (!strcmp(att->name, "protoID")) ID = atoi(att->value);
		else if (!strcmp(att->name, "locations")) extURL = att->value;
		else if (!strcmp(att->name, "url")) extURL = att->value;
	}

	ID = xmt_get_next_proto_id(parser);
	proto = gf_sg_proto_new(parser->load->scene_graph, ID, szName, proto_list ? 1 : 0);
	if (proto_list) gf_list_add(proto_list, proto);
	if (parser->load->ctx && (parser->load->ctx->max_proto_id<ID)) parser->load->ctx->max_proto_id=ID;

	/*store previous proto*/
	proto->userpriv = parser->parsing_proto;
	parser->parsing_proto = proto;
	parser->load->scene_graph = gf_sg_proto_get_graph(proto);

	if (extURL) {
		info.fieldType = GF_SG_VRML_MFURL;
		info.far_ptr = &proto->ExternProto;
		info.name = "ExternURL";
		xmt_parse_mf_field(parser, &info, NULL, extURL);
	}
}
static u32 xmt_get_protofield_qp_type(const char *QP_Type)
{
#ifndef GPAC_DISABLE_BIFS
	if (!strcmp(QP_Type, "position3D")) return QC_3DPOS;
	else if (!strcmp(QP_Type, "position2D")) return QC_2DPOS;
	else if (!strcmp(QP_Type, "drawingOrder")) return QC_ORDER;
	else if (!strcmp(QP_Type, "color")) return QC_COLOR;
	else if (!strcmp(QP_Type, "textureCoordinate")) return QC_TEXTURE_COORD;
	else if (!strcmp(QP_Type, "angle")) return QC_ANGLE;
	else if (!strcmp(QP_Type, "scale")) return QC_SCALE;
	else if (!strcmp(QP_Type, "keys")) return QC_INTERPOL_KEYS;
	else if (!strcmp(QP_Type, "normals")) return QC_NORMALS;
	else if (!strcmp(QP_Type, "rotations")) return QC_ROTATION;
	else if (!strcmp(QP_Type, "size3D")) return QC_SIZE_3D;
	else if (!strcmp(QP_Type, "size2D")) return QC_SIZE_2D;
	else if (!strcmp(QP_Type, "linear")) return QC_LINEAR_SCALAR;
	else if (!strcmp(QP_Type, "coordIndex")) return QC_COORD_INDEX;
#endif
	return 0;
}

static GF_Err x3d_get_default_container(GF_Node *par, GF_Node *n, GF_FieldInfo *info)
{
	u32 i, count;
	count = gf_node_get_field_count(par);
	/*get the first field/exposedField accepting this child*/
	for (i=0; i<count; i++) {
		gf_node_get_field(par, i, info);
		if ((info->fieldType!=GF_SG_VRML_SFNODE) && (info->fieldType!=GF_SG_VRML_MFNODE)) continue;
		if ((info->eventType==GF_SG_EVENT_OUT) || (info->eventType==GF_SG_EVENT_IN)) continue;
		if (gf_node_in_table(n, info->NDTtype)) return GF_OK;
	}
	return GF_BAD_PARAM;
}


static GF_Node *xmt_parse_element(GF_XMTParser *parser, char *name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes, XMTNodeStack *parent)
{
	GF_Err e;
	GF_FieldInfo info;
	u32	tag, i, ID;
	Bool register_def = 0;
	Bool is_script = 0;
	GF_Node *node;
	GF_FieldInfo container;
	char *def_name;
	GF_Proto *proto = NULL;

	node = NULL;
	if (!strcmp(name, "NULL")) return NULL;
	if (!strcmp(name, "ROUTE")) {
		if (!parser->parsing_proto && (parser->doc_type==1) ) {
			GF_Command *sgcom = gf_sg_command_new(parser->load->scene_graph, GF_SG_ROUTE_INSERT);
			gf_list_add(parser->scene_au->commands, sgcom);
			xmt_parse_route(parser, attributes, nb_attributes, 0, sgcom);
			if (sgcom->RouteID) gf_list_add(parser->inserted_routes, sgcom);
		} else {
			xmt_parse_route(parser, attributes, nb_attributes, 0, NULL);
		}
		return NULL;
	}
	if (parent && parent->node && ((parent->node->sgprivate->tag==TAG_MPEG4_Script)
#ifndef GPAC_DISABLE_X3D
	                               || (parent->node->sgprivate->tag==TAG_X3D_Script)
#endif
	                              ) ) {
		is_script = 1;
		if (!strcmp(name, "field")) {
			xmt_parse_script_field(parser, parent->node, attributes, nb_attributes);
			return NULL;
		}
		else if (!strcmp(name, "node") || !strcmp(name, "nodes") ) return NULL;
	}

	/*proto declaration*/
	if (!strcmp(name, "ProtoDeclare") || !strcmp(name, "ExternProtoDeclare")) {
		if (!parser->parsing_proto && parser->command && !parser->command->new_proto_list) parser->command->new_proto_list = gf_list_new();
		xmt_parse_proto(parser, attributes, nb_attributes, (!parser->parsing_proto && parser->command) ? parser->command->new_proto_list : NULL);
		return NULL;
	}
	/*proto parsing*/
	if (parser->parsing_proto) {
		if (!strcmp(name, "IS"))
			return NULL;

		if (!strcmp(name, "field")) {
			char *fieldName = NULL;
			char *value = NULL;
			u32 fType, eType;
			fType = eType = 0;

			for (i=0; i<nb_attributes; i++) {
				GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
				if (!att->value || !strlen(att->value)) continue;

				if (!strcmp(att->name, "name")) fieldName  = att->value;
				else if (!strcmp(att->name, "type")) fType = xmt_get_ft_by_name(att->value);
				else if (!strcmp(att->name, "vrml97Hint") || !strcmp(att->name, "accessType") ) eType = xmt_get_et_by_name(att->value);
				else if (strstr(att->name, "value") || strstr(att->name, "Value")) value = att->value;
			}
			parser->proto_field = gf_sg_proto_field_new(parser->parsing_proto, fType, eType, fieldName);
			if (value && strlen(value)) {
				gf_sg_proto_field_get_field(parser->proto_field, &info);
				if (gf_sg_vrml_is_sf_field(fType)) {
					xmt_parse_sf_field(parser, &info, NULL, value);
				} else {
					xmt_parse_mf_field(parser, &info, NULL, value);
				}
			} else if (gf_sg_vrml_get_sf_type(fType) != GF_SG_VRML_SFNODE) {
				/*value not specified for exter proto*/
			}
			/*SF/MFNode proto field: push node stack with container info but no parent*/
			else {
				XMTNodeStack *pf_stack;
				GF_SAFEALLOC(pf_stack, XMTNodeStack);
				gf_sg_proto_field_get_field(parser->proto_field, &pf_stack->container_field);
				gf_list_add(parser->nodes, pf_stack);
			}
			return NULL;
		}

		/*X3D style*/
		if (!strcmp(name, "ProtoInterface") || !strcmp(name, "ProtoBody"))
			return NULL;
		/*XMT1 decl for SFNode proto fields*/
		if (parser->proto_field && (!strcmp(name, "node") || !strcmp(name, "nodes")) )
			return NULL;
		/*anim & QP info */
		if (parser->proto_field  && !strcmp(name, "InterfaceCodingParameters")) {
			u32 qp_type, nbBits, hasMinMax, qp_sftype;
			Fixed ftMin, ftMax;
			ftMin = ftMax = 0;
			qp_type = hasMinMax = nbBits = 0;
			for (i=0; i<nb_attributes; i++) {
				GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
				if (!att->value || !strlen(att->value)) continue;
				if (!strcmp(att->name, "quantCategory")) qp_type = xmt_get_protofield_qp_type(att->value);
				else if (!strcmp(att->name, "nbBits")) nbBits = atoi(att->value);
				else if (!strncmp(att->name, "position3DM", 11) || !strncmp(att->name, "position2DM", 11)
				         || !strncmp(att->name, "drawOrderM", 10) || !strncmp(att->name, "colorM", 6)
				         || !strncmp(att->name, "textureCoordinateM", 18) || !strncmp(att->name, "angleM", 6)
				         || !strncmp(att->name, "scaleM", 6) || !strncmp(att->name, "keyM", 4) || !strncmp(att->name, "sizeM", 5)
				        ) {
					hasMinMax = 1;
					if (strstr(att->name, "Min")) xmt_parse_float(parser, att->name, &ftMin, att->value);
					else xmt_parse_float(parser, att->name, &ftMax, att->value);
				}
			}
			if (gf_sg_vrml_get_sf_type(parser->proto_field->FieldType) == GF_SG_VRML_SFINT32) {
				qp_sftype = GF_SG_VRML_SFINT32;
			} else {
				qp_sftype = GF_SG_VRML_SFFLOAT;
			}
			gf_bifs_proto_field_set_aq_info(parser->proto_field, qp_type, hasMinMax, qp_sftype, &ftMin, &ftMax, nbBits);
			return NULL;
		}
		/*connect */
		if (!strcmp(name, "connect")) {
			GF_ProtoFieldInterface *pf;
			Bool is_script = 0;
			GF_FieldInfo pfield, nfield;
			char *atField, *atProtoField;
			XMTNodeStack *last = (XMTNodeStack*)gf_list_last(parser->nodes);
			if (!last) {
				xmt_report(parser, GF_OK, "connect: no parent node specified - skipping");
				return NULL;
			}
			atField = atProtoField = NULL;
			for (i=0; i<nb_attributes; i++) {
				GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
				if (!att->value || !strlen(att->value)) continue;
				if (!strcmp(att->name, "nodeField")) atField = att->value;
				else if (!strcmp(att->name, "protoField")) atProtoField = att->value;
			}
			if (!atField) {
				xmt_report(parser, GF_OK, "connect: Missing node field - skipping");
				return NULL;
			}
			if (!atProtoField) {
				xmt_report(parser, GF_OK, "connect: Missing proto field - skipping");
				return NULL;
			}
			if ( (e = gf_node_get_field_by_name(last->node, atField, &nfield)) != GF_OK) {
				u32 l_tag = gf_node_get_tag(last->node);
				if ((l_tag!=TAG_MPEG4_Script)
#ifndef GPAC_DISABLE_X3D
				        && (l_tag!=TAG_X3D_Script)
#endif
				   ) {
					xmt_report(parser, e, "connect: %s not an field of node %s", atField, gf_node_get_class_name(last->node) );
					return NULL;
				}
				is_script = 1;
			}
			pf = gf_sg_proto_field_find_by_name(parser->parsing_proto, atProtoField);
			if (!pf) {
				xmt_report(parser, GF_BAD_PARAM, "connect: Proto field %s is not defined", atProtoField);
				return NULL;
			}
			gf_sg_proto_field_get_field(pf, &pfield);
			if (is_script) {
				gf_sg_script_field_new(last->node, pfield.eventType, pfield.fieldType, atField);
				gf_node_get_field_by_name(last->node, atField, &nfield);
			}
			e = gf_sg_proto_field_set_ised(parser->parsing_proto, pfield.fieldIndex, last->node, nfield.fieldIndex);
			if (e) xmt_report(parser, GF_BAD_PARAM, "connect: %s", gf_error_to_string(e));
			return NULL;
		}
	}
	/*proto instance field*/
	if (!strcmp(name, "fieldValue")) {
		char *field, *value;
		if (!parent || (parent->node->sgprivate->tag != TAG_ProtoNode)) {
			xmt_report(parser, GF_OK, "Warning: fieldValue not a valid node");
			return NULL;
		}
		field = value = NULL;
		for (i=0; i<nb_attributes; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
			if (!att->value || !strlen(att->value)) continue;
			if (!strcmp(att->name, "name")) field = att->value;
			else if (!strstr(att->name, "Value") || !strstr(att->name, "value")) value = att->value;
		}
		if (!field) {
			xmt_report(parser, GF_OK, "Warning: unspecified proto field name - skipping");
			return NULL;
		}
		e = gf_node_get_field_by_name(parent->node, field, &info);
		if (e) {
			xmt_report(parser, GF_OK, "Warning: Unknown proto field %s - skipping", field);
			return NULL;
		}
		if (value) {
			if (gf_sg_vrml_is_sf_field(info.fieldType)) {
				xmt_parse_sf_field(parser, &info, parent->node, value);
			} else {
				xmt_parse_mf_field(parser, &info, parent->node, value);
			}
			gf_sg_proto_mark_field_loaded(parent->node, &info);
		} else if (gf_sg_vrml_get_sf_type(info.fieldType) == GF_SG_VRML_SFNODE) {
			parent->container_field = info;
			parent->last = NULL;
		}
		return NULL;
	}
	if (parent && parent->node && (parent->node->sgprivate->tag == TAG_ProtoNode) && (!strcmp(name, "node") || !strcmp(name, "nodes")) ) {
		return NULL;
	}

	ID = 0;
	def_name = NULL;
	tag = 0;

	if (!strcmp(name, "ProtoInstance")) {
		char *proto_name = NULL;
		char *proto_use = NULL;
		node = NULL;
		for (i=0; i<nb_attributes; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
			if (!att->value || !strlen(att->value)) continue;
			if (!strcmp(att->name, "name")) {
				proto_name = att->value;
				att->value = NULL;
			} else if (!strcmp(att->name, "USE")) {
				proto_use = att->value;
			}
		}
		if (proto_use) {
			node = xmt_find_node(parser, proto_use);
			e = GF_OK;
			if (!node)
				e = xmt_report(parser, GF_BAD_PARAM, "Warning: Cannot find node %s referenced in USE - skipping", proto_use);

			if (e) return NULL;
			ID = 0;
			register_def = 0;
			tag = 0;
		} else if (proto_name) {
			GF_SceneGraph *sg = parser->load->scene_graph;
			while (1) {
				proto = gf_sg_find_proto(sg, 0, proto_name);
				if (proto) break;
				sg = sg->parent_scene;
				if (!sg) break;
			}
			if (!proto) {
				xmt_report(parser, GF_BAD_PARAM, "%s: not a valid/supported proto", proto_name);
				return NULL;
			}
			node = gf_sg_proto_create_instance(parser->load->scene_graph, proto);
		}
	} else {
		tag = xmt_get_node_tag(parser, name);

		if (!tag) {
			/*XMT-A weird syntax*/
			if (parent) {
				if (gf_node_get_field_by_name(parent->node, name, &parent->container_field)==GF_OK) {
					parent->last = NULL;
					if (parent->container_field.fieldType==GF_SG_VRML_SFCOMMANDBUFFER) {
						parser->command_buffer = (SFCommandBuffer*)parent->container_field.far_ptr;
						/*store command*/
						parser->command_buffer->buffer = (unsigned char *)parser->command;
						parser->state = XMT_STATE_COMMANDS;
					}
					return NULL;
				}
				if (!strcmp(name, "store") && (parent->container_field.fieldType==GF_SG_VRML_MFATTRREF)) {
					GF_FieldInfo pinfo;
					GF_Node *atNode = NULL;
					char *fieldName = NULL;
					for (i=0; i<nb_attributes; i++) {
						GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
						if (!att->value || !strlen(att->value)) continue;
						if (!strcmp(att->name, "node")) {
							atNode = xmt_find_node(parser, att->value);
							if (!atNode) xmt_report(parser, GF_BAD_PARAM, "Cannot locate node %s", att->value);
						}
						if (!strcmp(att->name, "field")) fieldName = att->value;
					}
					if (!fieldName || !atNode) {
						xmt_report(parser, GF_BAD_PARAM, "Node or field name missing in <store>");
					} else if (gf_node_get_field_by_name(atNode, fieldName, &pinfo) != GF_OK) {
						xmt_report(parser, GF_BAD_PARAM, "Field %s not a member of node %s", fieldName, gf_node_get_class_name(node) );
					} else {
						SFAttrRef *ptr;
						gf_sg_vrml_mf_append(parent->container_field.far_ptr , GF_SG_VRML_MFATTRREF, (void **) &ptr);
						ptr->node = atNode;
						ptr->fieldIndex = pinfo.fieldIndex;
					}
					return NULL;
				}
				parent->container_field.far_ptr = NULL;
			}
			else if (parser->command && (parser->command->tag == GF_SG_MULTIPLE_REPLACE)) {
				if (gf_node_get_field_by_name(parser->command->node, name, &container)==GF_OK) {
					GF_CommandField *field = gf_sg_command_field_new(parser->command);
					field->fieldIndex = container.fieldIndex;
					field->fieldType = container.fieldType;
					return NULL;
				}
			}
			xmt_report(parser, GF_OK, "Warning: %s is not a valid node - skipping", name);
			return NULL;
		}
		node = gf_node_new(parser->load->scene_graph, tag);
		if (!node) {
			xmt_report(parser, GF_SG_UNKNOWN_NODE, "Warning: %s is not a supported node - skipping", name);
			return NULL;
		}
	}

	parser->current_node_tag = tag;

	if (parent) container = parent->container_field;
	else {
		container.far_ptr = NULL;
		container.fieldIndex = 0;
		container.fieldType = 0;
	}

	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
		if (!att->value || !strlen(att->value)) continue;

		if (!strcmp(att->name, "DEF")) {
			GF_Node *undef_node = gf_sg_find_node_by_name(parser->load->scene_graph, att->value);
			register_def = 1;
			if (undef_node) {
				gf_list_del_item(parser->peeked_nodes, undef_node);
				/*if we see twice a DEF N1 then force creation of a new node*/
				if (xmt_has_been_def(parser, att->value)) {
					ID = xmt_get_node_id(parser, att->value);
					xmt_report(parser, GF_OK, "Warning: Node %s has been defined several times - IDs may get corrupted", att->value);
				} else {
					gf_node_register(node, NULL);
					gf_node_unregister(node, NULL);
					node = undef_node;
					ID = 0;
				}
			} else {
				ID = xmt_get_node_id(parser, att->value);

			}
			def_name = att->value;
		}
		/*USE node*/
		else if (!strcmp(att->name, "USE")) {
			GF_Err e;
			GF_Node *def_node;

			def_node = xmt_find_node(parser, att->value);

			e = GF_OK;
			if (!def_node)
				e = xmt_report(parser, GF_BAD_PARAM, "Warning: Cannot find node %s referenced in USE - skipping", att->value);
			else if (tag != gf_node_get_tag(def_node)) {
				xmt_report(parser, GF_OK, "Warning: Node type %s doesn't match type %s of node %s", gf_node_get_class_name(node), gf_node_get_class_name(def_node), att->value);
			}

			/*DESTROY NODE*/
			gf_node_register(node, NULL);
			gf_node_unregister(node, NULL);

			if (e) return NULL;

			node = def_node;
			ID = 0;
			register_def = 0;
			tag = 0;
			break;
		}
		/*X3D stuff*/
		else if (!strcmp(att->name, "containerField")) {
			if (parent) {
				if (gf_node_get_field_by_name(parent->node, att->value, &container) != GF_OK) {
					xmt_report(parser, GF_BAD_PARAM, "Warning: Container field %s not member of node %s", att->value, name);
					container.far_ptr = NULL;
				}
			}
		}
		/*ignored ones*/
		else if (!strcmp(att->name, "bboxCenter") || !strcmp(att->name, "bboxSize")) {
		}
		/*all other fields*/
		else {
			e = gf_node_get_field_by_name(node, att->name, &info);
			if (e) xmt_report(parser, GF_OK, "Warning: Unknown field \"%s\" for node %s - skipping", att->name, name);
			else if (gf_sg_vrml_is_sf_field(info.fieldType)) {
				xmt_parse_sf_field(parser, &info, node, att->value);
			} else {
				xmt_parse_mf_field(parser, &info, node, att->value);
			}
		}
	}

	if (!parser->parsing_proto) xmt_update_timenode(parser, node);

	if (register_def) gf_list_add(parser->def_nodes, node);
	if (ID) gf_node_set_id(node, ID, def_name);

	if (is_script) {
		u32 last_field = gf_node_get_field_count(parent->node);
		gf_node_get_field(parent->node, last_field-1, &container);
	}

	if (parent) {
		if (!container.far_ptr) {
			if (parser->doc_type==2) {
				x3d_get_default_container(parent->node, node, &container);
				parent->last = NULL;
			}
			if (!container.far_ptr) {
				gf_node_get_field_by_name(parent->node, "children", &container);
				parent->last = NULL;
			}

		}
		if (container.fieldType == GF_SG_VRML_SFNODE) {
			if (* ((GF_Node **)container.far_ptr) ) gf_node_unregister(* ((GF_Node **)container.far_ptr) , parent->node);
			* ((GF_Node **)container.far_ptr) = node;
			gf_node_register(node, parent->node);
			parent->container_field.far_ptr = NULL;
			parent->last = NULL;
		} else if (container.fieldType == GF_SG_VRML_MFNODE) {
			gf_node_list_add_child_last( (GF_ChildNodeItem **)container.far_ptr, node, &parent->last);
			gf_node_register(node, parent->node);
		}
		assert(parent->node);
		gf_node_changed(parent->node, NULL);
	}

	if (!parser->parsing_proto && (tag || proto) )
		gf_node_init(node);

	/*For Ivica: load proto as soon as found when in playback mode*/
	if ( (parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) && proto && !parser->parsing_proto) {
		parser->last_error = gf_sg_proto_load_code(node);
	}
	return node;
}


GF_Descriptor *xmt_parse_descriptor(GF_XMTParser *parser, char *name, const GF_XMLAttribute *attributes, u32 nb_attributes, GF_Descriptor *parent)
{
	GF_Err e;
	u32 i;
	u32 fake_desc = 0;
	GF_Descriptor *desc;
	char *xmt_desc_name = NULL, *ocr_ref = NULL, *dep_ref = NULL;
	u32 binaryID = 0;
	u8 tag = gf_odf_get_tag_by_name(name);

	if (!tag) {
		if (!parent) return NULL;
		switch (parent->tag) {
		case GF_ODF_IOD_TAG:
		case GF_ODF_OD_TAG:
			if (!strcmp(name, "Profiles")) fake_desc = 1;
			else if (!strcmp(name, "Descr")) fake_desc = 1;
			else if (!strcmp(name, "esDescr")) fake_desc = 1;
			else if (!strcmp(name, "URL")) fake_desc = 1;
			else return NULL;
			break;
		case GF_ODF_ESD_TAG:
			if (!strcmp(name, "decConfigDescr")) fake_desc = 1;
			else if (!strcmp(name, "slConfigDescr")) fake_desc = 1;
			else return NULL;
			break;
		case GF_ODF_DCD_TAG:
			if (!strcmp(name, "decSpecificInfo")) fake_desc = 1;
			else return NULL;
			break;
		case GF_ODF_SLC_TAG:
			if (!strcmp(name, "custom")) fake_desc = 1;
			else return NULL;
			break;
		case GF_ODF_MUXINFO_TAG:
			if (!strcmp(name, "MP4MuxHints")) fake_desc = 1;
			else return NULL;
			break;
		case GF_ODF_BIFS_CFG_TAG:
			if (!strcmp(name, "commandStream")) fake_desc = 1;
			else if (!strcmp(name, "size")) fake_desc = 2;
			else return NULL;
			break;
		default:
			return NULL;
		}
	}
	if (fake_desc) {
		tag = parent->tag;
		desc = parent;
	} else {
		desc = gf_odf_desc_new(tag);
		if (!desc) return NULL;
	}

	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
		if (!att->value || !strlen(att->value)) continue;
		if (!strcmp(att->name, "binaryID")) binaryID = atoi(att->value);
		else if (!stricmp(att->name, "objectDescriptorID")) xmt_desc_name = att->value;
		else if (!strcmp(att->name, "ES_ID")) xmt_desc_name = att->value;
		else if (!strcmp(att->name, "OCR_ES_ID")) ocr_ref = att->value;
		else if (!strcmp(att->name, "dependsOn_ES_ID")) dep_ref = att->value;
		else {
			e = gf_odf_set_field(desc, att->name, att->value);
			if (e) xmt_report(parser, e, "Warning: %s not a valid attribute for descriptor %s", att->name, name);
			//store src path but do not concatenate, othewise we break BT<->XMT conversion ...
			if ((desc->tag==GF_ODF_MUXINFO_TAG) && (!stricmp(att->name, "fileName") || !stricmp(att->name, "url"))) {
				GF_MuxInfo *mux = (GF_MuxInfo *) desc;
				if (!mux->src_url)
					mux->src_url = gf_strdup(parser->load->src_url ? parser->load->src_url : parser->load->fileName);
			}
		}
	}
	if (binaryID || xmt_desc_name) {
		if ((tag == GF_ODF_IOD_TAG) || (tag == GF_ODF_OD_TAG))
			xmt_new_od_link(parser, (GF_ObjectDescriptor *)desc, xmt_desc_name, binaryID);
		else if (tag == GF_ODF_ESD_TAG) {
			xmt_new_esd_link(parser, (GF_ESD *) desc, xmt_desc_name, binaryID);

			/*set references once the esd link has been established*/
			if (ocr_ref) xmt_set_depend_id(parser, (GF_ESD *) desc, ocr_ref, 1);
			if (dep_ref) xmt_set_depend_id(parser, (GF_ESD *) desc, dep_ref, 0);
		}
	}

	if (fake_desc) {
		if (fake_desc==2) {
			GF_BIFSConfig *bcfg = (GF_BIFSConfig *)desc;
			parser->load->ctx->scene_width = bcfg->pixelWidth;
			parser->load->ctx->scene_height = bcfg->pixelHeight;
			parser->load->ctx->is_pixel_metrics = bcfg->pixelMetrics;
		}
		return NULL;
	}
	if (parent) {
		e = gf_odf_desc_add_desc(parent, desc);
		if (e) {
			xmt_report(parser, GF_OK, "Invalid child descriptor");
			gf_odf_desc_del(desc);
			return NULL;
		}
		/*finally check for scene manager streams (scene description, OD, ...)*/
		if (parent->tag == GF_ODF_ESD_TAG) {
			GF_ESD *esd = (GF_ESD *)parent;
			if (esd->decoderConfig) {
				switch (esd->decoderConfig->streamType) {
				case GF_STREAM_SCENE:
				case GF_STREAM_OD:
					if (!esd->decoderConfig->objectTypeIndication) esd->decoderConfig->objectTypeIndication = 1;
					/*watchout for default BIFS stream*/
					if (parser->scene_es && !parser->base_scene_id && (esd->decoderConfig->streamType==GF_STREAM_SCENE)) {
						parser->scene_es->ESID = parser->base_scene_id = esd->ESID;
						parser->scene_es->timeScale = (esd->slConfig && esd->slConfig->timestampResolution) ? esd->slConfig->timestampResolution : 1000;
					} else {
						char *name;
						GF_StreamContext *sc = gf_sm_stream_new(parser->load->ctx, esd->ESID, esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
						/*set default timescale for systems tracks (ignored for other)*/
						if (sc) sc->timeScale = (esd->slConfig && esd->slConfig->timestampResolution) ? esd->slConfig->timestampResolution : 1000;
						if (!parser->base_scene_id && (esd->decoderConfig->streamType==GF_STREAM_SCENE)) parser->base_scene_id = esd->ESID;
						else if (!parser->base_od_id && (esd->decoderConfig->streamType==GF_STREAM_OD)) parser->base_od_id = esd->ESID;

						name = xmt_get_es_name(parser, esd->ESID);
						if (sc && name && !sc->name) sc->name = gf_strdup(name);
					}
					break;
				}
			}
		}
	}
	return desc;
}

static void xmt_parse_command(GF_XMTParser *parser, const char *name, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	GF_Err e;
	GF_FieldInfo info;
	GF_CommandField *field;
	u32 i;
	if (!strcmp(name, "Scene")) {
		parser->state = XMT_STATE_ELEMENTS;
		return;
	}
	if (!parser->in_com)
		parser->stream_id = parser->load->force_es_id;

	if (!strcmp(name, "par")) {
		parser->in_com = 1;
		for (i=0; i<nb_attributes; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
			if (!att->value || !strlen(att->value)) continue;
			if (!strcmp(att->name, "begin")) parser->au_time = atof(att->value);
			else if (!strcmp(att->name, "isRAP")) parser->au_is_rap = !strcmp(att->value, "yes") ? 1 : 0;
			else if (!strcmp(att->name, "atES_ID")) {
				parser->stream_id = xmt_locate_stream(parser, att->value);
				if (!parser->stream_id) xmt_report(parser, GF_OK, "Warning: Cannot locate command's target stream %s", att->value);
			}
		}
		return;
	}
	/*ROUTE insert/replace*/
	if (!strcmp(name, "ROUTE")) {
		if (!parser->command || ((parser->command->tag!=GF_SG_ROUTE_REPLACE) && (parser->command->tag!=GF_SG_ROUTE_INSERT))) {
			xmt_report(parser, GF_BAD_PARAM, "ROUTE declared outside command scope");
			return;
		}
		if (parser->command->tag==GF_SG_ROUTE_INSERT) {
			xmt_parse_route(parser, attributes, nb_attributes, 1, parser->command);
			gf_list_add(parser->inserted_routes, parser->command);
		} else {
			xmt_parse_route(parser, attributes, nb_attributes, 0, parser->command);
			if (!parser->command->RouteID) {
				parser->command->unresolved = 1;
				if (gf_list_find(parser->unresolved_routes, parser->command)<0)
					gf_list_add(parser->unresolved_routes, parser->command);
			}
		}
		return;
	}
	/*multiple replace*/
	if (!strcmp(name, "repField")) {
		char *fieldName = NULL;
		char *fieldValue = NULL;
		assert(parser->command);
		if (!parser->command->node) return;
		for (i=0; i<nb_attributes; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
			if (!att->value || !strlen(att->value)) continue;
			if (!strcmp(att->name, "atField")) fieldName = att->value;
			else if (!strcmp(att->name, "value")) fieldValue = att->value;
		}
		if (!fieldName) {
			parser->state = XMT_STATE_ELEMENTS;
			return;
		}
		e = gf_node_get_field_by_name(parser->command->node, fieldName, &info);
		if (e) {
			xmt_report(parser, GF_BAD_PARAM, "Warning: Field %s not a member of node %s ", fieldName, gf_node_get_class_name(parser->command->node) );
			return;
		}
		if (gf_sg_vrml_get_sf_type(info.fieldType) == GF_SG_VRML_SFNODE) {
			parser->state = XMT_STATE_ELEMENTS;
			return;
		}
		if (!fieldValue) return;

		field = gf_sg_command_field_new(parser->command);
		field->fieldIndex = info.fieldIndex;
		field->fieldType = info.fieldType;
		if (fieldValue) {
			field->field_ptr = gf_sg_vrml_field_pointer_new(info.fieldType);
			info.far_ptr = field->field_ptr;
			if (gf_sg_vrml_is_sf_field(info.fieldType)) {
				xmt_parse_sf_field(parser, &info, parser->command->node, fieldValue);
			} else {
				xmt_parse_mf_field(parser, &info, parser->command->node, fieldValue);
			}
		} else {
			parser->state = XMT_STATE_ELEMENTS;
		}
		return;
	}
	/*multiple index replace*/
	if (!strcmp(name, "repValue")) {
		s32 position = -1;
		char *fieldValue = NULL;
		assert(parser->command);
		if (!parser->command->node) return;
		for (i=0; i<nb_attributes; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
			if (!att->value || !strlen(att->value)) continue;
			if (!strcmp(att->name, "position")) {
				if (!strcmp(att->value, "BEGIN")) position = 0;
				else if (!strcmp(att->value, "END")) position = -1;
				else position = atoi(att->value);
			}
			else if (!strcmp(att->name, "value")) fieldValue = att->value;
		}
		gf_node_get_field(parser->command->node, parser->command->fromFieldIndex, &info);
		if (info.fieldType == GF_SG_VRML_MFNODE) {
			field = gf_sg_command_field_new(parser->command);
			field->fieldIndex = info.fieldIndex;
			field->fieldType = GF_SG_VRML_SFNODE;
			field->pos = position;
			parser->state = XMT_STATE_ELEMENTS;
		} else if (fieldValue) {
			field = gf_sg_command_field_new(parser->command);
			field->fieldIndex = info.fieldIndex;
			field->fieldType = info.fieldType = gf_sg_vrml_get_sf_type(info.fieldType);
			field->field_ptr = gf_sg_vrml_field_pointer_new(info.fieldType);
			field->pos = position;
			info.far_ptr = field->field_ptr;
			xmt_parse_sf_field(parser, &info, parser->command->node, fieldValue);
		}
		return;
	}


	/*BIFS command*/
	if (!strcmp(name, "Replace") || !strcmp(name, "Insert") || !strcmp(name, "Delete")) {
		GF_Node *atNode;
		u8 tag = GF_SG_UNDEFINED;
		u32 stream_id;
		Double au_time = parser->au_time;
		Bool au_is_rap = parser->au_is_rap;
		char *nodeName = NULL;
		char *fieldName = NULL;
		char *fieldValue = NULL;
		char *routeName = NULL;
		char *extended = NULL;
		char *idxNode = NULL;
		char *idxField = NULL;
		char *childField = NULL;
		char *fromNode = NULL;
		char *fromField = NULL;

		s32 position = -2;

		if (!parser->stream_id) parser->stream_id = parser->base_scene_id;
		stream_id = parser->stream_id;

		for (i=0; i<nb_attributes; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
			if (!att->value || !strlen(att->value)) continue;
			if (!strcmp(att->name, "begin")) au_time = atoi(att->value);
			else if (!strcmp(att->name, "isRAP")) au_is_rap = !strcmp(att->value, "yes") ? 1 : 0;
			else if (!strcmp(att->name, "atES_ID")) {
				stream_id = xmt_locate_stream(parser, att->value);
				if (!stream_id) {
					xmt_report(parser, GF_OK, "Warning: Cannot locate command's target stream %s", att->value);
					stream_id = parser->stream_id;
				}
			}
			else if (!strcmp(att->name, "atNode")) nodeName = att->value;
			else if (!strcmp(att->name, "atField")) fieldName = att->value;
			else if (!strcmp(att->name, "value")) fieldValue = att->value;
			else if (!strcmp(att->name, "atRoute")) routeName = att->value;
			else if (!strcmp(att->name, "extended")) extended = att->value;
			else if (!strcmp(att->name, "atIndexNode")) idxNode = att->value;
			else if (!strcmp(att->name, "atIndexField")) idxField = att->value;
			else if (!strcmp(att->name, "atChildField")) childField = att->value;
			else if (!strcmp(att->name, "fromNode")) fromNode = att->value;
			else if (!strcmp(att->name, "fromField")) fromField = att->value;
			else if (!strcmp(att->name, "position")) {
				if (!strcmp(att->value, "BEGIN")) position = 0;
				else if (!strcmp(att->value, "END")) position = -1;
				else position = atoi(att->value);
			}
		}

		/*if we are parsing in an already loaded context and no time is given, force a time != 0 to create a new command*/
		if (!au_time && (parser->load->flags&GF_SM_LOAD_CONTEXT_READY))
			au_time = 0.001;

		atNode = NULL;
		if (nodeName) {
			if (fieldName) {
				if (position>-2) {
					if (!strcmp(name, "Replace")) tag = GF_SG_INDEXED_REPLACE;
					else if (!strcmp(name, "Insert")) tag = GF_SG_INDEXED_INSERT;
					else if (!strcmp(name, "Delete")) tag = GF_SG_INDEXED_DELETE;
				} else {
					if (!strcmp(name, "Replace")) {
						if ((idxNode && idxField) || childField || (fromNode && fromField)) {
							tag = GF_SG_XREPLACE;
						} else {
							tag = GF_SG_FIELD_REPLACE;
						}
					}
				}
			} else {
				if (!strcmp(name, "Replace")) {
					tag = GF_SG_NODE_REPLACE;
					parser->state = XMT_STATE_ELEMENTS;
				}
				else if (!strcmp(name, "Insert")) {
					tag = GF_SG_NODE_INSERT;
					parser->state = XMT_STATE_ELEMENTS;
				}
				else if (!strcmp(name, "Delete")) tag = GF_SG_NODE_DELETE;
			}

			atNode = xmt_find_node(parser, nodeName);
			if (!atNode) {
				xmt_report(parser, GF_BAD_PARAM, "Warning: Cannot locate node %s for command %s", nodeName, name);
				return;
			}
			if (fieldName) {
				e = gf_node_get_field_by_name(atNode, fieldName, &info);
				if (e) {
					xmt_report(parser, GF_BAD_PARAM, "Warning: Field %s not a member of node %s ", fieldName, nodeName);
					return;
				}
			}
		}
		else if (routeName) {
			if (!strcmp(name, "Replace")) tag = GF_SG_ROUTE_REPLACE;
			else if (!strcmp(name, "Delete")) tag = GF_SG_ROUTE_DELETE;
		}
		else if (!strcmp(name, "Replace")) {
			tag = GF_SG_SCENE_REPLACE;
			au_is_rap = 1;
			gf_list_reset(parser->def_nodes);
		}
		else if (!strcmp(name, "Insert"))
			tag = GF_SG_ROUTE_INSERT;

		if (extended) {
			if (!strcmp(extended, "globalQuant")) {
				tag = GF_SG_GLOBAL_QUANTIZER;
				parser->state = XMT_STATE_ELEMENTS;
			}
			else if (!strcmp(extended, "fields")) {
				tag = GF_SG_MULTIPLE_REPLACE;
				parser->state = XMT_STATE_COMMANDS;
			}
			else if (!strcmp(extended, "indices")) {
				tag = GF_SG_MULTIPLE_INDEXED_REPLACE;
				parser->state = XMT_STATE_COMMANDS;
			}
			else if (!strcmp(extended, "deleteOrder")) tag = GF_SG_NODE_DELETE_EX;
			else if (!strcmp(extended, "allProtos")) tag = GF_SG_PROTO_DELETE_ALL;
			else if (!strcmp(extended, "proto") || !strcmp(extended, "protos")) {
				if (!strcmp(name, "Insert")) {
					parser->state = XMT_STATE_ELEMENTS;
					tag = GF_SG_PROTO_INSERT;
				}
				else if (!strcmp(name, "Delete")) tag = GF_SG_PROTO_DELETE;
			}
			else {
				xmt_report(parser, GF_BAD_PARAM, "Warning: Unknown extended command %s", extended);
				return;
			}

		}

		if (tag == GF_SG_UNDEFINED) {
			xmt_report(parser, GF_BAD_PARAM, "Warning: Unknown scene command %s", name);
			return;
		}

		parser->command = gf_sg_command_new(parser->load->scene_graph, tag);
		if (parser->command_buffer) {
			gf_list_add(parser->command_buffer->commandList, parser->command);
			parser->command_buffer->bufferSize++;
		} else {
			GF_StreamContext *stream = gf_sm_stream_find(parser->load->ctx, (u16) stream_id);
			if (!stream || (stream->streamType!=GF_STREAM_SCENE)) stream_id = parser->base_scene_id;

			parser->scene_es = gf_sm_stream_new(parser->load->ctx, (u16) stream_id, GF_STREAM_SCENE, GPAC_OTI_SCENE_BIFS);
			parser->scene_au = gf_sm_stream_au_new(parser->scene_es, 0, au_time, au_is_rap);
			gf_list_add(parser->scene_au->commands, parser->command);
		}

		if (atNode) {
			parser->command->node = atNode;
			gf_node_register(atNode, NULL);
			if (tag == GF_SG_MULTIPLE_INDEXED_REPLACE) {
				parser->command->fromFieldIndex = info.fieldIndex;
				return;
			}
			if (fieldName) {
				field = gf_sg_command_field_new(parser->command);
				field->fieldIndex = info.fieldIndex;

				if (idxNode && idxField) {
					GF_Node *iNode = xmt_find_node(parser, idxNode);
					if (iNode) {
						GF_FieldInfo idxF;
						parser->command->toNodeID = gf_node_get_id(iNode);
						gf_node_get_field_by_name(iNode, idxField, &idxF);
						parser->command->toFieldIndex = idxF.fieldIndex;
						position = 0;
						switch (idxF.fieldType) {
						case GF_SG_VRML_SFBOOL:
							if (*(SFBool*)idxF.far_ptr) position = 1;
							break;
						case GF_SG_VRML_SFINT32:
							if (*(SFInt32*)idxF.far_ptr >=0) position = *(SFInt32*)idxF.far_ptr;
							break;
						case GF_SG_VRML_SFFLOAT:
							if ( (*(SFFloat *)idxF.far_ptr) >=0) position = (s32) floor( FIX2FLT(*(SFFloat*)idxF.far_ptr) );
							break;
						case GF_SG_VRML_SFTIME:
							if ( (*(SFTime *)idxF.far_ptr) >=0) position = (s32) floor( (*(SFTime *)idxF.far_ptr) );
							break;
						}
					}
				}
				if (childField) {
					GF_Node *child = gf_node_list_get_child( ((GF_ParentNode*)atNode)->children, position);
					if (child) {
						parser->command->ChildNodeTag = gf_node_get_tag(child);
						if (parser->command->ChildNodeTag == TAG_ProtoNode) {
							s32 p_id = gf_sg_proto_get_id(gf_node_get_proto(child));
							parser->command->ChildNodeTag = -p_id;
						}
						/*get field in the info struct for later parsing*/
						gf_node_get_field_by_name(child, childField, &info);
						parser->command->child_field = info.fieldIndex;
					}
				}
				/*do not keep position info if index node is used*/
				if (idxNode && idxField) position = -2;

				if (fromNode && fromField) {
					GF_Node *fNode = xmt_find_node(parser, fromNode);
					if (fNode) {
						GF_FieldInfo fField;
						parser->command->fromNodeID = gf_node_get_id(fNode);
						gf_node_get_field_by_name(fNode, fromField, &fField);
						parser->command->fromFieldIndex = fField.fieldIndex;
					}
				}
				if (!fromNode && !fromField) {
					if (gf_sg_vrml_get_sf_type(info.fieldType) != GF_SG_VRML_SFNODE) {
						if (position==-2) {
							field->fieldType = info.fieldType;
							field->field_ptr = gf_sg_vrml_field_pointer_new(info.fieldType);
							info.far_ptr = field->field_ptr;
							if (gf_sg_vrml_is_sf_field(info.fieldType)) {
								xmt_parse_sf_field(parser, &info, atNode, fieldValue);
							} else {
								xmt_parse_mf_field(parser, &info, atNode, fieldValue);
							}
						} else {
							field->fieldType = info.fieldType = gf_sg_vrml_get_sf_type(info.fieldType);
							field->pos = position;
							if (tag != GF_SG_INDEXED_DELETE) {
								field->field_ptr = gf_sg_vrml_field_pointer_new(info.fieldType);
								info.far_ptr = field->field_ptr;
								xmt_parse_sf_field(parser, &info, atNode, fieldValue);
							}
						}
					} else {
						field->pos = position;
						if ((position==-2) && (info.fieldType==GF_SG_VRML_MFNODE)) {
							field->fieldType = GF_SG_VRML_MFNODE;
						} else {
							field->fieldType = GF_SG_VRML_SFNODE;
						}
						parser->state = XMT_STATE_ELEMENTS;
					}
				}
			} else if (tag==GF_SG_NODE_INSERT) {
				field = gf_sg_command_field_new(parser->command);
				field->fieldType = GF_SG_VRML_SFNODE;
				field->pos = position;
				parser->state = XMT_STATE_ELEMENTS;
			}
		}
		else if (routeName) {
			u32 rID = xmt_get_route(parser, routeName, 0);
			if (!rID) {
				parser->command->unres_name = gf_strdup(routeName);
				parser->command->unresolved = 1;
				gf_list_add(parser->unresolved_routes, parser->command);
			} else {
				parser->command->RouteID = rID;
				/*for bt<->xmt conversions*/
				parser->command->def_name = gf_strdup(routeName);
			}
		}
		else if (tag == GF_SG_PROTO_DELETE) {
			char *sep;
			while (fieldValue) {
				GF_Proto *p;
				sep = strchr(fieldValue, ' ');
				if (sep) sep[0] = 0;
				p = gf_sg_find_proto(parser->load->scene_graph, 0, fieldValue);
				if (!p) p = gf_sg_find_proto(parser->load->scene_graph, atoi(fieldValue), NULL);

				if (!p) xmt_report(parser, GF_OK, "Warning: Cannot locate proto %s - skipping", fieldValue);
				else {
					parser->command->del_proto_list = (u32*)gf_realloc(parser->command->del_proto_list, sizeof(u32) * (parser->command->del_proto_list_size+1));
					parser->command->del_proto_list[parser->command->del_proto_list_size] = p->ID;
					parser->command->del_proto_list_size++;
				}
				if (!sep) break;
				sep[0] = ' ';
				fieldValue = sep+1;
			}
		}

		return;
	}

	/*OD commands*/
	if (!strcmp(name, "ObjectDescriptorUpdate") || !strcmp(name, "ObjectDescriptorRemove")
	        || !strcmp(name, "ES_DescriptorUpdate") || !strcmp(name, "ES_DescriptorRemove")
	        || !strcmp(name, "IPMP_DescriptorUpdate") || !strcmp(name, "IPMP_DescriptorRemove") ) {
		u32 stream_id;
		u8 tag = 0;
		GF_StreamContext *stream;
		char *od_ids = NULL;
		char *es_ids = NULL;
		Double au_time = parser->au_time;
		Bool au_is_rap = parser->au_is_rap;

		if (!parser->stream_id) parser->stream_id = parser->base_od_id;
		stream_id = parser->stream_id;

		for (i=0; i<nb_attributes; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
			if (!att->value || !strlen(att->value)) continue;
			if (!strcmp(att->name, "begin")) au_time = atoi(att->value);
			else if (!strcmp(att->name, "isRAP")) au_is_rap = !strcmp(att->value, "yes") ? 1 : 0;
			else if (!stricmp(att->name, "objectDescriptorId")) od_ids = att->value;
			else if (!strcmp(att->name, "ES_ID")) es_ids = att->value;
		}

		/*if we are parsing in an already loaded context and no time is given, force a time != 0 to create a new command*/
		if (!au_time && (parser->load->flags&GF_SM_LOAD_CONTEXT_READY))
			au_time = 0.001;

		if (!strcmp(name, "ObjectDescriptorUpdate")) tag = GF_ODF_OD_UPDATE_TAG;
		else if (!strcmp(name, "ES_DescriptorUpdate")) tag = GF_ODF_ESD_UPDATE_TAG;
		else if (!strcmp(name, "IPMP_DescriptorUpdate")) tag = GF_ODF_IPMP_UPDATE_TAG;
		else if (!strcmp(name, "ObjectDescriptorRemove")) {
			if (!od_ids) return;
			tag = GF_ODF_OD_REMOVE_TAG;
		}
		else if (!strcmp(name, "ES_DescriptorRemove")) {
			if (!od_ids || !es_ids) return;
			tag = GF_ODF_ESD_REMOVE_TAG;
		}
		else if (!strcmp(name, "IPMP_DescriptorRemove")) tag = GF_ODF_IPMP_REMOVE_TAG;

		stream = gf_sm_stream_find(parser->load->ctx, (u16) stream_id);
		if (stream && (stream->streamType!=GF_STREAM_OD)) stream_id = parser->base_od_id;
		parser->od_es = gf_sm_stream_new(parser->load->ctx, (u16) stream_id, GF_STREAM_OD, 0);
		parser->od_au = gf_sm_stream_au_new(parser->od_es, 0, au_time, au_is_rap);
		parser->od_command = gf_odf_com_new(tag);
		gf_list_add(parser->od_au->commands, parser->od_command);

		if (tag == GF_ODF_ESD_REMOVE_TAG) {
			char *sep;
			GF_ESDRemove *esdR = (GF_ESDRemove *) parser->od_command ;
			esdR->ODID = xmt_get_od_id(parser, od_ids);
			while (es_ids) {
				u32 es_id;
				sep = strchr(es_ids, ' ');
				if (sep) sep[0] = 0;
				es_id = xmt_get_esd_id(parser, es_ids);
				if (!es_id) xmt_report(parser, GF_OK, "Warning: Cannot find ES Descriptor %s - skipping", es_ids);
				else {
					esdR->ES_ID = (u16*)gf_realloc(esdR->ES_ID, sizeof(u16) * (esdR->NbESDs+1));
					esdR->ES_ID[esdR->NbESDs] = es_id;
					esdR->NbESDs++;
				}
				if (!sep) break;
				sep[0] = ' ';
				es_ids = sep+1;
			}
		}
		else if (tag == GF_ODF_OD_REMOVE_TAG) {
			char *sep;
			GF_ODRemove *odR = (GF_ODRemove *) parser->od_command ;
			while (od_ids) {
				u32 od_id;
				sep = strchr(od_ids, ' ');
				if (sep) sep[0] = 0;
				od_id = xmt_get_od_id(parser, od_ids);
				if (!od_id) xmt_report(parser, GF_OK, "Warning: Cannot find Object Descriptor %s - skipping", od_ids);
				else {
					odR->OD_ID = (u16*)gf_realloc(odR->OD_ID, sizeof(u16) * (odR->NbODs+1));
					odR->OD_ID[odR->NbODs] = od_id;
					odR->NbODs++;
				}
				if (!sep) break;
				sep[0] = ' ';
				od_ids = sep+1;
			}
		}
	}
}

static void xmt_node_start(void *sax_cbck, const char *name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	GF_Node *elt;
	XMTNodeStack *top, *new_top;
	GF_XMTParser *parser = (GF_XMTParser *)sax_cbck;

	if (parser->last_error) {
		gf_xml_sax_suspend(parser->sax_parser, 1);
		if (parser->command_buffer)
			parser->command_buffer->buffer = NULL;
		return;
	}

	/*init doc type*/
	if (!parser->doc_type) {
		if (!strcmp(name, "XMT-A")) parser->doc_type = 1;
		else if (!strcmp(name, "X3D")) {
			parser->doc_type = 2;
			parser->script_to_load = gf_list_new();
		}
		else if (!strcmp(name, "XMT-O")) parser->doc_type = 3;
		return;
	}

	/*init doc state with already loaded context (for chunk encoding)*/
	if ((parser->state == XMT_STATE_INIT) && (parser->load->flags & GF_SM_LOAD_CONTEXT_READY) && (parser->doc_type == 1)) {
		parser->state = XMT_STATE_COMMANDS;
	}
	/*init doc state for regular parsing*/
	else if (parser->state == XMT_STATE_INIT) {
		/*XMT-A header*/
		if ((parser->doc_type == 1) && !strcmp(name, "Header")) parser->state = XMT_STATE_HEAD;
		/*X3D header*/
		else if ((parser->doc_type == 2) && !strcmp(name, "head")) parser->state = XMT_STATE_HEAD;
		/*XMT-O header*/
		else if ((parser->doc_type == 3) && !strcmp(name, "head")) parser->state = XMT_STATE_HEAD;

		return;
	}

	/*XMT-A header: parse OD/IOD*/
	if ((parser->doc_type == 1) && (parser->state == XMT_STATE_HEAD)) {
		GF_Descriptor *desc, *par;
		par = (GF_Descriptor *)gf_list_last(parser->descriptors);
		desc = xmt_parse_descriptor(parser, (char *) name, attributes, nb_attributes, par);
		if (desc) gf_list_add(parser->descriptors, desc);
		return;
	}
	if (parser->state==XMT_STATE_END) {
		if (!strcmp(name, "head")) {
			parser->state = XMT_STATE_HEAD;
		} else {
			parser->state = XMT_STATE_COMMANDS;
		}
	}

	/*scene content*/
	if (parser->state==XMT_STATE_BODY) {
		/*XMT-A body*/
		if ((parser->doc_type == 1) && !strcmp(name, "Body")) parser->state = XMT_STATE_COMMANDS;
		/*X3D scene*/
		else if ((parser->doc_type == 2) && !strcmp(name, "Scene")) {
			parser->state = XMT_STATE_ELEMENTS;
			if (parser->load->ctx) {
				parser->load->ctx->is_pixel_metrics = 0;
				parser->load->ctx->scene_width = parser->load->ctx->scene_height = 0;
			}
			gf_sg_set_scene_size_info(parser->load->scene_graph, 0, 0, 0);
		}
		/*XMT-O body*/
		else if ((parser->doc_type == 3) && !strcmp(name, "body")) parser->state = XMT_STATE_COMMANDS;
		return;
	}
	/*XMT-A command*/
	if ((parser->doc_type == 1) && (parser->state == XMT_STATE_COMMANDS)) {
		/*OD command*/
		if (parser->od_command) {
			GF_Descriptor *desc, *par;
			par = (GF_Descriptor *)gf_list_last(parser->descriptors);
			desc = xmt_parse_descriptor(parser, (char *) name, attributes, nb_attributes, par);
			if (desc) gf_list_add(parser->descriptors, desc);
		} else {
			xmt_parse_command(parser, name, attributes, nb_attributes);
		}
		return;
	}


	/*node*/
	if (parser->state != XMT_STATE_ELEMENTS) return;

	top = (XMTNodeStack*)gf_list_last(parser->nodes);
	if (!top) top = parser->x3d_root;

	elt = xmt_parse_element(parser, (char *) name, name_space, attributes, nb_attributes, top);
	if (!elt) return;
	GF_SAFEALLOC(new_top, XMTNodeStack);
	if (!new_top) return;

	new_top->node = elt;
	gf_list_add(parser->nodes, new_top);

	/*assign root node here to enable progressive loading*/
	if (!top && (parser->doc_type == 1) && !parser->parsing_proto && parser->command && (parser->command->tag==GF_SG_SCENE_REPLACE) && !parser->command->node) {
		parser->command->node = elt;
		gf_node_register(elt, NULL);
	}
}

static void xmt_node_end(void *sax_cbck, const char *name, const char *name_space)
{
	u32 tag;
	GF_XMTParser *parser = (GF_XMTParser *)sax_cbck;
	XMTNodeStack *top;
	GF_Descriptor *desc;
	GF_Node *node = NULL;
	if (!parser->doc_type || !parser->state) return;

	top = (XMTNodeStack *)gf_list_last(parser->nodes);

	if (!top) {
		/*check descr*/
		desc = (GF_Descriptor*)gf_list_last(parser->descriptors);
		if (desc && (desc->tag == gf_odf_get_tag_by_name((char *)name)) ) {

			/*assign timescales once the ESD has been parsed*/
			if (desc->tag == GF_ODF_ESD_TAG) {
				GF_ESD *esd = (GF_ESD*)desc;
				GF_StreamContext *sc = gf_sm_stream_new(parser->load->ctx, esd->ESID, esd->decoderConfig ? esd->decoderConfig->streamType : 0, esd->decoderConfig ? esd->decoderConfig->objectTypeIndication : 0);
				if (sc && esd->slConfig && esd->slConfig->timestampResolution)
					sc->timeScale = esd->slConfig->timestampResolution;
			}

			gf_list_rem_last(parser->descriptors);
			if (gf_list_count(parser->descriptors)) return;

			if ((parser->doc_type==1) && (parser->state==XMT_STATE_HEAD) && parser->load->ctx && !parser->load->ctx->root_od) {
				parser->load->ctx->root_od = (GF_ObjectDescriptor *)desc;
			}
			else if (!parser->od_command) {
				xmt_report(parser, GF_OK, "Warning: descriptor %s defined outside scene scope - skipping", name);
				gf_odf_desc_del(desc);
			} else {
				switch (parser->od_command->tag) {
				case GF_ODF_ESD_UPDATE_TAG:
					gf_list_add( ((GF_ESDUpdate *)parser->od_command)->ESDescriptors, desc);
					break;
				/*same struct for OD update and IPMP update*/
				case GF_ODF_OD_UPDATE_TAG:
				case GF_ODF_IPMP_UPDATE_TAG:
					gf_list_add( ((GF_ODUpdate *)parser->od_command)->objectDescriptors, desc);
					break;
				}
			}

			return;
		}
		if (parser->state == XMT_STATE_HEAD) {
			if ((parser->doc_type == 1) && !strcmp(name, "Header")) parser->state = XMT_STATE_BODY;
			/*X3D header*/
			else if ((parser->doc_type == 2) && !strcmp(name, "head")) {
				parser->state = XMT_STATE_BODY;
				/*create a group at root level*/
				tag = xmt_get_node_tag(parser, "Group");
				node = gf_node_new(parser->load->scene_graph, tag);
				gf_node_register(node, NULL);
				gf_sg_set_root_node(parser->load->scene_graph, node);
				gf_node_init(node);

				/*create a default top for X3D*/
				GF_SAFEALLOC(parser->x3d_root, XMTNodeStack);
				if (!parser->x3d_root) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("Failed to allocate X3D root\n"));
					return;
				}
				parser->x3d_root->node = node;
			}
			/*XMT-O header*/
			else if ((parser->doc_type == 3) && !strcmp(name, "head")) parser->state = XMT_STATE_BODY;
		}
		else if (parser->state == XMT_STATE_ELEMENTS) {
			assert((parser->doc_type != 1) || parser->command);
			if (!strcmp(name, "Replace") || !strcmp(name, "Insert") || !strcmp(name, "Delete")) {
				parser->command = NULL;
				parser->state = XMT_STATE_COMMANDS;
			}
			/*end proto*/
			else if (!strcmp(name, "ProtoDeclare") || !strcmp(name, "ExternProtoDeclare"))  {
				GF_Proto *cur = parser->parsing_proto;
				xmt_resolve_routes(parser);
				parser->parsing_proto = (GF_Proto*)cur->userpriv;
				parser->load->scene_graph = cur->parent_graph;
				cur->userpriv = NULL;
			}
			else if (parser->proto_field && !strcmp(name, "field")) parser->proto_field = NULL;
			/*end X3D body*/
			else if ((parser->doc_type == 2) && !strcmp(name, "Scene")) parser->state = XMT_STATE_BODY_END;
		}
		else if (parser->state == XMT_STATE_COMMANDS) {
			/*end XMT-A body*/
			if ((parser->doc_type == 1) && !strcmp(name, "Body")) parser->state = XMT_STATE_BODY_END;
			/*end X3D body*/
			else if ((parser->doc_type == 2) && !strcmp(name, "Scene")) parser->state = XMT_STATE_BODY_END;
			/*end XMT-O body*/
			else if ((parser->doc_type == 3) && !strcmp(name, "body")) parser->state = XMT_STATE_BODY_END;

			/*end scene command*/
			else if (!strcmp(name, "Replace") || !strcmp(name, "Insert") || !strcmp(name, "Delete") )  {
				/*restore parent command if in CommandBuffer*/
				if (parser->command && parser->command_buffer && parser->command_buffer->buffer) {
					//empty <Insert>
					if ((parser->command->tag==GF_SG_ROUTE_INSERT) && !parser->command->fromNodeID) {
						gf_list_del_item(parser->command_buffer->commandList, parser->command);
					}

					parser->command = (GF_Command*) parser->command_buffer->buffer;
					parser->command_buffer->buffer = NULL;
					parser->command_buffer = NULL;
				} else {
					//empty <Insert>
					if (parser->command && (parser->command->tag==GF_SG_ROUTE_INSERT) && !parser->command->fromNodeID) {
						gf_list_del_item(parser->scene_au->commands, parser->command);
					}
					parser->command = NULL;
				}
			}
			/*end OD command*/
			else if (!strcmp(name, "ObjectDescriptorUpdate") || !strcmp(name, "ObjectDescriptorRemove")
			         || !strcmp(name, "ES_DescriptorUpdate") || !strcmp(name, "ES_DescriptorRemove")
			         || !strcmp(name, "IPMP_DescriptorUpdate") || !strcmp(name, "IPMP_DescriptorRemove") ) {
				parser->od_command = NULL;
			}

			else if (!strcmp(name, "par"))
				parser->in_com = 1;


		}
		else if (parser->state == XMT_STATE_BODY_END) {
			/*end XMT-A*/
			if ((parser->doc_type == 1) && !strcmp(name, "XMT-A")) parser->state = XMT_STATE_END;
			/*end X3D*/
			else if ((parser->doc_type == 2) && !strcmp(name, "X3D")) {
				while (1) {
					GF_Node *n = (GF_Node *)gf_list_last(parser->script_to_load);
					if (!n) break;
					gf_list_rem_last(parser->script_to_load);
					gf_sg_script_load(n);
				}
				gf_list_del(parser->script_to_load);
				parser->script_to_load = NULL;
				parser->state = XMT_STATE_END;
			}
			/*end XMT-O*/
			else if ((parser->doc_type == 3) && !strcmp(name, "XMT-O")) parser->state = XMT_STATE_END;
		}
		return;
	}
	/*only remove created nodes ... */
	tag = xmt_get_node_tag(parser, name);
	if (!tag) {
		if (top->container_field.name) {
			if (!strcmp(name, top->container_field.name)) {
				if (top->container_field.fieldType==GF_SG_VRML_SFCOMMANDBUFFER) {
					parser->state = XMT_STATE_ELEMENTS;
					parser->command = (GF_Command *) (void *) parser->command_buffer->buffer;
					parser->command_buffer->buffer = NULL;
					parser->command_buffer = NULL;
				}
				top->container_field.far_ptr = NULL;
				top->container_field.name = NULL;
				top->last = NULL;
			}
			/*end of command inside an command (conditional.buffer replace)*/
			else if (!strcmp(name, "Replace") || !strcmp(name, "Insert") || !strcmp(name, "Delete") )  {
				if (parser->command_buffer) {
					if (parser->command_buffer->bufferSize) {
						parser->command_buffer->bufferSize--;
					} else {
						SFCommandBuffer *prev = (SFCommandBuffer *) parser->command_buffer->buffer;
						parser->command_buffer->buffer = NULL;
						parser->command_buffer = prev;
					}
					/*stay in command parsing mode (state 3) until we find </buffer>*/
					parser->state = XMT_STATE_COMMANDS;
				}
			}
			/*end of protofield node(s) content*/
			else if (!strcmp(name, "node") || !strcmp(name, "nodes")) {
				top->container_field.far_ptr = NULL;
				top->container_field.name = NULL;
				top->last = NULL;
			}
		}
		/*SF/MFNode proto field, just pop node stack*/
		else if (!top->node && !strcmp(name, "field")) {
			gf_list_rem_last(parser->nodes);
			gf_free(top);
		} else if (top->node && top->node->sgprivate->tag == TAG_ProtoNode) {
			if (!strcmp(name, "node") || !strcmp(name, "nodes")) {
				top->container_field.far_ptr = NULL;
				top->container_field.name = NULL;
				top->last = NULL;
			} else if (!strcmp(name, "ProtoInstance")) {
				gf_list_rem_last(parser->nodes);
				node = top->node;
				gf_free(top);
				goto attach_node;
			}
		}
	} else if (top->node->sgprivate->tag==tag) {
		node = top->node;
		gf_list_rem_last(parser->nodes);
		gf_free(top);

attach_node:
		top = (XMTNodeStack*)gf_list_last(parser->nodes);
		/*add node to command*/
		if (!top || (top->container_field.fieldType==GF_SG_VRML_SFCOMMANDBUFFER)) {
			if (parser->doc_type == 1) {
				GF_CommandField *inf;
				Bool single_node = 0;
				assert(parser->command);
				switch (parser->command->tag) {
				case GF_SG_SCENE_REPLACE:
					if (parser->parsing_proto) {
						gf_sg_proto_add_node_code(parser->parsing_proto, node);
						gf_node_register(node, NULL);
					} else if (!parser->command->node) {
						parser->command->node = node;
						gf_node_register(node, NULL);
					} else if (parser->command->node != node) {
						xmt_report(parser, GF_OK, "Warning: top-node already assigned - discarding node %s", name);
						gf_node_register(node, NULL);
						gf_node_unregister(node, NULL);
					}
					break;
				case GF_SG_GLOBAL_QUANTIZER:
				case GF_SG_NODE_INSERT:
				case GF_SG_INDEXED_INSERT:
				case GF_SG_INDEXED_REPLACE:
					single_node = 1;
				case GF_SG_NODE_REPLACE:
				case GF_SG_FIELD_REPLACE:
				case GF_SG_MULTIPLE_REPLACE:
					inf = (GF_CommandField*)gf_list_last(parser->command->command_fields);
					if (!inf) {
						inf = gf_sg_command_field_new(parser->command);
						inf->fieldType = GF_SG_VRML_SFNODE;
					}
					if ((inf->fieldType==GF_SG_VRML_MFNODE) && !inf->node_list) {
						inf->field_ptr = &inf->node_list;
						if (inf->new_node) {
							gf_node_list_add_child(& inf->node_list, inf->new_node);
							inf->new_node = NULL;
						}
					}

					if (inf->new_node) {
						if (single_node) {
							gf_node_unregister(inf->new_node, NULL);
						} else {
							inf->field_ptr = &inf->node_list;
							gf_node_list_add_child(& inf->node_list, inf->new_node);
							inf->fieldType = GF_SG_VRML_MFNODE;
						}
						inf->new_node = NULL;
					}
					gf_node_register(node, NULL);
					if (inf->node_list) {
						gf_node_list_add_child(& inf->node_list, node);
					} else {
						inf->new_node = node;
						inf->field_ptr = &inf->new_node;
					}
					break;
				case GF_SG_PROTO_INSERT:
					if (parser->parsing_proto) {
						gf_sg_proto_add_node_code(parser->parsing_proto, node);
						gf_node_register(node, NULL);
						break;
					}
				default:
					xmt_report(parser, GF_OK, "Warning: node %s defined outside scene scope - skipping", name);
					gf_node_register(node, NULL);
					gf_node_unregister(node, NULL);
					break;

				}
			}
			/*X3D*/
			else if (parser->doc_type == 2) {
				if (parser->parsing_proto) {
					gf_sg_proto_add_node_code(parser->parsing_proto, node);
					gf_node_register(node, NULL);
				} else {
					M_Group *gr = (M_Group *)gf_sg_get_root_node(parser->load->scene_graph);
					if (!gr) {
						xmt_report(parser, GF_OK, "Warning: node %s defined outside scene scope - skipping", name);
						gf_node_register(node, NULL);
						gf_node_unregister(node, NULL);
					} else {
						//node has already been added to its parent with X3d parsing, because of the default container resolving
//						gf_node_list_add_child(& gr->children, node);
//						gf_node_register(node, NULL);
					}
				}
			}
			/*special case: replace scene has already been applied (progressive loading)*/
			else if ((parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) && (parser->load->scene_graph->RootNode!=node) ) {
				gf_node_register(node, NULL);
			} else {
				xmt_report(parser, GF_OK, "Warning: node %s defined outside scene scope - skipping", name);
				gf_node_register(node, NULL);
				gf_node_unregister(node, NULL);
			}
		}
		if (parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) {
			/*load scripts*/
			if (!parser->parsing_proto) {
				if ((tag==TAG_MPEG4_Script)
#ifndef GPAC_DISABLE_X3D
				        || (tag==TAG_X3D_Script)
#endif
				   ) {
					/*it may happen that the script uses itself as a field (not sure this is compliant since this
					implies a cyclic structure, but happens in some X3D conformance seq)*/
					if (!top || (top->node != node)) {
						if (parser->command) {
							if (!parser->command->scripts_to_load) parser->command->scripts_to_load = gf_list_new();
							gf_list_add(parser->command->scripts_to_load, node);
						}
						/*do not load script until all routes are established!!*/
						else if (parser->doc_type==2) {
							gf_list_add(parser->script_to_load, node);
						} else {
							gf_sg_script_load(node);
						}
					}
				}
			}
		}
	} else if (parser->current_node_tag==tag) {
		gf_list_rem_last(parser->nodes);
		gf_free(top);
	} else {
		xmt_report(parser, GF_OK, "Warning: closing element %s doesn't match created node %s", name, gf_node_get_class_name(top->node) );
	}
}

static void xmt_text_content(void *sax_cbck, const char *text_content, Bool is_cdata)
{
	const char *buf;
	u32 len;
	GF_XMTParser *parser = (GF_XMTParser *)sax_cbck;
	GF_Node *node;
	XMTNodeStack *top = (XMTNodeStack *)gf_list_last(parser->nodes);
	if (!top || !top->node) return;

	node = top->node;

	buf = text_content;
	len = (u32) strlen(buf);

	if (!len) return;

	switch (gf_node_get_tag((GF_Node *)node)) {
	case TAG_MPEG4_Script:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Script:
#endif
		if (is_cdata) {
			SFScript *sc_f;
			M_Script *sc = (M_Script *) node;
			gf_sg_vrml_mf_reset(& sc->url, GF_SG_VRML_MFSCRIPT);
			gf_sg_vrml_mf_append(& sc->url, GF_SG_VRML_MFSCRIPT, (void **) &sc_f);
			sc->url.vals[0].script_text = gf_strdup(text_content);
		}
		break;
	default:
		break;
	}
}


static GF_XMTParser *xmt_new_parser(GF_SceneLoader *load)
{
	GF_XMTParser *parser;
	if ((load->type==GF_SM_LOAD_XSR) && !load->ctx) return NULL;
	GF_SAFEALLOC(parser, GF_XMTParser);
	if (!parser) return NULL;

	parser->nodes = gf_list_new();
	parser->descriptors = gf_list_new();
	parser->od_links = gf_list_new();
	parser->esd_links = gf_list_new();
	parser->def_nodes = gf_list_new();
	parser->peeked_nodes = gf_list_new();
	parser->inserted_routes = gf_list_new();
	parser->unresolved_routes = gf_list_new();

	parser->sax_parser = gf_xml_sax_new(xmt_node_start, xmt_node_end, xmt_text_content, parser);
	parser->load = load;
	load->loader_priv = parser;
	if (load->ctx) load->ctx->is_pixel_metrics = 1;

	return parser;
}

static GF_Err xmt_restore_context(GF_SceneLoader *load)
{
	u32 i;
	GF_StreamContext *sc;
	GF_XMTParser *parser = (GF_XMTParser *)load->loader_priv;
	if (!parser || !load->ctx) return GF_BAD_PARAM;

	/*restore context - note that base layer are ALWAYS declared BEFORE enhancement layers with gpac parsers*/
	i=0;
	while ((sc = (GF_StreamContext*)gf_list_enum(load->ctx->streams, &i))) {
		switch (sc->streamType) {
		case GF_STREAM_SCENE:
		case GF_STREAM_PRIVATE_SCENE:
			if (!parser->scene_es)
				parser->scene_es = sc;
			break;
		case GF_STREAM_OD:
			if (!parser->od_es)
				parser->od_es = sc;
			break;
		default:
			break;
		}
	}
	/*scene creation - pick up a size*/
	if (!parser->scene_es) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("XMT: No BIFS Streams found in existing context - creating one\n"));
		parser->scene_es = gf_sm_stream_new(load->ctx, 0, GF_STREAM_SCENE, GPAC_OTI_SCENE_BIFS);
		parser->load->ctx->scene_width = 0;
		parser->load->ctx->scene_height = 0;
		parser->load->ctx->is_pixel_metrics = 1;
	}
	else parser->base_scene_id = parser->scene_es->ESID;
	if (parser->od_es) parser->base_od_id = parser->od_es->ESID;

	parser->doc_type = (load->type==GF_SM_LOAD_X3D) ? 2 : 1;
	return GF_OK;
}


static GF_Err load_xmt_initialize(GF_SceneLoader *load, const char *str_data)
{
	GF_Err e;
	GF_XMTParser *parser;

	if (str_data) {
		char BOM[5];
		if (strlen(str_data)<4) return GF_BAD_PARAM;
		BOM[0] = str_data[0];
		BOM[1] = str_data[1];
		BOM[2] = str_data[2];
		BOM[3] = str_data[3];
		BOM[4] = 0;
		parser = xmt_new_parser(load);
		e = gf_xml_sax_init(parser->sax_parser, (unsigned char*)BOM);
		if (e) {
			xmt_report(parser, e, "Error initializing SAX parser");
			return e;
		}
		str_data += 4;
	} else if (load->fileName) {
		parser = xmt_new_parser(load);
	} else {
		return GF_BAD_PARAM;
	}

	/*chunk parsing*/
	if (load->flags & GF_SM_LOAD_CONTEXT_READY) {
		GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("XMT: MPEG-4 (XMT) Chunk Parsing\n"));

		e = xmt_restore_context(load);
		if (e) return e;

	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("XMT: MPEG-4 (XMT) Scene Parsing\n"));
	}

	if (str_data) {
		return gf_xml_sax_parse(parser->sax_parser, str_data);
	}
	return GF_OK;
}


static GF_Err load_xmt_run(GF_SceneLoader *load)
{
	GF_Err e;
	GF_XMTParser *parser = (GF_XMTParser *)load->loader_priv;
	if (!parser) {
		e = load_xmt_initialize(load, NULL);
		if (e) return e;
		parser = (GF_XMTParser *)load->loader_priv;
		if (!parser) return GF_OUT_OF_MEM;
	}

	e = gf_xml_sax_parse_file(parser->sax_parser, (const char *)load->fileName, xmt_progress);
	if (e==GF_OK) e = parser->last_error;

	xmt_resolve_routes(parser);
	xmt_resolve_od_links(parser);

	parser->last_error=GF_OK;
	if (e<0) return xmt_report(parser, e, "Invalid XML document: %s", gf_xml_sax_get_error(parser->sax_parser));

	return GF_OK;
}

static GF_Err load_xmt_parse_string(GF_SceneLoader *load, const char *str)
{
	GF_Err e;
	GF_XMTParser *parser = (GF_XMTParser *)load->loader_priv;
	if (!parser) {
		return load_xmt_initialize(load, str);
	}
	e = gf_xml_sax_parse(parser->sax_parser, str);
	if (e==GF_OK) e = parser->last_error;

	xmt_resolve_routes(parser);
	xmt_resolve_od_links(parser);

	parser->last_error=GF_OK;
	if (e<0) return xmt_report(parser, e, "Invalid XML document: %s", gf_xml_sax_get_error(parser->sax_parser));
	return GF_OK;
}

static void load_xmt_done(GF_SceneLoader *load)
{
	GF_XMTParser *parser = (GF_XMTParser *)load->loader_priv;
	if (!parser) return;

	while (1) {
		XMTNodeStack *st = (XMTNodeStack *)gf_list_last(parser->nodes);
		if (!st) break;
		gf_list_rem_last(parser->nodes);
		gf_node_register(st->node, NULL);
		gf_node_unregister(st->node, NULL);
		gf_free(st);
	}
	if (parser->x3d_root) gf_free(parser->x3d_root);
	gf_list_del(parser->nodes);
	gf_list_del(parser->descriptors);
	gf_list_del(parser->def_nodes);
	gf_list_del(parser->peeked_nodes);

	gf_list_del(parser->inserted_routes);
	gf_list_del(parser->unresolved_routes);
	gf_list_del(parser->od_links);
	gf_list_del(parser->esd_links);
	gf_xml_sax_del(parser->sax_parser);
	if (parser->script_to_load) gf_list_del(parser->script_to_load);
	gf_free(parser);
	load->loader_priv = NULL;
}

static GF_Err load_xmt_suspend(GF_SceneLoader *load, Bool suspend)
{
	GF_XMTParser *parser = (GF_XMTParser *)load->loader_priv;
	if (parser) gf_xml_sax_suspend(parser->sax_parser, suspend);
	return GF_OK;
}


GF_Err gf_sm_load_init_xmt(GF_SceneLoader *load)
{
	load->process = load_xmt_run;
	load->done = load_xmt_done;
	load->parse_string = load_xmt_parse_string;
	load->suspend = load_xmt_suspend;
	if (load->fileName) return load_xmt_initialize(load, NULL);
	return GF_OK;
}


#endif /*GPAC_DISABLE_LOADER_XMT*/
