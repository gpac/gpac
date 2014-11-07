/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Romain Bouqueau
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / command-line client
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

/*includes both terminal and od browser*/
#include <gpac/terminal.h>
#include <gpac/term_info.h>
#include <gpac/constants.h>
#include <gpac/options.h>
#include <gpac/modules/service.h>


#ifndef WIN32
#include <pwd.h>
#include <unistd.h>

#else
#include <windows.h> /*for GetModuleFileName*/
#include <direct.h>  /*for _mkdir*/
#include <shlobj.h>  /*for getting user-dir*/
#ifndef SHGFP_TYPE_CURRENT
#define SHGFP_TYPE_CURRENT 0 /*needed for MinGW*/
#endif

#ifdef _MSC_VER
/*get rid of console*/
#if 0
#pragma comment(linker,"/SUBSYSTEM:WINDOWS")
#pragma comment(linker,"/ENTRY:main")
#else
#pragma comment(linker,"/SUBSYSTEM:CONSOLE")
#endif

#endif // _MSC_VER

#endif	//WIN32


static Bool restart = 0;
#if defined(__DARWIN__) || defined(__APPLE__)
static Bool not_threaded = 1;
#else
static Bool not_threaded = 0;
#endif
static Bool no_audio = 0;
static Bool no_regulation = 0;
static Bool bench_mode = 0;
Bool is_connected = 0;
Bool startup_file = 0;
GF_User user;
GF_Terminal *term;
u64 Duration;
GF_Err last_error = GF_OK;

static Fixed bench_speed = FLT2FIX(20);

static Bool request_next_playlist_item = 0;

static GF_Config *cfg_file;
static Bool display_rti = 0;
static Bool Run;
static Bool CanSeek = 0;
static char the_url[GF_MAX_PATH];
static char pl_path[GF_MAX_PATH];
static Bool no_mime_check = 1;
static Bool be_quiet = 0;
static u32 log_time_start = 0;

static u32 forced_width=0;
static u32 forced_height=0;

/*windowless options*/
u32 align_mode = 0;
u32 init_w = 0;
u32 init_h = 0;
u32 last_x, last_y;
Bool right_down = 0;

void dump_frame(GF_Terminal *term, char *rad_path, u32 dump_type, u32 frameNum);
Bool dump_file(char *the_url, u32 dump_mode, Double fps, u32 width, u32 height, Float scale, u32 *times, u32 nb_times);

void PrintUsage()
{
	fprintf(stdout, "Usage Osmo4iOS [options] [filename]\n"
	        "\t-c fileName:    user-defined configuration file\n"
	        "\t-rti fileName:  logs run-time info (FPS, CPU, Mem usage) to file\n"
	        "\t-rtix fileName: same as -rti but driven by GPAC logs\n"
	        "\t-quiet:         removes script message, buffering and downloading status\n"
	        "\t-opt	option:    Overrides an option in the configuration file. String format is section:key=value\n"
	        "\t-log-file file: sets output log file.\n"
	        "\t-log-level lev: sets log level. Possible values are:\n"
	        "\t        \"error\"      : logs only error messages\n"
	        "\t        \"warning\"    : logs error+warning messages\n"
	        "\t        \"info\"       : logs error+warning+info messages\n"
	        "\t        \"debug\"      : logs all messages\n"
	        "\n"
	        "\t-log-tools lt:  sets tool(s) to log. List of \':\'-separated values:\n"
	        "\t        \"core\"       : libgpac core\n"
	        "\t        \"coding\"     : bitstream formats (audio, video, scene)\n"
	        "\t        \"container\"  : container formats (ISO File, MPEG-2 TS, AVI, ...)\n"
	        "\t        \"network\"    : network data exept RTP trafic\n"
	        "\t        \"rtp\"        : rtp trafic\n"
	        "\t        \"author\"     : authoring tools (hint, import, export)\n"
	        "\t        \"sync\"       : terminal sync layer\n"
	        "\t        \"codec\"      : terminal codec messages\n"
	        "\t        \"parser\"     : scene parsers (svg, xmt, bt) and other\n"
	        "\t        \"media\"      : terminal media object management\n"
	        "\t        \"scene\"      : scene graph and scene manager\n"
	        "\t        \"script\"     : scripting engine messages\n"
	        "\t        \"interact\"   : interaction engine (events, scripts, etc)\n"
	        "\t        \"compose\"    : composition engine (2D, 3D, etc)\n"
	        "\t        \"service\"    : network service management\n"
	        "\t        \"mmio\"       : Audio/Video HW I/O management\n"
	        "\t        \"none\"       : no tool logged\n"
	        "\t        \"all\"        : all tools logged\n"
	        "\n"
	        "\t-size WxH:      specifies visual size (default: scene size)\n"
	        "\t-scale s:      scales the visual size (default: 1)\n"
#if defined(__DARWIN__) || defined(__APPLE__)
	        "\t-thread:        enables thread usage for terminal and compositor \n"
#else
	        "\t-no-thread:     disables thread usage (except for audio)\n"
#endif
	        "\t-no-audio:	   disables audio \n"
	        "\t-no-wnd:        uses windowless mode (Win32 only)\n"
	        "\t-align vh:      specifies v and h alignment for windowless mode\n"
	        "                   possible v values: t(op), m(iddle), b(ottom)\n"
	        "                   possible h values: l(eft), m(iddle), r(ight)\n"
	        "                   default alignment is top-left\n"
	        "                   default alignment is top-left\n"
	        "\t-pause:         pauses at first frame\n"
	        "\n"
	        "Dumper Options:\n"
	        "\t-bmp [times]:   dumps given frames to bmp\n"
	        "\t-raw [times]:   dumps given frames to bmp\n"
	        "\t-avi [times]:   dumps given file to raw avi\n"
	        "\t-rgbds:         dumps the RGBDS pixel format texture\n"
	        "                   with -avi [times]: dumps an rgbds-format .avi\n"
	        "\t-rgbd:          dumps the RGBD pixel format texture\n"
	        "					with -avi [times]: dumps an rgbd-format .avi\n"
	        "\t-depth:         dumps depthmap (z-buffer) frames\n"
	        "                   with -avi [times]: dumps depthmap in grayscale .avi\n"
	        "                   with -bmp: dumps depthmap in grayscale .bmp\n"
	        "\t-fps FPS:       specifies frame rate for AVI dumping (default: 25.0)\n"
	        "\t-2d:            uses 2D compositor\n"
	        "\t-3d:            uses 3D compositor\n"
	        "\t-fill:          uses fill aspect ratio for dumping (default: none)\n"
	        "\t-show:          show window while dumping (default: no)\n"
	        "MP4Client - GPAC command line player and dumper - version %s\n"
	        "GPAC Written by Jean Le Feuvre (c) 2001-2005 - ENST (c) 2005-200X\n",

	        GPAC_FULL_VERSION
	       );
}


