/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / codec pack module
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
 
#include <gpac/tools.h>
#include <gpac/constants.h>

// for native window JNI
#include <android/native_window_jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <jni.h>

#include "dec_mediacodec.h"

#define ALL_CODECS 1

static jclass cSurfaceTexture = NULL;
static jclass cSurface = NULL;
static jclass cMediaCodecList = NULL;
static jclass cMediaFormat = NULL;
static jmethodID mUpdateTexImage;
static jmethodID mSurfaceTexConstructor;
static jmethodID mSurfaceConstructor;
static jmethodID mSurfaceRelease;
static jmethodID mGetTransformMatrix;
static jmethodID mFindClassMethod;
static jmethodID mMediaCodecListConstructor;
static jmethodID mFindDecoderForFormat;
static jmethodID mCreateVideoFormat;
static jmethodID mSetFeatureEnabled;
static jobject oClassLoader = NULL;
static JavaVM* javaVM = 0;

static void HandleJNIError(JNIEnv *env)
{
	(*env)->ExceptionDescribe(env);
	(*env)->ExceptionClear(env);
}
static jclass FindAppClass(JNIEnv* env, const char* name)
{

	return (jclass)((*env)->CallObjectMethod(env, oClassLoader, mFindClassMethod, (*env)->NewStringUTF(env, name)));
}

static GF_Err CacheAppClassLoader(JNIEnv* env, const char* name)
{
	GF_Err ret = GF_BAD_PARAM;
	jclass randomClass = (*env)->FindClass(env, name);
	if (!randomClass) goto cache_calassloader_failed;
	jclass classClass = (*env)->GetObjectClass(env, randomClass);
	if (!classClass) goto cache_calassloader_failed;
    	jclass classLoaderClass = (*env)->FindClass(env, "java/lang/ClassLoader");
	if (!classLoaderClass) goto cache_calassloader_failed;
    	jmethodID getClassLoaderMethod = (*env)->GetMethodID(env, classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
	if (!getClassLoaderMethod) goto cache_calassloader_failed;
	oClassLoader = (jobject) (*env)->NewGlobalRef(env,(*env)->CallObjectMethod(env,randomClass, getClassLoaderMethod));
	if(!oClassLoader) goto cache_calassloader_failed;
    	mFindClassMethod = (*env)->GetMethodID(env, classLoaderClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
	if(!mFindClassMethod) goto cache_calassloader_failed;
	ret = GF_OK;

cache_calassloader_failed:
	if(randomClass)
		 (*env)->DeleteLocalRef(env, randomClass);
	 if(classClass)
		 (*env)->DeleteLocalRef(env, classClass);
	 if(classLoaderClass)
		 (*env)->DeleteLocalRef(env, classLoaderClass);
	return ret;
}

#ifndef GPAC_STATIC_MODULES

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	JNIEnv* env = NULL;
	javaVM = vm;
	
	if ((*javaVM)->GetEnv(javaVM, (void**)&env, JNI_VERSION_1_2) != JNI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("GetEnv failed"));
		return GF_BAD_PARAM;
	}
	return JNI_VERSION_1_2;
}

JNIEnv* GetEnv()
{
	JNIEnv* env = 0;
	if (javaVM) (*javaVM)->GetEnv(javaVM, (void**)&env, JNI_VERSION_1_2);
	return env;
}

JavaVM* GetJavaVM()
{
	return javaVM;
}

#else

//if static modules, these are defined in main wrapper
JavaVM* GetJavaVM();
JNIEnv* GetEnv();

#endif


u32 mcdec_exit_callback(void * param) {

	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[MCDec] Detach decoder thread\n"));
	(*GetJavaVM())->DetachCurrentThread(GetJavaVM());
	return GF_OK;
}

