/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004-2005
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

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <gpac/list.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>


#define RNG_NS "http://relaxng.org/ns/structure/1.0"
#define RNGA_NS "http://relaxng.org/ns/compatibility/annotations/1.0"
#define SVGA_NS "http://www.w3.org/2005/02/svg-annotations"

#define RNG_PREFIX "rng"
#define RNGA_PREFIX "rnga"
#define SVGA_PREFIX "svg"

#define COPYRIGHT "/*\n *			GPAC - Multimedia Framework C SDK\n *\n *			Authors: Cyril Concolato - Jean Le Feuvre\n *    Copyright (c)2004-200X ENST - All rights reserved\n *\n *  This file is part of GPAC / SVG Scene Graph sub-project\n *\n *  GPAC is free software; you can redistribute it and/or modify\n *  it under the terms of the GNU Lesser General Public License as published by\n *  the Free Software Foundation; either version 2, or (at your option)\n *  any later version.\n *\n *  GPAC is distributed in the hope that it will be useful,\n *  but WITHOUT ANY WARRANTY; without even the implied warranty of\n *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n *  GNU Lesser General Public License for more details.	\n *\n *  You should have received a copy of the GNU Lesser General Public\n *  License along with this library; see the file COPYING.  If not, write to\n *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.\n *\n */\n"

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
} SVGElement;

typedef struct {
	xmlChar *svg_name; 
	char implementation_name[50];
	xmlChar *svg_type; 
	char impl_type[50]; 
	u8 animatable; 
	u8 inheritable; 
	Bool optional;
	xmlChar *default_value;
} SVGAttribute;

typedef struct {
	char *name;
	char imp_name[50];
	GF_List *attrs;
	GF_List *attrgrps;
} SVGAttrGrp;

SVGAttribute *NewSVGAttribute()
{
	SVGAttribute *att;
	GF_SAFEALLOC(att, sizeof(SVGAttribute))
	return att;
}

void deleteSVGAttribute(SVGAttribute **p)
{
	xmlFree((*p)->svg_name);
	xmlFree((*p)->svg_type);
	free(*p);
	*p = NULL;
}

SVGAttrGrp *NewAttrGrp()
{
	SVGAttrGrp *tmp;
	GF_SAFEALLOC(tmp, sizeof(SVGAttrGrp))
	tmp->attrs = gf_list_new();
	tmp->attrgrps = gf_list_new();
	return tmp;
}

SVGElement *NewSVGElement() 
{
	SVGElement *elt;
	GF_SAFEALLOC(elt, sizeof(SVGElement));
	if (elt) {
		elt->attributes = gf_list_new();
		elt->generic_attributes = gf_list_new();
	}
	return elt;
}

void deleteSVGElement(SVGElement **p)
{
	u32 i;
	xmlFree((*p)->svg_name);
	for (i = 0; i < gf_list_count((*p)->attributes); i++) {
		SVGAttribute *a = gf_list_get((*p)->attributes, i);
		deleteSVGAttribute(&a);
	}
	gf_list_del((*p)->attributes);
	free(*p);
	*p = NULL;
}

static GF_List *sortElements(GF_List *elements)
{
	u32 i, j;
	GF_List *sorted_elements = gf_list_new();

	for (i = 0; i< gf_list_count(elements); i++) {
		u8 is_added = 0;
		SVGElement *elt = gf_list_get(elements, i);
		for (j = 0; j < gf_list_count(sorted_elements); j++) {
			SVGElement *selt = gf_list_get(sorted_elements, j);
			if (strcmp(elt->svg_name, selt->svg_name) < 0) {
				gf_list_insert(sorted_elements, elt, j);
				is_added = 1;
				break;
			}
		}
		if (!is_added) gf_list_add(sorted_elements, elt);
	}

	gf_list_del(elements);
	return sorted_elements;
}

static GF_List *sortAttrGrp(GF_List *attgrps)
{
	u32 i, j;
	GF_List *sorted_attgrps = gf_list_new();

	for (i = 0; i< gf_list_count(attgrps); i++) {
		u8 is_added = 0;
		SVGAttrGrp *grp = gf_list_get(attgrps, i);
		for (j = 0; j < gf_list_count(sorted_attgrps); j++) {
			SVGAttrGrp *sgrp = gf_list_get(sorted_attgrps, j);
			if (strcmp(grp->name, sgrp->name) < 0) {
				gf_list_insert(sorted_attgrps, grp, j);
				is_added = 1;
				break;
			}
		}
		if (!is_added) gf_list_add(sorted_attgrps, grp);
	}

	gf_list_del(attgrps);
	return sorted_attgrps;
}

static GF_List *sortAttr(GF_List *atts)
{
	u32 i, j;
	GF_List *sorted_atts = gf_list_new();

	for (i = 0; i< gf_list_count(atts); i++) {
		u8 is_added = 0;
		SVGAttribute *att = gf_list_get(atts, i);
		for (j = 0; j < gf_list_count(sorted_atts); j++) {
			SVGAttribute *satt = gf_list_get(sorted_atts, j);
			if (strcmp(att->svg_name, satt->svg_name) < 0) {
				gf_list_insert(sorted_atts, att, j);
				is_added = 1;
				break;
			}
		}
		if (!is_added) gf_list_add(sorted_atts, att);
	}

	gf_list_del(atts);
	return sorted_atts;
}

void svgNameToImplementationName(xmlChar *svg_name, char implementation_name[50]) {
	char *tmp;
	strcpy(implementation_name, svg_name);
	tmp = implementation_name;
	while ( (tmp = strchr(tmp, '.')) ) { *tmp='_'; tmp++; }
	tmp = implementation_name;
	while ( (tmp = strchr(tmp, '-')) ) { *tmp='_'; tmp++; }
	tmp = implementation_name;
	while ( (tmp = strchr(tmp, ':')) ) { *tmp='_'; tmp++; }
}

static Bool isGenericAttributesGroup(char *name) 
{
	if (!strcmp(name, "svg.Core.attr") ||
		!strcmp(name, "svg.CorePreserve.attr") ||
		!strcmp(name, "svg.External.attr") ||
		!strcmp(name, "svg.Properties.attr") ||
		!strcmp(name, "svg.Media.attr") ||
		!strcmp(name, "svg.MediaClip.attr") ||
		!strcmp(name, "svg.Opacity.attr") ||
		!strcmp(name, "svg.FocusHighlight.attr") || 
		!strcmp(name, "svg.Focus.attr") ||
		!strcmp(name, "svg.AnimateCommon.attr") ||
		!strcmp(name, "svg.XLinkEmbed.attr") ||
		!strcmp(name, "svg.XLinkRequired.attr") ||
		!strcmp(name, "svg.XLinkReplace.attr") ||
//		!strcmp(name, "svg.ContentType.attr") ||
		!strcmp(name, "svg.AnimateTiming.attr") ||
		!strcmp(name, "svg.AnimateTimingNoMinMax.attr") ||
		!strcmp(name, "svg.AnimateBegin.attr") || 
		!strcmp(name, "svg.AnimateTimingNoFillNoMinMax.attr") ||
		!strcmp(name, "svg.AnimateSync.attr") ||
		!strcmp(name, "svg.AnimateSyncDefault.attr") ||
		!strcmp(name, "svg.AnimateAttributeCommon.attr") ||
		!strcmp(name, "svg.AnimateToCommon.attr") ||
		!strcmp(name, "svg.AnimateValueCommon.attr") ||
		!strcmp(name, "svg.AnimateAdditionCommon.attr") ||
		!strcmp(name, "svg.AnimateTypeCommon.attr") ||
		!strcmp(name, "svg.Conditional.attr") ||
//		!strcmp(name, "svg.XY.attr") ||
		!strcmp(name, "svg.Transform.attr")) {
		return 1;
	} else {
		return 0;
	}
}

