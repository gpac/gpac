LOCAL_PATH := $(call my-dir)



#Settings common to all JNI Builds
TARGET_PLATFORM	 := android-4

# Common Flags for ligpac and modules
LOCAL_CFLAGS += -DGPAC_ANDROID
LOCAL_CFLAGS +=	-DGPAC_HAVE_CONFIG_H
LOCAL_CFLAGS += -DXP_UNIX
LOCAL_CFLAGS += -DGPAC_HAS_IPV6

LOCAL_MODULE    := gpacWrapper
LOCAL_C_INCLUDES := ../../../../../../include  ../../../../../../build/android/jni/libgpac
LOCAL_SRC_FILES := wrapper.cpp


LOCAL_LDLIBS    += -L../jniLibs/$(TARGET_ARCH_ABI)
LOCAL_LDLIBS    += -llog -lgpac

#LOCAL_CFLAGS +=	-DGPAC_GUI_ONLY
LOCAL_CFLAGS +=	-DDEBUG_MODE

include $(BUILD_SHARED_LIBRARY)
