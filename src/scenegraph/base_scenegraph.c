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


#ifdef GPAC_USE_LASeR
#include "../LASeR/m4_laser_dev.h"
#endif


static void ReplaceDEFNode(GF_Node *FromNode, u32 NodeID, GF_Node *newNode, Bool updateOrderedGroup);


#define DEFAULT_MAX_CYCLIC_RENDER	30

GF_SceneGraph *gf_sg_new()
{
	GF_SceneGraph *tmp;
	SAFEALLOC(tmp, sizeof(GF_SceneGraph));
	if (!tmp) return NULL;

	tmp->protos = gf_list_new();
	tmp->unregistered_protos = gf_list_new();

	tmp->node_registry = malloc(sizeof(GF_Node *) * NODEREG_STEP_ALLOC);
	tmp->node_reg_alloc = NODEREG_STEP_ALLOC;

#ifdef GF_CYCLIC_RENDER_ON
	tmp->max_cyclic_render = DEFAULT_MAX_CYCLIC_RENDER;
#endif
	tmp->Routes = gf_list_new();
	tmp->routes_to_activate = gf_list_new();
	tmp->routes_to_destroy = gf_list_new();
	return tmp;
}

GF_SceneGraph *gf_sg_new_subscene(GF_SceneGraph *scene)
{
	GF_SceneGraph *tmp;
	if (!scene) return NULL;
	tmp = gf_sg_new();
	if (!tmp) return NULL;
	tmp->parent_scene = scene;
	tmp->userpriv = scene->userpriv;
	/*by default use the same scene time (protos need that) - user overrides it if needed (inlineScene)*/
	tmp->GetSceneTime = scene->GetSceneTime;
	tmp->SceneCallback = scene->SceneCallback;
	tmp->GetExternProtoLib = scene->GetExternProtoLib;
	tmp->js_ifce = scene->js_ifce;
	tmp->gf_sg_script_load = scene->gf_sg_script_load;

#ifdef GF_CYCLIC_RENDER_ON
	tmp->max_cyclic_render = scene->max_cyclic_render;
#endif
	tmp->UserNodeInit = scene->UserNodeInit;
	tmp->NodeInitCallback = scene->NodeInitCallback;
	tmp->NodeModified = scene->NodeModified;
	tmp->ModifCallback = scene->ModifCallback;
	return tmp;
}


void gf_sg_set_init_callback(GF_SceneGraph *sg, void (*UserNodeInit)(void *NodeInitCallback, GF_Node *newNode), void *NodeInitCallback)
{
	sg->UserNodeInit = UserNodeInit;
	sg->NodeInitCallback = NodeInitCallback;
}

/*set node modified callback*/
void gf_sg_set_modified_callback(GF_SceneGraph *sg, void (*UserNodeModified)(void *NodeModifiedCallback, GF_Node *newNode), void *NodeModifiedCallback)
{
	sg->NodeModified = UserNodeModified;
	sg->ModifCallback = NodeModifiedCallback;
}

void gf_sg_set_scene_time_callback(GF_SceneGraph *sg, Double (*GetSceneTime)(void *scene_callback), void *cbck)
{
	sg->GetSceneTime = GetSceneTime;
	sg->SceneCallback = cbck;
}


Double gf_node_get_scene_time(GF_Node *node)
{
	if (!node || !node->sgprivate->scenegraph->GetSceneTime) return 0.0;
	return node->sgprivate->scenegraph->GetSceneTime(node->sgprivate->scenegraph->SceneCallback);
}

void gf_sg_set_max_render_cycle(GF_SceneGraph *sg, u16 max_cycle)
{
#ifdef GF_CYCLIC_RENDER_ON
	/*this is a safety guard for the 3D renderer which may call Shape.render from within Shape.render*/
	if (max_cycle<2) max_cycle = 2;
	sg->max_cyclic_render = (u32) max_cycle;
#endif
}

void gf_sg_del(GF_SceneGraph *sg)
{	
	if (!sg) return;

	gf_sg_reset(sg);

	gf_list_del(sg->Routes);
	gf_list_del(sg->protos);
	gf_list_del(sg->unregistered_protos);
	gf_list_del(sg->routes_to_activate);
	gf_list_del(sg->routes_to_destroy);
	free(sg->node_registry);

	free(sg);
}

