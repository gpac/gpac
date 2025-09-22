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


#ifndef _GF_SG_VRML_H_
#define _GF_SG_VRML_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
\file <gpac/scenegraph_vrml.h>
\brief Scenegraph for VRML files
*/
	
/*!
\addtogroup svrml BIFS/VRML/X3D Scenegraph
\ingroup scene_grp
\brief Scenegraph for VRML files.

This section documents the Scenegraph for VRML files.

@{
 */


#include <gpac/scenegraph.h>
#include <gpac/maths.h>

/*
	All extensions for VRML/MPEG-4/X3D graph structure
*/

/*! reserved NDT for MPEG4 (match binary coding)*/
#define MPEG4_RESERVED_NDT		200

/*! the NDTs used in X3D not defined in MPEG4*/
enum
{
	NDT_SFMetadataNode = MPEG4_RESERVED_NDT+1,
	NDT_SFFillPropertiesNode,
	NDT_SFX3DLinePropertiesNode,
	NDT_SFGeoOriginNode,
	NDT_SFHAnimNode,
	NDT_SFHAnimDisplacerNode,
	NDT_SFNurbsControlCurveNode,
	NDT_SFNurbsSurfaceNode,
	NDT_SFNurbsCurveNode
};

/*
	VRML / BIFS TYPES DEFINITION
*/

/*! event types of fields, as defined in the specs
	this should not be needed by non binary codecs
*/
enum
{
	GF_SG_EVENT_FIELD		=	0,
	GF_SG_EVENT_EXPOSED_FIELD	=	1,
	GF_SG_EVENT_IN		=	2,
	GF_SG_EVENT_OUT		=	3,
	GF_SG_EVENT_UNKNOWN	=	4
};
/*! gets the event type name
\param EventType the event type
\param forX3D for X3D dumping
\return the event name
*/
const char *gf_sg_vrml_get_event_type_name(u32 EventType, Bool forX3D);

/*! field coding mode

	BIFS defines the bitstream syntax contextually, and therefore sometimes refer to fields as indexed
  in the node ("all" mode) or just as a sub-set (in, out, def, dyn modes) of similar types
*/
enum
{
	/*all fields and events*/
	GF_SG_FIELD_CODING_ALL		=	0,
	/*defined fields (exposedField and Field)*/
	GF_SG_FIELD_CODING_DEF		=	1,
	/*input field (exposedField and eventIn)*/
	GF_SG_FIELD_CODING_IN		=	2,
	/*output field (exposedField and eventOut)*/
	GF_SG_FIELD_CODING_OUT		=	3,
	/*field that can be animated (subset of inFields) used in BIFS_Anim only*/
	GF_SG_FIELD_CODING_DYN		=	4
};

/*! gets the number of field in the given mode (BIFS specific)
\param n the target node
\param IndexMode the field indexing mode
\return the number of field in this mode*/
u32 gf_node_get_num_fields_in_mode(GF_Node *n, u8 IndexMode);

/*		SF Types	*/
/*! Boolean*/
typedef u32 SFBool;
/*! Integer*/
typedef s32 SFInt32;
/*! Integer*/
typedef s32 SFInt;
/*! Float*/
typedef Fixed SFFloat;
/*! Double*/
typedef Double SFDouble;

/*! String*/
typedef struct
{
	char* buffer;
} SFString;

/*! Time*/
typedef Double SFTime;

/*! RGB color*/
typedef struct {
	Fixed	red;
	Fixed	green;
	Fixed	blue;
} SFColor;

/*! RGBA color*/
typedef struct {
	Fixed	red;
	Fixed	green;
	Fixed	blue;
	Fixed	alpha;
} SFColorRGBA;

/*! URL*/
typedef struct {
	u32 OD_ID;
	char *url;
} SFURL;

/*! 2D vector (double)*/
typedef struct {
	Double	x;
	Double	y;
} SFVec2d;

/*! 3D vector (double)*/
typedef struct {
	Double	x;
	Double	y;
	Double	z;
} SFVec3d;

/*typedef's to main math tools*/

