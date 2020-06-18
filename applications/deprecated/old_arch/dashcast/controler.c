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

#include "controler.h"


#if (!defined(__DARWIN__) && !defined(__APPLE__))
# include <malloc.h>
#endif

#include <time.h>

#if defined(__GNUC__)
# include <unistd.h>
# include <sys/time.h>
#elif defined(WIN32)
# include <Winsock.h>
# include <sys/timeb.h>
# define suseconds_t long
#else
# error
#endif

#include <gpac/network.h>

typedef struct {
	int segnum;
	u64 utc_time, ntpts;
} segtime;


//#define MAX_SOURCE_NUMBER 20
#define FRAGMENTER 0
//#define DEBUG 1

//#define VIDEO_MUXER FFMPEG_VIDEO_MUXER
//#define VIDEO_MUXER RAW_VIDEO_H264
//#define VIDEO_MUXER GPAC_VIDEO_MUXER
#define VIDEO_MUXER GPAC_INIT_VIDEO_MUXER_AVC1
//#define VIDEO_MUXER GPAC_INIT_VIDEO_MUXER_AVC3

//#define AUDIO_MUXER FFMPEG_AUDIO_MUXER
//#define AUDIO_MUXER GPAC_AUDIO_MUXER
#define AUDIO_MUXER GPAC_INIT_AUDIO_MUXER

#define AUDIO_FRAME_SIZE 1024


void optimize_seg_frag_dur(int *seg, int *frag)
{
	int min_rem;
	int seg_nb = *seg;
	int frag_nb = *frag;
	if (!frag_nb) frag_nb = 1;

	min_rem = seg_nb % frag_nb;

	if (seg_nb % (frag_nb + 1) < min_rem) {
		min_rem = seg_nb % (frag_nb + 1);
		*seg = seg_nb;
		*frag = frag_nb + 1;
	}

	if ((seg_nb + 1) % frag_nb < min_rem) {
		min_rem = (seg_nb + 1) % frag_nb;
		*seg = seg_nb + 1;
		*frag = frag_nb;
	}

	if ((seg_nb + 1) % (frag_nb + 1) < min_rem) {
		min_rem = (seg_nb + 1) % (frag_nb + 1);
		*seg = seg_nb + 1;
		*frag = frag_nb + 1;
	}

	*seg -= min_rem;
}

Bool change_source_thread(void *params)
{
	Bool ret = GF_FALSE;
	return ret;
}

u32 send_frag_event(void *params)
{
	int ret;
	//int status;
	char buff[GF_MAX_PATH];
	ThreadParam *th_param = (ThreadParam*)params;
	CmdData *cmd_data = th_param->in_data;
	MessageQueue *mq = th_param->mq;

	while (1) {
		if (cmd_data->exit_signal) {
			break;
		}

		ret = dc_message_queue_get(mq, (void*) buff);
		if (ret > 0) {
			fprintf(stdout, "Message received: %s\n", buff);
		}
	}

	return 0;
}

static void dc_write_mpd(CmdData *cmddata, const AudioDataConf *audio_data_conf, const VideoDataConf *video_data_conf, const char *presentation_duration, const char *availability_start_time, const char *time_shift, const int segnum, const int ast_offset)
{
	u32 i = 0, sec;
	int audio_seg_dur = 0, video_seg_dur = 0, audio_frag_dur = 0,	video_frag_dur = 0;
	int audio_frame_size = AUDIO_FRAME_SIZE;
	time_t gtime;
	struct tm *t;
	FILE *f;

	char name[GF_MAX_PATH];

	snprintf(name, sizeof(name), "%s/%s", cmddata->out_dir, cmddata->mpd_filename);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] Write MPD at UTC "LLU" ms - %s : %s\n", gf_net_get_utc(), (cmddata->mode == ON_DEMAND) ? "mediaPresentationDuration" : "availabilityStartTime", (cmddata->mode == ON_DEMAND) ? presentation_duration : availability_start_time));

	if (strcmp(cmddata->audio_data_conf.filename, "") != 0) {
		audio_data_conf = (const AudioDataConf*)gf_list_get(cmddata->audio_lst, 0);
		if (audio_data_conf) {
			audio_seg_dur = (int)((audio_data_conf->samplerate / (double) audio_frame_size) * (cmddata->seg_dur / 1000.0));
			audio_frag_dur = (int)((audio_data_conf->samplerate / (double) audio_frame_size) * (cmddata->frag_dur / 1000.0));
			optimize_seg_frag_dur(&audio_seg_dur, &audio_frag_dur);
		}
	}

	if (strcmp(cmddata->video_data_conf.filename, "") != 0) {
		video_data_conf = (VideoDataConf*)gf_list_get(cmddata->video_lst, 0);
		if (video_data_conf) {
			video_seg_dur = (int)(video_data_conf->framerate * (cmddata->seg_dur / 1000.0));
			video_frag_dur = (int)(video_data_conf->framerate * (cmddata->frag_dur / 1000.0));
			optimize_seg_frag_dur(&video_seg_dur, &video_frag_dur);
		}
	}

	f = gf_fopen(name, "w");
	//TODO: if (!f) ...

	//	time_t t = time(NULL);
	//	time_t t2 = t + 2;
	//	t += (2 * (cmddata->seg_dur / 1000.0));
	//	tm = *gmtime(&t2);
	//	snprintf(availability_start_time, "%d-%d-%dT%d:%d:%dZ", tm.tm_year + 1900,
	//			tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	//	fprintf(stdout, "%s \n", availability_start_time);

	fprintf(f, "<?xml version=\"1.0\"?>\n");
	fprintf(f, "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" profiles=\"urn:mpeg:dash:profile:full:2011\" minBufferTime=\"PT%fS\"", cmddata->min_buffer_time);
	
	if (cmddata->mode == ON_DEMAND) {
		fprintf(f, " type=\"static\" mediaPresentationDuration=\"%s\"", presentation_duration);
	} else {
		fprintf(f, " type=\"dynamic\" availabilityStartTime=\"%s\"", availability_start_time);
		if (time_shift) fprintf(f, " timeShiftBufferDepth=\"%s\"", time_shift);

		if (cmddata->minimum_update_period > 0)
			fprintf(f, " minimumUpdatePeriod=\"PT%dS\"", cmddata->minimum_update_period);

		gf_net_get_ntp(&sec, NULL);
		gtime = sec - GF_NTP_SEC_1900_TO_1970;
		t = gmtime(&gtime);
		fprintf(f, " publishTime=\"%d-%02d-%02dT%02d:%02d:%02dZ\"", 1900+t->tm_year, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

	}

	fprintf(f, ">\n");

	fprintf(f,
	        " <ProgramInformation moreInformationURL=\"http://gpac.io\">\n"
	        "  <Title>%s</Title>\n"
	        " </ProgramInformation>\n", cmddata->mpd_filename);

	if (strcmp(cmddata->base_url, "") != 0) {
		fprintf(f, " <BaseURL>%s</BaseURL>\n", cmddata->base_url);
	}

	fprintf(f, " <Period start=\"PT0H0M0.000S\" id=\"P1\">\n");

	if (strcmp(cmddata->audio_data_conf.filename, "") != 0) {
		fprintf(f, "  <AdaptationSet segmentAlignment=\"true\" bitstreamSwitching=\"false\">\n");

		fprintf(f,
		        "   <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" "
		        "value=\"2\"/>\n");

		fprintf(f,
		        "   <SegmentTemplate timescale=\"%d\" duration=\"%d\" media=\"$RepresentationID$_$Number$_gpac.m4s\""
		        " startNumber=\"%d\" initialization=\"$RepresentationID$_init_gpac.mp4\"",
		        audio_data_conf->samplerate, audio_seg_dur * audio_frame_size, segnum);

		if (ast_offset<0) {
			fprintf(f, " availabilityTimeOffset=\"%g\"", -ast_offset/1000.0);
		}
		fprintf(f, "/>\n");



		for (i = 0; i < gf_list_count(cmddata->audio_lst); i++) {
			audio_data_conf = (const AudioDataConf*)gf_list_get(cmddata->audio_lst, i);
			fprintf(f,
			        "   <Representation id=\"%s\" mimeType=\"audio/mp4\" codecs=\"%s\" "
			        "audioSamplingRate=\"%d\" startWithSAP=\"1\" bandwidth=\"%d\">\n"
			        "   </Representation>\n", audio_data_conf->filename, audio_data_conf->codec6381, audio_data_conf->samplerate, audio_data_conf->bitrate);
		}

		fprintf(f, "  </AdaptationSet>\n");
	}

	if (strcmp(cmddata->video_data_conf.filename, "") != 0) {
		fprintf(f, "  <AdaptationSet segmentAlignment=\"true\" bitstreamSwitching=\"false\">\n");

		fprintf(f,
		        "   <SegmentTemplate timescale=\"%d\" duration=\"%d\" media=\"$RepresentationID$_$Number$_gpac.m4s\""
		        " startNumber=\"%d\" initialization=\"$RepresentationID$_init_gpac.mp4\"",
		        video_data_conf->framerate, video_seg_dur, segnum);


		if (ast_offset<0) {
			fprintf(f, " availabilityTimeOffset=\"%g\"", -ast_offset/1000.0);
		}
		fprintf(f, "/>\n");

		for (i = 0; i < gf_list_count(cmddata->video_lst); i++) {
			video_data_conf = (VideoDataConf*)gf_list_get(cmddata->video_lst, i);
			fprintf(f, "   <Representation id=\"%s\" mimeType=\"video/mp4\" codecs=\"%s\" "
			        "width=\"%d\" height=\"%d\" frameRate=\"%d\" sar=\"1:1\" startWithSAP=\"1\" bandwidth=\"%d\">\n"
			        "   </Representation>\n", video_data_conf->filename,
			        VIDEO_MUXER == GPAC_INIT_VIDEO_MUXER_AVC1 ? video_data_conf->codec6381 : "avc3",
			        video_data_conf->width, video_data_conf->height, video_data_conf->framerate,
			        video_data_conf->bitrate);
		}

		fprintf(f, "  </AdaptationSet>\n");
	}

	fprintf(f, " </Period>\n");

	fprintf(f, "</MPD>\n");

	gf_fclose(f);
}

