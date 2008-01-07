/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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

#include "visual_manager.h"
#include "nodes_stacks.h"
#include <gpac/options.h>

static void gf_sc_reset_collide_cursor(GF_Compositor *compositor)
{
	if (compositor->sensor_type == GF_CURSOR_COLLIDE) {
		GF_Event evt;
		compositor->sensor_type = evt.cursor.cursor_type = GF_CURSOR_NORMAL;
		evt.type = GF_EVENT_SET_CURSOR;
		compositor->video_out->ProcessEvent(compositor->video_out, &evt);
	}
}


static Bool exec_event_dom(GF_Compositor *compositor, GF_Event *event)
{
#ifndef GPAC_DISABLE_SVG
	GF_DOM_Event evt;
	u32 cursor_type;
	Bool ret = 0;

	cursor_type = GF_CURSOR_NORMAL;
	/*all mouse events*/
	if (event->type<=GF_EVENT_MOUSEMOVE) {
		Fixed X = compositor->hit_world_point.x;
		Fixed Y = compositor->hit_world_point.y;
		if (compositor->hit_node) {
			cursor_type = compositor->sensor_type;
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.clientX = evt.screenX = FIX2INT(X);
			evt.clientY = evt.screenY = FIX2INT(Y);
			evt.bubbles = 1;
			evt.cancelable = 1;
			evt.key_flags = compositor->key_states;

			switch (event->type) {
			case GF_EVENT_MOUSEMOVE:
				evt.cancelable = 0;
				if ((compositor->grab_node != compositor->hit_node) || (compositor->grab_use != compositor->hit_use) ) {
					/*mouse out*/
					if (compositor->grab_node) {
						evt.relatedTarget = compositor->hit_node;
						evt.type = GF_EVENT_MOUSEOUT;
						ret += gf_dom_event_fire(compositor->grab_node, compositor->grab_use, &evt);
						/*prepare mouseOver*/
						evt.relatedTarget = compositor->grab_node;
					}
					compositor->grab_node = compositor->hit_node;
					compositor->grab_use = compositor->hit_use;

					/*mouse over*/
					evt.type = GF_EVENT_MOUSEOVER;
					ret += gf_dom_event_fire(compositor->grab_node, compositor->grab_use, &evt);

				} else {
					evt.type = GF_EVENT_MOUSEMOVE;
					ret += gf_dom_event_fire(compositor->grab_node, compositor->grab_use, &evt);
				}
				compositor->num_clicks = 0;
				break;
			case GF_EVENT_MOUSEDOWN:
				if ((compositor->grab_x != X) || (compositor->grab_y != Y)) compositor->num_clicks = 0;
				evt.type = GF_EVENT_MOUSEDOWN;
				evt.detail = event->mouse.button;
				ret += gf_dom_event_fire(compositor->grab_node, compositor->grab_use, &evt);
				compositor->grab_x = X;
				compositor->grab_y = Y;
				break;
			case GF_EVENT_MOUSEUP:
				evt.type = GF_EVENT_MOUSEUP;
				evt.detail = event->mouse.button;
				ret += gf_dom_event_fire(compositor->grab_node, compositor->grab_use, &evt);
				if ((compositor->grab_x == X) && (compositor->grab_y == Y)) {
					compositor->num_clicks ++;
					evt.type = GF_EVENT_CLICK;
					evt.detail = compositor->num_clicks;
					ret += gf_dom_event_fire(compositor->grab_node, compositor->grab_use, &evt);
				}
				break;
			}
			cursor_type = evt.has_ui_events ? GF_CURSOR_TOUCH : GF_CURSOR_NORMAL;
		} else {
			/*mouse out*/
			if (compositor->grab_node) {
				memset(&evt, 0, sizeof(GF_DOM_Event));
				evt.clientX = evt.screenX = FIX2INT(X);
				evt.clientY = evt.screenY = FIX2INT(Y);
				evt.bubbles = 1;
				evt.cancelable = 1;
				evt.key_flags = compositor->key_states;
				evt.type = GF_EVENT_MOUSEOUT;
				ret += gf_dom_event_fire(compositor->grab_node, compositor->grab_use, &evt);
			}
			compositor->grab_node = NULL;
			compositor->grab_use = NULL;

			/*dispatch event to root SVG*/
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.clientX = evt.screenX = FIX2INT(X);
			evt.clientY = evt.screenY = FIX2INT(Y);
			evt.bubbles = 1;
			evt.cancelable = 1;
			evt.key_flags = compositor->key_states;
			evt.type = event->type;
			ret += gf_dom_event_fire(gf_sg_get_root_node(compositor->scene), NULL, &evt);
		}
		if (compositor->sensor_type != cursor_type) {
			GF_Event c_evt; 
			c_evt.type = GF_EVENT_SET_CURSOR; 
			c_evt.cursor.cursor_type = cursor_type;
			compositor->video_out->ProcessEvent(compositor->video_out, &c_evt);
			compositor->sensor_type = cursor_type;
		}
	}
	else if (event->type==GF_EVENT_TEXTINPUT) {
	} 
	else if ((event->type>=GF_EVENT_KEYUP) && (event->type<=GF_EVENT_LONGKEYPRESS)) {
		GF_Node *target;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.key_flags = event->key.flags;
		evt.bubbles = 1;
		evt.cancelable = 1;
		evt.type = event->type;
		evt.detail = event->key.key_code;
		evt.key_hw_code = event->key.hw_code;
		target = compositor->focus_node;
		if (!target) target = gf_sg_get_root_node(compositor->scene);
		ret += gf_dom_event_fire(target, NULL, &evt);

		if (event->type==GF_EVENT_KEYDOWN) {
			switch (event->key.key_code) {
			case GF_KEY_ENTER:
				evt.type = GF_EVENT_ACTIVATE;
				evt.detail = 0;
				ret += gf_dom_event_fire(target, NULL, &evt);
				break;
			case GF_KEY_TAB:
				ret += gf_sc_svg_focus_switch_ring(compositor, (event->key.flags & GF_KEY_MOD_SHIFT) ? 1 : 0);
				break;
			case GF_KEY_UP:
			case GF_KEY_DOWN:
			case GF_KEY_LEFT:
			case GF_KEY_RIGHT:
				ret += gf_sc_svg_focus_navigate(compositor, event->key.key_code);
				break;
			}
		} 

/*		else if (event->event_type==GF_EVENT_CHAR) {
		} else {
			evt.type = ((event->event_type==GF_EVENT_VKEYDOWN) || (event->event_type==GF_EVENT_KEYDOWN)) ? GF_EVENT_KEYDOWN : GF_EVENT_KEYUP;
			evt.detail = event->key.virtual_code;
		}
*/
	}
	return ret;
#else
	return 0;
#endif
}

