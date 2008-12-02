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
#include "mpeg4_grouping.h"
#include <gpac/options.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/utf.h>

static GF_Node *browse_parent_for_focus(GF_Compositor *compositor, GF_Node *elt, Bool prev_focus);
u32 gf_sc_focus_switch_ring_ex(GF_Compositor *compositor, Bool move_prev, GF_Node *focus, Bool force_focus);

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
		if (compositor->hit_text) {
			compositor->text_selection = compositor->hit_text;
			/*return 0: the event may be consumed by the tree*/
			return 0;
		}
		break;
	}
	return 0;
}

static void flush_text_node_edit(GF_Compositor *compositor, Bool final_flush)
{
	u8 *txt;
	u32 len;
	if (!compositor->edited_text) return;

	if (final_flush && compositor->sel_buffer_len) {
		memmove(&compositor->sel_buffer[compositor->caret_pos], &compositor->sel_buffer[compositor->caret_pos+1], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos));
		compositor->sel_buffer_len--;
		compositor->sel_buffer[compositor->sel_buffer_len] = 0;
	}

	if (*compositor->edited_text) {
		free(*compositor->edited_text);
		*compositor->edited_text = NULL;
	}
	if (compositor->sel_buffer_len) {
		const u16 *lptr;
		txt = malloc(sizeof(char)*2*compositor->sel_buffer_len);
		lptr = compositor->sel_buffer;
		len = gf_utf8_wcstombs(txt, 2*compositor->sel_buffer_len, &lptr);
		txt[len] = 0;
		*compositor->edited_text = strdup(txt);
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


GF_Err gf_sc_paste_text(GF_Compositor *compositor, const char *text)
{
	u16 *conv_buf;
	u32 len;
	if (!compositor->sel_buffer || !compositor->edited_text) return GF_BAD_PARAM;
	if (!text) return GF_OK;
	len = strlen(text);
	if (!len) return GF_OK;

	gf_sc_lock(compositor, 1);

	conv_buf = malloc(sizeof(u16)*len);
	len = gf_utf8_mbstowcs(conv_buf, len, &text);

	compositor->sel_buffer_alloc += len;
	if (compositor->sel_buffer_len == compositor->sel_buffer_alloc)
		compositor->sel_buffer_alloc++;

	compositor->sel_buffer = realloc(compositor->sel_buffer, sizeof(u16)*compositor->sel_buffer_alloc);
	memmove(&compositor->sel_buffer[compositor->caret_pos+len], &compositor->sel_buffer[compositor->caret_pos], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos));
	memcpy(&compositor->sel_buffer[compositor->caret_pos], conv_buf, sizeof(u16)*len);
	free(conv_buf);
	compositor->sel_buffer_len += len;
	compositor->caret_pos += len;
	compositor->sel_buffer[compositor->sel_buffer_len]=0;
	flush_text_node_edit(compositor, 0);
	gf_sc_lock(compositor, 0);

	return GF_OK;
}
static Bool load_text_node(GF_Compositor *compositor, u32 cmd_type)
{
	char **res = NULL;
	u32 prev_pos, pos=0;
	s32 caret_pos;
	Bool append;
	Bool delete_cr = 0;
	
	caret_pos = -1;

	append = 0;
	switch (cmd_type) {
	/*load last text chunk*/
	case 0:
		pos = 0;
		break;
	/*load prev text chunk*/
	case 4:
		delete_cr = 1;
	case 1:
		if (compositor->dom_text_pos ==0) return 0;
		pos = compositor->dom_text_pos - 1;
		break;
	/*load next text chunk*/
	case 2:
		pos = compositor->dom_text_pos + 1;
		caret_pos = 0;
		break;
	/*split text chunk/append new*/
	case 3:
		append=1;
		pos = compositor->dom_text_pos;
		caret_pos = 0;
		break;
	}
	prev_pos = compositor->dom_text_pos;

	compositor->dom_text_pos = 0;

	if (compositor->focus_text_type==3) {
		MFString *mf = &((M_Text*)compositor->focus_node)->string;

		if (append) {
			gf_sg_vrml_mf_append(mf, GF_SG_VRML_MFSTRING, (void **)res);
			compositor->dom_text_pos = mf->count;
		} else if (!cmd_type) {
			compositor->dom_text_pos = mf->count;
		} else if (pos <= mf->count) {
			compositor->dom_text_pos = pos;
		} else {
			compositor->dom_text_pos = prev_pos;
			return 0;
		}
		res = &mf->vals[compositor->dom_text_pos-1];
	} else {
#ifndef GPAC_DISABLE_SVG
		GF_ChildNodeItem *child = ((GF_ParentNode *) compositor->focus_node)->children;

		while (child) {
			switch (gf_node_get_tag(child->node)) {
			case TAG_DOMText:
				break;
			default:
				child = child->next;
				continue;
			}
			compositor->dom_text_pos++;
			if (!cmd_type) res = &((GF_DOMText *)child->node)->textContent;
			else if (pos==compositor->dom_text_pos) {
				if (append) {
					u16 end;
					u16 *srcp;
					u32 len;
					GF_DOMText *cur, *ntext;
					GF_ChildNodeItem *children = ((GF_ParentNode *) compositor->focus_node)->children;
					GF_Node *t = gf_node_new(gf_node_get_graph(child->node), TAG_SVG_tbreak);

					gf_node_init(t);
					gf_node_register(t, compositor->focus_node);

					pos = gf_node_list_find_child(children, child->node);

					/*we're only inserting a tbreak*/
					if (!compositor->caret_pos) {
						gf_node_list_insert_child(&children, t, pos);
						res = &((GF_DOMText *)child->node)->textContent;
					} else {
						gf_node_list_insert_child(&children, t, pos+1);
						ntext = (GF_DOMText*) gf_node_new(gf_node_get_graph(child->node), TAG_DOMText);
						gf_node_init(t);
						gf_node_list_insert_child(&children, (GF_Node *)ntext, pos+2);
						gf_node_register((GF_Node*)ntext, compositor->focus_node);

						cur = (GF_DOMText*) child->node;

						free(cur->textContent);
						end = compositor->sel_buffer[compositor->caret_pos];
						compositor->sel_buffer[compositor->caret_pos] = 0;
						len = gf_utf8_wcslen(compositor->sel_buffer);
						cur->textContent = malloc(sizeof(char)*(len+1));
						srcp = compositor->sel_buffer;
						len = gf_utf8_wcstombs(cur->textContent, len, &srcp);
						cur->textContent[len] = 0;
						compositor->sel_buffer[compositor->caret_pos] = end;

						if (compositor->caret_pos+1<compositor->sel_buffer_len) {
							len = gf_utf8_wcslen(compositor->sel_buffer + compositor->caret_pos + 1);
							ntext->textContent = malloc(sizeof(char)*(len+1));
							srcp = compositor->sel_buffer + compositor->caret_pos + 1;
							len = gf_utf8_wcstombs(ntext->textContent, len, &srcp);
							ntext->textContent[len] = 0;
						} else {
							ntext->textContent = strdup("");
						}
						res = &ntext->textContent;
						compositor->dom_text_pos ++;
						compositor->edited_text = NULL;
					}

				} else {
					if (delete_cr && child->next) {
						GF_Node *tbreak = child->next->node;
						GF_ChildNodeItem *children = ((GF_ParentNode *) compositor->focus_node)->children;
						gf_node_list_del_child(&children, tbreak);
						gf_node_unregister(tbreak, compositor->focus_node);
						if (child->next && (gf_node_get_tag(child->next->node)==TAG_DOMText) ) {
							GF_DOMText *n1 = (GF_DOMText *)child->node;
							GF_DOMText *n2 = (GF_DOMText *)child->next->node;

							if (compositor->edited_text) {
								flush_text_node_edit(compositor, 1);
							}
							caret_pos = strlen(n1->textContent);
							if (n2->textContent) {
								n1->textContent = realloc(n1->textContent, sizeof(char)*(strlen(n1->textContent)+strlen(n2->textContent)+1));
								strcat(n1->textContent, n2->textContent);
							}
							gf_node_list_del_child(&children, (GF_Node*)n2);
							gf_node_unregister((GF_Node*)n2, compositor->focus_node);
							compositor->edited_text = NULL;
						}
					}
					res = &((GF_DOMText *)child->node)->textContent;
				}
/*				if (1) {
					GF_ChildNodeItem *child = ((GF_ParentNode *) compositor->focus_node)->children;
					fprintf(stdout, "Dumping text tree:\n");
					while (child) {
						switch (gf_node_get_tag(child->node)) {
						case TAG_SVG_tbreak: fprintf(stdout, "\ttbreak\n"); break;
						case TAG_DOMText: fprintf(stdout, "\ttext: %s\n", ((GF_DOMText *)child->node)->textContent); break;
						}
						child = child->next;
					}
				}
*/
				break;
			}
			child = child->next;
			continue;
		}
		/*load of an empty text*/
		if (!res && !cmd_type) {
			GF_DOMText *t = gf_dom_add_text_node(compositor->focus_node, strdup(""));
			res = &t->textContent;
		}

		if (!res) {
			compositor->dom_text_pos = prev_pos;
			return 0;
		}
#endif
	}

	if (compositor->edited_text) {
		flush_text_node_edit(compositor, 1);
	}

	if (*res) {
		const char *src = *res;
		compositor->sel_buffer_alloc = 2+strlen(src);
		compositor->sel_buffer = realloc(compositor->sel_buffer, sizeof(u16)*compositor->sel_buffer_alloc);

		if (caret_pos>=0) {
			compositor->sel_buffer_len = gf_utf8_mbstowcs(compositor->sel_buffer, compositor->sel_buffer_alloc, &src);
			memmove(&compositor->sel_buffer[caret_pos+1], &compositor->sel_buffer[caret_pos], sizeof(u16)*(compositor->sel_buffer_len-caret_pos));
			compositor->sel_buffer[caret_pos]='|';
			compositor->caret_pos = caret_pos;
		
		} else {
			compositor->sel_buffer_len = gf_utf8_mbstowcs(compositor->sel_buffer, compositor->sel_buffer_alloc, &src);
			compositor->sel_buffer[compositor->sel_buffer_len]='|';
			compositor->caret_pos = compositor->sel_buffer_len;
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
	return 1;
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
				load_text_node(compositor, 1);
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
				load_text_node(compositor, 2);
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
			if (!load_text_node(compositor, (event->key.flags & GF_KEY_MOD_SHIFT) ? 1 : 2)) {
				is_end = 1;
				break;
			}
			return;
		case GF_KEY_BACKSPACE:
			if (compositor->caret_pos) {
				if (compositor->sel_buffer_len) compositor->sel_buffer_len--;
				memmove(&compositor->sel_buffer[compositor->caret_pos-1], &compositor->sel_buffer[compositor->caret_pos], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos+1));
				compositor->sel_buffer[compositor->sel_buffer_len]=0;
				compositor->caret_pos--;
				flush_text_node_edit(compositor, 0);
			} else {
				load_text_node(compositor, 4);
			}
			return;
		case GF_KEY_DEL:
			if (compositor->caret_pos+1<compositor->sel_buffer_len) {
				if (compositor->sel_buffer_len) compositor->sel_buffer_len--;
				memmove(&compositor->sel_buffer[compositor->caret_pos+1], &compositor->sel_buffer[compositor->caret_pos+2], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos-1));
				compositor->sel_buffer[compositor->sel_buffer_len]=0;
				flush_text_node_edit(compositor, 0);
				return;
			}
			break;
		case GF_KEY_ENTER:
			load_text_node(compositor, 3);
			return;
		default:
			return;
		}
		if (!is_end) {
			if (compositor->caret_pos==prev_caret) return;
			memmove(&compositor->sel_buffer[prev_caret], &compositor->sel_buffer[prev_caret+1], sizeof(u16)*(compositor->sel_buffer_len-prev_caret));
			memmove(&compositor->sel_buffer[compositor->caret_pos+1], &compositor->sel_buffer[compositor->caret_pos], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos));
			compositor->sel_buffer[compositor->caret_pos]='|';
		}
	} else {
		return;
	}

	flush_text_node_edit(compositor, is_end);
}

