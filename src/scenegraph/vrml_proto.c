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

GF_EXPORT
GF_Proto *gf_sg_proto_new(GF_SceneGraph *inScene, u32 ProtoID, char *name, Bool unregistered)
{
	GF_Proto *tmp;
	if (!inScene) return NULL;

	/*make sure we don't define a proto already defined in this scope*/
	if (!unregistered) {
		tmp = gf_sg_find_proto(inScene, ProtoID, name);
		if (tmp) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[Scenegraph] PROTO %s redefined - skipping loading\n", name));
			return NULL;
		}
	}

	GF_SAFEALLOC(tmp, GF_Proto)
	if (!tmp) return NULL;

	tmp->proto_fields = gf_list_new();
	tmp->node_code = gf_list_new();
	tmp->parent_graph = inScene;
	tmp->sub_graph = gf_sg_new_subscene(inScene);
	tmp->instances = gf_list_new();

	if (name)
		tmp->Name = gf_strdup(name);
	else
		tmp->Name = gf_strdup("Unnamed Proto");
	tmp->ID = ProtoID;
	if (!unregistered) {
		gf_list_add(inScene->protos, tmp);
	} else {
		gf_list_add(inScene->unregistered_protos, tmp);
	}
	return tmp;
}

#if 0
/*used for memory handling of scene graph only. move proto from off-graph to in-graph or reverse*/
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
#endif

GF_EXPORT
GF_Err gf_sg_proto_del(GF_Proto *proto)
{
	s32 i;

	if (!proto) return GF_OK;
	i = gf_list_del_item(proto->parent_graph->protos, proto);
	if (i<0) {
		gf_list_del_item(proto->parent_graph->unregistered_protos, proto);
	}
	if (proto->userpriv && proto->OnDelete) proto->OnDelete(proto->userpriv);

	/*first destroy the code*/
	while (gf_list_count(proto->node_code)) {
		GF_Node *node = (GF_Node*)gf_list_get(proto->node_code, 0);
		gf_node_unregister(node, NULL);
		gf_list_rem(proto->node_code, 0);
	}
	gf_list_del(proto->node_code);

	/*delete interface*/
	while (gf_list_count(proto->proto_fields)) {
		GF_ProtoFieldInterface *field = (GF_ProtoFieldInterface*)gf_list_get(proto->proto_fields, 0);
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

		if (field->FieldName) gf_free(field->FieldName);

		/*QP fields are SF fields, we can safely gf_free() them*/
		if (field->qp_max_value) gf_free(field->qp_max_value);
		if (field->qp_min_value) gf_free(field->qp_min_value);
		gf_free(field);
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


	if (proto->Name) gf_free(proto->Name);
	gf_sg_mfurl_del(proto->ExternProto);
	gf_list_del(proto->instances);
	gf_free(proto);
	return GF_OK;
}

GF_EXPORT
GF_SceneGraph *gf_sg_proto_get_graph(GF_Proto *proto)
{
	return proto ? proto->sub_graph : NULL;
}


#if 0
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
#endif

GF_EXPORT
MFURL *gf_sg_proto_get_extern_url(GF_Proto *proto)
{
	return proto ? &proto->ExternProto : NULL;
}

GF_EXPORT
GF_Err gf_sg_proto_add_node_code(GF_Proto *proto, GF_Node *pNode)
{
	if (!proto) return GF_BAD_PARAM;
	return gf_list_add(proto->node_code, pNode);
}

GF_EXPORT
GF_ProtoFieldInterface *gf_sg_proto_field_find_by_name(GF_Proto *proto, char *fieldName)
{
	GF_ProtoFieldInterface *ptr;
	u32 i=0;
	while ((ptr = (GF_ProtoFieldInterface*)gf_list_enum(proto->proto_fields, &i))) {
		if (ptr->FieldName && !strcmp(ptr->FieldName, fieldName)) return ptr;
	}
	return NULL;
}

GF_EXPORT
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

	if (fieldName) tmp->FieldName = gf_strdup(fieldName);

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

#if 0 //unused
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
#endif

GF_EXPORT
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

	/*otherwise this is an instantiated proto*/
	if (node->sgprivate->tag!=TAG_ProtoNode) return GF_BAD_PARAM;

	inst = (GF_ProtoInstance *) node;
	field = (GF_ProtoField*)gf_list_get(inst->fields, info->fieldIndex);
	if (!field) return GF_BAD_PARAM;

	info->fieldType = field->FieldType;
	info->eventType = field->EventType;
	info->on_event_in = field->on_event_in;
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
	GF_Proto *__proto=NULL;

	if (!node && !proto) return -1;
	if (node && (node->sgprivate->tag!=TAG_ProtoNode)) return -1;

	if (proto)
		__proto = proto;
	else if (node)
		__proto = ((GF_ProtoInstance *) node)->proto_interface;

	if (!__proto ) return -1;

	for (i=0; i<gf_list_count(__proto->proto_fields); i++) {
		GF_ProtoFieldInterface *proto_field = (GF_ProtoFieldInterface*)gf_list_get(__proto->proto_fields, i);
		if (proto_field->FieldName && !strcmp(proto_field->FieldName, name)) return i;
	}
	return -1;
}


