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


#ifndef _GF_SCENEGRAPH_H_
#define _GF_SCENEGRAPH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/list.h>
#include <gpac/math.h>


/*define to enable SVG test graph with static attribute allocation but no inheritance*/
//#define	GPAC_ENABLE_SVG_SA
//#define	GPAC_ENABLE_SVG_SANI

#if defined(GPAC_ENABLE_SVG_SA) || defined(GPAC_ENABLE_SVG_SANI)
#define	GPAC_ENABLE_SVG_SA_BASE
#endif


/*
	TAG definitions are static, in order to be able to mix nodes from different standard
	in a single scenegraph. These TAGs are only used internally (they do not match any
	binary encoding)
*/
enum {
	/*no node shall use this tag*/
	TAG_ForbiddenZero = 0,
	/*undefined node: just the base node class, used for parsing*/
	TAG_UndefinedNode = 1,
	/*all MPEG-4/VRML/X3D proto instances have this tag*/
	TAG_ProtoNode,

	/*reserved TAG ranges per standard*/

	/*range for MPEG4*/
	GF_NODE_RANGE_FIRST_MPEG4,
	GF_NODE_RANGE_LAST_MPEG4 = GF_NODE_RANGE_FIRST_MPEG4+512,

	/*range for X3D*/
	GF_NODE_RANGE_FIRST_X3D, 
	GF_NODE_RANGE_LAST_X3D = GF_NODE_RANGE_FIRST_X3D+512,

	/*all nodes after this are always parent nodes*/
	GF_NODE_RANGE_LAST_VRML,

	/*DOM container for BIFS/LASeR/etc updates*/
	TAG_DOMUpdates,

	/*all nodes below MUST be parent nodes*/
	GF_NODE_FIRST_PARENT_NODE_TAG,

#ifdef GPAC_ENABLE_SVG_SA
	/*range for SVG / static alloc mode*/
	GF_NODE_RANGE_FIRST_SVG_SA, 
	GF_NODE_RANGE_LAST_SVG_SA = GF_NODE_RANGE_FIRST_SVG_SA+100,
#endif

#ifdef GPAC_ENABLE_SVG_SANI
	/*range for SVG static alloc and no inheritance mode*/
	GF_NODE_RANGE_FIRST_SVG_SANI, 
	GF_NODE_RANGE_LAST_SVG_SANI = GF_NODE_RANGE_FIRST_SVG_SANI+100,
#endif

	/*DOM text node*/
	TAG_DOMText,
	/*all nodes below MUST use the base DOM structure (with dyn attribute list)*/
	GF_NODE_FIRST_DOM_NODE_TAG,
	
	/*full node*/
	TAG_DOMFullNode = GF_NODE_FIRST_DOM_NODE_TAG,
	/*range for SVG*/
	GF_NODE_RANGE_FIRST_SVG, 
	GF_NODE_RANGE_LAST_SVG = GF_NODE_RANGE_FIRST_SVG+100,

};



/*private handler for this library on all nodes*/
#define BASE_NODE	struct _nodepriv *sgprivate;

/*base node type*/
typedef struct _base_node
{
	BASE_NODE
} GF_Node;

/*child storage - this is not integrated in the base node, because of VRML/MPEG-4 USE: a node
may be present at different places in the tree, hence have different "next" siblings.*/
typedef struct _child_node
{
	struct _child_node *next;
	GF_Node *node;
} GF_ChildNodeItem;

/*grouping nodes macro :
	children: list of children SFNodes
*/

#define CHILDREN									\
	struct _child_node *children;

typedef struct
{
	BASE_NODE
	CHILDREN
} GF_ParentNode;

