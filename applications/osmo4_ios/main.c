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
#include <gpac/events.h>
#include <gpac/modules/service.h>
#include <gpac/internal/terminal_dev.h>

#include <pwd.h>
#include <unistd.h>

#include "sensors_def.h"

void PrintAVInfo(Bool final);

static Bool restart = 0;
static Bool not_threaded = 1;

static Bool no_audio = 0;
static Bool no_regulation = 0;
Bool is_connected = 0;
Bool startup_file = 0;
GF_User user;
GF_Terminal *term;
u64 Duration;
GF_Err last_error = GF_OK;

static GF_Config *cfg_file;
static u32 display_rti = 0;
static Bool Run;
static Bool CanSeek = 0;
static char the_url[GF_MAX_PATH];
static Bool no_mime_check = 1;
static Bool be_quiet = 0;
static u32 log_time_start = 0;

static u32 forced_width=0;
static u32 forced_height=0;
static u32 bench_mode = 0;
static u32 bench_mode_start = 0;
static u32 paused_by_bench = 0;

/*windowless options*/
u32 align_mode = 0;
u32 init_w = 0;
u32 init_h = 0;
u32 last_x, last_y;
Bool right_down = 0;

static u32 rti_update_time_ms = 200;
static FILE *rti_logs = NULL;
static u64 memory_at_gpac_startup = 0;

static void UpdateRTInfo(const char *legend)
{
	GF_SystemRTInfo rti;
	if (paused_by_bench==1) return;

	/*refresh every second*/
	if (!display_rti && !rti_logs) return;
	if (!gf_sys_get_rti(rti_update_time_ms, &rti, 0) && !legend)
		return;

	if (display_rti) {
		if (display_rti==2) {
			char szMsg[1024];

			if (rti.total_cpu_usage && (bench_mode<2) ) {
				sprintf(szMsg, "FPS %02.02f CPU %2d (%02d) Mem %d kB",
			        gf_term_get_framerate(term, 0), rti.total_cpu_usage, rti.process_cpu_usage, (u32) (rti.gpac_memory / 1024));
			} else {
				sprintf(szMsg, "FPS %02.02f CPU %02d Mem %d kB",
			        gf_term_get_framerate(term, 0), rti.process_cpu_usage, (u32) (rti.gpac_memory / 1024) );
			}

			PrintAVInfo(GF_FALSE);


			fprintf(stderr, "%s\r", szMsg);
		} else {
			if (!rti.process_memory) rti.process_memory = (u32) (memory_at_gpac_startup-rti.physical_memory_avail);

			if (!rti.gpac_memory) rti.gpac_memory = (u32) (memory_at_gpac_startup-rti.physical_memory_avail);
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


void *gyro_dev = NULL;

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
		gf_term_user_event(term, &evt);
		return;
	default:
		return;
	}
}

static s32 request_next_playlist_item = GF_FALSE;
FILE *playlist = NULL;

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
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s (%s): %s\n", evt->message.message, servName, gf_error_to_string(evt->message.error) ));
		} else if (!be_quiet) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("(%s) %s\r", servName, evt->message.message ));
		}
	}
	break;
	case GF_EVENT_PROGRESS:
		if (bench_mode && (evt->progress.progress_type==1) ) {

			fprintf(stderr, "Bench mode paused - downloading %02d %%\r", (u32) (evt->progress.done*100.0/evt->progress.total) );
			if (!paused_by_bench) {
				gf_term_set_option(term, GF_OPT_VIDEO_BENCH, 0);
				gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
				paused_by_bench=1;
			} else if (evt->progress.done == evt->progress.total) {
				gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
				gf_term_set_option(term, GF_OPT_VIDEO_BENCH, (bench_mode==3) ? 2 : 1);
				bench_mode_start = gf_sys_clock();
				paused_by_bench=2;
			}
		}
		break;

	case GF_EVENT_CONNECT:
		if (evt->connect.is_connected) {
			is_connected = 1;
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Service Connected\n"));
		} else if (is_connected) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Service %s\n", is_connected ? "Disconnected" : "Connection Failed"));
			is_connected = 0;
			Duration = 0;
		}
		if (init_w && init_h) {
			gf_term_set_size(term, init_w, init_h);
		}
		ResetCaption();
		break;

	case GF_EVENT_QUIT:
		Run = 0;
		break;
	case GF_EVENT_DISCONNECT:
		gf_term_disconnect(term);
		break;

	case GF_EVENT_NAVIGATE:
		if (gf_term_is_supported_url(term, evt->navigate.to_url, 1, no_mime_check)) {
			strcpy(the_url, evt->navigate.to_url);
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Navigating to URL %s\n", the_url));
			gf_term_navigate_to(term, evt->navigate.to_url);
			return 1;
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Navigation destination not supported\nGo to URL: %s\n", evt->navigate.to_url ));
		}
		break;
	case GF_EVENT_DBLCLICK:
		if (playlist) {
			request_next_playlist_item=GF_TRUE;
		}
		break;
	case GF_EVENT_SENSOR_REQUEST:
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
	return 0;
}


