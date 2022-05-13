/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Ivica Arsov, Jean Le Feuvre
 *			Copyright (c) Mines-Telecom 2009-
 *					All rights reserved
 *
 *  This file is part of GPAC / Android camera module
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

#include <gpac/terminal.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/modules/codec.h>
#include <gpac/constants.h>
#include <gpac/modules/service.h>
#include <gpac/thread.h>
#include <gpac/media_tools.h>

#include <jni.h>
#include <android/log.h>

#define LOG_TAG "ANDROID_CAMERA"
#ifdef ANDROID
#  define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#  define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#  define QUOTEME_(x) #x
#  define QUOTEME(x) QUOTEME_(x)
#  define LOGI(...) fprintf(stderr, "I/" LOG_TAG " (" __FILE__ ":" QUOTEME(__LINE__) "): " __VA_ARGS__)
#  define LOGE(...) fprintf(stderr, "E/" LOG_TAG "(" ")" __VA_ARGS__)
#endif

static JavaVM* javaVM = 0;
static jclass camCtrlClass;
static jmethodID cid;
static jmethodID startCamera;
static jmethodID stopCamera;
static jmethodID startProcessing;
static jmethodID stopProcessing;
static jmethodID getImageFormat;
static jmethodID getImageHeight;
static jmethodID getImageWidth;
static jmethodID getBitsPerPixel;

#ifndef GPAC_STATIC_MODULES
jint JNI_OnLoad(JavaVM* vm, void* reserved)
#else
jint static_JNI_OnLoad(JavaVM* vm, void* reserved)
#endif
{
	JNIEnv* env = 0;
	javaVM = vm;

	if ( (*javaVM)->GetEnv(javaVM, (void**)&env, JNI_VERSION_1_2) != JNI_OK )
		return -1;

	// Get the class and its methods in the main env

	// Get the CameraController class
	// This is just a local refenrece. Cannot be used in the other JNI calls.
	camCtrlClass = (*env)->FindClass(env, "io/gpac/gpac/Preview");
	if (camCtrlClass == 0)
	{
		LOGE("CameraController class = null [myCamera.c, startCameraNative()]");
		return -1;
	}

	// Get Global Reference to be able to use the class
	camCtrlClass = (*env)->NewGlobalRef(env, camCtrlClass);
	if ( camCtrlClass == 0 )
	{
		LOGE("[MPEG-V_IN] Cannot create Global Reference\n");
		return -1;
	}

	// Get the method ID for the CameraController constructor.
	cid = (*env)->GetMethodID(env, camCtrlClass, "<init>", "()V");
	if (cid == 0)
	{
		LOGE("Method ID for CameraController constructor is null [myCamera.c, startCameraNative()]");
		return -1;
	}

	// Get startCamera() method from class CameraController
	startCamera = (*env)->GetMethodID(env, camCtrlClass, "initializeCamera", "(Z)Z");
	if (startCamera == 0)
	{
		LOGE("[ANDROID_CAMERA] Function startCamera not found");
		return -1;
	}

	stopCamera = (*env)->GetMethodID(env, camCtrlClass, "stopCamera", "()V");
	if (stopCamera == 0)
	{
		LOGE("[ANDROID_CAMERA] Function stopCamera not found");
		return -1;
	}

	startProcessing = (*env)->GetMethodID(env, camCtrlClass, "resumePreview", "()V");
	if (startProcessing == 0)
	{
		LOGE("[ANDROID_CAMERA] Function startProcessing not found");
		return -1;
	}

	stopProcessing = (*env)->GetMethodID(env, camCtrlClass, "pausePreview", "()V");
	if (stopProcessing == 0)
	{
		LOGE("[ANDROID_CAMERA] Function stopProcessing not found");
		return -1;
	}

	getImageFormat = (*env)->GetMethodID(env, camCtrlClass, "getPreviewFormat", "()I");
	if (getImageFormat == 0)
	{
		LOGE("[ANDROID_CAMERA] Function getImageFormat not found");
		return -1;
	}

	getImageHeight = (*env)->GetMethodID(env, camCtrlClass, "getImageHeight", "()I");
	if (getImageHeight == 0)
	{
		LOGE("[ANDROID_CAMERA] Function getImageHeight not found");
		return -1;
	}

	getImageWidth = (*env)->GetMethodID(env, camCtrlClass, "getImageWidth", "()I");
	if (getImageWidth == 0)
	{
		LOGE("[ANDROID_CAMERA] Function getImageWidth not found");
		return -1;
	}

	getBitsPerPixel = (*env)->GetMethodID(env, camCtrlClass, "getBitsPerPixel", "()I");
	if (getBitsPerPixel == 0)
	{
		LOGE("[ANDROID_CAMERA] Function getBitsPerPixel not found");
		return -1;
	}

	return JNI_VERSION_1_2;
}

