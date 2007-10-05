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
/*svg proto*/
#include <gpac/scenegraph_svg.h>
/*MPEG4 tags (for internal nodes)*/
#include <gpac/nodes_mpeg4.h>
/*X3D tags (for internal nodes)*/
#include <gpac/nodes_x3d.h>

static void ReplaceDEFNode(GF_Node *FromNode, GF_Node *node, GF_Node *newNode, Bool updateOrderedGroup);

#ifndef GPAC_DISABLE_SVG
static void ReplaceIRINode(GF_Node *FromNode, GF_Node *oldNode, GF_Node *newNode);
#endif


GF_SceneGraph *gf_sg_new()
{
	GF_SceneGraph *tmp;
	GF_SAFEALLOC(tmp, GF_SceneGraph);
	if (!tmp) return NULL;

	tmp->protos = gf_list_new();
	tmp->unregistered_protos = gf_list_new();

	tmp->Routes = gf_list_new();
	tmp->routes_to_activate = gf_list_new();
	tmp->routes_to_destroy = gf_list_new();
#ifndef GPAC_DISABLE_SVG
	tmp->xlink_hrefs = gf_list_new();
	tmp->smil_timed_elements = gf_list_new();
#endif
#ifdef GPAC_HAS_SPIDERMONKEY
	tmp->scripts = gf_list_new();
	tmp->objects = gf_list_new();
	tmp->listeners_to_add = gf_list_new();
#endif
	return tmp;
}

GF_EXPORT
GF_SceneGraph *gf_sg_new_subscene(GF_SceneGraph *scene)
{
	GF_SceneGraph *tmp;
	if (!scene) return NULL;
	tmp = gf_sg_new();
	if (!tmp) return NULL;
	tmp->parent_scene = scene;
	tmp->script_action = scene->script_action;
	tmp->script_action_cbck = scene->script_action_cbck;
	tmp->script_load = scene->script_load;

	/*by default use the same callbacks*/
	tmp->userpriv = scene->userpriv;
	tmp->GetSceneTime = scene->GetSceneTime;
	tmp->GetExternProtoLib = scene->GetExternProtoLib;
	tmp->NodeCallback = scene->NodeCallback;
	return tmp;
}


GF_EXPORT
void gf_sg_set_node_callback(GF_SceneGraph *sg, void (*NodeCallback)(void *user_priv, u32 type, GF_Node *node, void *ctxdata) )
{
	sg->NodeCallback = NodeCallback;
}

GF_EXPORT
void gf_sg_set_scene_time_callback(GF_SceneGraph *sg, Double (*GetSceneTime)(void *user_priv))
{
	sg->GetSceneTime = GetSceneTime;
}


GF_EXPORT
Double gf_node_get_scene_time(GF_Node *node)
{
	if (!node || !node->sgprivate->scenegraph->GetSceneTime) return 0.0;
	return node->sgprivate->scenegraph->GetSceneTime(node->sgprivate->scenegraph->userpriv);
}


GF_EXPORT
void gf_sg_del(GF_SceneGraph *sg)
{	
	if (!sg) return;

	gf_sg_reset(sg);

#ifndef GPAC_DISABLE_SVG
	gf_list_del(sg->xlink_hrefs);
	gf_list_del(sg->smil_timed_elements);
	gf_list_del(sg->listeners_to_add);
#endif
#ifdef GPAC_HAS_SPIDERMONKEY
	gf_list_del(sg->scripts);
	gf_list_del(sg->objects);
#endif
	gf_list_del(sg->Routes);
	gf_list_del(sg->protos);
	gf_list_del(sg->unregistered_protos);
	gf_list_del(sg->routes_to_activate);
	gf_list_del(sg->routes_to_destroy);
	free(sg);
}

/*recursive traverse of the whole graph to check for scope mixes (nodes from an inline graph
inserted in a parent graph through bind or routes). We must do this otherwise we're certain to get random
crashes or mem leaks.*/
void SG_GraphRemoved(GF_Node *node, GF_SceneGraph *sg)
{
	u32 i, count;
	GF_FieldInfo info;
	u32 tag;

	tag = node->sgprivate->tag;
	count = gf_node_get_field_count(node);
#ifndef GPAC_DISABLE_SVG
	if (((tag>= GF_NODE_RANGE_FIRST_SVG) && (tag<= GF_NODE_RANGE_LAST_SVG)) 
#ifdef GPAC_ENABLE_SVG_SA
		|| ((tag>= GF_NODE_RANGE_FIRST_SVG_SA) && (tag<= GF_NODE_RANGE_LAST_SVG_SA)) 
#endif
#ifdef GPAC_ENABLE_SVG_SANI
		|| ((tag>= GF_NODE_RANGE_FIRST_SVG_SANI) && (tag<= GF_NODE_RANGE_LAST_SVG_SANI))
#endif
		) {
		/* TODO */
		tag = 0;
	} else 
#endif
	{
		for (i=0; i<count; i++) {
			gf_node_get_field(node, i, &info);
			if (info.fieldType==GF_SG_VRML_SFNODE) {
				GF_Node *n = *(GF_Node **) info.far_ptr;
				if (n) {
					if (n->sgprivate->scenegraph==sg) {
						/*if root of graph, skip*/
						if (sg->RootNode!=n) {
							gf_node_unregister(n, node);
							/*don't forget to remove node...*/
							*(GF_Node **) info.far_ptr = NULL;
						}
					} else {
						SG_GraphRemoved(n, sg);
					}
				}
			}
			else if (info.fieldType==GF_SG_VRML_MFNODE) {
				GF_ChildNodeItem *cur, *prev, *list = *(GF_ChildNodeItem **) info.far_ptr;
				prev = NULL;
				while (list) {
					if (list->node->sgprivate->scenegraph==sg) {
						gf_node_unregister(list->node, node);
					
						if (prev) prev->next = list->next;
						else *(GF_ChildNodeItem **) info.far_ptr = list->next;
						cur = list;
						free(cur);
					} else {
						SG_GraphRemoved(list->node, sg);
					}
					list = list->next;
				}
			}
		}
	}
}

GFINLINE GF_Node *SG_SearchForNode(GF_SceneGraph *sg, GF_Node *node)
{
	NodeIDedItem *reg_node = sg->id_node;
	while (reg_node) {
		if (reg_node->node == node) return reg_node->node;
		reg_node = reg_node->next;
	}
	return NULL;
}

static GFINLINE u32 get_num_id_nodes(GF_SceneGraph *sg)
{
	u32 count = 0;
	NodeIDedItem *reg_node = sg->id_node;
	while (reg_node) {
		count++;
		reg_node = reg_node->next;
	}
	return count;
}

