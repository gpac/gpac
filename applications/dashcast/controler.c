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
#include <malloc.h>
#endif

#include <time.h>
//FIXME: use GPAC utility functions
#if defined(__GNUC__)
#include <unistd.h>
#include <sys/time.h>
#elif defined(WIN32)
#include <Winsock.h>
#include <sys/timeb.h>
#define suseconds_t long

s32 gettimeofday(struct timeval *tp, void *tz)
{
	struct _timeb timebuffer;

	_ftime( &timebuffer );
	tp->tv_sec  = (long) (timebuffer.time);
	tp->tv_usec = timebuffer.millitm * 1000;
	return 0;
}
#else
#error
#endif


typedef struct {
	int segnum;
	u64 time;
} segtime;


//#define MAX_SOURCE_NUMBER 20
#define DASHER 0
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

#define DASHCAST_PRINT

#define AUDIO_FRAME_SIZE 1024

void optimize_seg_frag_dur(int * seg, int * frag) {

	int seg_nb = *seg;
	int frag_nb = *frag;

	int min_rem = seg_nb % frag_nb;

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

Bool change_source_thread(void * p_params) {

	int ret = 0;

	return ret;
}

u32 send_frag_event(void * p_params) {

	int ret;
	//int status;
	ThreadParam * p_th_param = (ThreadParam *) p_params;
	CmdData * p_cmdd = p_th_param->p_in_data;
	MessageQueue * p_mq = p_th_param->p_mq;

	char buff[512];

	while (1) {

		if (p_cmdd->i_exit_signal) {
			break;
		}

		ret = dc_message_queue_get(p_mq, (void*) buff);

		if (ret > 0) {
			printf("Message received: %s\n", buff);
		}

		if (p_cmdd->i_exit_signal) {
			break;
		}

	}

	return 0;

}

static void dc_write_mpd(CmdData *p_cmddata, const AudioData *p_adata, const VideoData *p_vdata, const char *presentation_duration, const char *availability_start_time, const char *time_shift, const int segnum) {
	u32 i = 0;
	int audio_seg_dur = 0, video_seg_dur = 0, audio_frag_dur = 0,	video_frag_dur = 0;
	int audio_frame_size = AUDIO_FRAME_SIZE;
	FILE * p_f;
	char psz_name[512];
	
	sprintf(psz_name, "%s/%s", p_cmddata->psz_out, p_cmddata->psz_mpd);

	if (strcmp(p_cmddata->adata.psz_name, "") != 0) {
		p_adata = gf_list_get(p_cmddata->p_audio_lst, 0);

		audio_seg_dur = (int)((p_adata->i_samplerate / (double) audio_frame_size) * (p_cmddata->i_seg_dur / 1000.0));
		audio_frag_dur = (int)((p_adata->i_samplerate / (double) audio_frame_size) * (p_cmddata->i_frag_dur / 1000.0));
		optimize_seg_frag_dur(&audio_seg_dur, &audio_frag_dur);
	}

	if (strcmp(p_cmddata->vdata.psz_name, "") != 0) {
		p_vdata = gf_list_get(p_cmddata->p_video_lst, 0);

		video_seg_dur = (int)(p_vdata->i_framerate * (p_cmddata->i_seg_dur / 1000.0));
		video_frag_dur = (int)(p_vdata->i_framerate * (p_cmddata->i_frag_dur / 1000.0));
		optimize_seg_frag_dur(&video_seg_dur, &video_frag_dur);
	}
	
	p_f = fopen(psz_name, "w");

	//	time_t t = time(NULL);
	//	time_t t2 = t + 2;
	//	t += (2 * (p_cmddata->i_seg_dur / 1000.0));
	//	tm = *gmtime(&t2);
	//	sprintf(availability_start_time, "%d-%d-%dT%d:%d:%dZ", tm.tm_year + 1900,
	//			tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	//	printf("%s \n", availability_start_time);

	fprintf(p_f, "<?xml version=\"1.0\"?>\n");
	fprintf(p_f, "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" "
							"%s=\"%s\" "
							"minBufferTime=\"PT%fS\" %s type=\"%s\" "
							"profiles=\"urn:mpeg:dash:profile:full:2011\">\n",
							(p_cmddata->i_mode == ON_DEMAND) ? "mediaPresentationDuration" : "availabilityStartTime",
							(p_cmddata->i_mode == ON_DEMAND) ? presentation_duration : availability_start_time,
							p_cmddata->f_minbuftime, time_shift,
							(p_cmddata->i_mode == ON_DEMAND) ? "static" : "dynamic");

	fprintf(p_f,
		" <ProgramInformation moreInformationURL=\"http://gpac.sourceforge.net\">\n"
		"  <Title>%s</Title>\n"
		" </ProgramInformation>\n", p_cmddata->psz_mpd);

	fprintf(p_f, " <Period id=\"\">\n");

	if (strcmp(p_cmddata->adata.psz_name, "") != 0) {
		fprintf(p_f,
			"  <AdaptationSet segmentAlignment=\"true\" bitstreamSwitching=\"false\">\n");

		fprintf(p_f,
			"   <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" "
			"value=\"2\"/>\n");

		fprintf(p_f,
			"   <SegmentTemplate timescale=\"%d\" duration=\"%d\" media=\"$RepresentationID$_$Number$_gpac.m4s\""
			" startNumber=\"%d\" initialization=\"$RepresentationID$_init_gpac.mp4\"/>\n",
			p_adata->i_samplerate, audio_seg_dur * audio_frame_size, segnum);

		for (i = 0; i < gf_list_count(p_cmddata->p_audio_lst); i++) {

			p_adata = gf_list_get(p_cmddata->p_audio_lst, i);
			fprintf(p_f,
				"   <Representation id=\"%s\" mimeType=\"audio/mp4\" codecs=\"mp4a.40.2\" "
				"audioSamplingRate=\"%d\" startWithSAP=\"1\" bandwidth=\"%d\">\n"
				"   </Representation>\n", p_adata->psz_name,
				p_adata->i_samplerate, p_adata->i_bitrate);

		}

		fprintf(p_f, "  </AdaptationSet>\n");
	}

	if (strcmp(p_cmddata->vdata.psz_name, "") != 0) {
		fprintf(p_f,
			"  <AdaptationSet segmentAlignment=\"true\" bitstreamSwitching=\"false\">\n");

		fprintf(p_f,
			"   <SegmentTemplate timescale=\"%d\" duration=\"%d\" media=\"$RepresentationID$_$Number$_gpac.m4s\""
			" startNumber=\"%d\" initialization=\"$RepresentationID$_init_gpac.mp4\"/>\n",
			p_vdata->i_framerate, video_seg_dur, segnum);

		for (i = 0; i < gf_list_count(p_cmddata->p_video_lst); i++) {
			p_vdata = gf_list_get(p_cmddata->p_video_lst, i);
			fprintf(p_f,
				"   <Representation id=\"%s\" mimeType=\"video/mp4\" codecs=\"%s\" "
				"width=\"%d\" height=\"%d\" frameRate=\"%d\" sar=\"1:1\" startWithSAP=\"1\" bandwidth=\"%d\">\n"
				"   </Representation>\n", p_vdata->psz_name,
				VIDEO_MUXER == GPAC_INIT_VIDEO_MUXER_AVC1 ? "avc1.42e01e" : "avc3", //FIXME: hardcoded. We would need acces to the ISOFile to call gf_media_get_rfc_6381_codec_name()
				p_vdata->i_width, p_vdata->i_height, p_vdata->i_framerate,
				p_vdata->i_bitrate);
		}

		fprintf(p_f, "  </AdaptationSet>\n");
	}

	fprintf(p_f, " </Period>\n");

	fprintf(p_f, "</MPD>\n");

	fclose(p_f);

	printf("\33[34m\33[1m");
	printf("MPD file generated: %s/%s\n", p_cmddata->psz_out, p_cmddata->psz_mpd);
	printf("\33[0m");
}

static u32 mpd_thread(void * p_params) {
	ThreadParam * p_th_param = (ThreadParam *) p_params;
	CmdData * p_cmddata = p_th_param->p_in_data;
	MessageQueue * p_mq = p_th_param->p_mq;
	char availability_start_time[512];
	char presentation_duration[512];
	char time_shift[512] = "";

	AudioData * p_adata = NULL;
	VideoData * p_vdata = NULL;

	if (p_cmddata->i_mode == LIVE_CAMERA || p_cmddata->i_mode == LIVE_MEDIA) {

		while (1) {
			time_t t;
			int ms;
			segtime seg_time;
			seg_time.segnum = 0;
			seg_time.time = 0;

			if (p_cmddata->i_exit_signal) {
				break;
			}

			if (strcmp(p_cmddata->adata.psz_name, "") != 0) {
				dc_message_queue_get(p_mq, &seg_time);
			}

			if (strcmp(p_cmddata->vdata.psz_name, "") != 0) {
				dc_message_queue_get(p_mq, &seg_time);
			}

			seg_time.time += p_cmddata->i_ast_offset;
			seg_time.segnum -= p_cmddata->i_ast_offset / p_cmddata->i_seg_dur;
			if (seg_time.segnum < 0) {
				continue;
			}

			t = seg_time.time / 1000;
			ms = seg_time.time % 1000;

			//t += (1 * (p_cmddata->i_seg_dur / 1000.0));
			//t += p_cmddata->i_ast_offset;
			{	
				struct tm ast_time = *gmtime(&t);
				strftime(availability_start_time, 64, "%Y-%m-%dT%H:%M:%S", &ast_time);
				sprintf(availability_start_time,"%s.%dZ", availability_start_time, ms);
				//sprintf(availability_start_time, "%d-%02d-%02dT%02d:%02d:%02dZ",
				//		time.tm_year + 1900, time.tm_mon + 1, time.tm_mday, time.tm_hour,
				//		time.tm_min, time.tm_sec);
				printf("StartTime: %s\n", availability_start_time);
			}

			if (p_cmddata->i_time_shift != -1) {
				int ts, h, m, s;
				ts = p_cmddata->i_time_shift;
				h = ts / 3600;
				ts = ts % 3600;
				m = ts / 60;
				s = ts % 60;
				sprintf(time_shift, "timeShiftBufferDepth=\"PT%02dH%02dM%02dS\"", h, m, s);
			}

			dc_write_mpd(p_cmddata, p_adata, p_vdata, presentation_duration, availability_start_time, time_shift, seg_time.segnum);
		}
	} else {

		int a_dur = 0;
		int v_dur = 0;

		if (strcmp(p_cmddata->adata.psz_name, "") != 0) {
			dc_message_queue_get(p_mq, &a_dur);
		}

		if (strcmp(p_cmddata->vdata.psz_name, "") != 0) {
			dc_message_queue_get(p_mq, &v_dur);
		}

		{
			int dur, h, m, s, ms;
			dur = v_dur > a_dur ? v_dur : a_dur;
			h = dur / 3600000;
			dur = dur % 3600000;
			m = dur / 60000;
			dur = dur % 60000;
			s = dur / 1000;
			ms = dur % 1000;
			sprintf(presentation_duration, "PT%02dH%02dM%02d.%03dS", h, m, s, ms);
			printf("Duration: %s\n", presentation_duration);
		}

		dc_write_mpd(p_cmddata, p_adata, p_vdata, presentation_duration, availability_start_time, time_shift, 0);
	}

	return 0;
}

u32 delete_seg_thread(void * p_params) {

	int ret;
	//int status;
	ThreadParam * p_th_param = (ThreadParam *) p_params;
	CmdData * p_cmdd = p_th_param->p_in_data;
	MessageQueue * p_mq = p_th_param->p_mq;

	char buff[512];

	while (1) {

		ret = dc_message_queue_get(p_mq, (void*) buff);

		if (ret > 0) {
			//printf("Message received: %s\n", buff);
			unlink(buff);

			//if (status != 0) {
			//	printf("Unable to delete the file %s\n", buff);
			//}
		}

		if (p_cmdd->i_exit_signal) {
			break;
		}

	}

	return 0;
}

Bool fragmenter_thread(void * p_params) {

	int ret;
	ThreadParam * p_th_param = (ThreadParam *) p_params;
	CmdData * p_cmdd = p_th_param->p_in_data;
	MessageQueue * p_mq = p_th_param->p_mq;

	char buff[256];

	while (1) {

		ret = dc_message_queue_get(p_mq, (void*) buff);

		if (ret > 0) {
			printf("Message received: %s\n", buff);
		}

		if (p_cmdd->i_exit_signal) {
			break;
		}

	}

	return 0;
}

Bool dasher_thread(void * p_params) {

//	int i;
//	ThreadParam * p_th_param = (ThreadParam *) p_params;
//	CmdData * p_cmdd = p_th_param->p_in_data;
//
//	char sz_mpd[GF_MAX_PATH];
//	GF_DashSegmenterInput * dash_inputs;
//	u32 nb_dash_inputs = 0;
//	Bool use_url_template = 0;
//	GF_Err e;
//	s32 subsegs_per_sidx = 0;
//	Bool daisy_chain_sidx = 0;
//	char *seg_ext = NULL;
//	const char *dash_title = NULL;
//	const char *dash_source = NULL;
//	const char *dash_more_info = NULL;
//	char *tmpdir = NULL, *cprt = NULL, *seg_name = NULL;
//	char **mpd_base_urls = NULL;
//	u32 nb_mpd_base_urls = 0;
//	Bool single_segment = 0;
//
//	Bool single_file = 0;
//	GF_DashSwitchingMode bitstream_switching_mode = GF_DASH_BSMODE_INBAND;
//	Bool seg_at_rap = 0;
//	Bool frag_at_rap = 0;
//
//	Double interleaving_time = 0.0;
//	u32 time_shift_depth = 0;
//	Double dash_duration = 0.0, dash_subduration = 0.0;
//	u32 mpd_update_time = 0;
//
//
//	GF_DashProfile dash_profile = GF_DASH_PROFILE_UNKNOWN;
//	u32 dash_dynamic = 0;
//	GF_Config *dash_ctx = NULL;
//
//	int video_list_size = gf_list_count(p_cmdd->p_video_lst);
//	int audio_list_size = gf_list_count(p_cmdd->p_audio_lst);
//	nb_dash_inputs = video_list_size + audio_list_size;
//
//	dash_inputs = gf_malloc(nb_dash_inputs * sizeof(GF_DashSegmenterInput));
//
//	for (i = 0; i < video_list_size; i++) {
//
//		VideoData * p_vdata = gf_list_get(p_cmdd->p_video_lst, i);
//		dash_inputs[i].file_name = p_vdata->psz_name;
//		sprintf(dash_inputs[i].representationID, "%d", i);
//		strcpy(dash_inputs[i].periodID, "");
//		strcpy(dash_inputs[i].role, "");
//
//	}
//
//	for (i = 0; i < audio_list_size; i++) {
//
//		AudioData * p_adata = gf_list_get(p_cmdd->p_audio_lst, i);
//		dash_inputs[i + video_list_size].file_name = p_adata->psz_name;
//		sprintf(dash_inputs[i + video_list_size].representationID, "%d",
//				i + video_list_size);
//		strcpy(dash_inputs[i + video_list_size].periodID, "");
//		strcpy(dash_inputs[i + video_list_size].role, "");
//
//	}
//
//	dash_profile = p_cmdd->i_live ? GF_DASH_PROFILE_LIVE : GF_DASH_PROFILE_MAIN;
//	strncpy(sz_mpd, p_cmdd->psz_mpd, GF_MAX_PATH);
//
//	dash_duration = p_cmdd->i_dash_dur ? p_cmdd->i_dash_dur / 1000 : 1;
//
//	if (p_cmdd->i_live) {
//		dash_ctx = gf_cfg_new(NULL, NULL);
//	}
//
//	if (!dash_dynamic) {
//		time_shift_depth = 0;
//		mpd_update_time = 0;
//	} else if ((dash_profile >= GF_DASH_PROFILE_MAIN) && !use_url_template
//			&& !mpd_update_time) {
//		/*use a default MPD update of dash_duration sec*/
//		mpd_update_time = (u32) (
//				dash_subduration ? dash_subduration : dash_duration);
//		fprintf(stderr, "Using default MPD refresh of %d seconds\n",
//				mpd_update_time);
//	}
//
//	if (p_cmdd->i_live)
//		gf_sleep(dash_duration * 1000);
//
//	while (1) {
//
//		//TODO Signiture of this API has changed!
//		/*
//		e = gf_dasher_segment_files(sz_mpd, dash_inputs, nb_dash_inputs,
//				dash_profile, dash_title, dash_source, cprt, dash_more_info,
//				(const char **) mpd_base_urls, nb_mpd_base_urls,
//				use_url_template, single_segment, single_file,
//				bitstream_switching_mode, seg_at_rap, dash_duration, seg_name,
//				seg_ext, interleaving_time, subsegs_per_sidx, daisy_chain_sidx,
//				frag_at_rap, tmpdir, dash_ctx, dash_dynamic, mpd_update_time,
//				time_shift_depth, dash_subduration);
//
//		if (e) {
//			printf("Error while segmenting.\n");
//			break;
//		}
//		*/
//
//		if (!p_cmdd->i_live)
//			break;
//
//		u32 sleep_for = gf_dasher_next_update_time(dash_ctx, mpd_update_time);
//
//		if (p_cmdd->i_exit_signal) {
//			break;
//		}
//
//		if (sleep_for) {
//			if (dash_dynamic != 2) {
//				fprintf(stderr, "sleep for %d ms\n", sleep_for);
//				gf_sleep(sleep_for);
//			}
//		}
//
//	}

	return 0;
}

static u32 keyboard_thread(void * p_params) {

	ThreadParam * p_th_param = (ThreadParam *) p_params;
	CmdData * p_in_data = p_th_param->p_in_data;
	char c;

	while (1) {
		if (gf_prompt_has_input()) {
			c = gf_prompt_get_char();
			if (c == 'q' || c == 'Q') {
				p_in_data->i_exit_signal = 1;
				break;
			}
		}

		if (p_in_data->i_exit_signal) {
			break;
		}

		gf_sleep(100);
	}
fprintf(stderr, "Keyboard thread exit\n");
	return 0;
}

u32 video_decoder_thread(void * p_params) {
#ifdef DASHCAST_PRINT
	int i = 0;
#endif

	int ret;
	int source_number = 0;

	//int first_time = 1;
	struct timeval time_start, time_end, time_wait;
	VideoThreadParam * p_thread_params = (VideoThreadParam *) p_params;

	CmdData * p_in_data = p_thread_params->p_in_data;
	VideoInputData * p_vind = p_thread_params->p_vind;
	VideoInputFile ** p_vinf = p_thread_params->p_vinf;

	suseconds_t total_wait_time = 1000000 / p_in_data->vdata.i_framerate;
	suseconds_t pick_packet_delay, select_delay = 0, real_wait, other_delays = 2;

	Task t;
	//printf("wait time : %f\n", total_wait_time);

	if (!gf_list_count(p_in_data->p_video_lst))
		return 0;

	while (1) {

		dc_task_get_current(&p_in_data->task_list, &t);
		source_number = t.source_number;

		//printf("sourcenumber: %d\n", source_number);

		if (p_vinf[source_number]->i_mode == LIVE_MEDIA) {
			gettimeofday(&time_start, NULL);
		}

		ret = dc_video_decoder_read(p_vinf[source_number], p_vind, source_number);

#ifdef DASHCAST_PRINT
		printf("Read video frame %d\r", i++);
		fflush(stdout);
#endif

		if (ret == -2) {

#ifdef DEBUG
			printf("Video reader has no more frame to read.\n");
#endif
			break;
		}
		if (ret == -1) {
			fprintf(stderr, "An error occurred while reading video frame.\n");
			break;
		}

		if (p_in_data->i_exit_signal) {
			dc_video_input_data_end_signal(p_vind);
			break;
		}

		if (p_vinf[source_number]->i_mode == LIVE_MEDIA) {

			gettimeofday(&time_end, NULL);

			pick_packet_delay =
					((time_end.tv_sec - time_start.tv_sec) * 1000000)
							+ time_end.tv_usec - time_start.tv_usec;

			time_wait.tv_sec = 0;
			real_wait = total_wait_time - pick_packet_delay - select_delay
					- other_delays;
			time_wait.tv_usec = real_wait;
			//printf("delay: %ld = %ld - %ld\n", time_wait.tv_usec,
			//				total_wait_time, pick_packet_delay);

			gettimeofday(&time_start, NULL);

			select(0, NULL, NULL, NULL, &time_wait);

			gettimeofday(&time_end, NULL);

			select_delay = (((time_end.tv_sec - time_start.tv_sec) * 1000000)
					+ time_end.tv_usec - time_start.tv_usec) - real_wait;

		}

	}

#ifdef DEBUG
	printf("Video decoder is exiting...\n");
#endif
	fprintf(stderr, "video decoder thread exit\n");
	return 0;
}

u32 audio_decoder_thread(void * p_params) {
#ifdef DASHCAST_PRINT
	int i = 0;
#endif

	int ret;
	struct timeval time_start, time_end, time_wait;
	AudioThreadParam * p_thread_params = (AudioThreadParam *) p_params;

	CmdData * p_in_data = p_thread_params->p_in_data;
	AudioInputData * p_aind = p_thread_params->p_aind;
	AudioInputFile * p_ainf = p_thread_params->p_ainf;

	suseconds_t total_wait_time = 1000000
			/ (p_in_data->adata.i_samplerate / 1024);
	suseconds_t pick_packet_delay, select_delay = 0, real_wait,
			other_delays = 1;
	;

	//printf("wait time : %ld\n", total_wait_time);
	//printf("sample-rate : %ld\n", p_in_data->adata.i_samplerate);

	if (!gf_list_count(p_in_data->p_audio_lst))
		return 0;

	while (1) {

		if (p_ainf->i_mode == LIVE_MEDIA) {
			gettimeofday(&time_start, NULL);
		}

		ret = dc_audio_decoder_read(p_ainf, p_aind);

#ifdef DASHCAST_PRINT
		printf("Read audio frame %d\r", i++);
		fflush(stdout);
#endif

		if (ret == -2) {
#ifdef DEBUG
			printf("Audio decoder has no more frame to read.\n");
#endif
			break;
		}
		if (ret == -1) {
			fprintf(stderr, "An error occurred while reading video frame.\n");
			break;
		}

		if (p_in_data->i_exit_signal) {
			dc_audio_inout_data_end_signal(p_aind);
			break;
		}

		if (p_ainf->i_mode == LIVE_MEDIA) {

			gettimeofday(&time_end, NULL);

			pick_packet_delay =
					((time_end.tv_sec - time_start.tv_sec) * 1000000)
							+ time_end.tv_usec - time_start.tv_usec;

			time_wait.tv_sec = 0;
			real_wait = total_wait_time - pick_packet_delay - select_delay
					- other_delays;
			time_wait.tv_usec = real_wait;

			gettimeofday(&time_start, NULL);

			select(0, NULL, NULL, NULL, &time_wait);

			gettimeofday(&time_end, NULL);

			select_delay = (((time_end.tv_sec - time_start.tv_sec) * 1000000)
					+ time_end.tv_usec - time_start.tv_usec) - real_wait;

		}

	}

#ifdef DEBUG
	printf("Audio decoder is exiting...\n");
#endif
	return 0;
}

u32 video_scaler_thread(void * p_params) {

	int ret;

	VideoThreadParam * p_thread_params = (VideoThreadParam *) p_params;

	CmdData * p_in_data = p_thread_params->p_in_data;
	VideoInputData * p_vind = p_thread_params->p_vind;
	VideoScaledData * p_vsd = p_thread_params->p_vsd;

	if (!gf_list_count(p_in_data->p_video_lst))
		return 0;

	while (1) {

		ret = dc_video_scaler_scale(p_vind, p_vsd);

		if (ret == -2) {
#ifdef DEBUG
			printf("Video scaler has no more frame to read.\n");
#endif
			break;
		}
	}

	dc_video_scaler_end_signal(p_vsd);

#ifdef DEBUG
	printf("Video scaler is exiting...\n");
#endif
	fprintf(stderr, "video scaler thread exit\n");
	return 0;
}

u32 video_encoder_thread(void * p_params) {

	int ret;
	int frame_nb;
	int seg_frame_max;
	int frag_frame_max;
	int seg_nb = 0;
	int quit = 0;
	char name_to_delete[512];
	char name_to_send[512];
	int shift;

	VideoMuxerType muxer_type = VIDEO_MUXER;

	VideoThreadParam * p_thread_params = (VideoThreadParam *) p_params;

	CmdData * p_in_data = p_thread_params->p_in_data;
	int video_conf_idx = p_thread_params->video_conf_idx;
	VideoData * p_vdata = gf_list_get(p_in_data->p_video_lst, video_conf_idx);

	VideoScaledData * p_vsd = p_thread_params->p_vsd;
	VideoOutputFile out_file;

	MessageQueue * p_mq = p_thread_params->p_mq;
	MessageQueue * p_delete_seg_mq = p_thread_params->p_delete_seg_mq;
	MessageQueue * p_send_seg_mq = p_thread_params->p_send_seg_mq;

#ifndef FRAGMENTER
	MessageQueue * p_mq = p_thread_params->p_mq;
#endif

	if (!gf_list_count(p_in_data->p_video_lst))
		return 0;

	seg_frame_max = (int)(p_vdata->i_framerate * (float) (p_in_data->i_seg_dur / 1000.0));
	frag_frame_max = (int)(p_vdata->i_framerate * (float) (p_in_data->i_frag_dur / 1000.0));

	optimize_seg_frag_dur(&seg_frame_max, &frag_frame_max);

	out_file.i_gop_size = seg_frame_max;

	if (seg_frame_max <= 0)
		seg_frame_max = -1;

	if (dc_video_muxer_init(&out_file, p_vdata, muxer_type, seg_frame_max,
			frag_frame_max, p_in_data->i_seg_marker, p_in_data->i_gdr) < 0) {
		fprintf(stderr, "Cannot init output video file.\n");
		p_in_data->i_exit_signal = 1;
		return -1;
	}

	if (dc_video_encoder_open(&out_file, p_vdata) < 0) {
		fprintf(stderr, "Cannot open output video stream.\n");
		p_in_data->i_exit_signal = 1;
		return -1;
	}

	while (1) {

		frame_nb = 0;

		if (dc_video_muxer_open(&out_file, p_in_data->psz_out,
				p_vdata->psz_name, seg_nb) < 0) {
			fprintf(stderr, "Cannot open output video file.\n");
			p_in_data->i_exit_signal = 1;
			return -1;
		}

//		printf("Header size: %d\n", ret);

		while (1) {

//			if (seg_frame_max > 0) {
//				if (frame_nb == seg_frame_max)
//					break;
//			}

			ret = dc_video_encoder_encode(&out_file, p_vsd);

			if (ret == -2) {
#ifdef DEBUG
				printf("Audio encoder has no more data to encode.\n");
#endif
				quit = 1;
				break;
			}
			if (ret == -1) {
				fprintf(stderr,
						"An error occured while writing video frame.\n");
				quit = 1;
				break;
			}

			if (ret > 0) {

				int r = dc_video_muxer_write(&out_file, frame_nb);

				if (r == 1) {
					//printf("fragment is written!\n");
					if (p_in_data->i_send_message == 1) {
						sprintf(name_to_send, "%s/%s_%d_gpac.m4s", p_in_data->psz_out, p_vdata->psz_name, seg_nb);
						dc_message_queue_put(p_send_seg_mq, name_to_send, sizeof(name_to_send));
					}

					break;
				}

				frame_nb++;
			}
		}

		dc_video_muxer_close(&out_file);

#ifndef FRAGMENTER
		dc_message_queue_put(p_mq, p_vdata->psz_name, sizeof(p_vdata->psz_name));
#endif

		// If system is live,
		// Send the time that a segment is available to MPD generator thread.
		if (p_in_data->i_mode == LIVE_MEDIA || p_in_data->i_mode == LIVE_CAMERA) {
			if (p_thread_params->video_conf_idx == 0) {
				segtime t;
				t.segnum = seg_nb;
				t.time = gf_net_get_utc();
				//time_t t = time(NULL);
				dc_message_queue_put(p_mq, &t, sizeof(t));
			}
		}
	
		if (p_in_data->i_time_shift != -1) {
			shift = 1000 * p_in_data->i_time_shift / p_in_data->i_seg_dur;
			sprintf(name_to_delete, "%s/%s_%d_gpac.m4s", p_in_data->psz_out,
					p_vdata->psz_name, (seg_nb - shift));
			dc_message_queue_put(p_delete_seg_mq, name_to_delete,
					sizeof(name_to_delete));

		}

		seg_nb++;

		if (quit)
			break;

	}

	// If system is not live,
	// Send the duration of the video
	if (p_in_data->i_mode == ON_DEMAND) {
		if (p_thread_params->video_conf_idx == 0) {
			int dur = (seg_nb * seg_frame_max * 1000) / p_vdata->i_framerate;
			int dur_tot = (out_file.p_codec_ctx->frame_number * 1000)
					/ p_vdata->i_framerate;
			if (dur > dur_tot)
				dur = dur_tot;
			//printf("Duration: %d \n", dur);
			dc_message_queue_put(p_mq, &dur, sizeof(dur));
		}
	}

	/* Close output video file */
	dc_video_encoder_close(&out_file);

	dc_video_muxer_free(&out_file);

#ifdef DEBUG
	printf("Video encoder is exiting...\n");
#endif
	fprintf(stderr, "video encoder thread exit\n");
	return 0;
}

u32 audio_encoder_thread(void * p_params) {

	int ret;
	int exit_loop = 0;
	int quit = 0;
	int seg_nb = 0;
	//int seg_frame_max;
	//int frag_frame_max;
	int frame_per_seg;
	int frame_per_frag;
	int frame_nb;
	//int audio_frame_size = AUDIO_FRAME_SIZE;
	char name_to_delete[512];
	int shift;

	AudioMuxerType muxer_type = AUDIO_MUXER;

	AudioThreadParam * p_thread_params = (AudioThreadParam *) p_params;

	CmdData * p_in_data = p_thread_params->p_in_data;
	int audio_conf_idx = p_thread_params->audio_conf_idx;
	AudioData * p_adata = gf_list_get(p_in_data->p_audio_lst, audio_conf_idx);

	AudioInputData * p_aind = p_thread_params->p_aind;
	AudioOutputFile aout;

	MessageQueue * p_mq = p_thread_params->p_mq;
	MessageQueue * p_delete_seg_mq = p_thread_params->p_delete_seg_mq;

#ifndef FRAGMENTER
	MessageQueue * p_mq = p_thread_params->p_mq;
#endif

	if (!gf_list_count(p_in_data->p_audio_lst))
		return 0;

	//seg_frame_max = p_adata->i_samplerate
	//		* (float) (p_in_data->i_seg_dur / 1000.0);

	//frag_frame_max = p_adata->i_samplerate * (float) (p_in_data->i_frag_dur / 1000.0);
	//if (seg_frame_max <= 0)
	//	seg_frame_max = -1;

	if (dc_audio_encoder_open(&aout, p_adata) < 0) {
		fprintf(stderr, "Cannot open output audio stream.\n");
		p_in_data->i_exit_signal = 1;
		return -1;
	}

	frame_per_seg  = (int)((p_adata->i_samplerate / (double) aout.i_frame_size) * (p_in_data->i_seg_dur  / 1000.0));
	frame_per_frag = (int)((p_adata->i_samplerate / (double) aout.i_frame_size) * (p_in_data->i_frag_dur / 1000.0));

	optimize_seg_frag_dur(&frame_per_seg, &frame_per_frag);

	if (dc_audio_muxer_init(&aout, p_adata, muxer_type, frame_per_seg, frame_per_frag, p_in_data->i_seg_marker) < 0) {
		fprintf(stderr, "Cannot init output audio.\n");
		p_in_data->i_exit_signal = 1;
		return -1;
	}

	while (1) {

		frame_nb = 0;
		quit = 0;

		if (dc_audio_muxer_open(&aout, p_in_data->psz_out, p_adata->psz_name, seg_nb) < 0) {
			fprintf(stderr, "Cannot open output audio.\n");
			p_in_data->i_exit_signal = 1;
			return -1;
		}

		while (1) {

			exit_loop = 0;

//			if (frame_per_seg > 0) {
//				if (frame_nb == frame_per_seg) {
//
//					//if (dc_audio_encoder_flush(&aout, p_aind) == 0) {
//					//	dc_audio_muxer_write(&aout);
//					//	frame_nb ++;//= aout.p_codec_ctx->frame_size; //aout.acc_samples;
//					//}
//
//					exit_loop = 1;
//					break;
//				}
//			}

			ret = dc_audio_encoder_read(&aout, p_aind);

			if (ret == -2) {
#ifdef DEBUG
				printf("Audio encoder has no more data to encode.\n");
#endif
				//if (dc_audio_encoder_flush(&aout, p_aind) == 0) {
				//	dc_audio_muxer_write(&aout);
				//	frame_nb ++;//= aout.p_codec_ctx->frame_size; //aout.acc_samples;
				//}

				quit = 1;
				break;
			}

			while (1) {

				ret = dc_audio_encoder_encode(&aout, p_aind);

				if (ret == 1) {
					break;
				}

				if (ret == -1) {
					fprintf(stderr, "An error occured while encoding audio frame.\n");
					quit = 1;
					break;
				}

				ret = dc_audio_muxer_write(&aout, frame_nb);

				if (ret == -1) {
					fprintf(stderr, "An error occured while writing audio frame.\n");
					quit = 1;
					break;
				}

				if (ret == 1) {
					exit_loop = 1;
					break;
				}

				frame_nb++; //= aout.p_codec_ctx->frame_size; //aout.acc_samples;

			}

			if (exit_loop || quit)
				break;

		}

		dc_audio_muxer_close(&aout);

#ifndef FRAGMENTER
		dc_message_queue_put(p_mq, p_adata->psz_name,
				sizeof(p_adata->psz_name));
#endif

		// Send the time that a segment is available to MPD generator thread.
		if (p_in_data->i_mode == LIVE_CAMERA || p_in_data->i_mode == LIVE_MEDIA) {
			if (p_thread_params->audio_conf_idx == 0) {
				segtime t;
				t.segnum = seg_nb;
				t.time = gf_net_get_utc();
				//time_t t = time(NULL);
				dc_message_queue_put(p_mq, &t, sizeof(t));
			}
		}

		if (p_in_data->i_time_shift != -1) {
			shift = 1000 * p_in_data->i_time_shift / p_in_data->i_seg_dur;
			sprintf(name_to_delete, "%s/%s_%d_gpac.m4s", p_in_data->psz_out,
					p_adata->psz_name, (seg_nb - shift));
			dc_message_queue_put(p_delete_seg_mq, name_to_delete,
					sizeof(name_to_delete));

		}

		seg_nb++;

		if (quit)
			break;

	}

	// If system is not live,
	// Send the duration of the video
	if (p_in_data->i_mode == ON_DEMAND) {
		if (p_thread_params->audio_conf_idx == 0) {
			int dur = (seg_nb * aout.i_frame_size * frame_per_seg * 1000)
					/ p_adata->i_samplerate;
			int dur_tot = (aout.p_codec_ctx->frame_number * aout.i_frame_size
					* 1000) / p_adata->i_samplerate;
			if (dur > dur_tot)
				dur = dur_tot;
			//printf("Duration: %d \n", dur);
			dc_message_queue_put(p_mq, &dur, sizeof(dur));
		}
	}

	dc_audio_muxer_free(&aout);

	/* Close output audio file */
	dc_audio_encoder_close(&aout);

#ifdef DEBUG
	printf("Audio encoder is exiting...\n");
#endif
	return 0;
}

int dc_run_controler(CmdData * p_in_data) {

	u32 i, j;
	
	ThreadParam keyboard_th_params;
	ThreadParam mpd_th_params;
	ThreadParam delete_seg_th_params;
	ThreadParam send_frag_th_params;

	//Video parameters
	VideoThreadParam vdecoder_th_params;
	VideoThreadParam *vencoder_th_params = alloca(gf_list_count(p_in_data->p_video_lst) * sizeof(VideoThreadParam));
	VideoInputData vind;
	VideoInputFile * vinf[MAX_SOURCE_NUMBER];
	VideoScaledDataList p_vsdl;
	VideoThreadParam * vscaler_th_params = NULL;

	//Audio parameters
	AudioThreadParam adecoder_th_params;
	AudioThreadParam *aencoder_th_params = alloca(gf_list_count(p_in_data->p_audio_lst) * sizeof(AudioThreadParam));
	AudioInputData aind;
	AudioInputFile ainf;

#ifndef DASHER
	ThreadParam dasher_th_params;
#endif

#ifndef FRAGMENTER
	ThreadParam fragmenter_th_params;
#endif

	MessageQueue mq;
	MessageQueue delete_seg_mq;
	MessageQueue send_frag_mq;

	dc_register_libav();

	for (i = 0; i < MAX_SOURCE_NUMBER; i++)
		vinf[i] = gf_malloc(sizeof(VideoInputFile));

	dc_message_queue_init(&mq);
	dc_message_queue_init(&delete_seg_mq);
	dc_message_queue_init(&send_frag_mq);

	if (strcmp(p_in_data->vdata.psz_name, "") != 0) {

		dc_video_scaler_list_init(&p_vsdl, p_in_data->p_video_lst);

		vscaler_th_params = gf_malloc(p_vsdl.i_size * sizeof(VideoThreadParam));

		/* Open input video */
		if (dc_video_decoder_open(vinf[0], &p_in_data->vdata, p_in_data->i_mode, p_in_data->i_no_loop) < 0) {
			fprintf(stderr, "Cannot open input video.\n");
			return -1;
		}

		if (dc_video_input_data_init(&vind, /*vinf[0]->i_width, vinf[0]->i_height,
		 vinf[0]->i_pix_fmt,*/p_vsdl.i_size, p_in_data->i_mode, MAX_SOURCE_NUMBER)
				< 0) {
			fprintf(stderr, "Cannot initialize audio data.\n");
			return -1;
		}

		/* open other input videos for source switching */
		for (i = 0; i < gf_list_count(p_in_data->p_vsrc); i++) {
			VideoData * p_vconf = gf_list_get(p_in_data->p_vsrc, i);
			if (dc_video_decoder_open(vinf[i + 1], p_vconf, LIVE_MEDIA, 1)
					< 0) {
				fprintf(stderr, "Cannot open input video.\n");
				return -1;
			}

//			if (dc_video_input_data_init(&vind, vinf[i + 1]->i_width,
//					vinf[i + 1]->i_height, vinf[i + 1]->i_pix_fmt, p_vsdl.i_size,
//					p_in_data->i_mode, MAX_SOURCE_NUMBER) < 0) {
//				fprintf(stderr, "Cannot initialize audio data.\n");
//				return -1;
//			}
		}

//		dc_video_input_data_set_prop(&vind, 0, vinf[1]->i_width,
//				vinf[1]->i_height, vinf[1]->i_pix_fmt);

		//int source_nb = gf_list_count(p_in_data->p_vsrc);
		//printf("source_nb: %d \n", source_nb);
		for (i = 0; i < gf_list_count(p_in_data->p_vsrc) + 1; i++) {
			dc_video_input_data_set_prop(&vind, i, vinf[i]->i_width,
					vinf[i]->i_height, vinf[i]->i_pix_fmt);
		}

		for (i = 0; i < (u32)p_vsdl.i_size; i++) {
			dc_video_scaler_data_init(&vind, p_vsdl.p_vsd[i],
					MAX_SOURCE_NUMBER);

			for (j = 0; j < gf_list_count(p_in_data->p_vsrc) + 1; j++) {
				dc_video_scaler_data_set_prop(&vind, p_vsdl.p_vsd[i], j);
			}
		}

		/* Initialize video decoder thread */
		vdecoder_th_params.p_thread = gf_th_new("video_decoder_thread");

		for (i = 0; i < (u32)p_vsdl.i_size; i++) {
			vscaler_th_params[i].p_thread = gf_th_new("video_scaler_thread");
		}

		/* Initialize video encoder threads */
		for (i = 0; i < gf_list_count(p_in_data->p_video_lst); i++)
			vencoder_th_params[i].p_thread = gf_th_new("video_encoder_thread");

	}

	if (strcmp(p_in_data->adata.psz_name, "") != 0) {

		/* Open input audio */
		if (dc_audio_decoder_open(&ainf, &p_in_data->adata, p_in_data->i_mode,
				p_in_data->i_no_loop) < 0) {
			fprintf(stderr, "Cannot open input audio.\n");
			return -1;
		}

		if (dc_audio_input_data_init(&aind, p_in_data->adata.i_channels,
				p_in_data->adata.i_samplerate,
				gf_list_count(p_in_data->p_audio_lst), p_in_data->i_mode) < 0) {
			fprintf(stderr, "Cannot initialize audio data.\n");
			return -1;
		}

		/* Initialize audio decoder thread */
		adecoder_th_params.p_thread = gf_th_new("audio_decoder_thread");

		/* Initialize audio encoder threads */
		for (i = 0; i < gf_list_count(p_in_data->p_audio_lst); i++)
			aencoder_th_params[i].p_thread = gf_th_new("video_encoder_thread");

	}

	/******** Keyboard controler Thread ********/

	/* Initialize keyboard controller thread */
	keyboard_th_params.p_thread = gf_th_new("keyboard_thread");
	
	/* Create keyboard controller thread */
	keyboard_th_params.p_in_data = p_in_data;
	if (gf_th_run(keyboard_th_params.p_thread, keyboard_thread, (void *)&keyboard_th_params) != GF_OK) {

		fprintf(stderr,
				"Error while doing pthread_create for keyboard_thread.\n");
	}

	/********************************************/

	//Communication between decoder and audio encoder
	for (i = 0; i < gf_list_count(p_in_data->p_audio_lst); i++) {
		AudioData * p_tmp_adata = gf_list_get(p_in_data->p_audio_lst, i);
		p_tmp_adata->i_channels = p_in_data->adata.i_channels;
		p_tmp_adata->i_samplerate = p_in_data->adata.i_samplerate;
	}

	//Communication between decoder and video encoder
	for (i = 0; i < gf_list_count(p_in_data->p_video_lst); i++) {
		VideoData * p_tmp_vdata = gf_list_get(p_in_data->p_video_lst, i);
		p_tmp_vdata->i_framerate = p_in_data->vdata.i_framerate;
	}
	
	/******** MPD Thread ********/

	/* Initialize MPD generator thread */
	mpd_th_params.p_thread = gf_th_new("mpd_thread");
	
	/* Create MPD generator thread */
	mpd_th_params.p_in_data = p_in_data;
	mpd_th_params.p_mq = &mq;
	if (gf_th_run(mpd_th_params.p_thread, mpd_thread, (void *)&mpd_th_params) != GF_OK) {
		fprintf(stderr, "Error while doing pthread_create for mpd_thread.\n");
	}

	/****************************/

	if (strcmp(p_in_data->vdata.psz_name, "") != 0) {
		/* Create video encoder threads */
		for (i = 0; i < gf_list_count(p_in_data->p_video_lst); i++) {

			VideoData * p_vconf = gf_list_get(p_in_data->p_video_lst, i);

			vencoder_th_params[i].p_in_data = p_in_data;
			vencoder_th_params[i].video_conf_idx = i;
			vencoder_th_params[i].p_vsd = dc_video_scaler_get_data(&p_vsdl,
					p_vconf->i_width, p_vconf->i_height);

			vencoder_th_params[i].p_mq = &mq;
			vencoder_th_params[i].p_delete_seg_mq = &delete_seg_mq;
			vencoder_th_params[i].p_send_seg_mq = &send_frag_mq;

			if (gf_th_run(vencoder_th_params[i].p_thread, video_encoder_thread,
					(void *) &vencoder_th_params[i]) != GF_OK) {
				fprintf(stderr,
						"Error while doing pthread_create for video_encoder_thread.\n");
			}
		}

		/* Create video scaler threads */
		for (i = 0; i < (u32)p_vsdl.i_size; i++) {

			vscaler_th_params[i].p_in_data = p_in_data;
			vscaler_th_params[i].p_vsd = p_vsdl.p_vsd[i];
			vscaler_th_params[i].p_vind = &vind;

			if (gf_th_run(vscaler_th_params[i].p_thread, video_scaler_thread,
					(void *) &vscaler_th_params[i]) != GF_OK) {
				fprintf(stderr,
						"Error while doing pthread_create for video_scaler_thread.\n");
			}
		}
	}

	if (strcmp(p_in_data->adata.psz_name, "") != 0) {
		/* Create audio encoder threads */
		for (i = 0; i < gf_list_count(p_in_data->p_audio_lst); i++) {

			aencoder_th_params[i].p_in_data = p_in_data;
			aencoder_th_params[i].audio_conf_idx = i;
			aencoder_th_params[i].p_aind = &aind;

			aencoder_th_params[i].p_mq = &mq;
			aencoder_th_params[i].p_delete_seg_mq = &delete_seg_mq;
			aencoder_th_params[i].p_send_seg_mq = &send_frag_mq;

			if (gf_th_run(aencoder_th_params[i].p_thread, audio_encoder_thread,
					(void *) &aencoder_th_params[i]) != GF_OK) {
				fprintf(stderr,
						"Error while doing pthread_create for audio_encoder_thread.\n");
			}
		}
	}

	if (strcmp(p_in_data->vdata.psz_name, "") != 0) {
		/* Create video decoder thread */
		vdecoder_th_params.p_in_data = p_in_data;
		vdecoder_th_params.p_vind = &vind;
		vdecoder_th_params.p_vinf = vinf;
		if (gf_th_run(vdecoder_th_params.p_thread, video_decoder_thread,
				(void *) &vdecoder_th_params) != GF_OK) {

			fprintf(stderr,
					"Error while doing pthread_create for video_decoder_thread.\n");
		}
	}

	if (strcmp(p_in_data->adata.psz_name, "") != 0) {
		/* Create audio decoder thread */
		adecoder_th_params.p_in_data = p_in_data;
		adecoder_th_params.p_aind = &aind;
		adecoder_th_params.p_ainf = &ainf;
		if (gf_th_run(adecoder_th_params.p_thread, audio_decoder_thread,
				(void *) &adecoder_th_params) != GF_OK) {

			fprintf(stderr,
					"Error while doing pthread_create for audio_decoder_thread.\n");
		}
	}

	if (p_in_data->i_time_shift != -1) {

		/* Initialize delete segment thread */
		delete_seg_th_params.p_thread = gf_th_new("delete_seg_thread");

		delete_seg_th_params.p_in_data = p_in_data;
		delete_seg_th_params.p_mq = &delete_seg_mq;
		if (gf_th_run(delete_seg_th_params.p_thread, delete_seg_thread,
				(void *) &delete_seg_th_params) != GF_OK) {

			fprintf(stderr,
					"Error while doing pthread_create for delete_seg_thread.\n");
		}

	}

	if (p_in_data->i_send_message == 1) {

		/* Initialize delete segment thread */
		send_frag_th_params.p_thread = gf_th_new("send_frag_event_thread");

		send_frag_th_params.p_in_data = p_in_data;
		send_frag_th_params.p_mq = &send_frag_mq;
		if (gf_th_run(send_frag_th_params.p_thread, send_frag_event,
				(void *) &send_frag_th_params) != GF_OK) {

			fprintf(stderr,
					"Error while doing pthread_create for send_frag_event_thread.\n");
		}

	}

#ifndef FRAGMENTER

	if (strcmp(p_in_data->psz_mpd, "") != 0) {

		/* Initialize keyboard controller thread */
		fragmenter_th_params.p_thread = gf_th_new("fragmenter_thread");

		fragmenter_th_params.p_in_data = p_in_data;
		fragmenter_th_params.p_mq = &mq;
		if (gf_th_run(fragmenter_th_params.p_thread, fragmenter_thread,
						(void *) &fragmenter_th_params) != GF_OK) {

			fprintf(stderr,
					"Error while doing pthread_create for fragmenter_thread.\n");
		}

	}

#endif

#ifndef DASHER

	if (p_in_data->i_live && strcmp(p_in_data->psz_mpd, "") != 0) {

		/* Initialize keyboard controller thread */
		dasher_th_params.p_thread = gf_th_new("dasher_thread");

		dasher_th_params.p_in_data = p_in_data;
		if (gf_th_run(dasher_th_params.p_thread, dasher_thread,
						(void *) &dasher_th_params) != GF_OK) {

			fprintf(stderr,
					"Error while doing pthread_create for dasher_thread.\n");
		}

	}

#endif

	printf("Press q or Q to exit...\n");

	if (strcmp(p_in_data->adata.psz_name, "") != 0) {
		/* Wait for and destroy audio decoder threads */
		gf_th_stop(adecoder_th_params.p_thread);
		gf_th_del(adecoder_th_params.p_thread);
	}

	if (strcmp(p_in_data->vdata.psz_name, "") != 0) {
		/* Wait for and destroy video decoder threads */
		gf_th_stop(vdecoder_th_params.p_thread);
		gf_th_del(vdecoder_th_params.p_thread);
	}

	if (strcmp(p_in_data->adata.psz_name, "") != 0) {
		/* Wait for and destroy audio encoder threads */
		for (i = 0; i < gf_list_count(p_in_data->p_audio_lst); i++) {
			gf_th_stop(aencoder_th_params[i].p_thread);
			gf_th_del(aencoder_th_params[i].p_thread);
		}
	}

	if (strcmp(p_in_data->vdata.psz_name, "") != 0) {
		/* Wait for and destroy video encoder threads */
		for (i = 0; i < gf_list_count(p_in_data->p_video_lst); i++) {
			gf_th_stop(vencoder_th_params[i].p_thread);
			gf_th_del(vencoder_th_params[i].p_thread);
		}

		/* Wait for and destroy video scaler threads */
		for (i = 0; i < (u32)p_vsdl.i_size; i++) {
			gf_th_stop(vscaler_th_params[i].p_thread);
			gf_th_del(vscaler_th_params[i].p_thread);
		}
	}

#ifndef DASHER
	if (p_in_data->i_live && strcmp(p_in_data->psz_mpd, "") != 0) {
		/* Wait for and destroy dasher thread */
		gf_th_stop(dasher_th_params.p_thread);
		gf_th_del(dasher_th_params.p_thread);
	}

#endif

	if (strcmp(p_in_data->adata.psz_name, "") != 0) {
		/* Destroy audio input data */
		dc_audio_input_data_destroy(&aind);
		/* Close input audio */
		dc_audio_decoder_close(&ainf);
	}

	if (strcmp(p_in_data->vdata.psz_name, "") != 0) {

		/* Destroy video input data */
		dc_video_input_data_destroy(&vind);

		for (i = 0; i < gf_list_count(p_in_data->p_vsrc); i++) {
			/* Close input video */
			dc_video_decoder_close(vinf[i]);
		}

		for (i = 0; i < (u32)p_vsdl.i_size; i++) {
			dc_video_scaler_data_destroy(p_vsdl.p_vsd[i]);
		}

		/* Destroy video scaled data */
		dc_video_scaler_list_destroy(&p_vsdl);
	}

	keyboard_th_params.p_in_data->i_exit_signal = 0;

#ifndef FRAGMENTER

	dc_message_queue_flush(&mq);

	if (strcmp(p_in_data->psz_mpd, "") != 0) {
		/* Wait for and destroy dasher thread */
		gf_th_stop(fragmenter_th_params.p_thread);
		gf_th_del(fragmenter_th_params.p_thread);
	}

#endif

	/********** Keyboard thread ***********/

	/* Wait for and destroy keyboard controler thread */
	gf_th_stop(keyboard_th_params.p_thread);
	gf_th_del(keyboard_th_params.p_thread);

	/**************************************/

	/********** MPD generator thread ***********/

	/* Wait for and destroy MPD generator thread */
	gf_th_stop(mpd_th_params.p_thread);
	gf_th_del(mpd_th_params.p_thread);

	/**************************************/

	if (p_in_data->i_time_shift != -1) {
		//	dc_message_queue_flush(&delete_seg_mq);
		/* Wait for and destroy delete segment thread */
		gf_th_stop(delete_seg_th_params.p_thread);
		gf_th_del(delete_seg_th_params.p_thread);
	}

	if (p_in_data->i_send_message == 1) {
		/* Wait for and destroy delete segment thread */
		gf_th_stop(send_frag_th_params.p_thread);
		gf_th_del(send_frag_th_params.p_thread);
	}

#ifndef DASHER
	if (!p_in_data->i_live && strcmp(p_in_data->psz_mpd, "") != 0) {
		dasher_th_params.p_in_data = p_in_data;
		dasher_thread((void*) &dasher_th_params);
	}
#endif


	if (vscaler_th_params)
		gf_free(vscaler_th_params);


	for (i = 0; i < MAX_SOURCE_NUMBER; i++)
		gf_free(vinf[i]);

	dc_message_queue_free(&mq);
	dc_message_queue_free(&delete_seg_mq);
	dc_message_queue_free(&send_frag_mq);


	return 0;
}

