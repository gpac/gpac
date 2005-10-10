/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Loader module
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


#ifndef _SVG_PARSER_H
#define _SVG_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>

typedef struct _svg_parser SVGParser;

SVGParser *NewSVGParser();

void SVGParser_Init(SVGParser *parser, char *filename, void *graph);

GF_Err SVGParser_ParseLASeR(SVGParser *parser);
GF_Err SVGParser_ParseFullDoc(SVGParser *parser);
GF_Err SVGParser_ParseProgressiveDoc(SVGParser *parser);

void SVGParser_Terminate(SVGParser *parser);


#ifndef GPAC_DISABLE_SVG

#include <gpac/scenegraph_svg.h>
#include <gpac/modules/codec.h>


#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <gpac/nodes_svg.h>
#include <gpac/internal/scenegraph_dev.h>

/*enable this if you want to link SVG loader against static libxml2 & iconv*/
#if 0
#if (_MSC_VER == 1200) && (WINVER < 0x0500)
long _ftol( double );
long _ftol2( double dblSource ) { return _ftol( dblSource ); }
#endif
#endif

/* comment this line if you don't want the GPAC cache mechanism */
#define USE_GPAC_CACHE_MECHANISM


enum {
	SVGLOADER_OTI_FULL_SVG		  = 2,
	SVGLOADER_OTI_PROGRESSIVE_SVG = 3,
	SVGLOADER_OTI_FULL_LASERML	  = 4
};

GF_Err SVGParser_ParseLASeR(SVGParser *parser);

struct _svg_parser
{
	/* 
		Only needed in Process data to attach graph to renderer 
	*/
	void *inline_scene;

	/* This graph is the same as the one in inline_scene, this pointer is just for ease 
	   and to separate as much as possible the SVG Parser from the GF_InlineScene structure */
	GF_SceneGraph *graph;

	/* 0 = not initialized, 1 = initialized*/
	u8 status;
	u8 oti;

	GF_Err last_error;

	char *fileName;

	/*svg fragments stuff*/
	char *temp_dir;
	char *szOriginalRad;
	u32 seg_idx;

	/* Unresolved begin/end value */
	GF_List *unresolved_timing_elements;

	GF_List *unresolved_hrefs;

	GF_List *defered_animation_elements;

	GF_List *ided_nodes;
	u32 max_node_id;

	u32 svg_w, svg_h;

	/* Document wallclock begin UTC since 1970 */
	u32 begin_sec, begin_ms;

#ifdef USE_GPAC_CACHE_MECHANISM
	/* to handle 'data:' urls */
	u32 cacheID;
	/* store the scene decoder to get the module manager for cache objects */
	GF_SceneDecoder *sdec;
#endif

	/*sceneUpdate node for LASeR parsing*/
	xmlNodePtr sU;
	Bool needs_attachement;
	u32 stream_time;
};

#endif /*GPAC_DISABLE_SVG*/


#ifdef __cplusplus
}
#endif

#endif // _SVG_PARSER_H
