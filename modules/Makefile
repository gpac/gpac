include ../config.mak

#all OS and lib independent
PLUGDIRS=soft_raster raw_out validator 

#disabled for now
#netctrl

ifneq ($(CONFIG_ZLIB), no)
#disabled for now
#PLUGDIRS+=widgetman
ifeq ($(DISABLE_VRML),no)
ifeq ($(DISABLE_LOADER_BT),no)
#disabled for now
#PLUGDIRS+=osd
endif
endif

endif


ifeq ($(CONFIG_FT), no)
else
PLUGDIRS+=ft_font
endif

ifeq ($(CONFIG_FREENECT), no)
else
PLUGDIRS+=freenect
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

ifeq ($(CONFIG_HID),yes)
PLUGDIRS+=psvr
endif


#w32 plugins
ifeq ($(CONFIG_WIN32),yes)
PLUGDIRS+=wav_out
ifeq ($(CONFIG_DIRECTX),yes)
PLUGDIRS+=dx_hw
endif
endif

#other linux
ifeq ($(CONFIG_LINUX),yes)
endif

#all modules below are not (yet) included in static build

ifeq ($(CONFIG_PLATINUM),yes)
PLUGDIRS+=platinum
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