/*recursive traverse of the whole graph to check for scope mixes (nodes from an inline graph
inserted in a parent graph through bind or routes). We must do this otherwise we're certain to get random
crashes or mem leaks.*/
void SG_GraphRemoved(GF_Node *node, GF_SceneGraph *sg)
{
	u32 i, j, count;
	GF_FieldInfo info;
	GF_Node *n;
	GF_List *mflist;

	count = gf_node_get_field_count(node);
	for (i=0; i<count; i++) {
		gf_node_get_field(node, i, &info);
		if (info.fieldType==GF_SG_VRML_SFNODE) {
			n = *(GF_Node **) info.far_ptr;
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
			mflist = *(GF_List **) info.far_ptr;
			for (j=0; j<gf_list_count(mflist); j++) {
				n = gf_list_get(mflist, j);
				if (n->sgprivate->scenegraph==sg) {
					gf_node_unregister(n, node);
					gf_list_rem(mflist, j);
					j--;
				} else {
					SG_GraphRemoved(n, sg);
				}
			}
		}
	}
}

void gf_sg_reset(GF_SceneGraph *sg)
{
	u32 i;
	if (!sg) return;

	/*inlined graph, remove any of this graph nodes from the parent graph*/
	if (!sg->pOwningProto && sg->parent_scene) {
		GF_SceneGraph *par = sg->parent_scene;
		while (par->parent_scene) par = par->parent_scene;
		if (par->RootNode) SG_GraphRemoved(par->RootNode, sg);
	}
	if (sg->RootNode) gf_node_unregister(sg->RootNode, NULL);
	sg->RootNode = NULL;

	while (gf_list_count(sg->routes_to_activate)) {
		gf_list_rem(sg->routes_to_activate, 0);
	}

	/*destroy all routes*/
	while (gf_list_count(sg->Routes)) {
		GF_Route *r = gf_list_get(sg->Routes, 0);
		/*this will unregister the route from the graph, so don't delete the chain entry*/
		gf_sg_route_del(r);

	}

	/*WATCHOUT: we may have cyclic dependencies due to
	1- a node referencing itself (forbidden in VRML)
	2- nodes refered to in commands of conditionals children of this node (MPEG-4 is mute about that)
	*/
	for (i=0; i<sg->node_reg_size; i++) {
		GF_Node *node = sg->node_registry[i];
		/*first replace all instances in parents by NULL WITHOUT UNREGISTERING (to avoid destroying the node).
		This will take care of nodes referencing themselves*/
#ifdef GF_ARRAY_PARENT_NODES
		u32 j;
		for (j=0; j<gf_list_count(node->sgprivate->parentNodes); j++) {
			GF_Node *par = gf_list_get(node->sgprivate->parentNodes, j);
			ReplaceDEFNode(par, node->sgprivate->NodeID, NULL, 0);
		}
		/*then we remove the node from the registry and destroy it. This will take 
		care of conditional case as we perform special checking when destroying commands*/
		gf_list_reset(node->sgprivate->parentNodes);
#else
		GF_NodeList *nlist = node->sgprivate->parents;
		while (nlist) {
			GF_NodeList *next = nlist->next;
			ReplaceDEFNode(nlist->node, node->sgprivate->NodeID, NULL, 0);
			free(nlist);
			nlist = next;
		}
		node->sgprivate->parents = NULL;
#endif
		sg->node_registry[i] = NULL;
		gf_node_del(node);
	}
	sg->node_reg_size = 0;

	/*destroy all proto*/
	while (gf_list_count(sg->protos)) {
		GF_Proto *p = gf_list_get(sg->protos, 0);
		/*this will unregister the proto from the graph, so don't delete the chain entry*/
		gf_sg_proto_del(p);
	}
	/*destroy all unregistered proto*/
	while (gf_list_count(sg->unregistered_protos)) {
		GF_Proto *p = gf_list_get(sg->unregistered_protos, 0);
		/*this will unregister the proto from the graph, so don't delete the chain entry*/
		gf_sg_proto_del(p);
	}

	/*last destroy all routes*/
	gf_sg_destroy_routes(sg);

	sg->simulation_tick = 0;
}


GFINLINE GF_Node *SG_SearchForDuplicateNodeID(GF_SceneGraph *sg, u32 nodeID, GF_Node *toExclude)
{
	u32 i;
	for (i=0; i<sg->node_reg_size; i++) {
		if (sg->node_registry[i] == toExclude) continue;
		if (sg->node_registry[i]->sgprivate->NodeID == nodeID) {
			return sg->node_registry[i];
		}
	}
	return NULL;
}

GFINLINE GF_Node *SG_SearchForNode(GF_SceneGraph *sg, GF_Node *node)
{
	u32 i;
	for (i=0; i<sg->node_reg_size; i++) {
		if (sg->node_registry[i] == node) {
			return sg->node_registry[i];
		}
	}
	return NULL;
}

