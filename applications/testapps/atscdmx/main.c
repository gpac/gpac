/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC - ATSC/ROUTE implementation
 *
 */

#include <gpac/atsc.h>
#include <time.h>

void PrintUsage()
{
	fprintf(stderr, "USAGE: [OPTS]\n"
#ifdef GPAC_MEMORY_TRACKING
            "-mem-track:		enables memory tracker\n"
            "-mem-track-stack:	enables memory tracker with stack dumping\n"
#endif
	        "-log-file file:	sets output log file. Also works with -lf\n"
	        "-logs log_args:	sets log tools and levels, formatted as a ':'-separated list of toolX[:toolZ]@levelX\n"
	        "-log-clock or -lc  logs time in micro sec since start time of GPAC before each log line.\n"
	        "-log-utc or -lu    logs UTC time in ms before each log line.\n"
	        "-ifce IP:			IP address of network interface to use\n"
	        "-dir PATH:			local filesystem path to which the files are written. If not set, memory mode is used.\n"
	        "                    NOTE: memory mode is not yet implemented ...\n"
	        "-service ID:		ID of the service to play. If not set, all services are dumped. If 0, no services are dumped\n"
	        "\n"
	        "\n on OSX with VM packet replay you will need to force mcast routing, eg:\n"
	        "route add -net 239.255.1.4/32 -interface vboxnet0\n"
	       );
}

static u64 log_time_start = 0;
static Bool log_utc_time = GF_FALSE;
static u64 last_log_time=0;
static void on_atscd_log(void *cbk, GF_LOG_Level ll, GF_LOG_Tool lm, const char *fmt, va_list list)
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

int main(int argc, char **argv)
{
	u32 i, serviceID=0xFFFFFFFF;
	Bool run = GF_TRUE;
	GF_MemTrackerType mem_track = GF_MemTrackerNone;
	const char *ifce = NULL;
	const char *dir = NULL;
	FILE *logfile = NULL;
	GF_ATSCDmx *atscd;
	if (argc<1) {
		PrintUsage();
		return 0;
	}
	for (i=0; i<(u32)argc; i++) {
		char *arg = argv[i];
		if (!strcmp(arg, "-mem-track") || !strcmp(arg, "-mem-track-stack")) {
#ifdef GPAC_MEMORY_TRACKING
            mem_track = !strcmp(arg, "-mem-track-stack") ? GF_MemTrackerBackTrace : GF_MemTrackerSimple;
#else
			fprintf(stderr, "WARNING - GPAC not compiled with Memory Tracker - ignoring \"%s\"\n", argv[i]);
#endif
		}
	}

	/*****************/
	/*   gpac init   */
	/*****************/
#ifdef GPAC_MEMORY_TRACKING
	gf_sys_init(mem_track);
#else
	gf_sys_init(GF_MemTrackerNone);
#endif
	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);
	gf_log_set_tool_level(GF_LOG_CONTAINER, GF_LOG_INFO);

	//parse args
	for (i=0; i<(u32)argc; i++) {
		char *arg = argv[i];

		if (!strcmp(arg, "-ifce")) ifce=argv[++i];
		else if (!strcmp(arg, "-dir")) dir=argv[++i];
		else if (!strcmp(arg, "-service")) serviceID=atoi(argv[++i]);
		else if (!strcmp(arg, "-log-file") || !strcmp(arg, "-lf")) {
			logfile = gf_fopen(argv[i+1], "wt");
			i++;
		} else if (!strcmp(arg, "-logs") ) {
			if (gf_log_set_tools_levels(argv[i+1]) != GF_OK) {
				return 1;
			}
			i++;
		} else if (!strcmp(arg, "-log-clock") || !strcmp(arg, "-lc")) {
			log_time_start = 1;
		} else if (!strcmp(arg, "-log-utc") || !strcmp(arg, "-lu")) {
			log_utc_time = 1;
		}
	}

	gf_log_set_callback(logfile, on_atscd_log);
	if (log_time_start) log_time_start = gf_sys_clock_high_res();
	/*****************/
	/*   atsc init   */
	/*****************/
	atscd = gf_atsc3_dmx_new(ifce, dir, 0);
	gf_atsc3_tune_in(atscd, serviceID, GF_FALSE);

	while (atscd && run) {
		GF_Err e = gf_atsc3_dmx_process(atscd);
		if (e == GF_IP_NETWORK_EMPTY)
			gf_sleep(1);

		if (gf_prompt_has_input()) {
			u8 c = gf_prompt_get_char();
			switch (c) {
			case 'q':
				run = GF_FALSE;
				break;
			}
		}
	}
	gf_atsc3_dmx_del(atscd);

	if (logfile) gf_fclose(logfile);
	gf_sys_close();

#ifdef GPAC_MEMORY_TRACKING
	if (mem_track && (gf_memory_size() || gf_file_handles_count() )) {
	        gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
		gf_memory_print();
		return 2;
	}
#endif
	return 0;
}

