/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *			Copyright (c) 2005-200X ENST
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
/*since 0.2.2, we use zlib for xmt/x3d reading to handle gz files*/
#include <zlib.h>

#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)
#pragma comment(lib, "zlib")
#endif


static GF_Err gf_xml_sax_parse_intern(GF_SAXParser *parser, char *current);

static char *xml_translate_xml_string(char *str)
{
	char *value;
	u32 size, i, j;
	if (!str || !strlen(str)) return NULL;
	value = (char *)malloc(sizeof(char) * 500);
	size = 500;
	i = j = 0;
	while (str[i]) {
		if (j >= size) {
			size += 500;
			value = (char *)realloc(value, sizeof(char)*size);
		}
		if (str[i] == '&') {
			if (str[i+1]=='#') {
				char szChar[20], *end;
				u16 wchar[2];
				u32 val;
				const unsigned short *srcp;
				strncpy(szChar, str+i, 10);
				end = strchr(szChar, ';');
				assert(end);
				end[1] = 0;
				i+=strlen(szChar);
				wchar[1] = 0;
				if (szChar[2]=='x')
					sscanf(szChar, "&#x%x;", &val);
				else
					sscanf(szChar, "&#%d;", &val);
				wchar[0] = val;
				srcp = wchar;
				j += gf_utf8_wcstombs(&value[j], 20, &srcp);
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
				j++; i++;
			}
		} else {
			value[j] = str[i];
			j++; i++;
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
};

typedef struct
{
	u32 name_start, name_end;
	u32 val_start, val_end;
	Bool has_entities;
} GF_XMLSaxAttribute;


//#define NO_GZIP


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
		parser->sax_attrs = (GF_XMLSaxAttribute *)realloc(parser->sax_attrs, sizeof(GF_XMLSaxAttribute)*parser->nb_alloc_attrs);
		parser->attrs = (GF_XMLAttribute *)realloc(parser->attrs, sizeof(GF_XMLAttribute)*parser->nb_alloc_attrs);
	}
	return &parser->sax_attrs[parser->nb_attrs++];
}

static void xml_sax_swap(GF_SAXParser *parser)
{
	if (parser->current_pos && ((parser->sax_state==SAX_STATE_TEXT_CONTENT) || (parser->sax_state==SAX_STATE_COMMENT) ) ) {
		assert(parser->line_size >= parser->current_pos);
		parser->line_size -= parser->current_pos;
		parser->file_pos += parser->current_pos;
		if (parser->line_size) memmove(parser->buffer, parser->buffer + parser->current_pos, sizeof(char)*parser->line_size); 
		parser->buffer[parser->line_size] = 0;
		parser->current_pos = 0;
	}
}

