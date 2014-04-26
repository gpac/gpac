/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / X3D Scene Graph Generator sub-project
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

#define COPYRIGHT "/*\n *			GPAC - Multimedia Framework C SDK\n *\n *			Authors: Jean Le Feuvre\n *			Copyright (c) Telecom ParisTech 2000-2012\n *					All rights reserved\n *\n *  This file is part of GPAC / X3D Scene Graph sub-project\n *\n *  GPAC is free software; you can redistribute it and/or modify\n *  it under the terms of the GNU Lesser General Public License as published by\n *  the Free Software Foundation; either version 2, or (at your option)\n *  any later version.\n *\n *  GPAC is distributed in the hope that it will be useful,\n *  but WITHOUT ANY WARRANTY; without even the implied warranty of\n *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n *  GNU Lesser General Public License for more details.	\n *\n *  You should have received a copy of the GNU Lesser General Public\n *  License along with this library; see the file COPYING.  If not, write to\n *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.\n *\n */\n"

static char *CurrentLine;

void PrintUsage()
{
	printf("X3DGen [skip_file]\n"
	       "\nGPAC X3D Scene Graph generator\n"
	       "\n"
	       "\nskip_file: txt file with list of nodes to leave unimplemented"
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
} X3DField;

//NDTs

//a BIFS node
typedef struct
{
	char name[1000];
	//NDT info. NDT are created in alphabetical order
	GF_List *NDT;
	GF_List *Fields;
	u8 hasDefault;
	char Child_NDT_Name[1000];
	u8 skip_impl;

} X3DNode;


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

X3DField *BlankField()
{
	X3DField *n = gf_malloc(sizeof(X3DField));
	memset(n, 0, sizeof(X3DField));
	return n;
}


X3DNode *BlankNode()
{
	X3DNode *n = gf_malloc(sizeof(X3DNode));
	memset(n, 0, sizeof(X3DNode));
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
	p = gf_malloc(strlen(token)+1);
	strcpy(p, token);
	gf_list_add(NDTs, p);
}

/*type: 0: header, 1: source*/
FILE *BeginFile(u32 type)
{
	FILE *f;

	char sPath[GF_MAX_PATH];

	if (!type) {
		sprintf(sPath, "..%c..%c..%cinclude%cgpac%cnodes_x3d.h", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
	} else {
		sprintf(sPath, "..%c..%c..%csrc%cscenegraph%cx3d_nodes.c", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
	}

	f = fopen(sPath, "wt");
	fprintf(f, "%s\n", COPYRIGHT);

	{
		time_t rawtime;
		time(&rawtime);
		fprintf(f, "\n/*\n\tDO NOT MOFIFY - File generated on GMT %s\n\tBY X3DGen for GPAC Version %s\n*/\n\n", asctime(gmtime(&rawtime)), GPAC_VERSION);
	}

	if (!type) {
		fprintf(f, "#ifndef _GF_X3D_NODES_H\n");
		fprintf(f, "#define _GF_X3D_NODES_H\n\n");
		fprintf(f, "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n");
	}
	return f;
}

void EndFile(FILE *f, u32 type)
{
	if (!type) {
		fprintf(f, "#ifdef __cplusplus\n}\n#endif\n\n");
		fprintf(f, "\n\n#endif\t\t/*_GF_X3D_NODES_H*/\n\n");
	}
	fclose(f);
}

void TranslateToken(char *token)
{
	if (!strcmp(token, "+I") || !strcmp(token, "I")) {
		strcpy(token, "GF_MAX_FLOAT");
	}
	else if (!strcmp(token, "-I")) {
		strcpy(token, "GF_MIN_FLOAT");
	}
}



void WriteNodesFile(GF_List *BNodes, GF_List *NDTs)
{
	FILE *f;
	u32 i, j;
	X3DNode *n;
	X3DField *bf;
	f = BeginFile(0);

	fprintf(f, "#include <gpac/scenegraph_vrml.h>\n\n");
	fprintf(f, "#ifndef GPAC_DISABLE_X3D\n\n");

	//write all tags
	fprintf(f, "\n\nenum {\n");

	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (i)
			fprintf(f, ",\n\tTAG_X3D_%s", n->name);
		else
			fprintf(f, "\tTAG_X3D_%s = GF_NODE_RANGE_FIRST_X3D", n->name);
	}
	fprintf(f, ",\n\tTAG_LastImplementedX3D\n};\n\n");

	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (n->skip_impl) continue;
		fprintf(f, "typedef struct _tagX3D%s\n{\n", n->name);
		fprintf(f, "\tBASE_NODE\n");

		/*write children field*/
		for (j=0; j<gf_list_count(n->Fields); j++) {
			bf = gf_list_get(n->Fields, j);
			if (!stricmp(bf->name, "addChildren") || !strcmp(bf->name, "removeChildren")) continue;
			if (strcmp(bf->type, "eventOut") && !strcmp(bf->name, "children")) {
				fprintf(f, "\tVRML_CHILDREN\n");
				break;
			}
		}
		for (j=0; j<gf_list_count(n->Fields); j++) {
			bf = gf_list_get(n->Fields, j);

			if (!strcmp(bf->name, "addChildren") || !strcmp(bf->name, "removeChildren")) continue;
			if (strcmp(bf->type, "eventOut") && !strcmp(bf->name, "children")) continue;

			if (strstr(bf->familly, "Node")) {
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
			if (!strcmp(bf->type, "eventIn"))
				fprintf(f, "\tvoid (*on_%s)(GF_Node *pThis, struct _route *route);\t/*eventInHandler*/\n", bf->name);
		}
		fprintf(f, "} X_%s;\n\n\n", n->name);
	}

	fprintf(f, "#endif /*GPAC_DISABLE_X3D*/\n\n");
	EndFile(f, 0);

}

void WriteNodeFields(FILE *f, X3DNode *n)
{
	u32 i;
	X3DField *bf;

	fprintf(f, "\nstatic u32 %s_get_field_count(GF_Node *node, u8 dummy)\n{\n\treturn %d;\n}\n\n", n->name, gf_list_count(n->Fields));
	fprintf(f, "static GF_Err %s_get_field(GF_Node *node, GF_FieldInfo *info)\n{\n\tswitch (info->fieldIndex) {\n", n->name);
	for (i=0; i<gf_list_count(n->Fields); i++) {
		bf = gf_list_get(n->Fields, i);

		fprintf(f, "\tcase %d:\n", i);

		fprintf(f, "\t\tinfo->name = \"%s\";\n", bf->name);

		//skip all eventIn
		if (!strcmp(bf->type, "eventIn")) {
			fprintf(f, "\t\tinfo->eventType = GF_SG_EVENT_IN;\n");
			fprintf(f, "\t\tinfo->on_event_in = ((X_%s *)node)->on_%s;\n", n->name, bf->name);
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
			fprintf(f, "\t\tinfo->far_ptr = & ((X_%s *)node)->%s;\n", n->name, bf->name);
		} else {
			char szName[20];
			strcpy(szName, bf->familly);
			strupr(szName);
			//no ext type
			fprintf(f, "\t\tinfo->fieldType = GF_SG_VRML_%s;\n", szName);
			fprintf(f, "\t\tinfo->far_ptr = & ((X_%s *) node)->%s;\n", n->name, bf->name);
		}
		fprintf(f, "\t\treturn GF_OK;\n");
	}
	fprintf(f, "\tdefault:\n\t\treturn GF_BAD_PARAM;\n\t}\n}\n\n");

	fprintf(f, "\nstatic s32 %s_get_field_index_by_name(char *name)\n{\n", n->name);
	for (i=0; i<gf_list_count(n->Fields); i++) {
		bf = gf_list_get(n->Fields, i);
		fprintf(f, "\tif (!strcmp(\"%s\", name)) return %d;\n", bf->name, i);
	}
	fprintf(f, "\treturn -1;\n\t}\n");
}

void WriteNodeCode(GF_List *BNodes, FILE *vrml_code)
{
	char token[20], tok[20];
	char *store;
	u32 i, j, k, go;
	X3DField *bf;
	X3DNode *n;

	fprintf(vrml_code, "\n#include <gpac/nodes_x3d.h>\n");
	fprintf(vrml_code, "\n#include <gpac/internal/scenegraph_dev.h>\n");
	fprintf(vrml_code, "\n/*for NDT tag definitions*/\n#include <gpac/nodes_mpeg4.h>\n");
	fprintf(vrml_code, "#ifndef GPAC_DISABLE_X3D\n\n");

	for (k=0; k<gf_list_count(BNodes); k++) {
		Bool is_parent = 0;
		n = gf_list_get(BNodes, k);
		if (n->skip_impl) continue;
		fprintf(vrml_code, "\n/*\n\t%s Node deletion\n*/\n\n", n->name);
		fprintf(vrml_code, "static void %s_Del(GF_Node *node)\n{\n\tX_%s *p = (X_%s *) node;\n", n->name, n->name, n->name);

		for (i=0; i<gf_list_count(n->Fields); i++) {
			bf = gf_list_get(n->Fields, i);
			//nothing on child events
			if (!strcmp(bf->name, "addChildren")) continue;
			if (!strcmp(bf->name, "removeChildren")) continue;

			//delete all children node
			if (strcmp(bf->type, "eventOut") && !strcmp(bf->name, "children")) {
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
				fprintf(vrml_code, "\tgf_sg_%s_del(p->%s);\n", szName, bf->name);
			} else if (strstr(bf->familly, "Node")) {
				//this is a POINTER to a node
				if (strstr(bf->familly, "SF")) {
					fprintf(vrml_code, "\tgf_node_unregister((GF_Node *) p->%s, node);\t\n", bf->name);
				} else {
					//this is a POINTER to a chain
					fprintf(vrml_code, "\tgf_node_unregister_children(node, p->%s);\t\n", bf->name);
				}
			}
		}
		if (is_parent)
			fprintf(vrml_code, "\tgf_sg_vrml_parent_destroy(node);\t\n");
		/*avoids gcc warnings in case no field to delete*/
		fprintf(vrml_code, "\tgf_node_free((GF_Node *)p);\n}\n\n");

		//node fields
		WriteNodeFields(vrml_code, n);

		//
		//		Constructor
		//

		fprintf(vrml_code, "\n\nstatic GF_Node *%s_Create()\n{\n\tX_%s *p;\n\tGF_SAFEALLOC(p, X_%s);\n", n->name, n->name, n->name);
		fprintf(vrml_code, "\tif(!p) return NULL;\n");
		fprintf(vrml_code, "\tgf_node_setup((GF_Node *)p, TAG_X3D_%s);\n", n->name);

		for (i=0; i<gf_list_count(n->Fields); i++) {
			bf = gf_list_get(n->Fields, i);
			//setup all children node
			if (strcmp(bf->type, "eventOut") && !strcmp(bf->name, "children")) {
				fprintf(vrml_code, "\tgf_sg_vrml_parent_setup((GF_Node *) p);\n");
				break;
			}
			else if ( strstr(bf->familly, "Node") && strncmp(bf->type, "event", 5) ) {
				//this is a POINTER to a node
				if (strstr(bf->familly, "MF")) {
					//this is a POINTER to a chain
					//fprintf(vrml_code, "\tp->%s = gf_list_new();\t\n", bf->name);
				}
			}
			/*special case for SFCommandBuffer: we also create a command list*/
			if (!stricmp(bf->familly, "SFCommandBuffer")) {
				fprintf(vrml_code, "\tp->%s.commandList = gf_list_new();\t\n", bf->name);
			}
		}

		fprintf(vrml_code, "\n\t/*default field values*/\n");

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
					fprintf(vrml_code, "\tp->%s = 1;\n", bf->name);
			}
			//SFFloat
			else if (!strcmp(bf->familly, "SFFloat")) {
				fprintf(vrml_code, "\tp->%s = FLT2FIX(%s);\n", bf->name, bf->def);
			}
			//SFDouble
			else if (!strcmp(bf->familly, "SFDouble")) {
				fprintf(vrml_code, "\tp->%s = (SFDouble) %s;\n", bf->name, bf->def);
			}
			//SFTime
			else if (!strcmp(bf->familly, "SFTime")) {
				fprintf(vrml_code, "\tp->%s = %s;\n", bf->name, bf->def);
			}
			//SFInt32
			else if (!strcmp(bf->familly, "SFInt32")) {
				fprintf(vrml_code, "\tp->%s = %s;\n", bf->name, bf->def);
			}
			//SFColor
			else if (!strcmp(bf->familly, "SFColor")) {
				CurrentLine = bf->def;
				GetNextToken(token, " ");
				TranslateToken(token);

				fprintf(vrml_code, "\tp->%s.red = FLT2FIX(%s);\n", bf->name, token);
				GetNextToken(token, " ");
				fprintf(vrml_code, "\tp->%s.green = FLT2FIX(%s);\n", bf->name, token);
				GetNextToken(token, " ");
				fprintf(vrml_code, "\tp->%s.blue = FLT2FIX(%s);\n", bf->name, token);
			}
			//SFVec2f
			else if (!strcmp(bf->familly, "SFVec2f")) {
				CurrentLine = bf->def;
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(vrml_code, "\tp->%s.x = FLT2FIX(%s);\n", bf->name, token);
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(vrml_code, "\tp->%s.y = FLT2FIX(%s);\n", bf->name, token);
			}
			//SFVec2d
			else if (!strcmp(bf->familly, "SFVec2d")) {
				CurrentLine = bf->def;
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(vrml_code, "\tp->%s.x = (SFDouble) %s;\n", bf->name, token);
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(vrml_code, "\tp->%s.y = (SFDouble) %s;\n", bf->name, token);
			}
			//SFVec3f
			else if (!strcmp(bf->familly, "SFVec3f")) {
				CurrentLine = bf->def;
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(vrml_code, "\tp->%s.x = FLT2FIX(%s);\n", bf->name, token);
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(vrml_code, "\tp->%s.y = FLT2FIX(%s);\n", bf->name, token);
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(vrml_code, "\tp->%s.z = FLT2FIX(%s);\n", bf->name, token);
			}
			//SFVec3d
			else if (!strcmp(bf->familly, "SFVec3d")) {
				CurrentLine = bf->def;
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(vrml_code, "\tp->%s.x = (SFDouble) %s;\n", bf->name, token);
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(vrml_code, "\tp->%s.y = (SFDouble) %s;\n", bf->name, token);
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(vrml_code, "\tp->%s.z = (SFDouble) %s;\n", bf->name, token);
			}
			//SFVec4f & SFRotation
			else if (!strcmp(bf->familly, "SFVec4f") || !strcmp(bf->familly, "SFRotation")) {
				CurrentLine = bf->def;
				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(vrml_code, "\tp->%s.x = FLT2FIX(%s);\n", bf->name, token);

				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(vrml_code, "\tp->%s.y = FLT2FIX(%s);\n", bf->name, token);

				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(vrml_code, "\tp->%s.z = FLT2FIX(%s);\n", bf->name, token);

				GetNextToken(token, " ");
				TranslateToken(token);
				fprintf(vrml_code, "\tp->%s.q = FLT2FIX(%s);\n", bf->name, token);
			}
			//SFString
			else if (!strcmp(bf->familly, "SFString")) {
				fprintf(vrml_code, "\tp->%s.buffer = (char*) gf_malloc(sizeof(char) * %d);\n", bf->name, strlen(bf->def)+1);
				fprintf(vrml_code, "\tstrcpy(p->%s.buffer, \"%s\");\n", bf->name, bf->def);
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
				fprintf(vrml_code, "\tp->%s.vals = (SFFloat *)gf_malloc(sizeof(SFFloat)*%d);\n", bf->name, j);
				fprintf(vrml_code, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, " ,")) go = 0;
					TranslateToken(token);
					fprintf(vrml_code, "\tp->%s.vals[%d] = FLT2FIX(%s);\n", bf->name, j, token);
					j+=1;
				}
			}
			//MFDouble
			else if (!strcmp(bf->familly, "MFDouble")) {
				j = 0;
				CurrentLine = bf->def;
				while (GetNextToken(token, " ,")) j++;
				j+=1;
				fprintf(vrml_code, "\tp->%s.vals = (SFFloat*)gf_malloc(sizeof(SFFloat)*%d);\n", bf->name, j);
				fprintf(vrml_code, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, " ,")) go = 0;
					TranslateToken(token);
					fprintf(vrml_code, "\tp->%s.vals[%d] = (SFDouble) %s;\n", bf->name, j, token);
					j+=1;
				}
			}
			//MFVec2f
			else if (!strcmp(bf->familly, "MFVec2f")) {
				j = 0;
				CurrentLine = bf->def;
				while (GetNextToken(token, ",")) j++;
				j+=1;
				fprintf(vrml_code, "\tp->%s.vals = (SFVec2f*) gf_malloc(sizeof(SFVec2f)*%d);\n", bf->name, j);
				fprintf(vrml_code, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d].x = FLT2FIX(%s);\n", bf->name, j, tok);
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d].y = FLT2FIX(%s);\n", bf->name, j, tok);
					j+=1;
					CurrentLine = store;
				}
			}
			//MFVec2d
			else if (!strcmp(bf->familly, "MFVec2d")) {
				j = 0;
				CurrentLine = bf->def;
				while (GetNextToken(token, ",")) j++;
				j+=1;
				fprintf(vrml_code, "\tp->%s.vals = (SFVec2f*)gf_malloc(sizeof(SFVec2f)*%d);\n", bf->name, j);
				fprintf(vrml_code, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d].x = (SFDouble) %s;\n", bf->name, j, tok);
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d].y = (SFDouble) %s;\n", bf->name, j, tok);
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
				fprintf(vrml_code, "\tp->%s.vals = (SFVec3f*)gf_malloc(sizeof(SFVec3f)*%d);\n", bf->name, j);
				fprintf(vrml_code, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d].x = FLT2FIX(%s);\n", bf->name, j, tok);
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d].y = FLT2FIX(%s);\n", bf->name, j, tok);
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d].z = FLT2FIX(%s);\n", bf->name, j, tok);
					j+=1;
					CurrentLine = store;
				}
			}
			//MFVec3d
			else if (!strcmp(bf->familly, "MFVec3d")) {
				j = 0;
				CurrentLine = bf->def;
				while (GetNextToken(token, ",")) j++;
				j+=1;
				fprintf(vrml_code, "\tp->%s.vals = (SFVec2f*)gf_malloc(sizeof(SFVec3f)*%d);\n", bf->name, j);
				fprintf(vrml_code, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d].x = (SFDouble) %s;\n", bf->name, j, tok);
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d].y = (SFDouble) %s;\n", bf->name, j, tok);
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d].z = (SFDouble) %s;\n", bf->name, j, tok);
					j+=1;
					CurrentLine = store;
				}
			}
			//MFVec4f & MFRotation
			else if (!strcmp(bf->familly, "MFVec4f") || !strcmp(bf->familly, "MFRotation")) {
				j = 0;
				CurrentLine = bf->def;
				while (GetNextToken(token, ",")) j++;
				j+=1;
				fprintf(vrml_code, "\tp->%s.vals = (GF_Vec4*)gf_malloc(sizeof(GF_Vec4)*%d);\n", bf->name, j);
				fprintf(vrml_code, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d].x = FLT2FIX(%s);\n", bf->name, j, tok);
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d].y = FLT2FIX(%s);\n", bf->name, j, tok);
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d].z = FLT2FIX(%s);\n", bf->name, j, tok);
					GetNextToken(tok, " ");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d].q = FLT2FIX(%s);\n", bf->name, j, tok);
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
				fprintf(vrml_code, "\tp->%s.vals = (SFInt32*)gf_malloc(sizeof(SFInt32)*%d);\n", bf->name, j);
				fprintf(vrml_code, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " ");
					fprintf(vrml_code, "\tp->%s.vals[%d] = %s;\n", bf->name, j, tok);
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
				fprintf(vrml_code, "\tp->%s.vals = (SFColor*)gf_malloc(sizeof(SFColor)*%d);\n", bf->name, j);
				fprintf(vrml_code, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " ");
					fprintf(vrml_code, "\tp->%s.vals[%d].red = FLT2FIX(%s);\n", bf->name, j, tok);
					GetNextToken(tok, " ");
					fprintf(vrml_code, "\tp->%s.vals[%d].green = FLT2FIX(%s);\n", bf->name, j, tok);
					GetNextToken(tok, " ");
					fprintf(vrml_code, "\tp->%s.vals[%d].blue = FLT2FIX(%s);\n", bf->name, j, tok);
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
				fprintf(vrml_code, "\tp->%s.vals = (char**)gf_malloc(sizeof(SFString)*%d);\n", bf->name, j);
				fprintf(vrml_code, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " \"");
					fprintf(vrml_code, "\tp->%s.vals[%d] = (char*)gf_malloc(sizeof(char) * %d);\n", bf->name, j, strlen(tok)+1);
					fprintf(vrml_code, "\tstrcpy(p->%s.vals[%d], \"%s\");\n", bf->name, j, tok);
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
				fprintf(vrml_code, "\tp->%s.vals = (SFTime*)gf_malloc(sizeof(SFTime)*%d);\n", bf->name, j);
				fprintf(vrml_code, "\tp->%s.count = %d;\n", bf->name, j);
				j = 0;
				go = 1;
				CurrentLine = bf->def;
				while (go) {
					if (!GetNextToken(token, ",")) go = 0;
					store = CurrentLine;
					CurrentLine = token;
					GetNextToken(tok, " \"");
					TranslateToken(tok);
					fprintf(vrml_code, "\tp->%s.vals[%d] = %s;\n", bf->name, j, tok);
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
				fprintf(vrml_code, "UNKNOWN FIELD (%s);\n", bf->familly);

			}
		}
		fprintf(vrml_code, "\treturn (GF_Node *)p;\n}\n\n");

	}

	fprintf(vrml_code, "\n\n\n");

	//creator function
	fprintf(vrml_code, "GF_Node *gf_sg_x3d_node_new(u32 NodeTag)\n{\n\tswitch (NodeTag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) fprintf(vrml_code, "\tcase TAG_X3D_%s:\n\t\treturn %s_Create();\n", n->name, n->name);
	}
	fprintf(vrml_code, "\tdefault:\n\t\treturn NULL;\n\t}\n}\n\n");

	fprintf(vrml_code, "const char *gf_sg_x3d_node_get_class_name(u32 NodeTag)\n{\n\tswitch (NodeTag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) fprintf(vrml_code, "\tcase TAG_X3D_%s:\n\t\treturn \"%s\";\n", n->name, n->name);
	}
	fprintf(vrml_code, "\tdefault:\n\t\treturn \"Unknown Node\";\n\t}\n}\n\n");

	fprintf(vrml_code, "void gf_sg_x3d_node_del(GF_Node *node)\n{\n\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) fprintf(vrml_code, "\tcase TAG_X3D_%s:\n\t\t%s_Del(node); return;\n", n->name, n->name);
	}
	fprintf(vrml_code, "\tdefault:\n\t\treturn;\n\t}\n}\n\n");

	fprintf(vrml_code, "u32 gf_sg_x3d_node_get_field_count(GF_Node *node)\n{\n\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) fprintf(vrml_code, "\tcase TAG_X3D_%s:return %s_get_field_count(node, 0);\n", n->name, n->name);
	}
	fprintf(vrml_code, "\tdefault:\n\t\treturn 0;\n\t}\n}\n\n");

	fprintf(vrml_code, "GF_Err gf_sg_x3d_node_get_field(GF_Node *node, GF_FieldInfo *field)\n{\n\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) fprintf(vrml_code, "\tcase TAG_X3D_%s: return %s_get_field(node, field);\n", n->name, n->name);
	}
	fprintf(vrml_code, "\tdefault:\n\t\treturn GF_BAD_PARAM;\n\t}\n}\n\n");

	fprintf(vrml_code, "\nu32 gf_node_x3d_type_by_class_name(const char *node_name)\n{\n\tif(!node_name) return 0;\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) fprintf(vrml_code, "\tif (!strcmp(node_name, \"%s\")) return TAG_X3D_%s;\n", n->name, n->name);
	}
	fprintf(vrml_code, "\treturn 0;\n}\n\n");

	fprintf(vrml_code, "s32 gf_sg_x3d_node_get_field_index_by_name(GF_Node *node, char *name)\n{\n\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(BNodes); i++) {
		n = gf_list_get(BNodes, i);
		if (!n->skip_impl) {
			fprintf(vrml_code, "\tcase TAG_X3D_%s: return %s_get_field_index_by_name(name);\n", n->name, n->name);
		}
	}
	fprintf(vrml_code, "\tdefault:\n\t\treturn -1;\n\t}\n}\n\n");

}


