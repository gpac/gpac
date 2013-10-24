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


int dc_str_to_resolution(char *str, int *width, int *height)
{
	char *token = strtok(str, "x");
	if (!token) {
		fprintf(stderr, "Cannot parse resolution string.\n");
		return -1;
	}
	*width = atoi(token);

	token = strtok(NULL, " ");
	if (!token) {
		fprintf(stderr, "Cannot parse resolution string.\n");
		return -1;
	}
	*height = atoi(token);

	return 0;
}


#define DEFAULT_VIDEO_BITRATE    400000
#define DEFAULT_VIDEO_FRAMERATE  25
#define DEFAULT_VIDEO_WIDTH      640
#define DEFAULT_VIDEO_HEIGHT     480
#define DEFAULT_VIDEO_CODEC      "libx264"
#define DEFAULT_AUDIO_BITRATE    192000
#define DEFAULT_AUDIO_SAMPLERATE 48000
#define DEFAULT_AUDIO_CHANNELS   2
#define DEFAULT_AUDIO_CODEC      "mp2"

static void dc_create_configuration(CmdData *cmd_data)
{	
	u32 i;
	GF_Config *conf = cmd_data->conf;
	u32 sec_count = gf_cfg_get_section_count(conf);
	if (!sec_count) {
		gf_cfg_set_key(conf, "v1", "type", "video");
		gf_cfg_set_key(conf, "a1", "type", "audio");
		sec_count = gf_cfg_get_section_count(conf);
	}
	for (i=0; i<sec_count; i++) {
		char value[GF_MAX_PATH];		
		const char *section_name = gf_cfg_get_section_name(conf, i);
		const char *section_type = gf_cfg_get_key(conf, section_name, "type");

		if (strcmp(section_type, "video") == 0) {
			if (!gf_cfg_get_key(conf, section_name, "bitrate")) {
				if (cmd_data->video_data_conf.bitrate == -1)
					cmd_data->video_data_conf.bitrate = DEFAULT_VIDEO_BITRATE;
				snprintf(value, sizeof(value), "%d", cmd_data->video_data_conf.bitrate);
				gf_cfg_set_key(conf, section_name, "bitrate", value);
			}
		
			if (!gf_cfg_get_key(conf, section_name, "framerate")) {
				if (cmd_data->video_data_conf.framerate == -1)
					cmd_data->video_data_conf.framerate = DEFAULT_VIDEO_FRAMERATE;
				snprintf(value, sizeof(value), "%d", cmd_data->video_data_conf.framerate);
				gf_cfg_set_key(conf, section_name, "framerate", value);
			}
		
			if (!gf_cfg_get_key(conf, section_name, "width")) {
				if (cmd_data->video_data_conf.width == -1)
					cmd_data->video_data_conf.width = DEFAULT_VIDEO_WIDTH;
				snprintf(value, sizeof(value), "%d", cmd_data->video_data_conf.width);
				gf_cfg_set_key(conf, section_name, "width", value);
			}
		
			if (!gf_cfg_get_key(conf, section_name, "height")) {
				if (cmd_data->video_data_conf.height == -1)
					cmd_data->video_data_conf.height = DEFAULT_VIDEO_HEIGHT;
				snprintf(value, sizeof(value), "%d", cmd_data->video_data_conf.height);
				gf_cfg_set_key(conf, section_name, "height", value);
			}
			
			if (!gf_cfg_get_key(conf, section_name, "codec"))
				gf_cfg_set_key(conf, section_name, "codec", DEFAULT_VIDEO_CODEC);
		}
		
		if (strcmp(section_type, "audio") == 0) {
			if (!gf_cfg_get_key(conf, section_name, "bitrate")) {
				if (cmd_data->audio_data_conf.bitrate == -1)
					cmd_data->audio_data_conf.bitrate = DEFAULT_AUDIO_BITRATE;
				snprintf(value, sizeof(value), "%d", cmd_data->audio_data_conf.bitrate);
				gf_cfg_set_key(conf, section_name, "bitrate", value);
			}
			
			if (!gf_cfg_get_key(conf, section_name, "samplerate")) {
				if (cmd_data->audio_data_conf.samplerate == -1)
					cmd_data->audio_data_conf.samplerate = DEFAULT_AUDIO_SAMPLERATE;
				snprintf(value, sizeof(value), "%d", cmd_data->audio_data_conf.samplerate);
				gf_cfg_set_key(conf, section_name, "samplerate", value);
			}
			
			if (!gf_cfg_get_key(conf, section_name, "channels")) {
				if (cmd_data->audio_data_conf.channels == -1)
					cmd_data->audio_data_conf.channels = DEFAULT_AUDIO_CHANNELS;
				snprintf(value, sizeof(value), "%d", cmd_data->audio_data_conf.channels);
				gf_cfg_set_key(conf, section_name, "channels", value);
			}
			
			if (!gf_cfg_get_key(conf, section_name, "codec"))
				gf_cfg_set_key(conf, section_name, "codec", DEFAULT_AUDIO_CODEC);
		}
	}
}

