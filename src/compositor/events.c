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
#include <gpac/utf.h>

static void gf_sc_reset_collide_cursor(GF_Compositor *compositor)
{
	if (compositor->sensor_type == GF_CURSOR_COLLIDE) {
		GF_Event evt;
		compositor->sensor_type = evt.cursor.cursor_type = GF_CURSOR_NORMAL;
		evt.type = GF_EVENT_SET_CURSOR;
		compositor->video_out->ProcessEvent(compositor->video_out, &evt);
	}
}


static Bool exec_text_selection(GF_Compositor *compositor, GF_Event *event)
{
	if (event->type>GF_EVENT_MOUSEMOVE) return 0;

	if (compositor->edited_text)
		return 0;
	if (compositor->text_selection)
		return 1;
	switch (event->type) {
	case GF_EVENT_MOUSEMOVE:
		if (compositor->text_selection)
			return 1;
		break;
	case GF_EVENT_MOUSEDOWN:
		if (compositor->hit_text)
			compositor->text_selection = compositor->hit_text;
		break;
	}
	return 0;
}

static void flush_text_node_edit(GF_Compositor *compositor, Bool final_flush)
{
	u8 *txt;
	u16 *lptr;
	u32 len;
	if (!compositor->edited_text) return;

	if (final_flush && compositor->sel_buffer_len) {
		memmove(&compositor->sel_buffer[compositor->caret_pos], &compositor->sel_buffer[compositor->caret_pos+1], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos));
		compositor->sel_buffer_len--;
		compositor->sel_buffer[compositor->sel_buffer_len] = 0;
	}

	if (compositor->edited_text->textContent) free(compositor->edited_text->textContent);
	if (!compositor->sel_buffer_len) {
		compositor->edited_text->textContent= NULL;
	} else {
		txt = malloc(sizeof(char)*2*compositor->sel_buffer_len);
		lptr = compositor->sel_buffer;
		len = gf_utf8_wcstombs(txt, 2*compositor->sel_buffer_len, &lptr);
		txt[len] = 0;
		compositor->edited_text->textContent = strdup(txt);
		free(txt);
	}
	gf_node_dirty_set(compositor->focus_node, 0, (compositor->focus_text_type==2));
	gf_node_set_private(compositor->focus_highlight->node, NULL);
	compositor->draw_next_frame = 1;

	if (final_flush) {
		if (compositor->sel_buffer) free(compositor->sel_buffer);
		compositor->sel_buffer = NULL;
		compositor->sel_buffer_len = compositor->sel_buffer_alloc = 0;
		compositor->edited_text = NULL;
	}
}

static void load_text_node(GF_Compositor *compositor, s32 node_idx_inc)
{
	GF_DOMText *res = NULL;
	u32 prev_pos, pos;
	GF_ChildNodeItem *child;

	if ((s32)compositor->dom_text_pos + node_idx_inc < 0) return;
	pos = compositor->dom_text_pos + node_idx_inc;
	prev_pos = compositor->dom_text_pos;

	child = ((GF_ParentNode *) compositor->focus_node)->children;

	compositor->dom_text_pos = 0;
	while (child) {
		switch (gf_node_get_tag(child->node)) {
		case TAG_DOMText:
			break;
		default:
			child = child->next;
			continue;
		}
		compositor->dom_text_pos++;
		if (!node_idx_inc) res = (GF_DOMText *)child->node;
		else if (pos==compositor->dom_text_pos) {
			res = (GF_DOMText *)child->node;
			break;
		}
		child = child->next;
		continue;
	}
	if (!res) {
		compositor->dom_text_pos = prev_pos;
		return;
	}

	if (compositor->edited_text) {
		flush_text_node_edit(compositor, 1);
	}

	if (res->textContent) {
		u8 *src = res->textContent;
		compositor->sel_buffer_alloc = 2+strlen(src);
		compositor->sel_buffer = malloc(sizeof(u16)*compositor->sel_buffer_alloc);

		if (node_idx_inc<=0) {
			compositor->sel_buffer_len = gf_utf8_mbstowcs(compositor->sel_buffer, compositor->sel_buffer_alloc, &src);
			compositor->sel_buffer[compositor->sel_buffer_len]='|';
			compositor->caret_pos = compositor->sel_buffer_len;
		} else {
			compositor->sel_buffer_len = gf_utf8_mbstowcs(&compositor->sel_buffer[1], compositor->sel_buffer_alloc, &src);
			compositor->sel_buffer[0]='|';
			compositor->caret_pos = 0;
		}
		compositor->sel_buffer_len++;
		compositor->sel_buffer[compositor->sel_buffer_len]=0;
	} else {
		compositor->sel_buffer_alloc = 2;
		compositor->sel_buffer = malloc(sizeof(u16)*2);
		compositor->sel_buffer[0] = '|';
		compositor->sel_buffer[1] = 0;
		compositor->caret_pos = 0;
		compositor->sel_buffer_len = 1;
	}
	compositor->edited_text = res;
	flush_text_node_edit(compositor, 0);
}

