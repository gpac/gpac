//#include "wrapper_jni.h"
#ifndef WRAPPER_JNI
#define WRAPPER_JNI

#ifdef __cplusplus
extern "C" {
#endif

#include <jni.h>
#include <android/log.h>
#include "wrapper.h"

#define jniTAG "WRAPPER_JNI"

#define jniLOGV(X)  __android_log_print(ANDROID_LOG_VERBOSE, jniTAG, X)
#define jniLOGI(X)  __android_log_print(ANDROID_LOG_INFO, jniTAG, X)
#define jniLOGE(X)  __android_log_print(ANDROID_LOG_ERROR, jniTAG, X)

#define CAST_HANDLE(wr) jclass c = env->GetObjectClass(obj);\
                        if (!c) return;\
                        jfieldID fid = env->GetFieldID(c, "handle", "J");\
                        if (!fid){\
                          __android_log_print(ANDROID_LOG_ERROR, jniTAG, "No Field ID, ERROR");\
                          return;\
                        }\
                        jlong h = env->GetLongField(obj, fid);\
                        CNativeWrapper* wr = (CNativeWrapper*)h;
//                        __android_log_print(ANDROID_LOG_VERBOSE, jniTAG, "Handle = %p", wr);

/*
 * Class:     com_gpac_Osmo4_GPACInstance
 * Method:    createInstance
 * Signature: (Lcom/gpac/Osmo4/GpacCallback;IILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jlong JNICALL Java_com_gpac_Osmo4_GPACInstance_createInstance(JNIEnv * env, jclass obj, jobject callback, jint width, jint height, jstring cfg_dir, jstring modules_dir, jstring cache_dir, jstring font_dir, jstring gui_dir, jstring url_to_open);
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpacconnect(JNIEnv * env, jobject obj, jstring fileName);
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpacdisconnect(JNIEnv * env, jobject obj);
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpacfree(JNIEnv * env, jobject obj);
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpacrender (JNIEnv * env, jobject obj);
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpacresize (JNIEnv * env, jobject obj, jint width, jint height);
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpaceventmousedown(JNIEnv * env, jobject obj, jfloat x, jfloat y);
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpaceventmouseup(JNIEnv * env, jobject obj, jfloat x, jfloat y);
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpaceventmousemove(JNIEnv * env, jobject obj, jfloat x, jfloat y);
//-----------------------------------
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpaceventkeypress(JNIEnv * env, jobject obj, jint keycode, jint rawkeycode, jint up, jint flag, jint unicode);
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
(JNIEnv * env, jobject obj, jstring category, jstring name, jstring value);



JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_setGpacLogs(JNIEnv * env, jobject obj, jstring tools_at_levels);
JNIEXPORT void JNICALL Java_com_gpac_Osmo4_GPACInstance_gpaceventorientationchange(JNIEnv * env, jobject obj, jfloat x, jfloat y, jfloat z);

#ifdef __cplusplus
}
#endif
#endif
