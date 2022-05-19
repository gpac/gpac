/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Ivica Arsov
 *			Copyright (c) Telecom Paris 2010-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / android JNI wrapper for player
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
#include <android/log.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>

#include <gpac/thread.h>
#include <gpac/network.h>
#include <gpac/tools.h>
#include <gpac/filters.h>
#include <gpac/compositor.h>

#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, "GPAC",  __VA_ARGS__)
#define LOGW(...)  __android_log_print(ANDROID_LOG_WARN, "GPAC",  __VA_ARGS__)


#define GET_GPAC_JNI(_e)	 jclass c = env->GetObjectClass(obj);\
                        if (!c) return _e;\
                        jfieldID fid = env->GetFieldID(c, "gpac_jni_handle", "J");\
                        if (!fid) {\
                          LOGE("No Field ID, ERROR");\
                          return _e;\
                        }\
                        jlong h = env->GetLongField(obj, fid);\
                        GPAC_JNI *gpac = (GPAC_JNI *)h;\
                        if (!gpac) return _e;


typedef struct _JavaEnvTh {
	JNIEnv * env;
	u32 javaThreadId;
	//callbacks into GPAC
	jobject cbk_obj;
	jmethodID cbk_messageDialog;
	jmethodID cbk_onProgress;
	jmethodID cbk_showKeyboard;
	jmethodID cbk_sensorSwitch;
	jmethodID cbk_setOrientation;
} JavaEnvTh;

static JavaVM* javaVM = NULL;
static pthread_key_t jni_thread_env_key = 0;

typedef struct
{
	//GPAC class instance for callbacks
	jobject gpac_cbk_obj;

	GF_FilterSession *fsess;
	GF_Compositor *compositor;

	u32 last_width, last_height;

	Bool has_display_cutout;
	int	no_gui;
	int	do_log;
	FILE *log_file;

	char *log_buf;
	u32 log_buf_size;

	char out_std[GF_MAX_PATH];
} GPAC_JNI;

static Bool gpac_jni_event_proc(void *cbk, GF_Event *evt);
static void gpac_jni_on_log(void *cbk, GF_LOG_Level ll, GF_LOG_Tool lm, const char *fmt, va_list list);
static void translate_key_to_gpac(int android_keycode, GF_EventKey *evt);
static void on_progress_cbk(const void *usr, const char *title, u64 done, u64 total);

#ifdef __cplusplus
extern "C" {
#endif

int gpac_main(int argc, char **argv);
int mp4box_main(int argc, char **argv);
void gpac_abort(void);

#ifdef __cplusplus
}
#endif


void gpac_jni_set_java_env(JavaEnvTh * envToSet, JNIEnv *env, jobject callback)
{
	assert( envToSet );
	jclass localRef = env->GetObjectClass(callback);
	envToSet->env = env;
	envToSet->javaThreadId = gf_th_id();
	envToSet->cbk_obj = callback;
	envToSet->cbk_messageDialog = env->GetMethodID(localRef, "messageDialog", "(Ljava/lang/String;Ljava/lang/String;I)V");
	envToSet->cbk_onProgress = env->GetMethodID(localRef, "onProgress", "(Ljava/lang/String;II)V");
	envToSet->cbk_showKeyboard = env->GetMethodID(localRef, "showKeyboard", "(Z)V");
	envToSet->cbk_sensorSwitch = env->GetMethodID(localRef, "sensorSwitch", "(IZ)V");
	envToSet->cbk_setOrientation = env->GetMethodID(localRef, "setOrientation", "(I)V");
	env->DeleteLocalRef(localRef);
}

JavaEnvTh * gpac_jni_get_env(GPAC_JNI *gpac_jni)
{
	JNIEnv *env;
	JavaEnvTh * javaEnv;
	if (!javaVM) {
		LOGE("No JVM Found");
		return NULL;
	}
	javaEnv = (JavaEnvTh*) pthread_getspecific( jni_thread_env_key );
	if (javaEnv) return javaEnv;
	javaEnv = (JavaEnvTh *) malloc(sizeof(JavaEnvTh));
	if (!javaEnv) return NULL;

	memset(javaEnv, 0, sizeof(JavaEnvTh));
	javaVM->AttachCurrentThread(&env, NULL);
	if (!env) {
		LOGE("Attaching to thread did failed for thread id=%d", gf_th_id());
		free(javaEnv);
		return NULL;
	}

	gpac_jni_set_java_env(javaEnv, env, gpac_jni->gpac_cbk_obj);
	if (pthread_setspecific(jni_thread_env_key, javaEnv)) {
		LOGE("Failed to set specific thread data to jni_thread_env_key for thread=%d. No ENV available !", gf_th_id());
		free(javaEnv);
		return NULL;
	}
	return javaEnv;
}


