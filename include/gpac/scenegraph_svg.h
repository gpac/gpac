/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Scene Graph sub-project
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

#ifndef _GF_SG_SVG_H_
#define _GF_SG_SVG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/scenegraph.h>
#include <gpac/svg_types.h>

/*******************************************************************************
 * 
 *          DOM base scene graph
 *
 *******************************************************************************/

enum
{
	/*this tag is set for a full dom attribute only - attribute name is then available*/
	TAG_DOM_ATTRIBUTE_FULL = 0,
	TAG_SVG_ATT_RANGE_FIRST,
	TAG_XBL_ATT_RANGE_FIRST = TAG_SVG_ATT_RANGE_FIRST + 256,
};


#define GF_DOM_BASE_ATTRIBUTE	\
	u16 tag;	/*attribute identifier*/	\
	u16 data_type; /*attribute datatype*/	  \
	void *data; /*data pointer*/				\
	struct __dom_base_attribute *next; /*next attribute*/

#define GF_DOM_FULL_ATTRIBUTE	\
	GF_DOM_ATTRIBUTE	\

typedef struct __dom_base_attribute
{
	GF_DOM_BASE_ATTRIBUTE
} GF_DOMAttribute;

typedef struct __dom_full_attribute
{
	GF_DOM_BASE_ATTRIBUTE
	char *name; /*attribute name - in this case, the data field is the attribute literal value*/
} GF_DOMFullAttribute;

#define GF_DOM_BASE_NODE	 \
	BASE_NODE				\
	CHILDREN				\
	GF_DOMAttribute *attributes;

typedef struct __dom_base_node
{
	GF_DOM_BASE_NODE
} GF_DOMNode;

typedef struct __dom_full_node
{
	GF_DOM_BASE_NODE
	char *name;
	char *ns;
} GF_DOMFullNode;

enum
{
	GF_DOM_TEXT_REGULAR = 0,
	GF_DOM_TEXT_CDATA,
	/*inserted text node (typically external script)*/
	GF_DOM_TEXT_INSERTED
};

typedef struct
{
	BASE_NODE
	CHILDREN
	char *textContent;
	u32 type;
} GF_DOMText;

/*creates a new text node, assign string (does NOT duplicate it) and register node with parent if desired*/
GF_DOMText *gf_dom_add_text_node(GF_Node *parent, char *text_data);

/*creates a new text node - this DOES NOT register the node at all*/
GF_DOMText *gf_dom_new_text_node(GF_SceneGraph *sg);

typedef struct
{
	BASE_NODE
	CHILDREN
	char *data;
	u32 data_size;
	GF_List *updates;
} GF_DOMUpdates;

/*creates a new updates node and register node with parent*/
GF_DOMUpdates *gf_dom_add_updates_node(GF_Node *parent);


/* 
	DOM event handling
*/

typedef struct
{
	/*event type, as defined in <gpac/events.h>*/
	u32 type;
	/*event phase type, READ-ONLY
	0: at target, 1: bubbling, 2: capturing , 3: canceled
	*/
	u8 event_phase;
	u8 bubbles;
	u8 cancelable;
	/*output only - indicates UI events (mouse) have been detected*/
	u8 has_ui_events;
	GF_Node *target;
	GF_Node *currentTarget;
	Double timestamp;
	/*UIEvent extension. For mouse extensions, stores button type. For key event, the key code*/
	u32 detail;
	/*MouseEvent extension*/
	s32 screenX, screenY;
	s32 clientX, clientY;
	/*key flags*/
	u32 key_flags;
	/*key hardware code*/
	u32 key_hw_code;
	GF_Node *relatedTarget;
	/*Zoom event*/
	GF_Rect screen_rect;
	GF_Point2D prev_translate, new_translate;
	Fixed prev_scale, new_scale;
	/* CPU */
	u32 cpu_percentage;
	/* Battery */
	Bool onBattery;
	u32 batteryState, batteryLevel;
	/*smil event time*/
	Double smil_event_time;
	/* mutation event */
	GF_Node *relatedNode;
} GF_DOM_Event;

/*fires event on the specified node
BE CAREFULL: event execution may very well destroy ANY node, especially the event target node !!
use_stack: a list of parent node/use node for bubbling phase
*/
Bool gf_dom_event_fire(GF_Node *node, GF_List *use_stack, GF_DOM_Event *event);

u32 gf_dom_event_type_by_name(const char *name);
const char *gf_dom_event_get_name(u32 type);

