/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#include <gpac/internal/scenegraph_dev.h>
/*MPEG4 & X3D tags (for node tables & script handling)*/
#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>

#ifndef GPAC_DISABLE_VRML

GF_Route* gf_sg_route_exists(GF_SceneGraph *sg, GF_Node *fromNode, u32 fromField, GF_Node *toNode, u32 toField);

GF_EXPORT
GF_Route *gf_sg_route_new(GF_SceneGraph *sg, GF_Node *fromNode, u32 fromField, GF_Node *toNode, u32 toField)
{
	GF_Route *r;
	if (!sg || !toNode || !fromNode) return NULL;

	if ( (r = gf_sg_route_exists(sg, fromNode, fromField, toNode, toField)) )
		return r;

	GF_SAFEALLOC(r, GF_Route)
	if (!r) return NULL;
	r->FromNode = fromNode;
	r->FromField.fieldIndex = fromField;
	r->ToNode = toNode;
	r->ToField.fieldIndex = toField;
	r->graph = sg;

	if (!fromNode->sgprivate->interact) {
		GF_SAFEALLOC(fromNode->sgprivate->interact, struct _node_interactive_ext);
		if (!fromNode->sgprivate->interact) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRML] Failed to create interact storage\n"));
			gf_free(r);
			return NULL;
		}
	}
	if (!fromNode->sgprivate->interact->routes) fromNode->sgprivate->interact->routes = gf_list_new();
	gf_list_add(fromNode->sgprivate->interact->routes, r);
	gf_list_add(sg->Routes, r);
	return r;
}

GF_Route* gf_sg_route_exists(GF_SceneGraph *sg, GF_Node *fromNode, u32 fromField, GF_Node *toNode, u32 toField)
{
	u32 i = 0;
	GF_Route* rt;
	if ( !fromNode->sgprivate->interact || !fromNode->sgprivate->interact->routes )
		return NULL;

	while ( (rt = (GF_Route*)gf_list_enum(fromNode->sgprivate->interact->routes, &i) )) {
		if ( rt->FromField.fieldIndex == fromField && rt->ToNode == toNode && rt->ToField.fieldIndex == toField )
			return rt;
	}
	return NULL;
}

GF_EXPORT
void gf_sg_route_del(GF_Route *r)
{
	GF_SceneGraph *sg;

	/*remove declared routes*/
	gf_list_del_item(r->graph->Routes, r);
	/*remove route from node - do this regardless of setup state since the route is registered upon creation*/
	if (r->FromNode && r->FromNode->sgprivate->interact && r->FromNode->sgprivate->interact->routes) {
		gf_list_del_item(r->FromNode->sgprivate->interact->routes, r);
		if (!gf_list_count(r->FromNode->sgprivate->interact->routes)) {
			gf_list_del(r->FromNode->sgprivate->interact->routes);
			r->FromNode->sgprivate->interact->routes = NULL;
		}
	}
	/*special case for script events: notify desdctruction*/
	if (r->ToNode && (r->ToField.fieldType==GF_SG_VRML_SCRIPT_FUNCTION) && r->ToField.on_event_in) {
		r->is_setup = 0;
		r->FromNode = NULL;
		if (!r->graph->pOwningProto) r->ToField.on_event_in(r->ToNode, r);
	}

	r->is_setup = 0;
	sg = r->graph;
	while (sg->parent_scene) sg = sg->parent_scene;
	gf_list_add(sg->routes_to_destroy, r);
	gf_list_del_item(sg->routes_to_activate, r);
}

GF_EXPORT
GF_Err gf_sg_route_del_by_id(GF_SceneGraph *sg,u32 routeID)
{
	GF_Route *r;
	if(!sg) return GF_BAD_PARAM;
	r = gf_sg_route_find(sg, routeID);
	if (!r) return GF_BAD_PARAM;
	gf_sg_route_del(r);
	return GF_OK;
}

void gf_sg_destroy_routes(GF_SceneGraph *sg)
{
	while (gf_list_count(sg->routes_to_destroy) ) {
		GF_Route *r = (GF_Route *)gf_list_get(sg->routes_to_destroy, 0);
		gf_list_rem(sg->routes_to_destroy, 0);
		gf_sg_route_unqueue(sg, r);
		if (r->name) gf_free(r->name);
		gf_free(r);
	}
}


void gf_sg_route_queue(GF_SceneGraph *sg, GF_Route *r)
{
	u32 now;
	if (!sg) return;

	/*get the top level scene (that's the only reliable one regarding simulatioin tick)*/
	while (sg->parent_scene) sg = sg->parent_scene;
	/*a single route may not be activated more than once in a simulation tick*/
	now = 1 + sg->simulation_tick;
	if (r->lastActivateTime >= now) return;
	r->lastActivateTime = now;
	gf_list_add(sg->routes_to_activate, r);
}