GF_Node *gf_vrml_node_clone(GF_SceneGraph *inScene, GF_Node *orig, GF_Node *cloned_parent, char *inst_id_suffix)
{
	u32 i, count, id;
	char *szNodeName;
	Bool is_script;
	GF_Node *node, *child;
	GF_ChildNodeItem *list, *last;
	GF_Route *r1;
#ifndef GPAC_DISABLE_BIFS
	void BIFS_SetupConditionalClone(GF_Node *node, GF_Node *orig);
#endif
	GF_ProtoInstance *proto;
	GF_FieldInfo field_orig, field;

	/*this is not a mistake*/
	if (!orig) return NULL;

	/*check for DEF/USE*/
	szNodeName = NULL;
	if (!inst_id_suffix) id = 0;
	else {
		const char *orig_name = gf_node_get_name_and_id(orig, &id);
		/*generate clone IDs based on original one*/
		if (inst_id_suffix[0] && id) {
			id = gf_sg_get_next_available_node_id(inScene);
			if (orig_name) {
				szNodeName = gf_malloc(sizeof(char)*(strlen(orig_name)+strlen(inst_id_suffix)+1));
				strcpy(szNodeName, orig_name);
				strcat(szNodeName, inst_id_suffix);
			}
		}
		else if (orig_name) szNodeName = gf_strdup(orig_name);
	}

	if (id) {
		node = szNodeName ? gf_sg_find_node_by_name(inScene, szNodeName) : gf_sg_find_node(inScene, id);
		/*node already created, USE*/
		if (node) {
			gf_node_register(node, cloned_parent);
			if (szNodeName) gf_free(szNodeName);
			return node;
		}
	}
	/*create a node*/
	if (orig->sgprivate->tag == TAG_ProtoNode) {
		GF_Proto *proto_node = ((GF_ProtoInstance *)orig)->proto_interface;
		/*create the instance but don't load the code -c we MUST wait for ISed routes to be cloned before*/
		node = gf_sg_proto_create_node(inScene, proto_node, (GF_ProtoInstance *) orig);
	} else {
		node = gf_node_new(inScene, orig->sgprivate->tag);
	}

	count = gf_node_get_field_count(orig);

	is_script = 0;
	if (orig->sgprivate->tag==TAG_MPEG4_Script) is_script = 1;
#ifndef GPAC_DISABLE_X3D
	else if (orig->sgprivate->tag==TAG_X3D_Script) is_script = 1;
#endif

	if (is_script) gf_sg_script_prepare_clone(node, orig);


	/*register node*/
	if (id) {
		gf_node_set_id(node, id, szNodeName);
		if (szNodeName) gf_free(szNodeName);
	}
	gf_node_register(node, cloned_parent);

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
			child = gf_node_clone(inScene, (* ((GF_Node **) field_orig.far_ptr)), node, inst_id_suffix, 1);
			*((GF_Node **) field.far_ptr) = child;
			break;
		case GF_SG_VRML_MFNODE:
			last = NULL;
			list = *( (GF_ChildNodeItem **) field_orig.far_ptr);
			while (list) {
				child = gf_node_clone(inScene, list->node, node, inst_id_suffix, 1);
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
			} else if (!stricmp(field.name, "startTime") || !stricmp(field_orig.name, "startTime") ) {
				*((SFTime *)field.far_ptr) += inScene->GetSceneTime(inScene->userpriv);
			}
			break;
		default:
			gf_sg_vrml_field_clone(field.far_ptr, field_orig.far_ptr, field.fieldType, inScene);
			break;
		}
	}

