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
/*svg proto*/
#include <gpac/scenegraph_svg.h>
/*MPEG4 tags (for internal nodes)*/
#include <gpac/nodes_mpeg4.h>
/*X3D tags (for internal nodes)*/
#include <gpac/nodes_x3d.h>
#include <gpac/events.h>
#include <gpac/nodes_svg.h>

static void ReplaceDEFNode(GF_Node *FromNode, GF_Node *node, GF_Node *newNode, Bool updateOrderedGroup);

#ifndef GPAC_DISABLE_SVG
static void ReplaceIRINode(GF_Node *FromNode, GF_Node *oldNode, GF_Node *newNode);
#endif

static void node_modif_stub(GF_SceneGraph *sg, GF_Node *node, GF_FieldInfo *info, GF_Node *script)
{
}

GF_EXPORT
GF_SceneGraph *gf_sg_new()
{
	GF_SceneGraph *tmp;
	GF_SAFEALLOC(tmp, GF_SceneGraph);
	if (!tmp) return NULL;

	tmp->exported_nodes = gf_list_new();

#ifndef GPAC_DISABLE_VRML
	tmp->protos = gf_list_new();
	tmp->unregistered_protos = gf_list_new();
	tmp->Routes = gf_list_new();
	tmp->routes_to_activate = gf_list_new();
	tmp->routes_to_destroy = gf_list_new();
#endif

#ifndef GPAC_DISABLE_SVG
	tmp->dom_evt_mx = gf_mx_new("DOMEvent");
	tmp->dom_evt = gf_dom_event_target_new(GF_DOM_EVENT_TARGET_DOCUMENT, tmp);
	tmp->xlink_hrefs = gf_list_new();
	tmp->smil_timed_elements = gf_list_new();
	tmp->modified_smil_timed_elements = gf_list_new();
	tmp->listeners_to_add = gf_list_new();
#endif

#ifdef GPAC_HAS_SPIDERMONKEY
	tmp->scripts = gf_list_new();
	tmp->objects = gf_list_new();
#endif
	tmp->on_node_modified = node_modif_stub;
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
	tmp->on_node_modified = scene->on_node_modified;

	/*by default use the same callbacks*/
	tmp->userpriv = scene->userpriv;
	tmp->GetSceneTime = scene->GetSceneTime;
	tmp->NodeCallback = scene->NodeCallback;

#ifndef GPAC_DISABLE_VRML
	tmp->GetExternProtoLib = scene->GetExternProtoLib;
#endif
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

#ifndef GPAC_DISABLE_VRML
	if (sg->global_qp) {
		gf_node_unregister(sg->global_qp, NULL);
		sg->global_qp = NULL;
	}
#endif

	gf_sg_reset(sg);

#ifndef GPAC_DISABLE_SVG

	gf_dom_event_target_del(sg->dom_evt);
	gf_list_del(sg->xlink_hrefs);
	gf_list_del(sg->smil_timed_elements);
	gf_list_del(sg->modified_smil_timed_elements);
	gf_list_del(sg->listeners_to_add);
	gf_mx_del(sg->dom_evt_mx);
#endif

#ifdef GPAC_HAS_SPIDERMONKEY
	gf_list_del(sg->scripts);
	sg->scripts = NULL;
	gf_list_del(sg->objects);
	sg->objects = NULL;
#ifndef GPAC_DISABLE_SVG
	if (sg->svg_js) {
		void gf_svg_script_context_del(GF_SVGJS *svg_js, GF_SceneGraph *scenegraph);
		gf_svg_script_context_del(sg->svg_js, sg);
	}
#endif

#endif //GPAC_HAS_SPIDERMONKEY

#ifndef GPAC_DISABLE_VRML
	gf_list_del(sg->Routes);
	gf_list_del(sg->protos);
	gf_list_del(sg->unregistered_protos);
	gf_list_del(sg->routes_to_activate);
	gf_list_del(sg->routes_to_destroy);
#endif
	gf_list_del(sg->exported_nodes);
	gf_free(sg);
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
	if (tag == TAG_ProtoNode) return;

	/*not possible in DOM ?*/
	if (tag>GF_NODE_RANGE_LAST_VRML) return;

	count = gf_node_get_field_count(node);
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
					gf_free(cur);
				} else {
					SG_GraphRemoved(list->node, sg);
				}
				list = list->next;
			}
		}
		/*for uncompressed files (bt/xmt/laserML), also reset command lists*/
		else if (info.fieldType==GF_SG_VRML_SFCOMMANDBUFFER) {
			u32 j, count2;
			SFCommandBuffer *cb = (SFCommandBuffer*)info.far_ptr;
			count2 = gf_list_count(cb->commandList);
			for (j=0; j<count2; j++) {
				u32 k = 0;
				GF_CommandField *f;
				GF_Command *com = gf_list_get(cb->commandList, j);
				while ((f=gf_list_enum(com->command_fields, &k))) {
					switch (f->fieldType) {
					case GF_SG_VRML_SFNODE:
						if (f->new_node) {
							if (f->new_node->sgprivate->scenegraph==sg) {
								/*if root of graph, skip*/
								if (sg->RootNode!=f->new_node) {
									gf_node_unregister(f->new_node, node);
									/*don't forget to remove node...*/
									f->new_node = NULL;
								}
							} else {
								SG_GraphRemoved(f->new_node, sg);
							}
						}
						break;
					case GF_SG_VRML_MFNODE:
						if (f->node_list) {
							GF_ChildNodeItem *cur, *prev, *list = f->node_list;
							prev = NULL;
							while (list) {
								if (list->node->sgprivate->scenegraph==sg) {
									gf_node_unregister(list->node, node);

									if (prev) prev->next = list->next;
									else f->node_list = list->next;
									cur = list;
									gf_free(cur);
								} else {
									SG_GraphRemoved(list->node, sg);
								}
								list = list->next;
							}
						}
						break;
					}
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
	GF_SceneGraph *par;
	GF_List *gc;
#ifndef GPAC_DISABLE_SVG
	u32 type;
#endif
	u32 count;
	NodeIDedItem *reg_node;
	if (!sg) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCENE, ("[SceneGraph] Reseting scene graph\n"));
#if 0
	/*inlined graph, remove any of this graph nodes from the parent graph*/
	if (!sg->pOwningProto && sg->parent_scene) {
		GF_SceneGraph *par = sg->parent_scene;
		while (par->parent_scene) par = par->parent_scene;
		if (par->RootNode) SG_GraphRemoved(par->RootNode, sg);
	}
#endif

	gc = gf_list_new();
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
		/*remember this script was forced to be destroyed*/
		gf_list_add(gc, n);
	}
#endif

#ifndef GPAC_DISABLE_SVG

	gf_mx_p(sg->dom_evt_mx);
	/*remove listeners attached to the doc*/
	gf_dom_event_remove_all_listeners(sg->dom_evt);
	/*flush any pending add_listener*/
	gf_dom_listener_reset_defered(sg);
	gf_mx_v(sg->dom_evt_mx);
#endif

#ifndef GPAC_DISABLE_VRML
	while (gf_list_count(sg->routes_to_activate)) {
		gf_list_rem(sg->routes_to_activate, 0);
	}

	/*destroy all routes*/
	while (gf_list_count(sg->Routes)) {
		GF_Route *r = (GF_Route*)gf_list_get(sg->Routes, 0);
		/*this will unregister the route from the graph, so don't delete the chain entry*/
		gf_sg_route_del(r);

	}
#endif


	/*reset all exported symbols */
	while (gf_list_count(sg->exported_nodes)) {
		GF_Node *n = gf_list_get(sg->exported_nodes, 0);
		gf_list_rem(sg->exported_nodes, 0);
		gf_node_replace(n, NULL, 0);
	}
	/*reassign the list of exported nodes to our garbage collected nodes*/
	gf_list_del(sg->exported_nodes);
	sg->exported_nodes = gc;

	/*reset the main tree*/
	if (sg->RootNode) gf_node_unregister(sg->RootNode, NULL);
	sg->RootNode = NULL;

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
		if (!node
#ifndef GPAC_DISABLE_VRML
		        || (node==sg->global_qp)
#endif
		   ) {
			reg_node = reg_node->next;
			continue;
		}

		/*first replace all instances in parents by NULL WITHOUT UNREGISTERING (to avoid destroying the node).
		This will take care of nodes referencing themselves*/
		{
			GF_ParentList *nlist = node->sgprivate->parents;
#ifndef GPAC_DISABLE_SVG
			type = (node->sgprivate->tag>GF_NODE_RANGE_LAST_VRML) ? 1 : 0;
#endif
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

				/*direct cyclic reference to ourselves, make sure we update the parentList to the next entry before freeing it
				since the next parent node could be reg_node again (reg_node->reg_node)*/
				if (nlist->node==node) {
					node->sgprivate->parents = next;
				}
				gf_free(nlist);
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
		/*remember this node was forced to be destroyed*/
		gf_list_add(sg->exported_nodes, node);
		gf_node_unregister(node, NULL);
		if (count != get_num_id_nodes(sg)) goto restart;
		reg_node = reg_node->next;
	}

	/*reset the forced destroy ndoes*/
	gf_list_reset(sg->exported_nodes);

#ifndef GPAC_DISABLE_VRML
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

	/*last destroy all routes*/
	gf_sg_destroy_routes(sg);
	sg->simulation_tick = 0;

#endif /*GPAC_DISABLE_VRML*/

#ifndef GPAC_DISABLE_SVG
//	assert(gf_list_count(sg->xlink_hrefs) == 0);
#endif

	while (gf_list_count(sg->ns)) {
		GF_XMLNS *ns = gf_list_get(sg->ns, 0);
		gf_list_rem(sg->ns, 0);
		if (ns->name) gf_free(ns->name);
		if (ns->qname) gf_free(ns->qname);
		gf_free(ns);
	}
	gf_list_del(sg->ns);
	sg->ns = 0;

	par = sg;
	while (par->parent_scene) par = par->parent_scene;

#ifndef GPAC_DISABLE_SVG
	if (par != sg) {
		u32 count, i;
		count = gf_list_count(par->smil_timed_elements);
		for (i=0; i<count; i++) {
			SMIL_Timing_RTI *rti = gf_list_get(par->smil_timed_elements, i);
			if (rti->timed_elt->sgprivate->scenegraph == sg) {
				gf_list_rem(par->smil_timed_elements, i);
				i--;
				count--;
			}
		}
	}
#endif

#ifdef GF_SELF_REPLACE_ENABLE
	sg->graph_has_been_reset = 1;
#endif
	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCENE, ("[SceneGraph] Scene graph has been reset\n"));
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

