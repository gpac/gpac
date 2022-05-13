LOCAL_PATH:= $(call my-dir)

include $(LOCAL_PATH)/gm_ft_font.mk
include $(LOCAL_PATH)/gm_droid_out.mk
include $(LOCAL_PATH)/gm_droid_audio.mk

#old arch not yet backported
#include $(LOCAL_PATH)/gm_osd.mk
#include $(LOCAL_PATH)/gm_droid_cam.mk
#include $(LOCAL_PATH)/gm_widgetman.mk

#deprecated
#include $(LOCAL_PATH)/gm_droid_mpegv.mk
