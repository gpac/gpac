/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Ivica Arsov, Jean Le Feuvre
 *			Copyright (c) Mines-Telecom 2009-
 *					All rights reserved
 *
 *  This file is part of GPAC / iOS camera module
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

#ifndef _cam_wrap_h
#define _cam_wrap_h

#ifdef __cplusplus
extern "C" {
#endif

typedef void (GetPixelsCallback)(unsigned char* pixels, unsigned int size);

void* CAM_CreateInstance();

int CAM_DestroyInstance(void** inst);

int CAM_SetupFormat(void* inst, int width, int height, int color);

int CAM_GetCurrentFormat(void* inst, unsigned int* width, unsigned int* height, int* color, int* stride);

int CAM_Start(void* inst);

int CAM_Stop(void* inst);

int CAM_IsStarted(void* inst);

int CAM_SetCallback(void* inst, GetPixelsCallback *callback);

#ifdef __cplusplus
}
#endif

#endif
