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


GF_Proto *gf_sg_proto_new(GF_SceneGraph *inScene, u32 ProtoID, char *name, Bool unregistered)
{
	GF_Proto *tmp;
	if (!inScene) return NULL;

	/*make sure we don't define a proto already defined in this scope*/
	if (!unregistered) {
		tmp = gf_sg_find_proto(inScene, ProtoID, name);
		if (tmp) return NULL;
	}

	GF_SAFEALLOC(tmp, GF_Proto)
	if (!tmp) return NULL;

	tmp->proto_fields = gf_list_new();
	tmp->node_code = gf_list_new();
	tmp->parent_graph = inScene;
	tmp->sub_graph = gf_sg_new_subscene(inScene);
	tmp->instances = gf_list_new();

	if (name) 
		tmp->Name = strdup(name);
	else
		tmp->Name = strdup("Unnamed Proto");
	tmp->ID = ProtoID;
	if (!unregistered) {
		gf_list_add(inScene->protos, tmp);
	} else {
		gf_list_add(inScene->unregistered_protos, tmp);
	}
	return tmp;
}


GF_Err gf_sg_proto_set_in_graph(GF_Proto *proto, GF_SceneGraph *inScene, Bool set_in)
{
	u32 i;
	GF_Proto *tmp;
	GF_List *removeFrom;
	GF_List *insertIn;

	if (set_in) {
		removeFrom = proto->parent_graph->unregistered_protos;
		insertIn = proto->parent_graph->protos;
	} else {
		insertIn = proto->parent_graph->unregistered_protos;
		removeFrom = proto->parent_graph->protos;
	}

	gf_list_del_item(removeFrom, proto);

	i=0;
	while ((tmp = (GF_Proto*)gf_list_enum(insertIn, &i))) {
		if (tmp==proto) return GF_OK;
		if (!set_in) continue;
		/*if registering, make sure no other proto has the same ID/name*/
		if (tmp->ID==proto->ID) return GF_BAD_PARAM;
		if (!stricmp(tmp->Name, proto->Name)) return GF_BAD_PARAM;
	}
	return gf_list_add(insertIn, proto);
}


GF_Err gf_sg_proto_del(GF_Proto *proto)
{
	GF_Node *node;
	GF_ProtoFieldInterface *field;
	s32 i;

	if (!proto) return GF_OK;
	i = gf_list_del_item(proto->parent_graph->protos, proto);
	if (i<0) i = gf_list_del_item(proto->parent_graph->unregistered_protos, proto);

	if (proto->userpriv && proto->OnDelete) proto->OnDelete(proto->userpriv);

	/*first destroy the code*/
	while (gf_list_count(proto->node_code)) {
		node = (GF_Node*)gf_list_get(proto->node_code, 0);
		gf_node_unregister(node, NULL);
		gf_list_rem(proto->node_code, 0);
	}
	gf_list_del(proto->node_code);

	/*delete interface*/
	while (gf_list_count(proto->proto_fields)) {
		field = (GF_ProtoFieldInterface*)gf_list_get(proto->proto_fields, 0);
		if (field->userpriv && field->OnDelete) field->OnDelete(field->userpriv);

		if (field->FieldType==GF_SG_VRML_SFNODE) {
			if (field->def_sfnode_value) 
				gf_node_unregister(field->def_sfnode_value, NULL);
		} 
		else if (field->FieldType==GF_SG_VRML_MFNODE) {
			if (field->def_mfnode_value)
				gf_node_unregister_children(NULL, field->def_mfnode_value);
		} 
		else if (field->def_value) 
			gf_sg_vrml_field_pointer_del(field->def_value, field->FieldType);
	
		if (field->FieldName) free(field->FieldName);

		/*QP fields are SF fields, we can safely free() them*/
		if (field->qp_max_value) free(field->qp_max_value);
		if (field->qp_min_value) free(field->qp_min_value);
		free(field);
		gf_list_rem(proto->proto_fields, 0);
	}
	gf_list_del(proto->proto_fields);

	while (gf_list_count(proto->instances)) {
		GF_ProtoInstance *p = (GF_ProtoInstance *)gf_list_get(proto->instances, 0);
		gf_list_rem(proto->instances, 0);
		p->proto_interface = NULL;
	}

	/*delete sub graph*/
	gf_sg_del(proto->sub_graph);


	if (proto->Name) free(proto->Name);
	gf_sg_mfurl_del(proto->ExternProto);
	gf_list_del(proto->instances);	
	free(proto);
	return GF_OK;
}

GF_SceneGraph *gf_sg_proto_get_graph(GF_Proto *proto)
{
	return proto ? proto->sub_graph : NULL;
}

void gf_sg_proto_set_private(GF_Proto *p, void *ptr, void (*OnDelete)(void *ptr) )
{
	if (p) {
		p->userpriv = ptr;
		p->OnDelete = OnDelete;
	}
}
void *gf_sg_proto_get_private(GF_Proto *p)
{
	return p ? p->userpriv : NULL;
}

GF_EXPORT
MFURL *gf_sg_proto_get_extern_url(GF_Proto *proto)
{
	return proto ? &proto->ExternProto : NULL;
}

GF_Err gf_sg_proto_add_node_code(GF_Proto *proto, GF_Node *pNode)
{
	if (!proto) return GF_BAD_PARAM;
	return gf_list_add(proto->node_code, pNode);
}

GF_ProtoFieldInterface *gf_sg_proto_field_find_by_name(GF_Proto *proto, char *fieldName)
{
	GF_ProtoFieldInterface *ptr;
	u32 i=0;
	while ((ptr = (GF_ProtoFieldInterface*)gf_list_enum(proto->proto_fields, &i))) {
		if (ptr->FieldName && !strcmp(ptr->FieldName, fieldName)) return ptr;
	}
	return NULL;
}

