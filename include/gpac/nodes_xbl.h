/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean Le Feuvre
 *    Copyright (c)2004-200X ENST - All rights reserved
 *
 *  This file is part of GPAC / XBL Elements 
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


#ifndef _GF_XBL_NODES_H
#define _GF_XBL_NODES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/internal/scenegraph_dev.h>
#include <gpac/svg_types.h>

#define XBL_String_datatype SVG_String_datatype 


/* TAG names are made of "TAG_XBL" + XBL element name (with - replaced by _) */
enum {
	TAG_XBL_bindings = GF_NODE_RANGE_FIRST_XBL,
	TAG_XBL_binding,
	TAG_XBL_content,
	TAG_XBL_children,
	TAG_XBL_implementation,
	TAG_XBL_constructor,
	TAG_XBL_destructor,
	TAG_XBL_field,
	TAG_XBL_property,
	TAG_XBL_getter,
	TAG_XBL_setter,
	TAG_XBL_method,
	TAG_XBL_parameter,
	TAG_XBL_body,
	TAG_XBL_handlers,
	TAG_XBL_handler,
	TAG_XBL_resources,
	TAG_XBL_stylesheet,
	TAG_XBL_image
};

/* Definition of SVG 3 attribute internal tags - 200 defined */
/* TAG names are made of "TAG_XBL_ATT_" + SVG attribute name (with - replaced by _) */
enum {
	TAG_XBL_ATT_id = TAG_SVG_ATT_RANGE_FIRST,
	TAG_XBL_ATT_extends,
	TAG_XBL_ATT_display,
	TAG_XBL_ATT_inheritstyle,
	TAG_XBL_ATT_includes,
	TAG_XBL_ATT_name,
	TAG_XBL_ATT_implements,
	TAG_XBL_ATT_type,
	TAG_XBL_ATT_readonly,
	TAG_XBL_ATT_onget,
	TAG_XBL_ATT_onset,
	TAG_XBL_ATT_event,
	TAG_XBL_ATT_action,
	TAG_XBL_ATT_phase,
	TAG_XBL_ATT_button,
	TAG_XBL_ATT_modifiers,
	TAG_XBL_ATT_keycode,
	TAG_XBL_ATT_key,
	TAG_XBL_ATT_charcode,
	TAG_XBL_ATT_clickcount,
	TAG_XBL_ATT_command,
	TAG_XBL_ATT_preventdefault,
	TAG_XBL_ATT_src,
	/*undefined attributes (when parsing) use this tag*/
	TAG_XBL_ATT_Unknown
};

struct _all_atts {
	DOM_String *id;
	DOM_String *extends;
	DOM_String *display;
	DOM_String *inheritstyle;
	DOM_String *includes;
	DOM_String *name;
	DOM_String *implements;
	DOM_String *type;
	DOM_String *readonly;
	DOM_String *onget;
	DOM_String *onset;
	DOM_String *event;
	DOM_String *action;
	DOM_String *phase;
	DOM_String *button;
	DOM_String *modifiers;
	DOM_String *keycode;
	DOM_String *key;
	DOM_String *charcode;
	DOM_String *clickcount;
	DOM_String *command;
	DOM_String *preventdefault;
	DOM_String *src;
};
#ifdef __cplusplus
}
#endif



#endif		/*_GF_SVG_NODES_H*/