GF_EXPORT
void gf_sg_set_private(GF_SceneGraph *sg, void *ptr)
{
	if (sg) sg->userpriv = ptr;
}

GF_EXPORT
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
#ifndef GPAC_DISABLE_VRML
		while (sg->pOwningProto) sg = sg->parent_scene;
#endif
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

void remove_node_id(GF_SceneGraph *sg, GF_Node *node)
{
	NodeIDedItem *reg_node = sg->id_node;
	if (!reg_node) return;
	
	if (reg_node->node==node) {
		sg->id_node = reg_node->next;
		if (sg->id_node_last==reg_node)
			sg->id_node_last = reg_node->next;
		if (reg_node->NodeName) gf_free(reg_node->NodeName);
		gf_free(reg_node);
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
			if (to_del->NodeName) gf_free(to_del->NodeName);
			to_del->NodeName = NULL;
			gf_free(to_del);
			break;
		}
	}
}

GF_Err gf_node_try_destroy(GF_SceneGraph *sg, GF_Node *pNode, GF_Node *parentNode)
{
	if (!sg) return GF_BAD_PARAM;
	/*if node has been destroyed, don't even look at it*/
	if (gf_list_find(sg->exported_nodes, pNode)>=0) return GF_OK;
	if (!pNode || !pNode->sgprivate->num_instances) return GF_OK;
	return gf_node_unregister(pNode, parentNode);
}

GF_EXPORT
GF_Err gf_node_unregister(GF_Node *pNode, GF_Node *parentNode)
{
#ifndef GPAC_DISABLE_VRML
	u32 j;
	GF_Route *r;
#endif
#ifdef GPAC_HAS_SPIDERMONKEY
	Bool detach=0;
#endif
	GF_SceneGraph *pSG;

	if (!pNode) return GF_OK;
	pSG = pNode->sgprivate->scenegraph;

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
				gf_free(nlist);
#ifdef GPAC_HAS_SPIDERMONKEY
				if (pNode->sgprivate->parents==NULL) detach=1;
#endif
				break;
			}
		}
		if (parentNode->sgprivate->scenegraph != pSG) {
			gf_list_del_item(pSG->exported_nodes, pNode);
		}
	}

#ifndef GPAC_DISABLE_VRML
	/*if this is a proto its is registered in its parent graph, not the current*/
	if (pSG && (pNode == (GF_Node*)pSG->pOwningProto)) pSG = pSG->parent_scene;
