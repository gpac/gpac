/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2022
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

#include <gpac/main.h>
#include <gpac/filters.h>
#include <gpac/thread.h>
#include "gpac.h"

/*
	exported for gpac_help.c
*/
GF_FilterSession *session=NULL;
u32 list_filters = 0;
u32 print_filter_info = 0;
int alias_argc = 0;
char **alias_argv = NULL;
GF_List *args_used = NULL;
GF_List *args_alloc = NULL;
u32 gen_doc = 0;
u32 help_flags = 0;
FILE *sidebar_md=NULL;
FILE *helpout = NULL;
const char *auto_gen_md_warning = "<!-- automatically generated - do not edit, patch gpac/applications/gpac/gpac.c -->\n";

#ifndef GPAC_CONFIG_ANDROID
u32 compositor_mode = 0;
#endif

//the default set of separators
char separator_set[7] = GF_FS_DEFAULT_SEPS;

/*
	exported by gpac_help.c
*/
extern const char *gpac_doc;

/*
	static vars
*/
static GF_SystemRTInfo rti;
static Bool dump_stats = GF_FALSE;
static Bool dump_graph = GF_FALSE;
static Bool print_meta_filters = GF_FALSE;
static Bool load_test_filters = GF_FALSE;
static s32 nb_loops = 0;
static s32 runfor = 0;
static Bool runfor_exit = GF_FALSE;
static u32 exit_mode = 0;
static Bool enable_prompt = GF_FALSE;
static u32 enable_reports = 0;
static char *report_filter = NULL;
static Bool do_unit_tests = GF_FALSE;
static Bool use_step_mode = GF_FALSE;
static u32 loops_done = 0;
static GF_Err evt_ret_val = GF_OK;
static Bool in_sig_handler = GF_FALSE;
static Bool custom_event_proc=GF_FALSE;
static u64 run_start_time = 0;

#ifdef GPAC_CONFIG_ANDROID
static void reset_static_vars()
{
	list_filters = print_filter_info = gen_doc = help_flags = 0;
	strcpy(separator_set, GF_FS_DEFAULT_SEPS);

	//bools
	dump_stats = dump_graph = print_meta_filters = load_test_filters = GF_FALSE;
	runfor_exit = enable_prompt = use_step_mode = in_sig_handler = custom_event_proc = GF_FALSE;
	//s32
	nb_loops = runfor = 0;
	//u32
	exit_mode = enable_reports = loops_done = 0;
	//u64
	run_start_time = 0;
	report_filter = NULL;
	evt_ret_val = GF_OK;
}
#endif


//forward defs - c.f. end of file
static void gpac_print_report(GF_FilterSession *fsess, Bool is_init, Bool is_final);
static void cleanup_logs(void);
static Bool gpac_handle_prompt(GF_FilterSession *fsess, char char_code);
static void gpac_fsess_task_help(void);
static const char *make_fileio(const char *inargs, const char **out_arg, Bool is_input, GF_Err *e);
static void cleanup_file_io(void);
static GF_Filter *load_custom_filter(GF_FilterSession *sess, char *opts, GF_Err *e);
static u32 gpac_unit_tests(GF_MemTrackerType mem_track);
static Bool revert_cache_file(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info);
#ifdef WIN32
#include <windows.h>
static BOOL WINAPI gpac_sig_handler(DWORD sig);
#else
#include <signal.h>
static void gpac_sig_handler(int sig);
#endif

static Bool gpac_event_proc(void *opaque, GF_Event *event)
{
	GF_FilterSession *fsess = (GF_FilterSession *)opaque;
	if ((event->type==GF_EVENT_PROGRESS) && (event->progress.progress_type==3)) {
		gpac_print_report(fsess, GF_FALSE, GF_FALSE);
	}
	else if (event->type==GF_EVENT_QUIT) {
		if (event->message.error>0)
			evt_ret_val = event->message.error;
		gf_fs_abort(fsess, GF_FS_FLUSH_ALL);
	}
#ifndef GPAC_CONFIG_ANDROID
	else if (custom_event_proc) {
		return mp4c_event_proc(opaque, event);
	}
#endif
	return GF_FALSE;
}


static Bool gpac_fsess_task(GF_FilterSession *fsess, void *callback, u32 *reschedule_ms)
{
	if (enable_prompt && gf_prompt_has_input()) {
#ifndef GPAC_CONFIG_ANDROID
		if ((compositor_mode==LOAD_MP4C) && mp4c_handle_prompt(gf_prompt_get_char())) {

		} else
#endif
		if (!gpac_handle_prompt(fsess, gf_prompt_get_char()))
			return GF_FALSE;
	}

#ifndef GPAC_CONFIG_ANDROID
	if ((compositor_mode==LOAD_MP4C) && mp4c_task()) {
		gf_fs_abort(fsess, GF_FS_FLUSH_NONE);
	}
#endif

	if (runfor>0) {
		u64 now = gf_sys_clock_high_res();
		if (!run_start_time) {
			run_start_time = now;
		} else if (now - run_start_time > runfor) {
			//segfault requested
			if (exit_mode==1) {
				exit(127);
			}
			//deadlock requested
			else if (exit_mode==2) {
				while(1) {
					gf_sleep(1);
				}
			}
			if (nb_loops || loops_done) {
				gf_fs_abort(fsess, runfor_exit ? GF_FS_FLUSH_NONE : GF_FS_FLUSH_ALL);
				run_start_time = 0;
			} else {
				if (runfor_exit)
					exit(0);
				gf_fs_abort(fsess, GF_FS_FLUSH_ALL);
			}
			return GF_FALSE;
		}
	}

	if (gf_fs_is_last_task(fsess))
		return GF_FALSE;
	//check every 50 ms
	*reschedule_ms = 50;
	return GF_TRUE;
}

static int gpac_exit_fun(int code, char **alias_argv, int alias_argc)
{
	u32 i;
	if (code>=0) {
		for (i=1; i<gf_sys_get_argc(); i++) {
			if (!gf_sys_is_arg_used(i)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Warning: argument %s set but not used\n", gf_sys_get_arg(i) ));
			}
		}
	} else {
		//negative code is unrecognized option, don't print unused arguments
		code = 1;
	}

	if (alias_argv) {
		while (gf_list_count(args_alloc)) {
			gf_free(gf_list_pop_back(args_alloc));
		}
		gf_list_del(args_alloc);
		args_alloc = NULL;
		gf_list_del(args_used);
		args_used = NULL;
		gf_free(alias_argv);
		alias_argv = NULL;
		alias_argc = 0;
	}
	if ((helpout != stdout) && (helpout != stderr)) {
		if (gen_doc==2) {
			fprintf(helpout, ".SH EXAMPLES\n.TP\nBasic and advanced examples are available at https://wiki.gpac.io/Filters\n");
			fprintf(helpout, ".SH MORE\n.LP\nAuthors: GPAC developers, see git repo history (-log)\n"
			".br\nFor bug reports, feature requests, more information and source code, visit https://github.com/gpac/gpac\n"
			".br\nbuild: %s\n"
			".br\nCopyright: %s\n.br\n"
			".SH SEE ALSO\n"
			".LP\ngpac(1), MP4Box(1)\n", gf_gpac_version(), gf_gpac_copyright());
		}
		gf_fclose(helpout);
	}

	if (sidebar_md) {
		gf_fclose(sidebar_md);
		sidebar_md = NULL;
	}

	cleanup_logs();

	gf_sys_close();

#ifdef GPAC_CONFIG_ANDROID
	reset_static_vars();
#endif

	if (code) return code;

#ifdef GPAC_MEMORY_TRACKING
	if (gf_memory_size() || gf_file_handles_count() ) {
		gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
		gf_memory_print();
		return 2;
	}
#endif
	return 0;
}

u32 get_u32(char *val, char *log_name)
{
	u32 res;
	if (sscanf(val, "%u", &res)==1) return res;
	GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("%s must be an unsigned integer (got %s), using 0\n", log_name, val));
	return 0;
}
s32 get_s32(char *val, char *log_name)
{
	s32 res;
	if (sscanf(val, "%d", &res)==1) return res;
	GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("%s must be a signed integer (got %s), using 0\n", log_name, val));
	return 0;
}


#define gpac_exit(_code) \
	return gpac_exit_fun(_code, alias_argv, alias_argc)


#if defined(GPAC_CONFIG_DARWIN) && !defined(GPAC_CONFIG_IOS)
#include <syslog.h>

static void gpac_syslog(void *cbck, GF_LOG_Level log_level, GF_LOG_Tool log_tool, const char* fmt, va_list vlist)
{
	int prio;
	char szMsg[4000];
	vsprintf(szMsg, fmt, vlist);
	if (log_level==GF_LOG_DEBUG) prio = LOG_DEBUG;
	else if (log_level==GF_LOG_INFO) prio = LOG_INFO;
	else if (log_level==GF_LOG_WARNING) prio = LOG_WARNING;
	else prio = LOG_ERR;
	syslog(prio, "%s", szMsg);
}
#endif

#ifndef GPAC_CONFIG_ANDROID
static
#endif
int gpac_main(int argc, char **argv)
{
	GF_Err e=GF_OK;
	int i;
	const char *profile=NULL;
	const char *session_js=NULL;
	u32 sflags=0;
	Bool override_seps=GF_FALSE;
	Bool write_profile=GF_FALSE;
	Bool write_core_opts=GF_FALSE;
	Bool write_extensions=GF_FALSE;
	GF_List *links_directive=NULL;
	GF_List *loaded_filters=NULL;
	GF_SysArgMode argmode = GF_ARGMODE_BASE;
	u32 nb_filters = 0;
	Bool nothing_to_do = GF_TRUE;
	GF_MemTrackerType mem_track=GF_MemTrackerNone;
	char *view_conn_for_filter = NULL;
	Bool view_filter_conn = GF_FALSE;
	Bool dump_codecs = GF_FALSE;
	Bool has_alias = GF_FALSE;
	Bool alias_set = GF_FALSE;
	GF_FilterSession *tmp_sess;
	Bool alias_is_play = GF_FALSE;
	Bool prev_filter_is_sink;
	Bool prev_filter_is_not_source;
	u32 current_subsession_id;
	u32 current_source_id;
	Bool has_xopt = GF_FALSE;

	helpout = stdout;
	
#ifdef WIN32
	Bool gpac_is_global_launch();
	char *argv_w[2];
	if ((argc == 1) && gpac_is_global_launch()) {
		argc = 2;
		argv_w[0] = argv[0];
		argv_w[1] = "-gui";
		argv = argv_w;
	}
#endif

	//look for mem track and profile, and check for link directives
	for (i=1; i<argc; i++) {
		char *arg = argv[i];
		if (!strcmp(arg, "-mem-track") || !strcmp(arg, "-mem-track-stack")) {
#ifdef GPAC_MEMORY_TRACKING
            mem_track = !strcmp(arg, "-mem-track-stack") ? GF_MemTrackerBackTrace : GF_MemTrackerSimple;
#else
			fprintf(stderr, "WARNING - GPAC not compiled with Memory Tracker - ignoring \"%s\"\n", arg);
#endif
		} else if (!strncmp(arg, "-p=", 3)) {
			profile = arg+3;
		} else if (!strncmp(arg, "-mkl=", 5)) {
			return gpac_make_lang(arg+5);
		}
	}

	gf_sys_init(mem_track, profile);


#ifdef GPAC_CONFIG_ANDROID
	//prevent destruction of JSRT until we unload the JNI gpac wrapper (see applications/gpac_android/src/main/jni/gpac_jni.cpp)
	gf_opts_set_key("temp", "static-jsrt", "true");
#endif


//	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);
	gf_log_set_tool_level(GF_LOG_APP, GF_LOG_INFO);

	if (gf_opts_get_key_count("gpac.alias")) {
		for (i=1; i<argc; i++) {
			char *arg = argv[i];
			if (gf_opts_get_key("gpac.alias", arg) != NULL) {
				has_alias = GF_TRUE;
				if (!strcmp(arg, "-play"))
					alias_is_play = GF_TRUE;
				break;
			}
		}
		if (has_alias) {
			if (! gpac_expand_alias(argc, argv) ) {
				gpac_exit(1);
			}
			argc = alias_argc;
			argv = alias_argv;
		}
	}

#ifndef GPAC_CONFIG_ANDROID
	if (compositor_mode) {
		//we by default want 2 additional threads in gui:
		//main thread might get locked on vsync
		//second thread might be busy decoding audio/video
		//third thread will then be able to refill all buffers/perform networks tasks
		gf_opts_set_key("temp", "threads", "2");
		//the number of threads will be overridden if set at command line

		if (compositor_mode==LOAD_MP4C)
			enable_prompt = GF_TRUE;
	}
#endif


	//this will parse default args
	e = gf_sys_set_args(argc, (const char **) argv);
	if (e) {
		fprintf(stderr, "Error assigning libgpac arguments: %s\n", gf_error_to_string(e) );
		gpac_exit(1);
	}

	if (!profile || strcmp(profile, "0") )
		gpac_load_suggested_filter_args();

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_APP, GF_LOG_DEBUG)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_APP, ("GPAC args: "));
		for (i=1; i<argc; i++) {
			char *arg = argv[i];
			GF_LOG(GF_LOG_DEBUG, GF_LOG_APP, ("%s ", arg));
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_APP, ("\n"));
	}
