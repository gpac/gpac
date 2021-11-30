/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre - Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2004-2019
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

/*!
\file <gpac/scenegraph_svg.h>
\brief Scenegraph for SVG files
*/
	
/*!
\addtogroup ssvg SVG Scenegraph
\ingroup scene_grp
\brief Scenegraph for SVG files.

This section documents the Scenegraph for SVG files.

@{
 */


#include <gpac/scenegraph.h>
#include <gpac/svg_types.h>


/*******************************************************************************
 *
 *          DOM base scene graph
 *
 *******************************************************************************/
/*! DOM attributes tags for xmlspace, xmlev, xlink, SVG and LASeR*/
enum
{
	/*should never be used, this is only a parsing error*/
	TAG_DOM_ATTRIBUTE_NULL,
	/*this tag is set for a full dom attribute only - attribute name is then available*/
	TAG_DOM_ATT_any,

	TAG_XML_ATT_RANGE_FIRST,
	TAG_XML_ATT_id = TAG_XML_ATT_RANGE_FIRST,
	TAG_XML_ATT_base,
	TAG_XML_ATT_lang,
	TAG_XML_ATT_space,
	TAG_XML_ATT_RANGE_LAST,
	TAG_XLINK_ATT_RANGE_FIRST,

	TAG_XLINK_ATT_type = TAG_XLINK_ATT_RANGE_FIRST,
	TAG_XLINK_ATT_role,
	TAG_XLINK_ATT_arcrole,
	TAG_XLINK_ATT_title,
	TAG_XLINK_ATT_href,
	TAG_XLINK_ATT_show,
	TAG_XLINK_ATT_actuate,
	TAG_XLINK_ATT_RANGE_LAST,

	TAG_XMLEV_ATT_RANGE_FIRST,
	TAG_XMLEV_ATT_event,
	TAG_XMLEV_ATT_phase,
	TAG_XMLEV_ATT_propagate,
	TAG_XMLEV_ATT_defaultAction,
	TAG_XMLEV_ATT_observer,
	TAG_XMLEV_ATT_target,
	TAG_XMLEV_ATT_handler,
	TAG_XMLEV_ATT_RANGE_LAST,

	TAG_LSR_ATT_RANGE_FIRST,
	TAG_LSR_ATT_enabled,
	TAG_LSR_ATT_RANGE_LAST,
	/*these attribute types are only use for binary purpose*/
	TAG_LSR_ATT_children,
	TAG_LSR_ATT_overflow,
	TAG_LSR_ATT_rotation,
	TAG_LSR_ATT_scale,
	TAG_LSR_ATT_translation,
	TAG_LSR_ATT_svg_width,
	TAG_LSR_ATT_svg_height,
	TAG_LSR_ATT_textContent,
	TAG_LSR_ATT_text_display,

	TAG_SVG_ATT_RANGE_FIRST,