/*adds a child to a given container*/
GF_Err gf_node_list_add_child(GF_ChildNodeItem **list, GF_Node *n);
/*adds a child to a given container, updating last position*/
GF_Err gf_node_list_add_child_last(GF_ChildNodeItem **list, GF_Node *n, GF_ChildNodeItem **last_child);
/*inserts a child to a given container - if pos doesn't match, append the child*/
GF_Err gf_node_list_insert_child(GF_ChildNodeItem **list, GF_Node *n, u32 pos);
/*removes a child to a given container - return 0 if child not found*/
Bool gf_node_list_del_child(GF_ChildNodeItem **list, GF_Node *n);
/*finds a child in a given container, returning its 0-based index if found, -1 otherwise*/
s32 gf_node_list_find_child(GF_ChildNodeItem *list, GF_Node *n);
/*finds a child in a given container given its index, returning the child or NULL if not found
if pos is <0, returns the last child*/
GF_Node *gf_node_list_get_child(GF_ChildNodeItem *list, s32 pos);
/*gets the number of children in a given container*/
u32 gf_node_list_get_count(GF_ChildNodeItem *list);
/*deletes node entry at given idx, returning node if found, NULL otherwise*/
GF_Node *gf_node_list_del_child_idx(GF_ChildNodeItem **list, u32 pos);



/*tag is set upon creation and cannot be modified*/
u32 gf_node_get_tag(GF_Node*);
/*set node def
	@ID: node ID, !=0 set def node - if a different node with the same ID exists, returns error. 
You may change the node ID by recalling the function with a different ID value. You may get a node ID
by calling the gf_sg_get_next_available_node_id function
	@defName: optional readable name (script, MPEGJ). To change the name, recall the function with a different name and the same ID
*/
GF_Err gf_node_set_id(GF_Node*n, u32 nodeID, const char *nodeDEFName);
/*get def name of the node , NULL if not set*/
const char *gf_node_get_name(GF_Node*);
/*get def ID of the node, 0 if node not def*/
u32 gf_node_get_id(GF_Node*);
/* gets node built-in name (eg 'Appearance', ..) */
const char *gf_node_get_class_name(GF_Node *Node);

/*unset the node ID*/
GF_Err gf_node_remove_id(GF_Node *p);

/*get/set user private stack*/
void *gf_node_get_private(GF_Node*);
void gf_node_set_private(GF_Node*, void *);

/*set rendering function. When rendering a scene graph, the render stack is passed
through the graph without being touched. If a node has no associated RenderNode(), the traversing of the 
graph won't propagate below it. It is the app responsability to setup traversing functions as needed
VRML/MPEG4:  Instanciated Protos are handled internally as well as interpolators, valuators and scripts
@is_destroy: set when the node is about to be destroyed
*/
GF_Err gf_node_set_callback_function(GF_Node *, void (*NodeFunction)(GF_Node *node, void *render_stack, Bool is_destroy) );

/*register a node (DEFed or not), specifying parent if any.
A node must be registered whenever used by something (a parent node, a command, whatever) to prevent its 
destruction (think of it as a reference counting).
NOTE: NODES ARE CREATED WITHOUT BEING REGISTERED
*/
GF_Err gf_node_register(GF_Node *node, GF_Node *parent_node);

/*unregister a node from parent (node may or not be DEF'ed). Parent may be NULL (DEF root node, commands).
This MUST be called whenever a node is destroyed (removed from a parent node)
If this is the last instance of the node, the node is destroyed
NOTE: NODES ARE CREATED WITHOUT BEING REGISTERED, hence they MUST be registered at least once before 
being destroyed
*/
GF_Err gf_node_unregister(GF_Node *node, GF_Node *parent_node);
/*deletes all node instances in the given list*/
void gf_node_unregister_children(GF_Node *node, GF_ChildNodeItem *childrenlist);

/*get all parents of the node and replace the old_node by the new node in all parents
Note: if the new node is not DEFed, only the first instance of "old_node" will be replaced, the other ones deleted*/
GF_Err gf_node_replace(GF_Node *old_node, GF_Node *new_node, Bool updateOrderedGroup);

/*returns number of instances for this node*/
u32 gf_node_get_num_instances(GF_Node *node);


/*calls RenderNode on this node*/
void gf_node_render(GF_Node *node, void *renderStack);
/*allows a node to be re-rendered - by default a node in its render phase will never be rendered a second time. 
Use this function to enable a second render for this node - this must be called while node is being rendered*/
void gf_node_allow_cyclic_render(GF_Node *node);