static void on_gpac_log(void *cbk, GF_LOG_Level ll, GF_LOG_Tool lm, const char *fmt, va_list list)
{
	FILE *logs = cbk;

	if (rti_logs && (lm & GF_LOG_RTI)) {
		char szMsg[2048];
		vsnprintf(szMsg, 2048, fmt, list);
		UpdateRTInfo(szMsg + 6 /*"[RTI] "*/);
	} else {
		if (log_time_start) fprintf(logs, "[At %d]", gf_sys_clock() - log_time_start);
		vfprintf(logs, fmt, list);
		fflush(logs);
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
			gf_log_set_callback(NULL, on_gpac_log);
			gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_ERROR);
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
	{
		const size_t sepIdx = sep - opt_string;
		strncpy(szSec, opt_string, sepIdx);
		szSec[sepIdx] = 0;
	}
	sep ++;
	sep2 = strchr(sep, '=');
	if (!sep2) {
		fprintf(stderr, "Badly formatted option %s - expected Section:Name=Value\n", opt_string);
		return;
	}
	{
		const size_t sepIdx = sep2 - sep;
		strncpy(szKey, sep, sepIdx);
		szKey[sepIdx] = 0;
		strcpy(szVal, sep2+1);
	}

	if (!stricmp(szKey, "*")) {
		if (stricmp(szVal, "null")) {
			fprintf(stderr, "Badly formatted option %s - expected Section:*=null\n", opt_string);
			return;
		}
		gf_cfg_del_section(cfg_file, szSec);
		return;
	}

	if (!stricmp(szVal, "null")) {
		szVal[0]=0;
	}
	gf_cfg_set_key(cfg_file, szSec, szKey, szVal[0] ? szVal : NULL);
}


