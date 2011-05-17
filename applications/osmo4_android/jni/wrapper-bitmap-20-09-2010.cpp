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

#include <math.h>

#undef PI
#define PI 3.1415926535897932f

#include <GLES/gl.h>
#ifndef ANDROID_NDK
//#include <GLES/egl.h>
#endif /* !ANDROID_NDK */
//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
//CPeriodic
//-------------------------------
CPeriodic::CPeriodic(){}
//-------------------------------
CPeriodic::~CPeriodic(){}
//-------------------------------
void CPeriodic::Start(){
}
//-------------------------------
void CPeriodic::Cancel(){}
//---------------------------------------------------------------------------------------------------

void initGL()
{
    /* Enable smooth shading */
    glShadeModel(GL_SMOOTH);

    /* Set the background black */
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    /* Depth buffer setup */
    glClearDepthf(1.0f);

    /* Enables Depth Testing */
    glEnable(GL_DEPTH_TEST);

    /* The Type Of Depth Test To Do */
    glDepthFunc(GL_LEQUAL);

    /* Really Nice Perspective Calculations */
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
}

void gluPerspective(GLfloat fovy, GLfloat aspect,
                           GLfloat zNear, GLfloat zFar)
{
    GLfloat xmin, xmax, ymin, ymax;

    ymax = zNear * (GLfloat)tan(fovy * PI / 360);
    ymin = -ymax;
    xmin = ymin * aspect;
    xmax = ymax * aspect;

    glFrustumx((GLfixed)(xmin * 65536), (GLfixed)(xmax * 65536),
               (GLfixed)(ymin * 65536), (GLfixed)(ymax * 65536),
               (GLfixed)(zNear * 65536), (GLfixed)(zFar * 65536));
}

void resizeWindow(int width, int height)
{
    /* Height / width ration */
    GLfloat ratio;

    /* Protect against a divide by zero */
    if (height==0)
    {
        height=1;
    }

    ratio=(GLfloat)width/(GLfloat)height;

    /* Setup our viewport. */
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);

    /* change to the projection matrix and set our viewing volume. */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    /* Set our perspective */
    gluPerspective(45.0f, ratio, 0.1f, 100.0f);

    /* Make sure we're chaning the model view and not the projection */
    glMatrixMode(GL_MODELVIEW);

    /* Reset The View */
    glLoadIdentity();
}

void drawGLScene()
{
    GLfloat vertices[4][3];

    /* Clear The Screen And The Depth Buffer */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Enable VERTEX array */
    glEnableClientState(GL_VERTEX_ARRAY);

    /* Setup pointer to  VERTEX array */
    glVertexPointer(3, GL_FLOAT, 0, vertices);

    /* Move Left 1.5 Units And Into The Screen 6.0 */
    glLoadIdentity();
    glTranslatef(-1.5f, 0.0f, -6.0f);

    /* Top Of Triangle */
    vertices[0][0]=0.0f; vertices[0][1]=1.0f; vertices[0][2]=0.0f;
    /* Left Of Triangle */
    vertices[1][0]=-1.0f; vertices[1][1]=-1.0f; vertices[1][2]=0.0f;
    /* Right Of Triangle */
    vertices[2][0]=1.0f; vertices[2][1]=-1.0f; vertices[2][2]=0.0f;

    /* Drawing Using Triangles, draw triangles using 3 vertices */
    glDrawArrays(GL_TRIANGLES, 0, 3);

    /* Move Right 3 Units */
    glLoadIdentity();
    glTranslatef(1.5f, 0.0f, -6.0f);

    /* Top Right Of The Quad    */
    vertices[0][0]=1.0f;  vertices[0][1]=1.0f;  vertices[0][2]=0.0f;
    /* Top Left Of The Quad     */
    vertices[1][0]=-1.0f; vertices[1][1]=1.0f;  vertices[1][2]=0.0f;
    /* Bottom Left Of The Quad  */
    vertices[2][0]=1.0f;  vertices[2][1]=-1.0f; vertices[2][2]=0.0f;
    /* Bottom Right Of The Quad */
    vertices[3][0]=-1.0f; vertices[3][1]=-1.0f; vertices[3][2]=0.0f;

    /* Drawing using triangle strips, draw triangles using 4 vertices */
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    /* Disable vertex array */
    glDisableClientState(GL_VERTEX_ARRAY);

    /* Flush all drawings */
    glFinish();
}

