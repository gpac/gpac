#
# Main gpac Makefile
#
include config.mak

vpath %.c $(SRC_PATH)

all:	version
	$(MAKE) -C src all
	$(MAKE) -C applications all
	$(MAKE) -C modules all

SVNREV_PATH:=$(SRC_PATH)/include/gpac/revision.h

version:
	@if [ -d $(SRC_PATH)/".svn" ]; then \
		if which svnversion >/dev/null; then \
			echo "#define GPAC_SVN_REVISION	\"$(shell svnversion $(SRC_PATH) )\"" > $(SVNREV_PATH).new ; \
			if diff -q $(SVNREV_PATH) $(SVNREV_PATH).new >/dev/null ; then \
				mv $(SVNREV_PATH).new  $(SVNREV_PATH) ;  \
			fi ; \
		fi \
	else \
		echo "No SVN Version found" ; \
	fi \

lib:	version
	$(MAKE) -C src all

apps:
	$(MAKE) -C applications all

sggen:
	$(MAKE) -C applications sggen

mods:
	$(MAKE) -C modules all

instmoz:
	$(MAKE) -C applications/osmozilla install

depend:
	$(MAKE) -C src dep
	$(MAKE) -C applications dep
	$(MAKE) -C modules dep

clean:
	$(MAKE) -C src clean
	$(MAKE) -C applications clean
	$(MAKE) -C modules clean

distclean:
	$(MAKE) -C src distclean
	$(MAKE) -C applications distclean
	$(MAKE) -C modules distclean
	rm -f config.mak config.h

dep:	depend

# tar release (use 'make -k tar' on a checkouted tree)
FILE=gpac-$(shell grep "\#define GPAC_VERSION " include/gpac/version.h | \
                    cut -d "\"" -f 2 )

tar:
	( tar zcvf ~/$(FILE).tar.gz ../gpac --exclude CVS --exclude bin --exclude lib --exclude Obj --exclude temp --exclude amr_nb --exclude amr_nb_ft --exclude amr_wb_ft --exclude *.mak --exclude *.o --exclude *.~*)

install:
	$(INSTALL) -d "$(DESTDIR)$(prefix)"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/$(libdir)"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/bin"
