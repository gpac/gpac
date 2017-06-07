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
#include <gpac/thread.h>
// for native window JNI
#include <android/native_window_jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include "mediacodec_dec.h"
#define RANDOM_JAVA_APP_CLASS "com/gpac/Osmo4/GPACInstance"

static jclass cSurfaceTexture = NULL;
static jclass cSurface = NULL;
static jmethodID mUpdateTexImage;
static jmethodID mSurfaceTexConstructor;
static jmethodID mSurfaceConstructor;
static jmethodID mSurfaceRelease;
static jmethodID mGetTransformMatrix;
static jmethodID mFindClassMethod;
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

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	JNIEnv* env = NULL;
	jint res = 0;
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

static u32 beforeThreadExits(void * param) {

	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC,(" [Android Mediacodec decoder] Detach decoder thread %p...\n", gf_th_current()));
	(*GetJavaVM())->DetachCurrentThread(GetJavaVM());
	
	return GF_OK;
}

GF_Err MCDec_CreateSurface (GLuint tex_id, ANativeWindow ** window, Bool * surface_rendering, MC_SurfaceTexture * surfaceTex)
{
	JNIEnv* env = NULL;
	jobject otmp= NULL;
	jclass ctmp = NULL;
	jint res = 0;
	jobject oSurface = NULL;
	GF_Err ret = GF_BAD_PARAM;
	u32 gl_tex_id;
	
	res = (*GetJavaVM())->GetEnv(GetJavaVM(), (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED ) {
		(*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
	}
	if (!env) goto create_surface_failed;
	
	if (gf_register_before_exit_function(gf_th_current(), &beforeThreadExits) != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("Failed to register exit function for the decoder thread %p, try to continue anyway...\n", gf_th_current()));
	}
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

GF_Err MCFrame_UpdateTexImage(MC_SurfaceTexture surfaceTex)
{
	JNIEnv* env = NULL;
	jint res = 0;
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

GF_Err MCFrame_GetTransformMatrix(GF_CodecMatrix * mx, MC_SurfaceTexture surfaceTex)
{
	JNIEnv* env = NULL;
	jint res = 0;
	int i =0;
	jfloatArray texMx;
	jsize len = 0;
	jfloat *body;
	GF_Err ret = GF_BAD_PARAM;
	
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

GF_Err MCDec_DeleteSurface(MC_SurfaceTexture  surfaceTex)
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
	(*env)->DeleteGlobalRef(env, surfaceTex.oSurfaceTex);
	surfaceTex.oSurfaceTex = NULL;
	
	return GF_OK;
}

