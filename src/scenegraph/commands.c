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
/*MPEG4 tags (for internal nodes)*/
#include <gpac/nodes_mpeg4.h>

#include <gpac/internal/laser_dev.h>


GF_EXPORT
GF_Command *gf_sg_command_new(GF_SceneGraph *graph, u32 tag)
{
	GF_Command *ptr;
	GF_SAFEALLOC(ptr, GF_Command);
	if (!ptr) return NULL;
	ptr->tag = tag;
	ptr->in_scene = graph;
	ptr->command_fields = gf_list_new();
	if (tag < GF_SG_LAST_BIFS_COMMAND) ptr->new_proto_list = gf_list_new();
	return ptr;
}

GF_EXPORT
void gf_sg_command_del(GF_Command *com)
{
#ifndef GPAC_DISABLE_VRML
	u32 i;
	GF_Proto *proto;
#endif
	if (!com) return;

	if (com->tag < GF_SG_LAST_BIFS_COMMAND) {
#ifndef GPAC_DISABLE_VRML
		while (gf_list_count(com->command_fields)) {
			GF_CommandField *inf = (GF_CommandField *)gf_list_get(com->command_fields, 0);
			gf_list_rem(com->command_fields, 0);

			switch (inf->fieldType) {
			case GF_SG_VRML_SFNODE:
				if (inf->new_node) gf_node_try_destroy(com->in_scene, inf->new_node, NULL);
				break;
			case GF_SG_VRML_MFNODE:
				if (inf->field_ptr) {
					GF_ChildNodeItem *child;
					child = inf->node_list;
					while (child) {
						GF_ChildNodeItem *cur = child;
						gf_node_try_destroy(com->in_scene, child->node, NULL);
						child = child->next;
						gf_free(cur);
					}
				}
				break;
			default:
				if (inf->field_ptr) gf_sg_vrml_field_pointer_del(inf->field_ptr, inf->fieldType);
				break;
			}
			gf_free(inf);
		}
#endif
	} else {
#ifndef GPAC_DISABLE_SVG
		while (gf_list_count(com->command_fields)) {
			GF_CommandField *inf = (GF_CommandField *)gf_list_get(com->command_fields, 0);
			gf_list_rem(com->command_fields, 0);

			if (inf->new_node)
				gf_node_try_destroy(com->in_scene, inf->new_node, NULL);
			else if (inf->node_list) {
				GF_ChildNodeItem *child;
				child = inf->node_list;
				while (child) {
					GF_ChildNodeItem *cur = child;
					gf_node_try_destroy(com->in_scene, child->node, NULL);
					child = child->next;
					gf_free(cur);
				}
			} else if (inf->field_ptr) {
				gf_svg_delete_attribute_value(inf->fieldType, inf->field_ptr, com->in_scene);
			}
			gf_free(inf);
		}
#endif
	}
	gf_list_del(com->command_fields);

#ifndef GPAC_DISABLE_VRML
	i=0;
	while ((proto = (GF_Proto*)gf_list_enum(com->new_proto_list, &i))) {
		gf_sg_proto_del(proto);
	}
	gf_list_del(com->new_proto_list);
#endif

	if (com->node) {
		gf_node_try_destroy(com->in_scene, com->node, NULL);
	}

	if (com->del_proto_list) gf_free(com->del_proto_list);
	if (com->def_name) gf_free(com->def_name);
	if (com->scripts_to_load) gf_list_del(com->scripts_to_load);
	if (com->unres_name) gf_free(com->unres_name);
	gf_free(com);
}

static void SG_CheckFieldChange(GF_Node *node, GF_FieldInfo *field)
{
	/*and propagate eventIn if any*/
	if (field->on_event_in) {
		field->on_event_in(node, NULL);
	}
#ifndef GPAC_DISABLE_VRML
	else if ((field->eventType==GF_SG_EVENT_IN) && (gf_node_get_tag(node) == TAG_MPEG4_Script)) {
		gf_sg_script_event_in(node, field);
	}
	else {
		/*Notify eventOut in all cases to handle protos*/
		gf_node_event_out(node, field->fieldIndex);
	}
#endif
	/*signal node modif*/
	gf_node_changed(node, field);
}

#ifndef GPAC_DISABLE_SVG
static void gf_node_unregister_children_deactivate(GF_Node *container, GF_ChildNodeItem *child)
{
	while (child) {
		GF_ChildNodeItem *cur;
		gf_node_unregister(child->node, container);
		gf_node_deactivate(child->node);
		cur = child;
		child = child->next;
		gf_free(cur);
	}
}
#endif


