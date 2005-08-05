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


/*! \file "gpac/tools.h"
 *	\brief Base definitions and functions of GPAC.
 *
 * This file contains basic functions and core definitions of the GPAC framework. This file is
 * usually included by all GPAC header files since it contains the error definitions.
*/

/*! \defgroup utils_grp utils
 *	You will find in this module the documentation of all tools used in GPAC.
*/

/*! \addtogroup tools_grp base utils
 *	\ingroup utils_grp
 *	\brief Base definitions and functions of GPAC.
 *
 *	This section documents some very basic functions and core definitions of the GPAC framework.
 *	@{
 */

/*!
 *	\brief GPAC Version
 *	\hideinitializer
 *
 *	Macro giving GPAC version expressed as a printable string
*/
/*KEEP SPACE SEPARATORS FOR MAKE / GREP (SEE MAIN MAKEFILE)!!!, and NO SPACE in GPAC_VERSION for proper install*/
#define GPAC_VERSION       "0.4.1-DEV"
/*!
 *	\brief GPAC Version
 *	\hideinitializer
 *
 *	Macro giving GPAC version expressed as an integer, where version X.Y.Z is coded as 0x00XXYYZZ
*/
#define GPAC_VERSION_INT	0x00000401

/*!
 *	\brief Memory allocation
 *	\hideinitializer
 *
 *	Macro allocating memory and zero-ing it
*/
#define GF_SAFEALLOC(__ptr, __size) __ptr = malloc(__size); if (__ptr) memset(__ptr, 0, __size);

/*!
 *	\brief 4CC Formatting
 *	\hideinitializer
 *
 *	Macro formating a 4-character code (or 4CC) "abcd" as 0xAABBCCDD
*/
#define GF_FOUR_CHAR_INT(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))
/*!
 *	\brief 4CC Printing
 *
 *	returns a 4CC printable form
*/
const char *gf_4cc_to_str(u32 type);

/*!
 *	\brief large file opening
 *
 *	Opens a large file (>4GB)
 *	\param file_name Same semantics as fopen
 *	\param mode Same semantics as fopen
 *	\return stream handle of the file object
 *	\note You only need to call this function if you're suspecting the file to be a large one (usually only media files), otherwise use regular stdio.
*/
FILE *gf_f64_open(const char *file_name, const char *mode);
/*!
 *	\brief large file position query
 *
 *	Queries the current read/write position in a large file
 *	\param f Same semantics as ftell
 *	\return position in the file
 *	\note You only need to call this function if you're suspecting the file to be a large one (usually only media files), otherwise use regular stdio.
*/
u64 gf_f64_tell(FILE *f);
/*!
 *	\brief large file seeking
 *
 *	Seeks the current read/write position in a large file
 *	\param f Same semantics as fseek
 *	\param pos Same semantics as fseek
 *	\param whence Same semantics as fseek
 *	\return new position in the file
 *	\note You only need to call this function if you're suspecting the file to be a large one (usually only media files), otherwise use regular stdio.
*/
u64 gf_f64_seek(FILE *f, s64 pos, s32 whence);

/*! @} */


/*! \addtogroup errors_grp error codes
 *	\ingroup utils_grp
 *	\brief Errors used in GPAC.
 *
 *	This section documents all error codes used in the GPAC framework. Most of the GPAC's functions will use these as 
 * return values, and some of these errors are also used for state communication with the different modules of the framework.
 *	@{
 */

/*!
 * GPAC Error
 *	\hideinitializer
 *
 *	positive values are warning and info, 0 means no error and negative values are errors
 */