static Bool setGenericAttributesFlags(char *name, SVGElement *e) 
{
	Bool ret = 1;
	if (!strcmp(name, "svg.Core.attr") ||
		!strcmp(name, "svg.CorePreserve.attr") ||
		!strcmp(name, "svg.External.attr")) {
		e->has_svg_generic = 1; 
		e->has_xml_generic = 1;
	} else if (!strcmp(name, "svg.Properties.attr")) {
		e->has_properties = 1;
		e->has_media_properties = 1;
	} else if (!strcmp(name, "svg.Media.attr")) {
		e->has_media_properties = 1;
	} else if (!strcmp(name, "svg.Opacity.attr")) {
		e->has_opacity_properties = 1;
	} else if (!strcmp(name, "svg.FocusHighlight.attr") || 
		       !strcmp(name, "svg.Focus.attr")) {
		e->has_focus = 1;
	} else if (!strcmp(name, "svg.AnimateCommon.attr") ||
			   !strcmp(name, "svg.XLinkEmbed.attr") ||
			   !strcmp(name, "svg.XLinkRequired.attr") ||
			   !strcmp(name, "svg.XLinkReplace.attr")) { //||
//			   !strcmp(name, "svg.ContentType.attr")) {
		e->has_xlink = 1;
	} else if (!strcmp(name, "svg.AnimateTiming.attr") ||
			   !strcmp(name, "svg.AnimateTimingNoMinMax.attr") ||
			   !strcmp(name, "svg.AnimateBegin.attr") || 
			   !strcmp(name, "svg.AnimateTimingNoFillNoMinMax.attr")) {
		e->has_timing = 1;
	} else if (!strcmp(name, "svg.AnimateSync.attr") ||
		       !strcmp(name, "svg.AnimateSyncDefault.attr")) {
		e->has_sync= 1;
	} else if (!strcmp(name, "svg.AnimateAttributeCommon.attr") ||
			   !strcmp(name, "svg.AnimateToCommon.attr") ||
			   !strcmp(name, "svg.AnimateValueCommon.attr") ||
			   !strcmp(name, "svg.AnimateAdditionCommon.attr") ||
			   !strcmp(name, "svg.AnimateTypeCommon.attr")) {
		e->has_animation = 1;
	} else if (!strcmp(name, "svg.Conditional.attr")) {
		e->has_conditional = 1;
	} else if (!strcmp(name, "svg.Transform.attr")) {
		e->has_transform = 1;
	} else if (!strcmp(name, "svg.XY.attr")) {
		e->has_xy = 1;
	} else {
		ret = 0;
	}
	return ret;
}

static void flattenAttributeGroup(SVGAttrGrp attgrp, SVGElement *e, Bool all);

static void flattenAttributeGroups(GF_List *attrgrps, SVGElement *e, Bool all) 
{
	u32 i;
	for (i = 0; i < gf_list_count(attrgrps); i ++) {
		SVGAttrGrp *ag = gf_list_get(attrgrps, i);
		flattenAttributeGroup(*ag, e, all);
	} 
}

static void flattenAttributeGroup(SVGAttrGrp attgrp, SVGElement *e, Bool all) 
{
	u32 i;

	if (isGenericAttributesGroup(attgrp.name) && !all) {
		setGenericAttributesFlags(attgrp.name, e);
		flattenAttributeGroups(attgrp.attrgrps, e, 1);
		for (i = 0; i < gf_list_count(attgrp.attrs); i++) {
			gf_list_add(e->generic_attributes, gf_list_get(attgrp.attrs, i));
		}
	} else {
		flattenAttributeGroups(attgrp.attrgrps, e, all);
		for (i = 0; i < gf_list_count(attgrp.attrs); i++) {
			if (all) 
				gf_list_add(e->generic_attributes, gf_list_get(attgrp.attrs, i));
			else 
				gf_list_add(e->attributes, gf_list_get(attgrp.attrs, i));
		}
	}
}

static SVGAttribute *findAttribute(SVGElement *e, char *name) 
{
	u32 i;
	for (i = 0; i < gf_list_count(e->attributes); i++) {
		SVGAttribute *a = gf_list_get(e->attributes, i);
		if (!strcmp(a->svg_name, name)) return a;
	}
	for (i = 0; i < gf_list_count(e->generic_attributes); i++) {
		SVGAttribute *a = gf_list_get(e->generic_attributes, i);
		if (!strcmp(a->svg_name, name)) return a;
	}
	return NULL;
}

static u32 countAttributesAllInGroup(SVGAttrGrp *ag)
{
	u32 i, ret = 0;
	for (i = 0; i < gf_list_count(ag->attrgrps); i ++) {
		SVGAttrGrp *agtmp = gf_list_get(ag->attrgrps, i);
		ret += countAttributesAllInGroup(agtmp);
	} 
	ret += gf_list_count(ag->attrs);
	return ret;
}

/* XML related functions */
xmlNodeSetPtr findNodes( xmlXPathContextPtr ctxt, xmlChar * path ) 
{
    xmlXPathObjectPtr res = NULL;
  
    if ( ctxt->node != NULL && path != NULL ) {
        xmlXPathCompExprPtr comp;

        xmlDocPtr tdoc = NULL;
        xmlNodePtr froot = ctxt->node;

        comp = xmlXPathCompile( path );
        if ( comp == NULL ) {
            return NULL;
        }
        
        if ( ctxt->node->doc == NULL ) {
            /* if one XPaths a node from a fragment, libxml2 will
               refuse the lookup. this is not very usefull for XML
               scripters. thus we need to create a temporary document
               to make libxml2 do it's job correctly.
             */
            tdoc = xmlNewDoc( NULL );

            /* find refnode's root node */
            while ( froot != NULL ) {
                if ( froot->parent == NULL ) {
                    break;
                }
                froot = froot->parent;
            }
            xmlAddChild((xmlNodePtr)tdoc, froot);

            ctxt->node->doc = tdoc;
        }
       
        res = xmlXPathCompiledEval(comp, ctxt);

        xmlXPathFreeCompExpr(comp);

        if ( tdoc != NULL ) {
            /* after looking through a fragment, we need to drop the
               fake document again */
	    xmlSetTreeDoc(froot,NULL);
            froot->doc = NULL;
            tdoc->children = NULL;
            tdoc->last     = NULL;
            froot->parent  = NULL;
            ctxt->node->doc = NULL;

            xmlFreeDoc( tdoc );
        }
    }
	if (res && res->type == XPATH_NODESET)
		return res->nodesetval;
    else 
		return NULL;
}


/* definition of GPAC groups of SVG attributes */