#ifndef GPAC_STATIC_MODULES
//----------------------------------------------------------------------
JavaVM* GetJavaVM()
{
	return javaVM;
}

JNIEnv* GetEnv()
{
	JNIEnv* env;
	if ( (*GetJavaVM())->GetEnv(GetJavaVM(), (void**)&env, JNI_VERSION_1_2) != JNI_OK )
		return NULL;

	return env;
}
#else
JavaVM* GetJavaVM();
JNIEnv* GetEnv();

#endif

void JNI_OnUnload(JavaVM *vm, void *reserved)
{
	JNIEnv* env = 0;

	if ( (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_2) != JNI_OK )
		return;

	(*env)->DeleteGlobalRef(env, camCtrlClass);
}
//----------------------------------------------------------------------


//#define CAM_PIXEL_FORMAT GF_PIXEL_NV21
//GF_PIXEL_RGB_32
//#define CAM_PIXEL_SIZE 1.5f
//4
#define CAM_WIDTH 640
#define CAM_HEIGHT 480

//GF_PIXEL_RGB_24;

typedef struct
{
	GF_InputService *input;
	/*the service we're responsible for*/
	GF_ClientService *service;
	LPNETCHANNEL channel;

	/*input file*/
	u32 time_scale;

	u32 base_track_id;

	struct _tag_terminal *term;

	u32 cntr;

	u32 width;
	u32 height;

	Bool started;

	JNIEnv* env;
	u8 isAttached;
	jclass camCtrlClass;
	jmethodID cid;
	jobject camCtrlObj;
	jmethodID startCamera;
	jmethodID stopCamera;
	jmethodID startProcessing;
	jmethodID stopProcessing;
	jmethodID getImageFormat;
	jmethodID getImageHeight;
	jmethodID getImageWidth;
	jmethodID getBitsPerPixel;

} ISOMReader;

ISOMReader* globReader;

void loadCameraControler(ISOMReader *read);
void camStartCamera(ISOMReader *read);
void camStopCamera(ISOMReader *read);

Bool CAM_CanHandleURL(GF_InputService *plug, const char *url)
{
	if (!strnicmp(url, "hw://camera", 11)) return 1;

	return 0;
}

void unloadCameraControler(ISOMReader *read)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] unloadCameraControler: %d\n", gf_th_id()));
	if ( read->isAttached )
	{
		//(*rc->env)->PopLocalFrame(rc->env, NULL);
		(*GetJavaVM())->DetachCurrentThread(GetJavaVM());
		read->isAttached = 0;
	}

	read->env = NULL;
}

u32 unregisterFunc(void* data)
{
	unloadCameraControler(globReader);
	return 0;
}

