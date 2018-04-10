/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
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
#include <time.h>

static u64 log_time_start = 0;
static Bool log_utc_time = GF_FALSE;
static Bool logs_set = GF_FALSE;
static FILE *logfile = NULL;
static s32 nb_threads=0;
static GF_SystemRTInfo rti;
static u32 list_filters = 0;
static Bool dump_stats = GF_FALSE;
static Bool dump_graph = GF_FALSE;
static Bool print_filter_info = GF_FALSE;
static Bool load_test_filters = GF_FALSE;
static u64 last_log_time=0;

//the default set of separators
static char separator_set[6] = ":=#,@";
#define SEP_LINK	4
#define SEP_FRAG	2

static void print_filters(int argc, char **argv, GF_FilterSession *session);
static void dump_all_props(void);
static void dump_all_codec(GF_FilterSession *session);

static void on_gpac_log(void *cbk, GF_LOG_Level ll, GF_LOG_Tool lm, const char *fmt, va_list list)
{
	FILE *logs = cbk ? cbk : stderr;

	if (log_time_start) {
		u64 now = gf_sys_clock_high_res();
		fprintf(logs, "At "LLD" (diff %d) - ", now - log_time_start, (u32) (now - last_log_time) );
		last_log_time = now;
	}
	if (log_utc_time) {
		u64 utc_clock = gf_net_get_utc() ;
		time_t secs = utc_clock/1000;
		struct tm t;
		t = *gmtime(&secs);
		fprintf(logs, "UTC %d-%02d-%02dT%02d:%02d:%02dZ (TS "LLU") - ", 1900+t.tm_year, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, utc_clock);
	}
	vfprintf(logs, fmt, list);
	fflush(logs);
}