static Bool hit_node_editable(GF_Compositor *compositor, Bool check_focus_node) 
{
#ifndef GPAC_DISABLE_SVG
	SVGAllAttributes atts;
#endif
	u32 tag;
	GF_Node *text = check_focus_node ? compositor->focus_node : compositor->hit_node;
	if (!text) return 0;
	if (compositor->hit_node==compositor->focus_node) return compositor->focus_text_type ? 1 : 0;

	tag = gf_node_get_tag(text);
	if ( (tag==TAG_MPEG4_Text) || (tag==TAG_X3D_Text)) {
		M_FontStyle *fs = (M_FontStyle *) ((M_Text *)text)->fontStyle;
		if (!fs) return 0;
		if (!strstr(fs->style.buffer, "editable") && !strstr(fs->style.buffer, "EDITABLE")) return 0;
		compositor->focus_text_type = 3;
		compositor->focus_node = text;
		return 1;
	}
#ifndef GPAC_DISABLE_SVG
	if (tag <= GF_NODE_FIRST_DOM_NODE_TAG) return 0;
	gf_svg_flatten_attributes((SVG_Element *)text, &atts);
	if (!atts.editable || !*atts.editable) return 0;
	switch (tag) {
	case TAG_SVG_text:
	case TAG_SVG_textArea:
		compositor->focus_text_type = 1;
		break;
	case TAG_SVG_tspan:
		compositor->focus_text_type = 2;
		break;
	default:
		return 0;
	}
	if (compositor->focus_node != text) {
		GF_DOM_Event evt;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.bubbles = 1;
		evt.type = GF_EVENT_FOCUSOUT;
		gf_dom_event_fire(compositor->focus_node, &evt);

		compositor->focus_node = text;
		evt.type = GF_EVENT_FOCUSIN;
		gf_dom_event_fire(compositor->focus_node, &evt);
		compositor->focus_uses_dom_events = 1;
	}
	compositor->hit_node = NULL;
#endif
	return 1;
}

