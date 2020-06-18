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
#include "libavdevice/avdevice.h"
#ifdef DC_AUDIO_RESAMPLER
#include "libswresample/swresample.h"
#endif
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include <gpac/isomedia.h>
#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>
#include "audio_data.h"


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
	AudioDataConf *audio_data_conf;

	/* File format context structure */
	AVFormatContext *av_fmt_ctx;
	AVCodec *codec;
	AVCodecContext *codec_ctx;

#ifndef GPAC_DISABLE_ISOM
	GF_ISOFile *isof;
	GF_ISOSample *sample;
#endif

	int dts;

	/* The index to the audio stream in the file */
	int astream_idx;

	/* It keeps the index with which encoder access to the circular buffer (as a consumer) */
	Consumer consumer;

#ifdef DC_AUDIO_RESAMPLER
	/* Optional audio resampling between the decoder and the encoder */
	SwrContext *aresampler;
#endif

	/* Variables that encoder needs to encode data */
	AVFrame *aframe;
	uint8_t *adata_buf;
	int frame_bytes;
	AVPacket packet;

	/*
	 * Audio samples stored in the AVFrame are not always
	 * complete. Which means more than 1 AVFrame is needed
	 * to complete an access unit. This fifo is provided
	 * to store audio samples in the fifo and once an access unit
	 * is complete we can encode it.
	 */
	AVFifoBuffer *fifo;

	AudioMuxerType muxer_type;

	/* Accumulated sample */
	//int acc_samples;

	int frame_per_seg;
	int frame_per_frag;
	int first_dts;

	u32 seg_marker;
	u64 frame_ntp;
} AudioOutputFile;

int dc_audio_muxer_init(AudioOutputFile *audio_output_file, AudioDataConf *audio_data_conf, AudioMuxerType muxer_type, int frame_per_seg, int frame_per_frag, u32 seg_marker);
void dc_audio_muxer_free(AudioOutputFile *audio_output_file);

/*
 * Open the output audio
 *
 * @param audio_output_file [out] open the audio output on this file
 *
 * @param audio_data_conf [in] the structure containing the
 * configuration of the output file (bitrate, samplerate, name, channels)
 *
 * @return 0 on success, -1 on failure
 */
GF_Err dc_audio_muxer_open(AudioOutputFile *audio_output_file, char *directory, char *id_name, int seg);

GF_Err dc_audio_muxer_write(AudioOutputFile *audio_output_file, int frame_nb, Bool insert_ntp);

GF_Err dc_audio_muxer_close(AudioOutputFile *audio_output_file);

#endif /* AUDIO_MUXER_H_ */
