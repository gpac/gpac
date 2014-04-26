/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2010-2012
 *			All rights reserved
 *
 *  This file is part of GPAC / Test Suite Validator Recorder sub-project
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

#include <gpac/modules/term_ext.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/internal/media_dev.h>
#include <gpac/xml.h>
#include <gpac/options.h>

typedef struct __validation_module
{
	GF_Terminal *term;

	Bool is_recording;
	char *prev_fps;
	char *prev_alias;

	/* Clock used to synchronize events in recording and playback*/
	GF_Clock *ck;

	/* Next event to process */
	Bool next_event_snapshot;
	GF_Event next_event;
	u32 xvs_event_index;
	u32 next_time;
	Bool evt_loaded;

	GF_VideoListener video_listener;

	/* XML Validation List (the list of files to be tested) */
	char *xvl_filename;
	GF_DOMParser *xvl_parser;
	GF_XMLNode *xvl_node;
	GF_XMLNode *xvs_node_in_xvl;
	u32 xvl_node_index;

	/* Pointer to the current validation script file being tested */
	char *xvs_filename;
	GF_DOMParser *xvs_parser;
	GF_XMLNode *xvs_node;
	Bool xvs_result;

	/* test sequence */
	char *test_base;
	char *test_filename;

	Bool snapshot_next_frame;
	u32 snapshot_number;

	GF_TermEventFilter evt_filter;
} GF_Validator;

static void validator_xvs_close(GF_Validator *validator);
static Bool validator_xvs_next(GF_Validator *validator, Bool reverse);

static void validator_xvs_add_snapshot_node(GF_Validator *validator, const char *filename, u32 scene_time)
{
	GF_XMLNode *snap_node;
	GF_XMLAttribute *att;
	GF_SAFEALLOC(snap_node, GF_XMLNode);
	snap_node->name = gf_strdup("snapshot");
	snap_node->attributes = gf_list_new();
	GF_SAFEALLOC(att, GF_XMLAttribute);
	att->name = gf_strdup("time");
	att->value = gf_malloc(100);
	sprintf(att->value, "%d", scene_time);
	gf_list_add(snap_node->attributes, att);
	GF_SAFEALLOC(att, GF_XMLAttribute);
	att->name = gf_strdup("image");
	att->value = gf_strdup(filename);
	gf_list_add(snap_node->attributes, att);
	gf_list_add(validator->xvs_node->content, snap_node);

	/* adding an extra text node for line break in serialization */
	GF_SAFEALLOC(snap_node, GF_XMLNode);
	snap_node->type = GF_XML_TEXT_TYPE;
	snap_node->name = gf_strdup("\n");
	gf_list_add(validator->xvs_node->content, snap_node);
}

static char *validator_get_snapshot_name(char *test_filename, Bool is_reference, u32 number)
{
	char *dot;
	char dumpname[GF_MAX_PATH];
	dot = strrchr(test_filename, '.');
	dot[0] = 0;
	sprintf(dumpname, "%s-%s-%03d.png", test_filename, (is_reference?"reference":"newest"), number);
	dot[0] = '.';
	return gf_strdup(dumpname);
}

static char *validator_create_snapshot(GF_Validator *validator)
{
	GF_Err e;
	GF_VideoSurface fb;
	GF_Terminal *term = validator->term;
	char *dumpname;

	dumpname = validator_get_snapshot_name(validator->test_filename, validator->is_recording, validator->snapshot_number);

	e = gf_term_get_screen_buffer(term, &fb);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Error dumping screen buffer %s\n", gf_error_to_string(e)));
	} else {
		u32 dst_size = fb.width*fb.height*3;
		char *dst=gf_malloc(sizeof(char)*dst_size);

		e = gf_img_png_enc(fb.video_buffer, fb.width, fb.height, fb.pitch_y, fb.pixel_format, dst, &dst_size);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Error encoding PNG %s\n", gf_error_to_string(e)));
		} else {
			FILE *png = gf_f64_open(dumpname, "wb");
			if (!png) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Error writing file %s\n", dumpname));
			} else {
				gf_fwrite(dst, dst_size, 1, png);
				fclose(png);
				GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[Validator] Writing file %s\n", dumpname));
			}
		}
		if (dst) gf_free(dst);
		gf_term_release_screen_buffer(term, &fb);
	}
	validator->snapshot_number++;
	return dumpname;
}