static GF_Node *get_parent_focus(GF_Node *node, GF_List *hit_use_stack, u32 cur_idx)
{
	GF_Node *parent;
#ifndef GPAC_DISABLE_SVG
	GF_FieldInfo info;
#endif
	if (!node) return NULL;

#ifndef GPAC_DISABLE_SVG
	if (gf_node_get_attribute_by_tag(node, TAG_SVG_ATT_focusable, 0, 0, &info)==GF_OK) {
		if ( *(SVG_Focusable*)info.far_ptr == SVG_FOCUSABLE_TRUE) return node;
	}
#endif
	parent = gf_node_get_parent(node, 0);
	if (cur_idx) {
		GF_Node *n = gf_list_get(hit_use_stack, cur_idx-1);
		if (n==node) {
			parent = gf_list_get(hit_use_stack, cur_idx-2);
			if (cur_idx>1) cur_idx-=2;
			else cur_idx=0;
		}
	}
	return get_parent_focus(parent, hit_use_stack, cur_idx);
}

static Bool exec_event_dom(GF_Compositor *compositor, GF_Event *event)
{
#ifndef GPAC_DISABLE_SVG
	GF_DOM_Event evt;
	u32 cursor_type;
	Bool ret = 0;

	cursor_type = GF_CURSOR_NORMAL;
	/*all mouse events*/
	if ((event->type<=GF_EVENT_MOUSEWHEEL) 
		// && (gf_node_get_dom_event_filter(gf_sg_get_root_node(compositor->scene)) & GF_DOM_EVENT_MOUSE)
		) {
		Fixed X = compositor->hit_world_point.x;
		Fixed Y = compositor->hit_world_point.y;
		if (compositor->hit_node) {
			GF_Node *focus;
			Bool hit_changed = 0;
			GF_Node *current_use = gf_list_last(compositor->hit_use_stack);
			cursor_type = compositor->sensor_type;
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.clientX = evt.screenX = FIX2INT(X);
			evt.clientY = evt.screenY = FIX2INT(Y);
			evt.bubbles = 1;
			evt.cancelable = 1;
			evt.key_flags = compositor->key_states;

			/*first check if the hit node is not the grab (previous) node - this may happen regardless of the
			mouse actions (animations, ...)*/
			if ((compositor->grab_node != compositor->hit_node) || (compositor->grab_use != current_use) ) {
				/*mouse out*/
				if (compositor->grab_node) {
					evt.relatedTarget = compositor->hit_node;
					evt.type = GF_EVENT_MOUSEOUT;
					ret += gf_dom_event_fire_ex(compositor->grab_node, &evt, compositor->prev_hit_use_stack);
					/*prepare mouseOver*/
					evt.relatedTarget = compositor->grab_node;
				}
				compositor->grab_node = compositor->hit_node;
				compositor->grab_use = current_use;

				/*mouse over*/
				evt.type = GF_EVENT_MOUSEOVER;
				ret += gf_dom_event_fire_ex(compositor->grab_node, &evt, compositor->hit_use_stack);
				hit_changed = 1;
			}
			switch (event->type) {
			case GF_EVENT_MOUSEMOVE:
				evt.cancelable = 0;
				if (!hit_changed) {
					evt.type = GF_EVENT_MOUSEMOVE;
					ret += gf_dom_event_fire_ex(compositor->grab_node, &evt, compositor->hit_use_stack);
				}
				compositor->num_clicks = 0;
				break;
			case GF_EVENT_MOUSEDOWN:
				if ((compositor->grab_x != X) || (compositor->grab_y != Y)) compositor->num_clicks = 0;
				evt.type = GF_EVENT_MOUSEDOWN;
				compositor->num_clicks ++;
				evt.button = event->mouse.button;
				evt.detail = compositor->num_clicks;

				ret += gf_dom_event_fire_ex(compositor->grab_node, &evt, compositor->hit_use_stack);
				compositor->grab_x = X;
				compositor->grab_y = Y;

				/*change focus*/
				focus = get_parent_focus(compositor->grab_node, compositor->hit_use_stack, gf_list_count(compositor->hit_use_stack));
				if (focus) gf_sc_focus_switch_ring_ex(compositor, 0, focus, 1);
				else if (compositor->focus_node) gf_sc_focus_switch_ring_ex(compositor, 0, NULL, 1);

				break;
			case GF_EVENT_MOUSEUP:
				evt.type = GF_EVENT_MOUSEUP;
				evt.button = event->mouse.button;
				evt.detail = compositor->num_clicks;
				ret += gf_dom_event_fire_ex(compositor->grab_node, &evt, compositor->hit_use_stack);
				if ((compositor->grab_x == X) && (compositor->grab_y == Y)) {
					evt.type = GF_EVENT_CLICK;
					ret += gf_dom_event_fire_ex(compositor->grab_node, &evt, compositor->hit_use_stack);
				}
				break;
			case GF_EVENT_MOUSEWHEEL:
				evt.type = GF_EVENT_MOUSEWHEEL;
				evt.button = event->mouse.button;
				evt.new_scale = event->mouse.wheel_pos;
				ret += gf_dom_event_fire_ex(compositor->grab_node, &evt, compositor->hit_use_stack);
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
				ret += gf_dom_event_fire_ex(compositor->grab_node, &evt, compositor->prev_hit_use_stack);
			}
			
			/*reset focus*/
			if (compositor->focus_node && (event->type==GF_EVENT_MOUSEDOWN)) 
				gf_sc_focus_switch_ring_ex(compositor, 0, NULL, 1);

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
			ret += gf_dom_event_fire_ex(gf_sg_get_root_node(compositor->scene), &evt, compositor->hit_use_stack);
		}
		if (compositor->sensor_type != cursor_type) {
			GF_Event c_evt; 
			c_evt.type = GF_EVENT_SET_CURSOR; 
			c_evt.cursor.cursor_type = cursor_type;
			compositor->video_out->ProcessEvent(compositor->video_out, &c_evt);
			compositor->sensor_type = cursor_type;
		}
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
		ret += gf_dom_event_fire(target, &evt);

		if (event->type==GF_EVENT_KEYDOWN) {
			switch (event->key.key_code) {
			case GF_KEY_ENTER:
				evt.type = GF_EVENT_ACTIVATE;
				evt.detail = 0;
				ret += gf_dom_event_fire(target, &evt);
				break;
			}
		} 
	} else if (event->type == GF_EVENT_TEXTINPUT) {
		GF_Node *target;
		switch (event->character.unicode_char) {
		case '\r':
		case '\n':
		case '\t':
		case '\b':
			break;
		default:
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.key_flags = event->key.flags;
			evt.bubbles = 1;
			evt.cancelable = 1;
			evt.type = event->type;
			evt.detail = event->character.unicode_char;
			target = compositor->focus_node;
			if (!target) target = gf_sg_get_root_node(compositor->scene);
			ret += gf_dom_event_fire(target, &evt);
			break;
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


static Bool exec_vrml_key_event(GF_Compositor *compositor, GF_Node *node, GF_Event *ev, Bool is_focus_out)
{
	GF_SensorHandler *hdl = NULL;
	GF_ChildNodeItem *child;
	u32 ret = 0;
	if (!node) node = compositor->focus_node;
	if (!node) return 0;

	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Text:
	case TAG_X3D_Text:
		return 0;
	case TAG_MPEG4_Layout:
		hdl = compositor_mpeg4_layout_get_sensor_handler(node);
		break;
	case TAG_MPEG4_Anchor:
	case TAG_X3D_Anchor:
		hdl = compositor_mpeg4_get_sensor_handler(node);
		break;
	}
	child = ((GF_ParentNode*)node)->children;
	if (hdl) {
		ret++;
		hdl->OnUserEvent(hdl, is_focus_out ? 0 : 1, ev, compositor);
	} else {
		while (child) {
			hdl = compositor_mpeg4_get_sensor_handler(child->node);
			if (hdl) {
				ret++;
				hdl->OnUserEvent(hdl, is_focus_out ? 0 : 1, ev, compositor);
			}
			child = child->next;
		}
	}
	return ret ? 1 : 0;
}



Bool visual_execute_event(GF_VisualManager *visual, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children)
{
	Bool ret;
	Bool reset_sel = 0;
	GF_List *temp_stack;
	GF_Compositor *compositor = visual->compositor;
	tr_state->traversing_mode = TRAVERSE_PICK;
#ifndef GPAC_DISABLE_3D
	tr_state->layer3d = NULL;
#endif
	
	/*preprocess text selection and edition*/
	if ((ev->type<GF_EVENT_MOUSEWHEEL) && (ev->mouse.button==GF_MOUSE_LEFT)) {
		if (compositor->text_selection) {
			if (ev->type==GF_EVENT_MOUSEUP) {
				if (compositor->store_text_state==GF_SC_TSEL_ACTIVE)
					compositor->store_text_state = GF_SC_TSEL_FROZEN;
				else {
					reset_sel = 1;
				}
			}
			else if (ev->type==GF_EVENT_MOUSEDOWN) {
				reset_sel = 1;
			}
		} else if (compositor->edited_text) {
			if (ev->type==GF_EVENT_MOUSEDOWN) 
				reset_sel = 1;
		}
		if (ev->type==GF_EVENT_MOUSEUP) {
			if (hit_node_editable(compositor, 0)) {
				compositor->text_selection = NULL;
				exec_text_input(compositor, NULL);
				return 1;
			}
#if 0
			else if (!compositor->focus_node) {
				gf_sc_focus_switch_ring_ex(compositor, 0, gf_sg_get_root_node(compositor->scene), 1);
			}
#endif
		}
		if (reset_sel) {
			flush_text_node_edit(compositor, 1);

			compositor->store_text_state = GF_SC_TSEL_RELEASED;
			compositor->text_selection = NULL;
			if (compositor->selected_text) free(compositor->selected_text);
			compositor->selected_text = NULL;
			if (compositor->sel_buffer) free(compositor->sel_buffer);
			compositor->sel_buffer = NULL;
			compositor->sel_buffer_alloc = 0;
			compositor->sel_buffer_len = 0;

			compositor->draw_next_frame = 1;
		} else if (compositor->store_text_state == GF_SC_TSEL_RELEASED) {
			compositor->store_text_state = GF_SC_TSEL_NONE;
		}

	}

	/*pick node*/
	compositor->hit_appear = NULL;
	compositor->hit_text = NULL;
	temp_stack = compositor->prev_hit_use_stack;
	compositor->prev_hit_use_stack = compositor->hit_use_stack;
	compositor->hit_use_stack = temp_stack;
	
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
		if (!gf_list_count(compositor->sensors) && !gf_list_count(compositor->previous_sensors)) return 0;
	}
	return exec_event_vrml(compositor, ev);
}

u32 gf_sc_svg_focus_navigate(GF_Compositor *compositor, u32 key_code)
{
#ifndef GPAC_DISABLE_SVG
	SVGAllAttributes atts;
	GF_DOM_Event evt;
	u32 ret = 0;
	GF_Node *n;
	SVG_Focus *focus = NULL;

	/*only for dom-based nodes*/
	if (!compositor->focus_node || !compositor->focus_uses_dom_events) return 0;

	n=NULL;
	gf_svg_flatten_attributes((SVG_Element *)compositor->focus_node, &atts);
	switch (key_code) {
	case GF_KEY_LEFT: focus = atts.nav_left; break;
	case GF_KEY_RIGHT: focus = atts.nav_right; break;
	case GF_KEY_UP: focus = atts.nav_up; break;
	case GF_KEY_DOWN: focus = atts.nav_down; break;
	default: return 0;
	}
	if (!focus) return 0;

	if (focus->type==SVG_FOCUS_SELF) return 0;
	if (focus->type==SVG_FOCUS_AUTO) return 0;
	if (!focus->target.target) {
		if (!focus->target.string) return 0;
		focus->target.target = gf_sg_find_node_by_name(compositor->scene, focus->target.string+1);
	}
	n = focus->target.target;

	ret = 0;
	if (n != compositor->focus_node) {
		/*the event is already handled, even though no listeners may be present*/
		ret = 1;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.bubbles = 1;
		if (compositor->focus_node) {
			evt.type = GF_EVENT_FOCUSOUT;
			gf_dom_event_fire(compositor->focus_node, &evt);
		}
		if (n) {
			evt.relatedTarget = n;
			evt.type = GF_EVENT_FOCUSIN;
			gf_dom_event_fire(n, &evt);
		}
		compositor->focus_node = n;
		//invalidate in case we draw focus rect
		gf_sc_invalidate(compositor, NULL);
	}
	return ret;
#else
	return 0;
#endif /*GPAC_DISABLE_SVG*/
}

/*focus management*/
static Bool is_focus_target(GF_Node *elt)
{
#ifndef GPAC_DISABLE_SVG
	u32 i, count;
#endif
	u32 tag = gf_node_get_tag(elt);
	switch (tag) {
#ifndef GPAC_DISABLE_SVG
	case TAG_SVG_a:
#endif
	case TAG_MPEG4_Anchor:
	case TAG_X3D_Anchor:
		return 1;
	default:
		break;
	}
	if (tag<=GF_NODE_FIRST_DOM_NODE_TAG) return 0;

#ifndef GPAC_DISABLE_SVG
	count = gf_dom_listener_count(elt);	
	for (i=0; i<count; i++) {
		GF_FieldInfo info;
		GF_Node *l = gf_dom_listener_get(elt, i);
		if (gf_node_get_attribute_by_tag(l, TAG_XMLEV_ATT_event, 0, 0, &info)==GF_OK) {
			switch ( ((XMLEV_Event*)info.far_ptr)->type) {
			case GF_EVENT_FOCUSIN:
			case GF_EVENT_FOCUSOUT:
			case GF_EVENT_ACTIVATE:
				return 1;
			}
		}
	}
#endif /*GPAC_DISABLE_SVG*/
	return 0;
}

#define CALL_SET_FOCUS(__child)	{	\
	gf_list_add(compositor->focus_ancestors, elt);	\
	n = set_focus(compositor, __child, current_focus, prev_focus);	\
	if (n) return n;	\
	gf_list_rem_last(compositor->focus_ancestors);	\
	return NULL;	\
	}	\