#endif

	for (i=1; i<argc; i++) {
		char szArgName[1024];
		char *arg = argv[i];
		char *arg_val = strchr(arg, '=');
		if (arg_val) {
			u32 len = (u32) (arg_val - arg);
			if (len>1023) len=1023;
			strncpy(szArgName, arg, len);
			szArgName[len]=0;
			arg = szArgName;
			arg_val++;
		}

		gf_sys_mark_arg_used(i, GF_TRUE);

		if ((!has_xopt && !strcmp(arg, "-h")) || !strcmp(arg, "-help") || !strcmp(arg, "-ha") || !strcmp(arg, "-hx") || !strcmp(arg, "-hh")) {
			if (!strcmp(arg, "-ha")) argmode = GF_ARGMODE_ADVANCED;
			else if (!strcmp(arg, "-hx")) argmode = GF_ARGMODE_EXPERT;
			else if (!strcmp(arg, "-hh")) argmode = GF_ARGMODE_ALL;

			gf_opts_set_key("temp", "gpac-help", arg+1);

			if (i+1<argc)
				gf_sys_mark_arg_used(i+1, GF_TRUE);
			if ((i+1==argc) || (argv[i+1][0]=='-')) {
				gpac_usage(argmode);
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "log")) {
				gpac_core_help(GF_ARGMODE_ALL, GF_TRUE);
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "core")) {
				gpac_core_help(argmode, GF_FALSE);
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "modules")) {
				gpac_modules_help();
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "doc")) {
				gpac_filter_help();
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "alias")) {
				gpac_alias_help(argmode);
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "props")) {
				if (i+2<argc) {
					gf_sys_mark_arg_used(i+2, GF_TRUE);
					dump_all_props(argv[i+2]);
				} else {
					dump_all_props(NULL);
				}
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "colors")) {
				dump_all_colors();
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "layouts")) {
				dump_all_audio_cicp();
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "cfg")) {
				gpac_config_help();
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "prompt")) {
				gpac_fsess_task_help();
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "mp4c")) {
#ifdef GPAC_CONFIG_ANDROID
				gf_sys_format_help(helpout, help_flags, "-mp4c unavailable for android\n");
#else
				mp4c_help(argmode);
#endif
				gpac_exit(0);

			}
			//old syntax -gui -h us still valid, but also allow -h gui to be consistent with other help options
			else if (!strcmp(argv[i+1], "gui")) {
				gf_opts_set_key("temp", "gpac-help", "yes");
				session = gf_fs_new_defaults(0);
				gf_set_progress_callback(session, NULL);
				gf_fs_load_filter(session, "compositor:player=gui", NULL);
				gf_fs_run(session);
				gf_fs_del(session);
				session = NULL;
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "codecs")) {
				dump_codecs = GF_TRUE;
				sflags |= GF_FS_FLAG_LOAD_META | GF_FS_FLAG_NO_GRAPH_CACHE;
				i++;
			} else if (!strcmp(argv[i+1], "links")) {
				view_filter_conn = GF_TRUE;
				if ((i+2<argc)	&& (argv[i+2][0] != '-')) {
					view_conn_for_filter = argv[i+2];
					gf_sys_mark_arg_used(i+2, GF_TRUE);
					i++;
				}

				i++;
			} else if (!strcmp(argv[i+1], "bin")) {
				gf_sys_format_help(helpout, help_flags, "GPAC binary information:\n"
					"Version: %s\n"
					"Compilation configuration: " GPAC_CONFIGURATION "\n"
					"Enabled features: %s\n"
	        		"Disabled features: %s\n", gf_gpac_version(), gf_sys_features(GF_FALSE), gf_sys_features(GF_TRUE)
				);
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "exts")) {
				list_filters = 4;
				sflags |= GF_FS_FLAG_NO_GRAPH_CACHE;
				i++;
			} else if (!strcmp(argv[i+1], "filters")) {
				list_filters = 1;
				sflags |= GF_FS_FLAG_NO_GRAPH_CACHE;
				i++;
			} else if (!strcmp(argv[i+1], "filters:*") || !strcmp(argv[i+1], "filters:@")) {
				list_filters = 2;
				sflags |= GF_FS_FLAG_NO_GRAPH_CACHE;
				i++;
			} else {
				print_filter_info = 1;
				if (!strcmp(argv[i+1], "*:*") || !strcmp(argv[i+1], "@:@"))
					print_filter_info = 2;
				sflags |= GF_FS_FLAG_NO_GRAPH_CACHE;
			}
		}
		else if (has_xopt && !strcmp(arg, "-h")) {
			gf_opts_set_key("temp", "gpac-help", "yes");
		}
		else if (!strcmp(arg, "-genmd") || !strcmp(arg, "-genman")) {
			argmode = GF_ARGMODE_ALL;
			if (!strcmp(arg, "-genmd")) {
				gf_opts_set_key("temp", "gendoc", "yes");
				gen_doc = 1;
				help_flags = GF_PRINTARG_MD;
				helpout = gf_fopen("gpac_general.md", "w");
				fprintf(helpout, "[**HOME**](Home) » [**Filters**](Filters) » Usage\n");
				fprintf(helpout, "%s", auto_gen_md_warning);
				fprintf(helpout, "# General Usage of gpac\n");
			} else {
				gf_opts_set_key("temp", "gendoc", "yes");
				gen_doc = 2;
				help_flags = GF_PRINTARG_MAN;
				helpout = gf_fopen("gpac.1", "w");
	 			fprintf(helpout, ".TH gpac 1 2019 gpac GPAC\n");
				fprintf(helpout, ".\n.SH NAME\n.LP\ngpac \\- GPAC command-line filter session manager\n"
				".SH SYNOPSIS\n.LP\n.B gpac\n"
				".RI [options] FILTER [LINK] FILTER [...]\n.br\n.\n");
			}
			gpac_usage(GF_ARGMODE_ALL);

			if (gen_doc==1) {
				fprintf(helpout, "# Using Aliases\n");
			} else {
				fprintf(helpout, ".SH Using Aliases\n.PL\n");
			}
			gpac_alias_help(GF_ARGMODE_EXPERT);

			if (gen_doc==1) {
				gf_fclose(helpout);
				helpout = gf_fopen("core_config.md", "w");
				fprintf(helpout, "[**HOME**](Home) » [**Filters**](Filters) » Configuration\n");
				fprintf(helpout, "%s", auto_gen_md_warning);
			}
			gpac_config_help();

			if (gen_doc==1) {
				gf_fclose(helpout);
				helpout = gf_fopen("core_options.md", "w");
				fprintf(helpout, "[**HOME**](Home) » [**Filters**](Filters) » Core Options\n");
				fprintf(helpout, "%s", auto_gen_md_warning);
				fprintf(helpout, "# GPAC Core Options\n");
			}
			gpac_core_help(argmode, GF_FALSE);

			if (gen_doc==1) {
				gf_fclose(helpout);
				helpout = gf_fopen("core_logs.md", "w");
				fprintf(helpout, "[**HOME**](Home) » [**Filters**](Filters) » Logging System\n");
				fprintf(helpout, "%s", auto_gen_md_warning);
				fprintf(helpout, "# GPAC Log System\n");
			}
			gpac_core_help(argmode, GF_TRUE);

			if (gen_doc==1) {
				gf_fclose(helpout);
				helpout = gf_fopen("filters_general.md", "w");
				fprintf(helpout, "[**HOME**](Home) » [**Filters**](Filters) » General Concepts\n");
				fprintf(helpout, "%s", auto_gen_md_warning);
			}
			gf_sys_format_help(helpout, help_flags, "%s", gpac_doc);

			if (gen_doc==1) {
				gf_fclose(helpout);
				helpout = gf_fopen("filters_properties.md", "w");
				fprintf(helpout, "[**HOME**](Home) » [**Filters**](Filters) » Built-in Properties\n");
				fprintf(helpout, "%s", auto_gen_md_warning);
			}
			gf_sys_format_help(helpout, help_flags, "# GPAC Built-in properties\n");
			dump_all_props(NULL);
//			dump_codecs = GF_TRUE;

			if (gen_doc==2) {
				fprintf(helpout, ".SH EXAMPLES\n.TP\nBasic and advanced examples are available at https://wiki.gpac.io/Filters\n");
				fprintf(helpout, ".SH MORE\n.LP\nAuthors: GPAC developers, see git repo history (-log)\n"
				".br\nFor bug reports, feature requests, more information and source code, visit https://github.com/gpac/gpac\n"
				".br\nbuild: %s\n"
				".br\nCopyright: %s\n.br\n"
				".SH SEE ALSO\n"
				".LP\ngpac-filters(1),MP4Box(1)\n", gf_gpac_version(), gf_gpac_copyright());
				gf_fclose(helpout);

				helpout = gf_fopen("gpac-filters.1", "w");
	 			fprintf(helpout, ".TH gpac 1 2019 gpac GPAC\n");
				fprintf(helpout, ".\n.SH NAME\n.LP\ngpac \\- GPAC command-line filter session manager\n"
				".SH SYNOPSIS\n.LP\n.B gpac\n"
				".RI [options] FILTER [LINK] FILTER [...]\n.br\n.\n"
				".SH DESCRIPTION\n.LP"
				"\nThis page describes all filters usually present in GPAC\n"
				"\nTo check for help on a filter not listed here, use gpac -h myfilter\n"
				"\n"
				);
			}

			list_filters = 1;
		}
		else if (!strcmp(arg, "-ltf")) {
			load_test_filters = GF_TRUE;
		} else if (!strncmp(arg, "-lcf", 4)) {
		} else if (!strcmp(arg, "-stats")) {
			dump_stats = GF_TRUE;
		} else if (!strcmp(arg, "-graph")) {
			dump_graph = GF_TRUE;
		} else if (strstr(arg, ":*") || strstr(arg, ":@")) {
			if (list_filters && (list_filters<4))
				list_filters = 3;
			else
				print_meta_filters = GF_TRUE;
		} else if (!strcmp(arg, "-wc")) {
			write_core_opts = GF_TRUE;
		} else if (!strcmp(arg, "-we")) {
			write_extensions = GF_TRUE;
		} else if (!strcmp(arg, "-wf")) {
			write_profile = GF_TRUE;
		} else if (!strcmp(arg, "-wfx")) {
			write_profile = GF_TRUE;
			sflags |= GF_FS_FLAG_LOAD_META;
		} else if (!strcmp(arg, "-sloop")) {
			nb_loops = -1;
			if (arg_val) nb_loops = get_s32(arg_val, "sloop");
		} else if (!strcmp(arg, "-runfor")) {
			if (arg_val) runfor = 1000*get_u32(arg_val, "runfor");
		} else if (!strcmp(arg, "-runforx")) {
			if (arg_val) runfor = 1000*get_u32(arg_val, "runforx");
			runfor_exit = GF_TRUE;
		} else if (!strcmp(arg, "-runfors")) {
			if (arg_val) runfor = 1000*get_u32(arg_val, "runfors");
			exit_mode = 1;
		} else if (!strcmp(arg, "-runforl")) {
			if (arg_val) runfor = 1000*get_u32(arg_val, "runforl");
			exit_mode = 2;
		} else if (!strcmp(arg, "-uncache")) {
			const char *cache_dir = gf_opts_get_key("core", "cache");
			gf_enum_directory(cache_dir, GF_FALSE, revert_cache_file, NULL, ".txt");
			fprintf(stderr, "GPAC Cache dir %s flattened\n", cache_dir);
			gpac_exit(0);
		} else if (!strcmp(arg, "-cfg")) {
			nothing_to_do = GF_FALSE;
		}

		else if (!strcmp(arg, "-alias") || !strcmp(arg, "-aliasdoc")) {
			char *alias_val;
			Bool exists;
			if (!arg_val) {
				fprintf(stderr, "-alias does not have any argument, check usage \"gpac -h\"\n");
				gpac_exit(1);
			}
			alias_val = arg_val ? strchr(arg_val, ' ') : NULL;
			if (alias_val) alias_val[0] = 0;

			session = gf_fs_new_defaults(sflags);
			exists = gf_fs_filter_exists(session, arg_val);
			gf_fs_del(session);
			if (exists) {
				fprintf(stderr, "alias %s has the same name as an existing filter, not allowed\n", arg_val);
				if (alias_val) alias_val[0] = ' ';
				gpac_exit(1);
			}

			gf_opts_set_key(!strcmp(arg, "-alias") ? "gpac.alias" : "gpac.aliasdoc", arg_val, alias_val ? alias_val+1 : NULL);
			fprintf(stderr, "Set %s for %s to %s\n", arg, arg_val, alias_val ? alias_val+1 : "NULL");
			if (alias_val) alias_val[0] = ' ';
			alias_set = GF_TRUE;
		}
		else if (!strncmp(arg, "-seps", 5)) {
			parse_sep_set(arg_val, &override_seps);
		} else if (!strcmp(arg, "-mem-track") || !strcmp(arg, "-mem-track-stack")) {

		} else if (!strcmp(arg, "-k")) {
			if (arg_val && (!strcmp(arg_val, "0") || !strcmp(arg_val, "no") || !strcmp(arg_val, "false")))
				enable_prompt = GF_FALSE;
			else
				enable_prompt = GF_TRUE;
		} else if (!strcmp(arg, "-js")) {
			session_js = arg_val;
		} else if (!strcmp(arg, "-r")) {
			enable_reports = 2;
			if (arg_val && !strlen(arg_val)) {
				enable_reports = 1;
			} else {
				report_filter = arg_val;
			}
		} else if (!strcmp(arg, "-unit-tests")) {
			do_unit_tests = GF_TRUE;
		} else if (!strcmp(arg, "-cl")) {
			sflags |= GF_FS_FLAG_NO_IMPLICIT;
		} else if (!strcmp(arg, "-step")) {
			use_step_mode = GF_TRUE;
			sflags |= GF_FS_FLAG_NON_BLOCKING;
		} else if (!strcmp(arg, "-xopt")) {
			has_xopt = GF_TRUE;
#ifdef GPAC_CONFIG_IOS
		} else if (!strcmp(arg, "-req-gl")) {
#endif
		} else if (arg[0]=='-') {
			if (!strcmp(arg, "-i") || !strcmp(arg, "-src")
				|| !strcmp(arg, "-o") || !strcmp(arg, "-dst")
				|| !strcmp(arg, "-ib") || !strcmp(arg, "-ob")
			) {
				//skip next arg: input or output, could start with '-'
				i++;
				gf_sys_mark_arg_used(i, GF_TRUE);
			}
			//profile already processed, and global options (--) handled by libgpac, not this app
			else if (!strcmp(arg, "-p") || !strcmp(arg, "-")
			) {
			}
			else if (gf_sys_is_gpac_arg(arg) ) {
			}
#ifndef GPAC_CONFIG_ANDROID
			else if ((compositor_mode==LOAD_MP4C) && mp4c_parse_arg(arg, arg_val)) {
			}
#endif
			else {
				if (!has_xopt) {
					gpac_suggest_arg(arg);
					gpac_exit(-1);
				} else {
					gf_sys_mark_arg_used(i, GF_FALSE);
				}
			}
		}
	}

	if (do_unit_tests) {
		gpac_exit( gpac_unit_tests(mem_track) );
	}

	if (alias_set) {
		gpac_exit(0);
	}

	if (dump_stats && gf_sys_get_rti(0, &rti, 0) ) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("System info: %d MB RAM - %d cores - main thread ID %d\n", (u32) (rti.physical_memory/1024/1024), rti.nb_cores, gf_th_id() ));
	}
	if ((list_filters>=2) || print_meta_filters || dump_codecs || print_filter_info) sflags |= GF_FS_FLAG_LOAD_META;

	if (view_filter_conn || list_filters || (print_filter_info && (argmode == GF_ARGMODE_ALL)) )
		gf_opts_set_key("temp", "gendoc", "yes");

	if (list_filters || print_filter_info)
		gf_opts_set_key("temp", "helponly", "yes");


