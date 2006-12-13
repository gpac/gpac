/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Graph sub-project
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



#ifndef _GF_SCENEGRAPH_DEV_H_
#define _GF_SCENEGRAPH_DEV_H_

//#define GF_NODE_USE_POINTERS

/*for vrml base types, ROUTEs and PROTOs*/
#include <gpac/scenegraph_vrml.h>

#include <gpac/scenegraph_svg.h>


//#define GF_CYCLIC_RENDER_ON

//#define GF_ARRAY_PARENT_NODES

void gf_node_setup(GF_Node *p, u32 tag);

typedef struct _node_list
{
	struct _node_list *next;
	GF_Node *node;
} GF_NodeList;


typedef struct _nodepriv
{
	/*node type*/
	u32 tag;
	/*node ID or 0*/
	u32 NodeID;
	u16 is_dirty;

#ifdef GF_NODE_USE_POINTERS
	const char *name;
	u32 (*get_field_count)(struct _sfNode *node, u8 IndexMode);
	void (*node_del)(struct _sfNode *node);
	GF_Err (*get_field) (struct _sfNode *node, GF_FieldInfo *info);
#endif

	/*node def name (MPEGJ interfaces, VRML/X3D)*/
	char *NodeName;
	/*scenegraph holding the node*/
	struct __tag_scene_graph *scenegraph;

	/*user defined rendering function */
	void (*RenderNode)(struct _sfNode *node, void *render_stack);
	/*user defined pre-destroy function */
	void (*PreDestroyNode)(struct _sfNode *node);
	/*user defined stack*/
	void *privateStack;

	/*
		DEF/USE (implicit or explicit) handling - implicit DEF do NOT use the parentNodes list
		
		NOTE on DEF/USE: in VRML a node is DEF and then USE, but in MPEG4 a node can be deleted, replaced,
		a USE can be inserted before (in scene graph depth) the DEF, etc.
		this library considers that a DEF node is valid until all instances are deleted. If so the node is removed
		from the scene graph manager
	*/

	/*number of instances of this node in the graph*/
	u32 num_instances;
	/*list of all parent nodes (whether DEF or not, needed to invalidate parent tree)*/
#ifdef GF_ARRAY_PARENT_NODES
	GF_List *parentNodes;
#else
	GF_NodeList *parents;
#endif
	/*routes on eventOut, ISed routes, ... for VRML-based scene graphs
	event listeners for all others
	THIS IS DYNAMICALLY CREATED*/
	GF_List *events;

	/* SVG animations are registered in the target node */
	GF_List *animations;

#ifdef GF_CYCLIC_RENDER_ON
	u32 render_pass;
#endif
} NodePriv;


#ifndef NODEREG_STEP_ALLOC
#define NODEREG_STEP_ALLOC	50
#endif

struct __tag_scene_graph 
{
	/*all DEF nodes (explicit)*/
	GF_Node **node_registry;
	u32 node_reg_alloc, node_reg_size;

	/*all routes available*/
	GF_List *Routes;

	/*when a proto is instanciated it creates its own scene graph. BIFS/VRML specify that the namespace is the same 
	(eg cannot reuse a NodeID or route name/ID), but this could be done differently by some other stds
	if NULL this is the main scenegraph*/
	struct _proto_instance *pOwningProto;

	/*all first-level protos of the graph (the only ones that can be instanciated in this graph)*/
	GF_List *protos;
	/*all first-level protos of the graph not currently registered - memory handling of graph only*/
	GF_List *unregistered_protos;

	/*pointer to the root node*/
	GF_Node *RootNode;

	/*routes to be activated (cascade model). This is used at the top-level graph only (eg
	proto routes use that too, ecept ISed fields). It is the app responsability to 
	correctly connect or browse scene graphs connected through Inline*/
	GF_List *routes_to_activate;

	/*since events may trigger deletion of objects we use a 2 step delete*/
	GF_List *routes_to_destroy;

	u32 simulation_tick;

	/*user private data*/
	void *userpriv;

	/*callback routines*/
	/*node callback*/
	void (*NodeCallback)(void *user_priv, u32 type, GF_Node *node, void *ctxdata);
	/*real scene time callback*/
	Double (*GetSceneTime)(void *userpriv);

	GF_SceneGraph *(*GetExternProtoLib)(void *userpriv, MFURL *lib_url);

	/*parent scene if any*/
	struct __tag_scene_graph *parent_scene;