	TAG_SVG_ATT_id = TAG_SVG_ATT_RANGE_FIRST,
	TAG_SVG_ATT__class,
	TAG_SVG_ATT_requiredFeatures,
	TAG_SVG_ATT_requiredExtensions,
	TAG_SVG_ATT_requiredFormats,
	TAG_SVG_ATT_requiredFonts,
	TAG_SVG_ATT_systemLanguage,
	TAG_SVG_ATT_display,
	TAG_SVG_ATT_visibility,
	TAG_SVG_ATT_image_rendering,
	TAG_SVG_ATT_pointer_events,
	TAG_SVG_ATT_shape_rendering,
	TAG_SVG_ATT_text_rendering,
	TAG_SVG_ATT_audio_level,
	TAG_SVG_ATT_viewport_fill,
	TAG_SVG_ATT_viewport_fill_opacity,
	TAG_SVG_ATT_overflow,
	TAG_SVG_ATT_fill_opacity,
	TAG_SVG_ATT_stroke_opacity,
	TAG_SVG_ATT_fill,
	TAG_SVG_ATT_fill_rule,
	TAG_SVG_ATT_filter,
	TAG_SVG_ATT_stroke,
	TAG_SVG_ATT_stroke_dasharray,
	TAG_SVG_ATT_stroke_dashoffset,
	TAG_SVG_ATT_stroke_linecap,
	TAG_SVG_ATT_stroke_linejoin,
	TAG_SVG_ATT_stroke_miterlimit,
	TAG_SVG_ATT_stroke_width,
	TAG_SVG_ATT_color,
	TAG_SVG_ATT_color_rendering,
	TAG_SVG_ATT_vector_effect,
	TAG_SVG_ATT_solid_color,
	TAG_SVG_ATT_solid_opacity,
	TAG_SVG_ATT_display_align,
	TAG_SVG_ATT_line_increment,
	TAG_SVG_ATT_stop_color,
	TAG_SVG_ATT_stop_opacity,
	TAG_SVG_ATT_font_family,
	TAG_SVG_ATT_font_size,
	TAG_SVG_ATT_font_style,
	TAG_SVG_ATT_font_variant,
	TAG_SVG_ATT_font_weight,
	TAG_SVG_ATT_text_anchor,
	TAG_SVG_ATT_text_align,
	TAG_SVG_ATT_text_decoration,
	TAG_SVG_ATT_focusHighlight,
	TAG_SVG_ATT_externalResourcesRequired,
	TAG_SVG_ATT_focusable,
	TAG_SVG_ATT_nav_next,
	TAG_SVG_ATT_nav_prev,
	TAG_SVG_ATT_nav_up,
	TAG_SVG_ATT_nav_up_right,
	TAG_SVG_ATT_nav_right,
	TAG_SVG_ATT_nav_down_right,
	TAG_SVG_ATT_nav_down,
	TAG_SVG_ATT_nav_down_left,
	TAG_SVG_ATT_nav_left,
	TAG_SVG_ATT_nav_up_left,
	TAG_SVG_ATT_transform,
	TAG_SVG_ATT_target,
	TAG_SVG_ATT_attributeName,
	TAG_SVG_ATT_attributeType,
	TAG_SVG_ATT_begin,
	TAG_SVG_ATT_dur,
	TAG_SVG_ATT_end,
	TAG_SVG_ATT_repeatCount,
	TAG_SVG_ATT_repeatDur,
	TAG_SVG_ATT_restart,
	TAG_SVG_ATT_smil_fill,
	TAG_SVG_ATT_min,
	TAG_SVG_ATT_max,
	TAG_SVG_ATT_to,
	TAG_SVG_ATT_calcMode,
	TAG_SVG_ATT_values,
	TAG_SVG_ATT_keyTimes,
	TAG_SVG_ATT_keySplines,
	TAG_SVG_ATT_from,
	TAG_SVG_ATT_by,
	TAG_SVG_ATT_additive,
	TAG_SVG_ATT_accumulate,
	TAG_SVG_ATT_path,
	TAG_SVG_ATT_keyPoints,
	TAG_SVG_ATT_rotate,
	TAG_SVG_ATT_origin,
	TAG_SVG_ATT_transform_type,
	TAG_SVG_ATT_clipBegin,
	TAG_SVG_ATT_clipEnd,
	TAG_SVG_ATT_syncBehavior,
	TAG_SVG_ATT_syncTolerance,
	TAG_SVG_ATT_syncMaster,
	TAG_SVG_ATT_syncReference,
	TAG_SVG_ATT_x,
	TAG_SVG_ATT_y,
	TAG_SVG_ATT_width,
	TAG_SVG_ATT_height,
	TAG_SVG_ATT_preserveAspectRatio,
	TAG_SVG_ATT_initialVisibility,
	TAG_SVG_ATT_type,
	TAG_SVG_ATT_cx,
	TAG_SVG_ATT_cy,
	TAG_SVG_ATT_r,
	TAG_SVG_ATT_cursorManager_x,
	TAG_SVG_ATT_cursorManager_y,
	TAG_SVG_ATT_rx,
	TAG_SVG_ATT_ry,
	TAG_SVG_ATT_horiz_adv_x,
	TAG_SVG_ATT_horiz_origin_x,
	TAG_SVG_ATT_font_stretch,
	TAG_SVG_ATT_unicode_range,
	TAG_SVG_ATT_panose_1,
	TAG_SVG_ATT_widths,
	TAG_SVG_ATT_bbox,
	TAG_SVG_ATT_units_per_em,
	TAG_SVG_ATT_stemv,
	TAG_SVG_ATT_stemh,
	TAG_SVG_ATT_slope,
	TAG_SVG_ATT_cap_height,
	TAG_SVG_ATT_x_height,
	TAG_SVG_ATT_accent_height,
	TAG_SVG_ATT_ascent,
	TAG_SVG_ATT_descent,
	TAG_SVG_ATT_ideographic,
	TAG_SVG_ATT_alphabetic,
	TAG_SVG_ATT_mathematical,
	TAG_SVG_ATT_hanging,
	TAG_SVG_ATT_underline_position,
	TAG_SVG_ATT_underline_thickness,
	TAG_SVG_ATT_strikethrough_position,
	TAG_SVG_ATT_strikethrough_thickness,
	TAG_SVG_ATT_overline_position,
	TAG_SVG_ATT_overline_thickness,
	TAG_SVG_ATT_d,
	TAG_SVG_ATT_unicode,
	TAG_SVG_ATT_glyph_name,
	TAG_SVG_ATT_arabic_form,
	TAG_SVG_ATT_lang,
	TAG_SVG_ATT_u1,
	TAG_SVG_ATT_g1,
	TAG_SVG_ATT_u2,
	TAG_SVG_ATT_g2,
	TAG_SVG_ATT_k,
	TAG_SVG_ATT_opacity,
	TAG_SVG_ATT_x1,
	TAG_SVG_ATT_y1,
	TAG_SVG_ATT_x2,
	TAG_SVG_ATT_y2,
	TAG_SVG_ATT_filterUnits,
	TAG_SVG_ATT_gradientUnits,
	TAG_SVG_ATT_spreadMethod,
	TAG_SVG_ATT_gradientTransform,
	TAG_SVG_ATT_pathLength,
	TAG_SVG_ATT_points,
	TAG_SVG_ATT_mediaSize,
	TAG_SVG_ATT_mediaTime,
	TAG_SVG_ATT_mediaCharacterEncoding,
	TAG_SVG_ATT_mediaContentEncodings,
	TAG_SVG_ATT_bandwidth,
	TAG_SVG_ATT_fx,
	TAG_SVG_ATT_fy,
	TAG_SVG_ATT_size,
	TAG_SVG_ATT_choice,
	TAG_SVG_ATT_delta,
	TAG_SVG_ATT_offset,
	TAG_SVG_ATT_syncBehaviorDefault,
	TAG_SVG_ATT_syncToleranceDefault,
	TAG_SVG_ATT_viewBox,
	TAG_SVG_ATT_zoomAndPan,
	TAG_SVG_ATT_version,
	TAG_SVG_ATT_baseProfile,
	TAG_SVG_ATT_contentScriptType,
	TAG_SVG_ATT_snapshotTime,
	TAG_SVG_ATT_timelineBegin,
	TAG_SVG_ATT_playbackOrder,
	TAG_SVG_ATT_editable,
	TAG_SVG_ATT_text_x,
	TAG_SVG_ATT_text_y,
	TAG_SVG_ATT_text_rotate,
	TAG_SVG_ATT_transformBehavior,
	TAG_SVG_ATT_overlay,
	TAG_SVG_ATT_fullscreen,
	TAG_SVG_ATT_motionTransform,
	TAG_SVG_ATT_clip_path,

