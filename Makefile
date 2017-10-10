#
# Main gpac Makefile
#
include config.mak

vpath %.c $(SRC_PATH)

all:	version
	$(MAKE) -C src all
	$(MAKE) -C applications all
ifneq ($(MP4BOX_STATIC),yes)
	$(MAKE) -C modules all
endif

GITREV_PATH:=$(SRC_PATH)/include/gpac/revision.h
TAG:=$(shell git --git-dir=$(SRC_PATH)/.git describe --tags --abbrev=0 2> /dev/null)
VERSION:=$(shell echo `git --git-dir=$(SRC_PATH)/.git describe --tags --long  || echo "UNKNOWN"` | sed "s/^$(TAG)-//")
BRANCH:=$(shell git --git-dir=$(SRC_PATH)/.git rev-parse --abbrev-ref HEAD 2> /dev/null || echo "UNKNOWN")

version:
	@if [ -d $(SRC_PATH)/".git" ]; then \
		echo "#define GPAC_GIT_REVISION	\"$(VERSION)-$(BRANCH)\"" > $(GITREV_PATH).new; \
		if ! diff -q $(GITREV_PATH) $(GITREV_PATH).new >/dev/null ; then \
			mv $(GITREV_PATH).new  $(GITREV_PATH); \
		fi; \
	else \
		echo "No GIT Version found" ; \
	fi

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
	@find . -type f -name '*.gcno*' -delete
	@find . -type f -name '*.gcda*' -delete
	@rm -f coverage.info 2> /dev/null

docs:
	@cd $(SRC_PATH)/doc && doxygen

test_suite:
	@cd $(SRC_PATH)/tests && ./make_tests.sh

lcov_clean:
	lcov --directory . --zerocounters

