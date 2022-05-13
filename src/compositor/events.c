/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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
#include <gpac/utf.h>

static GF_Node *browse_parent_for_focus(GF_Compositor *compositor, GF_Node *elt, Bool prev_focus);

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
	if (event->type>GF_EVENT_MOUSEMOVE) return GF_FALSE;

	if (compositor->edited_text)
		return GF_FALSE;
	if (compositor->text_selection )
		return compositor->hit_text ? GF_TRUE : GF_FALSE;
	switch (event->type) {
	case GF_EVENT_MOUSEMOVE:
		if (compositor->text_selection && compositor->hit_text)
			return GF_TRUE;
		break;
	case GF_EVENT_MOUSEDOWN:
		if (compositor->hit_text) {
			compositor->text_selection = compositor->hit_text;
			/*return 0: the event may be consumed by the tree*/
			return GF_FALSE;
		}
		break;
	}
	return GF_FALSE;
}

static void flush_text_node_edit(GF_Compositor *compositor, Bool final_flush)
{
	Bool signal;
	if (!compositor->edited_text) return;

	/* if this is the final editing and there is text,
	we need to remove the caret from the text selection buffer */
	if (final_flush && compositor->sel_buffer_len) {
		memmove(&compositor->sel_buffer[compositor->caret_pos], &compositor->sel_buffer[compositor->caret_pos+1], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos));
		compositor->sel_buffer_len--;
		compositor->sel_buffer[compositor->sel_buffer_len] = 0;
	}

	/* Recomputes the edited text */
	if (*compositor->edited_text) {
		gf_free(*compositor->edited_text);
		*compositor->edited_text = NULL;
	}
	if (compositor->sel_buffer_len) {
		char *txt;
		u32 len;
		const u16 *lptr;
		txt = (char*)gf_malloc(sizeof(char)*2*compositor->sel_buffer_len);
		lptr = compositor->sel_buffer;
		len = gf_utf8_wcstombs(txt, 2*compositor->sel_buffer_len, &lptr);
		if (len == GF_UTF8_FAIL) len = 0;
		txt[len] = 0;
		*compositor->edited_text = gf_strdup(txt);
		gf_free(txt);
	}

	signal = final_flush;
	if ((compositor->focus_text_type==4) && (final_flush==1)) signal = GF_FALSE;

	gf_node_dirty_set(compositor->focus_node, 0, GF_TRUE);
	//(compositor->focus_text_type==2));
	gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
	/*notify compositor that text has been edited, in order to update composite textures*/
	//compositor->text_edit_changed = 1;

	gf_node_set_private(compositor->focus_highlight->node, NULL);

	/* if this is the final flush, we free the selection buffer and edited text buffer
	and signal a text content change in the focus node */
	if (final_flush) {
		GF_FieldInfo info;
		if (compositor->sel_buffer) gf_free(compositor->sel_buffer);
		compositor->sel_buffer = NULL;
		compositor->sel_buffer_len = compositor->sel_buffer_alloc = 0;
		compositor->edited_text = NULL;

		if (compositor->focus_node && signal) {
			memset(&info, 0, sizeof(GF_FieldInfo));
			info.fieldIndex = (u32) -1;
			if (compositor->focus_text_type>=3) {
				gf_node_get_field(compositor->focus_node, 0, &info);
#ifndef GPAC_DISABLE_VRML
				gf_node_event_out(compositor->focus_node, 0);
#endif
			}
			gf_node_changed(compositor->focus_node, &info);
		}
	}
}


GF_EXPORT
GF_Err gf_sc_paste_text(GF_Compositor *compositor, const char *text)
{
	u16 *conv_buf;
	u32 len;
	if (!compositor->sel_buffer || !compositor->edited_text) return GF_BAD_PARAM;
	if (!text) return GF_OK;
	len = (u32) strlen(text);
	if (!len) return GF_OK;

	gf_sc_lock(compositor, GF_TRUE);

	conv_buf = (u16*)gf_malloc(sizeof(u16)*(len+1));
	len = gf_utf8_mbstowcs(conv_buf, len, &text);
	if (len == GF_UTF8_FAIL) return GF_IO_ERR;

	compositor->sel_buffer_alloc += len;
	if (compositor->sel_buffer_len == compositor->sel_buffer_alloc)
		compositor->sel_buffer_alloc++;

	compositor->sel_buffer = (u16*)gf_realloc(compositor->sel_buffer, sizeof(u16)*compositor->sel_buffer_alloc);
	memmove(&compositor->sel_buffer[compositor->caret_pos+len], &compositor->sel_buffer[compositor->caret_pos], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos));
	memcpy(&compositor->sel_buffer[compositor->caret_pos], conv_buf, sizeof(u16)*len);
	gf_free(conv_buf);
	compositor->sel_buffer_len += len;
	compositor->caret_pos += len;
	compositor->sel_buffer[compositor->sel_buffer_len]=0;
	flush_text_node_edit(compositor, GF_FALSE);
	gf_sc_lock(compositor, GF_FALSE);

	return GF_OK;
}
static Bool load_text_node(GF_Compositor *compositor, u32 cmd_type)
{
	char **res = NULL;
	u32 prev_pos, pos=0;
	s32 caret_pos;
	Bool append;
#ifndef GPAC_DISABLE_SVG
	Bool delete_cr = GF_FALSE;
#endif

	caret_pos = -1;

	append = GF_FALSE;
	switch (cmd_type) {
	/*load last text chunk*/
	case 0:
		pos = 0;
		break;
	/*load prev text chunk*/
	case 4:
#ifndef GPAC_DISABLE_SVG
		delete_cr = GF_TRUE;
#endif
	case 1:
		if (compositor->dom_text_pos == 0) return GF_FALSE;
		pos = compositor->dom_text_pos - 1;
		break;
	/*load next text chunk*/
	case 2:
		pos = compositor->dom_text_pos + 1;
		caret_pos = 0;
		break;
	/*split text chunk/append new*/
	case 3:
		append = GF_TRUE;
		pos = compositor->dom_text_pos;
		caret_pos = 0;
		break;
	}
	prev_pos = compositor->dom_text_pos;
	compositor->dom_text_pos = 0;


#ifndef GPAC_DISABLE_VRML
	if (compositor->focus_text_type>=3) {
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
			return GF_FALSE;
		}

		if (compositor->picked_span_idx >=0) {
			compositor->dom_text_pos = 1+compositor->picked_span_idx;
			compositor->picked_span_idx = -1;
		}
		/*take care of loading empty text nodes*/
		if (!mf->count) {
			mf->count = 1;
			mf->vals = (char**)gf_malloc(sizeof(char*));
			mf->vals[0] = gf_strdup("");
		}
		if (!mf->vals[0]) mf->vals[0] = gf_strdup("");

		if (!compositor->dom_text_pos || (compositor->dom_text_pos>mf->count)) {
			compositor->dom_text_pos = prev_pos;
			return GF_FALSE;
		}
		res = &mf->vals[compositor->dom_text_pos-1];
		if (compositor->picked_glyph_idx>=0) {
			caret_pos = compositor->picked_glyph_idx;
			compositor->picked_glyph_idx = -1;

			if (caret_pos > (s32) strlen(*res))
				caret_pos = -1;
		}

	} else
