/*
 * openhevc.h wrapper to openhevc or ffmpeg
 * Copyright (c) 2012-2013 Micka�l Raulet, Wassim Hamidouche, Gildas Cocherel, Pierre Edouard Lepere
 *
 * This file is part of openhevc.
 *
 * openHevc is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * openhevc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with openhevc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef OPEN_HEVC_WRAPPER_H
#define OPEN_HEVC_WRAPPER_H

#define NV_VERSION  "2.0" ///< Current software version

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void* OpenHevc_Handle;

typedef struct OpenHevc_Rational{
    int num; ///< numerator
    int den; ///< denominator
} OpenHevc_Rational;

typedef struct OpenHevc_FrameInfo
{
   int         nYPitch;
   int         nUPitch;
   int         nVPitch;
   int         nBitDepth;
   int         nWidth;
   int         nHeight;
   OpenHevc_Rational  sample_aspect_ratio;
   OpenHevc_Rational  frameRate;
   int         display_picture_number;
   int         flag; //progressive, interlaced, interlaced top field first, interlaced bottom field first.
   int64_t     nTimeStamp;
} OpenHevc_FrameInfo;

typedef struct OpenHevc_Frame
{
   void**      pvY;
   void**      pvU;
   void**      pvV;
   OpenHevc_FrameInfo frameInfo;
} OpenHevc_Frame;

typedef struct OpenHevc_Frame_cpy
{
   void*        pvY;
   void*        pvU;
   void*        pvV;
   OpenHevc_FrameInfo frameInfo;
} OpenHevc_Frame_cpy;

OpenHevc_Handle libOpenHevcInit(int nb_pthreads, int thread_type);
int libOpenHevcStartDecoder(OpenHevc_Handle openHevcHandle);
int  libOpenHevcDecode(OpenHevc_Handle openHevcHandle, const unsigned char *buff, int nal_len, int64_t pts);
void libOpenHevcGetPictureInfo(OpenHevc_Handle openHevcHandle, OpenHevc_FrameInfo *openHevcFrameInfo);
void libOpenHevcCopyExtraData(OpenHevc_Handle openHevcHandle, unsigned char *extra_data, int extra_size_alloc);

void libOpenHevcGetPictureInfoCpy(OpenHevc_Handle openHevcHandle, OpenHevc_FrameInfo *openHevcFrameInfo);
int  libOpenHevcGetOutput(OpenHevc_Handle openHevcHandle, int got_picture, OpenHevc_Frame *openHevcFrame);
int  libOpenHevcGetOutputCpy(OpenHevc_Handle openHevcHandle, int got_picture, OpenHevc_Frame_cpy *openHevcFrame);
void libOpenHevcSetCheckMD5(OpenHevc_Handle openHevcHandle, int val);
void libOpenHevcSetDebugMode(OpenHevc_Handle openHevcHandle, int val);
void libOpenHevcSetTemporalLayer_id(OpenHevc_Handle openHevcHandle, int val);
void libOpenHevcSetNoCropping(OpenHevc_Handle openHevcHandle, int val);
void libOpenHevcSetActiveDecoders(OpenHevc_Handle openHevcHandle, int val);
void libOpenHevcSetViewLayers(OpenHevc_Handle openHevcHandle, int val);
void libOpenHevcClose(OpenHevc_Handle openHevcHandle);
void libOpenHevcFlush(OpenHevc_Handle openHevcHandle);
void libOpenHevcFlushSVC(OpenHevc_Handle openHevcHandle, int decoderId);

const char *libOpenHevcVersion(OpenHevc_Handle openHevcHandle);

#ifdef __cplusplus
}
#endif

#endif // OPEN_HEVC_WRAPPER_H
