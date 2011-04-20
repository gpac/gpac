LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= libjavaenv

LOCAL_SRC_FILES := ../../../../modules/droid_audio/javaenv.c

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_droidaudio

include $(LOCAL_PATH)/base.mk

LOCAL_LDLIBS    += -llog -ljavaenv -ldl

LOCAL_SRC_FILES := ../../../../modules/droid_audio/droidaudio.c

LOCAL_SHARED_LIBRARIES := javaenv

include $(BUILD_SHARED_LIBRARY)