GF_EXPORT
void gf_sg_reset(GF_SceneGraph *sg)
{
	u32 type, count;
	NodeIDedItem *reg_node;
	if (!sg) return;

	/*inlined graph, remove any of this graph nodes from the parent graph*/
	if (!sg->pOwningProto && sg->parent_scene) {
		GF_SceneGraph *par = sg->parent_scene;
		while (par->parent_scene) par = par->parent_scene;
		if (par->RootNode) SG_GraphRemoved(par->RootNode, sg);
	}

#ifdef GPAC_HAS_SPIDERMONKEY
	/*scripts are the first source of cylic references in the graph. In order to clean properly
	force a remove of all script nodes, this will release all references to nodes in JS*/
	while (gf_list_count(sg->scripts)) {
		GF_Node *n = gf_list_get(sg->scripts, 0);
		gf_list_rem(sg->scripts, 0);
		/*prevent destroy*/
		gf_node_register(n, NULL);
		/*remove from all parents*/
		gf_node_replace(n, NULL, 0);
		/*FORCE destroy in case the script refers to itself*/
		n->sgprivate->num_instances=1;
		gf_node_unregister(n, NULL);
	}
#endif

#ifndef GPAC_DISABLE_SVG
	/*flush any pending add_listener*/
	gf_dom_listener_process_add(sg);
#endif

	if (sg->RootNode) gf_node_unregister(sg->RootNode, NULL);
	sg->RootNode = NULL;

	while (gf_list_count(sg->routes_to_activate)) {
		gf_list_rem(sg->routes_to_activate, 0);
	}

	/*destroy all routes*/
	while (gf_list_count(sg->Routes)) {
		GF_Route *r = (GF_Route*)gf_list_get(sg->Routes, 0);
		/*this will unregister the route from the graph, so don't delete the chain entry*/
		gf_sg_route_del(r);

	}


	/*WATCHOUT: we may have cyclic dependencies due to
	1- a node referencing itself (forbidden in VRML)
	2- nodes refered to in commands of conditionals children of this node (MPEG-4 is mute about that)
	we recursively preocess from last declared DEF node to first one
	*/
restart:
	reg_node = sg->id_node;
	while (reg_node) {
		Bool ignore = 0;
		GF_Node *node = reg_node->node;
		if (!node) {
			reg_node = reg_node->next;
			continue;
		}

		/*first replace all instances in parents by NULL WITHOUT UNREGISTERING (to avoid destroying the node).
		This will take care of nodes referencing themselves*/
		{
		GF_ParentList *nlist = node->sgprivate->parents;
		type = node->sgprivate->tag;
#ifndef GPAC_DISABLE_SVG
		if (((type>= GF_NODE_RANGE_FIRST_SVG) && (type<= GF_NODE_RANGE_LAST_SVG)) 
#ifdef GPAC_ENABLE_SVG_SA
			|| ((type>= GF_NODE_RANGE_FIRST_SVG_SA) && (type<= GF_NODE_RANGE_LAST_SVG_SA)) 
#endif
#ifdef GPAC_ENABLE_SVG_SANI
			|| ((type>= GF_NODE_RANGE_FIRST_SVG_SANI) && (type<= GF_NODE_RANGE_LAST_SVG_SANI))
#endif
		) 
			type = 1;
		else 
#endif
			type = 0;

		while (nlist) {
			GF_ParentList *next = nlist->next;
#if 0
			/*parent is a DEF'ed node, try to clean-up properly?*/
			if ((nlist->node!=node) && SG_SearchForNode(sg, nlist->node) != NULL) {
				ignore = 1;
				break;
			}
#endif

#ifndef GPAC_DISABLE_SVG
			if (type) {
				ReplaceIRINode(nlist->node, node, NULL);
			} else 
#endif
				ReplaceDEFNode(nlist->node, reg_node->node, NULL, 0);
			
			free(nlist);
			nlist = next;
		}
		if (ignore) {
			node->sgprivate->parents = nlist;
			continue;
		}

		node->sgprivate->parents = NULL;
		}
		//sg->node_registry[i-1] = NULL;
		count = get_num_id_nodes(sg);
		node->sgprivate->num_instances = 1;
		gf_node_unregister(node, NULL);
		if (count != get_num_id_nodes(sg)) goto restart;
		reg_node = reg_node->next;
	}
	assert(sg->id_node==NULL);

	/*destroy all proto*/
	while (gf_list_count(sg->protos)) {
		GF_Proto *p = (GF_Proto *)gf_list_get(sg->protos, 0);
		/*this will unregister the proto from the graph, so don't delete the chain entry*/
		gf_sg_proto_del(p);
	}
	/*destroy all unregistered proto*/
	while (gf_list_count(sg->unregistered_protos)) {
		GF_Proto *p = (GF_Proto *)gf_list_get(sg->unregistered_protos, 0);
		/*this will unregister the proto from the graph, so don't delete the chain entry*/
		gf_sg_proto_del(p);
	}
#ifndef GPAC_DISABLE_SVG
	assert(gf_list_count(sg->xlink_hrefs) == 0);
#endif

	/*last destroy all routes*/
	gf_sg_destroy_routes(sg);
	sg->simulation_tick = 0;
#ifdef GF_SELF_REPLACE_ENABLE
	sg->graph_has_been_reset = 1;
#endif
}


GFINLINE GF_Node *SG_SearchForDuplicateNodeID(GF_SceneGraph *sg, u32 nodeID, GF_Node *toExclude)
{
	NodeIDedItem *reg_node = sg->id_node;
	while (reg_node) {
		if ((reg_node->node != toExclude) && (reg_node->NodeID == nodeID)) return reg_node->node;
		reg_node = reg_node->next;
	}
	return NULL;
}

void *gf_node_get_name_address(GF_Node*node)
{
	NodeIDedItem *reg_node;
	if (!(node->sgprivate->flags & GF_NODE_IS_DEF)) return NULL;
	reg_node = node->sgprivate->scenegraph->id_node;
	while (reg_node) {
		if (reg_node->node == node) return &reg_node->NodeName;
		reg_node = reg_node->next;
	}
	return NULL;
}

void gf_sg_set_private(GF_SceneGraph *sg, void *ptr)
{
	if (sg) sg->userpriv = ptr;
}

void *gf_sg_get_private(GF_SceneGraph *sg)
{
	return sg ? sg->userpriv : NULL;
}


GF_EXPORT
void gf_sg_set_scene_size_info(GF_SceneGraph *sg, u32 width, u32 height, Bool usePixelMetrics)
{
	if (!sg) return;
	if (width && height) {
		sg->width = width;
		sg->height = height;
	} else {
		sg->width = sg->height = 0;
	}
	sg->usePixelMetrics = usePixelMetrics;
}

GF_EXPORT
Bool gf_sg_use_pixel_metrics(GF_SceneGraph *sg)
{
	if (sg) {
		while (sg->pOwningProto) sg = sg->parent_scene;
		return sg->usePixelMetrics;
	}
	return 0;
}

GF_EXPORT
Bool gf_sg_get_scene_size_info(GF_SceneGraph *sg, u32 *width, u32 *height)
{
	if (!sg) return 0;
	*width = sg->width;
	*height = sg->height;
	return (sg->width && sg->height) ? 1 : 0;
}


