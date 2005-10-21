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

#ifndef WIN32
#include <pwd.h>
#include <unistd.h>
#else
/*for GetModuleFileName*/
#include <windows.h>
#endif

/*local prototypes*/
void PrintWorldInfo(GF_Terminal *term);
void ViewOD(GF_Terminal *term, u32 OD_ID);
void PrintODList(GF_Terminal *term);
void ViewODs(GF_Terminal *term, Bool show_timing);
void PrintGPACConfig();
/*console handling*/
Bool has_input();
u8 get_a_char();
void set_echo_off(Bool echo_off);

static Bool is_connected = 0;
static Bool display_rti = 0;
static Bool Run;
static u32 Duration;
static Bool CanSeek = 0;
static u32 Volume=100;
static char the_url[GF_MAX_PATH];
static Bool NavigateTo = 0;
static char the_next_url[GF_MAX_PATH];
static GF_Terminal *term;
static GF_Config *cfg_file;


void PrintHelp()
{
	fprintf(stdout, "MP4Client command keys:\n"
		"\to: connect to the specified URL\n"
		"\tO: connect to the specified URL in playlist mode\n"
		"\tN: switch to the next URL in the playlist\n"
		"\tr: restart current presentation\n"
		"\tp: play/pause the presentation\n"
		"\ts: step one frame ahead\n"
		"\tz: seek into presentation\n"
		"\tt: print current timing\n"
		"\n"
		"\tw: view world info\n"
		"\tv: view Object Descriptor list\n"
		"\ti: view Object Descriptor info\n"
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
		"\t2: restart using 2D renderer\n"
		"\t3: restart using 3D renderer\n"
		"\n"
		"\t4: forces 4/3 Aspect Ratio\n"
		"\t5: forces 16/9 Aspect Ratio\n"
		"\t6: forces no Aspect Ratio (always fill screen)\n"
		"\t7: forces original Aspect Ratio (default)\n"
		"\n"
		"\tl: list available modules\n"
		"\tc: prints some GPAC configuration info\n"
		"\tR: toggles run-time info display on/off\n"
		"\tq: exit the application\n"
		"\th: print this message\n"
		"\nStartup Options:\n"
		"\t-c config_path: specifies config file path\n"
		"\n"
		"MP4Client - GPAC command line player - version %s\n"
		"GPAC Written by Jean Le Feuvre (c) 2001-2005 - ENST (c) 2005-200X\n",

		GPAC_VERSION
		);
}



