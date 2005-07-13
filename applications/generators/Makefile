include ../../config.mak

GENDIRS=MPEG4 X3D

ifeq ($(HAS_LIBXML2), yes)
#sorry minGW friends, I can't get a stable libxml2 to compile ...
ifeq ($(CONFIG_WIN32), yes)
else
GENDIRS+=SVG
endif
endif

all: sggen 

sggen:
	for i in $(GENDIRS) ; do $(MAKE) -C $$i all; done 

dep:
	for i in $(GENDIRS) ; do $(MAKE) -C $$i dep; done 

clean: 
	for i in $(GENDIRS) ; do $(MAKE) -C $$i clean; done 

distclean: 
	for i in $(GENDIRS) ; do $(MAKE) -C $$i distclean; done 

