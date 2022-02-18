/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato, Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2010-2022
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

#include <gpac/modules/compositor_ext.h>
#include <gpac/filters.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/internal/media_dev.h>
#include <gpac/xml.h>
#include <gpac/options.h>

typedef struct __validation_module
{
	GF_Compositor *compositor;

	Bool is_recording;
	Bool trace_mode;

	/* Clock used to synchronize events in recording and playback*/
	GF_ObjectManager *root_odm;

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
    Bool owns_root;

	/* test sequence */
	char *test_base;
	char *test_filename;

	Bool snapshot_next_frame;
	u32 snapshot_number;

	GF_FSEventListener evt_filter;
} GF_Validator;

static void validator_xvs_close(GF_Validator *validator);
static Bool validator_xvs_next(GF_Validator *validator, Bool reverse);

#ifndef GPAC_DISABLE_AV_PARSERS

static void validator_xvs_add_snapshot_node(GF_Validator *validator, const char *filename, u32 scene_time)
{
	GF_XMLNode *snap_node;
	GF_XMLAttribute *att;
	GF_SAFEALLOC(snap_node, GF_XMLNode);
	if (!snap_node) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate snapshot\n"));
		return;
	}
	snap_node->name = gf_strdup("snapshot");
	snap_node->attributes = gf_list_new();

	att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
	if (!att) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate snapshot\n"));
		return;
	}
	att->name = gf_strdup("time");
	att->value = (char*)gf_malloc(100);
	sprintf(att->value, "%d", scene_time);
	gf_list_add(snap_node->attributes, att);
	
	att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
	if (!att) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate snapshot\n"));
		return;
	}
	att->name = gf_strdup("image");
	att->value = gf_strdup(filename);
	gf_list_add(snap_node->attributes, att);
	gf_list_add(validator->xvs_node->content, snap_node);

	/* adding an extra text node for line break in serialization */
	GF_SAFEALLOC(snap_node, GF_XMLNode);
	if (!snap_node) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate snapshot\n"));
		return;
	}
	snap_node->type = GF_XML_TEXT_TYPE;
	snap_node->name = gf_strdup("\n");
	gf_list_add(validator->xvs_node->content, snap_node);
}

static char *validator_get_snapshot_name(GF_Validator *validator, Bool is_reference, u32 number)
{
	char *name = validator->test_filename ? validator->test_filename : validator->xvs_filename;
	char *dot;
	char dumpname[GF_MAX_PATH];
	dot = gf_file_ext_start(name);
	dot[0] = 0;
	sprintf(dumpname, "%s-%s-%03d.png", name, (is_reference?"reference":"newest"), number);
	dot[0] = '.';
	return gf_strdup(dumpname);
}

static char *validator_create_snapshot(GF_Validator *validator)
{
	GF_Err e;
	GF_VideoSurface fb;
	GF_Compositor *compositor = validator->compositor;
	char *dumpname;

	dumpname = validator_get_snapshot_name(validator, validator->is_recording, validator->snapshot_number);

	e = gf_sc_get_screen_buffer(compositor, &fb, 0);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Error dumping screen buffer %s\n", gf_error_to_string(e)));
	} else {
		u32 dst_size = fb.width*fb.height*3;
		char *dst = (char*)gf_malloc(sizeof(char)*dst_size);

		e = gf_img_png_enc(fb.video_buffer, fb.width, fb.height, fb.pitch_y, fb.pixel_format, dst, &dst_size);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Error encoding PNG %s\n", gf_error_to_string(e)));
		} else {
			FILE *png = gf_fopen(dumpname, "wb");
			if (!png) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Error writing file %s\n", dumpname));
			} else {
				gf_fwrite(dst, dst_size, png);
				gf_fclose(png);
				GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[Validator] Writing file %s\n", dumpname));
			}
		}
		if (dst) gf_free(dst);
		gf_sc_release_screen_buffer(compositor, &fb);
	}
	validator->snapshot_number++;
	return dumpname;
}