/*blindly calls RenderNode on all nodes in the "children" list*/
void gf_node_render_children(GF_Node *node, void *renderStack);;
/*returns number of parent for this node (parent are kept regardless of DEF state)*/
u32 gf_node_get_parent_count(GF_Node *node);
/*returns desired parent for this node (parent are kept regardless of DEF state)
idx is 0-based parent index*/
GF_Node *gf_node_get_parent(GF_Node *node, u32 idx);


enum
{
	/*flag set whenever a field of the node has been modified*/
	GF_SG_NODE_DIRTY = 1,
	/*flag set whenever a child node of this node has been modified
	NOTE: unloaded extern protos always invalidate their parent subgraph to get a chance
	of being loaded. It is the user responsability to clear the CHILD_DIRTY flag before traversing
	if relying on this flag for sub-tree discarding (eg, culling or similar)*/
	GF_SG_CHILD_DIRTY = 1<<1,

	/*SVG-specific flags due to mix of geometry and appearance & co attributes*/
	/*SVG geometry changed is the same as base flag*/
	GF_SG_SVG_GEOMETRY_DIRTY		= GF_SG_NODE_DIRTY,
	GF_SG_SVG_COLOR_DIRTY			= 1<<2,
	GF_SG_SVG_DISPLAYALIGN_DIRTY	= 1<<3,
	GF_SG_SVG_FILL_DIRTY			= 1<<4,
	GF_SG_SVG_FILLOPACITY_DIRTY		= 1<<5,
	GF_SG_SVG_FILLRULE_DIRTY		= 1<<6,
	GF_SG_SVG_FONTFAMILY_DIRTY		= 1<<7,
	GF_SG_SVG_FONTSIZE_DIRTY		= 1<<8,
	GF_SG_SVG_FONTSTYLE_DIRTY		= 1<<9,
	GF_SG_SVG_FONTVARIANT_DIRTY		= 1<<10,
	GF_SG_SVG_FONTWEIGHT_DIRTY		= 1<<11,
	GF_SG_SVG_LINEINCREMENT_DIRTY	= 1<<12,
	GF_SG_SVG_OPACITY_DIRTY			= 1<<13,
	GF_SG_SVG_SOLIDCOLOR_DIRTY		= 1<<14,
	GF_SG_SVG_SOLIDOPACITY_DIRTY	= 1<<15,
	GF_SG_SVG_STOPCOLOR_DIRTY		= 1<<16,
	GF_SG_SVG_STOPOPACITY_DIRTY		= 1<<17,
	GF_SG_SVG_STROKE_DIRTY			= 1<<18,
	GF_SG_SVG_STROKEDASHARRAY_DIRTY	= 1<<19,
	GF_SG_SVG_STROKEDASHOFFSET_DIRTY= 1<<20,
	GF_SG_SVG_STROKELINECAP_DIRTY	= 1<<21,
	GF_SG_SVG_STROKELINEJOIN_DIRTY	= 1<<22,
	GF_SG_SVG_STROKEMITERLIMIT_DIRTY= 1<<23,
	GF_SG_SVG_STROKEOPACITY_DIRTY	= 1<<24,
	GF_SG_SVG_STROKEWIDTH_DIRTY		= 1<<25,
	GF_SG_SVG_TEXTALIGN_DIRTY		= 1<<26,
	GF_SG_SVG_TEXTANCHOR_DIRTY		= 1<<27,
	GF_SG_SVG_VECTOREFFECT_DIRTY	= 1<<28,
};

/*set dirty flags.
if @flags is 0, sets the base flags on (GF_SG_NODE_DIRTY).
if @flags is not 0, adds the flags to the node dirty state

If @invalidate_parents is set, all parent subtrees for this node are marked as GF_SG_CHILD_DIRTY
Note: parent subtree marking aborts if a node in the subtree is already marked with GF_SG_CHILD_DIRTY
which means tat if you never clean the dirty flags, no propagation will take place
*/
void gf_node_dirty_set(GF_Node *node, u32 flags, Bool dirty_parents);