//---------------------------------------------------------------------------------------------------
//CNativeWrapper
//-------------------------------
CNativeWrapper::CNativeWrapper(){
	do_log = 1;
	logfile = NULL;

	m_pTimer = NULL;
#ifndef GPAC_GUI_ONLY
	memset(&m_user, 0, sizeof(GF_User));
	m_term = NULL;
	m_mx = NULL;
	memset(&m_rti, 0, sizeof(GF_SystemRTInfo));
#endif

#ifdef	DEBUG_MODE
	debug_f = fopen(DEBUG_FILE, "w");
#endif
}
//-------------------------------
CNativeWrapper::~CNativeWrapper(){
	Shutdown();

	if (logfile) fclose(logfile);

#ifdef	DEBUG_MODE
	if (debug_f){
		debug_log("~CNativeWrapper()\n");
		fclose(debug_f);		
	}
		
#endif	
}
//-------------------------------
void CNativeWrapper::debug_log(const char* msg){
#ifdef	DEBUG_MODE
	fprintf(debug_f, "%s\n", msg);
	fflush(debug_f);
#endif
}
//-------------------------------
void CNativeWrapper::Shutdown()
{

	if (m_mx) gf_mx_del(m_mx);

	MessageBox("Osmo4 shutdown request", "");
	if (m_pTimer) {
		m_pTimer->Cancel();
		delete m_pTimer;
		m_pTimer = NULL;
	}
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
	MessageBox("Osmo4 shutdown OK", "");
}
//-------------------------------
int CNativeWrapper::MessageBox(const char* msg, const char* title){
	//call java function to display a message box

	debug_log(msg);

	return 1;
}
//-------------------------------
int CNativeWrapper::OnTick(){
#ifndef GPAC_GUI_ONLY
	if (m_term) gf_term_process_step(m_term);

	/*check RTI display*/
	//if (show_rti && gf_sys_get_rti(500, &m_rti, 0)) DisplayRTI();
#endif
	/*never stop...*/
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
//-------------------------------
void CNativeWrapper::on_gpac_log(void *cbk, u32 ll, u32 lm, const char *fmt, va_list list){
	// do log here
	FILE *logs = (FILE*)cbk;

	/*if (rti_logs && (lm & GF_LOG_RTI)) {
		char szMsg[2048];
		vsprintf(szMsg, fmt, list);
		UpdateRTInfo(szMsg + 6 );
	} else {
		if (log_time_start) fprintf(logs, "[At %d]", gf_sys_clock() - log_time_start);
		vfprintf(logs, fmt, list);
		fflush(logs);
	}*/
	
	vfprintf(logs, fmt, list);
	fflush(logs);

}
//-------------------------------
Bool CNativeWrapper::GPAC_EventProc(void *ptr, GF_Event *evt){
	//event
	return true;
}
//-------------------------------
void CNativeWrapper::Osmo4_progress_cbk(void *usr, char *title, u32 done, u32 total){
	
}
//-------------------------------
void CNativeWrapper::SetupLogs(){
	const char *opt;
	debug_log("SetupLogs()");

//#ifndef GPAC_GUI_ONLY
	gf_mx_p(m_mx);
	if (do_log) {
		gf_log_set_level(0);
		do_log = 0;
	}
	/*setup GPAC logs: log all errors*/
	gf_log_set_level(GF_LOG_DEBUG);
	gf_log_set_tools(0xFFFFFFFF);
	opt = gf_cfg_get_key(m_user.config, "General", "LogLevel");
	if ((opt && !stricmp(opt, "debug")) /*|| 1*/) { 
		FILE *logs = fopen(GPAC_LOG_FILE, "wt");
		if (!logs) {
			MessageBox("Cannot open log file - disabling logs", "Warning !");
		} else {
			MessageBox("Debug log enabled in \\data\\gpac_logs.txt", "Info");
			fclose(logs);
			do_log = 1;
			gf_log_set_level(GF_LOG_DEBUG);
			gf_log_set_tools(0xFFFFFFFF);
		}
	}
	if (!do_log) {
		gf_log_set_level(GF_LOG_ERROR);
		gf_log_set_tools(0xFFFFFFFF);
	}
	

	gf_log_set_level(GF_LOG_DEBUG);
	gf_log_set_tools(0xFFFFFFFF);

	logfile = fopen(GPAC_LOG_FILE, "wt");

	gf_log_set_callback(logfile, on_gpac_log);
	gf_mx_v(m_mx);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Osmo4 logs initialized\n"));
//#endif
}
//-------------------------------
int CNativeWrapper::init(JNIEnv * env, jobject * bitmap){

	debug_log("int CNativeWrapper::init()");

	int m_Width = 100, m_Height = 100;

	int first_launch = 0;
	const char *opt;

	m_window = env;
	m_session = bitmap;
	
	m_mx = gf_mx_new("Osmo4");

	//load config file
	m_user.config = gf_cfg_new(GPAC_CFG_DIR, "GPAC.cfg");
	if (!m_user.config) {
		first_launch = 1;
		FILE *ft = fopen(GPAC_CFG_DIR"GPAC.cfg", "wt");
		if (!ft) {
			MessageBox("Cannot create GPAC Config file", "Fatal Error");
			return Quit(KErrGeneral);
		} else {
			fclose(ft);
		}
		m_user.config = gf_cfg_new(GPAC_CFG_DIR, "GPAC.cfg");
		if (!m_user.config) {
			MessageBox("GPAC Configuration file not found", "Fatal Error");
			return Quit(KErrGeneral);
		}
	}

	SetupLogs();
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
		//gf_cfg_set_key(m_user.config, "General", "StartupFile", GPAC_CFG_DIR"gpac.mp4");
		/*setup UDP traffic autodetect*/
		gf_cfg_set_key(m_user.config, "Network", "AutoReconfigUDP", "yes");
		gf_cfg_set_key(m_user.config, "Network", "UDPTimeout", "10000");
		gf_cfg_set_key(m_user.config, "Network", "BufferLength", "3000");
		
		gf_cfg_set_key(m_user.config, "Compositor", "TextureTextMode", "Default");
		//gf_cfg_set_key(m_user.config, "Compositor", "FrameRate", "30");

		gf_cfg_set_key(m_user.config, "Video", "DriverName", "Android Video Output");

		gf_cfg_set_key(m_user.config, "FontEngine", "FontReader", "ft_font");

		gf_cfg_set_key(m_user.config, "FontEngine", "FontDirectory", GPAC_FONT_DIR);

		
		/*save cfg and reload*/
		gf_cfg_del(m_user.config);
		m_user.config = gf_cfg_new(GPAC_CFG_DIR, "GPAC.cfg");
		if (!m_user.config) {
			MessageBox("Cannot save initial GPAC Config file", "Fatal Error");
			return Quit(KErrGeneral);
		}


		MessageBox("Osmo4", "Thank you for Installing");
	}

	/*load modules*/
	opt = gf_cfg_get_key(m_user.config, "General", "ModulesDirectory");
	m_user.modules = gf_modules_new(opt, m_user.config);
	if (!m_user.modules || !gf_modules_get_count(m_user.modules)) {
		MessageBox(m_user.modules ? "No modules available" : "Cannot create module manager", "Fatal Error");
		if (m_user.modules) gf_modules_del(m_user.modules);
		gf_cfg_del(m_user.config);
		return Quit(KErrGeneral);
	}

	if (first_launch) {
		/*first launch, register all files ext*/
		for (u32 i=0; i<gf_modules_get_count(m_user.modules); i++) {
			GF_InputService *ifce = (GF_InputService *) gf_modules_load_interface(m_user.modules, i, GF_NET_CLIENT_INTERFACE);
			if (!ifce) continue;
			if (ifce) {
				ifce->CanHandleURL(ifce, "test.test");
				gf_modules_close_interface((GF_BaseInterface *)ifce);
			}
		}
	}

	/*we don't thread the terminal, ie appart from the audio renderer, media decoding and visual rendering is 
	handled by the app process*/
	m_user.init_flags = GF_TERM_NO_THREAD | GF_TERM_NO_REGULATION;
	m_user.init_flags |= GF_TERM_NO_AUDIO;
	m_user.EventProc = GPAC_EventProc;
	m_user.opaque = this;
	m_user.os_window_handler = m_window;
	m_user.os_display = m_session;

	m_term = gf_term_new(&m_user);
	if (!m_term) {
		MessageBox("Cannot load GPAC terminal", "Fatal Error");
		gf_modules_del(m_user.modules);
		gf_cfg_del(m_user.config);
		return Quit(KErrGeneral);
	}
	MessageBox("GPAC terminal loaded", "Success !");

	gf_term_set_size(m_term, m_Width, m_Height);

	m_pTimer = new CPeriodic();
	m_pTimer->Start();

	opt = gf_cfg_get_key(m_user.config, "General", "StartupFile");
	//if (opt) gf_term_connect(m_term, opt);

	//initGL();
}
//-------------------------------
int CNativeWrapper::connect(const char *url){
	/*debug_log("Starting to connect ...");	

	gf_term_connect_from_time(m_term, url, 0, false);
	//gf_term_connect(m_term, url);

	debug_log("connected ...");*/

	char pl_path[GF_MAX_PATH];
	
	strcpy(pl_path, url);

	debug_log("Connecting to ...");
	debug_log(pl_path);

	//gf_term_connect_with_path(m_term, url, pl_path);
	gf_term_connect(m_term, url);
	debug_log("connected ...");
}
//-----------------------------------------------------
void CNativeWrapper::disconnect(){
	
	gf_term_disconnect(m_term);
	debug_log("disconnected ...");
}
//-----------------------------------------------------
void CNativeWrapper::step(JNIEnv * env, jobject * bitmap){
	m_window = env;
	m_session = bitmap;
	debug_log("Step ...");
	if (!m_term)
		debug_log("m_term ...");
	else
	if (!m_term->compositor)
		debug_log("compositor ...");
	else
	if (!m_term->compositor->video_out)
		debug_log("video_out ...");
	else
	if (!m_term->compositor->video_out->Setup)
		debug_log("Setup ...");
	else
	{
		debug_log("Video Setup ...");
		m_term->compositor->video_out->Setup(m_term->compositor->video_out, m_window, m_session, -1);
	}
	gf_term_process_step(m_term);
	//drawGLScene();
}
//-----------------------------------------------------
void CNativeWrapper::resize(int w, int h){
	gf_term_set_size(m_term, w, h);
	//resizeWindow(w,h);
}
//-----------------------------------------------------
void CNativeWrapper::onMouseDown(float x, float y){
	char msg[100];	
	sprintf(msg, "onMousedown x=%f, y=%f", x, y );
	debug_log(msg);

	GF_Event evt;
	evt.type = GF_EVENT_MOUSEDOWN;
	evt.mouse.button = GF_MOUSE_LEFT;
	evt.mouse.x = x;
	evt.mouse.y = y;
	
	int ret = gf_term_user_event(m_term, &evt);
}
//-----------------------------------------------------
void CNativeWrapper::onMouseUp(float x, float y){
	char msg[100];	
	sprintf(msg, "onMouseUp x=%f, y=%f", x, y );
	debug_log(msg);

	GF_Event evt;
	evt.type = GF_EVENT_MOUSEUP;
	evt.mouse.button = GF_MOUSE_LEFT;
	evt.mouse.x = x;
	evt.mouse.y = y;
	
	int ret = gf_term_user_event(m_term, &evt);
}
//-----------------------------------------------------
void CNativeWrapper::onKeyPress(int keycode, int rawkeycode, int up, int flag){
	GF_Event evt;
	if (up == 0) evt.type = GF_EVENT_KEYUP;
	else evt.type = GF_EVENT_KEYDOWN;

	evt.key.flags = 0;
	evt.key.hw_code = rawkeycode;
	
	char msg[100];	
	sprintf(msg, "onKeyPress keycode=%d", keycode);
	debug_log(msg);	

	//translate_key(keycode, &evt.key);
	evt.key.key_code = GF_KEY_A;
	int ret = gf_term_user_event(m_term, &evt);
	/*generate a key up*/
	
	sprintf(msg, "onKeyPress gpac keycode=%d, GF_KEY_A=%d, ret=%d", evt.key.key_code, GF_KEY_A, ret);
	debug_log(msg);	
}
//-----------------------------------------------------
void CNativeWrapper::translate_key(ANDROID_KEYCODE keycode, GF_EventKey *evt){
	evt->flags = 0;
	
	char msg[100];	
	sprintf(msg, "translate_key keycode=%d", keycode);
	debug_log(msg);

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
//-----------------------------------------------------
GF_Config *CNativeWrapper::create_default_config(char *file_path, char *file_name){
	GF_Config *cfg;
	char szPath[GF_MAX_PATH];
	FILE *f;
	sprintf(szPath, "%s%c%s", file_path, GF_PATH_SEPARATOR, file_name);
	f = fopen(szPath, "wt");
	fprintf(stdout, "create %s: %s\n", szPath, (f==NULL) ? "Error" : "OK");
	if (!f) return NULL;
	fclose(f);

	cfg = gf_cfg_new(file_path, file_name);
	if (!cfg) return NULL;

#ifdef GPAC_MODULES_PATH
	fprintf(stdout, "Using module directory %s \n", GPAC_MODULES_PATH);
	strcpy(szPath, GPAC_MODULES_PATH);
#elif defined(WIN32)
	strcpy(szPath, file_path);	
#else 
	fprintf(stdout, "Please enter full path to GPAC modules directory:\n");
	scanf("%s", szPath);
#endif
	gf_cfg_set_key(cfg, "General", "ModulesDirectory", szPath);
	gf_cfg_set_key(cfg, "Audio", "ForceConfig", "yes");
	gf_cfg_set_key(cfg, "Audio", "NumBuffers", "2");
	gf_cfg_set_key(cfg, "Audio", "TotalDuration", "120");
	gf_cfg_set_key(cfg, "Audio", "DisableNotification", "no");
	gf_cfg_set_key(cfg, "FontEngine", "FontReader", "ft_font");

	/*these fonts seems installed by default on many systems...*/
	//gf_cfg_set_key(cfg, "FontEngine", "FontSerif", "Bitstream Vera Serif");
	//gf_cfg_set_key(cfg, "FontEngine", "FontSans", "Bitstream Vera Sans");
	//gf_cfg_set_key(cfg, "FontEngine", "FontFixed", "Bitstream Vera Monospace");
	strcpy(szPath, "/system/fonts/");

	fprintf(stdout, "Using default font directory %s\n", szPath);
	gf_cfg_set_key(cfg, "FontEngine", "FontDirectory", szPath);

	fprintf(stdout, "Using /tmp as a cache directory for HTTP downloads:\n");
	gf_cfg_set_key(cfg, "General", "CacheDirectory", "/data/osmo/tmp");

	gf_cfg_set_key(cfg, "Downloader", "CleanCache", "yes");
	gf_cfg_set_key(cfg, "Compositor", "AntiAlias", "All");
	gf_cfg_set_key(cfg, "Compositor", "FrameRate", "30");
	/*use power-of-2 emulation*/
	gf_cfg_set_key(cfg, "Compositor", "EmulatePOW2", "yes");

	gf_cfg_set_key(cfg, "Video", "DriverName", "Android Video Output");
	/*x11 only supports scalable zoom*/
	gf_cfg_set_key(cfg, "Compositor", "ScalableZoom", "yes");
	gf_cfg_set_key(cfg, "Audio", "DriverName", "SDL Audio Output");

	gf_cfg_set_key(cfg, "Video", "SwitchResolution", "no");
	gf_cfg_set_key(cfg, "Network", "AutoReconfigUDP", "yes");
	gf_cfg_set_key(cfg, "Network", "UDPTimeout", "10000");
	gf_cfg_set_key(cfg, "Network", "BufferLength", "3000");

	/*store and reload*/
	gf_cfg_del(cfg);
	return gf_cfg_new(file_path, file_name);
}
//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
// this function will be called by Java to init gpac

void gpac_init(JNIEnv * env, jobject * bitmap){

	gpac_obj = new CNativeWrapper();
	if (gpac_obj) gpac_obj->init(env, bitmap);
	//if (gpac_obj) gpac_obj->connect("/data/osmo/tidycity.mp4");
}

void gpac_render(JNIEnv * env, jobject * bitmap){
	if (gpac_obj) gpac_obj->step(env, bitmap);
}

void gpac_connect(const char *url){
	//gpac_obj->connect("/data/osmo/tidycity.mp4");
	gpac_obj->connect(url);
}

void gpac_resize(int w, int h){
	if (gpac_obj) gpac_obj->resize(w, h);
}

void gpac_disconnect(){
	if (gpac_obj) gpac_obj->disconnect();
}

void gpac_free(){
	if (gpac_obj) delete gpac_obj;
}

void gpac_onmousedown(float x, float y){
	if (gpac_obj) gpac_obj->onMouseDown(x, y);
}

void gpac_onmouseup(float x, float y){
	if (gpac_obj) gpac_obj->onMouseUp(x, y);
}
void gpac_onkeypress(int keycode, int rawkeycode, int up, int flag){
	if (gpac_obj) gpac_obj->onKeyPress(keycode, rawkeycode, up, flag);
}
//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------

JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpacinit(JNIEnv * env, jobject obj, jobject bitmap, jint width, jint height)
{
	int x = width;
	int y = height;
	gpac_init(env, &bitmap);
	//initGL();
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpacconnect(JNIEnv * env, jobject obj,  jstring url)
{
	const char *the_url = env->GetStringUTFChars(url, NULL);
	//(*env)->ReleaseStringUTFChars(env, prompt, str);
	gpac_connect(the_url);	
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpacdisconnect(JNIEnv * env, jobject obj){
	gpac_disconnect();
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpacfree(JNIEnv * env, jobject obj)
{
	gpac_free();
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpacrender (JNIEnv * env, jobject obj, jobject bitmap)
{
	gpac_render(env, &bitmap);
	//drawGLScene();
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpacresize (JNIEnv * env, jobject obj, jint width, jint height)
{
	int w = width;
	int h = height;
	gpac_resize(w,h);
	//resizeWindow(w,h);
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpaceventmousedown(JNIEnv * env, jobject obj, jfloat x, jfloat y){
	gpac_onmousedown(x, y);
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpaceventmouseup(JNIEnv * env, jobject obj, jfloat x, jfloat y){
	gpac_onmouseup(x, y);
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_artemis_Osmo4_GpacObject_gpaceventkeypress(JNIEnv * env, jobject obj, jint keycode, jint rawkeycode, jint up, jint flag){
	gpac_onkeypress(keycode, rawkeycode, up, flag);
}
//-----------------------------------
