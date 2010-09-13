/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *			Copyright (c) 2007-200X ENST
 *			All rights reserved
 *
 *  This file is part of GPAC / User Event Recorder sub-project
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

/*base SVG type*/
#include <gpac/modules/term_ext.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>


typedef struct __ui_rec
{
	FILE *uif;
	GF_BitStream *bs;
	GF_Terminal *term;
	GF_Clock *ck;

	GF_Event next_event;
	u32 next_time;
	Bool evt_loaded;

	Bool (*on_event)(struct __ui_rec *uir , GF_Event *event);
} GF_UIRecord;

Bool uir_on_event_play(GF_UIRecord *uir , GF_Event *event)
{
	switch (event->type) {
	case GF_EVENT_CONNECT:
		if (event->connect.is_connected) {
			uir->ck = uir->term->root_scene->scene_codec ? uir->term->root_scene->scene_codec->ck : uir->term->root_scene->dyn_ck;
		}
		break;
	}
	return 0;
}

Bool uir_on_event_record(GF_UIRecord *uir , GF_Event *event)
{
	switch (event->type) {
	case GF_EVENT_CONNECT:
		if (event->connect.is_connected) {
			uir->ck = uir->term->root_scene->scene_codec ? uir->term->root_scene->scene_codec->ck : uir->term->root_scene->dyn_ck;
		}
		break;
	case GF_EVENT_CLICK: 
	case GF_EVENT_MOUSEUP:
	case GF_EVENT_MOUSEDOWN: 
	case GF_EVENT_MOUSEOVER: 
	case GF_EVENT_MOUSEOUT:
	/*!! ALL MOUSE EVENTS SHALL BE DECLARED BEFORE MOUSEMOVE !! */
	case GF_EVENT_MOUSEMOVE:
	/*mouse wheel event*/
	case GF_EVENT_MOUSEWHEEL:
		gf_bs_write_u32(uir->bs, gf_clock_time(uir->ck) );
		gf_bs_write_u8(uir->bs, event->type);
		gf_bs_write_u8(uir->bs, event->mouse.button);
		gf_bs_write_u32(uir->bs, event->mouse.x);
		gf_bs_write_u32(uir->bs, event->mouse.y);
		gf_bs_write_float(uir->bs, FIX2FLT(event->mouse.wheel_pos) );
		gf_bs_write_u8(uir->bs, event->mouse.key_states);
		break;
	/*Key Events*/
	case GF_EVENT_KEYUP:
	case GF_EVENT_KEYDOWN:
	case GF_EVENT_LONGKEYPRESS:
		gf_bs_write_u32(uir->bs, gf_clock_time(uir->ck) );
		gf_bs_write_u8(uir->bs, event->type);
		gf_bs_write_u32(uir->bs, event->key.key_code);
		gf_bs_write_u32(uir->bs, event->key.hw_code);
		gf_bs_write_u32(uir->bs, event->key.flags);
		break;
	/*character input*/
	case GF_EVENT_TEXTINPUT:
		gf_bs_write_u32(uir->bs, gf_clock_time(uir->ck) );
		gf_bs_write_u8(uir->bs, event->type);
		gf_bs_write_u32(uir->bs, event->character.unicode_char);
		break;
	}
	return 0;
}

void uir_load_event(GF_UIRecord *uir)
{
	memset(&uir->next_event, 0, sizeof(GF_Event));
	uir->evt_loaded = 0;
	if (!gf_bs_available(uir->bs)) return;


	uir->next_time = gf_bs_read_u32(uir->bs);
	uir->next_event.type = gf_bs_read_u8(uir->bs);
	switch (uir->next_event.type) {
	case GF_EVENT_CLICK: 
	case GF_EVENT_MOUSEUP:
	case GF_EVENT_MOUSEDOWN: 
	case GF_EVENT_MOUSEOVER: 
	case GF_EVENT_MOUSEOUT:
	/*!! ALL MOUSE EVENTS SHALL BE DECLARED BEFORE MOUSEMOVE !! */
	case GF_EVENT_MOUSEMOVE:
	/*mouse wheel event*/
	case GF_EVENT_MOUSEWHEEL:
		uir->next_event.mouse.button = gf_bs_read_u8(uir->bs);
		uir->next_event.mouse.x = gf_bs_read_u32(uir->bs);
		uir->next_event.mouse.y = gf_bs_read_u32(uir->bs);
		uir->next_event.mouse.wheel_pos = FLT2FIX( gf_bs_read_float(uir->bs) );
		uir->next_event.mouse.key_states = gf_bs_read_u8(uir->bs);
		break;
	/*Key Events*/
	case GF_EVENT_KEYUP:
	case GF_EVENT_KEYDOWN:
	case GF_EVENT_LONGKEYPRESS:
		uir->next_event.key.key_code = gf_bs_read_u32(uir->bs);
		uir->next_event.key.hw_code = gf_bs_read_u32(uir->bs);
		uir->next_event.key.flags = gf_bs_read_u32(uir->bs);
		break;
	/*character input*/
	case GF_EVENT_TEXTINPUT:
		uir->next_event.character.unicode_char = gf_bs_read_u32(uir->bs);
		break;
	}
	uir->evt_loaded = 1;
}

