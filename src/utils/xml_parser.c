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

