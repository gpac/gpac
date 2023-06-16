/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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


/*!
\file <gpac/scenegraph.h>
\brief Scenegraph used for manipulating scenes (parsing, traversing, cleaning node status, ...)
*/
	
	
/*!
\addtogroup scene_grp Scene Graph
\brief Scene graph management.
*/
	
/*!
\addtogroup sscene Base Scenegraph
\ingroup scene_grp
\brief Scenegraph used for manipulating scenes.

This section documents the Scenegraph used in GPAC for all interactive scenes.

@{
 */


#include <gpac/list.h>
#include <gpac/maths.h>

/*! Tags of scene graph nodes
TAG definitions are static, in order to be able to mix nodes from different standard in a single scenegraph.
These TAGs are only used internally (they do not match any binary encoding)
*/
enum {
	/*undefined node: just the base node class, used for parsing*/
	TAG_UndefinedNode = 0,
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



/*! macro for defining base node (apply to all nodes)*/
#define BASE_NODE	struct _nodepriv *sgprivate;

/*! base node type*/
typedef struct _base_node
{
	BASE_NODE
} GF_Node;

/*! child storage
	This is not integrated in the base node, because of VRML/MPEG-4 USE: a node
may be present at different places in the tree, hence have different "next" siblings.*/
typedef struct _child_node
{
	struct _child_node *next;
	GF_Node *node;
} GF_ChildNodeItem;

/*! grouping nodes macro
	children: list of children SFNodes
*/
#define CHILDREN									\
	struct _child_node *children;

/*! generic parent node*/
typedef struct
{
	BASE_NODE
	CHILDREN
} GF_ParentNode;

/*! adds a child to a given container
\param list pointer to target child list
\param n node to add
\return error if any
*/
GF_Err gf_node_list_add_child(GF_ChildNodeItem **list, GF_Node *n);
/*! adds a child to a given container, updating last position
\param list pointer to target child list
\param n node to add
\param last_child set to position of add child
\return error if any
*/
GF_Err gf_node_list_add_child_last(GF_ChildNodeItem **list, GF_Node *n, GF_ChildNodeItem **last_child);
/*! inserts a child to a given container - if pos doesn't match, append the child
\param list pointer to target child list
\param n node to insert
\param pos 0-based index at which to insert
\return error if any
*/
GF_Err gf_node_list_insert_child(GF_ChildNodeItem **list, GF_Node *n, u32 pos);
/*! removes a child to a given container
\param list pointer to target child list
\param n node to remove
\return GF_TRUE if OK, GF_FALSE if not found
*/
Bool gf_node_list_del_child(GF_ChildNodeItem **list, GF_Node *n);
/*! finds a child in a given container
\param list target child list
\param n node to find
\return 0-based index if found, -1 otherwise
*/
s32 gf_node_list_find_child(GF_ChildNodeItem *list, GF_Node *n);
/*! finds a child in a given container given its index. if pos is <0, returns the last child
\param list target child list
\param pos 0-based index at which to insert
\return the child or NULL if not found
*/
GF_Node *gf_node_list_get_child(GF_ChildNodeItem *list, s32 pos);
/*! gets the number of children in a given list
\param list target child list
\return the number of children
*/
u32 gf_node_list_get_count(GF_ChildNodeItem *list);
/*! removes node at given idx
\param list pointer to target child list
\param pos 0-based index at which to insert
\return the removed node, or NULL if not found
*/
GF_Node *gf_node_list_del_child_idx(GF_ChildNodeItem **list, u32 pos);



/*! gets tag of node (tag is set upon creation and cannot be modified)
\param n the target node
\return the node tag
*/
u32 gf_node_get_tag(GF_Node*n);

/*! set node ID/def
If a different node with the same ID exists, returns error.
You may change the node ID by recalling the function with a different ID value. You may get a node ID by calling \ref gf_sg_get_next_available_node_id

\param n the target node
\param nodeID ID to set to the node, ignored if 0.
\param nodeDEFName optional readable name (script, MPEGJ). To change the name, recall the function with a different name and the same ID
\return error if any
*/
GF_Err gf_node_set_id(GF_Node*n, u32 nodeID, const char *nodeDEFName);
/*! gets def name of the node
\param n the target node
\return node name or NULL if not set
*/
const char *gf_node_get_name(GF_Node*n);
/*! gets def name of the node, or the string representation of the node pointer if not set
\param n the target node
\return node name or NULL if error
*/
const char *gf_node_get_log_name(GF_Node*n);
/*! gets def ID of the node
\param n the target node
\return the ID of the node, 0 if node not def
*/
u32 gf_node_get_id(GF_Node*n);
/*! gets node built-in name (eg 'Appearance', ..)
\param n the target node
\return node class name
*/
const char *gf_node_get_class_name(GF_Node *n);

/*! unsets the node ID
\param n the target node
\return error if any
*/
GF_Err gf_node_remove_id(GF_Node *n);

/*! gets user private of node
\param n the target node
\return user data if any, NULL if error or not assigned
*/
void *gf_node_get_private(GF_Node*n);
/*! sets user private of node
\param n the target node
\param udta user data to assign to the node
*/
void gf_node_set_private(GF_Node*n, void *udta);

/*! node callback function
\param n the target node
\param traverse_state opaque data passed during traversal
\param is_destroy set when the node is about to be destroyed
*/
typedef void (*gf_sg_node_callback)(GF_Node *n, void *traverse_state, Bool is_destroy);

/*! sets traversal callback function. If a node has no associated callback, the traversing of the
graph won't propagate below it. It is the app responsibility to setup traversing functions as needed
VRML/MPEG4:  Instantiated Protos are handled internally as well as interpolators, valuators and scripts
\param n the target node
\param NodeFunction the callback function
\return error if any
*/
GF_Err gf_node_set_callback_function(GF_Node *n, gf_sg_node_callback NodeFunction);

/*! registers a node (DEFed or not), specifying parent if any.
A node must be registered whenever used by something (a parent node, a command, whatever) to prevent its
destruction (think of it as a reference counting).
\warning NODES ARE CREATED WITHOUT BEING REGISTERED
\param n the target node
\param parent_node the parent node this node should be registered with
\return error if any
*/
GF_Err gf_node_register(GF_Node *n, GF_Node *parent_node);

/*! unregister a node from parent (node may or not be DEF'ed). Parent may be NULL (DEF root node, commands).
This MUST be called whenever a node is destroyed (removed from a parent node)
If this is the last instance of the node, the node is destroyed
\warning NODES ARE CREATED WITHOUT BEING REGISTERED, hence they MUST be registered at least once before
being destroyed
\param n the target node
\param parent_node the parent node this node should be unregistered from
\return error if any
*/
GF_Err gf_node_unregister(GF_Node *n, GF_Node *parent_node);

/*! unregisters all children in the given list
\param node the target parent node owning the list
\param childrenlist the list of children to unregister
*/
void gf_node_unregister_children(GF_Node *node, GF_ChildNodeItem *childrenlist);

/*! gets all parents of the node and replace the old_node by the new node in all parents
\note if the new node is not DEFed, only the first instance of "old_node" will be replaced, the other ones deleted
\param old_node old node to replace
\param new_node new node to replace with
\param updateOrderedGroup if GF_TRUE, update the order field of parent OrderdedGroup nodes
\return error if any
*/
GF_Err gf_node_replace(GF_Node *old_node, GF_Node *new_node, Bool updateOrderedGroup);

/*! gets the number of node instances
\param n the target node
\return the number of node instances
*/
u32 gf_node_get_num_instances(GF_Node *n);


/*! calls node traverse callback routine on this node
\param n the target node
\param udta opaque data passed to the node traverse callback
*/
void gf_node_traverse(GF_Node *n, void *udta);

/*! allows a node to be re-rendered - by default a node in its render callback will never be retraversed a second time.
Use this function to enable a second traverse for this node while traversing it
\param n the target node
*/
void gf_node_allow_cyclic_traverse(GF_Node *n);

/*! sets the cyclic traverse flag
\param n the target node
\param on indicates if cyclic traverse shall be turned on or off
\return GF_FALSE if flag was already set, GF_TRUE otherwise*/
Bool gf_node_set_cyclic_traverse_flag(GF_Node *n, Bool on);

/*! blindly calls traverse callback on all children nodes
\param n the target node
\param udta opaque data passed to the node traverse callback
*/
void gf_node_traverse_children(GF_Node *n, void *udta);
/*! get the number of parent for this node (parent are kept regardless of DEF state)
\param n the target node
\return the number of parents*/
u32 gf_node_get_parent_count(GF_Node *n);
/*! returns desired parent for this node (parent are kept regardless of DEF state)
\param n the target node
\param idx 0-based parent index
\return the parent node, or NULL of error*/
GF_Node *gf_node_get_parent(GF_Node *n, u32 idx);

/*! checks if a node belongs to a subtree
\param parent the target parent node
\param target the target node
\return GF_TRUE if target is in the subtree below parent, GF_FALSE otherwise*/
Bool gf_node_parent_of(GF_Node *parent, GF_Node *target);

/*! node dirty flags*/
enum
{
	/*flag set whenever a field of the node has been modified*/
	GF_SG_NODE_DIRTY = 1,
	/*flag set whenever a child node of this node has been modified
	\note Unloaded extern protos always invalidate their parent subgraph to get a chance
	of being loaded. It is the user responsibility to clear the CHILD_DIRTY flag before traversing
	if relying on this flag for sub-tree discarding (eg, culling or similar)*/
	GF_SG_CHILD_DIRTY = 1<<1,

