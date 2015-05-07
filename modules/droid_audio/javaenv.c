/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Ivica Arsov, Jean Le Feuvre
 *			Copyright (c) Mines-Telecom 2009-
 *					All rights reserved
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

#include <gpac/config_file.h>
#include <gpac/modules/droidaudio.h>
#include "javaenv.h"

/**
 * A reference to the jvm to be provided
 */
static JavaVM *jvm;

/**
 * Set to true when the current thread is attached
 * to the jvm
 */
static __thread Bool attached;


/**
 * Register the java virtual machine
 */
void gf_droidaudio_register_java_vm(JavaVM *vm)
{
	jvm = vm;
}

/**
 * Get the jni thread environment
 */
JNIEnv *gf_droidaudio_jni_get_thread_env()
{
	assert(jvm && *jvm);
	JNIEnv *env;
	jint rc = (*jvm)->GetEnv(jvm, (void **) &env, JNI_VERSION_1_6);
	if (rc != JNI_OK)
		return NULL;
	assert(env);
	assert((*env)->GetVersion(env));
	return env;
}

/**
 * Attach current thread to the jvm, check first if we are attached so
 * this can be called multiple time.
 */
JNIEnv *gf_droidaudio_jni_attach_current_thread()
{
	assert(jvm && *jvm);
	JNIEnv *env = NULL;
	jint rc = (*jvm)->GetEnv(jvm, (void **) &env, JNI_VERSION_1_6);
	if (rc == JNI_EDETACHED) {
		attached = GF_TRUE;
		(*jvm)->AttachCurrentThread(jvm, &env, NULL);
	}
	return env;
}

/**
 * Detach current thread from the jvm
 */
void gf_droidaudio_jni_detach_current_thread()
{
	assert(jvm && *jvm);
        if (attached)
            (*jvm)->DetachCurrentThread(jvm);
        attached = GF_FALSE;
}