	/*size info and pixel metrics - this is not used internally, however it helps when rendering
	and decoding modules don't know each-other (as in MPEG4)*/
	u32 width, height;
	Bool usePixelMetrics;

	/*application interface for javascript*/
	GF_JSInterface *js_ifce;
	/*script loader*/
	void (*script_load)(GF_Node *node);

	u32 max_defined_route_id;

#ifdef GF_CYCLIC_RENDER_ON
	/*max number of render() for cyclic graphs*/
	u32 max_cyclic_render;
#endif

#ifndef GPAC_DISABLE_SVG
	GF_List *xlink_hrefs;
	GF_List *smil_timed_elements;
	Bool update_smil_timing;
#ifdef GPAC_HAS_SPIDERMONKEY
	struct __tag_svg_script_ctx *svg_js;
#endif
#endif
};

void gf_sg_parent_setup(GF_Node *pNode);
void gf_sg_parent_reset(GF_Node *pNode);

struct _route
{
	u8 is_setup;
	/*set to true for proto IS fields*/
	u8 IS_route;

	u32 ID;
	char *name;

	GF_Node *FromNode;
	GF_FieldInfo FromField;

	GF_Node *ToNode;
	GF_FieldInfo ToField;

	/*scope of this route*/
	GF_SceneGraph *graph;
	u32 lastActivateTime;
};

void gf_sg_route_unqueue(GF_SceneGraph *sg, GF_Route *r);
/*returns TRUE if route modified destination node*/
Bool gf_sg_route_activate(GF_Route *r);
void gf_sg_route_queue(GF_SceneGraph *pSG, GF_Route *r);

void gf_sg_destroy_routes(GF_SceneGraph *sg);


/*MPEG4 def*/
GF_Node *gf_sg_mpeg4_node_new(u32 NodeTag);
u32 gf_sg_mpeg4_node_get_child_ndt(GF_Node *node);
GF_Err gf_sg_mpeg4_node_get_field_index(GF_Node *node, u32 inField, u8 code_mode, u32 *fieldIndex);
#ifndef GF_NODE_USE_POINTERS
GF_Err gf_sg_mpeg4_node_get_field(GF_Node *node, GF_FieldInfo *field);
u32 gf_sg_mpeg4_node_get_field_count(GF_Node *node, u8 code_mode);
void gf_sg_mpeg4_node_del(GF_Node *node);
const char *gf_sg_mpeg4_node_get_class_name(u32 NodeTag);
#endif
Bool gf_sg_mpeg4_node_get_aq_info(GF_Node *node, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits);
s32 gf_sg_mpeg4_node_get_field_index_by_name(GF_Node *node, char *name);

/*X3D def*/
GF_Node *gf_sg_x3d_node_new(u32 NodeTag);
#ifndef GF_NODE_USE_POINTERS
GF_Err gf_sg_x3d_node_get_field(GF_Node *node, GF_FieldInfo *field);
u32 gf_sg_x3d_node_get_field_count(GF_Node *node);
void gf_sg_x3d_node_del(GF_Node *node);
const char *gf_sg_x3d_node_get_class_name(u32 NodeTag);
#endif
s32 gf_sg_x3d_node_get_field_index_by_name(GF_Node *node, char *name);

void gf_sg_mfint32_del(MFInt32 par);
void gf_sg_mffloat_del(MFFloat par);
void gf_sg_mfdouble_del(MFDouble par);
void gf_sg_mfbool_del(MFBool par);
void gf_sg_mfcolor_del(MFColor par);
void gf_sg_mfcolor_rgba_del(MFColorRGBA par);
void gf_sg_mfrotation_del(MFRotation par);
void gf_sg_mfstring_del(MFString par);
void gf_sg_mftime_del(MFTime par);
void gf_sg_mfvec2f_del(MFVec2f par);
void gf_sg_mfvec3f_del(MFVec3f par);
void gf_sg_mfvec4f_del(MFVec4f par);
void gf_sg_mfvec2d_del(MFVec2d par);
void gf_sg_mfvec3d_del(MFVec3d par);
void gf_sg_mfurl_del(MFURL url);
void gf_sg_sfimage_del(SFImage im);
void gf_sg_sfstring_del(SFString par);
void gf_sg_mfscript_del(MFScript sc);
void gf_sg_sfcommand_del(SFCommandBuffer cb);
void gf_sg_sfurl_del(SFURL url);

Bool gf_sg_vrml_node_init(GF_Node *node);
Bool gf_sg_vrml_node_changed(GF_Node *node, GF_FieldInfo *field);