GF_EXPORT
GF_Node *gf_sg_get_root_node(GF_SceneGraph *sg)
{
	return sg ? sg->RootNode : NULL;
}

GF_EXPORT
void gf_sg_set_root_node(GF_SceneGraph *sg, GF_Node *node)
{
	if (sg) sg->RootNode = node;
}

GFINLINE void remove_node_id(GF_SceneGraph *sg, GF_Node *node)
{
	NodeIDedItem *reg_node = sg->id_node;
	if (reg_node && (reg_node->node==node)) {
		sg->id_node = reg_node->next;
		if (sg->id_node_last==reg_node) 
			sg->id_node_last = reg_node->next;
		if (reg_node->NodeName) free(reg_node->NodeName);
		free(reg_node);
	} else {
		NodeIDedItem *to_del;
		while (reg_node->next) {
			if (reg_node->next->node!=node) {
				reg_node = reg_node->next;
				continue;
			}
			to_del = reg_node->next;
			reg_node->next = to_del->next;
			if (sg->id_node_last==to_del) {
				sg->id_node_last = reg_node->next ? reg_node->next : reg_node;
			}
			if (to_del->NodeName) free(to_del->NodeName);
			free(to_del);
			break;
		}
	}
}

GF_EXPORT
GF_Err gf_node_unregister(GF_Node *pNode, GF_Node *parentNode)
{
	u32 j;
	GF_SceneGraph *pSG;
	GF_Route *r;

	if (!pNode) return GF_OK;
	pSG = pNode->sgprivate->scenegraph;
	/*if this is a proto its is registered in its parent graph, not the current*/
	if (pSG && (pNode == (GF_Node*)pSG->pOwningProto)) pSG = pSG->parent_scene;

	if (parentNode) {
		GF_ParentList *nlist = pNode->sgprivate->parents;
		if (nlist) {
			GF_ParentList *prev = NULL;
			while (nlist) {
				if (nlist->node != parentNode) {
					prev = nlist;
					nlist = nlist->next;
					continue;
				}
				if (prev) prev->next = nlist->next;
				else pNode->sgprivate->parents = nlist->next;
				free(nlist);
				break;
			}
		}
	}

	/*unregister the instance*/
	assert(pNode->sgprivate->num_instances);
	pNode->sgprivate->num_instances -= 1;
	
	/*this is just an instance removed*/
	if (pNode->sgprivate->num_instances) {
		return GF_OK;
	}

	
	assert(pNode->sgprivate->parents==NULL);

	if (pSG) {
		/*if def, remove from sg def table*/
		if (pNode->sgprivate->flags & GF_NODE_IS_DEF) {
			remove_node_id(pSG, pNode);
		}

		/*check all routes from or to this node and destroy them - cf spec*/
		j=0;
		while ((r = (GF_Route *)gf_list_enum(pSG->Routes, &j))) {
			if ( (r->ToNode == pNode) || (r->FromNode == pNode)) {
				gf_sg_route_del(r);
				j--;
			}
		}
	}
	if (pNode->sgprivate->scenegraph && (pNode->sgprivate->scenegraph->RootNode==pNode))
		pNode->sgprivate->scenegraph->RootNode = NULL;

	/*delete the node*/
	gf_node_del(pNode);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_node_register(GF_Node *node, GF_Node *parentNode)
{
	GF_SceneGraph *pSG; 
	if (!node) return GF_OK;
	
	pSG = node->sgprivate->scenegraph;
	/*if this is a proto register to the parent graph, not the current*/
	if (pSG && (node == (GF_Node*)pSG->pOwningProto)) pSG = pSG->parent_scene;

	node->sgprivate->num_instances ++;
	/*parent may be NULL (top node and proto)*/
	if (parentNode) {
		if (!node->sgprivate->parents) {
			node->sgprivate->parents = (GF_ParentList*)malloc(sizeof(GF_ParentList));
			node->sgprivate->parents->next = NULL;
			node->sgprivate->parents->node = parentNode;
		} else {
			GF_ParentList *item, *nlist = node->sgprivate->parents;
			while (nlist->next) nlist = nlist->next;
			item = (GF_ParentList*)malloc(sizeof(GF_ParentList));
			item->next = NULL;
			item->node = parentNode;
			nlist->next = item;
		}
	}
	return GF_OK;
}

/*replace or remove node instance in the given node (eg in all GF_Node or MFNode fields)
this doesn't propagate in the scene graph. If updateOrderedGroup and new_node is NULL, the order field of OG
is updated*/
static void ReplaceDEFNode(GF_Node *FromNode, GF_Node *node, GF_Node *newNode, Bool updateOrderedGroup)
{
	u32 i, j;
	GF_Node *p;
	GF_ChildNodeItem *list;

	GF_FieldInfo field;

	/*browse all fields*/
	for (i=0; i<gf_node_get_field_count(FromNode); i++) {
		gf_node_get_field(FromNode, i, &field);
		switch (field.fieldType) {
		case GF_SG_VRML_SFNODE:
			/*set to NULL for SFFields*/
			p = *((GF_Node **) field.far_ptr);
			/*this is a USE / DEF*/
			if (p == node) {
				*((GF_Node **) field.far_ptr) = NULL;
				if (newNode) {
					*((GF_Node **) field.far_ptr) = newNode;
				}
				goto exit;
			}
			break;
		case GF_SG_VRML_MFNODE:
			list = *(GF_ChildNodeItem **) field.far_ptr;
			j=0;
			while (list) {
				/*replace nodes different from newNode but with same ID*/
				if ((newNode == list->node) || (list->node != node)) {
					list = list->next;
					j++;
					continue;
				}
				if (newNode) {
					list->node = newNode;
				} else {
					gf_node_list_del_child( (GF_ChildNodeItem **) field.far_ptr, list->node);
					if (updateOrderedGroup && (FromNode->sgprivate->tag==TAG_MPEG4_OrderedGroup)) {
						M_OrderedGroup *og = (M_OrderedGroup *)FromNode;
						gf_sg_vrml_mf_remove(&og->order, GF_SG_VRML_SFINT32, j);
					}
				}
				goto exit;
			}
			break;
			/*not a node, continue*/
		default:
			continue;
		}
	}
	/*since we don't filter parent nodes this is called once per USE, not per container, so return if found*/
exit:
	gf_node_changed(FromNode, &field);
}

#ifndef GPAC_DISABLE_SVG

static void Replace_IRI(GF_SceneGraph *sg, GF_Node *old_node, GF_Node *newNode)
{
	u32 i, count;
	count = gf_list_count(sg->xlink_hrefs);
	for (i=0; i<count; i++) {
		XMLRI *iri = (XMLRI *)gf_list_get(sg->xlink_hrefs, i);
		if (iri->target == old_node) {
			iri->target = newNode;
			if (!newNode) {
				gf_list_rem(sg->xlink_hrefs, i);
				i--;
				count--;
			}
		} 
	}
}

/*replace or remove node instance in the given node (eg in all IRI)*/
static void ReplaceIRINode(GF_Node *FromNode, GF_Node *old_node, GF_Node *newNode)
{
	GF_ChildNodeItem *prev = NULL;
	GF_ChildNodeItem *child = ((SVG_Element *)FromNode)->children;
	while (child) {
		if (child->node != old_node) {
			prev = child;
			child = child->next;
			continue;
		}
		if (newNode) {
			child->node = newNode;
		} else {
			if (prev) prev->next = child->next;
			else ((SVG_Element *)FromNode)->children = child->next;
			free(child);
		}
		break;
	}
}
#endif

/*get all parents of the node and replace, the instance of the node and finally destroy the node*/
GF_Err gf_node_replace(GF_Node *node, GF_Node *new_node, Bool updateOrderedGroup)
{
	u32 type;
	Bool replace_root;
	GF_Node *par;
	GF_SceneGraph *pSG = node->sgprivate->scenegraph;

	/*if this is a proto its is registered in its parent graph, not the current*/
	if (node == (GF_Node*)pSG->pOwningProto) pSG = pSG->parent_scene;
//	if (!SG_SearchForNodeIndex(pSG, node, &i)) return GF_BAD_PARAM;
//	assert(node == pSG->node_registry[i]);

	type = node->sgprivate->tag;
#ifndef GPAC_DISABLE_SVG
	if (((type>= GF_NODE_RANGE_FIRST_SVG) && (type<= GF_NODE_RANGE_LAST_SVG)) 
#ifdef GPAC_ENABLE_SVG_SA
		|| ((type>= GF_NODE_RANGE_FIRST_SVG_SA) && (type<= GF_NODE_RANGE_LAST_SVG_SA)) 
#endif
#ifdef GPAC_ENABLE_SVG_SANI
		|| ((type>= GF_NODE_RANGE_FIRST_SVG_SANI) && (type<= GF_NODE_RANGE_LAST_SVG_SANI))
#endif
		) {
		type = 1;
		Replace_IRI(pSG, node, new_node);
	} else 
#endif
		type = 0;

	/*first check if this is the root node*/
	replace_root = (node->sgprivate->scenegraph->RootNode == node) ? 1 : 0;

	while (node->sgprivate->parents) {
		Bool do_break = node->sgprivate->parents->next ? 0 : 1;
		par = node->sgprivate->parents->node;

#ifndef GPAC_DISABLE_SVG
		if (type)
			ReplaceIRINode(par, node, new_node);
		else
#endif
			ReplaceDEFNode(par, node, new_node, updateOrderedGroup);

		if (new_node) gf_node_register(new_node, par);
		gf_node_unregister(node, par);
		gf_node_changed(par, NULL);
		if (do_break) break;
	}

	if (replace_root) {
		pSG = node->sgprivate->scenegraph;
		gf_node_unregister(node, NULL);
		pSG->RootNode = new_node;
	}
	return GF_OK;
}

static GFINLINE void insert_node_def(GF_SceneGraph *sg, GF_Node *def, u32 ID, const char *name)
{
	NodeIDedItem *reg_node, *cur;

	reg_node = (NodeIDedItem *) malloc(sizeof(NodeIDedItem));
	reg_node->node = def;
	reg_node->NodeID = ID;
	reg_node->NodeName = name ? strdup(name) : NULL;
	reg_node->next = NULL;

	if (!sg->id_node) {
		sg->id_node = reg_node;
		sg->id_node_last = sg->id_node;
	} else if (sg->id_node->NodeID>ID) {
		reg_node->next = sg->id_node;
		sg->id_node = reg_node;
	} else if (sg->id_node_last->NodeID < ID) {
		sg->id_node_last->next = reg_node;
		sg->id_node_last = reg_node;
	} else {
		cur = sg->id_node;
		while (cur->next) {
			if (cur->next->NodeID>ID) {
				reg_node->next = cur->next;
				cur->next = reg_node;
				return;
			}
			cur = cur->next;
		}
		cur->next = reg_node;
		sg->id_node_last = reg_node;
	}
}



GF_EXPORT
GF_Err gf_node_set_id(GF_Node *p, u32 ID, const char *name)
{
	GF_SceneGraph *pSG; 
	if (!ID || !p || !p->sgprivate->scenegraph) return GF_BAD_PARAM;

	pSG = p->sgprivate->scenegraph;
	/*if this is a proto register to the parent graph, not the current*/
	if (p == (GF_Node*)pSG->pOwningProto) pSG = pSG->parent_scene;

	/*new DEF ID*/
	if (!(p->sgprivate->flags & GF_NODE_IS_DEF) ) {
		p->sgprivate->flags |= GF_NODE_IS_DEF;
	} 
	/*reassigning ID, remove node def*/
	else {
		remove_node_id(pSG, p);
	}
	insert_node_def(pSG, p, ID, name);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_node_remove_id(GF_Node *p)
{
	GF_SceneGraph *pSG; 
	if (!p) return GF_BAD_PARAM;

	pSG = p->sgprivate->scenegraph;
	/*if this is a proto register to the parent graph, not the current*/
	if (p == (GF_Node*)pSG->pOwningProto) pSG = pSG->parent_scene;

	/*new DEF ID*/
	if (p->sgprivate->flags & GF_NODE_IS_DEF) {
		remove_node_id(pSG, p);
		p->sgprivate->flags &= ~GF_NODE_IS_DEF;
		return GF_OK;
	} 
	return GF_BAD_PARAM;
}

/*calls RenderNode on this node*/
GF_EXPORT
void gf_node_render(GF_Node *node, void *renderStack)
{
	if (!node || !node->sgprivate) return;

	if (node->sgprivate->flags & GF_NODE_IS_DEACTIVATED) return;

	if (node->sgprivate->UserCallback) { 
#ifdef GF_CYCLIC_RENDER_ON
		if (node->sgprivate->flags & GF_NODE_IN_RENDER) return;
		node->sgprivate->flags |= GF_NODE_IN_RENDER;
		assert(node->sgprivate->flags);
#endif
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCENE, ("[SceneGraph] Traversing node %s\n", gf_node_get_class_name(node) ));
		node->sgprivate->UserCallback(node, renderStack, 0);
#ifdef GF_CYCLIC_RENDER_ON
		node->sgprivate->flags &= ~GF_NODE_IN_RENDER;
#endif
		return;
	}
	if (node->sgprivate->tag != TAG_ProtoNode) return;

	/*proto only traverses its first child*/
	if (((GF_ProtoInstance *) node)->RenderingNode) {
		node = ((GF_ProtoInstance *) node)->RenderingNode;
	}
	/*if no rendering function is assigned this is a real proto (otherwise this is an hardcoded one)*/
	else if (!node->sgprivate->UserCallback) {
		/*if no rendering node, check if the proto is fully instanciated (externProto)*/
		GF_ProtoInstance *proto_inst = (GF_ProtoInstance *) node;
		gf_node_dirty_clear(node, 0);
		/*proto has been deleted or dummy proto (without node code)*/
		if (!proto_inst->proto_interface || proto_inst->is_loaded) return;
		/*try to load the code*/
		gf_sg_proto_instanciate(proto_inst);
		if (!proto_inst->RenderingNode) {
			gf_node_dirty_set(node, 0, 1);
			return;
		}
		node = proto_inst->RenderingNode;
		node->sgprivate->scenegraph->NodeCallback(node->sgprivate->scenegraph->userpriv, GF_SG_CALLBACK_MODIFIED, node, NULL);
	}
	if (node->sgprivate->UserCallback) {
#ifdef GF_CYCLIC_RENDER_ON
		if (node->sgprivate->flags & GF_NODE_IN_RENDER) return;
		node->sgprivate->flags |= GF_NODE_IN_RENDER;
#endif
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCENE, ("[SceneGraph] Traversing node %s\n", gf_node_get_class_name(node) ));
		node->sgprivate->UserCallback(node, renderStack, 0);
#ifdef GF_CYCLIC_RENDER_ON
		node->sgprivate->flags &= ~GF_NODE_IN_RENDER;
#endif
	}
}

