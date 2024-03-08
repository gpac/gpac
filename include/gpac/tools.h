/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2024
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
#include <gpac/version.h>
#include <time.h>


/*!
\file "gpac/tools.h"
\brief Core definitions and tools of GPAC.

This file contains basic functions and core definitions of the GPAC framework. This file is
usually included by all GPAC header files since it contains the error definitions.
*/

/*!
\addtogroup utils_grp Core Tools
\brief Core definitions and tools of GPAC.

You will find in this module the documentation of the core tools used in GPAC.
*/

/*!
\addtogroup setup_grp
@{
 */

/*!
\brief Stringizer
\hideinitializer

Macro transforming its input name into a string
*/
#define gf_stringizer(x) #x

/*!
\brief 4CC Formatting
\hideinitializer

Macro formatting a 4-character code (or 4CC) "abcd" as 0xAABBCCDD
*/
#ifndef GF_4CC
#define GF_4CC(a,b,c,d) ((((u32)a)<<24)|(((u32)b)<<16)|(((u32)c)<<8)|((u32)d))
#endif

/*! minimum buffer size to hold any 4CC in string format*/
#define GF_4CC_MSIZE	10

/*! converts four character code to string
\param type a four character code
\return a printable form of the code
*/
const char *gf_4cc_to_str(u32 type);

/*! converts four character code to string in the provided buffer - thread-safe version of \ref gf_4cc_to_str
\param type a four character code
\param szType buffer to write the code to
\return the onput buffer
*/
const char *gf_4cc_to_str_safe(u32 type, char szType[GF_4CC_MSIZE]);

/*! converts a 4CC string to its 32 bits value
\param val  four character string
\return code value or 0 if error
*/
u32 gf_4cc_parse(const char *val);

/*! @} */

/*!
\addtogroup errors_grp
\brief Error Types

This section documents all error codes used in the GPAC framework. Most of the GPAC's functions will use these as
 return values, and some of these errors are also used for state communication with the different modules of the framework.

@{
 */

/*! GPAC Error
\hideinitializer

Positive values are warning and info, 0 means no error and negative values are errors
 */
typedef enum
{
	/*!Message from any scripting engine used in the presentation (ECMAScript, MPEG-J, ...) (Info).*/
	GF_SCRIPT_INFO                                          = 3,
	/*!Indicates a send packet is not dispatched due to pending connections.*/
	GF_PENDING_PACKET					= 2,
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
	/*! The bitstream is not compliant to the specfication it refers to*/
	GF_NON_COMPLIANT_BITSTREAM				= -10,
	/*! No filter could be found to handle the desired media type*/
	GF_FILTER_NOT_FOUND						= -11,
	/*! The URL is not properly formatted or cannot be found*/
	GF_URL_ERROR							= -12,
	/*! An service error has occured at the local side*/
	GF_SERVICE_ERROR						= -13,
	/*! A service error has occured at the remote (server) side*/
	GF_REMOTE_SERVICE_ERROR					= -14,
	/*! The desired stream could not be found in the service*/
	GF_STREAM_NOT_FOUND						= -15,
    /*! The URL no longer exists*/
    GF_URL_REMOVED                          = -16,

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

	/*! UDP connection did not receive any data at all. Signaled by client services to reconfigure network if possible*/
	GF_IP_UDP_TIMEOUT						= -46,

	/*! Authentication with the remote host has failed*/
	GF_AUTHENTICATION_FAILURE				= -50,
	/*! Not ready for execution, later retry is needed */
	GF_NOT_READY						= -51,
	/*! Bad configuration for the current context */
	GF_INVALID_CONFIGURATION				= -52,
	/*! The element has not been found */
	GF_NOT_FOUND							= -53,
	/*! Unexpected format of data */
	GF_PROFILE_NOT_SUPPORTED				= -54,
	/*! filter PID config requires new instance of filter */
	GF_REQUIRES_NEW_INSTANCE = -56,
	/*! filter PID config cannot be supported by this filter, no use trying to find an alternate input filter chain*/
	GF_FILTER_NOT_SUPPORTED = -57
} GF_Err;

/*!
\brief Error Printing

Returns a printable version of a given error
\param e Error code requested
\return String representing the error
*/
const char *gf_error_to_string(GF_Err e);

/*! @} */


/*!
\addtogroup mem_grp
@{
 */

/*!
\brief Memory allocation for a structure
\hideinitializer

Macro allocating memory and zero-ing it
*/
#define GF_SAFEALLOC(__ptr, __struct) {\
		(__ptr) = (__struct *) gf_malloc(sizeof(__struct));\
		if (__ptr) {\
			memset((void *) (__ptr), 0, sizeof(__struct));\
		}\
	}

/*!
\brief Memory allocation for an array of n structs
\hideinitializer

Macro allocating memory for n structures and zero-ing it
*/
#define GF_SAFE_ALLOC_N(__ptr, __n, __struct) {\
		(__ptr) = (__struct *) gf_malloc( __n * sizeof(__struct));\
		if (__ptr) {\
			memset((void *) (__ptr), 0, __n * sizeof(__struct));\
		}\
	}


/*!
\brief dynamic string concatenation

Dynamic concatenation of string with optional separator
\param str pointer to destination string pointer
\param to_append string to append
\param sep optional separator string to insert before concatenation. If set and initial string is NULL, will not be appended
\return error code
 */
GF_Err gf_dynstrcat(char **str, const char *to_append, const char *sep);


/*!
\brief fraction parsing

Parse a 64 bit fraction from string
\param str string to parse
\param frac fraction to fill
\return GF_TRUE if success, GF_FALSE otherwise ( fraction being set to {0,0} )
 */
Bool gf_parse_lfrac(const char *str, GF_Fraction64 *frac);

/*!
\brief fraction parsing

Parse a 32 bit fraction from string
\param str string to parse
\param frac fraction to fill
\return GF_TRUE if success, GF_FALSE otherwise ( fraction being set to {0,0} )
 */
Bool gf_parse_frac(const char *str, GF_Fraction *frac);

/*!
\brief search string without case

Search a substring in a string without checking for case
\param text text to search
\param subtext string to find
\param subtext_len length of string to find
\return GF_TRUE if success, GF_FALSE otherwise
 */
Bool gf_strnistr(const char *text, const char *subtext, u32 subtext_len);

/*!
\brief search string in buffer

Search for a substring in a memory buffer of characters that may not be null-terminated
\param data buffer of chars in which to search
\param data_size size of data buffer
\param pat pattern to search for as a null-terminated string
\return a pointer to the first occurrence of pat in data, or null if not found (may not be null-terminated)
 */
const char* gf_strmemstr(const char *data, u32 data_size, const char *pat);

/*!
\brief safe timestamp rescale

Rescale a 64 bit timestamp value to new timescale, i.e. performs value * new_timescale / timescale
\param value value to rescale. A value of -1 means no timestamp defined and is returned unmodified
\param timescale timescale of value. Assumed to be less than 0xFFFFFFFF
\param new_timescale new timescale? Assumed to be less than 0xFFFFFFFF
\return new value
 */
u64 gf_timestamp_rescale(u64 value, u64 timescale, u64 new_timescale);

/*!
\brief safe signed timestamp rescale

Rescale a 64 bit timestamp value to new timescale, i.e. performs value * new_timescale / timescale
\param value value to rescale
\param timescale timescale of value. Assumed to be less than 0xFFFFFFFF
\param new_timescale new timescale. Assumed to be less than 0xFFFFFFFF
\return new value
 */
s64 gf_timestamp_rescale_signed(s64 value, u64 timescale, u64 new_timescale);

/*!
\brief compare timestamps

Compares two timestamps
\param value1 value to rescale
\param timescale1 timescale of value. Assumed to be less than 0xFFFFFFFF
\param value2 value to rescale
\param timescale2 timescale of value. Assumed to be less than 0xFFFFFFFF
\return GF_TRUE if (value1 / timescale1) is stricly less than (value2 / timescale2)
 */
Bool gf_timestamp_less(u64 value1, u64 timescale1, u64 value2, u64 timescale2);

/*!
\brief compare timestamps

Compares two timestamps
\param value1 value to rescale
\param timescale1 timescale of value. Assumed to be less than 0xFFFFFFFF
\param value2 value to rescale
\param timescale2 timescale of value. Assumed to be less than 0xFFFFFFFF
\return GF_TRUE if (value1 / timescale1) is stricly less than or equal to (value2 / timescale2)
 */
Bool gf_timestamp_less_or_equal(u64 value1, u64 timescale1, u64 value2, u64 timescale2);

/*!
\brief compare timestamps

Compares two timestamps
\param value1 value to rescale
\param timescale1 timescale of value. Assumed to be less than 0xFFFFFFFF
\param value2 value to rescale
\param timescale2 timescale of value. Assumed to be less than 0xFFFFFFFF
\return GF_TRUE if (value1 / timescale1) is stricly greater than (value2 / timescale2)
 */
Bool gf_timestamp_greater(u64 value1, u64 timescale1, u64 value2, u64 timescale2);