	/*flag set by bindable nodes to indicate a modification of the bindable stack. This is
	only used for offscreen rendering of Layer3D*/
	GF_SG_VRML_BINDABLE_DIRTY = 1<<2,

	/*flag set whenever a ColorTransform node is removed from a parent node*/
	GF_SG_VRML_COLOR_DIRTY = 1<<3,


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
	GF_SG_SVG_SOLIDCOLOR_OR_OPACITY_DIRTY		= 1<<14,
	GF_SG_SVG_STOPCOLOR_OR_OPACITY_DIRTY		= 1<<15,
	GF_SG_SVG_STROKE_DIRTY			= 1<<16,
	GF_SG_SVG_STROKEDASHARRAY_DIRTY	= 1<<17,
	GF_SG_SVG_STROKEDASHOFFSET_DIRTY= 1<<18,
	GF_SG_SVG_STROKELINECAP_DIRTY	= 1<<19,
	GF_SG_SVG_STROKELINEJOIN_DIRTY	= 1<<20,
	GF_SG_SVG_STROKEMITERLIMIT_DIRTY= 1<<21,
	GF_SG_SVG_STROKEOPACITY_DIRTY	= 1<<22,
	GF_SG_SVG_STROKEWIDTH_DIRTY		= 1<<23,
	GF_SG_SVG_TEXTPOSITION_DIRTY	= 1<<24,
	GF_SG_SVG_DISPLAY_DIRTY			= 1<<25,
	GF_SG_SVG_VECTOREFFECT_DIRTY	= 1<<26,
	GF_SG_SVG_XLINK_HREF_DIRTY		= 1<<27,
};

/*! sets dirty flags
\param n the target node
\param flags if 0, sets the base flags on (GF_SG_NODE_DIRTY); otherwise, adds the flags to the node dirty state
\param dirty_parents if GF_TRUE, all parent subtrees for this node are marked as GF_SG_CHILD_DIRTY
\note parent subtree marking aborts if a node in the subtree is already marked with GF_SG_CHILD_DIRTY
which means tat if you never clean the dirty flags, no propagation will take place
*/
void gf_node_dirty_set(GF_Node *n, u32 flags, Bool dirty_parents);

/*! marks all parent subtrees for this node as GF_SG_CHILD_DIRTY
\note parent subtree marking aborts if a node in the subtree is already marked with GF_SG_CHILD_DIRTY
which means that if you never clean the dirty flags, no propagation will take place
\param n the target node
*/
void gf_node_dirty_parents(GF_Node *n);

/*! sets dirty flag off. It is the user responsibility to clear dirty flags
\param n the target node
\param flags if 0, all flags are set off; otherwise, removes the indicated flags from the node dirty state
*/
void gf_node_dirty_clear(GF_Node *n, u32 flags);

/*! reset dirty state of a node
\param n the target node
\param reset_children if GF_TRUE and node was dirty, resets the state of all its children
*/
void gf_node_dirty_reset(GF_Node *n, Bool reset_children);

/*! gets dirty flag value
\param n the target node
\return the dirty fkags
*/
u32 gf_node_dirty_get(GF_Node *n);

/*! VRML/BIFS route object*/
typedef struct _route GF_Route;


/*! Node Field/attribute information for VRML/BIFS/SVG
\note all scene graph implementations should answer node field query with this interface.
In case an implementation does not use this:
	- the implementation shall handle the parent node dirty flag itself most of the time
	- the implementation shall NOT allow referencing of a graph node in a parent graph node (when inlining
content) otherwise the app is guaranteed to crash.

other fieldTypes may be ignored by implmentation not using VRML/MPEG4 native types
*/
typedef struct
{
	/*! 0-based index of the field in the node*/
	u32 fieldIndex;
	/*! field type - VRML/MPEG4 types are listed in scenegraph_vrml.h*/
	u32 fieldType;
	/*! far ptr to the field (eg GF_Node **, GF_List**, MFInt32 *, ...)*/
	void *far_ptr;
	/*! field name*/
	const char *name;
	/*! NDT type in case of SF/MFNode field - cf BIFS specific tools*/
	u32 NDTtype;
	/*! event type*/
	u32 eventType;
	/*! eventin handler if any*/
	void (*on_event_in)(GF_Node *pNode, GF_Route *from_route);
} GF_FieldInfo;

/*! returns number of field for this node.
For BIFS/VRML/X3D, this is the number of defined fields by the spec.
For SVG/DOM, this is the number of attributes defined for the node.
\param n the target node
\return the number of defined fields
*/
u32 gf_node_get_field_count(GF_Node *n);

/*! fills the field info structure for the given field
\param n the target node
\param FieldIndex the 0-based index of the target field
\param info filled with field info
\return error if any
*/
GF_Err gf_node_get_field(GF_Node *n, u32 FieldIndex, GF_FieldInfo *info);

/*! gets the field by its name
\param n the target node
\param name name of the target field
\param field filled with field info
\return error if any
*/
GF_Err gf_node_get_field_by_name(GF_Node *n, char *name, GF_FieldInfo *field);

/*! Scenegraph structure*/
typedef struct __tag_scene_graph GF_SceneGraph;

/*! creates a new scene graph
\return a new scene graph
*/
GF_SceneGraph *gf_sg_new();

/*! creates a sub scene graph (typically used with Inline node): independent graph with same private stack,
and user callbacks as parent. All routes triggered in this subgraph are executed in the parent graph (this
means you only have to activate routes on the main graph)
\note The resulting graph is not destroyed when the parent graph is
\param scene the parent scene graph
\return a new scene graph
*/
GF_SceneGraph *gf_sg_new_subscene(GF_SceneGraph *scene);

/*! destroys a scene graph
\param sg the target scene graph
*/
void gf_sg_del(GF_SceneGraph *sg);
/*! resets the graph - all nodes, routes and protos are destroyed
\param sg the target scene graph
*/
void gf_sg_reset(GF_SceneGraph *sg);

/*! sets user private data for scene graph
\param sg the target scene graph
\param udta user private data to set
*/
void gf_sg_set_private(GF_SceneGraph *sg, void *udta);
/*! gets user private data of scene graph
\param sg the target scene graph
\return user private data
*/
void *gf_sg_get_private(GF_SceneGraph *sg);

/*! sets the scene time query callback (functions returns time in sec)
\param sg the target scene graph
\param GetSceneTime the scene time query callback
*/
void gf_sg_set_scene_time_callback(GF_SceneGraph *sg, Double (*GetSceneTime)(void *user_priv) );

/*! gets the parent scene graph of a graph
\param sg the target scene graph
\return the parent graph or NULL if none defined
*/
GF_SceneGraph *gf_sg_get_parent(GF_SceneGraph *sg);

/*! node callback type*/
typedef enum
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
	//node is being destroyed
	GF_SG_CALLBACK_NODE_DESTROY,
} GF_SGNodeCbkType;

