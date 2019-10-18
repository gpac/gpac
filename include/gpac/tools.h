/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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

Macro formating a 4-character code (or 4CC) "abcd" as 0xAABBCCDD
*/
#ifndef GF_4CC
#define GF_4CC(a,b,c,d) ((((u32)a)<<24)|(((u32)b)<<16)|(((u32)c)<<8)|((u32)d))
#endif


/*! converts four character code to string
\param type a four character code
\return a printable form of the code
*/
const char *gf_4cc_to_str(u32 type);

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
	/*! Bitstream is not compliant to the specfication it refers to*/
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
	/*! Script not ready for playback */
	GF_SCRIPT_NOT_READY						= -51,
	/*! Bad configuration for the current contex */
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
#define GF_SAFEALLOC(__ptr, __struct) { __ptr = (__struct *) gf_malloc(sizeof(__struct)); if (__ptr) memset((void *) __ptr, 0, sizeof(__struct)); }

/*!
\brief Memory allocation for an array of n structs
\hideinitializer

Macro allocating memory for n structures and zero-ing it
*/
#define GF_SAFE_ALLOC_N(__ptr, __n, __struct) { __ptr = (__struct *) gf_malloc( __n * sizeof(__struct)); if (__ptr) memset((void *) __ptr, 0, __n * sizeof(__struct)); }


/*!
\brief dynamic string concatenation

Dynamic concatenation of string with optional separator
\param str pointer to destination string pointer
\param to_append string to append
\param sep optional separator string to insert before concatenation. If set and initial string is NULL, will not be appended
\return error code
 */
GF_Err gf_dynstrcat(char **str, const char *to_append, const char *sep);

/*! @} */

/*!
\addtogroup libsys_grp
\brief Library configuration

These functions are used to initialize, shutdown and configure libgpac.

The library shall be initialized using \ref gf_sys_init and terminated using gf_sys_close

The library can usually be configured from command line if your program uses \ref gf_sys_set_args.

The library can also be configured from your program using \ref gf_opts_set_key and related functions right after initializing the library.

For more information on configuration options, see \code gpac -hx core \endcode and https://github.com/gpac/gpac/wiki/core_options

For more information on filters configuration options, see https://github.com/gpac/gpac/wiki/Filters

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

Inits the system high-resolution clock if any, CPU usage manager, random number and GPAC global config. It is strongly recommended to call this function before calling any other GPAC functions, since on some systems (like winCE) it may result in a better memory usage estimation.

The profile allows using a different global config file than the default, and may be a name (without / or \\) or point to an existing config file.
\note This can be called several times but only the first call will result in system setup.
\param mem_tracker_type memory tracking mode
\param profile name of the profile to load, NULL for default.
\return Error code if any
 */
GF_Err gf_sys_init(GF_MemTrackerType mem_tracker_type, const char *profile);
/*!
\brief System closing
Closes the system high-resolution clock and any CPU associated ressources.
\note This can be called several times but the system will be closed when no more users are counted.
 */
void gf_sys_close();

/*!
\brief System arguments

Sets the user app arguments (used by GUI mode)
\param argc Number of arguments
\param argv Array of arguments
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
\brief checks if test mode is enabled

Checks if test mode is enabled (no date nor GPAC version should be written).
\return GF_TRUE if test mode is enabled, GF_FALSE otherwise.
 */
Bool gf_sys_is_test_mode();


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
\param strict strict behaviour when encoutering a serious error.
\return old value before the call.
 */
Bool gf_log_set_strict_error(Bool strict);

/*!
\brief gets string-formated log tools

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
	/*! Log message from the RTP/RTCP stack (TS info) and packet structure & hinting (debug)*/
	GF_LOG_RTP,
	/*! Log message from authoring subsystem (file manip, import/export)*/
	GF_LOG_AUTHOR,
	/*! Log message from the sync layer of the terminal*/
	GF_LOG_SYNC,
	/*! Log message from a codec*/
	GF_LOG_CODEC,
	/*! Log message from any XML parser (context loading, etc)*/
	GF_LOG_PARSER,
	/*! Log message from the terminal/compositor, indicating media object state*/
	GF_LOG_MEDIA,
	/*! Log message from the scene graph/scene manager (handling of nodes and attribute modif, DOM core)*/
	GF_LOG_SCENE,
	/*! Log message from the scripting engine APIs - does not cover alert() in the script code itself*/
	GF_LOG_SCRIPT,
	/*! Log message from event handling*/
	GF_LOG_INTERACT,
	/*! Log message from compositor*/
	GF_LOG_COMPOSE,
	/*! Log for video object cache */
	GF_LOG_CACHE,
	/*! Log message from multimedia I/O devices (audio/video input/output, ...)*/
	GF_LOG_MMIO,
	/*! Log for runtime info (times, memory, CPU usage)*/
	GF_LOG_RTI,
	/*! Log for SMIL timing and animation*/
	GF_LOG_SMIL,
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
	/*! Log for all messages coming from filters */
	GF_LOG_FILTER,
	/*! Log for filter scheduler only */
	GF_LOG_SCHEDULER,
	/*! Log for all messages coming from GF_Terminal or script alert()*/
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
/*note:
		to turn log on, change to GPAC_ENABLE_LOG
		to turn log off, change to GPAC_DISABLE_LOG
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
#define GF_LOG(_ll, _lm, __args)
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

/*!
 * Blob structure used to pass data pointer around
 */
