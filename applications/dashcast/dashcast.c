/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Arash Shafiei
 *			Copyright (c) Telecom ParisTech 2000-2013
 *					All rights reserved
 *
 *  This file is part of GPAC / dashcast
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

#include "cmd_data.h"
#include "controler.h"


int main(int argc, char **argv)
{
#ifdef GPAC_MEMORY_TRACKING
	Bool use_mem_track = 0;
#endif
	s32 res;
	CmdData cmd_data;

	/* Read command line (performs init) and parse input */
	res = dc_parse_command(argc, argv, &cmd_data);
	if (res < 0) {
		if (res==-1) dc_cmd_data_destroy(&cmd_data);
		return -1;
	}

	res = dc_run_controler(&cmd_data);

#ifdef GPAC_MEMORY_TRACKING
	use_mem_track = cmd_data.use_mem_track;
#endif

	/* Destroy command data */
	dc_cmd_data_destroy(&cmd_data);

	if (res) return res;

#ifdef GPAC_MEMORY_TRACKING
	if (use_mem_track && (gf_memory_size() != 0)) {
        gf_memory_print();
		return 2;
	}
#endif	
	return 0;
}