/*! node callback function for scene graph
\param udta user private data of scene graph, see \ref gf_sg_set_private
\param type the type of callback
\param node the target node for the callback
\param ctxdata associated data, type depends on the callback type
*/
typedef void (*gf_sg_node_init_callback)(void *udta, GF_SGNodeCbkType type, GF_Node *node, void *ctxdata);

/*! sets node callback: function called upon node creation.
Application should instantiate the node rendering stack and any desired callback
\param sg the target scene graph
\param NodeCallback the node callback function
*/
void gf_sg_set_node_callback(GF_SceneGraph *sg, gf_sg_node_init_callback NodeCallback);

/*! gets the root node of the graph
\param sg the target scene graph
\return root node of the scene graph, NULL otherwise
*/
GF_Node *gf_sg_get_root_node(GF_SceneGraph *sg);
/*! sets the root node of the graph
\param sg the target scene graph
\param node root node of the scene graph
*/
void gf_sg_set_root_node(GF_SceneGraph *sg, GF_Node *node);

/*! finds a registered node by ID
\param sg the target scene graph
\param nodeID ID of the node to find
\return node if found, NULL otherwise
*/
GF_Node *gf_sg_find_node(GF_SceneGraph *sg, u32 nodeID);
/*! finds a registered node by DEF name
\param sg the target scene graph
\param name name of the node to find
\return node if found, NULL otherwise
*/
GF_Node *gf_sg_find_node_by_name(GF_SceneGraph *sg, char *name);

