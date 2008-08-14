/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG4 Scene Graph Generator sub-project
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gpac/list.h>

#include <time.h>


#define COPYRIGHT_SCENE "/*\n *			GPAC - Multimedia Framework C SDK\n *\n *			Copyright (c) Jean Le Feuvre 2000-2005\n *					All rights reserved\n *\n *  This file is part of GPAC / Scene Graph sub-project\n *\n *  GPAC is free software; you can redistribute it and/or modify\n *  it under the terms of the GNU Lesser General Public License as published by\n *  the Free Software Foundation; either version 2, or (at your option)\n *  any later version.\n *\n *  GPAC is distributed in the hope that it will be useful,\n *  but WITHOUT ANY WARRANTY; without even the implied warranty of\n *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n *  GNU Lesser General Public License for more details.	\n *\n *  You should have received a copy of the GNU Lesser General Public\n *  License along with this library; see the file COPYING.  If not, write to\n *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.\n *\n */\n"
#define COPYRIGHT_BIFS "/*\n *			GPAC - Multimedia Framework C SDK\n *\n *			Copyright (c) Jean Le Feuvre 2000-2005\n *					All rights reserved\n *\n *  This file is part of GPAC / BIFS codec sub-project\n *\n *  GPAC is free software; you can redistribute it and/or modify\n *  it under the terms of the GNU Lesser General Public License as published by\n *  the Free Software Foundation; either version 2, or (at your option)\n *  any later version.\n *\n *  GPAC is distributed in the hope that it will be useful,\n *  but WITHOUT ANY WARRANTY; without even the implied warranty of\n *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n *  GNU Lesser General Public License for more details.	\n *\n *  You should have received a copy of the GNU Lesser General Public\n *  License along with this library; see the file COPYING.  If not, write to\n *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.\n *\n */\n"

static char *CurrentLine;

void PrintUsage()
{
	printf("MPEG4Gen [-p file] template_file_v1 (template_file_v2 ...)\n"
			"\nGPAC MPEG4 Scene Graph generator. Usage:\n"
			"-p: listing file of nodes to exclude from tables\n"
			"Template files MUST be fed in order\n"
			"\n"
			"Generated Files are directly updated in the GPAC distribution - do NOT try to change this\n\n"
			"Written by Jean Le Feuvre - (c) 2000-2005\n"
			);
}

//a node field
typedef struct
{
	char type[50];
	//SFxxx, MFxxx
	char familly[50];
	//name
	char name[1000];
	//default value
	char def[100];
	//bounds
	u32 hasBounds;
	char b_min[20];
	char b_max[20];
	//Quant
	u32 hasQuant;
	char quant_type[50];
	char qt13_bits[50];
	//Anim
	u32 hasAnim;
	u32 AnimType;

} BField;

//NDTs

//a BIFS node
typedef struct
{
	char name[1000];
	//NDT info. NDT are created in alphabetical order
	GF_List *NDT;
	//0: normal, 1: special
	u32 codingType;
	u32 version;

	GF_List *Fields;

	//coding types
	u8 hasDef, hasIn, hasOut, hasDyn;
	u8 hasAQInfo;

	u8 hasDefault;

	u8 skip_impl;

	char Child_NDT_Name[1000];
} BNode;


void skip_sep(char *sep)
{
	//skip separaors
	while (*CurrentLine && strchr(sep, *CurrentLine)) {
		CurrentLine = CurrentLine + 1;
		//end of line - no token
		if (*CurrentLine == '\n') return;
	}
}

//note that we increment the line no matter what
u32 GetNextToken(char *token, char *sep)
{
	u32 i , j = 0;

	strcpy(token, "");
	
	//skip separaors
	while (*CurrentLine && strchr(sep, *CurrentLine)) {
		CurrentLine = CurrentLine + 1;
		j ++;
		//end of line - no token
		if (*CurrentLine == '\n') return 0;
	}

	//copy token untill next blank
	i=0;
	while (1) {
		//bad line
		if (! *CurrentLine) {
			token[i] = 0;
			return 0;
		}
		//end of token or end of line
		if (strchr(sep, *CurrentLine) || (*CurrentLine == '\n') ) {
			token[i] = 0;
			CurrentLine = CurrentLine + 1;
			return i;
		} else {
			token[i] = *CurrentLine;
		}
		CurrentLine = CurrentLine + 1;
		i++;
		j++;
	}
	return 1;
}


char szFixedVal[5000];
char *GetFixedPrintout(char *val)
{
	if (!strcmp(val, "FIX_MIN") || !strcmp(val, "FIX_MAX")) return val;
	/*composite texture...*/
	if (!strcmp(val, "65535")) return "FIX_MAX /*WARNING: modified to allow 16.16 fixed point version!!*/";
	sprintf(szFixedVal, "FLT2FIX(%s)", val);
	return szFixedVal;
}

BField *BlankField()
{
	BField *n = malloc(sizeof(BField));
	memset(n, 0, sizeof(BField));
	return n;
}


BNode *BlankNode()
{
	BNode *n = malloc(sizeof(BNode));
	memset(n, 0, sizeof(BNode));
	n->NDT = gf_list_new();
	n->Fields = gf_list_new();
	return n;
}

u8 IsNDT(GF_List *NDTs, char *famName)
{
	u32 i;
	char *ndtName;
	for (i=0; i<gf_list_count(NDTs); i++) {
		ndtName = gf_list_get(NDTs, i);
		//remove SF / MF as we don't need that
		if (!strcmp(ndtName+2, famName+2)) return 1;
	}
	return 0;
}

void CheckInTable(char *token, GF_List *NDTs)
{
	u32 i;
	char *p;
	for (i=0; i<gf_list_count(NDTs); i++) {
		p = gf_list_get(NDTs, i);
		if (!strcmp(p, token)) return;
	}
	p = malloc(strlen(token)+1);
	strcpy(p, token);
	gf_list_add(NDTs, p);
}

