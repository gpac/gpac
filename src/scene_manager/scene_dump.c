/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_x3d.h>
#include <gpac/nodes_svg.h>
#include <gpac/events.h>
#include <gpac/base_coding.h>

#ifndef __SYMBIAN32__
#include <wchar.h>
#endif

#ifndef GPAC_DISABLE_SCENE_DUMP

/*for QP types*/
#include "../bifs/quant.h"

struct _scenedump
{
	/*the scene we're dumping - set at each SceneReplace or mannually*/
	GF_SceneGraph *sg;
#ifndef GPAC_DISABLE_VRML
	/*the proto we're dumping*/
	GF_Proto *current_proto;
#endif
	FILE *trace;
	u32 indent;
	char *filename;

	GF_SceneDumpFormat dump_mode;
	u16 CurrentESID;
	u8 ind_char;
	Bool XMLDump, X3DDump, LSRDump;

	GF_List *dump_nodes;

	/*nodes created through conditionals while parsing but not applied*/
	GF_List *mem_def_nodes;

	Bool skip_scene_replace;
	/*for route insert/replace in conditionals in current scene replace*/
	GF_List *current_com_list;
	GF_List *inserted_routes;

	Bool in_text;
};

static GF_Err gf_dump_vrml_route(GF_SceneDumper *sdump, GF_Route *r, u32 dump_type);
static void gf_dump_vrml_node(GF_SceneDumper *sdump, GF_Node *node, Bool in_list, char *fieldContainer);

#ifndef GPAC_DISABLE_SVG
void gf_dump_svg_element(GF_SceneDumper *sdump, GF_Node *n, GF_Node *parent, Bool is_root);
#endif

GF_EXPORT
GF_SceneDumper *gf_sm_dumper_new(GF_SceneGraph *graph, char *_rad_name, Bool is_final_name, char indent_char, GF_SceneDumpFormat dump_mode)
{
	GF_SceneDumper *tmp;
	if (!graph) return NULL;
	GF_SAFEALLOC(tmp, GF_SceneDumper);
	if (!tmp) return NULL;

	/*store original*/
	tmp->dump_mode = dump_mode;

#ifndef GPAC_DISABLE_SVG
	if ((graph->RootNode && (graph->RootNode->sgprivate->tag>=GF_NODE_RANGE_LAST_VRML) )
	        || (dump_mode==GF_SM_DUMP_LASER) || (dump_mode==GF_SM_DUMP_SVG)) {
		tmp->XMLDump = GF_TRUE;
		if (dump_mode==GF_SM_DUMP_LASER) {
			tmp->LSRDump = GF_TRUE;
		}
		if (_rad_name) {
			const char* ext_name = tmp->LSRDump ? ".xsr" : ".svg";
			tmp->filename = (char *)gf_malloc(strlen(_rad_name) + strlen(ext_name) + 1);
			strcpy(tmp->filename, _rad_name);
			if (!is_final_name) strcat(tmp->filename, ext_name);
			tmp->trace = gf_fopen(tmp->filename, "wt");
			if (!tmp->trace) {
				gf_free(tmp);
				return NULL;
			}
		} else {
			tmp->trace = stdout;
		}
	} else
#endif
	{

		if (dump_mode==GF_SM_DUMP_AUTO_TXT) {
			if (!graph->RootNode || (graph->RootNode->sgprivate->tag<=GF_NODE_RANGE_LAST_MPEG4) ) {
				dump_mode = GF_SM_DUMP_BT;
			} else if (graph->RootNode->sgprivate->tag<=GF_NODE_RANGE_LAST_X3D) {
				dump_mode = GF_SM_DUMP_X3D_VRML;
			}
		}
		else if (dump_mode==GF_SM_DUMP_AUTO_XML) {
			if (!graph->RootNode || (graph->RootNode->sgprivate->tag<=GF_NODE_RANGE_LAST_MPEG4) ) {
				dump_mode = GF_SM_DUMP_XMTA;
			} else {
				dump_mode = GF_SM_DUMP_X3D_XML;
			}
		}

		if (_rad_name) {
			const char* ext_name;
			switch (dump_mode) {
			case GF_SM_DUMP_X3D_XML:
				ext_name = ".x3d";
				tmp->XMLDump = GF_TRUE;
				tmp->X3DDump = GF_TRUE;
				break;
			case GF_SM_DUMP_XMTA:
				ext_name = ".xmt";
				tmp->XMLDump = GF_TRUE;
				break;
			case GF_SM_DUMP_X3D_VRML:
				ext_name = ".x3dv";
				tmp->X3DDump = GF_TRUE;
				break;
			case GF_SM_DUMP_VRML:
				ext_name = ".wrl";
				break;
			default:
				ext_name = ".bt";
				break;
			}

			tmp->filename = (char *)gf_malloc(strlen(_rad_name ? _rad_name : "") + strlen(ext_name) + 1);
			strcpy(tmp->filename, _rad_name ? _rad_name : "");
			if (!is_final_name) strcat(tmp->filename, ext_name);
			tmp->trace = gf_fopen(tmp->filename, "wt");
			if (!tmp->trace) {
				gf_free(tmp->filename);
				gf_free(tmp);
				return NULL;
			}
		} else {
			tmp->trace = stdout;
			switch (dump_mode) {
			case GF_SM_DUMP_X3D_XML:
				tmp->XMLDump = GF_TRUE;
				tmp->X3DDump = GF_TRUE;
				break;
			case GF_SM_DUMP_XMTA:
				tmp->XMLDump = GF_TRUE;
				break;
			case GF_SM_DUMP_X3D_VRML:
				tmp->X3DDump = GF_TRUE;
				break;
			default:
				break;
			}
		}
	}
	tmp->ind_char = indent_char;
	tmp->dump_nodes = gf_list_new();
	tmp->mem_def_nodes = gf_list_new();
	tmp->inserted_routes = gf_list_new();
	tmp->sg = graph;
	return tmp;
}

GF_EXPORT
void gf_sm_dumper_set_extra_graph(GF_SceneDumper *sdump, GF_SceneGraph *extra)
{
	sdump->sg = extra;
}

GF_EXPORT
void gf_sm_dumper_del(GF_SceneDumper *sdump)
{
	gf_list_del(sdump->dump_nodes);
	while (gf_list_count(sdump->mem_def_nodes)) {
		GF_Node *tmp = (GF_Node *)gf_list_get(sdump->mem_def_nodes, 0);
		gf_list_rem(sdump->mem_def_nodes, 0);
		gf_node_unregister(tmp, NULL);
	}
	gf_list_del(sdump->mem_def_nodes);
	gf_list_del(sdump->inserted_routes);
	if (sdump->trace != stdout) gf_fclose(sdump->trace);
	if (sdump->filename) {
		gf_free(sdump->filename);
		sdump->filename = NULL;
	}
	gf_free(sdump);
}

char *gf_sm_dump_get_name(GF_SceneDumper *bd)
{
	if (!bd) return NULL;
	return bd->filename;
}

static void gf_dump_setup(GF_SceneDumper *sdump, GF_Descriptor *root_od)
{
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		if (sdump->dump_mode==GF_SM_DUMP_XML) return;
		gf_fprintf(sdump->trace, "<!-- %s Scene Dump -->\n",
		        (sdump->dump_mode==GF_SM_DUMP_SVG) ? "SVG" :
		        (sdump->dump_mode==GF_SM_DUMP_LASER) ? "LASeR" :
		        sdump->X3DDump ? "X3D" : "XMT-A"
		       );
	}
	if (sdump->dump_mode==GF_SM_DUMP_SVG) return;
	if (sdump->LSRDump) {
		gf_fprintf(sdump->trace, "<saf:SAFSession xmlns:saf=\"urn:mpeg:mpeg4:SAF:2005\" >\n");
#ifndef GPAC_DISABLE_OD_DUMP
		if (root_od) {
			GF_ObjectDescriptor *iod = (GF_ObjectDescriptor *)root_od;
			u32 i, count;
			gf_fprintf(sdump->trace, "<saf:sceneHeader>\n");
			count = gf_list_count(iod->ESDescriptors);
			for (i=0; i<count; i++) {
				GF_LASERConfig lsrcfg;
				GF_ESD *esd = (GF_ESD *)gf_list_get(iod->ESDescriptors, i);
				if (!esd || !esd->decoderConfig) continue;
				if (esd->decoderConfig->streamType != GF_STREAM_SCENE) continue;
				if (esd->decoderConfig->objectTypeIndication != 0x09) continue;
				if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) continue;
				gf_odf_get_laser_config(esd->decoderConfig->decoderSpecificInfo, &lsrcfg);
				gf_odf_dump_desc((GF_Descriptor*)&lsrcfg, sdump->trace, 1, 1);
			}
			gf_fprintf(sdump->trace, "</saf:sceneHeader>\n");
		}
#endif
		return;
	}

	if (!sdump->X3DDump) {
		/*setup XMT*/
		if (sdump->XMLDump) {
			gf_fprintf(sdump->trace, "<XMT-A xmlns=\"urn:mpeg:mpeg4:xmta:schema:2002\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"urn:mpeg:mpeg4:xmta:schema:2002 xmt-a.xsd\">\n");
			gf_fprintf(sdump->trace, " <Header>\n");
#ifndef GPAC_DISABLE_OD_DUMP
			if (root_od) gf_odf_dump_desc(root_od, sdump->trace, 1, 1);
#endif
			gf_fprintf(sdump->trace, " </Header>\n");
			gf_fprintf(sdump->trace, " <Body>\n");
			if (!root_od) {
				gf_fprintf(sdump->trace, "  <Replace>\n");
			}
		} else {
			if (sdump->dump_mode==GF_SM_DUMP_VRML) {
				gf_fprintf(sdump->trace, "#VRML V2.0\n");
			} else {
				/*dump root OD*/
#ifndef GPAC_DISABLE_OD_DUMP
				if (root_od) gf_odf_dump_desc(root_od, sdump->trace, 0, 0);
#endif
			}
			gf_fprintf(sdump->trace, "\n");
		}
	} else {
		if (sdump->XMLDump) {
			gf_fprintf(sdump->trace, "<!DOCTYPE X3D PUBLIC \"ISO//Web3D//DTD X3D 3.0//EN\" \"http://www.web3d.org/specifications/x3d-3.0.dtd\">\n");
			gf_fprintf(sdump->trace, "<X3D xmlns:xsd=\"http://www.w3.org/2001/XMLSchema-instance\" xsd:noNamespaceSchemaLocation=\"http://www.web3d.org/specifications/x3d-3.0.xsd\" version=\"3.0\">\n");
			gf_fprintf(sdump->trace, "<head>\n");
			gf_fprintf(sdump->trace, "<meta content=\"X3D File Converted/Dumped by GPAC Version %s - %s\" name=\"generator\"/>\n", gf_gpac_version(), gf_gpac_copyright() );
			gf_fprintf(sdump->trace, "</head>\n");
			gf_fprintf(sdump->trace, " <Scene>\n");
		} else {
			gf_fprintf(sdump->trace, "#X3D V3.0\n\n");
		}
	}
}

static void gf_dump_finalize(GF_SceneDumper *sdump, GF_Descriptor *root_od)
{
	if (sdump->dump_mode==GF_SM_DUMP_SVG) return;

	if (sdump->LSRDump) {
		gf_fprintf(sdump->trace, "<saf:endOfSAFSession/>\n</saf:SAFSession>\n");
		return;
	}
	if (!sdump->XMLDump) return;

	if (!sdump->X3DDump) {
		if (!root_od) {
			gf_fprintf(sdump->trace, "  </Replace>\n");
		}
		gf_fprintf(sdump->trace, " </Body>\n");
		gf_fprintf(sdump->trace, "</XMT-A>\n");
	} else {
		gf_fprintf(sdump->trace, " </Scene>\n");
		gf_fprintf(sdump->trace, "</X3D>\n");
	}
}

static Bool gf_dump_vrml_is_def_node(GF_SceneDumper *sdump, GF_Node *node)
{
	s32 i = gf_list_find(sdump->dump_nodes, node);
	if (i>=0) return 0;
	gf_list_add(sdump->dump_nodes, node);
	return 1;
}

static GF_Node *gf_dump_find_node(GF_SceneDumper *sdump, u32 ID)
{
	GF_Node *ret = gf_sg_find_node(sdump->sg, ID);
	if (ret) return ret;
	return NULL;
}

#define DUMP_IND(sdump)	\
	if (sdump->trace) {		\
		u32 z;	\
		for (z=0; z<sdump->indent; z++) gf_fprintf(sdump->trace, "%c", sdump->ind_char);	\
	}


static void StartElement(GF_SceneDumper *sdump, const char *name)
{
	if (!sdump->trace) return;
	DUMP_IND(sdump);
	if (!sdump->XMLDump) {
		gf_fprintf(sdump->trace, "%s {\n", name);
	} else {
		gf_fprintf(sdump->trace, "<%s", name);
	}
}

static void EndElementHeader(GF_SceneDumper *sdump, Bool has_sub_el)
{
	if (!sdump->trace) return;
	if (sdump->XMLDump) {
		if (has_sub_el) {
			gf_fprintf(sdump->trace, ">\n");
		} else {
			gf_fprintf(sdump->trace, "/>\n");
		}
	}
}

static void EndElement(GF_SceneDumper *sdump, const char *name, Bool had_sub_el)
{
	if (!sdump->trace) return;
	if (!sdump->XMLDump) {
		DUMP_IND(sdump);
		gf_fprintf(sdump->trace, "}\n");
	} else {
		if (had_sub_el) {
			DUMP_IND(sdump);
			gf_fprintf(sdump->trace, "</%s>\n", name);
		}
	}
}

static void StartAttribute(GF_SceneDumper *sdump, const char *name)
{
	if (!sdump->trace) return;
	if (!sdump->XMLDump) {
		DUMP_IND(sdump);
		gf_fprintf(sdump->trace, "%s ", name);
	} else {
		gf_fprintf(sdump->trace, " %s=\"", name);
	}
}

static void EndAttribute(GF_SceneDumper *sdump)
{
	if (!sdump->trace) return;
	if (!sdump->XMLDump) {
		gf_fprintf(sdump->trace, "\n");
	} else {
		gf_fprintf(sdump->trace, "\"");
	}
}


static void StartList(GF_SceneDumper *sdump, const char *name)
{
	if (!sdump->trace) return;
	DUMP_IND(sdump);
	if (!sdump->XMLDump) {
		if (name)
			gf_fprintf(sdump->trace, "%s [\n", name);
		else
			gf_fprintf(sdump->trace, "[\n");
	} else {
		gf_fprintf(sdump->trace, "<%s>\n", name);
	}
}

static void EndList(GF_SceneDumper *sdump, const char *name)
{
	if (!sdump->trace) return;
	DUMP_IND(sdump);
	if (!sdump->XMLDump) {
		gf_fprintf(sdump->trace, "]\n");
	} else {
		gf_fprintf(sdump->trace, "</%s>\n", name);
	}
}


static void scene_dump_utf_string(GF_SceneDumper *sdump, Bool escape_xml, char *str)
{
	u32 len, i;
	u16 *uniLine;
	if (!str) return;
	len = (u32) strlen(str);
	if (!len) return;
	uniLine = (u16*)gf_malloc(sizeof(u16) * len*4);
	len = gf_utf8_mbstowcs(uniLine, len, (const char **) &str);
	if (len != GF_UTF8_FAIL) {
		for (i=0; i<len; i++) {
			//if (uniLine[i] == (u16) '\"') gf_fprintf(sdump->trace, "\\");
			switch (uniLine[i]) {
			case '\'':
				if (escape_xml) gf_fprintf(sdump->trace, "&apos;");
				else gf_fprintf(sdump->trace, "'");
				break;
			case '\"':
				if (escape_xml) gf_fprintf(sdump->trace, "&quot;");
				else gf_fprintf(sdump->trace, "\"");
				break;
			case '&':
				gf_fprintf(sdump->trace, "&amp;");
				break;
			case '>':
				gf_fprintf(sdump->trace, "&gt;");
				break;
			case '<':
				gf_fprintf(sdump->trace, "&lt;");
				break;
			case '\r':
			case '\n':
				/* Does nothing : gf_fprintf(sdump->trace, "");, fflush instead ?*/
				break;
			default:
				if (uniLine[i]<128) {
					gf_fprintf(sdump->trace, "%c", (u8) uniLine[i]);
				} else {
					gf_fprintf(sdump->trace, "&#%d;", uniLine[i]);
				}
				break;
			}
		}
	}
	gf_free(uniLine);
}

#ifndef GPAC_DISABLE_VRML

static void scene_dump_vrml_id(GF_SceneDumper *sdump, GF_Node *node)
{
	u32 id;
	const char *node_name;
	if (!sdump->trace) return;
	/*FIXME - optimize id/name fetch*/
	node_name = gf_node_get_name_and_id(node, &id);
	if (node_name)
		gf_fprintf(sdump->trace, "%s", node_name);
	else
		gf_fprintf(sdump->trace, "N%d", id - 1);
}