static void gpac_filter_help(void)
{
	fprintf(stderr,
"Usage: gpac [options] FILTER_ARGS [LINK] FILTER_ARGS\n"
"This is the command line utility of GPAC for setting up and running filter chains.\n"
"See -h for available options.\n"
"Help is given with default separator sets :=#,@. See -s to change them.\n"
"\n"
"Filters are listed with their name and options are given using a list of colon-separated Name=Value: \n"
"\tValue can be omitted for booleans, defaulting to true.\n"
"\tName can be omitted for enumerations (eg :mode=pbo is equivalent to :pbo).\n"
"\n"
"When string parameters are used (eg URLs), it is recommended to escape the string using the keword \"gpac\" \n"
"\tEX: \"filter:ARG=http://foo/bar?yes:gpac:opt=VAL\" will properly extract the URL\n"
"\tEX: \"filter:ARG=http://foo/bar?yes:opt=VAL\" will fail to extract it and keep :opt=VAL as part of the URL\n"
"Note that the escape mechanism is not needed for local source files, for which file existence is probed during argument parsing\n"
"\n"
"Source and sinks filters do not need to be adressed by the filter name, specifying src= or dst= instead is enough\n"
"\tEX: \"src=file.mp4\" will find a filter (for example filein) able to load src, and is equivalent to filein:src=file.mp4\n"
"\tEX: \"src=file.mp4 dst=dump.yuv\" will dump the video content of file.mp4 in dump.yuv\n"
"\tSpecific source or sink filters may also be specified using filterName:src=URL or filterName:dst=URL.\n"
"\n"
"There is a special option called \"gfreg\" which allows specifying prefered filters to use when handling URLs\n"
"\tEX: \"src=file.mp4:gfreg=ffdmx,ffdec\" will use ffdmx to handle the file and ffdec to decode\n"
"This can be used to test a specific filter when alternate filter chains are possible.\n"
"\n"
"There is a special shorcut filter name for encoders \"enc\" allowing to match a filter providing the desired encoding.\n"
"The parameters for enc are:\n"
"\tc=NAME identifes the desired codec. NAME can be the gpac codec name or the encoder instance for ffmpeg/others\n"
"\tb=VAL or rate=VAL or bitrate=VAL indicates the bitrate in bits per second (UINT)\n"
"\tg=VAL or gop=VAL indicates the GOP size in frames (UINT)\n"
"\tpfmt=VAL indicates the target pixel format of the source, if supported by codec (UINT)\n"
"\tall_intra=BOOL indicates all frames should be intra frames, if supported by codec (UINT)\n"

"Other options will be passed to the filter if it accepts generic argument parsing (as is the case for ffmpeg).\n"
"\tEX: \"src=dump.yuv:size=320x240:fps=25 enc:c=avc:b=150000:g=50:cgop=true:fast=true dst=raw.264 creates a 25 fps AVC at 175kbps with a gop duration of 2 seconds, using closed gop and fast encoding settings for ffmpeg\n"
"\n"
"LINK directives may be specified. The syntax is an '@' character optionnaly followed by an integer (0 if omitted).\n"
"This indicates which previous (0-based) filters should be link to the next filter listed.\n"
"Only the last link directive occuring before a filter is used to setup links for that filter.\n"
"\tEX: \"fA fB @1 fC\" indicates to direct fA outputs to fC\n"
"\tEX: \"fA fB @1 @0 fC\" indicates to direct fB outputs to fC, @1 is ignored\n"
"\n"
"If no link directives are given, the links will be dynamically solved to fullfill as many connections as possible.\n"
"For example, \"fA fB fC\" may have fA linked to fB and fC if fB and fC accept fA outputs\n"
"\n"
"LINK directive is just a quick shortcut to set reserved arguments:\n"
"- FID=name, which sets the Filter ID\n"
"- SID=name1[,name2...], which set the sourceIDs restricting the list of possible inputs for a filter.\n"
"\n"
"\tEX: \"fA fB @1 fC\" is equivalent to \"fA fB:FID=1 fC:SID=1\" \n"
"\tEX: \"fA:FID=1 fB fC:SID=1\" indicates to direct fA outputs to fC\n"
"\tEX: \"fA:FID=1 fB:FID=2 fC:SID=1 fD:SID=1,2\" indicates to direct fA outputs to fC, and fA and fB outputs to fD\n"
"\n"
"A filter with sourceID set cannot get input from filters with no IDs.\n"
"The name can be further extended using fragment identifier (# by default):\n"
"\tname#PIDNAME: accepts only PIDs with name PIDNAME\n"
"\tname#PID=N: accepts only PIDs with ID N\n"
"\tname#TYPE: accepts only PIDs of matching media type. TYPE can be 'audio' 'video' 'scene' 'text' 'font'\n"
"\tname#TYPEN: accepts only Nth PID of matching type from source\n"
"\n"
"Note that these extensions also work with the LINK shortcut:\n"
"\tEX: \"fA fB @1#video fC\" indicates to direct fA video outputs to fC\n"
"\n"
"The filter engine will resolve implicit or explicit (LINK) connections between filters\n"
"and will allocate any filter chain required to connect the filters.\n"
"In doing so, it loads new filters with arguments inherited from both the source and the destination.\n"
"\tEX: \"src=file.mp4:OPT dst=file.aac dst=file.264\" will pass the \":OPT\" to all filters loaded between the source and the two destinations\n"
"\tEX: \"src=file.mp4 dst=file.aac:OPT dst=file.264\" will pass the \":OPT\" to all filters loaded between the source and the file.aac destination\n"
"\n"
"Note that if a filter PID gets connected to a loaded filter, no further dynamic link resolution will\n"
"be done to connect it to other filters. Link directives should be carfully setup\n"
"\tEX: src=file.mp4 @ reframer dst=dump.mp4\n"
"This will link src pid (type file) to dst (type file) because dst has no sourceID and therefore will\n"
"accept input from src. Since the pid is connected, the filter engine will not try to solve\n"
"a link between src and reframer\n"
"\tEX: src=file.mp4 reframer @ dst=dump.mp4\n"
"This will force dst to accept only from reframer, a muxer will be loaded to solve this link, and\n"
"src pid will be linked reframer (no source ID), loading a demuxer to solve the link\n"
"\n"
"Destination URLs can be templated using the same mechanism as MPEG-DASH:\n"
"\t$KEYWORD$ is replaced in the template with the resolved value,\n"
"\t$KEYWORD%%0Nd$ is replaced in the template with the resolved integer, padded with N zeros if needed,\n"
"\t$$ is an escape for $\n"
"\n"
"Supported KEYWORD (case insensitive):\n"
"\tNumber or num: replaced by file number if defined, 0 otherwise\n"
"\tPID: ID of the source pid\n"
"\tURL: URL of source file\n"
"\tFile: path on disk for source file\n"
"\tp4cc=ABCD: uses pid property with 4CC ABCD\n"
"\tpname=VAL: uses pid property with name VAL\n"
"\tOTHER: locates property 4CC for the given name, or property name if no 4CC matches.\n"
"\n"
"Templating can be usefull when encoding several qualities in one pass:\n"
"\tEX: src=dump.yuv:size=640x360 vcrop:wnd=0x0x320x180 enc:c=avc:b=1M @2 enc:c=avc:b=750k dst=dump_$CropOrigin$x$Width$x$Height$.264\n"
"This will create a croped version of the source, encoded in AVC at 1M, and a full version of the content in AVC at 750k\n"
"outputs will be dump_0x0x320x180.264 for the croped version and dump_0x0x640x360.264 for the non-croped one\n"
"\n"
	);
}