static void PrintTime(u64 time)
{
	u32 ms, h, m, s;
	h = (u32) (time / 1000 / 3600);
	m = (u32) (time / 1000 / 60 - h*60);
	s = (u32) (time / 1000 - h*3600 - m*60);
	ms = (u32) (time - (h*3600 + m*60 + s) * 1000);
	fprintf(stdout, "%02d:%02d:%02d.%02d", h, m, s, ms);
}


static u32 rti_update_time_ms = 200;
static FILE *rti_logs = NULL;
static u64 memory_at_gpac_startup = 0;

static void UpdateRTInfo(const char *legend)
{
	GF_SystemRTInfo rti;

	/*refresh every second*/
	if (!display_rti && !rti_logs) return;
	if (!gf_sys_get_rti(rti_update_time_ms, &rti, 0) && !legend)
		return;

	if (display_rti) {
		if (!rti.process_memory) rti.process_memory = (u32) (memory_at_gpac_startup-rti.physical_memory_avail);
		if (!rti.gpac_memory) rti.gpac_memory = (u32) (memory_at_gpac_startup-rti.physical_memory_avail);

		if (display_rti==2) {
			fprintf(stdout, "FPS %02.2f - CPU %02d (%02d) - Mem %d kB\r",
			        gf_term_get_framerate(term, 0), rti.total_cpu_usage, rti.process_cpu_usage, (u32) (rti.gpac_memory / 1024) );
		} else {
			char szMsg[1024];
			GF_Event evt;

			sprintf(szMsg, "FPS %02.2f - CPU %02d (%02d) - Mem %d kB",
			        gf_term_get_framerate(term, 0), rti.total_cpu_usage, rti.process_cpu_usage, (u32) (rti.gpac_memory / 1024) );
			evt.type = GF_EVENT_SET_CAPTION;
			evt.caption.caption = szMsg;
			gf_term_user_event(term, &evt);
		}
	}
	if (rti_logs) {
		fprintf(rti_logs, "% 8d\t% 8d\t% 8d\t% 4d\t% 8d\t%s",
		        gf_sys_clock(),
		        gf_term_get_time_in_ms(term),
		        rti.total_cpu_usage,
		        (u32) gf_term_get_framerate(term, 0),
		        (u32) (rti.gpac_memory / 1024),
		        legend ? legend : ""
		       );
		if (!legend) fprintf(rti_logs, "\n");
	}
}

static void ResetCaption()
{
	GF_Event event;
	if (display_rti) return;
	event.type = GF_EVENT_SET_CAPTION;
	if (is_connected) {
		char szName[1024];
		NetInfoCommand com;

		event.caption.caption = NULL;
		/*get any service info*/
		if (!startup_file && gf_term_get_service_info(term, gf_term_get_root_object(term), &com) == GF_OK) {
			strcpy(szName, "");
			if (com.track_info) {
				char szBuf[10];
				sprintf(szBuf, "%02d ", (u32) (com.track_info>>16) );
				strcat(szName, szBuf);
			}
			if (com.artist) {
				strcat(szName, com.artist);
				strcat(szName, " ");
			}
			if (com.name) {
				strcat(szName, com.name);
				strcat(szName, " ");
			}
			if (com.album) {
				strcat(szName, "(");
				strcat(szName, com.album);
				strcat(szName, ")");
			}

			if (strlen(szName)) event.caption.caption = szName;
		}
		if (!event.caption.caption) {
			char *str = strrchr(the_url, '\\');
			if (!str) str = strrchr(the_url, '/');
			event.caption.caption = str ? str+1 : the_url;
		}
	} else {
		event.caption.caption = "GPAC MP4Client " GPAC_FULL_VERSION;
	}
	gf_term_user_event(term, &event);
}

#ifdef WIN32
u32 get_sys_col(int idx)
{
	u32 res;
	DWORD val = GetSysColor(idx);
	res = (val)&0xFF;
	res<<=8;
	res |= (val>>8)&0xFF;
	res<<=8;
	res |= (val>>16)&0xFF;
	return res;
}
#endif

