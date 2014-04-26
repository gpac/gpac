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

#ifndef JAVA_ENV
#define JAVA_ENV

#include <jni.h>

JavaVM* GetJavaVM();
JNIEnv* GetEnv();

#endif //JAVA_ENV
