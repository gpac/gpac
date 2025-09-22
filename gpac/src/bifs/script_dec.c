/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / BIFS codec sub-project
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


#include "script.h"

#if !defined(GPAC_DISABLE_BIFS) && defined(GPAC_HAS_QJS)

#define BINOP_MINVAL ET_EQ

typedef struct
{
	GF_Node *script;
	GF_BifsDecoder *codec;
	GF_BitStream *bs;
	char *string;
	u32 length;
	GF_List *identifiers;
	char *new_line;
	u32 indent;
	u32 expr_stack_size;
} ScriptParser;


void SFS_Identifier(ScriptParser *parser);
void SFS_Arguments(ScriptParser *parser, Bool is_var);
void SFS_StatementBlock(ScriptParser *parser, Bool functBody);
void SFS_Statement(ScriptParser *parser);
void SFS_IfStatement(ScriptParser *parser);
void SFS_SwitchStatement(ScriptParser *parser);
void SFS_ForStatement(ScriptParser *parser);
void SFS_WhileStatement(ScriptParser *parser);
void SFS_ReturnStatement(ScriptParser *parser);
void SFS_CompoundExpression(ScriptParser *parser);
void SFS_OptionalExpression(ScriptParser *parser);
void SFS_Expression(ScriptParser *parser);
void SFS_NewObject(ScriptParser *parser);
void SFS_ArrayDeref(ScriptParser *parser);
void SFS_FunctionCall(ScriptParser *parser);
void SFS_ObjectMemberAccess(ScriptParser *parser);
void SFS_ObjectMethodCall(ScriptParser *parser);
void SFS_Params(ScriptParser *parser);
void SFS_GetNumber(ScriptParser *parser);
void SFS_GetString(ScriptParser *parser);
void SFS_GetBoolean(ScriptParser *parser);

#define PARSER_STEP_ALLOC	500

static void SFS_AddString(ScriptParser *parser, char *str)
{
	char *new_str;
	if (!str) return;
	if (strlen(parser->string) + strlen(str) >= parser->length) {
		parser->length = (u32) ( strlen(parser->string) + strlen(str) + PARSER_STEP_ALLOC );
		new_str = (char *)gf_malloc(sizeof(char)*parser->length);
		strcpy(new_str, parser->string);
		gf_free(parser->string);
		parser->string = new_str;
	}
	strncat(parser->string, str, parser->length - strlen(parser->string) - 1);
}

static void SFS_AddInt(ScriptParser *parser, s32 val)
{
	char msg[500];
	sprintf(msg, "%d", val);
	SFS_AddString(parser, msg);
}
static void SFS_AddChar(ScriptParser *parser, char c)
{
	char msg[2];
	sprintf(msg, "%c", c);
	SFS_AddString(parser, msg);
}


GF_Err ParseScriptField(ScriptParser *parser)
{
	GF_ScriptField *field;
	GF_Err e;
	u32 eventType, fieldType;
	char name[1000];
	GF_FieldInfo info;

	eventType = gf_bs_read_int(parser->bs, 2);
	fieldType = gf_bs_read_int(parser->bs, 6);
	gf_bifs_dec_name(parser->bs, name, 1000);
	field = gf_sg_script_field_new(parser->script, eventType, fieldType, name);
	if (!field) return GF_NON_COMPLIANT_BITSTREAM;

	//save the name in the list of identifiers
	gf_list_add(parser->identifiers, gf_strdup(name));

	if (parser->codec->pCurrentProto) {
		Bool isISfield = (Bool)gf_bs_read_int(parser->bs, 1);
		if (isISfield) {
			u32 numProtoField = gf_sg_proto_get_field_count(parser->codec->pCurrentProto);
			u32 numBits = gf_get_bit_size(numProtoField - 1);
			u32 field_all = gf_bs_read_int(parser->bs, numBits);
			e = gf_sg_script_field_get_info(field, &info);
			if (e) return e;
			e = gf_sg_proto_field_set_ised(parser->codec->pCurrentProto, field_all, parser->script, info.fieldIndex);
			return e;
		}
	}
	/*get default value*/
	if (eventType == GF_SG_SCRIPT_TYPE_FIELD) {
		if (gf_bs_read_int(parser->bs, 1)) {
			e = gf_sg_script_field_get_info(field, &info);
			if (e) return e;
			gf_bifs_dec_field(parser->codec, parser->bs, parser->script, &info, GF_FALSE);
		}
	}

	return parser->codec->LastError;
}

