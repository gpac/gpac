/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
 *			All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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

#include <gpac/xml.h>
#include <gpac/utf.h>
#include <gpac/network.h>

#ifndef GPAC_DISABLE_CORE_TOOLS

#ifndef GPAC_DISABLE_ZLIB
/*since 0.2.2, we use zlib for xmt/x3d reading to handle gz files*/
#include <zlib.h>

#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)
#pragma comment(lib, "zlib")
#endif
#else
#define NO_GZIP
#endif


#define XML_INPUT_SIZE	4096


static GF_Err gf_xml_sax_parse_intern(GF_SAXParser *parser, char *current);

static char *xml_translate_xml_string(char *str)
{
	char *value;
	u32 size, i, j;
	if (!str || !strlen(str)) return NULL;
	value = (char *)gf_malloc(sizeof(char) * 500);
	size = 500;
	i = j = 0;
	while (str[i]) {
		if (j+20 >= size) {
			size += 500;
			value = (char *)gf_realloc(value, sizeof(char)*size);
		}
		if (str[i] == '&') {
			if (str[i+1]=='#') {
				char szChar[20], *end;
				u16 wchar[2];
				u32 val;
				const unsigned short *srcp;
				strncpy(szChar, str+i, 10);
				end = strchr(szChar, ';');
				if (!end) break;
				end[1] = 0;
				i += (u32) strlen(szChar);
				wchar[1] = 0;
				if (szChar[2]=='x')
					sscanf(szChar, "&#x%x;", &val);
				else
					sscanf(szChar, "&#%u;", &val);
				wchar[0] = val;
				srcp = wchar;
				j += (u32) gf_utf8_wcstombs(&value[j], 20, &srcp);
			}
			else if (!strnicmp(&str[i], "&amp;", sizeof(char)*5)) {
				value[j] = '&';
				j++;
				i+= 5;
			}
			else if (!strnicmp(&str[i], "&lt;", sizeof(char)*4)) {
				value[j] = '<';
				j++;
				i+= 4;
			}
			else if (!strnicmp(&str[i], "&gt;", sizeof(char)*4)) {
				value[j] = '>';
				j++;
				i+= 4;
			}
			else if (!strnicmp(&str[i], "&apos;", sizeof(char)*6)) {
				value[j] = '\'';
				j++;
				i+= 6;
			}
			else if (!strnicmp(&str[i], "&quot;", sizeof(char)*6)) {
				value[j] = '\"';
				j++;
				i+= 6;
			} else {
				value[j] = str[i];
				j++;
				i++;
			}
		} else {
			value[j] = str[i];
			j++;
			i++;
		}
	}
	value[j] = 0;
	return value;
}


enum
{
	SAX_STATE_ATT_NAME,
	SAX_STATE_ATT_VALUE,
	SAX_STATE_ELEMENT,
	SAX_STATE_COMMENT,
	SAX_STATE_TEXT_CONTENT,
	SAX_STATE_ENTITY,
	SAX_STATE_SKIP_DOCTYPE,
	SAX_STATE_CDATA,
	SAX_STATE_DONE,
	SAX_STATE_XML_PROC,
	SAX_STATE_SYNTAX_ERROR,
	SAX_STATE_ALLOC_ERROR,
};

typedef struct
{
	u32 name_start, name_end;
	u32 val_start, val_end;
	Bool has_entities;
} GF_XMLSaxAttribute;


/* #define NO_GZIP */


struct _tag_sax_parser
{
	/*0: UTF-8, 1: UTF-16 BE, 2: UTF-16 LE. String input is always converted back to utf8*/
	s32 unicode_type;
	char *buffer;
	/*alloc size, line size and current position*/
	u32 alloc_size, line_size, current_pos;
	/*current node depth*/
	u32 node_depth;

	/*gz input file*/
#ifdef NO_GZIP
	FILE *f_in;
#else
	gzFile gz_in;
#endif
	/*current line , file size and pos for user notif*/
	u32 line, file_size, file_pos;

	/*SAX callbacks*/
	gf_xml_sax_node_start sax_node_start;
	gf_xml_sax_node_end sax_node_end;
	gf_xml_sax_text_content sax_text_content;
	void *sax_cbck;
	gf_xml_sax_progress on_progress;

	u32 sax_state;
	u32 init_state;
	GF_List *entities;
	char att_sep;
	Bool in_entity, suspended;
	u32 in_quote;

	u32 elt_start_pos, elt_end_pos;

	/*last error found*/
	char err_msg[1000];

	u32 att_name_start, elt_name_start, elt_name_end, text_start, text_end;

	GF_XMLAttribute *attrs;
	GF_XMLSaxAttribute *sax_attrs;
	u32 nb_attrs, nb_alloc_attrs;
};

static GF_XMLSaxAttribute *xml_get_sax_attribute(GF_SAXParser *parser)
{
	if (parser->nb_attrs==parser->nb_alloc_attrs) {
		parser->nb_alloc_attrs++;
		parser->sax_attrs = (GF_XMLSaxAttribute *)gf_realloc(parser->sax_attrs, sizeof(GF_XMLSaxAttribute)*parser->nb_alloc_attrs);
		parser->attrs = (GF_XMLAttribute *)gf_realloc(parser->attrs, sizeof(GF_XMLAttribute)*parser->nb_alloc_attrs);
	}
	return &parser->sax_attrs[parser->nb_attrs++];
}

static void xml_sax_swap(GF_SAXParser *parser)
{
	if (parser->current_pos && ((parser->sax_state==SAX_STATE_TEXT_CONTENT) || (parser->sax_state==SAX_STATE_COMMENT) ) ) {
		if (parser->line_size >= parser->current_pos) {
			parser->line_size -= parser->current_pos;
			parser->file_pos += parser->current_pos;
			if (parser->line_size) memmove(parser->buffer, parser->buffer + parser->current_pos, sizeof(char)*parser->line_size);
			parser->buffer[parser->line_size] = 0;
			parser->current_pos = 0;
		}
	}
}

static void format_sax_error(GF_SAXParser *parser, u32 linepos, const char* fmt, ...)
{
	va_list args;
	u32 len;
	char szM[20];

	if (!parser) return;

	va_start(args, fmt);
	vsnprintf(parser->err_msg, ARRAY_LENGTH(parser->err_msg), fmt, args);
	va_end(args);

	if (strlen(parser->err_msg)+30 < ARRAY_LENGTH(parser->err_msg)) {
		snprintf(szM, 20, " - Line %d: ", parser->line + 1);
		strcat(parser->err_msg, szM);
		len = (u32) strlen(parser->err_msg);
		strncpy(parser->err_msg + len, parser->buffer+ (linepos ? linepos : parser->current_pos), 10);
		parser->err_msg[len + 10] = 0;
	}
	parser->sax_state = SAX_STATE_SYNTAX_ERROR;
}

static void xml_sax_node_end(GF_SAXParser *parser, Bool had_children)
{
	char *name, *sep, c;

	assert(parser->elt_name_start);
	assert(parser->elt_name_end);
	if (!parser->node_depth) {
		format_sax_error(parser, 0, "Markup error");
		return;
	}
	c = parser->buffer[parser->elt_name_end - 1];
	parser->buffer[parser->elt_name_end - 1] = 0;
	name = parser->buffer + parser->elt_name_start - 1;

	if (parser->sax_node_end) {
		sep = strchr(name, ':');
		if (sep) {
			sep[0] = 0;
			parser->sax_node_end(parser->sax_cbck, sep+1, name);
			sep[0] = ':';
		} else {
			parser->sax_node_end(parser->sax_cbck, name, NULL);
		}
	}
	parser->buffer[parser->elt_name_end - 1] = c;
	parser->node_depth--;
	if (!parser->init_state && !parser->node_depth) parser->sax_state = SAX_STATE_DONE;
	xml_sax_swap(parser);
	parser->text_start = parser->text_end = 0;
}

static void xml_sax_node_start(GF_SAXParser *parser)
{
	Bool has_entities = GF_FALSE;
	u32 i;
	char *sep, c, *name;

	assert(parser->elt_name_start && parser->elt_name_end);
	c = parser->buffer[parser->elt_name_end - 1];
	parser->buffer[parser->elt_name_end - 1] = 0;
	name = parser->buffer + parser->elt_name_start - 1;

	for (i=0; i<parser->nb_attrs; i++) {
		parser->attrs[i].name = parser->buffer + parser->sax_attrs[i].name_start - 1;
		parser->buffer[parser->sax_attrs[i].name_end-1] = 0;
		parser->attrs[i].value = parser->buffer + parser->sax_attrs[i].val_start - 1;
		parser->buffer[parser->sax_attrs[i].val_end-1] = 0;

		if (strchr(parser->attrs[i].value, '&')) {
			parser->sax_attrs[i].has_entities = GF_TRUE;
			has_entities = GF_TRUE;
			parser->attrs[i].value = xml_translate_xml_string(parser->attrs[i].value);
		}
		/*store first char pos after current attrib for node peeking*/
		parser->att_name_start = parser->sax_attrs[i].val_end;
	}

	if (parser->sax_node_start) {
		sep = strchr(name, ':');
		if (sep) {
			sep[0] = 0;
			parser->sax_node_start(parser->sax_cbck, sep+1, name, parser->attrs, parser->nb_attrs);
			sep[0] = ':';
		} else {
			parser->sax_node_start(parser->sax_cbck, name, NULL, parser->attrs, parser->nb_attrs);
		}
	}
	parser->att_name_start = 0;
	parser->buffer[parser->elt_name_end - 1] = c;
	parser->node_depth++;
	if (has_entities) {
		for (i=0; i<parser->nb_attrs; i++) {
			if (parser->sax_attrs[i].has_entities) {
				parser->sax_attrs[i].has_entities = GF_FALSE;
				gf_free(parser->attrs[i].value);
			}
		}
	}
	parser->nb_attrs = 0;
	xml_sax_swap(parser);
	parser->text_start = parser->text_end = 0;
}

