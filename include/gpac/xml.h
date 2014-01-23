/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
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
#include <gpac/list.h>

/*!
 *	\file <gpac/xml.h>
 *	\brief XML functions.
 */

/*!
 *	\addtogroup xml_grp XML
 *	\ingroup utils_grp
 *	\brief XML Parsing functions
 *
 *This section documents the XML functions of the GPAC framework.\n
 *	@{
 */



typedef struct
{
	/*name or namespace:name*/
	char *name;
	/*value*/
	char *value;
} GF_XMLAttribute;

/*XML node types*/
enum
{
	GF_XML_NODE_TYPE = 0,
	GF_XML_TEXT_TYPE,
	GF_XML_CDATA_TYPE,
};

typedef struct _xml_node
{
	u32 type;
	/*
	For DOM nodes: name
	For other (text, css, cdata), element content
	*/
	char *name;

	/*for DOM nodes only*/
	char *ns;	/*namespace*/
	GF_List *attributes;
	GF_List *content;
} GF_XMLNode;



/*
	SAX XML Parser
*/

typedef struct _tag_sax_parser GF_SAXParser;
typedef	void (*gf_xml_sax_node_start)(void *sax_cbck, const char *node_name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes);
typedef	void (*gf_xml_sax_node_end)(void *sax_cbck, const char *node_name, const char *name_space);
typedef	void (*gf_xml_sax_text_content)(void *sax_cbck, const char *content, Bool is_cdata);

typedef	void (*gf_xml_sax_progress)(void *cbck, u64 done, u64 tot);

/*creates new sax parser - all callbacks are optionals*/
GF_SAXParser *gf_xml_sax_new(gf_xml_sax_node_start on_node_start, 
							 gf_xml_sax_node_end on_node_end,
							 gf_xml_sax_text_content on_text_content,
							 void *cbck);

/*destroys sax parser */
void gf_xml_sax_del(GF_SAXParser *parser);
/*inits parser with BOM. BOM must be 4 char string with 0 terminaison. If BOM is NULL, parsing will
assume UTF-8 compatible coding*/
GF_Err gf_xml_sax_init(GF_SAXParser *parser, unsigned char *BOM);
/*parses input string data. string data MUST be terminated by the 0 character (eg 2 0s for UTF-16)*/
GF_Err gf_xml_sax_parse(GF_SAXParser *parser, const void *string_bytes);
/*suspends/resume sax parsing. 
	When resuming on file, the function will run until suspended/end of file/error
	When resuming on steram, the function will simply return
*/
GF_Err gf_xml_sax_suspend(GF_SAXParser *parser, Bool do_suspend);
/*parses file (potentially gzipped). OnProgress is optional, used to get progress callback*/
GF_Err gf_xml_sax_parse_file(GF_SAXParser *parser, const char *fileName, gf_xml_sax_progress OnProgress);
/*get current line number*/
u32 gf_xml_sax_get_line(GF_SAXParser *parser);
/*get file size - may be inaccurate if gzipped (only compressed file size is known)*/
u32 gf_xml_sax_get_file_size(GF_SAXParser *parser);
/*get current file position*/
u32 gf_xml_sax_get_file_pos(GF_SAXParser *parser);

/*peeks a node forward in the file. May be used to pick the attribute of the first node found matching a given (attributeName, attributeValue) couple*/
char *gf_xml_sax_peek_node(GF_SAXParser *parser, char *att_name, char *att_value, char *substitute, char *get_attr, char *end_pattern, Bool *is_substitute);

/*file mode only, returns 1 if file is compressed, 0 otherwise*/
Bool gf_xml_sax_binary_file(GF_SAXParser *parser);

const char *gf_xml_sax_get_error(GF_SAXParser *parser);

char *gf_xml_get_root_type(const char *file, GF_Err *ret_code);

u32 gf_xml_sax_get_node_start_pos(GF_SAXParser *parser);
u32 gf_xml_sax_get_node_end_pos(GF_SAXParser *parser);

typedef struct _tag_dom_parser GF_DOMParser;
GF_DOMParser *gf_xml_dom_new();
void gf_xml_dom_del(GF_DOMParser *parser);
GF_Err gf_xml_dom_parse(GF_DOMParser *parser, const char *file, gf_xml_sax_progress OnProgress, void *cbk);
GF_Err gf_xml_dom_parse_string(GF_DOMParser *dom, char *string);
const char *gf_xml_dom_get_error(GF_DOMParser *parser);
u32 gf_xml_dom_get_line(GF_DOMParser *parser);