int show_message(GPAC_JNI *gpac, const char* msg, const char* title, GF_Err status)
{
	JavaEnvTh * env = gpac_jni_get_env(gpac);
	if (!env || !env->cbk_messageDialog) return 0;

	env->env->PushLocalFrame(2);
	jstring _title = title ? env->env->NewStringUTF(title) : NULL;
	jstring _msg = msg ? env->env->NewStringUTF(msg) : NULL;
	env->env->CallVoidMethod(env->cbk_obj, env->cbk_messageDialog, _msg, _title, status);
	env->env->PopLocalFrame(NULL);
	return 1;
}

static GF_Err gpac_init_gui(GPAC_JNI *gpac)
{
	GF_Filter *comp_filter=NULL;
	const char *opt=NULL;
	GF_Err e = GF_OK;

	gf_sys_init(GF_MemTrackerNone, NULL);

	if (!gf_opts_get_key("core", "threads"))
		gf_opts_set_key("temp", "threads", "2");

	opt = gf_opts_get_key("core", "log-file");
	if (opt && strcmp(opt, "system") ) {
		gpac->log_file = gf_fopen(opt, "wt");
		if (!gpac->log_file) opt = "system";
	}
	if (opt) {
		gf_log_set_tools_levels( gf_opts_get_key("core", "logs"), GF_TRUE );
		gf_log_set_callback(gpac, gpac_jni_on_log);
		gpac->do_log = 1;
	}
	gf_opts_set_key("temp", "display-cutout", gpac->has_display_cutout ? "yes" : "no");

	opt = gf_opts_get_key("core", "startup-file");
	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("GUI path is %s\n", opt ? opt : "NOT SET!") );
	gpac->no_gui = 0;
	if (!opt || !gf_file_exists(opt)) {
		gpac->no_gui = 1;
		//if no GUI, redirect progress events
		gf_set_progress_callback(gpac, on_progress_cbk);
	}

	//run in non blocking mode
	gpac->fsess = gf_fs_new_defaults(GF_FS_FLAG_NON_BLOCKING);
	if (!gpac->fsess) return GF_OUT_OF_MEM;

	comp_filter = gf_fs_load_filter(gpac->fsess, "compositor:player=gui", &e);
	if (!comp_filter) return e ? e : GF_OUT_OF_MEM;

	gpac->compositor = (GF_Compositor *) gf_filter_get_udta(comp_filter);
	if (!gpac->compositor) return GF_OUT_OF_MEM;

	/*force fullscreen*/
	gf_sc_set_option(gpac->compositor, GF_OPT_FULLSCREEN, 1);
	gf_sc_set_size(gpac->compositor, gpac->last_width, gpac->last_height);
	gf_fs_set_ui_callback(gpac->fsess, gpac_jni_event_proc, gpac);

	//prevent destruction of JSRT until we unload gpac
	gf_opts_set_key("temp", "static-jsrt", "true");

	return GF_OK;
}