typedef enum
{
	/*!Message from any scripting engine used in the presentation (ECMAScript, MPEG-J, ...) (Info).*/
	GF_SCRIPT_INFO						= 3,
	/*!Indicates an data frame has several AU packed (not MPEG-4 compliant). This is used by decoders to force 
	multiple decoding of the same data frame (Info).*/
	GF_PACKED_FRAMES					= 2,
	/*!Indicates the end of a stream or of a file (Info).*/
	GF_EOS								= 1,
	/*!
	\n\n
	*/
	/*!Operation success (no error).*/
	GF_OK								= 0,
	/*!\n*/
	/*!One of the input parameter is not correct or cannot be used in the current operating mode of the framework.*/
	GF_BAD_PARAM							= -1,
	/*! Memory allocation failure.*/
	GF_OUT_OF_MEM							= -2,
	/*! Input/Output failure (disk access, system call failures)*/
	GF_IO_ERR								= -3,
	/*! The desired feature or operation is not supported by the framework*/
	GF_NOT_SUPPORTED						= -4,
	/*! Input data has been corrupted*/
	GF_CORRUPTED_DATA						= -5,
	/*! A modification was attempted on a scene node which could not be found*/
	GF_SG_UNKNOWN_NODE						= -6,
	/*! The PROTO node interface does not match the nodes using it*/
	GF_SG_INVALID_PROTO						= -7,
	/*! An error occured in the scripting engine*/
	GF_SCRIPT_ERROR							= -8,
	/*! Buffer is too small to contain decoded data. Decoders shall use this error whenever they need to resize their output memory buffers*/
	GF_BUFFER_TOO_SMALL						= -9,
	/*! Bitstream is not compliant to the specfication it refers to*/
	GF_NON_COMPLIANT_BITSTREAM				= -10,
	/*! No decoders could be found to handle the desired media type*/
	GF_CODEC_NOT_FOUND						= -11,
	/*! The URL is not properly formatted or cannot be found*/
	GF_URL_ERROR							= -12,
	/*! An service error has occured at the local side*/
	GF_SERVICE_ERROR						= -13,
	/*! A service error has occured at the remote (server) side*/
	GF_REMOTE_SERVICE_ERROR					= -14,
	/*! The desired stream could not be found in the service*/
	GF_STREAM_NOT_FOUND						= -15,
	/*! The IsoMedia file is not a valid one*/
	GF_ISOM_INVALID_FILE					= -20,
	/*! The IsoMedia file is not complete. Either the file is being downloaded, or it has been truncated*/
	GF_ISOM_INCOMPLETE_FILE					= -21,
	/*! The media in this IsoMedia track is not valid (usually due to a broken stream description)*/
	GF_ISOM_INVALID_MEDIA					= -22,
	/*! The requested operation cannot happen in the current opening mode of the IsoMedia file*/
	GF_ISOM_INVALID_MODE					= -23,
	/*! This IsoMedia track refers to media outside the file in an unknown way*/
	GF_ISOM_UNKNOWN_DATA_REF				= -24,

	/*! An invalid MPEG-4 Object Descriptor was found*/
	GF_ODF_INVALID_DESCRIPTOR				= -30,
	/*! An MPEG-4 Object Descriptor was found or added to a forbidden descriptor*/
	GF_ODF_FORBIDDEN_DESCRIPTOR				= -31,
	/*! An invalid MPEG-4 BIFS command was detected*/
	GF_ODF_INVALID_COMMAND					= -32,
	/*! The scene has been encoded using an unknown BIFS version*/
	GF_BIFS_UNKNOWN_VERSION					= -33,

	/*! The remote IP address could not be solved*/
	GF_IP_ADDRESS_NOT_FOUND					= -40,
	/*! The connection to the remote peer has failed*/
	GF_IP_CONNECTION_FAILURE				= -41,
	/*! The network operation has failed*/
	GF_IP_NETWORK_FAILURE					= -42,
	/*! The network connection has been closed*/
	GF_IP_CONNECTION_CLOSED					= -43,
	/*! The network operation has failed because no data is available*/
	GF_IP_NETWORK_EMPTY						= -44,
	/*! The network operation has been discarded because it would be a blocking one*/
	GF_IP_SOCK_WOULD_BLOCK					= -45,
	/*! UDP connection did not receive any data at all. Signaled by client services to reconfigure network if possible*/
	GF_IP_UDP_TIMEOUT						= -46,

	/*! Authentication with the remote host has failed*/
	GF_AUTHENTICATION_FAILURE				= -50,
} GF_Err;

