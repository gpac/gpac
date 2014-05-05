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
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/mathematics.h"
#include <gpac/isomedia.h>
#include <gpac/internal/media_dev.h>

#include "video_scaler.h"


typedef enum {
	FFMPEG_VIDEO_MUXER,
	RAW_VIDEO_H264,
	GPAC_VIDEO_MUXER,
	GPAC_INIT_VIDEO_MUXER_AVC1,
	GPAC_INIT_VIDEO_MUXER_AVC3
} VideoMuxerType;

/*
 * VideoOutputFile structure has the data needed to encode video frames and write them on the file.
 * It reads the data from a circular buffer so it needs to keep the index to that circular buffer. This index is
 * available in Consumer data structure.
 */
typedef struct {
	VideoDataConf *video_data_conf;
	VideoMuxerType muxer_type;

	/* file format context structure */
	AVFormatContext *av_fmt_ctx;
	AVCodecContext *codec_ctx;
	AVCodec *codec;

	FILE *file;

	GF_ISOFile *isof;
	GF_ISOSample *sample;
	u32 trackID;
	/* Index of the video stream in the file */
	int vstream_idx;
	/* keeps the index with which encoder access to the circular buffer (as a consumer) */
	Consumer consumer;

	/* Variables that encoder needs to encode data */
	uint8_t *vbuf;
	int vbuf_size;
	int encoded_frame_size;

	int frame_per_fragment;
	int frame_per_segment;

	int seg_dur;
	int frag_dur;

	u64 first_dts;

	u32 seg_marker;

	int gop_size;
	int gdr;

	Bool use_source_timing;

	u64 pts_at_segment_start;
	u64 last_pts, last_dts;
	u64 frame_dur;
	u32 timescale;
	Bool fragment_started, segment_started;
	const char *rep_id;
} VideoOutputFile;

int dc_video_muxer_init(VideoOutputFile *video_output_file, VideoDataConf *video_data_conf, VideoMuxerType muxer_type, int frame_per_segment, int frame_per_fragment, u32 seg_marker, int gdr, int seg_dur, int frag_dur, int frame_dur, int gop_size, int video_cb_size);

int dc_video_muxer_free(VideoOutputFile *video_output_file);

int dc_video_muxer_open(VideoOutputFile *video_output_file, char *directory, char *id_name, int seg);

int dc_video_muxer_write(VideoOutputFile *video_output_file, int frame_nb, Bool insert_utc);

int dc_video_muxer_close(VideoOutputFile *video_output_file);

#endif /* VIDEO_MUXER_H_ */
