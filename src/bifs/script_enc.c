/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

/*
		script encoder is IM1 version
*/

#include "script.h"

#include <gpac/internal/scenegraph_dev.h>

#if !defined(GPAC_DISABLE_BIFS) && !defined(GPAC_DISABLE_BIFS_ENC) && defined(GPAC_HAS_QJS)

typedef struct
{
	GF_Node *script;
	GF_BifsEncoder *codec;
	GF_BitStream *bs;
	GF_List *identifiers;
	GF_Err err;

	char *cur_buf;
	char token[500];
	u32 token_code;

	u32 cur_line;

	Bool emul;

	char expr_toks[500];
	u32 expr_toks_len;
	GF_List *id_buf;
} ScriptEnc;


#define SFE_WRITE_INT(sc_enc, val, nbBits, str1, str2)	\
		if (!sc_enc->emul) GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, val, nbBits, str1, str2);	\


static GF_Err EncScriptFields(ScriptEnc *sc_enc)
{
	u32 nbFields, nbBits, eType, nbBitsProto, i;
	Bool use_list;
	GF_Err e;
	GF_FieldInfo info;

	nbFields = gf_node_get_num_fields_in_mode(sc_enc->script, GF_SG_FIELD_CODING_ALL) - 3;
	use_list = GF_TRUE;
	nbBits = gf_get_bit_size(nbFields);
	if (nbFields+1 > 4 + gf_get_bit_size(nbFields)) use_list = GF_FALSE;
	if (!nbFields) {
		GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, 1, 1, "Script::isList", NULL);
		GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, 1, 1, "end", NULL);
		return GF_OK;
	}
	GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, use_list ? 1 : 0, 1, "Script::isList", NULL);
	if (!use_list) {
		GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, nbBits, 4, "nbBits", NULL);
		GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, nbFields, nbBits, "count", NULL);
	}

	nbBitsProto = 0;
	if (sc_enc->codec->encoding_proto) nbBitsProto = gf_get_bit_size(gf_sg_proto_get_field_count(sc_enc->codec->encoding_proto) - 1);

	for (i=0; i<nbFields; i++) {
		if (use_list) GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, 0, 1, "end", NULL);

		gf_node_get_field(sc_enc->script, i+3, &info);
		switch (info.eventType) {
		case GF_SG_EVENT_IN:
			eType = GF_SG_SCRIPT_TYPE_EVENT_IN;
			break;
		case GF_SG_EVENT_OUT:
			eType = GF_SG_SCRIPT_TYPE_EVENT_OUT;
			break;
		default:
			eType = GF_SG_SCRIPT_TYPE_FIELD;
			break;
		}
		GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, eType, 2, "eventType", NULL);
		GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, info.fieldType, 6, "fieldType", NULL);
		gf_bifs_enc_name(sc_enc->codec, sc_enc->bs, (char *) info.name);
		/*this is an identifier for script*/
		gf_list_add(sc_enc->identifiers, gf_strdup(info.name));

		if (sc_enc->codec->encoding_proto) {
			GF_Route *isedField = gf_bifs_enc_is_field_ised(sc_enc->codec, sc_enc->script, i+3);
			if (isedField) {
				GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, 1, 1, "isedField", NULL);

				if (isedField->ToNode == sc_enc->script) {
					GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, isedField->FromField.fieldIndex, nbBitsProto, "protoField", NULL);
				} else {
					GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, isedField->ToField.fieldIndex, nbBitsProto, "protoField", NULL);
				}
				continue;
			}
			GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, 0, 1, "isedField", NULL);
		}
		/*default value*/
		if (eType == GF_SG_SCRIPT_TYPE_FIELD) {
			GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, (info.far_ptr) ? 1 : 0, 1, "hasInitialValue", NULL);
			if (info.far_ptr) {
				e = gf_bifs_enc_field(sc_enc->codec, sc_enc->bs, sc_enc->script, &info);
				if (e) return e;
			}
		}
	}
	if (use_list) GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, 1, 1, "end", NULL);
	return GF_OK;
}



enum
{
	TOK_FUNCTION,
	TOK_IF,
	TOK_ELSE,
	TOK_FOR,
	TOK_WHILE,
	TOK_RETURN,
	TOK_BREAK,
	TOK_CONTINUE,
	TOK_NEW,
	TOK_SWITCH,
	TOK_CASE,
	TOK_DEFAULT,
	TOK_VAR,
	NUMBER_OF_KEYWORD,
	TOK_LEFT_BRACE = NUMBER_OF_KEYWORD,
	TOK_RIGHT_BRACE,
	TOK_LEFT_CURVE,
	TOK_RIGHT_CURVE,
	TOK_LEFT_BRACKET,
	TOK_RIGHT_BRACKET,
	TOK_PERIOD,
	TOK_NOT,
	TOK_ONESCOMP,
	TOK_NEGATIVE,
	TOK_INCREMENT,
	TOK_DECREMENT,
	TOK_MULTIPLY,
	TOK_DIVIDE,
	TOK_MOD,
	TOK_PLUS,
	TOK_MINUS,
	TOK_LSHIFT,
	TOK_RSHIFT,
	TOK_RSHIFTFILL,
	TOK_LT,
	TOK_LE,
	TOK_GT,
	TOK_GE,
	TOK_EQ,
	TOK_NE,
	TOK_AND,
	TOK_XOR,
	TOK_OR,
	TOK_LAND,
	TOK_LOR,
	TOK_CONDTEST,
	TOK_ASSIGN,
	TOK_PLUSEQ,
	TOK_MINUSEQ,
	TOK_MULTIPLYEQ,
	TOK_DIVIDEEQ,
	TOK_MODEQ,
	TOK_LSHIFTEQ,
	TOK_RSHIFTEQ,
	TOK_RSHIFTFILLEQ,
	TOK_ANDEQ,
	TOK_XOREQ,
	TOK_OREQ,
	TOK_COMMA,
	TOK_SEMICOLON,
	TOK_CONDSEP,
	TOK_IDENTIFIER,
	TOK_STRING,
	TOK_NUMBER,
	TOK_EOF,
	TOK_BOOLEAN
};

const char *tok_names[] =
{
	"function",
	"if",
	"else",
	"for",
	"while",
	"return",
	"break",
	"continue",
	"new",
	"switch",
	"case",
	"default",
	"var",
	"{",
	"}",
	"(",
	")",
	"[",
	"]",
	".",
	"!",
	"~",
	"-",
	"++",
	"--",
	"*",
	"/",
	"%",
	"+",
	"-",
	"<<",
	">>",
	">>>",
	"<",
	"<=",
	">",
	">=",
	"==",
	"!=",
	"&",
	"^",
	"|",
	"&&",
	"||",
	"?",
	"=",
	"+=",
	"-=",
	"*=",
	"/=",
	"%=",
	"<<=",
	">>=",
	">>>=",
	"&=",
	"^=",
	"|=",
	",",
	";",
	":",
	"identifier",
	"string",
	"number",
	"boolean",
	"end of script"
};

const char* sc_keywords [] =
{
	"function",
	"if",
	"else",
	"for",
	"while",
	"return",
	"break",
	"continue",
	"new",
	"switch",
	"case",
	"default",
	"var"
};

Bool SFE_GetNumber(ScriptEnc *sc_enc)
{
	u32 i = 0;
	Bool exp = GF_FALSE;
	while ( isdigit(sc_enc->cur_buf[i])
	        || (toupper(sc_enc->cur_buf[i])=='X')
	        || ((toupper(sc_enc->cur_buf[i]) >='A') && (toupper(sc_enc->cur_buf[i])<='F'))
	        || (sc_enc->cur_buf[i]=='.')
	        || (tolower(sc_enc->cur_buf[i])=='e')
	        || (exp && (sc_enc->cur_buf[i] == '-'))
	      ) {
		sc_enc->token[i] = sc_enc->cur_buf[i];
		if (tolower(sc_enc->cur_buf[i])=='e') exp = GF_TRUE;
		i++;
		if (!sc_enc->cur_buf[i]) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: Invalid number syntax (%s)\n", sc_enc->cur_buf));
			sc_enc->err = GF_BAD_PARAM;
			return GF_FALSE;
		}
	}
	sc_enc->token[i] = 0;
	sc_enc->cur_buf += i;
	sc_enc->token_code = TOK_NUMBER;
	return GF_TRUE;
}

