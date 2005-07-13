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

#ifndef _XML_PARSER_H_
#define _XML_PARSER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>

/*since 0.2.2, we use zlib for xmt/x3d reading to handle gz files*/
#include <zlib.h>

#define XML_LINE_SIZE	8000

typedef struct 
{
	/*gz input file*/
	gzFile gz_in;
	/*eof*/
	Bool done;
	/*current line parsed (mainly used for error reports)*/
	u32 line;
	/*0: UTF-8, 1: UTF-16 BE, 2: UTF-16 LE. String input is always converted back to utf8*/
	u32 unicode_type;
	/*line buffer - needs refinement, cannot handle attribute values with size over XMT_LINE_SIZE (except string
	where space is used as a line-break...)*/
	char line_buffer[XML_LINE_SIZE];
	/*name buffer for elements and attributes*/
	char name_buffer[1000];
	/*dynamic buffer for attribute values*/
	char *value_buffer;
	u32 att_buf_size;
	/*line size and current position*/
	s32 line_size, current_pos;
	/*absolute line start position in file (needed for hard seeking in xmt-a)*/
	s32 line_start_pos;
	/*text parsing mode (text with markers), avoids getting rid of \n & co*/
	Bool text_parsing;

	/*file size and pos for user notif*/
	u32 file_size, file_pos;
	/*if set notifies current progress*/
	void (*OnProgress)(void *cbck, u32 done, u32 tot);
	void *cbk;
} XMLParser;

/*inits parser with given local file (handles gzip) - checks UTF8/16*/
GF_Err xml_init_parser(XMLParser *parser, const char *fileName);
/*reset parser (closes file and destroy value buffer)*/
void xml_reset_parser(XMLParser *parser);
/*get next element name*/
char *xml_get_element(XMLParser *parser);
/*returns 1 if given element is done*/
Bool xml_element_done(XMLParser *parser, char *name);
/*skip given element*/
void xml_skip_element(XMLParser *parser, char *name);
/*skip element attributes*/
void xml_skip_attributes(XMLParser *parser);
/*returns 1 if element has attributes*/
Bool xml_has_attributes(XMLParser *parser);
/*returns next attribute name*/
char *xml_get_attribute(XMLParser *parser);
/*returns attribute value*/
char *xml_get_attribute_value(XMLParser *parser);
/*translate input string, removing all special XML encoding. Returned string shall be freed by caller*/
char *xml_translate_xml_string(char *str);
/*checks if next line should be loaded - is not needed except when seeking file outside the parser routines*/
void xml_check_line(XMLParser *parser);
/*sets text mode on/off*/
void xml_set_text_mode(XMLParser *parser, Bool text_mode);

/*loads text between elements if any (returns 1) or retruns 0 if no text.
Text data is stored in attribute value buffer*/
Bool xml_load_text(XMLParser *parser);
/*fetches CSS-attribute {name : Val} in the same way as attribute (eg returns Name and stores Val in attribute value buffer)*/
char *xml_get_css(XMLParser *parser);

#ifdef __cplusplus
}
#endif


#endif		/*_XML_PARSER_H_*/

