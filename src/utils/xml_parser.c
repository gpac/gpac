/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
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

/*
		WARNING: this is a brute XML parser and has nothing generic ...
*/


#include <gpac/xml.h>
#include <gpac/utf.h>


GF_Err xml_init_parser(XMLParser *parser, const char *fileName)
{
	char *str;
	FILE *test;
	gzFile gzInput;
	unsigned char BOM[5];

	memset(parser, 0, sizeof(XMLParser));

	/*check file exists and gets its size (zlib doesn't support SEEK_END)*/
	test = fopen(fileName, "rb");
	if (!test) return GF_URL_ERROR;
	fseek(test, 0, SEEK_END);
	parser->file_size = ftell(test);
	fclose(test);

	gzInput = gzopen(fileName, "rb");
	if (!gzInput) return GF_IO_ERR;
	gzgets(gzInput, BOM, 5);
	gzseek(gzInput, 0, SEEK_SET);

	if ((BOM[0]==0xFF) && (BOM[1]==0xFE)) {
		if (!BOM[2] && !BOM[3]) {
			gzclose(gzInput);
			return GF_NOT_SUPPORTED;
		} else {
			parser->unicode_type = 2;
			gzseek(gzInput, 2, SEEK_CUR);
		}
	} else if ((BOM[0]==0xFE) && (BOM[1]==0xFF)) {
		if (!BOM[2] && !BOM[3]) {
			gzclose(gzInput);
			return GF_NOT_SUPPORTED;
		} else {
			parser->unicode_type = 1;
			gzseek(gzInput, 2, SEEK_CUR);
		}
	} else if ((BOM[0]==0xEF) && (BOM[1]==0xBB) && (BOM[2]==0xBF)) {
		/*we handle UTF8 as asci*/
		parser->unicode_type = 0;
		gzseek(gzInput, 3, SEEK_CUR);
	}
	parser->gz_in = gzInput;

	/*check XML doc*/
	str = xml_get_element(parser);
	if (!str || strcmp(str, "?xml")) {
		gzclose(gzInput);
		return GF_NOT_SUPPORTED;
	}
	xml_skip_attributes(parser);
	return GF_OK;
}

void xml_reset_parser(XMLParser *parser)
{
	if (parser->gz_in) gzclose(parser->gz_in);
	if (parser->value_buffer) free(parser->value_buffer);
	memset(parser, 0, sizeof(XMLParser));
}

