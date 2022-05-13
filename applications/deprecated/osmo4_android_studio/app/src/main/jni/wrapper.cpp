/*
 *			GPAC - Multimedia Framework C SDK
 *
 *				Authors: Jean Le Feuvre
 *				Copyright (c) Telecom ParisTech 2009-
 *					All rights reserved
 *
 *	Created by NGO Van Luyen, Ivica ARSOV / ARTEMIS / Telecom SudParis /Institut TELECOM on Oct, 2010
 *
 *  This file is part of GPAC / Wrapper
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

#include <jni.h>

#include <gpac/terminal.h>
#include <gpac/thread.h>
#include <gpac/options.h>
#include <gpac/network.h>
//#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>

#include "wrapper.h"
#include "wrapper_jni.hpp"

#include <math.h>
#include <android/log.h>

#define TAG "GPAC_WRAPPER"

#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG,  __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, TAG,  __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, TAG,  __VA_ARGS__)
#define LOGW(...)  __android_log_print(ANDROID_LOG_WARN, TAG,  __VA_ARGS__)
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, TAG,  __VA_ARGS__)
#include <pthread.h>

static JavaVM* javaVM = NULL;

static pthread_key_t jni_thread_env_key = 0;

struct _tag_terminal
{
	GF_User *user;
	GF_Compositor *compositor;
	GF_FilterSession *fsess;
};

//these two are used by modumles - define them when using static module build
#ifdef GPAC_STATIC_MODULES

#ifdef __cplusplus
extern "C" {
#endif

JavaVM* GetJavaVM()
{
	return javaVM;
}

JNIEnv* GetEnv()
{
	JNIEnv* env = 0;
	if (javaVM) javaVM->GetEnv((void**)(&env), JNI_VERSION_1_2);
	return env;
}

#ifdef __cplusplus
}
#endif

#endif


/**
 * This method is called when a pthread is destroyed, so we can delete the JNI env
 */
static void jni_destroy_env_func(void * env) {
	LOGI("Destroying a thread with attached data, env=%p.\n", env);
	JavaEnvTh * jniEnv = (JavaEnvTh *) env;
	if (jniEnv) {
		/*jniEnv->env->DeleteLocalRef(&jniEnv->cbk_displayMessage);
		jniEnv->env->DeleteLocalRef(&jniEnv->cbk_onProgress);
		jniEnv->env->DeleteLocalRef(&jniEnv->cbk_showKeyboard);
		jniEnv->env->DeleteLocalRef(&jniEnv->cbk_setCaption);
		jniEnv->env->DeleteLocalRef(&jniEnv->cbk_onLog);*/
		memset(jniEnv, 0, sizeof(JavaEnvTh));
		gf_free(jniEnv);
	}
	pthread_setspecific(jni_thread_env_key, NULL);
	if (javaVM)
		javaVM->DetachCurrentThread();
}

static int jniRegisterNativeMethods(JNIEnv* env, const char* className,
                                    const JNINativeMethod* gMethods, int numMethods)
{
	int res;
	jclass clazz;
	clazz = env->FindClass(className);
	if (clazz == NULL) {
		LOGE("Native registration unable to find class '%s'\n", className);
		return -1;
	}
	LOGI("Registering %d methods...\n", numMethods );
	res = env->RegisterNatives(clazz, gMethods, numMethods);
	if (res < 0) {
		LOGE("RegisterNatives failed for '%s'\n", className);
		return res;
	}
	return 0;
}

static JNINativeMethod sMethods[] = {
	/* name, signature, funcPtr */

	{	"createInstance",
		"(Lcom/gpac/Osmo4/GpacCallback;IILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)J",
		(void*)&Java_com_gpac_Osmo4_GPACInstance_createInstance
	},
	{	"gpacdisconnect",
		"()V",
		(void*)Java_com_gpac_Osmo4_GPACInstance_gpacdisconnect
	},
	{	"gpacrender",
		"()V",
		(void*)Java_com_gpac_Osmo4_GPACInstance_gpacrender
	},
	{	"gpacresize",
		"(II)V",
		(void*)Java_com_gpac_Osmo4_GPACInstance_gpacresize
	},
	{	"gpacfree",
		"()V",
		(void*)Java_com_gpac_Osmo4_GPACInstance_gpacfree
	},
	{	"gpaceventkeypress",
		"(IIIII)V",
		(void*)Java_com_gpac_Osmo4_GPACInstance_gpaceventkeypress
	},
	{	"gpaceventmousedown",
		"(FF)V",
		(void*)Java_com_gpac_Osmo4_GPACInstance_gpaceventmousedown
	},
	{	"gpaceventmouseup",
		"(FF)V",
		(void*)Java_com_gpac_Osmo4_GPACInstance_gpaceventmouseup
	},
	{	"gpaceventmousemove",
		"(FF)V",
		(void*)Java_com_gpac_Osmo4_GPACInstance_gpaceventmousemove
	},
	{	"gpaceventorientationchange",
		"(FFF)V",
		(void*)Java_com_gpac_Osmo4_GPACInstance_gpaceventorientationchange
	},
	{	"setGpacPreference",
		"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
		(void*)Java_com_gpac_Osmo4_GPACInstance_setGpacPreference
	},
	{	"setGpacLogs",
		"(Ljava/lang/String;)V",
		(void*)Java_com_gpac_Osmo4_GPACInstance_setGpacLogs
	},
	NULL
};


