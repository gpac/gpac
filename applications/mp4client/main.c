/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2019
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
#include <gpac/events.h>
#include <gpac/media_tools.h>
#include <gpac/options.h>
#include <gpac/main.h>

#include <gpac/avparse.h>
#include <gpac/network.h>
#include <gpac/utf.h>
#include <time.h>

/*ISO 639 languages*/
#include <gpac/iso639.h>


#ifndef WIN32
#include <dlfcn.h>
#include <pwd.h>
#include <unistd.h>
#if defined(__DARWIN__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/stat.h>

void carbon_init();
void carbon_uninit();

#endif

#else
#include <windows.h> /*for GetModuleFileName*/
#endif	//WIN32

/*local prototypes*/
static void PrintWorldInfo(GF_Terminal *term);
static void ViewOD(GF_Terminal *term, u32 OD_ID, u32 number, const char *URL);
static void PrintODList(GF_Terminal *term, GF_ObjectManager *root_odm, u32 num, u32 indent, char *root_name);

static void ViewODs(GF_Terminal *term, Bool show_timing);
static void PrintGPACConfig(Bool full_dump);
static void MakeScreenshot(Bool for_coverage);
static void PrintAVInfo(Bool final);

static u32 gui_mode = 0;

static Bool restart = GF_FALSE;
static Bool reload = GF_FALSE;
Bool do_coverage = GF_FALSE;

Bool no_prog = 0;

#if defined(__DARWIN__) || defined(__APPLE__)
#define VK_MOD  GF_KEY_MOD_ALT
#else
#define VK_MOD  GF_KEY_MOD_CTRL
#endif

static Bool no_audio = GF_FALSE;
static u32 bench_mode = 0;
static u32 bench_mode_start = 0;
static u32 bench_buffer = 0;
static Bool eos_seen = GF_FALSE;
static Bool addon_visible = GF_TRUE;

Bool is_connected = GF_FALSE;
Bool startup_file = GF_FALSE;
GF_User user;
GF_Terminal *term;
u64 Duration;
GF_Err last_error = GF_OK;
static Bool enable_add_ons = GF_TRUE;
static Fixed playback_speed = FIX_ONE;

static s32 request_next_playlist_item = GF_FALSE;
FILE *playlist = NULL;
static Bool readonly_playlist = GF_FALSE;

static u32 display_rti = 0;
static Bool Run;
static Bool CanSeek = GF_FALSE;
static char the_url[GF_MAX_PATH];
static char pl_path[GF_MAX_PATH];
static Bool no_mime_check = GF_TRUE;
static u64 log_rti_time_start = 0;
static Bool loop_at_end = GF_FALSE;
static u32 forced_width=0;
static u32 forced_height=0;

/*windowless options*/
u32 align_mode = 0;
u32 init_w = 0;
u32 init_h = 0;
u32 last_x, last_y;
Bool right_down = GF_FALSE;

Float scale = 1;

static Bool shell_visible = GF_TRUE;
#if defined(WIN32) && !defined(_WIN32_WCE)

void w32_hide_shell(u32 cmd_type)
{
	typedef HWND (WINAPI *GetConsoleWindowT)(void);
	HMODULE hk32 = GetModuleHandle("kernel32.dll");
	GetConsoleWindowT GetConsoleWindow = (GetConsoleWindowT ) GetProcAddress(hk32,"GetConsoleWindow");
	if (cmd_type==0) {
		ShowWindow( GetConsoleWindow(), SW_SHOW);
		shell_visible = GF_TRUE;
	}
	else if (cmd_type==1) {
		ShowWindow( GetConsoleWindow(), SW_HIDE);
		shell_visible = GF_FALSE;
	}
	else if (cmd_type==2) PostMessage(GetConsoleWindow(), WM_CLOSE, 0, 0);
}

#define hide_shell w32_hide_shell
#else

#define hide_shell(_var)

#endif


void send_open_url(const char *url)
{
	GF_Event evt;
	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_NAVIGATE;
	evt.navigate.to_url = url;
	gf_term_send_event(term, &evt);
}

