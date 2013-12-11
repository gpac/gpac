/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2005-2012
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
#include <gpac/media_tools.h>
#include <gpac/options.h>
#include <gpac/modules/service.h>
#include <gpac/avparse.h>
#include <time.h>

/*ISO 639 languages*/
#include <gpac/iso639.h>

#include <gpac/internal/terminal_dev.h>


#ifndef WIN32
#include <dlfcn.h>
#include <pwd.h>
#include <unistd.h>
#if defined(__DARWIN__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/stat.h>
#endif

#else
#include <windows.h> /*for GetModuleFileName*/
#endif	//WIN32

/*local prototypes*/
void PrintWorldInfo(GF_Terminal *term);
void ViewOD(GF_Terminal *term, u32 OD_ID, u32 number);
void PrintODList(GF_Terminal *term, GF_ObjectManager *root_odm, u32 num, u32 indent, char *root_name);

void ViewODs(GF_Terminal *term, Bool show_timing);
void PrintGPACConfig();

static u32 gui_mode = 0;

static Bool restart = GF_FALSE;
static Bool reload = GF_FALSE;
#if defined(__DARWIN__) || defined(__APPLE__)
//we keep no decoder thread because of JS_GC deadlocks between threads ...
static u32 threading_flags = GF_TERM_NO_COMPOSITOR_THREAD | GF_TERM_NO_DECODER_THREAD;
#else
static u32 threading_flags = 0;
#endif
static Bool no_audio = GF_FALSE;
static Bool term_step = GF_FALSE;
static Bool no_regulation = GF_FALSE;
static Bool bench_mode = GF_FALSE;
static Bool eos_seen = GF_FALSE;
Bool is_connected = GF_FALSE;
Bool startup_file = GF_FALSE;
GF_User user;
GF_Terminal *term;
u64 Duration;
GF_Err last_error = GF_OK;

static Bool request_next_playlist_item = GF_FALSE;
FILE *playlist = NULL;
static Bool readonly_playlist = GF_FALSE;

static GF_Config *cfg_file;
static u32 display_rti = 0;
static Bool Run;
static Bool CanSeek = GF_FALSE;
static char the_url[GF_MAX_PATH];
static char pl_path[GF_MAX_PATH];
static Bool no_mime_check = GF_TRUE;
static Bool be_quiet = GF_FALSE;
static u32 log_time_start = 0;
static Bool log_utc_time = GF_FALSE;
static Bool loop_at_end = GF_FALSE;
static u32 forced_width=0;
static u32 forced_height=0;

/*windowless options*/
u32 align_mode = 0;
u32 init_w = 0;
u32 init_h = 0;
u32 last_x, last_y;
Bool right_down = GF_FALSE;

void dump_frame(GF_Terminal *term, char *rad_path, u32 dump_type, u32 frameNum);
Bool dump_file(char *the_url, u32 dump_mode, Double fps, u32 width, u32 height, Float scale, u32 *times, u32 nb_times);


void hide_shell(u32 cmd_type)
{
#if defined(WIN32) && !defined(_WIN32_WCE)
	typedef HWND (WINAPI *GetConsoleWindowT)(void);
	HMODULE hk32 = GetModuleHandle("kernel32.dll");
	GetConsoleWindowT GetConsoleWindow = (GetConsoleWindowT ) GetProcAddress(hk32,"GetConsoleWindow");
	if (cmd_type==0) ShowWindow( GetConsoleWindow(), SW_SHOW);
	else if (cmd_type==1) ShowWindow( GetConsoleWindow(), SW_HIDE);
	else if (cmd_type==2) PostMessage(GetConsoleWindow(), WM_CLOSE, 0, 0);
#endif
}




void PrintUsage()
{
	fprintf(stderr, "Usage MP4Client [options] [filename]\n"
		"\t-c fileName:    user-defined configuration file. Also works with -cfg\n"
#ifdef GPAC_MEMORY_TRACKING
		"\t-mem-track:  enables memory tracker\n"
#endif
		"\t-rti fileName:  logs run-time info (FPS, CPU, Mem usage) to file\n"
		"\t-rtix fileName: same as -rti but driven by GPAC logs\n"
		"\t-quiet:         removes script message, buffering and downloading status\n"
		"\t-strict-error:  exit when the player reports its first error\n"
		"\t-opt	option:    Overrides an option in the configuration file. String format is section:key=value\n"
		"\t-log-file file: sets output log file. Also works with -lf\n"
		"\t-logs log_args: sets log tools and levels, formatted as a ':'-separated list of toolX[:toolZ]@levelX\n"
		"\t                 levelX can be one of:\n"
		"\t        \"quiet\"      : skip logs\n"
		"\t        \"error\"      : logs only error messages\n"
		"\t        \"warning\"    : logs error+warning messages\n"
		"\t        \"info\"       : logs error+warning+info messages\n"
		"\t        \"debug\"      : logs all messages\n"
		"\t                 toolX can be one of:\n"
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
		"\t        \"smil\"       : SMIL timing engine\n"
		"\t        \"compose\"    : composition engine (2D, 3D, etc)\n"
		"\t        \"mmio\"       : Audio/Video HW I/O management\n"
		"\t        \"rti\"        : various run-time stats\n"
		"\t        \"cache\"      : HTTP cache subsystem\n"
		"\t        \"audio\"      : Audio renderer and mixers\n"
#ifdef GPAC_MEMORY_TRACKING
		"\t        \"mem\"        : GPAC memory tracker\n"
#endif
#ifndef GPAC_DISABLE_DASH_CLIENT
		"\t        \"dash\"       : HTTP streaming logs\n"
#endif
		"\t        \"module\"     : GPAC modules debugging\n"
		"\t        \"mutex\"      : mutex\n"
		"\t        \"all\"        : all tools logged - other tools can be specified afterwards.\n"
		"\n"
		"\t-log-clock or -lc      : logs time in ms since start time of GPAC before each log line.\n"
		"\t-log-utc or -lu        : logs UTC time in ms before each log line.\n"
		"\t-ifce IPIFCE           : Sets default Multicast interface\n"
		"\t-size WxH:      specifies visual size (default: scene size)\n"
#if defined(__DARWIN__) || defined(__APPLE__)
		"\t-thread:        enables thread usage for terminal and compositor \n"
#else
		"\t-no-thread:     disables thread usage (except for audio)\n"
#endif
		"\t-no-audio:	   disables audio \n"
		"\t-no-wnd:        uses windowless mode (Win32 only)\n"
		"\t-no-back:       uses transparent background for output window when no background is specified (Win32 only)\n"
		"\t-align vh:      specifies v and h alignment for windowless mode\n"
		"                   possible v values: t(op), m(iddle), b(ottom)\n"
		"                   possible h values: l(eft), m(iddle), r(ight)\n"
		"                   default alignment is top-left\n"
		"                   default alignment is top-left\n"
		"\t-pause:         pauses at first frame\n"
		"\t-loop:          loops presentation\n"
		"\t-no-regulation: disables framerate regulation\n"
		"\t-bench:         sets playback in bench mode (as fast as possible)\n"
		"\t-fs:	           starts in fullscreen mode\n"
		"\t-views v1:.:vN: creates an auto-stereo scene of N views. vN can be any type of URL supported by GPAC. \n"
		"                    in this mode, URL argument of GPAC is ignored, GUI as well.\n"
		"                    this is equivalent as using views://v1:.:N as an URL.\n"
		"\n"
		"\t-exit:          automatically exits when presentation is over\n"
		"\t-run-for TIME:  runs for TIME seconds and exits\n"
		"\t-gui:           starts in GUI mode. The GUI is indicated in GPAC config, section General, by the key [StartupFile]\n"
		"\n"
		"Dumper Options:\n"
		"\t-bmp [times]:   dumps given frames to bmp\n"
		"\t-png [times]:   dumps given frames to png\n"
		"\t-raw [times]:   dumps given frames to raw\n"
		"\t-avi [times]:   dumps given file to raw avi\n"
		"\t-rgbds:         dumps the RGBDS pixel format texture\n"
		"                   with -avi [times]: dumps an rgbds-format .avi\n"
		"\t-rgbd:          dumps the RGBD pixel format texture\n"
		"					with -avi [times]: dumps an rgbd-format .avi\n"		
		"\t-depth:         dumps depthmap (z-buffer) frames\n"
		"                   with -avi [times]: dumps depthmap in grayscale .avi\n"		
		"                   with -bmp: dumps depthmap in grayscale .bmp\n"		
		"\t-fps FPS:       specifies frame rate for AVI dumping (default: %f)\n"
		"\t-scale s:       scales the visual size (default: 1)\n"
		"\t-fill:          uses fill aspect ratio for dumping (default: none)\n"
		"\t-show:          show window while dumping (default: no)\n"
		"\n"
		"\t-help:          show this screen\n"
		"\n"
		"MP4Client - GPAC command line player and dumper - version "GPAC_FULL_VERSION"\n"
		"GPAC Written by Jean Le Feuvre (c) 2001-2005 - ENST (c) 2005-200X\n"
		"GPAC Configuration: " GPAC_CONFIGURATION "\n"
		"Features: %s\n", 
		GF_IMPORT_DEFAULT_FPS,
		gpac_features()
		);
}