/*type: 0: header, 1: source*/
FILE *BeginFile(char *name, u32 type)
{
	FILE *f;
	char *cprt = COPYRIGHT_SCENE;
	char sPath[GF_MAX_PATH];

	if (!type) {
		if (!strcmp(name, "NDT")) {
			sprintf(sPath, "..%c..%c..%cinclude%cgpac%cinternal%cbifs_tables.h", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
			cprt = COPYRIGHT_BIFS;
		}
		/*nodes_mpeg4.h is exported*/
		else {
			sprintf(sPath, "..%c..%c..%cinclude%cgpac%cnodes_mpeg4.h", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		}
	} else {
		if (!stricmp(name, "NDT")) {
			sprintf(sPath, "..%c..%c..%csrc%cbifs%cbifs_node_tables.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
			cprt = COPYRIGHT_BIFS;
		} else {
			sprintf(sPath, "..%c..%c..%csrc%cscenegraph%c%s.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, name);
		}
	}
	
	f = fopen(sPath, "wt");
	fprintf(f, "%s\n", cprt);



	{
		time_t rawtime;
		time(&rawtime);
		fprintf(f, "\n/*\n\tDO NOT MOFIFY - File generated on GMT %s\n\tBY MPEG4Gen for GPAC Version %s\n*/\n\n", asctime(gmtime(&rawtime)), GPAC_VERSION);
	}

	if (!type) {
		fprintf(f, "#ifndef _%s_H\n", name);
		fprintf(f, "#define _%s_H\n\n", name);
		if (!strcmp(name, "nodes_mpeg4")) {
			fprintf(f, "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n");
		}
	}
	return f;
}

void EndFile(FILE *f, char *name, u32 type)
{
	if (!type) {
		if (!strcmp(name, "nodes_mpeg4")) {
			fprintf(f, "#ifdef __cplusplus\n}\n#endif\n\n");
		}
		fprintf(f, "\n\n#endif\t\t/*_%s_H*/\n\n", name);
	}
	fclose(f);
}

void TranslateToken(char *token)
{
	if (!strcmp(token, "+I") || !strcmp(token, "I")) {
		strcpy(token, "FIX_MAX");
	}
	else if (!strcmp(token, "-I")) {
		strcpy(token, "FIX_MIN");
	}
}



void WriteNodesFile(GF_List *BNodes, GF_List *NDTs, u32 NumVersions)
{
	FILE *f;
	u32 i, j;
	char *NDTName;
	BNode *n;
	BField *bf;
	Bool hasViewport;

	f = BeginFile("nodes_mpeg4", 0);

	fprintf(f, "#include <gpac/scenegraph_vrml.h>\n\n");

	//write all tags
	fprintf(f, "\n\nenum {\n");
	
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (i)
			fprintf(f, ",\n\tTAG_MPEG4_%s", n->name);
		else
			fprintf(f, "\tTAG_MPEG4_%s = GF_NODE_RANGE_FIRST_MPEG4", n->name);
	}
	fprintf(f, ",\n\tTAG_LastImplementedMPEG4\n};\n\n");

	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (n->skip_impl) continue;

		fprintf(f, "typedef struct _tag%s\n{\n", n->name);
		fprintf(f, "\tBASE_NODE\n");

		/*write children field*/
		for (j=0; j<gf_list_count(n->Fields); j++) {
			bf = gf_list_get(n->Fields, j);
			if (!stricmp(bf->name, "addChildren") || !strcmp(bf->name, "removeChildren")) continue;
			if (!strcmp(bf->name, "children") && stricmp(n->name, "audioBuffer")) {
				fprintf(f, "\tVRML_CHILDREN\n");
				break;
			}
		}
		for (j=0; j<gf_list_count(n->Fields); j++) {
			bf = gf_list_get(n->Fields, j);
			
			if (!strcmp(bf->name, "addChildren") || !strcmp(bf->name, "removeChildren")) continue;
			if (!strcmp(bf->name, "children") && stricmp(n->name, "audioBuffer")) continue;

			//write remaining fields
			//eventIn fields are handled as pointer to functions, called by the route manager
			if (!strcmp(bf->type, "eventIn")) {
				fprintf(f, "\t%s %s;\t/*eventIn*/\n", bf->familly, bf->name);
				fprintf(f, "\tvoid (*on_%s)(GF_Node *pThis);\t/*eventInHandler*/\n", bf->name);
			} else if (!strcmp(bf->type, "eventOut")) {
				//eventOut fields are handled as an opaque stack pointing to the route manager
				//this will be refined once the route is in place
				//we will likely need a function such as:
				//	void SignalRoute(route_stack, node, par)
				fprintf(f, "\t%s %s;\t/*eventOut*/\n", bf->familly, bf->name);
			} else if (strstr(bf->familly, "Node")) {		
				//this is a POINTER to a node 
				if (strstr(bf->familly, "SF")) {
					fprintf(f, "\tGF_Node *%s;\t/*%s*/\n", bf->name, bf->type);
				} else {
					//this is a POINTER to a chain
					fprintf(f, "\tGF_ChildNodeItem *%s;\t/*%s*/\n", bf->name, bf->type);
				}
			} else {
				fprintf(f, "\t%s %s;\t/*%s*/\n", bf->familly, bf->name, bf->type);
			}
		}
		fprintf(f, "} M_%s;\n\n\n", n->name);
	}

	
	hasViewport = 0;
	//all NDTs are defined in v1
	fprintf(f, "/*NodeDataType tags*/\nenum {\n");
	for (i=0; i<gf_list_count(NDTs); i++) {
		NDTName = gf_list_get(NDTs, i);
		if (!i) {
			fprintf(f, "\tNDT_%s = 1", NDTName);
		} else {
			fprintf(f, ",\n\tNDT_%s", NDTName);
		}

		if (strcmp(NDTName, "SFViewportNode")) hasViewport = 1;
	}
	//template fix: some node use NDT_SFViewportNode but the table is empty-> not generated
	if (!hasViewport) fprintf(f, ",\n\tNDT_SFViewportNode");
	fprintf(f, "\n};\n\n");


	fprintf(f, "/*All BIFS versions handled*/\n");
	fprintf(f, "#define GF_BIFS_NUM_VERSION\t\t%d\n\n", NumVersions);
	fprintf(f, "enum {\n");
	for (i=0; i<NumVersions; i++) {
		if (!i) {
			fprintf(f, "\tGF_BIFS_V1 = 1,\n");
		} else {
			fprintf(f, "\tGF_BIFS_V%d,\n", i+1);
		}
	}
	fprintf(f, "\tGF_BIFS_LAST_VERSION = GF_BIFS_V%d\n};\n\n", i);
	fprintf(f, "\n\n");

	EndFile(f, "nodes_mpeg4", 0);

}


u32 IsNodeInTable(BNode *node, char *NDTName)
{
	u32 i;
	char *ndt;

	for (i=0; i<gf_list_count(node->NDT); i++) {
		ndt = gf_list_get(node->NDT, i);
		if (!strcmp(ndt, NDTName)) return 1;
	}
	return 0;
}

u32 GetBitsCount(u32 MaxVal)
{
	u32 k=0;

	while ((s32) MaxVal > ((1<<k)-1) ) k+=1;
	return k;
}

u32 GetNDTCount(char *NDTName, GF_List *BNodes, u32 Version)
{
	u32 i, nodeCount;
	BNode *n;

	//V1 begins at 0
	if (Version == 1) {
		nodeCount = 0;
	}
	//V2 at 2 (0 reserved + 1 proto)
	else if (Version == 2) {
		nodeCount = 2;
	}
	//other at 1 (0 reserved, no proto)
	else {
		nodeCount = 1;
	}
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (n->version != Version) continue;
		if (!IsNodeInTable(n, NDTName)) continue;
		nodeCount++;
	}
	return nodeCount;

}

void WriteNDT_H(FILE *f, GF_List *BNodes, GF_List *NDTs, u32 Version)
{
	u32 i, j, first, count;
	char *NDTName;
	BNode *n;


	fprintf(f, "\n\n/* NDT BIFS Version %d */\n\n", Version);

	//for all NDTs
	for (i=0; i<gf_list_count(NDTs); i++) {
		NDTName = gf_list_get(NDTs, i);
		count = GetNDTCount(NDTName, BNodes, Version);
		if (Version == 2) {
			count -= 2;
		} else if (Version > 2) {
			count -= 1;
		}
		if (!count) continue;
		
		//numBits
		fprintf(f, "#define %s_V%d_NUMBITS\t\t%d\n", NDTName, Version, GetBitsCount(count + ( (Version == 2) ? 1 : 0) ) );
		fprintf(f, "#define %s_V%d_Count\t%d\n\n", NDTName, Version, count);
		
		fprintf(f, "static const u32 %s_V%d_TypeToTag[%d] = {\n", NDTName, Version, count);
		first = 1;
		//browse each node.
		for (j=0; j<gf_list_count(BNodes); j++) {
			n = gf_list_get(BNodes, j);
			if (n->version != Version) continue;
			if (!IsNodeInTable(n, NDTName)) continue;
			
			if (first) {
				fprintf(f, " TAG_MPEG4_%s", n->name);
				first = 0;
			} else {
				fprintf(f, ", TAG_MPEG4_%s", n->name);
			}
		}
		fprintf(f, "\n};\n\n");

	}

	fprintf(f, "\nu32 NDT_V%d_GetNumBits(u32 NDT_Tag);\n", Version);
	fprintf(f, "u32 NDT_V%d_GetNodeTag(u32 Context_NDT_Tag, u32 NodeType);\n", Version);
	fprintf(f, "u32 NDT_V%d_GetNodeType(u32 NDT_Tag, u32 NodeTag);\n", Version);
	
	fprintf(f, "\n\n");
}

//write the NDTs functions for v1 nodes
//all our internal handling is in TAG_MPEG4_#nodename because we need an homogeneous
//namespace for all nodes (v1, v2, v3 and v4)
//the NDT functions will perform the translation from the NDT value to the absolute
//TAG of the node
void WriteNDT_Dec(FILE *f, GF_List *BNodes, GF_List *NDTs, u32 Version)
{
	char *NDTName;
	u32 i, count;

	//NodeTag complete translation
	fprintf(f, "\n\n\nu32 NDT_V%d_GetNodeTag(u32 Context_NDT_Tag, u32 NodeType)\n{\n\tif (!NodeType) return 0;\n", Version);

	//handle version
	fprintf(f, "\t/* adjust according to the table version */\n");
	if (Version == 2) {
		fprintf(f, "\t/* v2: 0 reserved for extensions, 1 reserved for protos */\n");
		fprintf(f, "\tif (NodeType == 1) return 0;\n");
		fprintf(f, "\tNodeType -= 2;\n");
	} else {
		fprintf(f, "\t/* v%d: 0 reserved for extensions */\n", Version);
		fprintf(f, "\tNodeType -= 1;\n");
	}

	fprintf(f, "\tswitch (Context_NDT_Tag) {\n");

	for (i=0; i<gf_list_count(NDTs); i++) {
		NDTName = gf_list_get(NDTs, i);

		count = GetNDTCount(NDTName, BNodes, Version);
		if (Version == 2) {
			count -= 2;
		} else if (Version > 2) {
			count -= 1;
		}
		if (!count) continue;
		
		fprintf(f, "\tcase NDT_%s:\n\t\tif (NodeType >= %s_V%d_Count) return 0;\n", NDTName, NDTName, Version);
		fprintf(f, "\t\treturn %s_V%d_TypeToTag[NodeType];\n", NDTName, Version);
	}
	fprintf(f, "\tdefault:\n\t\treturn 0;\n\t}\n}");

	//NDT codec bits
	fprintf(f, "\n\n\nu32 NDT_V%d_GetNumBits(u32 NDT_Tag)\n{\n\tswitch (NDT_Tag) {\n", Version);

	for (i=0; i<gf_list_count(NDTs); i++) {
		NDTName = gf_list_get(NDTs, i);

		count = GetNDTCount(NDTName, BNodes, Version);
		if (Version == 2) {
			count -= 2;
		} else if (Version > 2) {
			count -= 1;
		}
		if (!count) continue;
		
		fprintf(f, "\tcase NDT_%s:\n\t\treturn %s_V%d_NUMBITS;\n", NDTName, NDTName, Version);
	}
	/*all tables have 1 node in v2 for proto coding*/
	fprintf(f, "\tdefault:\n\t\treturn %d;\n\t}\n}\n\n", (Version==2) ? 1 : 0);
}


void WriteNDT_Enc(FILE *f, GF_List *BNodes, GF_List *NDTs, u32 Version)
{
	u32 i, count;
	char *NDTName;

	fprintf(f, "u32 NDT_V%d_GetNodeType(u32 NDT_Tag, u32 NodeTag)\n{\n\tif(!NDT_Tag || !NodeTag) return 0;\n\tswitch(NDT_Tag) {\n", Version);
	for (i=0; i<gf_list_count(NDTs); i++) {
		NDTName = gf_list_get(NDTs, i);
		count = GetNDTCount(NDTName, BNodes, Version);
		if (Version == 2) {
			count -= 2;
		} else if (Version > 2) {
			count -= 1;
		}
		if (!count) continue;
		fprintf(f, "\tcase NDT_%s:\n\t\treturn ALL_GetNodeType(%s_V%d_TypeToTag, %s_V%d_Count, NodeTag, GF_BIFS_V%d);\n", NDTName, NDTName, Version, NDTName, Version, Version);
	}
	fprintf(f, "\tdefault:\n\t\treturn 0;\n\t}\n}\n\n");
}



void WriteNodeFields(FILE *f, BNode *n)
{
	u32 i, first;
	BField *bf;
	u32 NbDef, NbIn, NbOut, NbDyn, hasAQ;

	NbDef = NbIn = NbOut = NbDyn = hasAQ = 0;
	for (i=0;i<gf_list_count(n->Fields); i++) {
		bf = gf_list_get(n->Fields, i);
		if (!strcmp(bf->type, "field") || !strcmp(bf->type, "exposedField")) {
			NbDef += 1;
		}
		if (!strcmp(bf->type, "eventIn") || !strcmp(bf->type, "exposedField")) {
			NbIn += 1;
			//check for anim
			if (bf->hasAnim) NbDyn += 1;
		}
		if (!strcmp(bf->type, "eventOut") || !strcmp(bf->type, "exposedField")) {
			NbOut += 1;
		}
		if (bf->hasAnim || bf->hasQuant) hasAQ = 1;
	}

	n->hasAQInfo = hasAQ;

	//write the def2all table
	if (NbDef) {
		first = 1;
		fprintf(f, "static const u16 %s_Def2All[] = { ", n->name);
		for (i=0; i<gf_list_count(n->Fields); i++) {
			bf = gf_list_get(n->Fields, i);
			if (strcmp(bf->type, "field") && strcmp(bf->type, "exposedField")) continue;
			if (first) {
				fprintf(f, "%d", i);
				first = 0;
			} else {
				fprintf(f, ", %d", i);
			}
		}
		fprintf(f, "};\n");
	}
	//write the in2all table
	if (NbIn) {
		first = 1;
		fprintf(f, "static const u16 %s_In2All[] = { ", n->name);
		for (i=0; i<gf_list_count(n->Fields); i++) {
			bf = gf_list_get(n->Fields, i);
			if (strcmp(bf->type, "eventIn") && strcmp(bf->type, "exposedField")) continue;
			if (first) {
				fprintf(f, "%d", i);
				first = 0;
			} else {
				fprintf(f, ", %d", i);
			}
		}
		fprintf(f, "};\n");
	}
	//write the out2all table
	if (NbOut) {
		first = 1;
		fprintf(f, "static const u16 %s_Out2All[] = { ", n->name);
		for (i=0; i<gf_list_count(n->Fields); i++) {
			bf = gf_list_get(n->Fields, i);
			if (strcmp(bf->type, "eventOut") && strcmp(bf->type, "exposedField")) continue;
			if (first) {
				fprintf(f, "%d", i);
				first = 0;
			} else {
				fprintf(f, ", %d", i);
			}
		}
		fprintf(f, "};\n");
	}
	//then write the dyn2all table
	if (NbDyn) {
		first = 1;
		fprintf(f, "static const u16 %s_Dyn2All[] = { ", n->name);
		for (i=0; i<gf_list_count(n->Fields); i++) {
			bf = gf_list_get(n->Fields, i);
			if (strcmp(bf->type, "eventIn") && strcmp(bf->type, "exposedField")) continue;
			if (!bf->hasAnim) continue;
			if (first) {
				fprintf(f, "%d", i);
				first = 0;
			} else {
				fprintf(f, ", %d", i);
			}
		}
		fprintf(f, "};\n");
	}

	n->hasDef = NbDef;
	n->hasDyn = NbDyn;
	n->hasIn = NbIn;
	n->hasOut = NbOut;


	fprintf(f, "\nstatic u32 %s_get_field_count(GF_Node *node, u8 IndexMode)\n{\n", n->name);
	fprintf(f, "\tswitch(IndexMode) {\n");

	fprintf(f, "\tcase GF_SG_FIELD_CODING_IN: return %d;\n", NbIn);
	fprintf(f, "\tcase GF_SG_FIELD_CODING_DEF: return %d;\n", NbDef);
	fprintf(f, "\tcase GF_SG_FIELD_CODING_OUT: return %d;\n", NbOut);
	fprintf(f, "\tcase GF_SG_FIELD_CODING_DYN: return %d;\n", NbDyn);
	fprintf(f, "\tdefault:\n");
	fprintf(f, "\t\treturn %d;\n", gf_list_count(n->Fields));
	fprintf(f, "\t}");

	fprintf(f, "\n}\n");

	fprintf(f, "\nstatic GF_Err %s_get_field_index(GF_Node *n, u32 inField, u8 IndexMode, u32 *allField)\n{\n", n->name);
	fprintf(f, "\tswitch(IndexMode) {\n");

	if (NbIn) {
		fprintf(f, "\tcase GF_SG_FIELD_CODING_IN:\n");
		fprintf(f, "\t\t*allField = %s_In2All[inField];\n\t\treturn GF_OK;\n", n->name);
	}
	if (NbDef) {
		fprintf(f, "\tcase GF_SG_FIELD_CODING_DEF:\n");
		fprintf(f, "\t\t*allField = %s_Def2All[inField];\n\t\treturn GF_OK;\n", n->name);
	}
	if (NbOut) {
		fprintf(f, "\tcase GF_SG_FIELD_CODING_OUT:\n");
		fprintf(f, "\t\t*allField = %s_Out2All[inField];\n\t\treturn GF_OK;\n", n->name);
	}
	if (NbDyn) {
		fprintf(f, "\tcase GF_SG_FIELD_CODING_DYN:\n");
		fprintf(f, "\t\t*allField = %s_Dyn2All[inField];\n\t\treturn GF_OK;\n", n->name);
	}

	fprintf(f, "\tdefault:\n");
	fprintf(f, "\t\treturn GF_BAD_PARAM;\n");
	fprintf(f, "\t}");

	fprintf(f, "\n}\n");


	fprintf(f, "static GF_Err %s_get_field(GF_Node *node, GF_FieldInfo *info)\n{\n\tswitch (info->fieldIndex) {\n", n->name);
	for (i=0;i<gf_list_count(n->Fields); i++) {
		bf = gf_list_get(n->Fields, i);

		fprintf(f, "\tcase %d:\n", i);
		
		fprintf(f, "\t\tinfo->name = \"%s\";\n", bf->name);

		//skip all eventIn
		if (!strcmp(bf->type, "eventIn")) {
			fprintf(f, "\t\tinfo->eventType = GF_SG_EVENT_IN;\n");
			fprintf(f, "\t\tinfo->on_event_in = ((M_%s *)node)->on_%s;\n", n->name, bf->name);
		}
		else if (!strcmp(bf->type, "eventOut")) {
			fprintf(f, "\t\tinfo->eventType = GF_SG_EVENT_OUT;\n");
		}
		else if (!strcmp(bf->type, "field")) {
			fprintf(f, "\t\tinfo->eventType = GF_SG_EVENT_FIELD;\n");
		}
		else {
			fprintf(f, "\t\tinfo->eventType = GF_SG_EVENT_EXPOSED_FIELD;\n");
		}

		if (strstr(bf->familly, "Node")) {
			if (strstr(bf->familly, "MF")) {
				fprintf(f, "\t\tinfo->fieldType = GF_SG_VRML_MFNODE;\n");
			} else {
				fprintf(f, "\t\tinfo->fieldType = GF_SG_VRML_SFNODE;\n");
			}
			//always remove the SF or MF, as all NDTs are SFXXX
			fprintf(f, "\t\tinfo->NDTtype = NDT_SF%s;\n", bf->familly+2);
			fprintf(f, "\t\tinfo->far_ptr = & ((M_%s *)node)->%s;\n", n->name, bf->name);
		} else {
			char szName[20];
			strcpy(szName, bf->familly);
			strupr(szName);
			//no ext type
			fprintf(f, "\t\tinfo->fieldType = GF_SG_VRML_%s;\n", szName);
			fprintf(f, "\t\tinfo->far_ptr = & ((M_%s *) node)->%s;\n", n->name, bf->name);
		}
		fprintf(f, "\t\treturn GF_OK;\n");
	}
	fprintf(f, "\tdefault:\n\t\treturn GF_BAD_PARAM;\n\t}\n}\n\n");

	fprintf(f, "\nstatic s32 %s_get_field_index_by_name(char *name)\n{\n", n->name);
	for (i=0;i<gf_list_count(n->Fields); i++) {
		bf = gf_list_get(n->Fields, i);
		fprintf(f, "\tif (!strcmp(\"%s\", name)) return %d;\n", bf->name, i);
	}
	fprintf(f, "\treturn -1;\n\t}\n");
}


//write the Quantization info for each node field(Quant and BIFS-Anim info)
void WriteNodeQuant(FILE *f, BNode *n)
{
	u32 i;
	BField *bf;
	fprintf(f, "static Bool %s_get_aq_info(GF_Node *n, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)\n{\n\tswitch (FieldIndex) {\n", n->name);
	
	for (i=0; i<gf_list_count(n->Fields) ; i++ ) {
		bf = gf_list_get(n->Fields, i);
		if (!bf->hasAnim && !bf->hasQuant) continue;

		fprintf(f, "\tcase %d:\n", i);
		//Anim Type
		fprintf(f, "\t\t*AType = %d;\n", bf->AnimType);
		//Quant Type
		fprintf(f, "\t\t*QType = %s;\n", bf->quant_type);
		if (!strcmp(bf->quant_type, "13")) 
			fprintf(f, "\t\t*QT13_bits = %s;\n", bf->qt13_bits);

		//Bounds
		if (bf->hasBounds) {
			if (!strcmp(bf->b_min, "+I") || !strcmp(bf->b_min, " +I") || !strcmp(bf->b_min, "I")) {
				fprintf(f, "\t\t*b_min = FIX_MAX;\n");
			} else if (!strcmp(bf->b_min, "-I")) {
				fprintf(f, "\t\t*b_min = FIX_MIN;\n");
			} else {
				fprintf(f, "\t\t*b_min = %s;\n", GetFixedPrintout(bf->b_min));
			}
			if (!strcmp(bf->b_max, "+I") || !strcmp(bf->b_max, " +I") || !strcmp(bf->b_max, "I")) {
				fprintf(f, "\t\t*b_max = FIX_MAX;\n");
			} else {
				fprintf(f, "\t\t*b_max = %s;\n", GetFixedPrintout(bf->b_max));
			}
		}
		fprintf(f, "\t\treturn 1;\n");
	}
	fprintf(f, "\tdefault:\n\t\treturn 0;\n\t}\n}\n\n");
}

void WriteNodeCode(GF_List *BNodes)
{
	FILE *f;
	char token[20], tok[20];
	char *store;
	u32 i, j, k, go;
	BField *bf;
	BNode *n;

	f = BeginFile("mpeg4_nodes", 1);

	fprintf(f, "#include <gpac/nodes_mpeg4.h>\n\n");
	fprintf(f, "\n#include <gpac/internal/scenegraph_dev.h>\n");

	for (k=0; k<gf_list_count(BNodes); k++) {
		Bool is_parent = 0;
		n = gf_list_get(BNodes, k);

		if (n->skip_impl) continue;

		fprintf(f, "\n/*\n\t%s Node deletion\n*/\n\n", n->name);
		fprintf(f, "static void %s_Del(GF_Node *node)\n{\n\tM_%s *p = (M_%s *) node;\n", n->name, n->name, n->name);

		for (i=0; i<gf_list_count(n->Fields); i++) {
			bf = gf_list_get(n->Fields, i);
			//nothing on child events
			if (!strcmp(bf->name, "addChildren")) continue;
			if (!strcmp(bf->name, "removeChildren")) continue;
			
			//delete all children node
			if (!strcmp(bf->name, "children") && stricmp(n->name, "audioBuffer")) {
				is_parent = 1;
				continue;
			}

			//delete ALL fields that must be deleted: this includes eventIn and out since 
			//all fields are defined in the node
			if (!strcmp(bf->familly, "MFInt")
				|| !strcmp(bf->familly, "MFFloat")
				|| !strcmp(bf->familly, "MFDouble")
				|| !strcmp(bf->familly, "MFBool")
				|| !strcmp(bf->familly, "MFInt32")
				|| !strcmp(bf->familly, "MFColor")
				|| !strcmp(bf->familly, "MFRotation")
				|| !strcmp(bf->familly, "MFString")
				|| !strcmp(bf->familly, "MFTime")
				|| !strcmp(bf->familly, "MFVec2f")
				|| !strcmp(bf->familly, "MFVec3f")
				|| !strcmp(bf->familly, "MFVec4f")
				|| !strcmp(bf->familly, "MFVec2d")
				|| !strcmp(bf->familly, "MFVec3d")
				|| !strcmp(bf->familly, "MFURL")
				|| !strcmp(bf->familly, "MFScript")
				|| !strcmp(bf->familly, "SFString")
				|| !strcmp(bf->familly, "SFURL")
				|| !strcmp(bf->familly, "SFImage")
				) {
				char szName[500];
				strcpy(szName, bf->familly);
				strlwr(szName);
				fprintf(f, "\tgf_sg_%s_del(p->%s);\n", szName, bf->name);
			}
			else if (!strcmp(bf->familly, "SFCommandBuffer")) {
				fprintf(f, "\tgf_sg_sfcommand_del(p->%s);\n", bf->name);
			}
			else if (strstr(bf->familly, "Node")) {		
				//this is a POINTER to a node 
				if (strstr(bf->familly, "SF")) {
					fprintf(f, "\tgf_node_unregister((GF_Node *) p->%s, (GF_Node *) p);\t\n", bf->name);
				} else {
					//this is a POINTER to a chain
					fprintf(f, "\tgf_node_unregister_children((GF_Node *) p, p->%s);\t\n", bf->name);
				}
			}
		}
		if (is_parent) fprintf(f, "\tgf_sg_vrml_parent_destroy((GF_Node *) p);\t\n");
		fprintf(f, "\tgf_node_free((GF_Node *) p);\n}\n\n");

		//node fields
		WriteNodeFields(f, n);
		WriteNodeQuant(f, n);

		//
		//		Constructor
		//

		fprintf(f, "\n\nGF_Node *%s_Create()\n{\n\tM_%s *p;\n\tGF_SAFEALLOC(p, M_%s);\n", n->name, n->name, n->name);
		fprintf(f, "\tif(!p) return NULL;\n");
		fprintf(f, "\tgf_node_setup((GF_Node *)p, TAG_MPEG4_%s);\n", n->name);

		for (i=0; i<gf_list_count(n->Fields); i++) {
			bf = gf_list_get(n->Fields, i);
			//setup all children node
			if (!strcmp(bf->name, "children") && stricmp(n->name, "audioBuffer")) {
				fprintf(f, "\tgf_sg_vrml_parent_setup((GF_Node *) p);\n");
				break;
			}
			else if ( strstr(bf->familly, "Node") && strncmp(bf->type, "event", 5) ) {		
#if 0
				//this is a POINTER to a node 
				if (strstr(bf->familly, "MF")) {
					//this is a POINTER to a chain
					fprintf(f, "\tp->%s = gf_list_new();\t\n", bf->name);
				}
#endif
			}
			/*special case for SFCommandBuffer: we also create a command list*/
			if (!stricmp(bf->familly, "SFCommandBuffer")) {
				fprintf(f, "\tp->%s.commandList = gf_list_new();\t\n", bf->name);
			}
		}

		//check if we have a child node
		for (i=0; i<gf_list_count(n->Fields); i++) {
			bf = gf_list_get(n->Fields, i);
			if ( !strcmp(bf->name, "children") || 
					( !strstr(bf->type, "event") && strstr(bf->familly, "MF") && strstr(bf->familly, "Node")) ) {
				sprintf(n->Child_NDT_Name, "NDT_SF%s", bf->familly+2);
				break;
			}
		}

		fprintf(f, "\n\t/*default field values*/\n");
		
		for (i=0; i<gf_list_count(n->Fields); i++) {
			bf = gf_list_get(n->Fields, i);

			//nothing on eventIn or Out
			if (!strcmp(bf->type, "eventIn")) continue;
			if (!strcmp(bf->type, "eventOut")) continue;

			if (!strcmp(bf->def, "")) continue;

			//no default on nodes
			if (strstr(bf->familly, "Node")) continue;
			//extract default falue
			
			//
			//		SF Fields
			//
			
			//SFBool
			if (!strcmp(bf->familly, "SFBool")) {
				if (!strcmp(bf->def, "1") || !strcmp(bf->def, "TRUE")) 
					fprintf(f, "\tp->%s = 1;\n", bf->name);
			}
			//SFFloat
			else if (!strcmp(bf->familly, "SFFloat")) {
				fprintf(f, "\tp->%s = %s;\n", bf->name, GetFixedPrintout(bf->def));
			}
			//SFTime
			else if (!strcmp(bf->familly, "SFTime")) {
				fprintf(f, "\tp->%s = %s;\n", bf->name, bf->def);
			}
			//SFInt32
			else if (!strcmp(bf->familly, "SFInt32")) {
				fprintf(f, "\tp->%s = %s;\n", bf->name, bf->def);
			}
			//SFColor
			else if (!strcmp(bf->familly, "SFColor")) {
				CurrentLine = bf->def;
				GetNextToken(token, " ");
				TranslateToken(token);

				fprintf(f, "\tp->%s.red = %s;\n", bf->name, GetFixedPrintout(token));
				GetNextToken(token, " ");
				fprintf(f, "\tp->%s.green = %s;\n", bf->name, GetFixedPrintout(token));
				GetNextToken(token, " ");
				fprintf(f, "\tp->%s.blue = %s;\n", bf->name, GetFixedPrintout(token));
			}
			//SFVec2f
			else if (!strcmp(bf->familly, "SFVec2f")) {
				CurrentLine = bf->def;
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(f, "\tp->%s.x = %s;\n", bf->name, GetFixedPrintout(token));
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(f, "\tp->%s.y = %s;\n", bf->name, GetFixedPrintout(token));
			}
			//SFVec3f
			else if (!strcmp(bf->familly, "SFVec3f")) {
				CurrentLine = bf->def;
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(f, "\tp->%s.x = %s;\n", bf->name, GetFixedPrintout(token));

				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(f, "\tp->%s.y = %s;\n", bf->name, GetFixedPrintout(token));

				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(f, "\tp->%s.z = %s;\n", bf->name, GetFixedPrintout(token));
			}
			//SFVec4f & SFRotation
			else if (!strcmp(bf->familly, "SFVec4f") || !strcmp(bf->familly, "SFRotation")) {
				CurrentLine = bf->def;
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(f, "\tp->%s.x = %s;\n", bf->name, GetFixedPrintout(token));

				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(f, "\tp->%s.y = %s;\n", bf->name, GetFixedPrintout(token));

				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(f, "\tp->%s.z = %s;\n", bf->name, GetFixedPrintout(token));

				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(f, "\tp->%s.q = %s;\n", bf->name, GetFixedPrintout(token));
			}
			//SFString
			else if (!strcmp(bf->familly, "SFString")) {
				fprintf(f, "\tp->%s.buffer = (char*)malloc(sizeof(char) * %d);\n", bf->name, strlen(bf->def)+1);
				fprintf(f, "\tstrcpy(p->%s.buffer, \"%s\");\n", bf->name, bf->def);
			}
		
			//
			//		MF Fields
			//
			//MFFloat
			else if (!strcmp(bf->familly, "MFFloat")) {
				j = 0;
				CurrentLine = bf->def;
				while (GetNextToken(token, " ,")) j++;
				j+=1;
				fprintf(f, "\tp->%s.vals = (SFFloat*)malloc(sizeof(SFFloat)*%d);\n", bf->name, j);
				fprintf(f, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, " ,")) go = 0;
					TranslateToken(token);
					fprintf(f, "\tp->%s.vals[%d] = %s;\n", bf->name, j, GetFixedPrintout(token));
					j+=1;
				}
			}
			//MFVec2f
			else if (!strcmp(bf->familly, "MFVec2f")) {
				j = 0;
				CurrentLine = bf->def;
				while (GetNextToken(token, ",")) j++;
				j+=1;
				fprintf(f, "\tp->%s.vals = (SFVec2f*)malloc(sizeof(SFVec2f)*%d);\n", bf->name, j);
				fprintf(f, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(f, "\tp->%s.vals[%d].x = %s;\n", bf->name, j, GetFixedPrintout(tok));
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(f, "\tp->%s.vals[%d].y = %s;\n", bf->name, j, GetFixedPrintout(tok));
					j+=1;
					CurrentLine = store;
				}
			}
			//MFVec3f
			else if (!strcmp(bf->familly, "MFVec3f")) {
				j = 0;
				CurrentLine = bf->def;
				while (GetNextToken(token, ",")) j++;
				j+=1;
				fprintf(f, "\tp->%s.vals = (SFVec3f *)malloc(sizeof(SFVec3f)*%d);\n", bf->name, j);
				fprintf(f, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(f, "\tp->%s.vals[%d].x = %s;\n", bf->name, j, GetFixedPrintout(tok));
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(f, "\tp->%s.vals[%d].y = %s;\n", bf->name, j, GetFixedPrintout(tok));
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(f, "\tp->%s.vals[%d].z = %s;\n", bf->name, j, GetFixedPrintout(tok));
					j+=1;
					CurrentLine = store;
				}
			}
			//MFVec4f
			else if (!strcmp(bf->familly, "MFVec4f") || !strcmp(bf->familly, "MFRotation")) {
				j = 0;
				CurrentLine = bf->def;
				while (GetNextToken(token, ",")) j++;
				j+=1;
				fprintf(f, "\tp->%s.vals = (GF_Vec4*)malloc(sizeof(GF_Vec4)*%d);\n", bf->name, j);
				fprintf(f, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(f, "\tp->%s.vals[%d].x = %s;\n", bf->name, j, GetFixedPrintout(tok));
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(f, "\tp->%s.vals[%d].y = %s;\n", bf->name, j, GetFixedPrintout(tok));
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(f, "\tp->%s.vals[%d].z = %s;\n", bf->name, j, GetFixedPrintout(tok));
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(f, "\tp->%s.vals[%d].q = %s;\n", bf->name, j, GetFixedPrintout(tok));
					j+=1;
					CurrentLine = store;
				}
			}
			//MFInt32
			else if (!strcmp(bf->familly, "MFInt32")) {
				j = 0;
				CurrentLine = bf->def;
				while (GetNextToken(token, ",")) j++;
				j+=1;
				fprintf(f, "\tp->%s.vals = (SFInt32*)malloc(sizeof(SFInt32)*%d);\n", bf->name, j);
				fprintf(f, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " ");
					fprintf(f, "\tp->%s.vals[%d] = %s;\n", bf->name, j, tok);
					j+=1;
					CurrentLine = store;
				}
			}
			//MFColor
			else if (!strcmp(bf->familly, "MFColor")) {
				j = 0;
				CurrentLine = bf->def;
				while (GetNextToken(token, ",")) j++;
				j+=1;
				fprintf(f, "\tp->%s.vals = (SFColor*)malloc(sizeof(SFColor)*%d);\n", bf->name, j);
				fprintf(f, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " ");
					fprintf(f, "\tp->%s.vals[%d].red = %s;\n", bf->name, j, GetFixedPrintout(tok));
					GetNextToken(tok, " ");
					fprintf(f, "\tp->%s.vals[%d].green = %s;\n", bf->name, j, GetFixedPrintout(tok));
					GetNextToken(tok, " ");
					fprintf(f, "\tp->%s.vals[%d].blue = %s;\n", bf->name, j, GetFixedPrintout(tok));
					j+=1;
					CurrentLine = store;
				}
			}
			//MFString
			else if (!strcmp(bf->familly, "MFString")) {
				j = 0;
				CurrentLine = bf->def;
				while (GetNextToken(token, ",")) j++;
				j+=1;
				fprintf(f, "\tp->%s.vals = (char**)malloc(sizeof(SFString)*%d);\n", bf->name, j);
				fprintf(f, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " \"");
					fprintf(f, "\tp->%s.vals[%d] = (char*)malloc(sizeof(char) * %d);\n", bf->name, j, strlen(tok)+1);
					fprintf(f, "\tstrcpy(p->%s.vals[%d], \"%s\");\n", bf->name, j, tok);
					j+=1;
					CurrentLine = store;
				}
			}
			//MFTime
			else if (!strcmp(bf->familly, "MFTime")) {
				j = 0;
				CurrentLine = bf->def;
				while (GetNextToken(token, ",")) j++;
				j+=1;
				fprintf(f, "\tp->%s.vals = (SFTime*)malloc(sizeof(SFTime)*%d);\n", bf->name, j);
				fprintf(f, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " \"");
					TranslateToken(tok);
					fprintf(f, "\tp->%s.vals[%d] = %s;\n", bf->name, j, tok);
					j+=1;
					CurrentLine = store;
				}
			}

			//other nodes
			else if (!strcmp(bf->familly, "SFImage")) {
				//we currently only have SFImage, with NO texture so do nothing
			}
			//unknown init (for debug)
			else {
				fprintf(f, "UNKNOWN FIELD (%s);\n", bf->familly);

			}
		}
		fprintf(f, "\treturn (GF_Node *)p;\n}\n\n");

	}

	fprintf(f, "\n\n\n");

	//creator function
	fprintf(f, "GF_Node *gf_sg_mpeg4_node_new(u32 NodeTag)\n{\n\tswitch (NodeTag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) {
			fprintf(f, "\tcase TAG_MPEG4_%s:\n\t\treturn %s_Create();\n", n->name, n->name);
		}
	}
	fprintf(f, "\tdefault:\n\t\treturn NULL;\n\t}\n}\n\n");

	fprintf(f, "const char *gf_sg_mpeg4_node_get_class_name(u32 NodeTag)\n{\n\tswitch (NodeTag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) fprintf(f, "\tcase TAG_MPEG4_%s:\n\t\treturn \"%s\";\n", n->name, n->name);
	}
	fprintf(f, "\tdefault:\n\t\treturn \"Unknown Node\";\n\t}\n}\n\n");

	fprintf(f, "void gf_sg_mpeg4_node_del(GF_Node *node)\n{\n\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) {
			fprintf(f, "\tcase TAG_MPEG4_%s:\n\t\t%s_Del(node); return;\n", n->name, n->name);
		}
	}
	fprintf(f, "\tdefault:\n\t\treturn;\n\t}\n}\n\n");

	fprintf(f, "u32 gf_sg_mpeg4_node_get_field_count(GF_Node *node, u8 code_mode)\n{\n\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) {
			fprintf(f, "\tcase TAG_MPEG4_%s:return %s_get_field_count(node, code_mode);\n", n->name, n->name);
		}
	}
	fprintf(f, "\tdefault:\n\t\treturn 0;\n\t}\n}\n\n");

	fprintf(f, "GF_Err gf_sg_mpeg4_node_get_field(GF_Node *node, GF_FieldInfo *field)\n{\n\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) {
			fprintf(f, "\tcase TAG_MPEG4_%s: return %s_get_field(node, field);\n", n->name, n->name);
		}
	}
	fprintf(f, "\tdefault:\n\t\treturn GF_BAD_PARAM;\n\t}\n}\n\n");

	fprintf(f, "GF_Err gf_sg_mpeg4_node_get_field_index(GF_Node *node, u32 inField, u8 code_mode, u32 *fieldIndex)\n{\n\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) {
			fprintf(f, "\tcase TAG_MPEG4_%s: return %s_get_field_index(node, inField, code_mode, fieldIndex);\n", n->name, n->name);
		}
	}
	fprintf(f, "\tdefault:\n\t\treturn GF_BAD_PARAM;\n\t}\n}\n\n");

	fprintf(f, "Bool gf_sg_mpeg4_node_get_aq_info(GF_Node *node, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)\n{\n\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) {
			fprintf(f, "\tcase TAG_MPEG4_%s: return %s_get_aq_info(node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);\n", n->name, n->name);
		}
	}
	fprintf(f, "\tdefault:\n\t\treturn 0;\n\t}\n}\n\n");

	fprintf(f, "u32 gf_sg_mpeg4_node_get_child_ndt(GF_Node *node)\n{\n\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl && strlen(n->Child_NDT_Name) ) {
			fprintf(f, "\tcase TAG_MPEG4_%s: return %s;\n", n->name, n->Child_NDT_Name);
		}
	}
	fprintf(f, "\tdefault:\n\t\treturn 0;\n\t}\n}\n\n");


	fprintf(f, "\nu32 gf_node_mpeg4_type_by_class_name(const char *node_name)\n{\n\tif(!node_name) return 0;\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (n->skip_impl) continue;
		fprintf(f, "\tif (!strcmp(node_name, \"%s\")) return TAG_MPEG4_%s;\n", n->name, n->name);
	}
	fprintf(f, "\treturn 0;\n}\n\n");

	fprintf(f, "s32 gf_sg_mpeg4_node_get_field_index_by_name(GF_Node *node, char *name)\n{\n\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) {
			fprintf(f, "\tcase TAG_MPEG4_%s: return %s_get_field_index_by_name(name);\n", n->name, n->name);
		}
	}
	fprintf(f, "\tdefault:\n\t\treturn -1;\n\t}\n}\n\n");


	EndFile(f, "", 1);
}