GF_ProtoFieldInterface *gf_sg_proto_field_new(GF_Proto *proto, u32 fieldType, u32 eventType, char *fieldName)
{
	GF_ProtoFieldInterface *tmp;

	if (fieldName) {
		tmp = gf_sg_proto_field_find_by_name(proto, fieldName);
		if (tmp) return NULL;
	}
	GF_SAFEALLOC(tmp, GF_ProtoFieldInterface)
	if (!tmp) return NULL;

	tmp->FieldType = fieldType;
	tmp->EventType = eventType;
	
	/*create container - can be NULL if SF node*/
	if ( fieldType == GF_SG_VRML_SFNODE) {
		tmp->def_sfnode_value = NULL;
		tmp->def_value = &tmp->def_sfnode_value;
	} else if ( fieldType == GF_SG_VRML_MFNODE) {
		tmp->def_mfnode_value = NULL;
		tmp->def_value = &tmp->def_mfnode_value;
	} else {
		tmp->def_value = gf_sg_vrml_field_pointer_new(fieldType);
	}
	
	if (fieldName) tmp->FieldName = strdup(fieldName);
	
	tmp->ALL_index = gf_list_count(proto->proto_fields);
	tmp->OUT_index = tmp->DEF_index = tmp->IN_index = (u32) -1;

	switch (eventType) {
	case GF_SG_EVENT_EXPOSED_FIELD:
		tmp->IN_index = proto->NumIn;
		proto->NumIn ++;
		tmp->OUT_index = proto->NumOut;
		proto->NumOut ++;
	case GF_SG_EVENT_FIELD:
		tmp->DEF_index = proto->NumDef;
		proto->NumDef ++;
		break;
	case GF_SG_EVENT_IN:
		tmp->IN_index = proto->NumIn;
		proto->NumIn ++;
		break;
	case GF_SG_EVENT_OUT:
		tmp->OUT_index = proto->NumOut;
		proto->NumOut ++;
		break;
	}

	gf_list_add(proto->proto_fields, tmp);
	return tmp;
}

void gf_sg_proto_field_set_private(GF_ProtoFieldInterface *field, void *ptr, void (*OnDelete)(void *ptr))
{
	if (field) {
		field->userpriv = ptr;
		field->OnDelete = OnDelete;
	}
}

void *gf_sg_proto_field_get_private(GF_ProtoFieldInterface *field)
{
	return field ? field->userpriv : NULL;
}



GF_Err gf_sg_proto_field_get_field(GF_ProtoFieldInterface *field, GF_FieldInfo *info)
{
	if (!field || !info) return GF_BAD_PARAM;
	memset(info, 0, sizeof(GF_FieldInfo));
	info->fieldIndex = field->ALL_index;
	info->fieldType = field->FieldType;
	info->eventType = field->EventType;
	info->far_ptr = field->def_value;
	info->name = field->FieldName;
	info->NDTtype = NDT_SFWorldNode;
	return GF_OK;
}

GF_Err gf_sg_proto_get_field(GF_Proto *proto, GF_Node *node, GF_FieldInfo *info)
{
	GF_ProtoFieldInterface *proto_field;
	GF_ProtoInstance *inst;
	GF_ProtoField *field;

	if (!proto && !node) return GF_BAD_PARAM;

	if (proto) {
		proto_field = (GF_ProtoFieldInterface*)gf_list_get(proto->proto_fields, info->fieldIndex);
		if (!proto_field) return GF_BAD_PARAM;

		info->fieldType = proto_field->FieldType;
		info->eventType = proto_field->EventType;
		info->fieldIndex = proto_field->ALL_index;
		info->NDTtype = NDT_SFWorldNode;
		info->far_ptr = proto_field->def_value;
		info->name = proto_field->FieldName;
		return GF_OK;
	}

	/*otherwise this is an instanciated proto*/
	if (node->sgprivate->tag!=TAG_ProtoNode) return GF_BAD_PARAM;

	inst = (GF_ProtoInstance *) node;
	field = (GF_ProtoField*)gf_list_get(inst->fields, info->fieldIndex);
	if (!field) return GF_BAD_PARAM;

	info->fieldType = field->FieldType;
	info->eventType = field->EventType;
	/*SF/MF nodes need pointers to field object - cf gf_sg_proto_create_node*/
	if (gf_sg_vrml_get_sf_type(field->FieldType) == GF_SG_VRML_SFNODE) {
		info->far_ptr = &field->field_pointer;
	} else {
		info->far_ptr = field->field_pointer;
	}
	/*set the name - watchout for deletion case*/
	if (inst->proto_interface) {
		proto_field = (GF_ProtoFieldInterface*)gf_list_get(inst->proto_interface->proto_fields, info->fieldIndex);
		info->name = proto_field->FieldName;
	} else {
		info->name = "ProtoFieldDeleted";
	}
	info->NDTtype = NDT_SFWorldNode;
	return GF_OK;
}

s32 gf_sg_proto_get_field_index_by_name(GF_Proto *proto, GF_Node *node, char *name)
{
	u32 i;
	GF_ProtoFieldInterface *proto_field;
	GF_Proto *__proto;

	if (node && (node->sgprivate->tag!=TAG_ProtoNode)) return -1;

	__proto = proto ? proto : ((GF_ProtoInstance *) node)->proto_interface;
	if (!__proto ) return -1;

	for (i=0; i<gf_list_count(__proto->proto_fields); i++) {
		proto_field = (GF_ProtoFieldInterface*)gf_list_get(__proto->proto_fields, i);
		if (proto_field->FieldName && !strcmp(proto_field->FieldName, name)) return i;
	}
	return -1;
}