static Bool xml_sax_parse_attribute(GF_SAXParser *parser)
{
	char *sep;
	GF_XMLSaxAttribute *att = NULL;

	/*looking for attribute name*/
	if (parser->sax_state==SAX_STATE_ATT_NAME) {
		/*looking for start*/
		if (!parser->att_name_start) {
			while (parser->current_pos < parser->line_size) {
				u8 c = parser->buffer[parser->current_pos];
				switch (c) {
				case '\n':
					parser->line++;
				case ' ':
				case '\r':
				case '\t':
					parser->current_pos++;
					continue;
				/*end of element*/
				case '?':
					if (parser->init_state!=1) break;
				case '/':
					/*not enough data*/
					if (parser->current_pos+1 == parser->line_size) return GF_TRUE;
					if (parser->buffer[parser->current_pos+1]=='>') {
						parser->current_pos+=2;
						parser->elt_end_pos = parser->file_pos + parser->current_pos - 1;
						/*done parsing attr AND elements*/
						if (!parser->init_state) {
							xml_sax_node_start(parser);
							/*move to SAX_STATE_TEXT_CONTENT to force text flush*/
							parser->sax_state = SAX_STATE_TEXT_CONTENT;
							xml_sax_node_end(parser, GF_FALSE);
						} else {
							parser->nb_attrs = 0;
						}
						parser->sax_state = (parser->init_state) ? SAX_STATE_ELEMENT : SAX_STATE_TEXT_CONTENT;
						parser->text_start = parser->text_end = 0;
						return GF_FALSE;
					}
					if (!parser->in_quote && (c=='/')) {
						if (!parser->init_state) {
							format_sax_error(parser, 0, "Markup error");
							return GF_TRUE;
						}
					}
					break;
				case '"':
					if (parser->sax_state==SAX_STATE_ATT_VALUE) break;
					if (parser->in_quote && (parser->in_quote!=c) ) {
						format_sax_error(parser, 0, "Markup error");
						return GF_TRUE;
					}
					if (parser->in_quote) parser->in_quote = 0;
					else parser->in_quote = c;
					break;
				case '>':
					parser->current_pos+=1;
					/*end of <!DOCTYPE>*/
					if (parser->init_state) {
						if (parser->init_state==1) {
							format_sax_error(parser, 0, "Invalid DOCTYPE");
							return GF_TRUE;
						}
						parser->sax_state = SAX_STATE_ELEMENT;
						return GF_FALSE;
					}
					/*done parsing attr*/
					parser->sax_state = SAX_STATE_TEXT_CONTENT;
					xml_sax_node_start(parser);
					return GF_FALSE;
				case '[':
					if (parser->init_state) {
						parser->current_pos+=1;
						if (parser->init_state==1) {
							format_sax_error(parser, 0, "Invalid DOCTYPE");
							return GF_TRUE;
						}
						parser->sax_state = SAX_STATE_ELEMENT;
						return GF_FALSE;
					}
					break;
				case '<':
					format_sax_error(parser, 0, "Invalid character '<'");
					return GF_FALSE;
				/*first char of attr name*/
				default:
					parser->att_name_start = parser->current_pos + 1;
					break;
				}
				parser->current_pos++;
				if (parser->att_name_start) break;
			}
			if (parser->current_pos == parser->line_size) return GF_TRUE;
		}

		if (parser->init_state==2) {
			sep = strchr(parser->buffer + parser->att_name_start - 1, parser->in_quote ?  parser->in_quote : ' ');
			/*not enough data*/
			if (!sep) return GF_TRUE;
			parser->current_pos = (u32) (sep - parser->buffer);
			parser->att_name_start = 0;
			if (parser->in_quote) {
				parser->current_pos++;
				parser->in_quote = 0;
			}
			return GF_FALSE;
		}

		/*looking for '"'*/
		if (parser->att_name_start) {
			u32 i, first=1;
			sep = strchr(parser->buffer + parser->att_name_start - 1, '=');
			/*not enough data*/
			if (!sep) return GF_TRUE;

			parser->current_pos = (u32) (sep - parser->buffer);
			att = xml_get_sax_attribute(parser);
			att->name_start = parser->att_name_start;
			att->name_end = parser->current_pos + 1;
			while (strchr(" \n\t", parser->buffer[att->name_end - 2])) {
				assert(att->name_end);
				att->name_end --;
			}
			att->has_entities = GF_FALSE;

			for (i=att->name_start; i<att->name_end; i++) {
				char c = parser->buffer[i-1];
				if ((c>='a') && (c<='z')) {}
				else if ((c>='A') && (c<='Z')) {}
				else if ((c==':') || (c=='_')) {}

				else if (!first && ((c=='-') || (c=='.') || ((c>='0') && (c<='9')) )) {}

				else {
					format_sax_error(parser, att->name_start-1, "Invalid character \'%c\' for attribute name", c);
					return GF_TRUE;
				}

				first=0;
			}

			parser->att_name_start = 0;
			parser->current_pos++;
			parser->sax_state = SAX_STATE_ATT_VALUE;

		}
	}

	if (parser->sax_state == SAX_STATE_ATT_VALUE) {
		att = &parser->sax_attrs[parser->nb_attrs-1];
		/*looking for first delimiter*/
		if (!parser->att_sep) {
			while (parser->current_pos < parser->line_size) {
				u8 c = parser->buffer[parser->current_pos];
				switch (c) {
				case '\n':
					parser->line++;
				case ' ':
				case '\r':
				case '\t':
					parser->current_pos++;
					continue;
				case '\'':
				case '"':
					parser->att_sep = c;
					att->val_start = parser->current_pos + 2;
					break;
				default:
					break;
				}
				parser->current_pos++;
				if (parser->att_sep) break;
			}
			if (parser->current_pos == parser->line_size) return GF_TRUE;
		}

att_retry:

		assert(parser->att_sep);
		sep = strchr(parser->buffer + parser->current_pos, parser->att_sep);
		if (!sep || !sep[1]) return GF_TRUE;

		if (sep[1]==parser->att_sep) {
			format_sax_error(parser, (u32) (sep - parser->buffer), "Invalid character %c after attribute value separator %c ", sep[1], parser->att_sep);
			return GF_TRUE;
		}

		if (!parser->init_state && (strchr(" />\n\t\r", sep[1])==NULL)) {
			parser->current_pos = (u32) (sep - parser->buffer + 1);
			goto att_retry;
		}

		parser->current_pos = (u32) (sep - parser->buffer);
		att->val_end = parser->current_pos + 1;
		parser->current_pos++;

		/*"style" always at the beginning of the attributes for ease of parsing*/
		if (!strncmp(parser->buffer + att->name_start-1, "style", 5)) {
			GF_XMLSaxAttribute prev = parser->sax_attrs[0];
			parser->sax_attrs[0] = *att;
			*att = prev;
		}
		parser->att_sep = 0;
		parser->sax_state = SAX_STATE_ATT_NAME;
		parser->att_name_start = 0;
		return GF_FALSE;
	}
	return GF_TRUE;
}


typedef struct
{
	char *name;
	char *value;
	u32 namelen;
	u8 sep;
} XML_Entity;

static void xml_sax_flush_text(GF_SAXParser *parser)
{
	char *text, c;
	if (!parser->text_start || parser->init_state || !parser->sax_text_content) return;

	assert(parser->text_start < parser->text_end);

	c = parser->buffer[parser->text_end-1];
	parser->buffer[parser->text_end-1] = 0;
	text = parser->buffer + parser->text_start-1;

	/*solve XML built-in entities*/
	if (strchr(text, '&') && strchr(text, ';')) {
		char *xml_text = xml_translate_xml_string(text);
		if (xml_text) {
			parser->sax_text_content(parser->sax_cbck, xml_text, (parser->sax_state==SAX_STATE_CDATA) ? GF_TRUE : GF_FALSE);
			gf_free(xml_text);
		}
	} else {
		parser->sax_text_content(parser->sax_cbck, text, (parser->sax_state==SAX_STATE_CDATA) ? GF_TRUE : GF_FALSE);
	}
	parser->buffer[parser->text_end-1] = c;
	parser->text_start = parser->text_end = 0;
}

