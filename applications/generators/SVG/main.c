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

#define COPYRIGHT "/*\n *			GPAC - Multimedia Framework C SDK\n *\n *			Copyright (c) Cyril Concolato 2004-2005\n *					All rights reserved\n *\n *  This file is part of GPAC / SVG Scene Graph sub-project\n *\n *  GPAC is free software; you can redistribute it and/or modify\n *  it under the terms of the GNU Lesser General Public License as published by\n *  the Free Software Foundation; either version 2, or (at your option)\n *  any later version.\n *\n *  GPAC is distributed in the hope that it will be useful,\n *  but WITHOUT ANY WARRANTY; without even the implied warranty of\n *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n *  GNU Lesser General Public License for more details.	\n *\n *  You should have received a copy of the GNU Lesser General Public\n *  License along with this library; see the file COPYING.  If not, write to\n *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.\n *\n */\n"

static GF_List *styling_SVGProperty_names;

static s32 indent = -1;

#define ATTR_BASE	Bool attr_or_prop; \
	xmlChar *svg_name; \
	char implementation_name[50];\
	xmlChar *svg_type; \
	char impl_type[50]; \
	u8 animatable; \
	u8 inheritable; 

typedef struct {
	ATTR_BASE
} SVGProperty;

typedef struct {
	ATTR_BASE
	Bool optional;
	xmlChar *default_value;
} SVGAttribute;

typedef struct
{
	xmlChar *svg_name;
	char implementation_name[50];
	Bool has_properties;
	GF_List *attributes;
} SVGElement;

SVGProperty *NewSVGProperty()
{
	SVGProperty *p;
	GF_SAFEALLOC(p, sizeof(SVGProperty))
	return p;
}

void deleteSVGProperty(SVGProperty **p)
{
	xmlFree((*p)->svg_name);
	xmlFree((*p)->svg_type);
	free(*p);
	*p = NULL;
}

SVGAttribute *NewSVGAttribute()
{
	SVGAttribute *att;
	GF_SAFEALLOC(att, sizeof(SVGAttribute))
	att->attr_or_prop = 1;
	return att;
}

void deleteSVGAttribute(SVGAttribute **p)
{
	xmlFree((*p)->svg_name);
	xmlFree((*p)->svg_type);
	free(*p);
	*p = NULL;
}

