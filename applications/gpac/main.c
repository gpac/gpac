/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
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


int gpac_main(int argc, char **argv)
{
	GF_Filter *src, *dst;
	GF_FilterSession *ms;

//	gf_sys_init(GF_MemTrackerBackTrace);
//	gf_sys_init(GF_MemTrackerSimple);
	gf_sys_init(GF_MemTrackerNone);

	ms = gf_fs_new();


	src = gf_fs_load_filter(ms, "UTSource");
	gf_filter_set_id(src, "1");

	dst = gf_fs_load_filter(ms, "UTFilter");
	gf_filter_set_id(dst, "2");
	gf_filter_set_sources(dst, "1");

	dst = gf_fs_load_filter(ms, "UTSink");
	gf_filter_set_id(dst, "3");
	gf_filter_set_sources(dst, "2");

	gf_fs_run(ms);

	gf_fs_print_stats(ms);

	gf_fs_del(ms);

	gf_sys_close();
	if (gf_memory_size() || gf_file_handles_count() ) {
		gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
		gf_memory_print();
		return 2;
	}
	return 0;
}

GPAC_DEC_MAIN(gpac_main)