static Bool scene_dump_vrml_find_route_name(GF_SceneDumper *sdump, u32 ID, const char **outName)
{
	GF_Route *r;
	u32 i;
	GF_Command *com;
	r = gf_sg_route_find(sdump->sg, ID);
	if (r) {
		(*outName) = r->name;
		return 1;
	}

	i=0;
	while ((com = (GF_Command *)gf_list_enum(sdump->inserted_routes, &i))) {
		if (com->tag == GF_SG_ROUTE_INSERT) {
			if (com->RouteID==ID) {
				(*outName) = com->def_name;
				return 1;
			}
		}
	}
	if (!sdump->current_com_list) return 0;
	i=1;
	while ((com = (GF_Command *)gf_list_enum(sdump->current_com_list, &i))) {
		if ((com->tag == GF_SG_ROUTE_INSERT) || (com->tag == GF_SG_ROUTE_REPLACE)) {
			if (com->RouteID==ID) {
				(*outName) = com->def_name;
				return 1;
			}
		} else return 0;
	}
	return 0;
}

static void scene_dump_vrml_route_id(GF_SceneDumper *sdump, u32 routeID, char *rName)
{
	if (!sdump->trace) return;
	if (!rName) scene_dump_vrml_find_route_name(sdump, routeID, (const char **) &rName);

	if (rName)
		gf_fprintf(sdump->trace, "%s", rName);
	else
		gf_fprintf(sdump->trace, "R%d", routeID - 1);
}


static void gf_dump_vrml_sffield(GF_SceneDumper *sdump, u32 type, void *ptr, Bool is_mf, GF_Node *node)
{
	switch (type) {
	case GF_SG_VRML_SFBOOL:
		gf_fprintf(sdump->trace, "%s", * ((SFBool *)ptr) ? "true" : "false");
		break;
	case GF_SG_VRML_SFINT32:
		gf_fprintf(sdump->trace, "%d", * ((SFInt32 *)ptr) );
		break;
	case GF_SG_VRML_SFFLOAT:
		gf_fprintf(sdump->trace, "%g", FIX2FLT( * ((SFFloat *)ptr) ) );
		break;
	case GF_SG_VRML_SFDOUBLE:
		gf_fprintf(sdump->trace, "%g", * ((SFDouble *)ptr) );
		break;
	case GF_SG_VRML_SFTIME:
		gf_fprintf(sdump->trace, "%g", * ((SFTime *)ptr) );
		break;
	case GF_SG_VRML_SFCOLOR:
		gf_fprintf(sdump->trace, "%g %g %g", FIX2FLT( ((SFColor *)ptr)->red ), FIX2FLT( ((SFColor *)ptr)->green ), FIX2FLT( ((SFColor *)ptr)->blue ));
		break;
	case GF_SG_VRML_SFCOLORRGBA:
		gf_fprintf(sdump->trace, "%g %g %g %g", FIX2FLT( ((SFColorRGBA *)ptr)->red ), FIX2FLT( ((SFColorRGBA *)ptr)->green ), FIX2FLT( ((SFColorRGBA *)ptr)->blue ), FIX2FLT( ((SFColorRGBA *)ptr)->alpha ));
		break;
	case GF_SG_VRML_SFVEC2F:
		gf_fprintf(sdump->trace, "%g %g", FIX2FLT( ((SFVec2f *)ptr)->x ), FIX2FLT( ((SFVec2f *)ptr)->y ));
		break;
	case GF_SG_VRML_SFVEC2D:
		gf_fprintf(sdump->trace, "%g %g", ((SFVec2d *)ptr)->x, ((SFVec2d *)ptr)->y);
		break;
	case GF_SG_VRML_SFVEC3F:
		gf_fprintf(sdump->trace, "%g %g %g", FIX2FLT( ((SFVec3f *)ptr)->x ), FIX2FLT( ((SFVec3f *)ptr)->y ), FIX2FLT( ((SFVec3f *)ptr)->z ));
		break;
	case GF_SG_VRML_SFVEC3D:
		gf_fprintf(sdump->trace, "%g %g %g", ((SFVec3d *)ptr)->x, ((SFVec3d *)ptr)->y, ((SFVec3d *)ptr)->z);
		break;
	case GF_SG_VRML_SFROTATION:
		gf_fprintf(sdump->trace, "%g %g %g %g", FIX2FLT( ((SFRotation *)ptr)->x ), FIX2FLT( ((SFRotation *)ptr)->y ), FIX2FLT( ((SFRotation *)ptr)->z ), FIX2FLT( ((SFRotation *)ptr)->q ) );
		break;

	case GF_SG_VRML_SFATTRREF:
	{
		SFAttrRef *ar = (SFAttrRef *)ptr;
		if (ar->node) {
			GF_FieldInfo pinfo;
			gf_node_get_field(ar->node, ar->fieldIndex, &pinfo);
			scene_dump_vrml_id(sdump, ar->node);
			gf_fprintf(sdump->trace, ".%s", pinfo.name);
		}
	}
	break;
	case GF_SG_VRML_SFSCRIPT:
	{
		u32 len, i;
		char *str;
		str = (char*)((SFScript *)ptr)->script_text;
		if (!str) {
			if (!sdump->XMLDump) {
				gf_fprintf(sdump->trace, "\"\"");
			}
			break;
		}
		len = (u32)strlen(str);

		if (!sdump->XMLDump) {
			gf_fprintf(sdump->trace, "\"%s\"", str);
		}
		else {
			u16 *uniLine = (u16*)gf_malloc(sizeof(u16) * ((len/2)*2 + 2));
			len = gf_utf8_mbstowcs(uniLine, len+1, (const char **)&str);

			if (len != GF_UTF8_FAIL) {
				for (i = 0; i<len; i++) {

					switch (uniLine[i]) {
					case '&':
						gf_fprintf(sdump->trace, "&amp;");
						break;
					case '<':
						gf_fprintf(sdump->trace, "&lt;");
						break;
					case '>':
						gf_fprintf(sdump->trace, "&gt;");
						break;
					case '\'':
					case '"':
						gf_fprintf(sdump->trace, "&apos;");
						break;
					case 0:
						break;
						/*FIXME: how the heck can we preserve newlines and spaces of JavaScript in
						an XML attribute in any viewer ? */
					default:
						if (uniLine[i]<128) {
							gf_fprintf(sdump->trace, "%c", (u8)uniLine[i]);
						}
						else {
							gf_fprintf(sdump->trace, "&#%d;", uniLine[i]);
						}
						break;
					}
			}
		}
			gf_free(uniLine);
	}
		DUMP_IND(sdump);
	}
	break;

	case GF_SG_VRML_SFSTRING:
	{
		char *str;
		if (sdump->XMLDump) {
			if (is_mf) gf_fprintf(sdump->trace, sdump->X3DDump ? "\"" : "&quot;");
		} else {
			gf_fprintf(sdump->trace, "\"");
		}
		/*dump in unicode*/
		str = ((SFString *)ptr)->buffer;

		if (node && (gf_node_get_tag(node)==TAG_MPEG4_BitWrapper)) {
			u32 bufsize = 37 + ((M_BitWrapper*)node)->buffer_len * 2 + 3;
			str = gf_malloc(sizeof(char) * bufsize);
			if (str) {
				s32 res;
				strcpy(str, "data:application/octet-string;base64,");
				res = gf_base64_encode(((M_BitWrapper*)node)->buffer.buffer, ((M_BitWrapper*)node)->buffer_len, str+37, bufsize-37);
				if (res<0) {
					gf_free(str);
					str = NULL;
				} else {
					str[res+37] = 0;
				}
			}
		}
		if (str && str[0]) {
			if (sdump->XMLDump) {
				scene_dump_utf_string(sdump, 1, str);
			} else if (!strchr(str, '\"')) {
				gf_fprintf(sdump->trace, "%s", str);
			} else {
				u32 i, len = (u32)strlen(str);
				for (i=0; i<len; i++) {
					if (str[i]=='\"') gf_fputc('\\', sdump->trace);
					gf_fputc(str[i], sdump->trace);
				}
			}
		}
		if (node && (gf_node_get_tag(node)==TAG_MPEG4_BitWrapper)) {
			if (str) gf_free(str);
		}

		if (sdump->XMLDump) {
			if (is_mf) gf_fprintf(sdump->trace, sdump->X3DDump ? "\"" : "&quot;");
		} else {
			gf_fprintf(sdump->trace, "\"");
		}
	}
	break;

	case GF_SG_VRML_SFURL:
		if (((SFURL *)ptr)->url) {
#if 0
			u32 len;
			char *str;
			short uniLine[5000];
			str = ((SFURL *)ptr)->url;
			len = gf_utf8_mbstowcs(uniLine, 5000, (const char **) &str);
			if (len != GF_UTF8_FAIL) {
				gf_fprintf(sdump->trace, sdump->XMLDump ? (sdump->X3DDump ?  "'" : "&quot;") : "\"");
				fwprintf(sdump->trace, (unsigned short *) uniLine);
				gf_fprintf(sdump->trace, sdump->XMLDump ? (sdump->X3DDump ?  "'" : "&quot;") : "\"");
			}
#else
			gf_fprintf(sdump->trace, sdump->XMLDump ? (sdump->X3DDump ?  "'" : "&quot;") : "\"");
			gf_fprintf(sdump->trace, "%s", ((SFURL *)ptr)->url);
			gf_fprintf(sdump->trace, sdump->XMLDump ? (sdump->X3DDump ?  "'" : "&quot;") : "\"");
#endif
		} else {
			if (sdump->XMLDump) {
				gf_fprintf(sdump->trace, "&quot;od://od%d&quot;", ((SFURL *)ptr)->OD_ID);
			} else {
				gf_fprintf(sdump->trace, "od:%d", ((SFURL *)ptr)->OD_ID);
			}
		}
		break;
	case GF_SG_VRML_SFIMAGE:
	{
		u32 i, count;
		SFImage *img = (SFImage *)ptr;
		gf_fprintf(sdump->trace, "%d %d %d", img->width, img->height, img->numComponents);
		count = img->width * img->height * img->numComponents;
		for (i=0; i<count; ) {
			switch (img->numComponents) {
			case 1:
				gf_fprintf(sdump->trace, " 0x%02X", img->pixels[i]);
				i++;
				break;
			case 2:
				gf_fprintf(sdump->trace, " 0x%02X%02X", img->pixels[i], img->pixels[i+1]);
				i+=2;
				break;
			case 3:
				gf_fprintf(sdump->trace, " 0x%02X%02X%02X", img->pixels[i], img->pixels[i+1], img->pixels[i+2]);
				i+=3;
				break;
			case 4:
				gf_fprintf(sdump->trace, " 0x%02X%02X%02X%02X", img->pixels[i], img->pixels[i+1], img->pixels[i+2], img->pixels[i+3]);
				i+=4;
				break;
			}
		}
	}
	break;
	}
}


static void gf_dump_vrml_simple_field(GF_SceneDumper *sdump, GF_FieldInfo field, GF_Node *parent)
{
	u32 i, sf_type;
	GF_ChildNodeItem *list;
	void *slot_ptr;

	switch (field.fieldType) {
	case GF_SG_VRML_SFNODE:
		gf_dump_vrml_node(sdump, field.far_ptr ? *(GF_Node **)field.far_ptr : NULL, 0, NULL);
		return;
	case GF_SG_VRML_MFNODE:
		list = * ((GF_ChildNodeItem **) field.far_ptr);
		gf_assert( list );
		sdump->indent++;
		while (list) {
			gf_dump_vrml_node(sdump, list->node, 1, NULL);
			list = list->next;
		}
		sdump->indent--;
		return;
	case GF_SG_VRML_SFCOMMANDBUFFER:
		return;
	}
	if (gf_sg_vrml_is_sf_field(field.fieldType)) {
		if (sdump->XMLDump) StartAttribute(sdump, "value");
		if (field.far_ptr)
			gf_dump_vrml_sffield(sdump, field.fieldType, field.far_ptr, 0, parent);
		if (sdump->XMLDump) EndAttribute(sdump);
	} else {
		GenMFField *mffield;
		mffield = (GenMFField *) field.far_ptr;
		sf_type = gf_sg_vrml_get_sf_type(field.fieldType);
		if (!sdump->XMLDump) {
			gf_fprintf(sdump->trace, "[");
		} else if (sf_type==GF_SG_VRML_SFSTRING) {
			gf_fprintf(sdump->trace, " value=\'");
		} else {
			StartAttribute(sdump, "value");
		}
		for (i=0; mffield && (i<mffield->count); i++) {
			if (i) gf_fprintf(sdump->trace, " ");
			gf_sg_vrml_mf_get_item(field.far_ptr, field.fieldType, &slot_ptr, i);
			/*this is to cope with single MFString which shall appear as SF in XMT*/
			gf_dump_vrml_sffield(sdump, sf_type, slot_ptr, 1, parent);
		}
		if (!sdump->XMLDump) {
			gf_fprintf(sdump->trace, "]");
		} else if (sf_type==GF_SG_VRML_SFSTRING) {
			gf_fprintf(sdump->trace, "\'");
		} else {
			EndAttribute(sdump);
		}
	}
}

static void gf_dump_vrml_field(GF_SceneDumper *sdump, GF_Node *node, GF_FieldInfo field)
{
	u32 i, sf_type;
	Bool needs_field_container;
	GF_ChildNodeItem *list;
	void *slot_ptr;

	switch (field.fieldType) {
	case GF_SG_VRML_SFNODE:
		assert ( *(GF_Node **)field.far_ptr);

		if (sdump->XMLDump) {
			if (!sdump->X3DDump) {
				StartElement(sdump, (char *) field.name);
				EndElementHeader(sdump, 1);
				sdump->indent++;
			}
		} else {
			StartAttribute(sdump, field.name);
		}
		gf_dump_vrml_node(sdump, *(GF_Node **)field.far_ptr, 0, NULL);

		if (sdump->XMLDump) {
			if (!sdump->X3DDump) {
				sdump->indent--;
				EndElement(sdump, (char *) field.name, 1);
			}
		} else {
			EndAttribute(sdump);
		}
		return;
	case GF_SG_VRML_MFNODE:
		needs_field_container = 0;
		if (sdump->XMLDump && sdump->X3DDump) {
			u32 count, nb_ndt;
			GF_FieldInfo info;
			if (!strcmp(field.name, "children")) {
				needs_field_container = 0;
			} else {
				nb_ndt = 0;
				count = gf_node_get_field_count(node);
				for (i=0; i<count; i++) {
					gf_node_get_field(node, i, &info);
					if ((info.eventType==GF_SG_EVENT_IN) || (info.eventType==GF_SG_EVENT_OUT)) continue;
					if (info.NDTtype==field.NDTtype) nb_ndt++;
				}
				needs_field_container = (nb_ndt>1) ? 1 : 0;
			}
		}

#ifndef GPAC_DISABLE_X3D
		if (!sdump->X3DDump) {
			if (gf_node_get_tag(node)==TAG_X3D_Switch) field.name = "choice";
		}
#endif
		list = * ((GF_ChildNodeItem **) field.far_ptr);
		gf_assert(list);
		if (!sdump->XMLDump || !sdump->X3DDump) StartList(sdump, field.name);
		sdump->indent++;
		while (list) {
			gf_dump_vrml_node(sdump, list->node, 1, needs_field_container ? (char *) field.name : NULL);
			list = list->next;
		}
		sdump->indent--;
		if (!sdump->XMLDump || !sdump->X3DDump) EndList(sdump, field.name);
		return;
	case GF_SG_VRML_SFCOMMANDBUFFER:
	{
		SFCommandBuffer *cb = (SFCommandBuffer *)field.far_ptr;
		StartElement(sdump, (char *) field.name);
		EndElementHeader(sdump, 1);
		sdump->indent++;
		if (!gf_list_count(cb->commandList)) {
			/*the arch does not allow for that (we would need a codec and so on, or decompress the command list
			in all cases...)*/
			if (sdump->trace && cb->bufferSize) {
				if (sdump->XMLDump) gf_fprintf(sdump->trace, "<!--SFCommandBuffer cannot be dumped while playing - use MP4Box instead-->\n");
				else gf_fprintf(sdump->trace, "#SFCommandBuffer cannot be dumped while playing - use MP4Box instead\n");
			}
		} else {
			gf_sm_dump_command_list(sdump, cb->commandList, sdump->indent, 0);
		}
		sdump->indent--;
		EndElement(sdump, (char *) field.name, 1);
	}
	return;

	case GF_SG_VRML_MFATTRREF:
		if (sdump->XMLDump) {
			MFAttrRef *ar = (MFAttrRef *)field.far_ptr;
			StartElement(sdump, (char *) field.name);
			EndElementHeader(sdump, 1);
			sdump->indent++;

			for (i=0; i<ar->count; i++) {
				if (ar->vals[i].node) {
					GF_FieldInfo pinfo;
					DUMP_IND(sdump);
					gf_node_get_field(ar->vals[i].node, ar->vals[i].fieldIndex, &pinfo);
					gf_fprintf(sdump->trace, "<store node=\"");
					scene_dump_vrml_id(sdump, ar->vals[i].node);
					gf_fprintf(sdump->trace, "\" field=\"%s\"/>\n", pinfo.name);
				}
			}

			sdump->indent--;
			EndElement(sdump, (char *) field.name, 1);
			return;
		}
		break;
	}


	if (gf_sg_vrml_is_sf_field(field.fieldType)) {
		StartAttribute(sdump, field.name);
		if (field.far_ptr)
			gf_dump_vrml_sffield(sdump, field.fieldType, field.far_ptr, 0, node);
		EndAttribute(sdump);
	} else {
		GenMFField *mffield = (GenMFField *) field.far_ptr;
		sf_type = gf_sg_vrml_get_sf_type(field.fieldType);

		if (sdump->XMLDump && sdump->X3DDump) {
			switch (sf_type) {
			case GF_SG_VRML_SFSTRING:
			case GF_SG_VRML_SFSCRIPT:
			case GF_SG_VRML_SFURL:
				gf_fprintf(sdump->trace, " %s=\'", (char *) field.name);
				break;
			default:
				StartAttribute(sdump, field.name);
				break;
			}
		} else {
			StartAttribute(sdump, field.name);
		}

		if (!sdump->XMLDump) gf_fprintf(sdump->trace, "[");
		for (i=0; mffield && (i<mffield->count); i++) {
			if (i) gf_fprintf(sdump->trace, " ");
			gf_sg_vrml_mf_get_item(field.far_ptr, field.fieldType, &slot_ptr, i);
			gf_dump_vrml_sffield(sdump, sf_type, slot_ptr, 1, node);
		}
		if (!sdump->XMLDump) gf_fprintf(sdump->trace, "]");

		if (sdump->XMLDump && sdump->X3DDump) {
			switch (sf_type) {
			case GF_SG_VRML_SFSTRING:
			case GF_SG_VRML_SFSCRIPT:
			case GF_SG_VRML_SFURL:
				gf_fprintf(sdump->trace, "\'");
				break;
			default:
				EndAttribute(sdump);
				break;
			}
		} else {
			EndAttribute(sdump);
		}
	}
}