Bool SFE_NextToken(ScriptEnc *sc_enc)
{
	u32 i;
	if (sc_enc->err) return GF_FALSE;
	while (strchr(" \t\r\n", sc_enc->cur_buf[0])) {
		if (sc_enc->cur_buf[0]=='\n') sc_enc->cur_line ++;
		sc_enc->cur_buf++;
	}
	if ((sc_enc->cur_buf[0] == '/') && (sc_enc->cur_buf[1] == '*')) {
		sc_enc->cur_buf += 2;
		while ((sc_enc->cur_buf[0] != '*') || (sc_enc->cur_buf[1] != '/')) {
			sc_enc->cur_buf++;
			if (!sc_enc->cur_buf[0] || !sc_enc->cur_buf[1]) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: cannot find closing comment */\n"));
				sc_enc->err = GF_BAD_PARAM;
				return GF_FALSE;
			}
		}
		sc_enc->cur_buf+=2;
		return SFE_NextToken(sc_enc);
	}
	i = 0;
	/*get a name*/
	if (isalpha(sc_enc->cur_buf[i]) || (sc_enc->cur_buf[i]=='_')) {
		while (isalnum(sc_enc->cur_buf[i]) || (sc_enc->cur_buf[i]=='_')) {
			sc_enc->token[i] = sc_enc->cur_buf[i];
			i++;
		}
		sc_enc->token[i] = 0;
		sc_enc->cur_buf += i;
		sc_enc->token_code = TOK_IDENTIFIER;
		/*check keyword*/
		for (i=0; i<NUMBER_OF_KEYWORD; i++) {
			if (!strcmp(sc_enc->token, sc_keywords[i])) {
				sc_enc->token_code = i;
				return GF_TRUE;
			}
		}
		if (!stricmp(sc_enc->token, "TRUE") || !stricmp(sc_enc->token, "FALSE") ) {
			sc_enc->token_code = TOK_BOOLEAN;
		}
		return GF_TRUE;
	}
	/*get a number*/
	if (isdigit(sc_enc->cur_buf[i])) return SFE_GetNumber(sc_enc);
	/*get a string*/
	if ((sc_enc->cur_buf[i]=='\'') || (sc_enc->cur_buf[i]=='\"')
	        || ((sc_enc->cur_buf[i]=='\\') && (sc_enc->cur_buf[i+1]=='\"'))
	   ) {
		char end;
		Bool skip_last = GF_FALSE;
		end = sc_enc->cur_buf[i];
		if (sc_enc->cur_buf[i]=='\\') {
			skip_last = GF_TRUE;
			sc_enc->cur_buf++;
		}
		while (sc_enc->cur_buf[i+1] != end) {
			sc_enc->token[i] = sc_enc->cur_buf[i+1];
			i++;
		}
		sc_enc->token[i] = 0;
		sc_enc->cur_buf += i+2;
		if (skip_last) sc_enc->cur_buf++;
		sc_enc->token_code = TOK_STRING;
		return GF_TRUE;
	}
	/*all other codes*/
	switch (sc_enc->cur_buf[i]) {
	case '.':
		if (isdigit(sc_enc->cur_buf[i+1])) {
			SFE_GetNumber(sc_enc);
			return GF_TRUE;
		} else {
			sc_enc->token_code = TOK_PERIOD;
		}
		break;
	case '!':
		if (sc_enc->cur_buf[i+1] == '=') {
			sc_enc->token_code = TOK_NE;
			sc_enc->cur_buf ++;
		} else {
			sc_enc->token_code = TOK_NOT;
		}
		break;
	case '=':
		if (sc_enc->cur_buf[i+1]=='=') {
			if (sc_enc->cur_buf[i+2]=='=') {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[BIFSEnc] JavaScript token '===' not supported by standard\n"));
				sc_enc->err = GF_NOT_SUPPORTED;
				return GF_FALSE;
			}
			sc_enc->token_code = TOK_EQ;
			sc_enc->cur_buf ++;
		} else {
			sc_enc->token_code = TOK_ASSIGN;
		}
		break;
	case '+':
		if(sc_enc->cur_buf[i+1]=='=') {
			sc_enc->token_code = TOK_PLUSEQ;
			sc_enc->cur_buf ++;
		} else if(sc_enc->cur_buf[i+1]=='+') {
			sc_enc->token_code = TOK_INCREMENT;
			sc_enc->cur_buf++;
		} else {
			sc_enc->token_code = TOK_PLUS;
		}
		break;
	case '-':
		if(sc_enc->cur_buf[i+1]=='=') {
			sc_enc->token_code = TOK_MINUSEQ;
			sc_enc->cur_buf++;
		} else if(sc_enc->cur_buf[i+1] == '-') {
			sc_enc->token_code = TOK_DECREMENT;
			sc_enc->cur_buf++;
		} else {
			sc_enc->token_code = TOK_MINUS;
		}
		break;
	case '*':
		if(sc_enc->cur_buf[i+1]=='=') {
			sc_enc->token_code = TOK_MULTIPLYEQ;
			sc_enc->cur_buf++;
		} else {
			sc_enc->token_code = TOK_MULTIPLY;
		}
		break;
	case '/':
		if(sc_enc->cur_buf[i+1]=='=') {
			sc_enc->token_code = TOK_DIVIDEEQ;
			sc_enc->cur_buf++;
		} else {
			sc_enc->token_code = TOK_DIVIDE;
		}
		break;
	case '%':
		if(sc_enc->cur_buf[i+1]=='=') {
			sc_enc->token_code = TOK_MODEQ;
			sc_enc->cur_buf++;
		} else {
			sc_enc->token_code = TOK_MOD;
		}
		break;
	case '&':
		if(sc_enc->cur_buf[i+1]=='=') {
			sc_enc->token_code = TOK_ANDEQ;
			sc_enc->cur_buf++;
		} else if(sc_enc->cur_buf[i+1]=='&') {
			sc_enc->token_code = TOK_LAND;
			sc_enc->cur_buf++;
		} else {
			sc_enc->token_code = TOK_AND;
		}
		break;
	case '|':
		if(sc_enc->cur_buf[i+1]=='=') {
			sc_enc->token_code = TOK_OREQ;
			sc_enc->cur_buf++;
		} else if(sc_enc->cur_buf[i+1]=='|') {
			sc_enc->token_code = TOK_LOR;
			sc_enc->cur_buf++;
		} else {
			sc_enc->token_code = TOK_OR;
		}
		break;
	case '^':
		if(sc_enc->cur_buf[i+1]=='=') {
			sc_enc->token_code = TOK_XOREQ;
			sc_enc->cur_buf++;
		} else {
			sc_enc->token_code = TOK_XOR;
		}
		break;
	case '<':
		if (sc_enc->cur_buf[i+1]=='<') {
			if(sc_enc->cur_buf[i+2]=='=') {
				sc_enc->token_code = TOK_LSHIFTEQ;
				sc_enc->cur_buf += 1;
			} else {
				sc_enc->token_code = TOK_LSHIFT;
			}
			sc_enc->cur_buf += 1;
		} else if(sc_enc->cur_buf[i+1]=='=') {
			sc_enc->token_code = TOK_LE;
			sc_enc->cur_buf ++;
		} else {
			sc_enc->token_code = TOK_LT;
		}
		break;
	case '>':
		if (sc_enc->cur_buf[i+1]=='>') {
			if (sc_enc->cur_buf[i+2]=='=') {
				sc_enc->token_code = TOK_RSHIFTEQ;
				sc_enc->cur_buf ++;
			} else if(sc_enc->cur_buf[i+2]=='>') {
				if(sc_enc->cur_buf[i+3]=='=') {
					sc_enc->token_code = TOK_RSHIFTFILLEQ;
					sc_enc->cur_buf ++;
				} else {
					sc_enc->token_code = TOK_RSHIFTFILL;
				}
				sc_enc->cur_buf ++;
			} else {
				sc_enc->token_code = TOK_RSHIFT;
			}
			sc_enc->cur_buf ++;
		} else if (sc_enc->cur_buf[i+1]=='=') {
			sc_enc->token_code = TOK_GE;
			sc_enc->cur_buf ++;
		} else {
			sc_enc->token_code = TOK_GT;
		}
		break;
	case '?':
		sc_enc->token_code = TOK_CONDTEST;
		break;
	case ':':
		sc_enc->token_code = TOK_CONDSEP;
		break;
	case '~':
		sc_enc->token_code = TOK_ONESCOMP;
		break;
	case ',':
		sc_enc->token_code = TOK_COMMA;
		break;
	case ';':
		sc_enc->token_code = TOK_SEMICOLON;
		break;
	case '{':
		sc_enc->token_code = TOK_LEFT_BRACE;
		break;
	case '}':
		sc_enc->token_code = TOK_RIGHT_BRACE;
		break;
	case '(':
		sc_enc->token_code = TOK_LEFT_CURVE;
		break;
	case ')':
		sc_enc->token_code = TOK_RIGHT_CURVE;
		break;
	case '[':
		sc_enc->token_code = TOK_LEFT_BRACKET;
		break;
	case ']':
		sc_enc->token_code = TOK_RIGHT_BRACKET;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: Unrecognized symbol %c\n", sc_enc->cur_buf[i]));
		sc_enc->err = GF_BAD_PARAM;
		return GF_FALSE;
	}
	sc_enc->cur_buf ++;
	return GF_TRUE;
}

