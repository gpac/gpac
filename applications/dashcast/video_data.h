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

#include "circular_buffer.h"

#include <time.h>

#define VIDEO_CB_SIZE 3

/*
 * This structure corresponds to an
 * entry of video configuration in the
 * configuration file.
 */
typedef struct {

	/* video file name */
	char psz_name[256];
	/* video format */
	char psz_format[256];
	/* v4l2 format */
	char psz_v4l2f[256];
	/* video width */
	int i_width;
	/* video height */
	int i_height;
	/* video bitrate */
	int i_bitrate;
	/* video frame rate */
	int i_framerate;
	/* video codec */
	char psz_codec[256];

	/* used for source switching */
	char psz_source_id[256];
	time_t start_time;
	time_t end_time;

} VideoData;


typedef struct {

	/*
	 * Width, height and pixel format
	 * of the input video
	 */
	int i_width;
	int i_height;
	int i_pix_fmt;

} VideoInputProp;

/*
 * VideoInputData is designed to keep the data
 * of input video in a circular buffer.
 * The circular buffer has its own mechanism for synchronization.
 */


typedef struct {
	/*
	 * The circular buffer of
	 * the video frames after decoding.
	 */
	CircularBuffer p_cb;
	/*
	 * The user of circular buffer has an index to it,
	 * which is in this variable.
	 */
	Producer pro;

	VideoInputProp * p_vprop;

	/*
	 * Width, height and pixel format
	 * of the input video
	 */
	//int i_width;
	//int i_height;
	//int i_pix_fmt;

} VideoInputData;


/*
 * Each node in a circular buffer is a pointer.
 * To use the circular buffer for video frame we must
 * define the node. VideoDataNode simply contains
 * an AVFrame.
 */
typedef struct {

	AVFrame * p_vframe;

	int source_number;

} VideoDataNode;

void dc_video_data_set_default(VideoData * vdata);

/*
 * Initialize a VideoInputData.
 *
 * @param vind [out] is the structure to be initialize.
 * @param width [in] input video width
 * @param height [in] input video height
 * @param pixfmt [in] input video pixel format
 * @param maxcon [in] contains information on the number of users of circular buffer;
 * which means the number of video encoders.
 * @param live [in] indicates the system is live
 *
 * @return 0 on success, -1 on failure.
 *
 * @note Must use dc_video_data_destroy to free memory.
 */
int dc_video_input_data_init(VideoInputData * vind,/* int width, int height, int pix_fmt,*/ int maxcon, int mode, int maxsource);

void dc_video_input_data_set_prop(VideoInputData * vind, int index, int width, int height, int pix_fmt);
/*
 * Destroy a VideoInputData
 *
 * @param vind [in] the structure to be destroyed.
 */
void dc_video_input_data_destroy(VideoInputData * vind);

/*
 * Signal to all the users of the circular buffer in the VideoInputData
 * which the current node is the last node to consume.
 *
 * @param vind [in] the structure to be signaled on.
 */
void dc_video_input_data_end_signal(VideoInputData * vind);

#endif /* VIDEO_DATA_H_ */