/*! signals node has been modified, indicating which field is modified
\note this is exposed for BIFS codec and BIFS/VRML rendering, it should not be needed by other apps
\param n the target node
\param fieldChanged the modified field, may be NULL of global node modification
*/
void gf_node_changed(GF_Node *n, GF_FieldInfo *fieldChanged);

/*! gets the scene graph of a node
\param n the target node
\return the parent scene graph
*/
GF_SceneGraph *gf_node_get_graph(GF_Node *n);

/*! sets size info for the graph - by default graphs have no size and are in meter metrics (VRML like)
if any of width or height is 0, the graph has no size info
\param sg the target scene graph
\param width width in pixels
\param height height in pixels
\param usePixelMetrics indicates coordinates in graph are given in pixels
*/
void gf_sg_set_scene_size_info(GF_SceneGraph *sg, u32 width, u32 height, Bool usePixelMetrics);
/*! checks if pixel metrics is used
\param sg the target scene graph
\return GF_TRUE if pixelMetrics*/
Bool gf_sg_use_pixel_metrics(GF_SceneGraph *sg);

/*! gets size information of graph
\param sg the target scene graph
\param width set to width of scene
\param height set to height of scene
\return GF_FALSE if no size info, otherwise GF_TRUE and set width/height*/
Bool gf_sg_get_scene_size_info(GF_SceneGraph *sg, u32 *width, u32 *height);