jint JNI_OnUnLoad(JavaVM* vm, void* reserved) {
	LOGI("Deleting library, vm=%p...\n", vm);
	if (pthread_key_delete(jni_thread_env_key)) {
		LOGW("Failed to delete key jni_thread_env_key jni_thread_env_key=%d\n", jni_thread_env_key);
	}
	javaVM = NULL;
	jni_thread_env_key = (int) NULL;
	return 0;
}

#define NUM_JNI_VERSIONS 4
//---------------------------------------------------------------------------------------------------
jint JNI_OnLoad(JavaVM* vm, void* reserved) {
	int res;
	int jniV;
	int allowedVersions[NUM_JNI_VERSIONS];
	const char * className = "com/gpac/Osmo4/GPACInstance";
	allowedVersions[0] = JNI_VERSION_1_6;
	allowedVersions[1] = JNI_VERSION_1_4;
	allowedVersions[2] = JNI_VERSION_1_2;
	allowedVersions[3] = JNI_VERSION_1_1;
	JNIEnv * env = NULL;
	if (!vm)
		return -1;
	for (int i = 0 ; i < NUM_JNI_VERSIONS; i++) {
		jniV = allowedVersions[i];
		if (vm->GetEnv((void**)(&env), jniV) == JNI_OK) {
			LOGI("Selected JNI VERSION[%d]\n", i);
			break;
		} else {
			if (i == NUM_JNI_VERSIONS - 1) {
				LOGW("Failed to find any supported JNI VERSION of the %d proposed.", NUM_JNI_VERSIONS);
				return -1;
			}
		}

	}
	if (! env) {
		LOGW("Failed to find any supported JNI VERSION");
		return -1;
	}
	javaVM = vm;
	LOGI("Registering %s natives\n", className);
	res = jniRegisterNativeMethods(env, className, sMethods, (sizeof(sMethods) - 1)/sizeof(JNINativeMethod));
	if (res < 0) {
		LOGE("Failed to register native methods, result was = %d, try to continue anyway...\n", res);
		/*return -1;*/
	}
	LOGI("Registering natives DONE, now registering pthread_keys with destructor=%p\n", &jni_destroy_env_func);
	int ret = pthread_key_create(&jni_thread_env_key, &jni_destroy_env_func);
	if (ret) {
		LOGE("Failed to register jni_thread_env_key jni_thread_env_key=%d\n", jni_thread_env_key);
	}
	return jniV;
}

//---------------------------------------------------------------------------------------------------
//CNativeWrapper
//-------------------------------

CNativeWrapper::CNativeWrapper() {
	do_log = 1;
	m_term = NULL;
	m_mx = NULL;
	mainJavaEnv = NULL;

	log_file = NULL;
	m_window = NULL;
	m_session = NULL;

	m_cfg_dir[0]=0;
	m_modules_dir[0]=0;
	m_cache_dir[0]=0;
	m_font_dir[0]=0;

#ifndef GPAC_GUI_ONLY
	memset(&m_user, 0, sizeof(GF_User));
	memset(&m_rti, 0, sizeof(GF_SystemRTInfo));
#endif
}
//-------------------------------
CNativeWrapper::~CNativeWrapper() {
	debug_log("~CNativeWrapper()");
	JavaEnvTh * env = getEnv();
	if (env && env->cbk_obj)
		env->env->DeleteGlobalRef(env->cbk_obj);
	if (log_file) gf_fclose(log_file);
	log_file = NULL;
	Shutdown();
	debug_log("~CNativeWrapper() : DONE\n");
}
//-------------------------------
void CNativeWrapper::debug_log(const char* msg) {
	LOGV("%s", msg);
}
//-------------------------------
void CNativeWrapper::Shutdown()
{
	debug_log("shutdown");
	if (m_term)
		gf_term_disconnect(m_term);
	if (m_mx)
		gf_mx_del(m_mx);
	m_mx = NULL;
#ifndef GPAC_GUI_ONLY
	if (m_term) {
		GF_Terminal *t = m_term;
		m_term = NULL;
		gf_term_del(t);
	}
	/*
	if (m_user.config) {
		gf_cfg_del(m_user.config);
		m_user.config = NULL;
	}
	if (m_user.modules) {
		gf_modules_del(m_user.modules);
		m_user.modules = NULL;
	}*/
#endif
	m_term = NULL;
	gf_sys_close();
	debug_log("shutdown end");
}

