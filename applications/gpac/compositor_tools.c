/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2022
 *					All rights reserved
 *
 *  This file is part of GPAC / gpac application
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

#include "gpac.h"

#ifndef GPAC_DISABLE_PLAYER

#include <gpac/main.h>
#include <gpac/events.h>
#include <gpac/filters.h>
#include <gpac/compositor.h>
//for window flags
#include <gpac/modules/video_out.h>
#include <gpac/avparse.h>

#ifndef WIN32
#include <dlfcn.h>
#include <pwd.h>
#include <unistd.h>

#else //WIN32

#include <windows.h> /*for GetModuleFileName*/

#endif	//WIN32

extern u32 compositor_mode;

#if !defined(GPAC_CONFIG_IOS)
#define DESKTOP_GUI
#endif


#ifdef DESKTOP_GUI
static void mp4c_take_screenshot(Bool for_coverage);
#endif


static Bool reload = GF_FALSE;
static u32 window_flags=0;
static Bool do_coverage = GF_FALSE;

extern FILE *helpout;
extern u32 gen_doc;
extern u32 help_flags;

#ifdef WIN32
#include <wincon.h>
#endif

#if defined(__DARWIN__) || defined(__APPLE__)
#define VK_MOD  GF_KEY_MOD_ALT
#else
#define VK_MOD  GF_KEY_MOD_CTRL
#endif

static u32 bench_mode = 0;
static u32 bench_mode_start = 0;
static u32 bench_buffer = 0;
static Bool eos_seen = GF_FALSE;
static Bool addon_visible = GF_TRUE;
static Bool is_connected = GF_FALSE;
static u64 duration_ms=0;
static GF_Err last_error = GF_OK;
static Bool enable_add_ons = GF_TRUE;
static Fixed playback_speed = FIX_ONE;

static u32 display_rti = 0;
static Bool can_seek = GF_FALSE;
static u64 log_rti_time_start = 0;

static Bool loop_at_end = GF_FALSE;

#ifdef DESKTOP_GUI
static u32 forced_width=0;
static u32 forced_height=0;


/*windowless options*/
static u32 align_mode = 0;
static u32 init_w = 0;
static u32 init_h = 0;
static u32 last_x, last_y;
static Bool right_down = GF_FALSE;

#endif


static GF_Filter *comp_filter = NULL;
static GF_Compositor *compositor = NULL;


