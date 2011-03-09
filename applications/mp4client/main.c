/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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
#include <gpac/avparse.h>
#include <dlfcn.h>

/*ISO 639 languages*/
#include <gpac/iso639.h>

#ifndef WIN32
#include <pwd.h>
#include <unistd.h>
#if defined(__DARWIN__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/stat.h>
#endif

#else
#include <windows.h> /*for GetModuleFileName*/
#include <direct.h>  /*for _mkdir*/
#include <shlobj.h>  /*for getting user-dir*/
#ifndef SHGFP_TYPE_CURRENT
#define SHGFP_TYPE_CURRENT 0 /*needed for MinGW*/
#endif

#endif	//WIN32

/*local prototypes*/
void PrintWorldInfo(GF_Terminal *term);
void ViewOD(GF_Terminal *term, u32 OD_ID, u32 number);
void PrintODList(GF_Terminal *term, GF_ObjectManager *root_odm, u32 num, u32 indent, char *root_name);

void ViewODs(GF_Terminal *term, Bool show_timing);
void PrintGPACConfig();

static Bool gui_mode = 0;

static Bool restart = 0;
#if defined(__DARWIN__) || defined(__APPLE__)
static u32 threading_flags = GF_TERM_NO_THREAD;
#else
static u32 threading_flags = 0;
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
	fprintf(stdout, "Usage MP4Client [options] [filename]\n"
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

void PrintHelp()
{
	fprintf(stdout, "MP4Client command keys:\n"
		"\to: connect to the specified URL\n"
		"\tO: connect to the specified URL in playlist mode\n"
		"\tN: switch to the next URL in the playlist (works with return key as well)\n"
		"\tr: restart current presentation\n"
		"\tp: play/pause the presentation\n"
		"\ts: step one frame ahead\n"
		"\tz: seek into presentation\n"
		"\tt: print current timing\n"
		"\n"
		"\tw: view world info\n"
		"\tv: view Object Descriptor list\n"
		"\ti: view Object Descriptor info (by ID)\n"
		"\tj: view Object Descriptor info (by number)\n"
		"\tb: view media objects timing and buffering info\n"
		"\tm: view media objects buffering and memory info\n"
		"\td: dumps scene graph\n"
		"\n"
		"\tC: Enable Streaming Cache\n"
		"\tS: Stops Streaming Cache and save to file\n"
		"\tA: Aborts Streaming Cache\n"
		"\n"
		"\tk: turns stress mode on/off\n"
		"\tn: changes navigation mode\n"
		"\tx: reset to last active viewpoint\n"
		"\n"
		"\t2: restart using 2D compositor\n"
		"\t3: restart using 3D compositor\n"
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
		"\tR: toggles run-time info display on/off\n"
		"\tq: exit the application\n"
		"\th: print this message\n"
		"\n"
		"MP4Client - GPAC command line player - version %s\n"
		"GPAC Written by Jean Le Feuvre (c) 2001-2005 - ENST (c) 2005-200X\n",

		GPAC_FULL_VERSION
		);
}

GF_Config *create_default_config(char *file_path, char *file_name)
{
	GF_Config *cfg;
	char szPath[GF_MAX_PATH];
	char gui_path[GF_MAX_PATH];
#if defined(__DARWIN__) || defined(__APPLE__)
	u32 size;
#endif

#ifdef WIN32
	FILE *f;
	Bool write_access = 0;

	strcpy(gui_path, file_path);

	/*following code is highly inspired by Osmo4*/
	/*do we have the write privileges on this dir ? if not, use user local data directory*/
	strcpy(szPath, file_path);
	strcat(szPath, "GPAC.cfg");
	f = gf_f64_open(szPath, "wb");
	if (f != NULL) {
		fclose(f);
		write_access = 1;
	} else {
		write_access = 0;
	}
	strcpy(szPath, file_path);
	
	/*get GPAC.cfg path*/
	if (!write_access) {
		char szPath2[GF_MAX_PATH];
		SHGetFolderPath(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, file_path);
		if (file_path[strlen((char *) file_path)-1] != '\\') strcat(file_path, "\\");
		strcat(file_path, "GPAC\\");
		/*create GPAC dir*/
		_mkdir(file_path);
		strcpy(szPath2, file_path);
		strcat(szPath2, "GPAC.cfg");
		f = gf_f64_open(szPath2, "wb");
		assert(f);
		if (!f) return NULL;
		fclose(f);
	}
#else
	FILE *f;
	strcpy(gui_path, "");

	sprintf(szPath, "%s%c%s", file_path, GF_PATH_SEPARATOR, file_name);
	f = gf_f64_open(szPath, "wt");
	fprintf(stdout, "create %s: %s\n", szPath, (f==NULL) ? "Error" : "OK");
	if (!f) return NULL;
	fclose(f);
#endif

	cfg = gf_cfg_new(file_path, file_name);
	if (!cfg) return NULL;

#if defined(WIN32)
	//szPath still contains the executable directory
#elif defined(__DARWIN__) || defined(__APPLE__)
	size = GF_MAX_PATH;
	if (_NSGetExecutablePath(szPath, &size)==0) {
		char *sep = strstr(szPath, "./");
		if (!sep) {
			sep = strrchr(szPath, '/');
			if (sep) sep++;
		}
		fprintf(stdout, "OSX Executable path %s...", szPath);
		if (sep) {
			sep[0] = 0;
			strcpy(gui_path, szPath);
			strcat(szPath, "modules/");
		}
		fprintf(stdout, "Trying modules path %s...", szPath);
		{
			struct stat buf;
			int status_dir;
			memset(&buf, 0, sizeof(struct stat));
			status_dir = stat(szPath, &buf);
			if (!status_dir){
				if (!(buf.st_mode & S_IFDIR)){
					fprintf(stdout, "%s is not a directory, trying with default\n", szPath);
#ifdef GPAC_MODULES_PATH
					strcpy(szPath, GPAC_MODULES_PATH);
					fprintf(stdout, "Using module directory %s\n", szPath);
#endif
				} else {
					fprintf(stdout, "OK\n");
				}
			} else {
					fprintf(stdout, "Cannot stat %s!!!\n", szPath);
#ifdef GPAC_MODULES_PATH
					strcpy(szPath, GPAC_MODULES_PATH);
					fprintf(stdout, "Using module directory %s\n", szPath);
#endif
			}
		}
	}
#elif defined(GPAC_MODULES_PATH)
	fprintf(stdout, "Using module directory %s \n", GPAC_MODULES_PATH);
	strcpy(szPath, GPAC_MODULES_PATH);
#else 
	fprintf(stdout, "Please enter full path to GPAC modules directory:\n");
	while ( 1 > scanf("%s", szPath));
#endif
	gf_cfg_set_key(cfg, "General", "ModulesDirectory", szPath);
	gf_cfg_set_key(cfg, "Audio", "ForceConfig", "yes");
	gf_cfg_set_key(cfg, "Audio", "NumBuffers", "2");
	gf_cfg_set_key(cfg, "Audio", "TotalDuration", "120");
	gf_cfg_set_key(cfg, "Audio", "DisableNotification", "no");
	gf_cfg_set_key(cfg, "FontEngine", "FontReader", "ft_font");

#ifdef WIN32
	GetWindowsDirectory((char*)szPath, MAX_PATH);
	if (szPath[strlen((char*)szPath)-1] != '\\') strcat((char*)szPath, "\\");
	strcat((char *)szPath, "Fonts");
#elif defined(__DARWIN__) || defined(__APPLE__)
	strcpy(szPath, "/Library/Fonts");
#else
	/*these fonts seems installed by default on many systems...*/
	gf_cfg_set_key(cfg, "FontEngine", "FontSerif", "Bitstream Vera Serif");
	gf_cfg_set_key(cfg, "FontEngine", "FontSans", "Bitstream Vera Sans");
	gf_cfg_set_key(cfg, "FontEngine", "FontFixed", "Bitstream Vera Monospace");
	strcpy(szPath, "/usr/share/fonts/truetype/");
#endif
	fprintf(stdout, "Using default font directory %s\n", szPath);
	gf_cfg_set_key(cfg, "FontEngine", "FontDirectory", szPath);

	{
	  char * tmp = gf_get_default_cache_directory();
	  gf_cfg_set_key(cfg, "General", "CacheDirectory", tmp);
	  fprintf(stdout, "Using default cache directory %s\n", tmp);
	  gf_free(tmp);
	}

	gf_cfg_set_key(cfg, "Downloader", "CleanCache", "yes");
	gf_cfg_set_key(cfg, "Compositor", "AntiAlias", "All");
	gf_cfg_set_key(cfg, "Compositor", "FrameRate", "30");
	/*use power-of-2 emulation*/
	gf_cfg_set_key(cfg, "Compositor", "EmulatePOW2", "yes");
#ifdef WIN32
	gf_cfg_set_key(cfg, "Compositor", "ScalableZoom", "yes");
	gf_cfg_set_key(cfg, "Video", "DriverName", "DirectX Video Output");
#elif defined(__DARWIN__)
	gf_cfg_set_key(cfg, "Video", "DriverName", "SDL Video Output");
	/*SDL not so fast with scalable zoom*/
	gf_cfg_set_key(cfg, "Compositor", "ScalableZoom", "no");
#else
	gf_cfg_set_key(cfg, "Video", "DriverName", "X11 Video Output");
	/*x11 only supports scalable zoom*/
	gf_cfg_set_key(cfg, "Compositor", "ScalableZoom", "yes");
	gf_cfg_set_key(cfg, "Audio", "DriverName", "SDL Audio Output");
#endif

	gf_cfg_set_key(cfg, "Video", "SwitchResolution", "no");
	gf_cfg_set_key(cfg, "Network", "AutoReconfigUDP", "yes");
	gf_cfg_set_key(cfg, "Network", "UDPTimeout", "10000");
	gf_cfg_set_key(cfg, "Network", "BufferLength", "3000");

#if defined(__DARWIN__) || defined(__APPLE__)
	gf_cfg_set_key(cfg, "Video", "DriverName", "SDL Video Output");
#endif

	if (gui_path[0]) {
		FILE *f;
		strcat(gui_path, "gui/gui.bt");
		f = fopen(gui_path, "rt");
		if (f) {
			fclose(f);
			gf_cfg_set_key(cfg, "General", "StartupFile", gui_path);
		}
	}


	/*store and reload*/
	gf_cfg_del(cfg);
	return gf_cfg_new(file_path, file_name);
}

static void check_config_directories(GF_Config *cfg)
{
#if (defined(__DARWIN__) || defined(__APPLE__) ) && !defined(GPAC_MODULES_PATH)
	char mod_path[GF_MAX_PATH];
	char gui_path[GF_MAX_PATH];
	char *sep;
	const char *opt;
	u32 size = GF_MAX_PATH;
	if (_NSGetExecutablePath(mod_path, &size)!=0) return;

	sep = strstr(mod_path, "./");
	if (!sep) {
		sep = strrchr(mod_path, '/');
		if (sep) sep++;
	}
	if (!sep) return;

	sep[0] = 0;
	strcpy(gui_path, mod_path);
	strcat(gui_path, "gui/gui.bt");
	strcat(mod_path, "modules/");
	opt = gf_cfg_get_key(cfg, "General", "ModulesDirectory");
	/*modules directory has changed, forced to new location*/
	if (!opt || strcmp(opt, mod_path)) {
		gf_cfg_set_key(cfg, "General", "ModulesDirectory", mod_path);
		gf_cfg_set_key(cfg, "General", "StartupFile", gui_path);
	}
#endif
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

void switch_bench()
{
	if (is_connected) {
		bench_mode = !bench_mode;
		display_rti = !display_rti;
		ResetCaption();
		gf_term_set_speed(term, bench_mode ? bench_speed : FIX_ONE);
	}
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
    if (fflush( stdout ))
      perror("Failed to flush buffer %s");
    do {
      line[i] = '\0';
      if (i >= maxSize - 1)
	return line;
      read = getch();
      if (read == 8 || read == 127){
	if (i > 0){
	  fprintf(stdout, "\b \b");
	  i--;
	}
      } else if (read > 32){
	fputc(showContent ? read : '*', stdout);
	line[i++] = read;
      }
      fflush(stdout);
    } while (read != '\n');
    if (!read)
      return 0;
    return line;
}

Bool GPAC_EventProc(void *ptr, GF_Event *evt)
{
	if (!term) return 0;
	
	if (gui_mode) {
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
			servName = "main service";
		} else if (!strnicmp(evt->message.service, "data:", 5)) {
			servName = "";
		} else {
			servName = evt->message.service;
		}
		servName = "";

		if (!evt->message.message) return 0;
		if (evt->message.error==GF_SCRIPT_INFO) {
			GF_LOG(GF_LOG_INFO, GF_LOG_SCRIPT, ("[Script] %s\n", evt->message.message));
			fprintf(stdout, "%s\n", evt->message.message);
		} else if (evt->message.error) {
			if (!is_connected) last_error = evt->message.error;
			fprintf(stdout, "%s (%s): %s\n", evt->message.message, servName, gf_error_to_string(evt->message.error));
		} else if (!be_quiet) 
			fprintf(stdout, "(%s) %s\r", servName, evt->message.message);
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
			if (evt->key.flags & GF_KEY_MOD_CTRL) fprintf(stdout, "Rendering rate: %f FPS\n", gf_term_get_framerate(term, 0));
			break;
		case GF_KEY_T:
			if (evt->key.flags & GF_KEY_MOD_CTRL) fprintf(stdout, "Scene Time: %f \n", gf_term_get_time_in_ms(term)/1000.0);
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
				fprintf(stdout, "[Status: %s]\n", is_pause ? "Playing" : "Paused");
				gf_term_set_option(term, GF_OPT_PLAY_STATE, (gf_term_get_option(term, GF_OPT_PLAY_STATE)==GF_STATE_PAUSED) ? GF_STATE_PLAYING : GF_STATE_PAUSED);
			}
			break;
		case GF_KEY_S:
			if ((evt->key.flags & GF_KEY_MOD_CTRL) && is_connected) {
				gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
				fprintf(stdout, "Step time: ");
				PrintTime(gf_term_get_time_in_ms(term));
				fprintf(stdout, "\n");
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
		}
		break;

	case GF_EVENT_CONNECT:
		if (evt->connect.is_connected) {
			is_connected = 1;
			fprintf(stdout, "Service Connected\n");
		} else if (is_connected) {
			fprintf(stdout, "Service %s\n", is_connected ? "Disconnected" : "Connection Failed");
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
		if (evt->navigate.to_url) fprintf(stdout, "Go to URL: \"%s\"\r", evt->navigate.to_url);
		break;
	case GF_EVENT_NAVIGATE:
		if (gf_term_is_supported_url(term, evt->navigate.to_url, 1, no_mime_check)) {
			strcpy(the_url, evt->navigate.to_url);
			fprintf(stdout, "Navigating to URL %s\n", the_url);
			gf_term_navigate_to(term, evt->navigate.to_url);
			return 1;
		} else {
			fprintf(stdout, "Navigation destination not supported\nGo to URL: %s\n", evt->navigate.to_url);
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
			fprintf(stdout, "**** Authorization required for site %s ****\n", evt->auth.site_url);
			fprintf(stdout, "login   : ");
			read_line_input(evt->auth.user, 50, 1);
			fprintf(stdout, "\npassword: ");
			read_line_input(evt->auth.password, 50, 0);
			printf("*********\n");
		}
		if (maxTries < 0){
		  printf("**** No User or password has been filled, aborting ***\n");
		  return 0;
		}
		return 1;
	}

	}
	return 0;
}

