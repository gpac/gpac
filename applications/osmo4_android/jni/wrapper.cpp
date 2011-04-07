/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) ENST 2009-
 *				Authors: Jean Le Feuvre
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
#include <gpac/modules/service.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>

#include "wrapper.h"
#include "wrapper_jni.c"

#include <math.h>
#include <android/log.h>

#define TAG "GPAC_WRAPPER"

#define LOGV(X, Y)  __android_log_print(ANDROID_LOG_VERBOSE, TAG, X, Y)
#define LOGD(X, Y)  __android_log_print(ANDROID_LOG_DEBUG, TAG, X, Y)
#define LOGE(X, Y)  __android_log_print(ANDROID_LOG_ERROR, TAG, X, Y)
#define LOGW(X, Y)  __android_log_print(ANDROID_LOG_WARN, TAG, X, Y)
#define LOGI(X, Y)  __android_log_print(ANDROID_LOG_INFO, TAG, X, Y)

static JavaVM* javaVM = NULL;

#define DETACH_ENV(this, env) if (javaVM && env != &(this->mainJavaEnv)) javaVM->DetachCurrentThread()

static int jniRegisterNativeMethods(JNIEnv* env, const char* className,
    const JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz;
    clazz = env->FindClass(className);
    if (clazz == NULL) {
        LOGE("Native registration unable to find class '%s'\n", className);
        return -1;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        LOGE("RegisterNatives failed for '%s'\n", className);
        return -1;
    }
    return 0;
}

static JNINativeMethod sMethods[] = {
     /* name, signature, funcPtr */

    {"createInstance",
      "(Lcom/artemis/Osmo4/GpacCallback;IILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)J",
      (void*)&Java_com_artemis_Osmo4_GPACInstance_createInstance},
    {"gpacdisconnect",
      "()V",
      (void*)Java_com_artemis_Osmo4_GPACInstance_gpacdisconnect},
    {"gpacrender",
      "()V",
      (void*)Java_com_artemis_Osmo4_GPACInstance_gpacrender},
    {"gpacresize",
      "(II)V",
      (void*)Java_com_artemis_Osmo4_GPACInstance_gpacresize},
    {"gpacfree",
      "()V",
      (void*)Java_com_artemis_Osmo4_GPACInstance_gpacfree},
    {"gpaceventkeypress",
      "(IIII)V",
      (void*)Java_com_artemis_Osmo4_GPACInstance_gpaceventkeypress},
    {"gpaceventmousedown",
      "(FF)V",
      (void*)Java_com_artemis_Osmo4_GPACInstance_gpaceventmousedown},
    {"gpaceventmouseup",
      "(FF)V",
      (void*)Java_com_artemis_Osmo4_GPACInstance_gpaceventmouseup},
    {"gpaceventmousemove",
      "(FF)V",
      (void*)Java_com_artemis_Osmo4_GPACInstance_gpaceventmousemove},
    NULL
};



//---------------------------------------------------------------------------------------------------
jint JNI_OnLoad(JavaVM* vm, void* reserved){
        const char * className = "com/artemis/Osmo4/GPACInstance";
        JNIEnv * env;
        if (!vm)
          return -1;
        if (vm->GetEnv((void**)(&env), JNI_VERSION_1_2) != JNI_OK)
          return -1;
	javaVM = vm;
        LOGI("Registering %s natives\n", className);
        if (jniRegisterNativeMethods(env, className, sMethods, 9) < 0){
          LOGE("Failed to register native methods for %s !\n", className);
          return -1;
        }
        LOGI("Registering %s natives DONE.\n", className);
	return JNI_VERSION_1_2;
}
//---------------------------------------------------------------------------------------------------
//CNativeWrapper
//-------------------------------

CNativeWrapper::CNativeWrapper(){
	do_log = 1;
        m_term = NULL;
        m_mx = NULL;
#ifndef GPAC_GUI_ONLY
	memset(&m_user, 0, sizeof(GF_User));
	memset(&m_rti, 0, sizeof(GF_SystemRTInfo));
#endif
}
//-------------------------------
CNativeWrapper::~CNativeWrapper(){
      JavaEnvTh * env = getEnv();
      debug_log("~CNativeWrapper()");
      if (env && env->cbk_obj)
        env->env->DeleteGlobalRef(env->cbk_obj);
      DETACH_ENV(this, env);
      Shutdown();
      debug_log("~CNativeWrapper() : DONE\n");
}
//-------------------------------
void CNativeWrapper::debug_log(const char* msg){
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
    if (m_user.config) {
		gf_cfg_del(m_user.config);
		m_user.config = NULL;
    }
    if (m_user.modules) {
		gf_modules_del(m_user.modules);
		m_user.modules = NULL;
    }
#endif
    m_term = NULL;
    debug_log("shutdown end");
}