static GFINLINE u32 node_search(GF_SceneGraph *sg, u32 low_pos, u32 high_pos, u32 ID) 
{
	u32 mid_pos;

	assert(low_pos<high_pos);

	mid_pos = (high_pos+low_pos)/2;
	
	if (sg->node_registry[mid_pos]->sgprivate->NodeID == ID) return mid_pos;

	/* greater than middle, search upper half */
	if (sg->node_registry[mid_pos]->sgprivate->NodeID < ID) {
		if (mid_pos+1==sg->node_reg_size) {
			if (sg->node_registry[sg->node_reg_size-1]->sgprivate->NodeID >= ID) return sg->node_reg_size-1;
			return sg->node_reg_size;
		}
		if (sg->node_registry[mid_pos+1]->sgprivate->NodeID >= ID) return mid_pos+1;
		
		return node_search(sg, mid_pos+1, high_pos, ID);
	}

	/* less than middle, search lower half */
	if (mid_pos<=1) {
		if (sg->node_registry[0]->sgprivate->NodeID<ID) return 1;
		return 0;
	}
	if (sg->node_registry[mid_pos-1]->sgprivate->NodeID < ID) return mid_pos;
	return node_search(sg, low_pos, mid_pos-1, ID);
}


GFINLINE GF_Node *SG_SearchForNodeByID(GF_SceneGraph *sg, u32 nodeID)
{
	u32 i;
	if (!sg->node_reg_size) return NULL;

	i = node_search(sg, 0, sg->node_reg_size, nodeID);
	if (i>=sg->node_reg_size ||sg->node_registry[i]->sgprivate->NodeID != nodeID) return NULL;
	return sg->node_registry[i];
}

GFINLINE Bool SG_SearchForNodeIndex(GF_SceneGraph *sg, GF_Node *node, u32 *out_index)
{
	u32 i;
	for (i=0; i<sg->node_reg_size; i++) {
		if (sg->node_registry[i] == node) {
			*out_index = i;
			return 1;
		}
	}
	return 0;
}


void gf_sg_set_private(GF_SceneGraph *sg, void *ptr)
{
	if (sg) sg->userpriv = ptr;
}

void *gf_sg_get_private(GF_SceneGraph *sg)
{
	return sg ? sg->userpriv : NULL;
}


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

Bool gf_sg_use_pixel_metrics(GF_SceneGraph *sg)
{
	return (sg ? sg->usePixelMetrics : 0);
}

Bool gf_sg_get_scene_size_info(GF_SceneGraph *sg, u32 *width, u32 *height)
{
	if (!sg) return 0;
	*width = sg->width;
	*height = sg->height;
	return (sg->width && sg->height) ? 1 : 0;
}


GF_Node *gf_sg_get_root_node(GF_SceneGraph *sg)
{
	return sg ? sg->RootNode : NULL;
}

void gf_sg_set_root_node(GF_SceneGraph *sg, GF_Node *node)
{
	if (sg) sg->RootNode = node;
}

GF_Err gf_node_unregister(GF_Node *pNode, GF_Node *parentNode)
{
	u32 node_ind, j;
	GF_SceneGraph *pSG;

	if (!pNode) return GF_OK;
	pSG = pNode->sgprivate->scenegraph;
	/*if this is a proto its is registered in its parent graph, not the current*/
	if (pNode == (GF_Node*)pSG->pOwningProto) pSG = pSG->parent_scene;
	assert(pSG);

#ifdef GF_ARRAY_PARENT_NODES
	if (parentNode) gf_list_rem(pNode->sgprivate->parentNodes, parentNode);
#else
	if (parentNode) {
		GF_NodeList *nlist = pNode->sgprivate->parents;
		if (nlist) {
			if (nlist->node==parentNode) {
				pNode->sgprivate->parents = nlist->next;
				free(nlist);
			} else {
				while (nlist->next) {
					if (nlist->next->node==parentNode) {
						GF_NodeList *item = nlist->next;
						nlist->next = item ? item->next : NULL;
						free(item);
						break;
					}
					nlist = nlist->next;
				}
			}
		}
	}
#endif

	/*unregister the instance*/
	assert(pNode->sgprivate->num_instances);
	pNode->sgprivate->num_instances -= 1;
	
	/*this is just an instance removed*/
	if (pNode->sgprivate->num_instances) return GF_OK;
	
#ifdef GF_ARRAY_PARENT_NODES
	assert(gf_list_count(pNode->sgprivate->parentNodes)==0);
#else
	assert(pNode->sgprivate->parents==NULL);
#endif

	/*if def, remove from sg def table*/
	if (pNode->sgprivate->NodeID) {
		if (!SG_SearchForNodeIndex(pSG, pNode, &node_ind)) {
			assert(0);
		}
		assert (pNode == pSG->node_registry[node_ind]);
		j = pSG->node_reg_size - node_ind - 1;
		if (j) memmove( & pSG->node_registry[node_ind], & pSG->node_registry[node_ind+1], j * sizeof(GF_Node *));
		pSG->node_reg_size -= 1;
	}

	/*check all routes from or to this node and destroy them - cf spec*/
	for (j=0; j<gf_list_count(pSG->Routes); j++) {
		GF_Route *r = gf_list_get(pSG->Routes, j);
		if ( (r->ToNode == pNode) || (r->FromNode == pNode)) {
			gf_sg_route_del(r);
			j--;
		}
	}

	/*delete the node*/
	gf_node_del(pNode);
	return GF_OK;
}