GF_EXPORT
GF_Node *gf_node_clone(GF_SceneGraph *inScene, GF_Node *orig, GF_Node *cloned_parent)
{
	u32 i, count, id;
	const char *orig_name;
	Bool is_script;
	GF_Node *node, *child;
	GF_ChildNodeItem *list, *last;
	GF_Route *r1, *r2;
	void BIFS_SetupConditionalClone(GF_Node *node, GF_Node *orig);
	GF_ProtoInstance *proto;
	GF_Proto *proto_node;
	GF_FieldInfo field_orig, field;

	/*this is not a mistake*/
	if (!orig) return NULL;

	/*check for DEF/USE*/
	orig_name  = gf_node_get_name_and_id(orig, &id);
	if (id) {
		node = gf_sg_find_node(inScene, id);
		/*node already created, USE*/
		if (node) {
			gf_node_register(node, cloned_parent);
			return node;
		}
	}
	/*create a node*/
	if (orig->sgprivate->tag == TAG_ProtoNode) {
		proto_node = ((GF_ProtoInstance *)orig)->proto_interface;
		/*create the instance but don't load the code -c we MUST wait for ISed routes to be cloned before*/
		node = gf_sg_proto_create_node(inScene, proto_node, (GF_ProtoInstance *) orig);
	} else {
		node = gf_node_new(inScene, orig->sgprivate->tag);
	}

	count = gf_node_get_field_count(orig);

	is_script = 0;
	if ((orig->sgprivate->tag==TAG_MPEG4_Script) || (orig->sgprivate->tag==TAG_X3D_Script)) is_script = 1;
	if (is_script) gf_sg_script_prepare_clone(node, orig);

	/*copy each field*/
	for (i=0; i<count; i++) {
		gf_node_get_field(orig, i, &field_orig);

		/*get target ptr*/
		gf_node_get_field(node, i, &field);

		assert(field.eventType==field_orig.eventType);
		assert(field.fieldType==field_orig.fieldType);

		/*duplicate it*/
		switch (field.fieldType) {
		case GF_SG_VRML_SFNODE:
			child = gf_node_clone(inScene, (* ((GF_Node **) field_orig.far_ptr)), node);
			*((GF_Node **) field.far_ptr) = child;
			break;
		case GF_SG_VRML_MFNODE:
			last = NULL;
			list = *( (GF_ChildNodeItem **) field_orig.far_ptr);
			while (list) {
				child = gf_node_clone(inScene, list->node, node);
				gf_node_list_add_child_last((GF_ChildNodeItem **) field.far_ptr, child, &last);
				list = list->next;
			}
			break;
		case GF_SG_VRML_SFTIME:
			gf_sg_vrml_field_copy(field.far_ptr, field_orig.far_ptr, field.fieldType);
			if (!inScene->GetSceneTime) break;
			/*update SFTime that must be updated when cloning the node*/
			if (orig->sgprivate->tag == TAG_ProtoNode) {
				if (gf_sg_proto_field_is_sftime_offset(orig, &field_orig))
					*((SFTime *)field.far_ptr) += inScene->GetSceneTime(inScene->userpriv);
			} else if (!stricmp(field_orig.name, "startTime") || !stricmp(field_orig.name, "startTime") ) {
				*((SFTime *)field.far_ptr) += inScene->GetSceneTime(inScene->userpriv);
			}
			break;
		default:
			gf_sg_vrml_field_copy(field.far_ptr, field_orig.far_ptr, field.fieldType);
			break;
		}
	}

	if (node->sgprivate->tag==TAG_MPEG4_InputSensor) {
		GF_Command *com_o, *com_f;
		u32 k = 0;
		M_InputSensor *clone_is = (M_InputSensor *)node;
		M_InputSensor *orig_is = (M_InputSensor *)orig;
		while ( (com_o = (GF_Command *)gf_list_enum(orig_is->buffer.commandList, &k) ) ) {
			com_f = gf_sg_command_clone(com_o, node->sgprivate->scenegraph);
			gf_list_add(clone_is->buffer.commandList, com_f);
		}
	}

	/*register node*/
	if (id ) {
		gf_node_set_id(node, id, orig_name);
	}
	gf_node_register(node, cloned_parent);

#ifndef GPAC_READ_ONLY
	/*init node before creating ISed routes so the eventIn handler are in place*/
	if (node->sgprivate->tag == TAG_MPEG4_Conditional) BIFS_SetupConditionalClone(node, orig);
	else 
#endif
		if (node->sgprivate->tag != TAG_ProtoNode) gf_node_init(node);

	if (!inScene->pOwningProto) return node;
	proto = inScene->pOwningProto;
	
	/*create Routes for ISed fields*/
	i=0;
	while ((r1 = (GF_Route*)gf_list_enum(proto->proto_interface->sub_graph->Routes, &i))) {
		r2 = NULL;
		/*locate only ISed routes*/
		if (!r1->IS_route) continue;
		
		/*eventOut*/
		if (r1->FromNode == orig) {
			r2 = gf_sg_route_new(inScene, node, r1->FromField.fieldIndex, (GF_Node *) proto, r1->ToField.fieldIndex);
			r2->IS_route = 1;
		}
		/*eventIn or exposedField*/
		else if (r1->ToNode == orig) {
			r2 = gf_sg_route_new(inScene, (GF_Node *) proto, r1->FromField.fieldIndex, node, r1->ToField.fieldIndex);
			r2->IS_route = 1;

			/*activate the route now so that proto instanciation works properly, otherwise we may load scripts with wrong field values
			Note: we don't activate eventOut routes upon instanciation since no event has been triggered yet*/
			gf_sg_route_activate(r2);
		}
	}
	
	/*remember scripts*/
	if (is_script) gf_list_add(proto->scripts_to_load, node);

	/*this is a proto node, init our internal stuff*/
	if (node->sgprivate->tag == TAG_ProtoNode) {
		node->sgprivate->UserCallback = NULL;
		node->sgprivate->UserPrivate = NULL;
		/*NO RENDER, this is filtered at the generic gf_node_traverse to cope with instanciations and externProto*/
		/*load code*/
		gf_sg_proto_instanciate((GF_ProtoInstance *)node);
	}
	return node;
}


#ifdef GF_NODE_USE_POINTERS
static GF_Err protoinst_get_field(GF_Node *node, GF_FieldInfo *info)
{
	info->NDTtype = NDT_SFWorldNode;
	return gf_sg_proto_get_field(NULL, node, info);
}
static void protoinst_del(GF_Node *n)
{
	gf_sg_proto_del_instance((GF_ProtoInstance *)n);
}
#endif

GF_Err gf_sg_proto_get_field_ind_static(GF_Node *Node, u32 inField, u8 IndexMode, u32 *allField)
{
	return gf_sg_proto_get_field_index((GF_ProtoInstance *)Node, inField, IndexMode, allField);
}