Bool SFE_CheckToken(ScriptEnc *sc_enc, u32 token)
{
	if (sc_enc->token_code != token) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: Bad token (expecting \"%s\" got \"%s\")\n", tok_names[token] , tok_names[sc_enc->token_code]));
		return GF_FALSE;
	}
	return GF_TRUE;
}

void SFE_PutIdentifier(ScriptEnc *sc_enc, char *id)
{
	u32 i;
	u32 nbBits, length;
	char *str;

	if (sc_enc->emul) return;

	i=0;
	while ((str = (char *)gf_list_enum(sc_enc->identifiers, &i))) {
		if (strcmp(str, id)) continue;

		nbBits = 0;
		length = gf_list_count(sc_enc->identifiers) - 1;
		while (length > 0) {
			length >>= 1;
			nbBits ++;
		}
		GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, 1, 1, "received", str);
		GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, i-1, nbBits, "identifierCode", str);
		return;
	}
	GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, 0, 1, "received", id);
	gf_list_add(sc_enc->identifiers, gf_strdup(id));
	gf_bifs_enc_name(sc_enc->codec, sc_enc->bs, id);
}


void SFE_Arguments(ScriptEnc *sc_enc)
{
	while (1) {
		if (!SFE_NextToken(sc_enc)) return;
		if (sc_enc->token_code == TOK_RIGHT_CURVE) break;
		else if (sc_enc->token_code == TOK_COMMA) continue;
		GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, 1, 1, "hasArgument", NULL);
		SFE_PutIdentifier(sc_enc, sc_enc->token);
	}
	GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, 0, 1, "hasArgument", NULL);
}

void SFE_StatementBlock(ScriptEnc *sc_enc);
void SFE_Statement(ScriptEnc *sc_enc);

void SFE_PutInteger(ScriptEnc *sc_enc, char *str)
{
	u32 nbBits, val = 0;
	if (sc_enc->emul) return;
	if ((str[0]=='0') && (str[1]=='x' || str[1]=='X')) {
		val = (u32) strtoul(sc_enc->token, (char **) NULL, 16);
	} else if (str[0]=='0' && isdigit(str[1])) {
		val = (u32) strtoul(str, (char **) NULL, 8);
	} else if (isdigit(str[0])) {
		val = (u32) strtoul(str, (char **) NULL, 10);
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: %s is not an integer\n", str));
		sc_enc->err = GF_BAD_PARAM;
		return;
	}
	nbBits = gf_get_bit_size(val);
	GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, nbBits, 5, "nbBitsInteger", NULL);
	GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, val, nbBits, "value", sc_enc->token);
}

u32 SFE_LoadExpression(ScriptEnc *sc_enc, u32 *expr_sep)
{
	Bool is_var = GF_FALSE;
	u32 close_code, open_code;
	u32 count = 0;
	u32 nbExpr = 1;
	u32 nbIndir = 0;
	expr_sep[0] = 0;

	sc_enc->expr_toks_len = 0;

	while ( (sc_enc->token_code != TOK_SEMICOLON) && (sc_enc->token_code != TOK_RIGHT_CURVE) ) {
		switch (sc_enc->token_code) {
		case TOK_CONDTEST:
			nbIndir ++;
			break;
		case TOK_CONDSEP:
			if (nbIndir > 0) nbIndir--;
			/*'case'*/
			else {
				goto break_loop;
			}
			break;
		case TOK_IDENTIFIER:
		case TOK_NUMBER:
		case TOK_STRING:
		case TOK_BOOLEAN:
			gf_list_add(sc_enc->id_buf, gf_strdup(sc_enc->token));
			break;
		case TOK_FUNCTION:
			goto break_loop;
		}

		if (sc_enc->token_code==TOK_VAR) is_var = GF_TRUE;
		if (!is_var || (sc_enc->token_code!=TOK_COMMA)) {
			sc_enc->expr_toks[sc_enc->expr_toks_len] = sc_enc->token_code;
			sc_enc->expr_toks_len++;
		}

		open_code = sc_enc->token_code;
		close_code = 0;
		if (sc_enc->token_code == TOK_LEFT_CURVE) close_code = TOK_RIGHT_CURVE;
		else if (sc_enc->token_code == TOK_LEFT_BRACKET) close_code = TOK_RIGHT_BRACKET;
		else if (sc_enc->token_code == TOK_LEFT_BRACE) close_code = TOK_RIGHT_BRACE;

		/*other expr*/
		if ((sc_enc->token_code == TOK_COMMA) && (sc_enc->expr_toks[0] != TOK_VAR) ) {
			expr_sep[nbExpr++] = sc_enc->expr_toks_len - 1;
		}
		/*sub-expr*/
		else if (close_code) {
			count++;
			do {
				SFE_NextToken(sc_enc);
				if ((sc_enc->token_code == TOK_IDENTIFIER) || (sc_enc->token_code == TOK_NUMBER)
				        || (sc_enc->token_code == TOK_STRING) || (sc_enc->token_code == TOK_BOOLEAN) ) {
					gf_list_add(sc_enc->id_buf, gf_strdup(sc_enc->token));
				}
				sc_enc->expr_toks[sc_enc->expr_toks_len] = sc_enc->token_code;
				sc_enc->expr_toks_len++;
				if (sc_enc->token_code == open_code) count++;
				else if (sc_enc->token_code == close_code) count--;
			} while ( (sc_enc->token_code != close_code) || count);
		}
		SFE_NextToken(sc_enc);

		if (sc_enc->err) break;
	}

break_loop:
	if (sc_enc->err) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: end of compoundExpression not found\n"));
		return 0;
	}
	expr_sep[nbExpr] = sc_enc->expr_toks_len;
	if ((sc_enc->token_code == TOK_IDENTIFIER) || (sc_enc->token_code == TOK_NUMBER)
	        || (sc_enc->token_code == TOK_STRING) || (sc_enc->token_code == TOK_BOOLEAN) ) {
		gf_list_add(sc_enc->id_buf, gf_strdup(sc_enc->token));
	}

	if ((sc_enc->token_code != TOK_CONDSEP) && (sc_enc->token_code != TOK_RIGHT_BRACE) && (sc_enc->expr_toks[0] != TOK_VAR)) {
		sc_enc->expr_toks[sc_enc->expr_toks_len] = sc_enc->token_code;
		sc_enc->expr_toks_len++;
	}
	return nbExpr;
}

u32 MoveToToken(ScriptEnc *sc_enc, u32 endTok, u32 cur, u32 end);

u32 SFE_ScanExpression(ScriptEnc *sc_enc, u32 start, u32 end, u32 *expr_sep)
{
	u32 curTok;
	u32 n = start;
	u32 nbExpr = 1;

	expr_sep[0] = start;
	while (n<end) {
		curTok = sc_enc->expr_toks[n++];
		if (curTok == TOK_LEFT_CURVE) {
			n = MoveToToken(sc_enc, TOK_RIGHT_CURVE, n-1, end);
			n++;
		} else if (curTok == TOK_LEFT_BRACKET) {
			n = MoveToToken(sc_enc, TOK_RIGHT_BRACKET, n-1, end);
			n++;
		} else if (curTok == TOK_COMMA) {
			expr_sep[nbExpr++] = n-1;
		}
	}
	expr_sep[nbExpr] = end;
	return nbExpr;
}

u32 SFE_Expression(ScriptEnc *sc_enc, u32 start, u32 end, Bool memberAccess);