GF_Err gf_node_register(GF_Node *node, GF_Node *parentNode)
{
	GF_SceneGraph *pSG; 
	
	pSG = node->sgprivate->scenegraph;
	/*if this is a proto register to the parent graph, not the current*/
	if (node == (GF_Node*)pSG->pOwningProto) pSG = pSG->parent_scene;
	assert(pSG);

#if 0
	if (node->sgprivate->NodeID) {
		GF_Node *the_node = SG_SearchForNode(pSG, node);
		assert(the_node);
		assert(the_node == node);
	}
#endif

	node->sgprivate->num_instances ++;
	/*parent may be NULL (top node and proto)*/
#ifdef GF_ARRAY_PARENT_NODES
	if (parentNode) gf_list_add(node->sgprivate->parentNodes, parentNode);
#else
	if (parentNode) {
		if (!node->sgprivate->parents) {
			node->sgprivate->parents = malloc(sizeof(GF_NodeList));
			node->sgprivate->parents->next = NULL;
			node->sgprivate->parents->node = parentNode;
		} else {
			GF_NodeList *item, *nlist = node->sgprivate->parents;
			while (nlist->next) nlist = nlist->next;
			item = malloc(sizeof(GF_NodeList));
			item->next = NULL;
			item->node = parentNode;
			nlist->next = item;
		}
	}
#endif
	return GF_OK;
}

void gf_node_unregister_children(GF_Node *container, GF_List *list)
{
	while (gf_list_count(list)) {
		GF_Node *p = gf_list_get(list, 0);
		gf_list_rem(list, 0);
		gf_node_unregister(p, container);
	}
}