int dc_read_configuration(CmdData *cmd_data)
{
	const char *opt;
	u32 i;
	GF_Config *conf = cmd_data->conf;

	u32 sec_count = gf_cfg_get_section_count(conf);
	for (i=0; i<sec_count; i++) {
		const char *section_name = gf_cfg_get_section_name(conf, i);
		const char *section_type = gf_cfg_get_key(conf, section_name, "type");

		if (strcmp(section_type, "video") == 0) {
			VideoDataConf *video_data_conf;
			GF_SAFEALLOC(video_data_conf, VideoDataConf);
			strcpy(video_data_conf->filename, section_name);
			opt = gf_cfg_get_key(conf, section_name, "codec");
			if (!opt) opt = DEFAULT_VIDEO_CODEC;
			strcpy(video_data_conf->codec, opt);
			opt = gf_cfg_get_key(conf, section_name, "bitrate");
			video_data_conf->bitrate = opt ? atoi(opt) : DEFAULT_VIDEO_BITRATE;
			opt = gf_cfg_get_key(conf, section_name, "framerate");
			video_data_conf->framerate = opt ? atoi(opt) : DEFAULT_VIDEO_FRAMERATE;
			opt = gf_cfg_get_key(conf, section_name, "height");
			video_data_conf->height = opt ? atoi(opt) : DEFAULT_VIDEO_HEIGHT;
			opt = gf_cfg_get_key(conf, section_name, "width");
			video_data_conf->width = opt ? atoi(opt) : DEFAULT_VIDEO_WIDTH;
			opt = gf_cfg_get_key(conf, section_name, "custom");
			video_data_conf->custom = opt ? strdup(opt) : NULL;
			gf_list_add(cmd_data->video_lst, (void *) video_data_conf);
		}
		else if (strcmp(section_type, "audio") == 0)
		{
			AudioDataConf *audio_data_conf;
			GF_SAFEALLOC(audio_data_conf, AudioDataConf);
			strcpy(audio_data_conf->filename, section_name);
			opt = gf_cfg_get_key(conf, section_name, "codec");
			if (!opt) opt = DEFAULT_AUDIO_CODEC;
			strcpy(audio_data_conf->codec, opt);
			opt = gf_cfg_get_key(conf, section_name, "bitrate");
			audio_data_conf->bitrate = opt ? atoi(opt) : DEFAULT_AUDIO_BITRATE;
			opt = gf_cfg_get_key(conf, section_name, "samplerate");
			audio_data_conf->samplerate = opt ? atoi(opt) : DEFAULT_AUDIO_SAMPLERATE;
			opt = gf_cfg_get_key(conf, section_name, "channels");
			audio_data_conf->channels = opt ? atoi(opt) : DEFAULT_AUDIO_CHANNELS;
			opt = gf_cfg_get_key(conf, section_name, "custom");
			audio_data_conf->custom = opt ? strdup(opt) : NULL;
			gf_list_add(cmd_data->audio_lst, (void *) audio_data_conf);
		} else {
			fprintf(stdout, "Configuration file: type %s is not supported.\n", section_type);
		}
	}

	fprintf(stdout, "\33[34m\33[1m");
	fprintf(stdout, "Configurations:\n");
	for (i=0; i<gf_list_count(cmd_data->video_lst); i++) {
		VideoDataConf *video_data_conf = gf_list_get(cmd_data->video_lst, i);
		fprintf(stdout, "    id:%s\tres:%dx%d\tvbr:%d\n", video_data_conf->filename,
				video_data_conf->width, video_data_conf->height,
				video_data_conf->bitrate/*, video_data_conf->framerate, video_data_conf->codec*/);	}

	for (i=0; i<gf_list_count(cmd_data->audio_lst); i++) {
		AudioDataConf *audio_data_conf = gf_list_get(cmd_data->audio_lst, i);
		fprintf(stdout, "    id:%s\tabr:%d\n", audio_data_conf->filename, audio_data_conf->bitrate/*, audio_data_conf->samplerate, audio_data_conf->channels,audio_data_conf->codec*/);
	}
	fprintf(stdout, "\33[0m");
	fflush(stdout);

	return 0;
}

