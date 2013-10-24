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

#ifndef VIDEO_ENCODER_H_
#define VIDEO_ENCODER_H_

#include "video_muxer.h"


/*
 * Open an video stream
 *
 * @param video_output_file [in] add a video stream to the file
 * with the parameters already passed to open_video_output
 *
 * @return 0 on success, -1 on failure
 */
int dc_video_encoder_open(VideoOutputFile *video_output_file, VideoDataConf *video_data_conf, Bool use_source_timing);

/*
 * Read the decoded video frames from circular buffer
 * of the corresponding resolution
 * and encode and write them on the output file
 *
 * @param video_output_file [in] video output file
 * @param video_scaled_data [in] scaled video data structure which
 * contains a circular buffer with video frames
 *
 * @return 0 on success, -1 on failure, -2 on finishing;
 * when there is no more data on circular buffer to encode
 */
int dc_video_encoder_encode(VideoOutputFile *video_output_file, VideoScaledData *video_scaled_data);

/*
 * Close the output video file
 *
 * @param video_output_file [in] video output file
 */
void dc_video_encoder_close(VideoOutputFile *video_output_file);

#endif /* VIDEO_ENCODER_H_ */
