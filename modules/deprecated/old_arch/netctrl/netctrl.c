/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2011-2012
 *			All rights reserved
 *
 *  This file is part of GPAC / Sampe On-Scvreen Display sub-project
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
#include <gpac/nodes_mpeg4.h>
#include <gpac/network.h>

#define XMIN 160
#define XMAX 480

#define YMAX 360
#define YMIN 120

typedef struct
{
	GF_Terminal *term;
	GF_Socket *sock;
	Bool mouse_down;
	u32 cnt;
	Float last_min_x, last_max_x, last_min_y, last_max_y;
} GF_NetControl;

static Bool netctrl_process(GF_TermExt *termext, u32 action, void *param)
{
	const char *sOpt, *server_ip;
	int port;
	Float face_min_x, face_max_x, face_min_y, face_max_y, gaze_x, gaze_y;
	GF_Event event;
	GF_NetControl *netctrl = termext->udta;
	char message[1024];
	GF_Err e;
	u32 bytes;
	u32 face = 0;

	switch (action) {
	case GF_TERM_EXT_START:
		netctrl->term = (GF_Terminal *) param;

		sOpt = gf_opts_get_key("NetControler", "Enabled");
		if (!sOpt || strcmp(sOpt, "yes")) return 0;
		sOpt = gf_opts_get_key("NetControler", "ServerIP");
		if (sOpt) server_ip = sOpt;
		else server_ip = "127.0.0.1";
		sOpt = gf_opts_get_key("NetControler", "Port");
		if (sOpt) port = atoi(sOpt);
		else port = 20320;

		termext->caps |= GF_TERM_EXTENSION_NOT_THREADED;
		netctrl->sock = gf_sk_new(GF_SOCK_TYPE_UDP);

		if (netctrl->sock < 0)   {
			GF_LOG(GF_LOG_ERROR, GF_LOG_INTERACT, ("[NetControl] Failed to open socket for %s:%d\n", server_ip, port));
			return 0;
		}

		e = gf_sk_bind(netctrl->sock, server_ip, port, NULL, 0, 0);
		if (e != GF_OK) {
			if (netctrl->sock) gf_sk_del(netctrl->sock);
			netctrl->sock = NULL;
			GF_LOG(GF_LOG_ERROR, GF_LOG_INTERACT, ("[NetControl] Failed to bind to socket %s:%d\n", server_ip, port));
			return 0;
		}

		return 1;

	case GF_TERM_EXT_STOP:
		if (netctrl->sock) gf_sk_del(netctrl->sock);
		break;

	case GF_TERM_EXT_PROCESS:
		gf_sk_receive(netctrl->sock, message, 1024, 0, &bytes);
		if (!bytes) break;
		message[bytes] = '\0';
		GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[NetControl] received message %s\n", message));

		if (!strncmp(message, "gpac splice ", 12) || !strncmp(message, "gpac add ", 9)
			|| !strncmp(message, "gpac select ", 12)
		) {
			gf_term_scene_update(netctrl->term, NULL, message);
			break;
		}

		if (strncmp(message, "gpac:face=", 10)) break;

		sscanf(message, "gpac:face=%d,%f,%f,%f,%f", &face, &face_min_x, &face_max_x, &face_min_y, &face_max_y);


		memset(&event, 0, sizeof(GF_Event));
		event.mouse.button = GF_MOUSE_LEFT;
		if (face == 0) {
			if (netctrl->last_min_x < 0.01) {
				netctrl->term->compositor->auto_rotate = 2;
			} else if (netctrl->last_max_x > 0.90) {
				netctrl->term->compositor->auto_rotate = 1;
			} else if (netctrl->last_min_y < 0.01) {
				netctrl->term->compositor->auto_rotate = 4;
			} else if (netctrl->last_max_y > 0.90) {
				netctrl->term->compositor->auto_rotate = 3;
			} else {
				netctrl->cnt++;
				if ((netctrl->cnt>=50) && (netctrl->mouse_down)) {
					netctrl->term->compositor->auto_rotate = 0;
					netctrl->mouse_down = GF_FALSE;
					event.type = GF_EVENT_MOUSEUP;
					netctrl->term->compositor->video_out->on_event(netctrl->term->compositor->video_out->evt_cbk_hdl, &event);
				}
			}
			break;
		}

		netctrl->last_min_x = face_min_x;
		netctrl->last_max_x = face_max_x;
		netctrl->last_min_y = face_min_y;
		netctrl->last_max_y = face_max_y;

		gaze_x = (face_min_x+face_max_x)/2;
		gaze_y = (face_min_y+face_max_y)/2;

		event.mouse.x = (1-gaze_x) * netctrl->term->compositor->display_width;
		event.mouse.y = (1-gaze_y) * netctrl->term->compositor->display_height;

		if (!netctrl->mouse_down) {
			//don't grab if mouse is down
			if (netctrl->term->compositor->navigation_state) break;
			netctrl->mouse_down = GF_TRUE;
			event.type = GF_EVENT_MOUSEDOWN;
			netctrl->term->compositor->video_out->on_event(netctrl->term->compositor->video_out->evt_cbk_hdl, &event);
			netctrl->cnt = 0;
		}
		event.type = GF_EVENT_MOUSEMOVE;
		netctrl->term->compositor->video_out->on_event(netctrl->term->compositor->video_out->evt_cbk_hdl, &event);
		break;
	}
	return 0;
}


GF_TermExt *netctrl_new()
{
	GF_TermExt *dr;
	GF_NetControl *netctrl;
	dr = (GF_TermExt*)gf_malloc(sizeof(GF_TermExt));
	memset(dr, 0, sizeof(GF_TermExt));
	GF_REGISTER_MODULE_INTERFACE(dr, GF_TERM_EXT_INTERFACE, "GPAC NetControl", "gpac distribution");

	GF_SAFEALLOC(netctrl, GF_NetControl);
	dr->process = netctrl_process;
	dr->udta = netctrl;
	return dr;
}


void netctrl_delete(GF_BaseInterface *ifce)
{
	GF_TermExt *dr = (GF_TermExt *) ifce;
	GF_NetControl *netctrl = dr->udta;
	gf_free(netctrl);
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
	if (InterfaceType == GF_TERM_EXT_INTERFACE) return (GF_BaseInterface *)netctrl_new();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_TERM_EXT_INTERFACE:
		netctrl_delete(ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( netctrl )