void PrintHelp()
{
	fprintf(stderr, "MP4Client command keys:\n"
		"\tq: quit\n"
		"\tX: kill\n"
		"\to: connect to the specified URL\n"
		"\tO: connect to the specified playlist\n"
		"\tN: switch to the next URL in the playlist. Also works with \\n\n"
		"\tP: jumps to a given number ahead in the playlist\n"
		"\tr: reload current presentation\n"
		"\tD: disconnects the current presentation\n"
		"\n"
		"\tp: play/pause the presentation\n"
		"\ts: step one frame ahead\n"
		"\tz: seek into presentation by percentage\n"
		"\tT: seek into presentation by time\n"
		"\tt: print current timing\n"
		"\n"
		"\tu: sends a command (BIFS or LASeR) to the main scene\n"
		"\tZ: dumps output video to PNG\n"
		"\n"
		"\tw: view world info\n"
		"\tv: view Object Descriptor list\n"
		"\ti: view Object Descriptor info (by ID)\n"
		"\tj: view Object Descriptor info (by number)\n"
		"\tb: view media objects timing and buffering info\n"
		"\tm: view media objects buffering and memory info\n"
		"\td: dumps scene graph\n"
		"\n"
		"\tk: turns stress mode on/off\n"
		"\tn: changes navigation mode\n"
		"\tx: reset to last active viewpoint\n"
		"\n"
		"\t3: switch OpenGL on or off for 2D scenes\n"
		"\n"
		"\t4: forces 4/3 Aspect Ratio\n"
		"\t5: forces 16/9 Aspect Ratio\n"
		"\t6: forces no Aspect Ratio (always fill screen)\n"
		"\t7: forces original Aspect Ratio (default)\n"
		"\n"
		"\tL: changes to new log level. CF MP4Client usage for possible values\n"
		"\tT: select new tools to log. CF MP4Client usage for possible values\n"
		"\n"
		"\tl: list available modules\n"
		"\tc: prints some GPAC configuration info\n"
		"\tE: forces reload of GPAC configuration\n"
		"\n"
		"\tR: toggles run-time info display in window title bar on/off\n"
		"\tF: toggle displaying of FPS in stderr on/off\n"
		"\tg: print GPAC allocated memory\n"
		"\th: print this message\n"
		"\n"
		"\tEXPERIMENTAL/UNSTABLE OPTIONS\n"
		"\tC: Enable Streaming Cache\n"
		"\tS: Stops Streaming Cache and save to file\n"
		"\tA: Aborts Streaming Cache\n"
		"\tM: specifies video cache memory for 2D objects\n"
		"\n"
		"MP4Client - GPAC command line player - version %s\n"
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
	fprintf(stderr, "%02d:%02d:%02d.%03d", h, m, s, ms);
}


static u32 rti_update_time_ms = 200;
static FILE *rti_logs = NULL;

static void UpdateRTInfo(const char *legend)
{
	GF_SystemRTInfo rti;

	/*refresh every second*/
	if (!display_rti && !rti_logs) return;
	if (!gf_sys_get_rti(rti_update_time_ms, &rti, 0) && !legend) 
		return;

	if (display_rti) {
		char szMsg[1024];

		if (rti.total_cpu_usage) {
			sprintf(szMsg, "FPS %02.2f - CPU %02d (%02d) - Mem %d kB", 
					gf_term_get_framerate(term, 0), rti.total_cpu_usage, rti.process_cpu_usage, (u32) (rti.gpac_memory / 1024) );
		} else {
			sprintf(szMsg, "FPS %02.2f - CPU %02d - Mem %d kB", 
				gf_term_get_framerate(term, 0), rti.process_cpu_usage, (u32) (rti.gpac_memory / 1024) );
		}
		
		if (display_rti==2) {
			fprintf(stderr, "%s\r", szMsg); 
		} else {
			GF_Event evt;
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
			if (com.artist) { strcat(szName, com.artist); strcat(szName, " "); }
			if (com.name) { strcat(szName, com.name); strcat(szName, " "); }
			if (com.album) { strcat(szName, "("); strcat(szName, com.album); strcat(szName, ")"); }
			
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
	res = (val)&0xFF; res<<=8;
	res |= (val>>8)&0xFF; res<<=8;
	res |= (val>>16)&0xFF;
	return res;
}
#endif

void switch_bench(Bool is_on)
{
	bench_mode = is_on;
	display_rti = is_on ;
	ResetCaption();
	gf_term_set_option(term, GF_OPT_VIDEO_BENCH, bench_mode ? 1 : 0);
}

#ifndef WIN32
#include <termios.h>
int getch() {
    struct termios old;
    struct termios new;
    int rc;
    if (tcgetattr(0, &old) == -1) {
        return -1;
    }
    new = old;
    new.c_lflag &= ~(ICANON | ECHO);
    new.c_cc[VMIN] = 1;
    new.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &new) == -1) {
        return -1;
    }
    rc = getchar();
    (void) tcsetattr(0, TCSANOW, &old);
    return rc;
}
#else
int getch(){
    return getchar();
}
#endif

/**
 * Reads a line of input from stdin
 * @param line the buffer to fill
 * @param
 */
static const char * read_line_input(char * line, int maxSize, Bool showContent){
    char read;
    int i = 0;
    if (fflush( stderr ))
      perror("Failed to flush buffer %s");
    do {
      line[i] = '\0';
      if (i >= maxSize - 1)
	return line;
      read = getch();
      if (read == 8 || read == 127){
	if (i > 0){
	  fprintf(stderr, "\b \b");
	  i--;
	}
      } else if (read > 32){
	fputc(showContent ? read : '*', stderr);
	line[i++] = read;
      }
      fflush(stderr);
    } while (read != '\n');
    if (!read)
      return 0;
    return line;
}

Bool GPAC_EventProc(void *ptr, GF_Event *evt)
{
	if (!term) return 0;
	
	if (gui_mode==1) {
		if (evt->type==GF_EVENT_QUIT) Run = 0;
		return 0;
	}

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
			servName = "";
		} else if (!strnicmp(evt->message.service, "data:", 5)) {
			servName = "(embedded data)";
		} else {
			servName = evt->message.service;
		}

		if (!evt->message.message) return 0;

		if (evt->message.error) {
			if (!is_connected) last_error = evt->message.error;
			if (evt->message.error==GF_SCRIPT_INFO) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONSOLE, ("%s\n", evt->message.message));
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONSOLE, ("%s %s: %s\n", servName, evt->message.message, gf_error_to_string(evt->message.error)));
			}
		} else if (!be_quiet) 
			GF_LOG(GF_LOG_INFO, GF_LOG_CONSOLE, ("%s %s\n", servName, evt->message.message));
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
			if (evt->key.flags & GF_KEY_MOD_CTRL) switch_bench(!bench_mode);
			break;
		}
		break;
	case GF_EVENT_KEYDOWN:
		gf_term_process_shortcut(term, evt);
		switch (evt->key.key_code) {
		case GF_KEY_SPACE:
			if (evt->key.flags & GF_KEY_MOD_CTRL) {
				/*ignore key repeat*/
				if (!bench_mode) switch_bench(!bench_mode);
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
		case GF_KEY_B:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected)
				ViewODs(term, 1);
			break;
		case GF_KEY_M:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected)
				ViewODs(term, 0);
			break;
		case GF_KEY_H:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected)
				gf_term_switch_quality(term, 1);
			break;
		case GF_KEY_L:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected)
				gf_term_switch_quality(term, 0);
			break;
		case GF_KEY_F5:
			if (is_connected)
				reload = 1;
			break;
		}
		break;

	case GF_EVENT_CONNECT:
		if (evt->connect.is_connected) {
			is_connected = 1;
			fprintf(stderr, "Service Connected\n");
			eos_seen = GF_FALSE;
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
		eos_seen = GF_TRUE;
		if (!playlist && loop_at_end) restart = 1;
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

	case GF_EVENT_OPENFILE:
	{
		u32 i, pos;
		/*todo - force playlist mode*/
		if (readonly_playlist) {
			fclose(playlist);
			playlist = NULL;
		}
		readonly_playlist = 0;
		if (!playlist) {
			readonly_playlist = 0;
			playlist = gf_temp_file_new();
		}
		pos = ftell(playlist);
		i=0;
		while (i<evt->open_file.nb_files) {
			if (evt->open_file.files[i] != NULL) {
				fprintf(playlist, "%s\n", evt->open_file.files[i]); 
			}
			i++;
		}
		fseek(playlist, pos, SEEK_SET);
		request_next_playlist_item = 1;
	}
		return 1;

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
	{
		int maxTries = 1;
		assert( evt->type == GF_EVENT_AUTHORIZATION);
		assert( evt->auth.user);
		assert( evt->auth.password);
		assert( evt->auth.site_url);
		while ((!strlen(evt->auth.user) || !strlen(evt->auth.password)) && (maxTries--) >= 0){
			fprintf(stderr, "**** Authorization required for site %s ****\n", evt->auth.site_url);
			fprintf(stderr, "login   : ");
			read_line_input(evt->auth.user, 50, 1);
			fprintf(stderr, "\npassword: ");
			read_line_input(evt->auth.password, 50, 0);
			fprintf(stderr, "*********\n");
		}
		if (maxTries < 0){
		  fprintf(stderr, "**** No User or password has been filled, aborting ***\n");
		  return 0;
		}
		return 1;
	}

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
	char nav;
	u32 type = gf_term_get_option(term, GF_OPT_NAVIGATION_TYPE);
	e = GF_OK;
	fflush(stdin);

	if (!type) {
		fprintf(stderr, "Content/compositor doesn't allow user-selectable navigation\n");
	} else if (type==1) {
		fprintf(stderr, "Select Navigation (\'N\'one, \'E\'xamine, \'S\'lide): ");
		nav = getch();
		if (nav=='N') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_NONE);
		else if (nav=='E') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_EXAMINE);
		else if (nav=='S') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_SLIDE);
		else fprintf(stderr, "Unknown selector \'%c\' - only \'N\',\'E\',\'S\' allowed\n", nav);
	} else if (type==2) {
		fprintf(stderr, "Select Navigation (\'N\'one, \'W\'alk, \'F\'ly, \'E\'xamine, \'P\'an, \'S\'lide, \'G\'ame, \'V\'R, \'O\'rbit): ");
		nav = getch();
		if (nav=='N') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_NONE);
		else if (nav=='W') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_WALK);
		else if (nav=='F') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_FLY);
		else if (nav=='E') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_EXAMINE);
		else if (nav=='P') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_PAN);
		else if (nav=='S') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_SLIDE);
		else if (nav=='G') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_GAME);
		else if (nav=='O') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_ORBIT);
		else if (nav=='V') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_VR);
		else fprintf(stderr, "Unknown selector %c - only \'N\',\'W\',\'F\',\'E\',\'P\',\'S\',\'G\', \'V\', \'O\' allowed\n", nav);
	}
	if (e) fprintf(stderr, "Error setting mode: %s\n", gf_error_to_string(e));
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
		if (sscanf(arg, "%02ud:%02ud:%02ud;%02ud/%02ud", &h, &m, &s, &f, &fps)==5) {
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
		if (strchr(arg, ':') && (sscanf(arg, "%02ud:%02ud:%02ud:%02ud", &h, &m, &s, &ms)==4)) {
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
		if (log_utc_time) {
			u64 utc_clock = gf_net_get_utc() ;
			time_t secs = utc_clock/1000;
			struct tm t;
			t = *gmtime(&secs);
			fprintf(logs, "[UTC %d-%02d-%02dT%02d:%02d:%02dZ - TS "LLU"]", 1900+t.tm_year, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, utc_clock);
		}
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
			gf_log_set_tool_level(GF_LOG_RTI, GF_LOG_DEBUG);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_RTI, ("[RTI] System state when enabling log\n"));
		} else if (log_time_start) {
			log_time_start = gf_sys_clock();
		}
	}
}