static u32 mpd_thread(void *params)
{
	ThreadParam *th_param = (ThreadParam*)params;
	CmdData *cmddata = th_param->in_data;
	MessageQueue *mq = th_param->mq;
	char availability_start_time[GF_MAX_PATH];
	char presentation_duration[GF_MAX_PATH];
	char time_shift[GF_MAX_PATH] = "";
	AudioDataConf *audio_data_conf = NULL;
	VideoDataConf *video_data_conf = NULL;
	struct tm ast_time;
	int dur = 0;
	int h, m, s, ms;
	segtime last_seg_time;
	segtime main_seg_time;
	Bool first = GF_TRUE;
	main_seg_time.segnum = 0;
	main_seg_time.utc_time = 0;
	main_seg_time.ntpts = 0;
	last_seg_time = main_seg_time;

	if (cmddata->mode == LIVE_CAMERA || cmddata->mode == LIVE_MEDIA) {
		while (1) {
			u32 msecs;
			time_t t;
			segtime seg_time;
			seg_time.segnum = 0;
			seg_time.utc_time = 0;
			seg_time.ntpts = 0;

			if (cmddata->exit_signal) {
				break;
			}
			
			if (strcmp(cmddata->video_data_conf.filename, "") != 0) {
				if (dc_message_queue_get(mq, &seg_time) < 0) {
					continue;
				}
			}
			
			if (strcmp(cmddata->audio_data_conf.filename, "") != 0) {
				if (dc_message_queue_get(mq, &seg_time) < 0) {
					continue;
				}
			}
			assert(seg_time.ntpts);

			if (cmddata->use_dynamic_ast) {
				main_seg_time = seg_time;
			} else {
				//get the last notification of AST
				if (first) {
					main_seg_time = seg_time;
					first = GF_FALSE;
				}
				assert(main_seg_time.ntpts);
			}

			last_seg_time = seg_time;
			assert(main_seg_time.ntpts <= seg_time.ntpts);
			
			t = (seg_time.ntpts >> 32)  - GF_NTP_SEC_1900_TO_1970;
			msecs = (u32) ( (seg_time.ntpts & 0xFFFFFFFF) * (1000.0/0xFFFFFFFF) );
			ast_time = *gmtime(&t);
			fprintf(stdout, "Generating MPD at %d-%02d-%02dT%02d:%02d:%02d.%03dZ\n", 1900 + ast_time.tm_year, ast_time.tm_mon+1, ast_time.tm_mday, ast_time.tm_hour, ast_time.tm_min, ast_time.tm_sec, msecs);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] Generating MPD at %d-%02d-%02dT%02d:%02d:%02d.%03dZ - UTC "LLU" ms - AST UTC "LLU" ms\n", 1900 + ast_time.tm_year, ast_time.tm_mon+1, ast_time.tm_mday, ast_time.tm_hour, ast_time.tm_min, ast_time.tm_sec, msecs, seg_time.utc_time, main_seg_time.utc_time));

			t = (main_seg_time.ntpts >> 32)  - GF_NTP_SEC_1900_TO_1970;
			if (cmddata->ast_offset>0) {
				t += cmddata->ast_offset/1000;
			}
			msecs = (u32) ( (main_seg_time.ntpts & 0xFFFFFFFF) * (1000.0/0xFFFFFFFF) );
			ast_time = *gmtime(&t);
			assert(ast_time.tm_year);
			
			sprintf(availability_start_time, "%d-%02d-%02dT%02d:%02d:%02d.%03dZ", 1900 + ast_time.tm_year, ast_time.tm_mon+1, ast_time.tm_mday, ast_time.tm_hour, ast_time.tm_min, ast_time.tm_sec, msecs);
			fprintf(stdout, "StartTime: %s - startNumber %d - last number %d\n", availability_start_time, main_seg_time.segnum+1, seg_time.segnum+1);

			if (cmddata->time_shift != -1) {
				int ts, h, m, s;
				ts = cmddata->time_shift;
				h = ts / 3600;
				ts = ts % 3600;
				m = ts / 60;
				s = ts % 60;
				snprintf(time_shift, sizeof(time_shift), "PT%02dH%02dM%02dS", h, m, s);
			}

			dc_write_mpd(cmddata, audio_data_conf, video_data_conf, presentation_duration, availability_start_time, time_shift, main_seg_time.segnum+1, cmddata->ast_offset);
		}
		
		if (cmddata->no_mpd_rewrite) return 0;

		//finally rewrite the MPD to static
		dur = cmddata->seg_dur * (last_seg_time.segnum - main_seg_time.segnum);
		if (cmddata->time_shift) {
			if (dur > cmddata->time_shift * 1000) {
				u32 nb_seg = cmddata->time_shift*1000 / cmddata->seg_dur;
				main_seg_time.segnum = last_seg_time.segnum - nb_seg;
				//dur = cmddata->time_shift;
			}
			dur = cmddata->seg_dur * (last_seg_time.segnum - main_seg_time.segnum);
		}
		cmddata->mode = ON_DEMAND;
		
	} else {
		int a_dur = 0;
		int v_dur = 0;

		if (strcmp(cmddata->audio_data_conf.filename, "") != 0) {
			dc_message_queue_get(mq, &a_dur);
		}

		if (strcmp(cmddata->video_data_conf.filename, "") != 0) {
			dc_message_queue_get(mq, &v_dur);
		}
		
		dur =  v_dur > a_dur ? v_dur : a_dur;
	}
	
	
	h = dur / 3600000;
	dur = dur % 3600000;
	m = dur / 60000;
	dur = dur % 60000;
	s = dur / 1000;
	ms = dur % 1000;
	snprintf(presentation_duration, sizeof(presentation_duration), "PT%02dH%02dM%02d.%03dS", h, m, s, ms);
	fprintf(stdout, "Duration: %s\n", presentation_duration);
		

	dc_write_mpd(cmddata, audio_data_conf, video_data_conf, presentation_duration, availability_start_time, 0, main_seg_time.segnum+1, 0);

	return 0;
}

