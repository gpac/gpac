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
#include "libavutil/mem.h"
#include "circular_buffer.h"

#include <time.h>


/*
 * AudioInputData is designed to keep the data
 * of input audio in a circular buffer.
 * The circular buffer has its own mechanism for synchronization.
 */
typedef struct {

	/*
	 * The circular buffer of input audio.
	 * Input audio is the audio frames after decoding.
	 */
	CircularBuffer p_cb;

	/*
	 * The user of circular buffer has an index to it,
	 * which is in this variable.
	 */
	Producer pro;

	AVFrame * p_aframe;

	int64_t next_pts;

	int i_channels;
	int i_samplerate;

} AudioInputData;

/*
 * This structure corresponds to an
 * entry of audio configuration in the
 * configuration file
 */
typedef struct {
	/* audio file name */
	char psz_name[256];
	/* audio format */
	char psz_format[256];
	/* audio bitrate */
	int i_bitrate;
	/* audio samplerate */
	int i_samplerate;
	/* audio channel number */
	int i_channels;
	/* audio codec */
	char psz_codec[256];

	/* used for source switching */
	char psz_source_id[256];
	time_t start_time;
	time_t end_time;

} AudioData;



/*
 * Each node in a circular buffer is a pointer.
 * To use the circular buffer for audio frame we must
 * define the node. AudioDataNode simply contains
 * an AVFrame.
 */
typedef struct {

	uint8_t * p_abuf;
	int i_abuf_size;

} AudioDataNode;

void dc_audio_data_set_default(AudioData * adata);

/*
 * Initialize an AudioInputData.
 *
 * @param ain [out] is the structure to be initialize.
 * @param maxcon [in] contains information on the number of users of circular buffer;
 * which means the number of audio encoders.
 * @param live [in] indicates the system is live
 *
 * @return 0 on success, -1 on failure.
 *
 * @note Must use dc_audio_data_destroy to free memory.
 */
int dc_audio_input_data_init(AudioInputData * ain, int channels, int samplerate,  int maxcon, int mode);

/*
 * Destroy an AudioInputData
 *
 * @param ain [in] the structure to be destroyed.
 */
void dc_audio_input_data_destroy(AudioInputData * ain);

/*
 * Signal to all the users of the circular buffer in the AudioInputData
 * which the current node is the last node to consume.
 *
 * @param ain [in] the structure to be signaled on.
 */
void dc_audio_inout_data_end_signal(AudioInputData * ain);

#endif /* AUDIO_DATA_H_ */