void CNativeWrapper::setJavaEnv(JavaEnvTh * envToSet, JNIEnv *env, jobject callback) {
	assert( envToSet );
	jclass localRef = env->GetObjectClass(callback);
	envToSet->env = env;
	envToSet->javaThreadId = gf_th_id();
	envToSet->cbk_obj = callback;
	envToSet->cbk_displayMessage =
	    env->GetMethodID(localRef, "displayMessage", "(Ljava/lang/String;Ljava/lang/String;I)V");
	envToSet->cbk_onProgress =
	    env->GetMethodID(localRef, "onProgress", "(Ljava/lang/String;II)V");
	envToSet->cbk_onLog =
	    env->GetMethodID(localRef, "onLog", "(IILjava/lang/String;)V");
	envToSet->cbk_setCaption =
	    env->GetMethodID(localRef, "setCaption", "(Ljava/lang/String;)V");
	envToSet->cbk_showKeyboard =
	    env->GetMethodID(localRef, "showKeyboard", "(Z)V");
	envToSet->cbk_setLogFile =
	    env->GetMethodID(localRef, "setLogFile", "(Ljava/lang/String;)V");
	envToSet->cbk_sensorSwitch =
	    env->GetMethodID(localRef, "sensorSwitch", "(Z)V");
	env->DeleteLocalRef(localRef);
}

static u32 beforeThreadExits(void * param) {
	/* Ivica - I think there is no need for this because the detach is done in jni_destroy_env_func()*/

	/*LOGI("Before Thread exist, detach the JavaVM from Thread for thread %p...\n", gf_th_current());
	if (javaVM)
	javaVM->DetachCurrentThread();
	*/
	return 0;
}

JavaEnvTh * CNativeWrapper::getEnv() {
	JNIEnv *env;
	JavaEnvTh * javaEnv;
	if (!javaVM) {
		debug_log("************* No JVM Found ************");
		return NULL;
	}
	javaEnv = (JavaEnvTh*) pthread_getspecific( jni_thread_env_key );
	if (javaEnv)
		return javaEnv;
	javaEnv = (JavaEnvTh *) gf_malloc(sizeof(JavaEnvTh));
	if (!javaEnv)
		return NULL;
	memset(javaEnv, 0, sizeof(JavaEnvTh));
	javaVM->AttachCurrentThread(&env, NULL);
	if (!env) {
		LOGE("Attaching to thread did failed for thread id=%d", gf_th_id());
		gf_free(javaEnv);
		return NULL;
	}
	LOGI("Rebuilding methods for thread %d", gf_th_id());
	setJavaEnv(javaEnv, env, mainJavaEnv->cbk_obj);
	if (pthread_setspecific(jni_thread_env_key, javaEnv)) {
		LOGE("Failed to set specific thread data to jni_thread_env_key for thread=%d. No ENV available !", gf_th_id());
		gf_free(javaEnv);
		return NULL;
	}
	GF_Thread * t;
	LOGI("Getting current Thread %d...", gf_th_id());
	t = gf_th_current();
	LOGI("Getting current Thread DONE = %p, now registering before exit...", t);

	if (GF_OK != gf_register_before_exit_function(gf_th_current(), &beforeThreadExits)) {
		LOGE("Failed to register exit function for thread %p, no javaEnv for current thread.", gf_th_current());
		//javaVM->DetachCurrentThread();
		gf_free(javaEnv);
		javaEnv = NULL;
	}
	LOGI("Registering DONE for %d", gf_th_id());
	return javaEnv;
}


//-------------------------------
int CNativeWrapper::MessageBox(const char* msg, const char* title, GF_Err status) {
	LOGV("MessageBox start %s", msg);
	JavaEnvTh * env = getEnv();
	if (!env || !env->cbk_displayMessage)
		return 0;
	env->env->PushLocalFrame(2);
	jstring tit = env->env->NewStringUTF(title?title:"null");
	jstring mes = env->env->NewStringUTF(msg?msg:"null");
	env->env->CallVoidMethod(env->cbk_obj, env->cbk_displayMessage, mes, tit, status);
	env->env->PopLocalFrame(NULL);
	LOGV("MessageBox done %s", msg);
	return 1;
}
//-------------------------------
int CNativeWrapper::Quit(int code) {

	Shutdown();
	// send shutdown request to java
	return code;
}

#include <stdio.h>

void CNativeWrapper::on_fm_request(void *cbk, u32 type, u32 param, int *value) {
	CNativeWrapper * self = (CNativeWrapper *) cbk;
	JavaEnvTh *envth = self->getEnv();

	envth->env->PushLocalFrame(1);
	switch(type) {
	case 0: // Get PI
	case 1: // Get RT
	case 2: // Get PS
		jint jvalue;
		jvalue = envth->env->CallIntMethod(envth->cbk_obj, envth->cbk_onFmRequest, type, param);
		*value = (int)jvalue;
		break;
	case 3: // Set Freq
	case 4: // Set Volume
		envth->env->CallVoidMethod(envth->cbk_obj, envth->cbk_onFmRequest, type, param);
		break;
	}
	envth->env->PopLocalFrame(NULL);
}