/*!
\brief compare timestamps

Compares two timestamps
\param value1 value to rescale
\param timescale1 timescale of value. Assumed to be less than 0xFFFFFFFF
\param value2 value to rescale
\param timescale2 timescale of value. Assumed to be less than 0xFFFFFFFF
\return GF_TRUE if (value1 / timescale1) is stricly greater than or equal to (value2 / timescale2)
 */
Bool gf_timestamp_greater_or_equal(u64 value1, u64 timescale1, u64 value2, u64 timescale2);

/*!
\brief compare timestamps

Compares two timestamps
\param value1 value to rescale
\param timescale1 timescale of value. Assumed to be less than 0xFFFFFFFF
\param value2 value to rescale
\param timescale2 timescale of value. Assumed to be less than 0xFFFFFFFF
\return GF_TRUE if (value1 / timescale1) is equal to (value2 / timescale2)
 */
Bool gf_timestamp_equal(u64 value1, u64 timescale1, u64 value2, u64 timescale2);

/*!
\brief strict convert str into integer

Validate and parse str into integer
\param str text to convert to integer
\param ans integer to fill
\return GF_TRUE if str represents an integer without any leading space nor extra chars
 */
Bool gf_strict_atoi(const char* str, int* ans);

/*! @} */

/*!
\addtogroup libsys_grp
\brief Library configuration

These functions are used to initialize, shutdown and configure libgpac.

The library shall be initialized using \ref gf_sys_init and terminated using gf_sys_close

The library can usually be configured from command line if your program uses \ref gf_sys_set_args.

The library can also be configured from your program using \ref gf_opts_set_key and related functions right after initializing the library.

For more information on configuration options, see \code gpac -hx core \endcode and https://wiki.gpac.io/core_options

For more information on filters configuration options, see https://wiki.gpac.io/Filters

@{
 */

/*!
Selection flags for memory tracker
\hideinitializer
 */
typedef enum
{
    /*! No memory tracking*/
    GF_MemTrackerNone = 0,
    /*! Memory tracking without backtrace*/
    GF_MemTrackerSimple,
    /*! Memory tracking with backtrace*/
    GF_MemTrackerBackTrace,
} GF_MemTrackerType;

/*!
\brief System setup

Inits system tools (GPAC global config, high-resolution clock if any, CPU usage manager, random number, ...).

You MUST call this function before calling any libgpac function, typically only once at startup.

The profile allows using a different global config file than the default, and may be a name (without / or \\) or point to an existing config file.
\note This can be called several times but only the first call will result in system setup.

\param mem_tracker_type memory tracking mode
\param profile name of the profile to load, NULL for default.
\return Error code if any
 */
GF_Err gf_sys_init(GF_MemTrackerType mem_tracker_type, const char *profile);

/*!
\brief System closing

Closes allocated system resources opened by \ref gf_sys_init.
\note This can be called several times but systems resources will be closed when no more users are accounted for.

When JavaScript APIs are loaded, the JS runtime is destroyed in this function call.
If your application needs to reload GPAC a second time after this, you MUST prevent JS runtime destruction otherwise the application will crash due to static JS class definitions refering to freed memory.
To prevent destruction, make sure you have called
 \code gf_opts_set_key("temp", "static-jsrt", "true"); \endcode

before calling \ref gf_sys_close. For example:

\code
gf_sys_init(GF_MemTrackerNone, NULL);
//run your code, eg a filter session

//prevent JSRT destruction
gf_opts_set_key("temp", "static-jsrt", "true");
gf_sys_close();

//The JSRT is still valid

gf_sys_init(GF_MemTrackerNone, NULL);
//run your code, eg another filter session

//do NOT prevent JSRT, parent app will exit after this call
gf_sys_close();

\endcode

 */
void gf_sys_close();


#if defined(GPAC_CONFIG_ANDROID)

/*!
\brief Android paths setup

Sets application data and external storage paths for android, to be called by JNI wrappers BEFORE \ref gf_sys_init is called.
If not set, default are:
- app_data is /data/data/io.gpac.gpac
- ext_storage is /sdcard

The default profile used on android is located in $ext_storage/GPAC/ if that directory exists, otherwise in $app_data/GPAC

\param app_data application data path, with no trailing path separator
\param ext_storage external storage location, with no trailing path separator
*/
void gf_sys_set_android_paths(const char *app_data, const char *ext_storage);

#endif // GPAC_CONFIG_ANDROID


/*!
\brief System arguments

Sets the user app arguments (used by GUI mode)
\param argc Number of arguments
\param argv Array of arguments - the first string is ignored (considered to be the executable name)
\return error code if any, GF_OK otherwise
 */
GF_Err gf_sys_set_args(s32 argc, const char **argv);

/*!
\brief Get number of args

Gets the number of argument of the user application if any
\return number of argument of the user application
 */
u32 gf_sys_get_argc();

/*!
\brief Get program arguments

Gets the arguments of the user application if any
\return  argument of the user application
 */
const char **gf_sys_get_argv();

/*!
\brief Get number of args

Gets the number of argument of the user application if any
\param arg Index of argument to retrieve
\return number of argument of the user application
 */
const char *gf_sys_get_arg(u32 arg);

/*!
\brief Locate a global filter arg

Looks for a filter option specified as global argument
\param arg name of option to search, without "--" or "-+" specififers
\return argument value string, empty string for booleans or NULL if not found
 */
const char *gf_sys_find_global_arg(const char *arg);

/*!
\brief Mark arg as used

Marks the argument at given index as used. By default all args are marked as not used when assigning args
\param arg_idx Index of argument to mark
\param used flag to set
*/
void gf_sys_mark_arg_used(s32 arg_idx, Bool used);

/*!
\brief Check if arg is marked as used

Marks the argument at given index as used
\param arg_idx Index of argument to mark
\return used flag of the arg
*/
Bool gf_sys_is_arg_used(s32 arg_idx);

/*!
\brief checks if test mode is enabled

Checks if test mode is enabled (no date nor GPAC version should be written).
\return GF_TRUE if test mode is enabled, GF_FALSE otherwise.
 */
Bool gf_sys_is_test_mode();

/*!
\brief checks if compatibility with old arch is enabled

Checks if compatibility with old arch is enabled - this function will be removed when master will be moved to filters branch
\return GF_TRUE if old arch compat is enabled, GF_FALSE otherwise.
 */
Bool gf_sys_old_arch_compat();

#ifdef GPAC_ENABLE_COVERAGE
/*!
\brief checks if coverage tests are enabled

Checks if coverage tests are enabled
\return GF_TRUE if coverage is enabled, GF_FALSE otherwise.
 */
Bool gf_sys_is_cov_mode();
#endif

/*!
\brief checks if running in quiet mode

Checks if quiet mode is enabled
\return 2 if quiet mode is enabled, 1 if quiet mode not enabled but progress is disabled, 0 otherwise.
 */
u32 gf_sys_is_quiet();

/*! gets GPAC feature list in this GPAC build
\param disabled if GF_TRUE, gets disabled features, otherwise gets enabled features
\return the list of features.
*/
const char *gf_sys_features(Bool disabled);

/*! callback function for remotery profiler
 \param udta user data passed by \ref gf_sys_profiler_set_callback
 \param text string sent by webbrowser client
*/
typedef void (*gf_rmt_user_callback)(void *udta, const char* text);

/*! Enables remotery profiler callback. If remotery is enabled, commands sent via webbrowser client will be forwarded to the callback function specified
\param udta user data
\param rmt_usr_cbk callback function
\return GF_OK if success, GF_BAD_PARAM if profiler is not running, GF_NOT_SUPPORTED if profiler not supported
*/
GF_Err gf_sys_profiler_set_callback(void *udta, gf_rmt_user_callback rmt_usr_cbk);


/*! Sends a log message to remotery web client
\param msg text message to send. The message format should be json
\return GF_OK if success, GF_BAD_PARAM if profiler is not running, GF_NOT_SUPPORTED if profiler not supported
*/
GF_Err gf_sys_profiler_log(const char *msg);

/*! Sends a message to remotery web client
\param msg text message to send. The message format should be json
\return GF_OK if success, GF_BAD_PARAM if profiler is not running, GF_NOT_SUPPORTED if profiler not supported
*/
GF_Err gf_sys_profiler_send(const char *msg);

/*! Enables sampling times in RMT
 \param enable if GF_TRUE, sampling will be enabled, otherwise disabled*/
void gf_sys_profiler_enable_sampling(Bool enable);

/*! Checks if sampling is enabled in RMT. Sampling is by default enabled when enabling remotery
 \return GF_TRUE if sampling is enabled, GF_FALSE otherwise*/
Bool gf_sys_profiler_sampling_enabled();

/*!
GPAC Log tools
\hideinitializer

Describes the color code for console print
 */