static Bool uir_process(GF_TermExt *termext, u32 action, void *param)
{
	const char *opt, *uifile;
	GF_UIRecord *uir = termext->udta;

	switch (action) {
	case GF_TERM_EXT_START:
		uir->term = (GF_Terminal *) param;
		opt = gf_modules_get_option((GF_BaseInterface*)termext, "UIRecord", "Mode");
		if (!opt) return 0;
		uifile = gf_modules_get_option((GF_BaseInterface*)termext, "UIRecord", "File");
		if (!opt) return 0;

		if (!strcmp(opt, "Play")) {
			uir->uif = fopen(uifile, "rb");
			if (!uir->uif) return 0;
			uir->bs = gf_bs_from_file(uir->uif, GF_BITSTREAM_READ);
			termext->caps |= GF_TERM_EXTENSION_NOT_THREADED;

			uir->on_event = uir_on_event_play;
			termext->caps |= GF_TERM_EXTENSION_FILTER_EVENT;

			uir_load_event(uir);
		} else if (!strcmp(opt, "Record")) {
			uir->uif = fopen(uifile, "wb");
			if (!uir->uif) return 0;
			uir->bs = gf_bs_from_file(uir->uif, GF_BITSTREAM_WRITE);

			uir->on_event = uir_on_event_record;
			termext->caps |= GF_TERM_EXTENSION_FILTER_EVENT;
		} else {
			return 0;
		}
		return 1;

	case GF_TERM_EXT_STOP:
		if (uir->uif) fclose(uir->uif);
		if (uir->bs) gf_bs_del(uir->bs);
		uir->term = NULL;
		/*auto-disable the plugin by default*/
		gf_modules_set_option((GF_BaseInterface*)termext, "UIRecord", "Mode", "Disable");
		break;

	case GF_TERM_EXT_PROCESS:
		/*flush all events until current time*/
		while (uir->evt_loaded && uir->ck && (uir->next_time <= gf_clock_time(uir->ck) )) {
			uir->term->compositor->video_out->on_event(uir->term->compositor->video_out->evt_cbk_hdl, &uir->next_event);
			uir_load_event(uir);
		}
		break;
	case GF_TERM_EXT_EVENT:
		return uir->on_event(uir, (GF_Event*)param);
	}
	return 0;
}


GF_TermExt *uir_new()
{
	GF_TermExt *dr;
	GF_UIRecord *uir;
	dr = malloc(sizeof(GF_TermExt));
	memset(dr, 0, sizeof(GF_TermExt));
	GF_REGISTER_MODULE_INTERFACE(dr, GF_TERM_EXT_INTERFACE, "GPAC UI Recorder", "gpac distribution");

	GF_SAFEALLOC(uir, GF_UIRecord);
	dr->process = uir_process;
	dr->udta = uir;
	return dr;
}


void uir_delete(GF_BaseInterface *ifce)
{
	GF_TermExt *dr = (GF_TermExt *) ifce;
	GF_UIRecord *uir = dr->udta;
	free(uir);
	free(dr);
}

GF_EXPORT
const u32 *QueryInterfaces() 
{
	static u32 si [] = {
		GF_TERM_EXT_INTERFACE,
		0
	};
	return si; 
}

GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_TERM_EXT_INTERFACE) return (GF_BaseInterface *)uir_new();
	return NULL;
}

GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_TERM_EXT_INTERFACE:
		uir_delete(ifce);
		break;
	}
}