/*replace or remove node instance in the given node (eg in all GF_Node or MFNode fields)
this doesn't propagate in the scene graph. If updateOrderedGroup and new_node is NULL, the order field of OG
is updated*/
static void ReplaceDEFNode(GF_Node *FromNode, u32 NodeID, GF_Node *newNode, Bool updateOrderedGroup)
{
	u32 i, j;
	GF_Node *p;
	GF_List *container;

	GF_FieldInfo field;

	/*browse all fields*/
	for (i=0; i<gf_node_get_field_count(FromNode); i++) {
		gf_node_get_field(FromNode, i, &field);
		switch (field.fieldType) {
		case GF_SG_VRML_SFNODE:
			/*set to NULL for SFFields*/
			p = *((GF_Node **) field.far_ptr);
			/*this is a USE / DEF*/
			if (p && (gf_node_get_id(p) == NodeID) ) {
				*((GF_Node **) field.far_ptr) = NULL;
				if (newNode) {
					*((GF_Node **) field.far_ptr) = newNode;
				}
				goto exit;
			}
			break;
		case GF_SG_VRML_MFNODE:
			container = *(GF_List **) field.far_ptr;
			for (j=0; j<gf_list_count(container); j++) {
				p = gf_list_get(container, j);
				/*replace nodes different from newNode but with same ID*/
				if ((newNode == p) || (gf_node_get_id(p) != NodeID)) continue;

				gf_list_rem(container, j);
				if (newNode) {
					gf_list_insert(container, newNode, j);
				}
				else if (updateOrderedGroup && (FromNode->sgprivate->tag==TAG_MPEG4_OrderedGroup)) {
					M_OrderedGroup *og = (M_OrderedGroup *)FromNode;
					gf_sg_vrml_mf_remove(&og->order, GF_SG_VRML_SFINT32, j);
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

/*get all parents of the node and replace, the instance of the node and finally destroy the node*/
GF_Err gf_node_replace(GF_Node *node, GF_Node *new_node, Bool updateOrderedGroup)
{
	u32 i;
	Bool replace_root;
	GF_Node *par;
	GF_SceneGraph *pSG = node->sgprivate->scenegraph;

	/*if this is a proto its is registered in its parent graph, not the current*/
	if (node == (GF_Node*)pSG->pOwningProto) pSG = pSG->parent_scene;
	if (!SG_SearchForNodeIndex(pSG, node, &i)) return GF_BAD_PARAM;
	assert(node == pSG->node_registry[i]);

	/*first check if this is the root node*/
	replace_root = (node->sgprivate->scenegraph->RootNode == node) ? 1 : 0;

#ifdef GF_ARRAY_PARENT_NODES
	while ( (i = gf_list_count(node->sgprivate->parentNodes)) ) {
		par = gf_list_get(node->sgprivate->parentNodes, 0);
		ReplaceDEFNode(par, node->sgprivate->NodeID, new_node, updateOrderedGroup);
				
		/*adds the parent to the new node*/
		if (new_node) gf_node_register(new_node, par);

		/*unregister node*/
		gf_node_unregister(node, par);
		if (i==1) break;	/*we may be destroyed now*/
	}
#else
	while (node->sgprivate->parents) {
		Bool do_break = node->sgprivate->parents->next ? 0 : 1;
		par = node->sgprivate->parents->node;

		ReplaceDEFNode(par, node->sgprivate->NodeID, new_node, updateOrderedGroup);
		if (new_node) gf_node_register(new_node, par);
		gf_node_unregister(node, par);
		if (do_break) break;
	}
#endif

	if (replace_root && new_node) new_node->sgprivate->scenegraph->RootNode = new_node;
	return GF_OK;
}

static GFINLINE void insert_node_def(GF_SceneGraph *sg, GF_Node *def)
{
	u32 i, remain;

	if (sg->node_reg_alloc==sg->node_reg_size) {
		sg->node_reg_alloc+=NODEREG_STEP_ALLOC;
		sg->node_registry = realloc(sg->node_registry, sg->node_reg_alloc * sizeof(GF_Node *));
	}

	i=0;
	if (sg->node_reg_size) {
		i = node_search(sg, 0, sg->node_reg_size, def->sgprivate->NodeID);
	}
	if (i<sg->node_reg_size) {
		remain = sg->node_reg_size-i;
		memmove(&sg->node_registry[i+1], &sg->node_registry[i], sizeof(GF_Node *) * remain);
	}
	sg->node_registry[i] = def;
	sg->node_reg_size++;
}



GF_Err gf_node_set_id(GF_Node *p, u32 ID, const char *name)
{
	char *new_name;
	u32 i, j;
	GF_SceneGraph *pSG; 
	if (!p || !p->sgprivate->scenegraph) return GF_BAD_PARAM;

	pSG = p->sgprivate->scenegraph;
	/*if this is a proto register to the parent graph, not the current*/
	if (p == (GF_Node*)pSG->pOwningProto) pSG = pSG->parent_scene;

	/*new DEF ID*/
	if (!p->sgprivate->NodeID) {
		p->sgprivate->NodeID = ID;
		if (p->sgprivate->NodeName) free(p->sgprivate->NodeName);
		p->sgprivate->NodeName = NULL;
		if (name) p->sgprivate->NodeName = strdup(name);
		assert(pSG);
		if (ID) insert_node_def(pSG, p);
		return GF_OK;
	}

	/*just change DEF name*/
	new_name = name ? strdup(name) : NULL;
	if (p->sgprivate->NodeName) free(p->sgprivate->NodeName);
	p->sgprivate->NodeName = new_name;
	/*same ID, just return*/
	if (p->sgprivate->NodeID == ID) return GF_OK;

	/*different ID, remove from node registry and re-insert (needed because node registry is sorted by IDs)*/
	if (!SG_SearchForNodeIndex(pSG, p, &i)) {
		assert(0);
	}
	assert (p == pSG->node_registry[i]);
	j = pSG->node_reg_size - i - 1;
	if (j) memmove( & pSG->node_registry[i], & pSG->node_registry[i+1], j * sizeof(GF_Node *));
	pSG->node_reg_size -= 1;
	p->sgprivate->NodeID = ID;
	if (ID) insert_node_def(pSG, p);
	return GF_OK;
}

/*calls RenderNode on this node*/
void gf_node_render(GF_Node *node, void *renderStack)
{
#ifdef GF_CYCLIC_RENDER_ON
	u32 max_pass;
#endif
	if (!node) return;

#ifdef GF_CYCLIC_RENDER_ON
	max_pass = (node->sgprivate->render_pass>>16);
	if (!max_pass) max_pass = node->sgprivate->scenegraph->max_cyclic_render;
#endif

	if (node->sgprivate->tag != TAG_ProtoNode) {
#ifdef GF_CYCLIC_RENDER_ON
		if (node->sgprivate->RenderNode && (node->sgprivate->render_pass < max_pass)) { 
			node->sgprivate->render_pass ++;
			node->sgprivate->RenderNode(node, renderStack);
			node->sgprivate->render_pass --;
		}
#else
		if (node->sgprivate->RenderNode)
			node->sgprivate->RenderNode(node, renderStack);
#endif
		return;
	}

	/*proto only traverses its first child*/
	if (((GF_ProtoInstance *) node)->RenderingNode) {
		node = ((GF_ProtoInstance *) node)->RenderingNode;
	}
	/*if no rendering function is assigned this is a real proto (otherwise this is an hardcoded one)*/
	else if (!node->sgprivate->RenderNode) {
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
		node->sgprivate->scenegraph->NodeModified(node->sgprivate->scenegraph->ModifCallback, node);
	}
#ifdef GF_CYCLIC_RENDER_ON
	if (node->sgprivate->RenderNode && (node->sgprivate->render_pass < node->sgprivate->scenegraph->max_cyclic_render)) {
		node->sgprivate->render_pass ++;
		node->sgprivate->RenderNode(node, renderStack);
		node->sgprivate->render_pass --;
	}
#else
	if (node->sgprivate->RenderNode)
		node->sgprivate->RenderNode(node, renderStack);
#endif
}

/*blindly calls RenderNode on all nodes in the "children" list*/
void gf_node_render_children(GF_Node *node, void *renderStack)
{
	u32 i;
	GF_Node *ptr;
	GF_ParentNode *par = (GF_ParentNode *)node;
	for (i=0; i<gf_list_count(par->children); i++) {
		ptr = gf_list_get(par->children, i);
		if (ptr) gf_node_render(ptr, renderStack);
	}
}


GF_Err gf_node_get_field_by_name(GF_Node *node, char *name, GF_FieldInfo *field)
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

GF_SceneGraph *gf_node_get_graph(GF_Node *node)
{
	return (node ? node->sgprivate->scenegraph : NULL);
}


GF_Node *gf_sg_find_node(GF_SceneGraph *sg, u32 nodeID)
{
	GF_Node *node;
	assert(sg);
	node = SG_SearchForNodeByID(sg, nodeID);
	return node;
}

GF_Node *gf_sg_find_node_by_name(GF_SceneGraph *sg, char *name)
{
	u32 i;
	assert(sg);
	for (i=0; i<sg->node_reg_size; i++) {
		if (!sg->node_registry[i]->sgprivate->NodeName) continue;
		if (!strcmp(sg->node_registry[i]->sgprivate->NodeName, name)) {
			return sg->node_registry[i];
		}
	}
	return NULL;
}


u32 gf_sg_get_next_available_node_id(GF_SceneGraph *sg) 
{
	u32 i, ID;
	if (sg->node_reg_size == 0) return 1;
	ID = sg->node_registry[0]->sgprivate->NodeID;
	/*nodes are sorted*/
	for (i=1; i<sg->node_reg_size; i++) {
		if (ID+1<sg->node_registry[i]->sgprivate->NodeID) return ID+1;
		ID = sg->node_registry[i]->sgprivate->NodeID;
	}
	return ID+1;
}

u32 gf_sg_get_max_node_id(GF_SceneGraph *sg)
{
	if (sg->node_reg_size == 0) return 0;
	return sg->node_registry[sg->node_reg_size-1]->sgprivate->NodeID;
}

void gf_node_setup(GF_Node *p, u32 tag)
{
	SAFEALLOC(p->sgprivate, sizeof(NodePriv));
	p->sgprivate->tag = tag;
	p->sgprivate->is_dirty = GF_SG_NODE_DIRTY;
#ifdef GF_ARRAY_PARENT_NODES
	p->sgprivate->parentNodes = gf_list_new();
#endif
}

NodePriv *Node_GetPriv(GF_Node *node)
{
	return node->sgprivate;
}

GF_Node *gf_sg_new_base_node()
{
	GF_Node *newnode = malloc(sizeof(GF_Node));
	gf_node_setup(newnode, TAG_UndefinedNode);
	return newnode;
}
u32 gf_node_get_tag(GF_Node*p)
{
	return p->sgprivate->tag;
}
u32 gf_node_get_id(GF_Node*p)
{
	return p->sgprivate->NodeID;
}
const char *gf_node_get_name(GF_Node*p)
{
	return p->sgprivate->NodeName;
}
void *gf_node_get_private(GF_Node*p)
{
	return p->sgprivate->privateStack;
}
void gf_node_set_private(GF_Node*p, void *pr)
{
	p->sgprivate->privateStack = pr;
}
GF_Err gf_node_set_render_function(GF_Node *p, void (*RenderNode)(GF_Node *node, void *render_stack) )
{
	p->sgprivate->RenderNode = RenderNode;
	return GF_OK;
}

GF_Err gf_node_set_predestroy_function(GF_Node *p, void (*PreDestroyNode)(struct _sfNode *node) )
{
	p->sgprivate->PreDestroyNode = PreDestroyNode;
	return GF_OK;
}

void gf_sg_parent_setup(GF_Node *pNode)
{
	GF_ParentNode *par = (GF_ParentNode *)pNode;
	par->children = gf_list_new();
	pNode->sgprivate->is_dirty |= GF_SG_CHILD_DIRTY;
}
void gf_node_list_del(GF_List *children, GF_Node *parent)
{
	gf_node_unregister_children(parent, children);
	gf_list_del(children);
}

void gf_sg_parent_reset(GF_Node *pNode)
{
	GF_ParentNode *par = (GF_ParentNode *)pNode;
	gf_node_list_del(par->children, pNode);
}



void gf_node_free(GF_Node *node)
{
	if (!node) return;

	if (node->sgprivate->routes) {
		assert(gf_list_count(node->sgprivate->routes)==0);
/*
		while (gf_list_count(node->sgprivate->routes)) {
			GF_Route *r = gf_list_get(node->sgprivate->routes, 0);
			gf_list_rem(node->sgprivate->routes, 0);
			r->FromNode = NULL;
		}
*/

		gf_list_del(node->sgprivate->routes);
		node->sgprivate->routes = NULL;
	}
	if (node->sgprivate->PreDestroyNode) node->sgprivate->PreDestroyNode(node);
#ifdef GF_ARRAY_PARENT_NODES
	assert(! gf_list_count(node->sgprivate->parentNodes));
	gf_list_del(node->sgprivate->parentNodes);
#else
	assert(! node->sgprivate->parents);
#endif
	if (node->sgprivate->NodeName) free(node->sgprivate->NodeName);
	free(node->sgprivate);
	free(node);
}

u32 gf_node_get_parent_count(GF_Node *node)
{
#ifdef GF_ARRAY_PARENT_NODES
	return gf_list_count(node->sgprivate->parentNodes);
#else
	u32 count = 0;
	GF_NodeList *nlist = node->sgprivate->parents;
	while (nlist) { count++; nlist = nlist->next; }
	return count;
#endif
}

GF_Node *gf_node_get_parent(GF_Node *node, u32 idx)
{
#ifdef GF_ARRAY_PARENT_NODES
	return gf_list_get(node->sgprivate->parentNodes, idx);
#else
	GF_NodeList *nlist = node->sgprivate->parents;
	while (idx) { nlist = nlist->next; idx--;}
	return nlist->node;
#endif
}

static GFINLINE void dirty_children(GF_Node *node, u16 val)
{
	u32 i, count;
	GF_FieldInfo info;
	if (!node) return;
	
	node->sgprivate->is_dirty = val;
	count = gf_node_get_field_count(node);
	for (i=0; i<count; i++) {
		gf_node_get_field(node, i, &info);
		if (info.fieldType==GF_SG_VRML_SFNODE) dirty_children(*(GF_Node **)info.far_ptr, val);
		else if (info.fieldType==GF_SG_VRML_MFNODE) {
			GF_List *list = *(GF_List **) info.far_ptr;
			u32 j, n;
			n = gf_list_count(list);
			for (j=0; j<n; j++) 
				dirty_children((GF_Node *)gf_list_get(list, j), val);
		}
	}
}
static void dirty_parents(GF_Node *node)
{
#ifdef GF_ARRAY_PARENT_NODES
	u32 i, count = gf_list_count(node->sgprivate->parentNodes);
	for (i=0; i<count; i++) {
		GF_Node *p = gf_list_get(node->sgprivate->parentNodes, i);
		if (p->sgprivate->is_dirty & GF_SG_CHILD_DIRTY) continue;
		p->sgprivate->is_dirty |= GF_SG_CHILD_DIRTY;
		dirty_parents(p);
	}
#else
	GF_NodeList *nlist = node->sgprivate->parents;
	while (nlist) {
		GF_Node *p = nlist->node;
		if (! (p->sgprivate->is_dirty & GF_SG_CHILD_DIRTY)) {
			p->sgprivate->is_dirty |= GF_SG_CHILD_DIRTY;
			dirty_parents(p);
		}
		nlist = nlist->next;
	}
#endif
}

void gf_node_dirty_set(GF_Node *node, u16 flags, Bool and_dirty_parents)
{
	if (!node) return;
	
	if (flags) node->sgprivate->is_dirty |= flags;
	else node->sgprivate->is_dirty |= GF_SG_NODE_DIRTY;

	if (and_dirty_parents) dirty_parents(node);
}

void gf_node_dirty_clear(GF_Node *node, u16 flag_to_remove)
{
	if (!node) return;
	if (flag_to_remove) node->sgprivate->is_dirty &= ~flag_to_remove;
	else node->sgprivate->is_dirty = 0;
}

u16 gf_node_dirty_get(GF_Node *node)
{
	if (node) return node->sgprivate->is_dirty;
	return 0;
}


void gf_node_dirty_reset(GF_Node *node)
{
	if (!node) return;
	if (node->sgprivate->is_dirty) {
		node->sgprivate->is_dirty = 0;
		dirty_children(node, 0);
	}
}


Bool gf_sg_is_first_render_cycle(GF_Node *n)
{
#ifdef GF_CYCLIC_RENDER_ON
	return ( (n->sgprivate->render_pass & 0x0000FFFF) == 1);
#else
	return 1;
#endif
}


void gf_node_init(GF_Node *node)
{
	GF_SceneGraph *pSG = node->sgprivate->scenegraph;
	assert(pSG);

	/*internal nodes*/
	if (gf_sg_vrml_node_init(node)) return;
	/*user defined init*/
	if (pSG->UserNodeInit) pSG->UserNodeInit(pSG->NodeInitCallback, node);
}


void gf_node_changed(GF_Node *node, GF_FieldInfo *field)
{
	GF_SceneGraph *sg;
	if (!node) return;

	sg = node->sgprivate->scenegraph;
	assert(sg);

	/*internal nodes*/
	if (gf_sg_vrml_node_changed(node, field)) return;

	/*THIS IS BAD, LASeR MUST BE REWRITTEN TO USE FIELD TYPES for SF/MF Nodes*/
#ifdef GPAC_USE_LASeR
	switch (node->sgprivate->tag) {
	case TAG_LASeRTransform:
	case TAG_LASeRUse:
		node->sgprivate->is_dirty |= GF_SG_CHILD_DIRTY;
		break;
	}
#endif

	/*force child dirty tag*/
	if (field && ((field->fieldType==GF_SG_VRML_SFNODE) || (field->fieldType==GF_SG_VRML_MFNODE))) node->sgprivate->is_dirty |= GF_SG_CHILD_DIRTY;
	if (sg->NodeModified) sg->NodeModified(sg->ModifCallback, node);
}

void gf_node_del(GF_Node *node)
{
#ifdef GF_NODE_USE_POINTERS
	node->sgprivate->node_del(node);
#else

	if (node->sgprivate->tag==TAG_UndefinedNode) gf_node_free(node);
	else if (node->sgprivate->tag == TAG_ProtoNode) gf_sg_proto_del_instance((GF_ProtoInstance *)node);
	else if (node->sgprivate->tag<=GF_NODE_RANGE_LAST_MPEG4) gf_sg_mpeg4_node_del(node);
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_X3D) gf_sg_x3d_node_del(node);
#ifndef GPAC_DISABLE_SVG
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG) SVGElement_Del((SVGElement *) node);
#endif
#ifdef GPAC_USE_LASeR
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_LASER) LASeRNode_Del(node);
#endif
	else gf_node_free(node);
#endif
}

u32 gf_node_get_field_count(GF_Node *node)
{
	assert(node);
	if (node->sgprivate->tag <= TAG_UndefinedNode) return 0;
	/*for both MPEG4 & X3D*/
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_X3D) return gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_ALL);
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG) return SVG_GetAttributeCount(node);
#ifdef GPAC_USE_LASeR
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_LASER) return LASeRNode_GetFieldCount(node, 0);
#endif
	return 0;
}



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
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG) return SVG_GetElementName(node->sgprivate->tag);
#ifdef GPAC_USE_LASeR
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_LASER) return LASeR_GetNodeName(node->sgprivate->tag);
#endif
	else return "UnsupportedNode";