/**
 * Parse time from a string to a struct tm.
 */
static Bool parse_time(const char* str_time, struct tm *tm_time)
{
	if (!tm_time)
		return GF_FALSE;

#if defined(__GNUC__)
	strptime(str_time, "%Y-%m-%d %H:%M:%S", tm_time);
#elif defined(WIN32)
	assert(0); //TODO
#else
#error
#endif

	return GF_TRUE;
}

int dc_read_switch_config(CmdData *cmd_data)
{
	u32 i;
	int src_number;
	char start_time[4096], end_time[4096];

	time_t now_t = time(NULL);
	struct tm start_tm = *localtime(&now_t);
	struct tm end_tm = *localtime(&now_t);

	GF_Config *conf = cmd_data->switch_conf;
	u32 sec_count = gf_cfg_get_section_count(conf);

	dc_task_init(&cmd_data->task_list);

	if (sec_count == 0) {
		return 0;
	}

	for (i = 0; i < sec_count; i++) {
		const char *section_name = gf_cfg_get_section_name(conf, i);
		const char *section_type = gf_cfg_get_key(conf, section_name, "type");

		if (strcmp(section_type, "video") == 0) {
			VideoDataConf *video_data_conf = gf_malloc(sizeof(VideoDataConf));

			strcpy(video_data_conf->source_id, section_name);
			strcpy(video_data_conf->filename, gf_cfg_get_key(conf, section_name, "source"));

			strcpy(start_time, gf_cfg_get_key(conf, section_name, "start"));
			parse_time(start_time, &start_tm);
			video_data_conf->start_time = mktime(&start_tm);
			strcpy(end_time, gf_cfg_get_key(conf, section_name, "end"));
			parse_time(end_time, &end_tm);
			video_data_conf->end_time = mktime(&end_tm);

			gf_list_add(cmd_data->vsrc, (void *) video_data_conf);

			src_number = gf_list_count(cmd_data->vsrc);

			dc_task_add(&cmd_data->task_list, src_number, video_data_conf->source_id, video_data_conf->start_time, video_data_conf->end_time);
		}
		else if (strcmp(section_type, "audio") == 0)
		{
			AudioDataConf *audio_data_conf = gf_malloc(sizeof(AudioDataConf));
			strcpy(audio_data_conf->source_id, section_name);
			strcpy(audio_data_conf->filename, gf_cfg_get_key(conf, section_name, "source"));

			strcpy(start_time, gf_cfg_get_key(conf, section_name, "start"));
			parse_time(start_time, &start_tm);
			audio_data_conf->start_time = mktime(&start_tm);

			strcpy(end_time, gf_cfg_get_key(conf, section_name, "end"));
			parse_time(end_time, &end_tm);
			audio_data_conf->end_time = mktime(&end_tm);

			gf_list_add(cmd_data->asrc, (void *) audio_data_conf);
		} else {
			fprintf(stdout, "Switch source configuration file: type %s is not supported.\n", section_type);
		}
	}

	fprintf(stdout, "\33[34m\33[1m");
	fprintf(stdout, "Sources:\n");
	for (i=0; i<gf_list_count(cmd_data->vsrc); i++) {
		VideoDataConf *video_data_conf = gf_list_get(cmd_data->vsrc, i);
		strftime(start_time, 20, "%Y-%m-%d %H:%M:%S", localtime(&video_data_conf->start_time));
		strftime(end_time, 20, "%Y-%m-%d %H:%M:%S", localtime(&video_data_conf->end_time));
		fprintf(stdout, "    id:%s\tsource:%s\tstart:%s\tend:%s\n", video_data_conf->source_id, video_data_conf->filename, start_time, end_time);
	}

	for (i=0; i<gf_list_count(cmd_data->asrc); i++) {
		AudioDataConf *audio_data_conf = gf_list_get(cmd_data->asrc, i);
		strftime(start_time, 20, "%Y-%m-%d %H:%M:%S", localtime(&audio_data_conf->start_time));
		strftime(end_time, 20, "%Y-%m-%d %H:%M:%S", localtime(&audio_data_conf->end_time));
		fprintf(stdout, "    id:%s\tsource:%s\tstart:%s\tend:%s\n", audio_data_conf->source_id, audio_data_conf->filename, start_time, end_time);
	}
	fprintf(stdout, "\33[0m");
	fflush(stdout);

	return 0;
}