u32 delete_seg_thread(void *params)
{
	int ret;
	ThreadParam *th_param = (ThreadParam*)params;
	CmdData *cmd_data = th_param->in_data;
	MessageQueue *mq = th_param->mq;

	char buff[GF_MAX_PATH];

	while (1) {
		ret = dc_message_queue_get(mq, (void*) buff);
		if (ret > 0) {
			int status;
			status = unlink(buff);
			if (status != 0) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("Unable to delete the file %s\n", buff));
			}
		}

		if (cmd_data->exit_signal) {
			break;
		}
	}

	return 0;
}

Bool fragmenter_thread(void *params)
{
//	int ret;
	ThreadParam *th_param = (ThreadParam*)params;
	CmdData *cmd_data = th_param->in_data;
	MessageQueue *mq = th_param->mq;

	char buff[GF_MAX_PATH];

	while (1) {
		/*ret = */dc_message_queue_get(mq, (void*) buff);
		if (cmd_data->exit_signal) {
			break;
		}
	}

	return GF_FALSE;
}

static u32 keyboard_thread(void *params)
{

	ThreadParam *th_param = (ThreadParam*)params;
	CmdData *in_data = th_param->in_data;
	char c;

	while (1) {
		if (gf_prompt_has_input()) {
			c = gf_prompt_get_char();
			if (c == 'q' || c == 'Q') {
				in_data->exit_signal = 1;
				break;
			}
		}

		if (in_data->exit_signal) {
			break;
		}

		gf_sleep(100);
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Keyboard thread exit\n"));
	return 0;
}

u32 video_decoder_thread(void *params)
{
#ifdef DASHCAST_PRINT
	int i = 0;
#endif
	int ret;
	int source_number = 0;

	//int first_time = 1;
	struct timeval time_start, time_end, time_wait;
	VideoThreadParam *thread_params = (VideoThreadParam *) params;

	CmdData *in_data = thread_params->in_data;
	VideoInputData *video_input_data = thread_params->video_input_data;
	VideoInputFile **video_input_file = thread_params->video_input_file;

	suseconds_t total_wait_time = (int) (1000000.0 / (double) in_data->video_data_conf.framerate);
	suseconds_t pick_packet_delay, select_delay = 0, real_wait, other_delays = 2;

	Task t;
	//fprintf(stdout, "wait time : %f\n", total_wait_time);

	if (!gf_list_count(in_data->video_lst))
		return 0;

	while (1) {
		dc_task_get_current(&in_data->task_list, &t);
		source_number = t.source_number;

		//fprintf(stdout, "sourcenumber: %d\n", source_number);

//		if (video_input_file[source_number]->mode == LIVE_MEDIA) {
			gf_gettimeofday(&time_start, NULL);
//		}

		ret = dc_video_decoder_read(video_input_file[source_number], video_input_data, source_number, in_data->use_source_timing, (in_data->mode == LIVE_CAMERA) ? 1 : 0, (const int *) &in_data->exit_signal);
#ifdef DASHCAST_PRINT
		fprintf(stdout, "Read video frame %d\r", i++);
		fflush(stdout);
#endif
		if (ret == -2) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("Video reader has no more frame to read.\n"));
			break;
		}
		if (ret == -1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("An error occurred while reading video frame.\n"));
			break;
		}

		if (in_data->exit_signal) {
			dc_video_input_data_end_signal(video_input_data);
			break;
		}

		if (video_input_file[source_number]->mode == LIVE_MEDIA) {
			gf_gettimeofday(&time_end, NULL);
			pick_packet_delay = ((time_end.tv_sec - time_start.tv_sec) * 1000000) + time_end.tv_usec - time_start.tv_usec;
			time_wait.tv_sec = 0;
			real_wait = total_wait_time - pick_packet_delay - select_delay - other_delays;
			time_wait.tv_usec = real_wait;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("delay: %ld = %ld - %ld\n", time_wait.tv_usec, total_wait_time, pick_packet_delay));

			gf_gettimeofday(&time_start, NULL);
			select(0, NULL, NULL, NULL, &time_wait);
			gf_gettimeofday(&time_end, NULL);

			select_delay = (((time_end.tv_sec - time_start.tv_sec) * 1000000) + time_end.tv_usec - time_start.tv_usec) - real_wait;
		}
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("Video decoder is exiting...\n"));
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("video decoder thread exit\n"));
	return 0;
}