#ifndef GPAC_DISABLE_BIFS
	/*init node before creating ISed routes so the eventIn handler are in place*/
	if (node->sgprivate->tag == TAG_MPEG4_Conditional)
		BIFS_SetupConditionalClone(node, orig);
	else
#endif
		if (node->sgprivate->tag != TAG_ProtoNode) gf_node_init(node);

	if (!inScene->pOwningProto) return node;
	proto = inScene->pOwningProto;

	/*create Routes for ISed fields*/
	i=0;
	while ((r1 = (GF_Route*)gf_list_enum(proto->proto_interface->sub_graph->Routes, &i))) {
		GF_Route *r2 = NULL;
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
		gf_sg_proto_instantiate((GF_ProtoInstance *)node);
	}
	return node;
}

GF_Err gf_sg_proto_get_field_ind_static(GF_Node *Node, u32 inField, u8 IndexMode, u32 *allField)
{
	return gf_sg_proto_get_field_index((GF_ProtoInstance *)Node, inField, IndexMode, allField);
}


static Bool is_same_proto(GF_Proto *extern_proto, GF_Proto *proto)
{
	u32 i, count;
	/*VRML allows external protos to have more fields defined that the externProto referencing them*/
	if (gf_list_count(extern_proto->proto_fields) > gf_list_count(proto->proto_fields)) return 0;
	count = gf_list_count(extern_proto->proto_fields);
	for (i=0; i<count; i++) {
		GF_ProtoFieldInterface *pf1 = (GF_ProtoFieldInterface*)gf_list_get(extern_proto->proto_fields, i);
		GF_ProtoFieldInterface *pf2 = (GF_ProtoFieldInterface*)gf_list_get(proto->proto_fields, i);
		if (pf1->EventType != pf2->EventType) return 0;
		if (pf1->FieldType != pf2->FieldType) return 0;
		/*note we don't check names since we're not sure both protos use name coding (MPEG4 only)*/
	}
	return 1;
}

static GF_Proto *find_proto_by_interface(GF_SceneGraph *sg, GF_Proto *extern_proto)
{
	GF_Proto *proto;
	u32 i, count;

	assert(sg);

	/*browse all top-level */
	i=0;
	while ((proto = (GF_Proto*)gf_list_enum(sg->protos, &i))) {
		if (is_same_proto(proto, extern_proto)) return proto;
	}
	/*browse all top-level unregistered in reverse order*/
	count = gf_list_count(sg->unregistered_protos);
	for (i=count; i>0; i--) {
		proto = (GF_Proto*)gf_list_get(sg->unregistered_protos, i-1);
		if (is_same_proto(proto, extern_proto)) return proto;
	}
	return NULL;
}