GF_EXPORT
GF_Err gf_sg_command_apply(GF_SceneGraph *graph, GF_Command *com, Double time_offset)
{
	GF_Err e;
#if !defined(GPAC_DISABLE_VRML) || !defined(GPAC_DISABLE_SVG)
	GF_CommandField *inf;
	GF_Node *node;
#endif

#ifndef GPAC_DISABLE_VRML
	GF_FieldInfo field;
	void *slot_ptr;
	GF_Node *def;
#endif

	if (!com || !graph) return GF_BAD_PARAM;

	if (com->never_apply) return GF_OK;

	e = GF_OK;
	switch (com->tag) {
#ifndef GPAC_DISABLE_VRML
	case GF_SG_SCENE_REPLACE:
		/*unregister root*/
		gf_node_unregister(graph->RootNode, NULL);
		/*remove all protos and routes*/
		while (gf_list_count(graph->routes_to_activate))
			gf_list_rem(graph->routes_to_activate, 0);

		if (!com->aggregated) {
			/*destroy all routes*/
			while (gf_list_count(graph->Routes)) {
				GF_Route *r = (GF_Route *)gf_list_get(graph->Routes, 0);
				/*this will unregister the route from the graph, so don't delete the chain entry*/
				gf_sg_route_del(r);
			}
			/*destroy all proto*/
			while (gf_list_count(graph->protos)) {
				GF_Proto *p = (GF_Proto*)gf_list_get(graph->protos, 0);
				/*this will unregister the proto from the graph, so don't delete the chain entry*/
				gf_sg_proto_del(p);
			}
		}
		/*DO NOT TOUCH node registry*/
		/*DO NOT TOUCH UNREGISTERED PROTOS*/

		/*if no protos (previously aggregated command) create proto list*/
		if (!graph->protos) graph->protos = gf_list_new();

		/*move all protos in graph*/
		while (gf_list_count(com->new_proto_list)) {
			GF_Proto *p = (GF_Proto*)gf_list_get(com->new_proto_list, 0);
			gf_list_rem(com->new_proto_list, 0);
			gf_list_del_item(graph->unregistered_protos, p);
			gf_list_add(graph->protos, p);
		}
		/*assign new root (no need to register/unregister)*/
		graph->RootNode = com->node;
		com->node = NULL;
		break;

	case GF_SG_NODE_REPLACE:
		if (!gf_list_count(com->command_fields)) return GF_OK;
		inf = (GF_CommandField*)gf_list_get(com->command_fields, 0);
		e = gf_node_replace(com->node, inf->new_node, 0);
		//TOCHECK - this is commented as registering shouldn't happen (already done at command creation) and creates mem leak
//		if (inf->new_node) gf_node_register(inf->new_node, NULL);
		break;

	case GF_SG_MULTIPLE_REPLACE:
	case GF_SG_FIELD_REPLACE:
	{
		u32 j;
		GF_ChildNodeItem *list, *cur, *prev;
		j=0;
		while ((inf = (GF_CommandField*)gf_list_enum(com->command_fields, &j))) {
			e = gf_node_get_field(com->node, inf->fieldIndex, &field);
			if (e) return e;

			switch (field.fieldType) {
			case GF_SG_VRML_SFNODE:
			{
				node = *((GF_Node **) field.far_ptr);
				e = gf_node_unregister(node, com->node);
				*((GF_Node **) field.far_ptr) = inf->new_node;
				if (!e) gf_node_register(inf->new_node, com->node);
				break;
			}
			case GF_SG_VRML_MFNODE:
				gf_node_unregister_children(com->node, * ((GF_ChildNodeItem **) field.far_ptr));
				* ((GF_ChildNodeItem **) field.far_ptr) = NULL;

				list = * ((GF_ChildNodeItem **) inf->field_ptr);
				prev=NULL;
				while (list) {
					cur = gf_malloc(sizeof(GF_ChildNodeItem));
					cur->next = NULL;
					cur->node = list->node;
					if (prev) {
						prev->next = cur;
					} else {
						* ((GF_ChildNodeItem **) field.far_ptr) = cur;
					}
					gf_node_register(list->node, com->node);
					prev = cur;
					list = list->next;
				}
				break;
			case GF_SG_VRML_SFCOMMANDBUFFER:
			{
				u32 i, count;
				GF_SceneGraph *sg;
				SFCommandBuffer *cb_dst = (SFCommandBuffer *)field.far_ptr;
				SFCommandBuffer *cb_src = (SFCommandBuffer *)inf->field_ptr;

				/*reset dest*/
				if (!cb_dst->commandList) cb_dst->commandList = gf_list_new();

				while (gf_list_count(cb_dst->commandList)) {
					GF_Command *sub_com = (GF_Command *)gf_list_get(cb_dst->commandList, 0);
					gf_sg_command_del(sub_com);
					gf_list_rem(cb_dst->commandList, 0);
				}
				if (cb_dst->buffer) {
					gf_free(cb_dst->buffer);
					cb_dst->buffer = NULL;
				}

				/*clone command list*/
				sg = gf_node_get_graph(com->node);
				count = gf_list_count(cb_src->commandList);
				for (i=0; i<count; i++) {
					GF_Command *sub_com = (GF_Command *)gf_list_get(cb_src->commandList, i);
					GF_Command *new_com = gf_sg_vrml_command_clone(sub_com, sg, 0);
					gf_list_add(cb_dst->commandList, new_com);
				}
			}
			break;

			default:
				/*this is a regular field, reset it and clone - we cannot switch pointers since the
				original fields are NOT pointers*/
				if (!gf_sg_vrml_is_sf_field(field.fieldType)) {
					e = gf_sg_vrml_mf_reset(field.far_ptr, field.fieldType);
				}
				if (e) return e;
				gf_sg_vrml_field_copy(field.far_ptr, inf->field_ptr, field.fieldType);

				if ((field.fieldType==GF_SG_VRML_SFTIME) && !strstr(field.name, "media"))
					*(SFTime *)field.far_ptr = *(SFTime *)field.far_ptr + time_offset;
				break;
			}
			SG_CheckFieldChange(com->node, &field);
		}
		break;
	}

	case GF_SG_MULTIPLE_INDEXED_REPLACE:
	case GF_SG_INDEXED_REPLACE:
	{
		u32 sftype, i=0;
		while ((inf = (GF_CommandField*)gf_list_enum(com->command_fields, &i))) {
			e = gf_node_get_field(com->node, inf->fieldIndex, &field);
			if (e) return e;

			/*if MFNode remove the child and set new node*/
			if (field.fieldType == GF_SG_VRML_MFNODE) {
				/*we must remove the node before in case the new node uses the same ID (not forbidden) and this
				command removes the last instance of the node with the same ID*/
				gf_node_replace_child(com->node, (GF_ChildNodeItem**) field.far_ptr, inf->pos, inf->new_node);
				if (inf->new_node) gf_node_register(inf->new_node, NULL);
			}
			/*erase the field item*/
			else {
				if ((inf->pos < 0) || ((u32) inf->pos >= ((GenMFField *) field.far_ptr)->count) ) {
					inf->pos = ((GenMFField *)field.far_ptr)->count - 1;
					/*may happen with text and default value*/
					if (inf->pos < 0) {
						inf->pos = 0;
						gf_sg_vrml_mf_alloc(field.far_ptr, field.fieldType, 1);
					}
				}
				e = gf_sg_vrml_mf_get_item(field.far_ptr, field.fieldType, & slot_ptr, inf->pos);
				if (e) return e;
				sftype = gf_sg_vrml_get_sf_type(field.fieldType);
				gf_sg_vrml_field_copy(slot_ptr, inf->field_ptr, sftype);
				/*note we don't add time offset, since there's no MFTime*/
			}
			SG_CheckFieldChange(com->node, &field);
		}
		break;
	}
	case GF_SG_ROUTE_REPLACE:
	{
		GF_Route *r;
		char *name;
		r = gf_sg_route_find(graph, com->RouteID);
		def = gf_sg_find_node(graph, com->fromNodeID);
		node = gf_sg_find_node(graph, com->toNodeID);
		if (!node || !def) return GF_SG_UNKNOWN_NODE;
		name = NULL;
		if (r) {
			name = r->name;
			r->name = NULL;
			gf_sg_route_del(r);
		}
		r = gf_sg_route_new(graph, def, com->fromFieldIndex, node, com->toFieldIndex);
		gf_sg_route_set_id(r, com->RouteID);
		if (name) {
			gf_sg_route_set_name(r, name);
			gf_free(name);
		}
		break;
	}
	case GF_SG_NODE_DELETE_EX:
	case GF_SG_NODE_DELETE:
	{
		if (com->node) gf_node_replace(com->node, NULL, (com->tag==GF_SG_NODE_DELETE_EX) ? 1 : 0);
		break;
	}
	case GF_SG_ROUTE_DELETE:
	{
		return gf_sg_route_del_by_id(graph, com->RouteID);
	}
	case GF_SG_INDEXED_DELETE:
	{
		if (!gf_list_count(com->command_fields)) return GF_OK;
		inf = (GF_CommandField*)gf_list_get(com->command_fields, 0);

		e = gf_node_get_field(com->node, inf->fieldIndex, &field);
		if (e) return e;
		if (gf_sg_vrml_is_sf_field(field.fieldType)) return GF_NON_COMPLIANT_BITSTREAM;

		/*then we need special handling in case of a node*/
		if (gf_sg_vrml_get_sf_type(field.fieldType) == GF_SG_VRML_SFNODE) {
			e = gf_node_replace_child(com->node, (GF_ChildNodeItem **) field.far_ptr, inf->pos, NULL);
		} else {
			if ((inf->pos < 0) || ((u32) inf->pos >= ((GenMFField *) field.far_ptr)->count) ) {
				inf->pos = ((GenMFField *)field.far_ptr)->count - 1;
			}
			/*this is a regular MFField, just remove the item (gf_realloc)*/
			e = gf_sg_vrml_mf_remove(field.far_ptr, field.fieldType, inf->pos);
		}
		/*deletion -> node has changed*/
		if (!e) SG_CheckFieldChange(com->node, &field);
		break;
	}
	case GF_SG_NODE_INSERT:
	{
		if (!gf_list_count(com->command_fields)) return GF_OK;
		inf = (GF_CommandField*)gf_list_get(com->command_fields, 0);

		e = gf_node_insert_child(com->node, inf->new_node, inf->pos);
		if (!e) e = gf_node_register(inf->new_node, com->node);
		if (!e) {
			gf_node_event_out(com->node, inf->fieldIndex);
			gf_node_changed(com->node, NULL);
		}
		break;
	}
	case GF_SG_ROUTE_INSERT:
	{
		GF_Route *r;
		def = gf_sg_find_node(graph, com->fromNodeID);
		node = gf_sg_find_node(graph, com->toNodeID);
		if (!node || !def) return GF_SG_UNKNOWN_NODE;
		r = gf_sg_route_new(graph, def, com->fromFieldIndex, node, com->toFieldIndex);
		if (com->RouteID) gf_sg_route_set_id(r, com->RouteID);
		if (com->def_name) {
			gf_sg_route_set_name(r, com->def_name);
			gf_free(com->def_name);
			com->def_name = NULL;
		}
		break;
	}
	case GF_SG_INDEXED_INSERT:
	{
		u32 sftype;
		if (!gf_list_count(com->command_fields)) return GF_OK;
		inf = (GF_CommandField*)gf_list_get(com->command_fields, 0);
		e = gf_node_get_field(com->node, inf->fieldIndex, &field);
		if (e) return e;

		/*rescale the MFField and parse the SFField*/
		if (field.fieldType != GF_SG_VRML_MFNODE) {
			if (inf->pos == -1) {
				e = gf_sg_vrml_mf_append(field.far_ptr, field.fieldType, & slot_ptr);
			} else {
				e = gf_sg_vrml_mf_insert(field.far_ptr, field.fieldType, & slot_ptr, inf->pos);
			}
			if (e) return e;
			sftype = gf_sg_vrml_get_sf_type(field.fieldType);
			gf_sg_vrml_field_copy(slot_ptr, inf->field_ptr, sftype);
		} else {
			if (inf->new_node) {
				if (inf->pos == -1) {
					gf_node_list_add_child( (GF_ChildNodeItem **) field.far_ptr, inf->new_node);
				} else {
					gf_node_list_insert_child((GF_ChildNodeItem **) field.far_ptr, inf->new_node, inf->pos);
				}
				gf_node_register(inf->new_node, com->node);
			}
		}
		if (!e) SG_CheckFieldChange(com->node, &field);
		break;
	}
	case GF_SG_PROTO_INSERT:
		/*destroy all proto*/
		while (gf_list_count(com->new_proto_list)) {
			GF_Proto *p = (GF_Proto*)gf_list_get(com->new_proto_list, 0);
			gf_list_rem(com->new_proto_list, 0);
			gf_list_del_item(graph->unregistered_protos, p);
			gf_list_add(graph->protos, p);
		}
		return GF_OK;
	case GF_SG_PROTO_DELETE:
	{
		u32 i;
		for (i=0; i<com->del_proto_list_size; i++) {
			/*note this will check for unregistered protos, but since IDs are unique we are sure we will
			not destroy an unregistered one*/
			GF_Proto *proto = gf_sg_find_proto(graph, com->del_proto_list[i], NULL);
			if (proto) gf_sg_proto_del(proto);
		}
	}
	return GF_OK;
	case GF_SG_PROTO_DELETE_ALL:
		/*destroy all proto*/
		while (gf_list_count(graph->protos)) {
			GF_Proto *p = (GF_Proto*)gf_list_get(graph->protos, 0);
			gf_list_rem(graph->protos, 0);
			/*this will unregister the proto from the graph, so don't delete the chain entry*/
			gf_sg_proto_del(p);
		}
		/*DO NOT TOUCH UNREGISTERED PROTOS*/
		return GF_OK;
	case GF_SG_XREPLACE:
	{
		s32 pos = -2;
		GF_Node *target = NULL;
		GF_ChildNodeItem *list, *cur;
		GF_FieldInfo value;
		inf = (GF_CommandField*)gf_list_get(com->command_fields, 0);
		if (!inf) return GF_SG_UNKNOWN_NODE;

		e = gf_node_get_field(com->node, inf->fieldIndex, &field);
		if (e) return e;
		target = com->node;

		if (!gf_sg_vrml_is_sf_field(field.fieldType)) {
			GF_FieldInfo idxField;
			if ((inf->pos != -2) || com->toNodeID) {
				if (com->toNodeID) {
					GF_Node *idxNode = gf_sg_find_node(graph, com->toNodeID);
					if (!idxNode) return GF_SG_UNKNOWN_NODE;

					if (gf_node_get_field(idxNode, com->toFieldIndex, &idxField) != GF_OK) return GF_OK;
					pos = 0;
					switch (idxField.fieldType) {
					case GF_SG_VRML_SFBOOL:
						if (*(SFBool*)idxField.far_ptr) pos = 1;
						break;
					case GF_SG_VRML_SFINT32:
						if (*(SFInt32*)idxField.far_ptr >=0) pos = *(SFInt32*)idxField.far_ptr;
						break;
					case GF_SG_VRML_SFFLOAT:
						if ( (*(SFFloat *)idxField.far_ptr) >=0) pos = (s32) floor( FIX2FLT(*(SFFloat*)idxField.far_ptr) );
						break;
					case GF_SG_VRML_SFTIME:
						if ( (*(SFTime *)idxField.far_ptr) >=0) pos = (s32) floor( (*(SFTime *)idxField.far_ptr) );
						break;
					}
				} else {
					pos = inf->pos;
				}
			}
		}
		/*override target node*/
		if ((field.fieldType==GF_SG_VRML_MFNODE) && (pos>=-1) && com->ChildNodeTag) {
			target = gf_node_list_get_child(*(GF_ChildNodeItem **)field.far_ptr, pos);
			if (!target) return GF_SG_UNKNOWN_NODE;
			if (gf_node_get_field(target, com->child_field, &field) != GF_OK) return GF_SG_UNKNOWN_NODE;
			pos=-2;
		}

		if (com->fromNodeID) {
			GF_Node *fromNode = gf_sg_find_node(graph, com->fromNodeID);
			if (!fromNode) return GF_SG_UNKNOWN_NODE;
			e = gf_node_get_field(fromNode, com->fromFieldIndex, &value);
			if (e) return e;
		} else {
			value.far_ptr = inf->field_ptr;
			value.fieldType = inf->fieldType;
		}
		/*indexed replacement*/
		if (pos>=-1) {
			/*if MFNode remove the child and set new node*/
			if (field.fieldType == GF_SG_VRML_MFNODE) {
				GF_Node *nn = *(GF_Node**)value.far_ptr;
				gf_node_replace_child(target, (GF_ChildNodeItem**) field.far_ptr, pos, nn);
				if (nn) gf_node_register(nn, NULL);
			}
			/*erase the field item*/
			else {
				u32 sftype;
				if ((pos < 0) || ((u32) pos >= ((GenMFField *) field.far_ptr)->count) ) {
					pos = ((GenMFField *)field.far_ptr)->count - 1;
					/*may happen with text and default value*/
					if (pos < 0) {
						pos = 0;
						gf_sg_vrml_mf_alloc(field.far_ptr, field.fieldType, 1);
					}
				}
				e = gf_sg_vrml_mf_get_item(field.far_ptr, field.fieldType, & slot_ptr, pos);
				if (e) return e;
				sftype = gf_sg_vrml_get_sf_type(field.fieldType);
				gf_sg_vrml_field_copy(slot_ptr, value.far_ptr, sftype);
				/*note we don't add time offset, since there's no MFTime*/
			}
		} else {
			GF_ChildNodeItem *prev;
			switch (field.fieldType) {
			case GF_SG_VRML_SFNODE:
			{
				node = *((GF_Node **) field.far_ptr);
				e = gf_node_unregister(node, target);
				*((GF_Node **) field.far_ptr) = *((GF_Node **) value.far_ptr) ;
				if (!e) gf_node_register(*(GF_Node **) value.far_ptr, target);
				break;
			}
			case GF_SG_VRML_MFNODE:
				gf_node_unregister_children(target, * ((GF_ChildNodeItem **) field.far_ptr));
				* ((GF_ChildNodeItem **) field.far_ptr) = NULL;

				list = * ((GF_ChildNodeItem **) value.far_ptr);
				prev=NULL;
				while (list) {
					cur = gf_malloc(sizeof(GF_ChildNodeItem));
					cur->next = NULL;
					cur->node = list->node;
					if (prev) {
						prev->next = cur;
					} else {
						* ((GF_ChildNodeItem **) field.far_ptr) = cur;
					}
					gf_node_register(list->node, target);
					prev = cur;
					list = list->next;
				}
				break;

			default:
				if (!gf_sg_vrml_is_sf_field(field.fieldType)) {
					e = gf_sg_vrml_mf_reset(field.far_ptr, field.fieldType);
				}
				if (e) return e;
				gf_sg_vrml_field_clone(field.far_ptr, value.far_ptr, field.fieldType, graph);

				if ((field.fieldType==GF_SG_VRML_SFTIME) && !strstr(field.name, "media"))
					*(SFTime *)field.far_ptr = *(SFTime *)field.far_ptr + time_offset;
				break;
			}
		}
		SG_CheckFieldChange(target, &field);
	}
	return GF_OK;
	/*only used by BIFS*/
	case GF_SG_GLOBAL_QUANTIZER:
		return GF_OK;

#endif /*GPAC_DISABLE_VRML*/


#ifndef GPAC_DISABLE_SVG
	/*laser commands*/
	case GF_SG_LSR_NEW_SCENE:
		/*DO NOT TOUCH node registry*/

		/*assign new root (no need to register/unregister)*/
		graph->RootNode = com->node;
		com->node = NULL;
		break;
	case GF_SG_LSR_DELETE:
		if (!com->node) return GF_NON_COMPLIANT_BITSTREAM;
		if (!gf_list_count(com->command_fields)) {
			gf_node_replace(com->node, NULL, 0);
			gf_node_deactivate(com->node);
			return GF_OK;
		}
		inf = (GF_CommandField*)gf_list_get(com->command_fields, 0);
		node = gf_node_list_get_child(((SVG_Element *)com->node)->children, inf->pos);
		if (node) {
			e = gf_node_replace_child(com->node, &((SVG_Element *)com->node)->children, inf->pos, NULL);
			gf_node_deactivate(node);
		}
		break;
	case GF_SG_LSR_INSERT:
		inf = (GF_CommandField*)gf_list_get(com->command_fields, 0);
		if (!com->node || !inf) return GF_NON_COMPLIANT_BITSTREAM;
		if (inf->new_node) {
			if (inf->pos<0)
				gf_node_list_add_child(& ((SVG_Element *)com->node)->children, inf->new_node);
			else
				gf_node_list_insert_child(& ((SVG_Element *)com->node)->children, inf->new_node, inf->pos);

			gf_node_register(inf->new_node, com->node);
			gf_node_activate(inf->new_node);
			gf_node_changed(com->node, NULL);
		} else {
			/*NOT SUPPORTED*/
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[LASeR] VALUE INSERTION NOT SUPPORTED\n"));
		}
		break;
	case GF_SG_LSR_ADD:
	case GF_SG_LSR_REPLACE:
		inf = (GF_CommandField*)gf_list_get(com->command_fields, 0);
		if (!com->node || !inf) return GF_NON_COMPLIANT_BITSTREAM;
		if (inf->new_node) {
			if (inf->pos<0) {
				/*if fieldIndex (eg attributeName) is set, this is children replacement*/
				if (inf->fieldIndex>0) {
					gf_node_unregister_children_deactivate(com->node, ((SVG_Element *)com->node)->children);
					((SVG_Element *)com->node)->children = NULL;
					gf_node_list_add_child(& ((SVG_Element *)com->node)->children, inf->new_node);
					gf_node_register(inf->new_node, com->node);
					gf_node_activate(inf->new_node);
				} else {
					e = gf_node_replace(com->node, inf->new_node, 0);
					gf_node_activate(inf->new_node);
				}
			} else {
				node = gf_node_list_get_child( ((SVG_Element *)com->node)->children, inf->pos);
				gf_node_replace_child(com->node, & ((SVG_Element *)com->node)->children, inf->pos, inf->new_node);
				gf_node_register(inf->new_node, com->node);
				if (node) gf_node_deactivate(node);
				gf_node_activate(inf->new_node);
			}
			/*signal node modif*/
			gf_node_changed(com->node, NULL);
			return e;
		} else if (inf->node_list) {
			GF_ChildNodeItem *child, *cur, *prev;
			gf_node_unregister_children_deactivate(com->node, ((SVG_Element *)com->node)->children);
			((SVG_Element *)com->node)->children = NULL;

			prev = NULL;
			child = inf->node_list;
			while (child) {
				cur = (GF_ChildNodeItem*)gf_malloc(sizeof(GF_ChildNodeItem));
				cur->next = NULL;
				cur->node = child->node;
				gf_node_register(child->node, com->node);
				gf_node_activate(child->node);
				if (prev) prev->next = cur;
				else ((SVG_Element *)com->node)->children = cur;
				prev = cur;
				child = child->next;
			}
			/*signal node modif*/
			gf_node_changed(com->node, NULL);
			return GF_OK;
		}
		/*attribute modif*/
		else if (inf->field_ptr) {
			GF_FieldInfo a, b;
			if (inf->fieldIndex==(u32) -2) {
				GF_Point2D scale, translate;
				Fixed rotate;
				GF_Matrix2D *dest;
				gf_node_get_field_by_name(com->node, "transform", &a);
				dest = (GF_Matrix2D*)a.far_ptr;

				if (com->tag==GF_SG_LSR_REPLACE) {
					if (gf_mx2d_decompose(dest, &scale, &rotate, &translate)) {
						gf_mx2d_init(*dest);
						if (inf->fieldType==SVG_TRANSFORM_SCALE) scale = *(GF_Point2D *)inf->field_ptr;
						else if (inf->fieldType==SVG_TRANSFORM_TRANSLATE) translate = *(GF_Point2D *)inf->field_ptr;
						else if (inf->fieldType==SVG_TRANSFORM_ROTATE) rotate = ((SVG_Point_Angle*)inf->field_ptr)->angle;

						gf_mx2d_add_scale(dest, scale.x, scale.y);
						gf_mx2d_add_rotation(dest, 0, 0, rotate);
						gf_mx2d_add_translation(dest, translate.x, translate.y);
					}
				} else {
					GF_Point2D *pt = (GF_Point2D *)inf->field_ptr;
					if (inf->fieldType==SVG_TRANSFORM_SCALE) gf_mx2d_add_scale(dest, pt->x, pt->y);
					else if (inf->fieldType==SVG_TRANSFORM_TRANSLATE) gf_mx2d_add_translation(dest, pt->x, pt->y);
					else if (inf->fieldType == SVG_TRANSFORM_ROTATE) gf_mx2d_add_rotation(dest, 0, 0, ((SVG_Point_Angle*)inf->field_ptr)->angle);
				}
				/*signal node modif*/
				gf_node_changed(com->node, &a);
			} else {
				if ((inf->fieldIndex==(u32) -1) && (inf->fieldType==DOM_String_datatype)) {
					char *str = *(SVG_String*)inf->field_ptr;

					if (com->tag == GF_SG_LSR_REPLACE) {
						GF_DOMText *t = ((SVG_Element*)com->node)->children ? (GF_DOMText*) ((SVG_Element*)com->node)->children->node :NULL;
						if (t && (t->sgprivate->tag==TAG_DOMText)) {
							if (t->textContent) gf_free(t->textContent);
							t->textContent = NULL;
							if (str) t->textContent = gf_strdup(str);
						}
					} else {
						if (str) gf_dom_add_text_node(com->node, gf_strdup(str));
					}
					/*signal node modif*/
					gf_node_changed(com->node, NULL);
				}
				else if ((inf->fieldIndex==TAG_LSR_ATT_scale)
				         || (inf->fieldIndex==TAG_LSR_ATT_translation)
				         || (inf->fieldIndex==TAG_LSR_ATT_rotation)
				        ) {
					SVG_Transform *mx;
					gf_node_get_attribute_by_tag(com->node, TAG_SVG_ATT_transform, 1, 0, &a);
					mx = a.far_ptr;
					if (com->tag == GF_SG_LSR_REPLACE) {
						GF_Point2D scale, translate;
						SVG_Point_Angle rotate;
						if (gf_mx2d_decompose(&mx->mat, &scale, &rotate.angle, &translate)) {
							gf_mx2d_init(mx->mat);
							if (inf->fieldIndex==TAG_LSR_ATT_scale) scale = *(GF_Point2D *)inf->field_ptr;
							else if (inf->fieldIndex==TAG_LSR_ATT_translation) translate = *(GF_Point2D *)inf->field_ptr;
							else if (inf->fieldIndex==TAG_LSR_ATT_rotation) rotate = *(SVG_Point_Angle*)inf->field_ptr;

							gf_mx2d_add_scale(&mx->mat, scale.x, scale.y);
							gf_mx2d_add_rotation(&mx->mat, 0, 0, rotate.angle);
							gf_mx2d_add_translation(&mx->mat, translate.x, translate.y);
						}
					} else {
						if (inf->fieldIndex==TAG_LSR_ATT_scale) gf_mx2d_add_scale(&mx->mat, ((GF_Point2D*)inf->field_ptr)->x, ((GF_Point2D*)inf->field_ptr)->y);
						if (inf->fieldIndex==TAG_LSR_ATT_translation) gf_mx2d_add_translation(&mx->mat, ((GF_Point2D*)inf->field_ptr)->x, ((GF_Point2D*)inf->field_ptr)->y);
						if (inf->fieldIndex==TAG_LSR_ATT_rotation) gf_mx2d_add_rotation(&mx->mat, 0, 0, ((SVG_Point_Angle*)inf->field_ptr)->angle);
					}
					/*signal node modif*/
					gf_node_changed(com->node, &a);
				}
				else if (gf_node_get_attribute_by_tag(com->node, inf->fieldIndex, 1, 0, &a) == GF_OK) {
					b = a;
					b.far_ptr = inf->field_ptr;
					if (com->tag == GF_SG_LSR_REPLACE) {
						gf_svg_attributes_copy(&a, &b, 0);
					} else {
						gf_svg_attributes_add(&a, &b, &a, 0);
					}
					if (a.fieldType==XMLRI_datatype) {
						gf_node_dirty_set(com->node, GF_SG_SVG_XLINK_HREF_DIRTY, 0);
					}
					/*signal node modif*/
					gf_node_changed(com->node, &a);
				}
			}
		} else if (com->fromNodeID) {
			GF_FieldInfo a, b;
			GF_Node *fromNode = gf_sg_find_node(graph, com->fromNodeID);
			if (!fromNode) return GF_NON_COMPLIANT_BITSTREAM;
			if (gf_node_get_field(fromNode, com->fromFieldIndex, &b) != GF_OK) return GF_NON_COMPLIANT_BITSTREAM;

			if ((inf->fieldIndex==(u32) -1) && (inf->fieldType==DOM_String_datatype)) {
				char *str = inf->field_ptr ? *(SVG_String*)inf->field_ptr : NULL;

				if (com->tag == GF_SG_LSR_REPLACE) {
					GF_DOMText *t = ((SVG_Element*)com->node)->children ? (GF_DOMText*) ((SVG_Element*)com->node)->children->node :NULL;
					if (t && (t->sgprivate->tag==TAG_DOMText)) {
						if (t->textContent) gf_free(t->textContent);
						t->textContent = NULL;
						if (str) t->textContent = gf_strdup(str);
					}
				} else {
					if (str) gf_dom_add_text_node(com->node, gf_strdup(str));
				}
			} else {
				gf_node_get_field(com->node, inf->fieldIndex, &a);
				if (com->tag == GF_SG_LSR_REPLACE) {
					e = gf_svg_attributes_copy(&a, &b, 0);
				} else {
					e = gf_svg_attributes_add(&a, &b, &a, 0);
				}
			}
			gf_node_changed(com->node, &a);
			return e;
		} else {
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		break;
	case GF_SG_LSR_ACTIVATE:
		gf_node_activate(com->node);
		break;
	case GF_SG_LSR_DEACTIVATE:
		gf_node_deactivate(com->node);
		gf_node_changed(com->node, NULL);
		break;
	case GF_SG_LSR_SEND_EVENT:
	{
		GF_DOM_Event evt;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.type = com->send_event_name;
		evt.detail = com->send_event_integer;
		evt.clientX = com->send_event_x;
		evt.clientY = com->send_event_y;
		gf_dom_event_fire(com->node, &evt);
	}
	break;
#endif

	default:
		return GF_NOT_SUPPORTED;
	}
	if (e) return e;

#ifndef GPAC_DISABLE_VRML
	if (com->scripts_to_load) {
		while (gf_list_count(com->scripts_to_load)) {
			GF_Node *script = (GF_Node *)gf_list_get(com->scripts_to_load, 0);
			gf_list_rem(com->scripts_to_load, 0);
			gf_sg_script_load(script);
		}
		gf_list_del(com->scripts_to_load);
		com->scripts_to_load = NULL;
	}
#endif

	return GF_OK;
}

GF_EXPORT
GF_CommandField *gf_sg_command_field_new(GF_Command *com)
{
	GF_CommandField *ptr;
	GF_SAFEALLOC(ptr, GF_CommandField);
	if (ptr)
		gf_list_add(com->command_fields, ptr);
	return ptr;
}


GF_EXPORT
GF_Err gf_sg_command_apply_list(GF_SceneGraph *graph, GF_List *comList, Double time_offset)
{
	GF_Err e;
	GF_Command *com;
	u32 i=0;
	while ((com = (GF_Command *)gf_list_enum(comList, &i))) {
		e = gf_sg_command_apply(graph, com, time_offset);
		if (e) return e;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_VRML
GF_Command *gf_sg_vrml_command_clone(GF_Command *com, GF_SceneGraph *inGraph, Bool force_clone)
{
	u32 i, count;
	GF_Command *dest;

	/*FIXME - to do*/
	if (gf_list_count(com->new_proto_list)) return NULL;
	dest = gf_sg_command_new(inGraph, com->tag);

	if (com->in_scene!=inGraph) force_clone = 1;

	/*node the command applies to - may be NULL*/
	if (force_clone) {
		dest->node = gf_node_clone(inGraph, com->node, NULL, "", 0);
	} else {
		dest->node = com->node;
		gf_node_register(dest->node, NULL);
	}
	/*route insert, replace and delete*/
	dest->RouteID = com->RouteID;
	if (com->def_name) dest->def_name = gf_strdup(com->def_name);
	//this is an union
	//if (com->send_event_string) dest->send_event_string = gf_strdup(com->send_event_string);

	dest->fromNodeID = com->fromNodeID;
	dest->fromFieldIndex = com->fromFieldIndex;
	dest->toNodeID = com->toNodeID;
	dest->toFieldIndex = com->toFieldIndex;
	dest->send_event_integer = com->send_event_integer;
	dest->send_event_x = com->send_event_x;
	dest->send_event_y = com->send_event_y;

	dest->del_proto_list_size = com->del_proto_list_size;
	if (com->del_proto_list_size) {
		dest->del_proto_list = (u32*)gf_malloc(sizeof(u32) * com->del_proto_list_size);
		memcpy(dest->del_proto_list, com->del_proto_list, sizeof(u32) * com->del_proto_list_size);
	}
	count = gf_list_count(com->command_fields);
	for (i=0; i<count; i++) {
		GF_CommandField *fo = (GF_CommandField *)gf_list_get(com->command_fields, i);
		GF_CommandField *fd = (GF_CommandField *)gf_sg_command_field_new(dest);

		fd->fieldIndex = fo->fieldIndex;
		fd->fieldType = fo->fieldType;
		fd->pos = fo->pos;

		/*FIXME - this can also be LASeR commands, not supported for now*/
		if (fo->field_ptr) {
			fd->field_ptr = gf_sg_vrml_field_pointer_new(fd->fieldType);
			gf_sg_vrml_field_clone(fd->field_ptr, fo->field_ptr, fo->fieldType, dest->in_scene);
		}

		if (fo->new_node) {
			if (force_clone) {
				fd->new_node = gf_node_clone(inGraph, fo->new_node, dest->node, "", 0);
			} else {
				fd->new_node = fo->new_node;
				gf_node_register(fd->new_node, NULL);
			}
			fd->field_ptr = &fd->new_node;
		}
		if (fo->node_list) {
			GF_ChildNodeItem *child, *cur, *prev;
			prev = NULL;
			child = fo->node_list;
			while (child) {
				cur = (GF_ChildNodeItem*) gf_malloc(sizeof(GF_ChildNodeItem));
				if (force_clone) {
					cur->node = gf_node_clone(inGraph, child->node, dest->node, "", 0);
				} else {
					cur->node = child->node;
					gf_node_register(cur->node, NULL);
				}
				cur->next = NULL;
				if (prev) prev->next = cur;
				else fd->node_list = cur;
				prev = cur;
				child = child->next;
			}
			fd->field_ptr = &fd->node_list;
		}
	}
	return dest;
}

#endif