GF_Config *loadconfigfile(char *filepath)
{
	GF_Config *cfg = NULL;
	char *cfg_dir;
	char szPath[GF_MAX_PATH];

	if (filepath) {
		strcpy(szPath, filepath);
		cfg_dir = strrchr(szPath, '\\');
		if (!cfg_dir) cfg_dir = strrchr(szPath, '/');
		if (cfg_dir) {
			char c = cfg_dir[0];
			cfg_dir[0] = 0;
			cfg = gf_cfg_new(cfg_dir, cfg_dir+1);
			cfg_dir[0] = c;
			if (cfg) goto success;
		} else {
			cfg = gf_cfg_new(".", filepath);
			if (cfg) goto success;
		}
	}
	
#ifdef WIN32
	GetModuleFileNameA(NULL, szPath, GF_MAX_PATH);
	cfg_dir = strrchr(szPath, '\\');
	if (cfg_dir) cfg_dir[1] = 0;

	cfg = gf_cfg_new(szPath, "GPAC.cfg");
	if (cfg) goto success;
	strcpy(szPath, ".");
	cfg = gf_cfg_new(szPath, "GPAC.cfg");
	if (cfg) goto success;
	strcpy(szPath, ".");
	cfg = gf_cfg_new(szPath, "GPAC.cfg");
	if (cfg) goto success;

	GetModuleFileNameA(NULL, szPath, GF_MAX_PATH);
	cfg_dir = strrchr(szPath, '\\');
	if (cfg_dir) cfg_dir[1] = 0;
	cfg = create_default_config(szPath, "GPAC.cfg");
#else
	/*linux*/
	cfg_dir = getenv("HOME");
	if (cfg_dir) {
		strcpy(szPath, cfg_dir);
	} else {
		fprintf(stdout, "WARNING: HOME env var not set - using current directory for config file\n");
		strcpy(szPath, ".");
	}
	cfg = gf_cfg_new(szPath, ".gpacrc");
	if (cfg) goto success;
	fprintf(stdout, "GPAC config file not found in %s - creating new file\n", szPath);
	cfg = create_default_config(szPath, ".gpacrc");
#endif
	if (!cfg) {
	  fprintf(stdout, "cannot create config file in %s directory\n", szPath);
	  return NULL;
	}
 success:
	fprintf(stdout, "Using config file in %s directory\n", szPath);
	check_config_directories(cfg);
	return cfg;
}