static void xml_sax_store_text(GF_SAXParser *parser, u32 txt_len)
{
	if (!txt_len) return;

	if (!parser->text_start) {
		parser->text_start = parser->current_pos + 1;
		parser->text_end = parser->text_start + txt_len;
		parser->current_pos += txt_len;
		assert(parser->current_pos <= parser->line_size);
		return;
	}
	/*contiguous text*/
	if (parser->text_end && (parser->text_end-1 == parser->current_pos)) {
		parser->text_end += txt_len;
		parser->current_pos += txt_len;
		assert(parser->current_pos <= parser->line_size);
		return;
	}
	/*need to flush*/
	xml_sax_flush_text(parser);

	parser->text_start = parser->current_pos + 1;
	parser->text_end = parser->text_start + txt_len;
	parser->current_pos += txt_len;
	assert(parser->current_pos <= parser->line_size);
}

static char *xml_get_current_text(GF_SAXParser *parser)
{
	char *text, c;
	if (!parser->text_start) return NULL;

	c = parser->buffer[parser->text_end-1];
	parser->buffer[parser->text_end-1] = 0;
	text = gf_strdup(parser->buffer + parser->text_start-1);
	parser->buffer[parser->text_end-1] = c;
	parser->text_start = parser->text_end = 0;
	return text;
}

static void xml_sax_skip_doctype(GF_SAXParser *parser)
{
	while (parser->current_pos < parser->line_size) {
		if (parser->buffer[parser->current_pos]=='>') {
			parser->sax_state = SAX_STATE_ELEMENT;
			parser->current_pos++;
			xml_sax_swap(parser);
			return;
		}
		parser->current_pos++;
	}
}

static void xml_sax_skip_xml_proc(GF_SAXParser *parser)
{
	while (parser->current_pos + 1 < parser->line_size) {
		if ((parser->buffer[parser->current_pos]=='?') && (parser->buffer[parser->current_pos+1]=='>')) {
			parser->sax_state = SAX_STATE_ELEMENT;
			parser->current_pos++;
			xml_sax_swap(parser);
			return;
		}
		parser->current_pos++;
	}
}


static void xml_sax_parse_entity(GF_SAXParser *parser)
{
	char szName[1024];
	u32 i = 0;
	XML_Entity *ent = (XML_Entity *)gf_list_last(parser->entities);
	char *skip_chars = " \t\n\r";
	i=0;
	if (ent && ent->value) ent = NULL;
	if (ent) skip_chars = NULL;

	while (parser->current_pos+i < parser->line_size) {
		u8 c = parser->buffer[parser->current_pos+i];
		if (skip_chars && strchr(skip_chars, c)) {
			if (c=='\n') parser->line++;
			parser->current_pos++;
			continue;
		}
		if (!ent && (c=='%')) {
			parser->current_pos+=i+1;
			parser->sax_state = SAX_STATE_SKIP_DOCTYPE;
			return;
		}
		else if (!ent && ((c=='\"') || (c=='\'')) ) {
			szName[i] = 0;
			GF_SAFEALLOC(ent, XML_Entity);
			if (!ent) {
				parser->sax_state = SAX_STATE_ALLOC_ERROR;
				return;
			}
			ent->name = gf_strdup(szName);
			ent->namelen = (u32) strlen(ent->name);
			ent->sep = c;
			parser->current_pos += 1+i;
			assert(parser->current_pos < parser->line_size);
			xml_sax_swap(parser);
			i=0;
			gf_list_add(parser->entities, ent);
			skip_chars = NULL;
		} else if (ent && c==ent->sep) {
			xml_sax_store_text(parser, i);

			ent->value = xml_get_current_text(parser);
			if (!ent->value) ent->value = gf_strdup("");

			parser->current_pos += 1;
			assert(parser->current_pos < parser->line_size);
			xml_sax_swap(parser);
			parser->sax_state = SAX_STATE_SKIP_DOCTYPE;
			return;
		} else if (!ent) {
			szName[i] = c;
			i++;
		} else {
			i++;
		}
	}
	xml_sax_store_text(parser, i);
}

static void xml_sax_cdata(GF_SAXParser *parser)
{
	char *cd_end = strstr(parser->buffer + parser->current_pos, "]]>");
	if (!cd_end) {
		xml_sax_store_text(parser, parser->line_size - parser->current_pos);
	} else {
		u32 size = (u32) (cd_end - (parser->buffer + parser->current_pos));
		xml_sax_store_text(parser, size);
		xml_sax_flush_text(parser);
		parser->current_pos += 3;
		assert(parser->current_pos <= parser->line_size);
		parser->sax_state = SAX_STATE_TEXT_CONTENT;
	}
}

static Bool xml_sax_parse_comments(GF_SAXParser *parser)
{
	char *end = strstr(parser->buffer + parser->current_pos, "-->");
	if (!end) {
		if (parser->line_size>3)
			parser->current_pos = parser->line_size-3;
		xml_sax_swap(parser);
		return GF_FALSE;
	}

	parser->current_pos += 3 + (u32) (end - (parser->buffer + parser->current_pos) );
	assert(parser->current_pos <= parser->line_size);
	parser->sax_state = SAX_STATE_TEXT_CONTENT;
	parser->text_start = parser->text_end = 0;
	xml_sax_swap(parser);
	return GF_TRUE;
}



static GF_Err xml_sax_parse(GF_SAXParser *parser, Bool force_parse)
{
	u32 i = 0;
	Bool is_text;
	u32 is_end;
	u8 c;
	char *elt, sep;
	u32 cdata_sep;

	while (parser->current_pos<parser->line_size) {
		if (!force_parse && parser->suspended) goto exit;

restart:
		is_text = GF_FALSE;
		switch (parser->sax_state) {
		/*load an XML element*/
		case SAX_STATE_TEXT_CONTENT:
			is_text = GF_TRUE;
		case SAX_STATE_ELEMENT:
			elt = NULL;
			i=0;
			while ((c = parser->buffer[parser->current_pos+i]) !='<') {
				if ((parser->init_state==2) && (c ==']')) {
					parser->sax_state = SAX_STATE_ATT_NAME;
					parser->current_pos+=i+1;
					goto restart;
				}
				i++;
				if (c=='\n') parser->line++;

				if (parser->current_pos+i==parser->line_size) {
					if ((parser->line_size>=2*XML_INPUT_SIZE) && !parser->init_state)
						parser->sax_state = SAX_STATE_SYNTAX_ERROR;

					goto exit;
				}
			}
			if (is_text && i) {
				xml_sax_store_text(parser, i);
				parser->sax_state = SAX_STATE_ELEMENT;
			} else if (i) {
				parser->current_pos += i;
				assert(parser->current_pos < parser->line_size);
			}
			is_end = 0;
			i = 0;
			cdata_sep = 0;
			while (1) {
				char c = parser->buffer[parser->current_pos+1+i];
				if (!strncmp(parser->buffer+parser->current_pos+1+i, "!--", 3)) {
					parser->sax_state = SAX_STATE_COMMENT;
					i += 3;
					break;
				}
				if (!c) {
					goto exit;
				}
				if ((c=='\t') || (c=='\r') || (c==' ') ) {
					if (i) break;
					else parser->current_pos++;
				}
				else if (c=='\n') {
					parser->line++;
					if (i) break;
					else parser->current_pos++;
				}
				else if (c=='>') break;
				else if (c=='=') break;
				else if (c=='[') {
					i++;
					if (!cdata_sep) cdata_sep = 1;
					else {
						break;
					}
				}
				else if (c=='/') {
					is_end = !i ? 1 : 2;
					i++;
				} else if (c=='<') {
					if (parser->sax_state != SAX_STATE_COMMENT) {
						parser->sax_state = SAX_STATE_SYNTAX_ERROR;
						return GF_CORRUPTED_DATA;
					}
				} else {
					i++;
				}
				/*				if ((c=='[') && (parser->buffer[parser->elt_name_start-1 + i-2]=='A') ) break; */
				if (parser->current_pos+1+i==parser->line_size) {
					goto exit;
				}
			}
			if (i) {
				parser->elt_name_start = parser->current_pos+1 + 1;
				if (is_end==1) parser->elt_name_start ++;
				if (is_end==2) parser->elt_name_end = parser->current_pos+1+i;
				else parser->elt_name_end = parser->current_pos+1+i + 1;
			}
			if (is_end) {
				xml_sax_flush_text(parser);
				parser->elt_end_pos = parser->file_pos + parser->current_pos + i;
				if (is_end==2) {
					parser->sax_state = SAX_STATE_ELEMENT;
					xml_sax_node_start(parser);
					xml_sax_node_end(parser, GF_FALSE);
				} else {
					parser->elt_end_pos += parser->elt_name_end - parser->elt_name_start;
					xml_sax_node_end(parser, GF_TRUE);
				}
				if (parser->sax_state == SAX_STATE_SYNTAX_ERROR) break;
				parser->current_pos+=2+i;
				parser->sax_state = SAX_STATE_TEXT_CONTENT;
				break;
			}
			if (!parser->elt_name_end) {
				return GF_CORRUPTED_DATA;
			}
			sep = parser->buffer[parser->elt_name_end-1];
			parser->buffer[parser->elt_name_end-1] = 0;
			elt = parser->buffer + parser->elt_name_start-1;

			parser->sax_state = SAX_STATE_ATT_NAME;
			assert(parser->elt_start_pos <= parser->file_pos + parser->current_pos);
			parser->elt_start_pos = parser->file_pos + parser->current_pos;

			if (!strncmp(elt, "!--", 3)) {
				xml_sax_flush_text(parser);
				parser->sax_state = SAX_STATE_COMMENT;
				if (i>3) parser->current_pos -= (i-3);
			}
			else if (!strcmp(elt, "?xml")) parser->init_state = 1;
			else if (!strcmp(elt, "!DOCTYPE")) parser->init_state = 2;
			else if (!strcmp(elt, "!ENTITY")) parser->sax_state = SAX_STATE_ENTITY;
			else if (!strcmp(elt, "!ATTLIST") || !strcmp(elt, "!ELEMENT")) parser->sax_state = SAX_STATE_SKIP_DOCTYPE;
			else if (!strcmp(elt, "![CDATA["))
				parser->sax_state = SAX_STATE_CDATA;
			else if (elt[0]=='?') {
				i--;
				parser->sax_state = SAX_STATE_XML_PROC;
			}
			/*node found*/
			else {
				xml_sax_flush_text(parser);
				if (parser->init_state) {
					parser->init_state = 0;
					/*that's a bit ugly: since we solve entities when appending text, we need to
					reparse the current buffer*/
					if (gf_list_count(parser->entities)) {
						char *orig_buf;
						GF_Err e;
						parser->buffer[parser->elt_name_end-1] = sep;
						orig_buf = gf_strdup(parser->buffer + parser->current_pos);
						parser->current_pos = 0;
						parser->line_size = 0;
						parser->elt_start_pos = 0;
						parser->sax_state = SAX_STATE_TEXT_CONTENT;
						e = gf_xml_sax_parse_intern(parser, orig_buf);
						gf_free(orig_buf);
						return e;
					}
				}
			}
			parser->current_pos+=1+i;
			parser->buffer[parser->elt_name_end-1] = sep;
			break;
		case SAX_STATE_COMMENT:
			if (!xml_sax_parse_comments(parser)) {
				xml_sax_swap(parser);
				goto exit;
			}
			break;
		case SAX_STATE_ATT_NAME:
		case SAX_STATE_ATT_VALUE:
			if (xml_sax_parse_attribute(parser))
				goto exit;
			break;
		case SAX_STATE_ENTITY:
			xml_sax_parse_entity(parser);
			break;
		case SAX_STATE_SKIP_DOCTYPE:
			xml_sax_skip_doctype(parser);
			break;
		case SAX_STATE_XML_PROC:
			xml_sax_skip_xml_proc(parser);
			break;
		case SAX_STATE_CDATA:
			xml_sax_cdata(parser);
			break;
		case SAX_STATE_SYNTAX_ERROR:
			return GF_CORRUPTED_DATA;
		case SAX_STATE_ALLOC_ERROR:
			return GF_OUT_OF_MEM;
		case SAX_STATE_DONE:
			return GF_EOS;
		}
	}
exit:
#if 0
	if (is_text) {
		if (i) xml_sax_store_text(parser, i);
		/*DON'T FLUSH TEXT YET, wait for next '<' to do so otherwise we may corrupt xml base entities (&apos;, ...)*/
	}
#endif
	xml_sax_swap(parser);

	if (parser->sax_state==SAX_STATE_SYNTAX_ERROR)
		return GF_CORRUPTED_DATA;
	else
		return GF_OK;
}