static const char *GetXMTFieldTypeName(u32 fieldType)
{
	switch (fieldType) {
	case GF_SG_VRML_SFBOOL:
		return "Boolean";
	case GF_SG_VRML_SFINT32:
		return "Integer";
	case GF_SG_VRML_SFCOLOR:
		return "Color";
	case GF_SG_VRML_SFVEC2F:
		return "Vector2";
	case GF_SG_VRML_SFIMAGE:
		return "Image";
	case GF_SG_VRML_SFTIME:
		return "Time";
	case GF_SG_VRML_SFFLOAT:
		return "Float";
	case GF_SG_VRML_SFVEC3F:
		return "Vector3";
	case GF_SG_VRML_SFROTATION:
		return "Rotation";
	case GF_SG_VRML_SFSTRING:
		return "String";
	case GF_SG_VRML_SFNODE:
		return "Node";
	case GF_SG_VRML_MFBOOL:
		return "Booleans";
	case GF_SG_VRML_MFINT32:
		return "Integers";
	case GF_SG_VRML_MFCOLOR:
		return "Colors";
	case GF_SG_VRML_MFVEC2F:
		return "Vector2Array";
	case GF_SG_VRML_MFIMAGE:
		return "Images";
	case GF_SG_VRML_MFTIME:
		return "Times";
	case GF_SG_VRML_MFFLOAT:
		return "Floats";
	case GF_SG_VRML_MFVEC3F:
		return "Vector3Array";
	case GF_SG_VRML_MFROTATION:
		return "Rotations";
	case GF_SG_VRML_MFSTRING:
		return "Strings";
	case GF_SG_VRML_MFNODE:
		return "Nodes";
	default:
		return "unknown";
	}
}
static const char *GetXMTFieldTypeValueName(u32 fieldType)
{
	switch (fieldType) {
	case GF_SG_VRML_SFBOOL:
		return "booleanValue";
	case GF_SG_VRML_SFINT32:
		return "intValue";
	case GF_SG_VRML_SFCOLOR:
		return "colorValue";
	case GF_SG_VRML_SFVEC2F:
		return "vector2Value";
	case GF_SG_VRML_SFIMAGE:
		return "imageValue";
	case GF_SG_VRML_SFTIME:
		return "timeValue";
	case GF_SG_VRML_SFFLOAT:
		return "floatValue";
	case GF_SG_VRML_SFVEC3F:
		return "vector3Value";
	case GF_SG_VRML_SFROTATION:
		return "rotationValue";
	case GF_SG_VRML_SFSTRING:
		return "stringValue";
	case GF_SG_VRML_MFBOOL:
		return "booleanArrayValue";
	case GF_SG_VRML_MFINT32:
		return "intArrayValue";
	case GF_SG_VRML_MFCOLOR:
		return "colorArrayValue";
	case GF_SG_VRML_MFVEC2F:
		return "vector2ArrayValue";
	case GF_SG_VRML_MFIMAGE:
		return "imageArrayValue";
	case GF_SG_VRML_MFTIME:
		return "timeArrayValue";
	case GF_SG_VRML_MFFLOAT:
		return "floatArrayValue";
	case GF_SG_VRML_MFVEC3F:
		return "vector3ArrayValue";
	case GF_SG_VRML_MFROTATION:
		return "rotationArrayValue";
	case GF_SG_VRML_MFSTRING:
		return "stringArrayValue";
	default:
		return "unknown";
	}
}

/*field dumping for proto declaration and Script*/
static void gf_dump_vrml_dyn_field(GF_SceneDumper *sdump, GF_Node *node, GF_FieldInfo field, Bool has_sublist)
{
	u32 i, sf_type;
	void *slot_ptr;

	if (gf_sg_vrml_is_sf_field(field.fieldType)) {
		DUMP_IND(sdump);
		if (sdump->XMLDump) {
			if (sdump->X3DDump) {
				gf_fprintf(sdump->trace, "<field name=\"%s\" type=\"%s\" accessType=\"%s\"",
				        field.name, gf_sg_vrml_get_field_type_name(field.fieldType), gf_sg_vrml_get_event_type_name(field.eventType, 1));
			} else {
				gf_fprintf(sdump->trace, "<field name=\"%s\" type=\"%s\" vrml97Hint=\"%s\"",
				        field.name, GetXMTFieldTypeName(field.fieldType), gf_sg_vrml_get_event_type_name(field.eventType, 0));
			}

			if ((field.eventType == GF_SG_EVENT_FIELD) || (field.eventType == GF_SG_EVENT_EXPOSED_FIELD)) {
				if (field.fieldType == GF_SG_VRML_SFNODE) {
					if (!sdump->X3DDump) {
						gf_fprintf(sdump->trace, ">\n");
						sdump->indent++;
						gf_fprintf(sdump->trace, "<node>");
						gf_dump_vrml_node(sdump, field.far_ptr ? *(GF_Node **)field.far_ptr : NULL, 0, NULL);
						gf_fprintf(sdump->trace, "</node>");
						sdump->indent--;
						if (!has_sublist)
							gf_fprintf(sdump->trace, "</field>\n");
					} else {
						if (field.far_ptr) {
							gf_fprintf(sdump->trace, ">\n");
							gf_dump_vrml_node(sdump, *(GF_Node **)field.far_ptr, 0, NULL);
							gf_fprintf(sdump->trace, "</field>\n");
						} else {
							gf_fprintf(sdump->trace, "/>\n");
						}
					}
					DUMP_IND(sdump);
				} else {
					if (sdump->X3DDump) {
						gf_fprintf(sdump->trace, " value=\"");
					} else {
						gf_fprintf(sdump->trace, " %s=\"", GetXMTFieldTypeValueName(field.fieldType));
					}
					if (field.far_ptr)
						gf_dump_vrml_sffield(sdump, field.fieldType, field.far_ptr, 0, node);
					if (has_sublist)
						gf_fprintf(sdump->trace, "\">\n");
					else
						gf_fprintf(sdump->trace, "\"/>\n");
				}
			} else {
				gf_fprintf(sdump->trace, "/>\n");
			}
		} else {
			gf_fprintf(sdump->trace, "%s %s %s", gf_sg_vrml_get_event_type_name(field.eventType, sdump->X3DDump), gf_sg_vrml_get_field_type_name(field.fieldType), field.name);
			if ((field.eventType==GF_SG_EVENT_FIELD) || (field.eventType==GF_SG_EVENT_EXPOSED_FIELD)) {
				gf_fprintf(sdump->trace, " ");
				if (field.fieldType == GF_SG_VRML_SFNODE) {
					gf_dump_vrml_node(sdump, field.far_ptr ? *(GF_Node **)field.far_ptr : NULL, 0, NULL);
				} else if (field.far_ptr) {
					gf_dump_vrml_simple_field(sdump, field, node);
				}
			}
			gf_fprintf(sdump->trace, "\n");
		}
	} else if (field.far_ptr) {
		GenMFField *mffield = (GenMFField *) field.far_ptr;
		sf_type = gf_sg_vrml_get_sf_type(field.fieldType);

		DUMP_IND(sdump);
		if (!sdump->XMLDump) {
			gf_fprintf(sdump->trace, "%s %s %s", gf_sg_vrml_get_event_type_name(field.eventType, sdump->X3DDump), gf_sg_vrml_get_field_type_name(field.fieldType), field.name);
			if ((field.eventType==GF_SG_EVENT_FIELD) || (field.eventType==GF_SG_EVENT_EXPOSED_FIELD)) {
				gf_fprintf(sdump->trace, " [");

				if (sf_type == GF_SG_VRML_SFNODE) {
					GF_ChildNodeItem *l = *(GF_ChildNodeItem **)field.far_ptr;
					gf_fprintf(sdump->trace, "\n");
					sdump->indent++;
					while (l) {
						gf_dump_vrml_node(sdump, l->node, 1, NULL);
						l = l->next;
					}
					sdump->indent--;
					DUMP_IND(sdump);
				} else {
					for (i=0; mffield && (i<mffield->count); i++) {
						if (i) gf_fprintf(sdump->trace, " ");
						if (field.fieldType != GF_SG_VRML_MFNODE) {
							gf_sg_vrml_mf_get_item(field.far_ptr, field.fieldType, &slot_ptr, i);
							gf_dump_vrml_sffield(sdump, sf_type, slot_ptr, (mffield->count>1) ? 1 : 0, node);
						}
					}
				}
				gf_fprintf(sdump->trace, "]");
			}
			gf_fprintf(sdump->trace, "\n");
		} else {
			if (sdump->X3DDump) {
				gf_fprintf(sdump->trace, "<field name=\"%s\" type=\"%s\" accessType=\"%s\"",
				        field.name, gf_sg_vrml_get_field_type_name(field.fieldType), gf_sg_vrml_get_event_type_name(field.eventType, 1));
			} else {
				gf_fprintf(sdump->trace, "<field name=\"%s\" type=\"%s\" vrml97Hint=\"%s\"",
				        field.name, GetXMTFieldTypeName(field.fieldType), gf_sg_vrml_get_event_type_name(field.eventType, 0));
			}

			if ((field.eventType==GF_SG_EVENT_FIELD) || (field.eventType==GF_SG_EVENT_EXPOSED_FIELD)) {
				if (sf_type == GF_SG_VRML_SFNODE) {
					GF_ChildNodeItem *list = *(GF_ChildNodeItem **)field.far_ptr;
					gf_fprintf(sdump->trace, ">\n");
					sdump->indent++;
					if (!sdump->X3DDump) gf_fprintf(sdump->trace, "<nodes>");
					while (list) {
						gf_dump_vrml_node(sdump, list->node, 1, NULL);
						list = list->next;
					}
					if (!sdump->X3DDump) gf_fprintf(sdump->trace, "</nodes>");
					sdump->indent++;
					DUMP_IND(sdump);
					if (!has_sublist)
						gf_fprintf(sdump->trace, "</field>\n");
				} else {
					if (sdump->X3DDump) {
						gf_fprintf(sdump->trace, " value=\"");
					} else {
						gf_fprintf(sdump->trace, " %s=\"", GetXMTFieldTypeValueName(field.fieldType));
					}
					for (i=0; mffield && (i<mffield->count); i++) {
						if (i) gf_fprintf(sdump->trace, " ");
						if (field.fieldType != GF_SG_VRML_MFNODE) {
							gf_sg_vrml_mf_get_item(field.far_ptr, field.fieldType, &slot_ptr, i);
							gf_dump_vrml_sffield(sdump, sf_type, slot_ptr, (mffield->count>1) ? 1 : 0, node);
						}
					}
					if (has_sublist)
						gf_fprintf(sdump->trace, "\">\n");
					else
						gf_fprintf(sdump->trace, "\"/>\n");
				}
			} else {
				gf_fprintf(sdump->trace, "/>\n");
			}
		}
	}
}


/*field dumping for proto instance*/
static void gf_dump_vrml_proto_field(GF_SceneDumper *sdump, GF_Node *node, GF_FieldInfo field)
{
	u32 i, sf_type;
	void *slot_ptr;

	DUMP_IND(sdump);
	gf_fprintf(sdump->trace, "<fieldValue name=\"%s\" ", field.name);
	if (gf_sg_vrml_is_sf_field(field.fieldType)) {
		if (field.fieldType == GF_SG_VRML_SFNODE) {
			gf_fprintf(sdump->trace, ">\n");
			sdump->indent++;
			if (!sdump->X3DDump) gf_fprintf(sdump->trace, "<node>");
			gf_dump_vrml_node(sdump, field.far_ptr ? *(GF_Node **)field.far_ptr : NULL, 0, NULL);
			if (!sdump->X3DDump) gf_fprintf(sdump->trace, "</node>");
			sdump->indent--;
			DUMP_IND(sdump);
			gf_fprintf(sdump->trace, "</fieldValue>\n");
		} else {
			if (sdump->X3DDump) {
				gf_fprintf(sdump->trace, " value=\"");
			} else {
				gf_fprintf(sdump->trace, " %s=\"", GetXMTFieldTypeValueName(field.fieldType));
			}
			if (field.far_ptr)
				gf_dump_vrml_sffield(sdump, field.fieldType, field.far_ptr, 0, node);
			gf_fprintf(sdump->trace, "\"/>\n");
		}
	} else {
		GenMFField *mffield = (GenMFField *) field.far_ptr;
		sf_type = gf_sg_vrml_get_sf_type(field.fieldType);

		if ((field.eventType==GF_SG_EVENT_FIELD) || (field.eventType==GF_SG_EVENT_EXPOSED_FIELD)) {
			if (sf_type == GF_SG_VRML_SFNODE) {
				GF_ChildNodeItem *list = *(GF_ChildNodeItem **)field.far_ptr;
				gf_fprintf(sdump->trace, ">\n");
				sdump->indent++;
				if (!sdump->X3DDump) gf_fprintf(sdump->trace, "<nodes>");
				while (list) {
					gf_dump_vrml_node(sdump, list->node, 1, NULL);
					list = list->next;
				}
				if (!sdump->X3DDump) gf_fprintf(sdump->trace, "</nodes>");
				sdump->indent--;
				DUMP_IND(sdump);
				gf_fprintf(sdump->trace, "</fieldValue>\n");
			} else {
				if (sdump->X3DDump) {
					gf_fprintf(sdump->trace, " value=\"");
				} else {
					gf_fprintf(sdump->trace, " %s=\"", GetXMTFieldTypeValueName(field.fieldType));
				}
				for (i=0; mffield && (i<mffield->count); i++) {
					if (i) gf_fprintf(sdump->trace, " ");
					if (field.fieldType != GF_SG_VRML_MFNODE) {
						gf_sg_vrml_mf_get_item(field.far_ptr, field.fieldType, &slot_ptr, i);
						gf_dump_vrml_sffield(sdump, sf_type, slot_ptr, (mffield->count>1) ? 1 : 0, node);
					}
				}
				gf_fprintf(sdump->trace, "\"/>\n");
			}
		}
	}
}

static GF_Route *gf_dump_vrml_get_IS(GF_SceneDumper *sdump, GF_Node *node, GF_FieldInfo *field)
{
	u32 i;
	GF_Route *r;
	i=0;
	while ((r = (GF_Route*)gf_list_enum(sdump->current_proto->sub_graph->Routes, &i))) {
		if (!r->IS_route) continue;
		if ((r->ToNode==node) && (r->ToField.fieldIndex==field->fieldIndex)) return r;
	}
	if (!node || !node->sgprivate->interact || !node->sgprivate->interact->routes) return NULL;
	i=0;
	while ((r = (GF_Route*)gf_list_enum(node->sgprivate->interact->routes, &i))) {
		if (!r->IS_route) continue;
		if (r->FromField.fieldIndex == field->fieldIndex) return r;
	}
	return NULL;
}

