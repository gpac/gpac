/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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

/*
		WARNING: this is a brute XMT parser, has nothing generic and only accepts 100% XMT-A files only
*/

#include <gpac/scene_manager.h>
#include <gpac/constants.h>
#include <gpac/utf.h>
#include <gpac/xml.h>
#include <gpac/internal/bifs_dev.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_x3d.h>

typedef struct
{
	GF_SceneLoader *load;
	GF_Err last_error;
	XMLParser xml_parser;

	Bool is_x3d;

	/*routes are not created in the graph when parsing, so we need to track insert and delete/replace*/
	GF_List *unresolved_routes, *inserted_routes;
	GF_List *def_nodes, *peeked_nodes;

	char *temp_att;

	/*set when parsing proto*/
	GF_Proto *parsing_proto;

	/*current stream ID, AU time and RAP flag*/
	u32 stream_id;
	Double au_time;
	Bool au_is_rap;

	/*current BIFS stream & AU*/
	GF_StreamContext *bifs_es;
	GF_AUContext *bifs_au;
	u32 base_bifs_id;
	/*for script storage*/
	GF_Command *cur_com;

	/*current OD stream & AU*/
	GF_StreamContext *od_es;
	GF_AUContext *od_au;
	u32 base_od_id;
	u32 bifs_w, bifs_h;
	Bool pixelMetrics;

	/*XMT is soooo ugly for descritpors...*/
	GF_List *od_links;
	GF_List *esd_links;

	Bool resume_is_par;
} XMTParser;


typedef struct
{
	char *desc_name;
	u32 ID;
	/*store nodes refering to this URL*/
	GF_List *nodes;
	GF_ObjectDescriptor *od;
} ODLink;

typedef struct
{
	char *desc_name;
	u32 ESID;
	GF_ESD *esd;
	char *OCR_Name;
	char *Depends_Name;
} ESDLink;

void xmt_parse_command(XMTParser *parser, char *name, GF_List *com_list);
void xmt_resolve_routes(XMTParser *parser);


static GF_Err xmt_report(XMTParser *parser, GF_Err e, char *format, ...)
{
	va_list args;
	va_start(args, format);
	if (parser->load->OnMessage) {
		char szMsg[2048];
		char szMsgFull[2048];
		vsprintf(szMsg, format, args);
		sprintf(szMsgFull, "(line %d) %s", parser->xml_parser.line, szMsg);
		parser->load->OnMessage(parser->load->cbk, szMsgFull, e);
	} else {
		fprintf(stdout, "(line %d) ", parser->xml_parser.line);
		vfprintf(stdout, format, args);
		fprintf(stdout, "\n");
	}
	va_end(args);
	if (e) parser->last_error = e;
	return e;
}



void xmt_new_od_link(XMTParser *parser, GF_ObjectDescriptor *od, char *name)
{
	u32 i, ID, j;
	ODLink *odl;

	ID = 0;
	if (!strnicmp(name, "od", 2)) ID = atoi(name + 2);
	else if (!strnicmp(name, "iod", 3)) ID = atoi(name+ 3);
	/*be carefull, an url like "11-regression-test.mp4" will return 1 on sscanf :)*/
	else if (sscanf(name, "%d", &ID) == 1) {
		char szURL[20];
		sprintf(szURL, "%d", ID);
		if (strcmp(szURL, name)) {
			ID = 0;
		} else {
			name = NULL;
		}
	}
	else ID = 0;

	for (i=0; i<gf_list_count(parser->od_links); i++) {
		odl = gf_list_get(parser->od_links, i);
		if ( (ID && (odl->ID == ID))
			|| (odl->od == od)
			|| (odl->desc_name && name && !strcmp(odl->desc_name, name))
		) {
			if (!odl->od) odl->od = od;
			if (!odl->desc_name && name) odl->desc_name = strdup(name);
			if (!od->objectDescriptorID) {
				od->objectDescriptorID = ID;
			} else if (ID && (od->objectDescriptorID != ID)) {
				xmt_report(parser, GF_BAD_PARAM, "Conflicting OD IDs %d vs %d\n", ID, od->objectDescriptorID);
			}

			for (j=i+1; j<gf_list_count(parser->od_links); j++) {
				ODLink *l2 = gf_list_get(parser->od_links, j);
				if (l2->od == od) {
					odl->ID = od->objectDescriptorID = odl->od->objectDescriptorID;
					gf_list_rem(parser->od_links, j);
					if (l2->desc_name) free(l2->desc_name);
					gf_list_del(l2->nodes);
					free(l2);
					break;
				}
			}
			return;
		}
	}
	odl = malloc(sizeof(ODLink));
	memset(odl, 0, sizeof(ODLink));
	odl->nodes = gf_list_new();
	odl->od = od;
	if (ID) od->objectDescriptorID = ID;
	if (name) odl->desc_name = strdup(name);
	gf_list_add(parser->od_links, odl);
}
void xmt_new_od_link_from_node(XMTParser *parser, char *name, GF_Node *in_node)
{
	u32 i, ID;
	ODLink *odl;

	ID = 0;
	if (!strnicmp(name, "od", 2)) ID = atoi(name + 2);
	else if (!strnicmp(name, "iod", 3)) ID = atoi(name + 3);
	/*be carefull, an url like "11-regression-test.mp4" will return 1 on sscanf :)*/
	else if (sscanf(name, "%d", &ID) == 1) {
		char szURL[20];
		sprintf(szURL, "%d", ID);
		if (strcmp(szURL, name)) {
			ID = 0;
		} else {
			name = NULL;
		}
	}
	else ID = 0;
	
	for (i=0; i<gf_list_count(parser->od_links); i++) {
		odl = gf_list_get(parser->od_links, i);
		if ( (name && odl->desc_name && !strcmp(odl->desc_name, name))
			|| (ID && odl->od && odl->od->objectDescriptorID==ID)
			|| (ID && (odl->ID==ID))
			) {
			if (in_node && (gf_list_find(odl->nodes, in_node)<0) ) gf_list_add(odl->nodes, in_node);
			return;
		}
	}
	odl = malloc(sizeof(ODLink));
	memset(odl, 0, sizeof(ODLink));
	odl->nodes = gf_list_new();
	if (in_node) gf_list_add(odl->nodes, in_node);
	if (ID) odl->ID = ID;
	else odl->desc_name = strdup(name);
	gf_list_add(parser->od_links, odl);
}

void xmt_new_esd_link(XMTParser *parser, GF_ESD *esd, char *desc_name, u32 ID)
{
	u32 i, j;
	ESDLink *esdl;

	for (i=0; i<gf_list_count(parser->esd_links); i++) {
		esdl = gf_list_get(parser->esd_links, i);
		if (esdl->esd  && (esd!=esdl->esd)) continue;
		if (!esdl->esd) {
			if (!esdl->ESID || !desc_name || strcmp(esdl->desc_name, desc_name)) continue;
			esdl->esd = esd;
		}
		if (ID) {
			/*remove temp links*/
			if (esdl->ESID == (u16) ( ( ((u32) esdl) >> 16) | ( ((u32)esdl) & 0x0000FFFF) ) ) {
				for (j=0; j<gf_list_count(parser->load->ctx->streams); j++) {
					GF_StreamContext *sc = gf_list_get(parser->load->ctx->streams, j);
					if (sc->ESID!=esdl->ESID) continue;
					/*reassign*/
					sc->ESID = ID;
					break;
				}
			}
			esdl->ESID = esdl->esd->ESID = ID;
		}
		if (desc_name && !esdl->desc_name) {
			esdl->desc_name = strdup(desc_name);
			if (!esdl->ESID && !strnicmp(desc_name, "es", 2)) esdl->ESID = atoi(&desc_name[2]);
		}
		return;
	}
	esdl = malloc(sizeof(ESDLink));
	memset(esdl, 0, sizeof(ESDLink));
	esdl->esd = esd;
	if (ID) esd->ESID = esdl->ESID = ID;
	if (desc_name) {
		if (!esdl->ESID && !strnicmp(desc_name, "es", 2)) esdl->ESID = atoi(&desc_name[2]);
		esdl->desc_name = strdup(desc_name);
	}
	gf_list_add(parser->esd_links, esdl);
}

u16 xmt_locate_stream(XMTParser *parser, char *stream_name)
{
	ESDLink *esdl;
	u32 i;
	char szN[200];

	for (i=0; i<gf_list_count(parser->esd_links); i++) {
		esdl = gf_list_get(parser->esd_links, i);
		if (esdl->desc_name && !strcmp(esdl->desc_name, stream_name)) return esdl->ESID;
		if (esdl->ESID) {
			sprintf(szN, "es%d", esdl->ESID);
			if (!strcmp(szN, stream_name)) return esdl->ESID;
			sprintf(szN, "%d", esdl->ESID);
			if (!strcmp(szN, stream_name)) return esdl->ESID;
		}
	}
	/*create a temp one*/
	esdl = malloc(sizeof(ESDLink));
	memset(esdl, 0, sizeof(ESDLink));
	esdl->ESID = (u16) ( ((u32) esdl) >> 16) | ( ((u32)esdl) & 0x0000FFFF);
	if (!strnicmp(stream_name, "es", 2)) esdl->ESID = atoi(&stream_name[2]);
	esdl->desc_name = strdup(stream_name);
	gf_list_add(parser->esd_links, esdl);
	return esdl->ESID;
}

Bool xmt_set_dep_id(XMTParser *parser, GF_ESD *desc, char *es_name, Bool is_ocr_dep)
{
	u32 i;
	if (!desc || !es_name) return 0;

	for (i=0; i<gf_list_count(parser->esd_links); i++) {
		ESDLink *esdl = gf_list_get(parser->esd_links, i);
		if (esdl->esd == desc) {
			if (is_ocr_dep)
				esdl->OCR_Name = strdup(es_name);
			else
				esdl->Depends_Name = strdup(es_name);
			return 1;
		}
	}
	return 0;
}


u32 xmt_get_next_node_id(XMTParser *parser)
{
	u32 ID;
	GF_SceneGraph *sc = parser->load->scene_graph;
	if (parser->parsing_proto) sc = gf_sg_proto_get_graph(parser->parsing_proto);
	ID = gf_sg_get_next_available_node_id(sc);
	if (parser->load->ctx && (ID>parser->load->ctx->max_node_id)) 
		parser->load->ctx->max_node_id = ID;
	return ID;
}
u32 xmt_get_next_route_id(XMTParser *parser)
{
	u32 ID;
	GF_SceneGraph *sc = parser->load->scene_graph;
	if (parser->parsing_proto) sc = gf_sg_proto_get_graph(parser->parsing_proto);

	ID = gf_sg_get_next_available_route_id(sc);
	if (parser->load->ctx && (ID>parser->load->ctx->max_route_id)) 
		parser->load->ctx->max_route_id = ID;
	return ID;
}
u32 xmt_get_next_proto_id(XMTParser *parser)
{
	u32 ID;
	GF_SceneGraph *sc = parser->load->scene_graph;
	if (parser->parsing_proto) sc = gf_sg_proto_get_graph(parser->parsing_proto);
	ID = gf_sg_get_next_available_proto_id(sc);
	if (parser->load->ctx && (ID>parser->load->ctx->max_node_id)) 
		parser->load->ctx->max_proto_id = ID;
	return ID;
}

u32 xmt_get_node_id(XMTParser *parser)
{
	GF_Node *n;
	u32 ID;
	if (sscanf(parser->xml_parser.value_buffer, "N%d", &ID) == 1) {
		ID ++;
		n = gf_sg_find_node(parser->load->scene_graph, ID);
		if (n) {
			u32 nID = xmt_get_next_node_id(parser);
			xmt_report(parser, GF_OK, "WARNING: changing node \"%s\" ID from %d to %d", n->sgprivate->NodeName, n->sgprivate->NodeID-1, nID-1);
			gf_node_set_id(n, nID, n->sgprivate->NodeName);
		}
		if (parser->load->ctx && (parser->load->ctx->max_node_id<ID)) parser->load->ctx->max_node_id=ID;
	} else {
		ID = xmt_get_next_node_id(parser);
	}
	return ID;
}

static void xmt_offset_time(XMTParser *parser, Double *time)
{
	*time += parser->au_time;
}

static void xmt_check_time_offset(XMTParser *parser, GF_Node *n, GF_FieldInfo *info)
{
	if (!(parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK)) return;
	if (gf_node_get_tag(n) != TAG_ProtoNode) {
		if (!stricmp(info->name, "startTime") || !stricmp(info->name, "stopTime")) 
			xmt_offset_time(parser, (Double *)info->far_ptr);
	} else if (gf_sg_proto_field_is_sftime_offset(n, info)) {
		xmt_offset_time(parser, (Double *)info->far_ptr);
	}
}
static void xmt_update_timenode(XMTParser *parser, GF_Node *node)
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


#define XMT_GET_VAL \
	char value[100];	\
	u32 i;			\
	char *str = parser->temp_att;	\
	if (!str) {		\
		xmt_report(parser, GF_BAD_PARAM, "%s: Number expected", name);	\
		return;		\
	}		\
	while (str[0] == ' ') str += 1;	\
	i = 0;	\
	while ((str[i] != ' ') && str[i]) {	\
		value[i] = str[i];			\
		i++;				\
	}					\
	value[i] = 0;	\
	while ((str[i] == ' ') && str[i]) i++;	\
	if (!str[i]) parser->temp_att = NULL;	\
	else parser->temp_att = &str[i];	

void xmt_parse_int(XMTParser *parser, const char *name, SFInt32 *val)
{
	XMT_GET_VAL
	*val = atoi(value);
}
void xmt_parse_float(XMTParser *parser, const char *name, SFFloat *val)
{
	XMT_GET_VAL
	*val = FLT2FIX(atof(value));
}
void xmt_parse_time(XMTParser *parser, const char *name, SFTime *val)
{
	XMT_GET_VAL
	*val = atof(value);
}
void xmt_parse_bool(XMTParser *parser, const char *name, SFBool *val)
{
	XMT_GET_VAL
	if (!stricmp(value, "1") || !stricmp(value, "true"))
		*val = 1;
	else
		*val = 0;
}

void xmt_parse_string(XMTParser *parser, const char *name, SFString *val, Bool is_mf)
{
	char value[5000];
	char sep[10];
	u32 i=0;
	u32 k=0;
	char *str = parser->temp_att;
	if (!str) {
		xmt_report(parser, GF_BAD_PARAM, "%s: String expected", name);
		return;
	}

	/*SF string, no inspection*/
	if (!is_mf) {
		if (val->buffer) free(val->buffer);
		val->buffer = NULL;
		if (strlen(str)) val->buffer = xml_translate_xml_string(str);
		parser->temp_att = NULL;
		return;
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
		if (val->buffer) free(val->buffer);
		val->buffer = NULL;
		if (strlen(str)) val->buffer = xml_translate_xml_string(str);
		parser->temp_att = NULL;
		return;
	}
	k = 0;
	i += strlen(sep);

	if (strncmp(&str[i], sep, strlen(sep))) {

		while (str[i]) {
			if ((str[i] == '\\') && !strncmp(&str[i+1], sep, strlen(sep))) {
				i++;
				continue;
			}

			/*handle UTF-8 - WARNING: if parser is in unicode string is already utf8 multibyte chars*/
			if (!parser->xml_parser.unicode_type && (str[i] & 0x80)) {
				/*non UTF8 (likely some win-CP)*/
				if ( (str[i+1] & 0xc0) != 0x80) {
					value[k] = 0xc0 | ( (str[i] >> 6) & 0x3 );
					k++;
					str[i] &= 0xbf;
				}
				/*we only handle UTF8 chars on 2 bytes (eg first byte is 0b110xxxxx)*/
				else if ( (str[i] & 0xe0) == 0xc0) {
					value[k] = str[i];
					i++;
					k++;
				}
			}

			value[k] = str[i];
			i++;
			k++;

			if (!strncmp(&str[i], sep, strlen(sep)) && (str[i-1] != '\\')) break;
		}
	}
	value[k] = 0;
	if (!str[i+strlen(sep)]) parser->temp_att = NULL;
	else if (str[i]) parser->temp_att = &str[i] + strlen(sep);
	else parser->temp_att = NULL;

	
	if (val->buffer) free(val->buffer);
	val->buffer = NULL;
	if (strlen(value)) val->buffer = xml_translate_xml_string(value);
}