void setAttributeType(SVGAttribute *att) 
{
	if (!att->svg_type) { /* if the type is not given in the RNG, we explicitely set it */
		if (!strcmp(att->svg_name, "textContent")) {
			strcpy(att->impl_type, "SVG_TextContent");
		} else if (!strcmp(att->svg_name, "class")) {
			strcpy(att->implementation_name, "_class");
			strcpy(att->impl_type, "SVG_String");
		} else if (!strcmp(att->svg_name, "visibility")) {
			strcpy(att->impl_type, "SVG_Visibility");
		} else if (!strcmp(att->svg_name, "display")) {
			strcpy(att->impl_type, "SVG_Display");
		} else if (!strcmp(att->svg_name, "stroke-linecap")) {
			strcpy(att->impl_type, "SVG_StrokeLineCap");
		} else if (!strcmp(att->svg_name, "stroke-dasharray")) {
			strcpy(att->impl_type, "SVG_StrokeDashArray");
		} else if (!strcmp(att->svg_name, "stroke-linejoin")) {
			strcpy(att->impl_type, "SVG_StrokeLineJoin");
		} else if (!strcmp(att->svg_name, "font-style")) {
			strcpy(att->impl_type, "SVG_FontStyle");
		} else if (!strcmp(att->svg_name, "font-weight")) {
			strcpy(att->impl_type, "SVG_FontWeight");
		} else if (!strcmp(att->svg_name, "text-anchor")) {
			strcpy(att->impl_type, "SVG_TextAnchor");
		} else if (!strcmp(att->svg_name, "fill")) {
			strcpy(att->impl_type, "SMIL_Fill");
		} else if (!strcmp(att->svg_name, "fill-rule")) {
			strcpy(att->impl_type, "SVG_FillRule");
		} else if (!strcmp(att->svg_name, "font-family")) {
			strcpy(att->impl_type, "SVG_FontFamily");
		} else if (!strcmp(att->svg_name, "calcMode")) {
			strcpy(att->impl_type, "SMIL_CalcMode");
		} else if (!strcmp(att->svg_name, "values")) {
			strcpy(att->impl_type, "SMIL_AnimateValues");
		} else if (!strcmp(att->svg_name, "keyTimes")) {
			strcpy(att->impl_type, "SMIL_KeyTimes");
		} else if (!strcmp(att->svg_name, "keySplines")) {
			strcpy(att->impl_type, "SMIL_KeySplines");
		} else if (!strcmp(att->svg_name, "keyPoints")) {
			strcpy(att->impl_type, "SMIL_KeyPoints");
		} else if (!strcmp(att->svg_name, "from") || 
				   !strcmp(att->svg_name, "to") || 
				   !strcmp(att->svg_name, "by")) {
			strcpy(att->impl_type, "SMIL_AnimateValue");
		} else if (!strcmp(att->svg_name, "additive")) {
			strcpy(att->impl_type, "SMIL_Additive");
		} else if (!strcmp(att->svg_name, "accumulate")) {
			strcpy(att->impl_type, "SMIL_Accumulate");
		} else if (!strcmp(att->svg_name, "begin") ||
				   !strcmp(att->svg_name, "end") 
				  ) {
			strcpy(att->impl_type, "SMIL_Times");
		} else if (!strcmp(att->svg_name, "clipBegin") ||
				   !strcmp(att->svg_name, "clipEnd")
				  ) {
			strcpy(att->impl_type, "SVG_Clock");
		} else if (!strcmp(att->svg_name, "min") ||
				   !strcmp(att->svg_name, "max") ||
				   !strcmp(att->svg_name, "dur") ||
				   !strcmp(att->svg_name, "repeatDur")
				  ) {
			strcpy(att->impl_type, "SMIL_Duration");
		} else if (!strcmp(att->svg_name, "repeat")) {
			strcpy(att->impl_type, "SMIL_Repeat");
		} else if (!strcmp(att->svg_name, "restart")) {
			strcpy(att->impl_type, "SMIL_Restart");
		} else if (!strcmp(att->svg_name, "repeatCount")) {
			strcpy(att->impl_type, "SMIL_RepeatCount");
		} else if (!strcmp(att->svg_name, "attributeName")) {
			strcpy(att->impl_type, "SMIL_AttributeName");
		} else if (!strcmp(att->svg_name, "type")) {
			strcpy(att->impl_type, "SVG_TransformType");
		} else if (!strcmp(att->svg_name, "font-size")) {
			strcpy(att->impl_type, "SVG_FontSize");
		} else if (!strcmp(att->svg_name, "viewBox")) {
			strcpy(att->impl_type, "SVG_ViewBox");
		} else if (!strcmp(att->svg_name, "preserveAspectRatio")) {
			strcpy(att->impl_type, "SVG_PreserveAspectRatio");
		} else if (!strcmp(att->svg_name, "zoomAndPan")) {
			strcpy(att->impl_type, "SVG_ZoomAndPan");
		} else if (!strcmp(att->svg_name, "path")) {
			strcpy(att->impl_type, "SVG_PathData");
		} else if (!strcmp(att->svg_name, "image-rendering")) {
			strcpy(att->impl_type, "SVG_RenderingHint");
		} else if (!strcmp(att->svg_name, "color-rendering")) {
			strcpy(att->impl_type, "SVG_RenderingHint");
		} else if (!strcmp(att->svg_name, "text-rendering")) {
			strcpy(att->impl_type, "SVG_RenderingHint");
		} else if (!strcmp(att->svg_name, "shape-rendering")) {
			strcpy(att->impl_type, "SVG_RenderingHint");
		} else if (!strcmp(att->svg_name, "pointer-events")) {
			strcpy(att->impl_type, "SVG_PointerEvents");
		} else if (!strcmp(att->svg_name, "vector-effect")) {
			strcpy(att->impl_type, "SVG_VectorEffect");
		} else if (!strcmp(att->svg_name, "vector-effect")) {
			strcpy(att->impl_type, "SVG_VectorEffect");
		} else if (!strcmp(att->svg_name, "display-align")) {
			strcpy(att->impl_type, "SVG_DisplayAlign");
		} else if (!strcmp(att->svg_name, "text-align")) {
			strcpy(att->impl_type, "SVG_TextAlign");
		} else if (!strcmp(att->svg_name, "propagate")) {
			strcpy(att->impl_type, "XMLEV_Propagate");
		} else if (!strcmp(att->svg_name, "defaultAction")) {
			strcpy(att->impl_type, "XMLEV_DefaultAction");
		} else if (!strcmp(att->svg_name, "phase")) {
			strcpy(att->impl_type, "XMLEV_Phase");
		} else if (!strcmp(att->svg_name, "syncBehavior")) {
			strcpy(att->impl_type, "SMIL_SyncBehavior");
		} else if (!strcmp(att->svg_name, "syncBehaviorDefault")) {
			strcpy(att->impl_type, "SMIL_SyncBehavior");
		} else if (!strcmp(att->svg_name, "attributeType")) {
			strcpy(att->impl_type, "SMIL_AttributeType");
		} else if (!strcmp(att->svg_name, "playbackOrder")) {
			strcpy(att->impl_type, "SVG_PlaybackOrder");
		} else if (!strcmp(att->svg_name, "timelineBegin")) {
			strcpy(att->impl_type, "SVG_TimelineBegin");
		} else if (!strcmp(att->svg_name, "xml:space")) {
			strcpy(att->impl_type, "XML_Space");
		} else if (!strcmp(att->svg_name, "snapshotTime")) {
			strcpy(att->impl_type, "SVG_Clock");
		} else if (!strcmp(att->svg_name, "version")) {
			strcpy(att->impl_type, "SVG_String");
		} else if (!strcmp(att->svg_name, "gradientUnits")) {
			strcpy(att->impl_type, "SVG_GradientUnit");
		} else if (!strcmp(att->svg_name, "baseProfile")) {
			strcpy(att->impl_type, "SVG_String");
		} else if (!strcmp(att->svg_name, "focusHighlight")) {
			strcpy(att->impl_type, "SVG_FocusHighlight");
		} else if (!strcmp(att->svg_name, "initialVisibility")) {
			strcpy(att->impl_type, "SVG_InitialVisibility");
		} else if (!strcmp(att->svg_name, "overlay")) {
			strcpy(att->impl_type, "SVG_Overlay");
		} else if (!strcmp(att->svg_name, "transformBehavior")) {
			strcpy(att->impl_type, "SVG_TransformBehavior");
		} else if (!strcmp(att->svg_name, "rotate")) {
			strcpy(att->impl_type, "SVG_Rotate");
		} else if (!strcmp(att->svg_name, "font-variant")) {
			strcpy(att->impl_type, "SVG_FontVariant");
		} else if (!strcmp(att->svg_name, "lsr:enabled")) {
			strcpy(att->impl_type, "SVG_Boolean");
		} else if (!strcmp(att->svg_name, "choice")) {
			strcpy(att->impl_type, "LASeR_Choice");
		} else if (!strcmp(att->svg_name, "size") || 
				   !strcmp(att->svg_name, "delta")) {
			strcpy(att->impl_type, "LASeR_Size");
		} else if (!strcmp(att->svg_name, "spreadMethod")) {
			strcpy(att->impl_type, "SVG_SpreadMethod");
		} else if (!strcmp(att->svg_name, "gradientTransform")) {
			strcpy(att->impl_type, "SVG_Matrix");
		} else if (!strcmp(att->svg_name, "editable")) {
			strcpy(att->impl_type, "SVG_Boolean");
		} else {
			/* For all other attributes, we use String as default type */
			strcpy(att->impl_type, "SVG_String");
			fprintf(stdout, "Warning: using type SVG_String for attribute %s.\n", att->svg_name);
		}
	} else { /* for some attributes, the type given in the RNG needs to be overriden */
		if (!strcmp(att->svg_name, "color")) {
			strcpy(att->impl_type, "SVG_Paint");
		} else if (!strcmp(att->svg_name, "viewport-fill")) {
			strcpy(att->impl_type, "SVG_Paint");
		} else if (!strcmp(att->svg_name, "syncTolerance")) {
			strcpy(att->impl_type, "SMIL_SyncTolerance");
		} else if (!strcmp(att->svg_name, "syncToleranceDefault")) {
			strcpy(att->impl_type, "SMIL_SyncTolerance");
		} else if (!strcmp(att->svg_name, "transform")) {
			strcpy(att->impl_type, "SVG_Matrix");
		} else if (!strcmp(att->svg_name, "event") || !strcmp(att->svg_name, "ev:event")) {
			strcpy(att->impl_type, "XMLEV_Event");
		} else if (!strcmp(att->svg_name, "gradientTransform")) {
			strcpy(att->impl_type, "SVG_Matrix");
		} else if (strstr(att->svg_type, "datatype")) {
			char *tmp;
			sprintf(att->impl_type, "SVG_%s", att->svg_type);
			tmp = att->impl_type;
			while ( (tmp = strstr(tmp, "-")) ) { *tmp='_'; tmp++; }
			tmp = att->impl_type;
			while ( (tmp = strstr(tmp, ".")) ) { *tmp='_'; tmp++; }
			tmp = att->impl_type;;
			if ( (tmp = strstr(tmp, "datatype")) ) {
				tmp--;
				*tmp = 0;
			} 
		} 
	}
}

void getAttributeType(xmlDocPtr doc, xmlXPathContextPtr xpathCtx, xmlNodePtr attributeNode, SVGAttribute *a) 
{

	xmlNodeSetPtr refNodes;
	xpathCtx->node = attributeNode;
	refNodes = findNodes(xpathCtx, ".//rng:ref");
	if (refNodes->nodeNr == 0) {
		//a->svg_type = xmlStrdup("0_ref_type");
	} else if (refNodes->nodeNr == 1) {
		xmlNodePtr ref = refNodes->nodeTab[0];
		a->svg_type = xmlStrdup(xmlGetProp(ref, "name"));
	} else {
		//a->svg_type = xmlStrdup("N_ref_type");
	}
}