static void rebuild_focus_ancestor(GF_Compositor *compositor, GF_Node *elt) 
{
	gf_list_reset(compositor->focus_ancestors);
	while (elt) {
		GF_Node *par = gf_node_get_parent(elt, 0);
		if (!par) break;
		gf_list_insert(compositor->focus_ancestors, par, 0);
		elt = par;
	}
}

static GF_Node *set_focus(GF_Compositor *compositor, GF_Node *elt, Bool current_focus, Bool prev_focus)
{
	u32 tag;
	GF_ChildNodeItem *child = NULL;
	GF_Node *use_node = NULL;
	GF_Node *anim_node = NULL;
	GF_Node *n;

	if (!elt) return NULL;

	tag = gf_node_get_tag(elt);
	if (tag <= GF_NODE_FIRST_DOM_NODE_TAG) {
		switch (tag) {
		case TAG_MPEG4_Group: case TAG_X3D_Group:
		case TAG_MPEG4_Transform: case TAG_X3D_Transform:
		case TAG_MPEG4_Billboard: case TAG_X3D_Billboard:
		case TAG_MPEG4_Collision: case TAG_X3D_Collision: 
		case TAG_MPEG4_LOD: case TAG_X3D_LOD: 
		case TAG_MPEG4_OrderedGroup:
		case TAG_MPEG4_Transform2D:
		case TAG_MPEG4_TransformMatrix2D:
		case TAG_MPEG4_ColorTransform:
		case TAG_MPEG4_Layer3D:
		case TAG_MPEG4_Layer2D:
		case TAG_MPEG4_PathLayout:
		case TAG_MPEG4_Form:
		case TAG_MPEG4_Anchor: case TAG_X3D_Anchor:
			if (!current_focus) {
				/*get the base grouping stack (*/
				BaseGroupingStack *grp = (BaseGroupingStack*)gf_node_get_private(elt);
				if (grp && (grp->flags & (GROUP_HAS_SENSORS | GROUP_IS_ANCHOR) )) 
					return elt;
			}
			break;
		case TAG_MPEG4_Switch: case TAG_X3D_Switch:
		{
			s32 i, wc;
			GF_ChildNodeItem *child;
			if (tag==TAG_X3D_Switch) {
				child = ((X_Switch*)elt)->children;
				wc = ((X_Switch*)elt)->whichChoice;
			} else {
				child = ((M_Switch*)elt)->choice;
				wc = ((M_Switch*)elt)->whichChoice;
			}
			if (wc==-1) return NULL;
			i=0;
			while (child) {
				if (i==wc) CALL_SET_FOCUS(child->node);
				child = child->next;
			}
			return NULL;
		}

		case TAG_MPEG4_Text: case TAG_X3D_Text:
			if (!current_focus) {
				M_FontStyle *fs = (M_FontStyle *) ((M_Text *)elt)->fontStyle;
				if (!fs) return NULL;
				if (!strstr(fs->style.buffer, "editable") && !strstr(fs->style.buffer, "EDITABLE")) return NULL;
				compositor->focus_text_type = 3;
				return elt;
			}
			return NULL;
		case TAG_MPEG4_Layout:
			if (!current_focus && (compositor_mpeg4_layout_get_sensor_handler(elt)!=NULL)) 
				return elt;
			break;

		case TAG_ProtoNode:
			CALL_SET_FOCUS(gf_node_get_proto_root(elt));
		
		case TAG_MPEG4_Inline: case TAG_X3D_Inline: 
			CALL_SET_FOCUS(gf_inline_get_subscene_root(elt));

		case TAG_MPEG4_Shape: case TAG_X3D_Shape:
			gf_list_add(compositor->focus_ancestors, elt);
			n = set_focus(compositor, ((M_Shape*)elt)->geometry, current_focus, prev_focus);
			if (n) return n;
			n = set_focus(compositor, ((M_Shape*)elt)->appearance, current_focus, prev_focus);
			if (n) return n;
			gf_list_rem_last(compositor->focus_ancestors);
			return NULL;

		case TAG_MPEG4_Appearance: case TAG_X3D_Appearance: 
			CALL_SET_FOCUS(((M_Appearance*)elt)->texture);

		case TAG_MPEG4_CompositeTexture2D: case TAG_MPEG4_CompositeTexture3D:
			/*CompositeTextures are not grouping nodes per say*/
			child = ((GF_ParentNode*)elt)->children;
			while (child) {
				if (compositor_mpeg4_get_sensor_handler(child->node) != NULL)
					return elt;
				child = child->next;
			}
			break;

		default:
			return NULL;
		}

		child = ((GF_ParentNode*)elt)->children;
	} else {
#ifndef GPAC_DISABLE_SVG
		SVGAllAttributes atts;

		if (tag==TAG_SVG_defs) return NULL;

		gf_svg_flatten_attributes((SVG_Element *)elt, &atts);

		if (atts.display && (*atts.display==SVG_DISPLAY_NONE)) return NULL;

		if (!current_focus) {
			Bool is_auto = 1;
			if (atts.focusable) {
				if (*atts.focusable==SVG_FOCUSABLE_TRUE) return elt;
				if (*atts.focusable==SVG_FOCUSABLE_FALSE) is_auto = 0;
			}
			if (is_auto && is_focus_target(elt)) return elt;

			if (atts.editable && *atts.editable) {
				switch (tag) {
				case TAG_SVG_text:
				case TAG_SVG_textArea:
					compositor->focus_text_type = 1;
					return elt;
				case TAG_SVG_tspan:
					compositor->focus_text_type = 2;
					return elt;
				default:
					break;
				}
			}
		}

		if (prev_focus) {
			if (atts.nav_prev) {
				switch (atts.nav_prev->type) {
				case SVG_FOCUS_SELF:
					/*focus locked on element*/
					return elt;
				case SVG_FOCUS_IRI:
					if (!atts.nav_prev->target.target) {
						if (!atts.nav_prev->target.string) return NULL;
						atts.nav_prev->target.target = gf_sg_find_node_by_name(compositor->scene, atts.nav_prev->target.string+1);
					}
					if (atts.nav_prev->target.target) {
						rebuild_focus_ancestor(compositor, atts.nav_prev->target.target);
						return atts.nav_prev->target.target;
					}
				default:
					break;
				}
			}
		} else {
			/*check next*/
			if (atts.nav_next) {
				switch (atts.nav_next->type) {
				case SVG_FOCUS_SELF:
					/*focus locked on element*/
					return elt;
				case SVG_FOCUS_IRI:
					if (!atts.nav_next->target.target) {
						if (!atts.nav_next->target.string) return NULL;
						atts.nav_next->target.target = gf_sg_find_node_by_name(compositor->scene, atts.nav_next->target.string+1);
					}
					if (atts.nav_next->target.target) {
						rebuild_focus_ancestor(compositor, atts.nav_next->target.target);
						return atts.nav_next->target.target;
					}
				default:
					break;
				}
			}
		}
		if (atts.xlink_href) {
			switch (tag) {
			case TAG_SVG_use:
				use_node = compositor_svg_get_xlink_resource_node(elt, atts.xlink_href);
				break;
			case TAG_SVG_animation:
				anim_node = compositor_svg_get_xlink_resource_node(elt, atts.xlink_href);
				break;
			}
		}
#endif /*GPAC_DISABLE_SVG*/
		child = ((GF_ParentNode *)elt)->children;
	}

	if (prev_focus) {
		u32 count, i;
		/*check all children except if current focus*/
		if (current_focus) return NULL;
		gf_list_add(compositor->focus_ancestors, elt);
		count = gf_node_list_get_count(child);
		for (i=count; i>0; i--) {
			/*get in the subtree*/
			n = gf_node_list_get_child( ((GF_ParentNode *)elt)->children, i-1);
			n = set_focus(compositor, n, 0, 1);
			if (n) return n;
		}
	} else {
		/*check all children */
		gf_list_add(compositor->focus_ancestors, elt);
		while (child) {
			/*get in the subtree*/
			n = set_focus(compositor, child->node, 0, 0);
			if (n) return n;
			child = child->next;
		}
	}
	if (use_node) {
		gf_list_add(compositor->focus_use_stack, use_node);
		gf_list_add(compositor->focus_use_stack, elt);
		n = set_focus(compositor, use_node, 0, 0);
		if (n) {
			compositor->focus_used = elt;
			return n;
		}
		gf_list_rem_last(compositor->focus_use_stack);
		gf_list_rem_last(compositor->focus_use_stack);
	} else if (anim_node) {
		n = set_focus(compositor, anim_node, 0, 0);
		if (n) return n;
	}

	gf_list_rem_last(compositor->focus_ancestors);

	return NULL;
}

