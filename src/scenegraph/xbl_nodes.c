/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean Le Feuvre
 *    Copyright (c)2004-200X ENST - All rights reserved
 *
 *  This file is part of GPAC / XBL Element sub-project
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

#ifndef GPAC_DISABLE_XBL

#include <gpac/internal/scenegraph_dev.h>

#include <gpac/nodes_xbl.h>

u32 gf_xbl_get_attribute_tag(u32 element_tag, const char *attribute_name)
{
	if (!attribute_name) return TAG_XBL_ATT_Unknown;
	if (!stricmp(attribute_name, "id")) return TAG_XBL_ATT_id;
	if (!stricmp(attribute_name, "extends")) return TAG_XBL_ATT_extends;
	if (!stricmp(attribute_name, "display")) return TAG_XBL_ATT_display;
	if (!stricmp(attribute_name, "inheritstyle")) return TAG_XBL_ATT_inheritstyle;
	if (!stricmp(attribute_name, "includes")) return TAG_XBL_ATT_includes;
	if (!stricmp(attribute_name, "name")) return TAG_XBL_ATT_name;
	if (!stricmp(attribute_name, "implements")) return TAG_XBL_ATT_implements;
	if (!stricmp(attribute_name, "type")) return TAG_XBL_ATT_type;
	if (!stricmp(attribute_name, "readonly")) return TAG_XBL_ATT_readonly;
	if (!stricmp(attribute_name, "onget")) return TAG_XBL_ATT_onget;
	if (!stricmp(attribute_name, "onset")) return TAG_XBL_ATT_onset;
	if (!stricmp(attribute_name, "event")) return TAG_XBL_ATT_event;
	if (!stricmp(attribute_name, "action")) return TAG_XBL_ATT_action;
	if (!stricmp(attribute_name, "phase")) return TAG_XBL_ATT_phase;
	if (!stricmp(attribute_name, "button")) return TAG_XBL_ATT_button;
	if (!stricmp(attribute_name, "modifiers")) return TAG_XBL_ATT_modifiers;
	if (!stricmp(attribute_name, "keycode")) return TAG_XBL_ATT_keycode;
	if (!stricmp(attribute_name, "key")) return TAG_XBL_ATT_key;
	if (!stricmp(attribute_name, "charcode")) return TAG_XBL_ATT_charcode;
	if (!stricmp(attribute_name, "clickcount")) return TAG_XBL_ATT_clickcount;
	if (!stricmp(attribute_name, "command")) return TAG_XBL_ATT_command;
	if (!stricmp(attribute_name, "preventdefault")) return TAG_XBL_ATT_preventdefault;
	if (!stricmp(attribute_name, "src")) return TAG_XBL_ATT_src;
	return TAG_XBL_ATT_Unknown;
}

u32 gf_xbl_get_attribute_type(u32 tag)
{
	return XBL_String_datatype;
}

const char*gf_xbl_get_attribute_name(u32 tag)
{
	switch(tag) {
		case TAG_XBL_ATT_id: return "id";
		case TAG_XBL_ATT_extends: return "extends";
		case TAG_XBL_ATT_display: return "display";
		case TAG_XBL_ATT_inheritstyle: return "inheritstyle";
		case TAG_XBL_ATT_includes: return "includes";
		case TAG_XBL_ATT_name: return "name";
		case TAG_XBL_ATT_implements: return "implements";
		case TAG_XBL_ATT_type: return "type";
		case TAG_XBL_ATT_readonly: return "readonly";
		case TAG_XBL_ATT_onget: return "onget";
		case TAG_XBL_ATT_onset: return "onset";
		case TAG_XBL_ATT_event: return "event";
		case TAG_XBL_ATT_action: return "action";
		case TAG_XBL_ATT_phase: return "phase";
		case TAG_XBL_ATT_button: return "button";
		case TAG_XBL_ATT_modifiers: return "modifiers";
		case TAG_XBL_ATT_keycode: return "keycode";
		case TAG_XBL_ATT_key: return "key";
		case TAG_XBL_ATT_charcode: return "charcode";
		case TAG_XBL_ATT_clickcount: return "clickcount";
		case TAG_XBL_ATT_command: return "command";
		case TAG_XBL_ATT_preventdefault: return "preventdefault";
		case TAG_XBL_ATT_src: return "src";
		default: return "unknown";
	}
}