static GF_Err validator_file_dec(char *png_filename, u32 *hint_codecid, u32 *width, u32 *height, u32 *pixel_format, char **dst, u32 *dst_size)
{
	u32 fsize, codecid;
	char *data;
	GF_Err e;

	codecid = 0;
	if (!hint_codecid || ! *hint_codecid) {
		char *ext = gf_file_ext_start(png_filename);
		if (!ext) return GF_NOT_SUPPORTED;
		if (!stricmp(ext, ".png")) codecid = GF_CODECID_PNG;
		else if (!stricmp(ext, ".jpg") || !stricmp(ext, ".jpeg")) codecid = GF_CODECID_JPEG;
	} else {
		codecid = *hint_codecid;
	}

	e = gf_file_load_data(png_filename, (u8 **)&data, &fsize);
	if (e) return e;

	e = GF_NOT_SUPPORTED;
	*dst_size = 0;
	if (codecid == GF_CODECID_JPEG) {
#ifdef GPAC_HAS_JPEG
		e = gf_img_jpeg_dec(data, fsize, width, height, pixel_format, NULL, dst_size, 0);
		if (*dst_size) {
			*dst = gf_malloc(*dst_size);
			return gf_img_jpeg_dec(data, fsize, width, height, pixel_format, *dst, dst_size, 0);
		}
#endif
	} else if (codecid == GF_CODECID_PNG) {
#ifdef GPAC_HAS_PNG
		e = gf_img_png_dec(data, fsize, width, height, pixel_format, NULL, dst_size);
		if (*dst_size) {
			*dst = gf_malloc(*dst_size);
			return gf_img_png_dec(data, fsize, width, height, pixel_format, *dst, dst_size);
		}
#endif
	}
	return e;
}

static Bool validator_compare_snapshots(GF_Validator *validator)
{
	char *ref_name, *new_name;
	u32 ref_width, ref_height, ref_pixel_format, ref_data_size;
	u32 new_width, new_height, new_pixel_format, new_data_size;
	char *ref_data=NULL, *new_data=NULL;
	Bool result = GF_FALSE;
	GF_Err e;
	u32 i;

	u32 snap_number = validator->snapshot_number - 1;
	ref_name = validator_get_snapshot_name(validator, GF_TRUE, snap_number);
	new_name = validator_get_snapshot_name(validator, GF_FALSE, snap_number);

	e = validator_file_dec(ref_name, NULL, &ref_width, &ref_height, &ref_pixel_format, &ref_data, &ref_data_size);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Cannot decode PNG file %s\n", ref_name));
		goto end;
	}
	e = validator_file_dec(new_name, NULL, &new_width, &new_height, &new_pixel_format, &new_data, &new_data_size);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Cannot decode PNG file %s\n", new_name));
		goto end;
	}
	if (!ref_data) ref_data_size = 0;
	if (!new_data) new_data_size = 0;

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
	if (ref_data_size != new_data_size) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Snapshots have different pixel formats: %d vs %d\n", ref_pixel_format, new_pixel_format));
		goto end;
	}

	for (i = 0; i<ref_data_size; i++) {
		if (ref_data[i] != new_data[i]) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Snapshots have different pixel values at position %d: %d vs %d\n", i, ref_data[i], new_data[i]));
			break;
		}
	}
	if (i==ref_data_size) result = GF_TRUE;

end:
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Validator] PNG Comparison result: %s\n", (result?"Same":"Different")));
	if (ref_name) gf_free(ref_name);
	if (new_name) gf_free(new_name);
	if (ref_data) gf_free(ref_data);
	if (new_data) gf_free(new_data);
	return result;
}
#endif