static void gf_log_usage(void)
{
	fprintf(stderr, "You can independently log different tools involved in a session by using -logs=log_args option.\n"
			"log_args is formatted as a ':'-separated list of toolX[:toolZ]@levelX\n"
	        "levelX can be one of:\n"
	        "\t        \"quiet\"      : skip logs\n"
	        "\t        \"error\"      : logs only error messages\n"
	        "\t        \"warning\"    : logs error+warning messages\n"
	        "\t        \"info\"       : logs error+warning+info messages\n"
	        "\t        \"debug\"      : logs all messages\n"
	        "toolX can be one of:\n"
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
	        "\t        \"filter\"     : filters debugging\n"
	        "\t        \"sched\"      : filter session scheduler debugging\n"
	        "\t        \"mutex\"      : mutex\n"
	        "\t        \"all\"        : all tools logged - other tools can be specified afterwards.\n"
	        "A special keyword ncl can be set to disable color logs.\n"
	        "\tEX: -logs all@info:dash@debug:ncl moves all log to info level, dash to debug level and disable color logs.\n"
	        "\n"
	);
}

static void gpac_usage(void)
{
	fprintf(stderr, "Usage: gpac [options] FILTER_ARGS [LINK] FILTER_ARGS\n"
			"This is the command line utility of GPAC for setting up and running filter chains.\n"
			"\n"
			"Global options are:\n"
#ifdef GPAC_MEMORY_TRACKING
            "-mem-track:  enables memory tracker\n"
            "-mem-track-stack:  enables memory tracker with stack dumping\n"
#endif
			"-s=CHARLIST      : sets the default character sets used to seperate various arguments, default is %s\n"
			"                   The first char is used to seperate argument names\n"
			"                   The second char, if present, is used to seperate names and values\n"
			"                   The third char, if present, is used to seperate fragments for PID sources\n"
			"                   The fourth char, if present, is used for list separators (sourceIDs, gfreg, ...)\n"
			"                   The fifth char, if present, is used for LINK directives\n"
			"-props          : prints all built-in properties.\n"
			"-list           : lists all supported filters.\n"
			"-list-meta      : lists all supported filters including meta-filters (ffmpeg & co).\n"
			"-info NAME[ NAME2]      : print info on filter NAME. For meta-filters, use NAME:INST, eg ffavin:avfoundation\n"
			"                    Use * to print info on all filters (warning, big output!)\n"
			"                    Use *:* to print info on all filters including meta-filters (warning, big big output!)\n"
			"-links          : prints possible connections between each supported filters and exits\n"
			"-stats          : print stats after execution. Stats can be viewed at runtime by typing 's' in the prompt\n"
			"-graph          : print stats after  Graph can be viewed at runtime by typing 'g' in the prompt\n"
	        "-threads=N      : sets N extra thread for the session. -1 means use all available cores\n"
			"-no-block       : disables blocking mode of filters\n"
	        "-sched=MODE     : sets scheduler mode. Possible modes are:\n"
	        "                   free: uses lock-free queues (default)\n"
	        "                   lock: uses mutexes for queues when several threads\n"
	        "                   flock: uses mutexes for queues even when no thread (debug mode)\n"
	        "                   direct: uses no threads and direct dispatch of tasks whenever possible (debug mode)\n"
			"-bl=NAMES        : blacklist the filters in NAMES (comma-seperated list)\n"
			"\n"
			"-ltf            : loads test filters for unit tests.\n"
	        "-strict-error   :  exit at first error\n"
	        "-log-file=file  : sets output log file. Also works with -lf\n"
	        "-logs=log_args  : sets log tools and levels, see -hlog\n"
	        "-log-clock or -lc      : logs time in micro sec since start time of GPAC before each log line.\n"
	        "-log-utc or -lu        : logs UTC time in ms before each log line.\n"
			"-quiet        : quiet mode\n"
			"-noprog       : disables messages if any\n"
			"-h or -help   : shows command line options.\n"
			"-doc          : shwos filter usage doc.\n"
			"\n"
	        "gpac - gpac command line filter engine - version "GPAC_FULL_VERSION"\n"
	        "GPAC Written by Jean Le Feuvre (c) Telecom ParisTech 2017-2018\n"
	        "GPAC Configuration: " GPAC_CONFIGURATION "\n"
	        "Features: %s\n", separator_set, gpac_features()
	);

}