void loadCameraControler(ISOMReader *read)
{
	JNIEnv* env = NULL;
	jint res = 0;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] loadCameraControler: %d\n", gf_th_id()));

	// Get the JNI interface pointer
	res = (*GetJavaVM())->GetEnv(javaVM, (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED )
	{
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] The current thread is not attached to the VM, assuming native thread\n"));
		res = (*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
		if ( res )
		{
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[ANDROID_CAMERA] Attach current thread failed: %d\n", res));
			return;
		}
		gf_register_before_exit_function(gf_th_current(), unregisterFunc);
		read->isAttached = 1;
		//(*rc->env)->PushLocalFrame(rc->env, 2);
	}
	else if ( res == JNI_EVERSION )
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[ANDROID_CAMERA] The specified version is not supported\n"));
		return;
	}

	read->env = env;
	read->camCtrlClass = camCtrlClass;
	read->cid = cid;
	read->startCamera = startCamera;
	read->stopCamera = stopCamera;
	read->startProcessing = startProcessing;
	read->stopProcessing = stopProcessing;
	read->getImageFormat = getImageFormat;
	read->getImageHeight = getImageHeight;
	read->getImageWidth = getImageWidth;
	read->getBitsPerPixel = getBitsPerPixel;

	// Create the object.
	read->camCtrlObj = (*env)->NewObject(env, read->camCtrlClass, read->cid);
	if (read->camCtrlObj == 0)
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("CameraController object creation failed. [myCamera.c, startCameraNative()]"));
		return;
	}
}

GF_Err CAM_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	ISOMReader *read;
	if (!plug || !plug->priv || !serv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] CAM_ConnectService: %d\n", gf_th_id()));

	read->input = plug;
	read->service = serv;
	read->base_track_id = 1;
	read->time_scale = 1000;

	read->term = serv->term;

	loadCameraControler(read);

	camStartCamera(read);

	/*reply to user*/
	gf_service_connect_ack(serv, NULL, GF_OK);
	//if (read->no_service_desc) isor_declare_objects(read);

	return GF_OK;
}

GF_Err CAM_CloseService(GF_InputService *plug)
{
	GF_Err reply;
	ISOMReader *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] CAM_CloseService: %d\n", gf_th_id()));

	reply = GF_OK;

	(*GetEnv())->DeleteLocalRef( GetEnv(), read->camCtrlObj );
	read->camCtrlObj = NULL;

	//unloadCameraControler(read);

	gf_service_disconnect_ack(read->service, NULL, reply);
	return GF_OK;
}

u32 getWidth(ISOMReader *read);
u32 getHeight(ISOMReader *read);
u32 getFormat(ISOMReader *read);
u32 getBitsPerPix(ISOMReader *read);

static GF_Descriptor *CAM_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	u32 trackID;
	ISOMReader *read;
	char *buf;
	u32 buf_size;
	if (!plug || !plug->priv) return NULL;
	read = (ISOMReader *) plug->priv;

	trackID = read->base_track_id;
	read->base_track_id = 0;

	if (trackID && (expect_type==GF_MEDIA_OBJECT_VIDEO) ) {
		GF_BitStream *bs;
		GF_ESD *esd;
		GF_ObjectDescriptor *od;
		od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		od->objectDescriptorID = 1;

		esd = gf_odf_desc_esd_new(0);
		esd->slConfig->timestampResolution = 1000;
		esd->decoderConfig->streamType = GF_STREAM_VISUAL;
		esd->ESID = 1;
		esd->decoderConfig->objectTypeIndication = GPAC_OTI_RAW_MEDIA_STREAM;

		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

		read->width = getWidth(read);
		read->height = getHeight(read);

		gf_bs_write_u32(bs, getFormat(read)); // fourcc
		gf_bs_write_u16(bs, read->width); // width
		gf_bs_write_u16(bs, read->height); // height
		gf_bs_write_u32(bs, read->width * read->height * getBitsPerPix(read) / 8); // framesize
		gf_bs_write_u32(bs, read->width); // stride

		gf_bs_align(bs);
		gf_bs_get_content(bs, &buf, &buf_size);
		gf_bs_del(bs);

		esd->decoderConfig->decoderSpecificInfo->data = buf;
		esd->decoderConfig->decoderSpecificInfo->dataLength = buf_size;

		gf_list_add(od->ESDescriptors, esd);
		return (GF_Descriptor *) od;
	}

	return NULL;
}




