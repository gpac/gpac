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

#ifndef AUDIO_DECODER_H_
#define AUDIO_DECODER_H_

#include "audio_data.h"

#include "libavformat/avformat.h"
#include "libavutil/fifo.h"
#ifdef DC_AUDIO_RESAMPLER
#include "libavutil/opt.h"
#include "libswresample/swresample.h"
#endif


/*
 * The structure which keeps the data of
 * input audio file.
 */
typedef struct {
	/* Format context structure provided by avlib to open and read from a media file. */
	AVFormatContext *av_fmt_ctx;

	/* A list of AVPackets and return value to be processed: when this parameter is non-null,
	 * the video thread makes the demux and pushes the packets. */
	GF_List  *av_pkt_list;
	GF_Mutex *av_pkt_list_mutex;

	/* The index of the audio stream in the file. */
	int astream_idx;

	/* This is the output FIFO linking the decoder to the other encoder: only conveys
	 * stereo 44100 (and resample if needed) */
	AVFifoBuffer *fifo;
#ifdef DC_AUDIO_RESAMPLER
	/* Optional audio resampling between the decoder and the encoder */
	SwrContext *aresampler;
#endif

	LockMode mode;
	int no_loop;
} AudioInputFile;

/*
 * Open the input audio
 *
 * @param cmd_data [in] contains information about the file name
 * and the audio format.
 *
 * @param audio_input_file [out] pointer to the structure which we want to
 * open the file
 *
 * @return 0 on success -1 on failure.
 */
int dc_audio_decoder_open(AudioInputFile *audio_input_file, AudioDataConf *audio_data_conf, int mode, int no_loop, int video_framerate);

/*
 * Read and decode audio and put samples on circular buffer
 *
 * @param audio_input_file [in] contains info on input audio. This parameter
 * must have been opened with open_audio_input
 *
 * @param audio_input_data [out] the samples will be saved on the circular buffer
 * of this parameter.
 *
 * @return 0 on success, -1 on failure, -2 on EOF (end of the file)
 */
int dc_audio_decoder_read(AudioInputFile *audio_input_file, AudioInputData *audio_input_data);

/*
 * Close the input audio
 *
 * @param audio_input_file [in] the audio file to be closed
 */
void dc_audio_decoder_close(AudioInputFile *audio_input_file);

#endif /* AUDIO_DECODER_H_ */