GF_EXPORT
void gf_node_allow_cyclic_render(GF_Node *node)
{
#ifdef GF_CYCLIC_RENDER_ON
	if (node) node->sgprivate->flags &= ~GF_NODE_IN_RENDER;
#endif
}

/*blindly calls RenderNode on all nodes in the "children" list*/
GF_EXPORT
void gf_node_render_children(GF_Node *node, void *renderStack)
{
	GF_ChildNodeItem *child;

	assert(node);
	child = ((GF_ParentNode *)node)->children;
	while (child) {
		gf_node_render(child->node, renderStack);
		child = child->next;
	}
}


GF_EXPORT
GF_SceneGraph *gf_node_get_graph(GF_Node *node)
{
	return (node ? node->sgprivate->scenegraph : NULL);
}

GF_EXPORT
GF_Node *gf_sg_find_node(GF_SceneGraph *sg, u32 nodeID)
{
	NodeIDedItem *reg_node = sg->id_node;
	while (reg_node) {
		if (reg_node->NodeID == nodeID) return reg_node->node;
		reg_node = reg_node->next;
	}
	return NULL;
}

GF_EXPORT
GF_Node *gf_sg_find_node_by_name(GF_SceneGraph *sg, char *name)
{
	if (name) {
		NodeIDedItem *reg_node = sg->id_node;
		while (reg_node) {
			if (reg_node->NodeName && !strcmp(reg_node->NodeName, name)) return reg_node->node;
			reg_node = reg_node->next;
		}
	}
	return NULL;
}