void switch_bench()
{
	if (is_connected) {
		bench_mode = !bench_mode;
		display_rti = !display_rti;
		ResetCaption();
		gf_term_set_speed(term, bench_mode ? bench_speed : FIX_ONE);
	}
}

Bool GPAC_EventProc(void *ptr, GF_Event *evt)
{
	if (!term) return 0;

	switch (evt->type) {
	case GF_EVENT_DURATION:
		Duration = 1000;
		Duration = (u64) (((s64) Duration) * evt->duration.duration);
		CanSeek = evt->duration.can_seek;
		break;
	case GF_EVENT_MESSAGE:
	{
		const char *servName;
		if (!evt->message.service || !strcmp(evt->message.service, the_url)) {
			servName = "main service";
		} else if (!strnicmp(evt->message.service, "data:", 5)) {
			servName = "";
		} else {
			servName = evt->message.service;
		}
		if (!evt->message.message) return 0;
		if (evt->message.error) {
			if (!is_connected) last_error = evt->message.error;
			fprintf(stderr, "%s (%s): %s\n", evt->message.message, servName, gf_error_to_string(evt->message.error));
		} else if (!be_quiet)
			fprintf(stderr, "(%s) %s\r", servName, evt->message.message);
	}
	break;
	case GF_EVENT_PROGRESS:
	{
		char *szTitle = "";
		if (evt->progress.progress_type==0) szTitle = "Buffer ";
		else if (evt->progress.progress_type==1) szTitle = "Download ";
		else if (evt->progress.progress_type==2) szTitle = "Import ";
		gf_set_progress(szTitle, evt->progress.done, evt->progress.total);
	}
	break;


	case GF_EVENT_DBLCLICK:
		gf_term_set_option(term, GF_OPT_FULLSCREEN, !gf_term_get_option(term, GF_OPT_FULLSCREEN));
		return 0;

	case GF_EVENT_MOUSEDOWN:
		if (evt->mouse.button==GF_MOUSE_RIGHT) {
			right_down = 1;
			last_x = evt->mouse.x;
			last_y = evt->mouse.y;
		}
		return 0;
	case GF_EVENT_MOUSEUP:
		if (evt->mouse.button==GF_MOUSE_RIGHT) {
			right_down = 0;
			last_x = evt->mouse.x;
			last_y = evt->mouse.y;
		}
		return 0;
	case GF_EVENT_MOUSEMOVE:
		if (right_down && (user.init_flags & GF_TERM_WINDOWLESS) ) {
			GF_Event move;
			move.move.x = evt->mouse.x - last_x;
			move.move.y = last_y-evt->mouse.y;
			move.type = GF_EVENT_MOVE;
			move.move.relative = 1;
			gf_term_user_event(term, &move);
		}
		return 0;

	case GF_EVENT_KEYUP:
		switch (evt->key.key_code) {
		case GF_KEY_SPACE:
			if (evt->key.flags & GF_KEY_MOD_CTRL) switch_bench();
			break;
		}
		break;
	case GF_EVENT_KEYDOWN:
		gf_term_process_shortcut(term, evt);
		switch (evt->key.key_code) {
		case GF_KEY_SPACE:
			if (evt->key.flags & GF_KEY_MOD_CTRL) {
				/*ignore key repeat*/
				if (!bench_mode) switch_bench();
			}
			break;
		case GF_KEY_PAGEDOWN:
		case GF_KEY_MEDIANEXTTRACK:
			request_next_playlist_item = 1;
			break;
		case GF_KEY_MEDIAPREVIOUSTRACK:
			break;
		case GF_KEY_ESCAPE:
			gf_term_set_option(term, GF_OPT_FULLSCREEN, !gf_term_get_option(term, GF_OPT_FULLSCREEN));
			break;
		case GF_KEY_F:
			if (evt->key.flags & GF_KEY_MOD_CTRL) fprintf(stderr, "Rendering rate: %f FPS\n", gf_term_get_framerate(term, 0));
			break;
		case GF_KEY_T:
			if (evt->key.flags & GF_KEY_MOD_CTRL) fprintf(stderr, "Scene Time: %f \n", gf_term_get_time_in_ms(term)/1000.0);
			break;
		case GF_KEY_D:
			if (evt->key.flags & GF_KEY_MOD_CTRL) gf_term_set_option(term, GF_OPT_DRAW_MODE, (gf_term_get_option(term, GF_OPT_DRAW_MODE)==GF_DRAW_MODE_DEFER) ? GF_DRAW_MODE_IMMEDIATE : GF_DRAW_MODE_DEFER );
			break;
		case GF_KEY_4:
			if (evt->key.flags & GF_KEY_MOD_CTRL)
				gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_4_3);
			break;
		case GF_KEY_5:
			if (evt->key.flags & GF_KEY_MOD_CTRL)
				gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_16_9);
			break;
		case GF_KEY_6:
			if (evt->key.flags & GF_KEY_MOD_CTRL)
				gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_FILL_SCREEN);
			break;
		case GF_KEY_7:
			if (evt->key.flags & GF_KEY_MOD_CTRL)
				gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_KEEP);
			break;
		case GF_KEY_P:
			if (evt->key.flags & GF_KEY_MOD_CTRL && is_connected) {
				Bool is_pause = gf_term_get_option(term, GF_OPT_PLAY_STATE);
				fprintf(stderr, "[Status: %s]\n", is_pause ? "Playing" : "Paused");
				gf_term_set_option(term, GF_OPT_PLAY_STATE, (gf_term_get_option(term, GF_OPT_PLAY_STATE)==GF_STATE_PAUSED) ? GF_STATE_PLAYING : GF_STATE_PAUSED);
			}
			break;
		case GF_KEY_S:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected) {
				gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
				fprintf(stderr, "Step time: ");
				PrintTime(gf_term_get_time_in_ms(term));
				fprintf(stderr, "\n");
			}
			break;
		case GF_KEY_H:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected)
				gf_term_switch_quality(term, 1);
			break;
		case GF_KEY_L:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected)
				gf_term_switch_quality(term, 0);
			break;
		}
		break;

	case GF_EVENT_CONNECT:
		if (evt->connect.is_connected) {
			is_connected = 1;
			fprintf(stderr, "Service Connected\n");
		} else if (is_connected) {
			fprintf(stderr, "Service %s\n", is_connected ? "Disconnected" : "Connection Failed");
			is_connected = 0;
			Duration = 0;
		}
		if (init_w && init_h) {
			gf_term_set_size(term, init_w, init_h);
		}
		ResetCaption();
		break;
	case GF_EVENT_EOS:
		restart = 1;
		break;
	case GF_EVENT_SIZE:
		if (user.init_flags & GF_TERM_WINDOWLESS) {
			GF_Event move;
			move.type = GF_EVENT_MOVE;
			move.move.align_x = align_mode & 0xFF;
			move.move.align_y = (align_mode>>8) & 0xFF;
			move.move.relative = 2;
			gf_term_user_event(term, &move);
		}
		break;
	case GF_EVENT_SCENE_SIZE:
		if (forced_width && forced_height) {
			GF_Event size;
			size.type = GF_EVENT_SIZE;
			size.size.width = forced_width;
			size.size.height = forced_height;
			gf_term_user_event(term, &size);
		}
		break;

	case GF_EVENT_METADATA:
		ResetCaption();
		break;

	case GF_EVENT_QUIT:
		Run = 0;
		break;
	case GF_EVENT_DISCONNECT:
		gf_term_disconnect(term);
		break;
	case GF_EVENT_MIGRATE:
	{
	}
	break;
	case GF_EVENT_NAVIGATE_INFO:
		if (evt->navigate.to_url) fprintf(stderr, "Go to URL: \"%s\"\r", evt->navigate.to_url);
		break;
	case GF_EVENT_NAVIGATE:
		if (gf_term_is_supported_url(term, evt->navigate.to_url, 1, no_mime_check)) {
			strcpy(the_url, evt->navigate.to_url);
			fprintf(stderr, "Navigating to URL %s\n", the_url);
			gf_term_navigate_to(term, evt->navigate.to_url);
			return 1;
		} else {
			fprintf(stderr, "Navigation destination not supported\nGo to URL: %s\n", evt->navigate.to_url);
		}
		break;
	case GF_EVENT_SET_CAPTION:
		gf_term_user_event(term, evt);
		break;
	case GF_EVENT_AUTHORIZATION:
		if (!strlen(evt->auth.user)) {
			fprintf(stderr, "Authorization required for site %s\n", evt->auth.site_url);
			fprintf(stderr, "login: ");
			scanf("%s", evt->auth.user);
		} else {
			fprintf(stderr, "Authorization required for %s@%s\n", evt->auth.user, evt->auth.site_url);
		}
		fprintf(stderr, "password: ");
		gf_prompt_set_echo_off(1);
		scanf("%s", evt->auth.password);
		gf_prompt_set_echo_off(0);
		return 1;
	case GF_EVENT_SYS_COLORS:
