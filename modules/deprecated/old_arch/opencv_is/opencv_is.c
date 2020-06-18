/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2009-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / OpenCV demo module
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


#include <gpac/modules/codec.h>
#include <gpac/scenegraph_vrml.h>
#include <gpac/thread.h>

#include <cvaux.h>
#include <highgui.h>



typedef struct
{
	GF_Thread *th;
	Bool running;

} GF_OpenCV;


static Bool OCV_RegisterDevice(struct __input_device *ifce, const char *urn, GF_BitStream *dsi, void (*AddField)(struct __input_device *_this, u32 fieldType, const char *name))
{
	if (strncmp(urn, "OpenCV", 6)) return 0;

	AddField(ifce, GF_SG_VRML_SFVEC2F, "position");

	return 1;
}



CvHaarClassifierCascade* load_object_detector( const char* cascade_path )
{
	return (CvHaarClassifierCascade*)cvLoad( cascade_path, NULL, NULL, NULL );
}

int prev_x0=0, prev_y0=0;

void detect_and_draw_objects(GF_InputSensorDevice *ifce, IplImage* image,
                             CvHaarClassifierCascade* cascade,
                             int do_pyramids )
{
	IplImage* small_image = image;
	CvMemStorage* storage = cvCreateMemStorage(0);
	CvSeq* faces;
	int i, scale = 1;
	//CvRect* theRealFace;
	int theRealX=0, theRealY=0, theRealHeight=0 , theRealWidth=0;

	int tmpMaxSurface=0;


	if( do_pyramids )
	{
		small_image = cvCreateImage( cvSize(image->width/2,image->height/2), IPL_DEPTH_8U, 3 );
		cvPyrDown( image, small_image, CV_GAUSSIAN_5x5 );
		scale = 2;
	}

	faces = cvHaarDetectObjects( small_image, cascade, storage, 1.2, 2, CV_HAAR_DO_CANNY_PRUNING, cvSize(0,0) );

	for( i = 0; i < faces->total; i++ )
	{

		CvRect face_rect = *(CvRect*)cvGetSeqElem( faces, i );
		/* cvRectangle( image, cvPoint(face_rect.x*scale,face_rect.y*scale),
		              cvPoint((face_rect.x+face_rect.width)*scale,
		                      (face_rect.y+face_rect.height)*scale),
		              CV_RGB(0,255,0), 3 );*/
		if(face_rect.width*face_rect.height>tmpMaxSurface) {
			theRealX=face_rect.x;
			theRealY=face_rect.y;
			theRealHeight=face_rect.height;
			theRealWidth=face_rect.width;
			tmpMaxSurface=face_rect.width*face_rect.height;
		}

	}
	cvRectangle( image, cvPoint(theRealX*scale,theRealY*scale),
	             cvPoint((theRealX+theRealWidth)*scale,
	                     (theRealY+theRealHeight)*scale),
	             CV_RGB(0,255,0), 3, 8, 0 );

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[OpenCV] translation selon X : %d - translation selon Y : %d\n", (theRealX - prev_x0), (theRealY -prev_y0) ));

	/*send data frame to GPAC*/
	{
		char *buf;
		u32 buf_size;
		GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_float(bs, (Float) (theRealX - 640/2) );
		gf_bs_write_float(bs, (Float) (480/2 - theRealY) );

		gf_bs_align(bs);
		gf_bs_get_content(bs, &buf, &buf_size);
		gf_bs_del(bs);
		ifce->DispatchFrame(ifce, buf, buf_size);
		gf_free(buf);
	}


	prev_x0=theRealX;
	prev_y0=theRealY;

	if( small_image != image )
		cvReleaseImage( &small_image );

	cvReleaseMemStorage( &storage );
}

static u32 OCV_Run(void *par)
{
	IplImage* image;
	CvCapture *capture;
	GF_InputSensorDevice *ifce = (GF_InputSensorDevice *)par;
	GF_OpenCV *ocv = (GF_OpenCV *)ifce->udta;

	capture= cvCaptureFromCAM(0);
	cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH,640  );
	cvSetCaptureProperty( capture, CV_CAP_PROP_FRAME_HEIGHT, 480 );

	cvNamedWindow( "test", 0 );

	image = NULL;
	while (ocv->running) {
		if (cvGrabFrame(capture)) {
			CvHaarClassifierCascade* cascade;

			image = cvRetrieveFrame(capture);

			cascade = load_object_detector("haarcascade_frontalface_default.xml");
			detect_and_draw_objects(ifce, image, cascade, 1 );


			cvShowImage( "test", image );
			cvWaitKey(40);
		}
	}
	if (image) cvReleaseImage( &image);
	return 0;
}

static void OCV_Start(struct __input_device *ifce)
{
	GF_OpenCV *ocv = (GF_OpenCV*)ifce->udta;
	ocv->running = 1;
	gf_th_run(ocv->th, OCV_Run, ifce);
}

static void OCV_Stop(struct __input_device *ifce)
{
	GF_OpenCV *ocv = (GF_OpenCV*)ifce->udta;
	ocv->running = 0;
}


GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_INPUT_DEVICE_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	GF_InputSensorDevice *plug;
	GF_OpenCV *udta;
	if (InterfaceType != GF_INPUT_DEVICE_INTERFACE) return NULL;

	GF_SAFEALLOC(plug, GF_InputSensorDevice);
	GF_REGISTER_MODULE_INTERFACE(plug, GF_INPUT_DEVICE_INTERFACE, "GPAC Demo InputSensor", "gpac distribution")

	plug->RegisterDevice = OCV_RegisterDevice;
	plug->Start = OCV_Start;
	plug->Stop = OCV_Stop;

	GF_SAFEALLOC(udta, GF_OpenCV);
	plug->udta = udta;
	udta->th = gf_th_new("OpenCV");

	return (GF_BaseInterface *)plug;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *bi)
{
	GF_InputSensorDevice *ifcn = (GF_InputSensorDevice*)bi;
	if (ifcn->InterfaceType==GF_INPUT_DEVICE_INTERFACE) {
		gf_free(bi);
	}
}

GPAC_MODULE_STATIC_DECLARATION( opencv_is )
