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

int dc_str_to_resolution(char * psz_str, int * p_width, int * p_height) {

	char * p_save = NULL;
	char * token = (char *) strtok_r(psz_str, "x", &p_save);

	if (!token) {
		fprintf(stderr, "Cannot parse resolution string.\n");
		return -1;
	}
	*p_width = atoi(token);

	token = (char *) strtok_r(NULL, " ", &p_save);

	if (!token) {
		fprintf(stderr, "Cannot parse resolution string.\n");
		return -1;
	}
	*p_height = atoi(token);

	return 0;
}

int dc_read_configuration(CmdData * p_cmdd) {

	int i;

	GF_Config * p_conf = p_cmdd->p_conf;

	int i_sec_count = gf_cfg_get_section_count(p_conf);

	if (i_sec_count == 0) {

		gf_cfg_set_key(p_conf, "v1", "type", "video");
		gf_cfg_set_key(p_conf, "v1", "bitrate", "400000");
//		gf_cfg_set_key(p_conf, "v1", "framerate", "25");
		gf_cfg_set_key(p_conf, "v1", "width", "640");
		gf_cfg_set_key(p_conf, "v1", "height", "480");
//		gf_cfg_set_key(p_conf, "v1", "codec", "libx264");

		gf_cfg_set_key(p_conf, "a1", "type", "audio");
		gf_cfg_set_key(p_conf, "a1", "bitrate", "200000");
//		gf_cfg_set_key(p_conf, "a1", "samplerate", "48000");
//		gf_cfg_set_key(p_conf, "a1", "channels", "2");
//		gf_cfg_set_key(p_conf, "a1", "codec", "aac");

		i_sec_count = 2;
	}
	for (i = 0; i < i_sec_count; i++) {

		const char * psz_sec_name = gf_cfg_get_section_name(p_conf, i);

		const char * psz_type = gf_cfg_get_key(p_conf, psz_sec_name, "type");

		if (strcmp(psz_type, "video") == 0) {

			VideoData * p_vconf = malloc(sizeof(VideoData));
			strcpy(p_vconf->psz_name, psz_sec_name);
//			strcpy(p_vconf->psz_codec,
//					gf_cfg_get_key(p_conf, psz_sec_name, "codec"));
			p_vconf->i_bitrate = atoi(
					gf_cfg_get_key(p_conf, psz_sec_name, "bitrate"));
//			p_vconf->i_framerate = atoi(
//					gf_cfg_get_key(p_conf, psz_sec_name, "framerate"));
			p_vconf->i_height = atoi(
					gf_cfg_get_key(p_conf, psz_sec_name, "height"));
			p_vconf->i_width = atoi(
					gf_cfg_get_key(p_conf, psz_sec_name, "width"));

			gf_list_add(p_cmdd->p_video_lst, (void *) p_vconf);
		} else if (strcmp(psz_type, "audio") == 0) {

			AudioData * p_aconf = malloc(sizeof(AudioData));
			strcpy(p_aconf->psz_name, psz_sec_name);
//			strcpy(p_aconf->psz_codec,
//					gf_cfg_get_key(p_conf, psz_sec_name, "codec"));
			p_aconf->i_bitrate = atoi(
					gf_cfg_get_key(p_conf, psz_sec_name, "bitrate"));
//			p_aconf->i_samplerate = atoi(
//					gf_cfg_get_key(p_conf, psz_sec_name, "samplerate"));
//			p_aconf->i_channels = atoi(
//					gf_cfg_get_key(p_conf, psz_sec_name, "channels"));

			gf_list_add(p_cmdd->p_audio_lst, (void *) p_aconf);

		} else {
			printf("Configuration file: type %s is not supported.\n", psz_type);
		}

	}

	printf("\33[34m\33[1m");
	printf("Configurations:\n");
	for (i = 0; i < gf_list_count(p_cmdd->p_video_lst); i++) {
		VideoData * p_vconf = gf_list_get(p_cmdd->p_video_lst, i);
		printf("    id:%s\tres:%dx%d\tvbr:%d\n", p_vconf->psz_name,
				p_vconf->i_width, p_vconf->i_height,
				p_vconf->i_bitrate/*, p_vconf->i_framerate, p_vconf->psz_codec*/);
	}

	for (i = 0; i < gf_list_count(p_cmdd->p_audio_lst); i++) {
		AudioData * p_aconf = gf_list_get(p_cmdd->p_audio_lst, i);
		printf("    id:%s\tabr:%d\n", p_aconf->psz_name, p_aconf->i_bitrate/*, p_aconf->i_samplerate,
		 p_aconf->i_channels,p_aconf->psz_codec*/);
	}
	printf("\33[0m");
	fflush(stdout);

	return 0;
}