void CNativeWrapper::setJavaEnv(JavaEnvTh * envToSet, JNIEnv *env, jobject callback){
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
}

JavaEnvTh * CNativeWrapper::getEnv(){
    JNIEnv *env;
    if (!javaVM){
      debug_log("************* No JVM Found ************");
      return NULL;
    }
    if (mainJavaEnv.javaThreadId == gf_th_id())
      return &mainJavaEnv;
    debug_log("Attaching thread...");
    javaVM->AttachCurrentThread(&env, NULL);
    debug_log("Rebuilding methods...");
    if (!env){
      debug_log("getEnv() returns NULL !");
      return NULL;
    }
    setJavaEnv(&currentJavaEnv, env, mainJavaEnv.cbk_obj);
    return &currentJavaEnv;
}


//-------------------------------
int CNativeWrapper::MessageBox(const char* msg, const char* title, GF_Err status){
         debug_log("MessageBox start");
	JavaEnvTh * env = getEnv();
	if (!env || !env->cbk_displayMessage)
		return 0;
	jstring tit = env->env->NewStringUTF(title?title:"null");
	jstring mes = env->env->NewStringUTF(msg?msg:"null");
	env->env->CallVoidMethod(env->cbk_obj, env->cbk_displayMessage, mes, tit, status);
        DETACH_ENV(this, env);
        debug_log("MessageBox end");
	return 1;
}
//-------------------------------
void CNativeWrapper::DisplayRTI(){
#ifndef GPAC_GUI_ONLY
	// display some system informations ?
#endif
}
//-------------------------------
int CNativeWrapper::Quit(int code){

	Shutdown();
	// send shutdown request to java
	return code;
}

#include <stdio.h>