GF_EXPORT
u32 gf_sg_get_next_available_node_id(GF_SceneGraph *sg) 
{
	u32 ID;
	NodeIDedItem *reg_node;
	if (!sg->id_node) return 1;
	reg_node = sg->id_node;
	ID = reg_node->NodeID;
	/*nodes are sorted*/
	while (reg_node->next) {
		if (ID+1<reg_node->next->NodeID) return ID+1;
		ID = reg_node->next->NodeID;
		reg_node = reg_node->next;
	}
	return ID+1;
}

u32 gf_sg_get_max_node_id(GF_SceneGraph *sg)
{
	NodeIDedItem *reg_node;
	if (!sg->id_node) return 0;
	if (sg->id_node_last) return sg->id_node_last->NodeID;
	reg_node = sg->id_node;
	while (reg_node->next) reg_node = reg_node->next;
	return reg_node->NodeID;
}

void gf_node_setup(GF_Node *p, u32 tag)
{
	GF_SAFEALLOC(p->sgprivate, NodePriv);
	p->sgprivate->tag = tag;
	p->sgprivate->flags = GF_SG_NODE_DIRTY;
}

GF_Node *gf_sg_new_base_node()
{
	GF_Node *newnode = (GF_Node *)malloc(sizeof(GF_Node));
	gf_node_setup(newnode, TAG_UndefinedNode);
	return newnode;
}
GF_EXPORT
u32 gf_node_get_tag(GF_Node*p)
{
	assert(p);
	return p->sgprivate->tag;
}
GF_EXPORT
u32 gf_node_get_id(GF_Node*p)
{
	NodeIDedItem *reg_node;
	GF_SceneGraph *sg; 
	assert(p);
	if (!(p->sgprivate->flags & GF_NODE_IS_DEF)) return 0;
	sg = p->sgprivate->scenegraph;
	/*if this is a proto, look in parent graph*/
	if (p == (GF_Node*)sg->pOwningProto) sg = sg->parent_scene;

	reg_node = sg->id_node;
	while (reg_node) {
		if (reg_node->node==p) return reg_node->NodeID;
		reg_node = reg_node->next;
	}
	return 0;
}

GF_EXPORT
const char *gf_node_get_name(GF_Node*p)
{
	GF_SceneGraph *sg; 
	NodeIDedItem *reg_node;
	assert(p);
	if (!(p->sgprivate->flags & GF_NODE_IS_DEF)) return NULL;

	sg = p->sgprivate->scenegraph;
	/*if this is a proto, look in parent graph*/
	if (p == (GF_Node*)sg->pOwningProto) sg = sg->parent_scene;

	reg_node = sg->id_node;
	while (reg_node) {
		if (reg_node->node==p) return reg_node->NodeName;
		reg_node = reg_node->next;
	}
	return NULL;
}

GF_EXPORT
const char *gf_node_get_name_and_id(GF_Node*p, u32 *id)
{
	GF_SceneGraph *sg; 
	NodeIDedItem *reg_node;
	assert(p);
	if (!(p->sgprivate->flags & GF_NODE_IS_DEF)) {
		*id = 0;
		return NULL;
	}

	sg = p->sgprivate->scenegraph;
	/*if this is a proto, look in parent graph*/
	if (p == (GF_Node*)sg->pOwningProto) sg = sg->parent_scene;

	reg_node = sg->id_node;
	while (reg_node) {
		if (reg_node->node==p) {
			*id = reg_node->NodeID;
			return reg_node->NodeName;
		}
		reg_node = reg_node->next;
	}
	*id = 0;
	return NULL;
}

GF_EXPORT
void *gf_node_get_private(GF_Node*p)
{
	assert(p);
	return p->sgprivate->UserPrivate;
}
GF_EXPORT
void gf_node_set_private(GF_Node*p, void *pr)
{
	assert(p);
	p->sgprivate->UserPrivate = pr;
}
GF_EXPORT
GF_Err gf_node_set_callback_function(GF_Node *p, void (*RenderNode)(GF_Node *node, void *render_stack, Bool is_destroy) )
{
	assert(p);
	p->sgprivate->UserCallback = RenderNode;
	return GF_OK;
}