/*!
 *	\brief Error Printing
 *
 *	Returns a printable version of a given error
 *	\param e Error code requested
 *	\return String representing the error
*/
const char *gf_error_to_string(GF_Err e);


/*! @} */

/*! \addtogroup tools_grp
 *	@{
 */


/*!
 *	\brief PseudoRandom Integer Generation Initialization
 *
 *	Sets the starting point for generating a series of pseudorandom integers.
 *	\param Reset Re-initializes the random number generator
*/
void gf_rand_init(Bool Reset);
/*!
 *	\brief PseudoRandom Integer Generation
 *
 *	Returns a pseudorandom integer.
*/
u32 gf_rand();

/*!
 *	\brief Directory Enumeration Callback
 *
 * The gf_enum_dir_item type is the type for the callback of the \ref gf_enum_directory function
 *	\param cbck Opaque user data.
 *	\param item_name File or directory name.
 *	\param item_path File or directory full path and name from filesystem root.
 *	\return 1 to abort enumeration, 0 to continue enumeration.
 *
 */
typedef Bool (*gf_enum_dir_item)(void *cbck, char *item_name, char *item_path);
/*!
 *	\brief Directory enumeration
 *
 *	Enumerates a directory content. Feedback is provided by the enum_dir_item function
 *	\param dir Directory to enumerate
 *	\param enum_directory If set, only directories will be enumerated, otherwise only files are.
 *	\param enum_dir \ref gf_enum_dir_item callback function for enumeration. 
 *	\param cbck Opaque user data passed to callback function.
*/
GF_Err gf_enum_directory(const char *dir, Bool enum_directory, gf_enum_dir_item enum_dir, void *cbck);


/*!
 *	\brief File Deletion
 *
 *	Deletes a file from the disk.
 *	\param fileName absolute name of the file or name relative to the current working directory.
*/
void gf_delete_file(char *fileName);
/*!
 *	\brief File Deletion
 *
 *	Creates a new temporary file in binary mode
 *	\return stream handle to the new file ressoucre
 */
FILE *gf_temp_file_new();

/*!
 *	\brief System clock setup
 *
 *	Inits the system clock. The framework works with a clock resolution of 1 millisecond.
 *	\note This can be called several times but only the first call will result in clock setup.
 */
void gf_sys_clock_start();
/*!
 *	\brief System clock closing
 *
 *	Closes the system clock and any associated ressources.
 *	\note This can be called several times but the clock will be closed when no more users are counted.
 */
void gf_sys_clock_stop();
/*!
 *	\brief System clock query
 *
 *	Gets the system clock time.
 *	\return System clock value since initialization in milliseconds.
 */
u32 gf_sys_clock();

/*!
 *	\brief Sleeps thread/process
 *
 *	Locks calling thread/process execution for a given time.
 *	\param ms Amount of time to sleep in milliseconds.
 */
void gf_sleep(u32 ms);

/*!
 *	\brief Field bit-size 
 *
 *	Gets the number of bits needed to represent the value.
 *	\param MaxVal Maximum value to be represented.
 *	\return number of bits required to represent the value.
 */
u32 gf_get_bit_size(u32 MaxVal);

/*formats progress to stdout, title IS a string, but declared as void * to avoid GCC warnings!! */
/*!
 *	\brief Progress formatting
 *
 *	Formats a progress bar to standard output
 *	\param title title string of the progress, or NULL for no progress
 *	\param done Current amount performed of the action.
 *	\param total Total amount of the action.
 *	\note "title" is declared as void * to avoid GCC warnings when used as a callback.
 */
void gf_cbk_on_progress(void *title, u32 done, u32 total);


/*! @} */

#ifdef __cplusplus
}
#endif


#endif		/*_GF_CORE_H_*/