void dc_cmd_data_init(CmdData *cmd_data)
{
	memset(cmd_data, 0, sizeof(CmdData));
	dc_audio_data_set_default(&cmd_data->audio_data_conf);
	dc_video_data_set_default(&cmd_data->video_data_conf);

	cmd_data->mode = ON_DEMAND;
	cmd_data->ast_offset = -1;
	cmd_data->min_buffer_time = -1;
	cmd_data->use_source_timing = 1;

	cmd_data->audio_lst = gf_list_new();
	cmd_data->video_lst = gf_list_new();
	cmd_data->asrc = gf_list_new();
	cmd_data->vsrc = gf_list_new();
}

void dc_cmd_data_destroy(CmdData *cmd_data)
{
	while (gf_list_count(cmd_data->audio_lst)) {
		AudioDataConf *audio_data_conf = gf_list_last(cmd_data->audio_lst);
		gf_list_rem_last(cmd_data->audio_lst);
		gf_free(audio_data_conf);
	}
	gf_list_del(cmd_data->audio_lst);

	while (gf_list_count(cmd_data->video_lst)) {
		VideoDataConf *video_data_conf = gf_list_last(cmd_data->video_lst);
		gf_list_rem_last(cmd_data->video_lst);
		gf_free(video_data_conf);
	}
	gf_list_del(cmd_data->video_lst);

	gf_list_del(cmd_data->asrc);
	gf_list_del(cmd_data->vsrc);
	gf_cfg_del(cmd_data->conf);
	gf_cfg_del(cmd_data->switch_conf);
	if (cmd_data->logfile)
		fclose(cmd_data->logfile);

	dc_task_destroy(&cmd_data->task_list);

	gf_sys_close();
}

static void on_dc_log(void *cbk, u32 ll, u32 lm, const char *fmt, va_list list)
{
	FILE *logs = cbk;
	vfprintf(logs, fmt, list);
	fflush(logs);
}