static void gf_dump_vrml_IS_field(GF_SceneDumper *sdump, GF_Node *node, GF_FieldInfo field, Bool isScript, Bool skip_is)
{
	GF_FieldInfo pfield;

	GF_Route *r = gf_dump_vrml_get_IS(sdump, node, &field);
	if (r->FromNode) {
		pfield.fieldIndex = r->ToField.fieldIndex;
		gf_sg_proto_get_field(sdump->current_proto, NULL, &pfield);
	} else {
		pfield.fieldIndex = r->FromField.fieldIndex;
		gf_sg_proto_get_field(sdump->current_proto, NULL, &pfield);
	}

	if (!sdump->XMLDump) {
		DUMP_IND(sdump);
		if (isScript) gf_fprintf(sdump->trace, "%s %s ", gf_sg_vrml_get_event_type_name(field.eventType, sdump->X3DDump), gf_sg_vrml_get_field_type_name(field.fieldType));
		gf_fprintf(sdump->trace, "%s IS %s\n", field.name, pfield.name);
	} else {
		if (!skip_is) {
			StartElement(sdump, "IS");
			EndElementHeader(sdump, 1);
			sdump->indent++;
		}
		DUMP_IND(sdump);
		gf_fprintf(sdump->trace, "<connect nodeField=\"%s\" protoField=\"%s\"/>\n", field.name, pfield.name);
		if (!skip_is) {
			sdump->indent--;
			EndElement(sdump, "IS", 1);
		}
	}
}

static Bool scene_dump_vrml_can_dump(GF_SceneDumper *sdump, GF_Node *node)
{
#ifndef GPAC_DISABLE_VRML
	u32 tag;

	if (node->sgprivate->tag==TAG_ProtoNode) return 1;

	if (sdump->X3DDump || (sdump->dump_mode==GF_SM_DUMP_VRML)) {
		if (node->sgprivate->tag>=GF_NODE_RANGE_FIRST_X3D) return 1;
		if (node->sgprivate->tag==TAG_MPEG4_Rectangle) return 1;
		if (node->sgprivate->tag==TAG_MPEG4_Circle) return 1;
#ifndef GPAC_DISABLE_X3D
		tag = gf_node_x3d_type_by_class_name(gf_node_get_class_name(node));
		return tag ? 1 : 0;
#else
		return 0;
#endif
	} else {
		if (node->sgprivate->tag<=GF_NODE_RANGE_LAST_MPEG4) return 1;
#ifndef GPAC_DISABLE_X3D
		if (node->sgprivate->tag==TAG_X3D_Rectangle2D) return 1;
		if (node->sgprivate->tag==TAG_X3D_Circle2D) return 1;
#endif
		tag = gf_node_mpeg4_type_by_class_name(gf_node_get_class_name(node));
		return tag ? 1 : 0;
	}
#else
	return 1;
#endif
}

static void gf_dump_vrml_node(GF_SceneDumper *sdump, GF_Node *node, Bool in_list, char *fieldContainer)
{
	u32 i, count, to_dump, sub_el, ID;
	u32 *def_fields;
	Bool isDEF, isScript, isProto, hasISed;
	char *name;
	GF_Node *base;
	GF_FieldInfo field, base_field;

	if (!node) {
		gf_fprintf(sdump->trace, "NULL");
		return;
	}

	/*this dumper works only for VRML like graphs*/
	if (node->sgprivate->tag>GF_NODE_RANGE_LAST_X3D) return;

	if (!scene_dump_vrml_can_dump(sdump, node)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[Scene Dump] node %s not part of %s standard - removing\n", gf_node_get_class_name(node), sdump->X3DDump ? "X3D" : (sdump->dump_mode==GF_SM_DUMP_VRML) ? "VRML" : "MPEG4"));
		if (!in_list) gf_fprintf(sdump->trace, "NULL");
		return;
	}

	/*convert whatever possible*/
	name = (char*)gf_node_get_class_name(node);
#ifndef GPAC_DISABLE_VRML
	if (sdump->X3DDump) {
		if (node->sgprivate->tag == TAG_MPEG4_Circle) name = "Circle2D";
		else if (node->sgprivate->tag == TAG_MPEG4_Rectangle) name = "Rectangle2D";
#ifndef GPAC_DISABLE_X3D
	} else {
		if (node->sgprivate->tag == TAG_X3D_Circle2D) name = "Circle";
		else if (node->sgprivate->tag == TAG_X3D_Rectangle2D) name = "Rectangle";
#endif
	}
#endif



	isProto = (gf_node_get_tag(node) == TAG_ProtoNode) ? 1 : 0;
	ID = gf_node_get_id(node);
	isDEF = 0;
	if (ID) {
		isDEF = gf_dump_vrml_is_def_node(sdump, node);
		if (!isDEF) {
			if (!sdump->XMLDump) {
				if (in_list) DUMP_IND(sdump);
				gf_fprintf(sdump->trace, "USE ");
				scene_dump_vrml_id(sdump, node);
				if (in_list) gf_fprintf(sdump->trace, "\n");
			} else {
				if (isProto) {
					StartElement(sdump, "ProtoInstance");
					StartAttribute(sdump, "name");
					gf_fprintf(sdump->trace, "%s", name);
					EndAttribute(sdump);
				} else {
					StartElement(sdump, name);
				}
				StartAttribute(sdump, "USE");
				scene_dump_vrml_id(sdump, node);
				EndAttribute(sdump);
				EndElementHeader(sdump, 0);
			}
			return;
		}
	}

	/*get all fields*/
	count = gf_node_get_field_count(node);
	def_fields = (u32*)gf_malloc(sizeof(u32) * count);

	base = NULL;
	switch (gf_node_get_tag(node)) {
#ifndef GPAC_DISABLE_VRML
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Script:
#endif
	case TAG_MPEG4_Script:
		isScript = 1;
		break;
#endif
	default:
		isScript = 0;
		break;
	}


	if (!isScript) {
		if (isProto) {
			base = gf_sg_proto_create_instance(node->sgprivate->scenegraph, ((GF_ProtoInstance *)node)->proto_interface);
		} else {
			base = gf_node_new(node->sgprivate->scenegraph, node->sgprivate->tag);
		}
	}

	if (base) gf_node_register(base, NULL);

	hasISed = 0;
	to_dump = sub_el = 0;
	for (i=0; i<count; i++) {
		if (isScript) {
			/*dyn script fields are complex types*/
			def_fields[i] = (i>2) ? 2 : 1;
		} else {
			def_fields[i] = 0;
		}

		gf_node_get_field(node, i, &field);

		if (sdump->current_proto) {
			if (gf_dump_vrml_get_IS(sdump, node, &field) != NULL) {
				def_fields[i] = 3;
				if ((field.fieldType == GF_SG_VRML_SFNODE) || (field.fieldType == GF_SG_VRML_MFNODE))
					def_fields[i] = sdump->XMLDump ? 4 : 3;
				/*in XMT the ISed is not an attribute*/
				if (sdump->XMLDump) sub_el++;
				to_dump++;
				hasISed = 1;
				continue;
			}
		}

		if (!isScript && ((field.eventType == GF_SG_EVENT_IN) || (field.eventType == GF_SG_EVENT_OUT)) ) {
			continue;
		}
		/*proto instance in XMT lists all fields as elements*/
		if (sdump->XMLDump && isProto) {
			def_fields[i] = 2;
			to_dump++;
			sub_el++;
			continue;
		}
		switch (field.fieldType) {
		case GF_SG_VRML_SFNODE:
			if (* (GF_Node **) field.far_ptr) {
				def_fields[i] = 2;
				to_dump++;
				sub_el++;
			}
			break;
		case GF_SG_VRML_MFNODE:
			if (* (GF_ChildNodeItem**) field.far_ptr) {
				def_fields[i] = 2;
				to_dump++;
				sub_el++;
			}
			break;
		case GF_SG_VRML_SFCOMMANDBUFFER:
		{
			SFCommandBuffer *p = (SFCommandBuffer *)field.far_ptr;
			if (p->bufferSize || gf_list_count(p->commandList)) {
				def_fields[i] = 2;
				to_dump++;
				sub_el++;
			}
		}
		break;
		case GF_SG_VRML_MFATTRREF:
		{
			MFAttrRef *p = (MFAttrRef*)field.far_ptr;
			if (p->count) {
				def_fields[i] = 2;
				to_dump++;
				sub_el++;
			}
		}
		break;
		default:
			if (isScript) {
				to_dump++;
			} else {
				gf_node_get_field(base, i, &base_field);
				if (!gf_sg_vrml_field_equal(base_field.far_ptr, field.far_ptr, field.fieldType)) {
					def_fields[i] = 1;
					to_dump++;
				}
			}
			break;
		}
	}
	if (base) gf_node_unregister(base, NULL);

	if (!to_dump) {
		if (in_list) DUMP_IND(sdump);
		if (!sdump->XMLDump) {
			if (isDEF) {
				gf_fprintf(sdump->trace, "DEF ");
				scene_dump_vrml_id(sdump, node);
				gf_fprintf(sdump->trace, " ");
			}
			gf_fprintf(sdump->trace, "%s {}\n", name);
		} else {
			if (isDEF) {
				if (isProto) {
					gf_fprintf(sdump->trace, "<ProtoInstance name=\"%s\" DEF=\"", name);
				} else {
					gf_fprintf(sdump->trace, "<%s DEF=\"", name);
				}
				scene_dump_vrml_id(sdump, node);
				gf_fprintf(sdump->trace, "\"/>\n");
			} else {
				if (isProto) {
					gf_fprintf(sdump->trace, "<ProtoInstance name=\"%s\"/>\n", name);
				} else {
					gf_fprintf(sdump->trace, "<%s/>\n", name);
				}
			}
		}
		gf_free(def_fields);
		return;
	}

	if (!sdump->XMLDump) {
		if (in_list) DUMP_IND(sdump);
		if (isDEF) {
			gf_fprintf(sdump->trace, "DEF ");
			scene_dump_vrml_id(sdump, node);
			gf_fprintf(sdump->trace, " ");
		}
		gf_fprintf(sdump->trace, "%s {\n", name);
	} else {
		if (isProto) {
			StartElement(sdump, "ProtoInstance");
			StartAttribute(sdump, "name");
			gf_fprintf(sdump->trace, "%s", name);
			EndAttribute(sdump);
		} else {
			StartElement(sdump, name);
		}
		if (isDEF) {
			StartAttribute(sdump, "DEF");
			scene_dump_vrml_id(sdump, node);
			EndAttribute(sdump);
		}
	}

	sdump->indent ++;
	for (i=0; i<count; i++) {
		switch (def_fields[i]) {
		/*regular field*/
		case 1:
			gf_node_get_field(node, i, &field);
			if (!isScript) {
				gf_dump_vrml_field(sdump, node, field);
			}
			/*special script dump case, static fields except url*/
			else if (i==1 || i==2) {
				if (*((SFBool *)field.far_ptr)) gf_dump_vrml_field(sdump, node, field);
			}
			/*in bt first dump fields - in XMT first dump url*/
			else if (i && !sdump->XMLDump) {
				gf_dump_vrml_dyn_field(sdump, node, field, 0);
			} else if (!i && sdump->XMLDump) {
				gf_dump_vrml_field(sdump, node, field);
			}
			break;
		/*IS field*/
		case 3:
			if (sdump->XMLDump) break;
			gf_node_get_field(node, i, &field);
			gf_dump_vrml_IS_field(sdump, node, field, isScript, 0);
			def_fields[i] = 0;
			break;
		default:
			break;
		}
	}
	if (fieldContainer) gf_fprintf(sdump->trace, " fieldContainer=\"%s\"", fieldContainer);

	if (isScript) sub_el = 1;
	EndElementHeader(sdump, sub_el ? 1 : 0);

	if (sub_el) {
		/*dump all normal IS elements for XMT*/
		if (hasISed && sdump->XMLDump) {
			StartElement(sdump, "IS");
			EndElementHeader(sdump, 1);
			sdump->indent++;
		}
		for (i=0; i<count; i++) {
			if (def_fields[i]==3) {
				gf_node_get_field(node, i, &field);
				gf_dump_vrml_IS_field(sdump, node, field, isScript, 1);
			}
		}
		if (hasISed && sdump->XMLDump) {
			sdump->indent--;
			EndElement(sdump, "IS", 1);
		}
		/*dump all sub elements and complex IS*/
		for (i=0; i<count; i++) {
			switch (def_fields[i]) {
			case 2:
				gf_node_get_field(node, i, &field);
				if (!isScript) {
					if (isProto && sdump->XMLDump) {
						gf_dump_vrml_proto_field(sdump, node, field);
					} else {
						gf_dump_vrml_field(sdump, node, field);
					}
				} else {
#ifndef GPAC_DISABLE_X3D
					/*X3D script metadata, NOT DYN*/
					if ((i==3) && (node->sgprivate->tag==TAG_X3D_Script) ) {
						if (*((GF_Node **)field.far_ptr)) gf_dump_vrml_field(sdump, node, field);
					} else
#endif
					{
						gf_dump_vrml_dyn_field(sdump, node, field, 0);
					}
				}
				break;
			case 4:
				gf_node_get_field(node, i, &field);
				gf_dump_vrml_IS_field(sdump, node, field, isScript, 0);
				break;
			}
		}
	}

	/*finally dump script - XMT dumping is broken!!*/
	if (isScript && !sdump->XMLDump) {
		gf_node_get_field(node, 0, &field);
		gf_dump_vrml_field(sdump, node, field);
	}

	sdump->indent --;
	if (!sdump->XMLDump && !in_list) {
		DUMP_IND(sdump);
		gf_fprintf(sdump->trace, "}");
	} else {
		EndElement(sdump, isProto ? "ProtoInstance" : name, sub_el);
	}
	gf_free(def_fields);
}


static GF_Err DumpMultipleIndexedReplace(GF_SceneDumper *sdump, GF_Command *com)
{
	u32 i;
	GF_FieldInfo field;
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *) gf_list_get(com->command_fields, 0);
	gf_node_get_field(com->node, inf->fieldIndex, &field);
	field.fieldType = inf->fieldType;

	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<Replace extended=\"indices\" atNode=\"");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, "\" atField=\"%s\">\n", field.name);
	} else {
		gf_fprintf(sdump->trace, "MULTIPLEINDREPLACE ");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, ".%s [\n", field.name);
	}
	sdump->indent++;
	i=0;
	while ((inf = (GF_CommandField *) gf_list_enum(com->command_fields, &i))) {
		field.far_ptr = inf->field_ptr;

		DUMP_IND(sdump);
		if (sdump->XMLDump) {
			gf_fprintf(sdump->trace, "<repValue position=\"%d\" ", inf->pos);
		} else {
			gf_fprintf(sdump->trace, "%d BY ", inf->pos);
		}
		gf_dump_vrml_simple_field(sdump, field, com->node);
		if (sdump->XMLDump) {
			gf_fprintf(sdump->trace, "/>");
		} else {
			gf_fprintf(sdump->trace, "\n");
		}
	}
	sdump->indent--;
	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "</Replace>\n");
	} else {
		gf_fprintf(sdump->trace, "]\n");
	}
	return GF_OK;
}

static GF_Err DumpMultipleReplace(GF_SceneDumper *sdump, GF_Command *com)
{
	u32 i;
	GF_FieldInfo info;
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;

	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<Replace extended=\"fields\" atNode=\"");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, "\">\n");

		sdump->indent++;
		i=0;
		while ((inf = (GF_CommandField *) gf_list_enum(com->command_fields, &i))) {
			gf_node_get_field(com->node, inf->fieldIndex, &info);
			info.far_ptr = inf->field_ptr;

			DUMP_IND(sdump);
			if (gf_sg_vrml_get_sf_type(info.fieldType) != GF_SG_VRML_SFNODE) {
				gf_fprintf(sdump->trace, "<repField atField=\"%s\" ", info.name);
				gf_dump_vrml_simple_field(sdump, info, com->node);
				gf_fprintf(sdump->trace, "/>\n");
			} else {
				gf_fprintf(sdump->trace, "<repField>");
				gf_dump_vrml_field(sdump, com->node, info);
				gf_fprintf(sdump->trace, "</repField>\n");
			}
		}
		sdump->indent--;

		DUMP_IND(sdump);
		gf_fprintf(sdump->trace, "</Replace>\n");
	} else {
		gf_fprintf(sdump->trace, "MULTIPLEREPLACE ");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, " {\n");
		sdump->indent++;
		i=0;
		while ((inf = (GF_CommandField *) gf_list_enum(com->command_fields, &i))) {
			gf_node_get_field(com->node, inf->fieldIndex, &info);
			info.far_ptr = inf->field_ptr;
			gf_dump_vrml_field(sdump, com->node, info);
		}
		sdump->indent--;
		DUMP_IND(sdump);
		gf_fprintf(sdump->trace, "}\n");
	}
	return GF_OK;
}

static GF_Err DumpGlobalQP(GF_SceneDumper *sdump, GF_Command *com)
{
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *) gf_list_get(com->command_fields, 0);

	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<Replace extended=\"globalQuant\">\n");
	} else {
		gf_fprintf(sdump->trace, "GLOBALQP ");
	}
	gf_dump_vrml_node(sdump, inf->new_node, 0, NULL);
	if (sdump->XMLDump) gf_fprintf(sdump->trace, "</Replace>\n");
	else gf_fprintf(sdump->trace, "\n");
	return GF_OK;
}