#ifdef WIN32
		evt->sys_cols.sys_colors[0] = get_sys_col(COLOR_ACTIVEBORDER);
		evt->sys_cols.sys_colors[1] = get_sys_col(COLOR_ACTIVECAPTION);
		evt->sys_cols.sys_colors[2] = get_sys_col(COLOR_APPWORKSPACE);
		evt->sys_cols.sys_colors[3] = get_sys_col(COLOR_BACKGROUND);
		evt->sys_cols.sys_colors[4] = get_sys_col(COLOR_BTNFACE);
		evt->sys_cols.sys_colors[5] = get_sys_col(COLOR_BTNHIGHLIGHT);
		evt->sys_cols.sys_colors[6] = get_sys_col(COLOR_BTNSHADOW);
		evt->sys_cols.sys_colors[7] = get_sys_col(COLOR_BTNTEXT);
		evt->sys_cols.sys_colors[8] = get_sys_col(COLOR_CAPTIONTEXT);
		evt->sys_cols.sys_colors[9] = get_sys_col(COLOR_GRAYTEXT);
		evt->sys_cols.sys_colors[10] = get_sys_col(COLOR_HIGHLIGHT);
		evt->sys_cols.sys_colors[11] = get_sys_col(COLOR_HIGHLIGHTTEXT);
		evt->sys_cols.sys_colors[12] = get_sys_col(COLOR_INACTIVEBORDER);
		evt->sys_cols.sys_colors[13] = get_sys_col(COLOR_INACTIVECAPTION);
		evt->sys_cols.sys_colors[14] = get_sys_col(COLOR_INACTIVECAPTIONTEXT);
		evt->sys_cols.sys_colors[15] = get_sys_col(COLOR_INFOBK);
		evt->sys_cols.sys_colors[16] = get_sys_col(COLOR_INFOTEXT);
		evt->sys_cols.sys_colors[17] = get_sys_col(COLOR_MENU);
		evt->sys_cols.sys_colors[18] = get_sys_col(COLOR_MENUTEXT);
		evt->sys_cols.sys_colors[19] = get_sys_col(COLOR_SCROLLBAR);
		evt->sys_cols.sys_colors[20] = get_sys_col(COLOR_3DDKSHADOW);
		evt->sys_cols.sys_colors[21] = get_sys_col(COLOR_3DFACE);
		evt->sys_cols.sys_colors[22] = get_sys_col(COLOR_3DHIGHLIGHT);
		evt->sys_cols.sys_colors[23] = get_sys_col(COLOR_3DLIGHT);
		evt->sys_cols.sys_colors[24] = get_sys_col(COLOR_3DSHADOW);
		evt->sys_cols.sys_colors[25] = get_sys_col(COLOR_WINDOW);
		evt->sys_cols.sys_colors[26] = get_sys_col(COLOR_WINDOWFRAME);
		evt->sys_cols.sys_colors[27] = get_sys_col(COLOR_WINDOWTEXT);
		return 1;