static Bool gpac_fsess_task(GF_FilterSession *fsess, void *callback, u32 *reschedule_ms)
{
	if (gf_prompt_has_input()) {
		char c = gf_prompt_get_char();
		switch (c) {
		case 'q':
			gf_fs_abort(fsess);
			return GF_FALSE;
		case 's':
			gf_fs_print_stats(fsess);
			break;
		case 'g':
			gf_fs_print_connections(fsess);
			break;
		default:
			break;
		}
	}
	if (gf_fs_is_last_task(fsess))
		return GF_FALSE;
	*reschedule_ms = 500;
	return GF_TRUE;
}

static void progress_quiet(const void *cbck, const char *title, u64 done, u64 total) { }

static int gpac_main(int argc, char **argv)
{
	GF_Err e=GF_OK;
	int i;
	Bool override_seps=GF_FALSE;
	s32 link_prev_filter = -1;
	char *link_prev_filter_ext=NULL;
	GF_List *loaded_filters=NULL;
	u32 quiet = 0;
	GF_FilterSchedulerType sched_type = GF_FS_SCHEDULER_LOCK_FREE;
	GF_MemTrackerType mem_track=GF_MemTrackerNone;
	GF_FilterSession *session;
	Bool disable_blocking = GF_FALSE;
	Bool view_filter_conn = GF_FALSE;
	Bool dump_codecs = GF_FALSE;
	const char *blacklist = NULL;

	for (i=1; i<argc; i++) {
		char *arg = argv[i];
		if (!strcmp(arg, "-mem-track") || !strcmp(arg, "-mem-track-stack")) {
#ifdef GPAC_MEMORY_TRACKING
            mem_track = !strcmp(arg, "-mem-track-stack") ? GF_MemTrackerBackTrace : GF_MemTrackerSimple;
#else
			fprintf(stderr, "WARNING - GPAC not compiled with Memory Tracker - ignoring \"%s\"\n", arg);
#endif
		} else if (!strcmp(arg, "-h") || !strcmp(arg, "-help")) {
			gpac_usage();
			return 0;
		} else if (!strcmp(arg, "-hlog") ) {
			gf_log_usage();
			return 0;
		} else if (!strcmp(arg, "-doc")) {
			gpac_filter_help();
			return 0;
		} else if (!strcmp(arg, "-codecs")) {
			dump_codecs = GF_TRUE;
		} else if (!strcmp(arg, "-props")) {
			dump_all_props();
			return 0;
		} else if (!strncmp(arg, "-s=", 3)) {
			u32 len;
			arg = arg+3;
			len = strlen(arg);
			if (!len) continue;
			override_seps = GF_TRUE;
			if (len>=1) separator_set[0] = arg[0];
			if (len>=2) separator_set[1] = arg[1];
			if (len>=3) separator_set[2] = arg[2];
			if (len>=4) separator_set[3] = arg[3];
			if (len>=4) separator_set[4] = arg[4];
		} else if (!strcmp(arg, "-noprog")) {
			if (!quiet) quiet = 1;
		} else if (!strcmp(arg, "-quiet")) {
			quiet = 2;
		}
	}
	gf_sys_init(mem_track);
	gf_sys_set_args(argc, (const char **) argv);

	for (i=1; i<argc; i++) {
		char *arg = argv[i];
		char *arg_val = strchr(arg, '=');
		if (arg_val) {
			arg_val[0]=0;
			arg_val++;
		}

		if (!strcmp(arg, "-strict-error")) {
			gf_log_set_strict_error(1);
		} else if (!strcmp(arg, "-log-file") || !strcmp(arg, "-lf")) {
			if (arg_val) {
				logfile = gf_fopen(arg_val, "wt");
				gf_log_set_callback(logfile, on_gpac_log);
			}
		} else if (!strcmp(arg, "-logs") ) {
			if (gf_log_set_tools_levels(arg_val) != GF_OK) {
				return 1;
			}
			logs_set = GF_TRUE;
		} else if (!strcmp(arg, "-log-clock") || !strcmp(arg, "-lc")) {
			log_time_start = 1;
		} else if (!strcmp(arg, "-log-utc") || !strcmp(arg, "-lu")) {
			log_utc_time = 1;
		} else if (arg_val && !strcmp(arg, "-threads")) {
			nb_threads = atoi(arg_val);
		} else if (arg_val && !strcmp(arg, "-sched")) {
			if (!strcmp(arg_val, "lock")) sched_type = GF_FS_SCHEDULER_LOCK;
			else if (!strcmp(arg_val, "flock")) sched_type = GF_FS_SCHEDULER_LOCK_FORCE;
			else if (!strcmp(arg_val, "direct")) sched_type = GF_FS_SCHEDULER_DIRECT;
			else if (!strcmp(arg_val, "freex")) sched_type = GF_FS_SCHEDULER_LOCK_FREE_X;
		} else if (!strcmp(arg, "-ltf")) {
			load_test_filters = GF_TRUE;
		} else if (!strcmp(arg, "-list")) {
			list_filters = 1;
		} else if (!strcmp(arg, "-list-meta")) {
			list_filters = 2;
		} else if (!strcmp(arg, "-stats")) {
			dump_stats = GF_TRUE;
		} else if (!strcmp(arg, "-graph")) {
			dump_graph = GF_TRUE;
		} else if (!strcmp(arg, "-info")) {
			print_filter_info = GF_TRUE;
		} else if (!strcmp(arg, "-no-block")) {
			disable_blocking = GF_TRUE;
		} else if (!strcmp(arg, "-links")) {
			view_filter_conn = GF_TRUE;
		} else if (strstr(arg, ":*") && list_filters) {
			list_filters = 3;
		} else if (!strncmp(arg, "-bl", 3)) {
			blacklist = arg_val;
		}

		if (arg_val) {
			arg_val--;
			arg_val[0]='=';
		}
	}

	if (quiet==2) {
		gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_QUIET);
	} else if (!logs_set) {
		gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);
		gf_log_set_tool_level(GF_LOG_AUTHOR, GF_LOG_INFO);
		if (quiet) gf_set_progress_callback(NULL, progress_quiet);
	}

	if (gf_sys_get_rti(0, &rti, 0)) {
		if (dump_stats)
			fprintf(stderr, "System info: %d MB RAM - %d cores\n", (u32) (rti.physical_memory/1024/1024), rti.nb_cores);
		if (nb_threads<0) {
			nb_threads = rti.nb_cores-1;
			if (nb_threads<0) nb_threads=0;
		}
	}

	session = gf_fs_new(nb_threads, sched_type, NULL, ((list_filters>=2) || print_filter_info || dump_codecs) ? GF_TRUE : GF_FALSE, disable_blocking, blacklist);
	if (!session) {
		return 1;
	}
	if (override_seps) gf_fs_set_separators(session, separator_set);
	if (load_test_filters) gf_fs_register_test_filters(session);

	if (list_filters || print_filter_info) {
		print_filters(argc, argv, session);
		goto exit;
	}
	if (view_filter_conn) {
		gf_fs_filter_print_possible_connections(session);
		goto exit;
	}
	if (dump_codecs) {
		dump_all_codec(session);
		return 0;
	}

	//all good to go, load filters
	loaded_filters = gf_list_new();
	for (i=1; i<argc; i++) {
		GF_Filter *filter;
		char *arg = argv[i];
		if (arg[0]=='-') continue;

		if (arg[0]== separator_set[SEP_LINK] ) {
			char *ext = strchr(arg, separator_set[SEP_FRAG]);
			if (ext) {
				ext[0] = 0;
				link_prev_filter_ext = ext+1;
			}
			link_prev_filter = 0;
			if (strlen(arg)>1)
				link_prev_filter = atoi(arg+1);

			if (ext) ext[0] = separator_set[SEP_FRAG];
			continue;
		}

		if (!strncmp(arg, "src=", 4) ) {
			filter = gf_fs_load_source(session, arg+4, NULL, NULL, &e);
		} else if (!strncmp(arg, "dst=", 4) ) {
			filter = gf_fs_load_destination(session, arg+4, NULL, NULL, &e);
		} else {
			filter = gf_fs_load_filter(session, arg);
		}
		if (link_prev_filter>=0) {
			GF_Filter *link_from = gf_list_get(loaded_filters, gf_list_count(loaded_filters)-1-link_prev_filter);
			if (!link_from) {
				fprintf(stderr, "Wrong filter index @%d\n", link_prev_filter);
				e = GF_BAD_PARAM;
				goto exit;
			}
			link_prev_filter = -1;
			gf_filter_set_source(filter, link_from, link_prev_filter_ext);
			link_prev_filter_ext = NULL;
		}

		if (!filter) {
			fprintf(stderr, "Failed to load filter %s\n", arg);
			e = GF_NOT_SUPPORTED;
			goto exit;
		}
		gf_list_add(loaded_filters, filter);
	}
	if (!gf_list_count(loaded_filters)) {
		gpac_usage();
		e = GF_BAD_PARAM;
		goto exit;
	}
	if (quiet<2)
		fprintf(stderr, "Running session, press 'q' to abort\n");
	gf_fs_post_user_task(session, gpac_fsess_task, NULL, "gpac_fsess_task");
	gf_fs_run(session);

	if (dump_stats)
		gf_fs_print_stats(session);
	if (dump_graph)
		gf_fs_print_connections(session);