u32 audio_decoder_thread(void *params)
{
	int ret;
	struct timeval time_start, time_end, time_wait;
	AudioThreadParam *thread_params = (AudioThreadParam*)params;

	CmdData *in_data = thread_params->in_data;
	AudioInputData *audio_input_data = thread_params->audio_input_data;
	AudioInputFile *audio_input_file = thread_params->audio_input_file;

	suseconds_t pick_packet_delay, select_delay = 0, real_wait, other_delays = 1;
	suseconds_t total_wait_time;
	if (in_data->audio_data_conf.samplerate < 1024) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error: invalid audio sample rate: %d\n", in_data->audio_data_conf.samplerate));
		dc_audio_inout_data_end_signal(audio_input_data);
		//FIXME: deadlock on the mpd thread. Reproduce with big_buck_bunny.mp4.
		return 1;
	}
	total_wait_time = (int) (1000000.0 / (in_data->audio_data_conf.samplerate / (double) AUDIO_FRAME_SIZE));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("wait time : %ld\n", total_wait_time));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("sample-rate : %ld\n", in_data->audio_data_conf.samplerate));

	if (!gf_list_count(in_data->audio_lst))
		return 0;

	while (1) {
//		if (audio_input_file->mode == LIVE_MEDIA) {
			gf_gettimeofday(&time_start, NULL);
//		}

		ret = dc_audio_decoder_read(audio_input_file, audio_input_data);
		if (ret == -2) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("Audio decoder has no more frame to read.\n"));
			break;
		}
		if (ret == -1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("An error occurred while reading audio frame.\n"));
			break;
		}

		if (in_data->exit_signal) {
			dc_audio_inout_data_end_signal(audio_input_data);
			break;
		}

		if (audio_input_file->mode == LIVE_MEDIA) {
			gf_gettimeofday(&time_end, NULL);
			pick_packet_delay = ((time_end.tv_sec - time_start.tv_sec) * 1000000) + time_end.tv_usec - time_start.tv_usec;
			time_wait.tv_sec = 0;
			real_wait = total_wait_time - pick_packet_delay - select_delay - other_delays;
			time_wait.tv_usec = real_wait;

			gf_gettimeofday(&time_start, NULL);
			select(0, NULL, NULL, NULL, &time_wait);
			gf_gettimeofday(&time_end, NULL);

			select_delay = (((time_end.tv_sec - time_start.tv_sec) * 1000000) + time_end.tv_usec - time_start.tv_usec) - real_wait;
		}
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("Audio decoder is exiting...\n"));
	return 0;
}

u32 video_scaler_thread(void *params)
{
	int ret;
	VideoThreadParam *thread_params = (VideoThreadParam *) params;
	CmdData *in_data = thread_params->in_data;
	VideoInputData *video_input_data = thread_params->video_input_data;
	VideoScaledData *video_scaled_data = thread_params->video_scaled_data;

	if (!gf_list_count(in_data->video_lst))
		return 0;

	while (1) {
		ret = dc_video_scaler_scale(video_input_data, video_scaled_data);
		if (ret == -2) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("Video scaler has no more frame to read.\n"));
			break;
		}
	}

	dc_video_scaler_end_signal(video_scaled_data);

	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("video scaler thread exit\n"));
	return 0;
}

u32 video_encoder_thread(void *params)
{
	int ret, shift, frame_nb, seg_frame_max, frag_frame_max, seg_nb = 0, loss_state = 0, quit = 0, real_video_seg_dur;
	char name_to_delete[GF_MAX_PATH], name_to_send[GF_MAX_PATH];
	u64 start_utc, seg_utc;
	segtime time_at_segment_start;
	VideoMuxerType muxer_type = VIDEO_MUXER;
	VideoThreadParam *thread_params = (VideoThreadParam*)params;
	u32 sec, frac;
	Bool init_mpd = GF_FALSE;
	CmdData *in_data = thread_params->in_data;
	int video_conf_idx = thread_params->video_conf_idx;
	VideoDataConf *video_data_conf = (VideoDataConf*)gf_list_get(in_data->video_lst, video_conf_idx);

	VideoScaledData *video_scaled_data = thread_params->video_scaled_data;
	int video_cb_size = (in_data->mode == LIVE_MEDIA || in_data->mode == LIVE_CAMERA) ? 1 : VIDEO_CB_DEFAULT_SIZE;
	VideoOutputFile out_file;

	MessageQueue *mq = thread_params->mq;
	MessageQueue *delete_seg_mq = thread_params->delete_seg_mq;
	MessageQueue *send_seg_mq = thread_params->send_seg_mq;

	if (!gf_list_count(in_data->video_lst))
		return 0;

	seg_frame_max = (int)(video_data_conf->framerate * (float) (in_data->seg_dur / 1000.0));
	frag_frame_max = (int)(video_data_conf->framerate * (float) (in_data->frag_dur / 1000.0));
	optimize_seg_frag_dur(&seg_frame_max, &frag_frame_max);

	real_video_seg_dur = (int) (seg_frame_max * 1000.0 / (float) video_data_conf->framerate);
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[video_encoder] seg_frame_max=%d, frag_frame_max=%d, real_video_seg_dur=%d ms\n", seg_frame_max, frag_frame_max, real_video_seg_dur));


	if (seg_frame_max <= 0)
		seg_frame_max = -1;

	if (dc_video_muxer_init(&out_file, video_data_conf, muxer_type, seg_frame_max, frag_frame_max, in_data->seg_marker, in_data->gdr, in_data->seg_dur, in_data->frag_dur, (u32) video_scaled_data->vsprop->video_input_data->frame_duration, in_data->gop_size, video_cb_size) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot init output video file.\n"));
		in_data->exit_signal = 1;
		return -1;
	}

	if (dc_video_encoder_open(&out_file, video_data_conf, in_data->use_source_timing, video_scaled_data->sar) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open output video stream.\n"));
		in_data->exit_signal = 1;
		return -1;
	}

	if (in_data->mode == LIVE_MEDIA || in_data->mode == LIVE_CAMERA) {
		init_mpd = GF_TRUE;
	}


	time_at_segment_start.ntpts = 0;
	start_utc = gf_net_get_utc();

	while (1) {
		frame_nb = 0;
		//log time at segment start, because segment availabilityStartTime is computed from AST anchor + segment duration
		//logging at the end of the segment production will induce one segment delay
		time_at_segment_start.segnum = seg_nb;
		time_at_segment_start.utc_time = gf_net_get_utc();
		gf_net_get_ntp(&sec, &frac);

#ifndef GPAC_DISABLE_LOG
		if (gf_log_tool_level_on(GF_LOG_DASH, GF_LOG_INFO)) {
			if (time_at_segment_start.ntpts) {
				u32 ref_sec, ref_frac;
				Double tr, t;
				ref_sec = (time_at_segment_start.ntpts>>32) & 0xFFFFFFFFULL;
				ref_frac = (u32) (time_at_segment_start.ntpts & 0xFFFFFFFFULL);
				tr = ref_sec * 1000.0;
				tr += ref_frac*1000.0 /0xFFFFFFFF;
				t = sec * 1000.0;
				t += frac*1000.0 /0xFFFFFFFF;
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DashCast] NTP diff since last segment start in msec is %f\n", t - tr));

			}
		}
#endif

		time_at_segment_start.ntpts = sec;
		time_at_segment_start.ntpts <<= 32;
		time_at_segment_start.ntpts |= frac;

		//force writing MPD before any encoding happens (eg don't wait for the end of the first segment)
		if (init_mpd) {
			init_mpd = GF_FALSE;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] Initial MPD publish at UTC "LLU" ms\n", time_at_segment_start.utc_time));
			dc_message_queue_put(mq, &time_at_segment_start, sizeof(time_at_segment_start));
		}

		assert(! out_file.segment_started);

		if (dc_video_muxer_open(&out_file, in_data->out_dir, video_data_conf->filename, seg_nb+1) < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open output video file.\n"));
			in_data->exit_signal = 1;
			return -1;
		}
