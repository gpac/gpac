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
 * @param voutf [in] add a video stream to the file
 * with the parameters already passed to open_video_output
 *
 * @return 0 on success, -1 on failure
 */
int dc_video_encoder_open(VideoOutputFile * voutf, VideoData * vdata, Bool use_source_timing);

/*
 * Read the decoded video frames from circular buffer
 * of the corresponding resolution
 * and encode and write them on the output file
 *
 * @param voutf [in] video output file
 * @param vsd [in] scaled video data structure which
 * contains a circular buffer with video frames
 *
 * @return 0 on success, -1 on failure, -2 on finishing;
 * when there is no more data on circular buffer to encode
 */
int dc_video_encoder_encode(VideoOutputFile * voutf, VideoScaledData * vsd);


/*
 * Close the output video file
 *
 * @param voutf [in] video output file
 */
void dc_video_encoder_close(VideoOutputFile * voutf);


#endif /* VIDEO_ENCODER_H_ */