static Bool validator_compare_snapshots(GF_Validator *validator)
{
	char *ref_name, *new_name;
	u32 ref_width, ref_height, ref_pixel_format, ref_data_size;
	u32 new_width, new_height, new_pixel_format, new_data_size;
	char *ref_data, *new_data;
	Bool result = 0;
	GF_Err e;
	u32 i;

	u32 snap_number = validator->snapshot_number - 1;
	ref_name = validator_get_snapshot_name(validator->test_filename, 1, snap_number);
	new_name = validator_get_snapshot_name(validator->test_filename, 0, snap_number);

	e = gf_img_file_dec(ref_name, NULL, &ref_width, &ref_height, &ref_pixel_format, &ref_data, &ref_data_size);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Cannot decode PNG file %s\n", ref_name));
		goto end;
	}
	e = gf_img_file_dec(new_name, NULL, &new_width, &new_height, &new_pixel_format, &new_data, &new_data_size);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Cannot decode PNG file %s\n", new_name));
		goto end;
	}
	if (ref_width != new_width) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Snapshots have different widths: %d vs %d\n", ref_width, new_width));
		goto end;
	}
	if (ref_height != new_height) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Snapshots have different heights: %d vs %d\n", ref_height, new_height));
		goto end;
	}
	if (ref_pixel_format != new_pixel_format) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Snapshots have different pixel formats: %d vs %d\n", ref_pixel_format, new_pixel_format));
		goto end;
	}

	for (i = 0; i<ref_data_size; i++) {
		if (ref_data[i] != new_data[i]) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Snapshots have different pixel values at position %d: %d vs %d\n", i, ref_data[i], new_data[i]));
			break;
		}
	}
	if (i==ref_data_size) result = 1;

end:
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Validator] PNG Comparison result: %s\n", (result?"Same":"Different")));
	if (ref_name) gf_free(ref_name);
	if (new_name) gf_free(new_name);
	return result;
}

static void validator_on_video_frame(void *udta, u32 time)
{
	GF_Validator *validator = (GF_Validator *)udta;
	if (validator->snapshot_next_frame) {
		char *snap_name = validator_create_snapshot(validator);
		validator_xvs_add_snapshot_node(validator, snap_name, gf_clock_time(validator->ck));
		gf_free(snap_name);
		validator->snapshot_next_frame = 0;
	}
}

static void validator_on_video_reconfig(void *udta, u32 width, u32 height, u8 bpp)
{
}

Bool validator_on_event_play(void *udta, GF_Event *event, Bool consumed_by_compositor)
{
	GF_Validator *validator = (GF_Validator *)udta;
	switch (event->type) {
	case GF_EVENT_CONNECT:
		if (event->connect.is_connected) {
			gf_sc_add_video_listener(validator->term->compositor, &validator->video_listener);
			validator->ck = validator->term->root_scene->scene_codec ?
			                validator->term->root_scene->scene_codec->ck :
			                validator->term->root_scene->dyn_ck;
		}
		break;
	case GF_EVENT_CLICK:
	case GF_EVENT_MOUSEUP:
	case GF_EVENT_MOUSEDOWN:
	case GF_EVENT_MOUSEOVER:
	case GF_EVENT_MOUSEOUT:
	case GF_EVENT_MOUSEMOVE:
	case GF_EVENT_MOUSEWHEEL:
	case GF_EVENT_KEYDOWN:
	case GF_EVENT_TEXTINPUT:
		return 1;
	case GF_EVENT_KEYUP:
		if ((event->key.key_code == GF_KEY_END)&&(event->key.flags & GF_KEY_MOD_CTRL)) {
			GF_Event evt;
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_QUIT;
			validator->term->compositor->video_out->on_event(validator->term->compositor->video_out->evt_cbk_hdl, &evt);
		}
		return 1;
	}
	return 0;
}