void gf_sg_parent_setup(GF_Node *node)
{
	((GF_ParentNode *)node)->children = NULL;
	node->sgprivate->flags |= GF_SG_CHILD_DIRTY;
}

GF_EXPORT
void gf_node_unregister_children(GF_Node *container, GF_ChildNodeItem *child)
{
	GF_ChildNodeItem *cur;
	while (child) {
		gf_node_unregister(child->node, container);
		cur = child;
		child = child->next;
		free(cur);
	}
}

GF_EXPORT
GF_Err gf_node_list_insert_child(GF_ChildNodeItem **list, GF_Node *n, u32 pos)
{
	GF_ChildNodeItem *child, *cur, *prev;
	u32 cur_pos = 0;
	
	assert(pos != (u32) -1);

	child = *list;
	
	cur = (GF_ChildNodeItem*) malloc(sizeof(GF_ChildNodeItem));
	if (!cur) return GF_OUT_OF_MEM;
	cur->node = n;
	cur->next = NULL;
	prev = NULL;
	while (child) {
		if (pos==cur_pos) break;
		/*append*/
		if (!child->next) {
			child->next = cur;
			return GF_OK;
		}
		prev = child;
		child = child->next;
		cur_pos++;
	}
	cur->next = child;
	if (prev) prev->next = cur;
	else *list = cur;
	return GF_OK;
}

GF_EXPORT
GF_Node *gf_node_list_get_child(GF_ChildNodeItem *list, s32 pos)
{
	s32 cur_pos = 0;
	while (list) {
		if (pos==cur_pos) return list->node;
		if ((pos<0) && !list->next) return list->node;
		list = list->next;
		cur_pos++;
	}
	return NULL;
}

GF_EXPORT
s32 gf_node_list_find_child(GF_ChildNodeItem *list, GF_Node *n)
{
	s32 res = 0;
	while (list) {
		if (list->node==n) return res;
		list = list->next;
		res++;
	}
	return -1;
}