#ifndef GPAC_DISABLE_SVG


SVGElement *gf_svg_create_node(u32 ElementTag);
void gf_svg_element_del(SVGElement *elt);
void gf_svg_reset_base_element(SVGElement *p);
Bool gf_sg_svg_node_init(GF_Node *node);


void gf_svg_init_conditional(SVGElement *p);
void gf_svg_init_anim(SVGElement *p);
void gf_svg_init_sync(SVGElement *p);
void gf_svg_init_timing(SVGElement *p);
void gf_svg_init_xlink(SVGElement *p);
void gf_svg_init_focus(SVGElement *p);
void gf_svg_init_properties(SVGElement *p);
void gf_svg_init_core(SVGElement *p);

Bool gf_sg_svg_node_changed(GF_Node *node, GF_FieldInfo *field);
void gf_smil_timing_modified(GF_Node *node, GF_FieldInfo *field);
Bool gf_smil_timing_is_active(GF_Node *node);

GF_Err gf_svg_get_attribute_info(GF_Node *node, GF_FieldInfo *info);
u32 gf_svg_get_attribute_count(GF_Node *);
const char *gf_svg_get_element_name(u32 tag);

/* animations */
GF_Err gf_node_animation_add(GF_Node *node, void *animation);
GF_Err gf_node_animation_del(GF_Node *node);
u32 gf_node_animation_count(GF_Node *node);
void *gf_node_animation_get(GF_Node *node, u32 i);

Bool gf_svg_is_inherit(GF_FieldInfo *a);
Bool gf_svg_is_current_color(GF_FieldInfo *a);
void *gf_svg_get_property_pointer(SVGPropertiesPointers *rendering_property_context, 
								  SVGProperties *elt_property_context, 
								  void *input_attribute);
void gf_svg_attributes_copy_computed_value(GF_FieldInfo *out, GF_FieldInfo *in, SVGElement*elt, void *orig_dom_ptr, SVGPropertiesPointers *inherited_props);


/* reset functions for SVG types */
void gf_svg_reset_path(SVG_PathData path);
void gf_svg_reset_iri(GF_SceneGraph *sg, SVG_IRI*iri);
/* delete functions for SVG types */
void gf_svg_delete_paint		(SVG_Paint *paint);
void gf_smil_delete_times		(GF_List *l);
void gf_svg_delete_points		(GF_List *l);
void gf_svg_delete_coordinates	(GF_List *l);
/*for keyTimes, keyPoints and keySplines*/
void gf_smil_delete_key_types	(GF_List *l);


/* SMIL Timing structures */
/* status of an SMIL timed element */ 
enum {
	SMIL_STATUS_WAITING_TO_BEGIN = 0,
	SMIL_STATUS_ACTIVE,
	SMIL_STATUS_POST_ACTIVE,
	SMIL_STATUS_FROZEN,
	SMIL_STATUS_DONE
};

typedef struct {
	u32 activation_cycle;
	u32 nb_iterations;

	/* negative values mean indefinite */
	Double begin, 
		   end,
		   simple_duration, 
		   active_duration;

} SMIL_Interval;

enum
{
	SMIL_TIMING_EVAL_NONE = 0,
	SMIL_TIMING_EVAL_UPDATE,
	SMIL_TIMING_EVAL_FREEZE,
	SMIL_TIMING_EVAL_REMOVE,
	SMIL_TIMING_EVAL_REPEAT,
	SMIL_TIMING_EVAL_FRACTION,
	SMIL_TIMING_EVAL_DISCARD,
};

typedef struct _smil_timing_rti
{
	SVGElement *timed_elt;
	Double scene_time;

	/* SMIL element life-cycle status */
	u8 status;
	u32 cycle_number;
	u32 first_frozen;

	/* List of possible intervals for activation of the element */
	GF_List *intervals;
	s32	current_interval_index;
	SMIL_Interval *current_interval;

	/* evaluation of timing attributes and activation of the timed element may be postponed in some cases
	   for instance, animation elements are activated when traversing the tree, but audio elements are not traversed.*/
	Bool postpone;

	void (*evaluate)(struct _smil_timing_rti *rti, Fixed normalized_simple_time, u32 state);
	u32 evaluate_status;

#if 0
	/* is called only when the timed element is active */
	void (*activation)(struct _smil_timing_rti *rti, Fixed normalized_simple_time);

	/* is called (possibly many times) when the timed element is frozen */
	void (*freeze)(struct _smil_timing_rti *rti, Fixed normalized_simple_time);

	/* is called (only once) when the timed element is restored */
	void (*restore)(struct _smil_timing_rti *rti, Fixed normalized_simple_time);

	/* is called only when the timed element is inactive and receives a fraction event, the second parameter is ignored */
	void (*fraction_activation)(struct _smil_timing_rti *rti, Fixed normalized_simple_time);
#endif
	/* simulated normalized simple time */
	Fixed fraction;

} SMIL_Timing_RTI;