/*activate all routes in the order they where triggered*/
GF_EXPORT
void gf_sg_activate_routes(GF_SceneGraph *sg)
{
	GF_Route *r;
	GF_Node *targ;
	if (!sg) return;

	sg->simulation_tick++;
	gf_sg_destroy_routes(sg);

	while (gf_list_count(sg->routes_to_activate)) {
		r = (GF_Route *)gf_list_get(sg->routes_to_activate, 0);
		gf_list_rem(sg->routes_to_activate, 0);
		if (r) {
			targ = r->ToNode;
			if (gf_sg_route_activate(r)) {
#ifdef GF_SELF_REPLACE_ENABLE
				if (sg->graph_has_been_reset) {
					sg->graph_has_been_reset = 0;
					return;
				}
#endif
				if (r->is_setup) gf_node_changed(targ, &r->ToField);
			}
		}
	}
}

void gf_sg_route_unqueue(GF_SceneGraph *sg, GF_Route *r)
{
	/*get the top level scene*/
	while (sg->parent_scene) sg = sg->parent_scene;
	/*remove route from queue list*/
	gf_list_del_item(sg->routes_to_activate, r);
}

GF_EXPORT
GF_Route *gf_sg_route_find(GF_SceneGraph *sg, u32 RouteID)
{
	GF_Route *r;
	u32 i=0;
	while ((r = (GF_Route*)gf_list_enum(sg->Routes, &i))) {
		if (r->ID == RouteID) return r;
	}
	return NULL;
}

GF_EXPORT
GF_Route *gf_sg_route_find_by_name(GF_SceneGraph *sg, char *name)
{
	GF_Route *r;
	u32 i;
	if (!sg || !name) return NULL;

	i=0;
	while ((r = (GF_Route*)gf_list_enum(sg->Routes, &i))) {
		if (r->name && !strcmp(r->name, name)) return r;
	}
	return NULL;
}

GF_EXPORT
GF_Err gf_sg_route_set_id(GF_Route *route, u32 ID)
{
	GF_Route *ptr;
	if (!route || !ID) return GF_BAD_PARAM;

	ptr = gf_sg_route_find(route->graph, ID);
	if (ptr) return GF_BAD_PARAM;
	route->ID = ID;
	return GF_OK;
}

GF_EXPORT
u32 gf_sg_route_get_id(GF_Route *route)
{
	return route->ID;
}

GF_EXPORT
GF_Err gf_sg_route_set_name(GF_Route *route, char *name)
{
	GF_Route *ptr;
	if (!name || !route) return GF_BAD_PARAM;
	ptr = gf_sg_route_find_by_name(route->graph, name);
	if (ptr) return GF_BAD_PARAM;
	if (route->name) gf_free(route->name);
	route->name = gf_strdup(name);
	return GF_OK;
}

GF_EXPORT
char *gf_sg_route_get_name(GF_Route *route)
{
	return route->name;
}

void gf_sg_route_setup(GF_Route *r)
{
	gf_node_get_field(r->FromNode, r->FromField.fieldIndex, &r->FromField);
	gf_node_get_field(r->ToNode, r->ToField.fieldIndex, &r->ToField);
	switch (r->FromField.fieldType) {
	case GF_SG_VRML_MFNODE:
		if (r->ToField.fieldType != GF_SG_VRML_MFNODE) return;
		break;
	case GF_SG_VRML_SFNODE:
		if (r->ToField.fieldType != GF_SG_VRML_SFNODE) return;
		break;
	}
	r->is_setup = 1;
}

/*send event out of proto - all ISed fields are ignored*/
void gf_node_event_out_proto(GF_Node *node, u32 FieldIndex)
{
	u32 i;
	GF_Route *r;
	if (!node) return;

	if (!node->sgprivate->interact) return;

	//search for routes to activate in the order they where declared
	i=0;
	while ((r = (GF_Route*)gf_list_enum(node->sgprivate->interact->routes, &i))) {
		if (r->IS_route) continue;
		if (r->FromNode != node) continue;
		if (r->FromField.fieldIndex != FieldIndex) continue;
		gf_sg_route_queue(node->sgprivate->scenegraph, r);
	}
}