/*set dirty flag off. It is the user responsability to clear dirty flags
if @flags is 0, all flags are set off
if @flags is not 0, removes the indicated flags from the node dirty state
*/
void gf_node_dirty_clear(GF_Node *node, u32 flags);

/*if the node is in a dirty state, resets it and the state of all its children*/
void gf_node_dirty_reset(GF_Node *node);

/*get dirty flag value*/
u32 gf_node_dirty_get(GF_Node *node);

/*Notes on GF_FieldInfo
all scene graph implementations should answer node field query with this interface. 
In case an implementation does not use this:
	- the implementation shall handle the parent node dirty flag itself most of the time
	- the implementation shall NOT allow referencing of a graph node in a parent graph node (when inlining
content) otherwise the app is guaranteed to crash.
*/

/*other fieldTypes may be ignored by implmentation not using VRML/MPEG4 native types*/

typedef struct
{	
	/*0-based index of the field in the node*/
	u32 fieldIndex;
	/*field type - VRML/MPEG4 types are listed in scenegraph_vrml.h*/
	u32 fieldType;
	/*far ptr to the field (eg GF_Node **, GF_List**, MFInt32 *, ...)*/
	void *far_ptr;
	/*field name*/
	const char *name;
	/*NDT type in case of SF/MFNode field - cf BIFS specific tools*/
	u32 NDTtype;
	/*event type*/
	u32 eventType;
	/*eventin handler if any*/
	void (*on_event_in)(GF_Node *pNode);
} GF_FieldInfo;

/*returns number of field for this node*/
u32 gf_node_get_field_count(GF_Node *node);

/*fill the field info structure for the given field*/
GF_Err gf_node_get_field(GF_Node *node, u32 FieldIndex, GF_FieldInfo *info);

/*get the field by its name*/
GF_Err gf_node_get_field_by_name(GF_Node *node, char *name, GF_FieldInfo *field);

typedef struct __tag_scene_graph GF_SceneGraph;

/*scene graph constructor*/
GF_SceneGraph *gf_sg_new();

/*creates a sub scene graph (typically used with Inline node): independent graph with same private stack, 
and user callbacks as parent. All routes triggered in this subgraph are executed in the parent graph (this 
means you only have to activate routes on the main graph)
NOTE: the resulting graph is not destroyed when the parent graph is 
*/
GF_SceneGraph *gf_sg_new_subscene(GF_SceneGraph *scene);

/*destructor*/
void gf_sg_del(GF_SceneGraph *sg);
/*reset the full graph - all nodes, routes and protos are destroyed*/
void gf_sg_reset(GF_SceneGraph *sg);

/*set/get user private data*/
void gf_sg_set_private(GF_SceneGraph *sg, void *user_priv);
void *gf_sg_get_private(GF_SceneGraph *sg);

/*set the scene timer (fct returns time in sec)*/
void gf_sg_set_scene_time_callback(GF_SceneGraph *scene, Double (*GetSceneTime)(void *user_priv) );

enum
{
	/*function called upon node creation. 
		ctxdata is not used*/
	GF_SG_CALLBACK_INIT = 0,
	/*function called upon node modification. You typically will set some of the dirty flags here.
		ctxdata is the fieldInfo pointer of the modified field*/
	GF_SG_CALLBACK_MODIFIED,
	/*function called when the a "set dirty" propagates to root node of the graph
		ctxdata is not used*/
	GF_SG_CALLBACK_GRAPH_DIRTY,
};

/*set node callback: function called upon node creation. 
Application should instanciate the node rendering stack and any desired callback*/
void gf_sg_set_node_callback(GF_SceneGraph *sg, void (*NodeCallback)(void *user_priv, u32 type, GF_Node *node, void *ctxdata) );

/*get/set the root node of the graph*/
GF_Node *gf_sg_get_root_node(GF_SceneGraph *sg);
void gf_sg_set_root_node(GF_SceneGraph *sg, GF_Node *node);