int dc_read_switch_config(CmdData * p_cmdd) {

	int i;
	int src_number;
	dc_task_init(&p_cmdd->task_list);

	char start_time[4096], end_time[4096];

	time_t now_t = time(NULL);
	struct tm start_tm  = *localtime(&now_t);
	struct tm end_tm  = *localtime(&now_t);

	GF_Config * p_conf = p_cmdd->p_switch_conf;



	int i_sec_count = gf_cfg_get_section_count(p_conf);

	if (i_sec_count == 0) {
		return 0;
	}

	for (i = 0; i < i_sec_count; i++) {

		const char * psz_sec_name = gf_cfg_get_section_name(p_conf, i);

		const char * psz_type = gf_cfg_get_key(p_conf, psz_sec_name, "type");

		if (strcmp(psz_type, "video") == 0) {

			VideoData * p_vconf = malloc(sizeof(VideoData));

			strcpy(p_vconf->psz_source_id, psz_sec_name);
			strcpy(p_vconf->psz_name,
					gf_cfg_get_key(p_conf, psz_sec_name, "source"));

			strcpy(start_time, gf_cfg_get_key(p_conf, psz_sec_name, "start"));
			strptime(start_time, "%Y-%m-%d %H:%M:%S", &start_tm);
			p_vconf->start_time = mktime(&start_tm);
			strcpy(end_time, gf_cfg_get_key(p_conf, psz_sec_name, "end"));
			strptime(end_time, "%Y-%m-%d %H:%M:%S", &end_tm);
			p_vconf->end_time = mktime(&end_tm);

			gf_list_add(p_cmdd->p_vsrc, (void *) p_vconf);

			src_number = gf_list_count(p_cmdd->p_vsrc);

			dc_task_add(&p_cmdd->task_list, src_number, p_vconf->psz_source_id, p_vconf->start_time, p_vconf->end_time);

		} else if (strcmp(psz_type, "audio") == 0) {

			AudioData * p_aconf = malloc(sizeof(AudioData));
			strcpy(p_aconf->psz_source_id, psz_sec_name);
			strcpy(p_aconf->psz_name,
					gf_cfg_get_key(p_conf, psz_sec_name, "source"));

			strcpy(start_time, gf_cfg_get_key(p_conf, psz_sec_name, "start"));
			strptime(start_time, "%Y-%m-%d %H:%M:%S", &start_tm);
			p_aconf->start_time = mktime(&start_tm);

			strcpy(end_time, gf_cfg_get_key(p_conf, psz_sec_name, "end"));
			strptime(end_time, "%Y-%m-%d %H:%M:%S", &end_tm);
			p_aconf->end_time = mktime(&end_tm);

			gf_list_add(p_cmdd->p_asrc, (void *) p_aconf);

		} else {
			printf(
					"Switch source configuration file: type %s is not supported.\n",
					psz_type);
		}


	}

	printf("\33[34m\33[1m");
	printf("Sources:\n");
	for (i = 0; i < gf_list_count(p_cmdd->p_vsrc); i++) {
		VideoData * p_vconf = gf_list_get(p_cmdd->p_vsrc, i);
		strftime(start_time, 20, "%Y-%m-%d %H:%M:%S", localtime(&p_vconf->start_time));
		strftime(end_time, 20, "%Y-%m-%d %H:%M:%S", localtime(&p_vconf->end_time));
		printf("    id:%s\tsource:%s\tstart:%s\tend:%s\n", p_vconf->psz_source_id,
				p_vconf->psz_name, start_time, end_time);
	}

	for (i = 0; i < gf_list_count(p_cmdd->p_asrc); i++) {
		AudioData * p_aconf = gf_list_get(p_cmdd->p_asrc, i);
		strftime(start_time, 20, "%Y-%m-%d %H:%M:%S", localtime(&p_aconf->start_time));
		strftime(end_time, 20, "%Y-%m-%d %H:%M:%S", localtime(&p_aconf->end_time));
		printf("    id:%s\tsource:%s\tstart:%s\tend:%s\n", p_aconf->psz_source_id,
				p_aconf->psz_name, start_time, end_time);

	}
	printf("\33[0m");
	fflush(stdout);

	return 0;
}