static GF_Node *browse_parent_for_focus(GF_Compositor *compositor, GF_Node *elt, Bool prev_focus)
{
	u32 tag;
	s32 idx = 0;
	GF_ChildNodeItem *child;
	GF_Node *n;
	GF_Node *par;

	par = gf_list_last(compositor->focus_ancestors);
	if (!par) return NULL;

	tag = gf_node_get_tag(par);
	if (tag <= GF_NODE_FIRST_DOM_NODE_TAG) {
		switch (tag) {
		case TAG_MPEG4_Group: case TAG_X3D_Group:
		case TAG_MPEG4_Transform: case TAG_X3D_Transform:
		case TAG_MPEG4_Billboard: case TAG_X3D_Billboard:
		case TAG_MPEG4_Collision: case TAG_X3D_Collision: 
		case TAG_MPEG4_LOD: case TAG_X3D_LOD: 
		case TAG_MPEG4_OrderedGroup:
		case TAG_MPEG4_Transform2D:
		case TAG_MPEG4_TransformMatrix2D:
		case TAG_MPEG4_ColorTransform:
		case TAG_MPEG4_Layer3D:
		case TAG_MPEG4_Layer2D:
		case TAG_MPEG4_Layout:
		case TAG_MPEG4_PathLayout:
		case TAG_MPEG4_Form:
		case TAG_MPEG4_Anchor: case TAG_X3D_Anchor:
		case TAG_MPEG4_CompositeTexture2D: case TAG_MPEG4_CompositeTexture3D:
			child = ((GF_ParentNode*)par)->children;
			break;
		/*for all other node, locate parent*/
		default:
			gf_list_rem_last(compositor->focus_ancestors);
			return browse_parent_for_focus(compositor, par, prev_focus);
		}
	} else {
		child = ((GF_ParentNode *)par)->children;
	}

	/*locate element*/
	idx = gf_node_list_find_child(child, elt);

	/*use or animation*/
	if (idx<0) {
		/*up one level*/
		gf_list_rem_last(compositor->focus_ancestors);
#ifndef GPAC_DISABLE_SVG
		if (tag==TAG_SVG_use) {
			gf_list_rem_last(compositor->focus_use_stack);
			gf_list_rem_last(compositor->focus_use_stack);
			if (compositor->focus_used == par) compositor->focus_used = NULL;
		}
#endif
		return browse_parent_for_focus(compositor, (GF_Node*)par, prev_focus);
	}

	if (prev_focus) {
		u32 i, count;
		count = gf_node_list_get_count(child);
		/*!! this may happen when walking up PROTO nodes !!*/
		for (i=idx; i>0; i--) {
			n = gf_node_list_get_child(child, i-1);
			/*get in the subtree*/
			n = set_focus(compositor, n, 0, 1);
			if (n) return n;
		}
	} else {
		/*!! this may happen when walking up PROTO nodes !!*/
		while (child) {
			if (idx<0) {
				/*get in the subtree*/
				n = set_focus(compositor, child->node, 0, 0);
				if (n) return n;
			}
			idx--;
			child = child->next;
		}
	}
	/*up one level*/
	gf_list_rem_last(compositor->focus_ancestors);
	return browse_parent_for_focus(compositor, (GF_Node*)par, prev_focus);
}