void set_cfg_option(char *opt_string)
{
	char *sep, *sep2, szSec[1024], szKey[1024], szVal[1024];
	sep = strchr(opt_string, ':');
	if (!sep) {
		fprintf(stderr, "Badly formatted option %s - expected Section:Name=Value\n", opt_string);
		return;
	}
	sep[0] = 0;
	strcpy(szSec, opt_string);
	sep[0] = ':'; sep ++;
	sep2 = strchr(sep, '=');
	if (!sep2) {
		fprintf(stderr, "Badly formatted option %s - expected Section:Name=Value\n", opt_string);
		return;
	}
	sep2[0] = 0;
	strcpy(szKey, sep);
	strcpy(szVal, sep2+1);
	sep2[0] = '='; 
	if (!stricmp(szVal, "null")) szVal[0]=0;
	gf_cfg_set_key(cfg_file, szSec, szKey, szVal[0] ? szVal : NULL);
}

#ifdef WIN32
#include <wincon.h>
#endif

int main (int argc, char **argv)
{
	char c;
	const char *str;
	u32 i, times[100], nb_times, dump_mode;
	u32 simulation_time_in_ms = 0;
	Bool auto_exit = GF_FALSE;
	Bool logs_set = GF_FALSE;
	Bool start_fs = GF_FALSE;
	Bool use_rtix = GF_FALSE;
	Bool rgbds_dump = GF_FALSE;
	Bool rgbd_dump = GF_FALSE;
	Bool depth_dump = GF_FALSE;
	Bool pause_at_first = GF_FALSE;
#ifdef GPAC_MEMORY_TRACKING
	Bool enable_mem_tracker = GF_FALSE;
#endif
	Double fps = GF_IMPORT_DEFAULT_FPS;
	Bool fill_ar, visible;
	char *url_arg, *the_cfg, *rti_file, *views;
	FILE *logfile = NULL;
	Float scale = 1;
#ifndef WIN32
	dlopen(NULL, RTLD_NOW|RTLD_GLOBAL);
#endif

	/*by default use current dir*/
	strcpy(the_url, ".");

	memset(&user, 0, sizeof(GF_User));

	dump_mode = 0;
	fill_ar = visible = GF_FALSE;
	url_arg = the_cfg = rti_file = views = NULL;
	nb_times = 0;
	times[0] = 0;

	/*first locate config file if specified*/
	for (i=1; i<(u32) argc; i++) {
		char *arg = argv[i];
		if (!strcmp(arg, "-c") || !strcmp(arg, "-cfg")) {
			the_cfg = argv[i+1];
			i++;
		}
		else if (!strcmp(arg, "-mem-track")) {
#ifdef GPAC_MEMORY_TRACKING
			enable_mem_tracker = GF_TRUE;
#else
			fprintf(stderr, "WARNING - GPAC not compiled with Memory Tracker - ignoring \"-mem-track\"\n"); 
#endif
		}
		else if (!strcmp(arg, "-h") || !strcmp(arg, "-help")) {
			PrintUsage();
			return 1;
		}
	}

#ifdef GPAC_MEMORY_TRACKING
	gf_sys_init(enable_mem_tracker);
#else
	gf_sys_init(GF_FALSE);
#endif

	cfg_file = gf_cfg_init(the_cfg, NULL);
	if (!cfg_file) {
		fprintf(stderr, "Error: Configuration File not found\n");
		return 1;
	}
	/*if logs are specified, use them*/
	if (gf_log_set_tools_levels( gf_cfg_get_key(cfg_file, "General", "Logs") ) != GF_OK) {
		return 1;
	}

	if( gf_cfg_get_key(cfg_file, "General", "Logs") != NULL ){
		logs_set = GF_TRUE;
	}

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
			use_rtix = GF_TRUE;
		} else if (!strcmp(arg, "-fill")) {
			fill_ar = GF_TRUE;
		} else if (!strcmp(arg, "-gui")) {
			gui_mode = 1;
		} else if (!strcmp(arg, "-guid")) {
			gui_mode = 2;
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
		} else if (!strcmp(arg, "-png")) {
			dump_mode=11;
			if ((url_arg || (i+2<(u32)argc)) && get_time_list(argv[i+1], times, &nb_times)) i++;
		} else if (!strcmp(arg, "-raw")) {
			dump_mode = 3;
			if ((url_arg || (i+2<(u32)argc)) && get_time_list(argv[i+1], times, &nb_times)) i++;

		} else if (!stricmp(arg, "-size")) {
			/*usage of %ud breaks sscanf on MSVC*/
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
		} else if (!strcmp(arg, "-strict-error")) {
			gf_log_set_strict_error(1);
		} else if (!strcmp(arg, "-log-file") || !strcmp(arg, "-lf")) {
			logfile = gf_f64_open(argv[i+1], "wt");
			gf_log_set_callback(logfile, on_gpac_log);
			i++;
		} else if (!strcmp(arg, "-logs") ) {
			if (gf_log_set_tools_levels(argv[i+1]) != GF_OK) {
				return 1;
			}
			logs_set = GF_TRUE;
			i++;
		} else if (!strcmp(arg, "-log-clock") || !strcmp(arg, "-lc")) {
			log_time_start = 1;
		} else if (!strcmp(arg, "-log-utc") || !strcmp(arg, "-lu")) {
			log_utc_time = 1;
		} else if (!strcmp(arg, "-align")) {
			if (argv[i+1][0]=='m') align_mode = 1;
			else if (argv[i+1][0]=='b') align_mode = 2;
			align_mode <<= 8;
			if (argv[i+1][1]=='m') align_mode |= 1;
			else if (argv[i+1][1]=='r') align_mode |= 2;
			i++;
		}
		else if (!strcmp(arg, "-no-wnd")) user.init_flags |= GF_TERM_WINDOWLESS;
		else if (!strcmp(arg, "-no-back")) user.init_flags |= GF_TERM_WINDOW_TRANSPARENT;
#if defined(__DARWIN__) || defined(__APPLE__)
		else if (!strcmp(arg, "-thread")) threading_flags = 0;
#else
		else if (!strcmp(arg, "-no-thread")) threading_flags = GF_TERM_NO_DECODER_THREAD | GF_TERM_NO_COMPOSITOR_THREAD | GF_TERM_WINDOW_NO_THREAD;