//-------------------------------
void CNativeWrapper::on_gpac_log(void *cbk, GF_LOG_Level ll, GF_LOG_Tool lm, const char *fmt, va_list list) {
	char szMsg[4096];
	const char * tag;
	char unknTag[32];
	int level = (ll == GF_LOG_ERROR) ? ANDROID_LOG_ERROR : ANDROID_LOG_DEBUG;
	vsnprintf(szMsg, 4096, fmt, list);
	CNativeWrapper * self = (CNativeWrapper *) cbk;
	if (!self)
		goto displayInAndroidlogs;

#if 0
	{
		JavaEnvTh *env = self->getEnv();
		jstring msg;

		if (!env || !env->cbk_onLog)
			goto displayInAndroidlogs;

		env->env->PushLocalFrame(1);
		msg = env->env->NewStringUTF(szMsg);
		env->env->CallVoidMethod(env->cbk_obj, env->cbk_onLog, level, lm, msg);
		env->env->PopLocalFrame(NULL);
		return;
	}

#else
	if (self->log_file) {
		vfprintf(self->log_file, fmt, list);
		fflush(self->log_file);
	}
#endif


displayInAndroidlogs:
	{
		/* When no callback is properly set, we use direct logging */
		switch( lm) {
		case GF_LOG_CORE:
			tag="GF_LOG_CORE";
			break;
		case GF_LOG_CODING:
			tag="GF_LOG_CODING";
			break;
		case GF_LOG_CONTAINER:
			tag="GF_LOG_CONTAINER";
			break;
		case GF_LOG_NETWORK:
			tag="GF_LOG_NETWORK";
			break;
		case GF_LOG_RTP:
			tag="GF_LOG_RTP";
			break;
		case GF_LOG_COMPTIME:
			tag="GF_LOG_COMPTIME";
			break;
		case GF_LOG_CODEC:
			tag="GF_LOG_CODEC";
			break;
		case GF_LOG_PARSER:
			tag="GF_LOG_PARSER";
			break;
		case GF_LOG_MEDIA:
			tag="GF_LOG_MEDIA";
			break;
		case GF_LOG_SCENE:
			tag="GF_LOG_SCENE";
			break;
		case GF_LOG_SCRIPT:
			tag="GF_LOG_SCRIPT";
			break;
		case GF_LOG_INTERACT:
			tag="GF_LOG_INTERACT";
			break;
		case GF_LOG_COMPOSE:
			tag="GF_LOG_COMPOSE";
			break;
		case GF_LOG_CACHE:
			tag="GF_LOG_CACHE";
			break;
		case GF_LOG_MMIO:
			tag="GF_LOG_MMIO";
			break;
		case GF_LOG_RTI:
			tag="GF_LOG_RTI";
			break;
		case GF_LOG_MEMORY:
			tag="GF_LOG_MEMORY";
			break;
		case GF_LOG_AUDIO:
			tag="GF_LOG_AUDIO";
			break;
		case GF_LOG_MODULE:
			tag="GF_LOG_MODULE";
			break;
		case GF_LOG_MUTEX:
			tag="GF_LOG_MUTEX";
			break;
		case GF_LOG_CONDITION:
			tag="GF_LOG_CONDITION";
			break;
		case GF_LOG_DASH:
			tag="GF_LOG_DASH";
			break;
		case GF_LOG_FILTER:
			tag="GF_LOG_FILTER";
			break;
		case GF_LOG_SCHEDULER:
			tag="GF_LOG_SCHEDULER";
			break;
		case GF_LOG_ROUTE:
			tag="GF_LOG_ROUTE";
			break;
		case GF_LOG_CONSOLE:
			tag="GF_LOG_CONSOLE";
			break;
		case GF_LOG_APP:
			tag="GF_LOG_APP";
			break;
		default:
			snprintf(unknTag, 32, "GPAC_UNKNOWN[%d]", lm);
			tag = unknTag;
		}
		__android_log_print(level, tag, "%s\n", szMsg);
	}
}
//-------------------------------
Bool CNativeWrapper::GPAC_EventProc(void *cbk, GF_Event *evt) {
	if (cbk)
	{
		CNativeWrapper* ptr = (CNativeWrapper *) cbk;
		char msg[4096];
		msg[0] = 0;
		LOGD("GPAC_EventProc() Message=%d", evt->type);
		switch (evt->type) {
		case GF_EVENT_CLICK:
		case GF_EVENT_MOUSEUP:
		case GF_EVENT_MOUSEDOWN:
		case GF_EVENT_MOUSEOVER:
		case GF_EVENT_MOUSEOUT:
		case GF_EVENT_MOUSEMOVE:
		case GF_EVENT_MOUSEWHEEL:
		case GF_EVENT_KEYUP:
		case GF_EVENT_KEYDOWN:
		case GF_EVENT_LONGKEYPRESS:
		case GF_EVENT_TEXTINPUT:
			/* We ignore all these events */
			break;
		case GF_EVENT_MEDIA_SETUP_BEGIN:
		case GF_EVENT_MEDIA_SETUP_DONE:
		case GF_EVENT_MEDIA_LOAD_START:
		case GF_EVENT_MEDIA_PLAYING:
		case GF_EVENT_MEDIA_WAITING:
		case GF_EVENT_MEDIA_PROGRESS:
		case GF_EVENT_MEDIA_LOAD_DONE:
		case GF_EVENT_ABORT:
		case GF_EVENT_ERROR:
			LOGD("GPAC_EventProc() Media Event detected = [index=%d]", evt->type - GF_EVENT_MEDIA_SETUP_BEGIN);
			break;
		case GF_EVENT_MESSAGE:
		{
			ptr->debug_log("GPAC_EventProc start");
			if ( evt->message.message )
			{
				strcat(msg, evt->message.message);
				strcat(msg, ": ");
			}
			strcat(msg, gf_error_to_string(evt->message.error));

			ptr->debug_log(msg);
			//ptr->MessageBox(msg, evt->message.service ? evt->message.service : "GF_EVENT_MESSAGE", evt->message.error);
			ptr->debug_log("GPAC_EventProc end");
		};
		break;
		case GF_EVENT_CONNECT:
			if (evt->connect.is_connected)
				ptr->MessageBox("Connected", "Connected to scene", GF_OK);
			else
				ptr->MessageBox("Disconnected", "Disconnected from scene.", GF_OK);
			break;
		case GF_EVENT_PROGRESS:
		{
			const char * szTitle;
			if (evt->progress.progress_type==0)
				szTitle = "Buffering";
			else if (evt->progress.progress_type==1)
				szTitle = "Downloading...";
			else if (evt->progress.progress_type==2)
				szTitle = "Import ";
			else
				szTitle = "Unknown Progress Event";
			ptr->Osmo4_progress_cbk(ptr, szTitle, evt->progress.done, evt->progress.total);
			gf_set_progress(szTitle, evt->progress.done, evt->progress.total);
		}
		break;
		case GF_EVENT_TEXT_EDITING_START:
		case GF_EVENT_TEXT_EDITING_END:
		{
			JavaEnvTh * env = ptr->getEnv();
			if (!env || !env->cbk_showKeyboard)
				return GF_FALSE;
			LOGI("Needs to display/hide the Virtual Keyboard (%d)", evt->type);
			env->env->CallVoidMethod(env->cbk_obj, env->cbk_showKeyboard, GF_EVENT_TEXT_EDITING_START == evt->type);
			LOGV("Done showing virtual keyboard (%d)", evt->type);
		}
		break;
		case GF_EVENT_EOS:
			LOGI("EOS Reached (%d)", evt->type);
			break;
		case GF_EVENT_DISCONNECT:
			/* FIXME : not sure about this behaviour */
			if (ptr)
				ptr->disconnect();
			break;
		case GF_EVENT_NAVIGATE:
			ptr->navigate( evt);
			break;
		case GF_EVENT_SENSOR_REQUEST:
			if(evt->activate_sensor.sensor_type == GF_EVENT_SENSOR_ORIENTATION){
				if(evt->activate_sensor.activate){
					LOGV("We received Sensor Request for turning ON location sensors");
					JavaEnvTh * env = ptr->getEnv();
					if (!env || !env->cbk_sensorSwitch)
						return GF_FALSE;
					env->env->CallVoidMethod(env->cbk_obj, env->cbk_sensorSwitch, evt->activate_sensor.activate);
				}else{
					LOGV("We received Sensor Request for turning OFF location sensors");
					JavaEnvTh * env = ptr->getEnv();
					if (!env || !env->cbk_sensorSwitch)
						return GF_FALSE;
					env->env->CallVoidMethod(env->cbk_obj, env->cbk_sensorSwitch, evt->activate_sensor.activate);
				}
				return GF_TRUE;
			}
			break;
		case GF_EVENT_SENSOR_ORIENTATION:
			/* Ignore this event */
			/*
			LOGV("We received Sensor Orientation event (x: %f, y: %f, z: %f)",evt->sensor.x, evt->sensor.y, evt->sensor.z);
			*/
			break;
		default:
			LOGI("Unknown Message %d", evt->type);
		}
	}
	return GF_FALSE;
}