static Bool exec_event_vrml(GF_Compositor *compositor, GF_Event *ev)
{
	GF_SensorHandler *hs, *hs_grabbed;
	GF_List *tmp;
	u32 i, count, stype;

	/*composite texture*/
	if (!gf_list_count(compositor->sensors) && compositor->hit_appear) 
		return compositor_compositetexture_handle_event(compositor, ev);

	hs_grabbed = NULL;
	stype = GF_CURSOR_NORMAL;
	count = gf_list_count(compositor->previous_sensors);
	for (i=0; i<count; i++) {
		hs = (GF_SensorHandler*)gf_list_get(compositor->previous_sensors, i);
		if (gf_list_find(compositor->sensors, hs) < 0) {
			/*that's a bit ugly but we need coords if pointer out of the shape when sensor grabbed...*/
			if (compositor->grabbed_sensor) {
				hs_grabbed = hs;
			} else {
				hs->OnUserEvent(hs, 0, ev, compositor);
			}
		}
	}

	count = gf_list_count(compositor->sensors);
	for (i=0; i<count; i++) {
		hs = (GF_SensorHandler*)gf_list_get(compositor->sensors, i);
		hs->OnUserEvent(hs, 1, ev, compositor);
		stype = gf_node_get_tag(hs->sensor);
		if (hs==hs_grabbed) hs_grabbed = NULL;
	}
	/*switch sensors*/
	tmp = compositor->sensors;
	compositor->sensors = compositor->previous_sensors;
	compositor->previous_sensors = tmp;

	/*check if we have a grabbed sensor*/
	if (hs_grabbed) {
		hs_grabbed->OnUserEvent(hs_grabbed, 0, ev, compositor);
		gf_list_reset(compositor->previous_sensors);
		gf_list_add(compositor->previous_sensors, hs_grabbed);
		stype = gf_node_get_tag(hs_grabbed->sensor);
	}

	/*and set cursor*/
	if (compositor->sensor_type != GF_CURSOR_COLLIDE) {
		switch (stype) {
		case TAG_MPEG4_Anchor: 
		case TAG_X3D_Anchor: 
			stype = GF_CURSOR_ANCHOR; 
			break;
		case TAG_MPEG4_PlaneSensor2D:
		case TAG_MPEG4_PlaneSensor: 
		case TAG_X3D_PlaneSensor: 
			stype = GF_CURSOR_PLANE; break;
		case TAG_MPEG4_CylinderSensor: 
		case TAG_X3D_CylinderSensor: 
		case TAG_MPEG4_DiscSensor:
		case TAG_MPEG4_SphereSensor:
		case TAG_X3D_SphereSensor:
			stype = GF_CURSOR_ROTATE; break;
		case TAG_MPEG4_ProximitySensor2D:
		case TAG_MPEG4_ProximitySensor:
		case TAG_X3D_ProximitySensor:
			stype = GF_CURSOR_PROXIMITY; break;
		case TAG_MPEG4_TouchSensor: 
		case TAG_X3D_TouchSensor: 
			stype = GF_CURSOR_TOUCH; break;
		default: stype = GF_CURSOR_NORMAL; break;
		}
		if ((stype != GF_CURSOR_NORMAL) || (compositor->sensor_type != stype)) {
			GF_Event evt;
			evt.type = GF_EVENT_SET_CURSOR;
			evt.cursor.cursor_type = stype;
			compositor->video_out->ProcessEvent(compositor->video_out, &evt);
			compositor->sensor_type = stype;
		}
	} else {
		gf_sc_reset_collide_cursor(compositor);
	}
	return count ? 1 : 0;
}