static void validator_xvs_add_event_dom(GF_Validator *validator, GF_Event *event)
{
	GF_XMLNode *evt_node;
	GF_XMLAttribute *att;

	GF_SAFEALLOC(evt_node, GF_XMLNode);

	switch (event->type) {
	case GF_EVENT_CLICK:
	case GF_EVENT_MOUSEUP:
	case GF_EVENT_MOUSEDOWN:
	case GF_EVENT_MOUSEOVER:
	case GF_EVENT_MOUSEOUT:
	case GF_EVENT_MOUSEMOVE:
	case GF_EVENT_MOUSEWHEEL:
	case GF_EVENT_KEYUP:
	case GF_EVENT_KEYDOWN:
	case GF_EVENT_TEXTINPUT:
		evt_node->name = gf_strdup(gf_dom_event_get_name(event->type));
		break;
	}

	if (!evt_node->name) {
		gf_free(evt_node);
		return;
	}

	evt_node->attributes = gf_list_new();

	GF_SAFEALLOC(att, GF_XMLAttribute);
	att->name = gf_strdup("time");
	att->value = gf_malloc(100);
	sprintf(att->value, "%f", gf_scene_get_time(validator->term->root_scene)*1000);
	gf_list_add(evt_node->attributes, att);

	switch (event->type) {
	case GF_EVENT_CLICK:
	case GF_EVENT_MOUSEUP:
	case GF_EVENT_MOUSEDOWN:
	case GF_EVENT_MOUSEOVER:
	case GF_EVENT_MOUSEOUT:
	case GF_EVENT_MOUSEMOVE:
	case GF_EVENT_MOUSEWHEEL:
		if (event->type == GF_EVENT_MOUSEDOWN || event->type == GF_EVENT_MOUSEUP) {
			GF_SAFEALLOC(att, GF_XMLAttribute);
			att->name = gf_strdup("button");
			switch (event->mouse.button) {
			case 0:
				att->value = gf_strdup("Left");
				break;
			case 1:
				att->value = gf_strdup("Middle");
				break;
			case 2:
				att->value = gf_strdup("Right");
				break;
			}
			gf_list_add(evt_node->attributes, att);
		}
		GF_SAFEALLOC(att, GF_XMLAttribute);
		att->name = gf_strdup("x");
		att->value = gf_malloc(100);
		sprintf(att->value, "%d", event->mouse.x);
		gf_list_add(evt_node->attributes, att);
		GF_SAFEALLOC(att, GF_XMLAttribute);
		att->name = gf_strdup("y");
		att->value = gf_malloc(100);
		sprintf(att->value, "%d", event->mouse.y);
		gf_list_add(evt_node->attributes, att);
		if (event->type == GF_EVENT_MOUSEWHEEL) {
			GF_SAFEALLOC(att, GF_XMLAttribute);
			att->name = gf_strdup("wheel_pos");
			att->value = gf_malloc(100);
			sprintf(att->value, "%f", event->mouse.wheel_pos);
			gf_list_add(evt_node->attributes, att);
		}
		if (event->mouse.key_states & GF_KEY_MOD_SHIFT) {
			GF_SAFEALLOC(att, GF_XMLAttribute);
			att->name = gf_strdup("shift");
			att->value = gf_strdup("true");
			gf_list_add(evt_node->attributes, att);
		}
		if (event->mouse.key_states & GF_KEY_MOD_CTRL) {
			GF_SAFEALLOC(att, GF_XMLAttribute);
			att->name = gf_strdup("ctrl");
			att->value = gf_strdup("true");
			gf_list_add(evt_node->attributes, att);
		}
		if (event->mouse.key_states & GF_KEY_MOD_ALT) {
			GF_SAFEALLOC(att, GF_XMLAttribute);
			att->name = gf_strdup("alt");
			att->value = gf_strdup("true");
			gf_list_add(evt_node->attributes, att);
		}
		break;
	/*Key Events*/
	case GF_EVENT_KEYUP:
	case GF_EVENT_KEYDOWN:
	case GF_EVENT_LONGKEYPRESS:
		GF_SAFEALLOC(att, GF_XMLAttribute);
		att->name = gf_strdup("key_identifier");
		att->value = gf_strdup(gf_dom_get_key_name(event->key.key_code));
		gf_list_add(evt_node->attributes, att);
		if (event->key.flags & GF_KEY_MOD_SHIFT) {
			GF_SAFEALLOC(att, GF_XMLAttribute);
			att->name = gf_strdup("shift");
			att->value = gf_strdup("true");
			gf_list_add(evt_node->attributes, att);
		}
		if (event->key.flags & GF_KEY_MOD_CTRL) {
			GF_SAFEALLOC(att, GF_XMLAttribute);
			att->name = gf_strdup("ctrl");
			att->value = gf_strdup("true");
			gf_list_add(evt_node->attributes, att);
		}
		if (event->key.flags & GF_KEY_MOD_ALT) {
			GF_SAFEALLOC(att, GF_XMLAttribute);
			att->name = gf_strdup("alt");
			att->value = gf_strdup("true");
			gf_list_add(evt_node->attributes, att);
		}
		break;
	case GF_EVENT_TEXTINPUT:
		GF_SAFEALLOC(att, GF_XMLAttribute);
		att->name = gf_strdup("unicode-char");
		att->value = gf_malloc(100);
		sprintf(att->value, "%d", event->character.unicode_char);
		gf_list_add(evt_node->attributes, att);
		break;
	}
	gf_list_add(validator->xvs_node->content, evt_node);
	/* adding an extra text node for line break in serialization */
	GF_SAFEALLOC(evt_node, GF_XMLNode);
	evt_node->type = GF_XML_TEXT_TYPE;
	evt_node->name = gf_strdup("\n");
	gf_list_add(validator->xvs_node->content, evt_node);
}