#endif
		else if (!strcmp(arg, "-no-audio")) no_audio = 1;
		else if (!strcmp(arg, "-no-regulation")) no_regulation = 1;
		else if (!strcmp(arg, "-fs")) start_fs = 1;
		else if (!strcmp(arg, "-pause")) pause_at_first = 1;
		else if (!strcmp(arg, "-exit")) auto_exit = 1;
		else if (!strcmp(arg, "-mem-track")) {
#ifdef GPAC_MEMORY_TRACKING
			enable_mem_tracker = 1;
#else
			fprintf(stderr, "WARNING - GPAC not compiled with Memory Tracker - ignoring \"-mem-track\"\n"); 
#endif
		}
		else if (!strcmp(arg, "-loop")) loop_at_end = 1;
		else if (!strcmp(arg, "-bench")) bench_mode = 1;
		else if (!strcmp(arg, "-opt")) {
			set_cfg_option(argv[i+1]);
			i++;
		}
		else if (!strcmp(arg, "-ifce")) {
			gf_cfg_set_key(cfg_file, "Network", "DefaultMCastInterface", argv[i+1]);
			i++;
		}
		else if (!stricmp(arg, "-views")) {
			views = argv[i+1];
			i++;
		}
		else if (!stricmp(arg, "-run-for")) {
			simulation_time_in_ms = atoi(argv[i+1]) * 1000;
			if (!simulation_time_in_ms)
				simulation_time_in_ms = 1; /*1ms*/
			i++;
		}
		else if (!stricmp(arg, "-help")) {
			PrintUsage();
			return 1;
		} else { 
			fprintf(stderr, "Unrecognized option %s - skipping\n", arg);
		}
	}
	if (dump_mode && !url_arg ) {
		fprintf(stderr, "Missing argument for dump\n");
		PrintUsage();
		if (logfile) fclose(logfile);
		return 1;
	}

	if (!url_arg && simulation_time_in_ms)
		simulation_time_in_ms += gf_sys_clock();

	if (!gui_mode) {
		str = gf_cfg_get_key(cfg_file, "General", "ForceGUI");
		if (str && !strcmp(str, "yes")) gui_mode = 1;
	}

	if (gui_mode) {
		threading_flags = GF_TERM_NO_DECODER_THREAD | GF_TERM_NO_COMPOSITOR_THREAD;
		if (gui_mode==1) {
			hide_shell(1);
			user.init_flags |= GF_TERM_WINDOW_NO_DECORATION;
		}
	}


	if (dump_mode) rti_file = NULL;

	if (!logs_set) {
		//gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_ERROR);
	}

	if (rti_file) init_rti_logs(rti_file, url_arg, use_rtix);

	{
		GF_SystemRTInfo rti;
		gf_sys_get_rti(0, &rti, 0);
		fprintf(stderr, "System info: %d MB RAM - %d cores\n", (u32) (rti.physical_memory/1024/1024), rti.nb_cores);
	} 


	/*setup dumping options*/
	if (dump_mode) {
		user.init_flags |= GF_TERM_NO_AUDIO | GF_TERM_NO_DECODER_THREAD | GF_TERM_NO_COMPOSITOR_THREAD | GF_TERM_NO_REGULATION /*| GF_TERM_INIT_HIDE*/;
		if (visible || dump_mode==8) user.init_flags |= GF_TERM_INIT_HIDE;
	} else {
		init_w = forced_width;
		init_h = forced_height;
	}

	user.modules = gf_modules_new(NULL, cfg_file);
	if (user.modules) i = gf_modules_get_count(user.modules);
	if (!i || !user.modules) {
		fprintf(stderr, "Error: no modules found - exiting\n");
		if (user.modules) gf_modules_del(user.modules);
		gf_cfg_del(cfg_file);
		gf_sys_close();
		if (logfile) fclose(logfile);
		return 1;
	}
	fprintf(stderr, "Modules Found : %d \n", i);

	user.config = cfg_file;
	user.EventProc = GPAC_EventProc;
	/*dummy in this case (global vars) but MUST be non-NULL*/
	user.opaque = user.modules;
	if (threading_flags) user.init_flags |= threading_flags;
	if (no_audio) user.init_flags |= GF_TERM_NO_AUDIO;
	if (no_regulation) user.init_flags |= GF_TERM_NO_REGULATION;

	if (threading_flags & (GF_TERM_NO_DECODER_THREAD|GF_TERM_NO_COMPOSITOR_THREAD) ) term_step = 1;

	fprintf(stderr, "Loading GPAC Terminal\n");	
	i = gf_sys_clock();
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
	fprintf(stderr, "Terminal Loaded in %d ms\n", gf_sys_clock()-i);

	if (bench_mode) {
		switch_bench(1);
	}

	if (dump_mode) {
//		gf_term_set_option(term, GF_OPT_VISIBLE, 0);
		if (fill_ar) gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_FILL_SCREEN);
	} else {
		/*check video output*/
		str = gf_cfg_get_key(cfg_file, "Video", "DriverName");
		if (!strcmp(str, "Raw Video Output")) fprintf(stderr, "WARNING: using raw output video (memory only) - no display used\n");
		/*check audio output*/
		str = gf_cfg_get_key(cfg_file, "Audio", "DriverName");
		if (!str || !strcmp(str, "No Audio Output Available")) fprintf(stderr, "WARNING: no audio output available - make sure no other program is locking the sound card\n");

		str = gf_cfg_get_key(cfg_file, "General", "NoMIMETypeFetch");
		no_mime_check = (str && !stricmp(str, "yes")) ? 1 : 0;
	}

	str = gf_cfg_get_key(cfg_file, "HTTPProxy", "Enabled");
	if (str && !strcmp(str, "yes")) {
		str = gf_cfg_get_key(cfg_file, "HTTPProxy", "Name");
		if (str) fprintf(stderr, "HTTP Proxy %s enabled\n", str);
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

	if (dump_mode) {
		if (!nb_times) {
			times[0] = 0;
			nb_times++;
		}
		dump_file(url_arg, dump_mode, fps, forced_width, forced_height, scale, times, nb_times);
		Run = 0;
	} else

	/*connect if requested*/
	if (views) {
	} else if (!gui_mode && url_arg) {
		char *ext;

		strcpy(the_url, url_arg);
		ext = strrchr(the_url, '.');
		if (ext && (!stricmp(ext, ".m3u") || !stricmp(ext, ".pls"))) {
			GF_Err e = GF_OK;
			fprintf(stderr, "Opening Playlist %s\n", the_url);
	
			strcpy(pl_path, the_url);
			/*this is not clean, we need to have a plugin handle playlist for ourselves*/
			if (!strncmp("http:", the_url, 5)) {
				GF_DownloadSession *sess = gf_dm_sess_new(term->downloader, the_url, GF_NETIO_SESSION_NOT_THREADED, NULL, NULL, &e);
				if (sess) {
					e = gf_dm_sess_process(sess);
					if (!e) strcpy(the_url, gf_dm_sess_get_cache_name(sess));
					gf_dm_sess_del(sess);
				}
			}
			
			playlist = e ? NULL : gf_f64_open(the_url, "rt");
			readonly_playlist = 1;
			if (playlist) {
				if (1 > fscanf(playlist, "%s", the_url))
				  fprintf(stderr, "Cannot read any URL from playlist\n");
				else {
				  fprintf(stderr, "Opening URL %s\n", the_url);
				  gf_term_connect_with_path(term, the_url, pl_path);
				}
			} else {
				if (e) 
					fprintf(stderr, "Failed to open playlist %s: %s\n", the_url, gf_error_to_string(e) );
				fprintf(stderr, "Hit 'h' for help\n\n");
			}
		} else {
			fprintf(stderr, "Opening URL %s\n", the_url);
			if (pause_at_first) fprintf(stderr, "[Status: Paused]\n");
			gf_term_connect_from_time(term, the_url, 0, pause_at_first);
		}
	} else {
		fprintf(stderr, "Hit 'h' for help\n\n");
		str = gf_cfg_get_key(cfg_file, "General", "StartupFile");
		if (str) {
			strcpy(the_url, "MP4Client "GPAC_FULL_VERSION);
			gf_term_connect(term, str);
			startup_file = 1;
		}
		if (url_arg) {
			gf_cfg_set_key(cfg_file, "Temp", "GUIStartupFile", url_arg);
		}
	}
	if (gui_mode==2) gui_mode=0;

	if (start_fs) gf_term_set_option(term, GF_OPT_FULLSCREEN, 1);

	if (views) {
		char szTemp[4046];
		sprintf(szTemp, "views://%s", views);
		gf_term_connect(term, szTemp);
	}

	while (Run) {		
		/*we don't want getchar to block*/
		if (gui_mode || !gf_prompt_has_input()) {
			if (reload) {
				reload = 0;
				gf_term_disconnect(term);
				gf_term_connect(term, startup_file ? gf_cfg_get_key(cfg_file, "General", "StartupFile") : the_url);
			}
			if (restart) {
				restart = 0;
				gf_term_play_from_time(term, 0, 0);
			}
			if (request_next_playlist_item) {
				c = '\n';
				request_next_playlist_item = 0;
				goto force_input;
			}
			if (!use_rtix || display_rti) UpdateRTInfo(NULL);
			if (term_step) {
				gf_term_process_step(term);
			} else {
				gf_sleep(rti_update_time_ms);
			}
			if (auto_exit && eos_seen && gf_term_get_option(term, GF_OPT_IS_OVER)) {
				Run = 0;
			} 

			/*sim time*/
			if (simulation_time_in_ms 
				&& ( (gf_term_get_time_in_ms(term)>simulation_time_in_ms) || (!url_arg && gf_sys_clock()>simulation_time_in_ms))
			) {
				Run = 0;
			}
			continue;
		}
		c = gf_prompt_get_char();

force_input:
		switch (c) {
		case 'q':
			Run = 0;
			break;
		case 'X':
			exit(0);
			break;
		case 'Q':
			break;
		case 'o':
			startup_file = 0;
			gf_term_disconnect(term);
			fprintf(stderr, "Enter the absolute URL\n");
			if (1 > scanf("%s", the_url)){
			    fprintf(stderr, "Cannot read absolute URL, aborting\n");
			    break;
			}
			if (rti_file) init_rti_logs(rti_file, the_url, use_rtix);
			gf_term_connect(term, the_url);
			break;
		case 'O':
			gf_term_disconnect(term);
			fprintf(stderr, "Enter the absolute URL to the playlist\n");
			if (1 > scanf("%s", the_url)){
			    fprintf(stderr, "Cannot read the absolute URL, aborting.\n");
			    break;
			}
			playlist = gf_f64_open(the_url, "rt");
			if (playlist) {
				if (1 >	fscanf(playlist, "%s", the_url)){
				    fprintf(stderr, "Cannot read any URL from playlist, aborting.\n");
				    fclose( playlist);
				    break;
				}
				fprintf(stderr, "Opening URL %s\n", the_url);
				gf_term_connect(term, the_url);
			}
			break;
		case '\n':
		case 'N':
			if (playlist) {
				int res;
				gf_term_disconnect(term);

				res = fscanf(playlist, "%s", the_url);
				if ((res == EOF) && loop_at_end) {
					fseek(playlist, 0, SEEK_SET);
					res = fscanf(playlist, "%s", the_url);
				}
				if (res == EOF) {
					fprintf(stderr, "No more items - exiting\n");
					Run = 0;
				} else {
					fprintf(stderr, "Opening URL %s\n", the_url);
					gf_term_connect_with_path(term, the_url, pl_path);
				}
			}
			break;
		case 'P':
			if (playlist) {
				u32 count;
				gf_term_disconnect(term);
				if (1 > scanf("%u", &count)){
				  fprintf(stderr, "Cannot read number, aborting.\n");
				  break;
				}
				while (count) {
					if (fscanf(playlist, "%s", the_url)){
					    fprintf(stderr, "Failed to read line, aborting\n");
					    break;
					}
					count--;
				}
				fprintf(stderr, "Opening URL %s\n", the_url);
				gf_term_connect(term, the_url);
			}
			break;
		case 'r':
			if (is_connected) 
				reload = 1;
			break;
		
		case 'D':
			if (is_connected) gf_term_disconnect(term);
			break;

		case 'p':
			if (is_connected) {
				Bool is_pause = gf_term_get_option(term, GF_OPT_PLAY_STATE);
				fprintf(stderr, "[Status: %s]\n", is_pause ? "Playing" : "Paused");
				gf_term_set_option(term, GF_OPT_PLAY_STATE, is_pause ? GF_STATE_PLAYING : GF_STATE_PAUSED);
			}
			break;
		case 's':
			if (is_connected) {
				gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
				fprintf(stderr, "Step time: ");
				PrintTime(gf_term_get_time_in_ms(term));
				fprintf(stderr, "\n");
			}
			break;

		case 'z':
		case 'T':
			if (!CanSeek || (Duration<=2000)) {
				fprintf(stderr, "scene not seekable\n");
			} else {
				Double res;
				s32 seekTo;
				fprintf(stderr, "Duration: ");
				PrintTime(Duration);
				res = gf_term_get_time_in_ms(term);
				if (c=='z') {
					res *= 100; res /= (s64)Duration;
					fprintf(stderr, " (current %.2f %%)\nEnter Seek percentage:\n", res);
					if (scanf("%d", &seekTo) == 1) { 
						if (seekTo > 100) seekTo = 100;
						res = (Double)(s64)Duration; res /= 100; res *= seekTo;
						gf_term_play_from_time(term, (u64) (s64) res, 0);
					}
				} else {
					u32 r, h, m, s;
					fprintf(stderr, " - Current Time: ");
					PrintTime((u64) res);
					fprintf(stderr, "\nEnter seek time (Format: s, m:s or h:m:s):\n");
					h = m = s = 0;
					r =scanf("%d:%d:%d", &h, &m, &s);
					if (r==2) { s = m; m = h; h = 0; }
					else if (r==1) { s = h; m = h = 0;	}

					if (r && (r<=3)) {
						u64 time = h*3600 + m*60 + s;
						gf_term_play_from_time(term, time*1000, 0);
					}
				}
			}
			break;

		case 't':
		{
			if (is_connected) {
				fprintf(stderr, "Current Time: ");
				PrintTime(gf_term_get_time_in_ms(term));
				fprintf(stderr, " - Duration: ");
				PrintTime(Duration);
				fprintf(stderr, "\n");
			}
		}
			break;
		case 'w':
			if (is_connected) PrintWorldInfo(term);
			break;
		case 'v':
			if (is_connected) PrintODList(term, NULL, 0, 0, "Root");
			break;
		case 'i':
			if (is_connected) {
				u32 ID;
				do {
				  fprintf(stderr, "Enter OD ID (0 for main OD): ");
				  fflush(stderr);
				} while( 1 > scanf("%ud", &ID));
				ViewOD(term, ID, (u32)-1);
			}
			break;
		case 'j':
			if (is_connected) {
				u32 num;
				do {
				  fprintf(stderr, "Enter OD number (0 for main OD): ");
				  fflush(stderr);
				} while( 1 > scanf("%ud", &num));
				ViewOD(term, (u32)-1, num);
			}
			break;
		case 'b':
			if (is_connected) ViewODs(term, 1);
			break;

		case 'm':
			if (is_connected) ViewODs(term, 0);
			break;

		case 'l':
			list_modules(user.modules);
			break;

		case 'n':
			if (is_connected) set_navigation();
			break;
		case 'x':
			if (is_connected) gf_term_set_option(term, GF_OPT_NAVIGATION_TYPE, 0);
			break;

		case 'd':
			if (is_connected) {
				GF_ObjectManager *odm = NULL;
				char radname[GF_MAX_PATH], *sExt;
				GF_Err e;
				u32 i, count, odid;
				Bool xml_dump, std_out;
				radname[0] = 0;
				do {
				  fprintf(stderr, "Enter Inline OD ID if any or 0 : ");
				  fflush(stderr);
				} while( 1 >  scanf("%ud", &odid));
				if (odid) {
					GF_ObjectManager *root_odm = gf_term_get_root_object(term);
					if (!root_odm) break;
					count = gf_term_get_object_count(term, root_odm);
					for (i=0; i<count; i++) {
						GF_MediaInfo info;
						odm = gf_term_get_object(term, root_odm, i);
						if (gf_term_get_object_info(term, odm, &info) == GF_OK) {
							if (info.od->objectDescriptorID==odid) break;
						}
						odm = NULL;
					}
				}
				do{
				  fprintf(stderr, "Enter file radical name (+\'.x\' for XML dumping) - \"std\" for stderr: ");
				  fflush(stderr);
				} while( 1 > scanf("%s", radname));
				sExt = strrchr(radname, '.');
				xml_dump = 0;
				if (sExt) {
					if (!stricmp(sExt, ".x")) xml_dump = 1;
					sExt[0] = 0;
				}
				std_out = strnicmp(radname, "std", 3) ? 0 : 1;
				e = gf_term_dump_scene(term, std_out ? NULL : radname, NULL, xml_dump, 0, odm);
				fprintf(stderr, "Dump done (%s)\n", gf_error_to_string(e));
			}
			break;

		case 'c':
			PrintGPACConfig();
			break;
		case '3':
		{
			Bool use_3d = !gf_term_get_option(term, GF_OPT_USE_OPENGL);
			if (gf_term_set_option(term, GF_OPT_USE_OPENGL, use_3d)==GF_OK) {
				fprintf(stderr, "Using %s for 2D drawing\n", use_3d ? "OpenGL" : "2D rasterizer");
			}
		}
			break;
		case 'k':
		{
			Bool opt = gf_term_get_option(term, GF_OPT_STRESS_MODE);
			opt = !opt;
			fprintf(stderr, "Turning stress mode %s\n", opt ? "on" : "off");
			gf_term_set_option(term, GF_OPT_STRESS_MODE, opt);
		}
			break;
		case '4': gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_4_3); break;
		case '5': gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_16_9); break;
		case '6': gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_FILL_SCREEN); break;
		case '7': gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_KEEP); break;

		case 'C':
			switch (gf_term_get_option(term, GF_OPT_MEDIA_CACHE)) {
			case GF_MEDIA_CACHE_DISABLED: gf_term_set_option(term, GF_OPT_MEDIA_CACHE, GF_MEDIA_CACHE_ENABLED); break;
			case GF_MEDIA_CACHE_ENABLED: gf_term_set_option(term, GF_OPT_MEDIA_CACHE, GF_MEDIA_CACHE_DISABLED); break;
			case GF_MEDIA_CACHE_RUNNING: fprintf(stderr, "Streaming Cache is running - please stop it first\n"); continue;
			}
			switch (gf_term_get_option(term, GF_OPT_MEDIA_CACHE)) {
			case GF_MEDIA_CACHE_ENABLED: fprintf(stderr, "Streaming Cache Enabled\n"); break;
			case GF_MEDIA_CACHE_DISABLED: fprintf(stderr, "Streaming Cache Disabled\n"); break;
			case GF_MEDIA_CACHE_RUNNING: fprintf(stderr, "Streaming Cache Running\n"); break;
			}
			break;
		case 'S':
		case 'A':
			if (gf_term_get_option(term, GF_OPT_MEDIA_CACHE)==GF_MEDIA_CACHE_RUNNING) {
				gf_term_set_option(term, GF_OPT_MEDIA_CACHE, (c=='S') ? GF_MEDIA_CACHE_DISABLED : GF_MEDIA_CACHE_DISCARD);
				fprintf(stderr, "Streaming Cache stopped\n");
			} else {
				fprintf(stderr, "Streaming Cache not running\n");
			}
			break;
		case 'R':
			display_rti = !display_rti;
			ResetCaption();
			break;
		case 'F':
			if (display_rti) display_rti = 0;
			else display_rti = 2;
			ResetCaption();
			break;

		case 'u':
		{
			GF_Err e;
			char szCom[8192];
			fprintf(stderr, "Enter command to send:\n");
			fflush(stdin);
			szCom[0] = 0;
			if (1 > scanf("%[^\t\n]", szCom)){
			    fprintf(stderr, "Cannot read command to send, aborting.\n");
			    break;
			}
			e = gf_term_scene_update(term, NULL, szCom);
			if (e) fprintf(stderr, "Processing command failed: %s\n", gf_error_to_string(e));
		}
			break;

		case 'L':
		{
			char szLog[1024], *cur_logs;
			cur_logs = gf_log_get_tools_levels();
			fprintf(stderr, "Enter new log level (current tools %s):\n", cur_logs);
			gf_free(cur_logs);
			if (scanf("%s", szLog) < 1) {
			    fprintf(stderr, "Cannot read new log level, aborting.\n");
			    break;
			}
			gf_log_modify_tools_levels(szLog);
		}
			break;

		case 'g':
		{
			GF_SystemRTInfo rti;
			gf_sys_get_rti(rti_update_time_ms, &rti, 0);
			fprintf(stderr, "GPAC allocated memory "LLD"\n", rti.gpac_memory);
		}
			break;
		case 'M':
		{
			u32 size;
			do {
				fprintf(stderr, "Enter new video cache memory in kBytes (current %ud):\n", gf_term_get_option(term, GF_OPT_VIDEO_CACHE_SIZE));
			} while (1 > scanf("%ud", &size));
			gf_term_set_option(term, GF_OPT_VIDEO_CACHE_SIZE, size);
		}
			break;

		case 'H':
		{
			u32 http_bitrate = gf_term_get_option(term, GF_OPT_HTTP_MAX_RATE);	
			do {
				fprintf(stderr, "Enter new http bitrate in bps (0 for none) - current limit: %d\n", http_bitrate);
			} while (1 > scanf("%ud", &http_bitrate));

			gf_term_set_option(term, GF_OPT_HTTP_MAX_RATE, http_bitrate);
		}
			break;

		case 'E':
			gf_term_set_option(term, GF_OPT_RELOAD_CONFIG, 1); 
			break;

		case 'B':
			switch_bench(!bench_mode);
			break;

		case 'Y':
		{
			char szOpt[8192];
			fprintf(stderr, "Enter option to set (Section:Name=Value):\n");
			fflush(stdin);
			szOpt[0] = 0;
			if (1 > scanf("%[^\t\n]", szOpt)){
			    fprintf(stderr, "Cannot read option\n");
			    break;
			}
			set_cfg_option(szOpt);
		}
			break;

		/*extract to PNG*/
		case 'Z':
		{
			char szFileName[100];
			u32 nb_pass, nb_views, offscreen_view = 0;
			GF_VideoSurface fb;
			GF_Err e;
			nb_pass = 1;
			nb_views = gf_term_get_option(term, GF_OPT_NUM_STEREO_VIEWS);
			if (nb_views>1) {
				fprintf(stderr, "Auto-stereo mode detected - type number of view to dump (0 is main output, 1 to %d offscreen view, %d for all offscreen, %d for all offscreen and main)\n", nb_views, nb_views+1, nb_views+2);
				if (scanf("%d", &offscreen_view) != 1) {
					offscreen_view = 0;
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
			while (nb_pass) {
				nb_pass--;
				if (offscreen_view) {
					sprintf(szFileName, "view%d_dump.png", offscreen_view);
					e = gf_term_get_offscreen_buffer(term, &fb, offscreen_view-1, 0);
				} else {
					sprintf(szFileName, "video_dump.png");
					e = gf_term_get_screen_buffer(term, &fb);
				}
				offscreen_view++;
				if (e) {
					fprintf(stderr, "Error dumping screen buffer %s\n", gf_error_to_string(e) );
					nb_pass = 0;
				} else {
#ifndef GPAC_DISABLE_AV_PARSERS
					u32 dst_size = fb.width*fb.height*4;
					char *dst = (char*)gf_malloc(sizeof(char)*dst_size);

					e = gf_img_png_enc(fb.video_buffer, fb.width, fb.height, fb.pitch_y, fb.pixel_format, dst, &dst_size);
					if (e) {
						fprintf(stderr, "Error encoding PNG %s\n", gf_error_to_string(e) );
						nb_pass = 0;
					} else {
						FILE *png = gf_f64_open(szFileName, "wb");
						if (!png) {
							fprintf(stderr, "Error writing file %s\n", szFileName);
							nb_pass = 0;
						} else {
							gf_fwrite(dst, dst_size, 1, png);
							fclose(png);
							fprintf(stderr, "Dump to %s\n", szFileName);
						}
					}
					if (dst) gf_free(dst);
					gf_term_release_screen_buffer(term, &fb);
#endif //GPAC_DISABLE_AV_PARSERS
				}
			}
			fprintf(stderr, "Done: %s\n", szFileName);
		}
			break;

		case 'h':
			PrintHelp();
			break;
		default:
			break;
		}
	}

	i = gf_sys_clock();
	gf_term_disconnect(term);
	if (rti_file) UpdateRTInfo("Disconnected\n");

	fprintf(stderr, "Deleting terminal... ");
	if (playlist) fclose(playlist);
	gf_term_del(term);
	fprintf(stderr, "done (in %d ms)\n", gf_sys_clock() - i);

	fprintf(stderr, "GPAC cleanup ...\n");
	gf_modules_del(user.modules);
	gf_cfg_del(cfg_file);

	gf_sys_close();
	if (rti_logs) fclose(rti_logs);
	if (logfile) fclose(logfile);

	if (gui_mode) {
		hide_shell(2);
	}
	return 0;
}