#endif
}

GF_Node *gf_node_new(GF_SceneGraph *inScene, u32 tag)
{
	GF_Node *node;
	if (!inScene) return NULL;
	/*cannot create proto this way*/
	if (tag==TAG_ProtoNode) return NULL;
	else if (tag==TAG_UndefinedNode) node = gf_sg_new_base_node();
	else if (tag <= GF_NODE_RANGE_LAST_MPEG4) node = gf_sg_mpeg4_node_new(tag);
	else if (tag <= GF_NODE_RANGE_LAST_X3D) node = gf_sg_x3d_node_new(tag);
#ifndef GPAC_DISABLE_SVG
	else if (tag <= GF_NODE_RANGE_LAST_SVG) node = (GF_Node *) SVG_CreateNode(tag);
#endif
#ifdef GPAC_USE_LASeR
	else if (tag <= GF_NODE_RANGE_LAST_LASER) node = LASeR_CreateNode(tag);
#endif
	else node = NULL;

	if (node) node->sgprivate->scenegraph = inScene;
	/*script is inited as soon as created since fields are dynamically added*/
	if ((tag==TAG_MPEG4_Script) || (tag==TAG_X3D_Script) ) gf_sg_script_init(node);
	return node;
}


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
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG) return SVG_GetAttributeInfo(node, info);
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_LASER) return GF_NOT_SUPPORTED;
#endif
	return GF_NOT_SUPPORTED;
}

/*LASeR specifc, to clean up!!*/
u32 gf_node_get_active(GF_Node*p)
{
#ifdef GPAC_USE_LASeR
	return p->sgprivate->active;
#else
	return 1;
#endif
}