//-------------------------------
void CNativeWrapper::on_gpac_log(void *cbk, u32 ll, u32 lm, const char *fmt, va_list list){
	char szMsg[4096];
        const char * tag;
        char unknTag[32];
        int debug;
        // We do not want to be flood by mutexes
        if (ll == GF_LOG_DEBUG && lm == GF_LOG_CORE)
          return;
        vsnprintf(szMsg, 4096, fmt, list);
        CNativeWrapper * self = (CNativeWrapper *) cbk;
        if (!self)
          goto displayInAndroidlogs;

        {
          JavaEnvTh *env = self->getEnv();
          jstring msg;
          if (!env || !env->cbk_onProgress)
                goto displayInAndroidlogs;
          msg = env->env->NewStringUTF(szMsg);
          env->env->CallVoidMethod(env->cbk_obj, env->cbk_onLog, ll, lm, msg);
          DETACH_ENV(self, env);
          return;
        }
displayInAndroidlogs:
  {
  /* When no callback is properly set, we use direct logging */
    switch (ll){
          case GF_LOG_DEBUG:
            debug = ANDROID_LOG_DEBUG;
            break;
          case GF_LOG_INFO:
            debug = ANDROID_LOG_INFO;
            break;
          case GF_LOG_WARNING:
            debug = ANDROID_LOG_WARN;
            break;
          case GF_LOG_ERROR:
            debug = ANDROID_LOG_ERROR;
            break;
          default:
            debug = ANDROID_LOG_INFO;
    }
    switch( lm){
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
        case GF_LOG_AUTHOR:
                tag="GF_LOG_AUTHOR";
                break;
        case GF_LOG_SYNC:
                tag="GF_LOG_SYNC";
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
        case GF_LOG_SMIL:
                tag="GF_LOG_SMIL";
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
          default:
            snprintf(unknTag, 32, "GPAC_UNKNOWN[%d]", lm);
            tag = unknTag;
    }
    __android_log_print(debug, tag, szMsg);
  }
}
//-------------------------------
Bool CNativeWrapper::GPAC_EventProc(void *cbk, GF_Event *evt){
	if (cbk)
	{
		CNativeWrapper* ptr = (CNativeWrapper*)cbk;
		char msg[4096];
		msg[0] = 0;
                LOGD("GPAC_EventProc() Message=%d", evt->type);
                switch (evt->type){
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
                  case GF_EVENT_MEDIA_BEGIN_SESSION_SETUP:
                  case GF_EVENT_MEDIA_END_SESSION_SETUP:
                  case GF_EVENT_MEDIA_DATA_REQUEST:
                  case GF_EVENT_MEDIA_PLAYABLE:
                  case GF_EVENT_MEDIA_NOT_PLAYABLE:
                  case GF_EVENT_MEDIA_DATA_PROGRESS:
                  case GF_EVENT_MEDIA_END_OF_DATA:
                  case GF_EVENT_MEDIA_STOP:
                  case GF_EVENT_MEDIA_ERROR:
                    LOGD("GPAC_EventProc() Media Event detected = [index=%d]", evt->type - GF_EVENT_MEDIA_BEGIN_SESSION_SETUP);
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
                          ptr->MessageBox(msg, evt->message.service ? evt->message.service : "GF_EVENT_MESSAGE", evt->message.error);
                          ptr->debug_log("GPAC_EventProc end");
                  };
                  break;
                  case GF_EVENT_CONNECT:
                    if (evt->connect.is_connected)
                      ptr->MessageBox("Connected", "Connected to scene", GF_OK);
                    else
                      ptr->MessageBox("Disconnected", "Disconnected from scene : Connection FAILED.", GF_IO_ERR);
                    break;
                  case GF_EVENT_PROGRESS:
                  {
                          const char * szTitle;;
                          if (evt->progress.progress_type==0)
                            szTitle = "Buffering";
                          else if (evt->progress.progress_type==1)
                            szTitle = "Downloading...";
                          else if (evt->progress.progress_type==2)
                            szTitle = "Import ";
                          else
                            szTitle = "Unknown Progress Event";
                          //ptr->Osmo4_progress_cbk(ptr, szTitle, evt->progress.done, evt->progress.total);
                          gf_set_progress(szTitle, evt->progress.done, evt->progress.total);
                  }
                  break;
                  case GF_EVENT_TEXT_EDITING_START:
                  case GF_EVENT_TEXT_EDITING_END:
                  {
                      JavaEnvTh * env = ptr->getEnv();
                      if (!env || !env->cbk_showKeyboard)
                              return 0;
                      LOGI("Needs to display/hide the Virtual Keyboard (%d)", evt->type);
                      env->env->CallVoidMethod(env->cbk_obj, env->cbk_showKeyboard, GF_EVENT_TEXT_EDITING_START == evt->type);
                      DETACH_ENV(ptr, env);
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
                  default:
                    LOGI("Unknown Message %d", evt->type);
                }
	}
	return 0;
}


void CNativeWrapper::progress_cbk(const char *title, u64 done, u64 total){
        JavaEnvTh *env = getEnv();
        if (!env || !env->cbk_onProgress)
                return;
        debug_log("Osmo4_progress_cbk start");
        jstring js = env->env->NewStringUTF(title);
        env->env->CallVoidMethod(env->cbk_obj, env->cbk_onProgress, js, done, total);
        DETACH_ENV(this, env);
        debug_log("Osmo4_progress_cbk end");
}