void PrintWorldInfo(GF_Terminal *term)
{
	u32 i;
	const char *title;
	GF_List *descs;
	descs = gf_list_new();
	title = gf_term_get_world_info(term, NULL, descs);
	if (!title && !gf_list_count(descs)) {
		fprintf(stderr, "No World Info available\n");
	} else {
		fprintf(stderr, "\t%s\n", title ? title : "No title available");
		for (i=0; i<gf_list_count(descs); i++) {
			char *str = gf_list_get(descs, i);
			fprintf(stderr, "%s\n", str);
		}
	}
	gf_list_del(descs);
}

void PrintODList(GF_Terminal *term, GF_ObjectManager *root_odm, u32 num, u32 indent, char *root_name)
{
	GF_MediaInfo odi;
	u32 i, count;
	char szIndent[50];
	GF_ObjectManager *odm;

	if (!root_odm) {
		fprintf(stderr, "Currently loaded objects:\n");
		root_odm = gf_term_get_root_object(term);
	}
	if (!root_odm) return;

	count = gf_term_get_current_service_id(term);
	if (count) 
		fprintf(stderr, "Current service ID %d\n", count);

	if (gf_term_get_object_info(term, root_odm, &odi) != GF_OK) return;
	if (!odi.od) {
		fprintf(stderr, "Service not attached\n");
		return;
	}

	for (i=0;i<indent;i++) szIndent[i]=' ';
	szIndent[indent]=0;
	
	fprintf(stderr, "%s", szIndent);
	fprintf(stderr, "#%d %s - ", num, root_name);
	if (odi.od->ServiceID) fprintf(stderr, "Service ID %d ", odi.od->ServiceID);
	if (odi.media_url) {
		fprintf(stderr, "%s\n", odi.media_url);
	} else {
		fprintf(stderr, "OD ID %d\n", odi.od->objectDescriptorID);
	}

	szIndent[indent]=' ';
	szIndent[indent+1]=0;
	indent++;

	count = gf_term_get_object_count(term, root_odm);
	for (i=0; i<count; i++) {
		odm = gf_term_get_object(term, root_odm, i);
		if (!odm) break;
		num++;
		if (gf_term_get_object_info(term, odm, &odi) == GF_OK) {
			switch (gf_term_object_subscene_type(term, odm)) {
			case 1:
				PrintODList(term, odm, num, indent, "Root");
				break;
			case 2:
				PrintODList(term, odm, num, indent, "Inline Scene");
				break;
			case 3:
				PrintODList(term, odm, num, indent, "EXTERNPROTO Library");
				break;
			default:
				fprintf(stderr, "%s", szIndent);
				fprintf(stderr, "#%d - ", num);
				if (odi.media_url) {
					fprintf(stderr, "%s", odi.media_url);
				} else {
					fprintf(stderr, "ID %d", odi.od->objectDescriptorID);
				}
				fprintf(stderr, " - %s", (odi.od_type==GF_STREAM_VISUAL) ? "Video" : (odi.od_type==GF_STREAM_AUDIO) ? "Audio" : "Systems");
				if (odi.od && odi.od->ServiceID) fprintf(stderr, " - Service ID %d", odi.od->ServiceID);
				fprintf(stderr, "\n");
				break;
			}
		}
	}
}