typedef enum
{
	/*!reset color*/
	GF_CONSOLE_RESET=0,
	/*!set text to red*/
	GF_CONSOLE_RED,
	/*!set text to green*/
	GF_CONSOLE_GREEN,
	/*!set text to blue*/
	GF_CONSOLE_BLUE,
	/*!set text to yellow*/
	GF_CONSOLE_YELLOW,
	/*!set text to cyan*/
	GF_CONSOLE_CYAN,
	/*!set text to white*/
	GF_CONSOLE_WHITE,
	/*!set text to magenta*/
	GF_CONSOLE_MAGENTA,
	/*!reset all console text*/
	GF_CONSOLE_CLEAR,
	/*!save console state*/
	GF_CONSOLE_SAVE,
	/*!restore console state*/
	GF_CONSOLE_RESTORE,

	/*!set text to bold modifier*/
	GF_CONSOLE_BOLD = 1<<16,
	/*!set text to italic*/
	GF_CONSOLE_ITALIC = 1<<17,
	/*!set text to underlined*/
	GF_CONSOLE_UNDERLINED = 1<<18,
	/*!set text to strikethrough*/
	GF_CONSOLE_STRIKE = 1<<19
} GF_ConsoleCodes;

/*! sets console code
\param std the output stream (stderr or stdout)
\param code the console code to set
*/
void gf_sys_set_console_code(FILE *std, GF_ConsoleCodes code);


/*! @} */


/*!
\addtogroup log_grp
\brief Logging System

@{
*/

/*!
\brief GPAC Log Levels
\hideinitializer

These levels describes messages priority used when filtering logs
*/
typedef enum
{
	/*! Disable all Log message*/
	GF_LOG_QUIET = 0,
	/*! Log message describes an error*/
	GF_LOG_ERROR,
	/*! Log message describes a warning*/
	GF_LOG_WARNING,
	/*! Log message is informational (state, etc..)*/
	GF_LOG_INFO,
	/*! Log message is a debug info*/
	GF_LOG_DEBUG
} GF_LOG_Level;

/*!
\brief Log exits at first error assignment

When GF_LOG_ERROR happens, program leaves with instruction exit(1);
\param strict strict behavior when encoutering a serious error.
\return old value before the call.
 */
Bool gf_log_set_strict_error(Bool strict);

/*!
\brief gets string-formatted log tools

Gets the string-formatted log tools and levels. Returned string shall be freed by the caller.
\return string-formatted log tools.
 */
char *gf_log_get_tools_levels(void);

/*!
\brief GPAC Log tools
\hideinitializer

These flags describes which sub-part of GPAC generates the log and are used when filtering logs
 */
typedef enum
{
	/*! Log message from the core library (init, threads, network calls, etc)*/
	GF_LOG_CORE = 0,
	/*! Log message from a raw media parser (BIFS, LASeR, A/V formats)*/
	GF_LOG_CODING,
	/*! Log message from a bitstream parser (IsoMedia, MPEG-2 TS, OGG, ...)*/
	GF_LOG_CONTAINER,
	/*! Log message from the network/service stack (messages & co)*/
	GF_LOG_NETWORK,
	/*! Log message from the HTTP stack*/
	GF_LOG_HTTP,
	/*! Log message from the RTP/RTCP stack (TS info) and packet structure & hinting (debug)*/
	GF_LOG_RTP,
	/*! Log message from a codec*/
	GF_LOG_CODEC,
	/*! Log message from any textual (XML, ...) parser (context loading, etc)*/
	GF_LOG_PARSER,
	/*! Generic log message from a filter (not from filter core library)*/
	GF_LOG_MEDIA,
	/*! Log message from the scene graph/scene manager (handling of nodes and attribute modif, DOM core)*/
	GF_LOG_SCENE,
	/*! Log message from the scripting engine APIs - does not cover alert() in the script code itself*/
	GF_LOG_SCRIPT,
	/*! Log message from event handling*/
	GF_LOG_INTERACT,
	/*! Log message from compositor*/
	GF_LOG_COMPOSE,
	/*! Log message from the compositor, indicating media object state*/
	GF_LOG_COMPTIME,
	/*! Log for video object cache */
	GF_LOG_CACHE,
	/*! Log message from multimedia I/O devices (audio/video input/output, ...)*/
	GF_LOG_MMIO,
	/*! Log for runtime info (times, memory, CPU usage)*/
	GF_LOG_RTI,
	/*! Log for memory tracker*/
	GF_LOG_MEMORY,
	/*! Log for audio compositor*/
	GF_LOG_AUDIO,
	/*! Generic Log for modules*/
	GF_LOG_MODULE,
	/*! Log for threads and mutexes */
	GF_LOG_MUTEX,
	/*! Log for threads and condition */
	GF_LOG_CONDITION,
	/*! Log for all HTTP streaming */
	GF_LOG_DASH,
	/*! Log for all messages from filter core library (not from a filter) */
	GF_LOG_FILTER,
	/*! Log for filter scheduler only */
	GF_LOG_SCHEDULER,
	/*! Log for all ROUTE message */
	GF_LOG_ROUTE,
	/*! Log for all messages coming from script*/
	GF_LOG_CONSOLE,
	/*! Log for all messages coming the application, not used by libgpac or the modules*/
	GF_LOG_APP,

	/*! special value used to set a level for all tools*/
	GF_LOG_ALL,
	GF_LOG_TOOL_MAX = GF_LOG_ALL,
} GF_LOG_Tool;

/*!
\brief Log modules assignment

Sets the tools to be checked for log filtering. By default no logging is performed.
\param log_tool the tool to be logged
\param log_level the level of logging for this tool
 *
 */
void gf_log_set_tool_level(GF_LOG_Tool log_tool, GF_LOG_Level log_level);

/*!
\brief Log Message Callback

The gf_log_cbk type is the type for the callback of the \ref gf_log_set_callback function. By default all logs are redirected to stderr
\param cbck Opaque user data.
\param log_level level of the log. This value is not guaranteed in multi-threaded context.
\param log_tool tool emitting the log. This value is not guaranteed in multi-threaded context.
\param fmt message log format.
\param vlist message log param.
 *
 */
typedef void (*gf_log_cbk)(void *cbck, GF_LOG_Level log_level, GF_LOG_Tool log_tool, const char* fmt, va_list vlist);

/*!
\brief Log overwrite

Assigns a user-defined callback for printing log messages. By default all logs are redirected to stderr
\param usr_cbk Opaque user data
\param cbk     Callback log function
\return previous callback function
*/
gf_log_cbk gf_log_set_callback(void *usr_cbk, gf_log_cbk cbk);


/*!
 \cond DUMMY_DOXY_SECTION
*/
#ifndef GPAC_DISABLE_LOG
/*\note
	- to turn log on, change to GPAC_ENABLE_LOG
	- to turn log off, change to GPAC_DISABLE_LOG
	this is needed by configure+sed to modify this file directly
*/
#define GPAC_ENABLE_LOG
#endif

/*!
 \endcond
*/


/*! \cond PRIVATE */
void gf_log(const char *fmt, ...);
void gf_log_lt(GF_LOG_Level ll, GF_LOG_Tool lt);
void gf_log_va_list(GF_LOG_Level level, GF_LOG_Tool tool, const char *fmt, va_list vl);
/*! \endcond */

/*!
\brief Log level checking

Checks if a given tool is logged for the given level
\param log_tool tool to check
\param log_level level to check
\return 1 if logged, 0 otherwise
*/
Bool gf_log_tool_level_on(GF_LOG_Tool log_tool, GF_LOG_Level log_level);

/*!
\brief Log tool name

Gets log  tool name
\param log_tool tool to check
\return name, or "unknwon" if not known
*/
const char *gf_log_tool_name(GF_LOG_Tool log_tool);

/*!
\brief Log level getter

Gets log level of a given tool
\param log_tool tool to check
\return log level of tool
*/
u32 gf_log_get_tool_level(GF_LOG_Tool log_tool);

/*!
\brief Set log tools and levels

Set log tools and levels according to the log_tools_levels string.
\param log_tools_levels string specifying the tools and levels. It is formatted as logToolX\@logLevelX:logToolZ\@logLevelZ:...
\param reset_all if GF_TRUE, all previous log settings are discarded.
\return GF_OK or GF_BAD_PARAM
*/
GF_Err gf_log_set_tools_levels(const char *log_tools_levels, Bool reset_all);

/*!
\brief Modify log tools and levels

Modify log tools and levels according to the log_tools_levels string. Previous log settings are kept.
\param val string specifying the tools and levels. It is formatted as logToolX\@logLevelX:logToolZ\@logLevelZ:...
\return GF_OK or GF_BAD_PARAM
*/
GF_Err gf_log_modify_tools_levels(const char *val);

/*!
\brief Checks if color logs is enabled

Checks if color logs is enabled
\return GF_TRUE if color logs are used
*/
Bool gf_log_use_color();

/*!
\brief Checks if logs are stored to file

Checks if logs are stored to file
\return GF_TRUE if logs are stored to file
*/
Bool gf_log_use_file();

#ifdef GPAC_DISABLE_LOG
void gf_log_check_error(u32 ll, u32 lt);
#define GF_LOG(_ll, _lm, __args) gf_log_check_error(_ll, _lm);
#else
/*!
\brief Message logging
\hideinitializer

Macro for logging messages. Usage is GF_LOG(log_lev, log_module, (fmt, ...)). The log function is only called if log filtering allows it. This avoids fetching logged parameters when the tool is not being logged.
*/
#define GF_LOG(_log_level, _log_tools, __args) if (gf_log_tool_level_on(_log_tools, _log_level) ) { gf_log_lt(_log_level, _log_tools); gf_log __args ;}
#endif