void xml_check_line(XMLParser *parser)
{
	u32 i;
	
	if (!parser->text_parsing) {
		while (
			(parser->line_buffer[parser->current_pos]==' ') 
			|| (parser->line_buffer[parser->current_pos]=='\t') 
			) parser->current_pos++;
	}

	/*skip comment*/
	if ((parser->line_buffer[parser->current_pos] =='<')
		&& (parser->line_buffer[parser->current_pos + 1] =='!')
		&& (parser->line_buffer[parser->current_pos + 2] =='-')
		&& (parser->line_buffer[parser->current_pos + 3] =='-')) {
		char *end = strstr(parser->line_buffer, "-->");
		/*comments may be on several lines*/
		while (!end) {
			parser->current_pos = parser->line_size;
			xml_check_line(parser);
			end = strstr(parser->line_buffer, "-->");
		}
		while ( (parser->line_buffer[parser->current_pos] != '-') 
			|| (parser->line_buffer[parser->current_pos+1] != '-')
			|| (parser->line_buffer[parser->current_pos+2] != '>') )
			parser->current_pos++;
		parser->current_pos += 3;

		xml_check_line(parser);
		return;
	}



	if (parser->line_size == parser->current_pos) {
		/*string based input - done*/
		if (!parser->gz_in) return;

next_line:
		parser->line_buffer[0] = 0;
		i=0;
		parser->line_start_pos = gztell(parser->gz_in);

		if (parser->unicode_type) {
			u8 c1, c2;
			unsigned short wchar;
			unsigned short l[XML_LINE_SIZE];
			unsigned short *dst = l;
			Bool is_ret = 0;
			u32 go = XML_LINE_SIZE - 1;
			u32 last_space_pos, last_space_pos_stream;
			last_space_pos = last_space_pos_stream = 0;
			while (go && !gzeof(parser->gz_in) ) {
				c1 = gzgetc(parser->gz_in);
				c2 = gzgetc(parser->gz_in);
				/*Little-endian order*/
				if (parser->unicode_type==2) {
					if (c2) { wchar = c2; wchar <<=8; wchar |= c1; }
					else wchar = c1;
				} else {
					wchar = c1;
					if (c2) { wchar <<= 8; wchar |= c2;}
				}
				*dst = wchar;
				if (wchar=='\r') is_ret = 1;
				else if (wchar=='\n') {
					dst++;
					break;
				}
				else if (is_ret && wchar!='\n') {
					u32 fpos = gztell(parser->gz_in);
					gzseek(parser->gz_in, fpos-2, SEEK_SET);
					is_ret = 1;
					break;
				}
				if (wchar==' ') {
					last_space_pos_stream = gztell(parser->gz_in);
					last_space_pos = dst - l;
				}

				dst++;
				go--;
			}
			*dst = 0;

			if (!go) {
				u32 rew_pos = gztell(parser->gz_in) - 2*(dst - &l[last_space_pos]);
				gzseek(parser->gz_in, rew_pos, SEEK_SET);
				l[last_space_pos+1] = 0;
			}

			/*check eof*/
			if (l[0]==0xFFFF) {
				parser->done = 1;
				return;
			}
			/*convert to mbc string*/
			dst = l;
			gf_utf8_wcstombs(parser->line_buffer, XML_LINE_SIZE, (const unsigned short **) &dst);

			if (!strlen(parser->line_buffer) && gzeof(parser->gz_in)) {
				parser->done = 1;
				return;
			}
		} else {
			if ((gzgets(parser->gz_in, parser->line_buffer, XML_LINE_SIZE) == NULL) 
				|| (!strlen(parser->line_buffer) && gzeof(parser->gz_in))) {
				parser->done = 1;
				return;
			}
			/*watchout for long lines*/
			if (1 + strlen(parser->line_buffer) == XML_LINE_SIZE) {
				u32 rew, pos;
				rew = 0;
				while (parser->line_buffer[strlen(parser->line_buffer)-1] != ' ') {
					parser->line_buffer[strlen(parser->line_buffer)-1] = 0;
					rew++;
				}
				pos = gztell(parser->gz_in);
				gzseek(parser->gz_in, pos-rew, SEEK_SET);
			}
		}

		if (!parser->text_parsing) {
			while (
				(parser->line_buffer[strlen(parser->line_buffer)-1]=='\n')
				|| (parser->line_buffer[strlen(parser->line_buffer)-1]=='\r')
				|| (parser->line_buffer[strlen(parser->line_buffer)-1]=='\t')
				)
				parser->line_buffer[strlen(parser->line_buffer)-1] = 0;
		}

		parser->line_size = strlen(parser->line_buffer);
		parser->current_pos = 0;
		parser->line++;

		/*file size and pos for user notif*/
		i = gztell(parser->gz_in);
		if (i>parser->file_pos) {
			parser->file_pos = i;
			if (parser->OnProgress) parser->OnProgress(parser->cbk, parser->file_pos, parser->file_size);
		}
		
		if (!parser->text_parsing) {
			while ((parser->line_buffer[parser->current_pos]==' ') || (parser->line_buffer[parser->current_pos]=='\t'))
				parser->current_pos++;
		}

		if (parser->current_pos == parser->line_size) goto next_line;
		/*comment*/
		if (!strnicmp(parser->line_buffer + parser->current_pos, "<!--", 4)) xml_check_line(parser);
	}
	if (!parser->line_size) {
		if (!gzeof(parser->gz_in)) xml_check_line(parser);
		else parser->done = 1;
	}
	else if (!parser->done && (parser->line_size == parser->current_pos)) xml_check_line(parser);
}

