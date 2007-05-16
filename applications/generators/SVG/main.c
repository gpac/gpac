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

#include "svggen.h"

SVGGenAttribute *NewSVGGenAttribute()
{
	SVGGenAttribute *att;
	GF_SAFEALLOC(att, SVGGenAttribute)
	return att;
}

void deleteSVGGenAttribute(SVGGenAttribute **p)
{
	xmlFree((*p)->svg_name);
	xmlFree((*p)->svg_type);
	free(*p);
	*p = NULL;
}

SVGGenAttrGrp *NewSVGGenAttrGrp()
{
	SVGGenAttrGrp *tmp;
	GF_SAFEALLOC(tmp, SVGGenAttrGrp)
	tmp->attrs = gf_list_new();
	tmp->attrgrps = gf_list_new();
	return tmp;
}

SVGGenElement *NewSVGGenElement() 
{
	SVGGenElement *elt;
	GF_SAFEALLOC(elt, SVGGenElement);
	if (elt) {
		elt->attributes = gf_list_new();
		elt->generic_attributes = gf_list_new();
	}
	return elt;
}

void deleteSVGGenElement(SVGGenElement **p)
{
	u32 i;
	xmlFree((*p)->svg_name);
	for (i = 0; i < gf_list_count((*p)->attributes); i++) {
		SVGGenAttribute *a = gf_list_get((*p)->attributes, i);
		deleteSVGGenAttribute(&a);
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
		SVGGenElement *elt = gf_list_get(elements, i);
		for (j = 0; j < gf_list_count(sorted_elements); j++) {
			SVGGenElement *selt = gf_list_get(sorted_elements, j);
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
		SVGGenAttrGrp *grp = gf_list_get(attgrps, i);
		for (j = 0; j < gf_list_count(sorted_attgrps); j++) {
			SVGGenAttrGrp *sgrp = gf_list_get(sorted_attgrps, j);
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
		SVGGenAttribute *att = gf_list_get(atts, i);
		for (j = 0; j < gf_list_count(sorted_atts); j++) {
			SVGGenAttribute *satt = gf_list_get(sorted_atts, j);
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

static Bool setGenericAttributesFlags(char *name, SVGGenElement *e) 
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

static void flattenAttributeGroup(SVGGenAttrGrp attgrp, SVGGenElement *e, Bool all);

static void flattenAttributeGroups(GF_List *attrgrps, SVGGenElement *e, Bool all) 
{
	u32 i;
	for (i = 0; i < gf_list_count(attrgrps); i ++) {
		SVGGenAttrGrp *ag = gf_list_get(attrgrps, i);
		flattenAttributeGroup(*ag, e, all);
	} 
}

static void flattenAttributeGroup(SVGGenAttrGrp attgrp, SVGGenElement *e, Bool all) 
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

SVGGenAttribute *findAttribute(SVGGenElement *e, char *name) 
{
	u32 i;
	for (i = 0; i < gf_list_count(e->attributes); i++) {
		SVGGenAttribute *a = gf_list_get(e->attributes, i);
		if (!strcmp(a->svg_name, name)) return a;
	}
	for (i = 0; i < gf_list_count(e->generic_attributes); i++) {
		SVGGenAttribute *a = gf_list_get(e->generic_attributes, i);
		if (!strcmp(a->svg_name, name)) return a;
	}
	return NULL;
}

static u32 countAttributesAllInGroup(SVGGenAttrGrp *ag)
{
	u32 i, ret = 0;
	for (i = 0; i < gf_list_count(ag->attrgrps); i ++) {
		SVGGenAttrGrp *agtmp = gf_list_get(ag->attrgrps, i);
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

void setAttributeType(SVGGenAttribute *att) 
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
		} else if (!strcmp(att->svg_name, "spreadMethod")) {
			strcpy(att->impl_type, "SVG_SpreadMethod");
		} else if (!strcmp(att->svg_name, "gradientTransform")) {
			strcpy(att->impl_type, "SVG_Transform_Full");
		} else if (!strcmp(att->svg_name, "editable")) {
			strcpy(att->impl_type, "SVG_Boolean");
		} else if (!strcmp(att->svg_name, "choice")) {
			strcpy(att->impl_type, "LASeR_Choice");
		} else if (!strcmp(att->svg_name, "size") || 
				   !strcmp(att->svg_name, "delta")) {
			strcpy(att->impl_type, "LASeR_Size");
		} else if (!strcmp(att->svg_name, "syncReference")) {
			strcpy(att->impl_type, "XMLRI");
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
			strcpy(att->impl_type, "SVG_Transform");
		} else if (!strcmp(att->svg_name, "gradientTransform")) {
			strcpy(att->impl_type, "SVG_Transform");
		} else if (!strcmp(att->svg_name, "focusable")) {
			strcpy(att->impl_type, "SVG_Focusable");
		} else if (!strcmp(att->svg_name, "event") || !strcmp(att->svg_name, "ev:event")) {
			strcpy(att->impl_type, "XMLEV_Event");
		} else if (!strcmp(att->svg_type, "IRI.datatype")) {
			strcpy(att->impl_type, "XMLRI");
		} else if (!strcmp(att->svg_type, "IDREF.datatype")) {
			strcpy(att->impl_type, "XML_IDREF");
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

void getAttributeType(xmlDocPtr doc, xmlXPathContextPtr xpathCtx, 
					  xmlNodePtr attributeNode, SVGGenAttribute *a) 
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

void getRealAttributes(xmlDocPtr doc, xmlXPathContextPtr xpathCtx, xmlNodePtr newCtxNode, 
					   GF_List *attributes)
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
			SVGGenAttribute *a = NewSVGGenAttribute();
			a->svg_name = xmlGetProp(attributeNode, "name");
			a->optional = xmlStrEqual(attributeNode->parent->name, "optional");
			svgNameToImplementationName(a->svg_name, a->implementation_name);
			getAttributeType(doc, xpathCtx, attributeNode, a);
			setAttributeType(a);
			for (j=0;j<gf_list_count(attributes); j++) {
				SVGGenAttribute *ta = gf_list_get(attributes, j);
				if (xmlStrEqual(ta->svg_name, a->svg_name)) {
					already_exists = 1;
					break;
				}
			}
			if (already_exists) {
				deleteSVGGenAttribute(&a);
			} else {
				//fprintf(stdout, "Adding attribute %s to element %s\n",a->svg_name, e->svg_name);
				gf_list_add(attributes, a);
			}
		}
	}
}

SVGGenAttrGrp *getOneGlobalAttrGrp(xmlDocPtr doc, xmlXPathContextPtr xpathCtx, xmlChar *name) 
{
	SVGGenAttrGrp *attgrp = NULL;
	xmlNodeSetPtr attrGrpDefNodes;
	xmlChar *expr;
	u32 j;
	int i, l;

	/* attributes group already resolved */
	for (j = 0; j < gf_list_count(globalAttrGrp); j++) {
		SVGGenAttrGrp *attgrp = gf_list_get(globalAttrGrp, j);
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
	attgrp = NewSVGGenAttrGrp();				
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
					SVGGenAttrGrp *g2 = getOneGlobalAttrGrp(doc, xpathCtx, rname);
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
			SVGGenElement *e = NewSVGGenElement();
			e->svg_name = xmlStrdup(xmlGetProp(elementNode, "name"));
			//fprintf(stdout, "\n\tElement %s\n", e->svg_name);
			
			svgNameToImplementationName(e->svg_name, e->implementation_name);
		
			/* getting the <define name="element.AT"/> */
			expr = xmlStrdup("//rng:define[@name=\"");
			if (!xmlStrcmp(e->svg_name, "polygon") || !xmlStrcmp(e->svg_name, "polyline")) {
				expr = xmlStrcat(expr, "polyCommon");
			} else {
				expr = xmlStrcat(expr, e->svg_name);
			}
			expr = xmlStrcat(expr, ".AT\"]");
			ATNodes = findNodes(xpathCtx, expr);
			if (ATNodes->nodeNr) {

				/* dealing with attributes defined in groups of attributes */
				xpathCtx->node = ATNodes->nodeTab[0];
				refNodes = findNodes(xpathCtx, ".//rng:ref");
				for (j = 0; j <refNodes->nodeNr; j++) {
					xmlNodePtr refNode = refNodes->nodeTab[j];
					char *name = xmlGetProp(refNode, "name");
					for (i = 0; i < gf_list_count(globalAttrGrp); i++) {						
						SVGGenAttrGrp *a = gf_list_get(globalAttrGrp, i);
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

				/* dealing with attributes defined directly here */
				getRealAttributes(doc, xpathCtx, ATNodes->nodeTab[0], e->attributes);
			}

			/* checking if this element is already present in the list of possible elements 
			   and if not, adding it */
			{
				Bool found = 0;
				for (i=0;i<gf_list_count(elements); i++) {
					SVGGenElement *etmp = gf_list_get(elements, i);
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

/*type: 0: header, 1: source*/
FILE *BeginFile(u32 type)
{
	FILE *f;

	char sPath[GF_MAX_PATH];

	if (!type) {
#ifdef LOCAL_SVG_NODES
		sprintf(sPath, "nodes_svg.h", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
#else
		if (generation_mode == 1) 
			sprintf(sPath, "..%c..%c..%c..%cinclude%cgpac%cnodes_svg_sa.h", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		else if (generation_mode == 2) 
			sprintf(sPath, "..%c..%c..%c..%cinclude%cgpac%cnodes_svg_sani.h", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		else if (generation_mode == 3)
			sprintf(sPath, "..%c..%c..%c..%cinclude%cgpac%cnodes_svg_da.h", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		
#endif
	} else if (type==1) {
#ifdef LOCAL_SVG_NODES
		sprintf(sPath, "svg_nodes.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
#else
		if (generation_mode == 1) 
			sprintf(sPath, "..%c..%c..%c..%csrc%cscenegraph%csvg_nodes_sa.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		else if (generation_mode == 2) 
			sprintf(sPath, "..%c..%c..%c..%csrc%cscenegraph%csvg_nodes_sani.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		else if (generation_mode == 3)
			sprintf(sPath, "..%c..%c..%c..%csrc%cscenegraph%csvg_nodes_da.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
#endif
	} else {
#ifdef LOCAL_SVG_NODES
		sprintf(sPath, "lsr_tables.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
#else
		if (generation_mode == 1) 
			sprintf(sPath, "..%c..%c..%c..%csrc%claser%clsr_tables_sa.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		else if (generation_mode == 2) 
			sprintf(sPath, "..%c..%c..%c..%csrc%claser%clsr_tables_sani.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		else if (generation_mode == 3) 
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
		if (generation_mode == 1) {
			fprintf(f, "#ifndef _GF_SVG_SA_NODES_H\n");
			fprintf(f, "#define _GF_SVG_SA_NODES_H\n\n");
		} else if (generation_mode == 2) {
			fprintf(f, "#ifndef _GF_SVG_SANI_NODES_H\n");
			fprintf(f, "#define _GF_SVG_SANI_NODES_H\n\n");
		} else if (generation_mode == 3) {
			fprintf(f, "#ifndef _GF_SVG_NODES_H\n");
			fprintf(f, "#define _GF_SVG_NODES_H\n\n");
		}
		fprintf(f, "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n");
	}
	return f;
}

void EndFile(FILE *f, u32 type)
{
	if (!type) {
		fprintf(f, "#ifdef __cplusplus\n}\n#endif\n\n");
		if (generation_mode == 1) fprintf(f, "\n\n#endif\t\t/*_GF_SVG_SA_NODES_H*/\n\n");
		if (generation_mode == 2) fprintf(f, "\n\n#endif\t\t/*_GF_SVG_SANI_NODES_H*/\n\n");
		if (generation_mode == 3) fprintf(f, "\n\n#endif\t\t/*_GF_SVG_NODES_H*/\n\n");
	} else {
		fprintf(f, "\n");
	}
	fclose(f);
}

void generateAttributes(FILE *output, GF_List *attributes, Bool inDefine) 
{
	u32 i;
	for (i = 0; i<gf_list_count(attributes); i++) {
		SVGGenAttribute *att = (SVGGenAttribute *)gf_list_get(attributes, i);
		if (inDefine) 
			if (i == gf_list_count(attributes) -1) 
				fprintf(output, "\t%s %s;\n", att->impl_type, att->implementation_name);
			else 
				fprintf(output, "\t%s %s; \\\n", att->impl_type, att->implementation_name);
		else 
			fprintf(output, "\t%s %s;\n", att->impl_type, att->implementation_name);
	}
}

/*
u32 generateAttributesGroupInfo(FILE *output, char * elt_imp_name, SVGGenAttrGrp *attgrp, u32 i)
{
	u32 att_index = i;
	u32 k;
	for (k=0; k<gf_list_count(attgrp->attrgrps); k++) {
		SVGGenAttrGrp *ag = gf_list_get(attgrp->attrgrps, k);
		att_index = generateAttributesGroupInfo(output, elt_imp_name, ag, att_index);
	}
	for (k=0; k<gf_list_count(attgrp->attrs); k++) {
		SVGGenAttribute *at = gf_list_get(attgrp->attrs, k);
		generateAttributeInfo(output, elt_imp_name, at, att_index++);
	}
	return att_index;
}
*/

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

int main(int argc, char **argv)
{
	xmlDocPtr doc = NULL;
	xmlXPathContextPtr xpathCtx = NULL; 
	GF_List *svg_elements = NULL;

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
		if (generation_mode == 1) generateSVGCode_V1(svg_elements);
		if (generation_mode == 2) generateSVGCode_V2(svg_elements);
		if (generation_mode == 3) generateSVGCode_V3(svg_elements);
	}

	xmlXPathFreeContext(xpathCtx); 
	//xmlFreeDoc(doc); 
	
	xmlCleanupParser();
	return 0;
}