#if defined(GPAC_CONFIG_DARWIN) && !defined(GPAC_CONFIG_IOS)
	if (compositor_mode==LOAD_GUI_ENV) {
		const char *sys_logs = getenv("GPAC_SYSLOG");
		if (sys_logs && !stricmp(sys_logs, "yes")) {
			gf_log_set_callback(NULL, gpac_syslog);
		}
	}
#endif

restart:
	prev_filter_is_sink = 0;
	current_subsession_id = 0;
	prev_filter_is_not_source = 0;
	current_source_id = 0;

	if (view_conn_for_filter && argmode>=GF_ARGMODE_EXPERT)
		sflags |= GF_FS_FLAG_PRINT_CONNECTIONS;

	session = gf_fs_new_defaults(sflags);

	if (!session) {
		gpac_exit(1);
	}
	if (override_seps) gf_fs_set_separators(session, separator_set);
	if (load_test_filters) gf_fs_register_test_filters(session);

	if (gf_fs_get_max_resolution_chain_length(session) <= 1 ) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\nDynamic resolution of filter connections disabled\n\n"));
	}

	if (list_filters || print_filter_info) {

		if (gen_doc==1) {
			list_filters = 4;
			print_filters(argc, argv, argmode);
			list_filters = 1;
		}

		if (print_filters(argc, argv, argmode)==GF_FALSE)
			e = GF_FILTER_NOT_FOUND;
		goto exit;
	}
	if (view_filter_conn) {
		gf_fs_print_all_connections(session, view_conn_for_filter, gf_sys_format_help);
		goto exit;
	}
	if (dump_codecs) {
		dump_all_codec(argmode);
		goto exit;
	}
	if (write_profile || write_extensions || write_core_opts) {
		if (write_core_opts)
			write_core_options();
		if (write_extensions)
			write_file_extensions();
		if (write_profile)
			write_filters_options();
		goto exit;
	}

	if (session_js) {
		e = gf_fs_load_script(session, session_js);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Failed to load JS for session: %s\n", gf_error_to_string(e) ));
			goto exit;
		}
	}

	//all good to go, load filters
	has_xopt = GF_FALSE;
	links_directive = gf_list_new();
	loaded_filters = gf_list_new();
	for (i=1; i<argc; i++) {
		GF_Filter *filter=NULL;
		Bool is_simple=GF_FALSE;
		Bool f_loaded = GF_FALSE;
		char *arg = argv[i];

		if (!strcmp(arg, "-src") || !strcmp(arg, "-i") || !strcmp(arg, "-ib") ) {
			if (!strcmp(arg, "-ib")) {
				const char *fargs=NULL;
				const char *fio_url = make_fileio(argv[i+1], &fargs, GF_TRUE, &e);
				if (fio_url)
					filter = gf_fs_load_source(session, fio_url, fargs, NULL, &e);
			} else {
				filter = gf_fs_load_source(session, argv[i+1], NULL, NULL, &e);
			}
			arg = argv[i+1];
			i++;
			f_loaded = GF_TRUE;
		} else if (!strcmp(arg, "-dst") || !strcmp(arg, "-o")  || !strcmp(arg, "-ob") ) {
			if (!strcmp(arg, "-ob")) {
				const char *fargs=NULL;
				const char *fio_url = make_fileio(argv[i+1], &fargs, GF_FALSE, &e);
				if (fio_url)
					filter = gf_fs_load_destination(session, fio_url, fargs, NULL, &e);
			} else {
				filter = gf_fs_load_destination(session, argv[i+1], NULL, NULL, &e);
			}
			arg = argv[i+1];
			i++;
			f_loaded = GF_TRUE;
		}
		else if (!strncmp(arg, "-lcf", 4)) {
			f_loaded = GF_TRUE;
			filter = load_custom_filter(session, arg+4, &e);
		}
		//appart from the above src/dst, other args starting with - are not filters
		else if (arg[0]=='-') {
			if (!strcmp(arg, "-xopt")) has_xopt = GF_TRUE;
			continue;
		}
		if (!f_loaded && !has_xopt) {
			if (arg[0]== separator_set[SEP_LINK] ) {
				gf_list_add(links_directive, arg);
				continue;
			}

			if (!strncmp(arg, "src=", 4) ) {
				filter = gf_fs_load_source(session, arg+4, NULL, NULL, &e);
			} else if (!strncmp(arg, "dst=", 4) ) {
				filter = gf_fs_load_destination(session, arg+4, NULL, NULL, &e);
			} else {
				e = GF_EOS;
				char *need_gfio = strstr(arg, "@gfi://");
				if (!need_gfio) need_gfio = strstr(arg, "@gfo://");
				if (need_gfio) {
					const char *fargs=NULL;
					const char *fio_url;
					char *updated_args;
					u32 len = (u32) (need_gfio - arg);
					updated_args = gf_malloc(sizeof(char)*(len+1));
					strncpy(updated_args, arg, len);
					updated_args[len]=0;

					fio_url = make_fileio(need_gfio+7, &fargs, need_gfio[3]=='i' ? GF_TRUE : GF_FALSE, &e);
					if (fio_url) {
						gf_dynstrcat(&updated_args, fio_url, NULL);
						if (fargs)
							gf_dynstrcat(&updated_args, fargs, NULL);

						filter = gf_fs_load_filter(session, updated_args, &e);
					}
					gf_free(updated_args);
				} else {
					filter = gf_fs_load_filter(session, arg, &e);
#ifndef GPAC_CONFIG_ANDROID
					if (filter && compositor_mode && !strncmp(arg, "compositor", 10)) {
						load_compositor(filter);
					}
#endif
				}
				is_simple=GF_TRUE;
				if (!filter && has_xopt)
					continue;
			}
		}

		if (!filter) {
			if (has_xopt)
				continue;
			if (!e) e = GF_FILTER_NOT_FOUND;

			if (e!=GF_FILTER_NOT_FOUND) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Failed to load filter%s \"%s\": %s\n", is_simple ? "" : " for",  arg, gf_error_to_string(e) ));
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Failed to find filter%s \"%s\"\n", is_simple ? "" : " for",  arg));

				gpac_suggest_filter(arg, GF_FALSE, GF_TRUE);
				nb_filters=0;
			}
			goto exit;
		}
		nb_filters++;

		if (!(sflags & GF_FS_FLAG_NO_IMPLICIT))
			gf_filter_tag_subsession(filter, current_subsession_id, current_source_id);

		while (gf_list_count(links_directive)) {
			char *link_prev_filter_ext = NULL;
			GF_Filter *link_from;
			Bool reverse_order = GF_FALSE;
			s32 link_filter_idx = -1;
			char *link = gf_list_pop_front(links_directive);
			char *ext = strchr(link, separator_set[SEP_FRAG]);
			if (ext) {
				ext[0] = 0;
				link_prev_filter_ext = ext+1;
			}
			if (strlen(link)>1) {
				if (link[1] == separator_set[SEP_LINK] ) {
					reverse_order = GF_TRUE;
					link++;
				}
				link_filter_idx = get_u32(link+1, "Link filter index");
				if (link_filter_idx < 0) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Wrong filter index %d, must be positive\n", link_filter_idx));
					e = GF_BAD_PARAM;
					goto exit;
				}
			} else {
				link_filter_idx = 0;
			}
			if (ext) ext[0] = separator_set[SEP_FRAG];

			if (reverse_order)
				link_from = gf_list_get(loaded_filters, link_filter_idx);
			else
				link_from = gf_list_get(loaded_filters, gf_list_count(loaded_filters)-1-link_filter_idx);

			if (!link_from) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Wrong filter index @%d\n", link_filter_idx));
				e = GF_BAD_PARAM;
				goto exit;
			}
			gf_filter_set_source(filter, link_from, link_prev_filter_ext);
		}

		gf_list_add(loaded_filters, filter);

		//implicit mode, check changes of source and sinks
		if (!(sflags & GF_FS_FLAG_NO_IMPLICIT)) {
			if (gf_filter_is_source(filter)) {
				if (prev_filter_is_not_source) {
					current_source_id++;
					gf_filter_tag_subsession(filter, current_subsession_id, current_source_id);
				}
				prev_filter_is_not_source = 0;
			} else {
				prev_filter_is_not_source = 1;
			}

			if (gf_filter_is_sink(filter)) {
				prev_filter_is_sink = GF_TRUE;
			}
			else if (prev_filter_is_sink && gf_filter_is_source(filter)) {
				prev_filter_is_sink = GF_FALSE;
				current_subsession_id++;
				current_source_id=0;
				prev_filter_is_not_source = 0;
				gf_filter_tag_subsession(filter, current_subsession_id, current_source_id);
			}
		}
	}
	if (!gf_list_count(loaded_filters) && !session_js) {
		if (nothing_to_do && !gen_doc) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Nothing to do, check usage \"gpac -h\"\ngpac - GPAC command line filter engine - version %s\n%s\n", gf_gpac_version(), gf_gpac_copyright_cite()));
			e = GF_BAD_PARAM;
		} else {
			e = GF_EOS;
		}
		goto exit;
	}
	if (enable_reports) {
		if (enable_reports==2)
			gf_fs_set_ui_callback(session, gpac_event_proc, session);

		gf_fs_enable_reporting(session, GF_TRUE);
	}
