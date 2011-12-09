###########################################################################################
##
## Copyright 2011 Telecom Paristech
## Author : Stanislas Selle
##
###########################################################################################

###  Configure.ac for hbbtvterminal


AC_INIT([hbbtvterminal], [0.1.0], [stanislas.selle@telecom-paristech.fr])

AC_CONFIG_AUX_DIR([build-aux])

AC_CONFIG_MACRO_DIR([m4])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_INIT_AUTOMAKE([foreign -Wall -Werror])

#check programs
AC_PROG_CXX

#check modules
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.28.0])
PKG_CHECK_MODULES([GTK], [gtk+-2.0 >= 2.20.1])
PKG_CHECK_MODULES([WEBKIT], [webkit-1.0 >= 1.5.2])
PKG_CHECK_MODULES([PIXMAN], [pixman-1 >= 0.16.4])
PKG_CHECK_MODULES([HBBTVBROWSERPLUGIN], [hbbtvbrowserplugin >= 0.0.3])

# Checks for header files.
AC_PATH_X

# check header
AC_HEADER_STDBOOL

# makefiles
AC_CONFIG_FILES([makefile src/makefile])


AC_OUTPUT