lcov_only:
	@echo "Generating lcov info in coverage.info"
	@rm -f ./gpac-conf-* > /dev/null
	@lcov -q -capture --directory . --output-file all.info 
	@lcov --remove all.info /usr/* /usr/include/* /usr/local/include/* /usr/include/libkern/i386/* /usr/include/sys/_types/* /opt/* /opt/local/include/* /opt/local/include/mozjs185/* --output coverage.info
	@rm all.info
	@echo "Purging lcov info"
	@cd src ; for dir in * ; do cd .. ; sed -i -- "s/$$dir\/$$dir\//$$dir\//g" coverage.info; cd src; done ; cd ..
	@echo "Done - coverage.info ready"

lcov:	lcov_only
	@rm -rf coverage/
	@genhtml -q -o coverage coverage.info

travis_tests:
	@echo "Running tests"
	@cd $(SRC_PATH)/tests && ./make_tests.sh -warn -sync-before

travis_deploy:
	@echo "Deploying results"
	@cd $(SRC_PATH)/tests && ./ghp_deploy.sh

travis: travis_tests lcov travis_deploy

dep:	depend

install:
	$(INSTALL) -d "$(DESTDIR)$(prefix)"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/$(libdir)"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/bin"
ifeq ($(DISABLE_ISOFF), no)
	if [ -f bin/gcc/MP4Box$(EXE_SUFFIX) ] ; then \
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/MP4Box$(EXE_SUFFIX) "$(DESTDIR)$(prefix)/bin" ; \
	fi
ifneq ($(MP4BOX_STATIC), yes)
	if [ -f bin/gcc/MP42TS$(EXE_SUFFIX) ] ; then \
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/MP42TS$(EXE_SUFFIX) "$(DESTDIR)$(prefix)/bin" ; \
	fi
ifneq ($(CONFIG_WIN32), yes)
ifneq ($(CONFIG_FFMPEG), no)
ifneq ($(DISABLE_CORE_TOOLS), yes)
ifneq ($(DISABLE_AV_PARSERS), yes)
	if [ -f bin/gcc/DashCast$(EXE_SUFFIX) ] ; then \
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/DashCast$(EXE_SUFFIX) "$(DESTDIR)$(prefix)/bin" ; \
	fi
endif
endif
endif
endif
endif
endif
ifneq ($(MP4BOX_STATIC), yes)
ifeq ($(DISABLE_PLAYER), no)
	if [ -f bin/gcc/MP4Client$(EXE_SUFFIX) ] ; then \
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/MP4Client$(EXE_SUFFIX) "$(DESTDIR)$(prefix)/bin" ; \
	fi
endif
endif
	if [ -d $(DESTDIR)$(prefix)/$(libdir)/pkgconfig ] ; then \
	$(INSTALL) $(INSTFLAGS) -m 644 gpac.pc "$(DESTDIR)$(prefix)/$(libdir)/pkgconfig" ; \
	fi
	$(INSTALL) -d "$(DESTDIR)$(moddir)"
ifneq ($(MP4BOX_STATIC),yes)
	$(INSTALL) bin/gcc/*$(DYN_LIB_SUFFIX) "$(DESTDIR)$(moddir)"
	rm -f $(DESTDIR)$(moddir)/libgpac$(DYN_LIB_SUFFIX)
	rm -f $(DESTDIR)$(moddir)/nposmozilla$(DYN_LIB_SUFFIX)
	$(MAKE) installdylib
endif
	$(INSTALL) -d "$(DESTDIR)$(mandir)"
	$(INSTALL) -d "$(DESTDIR)$(mandir)/man1"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/doc/man/mp4box.1 $(DESTDIR)$(mandir)/man1/ 
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/doc/man/mp4client.1 $(DESTDIR)$(mandir)/man1/ 
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/doc/man/gpac.1 $(DESTDIR)$(mandir)/man1/
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/doc/gpac.mp4 $(DESTDIR)$(prefix)/share/gpac/  
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac/gui"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/gui/gui.bt "$(DESTDIR)$(prefix)/share/gpac/gui/" 
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/gui/gui.js "$(DESTDIR)$(prefix)/share/gpac/gui/" 
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/gui/gwlib.js "$(DESTDIR)$(prefix)/share/gpac/gui/" 
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/gui/mpegu-core.js "$(DESTDIR)$(prefix)/share/gpac/gui/"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/gui/webvtt-renderer.js "$(DESTDIR)$(prefix)/share/gpac/gui/"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac/gui/icons"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac/gui/extensions"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac/shaders/"

	$(INSTALL) -d "$(DESTDIR)$(prefix)/include"

ifeq ($(CONFIG_DARWIN),yes)
	cp $(SRC_PATH)/gui/icons/* "$(DESTDIR)$(prefix)/share/gpac/gui/icons/"
	cp -R $(SRC_PATH)/gui/extensions/* "$(DESTDIR)$(prefix)/share/gpac/gui/extensions/"
	cp $(SRC_PATH)/shaders/* "$(DESTDIR)$(prefix)/share/gpac/shaders/"
	cp -R $(SRC_PATH)/include/* "$(DESTDIR)$(prefix)/include/"
else
	cp --no-preserve=mode,ownership,timestamp $(SRC_PATH)/gui/icons/* $(DESTDIR)$(prefix)/share/gpac/gui/icons/
	cp -R --no-preserve=mode,ownership,timestamp $(SRC_PATH)/gui/extensions/* $(DESTDIR)$(prefix)/share/gpac/gui/extensions/
	cp --no-preserve=mode,ownership,timestamp $(SRC_PATH)/shaders/* $(DESTDIR)$(prefix)/share/gpac/shaders/
	cp -R --no-preserve=mode,ownership,timestamp $(SRC_PATH)/include/* $(DESTDIR)$(prefix)/include/
endif

lninstall:
	$(INSTALL) -d "$(DESTDIR)$(prefix)"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/$(libdir)"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/bin"
ifeq ($(DISABLE_ISOFF), no)
	ln -sf $(BUILD_PATH)/bin/gcc/MP4Box$(EXE_SUFFIX) $(DESTDIR)$(prefix)/bin/MP4Box$(EXE_SUFFIX)
	ln -sf $(BUILD_PATH)/bin/gcc/MP42TS$(EXE_SUFFIX) $(DESTDIR)$(prefix)/bin/MP42TS$(EXE_SUFFIX)
endif
ifneq ($(MP4BOX_STATIC),yes)
ifeq ($(DISABLE_PLAYER), no)
	ln -sf $(BUILD_PATH)/bin/gcc/MP4Client$(EXE_SUFFIX) $(DESTDIR)$(prefix)/bin/MP4Client$(EXE_SUFFIX)
endif
ifneq ($(CONFIG_WIN32), yes)
ifneq ($(CONFIG_FFMPEG), no)
ifneq ($(DISABLE_CORE_TOOLS), yes)
ifneq ($(DISABLE_AV_PARSERS), yes)
	ln -sf $(BUILD_PATH)/bin/gcc/DashCast$(EXE_SUFFIX) $(DESTDIR)$(prefix)/bin/DashCast$(EXE_SUFFIX)
endif
endif
endif
endif
endif
ifeq ($(CONFIG_DARWIN),yes)
	ln -s $(BUILD_PATH)/bin/gcc/libgpac$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(libdir)/libgpac$(DYN_LIB_SUFFIX).$(VERSION_MAJOR)
	ln -sf $(DESTDIR)$(prefix)/$(libdir)/libgpac$(DYN_LIB_SUFFIX).$(VERSION_MAJOR) $(DESTDIR)$(prefix)/$(libdir)/libgpac$(DYN_LIB_SUFFIX)
else
	ln -s $(BUILD_PATH)/bin/gcc/libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(libdir)/libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME)
	ln -sf $(DESTDIR)$(prefix)/$(libdir)/libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(libdir)/libgpac.so.$(VERSION_MAJOR)
	ln -sf $(DESTDIR)$(prefix)/$(libdir)/libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(libdir)/libgpac.so
ifeq ($(DESTDIR)$(prefix),$(prefix))
	ldconfig || true
endif
endif

uninstall:
	$(MAKE) -C applications uninstall
	rm -rf $(DESTDIR)$(moddir)
	rm -rf $(DESTDIR)$(prefix)/$(libdir)/libgpac*
ifeq ($(CONFIG_WIN32),yes)
	rm -rf "$(DESTDIR)$(prefix)/bin/libgpac*"
endif
	rm -rf $(DESTDIR)$(prefix)/$(libdir)/pkgconfig/gpac.pc
	rm -rf $(DESTDIR)$(prefix)/bin/MP4Box
	rm -rf $(DESTDIR)$(prefix)/bin/MP4Client
	rm -rf $(DESTDIR)$(prefix)/bin/MP42TS
	rm -rf $(DESTDIR)$(prefix)/bin/DashCast
	rm -rf $(DESTDIR)$(mandir)/man1/mp4box.1
	rm -rf $(DESTDIR)$(mandir)/man1/mp4client.1
	rm -rf $(DESTDIR)$(mandir)/man1/gpac.1
	rm -rf $(DESTDIR)$(prefix)/share/gpac
	rm -rf $(DESTDIR)$(prefix)/include/gpac

installdylib:
ifneq ($(MP4BOX_STATIC),yes)
ifeq ($(CONFIG_WIN32),yes)
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/libgpac.dll.a $(DESTDIR)$(prefix)/$(libdir)
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/libgpac.dll $(DESTDIR)$(prefix)/bin
else
ifeq ($(DEBUGBUILD),no)
	$(STRIP) bin/gcc/libgpac$(DYN_LIB_SUFFIX)
endif
ifeq ($(CONFIG_DARWIN),yes)
	$(INSTALL) -m 755 bin/gcc/libgpac$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(libdir)/libgpac.$(VERSION)$(DYN_LIB_SUFFIX)
	ln -sf libgpac.$(VERSION)$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(libdir)/libgpac$(DYN_LIB_SUFFIX)
else
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(libdir)/libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME)
	ln -sf libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(libdir)/libgpac.so.$(VERSION_MAJOR)
	ln -sf libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(libdir)/libgpac.so
ifeq ($(DESTDIR)$(prefix),$(prefix))
	ldconfig || true
endif
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
	@if [ ! -z "$(shell git diff FETCH_HEAD)" ]; then \
		echo "Local revision and remote revision are not congruent; you may have local commit."; \
		echo "Please consider pushing your commit before generating an installer"; \
		exit 1; \
	fi
	rm "bin/gcc/MP4Client"
	$(MAKE) -C applications/mp4client
	./mkdmg.sh $(arch)
endif

ifeq ($(CONFIG_LINUX),yes)
deb:
	@if [ ! -z "$(shell git diff FETCH_HEAD)" ]; then \
		echo "Local revision and remote revision are not congruent; you may have local commit."; \
		echo "Please consider pushing your commit before generating an installer"; \
		exit 1; \
	fi
	git checkout --	debian/changelog
	fakeroot debian/rules clean
	sed -i "s/-DEV/-DEV-rev$(VERSION)-$(BRANCH)/" debian/changelog
	fakeroot debian/rules configure
	fakeroot debian/rules binary
	rm -rf debian/
	git checkout debian
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
	@echo "tests: run all tests. For more info, check gpac/regression_tests/test_suite_make.sh -h"
	@echo
	@echo "lcov: generate lcov files"
	@echo "lcov_clean: clean all lcov/gcov files"
	@echo
	@echo "docs:  build libgpac documentation in gpac/doc"

-include .depend