#else
		memset(evt->sys_cols.sys_colors, 0, sizeof(u32)*28);
		return 1;
#endif
		break;
	}
	return 0;
}


void list_modules(GF_ModuleManager *modules)
{
	u32 i;
	fprintf(stderr, "\rAvailable modules:\n");
	for (i=0; i<gf_modules_get_count(modules); i++) {
		char *str = (char *) gf_modules_get_file_name(modules, i);
		if (str) fprintf(stderr, "\t%s\n", str);
	}
	fprintf(stderr, "\n");
}


void set_navigation()
{
	GF_Err e;
	char navstr[20], nav;
	u32 type = gf_term_get_option(term, GF_OPT_NAVIGATION_TYPE);
	e = GF_OK;
	if (!type) {
		fprintf(stdout, "Content/compositor doesn't allow user-selectable navigation\n");
	} else if (type==1) {
		fprintf(stdout, "Select Navigation (\'N\'one, \'E\'xamine, \'S\'lide): ");
		scanf("%s", navstr);
		nav = navstr[0];
		if (nav=='N') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_NONE);
		else if (nav=='E') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_EXAMINE);
		else if (nav=='S') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_SLIDE);
		else fprintf(stdout, "Unknown selector \'%c\' - only \'N\',\'E\',\'S\' allowed\n", nav);
	} else if (type==2) {
		fprintf(stdout, "Select Navigation (\'N\'one, \'W\'alk, \'F\'ly, \'E\'xamine, \'P\'an, \'S\'lide, \'G\'ame, \'V\'R, \'O\'rbit): ");
		scanf("%s", navstr);
		nav = navstr[0];
		if (nav=='N') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_NONE);
		else if (nav=='W') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_WALK);
		else if (nav=='F') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_FLY);
		else if (nav=='E') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_EXAMINE);
		else if (nav=='P') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_PAN);
		else if (nav=='S') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_SLIDE);
		else if (nav=='G') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_GAME);
		else if (nav=='O') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_ORBIT);
		else if (nav=='V') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_VR);
		else fprintf(stdout, "Unknown selector %c - only \'N\',\'W\',\'F\',\'E\',\'P\',\'S\',\'G\', \'V\', \'O\' allowed\n", nav);
	}
	if (e) fprintf(stdout, "Error setting mode: %s\n", gf_error_to_string(e));
}


static Bool get_time_list(char *arg, u32 *times, u32 *nb_times)
{
	char *str;
	Float var;
	Double sec;
	u32 h, m, s, ms, f, fps;
	if (!arg || (arg[0]=='-') || !isdigit(arg[0])) return 0;

	/*SMPTE time code*/
	if (strchr(arg, ':') && strchr(arg, ';') && strchr(arg, '/')) {
		if (sscanf(arg, "%02d:%02d:%02d;%02d/%02d", &h, &m, &s, &f, &fps)==5) {
			sec = 0;
			if (fps) sec = ((Double)f) / fps;
			sec += 3600*h + 60*m + s;
			times[*nb_times] = (u32) (1000*sec);
			(*nb_times) ++;
			return 1;
		}
	}
	while (arg) {
		str = strchr(arg, '-');
		if (str) str[0] = 0;
		/*HH:MM:SS:MS time code*/
		if (strchr(arg, ':') && (sscanf(arg, "%02d:%02d:%02d:%02d", &h, &m, &s, &ms)==4)) {
			sec = ms;
			sec /= 1000;
			sec += 3600*h + 60*m + s;
			times[*nb_times] = (u32) (1000*sec);
			(*nb_times) ++;
		} else if (sscanf(arg, "%f", &var)==1) {
			sec = atof(arg);
			times[*nb_times] = (u32) (1000*sec);
			(*nb_times) ++;
		}
		if (!str) break;
		str[0] = '-';
		arg = str+1;
	}
	return 1;
}

static void on_gpac_log(void *cbk, u32 ll, u32 lm, const char *fmt, va_list list)
{
	FILE *logs = cbk;

	if (rti_logs && (lm & GF_LOG_RTI)) {
		char szMsg[2048];
		vsprintf(szMsg, fmt, list);
		UpdateRTInfo(szMsg + 6 /*"[RTI] "*/);
	} else {
		if (log_time_start) fprintf(logs, "[At %d]", gf_sys_clock() - log_time_start);
		vfprintf(logs, fmt, list);
		fflush(logs);
	}
}