#endif

	/*unregister the instance*/
	assert(pNode->sgprivate->num_instances);
	pNode->sgprivate->num_instances -= 1;

	/*this is just an instance removed*/
	if (pNode->sgprivate->num_instances) {
#ifdef GPAC_HAS_SPIDERMONKEY
		if (pNode->sgprivate->num_instances==1) detach=1;
		if (pSG && pNode->sgprivate->scenegraph->on_node_modified && detach && pNode->sgprivate->interact && pNode->sgprivate->interact->js_binding) {
			pNode->sgprivate->scenegraph->on_node_modified(pNode->sgprivate->scenegraph, pNode, NULL, NULL);
		}
#endif
		return GF_OK;
	}

	assert(pNode->sgprivate->parents==NULL);

	if (pSG) {
		/*if def, remove from sg def table*/
		if (pNode->sgprivate->flags & GF_NODE_IS_DEF) {
			remove_node_id(pSG, pNode);
		}

#ifndef GPAC_DISABLE_VRML
		/*check all routes from or to this node and destroy them - cf spec*/
		j=0;
		while ((r = (GF_Route *)gf_list_enum(pSG->Routes, &j))) {
			if ( (r->ToNode == pNode) || (r->FromNode == pNode)) {
				gf_sg_route_del(r);
				j--;
			}
		}
#endif

#ifndef GPAC_DISABLE_SVG
		if (pSG->use_stack && (gf_list_del_item(pSG->use_stack, pNode)>=0)) {
			pSG->abort_bubbling = 1;
		}
#endif

	}
	/*delete the node*/
	if (pNode->sgprivate->scenegraph && (pNode->sgprivate->scenegraph->RootNode==pNode)) {
		pSG = pNode->sgprivate->scenegraph;
		gf_node_del(pNode);
		pSG->RootNode = NULL;
	} else {
		gf_node_del(pNode);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_node_register(GF_Node *node, GF_Node *parentNode)
{
	if (!node) return GF_OK;

	node->sgprivate->num_instances ++;
	/*parent may be NULL (top node and proto)*/
	if (parentNode) {
		if (!node->sgprivate->parents) {
			node->sgprivate->parents = (GF_ParentList*)gf_malloc(sizeof(GF_ParentList));
			node->sgprivate->parents->next = NULL;
			node->sgprivate->parents->node = parentNode;
		} else {
			GF_ParentList *item, *nlist = node->sgprivate->parents;
			while (nlist->next) nlist = nlist->next;
			item = (GF_ParentList*)gf_malloc(sizeof(GF_ParentList));
			item->next = NULL;
			item->node = parentNode;
			nlist->next = item;
		}
		if (parentNode->sgprivate->scenegraph != node->sgprivate->scenegraph) {
			gf_list_add(node->sgprivate->scenegraph->exported_nodes, node);
		}
	}
	return GF_OK;
}

/*replace or remove node instance in the given node (eg in all GF_Node or MFNode fields)
this doesn't propagate in the scene graph. If updateOrderedGroup and new_node is NULL, the order field of OG
is updated*/
static void ReplaceDEFNode(GF_Node *FromNode, GF_Node *node, GF_Node *newNode, Bool updateOrderedGroup)
{
	u32 i, j, count;
	GF_Node *p;
	GF_ChildNodeItem *list;
	GF_FieldInfo field;

	/*browse all fields*/
	count = gf_node_get_field_count(FromNode);
	for (i=0; i<count; i++) {
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
#ifndef GPAC_DISABLE_VRML
					if (updateOrderedGroup && (FromNode->sgprivate->tag==TAG_MPEG4_OrderedGroup)) {
						GF_FieldInfo info;
						M_OrderedGroup *og = (M_OrderedGroup *)FromNode;
						info.fieldIndex = 3;
						info.fieldType = GF_SG_VRML_MFFLOAT;
						info.on_event_in = NULL;
						info.far_ptr = &og->order;
						gf_sg_vrml_mf_remove(&og->order, GF_SG_VRML_SFINT32, j);
						gf_node_changed_internal(FromNode, &info, 1);
					}
#endif
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

#ifndef GPAC_DISABLE_VRML
	/*Notify all scripts that the node is being removed from its parent - we have to do that because nodes used in scripts are not
	always registered in MF/SFNodes fields of the script.*/
	switch (FromNode->sgprivate->tag) {
	case TAG_MPEG4_Script:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Script:
#endif
		if (FromNode->sgprivate->scenegraph->on_node_modified)
			FromNode->sgprivate->scenegraph->on_node_modified(FromNode->sgprivate->scenegraph, node, NULL, FromNode);
		break;
	}
#endif

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
			gf_free(child);
		}
		break;
	}
}
#endif

/*get all parents of the node and replace, the instance of the node and finally destroy the node*/
GF_EXPORT
GF_Err gf_node_replace(GF_Node *node, GF_Node *new_node, Bool updateOrderedGroup)
{
#ifndef GPAC_DISABLE_SVG
	u32 type;
#endif
#ifndef GPAC_DISABLE_VRML
	Bool replace_proto;
#endif
	Bool replace_root;
	GF_Node *par;
	GF_SceneGraph *pSG = node->sgprivate->scenegraph;

#ifndef GPAC_DISABLE_VRML
	/*if this is a proto its is registered in its parent graph, not the current*/
	if (node == (GF_Node*)pSG->pOwningProto) pSG = pSG->parent_scene;
#endif

#ifndef GPAC_DISABLE_SVG
	type = (node->sgprivate->tag>GF_NODE_RANGE_LAST_VRML) ? 1 : 0;
	if (type) {
		Replace_IRI(pSG, node, new_node);
	}
#endif

	/*first check if this is the root node*/
	replace_root = (node->sgprivate->scenegraph->RootNode == node) ? 1 : 0;

#ifndef GPAC_DISABLE_VRML
	replace_proto = 0;
	if (node->sgprivate->scenegraph->pOwningProto
	        && (gf_list_find(node->sgprivate->scenegraph->pOwningProto->node_code, node)>=0)) {
		replace_proto = 1;
	}
#endif

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
#ifndef GPAC_DISABLE_VRML
	if (replace_proto) {
		pSG = node->sgprivate->scenegraph;
		gf_list_del_item(pSG->pOwningProto->node_code, node);
		if (pSG->pOwningProto->RenderingNode==node) pSG->pOwningProto->RenderingNode = NULL;
		gf_node_unregister(node, NULL);
	}
#endif
	return GF_OK;
}

static GFINLINE void insert_node_def(GF_SceneGraph *sg, GF_Node *def, u32 ID, const char *name)
{
	NodeIDedItem *reg_node, *cur;

	reg_node = (NodeIDedItem *) gf_malloc(sizeof(NodeIDedItem));
	reg_node->node = def;
	reg_node->NodeID = ID;
	reg_node->NodeName = name ? gf_strdup(name) : NULL;

	if (!sg->id_node) {
		sg->id_node = reg_node;
		sg->id_node_last = sg->id_node;
		reg_node->next = NULL;
	} else if (sg->id_node_last->NodeID < ID) {
		sg->id_node_last->next = reg_node;
		sg->id_node_last = reg_node;
		reg_node->next = NULL;
	} else if (sg->id_node->NodeID>ID) {
		reg_node->next = sg->id_node;
		sg->id_node = reg_node;
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
		reg_node->next = NULL;
	}
}