static void SFS_IncIndent(ScriptParser *pars) {
	pars->indent++;
}
static void SFS_DecIndent(ScriptParser *pars) {
	pars->indent--;
}
static void SFS_Space(ScriptParser *pars) {
	if (pars->new_line) SFS_AddString(pars, " ");
}
static void SFS_Indent(ScriptParser *pars)
{
	u32 i;
	if (pars->new_line) {
		for (i=0; i<pars->indent; i++) SFS_AddString(pars, " ");
	}
}
static GFINLINE void SFS_Line(ScriptParser *parser)
{
	if (parser->new_line) {
		SFS_AddString(parser, parser->new_line);
	}
}


GF_Err SFScript_Parse(GF_BifsDecoder *codec, SFScript *script_field, GF_BitStream *bs, GF_Node *n)
{
	GF_Err e;
	u32 i, count, nbBits;
	char *ptr;
	ScriptParser parser;
	e = GF_OK;
	if (gf_node_get_tag(n) != TAG_MPEG4_Script) return GF_NON_COMPLIANT_BITSTREAM;

	memset(&parser, 0, sizeof(ScriptParser));
	parser.codec = codec;
	parser.script = n;
	parser.bs = bs;
	parser.length = 500;
	parser.string = (char *) gf_malloc(sizeof(char)* parser.length);
	parser.string[0] = 0;
	parser.identifiers = gf_list_new();
	parser.new_line = (char *) (codec->dec_memory_mode ? "\n" : NULL);
	parser.indent = 0;

	//first parse fields

	if (gf_bs_read_int(bs, 1)) {
		//endFlag
		while (!gf_bs_read_int(bs, 1)) {
			e = ParseScriptField(&parser);
			if (e) goto exit;
		}
	} else {
		nbBits = gf_bs_read_int(bs, 4);
		count = gf_bs_read_int(bs, nbBits);
		for (i=0; i<count; i++) {
			e = ParseScriptField(&parser);
			if (e) goto exit;
		}
	}
	//reserevd
	gf_bs_read_int(bs, 1);
	//then parse
	SFS_AddString(&parser, "javascript:");
	SFS_AddString(&parser, parser.new_line);

	//hasFunction
	while (gf_bs_read_int(bs, 1)) {
		SFS_AddString(&parser, "function ");
		SFS_Identifier(&parser);
		SFS_Arguments(&parser, GF_FALSE);
		SFS_Space(&parser);
		SFS_StatementBlock(&parser, GF_TRUE);
		SFS_Line(&parser);
		if (codec->LastError) {
			e = codec->LastError;
			goto exit;
		}
	}

	SFS_Line(&parser);

	if (script_field->script_text) gf_free(script_field->script_text);
	script_field->script_text = (char *) gf_strdup(parser.string);

exit:
	//clean up
	while (gf_list_count(parser.identifiers)) {
		ptr = (char *)gf_list_get(parser.identifiers, 0);
		gf_free(ptr);
		gf_list_rem(parser.identifiers, 0);
	}
	gf_list_del(parser.identifiers);
	if (parser.string) gf_free(parser.string);
	return e;
}



void SFS_Identifier(ScriptParser *parser)
{
	u32 index;
	char name[500];

	if (parser->codec->LastError) return;

	//received
	if (gf_bs_read_int(parser->bs, 1)) {
		index = gf_bs_read_int(parser->bs, gf_get_bit_size(gf_list_count(parser->identifiers) - 1));
		SFS_AddString(parser, (char *)gf_list_get(parser->identifiers, index));
	}
	//parse
	else {
		gf_bifs_dec_name(parser->bs, name, 500);
		gf_list_add(parser->identifiers, gf_strdup(name));
		SFS_AddString(parser, name);
	}
}