JNIEXPORT jlong JNICALL gpac_jni_init(JNIEnv * env, jclass obj, jobject gpac_class_obj, jint width, jint height, jboolean has_display_cutout, jstring _url_to_open, jstring _ext_storage, jstring _app_data_dir)
{
	GF_Err e = GF_OK;
	int success = 1;
	jboolean isCopy;
	const char *ext_storage = NULL, *app_data_dir=NULL, *url_to_open=NULL;

	if (!javaVM) {
		LOGE("NO JAVA VM FOUND !\n");
		return (jlong) NULL;
	}

	if (_ext_storage)
		ext_storage = env->GetStringUTFChars(_ext_storage, &isCopy);

	if (_app_data_dir)
		app_data_dir = env->GetStringUTFChars(_app_data_dir, &isCopy);

	if (_url_to_open)
		url_to_open = env->GetStringUTFChars(_url_to_open, &isCopy);


	GPAC_JNI *gpac = (GPAC_JNI *) malloc(sizeof(GPAC_JNI));
	if (!gpac) { success = 0; goto exit; }
	memset(gpac, 0, sizeof(GPAC_JNI));

	//keep a global ref to the java class instance
	gpac->gpac_cbk_obj = env->NewGlobalRef(gpac_class_obj);

	gpac->last_width = width;
	gpac->last_height = height;
	gpac->has_display_cutout = has_display_cutout ? GF_TRUE : GF_FALSE;
	gf_sys_set_android_paths(app_data_dir, ext_storage);

	e = gpac_init_gui(gpac);
	if (e) { success = 0; goto exit; }

	if (url_to_open) {
		if (gpac->no_gui) {
			GF_Event evt;
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_NAVIGATE;
			evt.navigate.to_url = url_to_open;
			gf_sc_user_event(gpac->compositor, &evt);
		} else {
			gf_opts_set_key("temp", "gui_load_urls", url_to_open);
		}
	}

	gpac->out_std[0] = 0;
	if (ext_storage) {
		strcpy(gpac->out_std, ext_storage);
		strcat(gpac->out_std, "/GPAC/");
		if (!gf_dir_exists(gpac->out_std)) {
			gpac->out_std[0] = 0;
		} else {
			strcat(gpac->out_std, "/stderr_stdout.txt");
		}
	}

exit:

	if (app_data_dir)
		env->ReleaseStringUTFChars(_app_data_dir, app_data_dir);
	if (ext_storage)
		env->ReleaseStringUTFChars(_ext_storage, ext_storage);
	if (url_to_open)
		env->ReleaseStringUTFChars(_url_to_open, url_to_open);

	if (!success) {
		if (!e) e = GF_OUT_OF_MEM;
		if (gpac) {
			show_message(gpac, "Cannot load GPAC", "Fatal Error", e);
			if (gpac->fsess) gf_fs_del(gpac->fsess);
			free(gpac);
			gpac = NULL;
		}
		LOGE("Cannot load GPAC filter session");
	}
	return (jlong) gpac;
}

static void gpac_uninit(GPAC_JNI *gpac)
{
	if (!gpac->fsess)
		return;

	GF_FilterSession *fs = gpac->fsess;
	gpac->fsess = NULL;
	gf_fs_abort(fs, GF_FS_FLUSH_FAST);
	gf_fs_del(fs);
	gpac->compositor = NULL;

	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("GPAC shutdown\n"));
	if (gpac->log_file) gf_fclose(gpac->log_file);
	gpac->log_file = NULL;
	gf_log_set_callback(NULL, NULL);

	gf_sys_close();
}

JNIEXPORT void JNICALL gpac_jni_uninit(JNIEnv * env, jobject obj)
{
	GET_GPAC_JNI( )

	//final destruction, unload JSRT if any
	gf_opts_set_key("temp", "static-jsrt", "false");

	gpac_uninit(gpac);

	//shutdown JNI
	JavaEnvTh *th_env = gpac_jni_get_env(gpac);
	if (th_env)
		th_env->env->DeleteGlobalRef(gpac->gpac_cbk_obj);

	if (gpac->log_buf) free(gpac->log_buf);
	free(gpac);
}


static int start_logger(GPAC_JNI *gpac);
static void stop_logger(GPAC_JNI *gpac);


