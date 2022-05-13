/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2022
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
static void MakeScreenshot(Bool for_coverage);
static void PrintAVInfo(Bool final);

static u32 gui_mode = 0;
static int ret_val = 0;

static Bool restart = GF_FALSE;
static Bool reload = GF_FALSE;
Bool do_coverage = GF_FALSE;

Bool no_prog = 0;

static FILE *helpout = NULL;
u32 help_flags = 0;

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

static HWND console_hwnd = NULL;
static Bool owns_wnd = GF_FALSE;
#include <tlhelp32.h>
#include <Psapi.h>
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
static void w32_hide_shell(u32 cmd_type)
{
	typedef HWND (WINAPI *GetConsoleWindowT)(void);
	HMODULE hk32 = GetModuleHandle("kernel32.dll");
	if (!console_hwnd) {
		char parentName[GF_MAX_PATH];
		DWORD dwProcessId = 0;
		DWORD dwParentProcessId = 0;
		DWORD dwParentParentProcessId = 0;
		GetConsoleWindowT GetConsoleWindow = (GetConsoleWindowT)GetProcAddress(hk32, "GetConsoleWindow");
		console_hwnd = GetConsoleWindow();
		dwProcessId = GetCurrentProcessId();
		dwParentProcessId = getParentPID(dwProcessId);
		if (dwParentProcessId)
				dwParentParentProcessId = getParentPID(dwParentProcessId);
		//get parent process name, check for explorer
		parentName[0] = 0;
		getProcessName(dwParentProcessId, parentName, GF_MAX_PATH);
		if (strstr(parentName, "explorer")) {
			owns_wnd = GF_TRUE;
		}
		//get parent parent process name, check for devenv (or any other ide name ...)
		else if (dwParentParentProcessId) {
			owns_wnd = GF_FALSE;
			parentName[0] = 0;
			getProcessName(dwParentParentProcessId, parentName, GF_MAX_PATH);
			if (strstr(parentName, "devenv")) {
				owns_wnd = GF_TRUE;
			}
		} else {
			owns_wnd = GF_FALSE;
		}
	}
	if (!owns_wnd || !console_hwnd) return;

	if (cmd_type==0) {
		ShowWindow(console_hwnd, SW_SHOW);
		shell_visible = GF_TRUE;
	}
	else if (cmd_type==1) {
		ShowWindow(console_hwnd, SW_HIDE);
		shell_visible = GF_FALSE;
	}
	else if (cmd_type == 2) {
		PostMessage(GetConsoleWindow(), WM_CLOSE, 0, 0);
	}
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

GF_GPACArg mp4client_args[] =
{
#ifdef GPAC_MEMORY_TRACKING
 	GF_DEF_ARG("mem-track", NULL, "enable memory tracker", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("mem-track-stack", NULL, "enable memory tracker with stack dumping", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
#endif
	 GF_DEF_ARG("rti", NULL, "log run-time info (FPS, CPU, Mem usage) to given file", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT),
	 GF_DEF_ARG("rtix", NULL, "same as -rti but driven by GPAC logs", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT),
	 GF_DEF_ARG("size", NULL, "specify visual size WxH. If not set, scene size or video size is used", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
	 GF_DEF_ARG("rti-refresh", NULL, "set refresh time in ms between two runt-time counters queries (default is 200)", NULL, NULL, GF_ARG_INT, 0),

 	GF_DEF_ARG("no-thread", NULL, "disable thread usage (except for depending on driver audio)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("no-audio", NULL, "disable audio", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),

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

 	GF_DEF_ARG("pause", NULL, "pause at first frame", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("play-from", NULL, "start playback from given time in seconds in media", NULL, NULL, GF_ARG_DOUBLE, 0),
	GF_DEF_ARG("speed", NULL, "start playback wit given speed", NULL, NULL, GF_ARG_DOUBLE, 0),
	GF_DEF_ARG("loop", NULL, "loop playback", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("fs", NULL, "start in fullscreen mode", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("exit", NULL, "exit when presentation is over", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("run-for", NULL, "run for indicated time in seconds and exits", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("service", NULL, "auto-tune to given service ID in a multiplex", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("no-save", NULL, "do not save configuration file on exit", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("no-addon", NULL, "disable automatic loading of media addons declared in source URL", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("gui", NULL, "start in GUI mode. The GUI is indicated in the [configuration](core_config) file __[General]StartupFile__", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("p", NULL, "use indicated profile for the global GPAC config. If not found, config file is created. If a file path is indicated, this will load profile from that file. Otherwise, this will create a directory of the specified name and store new config there. Reserved name `0` means a new profile, not stored to disk. Works using -p=NAME or -p NAME", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("stats", NULL, "dump filter session stats after playback", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("graph", NULL, "dump filter session graph after playback", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("nk", NULL, "disable keyboard interaction", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("h", "help", "show this help. Use `-hx` to show expert help", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("hc", NULL, "show libgpac core options", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("hr", NULL, "show runtime options when keyboard interaction is enabled", NULL, NULL, GF_ARG_BOOL, 0),
	{0}
};


void PrintUsage(Bool show_all)
{
	u32 i=0;

	if (help_flags & GF_PRINTARG_MAN) {
		fprintf(helpout, ".SH \"DESCRIPTION\"\n.LP\nMP4Client is GPAC command-line media player. It supports all GPAC playback features (2D and 3D support, local playback, RTP streaming, HTTP faststart, many audio and video codecs ...).\n.br\nSpecific URLs shortcuts are available, see gpac -h compositor\n.\n.\n.SH OPTIONS\n");
	} else {
		gf_sys_format_help(helpout, help_flags, "Usage: MP4Client [options] [filename]\n"
			"# General\n"
			"The player accepts any URL supported by GPAC.\n"
			"Specific URLs shortcuts are available, see [GPAC Compositor (gpac -h compositor)](compositor)\n"
			"\n"
			"Warning: MP4Client is being deprecated, use `gpac -play URL`, `gpac -gui URL` or `gpac -mp4c URL`.\n"
			"\n"
			"Version: %s\n"
			"%s\n"
			"For more info on GPAC configuration, use `gpac ` [-h](GPAC) `bin`  \n  \n"
			"# Options  \n  \n",
			(help_flags == GF_PRINTARG_MD) ? GPAC_VERSION : gf_gpac_version(),
			gf_gpac_copyright_cite()
		);
	}

	while (mp4client_args[i].name) {
		GF_GPACArg *arg = &mp4client_args[i];
		i++;
		if (!show_all && (arg->flags & (GF_ARG_HINT_EXPERIMENTAL|GF_ARG_HINT_EXPERT) ))
			continue;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4client");
	}
}

typedef enum
{
	MP4C_QUIT = 0,
	MP4C_KILL,
	MP4C_RELOAD,
	MP4C_OPEN,
	MP4C_OPEN_PL,
	MP4C_PL_NEXT,
	MP4C_PL_JUMP,
	MP4C_DISCONNECT,
	MP4C_SELECT,
	MP4C_PAUSE_RESUME,
	MP4C_STEP,
	MP4C_SEEK,
	MP4C_SEEK_TIME,
	MP4C_TIME,
	MP4C_UPDATE,
	MP4C_EVALJS,
	MP4C_SCREENSHOT,
	MP4C_WORLDINFO,
	MP4C_ODLIST,
	MP4C_ODINFO_ID,
	MP4C_ODINFO_NUM,
	MP4C_ODTIME,
	MP4C_ODBUF,
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
	MP4C_DISP_STATS,
	MP4C_DISP_GRAPH,
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
	{'q', MP4C_QUIT, "quit", 0},
	{'X', MP4C_KILL, "kill", 0},
	{'r', MP4C_KILL, "reload current presentation", 0},
	{'o', MP4C_OPEN, "connect to the specified URL", 0},
	{'O', MP4C_OPEN_PL, "connect to the specified playlist", 0},
	{'N', MP4C_PL_NEXT, "switch to the next URL in the playlist. Also works with `\\n`", 0},
	{'P', MP4C_PL_JUMP, "jump to a given number ahead in the playlist", 0},
	{'D', MP4C_DISCONNECT, "disconnect the current presentation", 0},
	{'G', MP4C_SELECT, "select object or service ID", 0},
	{'p', MP4C_PAUSE_RESUME, "play/pause the presentation", 0},
	{'s', MP4C_STEP, "step one frame ahead", 0},
	{'z', MP4C_SEEK, "seek into presentation by percentage", 0},
	{'T', MP4C_SEEK_TIME, "seek into presentation by time", 0},
	{'t', MP4C_TIME, "print current timing", 0},
	{'u', MP4C_UPDATE, "send a command (BIFS or LASeR) to the main scene", 0},
	{'e', MP4C_EVALJS, "evaluate JavaScript code in the main scene", 0},
	{'Z', MP4C_SCREENSHOT, "dump current output frame to PNG", 0},
	{'w', MP4C_WORLDINFO, "view world info", 0},
	{ 'v', MP4C_ODLIST, "view list of active media objects in scene", 0},
	{ 'i', MP4C_ODINFO_ID, "view Object Descriptor info (by ID)", 0},
	{ 'j', MP4C_ODINFO_NUM, "view Object Descriptor info (by number)", 0},
	{ 'b', MP4C_ODTIME, "view media objects timing and buffering info", 0},
	{ 'm', MP4C_ODBUF, "view media objects buffering and memory info", 0},
	{ 'd', MP4C_DUMPSCENE, "dump scene graph", 0},
	{ 'k', MP4C_STRESSMODE, "turn stress mode on/off", 0},
	{ 'n', MP4C_NAVMODE, "change navigation mode", 0},
	{ 'x', MP4C_LASTVP, "reset to last active viewpoint", 0},
	{ '3', MP4C_OGL2D, "switch OpenGL on or off for 2D scenes", 0},
	{ '4', MP4C_AR_4_3, "force 4/3 Aspect Ratio", 0},
	{ '5', MP4C_AR_16_9, "force 16/9 Aspect Ratio", 0},
	{ '6', MP4C_AR_NONE, "force no Aspect Ratio (always fill screen)", 0},
	{ '7', MP4C_AR_ORIG, "force original Aspect Ratio (default)", 0},
	{ 'H', MP4C_DOWNRATE, "set HTTP max download rate", 0},
	{ 'E', MP4C_RELOAD_OPTS, "force reload of compositor options", 0},

	{ 'L', MP4C_LOGS, "change to new log tool/level. CF MP4Client usage for possible values", 0},
	{ 'R', MP4C_DISP_RTI, "toggle run-time info display in window title bar on/off", 0},
	{ 'F', MP4C_DISP_FPS, "toggle displaying of FPS in stderr on/off", 0},
	{ 'f', MP4C_DISP_STATS, "print filter session stats", 0},
	{ 'g', MP4C_DISP_GRAPH, "print filter session graph", 0},

	{ 'h', MP4C_HELP, "print this message", 0},
	//below this, only experimental features
	{ 'M', MP4C_VMEM_CACHE, "specify video cache memory for 2D objects", 1},
	{0}
};

MP4C_Command get_cmd(u8 char_code)
{
	u32 i=0;
	while (MP4C_Keys[i].char_code) {
		if (MP4C_Keys[i].char_code == char_code)
			return MP4C_Keys[i].cmd_type;
		i++;
	}
	return 0;
}

void PrintHelp()
{
	u32 i=0;

	gf_sys_format_help(helpout, help_flags, "# MP4Client runtime commands\n"
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
	if (!Run) return;
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

#ifdef GPAC_ENABLE_COVERAGE
#define getch() 0
#define read_line_input(_line, _maxSize, _showContent) GF_FALSE

#else

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
static Bool read_line_input(char * line, int maxSize, Bool showContent) {
	char read;
	int i = 0;
	if (fflush( stderr ))
		perror("Failed to flush buffer %s");
	do {
		line[i] = '\0';
		if (i >= maxSize - 1)
			return GF_FALSE;
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
		return GF_FALSE;
	return GF_TRUE;
}
#endif //!GPAC_ENABLE_COVERAGE

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
			if (evt->message.error>0)
				ret_val = evt->message.error;
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
		if (right_down && (user.init_flags & GF_VOUT_WINDOWLESS) ) {
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
		if (user.init_flags & GF_VOUT_WINDOWLESS) {
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
			playlist = gf_file_temp(NULL);
		}
		pos = (u32) gf_ftell(playlist);
		i=0;
		while (i<evt->open_file.nb_files) {
			if (evt->open_file.files[i] != NULL) {
				fprintf(playlist, "%s\n", evt->open_file.files[i]);
			}
			i++;
		}
		gf_fseek(playlist, pos, SEEK_SET);
		request_next_playlist_item = 1;
	}
	return 1;

	case GF_EVENT_QUIT:
		if (evt->message.error<0)  {
			fprintf(stderr, "A fatal error was encoutered: %s (%s) - exiting ...\n", evt->message.message ? evt->message.message : "no details", gf_error_to_string(evt->message.error) );
		} else {
			ret_val = evt->message.error;
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
		u32 nb_retry = 4;
		assert( evt->type == GF_EVENT_AUTHORIZATION);
		assert( evt->auth.user);
		assert( evt->auth.password);
		assert( evt->auth.site_url);
		while ((!strlen(evt->auth.user) || !strlen(evt->auth.password)) && (nb_retry > 0) ) {
			nb_retry--;
			fprintf(stderr, "**** Authorization required for site %s ****\n", evt->auth.site_url);
			fprintf(stderr, "login   : ");
			if (!read_line_input(evt->auth.user, 50, 1))
				continue;
			fprintf(stderr, "\npassword: ");
			if (!read_line_input(evt->auth.password, 50, 0))
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
//	fflush(stdin);

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
	MP4C_Command cmdtype;
	const char *str;
	GF_Err e;
	u32 i, ll;
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
	char *url_arg, *gpac_profile, *rti_file;
	FILE *logfile = NULL;
#ifndef WIN32
	dlopen(NULL, RTLD_NOW|RTLD_GLOBAL);
#endif

	helpout = stdout;

	/*by default use current dir*/
	strcpy(the_url, ".");

	memset(&user, 0, sizeof(GF_User));

	has_command = GF_FALSE;
	url_arg = gpac_profile = rti_file = NULL;

	/*first identify profile and mem tracking */
	for (i=1; i<(u32) argc; i++) {
		char *arg = argv[i];
		if (!strcmp(arg, "-p")) {
			gpac_profile = argv[i+1];
			i++;
		} else if (!strncmp(arg, "-p=", 3)) {
			gpac_profile = argv[i]+3;
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
			PrintUsage(GF_FALSE);
			return 0;
		} else if (!strcmp(arg, "-hx")) {
			PrintUsage(GF_TRUE);
			return 0;
		} else if (!strcmp(arg, "-hr")) {
			PrintHelp();
			return 0;
		} else if (!strcmp(arg, "-hc")) {
			fprintf(helpout, "libgpac options:\n");
			gf_sys_print_core_help(helpout, help_flags, GF_ARGMODE_ALL, 0);
			return 0;
		}
	}

#ifdef GPAC_MEMORY_TRACKING
	gf_sys_init(mem_track, gpac_profile);
#else
	gf_sys_init(GF_MemTrackerNone, gpac_profile);
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

		if (!strcmp(arg, "-genmd")) {
			help_flags = GF_PRINTARG_MD;
			helpout = gf_fopen("mp4client.md", "w");

	 		fprintf(helpout, "[**HOME**](Home) » MP4Client  \n");
	 		fprintf(helpout, "<!-- automatically generated - do not edit, patch gpac/applications/mp4client/main.c -->\n");
			PrintUsage(GF_TRUE);
			PrintHelp();
			gf_fclose(helpout);
			gf_sys_close();
			return 0;
		} else if (!strcmp(arg, "-genman")) {
			help_flags = GF_PRINTARG_MAN;
			helpout = gf_fopen("mp4client.1", "w");


	 		fprintf(helpout, ".TH MP4Client 1 2019 MP4Client GPAC\n");
			fprintf(helpout, ".\n.SH NAME\n.LP\nMP4Client \\- GPAC command-line media player\n.SH SYNOPSIS\n.LP\n.B MP4Client\n.RI [options] \\ [file]\n.br\n.\n");

	 		PrintUsage(GF_TRUE);
			PrintHelp();

			fprintf(helpout, ".SH EXAMPLES\n.TP\nBasic and advanced examples are available at https://wiki.gpac.io/mp4client\n");
			fprintf(helpout, ".SH MORE\n.LP\nAuthors: GPAC developers, see git repo history (-log)\n"
			".br\nFor bug reports, feature requests, more information and source code, visit https://github.com/gpac/gpac\n"
			".br\nbuild: %s\n"
			".br\nCopyright: %s\n.br\n"
			".SH SEE ALSO\n"
			".LP\ngpac(1), MP4Box(1)\n", gf_gpac_version(), gf_gpac_copyright());

			gf_fclose(helpout);
			gf_sys_close();
			return 0;
		} else if (!strcmp(arg, "-rti")) {
			rti_file = argv[i+1];
			i++;
		} else if (!strcmp(arg, "-rtix")) {
			rti_file = argv[i+1];
			i++;
			use_rtix = GF_TRUE;
		} else if (!strcmp(arg, "-rti-refresh")) {
			rti_update_time_ms = atoi(argv[i+1]);
			i++;
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
		else if (!stricmp(arg, "-no-save") || !stricmp(arg, "--no-save") /*old versions used --n-save ...*/) {
			no_cfg_save=1;
		}
		else if (!stricmp(arg, "-run-for")) {
			simulation_time_in_ms = (u32) (atof(argv[i+1]) * 1000);
			if (!simulation_time_in_ms)
				simulation_time_in_ms = 1; /*1ms*/
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
		else if (!strcmp(arg, "-mem-track") || !strcmp(arg, "-mem-track-stack") || !strcmp(arg, "-gui") || !strcmp(arg, "-guid")
			 || !strncmp(arg, "-p=", 3)
		) {
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
			else if (!strcmp(arg, "-no-wnd")) user.init_flags |= GF_VOUT_WINDOWLESS;
			else if (!strcmp(arg, "-no-back")) user.init_flags |= GF_VOUT_WINDOW_NO_DECORATION;
			else if (!strcmp(arg, "-align")) {
				if (argv[i+1][0]=='m') align_mode = 1;
				else if (argv[i+1][0]=='b') align_mode = 2;
				align_mode <<= 8;
				if (argv[i+1][1]=='m') align_mode |= 1;
				else if (argv[i+1][1]=='r') align_mode |= 2;
				i++;
			}
			else if (!strcmp(arg, "-exit")) auto_exit = GF_TRUE;
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

	//will run switch helpout to stderr
	helpout = stderr;

	if (!gui_mode && !url_arg && (gf_opts_get_key("General", "StartupFile") != NULL)) {
		gui_mode=1;
	}

#ifdef WIN32
	if (gui_mode==1) {
		const char *opt;
		TCHAR buffer[1024];
		DWORD res = GetCurrentDirectory(1024, buffer);
		buffer[res] = 0;
		opt = gf_opts_get_key("core", "module-dir");
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

	if (no_audio) gf_opts_set_key("temp", "no-audio", "yes");

	if (bench_mode) {
		gf_opts_discard_changes();
		auto_exit = GF_TRUE;
		if (bench_mode!=2)
			gf_opts_set_key("temp", "no-video", "yes");
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
		UpdateRTInfo("At GPAC load time\n");
	}

	Run = 1;

	if (url_arg && !strcmp(url_arg, "NOGUI")) {
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
		//only local playlist - maybe we could remove and control through flist ?
		if (ext && !strncmp("http", the_url, 4) && (!stricmp(ext, ".m3u") || !stricmp(ext, ".pls"))) {
			e = GF_OK;
			fprintf(stderr, "Opening Playlist %s\n", the_url);

			strcpy(pl_path, the_url);
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
		if (c=='\n') cmdtype=MP4C_PL_NEXT;
		else cmdtype = get_cmd(c);

		switch (cmdtype) {
		case MP4C_QUIT:
		{
			GF_Event evt;
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_QUIT;
			gf_term_send_event(term, &evt);
		}
//			Run = 0;
		break;
		case MP4C_KILL:
			exit(0);
			break;

		case MP4C_OPEN:
			startup_file = 0;
			gf_term_disconnect(term);
			fprintf(stderr, "Enter the absolute URL\n");
			if (1 > scanf("%1023s", the_url)) {
				fprintf(stderr, "Cannot read absolute URL, aborting\n");
				break;
			}
			if (rti_file) init_rti_logs(rti_file, the_url, use_rtix);
			gf_term_connect(term, the_url);
			break;
		case MP4C_OPEN_PL:
			gf_term_disconnect(term);
			fprintf(stderr, "Enter the absolute URL to the playlist\n");
			if (1 > scanf("%1023s", the_url)) {
				fprintf(stderr, "Cannot read the absolute URL, aborting.\n");
				break;
			}
			playlist = gf_fopen(the_url, "rt");
			if (playlist) {
				if (1 >	fscanf(playlist, "%1023s", the_url)) {
					fprintf(stderr, "Cannot read any URL from playlist, aborting.\n");
					gf_fclose( playlist);
					break;
				}
				fprintf(stderr, "Opening URL %s\n", the_url);
				gf_term_connect(term, the_url);
			}
			break;
		case MP4C_PL_NEXT:
			if (playlist) {
				int res;
				gf_term_disconnect(term);

				res = fscanf(playlist, "%1023s", the_url);
				if ((res == EOF) && loop_at_end) {
					gf_fseek(playlist, 0, SEEK_SET);
					res = fscanf(playlist, "%1023s", the_url);
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
		case MP4C_PL_JUMP:
			if (playlist) {
				u32 count;
				gf_term_disconnect(term);
				if (1 > scanf("%u", &count)) {
					fprintf(stderr, "Cannot read number, aborting.\n");
					break;
				}
				while (count) {
					if (fscanf(playlist, "%1023s", the_url)) {
						fprintf(stderr, "Failed to read line, aborting\n");
						break;
					}
					count--;
				}
				fprintf(stderr, "Opening URL %s\n", the_url);
				gf_term_connect(term, the_url);
			}
			break;
		case MP4C_RELOAD:
			if (is_connected)
				reload = 1;
			break;

		case MP4C_DISCONNECT:
			if (is_connected) gf_term_disconnect(term);
			break;

		case MP4C_PAUSE_RESUME:
			if (is_connected) {
				Bool is_pause = gf_term_get_option(term, GF_OPT_PLAY_STATE);
				fprintf(stderr, "[Status: %s]\n", is_pause ? "Playing" : "Paused");
				gf_term_set_option(term, GF_OPT_PLAY_STATE, is_pause ? GF_STATE_PLAYING : GF_STATE_PAUSED);
			}
			break;
		case MP4C_STEP:
			if (is_connected) {
				gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
				fprintf(stderr, "Step time: ");
				PrintTime(gf_term_get_time_in_ms(term));
				fprintf(stderr, "\n");
			}
			break;

		case MP4C_SEEK:
		case MP4C_SEEK_TIME:
			if (!CanSeek || (Duration<=2000)) {
				fprintf(stderr, "scene not seekable\n");
			} else {
				Double res;
				s32 seekTo;
				fprintf(stderr, "Duration: ");
				PrintTime(Duration);
				res = gf_term_get_time_in_ms(term);
				if (cmdtype==MP4C_SEEK) {
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

		case MP4C_TIME:
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
		case MP4C_WORLDINFO:
			if (is_connected) PrintWorldInfo(term);
			break;
		case MP4C_ODLIST:
			if (is_connected) PrintODList(term, NULL, 0, 0, "Root");
			break;
		case MP4C_ODINFO_ID:
			if (is_connected) {
				u32 ID;
				fprintf(stderr, "Enter OD ID (0 for main OD): ");
				fflush(stderr);
				if (scanf("%ud", &ID) == 1) {
					ViewOD(term, ID, (u32)-1, NULL);
				} else {
					char str_url[GF_MAX_PATH];
					if (scanf("%1023s", str_url) == 1)
						ViewOD(term, 0, (u32)-1, str_url);
				}
			}
			break;
		case MP4C_ODINFO_NUM:
			if (is_connected) {
				u32 num;
				do {
					fprintf(stderr, "Enter OD number (0 for main OD): ");
					fflush(stderr);
				} while( 1 > scanf("%ud", &num));
				ViewOD(term, (u32)-1, num, NULL);
			}
			break;
		case MP4C_ODTIME:
			if (is_connected) ViewODs(term, 1);
			break;

		case MP4C_ODBUF:
			if (is_connected) ViewODs(term, 0);
			break;

		case MP4C_NAVMODE:
			if (is_connected) set_navigation();
			break;
		case MP4C_LASTVP:
			if (is_connected) gf_term_set_option(term, GF_OPT_NAVIGATION_TYPE, 0);
			break;

		case MP4C_DUMPSCENE:
			if (is_connected) {
				GF_ObjectManager *odm = NULL;
				char radname[GF_MAX_PATH], *sExt;
				u32 count, odid;
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
				} while( 1 > scanf("%1023s", radname));
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

		case MP4C_OGL2D:
		{
			Bool use_3d = !gf_term_get_option(term, GF_OPT_USE_OPENGL);
			if (gf_term_set_option(term, GF_OPT_USE_OPENGL, use_3d)==GF_OK) {
				fprintf(stderr, "Using %s for 2D drawing\n", use_3d ? "OpenGL" : "2D rasterizer");
			}
		}
		break;
		case MP4C_STRESSMODE:
		{
			Bool opt = gf_term_get_option(term, GF_OPT_STRESS_MODE);
			opt = !opt;
			fprintf(stderr, "Turning stress mode %s\n", opt ? "on" : "off");
			gf_term_set_option(term, GF_OPT_STRESS_MODE, opt);
		}
		break;
		case MP4C_AR_4_3:
			gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_4_3);
			break;
		case MP4C_AR_16_9:
			gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_16_9);
			break;
		case MP4C_AR_NONE:
			gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_FILL_SCREEN);
			break;
		case MP4C_AR_ORIG:
			gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_KEEP);
			break;

		case MP4C_DISP_RTI:
			display_rti = !display_rti;
			ResetCaption();
			break;
		case MP4C_DISP_FPS:
			if (display_rti) display_rti = 0;
			else display_rti = 2;
			ResetCaption();
			break;
		case MP4C_DISP_STATS:
		case MP4C_DISP_GRAPH:
			ll = gf_log_get_tool_level(GF_LOG_APP);
			gf_log_set_tool_level(GF_LOG_APP, GF_LOG_INFO);
			if (cmdtype==MP4C_DISP_STATS)
				gf_term_print_stats(term);
			else
				gf_term_print_graph(term);
			gf_log_set_tool_level(GF_LOG_APP, ll);
			break;
		case MP4C_UPDATE:
		{
			char szCom[8192];
			fprintf(stderr, "Enter command to send:\n");
//			fflush(stdin);
			szCom[0] = 0;
			if (1 > scanf("%8191[^\t\n]", szCom)) {
				fprintf(stderr, "Cannot read command to send, aborting.\n");
				break;
			}
			e = gf_term_scene_update(term, NULL, szCom);
			if (e) fprintf(stderr, "Processing command failed: %s\n", gf_error_to_string(e));
		}
		break;
		case MP4C_EVALJS:
		{
			char jsCode[8192];
			fprintf(stderr, "Enter JavaScript code to evaluate:\n");
//			fflush(stdin);
			jsCode[0] = 0;
			if (1 > scanf("%8191[^\t\n]", jsCode)) {
				fprintf(stderr, "Cannot read code to evaluate, aborting.\n");
				break;
			}
			e = gf_term_scene_update(term, "application/ecmascript", jsCode);
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
				fprintf(stderr, "Enter new video cache memory in kBytes (current %ud):\n", gf_term_get_option(term, GF_OPT_VIDEO_CACHE_SIZE));
			} while (1 > scanf("%ud", &size));
			gf_term_set_option(term, GF_OPT_VIDEO_CACHE_SIZE, size);
		}
		break;

		case MP4C_DOWNRATE:
		{
			u32 http_bitrate = gf_term_get_option(term, GF_OPT_HTTP_MAX_RATE);
			do {
				fprintf(stderr, "Enter new http bitrate in bps (0 for none) - current limit: %d\n", http_bitrate);
			} while (1 > scanf("%ud", &http_bitrate));

			gf_term_set_option(term, GF_OPT_HTTP_MAX_RATE, http_bitrate);
		}
		break;

		case MP4C_RELOAD_OPTS:
			gf_term_set_option(term, GF_OPT_RELOAD_CONFIG, 1);
			break;

		/*extract to PNG*/
		case MP4C_SCREENSHOT:
			MakeScreenshot(GF_FALSE);
			break;

		case MP4C_SELECT:
		{
			GF_ObjectManager *root_od, *odm;
			u32 index;
			char szOpt[8192];
			fprintf(stderr, "Enter 0-based index of object to select or service ID:\n");
//			fflush(stdin);
			szOpt[0] = 0;
			if (1 > scanf("%8191[^\t\n]", szOpt)) {
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

		case MP4C_HELP:
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

	if (print_graph || print_stats) {
		ll = gf_log_get_tool_level(GF_LOG_APP);
		gf_log_set_tool_level(GF_LOG_APP, GF_LOG_INFO);
		if (print_graph)
			gf_term_print_graph(term);
		if (print_stats)
			gf_term_print_stats(term);
		gf_log_set_tool_level(GF_LOG_APP, ll);
	}

#ifdef GPAC_ENABLE_COVERAGE
	if (do_coverage) {
		u32 w, h, k, nb_drawn;
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
		PrintUsage(0);
		PrintHelp();
		PrintWorldInfo(term);
		gf_term_dump_scene(term, NULL, NULL, GF_FALSE, 0, NULL);
		gf_term_get_current_service_id(term);
		gf_term_toggle_addons(term, GF_FALSE);
		set_navigation();
		get_cmd('v');

		gf_term_play_from_time(term, 0, 0);

		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_MOUSEUP;
		evt.mouse.x = 20;
		evt.mouse.y = 20;
		gf_term_send_event(term, &evt);

		gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
		//exercise step clocks
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

		gf_term_set_option(term, GF_OPT_AUDIO_MUTE, 1);


		MakeScreenshot(GF_TRUE);

		gf_term_scene_update(term, NULL, "REPLACE DYN_TRANS.translation BY 10 10");
		gf_term_add_object(term, NULL, GF_TRUE);

		gf_term_get_viewpoint(term, 1, &outName, &is_bound);
		gf_term_set_viewpoint(term, 1, "testvp");

		for (k=GF_NAVIGATE_WALK; k<=GF_NAVIGATE_VR; k++) {
			gf_term_set_option(term, GF_OPT_NAVIGATION, k);
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_MOUSEDOWN;
			evt.mouse.x = 100;
			evt.mouse.y = 100;
			gf_term_user_event(term, &evt);
			evt.type = GF_EVENT_MOUSEMOVE;
			evt.mouse.x += 10;
			gf_term_user_event(term, &evt);
			evt.mouse.y += 10;
			gf_term_user_event(term, &evt);

			evt.type = GF_EVENT_KEYDOWN;
			evt.key.key_code = GF_KEY_CONTROL;
			gf_term_user_event(term, &evt);

			evt.type = GF_EVENT_MOUSEMOVE;
			evt.mouse.x = 120;
			evt.mouse.y = 110;
			gf_term_user_event(term, &evt);

			evt.type = GF_EVENT_KEYUP;
			evt.key.key_code = GF_KEY_CONTROL;
			gf_term_user_event(term, &evt);

			evt.type = GF_EVENT_KEYDOWN;
			evt.key.key_code = GF_KEY_J;
			gf_term_user_event(term, &evt);

		}
		gf_term_set_option(term, GF_OPT_NAVIGATION_TYPE, 0);

		gf_term_connect_with_path(term, "logo.jpg", "./media/auxiliary_files/");
		gf_term_navigate_to(term, "./media/auxiliary_files/logo.jpg");
		send_open_url("./media/auxiliary_files/logo.jpg");
		switch_bench(1);
		do_set_speed(1.0);
		hide_shell(0);
		list_modules();
	}
#endif

	i = gf_sys_clock();
	gf_term_disconnect(term);
	if (rti_file) UpdateRTInfo("Disconnected\n");

	if (playlist) gf_fclose(playlist);

#if defined(__DARWIN__) || defined(__APPLE__)
	carbon_uninit();
#endif

	//special condition for immediate exit without terminal deletion
	if (ret_val==3) {
		fprintf(stderr, "Exit forced, no cleanup\n");
		exit(0);
	}

	fprintf(stderr, "Deleting terminal... ");
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
					gf_fwrite(dst, dst_size, png);
					gf_fclose(png);
					fprintf(stderr, "Dump to %s\n", szFileName);
				}
			}
			if (dst) gf_free(dst);
			gf_term_release_screen_buffer(term, &fb);

			if (for_coverage) gf_file_delete(szFileName);
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
	Double avg_dec_time;
	u32 tot_time;

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
		GF_ObjectManager *root_odm = gf_term_get_root_object(term);
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
	if (audio_odm) {
		gf_term_get_object_info(term, audio_odm, &a_odi);
	} else {
		memset(&a_odi, 0, sizeof(a_odi));
	}
	if (!video_odm && scene_odm) {
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
		gf_term_get_object_info(term, audio_odm, &a_odi);

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
		GF_ObjectManager *odm = gf_term_get_object(term, root_odm, i);
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
	if (odi.protection) fprintf(stderr, "Encrypted Media scheme %s\n", gf_4cc_to_str(odi.protection) );

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
