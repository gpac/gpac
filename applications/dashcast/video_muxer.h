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

#ifndef VIDEO_MUXER_H_
#define VIDEO_MUXER_H_

#include "../../modules/ffmpeg_in/ffmpeg_in.h"
#include "libavformat/avformat.h"
#ifndef WIN32
#include "libavdevice/avdevice.h"
#endif
#include "libswscale/swscale.h"
#include "libavutil/mathematics.h"
#include <gpac/isomedia.h>
#include <gpac/internal/media_dev.h>

#include "video_scaler.h"
#include "libav_compat.h"


typedef enum {

	FFMPEG_VIDEO_MUXER,
	RAW_VIDEO_H264,
	GPAC_VIDEO_MUXER,
	GPAC_INIT_VIDEO_MUXER_AVC1,
	GPAC_INIT_VIDEO_MUXER_AVC3

} VideoMuxerType;

/*
 * VideoOutputFile structure has the data needed
 * to encode video frames and write them on the file.
 * It reads the data from a circular buffer so it needs
 * to keep the index to that circular buffer. This index is
 * available in Consumer data structure.
 *
 */
typedef struct {

	//VideoData * p_vdata;

	VideoMuxerType muxer_type;

	/* file format context structure */
	AVFormatContext * p_fmt;
	AVCodecContext * p_codec_ctx;
	AVCodec * p_codec;

	FILE * p_file;

	GF_ISOFile * p_isof;
	GF_ISOSample * p_sample;

	/* Index of the video stream in the file */
	int i_vstream_idx;
	/* keeps the index with which encoder access to the circular buffer (as a consumer) */
	Consumer vcon;

	/* Variables that encoder needs to encode data */
	uint8_t * p_vbuf;
	int i_vbuf_size;
	int i_encoded_frame_size;

	int frame_per_fragment;
	int frame_per_segment;
	u64 first_dts;

	u32 i_seg_marker;

	int i_gop_size;
	int i_gdr;

} VideoOutputFile;

int dc_video_muxer_init(VideoOutputFile * p_voutf, VideoData * p_vdata,
		VideoMuxerType muxer_type, int frame_per_segment, int frame_per_fragment, u32 seg_marker, int gdr);
int dc_video_muxer_free(VideoOutputFile * p_voutf);

int dc_video_muxer_open(VideoOutputFile * p_voutf, char * psz_directory, char * psz_id, int i_seg);
int dc_video_muxer_write(VideoOutputFile * p_voutf, int i_frame_nb);
int dc_video_muxer_close(VideoOutputFile * p_voutf);

#endif /* VIDEO_MUXER_H_ */