/*! 2D vector (float)*/
typedef struct __vec2f SFVec2f;
/*! 3D vector (float)*/
typedef struct __vec3f SFVec3f;
/*! rotation (float)*/
typedef struct __vec4f SFRotation;
/*! 4D vector (float)*/
typedef struct __vec4f SFVec4f;

/*! Image data (rgb pixels)*/
typedef struct {
	u32 width;
	u32 height;
	u8 numComponents;
	unsigned char* pixels;
} SFImage;
/*! BIFS Command Buffer*/
typedef struct {
	u32 bufferSize;
	u8* buffer;
	/*uncompressed command list*/
	GF_List *commandList;
} SFCommandBuffer;

/*! Script
 	\note The javascript or vrml script is handled in its decompressed (textual) format
since most JS interpreter work with text*/
typedef struct {
	char* script_text;
} SFScript;

/*! BIFS Attribute Reference*/
typedef struct {
	GF_Node *node;
	u32 fieldIndex;
} SFAttrRef;

/*		MF Types	*/

/*! generic MF field: all MF fields use the same syntax except MFNode which uses GF_List. You  can thus use
this structure to safely typecast MF field pointers*/
typedef struct {
	u32 count;
	u8 *array;
} GenMFField;
/*! Interger array*/
typedef struct {
	u32 count;
	s32* vals;
} MFInt32;
/*! Interger array*/
typedef struct {
	u32 count;
	s32* vals;
} MFInt;
/*! Float array*/
typedef struct {
	u32 count;
	Fixed *vals;
} MFFloat;
/*! Double array*/
typedef struct {
	u32 count;
	Double *vals;
} MFDouble;
/*! Boolean array*/
typedef struct {
	u32 count;
	u32* vals;
} MFBool;
/*! Color RGB array*/
typedef struct {
	u32 count;
	SFColor* vals;
} MFColor;
/*! Color RGBA array*/
typedef struct {
	u32 count;
	SFColorRGBA* vals;
} MFColorRGBA;
/*! Rotation array*/
typedef struct {
	u32 count;
	SFRotation*	vals;
} MFRotation;
/*! Time array*/
typedef struct {
	u32 count;
	Double* vals;
} MFTime;
/*! 2D Vector (float) array*/
typedef struct {
	u32 count;
	SFVec2f* vals;
} MFVec2f;
/*! 2D Vector (double) array*/
typedef struct {
	u32 count;
	SFVec2d* vals;
} MFVec2d;
/*! 3D Vector (float) array*/
typedef struct {
	u32 count;
	SFVec3f* vals;
} MFVec3f;
/*! 3D Vector (double) array*/
typedef struct {
	u32 count;
	SFVec3d* vals;
} MFVec3d;
/*! 4D Vector (float) array*/
typedef struct {
	u32 count;
	SFVec4f* vals;
} MFVec4f;

/*! URL array*/
typedef struct {
	u32 count;
	SFURL* vals;
} MFURL;
/*! String array*/
typedef struct {
	u32 count;
	char** vals;
} MFString;
/*! Script array*/
typedef struct {
	u32 count;
	SFScript *vals;
} MFScript;

/*! Attribute Reference array*/
typedef struct {
	u32 count;
	SFAttrRef* vals;
} MFAttrRef;

/*! converts an SFColor to an SFColorRGBA by setting the alpha component to 1
\param val the input color
\return the RGBA color*/
SFColorRGBA gf_sg_sfcolor_to_rgba(SFColor val);

/*! field types, as defined in BIFS encoding (used for scripts and proto coding)*/
enum
{
	GF_SG_VRML_SFBOOL		=	0,
	GF_SG_VRML_SFFLOAT		=	1,
	GF_SG_VRML_SFTIME		=	2,
	GF_SG_VRML_SFINT32		=	3,
	GF_SG_VRML_SFSTRING		=	4,
	GF_SG_VRML_SFVEC3F		=	5,
	GF_SG_VRML_SFVEC2F		=	6,
	GF_SG_VRML_SFCOLOR		=	7,
	GF_SG_VRML_SFROTATION	=	8,
	GF_SG_VRML_SFIMAGE		=	9,
	GF_SG_VRML_SFNODE		=	10,
	/*TO CHECK*/
	GF_SG_VRML_SFVEC4F		=	11,