u32 gf_sc_focus_switch_ring_ex(GF_Compositor *compositor, Bool move_prev, GF_Node *focus, Bool force_focus)
{
	Bool current_focus = 1;
	Bool prev_uses_dom_events;
	u32 ret = 0;
	GF_List *cloned_use = NULL;
	GF_Node *n, *prev, *prev_use;

	prev = compositor->focus_node;
	prev_use = compositor->focus_used;
	compositor->focus_text_type = 0;
	prev_uses_dom_events = compositor->focus_uses_dom_events;
	compositor->focus_uses_dom_events = 0;

	if (!compositor->focus_node) {
		compositor->focus_node = gf_sg_get_root_node(compositor->scene);
		gf_list_reset(compositor->focus_ancestors);
		if (!compositor->focus_node) return 0;
		current_focus = 0;
	}

	if (compositor->focus_used) {
		u32 i, count;
		cloned_use = gf_list_new();
		count = gf_list_count(compositor->focus_use_stack);
		for (i=0; i<count; i++) {
			gf_list_add(cloned_use, gf_list_get(compositor->focus_use_stack, i));
		}
	}

	/*get focus in current doc order*/
	if (force_focus) {
		gf_list_reset(compositor->focus_ancestors);
		n = focus;
	} else {
		n = set_focus(compositor, compositor->focus_node, current_focus, move_prev);
		if (!n) n = browse_parent_for_focus(compositor, compositor->focus_node, move_prev);

		if (!n) {
			if (!prev) n = gf_sg_get_root_node(compositor->scene);
			gf_list_reset(compositor->focus_ancestors);
		}
	}
	
	if (n && gf_node_get_tag(n)>=GF_NODE_FIRST_DOM_NODE_TAG) {
		compositor->focus_uses_dom_events = 1;
	}
	compositor->focus_node = n;

	ret = 0;
#ifndef GPAC_DISABLE_SVG
	if ((prev != compositor->focus_node) || (prev_use != compositor->focus_used)) {
		GF_DOM_Event evt;
		GF_Event ev;
		/*the event is already handled, even though no listeners may be present*/
		ret = 1;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		memset(&ev, 0, sizeof(GF_Event));
		ev.type = GF_EVENT_KEYDOWN;
		ev.key.key_code = move_prev ? GF_KEY_LEFT : GF_KEY_RIGHT;

		if (prev) {
			if (prev_uses_dom_events) {
				evt.bubbles = 1;
				evt.type = GF_EVENT_FOCUSOUT;
				gf_dom_event_fire_ex(prev, &evt, cloned_use);
			} else {
				exec_vrml_key_event(compositor, prev, &ev, 1);
			}
		}
		if (compositor->focus_node) {
			if (compositor->focus_uses_dom_events) {
				evt.bubbles = 1;
				evt.type = GF_EVENT_FOCUSIN;
				gf_dom_event_fire_ex(compositor->focus_node, &evt, compositor->focus_use_stack);
			} else {
				exec_vrml_key_event(compositor, NULL, &ev, 0);
			}
		}
		/*invalidate in case we draw focus rect*/
		gf_sc_invalidate(compositor, NULL);
	}
#endif /*GPAC_DISABLE_SVG*/
	if (cloned_use) gf_list_del(cloned_use);

	if (hit_node_editable(compositor, 1)) {
		compositor->text_selection = NULL;
		exec_text_input(compositor, NULL);
	}
	return ret;
}