static Bool is_same_proto(GF_Proto *p1, GF_Proto *p2)
{
	u32 i, count;
	if (gf_list_count(p1->proto_fields) != gf_list_count(p2->proto_fields)) return 0;
	count = gf_list_count(p1->proto_fields);
	for (i=0; i<count; i++) {
		GF_ProtoFieldInterface *pf1 = (GF_ProtoFieldInterface*)gf_list_get(p1->proto_fields, i);
		GF_ProtoFieldInterface *pf2 = (GF_ProtoFieldInterface*)gf_list_get(p2->proto_fields, i);
		if (pf1->EventType != pf2->EventType) return 0;
		if (pf1->FieldType != pf2->FieldType) return 0;
		/*note we don't check names since we're not sure both protos use name coding (MPEG4 only)*/
	}
	return 1;
}

static GF_Proto *SG_FindProtoByInterface(GF_SceneGraph *sg, GF_Proto *the_proto)
{
	GF_Proto *proto;
	u32 i, count;

	assert(sg);

	/*browse all top-level */
	i=0;
	while ((proto = (GF_Proto*)gf_list_enum(sg->protos, &i))) {
		if (is_same_proto(proto, the_proto)) return proto;
	}
	/*browse all top-level unregistered in reverse order*/
	count = gf_list_count(sg->unregistered_protos);
	for (i=count; i>0; i--) {
		proto = (GF_Proto*)gf_list_get(sg->unregistered_protos, i-1);
		if (is_same_proto(proto, the_proto)) return proto;
	}
	return NULL;
}
/*performs common initialization of routes ISed fields and protos once everything is loaded*/
void gf_sg_proto_instanciate(GF_ProtoInstance *proto_node)
{
	GF_Node *node, *orig;
	GF_Route *route, *r2;
	u32 i, count;
	GF_Proto *proto = proto_node->proto_interface;
	GF_Proto *owner = proto;

	if (!proto) return;

	if (owner->ExternProto.count) {
		GF_ProtoFieldInterface *pfi;
		GF_SceneGraph *extern_lib;
		if (!owner->parent_graph->GetExternProtoLib) return;
		extern_lib = owner->parent_graph->GetExternProtoLib(proto->parent_graph->userpriv, &owner->ExternProto);
		if (!extern_lib) return;

		/*this is an hardcoded proto - all routes, node modifications and co are handled internally*/
		if (extern_lib == GF_SG_INTERNAL_PROTO) {
			proto_node->sgprivate->flags |= GF_SG_NODE_DIRTY;
			owner->parent_graph->NodeCallback(owner->parent_graph->userpriv, GF_SG_CALLBACK_INIT, (GF_Node *) proto_node, NULL);
			return;
		}
		/*not loaded yet*/
		if (!gf_list_count(extern_lib->protos)) return;

		/*overwrite this proto by external one*/
		proto = NULL;
		/*start with proto v2 addressing*/
		if (owner->ExternProto.vals[0].url) {
			u32 ID = (u32) -1;
			char *szName = strrchr(owner->ExternProto.vals[0].url, '#');
			if (szName) {
				szName++;
				if (sscanf(szName, "%d", &ID)) ID = (u32) -1;
			}
			/*if we have the proto name, use it*/
			if (owner->Name) szName = owner->Name;
			proto = gf_sg_find_proto(extern_lib, ID, szName);
		}
		if (!proto) proto = gf_sg_find_proto(extern_lib, owner->ID, NULL);
		if (!proto) proto = SG_FindProtoByInterface(extern_lib, owner);
		/*couldn't find proto in the given lib, consider the proto as loaded (give up)*/
		if (!proto) {
			proto_node->is_loaded = 1;
			return;
		}
		count = gf_list_count(owner->proto_fields);
		for (i=0; i<count; i++) {
			GF_ProtoField *pf = (GF_ProtoField *)gf_list_get(proto_node->fields, i);
			pfi = (GF_ProtoFieldInterface*)gf_list_get(proto->proto_fields, i);
//			if (!pf->has_been_accessed) {
//				pfi = (GF_ProtoFieldInterface*)gf_list_get(proto->proto_fields, i);
//				gf_sg_vrml_field_copy(pf->field_pointer, pfi->def_value, pfi->FieldType);
//			}
			gf_sg_vrml_field_copy(pf->field_pointer, pfi->def_value, pfi->FieldType);
		}

		/*unregister from prev and reg with real proto*/
		gf_list_del_item(owner->instances, proto_node);
		gf_list_add(proto->instances, proto_node);
	}

	/*OVERRIDE the proto instance (eg don't instanciate an empty externproto...)*/
	proto_node->proto_interface = proto;

	/*clone all nodes*/
	i=0;
	while ((orig = (GF_Node*)gf_list_enum(proto->node_code, &i))) {
		/*node is cloned in the new scenegraph and its parent is NULL */
		node = gf_node_clone(proto_node->sgprivate->scenegraph, orig, NULL);
		assert(node);

		/*assign first rendering node*/
		if (i==1) proto_node->RenderingNode = node;
		gf_list_add(proto_node->node_code, node);
	}

	/*instantiate routes (not ISed ones)*/
	i=0;
	while ((route = (GF_Route*)gf_list_enum(proto->sub_graph->Routes, &i))) {
		if (route->IS_route) continue;

		r2 = gf_sg_route_new(proto_node->sgprivate->scenegraph, 
				gf_sg_find_node(proto_node->sgprivate->scenegraph, gf_node_get_id(route->FromNode) ), 
				route->FromField.fieldIndex, 
				gf_sg_find_node(proto_node->sgprivate->scenegraph, gf_node_get_id(route->ToNode) ), 
				route->ToField.fieldIndex);

		if (route->ID) gf_sg_route_set_id(r2, route->ID);
		if (route->name) gf_sg_route_set_name(r2, route->name);
	}
	/*activate all ISed fields so that inits on events is properly done*/
	i=0;
	while ((route = (GF_Route*)gf_list_enum(proto_node->sgprivate->scenegraph->Routes, &i))) {
		if (!route->IS_route) continue;
		/*do not activate eventIn to eventIn routes*/
		if (route->is_setup) {
			if ((route->ToField.eventType == GF_SG_EVENT_IN) && (route->FromField.eventType == GF_SG_EVENT_IN) ) continue;
		}
		gf_sg_route_activate(route);
	}
	/*and load all scripts (this must be done once all fields are routed for the "initialize" method)*/
	while (gf_list_count(proto_node->scripts_to_load)) {
		node = (GF_Node*)gf_list_get(proto_node->scripts_to_load, 0);
		gf_list_rem(proto_node->scripts_to_load, 0);
		gf_sg_script_load(node);
	}
	/*re-activate all ISed fields pointing to scripts once scripts are loaded (eventIns)*/
	i=0;
	while ((route = (GF_Route*)gf_list_enum(proto_node->sgprivate->scenegraph->Routes, &i))) {
		if (!route->IS_route || !route->ToNode) continue;
/*		assert(route->is_setup);
		if ((route->FromField.eventType == GF_SG_EVENT_OUT) || (route->FromField.eventType == GF_SG_EVENT_IN) ) continue;
*/		if ((route->ToNode->sgprivate->tag==TAG_MPEG4_Script) || (route->ToNode->sgprivate->tag==TAG_X3D_Script) )
			gf_sg_route_activate(route);
	}
	proto_node->is_loaded = 1;
}