const char *gf_dom_get_key_name(u32 key_identifier);
u32 gf_dom_get_key_type(char *key_name);


/*listener is simply a node added to the node events list. 
Only one observer can be attached to a listener. The listener will remove itself from the observer
event list when destructed.*/

typedef struct __xml_ev_handler 
{
	GF_DOM_BASE_NODE
	void (*handle_event)(GF_Node *hdl, GF_DOM_Event *event);
	/*if handler targets a VRML script, point to the script here*/
	void *js_context;
} GF_DOMHandler;

/*adds a listener to the node.
The listener node is NOT registered with the node (it may very well not be a direct child of the node)
@listener is a listenerElement (XML event)
*/
GF_Err gf_dom_listener_add(GF_Node *node, GF_Node *listener);
GF_Err gf_dom_listener_del(GF_Node *node, GF_Node *listener);
u32 gf_dom_listener_count(GF_Node *node);
GF_Node *gf_dom_listener_get(GF_Node *node, u32 i);

/*creates a default listener/handler for the given event on the given node, and return the 
handler element to allow for handler function override
owner is the node holding the created listener/handler. If null, they are registered with the observer*/
GF_DOMHandler *gf_dom_listener_build(GF_Node *observer, u32 event_type, u32 event_param, GF_Node *owner);





/**************************************************
 *  SVG's styling properties (see 6.1 in REC 1.1) *
 *************************************************/

typedef struct {
	/* Tiny 1.2 properties*/
	SVG_Paint					*color;
	SVG_Paint					*fill; 
	SVG_Paint					*stroke;
	SVG_Paint					*solid_color;
	SVG_Paint					*stop_color;
	SVG_Paint					*viewport_fill;

	SVG_Number					*fill_opacity;
	SVG_Number					*solid_opacity;
	SVG_Number					*stop_opacity;
	SVG_Number					*stroke_opacity;
	SVG_Number					*viewport_fill_opacity;
	SVG_Number					*opacity; /* Restricted property in Tiny 1.2 */

	SVG_Number					*audio_level;

	SVG_RenderingHint			*color_rendering;
	SVG_RenderingHint			*image_rendering;
	SVG_RenderingHint			*shape_rendering;
	SVG_RenderingHint			*text_rendering;

	SVG_Display					*display; 
	SVG_Visibility				*visibility;
	SVG_Overflow				*overflow; /* Restricted property in Tiny 1.2 */
	
	SVG_FontFamily				*font_family;
	SVG_FontSize				*font_size;
	SVG_FontStyle				*font_style; 
	SVG_FontWeight				*font_weight; 
	SVG_FontVariant				*font_variant; 
	SVG_Number					*line_increment;	
	SVG_TextAnchor				*text_anchor;
	SVG_DisplayAlign			*display_align;
	SVG_TextAlign				*text_align;

	SVG_PointerEvents			*pointer_events;
	
	SVG_FillRule				*fill_rule; 
	
	SVG_StrokeDashArray			*stroke_dasharray;
	SVG_Length					*stroke_dashoffset;
	SVG_StrokeLineCap			*stroke_linecap; 
	SVG_StrokeLineJoin			*stroke_linejoin; 
	SVG_Number					*stroke_miterlimit; 
	SVG_Length					*stroke_width;
	SVG_VectorEffect			*vector_effect;
	
	/* Full 1.1 props, i.e. not implemented */
/*
	SVG_String *font;
	SVG_String *font_size_adjust;
	SVG_String *font_stretch;
	SVG_String *direction;
	SVG_String *letter_spacing;
	SVG_String *text_decoration;
	SVG_String *unicode_bidi;
	SVG_String *word_spacing;
	SVG_String *clip; 
	SVG_String *cursor;
	SVG_String *clip_path;
	SVG_String *clip_rule;
	SVG_String *mask;
	SVG_String *enable_background;
	SVG_String *filter;
	SVG_String *flood_color;
	SVG_String *flood_opacity;
	SVG_String *lighting_color;
	SVG_String *color_interpolation;
	SVG_String *color_interpolation_filters;
	SVG_String *color_profile;
	SVG_String *marker;
	SVG_String *marker_end;
	SVG_String *marker_mid;
	SVG_String *marker_start;
	SVG_String *alignment_baseline;
	SVG_String *baseline_shift;
	SVG_String *dominant_baseline;
	SVG_String *glyph_orientation_horizontal;
	SVG_String *glyph_orientation_vertical;
	SVG_String *kerning;
	SVG_String *writing_mode;
*/
} SVGPropertiesPointers;