Bool validator_on_event_record(void *udta, GF_Event *event, Bool consumed_by_compositor)
{
	GF_Validator *validator = (GF_Validator *)udta;
	Bool rec_event = 1;
	switch (event->type) {
	case GF_EVENT_CONNECT:
		if (event->connect.is_connected) {
			gf_sc_add_video_listener(validator->term->compositor, &validator->video_listener);
			validator->ck = validator->term->root_scene->scene_codec ? validator->term->root_scene->scene_codec->ck : validator->term->root_scene->dyn_ck;
		}
		break;
	case GF_EVENT_KEYDOWN:
		if (event->key.key_code == GF_KEY_INSERT) {
			rec_event = 0;
		} else if (event->key.key_code == GF_KEY_PAGEDOWN) {
			rec_event = 0;
		} else if (event->key.key_code == GF_KEY_PAGEUP) {
			rec_event = 0;
		} else if (event->key.key_code == GF_KEY_END) {
			rec_event = 0;
		} else if (event->key.key_code == GF_KEY_CONTROL) {
			rec_event = 0;
		} else if (event->key.flags & GF_KEY_MOD_CTRL) {
			rec_event = 0;
		}
		break;
	case GF_EVENT_KEYUP:
		if (event->key.flags & GF_KEY_MOD_CTRL) {
			rec_event = 0;
			if (event->key.key_code == GF_KEY_INSERT) {
				char *snap_name = validator_create_snapshot(validator);
				validator_xvs_add_snapshot_node(validator, snap_name, gf_clock_time(validator->ck));
				gf_free(snap_name);
			} else if (event->key.key_code == GF_KEY_END) {
				GF_Event evt;
				memset(&evt, 0, sizeof(GF_Event));
				evt.type = GF_EVENT_QUIT;
				validator->term->compositor->video_out->on_event(validator->term->compositor->video_out->evt_cbk_hdl, &evt);
			} else if (event->key.key_code == GF_KEY_F1) {
				validator->snapshot_next_frame = 1;
			}
		} else if (event->key.key_code == GF_KEY_PAGEDOWN) {
			rec_event = 0;
			validator_xvs_close(validator);
			gf_term_disconnect(validator->term);
			gf_sc_remove_video_listener(validator->term->compositor, &validator->video_listener);
			validator_xvs_next(validator, 0);
		} else if (event->key.key_code == GF_KEY_PAGEUP) {
			rec_event = 0;
			validator_xvs_close(validator);
			gf_term_disconnect(validator->term);
			gf_sc_remove_video_listener(validator->term->compositor, &validator->video_listener);
			validator_xvs_next(validator, 1);
		} else if (event->key.key_code == GF_KEY_CONTROL) {
			rec_event = 0;
		}
		break;
	}
	if (rec_event) {
		validator_xvs_add_event_dom(validator, event);
	}
	return 0;
}

static void validator_xvl_open(GF_Validator *validator)
{
	GF_Err e;
	u32 att_index;
	GF_XMLAttribute *att;
	validator->xvl_parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(validator->xvl_parser, validator->xvl_filename, NULL, NULL);
	if (e != GF_OK) {
		gf_xml_dom_del(validator->xvl_parser);
		validator->xvl_parser = NULL;
		return;
	}
	validator->xvl_node = gf_xml_dom_get_root(validator->xvl_parser);
	if (!validator->xvl_node) {
		gf_xml_dom_del(validator->xvl_parser);
		validator->xvl_parser = NULL;
		return;
	}
	att_index = 0;
	while (1) {
		att = gf_list_get(validator->xvl_node->attributes, att_index);
		if (!att) break;
		if (!strcmp(att->name, "content-base")) {
			validator->test_base = gf_strdup(att->value);
		}
		att_index++;
	}
}

static void validator_xvl_close(GF_Validator *validator)
{
	if (validator->xvl_parser) {
		/* writing the validation results */
		if (!validator->is_recording) {
			FILE *xvl_fp;
			char *xvl_content;
			char result_filename[GF_MAX_PATH];
			char *dot;
			xvl_content = gf_xml_dom_serialize(validator->xvl_node, 0);
			dot = strrchr(validator->xvl_filename, '.');
			dot[0] = 0;
			sprintf(result_filename, "%s-result.xml", validator->xvl_filename);
			dot[0] = '.';
			xvl_fp = gf_f64_open(result_filename, "wt");
			gf_fwrite(xvl_content, strlen(xvl_content), 1, xvl_fp);
			fclose(xvl_fp);
			gf_free(xvl_content);
		}
		gf_xml_dom_del(validator->xvl_parser);
		validator->xvl_parser = NULL;
		validator->xvl_filename = NULL;
	}
}