static GF_Err DumpNodeInsert(GF_SceneDumper *sdump, GF_Command *com)
{
	GF_CommandField *inf;
	char posname[20];
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *) gf_list_get(com->command_fields, 0);

	switch (inf->pos) {
	case 0:
		strcpy(posname, "BEGIN");
		break;
	case -1:
		strcpy(posname, "END");
		break;
	default:
		sprintf(posname, "%d", inf->pos);
		break;
	}

	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<Insert atNode=\"");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, "\" position=\"%s\">", posname);
	} else {
		if (inf->pos==-1) {
			gf_fprintf(sdump->trace, "APPEND TO ");
		}
		else gf_fprintf(sdump->trace, "INSERT AT ");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, ".children");
		if (inf->pos!=-1) gf_fprintf(sdump->trace, "[%d]", inf->pos);
		gf_fprintf(sdump->trace, " ");
	}

	gf_dump_vrml_node(sdump, inf->new_node, 0, NULL);
	if (sdump->XMLDump) gf_fprintf(sdump->trace, "</Insert>");
	gf_fprintf(sdump->trace, "\n");
	return GF_OK;
}

static GF_Err DumpRouteInsert(GF_SceneDumper *sdump, GF_Command *com, Bool is_scene_replace)
{
	GF_Route r;

	memset(&r, 0, sizeof(GF_Route));
	r.ID = com->RouteID;
	r.name = com->def_name;
	r.FromNode = gf_dump_find_node(sdump, com->fromNodeID);
	r.FromField.fieldIndex = com->fromFieldIndex;
	r.ToNode = gf_dump_find_node(sdump, com->toNodeID);
	r.ToField.fieldIndex = com->toFieldIndex;

	gf_list_add(sdump->inserted_routes, com);

	if (is_scene_replace) {
		gf_dump_vrml_route(sdump, &r, 0);
	} else {
		DUMP_IND(sdump);
		if (sdump->XMLDump) {
			gf_fprintf(sdump->trace, "<Insert>\n");
		} else {
			gf_fprintf(sdump->trace, "INSERT ");
		}
		gf_dump_vrml_route(sdump, &r, 2);
		if (sdump->XMLDump) gf_fprintf(sdump->trace, "</Insert>");
	}
	return GF_OK;
}

static GF_Err DumpIndexInsert(GF_SceneDumper *sdump, GF_Command *com)
{
	GF_Err e;
	GF_FieldInfo field, sffield;
	GF_CommandField *inf;
	char posname[20];
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *) gf_list_get(com->command_fields, 0);

	switch (inf->pos) {
	case 0:
		strcpy(posname, "BEGIN");
		break;
	case -1:
		strcpy(posname, "END");
		break;
	default:
		sprintf(posname, "%d", inf->pos);
		break;
	}

	e = gf_node_get_field(com->node, inf->fieldIndex, &field);
	if (e) return e;
	if (gf_sg_vrml_is_sf_field(field.fieldType)) return GF_NON_COMPLIANT_BITSTREAM;

	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<Insert atNode=\"");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, "\" atField=\"%s\" position=\"%s\"", field.name, posname);
	} else {
		if (inf->pos==-1) {
			gf_fprintf(sdump->trace, "APPEND TO ");
		}
		else gf_fprintf(sdump->trace, "INSERT AT ");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, ".%s", field.name);
		if (inf->pos!=-1) gf_fprintf(sdump->trace, "[%d]", inf->pos);
		gf_fprintf(sdump->trace, " ");
	}

	memcpy(&sffield, &field, sizeof(GF_FieldInfo));
	sffield.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);

	if (field.fieldType==GF_SG_VRML_MFNODE) {
		if (sdump->XMLDump) gf_fprintf(sdump->trace, ">\n");
		gf_dump_vrml_node(sdump, inf->new_node, 0, NULL);
		if (sdump->XMLDump) gf_fprintf(sdump->trace, "</Insert>");
		gf_fprintf(sdump->trace, "\n");
	} else {
		sffield.far_ptr = inf->field_ptr;
		gf_dump_vrml_simple_field(sdump, sffield, com->node);
		if (sdump->XMLDump) gf_fprintf(sdump->trace, "/>");
		gf_fprintf(sdump->trace, "\n");
	}
	return e;
}

static GF_Err DumpIndexDelete(GF_SceneDumper *sdump, GF_Command *com)
{
	char posname[20];
	GF_FieldInfo field;
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *) gf_list_get(com->command_fields, 0);

	switch (inf->pos) {
	case -1:
		strcpy(posname, sdump->XMLDump ? "END" : "LAST");
		break;
	case 0:
		strcpy(posname, "BEGIN");
		break;
	default:
		sprintf(posname, "%d", inf->pos);
		break;
	}

	gf_node_get_field(com->node, inf->fieldIndex, &field);

	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<Delete atNode=\"");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, "\" atField=\"%s\" position=\"%s\"/>", field.name, posname);
	} else {
		gf_fprintf(sdump->trace, "DELETE ");
		if (inf->pos==-1) gf_fprintf(sdump->trace, "%s ", posname);
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, ".%s", field.name);
		if (inf->pos!=-1) gf_fprintf(sdump->trace, "[%d]", inf->pos);
		gf_fprintf(sdump->trace, "\n");
	}
	return GF_OK;
}


static GF_Err DumpNodeDelete(GF_SceneDumper *sdump, GF_Command *com)
{
	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		if (com->tag==GF_SG_NODE_DELETE_EX) {
			gf_fprintf(sdump->trace, "<Delete extended=\"deleteOrder\" atNode=\"");
		} else {
			gf_fprintf(sdump->trace, "<Delete atNode=\"");
		}
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, "\"/>\n");
	} else {
		if (com->tag==GF_SG_NODE_DELETE_EX) gf_fprintf(sdump->trace, "X");
		gf_fprintf(sdump->trace, "DELETE ");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, "\n");
	}
	return GF_OK;
}

static GF_Err DumpRouteDelete(GF_SceneDumper *sdump, GF_Command *com)
{
	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<Delete atRoute=\"");
		scene_dump_vrml_route_id(sdump, com->RouteID, com->def_name);
		gf_fprintf(sdump->trace, "\"/>\n");
	} else {
		gf_fprintf(sdump->trace, "DELETE ROUTE ");
		scene_dump_vrml_route_id(sdump, com->RouteID, com->def_name);
		gf_fprintf(sdump->trace, "\n");
	}
	return GF_OK;
}



static GF_Err DumpNodeReplace(GF_SceneDumper *sdump, GF_Command *com)
{
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *) gf_list_get(com->command_fields, 0);
	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<Replace atNode=\"");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, "\">");
		gf_dump_vrml_node(sdump, inf->new_node, 0, NULL);
		gf_fprintf(sdump->trace, "</Replace>\n");
	} else {
		gf_fprintf(sdump->trace, "REPLACE ");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, " BY ");
		gf_dump_vrml_node(sdump, inf->new_node, 0, NULL);
		gf_fprintf(sdump->trace, "\n");
	}
	return GF_OK;
}

static GF_Err DumpFieldReplace(GF_SceneDumper *sdump, GF_Command *com)
{
	GF_Err e;
	GF_FieldInfo field;
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *) gf_list_get(com->command_fields, 0);

	e = gf_node_get_field(com->node, inf->fieldIndex, &field);

	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<Replace atNode=\"");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, "\" atField=\"%s\" ", field.name);
	} else {
		gf_fprintf(sdump->trace, "REPLACE ");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, ".%s BY ", field.name);
	}

	switch (field.fieldType) {
	case GF_SG_VRML_SFNODE:
		if (sdump->XMLDump) gf_fprintf(sdump->trace, ">");
		gf_dump_vrml_node(sdump, inf->new_node, 0, NULL);
		if (sdump->XMLDump) gf_fprintf(sdump->trace, "</Replace>");
		else gf_fprintf(sdump->trace, "\n");
		break;
	case GF_SG_VRML_MFNODE:
	{
		GF_ChildNodeItem *tmp;
		if (sdump->XMLDump) {
			gf_fprintf(sdump->trace, ">");
		} else {
			gf_fprintf(sdump->trace, " [\n");
		}
		sdump->indent++;
		tmp = inf->node_list;
		while (tmp) {
			gf_dump_vrml_node(sdump, tmp->node, 1, NULL);
			tmp = tmp->next;
		}
		sdump->indent--;
		if (sdump->XMLDump) {
			gf_fprintf(sdump->trace, "</Replace>");
		} else {
			EndList(sdump, NULL);
		}
	}
	break;
	case GF_SG_VRML_SFCOMMANDBUFFER:
		if (sdump->XMLDump) {
			SFCommandBuffer *cb = (SFCommandBuffer*)inf->field_ptr;
			gf_fprintf(sdump->trace, ">\n");
			gf_sm_dump_command_list(sdump, cb->commandList, sdump->indent+1, 0);
			DUMP_IND(sdump);
			gf_fprintf(sdump->trace, "</Replace>\n");
		} else {
			SFCommandBuffer *cb = (SFCommandBuffer*)inf->field_ptr;
			gf_fprintf(sdump->trace, " {\n");
			gf_sm_dump_command_list(sdump, cb->commandList, sdump->indent+1, 0);
			DUMP_IND(sdump);
			gf_fprintf(sdump->trace, "}\n");
		}
		break;
	default:
		field.far_ptr = inf->field_ptr;
		gf_dump_vrml_simple_field(sdump, field, com->node);
		if (sdump->XMLDump) gf_fprintf(sdump->trace, "/>");
		gf_fprintf(sdump->trace, "\n");
	}
	return e;
}


static GF_Err DumpIndexReplace(GF_SceneDumper *sdump, GF_Command *com)
{
	char posname[20];
	GF_Err e;
	GF_FieldInfo field;
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *) gf_list_get(com->command_fields, 0);

	e = gf_node_get_field(com->node, inf->fieldIndex, &field);
	if (e) return e;
	if (gf_sg_vrml_is_sf_field(field.fieldType)) return GF_NON_COMPLIANT_BITSTREAM;

	switch (inf->pos) {
	case 0:
		strcpy(posname, "BEGIN");
		break;
	case -1:
		strcpy(posname, sdump->XMLDump ? "END" : "LAST");
		break;
	default:
		sprintf(posname, "%d", inf->pos);
		break;
	}

	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<Replace atNode=\"");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, "\" atField=\"%s\" position=\"%s\"", field.name, posname);
	} else {
		gf_fprintf(sdump->trace, "REPLACE ");
		if (inf->pos==-1) gf_fprintf(sdump->trace, "%s ", posname);
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, ".%s", field.name);
		if (inf->pos!=-1) gf_fprintf(sdump->trace, "[%d]", inf->pos);
		gf_fprintf(sdump->trace, " BY ");
	}

	if (field.fieldType == GF_SG_VRML_MFNODE) {
		if (sdump->XMLDump) gf_fprintf(sdump->trace, ">\n");
		gf_dump_vrml_node(sdump, inf->new_node, 0, NULL);
		gf_fprintf(sdump->trace, (sdump->XMLDump) ? "</Replace>\n" : "\n");
	} else {
		field.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);
		field.far_ptr = inf->field_ptr;
		gf_dump_vrml_simple_field(sdump, field, com->node);
		gf_fprintf(sdump->trace, sdump->XMLDump ? "/>\n" : "\n");
	}
	return GF_OK;
}


static GF_Err DumpXReplace(GF_SceneDumper *sdump, GF_Command *com)
{
	char posname[20];
	GF_Err e;
	GF_FieldInfo field, idxField;
	GF_Node *toNode, *target;
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *) gf_list_get(com->command_fields, 0);

	e = gf_node_get_field(com->node, inf->fieldIndex, &field);
	if (e) return e;

	toNode = target = NULL;
	/*indexed replacement with index given by other node field*/
	if (com->toNodeID) {
		toNode = gf_sg_find_node(com->in_scene, com->toNodeID);
		if (!toNode) return GF_NON_COMPLIANT_BITSTREAM;
		e = gf_node_get_field(toNode, com->toFieldIndex, &idxField);
		if (e) return e;
	}
	else {
		/*indexed replacement */
		if (inf->pos>=-1) {
			if (gf_sg_vrml_is_sf_field(field.fieldType)) return GF_NON_COMPLIANT_BITSTREAM;
			switch (inf->pos) {
			case 0:
				strcpy(posname, "BEGIN");
				break;
			case -1:
				strcpy(posname, sdump->XMLDump ? "END" : "LAST");
				break;
			default:
				sprintf(posname, "%d", inf->pos);
				break;
			}
			field.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);
		}
	}
	field.far_ptr = inf->field_ptr;

	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<Replace atNode=\"");
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, "\" atField=\"%s\"", field.name);

		if (toNode) {
			gf_fprintf(sdump->trace, " atIndexNode=\"");
			scene_dump_vrml_id(sdump, toNode);
			gf_fprintf(sdump->trace, "\" atIndexField=\"%s\"", idxField.name);

			field.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);
		}
		if (com->ChildNodeTag) {
			GF_FieldInfo cfield;
			GF_Node *cnode;

			if (com->ChildNodeTag>0) {
				cnode = gf_node_new(com->in_scene, com->ChildNodeTag);
			} else {
				GF_Proto *proto = gf_sg_find_proto(com->in_scene, -com->ChildNodeTag , NULL);
				if (!proto) return GF_SG_UNKNOWN_NODE;
				cnode = gf_sg_proto_create_instance(com->in_scene, proto);
			}
			if (!cnode) return GF_SG_UNKNOWN_NODE;
			gf_node_register(cnode, NULL);
			gf_node_get_field(cnode, com->child_field, &cfield);
			gf_fprintf(sdump->trace, " atChildField=\"%s\"", cfield.name);
			gf_node_unregister(cnode, NULL);

			field.fieldType = cfield.fieldType;
		}

		if (com->fromNodeID) {
			target = gf_sg_find_node(com->in_scene, com->fromNodeID);
			if (!target) return GF_NON_COMPLIANT_BITSTREAM;
			e = gf_node_get_field(target, com->fromFieldIndex, &idxField);
			if (e) return e;

			gf_fprintf(sdump->trace, " fromNode=\"");
			scene_dump_vrml_id(sdump, target);
			gf_fprintf(sdump->trace, "\" fromField=\"%s\">\n", idxField.name);
			return GF_OK;
		} else {
			if (inf->pos>=-1) gf_fprintf(sdump->trace, " position=\"%s\"", posname);
		}
	} else {
		gf_fprintf(sdump->trace, "XREPLACE ");
		if (inf->pos==-1) gf_fprintf(sdump->trace, "%s ", posname);
		scene_dump_vrml_id(sdump, com->node);
		gf_fprintf(sdump->trace, ".%s", field.name);
		if (toNode) {
			gf_fprintf(sdump->trace, "[");
			scene_dump_vrml_id(sdump, toNode);
			gf_fprintf(sdump->trace, ".%s]", idxField.name);
			field.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);
		}
		else if (inf->pos!=-1) gf_fprintf(sdump->trace, "[%d]", inf->pos);
		if (com->ChildNodeTag) {
			GF_FieldInfo cfield;
			GF_Node *cnode;
			if (com->ChildNodeTag>0) {
				cnode = gf_node_new(com->in_scene, com->ChildNodeTag);
			} else {
				GF_Proto *proto = gf_sg_find_proto(com->in_scene, -com->ChildNodeTag , NULL);
				if (!proto) return GF_SG_UNKNOWN_NODE;
				cnode = gf_sg_proto_create_instance(com->in_scene, proto);
			}
			if (!cnode) return GF_SG_UNKNOWN_NODE;
			gf_node_register(cnode, NULL);
			gf_node_get_field(cnode, com->child_field, &cfield);
			gf_fprintf(sdump->trace, ".%s", cfield.name);
			gf_node_unregister(cnode, NULL);
			field.fieldType = cfield.fieldType;
		}
		gf_fprintf(sdump->trace, " BY ");
	}

	if (field.fieldType == GF_SG_VRML_MFNODE) {
		if (sdump->XMLDump) gf_fprintf(sdump->trace, ">\n");
		gf_dump_vrml_node(sdump, inf->new_node, 0, NULL);
		gf_fprintf(sdump->trace, (sdump->XMLDump) ? "</Replace>\n" : "\n");
	} else {
		gf_dump_vrml_simple_field(sdump, field, com->node);
		gf_fprintf(sdump->trace, sdump->XMLDump ? "/>\n" : "\n");
	}
	return GF_OK;
}


