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

#define MAX_URI_LENGTH		4096

typedef struct _svg_parser SVGParser;

SVGParser *NewSVGParser();

void SVGParser_Init(SVGParser *parser, char *filename, void *graph);

/* DOM Reading from file */
GF_Err SVGParser_ParseLASeR(SVGParser *parser);
GF_Err SVGParser_ParseFullDoc(SVGParser *parser);

/* Progressive SAX reading from File */
/* size of a chunk of file read at each cycle */
#define  SAX_MAX_CHARS		150
GF_Err SVGParser_InitProgressiveFileChunk(SVGParser *parser);
GF_Err SVGParser_ParseProgressiveFileChunk(SVGParser *parser);

/* Progressive SAX reading from Buffer */
GF_Err SVGParser_ParseMemoryFirstChunk(SVGParser *parser, unsigned char *inBuffer, u32 inBufferLength);
GF_Err SVGParser_ParseMemoryNextChunk(SVGParser *parser, unsigned char *inBuffer, u32 inBufferLength);

void SVGParser_Terminate(SVGParser *parser);

/*returns true if download is complete*/
Bool SVG_CheckDownload(SVGParser *parser);

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
	/*defined by dummy_in plugin*/
	SVGLOADER_OTI_SVG = 2,
	/*defined by dummy_in plugin*/
	SVGLOADER_OTI_LASERML = 3,
	/*defined by ourselves - streamType 3 (scene description) for SVG streaming*/
	SVGLOADER_OTI_STREAMING_SVG	  = 10
};

enum {
	SVG_LOAD_DOM = 0, 
	SVG_LOAD_SAX = 1,
	SVG_LOAD_SAX_PROGRESSIVE = 2
};

typedef enum {
	UNDEF		= 0, 
	STARTSVG	= 1, 
	SVGCONTENT	= 2, 
	UNKNOWN		= 3, 
	FINISHSVG	= 4, 
	ERROR		= 5
} SVGParserSaxStatesEnum;


struct _svg_parser
{
	/* Pointer to the module using this parser */
	void *gpac_module;

	/* Only needed in Process data to attach graph to renderer */
	void *inline_scene;

	/* This graph is the same as the one in inline_scene, this pointer is just for ease 
	   and to separate as much as possible the SVG Parser from the GF_InlineScene structure */
	GF_SceneGraph *graph;

	/* Warning: interpretation of status is different 
	   depending on the type of parser (sax/dom/streaming, LASeR/SVG)
	   SAX SVG uses the following:
	   0 = not initialized, 
	   1 = initialized,
	   2 = SVG start tag parsed,
	   3 = running ...
	   4 = error */
	u8 loader_status;

	/* determines if the parser is from XML file (SAX or DOM) or from Access Unit, 
	   and if it's SVG or LASeR */
	u8 oti;

	GF_Err last_error;

	/* File name in case of a parser reading from an XML file (LASeR or SVG, Progressive or not)
	   it is not used if content is AU framed */
	char *file_name;
	/*total file size as given by cache*/
	u32 file_size;

	/* Unresolved begin/end value */
	GF_List *unresolved_timing_elements;

	/* Unresolved href value */
	GF_List *unresolved_hrefs;

	/* Animations that could not be parsed because the target was not resolved */
	GF_List *defered_animation_elements;

	/* List of nodes with an ID */
	GF_List *ided_nodes;
	/* Numerical ID of the latest node with XML ID */
	u32 max_node_id;

	/* width and height of the scene */
	u32 svg_w, svg_h;

#ifdef USE_GPAC_CACHE_MECHANISM
	/* to handle 'data:' urls */
	u32 cacheID;
	char *temp_dir;
#endif

	/* LASeR parsing structures */
	xmlNodePtr sU;
	Bool needs_attachement;
	u32 stream_time;

	/* SAX related structures */

	/* number of bytes read in the file in case of progressive loading from an SVG file
	   it is unused in the case of SVG content stored in AU */
	u32 nb_bytes_read;

	FILE *sax_file;
	/* Libxml structures */
	xmlSAXHandlerPtr	sax_handler; 
	xmlParserCtxtPtr	sax_ctx;

    SVGParserSaxStatesEnum	sax_state;
    SVGParserSaxStatesEnum	prev_sax_state;

	s32					unknown_depth;

	/* to know the current parent of following nodes  */
	GF_List			*	svg_node_stack; 
	
	/* list of ENTITY declarations */
	GF_List			*	entities; 

	u32 load_type;
	/*progressive loading control:
	loads chunks by chunks (SAX), trying to spend less ms than specified in parsing at each run.
	a typical value for the refresh period is the frame simulation duration
	*/
	u32 sax_max_duration;
};


typedef struct {
	/* node is used in DOM parsing */
	xmlNode *node;

	/* Animation element being defered */
	SVGElement *animation_elt;
	/* Parent of the animation element */
	SVGElement *parent;

	/* id of the target element */
	char *		target_id;

	/* target element in case of local defered element */
	SVGElement *target;

	/* attributes which cannot be parsed until the type of the target attribute is known */
	char *attributeName;
	char *type; /* only for animateTransform */
	char *to;
	char *from;
	char *by;
	char *values;
} defered_element;

typedef struct {
	SVG_IRI *iri;
	SVGElement *elt;
} href_instance;

Bool		svg_has_been_IDed	(SVGParser *parser, xmlChar *node_name);
u32			svg_get_node_id		(SVGParser *parser, xmlChar *nodename);
void		svg_parse_element_id(SVGParser *parser, SVGElement *elt, char *nodename);

/* DOM related functions */
void		svg_parse_dom_attributes		(SVGParser *parser, xmlNodePtr node, SVGElement *elt, u8 anim_value_type, u8 anim_transform_type);
void		svg_parse_dom_children          (SVGParser *parser, xmlNodePtr node, SVGElement *elt);
void		svg_parse_dom_defered_animation (SVGParser *parser, xmlNodePtr node, SVGElement *animation_elt, SVGElement *parent);
SVGElement *svg_parse_dom_element			(SVGParser *parser, xmlNodePtr node, SVGElement *parent);

/*SAX related functions */
void		svg_parse_sax_defered_animation	(SVGParser *parser, SVGElement *animation_elt, defered_element local_de);
SVGElement *svg_parse_sax_element			(SVGParser *parser, const xmlChar *name, const xmlChar **attrs, SVGElement *parent);

#endif /*GPAC_DISABLE_SVG*/

#ifdef __cplusplus
}
#endif

#endif // _SVG_PARSER_H
