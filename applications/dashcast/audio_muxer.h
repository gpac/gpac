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

#ifndef AUDIO_MUXER_H_
#define AUDIO_MUXER_H_

#include <stdlib.h>
#include "../../modules/ffmpeg_in/ffmpeg_in.h"
#include "libavutil/fifo.h"
#include "libavformat/avformat.h"
#ifndef WIN32
#include "libavdevice/avdevice.h"
#endif
#include "libavutil/mathematics.h"
#include <gpac/isomedia.h>
#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>
#include "audio_data.h"
#include "libav_compat.h"


typedef enum {

	FFMPEG_AUDIO_MUXER,
	GPAC_AUDIO_MUXER,
	GPAC_INIT_AUDIO_MUXER

} AudioMuxerType;

/*
 * AudioOutputFile structure has the data needed
 * to encode audio samples and write them on the file.
 * It reads the data from a circular buffer so it needs
 * to keep the index to that circular buffer. This index is
 * available in Consumer data structure.
 *
 */
typedef struct {

	//AudioData * p_adata;

	/* File format context structure */
	AVFormatContext * p_fmt;
	AVCodec * p_codec;
	AVCodecContext * p_codec_ctx;

	GF_ISOFile * p_isof;
	GF_ISOSample * p_sample;
	int dts;

	/* The index to the audio stream in the file */
	int i_astream_idx;

	/* It keeps the index with which encoder access to the circular buffer (as a consumer) */
	Consumer acon;

	/* Variables that encoder needs to encode data */
	AVFrame * p_aframe;
	uint8_t * p_adata_buf;
	int i_frame_bytes;
	AVPacket packet;

	/*
	 * Audio samples stored in the AVFrame are not always
	 * complete. Which means more than 1 AVFrame is needed
	 * to complete an access unit. This fifo is provided
	 * to store audio samples in the fifo and once an access unit
	 * is complete we can encode it.
	 */
	AVFifoBuffer * p_fifo;

	AudioMuxerType muxer_type;

	/* Accumulated sample */
	//int acc_samples;

	int i_frame_per_seg;
	int i_frame_per_frag;
	int i_first_dts;

	u32 i_seg_marker;

	int i_frame_size;

} AudioOutputFile;

int dc_audio_muxer_init(AudioOutputFile * aoutf, AudioData * aconf, AudioMuxerType muxer_type,
		int frame_per_seg, int frame_per_frag, u32 seg_marker);
void dc_audio_muxer_free(AudioOutputFile * aoutf);

/*
 * Open the output audio
 *
 * @param aoutf [out] open the audio output on this file
 *
 * @param aconf [in] the structure containing the
 * configuration of the output file (bitrate, samplerate, name, channels)
 *
 * @return 0 on success, -1 on failure
 */
GF_Err dc_audio_muxer_open(AudioOutputFile * aoutf, char * psz_directory, char * psz_id, int i_seg);
GF_Err dc_audio_muxer_write(AudioOutputFile * aout, int i_frame_nb);
GF_Err dc_audio_muxer_close(AudioOutputFile * aoutf);


#endif /* AUDIO_MUXER_H_ */