/*performs common initialization of routes ISed fields and protos once everything is loaded*/
void gf_sg_proto_instantiate(GF_ProtoInstance *proto_node)
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
		if (PTR_TO_U_CAST extern_lib == GF_SG_INTERNAL_PROTO) {
			proto_node->sgprivate->flags |= GF_SG_NODE_DIRTY;
			// take default values
			count = gf_list_count(owner->proto_fields);
			for (i=0; i<count; i++) {
				GF_ProtoField *pf = (GF_ProtoField *)gf_list_get(proto_node->fields, i);
				if (!pf->has_been_accessed) {
					pfi = (GF_ProtoFieldInterface*)gf_list_get(proto->proto_fields, i);
					gf_sg_vrml_field_copy(pf->field_pointer, pfi->def_value, pfi->FieldType);
				}
			}
			owner->parent_graph->NodeCallback(owner->parent_graph->userpriv, GF_SG_CALLBACK_INIT, (GF_Node *) proto_node, NULL);
			proto_node->flags |= GF_SG_PROTO_LOADED | GF_SG_PROTO_HARDCODED;
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
				if (sscanf(szName, "%u", &ID)) ID = (u32) -1;
			}
			/*if we have the proto name, use it*/
			if (owner->Name) szName = owner->Name;
			proto = gf_sg_find_proto(extern_lib, ID, szName);
		}
		if (!proto) proto = gf_sg_find_proto(extern_lib, owner->ID, owner->Name);
		if (!proto) proto = find_proto_by_interface(extern_lib, owner);

		if (proto && !is_same_proto(owner, proto)) {
			proto = NULL;
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[Scenegraph] fields/types mismatch for PROTO %s - skipping instantiation\n", owner->Name));
		}
		/*couldn't find proto in the given lib, consider the proto as loaded (give up)*/
		if (!proto) {
			proto_node->flags |= GF_SG_PROTO_LOADED;
			return;
		}
		/*cf VRML: once an external proto is loaded, copy back the default field values of the external proto*/
		count = gf_list_count(owner->proto_fields);
		for (i=0; i<count; i++) {
			GF_ProtoField *pf = (GF_ProtoField *)gf_list_get(proto_node->fields, i);
			if (!pf->has_been_accessed) {
				pfi = (GF_ProtoFieldInterface*)gf_list_get(proto->proto_fields, i);
				gf_sg_vrml_field_copy(pf->field_pointer, pfi->def_value, pfi->FieldType);
			} else {
				//pfi = (GF_ProtoFieldInterface*)gf_list_get(proto->proto_fields, i);
			}
		}

		/*unregister from prev and reg with real proto*/
		gf_list_del_item(owner->instances, proto_node);
		gf_list_add(proto->instances, proto_node);
	}

	/*OVERRIDE the proto instance (eg don't instantiate an empty externproto...)*/
	proto_node->proto_interface = proto;

	/*clone all nodes*/
	i=0;
	while ((orig = (GF_Node*)gf_list_enum(proto->node_code, &i))) {
		/*node is cloned in the new scenegraph and its parent is NULL */
		node = gf_node_clone(proto_node->sgprivate->scenegraph, orig, NULL, "", 1);
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
		*/

		if (route->is_setup) {
			if ((route->ToField.eventType == GF_SG_EVENT_IN) && (route->FromField.eventType == GF_SG_EVENT_IN) ) continue;
		}

		if (route->ToNode->sgprivate->tag==TAG_MPEG4_Script)
			gf_sg_route_activate(route);
#ifndef GPAC_DISABLE_X3D
		else if (route->ToNode->sgprivate->tag==TAG_X3D_Script)
			gf_sg_route_activate(route);
#endif
	}

#if 0
	/*reset all regular route activation times - if we don't do so, creating a proto by script and then manipulating one of its
	ISed field may not trigger the proper routes*/
	i=0;
	while ((route = (GF_Route*)gf_list_enum(proto_node->sgprivate->scenegraph->Routes, &i))) {
		if (!route->IS_route) {
			route->lastActivateTime = 0;
		}
	}
#endif
	proto_node->flags |= GF_SG_PROTO_LOADED;
}

void gf_sg_proto_mark_field_loaded(GF_Node *proto_inst, GF_FieldInfo *info)
{
	GF_ProtoInstance *inst= (proto_inst->sgprivate->tag==TAG_ProtoNode) ? (GF_ProtoInstance *)proto_inst : NULL;
	GF_ProtoField *pf = inst ? (GF_ProtoField *)gf_list_get(inst->fields, info->fieldIndex) : NULL;
	if (pf) pf->has_been_accessed = 1;
}