/*!
\brief Resets log file
Resets log file if any log file name was specified, by closing and reopening a new file.
*/
void gf_log_reset_file();



/*!	@} */

/*!
\addtogroup miscsys_grp
\brief System time CPU

This section documents time functionalities and CPU management in GPAC.

@{
 */


/*!
\brief PseudoRandom Integer Generation Initialization

Sets the starting point for generating a series of pseudorandom integers.
\param Reset Re-initializes the random number generator
*/
void gf_rand_init(Bool Reset);
/*! PseudoRandom integer generation
\return a pseudorandom integer
*/
u32 gf_rand();

/*! gets user name
\param buf buffer set to current user (login) name if available.
*/
void gf_get_user_name(char buf[1024]);

/*!
\brief Progress formatting

Signals progress in GPAC's operations. Note that progress signaling with this function is not thread-safe, the main purpose is to use it for authoring tools only.
\param title title string of the progress, or NULL for no progress
\param done Current amount performed of the action.
\param total Total amount of the action.
 */
void gf_set_progress(const char *title, u64 done, u64 total);

/*!
\brief Progress Callback

The gf_on_progress_cbk type is the type for the callback of the \ref gf_set_progress_callback function
\param cbck Opaque user data.
\param title preogress title.
\param done Current amount performed of the action
\param total Total amount of the action.
 *
 */
typedef void (*gf_on_progress_cbk)(const void *cbck, const char *title, u64 done, u64 total);

/*!
\brief Progress overwriting
Overwrites the progress signaling function by a user-defined one.
\param user_cbk Opaque user data
\param prog_cbk new callback function to use. Passing NULL restore default GPAC stderr notification.
 */
void gf_set_progress_callback(void *user_cbk, gf_on_progress_cbk prog_cbk);


/*!
\brief Prompt checking

Checks if a character is pending in the prompt buffer.
\return 1 if a character is ready to be fetched, 0 otherwise.
\note Function not available under WindowsCE nor SymbianOS
*/
Bool gf_prompt_has_input();

/*!
\brief Prompt character flush

Gets the current character entered at prompt if any.
\return value of the character.
\note Function not available under WindowsCE nor SymbianOS
*/
char gf_prompt_get_char();

/*!
\brief Get prompt TTY size

Gets the stdin prompt size (columns and rows)
\param width set to number of rows in the TTY
\param height set to number of columns in the TTY
\return error if any
*/
GF_Err gf_prompt_get_size(u32 *width, u32 *height);

/*!
\brief turns prompt echo on/off

Turns the prompt character echo on/off - this is useful when entering passwords.
\param echo_off indicates whether echo should be turned on or off.
\note Function not available under WindowsCE nor SymbianOS
*/
void gf_prompt_set_echo_off(Bool echo_off);

/*! @} */


/*! gets battery state
\param onBattery set to GF_TRUE if running on battery
\param onCharge set to GF_TRUE if battery is charging
\param level set to battery charge percent
\param batteryLifeTime set to battery lifetime
\param batteryFullLifeTime set to battery full lifetime
\return GF_TRUE if success
*/
Bool gf_sys_get_battery_state(Bool *onBattery, u32 *onCharge, u32 *level, u32 *batteryLifeTime, u32 *batteryFullLifeTime);


/*!
\brief parses 128 bit from string

Parses 128 bit from string
\param string the string containing the value in hexa. Non alphanum characters are skipped
\param value the value parsed
\return error code if any
 */
GF_Err gf_bin128_parse(const char *string, bin128 value);


enum
{
    GF_BLOB_IN_TRANSFER = 1,
    GF_BLOB_CORRUPTED = 1<<1,
};

/*!
 * Blob structure used to pass data pointer around
 */
typedef struct
{
	/*! data block of blob */
	u8 *data;
	/*! size of blob */
	u32 size;
    /*! blob flags */
    u32 flags;
#ifdef GPAC_DISABLE_THREADS
    void *mx;
#else
    /*! blob mutex for multi-thread access */
    struct __tag_mutex *mx;
#endif
} GF_Blob;

/*!
 * Retrieves data associated with a blob url. If success, \ref gf_blob_release must be called after this
\param blob_url URL of blob object (ie gmem://%p)
\param out_data if success, set to blob data pointer
\param out_size if success, set to blob data size
\param blob_flags if success, set to blob flags - may be NULL
\return error code
 */
GF_Err gf_blob_get(const char *blob_url, u8 **out_data, u32 *out_size, u32 *blob_flags);

/*!
 * Releases blob data
\param blob_url URL of blob object (ie gmem://%p)
\return error code
 */
GF_Err gf_blob_release(const char *blob_url);

/*!
 * Registers a new blob
\param blob  blob object
\return URL of blob object (ie gmem://%p), must be freed by user
 */
char *gf_blob_register(GF_Blob *blob);

/*!
 * Unegisters a blob. This must be called before destroying a registered blob
\param blob  blob object
 */
void gf_blob_unregister(GF_Blob *blob);

/*!
\brief Portable getch()

Returns immediately a typed char from stdin
\return the typed char
*/
int gf_getch();

/*!
\brief Reads a line of input from stdin
\param line the buffer to fill
\param maxSize the maximum size of the line to read
\param showContent boolean indicating if the line read should be printed on stderr or not
\return GF_TRUE if some content was read, GF_FALSE otherwise
*/
Bool gf_read_line_input(char * line, int maxSize, Bool showContent);


/*!
\addtogroup time_grp
\brief Time manipulation tools
@{
*/

/*!
\brief System clock query

Gets the system clock time.
\return System clock value since GPAC initialization in milliseconds.
 */
u32 gf_sys_clock();

/*!
\brief High precision system clock query

Gets the hight precision system clock time.
\return System clock value since GPAC initialization in microseconds.
 */
u64 gf_sys_clock_high_res();

/*!
\brief Sleeps thread/process

Locks calling thread/process execution for a given time.
\param ms Amount of time to sleep in milliseconds.
 */
void gf_sleep(u32 ms);

#ifdef WIN32
/*!
\brief WINCE time constant
\hideinitializer

time between jan 1, 1601 and jan 1, 1970 in units of 100 nanoseconds
*/
#define TIMESPEC_TO_FILETIME_OFFSET (((LONGLONG)27111902 << 32) + (LONGLONG)3577643008)

#endif

/*!
\brief gets UTC time in milliseconds

Gets UTC clock in milliseconds
\return UTC time in milliseconds
 */
u64 gf_net_get_utc();

/*!
\brief converts an ntp timestamp into UTC time in milliseconds

Converts NTP 64-bit timestamp to UTC clock in milliseconds
\param ntp NTP timestamp
\return UTC time in milliseconds
 */
u64 gf_net_ntp_to_utc(u64 ntp);

/*!
\brief parses date

Parses date and gets UTC value for this date. Date format is an XSD dateTime format or any of the supported formats from HTTP 1.1:
	Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
	Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
	Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() formatgets UTC time in milliseconds

\param date string containing the date to parse
\return UTC time in milliseconds
 */
u64 gf_net_parse_date(const char *date);

/*!
\brief returns 64-bit UTC timestamp from year, month, day, hour, min and sec
\param year the year
\param month the month, from 0 to 11
\param day the day
\param hour the hour
\param min the min
\param sec the sec
\return UTC time in milliseconds
 */
u64 gf_net_get_utc_ts(u32 year, u32 month, u32 day, u32 hour, u32 min, u32 sec);

/*!
\brief gets timezone adjustment in seconds

Gets timezone adjustment in seconds, with localtime - timezone = UTC time
\return timezone shift in seconds
 */
s32 gf_net_get_timezone();

/*!
\brief gets timezone daylight saving time status

Gets timezone daylight saving time
\return GF_TRUE if DST is active
 */
Bool gf_net_time_is_dst();

/*!
\brief gets time from UTC timestamp

Gets time from UTC timestamp
\param time timestamp value - see gmtime
\return time description structure - see gmtime
 */
struct tm *gf_gmtime(const time_t *time);

/*!	@} */

/*!
\addtogroup thr_grp
\brief Time manipulation tools
@{
*/

/*!
\brief Gets process ID

Gets ID of the process running this gpac instance.
\return the ID of the main process
*/
u32 gf_sys_get_process_id();