//		fprintf(stdout, "Header size: %d\n", ret);
		while (1) {
			//we have the RAP already encoded, skip coder
			if (loss_state == 2) {
				ret = 1;
				loss_state = 0;
			} else {
				ret = dc_video_encoder_encode(&out_file, video_scaled_data);
			}

			if (ret == -2) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("Video encoder has no more data to encode.\n"));
				quit = 1;
				break;
			}
			if (ret == -1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("An error occured while writing video frame.\n"));
				quit = 1;
				break;
			}

			if (ret > 0) {
				int r;

				/*resync at first RAP: flush current broken segment and restart next one on rap*/
				if ((loss_state==1) && out_file.codec_ctx->coded_frame->key_frame) {
					loss_state = 2;
					break;
				}

				r = dc_video_muxer_write(&out_file, frame_nb, in_data->insert_utc ? GF_TRUE : GF_FALSE);
				if (r < 0) {
					quit = 1;
					in_data->exit_signal = 1;
					break;
				} else if (r == 1) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("fragment is written!\n"));
					if (in_data->send_message == 1) {
						snprintf(name_to_send, sizeof(name_to_send), "%s/%s_%d_gpac.m4s", in_data->out_dir, video_data_conf->filename, seg_nb);
						dc_message_queue_put(send_seg_mq, name_to_send, sizeof(name_to_send));
					}

					break;
				}

				frame_nb++;
			}
		}

		dc_video_muxer_close(&out_file);

		// If system is live,
		// Send the time that a segment is available to MPD generator thread.
		if (in_data->mode == LIVE_MEDIA || in_data->mode == LIVE_CAMERA) {
			//check we don't loose sync
			int diff;
			int seg_diff;
			seg_utc = gf_net_get_utc();
			diff = (int) (seg_utc - start_utc);

			//if seg UTC is after next segment UTC (current ends at seg_nb+1, next at seg_nb+2), adjust numbers
			if (diff > (seg_nb+2) * real_video_seg_dur) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[video_encoder] Rep %s UTC diff %d bigger than segment duration %d - some frame where probably lost. Adjusting\n", out_file.rep_id, diff, seg_nb));

				while (diff > (seg_nb+2) * real_video_seg_dur) {
					seg_nb++;

					//do a rough estimate of losses to adjust timing...
					if (! in_data->use_source_timing) {
						out_file.first_dts_in_fragment += out_file.codec_ctx->time_base.den;
					}
				}
				//wait for RAP to cut next segment
				loss_state = 1;
			} else {
#define SYNC_SAFE	800

				seg_diff = diff;
				seg_diff -= (seg_nb+1) * real_video_seg_dur;
				if (seg_diff > SYNC_SAFE) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[video_encoder] Rep %s UTC diff at segment close: %d is higher than cumulated segment duration %d (diff %d) - frame rate is probably not correct!\n", out_file.rep_id, diff, (seg_nb+1) * in_data->seg_dur, seg_diff));
				}
				else if (seg_diff < -SYNC_SAFE) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[video_encoder] Rep %s UTC diff at segment close: %d is lower than cumulated segment duration %d (diff %d) - frame rate is probably not correct or frames were lost!\n", out_file.rep_id, diff, (seg_nb+1) * in_data->seg_dur, seg_diff));
				} else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[video_encoder] Rep %s UTC diff at segment close: %d - cumulated segment duration %d (diff %d)\n", out_file.rep_id, diff, (seg_nb+1) * in_data->seg_dur, seg_diff));
				}
			}

			//time_t t = time(NULL);
			dc_message_queue_put(mq, &time_at_segment_start, sizeof(time_at_segment_start));
		}

		if ((in_data->time_shift != -1)) {
			shift = 1000 * in_data->time_shift / in_data->seg_dur;
			if (seg_nb > shift) {
				snprintf(name_to_delete, sizeof(name_to_delete), "%s/%s_%d_gpac.m4s", in_data->out_dir, video_data_conf->filename, (seg_nb - shift));
				dc_message_queue_put(delete_seg_mq, name_to_delete, sizeof(name_to_delete));
			}
		}

		seg_nb++;

		if (quit)
			break;
	}

	// If system is not live,
	// Send the duration of the video
	if (in_data->mode == ON_DEMAND) {
		if (thread_params->video_conf_idx == 0) {
			int dur = (seg_nb * seg_frame_max * 1000) / video_data_conf->framerate;
			int dur_tot = (out_file.codec_ctx->frame_number * 1000)
			              / video_data_conf->framerate;
			if (dur > dur_tot)
				dur = dur_tot;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("Duration: %d \n", dur));
			dc_message_queue_put(mq, &dur, sizeof(dur));
		}
	}

	/* Close output video file */
	dc_video_encoder_close(&out_file);

	dc_video_muxer_free(&out_file);

	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("video encoder thread exit\n"));
	return 0;
}

