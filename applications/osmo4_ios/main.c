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

#include <pwd.h>
#include <unistd.h>


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
static Bool display_rti = 0;
static Bool Run;
static Bool CanSeek = 0;
static char the_url[GF_MAX_PATH];
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
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("FPS %02.2f - CPU %02d (%02d) - Mem %d kB\r",
			        gf_term_get_framerate(term, 0), rti.total_cpu_usage, rti.process_cpu_usage, (u32) (rti.gpac_memory / 1024) ));
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
	}
	return 0;
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
	const char *str;
	u32 i;
	u32 simulation_time = 0;
	Bool auto_exit = 0;
	Bool use_rtix = 0;
	Bool enable_mem_tracker = 0;
	Bool ret, fill_ar, visible;
	char *url_arg, *the_cfg, *rti_file;
	GF_SystemRTInfo rti;
	FILE *logfile = NULL;
    
	/*by default use current dir*/
	strcpy(the_url, ".");

	memset(&user, 0, sizeof(GF_User));

	fill_ar = visible = 0;
	url_arg = the_cfg = rti_file = NULL;

	gf_sys_init(enable_mem_tracker);

	cfg_file = gf_cfg_init(the_cfg, NULL);
	if (!cfg_file) {
		fprintf(stderr, "Error: Configuration File \"GPAC.cfg\" not found\n");
		if (logfile) fclose(logfile);
		return 1;
	}

	gf_log_set_tools_levels( gf_cfg_get_key(cfg_file, "General", "Logs") );


    
	if (!logfile) {
		const char *opt = gf_cfg_get_key(cfg_file, "General", "LogFile");
		if (opt) {
			logfile = gf_f64_open(opt, "wt");
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
		if (logfile) fclose(logfile);
		return 1;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Modules Loaded (%d found in %s)\n", i, str));

	user.config = cfg_file;
	user.EventProc = GPAC_EventProc;
	/*dummy in this case (global vars) but MUST be non-NULL*/
	user.opaque = user.modules;
	if (not_threaded) user.init_flags |= GF_TERM_NO_COMPOSITOR_THREAD;
	if (no_audio) user.init_flags |= GF_TERM_NO_AUDIO;
	if (no_regulation) user.init_flags |= GF_TERM_NO_REGULATION;

	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Loading GPAC Terminal\n"));
	term = gf_term_new(&user);
	if (!term) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("\nInit error - check you have at least one video out and one rasterizer...\nFound modules:\n"));
		gf_modules_del(user.modules);
		gf_cfg_del(cfg_file);
		gf_sys_close();
		if (logfile) fclose(logfile);
		return 1;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Terminal Loaded\n"));


    /*check video output*/
    str = gf_cfg_get_key(cfg_file, "Video", "DriverName");
    if (!strcmp(str, "Raw Video Output")) {
            GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("WARNING: using raw output video (memory only) - no display used\n"));
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

    str = gf_cfg_get_key(cfg_file, "General", "StartupFile");
    if (str) {
        strcpy(the_url, "MP4Client "GPAC_FULL_VERSION);
        gf_term_connect(term, str);
        startup_file = 1;
    }
    
	/*force fullscreen*/
    //gf_term_set_option(term, GF_OPT_FULLSCREEN, 1);

	while (Run) {
		if (restart) {
			restart = 0;
			gf_term_play_from_time(term, 0, 0);
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

	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Deleting terminal... "));
	gf_term_del(term);
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("OK\n"));

	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("GPAC cleanup ...\n"));
	gf_modules_del(user.modules);
	gf_cfg_del(cfg_file);

#ifdef GPAC_MEMORY_TRACKING
	if (enable_mem_tracker) {
		gf_memory_print();
	}
#endif

	gf_sys_close();
	if (rti_logs) fclose(rti_logs);
	if (logfile) fclose(logfile);

    return 0;
}