/*!\brief run-time system info object

The Run-Time Info object is used to get CPU and memory occupation of the calling process.
All time values are expressed in milliseconds (accuracy is not guaranteed).
*/
typedef struct
{
	/*!start of the sampling period*/
	u32 sampling_instant;
	/*!duration of the sampling period*/
	u32 sampling_period_duration;
	/*!total amount of time (User+kernel) spent in CPU for all processes as evaluated at the end of the sampling period*/
	u32 total_cpu_time;
	/*!total amount of time (User+kernel) spent in CPU for the calling process as evaluated at the end of the sampling period*/
	u32 process_cpu_time;
	/*!amount of time (User+kernel) spent in CPU for all processes during the sampling period*/
	u32 total_cpu_time_diff;
	/*!total amount of time (User+kernel) spent in CPU for the calling process during the sampling period*/
	u32 process_cpu_time_diff;
	/*!total amount of idle time during the sampling period.*/
	u32 cpu_idle_time;
	/*!percentage (from 0 to 100) of CPU usage during the sampling period.*/
	u32 total_cpu_usage;
	/*!percentage (from 0 to 100) of the CPU usage by the calling process during the sampling period.*/
	u32 process_cpu_usage;
	/*!calling process ID*/
	u32 pid;
	/*!calling process thread count if known*/
	u32 thread_count;
	/*!size of calling process allocated heaps*/
	u64 process_memory;
	/*!total physical memory in system*/
	u64 physical_memory;
	/*!available physical memory in system*/
	u64 physical_memory_avail;
	/*!total memory currently allocated by gpac*/
	u64 gpac_memory;
	/*!total number of cores on the system*/
	u32 nb_cores;
} GF_SystemRTInfo;

/*!
Selection flags for run-time info retrieval
\hideinitializer
 */
enum
{
	/*!Indicates all processes' times must be fetched. If not set, only the current process times will be retrieved, and the
	thread count and total times won't be available*/
	GF_RTI_ALL_PROCESSES_TIMES = 1,
	/*!Indicates the process allocated heap size must be fetch. If not set, only the system physical memory is fetched.
	Fetching the entire ocess  allocated memory can have a large impact on performances*/
	GF_RTI_PROCESS_MEMORY = 1<<1,
	/*!Indicates that only system memory should be fetched. When set, all refreshing info is ignored*/
	GF_RTI_SYSTEM_MEMORY_ONLY = 1<<2
};

/*!
\brief Gets Run-Time info

Gets CPU and memory usage info for the calling process and the system. Information gathering is controled through timeout values.
\param refresh_time_ms refresh time period in milliseconds. If the last sampling was done less than this period ago, the run-time info is not refreshed.
\param rti holder to the run-time info structure to update.
\param flags specify which info is to be retrieved.
\return 1 if info has been updated, 0 otherwise.
\note You should not try to use a too small refresh time. Typical values are 500 ms or one second.
 */
Bool gf_sys_get_rti(u32 refresh_time_ms, GF_SystemRTInfo *rti, u32 flags);

/*!	@} */

/*!
\addtogroup osfile_grp
\brief File System tools

This section documents file system tools used in GPAC.

FILE objects are wrapped in GPAC for direct memory or callback operations. All file functions not using stderr/stdout must use the gf_ prefixed versions, eg:
\code
//bad design, will fail when using wrapped memory IOs
FILE *t = fopen(url, "rb");
fputs(t, mystring);
fclose(t);

//good design, will work fine when using wrapped memory IOs
FILE *t = gf_fopen(url, "rb");
gf_fputs(t, mystring);
gf_fclose(t);
\endcode

@{
 */

/*!
\brief reads a file into memory

Reads a local file into memory, in binary open mode.
\param file_name path on disk of the file to read
\param out_data pointer to allocted address, to be freed by caller
\param out_size pointer to allocted size
\return error code if any
 */
GF_Err gf_file_load_data(const char *file_name, u8 **out_data, u32 *out_size);

/*!
\brief reads a file into memory

Reads a local file into memory, in binary open mode.
\param file stream object to read (no seek is performed)
\param out_data pointer to allocted address, to be freed by caller
\param out_size pointer to allocted size
\return error code if any
 */
GF_Err gf_file_load_data_filep(FILE *file, u8 **out_data, u32 *out_size);

/*!
\brief Delete Directory

Delete a  dir within the full path.
\param DirPathName the file path name.
\return error if any
 */
GF_Err gf_rmdir(const char *DirPathName);

/*!
\brief Create Directory

Create a directory within the full path.
\param DirPathName the dir path name.
\return error if any
 */
GF_Err gf_mkdir(const char* DirPathName);

/*!
\brief Check Directory Exists

Create a directory within the full path.
\param DirPathName the dir path name.
\return GF_TRUE if directory exists
 */
Bool gf_dir_exists(const char *DirPathName);

/*!
\brief Create Directory

Cleanup a directory within the full path, removing all the files and the directories.
\param DirPathName the dir path name.
\return error if any
 */
GF_Err gf_dir_cleanup(const char* DirPathName);


/**
Gets a newly allocated string containing the default cache directory.
It is the responsibility of the caller to free the string.
\return a fully qualified path to the default cache directory
 */
const char * gf_get_default_cache_directory();

/**
Gets the number of open file handles (gf_fopen/gf_fclose only).
\return  number of open file handles
 */
u32 gf_file_handles_count();

/*!
\brief file writing helper

Wrapper to properly handle calls to fwrite(), ensuring proper error handling is invoked when it fails.
\param ptr data buffer to write
\param nb_bytes number of bytes to write
\param stream stream object
\return number of bytes to written
*/
size_t gf_fwrite(const void *ptr, size_t nb_bytes, FILE *stream);

/*!
\brief file reading helper

Wrapper to properly handle calls to fread()
\param ptr data buffer to read
\param nbytes number of bytes to read
\param stream stream object
\return number of bytes read
*/
size_t gf_fread(void *ptr, size_t nbytes, FILE *stream);

/*!
\brief file reading helper

Wrapper to properly handle calls to fgets()
\param buf same as fgets
\param size same as fgets
\param stream same as fgets
\return same as fgets
*/
char *gf_fgets(char *buf, size_t size, FILE *stream);
/*!
\brief file reading helper

Wrapper to properly handle calls to fgetc()
\param stream same as fgetc
\return same as fgetc
*/
int gf_fgetc(FILE *stream);
/*!
\brief file writing helper

Wrapper to properly handle calls to fputc()
\param val same as fputc
\param stream same as fputc
\return same as fputc
*/
int gf_fputc(int val, FILE *stream);
/*!
\brief file writing helper

Wrapper to properly handle calls to fputs()
\param buf same as fputs
\param stream same as fputs
\return same as fputs
*/
int	gf_fputs(const char *buf, FILE *stream);
/*!
\brief file writing helper

Wrapper to properly handle calls to fprintf()
\param stream same as fprintf
\param format same as fprintf
\return same as fprintf
*/
int gf_fprintf(FILE *stream, const char *format, ...);

/*!
\brief file writing helper

Wrapper to properly handle calls to vfprintf()
\param stream same as vfprintf
\param format same as vfprintf
\param args same as vfprintf
\return same as fprintf
*/
int gf_vfprintf(FILE *stream, const char *format, va_list args);

/*!
\brief file flush helper

Wrapper to properly handle calls to fflush()
\param stream same as fflush
\return same as fflush
*/
int gf_fflush(FILE *stream);
/*!
\brief end of file helper

Wrapper to properly handle calls to feof()
\param stream same as feof
\return same as feof
*/
int gf_feof(FILE *stream);
/*!
\brief file error helper

Wrapper to properly handle calls to ferror()
\param stream same as ferror
\return same as ferror
*/
int gf_ferror(FILE *stream);

/*!
\brief file size helper

Gets the file size given a FILE object. The FILE object position will be reset to 0 after this call
\param fp FILE object to check
\return file size in bytes
*/
u64 gf_fsize(FILE *fp);

/*!
\brief file IO checker

Checks if the given FILE object is a native FILE or a GF_FileIO wrapper.
\param fp FILE object to check
\return GF_TRUE if the FILE object is a wrapper to GF_FileIO
*/
Bool gf_fileio_check(FILE *fp);

/*! FileIO write state*/
typedef enum
{
	/*! FileIO object is ready for write operations*/
	GF_FIO_WRITE_READY=0,
	/*! FileIO object is not yet ready for write operations*/
	GF_FIO_WRITE_WAIT,
	/*! FileIO object has been canceled*/
	GF_FIO_WRITE_CANCELED,
} GF_FileIOWriteState;
/*!
\brief file IO write checker

Checks if the given FILE object is ready for write.
\param fp FILE object to check
\return write state of GF_FileIO object, GF_FIO_WRITE_READY if not a GF_FileIO object
*/
GF_FileIOWriteState gf_fileio_write_ready(FILE *fp);

/*!
\brief file opening

Opens a file, potentially bigger than 4GB. if filename identifies a blob (gmem://), the blob will be opened
\param file_name same as fopen
\param mode same as fopen
\return stream handle of the file object
*/
FILE *gf_fopen(const char *file_name, const char *mode);

/*!
\brief file opening

Opens a file, potentially using file IO if the parent URL is a File IO wrapper
\param file_name same as fopen
\param parent_url URL of parent file. If not a file io wrapper (gfio://), the function is equivalent to gf_fopen
\param mode same as fopen - value "mkdir" checks if parent dir(s) need to be created, create them if needed and returns NULL (no file open)
\param no_warn if GF_TRUE, do not throw log message if failure
\return stream handle of the file object
\note You only need to call this function if you're suspecting the file to be a large one (usually only media files), otherwise use regular stdio.
\return stream habdle of the file or file IO object*/
FILE *gf_fopen_ex(const char *file_name, const char *parent_url, const char *mode, Bool no_warn);