GF_Config *create_default_config(char *file_path, char *file_name)
{
	GF_Config *cfg;
	char szPath[GF_MAX_PATH];
	FILE *f;
	sprintf(szPath, "%s%c%s", file_path, GF_PATH_SEPARATOR, file_name);
	f = fopen(szPath, "wt");
	fprintf(stdout, "create %s: %s\n", szPath, (f==NULL) ? "Error" : "OK");
	if (!f) return NULL;
	fclose(f);

	cfg = gf_cfg_new(file_path, file_name);
	if (!cfg) return NULL;

#ifdef GPAC_MODULES_PATH
	fprintf(stdout, "Using module directory %s \n", GPAC_MODULES_PATH);
	strcpy(szPath, GPAC_MODULES_PATH);
#else
	fprintf(stdout, "Please enter full path to GPAC modules directory:\n");
	scanf("%s", szPath);
#endif
	gf_cfg_set_key(cfg, "General", "ModulesDirectory", szPath);
	gf_cfg_set_key(cfg, "Audio", "ForceConfig", "yes");
	gf_cfg_set_key(cfg, "Audio", "NumBuffers", "2");
	gf_cfg_set_key(cfg, "Audio", "TotalDuration", "120");
	gf_cfg_set_key(cfg, "Audio", "DisableNotification", "no");
	gf_cfg_set_key(cfg, "FontEngine", "DriverName", "ft_font");
	fprintf(stdout, "Please enter full path to a TrueType font directory (.ttf, .ttc):\n");
	scanf("%s", szPath);
	gf_cfg_set_key(cfg, "FontEngine", "FontDirectory", szPath);
	fprintf(stdout, "Please enter full path to a cache directory for HTTP downloads:\n");
	scanf("%s", szPath);
	gf_cfg_set_key(cfg, "General", "CacheDirectory", szPath);
	gf_cfg_set_key(cfg, "Downloader", "CleanCache", "yes");
	gf_cfg_set_key(cfg, "Rendering", "AntiAlias", "All");
	gf_cfg_set_key(cfg, "Rendering", "Framerate", "30");
	/*use power-of-2 emulation*/
	gf_cfg_set_key(cfg, "Render3D", "EmulatePOW2", "yes");
#ifdef WIN32
	gf_cfg_set_key(cfg, "Render2D", "ScalableZoom", "yes");
	gf_cfg_set_key(cfg, "Video", "DriverName", "DirectX Video Output");
#else
#ifdef __DARWIN__
	gf_cfg_set_key(cfg, "Video", "DriverName", "SDL Video Output");
	/*SDL not so fast with scalable zoom*/
	gf_cfg_set_key(cfg, "Render2D", "ScalableZoom", "no");
#else
	gf_cfg_set_key(cfg, "Video", "DriverName", "X11 Video Output");
	/*x11 only supports scalable zoom*/
	gf_cfg_set_key(cfg, "Render2D", "ScalableZoom", "yes");
#endif
#endif
	gf_cfg_set_key(cfg, "Video", "SwitchResolution", "no");
	gf_cfg_set_key(cfg, "Network", "AutoReconfigUDP", "yes");
	gf_cfg_set_key(cfg, "Network", "UDPNotAvailable", "no");
	gf_cfg_set_key(cfg, "Network", "UDPTimeout", "10000");
	gf_cfg_set_key(cfg, "Network", "BufferLength", "3000");

	/*store and reload*/
	gf_cfg_del(cfg);
	return gf_cfg_new(file_path, file_name);
}

static void PrintTime(u32 time)
{
	u32 ms, h, m, s;
	h = time / 1000 / 3600;
	m = time / 1000 / 60 - h*60;
	s = time / 1000 - h*3600 - m*60;
	ms = time - (h*3600 + m*60 + s) * 1000;
	fprintf(stdout, "%02d:%02d:%02d.%02d", h, m, s, ms);
}

static u64 memory_at_gpac_startup = 0;

static void UpdateRTInfo()
{
	GF_Event evt;
	GF_SystemRTInfo rti;
	char szMsg[1024];

	/*refresh every second*/
	if (!display_rti || !gf_sys_get_rti(1000, &rti, 0 /*GF_RTI_ALL_PROCESSES_TIMES | GF_RTI_PROCESS_MEMORY*/) || !rti.sampling_period_duration) return;
	if (!rti.process_memory) rti.process_memory = (u32) (memory_at_gpac_startup-rti.physical_memory_avail);

	sprintf(szMsg, "FPS %02.2f - CPU %02d (%02d) - Mem %d kB", 
		gf_term_get_framerate(term, 0), rti.total_cpu_usage, rti.process_cpu_usage, (u32) (rti.process_memory / 1024) );

	evt.type = GF_EVT_SET_CAPTION;
	evt.caption.caption = szMsg;
	gf_term_user_event(term, &evt);
}

static void ResetCaption()
{
	GF_Event event;
	if (display_rti) return;
	event.type = GF_EVT_SET_CAPTION;
	if (is_connected) {
		char *str = strrchr(the_url, '\\');
		if (!str) str = strrchr(the_url, '/');
		event.caption.caption = str ? str+1 : the_url;
	} else {
		event.caption.caption = "GPAC MP4Client";
	}
	gf_term_user_event(term, &event);
}