void PrintUsage()
{
	fprintf(stderr, "Usage MP4Client [options] [filename]\n"
#ifdef GPAC_MEMORY_TRACKING
            "\t-mem-track:  enables memory tracker\n"
            "\t-mem-track-stack:  enables memory tracker with stack dumping\n"
#endif
	        "\t-rti fileName:  logs run-time info (FPS, CPU, Mem usage) to file\n"
	        "\t-rtix fileName: same as -rti but driven by GPAC logs\n"
	        "\t-size WxH:      specifies visual size (default: scene size)\n"
	        "\t-no-thread:     disables thread usage (except for depending on driver audio)\n"
	        "\t-no-audio:      disables audio \n"
	        "\t-no-wnd:        uses windowless mode (Win32 only)\n"
	        "\t-no-back:       uses transparent background for output window when no background is specified (Win32 only)\n"
	        "\t-align vh:      specifies v and h alignment for windowless mode\n"
	        "\t                 possible v values: t(op), m(iddle), b(ottom)\n"
	        "\t                 possible h values: l(eft), m(iddle), r(ight)\n"
	        "\t                 default alignment is top-left\n"
	        "\t                 default alignment is top-left\n"
	        "\t-pause:         pauses at first frame\n"
	        "\t-play-from T:   starts from T seconds in media\n"
	        "\t-speed S:       starts with speed S\n"
	        "\t-loop:          loops presentation\n"
	        "\t-bench:         disable a/v output and bench source decoding (as fast as possible)\n"
	        "\t-vbench:        disable audio output, video sync bench source decoding/display (as fast as possible)\n"
	        "\t-sbench:        disable all decoders and bench systems layer (as fast as possible)\n"
	        "\t-fs:            starts in fullscreen mode\n"
	        "\t-views v1:.:vN: creates an auto-stereo scene of N views. vN can be any type of URL supported by GPAC.\n"
	        "\t                 in this mode, URL argument of GPAC is ignored, GUI as well.\n"
	        "\t                 this is equivalent as using views://v1:.:N as an URL.\n"
	        "\t-mosaic v1:.:vN: creates a mosaic of N views. vN can be any type of URL supported by GPAC.\n"
	        "\t                 in this mode, URL argument of GPAC is ignored.\n"
	        "\t                 this is equivalent as using mosaic://v1:.:N as an URL.\n"
	        "\n"
	        "\t-exit:          automatically exits when presentation is over\n"
	        "\t-run-for TIME:  runs for TIME seconds and exits\n"
	        "\t-service ID:    auto-tune to given service ID in a multiplex\n"
	        "\t-no-save:       disable saving config file on exit\n"
	        "\t-no-addon:      disable automatic loading of media addons declared in source URL\n"
	        "\t-gui:           starts in GUI mode. The GUI is indicated in GPAC config, section General, by the key [StartupFile]\n"
	        "\t-ntp-shift T:   shifts NTP clock of T (signed int) milliseconds\n"
	        "\n"
	        "\t-p profile:    user-defined profile, either a name or path to an existing GPAC config file.\n"
	        "\t-stats:        dumps filter session stats after playback.\n"
	        "\t-graph:        dumps filter session graph after playback.\n"
	        "\t-nk:           disables keyboard interaction.\n"
	        "\t-h or -help:   shows this screen\n"
	        "\t-hc:           shows libgpac core options\n"
	        "\n"
	        "MP4Client - GPAC command line player and dumper - version %s\n"
	        "%s\n"
	        "GPAC Configuration: " GPAC_CONFIGURATION "\n"
	        "Features: %s %s\n",
	        gf_gpac_version(),
	        gf_gpac_copyright(),
	        gf_enabled_features(), gf_disabled_features()
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
	        "\tG: selects object or service ID\n"
	        "\n"
	        "\tp: play/pause the presentation\n"
	        "\ts: step one frame ahead\n"
	        "\tz: seek into presentation by percentage\n"
	        "\tT: seek into presentation by time\n"
	        "\tt: print current timing\n"
	        "\n"
	        "\tu: sends a command (BIFS or LASeR) to the main scene\n"
	        "\te: evaluates JavaScript code\n"
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
	        "%s\n",

	        gf_gpac_version(), gf_gpac_copyright()
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

		if (rti.total_cpu_usage && (bench_mode<2) ) {
			sprintf(szMsg, "FPS %02.02f CPU %2d (%02d) Mem %d kB",
			        gf_term_get_framerate(term, 0), rti.total_cpu_usage, rti.process_cpu_usage, (u32) (rti.gpac_memory / 1024));
		} else {
			sprintf(szMsg, "FPS %02.02f CPU %02d Mem %d kB",
			        gf_term_get_framerate(term, 0), rti.process_cpu_usage, (u32) (rti.gpac_memory / 1024) );
		}

		if (display_rti==2) {
			if (bench_mode>=2) {
				PrintAVInfo(GF_FALSE);
			}
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

#include <gpac/revision.h>
#define MP4CLIENT_CAPTION	"GPAC MP4Client "GPAC_VERSION "-rev" GPAC_GIT_REVISION

static void ResetCaption()
{
	GF_Event event;
	if (display_rti) return;
	event.type = GF_EVENT_SET_CAPTION;
	event.caption.caption = NULL;

	if (is_connected) {
		char szName[1024];
		GF_TermURLInfo urli;

		/*get any service info*/
		if (!startup_file && gf_term_get_service_info(term, gf_term_get_root_object(term), &urli) == GF_OK) {
			strcpy(szName, "");
			if (urli.track_num) {
				char szBuf[10];
				sprintf(szBuf, "%02d ", (u32) urli.track_num );
				strcat(szName, szBuf);
			}
			if (urli.artist) {
				strcat(szName, urli.artist);
				strcat(szName, " ");
			}
			if (urli.name) {
				strcat(szName, urli.name);
				strcat(szName, " ");
			}
			if (urli.album) {
				strcat(szName, "(");
				strcat(szName, urli.album);
				strcat(szName, ")");
			}
			if (urli.provider) {
				strcat(szName, "(");
				strcat(szName, urli.provider);
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
		event.caption.caption = MP4CLIENT_CAPTION;
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

void switch_bench(u32 is_on)
{
	bench_mode = is_on;
	display_rti = is_on ? 2 : 0;
	ResetCaption();
	gf_term_set_option(term, GF_OPT_VIDEO_BENCH, is_on);
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
int getch() {
	return getchar();
}
#endif

/**
 * Reads a line of input from stdin
 * @param line the buffer to fill
 * @param maxSize the maximum size of the line to read
 * @param showContent boolean indicating if the line read should be printed on stderr or not
 */
static const char * read_line_input(char * line, int maxSize, Bool showContent) {
	char read;
	int i = 0;
	if (fflush( stderr ))
		perror("Failed to flush buffer %s");
	do {
		line[i] = '\0';
		if (i >= maxSize - 1)
			return line;
		read = getch();
		if (read == 8 || read == 127) {
			if (i > 0) {
				fprintf(stderr, "\b \b");
				i--;
			}
		} else if (read > 32) {
			fputc(showContent ? read : '*', stderr);
			line[i++] = read;
		}
		fflush(stderr);
	} while (read != '\n');
	if (!read)
		return 0;
	return line;
}

static void do_set_speed(Fixed desired_speed)
{
	if (gf_term_set_speed(term, desired_speed) == GF_OK) {
		playback_speed = desired_speed;
		fprintf(stderr, "Playing at %g speed\n", FIX2FLT(playback_speed));
	} else {
		fprintf(stderr, "Adjusting speed to %g not supported for this content\n", FIX2FLT(desired_speed));
	}
}

Bool GPAC_EventProc(void *ptr, GF_Event *evt)
{
	if (!term) return 0;

	if (gui_mode==1) {
		if (evt->type==GF_EVENT_QUIT) {
			Run = 0;
		} else if (evt->type==GF_EVENT_KEYDOWN) {
			switch (evt->key.key_code) {
			case GF_KEY_C:
				if (evt->key.flags & (GF_KEY_MOD_CTRL|GF_KEY_MOD_ALT)) {
					hide_shell(shell_visible ? 1 : 0);
					if (shell_visible) gui_mode=2;
				}
				break;
			default:
				break;
			}
		}
		return 0;
	}

	switch (evt->type) {
	case GF_EVENT_DURATION:
		Duration = (u64) ( 1000 * (s64) evt->duration.duration);
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
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONSOLE, ("%s %s\n", servName, evt->message.message));
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
		case GF_KEY_C:
			if (evt->key.flags & (GF_KEY_MOD_CTRL|GF_KEY_MOD_ALT)) {
				hide_shell(shell_visible ? 1 : 0);
				if (!shell_visible) gui_mode=1;
			}
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
		case GF_KEY_O:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected) {
				if (gf_term_get_option(term, GF_OPT_MAIN_ADDON)) {
					fprintf(stderr, "Resuming to main content\n");
					gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_PLAY_LIVE);
				} else {
					fprintf(stderr, "Main addon not enabled\n");
				}
			}
			break;
		case GF_KEY_P:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected) {
				u32 pause_state = gf_term_get_option(term, GF_OPT_PLAY_STATE) ;
				fprintf(stderr, "[Status: %s]\n", pause_state ? "Playing" : "Paused");
				if ((pause_state == GF_STATE_PAUSED) && (evt->key.flags & GF_KEY_MOD_SHIFT)) {
					gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_PLAY_LIVE);
				} else {
					gf_term_set_option(term, GF_OPT_PLAY_STATE, (pause_state==GF_STATE_PAUSED) ? GF_STATE_PLAYING : GF_STATE_PAUSED);
				}
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
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected) {
				gf_term_switch_quality(term, 1);
			//	gf_term_set_option(term, GF_OPT_MULTIVIEW_MODE, 0);
			}
			break;
		case GF_KEY_L:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected) {
				gf_term_switch_quality(term, 0);
			//	gf_term_set_option(term, GF_OPT_MULTIVIEW_MODE, 1);
			}
			break;
		case GF_KEY_F5:
			if (is_connected)
				reload = 1;
			break;
		case GF_KEY_A:
			addon_visible = !addon_visible;
			gf_term_toggle_addons(term, addon_visible);
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
				gf_term_set_speed(term, playback_speed);

			if (do_coverage) {
				gf_term_switch_quality(term, 1);
			}

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
		if (playlist) {
			if (Duration>1500)
				request_next_playlist_item = GF_TRUE;
		}
		else if (loop_at_end) {
			restart = 1;
		}
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

		if ((forced_width && forced_height) || scale) {
			GF_Event size;
			u32 nw = forced_width ? forced_width : evt->size.width;
			u32 nh = forced_height ? forced_height : evt->size.height;

			if (scale != 1) {
				nw  = (u32)(nw * scale);
				nh = (u32)(nh * scale);
			}
			if ((nw != evt->size.width) || (nh != evt->size.height)) {
				size.type = GF_EVENT_SIZE;
				size.size.width = nw;
				size.size.height = nh;
				gf_term_user_event(term, &size);
			}
		}
		break;

	case GF_EVENT_METADATA:
		ResetCaption();
		break;

	case GF_EVENT_RELOAD:
		if (is_connected)
			reload = 1;
		break;
	case GF_EVENT_DROPFILE:
	{
		u32 i, pos;
		/*todo - force playlist mode*/
		if (readonly_playlist) {
			gf_fclose(playlist);
			playlist = NULL;
		}
		readonly_playlist = 0;
		if (!playlist) {
			readonly_playlist = 0;
			playlist = gf_temp_file_new(NULL);
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
		if (evt->message.error)  {
			fprintf(stderr, "A fatal error was encoutered: %s (%s) - exiting ...\n", evt->message.message ? evt->message.message : "no details", gf_error_to_string(evt->message.error) );
		}
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
		if (evt->navigate.to_url)
			fprintf(stderr, "Go to URL: \"%s\"\r", evt->navigate.to_url);
		break;
	case GF_EVENT_NAVIGATE:
		if (gf_term_is_supported_url(term, evt->navigate.to_url, 1, no_mime_check)) {
			strncpy(the_url, evt->navigate.to_url, sizeof(the_url)-1);
			the_url[sizeof(the_url) - 1] = 0;
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
		while ((!strlen(evt->auth.user) || !strlen(evt->auth.password)) && (maxTries--) >= 0) {
			fprintf(stderr, "**** Authorization required for site %s ****\n", evt->auth.site_url);
			fprintf(stderr, "login   : ");
			read_line_input(evt->auth.user, 50, 1);
			fprintf(stderr, "\npassword: ");
			read_line_input(evt->auth.password, 50, 0);
			fprintf(stderr, "*********\n");
		}
		if (maxTries < 0) {
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


void list_modules()
{
	u32 i;
	fprintf(stderr, "\rAvailable modules:\n");
	for (i=0; i<gf_modules_count(); i++) {
		char *str = (char *) gf_modules_get_file_name(i);
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
		nav = do_coverage ? 'N' : getch();
		if (nav=='N') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_NONE);
		else if (nav=='E') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_EXAMINE);
		else if (nav=='S') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_SLIDE);
		else fprintf(stderr, "Unknown selector \'%c\' - only \'N\',\'E\',\'S\' allowed\n", nav);
	} else if (type==2) {
		fprintf(stderr, "Select Navigation (\'N\'one, \'W\'alk, \'F\'ly, \'E\'xamine, \'P\'an, \'S\'lide, \'G\'ame, \'V\'R, \'O\'rbit): ");
		nav = do_coverage ? 'N' : getch();
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


static void on_rti_log(void *cbk, GF_LOG_Level ll, GF_LOG_Tool lm, const char *fmt, va_list list)
{
	if (rti_logs && (lm & GF_LOG_RTI)) {
		char szMsg[2048];
		vsnprintf(szMsg, 2048, fmt, list);
		UpdateRTInfo(szMsg + 6 /*"[RTI] "*/);
	}
}

static void init_rti_logs(char *rti_file, char *url, Bool use_rtix)
{
	if (rti_logs) gf_fclose(rti_logs);
	rti_logs = gf_fopen(rti_file, "wt");
	if (rti_logs) {
		fprintf(rti_logs, "!! GPAC RunTime Info ");
		if (url) fprintf(rti_logs, "for file %s", url);
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


#ifdef WIN32
#include <wincon.h>
#endif

int mp4client_main(int argc, char **argv)
{
	char c;
	const char *str;
	int ret_val = 0;
	GF_Err e;
	u32 i;
	u32 simulation_time_in_ms = 0;
	u32 initial_service_id = 0;
	Bool auto_exit = GF_FALSE;
	Bool start_fs = GF_FALSE;
	Bool use_rtix = GF_FALSE;
	Bool pause_at_first = GF_FALSE;
	Bool no_cfg_save = GF_FALSE;
	Bool print_stats = GF_FALSE;
	Bool print_graph = GF_FALSE;
	Bool no_keyboard = GF_FALSE;
	Double play_from = 0;
#ifdef GPAC_MEMORY_TRACKING
    GF_MemTrackerType mem_track = GF_MemTrackerNone;
#endif
	Bool has_command;
	char *url_arg, *out_arg, *profile, *rti_file, *views, *mosaic;
	FILE *logfile = NULL;
#ifndef WIN32
	dlopen(NULL, RTLD_NOW|RTLD_GLOBAL);
#endif

	/*by default use current dir*/
	strcpy(the_url, ".");

	memset(&user, 0, sizeof(GF_User));

	has_command = GF_FALSE;
	url_arg = out_arg = profile = rti_file = views = mosaic = NULL;

	/*first identify profile and mem tracking */
	for (i=1; i<(u32) argc; i++) {
		char *arg = argv[i];
		if (!strcmp(arg, "-p")) {
			profile = argv[i+1];
			i++;
		} else if (!strcmp(arg, "-mem-track") || !strcmp(arg, "-mem-track-stack")) {
#ifdef GPAC_MEMORY_TRACKING
            mem_track = !strcmp(arg, "-mem-track-stack") ? GF_MemTrackerBackTrace : GF_MemTrackerSimple;
#else
			fprintf(stderr, "WARNING - GPAC not compiled with Memory Tracker - ignoring \"%s\"\n", arg);
#endif
		} else if (!strcmp(arg, "-gui")) {
			gui_mode = 1;
		} else if (!strcmp(arg, "-guid")) {
			gui_mode = 2;
		} else if (!strcmp(arg, "-h") || !strcmp(arg, "-help")) {
			PrintUsage();
			return 0;
		} else if (!strcmp(arg, "-hc")) {
			fprintf(stderr, "libgpac options:\n");
			gf_sys_print_core_help(GF_ARGMODE_ALL, 0);
			return 0;
		}
	}

#ifdef GPAC_MEMORY_TRACKING
	gf_sys_init(mem_track, profile);
#else
	gf_sys_init(GF_MemTrackerNone, profile);
#endif

	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);
	//we by default want 2 additional threads:
	//main thread might get locked on vsync
	//second thread might be busy decoding audio/video
	//third thread will then be able to refill all buffers/perform networks tasks
	gf_opts_set_key("temp", "threads", "2");

	e = gf_sys_set_args(argc, (const char **) argv);
	if (e) {
		fprintf(stderr, "Error assigning libgpac arguments: %s\n", gf_error_to_string(e) );
		gf_sys_close();
		return 1;
	}

	if (!gui_mode) {
		str = gf_opts_get_key("General", "ForceGUI");
		if (str && !strcmp(str, "yes")) gui_mode = 1;
	}

	for (i=1; i<(u32) argc; i++) {
		char *arg = argv[i];

		if (!strcmp(arg, "-rti")) {
			rti_file = argv[i+1];
			i++;
		} else if (!strcmp(arg, "-rtix")) {
			rti_file = argv[i+1];
			i++;
			use_rtix = GF_TRUE;
		} else if (!stricmp(arg, "-size")) {
			/*usage of %ud breaks sscanf on MSVC*/
			if (sscanf(argv[i+1], "%dx%d", &forced_width, &forced_height) != 2) {
				forced_width = forced_height = 0;
			}
			i++;
		}
		//libgpac opts using an argument
		else if (!strcmp(arg, "-log-file") || !strcmp(arg, "-lf") || !strcmp(arg, "-logs") || !strcmp(arg, "-cfg")  || !strcmp(arg, "-ifce") ) {
			i++;
		}

		else if (!strcmp(arg, "-no-thread")) {
			gf_opts_set_key("temp", "threads", "0");
		}
		else if (!strcmp(arg, "-no-audio")) {
			no_audio = GF_TRUE;
		}
		else if (!strcmp(arg, "-fs")) start_fs = 1;
		else if (!stricmp(arg, "-help")) {
			PrintUsage();
			return 1;
		}
		else if (!stricmp(arg, "-no-save") || !stricmp(arg, "--no-save") /*old versions used --n-save ...*/) {
			no_cfg_save=1;
		}
		else if (!stricmp(arg, "-ntp-shift")) {
			s32 shift = atoi(argv[i+1]);
			i++;
			gf_net_set_ntp_shift(shift);
		}
		else if (!stricmp(arg, "-run-for")) {
			simulation_time_in_ms = atoi(argv[i+1]) * 1000;
			if (!simulation_time_in_ms)
				simulation_time_in_ms = 1; /*1ms*/
			i++;
		}

		else if (!strcmp(arg, "-out")) {
			out_arg = argv[i+1];
			i++;
		} else if (!stricmp(arg, "-scale")) {
			sscanf(argv[i+1], "%f", &scale);
			i++;
		}
		/* already parsed */
		else if (!strcmp(arg, "-nk")) {
			no_keyboard = GF_TRUE;
		}
		/* already parsed */
		else if (!strcmp(arg, "-p")) {
			i++;
		}
		/* already parsed */
		else if (!strcmp(arg, "-mem-track") || !strcmp(arg, "-mem-track-stack") || !strcmp(arg, "-gui") || !strcmp(arg, "-guid")) {
		}

		/*arguments only used in non-gui mode*/
		else if (!gui_mode) {
			if (arg[0] != '-') {
				if (url_arg) {
					fprintf(stderr, "Several input URLs provided (\"%s\", \"%s\"). Check your command-line.\n", url_arg, arg);
					return 1;
				}
				url_arg = arg;
			}
			else if (!strcmp(arg, "-loop")) loop_at_end = 1;
			else if (!strcmp(arg, "-bench")) bench_mode = 1;
			else if (!strcmp(arg, "-vbench")) bench_mode = 2;
			else if (!strcmp(arg, "-sbench")) bench_mode = 3;
			else if (!strcmp(arg, "-no-addon")) enable_add_ons = GF_FALSE;

			else if (!strcmp(arg, "-pause")) pause_at_first = 1;
			else if (!strcmp(arg, "-play-from")) {
				play_from = atof((const char *) argv[i+1]);
				i++;
			}
			else if (!strcmp(arg, "-speed")) {
				playback_speed = FLT2FIX( atof((const char *) argv[i+1]) );
				if (playback_speed <= 0) playback_speed = FIX_ONE;
				i++;
			}
			else if (!strcmp(arg, "-no-wnd")) user.init_flags |= GF_TERM_WINDOWLESS;
			else if (!strcmp(arg, "-no-back")) user.init_flags |= GF_TERM_WINDOW_TRANSPARENT;
			else if (!strcmp(arg, "-align")) {
				if (argv[i+1][0]=='m') align_mode = 1;
				else if (argv[i+1][0]=='b') align_mode = 2;
				align_mode <<= 8;
				if (argv[i+1][1]=='m') align_mode |= 1;
				else if (argv[i+1][1]=='r') align_mode |= 2;
				i++;
			}
			else if (!strcmp(arg, "-exit")) auto_exit = GF_TRUE;
			else if (!stricmp(arg, "-views")) {
				views = argv[i+1];
				i++;
			}
			else if (!stricmp(arg, "-mosaic")) {
				mosaic = argv[i+1];
				i++;
			}
			else if (!stricmp(arg, "-com")) {
				has_command = GF_TRUE;
				i++;
			}
			else if (!stricmp(arg, "-service")) {
				initial_service_id = atoi(argv[i+1]);
				i++;
			} else if (!stricmp(arg, "-stats")) {
				print_stats=GF_TRUE;
			} else if (!stricmp(arg, "-graph")) {
				print_graph=GF_TRUE;
			} else if (!stricmp(arg, "-cov")) {
				do_coverage = GF_TRUE;
				print_stats = GF_TRUE;
				print_graph = GF_TRUE;
			} else {
				u32 res = gf_sys_is_gpac_arg(arg);
				if (!res) {
					fprintf(stderr, "Unrecognized option %s\n", arg);
				} else if (res==2) {
					i++;
				}
			}
		} else if (gf_sys_is_gpac_arg(arg)==2) {
			i++;
		}
 	}

	if (!gui_mode && !url_arg && (gf_opts_get_key("General", "StartupFile") != NULL)) {
		gui_mode=1;
	}

#ifdef WIN32
	if (gui_mode==1) {
		const char *opt;
		TCHAR buffer[1024];
		DWORD res = GetCurrentDirectory(1024, buffer);
		buffer[res] = 0;
		opt = gf_opts_get_key("core", "mod-dirs");
		if (strstr(opt, buffer)) {
			gui_mode=1;
		} else {
			gui_mode=2;
		}
	}
#endif

	if (gui_mode==1) {
		hide_shell(1);
	}
	if (gui_mode) {
		gf_sys_set_cfg_option("core:noprog=yes");
	}

#if defined(__DARWIN__) || defined(__APPLE__)
	carbon_init();
#endif


	if (rti_file) init_rti_logs(rti_file, url_arg, use_rtix);

	{
		GF_SystemRTInfo rti;
		if (gf_sys_get_rti(0, &rti, 0))
			fprintf(stderr, "System info: %d MB RAM - %d cores\n", (u32) (rti.physical_memory/1024/1024), rti.nb_cores);
	}


	init_w = forced_width;
	init_h = forced_height;

	user.EventProc = GPAC_EventProc;
	/*dummy in this case (global vars) but MUST be non-NULL*/
	user.opaque = &user;

	if (no_audio) user.init_flags |= GF_TERM_NO_AUDIO;	

	if (bench_mode) {
		gf_opts_discard_changes();
		auto_exit = GF_TRUE;
		if (bench_mode!=2) user.init_flags |= GF_TERM_NO_VIDEO;
	}

	if (forced_width && forced_height) {
		char dim[50];
		sprintf(dim, "%d", forced_width);
		gf_opts_set_key("Temp", "DefaultWidth", dim);
		sprintf(dim, "%d", forced_height);
		gf_opts_set_key("Temp", "DefaultHeight", dim);
	}

	fprintf(stderr, "Loading GPAC Terminal\n");
	i = gf_sys_clock();

	term = gf_term_new(&user);
	if (!term) {
		fprintf(stderr, "\nInit error - check you have at least one video out and one rasterizer...\nFound modules:\n");
		list_modules();
		gf_opts_discard_changes();
		gf_sys_close();
		if (logfile) gf_fclose(logfile);
		return 1;
	}
	fprintf(stderr, "Terminal Loaded in %d ms\n", gf_sys_clock()-i);

	if (bench_mode) {
		display_rti = 2;
		gf_term_set_option(term, GF_OPT_VIDEO_BENCH, (bench_mode==3) ? 2 : 1);
		if (bench_mode==1) bench_mode=2;
	}

	str = gf_opts_get_key("General", "NoMIMETypeFetch");
	no_mime_check = (str && !stricmp(str, "yes")) ? 1 : 0;

	if (gf_opts_get_bool("core", "proxy-on")) {
		str = gf_opts_get_key("core", "proxy-name");
		if (str) fprintf(stderr, "HTTP Proxy %s enabled\n", str);
	}

	if (rti_file) {
		str = gf_opts_get_key("General", "RTIRefreshPeriod");
		if (str) {
			rti_update_time_ms = atoi(str);
		} else {
			gf_opts_set_key("General", "RTIRefreshPeriod", "200");
		}
		UpdateRTInfo("At GPAC load time\n");
	}

	Run = 1;

	if (views) {
	}
	else if (url_arg && !strcmp(url_arg, "NOGUI")) {
		url_arg = NULL;
	}
	/*connect if requested*/
	else if (!gui_mode && url_arg) {
		char *ext;

		if (strlen(url_arg) >= sizeof(the_url)) {
			fprintf(stderr, "Input url %s is too long, truncating to %d chars.\n", url_arg, (int)(sizeof(the_url) - 1));
			strncpy(the_url, url_arg, sizeof(the_url)-1);
			the_url[sizeof(the_url) - 1] = 0;
		}
		else {
			strcpy(the_url, url_arg);
		}
		ext = strrchr(the_url, '.');
		if (ext && (!stricmp(ext, ".m3u") || !stricmp(ext, ".pls"))) {
			GF_Err e = GF_OK;
			fprintf(stderr, "Opening Playlist %s\n", the_url);

			strcpy(pl_path, the_url);
			/*this is not clean, we need to have a plugin handle playlist for ourselves*/
			if (!strncmp("http:", the_url, 5)) {
#ifdef FILTER_FIXME
				GF_DownloadSession *sess = gf_dm_sess_new(term->downloader, the_url, GF_NETIO_SESSION_NOT_THREADED, NULL, NULL, &e);
				if (sess) {
					e = gf_dm_sess_process(sess);
					if (!e) {
						strncpy(the_url, gf_dm_sess_get_cache_name(sess), sizeof(the_url) - 1);
						the_url[sizeof(the_url) - 1] = 0;
					}
					gf_dm_sess_del(sess);
				}
#endif
			}

			playlist = e ? NULL : gf_fopen(the_url, "rt");
			readonly_playlist = 1;
			if (playlist) {
				request_next_playlist_item = GF_TRUE;
			} else {
				if (e)
					fprintf(stderr, "Failed to open playlist %s: %s\n", the_url, gf_error_to_string(e) );
				fprintf(stderr, "Hit 'h' for help\n\n");
			}
		} else {
			fprintf(stderr, "Opening URL %s\n", the_url);
			if (pause_at_first) fprintf(stderr, "[Status: Paused]\n");
			gf_term_connect_from_time(term, the_url, (u64) (play_from*1000), pause_at_first);
		}
	} else {
		fprintf(stderr, "Hit 'h' for help\n\n");
		str = gf_opts_get_key("General", "StartupFile");
		if (str) {
			snprintf(the_url, sizeof(the_url)-1, "MP4Client %s", gf_gpac_version() );
			the_url[sizeof(the_url) - 1] = 0;
			gf_term_connect(term, str);
			startup_file = 1;
			is_connected = 1;
		}
	}
	if (gui_mode==2) gui_mode=0;

	if (start_fs) gf_term_set_option(term, GF_OPT_FULLSCREEN, 1);

	if (views) {
		char szTemp[4046];
		sprintf(szTemp, "views://%s", views);
		gf_term_connect(term, szTemp);
	}
	if (mosaic) {
		char szTemp[4046];
		sprintf(szTemp, "mosaic://%s", mosaic);
		gf_term_connect(term, szTemp);
	}
	if (bench_mode) {
		rti_update_time_ms = 500;
		bench_mode_start = gf_sys_clock();
	}

	if (simulation_time_in_ms)
		simulation_time_in_ms += gf_sys_clock();


	while (Run) {

		/*we don't want getchar to block*/
		if ((gui_mode==1) || (no_keyboard || !gf_prompt_has_input()) ) {
			if (reload) {
				reload = 0;
				gf_term_disconnect(term);
				gf_term_connect(term, startup_file ? gf_opts_get_key("General", "StartupFile") : the_url);
			}
			if (restart && gf_term_get_option(term, GF_OPT_IS_OVER)) {
				restart = 0;
				gf_term_play_from_time(term, 0, 0);
			}
			if (request_next_playlist_item) {
				c = '\n';
				request_next_playlist_item = 0;
				goto force_input;
			}

			if (has_command && is_connected) {
				has_command = GF_FALSE;
				for (i=0; i<(u32)argc; i++) {
					if (!strcmp(argv[i], "-com")) {
						gf_term_scene_update(term, NULL, argv[i+1]);
						i++;
					}
				}
			}
			if (initial_service_id && is_connected) {
				GF_ObjectManager *root_od = gf_term_get_root_object(term);
				if (root_od) {
					gf_term_select_service(term, root_od, initial_service_id);
					initial_service_id = 0;
				}
			}

			if (!use_rtix || display_rti) UpdateRTInfo(NULL);


			gf_term_process_step(term);

			if (auto_exit && eos_seen && gf_term_get_option(term, GF_OPT_IS_OVER)) {
				Run = GF_FALSE;
			}

			/*sim time*/
			if (simulation_time_in_ms
			        && ( (gf_term_get_elapsed_time_in_ms(term)>simulation_time_in_ms) || (!url_arg && gf_sys_clock()>simulation_time_in_ms))
			   ) {
				Run = GF_FALSE;
			}
			continue;
		}

		c = gf_prompt_get_char();

force_input:
		switch (c) {
		case 'q':
		{
			GF_Event evt;
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_QUIT;
			gf_term_send_event(term, &evt);
		}
//			Run = 0;
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
			if (1 > scanf("%s", the_url)) {
				fprintf(stderr, "Cannot read absolute URL, aborting\n");
				break;
			}
			if (rti_file) init_rti_logs(rti_file, the_url, use_rtix);
			gf_term_connect(term, the_url);
			break;
		case 'O':
			gf_term_disconnect(term);
			fprintf(stderr, "Enter the absolute URL to the playlist\n");
			if (1 > scanf("%s", the_url)) {
				fprintf(stderr, "Cannot read the absolute URL, aborting.\n");
				break;
			}
			playlist = gf_fopen(the_url, "rt");
			if (playlist) {
				if (1 >	fscanf(playlist, "%s", the_url)) {
					fprintf(stderr, "Cannot read any URL from playlist, aborting.\n");
					gf_fclose( playlist);
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
				} else if (the_url[0] == '#') {
					request_next_playlist_item = GF_TRUE;
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
				if (1 > scanf("%u", &count)) {
					fprintf(stderr, "Cannot read number, aborting.\n");
					break;
				}
				while (count) {
					if (fscanf(playlist, "%s", the_url)) {
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
					res *= 100;
					res /= (s64)Duration;
					fprintf(stderr, " (current %.2f %%)\nEnter Seek percentage:\n", res);
					if (scanf("%d", &seekTo) == 1) {
						if (seekTo > 100) seekTo = 100;
						res = (Double)(s64)Duration;
						res /= 100;
						res *= seekTo;
						gf_term_play_from_time(term, (u64) (s64) res, 0);
					}
				} else {
					u32 r, h, m, s;
					fprintf(stderr, " - Current Time: ");
					PrintTime((u64) res);
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
				fprintf(stderr, "Enter OD ID (0 for main OD): ");
				fflush(stderr);
				if (scanf("%ud", &ID) == 1) {
					ViewOD(term, ID, (u32)-1, NULL);
				} else {
					char str_url[GF_MAX_PATH];
					if (scanf("%s", str_url) == 1)
						ViewOD(term, 0, (u32)-1, str_url);
				}
			}
			break;
		case 'j':
			if (is_connected) {
				u32 num;
				do {
					fprintf(stderr, "Enter OD number (0 for main OD): ");
					fflush(stderr);
				} while( 1 > scanf("%ud", &num));
				ViewOD(term, (u32)-1, num, NULL);
			}
			break;
		case 'b':
			if (is_connected) ViewODs(term, 1);
			break;

		case 'm':
			if (is_connected) ViewODs(term, 0);
			break;

		case 'l':
			list_modules();
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
							if (info.ODID==odid) break;
						}
						odm = NULL;
					}
				}
				do {
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
			PrintGPACConfig(GF_FALSE);
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
		case '4':
			gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_4_3);
			break;
		case '5':
			gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_16_9);
			break;
		case '6':
			gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_FILL_SCREEN);
			break;
		case '7':
			gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_KEEP);
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
		case 'f':
			gf_term_print_stats(term);
			break;
		case 'g':
			gf_term_print_graph(term);
			break;

		case 'u':
		{
			GF_Err e;
			char szCom[8192];
			fprintf(stderr, "Enter command to send:\n");
			fflush(stdin);
			szCom[0] = 0;
			if (1 > scanf("%[^\t\n]", szCom)) {
				fprintf(stderr, "Cannot read command to send, aborting.\n");
				break;
			}
			e = gf_term_scene_update(term, NULL, szCom);
			if (e) fprintf(stderr, "Processing command failed: %s\n", gf_error_to_string(e));
		}
		break;
		case 'e':
		{
			GF_Err e;
			char jsCode[8192];
			fprintf(stderr, "Enter JavaScript code to evaluate:\n");
			fflush(stdin);
			jsCode[0] = 0;
			if (1 > scanf("%[^\t\n]", jsCode)) {
				fprintf(stderr, "Cannot read code to evaluate, aborting.\n");
				break;
			}
			e = gf_term_scene_update(term, "application/ecmascript", jsCode);
			if (e) fprintf(stderr, "Processing JS code failed: %s\n", gf_error_to_string(e));
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
			if (1 > scanf("%[^\t\n]", szOpt)) {
				fprintf(stderr, "Cannot read option\n");
				break;
			}
			gf_sys_set_cfg_option(szOpt);
		}
		break;

		/*extract to PNG*/
		case 'Z':
			MakeScreenshot(GF_FALSE);
			break;

		case 'G':
		{
			GF_ObjectManager *root_od, *odm;
			u32 index;
			char szOpt[8192];
			fprintf(stderr, "Enter 0-based index of object to select or service ID:\n");
			fflush(stdin);
			szOpt[0] = 0;
			if (1 > scanf("%[^\t\n]", szOpt)) {
				fprintf(stderr, "Cannot read OD ID\n");
				break;
			}
			index = atoi(szOpt);
			odm = NULL;
			root_od = gf_term_get_root_object(term);
			if (root_od) {
				if ( gf_term_find_service(term, root_od, index)) {
					gf_term_select_service(term, root_od, index);
				} else {
					fprintf(stderr, "Cannot find service %d - trying with object index\n", index);
					odm = gf_term_get_object(term, root_od, index);
					if (odm) {
						gf_term_select_object(term, odm);
					} else {
						fprintf(stderr, "Cannot find object at index %d\n", index);
					}
				}
			}
		}
		break;

		case 'h':
			PrintHelp();
			break;
		default:
			break;
		}
	}

	if (bench_mode) {
		PrintAVInfo(GF_TRUE);
	}

	/*FIXME: we have an issue in cleaning up after playing in bench mode and run-for 0 (buildbot tests). We for now disable error checks after run-for is done*/
	if (simulation_time_in_ms) {
		gf_log_set_strict_error(0);
	}

	if (print_graph)
		gf_term_print_graph(term);
	if (print_stats)
		gf_term_print_stats(term);

	if (do_coverage) {
		u32 w, h, nb_drawn;
		GF_Event evt;
		Bool is_bound;
		const char *outName;
		GF_ObjectManager *root_odm, *odm;
		PrintAVInfo(GF_TRUE);
		PrintODList(term, NULL, 0, 0, "Root");
		ViewODs(term, GF_TRUE);
		ViewODs(term, GF_FALSE);
		ViewOD(term, 0, (u32) -1, NULL);
		ViewOD(term, 0, 1, NULL);
		PrintUsage();
		PrintHelp();
		PrintWorldInfo(term);
		PrintGPACConfig(GF_TRUE);
		gf_term_dump_scene(term, NULL, NULL, GF_FALSE, 0, NULL);
		gf_term_get_current_service_id(term);
		gf_term_toggle_addons(term, GF_FALSE);
		set_navigation();

		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_MOUSEUP;
		evt.mouse.x = 20;
		evt.mouse.y = 20;
		gf_term_send_event(term, &evt);

		gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
		//exercice step clocks
		gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
		root_odm = gf_term_get_root_object(term);
		gf_term_find_service(term, root_odm, 0);
		gf_term_select_service(term, root_odm, 0);
		odm = gf_term_get_object(term, root_odm, 0);
		gf_term_select_object(term, odm );
		gf_term_object_subscene_type(term, odm);
		gf_term_get_visual_output_size(term, &w, &h);

		gf_term_is_type_supported(term, "video/mp4");
		gf_term_get_url(term);
		gf_term_get_simulation_frame_rate(term, &nb_drawn);

		gf_term_get_text_selection(term, GF_TRUE);
		gf_term_paste_text(term, "test", GF_TRUE);

		MakeScreenshot(GF_TRUE);

		gf_term_scene_update(term, NULL, "REPLACE DYN_TRANS.translation BY 10 10");
		gf_term_add_object(term, "./media/auxiliary_files/subtitle.srt", GF_TRUE);

		gf_term_play_from_time(term, 1000, GF_TRUE);

		gf_term_get_viewpoint(term, 1, &outName, &is_bound);
		gf_term_set_viewpoint(term, 1, "testvp");

		gf_term_connect_with_path(term, "logo.jpg", "./media/auxiliary_files/");
		gf_term_navigate_to(term, "./media/auxiliary_files/logo.jpg");
		send_open_url("./media/auxiliary_files/logo.jpg");
		switch_bench(1);
		do_set_speed(1.0);
		hide_shell(0);
		list_modules();
	}

	i = gf_sys_clock();
	gf_term_disconnect(term);
	if (rti_file) UpdateRTInfo("Disconnected\n");

	fprintf(stderr, "Deleting terminal... ");
	if (playlist) gf_fclose(playlist);

#if defined(__DARWIN__) || defined(__APPLE__)
	carbon_uninit();
#endif

	gf_term_del(term);
	fprintf(stderr, "done (in %d ms) - ran for %d ms\n", gf_sys_clock() - i, gf_sys_clock());

	fprintf(stderr, "GPAC cleanup ...\n");

	if (no_cfg_save)
		gf_opts_discard_changes();

	gf_sys_close();

	if (rti_logs) gf_fclose(rti_logs);
	if (logfile) gf_fclose(logfile);

	if (gui_mode) {
		hide_shell(2);
	}

#ifdef GPAC_MEMORY_TRACKING
	if (mem_track && (gf_memory_size() || gf_file_handles_count() )) {
	        gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
		gf_memory_print();
		return 2;
	}
#endif

	return ret_val;
}

GF_MAIN_FUNC(mp4client_main)


static void MakeScreenshot(Bool for_coverage)
{
	char szFileName[100];
	u32 nb_pass, nb_views, offscreen_view = 0;
	GF_VideoSurface fb;
	GF_Err e;
	nb_pass = 1;
	nb_views = gf_term_get_option(term, GF_OPT_NUM_STEREO_VIEWS);
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
		if (gf_term_get_offscreen_buffer(term, &fb, 0, 0)==GF_OK)
			gf_term_release_screen_buffer(term, &fb);
	}
	while (nb_pass) {
		nb_pass--;
		if (offscreen_view) {
			sprintf(szFileName, "view%d_dump.png", offscreen_view);
			e = gf_term_get_offscreen_buffer(term, &fb, offscreen_view-1, 0);
		} else {
			sprintf(szFileName, "gpac_video_dump_"LLU".png", gf_net_get_utc() );
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
				FILE *png = gf_fopen(szFileName, "wb");
				if (!png) {
					fprintf(stderr, "Error writing file %s\n", szFileName);
					nb_pass = 0;
				} else {
					gf_fwrite(dst, dst_size, 1, png);
					gf_fclose(png);
					fprintf(stderr, "Dump to %s\n", szFileName);
				}
			}
			if (dst) gf_free(dst);
			gf_term_release_screen_buffer(term, &fb);

			if (for_coverage) gf_delete_file(szFileName);
#endif //GPAC_DISABLE_AV_PARSERS
		}
	}
	fprintf(stderr, "Done: %s\n", szFileName);
}

static GF_ObjectManager *video_odm = NULL;
static GF_ObjectManager *audio_odm = NULL;
static GF_ObjectManager *scene_odm = NULL;
static u32 last_odm_count = 0;

static void PrintAVInfo(Bool final)
{
	GF_MediaInfo a_odi, v_odi, s_odi;
	Double avg_dec_time=0;
	u32 tot_time=0;
	Bool print_codecs = final;

	if (scene_odm) {
		GF_ObjectManager *root_odm = gf_term_get_root_object(term);
		u32 count = gf_term_get_object_count(term, root_odm);
		if (last_odm_count != count) {
			last_odm_count = count;
			scene_odm = NULL;
		}
	}
	if (!video_odm && !audio_odm && !scene_odm) {
		u32 count, i;
		GF_ObjectManager *root_odm = root_odm = gf_term_get_root_object(term);
		if (!root_odm) return;

		if (gf_term_get_object_info(term, root_odm, &v_odi)==GF_OK) {
			if (!scene_odm  && (v_odi.generated_scene== 0)) {
				scene_odm = root_odm;
			}
		}

		count = gf_term_get_object_count(term, root_odm);
		for (i=0; i<count; i++) {
			GF_ObjectManager *odm = gf_term_get_object(term, root_odm, i);
			if (!odm) break;
			if (gf_term_get_object_info(term, odm, &v_odi) == GF_OK) {
				if (!video_odm && (v_odi.od_type == GF_STREAM_VISUAL) && (v_odi.raw_media || (v_odi.cb_max_count>1) || v_odi.direct_video_memory || (bench_mode == 3) )) {
					video_odm = odm;
				}
				else if (!audio_odm && (v_odi.od_type == GF_STREAM_AUDIO)) {
					audio_odm = odm;
				}
				else if (!scene_odm && (v_odi.od_type == GF_STREAM_SCENE)) {
					scene_odm = odm;
				}
			}
		}
	}

	if (0 && bench_buffer) {
		fprintf(stderr, "Buffering %d %% ", bench_buffer-1);
		return;
	}

	if (video_odm) {
		if (gf_term_get_object_info(term, video_odm, &v_odi)!= GF_OK) {
			video_odm = NULL;
			return;
		}
	} else {
		memset(&v_odi, 0, sizeof(v_odi));
	}
	if (print_codecs && audio_odm) {
		gf_term_get_object_info(term, audio_odm, &a_odi);
	} else {
		memset(&a_odi, 0, sizeof(a_odi));
	}
	if ((print_codecs || !video_odm) && scene_odm) {
		gf_term_get_object_info(term, scene_odm, &s_odi);
	} else {
		memset(&s_odi, 0, sizeof(s_odi));
	}

	if (final) {
		tot_time = gf_sys_clock() - bench_mode_start;
		fprintf(stderr, "                                                                                     \r");
		fprintf(stderr, "************** Bench Mode Done in %d ms ********************\n", tot_time);
		if (bench_mode==3) fprintf(stderr, "** Systems layer only (no decoding) **\n");

		if (!video_odm) {
			u32 nb_frames_drawn;
			Double FPS = gf_term_get_simulation_frame_rate(term, &nb_frames_drawn);
			fprintf(stderr, "Drawn %d frames FPS %.2f (simulation FPS %.2f) - duration %d ms\n", nb_frames_drawn, ((Float)nb_frames_drawn*1000)/tot_time,(Float) FPS, gf_term_get_time_in_ms(term)  );
		}
	}
	if (print_codecs) {
		if (video_odm) {
			fprintf(stderr, "%s %dx%d sar=%d:%d duration %.2fs\n", v_odi.codec_name, v_odi.width, v_odi.height, v_odi.par ? (v_odi.par>>16)&0xFF : 1, v_odi.par ? (v_odi.par)&0xFF : 1, v_odi.duration);
			if (final) {
				u32 dec_run_time = v_odi.last_frame_time - v_odi.first_frame_time;
				if (!dec_run_time) dec_run_time = 1;
				if (v_odi.duration) fprintf(stderr, "%d%% ", (u32) (100*v_odi.current_time / v_odi.duration ) );
				fprintf(stderr, "%d frames FPS %.2f (max %d us/f) rate avg %d max %d", v_odi.nb_dec_frames, ((Float)v_odi.nb_dec_frames*1000) / dec_run_time, v_odi.max_dec_time, (u32) v_odi.avg_bitrate/1000, (u32) v_odi.max_bitrate/1000);
				if (v_odi.nb_dropped) {
					fprintf(stderr, " (Error during bench: %d frames drop)", v_odi.nb_dropped);
				}
				fprintf(stderr, "\n");
			}
		}
		if (audio_odm) {
			fprintf(stderr, "%s SR %d num channels %d %s duration %.2fs\n", a_odi.codec_name, a_odi.sample_rate, a_odi.num_channels, gf_audio_fmt_name(a_odi.afmt), a_odi.duration);
			if (final) {
				u32 dec_run_time = a_odi.last_frame_time - a_odi.first_frame_time;
				if (!dec_run_time) dec_run_time = 1;
				if (a_odi.duration) fprintf(stderr, "%d%% ", (u32) (100*a_odi.current_time / a_odi.duration ) );
				fprintf(stderr, "%d frames (ms/f %.2f avg %.2f max) rate avg %d max %d", a_odi.nb_dec_frames, ((Float)dec_run_time)/a_odi.nb_dec_frames, a_odi.max_dec_time/1000.0, (u32) a_odi.avg_bitrate/1000, (u32) a_odi.max_bitrate/1000);
				if (a_odi.nb_dropped) {
					fprintf(stderr, " (Error during bench: %d frames drop)", a_odi.nb_dropped);
				}
				fprintf(stderr, "\n");
			}
		}
		if (scene_odm) {
			u32 w, h;
			gf_term_get_visual_output_size(term, &w, &h);
			fprintf(stderr, "%s scene size %dx%d rastered to %dx%d duration %.2fs\n", s_odi.codec_name ? s_odi.codec_name : "", s_odi.width, s_odi.height, w, h, s_odi.duration);
			if (final) {
				if (s_odi.nb_dec_frames>2 && s_odi.total_dec_time) {
					u32 dec_run_time = s_odi.last_frame_time - s_odi.first_frame_time;
					if (!dec_run_time) dec_run_time = 1;
					fprintf(stderr, "%d frames FPS %.2f (max %d us/f) rate avg %d max %d", s_odi.nb_dec_frames, ((Float)s_odi.nb_dec_frames*1000) / dec_run_time, s_odi.max_dec_time, (u32) s_odi.avg_bitrate/1000, (u32) s_odi.max_bitrate/1000);
					fprintf(stderr, "\n");
				} else {
					u32 nb_frames_drawn;
					Double FPS;
					gf_term_get_simulation_frame_rate(term, &nb_frames_drawn);
					tot_time = gf_sys_clock() - bench_mode_start;
					FPS = gf_term_get_framerate(term, 0);
					fprintf(stderr, "%d frames FPS %.2f (abs %.2f)\n", nb_frames_drawn, (1000.0*nb_frames_drawn / tot_time), FPS);
				}
			}
		}
		if (final) {
			fprintf(stderr, "**********************************************************\n\n");
			return;
		}
	}

	if (video_odm) {
		tot_time = v_odi.last_frame_time - v_odi.first_frame_time;
		if (!tot_time) tot_time=1;
		if (v_odi.duration) fprintf(stderr, "%d%% ", (u32) (100*v_odi.current_time / v_odi.duration ) );
		fprintf(stderr, "%d f FPS %.2f (%.2f ms max) rate %d ", v_odi.nb_dec_frames, ((Float)v_odi.nb_dec_frames*1000) / tot_time, v_odi.max_dec_time/1000.0, (u32) v_odi.avg_bitrate/1000);
	}
	else if (scene_odm) {

		if (s_odi.nb_dec_frames>2 && s_odi.total_dec_time) {
			avg_dec_time = (Float) 1000000 * s_odi.nb_dec_frames;
			avg_dec_time /= s_odi.total_dec_time;
			if (s_odi.duration) fprintf(stderr, "%d%% ", (u32) (100*s_odi.current_time / s_odi.duration ) );
			fprintf(stderr, "%d f %.2f (%d us max) - rate %d ", s_odi.nb_dec_frames, avg_dec_time, s_odi.max_dec_time, (u32) s_odi.avg_bitrate/1000);
		} else {
			u32 nb_frames_drawn;
			Double FPS;
			gf_term_get_simulation_frame_rate(term, &nb_frames_drawn);
			tot_time = gf_sys_clock() - bench_mode_start;
			FPS = gf_term_get_framerate(term, 1);
			fprintf(stderr, "%d f FPS %.2f (abs %.2f) ", nb_frames_drawn, (1000.0*nb_frames_drawn / tot_time), FPS);
		}
	}
	else if (audio_odm) {
		if (!print_codecs) {
			gf_term_get_object_info(term, audio_odm, &a_odi);
		}
		tot_time = a_odi.last_frame_time - a_odi.first_frame_time;
		if (!tot_time) tot_time=1;
		if (a_odi.duration) fprintf(stderr, "%d%% ", (u32) (100*a_odi.current_time / a_odi.duration ) );
		fprintf(stderr, "%d frames (ms/f %.2f avg %.2f max)", a_odi.nb_dec_frames, ((Float)tot_time)/a_odi.nb_dec_frames, a_odi.max_dec_time/1000.0);
	}
}

static void PrintWorldInfo(GF_Terminal *term)
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

static void PrintODList(GF_Terminal *term, GF_ObjectManager *root_odm, u32 num, u32 indent, char *root_name)
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
	if (!odi.ODID) {
		fprintf(stderr, "Service not attached\n");
		return;
	}

	for (i=0; i<indent; i++) szIndent[i]=' ';
	szIndent[indent]=0;

	fprintf(stderr, "%s", szIndent);
	fprintf(stderr, "#%d %s - ", num, root_name);
	if (odi.ServiceID) fprintf(stderr, "Service ID %d ", odi.ServiceID);
	if (odi.media_url) {
		fprintf(stderr, "%s\n", odi.media_url);
	} else {
		fprintf(stderr, "OD ID %d\n", odi.ODID);
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
				} else if (odi.service_url) {
					fprintf(stderr, "%s", odi.service_url);
				} else if (odi.remote_url) {
					fprintf(stderr, "%s", odi.remote_url);
				} else {
					fprintf(stderr, "ID %d", odi.ODID);
				}
				fprintf(stderr, " - %s", (odi.od_type==GF_STREAM_VISUAL) ? "Video" : (odi.od_type==GF_STREAM_AUDIO) ? "Audio" : "Systems");
				if (odi.ServiceID) fprintf(stderr, " - Service ID %d", odi.ServiceID);
				fprintf(stderr, "\n");
				break;
			}
		}
	}
}