GF_Node *gf_sg_proto_create_node(GF_SceneGraph *scene, GF_Proto *proto, GF_ProtoInstance *from_inst)
{
	u32 i;
	GF_ProtoField *inst, *from_field;
	GF_ProtoFieldInterface *field;

	GF_ProtoInstance *proto_node;
	GF_SAFEALLOC(proto_node, GF_ProtoInstance)
	if (!proto_node) return NULL;

	gf_node_setup((GF_Node *)proto_node, TAG_ProtoNode);
	proto_node->node_code = gf_list_new();
	proto_node->fields = gf_list_new();
	proto_node->scripts_to_load = gf_list_new();

	proto_node->proto_interface = proto;
	gf_list_add(proto->instances, proto_node);

#ifdef GF_NODE_USE_POINTERS
	proto_node->sgprivate->node_del = protoinst_del;
	proto_node->sgprivate->get_field = protoinst_get_field;
	proto_node->sgprivate->get_field_count = gf_sg_proto_get_num_fields;
	proto_node->sgprivate->name = strdup(proto->Name);
#else
	proto_node->proto_name = strdup(proto->Name);
#endif


	/*create the namespace*/
	proto_node->sgprivate->scenegraph = gf_sg_new_subscene(scene);
	/*set this proto as owner of the new graph*/
	proto_node->sgprivate->scenegraph->pOwningProto = proto_node;

	/*instanciate fields*/
	i=0;
	while ((field = (GF_ProtoFieldInterface*)gf_list_enum(proto->proto_fields, &i))) {
		inst = (GF_ProtoField *)malloc(sizeof(GF_ProtoField));
		inst->EventType = field->EventType;
		inst->FieldType = field->FieldType;
		inst->has_been_accessed = 0;

		/*this is OK to call on GF_Node (returns NULL) and MFNode (returns gf_list_new() )*/
		inst->field_pointer = gf_sg_vrml_field_pointer_new(inst->FieldType);

		/*regular field, duplicate from default value or instanciated one if specified (since
		a proto may be partially instanciated when used in another proto)*/
		if (gf_sg_vrml_get_sf_type(inst->FieldType) != GF_SG_VRML_SFNODE) {
			if (from_inst) {
				from_field = (GF_ProtoField *)gf_list_get(from_inst->fields, i-1);
				gf_sg_vrml_field_copy(inst->field_pointer, from_field->field_pointer, inst->FieldType);
			} else {
				gf_sg_vrml_field_copy(inst->field_pointer, field->def_value, inst->FieldType);
			}
		}
		/*No default values for SFNodes as interfaces ...*/
		gf_list_add(proto_node->fields, inst);
	}
	return (GF_Node *) proto_node;
}



GF_Node *gf_sg_proto_create_instance(GF_SceneGraph *sg, GF_Proto *proto)
{
	return gf_sg_proto_create_node(sg, proto, NULL);
}

GF_Err gf_sg_proto_load_code(GF_Node *node)
{
	GF_ProtoInstance *inst;
	if (node->sgprivate->tag != TAG_ProtoNode) return GF_BAD_PARAM;
	inst = (GF_ProtoInstance *) node;
	if (!inst->proto_interface) return GF_BAD_PARAM;
	if (inst->is_loaded) return GF_OK;
	gf_sg_proto_instanciate(inst);
	return GF_OK;
}


u32 gf_sg_proto_get_num_fields(GF_Node *node, u8 code_mode)
{
	GF_ProtoInstance *proto;
	if (!node) return 0;

	proto = (GF_ProtoInstance *)node;
	/*watchout for deletion case*/
	switch (code_mode) {
	case GF_SG_FIELD_CODING_IN: return proto->proto_interface ? proto->proto_interface->NumIn : 0;
	case GF_SG_FIELD_CODING_OUT: return proto->proto_interface ? proto->proto_interface->NumOut : 0;
	case GF_SG_FIELD_CODING_DEF: return proto->proto_interface ? proto->proto_interface->NumDef : 0;
	case GF_SG_FIELD_CODING_ALL: return gf_list_count(proto->proto_interface ? proto->proto_interface->proto_fields : proto->fields);
	/*BIFS-ANIM not supported*/
	case GF_SG_FIELD_CODING_DYN:
	default:
		return 0;
	}
}