u32 audio_encoder_thread(void *params)
{
	int ret, exit_loop = 0, quit = 0, seg_nb = 0, frame_per_seg, frame_per_frag, frame_nb, shift, real_audio_seg_dur;
	//int seg_frame_max;
	//int frag_frame_max;
	//int audio_frame_size = AUDIO_FRAME_SIZE;
	char name_to_delete[GF_MAX_PATH];
	u64 start_utc, seg_utc;
	segtime time_at_segment_start;
	Bool check_first_seg_utc = GF_TRUE;

	AudioMuxerType muxer_type = AUDIO_MUXER;
	AudioThreadParam *thread_params = (AudioThreadParam *) params;

	CmdData *in_data = thread_params->in_data;
	int audio_conf_idx = thread_params->audio_conf_idx;
	AudioDataConf *audio_data_conf = (AudioDataConf*)gf_list_get(in_data->audio_lst, audio_conf_idx);

	AudioInputData *audio_input_data = thread_params->audio_input_data;
	AudioOutputFile audio_output_file;

	MessageQueue *mq = thread_params->mq;
	MessageQueue *delete_seg_mq = thread_params->delete_seg_mq;

	if (!gf_list_count(in_data->audio_lst))
		return 0;

	//seg_frame_max = audio_data_conf->samplerate
	//		* (float) (in_data->seg_dur / 1000.0);

	//frag_frame_max = audio_data_conf->samplerate * (float) (in_data->frag_dur / 1000.0);
	//if (seg_frame_max <= 0)
	//	seg_frame_max = -1;

	if (dc_audio_encoder_open(&audio_output_file, audio_data_conf) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open output audio stream.\n"));
		in_data->exit_signal = 1;
		return -1;
	}

	frame_per_seg  = (int)((audio_data_conf->samplerate / (double) audio_output_file.codec_ctx->frame_size) * (in_data->seg_dur  / 1000.0));
	frame_per_frag = (int)((audio_data_conf->samplerate / (double) audio_output_file.codec_ctx->frame_size) * (in_data->frag_dur / 1000.0));
	optimize_seg_frag_dur(&frame_per_seg, &frame_per_frag);

	real_audio_seg_dur = (int) (frame_per_seg * (double) audio_output_file.codec_ctx->frame_size * 1000.0 / (double) audio_data_conf->samplerate);
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH,("[audio_encoder] frame_per_seg=%d, frame_per_frag=%d, real_audio_seg_dur=%d ms\n", frame_per_seg, frame_per_frag, real_audio_seg_dur) );

	if (dc_audio_muxer_init(&audio_output_file, audio_data_conf, muxer_type, frame_per_seg, frame_per_frag, in_data->seg_marker) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot init output audio.\n"));
		in_data->exit_signal = 1;
		return -1;
	}

	start_utc = gf_net_get_utc();
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[audio_encoder] start_utc="LLU"\n", start_utc));

	while (1) {
		//logging at the end of the segment production will induce one segment delay
		time_at_segment_start.utc_time = gf_net_get_utc();
		time_at_segment_start.ntpts = gf_net_get_ntp_ts();

		frame_nb = 0;
		quit = 0;
		
		if (dc_audio_muxer_open(&audio_output_file, in_data->out_dir, audio_data_conf->filename, seg_nb+1) < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open output audio.\n"));
			in_data->exit_signal = 1;
			return -1;
		}

		while (1) {
			exit_loop = 0;
//			if (frame_per_seg > 0) {
//				if (frame_nb == frame_per_seg) {
//
//					//if (dc_audio_encoder_flush(&audio_output_file, audio_input_data) == 0) {
//					//	dc_audio_muxer_write(&audio_output_file);
//					//	frame_nb++;//= audio_output_file.codec_ctx->frame_size; //audio_output_file.acc_samples;
//					//}
//
//					exit_loop = 1;
//					break;
//				}
//			}

			audio_output_file.frame_ntp = gf_net_get_ntp_ts();
			ret = dc_audio_encoder_read(&audio_output_file, audio_input_data);
			if (ret == -2) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("Audio encoder has no more data to encode.\n"));
				//if (dc_audio_encoder_flush(&audio_output_file, audio_input_data) == 0) {
				//	dc_audio_muxer_write(&audio_output_file);
				//	frame_nb++;//= audio_output_file.codec_ctx->frame_size; //audio_output_file.acc_samples;
				//}
				quit = 1;
				break;
			}

			while (1) {
				ret = dc_audio_encoder_encode(&audio_output_file, audio_input_data);
				if (ret == 1) {
					break;
				}
				if (ret == -1) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("An error occured while encoding audio frame.\n"));
					quit = 1;
					break;
				}

				ret = dc_audio_muxer_write(&audio_output_file, frame_nb, in_data->insert_utc);
				if (ret == -1) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("An error occured while writing audio frame.\n"));
					quit = 1;
					break;
				}

				if (ret == 1) {
					exit_loop = 1;
					break;
				}

				frame_nb++; //= audio_output_file.codec_ctx->frame_size; //audio_output_file.acc_samples;
			}

			if (exit_loop || quit)
				break;
		}

		dc_audio_muxer_close(&audio_output_file);

		// Send the time that a segment is available to MPD generator thread.
		if (in_data->mode == LIVE_CAMERA || in_data->mode == LIVE_MEDIA) {
			int diff;
			if (check_first_seg_utc) {
				u64 now = gf_net_get_utc();
				check_first_seg_utc = GF_FALSE;
				
				diff = (int) (now - time_at_segment_start.utc_time);
				if (diff  < real_audio_seg_dur / 2) {
					u32 sec, frac, ms;
					s32 left, nb_s;
					gf_net_get_ntp(&sec, &frac);
					nb_s = real_audio_seg_dur/1000;
					sec -= nb_s;
					ms = (u32) ((u64) 1000 * frac / 0xFFFFFFFF);
					left = ms;
					left -= (s32) (real_audio_seg_dur - 1000*nb_s);
					while (left<0) {
						left += 1000;
						sec-=1;
					}
					time_at_segment_start.ntpts = sec;
					time_at_segment_start.ntpts <<= 32;
					time_at_segment_start.ntpts |= (u32) ((0xFFFFFFFF*left)/1000);
					
					start_utc = time_at_segment_start.utc_time = now - real_audio_seg_dur;
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[audio_encoder] First segment produced faster (%d ms) than duration (%d ms) probably due to HW buffers - adjusting ast\n", diff, real_audio_seg_dur));
				}
			}
			if (thread_params->audio_conf_idx == 0) {

				//check we don't loose sync
				seg_utc = gf_net_get_utc();
				diff = (int) (seg_utc - start_utc);
				//if seg UTC is after next segment UTC (current ends at seg_nb+1, next at seg_nb+2), adjust numbers
				if (diff > (seg_nb+2) * real_audio_seg_dur) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[audio_encoder] UTC dur %d bigger than segment duration %d - some frame where probably lost. Adjusting\n", diff, seg_nb));
					while (diff > (seg_nb+2) * real_audio_seg_dur) {
						seg_nb++;
					}
				}
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[audio_encoder] UTC dur %d - cumulated segment duration %d (diff %d ms)\n", diff, (seg_nb+1) * real_audio_seg_dur, diff - (seg_nb+1) * real_audio_seg_dur));

				
				time_at_segment_start.segnum = seg_nb;
				dc_message_queue_put(mq, &time_at_segment_start, sizeof(time_at_segment_start));
			}
		}

		if (in_data->time_shift != -1) {
			shift = 1000 * in_data->time_shift / in_data->seg_dur;
			if (seg_nb > shift) {
				snprintf(name_to_delete, sizeof(name_to_delete), "%s/%s_%d_gpac.m4s", in_data->out_dir, audio_data_conf->filename, (seg_nb - shift));
				dc_message_queue_put(delete_seg_mq, name_to_delete, sizeof(name_to_delete));
			}
		}

		seg_nb++;

		if (quit)
			break;
	}

	// If system is not live,
	// Send the duration of the video
	if (in_data->mode == ON_DEMAND) {
		if (thread_params->audio_conf_idx == 0) {
			int dur = (seg_nb * audio_output_file.codec_ctx->frame_size * frame_per_seg * 1000) / audio_data_conf->samplerate;
			int dur_tot = (audio_output_file.codec_ctx->frame_number * audio_output_file.codec_ctx->frame_size * 1000) / audio_data_conf->samplerate;
			if (dur > dur_tot)
				dur = dur_tot;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("Duration: %d \n", dur));
			dc_message_queue_put(mq, &dur, sizeof(dur));
		}
	}

	dc_audio_muxer_free(&audio_output_file);

	/* Close output audio file */
	dc_audio_encoder_close(&audio_output_file);

	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("Audio encoder is exiting...\n"));
	return 0;
}