JNIEXPORT jint JNICALL gpac_jni_load_service(JNIEnv * env, jobject obj, jstring fileName)
{
	int ret_val = 0;
	GET_GPAC_JNI(-1)

	jboolean isCopy;
	const char *service_url = env->GetStringUTFChars(fileName, &isCopy);

	//url starting with "gpac " , run gpac command line
	if (service_url
		&& (!strnicmp(service_url, "gpac ", 5) || !strnicmp(service_url, "mp4box ", 7) )
	) {
		gpac_uninit(gpac);

		u32 in_quote_s = 0;
		u32 in_quote_d = 0;
		u32 argc = 0;
		u32 alen;
		char *arg;
		char **argv = NULL;
		u32 start_pos = 0;
		u32 i, len = (u32) strlen(service_url);
		for (i=0; i<len; i++) {
			char c = service_url[i];
			if (c != ' ') {
				if (c=='\'') in_quote_s = in_quote_s ? 0 : 1;
				if (c=='"') in_quote_d = in_quote_d ? 0 : 1;
				if (!start_pos) {
					start_pos = i+1;
				}
				continue;
			}
			if (in_quote_d || in_quote_d) continue;
			if (!start_pos) continue;

			alen = (u32) (i - (start_pos-1));
			argv = (char **) realloc(argv, sizeof(char *)*(argc + 1));
			char *arg = (char *) malloc(sizeof(char)*(alen + 1));
			memcpy(arg, service_url+start_pos-1, alen);
			arg[alen] = 0;
			argv[argc] = arg;
			argc++;
			start_pos=0;
		}
		if (start_pos) {
			alen = (u32) (len - (start_pos-1));
			argv = (char **) realloc(argv, sizeof(char *)*(argc + 1));
			arg = (char *) malloc(sizeof(char)*(alen + 1));
			memcpy(arg, service_url+start_pos-1, alen);
			arg[alen] = 0;
			argv[argc] = arg;
			argc++;
		}

		//always set log callback when running a filter chain
		gf_log_set_callback(gpac, gpac_jni_on_log);

		start_logger(gpac);

		if (!strnicmp(service_url, "mp4box ", 7)) {
			ret_val = mp4box_main(argc, argv);
		} else {
			ret_val = gpac_main(argc, argv);
		}
		stop_logger(gpac);

		for (i=0; i<argc; i++) {
			free(argv[i]);
		}
		free(argv);
	} else {
		Bool send_nav_event = GF_FALSE;
		//not running in GUI mode, set it
		if (!gpac->compositor) {
			gpac_uninit(gpac);

			gpac_init_gui(gpac);
			if (gpac->no_gui) {
				send_nav_event = GF_TRUE;
			} else {
				gf_opts_set_key("temp", "gui_load_urls", service_url);
			}
		}
		//otherwise, already running in GUI mode, send navigate event
		else {
			send_nav_event = GF_TRUE;
		}

		if (send_nav_event && service_url) {
			GF_Event evt;
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_NAVIGATE;
			evt.navigate.to_url = service_url;
			gf_sc_user_event(gpac->compositor, &evt);
		}
	}

	if (service_url)
		env->ReleaseStringUTFChars(fileName, service_url);

	return ret_val;
}

JNIEXPORT void JNICALL gpac_jni_render_frame(JNIEnv * env, jobject obj)
{
	GET_GPAC_JNI( )

	//force frame draw type
	gf_sc_invalidate_next_frame(gpac->compositor);
	while (1) {
		gf_fs_run(gpac->fsess);
		if (gf_sc_frame_was_produced(gpac->compositor)) break;
	}
}

JNIEXPORT void JNICALL gpac_jni_set_size(JNIEnv * env, jobject obj, jint width, jint height, jint orientation)
{
	GET_GPAC_JNI( )

	gpac->last_width = (u32) width;
	gpac->last_height = (u32) height;

	GF_Event evt;
	memset(&evt, 0, sizeof(GF_Event));
	evt.size.type = GF_EVENT_SIZE;
	evt.size.width = (u32) width;
	evt.size.height = (u32) height;
	evt.size.orientation = (GF_DisplayOrientationType) orientation;
	gf_sc_user_event(gpac->compositor, &evt);
}

JNIEXPORT void JNICALL gpac_jni_eventmousedown(JNIEnv * env, jobject obj, jfloat x, jfloat y)
{
	GET_GPAC_JNI( )

	GF_Event evt;
	evt.type = GF_EVENT_MOUSEDOWN;
	evt.mouse.button = GF_MOUSE_LEFT;
	evt.mouse.x = x;
	evt.mouse.y = y;

	gf_sc_user_event(gpac->compositor, &evt);
}

JNIEXPORT void JNICALL gpac_jni_eventmouseup(JNIEnv * env, jobject obj, jfloat x, jfloat y)
{
	GET_GPAC_JNI( )

	GF_Event evt;
	evt.type = GF_EVENT_MOUSEUP;
	evt.mouse.button = GF_MOUSE_LEFT;
	evt.mouse.x = x;
	evt.mouse.y = y;

	gf_sc_user_event(gpac->compositor, &evt);
}

JNIEXPORT void JNICALL gpac_jni_eventmousemove(JNIEnv * env, jobject obj, jfloat x, jfloat y)
{
	GET_GPAC_JNI( )

	GF_Event evt;
	evt.type = GF_EVENT_MOUSEMOVE;
	evt.mouse.button = GF_MOUSE_LEFT;
	evt.mouse.x = x;
	evt.mouse.y = y;

	gf_sc_user_event(gpac->compositor, &evt);
}

JNIEXPORT void JNICALL gpac_jni_eventkeypress(JNIEnv * env, jobject obj, jint keycode, jint rawkeycode, jint up, jint flag, jint unicode)
{
	GET_GPAC_JNI( )

	GF_Event evt;
	memset(&evt, 0, sizeof(GF_Event));
	if (up == 0) evt.type = GF_EVENT_KEYUP;
	else evt.type = GF_EVENT_KEYDOWN;

	evt.key.flags = 0;
	evt.key.hw_code = rawkeycode;

	translate_key_to_gpac(keycode, &evt.key);
	gf_sc_user_event(gpac->compositor, &evt);

	if (evt.type == GF_EVENT_KEYUP && unicode) {
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_TEXTINPUT;
		evt.character.unicode_char = unicode;
		gf_sc_user_event(gpac->compositor, &evt);
	}
}