static GF_Err xml_sax_append_string(GF_SAXParser *parser, char *string)
{
	u32 size = parser->line_size;
	u32 nl_size = (u32) strlen(string);

	if (!nl_size) return GF_OK;

	if ( (parser->alloc_size < size+nl_size+1)
	        /*		|| (parser->alloc_size / 2 ) > size+nl_size+1 */
	   )
	{
		parser->alloc_size = size+nl_size+1;
		parser->alloc_size = 3 * parser->alloc_size / 2;
		parser->buffer = (char*)gf_realloc(parser->buffer, sizeof(char) * parser->alloc_size);
		if (!parser->buffer ) return GF_OUT_OF_MEM;
	}
	memcpy(parser->buffer+size, string, sizeof(char)*nl_size);
	parser->buffer[size+nl_size] = 0;
	parser->line_size = size+nl_size;
	return GF_OK;
}

static XML_Entity *gf_xml_locate_entity(GF_SAXParser *parser, char *ent_start, Bool *needs_text)
{
	u32 i, count;
	u32 len = (u32) strlen(ent_start);

	*needs_text = GF_FALSE;
	count = gf_list_count(parser->entities);

	for (i=0; i<count; i++) {
		XML_Entity *ent = (XML_Entity *)gf_list_get(parser->entities, i);
		if (len < ent->namelen + 1) {
			if (strncmp(ent->name, ent_start, len))
			 	return NULL;

			*needs_text = GF_TRUE;
			return NULL;
		}
		if (!strncmp(ent->name, ent_start, ent->namelen) && (ent_start[ent->namelen]==';')) {
			return ent;
		}
	}
	return NULL;
}


static GF_Err gf_xml_sax_parse_intern(GF_SAXParser *parser, char *current)
{
	u32 count;
	/*solve entities*/
	count = gf_list_count(parser->entities);
	while (count) {
		char *entityEnd;
		XML_Entity *ent;
		char *entityStart = strstr(current, "&");
		Bool needs_text;
		u32 line_num;

		/*if in entity, the start of the entity is in the buffer !!*/
		if (parser->in_entity) {
			u32 len;
			char *name;
			entityEnd = strstr(current, ";");
			if (!entityEnd) return xml_sax_append_string(parser, current);
			entityStart = strrchr(parser->buffer, '&');

			entityEnd[0] = 0;
			len = (u32) strlen(entityStart) + (u32) strlen(current) + 1;
			name = (char*)gf_malloc(sizeof(char)*len);
			sprintf(name, "%s%s;", entityStart+1, current);

			ent = gf_xml_locate_entity(parser, name, &needs_text);
			gf_free(name);

			if (!ent && !needs_text) {
				xml_sax_append_string(parser, current);
				xml_sax_parse(parser, GF_TRUE);
				entityEnd[0] = ';';
				current = entityEnd;
				continue;
			}
			assert(ent);
			/*truncate input buffer*/
			parser->line_size -= (u32) strlen(entityStart);
			entityStart[0] = 0;

			parser->in_entity = GF_FALSE;
			entityEnd[0] = ';';
			current = entityEnd+1;
		} else {
			if (!entityStart) break;

			ent = gf_xml_locate_entity(parser, entityStart+1, &needs_text);

			/*store current string before entity start*/
			entityStart[0] = 0;
			xml_sax_append_string(parser, current);
			xml_sax_parse(parser, GF_TRUE);
			entityStart[0] = '&';

			/*this is not an entitiy*/
			if (!ent && !needs_text) {
				xml_sax_append_string(parser, "&");
				current = entityStart+1;
				continue;
			}

			if (!ent) {
				parser->in_entity = GF_TRUE;
				/*store entity start*/
				return xml_sax_append_string(parser, entityStart);
			}
			current = entityStart + ent->namelen + 2;
		}
		/*append entity*/
		line_num = parser->line;
		xml_sax_append_string(parser, ent->value);
		xml_sax_parse(parser, GF_TRUE);
		parser->line = line_num;

	}
	xml_sax_append_string(parser, current);
	return xml_sax_parse(parser, GF_FALSE);
}

GF_EXPORT
GF_Err gf_xml_sax_parse(GF_SAXParser *parser, const void *string)
{
	GF_Err e;
	char *current;
	char *utf_conv = NULL;

	if (parser->unicode_type < 0) return GF_BAD_PARAM;

	if (parser->unicode_type>1) {
		const u16 *sptr = (const u16 *)string;
		u32 len = 2 * (u32) gf_utf8_wcslen(sptr);
		utf_conv = (char *)gf_malloc(sizeof(char)*(len+1));
		len = (u32) gf_utf8_wcstombs(utf_conv, len, &sptr);
		if (len==(u32) -1) {
			parser->sax_state = SAX_STATE_SYNTAX_ERROR;
			gf_free(utf_conv);
			return GF_CORRUPTED_DATA;
		}
		utf_conv[len] = 0;
		current = utf_conv;
	} else {
		current = (char *)string;
	}

	e = gf_xml_sax_parse_intern(parser, current);
	if (utf_conv) gf_free(utf_conv);
	return e;
}


GF_EXPORT
GF_Err gf_xml_sax_init(GF_SAXParser *parser, unsigned char *BOM)
{
	u32 offset;
	if (!BOM) {
		parser->unicode_type = 0;
		parser->sax_state = SAX_STATE_ELEMENT;
		return GF_OK;
	}

	if (parser->unicode_type >= 0) return gf_xml_sax_parse(parser, BOM);

	if ((BOM[0]==0xFF) && (BOM[1]==0xFE)) {
		if (!BOM[2] && !BOM[3]) return GF_NOT_SUPPORTED;
		parser->unicode_type = 2;
		offset = 2;
	} else if ((BOM[0]==0xFE) && (BOM[1]==0xFF)) {
		if (!BOM[2] && !BOM[3]) return GF_NOT_SUPPORTED;
		parser->unicode_type = 1;
		offset = 2;
	} else if ((BOM[0]==0xEF) && (BOM[1]==0xBB) && (BOM[2]==0xBF)) {
		/*we handle UTF8 as asci*/
		parser->unicode_type = 0;
		offset = 3;
	} else {
		parser->unicode_type = 0;
		offset = 0;
	}

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
		format_sax_error(NULL, 0, "");
	}