GF_EXPORT
GF_Err gf_node_set_id(GF_Node *p, u32 ID, const char *name)
{
	GF_SceneGraph *pSG;
	if (!ID || !p || !p->sgprivate->scenegraph) return GF_BAD_PARAM;

	pSG = p->sgprivate->scenegraph;
#ifndef GPAC_DISABLE_VRML
	/*if this is a proto register to the parent graph, not the current*/
	if (p == (GF_Node*)pSG->pOwningProto) pSG = pSG->parent_scene;
#endif

	/*new DEF ID*/
	if (!(p->sgprivate->flags & GF_NODE_IS_DEF) ) {
		p->sgprivate->flags |= GF_NODE_IS_DEF;
		insert_node_def(pSG, p, ID, name);
	}
	/*reassigning ID, remove node def*/
	else {
		char *_name = gf_strdup(name);
		remove_node_id(pSG, p);
		insert_node_def(pSG, p, ID, _name);
		gf_free(_name);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_node_remove_id(GF_Node *p)
{
	GF_SceneGraph *pSG;
	if (!p) return GF_BAD_PARAM;

	pSG = p->sgprivate->scenegraph;
#ifndef GPAC_DISABLE_VRML
	/*if this is a proto register to the parent graph, not the current*/
	if (p == (GF_Node*)pSG->pOwningProto) pSG = pSG->parent_scene;
#endif

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
void gf_node_traverse(GF_Node *node, void *renderStack)
{
	if (!node || !node->sgprivate) return;

	if (node->sgprivate->flags & GF_NODE_IS_DEACTIVATED) return;

	if (node->sgprivate->UserCallback) {
#ifdef GF_CYCLIC_TRAVERSE_ON
		if (node->sgprivate->flags & GF_NODE_IN_TRAVERSE) return;
		node->sgprivate->flags |= GF_NODE_IN_TRAVERSE;
		assert(node->sgprivate->flags);
#endif
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCENE, ("[SceneGraph] Traversing node %s (ID %s)\n", gf_node_get_class_name(node) , gf_node_get_name(node) ));
		node->sgprivate->UserCallback(node, renderStack, 0);
#ifdef GF_CYCLIC_TRAVERSE_ON
		node->sgprivate->flags &= ~GF_NODE_IN_TRAVERSE;
#endif
		return;
	}

#ifndef GPAC_DISABLE_VRML
	if (node->sgprivate->tag != TAG_ProtoNode) return;

	/*if no rendering function is assigned this is a real proto (otherwise this is an hardcoded one)*/
	if (!node->sgprivate->UserCallback) {

		/*if a rendering node is assigned use it*/
		if (((GF_ProtoInstance *) node)->RenderingNode) {
			node = ((GF_ProtoInstance *) node)->RenderingNode;
			/*if rendering node is a proto and not a hardcoded proto, traverse it*/
			if (!node->sgprivate->UserCallback && (node->sgprivate->tag == TAG_ProtoNode)) {
				gf_node_traverse(node, renderStack);
				return;
			}
		}
		/*if no rendering node, check if the proto is fully instantiated (externProto)*/
		else {
			GF_ProtoInstance *proto_inst = (GF_ProtoInstance *) node;
			gf_node_dirty_clear(node, 0);
			/*proto has been deleted or dummy proto (without node code)*/
			if (!proto_inst->proto_interface || (proto_inst->flags & GF_SG_PROTO_LOADED) ) return;
			/*try to load the code*/
			gf_sg_proto_instantiate(proto_inst);

			/*if user callback is set, this is an hardcoded proto. If not, locate the first traversable node*/
			if (!node->sgprivate->UserCallback) {
				if (!proto_inst->RenderingNode) {
					gf_node_dirty_set(node, 0, 1);
					return;
				}
				/*signal we have been loaded*/
				node->sgprivate->scenegraph->NodeCallback(node->sgprivate->scenegraph->userpriv, GF_SG_CALLBACK_MODIFIED, node, NULL);
			}
		}
	}

	if (node->sgprivate->UserCallback) {
#ifdef GF_CYCLIC_TRAVERSE_ON
		if (node->sgprivate->flags & GF_NODE_IN_TRAVERSE) return;
		node->sgprivate->flags |= GF_NODE_IN_TRAVERSE;
#endif
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCENE, ("[SceneGraph] Traversing node %s\n", gf_node_get_class_name(node) ));
		node->sgprivate->UserCallback(node, renderStack, 0);
#ifdef GF_CYCLIC_TRAVERSE_ON
		node->sgprivate->flags &= ~GF_NODE_IN_TRAVERSE;
#endif
	}

#endif /*GPAC_DISABLE_VRML*/
}

GF_EXPORT
void gf_node_allow_cyclic_traverse(GF_Node *node)
{
#ifdef GF_CYCLIC_TRAVERSE_ON
	if (node) node->sgprivate->flags &= ~GF_NODE_IN_TRAVERSE;
#endif
}


GF_EXPORT
Bool gf_node_set_cyclic_traverse_flag(GF_Node *node, Bool on)
{
	Bool ret = 1;
#ifdef GF_CYCLIC_TRAVERSE_ON
	if (node) {
		ret = (node->sgprivate->flags & GF_NODE_IN_TRAVERSE) ? 0 : 1;
		if (on) {
			node->sgprivate->flags |= GF_NODE_IN_TRAVERSE;
		} else {
			node->sgprivate->flags &= ~GF_NODE_IN_TRAVERSE;
		}
	}
#endif
	return ret;
}

/*blindly calls RenderNode on all nodes in the "children" list*/
GF_EXPORT
void gf_node_traverse_children(GF_Node *node, void *renderStack)
{
	GF_ChildNodeItem *child;

	assert(node);
	child = ((GF_ParentNode *)node)->children;
	while (child) {
		gf_node_traverse(child->node, renderStack);
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

GF_EXPORT
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
	if (!p) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[SceneGraph] Failed to setup NULL node\n"));
		return;
	}
	GF_SAFEALLOC(p->sgprivate, NodePriv);
	if (!p->sgprivate) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[SceneGraph] Failed to allocate node scenegraph private handler\n"));
		return;
	}
	p->sgprivate->tag = tag;
	p->sgprivate->flags = GF_SG_NODE_DIRTY;
}

GF_Node *gf_sg_new_base_node()
{
	GF_Node *newnode = (GF_Node *)gf_malloc(sizeof(GF_Node));
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
#ifndef GPAC_DISABLE_VRML
	/*if this is a proto, look in parent graph*/
	if (p == (GF_Node*)sg->pOwningProto) sg = sg->parent_scene;
#endif

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
	if (!p || !(p->sgprivate->flags & GF_NODE_IS_DEF)) return NULL;

	sg = p->sgprivate->scenegraph;
#ifndef GPAC_DISABLE_VRML
	/*if this is a proto, look in parent graph*/
	if (p == (GF_Node*)sg->pOwningProto) sg = sg->parent_scene;
#endif

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
#ifndef GPAC_DISABLE_VRML
	/*if this is a proto, look in parent graph*/
	if (p == (GF_Node*)sg->pOwningProto) sg = sg->parent_scene;
#endif

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
GF_Err gf_node_set_callback_function(GF_Node *p, void (*TraverseNode)(GF_Node *node, void *render_stack, Bool is_destroy) )
{
	assert(p);
	p->sgprivate->UserCallback = TraverseNode;
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
		gf_free(cur);
	}
}