/*finds a registered node by ID*/
GF_Node *gf_sg_find_node(GF_SceneGraph *sg, u32 nodeID);
/*finds a registered node by DEF name*/
GF_Node *gf_sg_find_node_by_name(GF_SceneGraph *sg, char *name);

/*used to signal modification of a node, indicating which field is modified - exposed for BIFS codec, 
should not be needed by other apps*/
void gf_node_changed(GF_Node *node, GF_FieldInfo *fieldChanged);

/*returns the graph this node belongs to*/
GF_SceneGraph *gf_node_get_graph(GF_Node *node);

/*Set size info for the graph - by default graphs have no size and are in meter metrics (VRML like)
if any of width or height is 0, the graph has no size info*/
void gf_sg_set_scene_size_info(GF_SceneGraph *sg, u32 width, u32 Height, Bool usePixelMetrics);
/*returns 1 if pixelMetrics*/
Bool gf_sg_use_pixel_metrics(GF_SceneGraph *sg);
/*returns 0 if no size info, otherwise 1 and set width/height*/
Bool gf_sg_get_scene_size_info(GF_SceneGraph *sg, u32 *width, u32 *Height);

/*creates a node of the given tag. sg is the parent scenegraph of the node,
eg the root one for scene nodes or the proto one for proto code (cf proto)
Note:
	- NODE IS NOT REGISTERED (no instances) AND CANNOT BE DESTROYED UNTIL REGISTERED
	- this doesn't perform application setup for the node, this must be done by the caller
*/
GF_Node *gf_node_new(GF_SceneGraph *sg, u32 tag);
/*clones a node in the given graph and register with parent cloned. The cloning respects DEF/USE nodes*/
GF_Node *gf_node_clone(GF_SceneGraph *inScene, GF_Node *orig, GF_Node *cloned_parent);
/*inits node (either internal stack or user-defined) - usually called once the node has been fully loaded*/
void gf_node_init(GF_Node *node);
/*gets scene time for scene this node belongs too, 0 if timeline not specified*/
Double gf_node_get_scene_time(GF_Node *node);

/*retuns next available NodeID*/
u32 gf_sg_get_next_available_node_id(GF_SceneGraph *sg);
/*retuns max ID used in graph*/
u32 gf_sg_get_max_node_id(GF_SceneGraph *sg);

const char *gf_node_get_name_and_id(GF_Node*node, u32 *id);


enum
{
	GF_SG_FOCUS_AUTO = 1,
	GF_SG_FOCUS_NEXT,
	GF_SG_FOCUS_PREV,
	GF_SG_FOCUS_NORTH,
	GF_SG_FOCUS_NORTH_EAST,
	GF_SG_FOCUS_EAST,
	GF_SG_FOCUS_SOUTH_EAST,
	GF_SG_FOCUS_SOUTH,
	GF_SG_FOCUS_SOUTH_WEST,
	GF_SG_FOCUS_WEST,
	GF_SG_FOCUS_NORTH_WEST
};

typedef struct
{
	const char *url;
	const char **params;
	u32 nb_params;
} GF_JSAPIURI;

typedef struct
{
	const char *section;
	const char *key;
	const char *key_val;
} GF_JSAPIOPT;

	/*for script message option*/
typedef struct
{
	GF_Err e;
	const char *msg;
} GF_JSAPIINFO;


typedef union
{
	u32 opt;
	Fixed val;
	GF_Point2D pt;
	GF_Rect rc;
	Double time;
	GF_BBox bbox;
	GF_Matrix mx;
	GF_JSAPIURI uri;
	GF_JSAPIOPT gpac_cfg;
	GF_Node *node;
	struct __gf_download_manager *dnld_man;
	GF_JSAPIINFO info;
} GF_JSAPIParam;