void SFE_CompoundExpression(ScriptEnc *sc_enc, u32 start, u32 end, u32 isPar)
{
	u32 nbExp, i;
	/*filled by indexes of ',' expressions in the expr_tok buffer*/
	u32 expr_sep[MAX_NUM_EXPR];

	if (sc_enc->err) return;

	if (end==0) {
		/*load expressions , eg "a ? ((a>b) ? 1 : 0) : 0" */
		nbExp = SFE_LoadExpression(sc_enc, expr_sep);
	} else {
		/*load sub-expression from loaded expression set*/
		nbExp = SFE_ScanExpression(sc_enc, start, end, expr_sep);
	}

	SFE_Expression(sc_enc, expr_sep[0], expr_sep[1], GF_FALSE);
	for (i=1; i<nbExp; i++) {
		SFE_WRITE_INT(sc_enc, 1, 1, isPar ? "hasParam" : "hasExpression", NULL);
		SFE_Expression(sc_enc, expr_sep[i]+1, expr_sep[i+1], GF_FALSE);
	}
	SFE_WRITE_INT(sc_enc, 0, 1, isPar ? "hasParam" : "hasExpression", NULL);
}

void SFE_OptionalExpression(ScriptEnc *sc_enc)
{
	if (sc_enc->token_code != TOK_SEMICOLON) {
		SFE_WRITE_INT(sc_enc, 1, 1, "hasCompoundExpression", NULL);
		SFE_CompoundExpression(sc_enc, 0, 0, 0);
	} else {
		SFE_WRITE_INT(sc_enc, 0, 1, "hasCompoundExpression", NULL);
	}
}

void SFE_IfStatement(ScriptEnc *sc_enc)
{
	char *buf_bck;
	u32 tok_bck;
	SFE_NextToken(sc_enc);
	SFE_CheckToken(sc_enc, TOK_LEFT_CURVE);
	SFE_NextToken(sc_enc);
	SFE_CompoundExpression(sc_enc, 0, 0, 0);
	SFE_CheckToken(sc_enc, TOK_RIGHT_CURVE);
	SFE_StatementBlock(sc_enc);

	buf_bck = sc_enc->cur_buf;
	tok_bck = sc_enc->token_code;
	SFE_NextToken(sc_enc);
	if (sc_enc->token_code == TOK_ELSE) {
		SFE_WRITE_INT(sc_enc, 1, 1, "hasELSEStatement", NULL);
		SFE_StatementBlock(sc_enc);
	} else {
		SFE_WRITE_INT(sc_enc, 0, 1, "hasELSEStatement", NULL);
		sc_enc->cur_buf = buf_bck;
		sc_enc->token_code = tok_bck;
	}
}

void SFE_ForStatement(ScriptEnc *sc_enc)
{
	SFE_NextToken(sc_enc);
	SFE_CheckToken(sc_enc, TOK_LEFT_CURVE);

	SFE_NextToken(sc_enc);
	SFE_OptionalExpression(sc_enc);
	SFE_CheckToken(sc_enc, TOK_SEMICOLON);

	SFE_NextToken(sc_enc);
	SFE_OptionalExpression(sc_enc);
	SFE_CheckToken(sc_enc, TOK_SEMICOLON);

	SFE_NextToken(sc_enc);
	SFE_OptionalExpression(sc_enc);
	SFE_CheckToken(sc_enc, TOK_RIGHT_CURVE);

	SFE_StatementBlock(sc_enc);
}

void SFE_WhileStatement(ScriptEnc *sc_enc)
{
	SFE_NextToken(sc_enc);
	SFE_CheckToken(sc_enc, TOK_LEFT_CURVE);
	SFE_NextToken(sc_enc);
	SFE_CompoundExpression(sc_enc, 0, 0, 0);
	SFE_CheckToken(sc_enc, TOK_RIGHT_CURVE);

	SFE_StatementBlock(sc_enc);
}

void SFE_ReturnStatement(ScriptEnc *sc_enc)
{
	SFE_NextToken(sc_enc);
	if (sc_enc->token_code != TOK_SEMICOLON) {
		SFE_WRITE_INT(sc_enc, 1, 1, "returnValue", NULL);
		SFE_CompoundExpression(sc_enc, 0, 0, 0);
	} else {
		SFE_WRITE_INT(sc_enc, 0, 1, "returnValue", NULL);
	}
}

void SFE_CaseBlock(ScriptEnc *sc_enc)
{
	SFE_WRITE_INT(sc_enc, 1, 1, "isCompoundStatement", NULL);
	SFE_NextToken(sc_enc);
	if (sc_enc->token_code == TOK_LEFT_BRACE) {
		SFE_NextToken(sc_enc);
		while (sc_enc->token_code != TOK_RIGHT_BRACE) {
			SFE_WRITE_INT(sc_enc, 1, 1, "hasStatement", NULL);
			SFE_Statement(sc_enc);
			SFE_NextToken(sc_enc);
		}
		SFE_NextToken(sc_enc);
	}
	while ((sc_enc->token_code != TOK_CASE) && (sc_enc->token_code != TOK_DEFAULT) && (sc_enc->token_code != TOK_RIGHT_BRACE)) {
		SFE_WRITE_INT(sc_enc, 1, 1, "hasStatement", NULL);
		SFE_Statement(sc_enc);
		SFE_NextToken(sc_enc);
	}
	SFE_WRITE_INT(sc_enc, 0, 1, "hasStatement", NULL);
}

u32 SFE_PutCaseInteger(ScriptEnc *sc_enc, char *str, u32 nbBits)
{
	u32 val = 0;
	if ((str[0]=='0') && (str[1]=='x' || str[1]=='X')) {
		val = (u32) strtoul(sc_enc->token, (char **) NULL, 16);
	} else if (str[0]=='0' && isdigit(str[1])) {
		val = (u32) strtoul(str, (char **) NULL, 8);
	} else if (isdigit(str[0])) {
		val = (u32) strtoul(str, (char **) NULL, 10);
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: %s is not an integer\n", str));
		sc_enc->err = GF_BAD_PARAM;
		return 0;
	}
	if (!sc_enc->emul) {
		GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, val, nbBits, "value", sc_enc->token);
	} else {
		nbBits = gf_get_bit_size(val);
	}
	return nbBits;
}

void SFE_SwitchStatement(ScriptEnc *sc_enc)
{
	u32 nbBits, maxBits = 0;
	Bool prev_emu;
	char *buf_bck;
	u32 tok_bck;

	SFE_NextToken(sc_enc);
	SFE_CheckToken(sc_enc, TOK_LEFT_CURVE);

	SFE_NextToken(sc_enc);
	SFE_CompoundExpression(sc_enc, 0, 0, 0);
	SFE_CheckToken(sc_enc, TOK_RIGHT_CURVE);

	SFE_NextToken(sc_enc);
	SFE_CheckToken(sc_enc, TOK_LEFT_BRACE);

	/*first pass in emul*/
	buf_bck = sc_enc->cur_buf;
	tok_bck = sc_enc->token_code;
	prev_emu = sc_enc->emul;
	sc_enc->emul = GF_TRUE;

	SFE_NextToken(sc_enc);
	while (sc_enc->token_code == TOK_CASE) {
		SFE_NextToken(sc_enc);
		SFE_CheckToken(sc_enc, TOK_NUMBER);
		nbBits = SFE_PutCaseInteger(sc_enc, sc_enc->token, 0);
		if (maxBits<nbBits) maxBits = nbBits;

		SFE_NextToken(sc_enc);
		SFE_CheckToken(sc_enc, TOK_CONDSEP);

		SFE_CaseBlock(sc_enc);
		SFE_WRITE_INT(sc_enc, (sc_enc->token_code == TOK_CASE) ? 1 : 0, 1, "hasMoreCases", NULL);
	}

	/*second pass in parent mode*/
	sc_enc->cur_buf = buf_bck;
	sc_enc->token_code = tok_bck;
	sc_enc->emul = prev_emu;
	maxBits ++;
	SFE_WRITE_INT(sc_enc, maxBits, 5, "caseNbBits", NULL);

	SFE_NextToken(sc_enc);
	while (sc_enc->token_code == TOK_CASE) {
		SFE_NextToken(sc_enc);
		SFE_CheckToken(sc_enc, TOK_NUMBER);
		SFE_PutCaseInteger(sc_enc, sc_enc->token, maxBits);

		SFE_NextToken(sc_enc);
		SFE_CheckToken(sc_enc, TOK_CONDSEP);

		SFE_CaseBlock(sc_enc);
		SFE_WRITE_INT(sc_enc, (sc_enc->token_code == TOK_CASE) ? 1 : 0, 1, "hasMoreCases", NULL);
	}

	if (sc_enc->token_code == TOK_DEFAULT) {
		SFE_WRITE_INT(sc_enc, 1, 1, "hasDefault", NULL);
		SFE_NextToken(sc_enc);
		SFE_CheckToken(sc_enc, TOK_CONDSEP);
		SFE_CaseBlock(sc_enc);
	} else {
		SFE_WRITE_INT(sc_enc, 0, 1, "hasDefault", NULL);
	}
	SFE_CheckToken(sc_enc, TOK_RIGHT_BRACE);
}