GF_Node *gf_sg_proto_create_node(GF_SceneGraph *scene, GF_Proto *proto, GF_ProtoInstance *from_inst)
{
	u32 i;
	GF_ProtoField *inst, *from_field;
	GF_ProtoFieldInterface *field;
	GF_ProtoInstance *proto_node;
	if (!proto) return NULL;
	
	GF_SAFEALLOC(proto_node, GF_ProtoInstance)
	if (!proto_node) return NULL;

	gf_node_setup((GF_Node *)proto_node, TAG_ProtoNode);
	proto_node->node_code = gf_list_new();
	proto_node->fields = gf_list_new();
	proto_node->scripts_to_load = gf_list_new();

	proto_node->proto_interface = proto;
	gf_list_add(proto->instances, proto_node);

	proto_node->proto_name = gf_strdup(proto->Name);

	/*create the namespace*/
	proto_node->sgprivate->scenegraph = gf_sg_new_subscene(scene);
	/*set this proto as owner of the new graph*/
	proto_node->sgprivate->scenegraph->pOwningProto = proto_node;

	/*instantiate fields*/
	i=0;
	while ((field = (GF_ProtoFieldInterface*)gf_list_enum(proto->proto_fields, &i))) {
		GF_SAFEALLOC(inst, GF_ProtoField);
		if (!inst) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRML] Failed to allocate proto instance field\n]"));
			continue;
		}
		
		inst->EventType = field->EventType;
		inst->FieldType = field->FieldType;

		/*this is OK to call on GF_Node (returns NULL) and MFNode (returns gf_list_new() )*/
		inst->field_pointer = gf_sg_vrml_field_pointer_new(inst->FieldType);

		/*regular field, duplicate from default value or instantiated one if specified (since
		a proto may be partially instantiated when used in another proto)*/
		if (gf_sg_vrml_get_sf_type(inst->FieldType) != GF_SG_VRML_SFNODE) {
			if (from_inst) {
				from_field = (GF_ProtoField *)gf_list_get(from_inst->fields, i-1);
				gf_sg_vrml_field_copy(inst->field_pointer, from_field->field_pointer, inst->FieldType);
				inst->has_been_accessed = from_field->has_been_accessed;
			} else {
				gf_sg_vrml_field_copy(inst->field_pointer, field->def_value, inst->FieldType);
			}
		}
		/*No default values for SFNodes as interfaces ...*/
		gf_list_add(proto_node->fields, inst);
	}
	return (GF_Node *) proto_node;
}


GF_EXPORT
GF_Node *gf_sg_proto_create_instance(GF_SceneGraph *sg, GF_Proto *proto)
{
	return gf_sg_proto_create_node(sg, proto, NULL);
}

GF_EXPORT
GF_Err gf_sg_proto_load_code(GF_Node *node)
{
	GF_ProtoInstance *inst;
	if (node->sgprivate->tag != TAG_ProtoNode) return GF_BAD_PARAM;
	inst = (GF_ProtoInstance *) node;
	if (!inst->proto_interface) return GF_BAD_PARAM;
	if (inst->flags & GF_SG_PROTO_LOADED) return GF_OK;
	gf_sg_proto_instantiate(inst);
	return GF_OK;
}


u32 gf_sg_proto_get_num_fields(GF_Node *node, u8 code_mode)
{
	GF_ProtoInstance *proto;
	if (!node) return 0;

	proto = (GF_ProtoInstance *)node;
	/*watchout for deletion case*/
	switch (code_mode) {
	case GF_SG_FIELD_CODING_IN:
		return proto->proto_interface ? proto->proto_interface->NumIn : 0;
	case GF_SG_FIELD_CODING_OUT:
		return proto->proto_interface ? proto->proto_interface->NumOut : 0;
	case GF_SG_FIELD_CODING_DEF:
		return proto->proto_interface ? proto->proto_interface->NumDef : 0;
	case GF_SG_FIELD_CODING_ALL:
		return gf_list_count(proto->proto_interface ? proto->proto_interface->proto_fields : proto->fields);
	/*BIFS-ANIM not supported*/
	case GF_SG_FIELD_CODING_DYN:
	default:
		return 0;
	}
}


void gf_sg_proto_del_instance(GF_ProtoInstance *inst)
{
	GF_SceneGraph *sg;

	while (gf_list_count(inst->fields)) {
		GF_ProtoField *field = (GF_ProtoField *)gf_list_get(inst->fields, 0);
		gf_list_rem(inst->fields, 0);

		if (field->field_pointer) {
			/*regular type*/
			if ( (field->FieldType!=GF_SG_VRML_SFNODE) && (field->FieldType!=GF_SG_VRML_MFNODE)) {
				gf_sg_vrml_field_pointer_del(field->field_pointer, field->FieldType);
			}
			/*node types: delete instances*/
			else {
				if (field->FieldType == GF_SG_VRML_SFNODE) {
					gf_node_unregister((GF_Node *) field->field_pointer, (GF_Node *) inst);
				} else {
					GF_ChildNodeItem *list = (GF_ChildNodeItem *)field->field_pointer;
					while (list) {
						GF_ChildNodeItem *cur = list;
						gf_node_unregister(list->node, (GF_Node *) inst);
						list = list->next;
						gf_free(cur);
					}
				}
			}
		}
		
		gf_free(field);
	}
	gf_list_del(inst->fields);

	/*destroy the code*/
	while (gf_list_count(inst->node_code)) {
		GF_Node *node = (GF_Node*)gf_list_get(inst->node_code, 0);
		gf_node_unregister(node, (GF_Node*) inst);
		gf_list_rem(inst->node_code, 0);
	}

	sg = inst->sgprivate->scenegraph;

	/*reset the scene graph before destroying the node code list, as unregistering nodes
	not destroyed in the previous phase (eg, cyclic references such as script and co) will
	refer to the node-code list*/
	gf_sg_reset(sg);
	sg->pOwningProto = NULL;

	gf_free((char *) inst->proto_name);
	gf_list_del(inst->node_code);
	assert(!gf_list_count(inst->scripts_to_load));
	gf_list_del(inst->scripts_to_load);

	if (inst->proto_interface && inst->proto_interface->instances) gf_list_del_item(inst->proto_interface->instances, inst);

	gf_node_free((GF_Node *)inst);
	gf_sg_del(sg);
}