int dc_run_controler(CmdData *in_data)
{
	int ret = 0;
	u32 video_cb_size = VIDEO_CB_DEFAULT_SIZE;
	u32 i, j;

	ThreadParam keyboard_th_params;
	ThreadParam mpd_th_params;
	ThreadParam delete_seg_th_params;
	ThreadParam send_frag_th_params;

	//Video parameters
	VideoThreadParam vdecoder_th_params;
	VideoThreadParam *vencoder_th_params = (VideoThreadParam*)alloca(gf_list_count(in_data->video_lst) * sizeof(VideoThreadParam));
	VideoInputData video_input_data;
	VideoInputFile *video_input_file[MAX_SOURCE_NUMBER];
	VideoScaledDataList video_scaled_data_list;
	VideoThreadParam *vscaler_th_params = NULL;

	//Audio parameters
	AudioThreadParam adecoder_th_params;
	AudioThreadParam *aencoder_th_params = (AudioThreadParam*)alloca(gf_list_count(in_data->audio_lst) * sizeof(AudioThreadParam));
	AudioInputData audio_input_data;
	AudioInputFile audio_input_file;

	MessageQueue mq;
	MessageQueue delete_seg_mq;
	MessageQueue send_frag_mq;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DashCast] Controler init at UTC "LLU"\n", gf_net_get_utc() ));
	dc_register_libav();

	for (i = 0; i < MAX_SOURCE_NUMBER; i++)
		video_input_file[i] = (VideoInputFile*)gf_malloc(sizeof(VideoInputFile));

	dc_message_queue_init(&mq);
	dc_message_queue_init(&delete_seg_mq);
	dc_message_queue_init(&send_frag_mq);

	memset(&audio_input_data, 0, sizeof(AudioInputData));
	memset(&audio_input_file, 0, sizeof(AudioInputFile));
	memset(&video_input_data, 0, sizeof(VideoInputData));


	if (in_data->mode == LIVE_CAMERA || in_data->mode == LIVE_MEDIA)
		video_cb_size  = 1;

	if (strcmp(in_data->video_data_conf.filename, "") != 0) {
		dc_video_scaler_list_init(&video_scaled_data_list, in_data->video_lst);
		vscaler_th_params = (VideoThreadParam*)gf_malloc(video_scaled_data_list.size * sizeof(VideoThreadParam));

		/* Open input video */
		if (dc_video_decoder_open(video_input_file[0], &in_data->video_data_conf, in_data->mode, in_data->no_loop, video_scaled_data_list.size) < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open input video.\n"));
			ret = -1;
			goto exit;
		}

		if (dc_video_input_data_init(&video_input_data, /*video_input_file[0]->width, video_input_file[0]->height,
		  video_input_file[0]->pix_fmt,*/video_scaled_data_list.size, in_data->mode, MAX_SOURCE_NUMBER, video_cb_size) < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot initialize audio data.\n"));
			ret = -1;
			goto exit;
		}

		/* open other input videos for source switching */
		for (i = 0; i < gf_list_count(in_data->vsrc); i++) {
			VideoDataConf *video_data_conf = (VideoDataConf*)gf_list_get(in_data->vsrc, i);
			if (dc_video_decoder_open(video_input_file[i + 1], video_data_conf, LIVE_MEDIA, 1, video_scaled_data_list.size) < 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open input video.\n"));
				ret = -1;
				goto exit;
			}
		}

		for (i=0; i<gf_list_count(in_data->vsrc) + 1; i++) {
			dc_video_input_data_set_prop(&video_input_data, i, video_input_file[i]->width, video_input_file[i]->height, in_data->video_data_conf.crop_x, in_data->video_data_conf.crop_y, video_input_file[i]->pix_fmt, video_input_file[i]->sar);
		}

		for (i=0; i<video_scaled_data_list.size; i++) {
			dc_video_scaler_data_init(&video_input_data, video_scaled_data_list.video_scaled_data[i], MAX_SOURCE_NUMBER, video_cb_size);

			for (j=0; j<gf_list_count(in_data->vsrc) + 1; j++) {
				dc_video_scaler_data_set_prop(&video_input_data, video_scaled_data_list.video_scaled_data[i], j);
			}
		}

		/* Initialize video decoder thread */
		vdecoder_th_params.thread = gf_th_new("video_decoder_thread");

		for (i=0; i<video_scaled_data_list.size; i++) {
			vscaler_th_params[i].thread = gf_th_new("video_scaler_thread");
		}

		/* Initialize video encoder threads */
		for (i=0; i<gf_list_count(in_data->video_lst); i++)
			vencoder_th_params[i].thread = gf_th_new("video_encoder_thread");
	}

	/* When video and audio share the same source, open it once. This allow to read from unicast streams */
	if (!strcmp(in_data->video_data_conf.filename, in_data->audio_data_conf.filename)) {
		audio_input_file.av_fmt_ctx = video_input_file[0]->av_fmt_ctx;
		video_input_file[0]->av_fmt_ctx_ref_cnt++;
		audio_input_file.av_pkt_list = video_input_file[0]->av_pkt_list = gf_list_new();
		audio_input_file.av_pkt_list_mutex = video_input_file[0]->av_pkt_list_mutex = gf_mx_new("Demux AVPackets List");
	}

	if (strcmp(in_data->audio_data_conf.filename, "") != 0) {
		/* Open input audio */
		if (dc_audio_decoder_open(&audio_input_file, &in_data->audio_data_conf, in_data->mode, in_data->no_loop, in_data->video_data_conf.framerate) < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot open input audio.\n"));
			ret = -1;
			goto exit;
		}

		if (dc_audio_input_data_init(&audio_input_data, in_data->audio_data_conf.channels, in_data->audio_data_conf.samplerate, gf_list_count(in_data->audio_lst), in_data->mode) < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot initialize audio data.\n"));
			ret = -1;
			goto exit;
		}

		/* Initialize audio decoder thread */
		adecoder_th_params.thread = gf_th_new("audio_decoder_thread");

		/* Initialize audio encoder threads */
		for (i = 0; i < gf_list_count(in_data->audio_lst); i++)
			aencoder_th_params[i].thread = gf_th_new("video_encoder_thread");
	}

	/******** Keyboard controler Thread ********/

	/* Initialize keyboard controller thread */
	keyboard_th_params.thread = gf_th_new("keyboard_thread");

	/* Create keyboard controller thread */
	keyboard_th_params.in_data = in_data;
	if (gf_th_run(keyboard_th_params.thread, keyboard_thread, (void *)&keyboard_th_params) != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error while doing pthread_create for keyboard_thread.\n"));
	}

	/********************************************/

	//Communication between decoder and audio encoder
	for (i = 0; i < gf_list_count(in_data->audio_lst); i++) {
		AudioDataConf *tmadata = (AudioDataConf*)gf_list_get(in_data->audio_lst, i);
		tmadata->channels = in_data->audio_data_conf.channels;
		tmadata->samplerate = in_data->audio_data_conf.samplerate;
	}

	//Communication between decoder and video encoder
	for (i = 0; i < gf_list_count(in_data->video_lst); i++) {
		VideoDataConf *tmvdata = (VideoDataConf*)gf_list_get(in_data->video_lst, i);
		tmvdata->framerate = in_data->video_data_conf.framerate;
		if (in_data->use_source_timing) {
			tmvdata->time_base = in_data->video_data_conf.time_base;
		}
	}

	/******** MPD Thread ********/

	/* Initialize MPD generator thread */
	mpd_th_params.thread = gf_th_new("mpd_thread");

	/* Create MPD generator thread */
	mpd_th_params.in_data = in_data;
	mpd_th_params.mq = &mq;
	if (gf_th_run(mpd_th_params.thread, mpd_thread, (void *)&mpd_th_params) != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error while doing pthread_create for mpd_thread.\n"));
	}


	if (strcmp(in_data->video_data_conf.filename, "") != 0) {
		/* Create video decoder thread */
		vdecoder_th_params.in_data = in_data;
		vdecoder_th_params.video_input_data = &video_input_data;
		vdecoder_th_params.video_input_file = video_input_file;
		if (gf_th_run(vdecoder_th_params.thread, video_decoder_thread, (void *) &vdecoder_th_params) != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error while doing pthread_create for video_decoder_thread.\n"));
		}

		while ((in_data->mode == LIVE_CAMERA) && !video_input_data.frame_duration) {
			gf_sleep(0);
		}
	}

	if (strcmp(in_data->audio_data_conf.filename, "") != 0) {
		/* Create audio decoder thread */
		adecoder_th_params.in_data = in_data;
		adecoder_th_params.audio_input_data = &audio_input_data;
		adecoder_th_params.audio_input_file = &audio_input_file;
		if (gf_th_run(adecoder_th_params.thread, audio_decoder_thread, (void *) &adecoder_th_params) != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error while doing pthread_create for audio_decoder_thread.\n"));
		}
	}

	/****************************/

	if (strcmp(in_data->video_data_conf.filename, "") != 0) {
		/* Create video encoder threads */
		for (i=0; i<gf_list_count(in_data->video_lst); i++) {
			VideoDataConf * video_data_conf = (VideoDataConf*)gf_list_get(in_data->video_lst, i);

			vencoder_th_params[i].in_data = in_data;
			vencoder_th_params[i].video_conf_idx = i;
			vencoder_th_params[i].video_scaled_data = dc_video_scaler_get_data(&video_scaled_data_list, video_data_conf->width, video_data_conf->height);

			vencoder_th_params[i].mq = &mq;
			vencoder_th_params[i].delete_seg_mq = &delete_seg_mq;
			vencoder_th_params[i].send_seg_mq = &send_frag_mq;

			if (gf_th_run(vencoder_th_params[i].thread, video_encoder_thread, (void*)&vencoder_th_params[i]) != GF_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error while doing pthread_create for video_encoder_thread.\n"));
			}
		}

		/* Create video scaler threads */
		for (i=0; i<video_scaled_data_list.size; i++) {
			vscaler_th_params[i].in_data = in_data;
			vscaler_th_params[i].video_scaled_data = video_scaled_data_list.video_scaled_data[i];
			vscaler_th_params[i].video_input_data = &video_input_data;

			if (gf_th_run(vscaler_th_params[i].thread, video_scaler_thread, (void*)&vscaler_th_params[i]) != GF_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error while doing pthread_create for video_scaler_thread.\n"));
			}
		}
	}

	if (strcmp(in_data->audio_data_conf.filename, "") != 0) {
		/* Create audio encoder threads */
		for (i = 0; i < gf_list_count(in_data->audio_lst); i++) {
			aencoder_th_params[i].in_data = in_data;
			aencoder_th_params[i].audio_conf_idx = i;
			aencoder_th_params[i].audio_input_data = &audio_input_data;

			aencoder_th_params[i].mq = &mq;
			aencoder_th_params[i].delete_seg_mq = &delete_seg_mq;
			aencoder_th_params[i].send_seg_mq = &send_frag_mq;

			if (gf_th_run(aencoder_th_params[i].thread, audio_encoder_thread, (void *) &aencoder_th_params[i]) != GF_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error while doing pthread_create for audio_encoder_thread.\n"));
			}
		}
	}

	if (in_data->time_shift != -1) {
		/* Initialize delete segment thread */
		delete_seg_th_params.thread = gf_th_new("delete_seg_thread");
		delete_seg_th_params.in_data = in_data;
		delete_seg_th_params.mq = &delete_seg_mq;
		if (gf_th_run(delete_seg_th_params.thread, delete_seg_thread, (void *) &delete_seg_th_params) != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error while doing pthread_create for delete_seg_thread.\n"));
		}
	}

	if (in_data->send_message == 1) {
		/* Initialize delete segment thread */
		send_frag_th_params.thread = gf_th_new("send_frag_event_thread");
		send_frag_th_params.in_data = in_data;
		send_frag_th_params.mq = &send_frag_mq;
		if (gf_th_run(send_frag_th_params.thread, send_frag_event, (void *) &send_frag_th_params) != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error while doing pthread_create for send_frag_event_thread.\n"));
		}
	}

	fprintf(stdout, "Press q or Q to exit...\n");

	if (strcmp(in_data->audio_data_conf.filename, "") != 0) {
		/* Wait for and destroy audio decoder threads */
		gf_th_stop(adecoder_th_params.thread);
		gf_th_del(adecoder_th_params.thread);
	}

	if (strcmp(in_data->video_data_conf.filename, "") != 0) {
		/* Wait for and destroy video decoder threads */
		gf_th_stop(vdecoder_th_params.thread);
		gf_th_del(vdecoder_th_params.thread);
	}

	if (strcmp(in_data->audio_data_conf.filename, "") != 0) {
		/* Wait for and destroy audio encoder threads */
		for (i = 0; i < gf_list_count(in_data->audio_lst); i++) {
			gf_th_stop(aencoder_th_params[i].thread);
			gf_th_del(aencoder_th_params[i].thread);
		}
	}

	if (strcmp(in_data->video_data_conf.filename, "") != 0) {
		/* Wait for and destroy video encoder threads */
		for (i=0; i<gf_list_count(in_data->video_lst); i++) {
			gf_th_stop(vencoder_th_params[i].thread);
			gf_th_del(vencoder_th_params[i].thread);
		}

		/* Wait for and destroy video scaler threads */
		for (i=0; i<video_scaled_data_list.size; i++) {
			gf_th_stop(vscaler_th_params[i].thread);
			gf_th_del(vscaler_th_params[i].thread);
		}
	}
	
	keyboard_th_params.in_data->exit_signal = 1;

	/********** Keyboard thread ***********/

	/* Wait for and destroy keyboard controler thread */
	gf_th_stop(keyboard_th_params.thread);
	gf_th_del(keyboard_th_params.thread);

	/**************************************/

	/********** MPD generator thread ***********/

	/* Wait for and destroy MPD generator thread */
	gf_th_stop(mpd_th_params.thread);
	gf_th_del(mpd_th_params.thread);

	/**************************************/

	if (in_data->time_shift != -1) {
		//	dc_message_queue_flush(&delete_seg_mq);
		/* Wait for and destroy delete segment thread */
		gf_th_stop(delete_seg_th_params.thread);
		gf_th_del(delete_seg_th_params.thread);
	}

	if (in_data->send_message == 1) {
		/* Wait for and destroy delete segment thread */
		gf_th_stop(send_frag_th_params.thread);
		gf_th_del(send_frag_th_params.thread);
	}

exit:
	if (strcmp(in_data->audio_data_conf.filename, "") != 0) {
		/* Destroy audio input data */
		dc_audio_input_data_destroy(&audio_input_data);
		/* Close input audio */
		dc_audio_decoder_close(&audio_input_file);
	}

	if (strcmp(in_data->video_data_conf.filename, "") != 0) {
		/* Destroy video input data */
		dc_video_input_data_destroy(&video_input_data);

		for (i = 0; i < gf_list_count(in_data->vsrc); i++) {
			/* Close input video */
			dc_video_decoder_close(video_input_file[i]);
		}

		for (i=0; i<video_scaled_data_list.size; i++) {
			dc_video_scaler_data_destroy(video_scaled_data_list.video_scaled_data[i]);
		}

		/* Destroy video scaled data */
		dc_video_scaler_list_destroy(&video_scaled_data_list);
	}

	if (vscaler_th_params)
		gf_free(vscaler_th_params);

	for (i = 0; i < MAX_SOURCE_NUMBER; i++)
		gf_free(video_input_file[i]);

	dc_message_queue_free(&mq);
	dc_message_queue_free(&delete_seg_mq);
	dc_message_queue_free(&send_frag_mq);

	dc_unregister_libav();

	return ret;
}
