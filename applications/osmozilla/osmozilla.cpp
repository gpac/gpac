/*
*			GPAC - Multimedia Framework C SDK
*
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
*					All rights reserved
*
*  This file is part of GPAC / Osmozilla NPAPI plugin
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

#include "osmozilla.h"

#ifdef XP_WIN
#include <windows.h>
#endif

#include <gpac/options.h>
#include <gpac/terminal.h>
#include <gpac/term_info.h>


short Osmozilla_GetURL(NPP instance, const char *url, const char *target);
void Osmozilla_SetStatus(NPP instance, const char *message);


void Osmozilla_Shutdown(Osmozilla *osmo)
{
	if (osmo->url) gf_free(osmo->url);
	osmo->url = NULL;
	if (osmo->term) {
		GF_Terminal *a_term = osmo->term;
		osmo->term = NULL;
		gf_term_del(a_term);
	}
	if (osmo->user) {
		if (osmo->user->modules) gf_modules_del(osmo->user->modules);
		if (osmo->user->config) gf_cfg_del(osmo->user->config);
		gf_free(osmo->user);
		osmo->user = NULL;
	}
}

static void osmozilla_do_log(void *cbk, u32 level, u32 tool, const char *fmt, va_list list)
{
	FILE *logs = (FILE *) cbk;
	vfprintf(logs, fmt, list);
	fflush(logs);
}


Bool Osmozilla_EventProc(void *opaque, GF_Event *evt)
{
	char msg[1024];
	Osmozilla *osmo = (Osmozilla *)opaque;
	if (!osmo->term) return GF_FALSE;

	switch (evt->type) {
	case GF_EVENT_MESSAGE:
		if (!evt->message.message) return GF_FALSE;
		if (evt->message.error)
			sprintf((char *)msg, "GPAC: %s (%s)", evt->message.message, gf_error_to_string(evt->message.error));
		else
			sprintf((char *)msg, "GPAC: %s", evt->message.message);

		Osmozilla_SetStatus(osmo->np_instance, msg);
		break;
	case GF_EVENT_PROGRESS:
		if (evt->progress.done == evt->progress.total) {
			Osmozilla_SetStatus(osmo->np_instance, "");
			osmo->download_progress = 100;
		} else {
			char *szTitle = (char *)"";
			if (evt->progress.progress_type==0) szTitle = (char *)"Buffer ";
			else if (evt->progress.progress_type==1)
			{
				szTitle = (char *)"Download ";
				osmo->download_progress = (int) (100.0*evt->progress.done) / evt->progress.total;
			}
			else if (evt->progress.progress_type==2) szTitle = (char *)"Import ";

			sprintf(msg, "(GPAC) %s: %02.2f", szTitle, (100.0*evt->progress.done) / evt->progress.total);
			Osmozilla_SetStatus(osmo->np_instance, msg);
		}
		break;

	/*IGNORE any scene size, just work with the size allocated in the parent doc*/
	case GF_EVENT_SCENE_SIZE:
		gf_term_set_size(osmo->term, osmo->width, osmo->height);
		break;
	/*window has been resized (full-screen plugin), resize*/
	case GF_EVENT_SIZE:
		osmo->width = evt->size.width;
		osmo->height = evt->size.height;
		gf_term_set_size(osmo->term, osmo->width, osmo->height);
		break;
	case GF_EVENT_CONNECT:
		osmo->is_connected = evt->connect.is_connected;
		break;
	case GF_EVENT_DURATION:
		osmo->can_seek = evt->duration.can_seek;
		osmo->duration = evt->duration.duration;
		break;
	case GF_EVENT_DBLCLICK:
		gf_term_set_option(osmo->term, GF_OPT_FULLSCREEN, !gf_term_get_option(osmo->term, GF_OPT_FULLSCREEN));
		break;
	case GF_EVENT_NAVIGATE_INFO:
		strcpy(msg, evt->navigate.to_url);
		Osmozilla_SetStatus(osmo->np_instance, msg);
		break;
	case GF_EVENT_NAVIGATE:
		if (gf_term_is_supported_url(osmo->term, evt->navigate.to_url, GF_TRUE, osmo->disable_mime ? GF_TRUE : GF_FALSE)) {
			gf_term_navigate_to(osmo->term, evt->navigate.to_url);
			return GF_TRUE;
		} else {
			u32 i;
			char *target = (char *)"_self";

			for (i=0; i<evt->navigate.param_count; i++) {
				if (!strcmp(evt->navigate.parameters[i], "_parent")) target = (char *)"_parent";
				else if (!strcmp(evt->navigate.parameters[i], "_blank")) target = (char *)"_blank";
				else if (!strcmp(evt->navigate.parameters[i], "_top")) target = (char *)"_top";
				else if (!strcmp(evt->navigate.parameters[i], "_new")) target = (char *)"_new";
				else if (!strnicmp(evt->navigate.parameters[i], "_target=", 8)) target = (char *) evt->navigate.parameters[i]+8;
			}
			Osmozilla_GetURL(osmo->np_instance, evt->navigate.to_url, target);
			return GF_TRUE;
		}
		break;
	}
	return GF_FALSE;
}

