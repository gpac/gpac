
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libgpacWrapper

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../include
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../

LOCAL_LDLIBS    += -L$(LOCAL_PATH)/../../../extra_lib/lib/android
LOCAL_LDLIBS    += -L$(LOCAL_PATH)/../../../build/android/libs/armeabi/
LOCAL_LDLIBS    += -lgpac
LOCAL_LDLIBS    += -llog

#LOCAL_CFLAGS +=	-DGPAC_GUI_ONLY
LOCAL_CFLAGS +=	-DDEBUG_MODE

LOCAL_SRC_FILES := wrapper.cpp


include $(BUILD_SHARED_LIBRARY)