#ifndef GPAC_CONFIG_ANDROID
	if ((compositor_mode==LOAD_MP4C) || (compositor_mode==LOAD_GUI_CBK)) {
		custom_event_proc = GF_TRUE;
		gf_fs_set_ui_callback(session, gpac_event_proc, session);
	}
#endif

	if (gf_list_count(links_directive)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("Link separators specified but no following filter, ignoring links "));
		while (gf_list_count(links_directive)) {
			const char *ld = gf_list_pop_front(links_directive);
			GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("\"%s\"", ld));
		}
		GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("\n"));
	}

	if (enable_prompt || (runfor>0)) {
		if (enable_prompt && !loops_done) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Running session, press 'h' for help\n"));
		}
#ifndef GPAC_CONFIG_ANDROID
		if (compositor_mode==LOAD_MP4C)
			gf_fs_post_user_task_main(session, gpac_fsess_task, NULL, "gpac_fsess_task");
		else
#endif
			gf_fs_post_user_task(session, gpac_fsess_task, NULL, "gpac_fsess_task");
	}
	if (!enable_prompt) {
#ifdef WIN32
		SetConsoleCtrlHandler((PHANDLER_ROUTINE)gpac_sig_handler, TRUE);
#else
		signal(SIGINT, gpac_sig_handler);
		signal(SIGTERM, gpac_sig_handler);
#endif
	}

	if (enable_reports) {
		gpac_print_report(session, GF_TRUE, GF_FALSE);
	}

	if (!session_js) {
		u32 j;
		Bool has_vout = GF_FALSE;
		Bool has_compositor = GF_FALSE;
		for (j=0; j<gf_list_count(loaded_filters); j++) {
			GF_FilterStats stats;
			GF_Filter *f = gf_list_get(loaded_filters, j);
			gf_filter_get_stats(f, &stats);
			if (!stats.reg_name) continue;
			if (!strcmp(stats.reg_name, "vout")) has_vout=GF_TRUE;
			//if compositor in player mode, don't load script
			else if (!strcmp(stats.reg_name, "compositor")) {
				GF_PropertyValue p;
				gf_filter_get_arg(f, "player", &p);
				if (p.value.uint!=0)
					has_compositor=GF_TRUE;
			}
		}
		if (has_vout && !has_compositor) {
			session_js = "$GSHARE/scripts/vout.js";
			gf_fs_load_script(session, session_js);
		}
	}

	if (use_step_mode) {
		do {
			gf_fs_run(session);
		} while (!gf_fs_is_last_task(session));

	} else {
		e = gf_fs_run(session);
		if (e>0) e = GF_OK;
	}

	if (e) {
		fprintf(stderr, "session error %s\n", gf_error_to_string(e) );
	} else {
		e = gf_fs_get_last_connect_error(session);
		if (e<0) fprintf(stderr, "session last connect error %s\n", gf_error_to_string(e) );

		if (!e) {
			e = gf_fs_get_last_process_error(session);
			if (e<0) fprintf(stderr, "session last process error %s\n", gf_error_to_string(e) );

			e = evt_ret_val;
			if (e < 0) fprintf(stderr, "UI last error %s\n", gf_error_to_string(e) );
		}
		gpac_check_session_args();
	}

	if (enable_reports) {
		if (enable_reports==2) {
			gf_sys_set_console_code(stderr, GF_CONSOLE_RESTORE);
		}
		gpac_print_report(session, GF_FALSE, GF_TRUE);
	}
	gf_fs_print_non_connected_ex(session, alias_is_play);

exit:
	if (enable_reports==2) {
		gf_log_set_callback(session, NULL);
	}

	if (e && nb_filters) {
		gf_fs_run(session);
	}
	if (dump_stats)
		gf_fs_print_stats(session);
	if (dump_graph)
		gf_fs_print_connections(session);

#ifndef GPAC_CONFIG_ANDROID
	if (compositor_mode)
		unload_compositor();
#endif

	tmp_sess = session;
	session = NULL;
	gf_fs_del(tmp_sess);
	if (loaded_filters) gf_list_del(loaded_filters);
	if (links_directive) gf_list_del(links_directive);

	cleanup_file_io();

	if (!e && nb_loops) {
		if (nb_loops>0) nb_loops--;
		loops_done++;
		fprintf(stderr, "session done, restarting (loop %d)\n", loops_done);

#ifndef GPAC_CONFIG_ANDROID
		fflush(stderr);
#endif
		gf_log_reset_file();
		goto restart;
	}

	gpac_exit(e<0 ? 1 : 0);
}

#if defined(GPAC_CONFIG_DARWIN) && !defined(GPAC_CONFIG_IOS)
int main(int argc, char **argv)
{
    const char *is_gui = (argc==1) ? getenv("GPAC_GUI") : NULL;
    if (is_gui && !stricmp(is_gui, "yes")) {
        char *_argv[2];
        _argv[0] = "gpac";
        _argv[1] = "-gui";
        compositor_mode = LOAD_GUI_ENV;
        return gpac_main(2, _argv);
    }
    return gpac_main(argc, argv);
}
#elif defined(GPAC_CONFIG_IOS)
/*
	Note - this requires a patched libSDL2.0.10 or higher where the "main" symbol in SDL_uikit_main is commented out or renamed
	To build with a vanilla libSDL, remove ios_main.m from project

	The goal is to launch gpac indepentently from SDL for command-line usage
*/
int SDL_main(int argc, char **argv)
{
    if (argc<=1) {
        char *_argv[3];
        _argv[0] = "gpac";
        _argv[1] = "-noprog";
        _argv[2] = "compositor:player=gui";
        compositor_mode = LOAD_GUI_CBK;
        return gpac_main(3, _argv);
    }
    return gpac_main(argc, argv);
}

#elif !defined(GPAC_CONFIG_ANDROID)

GF_MAIN_FUNC(gpac_main)

#endif

#ifdef GPAC_CONFIG_ANDROID
void gpac_abort(void)
{
	if (session) {
		gf_fs_abort(session, GF_FS_FLUSH_NONE);
		nb_loops = 0;
	}
}
#endif


/*
	prompt interaction
*/

typedef enum
{
	GPAC_COM_UNDEF = 0,
	GPAC_QUIT,
	GPAC_EXIT,
	GPAC_PRINT_STATS,
	GPAC_PRINT_GRAPH,
	GPAC_SEND_UPDATE,
	GPAC_LIST_FILTERS,
	GPAC_INSERT_FILTER,
	GPAC_REMOVE_FILTER,
	GPAC_PRINT_HELP
} GPAC_Command;

static struct _gpac_key
{
	u8 char_code;
	GPAC_Command cmd_type;
	const char *cmd_help;
	u32 flags;
} GPAC_Keys[] = {
	{'q', GPAC_QUIT, "flush all streams and exit", 0},
	{'x', GPAC_EXIT, "exit with no flush (may break output files)", 0},
	{'s', GPAC_PRINT_STATS, "print statistics", 0},
	{'g', GPAC_PRINT_GRAPH, "print filter graph", 0},
	{'l', GPAC_LIST_FILTERS, "list filters", 0},
	{'u', GPAC_SEND_UPDATE, "update argument of filter", 0},
	{'i', GPAC_INSERT_FILTER, "insert a filter in the chain", 0},
	{'r', GPAC_REMOVE_FILTER, "remove a filter from the chain", 0},
	{'h', GPAC_PRINT_HELP, "print this help", 0},
	{0}
};