GF_Err mcdec_create_surface(GLuint tex_id, ANativeWindow ** window, Bool * surface_rendering, GF_MCDecSurfaceTexture * surfaceTex)
{
	JNIEnv* env = NULL;
	jobject otmp= NULL;
	jclass ctmp = NULL;
	jint res = 0;
	jobject oSurface = NULL;
	GF_Err ret = GF_BAD_PARAM;
	
	res = (*GetJavaVM())->GetEnv(GetJavaVM(), (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED ) {
		(*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
	}
	if (!env) goto create_surface_failed;
	
	// cache classes
	if (!cSurfaceTexture) {
		ctmp = (*env)->FindClass(env, "android/graphics/SurfaceTexture");
		cSurfaceTexture = (*env)->NewGlobalRef(env, ctmp);
		if(!cSurfaceTexture) goto create_surface_failed;
	}
	if (!cSurface) {
		ctmp = (*env)->FindClass(env, "android/view/Surface");
		cSurface = (*env)->NewGlobalRef(env, ctmp);
		if (!cSurface) goto create_surface_failed;
	}
	
	// cache methods
	if(!mSurfaceTexConstructor) {
		mSurfaceTexConstructor = (*env)->GetMethodID(env, cSurfaceTexture, "<init>", "(I)V");
		if(!mSurfaceTexConstructor) goto create_surface_failed;
	}
	if(!mSurfaceConstructor) {
		mSurfaceConstructor = (*env)->GetMethodID(env, cSurface, "<init>", "(Landroid/graphics/SurfaceTexture;)V");
		if(!mSurfaceConstructor) goto create_surface_failed;
	}
	if(!mSurfaceRelease) {
		mSurfaceRelease = (*env)->GetMethodID(env, cSurface, "release", "()V");
		if(!mSurfaceRelease) goto create_surface_failed;
	}
	if(!mUpdateTexImage) {
		mUpdateTexImage = (*env)->GetMethodID(env, cSurfaceTexture, "updateTexImage", "()V");
		if(!mUpdateTexImage) goto create_surface_failed;
	}
	if(!mGetTransformMatrix) {
		mGetTransformMatrix = (*env)->GetMethodID(env, cSurfaceTexture, "getTransformMatrix", "([F)V");
		if(!mGetTransformMatrix) goto create_surface_failed;
	}
	
	surfaceTex->texture_id = tex_id;
	// create objects
	otmp = (*env)->NewObject(env, cSurfaceTexture, mSurfaceTexConstructor, surfaceTex->texture_id);
	surfaceTex->oSurfaceTex = (jobject) (*env)->NewGlobalRef(env,otmp);
	if(!surfaceTex->oSurfaceTex) goto create_surface_failed;
	
	oSurface = (*env)->NewObject(env, cSurface, mSurfaceConstructor, surfaceTex->oSurfaceTex);
	if(!oSurface) goto create_surface_failed;
	
	*window = ANativeWindow_fromSurface(env, oSurface);
	*surface_rendering = (*window) ? GF_TRUE : GF_FALSE;
	(*env)->CallVoidMethod(env, oSurface, mSurfaceRelease);
	if ((*env)->ExceptionCheck(env)) {
		HandleJNIError(env);
	}
	ret = GF_OK;
	
create_surface_failed:
	if(otmp)
		 (*env)->DeleteLocalRef(env, otmp);
	 if(ctmp)
		 (*env)->DeleteLocalRef(env, ctmp);
	 if(oSurface)
		 (*env)->DeleteLocalRef(env, oSurface);
	 
	 return ret;
}

static char * mcdec_get_decoder_name(JNIEnv * env, jobject mediaCodecList, jmethodID findId, jobject mediaFomat)
{
	jint res = 0;
	jstring jdecoder_name;
	char * decoder_name = NULL;

	jdecoder_name = (*env)->CallObjectMethod(env, mediaCodecList, findId, mediaFomat);
	if ((*env)->ExceptionCheck(env)) {
		HandleJNIError(env);
		goto get_decoder_exit;
	}
	if(jdecoder_name) {
		const char *decoder_name_tmp;
		decoder_name_tmp = (*env)->GetStringUTFChars(env, jdecoder_name, 0);
		decoder_name = gf_strdup(decoder_name_tmp);
		(*env)->ReleaseStringUTFChars(env, jdecoder_name, decoder_name_tmp);
	}

get_decoder_exit:
	if(jdecoder_name)
		(*env)->DeleteLocalRef(env, jdecoder_name);

	return decoder_name;
}

char * mcdec_find_decoder(const char * mime, u32 width, u32 height,  Bool * is_adaptive)
{
	JNIEnv* env = NULL;
	jobject oMediaCodecList = NULL, oMediaFormat = NULL;
	jclass ctmp = NULL;
	jint res = 0;
	jstring jmime, jfeature;
	char *decoder_name = NULL, *decoder_name_tmp;


	if(!mime || !is_adaptive) goto find_decoder_exit;

	res = (*GetJavaVM())->GetEnv(GetJavaVM(), (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED ) {
		(*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
	}
	if (!env) goto find_decoder_exit;

	// cache classes
	if (!cMediaCodecList) {
		ctmp = (*env)->FindClass(env, "android/media/MediaCodecList");
		cMediaCodecList = (*env)->NewGlobalRef(env, ctmp);
		if(!cMediaCodecList) goto find_decoder_exit;
	}
	if (!cMediaFormat) {
		ctmp = (*env)->FindClass(env, "android/media/MediaFormat");
		cMediaFormat = (*env)->NewGlobalRef(env, ctmp);
		if(!cMediaFormat) goto find_decoder_exit;
	}

	//methods
	if(!mMediaCodecListConstructor) {
		mMediaCodecListConstructor = (*env)->GetMethodID(env, cMediaCodecList, "<init>", "(I)V");
		if(!mMediaCodecListConstructor) goto find_decoder_exit;
	}
	if(!mFindDecoderForFormat) {
		mFindDecoderForFormat = (*env)->GetMethodID(env, cMediaCodecList, "findDecoderForFormat", "(Landroid/media/MediaFormat;)Ljava/lang/String;");
		if(!mFindDecoderForFormat) goto find_decoder_exit;
	}
	if(!mCreateVideoFormat) {
		mCreateVideoFormat = (*env)->GetStaticMethodID(env, cMediaFormat, "createVideoFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;");
		if(!mCreateVideoFormat) goto find_decoder_exit;
	}
	if(!mSetFeatureEnabled ) {
		mSetFeatureEnabled = (*env)->GetMethodID(env, cMediaFormat, "setFeatureEnabled", "(Ljava/lang/String;Z)V");
		if(!mSetFeatureEnabled) goto find_decoder_exit;
	}
	oMediaCodecList = (*env)->NewObject(env, cMediaCodecList, mMediaCodecListConstructor, ALL_CODECS);
	if(!oMediaCodecList) goto find_decoder_exit;

	jmime = (*env)->NewStringUTF(env, mime);
	oMediaFormat = (*env)->CallStaticObjectMethod(env, cMediaFormat, mCreateVideoFormat, jmime, width, height);
	if ((*env)->ExceptionCheck(env)) {
		HandleJNIError(env);
		goto find_decoder_exit;
	}
	decoder_name_tmp = mcdec_get_decoder_name(env, oMediaCodecList, mFindDecoderForFormat, oMediaFormat);
	if(!decoder_name_tmp) goto find_decoder_exit;

	jfeature = (*env)->NewStringUTF(env, "adaptive-playback");
	(*env)->CallVoidMethod(env, oMediaFormat, mSetFeatureEnabled, jfeature, JNI_TRUE);
	if ((*env)->ExceptionCheck(env)) {
		HandleJNIError(env);
		goto find_decoder_exit;
	}
	decoder_name = mcdec_get_decoder_name(env, oMediaCodecList, mFindDecoderForFormat, oMediaFormat);
	if(decoder_name){
		*is_adaptive = GF_TRUE;
		gf_free(decoder_name_tmp);
		decoder_name_tmp = NULL;
	} else {
		*is_adaptive = GF_FALSE;
		decoder_name = decoder_name_tmp;
	}

find_decoder_exit:
	if(ctmp)
		(*env)->DeleteLocalRef(env, ctmp);
	if(oMediaCodecList)
		(*env)->DeleteLocalRef(env, oMediaCodecList);
	if(oMediaFormat)
		(*env)->DeleteLocalRef(env, oMediaFormat);
	if(jmime)
		(*env)->DeleteLocalRef(env, jmime);
	if(jfeature)
		(*env)->DeleteLocalRef(env, jfeature);

	return decoder_name;

}

GF_Err mcdec_update_surface(GF_MCDecSurfaceTexture surfaceTex)
{
	JNIEnv* env = NULL;
	jint res = 0;
	//we must check this, as the calling thread is not always the decoder thread
	res = (*GetJavaVM())->GetEnv(GetJavaVM(), (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED ) {
		(*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
	}
	if (!env) return GF_BAD_PARAM;
	
	(*env)->CallVoidMethod(env, surfaceTex.oSurfaceTex, mUpdateTexImage);
	if ((*env)->ExceptionCheck(env)) {
		HandleJNIError(env);
		return GF_BAD_PARAM;
	}
	return GF_OK;
}

GF_Err mcdec_get_transform_matrix(GF_Matrix * mx, GF_MCDecSurfaceTexture surfaceTex)
{
	JNIEnv* env = NULL;
	jint res = 0;
	int i =0;
	jfloatArray texMx;
	jsize len = 0;
	jfloat *body;
	GF_Err ret = GF_BAD_PARAM;
	
	//we must check this, as the calling thread is not always the decoder thread
	res = (*GetJavaVM())->GetEnv(GetJavaVM(), (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED ) {
		(*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
	}
	if (!env) goto get_matrix_failed;
	
	texMx = (*env)->NewFloatArray(env, 16);
	if (texMx == NULL) goto get_matrix_failed;
	
	(*env)->CallVoidMethod(env, surfaceTex.oSurfaceTex, mGetTransformMatrix,texMx);
	if ((*env)->ExceptionCheck(env)) {
		HandleJNIError(env);
		goto get_matrix_failed;
	}
	len = (*env)->GetArrayLength(env, texMx);
	body = (*env)->GetFloatArrayElements(env, texMx, 0);
	if ((*env)->ExceptionCheck(env)) {
		HandleJNIError(env);
		goto get_matrix_failed;
	}
	for (i=0; i<len; i++) {
		mx->m[i] = FLT2FIX(body[i]);
	}
	
	(*env)->ReleaseFloatArrayElements(env, texMx, body, 0);
	if ((*env)->ExceptionCheck(env)) {
		HandleJNIError(env);
		goto get_matrix_failed;
	}
	ret = GF_OK;
	
get_matrix_failed:
	if(texMx)
		(*env)->DeleteLocalRef(env, texMx);
	return ret;
}

GF_Err mcdec_delete_surface(GF_MCDecSurfaceTexture  surfaceTex)
{
	JNIEnv* env = NULL;
	jint res = 0;
	
	res = (*GetJavaVM())->GetEnv(GetJavaVM(), (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED ) {
		(*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
	} 
	if (!env) return GF_BAD_PARAM;
	
	(*env)->DeleteGlobalRef(env, cSurface);
	cSurface = NULL;
	(*env)->DeleteGlobalRef(env, cSurfaceTexture);
	cSurfaceTexture = NULL;
	(*env)->DeleteGlobalRef(env, cMediaCodecList);
	cMediaCodecList = NULL;
	(*env)->DeleteGlobalRef(env, cMediaFormat);
	cMediaFormat = NULL;
	(*env)->DeleteGlobalRef(env, surfaceTex.oSurfaceTex);
	surfaceTex.oSurfaceTex = NULL;

	if ( res == JNI_EDETACHED ) {
		(*GetJavaVM())->DetachCurrentThread(GetJavaVM() );
	}

	return GF_OK;
}