JNIEXPORT void JNICALL gpac_jni_eventorientationchange(JNIEnv * env, jobject obj, jfloat x, jfloat y, jfloat z)
{
	GET_GPAC_JNI( )

	GF_Event evt;
	evt.type = GF_EVENT_SENSOR_ORIENTATION;
	evt.sensor.x = x;
	evt.sensor.y = y;
	evt.sensor.z = z;
	evt.sensor.w = 0;

	gf_sc_user_event(gpac->compositor, &evt);
}


JNIEXPORT void JNICALL gpac_jni_eventlocationchange(JNIEnv * env, jobject obj, jdouble longitude, jdouble latitude, jdouble altitude, jfloat bearing, jfloat accuracy)
{
	GET_GPAC_JNI( )

	GF_Event evt;
	evt.type = GF_EVENT_SENSOR_GPS;
	evt.gps.longitude = longitude;
	evt.gps.latitude = latitude;
	evt.gps.altitude = altitude;
	evt.gps.bearing = bearing;
	evt.gps.accuracy = accuracy;
	gf_sc_user_event(gpac->compositor, &evt);
}

JNIEXPORT jstring JNICALL gpac_jni_error_to_string(JNIEnv * env, jobject obj, jint code)
{
	const char *str = gf_error_to_string((GF_Err) (s32) code);

	jstring result = env->NewStringUTF(str);
    return result;
}

JNIEXPORT void JNICALL gpac_jni_abort_session(JNIEnv * env, jobject obj)
{
	gpac_abort();
}

JNIEXPORT void JNICALL gpac_jni_reset_config(JNIEnv * env, jobject obj)
{
	gf_opts_set_key("core", "reset", "yes");
}

//from  https://codelab.wordpress.com/2014/11/03/how-to-use-standard-output-streams-for-logging-in-android-apps/
#include <unistd.h>
static int pipe_fd[2];
static int run_logger = 0;
static FILE *f_std_err_out=NULL;

static void *logger_thread(void*_param)
{
	GPAC_JNI *gpac = (GPAC_JNI *)_param;
    ssize_t read_size;
    char buf[1025];
    while ((read_size = read(pipe_fd[0], buf, 1023)) > 0) {
		if (!run_logger) break;

        buf[read_size] = 0;
		show_message(gpac, buf, NULL, GF_OK);
		if (f_std_err_out)
			fwrite(buf, 1, read_size, f_std_err_out);
    }
    run_logger = 0;
    return 0;
}

static pthread_t thr;

static int start_logger(GPAC_JNI *gpac)
{
	char szPath[GF_MAX_PATH];
	//reopen stderr and stdout
	if (gpac->out_std[0]) {
		f_std_err_out = fopen(gpac->out_std, "w");
	}

    /* make stdout and stderr line-buffered*/
    setvbuf(stdout, 0, _IOLBF, 0);
    setvbuf(stderr, 0, _IOLBF, 0);

    /* create the pipe and redirect stdout and stderr */
    pipe(pipe_fd);
    dup2(pipe_fd[1], 1);
    dup2(pipe_fd[1], 2);

	run_logger = 1;
    if (pthread_create(&thr, 0, logger_thread, gpac) == -1) {
		run_logger = 0;
        return -1;
	}
    pthread_detach(thr);
    return 0;
}
static void stop_logger(GPAC_JNI *gpac)
{
	fflush(stderr);
	fflush(stdout);
	//give logger 100ms to flush everything
	gf_sleep(100);
	close(pipe_fd[0]);
	close(pipe_fd[1]);

	//wait for 1s for logger thread to exit
	u32 nb_retry = 9;
	while (run_logger && nb_retry) {
		gf_sleep(100);
		nb_retry--;
	}
	run_logger = 0;
	if (f_std_err_out) {
		fclose(f_std_err_out);
		f_std_err_out = NULL;
	}
}