char *xml_get_element(XMLParser *parser)
{
	s32 i;
	xml_check_line(parser);

	if (!parser->text_parsing) {
		while ((parser->current_pos<parser->line_size) && 
			( (parser->line_buffer[parser->current_pos]=='\n') 
			|| (parser->line_buffer[parser->current_pos]=='\r')
			|| (parser->line_buffer[parser->current_pos]=='\t')
			)  ) {
				parser->current_pos++;
		}
	}

	if (parser->line_buffer[parser->current_pos] !='<') {
		/*not really clean...*/
		if (!strnicmp(parser->line_buffer + parser->current_pos, "NULL", 4)) {
			parser->current_pos += 4;
			return "NULL";
		}
		return NULL;
	}
	if (parser->line_buffer[parser->current_pos + 1] =='/') return NULL;

	parser->current_pos++;
	xml_check_line(parser);

	i=0;
	while (1) {
		if (!parser->line_buffer[parser->current_pos + i]) break;
		else if (parser->line_buffer[parser->current_pos + i] == '>') break;
		else if (parser->line_buffer[parser->current_pos + i] == ' ') break;
		else if ((parser->line_buffer[parser->current_pos + i] == '/') && (parser->line_buffer[parser->current_pos + i + 1] == '>')) break;
		else if (parser->line_buffer[parser->current_pos + i] == '\t') break;
		parser->name_buffer[i] = parser->line_buffer[parser->current_pos + i];
		i++;
		if (parser->current_pos+i==parser->line_size) break;
	}
	parser->name_buffer[i] = 0;
	parser->current_pos += i;
	return parser->name_buffer;
}

void xml_skip_attributes(XMLParser *parser)
{
	s32 i = 0;
	xml_check_line(parser);
	
	if ((parser->line_buffer[parser->current_pos]=='<') && (parser->line_buffer[parser->current_pos + 1]!='/')) 
		return;

	while (1) {
		if (!parser->line_buffer[parser->current_pos + i]) break;
		else if ( (parser->line_buffer[parser->current_pos + i] == '/')  && (parser->line_buffer[parser->current_pos + i + 1] == '>') ) break;
		else if (parser->line_buffer[parser->current_pos + i] == '>') {
			i++;
			break;
		}
		i++;
		if (parser->current_pos+i==parser->line_size) {
			parser->current_pos = parser->line_size;
			xml_check_line(parser);
			i = 0;
		}
	}
	parser->name_buffer[0] = 0;
	parser->current_pos += i;
}

Bool xml_has_attributes(XMLParser *parser)
{
	xml_check_line(parser);

	if (!parser->text_parsing) {
		while ((parser->line_buffer[parser->current_pos] == ' ') || (parser->line_buffer[parser->current_pos] == '\t') ) {
			parser->current_pos++;
			if (parser->current_pos==parser->line_size) xml_check_line(parser);
		}
	}
	if (parser->line_buffer[parser->current_pos] == '>') {
		parser->current_pos++;
		return 0;
	}
	if ((parser->line_buffer[parser->current_pos] == '/') && (parser->line_buffer[parser->current_pos+1] == '>')) {
		return 0;
	}
	return 1;
}

Bool xml_element_done(XMLParser *parser, char *name)
{
	if (!parser->text_parsing) {
		while ((parser->line_buffer[parser->current_pos] == '\n') || (parser->line_buffer[parser->current_pos] == '\r') || (parser->line_buffer[parser->current_pos] == ' ') || (parser->line_buffer[parser->current_pos] == '\t') ) {
			parser->current_pos++;
			if (parser->current_pos==parser->line_size) xml_check_line(parser);
		}
	}
	xml_check_line(parser);
	if ((parser->line_buffer[parser->current_pos] == '/') && (parser->line_buffer[parser->current_pos+1] == '>')) {
		parser->current_pos += 2;
		return 1;
	}
	if (parser->line_buffer[parser->current_pos] != '<') return 0;
	if (parser->line_buffer[parser->current_pos + 1] != '/') return 0;
	if (strnicmp(&parser->line_buffer[parser->current_pos + 2], name, strlen(name))) return 0;
	xml_skip_attributes(parser);
	return 1;
}