void SFS_Arguments(ScriptParser *parser, Bool is_var)
{
	u32 val;
	if (parser->codec->LastError) return;
	if (!is_var) SFS_AddString(parser, "(");

	val = gf_bs_read_int(parser->bs, 1);
	while (val) {
		SFS_Identifier(parser);
		val = gf_bs_read_int(parser->bs, 1);
		if (val) SFS_AddString(parser, ",");
	}
	if (!is_var) SFS_AddString(parser, ")");
}

void SFS_StatementBlock(ScriptParser *parser, Bool funcBody)
{
	if (parser->codec->LastError) return;

	if (gf_bs_read_int(parser->bs, 1)) {
		SFS_AddString(parser, "{");
		SFS_IncIndent(parser);

		while (gf_bs_read_int(parser->bs, 1)) {
			SFS_Line(parser);
			SFS_Indent(parser);
			SFS_Statement(parser);
		}
		SFS_DecIndent(parser);
		SFS_Line(parser);
		SFS_Indent(parser);
		SFS_AddString(parser, "}");
	} else if (funcBody) {
		SFS_AddString(parser, "{");
		SFS_Statement(parser);
		SFS_AddString(parser, "}");
	} else {
		SFS_Statement(parser);
	}
}


void SFS_Statement(ScriptParser *parser)
{
	u32 val;
	if (parser->codec->LastError) return;

	val = gf_bs_read_int(parser->bs, NUMBITS_STATEMENT);
	switch (val) {
	case ST_IF:
		SFS_IfStatement(parser);
		break;
	case ST_FOR:
		SFS_ForStatement(parser);
		break;
	case ST_WHILE:
		SFS_WhileStatement(parser);
		break;
	case ST_RETURN:
		SFS_ReturnStatement(parser);
		break;
	case ST_BREAK:
		SFS_AddString(parser, "break;");
		break;
	case ST_CONTINUE:
		SFS_AddString(parser, "continue;");
		break;
	case ST_COMPOUND_EXPR:
		SFS_CompoundExpression(parser);
		SFS_AddString(parser, ";");
		break;
	case ST_SWITCH:
		SFS_SwitchStatement(parser);
		break;
	}
}

void SFS_IfStatement(ScriptParser *parser)
{
	if (parser->codec->LastError) return;
	SFS_AddString(parser, "if (");
	SFS_CompoundExpression(parser);
	SFS_AddString(parser, ") ");
	SFS_StatementBlock(parser, GF_FALSE);
	//has else
	if (gf_bs_read_int(parser->bs, 1)) {
		SFS_Line(parser);
		SFS_Indent(parser);
		SFS_AddString(parser, "else ");
		SFS_StatementBlock(parser, GF_FALSE);
	}
}

void SFS_SwitchStatement(ScriptParser *parser)
{
	u32 numBits, caseValue;

	if (parser->codec->LastError) return;
	SFS_AddString(parser, "switch (");
	SFS_CompoundExpression(parser);
	SFS_AddString(parser, ")");
	SFS_AddString(parser, "{");
	SFS_Line(parser);

	numBits = gf_bs_read_int(parser->bs, 5);
	do {
		SFS_Indent(parser);
		SFS_AddString(parser, "case ");
		caseValue = gf_bs_read_int(parser->bs, numBits);
		SFS_AddInt(parser, caseValue);
		SFS_AddString(parser, ":");
		SFS_Line(parser);
		SFS_Indent(parser);
		SFS_StatementBlock(parser, GF_FALSE);
		SFS_Line(parser);
	}
	while (gf_bs_read_int(parser->bs, 1));

	//default
	if (gf_bs_read_int(parser->bs, 1)) {
		SFS_AddString(parser, "default:");
		SFS_Line(parser);
		SFS_StatementBlock(parser, GF_FALSE);
	}
	SFS_AddString(parser, "}");
}

void SFS_ForStatement(ScriptParser *parser)
{
	if (parser->codec->LastError) return;
	SFS_AddString(parser, "for (");
	SFS_OptionalExpression(parser);
	SFS_AddString(parser, ";");
	SFS_OptionalExpression(parser);
	SFS_AddString(parser, ";");
	SFS_OptionalExpression(parser);
	SFS_AddString(parser, ")");

	SFS_StatementBlock(parser, GF_FALSE);
}