static GPAC_Command get_cmd(u8 char_code)
{
	u32 i=0;
	while (GPAC_Keys[i].char_code) {
		if (GPAC_Keys[i].char_code == char_code)
			return GPAC_Keys[i].cmd_type;
		i++;
	}
	return GPAC_COM_UNDEF;
}

static void gpac_fsess_task_help()
{
	gf_sys_format_help(helpout, help_flags, "Available runtime options/keys:\n");

	u32 i=0;
	while (GPAC_Keys[i].char_code) {
		gf_sys_format_help(helpout, help_flags, "- %c: %s\n", GPAC_Keys[i].char_code, GPAC_Keys[i].cmd_help);
		i++;
	}
}


static char szFilter[100];
static char szCom[2048];
static Bool gpac_handle_prompt(GF_FilterSession *fsess, char char_code)
{
	u32 i, count;
	GF_Filter *filter;
	const GF_Filter *link_from=NULL;
	GF_Err e;
	char *link_args = NULL;

	GPAC_Command c = get_cmd(char_code);
	switch (c) {
	case GPAC_QUIT:
		gf_fs_abort(fsess, GF_FS_FLUSH_ALL);
		nb_loops = 0;
		return GF_FALSE;
	case GPAC_EXIT:
		gf_fs_abort(fsess, GF_FS_FLUSH_NONE);
		nb_loops = 0;
		return GF_FALSE;
	case GPAC_PRINT_STATS:
		gf_fs_print_stats(fsess);
		break;
	case GPAC_PRINT_GRAPH:
		gf_fs_print_connections(fsess);
		break;
	case GPAC_PRINT_HELP:
		gpac_fsess_task_help();
		break;
	case GPAC_SEND_UPDATE:
		fprintf(stderr, "Sending filter update - enter the target filter ID, name, registry name or #N with N the 0-based index in loaded filter list:\n");

		if (1 > scanf("%99s", szFilter)) {
			fprintf(stderr, "Cannot read the filter ID, aborting.\n");
			break;
		}
		e = GF_OK;
		if (szFilter[0]=='#') {
			GF_FilterStats stats;
			u32 idx = atoi(szFilter+1);
			gf_fs_lock_filters(fsess, GF_TRUE);
			e = gf_fs_get_filter_stats(fsess, idx, &stats);
			gf_fs_lock_filters(fsess, GF_FALSE);
			if (e==GF_OK)
				strncpy(szFilter, stats.filter_id ? stats.filter_id : stats.name, 99);
		}
		if (e) {
			fprintf(stderr, "Cannot get filter for ID %s, aborting.\n", szFilter);
			break;
		}
		fprintf(stderr, "Enter the command to send\n");
		if (1 > scanf("%2047s", szCom)) {
			fprintf(stderr, "Cannot read the command, aborting.\n");
			break;
		}
		gf_fs_send_update(fsess, szFilter, NULL, szCom, NULL, 0);
		break;
	case GPAC_LIST_FILTERS:
		gf_fs_lock_filters(fsess, GF_TRUE);
		count = gf_fs_get_filters_count(fsess);
		fprintf(stderr, "Loaded filters:\n");
		for (i=0; i<count; i++) {
			GF_FilterStats stats;
			gf_fs_get_filter_stats(fsess, i, &stats);
			if (stats.filter_id)
				fprintf(stderr, "%s (%s) ID %s\n", stats.name, stats.reg_name, stats.filter_id);
			else
				fprintf(stderr, "%s (%s)\n", stats.name, stats.reg_name);
		}
		gf_fs_lock_filters(fsess, GF_FALSE);
		break;
	case GPAC_INSERT_FILTER:
	case GPAC_REMOVE_FILTER:
		fprintf(stderr, "%s filter ID, name, registry name or #N with N the 0-based index in loaded filter list:\n",
				(c==GPAC_INSERT_FILTER) ? "Inserting filter - enter the source" : "Removing filter - enter the");

		if (1 > scanf("%99s", szFilter)) {
			fprintf(stderr, "Cannot read the filter ID or index, aborting.\n");
			break;
		}
		link_args = strchr(szFilter, separator_set[SEP_LINK]);
		if (link_args) {
			link_args[0] = 0;
			link_args++;
		}
		if (szFilter[0]=='#') {
			GF_FilterStats stats;
			u32 idx = atoi(szFilter+1);
			gf_fs_lock_filters(fsess, GF_TRUE);
			gf_fs_get_filter_stats(fsess, idx, &stats);
			gf_fs_lock_filters(fsess, GF_FALSE);
			link_from = stats.filter;
		} else {
			const GF_Filter *link_from_by_reg=NULL;
			gf_fs_lock_filters(fsess, GF_TRUE);
			count = gf_fs_get_filters_count(fsess);
			for (i=0; i<count; i++) {
				GF_FilterStats stats;
				gf_fs_get_filter_stats(fsess, i, &stats);
				if (!strcmp(stats.name, szFilter)) {
					link_from = stats.filter;
					break;
				}
				if (!link_from_by_reg && !strcmp(stats.reg_name, szFilter)) {
					link_from_by_reg = stats.filter;
				}
			}
			gf_fs_lock_filters(fsess, GF_FALSE);
			if (!link_from)
				link_from = link_from_by_reg;
		}
		if (!link_from) {
			fprintf(stderr, "Failed to get filter %s, not found\n", szFilter);
			break;
		}

		if (c==GPAC_REMOVE_FILTER) {
			gf_filter_remove((GF_Filter *) link_from);
			break;
		}

		fprintf(stderr, "Enter the filter to insert\n");
		if (1 > scanf("%2047s", szCom)) {
			fprintf(stderr, "Cannot read the filter to insert, aborting.\n");
			break;
		}

		if (!strncmp(szCom, "src=", 4)) {
			filter = gf_fs_load_source(fsess, szCom+4, NULL, NULL, &e);
		} else if (!strncmp(szCom, "dst=", 4)) {
			filter = gf_fs_load_destination(fsess, szCom+4, NULL, NULL, &e);
		} else {
			filter = gf_fs_load_filter(fsess, szCom, &e);
		}

		if (!filter) {
			fprintf(stderr, "Cannot load filter %s: %s\n", szCom, gf_error_to_string(e));
			break;
		}
		gf_filter_set_source(filter, (GF_Filter *) link_from, link_args);
		//reconnect outputs of source
		gf_filter_reconnect_output((GF_Filter *) link_from);
		break;
	default:
		break;
	}
	return GF_TRUE;
}

/*
	run-time reporting vars
*/

static Bool logs_to_file=GF_FALSE;

#define DEF_LOG_ENTRIES	10

struct _logentry
{
	u32 tool, level;
	u32 nb_repeat;
	u64 clock;
	char *szMsg;
} *static_logs;
static u32 nb_log_entries = DEF_LOG_ENTRIES;

static u32 log_write=0;

static char *log_buf = NULL;
static u32 log_buf_size=0;


static void cleanup_logs()
{
	if (static_logs) {
		u32 i;
		for (i=0; i<nb_log_entries; i++) {
			if (static_logs[i].szMsg)
				gf_free(static_logs[i].szMsg);
		}
		gf_free(static_logs);
	}
	if (log_buf) gf_free(log_buf);
	log_buf = NULL;
	log_buf_size = 0;
	log_write = 0;
}

static void gpac_on_logs(void *cbck, GF_LOG_Level log_level, GF_LOG_Tool log_tool, const char* fmt, va_list vlist)
{
	va_list vlist_tmp;
	va_copy(vlist_tmp, vlist);
	u32 len = vsnprintf(NULL, 0, fmt, vlist_tmp);
	va_end(vlist_tmp);
	if (log_buf_size < len+2) {
		log_buf_size = len+2;
		log_buf = gf_realloc(log_buf, log_buf_size);
	}
	vsprintf(log_buf, fmt, vlist);

	if (log_write && static_logs[log_write-1].szMsg) {
		if (!strcmp(static_logs[log_write-1].szMsg, log_buf)) {
			static_logs[log_write-1].nb_repeat++;
			return;
		}
	}

	static_logs[log_write].level = log_level;
	static_logs[log_write].tool = log_tool;
	if (static_logs[log_write].szMsg) gf_free(static_logs[log_write].szMsg);
	static_logs[log_write].szMsg = gf_strdup(log_buf);
	static_logs[log_write].clock = gf_net_get_utc();

	log_write++;
	if (log_write==nb_log_entries) {
		log_write = nb_log_entries - 1;
		gf_free(static_logs[0].szMsg);
		memmove(&static_logs[0], &static_logs[1], sizeof (struct _logentry) * (nb_log_entries-1) );
		memset(&static_logs[log_write], 0, sizeof(struct _logentry));
	}
}

static u64 last_report_clock_us = 0;
static void print_date(u64 time)
{
	time_t gtime;
	struct tm *t;
	u32 sec;
	u32 ms;
	gtime = time / 1000;
	sec = (u32)(time / 1000);
	ms = (u32)(time - ((u64)sec) * 1000);
	t = gf_gmtime(&gtime);
//	fprintf(stderr, "[%d-%02d-%02dT%02d:%02d:%02d.%03dZ", 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, ms);
	fprintf(stderr, "[%02d:%02d:%02d.%03dZ] ", t->tm_hour, t->tm_min, t->tm_sec, ms);
}