	/*used types in GPAC but not defined in the MPEG4 spec*/
	GF_SG_VRML_SFURL,
	GF_SG_VRML_SFSCRIPT,
	GF_SG_VRML_SFCOMMANDBUFFER,
	/*used types in X3D*/
	GF_SG_VRML_SFDOUBLE,
	GF_SG_VRML_SFCOLORRGBA,
	GF_SG_VRML_SFVEC2D,
	GF_SG_VRML_SFVEC3D,

	GF_SG_VRML_FIRST_MF		= 32,
	GF_SG_VRML_MFBOOL		= GF_SG_VRML_FIRST_MF,
	GF_SG_VRML_MFFLOAT,
	GF_SG_VRML_MFTIME,
	GF_SG_VRML_MFINT32,
	GF_SG_VRML_MFSTRING,
	GF_SG_VRML_MFVEC3F,
	GF_SG_VRML_MFVEC2F,
	GF_SG_VRML_MFCOLOR,
	GF_SG_VRML_MFROTATION,
	GF_SG_VRML_MFIMAGE,
	GF_SG_VRML_MFNODE,
	GF_SG_VRML_MFVEC4F,

	GF_SG_VRML_SFATTRREF	=	45,
	GF_SG_VRML_MFATTRREF	=	46,

	/*used types in GPAC but not defined in the MPEG4 spec*/
	GF_SG_VRML_MFURL,
	GF_SG_VRML_MFSCRIPT,
	GF_SG_VRML_MFCOMMANDBUFFER,

	/*used types in X3D*/
	GF_SG_VRML_MFDOUBLE,
	GF_SG_VRML_MFCOLORRGBA,
	GF_SG_VRML_MFVEC2D,
	GF_SG_VRML_MFVEC3D,

	/*special event only used in routes for binding eventOut/exposedFields to script functions.
	 A route with ToField.FieldType set to this value holds a pointer to a function object.
	*/
	GF_SG_VRML_SCRIPT_FUNCTION,

	/*special event only used in routes for binding eventOut/exposedFields to generic functions.
	 A route with ToField.FieldType set to this value holds a pointer to a function object.
	*/
	GF_SG_VRML_GENERIC_FUNCTION,

	GF_SG_VRML_UNKNOWN
};

/*! checks if a field is an SF field
\param FieldType the tragte filed type
\return GF_TRUE if field is a single field*/
Bool gf_sg_vrml_is_sf_field(u32 FieldType);

/*! translates MF/SF to SF type
\param FieldType the tragte filed type
\return SF field type*/
u32 gf_sg_vrml_get_sf_type(u32 FieldType);

/*! inserts (+alloc) a slot in the MFField with a specified position for insertion and sets the ptr to the newly created slot
\param mf pointer to the MF field
\param FieldType the MF field type
\param new_ptr set to the allocated slot (do not free)
\param InsertAt is the 0-based index for the new slot
\return error if any
*/
GF_Err gf_sg_vrml_mf_insert(void *mf, u32 FieldType, void **new_ptr, u32 InsertAt);
/*! removes all items of the MFField
\param mf pointer to the MF field
\param FieldType the MF field type
\return error if any
*/
GF_Err gf_sg_vrml_mf_reset(void *mf, u32 FieldType);

/*! deletes an MFUrl field
\note exported for URL handling in compositor
\param url the MF url field to reset
*/
void gf_sg_mfurl_del(MFURL url);
/*! copies MFUrl field
\note exported for URL handling in compositor
\param dst the destination MF url field to copy
\param src the source MF url field to copy
*/
void gf_sg_vrml_copy_mfurl(MFURL *dst, MFURL *src);
/*! interpolates SFRotation
\note exported for 3D camera in compositor
\param kv1 start value for interpolation
\param kv2 end value for interpolation
\param f interpolation factor
\return the interpolated result
*/
SFRotation gf_sg_sfrotation_interpolate(SFRotation kv1, SFRotation kv2, Fixed f);