void SFE_Statement(ScriptEnc *sc_enc)
{
	switch (sc_enc->token_code) {
	case TOK_IF:
		SFE_WRITE_INT(sc_enc, ST_IF, NUMBITS_STATEMENT, "statementType", "if");
		SFE_IfStatement(sc_enc);
		break;
	case TOK_FOR:
		SFE_WRITE_INT(sc_enc, ST_FOR, NUMBITS_STATEMENT, "statementType", "for");
		SFE_ForStatement(sc_enc);
		break;
	case TOK_WHILE:
		SFE_WRITE_INT(sc_enc, ST_WHILE, NUMBITS_STATEMENT, "statementType", "while");
		SFE_WhileStatement(sc_enc);
		break;
	case TOK_RETURN:
		SFE_WRITE_INT(sc_enc, ST_RETURN, NUMBITS_STATEMENT, "statementType", "return");
		SFE_ReturnStatement(sc_enc);
		break;
	case TOK_BREAK:
		SFE_WRITE_INT(sc_enc, ST_BREAK, NUMBITS_STATEMENT, "statementType", "break");
		SFE_NextToken(sc_enc);
		break;
	case TOK_CONTINUE:
		SFE_WRITE_INT(sc_enc, ST_CONTINUE, NUMBITS_STATEMENT, "statementType", "continue");
		SFE_NextToken(sc_enc);
		break;
	case TOK_SWITCH:
		SFE_WRITE_INT(sc_enc, ST_SWITCH, NUMBITS_STATEMENT, "statementType", "while");
		SFE_SwitchStatement(sc_enc);
		break;
	default:
		SFE_WRITE_INT(sc_enc, ST_COMPOUND_EXPR, NUMBITS_STATEMENT, "statementType", "compoundExpr");
		SFE_CompoundExpression(sc_enc, 0, 0, 0);
		break;
	}
}

void SFE_Statements(ScriptEnc *sc_enc)
{
	while (1) {
		if (!SFE_NextToken(sc_enc)) return;
		if (sc_enc->token_code == TOK_RIGHT_BRACE) break;
		SFE_WRITE_INT(sc_enc, 1, 1, "hasStatement", NULL);
		SFE_Statement(sc_enc);
	}
	SFE_WRITE_INT(sc_enc, 0, 1, "hasStatement", NULL);
}

void SFE_StatementBlock(ScriptEnc *sc_enc)
{
	if (!SFE_NextToken(sc_enc)) return;
	if (sc_enc->token_code == TOK_LEFT_BRACE) {
		SFE_WRITE_INT(sc_enc, 1, 1, "isCompoundStatement", NULL);
		SFE_Statements(sc_enc);
	} else {
		SFE_WRITE_INT(sc_enc, 0, 1, "isCompoundStatement", NULL);
		SFE_Statement(sc_enc);
	}
}
void SFE_Function(ScriptEnc *sc_enc)
{
	char szName[1000];

	SFE_NextToken(sc_enc);
	SFE_CheckToken(sc_enc, TOK_FUNCTION);

	SFE_NextToken(sc_enc);
	SFE_CheckToken(sc_enc, TOK_IDENTIFIER);
	strcpy(szName, sc_enc->token);
	SFE_PutIdentifier(sc_enc, sc_enc->token);

	SFE_NextToken(sc_enc);
	SFE_CheckToken(sc_enc, TOK_LEFT_CURVE);

	SFE_Arguments(sc_enc);
	SFE_StatementBlock(sc_enc);

	if (sc_enc->err) GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: Error while parsing function %s\n", szName));
}


GF_Err SFScript_Encode(GF_BifsEncoder *codec, SFScript *script_field, GF_BitStream *bs, GF_Node *n)
{
	char *ptr;
	ScriptEnc sc_enc;
	if (gf_node_get_tag(n) != TAG_MPEG4_Script) return GF_NON_COMPLIANT_BITSTREAM;

	memset(&sc_enc, 0, sizeof(ScriptEnc));
	sc_enc.codec = codec;
	sc_enc.script = n;
	sc_enc.bs = bs;
	sc_enc.identifiers = gf_list_new();
	sc_enc.id_buf = gf_list_new();
	sc_enc.err = GF_OK;

	if (codec->is_encoding_command) {
		GF_BIFS_WRITE_INT(codec, bs, 1, 1, "Script::isList", NULL);
		GF_BIFS_WRITE_INT(codec, bs, 1, 1, "end", NULL);
	} else {
		EncScriptFields(&sc_enc);
	}
	/*reserevd*/
	GF_BIFS_WRITE_INT(codec, bs, 1, 1, "reserved", NULL);

	if (script_field) {
		sc_enc.cur_buf = (char *)script_field->script_text;
	} else if (((M_Script*)n)->url.count) {
		sc_enc.cur_buf = (char *)((M_Script*)n)->url.vals[0].script_text;
	}
	if (sc_enc.cur_buf) {
		if (!strnicmp(sc_enc.cur_buf, "javascript:", 11)
		        || !strnicmp(sc_enc.cur_buf, "vrmlscript:", 11)
		        || !strnicmp(sc_enc.cur_buf, "ECMAScript:", 11)
		   ) {
			sc_enc.cur_buf += 11;
		} else if (!strnicmp(sc_enc.cur_buf, "mpeg4script:", 12) ) {
			sc_enc.cur_buf += 12;
		}
	}

	/*encode functions*/
	while (sc_enc.cur_buf && sc_enc.cur_buf[0] && (sc_enc.cur_buf[0]!='}')) {
		if (strchr("\r\n\t ", sc_enc.cur_buf[0]) ) {
			sc_enc.cur_buf++;
			continue;
		}
		GF_BIFS_WRITE_INT(codec, bs, 1, 1, "hasFunction", NULL);
		SFE_Function(&sc_enc);
		if (sc_enc.err) break;
	}
	GF_BIFS_WRITE_INT(codec, bs, 0, 1, "hasFunction", NULL);

	//clean up
	while (gf_list_count(sc_enc.identifiers)) {
		ptr = (char *)gf_list_get(sc_enc.identifiers, 0);
		gf_list_rem(sc_enc.identifiers, 0);
		gf_free(ptr);
	}
	gf_list_del(sc_enc.identifiers);
	/*in case of error this is needed*/
	while (gf_list_count(sc_enc.id_buf)) {
		ptr = (char *)gf_list_get(sc_enc.id_buf, 0);
		gf_list_rem(sc_enc.id_buf, 0);
		gf_free(ptr);
	}
	gf_list_del(sc_enc.id_buf);

	return sc_enc.err;
}


void SFE_PutReal(ScriptEnc *sc_enc, char *str)
{
	u32 i;
	size_t length = strlen(str);
	for (i=0; i<length; i++) {
		s32 c = str[i];
		if (c >= '0' && c <= '9')
		{
			SFE_WRITE_INT(sc_enc, c-'0', 4, "floatChar", "Digital");
		}
		else if (c == '.')
		{
			SFE_WRITE_INT(sc_enc, 10, 4, "floatChar", "Decimal Point");
		}
		else if (c == 'e' || c == 'E')
		{
			SFE_WRITE_INT(sc_enc, 11, 4, "floatChar", "Exponential");
		}
		else if (c == '-')
		{
			SFE_WRITE_INT(sc_enc, 12, 4, "floatChar", "Sign");
		}
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: %s is not a real number\n", str));
			sc_enc->err = GF_BAD_PARAM;
			return;
		}
	}
	SFE_WRITE_INT(sc_enc, 15, 4, "floatChar", "End Symbol");
}

void SFE_PutNumber(ScriptEnc *sc_enc, char *str)
{
	if (strpbrk(str,".eE-") == 0) {
		SFE_WRITE_INT(sc_enc, 1, 1, "isInteger", "integer");
		SFE_PutInteger(sc_enc, str);
	} else {
		SFE_WRITE_INT(sc_enc, 0, 1, "isInteger", "real");
		SFE_PutReal(sc_enc, str);
	}
}