exit:
	if (e) {
		gf_fs_run(session);
	}
	gf_fs_del(session);
	if (loaded_filters) gf_list_del(loaded_filters);
	gf_sys_close();
	if (e) return 1;

#ifdef GPAC_MEMORY_TRACKING
	if (gf_memory_size() || gf_file_handles_count() ) {
		gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
		gf_memory_print();
		return 2;
	}
#endif
	return 0;
}

GPAC_DEC_MAIN(gpac_main)


static void dump_caps(u32 nb_caps, const GF_FilterCapability *caps)
{
	u32 i;
	for (i=0;i<nb_caps; i++) {
		const char *szName;
		const char *szVal;
		char szDump[100];
		const GF_FilterCapability *cap = &caps[i];
		if (!(cap->flags & GF_FILTER_CAPS_IN_BUNDLE) && i+1==nb_caps) break;
		if (!i) fprintf(stderr, "Capabilities Bundle:\n");
		else if (!(cap->flags & GF_FILTER_CAPS_IN_BUNDLE) ) {
			fprintf(stderr, "Capabilities Bundle:\n");
			continue;
		}

		szName = cap->name ? cap->name : gf_props_4cc_get_name(cap->code);
		if (!szName) szName = gf_4cc_to_str(cap->code);
		fprintf(stderr, "\t Flags:");
		if (cap->flags & GF_FILTER_CAPS_INPUT) fprintf(stderr, " Input");
		if (cap->flags & GF_FILTER_CAPS_OUTPUT) fprintf(stderr, " Output");
		if (cap->flags & GF_FILTER_CAPS_EXCLUDED) fprintf(stderr, " Exclude");
		if (cap->flags & GF_FILTER_CAPS_EXPLICIT) fprintf(stderr, " ExplicitOnly");

		//dump some interesting predefined ones which are not mapped to types
		if (cap->code==GF_PROP_PID_STREAM_TYPE) szVal = gf_stream_type_name(cap->val.value.uint);
		else if (cap->code==GF_PROP_PID_CODECID) szVal = (const char *) gf_codecid_name(cap->val.value.uint);
		else szVal = gf_prop_dump_val(&cap->val, szDump, GF_FALSE);

		fprintf(stderr, " Type=%s, value=%s", szName,  szVal);
		if (cap->priority) fprintf(stderr, ", priority=%d", cap->priority);
		fprintf(stderr, "\n");
	}
}

