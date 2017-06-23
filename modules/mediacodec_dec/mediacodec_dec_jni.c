/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
#ifdef GPAC_USE_GLES2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include <GLES/gl.h>
#include <GLES/glext.h>
#endif
#include "mediacodec_dec.h"

static jclass cSurfaceTexture = NULL;
static jclass cSurface = NULL;
static jobject oSurfaceTex = NULL;
static jobject oSurface = NULL;
static jmethodID mUpdateTexImage;
static jmethodID mSurfaceTexConstructor;
static jmethodID mSurfaceConstructor;
static jmethodID mSurfaceRelease;
static jmethodID mGetTransformMatrix;
static JavaVM* javaVM = 0;

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	javaVM = vm;
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

GF_Err MCDec_CreateSurface (ANativeWindow ** window, u32 *gl_tex_id, Bool * surface_rendering)
{
	JNIEnv* env = NULL;
	jobject otmp= NULL;
	jclass ctmp = NULL;
	jint res = 0;
	
	res = (*GetJavaVM())->GetEnv(GetJavaVM(), (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED ) {
		(*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
	}
	if (!env) return GF_BAD_PARAM;
	
	// cache classes
	if (!cSurfaceTexture) {
		ctmp = (*env)->FindClass(env, "android/graphics/SurfaceTexture");
		cSurfaceTexture = (*env)->NewGlobalRef(env, ctmp);
		if(!cSurfaceTexture) GF_BAD_PARAM;
	}
	if (!cSurface) {
		ctmp = (*env)->FindClass(env, "android/view/Surface");
		cSurface = (*env)->NewGlobalRef(env, ctmp);
		if (!cSurface) return GF_BAD_PARAM;
	}
	
	/* TOFIX : here we call glGenTextures without creating or getting opengl context*/
	//glGenTextures(1, gl_tex_id); The texture is created by the HW
	
	// cache methods
	if(!mSurfaceTexConstructor) {
		mSurfaceTexConstructor = (*env)->GetMethodID(env, cSurfaceTexture, "<init>", "(I)V");
		if(!mSurfaceTexConstructor) return GF_BAD_PARAM;
	}
	if(!mSurfaceConstructor) {
		mSurfaceConstructor = (*env)->GetMethodID(env, cSurface, "<init>", "(Landroid/graphics/SurfaceTexture;)V");
		if(!mSurfaceConstructor) return GF_BAD_PARAM;
	}
	if(!mSurfaceRelease) {
		mSurfaceRelease = (*env)->GetMethodID(env, cSurface, "release", "()V");
		if(!mSurfaceRelease) return GF_BAD_PARAM;
	}
	if(!mUpdateTexImage) {
		mUpdateTexImage = (*env)->GetMethodID(env, cSurfaceTexture, "updateTexImage", "()V");
		if(!mUpdateTexImage) return GF_BAD_PARAM;
	}
	if(!mGetTransformMatrix) {
		mGetTransformMatrix = (*env)->GetMethodID(env, cSurfaceTexture, "getTransformMatrix", "([F)V");
		if(!mGetTransformMatrix) return GF_BAD_PARAM;
	}
	
	// create objects
	otmp = (*env)->NewObject(env, cSurfaceTexture, mSurfaceTexConstructor, *gl_tex_id);
	oSurfaceTex = (jobject) (*env)->NewGlobalRef(env,otmp);
	if(!oSurfaceTex) return GF_BAD_PARAM;
	
	oSurface = (*env)->NewObject(env, cSurface, mSurfaceConstructor, oSurfaceTex);
	if(!oSurface) return GF_BAD_PARAM;
	*window = ANativeWindow_fromSurface(env, oSurface);
	*surface_rendering = (*window) ? GF_TRUE : GF_FALSE;
	(*env)->CallVoidMethod(env, oSurface, mSurfaceRelease);
	
	return GF_OK;
}

GF_Err MCFrame_UpdateTexImage()
{
	JNIEnv* env = NULL;
	jint res = 0;
	res = (*GetJavaVM())->GetEnv(GetJavaVM(), (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED ) {
		(*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
	}
	if (!env) return GF_BAD_PARAM;
	
	if(oSurfaceTex) {
		(*env)->CallVoidMethod(env, oSurfaceTex, mUpdateTexImage);
	}
	return GF_OK;
}
GF_Err MCFrame_GetTransformMatrix(GF_CodecMatrix * mx)
{
	JNIEnv* env = NULL;
	jint res = 0;
	int i =0;
	jfloatArray texMx;
	
	res = (*GetJavaVM())->GetEnv(GetJavaVM(), (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED ) {
		(*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
	}
	if (!env) return GF_BAD_PARAM;
	
	if(oSurfaceTex) {
		texMx = (*env)->NewFloatArray(env,16);
		if (texMx == NULL) {
			return GF_BAD_PARAM; /* out of memory error thrown */
		}
		
		(*env)->CallVoidMethod(env, oSurfaceTex, mGetTransformMatrix,texMx);
		
		jsize len = (*env)->GetArrayLength(env, texMx);
		
		jfloat *body = (*env)->GetFloatArrayElements(env, texMx, 0);
		
		for (i=0; i<len; i++) {
			mx->m[i] = FLT2FIX(body[i]);
		}
		
		(*env)->ReleaseFloatArrayElements(env, texMx, body, 0);
		
	}
	return GF_OK;
}

GF_Err MCDec_DeleteSurface()
{
	JNIEnv* env = NULL;
	jint res = 0;
	
	res = (*GetJavaVM())->GetEnv(GetJavaVM(), (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED ) {
		(*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
	} 
	if (!env) return GF_BAD_PARAM;
	(*env)->DeleteGlobalRef(env,cSurface);
	cSurface = NULL;
	(*env)->DeleteGlobalRef(env,cSurfaceTexture);
	cSurfaceTexture = NULL;
	(*env)->DeleteGlobalRef(env,oSurfaceTex);
	oSurfaceTex = NULL;
	
	return GF_OK;
}