static void gpac_jni_on_log(void *cbk, GF_LOG_Level ll, GF_LOG_Tool lm, const char *fmt, va_list list)
{
	GPAC_JNI *gpac = (GPAC_JNI *) cbk;

	if (!gpac->compositor) {
		va_list vlist_tmp;

		va_copy(vlist_tmp, list);
		u32 len = (u32) vsnprintf(NULL, 0, fmt, vlist_tmp);
		va_end(vlist_tmp);
		if (gpac->log_buf_size < len+2) {
			gpac->log_buf_size = len+2;
			gpac->log_buf = (char *) realloc(gpac->log_buf, gpac->log_buf_size);
		}

		va_copy(vlist_tmp, list);
		vsprintf(gpac->log_buf, fmt, vlist_tmp);
		va_end(vlist_tmp);
		show_message(gpac, gpac->log_buf, NULL, GF_OK);
	}
	if (!gpac->do_log) return;


	if (gpac && gpac->log_file) {
		vfprintf(gpac->log_file, fmt, list);
		fflush(gpac->log_file);
	} else {
		char szTag[50];
		int level;
		if (ll == GF_LOG_ERROR) level = ANDROID_LOG_ERROR;
		else if (ll == GF_LOG_WARNING) level = ANDROID_LOG_WARN;
		else if (ll == GF_LOG_INFO) level = ANDROID_LOG_INFO;
		else level = ANDROID_LOG_DEBUG;

		sprintf(szTag, "GPAC@%s", gf_log_tool_name(lm) );
		__android_log_vprint(level, szTag, fmt, list);
	}
}

static void on_progress_cbk(const void *usr, const char *title, u64 done, u64 total)
{
	if (!usr) return;
	JavaEnvTh *env = gpac_jni_get_env( (GPAC_JNI *) usr );
	if (!env || !env->cbk_onProgress) return;

	env->env->PushLocalFrame(1);
	jstring js = env->env->NewStringUTF(title);
	env->env->CallVoidMethod(env->cbk_obj, env->cbk_onProgress, js, done, total);
	env->env->PopLocalFrame(NULL);
}

static Bool gpac_jni_event_proc(void *cbk, GF_Event *evt)
{
	if (!cbk) return GF_FALSE;
	GPAC_JNI *gpac = (GPAC_JNI *) cbk;

	switch (evt->type) {
	case GF_EVENT_MESSAGE:
		GF_LOG(evt->message.error ? GF_LOG_ERROR : GF_LOG_INFO, GF_LOG_APP, ("%s: %s",
				evt->message.message ? evt->message.message : "Error",
				gf_error_to_string(evt->message.error)
		));
		break;
	case GF_EVENT_CONNECT:
		if (evt->connect.is_connected)
			show_message(gpac, "Connected", "Connected to scene", GF_OK);
		else
			show_message(gpac, "Disconnected", "Disconnected from scene.", GF_OK);
		break;
	case GF_EVENT_PROGRESS:
		if (evt->progress.progress_type==0)
			on_progress_cbk(gpac, "Buffering", evt->progress.done, evt->progress.total);
		else if (evt->progress.progress_type==1)
			on_progress_cbk(gpac, "Downloading", evt->progress.done, evt->progress.total);
		else if (evt->progress.progress_type==2)
			on_progress_cbk(gpac, "Import", evt->progress.done, evt->progress.total);
		break;
	case GF_EVENT_TEXT_EDITING_START:
	case GF_EVENT_TEXT_EDITING_END:
	{
		JavaEnvTh * env = gpac_jni_get_env(gpac);
		if (!env || !env->cbk_showKeyboard) return GF_FALSE;
		env->env->CallVoidMethod(env->cbk_obj, env->cbk_showKeyboard, GF_EVENT_TEXT_EDITING_START == evt->type);
	}
		break;
	case GF_EVENT_EOS:
		break;
	case GF_EVENT_DISCONNECT:
		break;
	case GF_EVENT_NAVIGATE:
		if (gf_sc_is_supported_url(gpac->compositor, evt->navigate.to_url, GF_TRUE)) {
			gf_sc_navigate_to(gpac->compositor, evt->navigate.to_url);
		}
		break;
	case GF_EVENT_SENSOR_REQUEST:
		if ((evt->activate_sensor.sensor_type == GF_EVENT_SENSOR_ORIENTATION)
			|| (evt->activate_sensor.sensor_type == GF_EVENT_SENSOR_GPS)
		) {
			JavaEnvTh * env = gpac_jni_get_env(gpac);
			if (!env || !env->cbk_sensorSwitch) return GF_FALSE;

			env->env->CallVoidMethod(env->cbk_obj, env->cbk_sensorSwitch, (evt->activate_sensor.sensor_type == GF_EVENT_SENSOR_GPS) ? 1 : 0, evt->activate_sensor.activate);
			return GF_TRUE;
		}
		break;
	case GF_EVENT_SET_ORIENTATION:
		{
			JavaEnvTh * env = gpac_jni_get_env(gpac);
			if (!env || !env->cbk_sensorSwitch) return GF_FALSE;

			env->env->CallVoidMethod(env->cbk_obj, env->cbk_setOrientation, evt->size.orientation);
			return GF_TRUE;
		}
		break;
	//ignore all the rest
	default:
		break;
	}
	return GF_FALSE;
}