static void exec_text_input(GF_Compositor *compositor, GF_Event *event)
{
	Bool is_end = 0;

	if (!event) {
		load_text_node(compositor, 0);
		return;
	} else if (event->type==GF_EVENT_TEXTINPUT) {
		switch (event->character.unicode_char) {
		case '\r':
		case '\n':
		case '\t':
		case '\b':
			return;
		default:
			if (compositor->sel_buffer_len + 1 == compositor->sel_buffer_alloc) {
				compositor->sel_buffer_alloc += 10;
				compositor->sel_buffer = realloc(compositor->sel_buffer, sizeof(u16)*compositor->sel_buffer_alloc);
			}
			memmove(&compositor->sel_buffer[compositor->caret_pos+1], &compositor->sel_buffer[compositor->caret_pos], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos));
			compositor->sel_buffer[compositor->caret_pos] = event->character.unicode_char;
			compositor->sel_buffer_len++;
			compositor->caret_pos++;
			compositor->sel_buffer[compositor->sel_buffer_len] = 0;
			break;
		}
	} else if (event->type==GF_EVENT_KEYDOWN) {
		u32 prev_caret = compositor->caret_pos;
		switch (event->key.key_code) {
		/*end of edit mode*/
		case GF_KEY_ENTER:
		case GF_KEY_ESCAPE:
			is_end = 1;
			break;
		case GF_KEY_LEFT:
			if (compositor->caret_pos) {
				if (event->key.flags & GF_KEY_MOD_CTRL) {
					while (compositor->caret_pos && (compositor->sel_buffer[compositor->caret_pos] != ' '))
						compositor->caret_pos--;
				} else {
					compositor->caret_pos--;
				}
			}
			else {
				load_text_node(compositor, -1);
				return;
			}
			break;
		case GF_KEY_RIGHT:
			if (compositor->caret_pos+1<compositor->sel_buffer_len) {
				if (event->key.flags & GF_KEY_MOD_CTRL) {
					while ((compositor->caret_pos+1<compositor->sel_buffer_len)
						&& (compositor->sel_buffer[compositor->caret_pos] != ' '))
						compositor->caret_pos++;
				} else {
					compositor->caret_pos++;
				}
			}
			else {
				load_text_node(compositor, +1);
				return;
			}
			break;
		case GF_KEY_HOME:
			compositor->caret_pos = 0;
			break;
		case GF_KEY_END:
			compositor->caret_pos = compositor->sel_buffer_len-1;
			break;

		case GF_KEY_TAB:
			load_text_node(compositor, (event->key.flags & GF_KEY_MOD_SHIFT) ? -1 : 1);
			return;
		case GF_KEY_BACKSPACE:
			if (compositor->caret_pos) {
				if (compositor->sel_buffer_len) compositor->sel_buffer_len--;
				memmove(&compositor->sel_buffer[compositor->caret_pos-1], &compositor->sel_buffer[compositor->caret_pos], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos+1));
				compositor->sel_buffer[compositor->sel_buffer_len]=0;
				compositor->caret_pos--;
				flush_text_node_edit(compositor, 0);
				return;
			}
			break;
		case GF_KEY_DEL:
			if (compositor->caret_pos+1<compositor->sel_buffer_len) {
				if (compositor->sel_buffer_len) compositor->sel_buffer_len--;
				memmove(&compositor->sel_buffer[compositor->caret_pos+1], &compositor->sel_buffer[compositor->caret_pos+2], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos-1));
				compositor->sel_buffer[compositor->sel_buffer_len]=0;
				flush_text_node_edit(compositor, 0);
				return;
			}
			break;
		}
		if (!is_end) {
			if (compositor->caret_pos==prev_caret) return;
			memmove(&compositor->sel_buffer[prev_caret], &compositor->sel_buffer[prev_caret+1], sizeof(u16)*(compositor->sel_buffer_len-prev_caret));
			memmove(&compositor->sel_buffer[compositor->caret_pos+1], &compositor->sel_buffer[compositor->caret_pos], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos));
			compositor->sel_buffer[compositor->caret_pos]='|';
		}
	}

	flush_text_node_edit(compositor, is_end);
}

