/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D+3D rendering module
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

static void render_reset_collide_cursor(Render *sr)
{
	if (sr->sensor_type == GF_CURSOR_COLLIDE) {
		GF_Event evt;
		sr->sensor_type = evt.cursor.cursor_type = GF_CURSOR_NORMAL;
		evt.type = GF_EVENT_SET_CURSOR;
		sr->compositor->video_out->ProcessEvent(sr->compositor->video_out, &evt);
	}
}

Bool render_has_composite_texture(GF_Node *appear)
{
	u32 tag;
	if (!appear) return 0;
	tag = gf_node_get_tag(appear);
	if ((tag==TAG_MPEG4_Appearance) || (tag==TAG_X3D_Appearance)) {
		M_Appearance *ap = (M_Appearance *)appear;
		if (!ap->texture) return 0;
		switch (gf_node_get_tag(((M_Appearance *)appear)->texture)) {
		case TAG_MPEG4_CompositeTexture2D:
		case TAG_MPEG4_CompositeTexture3D:
			return 1;
		}
	}
	return 0;
}


static Bool render_exec_event_dom(Render *sr, GF_Event *event)
{
	GF_DOM_Event evt;
	u32 cursor_type;
	Bool ret = 0;

	cursor_type = GF_CURSOR_NORMAL;
	/*all mouse events*/
	if (event->type<=GF_EVENT_MOUSEMOVE) {
		Fixed X = sr->hit_info.world_point.x;
		Fixed Y = sr->hit_info.world_point.y;
		if (sr->hit_info.picked) {
			cursor_type = sr->sensor_type;
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.clientX = evt.screenX = FIX2INT(X);
			evt.clientY = evt.screenY = FIX2INT(Y);
			evt.bubbles = 1;
			evt.cancelable = 1;
			evt.key_flags = sr->compositor->key_states;

			switch (event->type) {
			case GF_EVENT_MOUSEMOVE:
				evt.cancelable = 0;
				if ((sr->grab_node != sr->hit_info.picked) || (sr->grab_use != sr->hit_info.use) ) {
					/*mouse out*/
					if (sr->grab_node) {
						evt.relatedTarget = sr->hit_info.picked;
						evt.type = GF_EVENT_MOUSEOUT;
						ret += gf_dom_event_fire(sr->grab_node, sr->grab_use, &evt);
						/*prepare mouseOver*/
						evt.relatedTarget = sr->grab_node;
					}
					sr->grab_node = sr->hit_info.picked;
					sr->grab_use = sr->hit_info.use;

					/*mouse over*/
					evt.type = GF_EVENT_MOUSEOVER;
					ret += gf_dom_event_fire(sr->grab_node, sr->grab_use, &evt);

				} else {
					evt.type = GF_EVENT_MOUSEMOVE;
					ret += gf_dom_event_fire(sr->grab_node, sr->grab_use, &evt);
				}
				sr->num_clicks = 0;
				break;
			case GF_EVENT_MOUSEDOWN:
				if ((sr->grab_x != X) || (sr->grab_y != Y)) sr->num_clicks = 0;
				evt.type = GF_EVENT_MOUSEDOWN;
				evt.detail = event->mouse.button;
				ret += gf_dom_event_fire(sr->grab_node, sr->grab_use, &evt);
				sr->grab_x = X;
				sr->grab_y = Y;
				break;
			case GF_EVENT_MOUSEUP:
				evt.type = GF_EVENT_MOUSEUP;
				evt.detail = event->mouse.button;
				ret += gf_dom_event_fire(sr->grab_node, sr->grab_use, &evt);
				if ((sr->grab_x == X) && (sr->grab_y == Y)) {
					sr->num_clicks ++;
					evt.type = GF_EVENT_CLICK;
					evt.detail = sr->num_clicks;
					ret += gf_dom_event_fire(sr->grab_node, sr->grab_use, &evt);
				}
				break;
			}
			cursor_type = evt.has_ui_events ? GF_CURSOR_TOUCH : GF_CURSOR_NORMAL;
		} else {
			/*mouse out*/
			if (sr->grab_node) {
				memset(&evt, 0, sizeof(GF_DOM_Event));
				evt.clientX = evt.screenX = FIX2INT(X);
				evt.clientY = evt.screenY = FIX2INT(Y);
				evt.bubbles = 1;
				evt.cancelable = 1;
				evt.key_flags = sr->compositor->key_states;
				evt.type = GF_EVENT_MOUSEOUT;
				ret += gf_dom_event_fire(sr->grab_node, sr->grab_use, &evt);
			}
			sr->grab_node = NULL;
			sr->grab_use = NULL;

			/*dispatch event to root SVG*/
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.clientX = evt.screenX = FIX2INT(X);
			evt.clientY = evt.screenY = FIX2INT(Y);
			evt.bubbles = 1;
			evt.cancelable = 1;
			evt.key_flags = sr->compositor->key_states;
			evt.type = event->type;
			ret += gf_dom_event_fire(gf_sg_get_root_node(sr->compositor->scene), NULL, &evt);
		}
		if (sr->sensor_type != cursor_type) {
			GF_Event c_evt; 
			c_evt.type = GF_EVENT_SET_CURSOR; 
			c_evt.cursor.cursor_type = cursor_type;
			sr->compositor->video_out->ProcessEvent(sr->compositor->video_out, &c_evt);
			sr->sensor_type = cursor_type;
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
		target = sr->focus_node;
		if (!target) target = gf_sg_get_root_node(sr->compositor->scene);
		ret += gf_dom_event_fire(target, NULL, &evt);

		if (event->type==GF_EVENT_KEYDOWN) {
			switch (event->key.key_code) {
			case GF_KEY_ENTER:
				evt.type = GF_EVENT_ACTIVATE;
				evt.detail = 0;
				ret += gf_dom_event_fire(target, NULL, &evt);
				break;
			case GF_KEY_TAB:
				ret += render_svg_focus_switch_ring(sr, (event->key.flags & GF_KEY_MOD_SHIFT) ? 1 : 0);
				break;
			case GF_KEY_UP:
			case GF_KEY_DOWN:
			case GF_KEY_LEFT:
			case GF_KEY_RIGHT:
				ret += render_svg_focus_navigate(sr, event->key.key_code);
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
}

static Bool render_exec_event_vrml(Render *sr, GF_Event *ev)
{
	SensorHandler *hs, *hs_grabbed;
	GF_List *tmp;
	u32 i, count, stype;

	/*composite texture*/
	if (!gf_list_count(sr->sensors) && sr->hit_info.appear) 
		return render_composite_texture_handle_event(sr, ev);

	hs_grabbed = NULL;
	stype = GF_CURSOR_NORMAL;
	count = gf_list_count(sr->previous_sensors);
	for (i=0; i<count; i++) {
		hs = (SensorHandler*)gf_list_get(sr->previous_sensors, i);
		if (gf_list_find(sr->sensors, hs) < 0) {
			/*that's a bit ugly but we need coords if pointer out of the shape when sensor grabbed...*/
			if (sr->grabbed_sensor) {
				hs_grabbed = hs;
			} else {
				hs->OnUserEvent(hs, 0, ev, sr);
			}
		}
	}

	count = gf_list_count(sr->sensors);
	for (i=0; i<count; i++) {
		hs = (SensorHandler*)gf_list_get(sr->sensors, i);
		hs->OnUserEvent(hs, 1, ev, sr);
		stype = gf_node_get_tag(hs->sensor);
		if (hs==hs_grabbed) hs_grabbed = NULL;
	}
	/*switch sensors*/
	tmp = sr->sensors;
	sr->sensors = sr->previous_sensors;
	sr->previous_sensors = tmp;

	/*check if we have a grabbed sensor*/
	if (hs_grabbed) {
		hs_grabbed->OnUserEvent(hs_grabbed, 0, ev, sr);
		gf_list_reset(sr->previous_sensors);
		gf_list_add(sr->previous_sensors, hs_grabbed);
		stype = gf_node_get_tag(hs_grabbed->sensor);
	}

	/*and set cursor*/
	if (sr->sensor_type != GF_CURSOR_COLLIDE) {
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
		if ((stype != GF_CURSOR_NORMAL) || (sr->sensor_type != stype)) {
			GF_Event evt;
			evt.type = GF_EVENT_SET_CURSOR;
			evt.cursor.cursor_type = stype;
			sr->compositor->video_out->ProcessEvent(sr->compositor->video_out, &evt);
			sr->sensor_type = stype;
		}
	} else {
		render_reset_collide_cursor(sr);
	}
	return count ? 1 : 0;
}


Bool visual_execute_event(GF_VisualManager *vis, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children)
{
	tr_state->traversing_mode = TRAVERSE_PICK;
#ifndef GPAC_DISABLE_3D
	tr_state->layer3d = NULL;
#endif
	vis->render->hit_info.appear = NULL;
	
	/*pick node*/
#ifndef GPAC_DISABLE_3D
	if (vis->type_3d)
		visual_3d_pick_node(vis, tr_state, ev, children);
	else 
#endif
		visual_2d_pick_node(vis, tr_state, ev, children);
	
	gf_list_reset(tr_state->sensors);

	if (vis->render->hit_info.use_dom_events) 
		return render_exec_event_dom(vis->render, ev);
	else
		return render_exec_event_vrml(vis->render, ev);
}

Bool render_execute_event(Render *sr, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children)
{
	/*only process mouse events for MPEG-4/VRML*/
	if (ev->type>=GF_EVENT_MOUSEWHEEL) {
		/*FIXME - this is not working for mixed docs*/
		if (sr->root_uses_dom_events) 
			return render_exec_event_dom(sr, ev);
		return 0;
	} 

	/*pick even, call visual handler*/
	return visual_execute_event(sr->visual, tr_state, ev, children);
}
