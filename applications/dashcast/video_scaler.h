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

#ifndef VIDEO_SCALER_H_
#define VIDEO_SCALER_H_

#include <stdlib.h>
#include <gpac/list.h>

#include "video_data.h"

typedef struct {
	/* scaler of the libav */
	struct SwsContext * p_sws_ctx;
	/* width, height, and the pixel format of the scaled video */
	int i_in_width;
	int i_in_height;
	int i_in_pix_fmt;
} VideoScaledProp;
/*
 * VideoScaledData keeps a circular buffer
 * of video frame with a defined resolution.
 *
 */
typedef struct {

	VideoScaledProp * p_vsprop;

	int i_out_width;
	int i_out_height;
	int i_out_pix_fmt;

	/* scaler of the libav */
	//struct SwsContext * p_sws_ctx;
	/* width, height, and the pixel format of the scaled video */
	//int i_width;
	//int i_height;
	//int i_pix_fmt;

	/* circular buffer containing the scaled video frames */
	CircularBuffer p_cb;
	/* Scaler is a consumer and also producer.
	 * It consumes from the video input data and
	 * it produces the video scaled data.
	 * So it deals with two circular buffer and we
	 * need to keep the index for both.
	 */
	Producer svpro;
	Consumer svcon;

	/*
	 * The number of consumer of this circular buffer.
	 * (Which are the encoders who are using this resolution)
	 */
	int i_maxcon;
	int i_maxsource;

	u64 frame_duration;
} VideoScaledData;

/*
 * Each node in a circular buffer is a pointer.
 * To use the circular buffer for scaled video frame
 * we must define the node. This structure contains
 * the data needed to encode a video frame.
 */

typedef struct {

	AVFrame * p_vframe;
	uint8_t * p_pic_data_buf;
} VideoScaledDataNode;

/*
 * A list of pointers to scaled video data.
 */
typedef struct {

	VideoScaledData ** p_vsd;
	int i_size;

} VideoScaledDataList;

/*
 * Read the configuration file info and fill the video scaled data list
 * with all the resolution available. Each resolution is associated to a
 * circular buffer in a video scaled data.
 *
 * @param cmdd [in] Command data which contains the configuration file info
 * @param vsdl [out] the list to be filled
 *
 */
void dc_video_scaler_list_init(VideoScaledDataList * vsdl, GF_List * video_lst);

/*
 * Destroy a video scaled data list
 *
 * @param vsdl [in] the list to be destroyed
 *
 */
void dc_video_scaler_list_destroy(VideoScaledDataList * vsdl);

/*
 * Signal to all the users of the circular buffer in the VideoScaledData
 * which the current node is the last node to consume.
 *
 * @param vsd [in] the structure to be signaled on.
 */
void dc_video_scaler_end_signal(VideoScaledData * vsd);
/*
 * Initialize a VideoScaledData.
 *
 * @param vind [in] contains the info of the input video
 * @param vsd [out] structure to be initialized
 *
 * @return 0 on success, -1 on failure.
 *
 * @note Must use dc_video_scaler_data_destroy to free memory.
 */
int dc_video_scaler_data_init(VideoInputData * vind, VideoScaledData * vsd, int maxsource);

int dc_video_scaler_data_set_prop(VideoInputData * vind, VideoScaledData * vsd, int index);
/*
 * Get a frame from the circular buffer on the input video,
 * scale it and put the result on the circular buffer of the
 * video scaled data
 *
 * @param vind [in] contains input frames
 * @param vsd [out] contains scaled frames
 *
 * return 0 on success, -2 if the node is the last node to scale
 */
int dc_video_scaler_scale(VideoInputData * vind, VideoScaledData * vsd);
/*
 * Destroy a VideoScaledData
 *
 * @param vsd [in] structure to be destroyed.
 */
int dc_video_scaler_data_destroy(VideoScaledData * vsd);
/*
 * Return the VideoScaledData from the list which has width and height
 *
 * @param vsdl [in] video scaled data list
 * @param width [in] frame width
 * @param height [in] frame height
 *
 * @return a VideoScaledData which corresponds to the width and height on success, NULL on failure
 */
VideoScaledData * dc_video_scaler_get_data(VideoScaledDataList * vsdl, int width, int height);

#endif /* VIDEO_SCALER_H_ */
