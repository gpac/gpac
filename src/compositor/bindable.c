/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Compositor sub-project
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

#include "nodes_stacks.h"

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)

GF_List *Bindable_GetStack(GF_Node *bindable)
{
	void *st;
	if (!bindable) return 0;
	st = gf_node_get_private(bindable);
	switch (gf_node_get_tag(bindable)) {
	case TAG_MPEG4_Background2D:
		return ((Background2DStack*)st)->reg_stacks;
	case TAG_MPEG4_Viewport:
	case TAG_MPEG4_NavigationInfo:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_NavigationInfo:
#endif
		return ((ViewStack*)st)->reg_stacks;
#ifndef GPAC_DISABLE_3D
	case TAG_MPEG4_Background:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Background:
#endif
		return ((BackgroundStack*)st)->reg_stacks;
	case TAG_MPEG4_Viewpoint:
	case TAG_MPEG4_Fog:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Viewpoint:
	case TAG_X3D_Fog:
#endif
		return ((ViewStack*)st)->reg_stacks;
#endif
	default:
		return NULL;
	}
}

Bool Bindable_GetIsBound(GF_Node *bindable)
{
	if (!bindable) return GF_FALSE;
	switch (gf_node_get_tag(bindable)) {
	case TAG_MPEG4_Background2D:
		return ((M_Background2D*)bindable)->isBound;
	case TAG_MPEG4_Viewport:
		return ((M_Viewport*)bindable)->isBound;
	case TAG_MPEG4_Background:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Background:
#endif
		return ((M_Background*)bindable)->isBound;
	case TAG_MPEG4_NavigationInfo:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_NavigationInfo:
#endif
		return ((M_NavigationInfo*)bindable)->isBound;
	case TAG_MPEG4_Viewpoint:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Viewpoint:
#endif
		return ((M_Viewpoint*)bindable)->isBound;
	case TAG_MPEG4_Fog:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Fog:
#endif
		return ((M_Fog*)bindable)->isBound;
	default:
		return GF_FALSE;
	}
}

void Bindable_SetIsBound(GF_Node *bindable, Bool val)
{
	Bool has_bind_time = GF_FALSE;
	if (!bindable) return;
	switch (gf_node_get_tag(bindable)) {
	case TAG_MPEG4_Background2D:
		if ( ((M_Background2D*)bindable)->isBound == val) return;
		((M_Background2D*)bindable)->isBound = val;
		break;
	case TAG_MPEG4_Viewport:
		if ( ((M_Viewport*)bindable)->isBound == val) return;
		((M_Viewport*)bindable)->isBound = val;
		((M_Viewport*)bindable)->bindTime = gf_node_get_scene_time(bindable);
		has_bind_time = GF_TRUE;
		break;
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Background:
		if ( ((X_Background*)bindable)->isBound == val) return;
		((X_Background*)bindable)->isBound = val;
		((X_Background*)bindable)->bindTime = gf_node_get_scene_time(bindable);
		has_bind_time = GF_TRUE;
		break;
#endif
	case TAG_MPEG4_Background:
		if ( ((M_Background*)bindable)->isBound == val) return;
		((M_Background*)bindable)->isBound = val;
		break;
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_NavigationInfo:
		if ( ((X_NavigationInfo*)bindable)->isBound == val) return;
		((X_NavigationInfo*)bindable)->isBound = val;
		((X_NavigationInfo*)bindable)->bindTime = gf_node_get_scene_time(bindable);
		has_bind_time = GF_TRUE;
		break;
#endif
	case TAG_MPEG4_NavigationInfo:
		if ( ((M_NavigationInfo*)bindable)->isBound == val) return;
		((M_NavigationInfo*)bindable)->isBound = val;
		break;
	case TAG_MPEG4_Viewpoint:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Viewpoint:
#endif
		if ( ((M_Viewpoint*)bindable)->isBound == val) return;
		((M_Viewpoint*)bindable)->isBound = val;
		((M_Viewpoint*)bindable)->bindTime = gf_node_get_scene_time(bindable);
		has_bind_time = GF_TRUE;
		break;
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Fog:
		if ( ((X_Fog*)bindable)->isBound == val) return;
		((X_Fog*)bindable)->isBound = val;
		((X_Fog*)bindable)->bindTime = gf_node_get_scene_time(bindable);
		has_bind_time = GF_TRUE;
		break;
#endif
	case TAG_MPEG4_Fog:
		if ( ((M_Fog*)bindable)->isBound == val) return;
		((M_Fog*)bindable)->isBound = val;
		break;
	default:
		return;
	}
	gf_node_event_out_str(bindable, "isBound");
	if (has_bind_time) gf_node_event_out_str(bindable, "bindTime");
	/*force invalidate of the bindable stack's owner*/
	gf_node_dirty_set(bindable, 0, GF_TRUE);
}


