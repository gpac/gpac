LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_svg_in

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/svg_in/svg_in.c

LOCAL_LDLIBS    += -ljs_osmo -lz

include $(BUILD_SHARED_LIBRARY)
