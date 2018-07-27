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

#ifndef CONTROLER_H_
#define CONTROLER_H_

#include <gpac/tools.h>
#include <gpac/thread.h>
#include <gpac/media_tools.h>

#include "register.h"
#include "video_decoder.h"
#include "video_encoder.h"
#include "audio_decoder.h"
#include "audio_encoder.h"
#include "cmd_data.h"
#include "message_queue.h"


/* General thread parameters */
typedef struct {
	/* command data */
	CmdData *in_data;
	/* handle to thread */
	GF_Thread *thread;

	MessageQueue *mq;
} ThreadParam;

/* Video thread parameters */
typedef struct {
	/* command data */
	CmdData *in_data;
	/* The index in the configuration file to a video entry corresponding to the thread. */
	int video_conf_idx;
	/* Video input data structure corresponding to the thread. (This data is shared between video decoder and video scaler) */
	VideoInputData *video_input_data;
	/* Video scaled data structure corresponding to the thread. (This data is shared between video scaler and video encoder) */
	VideoScaledData *video_scaled_data;
	/* Video input file structure corresponding to the thread */
	VideoInputFile **video_input_file;
	/* handle to the thread */
	GF_Thread *thread;

	MessageQueue *mq;
	MessageQueue *delete_seg_mq;
	MessageQueue *send_seg_mq;
} VideoThreadParam;

/* Audio thread parameters */
typedef struct {
	/* command data */
	CmdData *in_data;
	/* The index in the configuration file to an audio entry corresponding to the thread */
	int audio_conf_idx;
	/* Audio input data (This data is shared between audio decoder and audio encoder */
	AudioInputData *audio_input_data;
	/* Audio input file structure */
	AudioInputFile *audio_input_file;
	/* handle to the thread */
	GF_Thread *thread;

	MessageQueue *mq;
	MessageQueue *delete_seg_mq;
	MessageQueue *send_seg_mq;
} AudioThreadParam;

/*
 * Run controler runs all decoder, scalers, and encoders
 * of audio and video
 *
 * @param cmd_data [in] command data
 *
 * @return 0 on success, -1 on failure
 */
int dc_run_controler(CmdData *);

#endif /* CONTROLER_H_ */
