LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_gpac_js

include $(LOCAL_PATH)/base.mk

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/android/js/jni
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/android/js/jni/Linux_All_DBG.OBJ
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/android/js/jni/editline
#LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/js/

LOCAL_SRC_FILES := ../../../../modules/gpac_js/gpac_js.c

include $(BUILD_SHARED_LIBRARY)