Bool Bindable_GetSetBind(GF_Node *bindable)
{
	if (!bindable) return GF_FALSE;
	switch (gf_node_get_tag(bindable)) {
	case TAG_MPEG4_Background2D:
		return ((M_Background2D*)bindable)->set_bind;
	case TAG_MPEG4_Viewport:
		return ((M_Viewport*)bindable)->set_bind;
	case TAG_MPEG4_Background:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Background:
#endif
		return ((M_Background*)bindable)->set_bind;
	case TAG_MPEG4_NavigationInfo:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_NavigationInfo:
#endif
		return ((M_NavigationInfo*)bindable)->set_bind;
	case TAG_MPEG4_Viewpoint:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Viewpoint:
#endif
		return ((M_Viewpoint*)bindable)->set_bind;
	case TAG_MPEG4_Fog:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Fog:
#endif
		return ((M_Fog*)bindable)->set_bind;
	default:
		return GF_FALSE;
	}
}

void Bindable_SetSetBindEx(GF_Node *bindable, Bool val, GF_List *stack)
{
	if (!bindable) return;
	switch (gf_node_get_tag(bindable)) {
	case TAG_MPEG4_Background2D:
		((M_Background2D*)bindable)->set_bind = val;
		((M_Background2D*)bindable)->on_set_bind(bindable, NULL);
		break;
	case TAG_MPEG4_Viewport:
		((M_Viewport*)bindable)->set_bind = val;
		((M_Viewport*)bindable)->on_set_bind(bindable, (GF_Route*)stack);
		break;
	case TAG_MPEG4_Background:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Background:
#endif
		((M_Background*)bindable)->set_bind = val;
		((M_Background*)bindable)->on_set_bind(bindable, NULL);
		break;
	case TAG_MPEG4_NavigationInfo:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_NavigationInfo:
#endif
		((M_NavigationInfo*)bindable)->set_bind = val;
		((M_NavigationInfo*)bindable)->on_set_bind(bindable, NULL);
		break;
	case TAG_MPEG4_Viewpoint:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Viewpoint:
#endif
		((M_Viewpoint*)bindable)->set_bind = val;
		((M_Viewpoint*)bindable)->on_set_bind(bindable, NULL);
		break;
	case TAG_MPEG4_Fog:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Fog:
#endif
		((M_Fog*)bindable)->set_bind = val;
		((M_Fog*)bindable)->on_set_bind(bindable, NULL);
		break;
	default:
		return;
	}
}
void Bindable_SetSetBind(GF_Node *bindable, Bool val)
{
	Bindable_SetSetBindEx(bindable, val, NULL);
}

void Bindable_OnSetBind(GF_Node *bindable, GF_List *stack_list, GF_List *for_stack)
{
	u32 i;
	Bool on_top, is_bound, set_bind;
	GF_Node *node;
	GF_List *stack;

	set_bind = Bindable_GetSetBind(bindable);
	is_bound = Bindable_GetIsBound(bindable);

	if (!set_bind && !is_bound) return;
	if (set_bind && is_bound) return;

	i=0;
	while ((stack = (GF_List*)gf_list_enum(stack_list, &i))) {
		if (for_stack && (for_stack!=stack)) continue;

		on_top = (gf_list_get(stack, 0)==bindable) ? GF_TRUE : GF_FALSE;

		if (!set_bind) {
			if (is_bound) Bindable_SetIsBound(bindable, GF_FALSE);
			if (on_top && (gf_list_count(stack)>1)) {
				gf_list_rem(stack, 0);
				gf_list_add(stack, bindable);
				node = (GF_Node*)gf_list_get(stack, 0);
				Bindable_SetIsBound(node, GF_TRUE);
			}
		} else {
			if (!is_bound) Bindable_SetIsBound(bindable, GF_TRUE);
			if (!on_top) {
				/*push old top one down and unbind*/
				node = (GF_Node*)gf_list_get(stack, 0);
				Bindable_SetIsBound(node, GF_FALSE);
				/*insert new top*/
				gf_list_del_item(stack, bindable);
				gf_list_insert(stack, bindable, 0);
			}
		}
	}
	/*force invalidate of the bindable stack's owner*/
	gf_node_dirty_set(bindable, 0, GF_TRUE);
	/*and redraw scene*/
	gf_sc_invalidate(gf_sc_get_compositor(bindable), NULL);
}

void BindableStackDelete(GF_List *stack)
{
	while (gf_list_count(stack)) {
		GF_List *bind_stack_list;
		GF_Node *bindable = (GF_Node*)gf_list_get(stack, 0);
		gf_list_rem(stack, 0);
		bind_stack_list = Bindable_GetStack(bindable);
		if (bind_stack_list) {
			gf_list_del_item(bind_stack_list, stack);
			assert(gf_list_find(bind_stack_list, stack)<0);
		}
	}
	gf_list_del(stack);
}

void PreDestroyBindable(GF_Node *bindable, GF_List *stack_list)
{
	Bool is_bound = Bindable_GetIsBound(bindable);
	Bindable_SetIsBound(bindable, GF_FALSE);

	while (gf_list_count(stack_list)) {
		GF_List *stack = (GF_List*)gf_list_get(stack_list, 0);
		gf_list_rem(stack_list, 0);
		gf_list_del_item(stack, bindable);
		if (is_bound) {
			GF_Node *stack_top = (GF_Node*)gf_list_get(stack, 0);
			if (stack_top) Bindable_SetSetBind(stack_top, GF_TRUE);
		}
	}
}

#endif /*GPAC_DISABLE_VRML && GPAC_DISABLE_COMPOSITOR*/