void xml_skip_element(XMLParser *parser, char *name)
{
	char *str;
	char szElt[2048];
	if (!strcmp(name, "NULL")) return;

	while ((parser->line_buffer[parser->current_pos] == '\n') || (parser->line_buffer[parser->current_pos] == '\r') || (parser->line_buffer[parser->current_pos] == ' ') || (parser->line_buffer[parser->current_pos] == '\t') ) {
		parser->current_pos++;
		if (parser->current_pos==parser->line_size) xml_check_line(parser);
	}

	if ((parser->line_buffer[parser->current_pos] == '/') && (parser->line_buffer[parser->current_pos+1] == '>')) {
		parser->current_pos += 2;
		return;
	}
	if ((parser->line_buffer[parser->current_pos] == '<') && (parser->line_buffer[parser->current_pos+1] == '/')) {
		if (!strncmp(parser->line_buffer + parser->current_pos+2, name, strlen(name))) {
			parser->current_pos += 3+strlen(name);
			return;
		}
	}

	strcpy(szElt, name);
	xml_skip_attributes(parser);
	
	while (!xml_element_done(parser, szElt)) {
		str = xml_get_element(parser);
		if (str) xml_skip_element(parser, str);
		else parser->current_pos++;
	}
}

char *xml_get_attribute(XMLParser *parser)
{
	char att_sep;
	s32 k = 0;
	s32 i = 0;
	while ((parser->line_buffer[parser->current_pos + i] == ' ') || (parser->line_buffer[parser->current_pos + i] == '\t') ) i++;
	while (1) {
		if (!parser->line_buffer[parser->current_pos + i] || (parser->current_pos+i==parser->line_size)) {
			xml_check_line(parser);
			i = 0;
			continue;
		}
		else if (parser->line_buffer[parser->current_pos + i] == '=') break;
		parser->name_buffer[k] = parser->line_buffer[parser->current_pos + i];
		i++;
		k++;
	}
	parser->name_buffer[k] = 0;
	parser->current_pos += i + 1;
	i=0;
	while ((parser->line_buffer[parser->current_pos + i] == ' ') || (parser->line_buffer[parser->current_pos + i] == '\t') ) i++;
	k = 0;
	if (!parser->value_buffer) {
		parser->value_buffer = malloc(sizeof(char) * 500);
		parser->att_buf_size = 500;
	}

	att_sep = 0;
	while (1) {
		if (!parser->line_buffer[parser->current_pos + i] || (parser->current_pos+i==parser->line_size)) {
			parser->current_pos = parser->line_size;
			xml_check_line(parser);
			i = 0;
			parser->value_buffer[k] = ' ';
			k++;
			continue;
		}
		else if (!att_sep && ((parser->line_buffer[parser->current_pos + i] == '\"') || (parser->line_buffer[parser->current_pos + i] == '\''))) {
			att_sep = parser->line_buffer[parser->current_pos + i];
		} else if (parser->line_buffer[parser->current_pos + i] == att_sep) {
			break;
		} else {
			if ((u32) k >= parser->att_buf_size) {
				parser->att_buf_size += 500;
				parser->value_buffer = realloc(parser->value_buffer, sizeof(char) * parser->att_buf_size);
			}
			parser->value_buffer[k] = parser->line_buffer[parser->current_pos + i];
			k++;
		}
		i++;
	}
	parser->value_buffer[k] = 0;
	parser->current_pos += i + 1;
	return parser->name_buffer;
}

