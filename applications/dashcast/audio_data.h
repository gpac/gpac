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

#ifndef AUDIO_DATA_H_
#define AUDIO_DATA_H_

#define AUDIO_CB_SIZE 3

#define LIVE_FRAME_SIZE 1024
#define MAX_AUDIO_PACKET_SIZE (128 * 1024)


#include "../../modules/ffmpeg_in/ffmpeg_in.h"
#include "libavcodec/avcodec.h"
#include "libavutil/channel_layout.h"
#include "libavutil/mem.h"
#include "libav_compat.h"
#include "circular_buffer.h"

#include <time.h>

//we force the number of channels between the decoder and the encoder: interleaved 16 bits stereo 44100Hz
#define DC_AUDIO_SAMPLE_RATE 44100
#define DC_AUDIO_NUM_CHANNELS 2
#define DC_AUDIO_CHANNEL_LAYOUT AV_CH_LAYOUT_STEREO
#define DC_AUDIO_SAMPLE_FORMAT AV_SAMPLE_FMT_S16

#define DC_AUDIO_MAX_CHUNCK_SIZE 192000


/*
 * AudioInputData is designed to keep the data of input audio in a circular buffer.
 * The circular buffer has its own mechanism for synchronization.
 */
typedef struct {
	/* The circular buffer of input audio. Input audio is the audio frames after decoding. */
	CircularBuffer circular_buf;

	/* The user of circular buffer has an index to it, which is in this variable. */
	Producer producer;

	AVFrame *aframe;

	int64_t next_pts;

	int channels;
	int samplerate;
} AudioInputData;

/*
 * This structure corresponds to an entry of audio configuration in the configuration file
 */
typedef struct {
	/* audio file name */
	char filename[GF_MAX_PATH];
	/* audio format */
	char format[GF_MAX_PATH];
	/* audio bitrate */
	int bitrate;
	/* audio samplerate */
	int samplerate;
	/* audio channel number */
	int channels;
	/* audio codec */
	char codec[GF_MAX_PATH];
	/* custom parameter to be passed directly to the encoder - free it once you're done */
	char custom[GF_MAX_PATH];

	/* used for source switching */
	char source_id[GF_MAX_PATH];
	time_t start_time;
	time_t end_time;

	/* RFC6381 codec name, only valid when VIDEO_MUXER == GPAC_INIT_VIDEO_MUXER_AVC1 */
	char codec6381[RFC6381_CODEC_NAME_SIZE_MAX];
} AudioDataConf;

/*
 * Each node in a circular buffer is a pointer.
 * To use the circular buffer for audio frame we must
 * define the node. AudioDataNode simply contains
 * an AVFrame.
 */
typedef struct {
	uint8_t *abuf;
	int abuf_size;
	uint64_t channel_layout;
	int sample_rate;
	int format;
	int channels;
} AudioDataNode;

void dc_audio_data_set_default(AudioDataConf *audio_data_conf);

/*
 * Initialize an AudioInputData.
 *
 * @param audio_input_data [out] is the structure to be initialize.
 * @param num_consumers [in] contains information on the number of users of circular buffer;
 * which means the number of audio encoders.
 * @param live [in] indicates the system is live
 *
 * @return 0 on success, -1 on failure.
 *
 * @note Must use dc_audio_data_destroy to free memory.
 */
int dc_audio_input_data_init(AudioInputData *audio_input_data, int channels, int samplerate, int num_consumers, int mode);

/*
 * Destroy an AudioInputData
 *
 * @param audio_input_data [in] the structure to be destroyed.
 */
void dc_audio_input_data_destroy(AudioInputData *audio_input_data);

/*
 * Signal to all the users of the circular buffer in the AudioInputData
 * which the current node is the last node to consume.
 *
 * @param audio_input_data [in] the structure to be signaled on.
 */
void dc_audio_inout_data_end_signal(AudioInputData *audio_input_data);

#endif /* AUDIO_DATA_H_ */