typedef struct {
	SVG_Paint					color;
	SVG_Paint					fill; 
	SVG_Paint					stroke;
	SVG_Paint					solid_color;
	SVG_Paint					stop_color;
	SVG_Paint					viewport_fill;

	SVG_Number					fill_opacity;
	SVG_Number					solid_opacity;
	SVG_Number					stop_opacity;
	SVG_Number					stroke_opacity;
	SVG_Number					viewport_fill_opacity;
	SVG_Number					opacity; /* Restricted property in Tiny 1.2 */

	SVG_Number  				audio_level;

	SVG_RenderingHint			color_rendering;
	SVG_RenderingHint			image_rendering;
	SVG_RenderingHint			shape_rendering;
	SVG_RenderingHint			text_rendering;

	SVG_Display					display; 
	SVG_Visibility				visibility;
	SVG_Overflow				overflow; /* Restricted property in Tiny 1.2 */

	SVG_FontFamily				font_family;
	SVG_FontSize				font_size;
	SVG_FontStyle				font_style; 
	SVG_FontWeight				font_weight; 
	SVG_FontVariant				font_variant; 
	SVG_Number					line_increment;
	SVG_TextAnchor				text_anchor;
	SVG_DisplayAlign			display_align;
	SVG_TextAlign				text_align;

	SVG_PointerEvents			pointer_events;

	SVG_FillRule				fill_rule; 

	SVG_StrokeDashArray			stroke_dasharray;
	SVG_Length					stroke_dashoffset;
	SVG_StrokeLineCap			stroke_linecap; 
	SVG_StrokeLineJoin			stroke_linejoin; 
	SVG_Number					stroke_miterlimit; 
	SVG_Length					stroke_width;
	SVG_VectorEffect			vector_effect;	

	/* Full 1.1 props, i.e. not implemented */
/*
	SVG_FontVariant *font_variant; 
	SVG_String *font;
	SVG_String *font_size_adjust;
	SVG_String *font_stretch;
	SVG_String *direction;
	SVG_String *letter_spacing;
	SVG_String *text_decoration;
	SVG_String *unicode_bidi;
	SVG_String *word_spacing;
	SVG_String *clip; 
	SVG_String *cursor;
	SVG_String *clip_path;
	SVG_String *clip_rule;
	SVG_String *mask;
	SVG_String *enable_background;
	SVG_String *filter;
	SVG_String *flood_color;
	SVG_String *flood_opacity;
	SVG_String *lighting_color;
	SVG_String *color_interpolation;
	SVG_String *color_interpolation_filters;
	SVG_String *color_profile;
	SVG_String *marker;
	SVG_String *marker_end;
	SVG_String *marker_mid;
	SVG_String *marker_start;
	SVG_String *alignment_baseline;
	SVG_String *baseline_shift;
	SVG_String *dominant_baseline;
	SVG_String *glyph_orientation_horizontal;
	SVG_String *glyph_orientation_vertical;
	SVG_String *kerning;
	SVG_String *writing_mode;
*/
} SVGProperties;

/*************************************
 * Generic SVG element functions     *
 *************************************/

void gf_svg_properties_init_pointers(SVGPropertiesPointers *svg_props);
void gf_svg_properties_reset_pointers(SVGPropertiesPointers *svg_props);

void gf_svg_apply_animations(GF_Node *node, SVGPropertiesPointers *render_svg_props);
Bool gf_svg_has_appearance_flag_dirty(u32 flags);

u32 gf_node_animation_count(GF_Node *node);

Bool gf_svg_is_element_transformable(u32 tag);

void *gf_svg_create_attribute_value(u32 attribute_type);
void gf_svg_delete_attribute_value(u32 type, void *value, GF_SceneGraph *sg);
/* a == b */
Bool gf_svg_attributes_equal(GF_FieldInfo *a, GF_FieldInfo *b);
/* a = b */
GF_Err gf_svg_attributes_copy(GF_FieldInfo *a, GF_FieldInfo *b, Bool clamp);
/* c = a + b */
GF_Err gf_svg_attributes_add(GF_FieldInfo *a, GF_FieldInfo *b, GF_FieldInfo *c, Bool clamp);
Bool gf_svg_attribute_is_interpolatable(u32 type) ;
/* c = coef * a + (1 - coef) * b */
GF_Err gf_svg_attributes_interpolate(GF_FieldInfo *a, GF_FieldInfo *b, GF_FieldInfo *c, Fixed coef, Bool clamp);
/* c = alpha * a + beta * b */
GF_Err gf_svg_attributes_muladd(Fixed alpha, GF_FieldInfo *a, Fixed beta, GF_FieldInfo *b, GF_FieldInfo *c, Bool clamp);