Bool visual_execute_event(GF_VisualManager *visual, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children)
{
	Bool ret;
	tr_state->traversing_mode = TRAVERSE_PICK;
#ifndef GPAC_DISABLE_3D
	tr_state->layer3d = NULL;
#endif
	visual->compositor->hit_appear = NULL;
	
	/*pick node*/
#ifndef GPAC_DISABLE_3D
	if (visual->type_3d)
		visual_3d_pick_node(visual, tr_state, ev, children);
	else 
#endif
		visual_2d_pick_node(visual, tr_state, ev, children);
	
	gf_list_reset(tr_state->vrml_sensors);

	if (visual->compositor->hit_use_dom_events) {
		ret = exec_event_dom(visual->compositor, ev);
		if (ret) return 1;
		/*no vrml sensors above*/
		if (!gf_list_count(visual->compositor->sensors)) return 0;
	}
	return exec_event_vrml(visual->compositor, ev);
}

Bool gf_sc_execute_event(GF_Compositor *compositor, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children)
{
	/*only process mouse events for MPEG-4/VRML*/
	if (ev->type>=GF_EVENT_MOUSEWHEEL) {
		/*FIXME - this is not working for mixed docs*/
		if (compositor->root_uses_dom_events) 
			return exec_event_dom(compositor, ev);
		return 0;
	} 

	/*pick even, call visual handler*/
	return visual_execute_event(compositor->visual, tr_state, ev, children);
}

Bool gf_sc_exec_event(GF_Compositor *compositor, GF_Event *evt)
{
	if (evt->type<=GF_EVENT_MOUSEMOVE) {
		if (compositor->visual->center_coords) {
			evt->mouse.x = evt->mouse.x - compositor->display_width/2;
			evt->mouse.y = compositor->display_height/2 - evt->mouse.y;
		} else {
			evt->mouse.x -= compositor->vp_x;
			evt->mouse.y -= compositor->vp_y;
		}
	}

	/*process regular events except if navigation is grabbed*/
	if (!compositor->navigation_grabbed && (compositor->interaction_level & GF_INTERACT_NORMAL) && gf_sc_execute_event(compositor, compositor->traverse_state, evt, NULL)) 
		return 1;
#ifndef GPAC_DISABLE_3D
	/*remember active layer on mouse click - may be NULL*/
	if ((evt->type==GF_EVENT_MOUSEDOWN) && (evt->mouse.button==GF_MOUSE_LEFT)) compositor->active_layer = compositor->traverse_state->layer3d;
#endif
	/*process navigation events*/
	if (compositor->interaction_level & GF_INTERACT_NAVIGATION) return compositor_handle_navigation(compositor, evt);
	return 0;
}