	TAG_SVG_ATT_filter_transfer_type,
	TAG_SVG_ATT_filter_table_values,
	TAG_SVG_ATT_filter_intercept,
	TAG_SVG_ATT_filter_amplitude,
	TAG_SVG_ATT_filter_exponent,


	TAG_GSVG_ATT_useAsPrimary,
	TAG_GSVG_ATT_depthOffset,
	TAG_GSVG_ATT_depthGain,
};

/*! macro for DOM base attribute*/
#define GF_DOM_BASE_ATTRIBUTE	\
	u16 tag;	/*attribute identifier*/	\
	u16 data_type; /*attribute datatype*/	  \
	void *data; /*data pointer*/				\
	struct __dom_base_attribute *next; /*next attribute*/

/*! macro for DOM full attribute*/
#define GF_DOM_FULL_ATTRIBUTE	\
	GF_DOM_ATTRIBUTE	\

/*! DOM attribute*/
typedef struct __dom_base_attribute
{
	GF_DOM_BASE_ATTRIBUTE
} GF_DOMAttribute;

/*! DOM full attribute*/
typedef struct __dom_full_attribute
{
	GF_DOM_BASE_ATTRIBUTE
	u32 xmlns;
	char *name; /*attribute name - in this case, the data field is the attribute literal value*/
} GF_DOMFullAttribute;

/*! macro for DOM base node*/
#define GF_DOM_BASE_NODE	 \
	BASE_NODE				\
	CHILDREN				\
	GF_DOMAttribute *attributes;

/*! DOM base node*/
typedef struct __dom_base_node
{
	GF_DOM_BASE_NODE
} GF_DOMNode;

/*! DOM full node*/
typedef struct __dom_full_node
{
	GF_DOM_BASE_NODE
	char *name;
	u32 ns;
} GF_DOMFullNode;

/*! DOM built-in namespaces*/
typedef enum
{
	/*XMLNS is undefined*/
	GF_XMLNS_UNDEFINED = 0,

	GF_XMLNS_XML,
	GF_XMLNS_XLINK,
	GF_XMLNS_XMLEV,
	GF_XMLNS_LASER,
	GF_XMLNS_SVG,
	GF_XMLNS_XBL,

	GF_XMLNS_SVG_GPAC_EXTENSION,

	/*any other namespace uses the CRC32 of the namespace as an identifier*/
} GF_NamespaceType;

/*! gets built-in XMLNS id for this namespace
\param name name of the namespace
\return namespace ID if known, otherwise GF_XMLNS_UNDEFINED*/
GF_NamespaceType gf_xml_get_namespace_id(char *name);
/*! adds a new namespace
\param sg the target scene graph
\param name name of the namespace (full URL)
\param qname QName of the namespace (short name in doc, eg "ev")
\return error if any
*/
GF_Err gf_sg_add_namespace(GF_SceneGraph *sg, char *name, char *qname);
/*! removes a new namespace
\param sg the target scene graph
\param name name of the namespace
\param qname QName of the namespace
\return error if any
*/
GF_Err gf_sg_remove_namespace(GF_SceneGraph *sg, char *name, char *qname);
/*! gets namespace code
\param sg the target scene graph
\param qname QName of the namespace
\return namespace code
*/
GF_NamespaceType gf_sg_get_namespace_code(GF_SceneGraph *sg, char *qname);
/*! gets namespace code from name
\param sg the target scene graph
\param name name of the namespace
\return namespace code
*/
GF_NamespaceType gf_sg_get_namespace_code_from_name(GF_SceneGraph *sg, char *name);

