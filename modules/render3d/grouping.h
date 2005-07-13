/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 3D rendering module
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

#ifndef GROUPING_H
#define GROUPING_H

#include "visual_surface.h"

/*
		NOTES about grouping nodes

***when a grouping node impacts placement of its children, rendering is done in 2 step: first traverse
the children and collect each direct child bounding rect (empty if 3D), then computes local transform 
per child according to the display rules of the node (form, layout).  

*/

typedef struct 
{
	/*the one and only child*/
	GF_Node *child;
	/*bounds before (projection of bound_vol) and after placement*/
	GF_Rect original, final;

	/*set if this group contains only text graphic nodes*/
	Bool is_text_group;
	/*in which case ascent and descent of the text is stored for baseline alignment*/
	Fixed ascent, descent;
	u32 split_text_idx;
} ChildGroup;


/*
	@children: pointer to children field
	@groups: list of ChildGroup visually render - used for post placement
	@sensors: cached list of sensors at this level, NULL if none
	@lights: cached list of lights (local and global) at this level, NULL if none
	@bbox: bounding box of sub-tree
	@dont_cull: if set, traversing always occur (needed for audio subgraph)
*/

#define GROUPINGNODESTACK	\
	GF_List *children;		\
	GF_List *groups;			\
	GF_List *sensors;			\
	GF_List *lights;			\
	GF_BBox bbox;			\
	Bool dont_cull;			\

typedef struct _parent_group
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GROUPINGNODESTACK
} GroupingNode;

/*performs stack init/destroy - doesn't free stack*/
void SetupGroupingNode(GroupingNode *ptr, GF_Renderer *sr, GF_Node *node, GF_List *children);
void DeleteGroupingNode(GroupingNode *gr);
/*destroy base stack*/
void DestroyBaseGrouping(GF_Node *node);
/*creates grouping stack and register callbacks*/
void NewGroupingNodeStack(GF_Renderer *sr, GF_Node *node, GF_List *children);


/*traverse all children of the node @alt_positions: alternate order for children - MUST HAVE THE EXACT 
SAME NUMBER OF INDICES AS THERE ARE CHILDREN - null for normal order*/
void grouping_traverse(GroupingNode *group, RenderEffect3D *effects, u32 *alt_positions);

/*starts new child group. If n is NULL, uses previous child group node for node (this is for text only)*/
void group_start_child(GroupingNode *group, GF_Node *n);
void group_end_child(GroupingNode *group, GF_BBox *bounds);
void group_end_text_child(GroupingNode *group, GF_Rect *bounds, Fixed ascent, Fixed descent, u32 split_text_idx);
/*reset ChildGroup info - does NOT reset real children*/
void group_reset_children(GroupingNode *group);


/*update clipers and matrices in regular mode (translations only)*/
void child_render_done(ChildGroup *cg, RenderEffect3D *eff);
/*update clipers and matrices in complex mode (free-form transforms) - if @mat is NULL group isn't rendered*/
void child_render_done_complex(ChildGroup *cg, RenderEffect3D *eff, GF_Matrix2D *mat);

/*activates all sensors in a group*/
void grouping_activate_sensors(GF_Node *group, Bool mouse_over, Bool is_active, SFVec3f *hit_point);

#endif