ifeq ($(DISABLE_ISOFF), no) 
ifeq ($(CONFIG_LINUX), yes)
ifneq ($(CONFIG_FFMPEG), no)
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/DashCast "$(DESTDIR)$(prefix)/bin"
endif
endif
endif
ifeq ($(DISABLE_ISOFF), no)
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/MP4Box "$(DESTDIR)$(prefix)/bin"
endif
ifeq ($(DISABLE_PLAYER), no)
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/MP4Client "$(DESTDIR)$(prefix)/bin"
endif
	if [ -d  $(DESTDIR)$(prefix)/$(libdir)/pkgconfig ] ; then \
	$(INSTALL) $(INSTFLAGS) -m 644 gpac.pc "$(DESTDIR)$(prefix)/$(libdir)/pkgconfig" ; \
	fi
	$(INSTALL) -d "$(DESTDIR)$(moddir)"
	$(INSTALL) bin/gcc/*.$(DYN_LIB_SUFFIX) "$(DESTDIR)$(moddir)"
	rm -f $(DESTDIR)$(moddir)/libgpac.$(DYN_LIB_SUFFIX)
	rm -f $(DESTDIR)$(moddir)/nposmozilla.$(DYN_LIB_SUFFIX)
	$(MAKE) installdylib
	$(INSTALL) -d "$(DESTDIR)$(mandir)"
	$(INSTALL) -d "$(DESTDIR)$(mandir)/man1";
	if [ -d  doc ] ; then \
	$(INSTALL) $(INSTFLAGS) -m 644 doc/man/mp4box.1 $(DESTDIR)$(mandir)/man1/ ; \
	$(INSTALL) $(INSTFLAGS) -m 644 doc/man/mp4client.1 $(DESTDIR)$(mandir)/man1/ ; \
	$(INSTALL) $(INSTFLAGS) -m 644 doc/man/gpac.1 $(DESTDIR)$(mandir)/man1/ ; \
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac" ; \
	$(INSTALL) $(INSTFLAGS) -m 644 doc/gpac.mp4 $(DESTDIR)$(prefix)/share/gpac/ ;  \
	fi
	if [ -d  gui ] ; then \
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac/gui" ; \
	$(INSTALL) $(INSTFLAGS) -m 644 gui/gui.bt "$(DESTDIR)$(prefix)/share/gpac/gui/" ; \
	$(INSTALL) $(INSTFLAGS) -m 644 gui/gui.js "$(DESTDIR)$(prefix)/share/gpac/gui/" ; \
	$(INSTALL) $(INSTFLAGS) -m 644 gui/gwlib.js "$(DESTDIR)$(prefix)/share/gpac/gui/" ; \
	$(INSTALL) $(INSTFLAGS) -m 644 gui/mpegu-core.js "$(DESTDIR)$(prefix)/share/gpac/gui/" ; \
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac/gui/icons" ; \
	$(INSTALL) $(INSTFLAGS) -m 644 gui/icons/*.svg "$(DESTDIR)$(prefix)/share/gpac/gui/icons/" ; \
	cp -R gui/extensions "$(DESTDIR)$(prefix)/share/gpac/gui/" ; \
	rm -rf "$(DESTDIR)$(prefix)/share/gpac/gui/extensions/*.svn" ; \
	fi

uninstall:
	$(MAKE) -C applications uninstall
	rm -rf $(DESTDIR)$(moddir)
	rm -rf $(DESTDIR)$(prefix)/$(libdir)/libgpac*
	rm -rf $(DESTDIR)$(prefix)/$(libdir)/pkgconfig/gpac.pc
	rm -rf $(DESTDIR)$(prefix)/bin/MP4Box
	rm -rf $(DESTDIR)$(prefix)/bin/MP4Client
	rm -rf $(DESTDIR)$(prefix)/bin/DashCast
	rm -rf $(DESTDIR)$(mandir)/man1/mp4box.1
	rm -rf $(DESTDIR)$(mandir)/man1/mp4client.1
	rm -rf $(DESTDIR)$(mandir)/man1/gpac.1
	rm -rf $(DESTDIR)$(prefix)/share/gpac
	rm -rf $(DESTDIR)$(prefix)/include/gpac

installdylib:
ifeq ($(CONFIG_WIN32),yes)
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/libgpac.dll $(prefix)/$(libdir)
else
ifeq ($(DEBUGBUILD),no)
	$(STRIP) bin/gcc/libgpac.$(DYN_LIB_SUFFIX)
endif
ifeq ($(CONFIG_DARWIN),yes)
	$(INSTALL) -m 755 bin/gcc/libgpac.$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(libdir)/libgpac.$(VERSION).$(DYN_LIB_SUFFIX)
	ln -sf libgpac.$(VERSION).$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(libdir)/libgpac.$(DYN_LIB_SUFFIX)
else
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/libgpac.$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(libdir)/libgpac.$(DYN_LIB_SUFFIX).$(VERSION_SONAME)
	ln -sf libgpac.$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(libdir)/libgpac.so.$(VERSION_MAJOR)
	ln -sf libgpac.$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(libdir)/libgpac.so
ifeq ($(DESTDIR)$(prefix),$(prefix))
	ldconfig || true
endif
endif
endif

install-lib:
	mkdir -p "$(DESTDIR)$(prefix)/include/gpac"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/include/gpac/*.h "$(DESTDIR)$(prefix)/include/gpac"
	mkdir -p "$(DESTDIR)$(prefix)/include/gpac/internal"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/include/gpac/internal/*.h "$(DESTDIR)$(prefix)/include/gpac/internal"
	mkdir -p "$(DESTDIR)$(prefix)/include/gpac/modules"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/include/gpac/modules/*.h "$(DESTDIR)$(prefix)/include/gpac/modules"
	$(INSTALL) $(INSTFLAGS) -m 644 config.h "$(DESTDIR)$(prefix)/include/gpac/configuration.h"
ifeq ($(GPAC_ENST), yes)
	mkdir -p "$(DESTDIR)$(prefix)/include/gpac/enst"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/include/gpac/enst/*.h "$(DESTDIR)$(prefix)/include/gpac/enst"
endif
	mkdir -p "$(DESTDIR)$(prefix)/$(libdir)"
	$(INSTALL) $(INSTFLAGS) -m 644 "./bin/gcc/libgpac_static.a" "$(DESTDIR)$(prefix)/$(libdir)"
	$(MAKE) installdylib

uninstall-lib:
	rm -rf "$(prefix)/include/gpac/internal"
	rm -rf "$(prefix)/include/gpac/modules"
	rm -rf "$(prefix)/include/gpac/enst"
	rm -rf "$(prefix)/include/gpac"

ifeq ($(CONFIG_DARWIN),yes)
dmg:
	rm "bin/gcc/MP4Client"
	$(MAKE) -C applications/mp4client
	./mkdmg.sh
endif

ifeq ($(CONFIG_LINUX),yes)
deb:
	fakeroot debian/rules clean
	sed -i "s/.DEV/.DEV-r`svnversion \"$(SRC_PATH)\"`/" debian/changelog
	fakeroot debian/rules configure
	fakeroot debian/rules binary
	rm -rf debian/
	svn cleanup
	svn up
endif

help:
	@echo "Input to GPAC make:"
	@echo "depend/dep: builds dependencies (dev only)"
	@echo "all (void): builds main library, programs and plugins"
	@echo "lib: builds GPAC library only (libgpac.so)"
	@echo "apps: builds programs only"
	@echo "modules: builds modules only"
	@echo "instmoz: build and local install of osmozilla"
	@echo "sggen: builds scene graph generators"
	@echo 
	@echo "clean: clean src repository"
	@echo "distclean: clean src repository and host config file"
	@echo "tar: create GPAC tarball"
	@echo 
	@echo "install: install applications and modules on system"
	@echo "uninstall: uninstall applications and modules"
ifeq ($(CONFIG_DARWIN),yes)
	@echo "dmg: creates DMG package file for OSX"
endif
ifeq ($(CONFIG_LINUX),yes)
        @echo "deb: creates DEB package file for debian based systems"
endif
	@echo 
	@echo "install-lib: install gpac library (dyn and static) and headers <gpac/*.h>, <gpac/modules/*.h> and <gpac/internal/*.h>"
	@echo "uninstall-lib: uninstall gpac library (dyn and static) and headers"
	@echo
	@echo "to build libgpac documentation, go to gpac/doc and type 'doxygen'"

-include .depend