/*!
\brief file closing

Closes a file
\note You only need to call this function if you're suspecting the file to be a large one (usually only media files), otherwise use regular stdio.
\param file file to close
\return same as flcose
*/
s32 gf_fclose(FILE *file);

/*!
\brief large file position query

Queries the current read/write position in a large file
\param f Same semantics as gf_ftell
\return position in the file
\note You only need to call this function if you're suspecting the file to be a large one (usually only media files), otherwise use regular stdio.
*/
u64 gf_ftell(FILE *f);
/*!
\brief large file seeking

Seeks the current read/write position in a large file
\param f Same semantics as fseek
\param pos Same semantics as fseek
\param whence Same semantics as fseek
\return 0 if success, -1 if error
\note You only need to call this function if you're suspecting the file to be a large one (usually only media files), otherwise use regular stdio.
*/
s32 gf_fseek(FILE *f, s64 pos, s32 whence);


/*! gets basename from filename/path
\param filename Path of the file, can be an absolute path
\return a pointer to the start of a filepath basename or null
*/
char* gf_file_basename(const char* filename);

/*! gets extension from filename
\param filename Path of the file, can be an absolute path
\return a pointer to the start of a filepath extension or null
*/
char* gf_file_ext_start(const char* filename);

/*!\brief FileEnum info object

The FileEnumInfo object is used to get file attributes upon enumeration of a directory.
*/
typedef struct
{
	/*!File is marked as hidden*/
	Bool hidden;
	/*!File is a directory*/
	Bool directory;
	/*!File is a drive mountpoint*/
	Bool drive;
	/*!File is a system file*/
	Bool system;
	/*!File size in bytes*/
	u64 size;
	/*!File last modif time in UTC seconds*/
	u64 last_modified;
} GF_FileEnumInfo;

/*!
\brief Directory Enumeration Callback

The gf_enum_dir_item type is the type for the callback of the \ref gf_enum_directory function
\param cbck Opaque user data.
\param item_name File or directory name.
\param item_path File or directory full path and name from filesystem root.
\param file_info information for the file or directory.
\return 1 to abort enumeration, 0 to continue enumeration.
 *
 */
typedef Bool (*gf_enum_dir_item)(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info);
/*!
\brief Directory enumeration

Enumerates a directory content. Feedback is provided by the enum_dir_item function
\param dir Directory to enumerate
\param enum_directory If set, only directories will be enumerated, otherwise only files are.
\param enum_dir gf_enum_dir_item callback function for enumeration.
\param cbck Opaque user data passed to callback function.
\param filter optional filter for file extensions. If a file extension without the dot '.' character is not found in the
 *	filter the file will be skipped.
\return error if any
 */
GF_Err gf_enum_directory(const char *dir, Bool enum_directory, gf_enum_dir_item enum_dir, void *cbck, const char *filter);


/*!
\brief File Deletion

Deletes a file from the disk.
\param fileName absolute name of the file or name relative to the current working directory.
\return error if any
*/
GF_Err gf_file_delete(const char *fileName);

/*!
\brief File Move

Moves or renames a file or directory.
\param fileName absolute path of the file / directory to move or rename
\param newFileName absolute new path/name of the file / directory
\return error if any
*/
GF_Err gf_file_move(const char *fileName, const char *newFileName);

/*!
\brief Temporary File Creation

Creates a new temporary file in binary mode
\param fileName if not NULL, strdup() of the temporary filename when created by GPAC (NULL otherwise as the system automatically removes its own tmp files)
\return stream handle to the new file ressoucre
 */
FILE *gf_file_temp(char ** const fileName);


/*!
\brief File Modification Time

Gets the UTC modification time of the given file in microseconds
\param filename file to check
\return modification time of the file
 */
u64 gf_file_modification_time(const char *filename);

/*!
\brief File existence check

Checks if file with given name exists
\param fileName path of the file to check
\return GF_TRUE if file exists */
Bool gf_file_exists(const char *fileName);

/*!
\brief File existence check

Checks if file with given name exists, for regular files or File IO wrapper
\param file_name path of the file to check
\param par_name  name of the parent file
\return GF_TRUE if file exists */
Bool gf_file_exists_ex(const char *file_name, const char *par_name);

/*! File IO wrapper object*/
typedef struct __gf_file_io GF_FileIO;


/*! open proc for memory file IO
\param fileio_ref reference file_io. A file can be opened multiple times for the same reference, your code must handle this
\param url target file name.
\param mode opening mode of file, same as fopen mode. The following additional modes are defined:
	- "ref": indicates this FileIO object is used by some part of the code and must not be destroyed upon closing of the file. Associated URL is null
	- "unref": indicates this FileIO object is not used by some part of the code and may be destroyed if no more references to this object are set. Associated URL is null
	- "url": indicates to create a new FileIO object for the given URL without opening the output file. The resulting FileIO object must be garbage collected by the app in case its is never used by the callers
	- "probe": checks if the file exists, but no need to open the file. The function should return NULL in this case. If file does not exist, set out_error to GF_URL_ERROR
	- "close": indicates the fileIO object is being closed (fclose)
\param out_error must be set to error code if any (never NULL)
\return the opened GF_FileIO if success, or NULL otherwise
 */
typedef GF_FileIO *(*gfio_open_proc)(GF_FileIO *fileio_ref, const char *url, const char *mode, GF_Err *out_error);

/*! seek proc for memory file IO
\param fileio target file IO object
\param offset offset in file
\param whence position from offset, same as fseek
\return error if any
 */
typedef GF_Err (*gfio_seek_proc)(GF_FileIO *fileio, u64 offset, s32 whence);

/*! read proc for memory file IO
\param fileio target file IO object
\param buffer buffer to read.
\param bytes number of bytes to read.
\return number of bytes read, 0 if error.
 */
typedef u32 (*gfio_read_proc)(GF_FileIO *fileio, u8 *buffer, u32 bytes);

/*! write proc for memory file IO
\param fileio target file IO object
\param buffer buffer to write.
\param bytes number of bytes to write. If 0, acts as fflush
\return number of bytes write, 0 if error
 */
typedef u32 (*gfio_write_proc)(GF_FileIO *fileio, u8 *buffer, u32 bytes);

/*! positon tell proc for memory file IO
\param fileio target file IO object
\return position in bytes from file start
 */
typedef s64 (*gfio_tell_proc)(GF_FileIO *fileio);
/*! end of file proc for memory file IO
\param fileio target file IO object
\return GF_TRUE if end of file, GF_FALSE otherwise
 */
typedef Bool (*gfio_eof_proc)(GF_FileIO *fileio);
/*! printf proc for memory file IO
\param fileio target file IO object
\param format format string to use
\param args variable argument list for printf, already initialized (va_start called)
\return same as vfprint
 */
typedef int (*gfio_printf_proc)(GF_FileIO *fileio, const char *format, va_list args);

/*! Creates a new file IO object

There is no guarantee that the corresponding resource will be opened by the framework, it is therefore the caller responsibility to track objects created by
gf_fileio_new or as a response to open with mode "url".

\param url the original URL this file IO object wraps
\param udta opaque data for caller
\param open open proc for IO, must not be NULL
\param seek seek proc for IO, must not be NULL
\param read read proc for IO - if NULL the file is considered write only
\param write write proc for IO - if NULL the file is considered read only
\param tell tell proc for IO, must not be NULL
\param eof eof proc for IO, must not be NULL
\param printf printf proc for IO, may be NULL
\return the newly created file IO wrapper
 */
GF_FileIO *gf_fileio_new(char *url, void *udta,
	gfio_open_proc open,
	gfio_seek_proc seek,
	gfio_read_proc read,
	gfio_write_proc write,
	gfio_tell_proc tell,
	gfio_eof_proc eof,
	gfio_printf_proc printf);

/*! Deletes a new fileIO object
\param fileio the File IO object to delete
*/
void gf_fileio_del(GF_FileIO *fileio);

/*! Gets associated user data of a fileIO object
\param fileio target file IO object
\return the associated user data
*/
void *gf_fileio_get_udta(GF_FileIO *fileio);

/*! Gets URL of a fileIO object.
 The url uses the protocol scheme "gfio://"
\param fileio target file IO object
\return the file IO url to use
*/
const char * gf_fileio_url(GF_FileIO *fileio);

/*! Gets a fileIO object from memory - the resulting fileIO can only be opened once at any time but can be closed/reopen.
\param URL of source data, may be null
\param data memory, must be valid until next close
\param size memory size
\return new file IO - use gf_fclose() on this object to close it
*/
GF_FileIO *gf_fileio_from_mem(const char *URL, const u8 *data, u32 size);

/*! Cache state for file IO object*/
typedef enum
{
	/*! File caching is in progress*/
	GF_FILEIO_CACHE_IN_PROGRESS=0,
	/*! File caching is done */
	GF_FILEIO_CACHE_DONE,
	/*! No file caching (file is not stored to disk)) */
	GF_FILEIO_NO_CACHE,
} GF_FileIOCacheState;