GF_Err gf_svg_get_attribute_by_tag(GF_Node *node, u32 attribute_tag, Bool create_if_not_found, Bool set_default, GF_FieldInfo *field);

char *gf_svg_attribute_type_to_string(u32 att_type);
GF_Err gf_svg_parse_attribute(GF_Node *n, GF_FieldInfo *info, char *attribute_content, u8 anim_value_type);
void gf_svg_parse_style(GF_Node *n, char *style);

GF_Err gf_svg_dump_attribute(GF_Node *elt, GF_FieldInfo *info, char *attValue);
GF_Err gf_svg_dump_attribute_indexed(GF_Node *elt, GF_FieldInfo *info, char *attValue);

GF_Err gf_svg_store_embedded_data(XMLRI *iri, const char *cache_dir, const char *base_filename);
void gf_svg_path_build(GF_Path *path, GF_List *commands, GF_List *points);
void gf_svg_register_iri(GF_SceneGraph *sg, XMLRI *iri);
void gf_svg_unregister_iri(GF_SceneGraph *sg, XMLRI *iri);

GF_Err gf_svg_parse_element_id(GF_Node *n, const char *nodename, Bool warning_if_defined);

const char *gf_svg_get_system_paint_server_name(u32 paint_type);
u32 gf_svg_get_system_paint_server_type(const char *name);


Bool gf_smil_notify_timed_elements(GF_SceneGraph *sg);
void gf_smil_timing_insert_clock(GF_Node *elt, Bool is_end, Double clock);

void gf_svg_parse_transformlist(GF_Matrix2D *mat, char *attribute_content);

typedef struct _smil_timing_rti SMIL_Timing_RTI;

enum
{
	SMIL_TIMING_EVAL_NONE = 0,
	SMIL_TIMING_EVAL_UPDATE,
	SMIL_TIMING_EVAL_FREEZE,
	SMIL_TIMING_EVAL_REMOVE,
	SMIL_TIMING_EVAL_REPEAT,
	SMIL_TIMING_EVAL_FRACTION,
	SMIL_TIMING_EVAL_DISCARD,
	/*signaled the animation element has been inserted in the DOM tree*/
	SMIL_TIMING_EVAL_ACTIVATE,
	/*signaled the animation element has been removed from the DOM tree*/
	SMIL_TIMING_EVAL_DEACTIVATE,
};

void gf_smil_set_evaluation_callback(GF_Node *smil_time, 
									 void (*smil_evaluate)(struct _smil_timing_rti *rti, Fixed normalized_simple_time, u32 state));

void gf_smil_set_media_duration(SMIL_Timing_RTI *rti, Double media_duration);
Double gf_smil_get_media_duration(SMIL_Timing_RTI *rti);
GF_Node *gf_smil_get_element(SMIL_Timing_RTI *rti);

Bool gf_smil_timing_is_active(GF_Node *node);
void gf_smil_timing_modified(GF_Node *node, GF_FieldInfo *field);

/*******************************************************************************
 * 
 *          SVG Scene Graph for dynamic allocation of attributes	           *
 *
 *******************************************************************************/

/*SVG attributes are just DOM ones*/
typedef struct __dom_base_attribute SVGAttribute;
typedef struct __dom_full_attribute SVGExtendedAttribute;
typedef struct __dom_base_node SVG_Element;

typedef struct __xml_ev_handler SVG_handlerElement;


typedef struct _all_atts SVGAllAttributes;

void gf_svg_flatten_attributes(SVG_Element *e, SVGAllAttributes *all_atts);
const char *gf_svg_get_attribute_name(u32 tag);
u32 gf_svg_apply_inheritance(SVGAllAttributes *all_atts, SVGPropertiesPointers *render_svg_props) ;

u32 gf_svg_get_attribute_type(u32 tag);

SVGAttribute *gf_svg_create_attribute(GF_Node *node, u32 tag);
u32 gf_svg_get_attribute_tag(u32 element_tag, const char *attribute_name);
u32 gf_svg_get_element_tag(const char *element_name);

#ifdef __cplusplus
}
#endif

#endif	//_GF_SG_SVG_H_
