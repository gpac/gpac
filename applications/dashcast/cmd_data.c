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


#define DASHCAST_CHECK_NEXT_ARG \
			i++; \
			if (i >= argc) { \
				fprintf(stderr, "%s: %s", command_error, argv[i]); \
				fprintf(stderr, "%s", command_usage); \
				return -1; \
			}


int dc_str_to_resolution(char *str, int *width, int *height)
{
	char *token = strtok(str, "x");
	if (!token) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot parse resolution string.\n"));
		return -1;
	}
	*width = atoi(token);

	token = strtok(NULL, " ");
	if (!token) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot parse resolution string.\n"));
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
#define DEFAULT_AUDIO_SAMPLERATE 44100
#define DEFAULT_AUDIO_CHANNELS   2
#define DEFAULT_AUDIO_CODEC      "aac"


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

			if (!gf_cfg_get_key(conf, section_name, "crop_x")) {
				if (cmd_data->video_data_conf.crop_x == -1)
					cmd_data->video_data_conf.crop_x = 0;
				snprintf(value, sizeof(value), "%d", cmd_data->video_data_conf.crop_x);
				gf_cfg_set_key(conf, section_name, "crop_x", value);
			}
			if (!gf_cfg_get_key(conf, section_name, "low_delay")) {
				gf_cfg_set_key(conf, section_name, "low_delay", cmd_data->video_data_conf.low_delay ? "yes" : "no");
			}

			if (!gf_cfg_get_key(conf, section_name, "crop_y")) {
				if (cmd_data->video_data_conf.crop_y == -1)
					cmd_data->video_data_conf.crop_y = 0;
				snprintf(value, sizeof(value), "%d", cmd_data->video_data_conf.crop_y);
				gf_cfg_set_key(conf, section_name, "crop_y", value);
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
			opt = gf_cfg_get_key(conf, section_name, "crop_x");
			video_data_conf->crop_x = opt ? atoi(opt) : 0;
			opt = gf_cfg_get_key(conf, section_name, "crop_y");
			video_data_conf->crop_x = opt ? atoi(opt) : 0;
			opt = gf_cfg_get_key(conf, section_name, "low_delay");
			video_data_conf->low_delay = (opt && !strcmp(opt, "yes")) ? 1 : 0;
			opt = gf_cfg_get_key(conf, section_name, "custom");
			if (opt) {
				if (strlen(opt) >= GF_MAX_PATH)
					fprintf(stderr, "Warning: video custom opt is too long. Truncating.\n");
				strncpy(video_data_conf->custom, opt, GF_MAX_PATH-1);
			}
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
			if (opt) {
				if (strlen(opt) >= GF_MAX_PATH)
					fprintf(stderr, "Warning: audio custom opt is too long. Truncating.\n");
				strncpy(audio_data_conf->custom, opt, GF_MAX_PATH-1);
			}
			gf_list_add(cmd_data->audio_lst, (void *) audio_data_conf);
		} else {
			fprintf(stderr, "Configuration file: type %s is not supported.\n", section_type);
		}
	}

	fprintf(stdout, "\33[34m\33[1m");
	fprintf(stdout, "Configurations:\n");
	for (i=0; i<gf_list_count(cmd_data->video_lst); i++) {
		VideoDataConf *video_data_conf = gf_list_get(cmd_data->video_lst, i);
		fprintf(stdout, "    id:%s\tres:%dx%d\tvbr:%d\n", video_data_conf->filename,
		        video_data_conf->width, video_data_conf->height,
		        video_data_conf->bitrate/*, video_data_conf->framerate, video_data_conf->codec*/);
	}

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
	cmd_data->minimum_update_period = -1;
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

static void on_dc_log(void *cbk, u32 ll, u32 lm, const char *av_fmt_ctx, va_list list)
{
	FILE *logs = cbk;
	vfprintf(logs, av_fmt_ctx, list);
	fflush(logs);
}