static void on_progress_null(const void *_ptr, const char *_title, u64 done, u64 total)
{
}
#ifdef GPAC_IPHONE
int SDL_main (int argc, char *argv[])
#else
int main (int argc, char *argv[])
#endif
{
	const char *str;
	char *ext;
	u32 i;
	u32 url_idx_plus_1 = 0;
	u32 simulation_time = 0;
	Bool auto_exit = 0;
	Bool use_gui = 0;
	Bool use_rtix = 0;
    GF_MemTrackerType mem_track = GF_MemTrackerNone;

	Bool ret, fill_ar, visible;
	Bool logs_set = GF_FALSE;
	char *url_arg, *the_cfg, *rti_file;
	const char *logs_settings = NULL;
	GF_SystemRTInfo rti;
	FILE *logfile = NULL;
	/*by default use current dir*/
	strcpy(the_url, ".");

	memset(&user, 0, sizeof(GF_User));

	fill_ar = visible = 0;
	url_arg = the_cfg = rti_file = NULL;

#ifdef GPAC_MEMORY_TRACKING
	for (i=1; i<argc; i++) {
		char *arg = argv[i];
		if (!strcmp(arg, "-mem-track") || !strcmp(arg, "-mem-track-stack")) {
            mem_track = !strcmp(arg, "-mem-track-stack") ? GF_MemTrackerBackTrace : GF_MemTrackerSimple;
		}
	}
#endif

	gf_sys_init(mem_track);
	gf_set_progress_callback(NULL, on_progress_null);

	cfg_file = gf_cfg_init(the_cfg, NULL);
	if (!cfg_file) {
		fprintf(stderr, "Error: Configuration File \"GPAC.cfg\" not found\n");
		if (logfile) gf_fclose(logfile);
		return 1;
	}

	for (i=1; i<argc; i++) {
		char *arg = argv[i];
		if (arg[0] != '-') {
			url_arg = arg;
			url_idx_plus_1 = i+1;
		}
		else if (!strcmp(arg, "-logs")) {
			logs_settings = argv[i+1];
			i++;
		}
		else if (!strcmp(arg, "-lf")) {
			logfile = gf_fopen(argv[i+1], "wt");
			gf_log_set_callback(logfile, on_gpac_log);
			i++;
		}
#ifndef GPAC_MEMORY_TRACKING
		else if (!strcmp(arg, "-mem-track") || !strcmp(arg, "-mem-track-stack")) {
			fprintf(stderr, "WARNING - GPAC not compiled with Memory Tracker - ignoring \"%s\"\n", arg);
		}
#endif
		else if (!strcmp(arg, "-no-audio")) no_audio = GF_TRUE;
		else if (!strcmp(arg, "-bench")) bench_mode = 1;
		else if (!strcmp(arg, "-vbench")) bench_mode = 2;
		else if (!strcmp(arg, "-sbench")) bench_mode = 3;
		else if (!strcmp(arg, "-gui") || !strcmp(arg, "-guid")) use_gui = 1;
		else if (!strcmp(arg, "-opt")) {
			set_cfg_option(argv[i+1]);
			i++;
		}
		else if (!strcmp(arg, "-run-for")) {
			simulation_time = atoi(argv[i+1]);
			i++;
		}
 	}

	if (logs_settings) {
		if (gf_log_set_tools_levels(logs_settings) != GF_OK) {
			return 1;
		}
		logs_set = GF_TRUE;
	}

	if (!logs_set)
		gf_log_set_tools_levels( gf_cfg_get_key(cfg_file, "General", "Logs") );

	gf_cfg_set_key(cfg_file, "Compositor", "OpenGLMode", "hybrid");

	if (!logfile) {
		const char *opt = gf_cfg_get_key(cfg_file, "General", "LogFile");
		if (opt) {
			logfile = gf_fopen(opt, "wt");
			if (logfile)
				gf_log_set_callback(logfile, on_gpac_log);
		}
	}

	gf_sys_get_rti(500, &rti, GF_RTI_SYSTEM_MEMORY_ONLY);
	memory_at_gpac_startup = rti.physical_memory_avail;
	if (rti_file) init_rti_logs(rti_file, url_arg, use_rtix);

	init_w = forced_width;
	init_h = forced_height;

	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Loading modules\n" ));
	str = gf_cfg_get_key(cfg_file, "General", "ModulesDirectory");

	user.modules = gf_modules_new((const char *) str, cfg_file);
	if (user.modules) i = gf_modules_get_count(user.modules);
	if (!i || !user.modules) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Error: no modules found in %s - exiting\n", str));
		if (user.modules) gf_modules_del(user.modules);
		gf_cfg_del(cfg_file);
		gf_sys_close();
		if (logfile) gf_fclose(logfile);
		return 1;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Modules Loaded (%d found in %s)\n", i, str));

//	url_arg="$IOS_DOCS/NBA_score_table_2_hd.mp4#LIVE360TV";
	gf_cfg_set_key(cfg_file, "Compositor", "NumViews", "1");
//	gf_cfg_set_key(cfg_file, "Compositor", "StereoType", "SideBySide");

	if (url_arg && !strncmp(url_arg, "$IOS_DOCS", 9)) {
		char *path = (char *) gf_cfg_get_key(cfg_file, "General", "iOSDocumentsDir");
		if (path) {
			strcpy(the_url, path);
			path = strchr(url_arg, '/');
			if (path) {
				strcat(the_url, path+1);
				url_arg = the_url;
				argv[url_idx_plus_1 - 1] = url_arg;
			}
		}
	}
	gf_sys_set_args(argc, (const char **) argv);

	user.config = cfg_file;
	user.EventProc = GPAC_EventProc;
	/*dummy in this case (global vars) but MUST be non-NULL*/
	user.opaque = user.modules;
	if (not_threaded) user.init_flags |= GF_TERM_NO_COMPOSITOR_THREAD;
	if (no_audio) user.init_flags |= GF_TERM_NO_AUDIO;
	if (no_regulation) user.init_flags |= GF_TERM_NO_REGULATION;
//	user.init_flags |= GF_TERM_NO_DECODER_THREAD;

	if (bench_mode) {
		gf_cfg_discard_changes(user.config);
		auto_exit = GF_TRUE;
		gf_cfg_set_key(user.config, "Audio", "DriverName", "Raw Audio Output");
		if (bench_mode!=2) {
			gf_cfg_set_key(user.config, "Video", "DriverName", "Raw Video Output");
			gf_cfg_set_key(user.config, "RAWVideo", "RawOutput", "null");
			gf_cfg_set_key(user.config, "Compositor", "OpenGLMode", "disable");
		} else {
			gf_cfg_set_key(user.config, "Video", "DisableVSync", "yes");
		}
	}


	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Loading GPAC Terminal\n"));
	term = gf_term_new(&user);
	if (!term) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("\nInit error - check you have at least one video out and one rasterizer...\nFound modules:\n"));
		gf_modules_del(user.modules);
		gf_cfg_del(cfg_file);
		gf_sys_close();
		if (logfile) gf_fclose(logfile);
		return 1;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Terminal Loaded\n"));

	if (bench_mode) {
		display_rti = 2;
		gf_term_set_option(term, GF_OPT_VIDEO_BENCH, (bench_mode==3) ? 2 : 1);
		if (bench_mode==1) bench_mode=2;
	} else {
		/*check video output*/
		str = gf_cfg_get_key(cfg_file, "Video", "DriverName");
		if (!strcmp(str, "Raw Video Output")) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("WARNING: using raw output video (memory only) - no display used\n"));
		}
	}

	/*check audio output*/
	str = gf_cfg_get_key(cfg_file, "Audio", "DriverName");
	if (!str || !strcmp(str, "No Audio Output Available")) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("WARNING: no audio output availble - make sure no other program is locking the sound card\n"));
	}
	str = gf_cfg_get_key(cfg_file, "General", "NoMIMETypeFetch");
	no_mime_check = (str && !stricmp(str, "yes")) ? 1 : 0;

	str = gf_cfg_get_key(cfg_file, "HTTPProxy", "Enabled");
	if (str && !strcmp(str, "yes")) {
		str = gf_cfg_get_key(cfg_file, "HTTPProxy", "Name");
		if (str) GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("HTTP Proxy %s enabled\n", str));
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


	ext = url_arg ? strrchr(url_arg, '.') : NULL;
	if (ext && (!stricmp(ext, ".m3u") || !stricmp(ext, ".pls"))) {
		char pl_path[GF_MAX_PATH];
		GF_Err e = GF_OK;
		fprintf(stderr, "Opening Playlist %s\n", url_arg);

		strcpy(pl_path, url_arg);
		/*this is not clean, we need to have a plugin handle playlist for ourselves*/
		if (!strncmp("http:", url_arg, 5)) {
			GF_DownloadSession *sess = gf_dm_sess_new(term->downloader, url_arg, GF_NETIO_SESSION_NOT_THREADED, NULL, NULL, &e);
			if (sess) {
				e = gf_dm_sess_process(sess);
				if (!e) strcpy(pl_path, gf_dm_sess_get_cache_name(sess));
				gf_dm_sess_del(sess);
			}
		}

		playlist = e ? NULL : gf_fopen(pl_path, "rt");
		if (playlist) {
			request_next_playlist_item = GF_TRUE;
		} else {
			if (e)
				fprintf(stderr, "Failed to open playlist %s: %s\n", url_arg, gf_error_to_string(e) );

			fprintf(stderr, "Hit 'h' for help\n\n");
		}
	} else if (!use_gui && url_arg) {
		gf_term_connect(term, url_arg);
	} else {
		str = gf_cfg_get_key(cfg_file, "General", "StartupFile");
		if (str) {
			if (!url_arg)
				strcpy(the_url, "MP4Client "GPAC_FULL_VERSION);
			gf_term_connect(term, str);
			startup_file = 1;
		}
	}

	/*force fullscreen*/
	gf_term_set_option(term, GF_OPT_FULLSCREEN, 1);

	if (bench_mode) {
		rti_update_time_ms = 500;
		bench_mode_start = gf_sys_clock();
	}

	while (Run) {
		if (restart) {
			restart = 0;
			gf_term_play_from_time(term, 0, 0);
		}

		if (request_next_playlist_item) {
			char szPath[GF_MAX_PATH];

			request_next_playlist_item=0;
			gf_term_disconnect(term);

			if (fscanf(playlist, "%s", szPath) == EOF) {
				fprintf(stderr, "No more items - exiting\n");
				Run = 0;
			} else if (szPath[0] == '#') {
				request_next_playlist_item = GF_TRUE;
			} else {
				fprintf(stderr, "Opening URL %s\n", szPath);
				gf_term_connect_with_path(term, szPath, url_arg);
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
	}

	if (bench_mode) {
		PrintAVInfo(GF_TRUE);
	}

	gf_term_disconnect(term);
	if (rti_file) UpdateRTInfo("Disconnected\n");

	if (gyro_dev) {
		sensor_stop(gyro_dev);
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Deleting terminal... "));
	gf_term_del(term);
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("OK\n"));

	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("GPAC cleanup ...\n"));
	gf_modules_del(user.modules);
	gf_cfg_del(cfg_file);

	if (gyro_dev) {
		sensor_destroy(&gyro_dev);
	}

#ifdef GPAC_MEMORY_TRACKING
	if (mem_track && (gf_memory_size() || gf_file_handles_count() )) {
		gf_memory_print();
	}
#endif

	gf_sys_close();
	if (rti_logs) gf_fclose(rti_logs);
	if (logfile) gf_fclose(logfile);
	if (playlist) gf_fclose(playlist);

	exit(0);
	return 0;
}

