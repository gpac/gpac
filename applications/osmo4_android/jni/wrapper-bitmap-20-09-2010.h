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

#include <android/bitmap.h>

#define		KErrGeneral	1
#define		GPAC_CFG_DIR	"/data/osmo/"
#define		GPAC_MODULES_DIR "/data/osmo/modules/"
#define		GPAC_MODULES_PATH "/data/osmo/modules/"
#define		GPAC_CACHE_DIR "/data/osmo/cache/"
#define		GPAC_LOG_FILE "/data/osmo/gpac_logs.txt"
#define		GPAC_FONT_DIR "/system/fonts/"

#define		DEBUG_MODE	1
#define		DEBUG_FILE	"/data/osmo/osmo_debug.txt"

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

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
class CPeriodic{
	public:
	CPeriodic();
	~CPeriodic();

	void Start();
	void Cancel();

};
//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
class CNativeWrapper{

	private:
		void* m_window;
		void* m_session;

		GF_User *GetUser() { return &m_user; }
		GF_Terminal *m_term;

		CPeriodic *m_pTimer;
		GF_Mutex *m_mx;
		GF_User m_user;
		GF_SystemRTInfo m_rti;

		int	do_log;
		FILE	*logfile;
	private:
		void SetupLogs();
		void Shutdown();
		void DisplayRTI();
	public:
		CNativeWrapper();
		~CNativeWrapper();
		int init(JNIEnv * env, jobject * bitmap);
		int OnTick();
		
		int connect(const char *url);
		void disconnect();
		void step(JNIEnv * env, jobject * bitmap);
		void resize(int w, int h);

		void onMouseDown(float x, float y);
		void onMouseUp(float x, float y);
		void onKeyPress(int keycode, int rawkeycode, int up, int flag);
		void translate_key(ANDROID_KEYCODE keycode, GF_EventKey *evt);
	public:		
		int MessageBox(const char* msg, const char* title);
		int Quit(int code);
		GF_Config *create_default_config(char *file_path, char *file_name);

		static void on_gpac_log(void *cbk, u32 ll, u32 lm, const char *fmt, va_list list);
		static Bool GPAC_EventProc(void *ptr, GF_Event *evt);
		static void Osmo4_progress_cbk(void *usr, char *title, u32 done, u32 total);

	private:
#ifdef	DEBUG_MODE
		FILE	*debug_f;
#endif		
		void debug_log(const char* msg);
		
};
//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
// this function will be called by Java to init gpac
CNativeWrapper* gpac_obj = NULL;

/*
void gpac_init();
void gpac_connect(const char *url);
void gpac_disconnect();
void gpac_render();
void gpac_free();
*/
//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
extern "C" {
	JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpacinit(JNIEnv * env, jobject obj,  jobject bitmap, jint width, jint height);
	JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpacconnect(JNIEnv * env, jobject obj,  jstring url);
	JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpacdisconnect(JNIEnv * env, jobject obj);
	JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpacrender(JNIEnv * env, jobject obj, jobject bitmap);
	JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpacresize (JNIEnv * env, jobject obj, jint width, jint height);
	JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpacfree(JNIEnv * env, jobject obj);

	JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpaceventmousedown(JNIEnv * env, jobject obj, jfloat x, jfloat y);
	JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpaceventmouseup(JNIEnv * env, jobject obj, jfloat x, jfloat y);
	JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpaceventkeypress(JNIEnv * env, jobject obj, jint keycode, jint rawkeycode, jint up, jint flag);
};