int dc_parse_command(int argc, char **argv, CmdData *cmd_data)
{
	Bool use_mem_track = GF_FALSE;
	int i;

	const char *command_usage =
	    "Usage: DashCast [options]\n"
	    "\n"
	    "General options:\n"
	    "    -log-file filename       set output log file. Also works with -lf\n"
	    "    -logs LOGS               set log tools and levels, formatted as a ':'-separated list of toolX[:toolZ]@levelX\n"
#ifdef GPAC_MEMORY_TRACKING
	    "    -mem-track               enable the memory tracker\n"
#endif
	    "    -conf filename           set the configuration file name (default: dashcast.conf)\n"
	    "    -switch-source filename  set the configuration file name for source switching\n"
	    "\n"
	    "Live options:\n"
	    "    -live                    system is live and input is a camera\n"
	    "    -live-media              system is live and input is a media file\n"
	    "    -no-loop                 system does not loop on the input media file when live\n"
	    "    -dynamic-ast             changes segment availability start time at each MPD generation (old behaviour but not allowed in most profiles)\n"
	    "    -insert-utc              inserts UTC clock at the start of each segment\n"
	    "\n"
	    "Source options:\n"
	    "    -npts                    use frame counting for timestamps (not error-free) instead of source timing (default)\n"
	    "    -av string               set the source name for a multiplexed audio and video input\n"
	    "                                - if this option is present, neither '-a' nor '-v' shall be present\n"
	    "* Video options:\n"
	    "    -v string                set the source name for a video input\n"
	    "                                - if input is from a webcam, use \"/dev/video[x]\" \n"
	    "                                  where x is the video device number\n"
	    "                                - if input is the screen video, use \":0.0+[x],[y]\" \n"
	    "                                  which captures from upper-left at x,y\n"
	    "                                - if input is from stdin, use \"pipe:\"\n"
	    "    -vf string               set the input video format\n"
#ifdef WIN32
	    "                                - to capture from a VfW webcam, set vfwcap\n"
	    "                                - to capture from a directshow device, set dshow\n"
#else
	    "                                - to capture from a webcam, set video4linux2\n"
	    "                                - to capture the screen, set x11grab\n"
	    "    -v4l2f inv4l2f           inv4l2f is the input format for webcam acquisition\n"
	    "                                - it can be mjpeg, yuyv422, etc.\n"
#endif
	    "    -pixf FMT                set the input pixel format\n"
	    "    -vfr N                   force the input video framerate\n"
	    "    -vres WxH                force the video resolution (e.g. 640x480)\n"
	    "    -vcrop XxY               crop the source video from X pixels left and Y pixels top. Must be used with -vres.\n"
		"* Audio options:\n"
	    "    -a string                set the source name for an audio input\n"
	    "                                - if input is from microphone, use \"plughw:[x],[y]\"\n"
	    "                                  where x is the card number and y is the device number\n"
	    "    -af string               set the input audio format\n"
	    "\n"
	    "Output options:\n"
	    "* Video encoding options:\n"
	    "    -vcodec string          set the output video codec (default: h264)\n"
#if 0 //TODO: bind to option and params - test first how it binds to current input parameters
	    "    -vb int                 set the output video bitrate (in bits)\n"
#endif
	    "    -vcustom string         send custom parameters directly to the video encoder\n"
	    "    -gdr                    use Gradual Decoder Refresh feature for video encoding (h264 codec only)\n"
		"    -gop                    specify GOP size in frames - default is framerate (1 sec gop)\n"
		"    -low-delay               specify that low delay settings should be used (no B-frames, fast encoding)\n"
	    "* Audio encoding options:\n"
	    "    -acodec string          set the output audio codec (default: aac)\n"
#if 0 //TODO: bind to option and params - test first how it binds to current input parameters
	    "    -ab int                 set the output audio bitrate in bits (default: 192000)\n"
	    "    -as int                 set the sample rate (default: 44100)\n"
	    "    -ach int                set the number of output audio channels (default: 2)\n"
#endif
	    "    -acustom string         send custom parameters directly to the audio encoder\n"
	    "\n"
	    "DASH options:\n"
	    "    -seg-dur dur:int         set the segment duration in millisecond (default value: 1000)\n"
	    "    -frag-dur dur:int        set the fragment duration in millisecond (default value: 1000)\n"
	    "    -seg-marker marker:4cc   add a marker box named marker at the end of DASH segment\n"
	    "    -out outdir:str          outdir is the output data directory (default: output)\n"
	    "    -mpd mpdname:str         mpdname is the MPD file name (default: dashcast.mpd)\n"
	    "    -ast-offset dur:int      dur is the MPD availabilityStartTime shift in milliseconds (default value: 0)\n"
	    "    -mpd-refresh dur:int     dur is the MPD minimumUpdatePeriod in seconds\n"
	    "    -time-shift dur:int      dur is the MPD TimeShiftBufferDepth in seconds\n"
	    "                                - the default value is 10. Specify -1 to keep all files.\n"
	    "    -min-buffer dur:float    dur is the MPD minBufferTime in seconds (default value: 1.0)\n"
	    "    -base-url baseurl:str    baseurl is the MPD BaseURL\n"
	    "\n"
	    "\n"
	    "Examples:\n"
	    "\n"
	    "    DashCast -av test.avi -live-media\n"
	    "    DashCast -a test_audio.mp3 -v test_audio.mp4 -live-media\n"
#ifdef WIN32
	    "    DashCast -vf vfwcap -vres 1280x720 -vfr 24 -v 0 -live\n"
	    "    DashCast -vf dshow  -vres 1280x720 -vfr 24 -v video=\"screen-capture-recorder\" -live (please install http://screencapturer.sf.net/)\n"
	    "    DashCast -vf dshow  -vres 1280x720 -vfr 24 -v video=\"YOUR-WEBCAM\" -pixf yuv420p -live\n"
#else
	    "    DashCast -vf video4linux2 -vres 1280x720 -vfr 24 -v4l2f mjpeg -v /dev/video0 -af alsa -a plughw:1,0 -live\n"
	    "    DashCast -vf x11grab -vres 800x600 -vfr 25 -v :0.0 -live\n"
#endif
	    "\n";

	const char *command_error = "\33[31mUnknown option or missing mandatory argument.\33[0m\n";

	if (argc == 1) {
		fprintf(stderr, "%s", command_usage);
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
			DASHCAST_CHECK_NEXT_ARG
			if (strcmp(argv[i - 1], "-a") == 0 || strcmp(argv[i - 1], "-av") == 0) {
				if (strcmp(cmd_data->audio_data_conf.filename, "") != 0) {
					fprintf(stderr, "Audio source has already been specified.\n");
					fprintf(stderr, "%s", command_usage);
					return -1;
				}
				strcpy(cmd_data->audio_data_conf.filename, argv[i]);
			}

			if (strcmp(argv[i - 1], "-v") == 0 || strcmp(argv[i - 1], "-av") == 0) {
				if (strcmp(cmd_data->video_data_conf.filename, "") != 0) {
					fprintf(stderr, "Video source has already been specified.\n");
					fprintf(stderr, "%s", command_usage);
					return -1;
				}
				strcpy(cmd_data->video_data_conf.filename, argv[i]);
			}

			i++;
		} else if (strcmp(argv[i], "-af") == 0 || strcmp(argv[i], "-vf") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (strcmp(argv[i - 1], "-af") == 0) {
				if (strcmp(cmd_data->audio_data_conf.format, "") != 0) {
					fprintf(stderr, "Audio format has already been specified.\n");
					fprintf(stderr, "%s", command_usage);
					return -1;
				}
				strcpy(cmd_data->audio_data_conf.format, argv[i]);
			}
			if (strcmp(argv[i - 1], "-vf") == 0) {
				if (strcmp(cmd_data->video_data_conf.format, "") != 0) {
					fprintf(stderr, "Video format has already been specified.\n");
					fprintf(stderr, "%s", command_usage);
					return -1;
				}
				strcpy(cmd_data->video_data_conf.format, argv[i]);
			}
			i++;
		} else if (strcmp(argv[i], "-pixf") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (strcmp(cmd_data->video_data_conf.pixel_format, "") != 0) {
				fprintf(stderr, "Input pixel format has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			strcpy(cmd_data->video_data_conf.pixel_format, argv[i]);
			i++;
		} else if (strcmp(argv[i], "-vfr") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (cmd_data->video_data_conf.framerate != -1) {
				fprintf(stderr, "Video framerate has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			cmd_data->video_data_conf.framerate = atoi(argv[i]);
			i++;
		} else if (strcmp(argv[i], "-vres") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (cmd_data->video_data_conf.height != -1 && cmd_data->video_data_conf.width != -1) {
				fprintf(stderr, "Video resolution has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			dc_str_to_resolution(argv[i], &cmd_data->video_data_conf.width, &cmd_data->video_data_conf.height);
			i++;
		} else if (strcmp(argv[i], "-vcrop") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (cmd_data->video_data_conf.crop_x && cmd_data->video_data_conf.crop_y) {
				fprintf(stderr, "Video crop has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			dc_str_to_resolution(argv[i], &cmd_data->video_data_conf.crop_x, &cmd_data->video_data_conf.crop_y);
			i++;
		} else if (strcmp(argv[i], "-vcodec") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (strcmp(cmd_data->video_data_conf.codec, "") != 0) {
				fprintf(stderr, "Video codec has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			strncpy(cmd_data->video_data_conf.codec, argv[i], GF_MAX_PATH-1);
			i++;
		} else if (strcmp(argv[i], "-vcustom") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (strlen(cmd_data->video_data_conf.custom)) {
				fprintf(stderr, "Video custom has already been specified: appending\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			if (strlen(argv[i]) >= GF_MAX_PATH)
				fprintf(stderr, "Warning: video custom is too long. Truncating.\n");
			strncpy(cmd_data->video_data_conf.custom, argv[i], GF_MAX_PATH-1);
			i++;
		} else if (strcmp(argv[i], "-acodec") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (strcmp(cmd_data->audio_data_conf.codec, "") != 0) {
				fprintf(stderr, "Audio codec has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			strncpy(cmd_data->audio_data_conf.codec, argv[i], GF_MAX_PATH-1);
			i++;
		} else if (strcmp(argv[i], "-acustom") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (strlen(cmd_data->audio_data_conf.custom)) {
				fprintf(stderr, "Audio custom has already been specified: appending\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			if (strlen(argv[i]) >= GF_MAX_PATH)
				fprintf(stderr, "Warning: audio custom is too long. Truncating.\n");
			strncpy(cmd_data->audio_data_conf.custom, argv[i], GF_MAX_PATH-1);
			i++;
		} else if (strcmp(argv[i], "-conf") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			cmd_data->conf = gf_cfg_force_new(NULL, argv[i]);
			i++;
		} else if (strcmp(argv[i], "-switch-source") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			cmd_data->switch_conf = gf_cfg_force_new(NULL, argv[i]);
			i++;
		} else if (strcmp(argv[i], "-out") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			strcpy(cmd_data->out_dir, argv[i]);
			i++;
#ifndef WIN32
		} else if (strcmp(argv[i], "-v4l2f") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			strcpy(cmd_data->video_data_conf.v4l2f, argv[i]);
			i++;
#endif
		} else if (strcmp(argv[i], "-seg-marker") == 0) {
			char *m;
			DASHCAST_CHECK_NEXT_ARG
			m = argv[i];
			if (strlen(m) == 4) {
				cmd_data->seg_marker = GF_4CC(m[0], m[1], m[2], m[3]);
			} else {
				fprintf(stderr, "Invalid marker box name specified: %s\n", m);
				return -1;
			}
			i++;
		} else if (strcmp(argv[i], "-mpd") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (strcmp(cmd_data->mpd_filename, "") != 0) {
				fprintf(stderr, "MPD file has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			strncpy(cmd_data->mpd_filename, argv[i], GF_MAX_PATH-1);
			i++;
		} else if (strcmp(argv[i], "-seg-dur") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (cmd_data->seg_dur != 0) {
				fprintf(stderr, "Segment duration has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			cmd_data->seg_dur = atoi(argv[i]);
			i++;
		} else if (strcmp(argv[i], "-frag-dur") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (cmd_data->frag_dur != 0) {
				fprintf(stderr, "Fragment duration has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			cmd_data->frag_dur = atoi(argv[i]);
			i++;
		} else if (strcmp(argv[i], "-ast-offset") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (cmd_data->ast_offset != -1) {
				fprintf(stderr, "AvailabilityStartTime offset has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			cmd_data->ast_offset = atoi(argv[i]);
			i++;
		} else if (strcmp(argv[i], "-mpd-refresh") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (cmd_data->minimum_update_period != -1) {
				fprintf(stderr, "minimumUpdatePeriod (mpd-refresh) has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			cmd_data->minimum_update_period = atoi(argv[i]);
			i++;
		} else if (strcmp(argv[i], "-time-shift") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (cmd_data->time_shift != 0) {
				fprintf(stderr, "TimeShiftBufferDepth has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			cmd_data->time_shift = atoi(argv[i]);
			i++;
		} else if (strcmp(argv[i], "-gop") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (cmd_data->gop_size != 0) {
				fprintf(stderr, "GOP size has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			cmd_data->gop_size = atoi(argv[i]);
			i++;
		} else if (strcmp(argv[i], "-min-buffer") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (cmd_data->min_buffer_time != -1) {
				fprintf(stderr, "Min Buffer Time has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			cmd_data->min_buffer_time = (float)atof(argv[i]);
			i++;
		} else if (strcmp(argv[i], "-base-url") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (strcmp(cmd_data->base_url, "") != 0) {
				fprintf(stderr, "BaseURL has already been specified.\n");
				fprintf(stderr, "%s", command_usage);
				return -1;
			}
			strncpy(cmd_data->base_url, argv[i], GF_MAX_PATH-1);
			i++;
		} else if (strcmp(argv[i], "-low-delay") == 0) {
			cmd_data->video_data_conf.low_delay = 1;
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
		} else if (strcmp(argv[i], "-insert-utc") == 0) {
			cmd_data->insert_utc = 1;
			i++;
		} else if (strcmp(argv[i], "-dynamic-ast") == 0) {
			cmd_data->use_dynamic_ast = 1;
			i++;
		} else if (strcmp(argv[i], "-send-message") == 0) {
			//FIXME: unreferenced option. Seems related to a separate fragment thread.
			cmd_data->send_message = 1;
			i++;
		} else if (strcmp(argv[i], "-logs") == 0) {
			DASHCAST_CHECK_NEXT_ARG
			if (gf_log_set_tools_levels(argv[i]) != GF_OK) {
				fprintf(stderr, "Invalid log format %s", argv[i]);
				return 1;
			}
			i++;
		} else if (strcmp(argv[i], "-mem-track") == 0) {
			i++;
#ifndef GPAC_MEMORY_TRACKING
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("WARNING - GPAC not compiled with Memory Tracker - ignoring \"-mem-track\"\n"));
#endif
		} else if (!strcmp(argv[i], "-lf") || !strcmp(argv[i], "-log-file")) {
			DASHCAST_CHECK_NEXT_ARG
			cmd_data->logfile = gf_f64_open(argv[i], "wt");
			gf_log_set_callback(cmd_data->logfile, on_dc_log);
			i++;
		} else if (strcmp(argv[i], "-gdr") == 0) {
			if ( (i+1 <= argc) && (argv[i+1][0] != '-')) {
				DASHCAST_CHECK_NEXT_ARG
				cmd_data->gdr = atoi(argv[i]);
			} else {
				//for historical reasons in dashcast
				cmd_data->gdr = 8;
			}
			i++;
		} else {
			fprintf(stderr, "%s: %s", command_error, argv[i]);
			fprintf(stderr, "%s", command_usage);
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
		fprintf(stderr, "Audio/Video source must be specified.\n");
		fprintf(stderr, "%s", command_usage);
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

	//safety checks
	if (cmd_data->video_data_conf.crop_x || cmd_data->video_data_conf.crop_y) {
		if (cmd_data->video_data_conf.width == -1 && cmd_data->video_data_conf.height == -1) {
			fprintf(stderr, "You cannot use '-vcrop' without '-vres'. Please check usage.\n");
			fprintf(stderr, "%s", command_usage);
			return -1;
		}
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
	if (cmd_data->video_data_conf.crop_x != -1 && cmd_data->video_data_conf.crop_y != -1) {
		fprintf(stdout, "    video crop: %dx%d\n", cmd_data->video_data_conf.crop_x, cmd_data->video_data_conf.crop_y);
	}

	fprintf(stdout, "    audio source: %s\n", cmd_data->audio_data_conf.filename);
	if (strcmp(cmd_data->audio_data_conf.format, "") != 0) {
		fprintf(stdout, "    audio format: %s\n", cmd_data->audio_data_conf.format);
	}
	fprintf(stdout, "\33[0m");
	fflush(stdout);

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