/**
 * This method is called when a pthread is destroyed, so we can delete the JNI env
 */
static void jni_destroy_env_func(void * env)
{
	JavaEnvTh * jniEnv = (JavaEnvTh *) env;
	if (jniEnv) {
		memset(jniEnv, 0, sizeof(JavaEnvTh));
		gf_free(jniEnv);
	}
	pthread_setspecific(jni_thread_env_key, NULL);
	if (javaVM)
		javaVM->DetachCurrentThread();
}

static int jniRegisterNativeMethods(JNIEnv* env, const char* className, const JNINativeMethod* gMethods, int numMethods)
{
	int res;
	jclass clazz;
	clazz = env->FindClass(className);
	if (clazz == NULL) {
		LOGE("Native registration unable to find class '%s'\n", className);
		return -1;
	}
	res = env->RegisterNatives(clazz, gMethods, numMethods);
	if (res < 0) {
		LOGE("RegisterNatives failed for '%s'\n", className);
		return res;
	}
	return 0;
}

/* name, signature, funcPtr */
static JNINativeMethod sMethods[] = {
	{	"init", "(Lio/gpac/gpac/GPAC;IIZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)J", (void*)&gpac_jni_init},
	{	"uninit", "()V", (void*)gpac_jni_uninit},
	{	"render_frame", "()V", (void*)gpac_jni_render_frame},
	{	"set_size", "(III)V", (void*)gpac_jni_set_size},
	{	"eventkeypress", "(IIIII)V", (void*)gpac_jni_eventkeypress},
	{	"eventmousedown", "(FF)V", (void*)gpac_jni_eventmousedown},
	{	"eventmouseup", "(FF)V", (void*)gpac_jni_eventmouseup},
	{	"eventmousemove", "(FF)V", (void*)gpac_jni_eventmousemove},
	{	"eventorientationchange", "(FFF)V", (void*)gpac_jni_eventorientationchange},
	{	"eventlocationchange", "(DDDFF)V", (void*)gpac_jni_eventlocationchange},
	{	"load_service", "(Ljava/lang/String;)I", (void*)gpac_jni_load_service},
	{	"error_to_string", "(I)Ljava/lang/String;", (void*)gpac_jni_error_to_string},
	{	"abort_session", "()V", (void*)gpac_jni_abort_session},
	{	"reset_config", "()V", (void*)gpac_jni_reset_config},
	NULL
};


jint JNI_OnUnLoad(JavaVM* vm, void* reserved)
{
	if (pthread_key_delete(jni_thread_env_key)) {
		LOGW("Failed to delete key jni_thread_env_key jni_thread_env_key=%d\n", jni_thread_env_key);
	}
	javaVM = NULL;
	jni_thread_env_key = (int) NULL;
	return 0;
}

#define NUM_JNI_VERSIONS 4

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	int res;
	int jniV;
	int allowedVersions[NUM_JNI_VERSIONS];
	const char * className = "io/gpac/gpac/GPAC";
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

	res = jniRegisterNativeMethods(env, className, sMethods, (sizeof(sMethods) - 1)/sizeof(JNINativeMethod));
	if (res < 0) {
		LOGE("Failed to register native methods, result was = %d, try to continue anyway...\n", res);
		/*return -1;*/
	}

	int ret = pthread_key_create(&jni_thread_env_key, &jni_destroy_env_func);
	if (ret) {
		LOGE("Failed to register jni_thread_env_key jni_thread_env_key=%d\n", jni_thread_env_key);
	}
	return jniV;
}

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