static void ViewOD(GF_Terminal *term, u32 OD_ID, u32 number, const char *szURL)
{
	GF_MediaInfo odi;
	u32 i, count, d_enum, id;
	GF_TermNetStats stats;
	GF_Err e;
	GF_ObjectManager *odm, *root_odm = gf_term_get_root_object(term);
	if (!root_odm) return;

	odm = NULL;
	if (!szURL && ((!OD_ID && (number == (u32)-1)) || ((OD_ID == (u32)(-1)) && !number))) {
		odm = root_odm;
		if ((gf_term_get_object_info(term, odm, &odi) != GF_OK)) odm=NULL;
	} else {
		count = gf_term_get_object_count(term, root_odm);
		for (i=0; i<count; i++) {
			odm = gf_term_get_object(term, root_odm, i);
			if (!odm) break;
			if (gf_term_get_object_info(term, odm, &odi) == GF_OK) {
				if (szURL && strstr(odi.service_url, szURL)) break;
				if ((number == (u32)(-1)) && (odi.ODID == OD_ID)) break;
				else if (i == (u32)(number-1)) break;
			}
			odm = NULL;
		}
	}
	if (!odm) {
		if (szURL) fprintf(stderr, "cannot find OD for URL %s\n", szURL);
		if (number == (u32)-1) fprintf(stderr, "cannot find OD with ID %d\n", OD_ID);
		else fprintf(stderr, "cannot find OD with number %d\n", number);
		return;
	}
	if (!odi.ODID) {
		if (number == (u32)-1) fprintf(stderr, "Object %d not attached yet\n", OD_ID);
		else fprintf(stderr, "Object #%d not attached yet\n", number);
		return;
	}

	fprintf(stderr, "Object Duration: ");
	if (odi.duration) {
		PrintTime((u32) (odi.duration*1000));
	} else {
		fprintf(stderr, "unknown");
	}
	fprintf(stderr, "\n");

	fprintf(stderr, "Service Handler: %s\n", odi.service_handler);
	fprintf(stderr, "Service URL: %s\n", odi.service_url);

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
		fprintf(stderr, "\tBitrate over last second: %d kbps\n\tMax bitrate over one second: %d kbps\n\tAverage Decoding Time %.2f us %d max)\n\tTotal decoded frames %d\n",
		        (u32) odi.avg_bitrate/1024, odi.max_bitrate/1024, avg_dec_time, odi.max_dec_time, odi.nb_dec_frames);
	}
	if (odi.protection) fprintf(stderr, "Encrypted Media%s\n", (odi.protection==2) ? " NOT UNLOCKED" : "");

	fprintf(stderr, "\nStream ID %d - %s - Clock ID %d\n", odi.pid_id, gf_stream_type_name(odi.od_type), odi.ocr_id);