typedef struct
{
	/*! data block of blob */
	u8 *data;
	/*! size of blob */
	u32 size;
} GF_Blob;

/*!
 * Retrieves data associated with a blob url
\param blob_url URL of blob object (ie gmem://%p)
\param out_data if success, set to blob data pointer
\param out_size if success, set to blob data size
\return error code
 */
GF_Err gf_blob_get_data(const char *blob_url, u8 **out_data, u32 *out_size);

/*!	@} */

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
\param month the month
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

This section documents time functionalities and CPU management in GPAC.

@{
 */

/*!
\brief reads a file into memory

Reads a local file into memory, in binary open mode.
\param file_name path on disk of the file to read
\param out_data pointer to allocted adress, to be freed by caller
\param out_size pointer to allocted size
\return error code if any
 */
GF_Err gf_file_load_data(const char *file_name, u8 **out_data, u32 *out_size);

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
GF_Err gf_cleanup_dir(const char* DirPathName);


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
\param ptr same as fwrite
\param size same as fwrite
\param nmemb same as fwrite
\param stream same as fwrite
\return same as fwrite
 *
*/
size_t gf_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

/*!
\brief large file opening

Opens a large file (>4GB)
\param file_name same as fopen
\param mode same as fopen
\return stream handle of the file object
\note You only need to call this function if you're suspecting the file to be a large one (usually only media files), otherwise use regular stdio.
*/
FILE *gf_fopen(const char *file_name, const char *mode);

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
\param f Same semantics as ftell
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
\return new position in the file
\note You only need to call this function if you're suspecting the file to be a large one (usually only media files), otherwise use regular stdio.
*/
u64 gf_fseek(FILE *f, s64 pos, s32 whence);

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
GF_Err gf_delete_file(const char *fileName);

/*!
\brief File Move

Moves or renames a file or directory.
\param fileName absolute path of the file / directory to move or rename
\param newFileName absolute new path/name of the file / directory
\return error if any
*/
GF_Err gf_move_file(const char *fileName, const char *newFileName);

/*!
\brief Temporary File Creation

Creates a new temporary file in binary mode
\param fileName if not NULL, strdup() of the temporary filename when created by GPAC (NULL otherwise as the system automatically removes its own tmp files)
\return stream handle to the new file ressoucre
 */
FILE *gf_temp_file_new(char ** const fileName);


/*!
\brief File Modification Time

Gets the modification time of the given file. The exact meaning of this value is system dependent
\param filename file to check
\return modification time of the file
 */
u64 gf_file_modification_time(const char *filename);

/*!
\brief File existence check

Moves or renames a file or directory.
\param fileName absolute path of the file / directory to move or rename
\return GF_TRUE if file exists
 */
Bool gf_file_exists(const char *fileName);

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
\return GF_OK if evertything went fine
 */
GF_Err gf_gz_compress_payload(u8 **data, u32 data_len, u32 *out_size);

/**
Decompresses a data buffer using zlib/deflate.
\param data data buffer to be decompressed
\param data_len length of the data buffer to be decompressed
\param uncompressed_data pointer to the uncompressed data buffer. It is the responsibility of the caller to free this buffer.
\param out_size size of the uncompressed buffer
\return GF_OK if evertything went fine
 */
GF_Err gf_gz_decompress_payload(u8 *data, u32 data_len, u8 **uncompressed_data, u32 *out_size);

/**
Compresses a data buffer in place using LZMA. Buffer may be reallocated in the process.
\param data pointer to the data buffer to be compressed
\param data_len length of the data buffer to be compressed
\param out_size pointer for output buffer size
\return GF_OK if evertything went fine
 */
GF_Err gf_lz_compress_payload(u8 **data, u32 data_len, u32 *out_size);

/**
Decompresses a data buffer using LZMA.
\param data data buffer to be decompressed
\param data_len length of the data buffer to be decompressed
\param uncompressed_data pointer to the uncompressed data buffer. It is the responsibility of the caller to free this buffer.
\param out_size size of the uncompressed buffer
\return GF_OK if evertything went fine
 */
GF_Err gf_lz_decompress_payload(u8 *data, u32 data_len, u8 **uncompressed_data, u32 *out_size);


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

/*! gets SHA-1 of input buffer
\param buf input buffer to hash
\param buflen sizeo of input buffer in bytes
\param digest buffer to store message digest
 */
void gf_sha1_csum(u8 *buf, u32 buflen, u8 digest[GF_SHA1_DIGEST_SIZE]);
/*! @} */


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

/*! @} */


//! @cond Doxygen_Suppress

#ifdef GPAC_DISABLE_3D
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
#ifdef GPAC_CONFIG_ANDROID
typedef void (*fm_callback_func)(void *cbk_obj, u32 type, u32 param, int *value);
extern void gf_fm_request_set_callback(void *cbk_obj, fm_callback_func cbk_func);
void gf_fm_request_call(u32 type, u32 param, int *value);
#endif //GPAC_CONFIG_ANDROID

/*to call whenever the OpenGL library is opened - this function is needed to bind openGL and remotery, and to load
openGL extensions on windows
not exported, and not included in src/compositor/gl_inc.h since it may be needed even when no OpenGL 
calls are made by the caller*/
void gf_opengl_init();

/* \endcond */

/*! macros to get the size of an array of struct*/
#define GF_ARRAY_LENGTH(a) (sizeof(a) / sizeof((a)[0]))

#ifdef __cplusplus
}
#endif


#endif		/*_GF_CORE_H_*/