enum
{
	/*!push message from script engine.*/
	GF_JSAPI_OP_MESSAGE,
	/*!get scene URI.*/
	GF_JSAPI_OP_GET_SCENE_URI,
	/*!get current user agent scale.*/
	GF_JSAPI_OP_GET_SCALE,
	/*!set current user agent scale.*/
	GF_JSAPI_OP_SET_SCALE,
	/*!get current user agent rotation.*/
	GF_JSAPI_OP_GET_ROTATION,
	/*!set current user agent rotation.*/
	GF_JSAPI_OP_SET_ROTATION,
	/*!get current user agent translation.*/
	GF_JSAPI_OP_GET_TRANSLATE,
	/*!set current user agent translation.*/
	GF_JSAPI_OP_SET_TRANSLATE,
	/*!get node time.*/
	GF_JSAPI_OP_GET_TIME,
	/*!set node time.*/
	GF_JSAPI_OP_SET_TIME,
	/*!get current viewport.*/
	GF_JSAPI_OP_GET_VIEWPORT,
	/*!get object bounding box in object local coord system.*/
	GF_JSAPI_OP_GET_LOCAL_BBOX,
	/*!get object bounding box in world (screen) coord system.*/
	GF_JSAPI_OP_GET_SCREEN_BBOX,
	/*!get transform matrix at object.*/
	GF_JSAPI_OP_GET_TRANSFORM,
	/*!move focus according to opt value.*/
	GF_JSAPI_OP_MOVE_FOCUS,
	/*!set focus to given node.*/
	GF_JSAPI_OP_GET_FOCUS,
	/*!set focus to given node.*/
	GF_JSAPI_OP_SET_FOCUS,
	/*!get target scene URL*/
	GF_JSAPI_OP_GET_URL,
	/*!replace target scene URL*/
	GF_JSAPI_OP_LOAD_URL,
	/*!get option by section and key*/
	GF_JSAPI_OP_GET_OPT,
	/*!get option by section and key*/
	GF_JSAPI_OP_SET_OPT,
	/*!retrieve download manager*/
	GF_JSAPI_OP_GET_DOWNLOAD_MANAGER,
};
/*
interface to various get/set options:
	type: operand type, one of the above
	node: target node, scene root node or NULL
	param: i/o param, depending on operand type
*/
typedef Bool (*gf_sg_script_action)(void *callback, u32 type, GF_Node *node, GF_JSAPIParam *param);

/*assign API to scene graph - by default, sub-graphs inherits the API if set*/
void gf_sg_set_script_action(GF_SceneGraph *scene, gf_sg_script_action script_act, void *cbk);

/*load script into engine - this should be called only for script in main scene, loading of scripts
in protos is done internally when instanciating the proto*/
void gf_sg_script_load(GF_Node *script);

/*returns true if current lib has javascript support*/
Bool gf_sg_has_scripting();



/*
			scene graph command tools used for BIFS and LASeR
		These are used to store updates in memory without applying changes to the graph, 
	for dumpers, encoders ... 
		The commands can then be applied through this lib
*/

/*
		Currently defined possible modifications
*/
enum
{
	/*BIFS commands*/
	GF_SG_SCENE_REPLACE = 0,
	GF_SG_NODE_REPLACE,
	GF_SG_FIELD_REPLACE, 
	GF_SG_INDEXED_REPLACE,
	GF_SG_ROUTE_REPLACE,
	GF_SG_NODE_DELETE,
	GF_SG_INDEXED_DELETE,
	GF_SG_ROUTE_DELETE,
	GF_SG_NODE_INSERT,
	GF_SG_INDEXED_INSERT,
	GF_SG_ROUTE_INSERT,
	/*extended updates (BIFS-only)*/
	GF_SG_PROTO_INSERT,
	GF_SG_PROTO_DELETE,
	GF_SG_PROTO_DELETE_ALL,
	GF_SG_MULTIPLE_REPLACE,
	GF_SG_MULTIPLE_INDEXED_REPLACE,
	GF_SG_GLOBAL_QUANTIZER,
	/*same as NodeDelete, and also updates OrderedGroup.order when deleting a child*/
	GF_SG_NODE_DELETE_EX,

	GF_SG_LAST_BIFS_COMMAND,


