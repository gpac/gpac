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

#ifndef CMD_DATA_H_
#define CMD_DATA_H_

#define MAX_SOURCE_NUMBER 20


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <gpac/config_file.h>
#include <gpac/list.h>
#include "audio_data.h"
#include "video_data.h"
#include "task.h"


/*
 * This structure corresponds to
 * the data in command line
 */
typedef struct {
	/* input video */
	VideoDataConf video_data_conf;
	/* audio input data */
	AudioDataConf audio_data_conf;
	/* Configuration file */
	GF_Config *conf;
	/* Switch source configuration file */
	GF_Config *switch_conf;
	/* MPD file */
	char mpd_filename[GF_MAX_PATH];
	/* segment duration */
	int seg_dur;
	/* fragment duration */
	int frag_dur;
	/* Exit signal emitting from user to end the program */
	volatile int exit_signal;
	/* List of entries for video in configuration file */
	GF_List *video_lst;
	/* List of entries for audio in configuration file */
	GF_List *audio_lst;
	/* Indicates that the system is live */
	/* List of video input sources */
	GF_List *vsrc;
	/* List of audio input sources */
	GF_List *asrc;
	//int live;
	/* Indicates that the system is live from a media input */
	//int live_media;
	/* The mode of the system: ON_DEMAND, LIVE_CAMERA, LIVE_MEDIA */
	int mode;
	/* Does not loop on input */
	int no_loop;
	/* MPD AvailabilityStartTime offset in milliseconds. Default is 1000 milliseconds delay */
	int ast_offset;
	/* MPD time shift buffer depth in seconds */
	int time_shift;
	/* Send message on port 1234 once fragment is ready */
	int send_message;
	/* End of Segment Box name */
	u32 seg_marker;
	/* GDR */
	int gdr;
	/* MPD min buffer time */
	float min_buffer_time;
	/* output directory name */
	char out_dir[GF_MAX_PATH];
	/* switch source configuration file */
	//char switch_cfg_filename[GF_MAX_PATH];
	FILE *logfile;
	TaskList task_list;
	/*dynamic ast*/
	int use_dynamic_ast;
	/*insert UTC */
	int insert_utc;
	/*insert UTC */
	int use_ast_offset;

	Bool use_source_timing;
} CmdData;

/*
 * Initilize the command data structure
 * 
 * @param cmd_data [out] structure to be initialize
 */
void dc_cmd_data_init(CmdData *cmd_data);

/*
 * Destroy the command data structure
 *
 * @param cmd_data [out] structure to be destroyed
 */
void dc_cmd_data_destroy(CmdData *cmd_data);

/*
 * Parse command line
 *
 * @param argc [in] number of arguments
 * @param argv [in] a list of strings each containing one argument
 * @param cmd_data [out] the data structure to fill
 */
int dc_parse_command(int argc, char **argv, CmdData *cmd_data);

#endif /* CMD_DATA_H_ */