#endif /*GPAC_DISABLE_VRML*/

		if (compositor->focus_node) {
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
						const u16 *srcp;
						GF_DOMText *ntext;
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
							u32 len;
							GF_DOMText *cur;

							gf_node_list_insert_child(&children, t, pos+1);
							ntext = (GF_DOMText*) gf_node_new(gf_node_get_graph(child->node), TAG_DOMText);
							gf_node_init(t);
							gf_node_list_insert_child(&children, (GF_Node *)ntext, pos+2);
							gf_node_register((GF_Node*)ntext, compositor->focus_node);

							cur = (GF_DOMText*) child->node;

							gf_free(cur->textContent);
							end = compositor->sel_buffer[compositor->caret_pos];
							compositor->sel_buffer[compositor->caret_pos] = 0;
							len = gf_utf8_wcslen(compositor->sel_buffer);
							cur->textContent = (char*)gf_malloc(sizeof(char)*(len+1));
							srcp = compositor->sel_buffer;
							len = gf_utf8_wcstombs(cur->textContent, len, &srcp);
							if (len == GF_UTF8_FAIL) len = 0;
							cur->textContent[len] = 0;
							compositor->sel_buffer[compositor->caret_pos] = end;

							if (compositor->caret_pos+1<compositor->sel_buffer_len) {
								len = gf_utf8_wcslen(compositor->sel_buffer + compositor->caret_pos + 1);
								ntext->textContent = (char*)gf_malloc(sizeof(char)*(len+1));
								srcp = compositor->sel_buffer + compositor->caret_pos + 1;
								len = gf_utf8_wcstombs(ntext->textContent, len, &srcp);
								if (len == GF_UTF8_FAIL) len = 0;
								ntext->textContent[len] = 0;
							} else {
								ntext->textContent = gf_strdup("");
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
									flush_text_node_edit(compositor, GF_TRUE);
								}
								if (!n1->textContent) n1->textContent = gf_strdup("");
								caret_pos = (u32) strlen(n1->textContent);
								if (n2->textContent) {
									n1->textContent = (char*)gf_realloc(n1->textContent, sizeof(char)*(strlen(n1->textContent)+strlen(n2->textContent)+1));
									strcat(n1->textContent, n2->textContent);
								}
								gf_node_list_del_child(&children, (GF_Node*)n2);
								gf_node_unregister((GF_Node*)n2, compositor->focus_node);
								compositor->edited_text = NULL;
							}
						}
						res = &((GF_DOMText *)child->node)->textContent;
					}
					break;
				}
				child = child->next;
				continue;
			}
			/*load of an empty text*/
			if (!res && !cmd_type) {
				GF_DOMText *t = gf_dom_add_text_node(compositor->focus_node, gf_strdup(""));
				res = &t->textContent;
			}

			if (!res) {
				compositor->dom_text_pos = prev_pos;
				return GF_FALSE;
			}
#endif
		}

	if (compositor->edited_text) {
		flush_text_node_edit(compositor, GF_TRUE);
	}

	if (res && *res && strlen(*res) ) {
		const char *src = *res;
		compositor->sel_buffer_alloc = 2 + (u32) strlen(src);
		compositor->sel_buffer = (u16*)gf_realloc(compositor->sel_buffer, sizeof(u16)*compositor->sel_buffer_alloc);

		if (caret_pos>=0) {
			u32 l = gf_utf8_mbstowcs(compositor->sel_buffer, compositor->sel_buffer_alloc, &src);
			if (l == GF_UTF8_FAIL) return GF_FALSE;
			compositor->sel_buffer_len = l;
			memmove(&compositor->sel_buffer[caret_pos+1], &compositor->sel_buffer[caret_pos], sizeof(u16)*(compositor->sel_buffer_len-caret_pos));
			compositor->sel_buffer[caret_pos] = GF_CARET_CHAR;
			compositor->caret_pos = caret_pos;

		} else {
			u32 l = gf_utf8_mbstowcs(compositor->sel_buffer, compositor->sel_buffer_alloc, &src);
			if (l == GF_UTF8_FAIL) return GF_FALSE;
			compositor->sel_buffer_len = l;
			compositor->sel_buffer[compositor->sel_buffer_len] = GF_CARET_CHAR;
			compositor->caret_pos = compositor->sel_buffer_len;
		}
		compositor->sel_buffer_len++;
		compositor->sel_buffer[compositor->sel_buffer_len]=0;
	} else {
		compositor->sel_buffer_alloc = 2;
		compositor->sel_buffer = (u16*)gf_malloc(sizeof(u16)*2);
		compositor->sel_buffer[0] = GF_CARET_CHAR;
		compositor->sel_buffer[1] = 0;
		compositor->caret_pos = 0;
		compositor->sel_buffer_len = 1;
	}
	compositor->edited_text = res;
	compositor->text_edit_changed = GF_TRUE;
	flush_text_node_edit(compositor, GF_FALSE);
	return GF_TRUE;
}

static void exec_text_input(GF_Compositor *compositor, GF_Event *event)
{
	Bool is_end = GF_FALSE;

	if (!event) {
		load_text_node(compositor, 0);
		return;
	} else if (event->type==GF_EVENT_TEXTINPUT) {
		u32 unicode_char = event->character.unicode_char;
		//filter all non-text symbols
		if (unicode_char <= 31) {
			return;
		}

		{
#ifndef GPAC_DISABLE_SVG
			GF_DOM_Event evt;
			GF_Node *target;
			/*send text input event*/
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.key_flags = event->key.flags;
			evt.bubbles = 1;
			evt.cancelable = 1;
			evt.type = event->type;
			evt.detail = unicode_char;
			target = compositor->focus_node;
			if (!target) target = gf_sg_get_root_node(compositor->scene);
			gf_dom_event_fire(target, &evt);
			/*event has been cancelled*/
			if (evt.event_phase & GF_DOM_EVENT_CANCEL_MASK) return;

			if (compositor->sel_buffer_len + 1 == compositor->sel_buffer_alloc) {
				compositor->sel_buffer_alloc += 10;
				compositor->sel_buffer = (u16*)gf_realloc(compositor->sel_buffer, sizeof(u16)*compositor->sel_buffer_alloc);
			}
			memmove(&compositor->sel_buffer[compositor->caret_pos+1], &compositor->sel_buffer[compositor->caret_pos], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos));
			compositor->sel_buffer[compositor->caret_pos] = unicode_char;
			compositor->sel_buffer_len++;
			compositor->caret_pos++;
			compositor->sel_buffer[compositor->sel_buffer_len] = 0;
#endif
		}
	} else if (event->type==GF_EVENT_KEYDOWN) {
		u32 prev_caret = compositor->caret_pos;
		switch (event->key.key_code) {
		/*end of edit mode*/
		case GF_KEY_ESCAPE:
			is_end = GF_TRUE;
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
				is_end = GF_TRUE;
				break;
			}
			return;
		case GF_KEY_BACKSPACE:
			if (compositor->caret_pos) {
				if (compositor->sel_buffer_len) compositor->sel_buffer_len--;
				memmove(&compositor->sel_buffer[compositor->caret_pos-1], &compositor->sel_buffer[compositor->caret_pos], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos+1));
				compositor->sel_buffer[compositor->sel_buffer_len]=0;
				compositor->caret_pos--;
				flush_text_node_edit(compositor, GF_FALSE);
			} else {
				load_text_node(compositor, 4);
			}
			return;
		case GF_KEY_DEL:
			if (compositor->caret_pos+1<compositor->sel_buffer_len) {
				if (compositor->sel_buffer_len) compositor->sel_buffer_len--;
				memmove(&compositor->sel_buffer[compositor->caret_pos+1], &compositor->sel_buffer[compositor->caret_pos+2], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos-1));
				compositor->sel_buffer[compositor->sel_buffer_len]=0;
				flush_text_node_edit(compositor, GF_FALSE);
				return;
			}
			break;
		case GF_KEY_ENTER:
			if (compositor->focus_text_type==4) {
				flush_text_node_edit(compositor, 2);
				break;
			}
			load_text_node(compositor, 3);
			return;
		default:
			return;
		}
		if (!is_end && compositor->sel_buffer) {
			if (compositor->caret_pos==prev_caret) return;
			memmove(&compositor->sel_buffer[prev_caret], &compositor->sel_buffer[prev_caret+1], sizeof(u16)*(compositor->sel_buffer_len-prev_caret));
			memmove(&compositor->sel_buffer[compositor->caret_pos+1], &compositor->sel_buffer[compositor->caret_pos], sizeof(u16)*(compositor->sel_buffer_len-compositor->caret_pos));
			compositor->sel_buffer[compositor->caret_pos]=GF_CARET_CHAR;
		}
	} else {
		return;
	}

	flush_text_node_edit(compositor, is_end);
}