void gf_smil_timing_init_runtime_info(SVGElement *timed_elt);
void gf_smil_timing_delete_runtime_info(SVGElement *timed_elt);
Fixed gf_smil_timing_get_normalized_simple_time(SMIL_Timing_RTI *rti, Double scene_time);
/*returns 1 if an animation changed a value in the rendering tree */
s32 gf_smil_timing_notify_time(SMIL_Timing_RTI *rti, Double scene_time);

/* SMIL Animation Structures */
/* This structure is used per animated attribute,
   it contains:
    - all the animations applying to the same attribute,
    - the specified value before any inheritance has been applied nor any animation started 
	    (as specified in the SVG document),
    - the presentation value passed from one animation to the next one, at the same level in the tree
	- a pointer to presentation value passed from the previous level in the tree
	- a pointer to the value of the color property (for handling of 'currentColor'), from previous level in the tree
	- the location of the attribute in the elt structure when it was created 
	   (used for fast comparison of SVG properties when animating from/to/by/values/... inherited values)
*/
typedef struct {
	GF_List *anims;
	GF_FieldInfo specified_value;
	GF_FieldInfo presentation_value;
	GF_FieldInfo parent_presentation_value;
	GF_FieldInfo current_color_value;
	void *orig_dom_ptr;
	/* flag set by any animation to inform other animations that there base value has changed */
	Bool presentation_value_changed;
} SMIL_AttributeAnimations;

/* This structure is per animation element, 
   it holds the result of the animation and 
   some info to make animation computation faster */
typedef struct {
	SMIL_AttributeAnimations *owner;

	/* animation element */
	SVGElement *anim_elt;

	/* in case of animateTransform without from or to, the underlying value is the identity transform */
	GF_Matrix2D identity;
	GF_FieldInfo default_transform_value;

	/* result of the animation */
	GF_FieldInfo interpolated_value;
	
	/* has the interpolated value changed since last cycle */
	Bool interpolated_value_changed;

	/* last value of the animation, used in accumulation phase */
	GF_FieldInfo last_specified_value;

	/* temporary value needed when the type of 
	   the key values is different from the target attribute type */
	GF_FieldInfo tmp_value;

	s32		previous_key_index;
	Fixed	previous_coef;
	u32		previous_keytime_index;

	GF_Path *path;
	u8 rotate;
	GF_PathIterator *path_iterator;
	Fixed length;

} SMIL_Anim_RTI;

void gf_smil_anim_init_node(GF_Node *node);
void gf_smil_anim_init_discard(GF_Node *node);
void gf_smil_anim_init_runtime_info(SVGElement *e);
void gf_smil_anim_delete_runtime_info(SMIL_Anim_RTI *rai);
void gf_smil_anim_delete_animations(SVGElement *e);
void gf_smil_anim_remove_from_target(SVGElement *anim, SVGElement *target);

void gf_svg_init_lsr_conditional(SVGCommandBuffer *script);
void gf_svg_reset_lsr_conditional(SVGCommandBuffer *script);

void gf_sg_handle_dom_event(struct _tagSVGhandlerElement *hdl, GF_DOM_Event *event);
void gf_smil_setup_events(GF_Node *node);

s32 gf_svg_get_attribute_index_by_name(GF_Node *node, char *name);

#endif


//
//		MF Fields tools
//	WARNING: MF / SF Nodes CANNOT USE THESE FUNCTIONS
//

//return the size (in bytes) of fixed fields (buffers are handled as a char ptr , 1 byte)
u32 gf_sg_vrml_get_sf_size(u32 FieldType);


/*BASE node (GF_Node) destructor*/
void gf_node_free(GF_Node *node);

/*node destructor dispatcher: redirects destruction for each graph type: VRML/MPEG4, X3D, SVG...)*/
void gf_node_del(GF_Node *node);

//these 2 functions are used when deleting the nodes (DESTRUCTORS ONLY), because the 
//info about DEF ? USE is stored somewhere else (usually, BIFS codec or XMT parser)
//the parent node is used to determined the acyclic portions of the scene graph
void gf_node_list_del(GF_List *children, GF_Node *parent);