GF_GPACArg mp4c_args[] =
{
#ifdef DESKTOP_GUI
	GF_DEF_ARG("size", NULL, "specify visual size WxH. If not set, scene size or video size is used", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("fs", NULL, "start in fullscreen mode", NULL, NULL, GF_ARG_BOOL, 0),
#endif

 	GF_DEF_ARG("bm", NULL, "benchmark compositor without video out", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("vbm", NULL, "benchmark compositor with video out", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("sbm", NULL, "benchmark compositor systems aspect (no decode, no video)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("loop", NULL, "loop playback", NULL, NULL, GF_ARG_BOOL, 0),

	GF_DEF_ARG("rti", NULL, "log run-time info (FPS, CPU, Mem usage) to given file", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("rtix", NULL, "same as -rti but driven by GPAC logs", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("rti-refresh", NULL, "set refresh time in ms between two runt-time counters queries (default is 200)", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("pause", NULL, "pause at first frame", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("start", NULL, "play from given time in seconds", NULL, NULL, GF_ARG_DOUBLE, 0),
	GF_DEF_ARG("speed", NULL, "set playback speed", NULL, NULL, GF_ARG_DOUBLE, 0),
	GF_DEF_ARG("exit", NULL, "exit when presentation is over", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("service", NULL, "auto-tune to given service ID in a multiplex", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED),

#ifdef GPAC_CONFIG_WIN32
 	GF_DEF_ARG("no-wnd", NULL, "use windowless mode (Win32 only)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERIMENTAL),
 	GF_DEF_ARG("no-back", NULL, "use transparent background for output window when no background is specified (Win32 only)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERIMENTAL),
 	GF_DEF_ARG("align", NULL, "specify v and h alignment for windowless mode\n"
 	"- tl: top/left\n"
 	"- tm: top/horizontal middle\n"
 	"- tr: top/right\n"
 	"- ml: vertical middle/left\n"
 	"- mm: vertical middle/horizontal middle\n"
 	"- mr: vertical middle/right\n"
 	"- bl: bottom/left\n"
 	"- bm: bottom/horizontal middle\n"
 	"- br: bottom/right", "tl", "tl|tm|tr|ml|mm|mr|bl|bm|br", GF_ARG_INT, GF_ARG_HINT_EXPERIMENTAL),
#endif // GPAC_CONFIG_WIN32

	GF_DEF_ARG("no-addon", NULL, "disable automatic loading of media addons declared in source URL", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("nk", NULL, "disable keyboard interaction", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
	{0}
};

typedef enum
{
	MP4C_RELOAD=0,
	MP4C_OPEN,
	MP4C_DISCONNECT,
	MP4C_PAUSE_RESUME,
	MP4C_STEP,
	MP4C_SEEK,
	MP4C_SEEK_TIME,
	MP4C_TIME,
	MP4C_UPDATE,
	MP4C_EVALJS,
#ifdef DESKTOP_GUI
	MP4C_SCREENSHOT,
#endif
	MP4C_DUMPSCENE,
	MP4C_STRESSMODE,
	MP4C_NAVMODE,
	MP4C_LASTVP,
	MP4C_OGL2D,
	MP4C_AR_4_3,
	MP4C_AR_16_9,
	MP4C_AR_NONE,
	MP4C_AR_ORIG,
	MP4C_LOGS,
	MP4C_RELOAD_OPTS,
	MP4C_DISP_RTI,
	MP4C_DISP_FPS,
	MP4C_HELP,
	MP4C_DOWNRATE,
	MP4C_VMEM_CACHE,

} MP4C_Command;

struct _mp4c_key
{
 u8 char_code;
 MP4C_Command cmd_type;
 const char *cmd_help;
 u32 flags;
} MP4C_Keys[] = {
	{'R', MP4C_RELOAD, "reload presentation", 0},
	{'o', MP4C_OPEN, "connect to the specified URL", 0},
	{'D', MP4C_DISCONNECT, "disconnect the current presentation", 0},
	{'p', MP4C_PAUSE_RESUME, "play/pause the presentation", 0},
	{'f', MP4C_STEP, "step one frame ahead", 0},
	{'z', MP4C_SEEK, "seek into presentation by percentage", 0},
	{'T', MP4C_SEEK_TIME, "seek into presentation by time", 0},
	{'t', MP4C_TIME, "print current timing", 0},
	{'a', MP4C_UPDATE, "send a command (BIFS or LASeR) to the main scene", 0},
	{'e', MP4C_EVALJS, "evaluate JavaScript code in the main scene", 0},
#ifdef DESKTOP_GUI
	{'Z', MP4C_SCREENSHOT, "dump current output frame to PNG", 0},
#endif
	{ 'd', MP4C_DUMPSCENE, "dump scene graph", 0},
	{ 'k', MP4C_STRESSMODE, "turn stress mode on/off", 0},
	{ 'n', MP4C_NAVMODE, "change navigation mode", 0},
	{ 'v', MP4C_LASTVP, "reset to last active viewpoint", 0},
	{ '3', MP4C_OGL2D, "switch OpenGL on or off for 2D scenes", 0},
	{ '4', MP4C_AR_4_3, "force 4/3 Aspect Ratio", 0},
	{ '5', MP4C_AR_16_9, "force 16/9 Aspect Ratio", 0},
	{ '6', MP4C_AR_NONE, "force no Aspect Ratio (always fill screen)", 0},
	{ '7', MP4C_AR_ORIG, "force original Aspect Ratio (default)", 0},
	{ 'H', MP4C_DOWNRATE, "set HTTP max download rate", 0},
	{ 'E', MP4C_RELOAD_OPTS, "force reload of compositor options", 0},

	{ 'L', MP4C_LOGS, "change log tool/level", 0},
	{ 'R', MP4C_DISP_RTI, "toggle run-time info display in window title bar on/off", 0},
	{ 'F', MP4C_DISP_FPS, "toggle displaying of FPS in stderr on/off", 0},

	{ 'h', MP4C_HELP, "print this message", 0},
	//below this, only experimental features
	{ 'M', MP4C_VMEM_CACHE, "specify video cache memory for 2D objects", 1},
	{0}
};

void mp4c_help(u32 argmode)
{
	u32 i=0;

	if (help_flags & (GF_PRINTARG_MAN|GF_PRINTARG_MD)) {
		return;
	}

	gf_sys_format_help(helpout, help_flags, "Usage: gpac -mp4c URL [options]\n"
		"# General\n"
		"The `-mp4c` option launches the GPAC compositor as a standalone player without GUI.\n"
		"Specific URLs shortcuts are available, see [GPAC Compositor (gpac -h compositor)](compositor)\n"
		"\n"
		"# Options  \n"
	);

	while (mp4c_args[i].name) {
		GF_GPACArg *arg = &mp4c_args[i];
		i++;
		if ((argmode<GF_ARGMODE_EXPERT) && (arg->flags & (GF_ARG_HINT_EXPERIMENTAL|GF_ARG_HINT_EXPERT) ))
			continue;
		gf_sys_print_arg(helpout, help_flags, arg, "gpac");
	}

	if (argmode<GF_ARGMODE_EXPERT) return;
	gf_sys_format_help(helpout, help_flags, "Usage: gpac -mp4c URL [options]\n"
		"# Run-time keyboard options  \n"
	);

	i=0;
	while (MP4C_Keys[i].char_code) {
		gf_sys_format_help(helpout, help_flags, "`%c`: %s\n", MP4C_Keys[i].char_code, MP4C_Keys[i].cmd_help);
		i++;
	}
	gf_sys_format_help(helpout, help_flags, "  \n");
}

static MP4C_Command get_gui_cmd(u8 char_code)
{
	u32 i=0;
	while (MP4C_Keys[i].char_code) {
		if (MP4C_Keys[i].char_code == char_code)
			return MP4C_Keys[i].cmd_type;
		i++;
	}
	return 0;
}

void gpac_open_urls(const char *urls)
{
	GF_Event evt;
	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_NAVIGATE;
	evt.navigate.to_url = urls;
	gf_sc_user_event(compositor, &evt);
}


static void mp4c_print_help()
{
	u32 i=0;

	gf_sys_format_help(helpout, help_flags, "# Runtime commands\n"
		"## Prompt Interaction\n"
		"The following keys are used for prompt interaction:\n");

	while (MP4C_Keys[i].char_code) {
		struct _mp4c_key *k = &MP4C_Keys[i];
		i++;
		gf_sys_format_help(helpout, help_flags|GF_PRINTARG_HIGHLIGHT_FIRST, "%c: %s%s\n", k->char_code, k->cmd_help, k->flags ? " **! experimental !**" : "");
	}

	gf_sys_format_help(helpout, help_flags, "\n"
		"## Content interaction\n"
		"It is possible to interact with content (interactive or not) using mouse and keyboard.\n"
		"The following commands are available:\n"
		"TODO\n"
		"\n"
	);
}

static void print_time(u64 time)
{
	u32 ms, h, m, s;
	h = (u32) (time / 1000 / 3600);
	m = (u32) (time / 1000 / 60 - h*60);
	s = (u32) (time / 1000 - h*3600 - m*60);
	ms = (u32) (time - (h*3600 + m*60 + s) * 1000);
	fprintf(stderr, "%02d:%02d:%02d.%03d", h, m, s, ms);
}

static u32 rti_update_time_ms = 200;
static FILE *rti_logs = NULL;

static void update_rti(const char *legend)
{
	GF_SystemRTInfo rti;

	/*refresh every second*/
	if (!display_rti && !rti_logs) return;
	if (!gf_sys_get_rti(rti_update_time_ms, &rti, 0) && !legend)
		return;

	if (display_rti) {
		char szMsg[1024];
		if (rti.total_cpu_usage && (bench_mode<2) ) {
			sprintf(szMsg, "FPS %02.02f CPU %2d (%02d) Mem %d kB",
			        gf_sc_get_fps(compositor, 0), rti.total_cpu_usage, rti.process_cpu_usage, (u32) (rti.gpac_memory / 1024));
		} else {
			sprintf(szMsg, "FPS %02.02f CPU %02d Mem %d kB",
			        gf_sc_get_fps(compositor, 0), rti.process_cpu_usage, (u32) (rti.gpac_memory / 1024) );
		}

		if (display_rti==2) {
			fprintf(stderr, "%s\r", szMsg);
		} else {
			GF_Event evt;
			evt.type = GF_EVENT_SET_CAPTION;
			evt.caption.caption = szMsg;
			gf_sc_user_event(compositor, &evt);
		}
	}
	if (rti_logs) {
		fprintf(rti_logs, "% 8d\t% 8d\t% 8d\t% 4d\t% 8d\t%s",
		        gf_sys_clock(),
		        gf_sc_get_time_in_ms(compositor),
		        rti.total_cpu_usage,
		        (u32) gf_sc_get_fps(compositor, 0),
		        (u32) (rti.gpac_memory / 1024),
		        legend ? legend : ""
		       );
		if (!legend) fprintf(rti_logs, "\n");
	}
}

#ifdef DESKTOP_GUI

#include <gpac/revision.h>

char *caption = NULL;

static void set_window_caption()
{
	GF_Event event;
	if (display_rti) return;
	event.type = GF_EVENT_SET_CAPTION;
	event.caption.caption = NULL;

	if (is_connected && (compositor_mode==LOAD_MP4C)) {
		const GF_PropertyValue *p;
		GF_PropertyEntry *pe=NULL;
		GF_FilterPid *pid = (GF_FilterPid *) gf_sc_get_main_pid(compositor);

		if (caption) gf_free(caption);
		caption = NULL;
		/*get any service info*/
		if (pid) {
			p = gf_filter_pid_get_info_str(pid, "info:track", &pe);
			if (p) {
				char szBuf[10];
				sprintf(szBuf, "%02d", p->value.frac.num);
				gf_dynstrcat(&caption, szBuf, " ");
			}
			p = gf_filter_pid_get_info_str(pid, "info:artist", &pe);
			if (p && p->value.string)
				gf_dynstrcat(&caption, p->value.string, " ");

			p = gf_filter_pid_get_info_str(pid, "info:name", &pe);
			if (p && p->value.string)
				gf_dynstrcat(&caption, p->value.string, " ");

			p = gf_filter_pid_get_info_str(pid, "info:album", &pe);
			if (p && p->value.string) {
				gf_dynstrcat(&caption, "(", " ");
				gf_dynstrcat(&caption, p->value.string, NULL);
				gf_dynstrcat(&caption, ")", NULL);
			}
			if (caption) event.caption.caption = caption;

			if (!event.caption.caption) {
				p = gf_filter_pid_get_property(pid, GF_PROP_PID_URL);
				if (!p) p = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
				if (p && p->value.string) caption = gf_strdup(gf_file_basename(p->value.string));
				event.caption.caption = caption;
			}
		}
	} else {
		event.caption.caption = "GPAC "GPAC_VERSION "-rev" GPAC_GIT_REVISION;
	}
	gf_sc_user_event(compositor, &event);
}
#else

#define set_window_caption(_ARG)

#endif

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

static void switch_bench(u32 is_on)
{
	bench_mode = is_on;
	display_rti = is_on ? 2 : 0;
	set_window_caption();
	gf_sc_set_option(compositor, GF_OPT_VIDEO_BENCH, is_on);
}

#ifdef GPAC_ENABLE_COVERAGE
#define gf_getch() 0
#define gf_read_line_input(_line, _maxSize, _showContent) GF_FALSE
#endif

static void do_set_speed(Fixed desired_speed)
{
	if (gf_sc_set_speed(compositor, desired_speed) == GF_OK) {
		playback_speed = desired_speed;
		fprintf(stderr, "Playing at %g speed\n", FIX2FLT(playback_speed));
	} else {
		fprintf(stderr, "Adjusting speed to %g not supported for this content\n", FIX2FLT(desired_speed));
	}
}

#ifdef GPAC_CONFIG_IOS
#include "sensors_def.h"
static void *gyro_dev = NULL;

void ios_sensor_callback(int sensorType, float x, float y, float z, float w)
{
	GF_Event evt;
	memset(&evt, 0, sizeof(GF_Event));

	switch (sensorType) {
	case SENSOR_ORIENTATION:
		evt.type = GF_EVENT_SENSOR_ORIENTATION;
		evt.sensor.x = x;
		evt.sensor.y = y;
		evt.sensor.z = z;
		evt.sensor.w = w;
		gf_sc_user_event(compositor, &evt);
		return;
	default:
		return;
	}
}
#endif


Bool mp4c_event_proc(void *ptr, GF_Event *evt)
{
	if (compositor_mode==LOAD_GUI_CBK) {
#ifdef GPAC_CONFIG_IOS
		if (evt->type == GF_EVENT_SENSOR_REQUEST) {
			if (evt->activate_sensor.sensor_type==GF_EVENT_SENSOR_ORIENTATION) {
				if (!gyro_dev) {
					gyro_dev = sensor_create(SENSOR_ORIENTATION, ios_sensor_callback);
					if (!gyro_dev)
						return GF_FALSE;
				}
				if (evt->activate_sensor.activate) {
					/*start sensors*/
					sensor_start(gyro_dev);
				} else if (gyro_dev) {
					sensor_stop(gyro_dev);
				}
				return GF_TRUE;
			}
			return GF_FALSE;
		}
#endif
		return GF_FALSE;
	}

	switch (evt->type) {
	case GF_EVENT_DURATION:
		duration_ms = (u64) ( 1000 * (s64) evt->duration.duration);
		can_seek = evt->duration.can_seek;
		break;
	case GF_EVENT_MESSAGE:
	{
		if (!evt->message.message) return 0;
		if (evt->message.error) {
			if (!is_connected) last_error = evt->message.error;
			if (evt->message.error==GF_SCRIPT_INFO) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONSOLE, ("%s\n", evt->message.message));
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONSOLE, ("%s: %s\n", evt->message.message, gf_error_to_string(evt->message.error)));
			}
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONSOLE, ("%s\n", evt->message.message));
		}
	}
	break;
	case GF_EVENT_PROGRESS:
	{
		char *szTitle = "";
		if (evt->progress.progress_type==0) {
			szTitle = "Buffer ";
			if (bench_mode && (bench_mode!=3) ) {
				if (evt->progress.done >= evt->progress.total) bench_buffer = 0;
				else bench_buffer = 1 + 100*evt->progress.done / evt->progress.total;
				break;
			}
		}
		else if (evt->progress.progress_type==1) {
			if (bench_mode) break;
			szTitle = "Download ";
		}
		else if (evt->progress.progress_type==2) szTitle = "Import ";
		gf_set_progress(szTitle, evt->progress.done, evt->progress.total);
	}
	break;


	case GF_EVENT_DBLCLICK:
		gf_sc_set_option(compositor, GF_OPT_FULLSCREEN, !gf_sc_get_option(compositor, GF_OPT_FULLSCREEN));
		return 0;

#ifdef DESKTOP_GUI
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
		if (right_down && (window_flags & GF_VOUT_WINDOWLESS) ) {
			GF_Event move;
			move.move.x = evt->mouse.x - last_x;
			move.move.y = last_y-evt->mouse.y;
			move.type = GF_EVENT_MOVE;
			move.move.relative = 1;
			gf_sc_user_event(compositor, &move);
		}
		return 0;
#endif

	case GF_EVENT_KEYUP:
		switch (evt->key.key_code) {
		case GF_KEY_SPACE:
			if (evt->key.flags & GF_KEY_MOD_CTRL) switch_bench(!bench_mode);
			break;
		}
		break;
	case GF_EVENT_KEYDOWN:
		switch (evt->key.key_code) {
		case GF_KEY_SPACE:
			if (evt->key.flags & GF_KEY_MOD_CTRL) {
				/*ignore key repeat*/
				if (!bench_mode) switch_bench(!bench_mode);
			}
			break;
		case GF_KEY_PAGEDOWN:
		case GF_KEY_MEDIANEXTTRACK:
			break;
		case GF_KEY_MEDIAPREVIOUSTRACK:
			break;
		case GF_KEY_ESCAPE:
			gf_sc_set_option(compositor, GF_OPT_FULLSCREEN, !gf_sc_get_option(compositor, GF_OPT_FULLSCREEN));
			break;
		case GF_KEY_F:
			if (evt->key.flags & GF_KEY_MOD_CTRL) fprintf(stderr, "Rendering rate: %f FPS\n", gf_sc_get_fps(compositor, 0));
			break;
		case GF_KEY_T:
			if (evt->key.flags & GF_KEY_MOD_CTRL) fprintf(stderr, "Scene Time: %f \n", gf_sc_get_time_in_ms(compositor)/1000.0);
			break;
		case GF_KEY_D:
			if (evt->key.flags & GF_KEY_MOD_CTRL) gf_sc_set_option(compositor, GF_OPT_DRAW_MODE, (gf_sc_get_option(compositor, GF_OPT_DRAW_MODE)==GF_DRAW_MODE_DEFER) ? GF_DRAW_MODE_IMMEDIATE : GF_DRAW_MODE_DEFER );
			break;
		case GF_KEY_4:
			if (evt->key.flags & GF_KEY_MOD_CTRL)
				gf_sc_set_option(compositor, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_4_3);
			break;
		case GF_KEY_5:
			if (evt->key.flags & GF_KEY_MOD_CTRL)
				gf_sc_set_option(compositor, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_16_9);
			break;
		case GF_KEY_6:
			if (evt->key.flags & GF_KEY_MOD_CTRL)
				gf_sc_set_option(compositor, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_FILL_SCREEN);
			break;
		case GF_KEY_7:
			if (evt->key.flags & GF_KEY_MOD_CTRL)
				gf_sc_set_option(compositor, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_KEEP);
			break;
		case GF_KEY_O:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected) {
				if (gf_sc_get_option(compositor, GF_OPT_MAIN_ADDON)) {
					fprintf(stderr, "Resuming to main content\n");
					gf_sc_set_option(compositor, GF_OPT_PLAY_STATE, GF_STATE_PLAY_LIVE);
				} else {
					fprintf(stderr, "Main addon not enabled\n");
				}
			}
			break;
		case GF_KEY_P:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected) {
				u32 pause_state = gf_sc_get_option(compositor, GF_OPT_PLAY_STATE) ;
				fprintf(stderr, "[Status: %s]\n", pause_state ? "Playing" : "Paused");
				if ((pause_state == GF_STATE_PAUSED) && (evt->key.flags & GF_KEY_MOD_SHIFT)) {
					gf_sc_set_option(compositor, GF_OPT_PLAY_STATE, GF_STATE_PLAY_LIVE);
				} else {
					gf_sc_set_option(compositor, GF_OPT_PLAY_STATE, (pause_state==GF_STATE_PAUSED) ? GF_STATE_PLAYING : GF_STATE_PAUSED);
				}
			}
			break;
		case GF_KEY_S:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected) {
				gf_sc_set_option(compositor, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
				fprintf(stderr, "Step time: ");
				print_time(gf_sc_get_time_in_ms(compositor));
				fprintf(stderr, "\n");
			}
			break;
		case GF_KEY_B:
			break;
		case GF_KEY_M:
			break;
		case GF_KEY_H:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected) {
				gf_sc_switch_quality(compositor, GF_TRUE);
			//	gf_sc_set_option(term, GF_OPT_MULTIVIEW_MODE, 0);
			}
			break;
		case GF_KEY_L:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected) {
				gf_sc_switch_quality(compositor, GF_FALSE);
			//	gf_sc_set_option(compositor, GF_OPT_MULTIVIEW_MODE, 1);
			}
			break;
		case GF_KEY_F5:
			if (is_connected)
				reload = 1;
			break;
		case GF_KEY_A:
			addon_visible = !addon_visible;
			gf_sc_toggle_addons(compositor, addon_visible);
			break;
		case GF_KEY_UP:
			if ((evt->key.flags & VK_MOD) && is_connected) {
				do_set_speed(playback_speed * 2);
			}
			break;
		case GF_KEY_DOWN:
			if ((evt->key.flags & VK_MOD) && is_connected) {
				do_set_speed(playback_speed / 2);
			}
			break;
		case GF_KEY_LEFT:
			if ((evt->key.flags & VK_MOD) && is_connected) {
				do_set_speed(-1 * playback_speed );
			}
			break;

		}
		break;

	case GF_EVENT_CONNECT:
		if (evt->connect.is_connected) {
			is_connected = 1;
			fprintf(stderr, "Service Connected\n");
			eos_seen = GF_FALSE;
			if (playback_speed != FIX_ONE)
				gf_sc_set_speed(compositor, playback_speed);
		} else if (is_connected) {
			fprintf(stderr, "Service %s\n", is_connected ? "Disconnected" : "Connection Failed");
			is_connected = 0;
			duration_ms = 0;
		}

#ifdef DESKTOP_GUI
		if (init_w && init_h) {
			gf_sc_set_size(compositor, init_w, init_h);
		}
		set_window_caption();
#endif
		break;
	case GF_EVENT_EOS:
		eos_seen = GF_TRUE;
		break;
	case GF_EVENT_SIZE:
#ifdef DESKTOP_GUI
		if (window_flags & GF_VOUT_WINDOWLESS) {
			GF_Event move;
			move.type = GF_EVENT_MOVE;
			move.move.align_x = align_mode & 0xFF;
			move.move.align_y = (align_mode>>8) & 0xFF;
			move.move.relative = 2;
			gf_sc_user_event(compositor, &move);
		}
#endif
		break;
	case GF_EVENT_SCENE_SIZE:

#ifdef DESKTOP_GUI
		if (forced_width && forced_height) {
			GF_Event size;
			u32 nw = forced_width ? forced_width : evt->size.width;
			u32 nh = forced_height ? forced_height : evt->size.height;

			if ((nw != evt->size.width) || (nh != evt->size.height)) {
				size.type = GF_EVENT_SIZE;
				size.size.width = nw;
				size.size.height = nh;
				gf_sc_user_event(compositor, &size);
			}
		}
#endif
		break;

	case GF_EVENT_METADATA:
		set_window_caption();
		break;
	case GF_EVENT_DROPFILE:
		if (evt->open_file.nb_files) {
			gf_sc_navigate_to(compositor, evt->open_file.files[0]);
		}
		return 1;

	case GF_EVENT_DISCONNECT:
		gf_sc_disconnect(compositor);
		break;
	case GF_EVENT_MIGRATE:
	{
	}
	break;
	case GF_EVENT_NAVIGATE_INFO:
		if (evt->navigate.to_url)
			fprintf(stderr, "Go to URL: \"%s\"\r", evt->navigate.to_url);
		break;
	case GF_EVENT_NAVIGATE:
		if (gf_sc_is_supported_url(compositor, evt->navigate.to_url, GF_TRUE)) {
			fprintf(stderr, "Navigating to URL %s\n", evt->navigate.to_url);
			gf_sc_navigate_to(compositor, evt->navigate.to_url);
			return 1;
		} else {
			fprintf(stderr, "Navigation destination not supported\nGo to URL: %s\n", evt->navigate.to_url);
		}
		break;
	case GF_EVENT_SET_CAPTION:
		gf_sc_user_event(compositor, evt);
		break;
	case GF_EVENT_AUTHORIZATION:
	{
		u32 nb_retry = 4;
		assert( evt->type == GF_EVENT_AUTHORIZATION);
		assert( evt->auth.user);
		assert( evt->auth.password);
		assert( evt->auth.site_url);
		while ((!strlen(evt->auth.user) || !strlen(evt->auth.password)) && (nb_retry > 0) ) {
			nb_retry--;
			fprintf(stderr, "**** Authorization required for site %s ****\n", evt->auth.site_url);
			fprintf(stderr, "login   : ");
			if (!gf_read_line_input(evt->auth.user, 50, 1))
				continue;
			fprintf(stderr, "\npassword: ");
			if (!gf_read_line_input(evt->auth.password, 50, 0))
				continue;
			fprintf(stderr, "*********\n");
		}
		if (nb_retry == 0) {
			fprintf(stderr, "**** No User or password has been filled, aborting ***\n");
			return 0;
		}
		return 1;
	}
	case GF_EVENT_ADDON_DETECTED:
		if (enable_add_ons) {
			fprintf(stderr, "Media Addon %s detected - enabling it\n", evt->addon_connect.addon_url);
			addon_visible = 1;
		}
		return enable_add_ons;
	}
	return 0;
}


static void set_navigation()
{
	GF_Err e;
	char nav;
	u32 type = gf_sc_get_option(compositor, GF_OPT_NAVIGATION_TYPE);
	e = GF_OK;
//	fflush(stdin);

	if (!type) {
		fprintf(stderr, "Content/compositor doesn't allow user-selectable navigation\n");
	} else if (type==1) {
		fprintf(stderr, "Select Navigation (\'N\'one, \'E\'xamine, \'S\'lide): ");
		nav = do_coverage ? 'N' : gf_getch();
		if (nav=='N') e = gf_sc_set_option(compositor, GF_OPT_NAVIGATION, GF_NAVIGATE_NONE);
		else if (nav=='E') e = gf_sc_set_option(compositor, GF_OPT_NAVIGATION, GF_NAVIGATE_EXAMINE);
		else if (nav=='S') e = gf_sc_set_option(compositor, GF_OPT_NAVIGATION, GF_NAVIGATE_SLIDE);
		else fprintf(stderr, "Unknown selector \'%c\' - only \'N\',\'E\',\'S\' allowed\n", nav);
	} else if (type==2) {
		fprintf(stderr, "Select Navigation (\'N\'one, \'W\'alk, \'F\'ly, \'E\'xamine, \'P\'an, \'S\'lide, \'G\'ame, \'V\'R, \'O\'rbit): ");
		nav = do_coverage ? 'N' : gf_getch();
		if (nav=='N') e = gf_sc_set_option(compositor, GF_OPT_NAVIGATION, GF_NAVIGATE_NONE);
		else if (nav=='W') e = gf_sc_set_option(compositor, GF_OPT_NAVIGATION, GF_NAVIGATE_WALK);
		else if (nav=='F') e = gf_sc_set_option(compositor, GF_OPT_NAVIGATION, GF_NAVIGATE_FLY);
		else if (nav=='E') e = gf_sc_set_option(compositor, GF_OPT_NAVIGATION, GF_NAVIGATE_EXAMINE);
		else if (nav=='P') e = gf_sc_set_option(compositor, GF_OPT_NAVIGATION, GF_NAVIGATE_PAN);
		else if (nav=='S') e = gf_sc_set_option(compositor, GF_OPT_NAVIGATION, GF_NAVIGATE_SLIDE);
		else if (nav=='G') e = gf_sc_set_option(compositor, GF_OPT_NAVIGATION, GF_NAVIGATE_GAME);
		else if (nav=='O') e = gf_sc_set_option(compositor, GF_OPT_NAVIGATION, GF_NAVIGATE_ORBIT);
		else if (nav=='V') e = gf_sc_set_option(compositor, GF_OPT_NAVIGATION, GF_NAVIGATE_VR);
		else fprintf(stderr, "Unknown selector %c - only \'N\',\'W\',\'F\',\'E\',\'P\',\'S\',\'G\', \'V\', \'O\' allowed\n", nav);
	}
	if (e) fprintf(stderr, "Error setting mode: %s\n", gf_error_to_string(e));
}


static void on_rti_log(void *cbk, GF_LOG_Level ll, GF_LOG_Tool lm, const char *fmt, va_list list)
{
	if (rti_logs && (lm & GF_LOG_RTI)) {
		char szMsg[2048];
		vsnprintf(szMsg, 2048, fmt, list);
		update_rti(szMsg + 6 /*"[RTI] "*/);
	}
}

static void init_rti_logs(char *rti_file, Bool use_rtix)
{
	if (rti_logs) gf_fclose(rti_logs);
	rti_logs = gf_fopen(rti_file, "wt");
	if (rti_logs) {
		fprintf(rti_logs, "!! GPAC RunTime Info ");
		fprintf(rti_logs, " !!\n");
		fprintf(rti_logs, "SysTime(ms)\tSceneTime(ms)\tCPU\tFPS\tMemory(kB)\tObservation\n");

		/*turn on RTI loging*/
		if (use_rtix) {
			gf_log_set_callback(NULL, on_rti_log);
			gf_log_set_tool_level(GF_LOG_RTI, GF_LOG_DEBUG);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_RTI, ("[RTI] System state when enabling log\n"));
		} else if (log_rti_time_start) {
			log_rti_time_start = gf_sys_clock_high_res();
		}
	}
}


static char *rti_file=NULL;
Bool use_rtix = GF_FALSE;
char *send_command = NULL;
u32 initial_service_id = 0;
Bool auto_exit = GF_FALSE;
Bool pause_at_first = GF_FALSE;
Bool no_keyboard = GF_FALSE;
Double play_from = 0;

static Bool has_args=GF_FALSE;

#ifdef DESKTOP_GUI
Bool start_fs = GF_FALSE;
#else
Bool start_fs = GF_TRUE;
#endif

Bool mp4c_parse_arg(char *arg, char *arg_val)
{
	if (!strcmp(arg, "-rti")) {
		rti_file = arg_val;
	} else if (!strcmp(arg, "-rtix")) {
		rti_file = arg_val;
		use_rtix = GF_TRUE;
	} else if (!strcmp(arg, "-rti-refresh")) {
		rti_update_time_ms = arg_val ? get_s32(arg_val, "rti-refresh") : 0;
	} else if (!strcmp(arg, "-cov")) {
		do_coverage = GF_TRUE;
	}
#ifdef DESKTOP_GUI
	else if (!stricmp(arg, "-size")) {
		/*usage of %ud breaks sscanf on MSVC*/
		if (arg_val && (sscanf(arg_val, "%dx%d", &forced_width, &forced_height) != 2)) {
			forced_width = forced_height = 0;
		}
	}
	else if (!strcmp(arg, "-fs")) start_fs = 1;
#endif
	else if (!strcmp(arg, "-nk")) no_keyboard = GF_TRUE;
	/*arguments only used in non-gui mode*/
	else if (compositor_mode==LOAD_MP4C) {
		if (arg[0] != '-') {
		}
		else if (!strcmp(arg, "-bm")) bench_mode = 1;
		else if (!strcmp(arg, "-vbm")) bench_mode = 2;
		else if (!strcmp(arg, "-sbm")) bench_mode = 3;
		else if (!strcmp(arg, "-loop")) loop_at_end = GF_TRUE;
		else if (!strcmp(arg, "-no-addon")) enable_add_ons = GF_FALSE;
		else if (!strcmp(arg, "-pause")) pause_at_first = 1;
		else if (!strcmp(arg, "-start")) {
			play_from = arg_val ? atof(arg_val) : 0;
		}
		else if (!strcmp(arg, "-speed")) {
			if (arg_val) playback_speed = FLT2FIX( atof(arg_val) );
			if (playback_speed == 0) { playback_speed = FIX_ONE; pause_at_first = 1; }
			else if (playback_speed < 0) playback_speed = FIX_ONE;
		}
#ifdef DESKTOP_GUI
		else if (!strcmp(arg, "-no-wnd")) window_flags |= GF_VOUT_WINDOWLESS;
		else if (!strcmp(arg, "-no-back")) window_flags |= GF_VOUT_WINDOW_NO_DECORATION;
		else if (!strcmp(arg, "-align")) {
			if (!arg_val) {}
			else if (arg_val[0]=='m') align_mode = 1;
			else if (arg_val[0]=='b') align_mode = 2;
			align_mode <<= 8;
			if (!arg_val) {}
			else if (arg_val[1]=='m') align_mode |= 1;
			else if (arg_val[1]=='r') align_mode |= 2;
		}
#endif
		else if (!strcmp(arg, "-exit")) auto_exit = GF_TRUE;
		else if (!stricmp(arg, "-com")) send_command = arg_val;
		else if (!stricmp(arg, "-service")) initial_service_id = arg_val ? get_u32(arg_val, "service") : 0;
		else {
			return GF_FALSE;
		}
	} else {
		return GF_FALSE;
	}
	has_args = GF_TRUE;
	return GF_TRUE;
}



void load_compositor(GF_Filter *);

void load_compositor(GF_Filter *filter)
{
	const char *str;

	char szArgs[20];
	sprintf(szArgs, "%d", window_flags);
	gf_opts_set_key("temp", "window-flags", szArgs);


	comp_filter = filter;
	compositor = gf_filter_get_udta(comp_filter);
	if (start_fs) gf_sc_set_option(compositor, GF_OPT_FULLSCREEN, 1);
	if (play_from || pause_at_first)
		gf_sc_connect_from_time(compositor, NULL, (u64) (play_from*1000), pause_at_first, GF_FALSE, NULL);

#if (defined(__DARWIN__) || defined(__APPLE__)) && !defined(GPAC_CONFIG_IOS)
	/*the output window is init, inject carbon event watcher on open doc, only in GUI mode (when launched by Finder)
	keep the hook active for the entire app so we can get send back "open with" commands to gpac
	*/
	if (compositor_mode==LOAD_GUI_ENV)
		carbon_set_hook();
#endif


	if (compositor_mode!=LOAD_MP4C) return;

	if (rti_file) init_rti_logs(rti_file, use_rtix);

	GF_SystemRTInfo rti;
	if (gf_sys_get_rti(0, &rti, 0) && (compositor_mode==LOAD_MP4C))
		fprintf(stderr, "System info: %d MB RAM - %d cores\n", (u32) (rti.physical_memory/1024/1024), rti.nb_cores);

	if (bench_mode) {
		gf_opts_discard_changes();
		auto_exit = GF_TRUE;
		if (bench_mode!=2)
			gf_opts_set_key("temp", "no-video", "yes");
	}

#ifdef DESKTOP_GUI
	init_w = forced_width;
	init_h = forced_height;

	if (forced_width && forced_height) {
/*		char dim[50];
		sprintf(dim, "%d", forced_width);
		gf_opts_set_key("Temp", "DefaultWidth", dim);
		sprintf(dim, "%d", forced_height);
		gf_opts_set_key("Temp", "DefaultHeight", dim);
*/
	}
#endif

	if (gf_opts_get_bool("core", "proxy-on")) {
		str = gf_opts_get_key("core", "proxy-name");
		if (str) fprintf(stderr, "HTTP Proxy %s enabled\n", str);
	}

	if (rti_file) {
		update_rti("At GPAC load time\n");
	}

	if (bench_mode) {
		display_rti = 2;
		gf_sc_set_option(compositor, GF_OPT_VIDEO_BENCH, (bench_mode==3) ? 2 : 1);
		if (bench_mode==1) bench_mode=2;
	}

	if (bench_mode) {
		rti_update_time_ms = 500;
		bench_mode_start = gf_sys_clock();
	}
}


#ifdef GPAC_ENABLE_COVERAGE
static void mp4c_coverage()
{
	u32 k, nb_drawn;
	GF_Event evt;
	Bool is_bound;
	const char *outName;

	GF_List *descs = gf_list_new();
	gf_sc_get_world_info(compositor, descs);
	gf_list_del(descs);


	gf_sc_dump_scene(compositor, NULL, NULL, GF_FALSE, 0);
	gf_sc_get_current_service_id(compositor);
	gf_sc_toggle_addons(compositor, GF_FALSE);
	set_navigation();

	get_gui_cmd('v');

	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_MOUSEUP;
	evt.mouse.x = 20;
	evt.mouse.y = 20;
	gf_sc_user_event(compositor, &evt);

	gf_sc_set_option(compositor, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
	//exercise step clocks
	gf_sc_set_option(compositor, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);

	gf_sc_get_url(compositor);
	gf_sc_get_simulation_frame_rate(compositor, &nb_drawn);

	gf_sc_has_text_selection(compositor);
	gf_sc_get_selected_text(compositor);
	gf_sc_paste_text(compositor, "test");

	gf_sc_set_option(compositor, GF_OPT_AUDIO_MUTE, 1);

	mp4c_take_screenshot(GF_TRUE);

	gf_sc_scene_update(compositor, NULL, "REPLACE DYN_TRANS.translation BY 10 10");
	gf_sc_add_object(compositor, NULL, GF_TRUE);

	gf_sc_get_viewpoint(compositor, 1, &outName, &is_bound);
	gf_sc_set_viewpoint(compositor, 1, "testvp");

	for (k=GF_NAVIGATE_WALK; k<=GF_NAVIGATE_VR; k++) {
		gf_sc_set_option(compositor, GF_OPT_NAVIGATION, k);
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_MOUSEDOWN;
		evt.mouse.x = 100;
		evt.mouse.y = 100;
		gf_sc_user_event(compositor, &evt);
		evt.type = GF_EVENT_MOUSEMOVE;
		evt.mouse.x += 10;
		gf_sc_user_event(compositor, &evt);
		evt.mouse.y += 10;
		gf_sc_user_event(compositor, &evt);

		evt.type = GF_EVENT_KEYDOWN;
		evt.key.key_code = GF_KEY_CONTROL;
		gf_sc_user_event(compositor, &evt);

		evt.type = GF_EVENT_MOUSEMOVE;
		evt.mouse.x = 120;
		evt.mouse.y = 110;
		gf_sc_user_event(compositor, &evt);

		evt.type = GF_EVENT_KEYUP;
		evt.key.key_code = GF_KEY_CONTROL;
		gf_sc_user_event(compositor, &evt);

		evt.type = GF_EVENT_KEYDOWN;
		evt.key.key_code = GF_KEY_J;
		gf_sc_user_event(compositor, &evt);
	}
	gf_sc_set_option(compositor, GF_OPT_NAVIGATION_TYPE, 0);

	gf_sc_connect_from_time(compositor, "logo.jpg", 0, 0, GF_FALSE, "./media/auxiliary_files/");
	gf_sc_navigate_to(compositor, "./media/auxiliary_files/logo.jpg");
	gpac_open_urls("./media/auxiliary_files/logo.jpg");
	switch_bench(1);
	do_set_speed(1.0);
}
#endif

#if defined(WIN32) && !defined(_WIN32_WCE)
static void close_console();
#endif

void unload_compositor()
{
	if (rti_file) update_rti("Disconnected\n");

	if (rti_logs) gf_fclose(rti_logs);

#ifdef GPAC_ENABLE_COVERAGE
	if (do_coverage) {
		mp4c_coverage();
	}
#endif

#if defined(WIN32) && !defined(_WIN32_WCE)
	close_console();
#endif


#ifdef DESKTOP_GUI
	if (caption) gf_free(caption);
	caption = NULL;
#endif

	comp_filter = NULL;
	compositor = NULL;
#if (defined(__DARWIN__) || defined(__APPLE__)) && !defined(GPAC_CONFIG_IOS)
	if (compositor_mode==LOAD_GUI_ENV)
		carbon_remove_hook();
#endif


#ifdef GPAC_CONFIG_IOS
	if (gyro_dev) {
		sensor_stop(gyro_dev);
	}
#endif

}

Bool mp4c_task()
{
	if (eos_seen && gf_sc_get_option(compositor, GF_OPT_IS_OVER)) {
		if (!auto_exit) return GF_FALSE;
		if (!loop_at_end) {
			gf_sc_disconnect(compositor);
			return GF_TRUE;
		}
		eos_seen = GF_FALSE;
		gf_sc_play_from_time(compositor, 0, 0);
		return GF_FALSE;
	}

	if (reload) {
		char *url = (char *) gf_sc_get_url(compositor);
		if (url) {
			url = gf_strdup(url);
			gf_sc_disconnect(compositor);
			gf_sc_connect_from_time(compositor, url, 0, 0, GF_FALSE, NULL);
			gf_free(url);
		}
		reload = 0;
	}

	if (send_command && is_connected) {
		gf_sc_scene_update(compositor, NULL, send_command);
		send_command = NULL;
	}
	if (initial_service_id && is_connected) {
		gf_sc_select_service(compositor, initial_service_id);
		initial_service_id = 0;
	}

	if (!use_rtix || display_rti)
		update_rti(NULL);

	return GF_FALSE;
}

Bool mp4c_handle_prompt(u8 char_val)
{
	MP4C_Command cmdtype = get_gui_cmd(char_val);
	switch (cmdtype) {
	case MP4C_OPEN:
	{
		char szURL[2000];
		gf_sc_disconnect(compositor);
		fprintf(stderr, "Enter the absolute URL\n");
		if (1 > scanf("%1999s", szURL)) {
			fprintf(stderr, "Cannot read absolute URL, aborting\n");
			break;
		}
		if (rti_file) init_rti_logs(rti_file, use_rtix);
		gf_sc_connect_from_time(compositor, szURL, 0, 0, 0, NULL);
	}
		break;
	case MP4C_RELOAD:
		if (is_connected)
			reload = 1;
		break;

	case MP4C_DISCONNECT:
		if (is_connected) gf_sc_disconnect(compositor);
		break;

	case MP4C_PAUSE_RESUME:
		if (is_connected) {
			Bool is_pause = gf_sc_get_option(compositor, GF_OPT_PLAY_STATE);
			fprintf(stderr, "[Status: %s]\n", is_pause ? "Playing" : "Paused");
			gf_sc_set_option(compositor, GF_OPT_PLAY_STATE, is_pause ? GF_STATE_PLAYING : GF_STATE_PAUSED);
		}
		break;
	case MP4C_STEP:
		if (is_connected) {
			gf_sc_set_option(compositor, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
			fprintf(stderr, "Step time: ");
			print_time(gf_sc_get_time_in_ms(compositor));
			fprintf(stderr, "\n");
		}
		break;

	case MP4C_SEEK:
	case MP4C_SEEK_TIME:
		if (!can_seek || (duration_ms<=2000)) {
			fprintf(stderr, "scene not seekable\n");
		} else {
			Double res;
			s32 seekTo;
			fprintf(stderr, "Duration: ");
			print_time(duration_ms);
			res = gf_sc_get_time_in_ms(compositor);
			if (cmdtype==MP4C_SEEK) {
				res *= 100;
				res /= (s64)duration_ms;
				fprintf(stderr, " (current %.2f %%)\nEnter Seek percentage:\n", res);
				if (scanf("%d", &seekTo) == 1) {
					if (seekTo > 100) seekTo = 100;
					res = (Double)(s64)duration_ms;
					res /= 100;
					res *= seekTo;
					gf_sc_play_from_time(compositor, (u64) (s64) res, 0);
				}
			} else {
				u32 r, h, m, s;
				fprintf(stderr, " - Current Time: ");
				print_time((u64) res);
				fprintf(stderr, "\nEnter seek time (Format: s, m:s or h:m:s):\n");
				h = m = s = 0;
				r =scanf("%d:%d:%d", &h, &m, &s);
				if (r==2) {
					s = m;
					m = h;
					h = 0;
				}
				else if (r==1) {
					s = h;
					m = h = 0;
				}

				if (r && (r<=3)) {
					u64 time = h*3600 + m*60 + s;
					gf_sc_play_from_time(compositor, time*1000, 0);
				}
			}
		}
		break;

	case MP4C_TIME:
	{
		if (is_connected) {
			fprintf(stderr, "Current Time: ");
			print_time(gf_sc_get_time_in_ms(compositor));
			fprintf(stderr, " - Duration: ");
			print_time(duration_ms);
			fprintf(stderr, "\n");
		}
	}
	break;
	case MP4C_NAVMODE:
		if (is_connected) set_navigation();
		break;
	case MP4C_LASTVP:
		if (is_connected) gf_sc_set_option(compositor, GF_OPT_NAVIGATION_TYPE, 0);
		break;

	case MP4C_DUMPSCENE:
		if (is_connected) {
			char radname[GF_MAX_PATH], *sExt;
			Bool xml_dump, std_out;
			radname[0] = 0;
			do {
				fprintf(stderr, "Enter file radical name (+\'.x\' for XML dumping) - \"std\" for stderr: ");
				fflush(stderr);
			} while( 1 > scanf("%1023s", radname));
			sExt = strrchr(radname, '.');
			xml_dump = 0;
			if (sExt) {
				if (!stricmp(sExt, ".x")) xml_dump = 1;
				sExt[0] = 0;
			}
			std_out = strnicmp(radname, "std", 3) ? 0 : 1;
			GF_Err e = gf_sc_dump_scene(compositor, std_out ? NULL : radname, NULL, xml_dump, 0);
			if (e<0)
				fprintf(stderr, "Dump failed: %s\n", gf_error_to_string(e));
			else
				fprintf(stderr, "Dump done\n");
		}
		break;

	case MP4C_OGL2D:
	{
		Bool use_3d = !gf_sc_get_option(compositor, GF_OPT_USE_OPENGL);
		if (gf_sc_set_option(compositor, GF_OPT_USE_OPENGL, use_3d)==GF_OK) {
			fprintf(stderr, "Using %s for 2D drawing\n", use_3d ? "OpenGL" : "2D rasterizer");
		}
	}
	break;
	case MP4C_STRESSMODE:
	{
		Bool opt = gf_sc_get_option(compositor, GF_OPT_STRESS_MODE);
		opt = !opt;
		fprintf(stderr, "Turning stress mode %s\n", opt ? "on" : "off");
		gf_sc_set_option(compositor, GF_OPT_STRESS_MODE, opt);
	}
	break;
	case MP4C_AR_4_3:
		gf_sc_set_option(compositor, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_4_3);
		break;
	case MP4C_AR_16_9:
		gf_sc_set_option(compositor, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_16_9);
		break;
	case MP4C_AR_NONE:
		gf_sc_set_option(compositor, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_FILL_SCREEN);
		break;
	case MP4C_AR_ORIG:
		gf_sc_set_option(compositor, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_KEEP);
		break;

	case MP4C_DISP_RTI:
		display_rti = !display_rti;
		set_window_caption();
		break;
	case MP4C_DISP_FPS:
		if (display_rti) display_rti = 0;
		else display_rti = 2;
		set_window_caption();
		break;
	case MP4C_UPDATE:
	{
		char szCom[8192];
		fprintf(stderr, "Enter command to send:\n");
		szCom[0] = 0;
		if (1 > scanf("%8191[^\t\n]", szCom)) {
			fprintf(stderr, "Cannot read command to send, aborting.\n");
			break;
		}
		GF_Err e = gf_sc_scene_update(compositor, NULL, szCom);
		if (e) fprintf(stderr, "Processing command failed: %s\n", gf_error_to_string(e));
	}
	break;
	case MP4C_EVALJS:
	{
		char jsCode[8192];
		fprintf(stderr, "Enter JavaScript code to evaluate:\n");
		jsCode[0] = 0;
		if (1 > scanf("%8191[^\t\n]", jsCode)) {
			fprintf(stderr, "Cannot read code to evaluate, aborting.\n");
			break;
		}
		GF_Err e = gf_sc_scene_update(compositor, "application/ecmascript", jsCode);
		if (e) fprintf(stderr, "Processing JS code failed: %s\n", gf_error_to_string(e));
	}
	break;

	case MP4C_LOGS:
	{
		char szLog[1024], *cur_logs;
		cur_logs = gf_log_get_tools_levels();
		fprintf(stderr, "Enter new log level (current tools %s):\n", cur_logs);
		gf_free(cur_logs);
		if (scanf("%1023s", szLog) < 1) {
			fprintf(stderr, "Cannot read new log level, aborting.\n");
			break;
		}
		gf_log_modify_tools_levels(szLog);
	}
	break;

	case MP4C_VMEM_CACHE:
	{
		u32 size;
		do {
			fprintf(stderr, "Enter new video cache memory in kBytes (current %ud):\n", gf_sc_get_option(compositor, GF_OPT_VIDEO_CACHE_SIZE));
		} while (1 > scanf("%ud", &size));
		gf_sc_set_option(compositor, GF_OPT_VIDEO_CACHE_SIZE, size);
	}
	break;

	case MP4C_DOWNRATE:
	{
		u32 http_bitrate = gf_sc_get_option(compositor, GF_OPT_HTTP_MAX_RATE);
		do {
			fprintf(stderr, "Enter new http bitrate in bps (0 for none) - current limit: %d\n", http_bitrate);
		} while (1 > scanf("%ud", &http_bitrate));

		gf_sc_set_option(compositor, GF_OPT_HTTP_MAX_RATE, http_bitrate);
	}
	break;

	case MP4C_RELOAD_OPTS:
		gf_sc_set_option(compositor, GF_OPT_RELOAD_CONFIG, 1);
		break;

#ifdef DESKTOP_GUI
	/*extract to PNG*/
	case MP4C_SCREENSHOT:
		mp4c_take_screenshot(GF_FALSE);
		break;
#endif

	case MP4C_HELP:
		mp4c_print_help();
		break;
	default:
		return GF_FALSE;
	}
	return GF_TRUE;
}


#ifdef DESKTOP_GUI
static void mp4c_take_screenshot(Bool for_coverage)
{
	char szFileName[100];
	u32 nb_pass, nb_views, offscreen_view = 0;
	GF_VideoSurface fb;
	GF_Err e;
	nb_pass = 1;
	nb_views = gf_sc_get_option(compositor, GF_OPT_NUM_STEREO_VIEWS);
	if (nb_views>1) {
		fprintf(stderr, "Auto-stereo mode detected - type number of view to dump (0 is main output, 1 to %d offscreen view, %d for all offscreen, %d for all offscreen and main)\n", nb_views, nb_views+1, nb_views+2);
		if (!for_coverage) {
			if (scanf("%d", &offscreen_view) != 1) {
				offscreen_view = 0;
			}
		}
		if (offscreen_view==nb_views+1) {
			offscreen_view = 1;
			nb_pass = nb_views;
		}
		else if (offscreen_view==nb_views+2) {
			offscreen_view = 0;
			nb_pass = nb_views+1;
		}
	}
	if (for_coverage && !offscreen_view) {
		if (gf_sc_get_offscreen_buffer(compositor, &fb, 0, 0)==GF_OK)
			gf_sc_release_screen_buffer(compositor, &fb);
	}
	while (nb_pass) {
		nb_pass--;
		if (offscreen_view) {
			sprintf(szFileName, "view%d_dump.png", offscreen_view);
			e = gf_sc_get_offscreen_buffer(compositor, &fb, offscreen_view-1, 0);
		} else {
			sprintf(szFileName, "gpac_video_dump_"LLU".png", gf_net_get_utc() );
			e = gf_sc_get_screen_buffer(compositor, &fb, 0);
		}
		offscreen_view++;
		if (e) {
			fprintf(stderr, "Error dumping screen buffer %s\n", gf_error_to_string(e) );
			nb_pass = 0;
		} else {
#ifndef GPAC_DISABLE_AV_PARSERS
			u32 dst_size = fb.width*fb.height*4;
			u8 *dst = (u8*)gf_malloc(sizeof(u8)*dst_size);

			e = gf_img_png_enc(fb.video_buffer, fb.width, fb.height, fb.pitch_y, fb.pixel_format, dst, &dst_size);
			if (e) {
				fprintf(stderr, "Error encoding PNG %s\n", gf_error_to_string(e) );
				nb_pass = 0;
			} else {
				FILE *png = gf_fopen(szFileName, "wb");
				if (!png) {
					fprintf(stderr, "Error writing file %s\n", szFileName);
					nb_pass = 0;
				} else {
					if (gf_fwrite(dst, dst_size, png) != dst_size) {
						fprintf(stderr, "Error dumping screen buffer %s\n", gf_error_to_string(GF_IO_ERR) );
					}
					gf_fclose(png);
					fprintf(stderr, "Dump to %s\n", szFileName);
				}
			}
			if (dst) gf_free(dst);
			gf_sc_release_screen_buffer(compositor, &fb);

			if (for_coverage) gf_file_delete(szFileName);
#endif //GPAC_DISABLE_AV_PARSERS
		}
	}
	fprintf(stderr, "Done: %s\n", szFileName);
}
#endif


#if defined(WIN32) && !defined(_WIN32_WCE)

#include <tlhelp32.h>
#include <psapi.h>

static DWORD getParentPID(DWORD pid)
{
	DWORD ppid = 0;
	HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (h) {
		PROCESSENTRY32 pe = { 0 };
		pe.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(h, &pe)) {
			do {
				if (pe.th32ProcessID == pid) {
					ppid = pe.th32ParentProcessID;
					break;
				}
			} while (Process32Next(h, &pe));
		}
		CloseHandle(h);
	}
	return (ppid);
}

static void getProcessName(DWORD pid, PUCHAR fname, DWORD sz)
{
	HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (h) {
		GetModuleFileNameEx(h, NULL, fname, sz);
		CloseHandle(h);
	}
}

static Bool owns_wnd = GF_FALSE;
HWND console_hwnd = NULL;
Bool gpac_is_global_launch()
{
	typedef HWND(WINAPI *GetConsoleWindowT)(void);
	HMODULE hk32 = GetModuleHandle("kernel32.dll");
	char parentName[GF_MAX_PATH];
	DWORD dwProcessId = 0;
	DWORD dwParentProcessId = 0;
	DWORD dwParentParentProcessId = 0;
	Bool no_parent_check = GF_FALSE;
	GetConsoleWindowT GetConsoleWindow = (GetConsoleWindowT)GetProcAddress(hk32, "GetConsoleWindow");
	console_hwnd = GetConsoleWindow();
	dwProcessId = GetCurrentProcessId();
	dwParentProcessId = getParentPID(dwProcessId);
	if (dwParentProcessId)
		dwParentParentProcessId = getParentPID(dwParentProcessId);
	//get parent process name, check for explorer
	parentName[0] = 0;
#ifndef GPAC_BUILD_FOR_WINXP
	getProcessName(dwParentProcessId, parentName, GF_MAX_PATH);
	if (strstr(parentName, "explorer")) {
		owns_wnd = GF_TRUE;
	}
#if 0
	//get parent parent process name, check for devenv (or any other ide name ...)
	else if (dwParentParentProcessId) {
		owns_wnd = GF_FALSE;
		parentName[0] = 0;
		getProcessName(dwParentParentProcessId, parentName, GF_MAX_PATH);
		if (strstr(parentName, "devenv")) {
			owns_wnd = GF_TRUE;
		}
	}
#endif
	else {
		owns_wnd = GF_FALSE;
	}
	if (!owns_wnd) return GF_FALSE;
#endif //GPAC_BUILD_FOR_WINXP
	ShowWindow(console_hwnd, SW_HIDE);
	return GF_TRUE;
}
static void close_console()
{
	if (owns_wnd && console_hwnd)
		PostMessage(console_hwnd, WM_CLOSE, 0, 0);
}

#endif

#endif //#ifndef GPAC_DISABLE_PLAYER