void ParseTemplateFile(FILE *nodes, GF_List *BNodes, GF_List *NDTs, u32 version)
{
	char sLine[2000];
	char token[100];
	char *p;
	BNode *n;
	BField *f;
	u32 j, i, k;

	//get lines one by one
	n = NULL;
	while (!feof(nodes)) {
		fgets(sLine, 2000, nodes);
		//skip comment and empty lines
		if (sLine[0] == '#') continue;
		if (sLine[0] == '\n') continue;

		CurrentLine = sLine;
		
		//parse the line till end of line
		while (GetNextToken(token, " \t")) {

			//this is a new node
			if (!strcmp(token, "PROTO") ) {
				n = BlankNode();
				n->version = version;
				gf_list_add(BNodes, n);

				//get its name
				GetNextToken(n->name, " \t[");
				if (!strcmp(n->name, "TimeSensor")) {
					n = n;
				}

				//extract the NDTs
				GetNextToken(token, "\t[ %#=");
				if (strcmp(token, "NDT")) {
					printf("Corrupted template file\n");
					return;
				}
				while (1) {
					GetNextToken(token, "=, \t");
					//done with NDTs
					if (token[0] == '%') break;

					//update the NDT list
					CheckInTable(token, NDTs);
					p = malloc(strlen(token)+1);
					strcpy(p, token);
					gf_list_add(n->NDT, p);
				}

				//extract the coding type
				if (strcmp(token, "%COD")) {
					printf("Corrupted template file\n");
					return;
				} else {
					GetNextToken(token, "= ");
					if (token[0] == 'N') {
						n->codingType = 0;
					} else {
						n->codingType = 1;
					}
				}
			}
			//this is NOT a field
			else if (token[0] == ']' || token[0] == '{' || token[0] == '}' ) {
				break;
			}
			//parse a field
			else {
				if (!n) {
					printf("Corrupted template file\n");
					return;
				}
				f = BlankField();
				gf_list_add(n->Fields, f);
				
				//get the field type
				strcpy(f->type, token);
				GetNextToken(f->familly, " \t");
				GetNextToken(f->name, " \t");
				//fix for our own code :(
				if (!strcmp(f->name, "tag")) strcpy(f->name, "_tag");

				//has default
				skip_sep(" \t");
				if (GetNextToken(token, "#\t")) {
					j=0;
					while (token[j] == ' ') j+=1;
					if (token[j] == '[') j+=1;
					if (token[j] == '"') j+=1;

					if (token[j] != '"' && token[j] != ']') {
						strcpy(f->def, token+j);
						j=1;
						while (j) {
							switch (f->def[strlen(f->def)-1]) {
							case ' ':
							case '"':
							case ']':
								f->def[strlen(f->def)-1] = 0;
								break;
							default:
								j=0;
								break;
							}
						}
					} else {
						strcpy(f->def, "");
					}
					if (!strcmp(f->familly, "SFFloat")) {
						if (!strcmp(f->def, "+I") || !strcmp(f->def, "I")) {
							strcpy(f->def, "FIX_MAX");
						} else if (!strcmp(f->def, "-I")) {
							strcpy(f->def, "FIX_MIN");
						}
					} else if (!strcmp(f->familly, "SFTime")) {
						if (!strcmp(f->def, "+I") || !strcmp(f->def, "I")) {
							strcpy(f->def, "FIX_MAX");
						} else if (!strcmp(f->def, "-I")) {
							strcpy(f->def, "FIX_MIN");
						}
					} else if (!strcmp(f->familly, "SFInt32")) {
						if (!strcmp(f->def, "+I") || !strcmp(f->def, "I")) {
							strcpy(f->def, "2 << 31");
						} else if (!strcmp(f->def, "-I")) {
							strcpy(f->def, "- (2 << 31)");
						}
					}
				}
				//has other
				while (GetNextToken(token, " \t#%=")) {
					switch (token[0]) {
					//bounds
					case 'b':
						f->hasBounds = 1;
						GetNextToken(f->b_min, "[(,");
						GetNextToken(f->b_max, ")]");
						break;
					case 'q':
						f->hasQuant = 1;
						GetNextToken(f->quant_type, " \t");
						if (!strcmp(f->quant_type, "13"))
							GetNextToken(f->qt13_bits, " \t");
						break;
					case 'a':
						f->hasAnim = 1;
						GetNextToken(token, " \t");
						f->AnimType = atoi(token);
						break;
					default:
						break;
					}
				}
			}
		}
	}


	for (k=0; k<gf_list_count(BNodes); k++) {
		n = gf_list_get(BNodes, k);

		for (i=0; i<gf_list_count(n->Fields); i++) {
			f = gf_list_get(n->Fields, i);
			//nothing on events
			if (!strcmp(f->type, "eventIn")) continue;
			if (!strcmp(f->type, "eventOut")) continue;
			if (!strcmp(f->def, "")) continue;
			if (strstr(f->familly, "Node")) continue;
			n->hasDefault = 1;
		}
	}
}