static void toggle_keyboard(GF_Compositor * compositor, Bool do_show)
{
	GF_Event evt;
	memset(&evt, 0, sizeof(GF_Event));
	evt.type = do_show ? GF_EVENT_TEXT_EDITING_START : GF_EVENT_TEXT_EDITING_END;

	if (compositor->video_out) {
		GF_Err e = compositor->video_out->ProcessEvent(compositor->video_out, &evt);
		if (e == GF_OK) return;
	}
	gf_sc_user_event(compositor, &evt);
}

static Bool hit_node_editable(GF_Compositor *compositor, Bool check_focus_node)
{
#ifndef GPAC_DISABLE_SVG
	SVGAllAttributes atts;
#endif
	u32 tag;
	GF_Node *text = check_focus_node ? compositor->focus_node : compositor->hit_node;
	if (!text) {
		toggle_keyboard(compositor, GF_FALSE);
		return GF_FALSE;
	}
	if (compositor->hit_node==compositor->focus_node) return compositor->focus_text_type ? GF_TRUE : GF_FALSE;

	tag = gf_node_get_tag(text);

#ifndef GPAC_DISABLE_VRML
	switch (tag) {
	case TAG_MPEG4_Text:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Text:
#endif
	{
		M_FontStyle *fs = (M_FontStyle *) ((M_Text *)text)->fontStyle;
		if (!fs || !fs->style.buffer) return GF_FALSE;
		if (strstr(fs->style.buffer, "editable") || strstr(fs->style.buffer, "EDITABLE")) {
			compositor->focus_text_type = 3;
		} else if (strstr(fs->style.buffer, "simple_edit") || strstr(fs->style.buffer, "SIMPLE_EDIT")) {
			compositor->focus_text_type = 4;
		} else {
			toggle_keyboard(compositor, GF_FALSE);
			return GF_FALSE;
		}
		compositor->focus_node = text;
		toggle_keyboard(compositor, compositor->focus_text_type > 2 ? GF_TRUE : GF_FALSE);
		return GF_TRUE;
	}
	default:
		break;
	}
#endif /*GPAC_DISABLE_VRML*/
	if (tag <= GF_NODE_FIRST_DOM_NODE_TAG) return GF_FALSE;
#ifndef GPAC_DISABLE_SVG
	gf_svg_flatten_attributes((SVG_Element *)text, &atts);
	if (!atts.editable || !*atts.editable) return GF_FALSE;
	switch (tag) {
	case TAG_SVG_text:
	case TAG_SVG_textArea:
		compositor->focus_text_type = 1;
		break;
	case TAG_SVG_tspan:
		compositor->focus_text_type = 2;
		break;
	default:
		return GF_FALSE;
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
		compositor->focus_uses_dom_events = GF_TRUE;
	}
	compositor->hit_node = NULL;
	toggle_keyboard(compositor, compositor->focus_text_type > 0 ? GF_TRUE : GF_FALSE);
#endif
	return GF_TRUE;
}

#ifndef GPAC_DISABLE_SVG
static GF_Node *get_parent_focus(GF_Node *node, GF_List *hit_use_stack, u32 cur_idx)
{
	GF_Node *parent;
	GF_FieldInfo info;
	if (!node) return NULL;

	if (gf_node_get_attribute_by_tag(node, TAG_SVG_ATT_focusable, GF_FALSE, GF_FALSE, &info)==GF_OK) {
		if ( *(SVG_Focusable*)info.far_ptr == SVG_FOCUSABLE_TRUE) return node;
	}
	parent = gf_node_get_parent(node, 0);
	if (cur_idx) {
		GF_Node *n = (GF_Node*)gf_list_get(hit_use_stack, cur_idx-1);
		if (n==node) {
			parent = (GF_Node*)gf_list_get(hit_use_stack, cur_idx-2);
			if (cur_idx>1) cur_idx-=2;
			else cur_idx=0;
		}
	}
	return get_parent_focus(parent, hit_use_stack, cur_idx);
}
#endif