/*
 *\brief Serialize a node
 *
 * Flush a node in a char
 *
 *\param node the node to flush
 *\param content_only Whether to include or not the parent node
 *\return The resulting serialization
 */
char *gf_xml_dom_serialize(GF_XMLNode *node, Bool content_only);

/*
 *\brief Create the root element of the DOM
 *
 * Create the root element -- the only top level element -- of the document. 
 *
 *\param parser the DOM structure
 *\return The created node if creation occurs properly, otherwise NULL;
 */
GF_XMLNode *gf_xml_dom_create_root(GF_DOMParser *parser, const char* name);

/*
 *\brief Get the root element of the DOM
 *
 * Get the root element -- the only top level element -- of the document. 
 *
 *\param parser the DOM structure
 *\return The corresponding node if exists, otherwise NULL;
 */
GF_XMLNode *gf_xml_dom_get_root(GF_DOMParser *parser);

/*
 *\brief Return and detach the root element of the DOM
 *
 * Return and detach the root element of the DOM
 *
 *\param parser the DOM structure
 *\return The corresponding node if exists, otherwise NULL;
 */
GF_XMLNode *gf_xml_dom_detach_root(GF_DOMParser *parser);

/*
 *\brief Sets an attribute value for this element.
 *
 * Sets an attribute value for this element
 *
 *\param node the GF_XMLNode node
 *\param name the name of the attribute
 *\param value the value of the attribute
 *\return The created attribute if setting occurs properly, otherwise NULL;
 */
GF_XMLAttribute *gf_xml_dom_set_attribute(GF_XMLNode *node, const char* name, const char* value);

/*
 *\brief Gets the attribute for this element with the given name.
 *
 * Gets the attribute for this element with the given name.
 *
 *\param node the GF_XMLNode node
 *\param name the attribute name
 *\return The corresponding attribute if exists, otherwise NULL;
 */
GF_XMLAttribute *gf_xml_dom_get_attribute(GF_XMLNode *node, const char* name);

/*
 *\brief Adds the node to the end of the list of children of this node.
 *
 * Adds the node to the end of the list of children of this node.
 *
 *\param node the GF_XMLNode node
 *\param child the GF_XMLNode child to append
 *\return GF_OK if append occurs properly, otherwise a GF_Err
 */
GF_Err gf_xml_dom_append_child(GF_XMLNode *node, GF_XMLNode *child);

/*
 *\brief Node constructor.
 *
 * Creates a node with the given name and namespace URI.
 *
 *\param ns the node namespace
 *\param name the name namespace
 *\return The created GF_XMLNode if creation occurs properly, otherwise NULL;
 */
GF_XMLNode* gf_xml_dom_node_new(const char* ns, const char* name);

/*
 *\brief Node destructor.
 *
 * Free a node, its attributes and its childs.
 *
 *\param node the node to free
 */
void gf_xml_dom_node_del(GF_XMLNode *node);

/*
 *\brief bitsequence parser.
 *
 * inspects all child elements of the node and converts <BS> children into bits. BS take the following attributes:.
 *bits: value gives the number of bits used to code a value or a length 
 *value: value is a 32 bit signed value 
 *dataOffset: value gives an offset into a file
 *dataLength: value gives the number of bits bytes to copy in a file
 *dataFile: value gives the name of the source file
 *textmode: indicates whether the file shall be opened in text or binary mode before reading
 *text: or string: value gives a string (length is first coded on number of bits in bits attribute)
 *fcc: value gives a four character code, coded on 32 bits
 *ID128: value gives a 128 bit vlue in hexadecimal
 *data64: value gives data coded as base64 
 *data: value gives data coded in hexa 
 *
 *
 *
 *\param node the root node of the bitstream to create
 *\param out_data pointer to output buffer allocated by the function to store the result
 *\param out_data_size pointer to output buffer size allocated by the function to store the result
 *\return error code or GF_OK
 */
GF_Err gf_xml_parse_bit_sequence(GF_XMLNode *bsroot, char **out_data, u32 *out_data_size);

/*! @} */

#ifdef __cplusplus
}
#endif


#endif		/*_XML_PARSER_H_*/