void CNativeWrapper::navigate( GF_Event* evt)
{
	if (gf_term_is_supported_url(m_term, evt->navigate.to_url, GF_TRUE, GF_TRUE))
	{
		gf_term_navigate_to(m_term, evt->navigate.to_url);
	}
}

void CNativeWrapper::progress_cbk(const char *title, u64 done, u64 total) {
	JavaEnvTh *env = getEnv();
	if (!env || !env->cbk_onProgress)
		return;
	//debug_log("Osmo4_progress_cbk start");
	env->env->PushLocalFrame(1);
	jstring js = env->env->NewStringUTF(title);
	env->env->CallVoidMethod(env->cbk_obj, env->cbk_onProgress, js, done, total);
	env->env->PopLocalFrame(NULL);
	//debug_log("Osmo4_progress_cbk end");
}


//-------------------------------
void CNativeWrapper::Osmo4_progress_cbk(const void *usr, const char *title, u64 done, u64 total) {
	if (!usr)
		return;
	CNativeWrapper * self = (CNativeWrapper *) usr;
	self->progress_cbk(title, done, total);
}
//-------------------------------
void CNativeWrapper::SetupLogs() {
	const char *opt;
	debug_log("SetupLogs()");

	gf_mx_p(m_mx);
	gf_log_set_tools_levels( gf_opts_get_key("core", "logs"), GF_TRUE );
	gf_log_set_callback(this, on_gpac_log);
	opt = gf_opts_get_key("core", "log-file");
	if (opt) {
#if 0
		JavaEnvTh *env = getEnv();
		if (env && env->cbk_setLogFile) {
			env->env->PushLocalFrame(1);
			jstring js = env->env->NewStringUTF(opt);
			env->env->CallVoidMethod(env->cbk_obj, env->cbk_setLogFile, js);
			env->env->PopLocalFrame(NULL);
		}
#else
		log_file = gf_fopen(opt, "wt");
#endif
	}
	gf_mx_v(m_mx);

	GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("Osmo4 logs initialized\n"));
	/* Test for JNI invocations, should work properly
	int k;
	for (k = 0 ; k < 512; k++){
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("Message %d\n", k));
	}*/
}
//-------------------------------
// dir should end with /
int CNativeWrapper::init(JNIEnv * env, void * bitmap, jobject * callback, int width, int height, const char * cfg_dir, const char * modules_dir, const char * cache_dir, const char * font_dir, const char * gui_dir, const char * urlToLoad) {
	LOGI("Initializing GPAC with URL=%s...", urlToLoad);
	strcpy(m_modules_dir, modules_dir);
	strcpy(m_cache_dir, cache_dir);
	strcpy(m_font_dir, font_dir);
	if (cfg_dir)
		strcpy(m_cfg_dir, cfg_dir);

/*	char m_cfg_filename[GF_MAX_PATH];
	if (m_cfg_dir[0]) {
		LOGI("GPAC.cfg found in %s, force using it.\n", m_cfg_dir);
		strcpy(m_cfg_filename, m_cfg_dir);
		strcat(m_cfg_filename, "GPAC.cfg");
	}
*/

	int m_Width = width;
	int m_Height = height;

	const char *opt;

	m_window = env;
	m_session = bitmap;
	if (!mainJavaEnv)
		mainJavaEnv = (JavaEnvTh *) gf_malloc(sizeof(JavaEnvTh));
	memset(mainJavaEnv, 0, sizeof(JavaEnvTh));
	setJavaEnv(mainJavaEnv, env, env->NewGlobalRef(*callback));
	if (pthread_setspecific( jni_thread_env_key, mainJavaEnv)) {
		LOGE("Failed to set specific thread data to jni_thread_env_key=%d for main thread !", jni_thread_env_key);
	}

	m_mx = gf_mx_new("Osmo4");

	m_user.init_flags = 0;
	m_user.opaque = this;

	m_user.os_window_handler = m_window;
	m_user.os_display = m_session;
	m_user.EventProc = GPAC_EventProc;
	if (!javaVM) {
		LOGE("NO JAVA VM FOUND, m_user=%p !!!!\n", &m_user);
		return Quit(KErrGeneral);
	}

	LOGD("Loading GPAC terminal, m_user=%p...", &m_user);
	gf_sys_init(GF_MemTrackerNone, NULL);
	gf_fm_request_set_callback(this, on_fm_request);

	gf_set_progress_callback(this, Osmo4_progress_cbk);

	//load config file
	//LOGI("Loading User Config %s...", "GPAC.cfg");
	//m_user.config = gf_cfg_init(m_cfg_dir ? m_cfg_filename : NULL, NULL);


	char path[512];
	strcpy(path, gui_dir);
	char vertex_path[512], fragment_path[512];
	strcpy(vertex_path, path);
	strcpy(fragment_path, path);
	strcat(vertex_path, "/../shaders/vertex.glsl");
	strcat(fragment_path, "/../shaders/fragment.glsl");

	gf_opts_set_key("core", "vert-shader", vertex_path);
	gf_opts_set_key("core", "frag-shader", fragment_path);
	gf_opts_set_key("filter@compositor", "ogl", "hybrid");
	opt = gf_opts_get_key("core", "mod-dirs");
	LOGI("loading modules in directory %s...", opt);

	/*m_user.modules = gf_modules_new(opt, m_user.config);
	if (!m_user.modules || !gf_modules_get_count(m_user.modules)) {
		LOGE("No modules found in directory %s !", opt);
		if (m_user.modules)
			gf_modules_del(m_user.modules);
		gf_cfg_del(m_user.config);
		m_user.config = NULL;
		return Quit(KErrGeneral);
	}*/

	SetupLogs();
	m_term = gf_term_new(&m_user);
	if (!m_term) {
		LOGE("Cannot load GPAC Terminal with m_user=%p", &m_user);
		MessageBox("Cannot load GPAC terminal", "Fatal Error", GF_SERVICE_ERROR);
		/*gf_modules_del(m_user.modules);
		m_user.modules = NULL;
		gf_cfg_del(m_user.config);
		m_user.config = NULL;*/
		return Quit(KErrGeneral);
	}

	/*force fullscreen*/
	gf_term_set_option(m_term, GF_OPT_FULLSCREEN, 1);

	//setAudioEnvironment(javaVM);

	LOGD("Setting term size m_user=%p...", &m_user);
	gf_term_set_size(m_term, m_Width, m_Height);

	opt = gf_opts_get_key("General", "StartupFile");
	LOGD("File loaded at startup=%s.", opt);

	if (!urlToLoad)
		urlToLoad = opt;
	if (urlToLoad) {
		LOGI("Connecting to %s...", urlToLoad);
		gf_term_connect(m_term, urlToLoad);
	}
	debug_log("init end");
	LOGD("Saving config file...\n");
	//gf_cfg_save(m_user.config);
	LOGI("Initialization complete, config file saved.\n");

	return 0;
}
//-------------------------------
int CNativeWrapper::connect(const char *url)
{
	if (m_term)
	{
		const char *str;
		debug_log("Starting to connect ...");
		str = gf_opts_get_key("General", "StartupFile");
		if (str)
		{
			gf_opts_set_key("Temp", "GUIStartupFile", url);
			gf_term_connect(m_term, str);
		}
		if( url )
		{
			gf_term_connect_from_time(m_term, url, 0, GF_FALSE);
		}
	}
	debug_log("connected ...");
	return 0;
}

