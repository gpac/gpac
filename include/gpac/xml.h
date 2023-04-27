/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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
#include <gpac/bitstream.h>

/*!
\file <gpac/xml.h>
\brief XML functions.
 */

/*!
\addtogroup xml_grp
\brief XML Parsing functions

This section documents the XML functions (full doc parsing and SAX parsing) of the GPAC framework.

\defgroup sax_grp SAX Parsing
\ingroup xml_grp
\defgroup dom_grp DOM Parsing
\ingroup xml_grp
\defgroup xmlb_grp XML Binary Formatting
\ingroup xml_grp
*/

/*!
\addtogroup xml_grp
@{
*/


/*! Structure containing a parsed attribute*/
typedef struct
{
	/*! name or namespace:name*/
	char *name;
	/*! value*/
	char *value;
} GF_XMLAttribute;

/*! XML node types*/
enum
{

	/*! XML node*/
	GF_XML_NODE_TYPE = 0,
	/*! text node (including carriage return between XML nodes)*/
	GF_XML_TEXT_TYPE,
	/*! CDATA node*/
	GF_XML_CDATA_TYPE,
};

/*! Structure containing a parsed XML node*/
typedef struct _xml_node
{
	/*! Type of the node*/
	u32 type;
	/*!
	For DOM nodes: name
	For other (text, css, cdata), element content
	*/
	char *name;

	/*! namespace of the node, for XML node type only*/
	char *ns;
	/*! list of attributes of the node, for XML node type only - can be NULL if no attributes*/
	GF_List *attributes;
	/*! list of children nodes of the node, for XML node type only - can be NULL if no content*/
	GF_List *content;
	/*! original pos in parent (used for DASH MPD)*/
	u32 orig_pos;
} GF_XMLNode;

/*! @} */


/*!
\addtogroup sax_grp

	! SAX XML Parser API
	GPAC can do progressive loading of XML document using this SAX api.
@{
*/

/*! SAX XML Parser object*/
typedef struct _tag_sax_parser GF_SAXParser;
/*! SAX XML node start callback
	\param sax_cbck user data passed durinc creation of SAX parser
	\param node_name name of the XML node starting
	\param name_space namespace of the XML node starting
	\param attributes array of attributes declared for that XML node
	\param nb_attributes number of items in array
*/
typedef	void (*gf_xml_sax_node_start)(void *sax_cbck, const char *node_name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes);
/*! SAX XML node end callback
	\param sax_cbck user data passed durinc creation of SAX parser
	\param node_name name of the XML node starting
	\param name_space namespace of the XML node starting
*/
typedef	void (*gf_xml_sax_node_end)(void *sax_cbck, const char *node_name, const char *name_space);
/*! SAX text content callback
	\param sax_cbck user data passed durinc creation of SAX parser
	\param content text content of the node
	\param is_cdata if TRUE the content was ancapsulated in CDATA; otherwise this isthe content of a text node
*/
typedef	void (*gf_xml_sax_text_content)(void *sax_cbck, const char *content, Bool is_cdata);
/*! SAX progress callback
	\param cbck user data passed durinc creation of SAX parser
	\param done amount of bytes parsed from the file
	\param total total number of bytes in the file
*/
typedef	void (*gf_xml_sax_progress)(void *cbck, u64 done, u64 total);

/*! creates new sax parser - all callbacks are optionals
\param on_node_start callback for XML node start
\param on_node_end callback for XML node end
\param on_text_content callback for text content
\param cbck user data passed to callback functions
\return a SAX parser object
*/
GF_SAXParser *gf_xml_sax_new(gf_xml_sax_node_start on_node_start,
                             gf_xml_sax_node_end on_node_end,
                             gf_xml_sax_text_content on_text_content,
                             void *cbck);

/*! destroys sax parser
\param parser the SAX parser to destroy
*/
void gf_xml_sax_del(GF_SAXParser *parser);
/*! Inits the parser with string containing BOM, if any. BOM must be 4 char string with 0 terminaison. If BOM is NULL, parsing will assume UTF-8 compatible coding
\param parser the SAX parser to init
\param BOM the 4 character with 0 terminaison BOM or NULL
\return error code if any
*/
GF_Err gf_xml_sax_init(GF_SAXParser *parser, unsigned char *BOM);
/*! Parses input string data. string data MUST be terminated by the 0 character (eg 2 0s for UTF-16)
\param parser the SAX parser to use
\param string_bytes the string to parse
\return error code if any
*/
GF_Err gf_xml_sax_parse(GF_SAXParser *parser, const void *string_bytes);
/*! Suspends or resumes SAX parsing.
	When resuming on file, the function will run until suspended/end of file/error
	When resuming on steram, the function will simply return

\param parser the SAX parser to use
\param do_suspend if GF_TRUE, SAX parsing is suspended, otherwise SAX parsing is resumed
\return error code if any
*/
GF_Err gf_xml_sax_suspend(GF_SAXParser *parser, Bool do_suspend);
/*! parses file (potentially gzipped). OnProgress is optional, used to get progress callback
\param parser the SAX parser to use
\param fileName the file to parse
\param OnProgress the progress function to use. The callback for the progress function is the one assigned at SAX parser creation \ref gf_xml_sax_new
\return error code if any
*/
GF_Err gf_xml_sax_parse_file(GF_SAXParser *parser, const char *fileName, gf_xml_sax_progress OnProgress);
/*! Gets current line number, useful for inspecting errors
\param parser the SAX parser to use
\return current line number of SAX parser
*/
u32 gf_xml_sax_get_line(GF_SAXParser *parser);

