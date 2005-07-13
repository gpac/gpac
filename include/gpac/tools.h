/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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

#ifndef _GF_TOOLS_H_
#define _GF_TOOLS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/setup.h>

/*build version - KEEP SPACE SEPARATORS FOR MAKE / GREP (SEE MAIN MAKEFILE)!!!, and NO SPACE in GPAC_VERSION for proper install*/
#define GPAC_VERSION       "0.4.0-DEV"
#define GPAC_VERSION_INT	0x00000400

#define FOUR_CHAR_INT( a, b, c, d ) (((a)<<24)|((b)<<16)|((c)<<8)|(d))
/*prints 4CC code to name - name MUST BE AT LEAST 5 bytes (4cc + 0 term byte) - returns val*/
char *gf_4cc_to_str(u32 type, char *name);


#define SAFEALLOC(__ptr, __size) __ptr = malloc(__size); if (__ptr) memset(__ptr, 0, __size);

/**********************************************************************
					ERROR DEFINITIONS
**********************************************************************/

typedef s32 GF_Err;
/*error types:
positive values are warning/info
0 is no error
negative values are errors
*/
enum
{
	/*script message*/
	GF_SCRIPT_INFO						= 3,
	/*signal a framed AU has several AU packed (not compliant) used by decoders*/
	GF_PACKED_FRAMES					= 2,
	/*end of stream / end of file*/
	GF_EOS								= 1,
	/*no error*/
	GF_OK								= 0,

	/* General errors */
	GF_BAD_PARAM							= -1,
	GF_OUT_OF_MEM							= -2,
	GF_IO_ERR								= -3,
	GF_NOT_SUPPORTED						= -4,
	GF_CORRUPTED_DATA						= -5,
	GF_SG_UNKNOWN_NODE						= -6,
	GF_SG_INVALID_PROTO						= -7,
	GF_SCRIPT_ERROR							= -8,
	GF_BUFFER_TOO_SMALL						= -9,
	GF_NON_COMPLIANT_BITSTREAM				= -10,
	GF_CODEC_NOT_FOUND						= -11,
	GF_URL_ERROR							= -12,
	GF_SERVICE_ERROR						= -13,
	GF_REMOTE_SERVICE_ERROR					= -14,
	GF_STREAM_NOT_FOUND						= -15,

	/* File Format Errors */
	GF_ISOM_INVALID_FILE					= -20,
	GF_ISOM_INCOMPLETE_FILE					= -21,
	GF_ISOM_INVALID_MEDIA					= -22,
	GF_ISOM_INVALID_MODE					= -23,
	GF_ISOM_UNKNOWN_DATA_REF				= -24,

	/* MPEG-4 BIFS & Object GF_Descriptor Errors */
	GF_ODF_INVALID_DESCRIPTOR				= -30,
	GF_ODF_FORBIDDEN_DESCRIPTOR				= -31,
	GF_ODF_INVALID_COMMAND					= -32,
	GF_BIFS_UNKNOWN_VERSION					= -33,

	/*network errors*/
	GF_IP_ADDRESS_NOT_FOUND					= -40,
	GF_IP_CONNECTION_FAILURE				= -41,
	GF_IP_NETWORK_FAILURE					= -42,
	GF_IP_CONNECTION_CLOSED					= -43,
	GF_IP_NETWORK_EMPTY						= -44,
	GF_IP_SOCK_WOULD_BLOCK					= -45,
	GF_IP_UDP_TIMEOUT						= -46,

	/*autrhentication errors - needs refinement...*/
	GF_AUTHENTICATION_FAILURE				= -50,
};

const char *gf_error_to_string(GF_Err e);

/* Random Generator inits (and reset if needed)*/
void gf_rand_init(Bool Reset);
/*get random*/
u32 gf_rand();

/*gets current user name*/
void gf_get_user_name(char *buf, u32 buf_size);


/*enumerate direcory content. 
	@enum_directory: if set, only directories are enumerated. Otherwise only files are
	@enum_dir_item: callback function for enumeration. if function returns TRUE enumeration is aborted. item_path is the full
	address of the enumerated item (Path + name)
	@cbck: private user callback data
*/
GF_Err gf_enum_directory(const char *dir, Bool enum_directory, Bool (*enum_dir_item)(void *cbck, char *item_name, char *item_path), void *cbck);


/* the DeleteFile function */
void gf_delete_file(char *fileName);
/* returns new temp file*/
FILE *gf_temp_file_new();

/* the time functions*/
void gf_sys_clock_start();
void gf_sys_clock_stop();
u32 gf_sys_clock();

/*get number of bits needed to represent the value*/
u32 gf_get_bit_size(u32 MaxVal);

/*formats progress to stdout, title IS a string, but declared as void * to avoid GCC warnings!! */
void gf_cbk_on_progress(void *title, u32 done, u32 total);

#ifdef __cplusplus
}
#endif


#endif		/*_GF_CORE_H_*/

