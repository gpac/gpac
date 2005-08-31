include ../config.mak

APPDIRS=mp4box mp4client mp42avi generators

#no osmozilla compilation for mingw, too many issues at the time being
ifeq ($(CONFIG_WIN32), no)
APPDIRS+=osmozilla
endif

V4STUDIODIR=
INSTDIRS=mp4client osmozilla

ifeq ($(USE_WXWIDGETS), yes)
APPDIRS+=osmo4_wx
V4STUDIODIR=V4Studio
INSTDIRS+=osmo4_wx
endif

ALLDIRS=$(APPDIRS)

all: apps


apps: 
	for i in $(APPDIRS) ; do $(MAKE) -C $$i all; done 

sggen:
	$(MAKE) -C generators all

V4Studio:
	for i in $(V4STUDIODIR) ; do $(MAKE) -C $$i dep; done 

dep:
	for i in $(ALLDIRS) ; do $(MAKE) -C $$i dep; done 

clean: 
	for i in $(ALLDIRS) ; do $(MAKE) -C $$i clean; done 

distclean: 
	for i in $(ALLDIRS) ; do $(MAKE) -C $$i distclean; done 

install:
	for i in $(INSTDIRS) ; do $(MAKE) -C $$i install; done

uninstall:
	for i in $(INSTDIRS) ; do $(MAKE) -C $$i uninstall; done