/*! Peeks a node forward in the file. This may be used to pick the attribute of the first node found matching a given (attributeName, attributeValue) couple
\param parser SAX parser to use
\param att_name attribute name to look for
\param att_value value for this attribute
\param substitute gives the name of an additional XML node type to inspect to match the node. May be NULL.
\param get_attr gives the name of the attribute in the substitute node that matches the condition. If substitue node with name atribute is found, the content of the name attribute is returned. May be NULL.
\param end_pattern gives a string indicating where to stop looking in the document. May be NULL.
\param is_substitute is set to GF_TRUE if the return value corresponds to the content of the name attribute of the substitute element
\return name of the XML node found, or NULL if no match. This string has to be freed by the caller using gf_free

*/
char *gf_xml_sax_peek_node(GF_SAXParser *parser, char *att_name, char *att_value, char *substitute, char *get_attr, char *end_pattern, Bool *is_substitute);

/*! For file mode only, indicates if a file is compressed or not
\param parser SAX parser to use
\return 1 if file is compressed, 0 otherwise
*/
Bool gf_xml_sax_binary_file(GF_SAXParser *parser);

/*! Returns the last error found during parsing
\param parser SAX parser
\return the last SAX error encountered
*/
const char *gf_xml_sax_get_error(GF_SAXParser *parser);

/*! Returns the name of the root XML element
\param file the XML file to inspect
\param ret_code return error code if any
\return the name of the root XML element. This string has to be freed by the caller using gf_free
*/
char *gf_xml_get_root_type(const char *file, GF_Err *ret_code);

/*! Returns the position in bytes of the start of the current node being parsed. The byte offset points to the first < character in the opening tag.
\param parser SAX parser
\return the 0-based position of the current XML node
*/
u32 gf_xml_sax_get_node_start_pos(GF_SAXParser *parser);
/*! Returns the position in bytes of the end of the current node being parsed. The byte offset points to the last > character in the closing tag.
\param parser SAX parser
\return the 0-based position of the current XML node
*/
u32 gf_xml_sax_get_node_end_pos(GF_SAXParser *parser);

/*! @} */

/*!
\addtogroup dom_grp

	! DOM Full XML document Parsing API
	GPAC can do one-pass full document parsing of XML document using this DOM API.
@{
*/

/*! the DOM loader. GPAC can also load complete XML document in memory, using a DOM-like approach. This is a simpler
approach for document parsing not requiring progressive loading*/
typedef struct _tag_dom_parser GF_DOMParser;

/*! the DOM loader constructor
\return the created DOM loader, NULL if memory error
*/
GF_DOMParser *gf_xml_dom_new();

/*! the DOM loader constructor
\param parser the DOM parser to destroy
*/
void gf_xml_dom_del(GF_DOMParser *parser);

/*! Parses an XML document or fragment contained in a file
\param parser the DOM parser to use
\param file the file to parse
\param OnProgress an optional callback for the parser
\param cbk an optional user data for the progress callback
\return error code if any
*/
GF_Err gf_xml_dom_parse(GF_DOMParser *parser, const char *file, gf_xml_sax_progress OnProgress, void *cbk);
/*! Parses an XML document or fragment contained in memory
\param parser the DOM parser to use
\param string the string to parse
\return error code if any
*/
GF_Err gf_xml_dom_parse_string(GF_DOMParser *parser, char *string);
/*! Gets the last error that happened during the parsing. The parser aborts at the first error found within a SAX callback
\param parser the DOM parser to use
\return last error code if any
*/
const char *gf_xml_dom_get_error(GF_DOMParser *parser);
/*! Gets the current line of the parser. Used for error logging
\param parser the DOM parser to use
\return last loaded line number
*/
u32 gf_xml_dom_get_line(GF_DOMParser *parser);

/*! Gets the number of root nodes in the document (not XML compliant, but used in DASH for remote periods)
\param parser the DOM parser to use
\return the number of root elements in the document
*/
u32 gf_xml_dom_get_root_nodes_count(GF_DOMParser *parser);
/*! Gets the root node at the given index.
\param parser the DOM parser to use
\param idx index of the root node to get (0 being the first node)
\return the root element at the given index, or NULL if error
*/
GF_XMLNode *gf_xml_dom_get_root_idx(GF_DOMParser *parser, u32 idx);

/*! Gets the root node of the document and assign the parser root to NULL.
\param parser the DOM parser to use
\return the root element at the given index, or NULL if error. The element must be freed by the caller
*/
GF_XMLNode *gf_xml_dom_detach_root(GF_DOMParser *parser);