static void gpac_print_report(GF_FilterSession *fsess, Bool is_init, Bool is_final)
{
	u32 i, count, nb_active;
	u64 now;

	if (in_sig_handler) return;

	if (is_init) {
		if (enable_reports==2)
			gf_sys_set_console_code(stderr, GF_CONSOLE_SAVE);

		logs_to_file = gf_log_use_file();
		if (!logs_to_file && (enable_reports==2) ) {
			if (!nb_log_entries) nb_log_entries = 1;
			static_logs = gf_malloc(sizeof(struct _logentry) * nb_log_entries);
			memset(static_logs, 0, sizeof(struct _logentry) * nb_log_entries);
			gf_log_set_callback(fsess, gpac_on_logs);
		}
		last_report_clock_us = gf_sys_clock_high_res();
		return;
	}

	now = gf_sys_clock_high_res();
	if ( (now - last_report_clock_us < 200000) && !is_final)
		return;

	last_report_clock_us = now;
	if (!is_final)
		gf_sys_set_console_code(stderr, GF_CONSOLE_CLEAR);

	gf_sys_get_rti(100, &rti, 0);
	gf_sys_set_console_code(stderr, GF_CONSOLE_CYAN);
	print_date(gf_net_get_utc());
	fprintf(stderr, "GPAC Session Status: ");
	gf_sys_set_console_code(stderr, GF_CONSOLE_RESET);
	fprintf(stderr, "mem % 10"LLD_SUF" kb CPU % 2d", rti.gpac_memory/1000, rti.process_cpu_time);
	fprintf(stderr, "\n");

	gf_fs_lock_filters(fsess, GF_TRUE);
	nb_active = count = gf_fs_get_filters_count(fsess);
	for (i=0; i<count; i++) {
		GF_FilterStats stats;
		gf_fs_get_filter_stats(fsess, i, &stats);
		if (stats.done || stats.filter_alias) {
			nb_active--;
			continue;
		}
		if (report_filter && (!strstr(report_filter, stats.reg_name)))
			continue;

		gf_sys_set_console_code(stderr, GF_CONSOLE_GREEN);
		fprintf(stderr, "%s", stats.name ? stats.name : stats.reg_name);
		gf_sys_set_console_code(stderr, GF_CONSOLE_RESET);
		if (stats.name && strcmp(stats.name, stats.reg_name))
			fprintf(stderr, " (%s)", stats.reg_name);
		fprintf(stderr, ": ");

		if (stats.status) {
			fprintf(stderr, "%s\n", stats.status);
		} else {
			if (stats.stream_type)
				fprintf(stderr, "%s ", gf_stream_type_name(stats.stream_type));
			if (stats.codecid)
			 	fprintf(stderr, "(%s) ", gf_codecid_name(stats.codecid) );

			if ((stats.nb_pid_in == stats.nb_pid_out) && (stats.nb_pid_in==1)) {
				Double pck_per_sec = (Double) (stats.nb_hw_pck_sent ? stats.nb_hw_pck_sent : stats.nb_pck_sent);
				pck_per_sec *= 1000000;
				pck_per_sec /= (stats.time_process+1);

				fprintf(stderr, "% 10"LLD_SUF" pck %02.02f FPS ", (s64) stats.nb_out_pck, pck_per_sec);
			} else {
				if (stats.nb_pid_in)
					fprintf(stderr, "%d input PIDs % 10"LLD_SUF" pck ", stats.nb_pid_in, (s64)stats.nb_in_pck);
				if (stats.nb_pid_out)
					fprintf(stderr, "%d output PIDs % 10"LLD_SUF" pck ", stats.nb_pid_out, (s64) stats.nb_out_pck);
			}
			if (stats.in_eos)
				fprintf(stderr, "- EOS");
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "Active filters: %d\n", nb_active);

	if (static_logs) {
		if (is_final && (!log_write || !static_logs[log_write-1].szMsg))
			return;

		fprintf(stderr, "\nLogs:\n");
		for (i=0; i<log_write; i++) {
			if (static_logs[i].level==GF_LOG_ERROR) gf_sys_set_console_code(stderr, GF_CONSOLE_RED);
			else if (static_logs[i].level==GF_LOG_WARNING) gf_sys_set_console_code(stderr, GF_CONSOLE_YELLOW);
			else if (static_logs[i].level==GF_LOG_INFO) gf_sys_set_console_code(stderr, GF_CONSOLE_GREEN);
			else gf_sys_set_console_code(stderr, GF_CONSOLE_CYAN);

			print_date(static_logs[i].clock);

			if (static_logs[i].nb_repeat)
				fprintf(stderr, "[repeated %d] ", static_logs[i].nb_repeat);

			fprintf(stderr, "%s", static_logs[i].szMsg);
			gf_sys_set_console_code(stderr, GF_CONSOLE_RESET);
		}
		fprintf(stderr, "\n");
	}
	gf_fs_lock_filters(fsess, GF_FALSE);
	fflush(stderr);
}

static Bool revert_cache_file(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	const char *url;
	GF_Config *cached;
	if (strncmp(item_name, "gpac_cache_", 11)) return GF_FALSE;
	cached = gf_cfg_new(NULL, item_path);
	url = gf_cfg_get_key(cached, "cache", "url");
	if (url) url = strstr(url, "://");
	if (url) {
		u32 i, len, dir_len=0, k=0;
		char *dst_name;
		char *sep;

		sep = strstr(item_path, "gpac_cache_");
		if (sep) {
			sep[0] = 0;
			dir_len = (u32) strlen(item_path);
			sep[0] = 'g';
		}
		url+=3;
		len = (u32) strlen(url);
		dst_name = gf_malloc(len+dir_len+1);
		memset(dst_name, 0, len+dir_len+1);

		strncpy(dst_name, item_path, dir_len);
		k=dir_len;
		for (i=0; i<len; i++) {
			dst_name[k] = url[i];
			if (dst_name[k]==':') dst_name[k]='_';
			else if (dst_name[k]=='/') {
				if (!gf_dir_exists(dst_name))
					gf_mkdir(dst_name);
			}
			k++;
		}
		sep = strrchr(item_path, '.');
		if (sep) {
			sep[0]=0;
			if (gf_file_exists(item_path)) {
				gf_file_move(item_path, dst_name);
			}
			sep[0]='.';
		}
		gf_free(dst_name);
	}
	gf_cfg_del(cached);
	gf_file_delete(item_path);
	return GF_FALSE;
}


typedef struct
{
	FILE *filep;
	char *path;
	Bool write;
	u32 nb_refs;
} FileIOCtx;

static GF_List *all_gfio_defined = NULL;

static GF_FileIO *fio_open(GF_FileIO *fileio_ref, const char *url, const char *mode, GF_Err *out_err);

static GF_Err fio_seek(GF_FileIO *fileio, u64 offset, s32 whence)
{
	FileIOCtx *ioctx = gf_fileio_get_udta(fileio);
	if (!ioctx || !ioctx->filep) return GF_BAD_PARAM;
	gf_fseek(ioctx->filep, offset, whence);
	return GF_OK;
}
static u32 fio_read(GF_FileIO *fileio, u8 *buffer, u32 bytes)
{
	FileIOCtx *ioctx = gf_fileio_get_udta(fileio);
	if (!ioctx || !ioctx->filep) return 0;
	return (u32) gf_fread(buffer, bytes, ioctx->filep);
}
static u32 fio_write(GF_FileIO *fileio, u8 *buffer, u32 bytes)
{
	FileIOCtx *ioctx = gf_fileio_get_udta(fileio);
	if (!ioctx || !ioctx->filep) return 0;
	if (!bytes) {
		fflush(ioctx->filep);
		return 0;
	}
	return (u32) gf_fwrite(buffer, bytes, ioctx->filep);
}
static s64 fio_tell(GF_FileIO *fileio)
{
	FileIOCtx *ioctx = gf_fileio_get_udta(fileio);
	if (!ioctx || !ioctx->filep) return -1;
	return gf_ftell(ioctx->filep);
}
static Bool fio_eof(GF_FileIO *fileio)
{
	FileIOCtx *ioctx = gf_fileio_get_udta(fileio);
	if (!ioctx || !ioctx->filep) return GF_TRUE;
	return feof(ioctx->filep);
}
static int fio_printf(GF_FileIO *fileio, const char *format, va_list args)
{
	FileIOCtx *ioctx = gf_fileio_get_udta(fileio);
	if (!ioctx || !ioctx->filep) return -1;
	return vfprintf(ioctx->filep, format, args);
}

#include <gpac/network.h>

static GF_FileIO *fio_open(GF_FileIO *fileio_ref, const char *url, const char *mode, GF_Err *out_err)
{
	GF_FileIO *gfio;
	FileIOCtx *ioctx;
	u32 i, count;
	u64 file_size;
	Bool no_concatenate = GF_FALSE;
	FileIOCtx *ioctx_ref = gf_fileio_get_udta(fileio_ref);

	*out_err = GF_OK;

	if (!strcmp(mode, "ref")) {
		ioctx_ref->nb_refs++;
		return fileio_ref;
	}
	if (!strcmp(mode, "unref")) {
		if (!ioctx_ref->nb_refs) return NULL;
		ioctx_ref->nb_refs--;
		if (ioctx_ref->nb_refs)
			return fileio_ref;

		url = NULL;
	}

	if (!strcmp(mode, "url")) {
		if (!url) return NULL;
		GF_SAFEALLOC(ioctx, FileIOCtx);
		if (!ioctx) return NULL;
		ioctx->path = gf_url_concatenate(ioctx_ref->path, url);
		gfio = gf_fileio_new(ioctx->path, ioctx, fio_open, fio_seek, fio_read, fio_write, fio_tell, fio_eof, fio_printf);
		if (!gfio) {
			if (ioctx->path) gf_free(ioctx->path);
			gf_free(ioctx);
			return NULL;
		}
		//remember it but no need to keep a ref on it
		gf_list_add(all_gfio_defined, gfio);
		return gfio;
	}
	if (!strcmp(mode, "probe")) {
		if (!gf_file_exists(url)) *out_err = GF_URL_ERROR;
		return NULL;
	}

	if (!url) {
		if (ioctx_ref->filep) gf_fclose(ioctx_ref->filep);
		ioctx_ref->filep = NULL;

		if (!ioctx_ref->nb_refs) {
			gf_list_del_item(all_gfio_defined, fileio_ref);
			gf_fileio_del(fileio_ref);
			if (ioctx_ref->path) gf_free(ioctx_ref->path);
			gf_free(ioctx_ref);
		}
		return NULL;
	}

	//file handle not opened, we can use the current gfio
	if (!ioctx_ref->filep && (!strnicmp(url, "gfio://", 7) || !strcmp(url, ioctx_ref->path)) ) {
		ioctx_ref->filep = gf_fopen(ioctx_ref->path, mode);
		if (!ioctx_ref->filep) {
			*out_err = GF_IO_ERR;
			return NULL;
		}
		file_size = gf_fsize(ioctx_ref->filep);
		//in test mode we want to use our ftell and fseek wrappers
		if (strchr(mode, 'r')) {
			gf_fileio_set_stats(fileio_ref, file_size, file_size, GF_TRUE, 0);
		}
		return fileio_ref;
	}

	//file handle already open (file is being opened twice), create a new gfio or check if we have already created one
	gfio = NULL;
	ioctx = NULL;
	count = gf_list_count(all_gfio_defined);
	for (i=0; i<count; i++) {
		GF_FileIO *a_gfio = gf_list_get(all_gfio_defined, i);
		ioctx = gf_fileio_get_udta(a_gfio);
		if (ioctx && !strcmp(ioctx->path, url)) {
			if (ioctx->filep) {
				no_concatenate = GF_TRUE;
				ioctx = NULL;
			}
			gfio = a_gfio;
			break;
		}
		ioctx = NULL;
	}
	if (!ioctx) {
		GF_SAFEALLOC(ioctx, FileIOCtx);
		if (!ioctx) {
			*out_err = GF_OUT_OF_MEM;
			return NULL;
		}
		if (strnicmp(url, "gfio://", 7)) {
			if (no_concatenate)
				ioctx->path = gf_strdup(url);
			else
				ioctx->path = gf_url_concatenate(ioctx_ref->path, url);
		} else {
			ioctx->path = gf_strdup(ioctx_ref->path);
		}
		gfio = gf_fileio_new(ioctx->path, ioctx, fio_open, fio_seek, fio_read, fio_write, fio_tell, fio_eof, fio_printf);
		if (!gfio) {
			if (ioctx->path) gf_free(ioctx->path);
			gf_free(ioctx);
			*out_err = GF_OUT_OF_MEM;
		}
	}

	if (strnicmp(url, "gfio://", 7)) {
		ioctx->filep = gf_fopen(ioctx->path, mode);
	} else {
		ioctx->filep = gf_fopen(ioctx_ref->path, mode);
	}

	if (!ioctx->filep) {
		*out_err = GF_IO_ERR;
		gf_list_del_item(all_gfio_defined, gfio);
		gf_fileio_del(gfio);
		if (ioctx->path) gf_free(ioctx->path);
		gf_free(ioctx);
		return NULL;
	}

	file_size = gf_fsize(ioctx->filep);
	if (strchr(mode, 'r'))
		gf_fileio_set_stats(gfio, file_size,file_size, GF_TRUE, 0);
	return gfio;
}


static const char *make_fileio(const char *inargs, const char **out_arg, Bool is_input, GF_Err *e)
{
	FileIOCtx *ioctx;
	GF_FileIO *fio;
	char *sep = (char *) gf_url_colon_suffix(inargs, separator_set[1]);
	*out_arg = NULL;
	if (sep) sep[0] = 0;

	GF_SAFEALLOC(ioctx, FileIOCtx);
	if (!ioctx) return NULL;
	ioctx->path = gf_strdup(inargs);
	if (!ioctx->path) {
		gf_free(ioctx);
		return NULL;
	}
	if (sep) {
		sep[0] = ':';
		*out_arg = sep+1;
	}
	fio = gf_fileio_new(ioctx->path, ioctx, fio_open, fio_seek, fio_read, fio_write, fio_tell, fio_eof, fio_printf);
	if (!fio) {
		gf_free(ioctx->path);
		gf_free(ioctx);
		*e = GF_OUT_OF_MEM;
		return NULL;
	}
	if (!all_gfio_defined) {
		all_gfio_defined = gf_list_new();
		if (!all_gfio_defined) return NULL;
	}
	gf_list_add(all_gfio_defined, fio);
	//keep alive until end
	ioctx->nb_refs = 1;
	return gf_fileio_url(fio);
}

static void cleanup_file_io()
{
	if (!all_gfio_defined) return;
	while (gf_list_count(all_gfio_defined)) {
		GF_FileIO *gfio = gf_list_pop_back(all_gfio_defined);
		FileIOCtx *ioctx = gf_fileio_get_udta(gfio);
		gf_fileio_del(gfio);

		if (ioctx->filep) {
			fprintf(stderr, "Warning: file IO for %s still opened!\n", ioctx->path);
			gf_fclose(ioctx->filep);
		}
		if (ioctx->path) gf_free(ioctx->path);
		gf_free(ioctx);
	}
	gf_list_del(all_gfio_defined);
	all_gfio_defined = NULL;
}

static GF_Err cust_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_FilterEvent evt;
	if (is_remove) return GF_OK;

	GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
	gf_filter_pid_send_event(pid, &evt);
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	return GF_OK;
}
static GF_Err cust_process(GF_Filter *filter)
{
	u32 i;
	for (i=0; i<gf_filter_get_ipid_count(filter); i++) {
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		const char *name = gf_filter_pid_get_name(pid);
		while (1) {
			GF_FilterPacket *pck = gf_filter_pid_get_packet(pid);
			if (!pck) break;
			fprintf(stderr, "PID %s Got packet CTS "LLU"\n", name, gf_filter_pck_get_cts(pck));
			gf_filter_pid_drop_packet(pid);
		}
	}
	return GF_OK;
}