static GF_ObjectManager *video_odm = NULL;
static GF_ObjectManager *audio_odm = NULL;
static GF_ObjectManager *scene_odm = NULL;
static u32 last_odm_count = 0;
void PrintAVInfo(Bool final)
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
				if (!video_odm && (v_odi.od_type == GF_STREAM_VISUAL) && (v_odi.raw_media || (v_odi.cb_max_count>1) || v_odi.direct_video_memory) ) {
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
			fprintf(stderr, "%s SR %d num channels %d bpp %d duration %.2fs\n", a_odi.codec_name, a_odi.sample_rate, a_odi.num_channels, a_odi.bits_per_sample, a_odi.duration);
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
		fprintf(stderr, "%d f FPS %.2f (%.2f ms max) rate %d ", v_odi.nb_dec_frames, ((Float)v_odi.nb_dec_frames*1000) / tot_time, v_odi.max_dec_time/1000.0, (u32) v_odi.instant_bitrate/1000);
	}
	else if (scene_odm) {

		if (s_odi.nb_dec_frames>2 && s_odi.total_dec_time) {
			avg_dec_time = (Float) 1000000 * s_odi.nb_dec_frames;
			avg_dec_time /= s_odi.total_dec_time;
			if (s_odi.duration) fprintf(stderr, "%d%% ", (u32) (100*s_odi.current_time / s_odi.duration ) );
			fprintf(stderr, "%d f %.2f (%d us max) - rate %d ", s_odi.nb_dec_frames, avg_dec_time, s_odi.max_dec_time, (u32) s_odi.instant_bitrate/1000);
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