void SFE_PutBoolean(ScriptEnc *sc_enc, char *str)
{
	u32 v = 1;
	if (!stricmp(str, "false") || !strcmp(str, "0")) v = 0;
	SFE_WRITE_INT(sc_enc, v, 1, "value", "bolean");
}


#define CHECK_TOK(x) \
	if (curTok != x) { \
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: Token %s read, %s expected\n", tok_names[curTok], tok_names[x])); \
		sc_enc->err = GF_BAD_PARAM;	\
	}	\


u32 TOK_To_ET(u32 tok)
{
	switch(tok) {
	case TOK_INCREMENT:
		return ET_INCREMENT;
	case TOK_DECREMENT:
		return ET_DECREMENT;
	case TOK_NOT:
		return ET_NOT;
	case TOK_ONESCOMP:
		return ET_ONESCOMP;
	case TOK_MULTIPLY :
		return ET_MULTIPLY;
	case TOK_DIVIDE :
		return ET_DIVIDE;
	case TOK_MOD :
		return ET_MOD;
	case TOK_PLUS :
		return ET_PLUS;
	case TOK_MINUS :
		return ET_MINUS;
	case TOK_LSHIFT :
		return ET_LSHIFT;
	case TOK_RSHIFT :
		return ET_RSHIFT;
	case TOK_RSHIFTFILL :
		return ET_RSHIFTFILL;
	case TOK_LT :
		return ET_LT;
	case TOK_LE :
		return ET_LE;
	case TOK_GT :
		return ET_GT;
	case TOK_GE :
		return ET_GE;
	case TOK_EQ :
		return ET_EQ;
	case TOK_NE :
		return ET_NE;
	case TOK_AND :
		return ET_AND;
	case TOK_XOR :
		return ET_XOR;
	case TOK_OR :
		return ET_OR;
	case TOK_LAND :
		return ET_LAND;
	case TOK_LOR :
		return ET_LOR;
	case TOK_CONDTEST :
		return ET_CONDTEST;
	case TOK_ASSIGN :
		return ET_ASSIGN;
	case TOK_PLUSEQ :
		return ET_PLUSEQ;
	case TOK_MINUSEQ :
		return ET_MINUSEQ;
	case TOK_MULTIPLYEQ :
		return ET_MULTIPLYEQ;
	case TOK_DIVIDEEQ :
		return ET_DIVIDEEQ;
	case TOK_MODEQ :
		return ET_MODEQ;
	case TOK_LSHIFTEQ :
		return ET_LSHIFTEQ;
	case TOK_RSHIFTEQ :
		return ET_RSHIFTEQ;
	case TOK_RSHIFTFILLEQ :
		return ET_RSHIFTFILLEQ;
	case TOK_ANDEQ :
		return ET_ANDEQ;
	case TOK_XOREQ :
		return ET_XOREQ;
	case TOK_OREQ :
		return ET_OREQ;
	case TOK_FUNCTION:
		return ET_FUNCTION_CALL;
	case TOK_VAR:
		return ET_VAR;
	default:
		gf_assert(0);
		return (u32) -1;
	}
}

#ifndef GPAC_DISABLE_LOG
static const char *expr_name[] = {
	"ET_CURVED_EXPR",
	"ET_NEGATIVE",
	"ET_NOT",
	"ET_ONESCOMP",
	"ET_INCREMENT",
	"ET_DECREMENT",
	"ET_POST_INCREMENT",
	"ET_POST_DECREMENT",
	"ET_CONDTEST",
	"ET_STRING",
	"ET_NUMBER",
	"ET_IDENTIFIER",
	"ET_FUNCTION_CALL",
	"ET_NEW",
	"ET_OBJECT_MEMBER_ACCESS",
	"ET_OBJECT_METHOD_CALL",
	"ET_ARRAY_DEREFERENCE",
	"ET_ASSIGN",
	"ET_PLUSEQ",
	"ET_MINUSEQ",
	"ET_MULTIPLYEQ",
	"ET_DIVIDEEQ",
	"ET_MODEQ",
	"ET_ANDEQ",
	"ET_OREQ",
	"ET_XOREQ",
	"ET_LSHIFTEQ",
	"ET_RSHIFTEQ",
	"ET_RSHIFTFILLEQ",
	"ET_EQ",
	"ET_NE",
	"ET_LT",
	"ET_LE",
	"ET_GT",
	"ET_GE",
	"ET_PLUS",
	"ET_MINUS",
	"ET_MULTIPLY",
	"ET_DIVIDE",
	"ET_MOD",
	"ET_LAND",
	"ET_LOR",
	"ET_AND",
	"ET_OR",
	"ET_XOR",
	"ET_LSHIFT",
	"ET_RSHIFT",
	"ET_RSHIFTFILL",
	"ET_BOOLEAN",
	"ET_VAR",
	"NUMBER_OF_EXPR_TYPE"
};
#endif


#define NUMBER_OF_RANK	15
static s32 ET_Rank[NUMBER_OF_EXPR_TYPE] =
{
	1,// ET_CURVED_EXPR
	2,// ET_NEGATIVE
	2,// ET_NOT
	2,// ET_ONESCOMP
	2,// ET_INCREMENT
	2,// ET_DECREMENT
	2,// ET_POST_INCREMENT
	2,// ET_POST_DECREMENT
	14,// ET_CONDTEST
	0,// ET_STRING
	0,// ET_NUMBER
	0,// ET_IDENTIFIER
	1,// ET_FUNCTION_CALL
	2,// ET_NEW
	1,// ET_OBJECT_MEMBER_ACCESS
	1,// ET_OBJECT_METHOD_CALL
	1,// ET_ARRAY_DEREFERENCE
	14,// ET_ASSIGN
	14,// ET_PLUSEQ
	14,// ET_MINUSEQ
	14,// ET_MULTIPLYEQ
	14,// ET_DIVIDEEQ
	14,// ET_MODEQ
	14,// ET_ANDEQ
	14,// ET_OREQ
	14,// ET_XOREQ
	14,// ET_LSHIFTEQ
	14,// ET_RSHIFTEQ
	14,// ET_RSHIFTFILLEQ
	8,// ET_EQ
	9,// ET_NE
	6,// ET_LT
	6,// ET_LE
	7,// ET_GT
	7,// ET_GE
	5,// ET_PLUS
	5,// ET_MINUS
	3,// ET_MULTIPLY
	3,// ET_DIVIDE
	3,// ET_MOD
	13,// ET_LAND
	14,// ET_LOR
	10,// ET_AND
	12,// ET_OR
	11,// ET_XOR
	5,// ET_LSHIFT
	6,// ET_RSHIFT
	6,// ET_RSHIFTFILL
	0, // ET_BOOLEAN
	14 // ET_VAR

	/*
		0, 0, 0,			// variable, number, string
		1, 1, 1, 1, 1,			// curved expr, call, member, array
		2, 2, 2, 2, 2, 2, 2, 2,		// unary operator
		3, 3, 3,			// multiply, divide, mod
		4, 4,				// add, subtract
		5, 5, 5,			// bitwise shift
		6, 6, 6, 6,			// relational
		7, 7,				// equality
		8,				// bitwise and
		9,				// bitwise xor
		10,				// bitwise or
		11,				// logical and
		12,				// logical or
		13,				// conditional
		14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14	// assignment
		*/
};
static s32 ET_leftAssoc[NUMBER_OF_RANK] = {1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0};

u32 MoveToToken(ScriptEnc *sc_enc, u32 endTok, u32 cur, u32 end)
{
	u32 cnt = 0;
	u32 startTok = TOK_EOF, curTok;

	if (endTok == TOK_RIGHT_CURVE) startTok = TOK_LEFT_CURVE;
	else if (endTok == TOK_RIGHT_BRACKET) startTok = TOK_LEFT_BRACKET;
	else if (endTok == TOK_RIGHT_BRACE) startTok = TOK_LEFT_BRACE;
	else if (endTok == TOK_CONDSEP) startTok = TOK_CONDTEST;
	else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: illegal MoveToToken %s\n", tok_names[endTok]));
		sc_enc->err = GF_BAD_PARAM;
		return (u32) -1;
	}
	do {
		curTok = sc_enc->expr_toks[cur++];
		if (curTok == startTok) cnt++;
		else if (curTok == endTok) cnt--;
	} while ( (curTok != endTok || cnt) && cur < end);
	if (curTok==endTok && cnt==0) return cur-1;
	return (u32) -1;
}


