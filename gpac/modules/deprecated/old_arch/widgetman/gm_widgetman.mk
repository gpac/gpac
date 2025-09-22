LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_widgetman

include $(LOCAL_PATH)/base.mk

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/js/

LOCAL_SRC_FILES := ../../../../modules/widgetman/widgetman.c ../../../../modules/widgetman/unzip.c ../../../../modules/widgetman/widget.c ../../../../modules/widgetman/wgt_load.c

LOCAL_LDLIBS    += -lz

include $(BUILD_SHARED_LIBRARY)