void list_modules(GF_ModuleManager *modules)
{
	u32 i;
	fprintf(stdout, "\rAvailable modules:\n");
	for (i=0; i<gf_modules_get_count(modules); i++) {
		char *str = (char *) gf_modules_get_file_name(modules, i);
		if (str) fprintf(stdout, "\t%s\n", str);
	}
	fprintf(stdout, "\n");
}

void set_navigation()
{
	GF_Err e;
	char nav;
	u32 type = gf_term_get_option(term, GF_OPT_NAVIGATION_TYPE);
	e = GF_OK;
	fflush(stdin);

	if (!type) {
		fprintf(stdout, "Content/compositor doesn't allow user-selectable navigation\n");
	} else if (type==1) {
		fprintf(stdout, "Select Navigation (\'N\'one, \'E\'xamine, \'S\'lide): ");
		nav = getch();
		if (nav=='N') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_NONE);
		else if (nav=='E') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_EXAMINE);
		else if (nav=='S') e = gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_SLIDE);
		else fprintf(stdout, "Unknown selector \'%c\' - only \'N\',\'E\',\'S\' allowed\n", nav);
	} else if (type==2) {
		fprintf(stdout, "Select Navigation (\'N\'one, \'W\'alk, \'F\'ly, \'E\'xamine, \'P\'an, \'S\'lide, \'G\'ame, \'V\'R, \'O\'rbit): ");
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
			gf_log_set_level(GF_LOG_DEBUG);
			gf_log_set_tools(GF_LOG_RTI);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_RTI, ("[RTI] System state when enabling log\n"));
		} else if (log_time_start) {
			log_time_start = gf_sys_clock();
		}
	}
}

#ifdef WIN32
#include <wincon.h>
#endif