static GF_Err DumpRouteReplace(GF_SceneDumper *sdump, GF_Command *com)
{
	const char *name;
	GF_Route r2;

	if (!scene_dump_vrml_find_route_name(sdump, com->RouteID, &name)) return GF_BAD_PARAM;

	memset(&r2, 0, sizeof(GF_Route));
	r2.FromNode = gf_dump_find_node(sdump, com->fromNodeID);
	r2.FromField.fieldIndex = com->fromFieldIndex;
	r2.ToNode = gf_dump_find_node(sdump, com->toNodeID);
	r2.ToField.fieldIndex = com->toFieldIndex;

	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<Replace atRoute=\"");
		scene_dump_vrml_route_id(sdump, com->RouteID, (char *) name);
		gf_fprintf(sdump->trace, "\">\n");
	} else {
		gf_fprintf(sdump->trace, "REPLACE ROUTE ");
		scene_dump_vrml_route_id(sdump, com->RouteID, (char *) name);
		gf_fprintf(sdump->trace, " BY ");
	}
	gf_dump_vrml_route(sdump, &r2, 1);
	if (sdump->XMLDump ) gf_fprintf(sdump->trace, "</Replace>");
	return GF_OK;
}

static GF_Err gf_dump_vrml_route(GF_SceneDumper *sdump, GF_Route *r, u32 dump_type)
{
	char toNodeBuf[100], fromNodeBuf[100], *to_node_p, *from_node_p;
	const char *node_name;
	u32 id;
	if (!r->is_setup) {
		gf_node_get_field(r->FromNode, r->FromField.fieldIndex, &r->FromField);
		gf_node_get_field(r->ToNode, r->ToField.fieldIndex, &r->ToField);
		r->is_setup = 1;
	}
	if (!r->FromNode || !r->ToNode) return GF_BAD_PARAM;

	if (sdump->XMLDump || !dump_type) DUMP_IND(sdump);

	to_node_p = toNodeBuf;
	from_node_p = fromNodeBuf;
	node_name = gf_node_get_name_and_id(r->FromNode, &id);
	if (node_name) {
		const char *to_name;
		from_node_p = (char *)node_name;
		to_name = gf_node_get_name(r->ToNode);
		if (to_name) {
			to_node_p = (char *) to_name;
		} else {
			id = gf_node_get_id(r->ToNode);
			sprintf(toNodeBuf, "node_%d", id);
		}
	} else {
		sprintf(fromNodeBuf, "N%d", id-1);
		sprintf(toNodeBuf, "N%d", gf_node_get_id(r->ToNode) - 1);
	}
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<ROUTE");
		if (r->ID) {
			StartAttribute(sdump, "DEF");
			scene_dump_vrml_route_id(sdump, r->ID, r->name);
			EndAttribute(sdump);
		}
		gf_fprintf(sdump->trace, " fromNode=\"%s\" fromField=\"%s\" toNode=\"%s\" toField=\"%s\"/>\n", from_node_p, r->FromField.name, to_node_p, r->ToField.name);
	} else {
		if (dump_type==2) gf_fprintf(sdump->trace, "ROUTE ");
		if (r->ID) {
			gf_fprintf(sdump->trace, "DEF ");
			scene_dump_vrml_route_id(sdump, r->ID, r->name);
			gf_fprintf(sdump->trace, " ");
		}
		if (dump_type==1) {
			gf_fprintf(sdump->trace, "%s.%s TO %s.%s\n", from_node_p, r->FromField.name, to_node_p, r->ToField.name);
		} else {
			if (dump_type!=2) gf_fprintf(sdump->trace, "ROUTE ");
			gf_fprintf(sdump->trace, "%s.%s TO %s.%s\n", from_node_p, r->FromField.name, to_node_p, r->ToField.name);
		}
	}
	return GF_OK;
}


static GF_Err DumpProtos(GF_SceneDumper *sdump, GF_List *protoList)
{
#ifdef GPAC_DISABLE_VRML
	return GF_OK;
#else
	u32 i, j, count;
	GF_FieldInfo field;
	GF_Err e;
	GF_SceneGraph *prev_sg;
	GF_Proto *proto, *prev_proto;

	prev_proto = sdump->current_proto;

	i=0;
	while ((proto = (GF_Proto*)gf_list_enum(protoList, &i))) {
		sdump->current_proto = proto;

		DUMP_IND(sdump);
		if (!sdump->XMLDump) {
			gf_fprintf(sdump->trace, proto->ExternProto.count ? "EXTERNPROTO " : "PROTO ");
			gf_fprintf(sdump->trace, "%s [\n", proto->Name);
		} else {
			gf_fprintf(sdump->trace, "<ProtoDeclare name=\"%s\" protoID=\"%d\"", proto->Name, proto->ID);
			if (proto->ExternProto.count) {
				gf_fprintf(sdump->trace, " locations=\"");
				gf_dump_vrml_sffield(sdump, GF_SG_VRML_SFURL, &proto->ExternProto.vals[0], 0, NULL);
				gf_fprintf(sdump->trace, "\"");
			}
			gf_fprintf(sdump->trace, ">\n");
		}

		if (sdump->XMLDump && sdump->X3DDump) gf_fprintf(sdump->trace, "<ProtoInterface>");

		sdump->indent++;
		count = gf_list_count(proto->proto_fields);
		for (j=0; j<count; j++) {
			GF_ProtoFieldInterface *pf = (GF_ProtoFieldInterface *)gf_list_get(proto->proto_fields, j);
			field.fieldIndex = pf->ALL_index;
			field.eventType = pf->EventType;
			field.far_ptr = pf->def_value;
			field.fieldType = pf->FieldType;
			field.name = pf->FieldName;
			field.NDTtype = NDT_SFWorldNode;
			field.on_event_in = NULL;

			gf_dump_vrml_dyn_field(sdump, NULL, field, pf->QP_Type ? 1 : 0);

			if (!pf->QP_Type) continue;

			/*dump interface coding - BT/TXT extensions, not supported by any other tool*/
			sdump->indent++;
			DUMP_IND(sdump);
			if (sdump->XMLDump) {
				const char *quant_catname = "unknown";
#ifndef GPAC_DISABLE_BIFS
				switch (pf->QP_Type) {
				case QC_3DPOS: quant_catname = "position3D"; break;
				case QC_2DPOS: quant_catname = "position2D"; break;
				case QC_ORDER: quant_catname = "drawingOrder"; break;
				case QC_COLOR: quant_catname = "color"; break;
				case QC_TEXTURE_COORD: quant_catname = "textureCoordinate"; break;
				case QC_ANGLE: quant_catname = "angle"; break;
				case QC_SCALE: quant_catname = "scale"; break;
				case QC_INTERPOL_KEYS: quant_catname = "keys"; break;
				case QC_NORMALS: quant_catname = "normals"; break;
				case QC_ROTATION: quant_catname = "rotations"; break;
				case QC_SIZE_3D: quant_catname = "size3D"; break;
				case QC_SIZE_2D: quant_catname = "size2D"; break;
				case QC_LINEAR_SCALAR: quant_catname = "linear"; break;
				case QC_COORD_INDEX:quant_catname = "coordIndex"; break;
				}
#endif
				gf_fprintf(sdump->trace, "<InterfaceCodingParameters quantCategoy=\"%s\"", quant_catname);
			} else {
				gf_fprintf(sdump->trace, "{QP %d", pf->QP_Type);
			}
#ifndef GPAC_DISABLE_BIFS
			if (pf->QP_Type==QC_LINEAR_SCALAR) gf_fprintf(sdump->trace, sdump->XMLDump ? " nbBits=\"%d\"" : " nbBits %d", pf->NumBits);
			if (pf->hasMinMax) {
				switch (pf->QP_Type) {
				case QC_LINEAR_SCALAR:
				case QC_COORD_INDEX:
					if (sdump->XMLDump) {
						gf_fprintf(sdump->trace, " intMin=\"%d\" intMax=\"%d\"", *((SFInt32 *)pf->qp_min_value), *((SFInt32 *)pf->qp_max_value));
					} else {
						gf_fprintf(sdump->trace, " b {%d %d}", *((SFInt32 *)pf->qp_min_value), *((SFInt32 *)pf->qp_max_value));
					}
					break;
				default:
					if (sdump->XMLDump) {
						gf_fprintf(sdump->trace, " floatMin=\"%g\" floatMax=\"%g\"", FIX2FLT( *((SFFloat *)pf->qp_min_value) ), FIX2FLT( *((SFFloat *)pf->qp_max_value) ));
					} else {
						gf_fprintf(sdump->trace, " b {%g %g}", FIX2FLT( *((SFFloat *)pf->qp_min_value) ), FIX2FLT( *((SFFloat *)pf->qp_max_value) ) );
					}
					break;
				}
			}
#endif
			gf_fprintf(sdump->trace, sdump->XMLDump ? "/>\n" : "}\n");
			sdump->indent--;
			if (sdump->XMLDump) {
				DUMP_IND(sdump);
				gf_fprintf(sdump->trace, "</field>\n");
			}

		}

		sdump->indent--;
		DUMP_IND(sdump);
		if (!sdump->XMLDump) {
			gf_fprintf(sdump->trace, "]");
		} else if (sdump->X3DDump) gf_fprintf(sdump->trace, "</ProtoInterface>\n");

		if (proto->ExternProto.count) {
			if (!sdump->XMLDump) {
				gf_fprintf(sdump->trace, " \"");
				gf_dump_vrml_sffield(sdump, GF_SG_VRML_SFURL, &proto->ExternProto.vals[0], 0, NULL);
				gf_fprintf(sdump->trace, "\"\n\n");
			} else {
				gf_fprintf(sdump->trace, "</ProtoDeclare>\n");
			}
			continue;
		}
		if (!sdump->XMLDump) gf_fprintf(sdump->trace, " {\n");

		sdump->indent++;

		if (sdump->XMLDump && sdump->X3DDump) gf_fprintf(sdump->trace, "<ProtoBody>\n");

		e = DumpProtos(sdump, proto->sub_graph->protos);
		if (e) return e;

		/*set namespace to the proto one*/
		prev_sg = sdump->sg;
		sdump->sg = gf_sg_proto_get_graph(proto);

		count = gf_list_count(proto->node_code);
		for (j=0; j<count; j++) {
			GF_Node *n = (GF_Node*)gf_list_get(proto->node_code, j);
			gf_dump_vrml_node(sdump, n, 1, NULL);
		}
		count = gf_list_count(proto->sub_graph->Routes);
		for (j=0; j<count; j++) {
			GF_Route *r = (GF_Route *)gf_list_get(proto->sub_graph->Routes, j);
			if (r->IS_route) continue;
			gf_dump_vrml_route(sdump, r, 0);
		}

		if (sdump->XMLDump && sdump->X3DDump) gf_fprintf(sdump->trace, "</ProtoBody>\n");

		/*restore namespace*/
		sdump->sg = prev_sg;

		sdump->indent--;
		DUMP_IND(sdump);
		if (!sdump->XMLDump) {
			gf_fprintf(sdump->trace, "}\n");
		} else {
			gf_fprintf(sdump->trace, "</ProtoDeclare>\n");
		}
	}
	sdump->current_proto = prev_proto;
	return GF_OK;
#endif
}

static GF_Err DumpSceneReplace(GF_SceneDumper *sdump, GF_Command *com)
{
	if (sdump->XMLDump) {
		if (!sdump->X3DDump) {
			StartElement(sdump, "Replace");
			EndElementHeader(sdump, 1);
			sdump->indent++;
		}
		//scene tag is already dumped with X3D header
		if (!sdump->X3DDump) StartElement(sdump, "Scene");
		if (!sdump->X3DDump && com->use_names) {
			StartAttribute(sdump, "USENAMES");
			gf_fprintf(sdump->trace, "%s", com->use_names ? "true" : "false");
			EndAttribute(sdump);
		}
		if (!sdump->X3DDump) EndElementHeader(sdump, 1);
		sdump->indent++;
	} else {
		if (!sdump->skip_scene_replace) {
			DUMP_IND(sdump);
			gf_fprintf(sdump->trace, "REPLACE SCENE BY ");
		}
	}
	DumpProtos(sdump, com->new_proto_list);
	gf_dump_vrml_node(sdump, com->node, 0, NULL);
	if (!sdump->XMLDump) gf_fprintf(sdump->trace, "\n\n");

	if (com->aggregated) {
		u32 i, count;
		count = gf_list_count(com->node->sgprivate->scenegraph->Routes);
		for (i=0; i<count; i++) {
			GF_Route *r = (GF_Route *)gf_list_get(com->node->sgprivate->scenegraph->Routes, i);
			if (r->IS_route) continue;
			gf_dump_vrml_route(sdump, r, 0);
		}
	}

	return GF_OK;
}