static void init_rti_logs(char *rti_file, char *url, Bool use_rtix)
{
	if (rti_logs) fclose(rti_logs);
	rti_logs = gf_f64_open(rti_file, "wt");
	if (rti_logs) {
		fprintf(rti_logs, "!! GPAC RunTime Info ");
		if (url) fprintf(rti_logs, "for file %s", url);
		fprintf(rti_logs, " !!\n");
		fprintf(rti_logs, "SysTime(ms)\tSceneTime(ms)\tCPU\tFPS\tMemory(kB)\tObservation\n");

		/*turn on RTI loging*/
		if (use_rtix) {
			gf_log_set_callback(NULL, on_gpac_log);
			gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_ERROR);
			gf_log_set_tool_level(GF_LOG_RTI, GF_LOG_DEBUG);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_RTI, ("[RTI] System state when enabling log\n"));
		} else if (log_time_start) {
			log_time_start = gf_sys_clock();
		}
	}
}

#ifdef GPAC_IPHONE
int SDL_main (int argc, char *argv[])
#else
int main (int argc, char *argv[])
#endif
{
	char c;
	const char *str;
	u32 i, times[100], nb_times, dump_mode;
	u32 simulation_time = 0;
	Bool auto_exit = 0;
	Bool start_fs = 1;
	Bool use_rtix = 0;
	Bool rgbds_dump = 0;
	Bool rgbd_dump = 0;
	Bool depth_dump = 0;
	Bool pause_at_first = 0;
	Bool enable_mem_tracker = 0;
	Double fps = 25.0;
	Bool ret, fill_ar, visible;
	char *url_arg, *the_cfg, *rti_file;
	GF_SystemRTInfo rti;
	FILE *playlist = NULL;
	FILE *logfile = NULL;
	Float scale = 1;
    
	/*by default use current dir*/
	strcpy(the_url, ".");

	memset(&user, 0, sizeof(GF_User));

	dump_mode = 0;
	fill_ar = visible = 0;
	url_arg = the_cfg = rti_file = NULL;
	nb_times = 0;
	times[0] = 0;

	/*first locate config file if specified*/
	for (i=1; i<(u32) argc; i++) {
		char *arg = argv[i];
		if (!strcmp(arg, "-c") || !strcmp(arg, "-cfg")) {
			the_cfg = argv[i+1];
			i++;
		}
		else if (!strcmp(arg, "-mem-track")) enable_mem_tracker = 1;
        
        fprintf(stdout, "arg is %s\n", arg);
	}

	gf_sys_init(enable_mem_tracker);

	cfg_file = gf_cfg_init(the_cfg, NULL);
	if (!cfg_file) {
		fprintf(stdout, "Error: Configuration File \"GPAC.cfg\" not found\n");
		if (logfile) fclose(logfile);
		return 1;
	}

	gf_log_set_tools_levels( gf_cfg_get_key(cfg_file, "General", "Logs") );

	for (i=1; i<(u32) argc; i++) {
		char *arg = argv[i];
//		if (isalnum(arg[0]) || (arg[0]=='/') || (arg[0]=='.') || (arg[0]=='\\') ) {
		if (arg[0] != '-') {
			url_arg = arg;
		} else if (!strcmp(arg, "-c") || !strcmp(arg, "-cfg")) {
			the_cfg = argv[i+1];
			i++;
		} else if (!strcmp(arg, "-rti")) {
			rti_file = argv[i+1];
			i++;
		} else if (!strcmp(arg, "-rtix")) {
			rti_file = argv[i+1];
			i++;
			use_rtix = 1;
		} else if (!strcmp(arg, "-fill")) {
			fill_ar = 1;
		} else if (!strcmp(arg, "-show")) {
			visible = 1;
		} else if (!strcmp(arg, "-avi")) {
			if (rgbds_dump) dump_mode = 5;
			else if (depth_dump) dump_mode = 8;
			else if (rgbd_dump) dump_mode = 10;
			else dump_mode=1;
			if ((url_arg || (i+2<(u32)argc)) && get_time_list(argv[i+1], times, &nb_times)) i++;
		} else if (!strcmp(arg, "-rgbds")) { /*get dump in rgbds pixel format*/
			rgbds_dump = 1;
			dump_mode=6;                    /* rgbds texture directly*/
			if (dump_mode==1) dump_mode = 5;    /* .avi rgbds dump*/
		} else if (!strcmp(arg, "-rgbd")) { /*get dump in rgbd pixel format*/
			rgbd_dump = 1;
			dump_mode=9;  /* rgbd texture directly*/
			if (dump_mode==1) dump_mode = 10;    /* .avi rgbds dump*/
		} else if (!strcmp(arg, "-depth")) {
			depth_dump = 1;
			if (dump_mode==2) dump_mode=7; /* grayscale .bmp depth dump*/
			else if (dump_mode==1) dump_mode=8; /* .avi depth dump*/
			else dump_mode=4;   /*depth dump*/
		} else if (!strcmp(arg, "-bmp")) {
			if(depth_dump) dump_mode=7; /*grayscale depth .bmp dump*/
			else dump_mode=2;
			if ((url_arg || (i+2<(u32)argc)) && get_time_list(argv[i+1], times, &nb_times)) i++;
		} else if (!strcmp(arg, "-raw")) {
			dump_mode = 3;
			if ((url_arg || (i+2<(u32)argc)) && get_time_list(argv[i+1], times, &nb_times)) i++;

		} else if (!stricmp(arg, "-size")) {
			if (sscanf(argv[i+1], "%dx%d", &forced_width, &forced_height) != 2) {
				forced_width = forced_height = 0;
			}
			i++;
		} else if (!stricmp(arg, "-scale")) {
			sscanf(argv[i+1], "%f", &scale);
			i++;
		} else if (!stricmp(arg, "-fps")) {
			fps = atof(argv[i+1]);
			i++;
		} else if (!strcmp(arg, "-quiet")) {
			be_quiet = 1;
		} else if (!strcmp(arg, "-log-file") || !strcmp(arg, "-lf")) {
			logfile = gf_f64_open(argv[i+1], "wt");
			gf_log_set_callback(logfile, on_gpac_log);
			i++;
		} else if (!strcmp(arg, "-logs")) {
			gf_log_set_tools_levels( argv[i+1] );
			i++;
		} else if (!strcmp(arg, "-log-clock") || !strcmp(arg, "-lc")) {
			log_time_start = 1;
		} else if (!strcmp(arg, "-align")) {
			if (argv[i+1][0]=='m') align_mode = 1;
			else if (argv[i+1][0]=='b') align_mode = 2;
			align_mode <<= 8;
			if (argv[i+1][1]=='m') align_mode |= 1;
			else if (argv[i+1][1]=='r') align_mode |= 2;
			i++;
		}
		else if (!strcmp(arg, "-no-wnd")) user.init_flags |= GF_TERM_WINDOWLESS;
#if defined(__DARWIN__) || defined(__APPLE__)
		else if (!strcmp(arg, "-thread")) not_threaded = 0;
#else
		else if (!strcmp(arg, "-no-thread")) not_threaded = 1;
#endif
		else if (!strcmp(arg, "-no-audio")) no_audio = 1;
		else if (!strcmp(arg, "-no-regulation")) no_regulation = 1;
		else if (!strcmp(arg, "-fs")) start_fs = 1;
		else if (!strcmp(arg, "-pause")) pause_at_first = 1;
		else if (!strcmp(arg, "-exit")) auto_exit = 1;
		else if (!strcmp(arg, "-mem-track")) enable_mem_tracker = 1;
		else if (!strcmp(arg, "-opt")) {
			char *sep, *sep2, szSec[1024], szKey[1024], szVal[1024];
			sep = strchr(argv[i+1], ':');
			if (sep) {
				sep[0] = 0;
				strcpy(szSec, argv[i+1]);
				sep[0] = ':';
				sep ++;
				sep2 = strchr(sep, '=');
				if (sep2) {
					sep2[0] = 0;
					strcpy(szKey, sep);
					strcpy(szVal, sep2+1);
					sep2[0] = '=';
					if (!stricmp(szVal, "null")) szVal[0]=0;
					gf_cfg_set_key(cfg_file, szSec, szKey, szVal[0] ? szVal : NULL);
				}
			}
			i++;
		}
		else if (!strncmp(arg, "-run-for=", 9)) simulation_time = atoi(arg+9);
		else {
			PrintUsage();
			return 1;
		}
	}
	if (!logfile) {
		const char *opt = gf_cfg_get_key(cfg_file, "General", "LogFile");
		if (opt) {
			logfile = gf_f64_open(opt, "wt");
			if (logfile)
				gf_log_set_callback(logfile, on_gpac_log);
		}
	}

	if (dump_mode && !url_arg) {
		fprintf(stdout, "Missing argument for dump\n");
		PrintUsage();
		if (logfile) fclose(logfile);
		return 1;
	}
	if (dump_mode) rti_file = NULL;

	gf_sys_get_rti(500, &rti, GF_RTI_SYSTEM_MEMORY_ONLY);
	memory_at_gpac_startup = rti.physical_memory_avail;
	if (rti_file) init_rti_logs(rti_file, url_arg, use_rtix);

	/*setup dumping options*/
	if (dump_mode) {
		user.init_flags |= GF_TERM_NO_AUDIO | GF_TERM_NO_COMPOSITOR_THREAD | GF_TERM_NO_REGULATION /*| GF_TERM_INIT_HIDE*/;
		if (visible || dump_mode==8) user.init_flags |= GF_TERM_INIT_HIDE;
	} else {
		init_w = forced_width;
		init_h = forced_height;
	}
    

	fprintf(stderr, "Loading modules\n");
	str = gf_cfg_get_key(cfg_file, "General", "ModulesDirectory");

	user.modules = gf_modules_new((const char *) str, cfg_file);
	if (user.modules) i = gf_modules_get_count(user.modules);
	if (!i || !user.modules) {
		fprintf(stdout, "Error: no modules found in %s - exiting\n", str);
		if (user.modules) gf_modules_del(user.modules);
		gf_cfg_del(cfg_file);
		gf_sys_close();
		if (logfile) fclose(logfile);
		return 1;
	}
	fprintf(stderr, "Modules Loaded (%d found in %s)\n", i, str);

	user.config = cfg_file;
	user.EventProc = GPAC_EventProc;
	/*dummy in this case (global vars) but MUST be non-NULL*/
	user.opaque = user.modules;
	if (not_threaded) user.init_flags |= GF_TERM_NO_COMPOSITOR_THREAD;
	if (no_audio) user.init_flags |= GF_TERM_NO_AUDIO;
	if (no_regulation) user.init_flags |= GF_TERM_NO_REGULATION;

	fprintf(stderr, "Loading GPAC Terminal\n");
	term = gf_term_new(&user);
	if (!term) {
		fprintf(stderr, "\nInit error - check you have at least one video out and one rasterizer...\nFound modules:\n");
		list_modules(user.modules);
		gf_modules_del(user.modules);
		gf_cfg_del(cfg_file);
		gf_sys_close();
		if (logfile) fclose(logfile);
		return 1;
	}
	fprintf(stderr, "Terminal Loaded\n");


	if (dump_mode) {
//		gf_term_set_option(term, GF_OPT_VISIBLE, 0);
		if (fill_ar) gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_FILL_SCREEN);
	} else {
		/*check video output*/
		str = gf_cfg_get_key(cfg_file, "Video", "DriverName");
		if (!strcmp(str, "Raw Video Output")) fprintf(stdout, "WARNING: using raw output video (memory only) - no display used\n");
		/*check audio output*/
		str = gf_cfg_get_key(cfg_file, "Audio", "DriverName");
		if (!str || !strcmp(str, "No Audio Output Available")) fprintf(stdout, "WARNING: no audio output availble - make sure no other program is locking the sound card\n");

		str = gf_cfg_get_key(cfg_file, "General", "NoMIMETypeFetch");
		no_mime_check = (str && !stricmp(str, "yes")) ? 1 : 0;
	}

	str = gf_cfg_get_key(cfg_file, "HTTPProxy", "Enabled");
	if (str && !strcmp(str, "yes")) {
		str = gf_cfg_get_key(cfg_file, "HTTPProxy", "Name");
		if (str) fprintf(stdout, "HTTP Proxy %s enabled\n", str);
	}

	if (rti_file) {
		str = gf_cfg_get_key(cfg_file, "General", "RTIRefreshPeriod");
		if (str) {
			rti_update_time_ms = atoi(str);
		} else {
			gf_cfg_set_key(cfg_file, "General", "RTIRefreshPeriod", "200");
		}
		UpdateRTInfo("At GPAC load time\n");
	}

	Run = 1;
	ret = 1;

	if (dump_mode) {
		if (!nb_times) {
			times[0] = 0;
			nb_times++;
		}
		ret = dump_file(url_arg, dump_mode, fps, forced_width, forced_height, scale, times, nb_times);
		Run = 0;
	} else

		/*connect if requested*/
		if (url_arg) {
			char *ext;
			strcpy(the_url, url_arg);
			ext = strrchr(the_url, '.');
			if (ext && (!stricmp(ext, ".m3u") || !stricmp(ext, ".pls"))) {
				fprintf(stdout, "Opening Playlist %s\n", the_url);
				playlist = gf_f64_open(the_url, "rt");
				if (playlist) {
					strcpy(pl_path, the_url);
					fscanf(playlist, "%s", the_url);
					fprintf(stdout, "Opening URL %s\n", the_url);
					gf_term_connect_with_path(term, the_url, pl_path);
				} else {
					fprintf(stdout, "Hit 'h' for help\n\n");
				}
			} else {
				fprintf(stdout, "Opening URL %s\n", the_url);
				if (pause_at_first) fprintf(stdout, "[Status: Paused]\n");
				gf_term_connect_from_time(term, the_url, 0, pause_at_first);
			}
		} else {
			fprintf(stdout, "Hit 'h' for help\n\n");
			str = gf_cfg_get_key(cfg_file, "General", "StartupFile");
			if (str) {
				strcpy(the_url, "MP4Client "GPAC_FULL_VERSION);
				gf_term_connect(term, str);
				startup_file = 1;
			}
		}
	/*force fullscreen*/
	if (0 && start_fs)
		gf_term_set_option(term, GF_OPT_FULLSCREEN, 1);

	while (Run) {
		if (restart) {
			restart = 0;
			gf_term_play_from_time(term, 0, 0);
		}
		if (request_next_playlist_item) {
			c = '\n';
			request_next_playlist_item = 0;
			if (playlist) {
				gf_term_disconnect(term);

				if (fscanf(playlist, "%s", the_url) == EOF) {
					fprintf(stdout, "No more items - exiting\n");
					Run = 0;
				} else {
					fprintf(stdout, "Opening URL %s\n", the_url);
					gf_term_connect_with_path(term, the_url, pl_path);
				}
			}
		}
		if (!use_rtix || display_rti) UpdateRTInfo(NULL);
		if (not_threaded) {
			//printf("gf_term_process_step from run loop\n");
			gf_term_process_step(term);
			if (auto_exit && gf_term_get_option(term, GF_OPT_IS_OVER)) {
				Run = 0;
			}
		} else {
			gf_sleep(rti_update_time_ms);
		}
		/*sim time*/
		if (simulation_time && (gf_term_get_time_in_ms(term)>simulation_time)) {
			Run = 0;
		}
		continue;
	}


	gf_term_disconnect(term);
	if (rti_file) UpdateRTInfo("Disconnected\n");

	fprintf(stdout, "Deleting terminal... ");
	if (playlist) fclose(playlist);
	gf_term_del(term);
	fprintf(stdout, "OK\n");

	fprintf(stdout, "GPAC cleanup ...\n");
	gf_modules_del(user.modules);
	gf_cfg_del(cfg_file);

#ifdef GPAC_MEMORY_TRACKING
	if (enable_mem_tracker) {
		gf_memory_print();
		fprintf(stdout, "print any key\n");
		while (!gf_prompt_has_input()) {
			gf_sleep(100);
		}
	}
#endif

	gf_sys_close();
	if (rti_logs) fclose(rti_logs);
	if (logfile) fclose(logfile);
	fprintf(stdout, "Bye\n");
	return 0;
}