void getRealAttributes(xmlDocPtr doc, xmlXPathContextPtr xpathCtx, xmlNodePtr newCtxNode, GF_List *attributes)
{
	xmlNodeSetPtr attributeNodes;
	int k;
	u32 j;

	xpathCtx->node = newCtxNode;
	attributeNodes = findNodes(xpathCtx, ".//rng:attribute");
	for (k = 0; k < attributeNodes->nodeNr; k++)	{
		Bool already_exists = 0;
		xmlNodePtr attributeNode = attributeNodes->nodeTab[k];
		if (attributeNode->type == XML_ELEMENT_NODE) {
			SVGAttribute *a = NewSVGAttribute();
			a->svg_name = xmlGetProp(attributeNode, "name");
			a->optional = xmlStrEqual(attributeNode->parent->name, "optional");
			svgNameToImplementationName(a->svg_name, a->implementation_name);
			getAttributeType(doc, xpathCtx, attributeNode, a);
			setAttributeType(a);
			for (j=0;j<gf_list_count(attributes); j++) {
				SVGAttribute *ta = gf_list_get(attributes, j);
				if (xmlStrEqual(ta->svg_name, a->svg_name)) {
					already_exists = 1;
					break;
				}
			}
			if (already_exists) {
				deleteSVGAttribute(&a);
			} else {
				//fprintf(stdout, "Adding attribute %s to element %s\n",a->svg_name, e->svg_name);
				gf_list_add(attributes, a);
			}
		}
	}
}

SVGAttrGrp *getOneGlobalAttrGrp(xmlDocPtr doc, xmlXPathContextPtr xpathCtx, xmlChar *name) 
{
	SVGAttrGrp *attgrp = NULL;
	xmlNodeSetPtr attrGrpDefNodes;
	xmlChar *expr;
	u32 j;
	int i, l;

	/* attributes group already resolved */
	for (j = 0; j < gf_list_count(globalAttrGrp); j++) {
		SVGAttrGrp *attgrp = gf_list_get(globalAttrGrp, j);
		if (!strcmp(attgrp->name, name)) {
			return attgrp;
		}
	}

	/* new attributes group */
	expr = xmlStrdup("//rng:define[@name=\"");
	expr = xmlStrcat(expr, name);
	expr = xmlStrcat(expr, "\" and not(rng:empty) and not(rng:notAllowed)]");
	attrGrpDefNodes = findNodes(xpathCtx, expr);
	if (!attrGrpDefNodes->nodeNr) {
		fprintf(stdout, "Warning: found 0 non-empty or allowed definition for the Group of Attributes: %s\n", name);
		return NULL;
	}
	attgrp = NewAttrGrp();				
	attgrp->name = strdup(name);
	svgNameToImplementationName(attgrp->name, attgrp->imp_name);
	gf_list_add(globalAttrGrp, attgrp);

	for (i = 0; i < attrGrpDefNodes->nodeNr; i++) {
		xmlNodePtr attrGrp = attrGrpDefNodes->nodeTab[i];
		getRealAttributes(doc, xpathCtx, attrGrp, attgrp->attrs);

		{
			xmlNodeSetPtr refNodes;
			xpathCtx->node = attrGrp;
			refNodes = findNodes(xpathCtx, ".//rng:ref");
			for (l = 0; l < refNodes->nodeNr; l++) {
				xmlNodePtr ref = refNodes->nodeTab[l];
				xmlChar *rname = xmlGetProp(ref, "name");
				if (xmlStrstr(rname, ".attr")) {
					SVGAttrGrp *g2 = getOneGlobalAttrGrp(doc, xpathCtx, rname);
					if (g2) {
						gf_list_add(attgrp->attrgrps, g2);
					}
				}
			}
		}
	}
	return attgrp;
}

void getAllGlobalAttrGrp(xmlDocPtr doc, xmlXPathContextPtr xpathCtx) 
{
	xmlNodeSetPtr elementNodes = findNodes(xpathCtx, "//rng:define");
	int k;
	for (k = 0; k < elementNodes->nodeNr; k++)	{
		xmlNodePtr elementNode = elementNodes->nodeTab[k];
		if (elementNode->type == XML_ELEMENT_NODE) {
			xmlChar *name = NULL;
			name = xmlGetProp(elementNode, "name");
			if (xmlStrstr(name, ".attr")) {
				getOneGlobalAttrGrp(doc, xpathCtx, name);
			}
		}
	}
}

GF_List *getElements(xmlDocPtr doc, xmlXPathContextPtr xpathCtx)
{
	xmlChar *expr;
	GF_List *elements = gf_list_new();
	xmlNodeSetPtr ATNodes;
	xmlNodeSetPtr refNodes, elementNodes = findNodes(xpathCtx, "//rng:element");
	int k, j;
	u32 i;

	for (k = 0; k < elementNodes->nodeNr; k++)	{
		xmlNodePtr elementNode = elementNodes->nodeTab[k];
		if (elementNode->type == XML_ELEMENT_NODE) {
			SVGElement *e = NewSVGElement();
			e->svg_name = xmlStrdup(xmlGetProp(elementNode, "name"));
			//fprintf(stdout, "\n\tElement %s\n", e->svg_name);
			
			svgNameToImplementationName(e->svg_name, e->implementation_name);
		
			/* getting the <define name="element.AT"/> */
			expr = xmlStrdup("//rng:define[@name=\"");
			if (!xmlStrcmp(e->svg_name, "polygon") || !xmlStrcmp(e->svg_name, "polyline")) {
				expr = xmlStrcat(expr, "polyCommon");
			} else
				expr = xmlStrcat(expr, e->svg_name);
			expr = xmlStrcat(expr, ".AT\"]");
			ATNodes = findNodes(xpathCtx, expr);
			if (ATNodes->nodeNr) {
				/* dealing with attributes defined directly here */
				getRealAttributes(doc, xpathCtx, ATNodes->nodeTab[0], e->attributes);

				/* dealing with attributes defined in groups of attributes */
				xpathCtx->node = ATNodes->nodeTab[0];
				refNodes = findNodes(xpathCtx, ".//rng:ref");
				for (j = 0; j <refNodes->nodeNr; j++) {
					xmlNodePtr refNode = refNodes->nodeTab[j];
					char *name = xmlGetProp(refNode, "name");
					for (i = 0; i < gf_list_count(globalAttrGrp); i++) {						
						SVGAttrGrp *a = gf_list_get(globalAttrGrp, i);
						if (!strcmp(a->name, name)) {
							if (isGenericAttributesGroup(a->name)) {
								setGenericAttributesFlags(a->name, e);
								flattenAttributeGroup(*a, e, 1);
							} else {
								flattenAttributeGroup(*a, e, 0);
							}
							break;
						}
					}
				}
			}

			/* checking if this element is already present in the list of possible elements 
			   and if not, adding it */
			{
				Bool found = 0;
				for (i=0;i<gf_list_count(elements); i++) {
					SVGElement *etmp = gf_list_get(elements, i);
					if (!strcmp(etmp->svg_name, e->svg_name)) {
						found = 1;
						break;
					}
				}
				if (!found) gf_list_add(elements, e);
			}
		}
	}
	return elements;
}

#undef LOCAL_SVG_NODES