u32 gf_sc_focus_switch_ring(GF_Compositor *compositor, Bool move_prev)
{
	return gf_sc_focus_switch_ring_ex(compositor, move_prev, NULL, 0);
}

Bool gf_sc_execute_event(GF_Compositor *compositor, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children)
{
	/*filter mouse events and other events (key...)*/
	if (ev->type>GF_EVENT_MOUSEWHEEL) {
		Bool ret = 0;
		/*send text edit*/
		if (compositor->edited_text) {
			exec_text_input(compositor, ev);
			return 1;
		}
		/*FIXME - this is not working for mixed docs*/
		if (compositor->focus_uses_dom_events) 
			ret = exec_event_dom(compositor, ev);
		else 
			ret = exec_vrml_key_event(compositor, compositor->focus_node, ev, 0);

		
		if (ev->type==GF_EVENT_KEYDOWN) {
			switch (ev->key.key_code) {
			case GF_KEY_ENTER:
				if (0&&compositor->focus_text_type) {
					exec_text_input(compositor, NULL);
					ret = 1;
				} 
				break;
			case GF_KEY_TAB:
				ret += gf_sc_focus_switch_ring(compositor, (ev->key.flags & GF_KEY_MOD_SHIFT) ? 1 : 0);
				break;
			case GF_KEY_UP:
			case GF_KEY_DOWN:
			case GF_KEY_LEFT:
			case GF_KEY_RIGHT:
				ret += gf_sc_svg_focus_navigate(compositor, ev->key.key_code);
				break;
			}
		} 
		return ret;
	} 
	/*pick even, call visual handler*/
	return visual_execute_event(compositor->visual, tr_state, ev, children);
}

Bool gf_sc_exec_event(GF_Compositor *compositor, GF_Event *evt)
{
	if (evt->type<=GF_EVENT_MOUSEWHEEL) {
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