/*! gets namespace qname from ID
\param sg the target scene graph
\param xmlns_id ID of the namespace
\return namespace qname
*/
const char *gf_sg_get_namespace_qname(GF_SceneGraph *sg, GF_NamespaceType xmlns_id);
/*! gets namespace from ID
\param sg the target scene graph
\param xmlns_id ID of the namespace
\return namespace name
*/
const char *gf_sg_get_namespace(GF_SceneGraph *sg, GF_NamespaceType xmlns_id);
/*! push namespace parsing state
\param elt the current node being parsed
*/
void gf_xml_push_namespaces(GF_DOMNode *elt);
/*! pop namespace parsing state
\param elt the current node being parsed
*/
void gf_xml_pop_namespaces(GF_DOMNode *elt);

/*! gets namespace of an element
\param n the target node
\return namespace ID
*/
GF_NamespaceType gf_xml_get_element_namespace(GF_Node *n);


/*! DOM text node type*/
enum
{
	/*! regular text*/
	GF_DOM_TEXT_REGULAR = 0,
	/*! CDATA section*/
	GF_DOM_TEXT_CDATA,
	/*! inserted text node (typically external script)*/
	GF_DOM_TEXT_INSERTED
};

/*! DOM text node*/
typedef struct
{
	BASE_NODE
	CHILDREN
	char *textContent;
	u32 type;
} GF_DOMText;

/*! creates a new text node, assign string (does NOT duplicate it) and register node with parent if desired
\param parent the target parent node
\param text_data UTF-8 data to add as a text node
\return the new inserted DOM text node or NULL if error
*/
GF_DOMText *gf_dom_add_text_node(GF_Node *parent, char *text_data);

/*! replaces text content of node by the specified string
\param n the target DOM node
\param text the replacement string in UTF-8. If NULL, only resets the children of the node data to add as a text node
*/
void gf_dom_set_textContent(GF_Node *n, char *text);

/*! flattens text content of the node
\param n the target DOM node
\return the flattened text - shall be free'ed by the caller*/
char *gf_dom_flatten_textContent(GF_Node *n);

/*! creates a new text node - this DOES NOT register the node
\param sg the target scene graph for the node
\return the new text node*/
GF_DOMText *gf_dom_new_text_node(GF_SceneGraph *sg);

/*! DOM update (DIMS/LASeR) node*/
typedef struct
{
	BASE_NODE
	CHILDREN
	char *data;
	u32 data_size;
	GF_List *updates;
} GF_DOMUpdates;

/*! creates a new updates node and register node with parent
\param parent the target parent node
\return a new DOM updates node
*/
GF_DOMUpdates *gf_dom_add_updates_node(GF_Node *parent);

/*! DOM Event phases*/
typedef enum
{
	GF_DOM_EVENT_PHASE_CAPTURE = 1,
	GF_DOM_EVENT_PHASE_AT_TARGET = 2,
	GF_DOM_EVENT_PHASE_BUBBLE = 3,

	GF_DOM_EVENT_CANCEL_MASK = 0xE0,
	/*special phase indicating the event has been canceled*/
	GF_DOM_EVENT_PHASE_CANCEL = 1<<5,
	/*special phase indicating the event has been canceled immediately*/
	GF_DOM_EVENT_PHASE_CANCEL_ALL = 1<<6,
	/*special phase indicating the default action of the event is prevented*/
	GF_DOM_EVENT_PHASE_PREVENT = 1<<7,
} GF_DOMEventPhase;

/*! DOM Event possible targets*/
typedef enum
{
	GF_DOM_EVENT_TARGET_NODE,
	GF_DOM_EVENT_TARGET_DOCUMENT,
	GF_DOM_EVENT_TARGET_MSE_MEDIASOURCE,
	GF_DOM_EVENT_TARGET_MSE_SOURCEBUFFERLIST,
	GF_DOM_EVENT_TARGET_MSE_SOURCEBUFFER,
	GF_DOM_EVENT_TARGET_XHR,
} GF_DOMEventTargetType;


/*! DOM EventTarget Interface*/
typedef struct
{
	/*! list of SVG Listener nodes attached to this Event Target*/
	GF_List *listeners;
	/*! pointer to the object implementing the DOM Event Target Interface*/
	void *ptr;
	/*! type of the object implementing the DOM Event Target Interface*/
	GF_DOMEventTargetType ptr_type;
} GF_DOMEventTarget;

/*! creates a new event target
\param type the type of the event target
\param obj opaque data passed to the event target
\return a new DOM event target
*/
GF_DOMEventTarget *gf_dom_event_target_new(GF_DOMEventTargetType type, void *obj);
/*! associates a listener node and a event target node
   - adds the listener node in the list of event listener nodes for the target node
   - sets the target node as the user of the listener
\param listener the target listener
\param evt_target the event target
\return error if any
*/
GF_Err gf_sg_listener_associate(GF_Node *listener, GF_DOMEventTarget *evt_target);