GF_EXPORT
GF_Err gf_node_list_insert_child(GF_ChildNodeItem **list, GF_Node *n, u32 pos)
{
	GF_ChildNodeItem *child, *cur, *prev;
	u32 cur_pos = 0;

	assert(pos != (u32) -1);

	child = *list;

	cur = (GF_ChildNodeItem*) gf_malloc(sizeof(GF_ChildNodeItem));
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
GF_Err gf_node_list_append_child(GF_ChildNodeItem **list, GF_ChildNodeItem **last_child, GF_Node *n)
{
	GF_ChildNodeItem *child, *cur;

	child = *list;

	cur = (GF_ChildNodeItem*) gf_malloc(sizeof(GF_ChildNodeItem));
	if (!cur) return GF_OUT_OF_MEM;
	cur->node = n;
	cur->next = NULL;

	if (!child) {
		*list = cur;
		*last_child = cur;
	} else {
		if (! *last_child) {
			while (child->next) {
				child = child->next;
			}
			*last_child = child;
		}
		(*last_child)->next = cur;
		*last_child = cur;
	}
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

	cur = (GF_ChildNodeItem*) gf_malloc(sizeof(GF_ChildNodeItem));
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

	cur = (GF_ChildNodeItem*) gf_malloc(sizeof(GF_ChildNodeItem));
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
		gf_free(child);
		return 1;
	}

	while (child->next) {
		if (child->next->node!=n) {
			child = child->next;
			continue;
		}
		cur = child->next;
		child->next = cur->next;
		gf_free(cur);
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
		gf_free(child);
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
		gf_free(cur);
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

	if (node->sgprivate->scenegraph && node->sgprivate->scenegraph->NodeCallback)
		node->sgprivate->scenegraph->NodeCallback(node->sgprivate->scenegraph->userpriv, GF_SG_CALLBACK_NODE_DESTROY, node, NULL);

	if (node->sgprivate->interact) {
		if (node->sgprivate->interact->routes) {
			gf_list_del(node->sgprivate->interact->routes);
		}
#ifndef GPAC_DISABLE_SVG
		if (node->sgprivate->interact->dom_evt) {
			gf_dom_event_remove_all_listeners(node->sgprivate->interact->dom_evt);
			gf_dom_event_target_del(node->sgprivate->interact->dom_evt);
		}
		if (node->sgprivate->interact->animations) {
			gf_list_del(node->sgprivate->interact->animations);
		}
#endif
#ifdef GPAC_HAS_SPIDERMONKEY
		if (node->sgprivate->interact->js_binding) {
			if (node->sgprivate->scenegraph && node->sgprivate->scenegraph->on_node_modified)
				node->sgprivate->scenegraph->on_node_modified(node->sgprivate->scenegraph, node, NULL, NULL);
			gf_list_del(node->sgprivate->interact->js_binding->fields);
			gf_free(node->sgprivate->interact->js_binding);
		}
#endif
		gf_free(node->sgprivate->interact);
	}
	assert(! node->sgprivate->parents);
	gf_free(node->sgprivate);
	gf_free(node);
}

GF_EXPORT
u32 gf_node_get_parent_count(GF_Node *node)
{
	u32 count = 0;
	GF_ParentList *nlist = node->sgprivate->parents;
	while (nlist) {
		count++;
		nlist = nlist->next;
	}
	return count;
}

GF_EXPORT
GF_Node *gf_node_get_parent(GF_Node *node, u32 idx)
{

	GF_ParentList *nlist = node->sgprivate->parents;
	/*break cyclic graphs*/
	if (node->sgprivate->scenegraph->RootNode==node) return NULL;
#ifndef GPAC_DISABLE_VRML
	if (node->sgprivate->scenegraph->pOwningProto && node->sgprivate->scenegraph->pOwningProto->RenderingNode==node)
		return NULL;
#endif
	if (!nlist) return NULL;
	while (idx) {
		nlist = nlist->next;
		idx--;
	}
	return nlist ? nlist->node : NULL;
}

static void dirty_children(GF_Node *node)
{
	u32 i, count;
	GF_FieldInfo info;
	if (!node) return;

	node->sgprivate->flags &= GF_NODE_INTERNAL_FLAGS;
	if (node->sgprivate->tag>=GF_NODE_RANGE_LAST_VRML) {
		GF_ChildNodeItem *child = ((GF_ParentNode*)node)->children;
		while (child) {
			dirty_children(child->node);
			child = child->next;
		}
	} else {
		count = gf_node_get_field_count(node);
		for (i=0; i<count; i++) {
			gf_node_get_field(node, i, &info);
			if (info.fieldType==GF_SG_VRML_SFNODE) dirty_children(*(GF_Node **)info.far_ptr);
			else if (info.fieldType==GF_SG_VRML_MFNODE) {
				GF_ChildNodeItem *list = *(GF_ChildNodeItem **) info.far_ptr;
				while (list) {
					dirty_children(list->node);
					list = list->next;
				}
			}
		}
	}
}
static void dirty_parents(GF_Node *node)
{
	Bool check_root = 1;
	GF_ParentList *nlist;
#if defined GPAC_ANDROID
	if ( !node || !node->sgprivate )
		return;
#else
	if (!node) return;
#endif
	nlist = node->sgprivate->parents;
	while (nlist) {
		GF_Node *p = nlist->node;
		if (! (p->sgprivate->flags & GF_SG_CHILD_DIRTY)) {
			p->sgprivate->flags |= GF_SG_CHILD_DIRTY;
			dirty_parents(p);
		}
		check_root = 0;
		nlist = nlist->next;
	}
	/*propagate to parent scene graph */
#if defined GPAC_ANDROID
	if (check_root && node->sgprivate->scenegraph) {
#else
	if (check_root) {
#endif
		/*if root node of the scenegraph*/
		if (node->sgprivate->scenegraph->NodeCallback && (node==node->sgprivate->scenegraph->RootNode) ) {
			node->sgprivate->scenegraph->NodeCallback(node->sgprivate->scenegraph->userpriv, GF_SG_CALLBACK_GRAPH_DIRTY, NULL, NULL);
		}
#ifndef GPAC_DISABLE_VRML
		/*or if parent graph is a protoinstance but the node is not this proto*/
		else if (node->sgprivate->scenegraph->pOwningProto) {
			GF_Node *the_node = (GF_Node *) node->sgprivate->scenegraph->pOwningProto;
			if (the_node != node) dirty_parents(the_node);
		}
#endif
	}
}
GF_EXPORT
void gf_node_dirty_parent_graph(GF_Node *node)
{
	/*if root node of the scenegraph*/
	if (node->sgprivate->scenegraph->NodeCallback ) {
		node->sgprivate->scenegraph->NodeCallback(node->sgprivate->scenegraph->userpriv, GF_SG_CALLBACK_GRAPH_DIRTY, NULL, NULL);
	}
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
void gf_node_dirty_reset(GF_Node *node, Bool reset_children)
{
	if (!node) return;
	if (node->sgprivate->flags & ~GF_NODE_INTERNAL_FLAGS) {
		node->sgprivate->flags &= GF_NODE_INTERNAL_FLAGS;
		if (reset_children) {
			dirty_children(node);
#ifndef GPAC_DISABLE_VRML
		} else if (node->sgprivate->tag==TAG_MPEG4_Appearance) {
			gf_node_dirty_reset( ((M_Appearance*)node)->material, 1);
#endif
		}
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
#ifndef GPAC_DISABLE_VRML
	if (gf_sg_vrml_node_init(node)) return;
#endif

#ifndef GPAC_DISABLE_SVG
	if (gf_svg_node_init(node)) return;
#endif

	/*user defined init*/
	pSG->NodeCallback(pSG->userpriv, GF_SG_CALLBACK_INIT, node, NULL);
}


void gf_node_changed_internal(GF_Node *node, GF_FieldInfo *field, Bool notify_scripts)
{
	GF_SceneGraph *sg;
	if (!node) return;

	sg = node->sgprivate->scenegraph;
	assert(sg);

#ifndef GPAC_DISABLE_VRML
	/*signal changes in node to JS for MFFields*/
	if (field && notify_scripts && (node->sgprivate->flags & GF_NODE_HAS_BINDING) && !gf_sg_vrml_is_sf_field(field->fieldType) ) {
		sg->on_node_modified(sg, node, field, NULL);
	}
#endif

#ifndef GPAC_DISABLE_SVG
	if (field && node->sgprivate->interact && node->sgprivate->interact->dom_evt) {
		GF_DOM_Event evt;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.bubbles = 1;
		evt.type = GF_EVENT_ATTR_MODIFIED;
		evt.attr = field;
		evt.detail = field->fieldIndex;
		gf_dom_event_fire(node, &evt);
	}
#endif


	/*internal nodes*/
#ifndef GPAC_DISABLE_VRML
	if (gf_sg_vrml_node_changed(node, field)) return;
#endif

#ifndef GPAC_DISABLE_SVG
	if (gf_svg_node_changed(node, field)) return;
#endif

	/*force child dirty tag*/
	if (field && ( (field->fieldType==GF_SG_VRML_SFNODE) || (field->fieldType==GF_SG_VRML_MFNODE)) )
		node->sgprivate->flags |= GF_SG_CHILD_DIRTY;

	if (sg->NodeCallback) sg->NodeCallback(sg->userpriv, GF_SG_CALLBACK_MODIFIED, node, field);
}

GF_EXPORT
void gf_node_changed(GF_Node *node, GF_FieldInfo *field)
{
	gf_node_changed_internal(node, field, 1);

#ifndef GPAC_DISABLE_SVG
	/* we should avoid dispatching a DOMSubtreeModified event on insertion of time values in begin/end fields
	   because this retriggers begin/end events and reinsertion */
	if ((field == NULL || ((field->fieldIndex != TAG_SVG_ATT_begin) && (field->fieldIndex != TAG_SVG_ATT_end))) &&
	        node->sgprivate->tag >= GF_NODE_RANGE_FIRST_SVG && node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG) {
		GF_DOM_Event evt;
		evt.type = GF_EVENT_TREE_MODIFIED;
		evt.bubbles = 0;
		evt.relatedNode = node;
		gf_dom_event_fire(node, &evt);
	}
#endif
}

void gf_node_del(GF_Node *node)
{
	if (node->sgprivate->tag==TAG_UndefinedNode) gf_node_free(node);
	else if (node->sgprivate->tag==TAG_DOMText) {
		GF_DOMText *t = (GF_DOMText *)node;
		if (t->textContent) gf_free(t->textContent);
		gf_sg_parent_reset(node);
		gf_node_free(node);
	}
	else if (node->sgprivate->tag==TAG_DOMUpdates) {
		u32 i, count;
		GF_DOMUpdates *up = (GF_DOMUpdates *)node;
		if (up->data) gf_free(up->data);
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
#ifndef GPAC_DISABLE_SVG
		gf_node_delete_attributes(node);
#endif
		if (n->name) gf_free(n->name);
		gf_sg_parent_reset(node);
		gf_node_free(node);
	}
#ifndef GPAC_DISABLE_VRML
	else if (node->sgprivate->tag == TAG_ProtoNode) gf_sg_proto_del_instance((GF_ProtoInstance *)node);
#endif
#ifndef GPAC_DISABLE_VRML
	else if (node->sgprivate->tag<=GF_NODE_RANGE_LAST_MPEG4) gf_sg_mpeg4_node_del(node);
#ifndef GPAC_DISABLE_X3D
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_X3D) gf_sg_x3d_node_del(node);
#endif
#endif /*GPAC_DISABLE_VRML*/

#ifndef GPAC_DISABLE_SVG
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_SVG) gf_svg_node_del(node);
#endif
	else gf_node_free(node);
}

GF_EXPORT
u32 gf_node_get_field_count(GF_Node *node)
{
	assert(node);
	if (node->sgprivate->tag <= TAG_UndefinedNode) return 0;
#ifndef GPAC_DISABLE_VRML
	/*for both MPEG4 & X3D*/
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_X3D) return gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_ALL);
#endif
#ifndef GPAC_DISABLE_SVG
	else if (node->sgprivate->tag >= GF_NODE_FIRST_DOM_NODE_TAG) return gf_node_get_attribute_count(node);
#endif
	return 0;
}