static void validator_xvl_get_next_xvs(GF_Validator *validator, Bool reverse)
{
	u32 xvl_att_index;
	validator->xvs_node = NULL;
	validator->xvs_filename = NULL;
	validator->test_filename = NULL;
	while (1) {
		validator->xvs_node_in_xvl = gf_list_get(validator->xvl_node->content, validator->xvl_node_index);
		if (!validator->xvs_node_in_xvl) {
			return;
		}
		if (validator->xvs_node_in_xvl->type != GF_XML_NODE_TYPE) {
			if (!reverse) validator->xvl_node_index++;
			else validator->xvl_node_index--;
			continue;
		}
		xvl_att_index = 0;
		while(1) {
			GF_XMLAttribute *att = gf_list_get(validator->xvs_node_in_xvl->attributes, xvl_att_index);
			if (!att) break;
			if (!strcmp(att->name, "scenario")) {
				validator->xvs_filename = att->value;
			} else if (!strcmp(att->name, "content")) {
				validator->test_filename = att->value;
			}
			xvl_att_index++;
		}
		if (!reverse) validator->xvl_node_index++;
		else validator->xvl_node_index--;
		break;
	}
}

static Bool validator_xvs_open(GF_Validator *validator)
{
	GF_Err e;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Validator] Opening Validation Script: %s\n", validator->xvs_filename));
	validator->snapshot_number = 0;
	validator->xvs_parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(validator->xvs_parser, validator->xvs_filename, NULL, NULL);
	if (e != GF_OK) {
		if (validator->is_recording) {
			GF_SAFEALLOC(validator->xvs_node, GF_XMLNode);
			validator->xvs_node->name = gf_strdup("TestValidationScript");
			validator->xvs_node->attributes = gf_list_new();
			validator->xvs_node->content = gf_list_new();
		} else {
			gf_xml_dom_del(validator->xvs_parser);
			validator->xvs_parser = NULL;
			return 0;
		}
	} else {
		validator->xvs_node = gf_xml_dom_get_root(validator->xvs_parser);
	}
	/* Get the file name from the XVS if not found in the XVL */
	if (!validator->test_filename) {
		GF_XMLAttribute *att;
		GF_XMLAttribute *att_file;
		u32 att_index = 0;
		att_file = NULL;
		while (1) {
			att = gf_list_get(validator->xvs_node->attributes, att_index);
			if (!att) {
				break;
			} else if (!strcmp(att->name, "file")) {
				att_file = att;
			}
			att_index++;
		}
		if (!att_file) {
			gf_xml_dom_del(validator->xvs_parser);
			validator->xvs_parser = NULL;
			validator->xvs_node = NULL;
			return 0;
		} else {
			char *sep;
			sep = strrchr(att_file->value, GF_PATH_SEPARATOR);
			if (!sep) {
				validator->test_filename = att_file->value;
			} else {
				sep[0] = 0;
				validator->test_base = gf_strdup(att_file->value);
				sep[0] = GF_PATH_SEPARATOR;
				validator->test_filename = sep+1;
			}
		}
	}
	if (validator->is_recording) {
		GF_XMLNode *node;
		/* Removing prerecorded interactions */
		while (gf_list_count(validator->xvs_node->content)) {
			GF_XMLNode *child = (GF_XMLNode *)gf_list_last(validator->xvs_node->content);
			gf_list_rem_last(validator->xvs_node->content);
			gf_xml_dom_node_del(child);
		}
		/* adding an extra text node for line break in serialization */
		GF_SAFEALLOC(node, GF_XMLNode);
		node->type = GF_XML_TEXT_TYPE;
		node->name = gf_strdup("\n");
		gf_list_add(validator->xvs_node->content, node);
	} else {
		validator->xvs_result = 1;
	}
	return 1;
}

