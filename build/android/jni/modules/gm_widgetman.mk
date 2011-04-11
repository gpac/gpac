LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_widgetman

include $(LOCAL_PATH)/base.mk

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/android/zlibdroid/jni/
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/android/js/jni/
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/android/js/jni/Linux_All_DBG.OBJ
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/android/js/jni/editline

LOCAL_SRC_FILES := ../../../../modules/widgetman/widgetman.c ../../../../modules/widgetman/unzip.c ../../../../modules/widgetman/widget.c ../../../../modules/widgetman/wgt_load.c

LOCAL_LDLIBS    += -ljs_osmo -lz
LOCAL_MODULE_FILENAME=gm_widgetman

include $(BUILD_SHARED_LIBRARY)