#endif

	parser->sax_state = SAX_STATE_ELEMENT;
	return gf_xml_sax_parse(parser, BOM + offset);
}

static void xml_sax_reset(GF_SAXParser *parser)
{
	while (1) {
		XML_Entity *ent = (XML_Entity *)gf_list_last(parser->entities);
		if (!ent) break;
		gf_list_rem_last(parser->entities);
		if (ent->name) gf_free(ent->name);
		if (ent->value) gf_free(ent->value);
		gf_free(ent);
	}
	if (parser->buffer) gf_free(parser->buffer);
	parser->buffer = NULL;
	parser->current_pos = 0;
	gf_free(parser->attrs);
	parser->attrs = NULL;
	gf_free(parser->sax_attrs);
	parser->sax_attrs = NULL;
	parser->nb_alloc_attrs = parser->nb_attrs = 0;
}


static GF_Err xml_sax_read_file(GF_SAXParser *parser)
{
	GF_Err e = GF_EOS;
	unsigned char szLine[XML_INPUT_SIZE+2];

#ifdef NO_GZIP
	if (!parser->f_in) return GF_BAD_PARAM;
#else
	if (!parser->gz_in) return GF_BAD_PARAM;
#endif


	while (!parser->suspended) {
#ifdef NO_GZIP
		s32 read = (s32)fread(szLine, 1, XML_INPUT_SIZE, parser->f_in);
#else
		s32 read = gzread(parser->gz_in, szLine, XML_INPUT_SIZE);
#endif
		if ((read<=0) /*&& !parser->node_depth*/) break;
		szLine[read] = 0;
		szLine[read+1] = 0;
		e = gf_xml_sax_parse(parser, szLine);
		if (e) break;
		if (parser->file_pos > parser->file_size) parser->file_size = parser->file_pos + 1;
		if (parser->on_progress) parser->on_progress(parser->sax_cbck, parser->file_pos, parser->file_size);
	}

#ifdef NO_GZIP
	if (feof(parser->f_in)) {
#else
	if (gzeof(parser->gz_in)) {
#endif
		if (!e) e = GF_EOS;
		if (parser->on_progress) parser->on_progress(parser->sax_cbck, parser->file_size, parser->file_size);

#ifdef NO_GZIP
		gf_fclose(parser->f_in);
		parser->f_in = NULL;
#else
		gzclose(parser->gz_in);
		parser->gz_in = 0;
#endif

		parser->elt_start_pos = parser->elt_end_pos = 0;
		parser->elt_name_start = parser->elt_name_end = 0;
		parser->att_name_start = 0;
		parser->current_pos = 0;
		parser->line_size = 0;
		parser->att_sep = 0;
		parser->file_pos = 0;
		parser->file_size = 0;
		parser->line_size = 0;
	}
	return e;
}

GF_EXPORT
GF_Err gf_xml_sax_parse_file(GF_SAXParser *parser, const char *fileName, gf_xml_sax_progress OnProgress)
{
	FILE *test;
	GF_Err e;
#ifndef NO_GZIP
	gzFile gzInput;
#endif
	unsigned char szLine[6];

	parser->on_progress = OnProgress;

	if (!strncmp(fileName, "gmem://", 7)) {
		u32 size;
		u8 *xml_mem_address;
		e = gf_blob_get_data(fileName, &xml_mem_address, &size);
		if (e) return e;

		parser->file_size = size;
		//copy possible BOM
		memcpy(szLine, xml_mem_address, 4);
		szLine[4] = szLine[5] = 0;

		parser->file_pos = 0;
		parser->elt_start_pos = 0;
		parser->current_pos = 0;

		e = gf_xml_sax_init(parser, szLine);
		if (e) return e;


		e = gf_xml_sax_parse(parser, xml_mem_address+4);
		if (parser->on_progress) parser->on_progress(parser->sax_cbck, parser->file_pos, parser->file_size);

		parser->elt_start_pos = parser->elt_end_pos = 0;
		parser->elt_name_start = parser->elt_name_end = 0;
		parser->att_name_start = 0;
		parser->current_pos = 0;
		parser->line_size = 0;
		parser->att_sep = 0;
		parser->file_pos = 0;
		parser->file_size = 0;
		parser->line_size = 0;
		return e;
	}

	/*check file exists and gets its size (zlib doesn't support SEEK_END)*/
	test = gf_fopen(fileName, "rb");
	if (!test) return GF_URL_ERROR;
	gf_fseek(test, 0, SEEK_END);
	assert(gf_ftell(test) < 0x80000000);
	parser->file_size = (u32) gf_ftell(test);
	gf_fclose(test);

	parser->file_pos = 0;
	parser->elt_start_pos = 0;
	parser->current_pos = 0;
	//open file and copy possible BOM
#ifdef NO_GZIP
	parser->f_in = gf_fopen(fileName, "rt");
	if (fread(szLine, 1, 4, parser->f_in) != 4) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[XML] Error loading BOM\n"));
	}
#else
	gzInput = gzopen(fileName, "rb");
	if (!gzInput) return GF_IO_ERR;
	parser->gz_in = gzInput;
	/*init SAX parser (unicode setup)*/
	gzread(gzInput, szLine, 4);
#endif

	szLine[4] = szLine[5] = 0;
	e = gf_xml_sax_init(parser, szLine);
	if (e) return e;

	return xml_sax_read_file(parser);
}

GF_EXPORT
Bool gf_xml_sax_binary_file(GF_SAXParser *parser)
{
	if (!parser) return GF_FALSE;
#ifdef NO_GZIP
	return GF_FALSE;
#else
	if (!parser->gz_in) return GF_FALSE;
	return (((z_stream*)parser->gz_in)->data_type==Z_BINARY) ? GF_TRUE : GF_FALSE;
#endif
}

GF_EXPORT
GF_SAXParser *gf_xml_sax_new(gf_xml_sax_node_start on_node_start,
                             gf_xml_sax_node_end on_node_end,
                             gf_xml_sax_text_content on_text_content,
                             void *cbck)
{
	GF_SAXParser *parser;
	GF_SAFEALLOC(parser, GF_SAXParser);
	if (!parser) return NULL;
	parser->entities = gf_list_new();
	parser->unicode_type = -1;
	parser->sax_node_start = on_node_start;
	parser->sax_node_end = on_node_end;
	parser->sax_text_content = on_text_content;
	parser->sax_cbck = cbck;
	return parser;
}

GF_EXPORT
void gf_xml_sax_del(GF_SAXParser *parser)
{
	xml_sax_reset(parser);
	gf_list_del(parser->entities);
#ifdef NO_GZIP
	if (parser->f_in) gf_fclose(parser->f_in);
#else
	if (parser->gz_in) gzclose(parser->gz_in);
#endif
	gf_free(parser);
}

GF_EXPORT
GF_Err gf_xml_sax_suspend(GF_SAXParser *parser, Bool do_suspend)
{
	parser->suspended = do_suspend;
	if (!do_suspend) {
#ifdef NO_GZIP
		if (parser->f_in) return xml_sax_read_file(parser);
#else
		if (parser->gz_in) return xml_sax_read_file(parser);
#endif
		return xml_sax_parse(parser, GF_FALSE);
	}
	return GF_OK;
}


GF_EXPORT
u32 gf_xml_sax_get_line(GF_SAXParser *parser) {
	return parser->line + 1 ;
}

#if 0 //unused
u32 gf_xml_sax_get_file_size(GF_SAXParser *parser)
{
#ifdef NO_GZIP
	return parser->f_in ? parser->file_size : 0;
#else
	return parser->gz_in ? parser->file_size : 0;
#endif
}

u32 gf_xml_sax_get_file_pos(GF_SAXParser *parser)
{
#ifdef NO_GZIP
	return parser->f_in ? parser->file_pos : 0;
#else
	return parser->gz_in ? parser->file_pos : 0;
#endif
}
#endif



