/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2020
 *					All rights reserved
 *
 *  This file is part of GPAC
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _GF_MAIN_H_
#define _GF_MAIN_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
\file <gpac/main.h>
\brief main() macro for win32.
 */

/*!
\addtogroup sysmain_grp
@{

Thiis section decribes functions useful when developing an application using libgpac such as:
- quick UTF8 conversion of arguments for main() on windows
- setting, checking and printing libgpac arguments as given from command line
*/

#include <gpac/setup.h>
#include <gpac/utf.h>




#if defined(WIN32) && !defined(NO_WMAIN)
/*! macro for main() with wide to char conversion on windows platforms*/
#define GF_MAIN_FUNC(__fun) \
int wmain( int argc, wchar_t** wargv )\
{\
	int i;\
	int res;\
	u32 len;\
	u32 res_len;\
	char **argv;\
	argv = (char **)malloc(argc*sizeof(wchar_t *));\
	for (i = 0; i < argc; i++) {\
		wchar_t *src_str = wargv[i];\
		len = UTF8_MAX_BYTES_PER_CHAR*gf_utf8_wcslen(wargv[i]);\
		argv[i] = (char *)malloc(len + 1);\
		res_len = gf_utf8_wcstombs(argv[i], len, (const unsigned short **) &src_str);\
		if (res_len != GF_UTF8_FAIL)\
			argv[i][res_len] = 0;\
		if (res_len > len) {\
			fprintf(stderr, "Length allocated for conversion of wide char to UTF-8 not sufficient\n");\
			return -1;\
		}\
	}\
	res = __fun(argc, argv);\
	for (i = 0; i < argc; i++) {\
		free(argv[i]);\
	}\
	free(argv);\
	return res;\
}

#else

/*! macro for main() with wide to char conversion on windows platforms*/
#define GF_MAIN_FUNC(__fun) \
int main(int argc, char **argv) {\
	return __fun( argc, argv ); \
}

#endif //win32

/*! macro defining fields of a libgpac arg (not a filter arg)*/
#define GF_GPAC_ARG_BASE \
	/*! name of arg*/ \
	const char *name; \
	/*! alternate name of arg*/ \
	const char *altname; \
	/*! description of arg*/ \
	const char *description; \
	/*! default value of arg*/ \
	const char *val; \
	/*! possible value of arg*/ \
	const char *values; \
	/*! argument type for UI construction - note that argument values are not parsed and shall be set as strings*/ \
	u16 type; \
	/*! argument flags*/ \
	u16 flags; \


/*! structure holding a libgpac arg (not a filter arg)*/
typedef struct
{
	/*! base structure, shall always be placed first if you extend args in your application*/
	GF_GPAC_ARG_BASE
} GF_GPACArg;

//these 3 values match argument hints of filters
/*! argument is of advanced type*/
#define GF_ARG_HINT_ADVANCED	(1<<1)
/*! argument is of expert type*/
#define GF_ARG_HINT_EXPERT		(1<<2)
/*! argument should not be presented in UIs*/
#define GF_ARG_HINT_HIDE 		(1<<3)
/*! argument is highly experimental*/
#define GF_ARG_HINT_EXPERIMENTAL	(1<<4)

/*! argument applies to the libgpac core subsystem*/
#define GF_ARG_SUBSYS_CORE 		(1<<5)
/*! argument applies to the log subsystem*/
#define GF_ARG_SUBSYS_LOG 		(1<<6)
/*! argument applies to the filter subsystem*/
#define GF_ARG_SUBSYS_FILTERS 	(1<<7)
/*! argument applies to the HTTP subsystem*/
#define GF_ARG_SUBSYS_HTTP 		(1<<8)
/*! argument applies to the video subsystem*/
#define GF_ARG_SUBSYS_VIDEO 		(1<<9)
/*! argument applies to the audio subsystem*/
#define GF_ARG_SUBSYS_AUDIO 		(1<<10)
/*! argument applies to the font and text subsystem*/
#define GF_ARG_SUBSYS_TEXT 		(1<<11)
/*! argument applies to the remotery subsystem*/
#define GF_ARG_SUBSYS_RMT 		(1<<12)
/*! argument belongs to hack tools, usually never used*/
#define GF_ARG_SUBSYS_HACKS 		(1<<13)

