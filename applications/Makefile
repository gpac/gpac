include ../config.mak

APPDIRS=gpac

ifeq ($(DISABLE_ISOFF),no)
APPDIRS+=mp4box 
endif

ALLDIRS=$(APPDIRS)

all: apps

apps: 
	set -e; for i in $(APPDIRS) ; do $(MAKE) -C $$i all; done 

sggen:
	$(MAKE) -C generators all

dep:
	set -e; for i in $(ALLDIRS) ; do $(MAKE) -C $$i dep; done 

clean: 
	set -e; for i in $(ALLDIRS) ; do $(MAKE) -C $$i clean; done 

distclean: 
	set -e; for i in $(ALLDIRS) ; do $(MAKE) -C $$i distclean; done 

install:
	set -e; for i in $(INSTDIRS) ; do $(MAKE) -C $$i install; done

uninstall:
	set -e; for i in $(INSTDIRS) ; do $(MAKE) -C $$i uninstall; done
	
