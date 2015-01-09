OMMON_PATH := $(call my-dir)

#Settings common to all JNI Builds
TARGET_PLATFORM	 := android-4

# Common Flags for ligpac and modules
LOCAL_CFLAGS += -DGPAC_ANDROID
LOCAL_CFLAGS +=	-DGPAC_HAVE_CONFIG_H
LOCAL_CFLAGS += -DXP_UNIX
LOCAL_CFLAGS += -DGPAC_HAS_IPV6