static void print_filter(const GF_FilterRegister *reg)
{
	fprintf(stderr, "Name: %s\n", reg->name);
	if (reg->description) fprintf(stderr, "Description: %s\n", reg->description);
	if (reg->author) fprintf(stderr, "Author: %s\n", reg->author);
	if (reg->comment) fprintf(stderr, "Comment: %s\n", reg->comment);

	if (reg->max_extra_pids==(u32) -1) fprintf(stderr, "Max Input pids: any\n");
	else fprintf(stderr, "Max Input pids: %d\n", 1 + reg->max_extra_pids);

	fprintf(stderr, "Flags:");
	if (reg->explicit_only) fprintf(stderr, " ExplicitOnly");
	if (reg->requires_main_thread) fprintf(stderr, "MainThread");
	if (reg->probe_url) fprintf(stderr, " IsSource");
	if (reg->reconfigure_output) fprintf(stderr, " ReconfigurableOutput");
	fprintf(stderr, "\nPriority %d\n", reg->priority);

	if (reg->args) {
		u32 idx=0;
		fprintf(stderr, "Options:\n");
		while (1) {
			const GF_FilterArgs *a = & reg->args[idx];
			if (!a || !a->arg_name) break;
			idx++;

			fprintf(stderr, "\t%s (%s): %s.", a->arg_name, gf_props_get_type_name(a->arg_type)
, a->arg_desc);
			if (a->arg_default_val) {
				fprintf(stderr, " Default %s.", a->arg_default_val);
			} else {
				fprintf(stderr, " No default.");
			}
			if (a->min_max_enum) {
				fprintf(stderr, " %s: %s", strchr(a->min_max_enum, '|') ? "Enum" : "minmax", a->min_max_enum);
			}
			if (a->updatable) fprintf(stderr, " Updatable attribute.");
			fprintf(stderr, "\n");
		}
	} else {
		fprintf(stderr, "No options\n");
	}

	if (reg->nb_caps) {
		dump_caps(reg->nb_caps, reg->caps);
	}
	fprintf(stderr, "\n");
}