/*! adds a new node to the "children" field
\warning DOES NOT CHECK CHILD/PARENT type compatibility
\param parent the target parent node
\param new_child the child to insert
\param pos the 0-BASED index in the list of children, -1 means end of list (append)
\return error if any
*/
GF_Err gf_node_insert_child(GF_Node *parent, GF_Node *new_child, s32 pos);

/*! removes and replace given child by specified node. If node is NULL, only delete target node
\warning DOES NOT CHECK CHILD/PARENT type compatibility
\param node the target node to replace
\param container the container list
\param pos the 0-BASED index in the list of children, -1 means end of list (append)
\param newNode the new node to use
\return error if any
*/
GF_Err gf_node_replace_child(GF_Node *node, GF_ChildNodeItem **container, s32 pos, GF_Node *newNode);


/*! internal prototype*/
#define GF_SG_INTERNAL_PROTO	(PTR_TO_U_CAST -1)


#ifndef GPAC_DISABLE_VRML


/*! VRML grouping nodes macro - note we have inverted the children field to be
compatible with the base GF_ParentNode node
All grouping nodes (with "children" field) implement the following:

addChildren: chain containing nodes to add passed as eventIn - handled internally through ROUTE
void (*on_addChildren)(GF_Node *pNode): add feventIn signaler - this is handled internally by the scene_graph and SHALL
NOT BE overridden since it takes care of node(s) routing

removeChildren: chain containing nodes to remove passed as eventIn - handled internally through ROUTE

void (*on_removeChildren)(GF_Node *pNode): remove eventIn signaler - this is handled internally by the scene_graph and SHALL
NOT BE overridden since it takes care of node(s) routing

children: list of children SFNodes
*/
#define VRML_CHILDREN							\
	CHILDREN									\
	GF_ChildNodeItem *addChildren;							\
	void (*on_addChildren)(GF_Node *pNode, struct _route *route);		\
	GF_ChildNodeItem *removeChildren;						\
	void (*on_removeChildren)(GF_Node *pNode, struct _route *route);		\

/*! generic VRML parent node*/
typedef struct
{
	BASE_NODE
	VRML_CHILDREN
} GF_VRMLParent;

/*! setup a vrml parent
\param n the target node*/
void gf_sg_vrml_parent_setup(GF_Node *n);
/*! resets all children in a vrml parent node (but does not destroy the node)
\param n the target node*/
void gf_sg_vrml_parent_destroy(GF_Node *n);

/*! checks if a given node tag is in a given NDT table
\param tag the node tag
\param NDTType the NDT type
\return GF_TRUE if node tag is in the NDT*/
Bool gf_node_in_table_by_tag(u32 tag, u32 NDTType);
/*! gets field type name
\param FieldType the field type
\return the field name*/
const char *gf_sg_vrml_get_field_type_name(u32 FieldType);

/*! allocates a new field
\note GF_SG_VRML_MFNODE will return a pointer to a GF_List structure (eg GF_List *), GF_SG_VRML_SFNODE will return NULL
\param FieldType the field type
\return the new field pointer*/
void *gf_sg_vrml_field_pointer_new(u32 FieldType);
/*! deletes a field pointer (including SF an,d MF nodes)
\param field the field pointer value
\param FieldType the field type
*/
void gf_sg_vrml_field_pointer_del(void *field, u32 FieldType);

/*! adds at the end of an MF field
\param mf pointer to the MF field
\param FieldType the MF field type
\param new_ptr set to the allocated SF field slot - do not destroy
\return error if any
*/
GF_Err gf_sg_vrml_mf_append(void *mf, u32 FieldType, void **new_ptr);
/*! removes the desired item of an MF field
\param mf pointer to the MF field
\param FieldType the MF field type
\param RemoveFrom the 0-based index of item to remove
\return error if any
*/
GF_Err gf_sg_vrml_mf_remove(void *mf, u32 FieldType, u32 RemoveFrom);
/*! allocates an MF array
\param mf pointer to the MF field
\param FieldType the MF field type
\param NbItems number of items to allocate
\return error if any
*/
GF_Err gf_sg_vrml_mf_alloc(void *mf, u32 FieldType, u32 NbItems);
/*! gets the item in the MF array
\param mf pointer to the MF field
\param FieldType the MF field type
\param new_ptr set to the SF field slot - do not destroy
\param ItemPos the 0-based index of item to remove
\return error if any
*/
GF_Err gf_sg_vrml_mf_get_item(void *mf, u32 FieldType, void **new_ptr, u32 ItemPos);