static GF_Filter *load_custom_filter(GF_FilterSession *sess, char *opts, GF_Err *e)
{
	GF_Filter *f = gf_fs_new_filter(sess, "custom", 0, e);
	if (!f) return NULL;

	*e = gf_filter_push_caps(f, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL), NULL, GF_CAPS_INPUT, 0);
	if (*e) return NULL;
	*e = gf_filter_set_configure_ckb(f, cust_configure_pid);
	if (*e) return NULL;
	*e = gf_filter_set_process_ckb(f, cust_process);
	if (*e) return NULL;
	*e = gf_filter_set_process_event_ckb(f, NULL);
	if (*e) return NULL;
	*e = gf_filter_set_reconfigure_output_ckb(f, NULL);
	if (*e) return NULL;
	*e = gf_filter_set_probe_data_cbk(f, NULL);
	if (*e) return NULL;

	return f;
}


/*
	sig handler - keep this one last since it uses macros for function prototypes which breaks xcode function browsing
*/
static Bool prev_was_cmd=GF_FALSE;
static Bool signal_catched=GF_FALSE;
static Bool signal_processed=GF_FALSE;
#ifdef WIN32
#include <windows.h>
static BOOL WINAPI gpac_sig_handler(DWORD sig)
{
	if (sig == CTRL_C_EVENT) {
		Bool is_inter = GF_TRUE;
#else
#include <signal.h>
static void gpac_sig_handler(int sig)
{
	if ((sig == SIGINT) || (sig == SIGTERM) || (sig == SIGABRT)) {
		Bool is_inter = (sig == SIGINT) ? GF_TRUE : GF_FALSE;
#endif
		nb_loops = 0;
		if (session) {
			if (signal_catched) {
				if (signal_processed) {
					fprintf(stderr, "catched SIGINT|SIGTERM twice and session not responding, forcing exit.\n");
				}
				exit(1);
			}

			signal_catched = GF_TRUE;
			if (is_inter) {
				char input;
				in_sig_handler = GF_TRUE;
				fprintf(stderr, "\nToggle reports (r), or exit with fast (Y), full (f) or no (n) session flush ? \n");
rescan:
				input = gf_getch();
				if (!input || input == 0x0A || input == 0x0D) input = 'Y'; // user pressed "return"
				switch (input) {
				case 'Y':
				case 'y':
					signal_processed = GF_TRUE;
					gf_fs_abort(session, GF_FS_FLUSH_FAST);
					break;
				case 'F':
				case 'f':
					signal_processed = GF_TRUE;
					gf_fs_abort(session, GF_FS_FLUSH_ALL);
					break;
				case 0:
					break;
				case '\n':
					//prev was a command, flush \n
					if (prev_was_cmd) {
						prev_was_cmd = GF_FALSE;
						goto rescan;
					}
					break;

				case 'R':
				case 'r':
					prev_was_cmd = GF_TRUE;
					if (!enable_reports) {
						enable_reports = 2;
						report_filter = NULL;
						gf_fs_set_ui_callback(session, gpac_event_proc, session);
						gf_fs_enable_reporting(session, GF_TRUE);
						in_sig_handler = GF_FALSE;
						gpac_print_report(session, GF_TRUE, GF_FALSE);
					} else {
						enable_reports = 0;
						gf_fs_enable_reporting(session, GF_FALSE);
						gf_sys_set_console_code(stderr, GF_CONSOLE_CLEAR);
						gf_sys_set_console_code(stderr, GF_CONSOLE_RESTORE);
					}
					signal_catched = GF_FALSE;
					signal_processed = GF_FALSE;
					break;
				default:
					signal_processed = GF_TRUE;
					gf_fs_abort(session, GF_FS_FLUSH_NONE);
					break;
				}
				in_sig_handler = GF_FALSE;
			} else {
				signal_processed = GF_TRUE;
				gf_fs_abort(session, GF_FS_FLUSH_NONE);
			}
		}
	}
#ifdef WIN32
	return TRUE;
#endif
}

/*
	Coverage
*/

#ifdef GPAC_ENABLE_COVERAGE
#include <gpac/utf.h>
#include <gpac/base_coding.h>
#include <gpac/network.h>
#include <gpac/iso639.h>
#include <gpac/token.h>
#include <gpac/xml.h>
#include <gpac/thread.h>
#include <gpac/avparse.h>
#include <gpac/mpegts.h>
#include <gpac/scenegraph_vrml.h>
#include <gpac/rtp_streamer.h>
#include <gpac/internal/odf_dev.h>
#include <gpac/internal/media_dev.h>
#include <gpac/internal/isomedia_dev.h>
#include <gpac/path2d.h>
#include <gpac/module.h>
#endif
static u32 gpac_unit_tests(GF_MemTrackerType mem_track)
{
#ifdef GPAC_ENABLE_COVERAGE
	u32 ucs4_buf[4];
	u32 i;
	u8 utf8_buf[7];

	void *mem = gf_calloc(4, sizeof(u32));
	gf_free(mem);

	if (mem_track == GF_MemTrackerNone) return 0;

	gpac_fsess_task_help(); //for coverage
	gf_dm_sess_last_error(NULL);
	gf_log_use_color();
	gf_4cc_parse("abcd");
	gf_gpac_abi_micro();
	gf_audio_fmt_get_layout_from_name("3/2.1");
	gf_audio_fmt_get_dolby_chanmap(4);
	gf_itags_get_id3tag(1);
	i=0;
	gf_itags_enum_tags(&i, NULL, NULL, NULL);

	GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[CoreUnitTests] performing tests\n"));

	utf8_buf[0] = 'a';
	utf8_buf[1] = 0;
	if (! utf8_to_ucs4 (ucs4_buf, 1, (unsigned char *) utf8_buf)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] UCS-4 translation failed for single char\n"));
		return 1;
	}
	utf8_buf[0] = 0xc2;
	utf8_buf[1] = 0xa3;
	utf8_buf[2] = 'a';
	utf8_buf[3] = 0;
	if (! utf8_to_ucs4 (ucs4_buf, 3, (unsigned char *) utf8_buf)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] UCS-4 translation failed for 2-byte + 1-byte char\n"));
		return 1;
	}
	utf8_buf[0] = 0xe0;
	utf8_buf[1] = 0xa4;
	utf8_buf[2] = 0xb9;
	utf8_buf[3] = 0;
	if (! utf8_to_ucs4 (ucs4_buf, 3, (unsigned char *) utf8_buf)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] UCS-4 translation failed for 3-byte char\n"));
		return 1;
	}
	utf8_buf[0] = 0xf0;
	utf8_buf[1] = 0x90;
	utf8_buf[2] = 0x8d;
	utf8_buf[3] = 0x88;
	utf8_buf[4] = 0;
	if (! utf8_to_ucs4 (ucs4_buf, 4, (unsigned char *) utf8_buf)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] UCS-4 translation failed for 4-byte char\n"));
		return 1;
	}

	utf8_buf[0] = 0xf8;
	utf8_buf[1] = 0x80;
	utf8_buf[2] = 0x80;
	utf8_buf[3] = 0x80;
	utf8_buf[4] = 0xaf;
	utf8_buf[5] = 0;
	if (! utf8_to_ucs4 (ucs4_buf, 5, (unsigned char *) utf8_buf)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] UCS-4 translation failed for 5-byte char\n"));
		return 1;
	}
	utf8_buf[0] = 0xfc;
	utf8_buf[1] = 0x80;
	utf8_buf[2] = 0x80;
	utf8_buf[3] = 0x80;
	utf8_buf[4] = 0x80;
	utf8_buf[5] = 0xaf;
	utf8_buf[6] = 0;
	if (! utf8_to_ucs4 (ucs4_buf, 6, (unsigned char *) utf8_buf)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] UCS-4 translation failed for 6-byte char\n"));
		return 1;
	}
	//test error case
	utf8_buf[0] = 0xf8;
	utf8_to_ucs4 (ucs4_buf, 6, (unsigned char *) utf8_buf);

	u8 buf[5], obuf[3];
	obuf[0] = 1;
	obuf[1] = 2;
	u32 res = gf_base16_encode(obuf, 2, buf, 5);
	if (res != 4) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] base16 encode fail\n"));
		return 1;
	}
	u32 res2 = gf_base16_decode(buf, res, obuf, 3);
	if (res2 != 2) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] base16 decode fail\n"));
		return 1;
	}

	u8 *zbuf;
	u32 osize;
	GF_Err e;
	u8 *ozbuf;

#ifndef GPAC_DISABLE_ZLIB
	zbuf = (u8 *) gf_strdup("123451234512345123451234512345");
	osize=0;
	e = gf_gz_compress_payload(&zbuf, 1 + (u32) strlen((char*)zbuf), &osize);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] zlib compress fail\n"));
		gf_free(zbuf);
		return 1;
	}
	ozbuf=NULL;
	res=0;
	e = gf_gz_decompress_payload(zbuf, osize, &ozbuf, &res);
	gf_free(zbuf);
	if (ozbuf) gf_free(ozbuf);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] zlib decompress fail\n"));
		return 1;
	}