void CNativeWrapper::setGpacPreference( const char * category, const char * name, const char * value)
{
	//if (m_user.config) {
		gf_opts_set_key(category, name, value);
	//	gf_cfg_save(m_user.config);
	//}
}

//-----------------------------------------------------
void CNativeWrapper::disconnect() {
	if (m_term) {
		debug_log("disconnecting");
		gf_term_disconnect(m_term);
		debug_log("disconnected ...");
	}
}
//-----------------------------------------------------
void CNativeWrapper::step(void * env, void * bitmap) {
	m_window = env;
	m_session = bitmap;
	m_term->compositor->frame_draw_type = GF_SC_DRAW_FRAME;
	while (!gf_term_process_step(m_term)) {
		debug_log("step(): nothing drawn, retrying\n");
	}
	debug_log("step(): frame drawn\n");
}

//-----------------------------------------------------
void CNativeWrapper::setAudioEnvironment(JavaVM* javaVM) {
	if (!m_term) {
		debug_log("setAudioEnvironment(): no m_term found.");
		return;
	}
	debug_log("setAudioEnvironment start");
	//m_term->compositor->audio_renderer->audio_out->Setup(m_term->compositor->audio_renderer->audio_out, javaVM, 0, 0);
	debug_log("setAudioEnvironment end");
}
//-----------------------------------------------------
void CNativeWrapper::resize(int w, int h) {
	if (!m_term)
		return;
	debug_log("resize start");
	gf_term_set_size(m_term, w, h);
	debug_log("resize end");
}

