LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_rtp_in

include $(LOCAL_PATH)/base.mk

#LOCAL_CFLAGS 		+= -DGPAC_DISABLE_STREAMING

LOCAL_SRC_FILES := ../../../../modules/rtp_in/rtp_in.c ../../../../modules/rtp_in/rtp_session.c ../../../../modules/rtp_in/rtp_signaling.c ../../../../modules/rtp_in/rtp_stream.c ../../../../modules/rtp_in/sdp_fetch.c ../../../../modules/rtp_in/sdp_load.c

include $(BUILD_SHARED_LIBRARY)