GF_EXPORT
GF_Err gf_node_list_add_child(GF_ChildNodeItem **list, GF_Node *n)
{
	GF_ChildNodeItem *child, *cur;

	child = *list;
	
	cur = (GF_ChildNodeItem*) malloc(sizeof(GF_ChildNodeItem));
	if (!cur) return GF_OUT_OF_MEM;
	cur->node = n;
	cur->next = NULL;
	if (child) {
		while (child->next) child = child->next;
		child->next = cur;
	} else {
		*list = cur;
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_node_list_add_child_last(GF_ChildNodeItem **list, GF_Node *n, GF_ChildNodeItem **last_child)
{
	GF_ChildNodeItem *child, *cur;

	child = *list;
	
	cur = (GF_ChildNodeItem*) malloc(sizeof(GF_ChildNodeItem));
	if (!cur) return GF_OUT_OF_MEM;
	cur->node = n;
	cur->next = NULL;
	if (child) {
		if (last_child && (*last_child) ) {
			while ((*last_child)->next) (*last_child) = (*last_child)->next;
			(*last_child)->next = cur;
			(*last_child) = (*last_child)->next;
		} else {
			while (child->next) child = child->next;
			child->next = cur;
			if (last_child) *last_child = child->next;
		}
	} else {
		*list = cur;
		if (last_child) 
			*last_child = *list;
	}
	return GF_OK;
}

GF_EXPORT
Bool gf_node_list_del_child(GF_ChildNodeItem **list, GF_Node *n)
{
	GF_ChildNodeItem *child, *cur;

	child = *list;
	if (!child) return 0;
	if (child->node==n) {
		*list = child->next;
		free(child);
		return 1;
	}
	
	while (child->next) {
		if (child->next->node!=n) {
			child = child->next;
			continue;
		}
		cur = child->next;
		child->next = cur->next;
		free(cur);
		return 1;
	}
	return 0;
}

GF_EXPORT
GF_Node *gf_node_list_del_child_idx(GF_ChildNodeItem **list, u32 pos)
{
	u32 cur_pos = 0;
	GF_Node *ret = NULL;
	GF_ChildNodeItem *child, *cur;

	child = *list;
	if (!child) return 0;
	if (!pos) {
		*list = child->next;
		ret = child->node;
		free(child);
		return ret;
	}
	
	while (child->next) {
		if (cur_pos+1 != pos) {
			child = child->next;
			cur_pos++;
			continue;
		}
		cur = child->next;
		child->next = cur->next;
		ret = cur->node;
		free(cur);
		return ret;
	}
	return NULL;
}

GF_EXPORT
u32 gf_node_list_get_count(GF_ChildNodeItem *list)
{
	u32 count = 0;
	while (list) {
		count++;
		list = list->next;
	}
	return count;
}

void gf_sg_parent_reset(GF_Node *node)
{
	gf_node_unregister_children(node, ((GF_ParentNode *)node)->children);
	((GF_ParentNode *)node)->children = NULL;
}


void gf_node_free(GF_Node *node)
{
	if (!node) return;

	if (node->sgprivate->UserCallback) node->sgprivate->UserCallback(node, NULL, 1);

	if (node->sgprivate->interact) {
		if (node->sgprivate->interact->events) {
			/*true for VRML-based graphs, not true for SVG yet*/
			//assert(gf_list_count(node->sgprivate->events)==0);
			gf_list_del(node->sgprivate->interact->events);
		}
		if (node->sgprivate->interact->animations) {
			gf_list_del(node->sgprivate->interact->animations);
		}
		free(node->sgprivate->interact);
	}
	assert(! node->sgprivate->parents);
	free(node->sgprivate);
	free(node);
}

GF_EXPORT
u32 gf_node_get_parent_count(GF_Node *node)
{
	u32 count = 0;
	GF_ParentList *nlist = node->sgprivate->parents;
	while (nlist) { count++; nlist = nlist->next; }
	return count;
}

GF_EXPORT
GF_Node *gf_node_get_parent(GF_Node *node, u32 idx)
{
	GF_ParentList *nlist = node->sgprivate->parents;
	if (!nlist) return NULL;
	while (idx) { nlist = nlist->next; idx--;}
	return nlist->node;
}

static GFINLINE void dirty_children(GF_Node *node, u32 val)
{
	u32 i, count;
	GF_FieldInfo info;
	if (!node) return;
	
	node->sgprivate->flags |= val;
	if (node->sgprivate->tag>=GF_NODE_RANGE_LAST_VRML) {
		GF_ChildNodeItem *child = ((GF_ParentNode*)node)->children;
		while (child) {
			dirty_children(child->node, val);
			child = child->next;
		}
	} else {
		count = gf_node_get_field_count(node);
		for (i=0; i<count; i++) {
			gf_node_get_field(node, i, &info);
			if (info.fieldType==GF_SG_VRML_SFNODE) dirty_children(*(GF_Node **)info.far_ptr, val);
			else if (info.fieldType==GF_SG_VRML_MFNODE) {
				GF_ChildNodeItem *list = *(GF_ChildNodeItem **) info.far_ptr;
				while (list) {
					dirty_children(list->node, val);
					list = list->next;
				}
			}
		}
	}
}
static void dirty_parents(GF_Node *node)
{
	Bool check_root = 1;
	GF_ParentList *nlist = node->sgprivate->parents;
	while (nlist) {
		GF_Node *p = nlist->node;
		if (! (p->sgprivate->flags & GF_SG_CHILD_DIRTY)) {
			p->sgprivate->flags |= GF_SG_CHILD_DIRTY;
			dirty_parents(p);
		}
		check_root = 0;
		nlist = nlist->next;
	}
	if (check_root && (node==node->sgprivate->scenegraph->RootNode) && node->sgprivate->scenegraph->NodeCallback) node->sgprivate->scenegraph->NodeCallback(node->sgprivate->scenegraph->userpriv, GF_SG_CALLBACK_GRAPH_DIRTY, NULL, NULL);
}

GF_EXPORT
void gf_node_dirty_set(GF_Node *node, u32 flags, Bool and_dirty_parents)
{
	if (!node) return;
	
	if (flags) node->sgprivate->flags |= (flags & (~GF_NODE_INTERNAL_FLAGS) );
	else node->sgprivate->flags |= GF_SG_NODE_DIRTY;

	if (and_dirty_parents) dirty_parents(node);
}

GF_EXPORT
void gf_node_dirty_parents(GF_Node *node)
{
	dirty_parents(node);
}

GF_EXPORT
void gf_node_dirty_clear(GF_Node *node, u32 flag_to_remove)
{
	if (!node) return;
	if (flag_to_remove) node->sgprivate->flags &= ~ (flag_to_remove & ~GF_NODE_INTERNAL_FLAGS);
	else node->sgprivate->flags &= GF_NODE_INTERNAL_FLAGS;
}

GF_EXPORT
u32 gf_node_dirty_get(GF_Node *node)
{
	if (node) return (node->sgprivate->flags & ~GF_NODE_INTERNAL_FLAGS);
	return 0;
}


GF_EXPORT
void gf_node_dirty_reset(GF_Node *node)
{
	if (!node) return;
	if (node->sgprivate->flags & ~GF_NODE_INTERNAL_FLAGS) {
		node->sgprivate->flags &= GF_NODE_INTERNAL_FLAGS;
		dirty_children(node, 0);
	}
}



GF_EXPORT
void gf_node_init(GF_Node *node)
{
	GF_SceneGraph *pSG = node->sgprivate->scenegraph;
	assert(pSG);
	/*no user-defined init, consider the scenegraph is only used for parsing/encoding/decoding*/
	if (!pSG->NodeCallback) return;

	/*internal nodes*/
	if (gf_sg_vrml_node_init(node)) return;
#ifndef GPAC_DISABLE_SVG
#ifdef GPAC_ENABLE_SVG_SA
	else if (gf_svg_sa_node_init(node)) return;
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if (gf_svg_sani_node_init(node)) return;
#endif
	else if (gf_svg_node_init(node)) return;
#endif
	/*user defined init*/
	else pSG->NodeCallback(pSG->userpriv, GF_SG_CALLBACK_INIT, node, NULL);
}


GF_EXPORT
void gf_node_changed(GF_Node *node, GF_FieldInfo *field)
{
	GF_SceneGraph *sg;
	if (!node) return;

	sg = node->sgprivate->scenegraph;
	assert(sg);

	/*internal nodes*/
	if (gf_sg_vrml_node_changed(node, field)) return;
#ifndef GPAC_DISABLE_SVG
#ifdef GPAC_ENABLE_SVG_SA
	else if (gf_svg_sa_node_changed(node, field)) return;
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if (gf_svg_sani_node_changed(node, field)) return;
#endif
	else if (gf_svg_node_changed(node, field)) return;
#endif

	/*force child dirty tag*/
	if (field && ((field->fieldType==GF_SG_VRML_SFNODE) || (field->fieldType==GF_SG_VRML_MFNODE))) node->sgprivate->flags |= GF_SG_CHILD_DIRTY;
	if (sg->NodeCallback) sg->NodeCallback(sg->userpriv, GF_SG_CALLBACK_MODIFIED, node, field);
}

void gf_node_del(GF_Node *node)
{
#ifdef GF_NODE_USE_POINTERS
	node->sgprivate->node_del(node);
#else

	if (node->sgprivate->tag==TAG_UndefinedNode) gf_node_free(node);
	else if (node->sgprivate->tag==TAG_DOMText) {
		GF_DOMText *t = (GF_DOMText *)node;
		if (t->textContent) free(t->textContent);
		gf_sg_parent_reset(node);
		gf_node_free(node);
	} 
	else if (node->sgprivate->tag==TAG_DOMUpdates) {
		u32 i, count;
		GF_DOMUpdates *up = (GF_DOMUpdates *)node;
		if (up->data) free(up->data);
		count = gf_list_count(up->updates);
		for (i=0; i<count; i++) {
			GF_Command *com = gf_list_get(up->updates, i);
			gf_sg_command_del(com);
		}
		gf_list_del(up->updates);
		gf_sg_parent_reset(node);
		gf_node_free(node);
	} 
	else if (node->sgprivate->tag == TAG_DOMFullNode) {
		GF_DOMFullNode *n = (GF_DOMFullNode *)node;
		while (n->attributes) {
			GF_DOMAttribute *att = n->attributes;
			n->attributes = att->next;
			if (att->tag==TAG_DOM_ATTRIBUTE_FULL) {
				GF_DOMFullAttribute *fa = (GF_DOMFullAttribute *)att;
				free(fa->data);
				free(fa->name);
			}
			free(att);
		}
		if (n->name) free(n->name);
		if (n->ns) free(n->ns);
		gf_sg_parent_reset(node);
		gf_node_free(node);
	}
	else if (node->sgprivate->tag == TAG_ProtoNode) gf_sg_proto_del_instance((GF_ProtoInstance *)node);
	else if (node->sgprivate->tag<=GF_NODE_RANGE_LAST_MPEG4) gf_sg_mpeg4_node_del(node);
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_X3D) gf_sg_x3d_node_del(node);
#ifndef GPAC_DISABLE_SVG
#ifdef GPAC_ENABLE_SVG_SA
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG_SA) gf_svg_sa_element_del((SVG_SA_Element *) node);
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG_SANI) gf_svg_sani_element_del((SVG_SANI_Element *) node);
#endif
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG) gf_svg_node_del(node);
#endif
	else gf_node_free(node);
#endif
}

