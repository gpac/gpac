/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2004-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Scene Graph Generator sub-project
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
#ifndef _SVGGEN_H_
#define  _SVGGEN_H_

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <gpac/list.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

/* if defined generates .c/.h directly in the appropriate GPAC source folders */
#undef LOCAL_SVG_NODES

/*
   Modes for generating SVG code
   - 1 means static allocation of attributes (including properties, use Tiny-1.2-NG)
   - 2 means static allocation of attributes (only useful properties on nodes, use Tiny-1.2-NG-noproperties)
   - 3 means dynamic allocation of attributes (including properties)
*/
static u32 generation_mode = 3;

#define RNG_NS "http://relaxng.org/ns/structure/1.0"
#define RNGA_NS "http://relaxng.org/ns/compatibility/annotations/1.0"
#define SVGA_NS "http://www.w3.org/2005/02/svg-annotations"

#define RNG_PREFIX "rng"
#define RNGA_PREFIX "rnga"
#define SVGA_PREFIX "svg"

#define COPYRIGHT "/*\n *			GPAC - Multimedia Framework C SDK\n *\n *			Authors: Cyril Concolato - Jean Le Feuvre\n *    Copyright (c) Telecom ParisTech 2000-2012 - All rights reserved\n *\n *  This file is part of GPAC / SVG Scene Graph sub-project\n *\n *  GPAC is free software; you can redistribute it and/or modify\n *  it under the terms of the GNU Lesser General Public License as published by\n *  the Free Software Foundation; either version 2, or (at your option)\n *  any later version.\n *\n *  GPAC is distributed in the hope that it will be useful,\n *  but WITHOUT ANY WARRANTY; without even the implied warranty of\n *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n *  GNU Lesser General Public License for more details.	\n *\n *  You should have received a copy of the GNU Lesser General Public\n *  License along with this library; see the file COPYING.  If not, write to\n *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.\n *\n */\n"


/*
	type declarations
*/

typedef struct
{
	xmlChar *svg_name;
	char implementation_name[50];

	Bool has_svg_generic;
	Bool has_xml_generic;
	Bool has_media_properties;
	Bool has_properties;
	Bool has_opacity_properties;
	Bool has_focus;
	Bool has_xlink;
	Bool has_timing;
	Bool has_sync;
	Bool has_animation;
	Bool has_conditional;
	Bool has_transform;
	Bool has_xy;

	GF_List *attributes;
	GF_List *generic_attributes;

	u32 nb_atts;
} SVGGenElement;

typedef struct {
	xmlChar *svg_name;
	char implementation_name[50];
	xmlChar *svg_type;
	char impl_type[50];
	u8 animatable;
	u8 inheritable;
	Bool optional;
	xmlChar *default_value;
	u32 index;
} SVGGenAttribute;
SVGGenAttribute *NewSVGGenAttribute();

typedef struct {
	char *name;
	char imp_name[50];
	GF_List *attrs;
	GF_List *attrgrps;
} SVGGenAttrGrp;



/*******************************************
 * Structures needed for static allocation *
 *******************************************/

static GF_List *globalAttrGrp;

/* SVG Generic */
static char *core[] = { "id", "class", "xml:id", "xml:base", "xml:lang", "xml:space", "externalResourceRequired" };

/* Media Properties */
static char *media_properties[] = {
	"audio-level", "display", "image-rendering", "pointer-events", "shape-rendering", "text-rendering",
	"viewport-fill", "viewport-fill-opacity", "visibility"
};

/* others */
static char *other_properties[] = {
	"color", "color-rendering", "display-align", "fill", "fill-opacity", "fill-rule",
	"font-family", "font-size", "font-style", "font-weight", "line-increment",
	"solid-color", "solid-opacity", "stop-color", "stop-opacity",
	"stroke", "stroke-dasharray", "stroke-dashoffset", "stroke-linecap", "stroke-linejoin", "stroke-miterlimit",
	"stroke-opacity", "stroke-width", "text-align", "text-anchor", "vector-effect"
};

/* only opacity on image */
static char *opacity_properties[] = {
	"opacity"
};

/* Focus */
static char *focus[] = {
	"focusHighlight", "focusable", "nav-down", "nav-down-left", "nav-down-right",
	"nav-left", "nav-next", "nav-prev", "nav-right", "nav-up", "nav-up-left", "nav-up-right"
};

/* Xlink */
static char *xlink[] = {
	"xlink:href", "xlink:show", "xlink:title", "xlink:actuate", "xlink:role", "xlink:arcrole", "xlink:type"
};

/* Timing */
static char *timing[] = {
	"begin", "end", "dur", "repeatCount", "repeatDur", "restart", "min", "max", "fill", "clipBegin", "clipEnd"
};

/* Sync */
static char *sync[] = {
	"syncBehavior", "syncBehaviorDefault", "syncTolerance", "syncToleranceDefault", "syncMaster", "syncReference"
};

/* Animation */
static char *anim[] = {
	"attributeName", "attributeType", "to", "from", "by", "values",
	"type", "calcMode", "keySplines", "keyTimes", "accumulate", "additive", "lsr:enabled"
};

/* Conditional Processing */
static char *conditional[] = {
	"requiredExtensions", "requiredFeatures", "requiredFonts", "requiredFormats", "systemLanguage"
};

typedef struct {
	int array_length;
	char **array; // mapping of constructs to the RNG definition
} _atts;

static _atts generic_attributes[] = {
	{ 7, core },
	{ 26, other_properties },
	{ 9, media_properties },
	{ 1, opacity_properties },
	{ 12, focus },
	{ 7, xlink },
	{ 11, timing },
	{ 6, sync },
	{ 13, anim },
	{ 5, conditional}
};

FILE *BeginFile(u32 type);
void EndFile(FILE *f, u32 type);


#endif // _SVGGEN_H_
