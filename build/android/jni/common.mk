OMMON_PATH := $(call my-dir)

#Settings common to all JNI Builds
TARGET_PLATFORM	 := android-4

# Common Flags for ligpac and modules
LOCAL_CFLAGS += -DGPAC_ANDROID
LOCAL_CFLAGS +=	-DGPAC_HAVE_CONFIG_H
LOCAL_CFLAGS += -DXP_UNIX
LOCAL_CFLAGS += -DGPAC_HAS_IPV6
LOCAL_CFLAGS += -Wno-shift-negative-value
LOCAL_CFLAGS += -Wno-deprecated-declarations
LOCAL_CFLAGS += -Wno-pointer-sign
