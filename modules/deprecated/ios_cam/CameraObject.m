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

#import "CameraObject.h"

@implementation CameraObject

- (id)init {
	if ((self = [super init])) {
        @autoreleasepool {
            AVCaptureSession* tmp = [[AVCaptureSession alloc] init];
            [self setCaptureSession:tmp];
            [tmp release];
        
            input = NULL;
            output = NULL;
            callback = NULL;
        
            m_width = 640;
            m_height = 480;
            m_color = kCVPixelFormatType_32BGRA;
            m_stride = m_width * 4;
        }
	}
	return self;
}

- (void)dealloc{
    
    [super dealloc];
}

- (int) setupFormat:(int)width :(int)height :(int)color
{
    desiredWidth = width;
    desiredHeight = height;
    desiredColor = color;
    return 0;
}

- (int) getFormat:(unsigned int*)width :(unsigned int*)height :(int*)color :(int*)stride
{
    if ( !m_width || !m_height )
        return 1;
    *width = m_width;
    *height = m_height;
    *color = m_color;
    *stride = m_stride;
    
    return 0;
}

- (int) startCam
{
    NSError *error = nil;
    
    // Configure the session to produce lower resolution video frames, if your
    // processing algorithm can cope. We'll specify medium quality for the
    // chosen device.
    
    [self captureSession].sessionPreset = AVCaptureSessionPreset640x480;
    
    // Find a suitable AVCaptureDevice
    AVCaptureDevice *device = [AVCaptureDevice
                               defaultDeviceWithMediaType:AVMediaTypeVideo];
    
    // Create a device input with the device and add it to the session.
    input = [AVCaptureDeviceInput deviceInputWithDevice:device
                                                  error:&error];
    if (!input) {
        // Handling the error appropriately.
    }
    [[self captureSession] addInput:input];
    
    // Create a VideoDataOutput and add it to the session
    output = [[[AVCaptureVideoDataOutput alloc] init] autorelease];
    [[self captureSession] addOutput:output];
    
    // Configure your output.
    dispatch_queue_t queue = dispatch_queue_create("myQueue", NULL);
    [output setSampleBufferDelegate:self queue:queue];
    dispatch_release(queue);
    
    // Specify the pixel format
    output.videoSettings =
    [NSDictionary dictionaryWithObject:
     [NSNumber numberWithInt:kCVPixelFormatType_32BGRA]
                                forKey:(id)kCVPixelBufferPixelFormatTypeKey];
    
    
    // If you wish to cap the frame rate to a known value, such as 15 fps, set
    // minFrameDuration.
    output.minFrameDuration = CMTimeMake(1, 15);
    
    // Start the session running to start the flow of data
    [[self captureSession] startRunning];
    
    return 0;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection
{
    if ( !callback )
        return;
    
    CVPixelBufferRef pixelBuffer = (CVPixelBufferRef)CMSampleBufferGetImageBuffer(sampleBuffer);
    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    
    void *baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);
    
    //size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
    // Get the pixel buffer width and height
    //size_t width = CVPixelBufferGetWidth(pixelBuffer);
    //size_t height = CVPixelBufferGetHeight(pixelBuffer);
    
    unsigned int size = m_height * m_stride;//CVPixelBufferGetDataSize(pixelBuffer);
    
    callback(baseAddress, size);
    
    CVPixelBufferUnlockBaseAddress(pixelBuffer,0);
}

- (int) setCallback:(GetPixelsCallback*) func
{
    callback = func;
    
    return 0;
}

- (int) stopCam
{
    [[self captureSession] stopRunning];
    [[self captureSession] removeInput: input];
    input = NULL;
    [[self captureSession] removeOutput: output];
    output = NULL;
    
    return 0;
}

- (int) isCamStarted
{
    return [self captureSession].running;
}

@end