void ViewOD(GF_Terminal *term, u32 OD_ID, u32 number)
{
	GF_MediaInfo odi;
	u32 i, j, count, d_enum,id;
	GF_Err e;
	NetStatCommand com;
	GF_ObjectManager *odm, *root_odm = gf_term_get_root_object(term);
	if (!root_odm) return;

	odm = NULL;
	if ((!OD_ID && (number == (u32)(-1))) ||
		((OD_ID == (u32)(-1)) && !number)) {
		odm = root_odm;
		if ((gf_term_get_object_info(term, odm, &odi) != GF_OK)) odm=NULL;
	} else {
		count = gf_term_get_object_count(term, root_odm);
		for (i=0; i<count; i++) {
			odm = gf_term_get_object(term, root_odm, i);
			if (!odm) break;
			if (gf_term_get_object_info(term, odm, &odi) == GF_OK) {
				if ((number == (u32)(-1)) && (odi.od->objectDescriptorID == OD_ID)) break;
				else if (i == (u32)(number-1)) break;
			}
			odm = NULL;
		}
	}
	if (!odm) {
		if (number == (u32)-1) fprintf(stderr, "cannot find OD with ID %d\n", OD_ID);
		else fprintf(stderr, "cannot find OD with number %d\n", number);
		return;
	}
	if (!odi.od) {
		if (number == (u32)-1) fprintf(stderr, "Object %d not attached yet\n", OD_ID);
		else fprintf(stderr, "Object #%d not attached yet\n", number);		
		return;
	}

	if (!odi.od) {
		fprintf(stderr, "Service not attached\n");
		return;
	}

	if (odi.od->tag==GF_ODF_IOD_TAG) {
		fprintf(stderr, "InitialObjectDescriptor %d\n", odi.od->objectDescriptorID);
		fprintf(stderr, "Profiles and Levels: Scene %x - Graphics %x - Visual %x - Audio %x - OD %x\n", 
			odi.scene_pl, odi.graphics_pl, odi.visual_pl, odi.audio_pl, odi.OD_pl);
		fprintf(stderr, "Inline Profile Flag %d\n", odi.inline_pl);
	} else {
		fprintf(stderr, "ObjectDescriptor %d\n", odi.od->objectDescriptorID);
	}

	fprintf(stderr, "Object Duration: ");
	if (odi.duration) {
	  PrintTime((u32) (odi.duration*1000));
	} else {
	  fprintf(stderr, "unknown");
	}
	fprintf(stderr, "\n");

	if (odi.owns_service) {
		fprintf(stderr, "Service Handler: %s\n", odi.service_handler);
		fprintf(stderr, "Service URL: %s\n", odi.service_url);
	}		
	if (odi.codec_name) {
		Float avg_dec_time;
		switch (odi.od_type) {
		case GF_STREAM_VISUAL:
			fprintf(stderr, "Video Object: Width %d - Height %d\r\n", odi.width, odi.height);
			fprintf(stderr, "Media Codec: %s\n", odi.codec_name);
			if (odi.par) fprintf(stderr, "Pixel Aspect Ratio: %d:%d\n", (odi.par>>16)&0xFF, (odi.par)&0xFF);
			break;
		case GF_STREAM_AUDIO:
			fprintf(stderr, "Audio Object: Sample Rate %d - %d channels\r\n", odi.sample_rate, odi.num_channels);
			fprintf(stderr, "Media Codec: %s\n", odi.codec_name);
			break;
		case GF_STREAM_SCENE:
		case GF_STREAM_PRIVATE_SCENE:
			if (odi.width && odi.height) {
				fprintf(stderr, "Scene Description - Width %d - Height %d\n", odi.width, odi.height);
			} else {
				fprintf(stderr, "Scene Description - no size specified\n");
			}
			fprintf(stderr, "Scene Codec: %s\n", odi.codec_name);
			break;
		case GF_STREAM_TEXT:
			if (odi.width && odi.height) {
				fprintf(stderr, "Text Object: Width %d - Height %d\n", odi.width, odi.height);
			} else {
				fprintf(stderr, "Text Object: No size specified\n");
			}
			fprintf(stderr, "Text Codec %s\n", odi.codec_name);
			break;
		}
	
		avg_dec_time = 0;
		if (odi.nb_dec_frames) { 
			avg_dec_time = (Float) odi.total_dec_time; 
			avg_dec_time /= odi.nb_dec_frames; 
		}
		fprintf(stderr, "\tBitrate over last second: %d kbps\n\tMax bitrate over one second: %d kbps\n\tAverage Decoding Time %.2f ms (%d max)\n\tTotal decoded frames %d\n", 
			(u32) odi.avg_bitrate/1024, odi.max_bitrate/1024, avg_dec_time, odi.max_dec_time, odi.nb_dec_frames);
	}
	if (odi.protection) fprintf(stderr, "Encrypted Media%s\n", (odi.protection==2) ? " NOT UNLOCKED" : "");

	count = gf_list_count(odi.od->ESDescriptors);
	fprintf(stderr, "%d streams in OD\n", count);
	for (i=0; i<count; i++) {
		GF_ESD *esd = (GF_ESD *) gf_list_get(odi.od->ESDescriptors, i);

		fprintf(stderr, "\nStream ID %d - Clock ID %d\n", esd->ESID, esd->OCRESID);
		if (esd->dependsOnESID) fprintf(stderr, "\tDepends on Stream ID %d for decoding\n", esd->dependsOnESID);

		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_OD:
			fprintf(stderr, "\tOD Stream - version %d\n", esd->decoderConfig->objectTypeIndication);
			break;
		case GF_STREAM_OCR:
			fprintf(stderr, "\tOCR Stream\n");
			break;
		case GF_STREAM_SCENE:
			fprintf(stderr, "\tScene Description Stream - version %d\n", esd->decoderConfig->objectTypeIndication);
			break;
		case GF_STREAM_VISUAL:
			fprintf(stderr, "\tVisual Stream - media type: %s", gf_esd_get_textual_description(esd));
			break;
		case GF_STREAM_AUDIO:
			fprintf(stderr, "\tAudio Stream - media type: %s", gf_esd_get_textual_description(esd));
			break;
		case GF_STREAM_MPEG7:
			fprintf(stderr, "\tMPEG-7 Stream - version %d\n", esd->decoderConfig->objectTypeIndication);
			break;
		case GF_STREAM_IPMP:
			fprintf(stderr, "\tIPMP Stream - version %d\n", esd->decoderConfig->objectTypeIndication);
			break;
		case GF_STREAM_OCI:
			fprintf(stderr, "\tOCI Stream - version %d\n", esd->decoderConfig->objectTypeIndication);
			break;
		case GF_STREAM_MPEGJ:
			fprintf(stderr, "\tMPEGJ Stream - version %d\n", esd->decoderConfig->objectTypeIndication);
			break;
		case GF_STREAM_INTERACT:
			fprintf(stderr, "\tUser Interaction Stream - version %d\n", esd->decoderConfig->objectTypeIndication);
			break;
		case GF_STREAM_TEXT:
			fprintf(stderr, "\tStreaming Text Stream - version %d\n", esd->decoderConfig->objectTypeIndication);
			break;
		default:
			fprintf(stderr, "\tUnknown Stream\n");
			break;
		}

		fprintf(stderr, "\tBuffer Size %d\n\tAverage Bitrate %d bps\n\tMaximum Bitrate %d bps\n", esd->decoderConfig->bufferSizeDB, esd->decoderConfig->avgBitrate, esd->decoderConfig->maxBitrate);
		if (esd->slConfig->predefined==SLPredef_SkipSL) {
			fprintf(stderr, "\tNot using MPEG-4 Synchronization Layer\n");
		} else {
			fprintf(stderr, "\tStream Clock Resolution %d\n", esd->slConfig->timestampResolution);
		}
		if (esd->URLString) fprintf(stderr, "\tStream Location: %s\n", esd->URLString);

		/*check language*/
		if (esd->langDesc) {
			u32 i=0;
			char lan[4], *szLang;
			lan[0] = esd->langDesc->langCode>>16;
			lan[1] = (esd->langDesc->langCode>>8)&0xFF;
			lan[2] = (esd->langDesc->langCode)&0xFF;
			lan[3] = 0;

			if ((lan[0]=='u') && (lan[1]=='n') && (lan[2]=='d')) szLang = "Undetermined";
			else {
				szLang = lan;
				while (GF_ISO639_Lang[i]) {
					if (GF_ISO639_Lang[i+2][0] && strstr(GF_ISO639_Lang[i+1], lan)) {
						szLang = (char*) GF_ISO639_Lang[i];
						break;
					}
					i+=3;
				}
			}
			fprintf(stderr, "\tStream Language: %s\n", szLang);
		}
	}
	fprintf(stderr, "\n");
	/*check OCI (not everything interests us) - FIXME: support for unicode*/
	count = gf_list_count(odi.od->OCIDescriptors);
	if (count) {
		fprintf(stderr, "%d Object Content Information descriptors in OD\n", count);
		for (i=0; i<count; i++) {
			GF_Descriptor *desc = (GF_Descriptor *) gf_list_get(odi.od->OCIDescriptors, i);
			switch (desc->tag) {
			case GF_ODF_SEGMENT_TAG:
			{
				GF_Segment *sd = (GF_Segment *) desc;
				fprintf(stderr, "Segment Descriptor: Name: %s - start time %g sec - duration %g sec\n", sd->SegmentName, sd->startTime, sd->Duration);
			}
				break;
			case GF_ODF_CC_NAME_TAG:
			{
				GF_CC_Name *ccn = (GF_CC_Name *)desc;
				fprintf(stderr, "Content Creators:\n");
				for (j=0; j<gf_list_count(ccn->ContentCreators); j++) {
					GF_ContentCreatorInfo *ci = (GF_ContentCreatorInfo *) gf_list_get(ccn->ContentCreators, j);
					if (!ci->isUTF8) continue;
					fprintf(stderr, "\t%s\n", ci->contentCreatorName);
				}
			}
				break;

			case GF_ODF_SHORT_TEXT_TAG:
				{
					GF_ShortTextual *std = (GF_ShortTextual *)desc;
					fprintf(stderr, "Description:\n\tEvent: %s\n\t%s\n", std->eventName, std->eventText);
				}
				break;
			default:
				break;
			}
		}
		fprintf(stderr, "\n");
	}

	switch (odi.status) {
	case 0: fprintf(stderr, "Stopped - "); break;
	case 1: fprintf(stderr, "Playing - "); break;
	case 2: fprintf(stderr, "Paused - "); break;
	case 3: fprintf(stderr, "Not setup yet\n"); return;
	default: fprintf(stderr, "Setup Failed\n"); return;
	}
	if (odi.buffer>=0) fprintf(stderr, "Buffer: %d ms - ", odi.buffer);
	else fprintf(stderr, "Not buffering - ");
	fprintf(stderr, "Clock drift: %d ms\n", odi.clock_drift);
	if (odi.db_unit_count) fprintf(stderr, "%d AU in DB\n", odi.db_unit_count);
	if (odi.cb_max_count) fprintf(stderr, "Composition Buffer: %d CU (%d max)\n", odi.cb_unit_count, odi.cb_max_count);
	fprintf(stderr, "\n");

	if (odi.owns_service) {
		const char *url;
		u32 done, total, bps;
		d_enum = 0;
		while (gf_term_get_download_info(term, odm, &d_enum, &url, NULL, &done, &total, &bps)) {
			if (d_enum==1) fprintf(stderr, "Current Downloads in service:\n");
			if (done && total) {
				fprintf(stderr, "%s: %d / %d bytes (%.2f %%) - %.2f kBps\n", url, done, total, (100.0f*done)/total, ((Float)bps)/1024.0f);
			} else {
				fprintf(stderr, "%s: %.2f kbps\n", url, ((Float)8*bps)/1024.0f);
			}
		}
		if (!d_enum) fprintf(stderr, "No Downloads in service\n");
		fprintf(stderr, "\n");
	}
	d_enum = 0;
	while (gf_term_get_channel_net_info(term, odm, &d_enum, &id, &com, &e)) {
		if (e) continue;
		if (!com.bw_down && !com.bw_up) continue;

		fprintf(stderr, "Stream ID %d statistics:\n", id);
		if (com.multiplex_port) {
			fprintf(stderr, "\tMultiplex Port %d - multiplex ID %d\n", com.multiplex_port, com.port);
		} else {
			fprintf(stderr, "\tPort %d\n", com.port);
		}
		fprintf(stderr, "\tPacket Loss Percentage: %.4f\n", com.pck_loss_percentage);
		fprintf(stderr, "\tDown Bandwidth: %d bps\n", com.bw_down);
		if (com.bw_up) fprintf(stderr, "\tUp Bandwidth: %d bps\n", com.bw_up);
		if (com.ctrl_port) {
			if (com.multiplex_port) {
				fprintf(stderr, "\tControl Multiplex Port: %d - Control Multiplex ID %d\n", com.multiplex_port, com.ctrl_port);
			} else {
				fprintf(stderr, "\tControl Port: %d\n", com.ctrl_port);
			}
			fprintf(stderr, "\tDown Bandwidth: %d bps\n", com.ctrl_bw_down);
			fprintf(stderr, "\tUp Bandwidth: %d bps\n", com.ctrl_bw_up);
		}
		fprintf(stderr, "\n");
	}
}