/*! copies a field content EXCEPT SF/MFNode. Pointers to field shall be used
\param dest pointer to the MF field
\param orig pointer to the MF field
\param FieldType the MF field type
*/
void gf_sg_vrml_field_copy(void *dest, void *orig, u32 FieldType);

/*! clones a field content EXCEPT SF/MFNode. Pointers to field shall be used
\param dest pointer to the MF field
\param orig pointer to the MF field
\param FieldType the MF field type
\param inScene target scene graph for SFCommandBuffers cloning
*/
void gf_sg_vrml_field_clone(void *dest, void *orig, u32 FieldType, GF_SceneGraph *inScene);

/*! indicates whether 2 fields of same type EXCEPT SF/MFNode are equal
\param dest pointer to the MF field
\param orig pointer to the MF field
\param FieldType the MF field type
\return GF_TRUE if fields equal
*/
Bool gf_sg_vrml_field_equal(void *dest, void *orig, u32 FieldType);


/*GF_Route manipultaion : routes are used to pass events between nodes. Event handling is managed by the scene graph
however only the nodes overloading the EventIn handler associated with the event will process the eventIn*/

/*! creates a new route
\note routes are automatically destroyed if either the target or origin node of the route is destroyed
\param sg the target scene graph of the route
\param fromNode the source node triggering the event out
\param fromField the source field triggering the event out
\param toNode the destination node accepting the event in
\param toField the destination field accepting the event in
\return a new route object
*/
GF_Route *gf_sg_route_new(GF_SceneGraph *sg, GF_Node *fromNode, u32 fromField, GF_Node *toNode, u32 toField);

/*! destroys a route
\param route the target route
*/
void gf_sg_route_del(GF_Route *route);
/*! destroys a route by ID
\param sg the scene graph of the route
\param routeID the ID of the route to destroy
\return error if any
*/
GF_Err gf_sg_route_del_by_id(GF_SceneGraph *sg,u32 routeID);

/*! locate a route by ID
\param sg the scene graph of the route
\param RouteID the ID of the route
\return the route object or NULL if not found
*/
GF_Route *gf_sg_route_find(GF_SceneGraph *sg, u32 RouteID);
/*! locate a route by name
\param sg the scene graph of the route
\param name the name of the route
\return the route object or NULL if not found
*/
GF_Route *gf_sg_route_find_by_name(GF_SceneGraph *sg, char *name);
/*! assigns a route ID
\param route the target route
\param ID the ID to assign
\return error if any - fails if a route with same ID already exists*/
GF_Err gf_sg_route_set_id(GF_Route *route, u32 ID);

/*! assign a route name
\param route the target route
\param name the name to assign
\return error if any - fails if a route with same name already exists
*/
GF_Err gf_sg_route_set_name(GF_Route *route, char *name);
/*! gets route name
\param route the target route
\return the route name or NULL if not set
*/
char *gf_sg_route_get_name(GF_Route *route);

/*! retuns next available RouteID
\note this doesn't track inserted routes, that's the caller responsibility
\param sg the target scene graph of the route
\return the next available ID for routes
*/
u32 gf_sg_get_next_available_route_id(GF_SceneGraph *sg);
/*! sets max defined route ID used in the scene - used to handle RouteInsert commands
note that this must be called by the user to be effective,; otherwise the max route ID is computed
from the routes present in scene
\param sg the target scene graph of the route
\param ID the value of the max defined route ID
*/
void gf_sg_set_max_defined_route_id(GF_SceneGraph *sg, u32 ID);

