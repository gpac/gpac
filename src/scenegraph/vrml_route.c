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

#include <gpac/internal/scenegraph_dev.h>
/*MPEG4 & X3D tags (for node tables & script handling)*/
#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>


GF_Route *gf_sg_route_new(GF_SceneGraph *sg, GF_Node *fromNode, u32 fromField, GF_Node *toNode, u32 toField)
{
	GF_Route *r;
	if (!sg || !toNode || !fromNode) return NULL;

	GF_SAFEALLOC(r, GF_Route)
	if (!r) return NULL;
	r->FromNode = fromNode;
	r->FromField.fieldIndex = fromField;
	r->ToNode = toNode;
	r->ToField.fieldIndex = toField;
	r->graph = sg;

	if (!fromNode->sgprivate->events) fromNode->sgprivate->events = gf_list_new();
	gf_list_add(fromNode->sgprivate->events, r);
	gf_list_add(sg->Routes, r);
	return r;
}

void gf_sg_route_del(GF_Route *r)
{
	GF_SceneGraph *sg;
	s32 ind;

	gf_sg_route_unqueue(r->graph, r);

	/*remove declared routes*/
	ind = gf_list_del_item(r->graph->Routes, r);
	/*remove route from node*/
	if (r->FromNode && r->FromNode->sgprivate->events) {
		gf_list_del_item(r->FromNode->sgprivate->events, r);
		if (!gf_list_count(r->FromNode->sgprivate->events)) {
			gf_list_del(r->FromNode->sgprivate->events);
			r->FromNode->sgprivate->events = NULL;
		}
	}
	r->is_setup = 0;
	sg = r->graph;
	while (sg->parent_scene) sg = sg->parent_scene;
	gf_list_add(sg->routes_to_destroy, r);
}


GF_Err gf_sg_route_del_by_id(GF_SceneGraph *sg,u32 routeID)
{
	GF_Route *r;
	if(!sg) return GF_BAD_PARAM;
	r = gf_sg_route_find(sg, routeID);
	if (!r) return GF_BAD_PARAM;
	gf_sg_route_del(r);
	return GF_OK;
}

GF_Route *gf_sg_route_find(GF_SceneGraph *sg, u32 RouteID)
{
	GF_Route *r;
	u32 i=0;
	while ((r = gf_list_enum(sg->Routes, &i))) {
		if (r->ID == RouteID) return r;
	}
	return NULL;
}

GF_Route *gf_sg_route_find_by_name(GF_SceneGraph *sg, char *name)
{
	GF_Route *r;
	u32 i;
	if (!sg || !name) return NULL;

	i=0;
	while ((r = gf_list_enum(sg->Routes, &i))) {
		if (r->name && !strcmp(r->name, name)) return r;
	}
	return NULL;
}


GF_Err gf_sg_route_set_id(GF_Route *route, u32 ID)
{
	GF_Route *ptr;
	if (!route || !ID) return GF_BAD_PARAM;

	ptr = gf_sg_route_find(route->graph, ID);
	if (ptr) return GF_BAD_PARAM;
	route->ID = ID;
	return GF_OK;
}
u32 gf_sg_route_get_id(GF_Route *route) 
{
	return route->ID;
}

GF_Err gf_sg_route_set_name(GF_Route *route, char *name)
{
	GF_Route *ptr;
	if (!name || !route) return GF_BAD_PARAM;
	ptr = gf_sg_route_find_by_name(route->graph, name);
	if (ptr) return GF_BAD_PARAM;
	if (route->name) free(route->name);
	route->name = strdup(name);
	return GF_OK;
}
char *gf_sg_route_get_name(GF_Route *route)
{
	return route->name;
}

static void gf_sg_route_setup(GF_Route *r) 
{
	gf_node_get_field(r->FromNode, r->FromField.fieldIndex, &r->FromField);
	gf_node_get_field(r->ToNode, r->ToField.fieldIndex, &r->ToField);
	r->is_setup = 1;
}

Bool gf_sg_route_activate(GF_Route *r)
{
	Bool ret;
	/*URL/String conversion clone*/
	void VRML_FieldCopyCast(void *dest, u32 dst_field_type, void *orig, u32 ori_field_type);

	if (!r->is_setup) {
		gf_sg_route_setup(r);
		/*special case when initing ISed routes on eventOuts: skip*/
		if (r->IS_route) {
			if (r->FromField.eventType == GF_SG_EVENT_OUT) return 0;
		}
	}
#ifndef GPAC_DISABLE_LOG
	if (r->IS_route) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[VRML Event] executing %s.%s IS %s.%s\n", r->FromNode->sgprivate->NodeName, r->FromField.name, r->ToNode->sgprivate->NodeName, r->ToField.name));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[VRML Event] executing ROUTE %s.%s TO %s.%s\n", r->FromNode->sgprivate->NodeName, r->FromField.name, r->ToNode->sgprivate->NodeName, r->ToField.name));
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
		u32 i;
		GF_Node *p;
		GF_List *orig = *(GF_List**)r->FromField.far_ptr;
		GF_List *dest = *(GF_List**)r->ToField.far_ptr;

		/*empty list*/
		while (gf_list_count(dest)){
			p = gf_list_get(dest, 0);
			gf_list_rem(dest, 0);
			gf_node_unregister(p, r->ToNode);
		}

		i=0;
		while ((p = gf_list_enum(orig, &i))) {
			gf_list_add(dest, p);
			gf_node_register(p, r->ToNode);
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

	//if this is a supported eventIn call watcher
	if (r->ToField.on_event_in) {
		r->ToField.on_event_in(r->ToNode);
	}
	//if this is a script eventIn call directly script
	else if (((r->ToNode->sgprivate->tag==TAG_MPEG4_Script) || (r->ToNode->sgprivate->tag==TAG_X3D_Script) ) 
		&& ((r->ToField.eventType==GF_SG_EVENT_IN) /*|| (r->ToField.eventType==GF_SG_EVENT_FIELD)*/) ) {
		gf_sg_script_event_in(r->ToNode, &r->ToField);
	}
	//check if ISed or not - this will notify the node of any changes
	else {
		gf_sg_proto_check_field_change(r->ToNode, r->ToField.fieldIndex);
		/*only happen on proto, an eventOut may route to an eventOut*/
		if (r->IS_route && r->ToField.eventType==GF_SG_EVENT_OUT) 
			gf_node_event_out(r->ToNode, r->ToField.fieldIndex);
	}
	/*and signal routes on exposed fields if field changed*/
	if (r->ToField.eventType == GF_SG_EVENT_EXPOSED_FIELD)
		gf_node_event_out(r->ToNode, r->ToField.fieldIndex);

	return ret;
}


void gf_node_event_out(GF_Node *node, u32 FieldIndex)
{
	u32 i;
	GF_Route *r;
	if (!node) return;
	
	//this is not an ISed
	if (!node->sgprivate->NodeID && !node->sgprivate->scenegraph->pOwningProto) return;
	if (!node->sgprivate->events) return;
	
	//search for routes to activate in the order they where declared
	i=0;
	while ((r = gf_list_enum(node->sgprivate->events, &i))) {
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

void gf_node_event_out_str(GF_Node *node, const char *eventName)
{
	u32 i;
	GF_Route *r;

	/*node is being deleted ignore event*/
	if (!node->sgprivate->events) return;

	//this is not an ISed
	if (!node->sgprivate->NodeID && !node->sgprivate->scenegraph->pOwningProto) return;
	
	//search for routes to activate in the order they where declared
	i=0;
	while ((r = gf_list_enum(node->sgprivate->events, &i))) {
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