#endif

	zbuf = (u8 *)gf_strdup("123451234512345123451234512345");
	osize=0;
	e = gf_lz_compress_payload(&zbuf, 1+(u32) strlen((char*)zbuf), &osize);
	if (e && (e!= GF_NOT_SUPPORTED)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] lzma compress fail\n"));
		gf_free(zbuf);
		return 1;
	}
	ozbuf=NULL;
	res=0;
	e = gf_lz_decompress_payload(zbuf, osize, &ozbuf, &res);
	gf_free(zbuf);
	if (ozbuf) gf_free(ozbuf);
	if (e && (e!= GF_NOT_SUPPORTED)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] lzma decompress fail\n"));
		return 1;
	}

	gf_htonl(0xAABBCCDD);
	gf_ntohl(0xAABBCCDD);
	gf_htons(0xAABB);
	gf_ntohs(0xAABB);
	gf_errno_str(-1);

	/* these two lock the bash shell in test mode
	gf_prompt_set_echo_off(GF_TRUE);
	gf_prompt_set_echo_off(GF_FALSE);
	*/

	gf_net_set_ntp_shift(-1000);
	gf_net_get_ntp_diff_ms(gf_net_get_ntp_ts() );
	gf_net_get_timezone();
	gf_net_get_utc_ts(70, 1, 0, 0, 0, 0);
	gf_net_ntp_diff_ms(1000000, 1000000);
	gf_lang_get_count();
	gf_lang_get_2cc(2);
	GF_Blob b;
	memset(&b, 0, sizeof(GF_Blob));
	b.data = (u8 *) "test";
	b.size = 5;
	char url[100];
	u8 *data;
	u32 size;
	sprintf(url, "gmem://%p", &b);

	gf_sys_profiler_set_callback(NULL, NULL);

	gf_blob_get(url, &data, &size, NULL);
	if (!data || strcmp((char *)data, "test")) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] blob url parsing fail\n"));
		return 1;
	}
	gf_sys_get_battery_state(NULL, NULL, NULL, NULL, NULL);
	gf_sys_get_process_id();
	data = (u8 *) gf_log_get_tools_levels();
	if (data) gf_free(data);

	gf_sys_is_quiet();
	gf_sys_get_argv();
	gf_mx_get_num_locks(NULL);
	signal_catched = GF_TRUE;

#ifdef WIN32
	gpac_sig_handler(CTRL_C_EVENT);
#else
	gpac_sig_handler(SIGINT);
	gpac_sig_handler(SIGTERM);
#endif

	gf_mkdir("testdir");
	gf_mkdir("testdir/somedir");
	strcpy(url, "testdir/somedir/test.bin");
	FILE *f=gf_fopen(url, "wb");
	fprintf(f, "some test\n");
#ifdef GPAC_MEMORY_TRACKING
	gf_memory_print();
#endif
	gf_fclose(f);
	gf_file_modification_time(url);
	gf_m2ts_probe_file(url);

	gf_dir_cleanup("testdir");
	gf_rmdir("testdir");

	//math.c not covered yet by our sample files
	GF_Matrix2D mx;
	gf_mx2d_init(mx);
	gf_mx2d_add_skew(&mx, FIX_ONE, FIX_ONE);
	gf_mx2d_add_skew_x(&mx, GF_PI/4);
	gf_mx2d_add_skew_y(&mx, GF_PI/4);
	GF_Point2D scale, translate;
	Fixed rotate;
	gf_mx2d_decompose(&mx, &scale, &rotate, &translate);
	GF_Rect rc1, rc2;
	memset(&rc1, 0, sizeof(GF_Rect));
	memset(&rc2, 0, sizeof(GF_Rect));
	gf_rect_equal(&rc1, &rc2);

	GF_Matrix mat;
	gf_mx_init(mat);
	Fixed yaw, pitch, roll;
	gf_mx_get_yaw_pitch_roll(&mat, &yaw, &pitch, &roll);
	gf_mx_ortho_reverse_z(&mat, -20, 20, -20, 20, 0.1, 100.0);
	gf_mx_perspective_reverse_z(&mat, 0.76, 1.0, 0.1, 100.0);

	GF_Ray ray;
	GF_Vec center, outPoint;
	memset(&ray, 0, sizeof(GF_Ray));
	ray.dir.z = FIX_ONE;
	memset(&center, 0, sizeof(GF_Vec));
	gf_ray_hit_sphere(&ray, &center, FIX_ONE, &outPoint);

	gf_closest_point_to_line(center, ray.dir, center);

	GF_Plane plane;
	plane.d = FIX_ONE;
	plane.normal = center;
	gf_plane_intersect_line(&plane, &center, &ray.dir, &outPoint);

	GF_Vec4 rot, quat;
	rot.x = rot.y = 0;
	rot.z = FIX_ONE;
	rot.q = GF_PI/4;
	quat = gf_quat_from_rotation(rot);
	gf_quat_get_inv(&quat);
	gf_quat_rotate(&quat, &ray.dir);
	gf_quat_slerp(quat, quat, FIX_ONE/2);
	GF_BBox bbox;
	memset(&bbox, 0, sizeof(GF_BBox));
	gf_bbox_equal(&bbox, &bbox);

	GF_Vec v;
	v.x = v.y = v.z = 0;
	gf_vec_scale_p(&v, 2*FIX_ONE);

	//token.c
	char container[1024];
	gf_token_get_strip("12 34{ 56 : }", 0, "{:", " ", container, 1024);

	//netwok.c
	char name[GF_MAX_IP_NAME_LEN];
	gf_sk_get_host_name(name);
	gf_sk_set_usec_wait(NULL, 1000);
	u32 fam;
	u16 port;
	//to remove once we have rtsp server back
	gf_sk_get_local_info(NULL, &port, &fam);
	gf_sk_receive_wait(NULL, NULL, 0, &fam, 1);
	gf_sk_send_wait(NULL, NULL, 0, 1);

	//path2D
	GF_Path *path = gf_path_new();
	gf_path_add_move_to(path, 0, 0);
	gf_path_add_quadratic_to(path, 5, 5, 10, 0);
	gf_path_point_over(path, 4, 0);
	gf_path_del(path);

	//xml dom - to update once we find a way to integrate atsc demux in tests
	GF_DOMParser *dom = gf_xml_dom_new();
	gf_xml_dom_parse_string(dom, "<Dummy>test</Dummy>");
	gf_xml_dom_get_error(dom);
	gf_xml_dom_get_line(dom);
	gf_xml_dom_get_root_nodes_count(dom);
	gf_xml_dom_del(dom);

	//downloader - to update once we find a way to integrate atsc demux in tests
	GF_DownloadManager *dm = gf_dm_new(NULL);
	gf_dm_set_auth_callback(dm, NULL, NULL);

	gf_dm_set_data_rate(dm, 0);
	gf_dm_get_data_rate(dm);
	gf_dm_set_localcache_provider(dm, NULL, NULL);
	gf_dm_sess_abort(NULL);
	gf_dm_del(dm);

	//constants
	gf_stream_type_afx_name(GPAC_AFX_3DMC);
	//thread
	gf_th_stop(NULL);
	gf_list_swap(NULL, NULL);
	//bitstream
	GF_BitStream *bs = gf_bs_new((u8 *)"test", 4, GF_BITSTREAM_READ);
	gf_bs_bits_available(bs);
	gf_bs_get_bit_offset(bs);
	gf_bs_read_vluimsbf5(bs);
	gf_bs_del(bs);
	//module
	gf_module_load_static(NULL);

	gf_mp3_version_name(0);
	u8 tsbuf[188];
	u8 is_pes=GF_TRUE;
	memset(tsbuf, 0, 188);
	tsbuf[0] = 0x47;
	tsbuf[1] = 0x40;
	tsbuf[4]=0x00;
	tsbuf[5]=0x00;
	tsbuf[6]=0x01;
	tsbuf[10] = 0x80;
	tsbuf[11] = 0xc0;
	tsbuf[13] = 0x2 << 4;
	gf_m2ts_restamp(tsbuf, 188, 1000, &is_pes);


	gf_filter_post_task(NULL,NULL,NULL,NULL);
	gf_filter_get_arg_str(NULL, NULL, NULL);
	gf_filter_all_sinks_done(NULL);

	gf_opts_discard_changes();

	gf_rtp_reset_ssrc(NULL);
	gf_rtp_enable_nat_keepalive(NULL, 0);
	gf_rtp_stop(NULL);
	gf_rtp_streamer_get_payload_type(NULL);
	gf_rtsp_unregister_interleave(NULL, 0);
	gf_rtsp_reset_aggregation(NULL);

	get_cmd('h');
	gpac_suggest_arg("blcksize");
	gpac_suggest_filter("outf", GF_FALSE, GF_FALSE);
	//todo: build tests for these two
	gf_filter_pid_negociate_property_str(NULL, NULL, NULL);
	gf_filter_pid_negociate_property_dyn(NULL, NULL, NULL);

	gf_props_parse_type("uint");
	//this one is just a wrapper around an internal function
	gf_filter_pck_new_copy(NULL, NULL, NULL);
	gf_filter_get_max_extra_input_pids(NULL);
	gf_filter_remove(NULL);
	gf_filter_reconnect_output(NULL);
	gf_filter_pid_get_udta_flags(NULL);

	gf_audio_fmt_get_cicp_layout(2, 1, 1);
	gf_audio_fmt_get_layout_from_cicp(3);
	gf_audio_fmt_get_layout_name_from_cicp(3);
	gf_audio_fmt_get_cicp_from_layout(GF_AUDIO_CH_FRONT_LEFT|GF_AUDIO_CH_FRONT_RIGHT);

	//old bifs parsing stuff
	gf_odf_desc_del(gf_odf_desc_new(GF_ODF_ELEM_MASK_TAG));
	GF_TextConfig *txtc = (GF_TextConfig *)gf_odf_desc_new(GF_ODF_TEXT_CFG_TAG);
	gf_odf_get_text_config(NULL, 0, 0, txtc);
	gf_odf_dump_txtcfg(txtc, NULL, 0, GF_FALSE);
	gf_odf_desc_del((GF_Descriptor *) txtc);

	//stuff only used by vtbdec
	gf_hevc_read_pps_bs(NULL, NULL);
	gf_hevc_read_sps_bs(NULL, NULL);
	gf_hevc_read_vps_bs(NULL, NULL);
	gf_mpegv12_get_config(NULL, 0, NULL);

	//hinting stuff
	GF_HintPacket *hpck = gf_isom_hint_pck_new(GF_ISOM_BOX_TYPE_RTCP_STSD);
	gf_isom_hint_pck_length(hpck);
	gf_isom_hint_pck_size(hpck);
	GF_BitStream *hbs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_isom_hint_pck_write(hpck, hbs);
	u8 *hbuf;
	u32 hsize;
	gf_bs_get_content(hbs, &hbuf, &hsize);
	gf_bs_del(hbs);
	hbs = gf_bs_new(hbuf, hsize, GF_BITSTREAM_READ);
	gf_isom_hint_pck_read(hpck, hbs);
	gf_bs_del(hbs);
	gf_free(hbuf);
	gf_isom_hint_pck_del(hpck);

	gf_isom_last_error(NULL);
	gf_isom_get_media_time(NULL, 0, 0, NULL);
	gf_isom_get_sample_description_index(NULL, 0, 0);

	gf_sg_has_scripting();
	gf_node_get_proto_root(NULL);
	gf_node_proto_is_grouping(NULL);
	gf_sg_proto_get_id(NULL);
	gf_sg_proto_instance_set_ised(NULL, 0, NULL, 0);

	gf_audio_fmt_to_isobmf(0);
	gf_pixel_fmt_probe(0, NULL);
	gf_net_ntp_to_utc(0);
	gf_sys_profiler_sampling_enabled();


#endif
	return 0;
}