/*! argument is a boolean*/
#define GF_ARG_BOOL		0
/*! argument is a 32 bit integer*/
#define GF_ARG_INT		1
/*! argument is a double*/
#define GF_ARG_DOUBLE	2
/*! argument is a string*/
#define GF_ARG_STRING	3
/*! argument is a camma-separated list of strings*/
#define GF_ARG_STRINGS	4
/*! argument is a custom arg, default value contains the syntax of the argument*/
#define GF_ARG_4CC		5
/*! argument is a custom arg, default value contains the syntax of the argument*/
#define GF_ARG_4CCS		6
/*! argument is a custom arg, default value contains the syntax of the argument*/
#define GF_ARG_CUSTOM	7

/*! macros for defining a GF_GPACArg argument*/
#define GF_DEF_ARG(_a, _b, _c, _d, _e, _f, _g) {_a, _b, _c, _d, _e, _f, _g}

/*! gets the options defined for libgpac
\return array of options*/
const GF_GPACArg *gf_sys_get_options();

/*! check if the given option is a libgpac argument
\param arg_name name of the argument
\return 0 if not a libgpac core option, 1 if option not consuming an argument, 2 if option consuming an argument*/
u32 gf_sys_is_gpac_arg(const char *arg_name);

/*! parses config string and update config accordingly
\param opt_string section/key/val formatted as Section:Key (discard key), Section:Key=null (discard key), Section:Key=Val (set key) or Section=null (discard section)
\return GF_TRUE if update is OK, GF_FALSE otherwise*/
Bool gf_sys_set_cfg_option(const char *opt_string);

/*! argument dump hint options */
typedef enum
{
	/*! only dumps simple arguments*/
	GF_ARGMODE_BASE=0,
	/*! only dumps advanced arguments*/
	GF_ARGMODE_ADVANCED,
	/*! only dumps expert arguments*/
	GF_ARGMODE_EXPERT,
	/*! dumps all arguments*/
	GF_ARGMODE_ALL
} GF_SysArgMode;

/*! flags for help formatting*/
typedef enum
{
	/*! first word in format string should be highlighted */
 	GF_PRINTARG_HIGHLIGHT_FIRST = 1,
	/*! prints <br/> instead of new line*/
	GF_PRINTARG_NL_TO_BR = 1<<1,
	/*! first word in format string is an option descripttor*/
	GF_PRINTARG_OPT_DESC = 1<<2,
	/*! the format string is an application string, not a gpac core one*/
	GF_PRINTARG_IS_APP = 1<<3,
	/*! insert an extra '-' at the beginning*/
	GF_PRINTARG_ADD_DASH = 1<<4,
	/*! do not insert '-' before arg name*/
	GF_PRINTARG_NO_DASH = 1<<5,
	/*! insert '-: before arg name*/
	GF_PRINTARG_COLON = 1<<6,
	/*! the generation is for markdown*/
	GF_PRINTARG_MD = 1<<16,
	/*! the generation is for man pages*/
	GF_PRINTARG_MAN = 1<<17,
	/*! XML < and > should be escaped (for  markdown generation only) */
	GF_PRINTARG_ESCAPE_XML = 1<<18,
	/*! '|' should be escaped (for  markdown generation only) */
	GF_PRINTARG_ESCAPE_PIPE = 1<<19,
} GF_SysPrintArgFlags;


/*! prints a argument
\param helpout destination file - if NULL, uses stderr
\param flags dump flags
\param arg argument to print
\param arg_subsystem name of subsystem of argument (core, gpac, filter name) for localization)
*/
void gf_sys_print_arg(FILE *helpout, GF_SysPrintArgFlags flags, const GF_GPACArg *arg, const char *arg_subsystem);

/*! prints libgpac help for builton core options to stderr
\param helpout destination file - if NULL, uses stderr
\param flags dump flags
\param mode filtering mode based on argument  type
\param subsystem_flags filtering mode based on argument subsytem flags
*/
void gf_sys_print_core_help(FILE *helpout, GF_SysPrintArgFlags flags, GF_SysArgMode mode, u32 subsystem_flags);

/*! gets localized version of string identified by module name and identifier.
\param sec_name name of the module to query, such as "gpac", "core", or filter name
\param str_name name of string to query, such as acore/app option or a filter argument
\param def_val default value to return if no locaization exists
\return localized version of the string
*/
const char *gf_sys_localized(const char *sec_name, const char *str_name, const char *def_val);

/*! formats help to output
\param output output file to dump to
\param flags help formatting flags
\param fmt arguments of the format
*/
void gf_sys_format_help(FILE *output, GF_SysPrintArgFlags flags, const char *fmt, ...);

/*! very basic word match, check the number of source characters in order in dest
\param orig word to test
\param dst word to compare to
\return GF_TRUE if words are similar, GF_FALSE otherwise
*/
Bool gf_sys_word_match(const char *orig, const char *dst);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif	//_GF_MAIN_H_