static void validator_xvs_close(GF_Validator *validator)
{
	if (validator->xvs_parser) {
		if (validator->is_recording) {
			FILE *xvs_fp;
			char *xvs_content;
			char filename[100];
			GF_XMLAttribute *att;
			GF_XMLAttribute *att_file = NULL;
			u32 att_index = 0;
			while (1) {
				att = gf_list_get(validator->xvs_node->attributes, att_index);
				if (!att) {
					break;
				} else if (!strcmp(att->name, "file")) {
					att_file = att;
				}
				att_index++;
			}

			if (!att_file) {
				GF_SAFEALLOC(att, GF_XMLAttribute);
				att->name = gf_strdup("file");
				gf_list_add(validator->xvs_node->attributes, att);
			} else {
				att = att_file;
				if (att->value) gf_free(att->value);
			}
			sprintf(filename, "%s%c%s", validator->test_base, GF_PATH_SEPARATOR, validator->test_filename);
			att->value = gf_strdup(filename);
			xvs_content = gf_xml_dom_serialize(validator->xvs_node, 0);
			xvs_fp = gf_f64_open(validator->xvs_filename, "wt");
			gf_fwrite(xvs_content, strlen(xvs_content), 1, xvs_fp);
			fclose(xvs_fp);
			gf_free(xvs_content);
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Validator] XVS Result : %s\n", (validator->xvs_result?"Success":"Failure")));
			if (validator->xvs_node_in_xvl) {
				GF_XMLAttribute *att;
				GF_XMLAttribute *att_result = NULL;
				u32 att_index = 0;
				while (1) {
					att = gf_list_get(validator->xvs_node_in_xvl->attributes, att_index);
					if (!att) {
						break;
					} else if (!strcmp(att->name, "result")) {
						att_result = att;
					}
					att_index++;
				}
				if (!att_result) {
					GF_SAFEALLOC(att_result, GF_XMLAttribute);
					att_result->name = gf_strdup("result");
					gf_list_add(validator->xvs_node_in_xvl->attributes, att_result);
				}
				if (att_result->value) gf_free(att_result->value);
				att_result->value = gf_strdup(validator->xvs_result ? "pass" : "fail");
			}
		}
		gf_xml_dom_del(validator->xvs_parser);
		validator->xvs_parser = NULL;
	}
	validator->xvs_node = NULL;
	validator->xvs_node_in_xvl = NULL;
	validator->xvs_filename = NULL;
	validator->test_filename = NULL;
	validator->ck = NULL;
	validator->xvs_event_index = 0;
	validator->snapshot_number = 0;
}

static void validator_test_open(GF_Validator *validator)
{
	char filename[100];
	if (validator->test_base)
		sprintf(filename, "%s%c%s", validator->test_base, GF_PATH_SEPARATOR, validator->test_filename);
	else
		sprintf(filename, "%s", validator->test_filename);
	gf_sc_add_video_listener(validator->term->compositor, &validator->video_listener);
	if (validator->is_recording)
		validator->snapshot_next_frame = 1;
	gf_term_connect(validator->term, filename);
	validator->ck = validator->term->root_scene->scene_codec ?
	                validator->term->root_scene->scene_codec->ck :
	                validator->term->root_scene->dyn_ck;
}

static Bool validator_xvs_next(GF_Validator *validator, Bool reverse)
{
	if (validator->xvl_node) {
		validator_xvl_get_next_xvs(validator, reverse);
		if (validator->xvs_filename) {
			validator_xvs_open(validator);
			if (!validator->xvs_node) {
				return 0;
			}
			if (validator->test_filename) {
				validator_test_open(validator);
			} else {
				validator_xvs_close(validator);
				return 0;
			}
		} else {
			return 0;
		}
		return 1;
	} else {
		return 0;
	}
}

static Bool validator_load_event(GF_Validator *validator)
{
	GF_XMLNode *event_node;
	GF_XMLAttribute *att;
	u32 att_index;

	memset(&validator->next_event, 0, sizeof(GF_Event));
	validator->evt_loaded = 0;
	validator->next_event_snapshot = 0;

	if (!validator->xvs_node) return 0;

	while (1) {
		event_node = gf_list_get(validator->xvs_node->content, validator->xvs_event_index);
		if (!event_node) {
			return 0;
		} else if (event_node->type == GF_XML_NODE_TYPE) {
			validator->xvs_event_index++;
			break;
		} else {
			validator->xvs_event_index++;
		}
	}

	if (!strcmp(event_node->name, "snapshot")) {
		validator->next_event_snapshot = 1;
	} else {
		validator->next_event.type = gf_dom_event_type_by_name(event_node->name);
		if (validator->next_event.type == GF_EVENT_UNKNOWN) {
			return 1;
		}
	}

	att_index = 0;
	while (1) {
		att = gf_list_get(event_node->attributes, att_index);
		if (!att) break;
		if (!strcmp(att->name, "time")) {
			validator->next_time = atoi(att->value);
		} else if (!strcmp(att->name, "button")) {
			if (!strcmp(att->value, "Left")) {
				validator->next_event.mouse.button = 0;
			} else if (!strcmp(att->value, "Middle")) {
				validator->next_event.mouse.button = 1;
			} else if (!strcmp(att->value, "Right")) {
				validator->next_event.mouse.button = 2;
			}
		} else if (!strcmp(att->name, "x")) {
			validator->next_event.mouse.x = atoi(att->value);
		} else if (!strcmp(att->name, "y")) {
			validator->next_event.mouse.y = atoi(att->value);
		} else if (!strcmp(att->name, "wheel_pos")) {
			validator->next_event.mouse.wheel_pos = FLT2FIX(atof(att->value));
		} else if (!strcmp(att->name, "shift") && !strcmp(att->value, "true")) {
			validator->next_event.mouse.key_states |= GF_KEY_MOD_SHIFT;
		} else if (!strcmp(att->name, "alt") && !strcmp(att->value, "true")) {
			validator->next_event.mouse.key_states |= GF_KEY_MOD_ALT;
		} else if (!strcmp(att->name, "ctrl") && !strcmp(att->value, "true")) {
			validator->next_event.mouse.key_states |= GF_KEY_MOD_CTRL;
		} else if (!strcmp(att->name, "key_identifier")) {
			validator->next_event.key.key_code = gf_dom_get_key_type(att->value);
		} else if (!strcmp(att->name, "unicode-char")) {
			validator->next_event.character.unicode_char = atoi(att->value);
		}
		att_index++;
	}
	validator->evt_loaded = 1;
	return 1;
}