char *xml_get_css(XMLParser *parser) 
{
	char att_sep;
	s32 k = 0;
	s32 i = 0;
	while ((parser->line_buffer[parser->current_pos + i] == ' ') 
		|| (parser->line_buffer[parser->current_pos + i] == '\t') 
		|| (parser->line_buffer[parser->current_pos + i] == '{') 
		
		) i++;

	while (1) {
		if (!parser->line_buffer[parser->current_pos + i] || (parser->current_pos+i==parser->line_size)) {
			xml_check_line(parser);
			i = 0;
			continue;
		}
		else if (parser->line_buffer[parser->current_pos + i] == ':') break;
		parser->name_buffer[k] = parser->line_buffer[parser->current_pos + i];
		i++;
		k++;
	}
	parser->name_buffer[k] = 0;
	parser->current_pos += i + 1;
	i=0;
	while ((parser->line_buffer[parser->current_pos + i] == ' ') || (parser->line_buffer[parser->current_pos + i] == '\t') ) i++;
	k = 0;
	if (!parser->value_buffer) {
		parser->value_buffer = malloc(sizeof(char) * 500);
		parser->att_buf_size = 500;
	}

	att_sep = 0;
	while (1) {
		if (!parser->line_buffer[parser->current_pos + i] || (parser->current_pos+i==parser->line_size)) {
			parser->current_pos = parser->line_size;
			xml_check_line(parser);
			i = 0;
			parser->value_buffer[k] = ' ';
			k++;
			continue;
		} else if (parser->line_buffer[parser->current_pos + i] == '}') {
			break;
		} else {
			if ((u32) k >= parser->att_buf_size) {
				parser->att_buf_size += 500;
				parser->value_buffer = realloc(parser->value_buffer, sizeof(char) * parser->att_buf_size);
			}
			parser->value_buffer[k] = parser->line_buffer[parser->current_pos + i];
			k++;
		}
		i++;
	}
	parser->value_buffer[k] = 0;
	parser->current_pos += i + 1;
	return parser->name_buffer;
}

void xml_set_text_mode(XMLParser *parser, Bool text_mode)
{
	parser->text_parsing = text_mode;
}

Bool xml_load_text(XMLParser *parser) 
{
	u32 k = 0;
	u32 i = 0;

	if (parser->line_buffer[parser->current_pos] == '<') return 0;

	if (!parser->value_buffer) {
		parser->value_buffer = malloc(sizeof(char) * 500);
		parser->att_buf_size = 500;
	}

	if (!parser->current_pos) {
		parser->value_buffer[k] = '\n';
		k++;
	}
	while (1) {
		if (!parser->line_buffer[parser->current_pos + i] || (parser->current_pos+i==(u32) parser->line_size)) {
			parser->current_pos = parser->line_size;
			xml_check_line(parser);
			i = 0;
			parser->value_buffer[k] = '\n';
			k++;
			continue;
		}

		if (parser->line_buffer[parser->current_pos + i] == '<') break;
		if (k==parser->att_buf_size) {
			parser->att_buf_size += 500;
			parser->value_buffer = realloc(parser->value_buffer, sizeof(char) * parser->att_buf_size);
		}
		parser->value_buffer[k] = parser->line_buffer[parser->current_pos + i];
		k++;
		i++;
	}
	parser->value_buffer[k] = 0;
	parser->current_pos += i;
	return 1;
}

char *xml_translate_xml_string(char *str)
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
	void (*on_progress)(void *cbck, u32 done, u32 tot);

	u32 sax_state;
	u32 init_state;
	GF_List *attributes;
	GF_List *nodes;
	GF_List *entities;
	char att_sep;
	Bool in_entity, suspended;

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
		GF_SAXAttribute * att = gf_list_last(parser->attributes);
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