/*! DOM Event media information*/
typedef struct
{
	Bool bufferValid;
	u32 level;
	Fixed remaining_time;
	u16 status;
	const char *session_name;
	u64 loaded_size, total_size;
} GF_DOMMediaEvent;

/*! DOM Event structure*/
typedef struct
{
	/*event type, as defined in <gpac/events.h>*/
	GF_EventType type;
	/*event phase type, READ-ONLY
	0: at target, 1: bubbling, 2: capturing , 3: canceled
	*/
	u8 event_phase;
	u8 bubbles;
	u8 cancelable;
	/*output only - indicates UI events (mouse) have been detected*/
	u8 has_ui_events;

	/*we don't use a GF_DOMEventTarget here since the structure is only created when events are attached */
	void *target;
	GF_DOMEventTargetType target_type;

	GF_DOMEventTarget *currentTarget;
	Double timestamp;
	/*UIEvent extension.
		For mouse extensions: number of clicks
		For key event: the key code
		For SMIL event: number of iteration (repeat)
	*/
	u32 detail;

	/*MouseEvent extension*/
	s32 screenX, screenY;
	s32 clientX, clientY;
	u32 button;
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

	/*DOM event used in VRML (GPAC's internal)*/
	Bool is_vrml;
	/*media event*/
	GF_DOMMediaEvent media_event;

	/*number of listeners triggered by the event*/
	u32 consumed;

	/*for GF_EVENT_ATTR_MODIFIED*/
	GF_FieldInfo *attr;
	GF_Err error_state;

	/* ADDON_DETECTED event*/
	const char *addon_url;
} GF_DOM_Event;

/*! fires an event on the specified node
\warning event execution may very well destroy ANY node, especially the event target node !!
\param node the target node
\param event the DOM event
\return GF_TRUE if event is consumed/aborted after this call, GF_FALSE otherwise (event now pending)
*/
Bool gf_dom_event_fire(GF_Node *node, GF_DOM_Event *event);
/*! fires a DOM event on the specified node
\warning event execution may very well destroy ANY node, especially the event target node !!
\param et the event target
\param event the dom event
\param sg the parent graph
\param n the event current target, can be NULL
\return GF_TRUE if event is consumed/aborted after this call, GF_FALSE otherwise (event now pending)
*/
Bool gf_sg_fire_dom_event(GF_DOMEventTarget *et, GF_DOM_Event *event, GF_SceneGraph *sg, GF_Node *n);

/*! fires event on the specified node
\warning event execution may very well destroy ANY node, especially the event target node !!
\param node the target node
\param event the DOM event
\param use_stack a list of parent node/use node pairs for bubbling phase - may be NULL
\return GF_TRUE if event is consumed/aborted after this call, GF_FALSE otherwise (event now pending)
*/
Bool gf_dom_event_fire_ex(GF_Node *node, GF_DOM_Event *event, GF_List *use_stack);

/*! gets event type by name
\param name the event name
\return the event type*/
GF_EventType gf_dom_event_type_by_name(const char *name);
/*! gets event name by type
\param type the event type
\return the event name*/
const char *gf_dom_event_get_name(GF_EventType type);

/*! gets key name by type
\param key_identifier the key type
\return the key name*/
const char *gf_dom_get_key_name(GF_KeyCode key_identifier);
/*! gets key type by name
\param key_name the key name
\return the key type*/
GF_KeyCode gf_dom_get_key_type(char *key_name);


/*! macro for DOM listener
DOM listener is simply a node added to the node events list.
Only one observer can be attached to a listener. The listener will remove itself from the observer
event list when destructed.*/
#define GF_DOM_BASE_LISTENER 	\
	/* JavaScript context in which the listener is applicable */ \
	struct js_handler_context *js_data;\
	/* text content of the callback */ \
	char *callback; \
	/* parent timed elt */ \
	GF_Node *timed_elt;

/*! DOM Event handler*/
typedef struct __xml_ev_handler
{
	GF_DOM_BASE_NODE
	/*! handler callback function*/
	void (*handle_event)(GF_Node *hdl, GF_DOM_Event *event, GF_Node *observer);
	GF_DOM_BASE_LISTENER
} GF_DOMHandler;

/*! DOM Event category*/
typedef enum
{
	GF_DOM_EVENT_UNKNOWN_CATEGORY,
	/*basic DOM events*/
	GF_DOM_EVENT_DOM = 1,
	/*DOM mutation events*/
	GF_DOM_EVENT_MUTATION = 1<<1,
	/*DOM mouse events*/
	GF_DOM_EVENT_MOUSE = 1<<2,
	/*DOM focus events*/
	GF_DOM_EVENT_FOCUS = 1<<3,
	/*DOM key events*/
	GF_DOM_EVENT_KEY = 1<<4,
	/*DOM/SVG/HTML UI events (resize, scroll, ...)*/
	GF_DOM_EVENT_UI = 1<<5,
	/*text events*/
	GF_DOM_EVENT_TEXT = 1<<6,
	/*SVG events*/
	GF_DOM_EVENT_SVG = 1<<7,
	/*SMIL events*/
	GF_DOM_EVENT_SMIL = 1<<8,
	/*LASeR events*/
	GF_DOM_EVENT_LASER = 1<<9,
	/*HTML Media events*/
	GF_DOM_EVENT_MEDIA = 1<<10,
	/*HTML Media Source events*/
	GF_DOM_EVENT_MEDIASOURCE = 1<<11,

	/*Internal GPAC events*/
	GF_DOM_EVENT_GPAC = 1<<30,
	/*fake events - these events are NEVER fired*/
	GF_DOM_EVENT_FAKE = 0x80000000 //1<<31
} GF_DOMEventCategory;

