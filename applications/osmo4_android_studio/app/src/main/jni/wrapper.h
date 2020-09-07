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

#ifndef WRAPPER
#define  WRAPPER

#include <jni.h>

#include <gpac/terminal.h>
#include <gpac/thread.h>
#include <gpac/options.h>
#include <gpac/network.h>

//#define		MAX_PATH	255

#define		KErrGeneral	1
//#define		GPAC_CFG_DIR	"/data/osmo/"
#define		GPAC_CFG_DIR	m_cfg_dir
//#define		GPAC_MODULES_DIR "/data/osmo/modules/"
#define		GPAC_MODULES_DIR m_modules_dir
//#define		GPAC_MODULES_PATH "/data/osmo/modules/"
#define		GPAC_MODULES_PATH m_modules_dir
//#define		GPAC_CACHE_DIR "/data/osmo/cache/"
#define		GPAC_CACHE_DIR m_cache_dir
//#define		GPAC_LOG_FILE "/data/osmo/gpac_logs.txt"
#define		GPAC_LOG_FILE m_log_filename
//#define		GPAC_FONT_DIR "/system/fonts/"
#define		GPAC_FONT_DIR m_font_dir

// keyboard code
#define ANDROID_KEYCODE		int
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

#define ANDROID_KEYCODE_UNKWON		-1

#include <jni.h>

typedef struct _JavaEnvTh {
	JNIEnv * env;
	u32 javaThreadId;
	jobject cbk_obj;
	jmethodID cbk_displayMessage;
	jmethodID cbk_onProgress;
	jmethodID cbk_showKeyboard;
	jmethodID cbk_setCaption;
	jmethodID cbk_onLog;
	jmethodID cbk_onFmRequest;
	jmethodID cbk_setLogFile;
	jmethodID cbk_sensorSwitch;
} JavaEnvTh;

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
class CNativeWrapper {

private:
	void* m_window;
	void* m_session;

	GF_Terminal *m_term;

	/*
	 * Callback management
	 */
	JavaEnvTh *mainJavaEnv;

	GF_Mutex *m_mx;
	GF_User m_user;
	GF_SystemRTInfo m_rti;

	int	do_log;
	FILE	*log_file;

private:
	char m_cfg_dir[GF_MAX_PATH];
	char m_modules_dir[GF_MAX_PATH];
	char m_cache_dir[GF_MAX_PATH];
	char m_font_dir[GF_MAX_PATH];
	void setJavaEnv(JavaEnvTh * envToSet, JNIEnv *env, jobject callback);
private:
	void SetupLogs();
	void Shutdown();
protected:
	JavaEnvTh * getEnv();

public:
	CNativeWrapper();
	~CNativeWrapper();
	int init(JNIEnv * env, void * bitmap, jobject * callback, int width, int height, const char * cfg_dir, const char * modules_dir, const char * cache_dir, const char * font_dir, const char * gui_dir, const char * urlToLoad);

	int connect(const char *url);
	void disconnect();
	void step(void * env, void * bitmap);
	void resize(int w, int h);
	void setAudioEnvironment(JavaVM* javaVM);

	void onMouseDown(float x, float y);
	void onMouseUp(float x, float y);
	void onMouseMove(float x, float y);
	void onKeyPress(int keycode, int rawkeycode, int up, int flag, int unicode);
	void onOrientationChange(float x, float y, float z);
	void translate_key(ANDROID_KEYCODE keycode, GF_EventKey *evt);
	void navigate( GF_Event* evt);
	void setGpacPreference( const char * category, const char * name, const char * value);
	void setGpacLogs(const char *tools_at_level);

public:
	int MessageBox(const char* msg, const char* title, GF_Err status);
	int Quit(int code);WRAPPER
	static void on_gpac_log(void *cbk, GF_LOG_Level ll, GF_LOG_Tool lm, const char *fmt, va_list list);
	static void on_fm_request(void *cbk, u32 type, u32 param, int *value);
	static Bool GPAC_EventProc(void *cbk, GF_Event *evt);
	void progress_cbk(const char *title, u64 done, u64 total);
	static void Osmo4_progress_cbk(const void *usr, const char *title, u64 done, u64 total);

private:
	void debug_log(const char* msg);

};

#endif