void xmt_parse_url(XMTParser *parser, const char *name, SFURL *val, GF_Node *owner, Bool is_mf)
{
	SFString sfstr;
	char value[5000], *tmp;

	/*parse as a string*/
	sfstr.buffer = NULL;
	xmt_parse_string(parser, name, &sfstr, is_mf);
	if (parser->last_error) return;

	if (val->url) free(val->url);
	val->url = sfstr.buffer;
	val->OD_ID = 0;
	/*empty*/
	if (!val->url) return;

	/*remove segments & viewpoints info to create OD link*/
	strcpy(value, val->url);
	tmp = strstr(value, "#");
	if (tmp) tmp[0] = 0;

	/*according to XMT-A spec, both 'od:' and 'od://' are tolerated in XMT-A*/
	if (!strnicmp(value, "od://", 5))
		xmt_new_od_link_from_node(parser, value+5, owner);
	if (!strnicmp(value, "od:", 3))
		xmt_new_od_link_from_node(parser, value+3, owner);
	else
		xmt_new_od_link_from_node(parser, value, owner);
}


void xmt_parse_script(XMTParser *parser, const char *name, SFScript *val, Bool is_mf)
{
	SFString sfstr;

	/*parse as a string*/
	sfstr.buffer = NULL;
	xmt_parse_string(parser, name, &sfstr, is_mf);
	if (parser->last_error) return;

	if (val->script_text) free(val->script_text);
	val->script_text = sfstr.buffer;
}

void xmt_sffield(XMTParser *parser, GF_FieldInfo *info, GF_Node *n)
{
	switch (info->fieldType) {
	case GF_SG_VRML_SFINT32:
		xmt_parse_int(parser, info->name, (SFInt32 *)info->far_ptr); 
		break;
	case GF_SG_VRML_SFBOOL:
		xmt_parse_bool(parser, info->name, (SFBool *)info->far_ptr); 
		break;
	case GF_SG_VRML_SFFLOAT:
		xmt_parse_float(parser, info->name, (SFFloat *)info->far_ptr); 
		break;
	case GF_SG_VRML_SFTIME:
		xmt_parse_time(parser, info->name, (SFTime *)info->far_ptr); 
		xmt_check_time_offset(parser, n, info);
		break;
	case GF_SG_VRML_SFCOLOR:
		xmt_parse_float(parser, info->name, & ((SFColor *)info->far_ptr)->red); 
		xmt_parse_float(parser, info->name, & ((SFColor *)info->far_ptr)->green); 
		xmt_parse_float(parser, info->name, & ((SFColor *)info->far_ptr)->blue); 
		break;
	case GF_SG_VRML_SFVEC2F:
		xmt_parse_float(parser, info->name, & ((SFVec2f *)info->far_ptr)->x); 
		xmt_parse_float(parser, info->name, & ((SFVec2f *)info->far_ptr)->y); 
		break;
	case GF_SG_VRML_SFVEC3F:
		xmt_parse_float(parser, info->name, & ((SFVec3f *)info->far_ptr)->x); 
		xmt_parse_float(parser, info->name, & ((SFVec3f *)info->far_ptr)->y); 
		xmt_parse_float(parser, info->name, & ((SFVec3f *)info->far_ptr)->z); 
		break;
	case GF_SG_VRML_SFROTATION:
		xmt_parse_float(parser, info->name, & ((SFRotation *)info->far_ptr)->x); 
		xmt_parse_float(parser, info->name, & ((SFRotation *)info->far_ptr)->y); 
		xmt_parse_float(parser, info->name, & ((SFRotation *)info->far_ptr)->z); 
		xmt_parse_float(parser, info->name, & ((SFRotation *)info->far_ptr)->q); 
		break;
	case GF_SG_VRML_SFSTRING:
		xmt_parse_string(parser, info->name, (SFString*)info->far_ptr, 0); 
		break;
	case GF_SG_VRML_SFURL:
		xmt_parse_url(parser, info->name, (SFURL*)info->far_ptr, n, 0); 
		break;
	case GF_SG_VRML_SFCOMMANDBUFFER:
	{
		GF_Command *prev_com = parser->cur_com;
		SFCommandBuffer *cb = (SFCommandBuffer *)info->far_ptr;
		xml_skip_attributes(&parser->xml_parser);
		while (!xml_element_done(&parser->xml_parser, "buffer") && !parser->last_error) {
			xmt_parse_command(parser, NULL, cb->commandList);
		}
		parser->cur_com = prev_com;
	}
		break;
	case GF_SG_VRML_SFSCRIPT:
		xmt_parse_script(parser, info->name, (SFScript *)info->far_ptr, 0); 
		break;

	case GF_SG_VRML_SFIMAGE:
	{
		u32 k, size, v;
		SFImage *img = (SFImage *)info->far_ptr;
		xmt_parse_int(parser, "width", &img->width);
		if (parser->last_error) return;
		xmt_parse_int(parser, "height", &img->height);
		if (parser->last_error) return;
		xmt_parse_int(parser, "nbComp", &v);
		if (parser->last_error) return;
		img->numComponents = v;
		size = img->width * img->height * img->numComponents;
		if (img->pixels) free(img->pixels);
		img->pixels = malloc(sizeof(char) * size);
		for (k=0; k<size; k++) {
			char *name = "pixels";
			XMT_GET_VAL
			if (strstr(value, "0x")) sscanf(value, "%x", &v);
			else sscanf(value, "%d", &v);
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
		}
	}
		break;

	default:
		parser->last_error = GF_NOT_SUPPORTED;
		break;

	}
}

void xmt_mffield(XMTParser *parser, GF_FieldInfo *info, GF_Node *n)
{
	GF_FieldInfo sfInfo;
	sfInfo.fieldType = gf_sg_vrml_get_sf_type(info->fieldType);
	sfInfo.name = info->name;
	gf_sg_vrml_mf_reset(info->far_ptr, info->fieldType);

	if (!strlen(parser->xml_parser.value_buffer)) return;

	parser->temp_att = parser->xml_parser.value_buffer;
	while (parser->temp_att && !parser->last_error) {
		gf_sg_vrml_mf_append(info->far_ptr, info->fieldType, &sfInfo.far_ptr);

		/*special case for MF type based on string (MFString, MFURL and MFScript), we need to take care
		of all possible forms of XML multi string encoding*/
		if (sfInfo.fieldType == GF_SG_VRML_SFSTRING) {
			xmt_parse_string(parser, info->name, (SFString*)sfInfo.far_ptr, 1); 
		} else if (sfInfo.fieldType == GF_SG_VRML_SFURL) {
			xmt_parse_url(parser, info->name, (SFURL*)sfInfo.far_ptr, n, 1); 
		} else if (sfInfo.fieldType == GF_SG_VRML_SFSCRIPT) {
			xmt_parse_script(parser, info->name, (SFScript*)sfInfo.far_ptr, 1); 
		} else {
			xmt_sffield(parser, &sfInfo, n);
		}
	}
}


Bool XMTCheckNDT(XMTParser *parser, GF_FieldInfo *info, GF_Node *node, GF_Node *parent)
{
	if (parent->sgprivate->tag == TAG_MPEG4_Script) return 1;
	if (parent->sgprivate->tag == TAG_X3D_Script) return 1;
	/*this handles undefined nodes*/
	if (gf_node_in_table(node, info->NDTtype)) return 1;
	/*not found*/
	xmt_report(parser, GF_BAD_PARAM, "Node %s not valid in field %s\n", gf_node_get_class_name(node), info->name);
	gf_node_unregister(node, parent);
	return 0;
}