static Bool xml_sax_parse_attribute(GF_SAXParser *parser)
{
	char szVal[XML_LINE_SIZE], *skip_chars;
	u32 i;
	GF_SAXAttribute *att = NULL;

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
				GF_SAFEALLOC(att, sizeof(GF_SAXAttribute));
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

static void xml_sax_parse_comments(GF_SAXParser *parser)
{
	char *end = strstr(parser->buffer + parser->current_pos, "-->");
	if (!end) {
		parser->current_pos = parser->line_size;
		return;
	}
	parser->current_pos += 3 + (u32) (end - (parser->buffer + parser->current_pos) );
	parser->sax_state = SAX_STATE_TEXT_CONTENT;
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
			parser->current_pos+=1+i;
			if (!strncmp(parser->node_name, "!--", 3)) parser->sax_state = SAX_STATE_COMMENT;
			else if (!strcmp(parser->node_name, "?xml")) parser->init_state = 1;
			else if (!strcmp(parser->node_name, "!DOCTYPE")) parser->init_state = 2;
			else if (!strcmp(parser->node_name, "!ENTITY")) parser->sax_state = SAX_STATE_ENTITY;
			else if (!strcmp(parser->node_name, "![CDATA[")) parser->sax_state = SAX_STATE_CDATA;
			/*root node found*/
			else {
				xml_sax_flush_text(parser);
				parser->init_state = 0;
				gf_list_add(parser->nodes, strdup(parser->node_name));
			}
			break;
		case SAX_STATE_COMMENT:
			xml_sax_parse_comments(parser);
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
	u32 i, count;
	
	if (parser->unicode_type < 0) return GF_BAD_PARAM;

	count = gf_list_count(parser->entities);
	current = string;

	/*solve entities*/
	while (count) {
		char szName[200];
		XML_Entity *ent;
		char *entStart = strstr(current, "&");
		char *entEnd = entStart ? strstr(entStart, ";") : NULL;

		if (parser->in_entity) {
			if (!entEnd) return xml_sax_append_string(parser, string);

			current = entEnd+1;
			entStart = strrchr(parser->buffer, '&');
			strcpy(szName, entStart+1);
			entStart[0] = 0;
			entEnd[0] = 0;
			strcat(szName, string);
			parser->in_entity = 0;
		} else {
			if (!entStart) break;

			entStart[0] = 0;
			xml_sax_append_string(parser, current);

			xml_sax_parse(parser, 1);
			entStart[0] = '&';

			if (!entEnd) {
				parser->in_entity = 1;
				/*store entity start*/
				return xml_sax_append_string(parser, entStart);
			}
			strncpy(szName, entStart+1, entEnd - entStart - 1);
			szName[entEnd - entStart - 1] = 0;

			current = entEnd + 1;
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
		GF_SAXAttribute * att = gf_list_last(parser->attributes);
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
	unsigned char szLine[1024];
	if (!parser->gz_in) return GF_BAD_PARAM;

	while (!gzeof(parser->gz_in) && !parser->suspended) {
		u32 read = gzread(parser->gz_in, szLine, 1023);
		szLine[read] = 0;
		e = gf_xml_sax_parse(parser, szLine);
		if (e) break;
		parser->file_pos = gztell(parser->gz_in);
		if (parser->file_pos > parser->file_size) parser->file_size = parser->file_pos + 1;
		if (parser->on_progress) parser->on_progress(parser->sax_cbck, parser->file_pos, parser->file_size);
	}
	if (gzeof(parser->gz_in)) {
		e = GF_EOS;
		if (parser->on_progress) parser->on_progress(parser->sax_cbck, parser->file_size, parser->file_size);
	}
	return e;
}

GF_Err gf_xml_sax_parse_file(GF_SAXParser *parser, const char *fileName, void (*OnProgress)(void *cbck, u32 done, u32 tot))
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
	gzgets(gzInput, szLine, 4);
	szLine[4] = 0;
	e = gf_xml_sax_init(parser, szLine);
	if (e) return e;
	return xml_sax_read_file(parser);
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

char *gf_xml_sax_get_root_type(const char *file)
{
	struct _peek_type pt;
	pt.res = NULL;
	pt.parser = gf_xml_sax_new(on_peek_node_start, NULL, NULL, &pt);
	gf_xml_sax_parse_file(pt.parser, file, NULL);
	gf_xml_sax_del(pt.parser);
	return pt.res;
}

