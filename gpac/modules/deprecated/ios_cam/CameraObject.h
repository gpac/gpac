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

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#include "cam_wrap.h"

@interface CameraObject : NSObject
	<AVCaptureVideoDataOutputSampleBufferDelegate>
{
	//    AVCaptureSession *session;
	//    AVCaptureDevice *device;
	AVCaptureDeviceInput *input;
	AVCaptureVideoDataOutput *output;

	int desiredWidth;
	int desiredHeight;
	int desiredColor;

	int m_width;
	int m_height;
	int m_color;
	int m_stride;

	GetPixelsCallback *callback;
}

@property (retain) AVCaptureSession *captureSession;

- (int) setupFormat:(int)width :(int)height :(int)color;
- (int) getFormat:(unsigned int*)width :(unsigned int*)height :(int*)color :(int*)stride;
- (int) startCam;
- (int) stopCam;
- (int) isCamStarted;
- (int) setCallback:(GetPixelsCallback*) func;

- (void)captureOutput:(AVCaptureOutput *)captureOutput
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
    fromConnection:(AVCaptureConnection *)connection;

@end