/*creates an undefined GF_Node - for parsing only*/
GF_Node *gf_sg_new_base_node();

/*returns field type from its name*/
u32 gf_sg_field_type_by_name(char *fieldType);



/*
			Proto node

*/

/*field interface to codec. This is used to do the node decoding, index translation
and all QP/BIFS Anim parsing. */
struct _protofield
{
	u8 EventType;
	u8 FieldType;
	/*if UseName, otherwise fieldN*/
	char *FieldName;

	/*default field value*/
	void *def_value;
	
	GF_Node *def_sfnode_value;
	GF_List *def_mfnode_value;

	/*for instanciation - if externProto dit not specify field val*/
	u8 val_not_loaded;

	/*coding indexes*/
	u32 IN_index, OUT_index, DEF_index, ALL_index;

	/*Quantization*/
	u32 QP_Type, hasMinMax;
	void *qp_min_value, *qp_max_value;
	/*this is for QP=13 only*/
	u32 NumBits;

	/*Animation*/
	u32 Anim_Type;

	void *userpriv;
	void (*OnDelete)(void *ptr);
};

GF_ProtoFieldInterface *gf_sg_proto_new_field_interface(u32 FieldType);


/*proto field instance. since it is useless to duplicate all coding info, names and the like
we seperate proto declaration and proto instanciation*/
typedef struct 
{
	u8 EventType;
	u8 FieldType;
	u8 has_been_accessed;
	void *field_pointer;
} GF_ProtoField;


struct _proto
{
	/*1 - Prototype interface*/
	u32 ID;
	char *Name;
	GF_List *proto_fields;

	/*pointer to parent scene graph*/
	struct __tag_scene_graph *parent_graph;
	/*pointer to proto scene graph*/
	struct __tag_scene_graph *sub_graph;

	/*2 - proto implementation as declared in the bitstream*/
	GF_List *node_code;

	/*num fields*/
	u32 NumIn, NumOut, NumDef, NumDyn;

	void *userpriv;
	void (*OnDelete)(void *ptr);

	/*URL of extern proto lib (if none, URL is empty)*/
	MFURL ExternProto;

	/*list of instances*/
	GF_List *instances;
};

/*proto field API*/
u32 gf_sg_proto_get_num_fields(GF_Node *node, u8 code_mode);
GF_Err gf_sg_proto_get_field(GF_Proto *proto, GF_Node *node, GF_FieldInfo *field);


typedef struct _proto_instance
{
	/*this is a node*/
	BASE_NODE

	/*Prototype interface for coding and field addressing*/
	GF_Proto *proto_interface;

	/*proto implementation at run-time (aka the state of the nodes may differ accross
	different instances of the proto)*/
	GF_List *fields;

	/*a proto doesn't have one root SFnode but a collection of nodes for implementation*/
	GF_List *node_code;

	/*node for proto rendering, first of all declared nodes*/
	GF_Node *RenderingNode;

#ifndef GF_NODE_USE_POINTERS
	/*in case the PROTO is destroyed*/
	char *proto_name;
#endif

	/*scripts are loaded once all IS routes are activated and node code is loaded*/
	GF_List *scripts_to_load;

	Bool is_loaded;
} GF_ProtoInstance;

/*destroy proto*/
void gf_sg_proto_del_instance(GF_ProtoInstance *inst);
GF_Err gf_sg_proto_get_field_index(GF_ProtoInstance *proto, u32 index, u32 code_mode, u32 *all_index);
Bool gf_sg_proto_get_aq_info(GF_Node *Node, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits);
GF_Err gf_sg_proto_get_field_ind_static(GF_Node *Node, u32 inField, u8 IndexMode, u32 *allField);
GF_Node *gf_sg_proto_create_node(GF_SceneGraph *scene, GF_Proto *proto, GF_ProtoInstance *from_inst);
void gf_sg_proto_instanciate(GF_ProtoInstance *proto_node);

/*get tag of first node in proto code - used for validation only*/
u32 gf_sg_proto_get_render_tag(GF_Proto *proto);


/*to call when a proto field has been modified (at creation or through commands, modifications through events 
are handled internally).
node can be the proto instance or a node from the proto code
this will call NodeChanged if needed, forward to proto/node or trigger any route if needed*/
void gf_sg_proto_check_field_change(GF_Node *node, u32 fieldIndex);