//-------------------------------
void CNativeWrapper::Osmo4_progress_cbk(const void *usr, const char *title, u64 done, u64 total){
	if (!usr)
		return;
	CNativeWrapper * self = (CNativeWrapper *) usr;
	self->progress_cbk(title, done, total);
}
//-------------------------------
void CNativeWrapper::SetupLogs(){
	const char *opt;
	debug_log("SetupLogs()");

	gf_mx_p(m_mx);

	/*setup GPAC logs: log all errors*/
	gf_log_set_level(GF_LOG_DEBUG);
	gf_log_set_tools(0xFFFFFFFF);

	opt = gf_cfg_get_key(m_user.config, "General", "LogLevel");
        /* FIXME : set the loglevel according to config file */

        opt = gf_cfg_get_key(m_user.config, "General", "LogTools");
	if (opt) gf_log_set_tools(gf_log_parse_tools(opt));

	gf_log_set_callback(this, on_gpac_log);
	gf_mx_v(m_mx);

	GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("Osmo4 logs initialized\n"));
}
//-------------------------------
// dir should end with /
int CNativeWrapper::init(JNIEnv * env, void * bitmap, jobject * callback, int width, int height, const char * cfg_dir, const char * modules_dir, const char * cache_dir, const char * font_dir, const char * urlToLoad){
	strcpy(m_cfg_dir, cfg_dir);
	strcpy(m_modules_dir, modules_dir);
	strcpy(m_cache_dir, cache_dir);
	strcpy(m_font_dir, font_dir);

	strcpy(m_log_filename, cfg_dir);
	strcat(m_log_filename, "gpac_logs.txt");
	strcpy(m_debug_filename, cfg_dir);
	strcat(m_debug_filename, "osmo_debug.txt");

	char m_cfg_filename[GF_MAX_PATH];
	strcpy(m_cfg_filename, m_cfg_dir);
	strcat(m_cfg_filename, "GPAC.cfg");

	setJavaEnv(&mainJavaEnv, env, env->NewGlobalRef(*callback));
	debug_log("int CNativeWrapper::init()");

	int m_Width = width;
	int m_Height = height;

	int first_launch = 0;
	const char *opt;

	m_window = env;
	m_session = bitmap;

	m_mx = gf_mx_new("Osmo4");

	//load config file
	m_user.config = gf_cfg_new(GPAC_CFG_DIR, "GPAC.cfg");
	if (!m_user.config) {
		first_launch = 1;
		FILE *ft = fopen(m_cfg_filename, "wt");
		if (!ft) {
			MessageBox("Cannot create GPAC Config file", "Fatal Error", GF_SERVICE_ERROR);
			return Quit(KErrGeneral);
		} else {
			fclose(ft);
		}
		m_user.config = gf_cfg_new(GPAC_CFG_DIR, "GPAC.cfg");
		if (!m_user.config) {
			MessageBox("GPAC Configuration file not found", "Fatal Error", GF_SERVICE_ERROR);
			return Quit(KErrGeneral);
		}
	}

	SetupLogs();
        debug_log("set process callback...");
	gf_set_progress_callback(this, Osmo4_progress_cbk);

	opt = gf_cfg_get_key(m_user.config, "General", "ModulesDirectory");
	if (!opt) first_launch = 2;

	if (first_launch) {
		/*hardcode module directory*/
		gf_cfg_set_key(m_user.config, "General", "ModulesDirectory", GPAC_MODULES_DIR);
		/*hardcode cache directory*/
		gf_cfg_set_key(m_user.config, "General", "CacheDirectory", GPAC_CACHE_DIR);
		gf_cfg_set_key(m_user.config, "Downloader", "CleanCache", "yes");
		/*startup file*/
		char msg[256];
		snprintf(msg, 256, "%sgui/gui.bt",GPAC_CFG_DIR);
		gf_cfg_set_key(m_user.config, "General", "StartupFile", msg);
		gf_cfg_set_key(m_user.config, "General", "LastWorkingDir", GPAC_CFG_DIR);
		gf_cfg_set_key(m_user.config, "GUI", "UnhideControlPlayer", "1");
		/*setup UDP traffic autodetect*/
		gf_cfg_set_key(m_user.config, "Network", "AutoReconfigUDP", "yes");
		gf_cfg_set_key(m_user.config, "Network", "UDPTimeout", "10000");
		gf_cfg_set_key(m_user.config, "Network", "BufferLength", "3000");

		gf_cfg_set_key(m_user.config, "Compositor", "TextureTextMode", "Default");
		//gf_cfg_set_key(m_user.config, "Compositor", "FrameRate", "30");

		gf_cfg_set_key(m_user.config, "Video", "DriverName", "Android Video Output");

		gf_cfg_set_key(m_user.config, "Audio", "DriverName", "Android Audio Output");
		gf_cfg_set_key(m_user.config, "Audio", "ForceConfig", "no");
		gf_cfg_set_key(m_user.config, "Audio", "NumBuffers", "1");

		gf_cfg_set_key(m_user.config, "FontEngine", "FontReader", "ft_font");

		gf_cfg_set_key(m_user.config, "FontEngine", "FontDirectory", GPAC_FONT_DIR);
	}
        debug_log("loading modules...");
	/*load modules*/
	opt = gf_cfg_get_key(m_user.config, "General", "ModulesDirectory");
	m_user.modules = gf_modules_new(opt, m_user.config);
	if (!m_user.modules || !gf_modules_get_count(m_user.modules)) {
                debug_log("No modules available !!!!");
		MessageBox(m_user.modules ? "No modules available" : "Cannot create module manager", "Fatal Error", GF_SERVICE_ERROR);
		if (m_user.modules) gf_modules_del(m_user.modules);
		gf_cfg_del(m_user.config);
                m_user.config = NULL;
		return Quit(KErrGeneral);
	}

// SOUCHAY : not needed anymore
//	if (first_launch) {
//		/*first launch, register all files ext*/
//		for (u32 i=0; i<gf_modules_get_count(m_user.modules); i++) {
//			GF_InputService *ifce = (GF_InputService *) gf_modules_load_interface(m_user.modules, i, GF_NET_CLIENT_INTERFACE);
//			if (!ifce) continue;
//			if (ifce) {
//				ifce->CanHandleURL(ifce, "test.test");
//				gf_modules_close_interface((GF_BaseInterface *)ifce);
//			}
//		}
//	}

	/*we don't thread the terminal, ie appart from the audio renderer, media decoding and visual rendering is
	handled by the app process*/
	//m_user.init_flags = GF_TERM_NO_VISUAL_THREAD | GF_TERM_NO_REGULATION;
	m_user.init_flags = GF_TERM_NO_THREAD | GF_TERM_NO_REGULATION;
	//m_user.init_flags |= GF_TERM_NO_AUDIO;
	m_user.opaque = this;

	m_user.os_window_handler = m_window;
	m_user.os_display = m_session;
        m_user.EventProc = GPAC_EventProc;
        debug_log("loading terminal...");
        if (!javaVM){
            debug_log("NO JAVA VM FOUND !!!!\n");
            return Quit(KErrGeneral);
        }
        gf_cfg_set_key(m_user.config, "Video", "DriverName", "Droid Video Output");

        gf_cfg_set_key(m_user.config, "Audio", "DriverName", "Droid Audio Output");
	m_term = gf_term_new(&m_user);
        debug_log("term new returned...");
	if (!m_term) {
                debug_log("Cannot load terminal !");
		MessageBox("Cannot load GPAC terminal", "Fatal Error", GF_SERVICE_ERROR);
		gf_modules_del(m_user.modules);
                m_user.modules = NULL;
		gf_cfg_del(m_user.config);
                m_user.config = NULL;
		return Quit(KErrGeneral);
	}


	//setAudioEnvironment(javaVM);

        debug_log("setting term size...");
	gf_term_set_size(m_term, m_Width, m_Height);

	opt = gf_cfg_get_key(m_user.config, "General", "StartupFile");

	char msg[2048];
	snprintf(msg, 2048, "File loaded at startup=%s", opt);
	debug_log(msg);

        if (!urlToLoad)
          urlToLoad = opt;
	if (urlToLoad){
          snprintf(msg, 2048, "Connecting to %s...", urlToLoad);
          debug_log(msg);
          gf_term_connect(m_term, urlToLoad);
        }
        if (m_user.config){
                /*save cfg and reload*/
                gf_cfg_del(m_user.config);
                m_user.config = gf_cfg_new(GPAC_CFG_DIR, "GPAC.cfg");
                if (!m_user.config) {
                        MessageBox("Cannot save initial GPAC Config file", "Fatal Error",GF_SERVICE_ERROR);
                        return Quit(KErrGeneral);
                }
        }
        debug_log("init end");
	//initGL();
        return 0;
}
//-------------------------------
int CNativeWrapper::connect(const char *url){
        if (m_term){
          debug_log("Starting to connect ...");
          gf_term_connect_from_time(m_term, url, 0, false);
          debug_log("connected ...");
        }
}
//-----------------------------------------------------
void CNativeWrapper::disconnect(){
        if (m_term){
          debug_log("disconnecting");
          gf_term_disconnect(m_term);
          debug_log("disconnected ...");
        }
}
//-----------------------------------------------------
void CNativeWrapper::step(void * env, void * bitmap){
	m_window = env;
	m_session = bitmap;
	//debug_log("Step ...");
	if (!m_term){
		debug_log("step(): No m_term found.");
                return;
        } else
          if (!m_term->compositor)
		debug_log("step(): No compositor found.");
          else if (!m_term->compositor->video_out)
		debug_log("step(): No video_out found");
          else if (!m_term->compositor->video_out->Setup)
		debug_log("step(): No video_out->Setup found");
          else {
                debug_log("step(): gf_term_process_step : start()");
                m_term->compositor->frame_draw_type = GF_SC_DRAW_FRAME;
                gf_term_process_step(m_term);
                debug_log("step(): gf_term_process_step : end()");
	}
}

