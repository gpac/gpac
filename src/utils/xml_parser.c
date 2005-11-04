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
	SAX_STATE_ELEMENT,
	SAX_STATE_COMMENT,
	SAX_STATE_ATT_NAME,
	SAX_STATE_ATT_VALUE,
	SAX_STATE_TEXT_CONTENT,
	SAX_STATE_ENTITY,
	SAX_STATE_SKIP_DOCTYPE,
	SAX_STATE_DONE,
	SAX_STATE_SYNTAX_ERROR,
};

void xml_sax_swap(XMLParser *parser)
{
	char szLine[XML_LINE_SIZE];
	if (parser->current_pos) {
		strcpy(szLine, parser->line_buffer + parser->current_pos);
		strcpy(parser->line_buffer, szLine);
		parser->line_size = strlen(parser->line_buffer);
		parser->current_pos = 0;
	}
}

void xml_sax_node_start(XMLParser *parser)
{
	parser->sax_xml_node_start(parser->sax_cbck, parser->name_buffer, NULL);
	fprintf(stdout, "<%s ", parser->name_buffer);
	gf_list_add(parser->nodes, strdup(parser->name_buffer));
}

void xml_sax_node_end(XMLParser *parser, Bool had_children)
{
	char *name;
	name = gf_list_last(parser->nodes);
	if (had_children) {
		if (strcmp(parser->name_buffer+1, name)) {
			parser->sax_state = SAX_STATE_SYNTAX_ERROR;
			return;
		}
	}
	assert(name);
	gf_list_rem_last(parser->nodes);

	parser->sax_xml_node_end(parser->sax_cbck, name, NULL);
	fprintf(stdout, "</%s>\n", name);
	free(name);
	if (!parser->init_state && !gf_list_count(parser->nodes)) parser->sax_state = SAX_STATE_DONE;

}

void xml_sax_attributes_done(XMLParser *parser)
{
	parser->sax_xml_attributes_parsed(parser->sax_cbck, parser->attributes);
	while (1) {
		XML_SAXAttribute * att = gf_list_last(parser->attributes);
		if (!att) break;
		gf_list_rem_last(parser->attributes);
		fprintf(stdout, "%s=\"%s\" ", att->name, att->value);
		if (att->name) free(att->name);
		if (att->name_space) free(att->name_space);
		if (att->value) free(att->value);
		free(att);
	}
	fprintf(stdout, ">\n");
}


Bool xml_sax_parse_attribute(XMLParser *parser)
{
	char *sep, szVal[XML_LINE_SIZE], *skip_chars;
	u32 i;
	XML_SAXAttribute *att = NULL;

	skip_chars = " \t\n\r";
	i=0;
	while (parser->current_pos+i < parser->line_size) {
		u8 c = parser->line_buffer[parser->current_pos+i];
		if (strchr(skip_chars, c)) {
			if (c=='\n') parser->line++;
			parser->current_pos++;
			continue;
		}
		if (parser->sax_state==SAX_STATE_ATT_NAME) {
			if ( (c=='/') || ((parser->init_state==1) && (c=='?')) ) {
				if ((parser->sax_state != SAX_STATE_ATT_NAME) || (parser->current_pos+i+1 == parser->line_size)) {
					parser->sax_state = SAX_STATE_SYNTAX_ERROR;
					return 1;
				}
				if (parser->line_buffer[parser->current_pos+i+1]=='>') {
					parser->current_pos+=i+2;
					parser->sax_state = (parser->init_state) ? SAX_STATE_ELEMENT : SAX_STATE_TEXT_CONTENT;
					/*done parsing attr AND elements*/
					if (!parser->init_state) {
						xml_sax_attributes_done(parser);
						xml_sax_node_end(parser, 0);
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
						return 1;
					}
					parser->sax_state = SAX_STATE_ELEMENT;
					return 0;
				}
				/*done parsing attr*/
				xml_sax_attributes_done(parser);
				parser->sax_state = SAX_STATE_TEXT_CONTENT;
				return 0;
			}
			else if (parser->init_state && (c=='[')) {
				parser->current_pos+=i+1;
				if (parser->init_state==1) {
					parser->sax_state = SAX_STATE_SYNTAX_ERROR;
					return 1;
				}
				parser->sax_state = SAX_STATE_ELEMENT;
				return 0;
			}

			if (c=='=') {
				parser->sax_state = SAX_STATE_ATT_VALUE;
				szVal[i] = 0;
				GF_SAFEALLOC(att, sizeof(XML_SAXAttribute));
				if (sep = strchr(szVal, ':')) {
					att->name = strdup(sep+1);
					sep[0] = 0;
					att->name_space = strdup(szVal);
				} else {
					att->name = strdup(szVal);
				}
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
				if (!att) {
					parser->att_sep = c;
					att = gf_list_last(parser->attributes);
					if (!att || att->value) {
						parser->sax_state = SAX_STATE_SYNTAX_ERROR;
						return 1;
					}
					skip_chars = "\t\n\r";
					parser->current_pos++;
					i=0;
				} else {
					szVal[i] = 0;
					if (!att->value) {
						att->value = strdup(szVal);
					} else {
						u32 len = strlen(att->value);
						att->value = realloc(att->value, sizeof(char)* (i+len));
						memcpy(att->value + len, szVal, i);
					}
					parser->current_pos+=i+1;
					xml_sax_swap(parser);
					parser->sax_state = SAX_STATE_ATT_NAME;
					return 0;
				}
			} else {
				if (!att) att = gf_list_last(parser->attributes);
				if (!att || !parser->att_sep) {
					parser->sax_state = SAX_STATE_SYNTAX_ERROR;
					return 1;
				}
				szVal[i] = c;
				i++;
			}
		} else {
			i++;
		}
	}
	if (att && i) {
		u32 len = att->value ? strlen(att->value) : 0;
		att->value = realloc(att->value, sizeof(char)* (i+len+1));
		memcpy(att->value + len, szVal, i-1);
		att->value[len+i+1] = 0;
	}
	return 1;
}
typedef struct
{
	char *name;
	char *value;
	u8 sep;
} XML_Entity;