GF_EXPORT
u32 gf_node_get_field_count(GF_Node *node)
{
	assert(node);
	if (node->sgprivate->tag <= TAG_UndefinedNode) return 0;
	/*for both MPEG4 & X3D*/
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_X3D) return gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_ALL);
#ifndef GPAC_DISABLE_SVG
#ifdef GPAC_ENABLE_SVG_SA
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG_SA) return gf_svg_sa_get_attribute_count(node);
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG_SANI) return gf_svg_sani_get_attribute_count(node);
#endif
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG) return gf_svg_get_attribute_count(node);
#endif
	return 0;
}



GF_EXPORT
const char *gf_node_get_class_name(GF_Node *node)
{
	assert(node && node->sgprivate->tag);
#ifdef GF_NODE_USE_POINTERS
	return node->sgprivate->name;
#else
	if (node->sgprivate->tag==TAG_UndefinedNode) return "UndefinedNode";
	else if (node->sgprivate->tag==TAG_ProtoNode) return ((GF_ProtoInstance*)node)->proto_name;
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_MPEG4) return gf_sg_mpeg4_node_get_class_name(node->sgprivate->tag);
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_X3D) return gf_sg_x3d_node_get_class_name(node->sgprivate->tag);
	else if (node->sgprivate->tag==TAG_DOMText) return "";
	else if (node->sgprivate->tag==TAG_DOMFullNode) return ((GF_DOMFullNode*)node)->name;
#ifndef GPAC_DISABLE_SVG
#ifdef GPAC_ENABLE_SVG_SA
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG_SA) return gf_svg_sa_get_element_name(node->sgprivate->tag);
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG_SANI) return gf_svg_sani_get_element_name(node->sgprivate->tag);
#endif
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG) return gf_svg_get_element_name(node->sgprivate->tag);
#endif
	else return "UnsupportedNode";
#endif
}

GF_EXPORT
GF_Node *gf_node_new(GF_SceneGraph *inScene, u32 tag)
{
	GF_Node *node;
//	if (!inScene) return NULL;
	/*cannot create proto this way*/
	if (tag==TAG_ProtoNode) return NULL;
	else if (tag==TAG_UndefinedNode) node = gf_sg_new_base_node();
	else if (tag <= GF_NODE_RANGE_LAST_MPEG4) node = gf_sg_mpeg4_node_new(tag);
	else if (tag <= GF_NODE_RANGE_LAST_X3D) node = gf_sg_x3d_node_new(tag);
	else if (tag == TAG_DOMText) {
		GF_DOMText *n;
		GF_SAFEALLOC(n, GF_DOMText);
		node = (GF_Node*)n;
		gf_node_setup(node, TAG_DOMText);
	}
	else if (tag == TAG_DOMFullNode) {
		GF_DOMFullNode*n;
		GF_SAFEALLOC(n, GF_DOMFullNode);
		node = (GF_Node*)n;
		gf_node_setup(node, TAG_DOMFullNode);
	}
#ifndef GPAC_DISABLE_SVG
#ifdef GPAC_ENABLE_SVG_SA
	else if (tag <= GF_NODE_RANGE_LAST_SVG_SA) node = (GF_Node *) gf_svg_sa_create_node(tag);
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if (tag <= GF_NODE_RANGE_LAST_SVG_SANI) node = (GF_Node *) gf_svg_sani_create_node(tag);
#endif
	else if (tag <= GF_NODE_RANGE_LAST_SVG) node = (GF_Node *) gf_svg_create_node(tag);
#endif
	else node = NULL;

	if (node) node->sgprivate->scenegraph = inScene;
	/*script is inited as soon as created since fields are dynamically added*/
	if ((tag==TAG_MPEG4_Script) || (tag==TAG_X3D_Script) ) gf_sg_script_init(node);
	return node;
}


GF_EXPORT
GF_Err gf_node_get_field(GF_Node *node, u32 FieldIndex, GF_FieldInfo *info)
{
	assert(node);
	assert(info);
	memset(info, 0, sizeof(GF_FieldInfo));
	info->fieldIndex = FieldIndex;

#ifdef GF_NODE_USE_POINTERS
	return node->sgprivate->get_field(node, info);
#else
	if (node->sgprivate->tag==TAG_UndefinedNode) return GF_BAD_PARAM;
	else if (node->sgprivate->tag == TAG_ProtoNode) return gf_sg_proto_get_field(NULL, node, info);
	else if ((node->sgprivate->tag == TAG_MPEG4_Script) || (node->sgprivate->tag == TAG_X3D_Script) )
		return gf_sg_script_get_field(node, info);
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_MPEG4) return gf_sg_mpeg4_node_get_field(node, info);
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_X3D) return gf_sg_x3d_node_get_field(node, info);
#ifndef GPAC_DISABLE_SVG
#ifdef GPAC_ENABLE_SVG_SA
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG_SA) return gf_svg_sa_get_attribute_info(node, info);
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG_SANI) return gf_svg_sani_get_attribute_info(node, info);
#endif
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG) return gf_svg_get_attribute_info(node, info);
#endif
#endif
	return GF_NOT_SUPPORTED;
}

u32 gf_node_get_num_instances(GF_Node *node)
{
	return node->sgprivate->num_instances;
}

static GF_Err gf_node_get_field_by_name_enum(GF_Node *node, char *name, GF_FieldInfo *field)
{
	u32 i, count;
	assert(node);
	count = gf_node_get_field_count(node);
	memset(field, 0, sizeof(GF_FieldInfo));
	for (i=0; i<count;i++) {
		gf_node_get_field(node, i, field);
		if (!strcmp(field->name, name)) return GF_OK;
	}
	return GF_BAD_PARAM;
}

GF_Err gf_node_get_field_by_name(GF_Node *node, char *name, GF_FieldInfo *field)
{
#ifdef GF_NODE_USE_POINTERS
	return gf_node_get_field_by_name_enum(node, name, field);
#else
	s32 res = -1;

	if (node->sgprivate->tag==TAG_UndefinedNode) return GF_BAD_PARAM;
	else if (node->sgprivate->tag == TAG_ProtoNode) {
		res = gf_sg_proto_get_field_index_by_name(NULL, node, name);
	}
	else if ((node->sgprivate->tag == TAG_MPEG4_Script) || (node->sgprivate->tag == TAG_X3D_Script) ) {
		return gf_node_get_field_by_name_enum(node, name, field);
	}
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_MPEG4) res = gf_sg_mpeg4_node_get_field_index_by_name(node, name);
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_X3D) res = gf_sg_x3d_node_get_field_index_by_name(node, name);
#ifndef GPAC_DISABLE_SVG
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG) return gf_svg_get_attribute_by_name(node, name, 1, 0, field);
#ifdef GPAC_ENABLE_SVG_SA
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG_SA) res = gf_svg_sa_get_attribute_index_by_name(node, name);
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG_SANI) return gf_node_get_field_by_name_enum(node, name, field);
#endif
#endif
	if (res==-1) return GF_BAD_PARAM;
	return gf_node_get_field(node, (u32) res, field);
#endif
}
