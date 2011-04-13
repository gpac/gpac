COMMON_PATH := $(call my-dir)

#Settings common to all JNI Builds
TARGET_PLATFORM	 := android-4
APP_OPTIM        := debug
#APP_ABI          := armeabi-v7a
APP_ABI          := armeabi
GPAC_LIB_L	 := ../libs/$(APP_ABI)/

# Common Flags for ligpac and modules
LOCAL_CFLAGS += -DGPAC_ANDROID
LOCAL_CFLAGS +=	-DGPAC_HAVE_CONFIG_H
LOCAL_CFLAGS += -DXP_UNIX

# Common includes for EXTRALIBS...
LOCAL_LDLIBS    += -L$(COMMON_PATH)/../../../extra_lib/lib/android/