GF_EXPORT
char *gf_xml_sax_peek_node(GF_SAXParser *parser, char *att_name, char *att_value, char *substitute, char *get_attr, char *end_pattern, Bool *is_substitute)
{
	u32 state, att_len, alloc_size, _len;
#ifdef NO_GZIP
	u64 pos;
#else
	z_off_t pos;
#endif
	Bool from_buffer;
	Bool dobreak=GF_FALSE;
	char szLine1[XML_INPUT_SIZE+2], szLine2[XML_INPUT_SIZE+2], *szLine, *cur_line, *sep, *start, first_c, *result;


#define CPYCAT_ALLOC(__str, __is_copy) _len = (u32) strlen(__str);\
							if ( _len + (__is_copy ? 0 : strlen(szLine))>=alloc_size) {\
								alloc_size = 1 + (u32) strlen(__str);	\
								if (!__is_copy) alloc_size += (u32) strlen(szLine); \
								szLine = gf_realloc(szLine, alloc_size);	\
							}\
							if (__is_copy) { memmove(szLine, __str, sizeof(char)*_len); szLine[_len] = 0; }\
							else strcat(szLine, __str); \

	from_buffer=GF_FALSE;
#ifdef NO_GZIP
	if (!parser->f_in) from_buffer=GF_TRUE;
#else
	if (!parser->gz_in) from_buffer=GF_TRUE;
#endif

	result = NULL;

	szLine1[0] = szLine2[0] = 0;
	pos=0;
	if (!from_buffer) {
#ifdef NO_GZIP
		pos = gf_ftell(parser->f_in);
#else
		pos = gztell(parser->gz_in);
#endif
	}
	att_len = (u32) strlen(parser->buffer + parser->att_name_start);
	if (att_len<2*XML_INPUT_SIZE) att_len = 2*XML_INPUT_SIZE;
	alloc_size = att_len;
	szLine = (char *) gf_malloc(sizeof(char)*alloc_size);
	strcpy(szLine, parser->buffer + parser->att_name_start);
	cur_line = szLine;
	att_len = (u32) strlen(att_value);
	state = 0;
	goto retry;

	while (1) {
		u32 read;
		u8 sep_char;
		if (!from_buffer) {
#ifdef NO_GZIP
			if (feof(parser->f_in)) break;
#else
			if (gzeof(parser->gz_in)) break;
#endif
		}

		if (dobreak) break;

		if (cur_line == szLine2) {
			cur_line = szLine1;
		} else {
			cur_line = szLine2;
		}
		if (from_buffer) {
			dobreak=GF_TRUE;
		} else {
#ifdef NO_GZIP
			read = (u32)fread(cur_line, 1, XML_INPUT_SIZE, parser->f_in);
#else
			read = gzread(parser->gz_in, cur_line, XML_INPUT_SIZE);
#endif
			cur_line[read] = cur_line[read+1] = 0;

			CPYCAT_ALLOC(cur_line, 0);
		}

		if (end_pattern) {
			start  = strstr(szLine, end_pattern);
			if (start) {
				start[0] = 0;
				dobreak = GF_TRUE;
			}
		}

retry:
		if (state == 2) goto fetch_attr;
		sep = strstr(szLine, att_name);
		if (!sep && !state) {
			state = 0;
			start = strrchr(szLine, '<');
			if (start) {
				CPYCAT_ALLOC(start, 1);
			} else {
				CPYCAT_ALLOC(cur_line, 1);
			}
			continue;
		}
		if (!state) {
			state = 1;
			/*load next line*/
			first_c = sep[0];
			sep[0] = 0;
			start = strrchr(szLine, '<');
			if (!start)
				goto exit;
			sep[0] = first_c;
			CPYCAT_ALLOC(start, 1);
			sep = strstr(szLine, att_name);
		}
		sep = strchr(sep, '=');
		if (!sep) {
			state = 0;
			CPYCAT_ALLOC(cur_line, 1);
			continue;
		}
		while (sep[0] && (sep[0] != '\"') && (sep[0] != '\'') ) sep++;
		if (!sep[0]) continue;
		sep_char = sep[0];
		sep++;
		while (sep[0] && strchr(" \n\r\t", sep[0]) ) sep++;
		if (!sep[0]) continue;
		if (!strchr(sep, sep_char))
			continue;

		/*found*/
		if (!strncmp(sep, att_value, att_len)) {
			u32 pos;
			sep = szLine + 1;
			while (strchr(" \t\r\n", sep[0])) sep++;
			pos = 0;
			while (!strchr(" \t\r\n", sep[pos])) pos++;
			first_c = sep[pos];
			sep[pos] = 0;
			state = 2;
			if (!substitute || !get_attr || strcmp(sep, substitute) ) {
				if (is_substitute) *is_substitute = GF_FALSE;
				result = gf_strdup(sep);
				goto exit;
			}
			sep[pos] = first_c;
fetch_attr:
			sep = strstr(szLine + 1, get_attr);
			if (!sep) {
				CPYCAT_ALLOC(cur_line, 1);
				continue;
			}
			sep += strlen(get_attr);
			while (strchr("= \t\r\n", sep[0])) sep++;
			sep++;
			pos = 0;
			while (!strchr(" \t\r\n/>", sep[pos])) pos++;
			sep[pos-1] = 0;
			result = gf_strdup(sep);
			if (is_substitute) *is_substitute = GF_TRUE;
			goto exit;
		}
		state = 0;
		CPYCAT_ALLOC(sep, 1);
		goto retry;
	}
exit:
	gf_free(szLine);

	if (!from_buffer) {
#ifdef NO_GZIP
		gf_fseek(parser->f_in, pos, SEEK_SET);
#else
		gzrewind(parser->gz_in);
		gzseek(parser->gz_in, pos, SEEK_SET);
#endif
	}
	return result;
}

GF_EXPORT
const char *gf_xml_sax_get_error(GF_SAXParser *parser)
{
	return parser->err_msg;
}


struct _peek_type
{
	GF_SAXParser *parser;
	char *res;
};

static void on_peek_node_start(void *cbk, const char *name, const char *ns, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	struct _peek_type *pt = (struct _peek_type*)cbk;
	pt->res = gf_strdup(name);
	pt->parser->suspended = GF_TRUE;
}

GF_EXPORT
char *gf_xml_get_root_type(const char *file, GF_Err *ret)
{
	GF_Err e;
	struct _peek_type pt;
	pt.res = NULL;
	pt.parser = gf_xml_sax_new(on_peek_node_start, NULL, NULL, &pt);
	e = gf_xml_sax_parse_file(pt.parser, file, NULL);
	if (ret) *ret = e;
	gf_xml_sax_del(pt.parser);
	return pt.res;
}


GF_EXPORT
u32 gf_xml_sax_get_node_start_pos(GF_SAXParser *parser)
{
	return parser->elt_start_pos;
}

GF_EXPORT
u32 gf_xml_sax_get_node_end_pos(GF_SAXParser *parser)
{
	return parser->elt_end_pos;
}

struct _tag_dom_parser
{
	GF_SAXParser *parser;
	GF_List *stack;
	//root node being parsed
	GF_XMLNode *root;
	//usually only one :)
	GF_List *root_nodes;
	u32 depth;

	void (*OnProgress)(void *cbck, u64 done, u64 tot);
	void *cbk;
};


GF_EXPORT
void gf_xml_dom_node_del(GF_XMLNode *node)
{
	if (!node) return;
	if (node->attributes) {
		while (gf_list_count(node->attributes)) {
			GF_XMLAttribute *att = (GF_XMLAttribute *)gf_list_last(node->attributes);
			gf_list_rem_last(node->attributes);
			if (att->name) gf_free(att->name);
			if (att->value) gf_free(att->value);
			gf_free(att);
		}
		gf_list_del(node->attributes);
	}
	if (node->content) {
		while (gf_list_count(node->content)) {
			GF_XMLNode *child = (GF_XMLNode *)gf_list_last(node->content);
			gf_list_rem_last(node->content);
			gf_xml_dom_node_del(child);
		}
		gf_list_del(node->content);
	}
	if (node->ns) gf_free(node->ns);
	if (node->name) gf_free(node->name);
	gf_free(node);
}

static void on_dom_node_start(void *cbk, const char *name, const char *ns, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	u32 i;
	GF_DOMParser *par = (GF_DOMParser *) cbk;
	GF_XMLNode *node;

	if (par->root && !gf_list_count(par->stack)) {
		par->parser->suspended = GF_TRUE;
		return;
	}

	GF_SAFEALLOC(node, GF_XMLNode);
	if (!node) {
		par->parser->sax_state = SAX_STATE_ALLOC_ERROR;
		return;
	}
	node->attributes = gf_list_new();
	node->content = gf_list_new();
	node->name = gf_strdup(name);
	if (ns) node->ns = gf_strdup(ns);
	gf_list_add(par->stack, node);
	if (!par->root) {
		par->root = node;
		gf_list_add(par->root_nodes, node);
	}

	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att;
		GF_SAFEALLOC(att, GF_XMLAttribute);
		if (! att) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[SAX] Failed to allocate attribute"));
			par->parser->sax_state = SAX_STATE_ALLOC_ERROR;
			return;
		}
		att->name = gf_strdup(attributes[i].name);
		att->value = gf_strdup(attributes[i].value);
		gf_list_add(node->attributes, att);
	}
}

static void on_dom_node_end(void *cbk, const char *name, const char *ns)
{
	GF_DOMParser *par = (GF_DOMParser *)cbk;
	GF_XMLNode *last = (GF_XMLNode *)gf_list_last(par->stack);
	gf_list_rem_last(par->stack);

	if (!last || (strlen(last->name)!=strlen(name)) || strcmp(last->name, name) || (!ns && last->ns) || (ns && !last->ns) || (ns && strcmp(last->ns, ns) ) ) {
		s32 idx;
		format_sax_error(par->parser, 0, "Invalid node stack: closing node is %s but %s was expected", name, last ? last->name : "unknown");
		par->parser->suspended = GF_TRUE;
		gf_xml_dom_node_del(last);
		if (last == par->root)
			par->root=NULL;
		idx = gf_list_find(par->root_nodes, last);
		if (idx != -1)
			gf_list_rem(par->root_nodes, idx);
		return;
	}

	if (last != par->root) {
		GF_XMLNode *node = (GF_XMLNode *)gf_list_last(par->stack);
		assert(node->content);
		assert(gf_list_find(node->content, last) == -1);
		gf_list_add(node->content, last);
	}
}

