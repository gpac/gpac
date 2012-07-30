include ../config.mak

#all OS and lib independent
PLUGDIRS=aac_in ac3_in audio_filter bifs_dec dummy_in soft_raster mp3_in isom_in odf_dec rtp_in timedtext img_in saf_in ismacryp

ifeq ($(DISABLE_MPD), no)
PLUGDIRS+=mpd_in
endif

ifeq ($(DISABLE_MEDIA_IMPORT), no)
PLUGDIRS+=mpegts_in 
endif

ifeq ($(DISABLE_SMGR), no)
PLUGDIRS+=ctx_load svg_in
endif

ifeq ($(DISABLE_SVG), no)
PLUGDIRS+=laser_dec svg_in widgetman
ifeq ($(DISABLE_LOADER_BT),no)
PLUGDIRS+=osd
endif
endif

ifeq ($(CONFIG_FT), no)
else
PLUGDIRS+=ft_font
endif

ifeq ($(CONFIG_FFMPEG), no)
else
PLUGDIRS+=ffmpeg_in
endif

ifeq ($(CONFIG_XVID), no)
else
PLUGDIRS+=xvid_dec
endif

ifeq ($(CONFIG_OGG), no)
else
PLUGDIRS+=ogg
endif

ifeq ($(CONFIG_FREENECT), no)
else
PLUGDIRS+=freenect
endif

ifeq ($(CONFIG_AMR_NB), yes)
PLUGDIRS+=amr_dec
endif

ifeq ($(CONFIG_AMR_NB_FT), yes)
PLUGDIRS+=amr_float_dec
else
ifeq ($(CONFIG_AMR_WB_FT), yes)
PLUGDIRS+=amr_float_dec
endif
endif

ifeq ($(CONFIG_OSS_AUDIO), no)
else
PLUGDIRS+=oss_audio
endif

ifeq ($(CONFIG_ALSA), yes)
PLUGDIRS+=alsa
endif

ifeq ($(CONFIG_JACK), yes)
PLUGDIRS+=jack
endif

ifeq ($(CONFIG_SDL), yes)
PLUGDIRS+=sdl_out
endif
ifeq ($(CONFIG_PULSEAUDIO), yes)
PLUGDIRS+=pulseaudio
endif

ifeq ($(CONFIG_X11),yes)
PLUGDIRS+=x11_out
endif

ifeq ($(CONFIG_DIRECTFB),yes)
PLUGDIRS+=directfb_out
endif

ifeq ($(CONFIG_JS),no)
else
PLUGDIRS+=gpac_js
endif

ifeq ($(CONFIG_PLATINUM),yes)
PLUGDIRS+=platinum
endif

ifeq ($(CONFIG_AVCAP),yes)
PLUGDIRS+=avcap
endif

ifeq ($(CONFIG_OPENSVC),yes)
PLUGDIRS+=opensvc_dec
endif

ifeq ($(RENOIR_ENABLE),yes)
PLUGDIRS+=viren_out
endif

#w32 plugins
ifeq ($(CONFIG_WIN32),yes)
PLUGDIRS+=wav_out
ifeq ($(CONFIG_DIRECTX),yes)
PLUGDIRS+=dx_hw
endif

endif

ifeq ($(DISABLE_PLAYER), yes)
PLUGDIRS=
endif


all: plugs

plugs:	
	set -e; for i in $(PLUGDIRS) ; do $(MAKE) -C $$i all; done 

dep:
	set -e; for i in $(PLUGDIRS) ; do $(MAKE) -C $$i dep; done 

clean:
	set -e; for i in $(PLUGDIRS) ; do $(MAKE) -C $$i clean; done 

distclean:	
	set -e; for i in $(PLUGDIRS) ; do $(MAKE) -C $$i distclean; done 