/*! Serialize a node to a string
\param node the node to flush
\param content_only Whether to include or not the parent node
\param no_escape if set, disable escape of XML reserved chars (<,>,",') in text nodes
\return The resulting serialization. The string has to be freed with gf_free
 */
char *gf_xml_dom_serialize(GF_XMLNode *node, Bool content_only, Bool no_escape);


/*! Serialize a root document node - same as \ref gf_xml_dom_serialize but insert \code <?xml version="1.0" encoding="UTF-8"?> \endcode at beginning
\param node the node to flush
\param content_only Whether to include or not the parent node
\param no_escape if set, disable escape of XML reserved chars (<,>,",') in text nodes
\return The resulting serialization. The string has to be freed with gf_free
 */
char *gf_xml_dom_serialize_root(GF_XMLNode *node, Bool content_only, Bool no_escape);

/*! Get the root element -- the only top level element -- of the document.
\param parser the DOM structure
\return The corresponding node if exists, otherwise NULL;
 */
GF_XMLNode *gf_xml_dom_get_root(GF_DOMParser *parser);

/*!
Creates an attribute with the given name and value.
\param name the attribute name
\param value the value
\return The created attribute ;
*/
GF_XMLAttribute *gf_xml_dom_create_attribute(const char* name, const char* value);

/*! Adds the node to the end of the list of children of this node.
\param node the GF_XMLNode node
\param child the GF_XMLNode child to append
\return Error code if any, otherwise GF_OK
 */
GF_Err gf_xml_dom_append_child(GF_XMLNode *node, GF_XMLNode *child);


/*! Create a node.
\param ns the target namespace or NULL if none
\param name the target name or NULL to create text node
\return new node, NULL if error. 
 */
GF_XMLNode *gf_xml_dom_node_new(const char* ns, const char* name);
/*! Destroys a node, its attributes and its children

\param node the node to free
 */
void gf_xml_dom_node_del(GF_XMLNode *node);

/*! Reset  a node

\param node the node to reset
\param reset_attribs if GF_TRUE, reset all node attributes
\param reset_children if GF_TRUE, reset all node children
 */
void gf_xml_dom_node_reset(GF_XMLNode *node, Bool reset_attribs, Bool reset_children);


/*! Gets the element and check that the namespace is known ('xmlns'-only supported for now)
\param n the node to process
\param expected_node_name optional expected name for node n
\param expected_ns_prefix optional expected namespace prefix for node n
\return error code or GF_OK
 */
GF_Err gf_xml_get_element_check_namespace(const GF_XMLNode *n, const char *expected_node_name, const char *expected_ns_prefix);

/*! Writes a string to an xml file and replaces forbidden chars with xml entities
 *\param file       the xml output file
 *\param before     optional string prefix (assumed xml-valid, pass NULL if not needed)
 *\param str        the string to dump and escape
 *\param after     optional string suffix (assumed xml-valid, pass NULL if not needed)
 */
void gf_xml_dump_string(FILE* file, const char* before, const char* str, const char* after);

/*! @} */


/*!
\addtogroup xmlb_grp

Binarization using XML in GPAC

GPAC uses a special node name BS (for BitSequence) in XML documents to transform text data into sequences of bits.
 This function inspects all child elements of the node and converts children node names BS into bits. BS take the following attributes:
 + bits: value gives the number of bits used to code a value or a length
 + value: value is a 32 bit signed value
 + dataOffset: value gives an offset into a file
 + dataLength: value gives the number of bits bytes to copy in a file
 + dataFile: value gives the name of the source file
 + textmode: indicates whether the file shall be opened in text or binary mode before reading
 + text: or string: value gives a string (length is first coded on number of bits in bits attribute)
 + fcc: value gives a four character code, coded on 32 bits
 + ID128: value gives a 128 bit vlue in hexadecimal
 + data64: value gives data coded as base64
 + data: value gives data coded in hexa

@{
*/

/*!\brief bit-sequence XML parser
\param bsroot the root node of XML document describing the bitstream to create
\param parent_url URL of the parent document
\param out_data pointer to output buffer allocated by the function to store the result
\param out_data_size pointer to output buffer size allocated by the function to store the result
\return error code if any or GF_OK
 */
GF_Err gf_xml_parse_bit_sequence(GF_XMLNode *bsroot, const char *parent_url, u8 **out_data, u32 *out_data_size);

/*!\brief bit-sequence XML parser

Parses XML bit sequence in an existing bitstream object. The syntax for the XML is the same as in \ref gf_xml_parse_bit_sequence
\param bsroot the root node of XML document describing the bitstream to create
\param parent_url URL of the parent document
\param base_media URL of base media file if any, relative to parent url. May be NULL
\param bs target bitstream to write the result into. The bitstream must be a dynamic write bitstream object
\return error code or GF_OK
 */
GF_Err gf_xml_parse_bit_sequence_bs(GF_XMLNode *bsroot, const char *parent_url, const char *base_media, GF_BitStream *bs);


/*! @} */

#ifdef __cplusplus
}
#endif

#endif		/*_XML_PARSER_H_*/
