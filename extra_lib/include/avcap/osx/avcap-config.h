#ifndef _AVCAP_CONFIG_H
#define _AVCAP_CONFIG_H 1
 
/* avcap-config.h. Generated automatically at end of configure. */
/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.in by autoheader.  */

/* Define to 1 if you have the `alarm' function. */
#ifndef AVCAP_HAVE_ALARM 
#define AVCAP_HAVE_ALARM  1 
#endif

/* Define to 1 if you have the <dlfcn.h> header file. */
#ifndef AVCAP_HAVE_DLFCN_H 
#define AVCAP_HAVE_DLFCN_H  1 
#endif

/* Define to 1 if you have the <fcntl.h> header file. */
#ifndef AVCAP_HAVE_FCNTL_H 
#define AVCAP_HAVE_FCNTL_H  1 
#endif

/* Define to 1 if you have the <float.h> header file. */
#ifndef AVCAP_HAVE_FLOAT_H 
#define AVCAP_HAVE_FLOAT_H  1 
#endif

/* Define to 1 if you have the `getpagesize' function. */
#ifndef AVCAP_HAVE_GETPAGESIZE 
#define AVCAP_HAVE_GETPAGESIZE  1 
#endif

/* Define to 1 if you have the `gettimeofday' function. */
#ifndef AVCAP_HAVE_GETTIMEOFDAY 
#define AVCAP_HAVE_GETTIMEOFDAY  1 
#endif

/* Define to 1 if you have the <inttypes.h> header file. */
#ifndef AVCAP_HAVE_INTTYPES_H 
#define AVCAP_HAVE_INTTYPES_H  1 
#endif

/* Define to 1 if you have the <limits.h> header file. */
#ifndef AVCAP_HAVE_LIMITS_H 
#define AVCAP_HAVE_LIMITS_H  1 
#endif

/* Define to 1 if your system has a GNU libc compatible `malloc' function, and
   to 0 otherwise. */
#ifndef AVCAP_HAVE_MALLOC 
#define AVCAP_HAVE_MALLOC  1 
#endif

/* Define to 1 if you have the <memory.h> header file. */
#ifndef AVCAP_HAVE_MEMORY_H 
#define AVCAP_HAVE_MEMORY_H  1 
#endif

/* Define to 1 if you have the `memset' function. */
#ifndef AVCAP_HAVE_MEMSET 
#define AVCAP_HAVE_MEMSET  1 
#endif

/* Define to 1 if you have a working `mmap' system call. */
#ifndef AVCAP_HAVE_MMAP 
#define AVCAP_HAVE_MMAP  1 
#endif

/* Define to 1 if you have the `munmap' function. */
#ifndef AVCAP_HAVE_MUNMAP 
#define AVCAP_HAVE_MUNMAP  1 
#endif

/* Define to 1 if you have the `pow' function. */
#ifndef AVCAP_HAVE_POW 
#define AVCAP_HAVE_POW  1 
#endif

/* Define to 1 if you have the `select' function. */
#ifndef AVCAP_HAVE_SELECT 
#define AVCAP_HAVE_SELECT  1 
#endif

/* Define to 1 if you have the `sqrt' function. */
#ifndef AVCAP_HAVE_SQRT 
#define AVCAP_HAVE_SQRT  1 
#endif

/* Define to 1 if `stat' has the bug that it succeeds when given the
   zero-length file name argument. */
/* #undef HAVE_STAT_EMPTY_STRING_BUG */

/* Define to 1 if stdbool.h conforms to C99. */
#ifndef AVCAP_HAVE_STDBOOL_H 
#define AVCAP_HAVE_STDBOOL_H  1 
#endif

/* Define to 1 if you have the <stddef.h> header file. */
#ifndef AVCAP_HAVE_STDDEF_H 
#define AVCAP_HAVE_STDDEF_H  1 
#endif

/* Define to 1 if you have the <stdint.h> header file. */
#ifndef AVCAP_HAVE_STDINT_H 
#define AVCAP_HAVE_STDINT_H  1 
#endif

/* Define to 1 if you have the <stdlib.h> header file. */
#ifndef AVCAP_HAVE_STDLIB_H 
#define AVCAP_HAVE_STDLIB_H  1 
#endif

/* Define to 1 if you have the `strerror' function. */
#ifndef AVCAP_HAVE_STRERROR 
#define AVCAP_HAVE_STRERROR  1 
#endif

/* Define to 1 if you have the <strings.h> header file. */
#ifndef AVCAP_HAVE_STRINGS_H 
#define AVCAP_HAVE_STRINGS_H  1 
#endif