s32 gf_sg_proto_get_field_index_by_name(GF_Proto *proto, GF_Node *node, char *name);


/*
		Script node
*/

#ifdef GPAC_HAS_SPIDERMONKEY

/*WIN32 and WinCE config (no configure script)*/
#if defined(WIN32) || defined(_WIN32_WCE) || defined(__SYMBIAN32__)
#ifndef XP_PC
#define XP_PC
#endif
/*WINCE specific config*/
#if defined(_WIN32_WCE)
#include <windows.h>
#define XP_WINCE
#endif
#endif

/*other platforms should be setup through configure*/

#include <jsapi.h> 

#endif

typedef struct 
{
	//extra script fields
	GF_List *fields;

	//BIFS coding stuff
	u32 numIn, numDef, numOut;

	/*pointer to original tables*/
#ifdef GF_NODE_USE_POINTERS
	GF_Err (*gf_sg_script_get_field)(GF_Node *node, GF_FieldInfo *info);
	GF_Err (*gf_sg_script_get_field_index)(GF_Node *node, u32 inField, u8 IndexMode, u32 *allField);
#endif


#ifdef GPAC_HAS_SPIDERMONKEY
	JSContext *js_ctx;
	struct JSObject *js_obj;
	struct JSObject *js_browser;
	/*all attached objects (eg, not created by the script) are stored here so that we don't
	allocate them again and again when getting properties. Garbage collection is performed (if needed)
	on these objects after each eventIn execution*/
	GF_List *js_cache;
#endif

	void (*JS_PreDestroy)(GF_Node *node);
	void (*JS_EventIn)(GF_Node *node, GF_FieldInfo *in_field);

	Bool is_loaded;
} GF_ScriptPriv;

/*setup script stack*/
void gf_sg_script_init(GF_Node *node);
/*get script field*/
GF_Err gf_sg_script_get_field(GF_Node *node, GF_FieldInfo *info);
/*get effective field count per event mode*/
u32 gf_sg_script_get_num_fields(GF_Node *node, u8 IndexMode);
/*translate field index from inMode to ALL mode*/
GF_Err gf_sg_script_get_field_index(GF_Node *Node, u32 inField, u8 IndexMode, u32 *allField);
/*create dynamic fields in the clone*/
GF_Err gf_sg_script_prepare_clone(GF_Node *dest, GF_Node *orig);

struct _scriptfield
{
	u32 eventType;
	u32 fieldType;
	char *name;

	s32 IN_index, OUT_index, DEF_index;
	u32 ALL_index;

	//real field
	void *pField;

	Double last_route_time;

	Bool activate_event_out;
};


#ifdef GPAC_HAS_SPIDERMONKEY

JSContext *gf_sg_ecmascript_new();
void gf_sg_ecmascript_del(JSContext *);

void gf_sg_script_init_sm_api(GF_ScriptPriv *sc, GF_Node *script);
JSBool gf_sg_script_eventout_set_prop(JSContext *c, JSObject *obj, jsval id, jsval *val);

typedef struct 
{
	GF_FieldInfo field;
	GF_Node *owner;
	JSObject *obj;

	/*JS list for MFFields or NULL*/
	JSObject *js_list;

	/*when creating SFnode from inside the script, the node is stored here untill attached to an object*/
	GF_Node *temp_node;
	GF_List *temp_list;
	/*when not owned by a node*/
	void *field_ptr;
} GF_JSField;

void gf_sg_script_to_node_field(JSContext *c, jsval v, GF_FieldInfo *field, GF_Node *owner, GF_JSField *parent);
jsval gf_sg_script_to_smjs_field(GF_ScriptPriv *priv, GF_FieldInfo *field, GF_Node *parent, Bool no_cache);




#ifndef GPAC_DISABLE_SVG
void JSScript_LoadSVG(GF_Node *node);

typedef struct __tag_svg_script_ctx 
{
	Bool (*script_execute)(struct __tag_scene_graph *sg, char *utf8_script, GF_DOM_Event *event);
	Bool (*handler_execute)(GF_Node *n, GF_DOM_Event *event);
	void (*on_node_destroy)(struct __tag_scene_graph *sg, GF_Node *n);

	JSContext *js_ctx;
	u32 nb_scripts;

	/*node bank*/
	GF_List *node_bank;
	/*global object*/
	JSObject *global;
	/*event object*/
	JSObject *event;
	/*document object*/
	JSObject *document;
} GF_SVGJS;

#endif

#endif

#endif	/*_GF_SCENEGRAPH_DEV_H_*/