void gf_sg_proto_del_instance(GF_ProtoInstance *inst)
{
	GF_SceneGraph *sg;
	GF_ProtoField *field;
	GF_Node *node;
	u32 index;

	index = 0;
	while (gf_list_count(inst->fields)) {
		field = (GF_ProtoField *)gf_list_get(inst->fields, 0);
		gf_list_rem(inst->fields, 0);

		/*regular type*/
		if ( (field->FieldType!=GF_SG_VRML_SFNODE) && (field->FieldType!=GF_SG_VRML_MFNODE)) {
			gf_sg_vrml_field_pointer_del(field->field_pointer, field->FieldType);
		}
		/*node types: delete instances*/
		else if (field->field_pointer) {
			if (field->FieldType == GF_SG_VRML_SFNODE) {
				gf_node_unregister((GF_Node *) field->field_pointer, (GF_Node *) inst);
			} else {
				GF_ChildNodeItem *list = (GF_ChildNodeItem *)field->field_pointer;
				while (list) {
					GF_ChildNodeItem *cur = list;
					gf_node_unregister(list->node, (GF_Node *) inst);
					list = list->next;
					free(cur);
				}
			}
		}

		free(field);
		index++;
	}
	gf_list_del(inst->fields);

	/*destroy the code*/
	while (gf_list_count(inst->node_code)) {
		node = (GF_Node*)gf_list_get(inst->node_code, 0);
		gf_node_unregister(node, (GF_Node*) inst);
		gf_list_rem(inst->node_code, 0);
	}
	gf_list_del(inst->node_code);

	assert(!gf_list_count(inst->scripts_to_load));
	gf_list_del(inst->scripts_to_load);

	if (inst->proto_interface) gf_list_del_item(inst->proto_interface->instances, inst);

	sg = inst->sgprivate->scenegraph;

#ifdef GF_NODE_USE_POINTERS
	/*this is duplicated for proto since a proto declaration may be destroyed while instances are active*/
	free((char *) inst->sgprivate->name);
#else
	free((char *) inst->proto_name);
#endif

	gf_node_free((GF_Node *)inst);
	/*and finally destroy the scene graph - do it last to handle the case of hardcoded protos which may need
	the scenegraph pointer when being destroyed*/
	gf_sg_del(sg);
}

/*Note on ISed fields: we cannot support fan-in on proto, eg we assume only one eventIn field can recieve events
thus situations where a proto recieves eventIn from outside and the node with ISed eventIn recieves event 
from inside the proto are undefined*/
GF_Err gf_sg_proto_field_set_ised(GF_Proto *proto, u32 protoFieldIndex, GF_Node *node, u32 nodeFieldIndex)
{
	GF_Err e;
	GF_Route *r;
	GF_FieldInfo field, nodeField;
	field.fieldIndex = protoFieldIndex;
	e = gf_sg_proto_get_field(proto, NULL, &field);
	if (e) return e;
	e = gf_node_get_field(node, nodeFieldIndex, &nodeField);
	if (e) return e;
	if (field.fieldType != nodeField.fieldType) {
		if ((gf_sg_vrml_get_sf_type(field.fieldType)==GF_SG_VRML_SFSTRING) && (gf_sg_vrml_get_sf_type(nodeField.fieldType) == GF_SG_VRML_SFURL)) {
			e = GF_OK;
		} else if ((gf_sg_vrml_get_sf_type(field.fieldType)==GF_SG_VRML_SFURL) && (gf_sg_vrml_get_sf_type(nodeField.fieldType) == GF_SG_VRML_SFSTRING)) {
			e = GF_OK;
		} else {
//			printf("error in IS - node field %s.%s - inType %s - outType %s\n", gf_node_get_class_name(node) , nodeField.name, gf_sg_vrml_get_field_type_by_name(field.fieldType), gf_sg_vrml_get_field_type_by_name(nodeField.fieldType));
			return GF_SG_INVALID_PROTO;
		}
	}

	GF_SAFEALLOC(r, GF_Route)
	if (!r) return GF_OUT_OF_MEM;
	r->IS_route = 1;

	if (nodeField.eventType==GF_SG_EVENT_OUT) {
		r->FromField.fieldIndex = nodeFieldIndex;
		r->FromNode = node;
		r->ToField.fieldIndex = protoFieldIndex;
		r->ToNode = NULL;
		if (!node->sgprivate->interact) GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
		if (!node->sgprivate->interact->events) node->sgprivate->interact->events = gf_list_new();
		gf_list_add(node->sgprivate->interact->events, r);
	} else {
		switch (field.eventType) {
		case GF_SG_EVENT_FIELD:
		case GF_SG_EVENT_EXPOSED_FIELD:
		case GF_SG_EVENT_IN:
			r->FromField.fieldIndex = protoFieldIndex;
			r->FromNode = NULL;
			r->ToField.fieldIndex = nodeFieldIndex;
			r->ToNode = node;
			break;
		case GF_SG_EVENT_OUT:
			r->FromField.fieldIndex = nodeFieldIndex;
			r->FromNode = node;
			r->ToField.fieldIndex = protoFieldIndex;
			r->ToNode = NULL;
			if (!node->sgprivate->interact) GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
			if (!node->sgprivate->interact->events) node->sgprivate->interact->events = gf_list_new();
			break;
		default:
			free(r);
			return GF_BAD_PARAM;
		}
	}
	r->graph = proto->sub_graph;
	return gf_list_add(proto->sub_graph->Routes, r);
}