SVGElement *NewSVGElement() 
{
	SVGElement *elt;
	GF_SAFEALLOC(elt, sizeof(SVGElement));
	if (elt) {
		elt->attributes = gf_list_new();
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

void setAttributeType(SVGProperty *att) 
{
	if (!att->svg_type) {
		if (!strcmp(att->svg_name, "textContent")) {
			strcpy(att->impl_type, "SVG_TextContent");
		} else if (!strcmp(att->svg_name, "class")) {
			strcpy(att->implementation_name, "class_attribute");
			strcpy(att->impl_type, "SVG_String");
		} else if (!strcmp(att->svg_name, "visibility")) {
			strcpy(att->impl_type, "SVG_VisibilityValue");
		} else if (!strcmp(att->svg_name, "color")) {
			strcpy(att->impl_type, "SVG_Color");
		} else if (!strcmp(att->svg_name, "display")) {
			strcpy(att->impl_type, "SVG_DisplayValue");
		} else if (!strcmp(att->svg_name, "stroke-linecap")) {
			strcpy(att->impl_type, "SVG_StrokeLineCapValue");
		} else if (!strcmp(att->svg_name, "stroke-linejoin")) {
			strcpy(att->impl_type, "SVG_StrokeLineJoinValue");
		} else if (!strcmp(att->svg_name, "font-style")) {
			strcpy(att->impl_type, "SVG_FontStyleValue");
		} else if (!strcmp(att->svg_name, "text-anchor")) {
			strcpy(att->impl_type, "SVG_TextAnchorValue");
		} else if (!strcmp(att->svg_name, "fill") && att->attr_or_prop == 1) {
			strcpy(att->impl_type, "SMIL_FillValue");
		} else if (!strcmp(att->svg_name, "calcMode")) {
			strcpy(att->impl_type, "SMIL_CalcModeValue");
		} else if (!strcmp(att->svg_name, "values")) {
			strcpy(att->impl_type, "SMIL_AnimateValues");
		} else if (!strcmp(att->svg_name, "keyTimes")) {
			strcpy(att->impl_type, "SMIL_KeyTimesValues");
		} else if (!strcmp(att->svg_name, "keySplines")) {
			strcpy(att->impl_type, "SMIL_KeySplinesValues");
		} else if (!strcmp(att->svg_name, "from") || !strcmp(att->svg_name, "to") || !strcmp(att->svg_name, "by")) {
			strcpy(att->impl_type, "SMIL_AnimateValue");
		} else if (!strcmp(att->svg_name, "additive")) {
			strcpy(att->impl_type, "SMIL_AdditiveValue");
		} else if (!strcmp(att->svg_name, "accumulate")) {
			strcpy(att->impl_type, "SMIL_AccumulateValue");
		} else if (!strcmp(att->svg_name, "begin") ||
				   !strcmp(att->svg_name, "end")
				  ) {
			strcpy(att->impl_type, "SMIL_BeginOrEndValues");
		} else if (!strcmp(att->svg_name, "min") ||
				   !strcmp(att->svg_name, "max") ||
				   !strcmp(att->svg_name, "dur") ||
				   !strcmp(att->svg_name, "repeatDur")
				  ) {
			strcpy(att->impl_type, "SMIL_MinMaxDurRepeatDurValue");
		} else if (!strcmp(att->svg_name, "repeat")) {
			strcpy(att->impl_type, "SMIL_RepeatValue");
		} else if (!strcmp(att->svg_name, "restart")) {
			strcpy(att->impl_type, "SMIL_RestartValue");
		} else if (!strcmp(att->svg_name, "repeatCount")) {
			strcpy(att->impl_type, "SMIL_RepeatCountValue");
		} else if (!strcmp(att->svg_name, "attributeName")) {
			strcpy(att->impl_type, "SMIL_AttributeName");
		} else if (!strcmp(att->svg_name, "type")) {
			strcpy(att->impl_type, "SVG_AnimateTransformTypeValue");
		} else if (!strcmp(att->svg_name, "font-size")) {
			strcpy(att->impl_type, "SVG_FontSizeValue");
		} else {
			strcpy(att->impl_type, "SVG_String");
			//fprintf(stdout, "Warning: using SVG_String_datatype for attribute %s.\n", att->svg_name);
		}
	} else {
		if (strstr(att->svg_type, "datatype")) {
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

void getAttributeType(xmlDocPtr doc, xmlXPathContextPtr xpathCtx, xmlNodePtr attributeNode, SVGProperty *a) 
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
GF_List *getProperties(xmlDocPtr xc, xmlXPathContextPtr xpathCtx)
{
	GF_List *properties = gf_list_new();

	xmlNodeSetPtr props = findNodes(xpathCtx, "//rng:define[@name=\"svg.Properties.attr\" and @combine=\"interleave\"]");
	int i,j,k;
	u32 j1;

	for (k = 0; k < props->nodeNr; k++)	{
		xmlNodePtr prop = props->nodeTab[k];
		if (prop->type == XML_ELEMENT_NODE) {
			xmlNodeSetPtr refs;
			xmlNodeSetPtr attrs;
			xpathCtx->node = prop;

			/* resolve references */
			refs = findNodes(xpathCtx, "./rng:ref");
			for (i = 0; i < refs->nodeNr; i++)	{
				xmlNodePtr r = refs->nodeTab[i];
				xmlChar *ref_name = xmlGetProp(r, "name");
				xmlChar *expr = xmlStrdup("//rng:define[@name=\"");
				xmlNodeSetPtr df;
				expr = xmlStrcat(expr, ref_name);
				expr = xmlStrcat(expr, "\" and @combine=\"interleave\"]/node()");
				xpathCtx->node = prop->doc->children;
				df = findNodes(xpathCtx, expr);
				xmlReplaceNode(r, NULL);
				if (df) {
					for (j = 0; j < df->nodeNr; j++)	{
						xmlNodePtr dfn = df->nodeTab[j];
						if (dfn->type == XML_ELEMENT_NODE) {
							xmlAddChild(prop, dfn);
						}
					}
				}
			}
			
			xpathCtx->node = prop;
			attrs = findNodes(xpathCtx, "./*/rng:attribute");
			for (i=0; i<attrs->nodeNr; i++) {
				xmlNodePtr attr = attrs->nodeTab[i];
				xmlChar *attr_name = xmlGetProp(attr, "name");
				SVGProperty *aProp;
				Bool found = 0;
				for (j1=0;j1<gf_list_count(properties); j1++) {
					SVGProperty *p = gf_list_get(properties, j1);
					if (p->svg_name && !strcmp(p->svg_name, attr_name)) {
						//fprintf(stdout, "Found a duplicate SVGProperty declaration for %s\n", attr_name);
						found = 1;
						break;
					}
				}
				if (found) continue;
				aProp = malloc(sizeof(SVGProperty));
				memset(aProp, 0, sizeof(SVGProperty));
				aProp->svg_name = strdup(attr_name);
				svgNameToImplementationName(aProp->svg_name, aProp->implementation_name);
				aProp->animatable = !xmlStrcasecmp(xmlGetNsProp(attr, "animatable", SVGA_NS), "true");
				aProp->inheritable = !xmlStrcasecmp(xmlGetNsProp(attr, "inheritable", SVGA_NS), "true");
				getAttributeType(xc, xpathCtx, attr, aProp);
				setAttributeType(aProp);
				gf_list_add(properties, aProp);
			}
		}
	}
	return properties;
}

/*
void getAllAttributes(xmlDocPtr doc, xmlXPathContextPtr xpathCtx, xmlNodePtr elementNode)
{
	int k;
	xmlNodeSetPtr attributeNodes;
	xpathCtx->node = elementNode;
	attributeNodes = findNodes(xpathCtx, "//rng:attribute[not(ancestor::rng:define[@name=\"svg.Properties.attr\"])]");
	for (k = 0; k < attributeNodes->nodeNr; k++)	{
		xmlNodePtr attributeNode = attributeNodes->nodeTab[k];
		xmlChar *name = xmlGetProp(attributeNode, "name");
		//fprintf(stdout, "%s\n", name);
	}

}
*/


void getAttributes(xmlDocPtr doc, xmlXPathContextPtr xpathCtx, xmlNodePtr elementNode, SVGElement *e)
{
	xmlNodeSetPtr attribute_refNodes, attributeNodes;
	int k;
	u32 j;
	s32 i;

	indent++;

	xpathCtx->node = elementNode;
	attribute_refNodes = findNodes(xpathCtx, ".//rng:ref");
	for (k = 0; k < attribute_refNodes->nodeNr; k++)	{
		xmlNodePtr attribute_refNode = attribute_refNodes->nodeTab[k];
		if (attribute_refNode->type == XML_ELEMENT_NODE) {
			xmlChar *ref_name = xmlGetProp(attribute_refNode, "name");
//			for (i = 0; i < indent; i++) fprintf(stdout, "  ");
//			fprintf(stdout, "ref %s found\n", ref_name);
			if (xmlStrstr(ref_name, "AT") || xmlStrstr(ref_name, "attr")) {
				if (xmlStrEqual(ref_name, "svg.Properties.attr")) {
					e->has_properties = 1;
				} else if (xmlStrEqual(ref_name, "svg.Media.attr")) {
					//fprintf(stdout, "ref %s is a media attr\n", ref_name);
				} else {
					xmlNodeSetPtr refNodes;
					xmlChar *expr = xmlStrdup("//rng:define[@name=\"");
					expr = xmlStrcat(expr, ref_name);
					expr = xmlStrcat(expr, "\" and not(rng:empty) and not(rng:notAllowed)]");
					refNodes = findNodes(xpathCtx, expr);
					if (refNodes->nodeNr > 0) {
						xmlNodePtr ref = refNodes->nodeTab[0];
						getAttributes(doc, xpathCtx, ref, e);
					} else {
						//fprintf(stdout, "Warning: no matching ref %s\n", ref_name);
					}
				}
			}
		}
	}

	xpathCtx->node = elementNode;
	attributeNodes = findNodes(xpathCtx, ".//rng:attribute");
	for (k = 0; k < attributeNodes->nodeNr; k++)	{
		Bool already_exists = 0;
		xmlNodePtr attributeNode = attributeNodes->nodeTab[k];
		if (attributeNode->type == XML_ELEMENT_NODE) {
			SVGAttribute *a = NewSVGAttribute();
			a->svg_name = xmlGetProp(attributeNode, "name");
			a->optional = xmlStrEqual(attributeNode->parent->name, "optional");
			svgNameToImplementationName(a->svg_name, a->implementation_name);
			getAttributeType(doc, xpathCtx, attributeNode, (SVGProperty*)a);
			setAttributeType((SVGProperty*)a);
			for (j=0;j<gf_list_count(e->attributes); j++) {
				SVGAttribute *ta = gf_list_get(e->attributes, j);
				if (xmlStrEqual(ta->svg_name, a->svg_name)) {
					already_exists = 1;
					break;
				}
			}
			if (already_exists) {
				deleteSVGAttribute(&a);
			} else {
				//fprintf(stdout, "Adding attribute %s to element %s\n",a->svg_name, e->svg_name);
				gf_list_add(e->attributes, a);
			}
		}
	}

	xpathCtx->node = elementNode;
	attributeNodes = findNodes(xpathCtx, "//rng:text[not(ancestor::rng:attribute)]");
	if (attributeNodes->nodeNr) {
		Bool already_exists = 0;
		SVGAttribute *a = NewSVGAttribute();
		a->svg_name = xmlStrdup("textContent");
		a->optional = 1;
		svgNameToImplementationName(a->svg_name, a->implementation_name);
		setAttributeType((SVGProperty*)a);
		for (j=0;j<gf_list_count(e->attributes); j++) {
			SVGAttribute *ta = gf_list_get(e->attributes, j);
			if (xmlStrEqual(ta->svg_name, a->svg_name)) {
				already_exists = 1;
				break;
			}
		}
		if (already_exists) {
			deleteSVGAttribute(&a);
		} else {
			//fprintf(stdout, "Adding attribute %s to element %s\n",a->svg_name, e->svg_name);
			gf_list_add(e->attributes, a);
		}
		//fprintf(stdout, "Element %s can contain %d text.\n", e->svg_name, attributeNodes->nodeNr);
	}

	indent--;

}


GF_List *getElements(xmlDocPtr doc, xmlXPathContextPtr xpathCtx)
{
	GF_List *elements = gf_list_new();
	xmlNodeSetPtr elementNodes = findNodes(xpathCtx, "//rng:element");
	int k;
	for (k = 0; k < elementNodes->nodeNr; k++)	{
		xmlNodePtr elementNode = elementNodes->nodeTab[k];
		if (elementNode->type == XML_ELEMENT_NODE) {
			SVGElement *e = NewSVGElement();
			e->svg_name = xmlStrdup(xmlGetProp(elementNode, "name"));
			//fprintf(stdout, "\n\tElement %s\n", e->svg_name);
			svgNameToImplementationName(e->svg_name, e->implementation_name);
			getAttributes(doc, xpathCtx, elementNode, e);
			if (e->has_properties) {
				u32 i;
				for (i=0;i<gf_list_count(styling_SVGProperty_names); i++) {
					SVGProperty *p = gf_list_get(styling_SVGProperty_names, i);
					gf_list_add(e->attributes, p);
				}
			}
			{
				u32 i;
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

/*type: 0: header, 1: source*/
static FILE *BeginFile(u32 type)
{
	FILE *f;

	char sPath[GF_MAX_PATH];

	if (!type) {
		sprintf(sPath, "..%c..%c..%c..%cinclude%cgpac%cnodes_svg.h", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
//		sprintf(sPath, "nodes_svg.h", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
	} else {
		sprintf(sPath, "..%c..%c..%c..%csrc%cscenegraph%csvg_nodes.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
//		sprintf(sPath, "svg_nodes.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
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
	}
	fclose(f);
}

void generateProperties(FILE *output) 
{
	u32 i;
	for (i = 0; i<gf_list_count(styling_SVGProperty_names); i++) {
		SVGProperty *att = (SVGProperty *)gf_list_get(styling_SVGProperty_names, i);
		fprintf(output, "\t%s %s; /* animatable: %s, inheritable: %s */\n", att->impl_type, att->implementation_name, (att->animatable?"yes":"no"), (att->inheritable?(att->inheritable>1?"explicit":"true"):"false"));
	}
}

void generateAttributes(FILE *output, GF_List *attributes) 
{
	u32 i;
	for (i = 0; i<gf_list_count(attributes); i++) {
		SVGAttribute *att = (SVGAttribute *)gf_list_get(attributes, i);
		if (!att->attr_or_prop) {
			SVGProperty *p=(SVGProperty *)att;
			fprintf(output, "\t%s %s; /* animatable: %s, inheritable: %s */\n", p->impl_type, p->implementation_name, (p->animatable?"yes":"no"), (p->inheritable?(p->inheritable>1?"explicit":"true"):"false"));
		} else {
			fprintf(output, "\t%s %s; /* %s, animatable: %s, inheritable: %s */\n", att->impl_type, att->implementation_name, (att->optional?"optional":"required"), (att->animatable?"yes":"no"), (att->inheritable?(att->inheritable>1?"explicit":"true"):"false"));
		}
	}
}

void generateNode(FILE *output, SVGElement* svg_elt) 
{
	fprintf(output, "typedef struct _tagSVG%sElement\n{\n", svg_elt->implementation_name);
	fprintf(output, "\tBASE_NODE\n");
	fprintf(output, "\tCHILDREN\n");
	fprintf(output, "\tBASE_SVG_ELEMENT\n");
	generateAttributes(output, svg_elt->attributes);
	fprintf(output, "} SVG%sElement;\n\n\n", svg_elt->implementation_name);
}

void generateNodeImpl(FILE *output, SVGElement* svg_elt) 
{
	u32 i;
	
	fprintf(output, "static void SVG_%s_Del(GF_Node *node)\n{\n", svg_elt->implementation_name);
	fprintf(output, "\tSVG%sElement *p = (SVG%sElement *)node;\n", svg_elt->implementation_name, svg_elt->implementation_name);
	if (strcmp(svg_elt->svg_name, "video") && strcmp(svg_elt->svg_name, "font-face")) {
		for (i = 0; i < gf_list_count(svg_elt->attributes); i++) {
			SVGAttribute *att = gf_list_get(svg_elt->attributes, i);
			if (!strcmp("SMIL_KeyTimesValues", att->impl_type) ||
				!strcmp("SMIL_KeySplinesValues", att->impl_type) 
				) {
				fprintf(output, "\tSVG_DeleteCoordinates(p->%s);\n", att->implementation_name);
			} else if (!strcmp("SMIL_AnimateValues", att->impl_type)) {				
				fprintf(output, "\tSMIL_DeleteAnimateValues(&(p->%s));\n", att->implementation_name);
			} else if (!strcmp("SMIL_AnimateValue", att->impl_type)) {
				fprintf(output, "\tSMIL_DeleteAnimateValue(&(p->%s));\n", att->implementation_name);
			} else if (!strcmp("SVG_Coordinates", att->impl_type)) {
				fprintf(output, "\tSVG_DeleteCoordinates(p->%s);\n", att->implementation_name);
			} else if (!strcmp("SVG_Points", att->impl_type)) {
				fprintf(output, "\tSVG_DeletePoints(p->%s);\n", att->implementation_name);
			} else if (!strcmp("SVG_PathData", att->impl_type)) {
				fprintf(output, "\tSVG_DeletePath(&(p->d));\n");
			} else if (!strcmp("SMIL_AnimateValue", att->impl_type)) {
				fprintf(output, "\tfree(p->%s.value);\n",att->implementation_name);
			} else if (!strcmp("SMIL_AnimateValues", att->impl_type)) {
				fprintf(output, "\tDeleteChain(p->%s.values);\n",att->implementation_name);
			} else if (!att->attr_or_prop && !strcmp(att->implementation_name, "fill")) {
				fprintf(output, "\tfree(p->fill.color);\n");
			} else if (!strcmp(att->svg_name, "stroke")) {
				fprintf(output, "\tfree(p->stroke.color);\n");
			} else if (!strcmp(att->svg_name, "stroke-dasharray")) {
				fprintf(output, "\tfree(p->stroke_dasharray.array.vals);\n");
			} else if (!strcmp(att->svg_name, "stop-color")) {
				fprintf(output, "\tfree(p->stop_color.color);\n");
			} else if (!strcmp(att->svg_name, "transform")) {
				fprintf(output, "\tSVG_DeleteTransformList(p->transform);\n");
			} else if (!strcmp("SMIL_BeginOrEndValues", att->impl_type)) {
				fprintf(output, "\tSVG_DeleteBeginOrEnd(p->%s);\n", att->implementation_name);
			} else if (!strcmp(att->svg_name, "textContent")) {
				fprintf(output, "\tfree(p->textContent);\n");				
			} else if (!strcmp(att->svg_name, "font-family")) {
				fprintf(output, "\tfree(p->font_family.value.string);\n");
			} else if (!strcmp(att->svg_name, "xlink:href")) {
				fprintf(output, "\tfree(p->xlink_href.iri);\n");
			}
		}
	}
	fprintf(output, "\tgf_sg_parent_reset((GF_Node *) p);\n");
	fprintf(output, "\tgf_node_free((GF_Node *)p);\n");
	fprintf(output, "}\n\n");
	
	fprintf(output, "static GF_Err SVG_%s_get_attribute(GF_Node *node, GF_FieldInfo *info)\n{\n", svg_elt->implementation_name);
	fprintf(output, "\tswitch (info->fieldIndex) {\n");
	for (i = 0; i < gf_list_count(svg_elt->attributes); i++) {
		SVGAttribute *att = gf_list_get(svg_elt->attributes, i);
		fprintf(output, "\t\tcase %d:\n", i);
		fprintf(output, "\t\t\tinfo->name = \"%s\";\n", att->svg_name);
		fprintf(output, "\t\t\tinfo->fieldType = %s_datatype;\n", att->impl_type);
		fprintf(output, "\t\t\tinfo->far_ptr = & ((SVG%sElement *)node)->%s;\n", svg_elt->implementation_name, att->implementation_name);
		fprintf(output, "\t\t\treturn GF_OK;\n");
	}
	fprintf(output, "\t\tdefault: return GF_BAD_PARAM;\n\t}\n}\n\n");

	fprintf(output, "void *SVG_New_%s()\n{\n\tSVG%sElement *p;\n", svg_elt->implementation_name,svg_elt->implementation_name);
	fprintf(output, "\tSAFEALLOC(p, sizeof(SVG%sElement));\n\tif (!p) return NULL;\n\tgf_node_setup((GF_Node *)p, TAG_SVG_%s);\n\tgf_sg_parent_setup((GF_Node *) p);\n",svg_elt->implementation_name,svg_elt->implementation_name);
	fprintf(output, "#ifdef GF_NODE_USE_POINTERS\n");
	fprintf(output, "\t((GF_Node *p)->sgprivate->name = \"%s\";\n", svg_elt->implementation_name);
	fprintf(output, "\t((GF_Node *p)->sgprivate->node_del = SVG_%s_Del;\n", svg_elt->implementation_name);
	fprintf(output, "\t((GF_Node *p)->sgprivate->get_field = SVG_%s_get_attribute;\n", svg_elt->implementation_name);
	fprintf(output, "#endif\n");

	if (strcmp(svg_elt->svg_name, "video") && strcmp(svg_elt->svg_name, "font-face")) {
		for (i = 0; i < gf_list_count(svg_elt->attributes); i++) {
			u32 j;
			SVGAttribute *att = gf_list_get(svg_elt->attributes, i);

			/* default values should be handled more properly, generically */
			if (!att->attr_or_prop && !strcmp(att->implementation_name, "fill")) {
				fprintf(output, "\tp->fill.paintType = SVG_PAINTTYPE_INHERIT;\n");
				fprintf(output, "\tSAFEALLOC(p->fill.color, sizeof(SVG_Color));\n");
			} else if (!strcmp(att->svg_name, "color")) {
				fprintf(output, "\tp->color.colorType = SVG_COLORTYPE_INHERIT;\n");
			} else if (!strcmp(att->svg_name, "fill-rule")) {
				fprintf(output, "\tp->fill_rule = SVGFillRule_inherit;\n");
			} else if (!strcmp(att->svg_name, "fill-opacity")) {
				fprintf(output, "\tp->fill_opacity.type = SVGFLOAT_INHERIT;\n");
			} else if (!strcmp(att->svg_name, "stroke")) {
				fprintf(output, "\tp->stroke.paintType = SVG_PAINTTYPE_INHERIT;\n");
				fprintf(output, "\tSAFEALLOC(p->stroke.color, sizeof(SVG_Color));\n");
			} else if (!strcmp(att->svg_name, "stop-color")) {
				fprintf(output, "\tp->stop_color.paintType = SVG_PAINTTYPE_INHERIT;\n");
				fprintf(output, "\tSAFEALLOC(p->stop_color.color, sizeof(SVG_Color));\n");
			} else if (!strcmp(att->svg_name, "stroke-opacity")) {
				fprintf(output, "\tp->stroke_opacity.type = SVGFLOAT_INHERIT;\n");
			} else if (!strcmp(att->svg_name, "stroke-width")) {
				fprintf(output, "\tp->stroke_width.unitType = SVG_LENGTHTYPE_INHERIT;\n");
			} else if (!strcmp(att->svg_name, "stroke-linejoin")) {
				fprintf(output, "\tp->stroke_linejoin = SVGStrokeLineJoin_inherit;\n");
			} else if (!strcmp(att->svg_name, "stroke-linecap")) {
				fprintf(output, "\tp->stroke_linecap = SVGStrokeLineCap_inherit;\n");
			} else if (!strcmp(att->svg_name, "stroke-miterlimit")) {
				fprintf(output, "\tp->stroke_miterlimit.type = SVGFLOAT_INHERIT;\n");
			} else if (!strcmp(att->svg_name, "stroke-dasharray")) {
				fprintf(output, "\tp->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;\n");
			} else if (!strcmp(att->svg_name, "stroke-dashoffset")) {
				fprintf(output, "\tp->stroke_dashoffset.type = SVGFLOAT_INHERIT;\n");
			} else if (!strcmp(att->svg_name, "stroke-linecap")) {
				fprintf(output, "\tp->stroke_linecap = SVGStrokeLineCap_inherit;\n");
			} else if (!strcmp(att->svg_name, "font-size")) {
				fprintf(output, "\tp->font_size.type = SVGFLOAT_INHERIT;\n");
			} else if (!strcmp(att->svg_name, "text-anchor")) {
				fprintf(output, "\tp->text_anchor = SVG_TEXTANCHOR_INHERIT;\n");
			} else if (!strcmp(att->svg_name, "min")) {
				fprintf(output, "\tp->min.type = SMILMinMaxDurRepeatDur_clock_value;\n");
			} else if (!strcmp(att->svg_name, "repeatCount")) {
				fprintf(output, "\tp->repeatCount = FIX_ONE;\n");
			} else if (!strcmp(att->svg_name, "repeatDur")) {
				fprintf(output, "\tp->repeatDur.clock_value = -1.;\n");
			} else if (!strcmp(att->svg_name, "calcMode") && !strcmp(svg_elt->svg_name, "animateMotion")) {
				fprintf(output, "\tp->calcMode = SMILCalcMode_paced;\n");
			} else 
				/* Inialization of complex types */
			if ( !strcmp("SVG_TransformList", att->impl_type) ||
						!strcmp("SVG_Points", att->impl_type) || 
						!strcmp("SVG_Coordinates", att->impl_type) ||
						!strcmp("SMIL_KeyTimesValues", att->impl_type) ||
						!strcmp("SMIL_KeySplinesValues", att->impl_type) ||			
						!strcmp("SMIL_BeginOrEndValues", att->impl_type) 
					  ) {
				fprintf(output, "\tp->%s = gf_list_new();\n", att->implementation_name);
			} else if (!strcmp("SVG_PathData", att->impl_type)) {
				fprintf(output, "\tp->d.path_commands = gf_list_new();\n");
				fprintf(output, "\tp->d.path_points = gf_list_new();\n");
			} else if (!strcmp("SMIL_AnimateValues", att->impl_type)) {
				fprintf(output, "\tp->%s.values = gf_list_new();\n",att->implementation_name);
			}

			for (j = 0; j < gf_list_count(styling_SVGProperty_names); j++) {
				SVGProperty *prop = gf_list_get(styling_SVGProperty_names, j);
				if (prop == (SVGProperty*)att) {
				//if (!strcmp(prop->, att->svg_name) && strcmp("freeze", att->implementation_name)) {
					fprintf(output, "\tp->properties.%s = &(p->%s);\n", att->implementation_name, att->implementation_name);
					break;
				}
			}
		}
	}
	fprintf(output, "\treturn p;\n}\n\n");

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
		fprintf(f, "<table>\n", elt->implementation_name);
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
			if (!att->attr_or_prop) {
				SVGProperty *p=(SVGProperty *)att;
				fprintf(f, "<td class='property-name'>%s</td>\n",p->svg_name);
			} else {
				fprintf(f, "<td class='attribute-name'>%s</td>\n",att->svg_name);
			}
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
	
	styling_SVGProperty_names = getProperties(doc, xpathCtx);
	xmlSaveFile("completerng_props.xml", doc);

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

		/* SVGElement *SVG_CreateNode(u32 ElementTag)*/
		fprintf(output, "SVGElement *SVG_CreateNode(u32 ElementTag)\n");
		fprintf(output, "{\n");
		fprintf(output, "\tswitch (ElementTag) {\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			fprintf(output, "\t\tcase TAG_SVG_%s: return SVG_New_%s();\n",elt->implementation_name,elt->implementation_name);
		}
		fprintf(output, "\t\tdefault: return NULL;\n\t}\n}\n\n");
		
		/* void SVGElement_Del(SVGElement *elt) */
		fprintf(output, "void SVGElement_Del(SVGElement *elt)\n{\n");
		fprintf(output, "\tGF_Node *node = (GF_Node *)elt;\n");
		fprintf(output, "\tswitch (node->sgprivate->tag) {\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			fprintf(output, "\t\tcase TAG_SVG_%s: SVG_%s_Del(node); return;\n", elt->implementation_name, elt->implementation_name);
		}
		fprintf(output, "\t\tdefault: return;\n\t}\n}\n\n");

		/* u32 SVG_GetAttributeCount(SVGElement *elt) */
		fprintf(output, "u32 SVG_GetAttributeCount(GF_Node *node)\n{\n");
		fprintf(output, "\tswitch (node->sgprivate->tag) {\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			fprintf(output, "\t\tcase TAG_SVG_%s: return %i;\n", elt->implementation_name, gf_list_count(elt->attributes));
		}
		fprintf(output, "\t\tdefault: return 0;\n\t}\n}\n\n");
		
		/* GF_Err SVG_GetAttributeInfo(GF_Node *node, GF_FieldInfo *info) */
		fprintf(output, "GF_Err SVG_GetAttributeInfo(GF_Node *node, GF_FieldInfo *info)\n{\n");
		fprintf(output, "\tswitch (node->sgprivate->tag) {\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			fprintf(output, "\t\tcase TAG_SVG_%s: return SVG_%s_get_attribute(node, info);\n", elt->implementation_name, elt->implementation_name);
		}
		fprintf(output, "\t\tdefault: return GF_BAD_PARAM;\n\t}\n}\n\n");

		/* u32 SVG_GetTagByName(const char *element_name) */
		fprintf(output, "u32 SVG_GetTagByName(const char *element_name)\n{\n\tif (!element_name) return TAG_UndefinedNode;\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			fprintf(output, "\tif (!stricmp(element_name, \"%s\")) return TAG_SVG_%s;\n", elt->svg_name, elt->implementation_name);
		}
		fprintf(output, "\treturn TAG_UndefinedNode;\n}\n\n");


		/* const char *SVG_GetElementName(u32 tag) */
		fprintf(output, "const char *SVG_GetElementName(u32 tag)\n{\n\tswitch(tag) {\n");
		for (i=0; i<gf_list_count(svg_elements); i++) {
			SVGElement *elt = (SVGElement *)gf_list_get(svg_elements, i);
			fprintf(output, "\tcase TAG_SVG_%s: return \"%s\";\n", elt->implementation_name, elt->svg_name);
		}
		fprintf(output, "\tdefault: return \"UndefinedNode\";\n\t}\n}\n\n");

		fprintf(output, "#endif /*GPAC_DISABLE_SVG*/\n\n");
		EndFile(output, 1); 
	}

	xmlXPathFreeContext(xpathCtx); 
	//xmlFreeDoc(doc); 
	
	xmlCleanupParser();
	return 0;
}