static u32 IsNodeInTable(X3DNode *node, char *NDTName)
{
	u32 i;
	char *ndt;

	for (i=0; i<gf_list_count(node->NDT); i++) {
		ndt = gf_list_get(node->NDT, i);
		if (!strcmp(ndt, NDTName)) return 1;
	}
	return 0;
}

static u32 GetNDTCount(char *NDTName, GF_List *XNodes)
{
	u32 i, nodeCount;
	X3DNode *n;
	nodeCount = 0;
	for (i=0; i<gf_list_count(XNodes); i++) {
		n = gf_list_get(XNodes, i);
		if (!IsNodeInTable(n, NDTName)) continue;
		nodeCount++;
	}
	return nodeCount;
}

/*write NDTs*/
void WriteNDT(FILE *f, GF_List *XNodes, GF_List *NDTs)
{
	u32 i, j, count;
	Bool first;
	X3DNode *n;
	char *NDTName;
	fprintf(f, "\n\n/* NDT X3D */\n\n");

	//for all NDTs
	for (i=0; i<gf_list_count(NDTs); i++) {
		NDTName = gf_list_get(NDTs, i);
		count = GetNDTCount(NDTName, XNodes);
		if (!count) continue;

		fprintf(f, "#define %s_X3D_Count\t%d\n", NDTName, count);
		fprintf(f, "static const u32 %s_X3D_TypeToTag[%d] = {\n", NDTName, count);
		first = 1;
		//browse each node.
		for (j=0; j<gf_list_count(XNodes); j++) {
			n = gf_list_get(XNodes, j);
			if (!IsNodeInTable(n, NDTName)) continue;

			if (first) {
				fprintf(f, " TAG_X3D_%s", n->name);
				first = 0;
			} else {
				fprintf(f, ", TAG_X3D_%s", n->name);
			}
		}
		fprintf(f, "\n};\n\n");
	}

	//NodeTag complete translation
	fprintf(f, "\n\n\nBool gf_x3d_get_node_type(u32 NDT_Tag, u32 NodeTag)\n{\n\tconst u32 *types;\n\tu32 count, i;\n\tif (!NodeTag) return 0;\n\ttypes = NULL; count = 0;\n");

	fprintf(f, "\tswitch (NDT_Tag) {\n");
	for (i=0; i<gf_list_count(NDTs); i++) {
		NDTName = gf_list_get(NDTs, i);
		count = GetNDTCount(NDTName, XNodes);
		if (!count) continue;
		fprintf(f, "\tcase NDT_%s:\n\t\ttypes = %s_X3D_TypeToTag; count = %s_X3D_Count; break;\n", NDTName, NDTName, NDTName);
	}
	fprintf(f, "\tdefault:\n\t\treturn 0;\n\t}\n");

	fprintf(f, "\tfor(i=0; i<count; i++) { if (types[i]==NodeTag) return 1;}\n");
	fprintf(f, "\treturn 0;\n}\n");
}