void WriteNodeDump(FILE *f, BNode *n)
{
	BField *bf;
	u32 i;

	fprintf(f, "static const char *%s_FieldName[] = {\n", n->name);
	for (i=0; i<gf_list_count(n->Fields); i++) {
		bf = gf_list_get(n->Fields, i);
		if (!i) {
			fprintf(f, " \"%s\"", bf->name);
		} else {
			fprintf(f, ", \"%s\"", bf->name);
		}
	}
	fprintf(f, "\n};\n\n");
}

void parse_profile(GF_List *nodes, FILE *prof)
{
	char sLine[2000];
	BNode *n;
	Bool found;
	u32 i;

	while (!feof(prof)) {
		fgets(sLine, 2000, prof);
		//skip comment and empty lines
		if (sLine[0] == '#') continue;
		if (sLine[0] == '\n') continue;
		if (strstr(sLine, "Proximity")) 
			found = 0;
		found = 1;
		while (found) {
			switch (sLine[strlen(sLine)-1]) {
			case '\n':
			case '\r':
			case ' ':
				sLine[strlen(sLine)-1] = 0;
				break;
			default:
				found = 0;
				break;
			}
		}

//		if (0 && !stricmp(sLine, "Appearance") || !stricmp(sLine, "Shape") || !stricmp(sLine, "Sound2D") ) {
		if (0) {
			printf("Warning: cannot disable node %s (required in all BIFS profiles)\n", sLine);
		} else {
			found = 0;
			for (i=0; i<gf_list_count(nodes); i++) {
				n = gf_list_get(nodes, i);
				if (!stricmp(n->name, sLine)) {
					n->skip_impl = 1;
					found = 1;
					break;
				}
			}
			if (!found) printf("cannot disable %s: node not found\n", sLine);
		}
	}
}