static void xml_sax_node_end(GF_SAXParser *parser, Bool had_children)
{
	char *name, *sep, c;

	assert(parser->elt_name_start && parser->elt_name_end && parser->node_depth);
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
	Bool has_entities = 0;
	u32 i;
	char *sep, c, *name;

	assert(parser->elt_name_start && parser->elt_name_end);
	c = parser->buffer[parser->elt_name_end - 1];
	parser->buffer[parser->elt_name_end - 1] = 0;
	name = parser->buffer + parser->elt_name_start - 1;

	for (i=0;i<parser->nb_attrs; i++) {
		parser->attrs[i].name = parser->buffer + parser->sax_attrs[i].name_start - 1;
		parser->buffer[parser->sax_attrs[i].name_end-1] = 0;
		parser->attrs[i].value = parser->buffer + parser->sax_attrs[i].val_start - 1;
		parser->buffer[parser->sax_attrs[i].val_end-1] = 0;

		if (strchr(parser->attrs[i].value, '&')) {
			parser->sax_attrs[i].has_entities = 1;
			has_entities = 1;
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
		for (i=0;i<parser->nb_attrs; i++) {
			if (parser->sax_attrs[i].has_entities) {
				parser->sax_attrs[i].has_entities = 0;
				free(parser->attrs[i].value);
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
					if (parser->current_pos+1 == parser->line_size) return 1;
					if (parser->buffer[parser->current_pos+1]=='>') {
						parser->current_pos+=2;
						parser->elt_end_pos = parser->file_pos + parser->current_pos - 1;
						/*done parsing attr AND elements*/
						if (!parser->init_state) {
							xml_sax_node_start(parser);
							xml_sax_node_end(parser, 0);
						} else {
							parser->nb_attrs = 0;
						}
						parser->sax_state = (parser->init_state) ? SAX_STATE_ELEMENT : SAX_STATE_TEXT_CONTENT;
						parser->text_start = parser->text_end = 0;
						return 0;
					}
					if (!parser->in_quote && (c=='/')) {
						if (!parser->init_state) {
							parser->sax_state = SAX_STATE_SYNTAX_ERROR;
							sprintf(parser->err_msg, "Markup error");
							return 1;
						}
					}
					break;
				case '"':
					if (parser->sax_state==SAX_STATE_ATT_VALUE) break;
					if (parser->in_quote && (parser->in_quote!=c) ) {
						parser->sax_state = SAX_STATE_SYNTAX_ERROR;
						sprintf(parser->err_msg, "Markup error");
						return 1;
					}
					if (parser->in_quote) parser->in_quote = 0;
					else parser->in_quote = c;
					break;
				case '>':
					parser->current_pos+=1;
					/*end of <!DOCTYPE>*/
					if (parser->init_state) {
						if (parser->init_state==1) {
							parser->sax_state = SAX_STATE_SYNTAX_ERROR;
							sprintf(parser->err_msg, "Invalid DOCTYPE");
							return 1;
						}
						parser->sax_state = SAX_STATE_ELEMENT;
						return 0;
					}
					/*done parsing attr*/
					parser->sax_state = SAX_STATE_TEXT_CONTENT;
					xml_sax_node_start(parser);
					return 0;
				case '[':
					if (parser->init_state) {
						parser->current_pos+=1;
						if (parser->init_state==1) {
							parser->sax_state = SAX_STATE_SYNTAX_ERROR;
							sprintf(parser->err_msg, "Invalid DOCTYPE");
							return 1;
						}
						parser->sax_state = SAX_STATE_ELEMENT;
						return 0;
					}
					break;
				case '<':
					parser->sax_state = SAX_STATE_SYNTAX_ERROR;
					sprintf(parser->err_msg, "Invalid character");
					return 0;
				/*first char of attr name*/
				default:
					parser->att_name_start = parser->current_pos + 1;
					break;
				}
				parser->current_pos++;
				if (parser->att_name_start) break;
			}
			if (parser->current_pos == parser->line_size) return 1;
		}

		if (parser->init_state==2) {
			sep = strchr(parser->buffer + parser->att_name_start - 1, parser->in_quote ?  parser->in_quote : ' ');
			/*not enough data*/
			if (!sep) return 1;
			parser->current_pos = sep - parser->buffer;
			parser->att_name_start = 0;
			if (parser->in_quote) {
				parser->current_pos++;
				parser->in_quote = 0;
			}
			return 0;
		}

		/*looking for '"'*/
		if (parser->att_name_start) {
			sep = strchr(parser->buffer + parser->att_name_start - 1, '=');
			/*not enough data*/
			if (!sep) return 1;

			parser->current_pos = sep - parser->buffer;
			att = xml_get_sax_attribute(parser);
			att->name_start = parser->att_name_start;
			att->name_end = parser->current_pos + 1;
			while (strchr(" \n\t", parser->buffer[att->name_end - 2])) {
				assert(att->name_end);
				att->name_end --;
			}
			att->has_entities = 0;
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
			if (parser->current_pos == parser->line_size) return 1;
		} 

		assert(parser->att_sep);
		sep = strchr(parser->buffer + parser->current_pos, parser->att_sep);
		if (!sep) return 1;

		parser->current_pos = sep - parser->buffer;
		att->val_end = parser->current_pos + 1;
		parser->current_pos++;

		/*"style" always at the begining of the attributes for ease of parsing*/
		if (!strncmp(parser->buffer + att->name_start-1, "style", 5)) {
			GF_XMLSaxAttribute prev = parser->sax_attrs[0];
			parser->sax_attrs[0] = *att;
			*att = prev;
		}
		parser->att_sep = 0;
		parser->sax_state = SAX_STATE_ATT_NAME;
		parser->att_name_start = 0;
		return 0;
	}
	return 1;
}