static Bool exec_event_dom(GF_Compositor *compositor, GF_Event *event)
{
#ifndef GPAC_DISABLE_SVG
	GF_DOM_Event evt;
	u32 cursor_type;
	Bool ret = GF_FALSE;

	cursor_type = GF_CURSOR_NORMAL;
	/*all mouse events*/
	if (event->type<=GF_EVENT_MOUSEWHEEL) {
		Fixed X = compositor->hit_world_point.x;
		Fixed Y = compositor->hit_world_point.y;
		/*flip back to origin at top-left*/
		if (compositor->visual->center_coords) {
			X += INT2FIX(compositor->visual->width)/2;
			Y = INT2FIX(compositor->visual->height)/2 - Y;
		}

		if (compositor->hit_node) {
			GF_Node *focus;
			Bool hit_changed = GF_FALSE;
			GF_Node *current_use = (GF_Node*)gf_list_last(compositor->hit_use_stack);
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
				hit_changed = GF_TRUE;
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
				if (focus) gf_sc_focus_switch_ring(compositor, GF_FALSE, focus, 1);
				else if (compositor->focus_node) gf_sc_focus_switch_ring(compositor, GF_FALSE, NULL, 1);

				break;
			case GF_EVENT_MOUSEUP:
				evt.type = GF_EVENT_MOUSEUP;
				evt.button = event->mouse.button;
				evt.detail = compositor->num_clicks;
				ret += gf_dom_event_fire_ex(compositor->grab_node, &evt, compositor->hit_use_stack);
				/*
				TODO quick- fix for iPhone as well
				TODO clean: figure out whether we use a mouse or a touch device - if touch device, remove this test
				*/
#if !defined(_WIN32_WCE) || !defined(GPAC_CONFIG_ANDROID)
				if ((compositor->grab_x == X) && (compositor->grab_y == Y))
#endif
				{
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
				gf_sc_focus_switch_ring(compositor, GF_FALSE, NULL, 1);

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
			evt.button = event->mouse.button;
			evt.new_scale = event->mouse.wheel_pos;
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

		/*dirty hack to simulate browserback*/
		if (event->key.key_code==GF_KEY_BACKSPACE && (event->key.flags & GF_KEY_MOD_CTRL)) {
			event->key.key_code = GF_KEY_BROWSERBACK;
		}

		if ((event->key.key_code>=GF_KEY_BROWSERBACK) && (event->key.key_code<=GF_KEY_BROWSERSTOP)) {
			target = NULL;
		}
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
			//case '\b':
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

#ifndef GPAC_DISABLE_VRML


Bool gf_sc_exec_event_vrml(GF_Compositor *compositor, GF_Event *ev)
{
	u32 res = 0;
	GF_SensorHandler *hs;
	GF_List *tmp;
	u32 i, count, stype;

	/*reset previous composite texture*/
	if (compositor->prev_hit_appear != compositor->hit_appear) {
		if (compositor->prev_hit_appear) {
			//reset previ appear node to avoid potential recursions
			GF_Node *hit_appear = compositor->prev_hit_appear;
			compositor->prev_hit_appear = NULL;
			compositor_compositetexture_handle_event(compositor, hit_appear, ev, GF_TRUE);
			//restore if we have a hit
			if (compositor->grabbed_sensor)
				compositor->prev_hit_appear = hit_appear;
		}
	}

	/*composite texture*/
	if (compositor->hit_appear) {
		GF_Node *appear = compositor->hit_appear;
		if (compositor_compositetexture_handle_event(compositor, compositor->hit_appear, ev, GF_FALSE)) {
			if (compositor->hit_appear) compositor->prev_hit_appear = appear;


			compositor->grabbed_sensor = 0;
			/*check if we have grabbed sensors*/
			count = gf_list_count(compositor->previous_sensors);
			for (i=0; i<count; i++) {
				hs = (GF_SensorHandler*)gf_list_get(compositor->previous_sensors, i);
				/*if sensor is grabbed, add it to the list of active sensor for next pick*/
				if (hs->grabbed) {
					hs->OnUserEvent(hs, GF_FALSE, GF_TRUE, ev, compositor);
					gf_list_add(compositor->sensors, hs);
					compositor->grabbed_sensor = 1;
				}
			}

			return GF_TRUE;
		}
//		compositor->prev_hit_appear = compositor->grabbed_sensor ? compositor->hit_appear : NULL;
		compositor->prev_hit_appear = compositor->hit_appear;
	}

	count = gf_list_count(compositor->sensors);
	/*if we have a hit node at the compositor level, use "touch" as default cursor - this avoid
	resetting the cursor when the picked node is a DOM node in a composite texture*/
//	stype = (compositor->hit_node!=NULL) ? GF_CURSOR_TOUCH : GF_CURSOR_NORMAL;
	stype = GF_CURSOR_NORMAL;
	for (i=0; i<count; i++) {
		GF_Node *keynav;
		Bool check_anchor = GF_FALSE;
		hs = (GF_SensorHandler*)gf_list_get(compositor->sensors, i);

		/*try to remove this sensor from the previous sensor list*/
		gf_list_del_item(compositor->previous_sensors, hs);
		if (gf_node_get_id(hs->sensor))
			stype = gf_node_get_tag(hs->sensor);

		keynav = gf_scene_get_keynav(gf_node_get_graph(hs->sensor), hs->sensor);
		if (keynav) gf_sc_change_key_navigator(compositor, keynav);

		/*call the sensor LAST, as this may triger a destroy of the scene the sensor is in
		this is only true for anchors, as other other sensors output events are queued as routes until next pass*/
		res += hs->OnUserEvent(hs, GF_TRUE, GF_FALSE, ev, compositor);
		if (stype == TAG_MPEG4_Anchor) check_anchor = GF_TRUE;
#ifndef GPAC_DISABLE_X3D
		else if (stype == TAG_X3D_Anchor) check_anchor = GF_TRUE;
#endif
		if (check_anchor) {
			/*subscene with active sensor has been deleted, we cannot continue process the sensors stack*/
			if (count != gf_list_count(compositor->sensors))
				break;
		}
	}
	compositor->grabbed_sensor = 0;
	/*check if we have grabbed sensors*/
	count = gf_list_count(compositor->previous_sensors);
	for (i=0; i<count; i++) {
		hs = (GF_SensorHandler*)gf_list_get(compositor->previous_sensors, i);
		res += hs->OnUserEvent(hs, GF_FALSE, GF_FALSE, ev, compositor);
		/*if sensor is grabbed, add it to the list of active sensor for next pick*/
		if (hs->grabbed) {
			gf_list_add(compositor->sensors, hs);
			compositor->grabbed_sensor = 1;
		}
		stype = gf_node_get_tag(hs->sensor);
	}
	gf_list_reset(compositor->previous_sensors);

	/*switch sensors*/
	tmp = compositor->sensors;
	compositor->sensors = compositor->previous_sensors;
	compositor->previous_sensors = tmp;

	/*and set cursor*/
	if (compositor->sensor_type != GF_CURSOR_COLLIDE) {
		switch (stype) {
		case TAG_MPEG4_Anchor:
			stype = GF_CURSOR_ANCHOR;
			break;
		case TAG_MPEG4_PlaneSensor2D:
		case TAG_MPEG4_PlaneSensor:
			stype = GF_CURSOR_PLANE;
			break;
		case TAG_MPEG4_CylinderSensor:
		case TAG_MPEG4_DiscSensor:
		case TAG_MPEG4_SphereSensor:
			stype = GF_CURSOR_ROTATE;
			break;
		case TAG_MPEG4_ProximitySensor2D:
		case TAG_MPEG4_ProximitySensor:
			stype = GF_CURSOR_PROXIMITY;
			break;
		case TAG_MPEG4_TouchSensor:
			stype = GF_CURSOR_TOUCH;
			break;
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_Anchor:
			stype = GF_CURSOR_ANCHOR;
			break;
		case TAG_X3D_PlaneSensor:
			stype = GF_CURSOR_PLANE;
			break;
		case TAG_X3D_CylinderSensor:
		case TAG_X3D_SphereSensor:
			stype = GF_CURSOR_ROTATE;
			break;
		case TAG_X3D_ProximitySensor:
			stype = GF_CURSOR_PROXIMITY;
			break;
		case TAG_X3D_TouchSensor:
			stype = GF_CURSOR_TOUCH;
			break;
#endif

		default:
			stype = GF_CURSOR_NORMAL;
			break;
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
	if (res) {
		GF_SceneGraph *sg;
		/*apply event cascade - this is needed for cases where several events are processed between
		2 simulation tick. If we don't flush the routes stack, the result will likely be wrong
		*/
		gf_sg_activate_routes(compositor->scene);
		i = 0;
		while ((sg = (GF_SceneGraph*)gf_list_enum(compositor->extra_scenes, &i))) {
			gf_sg_activate_routes(sg);
		}
		return 1;
	}
	return GF_FALSE;
}


static Bool exec_vrml_key_event(GF_Compositor *compositor, GF_Node *node, GF_Event *ev, Bool is_focus_out)
{
	GF_SensorHandler *hdl = NULL;
	GF_ChildNodeItem *child;
	u32 ret = 0;
	if (!node) node = compositor->focus_node;
	if (!node) return GF_FALSE;

	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Text:
		return GF_FALSE;
	case TAG_MPEG4_Layout:
		hdl = compositor_mpeg4_layout_get_sensor_handler(node);
		break;
	case TAG_MPEG4_Anchor:
		hdl = compositor_mpeg4_get_sensor_handler(node);
		break;
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Text:
		return GF_FALSE;
	case TAG_X3D_Anchor:
		hdl = compositor_mpeg4_get_sensor_handler(node);
		break;
#endif
	}
	child = ((GF_ParentNode*)node)->children;
	if (hdl) {
		ret += hdl->OnUserEvent(hdl, is_focus_out ? GF_FALSE : GF_TRUE, GF_FALSE, ev, compositor);
	} else {
		while (child) {
			hdl = compositor_mpeg4_get_sensor_handler(child->node);
			if (hdl) {
				ret += hdl->OnUserEvent(hdl, is_focus_out ? GF_FALSE : GF_TRUE, GF_FALSE, ev, compositor);
			}
			child = child->next;
		}
	}
	return ret ? GF_TRUE : GF_FALSE;
}

#endif /*GPAC_DISABLE_VRML*/

Bool visual_execute_event(GF_VisualManager *visual, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children)
{
	Bool ret;
	Bool reset_sel = GF_FALSE;
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
					reset_sel = GF_TRUE;
				}
			}
			else if (ev->type==GF_EVENT_MOUSEDOWN) {
				reset_sel = GF_TRUE;
			}
		} else if (compositor->edited_text) {
			if (ev->type==GF_EVENT_MOUSEDOWN)
				reset_sel = GF_TRUE;
		}
		if (ev->type==GF_EVENT_MOUSEUP) {
			if (hit_node_editable(compositor, GF_FALSE)) {
				compositor->text_selection = NULL;
				exec_text_input(compositor, NULL);
				return GF_TRUE;
			}
#if 0
			else if (!compositor->focus_node) {
				gf_sc_focus_switch_ring(compositor, 0, gf_sg_get_root_node(compositor->scene), 1);
			}
#endif
		}
		if (reset_sel) {
			flush_text_node_edit(compositor, GF_TRUE);

			compositor->store_text_state = GF_SC_TSEL_RELEASED;
			compositor->text_selection = NULL;
			if (compositor->selected_text) gf_free(compositor->selected_text);
			compositor->selected_text = NULL;
			if (compositor->sel_buffer) gf_free(compositor->sel_buffer);
			compositor->sel_buffer = NULL;
			compositor->sel_buffer_alloc = 0;
			compositor->sel_buffer_len = 0;

			gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
			compositor->text_edit_changed = GF_TRUE;
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

	tr_state->pick_x = ev->mouse.x;
	tr_state->pick_y = ev->mouse.y;


#ifndef GPAC_DISABLE_3D
	if (visual->type_3d)
		visual_3d_pick_node(visual, tr_state, ev, children);
	else
#endif
		visual_2d_pick_node(visual, tr_state, ev, children);

	gf_list_reset(tr_state->vrml_sensors);

	if (exec_text_selection(compositor, ev))
		return GF_TRUE;

	if (compositor->hit_use_dom_events) {
		ret = exec_event_dom(compositor, ev);
		if (ret) return GF_TRUE;
		/*no vrml sensors above*/
		if (!gf_list_count(compositor->sensors) && !gf_list_count(compositor->previous_sensors))
			return GF_FALSE;
	}
#ifndef GPAC_DISABLE_VRML
	return gf_sc_exec_event_vrml(compositor, ev);
#else
	return 0;
#endif
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
	case GF_KEY_LEFT:
		focus = atts.nav_left;
		break;
	case GF_KEY_RIGHT:
		focus = atts.nav_right;
		break;
	case GF_KEY_UP:
		focus = atts.nav_up;
		break;
	case GF_KEY_DOWN:
		focus = atts.nav_down;
		break;
	default:
		return 0;
	}
	if (!focus) return 0;

	if (focus->type==SVG_FOCUS_SELF) return 0;
	if (focus->type==SVG_FOCUS_AUTO) return 0;
	if (!focus->target.target) {
		if (!focus->target.string) return 0;
		focus->target.target = gf_sg_find_node_by_name(compositor->scene, focus->target.string+1);
	}
	n = (GF_Node*)focus->target.target;

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
#ifndef GPAC_DISABLE_SVG
static Bool is_focus_target(GF_Node *elt)
{
	u32 i, count;
	u32 tag = gf_node_get_tag(elt);
	switch (tag) {
	case TAG_SVG_a:
		return GF_TRUE;

#ifndef GPAC_DISABLE_VRML
	case TAG_MPEG4_Anchor:
		return GF_TRUE;
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Anchor:
		return GF_TRUE;
#endif
#endif

	default:
		break;
	}
	if (tag<=GF_NODE_FIRST_DOM_NODE_TAG) return GF_FALSE;

	count = gf_dom_listener_count(elt);
	for (i=0; i<count; i++) {
		GF_FieldInfo info;
		GF_Node *l = gf_dom_listener_get(elt, i);
		if (gf_node_get_attribute_by_tag(l, TAG_XMLEV_ATT_event, GF_FALSE, GF_FALSE, &info)==GF_OK) {
			switch ( ((XMLEV_Event*)info.far_ptr)->type) {
			case GF_EVENT_FOCUSIN:
			case GF_EVENT_FOCUSOUT:
			case GF_EVENT_ACTIVATE:
			/*although this is not in the SVGT1.2 spec, we also enable focus switching if key events are listened on the element*/
			case GF_EVENT_KEYDOWN:
			case GF_EVENT_KEYUP:
			case GF_EVENT_LONGKEYPRESS:
				return GF_TRUE;
			default:
				break;
			}
		}
	}
	return GF_FALSE;
}
#endif /*GPAC_DISABLE_SVG*/

#define CALL_SET_FOCUS(__child)	{	\
	gf_list_add(compositor->focus_ancestors, elt);	\
	n = set_focus(compositor, __child, current_focus, prev_focus);	\
	if (n) {	\
		gf_node_set_cyclic_traverse_flag(elt, 0);	\
		return n;	\
	}	\
	gf_list_rem_last(compositor->focus_ancestors);	\
	gf_node_set_cyclic_traverse_flag(elt, 0);\
	return NULL;	\
	}	\
 
#ifndef GPAC_DISABLE_SVG
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
#endif // GPAC_DISABLE_SVG

static GF_Node *set_focus(GF_Compositor *compositor, GF_Node *elt, Bool current_focus, Bool prev_focus)
{
	u32 tag;
	GF_ChildNodeItem *child = NULL;
	GF_Node *use_node = NULL;
	GF_Node *anim_node = NULL;
	GF_Node *n;

	if (!elt) return NULL;

	/*all return in this function shall be preceeded with gf_node_set_cyclic_traverse_flag(elt, 0)
	this ensures that we don't go into cyclic references when moving focus, hence stack overflow*/
	if (! gf_node_set_cyclic_traverse_flag(elt, GF_TRUE)) return NULL;

	tag = gf_node_get_tag(elt);
	if (tag <= GF_NODE_FIRST_DOM_NODE_TAG) {
		switch (tag) {
#ifndef GPAC_DISABLE_VRML
		case TAG_MPEG4_Transform:
		{
			M_Transform *tr=(M_Transform *)elt;
			if (!tr->scale.x || !tr->scale.y || !tr->scale.z) {
				gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
				return NULL;
			}
			goto test_grouping;
		}
		case TAG_MPEG4_Transform2D:
		{
			M_Transform2D *tr=(M_Transform2D *)elt;
			if (!tr->scale.x || !tr->scale.y) {
				gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
				return NULL;
			}
			goto test_grouping;
		}
		case TAG_MPEG4_Layer3D:
		case TAG_MPEG4_Layer2D:
		{
			M_Layer2D *l=(M_Layer2D *)elt;
			if (!l->size.x || !l->size.y) {
				gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
				return NULL;
			}
			goto test_grouping;
		}
		case TAG_MPEG4_Form:
		case TAG_MPEG4_TransformMatrix2D:
		case TAG_MPEG4_Group:
		case TAG_MPEG4_Billboard:
		case TAG_MPEG4_Collision:
		case TAG_MPEG4_LOD:
		case TAG_MPEG4_OrderedGroup:
		case TAG_MPEG4_ColorTransform:
		case TAG_MPEG4_PathLayout:
		case TAG_MPEG4_Anchor:
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_Group:
		case TAG_X3D_Transform:
		case TAG_X3D_Billboard:
		case TAG_X3D_Collision:
		case TAG_X3D_LOD:
		case TAG_X3D_Anchor:
#endif

test_grouping:
			if (!current_focus) {
				/*get the base grouping stack (*/
				BaseGroupingStack *grp = (BaseGroupingStack*)gf_node_get_private(elt);
				if (grp && (grp->flags & (GROUP_HAS_SENSORS | GROUP_IS_ANCHOR) )) {
					gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
					return elt;
				}
			}
			break;
		case TAG_MPEG4_Switch:
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_Switch:
#endif
		{
			s32 i, wc;
#ifndef GPAC_DISABLE_X3D
			if (tag==TAG_X3D_Switch) {
				child = ((X_Switch*)elt)->children;
				wc = ((X_Switch*)elt)->whichChoice;
			} else
#endif
			{
				child = ((M_Switch*)elt)->choice;
				wc = ((M_Switch*)elt)->whichChoice;
			}
			if (wc==-1) {
				gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
				return NULL;
			}
			i=0;
			while (child) {
				if (i==wc) CALL_SET_FOCUS(child->node);
				child = child->next;
			}
			gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
			return NULL;
		}

		case TAG_MPEG4_Text:
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_Text:
#endif
			gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);

			if (!current_focus) {
				M_FontStyle *fs = (M_FontStyle *) ((M_Text *)elt)->fontStyle;

				if (!fs || !fs->style.buffer) return NULL;

				if (strstr(fs->style.buffer, "editable") || strstr(fs->style.buffer, "EDITABLE")) {
					compositor->focus_text_type = 3;
				} else if (strstr(fs->style.buffer, "simple_edit") || strstr(fs->style.buffer, "SIMPLE_EDIT")) {
					compositor->focus_text_type = 4;
				} else {
					return NULL;
				}
				return elt;
			}
			return NULL;
		case TAG_MPEG4_Layout:
		{
			M_Layout *l=(M_Layout*)elt;
			if (!l->size.x || !l->size.y) {
				gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
				return NULL;
			}
			if (!current_focus && (compositor_mpeg4_layout_get_sensor_handler(elt)!=NULL)) {
				gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
				return elt;
			}
		}
		break;

		case TAG_ProtoNode:
			/*hardcoded proto acting as a grouping node*/
			if (gf_node_proto_is_grouping(elt)) {
				GF_FieldInfo info;
				if (!current_focus) {
					/*get the base grouping stack (*/
					BaseGroupingStack *grp = (BaseGroupingStack*)gf_node_get_private(elt);
					if (grp && (grp->flags & (GROUP_HAS_SENSORS | GROUP_IS_ANCHOR) )) {
						gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
						return elt;
					}
				}
				if ( (gf_node_get_field_by_name(elt, "children", &info) != GF_OK) || (info.fieldType != GF_SG_VRML_MFNODE)) {
					gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
					return NULL;
				}
				child = *(GF_ChildNodeItem **) info.far_ptr;
			} else {
				CALL_SET_FOCUS(gf_node_get_proto_root(elt));
			}
			break;

		case TAG_MPEG4_Inline:
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_Inline:
#endif
			CALL_SET_FOCUS(gf_scene_get_subscene_root(elt));

		case TAG_MPEG4_Shape:
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_Shape:
#endif

			gf_list_add(compositor->focus_ancestors, elt);
			n = set_focus(compositor, ((M_Shape*)elt)->geometry, current_focus, prev_focus);
			if (!n) n = set_focus(compositor, ((M_Shape*)elt)->appearance, current_focus, prev_focus);
			if (n) {
				gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
				return n;
			}
			gf_list_rem_last(compositor->focus_ancestors);
			gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
			return NULL;

		case TAG_MPEG4_Appearance:
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_Appearance:
#endif
			CALL_SET_FOCUS(((M_Appearance*)elt)->texture);

		case TAG_MPEG4_CompositeTexture2D:
		case TAG_MPEG4_CompositeTexture3D:
			/*CompositeTextures are not grouping nodes per say*/
			child = ((GF_ParentNode*)elt)->children;
			while (child) {
				if (compositor_mpeg4_get_sensor_handler(child->node) != NULL) {
					gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
					return elt;
				}
				child = child->next;
			}
			break;

#endif /*GPAC_DISABLE_VRML*/

		default:
			gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
			return NULL;
		}
		if (!child)
			child = ((GF_ParentNode*)elt)->children;
	} else {
#ifndef GPAC_DISABLE_SVG
		SVGAllAttributes atts;

		if (tag==TAG_SVG_defs) {
			gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
			return NULL;
		}
		gf_svg_flatten_attributes((SVG_Element *)elt, &atts);

		if (atts.display && (*atts.display==SVG_DISPLAY_NONE)) {
			gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
			return NULL;
		}
		if (!current_focus) {
			Bool is_auto = GF_TRUE;
			if (atts.focusable) {
				if (*atts.focusable==SVG_FOCUSABLE_TRUE) {
					gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
					return elt;
				}
				if (*atts.focusable==SVG_FOCUSABLE_FALSE) is_auto = GF_FALSE;
			}
			if (is_auto && is_focus_target(elt)) {
				gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
				return elt;
			}

			if (atts.editable && *atts.editable) {
				switch (tag) {
				case TAG_SVG_text:
				case TAG_SVG_textArea:
					compositor->focus_text_type = 1;
					gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
					return elt;
				case TAG_SVG_tspan:
					compositor->focus_text_type = 2;
					gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
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
					gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
					return elt;
				case SVG_FOCUS_IRI:
					if (!atts.nav_prev->target.target) {
						if (!atts.nav_prev->target.string) {
							gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
							return NULL;
						}
						atts.nav_prev->target.target = gf_sg_find_node_by_name(compositor->scene, atts.nav_prev->target.string+1);
					}
					if (atts.nav_prev->target.target) {
						rebuild_focus_ancestor(compositor, atts.nav_prev->target.target);
						gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
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
					gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
					return elt;
				case SVG_FOCUS_IRI:
					if (!atts.nav_next->target.target) {
						if (!atts.nav_next->target.string) {
							gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
							return NULL;
						}
						atts.nav_next->target.target = gf_sg_find_node_by_name(compositor->scene, atts.nav_next->target.string+1);
					}
					if (atts.nav_next->target.target) {
						rebuild_focus_ancestor(compositor, atts.nav_next->target.target);
						gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
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
		if (current_focus) {
			gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
			return NULL;
		}
		gf_list_add(compositor->focus_ancestors, elt);
		count = gf_node_list_get_count(child);
		for (i=count; i>0; i--) {
			/*get in the subtree*/
			n = gf_node_list_get_child( child, i-1);
			n = set_focus(compositor, n, GF_FALSE, GF_TRUE);
			if (n) {
				gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
				return n;
			}
		}
	} else {
		/*check all children */
		gf_list_add(compositor->focus_ancestors, elt);
		while (child) {
			/*get in the subtree*/
			n = set_focus(compositor, child->node, GF_FALSE, GF_FALSE);
			if (n) {
				gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
				return n;
			}
			child = child->next;
		}
	}
	if (use_node) {
		gf_list_add(compositor->focus_use_stack, use_node);
		gf_list_add(compositor->focus_use_stack, elt);
		n = set_focus(compositor, use_node, GF_FALSE, GF_FALSE);
		if (n) {
			compositor->focus_used = elt;
			gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
			return n;
		}
		gf_list_rem_last(compositor->focus_use_stack);
		gf_list_rem_last(compositor->focus_use_stack);
	} else if (anim_node) {
		n = set_focus(compositor, anim_node, GF_FALSE, GF_FALSE);
		if (n) {
			gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
			return n;
		}
	}

	gf_list_rem_last(compositor->focus_ancestors);
	gf_node_set_cyclic_traverse_flag(elt, GF_FALSE);
	return NULL;
}

static GF_Node *browse_parent_for_focus(GF_Compositor *compositor, GF_Node *elt, Bool prev_focus)
{
	u32 tag;
	s32 idx = 0;
	GF_ChildNodeItem *child;
	GF_Node *n;
	GF_Node *par;

	par = (GF_Node*)gf_list_last(compositor->focus_ancestors);
	if (!par) return NULL;

	tag = gf_node_get_tag(par);
	if (tag <= GF_NODE_FIRST_DOM_NODE_TAG) {
		switch (tag) {
#ifndef GPAC_DISABLE_VRML
		case TAG_MPEG4_Group:
		case TAG_MPEG4_Transform:
		case TAG_MPEG4_Billboard:
		case TAG_MPEG4_Collision:
		case TAG_MPEG4_LOD:
		case TAG_MPEG4_OrderedGroup:
		case TAG_MPEG4_Transform2D:
		case TAG_MPEG4_TransformMatrix2D:
		case TAG_MPEG4_ColorTransform:
		case TAG_MPEG4_Layer3D:
		case TAG_MPEG4_Layer2D:
		case TAG_MPEG4_Layout:
		case TAG_MPEG4_PathLayout:
		case TAG_MPEG4_Form:
		case TAG_MPEG4_Anchor:
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_Anchor:
		case TAG_X3D_Group:
		case TAG_X3D_Transform:
		case TAG_X3D_Billboard:
		case TAG_X3D_Collision:
		case TAG_X3D_LOD:
#endif
		case TAG_MPEG4_CompositeTexture2D:
		case TAG_MPEG4_CompositeTexture3D:
			child = ((GF_ParentNode*)par)->children;
			break;
		case TAG_ProtoNode:
			/*hardcoded proto acting as a grouping node*/
			if (gf_node_proto_is_grouping(par)) {
				GF_FieldInfo info;
				if ((gf_node_get_field_by_name(par, "children", &info) == GF_OK)
				        && (info.fieldType == GF_SG_VRML_MFNODE)
				   ) {
					child = *(GF_ChildNodeItem **) info.far_ptr;
					break;
				}
			}
			/*fall through*/

#endif	/*GPAC_DISABLE_VRML*/

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
		u32 i;
		/*!! this may happen when walking up PROTO nodes !!*/
		for (i=idx; i>0; i--) {
			n = gf_node_list_get_child(child, i-1);
			/*get in the subtree*/
			n = set_focus(compositor, n, GF_FALSE, GF_TRUE);
			if (n) return n;
		}
	} else {
		/*!! this may happen when walking up PROTO nodes !!*/
		while (child) {
			if (idx<0) {
				/*get in the subtree*/
				n = set_focus(compositor, child->node, GF_FALSE, GF_FALSE);
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

GF_EXPORT
u32 gf_sc_focus_switch_ring(GF_Compositor *compositor, Bool move_prev, GF_Node *focus, u32 force_focus)
{
	Bool current_focus = GF_TRUE;
#ifndef GPAC_DISABLE_SVG
	Bool prev_uses_dom_events;
	GF_Node *prev_use;
#endif
	u32 ret = 0;
	GF_List *cloned_use = NULL;
	GF_Node *n, *prev;

	compositor->focus_text_type = 0;
	prev = compositor->focus_node;
#ifndef GPAC_DISABLE_SVG
	prev_use = compositor->focus_used;
	prev_uses_dom_events = compositor->focus_uses_dom_events;
#endif
	compositor->focus_uses_dom_events = GF_FALSE;

	if (!compositor->focus_node) {
		compositor->focus_node = (force_focus==2) ? focus : gf_sg_get_root_node(compositor->scene);
		gf_list_reset(compositor->focus_ancestors);
		if (!compositor->focus_node) return 0;
		current_focus = GF_FALSE;
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
		if (force_focus==2) {
			current_focus = GF_FALSE;
			n = set_focus(compositor, focus, current_focus, move_prev);
			if (!n) n = browse_parent_for_focus(compositor, focus, move_prev);
		}
	} else {
		n = set_focus(compositor, compositor->focus_node, current_focus, move_prev);
		if (!n) n = browse_parent_for_focus(compositor, compositor->focus_node, move_prev);

		if (!n) {
			if (!prev) n = gf_sg_get_root_node(compositor->scene);
			gf_list_reset(compositor->focus_ancestors);
		}
	}

	if (n && gf_node_get_tag(n)>=GF_NODE_FIRST_DOM_NODE_TAG) {
		compositor->focus_uses_dom_events = GF_TRUE;
	}
	compositor->focus_node = n;

	ret = 0;
#ifndef GPAC_DISABLE_SVG
	if ((prev != compositor->focus_node) || (prev_use != compositor->focus_used)) {
		GF_DOM_Event evt;
		GF_Event ev;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		memset(&ev, 0, sizeof(GF_Event));
		ev.type = GF_EVENT_KEYDOWN;
		ev.key.key_code = move_prev ? GF_KEY_LEFT : GF_KEY_RIGHT;

		if (prev) {
			if (prev_uses_dom_events) {
				evt.bubbles = 1;
				evt.type = GF_EVENT_FOCUSOUT;
				gf_dom_event_fire_ex(prev, &evt, cloned_use);
			}
#ifndef GPAC_DISABLE_VRML
			else {
				exec_vrml_key_event(compositor, prev, &ev, GF_TRUE);
			}
#endif
		}
		if (compositor->focus_node) {
			/*the event is already handled, even though no listeners may be present*/
			ret = 1;
			if (compositor->focus_uses_dom_events) {
				evt.bubbles = 1;
				evt.type = GF_EVENT_FOCUSIN;
				gf_dom_event_fire_ex(compositor->focus_node, &evt, compositor->focus_use_stack);
			}
#ifndef GPAC_DISABLE_VRML
			else {
				exec_vrml_key_event(compositor, NULL, &ev, GF_FALSE);
			}
#endif
		}
		/*because of offscreen caches and composite texture, we must invalidate subtrees of previous and new focus to force a redraw*/
		if (prev) gf_node_dirty_set(prev, GF_SG_NODE_DIRTY, GF_TRUE);
		if (compositor->focus_node) gf_node_dirty_set(compositor->focus_node, GF_SG_NODE_DIRTY, GF_TRUE);
		/*invalidate in case we draw focus rect*/
		gf_sc_invalidate(compositor, NULL);
	}
#endif /*GPAC_DISABLE_SVG*/
	if (cloned_use) gf_list_del(cloned_use);

	if (hit_node_editable(compositor, GF_TRUE)) {
		compositor->text_selection = NULL;
		exec_text_input(compositor, NULL);
		/*invalidate parent graphs*/
		gf_node_dirty_set(compositor->focus_node, GF_SG_NODE_DIRTY, GF_TRUE);
	}
	return ret;
}

Bool gf_sc_execute_event(GF_Compositor *compositor, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children)
{
	/*filter mouse events and other events (key...)*/
	if (ev->type>GF_EVENT_MOUSEWHEEL) {
		Bool ret = GF_FALSE;
		/*send text edit*/
		if (compositor->edited_text) {
			exec_text_input(compositor, ev);
			if (compositor->edited_text) return GF_TRUE;
			/*if text is no longer edited, this is focus change so process as usual*/
		}
		/*FIXME - this is not working for mixed docs*/
		if (compositor->focus_uses_dom_events)
			ret = exec_event_dom(compositor, ev);
#ifndef GPAC_DISABLE_VRML
		else
			ret = exec_vrml_key_event(compositor, compositor->focus_node, ev, GF_FALSE);
#endif

		if (ev->type==GF_EVENT_KEYDOWN) {
			switch (ev->key.key_code) {
			case GF_KEY_ENTER:
				if ((0) && compositor->focus_text_type) {
					exec_text_input(compositor, NULL);
					ret = GF_TRUE;
#ifndef GPAC_DISABLE_VRML
				} else if (compositor->keynav_node && ((M_KeyNavigator*)compositor->keynav_node)->select) {
					gf_sc_change_key_navigator(compositor, ((M_KeyNavigator*)compositor->keynav_node)->select);
					ret = GF_TRUE;
#endif
				}
				break;
			case GF_KEY_TAB:
				ret += gf_sc_focus_switch_ring(compositor, (ev->key.flags & GF_KEY_MOD_SHIFT) ? 1 : 0, NULL, 0);
				break;
			case GF_KEY_UP:
			case GF_KEY_DOWN:
			case GF_KEY_LEFT:
			case GF_KEY_RIGHT:
				if (compositor->focus_uses_dom_events) {
					ret += gf_sc_svg_focus_navigate(compositor, ev->key.key_code);
				}
#ifndef GPAC_DISABLE_VRML
				else if (compositor->keynav_node) {
					GF_Node *next_nav = NULL;
					switch (ev->key.key_code) {
					case GF_KEY_UP:
						next_nav = ((M_KeyNavigator*)compositor->keynav_node)->up;
						break;
					case GF_KEY_DOWN:
						next_nav = ((M_KeyNavigator*)compositor->keynav_node)->down;
						break;
					case GF_KEY_LEFT:
						next_nav = ((M_KeyNavigator*)compositor->keynav_node)->left;
						break;
					case GF_KEY_RIGHT:
						next_nav = ((M_KeyNavigator*)compositor->keynav_node)->right;
						break;
					}
					if (next_nav) {
						gf_sc_change_key_navigator(compositor, next_nav);
						ret = GF_TRUE;
					}
				}
#endif
				break;
			}
		}
		return ret;
	}
	/*pick even, call visual handler*/
	return visual_execute_event(compositor->visual, tr_state, ev, children);
}

static Bool forward_event(GF_Compositor *compositor, GF_Event *ev, Bool consumed)
{
	Bool ret = gf_filter_forward_gf_event(compositor->filter, ev, consumed, GF_FALSE);

	if (consumed) return GF_FALSE;

	if ((ev->type==GF_EVENT_MOUSEUP) && (ev->mouse.button==GF_MOUSE_LEFT)) {
		u32 now;
		GF_Event event;
		/*emulate doubleclick unless in step mode*/
		now = gf_sys_clock();
		if (!compositor->step_mode && (now - compositor->last_click_time < DOUBLECLICK_TIME_MS)) {
			event.type = GF_EVENT_DBLCLICK;
			event.mouse.key_states = compositor->key_states;
			event.mouse.x = ev->mouse.x;
			event.mouse.y = ev->mouse.y;
			ret += gf_sc_send_event(compositor, &event);
		}
		compositor->last_click_time = now;
	}
	return ret ? GF_TRUE : GF_FALSE;
}


Bool gf_sc_exec_event(GF_Compositor *compositor, GF_Event *evt)
{
	s32 x=0, y=0;
	Bool switch_coords = GF_FALSE;
	Bool ret = GF_FALSE;
	if (evt->type<=GF_EVENT_MOUSEWHEEL) {
		if (compositor->sgaze) {
			compositor->gaze_x = evt->mouse.x;
			compositor->gaze_y = evt->mouse.y;
		}
#ifndef GPAC_DISABLE_3D
		else if ((compositor->visual->autostereo_type==GF_3D_STEREO_SIDE) || (compositor->visual->autostereo_type==GF_3D_STEREO_HEADSET)) {
			if (evt->mouse.x > compositor->visual->camera.proj_vp.width)
				evt->mouse.x -= FIX2INT(compositor->visual->camera.proj_vp.width);
			evt->mouse.x *= 2;
		}
		else if (compositor->visual->autostereo_type==GF_3D_STEREO_TOP) {
			if (evt->mouse.y > compositor->visual->camera.proj_vp.height)
				evt->mouse.y -= FIX2INT(compositor->visual->camera.proj_vp.height);
			evt->mouse.y *= 2;
		}
#endif

		if (compositor->visual->center_coords) {
			x = evt->mouse.x;
			y = evt->mouse.y;
			evt->mouse.x = evt->mouse.x - compositor->display_width/2;
			evt->mouse.y = compositor->display_height/2 - evt->mouse.y;
			switch_coords = GF_TRUE;
		}
	}

	/*process regular events except if navigation is grabbed*/
	if ( (compositor->navigation_state<2) && (compositor->interaction_level & GF_INTERACT_NORMAL)) {
		Bool res;
		res = gf_sc_execute_event(compositor, compositor->traverse_state, evt, NULL);
		if (res) {
			compositor->navigation_state = 0;
			ret = GF_TRUE;
		}
	}
	if (switch_coords) {
		evt->mouse.x = x;
		evt->mouse.y = y;

	}

	if (!ret) {
#ifndef GPAC_DISABLE_3D
		/*remember active layer on mouse click - may be NULL*/
		if ((evt->type==GF_EVENT_MOUSEDOWN) && (evt->mouse.button==GF_MOUSE_LEFT))
			compositor->active_layer = compositor->traverse_state->layer3d;
#endif

        ret = forward_event(compositor, evt, ret);
    }
    //if event is consumed before forwarding don't apply navigation
    else {
        forward_event(compositor, evt, ret);
    }
        
	if (!ret) {
		/*process navigation events*/
		if (compositor->interaction_level & GF_INTERACT_NAVIGATION)
			ret = compositor_handle_navigation(compositor, evt);
	}
	return ret;
}

void gf_sc_change_key_navigator(GF_Compositor *sr, GF_Node *n)
{
#ifndef GPAC_DISABLE_VRML
	GF_Node *par;
	M_KeyNavigator *kn;

	gf_list_reset(sr->focus_ancestors);

	if (sr->keynav_node) {
		kn = (M_KeyNavigator*)sr->keynav_node;
		kn->focusSet = 0;
		gf_node_event_out(sr->keynav_node, 9/*"focusSet"*/);
	}
	sr->keynav_node = n;
	kn = (M_KeyNavigator*)n;
	if (n) {
		kn->focusSet = 1;
		gf_node_event_out(sr->keynav_node, 9/*"focusSet"*/);
	}

	par = n ? kn->sensor : NULL;
	if (par) par = gf_node_get_parent(par, 0);

	gf_sc_focus_switch_ring(sr, GF_FALSE, par, 1);
#endif
}

void gf_sc_key_navigator_del(GF_Compositor *sr, GF_Node *n)
{
	if (sr->keynav_node==n) {
		sr->keynav_node = NULL;
	}
}



