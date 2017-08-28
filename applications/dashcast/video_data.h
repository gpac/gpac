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

#ifndef VIDEO_DATA_H_
#define VIDEO_DATA_H_

#include "../../modules/ffmpeg_in/ffmpeg_in.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libav_compat.h"

#include "circular_buffer.h"

#include <time.h>

//anything different is broken in dash cast (random frame inversions at encoding time ...)
#define VIDEO_CB_DEFAULT_SIZE 1


/*
 * This structure corresponds to an
 * entry of video configuration in the
 * configuration file.
 */
typedef struct {
	/* video file name */
	char filename[GF_MAX_PATH];
	/* video format */
	char format[GF_MAX_PATH];
	/* video format */
	char pixel_format[GF_MAX_PATH];
	/* v4l2 format */
	char v4l2f[GF_MAX_PATH];
	/* left crop */
	int crop_x;
	/* top crop */
	int crop_y;
	/* video final width */
	int width;
	/* video final height */
	int height;
	/* video bitrate */
	int bitrate;
	/* video frame rate */
	int framerate;
	/* video codec */
	char codec[GF_MAX_PATH];
	/* RFC6381 codec name, only valid when VIDEO_MUXER == GPAC_INIT_VIDEO_MUXER_AVC1 */
	char codec6381[RFC6381_CODEC_NAME_SIZE_MAX];
	/* custom parameter to be passed directly to the encoder - free it once you're done */
	char custom[GF_MAX_PATH];
	/*low delay is used*/
	int low_delay;
	/*demuxer buffer size or 0 if default FFmpeg one is used*/
	int demux_buffer_size;

	/* used for source switching */
	char source_id[GF_MAX_PATH];
	time_t start_time;
	time_t end_time;

	//copy over from source file
	AVRational time_base;
	u64 frame_duration;
} VideoDataConf;

typedef struct {
	/* Width, height and pixel format of the input video. */
	int width;
	int height;
	int crop_x, crop_y;
	int pix_fmt;
	AVRational sar;
} VideoInputProp;

/*
 * VideoInputData is designed to keep the data
 * of input video in a circular buffer.
 * The circular buffer has its own mechanism for synchronization.
 */
typedef struct {
	/* The circular buffer of the video frames after decoding. */
	CircularBuffer circular_buf;
	/* The user of circular buffer has an index to it, which is in this variable. */
	Producer producer;

	VideoInputProp *vprop;

	/* Width, height and pixel format of the input video */
	//int width;
	//int height;
	//int pix_fmt;
	u64 frame_duration;
} VideoInputData;


/*
 * Each node in a circular buffer is a pointer.
 * To use the circular buffer for video frame we must
 * define the node. VideoDataNode simply contains
 * an AVFrame.
 */
typedef struct {
	AVFrame * vframe;
	int source_number;
	uint8_t nb_raw_frames_ref;
	AVPacket raw_packet;

	u64 frame_ntp, frame_utc;
} VideoDataNode;

void dc_video_data_set_default(VideoDataConf *video_data_conf);

/*
 * Initialize a VideoInputData.
 *
 * @param video_input_data [out] is the structure to be initialize.
 * @param width [in] input video width
 * @param height [in] input video height
 * @param pixfmt [in] input video pixel format
 * @param num_consumers [in] contains information on the number of users of circular buffer;
 * which means the number of video encoders.
 * @param live [in] indicates the system is live
 *
 * @return 0 on success, -1 on failure.
 *
 * @note Must use dc_video_data_destroy to free memory.
 */
int dc_video_input_data_init(VideoInputData *video_input_data,/* int width, int height, int pix_fmt,*/ int num_consumers, int mode, int num_producers, int video_cb_size);

/*
 * Set properties for a VideoInputData.
 */
void dc_video_input_data_set_prop(VideoInputData *video_input_data, int index, int width, int height, int crop_x, int crop_y, int pix_fmt, AVRational sar);

/*
 * Destroy a VideoInputData.
 *
 * @param video_input_data [in] the structure to be destroyed.
 */
void dc_video_input_data_destroy(VideoInputData *video_input_data);

/*
 * Signal to all the users of the circular buffer in the VideoInputData
 * which the current node is the last node to consume.
 *
 * @param video_input_data [in] the structure to be signaled on.
 */
void dc_video_input_data_end_signal(VideoInputData *video_input_data);

#endif /* VIDEO_DATA_H_ */