u32 GetXMTFieldTypeByName(const char *name)
{
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
	else if (!strcmp(name, "Vector2s") || !strcmp(name, "MFVec2f")) return GF_SG_VRML_MFVEC2F;
	else if (!strcmp(name, "Images") || !strcmp(name, "MFImage")) return GF_SG_VRML_MFIMAGE;
	else if (!strcmp(name, "Times") || !strcmp(name, "MFTime")) return GF_SG_VRML_MFTIME;
	else if (!strcmp(name, "Floats") || !strcmp(name, "MFFloat")) return GF_SG_VRML_MFFLOAT;
	else if (!strcmp(name, "Vector3s") || !strcmp(name, "MFVec3f")) return GF_SG_VRML_MFVEC3F;
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
u32 GetXMTScriptEventTypeByName(const char *name)
{
	if (!strcmp(name, "eventIn") || !strcmp(name, "inputOnly") ) return GF_SG_SCRIPT_TYPE_EVENT_IN;
	else if (!strcmp(name, "eventOut") || !strcmp(name, "outputOnly")) return GF_SG_SCRIPT_TYPE_EVENT_OUT;
	else if (!strcmp(name, "field") || !strcmp(name, "initializeOnly") ) return GF_SG_SCRIPT_TYPE_FIELD;
	else return GF_SG_EVENT_UNKNOWN;
}
u32 GetXMTEventTypeByName(const char *name)
{
	if (!strcmp(name, "eventIn") || !strcmp(name, "inputOnly") ) return GF_SG_EVENT_IN;
	else if (!strcmp(name, "eventOut") || !strcmp(name, "outputOnly")) return GF_SG_EVENT_OUT;
	else if (!strcmp(name, "field") || !strcmp(name, "initializeOnly") ) return GF_SG_EVENT_FIELD;
	else if (!strcmp(name, "exposedField") || !strcmp(name, "inputOutput")) return GF_SG_EVENT_EXPOSED_FIELD;
	else return GF_SG_EVENT_UNKNOWN;
}

GF_Node *xmt_parse_node(XMTParser *parser, char *node_name, GF_Node *parent, s32 *outFieldIndex);


void xmt_parse_field_node(XMTParser *parser, GF_Node *node, GF_FieldInfo *field)
{
	char *str;
	s32 fieldIndex;
	char szType[20];

	/*X3D doesn't use node/nodes syntax element, just a regular listing*/
	if (parser->is_x3d) {
		GF_Node *n = xmt_parse_node(parser, NULL, node, &fieldIndex);
		if (n) {
			if (field->fieldType==GF_SG_VRML_SFNODE) * ((GF_Node **)field->far_ptr) = n;
			else if (field->fieldType==GF_SG_VRML_MFNODE) gf_list_add(*(GF_List **)field->far_ptr, n);
		}
		return;
	}

	str = xml_get_element(&parser->xml_parser);
	strcpy(szType, str);

	if ((field->fieldType==GF_SG_VRML_SFNODE) && strcmp(str, "node") ) {
		xmt_report(parser, GF_BAD_PARAM, "Invalid GF_Node field declaration: expecting \"node\" parent element", str);
		return;
	}
	else if ((field->fieldType==GF_SG_VRML_MFNODE) && strcmp(str, "nodes")) {
		xmt_report(parser, GF_BAD_PARAM, "Invalid MFNode field declaration: expecting \"nodes\" parent element", str);
		return;
	}
	xml_skip_attributes(&parser->xml_parser);
	while (!xml_element_done(&parser->xml_parser, szType)) {
		GF_Node *n = xmt_parse_node(parser, NULL, node, &fieldIndex);
		if (n) {
			if (field->fieldType==GF_SG_VRML_SFNODE) * ((GF_Node **)field->far_ptr) = n;
			else if (field->fieldType==GF_SG_VRML_MFNODE) gf_list_add(*(GF_List **)field->far_ptr, n);
		}
	}
}

void xmt_parse_script_field(XMTParser *parser, GF_Node *node)
{
	GF_ScriptField *scfield;
	GF_FieldInfo field;
	char *str, *val;
	u32 fieldType, eventType;
	char fieldName[1024];

	fieldType = eventType = 0;
	val = NULL;
	while (xml_has_attributes(&parser->xml_parser)) {
		str = xml_get_attribute(&parser->xml_parser);
		if (!strcmp(str, "name")) strcpy(fieldName, parser->xml_parser.value_buffer); 
		else if (!strcmp(str, "type")) fieldType = GetXMTFieldTypeByName(parser->xml_parser.value_buffer); 
		else if (!strcmp(str, "vrml97Hint") || !strcmp(str, "accessTtpe")) eventType = GetXMTScriptEventTypeByName(parser->xml_parser.value_buffer); 
		else if (strstr(str, "value") || strstr(str, "Value")) val = strdup(parser->xml_parser.value_buffer);
	}
	scfield = gf_sg_script_field_new(node, eventType, fieldType, fieldName);

	if (!scfield) {
		xmt_report(parser, GF_BAD_PARAM, "cannot create script field - check syntax");
		return;
	}
	if (val) {
		gf_node_get_field_by_name(node, fieldName, &field);
		str = parser->xml_parser.value_buffer;
		parser->xml_parser.value_buffer = parser->temp_att = val;
		if (gf_sg_vrml_is_sf_field(fieldType)) {
			xmt_sffield(parser, &field, node);
		} else {
			xmt_mffield(parser, &field, node);
		}
		parser->xml_parser.value_buffer = str;
		free(val);
	}
	/*for SF/MF Nodes*/
	while (!xml_element_done(&parser->xml_parser, "field")) {
		gf_node_get_field_by_name(node, fieldName, &field);
		xmt_parse_field_node(parser, node, &field);
		if (parser->last_error) return;
	}
}

void xmt_parse_ised(XMTParser *parser, GF_Node *node)
{
	GF_Err e;
	char *str;
	GF_ProtoFieldInterface *pf;
	GF_FieldInfo pfield, nfield;
	char szNode[1024], szProto[1024];

	while (!xml_element_done(&parser->xml_parser, "IS")) {
		str = xml_get_element(&parser->xml_parser);
		if (!strcmp(str, "connect")) {
			while (xml_has_attributes(&parser->xml_parser)) {
				str = xml_get_attribute(&parser->xml_parser);
				if (!strcmp(str, "nodeField")) strcpy(szNode, parser->xml_parser.value_buffer);
				else if (!strcmp(str, "protoField")) strcpy(szProto, parser->xml_parser.value_buffer);
			}
			xml_element_done(&parser->xml_parser, "connect");
			e = gf_node_get_field_by_name(node, szNode, &nfield);
			if (e) {
				xmt_report(parser, GF_BAD_PARAM, "%s: Unknown node field", szNode);
				return;
			}
			pf = gf_sg_proto_field_find_by_name(parser->parsing_proto, szProto);
			if (!pf) {
				xmt_report(parser, GF_BAD_PARAM, "%s: Unknown proto field", szProto);
				return;
			}
			gf_sg_proto_field_get_field(pf, &pfield);
			e = gf_sg_proto_field_set_ised(parser->parsing_proto, pfield.fieldIndex, node, nfield.fieldIndex);
			if (e) xmt_report(parser, GF_BAD_PARAM, "Cannot set IS field (Error %s)", gf_error_to_string(e));
		}
		else xml_skip_element(&parser->xml_parser, str);
	}
}


GF_Node *xmt_proto_instance(XMTParser *parser, GF_Node *parent)
{
	u32 ID;
	GF_Proto *p;
	char szDEFName[1024], szProtoName[1024];
	Bool isUSE;
	GF_Node *node;
	GF_FieldInfo info;
	GF_SceneGraph *sg;
	GF_Err e;
	char *str;

	p = NULL;

	isUSE = 0;
	szDEFName[0] = 0;
	szProtoName[0] = 0;
	ID = 0;
	while (xml_has_attributes(&parser->xml_parser)) {
		str = xml_get_attribute(&parser->xml_parser);
		/*DEF node*/
		if (!strcmp(str, "DEF")) strcpy(szDEFName, parser->xml_parser.value_buffer);
		/*USE node*/
		else if (!strcmp(str, "USE")) {
			strcpy(szDEFName, parser->xml_parser.value_buffer);
			isUSE = 1;
		}
		/*proto name*/
		else if (!strcmp(str, "name")) strcpy(szProtoName, parser->xml_parser.value_buffer);
	}

	sg = parser->load->scene_graph;
	while (1) {
		p = gf_sg_find_proto(sg, 0, szProtoName);
		if (p) break;
		sg = sg->parent_scene;
		if (!sg) break;
	}
	if (!p) {
		xmt_report(parser, GF_BAD_PARAM, "%s: not a valid/supported proto", szProtoName);
		return NULL;
	}

	if (isUSE) {
		node = gf_sg_find_node_by_name(parser->load->scene_graph, parser->xml_parser.value_buffer);
		if (!node) {
			/*create a temp node (undefined)*/
			node = gf_sg_proto_create_instance(parser->load->scene_graph, p);
			str = parser->xml_parser.value_buffer;
			parser->xml_parser.value_buffer = szDEFName;
			ID = xmt_get_node_id(parser);
			gf_node_set_id(node, ID, str);
			parser->xml_parser.value_buffer = str;
		}
		gf_node_register(node, parent);
		if (!xml_element_done(&parser->xml_parser, NULL)) {
			xmt_report(parser, GF_BAD_PARAM, "Too many attributes - only USE=\"ID\" allowed");
		}
		return node;
	}

	if (strlen(szDEFName)) {
		node = gf_sg_find_node_by_name(parser->load->scene_graph, szDEFName);
		if (node) {
			ID = node->sgprivate->NodeID;
		} else {
			ID = xmt_get_node_id(parser);
			node = gf_sg_proto_create_instance(parser->load->scene_graph, p);
			gf_node_set_id(node, ID, szDEFName);
		}
	} else {
		node = gf_sg_proto_create_instance(parser->load->scene_graph, p);
	}
	gf_node_register(node, parent);

	while (!xml_element_done(&parser->xml_parser, "ProtoInstance")) {
		str = xml_get_element(&parser->xml_parser);
		if (!strcmp(str, "fieldValue")) {
			char szField[1024];
			char *szVal;
			szVal = NULL;
			while (xml_has_attributes(&parser->xml_parser)) {
				str = xml_get_attribute(&parser->xml_parser);
				if (!strcmp(str, "name")) strcpy(szField, parser->xml_parser.value_buffer);
				else if (strstr(str, "value") || strstr(str, "Value")) szVal = strdup(parser->xml_parser.value_buffer);
			}
			e = gf_node_get_field_by_name(node, szField, &info);
			if (e) {
				xmt_report(parser, GF_BAD_PARAM, "%s: Unknown proto field", szField);
				goto err_exit;
			}
			if (szVal) {
				str = parser->xml_parser.value_buffer;
				parser->xml_parser.value_buffer = parser->temp_att = szVal;
				if (gf_sg_vrml_is_sf_field(info.fieldType)) {
					xmt_sffield(parser, &info, node);
				} else {
					xmt_mffield(parser, &info, node);
				}
				parser->xml_parser.value_buffer = str;
				free(szVal);
				gf_sg_proto_mark_field_loaded(node, &info);
			} else {
				/*for X3D, in XMT only 1 value possible with MFNodes*/
				while (!xml_element_done(&parser->xml_parser, "fieldValue")) {
					xmt_parse_field_node(parser, node, &info);
					if (parser->last_error) goto err_exit;
				}
			}
			xml_element_done(&parser->xml_parser, "fieldValue");
		}
		else xml_skip_element(&parser->xml_parser, str);
	}

	if (!parser->parsing_proto) gf_node_init(node);
	return node;

err_exit:
	gf_node_unregister(node, parent);
	return NULL;
}

Bool XMT_HasBeenDEF(XMTParser *parser, char *node_name)
{
	u32 i, count;
	count = gf_list_count(parser->def_nodes);
	for (i=0; i<count; i++) {
		GF_Node *n = gf_list_get(parser->def_nodes, i);
		if (!strcmp(n->sgprivate->NodeName, node_name)) return 1;
	}
	return 0;
}

u32 xmt_get_node_tag(XMTParser *parser, char *node_name) 
{
	u32 tag;

	/*if VRML and allowing non MPEG4 nodes, use X3D*/
	if (parser->is_x3d && !(parser->load->flags & GF_SM_LOAD_MPEG4_STRICT)) {
		tag = gf_node_x3d_type_by_class_name(node_name);
		if (!tag) tag = gf_node_mpeg4_type_by_class_name(node_name);
	} else {
		tag = gf_node_mpeg4_type_by_class_name(node_name);
		/*if allowing non MPEG4 nodes, try X3D*/
		if (!tag && !(parser->load->flags & GF_SM_LOAD_MPEG4_STRICT)) tag = gf_node_x3d_type_by_class_name(node_name);
	}
	return tag;
}

GF_Err xmt_get_default_container(GF_Node *par, GF_Node *n, GF_FieldInfo *info)
{
	u32 i, count;
	count = gf_node_get_field_count(par);
	/*get the first field/exposedField accepting this child*/
	for (i=0; i<count; i++) {
		gf_node_get_field(par, i, info);
		if ((info->eventType==GF_SG_EVENT_OUT) || (info->eventType==GF_SG_EVENT_IN)) continue;
		if (gf_node_in_table(n, info->NDTtype)) return GF_OK;
	}
	return GF_BAD_PARAM;
}

GF_Node *xmt_parse_node(XMTParser *parser, char *node_name, GF_Node *parent, s32 *fieldIndex)
{
	u32 tag, ID;
	Bool is_script, register_def;
	GF_Node *node, *undef_node, *n;
	GF_FieldInfo info;
	char nName[100], szDEFName[1000];
	/*for X3D child node syntax*/
	char * str;

	if (node_name) {
		str = node_name;
	} else {
		str = xml_get_element(&parser->xml_parser);
	}
	ID = 0;
	if (!strcmp(str, "ProtoInstance")) return xmt_proto_instance(parser, parent);
	if (!strcmp(str, "NULL")) return NULL;
	tag = xmt_get_node_tag(parser, str);
	if (!tag) {
		xmt_report(parser, GF_OK, "%s: not a valid/supported node, skipping", str);
		xml_skip_element(&parser->xml_parser, str);
		return NULL;
	}
	strcpy(nName, str);
	node = gf_node_new(parser->load->scene_graph, tag);
//	printf("create node %s\n", nName);
	
	is_script = 0;
	if ((tag==TAG_MPEG4_Script) || (tag==TAG_X3D_Script)) is_script = 1;

	if (!node) {
		parser->last_error = GF_SG_UNKNOWN_NODE;
		return NULL;
	}

	if (!parser->parsing_proto) xmt_update_timenode(parser, node);

	if (fieldIndex) (*fieldIndex) = -1;
	undef_node = NULL;
	ID = 0;
	register_def = 0;
	/*parse all attributes*/
	while (xml_has_attributes(&parser->xml_parser)) {
		str = xml_get_attribute(&parser->xml_parser);
		/*DEF node*/
		if (!strcmp(str, "DEF")) {
			register_def = 1;
			undef_node = gf_sg_find_node_by_name(parser->load->scene_graph, parser->xml_parser.value_buffer);
			if (undef_node) {
				gf_list_del_item(parser->peeked_nodes, undef_node);
				ID = undef_node->sgprivate->NodeID;
				/*if we see twice a DEF N1 then force creation of a new node*/
				if (XMT_HasBeenDEF(parser, parser->xml_parser.value_buffer)) {
					undef_node = NULL;
					ID = xmt_get_node_id(parser);
					xmt_report(parser, GF_OK, "Warning: Node %s has been DEFed several times, IDs may get corrupted", parser->xml_parser.value_buffer);
				}
			} else {
				ID = xmt_get_node_id(parser);
			}
			strcpy(szDEFName, parser->xml_parser.value_buffer);
		}
		/*USE node*/
		else if (!strcmp(str, "USE")) {
			GF_Node *def_node = gf_sg_find_node_by_name(parser->load->scene_graph, parser->xml_parser.value_buffer);

			/*DESTROY NODE*/
			gf_node_register(node, NULL);
			gf_node_unregister(node, NULL);

			if (!def_node) {
				xmt_report(parser, GF_BAD_PARAM, "USE: Cannot find node %s - skipping", parser->xml_parser.value_buffer);
				xml_skip_element(&parser->xml_parser, nName);
				parser->last_error = GF_OK;
				return NULL;
			}
			node = def_node;
			gf_node_register(node, parent);
			if (!xml_element_done(&parser->xml_parser, NULL)) {
				xmt_report(parser, GF_BAD_PARAM, "Too many attributes - only USE=\"ID\" allowed");
			}
			return node;
		}
		/*X3D stuff*/
		else if (!strcmp(str, "containerField")) {
			if (fieldIndex && parent) {
				gf_node_get_field_by_name(parent, str, &info);
				(*fieldIndex) = info.fieldIndex;
			}
		}
		/*all other fields*/
		else {
			parser->last_error = gf_node_get_field_by_name(node, str, &info);
			if (parser->last_error) {
				xmt_report(parser, parser->last_error, "%s: Unknown/Invalid node field", str);
				gf_node_register(node, parent);
				goto err;
			}
			if (gf_sg_vrml_is_sf_field(info.fieldType)) {
				parser->temp_att = parser->xml_parser.value_buffer;
				xmt_sffield(parser, &info, node);
			} else {
				xmt_mffield(parser, &info, node);
			}
		}
	}
	gf_node_register(node, parent);
	if (register_def) gf_list_add(parser->def_nodes, node);

	/*VRML: "The transformation hierarchy shall be a directed acyclic graph; results are undefined if a node 
	in the transformation hierarchy is its own ancestor"
	that's good, because the scene graph can't handle cyclic graphs (destroy will never be called).
	However we still have to register the node before parsing it, to update node registry and get correct IDs*/
	if (ID) {
		gf_node_set_id(node, ID, szDEFName);
		if (undef_node) gf_node_replace(undef_node, node, 0);
	}

	/*sub-nodes or complex att*/
	while (!xml_element_done(&parser->xml_parser, nName)) {
		char subName[100];

		if (parser->last_error) goto err;
		
		str = xml_get_element(&parser->xml_parser);

		if (is_script && !strcmp(str, "field")) {
			xmt_parse_script_field(parser, node);
			continue;
		}
		else if (parser->parsing_proto && !strcmp(str, "IS")) {
			xml_skip_attributes(&parser->xml_parser);
			xmt_parse_ised(parser, node);
			continue;
		}

		/*X3D has a different (and lighter) syntax here: SF/MFNode field names are never declared, 
		only the node is*/
		parser->last_error = gf_node_get_field_by_name(node, str, &info);
		if (parser->last_error) {
			s32 fieldIndex;
			u32 tag = xmt_get_node_tag(parser, str);
			if (!tag) {
				xml_skip_element(&parser->xml_parser, str);
				parser->last_error = GF_OK;
				continue;
			}
			parser->last_error = GF_OK;
			n = xmt_parse_node(parser, str, node, &fieldIndex);
			/*we had a container*/
			if (fieldIndex>=0) {
				parser->last_error = gf_node_get_field(node, (u32) fieldIndex, &info);
				if (n && !gf_node_in_table(n, info.NDTtype)) {
					xmt_report(parser, GF_BAD_PARAM, "Node %s not valid in field %s\n", gf_node_get_class_name(n), info.name);
					gf_node_unregister(n, node);
					goto err;
				}
			}
			/*get default container*/
			else if (n) {
				parser->last_error = xmt_get_default_container(node, n, &info);
			}
			/*if specified several times, destroy previous*/
			if ((info.fieldType==GF_SG_VRML_SFNODE) && (* ((GF_Node **)info.far_ptr) ) ) {
				gf_node_unregister(* ((GF_Node **)info.far_ptr), node);
				* ((GF_Node **)info.far_ptr) = NULL;
			}
			if (!n) continue;

			if (parser->last_error) {
				xmt_report(parser, parser->last_error, "Cannot locate container for node %s in node %s\n", gf_node_get_class_name(n), gf_node_get_class_name(node));
				goto err;
			}

			if (info.fieldType==GF_SG_VRML_SFNODE) {
				* ((GF_Node **)info.far_ptr) = n;
			} else if (info.fieldType==GF_SG_VRML_MFNODE) {
				gf_list_add(*(GF_List **)info.far_ptr, n);
			} else {
				xmt_report(parser, parser->last_error, "Bad container (%s) for for node %s in node %s\n", info.name, gf_node_get_class_name(n), gf_node_get_class_name(node));
				goto err;
			}
			continue;
		}

		strcpy(subName, str);
		if (gf_sg_vrml_get_sf_type(info.fieldType) != GF_SG_VRML_SFNODE) {
			if (gf_sg_vrml_is_sf_field(info.fieldType)) {
				parser->temp_att = parser->xml_parser.value_buffer;
				xmt_sffield(parser, &info, node);
			} else {
				xmt_mffield(parser, &info, node);
			}
		} else {
			s32 fieldIndex;
			xml_skip_attributes(&parser->xml_parser);
			while (!xml_element_done(&parser->xml_parser, subName) && !parser->last_error) {
				n = xmt_parse_node(parser, NULL, node, &fieldIndex);

				/*if specified several times, destroy previous*/
				if ((info.fieldType==GF_SG_VRML_SFNODE) && (* ((GF_Node **)info.far_ptr) ) ) {
					gf_node_unregister(* ((GF_Node **)info.far_ptr), node);
					* ((GF_Node **)info.far_ptr) = NULL;
				}

				if (n) {
					switch (info.fieldType) {
					case GF_SG_VRML_SFNODE:
						if (!XMTCheckNDT(parser, &info, n, node)) goto err;
						/*if specified several times, destroy previous*/
						if (* ((GF_Node **)info.far_ptr) )
							gf_node_unregister(* ((GF_Node **)info.far_ptr), node);
						* ((GF_Node **)info.far_ptr) = n;
						break;
					case GF_SG_VRML_MFNODE:
						if (!XMTCheckNDT(parser, &info, n, node)) goto err;
						gf_list_add(*(GF_List **)info.far_ptr, n);
						break;
					}
				}
			}
		}
	}

	if (!parser->parsing_proto) {
		gf_node_init(node);
		if (is_script && (parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) ) {
			if (parser->cur_com) {
				if (!parser->cur_com->scripts_to_load) parser->cur_com->scripts_to_load = gf_list_new();
				gf_list_add(parser->cur_com->scripts_to_load, node);
			} else {
				gf_sg_script_load(node);
			}
		}
	}
	return node;

err:
	gf_node_unregister(node, parent);
	return NULL;
}


GF_Descriptor *xmt_parse_descriptor(XMTParser *parser, char *name, GF_Descriptor *the_desc);
GF_IPMPX_Data *xmt_parse_ipmpx(XMTParser *parser, char *name);

void xmt_parse_ipmpx_field(XMTParser *parser, GF_IPMPX_Data *desc, char *name, char *value)
{
	char field[1024];
	u32 type;
	GF_IPMPX_Data *subdesc;
	GF_Descriptor *sdesc;
	/*regular field*/
	if (value) {
		parser->last_error = gf_ipmpx_set_field(desc, name, parser->xml_parser.value_buffer);
	} 
	/*descriptor field*/
	else {
		strcpy(field, name);
		type = gf_ipmpx_get_field_type(desc, name);
		switch (type) {
		/*desc*/
		case GF_ODF_FT_OD:
			assert(desc->tag==GF_IPMPX_CONNECT_TOOL_TAG);
			xml_skip_attributes(&parser->xml_parser);
			sdesc = xmt_parse_descriptor(parser, NULL, NULL);
			if (sdesc) {
				assert(sdesc->tag==GF_ODF_IPMP_TAG);
				((GF_IPMPX_ConnectTool *)desc)->toolDescriptor = (GF_IPMP_Descriptor *)sdesc;
			}
			xml_element_done(&parser->xml_parser, field);
			break;
		/*descriptor list*/
		case GF_ODF_FT_OD_LIST:
			assert(desc->tag==GF_IPMPX_GET_TOOLS_RESPONSE_TAG);
			xml_skip_attributes(&parser->xml_parser);
			while (!xml_element_done(&parser->xml_parser, field)) {
				sdesc = xmt_parse_descriptor(parser, NULL, NULL);
				if (sdesc) {
					assert(sdesc->tag==GF_ODF_IPMP_TOOL_TAG);
					gf_list_add( ((GF_IPMPX_GetToolsResponse *)desc)->ipmp_tools, sdesc);
				}
			}
			break;
		/*IPMPX ByteArray list*/
		case GF_ODF_FT_IPMPX_BA_LIST:
			xml_skip_attributes(&parser->xml_parser);
			while (!xml_element_done(&parser->xml_parser, field)) {
				char sfield[1024];
				char *str = xml_get_element(&parser->xml_parser);
				if (!str) break;
				strcpy(sfield, str);

				while (xml_has_attributes(&parser->xml_parser)) {
					char *str = xml_get_attribute(&parser->xml_parser);
					if (!stricmp(str, "array")) {
						GF_Err e = gf_ipmpx_set_byte_array(desc, field, parser->xml_parser.value_buffer);
						if (e) xmt_report(parser, e, "Error assigning IPMP ByteArray %s\n", field);
					}
				}
				xml_element_done(&parser->xml_parser, sfield);
			}
			break;
		/*IPMPX ByteArray*/
		case GF_ODF_FT_IPMPX_BA:
			while (xml_has_attributes(&parser->xml_parser)) {
				char *str = xml_get_attribute(&parser->xml_parser);
				if (!stricmp(str, "array")) {
					GF_Err e = gf_ipmpx_set_byte_array(desc, field, parser->xml_parser.value_buffer);
					if (e) xmt_report(parser, e, "Error assigning IPMP ByteArray %s\n", field);
				}
			}
			xml_element_done(&parser->xml_parser, field);
			break;
		/*IPMPX list*/
		case GF_ODF_FT_IPMPX_LIST:
			xml_skip_attributes(&parser->xml_parser);
			while (!xml_element_done(&parser->xml_parser, field)) {
				subdesc = xmt_parse_ipmpx(parser, NULL);
				if (subdesc && (gf_ipmpx_set_sub_data(desc, field, subdesc) != GF_OK)) {
					xmt_report(parser, GF_BAD_PARAM, "Invalid ipmpx in field %s - skipping", field);
					gf_ipmpx_data_del(subdesc);
				}
			}
			break;
		/*IPMPX */
		case GF_ODF_FT_IPMPX:
			xml_skip_attributes(&parser->xml_parser);
			subdesc = xmt_parse_ipmpx(parser, NULL);
			if (!subdesc) break;
			if (gf_ipmpx_set_sub_data(desc, field, subdesc) != GF_OK) {
				xmt_report(parser, GF_BAD_PARAM, "Invalid ipmpx in field %s - skipping", field);
				gf_ipmpx_data_del(subdesc);
			}
			xml_element_done(&parser->xml_parser, field);
			break;
		case GF_ODF_FT_DEFAULT:
			xmt_report(parser, GF_BAD_PARAM, "%s: unknown field", name);
			break;
		}
	}
}

GF_IPMPX_Data *xmt_parse_ipmpx(XMTParser *parser, char *name)
{
	char *str, desc_name[1024];
	GF_IPMPX_Data *desc;
	u8 tag;

	if (name) {
		strcpy(desc_name,  name);
	} else {
		str = xml_get_element(&parser->xml_parser);
		if (!str) return NULL;
		strcpy(desc_name, str);
	}
	tag = gf_ipmpx_get_tag(desc_name);
	if (!tag) {
		xmt_report(parser, GF_OK, "Unknown IPMPX data %s - skipping", desc_name);
		xml_skip_element(&parser->xml_parser, desc_name);
		return NULL;
	}

	desc = gf_ipmpx_data_new(tag);
	if (!desc) return NULL;

	/*parse attributes*/
	while (xml_has_attributes(&parser->xml_parser)) {
		str = xml_get_attribute(&parser->xml_parser);
		if (!strcmp(str, "value")) {
			xmt_parse_ipmpx_field(parser, desc, name, parser->xml_parser.value_buffer);
		} else {
			xmt_parse_ipmpx_field(parser, desc, str, parser->xml_parser.value_buffer);
		}
		if (parser->last_error) {
			gf_ipmpx_data_del(desc);
			xml_skip_element(&parser->xml_parser, desc_name);
			return NULL;
		}
	}
	/*parse sub desc - WARNING: XMT defines some properties as elements in OD and also introduces dummy containers!!!*/
	while (!xml_element_done(&parser->xml_parser, desc_name) && !parser->last_error) {
		str = xml_get_element(&parser->xml_parser);
		xmt_parse_ipmpx_field(parser, desc, str, NULL);
	}
	return desc;
}

static void xmt_add_desc(XMTParser *parser, GF_Descriptor *par, GF_Descriptor *child, char *fieldName)
{
	GF_Err e = gf_odf_desc_add_desc(par, child);
	if (e) {
		xmt_report(parser, GF_OK, "Invalid child descriptor in field %s", fieldName);
		gf_odf_desc_del(child);
	}
}

void xmt_parse_descr_field(XMTParser *parser, GF_Descriptor *desc, char *name, char *value)
{
	char field[1024];
	u32 type;
	GF_IPMPX_Data *ipmpx;
	GF_Descriptor *subdesc;
	/*regular field*/
	if (value) {
		parser->last_error = gf_odf_set_field(desc, name, parser->xml_parser.value_buffer);
	} 
	/*descriptor field*/
	else {
		strcpy(field, name);
		type = gf_odf_get_field_type(desc, name);
		switch (type) {
		/*IPMPX list*/
		case GF_ODF_FT_IPMPX_LIST:
			if(desc->tag!=GF_ODF_IPMP_TAG) {
				xmt_report(parser, GF_BAD_PARAM, "IPMPX_Data list only allowed in IPMP_Descriptor");
				gf_odf_desc_del(desc);
				return;
			}
			xml_skip_attributes(&parser->xml_parser);
			while (!xml_element_done(&parser->xml_parser, field)) {
				ipmpx = xmt_parse_ipmpx(parser, NULL);
				if (ipmpx) gf_list_add( ((GF_IPMP_Descriptor *)desc)->ipmpx_data, ipmpx);
			}
			break;
		/*ipmpx*/
		case GF_ODF_FT_IPMPX:
			if(desc->tag!=GF_ODF_IPMP_TOOL_TAG) {
				xmt_report(parser, GF_BAD_PARAM, "IPMPX_Data only allowed in IPMP_Tool");
				gf_odf_desc_del(desc);
				return;
			}
			xml_skip_attributes(&parser->xml_parser);
			ipmpx = xmt_parse_ipmpx(parser, NULL);
			if (!ipmpx) break;
			if (ipmpx->tag==GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG) {
				GF_IPMP_Tool *it = (GF_IPMP_Tool *)desc;
				if (it->toolParamDesc) gf_ipmpx_data_del((GF_IPMPX_Data *)it->toolParamDesc);
				it->toolParamDesc = (GF_IPMPX_ParametricDescription*)ipmpx;
			} else {
				xmt_report(parser, GF_OK, "Only ToolParametricDescription allowed in GF_IPMP_Tool - skipping");
				gf_ipmpx_data_del(ipmpx);
			}
			xml_element_done(&parser->xml_parser, field);
			break;
		/*desc list*/
		case GF_ODF_FT_OD_LIST:
			xml_skip_attributes(&parser->xml_parser);
			while (!xml_element_done(&parser->xml_parser, field)) {
				subdesc = xmt_parse_descriptor(parser, NULL, NULL);
				if (!subdesc) break;
				xmt_add_desc(parser, desc, subdesc, field);
			}
			break;
		/*desc*/
		case GF_ODF_FT_OD:
			if (!strcmp(field, "StreamSource")) {
				subdesc = gf_odf_desc_new(GF_ODF_MUXINFO_TAG);
				subdesc = xmt_parse_descriptor(parser, field, subdesc);
			} else {
				xml_skip_attributes(&parser->xml_parser);
				subdesc = xmt_parse_descriptor(parser, NULL, NULL);
			}
			if (!subdesc) break;
			xmt_add_desc(parser, desc, subdesc, field);
			xml_element_done(&parser->xml_parser, field);
			break;
		}
	}
	if (parser->last_error) {
		xmt_report(parser, parser->last_error, "%s: unknown field", name);
	}
}

GF_Descriptor *xmt_parse_descriptor(XMTParser *parser, char *name, GF_Descriptor *the_desc)
{
	char *str, desc_name[1024];
	GF_Descriptor *desc;
	u8 tag;
	u16 ES_ID;
	Bool has_link;

	if (!the_desc) {
		if (name) {
			strcpy(desc_name,  name);
		} else {
			str = xml_get_element(&parser->xml_parser);
			if (!str) return NULL;
			strcpy(desc_name, str);
		}
		tag = gf_odf_get_tag_by_name(desc_name);
		if (!tag) {
			xml_skip_element(&parser->xml_parser, desc_name);
			return xmt_parse_descriptor(parser, name, NULL);
		}
		desc = gf_odf_desc_new(tag);

		if (!desc) return NULL;
	} else {
		strcpy(desc_name,  name);
		desc = the_desc;
	}

	has_link = 0;
	ES_ID = 0;
	/*parse attributes*/
	while (xml_has_attributes(&parser->xml_parser)) {
		str = xml_get_attribute(&parser->xml_parser);
		if (!strcmp(str, "objectDescriptorID")) {
			xmt_new_od_link(parser, (GF_ObjectDescriptor *) desc, parser->xml_parser.value_buffer);
		} else if (!strcmp(str, "binaryID")) {
			if (desc->tag==GF_ODF_ESD_TAG) {
				ES_ID = atoi(parser->xml_parser.value_buffer);
				if (!ES_ID && !strnicmp(parser->xml_parser.value_buffer, "es", 2)) 
					ES_ID = atoi(parser->xml_parser.value_buffer + 2);
				if (has_link && ES_ID) xmt_new_esd_link(parser, (GF_ESD *) desc, NULL, ES_ID);
			} else {
				xmt_new_od_link(parser, (GF_ObjectDescriptor *) desc, parser->xml_parser.value_buffer);
			}
		} else if (!strcmp(str, "ES_ID")) {
			has_link = 1;
			xmt_new_esd_link(parser, (GF_ESD *) desc, parser->xml_parser.value_buffer, ES_ID);
		} else if (!strcmp(str, "OCR_ES_ID")) {
			xmt_set_dep_id(parser, (GF_ESD *) desc, parser->xml_parser.value_buffer, 1);
		} else if (!strcmp(str, "dependsOn_ES_ID")) {
			xmt_set_dep_id(parser, (GF_ESD *) desc, parser->xml_parser.value_buffer, 0);
		} else {
			if (!strcmp(str, "value")) {
				xmt_parse_descr_field(parser, desc, name, parser->xml_parser.value_buffer);
			} else {
				xmt_parse_descr_field(parser, desc, str, parser->xml_parser.value_buffer);
			}
		}
		if (parser->last_error) {
			gf_odf_desc_del(desc);
			return NULL;
		}
	}
	/*parse sub desc - WARNING: XMT defines some properties as elements in OD and also introduces dummy containers!!!*/
	while (!xml_element_done(&parser->xml_parser, desc_name) && !parser->last_error) {
		str = xml_get_element(&parser->xml_parser);
		/*special cases in IOD*/
		if (!strcmp(str, "Profiles")) xmt_parse_descriptor(parser, "Profiles", desc);
		/*special cases in OD/IOD*/
		else if (!strcmp(str, "Descr")) {
			xml_skip_attributes(&parser->xml_parser);
			while (!xml_element_done(&parser->xml_parser, "Descr") && !parser->last_error) {
				str = xml_get_element(&parser->xml_parser);
				if (str) xmt_parse_descr_field(parser, desc, str, NULL);
			}
		}
		/*special cases in BIFS config*/
		else if (!strcmp(str, "commandStream")) {
			xmt_parse_descriptor(parser, "commandStream", desc);
		}
		/*special cases OD URL*/
		else if (!strcmp(str, "URL")) xmt_parse_descriptor(parser, "URL", desc);
		else if (!strcmp(str, "size")) xmt_parse_descriptor(parser, "size", desc);
		else if (!strcmp(str, "predefined")) xmt_parse_descriptor(parser, "predefined", desc);
		else if (!strcmp(str, "custom")) xmt_parse_descriptor(parser, "custom", desc);
		else if (!strcmp(str, "MP4MuxHints")) xmt_parse_descriptor(parser, "MP4MuxHints", desc);
		/*all other desc*/
		else {
			xmt_parse_descr_field(parser, desc, str, NULL);
		}
	}
	if (desc->tag == GF_ODF_BIFS_CFG_TAG) {
		GF_BIFSConfig *bcfg = (GF_BIFSConfig *)desc;
		parser->pixelMetrics = bcfg->pixelMetrics;
		parser->load->ctx->scene_width = parser->bifs_w = bcfg->pixelWidth;
		parser->load->ctx->scene_height = parser->bifs_h = bcfg->pixelHeight;
		parser->load->ctx->is_pixel_metrics = bcfg->pixelMetrics;
		/*for xmt->bt*/
		if (!bcfg->version) bcfg->version = 1;
	}
	else if (desc->tag==GF_ODF_ESD_TAG) {
		GF_ESD *esd  =(GF_ESD*)desc;
		if (esd->decoderConfig) {
			switch (esd->decoderConfig->streamType) {
			case GF_STREAM_SCENE:
			case GF_STREAM_OD:
			{
			/*watchout for default BIFS stream*/
				if (parser->bifs_es && !parser->base_bifs_id && (esd->decoderConfig->streamType==GF_STREAM_SCENE)) {
					parser->bifs_es->ESID = parser->base_bifs_id = esd->ESID;
					parser->bifs_es->timeScale = (esd->slConfig && esd->slConfig->timestampResolution) ? esd->slConfig->timestampResolution : 1000;
				} else {
					GF_StreamContext *sc = gf_sm_stream_new(parser->load->ctx, esd->ESID, esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
					/*set default timescale for systems tracks (ignored for other)*/
					if (sc) sc->timeScale = (esd->slConfig && esd->slConfig->timestampResolution) ? esd->slConfig->timestampResolution : 1000;
					if (!parser->base_od_id && (esd->decoderConfig->streamType==GF_STREAM_OD)) parser->base_od_id = esd->ESID;
				}
			}
				break;
			}
		}
	}
	return desc;
}

/*
	locate node, if not defined yet parse ahead in current AU
	optimization: we actually peek ALL DEF NODES till end of AU
*/
GF_Node *xmt_peek_node(XMTParser *parser, char *defID)
{
	GF_Node *n, *the_node;
	u32 tag, ID;
	char *str;
	char nName[1000], elt[1000], aName[1000];
	u32 pos, line, current_pos, i, count;
	
	n = gf_sg_find_node_by_name(parser->load->scene_graph, defID);
	if (n) return n;
	count = gf_list_count(parser->peeked_nodes);
	for (i=0; i<count; i++) {
		n = gf_list_get(parser->peeked_nodes, i);
		if (!strcmp(n->sgprivate->NodeName, defID)) return n;
	}

	pos = parser->xml_parser.line_start_pos;
	current_pos = parser->xml_parser.current_pos;
	line = parser->xml_parser.line;
	strcpy(nName, defID);
	the_node = NULL;

	/*this is ugly, assumes DEF attribute is in the same text line than element...*/
	xml_skip_attributes(&parser->xml_parser);
	n = NULL;

	while (!parser->xml_parser.done) {
		if (strstr(parser->xml_parser.line_buffer, "<par") || strstr(parser->xml_parser.line_buffer, "</par>"))
			break;

		str = strstr(parser->xml_parser.line_buffer, " DEF=\"");
		if (!str) str = strstr(parser->xml_parser.line_buffer, " DEF=\'");
		if (!str) {
			parser->xml_parser.current_pos = parser->xml_parser.line_size;
			xml_check_line(&parser->xml_parser);
			continue;
		}
		str+=6;
		i = 0;
		while ((str[i] != '\"') && (str[i] != '\'') && str[i]) { aName[i] = str[i]; i++; }
		aName[i] = 0;

		str = strchr(parser->xml_parser.line_buffer, '<');
		if (!str || !strncmp(str+1, "ROUTE", 5)) {
			parser->xml_parser.current_pos = parser->xml_parser.line_size;
			xml_check_line(&parser->xml_parser);
			continue;
		}
		str+=1;
		i = 0;
		while ((str[i] != ' ') && str[i]) { elt[i] = str[i]; i++; }
		elt[i] = 0;
		if (!strcmp(elt, "ProtoInstance")) {
			str = strstr(str, "name=\"");
			if (!str) break;
			i = 0;
			str += 6;
			while ((str[i] != '\"') && str[i]) {
				elt[i] = str[i];
				i++;
			}
			elt[i] = 0;
		}
		tag = xmt_get_node_tag(parser, elt);
		if (!tag) {
			GF_Proto *p;
			GF_SceneGraph *sg = parser->load->scene_graph;
			while (1) {
				p = gf_sg_find_proto(sg, 0, elt);
				if (p) break;
				sg = sg->parent_scene;
				if (!sg) break;
			}
			if (!p) {
				/*locate proto*/
				xmt_report(parser, GF_BAD_PARAM, "%s: not a valid/supported node", elt);
				return NULL;
			}
			n = gf_sg_proto_create_instance(parser->load->scene_graph, p);
		} else {
			n = gf_node_new(parser->load->scene_graph, tag);
		}
		strcpy(parser->xml_parser.value_buffer, aName);
		ID = xmt_get_node_id(parser);
		if (n) {
			gf_node_set_id(n, ID, aName);
			if (!parser->parsing_proto) gf_node_init(n);
			gf_list_add(parser->peeked_nodes, n);
			if (!strcmp(aName, nName)) 
				the_node = n;
		}

		/*NO REGISTER on peek*/
		/*go on till end of AU*/
		parser->xml_parser.current_pos = parser->xml_parser.line_size;
		xml_check_line(&parser->xml_parser);
	}
	/*restore context*/
	parser->xml_parser.done = 0;
	gzrewind(parser->xml_parser.gz_in);
	gzseek(parser->xml_parser.gz_in, pos, SEEK_SET);
	parser->xml_parser.current_pos = parser->xml_parser.line_size;
	xml_check_line(&parser->xml_parser);
	parser->xml_parser.line = line;
	parser->xml_parser.current_pos = current_pos;
	return the_node;
}


u32 xmt_get_route(XMTParser *parser, char *name, Bool del_com) 
{
	u32 i;
	GF_Route *r = gf_sg_route_find_by_name(parser->load->scene_graph, name);
	if (r) return r->ID;
	for (i=0; i<gf_list_count(parser->inserted_routes); i++) {
		GF_Command *com = gf_list_get(parser->inserted_routes, i);
		if (com->def_name && !strcmp(com->def_name, name)) {
			if (del_com) gf_list_rem(parser->inserted_routes, i);
			return com->RouteID;
		}
	}
	return 0;
}

Bool xmt_route_id_used(XMTParser *parser, u32 ID)
{
	u32 i;
	GF_Route *r = gf_sg_route_find(parser->load->scene_graph, ID);
	if (r) return 1;
	for (i=0; i<gf_list_count(parser->inserted_routes); i++) {
		GF_Command *com = gf_list_get(parser->inserted_routes, i);
		if (com->RouteID == ID) return 1;
	}
	return 0;
}

void xmt_parse_route(XMTParser *parser, Bool is_insert, GF_Command *com)
{
	GF_Route *r;
	char *str, toN[1000], toNF[1000], fromN[1000], fromNF[1000], ID[1000];
	GF_Node *orig, *dest;
	GF_Err e;
	u32 rID;
	GF_FieldInfo orig_field, dest_field;

	ID[0] = toN[0] = toNF[0] = fromN[0] = fromNF[0] = 0;

	while (xml_has_attributes(&parser->xml_parser)) {
		str = xml_get_attribute(&parser->xml_parser);
		if (!strcmp(str, "fromNode")) strcpy(fromN, parser->xml_parser.value_buffer);
		else if (!strcmp(str, "fromField")) strcpy(fromNF, parser->xml_parser.value_buffer);
		else if (!strcmp(str, "toNode")) strcpy(toN, parser->xml_parser.value_buffer);
		else if (!strcmp(str, "toField")) strcpy(toNF, parser->xml_parser.value_buffer);
		else if (!strcmp(str, "DEF")) strcpy(ID, parser->xml_parser.value_buffer);
	}
	xml_element_done(&parser->xml_parser, "ROUTE");

	orig = xmt_peek_node(parser, fromN);
	if (!orig) {
		xmt_report(parser, GF_BAD_PARAM, "%s: Cannot find node", fromN);
		return;
	}
	e = gf_node_get_field_by_name(orig, fromNF, &orig_field);
	if ((e != GF_OK) && strstr(fromNF, "_changed")) {
		char *sz = strstr(fromNF, "_changed");
		sz[0] = 0;
		e = gf_node_get_field_by_name(orig, fromNF, &orig_field);
	}
	if (e!=GF_OK) {
		xmt_report(parser, GF_BAD_PARAM, "%s: Invalid node field", fromNF);
		return;
	}
	dest = xmt_peek_node(parser, toN);
	if (!dest) {
		xmt_report(parser, GF_BAD_PARAM, "%s: cannot find node", toN);
		return;
	}
	e = gf_node_get_field_by_name(dest, toNF, &dest_field);
	if ((e != GF_OK) && !strnicmp(toNF, "set_", 4)) e = gf_node_get_field_by_name(dest, &toNF[4], &dest_field);
	if (e != GF_OK) {
		xmt_report(parser, GF_BAD_PARAM, "%s: Invalid node field", toNF);
		return;
	}
	rID = 0;
	if (strlen(ID)) {
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
			com->def_name = strdup(ID);
			/*whenever not inserting in graph, keep track of max defined ID*/
			gf_sg_set_max_defined_route_id(parser->load->scene_graph, rID);
			if (rID>parser->load->ctx->max_route_id) parser->load->ctx->max_route_id = rID;

		}
		com->fromNodeID = orig->sgprivate->NodeID;
		com->fromFieldIndex = orig_field.fieldIndex;
		com->toNodeID = dest->sgprivate->NodeID;
		com->toFieldIndex = dest_field.fieldIndex;
		return;
	}
	r = gf_sg_route_new(parser->load->scene_graph, orig, orig_field.fieldIndex, dest, dest_field.fieldIndex);
	if (rID) {
		gf_sg_route_set_id(r, rID);
		gf_sg_route_set_name(r, ID);
	}
}

void xmt_resolve_routes(XMTParser *parser)
{
	GF_Command *com;
	/*resolve all commands*/
	while(gf_list_count(parser->unresolved_routes) ) {
		com = gf_list_get(parser->unresolved_routes, 0);
		gf_list_rem(parser->unresolved_routes, 0);
		switch (com->tag) {
		case GF_SG_ROUTE_DELETE:
		case GF_SG_ROUTE_REPLACE:
			com->RouteID = xmt_get_route(parser, com->unres_name, 0);
			if (!com->RouteID) {
				xmt_report(parser, GF_BAD_PARAM, "Cannot resolve GF_Route DEF %s", com->unres_name);
			}
			free(com->unres_name);
			com->unres_name = NULL;
			com->unresolved = 0;
			break;
		}
	}
	while (gf_list_count(parser->inserted_routes)) gf_list_rem(parser->inserted_routes, 0);
}

u32 xmt_get_od_id(XMTParser *parser, char *od_name)
{
	u32 i, ID;
	if (sscanf(od_name, "%d", &ID)==1) return ID;

	for (i=0; i<gf_list_count(parser->od_links); i++) {
		ODLink *l = gf_list_get(parser->od_links, i);
		if (!l->od) continue;
		if (l->desc_name && !strcmp(l->desc_name, od_name)) return l->od->objectDescriptorID;
	}
	return 0;
}

u32 xmt_get_esd_id(XMTParser *parser, char *esd_name)
{
	u32 i, ID;
	if (sscanf(esd_name, "%d", &ID)==1) return ID;

	for (i=0; i<gf_list_count(parser->esd_links); i++) {
		ESDLink *l = gf_list_get(parser->esd_links, i);
		if (!l->esd) continue;
		if (l->desc_name && !strcmp(l->desc_name, esd_name)) return l->esd->ESID;
	}
	return 0;
}

void proto_parse_field_dec(XMTParser *parser, GF_Proto *proto, Bool check_code)
{
	GF_FieldInfo info;
	GF_ProtoFieldInterface *pfield;
	char *szVal, *str;
	u32 fType, eType;
	char szFieldName[1024];

	if (check_code) {
		str = xml_get_element(&parser->xml_parser);
		if (strcmp(str, "field")) {
			xml_skip_element(&parser->xml_parser, str);
			return;
		}
	}
	szVal = NULL;
	fType = eType = 0;
	while (xml_has_attributes(&parser->xml_parser)) {
		str = xml_get_attribute(&parser->xml_parser);
		if (!strcmp(str, "name")) strcpy(szFieldName, parser->xml_parser.value_buffer);
		else if (!strcmp(str, "type")) fType = GetXMTFieldTypeByName(parser->xml_parser.value_buffer);
		else if (!strcmp(str, "vrml97Hint") || !strcmp(str, "accessType") ) eType = GetXMTEventTypeByName(parser->xml_parser.value_buffer);
		else if (strstr(str, "value") || strstr(str, "Value")) szVal = strdup(parser->xml_parser.value_buffer);
	}
	pfield = gf_sg_proto_field_new(proto, fType, eType, szFieldName);
	if (szVal) {
		gf_sg_proto_field_get_field(pfield, &info);
		str = parser->xml_parser.value_buffer;
		parser->temp_att = parser->xml_parser.value_buffer = szVal;
		if (gf_sg_vrml_is_sf_field(fType)) {
			xmt_sffield(parser, &info, NULL);
		} else {
			xmt_mffield(parser, &info, NULL);
		}
		parser->xml_parser.value_buffer = str;
		free(szVal);
		xml_element_done(&parser->xml_parser, "field");
	} else if (gf_sg_vrml_get_sf_type(fType)==GF_SG_VRML_SFNODE) {
		while (!xml_element_done(&parser->xml_parser, "field")) {
			xmt_parse_field_node(parser, NULL, &info);
		}
	} else {
		xml_element_done(&parser->xml_parser, "field");
		gf_sg_proto_field_set_value_undefined(pfield);
	}
}
void xmt_parse_proto(XMTParser *parser, GF_List *proto_list)
{
	GF_FieldInfo info;
	GF_Proto *proto, *prevproto;
	GF_SceneGraph *sg;
	char szName[1024];
	char *str, *extURL;
	u32 ID;

	extURL = NULL;
	while (xml_has_attributes(&parser->xml_parser)) {
		str = xml_get_attribute(&parser->xml_parser);
		if (!strcmp(str, "name")) strcpy(szName, parser->xml_parser.value_buffer);
		else if (!strcmp(str, "protoID")) ID = atoi(parser->xml_parser.value_buffer);
		else if (!strcmp(str, "locations")) extURL = strdup(parser->xml_parser.value_buffer);
	}
	ID = xmt_get_next_proto_id(parser);
	proto = gf_sg_proto_new(parser->load->scene_graph, ID, szName, proto_list ? 1 : 0);
	if (proto_list) gf_list_add(proto_list, proto);
	if (parser->load->ctx && (parser->load->ctx->max_proto_id<ID)) parser->load->ctx->max_proto_id=ID;

	prevproto = parser->parsing_proto;
	sg = parser->load->scene_graph;
	parser->parsing_proto = proto;
	parser->load->scene_graph = gf_sg_proto_get_graph(proto);

	/*parse all fields and proto code*/
	while (!xml_element_done(&parser->xml_parser, "ProtoDeclare")) {
		str = xml_get_element(&parser->xml_parser);
		/*proto field XMT style*/
		if (!strcmp(str, "field")) { 
			proto_parse_field_dec(parser, proto, 0);
		}
		/*proto field X3D style*/
		else if (!strcmp(str, "ProtoInterface")) { 
			xml_skip_attributes(&parser->xml_parser);
			while (!xml_element_done(&parser->xml_parser, "ProtoInterface")) {
				proto_parse_field_dec(parser, proto, 1);
			}
		}
		/*sub proto*/
		else if (!strcmp(str, "ProtoDeclare")) {
			xmt_parse_proto(parser, NULL);
		}
		/*route*/
		else if (!strcmp(str, "ROUTE")) {
			xmt_parse_route(parser, 0, NULL);
		}
		/*proto code X3D style*/
		else if (!strcmp(str, "ProtoBody")) { 
			s32 fieldIndex;
			xml_skip_attributes(&parser->xml_parser);
			while (!xml_element_done(&parser->xml_parser, "ProtoBody")) {
				GF_Node *n = xmt_parse_node(parser, NULL, NULL, &fieldIndex);
				gf_sg_proto_add_node_code(proto, n);
			}
		}
		/*proto code*/
		else {
			s32 fieldIndex;
			GF_Node *n = xmt_parse_node(parser, str, NULL, &fieldIndex);
			if (n) gf_sg_proto_add_node_code(proto, n);
			else if (parser->last_error) break;
		}
	}
	if (parser->last_error) {
		if (proto_list) gf_list_del_item(proto_list, proto); 
		gf_sg_proto_del(proto);
		parser->parsing_proto = prevproto;
		parser->load->scene_graph = sg;
		return;
	}

	if (extURL) {
		str = parser->xml_parser.value_buffer;
		parser->temp_att = parser->xml_parser.value_buffer = extURL;
		memset(&info, 0, sizeof(GF_FieldInfo));
		info.fieldType = GF_SG_VRML_MFURL;
		info.far_ptr = &proto->ExternProto;
		info.name = "ExternURL";
		xmt_mffield(parser, &info, NULL);
		parser->xml_parser.value_buffer = str;
		free(extURL);
	}

	xmt_resolve_routes(parser);
	parser->load->scene_graph = sg;
	parser->parsing_proto = prevproto;
}


static void xmt_set_com_node(GF_Command *com, GF_Node *node)
{
	com->node = node;
	gf_node_register(com->node, NULL);
}

void xmt_parse_command(XMTParser *parser, char *name, GF_List *com_list)
{
	char *str;
	GF_CommandField *field;
	GF_Command *com;
	u32 com_type;
	GF_FieldInfo inf;
	Bool hasField;
	s32 pos;
	u32 com_pos;
	char comName[50], fName[200], fVal[2000], extType[50];
	if (name) {
		strcpy(comName, name);
	} else {
		strcpy(comName, xml_get_element(&parser->xml_parser));
	}

	/*for ReplaceScene insertion*/
	com_pos = parser->bifs_au ? gf_list_count(parser->bifs_au->commands) : 0;

	/*BIFS commands*/
	if (!strcmp(comName, "Replace") || !strcmp(comName, "Insert") || !strcmp(comName, "Delete") 
		|| (!strcmp(comName, "Scene") && parser->is_x3d) ) {

		com = gf_sg_command_new(parser->load->scene_graph, GF_SG_UNDEFINED);
		parser->cur_com = com;

		if (!parser->stream_id) parser->stream_id = parser->base_bifs_id;

		if (!com_list) {
			if (!parser->bifs_es || (parser->bifs_es->ESID != parser->stream_id)) {
				GF_StreamContext *prev = parser->bifs_es;
				parser->bifs_es = gf_sm_stream_new(parser->load->ctx, (u16) parser->stream_id, GF_STREAM_SCENE, 0);
				/*force new AU if stream changed*/
				if (parser->bifs_es != prev) parser->bifs_au = NULL;
			}
			if (!parser->bifs_au) parser->bifs_au = gf_sm_stream_au_new(parser->bifs_es, 0, parser->au_time, parser->au_is_rap);
		}

		com_type = 0;
		if (!strcmp(comName, "Replace")) com_type = 0;
		else if (!strcmp(comName, "Insert")) com_type = 1;
		else if (!strcmp(comName, "Delete")) com_type = 2;

		/*parse attributes*/
		hasField = 0;
		/*default is END*/
		pos = -2;
		extType[0] = 0;
		while (xml_has_attributes(&parser->xml_parser)) {
			str = xml_get_attribute(&parser->xml_parser);
			if (!strcmp(str, "atNode")) {
				GF_Node *atNode = xmt_peek_node(parser, parser->xml_parser.value_buffer);
				if (!atNode ) {
					xmt_report(parser, GF_OK, "Cannot find node %s - skipping command", parser->xml_parser.value_buffer);
					xml_skip_element(&parser->xml_parser, comName);
					gf_sg_command_del(com);
					parser->cur_com = NULL;
					return;
				}
				xmt_set_com_node(com, atNode);
			}
			else if (!strcmp(str, "atField")) {
				strcpy(fName, parser->xml_parser.value_buffer);
				hasField = 1;
			}
			else if (!strcmp(str, "position")) {
				if (!strcmp(parser->xml_parser.value_buffer, "BEGIN")) pos = 0;
				else if (!strcmp(parser->xml_parser.value_buffer, "END")) pos = -1;
				else pos = atoi(parser->xml_parser.value_buffer);
			}
			else if (!strcmp(str, "value")) strcpy(fVal, parser->xml_parser.value_buffer);
			else if (!strcmp(str, "atRoute")) {
				u32 rID = xmt_get_route(parser, parser->xml_parser.value_buffer, 0);
				if (!rID) com->unres_name = strdup(parser->xml_parser.value_buffer);
				else {
					com->RouteID = rID;
					/*for bt<->xmt conversions*/
					com->def_name = strdup(parser->xml_parser.value_buffer);
				}
			}
			else if (!strcmp(str, "extended")) strcpy(extType, parser->xml_parser.value_buffer);
			else {
				xmt_report(parser, GF_BAD_PARAM, "Unknown command element %s", str);
				goto err;
			}
		}
		
		if (strlen(extType)) {
			if (!strcmp(extType, "globalQuant")) com->tag = GF_SG_GLOBAL_QUANTIZER;
			else if (!strcmp(extType, "fields")) com->tag = GF_SG_MULTIPLE_REPLACE;
			else if (!strcmp(extType, "indices")) com->tag = GF_SG_MULTIPLE_INDEXED_REPLACE;
			else if (!strcmp(extType, "deleteOrder")) com->tag = GF_SG_NODE_DELETE_EX;
			else if (!strcmp(extType, "allProtos")) com->tag = GF_SG_PROTO_DELETE_ALL;
			else if (!strcmp(extType, "proto") || !strcmp(extType, "protos")) {
				if (com_type == 1) {
					com->tag = GF_SG_PROTO_INSERT;
					com->new_proto_list = gf_list_new();
				} else {
					MFInt32 *IDs = gf_sg_vrml_field_pointer_new(GF_SG_VRML_MFINT32);
					inf.fieldType = GF_SG_VRML_MFINT32;
					inf.far_ptr = IDs;
					inf.name = "ProtoIDs";
					str = parser->xml_parser.value_buffer;
					parser->xml_parser.value_buffer = parser->temp_att = fVal;
					xmt_mffield(parser, &inf, NULL);
					parser->xml_parser.value_buffer = str;
					com->tag = GF_SG_PROTO_DELETE;
					com->del_proto_list = IDs->vals;
					com->del_proto_list_size = IDs->count;
					free(IDs);
				}
			}
			else {
				xmt_report(parser, GF_BAD_PARAM, "Unknown extended command %s", extType);
				goto err;
			}
		} else {
			switch (com_type) {
			case 0:
				if (com->node) {
					if (hasField && (pos > -2)) com->tag = GF_SG_INDEXED_REPLACE;
					else if (hasField) com->tag = GF_SG_FIELD_REPLACE;
					else com->tag = GF_SG_NODE_REPLACE;
				} else if (com->RouteID || (com->unres_name && strlen(com->unres_name)) ) {
					com->tag = GF_SG_ROUTE_REPLACE;
					if (!com->RouteID) {
						com->unresolved = 1;
						gf_list_add(parser->unresolved_routes, com);
					}
				} else {
					com->tag = GF_SG_SCENE_REPLACE;
				}
				break;
			case 1:
				if (com->node) {
					if (hasField) com->tag = GF_SG_INDEXED_INSERT;
					else com->tag = GF_SG_NODE_INSERT;
				} else {
					com->tag = GF_SG_ROUTE_INSERT;
				}
				break;
			case 2:
				if (com->node) {
					if (hasField) com->tag = GF_SG_INDEXED_DELETE;
					else com->tag = GF_SG_NODE_DELETE;
				} else {
					com->tag = GF_SG_ROUTE_DELETE;
					if (!com->RouteID) {
						com->unresolved = 1;
						gf_list_add(parser->unresolved_routes, com);
					}
				}
				break;
			}
		}

		field = NULL;
		if (com->node) {
			if (gf_node_get_field_by_name(com->node, fName, &inf) != GF_OK) {
			}
			/*simple commands*/
			if (hasField && !strlen(extType)) {
				field = gf_sg_command_field_new(com);
				field->fieldType = inf.fieldType;
				field->fieldIndex = inf.fieldIndex;
				if (gf_sg_vrml_get_sf_type(inf.fieldType) != GF_SG_VRML_SFNODE) {
					if (pos==-2) {
						str = parser->xml_parser.value_buffer;
						parser->xml_parser.value_buffer = fVal;
		
						field->field_ptr = gf_sg_vrml_field_pointer_new(inf.fieldType);
						inf.far_ptr = field->field_ptr;
						if (gf_sg_vrml_is_sf_field(inf.fieldType)) {
							parser->temp_att = parser->xml_parser.value_buffer;
							xmt_sffield(parser, &inf, com->node);
						} else {
							xmt_mffield(parser, &inf, com->node);
						}
						parser->xml_parser.value_buffer = str;
					} else {
						field->fieldType = inf.fieldType = gf_sg_vrml_get_sf_type(inf.fieldType);
						field->pos = pos;
						if (com->tag != GF_SG_INDEXED_DELETE) {
							str = parser->xml_parser.value_buffer;
							parser->xml_parser.value_buffer = fVal;

							field->field_ptr = gf_sg_vrml_field_pointer_new(inf.fieldType);
							inf.far_ptr = field->field_ptr;
							parser->temp_att = parser->xml_parser.value_buffer;
							xmt_sffield(parser, &inf, com->node);
							
							parser->xml_parser.value_buffer = str;
						}
					}
				}
			}
		}

		/*parse elements*/
		while (!xml_element_done(&parser->xml_parser, comName) && !parser->last_error) {
			str = NULL;
			if (!parser->is_x3d) str = xml_get_element(&parser->xml_parser);
			/*note that we register nodes*/
			switch (com->tag) {
			case GF_SG_SCENE_REPLACE:
				if (!parser->is_x3d && strcmp(str, "Scene")) {
					xmt_report(parser, GF_BAD_PARAM, "%s Unexpected symbol in scene replace", str);
					goto err;
				}
				/*if we have a previous scene*/
				xmt_resolve_routes(parser);
				while (gf_list_count(parser->def_nodes)) gf_list_rem(parser->def_nodes, 0);

				if (parser->is_x3d) {
					assert(com->node==NULL);
					com->node = gf_node_new(parser->load->scene_graph, (parser->load->flags & GF_SM_LOAD_MPEG4_STRICT) ? TAG_MPEG4_Group : TAG_X3D_Group);
					gf_node_register(com->node, NULL);
					gf_node_init(com->node );
				} else {
					while (xml_has_attributes(&parser->xml_parser)) {
						str = xml_get_attribute(&parser->xml_parser);
						com->use_names = 0;
						if (!strcmp(str, "USENAMES") && !strcmp(parser->xml_parser.value_buffer, "true")) com->use_names = 1;
					}
				}

				while (!xml_element_done(&parser->xml_parser, "Scene") && !parser->last_error) {
					str = xml_get_element(&parser->xml_parser);
					/*NULL*/
					if (!strcmp(str, "NULL")) {
					}
					/*proto */
					else if (!strcmp(str, "ProtoDeclare")) {
						xmt_parse_proto(parser, com->new_proto_list);
					}
					/*route*/
					else if (!strcmp(str, "ROUTE")) {
						GF_Command *sgcom = gf_sg_command_new(parser->load->scene_graph, GF_SG_ROUTE_INSERT);
						gf_list_add(parser->bifs_au->commands, sgcom);
						xmt_parse_route(parser, 0, sgcom);
						if (sgcom->RouteID) gf_list_add(parser->inserted_routes, sgcom);
					}
					/*top node*/
					else {
						if (parser->is_x3d) {
							GF_Node *n = xmt_parse_node(parser, str, NULL, NULL);
							if (n) gf_node_insert_child(com->node, n, -1);
						} else {
							if (com->node) {
								xmt_report(parser, GF_BAD_PARAM, "More than one top node specified");
								goto err;
							}
							com->node = xmt_parse_node(parser, str, NULL, NULL);
						}
					}
				}
				if (!parser->last_error && parser->is_x3d) parser->last_error = GF_EOS;
				break;
			case GF_SG_NODE_REPLACE:
				field = gf_sg_command_field_new(com);
				field->fieldType = GF_SG_VRML_SFNODE;
				if (!strcmp(str, "NULL")) {
					field->new_node = NULL;
				} else {
					field->new_node = xmt_parse_node(parser, str, NULL, NULL);
				}
				field->field_ptr = &field->new_node;
				break;
			case GF_SG_INDEXED_REPLACE:
				assert(field);
				field->fieldType = GF_SG_VRML_SFNODE;
				field->pos = pos;
				if (!strcmp(str, "NULL")) {
					field->new_node = NULL;
				} else {
					field->new_node = xmt_parse_node(parser, str, NULL, NULL);
				}
				field->field_ptr = &field->new_node;
				break;
			case GF_SG_FIELD_REPLACE:
				assert(field);
				if (field->fieldType == GF_SG_VRML_SFNODE) {
					if (!strcmp(str, "NULL")) {
						field->new_node = NULL;
					} else {
						field->new_node = xmt_parse_node(parser, str, NULL, NULL);
					}
					field->field_ptr = &field->new_node;
				} else if (field->fieldType == GF_SG_VRML_MFNODE) {
					if (!field->node_list) {
						field->node_list = gf_list_new();
						field->field_ptr = &field->node_list;
					}
					if (strcmp(str, "NULL")) {
						GF_Node *n = xmt_parse_node(parser, str, NULL, NULL);
						gf_list_add(field->node_list, n);
					}
				} else {
					assert(0);
				}
				break;
			case GF_SG_NODE_INSERT:
				field = gf_sg_command_field_new(com);
				/*fall through*/
			case GF_SG_INDEXED_INSERT:
				field->fieldType = GF_SG_VRML_SFNODE;
				if (!strcmp(str, "NULL")) {
					field->new_node = NULL;
				} else {
					field->new_node = xmt_parse_node(parser, str, NULL, NULL);
				}
				field->field_ptr = &field->new_node;
				field->pos = pos;
				break;
			case GF_SG_ROUTE_REPLACE:
				xmt_parse_route(parser, 0, com);
				break;
			case GF_SG_ROUTE_INSERT:
				xmt_parse_route(parser, 1, com);
				break;
			case GF_SG_GLOBAL_QUANTIZER:
				com->node = NULL;
				field = gf_sg_command_field_new(com);
				if (!strcmp(str, "NULL")) {
					field->new_node = NULL;
				} else {
					field->new_node = xmt_parse_node(parser, str, NULL, NULL);
				}
				field->field_ptr = &field->new_node;
				field->fieldType = GF_SG_VRML_SFNODE;
				break;
			case GF_SG_MULTIPLE_REPLACE:
			{
				char szName[1024];
				char *szVal;
				if (strcmp(str, "repField")) {
					xmt_report(parser, GF_BAD_PARAM, "%s: Unexpected symbol in multiple replace", str);
					goto err;
				}
				szVal = NULL;
				while (xml_has_attributes(&parser->xml_parser)) {
					str = xml_get_attribute(&parser->xml_parser);
					if (!strcmp(str, "atField")) strcpy(szName, parser->xml_parser.value_buffer);
					if (!strcmp(str, "value")) szVal = strdup(parser->xml_parser.value_buffer);
				}
				if (szVal) {
					if (gf_node_get_field_by_name(com->node, szName, &inf) != GF_OK) {
					} else {
						field = gf_sg_command_field_new(com);
						inf.far_ptr = field->field_ptr = gf_sg_vrml_field_pointer_new(inf.fieldType);
						field->fieldType = inf.fieldType;
						field->fieldIndex = inf.fieldIndex;
						str = parser->xml_parser.value_buffer;
						parser->temp_att = parser->xml_parser.value_buffer = szVal;
						if (gf_sg_vrml_is_sf_field(inf.fieldType)) {
							xmt_sffield(parser, &inf, NULL);
						} else {
							xmt_mffield(parser, &inf, NULL);
						}
						parser->xml_parser.value_buffer = str;
						free(szVal);
					}
				}
				/*nodes*/
				while (!xml_element_done(&parser->xml_parser, "repField")) {
					str = xml_get_element(&parser->xml_parser);
					strcpy(szName, str);
					if (gf_node_get_field_by_name(com->node, szName, &inf) != GF_OK) {
						xml_skip_element(&parser->xml_parser, str);
					} else {
						xml_skip_attributes(&parser->xml_parser);
						field = gf_sg_command_field_new(com);
						field->fieldIndex = inf.fieldIndex;
						field->fieldType = inf.fieldType;
						if (inf.fieldType == GF_SG_VRML_SFNODE) {
							field->new_node = xmt_parse_node(parser, NULL, NULL, NULL);
							field->field_ptr = &field->new_node;
							xml_element_done(&parser->xml_parser, szName);
						} else {
							field->node_list = gf_list_new();
							field->field_ptr = &field->node_list;
							while (!xml_element_done(&parser->xml_parser, szName)) {
								GF_Node *n = xmt_parse_node(parser, NULL, NULL, NULL);
								if (n) gf_list_add(field->node_list, n);
							}
						}
					}

				}
			}
				break;
			case GF_SG_MULTIPLE_INDEXED_REPLACE:
			{
				char szName[1024];
				char *szVal;
				if (strcmp(str, "repValue")) {
					xmt_report(parser, GF_BAD_PARAM, "%s: Unexpected symbol in multiple replace", str);
					goto err;
				}
				szVal = NULL;
				while (xml_has_attributes(&parser->xml_parser)) {
					str = xml_get_attribute(&parser->xml_parser);
					if (!strcmp(str, "position")) {
						if (!strcmp(parser->xml_parser.value_buffer, "BEGIN")) pos = 0;
						else if (!strcmp(parser->xml_parser.value_buffer, "END")) pos = -1;
						else pos = atoi(parser->xml_parser.value_buffer);
					}
					if (!strcmp(str, "value")) szVal = strdup(parser->xml_parser.value_buffer);
				}
				if (szVal) {
					field = gf_sg_command_field_new(com);
					inf.fieldType = field->fieldType = gf_sg_vrml_get_sf_type(inf.fieldType);
					inf.far_ptr = field->field_ptr = gf_sg_vrml_field_pointer_new(field->fieldType);
					field->fieldIndex = inf.fieldIndex;
					field->pos = pos;
					str = parser->xml_parser.value_buffer;
					parser->temp_att = parser->xml_parser.value_buffer = szVal;
					xmt_sffield(parser, &inf, NULL);
					parser->xml_parser.value_buffer = str;
					free(szVal);
				}
				/*nodes*/
				while (!xml_element_done(&parser->xml_parser, "repValue")) {
					str = xml_get_element(&parser->xml_parser);
					strcpy(szName, str);
					if (gf_node_get_field_by_name(com->node, szName, &inf) != GF_OK) {
						xml_skip_element(&parser->xml_parser, str);
					} else {
						xml_skip_attributes(&parser->xml_parser);
						field = gf_sg_command_field_new(com);
						field->pos = pos;
						field->fieldIndex = inf.fieldIndex;
						field->fieldType = GF_SG_VRML_SFNODE;
						field->new_node = xmt_parse_node(parser, NULL, NULL, NULL);
						field->field_ptr = &field->new_node;
						xml_element_done(&parser->xml_parser, szName);
					}

				}
			}
				break;
			case GF_SG_PROTO_INSERT:
				if (strcmp(str, "ProtoDeclare")) {
					xmt_report(parser, GF_BAD_PARAM, "%s: Unexpected symbol in ProtoInsert", str);
					goto err;
				}
				xmt_parse_proto(parser, com->new_proto_list);
				break;
			}
		}
		if (parser->last_error==GF_EOS) parser->last_error = GF_OK;

		if (com_list) gf_list_add(com_list, com);
		else {
			/*ROUTEs have been inserted*/
			if (com->tag==GF_SG_SCENE_REPLACE) {
				gf_list_insert(parser->bifs_au->commands, com, com_pos);
			} else {
				gf_list_add(parser->bifs_au->commands, com);
			}
		}
		if (com->tag==GF_SG_ROUTE_INSERT) gf_list_add(parser->inserted_routes, com);

		return;
err:
		gf_sg_command_del(com);
		return;
	}
	/*OD commands*/
	if (!strcmp(comName, "ObjectDescriptorUpdate") || !strcmp(comName, "ObjectDescriptorRemove")
			|| !strcmp(comName, "ES_DescriptorUpdate") || !strcmp(comName, "ES_DescriptorRemove")
			|| !strcmp(comName, "IPMP_DescriptorUpdate") || !strcmp(comName, "IPMP_DescriptorRemove") ) {
		
		if (!parser->stream_id) parser->stream_id = parser->base_od_id;
		else if (parser->bifs_es && parser->bifs_es->ESID==parser->stream_id) parser->stream_id = parser->base_od_id;

		if (!parser->od_es) parser->od_es = gf_sm_stream_new(parser->load->ctx, (u16) parser->stream_id, GF_STREAM_OD, 0);
		if (!parser->od_au) {
			parser->od_au = gf_sm_stream_au_new(parser->od_es, 0, parser->au_time, parser->au_is_rap);
			if (gf_list_count(parser->od_es->AUs)==1) parser->od_au->is_rap = 1;
		}

		if (!strcmp(comName, "ObjectDescriptorUpdate") ) {
			GF_ODUpdate *odU;
			xml_skip_attributes(&parser->xml_parser);
			while (1) {
				str = xml_get_element(&parser->xml_parser);
				if (!str) {
					xmt_report(parser, GF_BAD_PARAM, "Expecting <OD> in <ObjectDescriptorUpdate>");
					return;
				}
				if (!strcmp(str, "OD")) break;
				xml_skip_element(&parser->xml_parser, str);
			}
			xml_skip_attributes(&parser->xml_parser);
			odU = (GF_ODUpdate *) gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);
			while (!xml_element_done(&parser->xml_parser, "OD")) {
				GF_Descriptor *od = xmt_parse_descriptor(parser, NULL, NULL);
				if (od) gf_list_add(odU->objectDescriptors, od);
				else if (parser->last_error) {
					gf_odf_com_del((GF_ODCom **) &odU);
					return;
				}
			}
			if (!xml_element_done(&parser->xml_parser, "ObjectDescriptorUpdate")) {
				xmt_report(parser, GF_BAD_PARAM, "Expecting </ObjectDescriptorUpdate>");
				return;
			}
			gf_list_add(parser->od_au->commands, odU);
			return;
		}
		if (!strcmp(comName, "ObjectDescriptorRemove") ) {
			GF_ODRemove *odR;
			odR = (GF_ODRemove *) gf_odf_com_new(GF_ODF_OD_REMOVE_TAG);
			while (xml_has_attributes(&parser->xml_parser)) {
				str = xml_get_attribute(&parser->xml_parser);
				if (!strcmp(str, "objectDescriptorId")) {
					u32 i, j, start;
					char szBuf[100];
					i = j = start = 0;
					while (parser->xml_parser.value_buffer[start+i]) {
						j = 0;
						while (parser->xml_parser.value_buffer[start+i] && (parser->xml_parser.value_buffer[start+i] == ' ')) i++;
						while (parser->xml_parser.value_buffer[start+i] && (parser->xml_parser.value_buffer[start+i] != ' ')) {
							szBuf[j] = parser->xml_parser.value_buffer[start+i];
							i++;
							j++;
						}
						szBuf[j] = 0;
						start += i;
						if (parser->xml_parser.value_buffer[start] == ' ') start ++;
						i = 0;
						j = xmt_get_od_id(parser, szBuf);
						if (!j) {
							xmt_report(parser, GF_BAD_PARAM, "%s: Cannot find ObjectDescriptor", szBuf);
							gf_odf_com_del((GF_ODCom **) &odR);
							return;
						}
						odR->OD_ID = realloc(odR->OD_ID, sizeof(u16) * (odR->NbODs+1));
						odR->OD_ID[odR->NbODs] = j;
						odR->NbODs++;
					}
				}
			}
			xml_element_done(&parser->xml_parser, "ObjectDescriptorRemove");

			gf_list_add(parser->od_au->commands, odR);
			return;
		}
		if (!strcmp(comName, "ES_DescriptorRemove") ) {
			GF_ESDRemove *esdR;
			esdR = (GF_ESDRemove *) gf_odf_com_new(GF_ODF_ESD_REMOVE_TAG);
			while (xml_has_attributes(&parser->xml_parser)) {
				str = xml_get_attribute(&parser->xml_parser);
				if (!strcmp(str, "objectDescriptorId")) {
					esdR->ODID = xmt_get_od_id(parser, parser->xml_parser.value_buffer);
				} else if (!strcmp(str, "ES_ID")) {
					u32 i, j, start;
					char szBuf[100];
					i = j = start = 0;
					while (parser->xml_parser.value_buffer[start+i]) {
						j = 0;
						while (parser->xml_parser.value_buffer[start+i] && (parser->xml_parser.value_buffer[start+i] == ' ')) i++;
						while (parser->xml_parser.value_buffer[start+i] && (parser->xml_parser.value_buffer[start+i] != ' ')) {
							szBuf[j] = parser->xml_parser.value_buffer[start+i];
							i++;
							j++;
						}
						szBuf[j] = 0;
						start += i;
						if (parser->xml_parser.value_buffer[start] == ' ') start ++;
						i = 0;
						j = xmt_get_esd_id(parser, szBuf);
						if (!j) {
							xmt_report(parser, GF_BAD_PARAM, "%s: Cannot find GF_ESD", szBuf);
							gf_odf_com_del((GF_ODCom **) &esdR);
							return;
						}
						esdR->ES_ID = realloc(esdR->ES_ID, sizeof(u16) * (esdR->NbESDs+1));
						esdR->ES_ID[esdR->NbESDs] = j;
						esdR->NbESDs++;
					}
				}
			}
			xml_element_done(&parser->xml_parser, "ES_DescriptorRemove");
			gf_list_add(parser->od_au->commands, esdR);
			return;
		}
		/*IPMP descriptor update*/
		if (!strcmp(comName, "IPMP_DescriptorUpdate")) {
			GF_IPMPUpdate *ipU;
			xml_skip_attributes(&parser->xml_parser);
			while (1) {
				str = xml_get_element(&parser->xml_parser);
				if (!str) {
					xmt_report(parser, GF_BAD_PARAM, "Expecting <ipmpDesc> in <IPMP_DescriptorUpdate>");
					return;
				}
				if (!strcmp(str, "ipmpDesc")) break;
				xml_skip_element(&parser->xml_parser, str);
			}
			xml_skip_attributes(&parser->xml_parser);

			ipU = (GF_IPMPUpdate *) gf_odf_com_new(GF_ODF_IPMP_UPDATE_TAG);

			while (!xml_element_done(&parser->xml_parser, "ipmpDesc")) {
				GF_Descriptor *desc = xmt_parse_descriptor(parser, NULL, NULL);
				if (desc) gf_list_add(ipU->IPMPDescList, desc);
				else if (parser->last_error) {
					gf_odf_com_del((GF_ODCom **) &ipU);
					return;
				}
			}
			if (!xml_element_done(&parser->xml_parser, "IPMP_DescriptorUpdate")) {
				xmt_report(parser, GF_BAD_PARAM, "Expecting </IPMP_DescriptorUpdate>");
				return;
			}
			gf_list_add(parser->od_au->commands, ipU);
			return;
		}
	}
	/*not found*/
	xmt_report(parser, GF_OK, "Unknown command name %s - skipping", comName);
	xml_skip_element(&parser->xml_parser, comName);
}