/*type: 0: header, 1: source*/
static FILE *BeginFile(u32 type)
{
	FILE *f;

	char sPath[GF_MAX_PATH];

	if (!type) {
#ifdef LOCAL_SVG_NODES
		sprintf(sPath, "nodes_svg.h", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
#else
		sprintf(sPath, "..%c..%c..%c..%cinclude%cgpac%cnodes_svg.h", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
#endif
	} else if (type==1) {
#ifdef LOCAL_SVG_NODES
		sprintf(sPath, "svg_nodes.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
#else
		sprintf(sPath, "..%c..%c..%c..%csrc%cscenegraph%csvg_nodes.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
#endif
	} else {
#ifdef LOCAL_SVG_NODES
		sprintf(sPath, "lsr_tables.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
#else
		sprintf(sPath, "..%c..%c..%c..%csrc%claser%clsr_tables.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
#endif
	}
	
	f = fopen(sPath, "wt");
	fprintf(f, "%s\n", COPYRIGHT);

	{
		time_t rawtime;
		time(&rawtime);
		fprintf(f, "\n/*\n\tDO NOT MOFIFY - File generated on GMT %s\n\tBY SVGGen for GPAC Version %s\n*/\n\n", asctime(gmtime(&rawtime)), GPAC_VERSION);
	}

	if (!type) {
		fprintf(f, "#ifndef _GF_SVG_NODES_H\n");
		fprintf(f, "#define _GF_SVG_NODES_H\n\n");
		fprintf(f, "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n");
	}
	return f;
}

static void EndFile(FILE *f, u32 type)
{
	if (!type) {
		fprintf(f, "#ifdef __cplusplus\n}\n#endif\n\n");
		fprintf(f, "\n\n#endif\t\t/*_GF_SVG_NODES_H*/\n\n");
	} else {
		fprintf(f, "\n");
	}
	fclose(f);
}

void generateAttributes(FILE *output, GF_List *attributes, Bool inDefine) 
{
	u32 i;
	for (i = 0; i<gf_list_count(attributes); i++) {
		SVGAttribute *att = (SVGAttribute *)gf_list_get(attributes, i);
		if (inDefine) 
			if (i == gf_list_count(attributes) -1) 
				fprintf(output, "\t%s %s;\n", att->impl_type, att->implementation_name);
			else 
				fprintf(output, "\t%s %s; \\\n", att->impl_type, att->implementation_name);
		else 
			fprintf(output, "\t%s %s;\n", att->impl_type, att->implementation_name);
	}
}

void generateNode(FILE *output, SVGElement* svg_elt) 
{
	fprintf(output, "typedef struct _tagSVG%sElement\n{\n", svg_elt->implementation_name);
	fprintf(output, "\tBASE_SVG_ELEMENT\n");

	if (svg_elt->has_transform) {
		fprintf(output, "\tBool is_ref_transform;\n");
		fprintf(output, "\tSVG_Matrix transform;\n");
		fprintf(output, "\tSVG_Matrix *motionTransform;\n");
	}

	if (svg_elt->has_xy) {
		fprintf(output, "\tSVG_Point xy;\n");
	}

	if (!strcmp(svg_elt->implementation_name, "conditional")) {
		fprintf(output, "\tSVGCommandBuffer updates;\n");
	} 

	generateAttributes(output, svg_elt->attributes, 0);

	/*special case for handler node*/
	if (!strcmp(svg_elt->implementation_name, "handler")) {
		fprintf(output, "\tvoid (*handle_event)(struct _tagSVGhandlerElement *hdl, GF_DOM_Event *event);\n");
	}
	fprintf(output, "} SVG%sElement;\n\n\n", svg_elt->implementation_name);
}

void generateAttributeInfo(FILE *output, char * elt_imp_name, SVGAttribute *att, u32 i)
{
	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"%s\";\n", att->svg_name);
	fprintf(output, "\t\t\tinfo->fieldType = %s_datatype;\n", att->impl_type);
	fprintf(output, "\t\t\tinfo->far_ptr = & ((SVG%sElement *)node)->%s;\n", elt_imp_name, att->implementation_name);
	fprintf(output, "\t\t\treturn GF_OK;\n");
}

void generateAttributeInfo2(FILE *output, char *pointer, char *name, char *type, u32 i)
{
	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"%s\";\n", name);
	fprintf(output, "\t\t\tinfo->fieldType = %s_datatype;\n", type);
	fprintf(output, "\t\t\tinfo->far_ptr = &%s;\n", pointer);
	fprintf(output, "\t\t\treturn GF_OK;\n");
}

u32 generateAttributesGroupInfo(FILE *output, char * elt_imp_name, SVGAttrGrp *attgrp, u32 i)
{
	u32 att_index = i;
	u32 k;
	for (k=0; k<gf_list_count(attgrp->attrgrps); k++) {
		SVGAttrGrp *ag = gf_list_get(attgrp->attrgrps, k);
		att_index = generateAttributesGroupInfo(output, elt_imp_name, ag, att_index);
	}
	for (k=0; k<gf_list_count(attgrp->attrs); k++) {
		SVGAttribute *at = gf_list_get(attgrp->attrs, k);
		generateAttributeInfo(output, elt_imp_name, at, att_index++);
	}
	return att_index;
}

u32 generateCoreInfo(FILE *output, SVGElement *elt, u32 start)
{
	u32 i = start;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"id\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_ID_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &node->sgprivate->NodeName;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"xml:id\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_ID_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &node->sgprivate->NodeName;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"class\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_String_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVGElement *)node)->core->_class;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"xml:lang\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_LanguageID_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVGElement *)node)->core->lang;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"xml:base\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_String_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVGElement *)node)->core->base;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"xml:space\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = XML_Space_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVGElement *)node)->core->space;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"externalResourcesRequired\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_Boolean_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVGElement *)node)->core->eRR;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	return i;
}

u32 generateTransformInfo(FILE *output, SVGElement *elt, u32 start)
{
	u32 i = start;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"transform\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_Matrix_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVGTransformableElement *)node)->transform;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;
	return i;
}

u32 generateMotionTransformInfo(FILE *output, SVGElement *elt, u32 start)
{
	u32 i = start;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"motionTransform\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_Matrix_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = ((SVGTransformableElement *)node)->motionTransform;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;
	return i;
}

u32 generateXYInfo(FILE *output, SVGElement *elt, u32 start)
{
	u32 i = start;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"x\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_Coordinate_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVGTransformableElement *)node)->xy.x;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"y\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_Coordinate_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVGTransformableElement *)node)->xy.y;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;
	return i;
}

u32 generateGenericInfo(FILE *output, SVGElement *elt, u32 index, char *pointer_root, u32 start)
{
	u32 i = start;
	int k;
	for (k=0; k < generic_attributes[index].array_length; k++) {
		char *att_name = generic_attributes[index].array[k];
		SVGAttribute *a = findAttribute(elt, att_name);
		if (a) {
			char pointer[500];
			if (strstr(att_name, "xlink:")) {
				sprintf(pointer, "%s%s", pointer_root, att_name+6);
			} else if (strstr(att_name, "xml:")) {
				sprintf(pointer, "%s%s", pointer_root, att_name+4);
			} else {
				char imp_name[50];
				svgNameToImplementationName(att_name, imp_name);
				sprintf(pointer, "%s%s", pointer_root, imp_name);
			}
			generateAttributeInfo2(output, pointer, a->svg_name, a->impl_type, i);
			i++;
		}
	}
	return i;
}