/*! creates a new route from a node output to a given callback/function
\param sg the target scene graph of the route
\param fromNode the source node emiting the event out
\param fromField the source field emiting the event out
\param cbk opaque data to pass to the callback
\param route_callback route callback function to call
*/
void gf_sg_route_new_to_callback(GF_SceneGraph *sg, GF_Node *fromNode, u32 fromField, void *cbk, void ( *route_callback) (void *param, GF_FieldInfo *from_field) );

/*! activates all routes currently triggered - this follows the event cascade model of VRML/MPEG4:
	- routes are collected during eventOut generation
	- routes are activated. If eventOuts are generated during activation the cycle goes on.

  A route cannot be activated twice in the same simulation tick, hence this function shall be called
  ONCE AND ONLY ONCE per simulation tick

Note that children scene graphs register their routes with the top-level graph, so only the main
scene graph needs to be activated
\param sg the target scene graph of the route
*/
void gf_sg_activate_routes(GF_SceneGraph *sg);


/*
				proto handling

	The lib allows you to construct prototype nodes as defined in VRML/MPEG4 by constructing
	proto interfaces and instantiating them. An instantiated proto is handled as a single node for
	rendering, thus an application will never handle proto instances for rendering
*/

/*! proto object*/
typedef struct _proto GF_Proto;
/*! proto field object*/
typedef struct _protofield GF_ProtoFieldInterface;


/*! retuns next available proto ID
\param sg the target scene graph of the proto
\return the next available proto ID
*/
u32 gf_sg_get_next_available_proto_id(GF_SceneGraph *sg);

/*! constructs a new proto identified by ID/name in the given scene
2 protos in the same scene may not have the same ID/name

\param sg the target scene graph in which the proto is created
\param ProtoID ID of the proto to create
\param name name of the proto to create
\param unregistered if GF_TRUE, the proto is not stored in the graph main proto list but in an alternate list (used for memory handling of scene graph only). Several protos with the same ID/Name can be stored unregistered
\return a new proto object
*/
GF_Proto *gf_sg_proto_new(GF_SceneGraph *sg, u32 ProtoID, char *name, Bool unregistered);

/*! destroys a proto - can be used even if instances of the proto are still present
\param proto the target proto
\return error if any
*/
GF_Err gf_sg_proto_del(GF_Proto *proto);

/*! returns the graph associated with this proto. Such a graph cannot be used for rendering but is needed during
construction of proto dictionaries in case of nested protos
\param proto the target proto
\return associated scene graph proto the target proto
*/
GF_SceneGraph *gf_sg_proto_get_graph(GF_Proto *proto);

/*! adds node code - a proto is build of several nodes, the first node is used for rendering
and the others are kept private. This set of nodes is referred to as the proto "node code"
\param proto the target proto
\param n the node to add to the proto code
\return error if any
*/
GF_Err gf_sg_proto_add_node_code(GF_Proto *proto, GF_Node *n);

/*! gets number of field in the proto interface
\param proto the target proto
\return the number of fields
*/
u32 gf_sg_proto_get_field_count(GF_Proto *proto);
/*! locates a field declaration by name
\param proto the target proto
\param fieldName the name of the field
\return the proto field interface or NULL if not found
*/
GF_ProtoFieldInterface *gf_sg_proto_field_find_by_name(GF_Proto *proto, char *fieldName);
/*! locates field declaration by index
\param proto the target proto
\param fieldIndex 0-based index of the field to query
\return the proto field interface or NULL if not found
*/
GF_ProtoFieldInterface *gf_sg_proto_field_find(GF_Proto *proto, u32 fieldIndex);

/*! creates a new field declaration in the proto. of given fieldtype and eventType
fieldName can be NULL, if so the name will be fieldN, N being the index of the created field
\param proto the target proto
\param fieldType the data type of the field to create
\param eventType the event type of the field to create
\param fieldName the name of the field to create (may be NULL)
\return the new proto field interface
*/
GF_ProtoFieldInterface *gf_sg_proto_field_new(GF_Proto *proto, u32 fieldType, u32 eventType, char *fieldName);