GF_Err gf_sg_proto_instance_set_ised(GF_Node *protoinst, u32 protoFieldIndex, GF_Node *node, u32 nodeFieldIndex)
{
	GF_Err e;
	GF_Route *r;
	GF_FieldInfo field, nodeField;
	if (protoinst->sgprivate->tag != TAG_ProtoNode) return GF_BAD_PARAM;
	
	e = gf_node_get_field(protoinst, protoFieldIndex, &field);
	if (e) return e;
	e = gf_node_get_field(node, nodeFieldIndex, &nodeField);
	if (e) return e;
	if (field.fieldType != nodeField.fieldType) {
		if ((gf_sg_vrml_get_sf_type(field.fieldType)==GF_SG_VRML_SFSTRING) && (gf_sg_vrml_get_sf_type(nodeField.fieldType) == GF_SG_VRML_SFURL)) {
			e = GF_OK;
		} else if ((gf_sg_vrml_get_sf_type(field.fieldType)==GF_SG_VRML_SFURL) && (gf_sg_vrml_get_sf_type(nodeField.fieldType) == GF_SG_VRML_SFSTRING)) {
			e = GF_OK;
		} else {
//			printf("error in IS - node field %s.%s - inType %s - outType %s\n", gf_node_get_class_name(node) , nodeField.name, gf_sg_vrml_get_field_type_by_name(field.fieldType), gf_sg_vrml_get_field_type_by_name(nodeField.fieldType));
			return GF_SG_INVALID_PROTO;
		}
	}

	GF_SAFEALLOC(r, GF_Route)
	if (!r) return GF_OUT_OF_MEM;
	r->IS_route = 1;

	if (nodeField.eventType==GF_SG_EVENT_OUT) {
		r->FromField.fieldIndex = nodeFieldIndex;
		r->FromNode = node;
		r->ToField.fieldIndex = protoFieldIndex;
		r->ToNode = protoinst;
		if (!node->sgprivate->interact) GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
		if (!node->sgprivate->interact->events) node->sgprivate->interact->events = gf_list_new();
		gf_list_add(node->sgprivate->interact->events, r);
	} else {
		switch (field.eventType) {
		case GF_SG_EVENT_FIELD:
		case GF_SG_EVENT_EXPOSED_FIELD:
		case GF_SG_EVENT_IN:
			r->FromField.fieldIndex = protoFieldIndex;
			r->FromNode = protoinst;
			r->ToField.fieldIndex = nodeFieldIndex;
			r->ToNode = node;
			break;
		case GF_SG_EVENT_OUT:
			r->FromField.fieldIndex = nodeFieldIndex;
			r->FromNode = node;
			r->ToField.fieldIndex = protoFieldIndex;
			r->ToNode = protoinst;
			if (!node->sgprivate->interact) GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
			if (!node->sgprivate->interact->events) node->sgprivate->interact->events = gf_list_new();
			gf_list_add(node->sgprivate->interact->events, r);
			break;
		default:
			free(r);
			return GF_BAD_PARAM;
		}
	}
	r->graph = node->sgprivate->scenegraph;
	gf_sg_route_activate(r);
	return gf_list_add(r->graph->Routes, r);
}


GF_Err gf_bifs_proto_field_set_aq_info(GF_ProtoFieldInterface *field, 
										u32 QP_Type, 
										u32 hasMinMax, 
										u32 QPSFType, 
										void *qp_min_value, 
										void *qp_max_value, 
										u32 QP13_NumBits)
{

	if (!field) return GF_BAD_PARAM;
	if (!QP_Type) return GF_OK;
	if (!gf_sg_vrml_is_sf_field(QPSFType)) return GF_BAD_PARAM;

	field->QP_Type = QP_Type;
	field->hasMinMax = hasMinMax;
	if (hasMinMax) {
		if (qp_min_value) {
			field->qp_min_value = gf_sg_vrml_field_pointer_new(QPSFType);
			gf_sg_vrml_field_copy(field->qp_min_value, qp_min_value, QPSFType);
		}
		if (qp_max_value) {
			field->qp_max_value = gf_sg_vrml_field_pointer_new(QPSFType);
			gf_sg_vrml_field_copy(field->qp_max_value, qp_max_value, QPSFType);
		}
	}
	field->NumBits = QP13_NumBits;
	return GF_OK;
}


GF_Err gf_sg_proto_get_field_index(GF_ProtoInstance *proto, u32 index, u32 code_mode, u32 *all_index)
{
	u32 i;
	GF_ProtoFieldInterface *proto_field;

	i=0;
	while ((proto_field = (GF_ProtoFieldInterface*)gf_list_enum(proto->proto_interface->proto_fields, &i))) {
		assert(proto_field);
		switch (code_mode) {
		case GF_SG_FIELD_CODING_IN:
			if (proto_field->IN_index == index) {
				*all_index = proto_field->ALL_index;
				return GF_OK;
			}
			break;
		case GF_SG_FIELD_CODING_OUT:
			if (proto_field->OUT_index == index) {
				*all_index = proto_field->ALL_index;
				return GF_OK;
			}
			break;
		case GF_SG_FIELD_CODING_DEF:
			if (proto_field->DEF_index == index) {
				*all_index = proto_field->ALL_index;
				return GF_OK;
			}
			break;
		case GF_SG_FIELD_CODING_ALL:
			if (proto_field->ALL_index == index) {
				*all_index = proto_field->ALL_index;
				return GF_OK;
			}
			break;
		/*BIFS-ANIM not supported*/
		case GF_SG_FIELD_CODING_DYN:
		default:
			return GF_BAD_PARAM;
		}		
	}
	return GF_BAD_PARAM;
}

u32 gf_sg_proto_get_field_count(GF_Proto *proto)
{
	if (!proto) return 0;
	return gf_list_count(proto->proto_fields);
}

GF_ProtoFieldInterface *gf_sg_proto_field_find(GF_Proto *proto, u32 fieldIndex)
{
	if (!proto) return NULL;
	return (GF_ProtoFieldInterface*)gf_list_get(proto->proto_fields, fieldIndex);
}

void gf_sg_proto_check_field_change(GF_Node *node, u32 fieldIndex)
{
	u32 i;

	GF_Route *r;
	if (!node) return;

	if ((node->sgprivate->tag == TAG_ProtoNode) && node->sgprivate->interact && node->sgprivate->interact->events){
		i=0;
		while ((r = (GF_Route*)gf_list_enum(node->sgprivate->interact->events, &i))) {
			if (!r->IS_route) continue;
			/*eventIn or exposedField*/
			if ((r->FromNode == node) && (r->FromField.fieldIndex == fieldIndex) ) {
				if (gf_sg_route_activate(r)) 
					gf_node_changed(r->ToNode, &r->FromField);
			}
			/*eventOut*/
			if ((r->ToNode == node) && (r->ToField.fieldIndex == fieldIndex) ) {
				gf_sg_route_activate(r);
			}
		}
		/*no notification for proto changes*/
		return;
	}
	/*the node has to belong to a proto graph*/
	if (! node->sgprivate->scenegraph->pOwningProto) return;
	/*no routes defined*/
	if (!node->sgprivate->interact) return;

	/*search for IS routes_events in the node and activate them. Field can also be an eventOut !!*/
	i=0;
	while ((r = (GF_Route*)gf_list_enum(node->sgprivate->interact->events, &i))) {
		if (!r->IS_route) continue;
		/*activate eventOuts*/
		if ((r->FromNode == node) && (r->FromField.fieldIndex == fieldIndex)) {
			gf_sg_route_activate(r);
		}
		/*or eventIns / (exposed)Fields*/
		else if ((r->ToNode == node) && (r->ToField.fieldIndex == fieldIndex)) {
			/*don't forget to notify the node it has changed*/
			if (gf_sg_route_activate(r)) 
				gf_node_changed(node, &r->ToField);
		}
	}
}