void dc_cmd_data_init(CmdData * p_cmdd) {


	dc_audio_data_set_default(&p_cmdd->adata);
	dc_video_data_set_default(&p_cmdd->vdata);

	p_cmdd->p_conf = NULL;
	p_cmdd->p_switch_conf = NULL;
	strcpy(p_cmdd->psz_mpd, "");
	strcpy(p_cmdd->psz_out, "");
	p_cmdd->i_seg_marker = 0;
	p_cmdd->i_exit_signal = 0;
	p_cmdd->i_mode = ON_DEMAND;
	p_cmdd->i_no_loop = 0;
	p_cmdd->i_send_message = 0;
	p_cmdd->i_seg_dur = 0;
	p_cmdd->i_frag_dur = 0;
	p_cmdd->i_ast_offset = -1;
	p_cmdd->i_time_shift = 0;
	p_cmdd->f_minbuftime = -1;
	p_cmdd->i_gdr = 0;
	p_cmdd->p_audio_lst = gf_list_new();
	p_cmdd->p_video_lst = gf_list_new();
	p_cmdd->p_asrc = gf_list_new();
	p_cmdd->p_vsrc = gf_list_new();

}

void dc_cmd_data_destroy(CmdData * p_cmdd) {

	gf_list_del(p_cmdd->p_audio_lst);
	gf_list_del(p_cmdd->p_video_lst);
	gf_list_del(p_cmdd->p_asrc);
	gf_list_del(p_cmdd->p_vsrc);
	gf_cfg_del(p_cmdd->p_conf);
	gf_cfg_del(p_cmdd->p_switch_conf);

}