void generateNodeImpl(FILE *output, SVGElement* svg_elt) 
{
	u32 i;	

	/* Constructor */
	fprintf(output, "void *gf_svg_new_%s()\n{\n\tSVG%sElement *p;\n", svg_elt->implementation_name,svg_elt->implementation_name);
	fprintf(output, "\tGF_SAFEALLOC(p, sizeof(SVG%sElement));\n\tif (!p) return NULL;\n\tgf_node_setup((GF_Node *)p, TAG_SVG_%s);\n\tgf_sg_parent_setup((GF_Node *) p);\n",svg_elt->implementation_name,svg_elt->implementation_name);
	fprintf(output, "#ifdef GF_NODE_USE_POINTERS\n");
	fprintf(output, "\t((GF_Node *p)->sgprivate->name = \"%s\";\n", svg_elt->implementation_name);
	fprintf(output, "\t((GF_Node *p)->sgprivate->node_del = SVG_%s_Del;\n", svg_elt->implementation_name);
	fprintf(output, "\t((GF_Node *p)->sgprivate->get_field = SVG_%s_get_attribute;\n", svg_elt->implementation_name);
	fprintf(output, "#endif\n");

	fprintf(output, "\tgf_svg_init_core((SVGElement *)p);\n");		
	if (svg_elt->has_properties || 
		svg_elt->has_media_properties || 
		svg_elt->has_opacity_properties) {
		fprintf(output, "\tgf_svg_init_properties((SVGElement *)p);\n");		
	} 
	if (svg_elt->has_focus) {
		fprintf(output, "\tgf_svg_init_focus((SVGElement *)p);\n");		
	} 
	if (svg_elt->has_xlink) {
		fprintf(output, "\tgf_svg_init_xlink((SVGElement *)p);\n");		
	} 
	if (svg_elt->has_timing) {
		fprintf(output, "\tgf_svg_init_timing((SVGElement *)p);\n");		
	} 
	if (svg_elt->has_sync) {
		fprintf(output, "\tgf_svg_init_sync((SVGElement *)p);\n");		
	}
	if (svg_elt->has_animation){
		fprintf(output, "\tgf_svg_init_anim((SVGElement *)p);\n");		
	} 
	if (svg_elt->has_conditional) {
		fprintf(output, "\tgf_svg_init_conditional((SVGElement *)p);\n");		
	} 

	if (svg_elt->has_transform) {
		fprintf(output, "\tgf_mx2d_init(p->transform);\n");
	} 

	if (!strcmp(svg_elt->implementation_name, "conditional")) {
		fprintf(output, "\tgf_svg_init_lsr_conditional(&p->updates);\n");
		fprintf(output, "\tgf_svg_init_timing((SVGElement *)p);\n");		

	} 

	for (i = 0; i < gf_list_count(svg_elt->attributes); i++) {
		SVGAttribute *att = gf_list_get(svg_elt->attributes, i);
		/* Initialization of complex types */
		if ( !strcmp("SVG_Points", att->impl_type) || 
			 !strcmp("SVG_Coordinates", att->impl_type) ||
			 !strcmp("SMIL_KeyPoints", att->impl_type)) {
			fprintf(output, "\tp->%s = gf_list_new();\n", att->implementation_name);
		} else if (!strcmp("SVG_PathData", att->impl_type) && !strcmp(svg_elt->svg_name, "animateMotion")) {
			fprintf(output, "\tp->path.commands = gf_list_new();\n");
			fprintf(output, "\tp->path.points = gf_list_new();\n");
		} else if (!strcmp("SVG_PathData", att->impl_type)) {
			fprintf(output, "\tp->d.commands = gf_list_new();\n");
			fprintf(output, "\tp->d.points = gf_list_new();\n");
		} else if (!strcmp(att->svg_name, "lsr:enabled")) {
			fprintf(output, "\tp->lsr_enabled = 1;\n");
		} 
	}
	/*some default values*/
	if (!strcmp(svg_elt->svg_name, "svg")) {
		fprintf(output, "\tp->width.type = SVG_NUMBER_PERCENTAGE;\n");
		fprintf(output, "\tp->width.value = INT2FIX(100);\n");
		fprintf(output, "\tp->height.type = SVG_NUMBER_PERCENTAGE;\n");
		fprintf(output, "\tp->height.value = INT2FIX(100);\n");
	}
	else if (!strcmp(svg_elt->svg_name, "solidColor")) {
		fprintf(output, "\tp->properties->solid_opacity.value = FIX_ONE;\n");
	}
	else if (!strcmp(svg_elt->svg_name, "stop")) {
		fprintf(output, "\tp->properties->stop_opacity.value = FIX_ONE;\n");
	}
	else if (!strcmp(svg_elt->svg_name, "linearGradient")) {
		fprintf(output, "\tp->x2.value = FIX_ONE;\n");
		fprintf(output, "\tgf_mx2d_init(p->gradientTransform);\n");
	}
	else if (!strcmp(svg_elt->svg_name, "radialGradient")) {
		fprintf(output, "\tp->cx.value = FIX_ONE/2;\n");
		fprintf(output, "\tp->cy.value = FIX_ONE/2;\n");
		fprintf(output, "\tp->r.value = FIX_ONE/2;\n");
		fprintf(output, "\tgf_mx2d_init(p->gradientTransform);\n");
		fprintf(output, "\tp->fx.value = FIX_ONE/2;\n");
		fprintf(output, "\tp->fy.value = FIX_ONE/2;\n");
	}
	else if (!strcmp(svg_elt->svg_name, "video") || !strcmp(svg_elt->svg_name, "audio") || !strcmp(svg_elt->svg_name, "animation")) {
		fprintf(output, "\tp->timing->dur.type = SMIL_DURATION_MEDIA;\n");
	}
	fprintf(output, "\treturn p;\n}\n\n");

	/* Destructor */
	fprintf(output, "static void gf_svg_%s_del(GF_Node *node)\n{\n", svg_elt->implementation_name);
	fprintf(output, "\tSVG%sElement *p = (SVG%sElement *)node;\n", svg_elt->implementation_name, svg_elt->implementation_name);

	fprintf(output, "\tgf_svg_reset_base_element((SVGElement *)p);\n");

	if (!strcmp(svg_elt->implementation_name, "conditional")) {
		fprintf(output, "\tgf_svg_reset_lsr_conditional(&p->updates);\n");
	} 
	else if (!strcmp(svg_elt->implementation_name, "a")) {
		fprintf(output, "\tif (p->target) free(p->target);\n");
	} 

	for (i = 0; i < gf_list_count(svg_elt->attributes); i++) {
		SVGAttribute *att = gf_list_get(svg_elt->attributes, i);
		if (!strcmp("SMIL_KeyPoints", att->impl_type)) {
			fprintf(output, "\tgf_smil_delete_key_types(p->%s);\n", att->implementation_name);
		} else if (!strcmp("SVG_Coordinates", att->impl_type)) {
			fprintf(output, "\tgf_svg_delete_coordinates(p->%s);\n", att->implementation_name);
		} else if (!strcmp("SVG_Points", att->impl_type)) {
			fprintf(output, "\tgf_svg_delete_points(p->%s);\n", att->implementation_name);
		} else if (!strcmp("SVG_PathData", att->impl_type)) {
			if (!strcmp(svg_elt->svg_name, "animateMotion")) {
				fprintf(output, "\tgf_svg_reset_path(p->path);\n");
			} else {
				fprintf(output, "\tgf_svg_reset_path(p->d);\n");
			}
		} else if (!strcmp("SVG_IRI", att->impl_type)) {
			fprintf(output, "\tgf_svg_reset_iri(node->sgprivate->scenegraph, &p->%s);\n", att->implementation_name);
		} else if (!strcmp("SVG_FontFamily", att->impl_type)) {
			fprintf(output, "\tif (p->%s.value) free(p->%s.value);\n", att->implementation_name, att->implementation_name);
		} else if (!strcmp("SVG_String", att->impl_type) || !strcmp("SVG_ContentType", att->impl_type)) {
			fprintf(output, "\tfree(p->%s);\n", att->implementation_name);
		}
	}
	if (svg_elt->has_transform) {
		fprintf(output, "\tif (p->motionTransform) free(p->motionTransform);\n");
	} 

	fprintf(output, "\tgf_sg_parent_reset((GF_Node *) p);\n");
	fprintf(output, "\tgf_node_free((GF_Node *)p);\n");
	fprintf(output, "}\n\n");
	
	/* Attribute Access */
	fprintf(output, "static GF_Err gf_svg_%s_get_attribute(GF_Node *node, GF_FieldInfo *info)\n{\n", svg_elt->implementation_name);
	fprintf(output, "\tswitch (info->fieldIndex) {\n");
	svg_elt->nb_atts = 0;
	svg_elt->nb_atts = generateCoreInfo(output, svg_elt, svg_elt->nb_atts);

	if (svg_elt->has_media_properties) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 2, "((SVGElement *)node)->properties->", svg_elt->nb_atts);
	if (svg_elt->has_properties) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 1, "((SVGElement *)node)->properties->", svg_elt->nb_atts);
	if (svg_elt->has_opacity_properties) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 3, "((SVGElement *)node)->properties->", svg_elt->nb_atts);
	if (svg_elt->has_focus) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 4, "((SVGElement *)node)->focus->", svg_elt->nb_atts);
	if (svg_elt->has_xlink) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 5, "((SVGElement *)node)->xlink->", svg_elt->nb_atts);
	if (svg_elt->has_timing) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 6, "((SVGElement *)node)->timing->", svg_elt->nb_atts);
	if (svg_elt->has_sync) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 7, "((SVGElement *)node)->sync->", svg_elt->nb_atts);
	if (svg_elt->has_animation) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 8, "((SVGElement *)node)->anim->", svg_elt->nb_atts);
	if (svg_elt->has_conditional) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 9, "((SVGElement *)node)->conditional->", svg_elt->nb_atts);
	if (svg_elt->has_transform) {
		svg_elt->nb_atts = generateTransformInfo(output, svg_elt, svg_elt->nb_atts);
		svg_elt->nb_atts = generateMotionTransformInfo(output, svg_elt, svg_elt->nb_atts);
	}
	if (svg_elt->has_xy) 
		svg_elt->nb_atts = generateXYInfo(output, svg_elt, svg_elt->nb_atts);

	for (i = 0; i < gf_list_count(svg_elt->attributes); i++) {
		SVGAttribute *att = gf_list_get(svg_elt->attributes, i);
		generateAttributeInfo(output, svg_elt->implementation_name, att, svg_elt->nb_atts++);
	}
	fprintf(output, "\t\tdefault: return GF_BAD_PARAM;\n\t}\n}\n\n");

}