int Osmozilla_Initialize(Osmozilla *osmo, signed short argc, char* argn[], char* argv[])
{
	const char *str;
	int i;
	osmo->auto_start = 1;
	osmo->use_gui = 0;

	/*options sent from plugin*/
	for(i=0; i<argc; i++) {
		if (!argn[i] || !argv[i]) continue;
		if (!stricmp(argn[i],"autostart") && (!stricmp(argv[i], "false") || !stricmp(argv[i], "no")) )
			osmo->auto_start = 0;

		else if (!stricmp(argn[i],"src") ) {
			if (osmo->url) gf_free(osmo->url);
			osmo->url = gf_strdup(argv[i]);
		}
		else if (!stricmp(argn[i],"use3d") && (!stricmp(argv[i], "true") || !stricmp(argv[i], "yes") ) ) {
			osmo->use_3d = 1;
		}
		else if (!stricmp(argn[i],"loop") && (!stricmp(argv[i], "true") || !stricmp(argv[i], "yes") ) ) {
			osmo->loop = 1;
		}
		else if (!stricmp(argn[i],"aspectratio")) {
			osmo->aspect_ratio = GF_ASPECT_RATIO_KEEP;
			if (!stricmp(argv[i], "keep")) osmo->aspect_ratio = GF_ASPECT_RATIO_KEEP;
			else if (!stricmp(argv[i], "16:9")) osmo->aspect_ratio = GF_ASPECT_RATIO_16_9;
			else if (!stricmp(argv[i], "4:3")) osmo->aspect_ratio = GF_ASPECT_RATIO_4_3;
			else if (!stricmp(argv[i], "fill")) osmo->aspect_ratio = GF_ASPECT_RATIO_FILL_SCREEN;
		}
		else if (!stricmp(argn[i],"gui") && (!stricmp(argv[i], "true") || !stricmp(argv[i], "yes") ) )
			osmo->use_gui = 1;
	}

	/*URL is not absolute, request new stream to mozilla - we don't pass absolute URLs since some may not be
	handled by gecko */
	if (osmo->url) {
		Bool absolute_url = GF_FALSE;
		if (strstr(osmo->url, "://")) absolute_url = GF_TRUE;
		else if (osmo->url[0] == '/') {
			FILE *test = gf_f64_open(osmo->url, "rb");
			if (test) {
				absolute_url = GF_TRUE;
				fclose(test);
			}
		}
		else if ((osmo->url[1] == ':') && ((osmo->url[2] == '\\') || (osmo->url[2] == '/'))) absolute_url = GF_TRUE;

		if (!absolute_url) {
			char *url = osmo->url;
			osmo->url = NULL;
			Osmozilla_GetURL(osmo->np_instance, url, NULL);
			gf_free(url);
		}
	}

	GF_SAFEALLOC(osmo->user, GF_User);
	osmo->user->config = gf_cfg_init(NULL, NULL);
	/*need to have a valid cfg file for now*/
	if (!osmo->user->config) {
		gf_free(osmo->user);
		osmo->user = NULL;
#ifdef WIN32
		MessageBox(NULL, "GPAC CONFIGURATION FILE NOT FOUND OR INVALID", "OSMOZILLA FATAL ERROR", MB_OK);
#else
		fprintf(stdout, "OSMOZILLA FATAL ERROR\nGPAC CONFIGURATION FILE NOT FOUND OR INVALID\n");
#endif
		return 0;
	}

	osmo->user->modules = gf_modules_new(NULL, osmo->user->config);
	if (!gf_modules_get_count(osmo->user->modules)) {
		if (osmo->user->modules) gf_modules_del(osmo->user->modules);
		gf_free(osmo->user);
		osmo->user = NULL;
#ifdef WIN32
		MessageBox(NULL, "GPAC MODULES NOT FOUND", "OSMOZILLA FATAL ERROR", MB_OK);
#else
		fprintf(stdout, "OSMOZILLA FATAL ERROR\nGPAC MODULES NOT FOUND\n");
#endif
		return 0;
	}

	osmo->user->opaque = osmo;
	osmo->user->EventProc = Osmozilla_EventProc;

	/*always fetch mime ? Check with anchor examples*/
	osmo->disable_mime = 0;
	str = gf_cfg_get_key(osmo->user->config, "General", "NoMIMETypeFetch");
	if (str && !strcmp(str, "yes")) osmo->disable_mime = 0;
	/*check log file*/
	str = gf_cfg_get_key(osmo->user->config, "General", "LogFile");
	if (str) {
		osmo->logs = gf_f64_open(str, "wt");
		if (osmo->logs) gf_log_set_callback(osmo->logs, osmozilla_do_log);
	}

	/*setup logs*/
	if (gf_log_set_tools_levels(gf_cfg_get_key(osmo->user->config, "General", "Logs")) != GF_OK)
		fprintf(stdout, "Osmozilla: invalid log level specified\n");

	fprintf(stdout, "Osmozilla initialized\n");
	return 1;
}