/*! Sets statistics on a fileIO object.
\param fileio target file IO object
\param bytes_done number of bytes fetched for this file
\param file_size total size of this file, 0 if unknown
\param cache_state if GF_TRUE, means the file is completely available
\param bytes_per_sec reception bytes per second, 0 if unknown
*/
void gf_fileio_set_stats(GF_FileIO *fileio, u64 bytes_done, u64 file_size, GF_FileIOCacheState cache_state, u32 bytes_per_sec);

/*! Gets statistics on a fileIO object.
\param fileio target file IO object
\param bytes_done number of bytes fetched for this file (may be NULL)
\param file_size total size of this file, 0 if unknown (may be NULL)
\param cache_state set to caching state for this object (may be NULL)
\param bytes_per_sec reception bytes per second, 0 if unknown (may be NULL)
\return GF_TRUE if success, GF_FALSE otherwise
*/
Bool gf_fileio_get_stats(GF_FileIO *fileio, u64 *bytes_done, u64 *file_size, GF_FileIOCacheState *cache_state, u32 *bytes_per_sec);

/*! Sets write state of a  fileIO object.
\param fileio target file IO object
\param write_state the state to set
*/
void gf_fileio_set_write_state(GF_FileIO *fileio, GF_FileIOWriteState write_state);

/*! Checks if a FileIO object can write
\param fileio target file IO object
\param url the original resource URL to open
\param mode the desired open mode
\param out_err set to error code if any, must not be NULL
\return file IO object for this resource
*/
GF_FileIO *gf_fileio_open_url(GF_FileIO *fileio, const char *url, const char *mode, GF_Err *out_err);


/*! Tags a FileIO object to be accessed from main thread only
\param fileio target file IO object
\return error if any
*/
GF_Err gf_fileio_tag_main_thread(GF_FileIO *fileio);

/*! Check if a FileIO object is to be accessed from main thread only

 \note Filters accessing FileIO objects by other means that a filter option must manually tag themselves as main thread, potentially rescheduling a configure call to next process. Not doing so can result in binding crashes in multi-threaded mode.

\param url target url
\return GF_TRUE if object is tagged for main thread, GF_FALSE otherwise
*/
Bool gf_fileio_is_main_thread(const char *url);

/*! Gets GF_FileIO object from its URL
 The url uses the protocol scheme "gfio://"
\param url the URL of  the File IO object
\return the file IO object
*/
GF_FileIO *gf_fileio_from_url(const char *url);

/*! Constructs a new GF_FileIO object from a URL
 The url can be absolute or relative to the parent GF_FileIO. This is typcically needed by filters (dash input, dasher, NHML/NHNT writers...) generating or consuming additional files associated with the main file IO object but being written or read by other filters.
 The function will not open the associated resource, only create the file IO wrapper for later usage
 If you need to create a new fileIO to be opened immediately, use \ref gf_fopen_ex.

\param fileio parent file IO object
\param new_res_url the original URL of  the new object to create
\return the url (gfio://) of the created file IO object, NULL otherwise
*/
const char *gf_fileio_factory(GF_FileIO *fileio, const char *new_res_url);

/*! Translates a FileIO object URL into the original resource URL
\param url the URL of  the File IO object
\return the original resource URL associated with the file IO object
*/
const char * gf_fileio_translate_url(const char *url);
/*! Gets a FileIO  original resource URL
\param fileio target file IO object
\return the original resource URL associated with the file IO object
*/
const char * gf_fileio_resource_url(GF_FileIO *fileio);

/*! Checks if a FileIO object can read
\param fileio target file IO object
\return GF_TRUE if read is enabled on this object
*/
Bool gf_fileio_read_mode(GF_FileIO *fileio);
/*! Checks if a FileIO object can write
\param fileio target file IO object
\return GF_TRUE if write is enabled on this object
*/
Bool gf_fileio_write_mode(GF_FileIO *fileio);

/*!	@} */

/*!
\addtogroup hash_grp
\brief Data hashing, integrity and generic compression

This section documents misc data functions such as integrity and parsing such as SHA-1 hashing CRC checksum, 128 bit ID parsing...

@{
 */


/*!
\brief CRC32 compute

Computes the CRC32 value of a buffer.
\param data buffer
\param size buffer size
\return computed CRC32
 */
u32 gf_crc_32(const u8 *data, u32 size);


/**
Compresses a data buffer in place using zlib/deflate. Buffer may be reallocated in the process.
\param data pointer to the data buffer to be compressed
\param data_len length of the data buffer to be compressed
\param out_size pointer for output buffer size
\return error if any
 */
GF_Err gf_gz_compress_payload(u8 **data, u32 data_len, u32 *out_size);

/**
Compresses a data buffer in place using zlib/deflate. Buffer may be reallocated in the process.
\param data pointer to the data buffer to be compressed
\param data_len length of the data buffer to be compressed
\param out_size pointer for output buffer size
\param data_offset offset in source buffer - the input payload size is data_len - data_offset
\param skip_if_larger if GF_TRUE, will not override source buffer if compressed version is larger than input data
\param out_comp_data if not NULL, the compressed result is set in this pointer rather than doing inplace compression
\param use_gz if true, GZ header is present
\return error if any
 */
GF_Err gf_gz_compress_payload_ex(u8 **data, u32 data_len, u32 *out_size, u8 data_offset, Bool skip_if_larger, u8 **out_comp_data, Bool use_gz);

/**
Decompresses a data buffer using zlib/inflate.
\param data data buffer to be decompressed
\param data_len length of the data buffer to be decompressed
\param uncompressed_data pointer to the uncompressed data buffer. The resulting buffer is NULL-terminated. It is the responsibility of the caller to free this buffer.
\param out_size size of the uncompressed buffer
\return error if any
 */
GF_Err gf_gz_decompress_payload(u8 *data, u32 data_len, u8 **uncompressed_data, u32 *out_size);

/**
Decompresses a data buffer using zlib/inflate.
\param data data buffer to be decompressed
\param data_len length of the data buffer to be decompressed
\param uncompressed_data pointer to the uncompressed data buffer. The resulting buffer is NULL-terminated. It is the responsibility of the caller to free this buffer.
\param out_size size of the uncompressed buffer
\param use_gz if true, gz header is present
\return error if any
 */
GF_Err gf_gz_decompress_payload_ex(u8 *data, u32 data_len, u8 **uncompressed_data, u32 *out_size, Bool use_gz);


/**
Compresses a data buffer in place using LZMA. Buffer may be reallocated in the process.
\param data pointer to the data buffer to be compressed
\param data_len length of the data buffer to be compressed
\param out_size pointer for output buffer size
\return error if any
 */
GF_Err gf_lz_compress_payload(u8 **data, u32 data_len, u32 *out_size);

/**
Decompresses a data buffer using LZMA.
\param data data buffer to be decompressed
\param data_len length of the data buffer to be decompressed
\param uncompressed_data pointer to the uncompressed data buffer. It is the responsibility of the caller to free this buffer.
\param out_size size of the uncompressed buffer
\return error if any
 */
GF_Err gf_lz_decompress_payload(u8 *data, u32 data_len, u8 **uncompressed_data, u32 *out_size);


#ifndef GPAC_DISABLE_ZLIB
/*! Wrapper around gzseek, same parameters
\param file target gzfile
\param offset offset in file
\param whence same as gzseek
\return same as gzseek
*/
u64 gf_gzseek(void *file, u64 offset, int whence);
/*! Wrapper around gf_gztell, same parameters
 \param file target gzfile
 \return postion in file
 */
u64 gf_gztell(void *file);
/*! Wrapper around gzrewind, same parameters
 \param file target gzfile
 \return same as gzrewind
 */
s64 gf_gzrewind(void *file);
/*! Wrapper around gzeof, same parameters
 \param file target gzfile
 \return same as gzeof
 */
int gf_gzeof(void *file);
/*! Wrapper around gzclose, same parameters
\param file target gzfile
\return same as gzclose
*/
int gf_gzclose(void *file);
/*! Wrapper around gzerror, same parameters
\param file target gzfile
\param errnum same as gzerror
\return same as gzerror
 */
const char * gf_gzerror (void *file, int *errnum);
/*! Wrapper around gzclearerr, same parameters
 \param file target gzfile
 */
void gf_gzclearerr(void *file);
/*! Wrapper around gzopen, same parameters
\param path the file name to open
\param mode the file open mode
\return open file
 */
void *gf_gzopen(const char *path, const char *mode);
/*! Wrapper around gzread, same parameters
 \param file target gzfile
 \param buf same as gzread
 \param len same as gzread
 \return same as gzread
 */
int gf_gzread(void *file, void *buf, unsigned len);
/*! Wrapper around gzdirect, same parameters
 \param file target gzfile
 \return same as gzdirect
 */
int gf_gzdirect(void *file);
/*! Wrapper around gzgetc, same parameters
 \param file target gzfile
 \return same as gzgetc
 */
int gf_gzgetc(void *file);
/*! Wrapper around gzgets, same parameters
 \param file target gzfile
 \param buf same as gzread
 \param len same as gzread
 \return same as gzgets
*/
char * gf_gzgets(void *file, char *buf, int len);
#endif


/*! SHA1 context*/
typedef struct __sha1_context GF_SHA1Context;

/*! SHA1 message size */
#define GF_SHA1_DIGEST_SIZE		20