static FILE *BeginHtml()
{
	FILE *f;
	char sPath[GF_MAX_PATH];

	sprintf(sPath, "C:%cUsers%cCyril%ccontent%csvg%cregression%cregression_table.html", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
	f = fopen(sPath, "wt");

	fprintf(f, "<?xml version='1.0' encoding='UTF-8'?>\n");
	fprintf(f, "<html xmlns='http://www.w3.org/1999/xhtml'>\n");
	fprintf(f, "<head>\n");
	fprintf(f, "<title>Status of the SVG implementation in GPAC</title>\n");

	{
		time_t rawtime;
		time(&rawtime);
		fprintf(f, "<!--File generated on GMT %s bY SVGGen for GPAC Version %s-->\n\n", asctime(gmtime(&rawtime)), GPAC_VERSION);
	}
	fprintf(f, "<!--\n%s-->\n", COPYRIGHT);
	fprintf(f, "<link rel='stylesheet' type='text/css' href='gpac_table_style.css'/>\n");

	fprintf(f, "</head>\n");
	fprintf(f, "<body>\n");
    fprintf(f, "<h1 align='center'>Status of the SVG implementation in GPAC</h1>\n");
	return f;
}

static void EndHtml(FILE *f)
{
	fprintf(f, "</body>\n");
	fprintf(f, "</html>\n");
	fclose(f);
}

/* Generates an HTML table */
void generate_table(GF_List *elements)
{
	u32 i, j;
	u32 nbExamples = 0;
	FILE *f;
	f = BeginHtml();

	
	fprintf(f, "<h2>Legend</h2>\n");
	fprintf(f, "<table>\n");
	fprintf(f, "<thead>\n");
	fprintf(f, "<tr>\n");
	fprintf(f, "<th>Status</th>\n");
	fprintf(f, "<th>Color</th>\n");
	fprintf(f, "</tr>\n");
	fprintf(f, "</thead>\n");
	fprintf(f, "<tbody>\n");
	fprintf(f, "<tr>\n");
	fprintf(f, "<td>Not supported</td>\n");
	fprintf(f, "<td class='not-supported'></td>\n");
	fprintf(f, "</tr>\n");
	fprintf(f, "<tr>\n");
	fprintf(f, "<td>Partially supported</td>\n");
	fprintf(f, "<td class='partially-supported'></td>\n");
	fprintf(f, "</tr>\n");
	fprintf(f, "<tr>\n");
	fprintf(f, "<td>Fully supported</td>\n");
	fprintf(f, "<td class='fully-supported'></td>\n");
	fprintf(f, "</tr>\n");
	fprintf(f, "</tbody>\n");
	fprintf(f, "</table>\n");

	fprintf(f, "<h2>SVG Tiny 1.2 Elements</h2>\n");
	fprintf(f, "<table class='reference-list' align='center'>\n");
	fprintf(f, "<thead>\n");
	fprintf(f, "<tr>\n");
	fprintf(f, "<th>Element Name</th>\n");
	fprintf(f, "<th>Status</th>\n");
	fprintf(f, "<th>Observations</th>\n");
	fprintf(f, "<th>Example(s)</th>\n");
	fprintf(f, "<th>Bug(s)</th>\n");
	fprintf(f, "</tr>\n");
	fprintf(f, "</thead>\n");
	fprintf(f, "<tbody>\n");
	for (i = 0; i < gf_list_count(elements); i++) {
		SVGElement *elt = gf_list_get(elements, i);
		fprintf(f, "<tr class='element' id='element_%s'>\n", elt->implementation_name);
		fprintf(f, "<td class='element-name'><a href='#table_%s'>%s</a></td>\n", elt->svg_name, elt->svg_name);
		fprintf(f, "<td class='not-supported'></td>\n");
		fprintf(f, "<td class='element-observation'>&nbsp;</td>\n");
		fprintf(f, "<td class='element-example'>&nbsp;</td>\n");
		fprintf(f, "<td class='element-bug'>&nbsp;</td>\n");
		fprintf(f, "</tr>\n");		
	}
	fprintf(f, "</tbody>\n");
	fprintf(f, "</table>\n");

	for (i = 0; i < gf_list_count(elements); i++) {
		SVGElement *elt = gf_list_get(elements, i);
		fprintf(f, "<h2 id='table_%s'>%s</h2>\n", elt->implementation_name, elt->svg_name);
		fprintf(f, "<table>\n");
		fprintf(f, "<thead>\n");
		fprintf(f, "<tr>\n");
		fprintf(f, "<th>Attribute Name</th>\n");
		fprintf(f, "<th>Status</th>\n");
		fprintf(f, "<th>Observations</th>\n");
		fprintf(f, "<th>Example(s)</th>\n");
		fprintf(f, "<th>Bug(s)</th>\n");
		fprintf(f, "</tr>\n");
		fprintf(f, "</thead>\n");
		fprintf(f, "<tbody>\n");
		for (j = 0; j < gf_list_count(elt->attributes); j++) {
			SVGAttribute *att = gf_list_get(elt->attributes, j);
			if (!strcmp(att->svg_name, "textContent")) continue;
			fprintf(f, "<tr>\n");
			fprintf(f, "<td class='attribute-name'>%s</td>\n",att->svg_name);
			fprintf(f, "<td class='not-supported'></td>\n");
			fprintf(f, "<td class='attribute-observation'>&nbsp;</td>\n");
			fprintf(f, "<td class='attribute-example'>%d - &nbsp;</td>\n",++nbExamples);
			fprintf(f, "<td class='attribute-bug'>&nbsp;</td>\n");
			fprintf(f, "</tr>\n");
		}		
		fprintf(f, "</tbody>\n");
		fprintf(f, "</table>\n");
	}

	EndHtml(f);
	gf_list_del(elements);
}

void replaceIncludes(xmlDocPtr doc, xmlXPathContextPtr xpathCtx)
{
	int k;
	xmlNodeSetPtr nodes;
	xmlXPathObjectPtr xpathObj; 

    /* Get all the RNG elements */
    xpathObj = xmlXPathEvalExpression("//rng:include", xpathCtx);
    if(xpathObj == NULL || xpathObj->type != XPATH_NODESET) return;
	
	nodes = xpathObj->nodesetval;	
		
	for (k = 0; k < nodes->nodeNr; k++)	{
		xmlNodePtr node = nodes->nodeTab[k];
		if (node->type == XML_ELEMENT_NODE) {
			xmlChar *href;
			xmlDocPtr sub_doc;

			href = xmlGetNoNsProp(node, "href");
			sub_doc = xmlParseFile(href);
			xmlReplaceNode(nodes->nodeTab[k], xmlDocGetRootElement(sub_doc));
		}
	}
	xmlXPathFreeObject(xpathObj);
}

static char *laser_attribute_name_type_list[] = {
	"a.target", "accumulate", "additive", "attributeName", "audio-level", "bandwidth", "begin", "calcMode", "children", "choice", "clipBegin", "clipEnd", "color", "color-rendering", "cx", "cy", "d", "display", "display-align", "dur", "editable", "enabled", "end", "event", "externalResourcesRequired", "fill", "fill-opacity", "fill-rule", "focusable", "font-family", "font-size", "font-style", "font-variant", "font-weight", "gradientUnits", "handler", "height", "image-rendering", "keyPoints", "keySplines", "keyTimes", "line-increment", "listener.target", "mediaCharacterEncoding", "mediaContentEncodings", "mediaSize", "mediaTime", "nav-down", "nav-down-left", "nav-down-right", "nav-left", "nav-next", "nav-prev", "nav-right", "nav-up", "nav-up-left", "nav-up-right", "observer", "offset", "opacity", "overflow", "overlay", "path", "pathLength", "pointer-events", "points", "preserveAspectRatio", "r", "repeatCount", "repeatDur", "requiredExtensions", "requiredFeatures", "requiredFormats", "restart", "rotate", "rotation", "rx", "ry", "scale", "shape-rendering", "size", "solid-color", "solid-opacity", "stop-color", "stop-opacity", "stroke", "stroke-dasharray", "stroke-dashoffset", "stroke-linecap", "stroke-linejoin", "stroke-miterlimit", "stroke-opacity", "stroke-width", "svg.height", "svg.width", "syncBehavior", "syncBehaviorDefault", "syncReference", "syncTolerance", "syncToleranceDefault", "systemLanguage", "text-anchor", "text-decoration", "text-rendering", "textContent", "transform", "transformBehavior", "translation", "vector-effect", "viewBox", "viewport-fill", "viewport-fill-opacity", "visibility", "width", "x", "x1", "x2", "xlink:actuate", "xlink:arcrole", "xlink:href", "xlink:role", "xlink:show", "xlink:title", "xlink:type", "xml:base", "xml:lang", "y", "y1", "y2", "zoomAndPan", NULL
};


static s32 get_lsr_att_name_type(const char *name)
{
	u32 i = 0;
	while (laser_attribute_name_type_list[i]) {
		if (!strcmp(name, laser_attribute_name_type_list[i])) return i;
		i++;
	}
	return -1;
}

void generateGenericAttrib(FILE *output, SVGElement *elt, u32 index)
{
	int k;
	for (k=0; k < generic_attributes[index].array_length; k++) {
		char *att_name = generic_attributes[index].array[k];
		SVGAttribute *a = findAttribute(elt, att_name);
		if (a) {
			s32 type = get_lsr_att_name_type(att_name);
			/*SMIL anim fill not updatable*/
			if ((index==6) && !strcmp(att_name, "fill")) {
				type = -1;
			}
			fprintf(output, ", %d", type);
		}
	}
}

static void generate_laser_tables(GF_List *svg_elements)
{
	FILE *output;
	u32 i;
	u32 special_cases;

	output = BeginFile(2);
	fprintf(output, "\n#include <gpac/nodes_svg.h>\n\n");

	for (i=0; i<gf_list_count(svg_elements); i++) {
		u32 j, fcount;
		SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);

		fcount = gf_list_count(elt->attributes);

		fprintf(output, "static const s32 %s_field_to_attrib_type[] = {\n", elt->implementation_name);

		/*core info: id, xml:id, class, xml:lang, xml:base, xml:space, externalResourcesRequired*/
		fprintf(output, "-1, -1, -1, 125, 124, -1, 24");
		if (elt->has_media_properties) generateGenericAttrib(output, elt, 2);
		if (elt->has_properties) generateGenericAttrib(output, elt, 1);
		if (elt->has_opacity_properties) generateGenericAttrib(output, elt, 3);
		if (elt->has_focus) generateGenericAttrib(output, elt, 4); 
		if (elt->has_xlink) generateGenericAttrib(output, elt, 5);
		if (elt->has_timing) generateGenericAttrib(output, elt, 6);
		if (elt->has_sync) generateGenericAttrib(output, elt, 7);
		if (elt->has_animation) generateGenericAttrib(output, elt, 8);
		if (elt->has_conditional) generateGenericAttrib(output, elt, 9);
		/*WATCHOUT - HARDCODED VALUES*/
		if (elt->has_transform) fprintf(output, ", 105");
		if (elt->has_xy) fprintf(output, ", 116, 129");


		/*svg.width and svg.height escapes*/
		special_cases = 0;
		if (!strcmp(elt->svg_name, "svg")) special_cases = 1;
		else if (!strcmp(elt->svg_name, "a")) special_cases = 2;

		for (j=0; j<fcount; j++) {
			SVGAttribute *att = gf_list_get(elt->attributes, j);
			s32 type = get_lsr_att_name_type(att->svg_name);
			if (special_cases==1) {
				if (!strcmp(att->svg_name, "width")) 
					type = 95;
				else if (!strcmp(att->svg_name, "height")) 
					type = 94;
			}
			if ((special_cases==2) && !strcmp(att->svg_name, "target")) 
				type = 0;
			fprintf(output, ", %d", type);
		}
		fprintf(output, "\n};\n\n");

	}
	fprintf(output, "s32 gf_lsr_field_to_attrib_type(GF_Node *n, u32 fieldIndex)\n{\n\tif(!n) return -2;\n\tswitch (gf_node_get_tag(n)) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		u32 fcount;
		SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\tcase TAG_SVG_%s:\n", elt->implementation_name);
		fcount = gf_list_count(elt->attributes);
		fprintf(output, "\t\treturn %s_field_to_attrib_type[fieldIndex];\n", elt->implementation_name);
	}
	fprintf(output, "\tdefault:\n\t\treturn -2;\n\t}\n}\n\n");
}