GF_Err CAM_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	GF_Err e;
	ISOMReader *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] CAM_ConnectChannel: %d\n", gf_th_id()));

	e = GF_OK;
	if (upstream) {
		e = GF_ISOM_INVALID_FILE;
	}

	read->channel = channel;

	gf_service_connect_ack(read->service, channel, e);
	return e;
}

GF_Err CAM_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	GF_Err e;
	ISOMReader *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] CAM_DisconnectChannel: %d\n", gf_th_id()));

	e = GF_OK;

	camStopCamera(read);

	gf_service_disconnect_ack(read->service, channel, e);
	return e;
}

void Java_com_gpac_Osmo4_Preview_processFrameBuf( JNIEnv* env, jobject thiz, jbyteArray arr)
{
	ISOMReader* ctx = globReader;
	if ( ctx
	 	&& ctx->started
	 	&& ctx->term
	 	&& ctx->term->compositor
	 	&& ctx->term->compositor->audio_renderer
	) {

		u8* data;
		u32 datasize;
		GF_SLHeader hdr;
		u32 cts = 0;
		//u32 convTime = 0;
		//u32 j = 0;
		jbyte *jdata;
		jsize len;

		len = (*env)->GetArrayLength(env, arr);
		if ( len <= 0 ) return;
		jdata = (*env)->GetByteArrayElements(env,arr,0);

		//convTime = gf_term_get_time(ctx->term);

		data = (u8*)jdata;//(u8*)decodeYUV420SP((char*)jdata, ctx->width, ctx->height); //
		datasize = len;//ctx->width * ctx->height * CAM_PIXEL_SIZE;//

		cts = gf_term_get_time(ctx->term);

		//convTime = cts - convTime;

		memset(&hdr, 0, sizeof(hdr));
		hdr.compositionTimeStampFlag = 1;
		hdr.compositionTimeStamp = cts;
		gf_service_send_packet(ctx->service, ctx->channel, (void*)data, datasize, &hdr, GF_OK);

		//gf_free(data);

		(*env)->ReleaseByteArrayElements(env,arr,jdata,JNI_ABORT);

		//GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Camera Frame Sent %d\n", gf_sys_clock()));
	}
}

void CallCamMethod(ISOMReader *read, jmethodID methodID)
{
	JNIEnv* env = NULL;
	jint res = 0;
	u8 isAttached = 0;

	// Get the JNI interface pointer
	res = (*GetJavaVM())->GetEnv(javaVM, (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED )
	{
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] The current thread is not attached to the VM, assuming native thread\n"));
		res = (*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
		if ( res ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[ANDROID_CAMERA] Attach current thread failed: %d\n", res));
			return;
		}
		if ( env != read->env )
			isAttached = 1;
	}

	(*env)->CallNonvirtualVoidMethod(env, read->camCtrlObj, read->camCtrlClass, methodID);

	if (isAttached)
	{
		(*GetJavaVM())->DetachCurrentThread(GetJavaVM());
	}
}

void camStartCamera(ISOMReader *read)
{
	JNIEnv* env;
	jboolean isPortrait = JNI_FALSE;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] startCamera: %d\n", gf_th_id()));

	// Get the JNI interface pointer
	env = read->env;

	(*env)->CallNonvirtualBooleanMethod(env, read->camCtrlObj, read->camCtrlClass, read->startCamera, isPortrait);
}

void camStopCamera(ISOMReader *read)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] stopCamera: %d\n", gf_th_id()));

	CallCamMethod(read, read->stopCamera);
}

void pauseCamera(ISOMReader *read)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] pauseCamera: %d\n", gf_th_id()));

	read->started = 0;
	CallCamMethod(read, read->stopProcessing);
}

void resumeCamera(ISOMReader *read)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] resumeCamera: %d\n", gf_th_id()));

	read->started = 1;
	CallCamMethod(read, read->startProcessing);
}