int main (int argc, char **argv)
{
	char c;
	const char *str;
	u32 i, times[100], nb_times, dump_mode;
	u32 simulation_time = 0;
	Bool auto_exit = 0;
	Bool logs_set = 0;
	Bool start_fs = 0;
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
	dlopen(NULL, RTLD_NOW|RTLD_GLOBAL);


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
	}

	gf_sys_init(enable_mem_tracker);

	cfg_file = loadconfigfile(the_cfg);
	if (!cfg_file) {
		fprintf(stdout, "Error: Configuration File \"GPAC.cfg\" not found\n");
		if (logfile) fclose(logfile);
		return 1;
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
			use_rtix = 1;
		} else if (!strcmp(arg, "-fill")) {
			fill_ar = 1;
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
		} else if (!strcmp(arg, "-log-file") || !strcmp(arg, "-lf")) {
			logfile = gf_f64_open(argv[i+1], "wt");
			gf_log_set_callback(logfile, on_gpac_log);
			i++;
		} else if (!strcmp(arg, "-log-level") || !strcmp(arg, "-ll")) {
			gf_log_set_level(gf_log_parse_level(argv[i+1]));
			logs_set = 1;
			i++;
		} else if (!strcmp(arg, "-log-tools") || !strcmp(arg, "-lt")) {
			gf_log_set_tools(gf_log_parse_tools(argv[i+1]));
			logs_set = 1;
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
		else if (!strcmp(arg, "-no-back")) user.init_flags |= GF_TERM_WINDOW_TRANSPARENT;
#if defined(__DARWIN__) || defined(__APPLE__)
		else if (!strcmp(arg, "-thread")) threading_flags = 0;
#else
		else if (!strcmp(arg, "-no-thread")) threading_flags = GF_TERM_NO_THREAD | GF_TERM_WINDOW_NO_THREAD;
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
				sep[0] = ':'; sep ++;
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
		else if (!stricmp(arg, "-help")) {
			PrintUsage();
			return 1;
		} else { 
			fprintf(stdout, "Unrecognized option %s - skipping\n", arg);
		}
	}
	if (dump_mode && !url_arg) {
		fprintf(stdout, "Missing argument for dump\n");
		PrintUsage();
		if (logfile) fclose(logfile);
		return 1;
	}


	if (!gui_mode) {
		str = gf_cfg_get_key(cfg_file, "General", "ForceGUI");
		if (str && !strcmp(str, "yes")) gui_mode = 1;
	}

	if (gui_mode) {
		threading_flags = GF_TERM_NO_THREAD;
		if (gui_mode==1) {
			hide_shell(1);
			user.init_flags |= GF_TERM_WINDOW_NO_DECORATION;
		}
	}


	if (dump_mode) rti_file = NULL;

	if (!logs_set) {
		gf_log_set_level(GF_LOG_ERROR);
		gf_log_set_tools(0xFFFFFFFF);
	}

	gf_sys_get_rti(500, &rti, GF_RTI_SYSTEM_MEMORY_ONLY);
	memory_at_gpac_startup = rti.physical_memory_avail;
	if (rti_file) init_rti_logs(rti_file, url_arg, use_rtix);

	/*setup dumping options*/
	if (dump_mode) {
		user.init_flags |= GF_TERM_NO_AUDIO | GF_TERM_NO_THREAD | GF_TERM_NO_REGULATION /*| GF_TERM_INIT_HIDE*/;
		if (visible || dump_mode==8) user.init_flags |= GF_TERM_INIT_HIDE;
	} else {
		init_w = forced_width;
		init_h = forced_height;
	}

	fprintf(stdout, "Loading modules\n");
	str = gf_cfg_get_key(cfg_file, "General", "ModulesDirectory");
	assert( str );

	user.modules = gf_modules_new((const unsigned char *) str, cfg_file);
	if (user.modules) i = gf_modules_get_count(user.modules);
	if (!i || !user.modules) {
		fprintf(stdout, "Error: no modules found in %s - exiting\n", str);
		if (user.modules) gf_modules_del(user.modules);
		gf_cfg_del(cfg_file);
		gf_sys_close();
		if (logfile) fclose(logfile);
		return 1;
	}
	fprintf(stdout, "Modules Loaded (%d found in %s)\n", i, str);

	user.config = cfg_file;
	user.EventProc = GPAC_EventProc;
	/*dummy in this case (global vars) but MUST be non-NULL*/
	user.opaque = user.modules;
	if (threading_flags) user.init_flags |= threading_flags;
	if (no_audio) user.init_flags |= GF_TERM_NO_AUDIO;
	if (no_regulation) user.init_flags |= GF_TERM_NO_REGULATION;

	fprintf(stdout, "Loading GPAC Terminal\n");	
	term = gf_term_new(&user);
	if (!term) {
		fprintf(stdout, "\nInit error - check you have at least one video out and one rasterizer...\nFound modules:\n");
		list_modules(user.modules);
		gf_modules_del(user.modules);
		gf_cfg_del(cfg_file);
		gf_sys_close();
		if (logfile) fclose(logfile);
		return 1;
	}
	fprintf(stdout, "Terminal Loaded\n");

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
	if (!gui_mode && url_arg) {
		char *ext;

		strcpy(the_url, url_arg);
		ext = strrchr(the_url, '.');
		if (ext && strncmp("http:", the_url, 5) && (!stricmp(ext, ".m3u") || !stricmp(ext, ".pls"))) {
			fprintf(stdout, "Opening Playlist %s\n", the_url);
			playlist = gf_f64_open(the_url, "rt");
			if (playlist) {
				strcpy(pl_path, the_url);
				if (1 > fscanf(playlist, "%s", the_url))
				  fprintf(stderr, "Cannot read any URL from playlist\n");
				else {
				  fprintf(stdout, "Opening URL %s\n", the_url);
				  gf_term_connect_with_path(term, the_url, pl_path);
				}
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
		if (url_arg) {
			gf_cfg_set_key(cfg_file, "General", "GUIStartupFile", url_arg);
		}
	}
	if (gui_mode==2) gui_mode=0;

	if (start_fs) gf_term_set_option(term, GF_OPT_FULLSCREEN, 1);

	while (Run) {		
		/*we don't want getchar to block*/
		if (gui_mode || !gf_prompt_has_input()) {
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
			if (threading_flags) {
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
			fprintf(stdout, "Enter the absolute URL\n");
			if (1 > scanf("%s", the_url)){
			    fprintf(stderr, "Cannot read absolute URL, aborting\n");
			    break;
			}
			if (rti_file) init_rti_logs(rti_file, the_url, use_rtix);
			gf_term_connect(term, the_url);
			break;
		case 'O':
			gf_term_disconnect(term);
			fprintf(stdout, "Enter the absolute URL to the playlist\n");
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
				fprintf(stdout, "Opening URL %s\n", the_url);
				gf_term_connect(term, the_url);
			}
			break;
		case '\n':
		case 'N':
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
				fprintf(stdout, "Opening URL %s\n", the_url);
				gf_term_connect(term, the_url);
			}
			break;
		case 'r':
			if (is_connected) {
				gf_term_disconnect(term);
				gf_term_connect(term, startup_file ? gf_cfg_get_key(cfg_file, "General", "StartupFile") : the_url);
			}
			break;
		
		case 'D':
			if (is_connected) gf_term_disconnect(term);
			break;

		case 'p':
			if (is_connected) {
				Bool is_pause = gf_term_get_option(term, GF_OPT_PLAY_STATE);
				fprintf(stdout, "[Status: %s]\n", is_pause ? "Playing" : "Paused");
				gf_term_set_option(term, GF_OPT_PLAY_STATE, is_pause ? GF_STATE_PLAYING : GF_STATE_PAUSED);
			}
			break;
		case 's':
			if (is_connected) {
				gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
				fprintf(stdout, "Step time: ");
				PrintTime(gf_term_get_time_in_ms(term));
				fprintf(stdout, "\n");
			}
			break;

		case 'z':
			if (!CanSeek || (Duration<=2000)) {
				fprintf(stdout, "scene not seekable\n");
			} else {
				Double res;
				s32 seekTo;
				fprintf(stdout, "Duration: ");
				PrintTime(Duration);
				res = gf_term_get_time_in_ms(term);
				res *= 100; res /= (s64)Duration;
				fprintf(stdout, " (current %.2f %%)\nEnter Seek percentage:\n", res);
				if (scanf("%d", &seekTo) == 1) { 
					if (seekTo > 100) seekTo = 100;
					res = (Double)(s64)Duration; res /= 100; res *= seekTo;
					gf_term_play_from_time(term, (u64) (s64) res, 0);
				}
			}
			break;

		case 't':
		{
			if (is_connected) {
				fprintf(stdout, "Current Time: ");
				PrintTime(gf_term_get_time_in_ms(term));
				fprintf(stdout, " - Duration: ");
				PrintTime(Duration);
				fprintf(stdout, "\n");
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
				  fprintf(stdout, "Enter OD ID (0 for main OD): ");
				  fflush(stdout);
				} while( 1 > scanf("%ud", &ID));
				ViewOD(term, ID, (u32)-1);
			}
			break;
		case 'j':
			if (is_connected) {
				u32 num;
				do {
				  fprintf(stdout, "Enter OD number (0 for main OD): ");
				  fflush(stdout);
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
				  fprintf(stdout, "Enter Inline OD ID if any or 0 : ");
				  fflush(stdout);
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
				  fprintf(stdout, "Enter file radical name (+\'.x\' for XML dumping) - \"std\" for stdout: ");
				  fflush(stdout);
				} while( 1 > scanf("%s", radname));
				sExt = strrchr(radname, '.');
				xml_dump = 0;
				if (sExt) {
					if (!stricmp(sExt, ".x")) xml_dump = 1;
					sExt[0] = 0;
				}
				std_out = strnicmp(radname, "std", 3) ? 0 : 1;
				e = gf_term_dump_scene(term, std_out ? NULL : radname, NULL, xml_dump, 0, odm);
				fprintf(stdout, "Dump done (%s)\n", gf_error_to_string(e));
			}
			break;

		case 'c':
			PrintGPACConfig();
			break;
		case '3':
		{
			Bool use_3d = !gf_term_get_option(term, GF_OPT_USE_OPENGL);
			if (gf_term_set_option(term, GF_OPT_USE_OPENGL, use_3d)==GF_OK) {
				fprintf(stdout, "Using %s for 2D drawing\n", use_3d ? "OpenGL" : "2D rasterizer");
			}
		}
			break;
		case 'k':
		{
			Bool opt = gf_term_get_option(term, GF_OPT_STRESS_MODE);
			opt = !opt;
			fprintf(stdout, "Turning stress mode %s\n", opt ? "on" : "off");
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
			case GF_MEDIA_CACHE_RUNNING: fprintf(stdout, "Streaming Cache is running - please stop it first\n"); continue;
			}
			switch (gf_term_get_option(term, GF_OPT_MEDIA_CACHE)) {
			case GF_MEDIA_CACHE_ENABLED: fprintf(stdout, "Streaming Cache Enabled\n"); break;
			case GF_MEDIA_CACHE_DISABLED: fprintf(stdout, "Streaming Cache Disabled\n"); break;
			case GF_MEDIA_CACHE_RUNNING: fprintf(stdout, "Streaming Cache Running\n"); break;
			}
			break;
		case 'S':
		case 'A':
			if (gf_term_get_option(term, GF_OPT_MEDIA_CACHE)==GF_MEDIA_CACHE_RUNNING) {
				gf_term_set_option(term, GF_OPT_MEDIA_CACHE, (c=='S') ? GF_MEDIA_CACHE_DISABLED : GF_MEDIA_CACHE_DISCARD);
				fprintf(stdout, "Streaming Cache stopped\n");
			} else {
				fprintf(stdout, "Streaming Cache not running\n");
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
			fprintf(stdout, "Enter command to send:\n");
			fflush(stdin);
			szCom[0] = 0;
			if (1 > scanf("%[^\t\n]", szCom)){
			    fprintf(stderr, "Cannot read command to send, aborting.\n");
			    break;
			}
			e = gf_term_scene_update(term, NULL, szCom);
			if (e) fprintf(stdout, "Processing command failed: %s\n", gf_error_to_string(e));
		}
			break;

		case 'L':
		{
			char szLog[1024];
			fprintf(stdout, "Enter new log level:\n");
			if (1 > scanf("%s", szLog)){
			    fprintf(stderr, "Cannot read new log level, aborting.\n");
			    break;
			}
			gf_log_set_level(gf_log_parse_level(szLog));
		}
			break;
		case 'T':
		{
			char szLog[1024];
			fprintf(stdout, "Enter new log tools:\n");
			if (1 > scanf("%s", szLog)){
			    fprintf(stderr, "Cannot read new log tools, aborting.\n");
			    break;
			}
			gf_log_set_tools(gf_log_parse_tools(szLog));
		}
			break;
		case 'g':
		{
			GF_SystemRTInfo rti;
			gf_sys_get_rti(rti_update_time_ms, &rti, 0);
			fprintf(stdout, "GPAC allocated memory "LLD"\n", rti.gpac_memory);
		}
			break;
		case 'M':
		{
			u32 size;
			do {
				fprintf(stdout, "Enter new video cache memory in kBytes (current %ud):\n", gf_term_get_option(term, GF_OPT_VIDEO_CACHE_SIZE));
			} while (1 > scanf("%ud", &size));
			gf_term_set_option(term, GF_OPT_VIDEO_CACHE_SIZE, size);
		}
			break;

		case 'E':
			gf_term_set_option(term, GF_OPT_RELOAD_CONFIG, 1); 
			break;

		case 'B':
			switch_bench();
			break;

		/*extract to PNG*/
		case 'Z':
		{
			GF_VideoSurface fb;
			GF_Err e;
			e = gf_term_get_screen_buffer(term, &fb);
			if (e) {
				fprintf(stdout, "Error dumping screen buffer %s\n", gf_error_to_string(e) );
			} else {
				u32 dst_size = fb.width*fb.height*3;
				char *dst = (char*)gf_malloc(sizeof(char)*dst_size);

				e = gf_img_png_enc(fb.video_buffer, fb.width, fb.height, fb.pitch_y, fb.pixel_format, dst, &dst_size);
				if (e) {
					fprintf(stdout, "Error encoding PNG %s\n", gf_error_to_string(e) );
				} else {
					FILE *png = gf_f64_open("dump.png", "wb");
					if (!png) {
						fprintf(stdout, "Error writing file dump.png\n");
					} else {
						fwrite(dst, dst_size, 1, png);
						fclose(png);
						fprintf(stdout, "Writing file dump.png\n");
					}
				}
				if (dst) gf_free(dst);
				gf_term_release_screen_buffer(term, &fb);
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
		fprintf(stdout, "No World Info available\n");
	} else {
		fprintf(stdout, "\t%s\n", title ? title : "No title available");
		for (i=0; i<gf_list_count(descs); i++) {
			char *str = gf_list_get(descs, i);
			fprintf(stdout, "%s\n", str);
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
		fprintf(stdout, "Currently loaded objects:\n");
		root_odm = gf_term_get_root_object(term);
	}
	if (!root_odm) return;
	if (gf_term_get_object_info(term, root_odm, &odi) != GF_OK) return;
	if (!odi.od) {
		fprintf(stdout, "Service not attached\n");
		return;
	}

	for (i=0;i<indent;i++) szIndent[i]=' ';
	szIndent[indent]=0;
	
	fprintf(stdout, "%s", szIndent);
	fprintf(stdout, "#%d %s - ", num, root_name);
	if (odi.media_url) {
		fprintf(stdout, "%s\n", odi.media_url);
	} else {
		fprintf(stdout, "OD ID %d\n", odi.od->objectDescriptorID);
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
				fprintf(stdout, "%s", szIndent);
				fprintf(stdout, "#%d - ", num);
				if (odi.media_url) {
					fprintf(stdout, "%s", odi.media_url);
				} else {
					fprintf(stdout, "ID %d", odi.od->objectDescriptorID);
				}
				fprintf(stdout, " - %s\n", (odi.od_type==GF_STREAM_VISUAL) ? "Video" : (odi.od_type==GF_STREAM_AUDIO) ? "Audio" : "Systems");
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
	char code[5];
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
		if (number == (u32)-1) fprintf(stdout, "cannot find OD with ID %d\n", OD_ID);
		else fprintf(stdout, "cannot find OD with number %d\n", number);
		return;
	}
	if (!odi.od) {
		if (number == (u32)-1) fprintf(stdout, "Object %d not attached yet\n", OD_ID);
		else fprintf(stdout, "Object #%d not attached yet\n", number);		
		return;
	}

	if (!odi.od) {
		fprintf(stdout, "Service not attached\n");
		return;
	}

	if (odi.od->tag==GF_ODF_IOD_TAG) {
		fprintf(stdout, "InitialObjectDescriptor %d\n", odi.od->objectDescriptorID);
		fprintf(stdout, "Profiles and Levels: Scene %x - Graphics %x - Visual %x - Audio %x - OD %x\n", 
			odi.scene_pl, odi.graphics_pl, odi.visual_pl, odi.audio_pl, odi.OD_pl);
		fprintf(stdout, "Inline Profile Flag %d\n", odi.inline_pl);
	} else {
		fprintf(stdout, "ObjectDescriptor %d\n", odi.od->objectDescriptorID);
	}

	fprintf(stdout, "Object Duration: ");
	if (odi.duration) {
	  PrintTime((u32) (odi.duration*1000));
	} else {
	  fprintf(stdout, "unknown");
	}
	fprintf(stdout, "\n");

	if (odi.owns_service) {
		fprintf(stdout, "Service Handler: %s\n", odi.service_handler);
		fprintf(stdout, "Service URL: %s\n", odi.service_url);
	}		
	if (odi.codec_name) {
		Float avg_dec_time;
		switch (odi.od_type) {
		case GF_STREAM_VISUAL:
			fprintf(stdout, "Video Object: Width %d - Height %d\r\n", odi.width, odi.height);
			fprintf(stdout, "Media Codec: %s\n", odi.codec_name);
			if (odi.par) fprintf(stdout, "Pixel Aspect Ratio: %d:%d\n", (odi.par>>16)&0xFF, (odi.par)&0xFF);
			break;
		case GF_STREAM_AUDIO:
			fprintf(stdout, "Audio Object: Sample Rate %d - %d channels\r\n", odi.sample_rate, odi.num_channels);
			fprintf(stdout, "Media Codec: %s\n", odi.codec_name);
			break;
		case GF_STREAM_SCENE:
		case GF_STREAM_PRIVATE_SCENE:
			if (odi.width && odi.height) {
				fprintf(stdout, "Scene Description - Width %d - Height %d\n", odi.width, odi.height);
			} else {
				fprintf(stdout, "Scene Description - no size specified\n");
			}
			fprintf(stdout, "Scene Codec: %s\n", odi.codec_name);
			break;
		case GF_STREAM_TEXT:
			if (odi.width && odi.height) {
				fprintf(stdout, "Text Object: Width %d - Height %d\n", odi.width, odi.height);
			} else {
				fprintf(stdout, "Text Object: No size specified\n");
			}
			fprintf(stdout, "Text Codec %s\n", odi.codec_name);
			break;
		}
	
		avg_dec_time = 0;
		if (odi.nb_dec_frames) { 
			avg_dec_time = (Float) odi.total_dec_time; 
			avg_dec_time /= odi.nb_dec_frames; 
		}
		fprintf(stdout, "\tBitrate over last second: %d kbps\n\tMax bitrate over one second: %d kbps\n\tAverage Decoding Time %.2f ms (%d max)\n\tTotal decoded frames %d\n", 
			(u32) odi.avg_bitrate/1024, odi.max_bitrate/1024, avg_dec_time, odi.max_dec_time, odi.nb_dec_frames);
	}
	if (odi.protection) fprintf(stdout, "Encrypted Media%s\n", (odi.protection==2) ? " NOT UNLOCKED" : "");

	count = gf_list_count(odi.od->ESDescriptors);
	fprintf(stdout, "%d streams in OD\n", count);
	for (i=0; i<count; i++) {
		GF_ESD *esd = (GF_ESD *) gf_list_get(odi.od->ESDescriptors, i);

		fprintf(stdout, "\nStream ID %d - Clock ID %d\n", esd->ESID, esd->OCRESID);
		if (esd->dependsOnESID) fprintf(stdout, "\tDepends on Stream ID %d for decoding\n", esd->dependsOnESID);

		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_OD: fprintf(stdout, "\tOD Stream - version %d\n", esd->decoderConfig->objectTypeIndication); break;
		case GF_STREAM_OCR: fprintf(stdout, "\tOCR Stream\n"); break;
		case GF_STREAM_SCENE: fprintf(stdout, "\tScene Description Stream - version %d\n", esd->decoderConfig->objectTypeIndication); break;
		case GF_STREAM_VISUAL:
			fprintf(stdout, "\tVisual Stream - media type: ");
			switch (esd->decoderConfig->objectTypeIndication) {
			case GPAC_OTI_VIDEO_MPEG4_PART2: fprintf(stdout, "MPEG-4\n"); break;
			case GPAC_OTI_VIDEO_MPEG2_SIMPLE: fprintf(stdout, "MPEG-2 Simple Profile\n"); break;
			case GPAC_OTI_VIDEO_MPEG2_MAIN: fprintf(stdout, "MPEG-2 Main Profile\n"); break;
			case GPAC_OTI_VIDEO_MPEG2_SNR: fprintf(stdout, "MPEG-2 SNR Profile\n"); break;
			case GPAC_OTI_VIDEO_MPEG2_SPATIAL: fprintf(stdout, "MPEG-2 Spatial Profile\n"); break;
			case GPAC_OTI_VIDEO_MPEG2_HIGH: fprintf(stdout, "MPEG-2 High Profile\n"); break;
			case GPAC_OTI_VIDEO_MPEG2_422: fprintf(stdout, "MPEG-2 422 Profile\n"); break;
			case GPAC_OTI_VIDEO_MPEG1: fprintf(stdout, "MPEG-1\n"); break;
			case GPAC_OTI_IMAGE_JPEG: fprintf(stdout, "JPEG\n"); break;
			case GPAC_OTI_IMAGE_PNG: fprintf(stdout, "PNG\n"); break;
			case GPAC_OTI_IMAGE_JPEG_2000: fprintf(stdout, "JPEG2000\n"); break;
				
			case GPAC_OTI_MEDIA_GENERIC:
				memcpy(code, esd->decoderConfig->decoderSpecificInfo->data, 4);
				code[4] = 0;
				fprintf(stdout, "GPAC Intern (%s)\n", code);
				break;
			default:
				fprintf(stdout, "Private Type (0x%x)\n", esd->decoderConfig->objectTypeIndication);
				break;
			}
			break;

		case GF_STREAM_AUDIO:
			fprintf(stdout, "\tAudio Stream - media type: ");
			switch (esd->decoderConfig->objectTypeIndication) {
			case GPAC_OTI_AUDIO_AAC_MPEG4: fprintf(stdout, "MPEG-4\n"); break;
			case GPAC_OTI_AUDIO_AAC_MPEG2_MP: fprintf(stdout, "MPEG-2 AAC Main Profile\n"); break;
			case GPAC_OTI_AUDIO_AAC_MPEG2_LCP: fprintf(stdout, "MPEG-2 AAC LowComplexity Profile\n"); break;
			case GPAC_OTI_AUDIO_AAC_MPEG2_SSRP: fprintf(stdout, "MPEG-2 AAC Scalable Sampling Rate Profile\n"); break;
			case GPAC_OTI_AUDIO_MPEG2_PART3: fprintf(stdout, "MPEG-2 Audio\n"); break;
			case GPAC_OTI_AUDIO_MPEG1: fprintf(stdout, "MPEG-1 Audio\n"); break;
			case GPAC_OTI_AUDIO_EVRC_VOICE: fprintf(stdout, "EVRC Audio\n"); break;
			case GPAC_OTI_AUDIO_SMV_VOICE: fprintf(stdout, "SMV Audio\n"); break;
			case GPAC_OTI_AUDIO_13K_VOICE: fprintf(stdout, "QCELP Audio\n"); break;
			case GPAC_OTI_MEDIA_GENERIC:
				memcpy(code, esd->decoderConfig->decoderSpecificInfo->data, 4);
				code[4] = 0;
				fprintf(stdout, "GPAC Intern (%s)\n", code);
				break;
			default:
				fprintf(stdout, "Private Type (0x%x)\n", esd->decoderConfig->objectTypeIndication);
				break;
			}
			break;
		case GF_STREAM_MPEG7: fprintf(stdout, "\tMPEG-7 Stream - version %d\n", esd->decoderConfig->objectTypeIndication); break;
		case GF_STREAM_IPMP: fprintf(stdout, "\tIPMP Stream - version %d\n", esd->decoderConfig->objectTypeIndication); break;
		case GF_STREAM_OCI: fprintf(stdout, "\tOCI Stream - version %d\n", esd->decoderConfig->objectTypeIndication); break;
		case GF_STREAM_MPEGJ: fprintf(stdout, "\tMPEGJ Stream - version %d\n", esd->decoderConfig->objectTypeIndication); break;
		case GF_STREAM_INTERACT: fprintf(stdout, "\tUser Interaction Stream - version %d\n", esd->decoderConfig->objectTypeIndication); break;
		case GF_STREAM_TEXT: fprintf(stdout, "\tStreaming Text Stream - version %d\n", esd->decoderConfig->objectTypeIndication); break;
		default: fprintf(stdout, "Unknown Stream\r\n"); break;
		}

		fprintf(stdout, "\tBuffer Size %d\n\tAverage Bitrate %d bps\n\tMaximum Bitrate %d bps\n", esd->decoderConfig->bufferSizeDB, esd->decoderConfig->avgBitrate, esd->decoderConfig->maxBitrate);
		if (esd->slConfig->predefined==SLPredef_SkipSL) {
			fprintf(stdout, "\tNot using MPEG-4 Synchronization Layer\n");
		} else {
			fprintf(stdout, "\tStream Clock Resolution %d\n", esd->slConfig->timestampResolution);
		}
		if (esd->URLString) fprintf(stdout, "\tStream Location: %s\n", esd->URLString);

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
			fprintf(stdout, "\tStream Language: %s\n", szLang);
		}
	}
	fprintf(stdout, "\n");
	/*check OCI (not everything interests us) - FIXME: support for unicode*/
	count = gf_list_count(odi.od->OCIDescriptors);
	if (count) {
		fprintf(stdout, "%d Object Content Information descriptors in OD\n", count);
		for (i=0; i<count; i++) {
			GF_Descriptor *desc = (GF_Descriptor *) gf_list_get(odi.od->OCIDescriptors, i);
			switch (desc->tag) {
			case GF_ODF_SEGMENT_TAG:
			{
				GF_Segment *sd = (GF_Segment *) desc;
				fprintf(stdout, "Segment Descriptor: Name: %s - start time %g sec - duration %g sec\n", sd->SegmentName, sd->startTime, sd->Duration);
			}
				break;
			case GF_ODF_CC_NAME_TAG:
			{
				GF_CC_Name *ccn = (GF_CC_Name *)desc;
				fprintf(stdout, "Content Creators:\n");
				for (j=0; j<gf_list_count(ccn->ContentCreators); j++) {
					GF_ContentCreatorInfo *ci = (GF_ContentCreatorInfo *) gf_list_get(ccn->ContentCreators, j);
					if (!ci->isUTF8) continue;
					fprintf(stdout, "\t%s\n", ci->contentCreatorName);
				}
			}
				break;

			case GF_ODF_SHORT_TEXT_TAG:
				{
					GF_ShortTextual *std = (GF_ShortTextual *)desc;
					fprintf(stdout, "Description:\n\tEvent: %s\n\t%s\n", std->eventName, std->eventText);
				}
				break;
			default:
				break;
			}
		}
		fprintf(stdout, "\n");
	}

	switch (odi.status) {
	case 0: fprintf(stdout, "Stopped - "); break;
	case 1: fprintf(stdout, "Playing - "); break;
	case 2: fprintf(stdout, "Paused - "); break;
	case 3: fprintf(stdout, "Not setup yet\n"); return;
	default: fprintf(stdout, "Setup Failed\n"); return;
	}
	if (odi.buffer>=0) fprintf(stdout, "Buffer: %d ms - ", odi.buffer);
	else fprintf(stdout, "Not buffering - ");
	fprintf(stdout, "Clock drift: %d ms\n", odi.clock_drift);
	if (odi.db_unit_count) fprintf(stdout, "%d AU in DB\n", odi.db_unit_count);
	if (odi.cb_max_count) fprintf(stdout, "Composition Buffer: %d CU (%d max)\n", odi.cb_unit_count, odi.cb_max_count);
	fprintf(stdout, "\n");

	if (odi.owns_service) {
		const char *url;
		u32 done, total, bps;
		d_enum = 0;
		while (gf_term_get_download_info(term, odm, &d_enum, &url, NULL, &done, &total, &bps)) {
			if (d_enum==1) fprintf(stdout, "Current Downloads in service:\n");
			if (done && total) {
				fprintf(stdout, "%s: %d / %d bytes (%.2f %%) - %.2f kBps\n", url, done, total, (100.0f*done)/total, ((Float)bps)/1024.0f);
			} else {
				fprintf(stdout, "%s: %.2f kbps\n", url, ((Float)8*bps)/1024.0f);
			}
		}
		if (!d_enum) fprintf(stdout, "No Downloads in service\n");
		fprintf(stdout, "\n");
	}
	d_enum = 0;
	while (gf_term_get_channel_net_info(term, odm, &d_enum, &id, &com, &e)) {
		if (e) continue;
		if (!com.bw_down && !com.bw_up) continue;

		fprintf(stdout, "Stream ID %d statistics:\n", id);
		if (com.multiplex_port) {
			fprintf(stdout, "\tMultiplex Port %d - multiplex ID %d\n", com.multiplex_port, com.port);
		} else {
			fprintf(stdout, "\tPort %d\n", com.port);
		}
		fprintf(stdout, "\tPacket Loss Percentage: %.4f\n", com.pck_loss_percentage);
		fprintf(stdout, "\tDown Bandwidth: %d bps\n", com.bw_down);
		if (com.bw_up) fprintf(stdout, "\tUp Bandwidth: %d bps\n", com.bw_up);
		if (com.ctrl_port) {
			if (com.multiplex_port) {
				fprintf(stdout, "\tControl Multiplex Port: %d - Control Multiplex ID %d\n", com.multiplex_port, com.ctrl_port);
			} else {
				fprintf(stdout, "\tControl Port: %d\n", com.ctrl_port);
			}
			fprintf(stdout, "\tDown Bandwidth: %d bps\n", com.ctrl_bw_down);
			fprintf(stdout, "\tUp Bandwidth: %d bps\n", com.ctrl_bw_up);
		}
		fprintf(stdout, "\n");
	}
}

void PrintODTiming(GF_Terminal *term, GF_ObjectManager *odm)
{
	GF_MediaInfo odi;
	if (!odm) return;

	if (gf_term_get_object_info(term, odm, &odi) != GF_OK) return;
	if (!odi.od) {
		fprintf(stdout, "Service not attached\n");
		return;
	}

	fprintf(stdout, "OD %d: ", odi.od->objectDescriptorID);
	switch (odi.status) {
	case 1: fprintf(stdout, "Playing - "); break;
	case 2: fprintf(stdout, "Paused - "); break;
	default: fprintf(stdout, "Stopped - "); break;
	}
	if (odi.buffer>=0) fprintf(stdout, "Buffer: %d ms - ", odi.buffer);
	else fprintf(stdout, "Not buffering - ");
	fprintf(stdout, "Clock drift: %d ms", odi.clock_drift);
	fprintf(stdout, " - time: ");
	PrintTime((u32) (odi.current_time*1000));
	fprintf(stdout, "\n");
}

void PrintODBuffer(GF_Terminal *term, GF_ObjectManager *odm)
{
	Float avg_dec_time;
	GF_MediaInfo odi;
	if (!odm) return;

	if (gf_term_get_object_info(term, odm, &odi) != GF_OK) return;
	if (!odi.od) {
		fprintf(stdout, "Service not attached\n");
		return;
	}

	fprintf(stdout, "OD %d: ", odi.od->objectDescriptorID);
	switch (odi.status) {
	case 1: fprintf(stdout, "Playing"); break;
	case 2: fprintf(stdout, "Paused"); break;
	default: fprintf(stdout, "Stopped"); break;
	}
	if (odi.buffer>=0) fprintf(stdout, " - Buffer: %d ms", odi.buffer);
	if (odi.db_unit_count) fprintf(stdout, " - DB: %d AU", odi.db_unit_count);
	if (odi.cb_max_count) fprintf(stdout, " - CB: %d/%d CUs", odi.cb_unit_count, odi.cb_max_count);
	
	fprintf(stdout, "\n * %d decoded frames - %d dropped frames\n", odi.nb_dec_frames, odi.nb_droped);
	avg_dec_time = 0;
	if (odi.nb_dec_frames) { avg_dec_time = (Float) odi.total_dec_time; avg_dec_time /= odi.nb_dec_frames; }
	fprintf(stdout, " * Avg Bitrate %d kbps (%d max) - Avg Decoding Time %.2f ms (%d max)\n",
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
	fprintf(stdout, "\n");
}


void PrintGPACConfig()
{
	u32 i, j, cfg_count, key_count;
	char szName[200];
	char *secName = NULL;

	fprintf(stdout, "Enter section name (\"*\" for complete dump):\n");
	if (1 > scanf("%s", szName)){
	    fprintf(stderr, "No section name, aborting.\n");
	    return;
	}
	if (strcmp(szName, "*")) secName = szName;

	fprintf(stdout, "\n\n*** GPAC Configuration ***\n\n");

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
		fprintf(stdout, "[%s]\n", sec);
		key_count = gf_cfg_get_key_count(cfg_file, sec);
		for (j=0; j<key_count; j++) {
			const char *key = gf_cfg_get_key_name(cfg_file, sec, j);
			const char *val = gf_cfg_get_key(cfg_file, sec, key);
			fprintf(stdout, "%s=%s\n", key, val);
		}
		fprintf(stdout, "\n");
	}
}