int main(int argc, char **argv)
{
	FILE *output;
	xmlDocPtr doc = NULL;
	xmlXPathContextPtr xpathCtx = NULL; 
	GF_List *svg_elements = NULL;
	u32 i;

    xmlInitParser();
	LIBXML_TEST_VERSION

	doc = xmlParseFile(argv[1]);
    if (!doc) {
        printf("error: could not parse file %s\n", argv[1]);
		return -1;
    }

	xpathCtx = xmlXPathNewContext(doc);
    if(xpathCtx == NULL) {
        fprintf(stderr,"Error: unable to create new XPath context\n");
        xmlFreeDoc(doc); 
        return(-1);
    }
	xmlXPathRegisterNs(xpathCtx, RNG_PREFIX, RNG_NS);
	xmlXPathRegisterNs(xpathCtx, RNGA_PREFIX, RNGA_NS);
	xmlXPathRegisterNs(xpathCtx, SVGA_PREFIX, SVGA_NS);

	replaceIncludes(doc, xpathCtx);
	xmlSaveFile("completerng_props.xml", doc);
	
	globalAttrGrp = gf_list_new();
	getAllGlobalAttrGrp(doc, xpathCtx);

	svg_elements = getElements(doc, xpathCtx);
	svg_elements = sortElements(svg_elements);

	if (argv[2] && !strcmp(argv[2], "-html")) {
		generate_table(svg_elements);
	} else {
		output = BeginFile(0);
		fprintf(output, "#include <gpac/scenegraph_svg.h>\n\n\n");
		fprintf(output, "/* Definition of SVG element internal tags */\n");
		fprintf(output, "/* TAG names are made of \"TAG_SVG\" + SVG element name (with - replaced by _) */\n");
		/* write all tags */
		fprintf(output, "enum {\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			if (i == 0) 
				fprintf(output, "\tTAG_SVG_%s = GF_NODE_RANGE_FIRST_SVG", elt->implementation_name);
			else 
				fprintf(output, ",\n\tTAG_SVG_%s", elt->implementation_name);
		}
		fprintf(output, ",\n\t/*undefined elements (when parsing) use this tag*/\n\tTAG_SVG_UndefinedElement\n};\n\n");

#if 0
		fprintf(output, "/******************************************\n");
 		fprintf(output, "*   SVG Attributes Groups definitions     *\n");
 		fprintf(output, "*******************************************\n");
		for (i=0; i<gf_list_count(globalAttrGrp); i++) {
			SVGAttrGrp *attgrp = gf_list_get(globalAttrGrp, i);
			fprintf(output, "/* %d attributes *\n", attgrp->nb_attrs);
			fprintf(output, "#define %s \\\n", strupr(attgrp->imp_name));
			for (j = 0; j<gf_list_count(attgrp->attrgrps); j++) {
				SVGAttrGrp *a = gf_list_get(attgrp->attrgrps, j);
				fprintf(output, "\t%s ", strupr(a->imp_name));
				if (j != gf_list_count(attgrp->attrgrps) - 1) 
					fprintf(output, "\\\n");
				else if (gf_list_count(attgrp->attrs))
					fprintf(output, "\\\n");
				else 
					fprintf(output, "\n");
			}
			attgrp->attrs = sortAttr(attgrp->attrs);
			generateAttributes(output, attgrp->attrs, 1);
			fprintf(output, "\n");
		}
#endif
		fprintf(output, "/******************************************\n");
 		fprintf(output, "*   SVG Elements structure definitions    *\n");
 		fprintf(output, "*******************************************/\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			generateNode(output, elt);
		}
		fprintf(output, "/******************************************\n");
 		fprintf(output, "*  End SVG Elements structure definitions *\n");
 		fprintf(output, "*******************************************/\n");
		EndFile(output, 0);

		output = BeginFile(1);
		fprintf(output, "#include <gpac/nodes_svg.h>\n\n");
		fprintf(output, "#ifndef GPAC_DISABLE_SVG\n\n");
		fprintf(output, "#include <gpac/internal/scenegraph_dev.h>\n\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			generateNodeImpl(output, elt);
		}

		/* SVGElement *gf_svg_create_node(u32 ElementTag)*/
		fprintf(output, "SVGElement *gf_svg_create_node(u32 ElementTag)\n");
		fprintf(output, "{\n");
		fprintf(output, "\tswitch (ElementTag) {\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			fprintf(output, "\t\tcase TAG_SVG_%s: return gf_svg_new_%s();\n",elt->implementation_name,elt->implementation_name);
		}
		fprintf(output, "\t\tdefault: return NULL;\n\t}\n}\n\n");
		
		/* void gf_svg_element_del(SVGElement *elt) */
		fprintf(output, "void gf_svg_element_del(SVGElement *elt)\n{\n");
		fprintf(output, "\tGF_Node *node = (GF_Node *)elt;\n");
		fprintf(output, "\tswitch (node->sgprivate->tag) {\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			fprintf(output, "\t\tcase TAG_SVG_%s: gf_svg_%s_del(node); return;\n", elt->implementation_name, elt->implementation_name);
		}
		fprintf(output, "\t\tdefault: return;\n\t}\n}\n\n");

		/* u32 gf_svg_get_attribute_count(SVGElement *elt) */
		fprintf(output, "u32 gf_svg_get_attribute_count(GF_Node *node)\n{\n");
		fprintf(output, "\tswitch (node->sgprivate->tag) {\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			fprintf(output, "\t\tcase TAG_SVG_%s: return %i;\n", elt->implementation_name, elt->nb_atts);
		}
		fprintf(output, "\t\tdefault: return 0;\n\t}\n}\n\n");
		
		/* GF_Err gf_svg_get_attribute_info(GF_Node *node, GF_FieldInfo *info) */
		fprintf(output, "GF_Err gf_svg_get_attribute_info(GF_Node *node, GF_FieldInfo *info)\n{\n");
		fprintf(output, "\tswitch (node->sgprivate->tag) {\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			fprintf(output, "\t\tcase TAG_SVG_%s: return gf_svg_%s_get_attribute(node, info);\n", elt->implementation_name, elt->implementation_name);
		}
		fprintf(output, "\t\tdefault: return GF_BAD_PARAM;\n\t}\n}\n\n");

		/* u32 gf_node_svg_type_by_class_name(const char *element_name) */
		fprintf(output, "u32 gf_node_svg_type_by_class_name(const char *element_name)\n{\n\tif (!element_name) return TAG_UndefinedNode;\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			fprintf(output, "\tif (!stricmp(element_name, \"%s\")) return TAG_SVG_%s;\n", elt->svg_name, elt->implementation_name);
		}
		fprintf(output, "\treturn TAG_UndefinedNode;\n}\n\n");


		/* const char *gf_svg_get_element_name(u32 tag) */
		fprintf(output, "const char *gf_svg_get_element_name(u32 tag)\n{\n\tswitch(tag) {\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			fprintf(output, "\tcase TAG_SVG_%s: return \"%s\";\n", elt->implementation_name, elt->svg_name);
		}
		fprintf(output, "\tdefault: return \"UndefinedNode\";\n\t}\n}\n\n");

		fprintf(output, "#endif /*GPAC_DISABLE_SVG*/\n\n");
		EndFile(output, 1); 

		generate_laser_tables(svg_elements);
	}

	xmlXPathFreeContext(xpathCtx); 
	//xmlFreeDoc(doc); 
	
	xmlCleanupParser();
	return 0;
}


