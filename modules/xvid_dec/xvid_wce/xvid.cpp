/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Native API implementation  -
 *
 *  Copyright(C) 2001-2003 Peter Ross <pross@xvid.org>
 *
 *  This program is free software ; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation ; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * $Id: xvid.cpp,v 1.1.1.1 2005-07-13 14:36:16 jeanlf Exp $
 *
 ****************************************************************************/

#include "xvid.h"
#include "decoder.h"
#include "interpolate8x8.h"
#include "reduced.h"
#include "mem_transfer.h"
#include "quant.h"

#ifdef _DEBUG
dword xvid_debug = 0;
#endif

/*****************************************************************************
 * XviD Init Entry point
 *
 * Well this function initialize all internal function pointers according
 * to the CPU features forced by the library client or autodetected (depending
 * on the XVID_CPU_FORCE flag). It also initializes vlc coding tables and all
 * image colorspace transformation tables.
 *
 * Returned value : XVID_ERR_OK
 *                  + API_VERSION in the input XVID_INIT_PARAM structure
 *                  + core build  "   "    "       "               "
 *
 ****************************************************************************/


static int xvid_gbl_init(xvid_gbl_init_t * init) {
	//unsigned int cpu_flags;

	if (XVID_VERSION_MAJOR(init->version) != 1)
		return XVID_ERR_VERSION;

#if defined(_DEBUG)
	xvid_debug = init->debug;
#endif

	return 0;
}

/*****************************************************************************
 * XviD Global Entry point
 *
 * Well this function initialize all internal function pointers according
 * to the CPU features forced by the library client or autodetected (depending
 * on the XVID_CPU_FORCE flag). It also initializes vlc coding tables and all
 * image colorspace transformation tables.
 *
 ****************************************************************************/

int xvid_global(void *handle, int opt, void *param1, void *param2) {

	switch(opt) {
	case XVID_GBL_INIT :
		return xvid_gbl_init((xvid_gbl_init_t*)param1);
	/*
	case XVID_GBL_INFO :
	   return xvid_gbl_info((xvid_gbl_info_t*)param1);
	case XVID_GBL_CONVERT:
	   return xvid_gbl_convert((xvid_gbl_convert_t*)param1);
	   */

	default :
		return XVID_ERR_FAIL;
	}
}

/*****************************************************************************
 * XviD Native decoder entry point
 *
 * This function is just a wrapper to all the option cases.
 *
 * Returned values : XVID_ERR_FAIL when opt is invalid
 *                   else returns the wrapped function result
 *
 ****************************************************************************/
int xvid_decore(void *handle, int opt, void *param1, void *param2) {

	switch (opt) {
	case XVID_DEC_CREATE:
		return decoder_create((xvid_dec_create_t *) param1);

	case XVID_DEC_DESTROY:
		delete (S_decoder*)handle;
		return 0;

	case XVID_DEC_DECODE:
		return ((S_decoder*)handle)->Decode((xvid_dec_frame_t *) param1, (xvid_dec_stats_t*) param2);

	default:
		return XVID_ERR_FAIL;
	}
}

//----------------------------

extern const dword scan_tables[3][64];
const dword scan_tables[3][64] = {
	/* zig_zag_scan */
	{	0,  1,  8, 16,  9,  2,  3, 10,
		17, 24, 32, 25, 18, 11,  4,  5,
		12, 19, 26, 33, 40, 48, 41, 34,
		27, 20, 13,  6,  7, 14, 21, 28,
		35, 42, 49, 56, 57, 50, 43, 36,
		29, 22, 15, 23, 30, 37, 44, 51,
		58, 59, 52, 45, 38, 31, 39, 46,
		53, 60, 61, 54, 47, 55, 62, 63
	},

	/* horizontal_scan */
	{	0,  1,  2,  3,  8,  9, 16, 17,
		10, 11,  4,  5,  6,  7, 15, 14,
		13, 12, 19, 18, 24, 25, 32, 33,
		26, 27, 20, 21, 22, 23, 28, 29,
		30, 31, 34, 35, 40, 41, 48, 49,
		42, 43, 36, 37, 38, 39, 44, 45,
		46, 47, 50, 51, 56, 57, 58, 59,
		52, 53, 54, 55, 60, 61, 62, 63
	},

	/* vertical_scan */
	{	0,  8, 16, 24,  1,  9,  2, 10,
		17, 25, 32, 40, 48, 56, 57, 49,
		41, 33, 26, 18,  3, 11,  4, 12,
		19, 27, 34, 42, 50, 58, 35, 43,
		51, 59, 20, 28,  5, 13,  6, 14,
		21, 29, 36, 44, 52, 60, 37, 45,
		53, 61, 22, 30,  7, 15, 23, 31,
		38, 46, 54, 62, 39, 47, 55, 63
	}
};

//----------------------------

#ifdef WIN32
# include <windows.h>
# include <stdio.h>

#ifdef _DEBUG
#    define snprintf _snprintf
#    define vsnprintf _vsnprintf

void DPRINTF(int level, char *fmt, int p) {
	if(xvid_debug & level) {
		//va_list args;
		char buf[DPRINTF_BUF_SZ];
		//va_start(args, fmt);
		//vsprintf(buf, fmt, args);
		sprintf(buf, fmt, p);
		OutputDebugString(buf);
		//fprintf(stderr, "%s", buf);
	}
}
#endif

#elif defined _WINDOWS

inline void DPRINTF(int level, char *fmt, int p) {
}

#endif

//----------------------------