//-----------------------------------------------------
void CNativeWrapper::onMouseDown(float x, float y) {
	if (!m_term)
		return;
	debug_log("onMouseDown start");
	//char msg[100];
	//sprintf(msg, "onMousedown x=%f, y=%f", x, y );
	//debug_log(msg);

	GF_Event evt;
	evt.type = GF_EVENT_MOUSEDOWN;
	evt.mouse.button = GF_MOUSE_LEFT;
	evt.mouse.x = x;
	evt.mouse.y = y;

	gf_term_user_event(m_term, &evt);
	debug_log("onMouseDown end");
}
//-----------------------------------------------------
void CNativeWrapper::onMouseUp(float x, float y) {
	if (!m_term)
		return;
	debug_log("onMouseUp start");
	//char msg[100];
	//sprintf(msg, "onMouseUp x=%f, y=%f", x, y );
	//debug_log(msg);

	GF_Event evt;
	evt.type = GF_EVENT_MOUSEUP;
	evt.mouse.button = GF_MOUSE_LEFT;
	evt.mouse.x = x;
	evt.mouse.y = y;

	gf_term_user_event(m_term, &evt);
	debug_log("onMouseUp end");
}
//-----------------------------------------------------
void CNativeWrapper::onMouseMove(float x, float y) {
	if (!m_term)
		return;
	GF_Event evt;
	evt.type = GF_EVENT_MOUSEMOVE;
	evt.mouse.button = GF_MOUSE_LEFT;
	evt.mouse.x = x;
	evt.mouse.y = y;

	gf_term_user_event(m_term, &evt);
}
//-----------------------------------------------------
void CNativeWrapper::onKeyPress(int keycode, int rawkeycode, int up, int flag, int unicode) {
	if (!m_term)
		return;
	debug_log("onKeyPress start");
	GF_Event evt;
	memset(&evt, 0, sizeof(GF_Event));
	if (up == 0) evt.type = GF_EVENT_KEYUP;
	else evt.type = GF_EVENT_KEYDOWN;

	evt.key.flags = 0;
	evt.key.hw_code = rawkeycode;

	translate_key(keycode, &evt.key);
	//evt.key.key_code = GF_KEY_A;
	gf_term_user_event(m_term, &evt);

	if (evt.type == GF_EVENT_KEYUP && unicode) {
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_TEXTINPUT;
		evt.character.unicode_char = unicode;
		gf_term_user_event(m_term, &evt);
	}
}
//-----------------------------------------------------
void CNativeWrapper::translate_key(ANDROID_KEYCODE keycode, GF_EventKey *evt) {
	evt->flags = 0;
	switch (keycode) {
	case ANDROID_KEYCODE_BACK:
		evt->key_code = GF_KEY_BACKSPACE;
		evt->hw_code = 8;
		break;
	case ANDROID_KEYCODE_TAB:
		evt->key_code = GF_KEY_TAB;
		evt->hw_code = 9;
		break;
	case ANDROID_KEYCODE_CLEAR:
		evt->key_code = GF_KEY_CLEAR;
		break;
	case ANDROID_KEYCODE_DPAD_CENTER:
	case ANDROID_KEYCODE_ENTER:
		evt->key_code = GF_KEY_ENTER;
		evt->hw_code = 13;
		break;
	case ANDROID_KEYCODE_SHIFT_LEFT:
		evt->key_code = GF_KEY_SHIFT;
		break;
	case ANDROID_KEYCODE_SHIFT_RIGHT:
		evt->key_code = GF_KEY_SHIFT;
		break;
	case ANDROID_KEYCODE_ALT_LEFT:
		evt->key_code = GF_KEY_ALT;
		break;
	case ANDROID_KEYCODE_ALT_RIGHT:
		evt->key_code = GF_KEY_ALT;
		break;
	case ANDROID_KEYCODE_SPACE:
		evt->key_code = GF_KEY_SPACE;
		evt->hw_code = 32;
		break;
	case ANDROID_KEYCODE_HOME:
		evt->key_code = GF_KEY_HOME;
		break;
	case ANDROID_KEYCODE_DPAD_LEFT:
		evt->key_code = GF_KEY_LEFT;
		break;
	case ANDROID_KEYCODE_DPAD_UP:
		evt->key_code = GF_KEY_UP;
		break;
	case ANDROID_KEYCODE_DPAD_RIGHT:
		evt->key_code = GF_KEY_RIGHT;
		break;
	case ANDROID_KEYCODE_DPAD_DOWN:
		evt->key_code = GF_KEY_DOWN;
		break;
	case ANDROID_KEYCODE_DEL:
		evt->key_code = GF_KEY_BACKSPACE;
		evt->hw_code = 8;
		break;
	case ANDROID_KEYCODE_0:
		evt->key_code = GF_KEY_0;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case ANDROID_KEYCODE_1:
		evt->key_code = GF_KEY_1;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case ANDROID_KEYCODE_2:
		evt->key_code = GF_KEY_2;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case ANDROID_KEYCODE_3:
		evt->key_code = GF_KEY_3;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case ANDROID_KEYCODE_4:
		evt->key_code = GF_KEY_4;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case ANDROID_KEYCODE_5:
		evt->key_code = GF_KEY_5;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case ANDROID_KEYCODE_6:
		evt->key_code = GF_KEY_6;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case ANDROID_KEYCODE_7:
		evt->key_code = GF_KEY_7;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case ANDROID_KEYCODE_8:
		evt->key_code = GF_KEY_8;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case ANDROID_KEYCODE_9:
		evt->key_code = GF_KEY_9;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	/*thru VK_9 are the same as ASCII '0' thru '9' (0x30 - 0x39) */
	/* VK_A thru VK_Z are the same as ASCII 'A' thru 'Z' (0x41 - 0x5A) */
	default:
		if ((keycode>=ANDROID_KEYCODE_A) && (keycode<=ANDROID_KEYCODE_Z)) {
			evt->key_code = GF_KEY_A + keycode - ANDROID_KEYCODE_A;
		} else {
			evt->key_code = GF_KEY_UNIDENTIFIED;
		}
		break;
	}
}
//-----------------------------------------------------
     /**
      * x: yaw (azimuth - rotation around the -Z axis) range: [-PI, PI]
      * y: pitch (rotation around the -X axis) range: [-PI/2, PI/2]
      * z: roll (rotation around the Y axis) range: [-PI, PI]
      */
void CNativeWrapper::onOrientationChange(float x, float y, float z) {
	if (!m_term)
		return;
	GF_Event evt;
	evt.type = GF_EVENT_SENSOR_ORIENTATION;
	evt.sensor.x = x;
	evt.sensor.y = y;
	evt.sensor.z = z;
	evt.sensor.w = 0;

	gf_term_user_event(m_term, &evt);
}
//-----------------------------------------------------
void CNativeWrapper::setGpacLogs(const char *tools_at_level)
{
	gf_log_set_tools_levels(tools_at_level, GF_TRUE);
}

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