void ParseTemplateFile(FILE *nodes, GF_List *BNodes, GF_List *NDTs)
{
	char sLine[2000];
	char token[100];
	char *p;
	X3DNode *n;
	X3DField *f;
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
				gf_list_add(BNodes, n);

				//get its name
				GetNextToken(n->name, " \t[");

				//extract the NDTs
				GetNextToken(token, "\t[ %#=");
				if (strcmp(token, "NDT")) {
					printf("Corrupted template file\n");
					return;
				}
				while (1) {
					GetNextToken(token, "=, \t");
					//done with NDTs
					if (!token[0]) break;

					//update the NDT list
					CheckInTable(token, NDTs);
					p = gf_malloc(strlen(token)+1);
					strcpy(p, token);
					gf_list_add(n->NDT, p);
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
							strcpy(f->def, "GF_MAX_FLOAT");
						} else if (!strcmp(f->def, "-I")) {
							strcpy(f->def, "GF_MIN_FLOAT");
						}
					} else if (!strcmp(f->familly, "SFTime")) {
						if (!strcmp(f->def, "+I") || !strcmp(f->def, "I")) {
							strcpy(f->def, "GF_MAX_FLOAT");
						} else if (!strcmp(f->def, "-I")) {
							strcpy(f->def, "GF_MIN_FLOAT");
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
					case 'q':
					case 'a':
						printf("Corrupted X3D template file (quantization/animation not allowed)\n");
						gf_list_del_item(n->Fields, f);
						gf_free(f);
						return;
					default:
						break;
					}
				}
				/*we ignore these*/
				if (!stricmp(f->name, "bboxCenter") || !stricmp(f->name, "bboxSize")) {
					gf_list_del_item(n->Fields, f);
					gf_free(f);
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


void WriteNodeDump(FILE *f, X3DNode *n)
{
	X3DField *bf;
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
	X3DNode *n;
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
	FILE *nodes, *pf;
	GF_List *XNodes, *NDTs;
	X3DNode *n;
	X3DField *bf;
	u32 nb_nodes, nb_imp;

	nodes = fopen("templates_X3D.txt", "rt");
	if (!nodes) {
		fprintf(stdout, "cannot open \"templates_X3D.txt\" - aborting\n");
		return 0;
	}

	XNodes = gf_list_new();
	NDTs = gf_list_new();
	//all nodes are in the same list but we keep version info
	ParseTemplateFile(nodes, XNodes, NDTs);
	fclose(nodes);

	if (argc>1) {
		pf = fopen(argv[1], "rt");
		if (!pf) fprintf(stdout, "Cannot open profile file %s\n", argv[1]);
		else {
			parse_profile(XNodes, pf);
			fclose(pf);
		}
	}

	//write the nodes def
	WriteNodesFile(XNodes, NDTs);

	nodes = BeginFile(1);

	//write all nodes init stuff
	WriteNodeCode(XNodes, nodes);

	WriteNDT(nodes, XNodes, NDTs);
	fprintf(nodes, "#endif /*GPAC_DISABLE_X3D*/\n\n");

	EndFile(nodes, 1);

	//free NDTs
	while (gf_list_count(NDTs)) {
		char *tmp = gf_list_get(NDTs, 0);
		gf_free(tmp);
		gf_list_rem(NDTs, 0);
	}
	gf_list_del(NDTs);

	nb_nodes = gf_list_count(XNodes);
	nb_imp = 0;
	//free nodes
	while (gf_list_count(XNodes)) {
		n = gf_list_get(XNodes, 0);
		if (!n->skip_impl) nb_imp++;
		gf_list_rem(XNodes, 0);
		while (gf_list_count(n->NDT)) {
			char *tmp = gf_list_get(n->NDT, 0);
			gf_free(tmp);
			gf_list_rem(n->NDT, 0);
		}
		gf_list_del(n->NDT);
		while (gf_list_count(n->Fields)) {
			bf = gf_list_get(n->Fields, 0);
			gf_free(bf);
			gf_list_rem(n->Fields, 0);
		}
		gf_list_del(n->Fields);
		gf_free(n);
	}
	gf_list_del(XNodes);

	fprintf(stdout, "Generation done: %d nodes implemented (%d nodes total)\n", nb_imp, nb_nodes);
	return 0;
}

