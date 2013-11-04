/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau
 *			Copyright (c) Telecom ParisTech 2000-2013 - Romain Bouqueau 2013
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

#ifndef LIBAV_COMPAT_H_
#define LIBAV_COMPAT_H_

#ifndef URL_WRONLY
#define URL_WRONLY AVIO_FLAG_WRITE
#endif

#ifndef CODEC_ID_RAWVIDEO

#if (LIBAVCODEC_VERSION_MAJOR <= 53) && (LIBAVCODEC_VERSION_MINOR < 30)
#define CODEC_ID_RAWVIDEO AV_CODEC_ID_RAWVIDEO
#define CODEC_ID_H264 AV_CODEC_ID_H264
#endif
#endif

#ifndef AV_CH_LAYOUT_STEREO
#define AV_CH_FRONT_LEFT 0x00000001
#define AV_CH_FRONT_RIGHT 0x00000002
#define AV_CH_LAYOUT_STEREO (AV_CH_FRONT_LEFT|AV_CH_FRONT_RIGHT)
#endif

#endif

