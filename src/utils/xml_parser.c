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


static char *xml_translate_xml_string(char *str)
{
	char *value;
	u32 size, i, j;
	if (!str || !strlen(str)) return NULL;
	value = malloc(sizeof(char) * 500);
	size = 500;
	i = j = 0;
	while (str[i]) {
		if (j >= size) {
			size += 500;
			value = realloc(value, sizeof(char)*size);
		}
		if (str[i] == '&') {
			if (str[i+1]=='#') {
				char szChar[20], *end;
				u16 wchar[2];
				const unsigned short *srcp;
				strncpy(szChar, str+i, 10);
				end = strchr(szChar, ';');
				assert(end);
				end[1] = 0;
				i+=strlen(szChar);
				wchar[1] = 0;
				sscanf(szChar, "&#%hd;", &wchar[0]);
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
	SAX_STATE_ELEMENT,
	SAX_STATE_COMMENT,
	SAX_STATE_ATT_NAME,
	SAX_STATE_ATT_VALUE,
	SAX_STATE_TEXT_CONTENT,
	SAX_STATE_ENTITY,
	SAX_STATE_SKIP_DOCTYPE,
	SAX_STATE_CDATA,
	SAX_STATE_DONE,
	SAX_STATE_XML_PROC,
	SAX_STATE_SYNTAX_ERROR,
};


struct _tag_sax_parser
{
	/*0: UTF-8, 1: UTF-16 BE, 2: UTF-16 LE. String input is always converted back to utf8*/
	s32 unicode_type;
	char *buffer, *orig_buffer;
	/*name buffer for elements and attributes*/
	char node_name[1000];

	/*dynamic buffer for attribute values*/
	char *value_buffer;
	/*line size and current position*/
	u32 line_size, current_pos;

	/*gz input file*/
	gzFile gz_in;
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
	GF_List *attributes;
	GF_List *nodes;
	GF_List *entities;
	char att_sep;
	Bool in_entity, suspended;
	u32 in_quote;

	u32 elt_start_pos, elt_end_pos;

	/*last error found*/
	char err_msg[1000];
};

static void xml_sax_swap(GF_SAXParser *parser)
{
	if (parser->current_pos) {
		char *new_buf = strdup(parser->buffer + parser->current_pos);
		free(parser->orig_buffer);
		parser->orig_buffer = parser->buffer = new_buf;
		parser->line_size = strlen(new_buf);
		parser->file_pos += parser->current_pos;
		parser->current_pos = 0;
	}
}

static void xml_sax_node_end(GF_SAXParser *parser, Bool had_children)
{
	char *name, *sep;
	name = gf_list_last(parser->nodes);
	assert(name);

	if (had_children) {
		if (strcmp(parser->node_name+1, name)) {
			parser->sax_state = SAX_STATE_SYNTAX_ERROR;
			sprintf(parser->err_msg, "Closing element \"%s\" doesn't match opening element \"%s\"", parser->node_name+1, name);
			return;
		}
	}
	gf_list_rem_last(parser->nodes);
	if (parser->sax_node_end) {
		sep = strchr(name, ':');
		if (sep) {
			char szName[1024], szNS[1024];
			strcpy(szName, sep+1);
			sep[0] = 0;
			strcpy(szNS, name);
			sep[0] = ':';
			parser->sax_node_end(parser->sax_cbck, szName, szNS);
		} else {
			parser->sax_node_end(parser->sax_cbck, name, NULL);
		}
	}
	free(name);
	if (!parser->init_state && !gf_list_count(parser->nodes)) parser->sax_state = SAX_STATE_DONE;
}

static void xml_sax_reset_attributes(GF_SAXParser *parser)
{
	/*destroy attributes*/
	while (1) {
		GF_XMLAttribute * att = gf_list_last(parser->attributes);
		if (!att) break;
		gf_list_rem_last(parser->attributes);
		if (att->name) free(att->name);
		if (att->value) free(att->value);
		free(att);
	}
}

static void xml_sax_node_start(GF_SAXParser *parser)
{
	char *sep;
	char *name = gf_list_last(parser->nodes);
		
	//fprintf(stdout, "parsing node %s\n", name);
	if (parser->sax_node_start) {
		sep = strchr(name, ':');
		if (sep) {
			char szName[1024], szNS[1024];
			strcpy(szName, sep+1);
			sep[0] = 0;
			strcpy(szNS, name);
			sep[0] = ':';
			parser->sax_node_start(parser->sax_cbck, szName, szNS, parser->attributes);
		} else {
			parser->sax_node_start(parser->sax_cbck, name, NULL, parser->attributes);
		}
	}
	xml_sax_reset_attributes(parser);
}

#define XML_ATT_SIZE 8000

static Bool xml_sax_parse_attribute(GF_SAXParser *parser)
{
	char szVal[XML_ATT_SIZE], *skip_chars;
	u32 i;
	GF_XMLAttribute *att = NULL;

	if (parser->sax_state==SAX_STATE_ATT_NAME) {
		skip_chars = " \t\n\r";
	} else {
		skip_chars = "\t\n\r";
		if (parser->att_sep) att = gf_list_last(parser->attributes);
	}
	i=0;
	while (parser->current_pos+i < parser->line_size) {
		u8 c = parser->buffer[parser->current_pos+i];
		if (strchr(skip_chars, c)) {
			if (c=='\n') parser->line++;
			if ((parser->sax_state==SAX_STATE_ATT_NAME) || !parser->att_sep) {
				parser->current_pos++;
				continue;
			} else {
				c=' ';
			}
		}
		if (parser->sax_state==SAX_STATE_ATT_NAME) {
			if ( (c=='/') || ((parser->init_state==1) && (c=='?')) ) {
				if (parser->current_pos+i+1 == parser->line_size) return 1;

				if (parser->buffer[parser->current_pos+i+1]=='>') {
					parser->current_pos+=i+2;
					parser->elt_end_pos = parser->file_pos + parser->current_pos;
					parser->sax_state = (parser->init_state) ? SAX_STATE_ELEMENT : SAX_STATE_TEXT_CONTENT;
					/*done parsing attr AND elements*/
					if (!parser->init_state) {
						xml_sax_node_start(parser);
						xml_sax_node_end(parser, 0);
					} else {
						xml_sax_reset_attributes(parser);
					}
					return 0;
				}
				if (!parser->in_quote && (c=='/')) {
					if (!parser->init_state) {
						parser->sax_state = SAX_STATE_SYNTAX_ERROR;
						sprintf(parser->err_msg, "Markup error");
						return 1;
					}
				}
			}
			else if ((parser->sax_state!=SAX_STATE_ATT_VALUE) && (c=='"')) {
				if (parser->in_quote && (parser->in_quote!=c) ) {
					parser->sax_state = SAX_STATE_SYNTAX_ERROR;
					sprintf(parser->err_msg, "Markup error");
					return 1;
				}
				if (parser->in_quote) parser->in_quote = 0;
				else parser->in_quote = c;
			}
			else if (c=='>') {
				parser->current_pos+=i+1;
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
				xml_sax_node_start(parser);
				parser->sax_state = SAX_STATE_TEXT_CONTENT;
				return 0;
			}
			else if (parser->init_state && (c=='[')) {
				parser->current_pos+=i+1;
				if (parser->init_state==1) {
					parser->sax_state = SAX_STATE_SYNTAX_ERROR;
					sprintf(parser->err_msg, "Invalid DOCTYPE");
					return 1;
				}
				parser->sax_state = SAX_STATE_ELEMENT;
				return 0;
			}

			if (c=='=') {
				parser->sax_state = SAX_STATE_ATT_VALUE;
				szVal[i] = 0;
				GF_SAFEALLOC(att, sizeof(GF_XMLAttribute));
				att->name = strdup(szVal);
				gf_list_add(parser->attributes, att);
				parser->current_pos+=i+1;
				i=0;
				att = NULL;
				parser->att_sep = 0;
			} else {
				szVal[i] = c;
				i++;
			}
		} else if (parser->sax_state==SAX_STATE_ATT_VALUE) {
			if ( (!parser->att_sep && ((c=='\"') || (c=='\'')) ) || (c==parser->att_sep) ) {
				if (!parser->att_sep) {
					parser->att_sep = c;
					att = gf_list_last(parser->attributes);
					if (!att || att->value) {
						parser->sax_state = SAX_STATE_SYNTAX_ERROR;
						sprintf(parser->err_msg, "Missing attribute name");
						return 1;
					}
					skip_chars = "\t\n\r";
					parser->current_pos++;
					i=0;
				} else {
					assert(att);

					szVal[i] = 0;
					if (!att->value) {
						att->value = strdup(szVal);
					} else {
						u32 len = strlen(att->value);
						att->value = realloc(att->value, sizeof(char)* (i+len+1));
						memcpy(att->value + len, szVal, i);
						att->value[len+i] = 0;
					}
					parser->current_pos+=i+1;
					xml_sax_swap(parser);

					/*"style" always at the begining of the attributes*/
					if (!strcmp(att->name, "style")) {
						gf_list_rem_last(parser->attributes);
						gf_list_insert(parser->attributes, att, 0);
					}

					parser->sax_state = SAX_STATE_ATT_NAME;
					/*solve XML built-in entities*/
					if (strchr(att->value, '&') && strchr(att->value, ';')) {
						char *xml_text = xml_translate_xml_string(att->value);
						if (xml_text) {
							free(att->value);
							att->value = xml_text;
						}
					}
					return 0;
				}
			} else if (parser->att_sep) {
				//if (!att) att = gf_list_last(parser->attributes);
				assert(att);
				if (!att) {
					parser->sax_state = SAX_STATE_SYNTAX_ERROR;
					sprintf(parser->err_msg, "Missing attribute");
					return 1;
				}
				szVal[i] = c;
				i++;
			} else {
				parser->current_pos++;
			}
		} else {
			i++;
		}
	}
	if (att && i) {
		u32 len = att->value ? strlen(att->value) : 0;
		att->value = realloc(att->value, sizeof(char)* (i+len+1));
		memcpy(att->value + len, szVal, i);
		att->value[len+i] = 0;
		parser->current_pos += i;
	}
	return 1;
}
typedef struct
{
	char *name;
	char *value;
	u8 sep;
} XML_Entity;

static void xml_sax_store_text(GF_SAXParser *parser, u32 txt_len)
{
	u32 len;
	char *txt;
	if (!txt_len) return;

	txt = parser->buffer + parser->current_pos;
	len = 0;
	if (parser->value_buffer) len = strlen(parser->value_buffer);
	parser->value_buffer = realloc(parser->value_buffer, sizeof(char)*(len+txt_len+1) );
	strncpy(parser->value_buffer+len, txt, txt_len);
	parser->value_buffer[txt_len+len] = 0;
	parser->current_pos += txt_len;
	txt = parser->value_buffer+len;
	while (txt) {
		txt = strchr(txt, '\n');
		if (!txt) break;
		parser->line++;
		txt += 1;
	}
}

static void xml_sax_flush_text(GF_SAXParser *parser)
{
	if (parser->value_buffer) {
		if (!parser->init_state && parser->sax_text_content) {
			/*solve XML built-in entities*/
			if (strchr(parser->value_buffer, '&') && strchr(parser->value_buffer, ';')) {
				char *xml_text = xml_translate_xml_string(parser->value_buffer);
				if (xml_text) {
					parser->sax_text_content(parser->sax_cbck, xml_text, (parser->sax_state==SAX_STATE_CDATA) ? 1 : 0);
					free(xml_text);
				}
			} else {
				parser->sax_text_content(parser->sax_cbck, parser->value_buffer, (parser->sax_state==SAX_STATE_CDATA) ? 1 : 0);
			}
		}
		free(parser->value_buffer);
		parser->value_buffer = NULL;
	}
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
	XML_Entity *ent = gf_list_last(parser->entities);
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
			GF_SAFEALLOC(ent, sizeof(XML_Entity));
			ent->name = strdup(szName);
			ent->sep = c;
			parser->current_pos += 1+i;
			xml_sax_swap(parser);
			i=0;
			gf_list_add(parser->entities, ent);
			skip_chars = NULL;
		} else if (ent && c==ent->sep) {
			xml_sax_store_text(parser, i);
			if (parser->value_buffer) {
				ent->value = parser->value_buffer;
				parser->value_buffer = NULL;
			} else {
				ent->value = strdup("");
			}
			parser->current_pos += 1;
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
		parser->sax_state = SAX_STATE_TEXT_CONTENT;
	}
}

static Bool xml_sax_parse_comments(GF_SAXParser *parser)
{
	char *end = strstr(parser->buffer + parser->current_pos, "-->");
	if (!end) return 0;

	parser->current_pos += 3 + (u32) (end - (parser->buffer + parser->current_pos) );
	parser->sax_state = SAX_STATE_TEXT_CONTENT;
	return 1;
}



static GF_Err xml_sax_parse(GF_SAXParser *parser, Bool force_parse)
{
	u32 i = 0;
	Bool is_text, is_end;
	u8 c;
	char *elt;

	parser->line_size = strlen(parser->buffer);

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
			} else if (i) {
				parser->current_pos += i;
			}
			is_end = 0;
			i = 0;
			while (1) {
				char c = parser->buffer[parser->current_pos+1+i];
				if (!c) {
					i = 0;
					goto exit;
				}
				if (strchr(" \n\t\r>=", c)) {
					if (c=='\n') parser->line++;
					break;
				}
				else if (c=='/') is_end = !i ? 1 : 2;
				parser->node_name[i] = c;
				i++;
				if ((c=='[') && (parser->node_name[i-2]=='A') ) 
					break;
				if (parser->current_pos+1+i==parser->line_size) {
					i=0;
					goto exit;
				}
			}
			parser->node_name[i] = 0;
			if (is_end) {
				xml_sax_flush_text(parser);
				parser->elt_end_pos = parser->file_pos + parser->current_pos;
				if (is_end==2) {
					parser->node_name[i-1] = 0;
					gf_list_add(parser->nodes, strdup(parser->node_name));
					xml_sax_node_start(parser);
					xml_sax_node_end(parser, 0);
				} else {
					xml_sax_node_end(parser, 1);
				}
				if (parser->sax_state == SAX_STATE_SYNTAX_ERROR) break;
				parser->current_pos+=2+i;
				parser->sax_state = SAX_STATE_TEXT_CONTENT;
				break;
			}
			parser->sax_state = SAX_STATE_ATT_NAME;
			parser->elt_start_pos = parser->file_pos + parser->current_pos;
			parser->current_pos+=1+i;
			if (!strncmp(parser->node_name, "!--", 3)) parser->sax_state = SAX_STATE_COMMENT;
			else if (!strcmp(parser->node_name, "?xml")) parser->init_state = 1;
			else if (!strcmp(parser->node_name, "!DOCTYPE")) 
				parser->init_state = 2;
			else if (!strcmp(parser->node_name, "!ENTITY")) parser->sax_state = SAX_STATE_ENTITY;
			else if (!strcmp(parser->node_name, "![CDATA[")) parser->sax_state = SAX_STATE_CDATA;
			else if (parser->node_name[0]=='?') parser->sax_state = SAX_STATE_XML_PROC;
			/*node found*/
			else {
				xml_sax_flush_text(parser);
				parser->init_state = 0;
				gf_list_add(parser->nodes, strdup(parser->node_name));
			}
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
	if (is_text) {
		if (i) xml_sax_store_text(parser, i);
		/*DON'T FLUSH TEXT YET, wait for next '<' to do so otherwise we may corrupt xml base entities (&apos;, ...)*/
	}
	xml_sax_swap(parser);
	return GF_OK;
}

static GF_Err xml_sax_append_string(GF_SAXParser *parser, char *string)
{
	if (!parser->orig_buffer) {
		parser->orig_buffer = strdup(string);
		if (!parser->orig_buffer) return GF_OUT_OF_MEM;
		parser->buffer = parser->orig_buffer;
		parser->current_pos = 0;
	} else {
		u32 l1 = strlen(parser->orig_buffer);
		u32 l2 = strlen(string);
		char *buf = malloc(sizeof(char)* (l1+l2+1) );
		if (!buf) return GF_OUT_OF_MEM;
		memcpy(buf, parser->orig_buffer, l1);
		memcpy(buf + l1, string, l2);
		buf[l1+l2] = 0;
		free(parser->orig_buffer);
		parser->orig_buffer = buf;
		parser->buffer = parser->orig_buffer + parser->current_pos;
	}
	return GF_OK;
}

GF_Err gf_xml_sax_parse(GF_SAXParser *parser, void *string)
{
	char *current;
	char *utf_conv = NULL;
	u32 i, count;
	
	if (parser->unicode_type < 0) return GF_BAD_PARAM;

	count = gf_list_count(parser->entities);
	if (parser->unicode_type>1) {
		const u16 *sptr = string;
		u32 len = 2*gf_utf8_wcslen(sptr);
		utf_conv = malloc(sizeof(char)*(len+1));
		len = gf_utf8_wcstombs(utf_conv, len, &sptr);
		if (len==(u32) -1) {
			parser->sax_state = SAX_STATE_SYNTAX_ERROR;
			free(utf_conv);
			return GF_CORRUPTED_DATA;
		}
		utf_conv[len] = 0;
		current = utf_conv;
	} else {
		current = string;
	}


	/*solve entities*/
	while (count) {
		char *entityEnd, szName[200];
		XML_Entity *ent;
		char *entityStart = strstr(current, "&");

		if (parser->in_entity) {
			entityEnd = strstr(current, ";");
			if (!entityEnd) return xml_sax_append_string(parser, string);
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
			ent = gf_list_get(parser->entities, i);
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
	if (utf_conv) free(utf_conv);
	return xml_sax_parse(parser, 0);
}


GF_Err gf_xml_sax_init(GF_SAXParser *parser, unsigned char *BOM)
{
	u32 offset;
	if (!BOM) parser->unicode_type = 0;
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
		GF_XMLAttribute * att = gf_list_last(parser->attributes);
		if (!att) break;
		gf_list_rem_last(parser->attributes);
		if (att->name) free(att->name);
		if (att->value) free(att->value);
		free(att);
	}
	while (1) {
		char *name = gf_list_last(parser->nodes);
		if (!name) break;
		gf_list_rem_last(parser->nodes);
		free(name);
	}
	while (1) {
		XML_Entity *ent = gf_list_last(parser->entities);
		if (!ent) break;
		gf_list_rem_last(parser->entities);
		if (ent->name) free(ent->name);
		if (ent->value) free(ent->value);
		free(ent);
	}
	if (parser->value_buffer) free(parser->value_buffer);
	parser->value_buffer = NULL;
	if (parser->orig_buffer) free(parser->orig_buffer);
	parser->orig_buffer = parser->buffer = NULL;
	parser->current_pos = 0;
}

static GF_Err xml_sax_read_file(GF_SAXParser *parser)
{
	GF_Err e = GF_EOS;
	unsigned char szLine[1026];
	if (!parser->gz_in) return GF_BAD_PARAM;

	while (!gzeof(parser->gz_in) && !parser->suspended) {
		u32 read = gzread(parser->gz_in, szLine, 1024);
		szLine[read] = 0;
		szLine[read+1] = 0;
		e = gf_xml_sax_parse(parser, szLine);
		if (e) break;
//		parser->file_pos = gztell(parser->gz_in);
		if (parser->file_pos > parser->file_size) parser->file_size = parser->file_pos + 1;
		if (parser->on_progress) parser->on_progress(parser->sax_cbck, parser->file_pos, parser->file_size);
	}
	if (gzeof(parser->gz_in)) {
		if (!e) e = GF_EOS;
		if (parser->on_progress) parser->on_progress(parser->sax_cbck, parser->file_size, parser->file_size);
	}
	return e;
}

GF_Err gf_xml_sax_parse_file(GF_SAXParser *parser, const char *fileName, gf_xml_sax_progress OnProgress)
{
	FILE *test;
	GF_Err e;
	gzFile gzInput;
	unsigned char szLine[1024];

	/*check file exists and gets its size (zlib doesn't support SEEK_END)*/
	test = fopen(fileName, "rb");
	if (!test) return GF_URL_ERROR;
	fseek(test, 0, SEEK_END);
	parser->file_size = ftell(test);
	fclose(test);

	parser->on_progress = OnProgress;

	gzInput = gzopen(fileName, "rb");
	if (!gzInput) return GF_IO_ERR;
	parser->gz_in = gzInput;
	/*init SAX parser (unicode setup)*/
	gzread(gzInput, szLine, 4);
	szLine[4] = szLine[5] = 0;
	e = gf_xml_sax_init(parser, szLine);
	if (e) return e;
	return xml_sax_read_file(parser);
}

Bool gf_xml_sax_binary_file(GF_SAXParser *parser)
{
	if (!parser || !parser->gz_in) return 0;
	return (((z_stream*)parser->gz_in)->data_type==Z_BINARY) ? 1 : 0;
}

GF_SAXParser *gf_xml_sax_new(gf_xml_sax_node_start on_node_start, 
							 gf_xml_sax_node_end on_node_end,
							 gf_xml_sax_text_content on_text_content,
							 void *cbck)
{
	GF_SAXParser *parser;
	GF_SAFEALLOC(parser, sizeof(GF_SAXParser));

	parser->attributes = gf_list_new();
	parser->nodes = gf_list_new();
	parser->entities = gf_list_new();
	parser->unicode_type = -1;
	parser->sax_node_start = on_node_start;
	parser->sax_node_end = on_node_end;
	parser->sax_text_content = on_text_content;
	parser->sax_cbck = cbck;
	return parser;
}

void gf_xml_sax_del(GF_SAXParser *parser)
{
	xml_sax_reset(parser);
	gf_list_del(parser->attributes);
	gf_list_del(parser->nodes);
	gf_list_del(parser->entities);
	if (parser->gz_in) gzclose(parser->gz_in);
	free(parser);
}

GF_Err gf_xml_sax_suspend(GF_SAXParser *parser, Bool do_suspend)
{
	parser->suspended = do_suspend;
	if (!do_suspend) {
		if (parser->gz_in) return xml_sax_read_file(parser);
		return xml_sax_parse(parser, 0);
	}
	return GF_OK;
}

u32 gf_xml_sax_get_line(GF_SAXParser *parser) { return parser->line + 1 ; }
u32 gf_xml_sax_get_file_size(GF_SAXParser *parser) { return parser->gz_in ? parser->file_size : 0; }
u32 gf_xml_sax_get_file_pos(GF_SAXParser *parser) { return parser->gz_in ? parser->file_pos : 0; }

char *gf_xml_sax_peek_node(GF_SAXParser *parser, char *att_name, char *att_value, char *substitute, char *get_attr, char *end_pattern, Bool *is_substitute)
{
	u32 state, att_len;
	z_off_t pos;
	char szLine1[1024], szLine2[1024], *szLine, *cur_line, *sep, *start, first_c, *result;
	if (!parser->gz_in) return NULL;

	result = NULL;

	szLine1[0] = szLine2[0] = 0;
	pos = gztell(parser->gz_in);
	att_len = strlen(parser->buffer);
	if (att_len<2048) att_len = 2048;
	GF_SAFEALLOC(szLine, sizeof(char)*att_len);
	strcpy(szLine, parser->buffer);
	cur_line = szLine;
	att_len = strlen(att_value);
	state = 0;
	goto retry;

	while (!gzeof(parser->gz_in)) {
		u32 read;
		if (cur_line == szLine2) {
			cur_line = szLine1;
		} else {
			cur_line = szLine2;
		}
		read = gzread(parser->gz_in, cur_line, 1023);
		cur_line[read] = 0;

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
	gzrewind(parser->gz_in);
	gzseek(parser->gz_in, pos, SEEK_SET);
	return result;
}

const char *gf_xml_sax_get_error(GF_SAXParser *parser)
{
	return parser->err_msg;
}


struct _peek_type
{
	GF_SAXParser *parser;
	char *res;
};

static void on_peek_node_start(void *cbk, const char *name, const char *ns, GF_List *atts)
{
	struct _peek_type *pt = (struct _peek_type*)cbk;
	pt->res = strdup(name);
	pt->parser->suspended = 1;
}

char *gf_xml_get_root_type(const char *file)
{
	struct _peek_type pt;
	pt.res = NULL;
	pt.parser = gf_xml_sax_new(on_peek_node_start, NULL, NULL, &pt);
	gf_xml_sax_parse_file(pt.parser, file, NULL);
	gf_xml_sax_del(pt.parser);
	return pt.res;
}


u32 gf_xml_sax_get_node_start_pos(GF_SAXParser *parser)
{
	return parser->elt_start_pos;
}
u32 gf_xml_sax_get_node_end_pos(GF_SAXParser *parser)
{
	return parser->elt_end_pos;
}

struct _tag_dom_parser
{
	GF_SAXParser *parser;
	GF_List *stack;
	GF_XMLNode *root;

	void (*OnProgress)(void *cbck, u32 done, u32 tot);
	void *cbk;
};


static void gf_xml_dom_node_del(GF_XMLNode *node)
{
	if (node->attributes) {
		while (gf_list_count(node->attributes)) {
			GF_XMLAttribute *att = gf_list_last(node->attributes);
			gf_list_rem_last(node->attributes);
			if (att->name) free(att->name);
			if (att->value) free(att->value);
			free(att);
		}
		gf_list_del(node->attributes);
	}
	if (node->content) {
		while (gf_list_count(node->content)) {
			GF_XMLNode *child = gf_list_last(node->content);
			gf_list_rem_last(node->content);
			gf_xml_dom_node_del(child);
		}
		gf_list_del(node->content);
	}
	if (node->ns) free(node->ns);
	if (node->name) free(node->name);
	free(node);
}

static void on_dom_node_start(void *cbk, const char *name, const char *ns, GF_List *atts)
{
	GF_DOMParser *par = cbk;
	GF_XMLNode *node;


	if (par->root && !gf_list_count(par->stack)) {
		par->parser->suspended = 1;
		return;
	}

	GF_SAFEALLOC(node, sizeof(GF_XMLNode));
	node->attributes = gf_list_new();
	while (gf_list_count(atts)) {
		GF_XMLAttribute *att = gf_list_last(atts);
		gf_list_rem_last(atts);
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
	GF_DOMParser *par = cbk;
	GF_XMLNode *last = gf_list_last(par->stack);
	gf_list_rem_last(par->stack);

	if (!last || strcmp(last->name, name) || (!ns && last->ns) || (ns && !last->ns) || (ns && strcmp(last->ns, ns) ) ) {
		par->parser->suspended = 1;
		gf_xml_dom_node_del(last);
		return;
	}
	if (last != par->root) {
		GF_XMLNode *node = gf_list_last(par->stack);
		assert(node->content);
		assert(gf_list_find(node->content, last) == -1);
		gf_list_add(node->content, last);
	}
}

static void on_dom_text_content(void *cbk, const char *content, Bool is_cdata)
{
	GF_DOMParser *par = cbk;
	GF_XMLNode *node;
	GF_XMLNode *last = gf_list_last(par->stack);
	if (!last) return;
	assert(last->content);

	GF_SAFEALLOC(node, sizeof(GF_XMLNode));
	node->type = is_cdata ? GF_XML_CDATA_TYPE : GF_XML_TEXT_TYPE;
	node->name = strdup(content);
	gf_list_add(last->content, node);
}

GF_DOMParser *gf_xml_dom_new()
{
	GF_DOMParser *dom;
	GF_SAFEALLOC(dom, sizeof(GF_DOMParser));
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
			GF_XMLNode *n = gf_list_last(dom->stack);
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

void gf_xml_dom_del(GF_DOMParser *parser)
{
	gf_xml_dom_reset(parser, 1);
	free(parser);
}

static void dom_on_progress(void *cbck, u32 done, u32 tot)
{
	GF_DOMParser *dom = cbck;
	dom->OnProgress(dom->cbk, done, tot);
}

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

GF_XMLNode *gf_xml_dom_get_root(GF_DOMParser *parser)
{
	return parser->root;
}
const char *gf_xml_dom_get_error(GF_DOMParser *parser)
{
	return gf_xml_sax_get_error(parser->parser);
}
u32 gf_xml_dom_get_line(GF_DOMParser *parser)
{
	return gf_xml_sax_get_line(parser->parser);
}