static Bool validator_process(GF_TermExt *termext, u32 action, void *param)
{
	const char *opt;
	GF_Validator *validator = termext->udta;

	switch (action) {

	/* Upon starting of the terminal, we parse (possibly an XVL file), an XVS file, and start the first test sequence */
	case GF_TERM_EXT_START:
		validator->term = (GF_Terminal *) param;

		/* if the validator is loaded, we switch off anti-aliasing for image comparison and we put a low framerate,
		but we store the previous value to restore it upon termination of the validator */
		opt = (char *)gf_modules_get_option((GF_BaseInterface*)termext, "Compositor", "FrameRate");
		if (opt) validator->prev_fps = gf_strdup(opt);
		opt = (char *)gf_modules_get_option((GF_BaseInterface*)termext, "Compositor", "AntiAlias");
		if (opt) validator->prev_alias = gf_strdup(opt);

		/* Check if the validator should be loaded and in which mode */
		opt = gf_modules_get_option((GF_BaseInterface*)termext, "Validator", "Mode");
		if (!opt) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("Validator missing configuration, stopping.\n"));
			return 0;
		} else if (!strcmp(opt, "Play")) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Validator starting in playback mode.\n"));
			validator->is_recording = 0;
		} else if (!strcmp(opt, "Record")) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Validator starting in recording mode.\n"));
			validator->is_recording = 1;
		} else if (!strcmp(opt, "Disable")) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Validator is disabled.\n"));
			return 0;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("Validator configuration using wrong mode, stopping.\n"));
			return 0;
		}

		/* initializes the validator and starts */
		validator->xvs_filename = NULL;
		validator->xvl_filename = (char *)gf_modules_get_option((GF_BaseInterface*)termext, "Validator", "XVL");
		if (!validator->xvl_filename) {
			validator->xvs_filename = (char *)gf_modules_get_option((GF_BaseInterface*)termext, "Validator", "XVS");
			if (!validator->xvs_filename) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("Validator configuration without input, stopping.\n"));
				return 0;
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Validator using scenario file: %s\n", validator->xvs_filename));
			}
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Validator using scenario playlist: %s\n", validator->xvl_filename));
		}

		/* since we changed parameters of the compositor, we need to trigger a reconfiguration */
		gf_modules_set_option((GF_BaseInterface*)termext, "Compositor", "FrameRate", "5.0");
		gf_modules_set_option((GF_BaseInterface*)termext, "Compositor", "AntiAlias", "None");
		gf_term_set_option(validator->term, GF_OPT_RELOAD_CONFIG, 1);

		validator->evt_filter.udta = validator;
		if (!validator->is_recording) {
			validator->evt_filter.on_event = validator_on_event_play;
			termext->caps |= GF_TERM_EXTENSION_NOT_THREADED;
		} else {
			validator->evt_filter.on_event = validator_on_event_record;
		}
		gf_term_add_event_filter(validator->term, &validator->evt_filter);
		validator->video_listener.udta = validator;
		validator->video_listener.on_video_frame = validator_on_video_frame;
		validator->video_listener.on_video_reconfig = validator_on_video_reconfig;

		/* TODO: if start returns 0, the module is not loaded, so the above init (filter registration) is not removed,
		   should probably return 1 all the time, to make sure stop is called */
		if (validator->xvl_filename) {
			validator_xvl_open(validator);
			if (!validator->xvl_node) {
				return 0;
			}
			validator_xvs_next(validator, 0);
			if (!validator->xvs_node) {
				return 0;
			}
		} else if (validator->xvs_filename) {
			validator_xvs_open(validator);
			if (!validator->xvs_node) {
				return 0;
			}
			if (validator->test_filename) {
				validator_test_open(validator);
			} else {
				validator_xvs_close(validator);
				return 0;
			}
		} else {
			return 0;
		}
		if (!validator->is_recording) {
			validator_load_event(validator);
		}
		return 1;

	/* when the terminal stops, we close the XVS parser and XVL parser if any, restore the config,
	and free all validator data (the validator will be destroyed when the module is unloaded)
	Note: we don't need to disconnect the terminal since it's already stopping */
	case GF_TERM_EXT_STOP:
		gf_term_remove_event_filter(validator->term, &validator->evt_filter);
		validator_xvs_close(validator);
		validator_xvl_close(validator);
		validator->term = NULL;
		if (validator->test_base) {
			gf_free(validator->test_base);
			validator->test_base = NULL;
		}
		/*auto-disable the recording by default*/
		if (validator->is_recording) {
			gf_modules_set_option((GF_BaseInterface*)termext, "Validator", "Mode", "Play");
		} else {
			gf_modules_set_option((GF_BaseInterface*)termext, "Validator", "Mode", "Disable");
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Stopping validator\n"));
		if (validator->prev_fps) {
			gf_modules_set_option((GF_BaseInterface*)termext, "Compositor", "FrameRate", validator->prev_fps);
			gf_free(validator->prev_fps);
			validator->prev_fps = NULL;
		}
		if (validator->prev_alias) {
			gf_modules_set_option((GF_BaseInterface*)termext, "Compositor", "AntiAlias", validator->prev_alias);
			gf_free(validator->prev_alias);
			validator->prev_alias = NULL;
		}
		break;

	/* When called in the main loop of the terminal, we don't do anything in the recording mode.
	   In the playing/validating mode, we need to check if an event needs to be dispatched or if snapshots need to be made,
	   until there is no more event, in which case we trigger either the load of the next XVS or the quit */
	case GF_TERM_EXT_PROCESS:
		/* if the time is right, dispatch the event and load the next one */
		while (!validator->is_recording && validator->evt_loaded && validator->ck && (validator->next_time <= gf_clock_time(validator->ck) )) {
			Bool has_more_events;
			//u32 diff = gf_clock_time(validator->ck) - validator->next_time;
			//GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Time diff: evt_time=%d  clock_time = %d, diff=%d\n", validator->next_time, gf_clock_time(validator->ck), diff));
			if (validator->next_event_snapshot) {
				Bool res;
				char *snap_name = validator_create_snapshot(validator);
				gf_free(snap_name);
				res = validator_compare_snapshots(validator);
				validator->xvs_result &= res;
				validator->next_event_snapshot = 0;
			} else {
				validator->term->compositor->video_out->on_event(validator->term->compositor->video_out->evt_cbk_hdl, &validator->next_event);
			}
			has_more_events = validator_load_event(validator);
			if (!has_more_events) {
				validator_xvs_close(validator);
				gf_term_disconnect(validator->term);
				gf_sc_remove_video_listener(validator->term->compositor, &validator->video_listener);
				validator_xvs_next(validator, 0);
				if (!validator->xvs_node) {
					GF_Event evt;
					memset(&evt, 0, sizeof(GF_Event));
					evt.type = GF_EVENT_QUIT;
					validator->term->compositor->video_out->on_event(validator->term->compositor->video_out->evt_cbk_hdl, &evt);
				} else {
					if (!validator->is_recording) {
						validator_load_event(validator);
					}
				}
			}
		}
		break;
	}
	return 0;
}


GF_TermExt *validator_new()
{
	GF_TermExt *dr;
	GF_Validator *validator;
	dr = (GF_TermExt*)gf_malloc(sizeof(GF_TermExt));
	memset(dr, 0, sizeof(GF_TermExt));
	GF_REGISTER_MODULE_INTERFACE(dr, GF_TERM_EXT_INTERFACE, "GPAC Test Validator", "gpac distribution");

	GF_SAFEALLOC(validator, GF_Validator);
	dr->process = validator_process;
	dr->udta = validator;
	return dr;
}


void validator_delete(GF_BaseInterface *ifce)
{
	GF_TermExt *dr = (GF_TermExt *) ifce;
	GF_Validator *validator = dr->udta;
	if (validator->prev_fps) gf_free(validator->prev_fps);
	if (validator->prev_alias) gf_free(validator->prev_alias);
	gf_free(validator);
	gf_free(dr);
}

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_TERM_EXT_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_TERM_EXT_INTERFACE) return (GF_BaseInterface *)validator_new();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_TERM_EXT_INTERFACE:
		validator_delete(ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DELARATION( validator )