	/*LASER commands*/
	GF_SG_LSR_NEW_SCENE,
	GF_SG_LSR_REFRESH_SCENE,
	GF_SG_LSR_ADD,
	GF_SG_LSR_CLEAN,
	GF_SG_LSR_REPLACE,
	GF_SG_LSR_DELETE,
	GF_SG_LSR_INSERT,
	GF_SG_LSR_RESTORE,
	GF_SG_LSR_SAVE,
	GF_SG_LSR_SEND_EVENT,

	GF_SG_UNDEFINED
};


/*
				single command wrapper

  NOTE: In order to maintain node registry, the nodes replaced/inserted MUST be registered with 
  their parents even when the command is never applied. Registering shall be performed 
  with gf_node_register (see below).
  If you fail to do so, a node may be destroyed when destroying a command while still used
  in another command or in the graph - this will just crash.
*/

/*structure used to store field info, pos and static pointers to GF_Node/MFNode in commands*/
typedef struct
{
	u32 fieldIndex;
	/*field type*/
	u32 fieldType;
	/*field pointer for multiple replace/multiple indexed replace - if multiple indexed replace, must be the SF field being changed*/
	void *field_ptr;
	/*replace/insert/delete pos - -1 is append except in multiple indexed replace*/
	s32 pos;

	/*Whenever field pointer is of type GF_Node, store the node here and set the far pointer to this address.*/
	GF_Node *new_node;
	/*Whenever field pointer is of type MFNode, store the node list here and set the far pointer to this address.*/
	GF_ChildNodeItem *node_list;
} GF_CommandField;

typedef struct
{
	GF_SceneGraph *in_scene;
	u32 tag;

	/*node the command applies to - may be NULL*/
	GF_Node *node;

	/*list of GF_CommandField for all field commands replace/ index insert / index replace / index delete / MultipleReplace / MultipleIndexedreplace 
	the content is destroyed when deleting the command*/
	GF_List *command_fields;

	/*may be NULL, and may be present with any command inserting a node*/
	GF_List *scripts_to_load;
	/*for authoring purposes - must be cleaned by user*/
	Bool unresolved;
	char *unres_name;
	
	/*scene replace command: 
		root node is stored in com->node
		protos are stored in com->new_proto_list
		routes are stored as RouteInsert in the same frame
		BIFS only
	*/
	Bool use_names;

	/*route insert, replace and delete (BIFS only)
	fromNodeID is also used to identify operandElementId in LASeR Add/Replace
	*/
	u32 RouteID;
	char *def_name;
	u32 fromNodeID, fromFieldIndex;
	u32 toNodeID, toFieldIndex;

	/*proto list to insert - BIFS only*/
	GF_List *new_proto_list;
	/*proto ID list to delete - BIFS only*/
	u32 *del_proto_list;
	u32 del_proto_list_size;
} GF_Command;


/*creates command - graph only needed for SceneReplace*/
GF_Command *gf_sg_command_new(GF_SceneGraph *in_scene, u32 tag);
/*deletes command*/
void gf_sg_command_del(GF_Command *com);
/*apply command to graph - the command content is kept unchanged for authoring purposes - THIS NEEDS TESTING AND FIXING
@time_offset: offset for time fields if desired*/
GF_Err gf_sg_command_apply(GF_SceneGraph *inScene, GF_Command *com, Double time_offset);
/*apply list if command to graph - the command content is kept unchanged for authoring purposes
@time_offset: offset for time fields if desired*/
GF_Err gf_sg_command_apply_list(GF_SceneGraph *graph, GF_List *comList, Double time_offset);
/*returns new commandFieldInfo structure and registers it with command*/
GF_CommandField *gf_sg_command_field_new(GF_Command *com);
/*clones the command in another graph - needed for uncompressed conditional in protos*/
GF_Command *gf_sg_command_clone(GF_Command *com, GF_SceneGraph *inGraph);


#ifdef __cplusplus
}
#endif



#endif /*_GF_SCENEGRAPH_H_*/