void PrintODTiming(GF_Terminal *term, GF_ObjectManager *odm)
{
	GF_MediaInfo odi;
	if (!odm) return;

	if (gf_term_get_object_info(term, odm, &odi) != GF_OK) return;
	if (!odi.od) {
		fprintf(stderr, "Service not attached\n");
		return;
	}

	fprintf(stderr, "OD %d: ", odi.od->objectDescriptorID);
	switch (odi.status) {
	case 1: fprintf(stderr, "Playing - "); break;
	case 2: fprintf(stderr, "Paused - "); break;
	default: fprintf(stderr, "Stopped - "); break;
	}
	if (odi.buffer>=0) fprintf(stderr, "Buffer: %d ms - ", odi.buffer);
	else fprintf(stderr, "Not buffering - ");
	fprintf(stderr, "Clock drift: %d ms", odi.clock_drift);
	fprintf(stderr, " - time: ");
	PrintTime((u32) (odi.current_time*1000));
	fprintf(stderr, "\n");
}

void PrintODBuffer(GF_Terminal *term, GF_ObjectManager *odm)
{
	Float avg_dec_time;
	GF_MediaInfo odi;
	if (!odm) return;

	if (gf_term_get_object_info(term, odm, &odi) != GF_OK) return;
	if (!odi.od) {
		fprintf(stderr, "Service not attached\n");
		return;
	}

	fprintf(stderr, "OD %d: ", odi.od->objectDescriptorID);
	switch (odi.status) {
	case 1: fprintf(stderr, "Playing"); break;
	case 2: fprintf(stderr, "Paused"); break;
	default: fprintf(stderr, "Stopped"); break;
	}
	if (odi.buffer>=0) fprintf(stderr, " - Buffer: %d ms", odi.buffer);
	if (odi.db_unit_count) fprintf(stderr, " - DB: %d AU", odi.db_unit_count);
	if (odi.cb_max_count) fprintf(stderr, " - CB: %d/%d CUs", odi.cb_unit_count, odi.cb_max_count);
	
	fprintf(stderr, "\n * %d decoded frames - %d dropped frames\n", odi.nb_dec_frames, odi.nb_droped);
	avg_dec_time = 0;
	if (odi.nb_dec_frames) { avg_dec_time = (Float) odi.total_dec_time; avg_dec_time /= odi.nb_dec_frames; }
	fprintf(stderr, " * Avg Bitrate %d kbps (%d max) - Avg Decoding Time %.2f ms (%d max)\n",
								(u32) odi.avg_bitrate/1024, odi.max_bitrate/1024, avg_dec_time, odi.max_dec_time);
}