static void on_dom_text_content(void *cbk, const char *content, Bool is_cdata)
{
	GF_DOMParser *par = (GF_DOMParser *)cbk;
	GF_XMLNode *node;
	GF_XMLNode *last = (GF_XMLNode *)gf_list_last(par->stack);
	if (!last) return;
	assert(last->content);

	GF_SAFEALLOC(node, GF_XMLNode);
	if (!node) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[SAX] Failed to allocate XML node"));
		par->parser->sax_state = SAX_STATE_ALLOC_ERROR;
		return;
	}
	node->type = is_cdata ? GF_XML_CDATA_TYPE : GF_XML_TEXT_TYPE;
	node->name = gf_strdup(content);
	gf_list_add(last->content, node);
}

GF_EXPORT
GF_DOMParser *gf_xml_dom_new()
{
	GF_DOMParser *dom;
	GF_SAFEALLOC(dom, GF_DOMParser);
	if (!dom) return NULL;

	dom->root_nodes = gf_list_new();
	return dom;
}

static void gf_xml_dom_reset(GF_DOMParser *dom, Bool full_reset)
{
	if (full_reset && dom->parser) {
		gf_xml_sax_del(dom->parser);
		dom->parser = NULL;
	}

	if (dom->stack) {
		while (gf_list_count(dom->stack)) {
			GF_XMLNode *n = (GF_XMLNode *)gf_list_last(dom->stack);
			gf_list_rem_last(dom->stack);
			if (dom->root==n) {
				gf_list_del_item(dom->root_nodes, n);
				dom->root = NULL;
			}
			gf_xml_dom_node_del(n);
		}
		gf_list_del(dom->stack);
		dom->stack = NULL;
	}
	if (full_reset && gf_list_count(dom->root_nodes) ) {
		while (gf_list_count(dom->root_nodes)) {
			GF_XMLNode *n = (GF_XMLNode *)gf_list_last(dom->root_nodes);
			gf_list_rem_last(dom->root_nodes);
			gf_xml_dom_node_del(n);
		}
		dom->root = NULL;
	}
}

GF_EXPORT
void gf_xml_dom_del(GF_DOMParser *parser)
{
	if (!parser)
		return;

	gf_xml_dom_reset(parser, GF_TRUE);
	gf_list_del(parser->root_nodes);
	gf_free(parser);
}

#if 0 //unused
GF_XMLNode *gf_xml_dom_detach_root(GF_DOMParser *parser)
{
	GF_XMLNode *root = parser->root;
	parser->root = NULL;
	return root;
}
#endif

static void dom_on_progress(void *cbck, u64 done, u64 tot)
{
	GF_DOMParser *dom = (GF_DOMParser *)cbck;
	dom->OnProgress(dom->cbk, done, tot);
}

GF_EXPORT
GF_Err gf_xml_dom_parse(GF_DOMParser *dom, const char *file, gf_xml_sax_progress OnProgress, void *cbk)
{
	GF_Err e;
	gf_xml_dom_reset(dom, GF_TRUE);
	dom->stack = gf_list_new();
	dom->parser = gf_xml_sax_new(on_dom_node_start, on_dom_node_end, on_dom_text_content, dom);
	dom->OnProgress = OnProgress;
	dom->cbk = cbk;
	e = gf_xml_sax_parse_file(dom->parser, file, OnProgress ? dom_on_progress : NULL);
	gf_xml_dom_reset(dom, GF_FALSE);
	return e<0 ? e : GF_OK;
}

GF_EXPORT
GF_Err gf_xml_dom_parse_string(GF_DOMParser *dom, char *string)
{
	GF_Err e;
	gf_xml_dom_reset(dom, GF_TRUE);
	dom->stack = gf_list_new();
	dom->parser = gf_xml_sax_new(on_dom_node_start, on_dom_node_end, on_dom_text_content, dom);
	e = gf_xml_sax_init(dom->parser, (unsigned char *) string);
	gf_xml_dom_reset(dom, GF_FALSE);
	return e<0 ? e : GF_OK;
}

#if 0 //unused
GF_XMLNode *gf_xml_dom_create_root(GF_DOMParser *parser, const char* name) {
	GF_XMLNode * root;
	if (!parser) return NULL;

	GF_SAFEALLOC(root, GF_XMLNode);
	if (!root) return NULL;
	root->name = gf_strdup(name);

	return root;
}
#endif

GF_EXPORT
GF_XMLNode *gf_xml_dom_get_root(GF_DOMParser *parser)
{
	return parser ? parser->root : NULL;
}
GF_EXPORT
const char *gf_xml_dom_get_error(GF_DOMParser *parser)
{
	return gf_xml_sax_get_error(parser->parser);
}
GF_EXPORT
u32 gf_xml_dom_get_line(GF_DOMParser *parser)
{
	return gf_xml_sax_get_line(parser->parser);
}

GF_EXPORT
u32 gf_xml_dom_get_root_nodes_count(GF_DOMParser *parser)
{
	return parser? gf_list_count(parser->root_nodes) : 0;
}

GF_EXPORT
GF_XMLNode *gf_xml_dom_get_root_idx(GF_DOMParser *parser, u32 idx)
{
	return parser ? (GF_XMLNode*)gf_list_get(parser->root_nodes, idx) : NULL;
}


static void gf_xml_dom_node_serialize(GF_XMLNode *node, Bool content_only, char **str, u32 *alloc_size, u32 *size)
{
	u32 i, count, vlen;
	char *name;

#define SET_STRING(v)	\
	vlen = (u32) strlen(v);	\
	if (vlen+ (*size) >= (*alloc_size)) {	\
		(*alloc_size) += 1024;	\
		(*str) = gf_realloc((*str), (*alloc_size));	\
		(*str)[(*size)] = 0;	\
	}	\
	strcat((*str), v);	\
	*size += vlen;	\

	switch (node->type) {
	case GF_XML_CDATA_TYPE:
		SET_STRING("![CDATA[");
		SET_STRING(node->name);
		SET_STRING("]]>");
		return;
	case GF_XML_TEXT_TYPE:
		name = node->name;
		if ((name[0]=='\r') && (name[1]=='\n'))
			name++;
		SET_STRING(name);
		return;
	}

	if (!content_only) {
		SET_STRING("<");
		if (node->ns) {
			SET_STRING(node->ns);
			SET_STRING(":");
		}
		SET_STRING(node->name);
		SET_STRING(" ");
		count = gf_list_count(node->attributes);
		for (i=0; i<count; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute*)gf_list_get(node->attributes, i);
			SET_STRING(att->name);
			SET_STRING("=\"");
			SET_STRING(att->value);
			SET_STRING("\" ");
		}

		if (!gf_list_count(node->content)) {
			SET_STRING("/>");
			return;
		}
		SET_STRING(">");
	}

	count = gf_list_count(node->content);
	for (i=0; i<count; i++) {
		GF_XMLNode *child = (GF_XMLNode*)gf_list_get(node->content, i);
		gf_xml_dom_node_serialize(child, GF_FALSE, str, alloc_size, size);
	}
	if (!content_only) {
		SET_STRING("</");
		if (node->ns) {
			SET_STRING(node->ns);
			SET_STRING(":");
		}
		SET_STRING(node->name);
		SET_STRING(">");
	}
}

GF_EXPORT
char *gf_xml_dom_serialize(GF_XMLNode *node, Bool content_only)
{
	u32 alloc_size = 0;
	u32 size = 0;
	char *str = NULL;
	gf_xml_dom_node_serialize(node, content_only, &str, &alloc_size, &size);
	return str;
}

#if 0 //unused
GF_XMLAttribute *gf_xml_dom_set_attribute(GF_XMLNode *node, const char* name, const char* value) {
	GF_XMLAttribute *att;
	if (!name || !value) return NULL;
	if (!node->attributes) {
		node->attributes = gf_list_new();
		if (!node->attributes) return NULL;
	}

	att = gf_xml_dom_create_attribute(name, value);
	if (!att) return NULL;
	gf_list_add(node->attributes, att);
	return att;
}

GF_XMLAttribute *gf_xml_dom_get_attribute(GF_XMLNode *node, const char* name) {
	u32 i = 0;
	GF_XMLAttribute *att;
	if (!node || !name) return NULL;

	while ( (att = (GF_XMLAttribute*)gf_list_enum(node->attributes, &i))) {
		if (!strcmp(att->name, name)) {
			return att;
		}
	}

	return NULL;
}

#endif

GF_EXPORT
GF_XMLAttribute *gf_xml_dom_create_attribute(const char* name, const char* value) {
	GF_XMLAttribute *att;
	GF_SAFEALLOC(att, GF_XMLAttribute);
	if (!att) return NULL;

	att->name = gf_strdup(name);
	att->value = gf_strdup(value);
	return att;
}


GF_EXPORT
GF_Err gf_xml_dom_append_child(GF_XMLNode *node, GF_XMLNode *child) {
	if (!node || !child) return GF_BAD_PARAM;
	if (!node->content) {
		node->content = gf_list_new();
		if (!node->content) return GF_OUT_OF_MEM;
	}
	return gf_list_add(node->content, child);
}

GF_EXPORT
GF_Err gf_xml_dom_rem_child(GF_XMLNode *node, GF_XMLNode *child) {
	s32 idx;
	if (!node || !child || !node->content) return GF_BAD_PARAM;
	idx = gf_list_find(node->content, child);
	if (idx == -1) return GF_BAD_PARAM;
	return gf_list_rem(node->content, idx);
}