/*! assigns the node field to a field of the proto (the node field IS the proto field)
the node shall be a node of the proto scenegraph, and the fieldtype/eventType of both fields shall match
(except SF/MFString and MF/SFURL which are allowed) due to BIFS semantics

\param proto the target proto
\param protoFieldIndex the proto field index to assign
\param node the node (shall be part of the proto node code) to link to
\param nodeFieldIndex the field index of the node to link to
\return error if any
*/
GF_Err gf_sg_proto_field_set_ised(GF_Proto *proto, u32 protoFieldIndex, GF_Node *node, u32 nodeFieldIndex);

/*! returns field info of the field - this is typically used to setup the default value of the field
\param field the proto field interface to query
\param info filled with the proto field interface info
\return error if any
*/
GF_Err gf_sg_proto_field_get_field(GF_ProtoFieldInterface *field, GF_FieldInfo *info);

/*
	NOTE on proto instances:
		The proto instance is handled as an GF_Node outside the scenegraph lib, and is manipulated with the same functions
		as an GF_Node
		The proto instance may or may not be loaded.
		An unloaded instance only contains the proto instance fields
		A loaded instance contains the proto instance fields plus all the proto code (Nodes, routes) and
		will load any scripts present in it. This allows keeping the memory usage of proto very low, especially
		when nested protos (protos used as building blocks of their parent proto) are used.
*/

/*! creates the proto instance without the proto code
\param sg the target scene graph of the node
\param proto the proto to instanciate
\return the new prototype instance node
*/
GF_Node *gf_sg_proto_create_instance(GF_SceneGraph *sg, GF_Proto *proto);

/*! loads code in this instance - all subprotos are automatically created, thus you must only instantiate
top-level protos. VRML/BIFS doesn't allow for non top-level proto instanciation in the main graph
All nodes created in this proto will be forwarded to the app for initialization
\param proto_inst the proto instance to load
\return error if any
*/
GF_Err gf_sg_proto_load_code(GF_Node *proto_inst);

/*! locates a prototype definition by ID or by name. when looking by name, ID is ignored
\param sg the target scene graph of the proto
\param ProtoID the ID of the proto to locate
\param name the name of the proto to locate
\return the proto node or NULL if not found
*/
GF_Proto *gf_sg_find_proto(GF_SceneGraph *sg, u32 ProtoID, char *name);

/*! deletes all protos in given scene - does NOT delete instances of protos, only the proto object is destroyed
\param sg the target scene graph
\return error if any
*/
GF_Err gf_sg_delete_all_protos(GF_SceneGraph *sg);

/*! gets proto of a prototype instance node
\param node the target prototype instance node
\return the proto node or NULL if the node is not a prototype instance or the source proto was destroyed*/
GF_Proto *gf_node_get_proto(GF_Node *node);
/*! returns the ID of a proto
\param proto the target proto
\return the proto ID
*/
u32 gf_sg_proto_get_id(GF_Proto *proto);
/*! returns the name of a proto
\param proto the target proto
\return the proto name
*/
const char *gf_sg_proto_get_class_name(GF_Proto *proto);

/*! checks if a proto instance field is an SFTime routed to a startTime/stopTime field in the proto code (MPEG-4 specific for updates)
\param node the target prototype instance node
\param field the target field info
\return GF_TRUE if this is the case
*/
Bool gf_sg_proto_field_is_sftime_offset(GF_Node *node, GF_FieldInfo *field);

/*! sets an ISed (route between proto instance and internal proto code) field in a proto instance (not a proto) - this is needed with dynamic node creation inside a proto instance (conditionals)
\param protoinst the target prototype instance node
\param protoFieldIndex field index in prototype instance node
\param node the target node
\param nodeFieldIndex field index in the target node
\return error if any
*/
GF_Err gf_sg_proto_instance_set_ised(GF_Node *protoinst, u32 protoFieldIndex, GF_Node *node, u32 nodeFieldIndex);

/*! returns root node (the one and only one being traversed) of this proto instance if any
\param node the target prototype instance node
\return the root node of the proto code - may be NULL
*/
GF_Node *gf_node_get_proto_root(GF_Node *node);