/*! gets category of DOM event
\param type type of event
\return event category
*/
GF_DOMEventCategory gf_dom_event_get_category(GF_EventType type);

/*! registers an event category with the scene graph. Event with unregistered categories will not be fired
\param sg the target scene graph
\param category the DOM event category to add
*/
void gf_sg_register_event_type(GF_SceneGraph *sg, GF_DOMEventCategory category);
/*! unregisters an event category with the scene graph
\param sg the target scene graph
\param category the DOM event category to add
*/
void gf_sg_unregister_event_type(GF_SceneGraph *sg, GF_DOMEventCategory category);

/*! adds a listener to the node.
\param n the target node
\param listener a listenerElement (XML event). The listener node is NOT registered with the node (it may very well not be a direct child of the node)
\return error if any
*/
GF_Err gf_node_dom_listener_add(GF_Node *n, GF_Node *listener);
/*! gets number of listener of a node
\param n the target node
\return number of listeners
*/
u32 gf_dom_listener_count(GF_Node *n);
/*! gets a listener of a node
\param n the target node
\param idx 0-based index of the listener to query
\return listener node or NULL if error
*/
GF_Node *gf_dom_listener_get(GF_Node *n, u32 idx);

/*! creates a default listener/handler for the given event on the given node
Listener/handler are stored at the node level
\param observer the observer node
\param event_type the event type
\param event_param the event parameter
\return the created handler element to allow for handler function override
*/
GF_DOMHandler *gf_dom_listener_build(GF_Node *observer, GF_EventType event_type, u32 event_param);

/*! registers a XML IRI with the scene graph - this is needed to handle replacement of IRI (SMIL anim, DOM / LASeR updates)
\warning \ref gf_node_unregister_iri shall be called when the parent node of the IRI is destroyed
\param sg the target scene graph
\param iri the IRI to register
*/
void gf_node_register_iri(GF_SceneGraph *sg, XMLRI *iri);
/*! unregisters a XML IRI with the scene graph
\param sg the target scene graph
\param iri the IRI to unregister
*/
void gf_node_unregister_iri(GF_SceneGraph *sg, XMLRI *iri);
/*! gets number of animation targeting this node
\param n the target node
\return the number of animations
*/
u32 gf_node_animation_count(GF_Node *n);

/*! writes data embedded in IRI (such as base64 data) to the indicated cahce directory, and updates the IRI accordingly
\param iri the IRI to store
\param cache_dir location of the directory to which the data should be extracted
\param base_filename location of the file the IRI was declared in
\return error if any
*/
GF_Err gf_node_store_embedded_data(XMLRI *iri, const char *cache_dir, const char *base_filename);


/**************************************************
 *  SVG's styling properties (see 6.1 in REC 1.1) *
 *************************************************/

/*! SVG properties of node*/
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
	Fixed						computed_audio_level;

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


/*! initializes an SVGPropertiesPointers
\param svg_props pointer to structure to initialize
*/
void gf_svg_properties_init_pointers(SVGPropertiesPointers *svg_props);
/*! resets an SVGPropertiesPointers
\param svg_props pointer to structure to reset
*/
void gf_svg_properties_reset_pointers(SVGPropertiesPointers *svg_props);

/*! applies animations for the node
\param n the target node
\param render_svg_props pointer to rendering SVG properties of the node
*/
void gf_svg_apply_animations(GF_Node *n, SVGPropertiesPointers *render_svg_props);
/*! check if appearance flag is set
\param flags flags to test
\return GF_TRUE if appearance dirty flag is set
*/
Bool gf_svg_has_appearance_flag_dirty(u32 flags);

/*! checks if node tag indicates a transformable elemnt
\param tag tag to check
\return GF_TRUE if element is transformable
*/
Bool gf_svg_is_element_transformable(u32 tag);
/*! creates an SVG attribute value for the given type
\param attribute_type type of attribute to delete
\return newly allocated attribute value*/
void *gf_svg_create_attribute_value(u32 attribute_type);
/*! destroys an SVG attribute value
\param attribute_type type of attribute to delete
\param value the value to destroy
\param sg the parent scenegraph (needed for IRI registration)
*/
void gf_svg_delete_attribute_value(u32 attribute_type, void *value, GF_SceneGraph *sg);