int Osmozilla_SetWindow(Osmozilla *osmo, void *os_wnd_handle, void *os_wnd_display, unsigned int width, unsigned int height)
{
	const char *gui;

	if (!osmo->user) return 0;

	if (osmo->window_set) {
		osmo->width = width;
		osmo->height = height;
		if (osmo->is_connected) gf_term_set_size(osmo->term, osmo->width, osmo->height);
		return 1;
	}
	if (!os_wnd_handle) return 0;

	osmo->width = width;
	osmo->height = height;

	osmo->user->os_window_handler = os_wnd_handle;
	osmo->user->os_display = os_wnd_display;

	/*Everything is now setup, create the terminal*/
	fprintf(stdout, "Creating Osmozilla terminal\n");
	osmo->term = gf_term_new(osmo->user);
	if (!osmo->term) return 0;
	fprintf(stdout, "Osmozilla terminal created\n");

	gf_term_set_option(osmo->term, GF_OPT_ASPECT_RATIO, osmo->aspect_ratio);
	osmo->window_set = 1;

#ifdef XP_WIN
	SetFocus((HWND)os_wnd_handle);
#endif

	/*stream not ready*/
	if (!osmo->url || !osmo->auto_start) {
		fprintf(stdout, "Osmozilla ready - not connecting to %s yet\n", osmo->url);
		return 1;
	}

	/*connect from 0 and pause if not autoplay*/
	gui = gf_cfg_get_key(osmo->user->config, "General", "StartupFile");
	if (gui && osmo->use_gui) {
		gf_cfg_set_key(osmo->user->config, "Temp", "BrowserMode", "yes");
		gf_cfg_set_key(osmo->user->config, "Temp", "GUIStartupFile", osmo->url);
		gf_term_connect(osmo->term, gui);
	} else {
		gf_term_connect(osmo->term, osmo->url);
	}
	fprintf(stdout, "Osmozilla connected to %s\n", osmo->url);
	return 1;
}