static void print_filters(int argc, char **argv, GF_FilterSession *session)
{
	Bool found = GF_FALSE;
	u32 i, count = gf_fs_filters_registry_count(session);
	if (list_filters) fprintf(stderr, "Listing %d supported filters%s:\n", count, (list_filters==2) ? " including meta-filters" : "");
	for (i=0; i<count; i++) {
		const GF_FilterRegister *reg = gf_fs_get_filter_registry(session, i);
		if (print_filter_info) {
			u32 k;
			//all good to go, load filters
			for (k=1; k<argc; k++) {
				char *arg = argv[k];
				if (arg[0]=='-') continue;

				if (!strcmp(arg, reg->name) ) {
					print_filter(reg);
					found = GF_TRUE;
				} else {
					char *sep = strchr(arg, ':');
					if (!sep && !strcmp(arg, "*")) {
						print_filter(reg);
						found = GF_TRUE;
						break;
					} else if (sep && !strncmp(reg->name, arg, sep - arg) && !strcmp(sep, ":*") ) {
						print_filter(reg);
						found = GF_TRUE;
						break;
					}
				}
			}
		} else {
			fprintf(stderr, "%s: %s\n", reg->name, reg->description);
			found = GF_TRUE;

		}
	}
	if (!found) fprintf(stderr, "No such filter\n");
}