void xml_sax_store_text(XMLParser *parser, u32 txt_len)
{
	u32 len, i;
	char *txt;
	if (!txt_len) return;

	txt = parser->line_buffer + parser->current_pos;
	len = 0;
	if (parser->value_buffer) len = strlen(parser->value_buffer);
	parser->value_buffer = realloc(parser->value_buffer, sizeof(char)*(len+txt_len+1) );
	for (i=0; i<txt_len; i++) parser->value_buffer[len+i] = txt[i];
	parser->value_buffer[i] = 0;
	parser->current_pos += txt_len;
}
void xml_sax_flush_text(XMLParser *parser)
{
	if (parser->value_buffer) {
		fprintf(stdout, "%s", parser->value_buffer);
		free(parser->value_buffer);
		parser->value_buffer = NULL;
	}
}

void xml_sax_skip_doctype(XMLParser *parser)
{
	while (parser->current_pos < parser->line_size) {
		if (parser->line_buffer[parser->current_pos]=='>') {
			parser->sax_state = SAX_STATE_ELEMENT;
			parser->current_pos++;
			xml_sax_swap(parser);
			return;
		}
		parser->current_pos++;
	}
}
void xml_sax_parse_entity(XMLParser *parser)
{
	char szName[1024];
	u32 i = 0;
	XML_Entity *ent = gf_list_last(parser->entities);
	char *skip_chars = " \t\n\r";
	i=0;
	if (ent && ent->value) ent = NULL;
	if (ent) skip_chars = NULL;

	while (parser->current_pos+i < parser->line_size) {
		u8 c = parser->line_buffer[parser->current_pos+i];
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

void xml_sax_parse_comments(XMLParser *parser)
{
	char *end = strstr(parser->line_buffer + parser->current_pos, "-->");
	if (!end) {
		parser->current_pos = parser->line_size;
		return;
	}
	parser->current_pos += 3 + (u32) (end - (parser->line_buffer + parser->current_pos) );
	parser->sax_state = SAX_STATE_TEXT_CONTENT;
}



static GF_Err xml_sax_parse(XMLParser *parser)
{
	u32 i;
	Bool is_text, is_end;
	char *elt;

	parser->line_size = strlen(parser->line_buffer);

	is_text = 0;
	while (parser->current_pos<parser->line_size) {
restart:
		is_text = 0;
		switch (parser->sax_state) {
		/*load an XML element*/
		case SAX_STATE_TEXT_CONTENT:
			is_text = 1;
		case SAX_STATE_ELEMENT:
			elt = NULL;
			i=0;
			while (parser->line_buffer[parser->current_pos+i] !='<') {
				if ((parser->init_state==2) && (parser->line_buffer[parser->current_pos+i] ==']')) {
					parser->sax_state = SAX_STATE_ATT_NAME;
					parser->current_pos+=i+1;
					goto restart;
				}
				i++;
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
				char c = parser->line_buffer[parser->current_pos+1+i];
				if (strchr(" \n\t\r>=", c)) break;
				if (c=='/') is_end = 1;
				parser->name_buffer[i] = c;
				i++;
				if (parser->current_pos+1+i==parser->line_size) goto exit;
			}
			parser->name_buffer[i] = 0;
			if (is_end) {
				xml_sax_flush_text(parser);
				xml_sax_node_end(parser, 1);
				parser->current_pos+=2+i;
				parser->sax_state = SAX_STATE_TEXT_CONTENT;
				break;
			}
			parser->sax_state = SAX_STATE_ATT_NAME;
			parser->current_pos+=1+i;
			if (!strncmp(parser->name_buffer, "!--", 3)) parser->sax_state = SAX_STATE_COMMENT;
			else if (!strcmp(parser->name_buffer, "?xml")) parser->init_state = 1;
			else if (!strcmp(parser->name_buffer, "!DOCTYPE")) parser->init_state = 2;
			else if (!strcmp(parser->name_buffer, "!ENTITY")) parser->sax_state = SAX_STATE_ENTITY;
			/*root node found*/
			else {
				parser->init_state = 0;
				xml_sax_flush_text(parser);
				xml_sax_node_start(parser);
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
		case SAX_STATE_SYNTAX_ERROR:
			return GF_BAD_PARAM;
		case SAX_STATE_DONE:
			return GF_EOS;
		}
	}
exit:
	if (is_text && i) xml_sax_store_text(parser, i);
	xml_sax_swap(parser);
	return GF_OK;
}


GF_Err xml_sax_parse_string(XMLParser *parser, char *string, u32 nb_chars)
{
	char *current = string;
	u32 i, count = gf_list_count(parser->entities);
	/*solve entities*/
	while (count) {
		char szName[200];
		XML_Entity *ent;
		char *entStart = strstr(current, "&");
		char *entEnd = strstr(current, ";");
		if (!entStart || !entEnd) break;
		strncpy(szName, entStart+1, entEnd - entStart - 1);
		szName[entEnd - entStart - 1] = 0;
		ent = NULL;
		for (i=0; i<count; i++) {
			ent = gf_list_get(parser->entities, i);
			if (!strcmp(ent->name, szName)) break;
			ent = NULL;
		}
		if (!ent) break;
		entStart[0] = 0;
		strcat(parser->line_buffer, current);
		xml_sax_parse(parser);
		entStart[0] = '&';

		strcat(parser->line_buffer, ent->value);
		xml_sax_parse(parser);
		current = entEnd + 1;
	}
	strcat(parser->line_buffer, current);
	return xml_sax_parse(parser);
}


GF_Err xml_sax_init(XMLParser *parser, char *BOM, u32 bom_len)
{
	u32 offset;
	if (bom_len!=4) return GF_BAD_PARAM;
	if (parser->unicode_type >= 0) return xml_sax_parse_string(parser, BOM, bom_len);

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
	strcpy(parser->line_buffer, "");
	parser->attributes = gf_list_new();
	parser->nodes = gf_list_new();
	parser->entities = gf_list_new();
	
	parser->sax_state = SAX_STATE_ELEMENT;
	return xml_sax_parse_string(parser, BOM + offset, bom_len-offset);
}

GF_Err xml_sax_parse_file(XMLParser *parser, const char *fileName)
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

	gzInput = gzopen(fileName, "rb");
	if (!gzInput) return GF_IO_ERR;
	parser->gz_in = gzInput;

	parser->unicode_type = -1;
	parser->line = 0;
	/*init SAX parser (unicode setup)*/
	gzgets(gzInput, szLine, 4);
	szLine[4] = 0;
	e = xml_sax_init(parser, szLine, 4);
	if (!e) {
		while (!gzeof(parser->gz_in)) {
			u32 read = gzread(gzInput, szLine, 1023);
			szLine[read] = 0;
			e = xml_sax_parse_string(parser, szLine, read);
			if (e) break;
		}
	}
	gzclose(parser->gz_in);
	parser->gz_in = NULL;
	while (1) {
		XML_SAXAttribute * att = gf_list_last(parser->attributes);
		if (!att) break;
		gf_list_rem_last(parser->attributes);
		fprintf(stdout, "%s=\"%s\" ", att->name, att->value);
		if (att->name) free(att->name);
		if (att->name_space) free(att->name_space);
		if (att->value) free(att->value);
		free(att);
	}
	gf_list_del(parser->attributes);
	parser->attributes = NULL;
	while (1) {
		char *name = gf_list_last(parser->nodes);
		if (!name) break;
		gf_list_rem_last(parser->nodes);
		free(name);
	}
	gf_list_del(parser->nodes);
	parser->nodes = NULL;
	while (1) {
		XML_Entity *ent = gf_list_last(parser->entities);
		if (!ent) break;
		gf_list_rem_last(parser->entities);
		if (ent->name) free(ent->name);
		if (ent->value) free(ent->value);
		free(ent);
	}
	gf_list_del(parser->nodes);
	parser->nodes = NULL;
	if (parser->value_buffer) free(parser->value_buffer);
	parser->value_buffer = NULL;

	return e<0 ? e : GF_OK;
}