typedef struct
{
	char *name;
	char *value;
	u8 sep;
} XML_Entity;

static void xml_sax_flush_text(GF_SAXParser *parser)
{
	char *text, c;
	if (!parser->text_start || parser->init_state || !parser->sax_text_content) return;

	/* This optimization should be done at the application level
	   generic XML parsing should not try to remove any character !!*/
#if 0
	u32 offset;
	offset = 0;
	while (parser->text_start+offset<parser->text_end) {
		c = parser->buffer[parser->text_start-1+offset];
		if (c=='\r') offset++;
		else if (c==' ') offset++;
		else if (c=='\n') {
			parser->line++;
			offset++;
		} else {
			break;
		}
	}
	parser->text_start+=offset;
	if (parser->text_start == parser->text_end) {
		parser->text_start = parser->text_end = 0;
		return;
	}

	offset = 0;
	while (offset<parser->text_end) {
		c = parser->buffer[parser->text_end-2-offset];
		if (c=='\r') offset++;
		else if (c==' ') offset++;
		else if (c=='\n') {
			parser->line++;
			offset++;
		} else {
			break;
		}
	}
	parser->text_end-=offset;
#endif

	assert(parser->text_start < parser->text_end);

	c = parser->buffer[parser->text_end-1];
	parser->buffer[parser->text_end-1] = 0;
	text = parser->buffer + parser->text_start-1;

	/*solve XML built-in entities*/
	if (strchr(text, '&') && strchr(text, ';')) {
		char *xml_text = xml_translate_xml_string(text);
		if (xml_text) {
			parser->sax_text_content(parser->sax_cbck, xml_text, (parser->sax_state==SAX_STATE_CDATA) ? 1 : 0);
			free(xml_text);
		}
	} else {
		parser->sax_text_content(parser->sax_cbck, text, (parser->sax_state==SAX_STATE_CDATA) ? 1 : 0);
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
	text = strdup(parser->buffer + parser->text_start-1);
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
			ent->name = strdup(szName);
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
			if (!ent->value) ent->value = strdup("");

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
		u32 size = cd_end - (parser->buffer + parser->current_pos);
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
		return 0;
	}

	parser->current_pos += 3 + (u32) (end - (parser->buffer + parser->current_pos) );
	assert(parser->current_pos <= parser->line_size);
	parser->sax_state = SAX_STATE_TEXT_CONTENT;
	parser->text_start = parser->text_end = 0;
	xml_sax_swap(parser);
	return 1;
}



