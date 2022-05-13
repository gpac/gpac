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

#include "cam_wrap.h"
#include <stdlib.h>

#include "CameraObject.h"

void* CAM_CreateInstance(){
    void* obj = [[CameraObject alloc] init];
    return obj;
}

int CAM_DestroyInstance(void** inst){
    [(id)*inst dealloc];
    *inst = NULL;
    return 0;
}

int CAM_SetupFormat(void* inst, int width, int height, int color) {
    if ( !inst )
        return 1;
    
    return [(id)inst setupFormat: width :height :color];
}

int CAM_GetCurrentFormat(void* inst, unsigned int* width, unsigned int* height, int* color, int* stride) {
    if ( !inst )
        return 1;
    
    return [(id)inst getFormat: width :height :color :stride];
}

int CAM_Start(void* inst){
    if ( !inst )
        return 1;
    
    return [(id)inst startCam];
}

int CAM_Stop(void* inst){
    if ( !inst )
        return 1;
           
    return [(id)inst stopCam];
}

int CAM_IsStarted(void* inst){
    if ( !inst )
        return 1;
                    
    return [(id)inst isCamStarted];
}

int CAM_SetCallback(void* inst, GetPixelsCallback *callback){
    if ( !inst )
        return 1;
                            
    return [(id)inst setCallback: callback];
}
