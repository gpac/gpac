//#include "wrapper_jni.h"
#ifdef __cplusplus
extern "C" {
#endif

#include "wrapper_jni.hpp"

/*
 * Class:     com_gpac_Osmo4_GPACInstance
 * Method:    createInstance
 * Signature: (Lcom/gpac/Osmo4/GpacCallback;IILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jlong JNICALL Java_com_gpac_Osmo4_GPACInstance_createInstance(JNIEnv * env, jclass obj, jobject callback, jint width, jint height, jstring cfg_dir, jstring modules_dir, jstring cache_dir, jstring font_dir, jstring gui_dir, jstring url_to_open)
{
	jboolean isCopy;
	const char * s1 = NULL;
	if (cfg_dir)
		s1 = env->GetStringUTFChars(cfg_dir, &isCopy);
	const char * s2 = env->GetStringUTFChars(modules_dir, &isCopy);
	const char * s3 = env->GetStringUTFChars(cache_dir, &isCopy);
	const char * s4 = env->GetStringUTFChars(font_dir, &isCopy);
	const char * s5 = env->GetStringUTFChars(gui_dir, &isCopy);
	const char * s6 = NULL;
	if (url_to_open)
		s6 = env->GetStringUTFChars(url_to_open, &isCopy);
	else
		s6 = NULL;
	CNativeWrapper * gpac_obj = new CNativeWrapper();
	if (gpac_obj) {
		int w = width;
		int h = height;
		jniLOGI("Calling gpac_obj->init()...");
		if (gpac_obj->init(env, NULL, &callback,
		                   w, h,
		                   s1, s2, s3, s4, s5, s6)) {
			jniLOGE("FAILED to init(), return code not 0");
			delete gpac_obj;
			gpac_obj = NULL;
		}
	} else {
		jniLOGE("FAILED to create new CNativeWrapper() : not enough memory ?");
	}

	if (s1)
		env->ReleaseStringUTFChars(cfg_dir, s1);
	env->ReleaseStringUTFChars(modules_dir, s2);
	env->ReleaseStringUTFChars(cache_dir, s3);
	env->ReleaseStringUTFChars(font_dir, s4);
	env->ReleaseStringUTFChars(gui_dir, s5);
	if (s6)
		env->ReleaseStringUTFChars(url_to_open, s6);
	__android_log_print(ANDROID_LOG_VERBOSE, jniTAG, "Returned Handle = %p", gpac_obj);
	return (jlong) gpac_obj;
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpacconnect(JNIEnv * env, jobject obj, jstring fileName)
{
	CAST_HANDLE(wr);
	jniLOGV("connect::start");
	if (!wr) {
		jniLOGV("connect::end : aborted");
		return;
	}
	jboolean isCopy;
	const char * cFileName = env->GetStringUTFChars(fileName, &isCopy);

	wr->connect(cFileName);

	env->ReleaseStringUTFChars(fileName, cFileName);
	jniLOGV("connect::end");
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpacdisconnect(JNIEnv * env, jobject obj) {
	CAST_HANDLE(wr);
	jniLOGV("disconnect::start");
	if (wr)
		wr->disconnect();
	jniLOGV("disconnect::end");
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpacfree(JNIEnv * env, jobject obj)
{
	CAST_HANDLE(wr);
	jniLOGV("free::start");
	if (wr)
		delete wr;
	jniLOGV("free::end");
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpacrender (JNIEnv * env, jobject obj)
{
	CAST_HANDLE(wr);
	//jniLOGV("render::start");
	if (wr)
		wr->step(env, NULL);
	//jniLOGV("render::end");
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpacresize (JNIEnv * env, jobject obj, jint width, jint height)
{
	CAST_HANDLE(wr);
	jniLOGV("resize::start");
	if (wr)
		wr->resize(width, height);
	jniLOGV("resize::end");
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpaceventmousedown(JNIEnv * env, jobject obj, jfloat x, jfloat y) {
	CAST_HANDLE(wr);
	//jniLOGV("mouseDown::start");
	if (wr)
		wr->onMouseDown(x, y);
	//jniLOGV("mouseDown::end");
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpaceventmouseup(JNIEnv * env, jobject obj, jfloat x, jfloat y) {
	CAST_HANDLE(wr);
	//jniLOGV("mouseUp::start");
	if (wr)
		wr->onMouseUp(x, y);
	//jniLOGV("mouseUp::end");
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpaceventmousemove(JNIEnv * env, jobject obj, jfloat x, jfloat y) {
	CAST_HANDLE(wr);
	//jniLOGV("mouseMouv::start");
	if (wr)
		wr->onMouseMove(x, y);
	//jniLOGV("mouseMouv::end");
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpaceventkeypress(JNIEnv * env, jobject obj, jint keycode, jint rawkeycode, jint up, jint flag, jint unicode) {
	CAST_HANDLE(wr);
	//jniLOGV("keypress::start");
	if (wr)
		wr->onKeyPress(keycode, rawkeycode, up, flag, unicode);
	//jniLOGV("keypress::end");
}
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpaceventorientationchange(JNIEnv * env, jobject obj, jfloat x, jfloat y, jfloat z) {
	CAST_HANDLE(wr);
	if (wr)	
		wr->onOrientationChange(x, y, z);
}

/*
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_getdpi(JNIEnv * env, jobject obj, jint keycode,jfloat x, jfloat y){
        CAST_HANDLE(wr);
        jniLOGV("get DPI::start");
	jclass cls = (*env)->GetObjectClass(env, obj);
	jmethodID mid = (*env)->GetStaticMethodID(env, cls, "getdpi", "(FF)V");
	if (mid == 0)
	  return;
	(*env)->CallStaticIntMethod(env, cls, mid, x,y);
        jniLOGV("get DPI::end");
}
*/

/*
 * Class:     com_gpac_Osmo4_GPACInstance
 * Method:    setGpacPreference
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_setGpacPreference
(JNIEnv * env, jobject obj, jstring category, jstring name, jstring value) {
	CAST_HANDLE(wr);
	jboolean isCopy;
	const char * scat = env->GetStringUTFChars(category, &isCopy);
	const char * sname = env->GetStringUTFChars(name, &isCopy);
	const char * svalue;
	if (value)
		svalue = env->GetStringUTFChars(value, &isCopy);
	else
		svalue = NULL;
	if (wr)
		wr->setGpacPreference(scat, sname, svalue);
	env->ReleaseStringUTFChars(category, scat);
	env->ReleaseStringUTFChars(name, sname);
	if (value)
		env->ReleaseStringUTFChars(value, svalue);
}
//-----------------------------------



JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_setGpacLogs(JNIEnv * env, jobject obj, jstring tools_at_levels) {
	CAST_HANDLE(wr);
	jniLOGV("setDebug::start");
	if (wr) {
		jboolean isCopy;
		const char * string_tools_at_levels = env->GetStringUTFChars(tools_at_levels, &isCopy);
		wr->setGpacLogs(string_tools_at_levels);
		env->ReleaseStringUTFChars(tools_at_levels, string_tools_at_levels);
	}
	jniLOGV("setDebug::end");
}

#ifdef __cplusplus
}
#endif