static GF_Err xml_sax_parse(GF_SAXParser *parser, Bool force_parse)
{
	u32 i = 0;
	Bool is_text, is_end;
	u8 c;
	char *elt, sep;

	is_text = 0;
	while (parser->current_pos<parser->line_size) {
		if (!force_parse && parser->suspended) goto exit;

restart:
		is_text = 0;
		switch (parser->sax_state) {
		/*load an XML element*/
		case SAX_STATE_TEXT_CONTENT:
			is_text = 1;
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
				if (!is_text && (c=='\n')) parser->line++;
				if (parser->current_pos+i==parser->line_size) goto exit;
			}
			if (is_text && i) {
				xml_sax_store_text(parser, i);
				is_text = 0;
				parser->sax_state = SAX_STATE_ELEMENT;
			} else if (i) {
				parser->current_pos += i;
				assert(parser->current_pos < parser->line_size);
			}
			is_end = 0;
			i = 0;
			while (1) {
				char c = parser->buffer[parser->current_pos+1+i];
				if (!c) {
					i = 0;
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
				else if (c=='/') {
					is_end = !i ? 1 : 2;
					i++;
				} else {
					i++;
				}
//				if ((c=='[') && (parser->buffer[parser->elt_name_start-1 + i-2]=='A') ) break;
				if (parser->current_pos+1+i==parser->line_size) {
					i=0;
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
					xml_sax_node_end(parser, 0);
				} else {
					parser->elt_end_pos += parser->elt_name_end - parser->elt_name_start;
					xml_sax_node_end(parser, 1);
				}
				if (parser->sax_state == SAX_STATE_SYNTAX_ERROR) break;
				parser->current_pos+=2+i;
				parser->sax_state = SAX_STATE_TEXT_CONTENT;
				break;
			}
			sep = parser->buffer[parser->elt_name_end-1];
			parser->buffer[parser->elt_name_end-1] = 0;
			elt = parser->buffer + parser->elt_name_start-1;

			parser->sax_state = SAX_STATE_ATT_NAME;
			assert(parser->elt_start_pos <= parser->file_pos + parser->current_pos);
			parser->elt_start_pos = parser->file_pos + parser->current_pos;

			if (!strncmp(elt, "!--", 3)) { 
				parser->sax_state = SAX_STATE_COMMENT;
				if (i>3) parser->current_pos -= (i-3);
			}
			else if (!strcmp(elt, "?xml")) parser->init_state = 1;
			else if (!strcmp(elt, "!DOCTYPE")) parser->init_state = 2;
			else if (!strcmp(elt, "!ENTITY")) parser->sax_state = SAX_STATE_ENTITY;
			else if (!strcmp(elt, "!ATTLIST") || !strcmp(elt, "!ELEMENT")) parser->sax_state = SAX_STATE_SKIP_DOCTYPE;
			else if (!strcmp(elt, "![CDATA[")) 
				parser->sax_state = SAX_STATE_CDATA;
			else if (elt[0]=='?') parser->sax_state = SAX_STATE_XML_PROC;
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
						orig_buf = strdup(parser->buffer + parser->current_pos);
						parser->current_pos = 0;
						parser->line_size = 0;
						parser->elt_start_pos = 0;
						parser->sax_state = SAX_STATE_TEXT_CONTENT;
						e = gf_xml_sax_parse_intern(parser, orig_buf);
						free(orig_buf);
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
				return GF_OK;
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
	return GF_OK;
}

static GF_Err xml_sax_append_string(GF_SAXParser *parser, char *string)
{
	u32 size = parser->line_size;
	u32 nl_size = strlen(string);
	
	if ( (parser->alloc_size < size+nl_size+1) 
		|| (parser->alloc_size / 2 ) > size+nl_size+1) 
	{
		parser->buffer = realloc(parser->buffer, sizeof(char) * (size+nl_size+1) );
		if (!parser->buffer ) return GF_OUT_OF_MEM;
		parser->alloc_size = size+nl_size+1;
	}
	memcpy(parser->buffer+size, string, sizeof(char)*nl_size);
	parser->buffer[size+nl_size] = 0;
	parser->line_size = size+nl_size;
	return GF_OK;
}

static GF_Err gf_xml_sax_parse_intern(GF_SAXParser *parser, char *current)
{
	u32 i, count;
	/*solve entities*/
	count = gf_list_count(parser->entities);
	while (count) {
		char *entityEnd, szName[200];
		XML_Entity *ent;
		char *entityStart = strstr(current, "&");

		if (parser->in_entity) {
			entityEnd = strstr(current, ";");
			if (!entityEnd) return xml_sax_append_string(parser, current);
			entityStart = strrchr(parser->buffer, '&');

			strcpy(szName, entityStart+1);
			entityStart[0] = 0;
			entityEnd[0] = 0;
			strcat(szName, current);
			entityEnd[0] = ';';
			parser->in_entity = 0;
			current = entityEnd+1;
		} else {
			if (!entityStart) break;
			entityEnd = strstr(entityStart, ";");

			entityStart[0] = 0;
			xml_sax_append_string(parser, current);
			xml_sax_parse(parser, 1);
			entityStart[0] = '&';

			if (!entityEnd) {
				parser->in_entity = 1;
				/*store entity start*/
				return xml_sax_append_string(parser, entityStart);
			}
			strncpy(szName, entityStart+1, entityEnd - entityStart - 1);
			szName[entityEnd - entityStart - 1] = 0;
			current = entityEnd + 1;
		}

		for (i=0; i<count; i++) {
			ent = (XML_Entity *)gf_list_get(parser->entities, i);
			if (!strcmp(ent->name, szName)) {
				u32 line_num = parser->line;
				xml_sax_append_string(parser, ent->value);
				xml_sax_parse(parser, 1);
				parser->line = line_num;
				break;
			}
		}
	}
	xml_sax_append_string(parser, current);
	return xml_sax_parse(parser, 0);
}

GF_EXPORT
GF_Err gf_xml_sax_parse(GF_SAXParser *parser, void *string)
{
	GF_Err e;
	char *current;
	char *utf_conv = NULL;
	
	if (parser->unicode_type < 0) return GF_BAD_PARAM;

	if (parser->unicode_type>1) {
		const u16 *sptr = (const u16 *)string;
		u32 len = 2*gf_utf8_wcslen(sptr);
		utf_conv = (char *)malloc(sizeof(char)*(len+1));
		len = gf_utf8_wcstombs(utf_conv, len, &sptr);
		if (len==(u32) -1) {
			parser->sax_state = SAX_STATE_SYNTAX_ERROR;
			free(utf_conv);
			return GF_CORRUPTED_DATA;
		}
		utf_conv[len] = 0;
		current = utf_conv;
	} else {
		current = (char *)string;
	}

	e = gf_xml_sax_parse_intern(parser, current);
	if (utf_conv) free(utf_conv);
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

	offset = 0;
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
	parser->sax_state = SAX_STATE_ELEMENT;
	return gf_xml_sax_parse(parser, BOM + offset);
}

static void xml_sax_reset(GF_SAXParser *parser)
{
	while (1) {
		XML_Entity *ent = (XML_Entity *)gf_list_last(parser->entities);
		if (!ent) break;
		gf_list_rem_last(parser->entities);
		if (ent->name) free(ent->name);
		if (ent->value) free(ent->value);
		free(ent);
	}
	if (parser->buffer) free(parser->buffer);
	parser->buffer = NULL;
	parser->current_pos = 0;
	free(parser->attrs);
	parser->attrs = NULL;
	free(parser->sax_attrs);
	parser->sax_attrs = NULL;
	parser->nb_alloc_attrs = parser->nb_attrs = 0;
}

#define XML_INPUT_SIZE	4096

static GF_Err xml_sax_read_file(GF_SAXParser *parser)
{
	GF_Err e = GF_EOS;
	unsigned char szLine[XML_INPUT_SIZE+2];

#ifdef NO_GZIP
	if (!parser->f_in) return GF_BAD_PARAM;
#else
	if (!parser->gz_in) return GF_BAD_PARAM;
#endif

	parser->file_pos = 0;

	while (!parser->suspended) {
#ifdef NO_GZIP
		s32 read = fread(szLine, 1, XML_INPUT_SIZE, parser->f_in);
#else
		s32 read = gzread(parser->gz_in, szLine, XML_INPUT_SIZE);
#endif
		if (read<=0) break;
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
		fclose(parser->f_in);
		parser->f_in = NULL;
#else
		gzclose(parser->gz_in);
		parser->gz_in = 0;
#endif
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

	/*check file exists and gets its size (zlib doesn't support SEEK_END)*/
	test = fopen(fileName, "rb");
	if (!test) return GF_URL_ERROR;
	fseek(test, 0, SEEK_END);
	parser->file_size = ftell(test);
	fclose(test);

	parser->on_progress = OnProgress;

#ifdef NO_GZIP
	parser->f_in = fopen(fileName, "rt");
	fread(szLine, 1, 4, parser->f_in);
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
	if (!parser) return 0;
#ifdef NO_GZIP
	return 0;
#else
	if (!parser->gz_in) return 0;
	return (((z_stream*)parser->gz_in)->data_type==Z_BINARY) ? 1 : 0;
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
	if (parser->f_in) fclose(parser->f_in);
#else
	if (parser->gz_in) gzclose(parser->gz_in);
#endif
	free(parser);
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
		return xml_sax_parse(parser, 0);
	}
	return GF_OK;
}

GF_EXPORT
u32 gf_xml_sax_get_line(GF_SAXParser *parser) { return parser->line + 1 ; }

GF_EXPORT
u32 gf_xml_sax_get_file_size(GF_SAXParser *parser) 
{ 
#ifdef NO_GZIP
	return parser->f_in ? parser->file_size : 0; 
#else
	return parser->gz_in ? parser->file_size : 0; 
#endif
}

GF_EXPORT
u32 gf_xml_sax_get_file_pos(GF_SAXParser *parser) 
{
#ifdef NO_GZIP
	return parser->f_in ? parser->file_pos : 0; 
#else
	return parser->gz_in ? parser->file_pos : 0; 
#endif
}

GF_EXPORT
char *gf_xml_sax_peek_node(GF_SAXParser *parser, char *att_name, char *att_value, char *substitute, char *get_attr, char *end_pattern, Bool *is_substitute)
{
	u32 state, att_len;
	z_off_t pos;
	char szLine1[XML_INPUT_SIZE+2], szLine2[XML_INPUT_SIZE+2], *szLine, *cur_line, *sep, *start, first_c, *result;
#ifdef NO_GZIP
	if (!parser->f_in) return NULL;
#else
	if (!parser->gz_in) return NULL;
#endif

	result = NULL;

	szLine1[0] = szLine2[0] = 0;
#ifdef NO_GZIP
	pos = ftell(parser->f_in);
#else
	pos = gztell(parser->gz_in);
#endif
	att_len = strlen(parser->buffer + parser->att_name_start);
	if (att_len<2*XML_INPUT_SIZE) att_len = 2*XML_INPUT_SIZE;
	szLine = (char *) malloc(sizeof(char)*att_len);
	strcpy(szLine, parser->buffer + parser->att_name_start);
	cur_line = szLine;
	att_len = strlen(att_value);
	state = 0;
	goto retry;

#ifdef NO_GZIP
	while (!feof(parser->f_in)) {
#else
	while (!gzeof(parser->gz_in)) {
#endif
		u32 read;
		if (cur_line == szLine2) {
			cur_line = szLine1;
		} else {
			cur_line = szLine2;
		}
#ifdef NO_GZIP
		read = fread(cur_line, 1, XML_INPUT_SIZE, parser->f_in);
#else
		read = gzread(parser->gz_in, cur_line, XML_INPUT_SIZE);
#endif
		cur_line[read] = cur_line[read+1] = 0;

		strcat(szLine, cur_line);
retry:
		if (state == 2) goto fetch_attr;
		sep = strstr(szLine, att_name);
		if (!sep && !state) {
			state = 0;
			strcpy(szLine, cur_line);
			if (end_pattern && strstr(szLine, end_pattern)) goto exit;
			continue;
		}
		if (!state) {
			state = 1;
			/*load next line*/
			first_c = sep[0];
			sep[0] = 0;
			start = strrchr(szLine, '<');
			if (!start) goto exit;
			sep[0] = first_c;
			strcpy(szLine, start);
			sep = strstr(szLine, att_name);
		}
		sep = strchr(sep, '=');
		if (!sep) {
			state = 0;
			strcpy(szLine, cur_line);
			continue;
		}
		while ( (sep[0] != '\"') && (sep[0] != '\"') ) sep++;
		sep++;

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
				if (is_substitute) *is_substitute = 0;
				result = strdup(sep);
				goto exit;
			}
			sep[pos] = first_c;
fetch_attr:
			sep = strstr(szLine + 1, get_attr);
			if (!sep) {
				strcpy(szLine, cur_line);
				continue;
			}
			sep += strlen(get_attr);
			while (strchr("= \t\r\n", sep[0])) sep++;
			sep++;
			pos = 0;
			while (!strchr(" \t\r\n/>", sep[pos])) pos++;
			sep[pos-1] = 0;
			result = strdup(sep);
			if (is_substitute) *is_substitute = 1;
			goto exit;
		}
		state = 0;
		strcpy(szLine, sep);
		goto retry;
	}
exit:
	free(szLine);
#ifdef NO_GZIP
	fseek(parser->f_in, pos, SEEK_SET);
#else
	gzrewind(parser->gz_in);
	gzseek(parser->gz_in, pos, SEEK_SET);
#endif
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
	pt->res = strdup(name);
	pt->parser->suspended = 1;
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
	GF_XMLNode *root;
	u32 depth;

	void (*OnProgress)(void *cbck, u32 done, u32 tot);
	void *cbk;
};


static void gf_xml_dom_node_del(GF_XMLNode *node)
{
	if (node->attributes) {
		while (gf_list_count(node->attributes)) {
			GF_XMLAttribute *att = (GF_XMLAttribute *)gf_list_last(node->attributes);
			gf_list_rem_last(node->attributes);
			if (att->name) free(att->name);
			if (att->value) free(att->value);
			free(att);
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
	if (node->ns) free(node->ns);
	if (node->name) free(node->name);
	free(node);
}

static void on_dom_node_start(void *cbk, const char *name, const char *ns, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	u32 i;
	GF_DOMParser *par = (GF_DOMParser *) cbk;
	GF_XMLNode *node;

	if (par->root && !gf_list_count(par->stack)) {
		par->parser->suspended = 1;
		return;
	}

	GF_SAFEALLOC(node, GF_XMLNode);
	node->attributes = gf_list_new();
	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att;
		GF_SAFEALLOC(att, GF_XMLAttribute);
		att->name = strdup(attributes[i].name);
		att->value = strdup(attributes[i].value);
		gf_list_add(node->attributes, att);
	}
	node->content = gf_list_new();
	node->name = strdup(name);
	if (ns) node->ns = strdup(ns);
	gf_list_add(par->stack, node);
	if (!par->root) par->root = node;
}
static void on_dom_node_end(void *cbk, const char *name, const char *ns)
{
	GF_DOMParser *par = (GF_DOMParser *)cbk;
	GF_XMLNode *last = (GF_XMLNode *)gf_list_last(par->stack);
	gf_list_rem_last(par->stack);

	if (!last || strcmp(last->name, name) || (!ns && last->ns) || (ns && !last->ns) || (ns && strcmp(last->ns, ns) ) ) {
		par->parser->suspended = 1;
		gf_xml_dom_node_del(last);
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
	node->type = is_cdata ? GF_XML_CDATA_TYPE : GF_XML_TEXT_TYPE;
	node->name = strdup(content);
	gf_list_add(last->content, node);
}

GF_EXPORT
GF_DOMParser *gf_xml_dom_new()
{
	GF_DOMParser *dom;
	GF_SAFEALLOC(dom, GF_DOMParser);
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
			if (dom->root==n) dom->root = NULL;
			gf_xml_dom_node_del(n);
		}
		gf_list_del(dom->stack);	
		dom->stack = NULL;
	}
	if (full_reset && dom->root) {
		gf_xml_dom_node_del(dom->root);
		dom->root = NULL;
	}
}

GF_EXPORT
void gf_xml_dom_del(GF_DOMParser *parser)
{
	gf_xml_dom_reset(parser, 1);
	free(parser);
}

static void dom_on_progress(void *cbck, u32 done, u32 tot)
{
	GF_DOMParser *dom = (GF_DOMParser *)cbck;
	dom->OnProgress(dom->cbk, done, tot);
}

GF_EXPORT
GF_Err gf_xml_dom_parse(GF_DOMParser *dom, const char *file, gf_xml_sax_progress OnProgress, void *cbk)
{
	GF_Err e;
	gf_xml_dom_reset(dom, 1);
	dom->stack = gf_list_new();
	dom->parser = gf_xml_sax_new(on_dom_node_start, on_dom_node_end, on_dom_text_content, dom);
	dom->OnProgress = OnProgress;
	dom->cbk = cbk;
	e = gf_xml_sax_parse_file(dom->parser, file, OnProgress ? dom_on_progress : NULL);
	gf_xml_dom_reset(dom, 0);
	return e<0 ? e : GF_OK;
}

GF_EXPORT
GF_XMLNode *gf_xml_dom_get_root(GF_DOMParser *parser)
{
	return parser->root;
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