static Bool hit_node_editable(GF_Compositor *compositor) 
{
#ifndef GPAC_DISABLE_SVG
	SVGAllAttributes atts;
	u32 tag;
	/*no hit or not a text*/
	if (!compositor->hit_text || !compositor->hit_node) return 0;
	if (compositor->hit_node==compositor->focus_node) return compositor->focus_text_type ? 1 : 0;

	tag = gf_node_get_tag(compositor->hit_node);
	if (tag <= GF_NODE_FIRST_DOM_NODE_TAG) return 0;
	gf_svg_flatten_attributes((SVG_Element *)compositor->hit_node, &atts);
	if (!atts.editable || !*atts.editable) return 0;
	switch (tag) {
	case TAG_SVG_text:
	case TAG_SVG_textArea:
		compositor->focus_text_type = 1;
		compositor->focus_node = compositor->hit_node;
		return 1;
	case TAG_SVG_tspan:
		compositor->focus_text_type = 2;
		compositor->focus_node = compositor->hit_node;
		return 1;
	default:
		break;
	}
#endif
	return 0;
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
			GF_Node *current_use = gf_list_last(compositor->hit_use_stack);
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
				if ((compositor->grab_node != compositor->hit_node) || (compositor->grab_use != current_use) ) {
					/*mouse out*/
					if (compositor->grab_node) {
						evt.relatedTarget = compositor->hit_node;
						evt.type = GF_EVENT_MOUSEOUT;
						ret += gf_dom_event_fire(compositor->grab_node, compositor->hit_use_stack, &evt);
						/*prepare mouseOver*/
						evt.relatedTarget = compositor->grab_node;
					}
					compositor->grab_node = compositor->hit_node;
					compositor->grab_use = current_use;

					/*mouse over*/
					evt.type = GF_EVENT_MOUSEOVER;
					ret += gf_dom_event_fire(compositor->grab_node, compositor->hit_use_stack, &evt);

				} else {
					evt.type = GF_EVENT_MOUSEMOVE;
					ret += gf_dom_event_fire(compositor->grab_node, compositor->hit_use_stack, &evt);
				}
				compositor->num_clicks = 0;
				break;
			case GF_EVENT_MOUSEDOWN:
				if (hit_node_editable(compositor)) {
					exec_text_input(compositor, NULL);
					return 1;
				}
				if ((compositor->grab_x != X) || (compositor->grab_y != Y)) compositor->num_clicks = 0;
				evt.type = GF_EVENT_MOUSEDOWN;
				evt.detail = event->mouse.button;
				ret += gf_dom_event_fire(compositor->grab_node, compositor->hit_use_stack, &evt);
				compositor->grab_x = X;
				compositor->grab_y = Y;
				break;
			case GF_EVENT_MOUSEUP:
				evt.type = GF_EVENT_MOUSEUP;
				evt.detail = event->mouse.button;
				ret += gf_dom_event_fire(compositor->grab_node, compositor->hit_use_stack, &evt);
				if ((compositor->grab_x == X) && (compositor->grab_y == Y)) {
					compositor->num_clicks ++;
					evt.type = GF_EVENT_CLICK;
					evt.detail = compositor->num_clicks;
					ret += gf_dom_event_fire(compositor->grab_node, compositor->hit_use_stack, &evt);
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
				ret += gf_dom_event_fire(compositor->grab_node, compositor->hit_use_stack, &evt);
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
		if (compositor->edited_text) {
			exec_text_input(compositor, event);
			return 1;
		}
	} 
	else if ((event->type>=GF_EVENT_KEYUP) && (event->type<=GF_EVENT_LONGKEYPRESS)) {

		if (compositor->edited_text) {
			exec_text_input(compositor, event);
			return 1;
		} else {
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
					if (compositor->focus_text_type) {
						exec_text_input(compositor, NULL);
						ret = 1;
					} else {
						evt.type = GF_EVENT_ACTIVATE;
						evt.detail = 0;
						ret += gf_dom_event_fire(target, NULL, &evt);
					}
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
		}
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
	Bool reset_sel = 0;
	GF_Compositor *compositor = visual->compositor;
	tr_state->traversing_mode = TRAVERSE_PICK;
#ifndef GPAC_DISABLE_3D
	tr_state->layer3d = NULL;
#endif
	
	compositor->hit_appear = NULL;
	compositor->hit_text = NULL;

	if (compositor->text_selection) {
		if ((ev->type==GF_EVENT_MOUSEUP) && (ev->mouse.button==GF_MOUSE_LEFT)) {
			if (!compositor->store_text_sel) compositor->store_text_sel = 1;
			else {
				reset_sel = 1;
			}
		}
		else if ((ev->type==GF_EVENT_MOUSEDOWN) && (ev->mouse.button==GF_MOUSE_LEFT)) {
			reset_sel = 1;
		}
	} else if (compositor->edited_text) {
		if ((ev->type==GF_EVENT_MOUSEDOWN) && (ev->mouse.button==GF_MOUSE_LEFT))
			reset_sel = 1;
	}
	if (reset_sel) {
		flush_text_node_edit(compositor, 1);

		compositor->store_text_sel = 0;
		compositor->text_selection = NULL;
		if (compositor->selected_text) free(compositor->selected_text);
		compositor->selected_text = NULL;
		if (compositor->sel_buffer) free(compositor->sel_buffer);
		compositor->sel_buffer = NULL;
		compositor->sel_buffer_alloc = 0;
		compositor->sel_buffer_len = 0;

		compositor->draw_next_frame = 1;
	}
	
	/*pick node*/
#ifndef GPAC_DISABLE_3D
	if (visual->type_3d)
		visual_3d_pick_node(visual, tr_state, ev, children);
	else 
#endif
		visual_2d_pick_node(visual, tr_state, ev, children);
	
	gf_list_reset(tr_state->vrml_sensors);

	if (exec_text_selection(compositor, ev))
		return 1;

	if (compositor->hit_use_dom_events) {
		ret = exec_event_dom(compositor, ev);
		if (ret) return 1;
		/*no vrml sensors above*/
		if (!gf_list_count(compositor->sensors)) return 0;
	}
	return exec_event_vrml(compositor, ev);
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
		}
	}

	/*process regular events except if navigation is grabbed*/
	if ( (compositor->navigation_state<2) && (compositor->interaction_level & GF_INTERACT_NORMAL) && gf_sc_execute_event(compositor, compositor->traverse_state, evt, NULL)) {
		compositor->navigation_state = 0;
		return 1;
	}
#ifndef GPAC_DISABLE_3D
	/*remember active layer on mouse click - may be NULL*/
	if ((evt->type==GF_EVENT_MOUSEDOWN) && (evt->mouse.button==GF_MOUSE_LEFT)) compositor->active_layer = compositor->traverse_state->layer3d;
#endif
	/*process navigation events*/
	if (compositor->interaction_level & GF_INTERACT_NAVIGATION) return compositor_handle_navigation(compositor, evt);
	return 0;
}