static void validator_on_video_frame(void *udta, u32 time)
{
	GF_Validator *validator = (GF_Validator *)udta;
	if (validator->snapshot_next_frame) {
#ifndef GPAC_DISABLE_AV_PARSERS
		char *snap_name = validator_create_snapshot(validator);
		validator_xvs_add_snapshot_node(validator, snap_name, gf_clock_time(validator->root_odm->ck));
		gf_free(snap_name);
#endif
		validator->snapshot_next_frame = GF_FALSE;
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
			if (!validator->trace_mode) {
//deprecated				gf_sc_add_video_listener(validator->compositor, &validator->video_listener);
			}

			validator->root_odm = validator->compositor->root_scene->root_od;
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
		return GF_TRUE;
	case GF_EVENT_KEYUP:
		if ((event->key.key_code == GF_KEY_END)&&(event->key.flags & GF_KEY_MOD_CTRL)) {
			GF_Event evt;
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_QUIT;
			gf_sc_on_event(validator->compositor, &evt);
		}
		return GF_TRUE;
	}
	return GF_FALSE;
}

static void validator_xvs_add_event_dom(GF_Validator *validator, GF_Event *event)
{
	GF_XMLNode *evt_node;
	GF_XMLAttribute *att;

	GF_SAFEALLOC(evt_node, GF_XMLNode);
	if (!evt_node) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event\n"));
		return;
	}

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
#ifndef GPAC_DISABLE_SVG
		evt_node->name = gf_strdup(gf_dom_event_get_name(event->type));
#endif
		break;
	}

	if (!evt_node->name) {
		gf_free(evt_node);
		return;
	}

	evt_node->attributes = gf_list_new();

	att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
	if (!att) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event time\n"));
		return;
	}
	att->name = gf_strdup("time");
	att->value = (char*)gf_malloc(100);
	sprintf(att->value, "%f", gf_scene_get_time(validator->compositor->root_scene)*1000);
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
			att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
			if (!att) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event info\n"));
				return;
			}
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
		att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
		if (!att) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event info\n"));
			return;
		}
		att->name = gf_strdup("x");
		att->value = (char*)gf_malloc(100);
		sprintf(att->value, "%d", event->mouse.x);
		gf_list_add(evt_node->attributes, att);

		att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
		if (!att) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event info\n"));
			return;
		}
		att->name = gf_strdup("y");
		att->value = (char*)gf_malloc(100);
		sprintf(att->value, "%d", event->mouse.y);
		gf_list_add(evt_node->attributes, att);
		if (event->type == GF_EVENT_MOUSEWHEEL) {
			att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
			if (!att) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event info\n"));
				return;
			}
			att->name = gf_strdup("wheel_pos");
			att->value = (char*)gf_malloc(100);
			sprintf(att->value, "%f", FIX2FLT( event->mouse.wheel_pos) );
			gf_list_add(evt_node->attributes, att);
		}
		if (event->mouse.key_states & GF_KEY_MOD_SHIFT) {
			att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
			if (!att) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event info\n"));
				return;
			}
			att->name = gf_strdup("shift");
			att->value = gf_strdup("true");
			gf_list_add(evt_node->attributes, att);
		}
		if (event->mouse.key_states & GF_KEY_MOD_CTRL) {
			att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
			if (!att) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event info\n"));
				return;
			}
			att->name = gf_strdup("ctrl");
			att->value = gf_strdup("true");
			gf_list_add(evt_node->attributes, att);
		}
		if (event->mouse.key_states & GF_KEY_MOD_ALT) {
			att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
			if (!att) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event info\n"));
				return;
			}
			att->name = gf_strdup("alt");
			att->value = gf_strdup("true");
			gf_list_add(evt_node->attributes, att);
		}
		break;
	/*Key Events*/
	case GF_EVENT_KEYUP:
	case GF_EVENT_KEYDOWN:
	case GF_EVENT_LONGKEYPRESS:
		att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
		if (!att) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event info\n"));
			return;
		}
		att->name = gf_strdup("key_identifier");
#ifndef GPAC_DISABLE_SVG
		att->value = gf_strdup(gf_dom_get_key_name(event->key.key_code));
