/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / Elementary Stream Interface sub-project
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
 *	\file <gpac/main.h>
 *	\brief main() macro for win32.
 */
	
/*!
 *	\addtogroup bascod_grp Base coding
 *	\ingroup utils_grp
 *	\brief main() macro for win32.
 *
 *	@{
 */

#include <gpac/setup.h>




#if defined(WIN32) && !defined(NO_WMAIN)

#define GPAC_DEC_MAIN(__fun) \
int wmain( int argc, wchar_t** wargv )\
{\
	int i;\
	int res;\
	size_t len;\
	size_t res_len;\
	char **argv;\
	argv = (char **)malloc(argc*sizeof(wchar_t *));\
	for (i = 0; i < argc; i++) {\
		wchar_t *src_str = wargv[i];\
		len = UTF8_MAX_BYTES_PER_CHAR*gf_utf8_wcslen(wargv[i]);\
		argv[i] = (char *)malloc(len + 1);\
		res_len = gf_utf8_wcstombs(argv[i], len, &src_str);\
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

#define GPAC_DEC_MAIN(__fun) \
int main(int argc, char **argv) {\
	return __fun( argc, argv ); \
}

#endif //win32


/*! @} */

#ifdef __cplusplus
}
#endif

#endif	//_GF_ESI_H_