/* Define to 1 if you have the <string.h> header file. */
#ifndef AVCAP_HAVE_STRING_H 
#define AVCAP_HAVE_STRING_H  1 
#endif

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#ifndef AVCAP_HAVE_SYS_IOCTL_H 
#define AVCAP_HAVE_SYS_IOCTL_H  1 
#endif

/* Define to 1 if you have the <sys/select.h> header file. */
#ifndef AVCAP_HAVE_SYS_SELECT_H 
#define AVCAP_HAVE_SYS_SELECT_H  1 
#endif

/* Define to 1 if you have the <sys/socket.h> header file. */
#ifndef AVCAP_HAVE_SYS_SOCKET_H 
#define AVCAP_HAVE_SYS_SOCKET_H  1 
#endif

/* Define to 1 if you have the <sys/stat.h> header file. */
#ifndef AVCAP_HAVE_SYS_STAT_H 
#define AVCAP_HAVE_SYS_STAT_H  1 
#endif

/* Define to 1 if you have the <sys/time.h> header file. */
#ifndef AVCAP_HAVE_SYS_TIME_H 
#define AVCAP_HAVE_SYS_TIME_H  1 
#endif

/* Define to 1 if you have the <sys/types.h> header file. */
#ifndef AVCAP_HAVE_SYS_TYPES_H 
#define AVCAP_HAVE_SYS_TYPES_H  1 
#endif

/* Define to 1 if you have the <unistd.h> header file. */
#ifndef AVCAP_HAVE_UNISTD_H 
#define AVCAP_HAVE_UNISTD_H  1 
#endif

/* Define to 1 if the system has the type `_Bool'. */
/* #undef HAVE__BOOL */

/* Compile avcap for Linux */
/* #undef LINUX */

/* Define to 1 if `lstat' dereferences a symlink specified with a trailing
   slash. */
/* #undef LSTAT_FOLLOWS_SLASHED_SYMLINK */

/* Compile avcap for Mac OS X */
#ifndef AVCAP_OSX 
#define AVCAP_OSX  1 
#endif

/* Name of package */
#ifndef AVCAP_PACKAGE 
#define AVCAP_PACKAGE  "avcap" 
#endif

/* Define to the address where bug reports for this package should be sent. */
#ifndef AVCAP_PACKAGE_BUGREPORT 
#define AVCAP_PACKAGE_BUGREPORT  "Nico.Pranke@googlemail.com" 
#endif

/* Define to the full name of this package. */
#ifndef AVCAP_PACKAGE_NAME 
#define AVCAP_PACKAGE_NAME  "A video capture library" 
#endif

/* Define to the full name and version of this package. */
#ifndef AVCAP_PACKAGE_STRING 
#define AVCAP_PACKAGE_STRING  "A video capture library 0.1.9" 
#endif

/* Define to the one symbol short name of this package. */
#ifndef AVCAP_PACKAGE_TARNAME 
#define AVCAP_PACKAGE_TARNAME  "avcap" 
#endif

/* Define to the version of this package. */
#ifndef AVCAP_PACKAGE_VERSION 
#define AVCAP_PACKAGE_VERSION  "0.1.9" 
#endif

/* Define to the type of arg 1 for `select'. */
#ifndef AVCAP_SELECT_TYPE_ARG1 
#define AVCAP_SELECT_TYPE_ARG1  int 
#endif

/* Define to the type of args 2, 3 and 4 for `select'. */
#ifndef AVCAP_SELECT_TYPE_ARG234 
#define AVCAP_SELECT_TYPE_ARG234  (fd_set *) 
#endif

/* Define to the type of arg 5 for `select'. */
#ifndef AVCAP_SELECT_TYPE_ARG5 
#define AVCAP_SELECT_TYPE_ARG5  (struct timeval *) 
#endif

/* Define to 1 if you have the ANSI C header files. */
#ifndef AVCAP_STDC_HEADERS 
#define AVCAP_STDC_HEADERS  1 
#endif

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#ifndef AVCAP_TIME_WITH_SYS_TIME 
#define AVCAP_TIME_WITH_SYS_TIME  1 
#endif

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Version number of package */
#ifndef AVCAP_VERSION 
#define AVCAP_VERSION  "0.1.9" 
#endif

/* Compile avcap for Windows */
/* #undef WINDOWS */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to rpl_malloc if the replacement function should be used. */
/* #undef malloc */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to empty if the keyword `volatile' does not work. Warning: valid
   code using `volatile' can become incorrect without. Disable with care. */
/* #undef volatile */
 
/* once: _AVCAP_CONFIG_H */
#endif
