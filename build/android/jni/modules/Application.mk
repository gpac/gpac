LOCAL_PATH:= $(call my-dir)
include $(LOCAL_PATH)/../common.mk
APP_ABI          := armeabi-v7a armeabi x86
APP_BUILD_SCRIPT := $(APP_PROJECT_PATH)/jni/modules/Android.mk
APP_MODULES := gm_bifs_dec gm_ctx_load gm_dummy_in gm_ft_font gm_img_in gm_ismacryp gm_isom_in gm_laser_dec gm_odf_dec gm_rtp_in gm_saf_in gm_soft_raster gm_svg_in gm_timedtext libjavaenv gm_droidout gm_droidaudio gm_gpac_js gm_mp3_in gm_ffmpeg_in gm_mpegts_in gm_aac_in gm_mpd_in gm_widgetman gm_droid_cam gm_droid_mpegv gm_osd

