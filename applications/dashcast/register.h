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

#ifndef REGISTER_H_
#define REGISTER_H_

//#include <pthread.h>
#include "../../modules/ffmpeg_in/ffmpeg_in.h"
#include "libavcodec/avcodec.h"
#ifndef WIN32
#include "libavdevice/avdevice.h"
#endif
#include "libavformat/avformat.h"

#include <gpac/thread.h>

/* 
 * Register all codecs and define
 * the lock manager on top of avlib
 */
void dc_register_libav();

#endif /* REGISTER_H_ */
