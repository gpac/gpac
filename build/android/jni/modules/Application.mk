LOCAL_PATH:= $(call my-dir)
include $(LOCAL_PATH)/../common.mk

APP_BUILD_SCRIPT := $(APP_PROJECT_PATH)/jni/modules/Android.mk
APP_MODULES := gm_ft_font gm_droid_out libjavaenv gm_droid_audio

#old modules to backport or drop
#APP_MODULES :=  gm_widgetman gm_droid_cam gm_osd gm_mediacodec_dec
#deprecated
#APP_MODULES += gm_droid_mpegv

#APP_CFLAGS += -Wno-shift-negative-value