#endif
		gf_list_add(evt_node->attributes, att);
		if (event->key.flags & GF_KEY_MOD_SHIFT) {
			att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
			if (!att) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event info\n"));
				return;
			}
			att->name = gf_strdup("shift");
			att->value = gf_strdup("true");
			gf_list_add(evt_node->attributes, att);
		}
		if (event->key.flags & GF_KEY_MOD_CTRL) {
			att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
			if (!att) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event info\n"));
				return;
			}
			att->name = gf_strdup("ctrl");
			att->value = gf_strdup("true");
			gf_list_add(evt_node->attributes, att);
		}
		if (event->key.flags & GF_KEY_MOD_ALT) {
			att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
			if (!att) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event info\n"));
				return;
			}
			att->name = gf_strdup("alt");
			att->value = gf_strdup("true");
			gf_list_add(evt_node->attributes, att);
		}
		break;
	case GF_EVENT_TEXTINPUT:
		att = (GF_XMLAttribute *) gf_malloc(sizeof(GF_XMLAttribute));
		if (!att) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event info\n"));
			return;
		}
		att->name = gf_strdup("unicode-char");
		att->value = (char*)gf_malloc(100);
		sprintf(att->value, "%d", event->character.unicode_char);
		gf_list_add(evt_node->attributes, att);
		break;
	}
	gf_list_add(validator->xvs_node->content, evt_node);
	/* adding an extra text node for line break in serialization */
	GF_SAFEALLOC(evt_node, GF_XMLNode);
	if (!evt_node) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate event\n"));
		return;
	}
	evt_node->type = GF_XML_TEXT_TYPE;
	evt_node->name = gf_strdup("\n");
	gf_list_add(validator->xvs_node->content, evt_node);
}

Bool validator_on_event_record(void *udta, GF_Event *event, Bool consumed_by_compositor)
{
	GF_Validator *validator = (GF_Validator *)udta;
	Bool rec_event = GF_TRUE;
	switch (event->type) {
	case GF_EVENT_CONNECT:
		if (event->connect.is_connected) {
			if (!validator->trace_mode) {
//deprecated				gf_sc_add_video_listener(validator->compositor, &validator->video_listener);
			}
			validator->root_odm = validator->compositor->root_scene->root_od;
		}
		break;
	case GF_EVENT_KEYDOWN:
		if (event->key.key_code == GF_KEY_INSERT) {
			rec_event = GF_FALSE;
		} else if (event->key.key_code == GF_KEY_PAGEDOWN) {
			rec_event = GF_FALSE;
		} else if (event->key.key_code == GF_KEY_PAGEUP) {
			rec_event = GF_FALSE;
		} else if (event->key.key_code == GF_KEY_END) {
			rec_event = GF_FALSE;
		} else if (event->key.key_code == GF_KEY_CONTROL) {
			rec_event = GF_FALSE;
		} else if (event->key.flags & GF_KEY_MOD_CTRL) {
			rec_event = GF_FALSE;
		}
		break;
	case GF_EVENT_KEYUP:
		if (event->key.flags & GF_KEY_MOD_CTRL) {
			rec_event = GF_FALSE;
			if (event->key.key_code == GF_KEY_INSERT) {
#ifndef GPAC_DISABLE_AV_PARSERS
				char *snap_name = validator_create_snapshot(validator);
				validator_xvs_add_snapshot_node(validator, snap_name, gf_clock_time(validator->root_odm->ck));
				gf_free(snap_name);
#endif
			} else if (event->key.key_code == GF_KEY_END) {
				GF_Event evt;
				memset(&evt, 0, sizeof(GF_Event));
				evt.type = GF_EVENT_QUIT;
				gf_sc_on_event(validator->compositor, &evt);
			} else if (event->key.key_code == GF_KEY_F1) {
				validator->snapshot_next_frame = GF_TRUE;
			}
		} else if (event->key.key_code == GF_KEY_PAGEDOWN) {
			rec_event = GF_FALSE;
			validator_xvs_close(validator);
			gf_sc_disconnect(validator->compositor);
//deprecated			gf_sc_remove_video_listener(validator->compositor, &validator->video_listener);
			validator_xvs_next(validator, GF_FALSE);
		} else if (event->key.key_code == GF_KEY_PAGEUP) {
			rec_event = GF_FALSE;
			validator_xvs_close(validator);
			gf_sc_disconnect(validator->compositor);
//deprecated			gf_sc_remove_video_listener(validator->compositor, &validator->video_listener);
			validator_xvs_next(validator, GF_TRUE);
		} else if (event->key.key_code == GF_KEY_CONTROL) {
			rec_event = GF_FALSE;
		}
		break;
	}
	if (rec_event) {
		validator_xvs_add_event_dom(validator, event);
	}
	return GF_FALSE;
}