//	if (esd->dependsOnESID) fprintf(stderr, "\tDepends on Stream ID %d for decoding\n", esd->dependsOnESID);

	if (odi.lang_code)
		fprintf(stderr, "\tStream Language: %s\n", odi.lang_code);

	fprintf(stderr, "\n");

	switch (odi.status) {
	case 0:
		fprintf(stderr, "Stopped - ");
		break;
	case 1:
		fprintf(stderr, "Playing - ");
		break;
	case 2:
		fprintf(stderr, "Paused - ");
		break;
	case 3:
		fprintf(stderr, "Not setup yet\n");
		return;
	default:
		fprintf(stderr, "Setup Failed\n");
		return;
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
		while (gf_term_get_download_info(term, odm, &d_enum, &url, &done, &total, &bps)) {
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
	while (gf_term_get_channel_net_info(term, odm, &d_enum, &id, &stats, &e)) {
		if (e) continue;
		if (!stats.bw_down && !stats.bw_up) continue;

		fprintf(stderr, "Stream ID %d statistics:\n", id);
		if (stats.multiplex_port) {
			fprintf(stderr, "\tMultiplex Port %d - multiplex ID %d\n", stats.multiplex_port, stats.port);
		} else {
			fprintf(stderr, "\tPort %d\n", stats.port);
		}
		fprintf(stderr, "\tPacket Loss Percentage: %.4f\n", stats.pck_loss_percentage);
		fprintf(stderr, "\tDown Bandwidth: %d bps\n", stats.bw_down);
		if (stats.bw_up) fprintf(stderr, "\tUp Bandwidth: %d bps\n", stats.bw_up);
		if (stats.ctrl_port) {
			if (stats.multiplex_port) {
				fprintf(stderr, "\tControl Multiplex Port: %d - Control Multiplex ID %d\n", stats.multiplex_port, stats.ctrl_port);
			} else {
				fprintf(stderr, "\tControl Port: %d\n", stats.ctrl_port);
			}
			fprintf(stderr, "\tDown Bandwidth: %d bps\n", stats.ctrl_bw_down);
			fprintf(stderr, "\tUp Bandwidth: %d bps\n", stats.ctrl_bw_up);
		}
		fprintf(stderr, "\n");
	}
}

static void PrintODTiming(GF_Terminal *term, GF_ObjectManager *odm, u32 indent)
{
	GF_MediaInfo odi;
	u32 ind = indent;
	u32 i, count;
	if (!odm) return;

	if (gf_term_get_object_info(term, odm, &odi) != GF_OK) return;
	if (!odi.ODID) {
		fprintf(stderr, "Service not attached\n");
		return;
	}
	while (ind) {
		fprintf(stderr, " ");
		ind--;
	}

	if (! odi.generated_scene) {

		fprintf(stderr, "- OD %d: ", odi.ODID);
		switch (odi.status) {
		case 1:
			fprintf(stderr, "Playing - ");
			break;
		case 2:
			fprintf(stderr, "Paused - ");
			break;
		default:
			fprintf(stderr, "Stopped - ");
			break;
		}
		if (odi.buffer>=0) fprintf(stderr, "Buffer: %d ms - ", odi.buffer);
		else fprintf(stderr, "Not buffering - ");
		fprintf(stderr, "Clock drift: %d ms", odi.clock_drift);
		fprintf(stderr, " - time: ");
		PrintTime((u32) (odi.current_time*1000));
		fprintf(stderr, "\n");

	} else {
		fprintf(stderr, "+ Service %s:\n", odi.service_url);
	}

	count = gf_term_get_object_count(term, odm);
	for (i=0; i<count; i++) {
		GF_ObjectManager *an_odm = gf_term_get_object(term, odm, i);
		PrintODTiming(term, an_odm, indent+1);
	}
	return;

}

static void PrintODBuffer(GF_Terminal *term, GF_ObjectManager *odm, u32 indent)
{
	Float avg_dec_time;
	GF_MediaInfo odi;
	u32 ind, i, count;
	if (!odm) return;

	if (gf_term_get_object_info(term, odm, &odi) != GF_OK) return;
	if (!odi.ODID) {
		fprintf(stderr, "Service not attached\n");
		return;
	}

	ind = indent;
	while (ind) {
		fprintf(stderr, " ");
		ind--;
	}

	if (odi.generated_scene) {
		fprintf(stderr, "+ Service %s:\n", odi.service_url);
	} else {
		fprintf(stderr, "- OD %d: ", odi.ODID);
		switch (odi.status) {
		case 1:
			fprintf(stderr, "Playing");
			break;
		case 2:
			fprintf(stderr, "Paused");
			break;
		default:
			fprintf(stderr, "Stopped");
			break;
		}
		if (odi.buffer>=0) fprintf(stderr, " - Buffer: %d ms", odi.buffer);
		if (odi.db_unit_count) fprintf(stderr, " - DB: %d AU", odi.db_unit_count);
		if (odi.cb_max_count) fprintf(stderr, " - CB: %d/%d CUs", odi.cb_unit_count, odi.cb_max_count);

		fprintf(stderr, "\n");
		ind = indent;
		while (ind) {
			fprintf(stderr, " ");
			ind--;
		}

		fprintf(stderr, " %d decoded frames - %d dropped frames\n", odi.nb_dec_frames, odi.nb_dropped);

		ind = indent;
		while (ind) {
			fprintf(stderr, " ");
			ind--;
		}

		avg_dec_time = 0;
		if (odi.nb_dec_frames) {
			avg_dec_time = (Float) odi.total_dec_time;
			avg_dec_time /= odi.nb_dec_frames;
		}
		fprintf(stderr, " Avg Bitrate %d kbps (%d max) - Avg Decoding Time %.2f us (%d max)\n",
		        (u32) odi.avg_bitrate/1024, odi.max_bitrate/1024, avg_dec_time, odi.max_dec_time);
	}

	count = gf_term_get_object_count(term, odm);
	for (i=0; i<count; i++) {
		GF_ObjectManager *an_odm = gf_term_get_object(term, odm, i);
		PrintODBuffer(term, an_odm, indent+1);
	}

}

static void ViewODs(GF_Terminal *term, Bool show_timing)
{
	GF_ObjectManager *root_odm = gf_term_get_root_object(term);
	if (!root_odm) return;

	if (show_timing) {
		PrintODTiming(term, root_odm, 0);
	} else {
		PrintODBuffer(term, root_odm, 0);
	}
	fprintf(stderr, "\n");
}


static void PrintGPACConfig(Bool full_dump)
{
	u32 i, j, cfg_count, key_count;
	char szName[200];
	char *secName = NULL;

	if (full_dump) {
		strcpy(szName, "*");
	} else {
		fprintf(stderr, "Enter section name (\"*\" for complete dump):\n");
		if (1 > scanf("%s", szName)) {
			fprintf(stderr, "No section name, aborting.\n");
			return;
		}
		if (strcmp(szName, "*")) secName = szName;
	}

	fprintf(stderr, "\n\n*** GPAC Configuration ***\n\n");

	cfg_count = gf_opts_get_section_count();
	for (i=0; i<cfg_count; i++) {
		const char *sec = gf_opts_get_section_name(i);
		if (secName) {
			if (stricmp(sec, secName)) continue;
		} else {
			if (!stricmp(sec, "General")) continue;
			if (!stricmp(sec, "MimeTypes")) continue;
		}
		fprintf(stderr, "[%s]\n", sec);
		key_count = gf_opts_get_key_count(sec);
		for (j=0; j<key_count; j++) {
			const char *key = gf_opts_get_key_name(sec, j);
			const char *val = gf_opts_get_key(sec, key);
			fprintf(stderr, "%s=%s\n", key, val);
		}
		fprintf(stderr, "\n");
	}
}