int main (int argc, char **argv)
{
	FILE *nodes, *ndt_c, *ndt_h, *fskip;
	GF_List *BNodes, *NDTs;
	u32 i, j, nbVersion;
	BNode *n;
	BField *bf;

	if (argc < 2) {
		PrintUsage();
		return 0;
	}

	BNodes = gf_list_new();
	NDTs = gf_list_new();



	fskip = NULL;
	i=1;
	if (argv[i][0]=='-') {
		fskip = fopen(argv[i+1], "rt");
		if (!fskip) {
			printf("file %s not found\n", argv[i+1]);
			return 0;
		}
		i+=2;
	}
	nbVersion = 1;
	for (; i<(u32)argc; i++) {
		nodes = fopen(argv[i], "rt");
		//all nodes are in the same list but we keep version info
		ParseTemplateFile(nodes, BNodes, NDTs, nbVersion);
		

		//special case for viewport: it is present in V1 but empty
		if (nbVersion==1) CheckInTable("SFViewportNode", NDTs);
		nbVersion++;
		fclose(nodes);
	}
	nbVersion--;
	printf("BIFS tables parsed: %d versions\n", nbVersion);
	
	if (fskip) {
		parse_profile(BNodes, fskip);
		fclose(fskip);
	}

	//write the nodes def
	WriteNodesFile(BNodes, NDTs, nbVersion);
	//write all nodes init stuff
	WriteNodeCode(BNodes);

	//write all NDTs
	ndt_h = BeginFile("NDT", 0);
	ndt_c = BeginFile("NDT", 1);

	fprintf(ndt_h, "#include <gpac/nodes_mpeg4.h>\n\n");
	fprintf(ndt_c, "\n\n#include <gpac/internal/bifs_tables.h>\n");
	
	//prepare the encoding file
	fprintf(ndt_h, "\n\nu32 ALL_GetNodeType(const u32 *table, const u32 count, u32 NodeTag, u32 Version);\n\n");
	fprintf(ndt_c, "\n\nu32 ALL_GetNodeType(const u32 *table, const u32 count, u32 NodeTag, u32 Version)\n{\n\tu32 i = 0;");
	fprintf(ndt_c, "\n\twhile (i<count) {\n\t\tif (table[i] == NodeTag) goto found;\n\t\ti++;\n\t}\n\treturn 0;\nfound:\n\tif (Version == 2) return i+2;\n\treturn i+1;\n}\n\n");

	//write the NDT
	for (i=0; i<nbVersion; i++) {
		//write header
		WriteNDT_H(ndt_h, BNodes, NDTs, i+1);
		//write decoding code
		WriteNDT_Dec(ndt_c, BNodes, NDTs, i+1);
		//write encoding code
		WriteNDT_Enc(ndt_c, BNodes, NDTs, i+1);
	}



	fprintf(ndt_c, "\n\nu32 gf_bifs_ndt_get_node_type(u32 NDT_Tag, u32 NodeType, u32 Version)\n{\n\tswitch (Version) {\n");
	for (i=0; i<nbVersion; i++) {
		fprintf(ndt_c, "\tcase GF_BIFS_V%d:\n\t\treturn NDT_V%d_GetNodeTag(NDT_Tag, NodeType);\n", i+1, i+1);
	}
	fprintf(ndt_c, "\tdefault:\n\t\treturn 0;\n\t}\n}");

	fprintf(ndt_c, "\n\nu32 gf_bifs_get_ndt_bits(u32 NDT_Tag, u32 Version)\n{\n\tswitch (Version) {\n");
	for (i=0; i<nbVersion; i++) {
		fprintf(ndt_c, "\tcase GF_BIFS_V%d:\n\t\treturn NDT_V%d_GetNumBits(NDT_Tag);\n", i+1, i+1);
	}
	fprintf(ndt_c, "\tdefault:\n\t\treturn 0;\n\t}\n}");

	fprintf(ndt_c, "\n\nu32 gf_bifs_get_node_type(u32 NDT_Tag, u32 NodeTag, u32 Version)\n{\n\tswitch (Version) {\n");
	for (i=0; i<nbVersion; i++) {
		fprintf(ndt_c, "\tcase GF_BIFS_V%d:\n\t\treturn NDT_V%d_GetNodeType(NDT_Tag, NodeTag);\n", i+1, i+1);
	}
	fprintf(ndt_c, "\tdefault:\n\t\treturn 0;\n\t}\n}");	

	fprintf(ndt_h, "\nu32 NDT_GetChildTable(u32 NodeTag);\n");
	fprintf(ndt_h, "\n\n");
	
	//NDT checking
	fprintf(ndt_c, "u32 GetChildrenNDT(GF_Node *node)\n{\n\tif (!node) return 0;\n\tswitch (gf_node_get_tag(node)) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (n->skip_impl) continue;
		for (j=0; j<gf_list_count(n->Fields); j++) {
			bf = gf_list_get(n->Fields, j);
			if (!strcmp(bf->name, "children")) {
				fprintf(ndt_c, "\tcase TAG_MPEG4_%s:\n\t\treturn NDT_SF%s;\n", n->name, bf->familly+2);
				break;
			}
		}
	}
	fprintf(ndt_c, "\tdefault:\n\t\treturn 0;\n\t}\n}\n\n");

	EndFile(ndt_h, "NDT", 0);
	EndFile(ndt_c, "", 1);
	
	//free NDTs
	while (gf_list_count(NDTs)) {
		char *tmp = gf_list_get(NDTs, 0);
		free(tmp);
		gf_list_rem(NDTs, 0);
	}
	gf_list_del(NDTs);
	//free nodes
	while (gf_list_count(BNodes)) {
		n = gf_list_get(BNodes, 0);
		gf_list_rem(BNodes, 0);
		while (gf_list_count(n->NDT)) {
			char *tmp = gf_list_get(n->NDT, 0);
			free(tmp);
			gf_list_rem(n->NDT, 0);
		}
		gf_list_del(n->NDT);
		while (gf_list_count(n->Fields)) {
			bf = gf_list_get(n->Fields, 0);
			free(bf);
			gf_list_rem(n->Fields, 0);
		}
		gf_list_del(n->Fields);
		free(n);
	}
	gf_list_del(BNodes);

	return 0;
}