//-----------------------------------------------------
void CNativeWrapper::setAudioEnvironment(JavaVM* javaVM){
        if (!m_term){
            debug_log("setAudioEnvironment(): no m_term found.");
            return;
        }
	debug_log("setAudioEnvironment start");
	m_term->compositor->audio_renderer->audio_out->Setup(m_term->compositor->audio_renderer->audio_out, javaVM, 0, 0);
        debug_log("setAudioEnvironment end");
}
//-----------------------------------------------------
void CNativeWrapper::resize(int w, int h){
        if (!m_term)
          return;
        debug_log("resize start");
	gf_term_set_size(m_term, w, h);
        debug_log("resize end");
}
//-----------------------------------------------------
void CNativeWrapper::onMouseDown(float x, float y){
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

	int ret = gf_term_user_event(m_term, &evt);
        debug_log("onMouseDown end");
}
//-----------------------------------------------------
void CNativeWrapper::onMouseUp(float x, float y){
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

	int ret = gf_term_user_event(m_term, &evt);
        debug_log("onMouseUp end");
}
//-----------------------------------------------------
void CNativeWrapper::onMouseMove(float x, float y){
	if (!m_term)
          return;
	GF_Event evt;
	evt.type = GF_EVENT_MOUSEMOVE;
	evt.mouse.button = GF_MOUSE_LEFT;
	evt.mouse.x = x;
	evt.mouse.y = y;

	int ret = gf_term_user_event(m_term, &evt);
}
//-----------------------------------------------------
void CNativeWrapper::onKeyPress(int keycode, int rawkeycode, int up, int flag){
        if (!m_term)
          return;
        debug_log("onKeyPress start");
	GF_Event evt;
	if (up == 0) evt.type = GF_EVENT_KEYUP;
	else evt.type = GF_EVENT_KEYDOWN;

	evt.key.flags = 0;
	evt.key.hw_code = rawkeycode;

	char msg[256];

	translate_key(keycode, &evt.key);
	//evt.key.key_code = GF_KEY_A;
	int ret = gf_term_user_event(m_term, &evt);
	/*generate a key up*/

	sprintf(msg, "onKeyPress end gpac keycode=%d, GF_KEY_A=%d, ret=%d (original=%d, raw=%d)", evt.key.key_code, GF_KEY_A, ret, keycode, rawkeycode);
	debug_log(msg);
}
//-----------------------------------------------------
void CNativeWrapper::translate_key(ANDROID_KEYCODE keycode, GF_EventKey *evt){
	evt->flags = 0;

	switch (keycode) {
	case ANDROID_KEYCODE_BACK: evt->key_code = GF_KEY_BACKSPACE; break;
	case ANDROID_KEYCODE_TAB: evt->key_code = GF_KEY_TAB; break;
	case ANDROID_KEYCODE_CLEAR: evt->key_code = GF_KEY_CLEAR; break;
	case ANDROID_KEYCODE_ENTER: evt->key_code = GF_KEY_ENTER; break;
	case ANDROID_KEYCODE_SHIFT_LEFT: evt->key_code = GF_KEY_SHIFT; break;
	case ANDROID_KEYCODE_SHIFT_RIGHT: evt->key_code = GF_KEY_SHIFT; break;
	//case VK_CONTROL: evt->key_code = GF_KEY_CONTROL; break;
	case ANDROID_KEYCODE_ALT_LEFT: evt->key_code = GF_KEY_ALT; break;
	case ANDROID_KEYCODE_ALT_RIGHT: evt->key_code = GF_KEY_ALT; break;
	//case VK_PAUSE: evt->key_code = GF_KEY_PAUSE; break;
	//case VK_CAPITAL: evt->key_code = GF_KEY_CAPSLOCK; break;
	//case VK_KANA: evt->key_code = GF_KEY_KANAMODE; break;
	//case VK_JUNJA: evt->key_code = GF_KEY_JUNJAMODE; break;
	//case VK_FINAL: evt->key_code = GF_KEY_FINALMODE; break;
	//case VK_KANJI: evt->key_code = GF_KEY_KANJIMODE; break;
	//case VK_ESCAPE: evt->key_code = GF_KEY_ESCAPE; break;
	//case VK_CONVERT: evt->key_code = GF_KEY_CONVERT; break;
	case ANDROID_KEYCODE_SPACE: evt->key_code = GF_KEY_SPACE; break;
	//case VK_PRIOR: evt->key_code = GF_KEY_PAGEUP; break;
	//case VK_NEXT: evt->key_code = GF_KEY_PAGEDOWN; break;
	//case VK_END: evt->key_code = GF_KEY_END; break;
	case ANDROID_KEYCODE_HOME: evt->key_code = GF_KEY_HOME; break;
	case ANDROID_KEYCODE_DPAD_LEFT: evt->key_code = GF_KEY_LEFT; break;
	case ANDROID_KEYCODE_DPAD_UP: evt->key_code = GF_KEY_UP; break;
	case ANDROID_KEYCODE_DPAD_RIGHT: evt->key_code = GF_KEY_RIGHT; break;
	case ANDROID_KEYCODE_DPAD_DOWN: evt->key_code = GF_KEY_DOWN; break;
	//case VK_SELECT: evt->key_code = GF_KEY_SELECT; break;
	//case VK_PRINT:
	//case VK_SNAPSHOT:
	//	evt->key_code = GF_KEY_PRINTSCREEN; break;
	//case VK_EXECUTE: evt->key_code = GF_KEY_EXECUTE; break;
	//case VK_INSERT: evt->key_code = GF_KEY_INSERT; break;
	case ANDROID_KEYCODE_DEL: evt->key_code = GF_KEY_DEL; break;
	//case VK_HELP: evt->key_code = GF_KEY_HELP; break;


/*	case '!': evt->key_code = GF_KEY_EXCLAMATION; break;
	case '"': evt->key_code = GF_KEY_QUOTATION; break;
	case '#': evt->key_code = GF_KEY_NUMBER; break;
	case '$': evt->key_code = GF_KEY_DOLLAR; break;
	case '&': evt->key_code = GF_KEY_AMPERSAND; break;
	case '\'': evt->key_code = GF_KEY_APOSTROPHE; break;
	case '(': evt->key_code = GF_KEY_LEFTPARENTHESIS; break;
	case ')': evt->key_code = GF_KEY_RIGHTPARENTHESIS; break;
	case ',': evt->key_code = GF_KEY_COMMA; break;
	case ':': evt->key_code = GF_KEY_COLON; break;
	case ';': evt->key_code = GF_KEY_SEMICOLON; break;
	case '<': evt->key_code = GF_KEY_LESSTHAN; break;
	case '>': evt->key_code = GF_KEY_GREATERTHAN; break;
	case '?': evt->key_code = GF_KEY_QUESTION; break;
	case '@': evt->key_code = GF_KEY_AT; break;
	case '[': evt->key_code = GF_KEY_LEFTSQUAREBRACKET; break;
	case ']': evt->key_code = GF_KEY_RIGHTSQUAREBRACKET; break;
	case '\\': evt->key_code = GF_KEY_BACKSLASH; break;
	case '_': evt->key_code = GF_KEY_UNDERSCORE; break;
	case '`': evt->key_code = GF_KEY_GRAVEACCENT; break;
	case ' ': evt->key_code = GF_KEY_SPACE; break;
	case '/': evt->key_code = GF_KEY_SLASH; break;
	case '*': evt->key_code = GF_KEY_STAR; break;
	case '-': evt->key_code = GF_KEY_HIPHEN; break;
	case '+': evt->key_code = GF_KEY_PLUS; break;
	case '=': evt->key_code = GF_KEY_EQUALS; break;
	case '^': evt->key_code = GF_KEY_CIRCUM; break;
	case '{': evt->key_code = GF_KEY_LEFTCURLYBRACKET; break;
	case '}': evt->key_code = GF_KEY_RIGHTCURLYBRACKET; break;
	case '|': evt->key_code = GF_KEY_PIPE; break;
*/


/*	case VK_LWIN: return ;
	case VK_RWIN: return ;
	case VK_APPS: return ;
*/
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
	case ANDROID_KEYCODE_A:
		evt->key_code = GF_KEY_9;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	/*
	case VK_MULTIPLY:
		evt->key_code = GF_KEY_STAR;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_ADD:
		evt->key_code = GF_KEY_PLUS;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_SEPARATOR:
		evt->key_code = GF_KEY_FULLSTOP;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_SUBTRACT:
		evt->key_code = GF_KEY_HYPHEN;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_DECIMAL:
		evt->key_code = GF_KEY_COMMA;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_DIVIDE:
		evt->key_code = GF_KEY_SLASH;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;

	case VK_F1: evt->key_code = GF_KEY_F1; break;
	case VK_F2: evt->key_code = GF_KEY_F2; break;
	case VK_F3: evt->key_code = GF_KEY_F3; break;
	case VK_F4: evt->key_code = GF_KEY_F4; break;
	case VK_F5: evt->key_code = GF_KEY_F5; break;
	case VK_F6: evt->key_code = GF_KEY_F6; break;
	case VK_F7: evt->key_code = GF_KEY_F7; break;
	case VK_F8: evt->key_code = GF_KEY_F8; break;
	case VK_F9: evt->key_code = GF_KEY_F9; break;
	case VK_F10: evt->key_code = GF_KEY_F10; break;
	case VK_F11: evt->key_code = GF_KEY_F11; break;
	case VK_F12: evt->key_code = GF_KEY_F12; break;
	case VK_F13: evt->key_code = GF_KEY_F13; break;
	case VK_F14: evt->key_code = GF_KEY_F14; break;
	case VK_F15: evt->key_code = GF_KEY_F15; break;
	case VK_F16: evt->key_code = GF_KEY_F16; break;
	case VK_F17: evt->key_code = GF_KEY_F17; break;
	case VK_F18: evt->key_code = GF_KEY_F18; break;
	case VK_F19: evt->key_code = GF_KEY_F19; break;
	case VK_F20: evt->key_code = GF_KEY_F20; break;
	case VK_F21: evt->key_code = GF_KEY_F21; break;
	case VK_F22: evt->key_code = GF_KEY_F22; break;
	case VK_F23: evt->key_code = GF_KEY_F23; break;
	case VK_F24: evt->key_code = GF_KEY_F24; break;

	case VK_NUMLOCK: evt->key_code = GF_KEY_NUMLOCK; break;
	case VK_SCROLL: evt->key_code = GF_KEY_SCROLL; break;
	*/

/*
 * VK_L* & VK_R* - left and right Alt, Ctrl and Shift virtual keys.
 * Used only as parameters to GetAsyncKeyState() and GetKeyState().
 * No other API or message will distinguish left and right keys in this way.
 */
	/*
	case ANDROID_KEYCODE_SHIFT_LEFT:
		evt->key_code = GF_KEY_SHIFT;
		evt->flags = GF_KEY_EXT_LEFT;
		break;
	case ANDROID_KEYCODE_SHIFT_RIGHT:
		evt->key_code = GF_KEY_SHIFT;
		evt->flags = GF_KEY_EXT_RIGHT;
		break;
	case VK_LCONTROL:
		evt->key_code = GF_KEY_CONTROL;
		evt->flags = GF_KEY_EXT_LEFT;
		break;
	case VK_RCONTROL:
		evt->key_code = GF_KEY_CONTROL;
		evt->flags = GF_KEY_EXT_RIGHT;
		break;
	case VK_LMENU:
		evt->key_code = GF_KEY_ALT;
		evt->flags = GF_KEY_EXT_LEFT;
		break;
	case VK_RMENU:
		evt->key_code = GF_KEY_ALT;
		evt->flags = GF_KEY_EXT_RIGHT;
		break;
	*/

	/*thru VK_9 are the same as ASCII '0' thru '9' (0x30 - 0x39) */
	/* VK_A thru VK_Z are the same as ASCII 'A' thru 'Z' (0x41 - 0x5A) */
	default:
		if ((keycode>=ANDROID_KEYCODE_A) && (keycode<=ANDROID_KEYCODE_Z))  { evt->key_code = GF_KEY_A + keycode - ANDROID_KEYCODE_A; debug_log("default keycode"); }
		else
			evt->key_code = GF_KEY_UNIDENTIFIED;
		break;
	}

	//evt->hw_code = keycode;
	evt->hw_code = evt->key_code;
}

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------