#if 0
GF_XMLNode* gf_xml_dom_node_new(const char* ns, const char* name) {
	GF_XMLNode* node;
	GF_SAFEALLOC(node, GF_XMLNode);
	if (!node) return NULL;
	if (ns) {
		node->ns = gf_strdup(ns);
		if (!node->ns) {
			gf_free(node);
			return NULL;
		}
	}

	if (name) {
		node->name = gf_strdup(name);
		if (!node->name) {
			gf_free(node->ns);
			gf_free(node);
			return NULL;
		}
	}
	return node;
}
#endif //unused

#include <gpac/base_coding.h>

#define XML_SCAN_INT(_fmt, _value)	\
	{\
	if (strstr(att->value, "0x")) { u32 __i; sscanf(att->value+2, "%x", &__i); _value = __i; }\
	else if (strstr(att->value, "0X")) { u32 __i; sscanf(att->value+2, "%X", &__i); _value = __i; }\
	else sscanf(att->value, _fmt, &_value); \
	}\


GF_Err gf_xml_parse_bit_sequence_bs(GF_XMLNode *bsroot, const char *parent_url, GF_BitStream *bs)
{
	u32 i, j;
	GF_XMLNode *node;
	GF_XMLAttribute *att;

	i=0;
	while ((node = (GF_XMLNode *) gf_list_enum(bsroot->content, &i))) {
		u32 nb_bits = 0;
		u32 size = 0;
		u64 offset = 0;
		s64 value = 0;
		bin128 word128;
		Float val_float = 0;
		Double val_double = 0;
		Bool use_word128 = GF_FALSE;
		Bool use_text = GF_FALSE;
		Bool big_endian = GF_TRUE;
		Bool has_float = GF_FALSE;
		Bool has_double = GF_FALSE;
		const char *szFile = NULL;
		const char *szString = NULL;
		const char *szBase64 = NULL;
		const char *szData = NULL;
		if (node->type) continue;

		if (stricmp(node->name, "BS") ) {
			gf_xml_parse_bit_sequence_bs(node, parent_url, bs);
			continue;
		}

		j=0;
		while ( (att = (GF_XMLAttribute *)gf_list_enum(node->attributes, &j))) {
			if (!stricmp(att->name, "bits")) {
				XML_SCAN_INT("%d", nb_bits);
			} else if (!stricmp(att->name, "value")) {
				XML_SCAN_INT(LLD, value);
			} else if (!stricmp(att->name, "float")) {
				sscanf(att->value, "%f", &val_float);
				has_float = GF_TRUE;
			} else if (!stricmp(att->name, "double")) {
				sscanf(att->value, "%lf", &val_double);
				has_double = GF_TRUE;
			} else if (!stricmp(att->name, "mediaOffset") || !stricmp(att->name, "dataOffset")) {
				XML_SCAN_INT(LLU, offset);
			} else if (!stricmp(att->name, "dataLength")) {
				XML_SCAN_INT("%u", size);
			} else if (!stricmp(att->name, "mediaFile") || !stricmp(att->name, "dataFile")) {
				szFile = att->value;
			} else if (!stricmp(att->name, "text") || !stricmp(att->name, "string")) {
				szString = att->value;
			} else if (!stricmp(att->name, "fcc")) {
				value = GF_4CC(att->value[0], att->value[1], att->value[2], att->value[3]);
				nb_bits = 32;
			} else if (!stricmp(att->name, "ID128")) {
				GF_Err e = gf_bin128_parse(att->value, word128);
                if (e != GF_OK) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML/NHML] Cannot parse ID128\n"));
                    return e;
                }
				use_word128 = GF_TRUE;
			} else if (!stricmp(att->name, "textmode")) {
				if (!strcmp(att->value, "yes")) use_text = GF_TRUE;
			} else if (!stricmp(att->name, "data64")) {
				szBase64 = att->value;
			} else if (!stricmp(att->name, "data")) {
				szData = att->value;
				if (!strnicmp(szData, "0x", 2)) szData += 2;
			} else if (!stricmp(att->name, "endian") && !stricmp(att->value, "little")) {
				big_endian = GF_FALSE;
			}
		}
		if (szString) {
			u32 len = (u32) strlen(szString);
			if (nb_bits)
				gf_bs_write_int(bs, len, nb_bits);

			gf_bs_write_data(bs, szString, len);
		} else if (szBase64) {
			u32 len = (u32) strlen(szBase64);
			char *data = (char *) gf_malloc(sizeof(char)*len);
			u32 ret;
			if (!data ) return GF_OUT_OF_MEM;

			ret = (u32) gf_base64_decode((char *)szBase64, len, data, len);
			if ((s32) ret >=0) {
				gf_bs_write_int(bs, ret, nb_bits);
				gf_bs_write_data(bs, data, ret);
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML/NHML] Error decoding base64 %s\n", att->value));
				gf_free(data);
				return GF_BAD_PARAM;
			}
			gf_free(data);
		} else if (szData) {
			u32 j, len = (u32) strlen(szData);
			char *data = (char *) gf_malloc(sizeof(char)*len/2);
			if (!data) return GF_OUT_OF_MEM;

			for (j=0; j<len; j+=2) {
				u32 v;
				char szV[5];
				sprintf(szV, "%c%c", szData[j], szData[j+1]);
				sscanf(szV, "%x", &v);
				data[j/2] = v;
			}
			gf_bs_write_int(bs, len/2, nb_bits);
			gf_bs_write_data(bs, data, len/2);
			gf_free(data);
		} else if (has_float) {
			gf_bs_write_float(bs, val_float);
		} else if (has_double) {
			gf_bs_write_double(bs, val_double);
		} else if (nb_bits) {
			if (!big_endian) {
				if (nb_bits == 16)
					gf_bs_write_u16_le(bs, (u32)value);
				else if (nb_bits == 32)
					gf_bs_write_u32_le(bs, (u32)value);
				else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML/NHML] Little-endian values can only be 16 or 32-bit\n"));
					return GF_BAD_PARAM;
				}
			}
			else {
				if (nb_bits<33) gf_bs_write_int(bs, (s32) value, nb_bits);
				else gf_bs_write_long_int(bs, value, nb_bits);
			}
		} else if (szFile) {
			u32 read, remain;
			char block[1024];
			FILE *_tmp = NULL;
			if (parent_url) {
				char *f_url = gf_url_concatenate(parent_url, szFile);
				_tmp = gf_fopen(f_url, use_text ? "rt" : "rb");
				gf_free(f_url);
			} else {
				_tmp = gf_fopen(szFile, use_text ? "rt" : "rb");
			}

			if (!_tmp) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[XML/NHML] Error opening file %s\n", szFile));
				return GF_URL_ERROR;
			}

			if (!size) {
				gf_fseek(_tmp, 0, SEEK_END);
				size = (u32) gf_ftell(_tmp);
				//if offset only copy from offset until end
				if ((u64) size > offset)
					size -= (u32) offset;
			}
			remain = size;
			gf_fseek(_tmp, offset, SEEK_SET);
			while (remain) {
				read = (u32) fread(block, 1, (remain>1024) ? 1024 : remain, _tmp);
				if ((s32) read < 0) {
					gf_fclose(_tmp);
					return GF_IO_ERR;
				}

				gf_bs_write_data(bs, block, read);
				remain -= size;
			}
			gf_fclose(_tmp);
		} else if (use_word128) {
			gf_bs_write_data(bs, (char *)word128, 16);
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_xml_parse_bit_sequence(GF_XMLNode *bsroot, const char *parent_url, u8 **data, u32 *data_size)
{
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (!bs) return GF_OUT_OF_MEM;

	gf_xml_parse_bit_sequence_bs(bsroot, parent_url, bs);

	gf_bs_align(bs);
	gf_bs_get_content(bs, data, data_size);
	gf_bs_del(bs);
	return GF_OK;
}

GF_Err gf_xml_get_element_check_namespace(const GF_XMLNode *n, const char *expected_node_name, const char *expected_ns_prefix) {
	u32 i;
	GF_XMLAttribute *att;

	/*check we are processing the expected node*/
	if (expected_node_name && strcmp(expected_node_name, n->name)) {
		return GF_SG_UNKNOWN_NODE;
	}

	/*check for previously declared prefix (to be manually provided)*/
	if (!n->ns) {
		return GF_OK;
	}
	if (expected_ns_prefix && !strcmp(expected_ns_prefix, n->ns)) {
		return GF_OK;
	}

	/*look for new namespace in attributes*/
	i = 0;
	while ( (att = (GF_XMLAttribute*)gf_list_enum(n->attributes, &i)) ) {
		const char *ns;
		ns = strstr(att->name, ":");
		if (ns) {
			if (!strncmp(att->name, "xmlns", 5)) {
				if (!strcmp(ns+1, n->ns)) {
					return GF_OK;
				}
			} else if (ns) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[XML] Unsupported attribute namespace \"%s\": ignoring\n", att->name));
				continue;
			}
		}
	}

	GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[XML] Unresolved namespace \"%s\" for node \"%s\"\n", n->ns, n->name));
	return GF_BAD_PARAM;
}

#endif /*GPAC_DISABLE_CORE_TOOLS*/