/*! checks if two SVG attributes are equal
\param a first attribute
\param b second attribute
\return GF_TRUE if equal
*/
Bool gf_svg_attributes_equal(GF_FieldInfo *a, GF_FieldInfo *b);
/*! copies attributes (a = b)
\param a destination attribute
\param b source attribute
\param clamp if GF_TRUE, the interpolated value is clamped to its min/max possible values
\return error if any
*/
GF_Err gf_svg_attributes_copy(GF_FieldInfo *a, GF_FieldInfo *b, Bool clamp);
/*! adds attributes ( c = a + b)
\param a first attribute
\param b second attribute
\param c destination attribute
\param clamp if GF_TRUE, the interpolated value is clamped to its min/max possible values
\return error if any
 */
GF_Err gf_svg_attributes_add(GF_FieldInfo *a, GF_FieldInfo *b, GF_FieldInfo *c, Bool clamp);
/*! checks if attribute type can be interpolated
\param type attribute type
\return GF_TRUE if type can be interpolation*/
Bool gf_svg_attribute_is_interpolatable(u32 type);
/*! interpolates attributes ( c = coef * a + (1 - coef) * b )
\param a first attribute
\param b second attribute
\param c destination attribute
\param coef interpolation coefficient
\param clamp if GF_TRUE, the interpolated value is clamped to its min/max possible values
\return error if any
*/
GF_Err gf_svg_attributes_interpolate(GF_FieldInfo *a, GF_FieldInfo *b, GF_FieldInfo *c, Fixed coef, Bool clamp);
/*! multiply and add attributes (c = alpha * a + beta * b)
\param a first attribute
\param alpha first multiplier
\param b second attribute
\param beta second multiplier
\param c destination attribute
\param clamp if GF_TRUE, the interpolated value is clamped to its min/max possible values
\return error if any
*/
GF_Err gf_svg_attributes_muladd(Fixed alpha, GF_FieldInfo *a, Fixed beta, GF_FieldInfo *b, GF_FieldInfo *c, Bool clamp);
/*! gets an attribute by its tag (built-in name)
\param n the target node
\param attribute_tag the attribute tag
\param create_if_not_found if GF_TRUE, adds the attribute if not found
\param set_default if GF_TRUE, sets the attribute to default when creating it
\param field set to the attribute information
\return error if any
*/
GF_Err gf_node_get_attribute_by_tag(GF_Node *n, u32 attribute_tag, Bool create_if_not_found, Bool set_default, GF_FieldInfo *field);

/*! gets name of an attribute type
\param att_type the attribute type
\return the attribute name*/
const char *gf_svg_attribute_type_to_string(u32 att_type);
/*! parses an attribute
\param n the target node
\param info the attribute information
\param attribute_content the UTF-8 string to parse
\param anim_value_type the SMIL animation value type for this attribute, 0 if not animatable
\return error if any
*/
GF_Err gf_svg_parse_attribute(GF_Node *n, GF_FieldInfo *info, char *attribute_content, u8 anim_value_type);
/*! parses an SVG style
\param n the target node
\param style the UTF-8 style string to parse
*/
void gf_svg_parse_style(GF_Node *n, char *style);

/*! dumps an SVG attribute
\param n the target node
\param info the attribute information
\return the textural representation of the value - shall be destroyed by caller
*/
char *gf_svg_dump_attribute(GF_Node *n, GF_FieldInfo *info);
/*! dumps an SVG indexed attribute (special 1-D version of a N dimensional attribute, used by LASeR and DOM updates)
\param n the target node
\param info the attribute information
\return the textural representation of the value - shall be destroyed by caller
*/
char *gf_svg_dump_attribute_indexed(GF_Node *n, GF_FieldInfo *info);

#if USE_GF_PATH
/*! builds a path from its SVG representation
\param path a 2D graphical path object
\param commands a list of SVG path commands
\param points a list of SVG path points
*/
void gf_svg_path_build(GF_Path *path, GF_List *commands, GF_List *points);
#endif
/*! parses an SVG element ID
\param n the target node
\param nodename the node name to parse
\param warning_if_defined if GF_TRUE, throws a warning when the node name is already defined
\return error if any
*/
GF_Err gf_svg_parse_element_id(GF_Node *n, const char *nodename, Bool warning_if_defined);

/*! gets the name of a paint server type
\param paint_type the paint server type
\return paint server name*/
const char *gf_svg_get_system_paint_server_name(u32 paint_type);
/*! gets the type of a paint server by name
\param name the paint server name
\return paint server type*/
u32 gf_svg_get_system_paint_server_type(const char *name);

/*! notifies the scene time to all the timed elements in the given scene graph (including sub-scenes).
\param sg the target scene graph
\return the number of active timed elements, or 0 if no changes in SMIL timing nodes (no redraw/reevaluate needed)
*/
Bool gf_smil_notify_timed_elements(GF_SceneGraph *sg);
/*! inserts a new resolved time instant in the begin or end attribute.
   The insertion preserves the sorting and removes the previous insertions which have become obsolete
\warning Only used for inserting time when an <a> element, whose target is a timed element, is activated
\param n the target element
\param is_end if GF_TRUE, the SMIL clock is an end clock
\param clock clock time in seconds
 */