/*! creates a node of the given tag. sg is the parent scenegraph of the node,
eg the root one for scene nodes or the proto one for proto code (cf proto)
\note
	- NODE IS NOT REGISTERED (no instances) AND CANNOT BE DESTROYED UNTIL REGISTERED
	- this doesn't perform application setup for the node, this must be done by the caller

\param sg the target scene graph
\param tag tag of node to create
\return new node
*/
GF_Node *gf_node_new(GF_SceneGraph *sg, u32 tag);
/*! inits node (either internal stack or user-defined) - usually called once the node has been fully loaded
\param n the target node
*/
void gf_node_init(GF_Node *n);

/*! clones a node in the given graph and register with parent cloned.
\param inScene the target scene graph in which the node should be cloned
\param orig the target node to clone
\param cloned_parent the parent of the node to clone
\param id_suffix if NULL, all IDs are removed from the cloned subtree, (each node instance will become a hard copy). If empty string "", ID will be kept exactly as they where in the original subtree (this may lead to errors due to the presence of the same ID depending on the standard). Otherwise, all IDs are translated ($(name) -> $(name)id_suffix) and binary IDs are generated on the fly
\param deep clones children as well
\return the cloned node
*/
GF_Node *gf_node_clone(GF_SceneGraph *inScene, GF_Node *orig, GF_Node *cloned_parent, char *id_suffix, Bool deep);

/*! gets scene time for scene this node belongs too
\param n the target node
\return the scene time for the node, 0 if timeline not specified*/
Double gf_node_get_scene_time(GF_Node *n);