GF_DOMAttribute *gf_xbl_create_attribute(GF_DOMNode *elt, u32 tag)
{
	GF_DOMAttribute *att = elt->attributes;

	SVGAttribute *last_att = NULL;

	while (att) {
		if ((u32) att->tag == tag) {
			return att;
		}
		last_att = att;
		att = att->next;
	}

	GF_SAFEALLOC(att, GF_DOMAttribute);
	att->data_type = (u16) SVG_String_datatype;
	att->tag = (u16) tag;
	if (!elt->attributes) elt->attributes = att;
	else last_att->next = att;
	return att;
}

#if 0 
GF_EXPORT
void gf_xbl_flatten_attributes(GF_DOMNode *e, XBLAllAttributes *all_atts)
{
	XBLAllAttributes *att;
	memset(all_atts, 0, sizeof(XBLAllAttributes));
	if (e->sgprivate->tag <= GF_NODE_FIRST_DOM_NODE_TAG) return;
	att = e->attributes;
	while (att) {
		switch(att->tag) {
		case TAG_XBL_ATT_id: all_atts->id = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_extends: all_atts->extends = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_display: all_atts->display = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_inheritstyle: all_atts->inheritstyle = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_includes: all_atts->includes = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_name: all_atts->name = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_implements: all_atts->implements = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_type: all_atts->type = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_readonly: all_atts->readonly = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_onget: all_atts->onget = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_onset: all_atts->onset = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_event: all_atts->event = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_action: all_atts->action = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_phase: all_atts->phase = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_button: all_atts->button = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_modifiers: all_atts->modifiers = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_keycode: all_atts->keycode = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_key: all_atts->key = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_charcode: all_atts->charcode = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_clickcount: all_atts->clickcount = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_command: all_atts->command = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_preventdefault: all_atts->preventdefault = (DOM_String *)att->data; break;
		case TAG_XBL_ATT_src: all_atts->src = (DOM_String *)att->data; break;
		}
	att = att->next;
	}
}
#endif 

u32 gf_xbl_get_element_tag(const char *element_name)
{
	if (!element_name) return TAG_UndefinedNode;
	if (!stricmp(element_name, "bindings")) return TAG_XBL_bindings;
	if (!stricmp(element_name, "binding")) return TAG_XBL_binding;
	if (!stricmp(element_name, "content")) return TAG_XBL_content;
	if (!stricmp(element_name, "children")) return TAG_XBL_children;
	if (!stricmp(element_name, "implementation")) return TAG_XBL_implementation;
	if (!stricmp(element_name, "constructor")) return TAG_XBL_constructor;
	if (!stricmp(element_name, "destructor")) return TAG_XBL_destructor;
	if (!stricmp(element_name, "field")) return TAG_XBL_field;
	if (!stricmp(element_name, "property")) return TAG_XBL_property;
	if (!stricmp(element_name, "getter")) return TAG_XBL_getter;
	if (!stricmp(element_name, "setter")) return TAG_XBL_setter;
	if (!stricmp(element_name, "method")) return TAG_XBL_method;
	if (!stricmp(element_name, "parameter")) return TAG_XBL_parameter;
	if (!stricmp(element_name, "body")) return TAG_XBL_body;
	if (!stricmp(element_name, "handlers")) return TAG_XBL_handlers;
	if (!stricmp(element_name, "handler")) return TAG_XBL_handler;
	if (!stricmp(element_name, "resources")) return TAG_XBL_resources;
	if (!stricmp(element_name, "stylesheet")) return TAG_XBL_stylesheet;
	if (!stricmp(element_name, "image")) return TAG_XBL_image;
	return TAG_UndefinedNode;
}

const char *gf_xbl_get_element_name(u32 tag)
{
	switch(tag) {
	case TAG_XBL_bindings: return "bindings";
	case TAG_XBL_binding: return "binding";
	case TAG_XBL_content: return "content";
	case TAG_XBL_children: return "children";
	case TAG_XBL_implementation: return "implementation";
	case TAG_XBL_constructor: return "constructor";
	case TAG_XBL_destructor: return "destructor";
	case TAG_XBL_field: return "field";
	case TAG_XBL_property: return "property";
	case TAG_XBL_getter: return "getter";
	case TAG_XBL_setter: return "setter";
	case TAG_XBL_method: return "method";
	case TAG_XBL_parameter: return "parameter";
	case TAG_XBL_body: return "body";
	case TAG_XBL_handlers: return "handlers";
	case TAG_XBL_handler: return "handler";
	case TAG_XBL_resources: return "resources";
	case TAG_XBL_stylesheet: return "stylesheet";
	case TAG_XBL_image: return "image";
	default: return "XBL_UndefinedNode";
	}
}

#endif /*GPAC_DISABLE_XBL*/