/*! indicates proto field has been parsed and its value is valid - this is needed for externProtos not specifying default
values
\param node the target prototype instance node
\param info the target field info
*/
void gf_sg_proto_mark_field_loaded(GF_Node *node, GF_FieldInfo *info);


/*! sets proto loader callback - callback user data is the same as simulation time callback

GetExternProtoLib is a pointer to the proto lib loader - this callback shall return the LPSCENEGRAPH
of the extern proto lib if found and loaded, NULL if not found and GF_SG_INTERNAL_PROTO for internal
hardcoded protos (extensions of MPEG-4 scene graph used for module deveopment)
\param sg the target scene graph
\param GetExternProtoLib the callback function
*/
void gf_sg_set_proto_loader(GF_SceneGraph *sg, GF_SceneGraph *(*GetExternProtoLib)(void *SceneCallback, MFURL *lib_url));

/*! gets a pointer to the MF URL field for externProto info - DO NOT TOUCH THIS FIELD
\param proto the target proto field
\return the MFURL associated with an extern proto, or NULL if proto is not an extern proto
*/
MFURL *gf_sg_proto_get_extern_url(GF_Proto *proto);


/*
			JavaScript tools
*/

/*! script fields type don't have the same value as the bifs ones...*/
enum
{
	GF_SG_SCRIPT_TYPE_FIELD = 0,
	GF_SG_SCRIPT_TYPE_EVENT_IN,
	GF_SG_SCRIPT_TYPE_EVENT_OUT,
};
/*! script field object*/
typedef struct _scriptfield GF_ScriptField;
/*! creates new sript field - script fields are dynamically added to the node, and thus can be accessed through the
same functions as other GF_Node fields
\param script the script node
\param eventType the event type of the new field
\param fieldType the data type of the new field
\param name the name of the new field
\return a new scritp field
*/
GF_ScriptField *gf_sg_script_field_new(GF_Node *script, u32 eventType, u32 fieldType, const char *name);
/*! retrieves field info of a script field object, useful to get the field index
\param field the script field to query
\param info filled with the field info
\return error if any
*/
GF_Err gf_sg_script_field_get_info(GF_ScriptField *field, GF_FieldInfo *info);

/*! activates eventIn for script node - needed for BIFS field replace
\param script the target script node
\param in_field the field info of the activated event in field
*/
void gf_sg_script_event_in(GF_Node *script, GF_FieldInfo *in_field);


/*! signals eventOut has been set by field index
\note Routes are automatically triggered when the event is signaled
\param n the target node emitin the event
\param FieldIndex the field index emiting the event
*/
void gf_node_event_out(GF_Node *n, u32 FieldIndex);
/*! signals eventOut has been set by event name.
\note Routes are automatically triggered when the event is signaled
\param n the target node emitin the event
\param eventName the name of the field emiting the event
*/
void gf_node_event_out_str(GF_Node *n, const char *eventName);

/*! gets MPEG-4 / VRML node tag by class name
\param node_name the node name
\return the node tag*/
u32 gf_node_mpeg4_type_by_class_name(const char *node_name);

#ifndef GPAC_DISABLE_X3D
/*! gets X3D node tag by class name
\param node_name the node name
\return the node tag*/
u32 gf_node_x3d_type_by_class_name(const char *node_name);
#endif


#endif /*GPAC_DISABLE_VRML*/


/*! check if a hardcoded prototype node acts as a grouping node
\param n the target prototype instance node
\return GF_TRUE if acting as a grouping node*/
Bool gf_node_proto_is_grouping(GF_Node *n);

/*! tags a hardcoded proto as being a grouping node
\param n the target prototype instance node
\return error if any
*/
GF_Err gf_node_proto_set_grouping(GF_Node *n);

/*! assigns callback to an eventIn field of an hardcoded proto
\param n the target prototype instance node
\param fieldIndex the target field index
\param event_in_cbk the event callback function
\return error if any
*/
GF_Err gf_node_set_proto_eventin_handler(GF_Node *n, u32 fieldIndex, void (*event_in_cbk)(GF_Node *pThis, struct _route *route) );

/*! @} */


#ifdef __cplusplus
}
#endif



#endif /*_GF_SG_VRML_H_*/