/*! gets next available NodeID
\param sg the target scene graph
\return next available NodeID
*/
u32 gf_sg_get_next_available_node_id(GF_SceneGraph *sg);
/*! gets max ID used in graph
\param sg the target scene graph
\return max NodeID
*/
u32 gf_sg_get_max_node_id(GF_SceneGraph *sg);

/*! gets node ID and name
\param n the target node
\param ID set to the node ID or to 0 if not defined
\return the node name or NULL if not defined
*/
const char *gf_node_get_name_and_id(GF_Node*n, u32 *ID);

/*! SVG focus types*/
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

/*! JS API Url structure*/
typedef struct
{
	/*for GF_JSAPI_OP_RESOLVE_URI,
		set by caller to the URI to resolve.
				If NULL, the return URI is the unresolved parent scene one.
				Otherwise, the input URL will be reolved to its local name (eg for ZIP/... packages)
		upon return, ALLOCATED by the callee and must be freed by the caller
	*/
	char *url;
	const char **params;
	u32 nb_params;
} GF_JSAPIURI;

/*! JS API structure for GPAC config file*/
typedef struct
{
	const char *section;
	const char *key;
	const char *key_val;
} GF_JSAPIOPT;

/*! JS API structure for script message*/
typedef struct
{
	GF_Err e;
	const char *msg;
} GF_JSAPIINFO;

/*! JS API parameter type*/
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
	GF_SceneGraph *scene;
	void *compositor;
	GF_JSAPIINFO info;
} GF_JSAPIParam;

/*! JS API action types*/
typedef enum
{
	/*!push message from script engine.*/
	GF_JSAPI_OP_MESSAGE,
	/*!resolves a given URI.*/
	GF_JSAPI_OP_RESOLVE_URI,
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
	/*!replace target scene URL*/
	GF_JSAPI_OP_LOAD_URL,
	/*!get option by section and key*/
	GF_JSAPI_OP_GET_OPT,
	/*!get option by section and key*/
	GF_JSAPI_OP_SET_OPT,
	/*!retrieve download manager*/
	GF_JSAPI_OP_GET_DOWNLOAD_MANAGER,
	/*!get navigation speed if any*/
	GF_JSAPI_OP_GET_SPEED,
	/*!get current frame rate*/
	GF_JSAPI_OP_GET_FPS,
	/*!set current title*/
	GF_JSAPI_OP_SET_TITLE,
	/*!gets subscene for current node if any*/
	GF_JSAPI_OP_GET_SUBSCENE,
	/*!resolves relative Xlink based on xml:base*/
	GF_JSAPI_OP_RESOLVE_XLINK,

	/*!gets parent filter*/
	GF_JSAPI_OP_GET_COMPOSITOR,

	/*!pauses an SVG element*/
	GF_JSAPI_OP_PAUSE_SVG,
	/*!resumes an SVG ELEMENT*/
	GF_JSAPI_OP_RESUME_SVG,
	/*!restarts an SVG ELEMENT: this restarts all the media tunning on the main timeline*/
	GF_JSAPI_OP_RESTART_SVG,
	/*!sets scene speed*/
	GF_JSAPI_OP_SET_SCENE_SPEED,
	/*!gets the DPI*/
	GF_JSAPI_OP_GET_DPI_X,
	GF_JSAPI_OP_GET_DPI_Y,
} GF_JSAPIActionType;

/*! interface to various get/set options:
\param callback opaque data passed to the function
\param type operand type, one of the above
\param node target node, scene root node or NULL
\param param i/o param, depending on operand type
\return GF_TRUE if success, GF_FALSE otherwise
*/
typedef Bool (*gf_sg_script_action)(void *callback, GF_JSAPIActionType type, GF_Node *node, GF_JSAPIParam *param);

/*! assigns API to scene graph - by default, sub-graphs inherits the API if set
\param sg the target scene graph
\param script_act the script action callback
\param cbk opaque data to pass to the script action callback
*/
void gf_sg_set_script_action(GF_SceneGraph *sg, gf_sg_script_action script_act, void *cbk);

/*! loads script into engine - this should be called only for script in main scene, loading of scripts
in protos is done internally when instanciating the proto
\param script the target script node*/
void gf_sg_script_load(GF_Node *script);

