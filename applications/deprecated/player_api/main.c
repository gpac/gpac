/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau
 *			Copyright (c) Romain Bouqueau 2015
 *					All rights reserved
 *
 *  This file is part of GPAC - sample GPAC player API usage
 *
 */

#include <stdio.h>
#include <gpac/terminal.h>
#include <gpac/options.h> //for GF_OPT_* and GF_STATE_*

volatile Bool connected = GF_FALSE;

static Bool play_pause_seek_gettime(GF_Terminal *term, const char *fn)
{
	u32 time;
	const u32 target_time_in_ms = 10000;

	//play
	connected = GF_FALSE;
	gf_term_connect_from_time(term, fn, 0, GF_FALSE);
	while (!connected) gf_sleep(1);

	//seek to target_time_in_ms
	gf_term_play_from_time(term, target_time_in_ms, GF_FALSE);
	gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
	time = gf_term_get_time_in_ms(term);
	assert(time == target_time_in_ms);

	//seek to 0
	connected = GF_FALSE;
	gf_term_play_from_time(term, 0, GF_FALSE);
	while (!connected) gf_sleep(1);
	time = gf_term_get_time_in_ms(term);
	assert(time == 0);

	return GF_TRUE;
}

Bool GPAC_EventProc(void *ptr, GF_Event *evt)
{
	switch (evt->type) {
	case GF_EVENT_CONNECT:
		connected = GF_TRUE;
		break;
	default:
		break;
	}

	return GF_FALSE;
}

static Bool player_foreach(int argc, char **argv)
{
	int i;
	GF_User user;
	GF_Terminal *term;
	GF_Config *cfg_file;
	cfg_file = gf_cfg_init("GPAC.cfg", NULL);
	user.modules = gf_modules_new(NULL, cfg_file);
	user.config = cfg_file;
	user.EventProc = GPAC_EventProc;
	term = gf_term_new(&user);
	if (!term)
		return GF_FALSE;

	for (i=1; i<argc; ++i) {
		Bool res = play_pause_seek_gettime(term, argv[i]);
		if (res == GF_FALSE) {
			fprintf(stderr, "Failure with input %d: \"%s\"\n", i, argv[i]);
			return GF_FALSE;
		}
	}

	gf_term_disconnect(term);
	gf_term_del(term);
	gf_modules_del(user.modules);
	gf_cfg_del(cfg_file);

	return GF_TRUE;
}

int main(int argc, char **argv)
{
	int ret = 0;

	if (argc < 2) {
		fprintf(stderr, "USAGE: %s file1 [file2 ... filen]\n", argv[0]);
		ret = 1;
		goto exit;
	}

#ifdef GPAC_MEMORY_TRACKING
	gf_sys_init(GF_MemTrackerSimple);
#else
	gf_sys_init(GF_MemTrackerNone);
#endif
	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);

	if (!player_foreach(argc, argv)) {
		ret = 1;
		goto exit;
	}

exit:
	gf_sys_close();
	return ret;
}