Bool GPAC_EventProc(void *ptr, GF_Event *evt)
{
	switch (evt->type) {
	case GF_EVT_DURATION:
		Duration = (u32) (evt->duration.duration*1000);
		CanSeek = evt->duration.can_seek;
		break;
	case GF_EVT_MESSAGE:
	{
		const char *servName;
		if (!evt->message.service || !strcmp(evt->message.service, the_url)) {
			servName = "main service";
		} else {
			servName = evt->message.service;
		}
		if (!evt->message.message) return 0;
		if (evt->message.error) 
			fprintf(stdout, "%s (%s): %s\n", evt->message.message, servName, gf_error_to_string(evt->message.error));
		else
			fprintf(stdout, "(%s) %s\r", servName, evt->message.message);
	}
		break;
	case GF_EVT_PROGRESS:
	{
		char *szTitle = "";
		if (evt->progress.progress_type==0) szTitle = "Buffer ";
		else if (evt->progress.progress_type==1) szTitle = "Download ";
		else if (evt->progress.progress_type==2) szTitle = "Import ";
		gf_cbk_on_progress(szTitle, evt->progress.done, evt->progress.total);
	}
		break;
	
	case GF_EVT_VKEYDOWN:
		if ((evt->key.key_states & GF_KM_ALT)) {
			switch (evt->key.vk_code) {
			case GF_VK_LEFT:
				if (Duration>=2000) {
					s32 res = gf_term_get_time_in_ms(term) - 5*Duration/100;
					if (res<0) res=0;
					fprintf(stdout, "seeking to %.2f %% (", 100*(Float)res / Duration);
					PrintTime(res);
					fprintf(stdout, ")\n");
					gf_term_play_from_time(term, res);
				} 
				break;
			case GF_VK_RIGHT:
				if (Duration>=2000) {
					u32 res = gf_term_get_time_in_ms(term) + 5*Duration/100;
					if (res>=Duration) res = 0;
					fprintf(stdout, "seeking to %.2f %% (", 100*(Float)res / Duration);
					PrintTime(res);
					fprintf(stdout, ")\n");
					gf_term_play_from_time(term, res);
				}
				break;
			/*these 2 are likely not supported by most audio ouput modules*/
			case GF_VK_UP:
				if (Volume!=100) { Volume = MIN(Volume + 5, 100); gf_term_set_option(term, GF_OPT_AUDIO_VOLUME, Volume); } 
				break;
			case GF_VK_DOWN: 
				if (Volume) { Volume = (Volume > 5) ? (Volume-5) : 0; gf_term_set_option(term, GF_OPT_AUDIO_VOLUME, Volume); }
				break;
			}
		} else {
			switch (evt->key.vk_code) {
			case GF_VK_HOME:
				gf_term_set_option(term, GF_OPT_NAVIGATION_TYPE, 1);
				break;
			case GF_VK_ESCAPE:
				gf_term_set_option(term, GF_OPT_FULLSCREEN, !gf_term_get_option(term, GF_OPT_FULLSCREEN));
				break;
			}
		}
		return 0;

	case GF_EVT_LDOUBLECLICK:
		gf_term_set_option(term, GF_OPT_FULLSCREEN, !gf_term_get_option(term, GF_OPT_FULLSCREEN));
		return 0;

	/*we use CTRL and not ALT for keys, since windows shortcuts keypressed with ALT*/
	case GF_EVT_KEYDOWN:
		if (!(evt->key.key_states & GF_KM_CTRL)) return 0;
		switch (evt->key.virtual_code) {
		case 'F':
		case 'f':
			fprintf(stdout, "Rendering rate: %f FPS\n", gf_term_get_framerate(term, 0));
			break;
		case 'R':
		case 'r':
//			gf_term_set_option(term, GF_OPT_FORCE_REDRAW, 1);
			gf_term_set_option(term, GF_OPT_DIRECT_RENDER, !gf_term_get_option(term, GF_OPT_DIRECT_RENDER) );
			break;
		case '4': gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_4_3); break;
		case '5': gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_16_9); break;
		case '6': gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_FILL_SCREEN); break;
		case '7': gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_KEEP); break;
		case 'p':
		case 'P':
			gf_term_set_option(term, GF_OPT_PLAY_STATE, (gf_term_get_option(term, GF_OPT_PLAY_STATE)==GF_STATE_PAUSED) ? GF_STATE_PLAYING : GF_STATE_PAUSED);
			break;
		case 's':
		case 'S':
			gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
			break;
		}
		break;

	case GF_EVT_CONNECT:
		if (evt->connect.is_connected) {
			is_connected = 1;
			fprintf(stdout, "Service Connected\n");
		} else {
			fprintf(stdout, "Service %s\n", is_connected ? "Disconnected" : "Connection Failed");
			is_connected = 0;
			Duration = 0;
		}
		ResetCaption();
		break;
	case GF_EVT_QUIT:
		Run = 0;
		break;
	case GF_EVT_NAVIGATE:
		if (gf_term_is_supported_url(term, evt->navigate.to_url, 1, 0)) {
			/*eek that's ugly but I don't want to write an event queue for that*/
			strcpy(the_next_url, evt->navigate.to_url);
			NavigateTo = 1;
			return 1;
		} else {
			fprintf(stdout, "Navigation destination not supported\nGo to URL: %s\n", evt->navigate.to_url);
		}
		break;
	case GF_EVT_AUTHORIZATION:
		if (!strlen(evt->auth.user)) {
			fprintf(stdout, "Authorization required for site %s\n", evt->auth.site_url);
			fprintf(stdout, "login: ");
			scanf("%s", evt->auth.user);
		} else {
			fprintf(stdout, "Authorization required for %s@%s\n", evt->auth.user, evt->auth.site_url);
		}
		fprintf(stdout, "password: ");
		set_echo_off(1);
		scanf("%s", evt->auth.password);
		set_echo_off(0);
		return 1;
	}
	return 0;
}