/*Note on ISed fields: we cannot support fan-in on proto, eg we assume only one eventIn field can receive events
thus situations where a proto receives eventIn from outside and the node with ISed eventIn receives event
from inside the proto are undefined*/
GF_EXPORT
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
//			e = GF_OK;
		} else if ((gf_sg_vrml_get_sf_type(field.fieldType)==GF_SG_VRML_SFURL) && (gf_sg_vrml_get_sf_type(nodeField.fieldType) == GF_SG_VRML_SFSTRING)) {
//			e = GF_OK;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRML] error in IS - node field %s.%s - inType %s - outType %s\n", gf_node_get_class_name(node) , nodeField.name, gf_sg_vrml_get_field_type_name(field.fieldType), gf_sg_vrml_get_field_type_name(nodeField.fieldType)));
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
		if (!node->sgprivate->interact) {
			GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
			if (!node->sgprivate->interact) {
				return GF_OUT_OF_MEM;
			}
		}
		if (!node->sgprivate->interact->routes) node->sgprivate->interact->routes = gf_list_new();
		gf_list_add(node->sgprivate->interact->routes, r);
	} else {
		switch (field.eventType) {
		case GF_SG_EVENT_FIELD:
		case GF_SG_EVENT_EXPOSED_FIELD:
		case GF_SG_EVENT_IN:
			r->FromField.fieldIndex = protoFieldIndex;
			r->FromNode = NULL;
			r->ToField.fieldIndex = nodeFieldIndex;
			r->ToNode = node;
			/*create an ISed route for the eventOut part of the exposedFIeld*/
			if ((field.eventType==GF_SG_EVENT_EXPOSED_FIELD) && (nodeField.eventType==GF_SG_EVENT_EXPOSED_FIELD)) {
				GF_Route *r2;
				GF_SAFEALLOC(r2, GF_Route);
				if (!r2) {
					gf_free(r);
					return GF_OUT_OF_MEM;
				}
				r2->IS_route = 1;
				r2->FromField.fieldIndex = nodeFieldIndex;
				r2->FromNode = node;
				r2->ToField.fieldIndex = protoFieldIndex;
				r2->ToNode = NULL;
				r2->graph =  proto->sub_graph;
				if (!node->sgprivate->interact) {
					GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
					if (!node->sgprivate->interact) return GF_OUT_OF_MEM;
				}
				if (!node->sgprivate->interact->routes) node->sgprivate->interact->routes = gf_list_new();
				gf_list_add(node->sgprivate->interact->routes, r2);
				gf_list_add(proto->sub_graph->Routes, r2);
			}
			break;
		case GF_SG_EVENT_OUT:
			r->FromField.fieldIndex = nodeFieldIndex;
			r->FromNode = node;
			r->ToField.fieldIndex = protoFieldIndex;
			r->ToNode = NULL;
			if (!node->sgprivate->interact) {
				GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
				if (!node->sgprivate->interact) return GF_OUT_OF_MEM;
			}
			if (!node->sgprivate->interact->routes) node->sgprivate->interact->routes = gf_list_new();
			break;
		default:
			gf_free(r);
			return GF_BAD_PARAM;
		}
	}
	r->graph = proto->sub_graph;
	return gf_list_add(proto->sub_graph->Routes, r);
}

