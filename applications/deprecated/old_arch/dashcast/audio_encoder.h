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

#ifndef AUDIO_ENCODER_H_
#define AUDIO_ENCODER_H_

#include "audio_muxer.h"

/*
 * Open an audio stream
 *
 * @param audio_output_file [in] add an audio stream to the file
 * with the parameters already passed to open_audio_output
 *
 * @return 0 on success, -1 on failure
 */
int dc_audio_encoder_open(AudioOutputFile *audio_output_file, AudioDataConf *audio_data_conf);

int dc_audio_encoder_read(AudioOutputFile *audio_output_file, AudioInputData *audio_input_data);

//int dc_audio_encoder_flush(AudioOutputFile *audio_output_file, AudioInputData *audio_input_data);

/*
 * Read the decoded audio sample from circular buffer (which is in audio_input_data)
 * and encode and write them on the output file
 *
 * @param audio_output_file [in] audio output file
 * @param audio_input_data [in] audio input data structure which contains a circular buffer with audio samples
 *
 * @return 0 on success, -1 on failure, -2 on finishing;
 * when there is no more data on circular buffer to encode
 */
int dc_audio_encoder_encode(AudioOutputFile *audio_output_file, AudioInputData *audio_input_data);

/*
 * Close the output audio file
 *
 * @param audio_output_file [in] audio output file
 */
void dc_audio_encoder_close(AudioOutputFile *audio_output_file);

#endif /* AUDIO_ENCODER_H_ */