void ViewODs(GF_Terminal *term, Bool show_timing)
{
	u32 i, count;
	GF_ObjectManager *odm, *root_odm = gf_term_get_root_object(term);
	if (!root_odm) return;

	if (show_timing) {
		PrintODTiming(term, root_odm);
	} else {
		PrintODBuffer(term, root_odm);
	}
	count = gf_term_get_object_count(term, root_odm);
	for (i=0; i<count; i++) {
		odm = gf_term_get_object(term, root_odm, i);
		if (show_timing) {
			PrintODTiming(term, odm);
		} else {
			PrintODBuffer(term, odm);
		}
	}
	fprintf(stderr, "\n");
}


void PrintGPACConfig()
{
	u32 i, j, cfg_count, key_count;
	char szName[200];
	char *secName = NULL;

	fprintf(stderr, "Enter section name (\"*\" for complete dump):\n");
	if (1 > scanf("%s", szName)){
	    fprintf(stderr, "No section name, aborting.\n");
	    return;
	}
	if (strcmp(szName, "*")) secName = szName;

	fprintf(stderr, "\n\n*** GPAC Configuration ***\n\n");

	cfg_count = gf_cfg_get_section_count(cfg_file);
	for (i=0; i<cfg_count; i++) {
		const char *sec = gf_cfg_get_section_name(cfg_file, i);
		if (secName) {
			if (stricmp(sec, secName)) continue;
		} else {
			if (!stricmp(sec, "General")) continue;
			if (!stricmp(sec, "MimeTypes")) continue;
			if (!stricmp(sec, "RecentFiles")) continue;
		}
		fprintf(stderr, "[%s]\n", sec);
		key_count = gf_cfg_get_key_count(cfg_file, sec);
		for (j=0; j<key_count; j++) {
			const char *key = gf_cfg_get_key_name(cfg_file, sec, j);
			const char *val = gf_cfg_get_key(cfg_file, sec, key);
			fprintf(stderr, "%s=%s\n", key, val);
		}
		fprintf(stderr, "\n");
	}
}