GF_EXPORT
GF_Err gf_sg_proto_instance_set_ised(GF_Node *protoinst, u32 protoFieldIndex, GF_Node *node, u32 nodeFieldIndex)
{
	GF_Err e;
	GF_Route *r;
	GF_FieldInfo field, nodeField;
	if (!protoinst) return GF_BAD_PARAM;
	if (protoinst->sgprivate->tag != TAG_ProtoNode) return GF_BAD_PARAM;

	e = gf_node_get_field(protoinst, protoFieldIndex, &field);
	if (e) return e;
	e = gf_node_get_field(node, nodeFieldIndex, &nodeField);
	if (e) return e;
	if (field.fieldType != nodeField.fieldType) {
		if ((gf_sg_vrml_get_sf_type(field.fieldType)==GF_SG_VRML_SFSTRING) && (gf_sg_vrml_get_sf_type(nodeField.fieldType) == GF_SG_VRML_SFURL)) {
//			e = GF_OK;
		} else if ((gf_sg_vrml_get_sf_type(field.fieldType)==GF_SG_VRML_SFURL) && (gf_sg_vrml_get_sf_type(nodeField.fieldType) == GF_SG_VRML_SFSTRING)) {
//			e = GF_OK;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRML] error in IS - node field %s.%s - inType %s - outType %s\n", gf_node_get_class_name(node) , nodeField.name, gf_sg_vrml_get_field_type_name(field.fieldType), gf_sg_vrml_get_field_type_name(nodeField.fieldType)));
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
		if (!node->sgprivate->interact) {
			GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
			if (!node->sgprivate->interact) return GF_OUT_OF_MEM;
		}
		if (!node->sgprivate->interact->routes) node->sgprivate->interact->routes = gf_list_new();
		gf_list_add(node->sgprivate->interact->routes, r);
	} else {
		switch (field.eventType) {
		case GF_SG_EVENT_FIELD:
		case GF_SG_EVENT_EXPOSED_FIELD:
		case GF_SG_EVENT_IN:
			r->FromField.fieldIndex = protoFieldIndex;
			r->FromNode = protoinst;
			r->ToField.fieldIndex = nodeFieldIndex;
			r->ToNode = node;

			/*create an ISed route for the eventOut part of the exposedFIeld*/
			if ((field.eventType==GF_SG_EVENT_EXPOSED_FIELD) && (nodeField.eventType==GF_SG_EVENT_EXPOSED_FIELD)) {
				GF_Route *r2;
				GF_SAFEALLOC(r2, GF_Route);
				if (!r2) {
					gf_free(r);
					return GF_OUT_OF_MEM;
				}
				r2->IS_route = 1;
				r2->FromField.fieldIndex = nodeFieldIndex;
				r2->FromNode = node;
				r2->ToField.fieldIndex = protoFieldIndex;
				r2->ToNode = protoinst;
				r2->graph =  node->sgprivate->scenegraph;
				if (!node->sgprivate->interact) {
					GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
					if (!node->sgprivate->interact) return GF_OUT_OF_MEM;
				}
				if (!node->sgprivate->interact->routes) node->sgprivate->interact->routes = gf_list_new();
				gf_list_add(node->sgprivate->interact->routes, r2);
				gf_list_add(r->graph->Routes, r2);
			}
			break;
		case GF_SG_EVENT_OUT:
			r->FromField.fieldIndex = nodeFieldIndex;
			r->FromNode = node;
			r->ToField.fieldIndex = protoFieldIndex;
			r->ToNode = protoinst;
			if (!node->sgprivate->interact) {
				GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
				if (!node->sgprivate->interact) return GF_OUT_OF_MEM;
			}
			if (!node->sgprivate->interact->routes) node->sgprivate->interact->routes = gf_list_new();
			gf_list_add(node->sgprivate->interact->routes, r);
			break;
		default:
			gf_free(r);
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

GF_EXPORT
u32 gf_sg_proto_get_field_count(GF_Proto *proto)
{
	if (!proto) return 0;
	return gf_list_count(proto->proto_fields);
}

GF_EXPORT
GF_ProtoFieldInterface *gf_sg_proto_field_find(GF_Proto *proto, u32 fieldIndex)
{
	if (!proto) return NULL;
	return (GF_ProtoFieldInterface*)gf_list_get(proto->proto_fields, fieldIndex);
}

void gf_sg_proto_propagate_event(GF_Node *node, u32 fieldIndex, GF_Node *from_node)
{
	u32 i;
	GF_Route *r;
	if (!node) return;
	/*propagation only for proto*/
	if (node->sgprivate->tag != TAG_ProtoNode) return;
	/*with ISed fields*/
	if (!node->sgprivate->interact || !node->sgprivate->interact->routes) return;
	/*we only need to propagate ISed for eventIn/exposedField. This means that if the event comes from
	the same scene graph as the proto (eg from the proto code) we don't propagate the event*/
	if (from_node->sgprivate->scenegraph == node->sgprivate->scenegraph) return;

	/*for all ISed routes*/
	i=0;
	while ((r = (GF_Route*)gf_list_enum(node->sgprivate->interact->routes, &i))) {
		if (!r->IS_route) continue;
		/*connecting from this node && field to a destination node other than the event source (this will break loops due to exposedFields)*/
		if ((r->FromNode == node) && (r->FromField.fieldIndex == fieldIndex) && (r->ToNode != from_node) ) {
			if (gf_sg_route_activate(r))
				gf_node_changed(r->ToNode, &r->ToField);
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
GF_EXPORT
u32 gf_sg_proto_get_id(GF_Proto *proto)
{
	return proto ? proto->ID : 0;
}

GF_EXPORT
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

GF_EXPORT
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
		if (r->ToNode->sgprivate->tag == TAG_ProtoNode) {
			if (r->ToNode==node) continue;
			return gf_sg_proto_field_is_sftime_offset(r->ToNode, &inf);
		}
		/*IS to a startTime/stopTime field*/
		if (!stricmp(inf.name, "startTime") || !stricmp(inf.name, "stopTime")) return 1;
	}
	return 0;
}

GF_EXPORT
GF_Err gf_node_proto_set_grouping(GF_Node *node)
{
	if (!node || (node->sgprivate->tag!=TAG_ProtoNode)) return GF_BAD_PARAM;
	((GF_ProtoInstance *)node)->flags |= GF_SG_PROTO_IS_GROUPING;
	return GF_OK;
}

GF_EXPORT
Bool gf_node_proto_is_grouping(GF_Node *node)
{
	if (!node || (node->sgprivate->tag!=TAG_ProtoNode)) return 0;
	if ( ((GF_ProtoInstance *)node)->flags & GF_SG_PROTO_IS_GROUPING) return 1;
	return 0;
}

GF_EXPORT
GF_Node *gf_node_get_proto_root(GF_Node *node)
{
	if (!node || (node->sgprivate->tag!=TAG_ProtoNode)) return NULL;
	return ((GF_ProtoInstance *)node)->RenderingNode;
}

#if 0 //unused
GF_Node *gf_node_get_proto_parent(GF_Node *node)
{
	if (!node) return NULL;
	if (node->sgprivate->scenegraph->pOwningProto) {
		GF_Node *the_node = (GF_Node *) node->sgprivate->scenegraph->pOwningProto;
		if (the_node != node) return the_node;
	}
	return NULL;
}

Bool gf_node_is_proto_root(GF_Node *node)
{
	if (!node) return 0;
	if (!node->sgprivate->scenegraph->pOwningProto) return 0;

	if (gf_list_find(node->sgprivate->scenegraph->pOwningProto->node_code, node)>=0) return 1;
	return 0;
}
#endif


GF_EXPORT
GF_Err gf_node_set_proto_eventin_handler(GF_Node *node, u32 fieldIndex, void (*event_in_cbk)(GF_Node *pThis, struct _route *route) )
{
	GF_ProtoInstance *inst;
	GF_ProtoField *field;
	if (!node || (node->sgprivate->tag!=TAG_ProtoNode)) return GF_BAD_PARAM;

	inst = (GF_ProtoInstance *) node;
	field = (GF_ProtoField*)gf_list_get(inst->fields, fieldIndex);
	if (!field) return GF_BAD_PARAM;

	if (field->EventType!=GF_SG_EVENT_IN) return GF_BAD_PARAM;
	field->on_event_in = event_in_cbk;
	return GF_OK;
}



#endif	/*GPAC_DISABLE_VRML*/