char *Osmozilla_GetVersion()
{
	return (char *) "GPAC Plugin " GPAC_FULL_VERSION " for NPAPI compatible Web Browsers. For more information go to <a href=\"http://gpac.sourceforge.net\">GPAC website</a>";
}

void Osmozilla_ConnectTo(Osmozilla *osmo, const char *url)
{
	if (!osmo->user) return;

	if ( osmo->url && !strcmp(url, osmo->url))
		return;

	fprintf(stdout, "Osmozilla connecting to %s\n", url);

	if (osmo->url) gf_free(osmo->url);
	osmo->url = gf_strdup(url);

	/*connect from 0 and pause if not autoplay*/
	if (osmo->auto_start) {
		const char *gui = gf_cfg_get_key(osmo->user->config, "General", "StartupFile");
		if (gui && osmo->use_gui) {
			gf_cfg_set_key(osmo->user->config, "Temp", "BrowserMode", "yes");
			gf_cfg_set_key(osmo->user->config, "Temp", "GUIStartupFile", url);
			gf_term_connect(osmo->term, gui);
		} else {
			gf_term_connect(osmo->term, url);
		}
	}
	fprintf(stdout, "Osmozilla connected to %s\n", url);
}

void Osmozilla_Pause(Osmozilla *osmo)
{
	if (osmo->term) {
		if (gf_term_get_option(osmo->term, GF_OPT_PLAY_STATE) == GF_STATE_PAUSED) {
			gf_term_set_option(osmo->term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
		} else {
			gf_term_set_option(osmo->term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
		}
	}
}

void Osmozilla_Play(Osmozilla *osmo)
{
	if (!osmo->is_connected) {
		if (osmo->url) gf_term_connect(osmo->term, (const char *) osmo->url);
	} else {
		gf_term_set_option(osmo->term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
	}
}

void Osmozilla_Stop(Osmozilla *osmo)
{
	if (osmo->term) gf_term_disconnect(osmo->term);
}

#ifdef XP_WIN
PBITMAPINFO CreateBitmapInfoStruct(GF_VideoSurface *pfb)
{
	PBITMAPINFO pbmi;
	WORD    cClrBits;

	cClrBits = 32;

	pbmi = (PBITMAPINFO) LocalAlloc(LPTR,
	                                sizeof(BITMAPINFOHEADER));

	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth = pfb->width;
	pbmi->bmiHeader.biHeight = 1;
	pbmi->bmiHeader.biPlanes = 1;
	pbmi->bmiHeader.biBitCount = cClrBits;

	pbmi->bmiHeader.biCompression = BI_RGB;
	pbmi->bmiHeader.biSizeImage = ((pbmi->bmiHeader.biWidth * cClrBits +31) & ~31) /8
	                              * pbmi->bmiHeader.biHeight;
	pbmi->bmiHeader.biClrImportant = 0;
	return pbmi;
}
#endif

void Osmozilla_Print(Osmozilla *osmo, unsigned int is_embed, void *os_print_dc, unsigned int target_x, unsigned int target_y, unsigned int target_width, unsigned int target_height)
{
	if (is_embed) {
#ifdef XP_MACOS
		/*
		os_print_dc contains a THPrint reference on MacOS
		*/
	}
#endif  // XP_MACOS
#ifdef XP_UNIX
	/*
	os_print_dc contains a NPPrintCallbackStruct on Unix and
	the plug-in location and size in the NPWindow are in page coordinates (720/ inch), but the printer requires point coordinates (72/inch)
	*/
#endif  // XP_UNIX
#ifdef XP_WIN
	/*
	The coordinates for the window rectangle are in TWIPS format.
	This means that you need to convert the x-y coordinates using the Windows API call DPtoLP when you output text
	*/
	GF_VideoSurface fb;
	u32 xsrc, ysrc;
	u16 src_16;
	char *src;
	float deltay;
	int	ysuiv = 0;
	char *ligne;
	BITMAPINFO	*infoSrc;
	HDC pDC = (HDC)os_print_dc;
	/*lock the source buffer */
	gf_term_get_screen_buffer(osmo->term, &fb);
	infoSrc = CreateBitmapInfoStruct(&fb);
	deltay = (float)target_height/(float)fb.height;
	ligne = (char *) LocalAlloc(GMEM_FIXED, fb.width*4);
	for (ysrc=0; ysrc<fb.height; ysrc++) {
		int ycrt, delta;
		char *dst = (char*)ligne;
		src = fb.video_buffer + ysrc * fb.pitch_y;
		for (xsrc=0; xsrc<fb.width; xsrc++)
		{
			switch (fb.pixel_format) {
			case GF_PIXEL_RGB_32:
			case GF_PIXEL_ARGB:
				dst[0] = src[0];
				dst[1] = src[1];
				dst[2] = src[2];
				src+=4;
				break;
			case GF_PIXEL_BGR_32:
			case GF_PIXEL_RGBA:
				dst[0] = src[3];
				dst[1] = src[2];
				dst[2] = src[1];
				src+=4;
				break;
			case GF_PIXEL_RGB_24:
				dst[0] = src[2];
				dst[1] = src[1];
				dst[2] = src[0];
				src+=3;
				break;
			case GF_PIXEL_BGR_24:
				dst[0] = src[2];
				dst[1] = src[1];
				dst[2] = src[0];
				src+=3;
				break;
			case GF_PIXEL_RGB_565:
				src_16 = * ( (u16 *)src );
				dst[2] = (src_16 >> 8) & 0xf8;
				dst[2] += dst[2]>>5;
				dst[1] = (src_16 >> 3) & 0xfc;
				dst[1] += dst[1]>>6;
				dst[0] = (src_16 << 3) & 0xf8;
				dst[0] += dst[0]>>5;
				src+=2;
				break;
			case GF_PIXEL_RGB_555:
				src_16 = * (u16 *)src;
				dst[2] = (src_16 >> 7) & 0xf8;
				dst[2] += dst[2]>>5;
				dst[1] = (src_16 >> 2) & 0xf8;
				dst[1] += dst[1]>>5;
				dst[0] = (src_16 << 3) & 0xf8;
				dst[0] += dst[0]>>5;
				src+=2;
				break;
			}
			dst += 4;
		}
		ycrt = ysuiv;
		ysuiv = (u32) ( ((float)ysrc+1.0)*deltay);
		delta = ysuiv-ycrt;
		StretchDIBits(
		    pDC, target_x, target_y, target_width,
		    delta,
		    0, 0, fb.width, 1,
		    ligne, infoSrc, DIB_RGB_COLORS, SRCCOPY);
	}

	/*unlock GPAC frame buffer */
	gf_term_release_screen_buffer(osmo->term, &fb);
	/* gf_free temporary  objects */
	GlobalFree(ligne);
	LocalFree(infoSrc);
#endif   // XP_WIN

	return;
}

/*TODO - this is full print, present the print dialog and manage the print*/
}

void Osmozilla_Update(Osmozilla *osmo, const char *type, const char *commands)
{
	if (osmo->term) {
		GF_Err e = gf_term_scene_update(osmo->term, (char *) type, (char *) commands);
		if (e) {
			char szMsg[1024];
			sprintf((char *)szMsg, "GPAC: Error applying update (%s)", gf_error_to_string(e) );
			Osmozilla_SetStatus(osmo->np_instance, szMsg);
		}
	}
}

void Osmozilla_QualitySwitch(Osmozilla *osmo, int switch_up)
{
	if (osmo->term)
		gf_term_switch_quality(osmo->term, switch_up ? GF_TRUE : GF_FALSE);
}

void Osmozilla_SetURL(Osmozilla *osmo, const char *url)
{
	if (osmo->term) {
		if (osmo->url) gf_free(osmo->url);
		osmo->url = gf_strdup(url);
		gf_term_connect(osmo->term, osmo->url);
	}
}

int Osmozilla_GetDownloadProgress(Osmozilla *osmo)
{
	if (osmo->term)
		return osmo->download_progress;
	return 0;
}