GF_EXPORT
const char *gf_node_get_class_name(GF_Node *node)
{
	assert(node && node->sgprivate->tag);
	if (node->sgprivate->tag==TAG_UndefinedNode) return "UndefinedNode";
#ifndef GPAC_DISABLE_VRML
	else if (node->sgprivate->tag==TAG_ProtoNode) return ((GF_ProtoInstance*)node)->proto_name;
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_MPEG4) return gf_sg_mpeg4_node_get_class_name(node->sgprivate->tag);
#ifndef GPAC_DISABLE_X3D
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_X3D) return gf_sg_x3d_node_get_class_name(node->sgprivate->tag);
#endif
#endif
	else if (node->sgprivate->tag==TAG_DOMText) return "DOMText";
	else if (node->sgprivate->tag==TAG_DOMFullNode) {
		char *xmlns;
		GF_DOMFullNode*full = (GF_DOMFullNode*)node;
		u32 ns = gf_sg_get_namespace_code(node->sgprivate->scenegraph, NULL);
		if (ns == full->ns) return full->name;
		xmlns = (char *) gf_sg_get_namespace_qname(node->sgprivate->scenegraph, full->ns);
		if (!xmlns) return full->name;
		sprintf(node->sgprivate->scenegraph->szNameBuffer, "%s:%s", xmlns, full->name);
		return node->sgprivate->scenegraph->szNameBuffer;
	}
#ifndef GPAC_DISABLE_SVG
	else return gf_xml_get_element_name(node);
#endif
	return "UnsupportedNode";
}

GF_EXPORT
u32 gf_sg_node_get_tag_by_class_name(const char *name, u32 ns)
{
	u32 tag = TAG_UndefinedNode;

	/* TODO: handle name spaces */
#ifndef GPAC_DISABLE_VRML
	tag = gf_node_mpeg4_type_by_class_name(name);
	if (tag) return tag;

#ifndef GPAC_DISABLE_X3D
	tag = gf_node_x3d_type_by_class_name(name);
	if (tag) return tag;
#endif

#endif

#ifndef GPAC_DISABLE_SVG
	tag = gf_xml_get_element_tag(name, ns);
	if (tag != TAG_UndefinedNode) return tag;
#endif

	return 	tag;
}

GF_EXPORT
GF_Node *gf_node_new(GF_SceneGraph *inScene, u32 tag)
{
	GF_Node *node;
//	if (!inScene) return NULL;
	/*cannot create proto this way*/
	if (tag==TAG_ProtoNode) return NULL;
	else if (tag==TAG_UndefinedNode) node = gf_sg_new_base_node();
#ifndef GPAC_DISABLE_VRML
	else if (tag <= GF_NODE_RANGE_LAST_MPEG4) node = gf_sg_mpeg4_node_new(tag);
#ifndef GPAC_DISABLE_X3D
	else if (tag <= GF_NODE_RANGE_LAST_X3D) node = gf_sg_x3d_node_new(tag);
#endif
#endif
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
	else if (tag <= GF_NODE_RANGE_LAST_SVG) node = (GF_Node *) gf_svg_create_node(tag);
	else if (tag <= GF_NODE_RANGE_LAST_XBL) node = (GF_Node *) gf_xbl_create_node(tag);
#endif
	else node = NULL;

	if (node) node->sgprivate->scenegraph = inScene;

	/*script is inited as soon as created since fields are dynamically added*/
#ifndef GPAC_DISABLE_VRML
	switch (tag) {
	case TAG_MPEG4_Script:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Script:
#endif
		gf_sg_script_init(node);
		break;
	}
#endif

	return node;
}


