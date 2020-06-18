LOCAL_PATH:= $(call my-dir)
include $(LOCAL_PATH)/../common.mk
APP_ABI          := armeabi armeabi-v7a x86
APP_MODULES := gpac

#APP_CFLAGS += -Wno-shift-negative-value -Wno-pointer-sign