static GF_Err DumpProtoInsert(GF_SceneDumper *sdump, GF_Command *com)
{
	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "<Insert extended=\"proto\">\n");
	} else {
		gf_fprintf(sdump->trace, "INSERTPROTO [\n");
	}
	sdump->indent++;
	DumpProtos(sdump, com->new_proto_list);
	sdump->indent--;
	DUMP_IND(sdump);
	if (sdump->XMLDump) {
		gf_fprintf(sdump->trace, "</Insert>\n");
	} else {
		gf_fprintf(sdump->trace, "]\n");
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_VRML*/


#ifndef GPAC_DISABLE_SVG
static char *lsr_format_node_id(GF_Node *n, u32 NodeID, char *str)
{
	if (!n) sprintf(str, "N%d", NodeID-1);
	else {
		const char *name = gf_node_get_name_and_id(n, &NodeID);
		if (name) sprintf(str, "%s", name);
		else sprintf(str, "N%d", NodeID - 1);
	}
	return str;
}

static char szLSRName[1024];

static char *sd_get_lsr_namespace(GF_SceneGraph *sg)
{
	char *lsrns = (char *) gf_sg_get_namespace_qname(sg, GF_XMLNS_LASER);
	if (lsrns) {
		sprintf(szLSRName, "%s:", lsrns);
		return szLSRName;
	}
	return "";
}

static GF_Err DumpLSRNewScene(GF_SceneDumper *sdump, GF_Command *com)
{
	char *lsrns = sd_get_lsr_namespace(com->in_scene);
	gf_fprintf(sdump->trace, "<%sNewScene>\n", lsrns);
	gf_dump_svg_element(sdump, com->node, NULL, 0);
	gf_fprintf(sdump->trace, "</%sNewScene>\n", lsrns);
	return GF_OK;
}

static GF_Err DumpLSRAddReplaceInsert(GF_SceneDumper *sdump, GF_Command *com)
{
	char szID[100];
	Bool is_text = 0;
	GF_CommandField *f;
	char *lsrns = sd_get_lsr_namespace(com->in_scene);

	const char *com_name = (com->tag==GF_SG_LSR_REPLACE) ? "Replace" : ( (com->tag==GF_SG_LSR_ADD) ? "Add" : "Insert" );

	DUMP_IND(sdump);

	gf_fprintf(sdump->trace, "<%s%s ref=\"%s\" ", lsrns, com_name, lsr_format_node_id(com->node, com->RouteID, szID));
	f = (GF_CommandField *) gf_list_get(com->command_fields, 0);
	if (f && (f->pos>=0) ) gf_fprintf(sdump->trace, "index=\"%d\" ", f->pos);
	if (f) {
		GF_FieldInfo info;
		if (!f->new_node && !f->node_list) {
			char *att_name = NULL;
			if (f->fieldType==SVG_Transform_Scale_datatype) att_name = "scale";
			else if (f->fieldType==SVG_Transform_Rotate_datatype) att_name = "rotation";
			else if (f->fieldType==SVG_Transform_Translate_datatype) att_name = "translation";
			else if (f->fieldIndex==(u32) -1) att_name = "textContent";
			else {
				if (!com->node) return GF_NON_COMPLIANT_BITSTREAM;
				att_name = (char*) gf_svg_get_attribute_name(com->node, f->fieldIndex);
			}

			gf_fprintf(sdump->trace, "attributeName=\"%s\" ", att_name);
			if (f->field_ptr) {
				char *att;
				info.far_ptr = f->field_ptr;
				info.fieldIndex = f->fieldIndex;
				info.fieldType = f->fieldType;
				info.name = att_name;

				if ((s32) f->pos >= 0) {
					att = gf_svg_dump_attribute_indexed(com->node, &info);
				} else {
					att = gf_svg_dump_attribute(com->node, &info);
				}
				gf_fprintf(sdump->trace, "value=\"%s\" ", att ? att : "");
				if (att) gf_free(att);
			}

			if (com->fromNodeID) {
				GF_FieldInfo op_info;
				GF_Node *op = gf_sg_find_node(sdump->sg, com->fromNodeID);
				gf_fprintf(sdump->trace, "operandElementId=\"%s\" ", lsr_format_node_id(op, com->RouteID, szID));
				gf_node_get_field(op, com->fromFieldIndex, &op_info);
				gf_fprintf(sdump->trace, "operandAttributeName=\"%s\" ", op_info.name);
			}

			gf_fprintf(sdump->trace, "/>\n");
			return GF_OK;
		}
		if (f->new_node && f->new_node->sgprivate->tag==TAG_DOMText) is_text = 1;
		/*if fieldIndex (eg attributeName) is set, this is children replacement*/
		if (f->fieldIndex>0)
			gf_fprintf(sdump->trace, "attributeName=\"children\" ");
	}


	gf_fprintf(sdump->trace, ">");
	if (!is_text) {
		gf_fprintf(sdump->trace, "\n");
		sdump->indent++;
	}
	if (f) {
		if (f->new_node) {
			gf_dump_svg_element(sdump, f->new_node, com->node, 0);
		} else if (f->node_list) {
			GF_ChildNodeItem *list = f->node_list;
			while (list) {
				gf_dump_svg_element(sdump, list->node, com->node, 0);
				list = list->next;
			}
		}
	}
	if (!is_text) {
		sdump->indent--;
		DUMP_IND(sdump);
	}
	gf_fprintf(sdump->trace, "</%s%s>\n", lsrns, com_name);
	return GF_OK;
}

static GF_Err DumpLSRDelete(GF_SceneDumper *sdump, GF_Command *com)
{
	char szID[1024];
	GF_CommandField *f;
	char *lsrns = sd_get_lsr_namespace(com->in_scene);
	DUMP_IND(sdump);
	gf_fprintf(sdump->trace, "<%sDelete ref=\"%s\" ", lsrns, lsr_format_node_id(com->node, com->RouteID, szID));
	f = (GF_CommandField *) gf_list_get(com->command_fields, 0);
	if (f && (f->pos>=0) ) gf_fprintf(sdump->trace, "index=\"%d\" ", f->pos);
	gf_fprintf(sdump->trace, "/>\n");
	return GF_OK;
}
#ifdef GPAC_UNUSED_FUNC
static GF_Err DumpLSRInsert(GF_SceneDumper *sdump, GF_Command *com)
{
	return GF_OK;
}

static GF_Err SD_SetSceneGraph(GF_SceneDumper *sdump, GF_SceneGraph *sg)
{
	if (sdump) sdump->sg = sg;
	return GF_OK;
}

static GF_Err DumpLSRClean(GF_SceneDumper *sdump, GF_Command *com)
{
	return GF_OK;
}

static GF_Err DumpLSRRestore(GF_SceneDumper *sdump, GF_Command *com)
{
	return GF_OK;
}
static GF_Err DumpLSRSave(GF_SceneDumper *sdump, GF_Command *com)
{
	return GF_OK;
}
#endif /*GPAC_UNUSED_FUNC*/

static GF_Err DumpLSRSendEvent(GF_SceneDumper *sdump, GF_Command *com)
{
	char szID[1024];
	char *lsrns = sd_get_lsr_namespace(com->in_scene);
	DUMP_IND(sdump);
	gf_fprintf(sdump->trace, "<%sSendEvent ref=\"%s\" event=\"%s\"", lsrns,
	        lsr_format_node_id(com->node, com->RouteID, szID),
	        gf_dom_event_get_name(com->send_event_name)
	       );
	if ((com->send_event_name <= GF_EVENT_MOUSEWHEEL)
		|| (com->send_event_name == GF_EVENT_MOUSEOUT)
		|| (com->send_event_name == GF_EVENT_MOUSEOVER)
	) {
		gf_fprintf(sdump->trace, " pointvalue=\"%g %g\"", FIX2FLT(com->send_event_x), FIX2FLT(com->send_event_y) );
	}

	switch (com->send_event_name) {
	case GF_EVENT_KEYDOWN:
	case GF_EVENT_LONGKEYPRESS:
	case GF_EVENT_REPEAT_KEY:
	case GF_EVENT_SHORT_ACCESSKEY:
		if (com->send_event_integer) {
			gf_fprintf(sdump->trace, " stringvalue=\"%s\"", gf_dom_get_key_name(com->send_event_integer) );
			break;
		}
	default:
		if (com->send_event_integer)
			gf_fprintf(sdump->trace, " intvalue=\"%d\"", com->send_event_integer);
		if (com->send_event_string)
			gf_fprintf(sdump->trace, " stringvalue=\"%s\"", com->send_event_string);
		break;
	}

	gf_fprintf(sdump->trace, "/>\n");
	return GF_OK;
}
static GF_Err DumpLSRActivate(GF_SceneDumper *sdump, GF_Command *com)
{
	char szID[1024];
	char *lsrns = sd_get_lsr_namespace(com->in_scene);
	DUMP_IND(sdump);
	if (com->tag==GF_SG_LSR_ACTIVATE) {
		gf_fprintf(sdump->trace, "<%sActivate ref=\"%s\" />\n", lsrns, lsr_format_node_id(com->node, com->RouteID, szID));
	} else {
		gf_fprintf(sdump->trace, "<%sDeactivate ref=\"%s\" />\n", lsrns, lsr_format_node_id(com->node, com->RouteID, szID));
	}
	return GF_OK;
}

#endif

GF_EXPORT
GF_Err gf_sm_dump_command_list(GF_SceneDumper *sdump, GF_List *comList, u32 indent, Bool skip_first_replace)
{
	GF_Err e;
	u32 i, count;
	u32 prev_ind;
#ifndef GPAC_DISABLE_VRML
	u32 remain = 0, has_scene_replace = 0;
#endif
	Bool prev_skip;

	if (!sdump || !sdump->trace|| !comList || !sdump->sg) return GF_BAD_PARAM;

	prev_skip = sdump->skip_scene_replace;
	sdump->skip_scene_replace = skip_first_replace;
	prev_ind  = sdump->indent;
	sdump->indent = indent;

	e = GF_OK;
	count = gf_list_count(comList);
	for (i=0; i<count; i++) {
		GF_Command *com = (GF_Command *) gf_list_get(comList, i);
		if (i
#ifndef GPAC_DISABLE_VRML
			&& !remain
#endif
			&& (sdump->X3DDump || (sdump->dump_mode==GF_SM_DUMP_VRML))
		) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[Scene Dump] MPEG-4 Commands found, not supported in %s - skipping\n", sdump->X3DDump ? "X3D" : "VRML"));
			break;
		}
#ifndef GPAC_DISABLE_VRML
		if (has_scene_replace && (com->tag != GF_SG_ROUTE_INSERT)) {
			has_scene_replace = 0;
			if (sdump->XMLDump) {
				sdump->indent--;
				EndElement(sdump, "Scene", 1);
				sdump->indent--;
				EndElement(sdump, "Replace", 1);
			} else {
				DUMP_IND(sdump);
				gf_fprintf(sdump->trace, "\nAT 0 {\n");
				sdump->indent++;
			}
		}
#endif

		switch (com->tag) {
#ifndef GPAC_DISABLE_VRML
		/*insert commands*/
		case GF_SG_NODE_INSERT:
			e = DumpNodeInsert(sdump, com);
			break;
		case GF_SG_INDEXED_INSERT:
			e = DumpIndexInsert(sdump, com);
			break;
		case GF_SG_ROUTE_INSERT:
			e = DumpRouteInsert(sdump, com, has_scene_replace);
			if (remain) remain--;
			break;
		/*delete commands*/
		case GF_SG_NODE_DELETE:
			e = DumpNodeDelete(sdump, com);
			break;
		case GF_SG_INDEXED_DELETE:
			e = DumpIndexDelete(sdump, com);
			break;
		case GF_SG_ROUTE_DELETE:
			e = DumpRouteDelete(sdump, com);
			break;
		/*replace commands*/
		case GF_SG_NODE_REPLACE:
			e = DumpNodeReplace(sdump, com);
			break;
		case GF_SG_FIELD_REPLACE:
			e = DumpFieldReplace(sdump, com);
			break;
		case GF_SG_INDEXED_REPLACE:
			e = DumpIndexReplace(sdump, com);
			break;
		case GF_SG_ROUTE_REPLACE:
			e = DumpRouteReplace(sdump, com);
			break;
		case GF_SG_XREPLACE:
			e = DumpXReplace(sdump, com);
			break;
		case GF_SG_SCENE_REPLACE:
			/*we don't support replace scene in conditional*/
			gf_assert(!sdump->current_com_list);
			sdump->current_com_list = comList;
			e = DumpSceneReplace(sdump, com);
			sdump->current_com_list = NULL;
			has_scene_replace = 1;
			remain = count - i - 1;
			break;
		/*extended commands*/
		case GF_SG_PROTO_INSERT:
			e = DumpProtoInsert(sdump, com);
			break;
		case GF_SG_PROTO_DELETE_ALL:
			DUMP_IND(sdump);
			if (sdump->XMLDump) {
				gf_fprintf(sdump->trace, "<Delete extended=\"allProtos\"/>\n");
			} else {
				gf_fprintf(sdump->trace, "DELETEPROTO ALL\n");
			}
			e = GF_OK;
			break;
		case GF_SG_PROTO_DELETE:
		{
			u32 j;
			DUMP_IND(sdump);
			if (sdump->XMLDump) {
				gf_fprintf(sdump->trace, "<Delete extended=\"protos\" value=\"");
			} else {
				gf_fprintf(sdump->trace, "DELETEPROTO [");
			}
			for (j=0; j<com->del_proto_list_size; j++) {
				if (j) gf_fprintf(sdump->trace, " ");
				gf_fprintf(sdump->trace, "%d", com->del_proto_list[j]);
			}
			if (sdump->XMLDump) {
				gf_fprintf(sdump->trace, "\"/>\n");
			} else {
				gf_fprintf(sdump->trace, "]\n");
			}
			e = GF_OK;
		}
		break;
		case GF_SG_GLOBAL_QUANTIZER:
			e = DumpGlobalQP(sdump, com);
			break;
		case GF_SG_MULTIPLE_REPLACE:
			e = DumpMultipleReplace(sdump, com);
			break;
		case GF_SG_MULTIPLE_INDEXED_REPLACE:
			e = DumpMultipleIndexedReplace(sdump, com);
			break;
		case GF_SG_NODE_DELETE_EX:
			e = DumpNodeDelete(sdump, com);
			break;

#endif


#ifndef GPAC_DISABLE_SVG
		/*laser commands*/
		case GF_SG_LSR_NEW_SCENE:
			e = DumpLSRNewScene(sdump, com);
			break;
		case GF_SG_LSR_ADD:
			e = DumpLSRAddReplaceInsert(sdump, com);
			break;
		case GF_SG_LSR_CLEAN:
			//e = DumpLSRClean(sdump, com);
			break;
		case GF_SG_LSR_REPLACE:
			e = DumpLSRAddReplaceInsert(sdump, com);
			break;
		case GF_SG_LSR_DELETE:
			e = DumpLSRDelete(sdump, com);
			break;
		case GF_SG_LSR_INSERT:
			e = DumpLSRAddReplaceInsert(sdump, com);
			break;
		case GF_SG_LSR_RESTORE:
			//e = DumpLSRRestore(sdump, com);
			break;
		case GF_SG_LSR_SAVE:
			//e = DumpLSRSave(sdump, com);
			break;
		case GF_SG_LSR_SEND_EVENT:
			e = DumpLSRSendEvent(sdump, com);
			break;
		case GF_SG_LSR_ACTIVATE:
		case GF_SG_LSR_DEACTIVATE:
			e = DumpLSRActivate(sdump, com);
			break;
#endif
		}
		if (e) break;


		if (sdump->skip_scene_replace
#ifndef GPAC_DISABLE_VRML
			&& !has_scene_replace
#endif
		) {
			sdump->skip_scene_replace = 0;
			if (!sdump->XMLDump && (i+1<count)) {
				DUMP_IND(sdump);
				gf_fprintf(sdump->trace, "\nAT 0 {\n");
				sdump->indent++;
			}
		}
	}

#ifndef GPAC_DISABLE_VRML
	if (remain && !sdump->XMLDump) {
		sdump->indent--;
		DUMP_IND(sdump);
		gf_fprintf(sdump->trace, "}\n");
	}

	if (has_scene_replace && sdump->XMLDump) {
		sdump->indent--;
		if (!sdump->X3DDump) {
			EndElement(sdump, "Scene", 1);
			sdump->indent--;
			EndElement(sdump, "Replace", 1);
		}
	}
#endif

	sdump->indent = prev_ind;
	sdump->skip_scene_replace = prev_skip;
	return e;
}

#ifndef GPAC_DISABLE_SVG
void gf_dump_svg_element(GF_SceneDumper *sdump, GF_Node *n, GF_Node *parent, Bool is_root)
{
	GF_ChildNodeItem *list;
	char attName[100], *attValue;
	u32 nID;
	SVG_Element *svg = (SVG_Element *)n;
	GF_FieldInfo info;
	SVGAttribute *att;
	u32 tag, ns;
	if (!n) return;

	nID = gf_node_get_id(n);
	tag = n->sgprivate->tag;
	/*remove undef listener/handlers*/
	if (!nID) {
		switch (tag) {
		case TAG_SVG_listener:
			if ((0) && gf_node_get_attribute_by_tag(n, TAG_XMLEV_ATT_handler, 0, 0, &info)==GF_OK) {
				if (((XMLRI*)info.far_ptr)->target && !gf_node_get_id(((XMLRI*)info.far_ptr)->target) )
					return;
			}
			break;
		case TAG_SVG_handler:
			/*this handler was not declared in the graph*/
			if (!n->sgprivate->parents || (n->sgprivate->parents->node != parent))
				return;
			break;
		case TAG_DOMText:
		{
			GF_DOMText *txt = (GF_DOMText *)n;
			if (txt->textContent) {
				if ((txt->type==GF_DOM_TEXT_CDATA)
					|| (parent && (parent->sgprivate->tag == TAG_SVG_script))
					|| (parent && (parent->sgprivate->tag == TAG_SVG_handler))
				) {
					gf_fprintf(sdump->trace, "<![CDATA[");
					gf_fprintf(sdump->trace, "%s", txt->textContent);
					gf_fprintf(sdump->trace, "]]>");
				} else if (txt->type==GF_DOM_TEXT_REGULAR) {
					scene_dump_utf_string(sdump, 0, txt->textContent);
				}
			}
		}
		return;
		}
	}

	if (!sdump->in_text) {
		DUMP_IND(sdump);
	}

	/*register all namespaces specified on this element */
	gf_xml_push_namespaces((GF_DOMNode *)n);

	gf_fprintf(sdump->trace, "<%s", gf_node_get_class_name(n));
	ns = gf_xml_get_element_namespace(n);

	if (nID) {
		char attID[100];
		gf_fprintf(sdump->trace, " id=\"%s\"", lsr_format_node_id(n, 0, attID));
	}
	att = svg->attributes;
	while (att) {
		if (att->data_type==SVG_ID_datatype) {
			att = att->next;
			continue;
		}

		info.fieldIndex = att->tag;
		info.fieldType = att->data_type;
		if (att->tag==TAG_DOM_ATT_any) {
			u32 att_ns = ((GF_DOMFullAttribute*)att)->xmlns;
			info.name = ((GF_DOMFullAttribute*)att)->name;
			if ((att_ns != ns) && strncmp(info.name, "xmlns", 5)) {
				sprintf(attName, "%s:%s", gf_sg_get_namespace_qname(gf_node_get_graph(n), att_ns), ((GF_DOMFullAttribute*)att)->name);
				info.name = attName;
			}
		} else {
			info.name = gf_svg_get_attribute_name(n, att->tag);
		}

		if (att->data_type==XMLRI_datatype) {
			XMLRI *xlink = (XMLRI *)att->data;
			if (xlink->type==XMLRI_ELEMENTID) {
				if (!xlink->target || !gf_node_get_id((GF_Node*)xlink->target) ) {
					att = att->next;
					continue;
				}
				if (parent && (parent == (GF_Node *) xlink->target)) {
					att = att->next;
					continue;
				}
			}
			else if (xlink->type==XMLRI_STREAMID) {
				gf_fprintf(sdump->trace, " %s=\"#stream%d\"", info.name, xlink->lsr_stream_id);
				att = att->next;
				continue;
			} else {
				gf_fprintf(sdump->trace, " %s=\"%s\"", info.name, xlink->string);
				att = att->next;
				continue;
			}
		}
		info.far_ptr = att->data;
		attValue = gf_svg_dump_attribute((GF_Node*)svg, &info);
		if (attValue) {
			if (/*strcmp(info.name, "xmlns") &&*/ (info.fieldType = (u32) strlen(attValue)))
				gf_fprintf(sdump->trace, " %s=\"%s\"", info.name, attValue);
			gf_free(attValue);
		}
		att = att->next;
	}

	gf_dom_event_dump_listeners(n, sdump->trace);
	if (svg->children) {
		gf_fprintf(sdump->trace, ">");
	} else {
		gf_fprintf(sdump->trace, "/>");
		return;
	}

	if (n->sgprivate->tag==TAG_LSR_conditional) {
		GF_DOMUpdates *up = svg->children ? (GF_DOMUpdates *)svg->children->node : NULL;
		sdump->indent++;
		if (up && (up->sgprivate->tag==TAG_DOMUpdates)) {
			if (gf_list_count(up->updates)) {
				gf_fprintf(sdump->trace, "\n");
				gf_sm_dump_command_list(sdump, up->updates, sdump->indent, 0);
			} else if (up->data) {
				gf_fprintf(sdump->trace, "<!-- WARNING: LASeR scripts cannot be dumped at run-time -->\n");
			}
		}
		sdump->indent--;
		DUMP_IND(sdump);
		gf_fprintf(sdump->trace, "</%s>\n", gf_node_get_class_name(n));
		return;
	}

	if (tag==TAG_SVG_text || tag==TAG_SVG_textArea) sdump->in_text = 1;
	sdump->indent++;
	list = svg->children;
	while (list) {
		if (!sdump->in_text) gf_fprintf(sdump->trace, "\n");
		gf_dump_svg_element(sdump, list->node, n, 0);
		list = list->next;
	}
	if (!sdump->in_text) gf_fprintf(sdump->trace, "\n");
	sdump->indent--;
	if (!sdump->in_text) DUMP_IND(sdump);
	gf_fprintf(sdump->trace, "</%s>", gf_node_get_class_name(n));
	if (tag==TAG_SVG_text || tag==TAG_SVG_textArea) sdump->in_text = 0;
	/*removes all namespaces specified on this element */
	gf_xml_pop_namespaces((GF_DOMNode *)n);
}
#endif