GF_Config *loadconfigfile()
{
	GF_Config *cfg;
	char szPath[GF_MAX_PATH];

#ifdef WIN32
	char *sep;
	GetModuleFileNameA(NULL, szPath, GF_MAX_PATH);
	sep = strrchr(szPath, '\\');
	if (sep) sep[1] = 0;

	cfg = gf_cfg_new(szPath, "GPAC.cfg");
	if (cfg) goto success;
	strcpy(szPath, ".");
	cfg = gf_cfg_new(szPath, "GPAC.cfg");
	if (cfg) goto success;
	strcpy(szPath, "C:\\Program Files\\GPAC");
	cfg = gf_cfg_new(szPath, "GPAC.cfg");
	if (cfg) goto success;
	strcpy(szPath, ".");
	cfg = gf_cfg_new(szPath, "GPAC.cfg");
	if (cfg) goto success;
	cfg = create_default_config(szPath, "GPAC.cfg");
#else
	/*linux*/
	char *cfg_dir = getenv("HOME");
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
	char navstr[20], nav;
	u32 type = gf_term_get_option(term, GF_OPT_NAVIGATION_TYPE);
	e = GF_OK;
	if (!type) {
		fprintf(stdout, "Content/renderer doesn't allow user-selectable navigation\n");
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



int main (int argc, char **argv)
{
	const char *str;
	u32 i, url_arg;
	GF_User user;
	GF_SystemRTInfo rti;
	FILE *playlist = NULL;

	/*by default use current dir*/
	strcpy(the_url, ".");

	url_arg = (argc == 2) ? 1 : 0;

	memset(&user, 0, sizeof(GF_User));


	cfg_file = loadconfigfile();
	if (argc >= 2) {
		if (!strcmp(argv[1], "-c")) {
			if (argc > 4) {
				fprintf(stdout, "Usage: MP4Client [-c config_path] [url]\n");
				if (cfg_file) gf_cfg_del(cfg_file);
				return 1;
			}
			strcpy(the_url, argv[2]);
			url_arg = (argc == 3) ? 0 : 3;
		} else if (argc >= 3) {
			PrintHelp();
			if (cfg_file) gf_cfg_del(cfg_file);
			return 1;
		}
	}

	if (!cfg_file) {
		fprintf(stdout, "Error: Configuration File \"GPAC.cfg\" not found\n");
		return 1;
	}
	gf_sys_init();
	
	gf_sys_get_rti(500, &rti, GF_RTI_SYSTEM_MEMORY_ONLY);
	memory_at_gpac_startup = rti.physical_memory_avail;

	Run = 1;
	
	fprintf(stdout, "Loading modules ... ");
	str = gf_cfg_get_key(cfg_file, "General", "ModulesDirectory");

	user.modules = gf_modules_new((const unsigned char *) str, cfg_file);
	i = gf_modules_get_count(user.modules);
	if (!i) {
		fprintf(stdout, "Error: no modules found in %s - exiting\n", str);
		gf_modules_del(user.modules);
		gf_cfg_del(cfg_file);
		gf_sys_close();
		return 1;
	}
	fprintf(stdout, "OK (%d found in %s)\n", i, str);

	user.config = cfg_file;
	user.EventProc = GPAC_EventProc;
	/*dummy in this case (global vars) but MUST be non-NULL*/
	user.opaque = user.modules;

	fprintf(stdout, "Loading GPAC Terminal ... ");	
	term = gf_term_new(&user);
	if (!term) {
		fprintf(stdout, "Init error - check you have at least one video out...\nFound modules:\n");
		list_modules(user.modules);
		gf_modules_del(user.modules);
		gf_cfg_del(cfg_file);
		gf_sys_close();
		return 1;
	}
	fprintf(stdout, "OK\n");

	/*check video output*/
	str = gf_cfg_get_key(cfg_file, "Video", "DriverName");
	if (!strcmp(str, "Raw Video Output")) fprintf(stdout, "WARNING: using raw output video (memory only) - no display used\n");
	/*check audio output*/
	str = gf_cfg_get_key(cfg_file, "Audio", "DriverName");
	if (!strcmp(str, "No Audio Output Available")) fprintf(stdout, "WARNING: no audio output availble - make sure no other program is locking the sound card\n");

	fprintf(stdout, "Using %s\n", gf_cfg_get_key(cfg_file, "Rendering", "RendererName"));

	/*connect if requested*/
	if (url_arg) {
		char *ext;
		strcpy(the_url, argv[url_arg]);
		ext = strrchr(the_url, '.');
		if (!stricmp(ext, ".m3u")) {
			fprintf(stdout, "Opening Playlist %s\n", the_url);
			playlist = fopen(the_url, "rt");
			if (playlist) {
				fscanf(playlist, "%s", the_url);
				fprintf(stdout, "Opening URL %s\n", the_url);
				gf_term_connect(term, the_url);
			} else {
				fprintf(stdout, "Hit 'h' for help\n\n");
			}
		} else {
			fprintf(stdout, "Opening URL %s\n", the_url);
			gf_term_connect(term, the_url);
		}
	} else {
		fprintf(stdout, "Hit 'h' for help\n\n");
	}

	while (Run) {
		char c;
		
		if (NavigateTo) {
			fprintf(stdout, "Navigating to URL %s\n", the_next_url);
			NavigateTo = 0;
			gf_term_disconnect(term);
			strcpy(the_url, the_next_url);
			gf_term_connect(term, the_url);
		}

		/*we don't want getchar to block*/
		if (!has_input()) {
			UpdateRTInfo();
			gf_sleep(250);
			continue;
		}
		c = get_a_char();

		switch (c) {
		case 'q':
			Run = 0;
			break;
		case 'o':
			gf_term_disconnect(term);
			fprintf(stdout, "Enter the absolute URL\n");
			scanf("%s", the_url);
			gf_term_connect(term, the_url);
			break;
		case 'O':
			gf_term_disconnect(term);
			fprintf(stdout, "Enter the absolute URL to the playlist\n");
			scanf("%s", the_url);
			playlist = fopen(the_url, "rt");
			if (playlist) {
				fscanf(playlist, "%s", the_url);
				fprintf(stdout, "Opening URL %s\n", the_url);
				gf_term_connect(term, the_url);
			}
			break;
		case '\n':
		case 'N':
			if (playlist) {
				gf_term_disconnect(term);
				fscanf(playlist, "%s", the_url);
				fprintf(stdout, "Opening URL %s\n", the_url);
				gf_term_connect(term, the_url);
			}
			break;
		case 'M':
			if (playlist) {
				u32 count;
				gf_term_disconnect(term);
				scanf("%d", &count);
				while (count) {
					fscanf(playlist, "%s", the_url);
					count--;
				}
				fprintf(stdout, "Opening URL %s\n", the_url);
				gf_term_connect(term, the_url);
			}
			break;
		case 'r':
			if (is_connected) {
				gf_term_disconnect(term);
				gf_term_connect(term, the_url);
			}
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
				res = gf_term_get_time_in_ms(term) * 100;
				res /= Duration;
				fprintf(stdout, " (current %.2f %%)\nEnter Seek percentage:\n", res);
				if (scanf("%d", &seekTo) == 1) { 
					if (seekTo > 100) seekTo = 100;
					gf_term_play_from_time(term, seekTo*Duration/100);
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
			if (is_connected) PrintODList(term);
			break;
		case 'i':
			if (is_connected) {
				u32 ID;
				fprintf(stdout, "Enter OD ID (0 for main OD): ");
				scanf("%d", &ID);
				ViewOD(term, ID);
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
				char file[GF_MAX_PATH], *sExt;
				GF_Err e;
				Bool xml_dump, std_out;
				fprintf(stdout, "Enter file radical name (+\'.x\' for XML dumping) - \"std\" for stdout: ");
				scanf("%s", file);
				sExt = strrchr(file, '.');
				xml_dump = 0;
				if (sExt) {
					if (!stricmp(sExt, ".x")) xml_dump = 1;
					sExt[0] = 0;
				}
				std_out = strnicmp(file, "std", 3) ? 0 : 1;
				e = gf_term_dump_scene(term, std_out ? NULL : file, xml_dump, 0, NULL);
				fprintf(stdout, "Dump done (%s)\n", gf_error_to_string(e));
			}
			break;

		case 'c':
			PrintGPACConfig();
			break;
		case '2':
		case '3':
		{
			u32 now = gf_term_get_time_in_ms(term);
			Bool reconnect = is_connected;
			str = gf_cfg_get_key(cfg_file, "Rendering", "RendererName");
			if (strstr(str, "2D") && (c=='2')) { fprintf(stdout, "Already using 2D Renderer\n"); break; }
			if (strstr(str, "3D") && (c=='3')) { fprintf(stdout, "Already using 3D Renderer\n"); break; }
			gf_term_del(term);
			gf_cfg_set_key(cfg_file, "Rendering", "RendererName", (c=='2') ? "GPAC 2D Renderer" : "GPAC 3D Renderer");
			term = gf_term_new(&user);
			if (!term) {
				fprintf(stdout, "Error reloading renderer - aborting\n");
				goto exit;
			}
			fprintf(stdout, "Using %s\n", gf_cfg_get_key(cfg_file, "Rendering", "RendererName"));
			if (reconnect) gf_term_connect_from_time(term, the_url, now);
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
				fprintf(stdout, "Streaming Cache stoped\n");
			} else {
				fprintf(stdout, "Streaming Cache not running\n");
			}
			break;
		case 'R':
			display_rti = !display_rti;
			ResetCaption();
			break;

		case 'h':
			PrintHelp();
			break;
		default:
			break;
		}
	}

	fprintf(stdout, "Deleting terminal...\n");
	if (playlist) fclose(playlist);
	gf_term_del(term);
exit:
	fprintf(stdout, "Unloading modules...\n");
	gf_modules_del(user.modules);
	gf_cfg_del(cfg_file);
	gf_sys_close();
	fprintf(stdout, "goodbye\n");
	return 0;
}




void PrintWorldInfo(GF_Terminal *term)
{
	u32 i;
	char *title;
	GF_List *descs;
	descs = gf_list_new();
	title = gf_term_get_world_info(term, NULL, descs);
	if (!title) {
		fprintf(stdout, "No World Info available\n");
	} else {
		fprintf(stdout, "\t%s\n", title);
		for (i=0; i<gf_list_count(descs); i++) {
			char *str = gf_list_get(descs, i);
			fprintf(stdout, "%s\n", str);
			free(str);
		}
	}
	gf_list_del(descs);
}

void PrintODList(GF_Terminal *term)
{
	ODInfo odi;
	u32 i, count;
	GF_ObjectManager *odm, *root_odm = gf_term_get_root_object(term);
	if (!root_odm) return;
	if (gf_term_get_object_info(term, root_odm, &odi) != GF_OK) return;
	if (!odi.od) {
		fprintf(stdout, "Service not attached\n");
		return;
	}
	fprintf(stdout, "Currently loaded objects:\n");
	fprintf(stdout, "\tRootOD ID %d\n", odi.od->objectDescriptorID);

	count = gf_term_get_object_count(term, root_odm);
	for (i=0; i<count; i++) {
		odm = gf_term_get_object(term, root_odm, i);
		if (!odm) break;
		if (gf_term_get_object_info(term, odm, &odi) == GF_OK) 
			fprintf(stdout, "\t\tOD %d - ID %d (%s)\n", i+1, odi.od->objectDescriptorID, 
			(odi.od_type==GF_STREAM_VISUAL) ? "Video" : (odi.od_type==GF_STREAM_AUDIO) ? "Audio" : "Systems");
	}
}

void ViewOD(GF_Terminal *term, u32 OD_ID)
{
	ODInfo odi;
	u32 i, j, count, d_enum,id;
	GF_Err e;
	char code[5];
	NetStatCommand com;
	GF_ObjectManager *odm, *root_odm = gf_term_get_root_object(term);
	if (!root_odm) return;

	odm = NULL;
	if (!OD_ID) {
		odm = root_odm;
		if ((gf_term_get_object_info(term, odm, &odi) != GF_OK)) odm=NULL;
	} else {
		count = gf_term_get_object_count(term, root_odm);
		for (i=0; i<count; i++) {
			odm = gf_term_get_object(term, root_odm, i);
			if (!odm) break;
			if ((gf_term_get_object_info(term, odm, &odi) == GF_OK) && (odi.od->objectDescriptorID == OD_ID)) break;
			odm = NULL;
		}
	}
	if (!odm) {
		fprintf(stdout, "cannot find OD with ID %d\n", OD_ID);
		return;
	}
	if (!odi.od) {
		fprintf(stdout, "Object %d not attached yet\n", OD_ID);
		return;
	}

	while (odi.od && odi.od->URLString) {
		fprintf(stdout, "OD %d points to %s\n", odi.od->objectDescriptorID, odi.od->URLString);
		odm = gf_term_get_remote_object(term, odm);
		if (!odm) return;
		if (gf_term_get_object_info(term, odm, &odi) != GF_OK) return;
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
			case 0x20: fprintf(stdout, "MPEG-4\n"); break;
			case 0x60: fprintf(stdout, "MPEG-2 Simple Profile\n"); break;
			case 0x61: fprintf(stdout, "MPEG-2 Main Profile\n"); break;
			case 0x62: fprintf(stdout, "MPEG-2 SNR Profile\n"); break;
			case 0x63: fprintf(stdout, "MPEG-2 Spatial Profile\n"); break;
			case 0x64: fprintf(stdout, "MPEG-2 High Profile\n"); break;
			case 0x65: fprintf(stdout, "MPEG-2 422 Profile\n"); break;
			case 0x6A: fprintf(stdout, "MPEG-1\n"); break;
			case 0x6C: fprintf(stdout, "JPEG\n"); break;
			case 0x6D: fprintf(stdout, "PNG\n"); break;
			case 0x80:
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
			case 0x40: fprintf(stdout, "MPEG-4\n"); break;
			case 0x66: fprintf(stdout, "MPEG-2 AAC Main Profile\n"); break;
			case 0x67: fprintf(stdout, "MPEG-2 AAC LowComplexity Profile\n"); break;
			case 0x68: fprintf(stdout, "MPEG-2 AAC Scalable Sampling Rate Profile\n"); break;
			case 0x69: fprintf(stdout, "MPEG-2 Audio\n"); break;
			case 0x6B: fprintf(stdout, "MPEG-1 Audio\n"); break;
			case 0xA0: fprintf(stdout, "EVRC Audio\n"); break;
			case 0xA1: fprintf(stdout, "SMV Audio\n"); break;
			case 0xE1: fprintf(stdout, "QCELP Audio\n"); break;
			case 0x80:
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
		if (esd->langDesc) fprintf(stdout, "\tStream Language: %c%c%c\n", (char) ((esd->langDesc->langCode>>16)&0xFF), (char) ((esd->langDesc->langCode>>8)&0xFF), (char) (esd->langDesc->langCode & 0xFF) );
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
	case 0: fprintf(stdout, "Stoped - "); break;
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
	ODInfo odi;
	if (!odm) return;

	if (gf_term_get_object_info(term, odm, &odi) != GF_OK) return;
	while (odi.od && odi.od->URLString) {
		odm = gf_term_get_remote_object(term, odm);
		if (!odm) return;
		if (gf_term_get_object_info(term, odm, &odi) != GF_OK) return;
	}
	if (!odi.od) {
		fprintf(stdout, "Service not attached\n");
		return;
	}

	fprintf(stdout, "OD %d: ", odi.od->objectDescriptorID);
	switch (odi.status) {
	case 1: fprintf(stdout, "Playing - "); break;
	case 2: fprintf(stdout, "Paused - "); break;
	default: fprintf(stdout, "Stoped - "); break;
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
	ODInfo odi;
	if (!odm) return;

	if (gf_term_get_object_info(term, odm, &odi) != GF_OK) return;
	while (odi.od && odi.od->URLString) {
		odm = gf_term_get_remote_object(term, odm);
		if (!odm) return;
		if (gf_term_get_object_info(term, odm, &odi) != GF_OK) return;
	}
	if (!odi.od) {
		fprintf(stdout, "Service not attached\n");
		return;
	}

	fprintf(stdout, "OD %d: ", odi.od->objectDescriptorID);
	switch (odi.status) {
	case 1: fprintf(stdout, "Playing"); break;
	case 2: fprintf(stdout, "Paused"); break;
	default: fprintf(stdout, "Stoped"); break;
	}
	if (odi.buffer>=0) fprintf(stdout, " - Buffer: %d ms", odi.buffer);
	if (odi.db_unit_count) fprintf(stdout, " - DB: %d AU", odi.db_unit_count);
	if (odi.cb_max_count) fprintf(stdout, " - CB: %d/%d CUs", odi.cb_unit_count, odi.cb_max_count);
	
	avg_dec_time = 0;
	if (odi.nb_dec_frames) { avg_dec_time = (Float) odi.total_dec_time; avg_dec_time /= odi.nb_dec_frames; }
	fprintf(stdout, "\n * Avg Bitrate %d kbps (%d max) - Avg Decoding Time %.2f ms (%d max)\n",
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
	scanf("%s", szName);
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

/*seems OK under mingw also*/
#ifdef WIN32
#include <conio.h>
#include <windows.h>
Bool has_input()
{
	return kbhit();
}
u8 get_a_char()
{
	return getchar();
}
void set_echo_off(Bool echo_off) 
{
	DWORD flags;
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(hStdin, &flags);
	if (echo_off) flags &= ~ENABLE_ECHO_INPUT;
	else flags |= ENABLE_ECHO_INPUT;
	SetConsoleMode(hStdin, flags);
}
#else
/*linux kbhit/getchar- borrowed on debian mailing lists, (author Mike Brownlow)*/
#include <termios.h>

static struct termios t_orig, t_new;
static s32 ch_peek = -1;

void init_keyboard()
{
	tcgetattr(0, &t_orig);
	t_new = t_orig;
	t_new.c_lflag &= ~ICANON;
	t_new.c_lflag &= ~ECHO;
	t_new.c_lflag &= ~ISIG;
	t_new.c_cc[VMIN] = 1;
	t_new.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &t_new);
}
void close_keyboard(Bool new_line)
{
	tcsetattr(0,TCSANOW, &t_orig);
	if (new_line) fprintf(stdout, "\n");
}

void set_echo_off(Bool echo_off) 
{ 
	init_keyboard();
	if (echo_off) t_orig.c_lflag &= ~ECHO;
	else t_orig.c_lflag |= ECHO;
	close_keyboard(0);
}

Bool has_input()
{
	u8 ch;
	s32 nread;

	init_keyboard();
	if (ch_peek != -1) return 1;
	t_new.c_cc[VMIN]=0;
	tcsetattr(0, TCSANOW, &t_new);
	nread = read(0, &ch, 1);
	t_new.c_cc[VMIN]=1;
	tcsetattr(0, TCSANOW, &t_new);
	if(nread == 1) {
		ch_peek = ch;
		return 1;
	}
	close_keyboard(0);
	return 0;
}

u8 get_a_char()
{
	u8 ch;
	if (ch_peek != -1) {
		ch = ch_peek;
		ch_peek = -1;
		close_keyboard(1);
		return ch;
	}
	read(0,&ch,1);
	close_keyboard(1);
	return ch;
}

#endif

