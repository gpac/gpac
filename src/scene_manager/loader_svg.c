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

#include <gpac/scene_manager.h>
#include <gpac/constants.h>
#include <gpac/utf.h>
#include <gpac/xml.h>
#include <gpac/internal/bifs_dev.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_svg.h>


typedef struct
{
	GF_SceneLoader *load;
	GF_Err last_error;
	XMLParser xml_parser;

} SVGParser;

static GF_Err svg_report(SVGParser *parser, GF_Err e, char *format, ...)
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

static void svg_node_start(void *sax_cbck, const char *node_name, const char *name_space)
{
	SVGParser *parser = sax_cbck;
}
static void svg_node_end(void *sax_cbck, const char *node_name, const char *name_space)
{
	SVGParser *parser = sax_cbck;
}
static void svg_attributes_parsed(void *sax_cbck, GF_List *attributes)
{
	SVGParser *parser = sax_cbck;
}

GF_Err gf_sm_load_init_SVG(GF_SceneLoader *load)
{
	GF_Err e;
	SVGParser *parser;

	if (!load->ctx || !load->fileName) return GF_BAD_PARAM;
	GF_SAFEALLOC(parser, sizeof(SVGParser));

	parser->load = load;
	load->loader_priv = parser;

	parser->xml_parser.sax_xml_node_start = svg_node_start;
	parser->xml_parser.sax_xml_node_end = svg_node_end;
	parser->xml_parser.sax_xml_attributes_parsed = svg_attributes_parsed;
	parser->xml_parser.sax_cbck = parser;

	e = xml_sax_parse_file(&parser->xml_parser, load->fileName);
	if (e) {
		svg_report(parser, e, "Unable to open file %s", load->fileName);
		free(parser);
		return e;
	}
	return GF_OK;
}

GF_Err gf_sm_load_run_SVG(GF_SceneLoader *load)
{
	return GF_OK;
}

GF_Err gf_sm_load_done_SVG(GF_SceneLoader *load)
{
	SVGParser *parser = (SVGParser *)load->loader_priv;
	if (!parser) return GF_OK;
	free(parser);
	load->loader_priv = NULL;
	return GF_OK;
}