u32 getWidth(ISOMReader *read)
{
	JNIEnv* env;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] getWidth: %d\n", gf_th_id()));

	// Get the JNI interface pointer
	env = read->env;

	return (*env)->CallNonvirtualIntMethod(env, read->camCtrlObj, read->camCtrlClass, read->getImageWidth);
}

u32 getHeight(ISOMReader *read)
{
	JNIEnv* env;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] getHeight: %d\n", gf_th_id()));

	// Get the JNI interface pointer
	env = read->env;

	return (*env)->CallNonvirtualIntMethod(env, read->camCtrlObj, read->camCtrlClass, read->getImageHeight);
}

u32 getFormat(ISOMReader *read)
{
	JNIEnv* env;
	u32 pixel_format;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] getFormat: %d\n", gf_th_id()));

	// Get the JNI interface pointer
	env = read->env;

	pixel_format = (*env)->CallNonvirtualIntMethod(env, read->camCtrlObj, read->camCtrlClass, read->getImageFormat);
	switch (pixel_format) {
	case 17: //NV21
		return GF_PIXEL_NV21;
	case 842094169: //YV12
		return GF_PIXEL_YV12;
	default:
		return 0; //should never be here
	}
}

u32 getBitsPerPix(ISOMReader *read)
{
	JNIEnv* env;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[ANDROID_CAMERA] getBitsPerPix: %d\n", gf_th_id()));

	// Get the JNI interface pointer
	env = read->env;

	return (*env)->CallNonvirtualIntMethod(env, read->camCtrlObj, read->camCtrlClass, read->getBitsPerPixel);
}

GF_Err CAM_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	ISOMReader *read;
	if (!plug || !plug->priv || !com) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;

	if (com->command_type==GF_NET_SERVICE_INFO) {
		return GF_OK;
	}
	if (com->command_type==GF_NET_SERVICE_HAS_AUDIO) {
		return GF_NOT_SUPPORTED;
	}
	if (!com->base.on_channel) return GF_NOT_SUPPORTED;

	switch (com->command_type) {
	case GF_NET_CHAN_INTERACTIVE:
		return GF_OK;
	case GF_NET_CHAN_BUFFER:
		com->buffer.max = com->buffer.min = 0;
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		resumeCamera(read);
		return GF_OK;
	case GF_NET_CHAN_STOP:
		pauseCamera(read);
		return GF_OK;
	/*nothing to do on MP4 for channel config*/
	case GF_NET_CHAN_CONFIG:
		return GF_OK;
	default:
		break;
	}
	return GF_NOT_SUPPORTED;
}

GF_InputService *CAM_client_load()
{
	ISOMReader *reader;
	GF_InputService *plug;
	GF_SAFEALLOC(plug, GF_InputService);
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC Camera Plugin", "gpac distribution")
	plug->CanHandleURL = CAM_CanHandleURL;
	plug->ConnectService = CAM_ConnectService;
	plug->CloseService = CAM_CloseService;
	plug->GetServiceDescriptor = CAM_GetServiceDesc;
	plug->ConnectChannel = CAM_ConnectChannel;
	plug->DisconnectChannel = CAM_DisconnectChannel;
	plug->ServiceCommand = CAM_ServiceCommand;

	GF_SAFEALLOC(reader, ISOMReader);
	plug->priv = reader;
	globReader = reader;


#ifdef GPAC_STATIC_MODULES
	static_JNI_OnLoad(GetJavaVM(), NULL);
#endif

	return plug;
}

void CAM_client_del(GF_BaseInterface *bi)
{
	GF_InputService *plug = (GF_InputService *) bi;
	ISOMReader *read = (ISOMReader *)plug->priv;

	gf_free(read);
	gf_free(bi);
}

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_NET_CLIENT_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_NET_CLIENT_INTERFACE)
		return (GF_BaseInterface *)CAM_client_load();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_NET_CLIENT_INTERFACE:
		CAM_client_del(ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( droid_cam )