/*! create SHA-1 context
\return the SHA1 context*/
GF_SHA1Context *gf_sha1_starts();
/*! adds byte to the SHA-1 context
\param ctx the target SHA1 context
\param input data to hash
\param length size of data in bytes
*/
void gf_sha1_update(GF_SHA1Context *ctx, u8 *input, u32 length);
/*! generates SHA-1 of all bytes ingested
\param ctx the target SHA1 context
\param digest buffer to store message digest
*/
void gf_sha1_finish(GF_SHA1Context *ctx, u8 digest[GF_SHA1_DIGEST_SIZE] );

/*! gets SHA1 message digest of a file
\param filename name of file to hash
\param digest buffer to store message digest
\return error if any
*/
GF_Err gf_sha1_file(const char *filename, u8 digest[GF_SHA1_DIGEST_SIZE]);

/*! gets SHA1 message digest of a opened file
\param file  handle to open file
\param digest buffer to store message digest
\return error if any
*/
GF_Err gf_sha1_file_ptr(FILE *file, u8 digest[GF_SHA1_DIGEST_SIZE] );

/*! gets SHA-1 of input buffer
\param buf input buffer to hash
\param buflen sizeo of input buffer in bytes
\param digest buffer to store message digest
 */
void gf_sha1_csum(u8 *buf, u32 buflen, u8 digest[GF_SHA1_DIGEST_SIZE]);
/*! @} */

/*! checksum size for SHA-256*/
#define GF_SHA256_DIGEST_SIZE 32
/*! gets SHA-256 of input buffer
\param buf input buffer to hash
\param buflen sizeo of input buffer in bytes
\param digest buffer to store message digest
 */
void gf_sha256_csum(const void *buf, u64 buflen, u8 digest[GF_SHA256_DIGEST_SIZE]);


/*!
\addtogroup libsys_grp
@{
*/

/*! gets a global config key value from its section and name.
\param secName the desired key parent section name
\param keyName the desired key name
\return the desired key value if found, NULL otherwise.
*/
const char *gf_opts_get_key(const char *secName, const char *keyName);

/*! sets a global config key value from its section and name.
\param secName the desired key parent section name
\param keyName the desired key name
\param keyValue the desired key value
\note this will also create both section and key if they are not found in the configuration file
\return error if any
*/
GF_Err gf_opts_set_key(const char *secName, const char *keyName, const char *keyValue);

/*! removes all entries in the given section of the global config
\param secName the target section
*/
void gf_opts_del_section(const char *secName);
/*! gets the number of sections in the global config
\return the number of sections
*/
u32 gf_opts_get_section_count();
/*! gets a section name based on its index in the global config
\param secIndex 0-based index of the section to query
\return the section name if found, NULL otherwise
*/
const char *gf_opts_get_section_name(u32 secIndex);

/*! gets the number of keys in a section of the global config
\param secName the target section
\return the number of keys in the section
*/
u32 gf_opts_get_key_count(const char *secName);
/*! gets the number of keys in a section of the global config
\param secName the target section
\param keyIndex 0-based index of the key in the section
\return the key name if found, NULL otherwise
*/
const char *gf_opts_get_key_name(const char *secName, u32 keyIndex);

/*! gets a global config boolean value from its section and name.
\param secName the desired key parent section name
\param keyName the desired key name
\return the desired key value if found, GF_FALSE otherwise.
*/
Bool gf_opts_get_bool(const char *secName, const char *keyName);

/*! gets a global config integer value from its section and name.
\param secName the desired key parent section name
\param keyName the desired key name
\return the desired key value if found, 0 otherwise.
*/
u32 gf_opts_get_int(const char *secName, const char *keyName);

/*! gets a global config key value from its section and name.
\param secName the desired key parent section name
\param keyName the desired key name
\return the desired key value if found and if the key is not restricted, NULL otherwise.
*/
const char *gf_opts_get_key_restricted(const char *secName, const char *keyName);

/*!
 * Do not save modification to global options
\return error code
 */
GF_Err gf_opts_discard_changes();

/*!
 * Force immediate write of config
\return error code
 */
GF_Err gf_opts_save();

/*!
 * Returns file name of global config
\return file name of global config or NULL if libgpac is not initialized
 */
const char *gf_opts_get_filename();

/*!
 * Gets GPAC shared directory (gui, shaders, etc ..)
\param path_buffer GF_MAX_PATH buffer to store output
\return GF_TRUE if success, GF_FALSE otherwise
 */
Bool gf_opts_default_shared_directory(char *path_buffer);


/*!
 * Checks given user and password are valid
\param username user name
\param password password
\return GF_OK if success, GF_NOT_FOUND if no such user or GF_AUTHENTICATION_FAILURE if wrong password
 */
GF_Err gf_creds_check_password(const char *username, char *password);

/*!
 * Checks given user belongs to list of users or groups.
\param username user name
\param users comma-seprated list of users to check, may be NULL
\param groups comma-seprated list of groups to check, may be NULL
\return GF_TRUE if success, GF_FALSE otherwise
 */
Bool gf_creds_check_membership(const char *username, const char *users, const char *groups);

/*! @} */


//! @cond Doxygen_Suppress

#if defined(GPAC_DISABLE_3D) && !defined(GPAC_DISABLE_REMOTERY)
#define GPAC_DISABLE_REMOTERY 1
#endif

#ifdef GPAC_DISABLE_REMOTERY
#define RMT_ENABLED 0
#else
#define RMT_USE_OPENGL	1
#endif

#include <gpac/Remotery.h>

#define GF_RMT_AGGREGATE	RMTSF_Aggregate
/*! begins remotery CPU sample*/
#define gf_rmt_begin rmt_BeginCPUSample
/*! begins remotery CPU sample with hash*/
#define gf_rmt_begin_hash rmt_BeginCPUSampleStore
/*! ends remotery CPU sample*/
#define gf_rmt_end rmt_EndCPUSample
/*! sets remotery thread name*/
#define gf_rmt_set_thread_name rmt_SetCurrentThreadName
/*! logs remotery text*/
#define gf_rmt_log_text rmt_LogText
/*! begins remotery OpenGL sample*/
#define gf_rmt_begin_gl rmt_BeginOpenGLSample
/*! begins remotery OpenGL sample with hash*/
#define gf_rmt_begin_gl_hash rmt_BeginOpenGLSampleStore
/*!ends remotery OpenGL sample*/
#define gf_rmt_end_gl rmt_EndOpenGLSample

//! @endcond


/* \cond dummy */

/*to call whenever the OpenGL library is opened - this function is needed to bind OpenGL and remotery, and to load
OpenGL extensions on windows
not exported, and not included in src/compositor/gl_inc.h since it may be needed even when no OpenGL
calls are made by the caller*/
void gf_opengl_init();

typedef enum
{
	//pbo not enabled
	GF_GL_PBO_NONE=0,
	//pbo enabled, both push and glTexImage textures are done in gf_gl_txw_upload
	GF_GL_PBO_BOTH,
	//push only is done in gf_gl_txw_upload
	GF_GL_PBO_PUSH,
	// glTexImage textures only is done in gf_gl_txw_upload
	GF_GL_PBO_TEXIMG,
} GF_GLPBOState;

typedef struct _gl_texture_wrap
{
	u32 textures[4];
	u32 PBOs[4];

	u32 nb_textures;
	u32 width, height, pix_fmt, stride, uv_stride;
	Bool is_yuv;
	u32 bit_depth, uv_w, uv_h;
	u32 scale_10bit;
	u32 init_active_texture;

	u32 gl_format;
	u32 bytes_per_pix;
	Bool has_alpha;
	Bool internal_textures;
	Bool uniform_setup;
	u32 memory_format;
	struct _gf_filter_frame_interface *frame_ifce;
	Bool first_tx_load;

	//PBO state - must be managed by caller, especially if using separated push and texImg steps through gf_gl_txw_setup calls
	GF_GLPBOState pbo_state;
	Bool flip;
	//YUV is full video range
	Bool fullrange;
	s32 mx_cicp;

	u32 last_program;
} GF_GLTextureWrapper;

Bool gf_gl_txw_insert_fragment_shader(u32 pix_fmt, const char *tx_name, char **f_source, Bool y_flip);
Bool gf_gl_txw_setup(GF_GLTextureWrapper *tx, u32 pix_fmt, u32 width, u32 height, u32 stride, u32 uv_stride, Bool linear_interp, struct _gf_filter_frame_interface *frame_ifce, Bool full_range, s32 matrix_coef_or_neg);
Bool gf_gl_txw_upload(GF_GLTextureWrapper *tx, const u8 *data, struct _gf_filter_frame_interface *frame_ifce);
Bool gf_gl_txw_bind(GF_GLTextureWrapper *tx, const char *tx_name, u32 gl_program, u32 texture_unit);
void gf_gl_txw_reset(GF_GLTextureWrapper *tx);

/* \endcond */


/*! macros to get the size of an array of struct*/
#define GF_ARRAY_LENGTH(a) (sizeof(a) / sizeof((a)[0]))

#ifdef __cplusplus
}
#endif


#endif		/*_GF_CORE_H_*/