int dc_parse_command(int i_argc, char ** p_argv, CmdData * p_cmdd) {

	int i;

	char * psz_command_usage =
			"Usage: DashCast [options]\n"
					"\n"
					"Options:\n"
					"\n"
					"    -a <string>                  audio stream input source\n"
					"    -v <string>                  video stream input source\n"
					"    -av <string>                 multiplexed audio and video input source\n"
					"                                 (specify \"pipe:\" for piped input)\n"
					"    -vf <string>                 input video file format\n"
					"                                 (if necessary e.g. video4linux2, mpeg4)\n"
					"    -v4l2f <string>              camera input format\n"
					"                                 (if necessary e.g. mjpeg, yuyv422, etc.)\n"
					"    -vfr <int>                   input video framerate (if necessary)\n"
					"    -vres <intxint>              input video resolution (if necessary)\n"
					"    -af <string>                 input audio file format (if necessary)\n"
					"    -conf <string>               configuration file [default=dashcast.conf]\n\n"

					"    -seg-dur <int>               segment duration in millisecond [default=1000]\n"
					"    -frag-dur <int>              fragment duration in millisecond [default=1000]\n"
					"    -live                        live system from a camera\n"
					"    -live-media                  live system from a media file\n"
					"    -no-loop                     system does not loop on the input media file\n"
					"    -seg-marker <string>         add a marker box at the end of DASH segment\n"
					"    -gdr                         use Gradual Decoder Refresh feature for video encoding\n\n"

					"    -out <string>                output data directory name [default=output]\n"
					"    -mpd <string>                MPD file name [default=dashcast.mpd]\n"
					"    -ast-offset <int>            MPD AvailabilityStartTime offset in milliseconds [default=1000]\n"
					"    -time-shift <int>            MPD TimeShiftBufferDepth in seconds [default=10]\n"
					"                                 (specify -1 to keep all files)\n"
					"    -min-buffer <float>          MPD MinBufferTime in seconds [default=1.0]\n"

					"\n";

	char * psz_command_error =
			"\33[31mUnknown option or missing mandatory argument.\33[0m\n";

	if (i_argc == 1) {
		printf("%s", psz_command_usage);
		return -1;
	}

	i = 1;
	while (i < i_argc) {

		if (strcmp(p_argv[i], "-a") == 0 || strcmp(p_argv[i], "-v") == 0
				|| strcmp(p_argv[i], "-av") == 0) {

			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}

			if (strcmp(p_argv[i - 1], "-a") == 0
					|| strcmp(p_argv[i - 1], "-av") == 0) {
				if (strcmp(p_cmdd->adata.psz_name, "") != 0) {
					printf("Audio source has been already specified.\n");
					printf("%s", psz_command_usage);
					return -1;
				}
				//p_cmdd->psz_asrc = malloc(strlen(p_argv[i]) + 1);
				strcpy(p_cmdd->adata.psz_name, p_argv[i]);
				strcat(p_cmdd->adata.psz_name, "\0");
			}

			if (strcmp(p_argv[i - 1], "-v") == 0
					|| strcmp(p_argv[i - 1], "-av") == 0) {
				if (strcmp(p_cmdd->vdata.psz_name, "") != 0) {
					printf("Video source has been already specified.\n");
					printf("%s", psz_command_usage);
					return -1;
				}

				//p_cmdd->psz_vsrc = malloc(strlen(p_argv[i]) + 1);
				strcpy(p_cmdd->vdata.psz_name, p_argv[i]);
				strcat(p_cmdd->vdata.psz_name, "\0");
			}

			i++;

		} else if (strcmp(p_argv[i], "-af") == 0
				|| strcmp(p_argv[i], "-vf") == 0) {

			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}

			if (strcmp(p_argv[i - 1], "-af") == 0) {
				if (strcmp(p_cmdd->adata.psz_format, "") != 0) {
					printf("Audio format has been already specified.\n");
					printf("%s", psz_command_usage);
					return -1;
				}
				//p_cmdd->psz_af = malloc(strlen(p_argv[i]) + 1);
				strcpy(p_cmdd->adata.psz_format, p_argv[i]);
				strcat(p_cmdd->adata.psz_format, "\0");
			}

			if (strcmp(p_argv[i - 1], "-vf") == 0) {
				if (strcmp(p_cmdd->vdata.psz_format, "") != 0) {
					printf("Video format has been already specified.\n");
					printf("%s", psz_command_usage);
					return -1;
				}

				//p_cmdd->psz_vf = malloc(strlen(p_argv[i]) + 1);
				strcpy(p_cmdd->vdata.psz_format, p_argv[i]);
				strcat(p_cmdd->vdata.psz_format, "\0");
			}

			i++;

		} else if (strcmp(p_argv[i], "-vfr") == 0) {

			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}

			if (p_cmdd->vdata.i_framerate != -1) {
				printf("Video framerate has been already specified.\n");
				printf("%s", psz_command_usage);
				return -1;
			}
			//p_cmdd->psz_vfr = malloc(strlen(p_argv[i]) + 1);
			p_cmdd->vdata.i_framerate = atoi(p_argv[i]);
			//strcpy(p_cmdd->psz_vfr, p_argv[i]);
			//strcat(p_cmdd->psz_vfr, "\0");

			i++;

		} else if (strcmp(p_argv[i], "-vres") == 0) {

			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}

			if (p_cmdd->vdata.i_height != -1 && p_cmdd->vdata.i_width != -1) {
				printf("Video resolution has been already specified.\n");
				printf("%s", psz_command_usage);
				return -1;
			}
			dc_str_to_resolution(p_argv[i], &p_cmdd->vdata.i_width,
					&p_cmdd->vdata.i_height);

			i++;

		} else if (strcmp(p_argv[i], "-conf") == 0) {
			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}

			p_cmdd->p_conf = gf_cfg_force_new(NULL, p_argv[i]);

			i++;

		} else if (strcmp(p_argv[i], "-switch-source") == 0) {
			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}

			p_cmdd->p_switch_conf = gf_cfg_force_new(NULL, p_argv[i]);

			i++;

		} else if (strcmp(p_argv[i], "-out") == 0) {
			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}

			strcpy(p_cmdd->psz_out, p_argv[i]);
			strcat(p_cmdd->psz_out, "\0");

			i++;

		} else if (strcmp(p_argv[i], "-v4l2f") == 0) {
			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}

			strcpy(p_cmdd->vdata.psz_v4l2f, p_argv[i]);
			strcat(p_cmdd->vdata.psz_v4l2f, "\0");

			i++;

		} else if (strcmp(p_argv[i], "-seg-marker") == 0) {
			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}
			char * m = p_argv[i];
			if (strlen(m) == 4) {
				p_cmdd->i_seg_marker = GF_4CC(m[0], m[1], m[2], m[3]);
			} else {
				printf("Invalid marker box name specified: %s\n", m);
				return -1;
			}

			i++;

		} else if (strcmp(p_argv[i], "-mpd") == 0) {
			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}

			if (strcmp(p_cmdd->psz_mpd, "") != 0) {
				printf("MPD file has been already specified.\n");
				printf("%s", psz_command_usage);
				return -1;
			}

			strncpy(p_cmdd->psz_mpd, p_argv[i], GF_MAX_PATH);
			i++;

		} else if (strcmp(p_argv[i], "-seg-dur") == 0) {
			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}

			if (p_cmdd->i_seg_dur != 0) {
				printf("Segment duration has been already specified.\n");
				printf("%s", psz_command_usage);
				return -1;
			}

			p_cmdd->i_seg_dur = atoi(p_argv[i]);
			i++;

		} else if (strcmp(p_argv[i], "-frag-dur") == 0) {
			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}

			if (p_cmdd->i_frag_dur != 0) {
				printf("Fragment duration has been already specified.\n");
				printf("%s", psz_command_usage);
				return -1;
			}

			p_cmdd->i_frag_dur = atoi(p_argv[i]);
			i++;

		} else if (strcmp(p_argv[i], "-ast-offset") == 0) {
			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}

			if (p_cmdd->i_ast_offset != -1) {
				printf(
						"AvailabilityStartTime offset has been already specified.\n");
				printf("%s", psz_command_usage);
				return -1;
			}

			p_cmdd->i_ast_offset = atoi(p_argv[i]);
			i++;

		} else if (strcmp(p_argv[i], "-time-shift") == 0) {
			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}

			if (p_cmdd->i_time_shift != 0) {
				printf("TimeShiftBufferDepth has been already specified.\n");
				printf("%s", psz_command_usage);
				return -1;
			}

			p_cmdd->i_time_shift = atoi(p_argv[i]);
			i++;

		}

		else if (strcmp(p_argv[i], "-min-buffer") == 0) {
			i++;
			if (i >= i_argc) {
				printf("%s", psz_command_error);
				printf("%s", psz_command_usage);
				return -1;
			}
			if (p_cmdd->f_minbuftime != -1) {
				printf("Min Buffer Time has been already specified.\n");
				printf("%s", psz_command_usage);
				return -1;
			}

			p_cmdd->f_minbuftime = atof(p_argv[i]);
			i++;

		} else if (strcmp(p_argv[i], "-live") == 0) {
			p_cmdd->i_mode = LIVE_CAMERA;
			i++;
		} else if (strcmp(p_argv[i], "-live-media") == 0) {
			p_cmdd->i_mode = LIVE_MEDIA;
			i++;
		} else if (strcmp(p_argv[i], "-no-loop") == 0) {
			p_cmdd->i_no_loop = 1;
			i++;
		} else if (strcmp(p_argv[i], "-send-message") == 0) {
			p_cmdd->i_send_message = 1;
			i++;
		} else if (strcmp(p_argv[i], "-gdr") == 0) {
			p_cmdd->i_gdr = 1;
			i++;
		} else {
			printf("%s", psz_command_error);
			printf("%s", psz_command_usage);
			return -1;
		}

	}

	if (strcmp(p_cmdd->psz_mpd, "") == 0) {
		strcpy(p_cmdd->psz_mpd, "dashcast.mpd");
	}

	if (strcmp(p_cmdd->psz_out, "") == 0) {
		strcpy(p_cmdd->psz_out, "output/");

		struct stat status;

		if (stat(p_cmdd->psz_out, &status) != 0) {
			mkdir(p_cmdd->psz_out, 0777);
		}
	}

	if (strcmp(p_cmdd->vdata.psz_name, "") == 0
			&& strcmp(p_cmdd->adata.psz_name, "") == 0) {
		printf("Audio/Video source must be specified.\n");
		printf("%s", psz_command_usage);
		return -1;
	}

	if (p_cmdd->i_seg_dur == 0) {
		p_cmdd->i_seg_dur = 1000;
	}

	if (p_cmdd->i_frag_dur == 0) {
		p_cmdd->i_frag_dur = p_cmdd->i_seg_dur;
	}

	if (p_cmdd->i_ast_offset == -1) {
		p_cmdd->i_ast_offset = 1000;
	}

	if (p_cmdd->i_mode == ON_DEMAND)
		p_cmdd->i_time_shift = -1;
	else {
		if (p_cmdd->i_time_shift == 0) {
			p_cmdd->i_time_shift = 10;
		}
	}

	if (p_cmdd->f_minbuftime == -1) {
		p_cmdd->f_minbuftime = 1.0;
	}

	printf("\33[34m\33[1m");
	printf("Options:\n");
	printf("    video source: %s\n", p_cmdd->vdata.psz_name);
	if (strcmp(p_cmdd->vdata.psz_format, "") != 0) {
		printf("    video format: %s\n", p_cmdd->vdata.psz_format);
	}
	if (strcmp(p_cmdd->vdata.psz_v4l2f, "") != 0) {
		printf("    v4l2 format: %s\n", p_cmdd->vdata.psz_v4l2f);
	}
	if (p_cmdd->vdata.i_framerate != -1) {
		printf("    video framerate: %d\n", p_cmdd->vdata.i_framerate);
	}
	if (p_cmdd->vdata.i_height != -1 && p_cmdd->vdata.i_width != -1) {
		printf("    video resolution: %dx%d\n", p_cmdd->vdata.i_width,
				p_cmdd->vdata.i_height);
	}

	printf("    audio source: %s\n", p_cmdd->adata.psz_name);
	if (strcmp(p_cmdd->adata.psz_format, "") != 0) {
		printf("    audio format: %s\n", p_cmdd->adata.psz_format);
	}
	printf("\33[0m");
	fflush(stdout);

	if (!p_cmdd->p_conf) {
		p_cmdd->p_conf = gf_cfg_force_new(NULL, "dashcast.conf");
	}

	if (!p_cmdd->p_switch_conf) {
		p_cmdd->p_switch_conf = gf_cfg_force_new(NULL, "switch.conf");
	}

	dc_read_configuration(p_cmdd);

	dc_read_switch_config(p_cmdd);

	return 0;
}