Bool gf_sg_route_activate(GF_Route *r)
{
	Bool ret;
	/*URL/String conversion clone*/
	void VRML_FieldCopyCast(void *dest, u32 dst_field_type, void *orig, u32 ori_field_type);
	assert(r->FromNode);
	if (!r->is_setup) {
		gf_sg_route_setup(r);
		if (!r->is_setup) return 0;
		/*special case when initing ISed routes on eventOuts: skip*/
		if (r->IS_route) {
			if (r->FromField.eventType == GF_SG_EVENT_OUT) return 0;
			if (r->ToField.eventType == GF_SG_EVENT_OUT) return 0;
		}
		if (r->IS_route && ((r->ToNode->sgprivate->tag==TAG_MPEG4_Script)
#ifndef GPAC_DISABLE_X3D
		                    || (r->ToNode->sgprivate->tag==TAG_X3D_Script)
#endif
		                   ) && ((r->ToField.eventType==GF_SG_EVENT_IN) /*|| (r->ToField.eventType==GF_SG_EVENT_FIELD)*/)
		        && r->FromField.eventType==GF_SG_EVENT_IN) {
			return 0;
		}
	}
#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_INTERACT, GF_LOG_DEBUG)) {
		if (r->IS_route) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[VRML Event] executing %s.%s IS %s.%s", gf_node_get_name(r->FromNode), r->FromField.name, gf_node_get_name(r->ToNode), r->ToField.name));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[VRML Event] executing ROUTE %s.%s TO %s.%s", gf_node_get_name(r->FromNode), r->FromField.name, gf_node_get_name(r->ToNode), r->ToField.name));
		}
		if (r->FromField.fieldType==GF_SG_VRML_SFBOOL) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("\tBOOL VAL: %d\n", *((SFBool*)r->FromField.far_ptr)));
		} else if (r->FromField.fieldType==GF_SG_VRML_SFINT32) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("\tINT VAL: %d\n", *((SFInt32*)r->FromField.far_ptr)));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("\n"));
		}
	}
#endif

	ret = 1;
	switch (r->FromField.fieldType) {
	case GF_SG_VRML_SFNODE:
		if (* (GF_Node **) r->ToField.far_ptr != * (GF_Node **) r->FromField.far_ptr) {
			GF_Node *n = * (GF_Node **) r->ToField.far_ptr;
			/*delete instance*/
			if (n) gf_node_unregister(n, r->ToNode);
			/*and use the node*/
			* (GF_Node **) r->ToField.far_ptr = * (GF_Node **) r->FromField.far_ptr;
			n = * (GF_Node **) r->FromField.far_ptr;
			gf_node_register(n, r->ToNode);
		}
		break;

	/*move all pointers to dest*/
	case GF_SG_VRML_MFNODE:
	{
		GF_ChildNodeItem *last = NULL;
		GF_ChildNodeItem *orig = *(GF_ChildNodeItem **)r->FromField.far_ptr;

		/*empty list*/
		gf_node_unregister_children(r->ToNode, *(GF_ChildNodeItem **)r->ToField.far_ptr );
		*(GF_ChildNodeItem **)r->ToField.far_ptr = NULL;

		while (orig) {
			gf_node_list_add_child_last( (GF_ChildNodeItem **)r->ToField.far_ptr, orig->node, &last);
			gf_node_register(orig->node, r->ToNode);
			orig = orig->next;
		}
	}
	break;

	default:
		if (r->ToField.fieldType==r->FromField.fieldType) {
			/*if unchanged don't invalidate dst node*/
			if (gf_sg_vrml_field_equal(r->ToField.far_ptr, r->FromField.far_ptr, r->FromField.fieldType)) {
				ret = 0;
			} else {
				gf_sg_vrml_field_copy(r->ToField.far_ptr, r->FromField.far_ptr, r->FromField.fieldType);
			}
		}
		/*typecast URL <-> string if needed*/
		else {
			VRML_FieldCopyCast(r->ToField.far_ptr, r->ToField.fieldType, r->FromField.far_ptr, r->FromField.fieldType);
		}
		break;
	}

	//don't notify dest change for generic function since the dest is not a node
	if (r->ToField.fieldType==GF_SG_VRML_GENERIC_FUNCTION) {
		ret = 0;
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_INTERACT, GF_LOG_DEBUG)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[VRML Route] field copy/casted\n"));
	}