Bool gf_sg_proto_get_aq_info(GF_Node *Node, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	GF_Proto *proto;
	u32 i;
	GF_ProtoFieldInterface *proto_field;

	proto = ((GF_ProtoInstance *)Node)->proto_interface;

	i=0;
	while ((proto_field = (GF_ProtoFieldInterface*)gf_list_enum(proto->proto_fields, &i))) {
		if (proto_field->ALL_index!=FieldIndex) continue;
		
		*QType = proto_field->QP_Type;
		*AType = proto_field->Anim_Type;
		*b_min = FIX_MIN;
		*b_max = FIX_MAX;

		if (proto_field->hasMinMax) {

			/*translate our bounds*/
			switch (gf_sg_vrml_get_sf_type(proto_field->FieldType)) {
			case GF_SG_VRML_SFINT32:
				*b_min = (SFFloat) * ( (SFInt32 *) proto_field->qp_min_value);
				*b_max = (SFFloat) * ( (SFInt32 *) proto_field->qp_max_value);
				break;
			/*TO DO EVERYWHERE: check on field translation from double to float
			during quant bounds*/
			case GF_SG_VRML_SFTIME:
				*b_min = (SFFloat) * ( (SFTime *) proto_field->qp_min_value);
				*b_max = (SFFloat) * ( (SFTime *) proto_field->qp_max_value);
				break;
			default:
				if (proto_field->qp_min_value)
					*b_min = (SFFloat) * ( (SFFloat *) proto_field->qp_min_value);
				if (proto_field->qp_max_value)
					*b_max = (SFFloat) * ( (SFFloat *) proto_field->qp_max_value);
				break;
			}

		}
		*QT13_bits = proto_field->NumBits;
		return 1;
	}
	return 0;
}


GF_EXPORT
GF_Proto *gf_node_get_proto(GF_Node *node)
{
	GF_ProtoInstance *inst;
	if (node->sgprivate->tag != TAG_ProtoNode) return NULL;
	inst = (GF_ProtoInstance *) node;
	return inst->proto_interface;
}

/*returns the ID of the proto*/
u32 gf_sg_proto_get_id(GF_Proto *proto)
{
	return proto->ID;
}

const char *gf_sg_proto_get_class_name(GF_Proto *proto)
{
	return (const char *) proto->Name;
}

u32 gf_sg_proto_get_root_tag(GF_Proto *proto)
{
	GF_Node *n;
	if (!proto) return TAG_UndefinedNode;
	n = (GF_Node*)gf_list_get(proto->node_code, 0);
	if (!n) return TAG_UndefinedNode;
	if (n->sgprivate->tag == TAG_ProtoNode) return gf_sg_proto_get_root_tag(((GF_ProtoInstance *)n)->proto_interface);
	return n->sgprivate->tag;
}

Bool gf_sg_proto_field_is_sftime_offset(GF_Node *node, GF_FieldInfo *field)
{
	u32 i;
	GF_Route *r;
	GF_ProtoInstance *inst;
	GF_FieldInfo inf;
	if (node->sgprivate->tag != TAG_ProtoNode) return 0;
	if (field->fieldType != GF_SG_VRML_SFTIME) return 0;

	inst = (GF_ProtoInstance *) node;
	/*check in interface if this is ISed */
	i=0;
	while ((r = (GF_Route*)gf_list_enum(inst->proto_interface->sub_graph->Routes, &i))) {
		if (!r->IS_route) continue;
		/*only check eventIn/field/exposedField*/
		if (r->FromNode || (r->FromField.fieldIndex != field->fieldIndex)) continue;

		gf_node_get_field(r->ToNode, r->ToField.fieldIndex, &inf);
		/*IS to another proto*/
		if (r->ToNode->sgprivate->tag == TAG_ProtoNode) return gf_sg_proto_field_is_sftime_offset(r->ToNode, &inf);
		/*IS to a startTime/stopTime field*/
		if (!stricmp(inf.name, "startTime") || !stricmp(inf.name, "stopTime")) return 1;
	}
	return 0;
}

GF_SceneGraph *Node_GetExternProtoScene(GF_Node *node)
{
	GF_SceneGraph *sg;
	sg = node->sgprivate->scenegraph;
	if (!sg->pOwningProto) return NULL;
	if (!sg->pOwningProto->proto_interface->ExternProto.count) return NULL;
	sg = sg->pOwningProto->proto_interface->parent_graph;
	while (sg->parent_scene) sg = sg->parent_scene;
	return sg;
}


GF_EXPORT
GF_Node *gf_node_get_proto_root(GF_Node *node)
{
	if (!node || (node->sgprivate->tag!=TAG_ProtoNode)) return NULL;
	return ((GF_ProtoInstance *)node)->RenderingNode;
}

GF_EXPORT
GF_Node *gf_node_get_proto_parent(GF_Node *node)
{
	if (!node) return NULL;
	if (node->sgprivate->scenegraph->pOwningProto) {
		GF_Node *the_node = (GF_Node *) node->sgprivate->scenegraph->pOwningProto;
		if (the_node != node) return the_node;
	} 
	return NULL;
}

GF_EXPORT
Bool gf_node_is_proto_root(GF_Node *node)
{
	if (!node) return 0;
	if (!node->sgprivate->scenegraph->pOwningProto) return 0;

	if (gf_list_find(node->sgprivate->scenegraph->pOwningProto->node_code, node)>=0) return 1;
	return 0;
}