void SFS_WhileStatement(ScriptParser *parser)
{
	if (parser->codec->LastError) return;
	SFS_AddString(parser, "while (");
	SFS_CompoundExpression(parser);
	SFS_AddString(parser, ")");

	SFS_StatementBlock(parser, GF_FALSE);
}

void SFS_ReturnStatement(ScriptParser *parser)
{
	if (parser->codec->LastError) return;
	SFS_AddString(parser, "return");
	if (gf_bs_read_int(parser->bs, 1)) {
		SFS_AddString(parser, " ");
		SFS_CompoundExpression(parser);
	}
	SFS_AddString(parser, ";");
	SFS_Line(parser);
}

void SFS_CompoundExpression(ScriptParser *parser)
{
	if (parser->codec->LastError) return;
	SFS_Expression(parser);
	if (! gf_bs_read_int(parser->bs, 1)) return;
	if (parser->codec->LastError) return;
	SFS_AddString(parser, ",");
	SFS_CompoundExpression(parser);
}

void SFS_OptionalExpression(ScriptParser *parser)
{
	if (parser->codec->LastError) return;
	if (gf_bs_read_int(parser->bs, 1)) {
		SFS_CompoundExpression(parser);
	}
}

#define MAX_EXPR_STACK	500
void SFS_Expression(ScriptParser *parser)
{
	u32 val = gf_bs_read_int(parser->bs, NUMBITS_EXPR_TYPE);
	if (parser->codec->LastError) return;

	//limit max expression stack size
	parser->expr_stack_size++;
	if (parser->expr_stack_size>MAX_EXPR_STACK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[BIFS] Max stack size %d reached for expressions, not supported\n", MAX_EXPR_STACK))
		parser->codec->LastError = GF_NON_COMPLIANT_BITSTREAM;
		return;
	}

	switch(val) {
	case ET_CURVED_EXPR:
		SFS_AddString(parser, "(");
		SFS_CompoundExpression(parser);
		SFS_AddString(parser, ")");
		break;
	case ET_NEGATIVE:
		SFS_AddString(parser, "-");
		SFS_Expression(parser);
		break;
	case ET_NOT:
		SFS_AddString(parser, "!");
		SFS_Expression(parser);
		break;
	case ET_ONESCOMP:
		SFS_AddString(parser, "~");
		SFS_Expression(parser);
		break;
	case ET_INCREMENT:
		SFS_AddString(parser, "++");
		SFS_Expression(parser);
		break;
	case ET_DECREMENT:
		SFS_AddString(parser, "--");
		SFS_Expression(parser);
		break;
	case ET_POST_INCREMENT:
		SFS_Expression(parser);
		SFS_AddString(parser, "++");
		break;
	case ET_POST_DECREMENT:
		SFS_Expression(parser);
		SFS_AddString(parser, "--");
		break;
	case ET_CONDTEST:
		SFS_Expression(parser);
		SFS_AddString(parser, " ? ");
		SFS_Expression(parser);
		SFS_AddString(parser, " : ");
		SFS_Expression(parser);
		break;
	case ET_STRING:
		SFS_AddString(parser, "'");
		SFS_GetString(parser);
		SFS_AddString(parser, "'");
		break;
	case ET_NUMBER:
		SFS_GetNumber(parser);
		break;
	case ET_IDENTIFIER:
		SFS_Identifier(parser);
		break;
	case ET_FUNCTION_CALL:
		SFS_FunctionCall(parser);
		break;
	case ET_NEW:
		SFS_NewObject(parser);
		break;
	case ET_OBJECT_MEMBER_ACCESS:
		SFS_ObjectMemberAccess(parser);
		break;
	case ET_OBJECT_METHOD_CALL:
		SFS_ObjectMethodCall(parser);
		break;
	case ET_ARRAY_DEREFERENCE:
		SFS_ArrayDeref(parser);
		break;

	case ET_MULTIPLY:
		SFS_Expression(parser);
		SFS_AddString(parser, "*");
		SFS_Expression(parser);
		break;
	case ET_DIVIDE:
		SFS_Expression(parser);
		SFS_AddString(parser, "/");
		SFS_Expression(parser);
		break;
	case ET_MOD:
		SFS_Expression(parser);
		SFS_AddString(parser, "%");
		SFS_Expression(parser);
		break;
	case ET_PLUS:
		SFS_Expression(parser);
		SFS_AddString(parser, "+");
		SFS_Expression(parser);
		break;
	case ET_MINUS:
		SFS_Expression(parser);
		SFS_AddString(parser, "-");
		SFS_Expression(parser);
		break;
	case ET_LSHIFT:
		SFS_Expression(parser);
		SFS_AddString(parser, "<<");
		SFS_Expression(parser);
		break;
	case ET_RSHIFT:
		SFS_Expression(parser);
		SFS_AddString(parser, ">>");
		SFS_Expression(parser);
		break;
	case ET_RSHIFTFILL:
		SFS_Expression(parser);
		SFS_AddString(parser, ">>>");
		SFS_Expression(parser);
		break;
	case ET_AND:
		SFS_Expression(parser);
		SFS_AddString(parser, "&");
		SFS_Expression(parser);
		break;
	case ET_XOR:
		SFS_Expression(parser);
		SFS_AddString(parser, "^");
		SFS_Expression(parser);
		break;
	case ET_OR:
		SFS_Expression(parser);
		SFS_AddString(parser, "|");
		SFS_Expression(parser);
		break;
	case ET_LT:
		SFS_Expression(parser);
		SFS_AddString(parser, "<");
		SFS_Expression(parser);
		break;
	case ET_LE:
		SFS_Expression(parser);
		SFS_AddString(parser, "<=");
		SFS_Expression(parser);
		break;
	case ET_GT:
		SFS_Expression(parser);
		SFS_AddString(parser, ">");
		SFS_Expression(parser);
		break;
	case ET_GE:
		SFS_Expression(parser);
		SFS_AddString(parser, ">=");
		SFS_Expression(parser);
		break;
	case ET_EQ:
		SFS_Expression(parser);
		SFS_AddString(parser, "==");
		SFS_Expression(parser);
		break;
	case ET_NE:
		SFS_Expression(parser);
		SFS_AddString(parser, "!=");
		SFS_Expression(parser);
		break;
	case ET_LAND:
		SFS_Expression(parser);
		SFS_AddString(parser, "&&");
		SFS_Expression(parser);
		break;
	case ET_LOR:
		SFS_Expression(parser);
		SFS_AddString(parser, "||");
		SFS_Expression(parser);
		break;
	case ET_ASSIGN:
		SFS_Expression(parser);
		SFS_AddString(parser, "=");
		SFS_Expression(parser);
		break;
	case ET_PLUSEQ:
		SFS_Expression(parser);
		SFS_AddString(parser, "+=");
		SFS_Expression(parser);
		break;
	case ET_MINUSEQ:
		SFS_Expression(parser);
		SFS_AddString(parser, "-=");
		SFS_Expression(parser);
		break;
	case ET_MULTIPLYEQ:
		SFS_Expression(parser);
		SFS_AddString(parser, "*=");
		SFS_Expression(parser);
		break;
	case ET_DIVIDEEQ:
		SFS_Expression(parser);
		SFS_AddString(parser, "/=");
		SFS_Expression(parser);
		break;
	case ET_MODEQ:
		SFS_Expression(parser);
		SFS_AddString(parser, "%=");
		SFS_Expression(parser);
		break;
	case ET_LSHIFTEQ:
		SFS_Expression(parser);
		SFS_AddString(parser, "<<=");
		SFS_Expression(parser);
		break;
	case ET_RSHIFTEQ:
		SFS_Expression(parser);
		SFS_AddString(parser, ">>=");
		SFS_Expression(parser);
		break;
	case ET_RSHIFTFILLEQ:
		SFS_Expression(parser);
		SFS_AddString(parser, ">>>=");
		SFS_Expression(parser);
		break;
	case ET_ANDEQ:
		SFS_Expression(parser);
		SFS_AddString(parser, "&=");
		SFS_Expression(parser);
		break;
	case ET_XOREQ:
		SFS_Expression(parser);
		SFS_AddString(parser, "^=");
		SFS_Expression(parser);
		break;
	case ET_OREQ:
		SFS_Expression(parser);
		SFS_AddString(parser, "|=");
		SFS_Expression(parser);
		break;
	case ET_BOOLEAN:
		SFS_GetBoolean(parser);
		break;
	case ET_VAR:
		SFS_AddString(parser, "var ");
		SFS_Arguments(parser, GF_TRUE);
		break;
	case ET_FUNCTION_ASSIGN:
		SFS_AddString(parser, "function ");
		SFS_Arguments(parser, GF_FALSE);
		SFS_StatementBlock(parser, GF_TRUE);
		break;
	default:
		parser->codec->LastError = GF_NON_COMPLIANT_BITSTREAM;
		break;
	}
	parser->expr_stack_size--;
}