int dc_parse_command(int argc, char **argv, CmdData *cmd_data)
{
	Bool use_mem_track = GF_FALSE;
	int i;

	const char *command_usage =
			"Usage: DashCast [options]\n"
					"\n"
					"Options:\n"
					"\n"
					"    -log-file file               set output log file. Also works with -lf\n"
					"    -logs LOGS                   set log tools and levels, formatted as a ':'-separated list of toolX[:toolZ]@levelX\n"
					"    -a inasrc:str                input audio source named inasrc\n"
			    "                                    - If input is from microphone, inasrc will be \"plughw:[x],[y]\"\n"
			    "                                      where x is the card number and y is the device number.\n"
					"    -v invsrc:str                input video source named invsrc\n"
			    "                                    - If input is from a webcam, invsrc will be \"/dev/video[x]\" \n"
			    "                                      where x is the video device number.\n"
			    "                                    - If input is the screen video, invsrc will be \":0.0+[x],[y]\" \n"
			    "                                      which captures from upper-left at x,y.\n"
			    "                                    - If input is from stdin, invsrc will be \"pipe:\".\n"
					"    -av inavsrc:str              a multiplexed audio and video source named inavsrc\n"
					"                                    - If this option is present, non of '-a' or '-v' can be present.\n"
					"    -vf invfmt:str               invfmt is the input video format\n"
#ifdef WIN32
					"                                    - To capture from a VfW webcam invfmt will be vfwcap."
					"                                    - To capture from a directshow device invfmt will be dshow."
#else
					"                                    - To capture from a webcam invfmt will be video4linux2.\n"
					"                                    - To capture the screen invfmt will be x11grab.\n"
					"    -v4l2f inv4l2f:str           inv4l2f is the input format for webcam acquisition\n"
					"                                    - It can be mjpeg, yuyv422, etc.\n"
#endif
					"    -pixf FMT                    spcifies the input pixel format to use\n"
					"    -vfr invfr:int               invfr is the input video framerate\n"
					"    -vres invres:intxint         input video resolution\n"
					"    -af inafmt:str               inafmt is the input audio format\n"
					"    -conf confname:str           confname is the configuration file\n"
					"                                    - The default value is dashcast.conf\n\n"

					"    -seg-dur dur:int             dur is the segment duration in millisecond\n"
					"                                    - The default value is 1000.\n"
					"    -frag-dur dur:int            dur is the fragment duration in millisecond\n"
					"                                    - The default value is 1000.\n"
					"    -live                        system is live and input is a camera\n"
					"    -npts                        uses frame counting for timestamps (not error-free) instead of source timing (default)\n"
					"    -live-media                  system is live and input is a media file\n"
					"    -no-loop                     system does not loop on the input media file\n"
					"    -seg-marker marker:str       add a marker box named marker at the end of DASH segment\n"
					"    -gdr                         use Gradual Decoder Refresh feature for video encoding\n\n"

					"    -out outdir:str              outdir is the output data directory\n"
					"                                    - The default value is output.\n"
					"    -mpd mpdname:str             mpdname is the MPD file name\n"
					"                                    - The default value is dashcast.mpd.\n"
					"    -ast-offset dur:int          dur is the MPD availabilityStartTime shift in milliseconds\n"
					"                                    - The default value is 1000.\n"
					"    -time-shift dur:int          dur is the MPD TimeShiftBufferDepth in seconds\n"
					"                                    - The default value is 10. Specify -1 to keep all files.\n"
					"    -min-buffer dur:float        dur is the MPD minBufferTime in seconds\n"
					"                                    - The default value is 1.0.\n\n"

					"    -switch-source confname:str  confname is the name of configuration file for source switching.\n\n"


					"Examples:\n"
			        "\n"
			        "    DashCast -av test.avi -live-media\n"
					    "    DashCast -a test_audio.mp3 -v test_audio.mp4 -live-media\n"
#ifdef WIN32
	        		"    DashCast -vf vfwcap -vres 1280x720 -vfr 24 -v 0 -live\n"
					"    DashCast -vf dshow -vres 1280x720 -vfr 24 -v video=\"screen-capture-recorder\" -live (please install http://screencapturer.sf.net/)\n"
	        		"    DashCast -vf dshow -vres 1280x720 -vfr 24 -v video=\"YOUR-WEBCAM\" -pixf yuv420p -live\n"
#else
	        		"    DashCast -vf video4linux2 -vres 1280x720 -vfr 24 -v4l2f mjpeg -v /dev/video0 -af alsa -a plughw:1,0 -live\n"
	        		"    DashCast -vf x11grab -vres 800x600 -vfr 25 -v :0.0 -live\n"
#endif
					"\n";

	char *command_error = "\33[31mUnknown option or missing mandatory argument.\33[0m\n";

	if (argc == 1) {
		fprintf(stdout, "%s", command_usage);
		return -2;
	}

#ifdef GPAC_MEMORY_TRACKING
	i = 1;
	while (i < argc) {
		if (strcmp(argv[i], "-mem-track") == 0) {
			use_mem_track = GF_TRUE;
			break;
		}
		i++;
	}
#endif

	gf_sys_init(use_mem_track);

	if (use_mem_track) {
		gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
	}

	/* Initialize command data */
	dc_cmd_data_init(cmd_data);

	i = 1;
	while (i < argc) {
		if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-av") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			if (strcmp(argv[i - 1], "-a") == 0 || strcmp(argv[i - 1], "-av") == 0) {
				if (strcmp(cmd_data->audio_data_conf.filename, "") != 0) {
					fprintf(stdout, "Audio source has been already specified.\n");
					fprintf(stdout, "%s", command_usage);
					return -1;
				}
				strcpy(cmd_data->audio_data_conf.filename, argv[i]);
			}

			if (strcmp(argv[i - 1], "-v") == 0 || strcmp(argv[i - 1], "-av") == 0) {
				if (strcmp(cmd_data->video_data_conf.filename, "") != 0) {
					fprintf(stdout, "Video source has been already specified.\n");
					fprintf(stdout, "%s", command_usage);
					return -1;
				}
				strcpy(cmd_data->video_data_conf.filename, argv[i]);
			}

			i++;
		} else if (strcmp(argv[i], "-af") == 0 || strcmp(argv[i], "-vf") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			if (strcmp(argv[i - 1], "-af") == 0) {
				if (strcmp(cmd_data->audio_data_conf.format, "") != 0) {
					fprintf(stdout, "Audio format has been already specified.\n");
					fprintf(stdout, "%s", command_usage);
					return -1;
				}
				strcpy(cmd_data->audio_data_conf.format, argv[i]);
			}

			if (strcmp(argv[i - 1], "-vf") == 0) {
				if (strcmp(cmd_data->video_data_conf.format, "") != 0) {
					fprintf(stdout, "Video format has been already specified.\n");
					fprintf(stdout, "%s", command_usage);
					return -1;
				}
				strcpy(cmd_data->video_data_conf.format, argv[i]);
			}

			i++;
		} else if (strcmp(argv[i], "-pixf") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}
			if (strcmp(cmd_data->video_data_conf.pixel_format, "") != 0) {
				fprintf(stdout, "Input pixel format has been already specified.\n");
				fprintf(stdout, "%s", command_usage);
				return -1;
			}
			strcpy(cmd_data->video_data_conf.pixel_format, argv[i]);

			i++;
		} else if (strcmp(argv[i], "-vfr") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			if (cmd_data->video_data_conf.framerate != -1) {
				fprintf(stdout, "Video framerate has been already specified.\n");
				fprintf(stdout, "%s", command_usage);
				return -1;
			}
			cmd_data->video_data_conf.framerate = atoi(argv[i]);
			i++;
		} else if (strcmp(argv[i], "-vres") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			if (cmd_data->video_data_conf.height != -1 && cmd_data->video_data_conf.width != -1) {
				fprintf(stdout, "Video resolution has been already specified.\n");
				fprintf(stdout, "%s", command_usage);
				return -1;
			}
			dc_str_to_resolution(argv[i], &cmd_data->video_data_conf.width, &cmd_data->video_data_conf.height);

			i++;
		} else if (strcmp(argv[i], "-conf") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			cmd_data->conf = gf_cfg_force_new(NULL, argv[i]);

			i++;

		} else if (strcmp(argv[i], "-switch-source") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			cmd_data->switch_conf = gf_cfg_force_new(NULL, argv[i]);

			i++;
		} else if (strcmp(argv[i], "-out") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			strcpy(cmd_data->out_dir, argv[i]);

			i++;
#ifndef WIN32
		} else if (strcmp(argv[i], "-v4l2f") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			strcpy(cmd_data->video_data_conf.v4l2f, argv[i]);

			i++;
#endif
		} else if (strcmp(argv[i], "-seg-marker") == 0) {
			char *m;
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}
			m = argv[i];
			if (strlen(m) == 4) {
				cmd_data->seg_marker = GF_4CC(m[0], m[1], m[2], m[3]);
			} else {
				fprintf(stdout, "Invalid marker box name specified: %s\n", m);
				return -1;
			}

			i++;

		} else if (strcmp(argv[i], "-mpd") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			if (strcmp(cmd_data->mpd_filename, "") != 0) {
				fprintf(stdout, "MPD file has been already specified.\n");
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			strncpy(cmd_data->mpd_filename, argv[i], GF_MAX_PATH);
			i++;

		} else if (strcmp(argv[i], "-seg-dur") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			if (cmd_data->seg_dur != 0) {
				fprintf(stdout, "Segment duration has been already specified.\n");
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			cmd_data->seg_dur = atoi(argv[i]);
			i++;

		} else if (strcmp(argv[i], "-frag-dur") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			if (cmd_data->frag_dur != 0) {
				fprintf(stdout, "Fragment duration has been already specified.\n");
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			cmd_data->frag_dur = atoi(argv[i]);
			i++;

		} else if (strcmp(argv[i], "-ast-offset") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			if (cmd_data->ast_offset != -1) {
				fprintf(stdout, "AvailabilityStartTime offset has been already specified.\n");
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			cmd_data->ast_offset = atoi(argv[i]);
			i++;

		} else if (strcmp(argv[i], "-time-shift") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			if (cmd_data->time_shift != 0) {
				fprintf(stdout, "TimeShiftBufferDepth has been already specified.\n");
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			cmd_data->time_shift = atoi(argv[i]);
			i++;

		} else if (strcmp(argv[i], "-min-buffer") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}
			if (cmd_data->min_buffer_time != -1) {
				fprintf(stdout, "Min Buffer Time has been already specified.\n");
				fprintf(stdout, "%s", command_usage);
				return -1;
			}

			cmd_data->min_buffer_time = (float)atof(argv[i]);
			i++;

		} else if (strcmp(argv[i], "-live") == 0) {
			cmd_data->mode = LIVE_CAMERA;
			i++;
		} else if (strcmp(argv[i], "-npts") == 0) {
			cmd_data->use_source_timing = 0;
			i++;
		} else if (strcmp(argv[i], "-live-media") == 0) {
			cmd_data->mode = LIVE_MEDIA;
			i++;
		} else if (strcmp(argv[i], "-no-loop") == 0) {
			cmd_data->no_loop = 1;
			i++;
		} else if (strcmp(argv[i], "-send-message") == 0) {
			cmd_data->send_message = 1;
			i++;
		} else if (strcmp(argv[i], "-logs") == 0) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}
			if (gf_log_set_tools_levels(argv[i]) != GF_OK) {
				fprintf(stdout, "Invalid log format %s", argv[i]);
				return 1;
			}
			i++;
		} else if (strcmp(argv[i], "-mem-track") == 0) {
			i++;
#ifndef GPAC_MEMORY_TRACKING
			fprintf(stderr, "WARNING - GPAC not compiled with Memory Tracker - ignoring \"-mem-track\"\n");
#endif
		} else if (!strcmp(argv[i], "-lf") || !strcmp(argv[i], "-log-file")) {
			i++;
			if (i >= argc) {
				fprintf(stdout, "%s", command_error);
				fprintf(stdout, "%s", command_usage);
				return -1;
			}
			cmd_data->logfile = gf_f64_open(argv[i], "wt");
			gf_log_set_callback(cmd_data->logfile, on_dc_log);
			i++;
		} else if (strcmp(argv[i], "-gdr") == 0) {
			cmd_data->gdr = 1;
			i++;
		} else {
			fprintf(stdout, "%s", command_error);
			fprintf(stdout, "%s", command_usage);
			return -1;
		}
	}

	if (strcmp(cmd_data->mpd_filename, "") == 0) {
		strcpy(cmd_data->mpd_filename, "dashcast.mpd");
	}

	if (strcmp(cmd_data->out_dir, "") == 0) {
		struct stat status;
		strcpy(cmd_data->out_dir, "output");
		if (stat(cmd_data->out_dir, &status) != 0) {
			gf_mkdir(cmd_data->out_dir);
		}
	}

	if (strcmp(cmd_data->video_data_conf.filename, "") == 0 && strcmp(cmd_data->audio_data_conf.filename, "") == 0) {
		fprintf(stdout, "Audio/Video source must be specified.\n");
		fprintf(stdout, "%s", command_usage);
		return -1;
	}

	if (cmd_data->seg_dur == 0) {
		cmd_data->seg_dur = 1000;
	}

	if (cmd_data->frag_dur == 0) {
		cmd_data->frag_dur = cmd_data->seg_dur;
	}

	if (cmd_data->ast_offset == -1) {
		//generate MPD as soon as possible (no offset)
		cmd_data->ast_offset = 0;
	}

	if (cmd_data->mode == ON_DEMAND)
		cmd_data->time_shift = -1;
	else {
		if (cmd_data->time_shift == 0) {
			cmd_data->time_shift = 10;
		}
	}

	if (cmd_data->min_buffer_time == -1) {
		cmd_data->min_buffer_time = 1.0;
	}

	fprintf(stdout, "\33[34m\33[1m");
	fprintf(stdout, "Options:\n");
	fprintf(stdout, "    video source: %s\n", cmd_data->video_data_conf.filename);
	if (strcmp(cmd_data->video_data_conf.format, "") != 0) {
		fprintf(stdout, "    video format: %s\n", cmd_data->video_data_conf.format);
	}
#ifndef WIN32
	if (strcmp(cmd_data->video_data_conf.v4l2f, "") != 0) {
		fprintf(stdout, "    v4l2 format: %s\n", cmd_data->video_data_conf.v4l2f);
	}
#endif
	if (cmd_data->video_data_conf.framerate != -1) {
		fprintf(stdout, "    video framerate: %d\n", cmd_data->video_data_conf.framerate);
	}
	if (cmd_data->video_data_conf.height != -1 && cmd_data->video_data_conf.width != -1) {
		fprintf(stdout, "    video resolution: %dx%d\n", cmd_data->video_data_conf.width, cmd_data->video_data_conf.height);
	}

	fprintf(stdout, "    audio source: %s\n", cmd_data->audio_data_conf.filename);
	if (strcmp(cmd_data->audio_data_conf.format, "") != 0) {
		fprintf(stdout, "    audio format: %s\n", cmd_data->audio_data_conf.format);
	}
	fprintf(stdout, "\33[0m");
//	fflush(stdout);
		
	if (!cmd_data->conf) {
		cmd_data->conf = gf_cfg_force_new(NULL, "dashcast.conf");
		dc_create_configuration(cmd_data);
	}
	dc_read_configuration(cmd_data);

	if (!cmd_data->switch_conf) {
		cmd_data->switch_conf = gf_cfg_force_new(NULL, "switch.conf");
	}
	dc_read_switch_config(cmd_data);

	return 0;
}