#endif

	//if this is a supported eventIn call watcher
	if (r->ToField.on_event_in) {
		r->ToField.on_event_in(r->ToNode, r);
	}
	//if this is a script eventIn call directly script
	else if (((r->ToNode->sgprivate->tag==TAG_MPEG4_Script)
#ifndef GPAC_DISABLE_X3D
	          || (r->ToNode->sgprivate->tag==TAG_X3D_Script)
#endif
	         ) && ((r->ToField.eventType==GF_SG_EVENT_IN) /*|| (r->ToField.eventType==GF_SG_EVENT_FIELD)*/) ) {
		gf_sg_script_event_in(r->ToNode, &r->ToField);
	}
	//check if ISed or not - this will notify the node of any changes
	else {
		gf_sg_proto_propagate_event(r->ToNode, r->ToField.fieldIndex, r->FromNode);
		/*if not an ISed field, propagate (otherwise ROUTE is executed just below)*/
		if (r->ToField.eventType != GF_SG_EVENT_EXPOSED_FIELD)
			gf_sg_proto_propagate_event(r->ToNode, r->ToField.fieldIndex, r->FromNode);
		/*only happen on proto, an eventOut may route to an eventOut*/
		if (r->IS_route && r->ToField.eventType==GF_SG_EVENT_OUT)
			gf_node_event_out(r->ToNode, r->ToField.fieldIndex);
	}

	/*and signal routes on exposed fields if field changed*/
	if (r->ToField.eventType == GF_SG_EVENT_EXPOSED_FIELD) {
		if (r->IS_route)
			gf_node_event_out_proto(r->ToNode, r->ToField.fieldIndex);
		else
			gf_node_event_out(r->ToNode, r->ToField.fieldIndex);
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_INTERACT, GF_LOG_DEBUG)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[VRML Route] done executing (res %d)\n", ret));
	}
#endif

	return ret;
}


GF_EXPORT
void gf_node_event_out(GF_Node *node, u32 FieldIndex)
{
	u32 i;
	GF_Route *r;
	if (!node) return;

	/*node has no routes*/
	if (!node->sgprivate->interact || !node->sgprivate->interact->routes) return;

	//search for routes to activate in the order they where declared
	i=0;
	while ((r = (GF_Route*)gf_list_enum(node->sgprivate->interact->routes, &i))) {
		if (r->FromNode != node) continue;
		if (r->FromField.fieldIndex != FieldIndex) continue;

		/*no postpone for IS routes*/
		if (r->IS_route) {
			if (gf_sg_route_activate(r))
				gf_node_changed(r->ToNode, &r->ToField);
		}
		//queue
		else {
			gf_sg_route_queue(node->sgprivate->scenegraph, r);
		}
	}
}

GF_EXPORT
void gf_node_event_out_str(GF_Node *node, const char *eventName)
{
	u32 i;
	GF_Route *r;

	/*node has no routes*/
	if (!node->sgprivate->interact || !node->sgprivate->interact->routes) return;

	//search for routes to activate in the order they where declared
	i=0;
	while ((r = (GF_Route*)gf_list_enum(node->sgprivate->interact->routes, &i))) {
		if (!r->is_setup) gf_sg_route_setup(r);
		if (stricmp(r->FromField.name, eventName)) continue;

		//no postpone
		if (r->IS_route) {
			gf_sg_route_activate(r);
		}
		//queue
		else {
			gf_sg_route_queue(node->sgprivate->scenegraph, r);
		}
	}
}

typedef struct
{
	GF_Route r;
	void ( *route_callback) (void *param, GF_FieldInfo *from_field);
} GF_RouteToFunction;

static void on_route_to_function(GF_Node *node, GF_Route *r)
{
	GF_RouteToFunction *rf = (GF_RouteToFunction *)r;
	rf->route_callback(r->ToNode, &r->FromField);
}

GF_EXPORT
void gf_sg_route_new_to_callback(GF_SceneGraph *sg, GF_Node *fromNode, u32 fromField, void *cbk, void ( *route_callback) (void *param, GF_FieldInfo *from_field) )
{
	GF_Route *r;
	GF_RouteToFunction *rf;
	GF_SAFEALLOC(rf, GF_RouteToFunction);
	if (!rf) return;
	rf->route_callback = route_callback;

	r = (GF_Route *)rf;
	r->FromNode = fromNode;
	r->FromField.fieldIndex = fromField;
	gf_node_get_field(r->FromNode, fromField, &r->FromField);

	r->ToNode = (GF_Node *) cbk;
	r->ToField.fieldType = GF_SG_VRML_GENERIC_FUNCTION;
	r->ToField.on_event_in = on_route_to_function;
	r->ToField.eventType = GF_SG_EVENT_IN;
	r->ToField.far_ptr = NULL;

	r->is_setup = 1;
	r->graph = sg;

	if (!fromNode->sgprivate->interact) {
		GF_SAFEALLOC(fromNode->sgprivate->interact, struct _node_interactive_ext);
		if (!fromNode->sgprivate->interact) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRML] Failed to create interact storage\n"));
			gf_free(r);
			return;
		}
	}
	if (!fromNode->sgprivate->interact->routes) fromNode->sgprivate->interact->routes = gf_list_new();
	gf_list_add(fromNode->sgprivate->interact->routes, r);
	gf_list_add(fromNode->sgprivate->scenegraph->Routes, r);
}


#endif	/*GPAC_DISABLE_VRML*/