static void validator_xvl_open(GF_Validator *validator)
{
	GF_Err e;
	u32 att_index;
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
		GF_XMLAttribute *att = (GF_XMLAttribute*)gf_list_get(validator->xvl_node->attributes, att_index);
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
			xvl_content = gf_xml_dom_serialize(validator->xvl_node, GF_FALSE, GF_FALSE);
			dot = gf_file_ext_start(validator->xvl_filename);
			dot[0] = 0;
			sprintf(result_filename, "%s-result.xml", validator->xvl_filename);
			dot[0] = '.';
			xvl_fp = gf_fopen(result_filename, "wt");
			gf_fwrite(xvl_content, strlen(xvl_content), xvl_fp);
			gf_fclose(xvl_fp);
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
		validator->xvs_node_in_xvl = (GF_XMLNode*)gf_list_get(validator->xvl_node->content, validator->xvl_node_index);
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
			GF_XMLAttribute *att = (GF_XMLAttribute*)gf_list_get(validator->xvs_node_in_xvl->attributes, xvl_att_index);
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
			if (!validator->xvs_node) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate root node\n"));
				return 0;
			}
			
			validator->xvs_node->name = gf_strdup("TestValidationScript");
			validator->xvs_node->attributes = gf_list_new();
            validator->xvs_node->content = gf_list_new();
            validator->owns_root = GF_TRUE;
		} else {
			gf_xml_dom_del(validator->xvs_parser);
			validator->xvs_parser = NULL;
			return GF_FALSE;
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
			att = (GF_XMLAttribute*)gf_list_get(validator->xvs_node->attributes, att_index);
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
			return GF_FALSE;
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
		if (!node) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate node\n"));
			return GF_FALSE;
		}
		node->type = GF_XML_TEXT_TYPE;
		node->name = gf_strdup("\n");
		gf_list_add(validator->xvs_node->content, node);
	} else {
		validator->xvs_result = GF_TRUE;
	}
	return GF_TRUE;
}

