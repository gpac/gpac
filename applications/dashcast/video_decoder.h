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

#include "libav/include/libavformat/avformat.h"
#include "libav/include/libavdevice/avdevice.h"

#include "video_data.h"

/*
 * The structure which keeps the data of
 * input video file.
 */
typedef struct {

	/*
	 * Format context structure provided by avlib
	 * to open and read from a media file
	 */
	AVFormatContext * p_fmt_ctx;
	/*
	 * The index of the video stream
	 * in the file
	 */
	int i_vstream_idx;
	/*
	 * video width, height, and pixel format
	 */
	int i_width;
	int i_height;
	int i_pix_fmt;

	int i_mode;
	int i_no_loop;

} VideoInputFile;

/*
 * Open the input video
 *
 * @param cmdd [in] contains information about the file name
 * and the video format.
 *
 * @param vinf [out] pointer to the structure which we want to
 * open the file
 *
 * @return 0 on success -1 on failure.
 */
int dc_video_decoder_open(VideoInputFile * vinf, VideoData *vdata, int mode, int no_loop);
/*
 * Read and decode video and put decoded frames on circular buffer
 *
 * @param vinf [in] contains info on input video.
 * @param vind [out] the decoded samples will be put
 * on the circular buffer of this parameter.
 *
 * @return 0 on success, -1 on failure, -2 on EOF (end of the file)
 *
 */
int dc_video_decoder_read(VideoInputFile * vinf, VideoInputData * vind, int source_number);
/*
 * Close the input video
 *
 * @param vinf [in] the video file to be closed
 *
 */
void dc_video_decoder_close(VideoInputFile *);

#endif /* VIDEO_DECODER_H_ */