GF_EXPORT
GF_Err gf_node_get_field(GF_Node *node, u32 FieldIndex, GF_FieldInfo *info)
{
	assert(node);
	assert(info);
	memset(info, 0, sizeof(GF_FieldInfo));
	info->fieldIndex = FieldIndex;

	if (node->sgprivate->tag==TAG_UndefinedNode) return GF_BAD_PARAM;
#ifndef GPAC_DISABLE_VRML
	else if (node->sgprivate->tag == TAG_ProtoNode) return gf_sg_proto_get_field(NULL, node, info);
	else if (node->sgprivate->tag == TAG_MPEG4_Script)
		return gf_sg_script_get_field(node, info);
#ifndef GPAC_DISABLE_X3D
	else if (node->sgprivate->tag == TAG_X3D_Script)
		return gf_sg_script_get_field(node, info);
#endif
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_MPEG4) return gf_sg_mpeg4_node_get_field(node, info);
#ifndef GPAC_DISABLE_X3D
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_X3D) return gf_sg_x3d_node_get_field(node, info);
#endif
#endif /*GPAC_DISABLE_VRML*/

#ifndef GPAC_DISABLE_SVG
	else if (node->sgprivate->tag >= GF_NODE_FIRST_DOM_NODE_TAG) return gf_node_get_attribute_info(node, info);
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
	for (i=0; i<count; i++) {
		gf_node_get_field(node, i, field);
		if (!strcmp(field->name, name)) return GF_OK;
	}
	return GF_BAD_PARAM;
}

GF_EXPORT
GF_Err gf_node_get_field_by_name(GF_Node *node, char *name, GF_FieldInfo *field)
{
	s32 res = -1;

	if (node->sgprivate->tag==TAG_UndefinedNode) return GF_BAD_PARAM;
#ifndef GPAC_DISABLE_VRML
	else if (node->sgprivate->tag == TAG_ProtoNode) {
		res = gf_sg_proto_get_field_index_by_name(NULL, node, name);
	}
	else if (node->sgprivate->tag == TAG_MPEG4_Script)
		return gf_node_get_field_by_name_enum(node, name, field);
#ifndef GPAC_DISABLE_X3D
	else if (node->sgprivate->tag == TAG_X3D_Script)
		return gf_node_get_field_by_name_enum(node, name, field);
#endif
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_MPEG4) res = gf_sg_mpeg4_node_get_field_index_by_name(node, name);
#ifndef GPAC_DISABLE_X3D
	else if (node->sgprivate->tag <= GF_NODE_RANGE_LAST_X3D) res = gf_sg_x3d_node_get_field_index_by_name(node, name);
#endif
#endif /*GPAC_DISABLE_VRML*/

#ifndef GPAC_DISABLE_SVG
	else if (node->sgprivate->tag >= GF_NODE_FIRST_DOM_NODE_TAG) return gf_node_get_attribute_by_name(node, name, 0, 1, 0, field);
#endif
	if (res==-1) return GF_BAD_PARAM;
	return gf_node_get_field(node, (u32) res, field);
}


static char log_node_name[2+16+1];
const char *gf_node_get_log_name(GF_Node *anim)
{
	const char *name = gf_node_get_name(anim);
	if (name) return name;
	else {
		sprintf(log_node_name, "%p", anim);
		return log_node_name;
	}
}



static GF_Err gf_node_deactivate_ex(GF_Node *node)
{
#ifdef GPAC_DISABLE_SVG
	return GF_NOT_SUPPORTED;
#else
	GF_ChildNodeItem *item;
	if (node->sgprivate->tag<GF_NODE_FIRST_DOM_NODE_TAG) return GF_BAD_PARAM;
	if (! (node->sgprivate->flags & GF_NODE_IS_DEACTIVATED)) {

		node->sgprivate->flags |= GF_NODE_IS_DEACTIVATED;

		/*deactivate anmiations*/
		if (gf_svg_is_timing_tag(node->sgprivate->tag)) {
			SVGTimedAnimBaseElement *timed = (SVGTimedAnimBaseElement*)node;
			if (gf_list_del_item(node->sgprivate->scenegraph->smil_timed_elements, timed->timingp->runtime)>=0) {
				if (timed->timingp->runtime->evaluate) {
					timed->timingp->runtime->evaluate(timed->timingp->runtime, 0, SMIL_TIMING_EVAL_DEACTIVATE);
				}
			}
		}
		/*TODO unregister all listeners*/

	}
	/*and deactivate children*/
	item = ((GF_ParentNode*)node)->children;
	while (item) {
		gf_node_deactivate_ex(item->node);
		item = item->next;
	}
	return GF_OK;
#endif
}

GF_Err gf_node_deactivate(GF_Node *node)
{
	GF_Err e = gf_node_deactivate_ex(node);
	gf_node_changed(node, NULL);
	return e;
}

static u32 gf_node_activate_ex(GF_Node *node)
{
#ifdef GPAC_DISABLE_SVG
	return 0;
#else
	u32 ret = 0;
	GF_ChildNodeItem *item;
	if (node->sgprivate->tag<GF_NODE_FIRST_DOM_NODE_TAG) return 0;
	if (node->sgprivate->flags & GF_NODE_IS_DEACTIVATED) {

		node->sgprivate->flags &= ~GF_NODE_IS_DEACTIVATED;
		ret ++;

		/*deactivate anmiations*/
		if (gf_svg_is_timing_tag(node->sgprivate->tag)) {
			SVGTimedAnimBaseElement *timed = (SVGTimedAnimBaseElement*)node;
			gf_list_add(node->sgprivate->scenegraph->smil_timed_elements, timed->timingp->runtime);
			node->sgprivate->flags &= ~GF_NODE_IS_DEACTIVATED;
			if (timed->timingp->runtime->evaluate) {
				timed->timingp->runtime->evaluate(timed->timingp->runtime, 0, SMIL_TIMING_EVAL_ACTIVATE);
			}
		}
		/*TODO register all listeners*/

	}
	/*and deactivate children*/
	item = ((GF_ParentNode*)node)->children;
	while (item) {
		ret += gf_node_activate_ex(item->node);
		item = item->next;
	}
	return ret;
#endif
}

GF_Err gf_node_activate(GF_Node *node)
{
	if (!node) return GF_BAD_PARAM;
	if (gf_node_activate_ex(node))
		gf_node_changed(node, NULL);
	return GF_OK;
}

GF_EXPORT
GF_Node *gf_node_clone(GF_SceneGraph *inScene, GF_Node *orig, GF_Node *cloned_parent, char *id, Bool deep)
{
	if (!orig) return NULL;
	if (orig->sgprivate->tag < GF_NODE_RANGE_LAST_VRML) {
#ifndef GPAC_DISABLE_VRML
		/*deep clone is always true for VRML*/
		return gf_vrml_node_clone(inScene, orig, cloned_parent, id);
#endif
	} else if (orig->sgprivate->tag == TAG_DOMUpdates) {
		/*TODO*/
		return NULL;
	} else {
#ifndef GPAC_DISABLE_SVG
		return gf_xml_node_clone(inScene, orig, cloned_parent, id, deep);
#endif
	}
	return NULL;
}