void SFE_FunctionCall(ScriptEnc *sc_enc, u32 start, u32 end);
void SFE_Params(ScriptEnc *sc_enc, u32 start, u32 end);
void SFE_ConditionTest(ScriptEnc *sc_enc, u32 start, u32 op, u32 end);
void SFE_ObjectConstruct(ScriptEnc *sc_enc, u32 start, u32 op, u32 end);
void SFE_ArrayDereference(ScriptEnc *sc_enc, u32 start, u32 op, u32 end);
void SFE_ObjectMethodCall(ScriptEnc *sc_enc, u32 start, u32 op, u32 end);
void SFE_ObjectMemberAccess(ScriptEnc *sc_enc, u32 start, u32 op, u32 end);

u32 SFE_Expression(ScriptEnc *sc_enc, u32 start, u32 end, Bool memberAccess)
{
	char *str;
	u32 n = start;
	u32 curPos = 0, finalPos = 0;
	u32 curTok, prevTok;
	u32 curExpr = 0, expr = 0;
	u32 curRank, maxRank=0;

	if (sc_enc->err) return 0;

	curTok = prevTok = sc_enc->expr_toks[n];
	do {
		curPos = n;
		switch (curTok) {
		case TOK_IDENTIFIER:
			curExpr = ET_IDENTIFIER;
			break;
		case TOK_NUMBER:
			curExpr = ET_NUMBER;
			break;
		case TOK_BOOLEAN:
			curExpr = ET_BOOLEAN;
			break;
		case TOK_STRING:
			curExpr = ET_STRING;
			break;
		case TOK_LEFT_CURVE:
			if (prevTok == TOK_IDENTIFIER) curExpr = ET_FUNCTION_CALL;
			else curExpr = ET_CURVED_EXPR;
			n = MoveToToken(sc_enc, TOK_RIGHT_CURVE, n, end);
			curTok = TOK_RIGHT_CURVE;
			break;
		case TOK_LEFT_BRACKET:
			curExpr = ET_ARRAY_DEREFERENCE;
			n = MoveToToken(sc_enc, TOK_RIGHT_BRACKET, n, end);
			curTok = TOK_RIGHT_BRACKET;
			break;
		case TOK_PERIOD:
			curTok = sc_enc->expr_toks[++n];
			CHECK_TOK(TOK_IDENTIFIER);
			if (sc_enc->expr_toks[n+1] == TOK_LEFT_CURVE) {
				curExpr = ET_OBJECT_METHOD_CALL;
				n = MoveToToken(sc_enc, TOK_RIGHT_CURVE, n+1, end);
				curTok = TOK_RIGHT_CURVE;
			} else {
				curExpr = ET_OBJECT_MEMBER_ACCESS;
			}
			break;
		case TOK_NEW:
			curExpr = ET_NEW;
			curTok = sc_enc->expr_toks[++n];
			CHECK_TOK(TOK_IDENTIFIER);
			curTok = sc_enc->expr_toks[++n];
			CHECK_TOK(TOK_LEFT_CURVE);
			n = MoveToToken(sc_enc, TOK_RIGHT_CURVE, n, end);
			curTok = TOK_RIGHT_CURVE;
			break;
		case TOK_FUNCTION:
			curExpr = ET_FUNCTION_ASSIGN;
			break;
		case TOK_PLUS:
			if (
			    prevTok==TOK_RIGHT_CURVE || prevTok==TOK_RIGHT_BRACKET ||
			    prevTok==TOK_IDENTIFIER || prevTok==TOK_NUMBER ||
			    prevTok==TOK_STRING || prevTok==TOK_INCREMENT ||
			    prevTok==TOK_DECREMENT
			) {
				curExpr = ET_PLUS;
			} else {
				goto skip_token;
			}
			break;
		case TOK_MINUS:
			if (
			    prevTok==TOK_RIGHT_CURVE || prevTok==TOK_RIGHT_BRACKET ||
			    prevTok==TOK_IDENTIFIER || prevTok==TOK_NUMBER ||
			    prevTok==TOK_STRING || prevTok==TOK_INCREMENT ||
			    prevTok==TOK_DECREMENT
			) {
				curExpr = ET_MINUS;
			} else {
				curExpr = ET_NEGATIVE;
				curTok = TOK_NEGATIVE;
			}
			break;
		case TOK_INCREMENT:
			curExpr = ET_INCREMENT;
			break;
		case TOK_DECREMENT:
			curExpr = ET_DECREMENT;
			break;
		case TOK_NOT:
			curExpr = ET_NOT;
			break;
		case TOK_ONESCOMP:
			curExpr = ET_ONESCOMP;
			break;
		case TOK_CONDTEST:
			curExpr = ET_CONDTEST;
			break;
		case TOK_CONDSEP:
			break;
		case TOK_LEFT_BRACE:
			curExpr = ET_CURVED_EXPR;
			n = MoveToToken(sc_enc, TOK_RIGHT_BRACE, n, end);
			curTok = TOK_RIGHT_BRACE;
			break;
			break;
		default:

			if (curTok && (curTok != TOK_VAR)
			        && (curTok < TOK_MULTIPLY || curTok > TOK_OREQ)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: illegal token %s read\n", tok_names[curTok]));
				sc_enc->err = GF_BAD_PARAM;
				return 0;
			}
			curExpr = TOK_To_ET(curTok);
		}

		if (curTok == TOK_CONDSEP) {
			prevTok = curTok;
			curTok = sc_enc->expr_toks[++n];
			continue;
		}
		curRank = ET_Rank[curExpr];
		if (curRank > maxRank) {
			maxRank = curRank;
			expr = curExpr;
			finalPos = curPos;
		} else if (curRank == maxRank && ET_leftAssoc[curRank]) {
			expr = curExpr;
			finalPos = curPos;
		}
		prevTok = curTok;
skip_token:
		curTok = sc_enc->expr_toks[++n];
	} while (n<end);

	if (expr == ET_INCREMENT) {
		if (finalPos==start) {}
		else if (finalPos==end-1) expr = ET_POST_INCREMENT;
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: illegal Increment expression\n"));
			sc_enc->err = GF_BAD_PARAM;
			return expr;
		}
	} else if (expr == ET_DECREMENT) {
		if (finalPos==start) {}
		else if (finalPos==end-1) expr = ET_POST_DECREMENT;
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: illegal Decrement expression\n"));
			sc_enc->err = GF_BAD_PARAM;
			return expr;
		}
	}

	SFE_WRITE_INT(sc_enc, expr, NUMBITS_EXPR_TYPE, "expressionType", (char *) expr_name[expr]);

	switch (expr) {
	case ET_MULTIPLY:
	case ET_DIVIDE:
	case ET_MOD:
	case ET_PLUS:
	case ET_MINUS:
	case ET_LSHIFT:
	case ET_RSHIFT:
	case ET_RSHIFTFILL:
	case ET_LT:
	case ET_LE:
	case ET_GT:
	case ET_GE:
	case ET_EQ:
	case ET_NE:
	case ET_AND:
	case ET_XOR:
	case ET_OR:
	case ET_LAND:
	case ET_LOR:
		SFE_Expression(sc_enc, start, finalPos, GF_FALSE);
		SFE_Expression(sc_enc, finalPos+1, end, GF_FALSE);
		break;
	case ET_ASSIGN:
	case ET_PLUSEQ:
	case ET_MINUSEQ:
	case ET_MULTIPLYEQ:
	case ET_DIVIDEEQ:
	case ET_MODEQ:
	case ET_LSHIFTEQ:
	case ET_RSHIFTEQ:
	case ET_RSHIFTFILLEQ:
	case ET_ANDEQ:
	case ET_XOREQ:
	case ET_OREQ:
	{
		u32 ret = SFE_Expression(sc_enc, start, finalPos, GF_FALSE);
		if ( ret != ET_IDENTIFIER && ret != ET_OBJECT_MEMBER_ACCESS && ret != ET_ARRAY_DEREFERENCE ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: LeftVariable expected, %s returned\n", expr_name[ret]));
			sc_enc->err = GF_BAD_PARAM;
			return expr;
		}
		SFE_Expression(sc_enc, finalPos+1, end, GF_FALSE);
	}
	break;

	case ET_IDENTIFIER:
		str = (char *)gf_list_get(sc_enc->id_buf, 0);
		gf_list_rem(sc_enc->id_buf, 0);
		/*TODO: when accessing member, we should try to translate proto fields into _fieldALL when not
		using USENAMEs...*/
		if (memberAccess) {
		}
		SFE_PutIdentifier(sc_enc, str);
		gf_free(str);
		break;
	case ET_NUMBER:
		str = (char *)gf_list_get(sc_enc->id_buf, 0);
		gf_list_rem(sc_enc->id_buf, 0);
		SFE_PutNumber(sc_enc, str);
		gf_free(str);
		break;
	case ET_BOOLEAN:
		str = (char *)gf_list_get(sc_enc->id_buf, 0);
		gf_list_rem(sc_enc->id_buf, 0);
		SFE_PutBoolean(sc_enc, str);
		gf_free(str);
		break;
	case ET_VAR:
		while (1) {
			str = (char *)gf_list_get(sc_enc->id_buf, 0);
			if (!str) break;
			gf_list_rem(sc_enc->id_buf, 0);
			GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, 1, 1, "hasArgument", NULL);
			SFE_PutIdentifier(sc_enc, str);
			gf_free(str);
		}
		GF_BIFS_WRITE_INT(sc_enc->codec, sc_enc->bs, 0, 1, "hasArgument", NULL);
		break;
	case ET_STRING:
		str = (char *)gf_list_get(sc_enc->id_buf, 0);
		gf_list_rem(sc_enc->id_buf, 0);
		if (!sc_enc->emul) gf_bifs_enc_name(sc_enc->codec, sc_enc->bs, str);
		gf_free(str);
		break;
	case ET_NEGATIVE:
	case ET_INCREMENT:
	case ET_DECREMENT:
	case ET_NOT:
	case ET_ONESCOMP:
		SFE_Expression(sc_enc, finalPos+1, end, GF_FALSE);
		break;
	case ET_CURVED_EXPR:
		SFE_CompoundExpression(sc_enc, finalPos+1, end-1, 0);
		break;
	case ET_POST_INCREMENT :
	case ET_POST_DECREMENT :
		SFE_Expression(sc_enc, start, finalPos, GF_FALSE);
		break;
	case ET_FUNCTION_CALL:
		SFE_FunctionCall(sc_enc, start, end);
		break;
	case ET_OBJECT_MEMBER_ACCESS :
		SFE_ObjectMemberAccess(sc_enc, start, finalPos, end);
		break;
	case ET_OBJECT_METHOD_CALL:
		SFE_ObjectMethodCall(sc_enc, start, finalPos, end);
		break;
	case ET_ARRAY_DEREFERENCE:
		SFE_ArrayDereference(sc_enc, start, finalPos, end);
		break;
	case ET_NEW:
		SFE_ObjectConstruct(sc_enc, start, finalPos, end);
		break;
	case ET_CONDTEST:
		SFE_ConditionTest(sc_enc, start, finalPos, end);
		break;
	case ET_FUNCTION_ASSIGN:
		SFE_NextToken(sc_enc);
		SFE_CheckToken(sc_enc, TOK_LEFT_CURVE);

		SFE_Arguments(sc_enc);
		SFE_StatementBlock(sc_enc);
		SFE_NextToken(sc_enc);
		SFE_CheckToken(sc_enc, TOK_SEMICOLON);
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[bifs] Script encoding: illegal expression type %s\n", expr < GF_ARRAY_LENGTH(expr_name) ? expr_name[expr] : ""));
		sc_enc->err = GF_BAD_PARAM;
		break;
	}
	return expr;
}