Bool xmt_esid_available(XMTParser *parser, u16 ESID) 
{
	u32 i;
	for (i=0; i<gf_list_count(parser->esd_links); i++) {
		ESDLink *esdl = gf_list_get(parser->esd_links, i);
		if (esdl->esd->ESID == ESID) return 0;
	}
	return 1;
}
Bool xmt_odid_available(XMTParser *parser, u16 ODID) 
{
	u32 i;
	for (i=0; i<gf_list_count(parser->od_links); i++) {
		ODLink *l = gf_list_get(parser->od_links, i);
		if (l->ID == ODID) return 0;
		if (l->od && l->od->objectDescriptorID == ODID) return 0;
	}
	return 1;
}

void xmt_resolve_od(XMTParser *parser)
{
	u32 i, j;
	ODLink *l;
	char szURL[5000];

	/*fix ESD IDs*/
	for (i=0; i<gf_list_count(parser->esd_links); i++) {
		ESDLink *esdl = gf_list_get(parser->esd_links, i);
		if (!esdl->esd) {
			xmt_report(parser, GF_BAD_PARAM, "Stream %s ID %d has no associated ES descriptor\n", esdl->desc_name ? esdl->desc_name : "", esdl->ESID);
			gf_list_rem(parser->esd_links, i);
			if (esdl->desc_name) free(esdl->desc_name);
			free(esdl);
			i--;
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
	for (i=0; i<gf_list_count(parser->esd_links); i++) {
		Bool use_old_fmt;
		u16 ocr_id;
		char szTest[50];
		ESDLink *esdl = gf_list_get(parser->esd_links, i);
		esdl->esd->OCRESID = 0;
		if (!esdl->OCR_Name) continue;
		
		use_old_fmt = 0;
		ocr_id = atoi(esdl->OCR_Name);
		sprintf(szTest, "%d", ocr_id);
		if (!stricmp(szTest, esdl->OCR_Name)) use_old_fmt = 1;

		for (j=0; j<gf_list_count(parser->esd_links); j++) {
			ESDLink *esdl2 = gf_list_get(parser->esd_links, j);
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
		free(esdl->OCR_Name);
		esdl->OCR_Name = NULL;
	}

	/*set dependsOn es ids*/
	for (i=0; i<gf_list_count(parser->esd_links); i++) {
		Bool use_old_fmt;
		u16 dep_id;
		char szTest[50];
		ESDLink *esdl = gf_list_get(parser->esd_links, i);
		esdl->esd->dependsOnESID = 0;
		if (!esdl->Depends_Name) continue;
		
		use_old_fmt = 0;
		dep_id = atoi(esdl->Depends_Name);
		sprintf(szTest, "%d", dep_id);
		if (!stricmp(szTest, esdl->Depends_Name)) use_old_fmt = 1;

		for (j=0; j<gf_list_count(parser->esd_links); j++) {
			ESDLink *esdl2 = gf_list_get(parser->esd_links, j);
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
		free(esdl->Depends_Name);
		esdl->Depends_Name = NULL;
	}

	while (gf_list_count(parser->esd_links)) {
		ESDLink *esdl = gf_list_get(parser->esd_links, 0);
		gf_list_rem(parser->esd_links, 0);
		if (esdl->desc_name) free(esdl->desc_name);
		free(esdl);
	}

	for (i=0; i<gf_list_count(parser->od_links); i++) {
		l = gf_list_get(parser->od_links, i);
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
	for (i=0; i<gf_list_count(parser->od_links); i++) {
		l = gf_list_get(parser->od_links, i);
		/*not OD URL*/
		if (!l->ID) continue;
		for (j=i+1; j<gf_list_count(parser->od_links); j++) {
			ODLink *l2 = gf_list_get(parser->od_links, j);
			/*not OD URL*/
			if (!l2->ID) continue;
			if (l->ID == l2->ID) {
				while (gf_list_count(l2->nodes)) {
					GF_Node *n = gf_list_get(l2->nodes, 0);
					gf_list_rem(l2->nodes, 0);
					gf_list_add(l->nodes, n);
				}
				gf_list_rem(parser->od_links, j);
				j--;
				if (l2->desc_name) free(l2->desc_name);
				gf_list_del(l2->nodes);
				free(l2);
			}
		}
	}

	while (gf_list_count(parser->od_links) ) {
		l = gf_list_get(parser->od_links, 0);
		if (!l->od) {
			/*if no ID found this is not an OD URL*/
			if (l->ID) {
				if (l->desc_name) {
					xmt_report(parser, GF_OK, "WARNING: OD \"%s\" (ID %d) not assigned", l->desc_name, l->ID);
				} else{
					xmt_report(parser, GF_OK, "WARNING: OD ID %d not assigned", l->ID);
				}
			}
		} else {
			for (j=0; j<gf_list_count(l->nodes); j++) {
				GF_FieldInfo info;
				GF_Node *n = gf_list_get(l->nodes, j);
				if (gf_node_get_field_by_name(n, "url", &info) == GF_OK) {
					u32 k;
					MFURL *url = (MFURL *)info.far_ptr;
					for (k=0; k<url->count; k++) {
						char *seg = NULL;
						if (url->vals[k].url) seg = strstr(url->vals[k].url, "#");
						if (seg) {
							sprintf(szURL, "od:%d#%s", l->od->objectDescriptorID, seg+1);
							free(url->vals[k].url);
							url->vals[k].url = strdup(szURL);
						} else {
							if (url->vals[k].url) free(url->vals[k].url);
							url->vals[k].url = NULL;
							url->vals[k].OD_ID = l->od->objectDescriptorID;
						}
					}
				}
			}
		}

		if (l->desc_name) free(l->desc_name);
		gf_list_del(l->nodes);
		free(l);
		gf_list_rem(parser->od_links, 0);
	}
}

GF_Err gf_sm_load_run_XMT_Intern(GF_SceneLoader *load, Bool break_at_first_par)
{
	char *str;
	Bool is_resume, is_par;
	XMTParser *parser = (XMTParser *)load->loader_priv;
	if (!parser) return GF_BAD_PARAM;

	xml_check_line(&parser->xml_parser);
	if (parser->xml_parser.done) return GF_OK;

	is_resume = parser->resume_is_par;
	while (!xml_element_done(&parser->xml_parser, "Body") && !parser->last_error) {
		str = NULL;
		is_par = is_resume;
		if (!is_resume) str = xml_get_element(&parser->xml_parser);
		if (str && !strcmp(str, "par")) is_par = 1;
		
		if (is_par && break_at_first_par) {
			parser->resume_is_par = 1;
			return GF_OK;
		}
		
		/*when using chunk parsing*/
		if (str && (parser->load->flags & GF_SM_LOAD_CONTEXT_READY)) {
			if (!stricmp(str, "Header")) {
				xml_skip_element(&parser->xml_parser, "Header");
				continue;
			}
			else if (!stricmp(str, "Body")) {
				xml_skip_attributes(&parser->xml_parser);
				continue;
			}
		}

		is_resume = 0;
		/*ALWAYS explicit command here*/
		parser->stream_id = 0;

		while (is_par && xml_has_attributes(&parser->xml_parser)) {
			str = xml_get_attribute(&parser->xml_parser);
			if (!strcmp(str, "begin")) {
				parser->au_time = atof(parser->xml_parser.value_buffer);
			}
			else if (!strcmp(str, "atES_ID")) {
				parser->stream_id = xmt_locate_stream(parser, parser->xml_parser.value_buffer);
				if (!parser->stream_id) xmt_report(parser, GF_BAD_PARAM, "Cannot find stream %s targeted by command", parser->xml_parser.value_buffer);
			}
		}
		/*reset context - note we don't resolve OD/ESD links untill everything is parsed to make sure we don't remove them*/
		if (parser->od_au && (parser->od_au->timing_sec != parser->au_time)) {
			parser->od_au = NULL;
		}

		if (parser->bifs_au && (parser->bifs_au->timing_sec != parser->au_time)) {
			parser->bifs_au = NULL;
		}

		/*parse all commands context*/
		if (is_par) {
			while (!xml_element_done(&parser->xml_parser, "par") && !parser->last_error) {
				xmt_parse_command(parser, NULL, NULL);
			}
		} else {
		  assert(str);
			xmt_parse_command(parser, str, NULL);
		}
	}
	if (!parser->last_error && !xml_element_done(&parser->xml_parser, "XMT-A")) {
		xmt_report(parser, GF_BAD_PARAM, "Expecting </XMT-A> in XMT-A document");
	}

	xmt_resolve_routes(parser);
	xmt_resolve_od(parser);
	return parser->last_error;
}

GF_Err gf_sm_load_run_XMT(GF_SceneLoader *load)
{
	return gf_sm_load_run_XMT_Intern(load, 0);
}


void gf_sm_load_done_XMT(GF_SceneLoader *load)
{
	XMTParser *parser = (XMTParser *)load->loader_priv;
	if (!parser) return;
	xmt_resolve_routes(parser);
	gf_list_del(parser->unresolved_routes);
	gf_list_del(parser->inserted_routes);
	gf_list_del(parser->def_nodes);
	gf_list_del(parser->peeked_nodes);
	xmt_resolve_od(parser);
	gf_list_del(parser->od_links);
	gf_list_del(parser->esd_links);
	if (parser->xml_parser.value_buffer) free(parser->xml_parser.value_buffer);
	gzclose(parser->xml_parser.gz_in);
	free(parser);
	load->loader_priv = NULL;
}

GF_Err gf_sm_load_init_XMT(GF_SceneLoader *load)
{
	Bool is_done;
	char *str;
	GF_Err e;
	XMTParser *parser;

	if (!load->ctx || !load->fileName) return GF_BAD_PARAM;
	GF_SAFEALLOC(parser, sizeof(XMTParser));

	parser->load = load;
	e = xml_init_parser(&parser->xml_parser, load->fileName);
	if (e) {
		xmt_report(parser, e, "Unable to open file %s", load->fileName);
		free(parser);
		return e;
	}
	load->loader_priv = parser;
	parser->unresolved_routes = gf_list_new();
	parser->inserted_routes = gf_list_new();
	parser->od_links = gf_list_new();
	parser->esd_links = gf_list_new();
	parser->def_nodes = gf_list_new();
	parser->peeked_nodes = gf_list_new();

	/*chunk parsing*/
	if (load->flags & GF_SM_LOAD_CONTEXT_READY) {
		u32 i;
		if (!load->ctx) {
			gf_sm_load_done_XMT(load);
			return GF_BAD_PARAM;
		}
		
		/*restore context - note that base layer are ALWAYS declared BEFORE enhancement layers with gpac parsers*/
		for (i=0; i<gf_list_count(load->ctx->streams); i++) {
			GF_StreamContext *sc = gf_list_get(load->ctx->streams, 0); 
			switch (sc->streamType) {
			case GF_STREAM_SCENE: if (!parser->bifs_es) parser->bifs_es = sc; break;
			case GF_STREAM_OD: if (!parser->od_es) parser->od_es = sc; break;
			default: break;
			}
		}
		/*need at least one scene stream*/
		if (!parser->bifs_es) {
			gf_sm_load_done_XMT(load);
			return GF_BAD_PARAM;
		}
		parser->base_bifs_id = parser->bifs_es->ESID;
		if (parser->od_es) parser->base_od_id = parser->od_es->ESID; 

		if (load->OnMessage) load->OnMessage(load->cbk, "MPEG-4 (XMT-A) Scene Chunk Parsing", GF_OK);
		else fprintf(stdout, "MPEG-4 (XMT-A) Scene Chunk Parsing\n");
	} else {
		/*create at least one empty BIFS stream*/
		parser->bifs_es = gf_sm_stream_new(load->ctx, 0, GF_STREAM_SCENE, 0);
		parser->bifs_au = gf_sm_stream_au_new(parser->bifs_es, 0, 0, 1);
	}


	/*check XMT-A doc*/
	while (1) {
		str = xml_get_element(&parser->xml_parser);
		if (!str) {
			xmt_report(parser, GF_BAD_PARAM, "Invalid XMT-A document");
			goto exit;
		}
		if (!strcmp(str, "XMT-A")) break;
		if (!strcmp(str, "X3D")) {
			parser->is_x3d = 1;
			break;
		}
		if (!strcmp(str, "!DOCTYPE")) 
			xml_skip_attributes(&parser->xml_parser);
		else
			xml_skip_element(&parser->xml_parser, str);
	}
	xml_skip_attributes(&parser->xml_parser);

	parser->xml_parser.OnProgress = load->OnProgress;
	parser->xml_parser.cbk = load->cbk;

	/*chunk parsing, don't look for HEADER*/
	if (load->flags & GF_SM_LOAD_CONTEXT_READY) return GF_OK;

	if (parser->is_x3d) {
		if (load->OnMessage) load->OnMessage(load->cbk, "X3D (XML) Scene Parsing", GF_OK);
		else fprintf(stdout, "X3D (XML) Scene Parsing\n");

		/*parse all commands*/
		parser->au_time = 0;
		parser->au_is_rap = 1;
		is_done = 1;
		while (!xml_element_done(&parser->xml_parser, "X3D") && !parser->last_error) {
			str = xml_get_element(&parser->xml_parser);
			/*in X3D only get scene*/
			if (!strcmp(str, "Scene")) {
				parser->stream_id = 0;
				xmt_parse_command(parser, str, NULL);
				parser->au_is_rap = 0;
			} else {
				xml_skip_element(&parser->xml_parser, str);
			}
		}
	} else {
		if (load->OnMessage) load->OnMessage(load->cbk, "MPEG-4 (XMT-A) Scene Parsing", GF_OK);
		else fprintf(stdout, "MPEG-4 (XMT-A) Scene Parsing\n");
		/*check header*/
		while (1) {
			str = xml_get_element(&parser->xml_parser);
			if (!str) {
				xmt_report(parser, GF_BAD_PARAM, "Expecting <Header> in XMT-A document");
				goto exit;
			}
			if (!strcmp(str, "Header")) break;
			xml_skip_element(&parser->xml_parser, str);
		}
		xml_skip_attributes(&parser->xml_parser);
		parser->load->ctx->root_od = (GF_ObjectDescriptor *) xmt_parse_descriptor(parser, NULL, NULL);
		if (!xml_element_done(&parser->xml_parser, "Header")) {
			xmt_report(parser, GF_BAD_PARAM, "Expecting </Header> in XMT-A document");
			goto exit;
		}

		/*check body*/
		while (1) {
			str = xml_get_element(&parser->xml_parser);
			if (!str) {
				xmt_report(parser, GF_BAD_PARAM, "Expecting <Body> in XMT-A document");
				goto exit;
			}
			if (!strcmp(str, "Body")) break;
			xml_skip_element(&parser->xml_parser, str);
		}
		xml_skip_attributes(&parser->xml_parser);

		/*parse all commands*/
		parser->au_time = 0;
		parser->stream_id = 0;
		e = gf_sm_load_run_XMT_Intern(load, 1);
		is_done = parser->xml_parser.done;
	}
	if (is_done) parser->xml_parser.done = 1;

exit:
	e = parser->last_error;
	if (e) gf_sm_load_done(load);
	return e;
}
GF_Err gf_sm_load_init_XMTString(GF_SceneLoader *load, char *str)
{
	GF_Err e = GF_OK;
#ifdef 	gf_sm_load_init_XMTString_HAS_BEEN_VALIDATED
	XMTParser *parser;
	GF_Command *com;

	if (!load || (!load->ctx && !load->scene_graph) || !str) return GF_BAD_PARAM;
	if (!load->scene_graph) load->scene_graph = load->ctx->scene_graph;

	parser = (XMTParser *)malloc(sizeof(XMTParser)); 
	if (parser) memset(parser, 0, sizeof(XMTParser));
	else 
		return GF_OUT_OF_MEM;

	parser->is_wrl = 0;
	parser->load = load;
	/*we suppose an ascii string*/
	parser->unicode_type = 0;

	parser->line_buffer = str;
	parser->line_size = (s32)strlen(str);
	parser->gz_in = NULL;

	load->loader_priv = parser;

	parser->unresolved_routes = gf_list_new();
	parser->inserted_routes = gf_list_new();
	parser->undef_nodes = gf_list_new();
	parser->def_nodes = gf_list_new();

	/*chunk parsing*/
	if (load->flags & GF_SM_LOAD_CONTEXT_READY) {
		u32 i;
		if (!load->ctx) {
			gf_sm_load_done_XMT(load);
			return GF_BAD_PARAM;
		}
		
		/*restore context - note that base layer are ALWAYS declared BEFORE enhancement layers with gpac parsers*/
		for (i=0; i<gf_list_count(load->ctx->streams); i++) {
			GF_StreamContext *sc = (GF_StreamContext *)gf_list_get(load->ctx->streams, 0); 
			switch (sc->streamType) {
			case GF_STREAM_SCENE: if (!parser->bifs_es) parser->bifs_es = sc; break;
			case GF_STREAM_OD: if (!parser->od_es) parser->od_es = sc; break;
			default: break;
			}
		}
		/*need at least one scene stream*/
		if (!parser->bifs_es) {
			gf_sm_load_done_XMT(load);
			return GF_BAD_PARAM;
		}
		parser->base_bifs_id = parser->bifs_es->ESID;
		if (parser->od_es) parser->base_od_id = parser->od_es->ESID; 
		/*that's it, nothing else to do*/
		return GF_OK;
	}

	/*create at least one empty BIFS stream*/
	parser->bifs_es = gf_sm_stream_new(load->ctx, 0, GF_STREAM_SCENE, 0);
	parser->bifs_au = gf_sm_stream_au_new(parser->bifs_es, 0, 0, 1);

	/*default scene replace - we create it no matter what since it is used to store BIFS config
	when parsing IOD.*/
	com = gf_sg_command_new(parser->load->scene_graph, GF_SG_SCENE_REPLACE);
	gf_list_add(parser->bifs_au->commands, com);
	e = gf_sm_load_run_XMT_intern(parser, com);
	if (e) gf_sm_load_done_XMT(load);
	return e;
#endif
	return e;
}

GF_Err gf_sm_load_done_XMTString(GF_SceneLoader *load)
{
	XMTParser *parser = (XMTParser *)load->loader_priv;
	if (!parser) return GF_OK;
	gf_list_del(parser->unresolved_routes);
	gf_list_del(parser->inserted_routes);
	gf_list_del(parser->def_nodes);
	free(parser);
	load->loader_priv = NULL;
	return GF_OK;
}
