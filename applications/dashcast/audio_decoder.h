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

/*
 * The structure which keeps the data of
 * input audio file.
 */
typedef struct {

	/*
	 * Format context structure provided by avlib
	 * to open and read from a media file
	 */
	AVFormatContext * p_fmt;

	/*
	 * The index of the audio stream
	 * in the file
	 */
	int i_astream_idx;

	AVFifoBuffer * p_fifo;

	int i_mode;
	int i_no_loop;

} AudioInputFile;

/*
 * Open the input audio
 *
 * @param cmdd [in] contains information about the file name
 * and the audio format.
 *
 * @param ainf [out] pointer to the structure which we want to
 * open the file
 *
 * @return 0 on success -1 on failure.
 */
int dc_audio_decoder_open(AudioInputFile * ainf, AudioData * adata, int mode, int no_loop);

/*
 * Read and decode audio and put samples on circular buffer
 *
 * @param ainf [in] contains info on input audio. This parameter
 * must have been opened with open_audio_input
 *
 * @param aind [out] the samples will be saved on the circular buffer
 * of this parameter.
 *
 * @return 0 on success, -1 on failure, -2 on EOF (end of the file)
 *
 */
int dc_audio_decoder_read(AudioInputFile * ainf, AudioInputData * aind);

/*
 * Close the input audio
 *
 * @param ainf [in] the audio file to be closed
 *
 */
void dc_audio_decoder_close(AudioInputFile * ainf);

#endif /* AUDIO_DECODER_H_ */