void SFS_NewObject(ScriptParser *parser)
{
	if (parser->codec->LastError) return;
	SFS_AddString(parser, "new ");
	SFS_Identifier(parser);
	SFS_AddString(parser, "(");
	SFS_Params(parser);
	SFS_AddString(parser, ") ");
}

void SFS_ArrayDeref(ScriptParser *parser)
{
	if (parser->codec->LastError) return;
	SFS_Expression(parser);
	if (parser->codec->LastError) return;
	SFS_AddString(parser, "[");
	SFS_CompoundExpression(parser);
	SFS_AddString(parser, "]");
}

void SFS_FunctionCall(ScriptParser *parser)
{
	if (parser->codec->LastError) return;
	SFS_Identifier(parser);
	SFS_AddString(parser, "(");
	SFS_Params(parser);
	SFS_AddString(parser, ")");
}

void SFS_ObjectMemberAccess(ScriptParser *parser)
{
	if (parser->codec->LastError) return;
	SFS_Expression(parser);
	if (parser->codec->LastError) return;
	SFS_AddString(parser, ".");
	SFS_Identifier(parser);
}


void SFS_ObjectMethodCall(ScriptParser *parser)
{
	if (parser->codec->LastError) return;
	SFS_Expression(parser);
	if (parser->codec->LastError) return;
	SFS_AddString(parser, ".");
	SFS_Identifier(parser);
	SFS_AddString(parser, "(");
	SFS_Params(parser);
	SFS_AddString(parser, ")");
}