void SFE_FunctionCall(ScriptEnc *sc_enc, u32 start, u32 end)
{
	u32 curTok;
	char *str;
	curTok = sc_enc->expr_toks[start++];
	CHECK_TOK(TOK_IDENTIFIER);
	str = (char *)gf_list_get(sc_enc->id_buf, 0);
	gf_list_rem(sc_enc->id_buf, 0);
	SFE_PutIdentifier(sc_enc, str);
	gf_free(str);
	curTok = sc_enc->expr_toks[start++];
	CHECK_TOK(TOK_LEFT_CURVE);
	SFE_Params(sc_enc, start, end-1);
	curTok = sc_enc->expr_toks[end-1];
	CHECK_TOK(TOK_RIGHT_CURVE);
}

void SFE_ObjectMemberAccess(ScriptEnc *sc_enc, u32 start, u32 op, u32 end)
{
	u32 curTok;
	char *str;

	SFE_Expression(sc_enc, start, op, GF_TRUE);
	curTok = sc_enc->expr_toks[op];
	CHECK_TOK(TOK_PERIOD);
	curTok = sc_enc->expr_toks[end-1];
	CHECK_TOK(TOK_IDENTIFIER);
	str = (char *)gf_list_get(sc_enc->id_buf, 0);
	gf_list_rem(sc_enc->id_buf, 0);
	SFE_PutIdentifier(sc_enc, str);
	gf_free(str);
}

void SFE_ObjectMethodCall(ScriptEnc *sc_enc, u32 start, u32 op, u32 end)
{
	u32 curTok;
	char *str;

	SFE_Expression(sc_enc, start, op, GF_FALSE);
	curTok = sc_enc->expr_toks[op];
	CHECK_TOK(TOK_PERIOD);
	curTok = sc_enc->expr_toks[op+1];
	CHECK_TOK(TOK_IDENTIFIER);
	str = (char *)gf_list_get(sc_enc->id_buf, 0);
	gf_list_rem(sc_enc->id_buf, 0);
	SFE_PutIdentifier(sc_enc, str);
	gf_free(str);
	curTok = sc_enc->expr_toks[op+2];
	CHECK_TOK(TOK_LEFT_CURVE);
	SFE_Params(sc_enc, op+3, end-1);
	curTok = sc_enc->expr_toks[end-1];
	CHECK_TOK(TOK_RIGHT_CURVE);
}

void SFE_ArrayDereference(ScriptEnc *sc_enc, u32 start, u32 op, u32 end)
{
	u32 curTok;

	SFE_Expression(sc_enc, start, op, GF_FALSE);
	curTok = sc_enc->expr_toks[op];
	CHECK_TOK(TOK_LEFT_BRACKET);
	SFE_CompoundExpression(sc_enc, op+1, end-1, 0);
	curTok = sc_enc->expr_toks[end-1];
	CHECK_TOK(TOK_RIGHT_BRACKET);
}

void SFE_ObjectConstruct(ScriptEnc *sc_enc, u32 start, u32 op, u32 end)
{
	u32 curTok;
	char *str;

	curTok = sc_enc->expr_toks[start++];
	CHECK_TOK(TOK_NEW);
	curTok = sc_enc->expr_toks[start++];
	CHECK_TOK(TOK_IDENTIFIER);
	str = (char *)gf_list_get(sc_enc->id_buf, 0);
	gf_list_rem(sc_enc->id_buf, 0);
	SFE_PutIdentifier(sc_enc, str);
	gf_free(str);
	curTok = sc_enc->expr_toks[start++];
	CHECK_TOK(TOK_LEFT_CURVE);
	SFE_Params(sc_enc, start, end-1);
	curTok = sc_enc->expr_toks[end-1];
	CHECK_TOK(TOK_RIGHT_CURVE);
}

void SFE_ConditionTest(ScriptEnc *sc_enc, u32 start, u32 op, u32 end)
{
	u32 curTok;

	SFE_Expression(sc_enc, start, op, GF_FALSE);
	curTok = sc_enc->expr_toks[op];
	CHECK_TOK(TOK_CONDTEST);
	start = op+1;
	op = MoveToToken(sc_enc, TOK_CONDSEP, op, end-1);
	SFE_Expression(sc_enc, start, op, GF_FALSE);
	curTok = sc_enc->expr_toks[op];
	CHECK_TOK(TOK_CONDSEP);
	SFE_Expression(sc_enc, op+1, end, GF_FALSE);
}

void SFE_Params(ScriptEnc *sc_enc, u32 start, u32 end)
{
	u32 curTok;

	curTok = sc_enc->expr_toks[start];
	if (curTok != TOK_RIGHT_CURVE) {
		SFE_WRITE_INT(sc_enc, 1, 1, "hasParam", NULL);
		SFE_CompoundExpression(sc_enc, start, end, 1);
	} else {
		SFE_WRITE_INT(sc_enc, 0, 1, "hasParam", NULL);
	}
}

#endif	/* !defined(GPAC_DISABLE_BIFS) && !defined(GPAC_DISABLE_BIFS_ENC) && defined(GPAC_HAS_QJS) */