static void dump_all_props(void)
{
	u32 i=0;
	u32 prop_4cc;
	const char *name, *desc;
	u8 ptype;
	fprintf(stderr, "Built-in properties for PIDs and packets \"Name (type, 4CC): description\" :\n\n");
	while (gf_props_get_description(i, &prop_4cc, &name, &desc, &ptype)) {
		fprintf(stderr, "%s (%s, %s): %s", name, gf_props_get_type_name(ptype), gf_4cc_to_str(prop_4cc), desc);
		if (ptype==GF_PROP_PIXFMT) {
			fprintf(stderr, "\n\tNames: %s\n\tFile extensions: %s", gf_pixel_fmt_all_names(), gf_pixel_fmt_all_shortnames() );
		} else if (ptype==GF_PROP_PCMFMT) {
			fprintf(stderr, "\n\tNames: %s\n\tFile extensions: %s", gf_audio_fmt_all_names(), gf_audio_fmt_all_shortnames() );
		}
		fprintf(stderr, "\n");
		i++;
	}
}

static void dump_all_codec(GF_FilterSession *session)
{
	GF_PropertyValue rawp, cp;
	u32 cidx=0;
	u32 count = gf_fs_filters_registry_count(session);
	fprintf(stderr, "Codec names (I: Filter Input support, O: Filter Output support):\n");
	rawp.type = cp.type = GF_PROP_UINT;
	rawp.value.uint = GF_CODECID_RAW;
	while (1) {
		u32 i;
		const char *lname;
		const char *sname;
		Bool enc_found = GF_FALSE;
		Bool dec_found = GF_FALSE;
		cp.value.uint = gf_codecid_enum(cidx, &sname, &lname);
		cidx++;
		if (cp.value.uint == GF_CODECID_NONE) break;
		if (cp.value.uint == GF_CODECID_RAW) continue;
		if (!sname) break;

		for (i=0; i<count; i++) {
			const GF_FilterRegister *reg = gf_fs_get_filter_registry(session, i);
			if ( gf_fs_check_registry_cap(reg, GF_PROP_PID_CODECID, &rawp, GF_PROP_PID_CODECID, &cp)) enc_found = GF_TRUE;
			if ( gf_fs_check_registry_cap(reg, GF_PROP_PID_CODECID, &cp, GF_PROP_PID_CODECID, &rawp)) dec_found = GF_TRUE;
		}

		fprintf(stderr, "%s (%c%c): %s\n", sname, dec_found ? 'I' : '-', enc_found ? 'O' : '-', lname);
	}
	fprintf(stderr, "\n");
}