static void validator_xvs_close(GF_Validator *validator)
{
	if (validator->xvs_parser) {
		if (validator->is_recording) {
			FILE *xvs_fp;
			char *xvs_content;
			GF_XMLAttribute *att_file = NULL;
			u32 att_index = 0;
            
            if (!validator->trace_mode) {
				GF_XMLAttribute *att;
                while (1) {
                    att = (GF_XMLAttribute*)gf_list_get(validator->xvs_node->attributes, att_index);
                    if (!att) {
                        break;
                    } else if (!strcmp(att->name, "file")) {
                        att_file = att;
                    }
                    att_index++;
                }

                if (!att_file) {
                    GF_SAFEALLOC(att, GF_XMLAttribute);
					if (!att) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate file attribute\n"));
						return;
					}
                    att->name = gf_strdup("file");
                    gf_list_add(validator->xvs_node->attributes, att);
                } else {
                    att = att_file;
                    if (att->value) gf_free(att->value);
                }
                if (validator->test_base) {
					char filename[100];
                    sprintf(filename, "%s%c%s", validator->test_base, GF_PATH_SEPARATOR, validator->test_filename);
                    att->value = gf_strdup(filename);
                } else {
                    att->value = gf_strdup(validator->test_filename);
                }
            }
			xvs_content = gf_xml_dom_serialize(validator->xvs_node, GF_FALSE, GF_FALSE);
			xvs_fp = gf_fopen(validator->xvs_filename, "wt");
			gf_fwrite(xvs_content, strlen(xvs_content), xvs_fp);
			gf_fclose(xvs_fp);
			gf_free(xvs_content);
            if (validator->owns_root)
                gf_xml_dom_node_del(validator->xvs_node);
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Validator] XVS Result : %s\n", (validator->xvs_result?"Success":"Failure")));
			if (validator->xvs_node_in_xvl) {
				GF_XMLAttribute *att;
				GF_XMLAttribute *att_result = NULL;
				u32 att_index = 0;
				while (1) {
					att = (GF_XMLAttribute*)gf_list_get(validator->xvs_node_in_xvl->attributes, att_index);
					if (!att) {
						break;
					} else if (!strcmp(att->name, "result")) {
						att_result = att;
					}
					att_index++;
				}
				if (!att_result) {
					GF_SAFEALLOC(att_result, GF_XMLAttribute);
					if (!att_result) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Failed to allocate result attribute\n"));
						return;
					}
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
	validator->root_odm = NULL;
	validator->xvs_event_index = 0;
	validator->snapshot_number = 0;
}

static void validator_test_open(GF_Validator *validator)
{
	if (!validator->trace_mode) {
		char filename[100];
		if (validator->test_base)
			sprintf(filename, "%s%c%s", validator->test_base, GF_PATH_SEPARATOR, validator->test_filename);
		else
			sprintf(filename, "%s", validator->test_filename);

//deprecated		gf_sc_add_video_listener(validator->compositor, &validator->video_listener);
		if (validator->is_recording)
			validator->snapshot_next_frame = GF_TRUE;
		gf_sc_connect_from_time_ex(validator->compositor, filename, 0, 0, 0, NULL);

	}
//	validator->ck = validator->compositor->root_scene->scene_codec ? validator->compositor->root_scene->scene_codec->ck : validator->compositor->root_scene->dyn_ck;
}

static Bool validator_xvs_next(GF_Validator *validator, Bool reverse)
{
	if (validator->xvl_node) {
		validator_xvl_get_next_xvs(validator, reverse);
		if (validator->xvs_filename) {
			validator_xvs_open(validator);
			if (!validator->xvs_node) {
				return GF_FALSE;
			}
			if (validator->test_filename) {
				validator_test_open(validator);
			} else {
				validator_xvs_close(validator);
				return GF_FALSE;
			}
		} else {
			return GF_FALSE;
		}
		return GF_TRUE;
	} else {
		return GF_FALSE;
	}
}

static Bool validator_load_event(GF_Validator *validator)
{
	GF_XMLNode *event_node;
	u32 att_index;

	memset(&validator->next_event, 0, sizeof(GF_Event));
	validator->evt_loaded = GF_FALSE;
	validator->next_event_snapshot = GF_FALSE;

	if (!validator->xvs_node) {
		validator->compositor->validator_mode = GF_FALSE;
		return GF_FALSE;
	}

	while (1) {
		event_node = (GF_XMLNode*)gf_list_get(validator->xvs_node->content, validator->xvs_event_index);
		if (!event_node) {
			return GF_FALSE;
		} else if (event_node->type == GF_XML_NODE_TYPE) {
			validator->xvs_event_index++;
			break;
		} else {
			validator->xvs_event_index++;
		}
	}

	if (!strcmp(event_node->name, "snapshot")) {
		validator->next_event_snapshot = GF_TRUE;
	} else {
#ifndef GPAC_DISABLE_SVG
		validator->next_event.type = gf_dom_event_type_by_name(event_node->name);
		if (validator->next_event.type == GF_EVENT_UNKNOWN)
#endif
		{
			return GF_TRUE;
		}
	}

	att_index = 0;
	while (1) {
		GF_XMLAttribute *att = (GF_XMLAttribute*)gf_list_get(event_node->attributes, att_index);
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
#ifndef GPAC_DISABLE_SVG
		} else if (!strcmp(att->name, "key_identifier")) {
			validator->next_event.key.key_code = gf_dom_get_key_type(att->value);
#endif
		} else if (!strcmp(att->name, "unicode-char")) {
			validator->next_event.character.unicode_char = atoi(att->value);
		}
		att_index++;
	}
	validator->evt_loaded = GF_TRUE;
	validator->compositor->sys_frames_pending = GF_TRUE;
	return GF_TRUE;
}