void SFS_Params(ScriptParser *parser)
{
	u32 val;
	if (parser->codec->LastError) return;
	val = gf_bs_read_int(parser->bs, 1);
	while (val) {
		SFS_Expression(parser);
		if (parser->codec->LastError) return;
		val = gf_bs_read_int(parser->bs, 1);
		if(val) SFS_AddString(parser, ",");
	}
}

void SFS_GetNumber(ScriptParser *parser)
{
	u32 val, nbBits;

	if (parser->codec->LastError) return;
	// integer
	if (gf_bs_read_int(parser->bs, 1)) {
		nbBits = gf_bs_read_int(parser->bs, 5);
		val = gf_bs_read_int(parser->bs, nbBits);
		SFS_AddInt(parser, val);
		return;
	}
	// real number
	val = gf_bs_read_int(parser->bs, 4);
	while ( val != 15) {
		if (val<=9) {
			SFS_AddChar(parser, (char) (val + '0') );
		} else if (val==10) {
			SFS_AddChar(parser, '.');
		} else if (val==11) {
			SFS_AddChar(parser, 'E');
		} else if (val==12) {
			SFS_AddChar(parser, '-');
		}
		val = gf_bs_read_int(parser->bs, 4);
	}
}

void SFS_GetString(ScriptParser *parser)
{
	char name[1000];
	if (parser->codec->LastError) return;
	gf_bifs_dec_name(parser->bs, name, 1000);
	SFS_AddString(parser, name);
}

void SFS_GetBoolean(ScriptParser *parser)
{
	if (parser->codec->LastError) return;
	if (gf_bs_read_int(parser->bs, 1)) {
		SFS_AddString(parser, "true");
	} else {
		SFS_AddString(parser, "false");
	}
}

#endif /*!defined(GPAC_DISABLE_BIFS) && defined(GPAC_HAS_QJS)*/