// keyboard code
#define ANDROID_KEYCODE_0	7
#define ANDROID_KEYCODE_1	8
#define ANDROID_KEYCODE_2	9
#define ANDROID_KEYCODE_3	10
#define ANDROID_KEYCODE_4	11
#define ANDROID_KEYCODE_5	12
#define ANDROID_KEYCODE_6	13
#define ANDROID_KEYCODE_7	14
#define ANDROID_KEYCODE_8	15
#define ANDROID_KEYCODE_9	16
#define ANDROID_KEYCODE_A	29
#define ANDROID_KEYCODE_B	30
#define ANDROID_KEYCODE_C	31
#define ANDROID_KEYCODE_D	32
#define ANDROID_KEYCODE_E	33
#define ANDROID_KEYCODE_F	34
#define ANDROID_KEYCODE_G	35
#define ANDROID_KEYCODE_H	36
#define ANDROID_KEYCODE_I	37
#define ANDROID_KEYCODE_J	38
#define ANDROID_KEYCODE_K	39
#define ANDROID_KEYCODE_L	40
#define ANDROID_KEYCODE_M	41
#define ANDROID_KEYCODE_N	42
#define ANDROID_KEYCODE_O	43
#define ANDROID_KEYCODE_P	44
#define ANDROID_KEYCODE_Q	45
#define ANDROID_KEYCODE_R	46
#define ANDROID_KEYCODE_S	47
#define ANDROID_KEYCODE_T	48
#define ANDROID_KEYCODE_U	49
#define ANDROID_KEYCODE_V	50
#define ANDROID_KEYCODE_W	51
#define ANDROID_KEYCODE_X	52
#define ANDROID_KEYCODE_Y	53
#define ANDROID_KEYCODE_Z	54
#define ANDROID_KEYCODE_ALT_LEFT	57
#define ANDROID_KEYCODE_ALT_RIGHT	58
#define ANDROID_KEYCODE_AT		77
#define ANDROID_KEYCODE_BACK		4
#define ANDROID_KEYCODE_BACKSLASH	73
#define ANDROID_KEYCODE_CALL		5
#define ANDROID_KEYCODE_CAMERA		27
#define ANDROID_KEYCODE_CLEAR		28
#define ANDROID_KEYCODE_COMMA		55
#define ANDROID_KEYCODE_DEL		67
#define ANDROID_KEYCODE_DPAD_CENTER	23
#define ANDROID_KEYCODE_DPAD_DOWN	20
#define ANDROID_KEYCODE_DPAD_LEFT	21
#define ANDROID_KEYCODE_DPAD_RIGHT	22
#define ANDROID_KEYCODE_DPAD_UP		19
#define ANDROID_KEYCODE_ENDCALL		6
#define ANDROID_KEYCODE_ENTER		66
#define ANDROID_KEYCODE_ENVELOPE	65
#define ANDROID_KEYCODE_EQUALS		70
#define ANDROID_KEYCODE_EXPLORER	64
#define ANDROID_KEYCODE_FOCUS		80
#define ANDROID_KEYCODE_GRAVE		68
#define ANDROID_KEYCODE_HEADSETHOOK	79
#define ANDROID_KEYCODE_HOME		3
#define ANDROID_KEYCODE_LEFT_BRACKET	71
#define ANDROID_KEYCODE_MEDIA_FAST_FORWARD	90
#define ANDROID_KEYCODE_MEDIA_NEXT	87
#define ANDROID_KEYCODE_MEDIA_PLAY_PAUSE	85
#define ANDROID_KEYCODE_MEDIA_PREVIOUS	88
#define ANDROID_KEYCODE_MEDIA_REWIND	89
#define ANDROID_KEYCODE_MEDIA_STOP	86
#define ANDROID_KEYCODE_MENU		82
#define ANDROID_KEYCODE_MINUS		69
#define ANDROID_KEYCODE_MUTE		91
#define ANDROID_KEYCODE_NUM		78
#define ANDROID_KEYCODE_PLUS		81
#define ANDROID_KEYCODE_POWER		26
#define ANDROID_KEYCODE_RIGHT_BRACKET	72
#define ANDROID_KEYCODE_SEARCH		84
#define ANDROID_KEYCODE_SEMICOLON	74
#define ANDROID_KEYCODE_SHIFT_LEFT	59
#define ANDROID_KEYCODE_SHIFT_RIGHT	60
#define ANDROID_KEYCODE_SLASH		76
#define ANDROID_KEYCODE_SOFT_LEFT	1
#define ANDROID_KEYCODE_SOFT_RIGHT	2
#define ANDROID_KEYCODE_SPACE		62
#define ANDROID_KEYCODE_STAR		17
#define ANDROID_KEYCODE_SYM		63
#define ANDROID_KEYCODE_TAB		61


static void translate_key_to_gpac(int keycode, GF_EventKey *evt)
{
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