static Bool validator_process(GF_CompositorExt *termext, u32 action, void *param)
{
	const char *opt;
	GF_Validator *validator = (GF_Validator*)termext->udta;

	switch (action) {

	/* Upon starting of the terminal, we parse (possibly an XVL file), an XVS file, and start the first test sequence */
	case GF_COMPOSITOR_EXT_START:
		validator->compositor = (GF_Compositor *) param;

		/* Check if the validator should be loaded and in which mode */
		opt = gf_opts_get_key("Validator", "Mode");
		if (!opt) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("Validator missing configuration, stopping.\n"));
			return GF_FALSE;
		} else if (!strcmp(opt, "Play") ) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Validator starting in playback mode.\n"));
			validator->is_recording = GF_FALSE;
			//this will indicate to the compositor to increment scene time even though no new changes
			validator->compositor->validator_mode = GF_TRUE;
		} else if (!strcmp(opt, "Record")) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Validator starting in recording mode.\n"));
			validator->is_recording = GF_TRUE;
		} else if (!strcmp(opt, "Disable")) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Validator is disabled.\n"));
			return GF_FALSE;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("Validator configuration using wrong mode, stopping.\n"));
			return GF_FALSE;
		}

		gf_opts_set_key("Validator", "Mode", "Disable");

		/* initializes the validator and starts */
		validator->xvs_filename = NULL;
		validator->xvl_filename = (char *)gf_opts_get_key("Validator", "XVL");
		if (!validator->xvl_filename) {
			validator->xvs_filename = (char *)gf_opts_get_key("Validator", "XVS");
			if (!validator->xvs_filename) {
				validator->xvs_filename = (char *)gf_opts_get_key("Validator", "Trace");
				if (!validator->xvs_filename) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("Validator configuration without input, stopping.\n"));
					return GF_FALSE;
				}
				validator->test_filename = validator->xvs_filename;
				validator->trace_mode = GF_TRUE;
				GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Validator using trace file: %s\n", validator->xvs_filename));
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Validator using scenario file: %s\n", validator->xvs_filename));
			}
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Validator using scenario playlist: %s\n", validator->xvl_filename));
		}

		/* TODO: if start returns 0, the module is not loaded, so the above init (filter registration) is not removed,
		   should probably return 1 all the time, to make sure stop is called */
		if (validator->xvl_filename) {
			validator_xvl_open(validator);
			if (!validator->xvl_node) {
				return GF_FALSE;
			}
			validator_xvs_next(validator, GF_FALSE);
			if (!validator->xvs_node) {
				return GF_FALSE;
			}
		} else if (validator->xvs_filename) {
			validator_xvs_open(validator);
			if (!validator->xvs_node) {
				return GF_FALSE;
			}
			if (validator->test_filename) {
				validator_test_open(validator);
			} else {
				validator_xvs_close(validator);
				return GF_FALSE;
			}
		} else {
			return GF_FALSE;
		}

		validator->evt_filter.udta = validator;
		if (!validator->is_recording) {
			validator->evt_filter.on_event = validator_on_event_play;
			termext->caps |= GF_COMPOSITOR_EXTENSION_NOT_THREADED;
		} else {
			validator->evt_filter.on_event = validator_on_event_record;
		}
		gf_filter_add_event_listener(validator->compositor->filter, &validator->evt_filter);
		validator->video_listener.udta = validator;
		validator->video_listener.on_video_frame = validator_on_video_frame;
		validator->video_listener.on_video_reconfig = validator_on_video_reconfig;


		if (!validator->is_recording) {
			validator_load_event(validator);
		}
		return GF_TRUE;

	/* when the terminal stops, we close the XVS parser and XVL parser if any, restore the config,
	and free all validator data (the validator will be destroyed when the module is unloaded)
	Note: we don't need to disconnect the terminal since it's already stopping */
	case GF_COMPOSITOR_EXT_STOP:
		gf_filter_remove_event_listener(validator->compositor->filter, &validator->evt_filter);
		validator_xvs_close(validator);
		validator_xvl_close(validator);
		validator->compositor = NULL;
		if (validator->test_base) {
			gf_free(validator->test_base);
			validator->test_base = NULL;
		}
		/*auto-disable the recording by default*/
		if (!validator->trace_mode) {
			if (validator->is_recording ) {
				gf_opts_set_key("Validator", "Mode", "Play");
			} else {
				gf_opts_set_key("Validator", "Mode", "Disable");
			}
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Stopping validator\n"));
		break;

	/* When called in the main loop of the terminal, we don't do anything in the recording mode.
	   In the playing/validating mode, we need to check if an event needs to be dispatched or if snapshots need to be made,
	   until there is no more event, in which case we trigger either the load of the next XVS or the quit */
	case GF_COMPOSITOR_EXT_PROCESS:
		/* if the time is right, dispatch the event and load the next one */
		while (!validator->is_recording && validator->evt_loaded && validator->root_odm && validator->root_odm->ck && (validator->next_time <= gf_clock_time(validator->root_odm->ck) )) {
			Bool has_more_events;
			//u32 diff = gf_clock_time(validator->ck) - validator->next_time;
			//GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Validator] Time diff: evt_time=%d  clock_time = %d, diff=%d\n", validator->next_time, gf_clock_time(validator->ck), diff));
			if (validator->next_event_snapshot) {
#ifndef GPAC_DISABLE_AV_PARSERS
				Bool res;
				char *snap_name = validator_create_snapshot(validator);
				gf_free(snap_name);
				res = validator_compare_snapshots(validator);
				validator->xvs_result &= res;
				validator->next_event_snapshot = GF_FALSE;
#endif
			} else {
				gf_sc_on_event(validator->compositor, &validator->next_event);
			}
			has_more_events = validator_load_event(validator);
			if (!has_more_events) {
				validator_xvs_close(validator);
				if (! validator->trace_mode) {
					gf_sc_disconnect(validator->compositor);
//deprecated					gf_sc_remove_video_listener(validator->compositor, &validator->video_listener);
					validator_xvs_next(validator, GF_FALSE);
					if (!validator->xvs_node) {
						GF_Event evt;
						memset(&evt, 0, sizeof(GF_Event));
						evt.type = GF_EVENT_QUIT;
						gf_sc_on_event(validator->compositor, &evt);
					} else {
						if (!validator->is_recording) {
							validator_load_event(validator);
						}
					}
				}
			}
		}
		break;
	}
	return GF_FALSE;
}


GF_CompositorExt *validator_new()
{
	GF_CompositorExt *dr;
	GF_Validator *validator;
	dr = (GF_CompositorExt*)gf_malloc(sizeof(GF_CompositorExt));
	memset(dr, 0, sizeof(GF_CompositorExt));
	GF_REGISTER_MODULE_INTERFACE(dr, GF_COMPOSITOR_EXT_INTERFACE, "GPAC Test Validator", "gpac distribution");

	GF_SAFEALLOC(validator, GF_Validator);
	if (!validator) {
		gf_free(dr);
		return NULL;
	}
	dr->process = validator_process;
	dr->udta = validator;
	return dr;
}


void validator_delete(GF_BaseInterface *ifce)
{
	GF_CompositorExt *dr = (GF_CompositorExt *) ifce;
	GF_Validator *validator = (GF_Validator*)dr->udta;
	gf_free(validator);
	gf_free(dr);
}

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_COMPOSITOR_EXT_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_COMPOSITOR_EXT_INTERFACE) return (GF_BaseInterface *)validator_new();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_COMPOSITOR_EXT_INTERFACE:
		validator_delete(ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( validator )