GF_NamespaceType gf_xml_get_namespace_id(char *name)
{
	if (!strcmp(name, "http://www.w3.org/XML/1998/namespace")) return GF_XMLNS_XML;
	else if (!strcmp(name, "http://www.w3.org/2001/xml-events")) return GF_XMLNS_XMLEV;
	else if (!strcmp(name, "http://www.w3.org/1999/xlink")) return GF_XMLNS_XLINK;
	else if (!strcmp(name, "http://www.w3.org/2000/svg")) return GF_XMLNS_SVG;
	else if (!strcmp(name, "urn:mpeg:mpeg4:laser:2005")) return GF_XMLNS_LASER;
	else if (!strcmp(name, "http://www.w3.org/ns/xbl")) return GF_XMLNS_XBL;
	else if (!strcmp(name, "http://gpac.io/svg-extensions")) return GF_XMLNS_SVG_GPAC_EXTENSION;
	return GF_XMLNS_UNDEFINED;
}

GF_EXPORT
GF_Err gf_sg_add_namespace(GF_SceneGraph *sg, char *name, char *qname)
{
	u32 id;
	GF_XMLNS *ns;
	if (!name) return GF_BAD_PARAM;

	id = gf_xml_get_namespace_id(name);

	if (!sg->ns) sg->ns = gf_list_new();

	GF_SAFEALLOC(ns, GF_XMLNS);
	if (!ns) return GF_OUT_OF_MEM;
	
	ns->xmlns_id = id ? id : gf_crc_32(name, (u32) strlen(name));
	ns->name = gf_strdup(name);

	ns->qname = qname ? gf_strdup(qname) : NULL;
	return gf_list_insert(sg->ns, ns, 0);
}

GF_Err gf_sg_remove_namespace(GF_SceneGraph *sg, char *ns_name, char *q_name)
{
	u32 i, count;
	if (!ns_name) return GF_OK;
	count = sg->ns ? gf_list_count(sg->ns) : 0;
	for (i=0; i<count; i++) {
		Bool ok=0;
		GF_XMLNS *ns = gf_list_get(sg->ns, i);
		if (!q_name && !ns->qname)
			ok = 1;
		else if (q_name && ns->qname && !strcmp(ns->qname, q_name) )
			ok = 1;

		if (ok && ns->name && !strcmp(ns->name, ns_name)) {
			gf_list_rem(sg->ns, i);
			gf_free(ns->name);
			if (ns->qname) gf_free(ns->qname);
			gf_free(ns);
			return GF_OK;
		}
	}
	return GF_OK;
}

u32 gf_sg_get_namespace_code(GF_SceneGraph *sg, char *qname)
{
	GF_XMLNS *ns;
	u32 i, count;
	count = sg->ns ? gf_list_count(sg->ns) : 0;
	for (i=0; i<count; i++) {
		ns = gf_list_get(sg->ns, i);
		if (!ns->qname && !qname)
			return ns->xmlns_id;

		if (ns->qname && qname && !strcmp(ns->qname, qname))
			return ns->xmlns_id;
	}
	if (qname) {
		if (!strcmp(qname, "xml")) return GF_XMLNS_XML;
		/*we could also add the basic namespaces in case this has been forgotten ?*/
	}
	return GF_XMLNS_UNDEFINED;
}

u32 gf_sg_get_namespace_code_from_name(GF_SceneGraph *sg, char *name)
{
	GF_XMLNS *ns;
	u32 i, count;
	count = sg->ns ? gf_list_count(sg->ns) : 0;
	for (i=0; i<count; i++) {
		ns = gf_list_get(sg->ns, i);
		if (ns->name && name && !strcmp(ns->name, name))
			return ns->xmlns_id;
		if (!ns->name && !name)
			return ns->xmlns_id;
	}
	return GF_XMLNS_UNDEFINED;
}

const char *gf_sg_get_namespace_qname(GF_SceneGraph *sg, GF_NamespaceType xmlns_id)
{
	GF_XMLNS *ns;
	u32 i, count;
	count = sg->ns ? gf_list_count(sg->ns) : 0;
	for (i=0; i<count; i++) {
		ns = gf_list_get(sg->ns, i);
		if (ns->xmlns_id == xmlns_id)
			return ns->qname;
	}
	if (xmlns_id==GF_XMLNS_XML) return "xml";
	return NULL;
}


const char *gf_sg_get_namespace(GF_SceneGraph *sg, GF_NamespaceType xmlns_id)
{
	GF_XMLNS *ns;
	u32 i, count;
	count = sg->ns ? gf_list_count(sg->ns) : 0;
	for (i=0; i<count; i++) {
		ns = gf_list_get(sg->ns, i);
		if (ns->xmlns_id == xmlns_id)
			return ns->name;
	}
	return NULL;
}


GF_EXPORT
char *gf_node_dump_attribute(GF_Node *n, GF_FieldInfo *info)
{
#ifndef GPAC_DISABLE_SVG
	if (gf_node_get_tag(n) >= GF_NODE_FIRST_DOM_NODE_TAG) {
		return gf_svg_dump_attribute(n, info);
	}
#endif

#ifndef GPAC_DISABLE_VRML
	return gf_node_vrml_dump_attribute(n, info);
#else
	return NULL;
#endif
}


/*this is not a NodeReplace, thus only the given container is updated - pos is 0-based*/
GF_EXPORT
GF_Err gf_node_replace_child(GF_Node *node, GF_ChildNodeItem **container, s32 pos, GF_Node *newNode)
{
	GF_ChildNodeItem *child, *prev;
	u32 tag;
	u32 cur_pos = 0;

	child = *container;
	prev = NULL;
	while (child->next) {
		if ((pos<0) || (cur_pos!=(u32)pos)) {
			prev = child;
			child = child->next;
			cur_pos++;
			continue;
		}
		break;
	}
	tag = child->node->sgprivate->tag;
	gf_node_unregister(child->node, node);
	if (newNode) {
		child->node = newNode;
#ifndef GPAC_DISABLE_VRML
		if (tag==TAG_MPEG4_ColorTransform)
			node->sgprivate->flags |= GF_SG_VRML_COLOR_DIRTY;
#endif
	} else {
		if (prev) prev->next = child->next;
		else *container = child->next;
		gf_free(child);
	}
	return GF_OK;
}


GF_EXPORT
Bool gf_node_parent_of(GF_Node *node, GF_Node *target)
{
	u32 i, count;
	GF_FieldInfo info;
	if (!node) return 0;
	if (node==target) return 1;

	if (node->sgprivate->tag>=GF_NODE_RANGE_LAST_VRML) {
		GF_ChildNodeItem *child = ((GF_ParentNode*)node)->children;
		while (child) {
			if (gf_node_parent_of(child->node, target)) return 1;
			child = child->next;
		}
	} else {
		count = gf_node_get_field_count(node);
		for (i=0; i<count; i++) {
			gf_node_get_field(node, i, &info);
			if (info.fieldType==GF_SG_VRML_SFNODE) {
				if (gf_node_parent_of(*(GF_Node **)info.far_ptr, target)) return 1;
			}
			else if (info.fieldType==GF_SG_VRML_MFNODE) {
				GF_ChildNodeItem *list = *(GF_ChildNodeItem **) info.far_ptr;
				while (list) {
					if (gf_node_parent_of(list->node, target)) return 1;
					list = list->next;
				}
			}
		}
	}
	return 0;
}

GF_SceneGraph *gf_sg_get_parent(GF_SceneGraph *scene)
{
	return scene ? scene->parent_scene : NULL;
}

