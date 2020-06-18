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

#ifndef VIDEO_DECODER_H_
#define VIDEO_DECODER_H_

#include "video_data.h"

#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"


/*
 * The structure which keeps the data of
 * input video file.
 */
typedef struct {
	/* Format context structure provided by avlib to open and read from a media file. */
	AVFormatContext *av_fmt_ctx;
	/* A reference counter on the format context (may be shared with other sources). Currently redundant with av_pkt_list non-NULLness. */
	int av_fmt_ctx_ref_cnt;
	/* A list of AVPackets and return value to be processed: when this parameter is non-null,
	 * the video thread makes the demux and pushes the packets. Packets must be freed when retrieved.*/
	GF_List  *av_pkt_list;
	GF_Mutex *av_pkt_list_mutex;
	/* The index of the video stream in the file. */
	int vstream_idx;
	/* video width, height, and pixel format. */
	int width;
	int height;
	int pix_fmt;
	AVRational sar;

	int mode;
	int no_loop, nb_consumers;

	u32 frame_decoded;
	u32 pts_init;
	u64 first_pts, prev_pts, pts_dur_estimate, sync_tolerance;
	u64 utc_at_init;
} VideoInputFile;

/*
 * Open the input video
 *
 * @param cmd_data [in] contains information about the file name
 * and the video format.
 *
 * @param video_input_file [out] pointer to the structure which we want to
 * open the file
 *
 * @return 0 on success -1 on failure.
 */
int dc_video_decoder_open(VideoInputFile *video_input_file, VideoDataConf *video_data_conf, int mode, int no_loop, int nb_consumers);

/*
 * Read and decode video and put decoded frames on circular buffer
 *
 * @param video_input_file [in] contains info on input video.
 * @param video_input_data [out] the decoded samples will be put
 * on the circular buffer of this parameter.
 *
 * @return 0 on success, -1 on failure, -2 on EOF (end of the file)
 */
int dc_video_decoder_read(VideoInputFile *video_input_file, VideoInputData *video_input_data, int source_number, int use_source_timing, int is_live_capture, const int *exit_signal_addr);

/*
 * Close the input video
 *
 * @param video_input_file [in] the video file to be closed
 *
 */
void dc_video_decoder_close(VideoInputFile *);

#endif /* VIDEO_DECODER_H_ */