static void gf_sm_dump_saf_hdr(GF_SceneDumper *dumper, char *unit_name, u64 au_time, Bool is_rap)
{
	gf_fprintf(dumper->trace, "<saf:%s", unit_name);
	if (au_time) gf_fprintf(dumper->trace, " time=\""LLD"\"", au_time);
	if (is_rap) gf_fprintf(dumper->trace, " rap=\"true\"");
	gf_fprintf(dumper->trace, ">\n");
}

static void dump_od_to_saf(GF_SceneDumper *dumper, GF_AUContext *au, u32 indent)
{
	u32 i, count;

	count = gf_list_count(au->commands);
	for (i=0; i<count; i++) {
		u32 j, c2;
		GF_ODUpdate *com = (GF_ODUpdate *)gf_list_get(au->commands, i);
		if (com->tag != GF_ODF_OD_UPDATE_TAG) continue;

		c2 = gf_list_count(com->objectDescriptors);
		for (j=0; j<c2; j++) {
			GF_ObjectDescriptor *od = (GF_ObjectDescriptor *)gf_list_get(com->objectDescriptors, j);
			GF_ESD *esd = (GF_ESD *) gf_list_get(od->ESDescriptors, 0);
			GF_MuxInfo *mux;
			if (!esd || (esd->tag != GF_ODF_ESD_TAG)) {
				if (od->URLString) {
					gf_fprintf(dumper->trace, "<saf:RemoteStreamHeader streamID=\"stream%d\" url=\"%s\"", au->owner->ESID, od->URLString);
					if (au->timing) gf_fprintf(dumper->trace, " time=\""LLD"\"", au->timing);
					gf_fprintf(dumper->trace, "/>\n");
				}
				continue;
			}
			mux = (GF_MuxInfo *)gf_list_get(esd->extensionDescriptors, 0);
			if (!mux || (mux->tag!=GF_ODF_MUXINFO_TAG)) mux = NULL;


			gf_fprintf(dumper->trace, "<saf:mediaHeader streamID=\"stream%d\"", esd->ESID);
			if (esd->decoderConfig) {
				gf_fprintf(dumper->trace, " streamType=\"%d\" objectTypeIndication=\"%d\" timeStampResolution=\"%d\"", esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication, au->owner->timeScale);
			}
			if (au->timing) gf_fprintf(dumper->trace, " time=\""LLD"\"", au->timing);
			if (mux && mux->file_name) gf_fprintf(dumper->trace, " source=\"%s\"", mux->file_name);
			gf_fprintf(dumper->trace, "/>\n");
		}


	}
	gf_fprintf(dumper->trace, "</saf:mediaUnit>\n");
}

#ifndef GPAC_DISABLE_SVG
static GF_Err SD_DumpDOMElement(GF_SceneDumper *sdump, GF_DOMFullNode *node)
{
	const char *ns;
	u32 child_type = 0;
	GF_DOMFullAttribute *att;
	GF_ChildNodeItem *child;
	GF_DOMText *txt;
	ns = gf_sg_get_namespace_qname(node->sgprivate->scenegraph, node->ns);

	DUMP_IND(sdump);
	if (ns) gf_fprintf(sdump->trace, "<%s:%s", ns, node->name);
	else gf_fprintf(sdump->trace, "<%s", node->name);
	att = (GF_DOMFullAttribute *)node->attributes;
	while (att) {
		gf_fprintf(sdump->trace, " %s=\"%s\"", att->name, (char *) att->data);
		att = (GF_DOMFullAttribute *)att->next;
	}
	if (!node->children) {
		gf_fprintf(sdump->trace, "/>\n");
		return GF_OK;
	}
	gf_fprintf(sdump->trace, ">");
	sdump->indent++;
	child = node->children;
	while (child) {
		switch(child->node->sgprivate->tag) {
		case TAG_DOMFullNode:
			if (!child_type) gf_fprintf(sdump->trace, "\n");
			child_type = 1;
			SD_DumpDOMElement(sdump, (GF_DOMFullNode*)child->node);
			break;
		case TAG_DOMText:
			child_type = 2;
			txt = (GF_DOMText *)child->node;
			if (txt->type==GF_DOM_TEXT_REGULAR) {
				scene_dump_utf_string(sdump, 0, txt->textContent);
			} else if (txt->type==GF_DOM_TEXT_CDATA) {
				gf_fprintf(sdump->trace, "<![CDATA[");
				gf_fprintf(sdump->trace, "%s", txt->textContent);
				gf_fprintf(sdump->trace, "]]>");
			}
			break;
		}
		child = child->next;
	}

	sdump->indent--;
	if (child_type!=2) {
		DUMP_IND(sdump);
	}

	if (ns) gf_fprintf(sdump->trace, "</%s:%s>\n", ns, node->name);
	else gf_fprintf(sdump->trace, "</%s>\n", node->name);

	return GF_OK;
}
#endif

GF_EXPORT
GF_Err gf_sm_dump_graph(GF_SceneDumper *sdump, Bool skip_proto, Bool skip_routes)
{
	u32 tag;
	if (!sdump->trace || !sdump->sg || !sdump->sg->RootNode) return GF_BAD_PARAM;

	tag = sdump->sg->RootNode->sgprivate->tag;

	if (tag<=GF_NODE_RANGE_LAST_X3D) {
		gf_dump_setup(sdump, NULL);

		if (sdump->XMLDump) {
			StartElement(sdump, "Scene");
			EndElementHeader(sdump, 1);
			sdump->indent++;
		}

#ifndef GPAC_DISABLE_VRML
		GF_Err e;
		if (!skip_proto) {
			e = DumpProtos(sdump, sdump->sg->protos);
			if (e) return e;
		}

		if (sdump->X3DDump) {
			GF_ChildNodeItem *list = ((GF_ParentNode *)sdump->sg->RootNode)->children;
			while (list) {
				gf_dump_vrml_node(sdump, list->node, 0, NULL);
				list = list->next;
			}
		} else {
			gf_dump_vrml_node(sdump, sdump->sg->RootNode, 0, NULL);
		}
		if (!sdump->XMLDump) gf_fprintf(sdump->trace, "\n\n");
		if (!skip_routes) {
			GF_Route *r;
			u32 i=0;
			while ((r = (GF_Route*)gf_list_enum(sdump->sg->Routes, &i))) {
				if (r->IS_route || (r->graph!=sdump->sg)) continue;
				e = gf_dump_vrml_route(sdump, r, 0);
				if (e) return e;
			}
		}
		if (sdump->XMLDump) {
			sdump->indent--;
			EndElement(sdump, "Scene", 1);
		}
#endif /*GPAC_DISABLE_VRML*/

		gf_dump_finalize(sdump, NULL);
		return GF_OK;
	}
#ifndef GPAC_DISABLE_SVG
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		sdump->dump_mode = GF_SM_DUMP_SVG;
		gf_dump_setup(sdump, NULL);
		gf_dump_svg_element(sdump, sdump->sg->RootNode, NULL, 1);
		return GF_OK;
	}
	else if (tag==TAG_DOMFullNode) {
		sdump->dump_mode = GF_SM_DUMP_XML;
		gf_dump_setup(sdump, NULL);
		SD_DumpDOMElement(sdump, (GF_DOMFullNode*)sdump->sg->RootNode);
	}
#endif

	return GF_OK;
}




static void ReorderAUContext(GF_List *sample_list, GF_AUContext *au, Bool lsr_dump)
{
	u64 autime, time;
	u32 i;
	Bool has_base;
	GF_AUContext *ptr;

	/*
		this happens when converting from bt to xmt
		NOTE: Comment is wrong? this happens when just loading BT
	*/
	if (!au->timing_sec) {
		au->timing_sec = (Double) (s64) au->timing;
		/* Hack to avoid timescale=0 which happens when loading a BT with no SLConfig*/
		if (!au->owner->timeScale) au->owner->timeScale = 1000;
		au->timing_sec /= au->owner->timeScale;
	}
	/*this happens when converting from xmt to bt*/
	if (!au->timing) {
		gf_assert(au->owner->timeScale);
		au->timing = (u64) (au->timing_sec * au->owner->timeScale);
	}

	autime = au->timing + au->owner->imp_exp_time;
	has_base = 0;
	i=0;
	while ((ptr = (GF_AUContext*)gf_list_enum(sample_list, &i))) {
		time = ptr->timing + ptr->owner->imp_exp_time;
		if (
		    /*time ordered*/
		    (time > autime)
		    /*set bifs first for first AU*/
		    || (!has_base && (time == autime) && (ptr->owner->streamType < au->owner->streamType) )
		    /*set OD first for laser*/
		    || (lsr_dump && (au->owner->streamType==GF_STREAM_OD))
		) {
			gf_list_insert(sample_list, au, i-1);
			return;
		}

		has_base = 0;
		if ( (ptr->owner->streamType == au->owner->streamType) && (time == autime) ) has_base = 1;
	}
	gf_list_add(sample_list, au);
}


GF_EXPORT
GF_Err gf_sm_dump(GF_SceneManager *ctx, char *rad_name, Bool is_final_name, GF_SceneDumpFormat dump_mode)
{
	GF_Err e;
	GF_List *sample_list;
	Bool first_par;
	u32 i, j, indent, num_scene, num_od, first_bifs;
	Double time;
	GF_SceneDumper *dumper;
	GF_StreamContext *sc;
	GF_AUContext *au;
	Bool no_root_found = 1;

	num_scene = num_od = 0;
	indent = 0;
	dumper = gf_sm_dumper_new(ctx->scene_graph, rad_name, is_final_name, ' ', dump_mode);
	if (!dumper) {
		return GF_IO_ERR;
	}

	sample_list = gf_list_new();

	e = GF_OK;
	/*configure all systems streams we're dumping*/
	i=0;
	while ((sc = (GF_StreamContext*)gf_list_enum(ctx->streams, &i))) {

		switch (sc->streamType) {
		case GF_STREAM_SCENE:
			num_scene ++;
			break;
		case GF_STREAM_OD:
			num_od ++;
			break;
		default:
			continue;
		}

		j=0;
		while ((au = (GF_AUContext*)gf_list_enum(sc->AUs, &j))) {
			ReorderAUContext(sample_list, au, dumper->LSRDump);
			if (dumper->dump_mode==GF_SM_DUMP_SVG) break;
		}
		if (dumper->dump_mode==GF_SM_DUMP_SVG) break;
	}
	first_bifs = (num_scene==1) ? 1 : 0;
	num_scene = (num_scene>1) ? 1 : 0;
	num_od = (num_od>1) ? 1 : 0;

	gf_dump_setup(dumper, (GF_Descriptor *) ctx->root_od);

#ifndef GPAC_DISABLE_SVG
	if (dumper->dump_mode==GF_SM_DUMP_SVG) {
		au = (GF_AUContext*)gf_list_get(sample_list, 0);
		GF_Command *com = NULL;
		if (au) com = (GF_Command*)gf_list_get(au->commands, 0);
		if (!au) {
			gf_dump_svg_element(dumper, dumper->sg->RootNode, NULL, 1);
		} else if (!com || (com->tag!=GF_SG_LSR_NEW_SCENE) || !com->node) {
			e = GF_NOT_SUPPORTED;
		} else {
			gf_dump_svg_element(dumper, com->node, NULL, 1);
		}
		gf_dump_finalize(dumper, (GF_Descriptor *) ctx->root_od);
		gf_sm_dumper_del(dumper);
		gf_list_del(sample_list);
		return e;
	}
#endif

	time = dumper->LSRDump ? -1 : 0;
	first_par = 0;

	while (gf_list_count(sample_list)) {
		au = (GF_AUContext*)gf_list_get(sample_list, 0);
		gf_list_rem(sample_list, 0);

		if (!dumper->XMLDump) {

			if (!first_bifs || (au->owner->streamType != GF_STREAM_SCENE) ) {
				if (au->flags & GF_SM_AU_RAP) gf_fprintf(dumper->trace, "RAP ");
				gf_fprintf(dumper->trace, "AT "LLD" ", au->timing);
				if ( (au->owner->streamType==GF_STREAM_OD && num_od) || (au->owner->streamType==GF_STREAM_SCENE && num_scene)) {
					gf_fprintf(dumper->trace, "IN %d ", au->owner->ESID);
				}
				gf_fprintf(dumper->trace, "{\n");
				indent++;
			}

			switch (au->owner->streamType) {
			case GF_STREAM_OD:
				if (dumper->LSRDump) {
					dump_od_to_saf(dumper, au, indent);
				} else {
#ifndef GPAC_DISABLE_OD_DUMP
					e = gf_odf_dump_com_list(au->commands, dumper->trace, indent+1, 0);
#endif
				}
				break;
			case GF_STREAM_SCENE:
				e = gf_sm_dump_command_list(dumper, au->commands, indent, first_bifs);
				break;
			}
			if (first_bifs) {
				first_bifs = 0;
				gf_fprintf(dumper->trace, "\n");

			} else {
				indent--;
				gf_fprintf(dumper->trace, "}\n\n");
			}
		}
		else {
			if (dumper->LSRDump) {
/*				if (time != au->timing_sec) {
					time = au->timing_sec;
				}
*/
			} else if (!time && !num_scene && first_bifs) {
			} else if (num_scene || num_od) {
				if (!first_par) {
					first_par = 1;
					indent += 1;
				} else {
					gf_fprintf(dumper->trace, " </par>\n");
				}
				gf_fprintf(dumper->trace, " <par begin=\"%g\" atES_ID=\"es%d\" isRAP=\"%s\">\n", au->timing_sec, au->owner->ESID, (au->flags & GF_SM_AU_RAP) ? "yes" : "no");
			} else if (au->timing_sec>time) {
				if (!first_par) {
					first_par = 1;
					indent += 1;
				} else {
					gf_fprintf(dumper->trace, " </par>\n");
				}
				gf_fprintf(dumper->trace, "<par begin=\"%g\">\n", au->timing_sec);
			}
			switch (au->owner->streamType) {
			case GF_STREAM_OD:
				if (dumper->LSRDump) {
					dump_od_to_saf(dumper, au, indent+1);
				} else {
#ifndef GPAC_DISABLE_OD_DUMP
					e = gf_odf_dump_com_list(au->commands, dumper->trace, indent+1, 1);
#endif
				}
				break;
			case GF_STREAM_SCENE:
				if (gf_list_count(au->commands)) {
					if (dumper->LSRDump)
						gf_sm_dump_saf_hdr(dumper, "sceneUnit", au->timing, au->flags & GF_SM_AU_RAP);

					e = gf_sm_dump_command_list(dumper, au->commands, indent+1, first_bifs);
					first_bifs = 0;
					no_root_found = 0;

					if (dumper->LSRDump)
						gf_fprintf(dumper->trace, "</saf:sceneUnit>\n");
				}
				break;
			}
			time = au->timing_sec;
		}
		if (dumper->X3DDump || (dumper->dump_mode==GF_SM_DUMP_VRML)) break;
	}

#ifndef GPAC_DISABLE_VRML
	if (no_root_found && ctx->scene_graph->RootNode) {
		GF_Route *r;
		DumpProtos(dumper, ctx->scene_graph->protos);
		gf_dump_vrml_node(dumper, ctx->scene_graph->RootNode, 0, NULL);
		i=0;
		gf_fprintf(dumper->trace, "\n");
		while ((r = (GF_Route*)gf_list_enum(dumper->sg->Routes, &i))) {
			if (r->IS_route || (r->graph!=dumper->sg)) continue;
			e = gf_dump_vrml_route(dumper, r, 0);
			if (e) return e;
		}
	}
#endif


	/*close command*/
	if (!dumper->X3DDump && first_par) gf_fprintf(dumper->trace, " </par>\n");

	if (gf_list_count(sample_list) && (dumper->X3DDump || (dumper->dump_mode==GF_SM_DUMP_VRML)) ) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[Scene Dump] MPEG-4 Commands found, not supported in %s - skipping\n", dumper->X3DDump ? "X3D" : "VRML"));
	}

	gf_dump_finalize(dumper, (GF_Descriptor *) ctx->root_od);
	gf_sm_dumper_del(dumper);
	gf_list_del(sample_list);
	return e;
}

#endif /*GPAC_DISABLE_SCENE_DUMP*/