void gf_smil_timing_insert_clock(GF_Node *n, Bool is_end, Double clock);
/*! parses a transformation list into a 2D matrix
\param mat the matrix to fill
\param attribute_content the UTF8 string representin the SVG transformation
\return GF_TRUE if success*/
Bool gf_svg_parse_transformlist(GF_Matrix2D *mat, char *attribute_content);

/*! SMIL runtime timing information*/
typedef struct _smil_timing_rti SMIL_Timing_RTI;

/*! SMIL timing evaluation state*/
typedef enum
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
} GF_SGSMILTimingEvalState;

/*! SMIL timing evaluation callback
\param rti SMIL runtime info
\param normalized_simple_time SMIL normalized time
\param state SMIL evaluation state
*/
typedef void gf_sg_smil_evaluate(struct _smil_timing_rti *rti, Fixed normalized_simple_time, GF_SGSMILTimingEvalState state);
/*! sets the SMIL evaluation callback for a node
\param smil_time the target SMIL node
\param smil_evaluate the callback function
*/
void gf_smil_set_evaluation_callback(GF_Node *smil_time, gf_sg_smil_evaluate smil_evaluate);
/*! assigns media duration for a SMIL node
\param rti the target SMIL runtime info of the node
\param media_duration the media duration in seconds*/
void gf_smil_set_media_duration(SMIL_Timing_RTI *rti, Double media_duration);
/*! gets media duration of a SMIL node
\param rti the target SMIL runtime info of the node
\return the media duration in seconds*/
Double gf_smil_get_media_duration(SMIL_Timing_RTI *rti);
/*! gets node from a SMIL runtime info
\param rti the target SMIL runtime info of the node
\return the associated node*/
GF_Node *gf_smil_get_element(SMIL_Timing_RTI *rti);
/*! checks if a SMIL node is active
\param node the target node
\return GF_TRUE if node is active*/
Bool gf_smil_timing_is_active(GF_Node *node);
/*! notifies a SMIL node that it has been modified
\param node the target SMIL node
\param field the field information describing the modified attribute, may be NULL
*/
void gf_smil_timing_modified(GF_Node *node, GF_FieldInfo *field);

/*******************************************************************************
 *
 *          SVG Scene Graph for dynamic allocation of attributes	           *
 *
 *******************************************************************************/

/*SVG attributes are just DOM ones*/
/*! SVG attribute - same as DOM attribute*/
typedef struct __dom_base_attribute SVGAttribute;
/*! SVG extended attribute - same as DOM extended attribute*/
typedef struct __dom_full_attribute SVGExtendedAttribute;
/*! SVG node - same as DOM node*/
typedef struct __dom_base_node SVG_Element;
/*! SVG handler node - same as DOM Event handler*/
typedef struct __xml_ev_handler SVG_handlerElement;
/*! SVG all attributes*/
typedef struct _all_atts SVGAllAttributes;

/*! flattens all SVG attributes in a structure
\param n the target SVG node
\param all_atts filled with all SVG attributes defined fo node
*/
void gf_svg_flatten_attributes(SVG_Element *n, SVGAllAttributes *all_atts);
/*! gets SVG attribute name for a node
\param n the target SVG node
\param tag the attribute tag
\return the attribute name
*/
const char *gf_svg_get_attribute_name(GF_Node *n, u32 tag);
/*! applies inheritance of SVG attributes to a render_svg_props
\note Some properties (audio-level, display, opacity, solid*, stop*, vector-effect, viewport*) are inherited only when they are specified with the value 'inherit' otherwise they default to their initial value which for the function below means NULL, the compositor will take care of the rest

\param all_atts the flatten SVG attributes of the node
\param render_svg_props the set of properties applying to the parent node (initialize this one to 0 for root element)
\return the set of inherited flags (from node dirty flags)
*/
u32 gf_svg_apply_inheritance(SVGAllAttributes *all_atts, SVGPropertiesPointers *render_svg_props) ;

/*! creates a DOM Attribute for the given tag
\warning the attribute is not added to the node's attributes
\param n the target node
\param tag the target built-in tag
\return a new DOM Attribute
*/
GF_DOMAttribute *gf_xml_create_attribute(GF_Node *n, u32 tag);
/*! gets the type of a built-in XML/SVG/SMIL attribute tag
\param tag the target attribute tag
\return the attribute type*/
u32 gf_xml_get_attribute_type(u32 tag);

/*! gets the built-in tag of a node's attribute
\param n the target node
\param attribute_name the target attribute name
\param ns the target namespace
\return the attribute tag*/
u32 gf_xml_get_attribute_tag(GF_Node *n, char *attribute_name, GF_NamespaceType ns);

/*! gets the built-in tag of a node
\param node_name the target node name
\param xmlns the target namespace
\return the node tag*/
u32 gf_xml_get_element_tag(const char *node_name, u32 xmlns);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif	//_GF_SG_SVG_H_