/*! checks if scripting is supported in GPAC (built-in and run-time enabled)
\return GF_TRUE if javascript is supported*/
Bool gf_sg_has_scripting();

/*! Scene command tags

scene graph command tools used for BIFS and LASeR
These are used to store updates in memory without applying changes to the graph, for dumpers, encoders ...
The commands can then be applied through this lib
*/
enum
{
	GF_SG_RESERVED = 0,

#ifndef GPAC_DISABLE_VRML

	/*BIFS commands*/
	GF_SG_SCENE_REPLACE,
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

	/*BIFS*/
	GF_SG_XREPLACE,

#endif
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
	GF_SG_LSR_ACTIVATE,
	GF_SG_LSR_DEACTIVATE,

	GF_SG_UNDEFINED
};


/*! structure used to store field info, pos and static pointers to GF_Node/MFNode in commands*/
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

/*! structure used to store decoded BIFS command

\note In order to maintain node registry, the nodes replaced/inserted MUST be registered with
  their parents even when the command is never applied. Registering shall be performed
  with gf_node_register (see below).
  If you fail to do so, a node may be destroyed when destroying a command while still used
  in another command or in the graph - this will just crash.

*/
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


	union {
		/*scene replace command:
			root node is stored in com->node
			protos are stored in com->new_proto_list
			routes are stored as RouteInsert in the same frame
			BIFS only
		*/
		Bool use_names;
		u32 RouteID;
		s32 ChildNodeTag;
	};

	/*proto list to insert - BIFS only*/
	GF_List *new_proto_list;
	/*proto ID list to delete - BIFS only*/
	u32 *del_proto_list;
	union {
		u32 del_proto_list_size;
		u32 child_field;
	};

	union {
		char *def_name;
		char *send_event_string;
	};
	union {
		//route insertion - fromNodeID is also used to identify operandElementId in LASeR Add/Replace
		u32 fromNodeID;
		s32 send_event_integer;
	};
	union {
		u32 fromFieldIndex;
		u32 send_event_name;
	};

	union {
		u32 toNodeID;
		s32 send_event_x;
	};
	union {
		u32 toFieldIndex;
		s32 send_event_y;
	};
	Bool aggregated;
	/*some commands need to never be applied; for instance when building an aggregate carousel*/
	Bool never_apply;
} GF_Command;


/*! creates a command - graph is only needed for SceneReplace
\param sg parent scene graph of the command, only needed for SceneReplace
\param tag the command tag
\return a new command
*/
GF_Command *gf_sg_command_new(GF_SceneGraph *sg, u32 tag);
/*! destroys a command
\param com the target command
*/
void gf_sg_command_del(GF_Command *com);
/*! applies command to graph - the command content is kept unchanged for authoring purposes - THIS NEEDS TESTING AND FIXING
\param sg the target scene graph where to apply the command
\param com the target command
\param time_offset time offset in seconds for time fields if desired
\return error if any
*/
GF_Err gf_sg_command_apply(GF_SceneGraph *sg, GF_Command *com, Double time_offset);

/*! applies list if command to graph - the command content is kept unchanged for authoring purposes
\param sg the target scene graph where to apply the command
\param comList the list of commands
\param time_offset time offset in seconds for time fields if desired
\return error if any
*/
GF_Err gf_sg_command_apply_list(GF_SceneGraph *sg, GF_List *comList, Double time_offset);

/*! creates a new command field structire (some commands may target multiple fields at once) and registers it with command
\param com the parent command
\return new commandFieldInfo structure*/
GF_CommandField *gf_sg_command_field_new(GF_Command *com);

/*! XML node from DOM parser */
typedef struct _xml_node *GF_DOMXMLNODE;

/*! Creates a GF_SceneGraph from an XML root node. This is mostly used to allow using DOM API on an XML doc
 \param document an empty scene graph object
 \param root_node the root node of an XML document
 \return error if any
 */
GF_Err gf_sg_init_from_xml_node(GF_SceneGraph *document, GF_DOMXMLNODE root_node);


/*! @} */

#ifdef __cplusplus
}
#endif



#endif /*_GF_SCENEGRAPH_H_*/


