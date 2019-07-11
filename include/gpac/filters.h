/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2019
 *					All rights reserved
 *
 *  This file is part of GPAC / filters sub-project
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

#ifndef _GF_FILTERS_H_
#define _GF_FILTERS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>
#include <gpac/list.h>
#include <gpac/events.h>
#include <gpac/user.h>
#include <gpac/constants.h>
#include <gpac/download.h>

//for offsetof()
#include <stddef.h>

/*! \file "gpac/filters.h"
 *	\brief Filter management of GPAC.
 *
 * This file contains all exported functions for filter management of the GPAC framework.
*/

/*! \defgroup filters_grp Filter Management
 	\brief Filter Management of GPAC.

API Documentation of the filter managment system of GPAC.

The filter management in GPAC is built using the following core objects:
- \ref GF_FilterSession in charge of
 loading filters from register, managing argument parsing and co
 resolving filter graphs to handle PID connection(s)
 tracking data packets and properties exchanged on PIDs
 scheduling tasks between filters
 ensuring thread-safe filter state: a filter may be called from any thread in the session (unless explicitely asked not to), but only by a single thread at any time.
- \ref GF_FilterRegister static structure describing possible entry points of the filter, possible arguments and input output PID capabilities.
	Each filter share the same API (register definition) regardless of its type: source/sink, mux/demux, encode/decode, raw media processing, encoded media processing, ...
- \ref GF_Filter is an instance of the filter register. A filter implementation typical tasks are:
 accepting new input PIDs (for non source filters)
 defining new output PIDs (for non sink filters), applying any property change due to filter processing
 consuming packets on the input PIDs
 dispatching packets on the output PIDs
- \ref GF_FilterPid handling the connections between two filters.
	PID natively supports fan-out (one filter PID connecting to multiple destinations).
	A PID is in charge of dispatching packets to possible destinations and storing PID properties in sync with dispatched packets.
	Whenever PID properties change, the next packet sent on that PID is associated with the new state, and the destination filter(s) will be called
	upon fetching the new packet. This is the one of the two reentrant code of a filter, the other one being the \ref GF_FEVT_INFO_UPDATE event.
	When blocking mode is not disabled at the session filter, a PID is also in charge of managing its occupancy through either a number of packets or the
	cumulated duration of the packets it is holding.
	Whenever a PID holds too much data, it enters a blocking state. A filter with ALL its output PIDs in a blocked state won't be scheduled
	for processing. This is a semi-blocking design, which imply that if a filter has one of its PIDs in a non blocking state, it will be scheduled for processing. If a PID has multiple destinations and one of the destination consumes faster than the other one, the filter is currently not blocking (this might change in the near future).
	A PID is in charge of managing the packet references across filters, by performing memory management of allocated data packets
	 (avoid alloc/free at each packet but rather recycle the memory) and tracking shared packets references.
- \ref GF_FilterPacket holding data to dispatch from a filter on a given PID.
	Packets are always associated to a single output PID, ie it is not possible for a filter to send one packet to multiple PIDs, the data has to be cloned.
	Packets have default attributes such as timestamps, size, random access status, start/end frame, etc, as well as optional properties.
	All packets are reference counted.
	A packet can hold allocated block on the output PID, a pointer to some filter internal data, a data reference to a single input packet, or a frame interface object used for accessing data or OpenGL textures of the emitting filter.
	Packets holding data references rather than copy are notified back to their creators upon destruction.
- \ref GF_PropertyValue holding various properties for a PID or a packet
	Properties can be copied/merged between input and output PIDs, or input and output packets. These properties are reference counted.
	Two kinds of properties are defined, built-in ones which use a 32 bit identifier (usually a four character code), and user properties identified by a string.
	PID properties are defined by the filter creating the PID. They can be overridden/added after being set by the filter by specifying fragment properties
	in the filter arguments. For example fin=src=myfile.foo:#FEXT=bar will override the file extension property (FEXT) foo to bar AFTER the PID is being defined.
- \ref GF_FilterEvent used to pass various events (play/stop/buffer requirements/...) up and down the filter chain.
	This part of the API will likely change in the future, being merged with the global GF_Event of GPAC.


GPAC comes with a set of built-in filters in libgpac. It is also possible to define external filters in dynamic libraries. GPAC will look for such libraries
 in folders listed in GPAC config file section core, key mod-dirs. The files SHALL be named gf_* and export a function called RegisterFilter
 with the following prototype:

\param fsess is set to NULL unless meta filters are listed, in which case the filter register should list all possible meta filters it supports
\return a GF_FilterRegister structure used for instantiating the filter.
const GF_FilterRegister *RegisterFilter(GF_FilterSession *fsess);

*/

/*! \hideinitializer
 *	A packet byte offset is set to this value when not valid
*/
#define GF_FILTER_NO_BO 0xFFFFFFFFFFFFFFFFUL
/*! \hideinitializer
 *	A packet timestamp (Decoding or Composition) is set to this value when not valid
*/
#define GF_FILTER_NO_TS 0xFFFFFFFFFFFFFFFFUL

/*! \hideinitializer
 * Filter Session object
 */
typedef struct __gf_filter_session GF_FilterSession;

/*! \hideinitializer
 *	Filter object
 */
typedef struct __gf_filter GF_Filter;
/*! \hideinitializer
 *	Filter PID object
 */
typedef struct __gf_filter_pid GF_FilterPid;

/*! \hideinitializer
 *	Filter Packet object
 */
typedef struct __gf_filter_pck GF_FilterPacket;
/*!
 *	Filter Packet destructor function prototype
 */
typedef void (*gf_fsess_packet_destructor)(GF_Filter *filter, GF_FilterPid *PID, GF_FilterPacket *pck);

/*!
 *	Filter Event object
 */
typedef union __gf_filter_event GF_FilterEvent;

/*!
 *	Filter Session Task object
 */
typedef struct __gf_fs_task GF_FSTask;

/*!
 *	Filter Register object
 */
typedef struct __gf_filter_register GF_FilterRegister;
/*!
 *	Filter Property object
 */
typedef struct __gf_prop_val GF_PropertyValue;

/*!
 *	Filter Property Reference object, used for info query only
 */
typedef struct __gf_prop_entry GF_PropertyEntry;

/*!
 *\addtogroup fs_grp Filter Session
 *\ingroup filters_grp
 *\brief Filter Session

 The GPAC filter session object allows building media pipelines using multiple sources and destinations and arbitrary filter chains.

 Filters are described through a \ref GF_FilterRegister structure. A set of built-in filters are available, and user-defined filters can be added or removed at runtime.

 The filter session keeps an internal graph representation of all available filters and their possible input connections, which is used when resolving connections between filters.

 The number of \ref GF_FilterCapability matched between registries defines the weight of the connection.

 Paths from an instantiated filter are enabled/disabled based on the source PID capabilities.

 Paths to destination are recomputed for each destination, based on the instantiated destination filter capabilities.

 The graph edges are then enabled in the possible subgraphs allowed by the destination capabilities, and unused filter registries (without enabled input connections) are removed from the graph.

 The resulting weighted graph is then solved using Dijkstra's algorithm, using filter priority in case of weight equality.

 The filter session works by default in a semi-blocking state. Whenever output PID buffers on a filter are all full, the filter is marked as blocked and not scheduled for processing. Whenever one output PID buffer is not full, the filter unblocks.

 This implies that PID buffers may grow quite large if a filter is consuming data from a PID at a much faster rate than another filter consuming from that same PID.
 *	@{
 */


/*! Filter session scheduler type */
typedef enum
{
	/*! In this mode, the scheduler does not use locks for packet and property queues. Main task list is mutex-protected */
	GF_FS_SCHEDULER_LOCK_FREE=0,
	/*! In this mode, the scheduler uses locks for packet and property queues. Defaults to lock-free if no threads are used. Main task list is mutex-protected */
	GF_FS_SCHEDULER_LOCK,
	/*! In this mode, the scheduler does not use locks for packet and property queues, nor for the main task list */
	GF_FS_SCHEDULER_LOCK_FREE_X,
	/*! In this mode, the scheduler uses locks for packet and property queues even if single-threaded (test mode) */
	GF_FS_SCHEDULER_LOCK_FORCE,
	/*! In this mode, the scheduler uses direct dispatch and no threads, trying to nest task calls within task calls */
	GF_FS_SCHEDULER_DIRECT
} GF_FilterSchedulerType;

/*! Flag set to indicate meta filters should be loaded. A meta filter is a filter providing various subfilters.
The subfilters are usually not exposed as filters, only the parent one is.
When set, all subfilters are exposed. This should only be set when inspecting filters help*/
#define GF_FS_FLAG_LOAD_META	1<<1
/*! Flag set to disable the blocking mode of the filter session. The default is a semi-blocking mode, cf \ref gf_filter_pid_would_block*/
#define GF_FS_FLAG_NO_BLOCKING	1<<2
/*! Flag set to disable internal caching of filter graph connections. If disabled, the graph will be recomputed at each link resolution (less memory occupancy but slower)*/
#define GF_FS_FLAG_NO_GRAPH_CACHE	1<<3
/*! Flag set to disable main thread. Such sessions shall be run using \ref gf_fs_run_step*/
#define GF_FS_FLAG_NO_MAIN_THREAD	1<<4
/*! Flag set to disable session regulation (no sleep)*/
#define GF_FS_FLAG_NO_REGULATION	1<<5
/*! Flag set to disable data probe*/
#define GF_FS_FLAG_NO_PROBE	(1<<6)
/*! Flag set to disable source reassignment (e.g. switching from fin to ffdmx) in PID resolution*/
#define GF_FS_FLAG_NO_REASSIGN	(1<<7)
/*! Flag set to print enabled/disabled edges for debug of PID resolution*/
#define GF_FS_FLAG_PRINT_CONNECTIONS	(1<<8)
/*! Flag set to disable argument checking*/
#define GF_FS_FLAG_NO_ARG_CHECK	(1<<9)
/*disables reservoir for packets and properties, uses much less memory but much more alloc/free*/
#define GF_FS_FLAG_NO_RESERVOIR (1<<10)

/*! Creates a new filter session. This will also load all available filter registers not blacklisted.
\param nb_threads number of extra threads to allocate. A negative value means all core used by session (eg nb_cores-1 extra threads)
\param type scheduler type
\param flags set of above flags for the session. Modes set by flags cannot be changed at runtime
\param blacklist string containing comma-separated names of filters to disable
\return the created filter session
*/
GF_FilterSession *gf_fs_new(s32 nb_threads, GF_FilterSchedulerType type, u32 flags, const char *blacklist);

/*! Creates a new filter session, loading parameters from gpac config. This will also load all available filter registers not blacklisted.
\param flags set of flags for the session. Only \ref GF_FS_FLAG_LOAD_META and  \ref GF_FS_FLAG_NO_MAIN_THREAD are used, other flags 
\return the created filter session
*/
GF_FilterSession *gf_fs_new_defaults(u32 flags);

/*! Destructs the filter session
\param session the filter session to destruct
*/
void gf_fs_del(GF_FilterSession *session);
/*! Loads a given filter by its register name. Filter are created using their register name, with options appended as a list of colon-separated Name=Value pairs.
Value can be omitted for booleans, defaulting to true (eg :noedit). Using '!' before the name negates the result (eg :!moof_first).
Name can be omitted for enumerations (eg :disp=pbo is equivalent to :pbo), provided that filter developers pay attention to not reuse enum names in one filter.

\param session filter session
\param name name and arguments of the filter register to instantiate.
\return created filter or NULL if filter register cannot be found
*/
GF_Filter *gf_fs_load_filter(GF_FilterSession *session, const char *name);

/*! Checks if a filter register exists by name.
\param session filter session
\param name name of the filter register to check.
\return GF_TRUE if a filter register exists with the given name, GF_FALSE otherwise
*/
Bool gf_fs_filter_exists(GF_FilterSession *session, const char *name);

/*! Runs the session
\param session filter session
\return error if any, or GF_EOS. The last errors can be retrieved using \ref gf_fs_get_last_connect_error and \ref gf_fs_get_last_process_error
*/
GF_Err gf_fs_run(GF_FilterSession *session);

/*! The default separator set used*/
#define GF_FS_DEFAULT_SEPS	":=#,!@"

/*! Sets the set of separators to use when parsing args
\param session filter session
\param separator_set filter session.
The first char is used to separate argument names - default is ':'
The second char, if present, is used to separate names and values - default is '='
The third char, if present, is used to separate fragments for PID sources - default is '#'
The fourth char, if present, is used for list separators (sourceIDs, gfreg, ...) - default is ','
The fifth char, if present, is used for boolean negation - default is '!'
The sixth char, if present, is used for LINK directives - default is '@'
\return error if any
*/
GF_Err gf_fs_set_separators(GF_FilterSession *session, const char *separator_set);

/*! Sets the maximum length of a filter chain dynamically loaded to solve connection between two filters
\param session filter session
\param  max_chain_length sets maximum chain length when resolving filter links.
Default value is 6 ([ in -> ] demux -> reframe -> decode -> encode -> reframe -> mux [ -> out ])
(filter chains loaded for adaptation (eg pixel format change, audio resample) are loaded after the link resolution)
Setting the value to 0 disables dynamic link resolution. You will have to specify the entire chain manually
\return error if any
*/
GF_Err gf_fs_set_max_resolution_chain_length(GF_FilterSession *session, u32 max_chain_length);

/*! Sets the maximum sleep time when postponing tasks.
\param session filter session
\param  max_sleep maximum sleep time in milliseconds. 0 means yield only.
\return error if any
*/
GF_Err gf_fs_set_max_sleep_time(GF_FilterSession *session, u32 max_sleep);

/*! gets the maximum filter chain lengtG
\param session filter session
\return maximum chain length when resolving filter links.
*/
u32 gf_fs_get_max_resolution_chain_length(GF_FilterSession *session);

/*! Runs session in non blocking mode: process all tasks of oldest scheduled filter, process any pending PID connections and returns.
This can only be used if the flag \ref GF_FS_FLAG_NO_MAIN_THREAD was specified at session creation time
\param session filter session
*/
void gf_fs_run_step(GF_FilterSession *session);

/*! Stops the session, waiting for all additional threads to complete
\param session filter session
\return error if any
*/
GF_Err gf_fs_stop(GF_FilterSession *session);

/*! Gets the number of available filter registries (not blacklisted)
\param session filter session
\return number of filter registries
*/
u32 gf_fs_filters_registers_count(GF_FilterSession *session);

/*! Returns the register at the given index
\param session filter session
\param idx index of register, from 0 to \ref gf_fs_filters_registers_count
\return the register object, or NULL if index is out of bounds
*/
const GF_FilterRegister *gf_fs_get_filter_register(GF_FilterSession *session, u32 idx);

/*! Registers the test filters used for unit tests
\param session filter session
*/
void gf_fs_register_test_filters(GF_FilterSession *session);

/*! Loads a source filter from a URL and arguments
\param session filter session
\param url URL of the source to load. Can be a local file name, a full path (/.., \\...) or a full URL with scheme (eg http://, tcp://)
\param args arguments for the filter, see \ref gf_fs_load_filter
\param parent_url parent URL of the source, or NULL if none
\param err if not NULL, is set to error code if any
\return the filter loaded or NULL if error
*/
GF_Filter *gf_fs_load_source(GF_FilterSession *session, const char *url, const char *args, const char *parent_url, GF_Err *err);

/*! Loads a destination filter from a URL and arguments
\param session filter session
\param url URL of the source to load. Can ba a local file name, a full path (/.., \\...) or a full URL with scheme (eg http://, tcp://)
\param args arguments for the filter, see \ref gf_fs_load_filter
\param parent_url parent URL of the source, or NULL if none
\param err if not NULL, is set to error code if any
\return the filter loaded or NULL if error
*/
GF_Filter *gf_fs_load_destination(GF_FilterSession *session, const char *url, const char *args, const char *parent_url, GF_Err *err);

/*! Returns the last error which happened during a PID connection
\param session filter session
\return the error code if any
*/
GF_Err gf_fs_get_last_connect_error(GF_FilterSession *session);

/*! Returns the last error which happened during a filter process
\param session filter session
\return the error code if any
*/
GF_Err gf_fs_get_last_process_error(GF_FilterSession *session);

/*! Adds a user-defined register to the session
\param session filter session
\param freg filter register to add
*/
void gf_fs_add_filter_register(GF_FilterSession *session, const GF_FilterRegister *freg);

/*! Removes a user-defined register from the session
\param session filter session
\param freg filter register to remove
*/
void gf_fs_remove_filter_register(GF_FilterSession *session, GF_FilterRegister *freg);

/*! Posts a user task to the session
\param session filter session
\param task_execute the callback function for the task. The callback can return GF_TRUE to reschedule the task, in which case the task will be rescheduled
immediately or after reschedule_ms.
\param udta_callback callback user data passed back to the task_execute function
\param log_name log name of the task. If NULL, default is "user_task"
\return the error code if any
*/
GF_Err gf_fs_post_user_task(GF_FilterSession *session, Bool (*task_execute) (GF_FilterSession *fsess, void *callback, u32 *reschedule_ms), void *udta_callback, const char *log_name);

/*! Aborts the session. This can be called within a callback task to stop the session. Do NOT use \ref gf_fs_stop from within a user task callback, this will deadlock the session
\param session filter session
\param do_flush if set to true, sources will be forced into end of stream and all emitted packets will be processed. Otherwise everything is discarded, potentially breaking output files
\return the error code if any
*/
GF_Err gf_fs_abort(GF_FilterSession *session, Bool do_flush);
/*! Checks if the session is processing its last task. This can be called within a callback task to check if this is the last task, in order to avoid rescheduling the task
\param session filter session
\return GF_TRUE if no more task, GF_FALSE otherwise
*/
Bool gf_fs_is_last_task(GF_FilterSession *session);

/*! Checks if a given MIME type is supported as input
\param session filter session
\param mime MIME type to query
\return GF_TRUE if MIME is supported
*/
Bool gf_fs_mime_supported(GF_FilterSession *session, const char *mime);


/*! Sets UI callback event
\param session filter session
\param ui_event_proc the event proc callback function. Its return value depends on the event type, usually 0
\param cbk_udta pointer passed back to callback
*/
void gf_fs_set_ui_callback(GF_FilterSession *session, Bool (*ui_event_proc)(void *opaque, GF_Event *event), void *cbk_udta);

/*! Prints stats to logs using LOG_APP@LOG_INFO
\param session filter session
*/
void gf_fs_print_stats(GF_FilterSession *session);

/*! Prints connections between loaded filters in the session to logs using LOG_APP@LOG_INFO
\param session filter session
*/
void gf_fs_print_connections(GF_FilterSession *session);

/*! Prints all possible connections between filter registries to logs using LOG_APP@LOG_INFO
\param session filter session
\param filter_name if not null, only prints input connection for this filter register
\param print_fn optionnal callback function for print, otherwise print to stderr
*/
void gf_fs_print_all_connections(GF_FilterSession *session, char *filter_name, void (*print_fn)(FILE *output, u32 is_first, const char *fmt, ...) );

/*! Checks the presence of an input capability and an output capability in a target register. The caps are matched only if they belong to the same bundle.
\param filter_reg filter register to check
\param in_cap_code capability code (property type) of input capability to check
\param in_cap capabiility value of input capability to check
\param out_cap_code capability code (property type) of output capability to check
\param out_cap capability value of output capability to check
\return GF_TRUE if filter register has such a match, GF_FALSE otherwise
*/
Bool gf_fs_check_filter_register_cap(const GF_FilterRegister *filter_reg, u32 in_cap_code, GF_PropertyValue *in_cap, u32 out_cap_code, GF_PropertyValue *out_cap);


/*! Enables or disables filter reporting
\param session filter session
\param reporting_on if GF_TRUE, reporting will be enabled
*/
void gf_fs_enable_reporting(GF_FilterSession *session, Bool reporting_on);

/*! Locks global session mutex - mostly used to query filter reports and avoids concurrent destruction of a filter
\param session filter session
\param do_lock if GF_TRUE, session is locked, otherwise session is unlocked
*/
void gf_fs_lock_filters(GF_FilterSession *session, Bool do_lock);

/*! Gets number of active filters in the session
\param session filter session
\return number of active filters
*/
u32 gf_fs_get_filters_count(GF_FilterSession *session);

typedef enum
{
	GF_FS_STATS_FILTER_UNKNOWN,
	GF_FS_STATS_FILTER_RAWIN,
	GF_FS_STATS_FILTER_DEMUX,
	GF_FS_STATS_FILTER_DECODE,
	GF_FS_STATS_FILTER_ENCODE,
	GF_FS_STATS_FILTER_MUX,
	GF_FS_STATS_FILTER_RAWOUT,
} GF_FSFilterType;

typedef struct
{
	/*!number of tasks executed by this filter*/
	u64 nb_tasks_done;
	/*!number of packets processed by this filter*/
	u64 nb_pck_processed;
	/*!number of bytes processed by this filter*/
	u64 nb_bytes_processed;
	/*!number of packets sent by this filter*/
	u64 nb_pck_sent;
	/*!number of hardware frames packets sent by this filter*/
	u64 nb_hw_pck_sent;
	/*!number of processing errors in the lifetime of the filter*/
	u32 nb_errors;

	/*!number of bytes sent by this filter*/
	u64 nb_bytes_sent;
	/*!number of microseconds this filter was active*/
	u64 time_process;
	/*!percentage of data processed (between 0 and 100), otherwise unknown (-1)*/
	s32 percent;
	/*!last status report from filter, null if session reporting is not enabled*/
	const char *status;
	/*!filter name*/
	const char *name;
	/*!filter register name*/
	const char *reg_name;
	/*!set to GF_TRUE if filter is done processing*/
	Bool done;
	/*!number of input PIDs*/
	u32 nb_pid_in;
	/*!number of input packets processed*/
	u64 nb_in_pck;
	/*!number of output PIDs*/
	u32 nb_pid_out;
	/*!number of output packets sent*/
	u64 nb_out_pck;
	/*!set to GF_TRUE if filter has seen end of stream*/
	Bool in_eos;
	/*!set to GF_TRUE if filter has seen end of stream*/
	GF_FSFilterType type;
	/*!set to streamtype of output PID if this is not a demux*/
	u32 stream_type;
	/*!set to codecid of output PID*/
	u32 codecid;
} GF_FilterStats;

/*! Gets number of active filters in the session
\param session filter session
\param idx index of filter to query
\param stats statistics for filter
\return error code if any
*/
GF_Err gf_fs_get_filter_stats(GF_FilterSession *session, u32 idx, GF_FilterStats *stats);


/*! Enumerates filter and meta-filter arguments not matched in the session
\param session filter session
\param idx index of argument to query, 0 being first argument; this value is automatically incremented
\param argname set to argument name
\param argtype set to argument type: 0 was a filter param (eg:arg=val), 1 was a global arg (eg --arg=val) and 2 was a global meta arg (eg -+arg=val)
\return GF_TRUE if success, GF_FALSE if nothing more to enumerate
*/
Bool gf_fs_enum_unmapped_options(GF_FilterSession *session, u32 *idx, char **argname, u32 *argtype);

/*! @} */


/*!
 *\addtogroup fs_props Filter Properties
 *\ingroup filters_grp
 *\brief PID and filter properties
 *
 *Documents the property object used for PID and packets.
 *	@{
 */

/*! Property types*/
typedef enum
{
	/*! not allowed*/
	GF_PROP_FORBIDEN=0,
	/*! signed 32 bit integer*/
	GF_PROP_SINT,
	/*! unsigned 32 bit integer*/
	GF_PROP_UINT,
	/*! signed 64 bit integer*/
	GF_PROP_LSINT,
	/*! unsigned 64 bit integer*/
	GF_PROP_LUINT,
	/*! boolean*/
	GF_PROP_BOOL,
	/*! 32 bit / 32 bit fraction*/
	GF_PROP_FRACTION,
	/*! 64 bit / 64 bit fraction*/
	GF_PROP_FRACTION64,
	/*! float (Fixed) number*/
	GF_PROP_FLOAT,
	/*! double number*/
	GF_PROP_DOUBLE,
	/*! 2D signed integer vector*/
	GF_PROP_VEC2I,
	/*! 2D double number vector*/
	GF_PROP_VEC2,
	/*! 3D signed integer vector*/
	GF_PROP_VEC3I,
	/*! 3D double number vector*/
	GF_PROP_VEC3,
	/*! 4D signed integer vector*/
	GF_PROP_VEC4I,
	/*! 4D double number vector*/
	GF_PROP_VEC4,
	/*! Video Pixel format*/
	GF_PROP_PIXFMT,
	/*! Audio PCM format*/
	GF_PROP_PCMFMT,
	/*! string property, memory is duplicated when setting the property and managed internally*/
	GF_PROP_STRING,
	/*! string property, memory is NOT duplicated when setting the property but is then managed (and free) internally.
	Only used when setting a property, the type then defaults to GF_PROP_STRING*/
	GF_PROP_STRING_NO_COPY,
	/*! data property, memory is duplicated when setting the property and managed internally*/
	GF_PROP_DATA,
	/*! const string property, memory is NOT duplicated when setting the property, stays user-managed*/
	GF_PROP_NAME,
	/*! data property, memory is NOT duplicated when setting the property but is then managed (and free) internally.
	Only used when setting a property, the type then defaults to GF_PROP_DATA*/
	GF_PROP_DATA_NO_COPY,
	/*! const data property, memory is NOT duplicated when setting the property, stays user-managed*/
	GF_PROP_CONST_DATA,
	/*! user-managed pointer*/
	GF_PROP_POINTER,
	/*! string list, memory is NOT duplicated when setting the property, the GF_List * of
	the passed property is directly assigned to the new property and will be and managed internally (freed by the filter session)*/
	GF_PROP_STRING_LIST,
	/*! unsigned 32 bit integer list, memory is ALWAYS duplicated when setting the property*/
	GF_PROP_UINT_LIST,

	/*! not allowed*/
	GF_PROP_LAST_DEFINED,
} GF_PropType;

/*! Data property*/
typedef struct
{
	/*! data pointer */
	u8 *ptr;
	/*! data size */
	u32 size;
} GF_PropData;

/*! List of unsigned int property*/
typedef struct
{
	/*! array of unsigned integers */
	u32 *vals;
	/*! number of items in array */
	u32 nb_items;
} GF_PropUIntList;

/*! 2D signed integer vector property*/
typedef struct
{
	/*! x coord */
	s32 x;
	/*! y coord */
	s32 y;
} GF_PropVec2i;

/*! 2D double number vector property*/
typedef struct
{
	/*! x coord */
	Double x;
	/*! y coord */
	Double y;
} GF_PropVec2;

/*! 3D signed integer vector property*/
typedef struct
{
	/*! x coord */
	s32 x;
	/*! y coord */
	s32 y;
	/*! z coord */
	s32 z;
} GF_PropVec3i;

/*! 3D double number vector property*/
typedef struct
{
	/*! x coord */
	Double x;
	/*! y coord */
	Double y;
	/*! z coord */
	Double z;
} GF_PropVec3;

/*! 4D signed integer vector property*/
typedef struct
{
	/*! x coord */
	s32 x;
	/*! y coord */
	s32 y;
	/*! z coord */
	s32 z;
	/*! w coord */
	s32 w;
} GF_PropVec4i;

/*! 4D double number vector property*/
typedef struct
{
	/*! x coord */
	Double x;
	/*! y coord */
	Double y;
	/*! z coord */
	Double z;
	/*! w coord */
	Double w;
} GF_PropVec4;

/*! Property value used by PIDs and packets*/
struct __gf_prop_val
{
	/*! type of the property */
	GF_PropType type;
	/*! union of all possible property data types */
	union {
		/*! 64 bit unsigned integer value of property */
		u64 longuint;
		/*! 64 bit signed integer value of property */
		s64 longsint;
		/*! 32 bit signed integer value of property */
		s32 sint;
		/*! 32 bit unsigned integer value of property */
		u32 uint;
		/*! boolean value of property */
		Bool boolean;
		/*! fraction (32/32 bits) value of property */
		GF_Fraction frac;
		/*! fraction (64/64 bits) value of property */
		GF_Fraction64 lfrac;
		/*! fixed number (float or 32 bit depending on compilation settings) value of property */
		Fixed fnumber;
		/*! double value of property */
		Double number;
		/*! 2D signed integer vector value of property */
		GF_PropVec2i vec2i;
		/*! 2D double vector value of property */
		GF_PropVec2 vec2;
		/*! 3D signed integer vector value of property */
		GF_PropVec3i vec3i;
		/*! 3D double vector value of property */
		GF_PropVec3 vec3;
		/*! 4D signed integer vector value of property */
		GF_PropVec4i vec4i;
		/*! 4D double vector value of property */
		GF_PropVec4 vec4;
		/*! data value of property. For non const data type, the memory is freed by filter session.
		Otherwise caller is responsible to free it at end of filter/session*/
		GF_PropData data;
		/*! string value of property. For non const string / names, the memory is freed by filter session, otherwise handled as const char *. */
		char *string;
		/*! pointer value of property */
		void *ptr;
		/*! string list value of property - memory is handled by filter session (always copy)*/
		GF_List *string_list;
		/*! unsigned integer list value of property - memory is handled by filter session (always copy)*/
		GF_PropUIntList uint_list;
	} value;
};

/*! Playback mode type supported on PID*/
typedef enum
{
	/*! simplest playback mode, can play from 0 at speed=1 only*/
	GF_PLAYBACK_MODE_NONE=0,
	/*! seek playback mode, can play from any position at speed=1 only*/
	GF_PLAYBACK_MODE_SEEK,
	/*! fast forward playback mode, can play from any position at speed=N only, with N>=0*/
	GF_PLAYBACK_MODE_FASTFORWARD,
	/*! rewind playback mode, can play from any position at speed=N, N positive or negative*/
	GF_PLAYBACK_MODE_REWIND
} GF_FilterPidPlaybackMode;

/*! Built-in property types
See gpac help (gpac -h props) for codes, types, formats and and meaning
	\hideinitializer
*/
enum
{
	GF_PROP_PID_ID = GF_4CC('P','I','D','I'),
	GF_PROP_PID_ESID = GF_4CC('E','S','I','D'),
	GF_PROP_PID_ITEM_ID = GF_4CC('I','T','I','D'),
	GF_PROP_PID_SERVICE_ID = GF_4CC('P','S','I','D'),
	GF_PROP_PID_CLOCK_ID = GF_4CC('C','K','I','D'),
	GF_PROP_PID_DEPENDENCY_ID = GF_4CC('D','P','I','D'),
	GF_PROP_PID_SUBLAYER = GF_4CC('D','P','S','L'),
	GF_PROP_PID_PLAYBACK_MODE = GF_4CC('P','B','K','M'),
	GF_PROP_PID_SCALABLE = GF_4CC('S','C','A','L'),
	GF_PROP_PID_TILE_BASE = GF_4CC('S','A','B','T'),
	GF_PROP_PID_LANGUAGE = GF_4CC('L','A','N','G'),
	GF_PROP_PID_SERVICE_NAME = GF_4CC('S','N','A','M'),
	GF_PROP_PID_SERVICE_PROVIDER = GF_4CC('S','P','R','O'),
	GF_PROP_PID_STREAM_TYPE = GF_4CC('P','M','S','T'),
	GF_PROP_PID_SUBTYPE = GF_4CC('P','S','S','T'),
	GF_PROP_PID_ISOM_SUBTYPE = GF_4CC('P','I','S','T'),
	GF_PROP_PID_ORIG_STREAM_TYPE = GF_4CC('P','O','S','T'),
	GF_PROP_PID_CODECID = GF_4CC('P','O','T','I'),
	GF_PROP_PID_IN_IOD = GF_4CC('P','I','O','D'),
	GF_PROP_PID_UNFRAMED = GF_4CC('P','F','R','M'),
	GF_PROP_PID_UNFRAMED_FULL_AU = GF_4CC('P','F','R','F'),
	GF_PROP_PID_DURATION = GF_4CC('P','D','U','R'),
	GF_PROP_PID_NB_FRAMES = GF_4CC('N','F','R','M'),
	GF_PROP_PID_FRAME_SIZE = GF_4CC('C','F','R','S'),
	GF_PROP_PID_TIMESHIFT_DEPTH = GF_4CC('P','T','S','D'),
	GF_PROP_PID_TIMESHIFT_TIME = GF_4CC('P','T','S','T'),
	GF_PROP_PID_TIMESHIFT_STATE = GF_4CC('P','T','S','S'),
	GF_PROP_PID_TIMESCALE = GF_4CC('T','I','M','S'),
	GF_PROP_PID_PROFILE_LEVEL = GF_4CC('P','R','P','L'),
	GF_PROP_PID_DECODER_CONFIG = GF_4CC('D','C','F','G'),
	GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT = GF_4CC('E','C','F','G'),
	GF_PROP_PID_CONFIG_IDX =  GF_4CC('I','C','F','G'),
	GF_PROP_PID_SAMPLE_RATE = GF_4CC('A','U','S','R'),
	GF_PROP_PID_SAMPLES_PER_FRAME = GF_4CC('F','R','M','S'),
	GF_PROP_PID_NUM_CHANNELS = GF_4CC('C','H','N','B'),
	GF_PROP_PID_AUDIO_BPS = GF_4CC('A','B','P','S'),
	GF_PROP_PID_CHANNEL_LAYOUT = GF_4CC('C','H','L','O'),
	GF_PROP_PID_AUDIO_FORMAT = GF_4CC('A','F','M','T'),
	GF_PROP_PID_AUDIO_SPEED = GF_4CC('A','S','P','D'),
	GF_PROP_PID_UNFRAMED_LATM = GF_4CC('L','A','T','M'),
	GF_PROP_PID_DELAY = GF_4CC('M','D','L','Y'),
	GF_PROP_PID_CTS_SHIFT = GF_4CC('M','D','T','S'),
	GF_PROP_PID_WIDTH = GF_4CC('W','I','D','T'),
	GF_PROP_PID_HEIGHT = GF_4CC('H','E','I','G'),
	GF_PROP_PID_PIXFMT = GF_4CC('P','F','M','T'),
	GF_PROP_PID_STRIDE = GF_4CC('V','S','T','Y'),
	GF_PROP_PID_STRIDE_UV = GF_4CC('V','S','T','C'),
	GF_PROP_PID_BIT_DEPTH_Y = GF_4CC('Y','B','P','S'),
	GF_PROP_PID_BIT_DEPTH_UV = GF_4CC('C','B','P','S'),
	GF_PROP_PID_FPS = GF_4CC('V','F','P','F'),
	GF_PROP_PID_INTERLACED = GF_4CC('V','I','L','C'),
	GF_PROP_PID_SAR = GF_4CC('P','S','A','R'),
	GF_PROP_PID_PAR = GF_4CC('V','P','A','R'),
	GF_PROP_PID_WIDTH_MAX = GF_4CC('M', 'W','I','D'),
	GF_PROP_PID_HEIGHT_MAX = GF_4CC('M', 'H','E','I'),
	GF_PROP_PID_ZORDER = GF_4CC('V', 'Z','I','X'),
	GF_PROP_PID_TRANS_X = GF_4CC('V','T','R','X'),
	GF_PROP_PID_TRANS_Y = GF_4CC('V','T','R','Y'),
	GF_PROP_PID_HIDDEN = GF_4CC('H','I','D','E'),
	GF_PROP_PID_CROP_POS = GF_4CC('V','C','X','Y'),
	GF_PROP_PID_ORIG_SIZE = GF_4CC('V','O','W','H'),
	GF_PROP_PID_SRD = GF_4CC('S','R','D',' '),
	GF_PROP_PID_SRD_REF = GF_4CC('S','R','D','R'),
	GF_PROP_PID_ALPHA = GF_4CC('V','A','L','P'),
	GF_PROP_PID_BITRATE = GF_4CC('R','A','T','E'),
	GF_PROP_PID_MAXRATE = GF_4CC('M','R','A','T'),
	GF_PROP_PID_DBSIZE = GF_4CC('D','B','S','Z'),
	GF_PROP_PID_MEDIA_DATA_SIZE = GF_4CC('M','D','S','Z'),
	GF_PROP_PID_CAN_DATAREF = GF_4CC('D','R','E','F'),
	GF_PROP_PID_URL = GF_4CC('F','U','R','L'),
	GF_PROP_PID_REMOTE_URL = GF_4CC('R','U','R','L'),
	GF_PROP_PID_REDIRECT_URL = GF_4CC('R','E','L','O'),
	GF_PROP_PID_FILEPATH = GF_4CC('F','S','R','C'),
	GF_PROP_PID_MIME = GF_4CC('M','I','M','E'),
	GF_PROP_PID_FILE_EXT = GF_4CC('F','E','X','T'),
	GF_PROP_PID_OUTPATH = GF_4CC('F','D','S','T'),
	GF_PROP_PID_FILE_CACHED = GF_4CC('C','A','C','H'),
	GF_PROP_PID_DOWN_RATE = GF_4CC('D','L','B','W'),
	GF_PROP_PID_DOWN_SIZE = GF_4CC('D','L','S','Z'),
	GF_PROP_PID_DOWN_BYTES = GF_4CC('D','L','B','D'),
	GF_PROP_PID_FILE_RANGE = GF_4CC('F','B','R','A'),
	GF_PROP_PID_DISABLE_PROGRESSIVE = GF_4CC('N','P','R','G'),
	GF_PROP_PID_ISOM_BRANDS = GF_4CC('A','B','R','D'),
	GF_PROP_PID_ISOM_MBRAND = GF_4CC('M','B','R','D'),
	GF_PROP_SERVICE_WIDTH = GF_4CC('D','W','D','T'),
	GF_PROP_SERVICE_HEIGHT = GF_4CC('D','H','G','T'),
	GF_PROP_PID_CAROUSEL_RATE = GF_4CC('C','A','R','A'),
	GF_PROP_PID_UTC_TIME = GF_4CC('U','T','C','D'),
	GF_PROP_PID_UTC_TIMESTAMP = GF_4CC('U','T','C','T'),
	GF_PROP_PID_AUDIO_VOLUME = GF_4CC('A','V','O','L'),
	GF_PROP_PID_AUDIO_PAN = GF_4CC('A','P','A','N'),
	GF_PROP_PID_AUDIO_PRIORITY = GF_4CC('A','P','R','I'),
	GF_PROP_PID_PROTECTION_SCHEME_TYPE = GF_4CC('S','C','H','T'),
	GF_PROP_PID_PROTECTION_SCHEME_VERSION = GF_4CC('S','C','H','V'),
	GF_PROP_PID_PROTECTION_SCHEME_URI = GF_4CC('S','C','H','U'),
	GF_PROP_PID_PROTECTION_KMS_URI = GF_4CC('K','M','S','U'),
	GF_PROP_PID_ISMA_SELECTIVE_ENC = GF_4CC('I','S','S','E'),
	GF_PROP_PID_ISMA_IV_LENGTH = GF_4CC('I','S','I','V'),
	GF_PROP_PID_ISMA_KI_LENGTH = GF_4CC('I','S','K','I'),
	GF_PROP_PID_ISMA_KI = GF_4CC('I','K','E','Y'),
	GF_PROP_PID_OMA_CRYPT_TYPE = GF_4CC('O','M','C','T'),
	GF_PROP_PID_OMA_CID = GF_4CC('O','M','I','D'),
	GF_PROP_PID_OMA_TXT_HDR = GF_4CC('O','M','T','H'),
	GF_PROP_PID_OMA_CLEAR_LEN = GF_4CC('O','M','P','T'),
	GF_PROP_PID_CRYPT_INFO = GF_4CC('E','C','R','I'),
	GF_PROP_PID_DECRYPT_INFO = GF_4CC('E','D','R','I'),
	GF_PROP_PCK_SENDER_NTP = GF_4CC('N','T','P','S'),
	GF_PROP_PID_ADOBE_CRYPT_META = GF_4CC('A','M','E','T'),
	GF_PROP_PID_ENCRYPTED = GF_4CC('E','P','C','K'),
	GF_PROP_PID_OMA_PREVIEW_RANGE = GF_4CC('O','D','P','R'),
	GF_PROP_PID_CENC_PSSH = GF_4CC('P','S','S','H'),
	GF_PROP_PCK_CENC_SAI = GF_4CC('S','A','I','S'),
	GF_PROP_PID_KID = GF_4CC('S','K','I','D'),
	GF_PROP_PID_CENC_IV_SIZE = GF_4CC('S','A','I','V'),
	GF_PROP_PID_CENC_IV_CONST = GF_4CC('C','B','I','V'),
	GF_PROP_PID_CENC_PATTERN = GF_4CC('C','P','T','R'),
	GF_PROP_PID_CENC_STORE = GF_4CC('C','S','T','R'),
	GF_PROP_PID_AMR_MODE_SET = GF_4CC('A','M','S','T'),
	GF_PROP_PCK_SUBS = GF_4CC('S','U','B','S'),
	GF_PROP_PID_MAX_NALU_SIZE = GF_4CC('N','A','L','S'),
	GF_PROP_PCK_FILENUM = GF_4CC('F','N','U','M'),
	GF_PROP_PCK_FILENAME = GF_4CC('F','N','A','M'),
	GF_PROP_PCK_IDXFILENAME = GF_4CC('I','N','A','M'),
	GF_PROP_PCK_EODS = GF_4CC('E','O','D','S'),
	GF_PROP_PID_MAX_FRAME_SIZE = GF_4CC('M','F','R','S'),
	GF_PROP_PID_AVG_FRAME_SIZE = GF_4CC('A','F','R','S'),
	GF_PROP_PID_MAX_TS_DELTA = GF_4CC('M','T','S','D'),
	GF_PROP_PID_MAX_CTS_OFFSET = GF_4CC('M','C','T','O'),
	GF_PROP_PID_CONSTANT_DURATION = GF_4CC('S','C','T','D'),
	GF_PROP_PID_ISOM_TRACK_TEMPLATE = GF_4CC('I','T','K','T'),
	GF_PROP_PID_ISOM_TREX_TEMPLATE = GF_4CC('I','T','X','T'),
	GF_PROP_PID_ISOM_STSD_TEMPLATE = GF_4CC('I','S','T','D'),
	GF_PROP_PID_ISOM_UDTA = GF_4CC('I','M','U','D'),
	GF_PROP_PID_ISOM_HANDLER = GF_4CC('I','H','D','L'),
	GF_PROP_PID_ISOM_TRACK_FLAGS = GF_4CC('I','T','K','F'),
	GF_PROP_PID_ISOM_TRACK_MATRIX = GF_4CC('I','T','K','M'),
	GF_PROP_PID_PERIOD_ID = GF_4CC('P','E','I','D'),
	GF_PROP_PID_PERIOD_START = GF_4CC('P','E','S','T'),
	GF_PROP_PID_PERIOD_DUR = GF_4CC('P','E','D','U'),
	GF_PROP_PID_REP_ID = GF_4CC('D','R','I','D'),
	GF_PROP_PID_AS_ID = GF_4CC('D','A','I','D'),
	GF_PROP_PID_MUX_SRC = GF_4CC('M','S','R','C'),
	GF_PROP_PID_DASH_MODE = GF_4CC('D','M','O','D'),
	GF_PROP_PID_DASH_DUR = GF_4CC('D','D','U','R'),
	GF_PROP_PID_DASH_MULTI_PID = GF_4CC('D','M','S','D'),
	GF_PROP_PID_DASH_MULTI_PID_IDX = GF_4CC('D','M','S','I'),
	GF_PROP_PID_DASH_MULTI_TRACK = GF_4CC('D','M','T','K'),
	GF_PROP_PID_ROLE = GF_4CC('R','O','L','E'),
	GF_PROP_PID_PERIOD_DESC = GF_4CC('P','D','E','S'),
	GF_PROP_PID_AS_COND_DESC = GF_4CC('A','C','D','S'),
	GF_PROP_PID_AS_ANY_DESC = GF_4CC('A','A','D','S'),
	GF_PROP_PID_REP_DESC = GF_4CC('R','D','E','S'),
	GF_PROP_PID_BASE_URL = GF_4CC('B','U','R','L'),
	GF_PROP_PID_TEMPLATE = GF_4CC('D','T','P','L'),
	GF_PROP_PID_START_NUMBER = GF_4CC('D','R','S','N'),
	GF_PROP_PID_XLINK = GF_4CC('X','L','N','K'),
	GF_PROP_PID_CLAMP_DUR = GF_4CC('D','C','M','D'),
	GF_PROP_PID_HLS_PLAYLIST = GF_4CC('H','L','V','P'),
	GF_PROP_PID_DASH_CUE = GF_4CC('D','C','U','E'),
	GF_PROP_PID_SINGLE_SCALE = GF_4CC('D','S','T','S'),
	GF_PROP_PID_UDP = GF_4CC('P','U','D','P'),

	GF_PROP_PID_PRIMARY_ITEM = GF_4CC('P','I','T','M'),

	GF_PROP_PID_COLR_PRIMARIES = GF_4CC('C','P','R','M'),
	GF_PROP_PID_COLR_TRANSFER = GF_4CC('C','T','R','C'),
	GF_PROP_PID_COLR_MX = GF_4CC('C','M','X','C'),
	GF_PROP_PID_COLR_RANGE = GF_4CC('C','F','R','A'),

	GF_PROP_PCK_FRAG_START = GF_4CC('P','F','R','B'),
	GF_PROP_PCK_MOOF_TEMPLATE = GF_4CC('M','F','T','P'),

};

/*! Gets readable name of built-in property
\param prop_4cc property built-in 4cc
\return readable name
*/
const char *gf_props_4cc_get_name(u32 prop_4cc);

/*! Gets property type of built-in property
\param prop_4cc property built-in 4cc
\return property name
*/
u32 gf_props_4cc_get_type(u32 prop_4cc);

/*! Checks if two properties are equal
\param p1 first property to compare
\param p2 second property to compare
\return GF_TRUE if properties are equal, GF_FALSE otherwise
*/
Bool gf_props_equal(const GF_PropertyValue *p1, const GF_PropertyValue *p2);

/*! Gets the readable name for a property type
\param type property type
\return readable name
*/
const char *gf_props_get_type_name(u32 type);

/*! Parses a property value from string
\param type property type to parse
\param name property name to parse (for logs)
\param value string containing the value to parse
\param enum_values string containig enum_values, or NULL. enum_values are used for unsigned int properties, take the form "a|b|c" and resolve to 0|1|2.
\param list_sep_char value of the list seperator character to use
\return the parsed property value
*/
GF_PropertyValue gf_props_parse_value(u32 type, const char *name, const char *value, const char *enum_values, char list_sep_char);

/*! Maximum string size to use when dumping a property*/
#define GF_PROP_DUMP_ARG_SIZE	100

/*! Dumps a property value to string
\param att property value
\param dump buffer holding the resulting value for types requiring string conversions (integers, ...)
\param dump_data if set data will be dumped in hexadecimal. Otherwise, data buffer is not dumped
\param min_max_enum optionnal, gives the min/max or enum string when the property is a filter argument
\return string
*/
const char *gf_prop_dump_val(const GF_PropertyValue *att, char dump[GF_PROP_DUMP_ARG_SIZE], Bool dump_data, const char *min_max_enum);

/*! Dumps a property value to string, resolving any built-in types (pix formats, codec id, ...)
\param p4cc property 4CC
\param att property value
\param dump buffer holding the resulting value for types requiring string conversions (integers, ...)
\param dump_data if set data will be dumped in hexadecimal. Otherwise, data buffer is not dumped
\return string
*/
const char *gf_prop_dump(u32 p4cc, const GF_PropertyValue *att, char dump[GF_PROP_DUMP_ARG_SIZE], Bool dump_data);

/*! Property aplies only to packets */
#define GF_PROP_FLAG_PCK 1
/*! Property is optional for GPAC GSF serialization (not transmitted over network when property removal is enabled) */
#define GF_PROP_FLAG_GSF_REM 1<<1

/*! Structure describing a built-in property in GPAC*/
typedef struct {
	/*! identifier (4CC) */
	u32 type;
	/*! name */
	const char *name;
	/*! description */
	const char *description;
	/*! data type  (uint, float, etc ..) */
	u8 data_type;
	/*! flags for the property */
	u8 flags;
} GF_BuiltInProperty;

/*! Gets property description
\param prop_idx built-in property index, starting from 0
\return associated property description or NULL if property was not found
*/
const GF_BuiltInProperty *gf_props_get_description(u32 prop_idx);

/*! Gets built-in property 4CC from name
\param name built-in property name
\return built-in property 4CC or 0 if not found
*/
u32 gf_props_get_id(const char *name);

/*! Gets flags of built-in property
\param prop_4cc built-in property 4CC
\return built-in property flags, 0 if not found
*/
u8 gf_props_4cc_get_flags(u32 prop_4cc);

/*! Helper macro to set signed int properties */
#define PROP_SINT(_val) (GF_PropertyValue){.type=GF_PROP_SINT, .value.sint = _val}
/*! Helper macro to set unsigned int properties */
#define PROP_UINT(_val) (GF_PropertyValue){.type=GF_PROP_UINT, .value.uint = _val}
#define PROP_LONGSINT(_val) (GF_PropertyValue){.type=GF_PROP_LSINT, .value.longsint = _val}
#define PROP_LONGUINT(_val) (GF_PropertyValue){.type=GF_PROP_LUINT, .value.longuint = _val}
#define PROP_BOOL(_val) (GF_PropertyValue){.type=GF_PROP_BOOL, .value.boolean = _val}
#define PROP_FIXED(_val) (GF_PropertyValue){.type=GF_PROP_FLOAT, .value.fnumber = _val}
#define PROP_FLOAT(_val) (GF_PropertyValue){.type=GF_PROP_FLOAT, .value.fnumber = FLT2FIX(_val)}
#define PROP_FRAC_INT(_num, _den) (GF_PropertyValue){.type=GF_PROP_FRACTION, .value.frac.num = _num, .value.frac.den = _den}
#define PROP_FRAC(_val) (GF_PropertyValue){.type=GF_PROP_FRACTION, .value.frac = _val }
#define PROP_FRAC64(_val) (GF_PropertyValue){.type=GF_PROP_FRACTION64, .value.lfrac = _val}
#define PROP_FRAC64_INT(_num, _den) (GF_PropertyValue){.type=GF_PROP_FRACTION64, .value.lfrac.num = _num, .value.lfrac.den = _den}
#define PROP_DOUBLE(_val) (GF_PropertyValue){.type=GF_PROP_DOUBLE, .value.number = _val}
#define PROP_STRING(_val) (GF_PropertyValue){.type=GF_PROP_STRING, .value.string = (char *) _val}
#define PROP_STRING_NO_COPY(_val) (GF_PropertyValue){.type=GF_PROP_STRING_NO_COPY, .value.string = _val}
#define PROP_NAME(_val) (GF_PropertyValue){.type=GF_PROP_NAME, .value.string = _val}
#define PROP_DATA(_val, _len) (GF_PropertyValue){.type=GF_PROP_DATA, .value.data.ptr = _val, .value.data.size=_len}
#define PROP_DATA_NO_COPY(_val, _len) (GF_PropertyValue){.type=GF_PROP_DATA_NO_COPY, .value.data.ptr = _val, .value.data.size =_len}
#define PROP_CONST_DATA(_val, _len) (GF_PropertyValue){.type=GF_PROP_CONST_DATA, .value.data.ptr = _val, .value.data.size = _len}
#define PROP_VEC2(_val) (GF_PropertyValue){.type=GF_PROP_VEC2, .value.vec2 = _val}
#define PROP_VEC2I(_val) (GF_PropertyValue){.type=GF_PROP_VEC2I, .value.vec2i = _val}
#define PROP_VEC2I_INT(_x, _y) (GF_PropertyValue){.type=GF_PROP_VEC2I, .value.vec2i.x = _x, .value.vec2i.y = _y}
#define PROP_VEC3(_val) (GF_PropertyValue){.type=GF_PROP_VEC3, .value.vec3 = _val}
#define PROP_VEC3I(_val) (GF_PropertyValue){.type=GF_PROP_VEC3I, .value.vec3i = _val}
#define PROP_VEC4(_val) (GF_PropertyValue){.type=GF_PROP_VEC4, .value.vec4 = _val}
#define PROP_VEC4I(_val) (GF_PropertyValue){.type=GF_PROP_VEC4I, .value.vec4i = _val}
#define PROP_VEC4I_INT(_x, _y, _z, _w) (GF_PropertyValue){.type=GF_PROP_VEC4I, .value.vec4i.x = _x, .value.vec4i.y = _y, .value.vec4i.z = _z, .value.vec4i.w = _w}
#define PROP_POINTER(_val) (GF_PropertyValue){.type=GF_PROP_POINTER, .value.ptr = (void*)_val}


/*! @} */




/*!
 *\addtogroup fs_evt Filter Events
 *\ingroup filters_grp
 *\brief Filter Events

PIDs may receive commands and may emit messages using this system.

Events may flow downwards (towards the source), in which case they are commands, or upwards (towards the sink), in which case they are informative event.

A filter not implementing a process_event will result in the event being forwarded down to all input PIDs or up to all output PIDs.

A filter may decide to cancel an event, in which case the event is no longer forwarded down/up the chain.

GF_FEVT_PLAY, GF_FEVT_STOP and GF_FEVT_SOURCE_SEEK events will trigger a reset of PID buffers.

A GF_FEVT_PLAY event on a PID already playing is discarded.

A GF_FEVT_STOP event on a PID already stopped is discarded.

GF_FEVT_PLAY and GF_FEVT_SET_SPEED events will trigger larger (abs(speed)>1) or smaller (abs(speed)<1) PID buffer limit in blocking mode.

GF_FEVT_STOP and GF_FEVT_SOURCE_SEEK events are filtered to reset the PID buffers.

 *	@{
 */

/*! Filter event types */
typedef enum
{
	/*! PID control, usually triggered by sink - see \ref GF_FilterPidPlaybackMode*/
	GF_FEVT_PLAY = 1,
	/*! PID speed control, usually triggered by sink - see \ref GF_FilterPidPlaybackMode*/
	GF_FEVT_SET_SPEED,
	/*! PID control, usually triggered by sink - see \ref GF_FilterPidPlaybackMode*/
	GF_FEVT_STOP,
	/*! PID pause, usually triggered by sink - see \ref GF_FilterPidPlaybackMode*/
	GF_FEVT_PAUSE,
	/*! PID resume, usually triggered by sink - see \ref GF_FilterPidPlaybackMode*/
	GF_FEVT_RESUME,
	/*! PID source seek, allows seeking in bytes the source*/
	GF_FEVT_SOURCE_SEEK,
	/*! PID source switch, allows a source filter to switch its source URL for the same protocol*/
	GF_FEVT_SOURCE_SWITCH,
	/*! DASH segment size info, sent down from muxers to manifest generators*/
	GF_FEVT_SEGMENT_SIZE,
	/*! Scene attach event, sent down from compositor to filters (BIFS/OD/timedtext/any scene-related) to share the scene (resources and node graph)*/
	GF_FEVT_ATTACH_SCENE,
	/*! Scene reset event, sent down from compositor to filters (BIFS/OD/timedtext/any scene-related) to indicate scene reset (resources and node graph). This is a direct filter call, only sent processed by filters running on the main thread*/
	GF_FEVT_RESET_SCENE,
	/*! quality switching request event, helps filters decide how to adapt their processing*/
	GF_FEVT_QUALITY_SWITCH,
	/*! visibility hint event, helps filters decide how to adapt their processing*/
	GF_FEVT_VISIBILITY_HINT,
	/*! special event type sent to a filter whenever the PID info properties have been modified. No cancel because no forward - cf \ref gf_filter_pid_set_info. A filter returning GF_TRUE on this event will prevent info update notification on the destination filters*/
	GF_FEVT_INFO_UPDATE,
	/*! buffer requirement event. This event is NOT sent to filters, it is internaly processed by the filter session. Filters may however send this
	event to indicate their buffereing preference (real-time sinks mostly)*/
	GF_FEVT_BUFFER_REQ,
	/*! filter session capability change, sent whenever global capabilities (max width, max heoght, ... ) are changed*/
	GF_FEVT_CAPS_CHANGE,
	/*! inidicates the PID could not be connected - the PID passed is an output PID of the filter, no specific event structure is associated*/
	GF_FEVT_CONNECT_FAIL,
	/*! mouse move event, sent from compositor down to filters*/
	GF_FEVT_MOUSE,
} GF_FEventType;

/*! type: the type of the event*/
/*! on_pid: PID to which the event is targeted. If NULL the event is targeted at the whole filter */
#define FILTER_EVENT_BASE \
	GF_FEventType type; \
	GF_FilterPid *on_pid; \
	\


/*! Macro helper for event structure initializing*/
#define GF_FEVT_INIT(_a, _type, _on_pid)	{ memset(&_a, 0, sizeof(GF_FilterEvent)); _a.base.type = _type; _a.base.on_pid = _on_pid; }

/*! Base type of events. All events start with the fields defined in \ref FILTER_EVENT_BASE*/
typedef struct
{
	FILTER_EVENT_BASE
} GF_FEVT_Base;

/*! Event structure for GF_FEVT_PLAY, GF_FEVT_SET_SPEED*/
typedef struct
{
	FILTER_EVENT_BASE

	/*! params for : ranges in sec - if range is <0, it means end of file (eg [2, -1] with speed>0 means 2 +oo) */
	Double start_range, end_range;
	/*! params for GF_NET_CHAN_PLAY and GF_NET_CHAN_SPEED*/
	Double speed;

	/*! set when PLAY event is sent upstream to audio out, indicates HW buffer reset*/
	u8 hw_buffer_reset;
	/*! params for GF_FEVT_PLAY only: indicates this is the first PLAY on an element inserted from bcast*/
	u8 initial_broadcast_play;
	/*! params for GF_NET_CHAN_PLAY only
		0: range is in media time
		1: range is in timesatmps
		2: range is in media time but timestamps should not be shifted (hybrid dash only for now)
	*/
	u8 timestamp_based;
	/*! indicates the consumer only cares for the full file, not packets*/
	u8 full_file_only;
	/*! indicates any current download should be aborted*/
	u8 forced_dash_segment_switch;

	/*! indicates playback should start from given packet number - used by dasher when reloading sources*/
	u32 from_pck;
} GF_FEVT_Play;

/*! Event structure for GF_FEVT_SOURCE_SEEK and GF_FEVT_SOURCE_SWITCH*/
typedef struct
{
	FILTER_EVENT_BASE

	/*! start offset in source*/
	u64 start_offset;
	/*! end offset in source*/
	u64 end_offset;
	/*! new path to switch to*/
	const char *source_switch;
	/*! indicates previous source was a DASH init segment and should be kept in memory cache*/
	u8 previous_is_init_segment;
	/*! ignore cache expiration directive for HTTP*/
	u8 skip_cache_expiration;
	/*! hint block size for source, might not be respected*/
	u32 hint_block_size;
} GF_FEVT_SourceSeek;

/*! Event structure for GF_FEVT_SEGMENT_SIZE*/
typedef struct
{
	FILTER_EVENT_BASE
	/*! URL of segment this info is for, or NULL if single file*/
	const char *seg_url;
	/*! global sidx is signaled using is_init=1 and range in idx range*/
	Bool is_init;
	/*! media start range in segment file*/
	u64 media_range_start;
	/*! media end range in segment file*/
	u64 media_range_end;
	/*! index start range in segment file*/
	u64 idx_range_start;
	/*! index end range in segment file*/
	u64 idx_range_end;
} GF_FEVT_SegmentSize;

/*! Event structure for GF_FEVT_ATTACH_SCENE and GF_FEVT_RESET_SCENE
For GF_FEVT_RESET_SCENE, THIS IS A DIRECT FILTER CALL NOT THREADSAFE, filters processing this event SHALL run on the main thread*/
typedef struct
{
	FILTER_EVENT_BASE
	/*! Pointer to a GF_ObjectManager structure for this PID*/
	void *object_manager;
} GF_FEVT_AttachScene;

/*! Event structure for GF_FEVT_QUALITY_SWITCH*/
typedef struct
{
	FILTER_EVENT_BASE

	/*! switch quality up or down */
	Bool up;
	/*! 0: current group, otherwise index of the depending_on group */
	u32 dependent_group_index;
	/*! index of the quality to switch, as indicated in "has:qualities" property. If < 0, sets to automatic quality switching*/
	s32 q_idx;
	/*! 1+tile mode adaptation (does not change other selections) */
	u32 set_tile_mode_plus_one;
	/*! quality degradation hint, between 0 (full quality) and 100 (lowest quality, stream not currently rendered)*/
	u32 quality_degradation;
} GF_FEVT_QualitySwitch;

/*! Event structure for GF_FEVT_MOUSE*/
typedef struct
{
	FILTER_EVENT_BASE
	/*! GF_Event structure*/
	GF_Event event;
} GF_FEVT_Event;


/*! Event structure for GF_FEVT_VISIBILITY_HINT*/
typedef struct
{
	FILTER_EVENT_BASE
	/*! gives minimun and maximum coordinates of the visible rectangle associated with channels. min_x may be greater than max_x in case of 360 videos */
	u32 min_x, max_x, min_y, max_y;
	/*! if set, only min_x, min_y are used and indicate the gaze direction in pixels in the visual with/height frame (0,0) being top-left*/
	Bool is_gaze;
} GF_FEVT_VisibililityHint;


/*! Event structure for GF_FEVT_BUFFER_REQ*/
typedef struct
{
	FILTER_EVENT_BASE
	/*! indicates the max buffer to set on PID - the buffer is only activated on PIDs connected to decoders*/
	u32 max_buffer_us;
	/*! indicates the max playout buffer to set on PID (buffer level triggering playback)
		Note: this is not used internally by the blocking mechanisms, but may be needed by other filters to take decisions
	*/
	u32 max_playout_us;
	/*! indicates the min playout buffer to set on PID (buffer level triggering rebuffering)
		Note: this is not used internally by the blocking mechanisms, but may be needed by other filters to take decisions
	*/
	u32 min_playout_us;
	/*! if set, only the PID target of the event will have the buffer req set; otherwise, the buffer requirement event is passed down the chain until a raw media PID is found or a decoder is found. Used for muxers*/
	Bool pid_only;
} GF_FEVT_BufferRequirement;

/*! Event object*/
union __gf_filter_event
{
	GF_FEVT_Base base;
	GF_FEVT_Play play;
	GF_FEVT_SourceSeek seek;
	GF_FEVT_AttachScene attach_scene;
	GF_FEVT_Event user_event;
	GF_FEVT_QualitySwitch quality_switch;
	GF_FEVT_VisibililityHint visibility_hint;
	GF_FEVT_BufferRequirement buffer_req;
	GF_FEVT_SegmentSize seg_size;
};

/*! Gets readable name for event type
\param type type of the event
\return readable name of the event
*/
const char *gf_filter_event_name(GF_FEventType type);

/*! @} */


/*!
 *\addtogroup fs_filter Filter
 *\ingroup filters_grp
 *\brief Filter


The API for filters documents declaration of filters, their creation and functions available to interact with the filter session.
The gf_filter_* functions shall only be called from within a given filter, this filter being the target of the function. This is not checked at runtime.

Calling these functions on other filters will result in unpredictable behaviour, very likely crashes in multi-threading conditions.

A filter is instanciated through a filter register. The register holds entry points to a filter, arguments of a filter (basically property values) and capabilities.

Capabilities are used to check if a filter can be connected to a given input PID, or if two filters can be directly connected (capability match).

Capabilities are organized in so-called bundles, gathering the caps that shall be present or not present in the PID / connecting filter. Typically a capability bundle
will contain a stream type for input a stream type for output, a codec id for input and a codec id for output.

Several capability bundles can be used if needed. A good example is the writegen filter in GPAC, which simply transforms a sequence of media frames into a raw file, hence converts
stream_type/codecID to file extension and MIME type - cf \ref gpac/src/filters/write_generic.c

When resolving a chain, PID properties are checked against these capabilities. If a property of the same type exists in the PID than in the capability,
it must match the capability requirement (equal, excluded). If no property exists for a given non-optionnal capability type,
 the bundle is marked as not matching and the ext capability bundle in the filter is checked.
 A PID property not listed in any capability of the filter does not impact the matching.

 *	@{
 */


/*! Structure holding arguments for a filter*/
typedef enum
{
	/*! used for GUI config: regular argument type */
	GF_FS_ARG_HINT_NORMAL = 0,
	/*! used for GUI config: advanced argument type */
	GF_FS_ARG_HINT_ADVANCED = 1<<1,
	/*! used for GUI config: expert argument type */
	GF_FS_ARG_HINT_EXPERT = 1<<2,
	/*! used for GUI config: hidden argument type */
	GF_FS_ARG_HINT_HIDE = 1<<3,
	/*! if set indicates that the argument is updatable. If so, the value will be changed if \ref offset_in_private is valid, and the update_args function will be called if not NULL*/
	GF_FS_ARG_UPDATE = 1<<4,
	/*! used by meta filters (ffmpeg & co) to indicate the parsing is handled by the filter in which case the type is overloaded to string and passed to the update_args function*/
	GF_FS_ARG_META = 1<<5,
} GF_FSArgumentFlags;

/*! Structure holding arguments for a filter*/
typedef struct
{
	/*! argument name. Naming conventions:
		- only use lowercase
		- keep short and avoid '_'
	*/
	const char *arg_name;
	/*! offset of the argument in the structure, -1 means not exposed/stored in structure, in which case it is notified through the update_arg function.
	The offset is casted into the corresponding property value of the argument type*/
	s32 offset_in_private;
	/*! description of argument. Format conventions:
		- first letter is lowercase except if it is an acronym (eg, 'IP').
		- description does not end with a '.'; multiple sentences are however allowed with '.' separators.
		- description does not end with a new line '\n' or '\r' ; multiple sentences are however allowed with '.' separators.
		- keep it short. If too long, use " - see filter help" and put description in filter help.
		- only use markdown '`' and '*', except for enumeration lists which shall use "- ".
		- do not use CR/LF ('\r' and '\n') in description except to start enum types description.
		- use infinitive (eg \"set foo to bar\" and not \"sets foo to bar\").
		- Enumerations should be described as:
		 		sentence(s)'\n' (no '.' nor ':' at end)
		 		- name1: val'\n' (no '.' at end)
		 		- nameN: val (no '.' nor '\n' at end of last value description)
			and enum value description order shall match enum order
		- links are allowed, see \ref struct __gf_filter_register doc
	*/
	const char *arg_desc;
	/*! type of argument - this is a property type*/
	GF_PropType arg_type;
	/*! default value of argument, can be NULL (for number types, value 0 for each dimension of the type is assumed)*/
	const char *arg_default_val;
	/*! string describing an enum (unsigned integer type) or a min/max value.
		For min/max, the syntax is "min,max", with -I = -infinity and +I = +infinity
		For enum, the syntax is "a|b|...", resoling in a=0, b=1,... To skip a value insert '|' eg "|a|b" resolves to a=1, b=2, "a||b" resolves to a=0, b=2

		Naming conventions for enumeration:
			- use single word
			- use lowercase escept for single/double characters name (eg 'A', 'V').
	*/
	const char *min_max_enum;
	/*! set of argument flags*/
	GF_FSArgumentFlags flags;
} GF_FilterArgs;

/*! Some macros for assigning filter capability types quickly*/
#define CAP_SINT(_f, _a, _b) { .code=_a, .val={.type=GF_PROP_SINT, .value.sint = _b}, .flags=(_f) }
#define CAP_UINT(_f, _a, _b) { .code=_a, .val={.type=GF_PROP_UINT, .value.uint = _b}, .flags=(_f) }
#define CAP_LSINT(_f, _a, _b) { .code=_a, .val={.type=GF_PROP_LSINT, .value.longsint = _b}, .flags=(_f) }
#define CAP_LUINT(_f, _a, _b) { .code=_a, .val={.type=GF_PROP_LUINT, .value.longuint = _b}, .flags=(_f) }
#define CAP_BOOL(_f, _a, _b) { .code=_a, .val={.type=GF_PROP_BOOL, .value.boolean = _b}, .flags=(_f) }
#define CAP_FIXED(_f, _a, _b) { .code=_a, .val={.type=GF_PROP_FLOAT, .value.fnumber = _b}, .flags=(_f) }
#define CAP_FLOAT(_f, _a, _b) { .code=_a, .val={.type=GF_PROP_FLOAT, .value.fnumber = FLT2FIX(_b)}, .flags=(_f) }
#define CAP_FRAC_INT(_f, _a, _b, _c) { .code=_a, .val={.type=GF_PROP_FRACTION, .value.frac.num = _b, .value.frac.den = _c}, .flags=(_f) }
#define CAP_FRAC(_f, _a, _b) { .code=_a, .val={.type=GF_PROP_FRACTION, .value.frac = _b}, .flags=(_f) }
#define CAP_DOUBLE(_f, _a, _b) { .code=_a, .val={.type=GF_PROP_DOUBLE, .value.number = _b}, .flags=(_f) }
#define CAP_NAME(_f, _a, _b) { .code=_a, .val={.type=GF_PROP_NAME, .value.string = _b}, .flags=(_f) }
#define CAP_STRING(_f, _a, _b) { .code=_a, .val={.type=GF_PROP_STRING, .value.string = _b}, .flags=(_f) }
#define CAP_UINT_PRIORITY(_f, _a, _b, _p) { .code=_a, .val={.type=GF_PROP_UINT, .value.uint = _b}, .flags=(_f), .priority=_p}

/*! Flags for filter capabilities*/
enum
{
	/*! when not set, indicates the start of a new set of capabilities. Set by default by the generic GF_CAPS_* */
	GF_CAPFLAG_IN_BUNDLE = 1,
	/*!  if set this is an input capability of the bundle*/
	GF_CAPFLAG_INPUT = 1<<1,
	/*!  if set this is an output capability of the bundle. A capability can be declared both as input and output. For example stream type is usually the same on both inputs and outputs*/
	GF_CAPFLAG_OUTPUT = 1<<2,
	/*!  when set, the capability is valid if the value does not match. If an excluded capability is not found in the destination PID, it is assumed to match*/
	GF_CAPFLAG_EXCLUDED = 1<<3,
	/*! when set, the capability is validated only for filter loaded for this destination filter*/
	GF_CAPFLAG_LOADED_FILTER = 1<<4,
	/*! Only used for output capabilities, indicates that this capability applies to all bundles. This avoids repeating capabilities common to all bundles by setting them only in the first*/
	GF_CAPFLAG_STATIC = 1<<5,
	/*! Only used for input capabilities, indicates that this capability is optional in the input PID */
	GF_CAPFLAG_OPTIONAL = 1<<6,
};

/*! Some macros for assigning filter capability flags quicly*/
#define GF_CAPS_INPUT	(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT)
#define GF_CAPS_INPUT_OPT	(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_OPTIONAL)
#define GF_CAPS_INPUT_STATIC	(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_STATIC)
#define GF_CAPS_INPUT_STATIC_OPT	(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_STATIC|GF_CAPFLAG_OPTIONAL)
#define GF_CAPS_INPUT_EXCLUDED	(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_EXCLUDED)
#define GF_CAPS_INPUT_LOADED_FILTER	(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_LOADED_FILTER)
#define GF_CAPS_OUTPUT	(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT)
#define GF_CAPS_OUTPUT_LOADED_FILTER	(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_LOADED_FILTER)
#define GF_CAPS_OUTPUT_EXCLUDED	(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_EXCLUDED)
#define GF_CAPS_OUTPUT_STATIC	(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_STATIC)
#define GF_CAPS_OUTPUT_STATIC_EXCLUDED	(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_EXCLUDED|GF_CAPFLAG_STATIC)
#define GF_CAPS_INPUT_OUTPUT	(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_OUTPUT)
#define GF_CAPS_INPUT_OUTPUT_OPT	(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_OPTIONAL)

/*! Filter capability description*/
typedef struct
{
	/*! 4cc of the capability listed. This shall be 0 or the type of a built-in property */
	u32 code;
	/*! default type and value of the capability listed*/
	GF_PropertyValue val;
	/*! name of the capability listed, NULL if code is set. The special value * is used to indicate that the capability is solved at runtime (the filter must be loaded)*/
	const char *name;
	/*! Flags of the capability*/
	u32 flags;
	/*! overrides the filter register priority for this capability. Usually 0*/
	u8 priority;
} GF_FilterCapability;

/*! Filter session capability structure*/
typedef struct
{
	u32 max_screen_width;
	u32 max_screen_height;
	u32 max_screen_bpp;
	u32 max_screen_fps;
	u32 max_screen_nb_views;
	u32 max_audio_channels;
	u32 max_audio_sample_rate;
	u32 max_audio_bit_depth;
} GF_FilterSessionCaps;

/*! Gets filter session capability
\param filter source of the query - avoids fetching the filter session object
\param caps pointer filled with caps
*/
void gf_filter_get_session_caps(GF_Filter *filter, GF_FilterSessionCaps *caps);

/*! Sets filter session capability
\param filter source of the query - avoids fetching the filter session object
\param caps session capability new values - completely replace the old ones
*/
void gf_filter_set_session_caps(GF_Filter *filter, GF_FilterSessionCaps *caps);

/*! Filter probe score, used when probing a URL/MIME or when probing formats from data*/
typedef enum
{
	/*! input is not supported*/
	GF_FPROBE_NOT_SUPPORTED = 0,
	/*! input is supported with potentially missing features*/
	GF_FPROBE_MAYBE_SUPPORTED,
	/*! input is supported*/
	GF_FPROBE_SUPPORTED,
	/*! input data should be handled by this filter*/
	GF_FPROBE_FORCE,
	/*! used by formats not supporting data prober*/
	GF_FPROBE_EXT_MATCH,
} GF_FilterProbeScore;

/*! Quick macro for assigning the capability arrays to the register structure*/
#define SETCAPS( __struct ) .caps = __struct, .nb_caps=sizeof(__struct)/sizeof(GF_FilterCapability)

#ifndef GPAC_DISABLE_DOC
#define GF_FS_SET_DESCRIPTION(_desc) .description = _desc,
#define GF_FS_SET_AUTHOR(_author) .author = _author,
#define GF_FS_SET_HELP(_help) .help = _help,
#define GF_FS_DEF_ARG(_name, _offset, _desc, _type, _default, _enum, _flags) { _name, _offset, _desc, _type, _default, _enum, _flags }
#else
#define GF_FS_SET_DESCRIPTION(_desc)
#define GF_FS_SET_AUTHOR(_author)
#define GF_FS_SET_HELP(_help)
#define GF_FS_SET_ARGHELP(_help)
#define GF_FS_DEF_ARG(_name, _offset, _desc, _type, _default, _enum, _flags) {_name, _offset, _type, _default, _enum, _flags}
#endif

/*! Filter register flags*/
typedef enum
{
	/*! when set indicates all calls shall take place in the main thread (running GL output) - to be refined*/
	GF_FS_REG_MAIN_THREAD = 1<<1,
	/*! when set indicates the initial call to configure_pid will happen in the main thread. This is typically called by decoders requiring
	a GL context (currently only in main thread) upon init, but not requiring it for the decode. Such decoders get their GL frames mapped
	(through \ref get_gl_texture callback) in the main GL thread*/
	GF_FS_REG_CONFIGURE_MAIN_THREAD = 1<<2,
	/*! when set indicates the filter does not take part of dynamic filter chain resolution and can only be used by explicitly loading the filter*/
	GF_FS_REG_EXPLICIT_ONLY = 1<<3,
	/*! when set ignores the filter weight during link resolution - this is typically needed by decoders requiring a specific reframing
	e.g. nvdec wants annex B format, so that the weight of the reframer+decoder is the same as the weight of other decoders*/
	GF_FS_REG_HIDE_WEIGHT = 1<<4,
	/*! Usually set for filters acting as sources but without exposing an src argument. This prevents throwing warnings on arguments not handled by the filter*/
	GF_FS_REG_ACT_AS_SOURCE = 1<<5,
	/*! Indicates the filter is likely to block for a long time (typically, network IO).
	In multithread mode, this prevents the filter to be scheduled on the main thread, blocking video or audio output.
	Ignored in single thread mode.*/
	GF_FS_REG_BLOCKING = 1<<6,
	/*! Indicates the filter PIDs may be dynamically added uring process (e.g.M2TS, GSF, etc).
	This will prevent dectivating a filter when none of its output PIDs are connected*/
	GF_FS_REG_DYNAMIC_PIDS = 1<<7,

	/*! flag dynamically set at runtime for registries loaded through shared libraries*/
	GF_FS_REG_DYNLIB = 0x80000000
} GF_FSRegisterFlags;

/*! The filter register. Registries are loaded once at the start of the session and shall never be modified after that.
If capabilities need to be changed for a specific filter, use \ref gf_filter_override_caps*/
struct __gf_filter_register
{
	/*! mandatory - name of the filter as used when setting up filters. Naming conventions:
		- shall not contain any space (breaks filter lookup)
		- should use lowercase
		- should not use '_'
	*/
	const char *name;
	/*! optional - size of private stack structure. The structure is allocated by the framework and arguments are setup before calling any of the filter functions*/
	u32 private_size;
	/*! indicates the max number of additional input PIDs - muxers and scalable filters typically set this to (u32) -1. A value of 0 implies the filter can only handle one PID*/
	u32 max_extra_pids;
	/*! set of register flags*/
	GF_FSRegisterFlags flags;

	/*! list of PID capabilities*/
	const GF_FilterCapability *caps;
	/*! number of PID capabilities*/
	u32 nb_caps;

	/*! optional - filter arguments if any*/
	const GF_FilterArgs *args;

	/*! mandatory - callback for filter processing*/
	GF_Err (*process)(GF_Filter *filter);

	/*! optional for sources, mandatory for filters and sinks - callback for PID update may be called several times
	on the same PID if PID config is changed.
	Since discontinuities may happen at any time, and a filter may fetch packets in burst,
	this function may be called while the filter is calling \ref gf_filter_pid_get_packet (this is the only reentrant call for filters)

	\param filter the target filter
	\param PID the input PID to configure
	\param is_remove indicates the input PID is removed
	\return error if any. A filter returning an error will trigger a reconfigure of the chain to find another filter.
	a filter may return GF_REQUIRES_NEW_INSTANCE to indicate the PID cannot be processed
	in this instance but could be in a clone of the filter.
	a filter may return GF_FILTER_NOT_SUPPORTED to indicate the PID cannot be processed and no alternate chain resolution would help
	*/
	GF_Err (*configure_pid)(GF_Filter *filter, GF_FilterPid *PID, Bool is_remove);

	/*! optional - callback for filter initialization -  private stack of filter is allocated by framework)
	\param filter the target filter
	\return error if any.
	*/
	GF_Err (*initialize)(GF_Filter *filter);

	/*! optional - callback for filter desctruction -  private stack of filter is freed by framework
	\param filter the target filter
	*/
	void (*finalize)(GF_Filter *filter);

	/*! optional - callback for arguments update. If GF_OK is returned, the stack is updated accordingly
	if function is NULL, all updatable arguments will be changed in the stack without the filter being notified

	\param filter the target filter
	\param arg_name the name of the argument being set
	\param new_val the value of the argument being set
	\return error if any.
	*/
	GF_Err (*update_arg)(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *new_val);

	/*! optional - process a given event. Retruns TRUE if the event has to be canceled, FALSE otherwise
	\param filter the target filter
	\param evt the event to process
	\return GF_TRUE if the event should be canceled, GF_FALSE otherwise
	*/
	Bool (*process_event)(GF_Filter *filter, const GF_FilterEvent *evt);

	/*! optional - Called whenever an output PID needs format renegotiaition. If not set, a filter chain will be loaded to solve the negotiation

	\param filter the target filter
	\param PID the filter output PID being reconfigured
	\return error code if any
	*/
	GF_Err (*reconfigure_output)(GF_Filter *filter, GF_FilterPid *PID);

	/*! optional, mostly used for source filters and destination filters - probe the given URL, returning a score.
	This function is called before opening the source (no data received yet)

	\param url the url of the source to probe, never NULL.
	\param mime the MIME type of the source to probe. Can be NULL if mime not available at probe type
	\return probe score
	*/
	GF_FilterProbeScore (*probe_url)(const char *url, const char *mime);

	/*! optional, usually set by demuxers. This function probes the mime type of a data chunk, usually located at the start of the file.
	This function is called once the source is open, but is never called on an instanciated filter. The returned mime type (if any) is then used instead of the file extension
	for solving filter graph.
	Note: demux filters should always exposed 2 input caps bundle, one for specifiying input cap by file extension and one for specifying input cap by mime type.
	\param data data to probe
	\param size size of the data to probe
	\param score set to the probe score. Initially set to \ref GF_FPROBE_NOT_SUPPORTED before calling the function. If you are certain of the data type, use \ref GF_FPROBE_SUPPORTED, if unsure use \ref GF_FPROBE_MAYBE_SUPPORTED. If the format cannot be probed (bad design), set it to \ref GF_FPROBE_EXT_MATCH
	\return mime type of content, list of known extensions for the format if score is set to GF_FPROBE_EXT_MATCH, or NULL if not detected.
	*/
	const char * (*probe_data)(const u8 *data, u32 size, GF_FilterProbeScore *score);

	/*! for filters having the same match of input capabilities for a PID, the filter with priority at the lowest value will be used
	scalable decoders should use high values, so that they are only selected when enhancement layers are present*/
	u8 priority;

	/*! optional for dynamic filter registries. Dynamic registries may declare any number of registries. The register_free function will be called to cleanup any allocated memory

	\param session the filter session
	\param freg the filter register to destroy
	*/
	void (*register_free)(GF_FilterSession *session, struct __gf_filter_register *freg);
	/*! user data of register loader, not inspected/modified by filter session*/
	void *udta;


	/*! version of the filter, usually only for external libs*/
	const char *version;
#ifndef GPAC_DISABLE_DOC
	/*! short description of the filter. Conventions:
		- should be small (used to generate UI / documentation)
		- Should be Capitalized
		- shall not use markdown
		- shall be single line
	*/
	const char *description;
	/*! author of the filter. Conventions:
		- shall not use markdown
		- first line if present is author name and should be normally capitalized
		- second line if present is comma-separated list of contact info (eg http://foo.bar,mailto:foo@bar.com)
	*/
	const char *author;
	/*! help of the filter. Conventions:
		- describe any non-obvious behaviour of the filter here
		- markdown is allowed. Use '`' for code, "__" for italic, "**" for bold and  "~~" for strikethrough.
		- mardown top-level sections shall use '#', second level '##', and so on
		- mardown bullet lists shall use "- ", " - " etc...
		- notes shall be identifed as a line starting with "Note: "
		- warnings shall be identifed as a line starting with "Warning: "
		- the sequence "[-" is reserved for option links. It is formated as:
		 		- "[-OPT]()": link to self page for option OPT
		 		- "[-OPT](LINK)": link to other page for option OPT.
			LINK can be:
				- GPAC: resolves to general GPAC help
				- CORE: resolves to general CORE help
				- LOG: resolves to general LOG help
				- ANY: resolves to filter with register name 'ANY'

			Note that [text](ANY) with ANY a filter register name will link to that filter help page
	*/
	const char *help;
#endif
};


/*! Gets filter private stack - the stack is allocated and freed by the filter session, as are any arguments. The rest is up to the filter to delete it
\param filter target filter
\return filter private stack if any, NULL otherwise
*/
void *gf_filter_get_udta(GF_Filter *filter);

/*! Sets filter name - mostly used for logging purposes
\param filter target filter
\param name new name to assign. If NULL, name is reset to register name
*/
void gf_filter_set_name(GF_Filter *filter, const char *name);

/*! Gets filter name
\param filter target filter
\return name of the filter
*/
const char *gf_filter_get_name(GF_Filter *filter);

/*! Makes the filter sticky. A sticky filter is not removed when all its input PIDs are disconnected. Typically used by the player
\param filter target filter
*/
void gf_filter_make_sticky(GF_Filter *filter);

/*! Return the number of queued events on the filter. Events are not aggregated, some filter may want to wait until all events are processed before taking actions. The function recursively goes up the filter chain and count queued events.
\param filter target filter
\return number of queued events
*/
u32 gf_filter_get_num_events_queued(GF_Filter *filter);

/*! Returns the single instance of GPAC download manager. DO NOT DESTROY IT!!
\param filter target filter
\return the GPAC download manager
*/
GF_DownloadManager *gf_filter_get_download_manager(GF_Filter *filter);

/*! Asks task reschedule for a given delay. There is no guarantee that the task will be recalled at exactly the desired delay
\param filter target filter
\param us_until_next number of microseconds to wait before recalling this task
*/
void gf_filter_ask_rt_reschedule(GF_Filter *filter, u32 us_until_next);

/*! Posts a filter process task to the parent session. This is needed for some filters not having any input packets to process but still needing to work
such as decoder flushes, servers, etc... The filter session will ignore this call if the filter is already scheduled for processing
\param filter target filter
*/
void gf_filter_post_process_task(GF_Filter *filter);

/*! Posts a task to the session scheduler. This task is not a process task but the filter will still be called by a single thread at any time (ie, might delay process)
\param filter target filter
\param task_execute the callback function for the task. The callback can return GF_TRUE to reschedule the task, in which case the task will be rescheduled
immediately or after reschedule_ms.
\param udta user data passed back to the task function
\param task_name name of the task for logging purposes
\return error code if failure
*/
GF_Err gf_filter_post_task(GF_Filter *filter, Bool (*task_execute) (GF_Filter *filter, void *callback, u32 *reschedule_ms), void *udta, const char *task_name);


/*! Sets callback function on source filter setup failure
\param filter target filter
\param source_filter the source filter to monitor
\param on_setup_error callback function to call upon source  setup error
\param udta user data passed back to the task function
*/
void gf_filter_set_setup_failure_callback(GF_Filter *filter, GF_Filter *source_filter, void (*on_setup_error)(GF_Filter *f, void *on_setup_error_udta, GF_Err e), void *udta);

/*! Notify a filter setup error. This is typically called when a source filter or a filter having accepted input PIDs detects an issue.
For a source filter (no input PID), the failure callback will be called if any, and the filter will be removed.
For a connected filter, all input PIDs od the filter will be disconnected and the filter removed.
\param filter target filter
\param reason the failure reason code
*/
void gf_filter_setup_failure(GF_Filter *filter, GF_Err reason);

/*! Notify a filter fatal error but not a connection error. Typically, a 404 in HTTP.
\param filter target filter
\param reason the failure reason code
\param force_disconnect indicates if the filter chain should be disconnected or not
*/
void gf_filter_notification_failure(GF_Filter *filter, GF_Err reason, Bool force_disconnect);

/*! Disconnects a source filter chain between two filters
\param filter the calling filter. This filter is NOT disconnected
\param src_filter the source filter point of the chain to disconnect.
*/
void gf_filter_remove_src(GF_Filter *filter, GF_Filter *src_filter);

/*! Sets the number of additional input PID a filter can accept. This overrides the default value of the filter register
\param filter the target filter
\param max_extra_pids the number of additional PIDs this filter can accept
*/
void gf_filter_sep_max_extra_input_pids(GF_Filter *filter, u32 max_extra_pids);

/*! Queries if blocking mode is enabled for the filter
\param filter the target filter
\return GF_TRUE if blocking mode is enabled, GF_FALSE otherwise
*/
Bool gf_filter_block_enabled(GF_Filter *filter);


/*! Connects a source to this filter.
Note:
Any filter loaded between the source and the calling filter will not use argument inheritance from the caller.

\param filter the target filter
\param url url of source to connect to, with optional arguments.
\param parent_url url of parent if any
\param err return code - can be NULL
\return the new source filter instance or NULL if error
*/
GF_Filter *gf_filter_connect_source(GF_Filter *filter, const char *url, const char *parent_url, GF_Err *err);

/*! Connects a destination to this filter
\param filter the target filter
\param url url of destination to connect to, with optional arguments.
\param err return code - can be NULL
\return the new destination filter instance or NULL if error
*/
GF_Filter *gf_filter_connect_destination(GF_Filter *filter, const char *url, GF_Err *err);

/*! Checks if a source filter can handle the given URL. The source filter is not loaded.

\param filter the target filter
\param url url of source to connect to, with optional arguments.
\param parent_url url of parent if any
\return GF_TRUE if a source filter can be found for this URL, GF_FALSE otherwise
*/
Bool gf_filter_is_supported_source(GF_Filter *filter, const char *url, const char *parent_url);

/*! Gets the number of input PIDs connected to a filter
\param filter the target filter
\return number of input PIDs
*/
u32 gf_filter_get_ipid_count(GF_Filter *filter);

/*! Gets an input PIDs connected to a filter
\param filter the target filter
\param idx index of the input PID to retrieve, between 0 and \ref gf_filter_get_ipid_count
\return the input PID, or NULL if not found
*/
GF_FilterPid *gf_filter_get_ipid(GF_Filter *filter, u32 idx);

/*! Gets the number of output PIDs connected to a filter
\param filter the target filter
\return number of output PIDs
*/
u32 gf_filter_get_opid_count(GF_Filter *filter);

/*! Gets an output PIDs connected to a filter
\param filter the target filter
\param idx index of the output PID to retrieve, between 0 and \ref gf_filter_get_opid_count
\return the output PID, or NULL if not found
*/
GF_FilterPid *gf_filter_get_opid(GF_Filter *filter, u32 idx);

/*! Queries buffer max limits on a filter. This is the max of buffer limits on all its connected outputs
\param filter the target filter
\param max_buf will be set to the maximum buffer duration in microseconds
\param max_playout_buf will be set to the maximum playout buffer (the one triggering play) duration in microseconds
*/
void gf_filter_get_buffer_max(GF_Filter *filter, u32 *max_buf, u32 *max_playout_buf);

/*! Sets a clock state at session level indicating the time / timestamp of the last rendered frame. This is used by basic audio output
\param filter the target filter
\param time_in_us the system time in us, see \ref gf_sys_clock_high_res
\param media_timestamp the media timestamp associated with this time.
*/
void gf_filter_hint_single_clock(GF_Filter *filter, u64 time_in_us, Double media_timestamp);

/*! Retrieves the clock state at session level, as set by \ref gf_filter_hint_single_clock
\param filter the target filter
\param time_in_us will be set to the system time in us, see \ref gf_sys_clock_high_res
\param media_timestamp will be set to the media timestamp associated with this time.
*/
void gf_filter_get_clock_hint(GF_Filter *filter, u64 *time_in_us, Double *media_timestamp);

/*! Explicitly assigns a source ID to a filter. This shall be called before connecting the link_from filter
If no ID is assigned to the linked filter, a dynamic one in the form of _%08X_ (using the filter mem adress) will be used
\param filter the target filter
\param link_from the filter to link from
\param link_ext any link extensions allowed in link syntax:
#PIDNAME: accepts only PID(s) with name PIDNAME
#TYPE: accepts only PIDs of matching media type. TYPE can be 'audio' 'video' 'scene' 'text' 'font'
#TYPEN: accepts only Nth PID of matching type from source
#P4CC=VAL: accepts only PIDs with property matching VAL.
#PName=VAL: same as above, using the built-in name corresponding to the property.
#P4CC-VAL: accepts only PIDs with property strictly less than VAL (only for 1-dimension number properties).
#P4CC+VAL: accepts only PIDs with property strictly greater than VAL (only for 1-dimension number properties).
\return error code if any
*/
GF_Err gf_filter_set_source(GF_Filter *filter, GF_Filter *link_from, const char *link_ext);

/*! Explicitly assigns an ID to a filter. This shall be called before running the session, and cannot be called on a filter with ID assign
\param filter the target filter
\param filter_id the ID to assign
\return error code if any
*/
GF_Err gf_filter_assign_id(GF_Filter *filter, const char *filter_id);

/*! Gets the ID of a filter
\param filter the target filter
\return ID of the filter, NULL if not defined
*/
const char *gf_filter_get_id(GF_Filter *filter);


/*! Overrides the filter register caps with new caps for this instance. Typically used when an option of the filter changes the capabilities
\param filter the target filter
\param caps the new set of capabilities to use for the filter. These are NOT copied and shall be valid for the lifetime of the filter
\param nb_caps number of capabilities set
\return error code if any
*/
GF_Err gf_filter_override_caps(GF_Filter *filter, const GF_FilterCapability *caps, u32 nb_caps );

/*! Filter session separator set query*/
typedef enum
{
	/*! queries the character code used to separate between filter arguments*/
	GF_FS_SEP_ARGS=0,
	/*! queries the character code used to separate between argument name and value*/
	GF_FS_SEP_NAME,
	/*! queries the character code used to indicate fragment identifiers (source PIDs, PID properties)*/
	GF_FS_SEP_FRAG,
	/*! queries the character code used to separate items in a list*/
	GF_FS_SEP_LIST,
	/*! queries the character code used to negate values*/
	GF_FS_SEP_NEG,
} GF_FilterSessionSepType;

/*! Queries the character code used as a given separator type in argument names. Used for formating arguments when loading sources and destinations from inside a filter
\param filter the target filter
\param sep_type the separator type to query
\return character code of the separator
*/
u8 gf_filter_get_sep(GF_Filter *filter, GF_FilterSessionSepType sep_type);

/*! Queries the arguments of the destination arguments. The first output PID connected to a filter with non NULL args will be used (this is a recursive check until end of chain)
\param filter the target filter
\return the argument string of the destination args
*/
const char *gf_filter_get_dst_args(GF_Filter *filter);

/*! Queries the destination name. The first output PID connected to a filter with non NULL args will be used (this is a recursive check until end of chain)
\param filter the target filter
\return the argument string of the destination (SHALL be freed by caller), NULL if none found
*/
char *gf_filter_get_dst_name(GF_Filter *filter);

/*! Sends an event on all input PIDs
\param filter the target filter
\param evt the event to send
*/
void gf_filter_send_event(GF_Filter *filter, GF_FilterEvent *evt);

/*! Looks for a built-in property value marked as informative on a filter on all PIDs (inputs and output)
This is a recursive call on both input and ouput chain.
There is no guarantee that a queried property will still be valid at the setter side upon returning the call, the setter could have
already reassigned it to NULL. To avoids random behaviour, the property returned is reference counted so that it is not
destroyed by the setter while the caller uses it.

Properties retrieved shall be released using \ref gf_filter_release_property. Failure to do so will cause memory leaks in the program.

If the \ref propentry pointer references a non-null value, this value will be released using \ref gf_filter_release_property,
so make sure to initialize the pointer to a NULL value on the first call.
This avoid calling  \ref gf_filter_release_property after each get_info in the calling code:

\code{.c}
GF_PropertyEntry *pe=NULL;
const GF_PropertyValue *p;
p = gf_filter_get_info(filter, FOO, &pe);
if (p) { }
p = gf_filter_get_info_str(filter, "BAR", &pe);
if (p) { }
p = gf_filter_pid_get_info(PID, ABCD, &pe);
if (p) { }
p = gf_filter_pid_get_info_str(PID, "MyProp", &pe);
if (p) { }
gf_filter_release_property(pe);
\endcode



\param filter the target filter
\param prop_4cc the code of the built-in property to fetch
\param propentry the property reference object for later release. SHALL not be NULL
\return the property if found NULL otherwise.
*/
const GF_PropertyValue *gf_filter_get_info(GF_Filter *filter, u32 prop_4cc, GF_PropertyEntry **propentry);

/*! Looks for a property value on a filter on all PIDs (inputs and output).
This is a recursive call on both input and ouput chain
Properties retrieved shall be released using \ref gf_filter_release_property. See \ref gf_filter_pid_get_info for more details.
\param filter the target filter
\param prop_name the name of the property to fetch
\param propentry the property reference object for later release. See \ref gf_filter_pid_get_info for more details.
\return the property if found NULL otherwise
*/
const GF_PropertyValue *gf_filter_get_info_str(GF_Filter *filter, const char *prop_name, GF_PropertyEntry **propentry);

/*! Release a property previously queried, only used for []_get_info_[] functions.
This is a recursive call on both input and ouput chain
\param propentry the property reference object to be released
*/
void gf_filter_release_property(GF_PropertyEntry *propentry);

/*! Sends a filter argument update
\param filter the target filter
\param target_filter_id if set, the target filter will be changed to the filter with this id if any. otherwise the target filter is the one specified
\param arg_name argument name
\param arg_val argument value
\param propagate_mask propagation flags - 0 means no propagation
*/
void gf_filter_send_update(GF_Filter *filter, const char *target_filter_id, const char *arg_name, const char *arg_val, u32 propagate_mask);



/*! Filters may request listening on global events.\n
\warning This is not thread safe, the callback will be called directly upon event firing, your filter has to take care of this

API of event filter is likely to change any time soon - used for backward compatibility with some old modules
*/
typedef struct
{
	/*! user data of the listener */
	void *udta;
	/*! callback called when an event should be filtered.
	\param udta user data of the listener
	\param evt the event to be processed
	\param consumed_by_compositor indicates the event was already used by the compositor
	\return GF_TRUE if the event is to be cancelled, GF_FALSE otherwise
	*/
	Bool (*on_event)(void *udta, GF_Event *evt, Bool consumed_by_compositor);
} GF_FSEventListener;

/*! Adds an event listener to the session
\param filter filter object
\param el the event listener to add
\return the error code if any
*/
GF_Err gf_filter_add_event_listener(GF_Filter *filter, GF_FSEventListener *el);

/*! Removes an event listener from the session
\param filter filter object
\param el the event listener to add
\return the error code if any
*/
GF_Err gf_filter_remove_event_listener(GF_Filter *filter, GF_FSEventListener *el);

/*! Forwards an event to the filter session
\param filter filter object
\param evt the event forwarded
\param consumed if set, indicates the event was already consummed/processed before forwarding
\param skip_user if set, indicates the event should only be dispatched to event listeners.
Otherwise, if a user is assigned to the session, the event is forwarded to the user
\return the error code if any
*/
Bool gf_filter_forward_gf_event(GF_Filter *filter, GF_Event *evt, Bool consumed, Bool skip_user);

/*! Forwards an event to the filter session. This is a shortcut for \ref gf_filter_forward_gf_event(session, evt, GF_FALSE, GF_FALSE);
\param filter filter object
\param evt the event forwarded
\return the error code if any
*/
Bool gf_filter_send_gf_event(GF_Filter *filter, GF_Event *evt);

/*! Checks if all sink filters in the session have seen end of stream.
\param filter filter object
\return GF_TRUE if all sinks are in end of stream, GF_FALSE otherwise
*/
Bool gf_filter_all_sinks_done(GF_Filter *filter);

/*! Gets a filter argument value as string for a given argument name..
\param filter filter object
\param arg_name name of the filter argument
\param dump buffer in which any formating of argument value will take place
\return the string value of the argument, or NULL if argument is not found or is invalid
*/
const char *gf_filter_get_arg(GF_Filter *filter, const char *arg_name, char dump[GF_PROP_DUMP_ARG_SIZE]);

/*! Checks if a given mime type is supported as input in the parent session
\param filter querying filter
\param mime mime type to query
\return GF_TRUE if mime is supported
*/
Bool gf_filter_is_supported_mime(GF_Filter *filter, const char *mime);


/*! Sends UI event from video output into the session. For now only direct callback to global event handler is used, event is not forwarded down the chain
\param filter triggering filter
\param uievt event to send
\return GF_TRUE if event has been cancelled, FALSE otherwise
*/
Bool gf_filter_ui_event(GF_Filter *filter, GF_Event *uievt);


/*! Registers filter as an openGL provider. This is only used by sink filters creating a openGL context, to avoid creating another context. Filters registered as OpenGL providers will run on the main thread.
\param filter filter providing OpenGL context
\param do_register if TRUE the filter carries a valid gl context, if FALSE the filter no longer carries a valid GL context
*/
void gf_filter_register_opengl_provider(GF_Filter *filter, Bool do_register);

/*! Requests openGL support for this filter. If no OpenGL providers exist, a default provider will be created (GL context creation on hidden window). Filters requiring OpenGL will run on the main thread.

Note: filters using openGL should not assume any persistent GL state among calls.
Other filters using openGL might change the openGL context state (depth test, viewport, alpha blending, culling, etc...).

\param filter filter providing OpenGL context
\return error code if any
*/
GF_Err gf_filter_request_opengl(GF_Filter *filter);


/*! Count the number of source filters for the given filter matching the given protocol type.
\param filter filter to inspect
\param protocol_scheme scheme of the protocol to test, without ://, eg "http", "rtp", "https"
\param expand_proto if set to GF_TRUE, any source protocol with a name begining like \ref protocol_scheme will be matched. For example, use this with "http" to match all http and https schemes.
\param enum_pids user function to enumerate PIDs against which the source should be checked . If not set, all source filters will be checked.
\param udta user data for the callback function
\return the number of source filters matched by protocols
*/
u32 gf_filter_count_source_by_protocol(GF_Filter *filter, const char *protocol_scheme, Bool expand_proto, GF_FilterPid * (*enum_pids)(void *udta, u32 *idx), void *udta);

/*! Disables data probing on the given filter. Typically used by filters loading source filters.
\param filter target filter
*/
void gf_filter_disable_probe(GF_Filter *filter);

/*! Disables input connection on the filter. This is used by filters acting both as true sources or demux/processing filters depending on their option.
\param filter target filter
*/
void gf_filter_disable_inputs(GF_Filter *filter);

/*! Checks if some PIDs are still not connected in the graph originating at filter. This is typically used by filters dynamically loading source filters to make sure all PIDs from the source are connected.

NOTE: this does not guarantee that no other PID remove or configure will happen later on, this depends on the source type and is unknown by GPAC's filter architecture.
\param filter target filter
\param stop_at_filter check connections until this filter. If NULL, connections are checked until upper (sink) end of graph
\return GF_TRUE if any filter in the path has pending PID connections
*/
Bool gf_filter_has_pid_connection_pending(GF_Filter *filter, GF_Filter *stop_at_filter);


/*! Checks if reporting is turned on at session level.
\param filter target filter
\return GF_TRUE if reporting is enabled, GF_FALSE otherwise
*/
Bool gf_filter_reporting_enabled(GF_Filter *filter);

/*! Updates filter status string and progress. Should not be called if reporting is turned off at session level.
This allows gathering stats from filter in realtime. The status string can be anything but shall not contain '\n' and
should not contain any source or destination URL except for sources and sinks.
\param filter target filter
\param percent percentage (from 0 to 10000) of operation status. If more than 10000 ignored
\param szStatus string giving a status of the filter
\return error code if any
*/
GF_Err gf_filter_update_status(GF_Filter *filter, u32 percent, char *szStatus);


/*! used by meta-filters (ffmpeg and co) to report an option was set but not used by the filter. This is needed since these filters might not
know the set of available options at initialize() time.
\param filter target filter
\param arg name of the argument not used/found
*/
void gf_filter_report_unused_meta_option(GF_Filter *filter, const char *arg);

/*! @} */


/*!
 *\addtogroup fs_pid Filter PID
 *\ingroup filters_grp
 *\brief Filter Interconnection


A PID is a connection between two filters, holding packets to process. Internally, a PID created by a filter (output PID) is different from an input PID to a filter (configure_pid)
but the API has been designed to hide this, so that most PID functions can be called regardless of the input/output nature of the PID.

All setters functions (gf_filter_pid_set*) will fail on an input PID.

The generic design of the architecture is that each filter is free to decide how it handle PIDs and their packets. This implies that the filter session has no clue how
an output PID relates to an input PID (same goes for packets).
Developpers must therefore manually copy PID properties that seem relevant, or more practically copy all properties from input PID to output PID and
reassign output PID properties changed by the filter.

PIDs can be reconfigured multiple times, even potentially changing caps on the fly. The current architecture does not check for capability matching during reconfigure, it is up to
the filter to do so.

It is also up to filters to decide how to handle a an input PID removal: remove the output PID immediately, keep it open to flush internal data or keep generating data on the output.
The usual practice is to remove the output as soon as the input is removed.

Once an input PID has been notified to be removed, it shall no longer be used by the filter, as it may be discarded/freed (PID are NOT reference counted).

 *	@{
 */



/*! Creates a new output PID for the filter. If the filter has a single input PID already connected, the PID properties are copied by default
\param filter the target filter
\return the new output PID
*/
GF_FilterPid *gf_filter_pid_new(GF_Filter *filter);

/*! Removes an output PID from the filter. This will trigger removal of the PID upward in the chain
\param PID the target filter PID to remove
*/
void gf_filter_pid_remove(GF_FilterPid *PID);

/*! Creates an output PID for a raw input filter (file, sockets, pipe, etc). This will assign file name, local name, mime and extension properties to the created PID
\param filter the target filter
\param url URL of the source, SHALL be set
\param local_file path on local host, can be NULL
\param mime_type mime type of the content. If none provided and propbe_data is not null, the data will be probed for mime type resolution
\param fext file extension of the content. If NULL, will be extracted from URL
\param probe_data data of the stream to probe in order to solve its mime type
\param probe_size size of the probe data
\param trust_mime if set and mime_type is set, disables data probing
\param out_pid the output PID to create or update. If no referer PID, a new PID will be created otherwise the PID will be updated
\return error code if any
*/
GF_Err gf_filter_pid_raw_new(GF_Filter *filter, const char *url, const char *local_file, const char *mime_type, const char *fext, u8 *probe_data, u32 probe_size, Bool trust_mime, GF_FilterPid **out_pid);

/*! Sets a new property on an output PID for built-in property names.
Previous properties (ones set before last packet dispatch) will still be valid. Property with same type/name will be reassigned
You need to remove them one by one using \ref gf_filter_pid_set_property with NULL property, or reset the properties with \ref gf_filter_pid_reset_properties.
Setting a new property will trigger a PID reconfigure.

\param PID the target filter PID
\param prop_4cc the built-in property code to modify
\param value the new value to assign, or NULL if the property is to be removed
\return error code if any
*/
GF_Err gf_filter_pid_set_property(GF_FilterPid *PID, u32 prop_4cc, const GF_PropertyValue *value);

/*! Sets a new property on an output PID - see \ref gf_filter_pid_set_property.
\param PID the target filter PID
\param name the name of the property to modify
\param value the new value to assign, or NULL if the property is to be removed
\return error code if any
*/
GF_Err gf_filter_pid_set_property_str(GF_FilterPid *PID, const char *name, const GF_PropertyValue *value);

/*! Sets a new property on an output PID - see \ref gf_filter_pid_set_property.
\param PID the target filter PID
\param name the name of the property to modify. The name will be copied to the property, and memory destruction performed by the filter session
\param value the new value to assign, or NULL if the property is to be removed
\return error code if any
*/
GF_Err gf_filter_pid_set_property_dyn(GF_FilterPid *PID, char *name, const GF_PropertyValue *value);

/*! Sets a new info property on an output PID for built-in property names.
Similar to \ref gf_filter_pid_set_property, but infos are not copied up the chain and to not trigger PID reconfiguration.
First packet dispatched after calling this function will be marked, and its fetching by the consuming filter will trigger a process_event notification.
If the consumming filter copies properties from source packet to output packet, the flag will be passed to such new output packet.
Note: any property type can be used for info, except \ref GF_PROP_POINTER.

\param PID the target filter PID
\param prop_4cc the built-in property code to modify
\param value the new value to assign, or NULL if the property is to be removed
\return error code if any
*/
GF_Err gf_filter_pid_set_info(GF_FilterPid *PID, u32 prop_4cc, const GF_PropertyValue *value);

/*! Sets a new info property on an output PID - see \ref gf_filter_pid_set_info
See  \ref gf_filter_pid_set_info
\param PID the target filter PID
\param name the name of the property to modify
\param value the new value to assign, or NULL if the property is to be removed
\return error code if any
*/
GF_Err gf_filter_pid_set_info_str(GF_FilterPid *PID, const char *name, const GF_PropertyValue *value);

/*! Sets a new property on an output PID.
See \ref gf_filter_pid_set_info
\param PID the target filter PID
\param name the name of the property to modify. The name will be copied to the property, and memory destruction performed by the filter session
\param value the new value to assign, or NULL if the property is to be removed
\return error code if any
*/
GF_Err gf_filter_pid_set_info_dyn(GF_FilterPid *PID, char *name, const GF_PropertyValue *value);

/*! Sets user data pointer for a PID - see \ref gf_filter_pid_set_info
\param PID the target filter PID
\param udta user data pointer
*/
void gf_filter_pid_set_udta(GF_FilterPid *PID, void *udta);

/*! Gets user data pointer for a PID
\param PID the target filter PID
\return udta user data pointer
*/
void *gf_filter_pid_get_udta(GF_FilterPid *PID);

/*! Gets PID name. Mostly used for logging purposes
\param PID the target filter PID
\param name the new PID name. function ignored if NULL.
*/
void gf_filter_pid_set_name(GF_FilterPid *PID, const char *name);

/*! Gets PID name
\param PID the target filter PID
\return PID name
*/
const char *gf_filter_pid_get_name(GF_FilterPid *PID);

/*! Gets filter name of input PID
\param PID the target filter PID
\return name of the filter owning this input PID
*/
const char *gf_filter_pid_get_filter_name(GF_FilterPid *PID);

/*! Gets the source arguments of the PID, walking down the chain until the source filter
\param PID the target filter PID
\return argument of the source filter
*/
const char *gf_filter_pid_orig_src_args(GF_FilterPid *PID);

/*! Gets the source filter name or class name for the PID, walking down the chain until the source filter (ony the first input PID of each filter is used).
\param PID the target filter PID
\return argument of the source filter
*/
const char *gf_filter_pid_get_source_filter_name(GF_FilterPid *PID);

/*! Gets the arguments for the filter
\param PID the target filter PID
\return arguments of the source filter
*/
const char *gf_filter_pid_get_args(GF_FilterPid *PID);

/*! Sets max buffer requirement of an output PID. Typically used by audio to make sure several packets can be dispatched on a PID
that would otherwise block after one packet
\param PID the target filter PID
\param total_duration_us buffer max occupancy in us
*/
void gf_filter_pid_set_max_buffer(GF_FilterPid *PID, u32 total_duration_us);

/*! Returns max buffer requirement of a PID.
\param PID the target filter PID
\return buffer max in us
*/
u32 gf_filter_pid_get_max_buffer(GF_FilterPid *PID);

/*! Checks if a given filter is in the PID parent chain. This is used to identify sources (rather than checking URL/...)
\param PID the target filter PID
\param filter the source filter to check
\return GF_TRUE if filter is a source for that PID, GF_FALSE otherwise
*/
Bool gf_filter_pid_is_filter_in_parents(GF_FilterPid *PID, GF_Filter *filter);

/*! Gets current buffer levels of the PID
\param PID the target filter PID
\param max_units maximum number of packets allowed - can be 0 if buffer is measured in time
\param nb_pck number of packets in PID
\param max_duration maximum buffer duration allowed in us - can be 0 if buffer is measured in units
\param duration buffer duration in us
\return GF_TRUE if normal buffer query, GF_FALSE if final session flush, in which case buffer might never complete
*/
Bool gf_filter_pid_get_buffer_occupancy(GF_FilterPid *PID, u32 *max_units, u32 *nb_pck, u32 *max_duration, u32 *duration);

/*! Sets loose connect for a PID, avoiding to throw an error if connection of the PID fails. Used by the compositor filter which acts as both sink and filter.
\param PID the target filter PID
*/
void gf_filter_pid_set_loose_connect(GF_FilterPid *PID);


/*! Negotiate a given property on an input PID for built-in properties
Filters may accept some PID connection but may need an adaptaion chain to be able to process packets, eg change pixel format or sample rate
This function will trigger a reconfiguration of the filter chain to try to adapt this. If failing, the filter chain will disconnect
This process is asynchronous, the filter asking for a PID negociation will see the notification through a pid_reconfigure if success.
\param PID the target filter PID - this MUST be an input PID
\param prop_4cc the built-in property code to negotiate
\param value the new value to negotiate, SHALL NOT be NULL
\return error code if any
*/
GF_Err gf_filter_pid_negociate_property(GF_FilterPid *PID, u32 prop_4cc, const GF_PropertyValue *value);

/*! Negotiate a given property on an input PID for regular properties
see \ref gf_filter_pid_negociate_property
\param PID the target filter PID - this MUST be an input PID
\param name name of the property to negotiate
\param value the new value to negotiate, SHALL NOT be NULL
\return error code if any
*/
GF_Err gf_filter_pid_negociate_property_str(GF_FilterPid *PID, const char *name, const GF_PropertyValue *value);

/*! Negotiate a given property on an input PID for regular properties
see \ref gf_filter_pid_negociate_property
\param PID the target filter PID - this MUST be an input PID
\param name the name of the property to modify. The name will be copied to the property, and memory destruction performed by the filter session
\param value the new value to negotiate, SHALL NOT be NULL
\return error code if any
*/
GF_Err gf_filter_pid_negociate_property_dyn(GF_FilterPid *PID, char *name, const GF_PropertyValue *value);

/*! Queries a negotiated built-in capability on an output PID
Filters may check if a property negotiation was done on an output PID, and check the property value.
This can be done on an output PID in a filter->reconfigure_output if the filter accpets caps negotiation
This can be done on an input PID in a generic reconfigure_pid

\param PID the target filter PID
\param prop_4cc the built-in property code to negotiate
\return the negociated property value
*/
const GF_PropertyValue *gf_filter_pid_caps_query(GF_FilterPid *PID, u32 prop_4cc);

/*! Queries a negotiated capability on an output PID - see \ref gf_filter_pid_caps_query
\param PID the target filter PID
\param prop_name the property name to negotiate
\return the negociated property value
*/
const GF_PropertyValue *gf_filter_pid_caps_query_str(GF_FilterPid *PID, const char *prop_name);

/*! Statistics for PID*/
typedef struct
{
	/*! if set, indicates the PID is disconnected and stats are not valid*/
	u32 disconnected;
	/*! average process rate on that PID in bits per seconds*/
	u32 average_process_rate;
	/*! max process rate on that PID in bits per seconds*/
	u32 max_process_rate;
	/*! average bitrate for that PID*/
	u32 avgerage_bitrate;
	/*! max bitrate for that PID*/
	u32 max_bitrate;
	/*! number of packets processed on that PID*/
	u32 nb_processed;
	/*! max packet process time of the filter in us*/
	u32 max_process_time;
	/*! total process time of the filter in us*/
	u64 total_process_time;
	/*! process time of first packet of the filter in us*/
	u64 first_process_time;
	/*! process time of the last packet on that PID in us*/
	u64 last_process_time;
	/*! minimum frame duration on that PID in us*/
	u32 min_frame_dur;
	/*! number of saps 1/2/3 on that PID*/
	u32 nb_saps;
	/*! max process time of SAP packets on that PID in us*/
	u32 max_sap_process_time;
	/*! total process time of SAP packets on that PID in us*/
	u64 total_sap_process_time;

	/*! max buffer time in us - only set when querying decoder stats*/
	u64 max_buffer_time;
	/*! max playout buffer time in us - only set when querying decoder stats*/
	u64 max_playout_time;
	/*! min playout buffer time in us - only set when querying decoder stats*/
	u64 min_playout_time;
	/*! current buffer time in us - only set when querying decoder stats*/
	u64 buffer_time;
	/*! number of units in input buffer of the filter - only set when querying decoder stats*/
	u32 nb_buffer_units;
} GF_FilterPidStatistics;

/*! Direction for stats querying*/
typedef enum
{
	/*! statistics are fetched on the current PID's parent filter. If the PID is an output PID, the statistics are fetched on all the destinations for that PID*/
	GF_STATS_LOCAL = 0,
	/*! statistics are fetched on the current PID's parent filter. The statistics are fetched on all input of the parent filter*/
	GF_STATS_LOCAL_INPUTS,
	/*! statistics are fetched on all inputs of the next decoder filter up the chain (towards the sink)*/
	GF_STATS_DECODER_SINK,
	/*! statistics are fetched on all inputs of the previous decoder filter down the chain (towards the source)*/
	GF_STATS_DECODER_SOURCE,
	/*! statistics are fetched on all inputs of the next encoder filter up the chain (towards the sink)*/
	GF_STATS_ENCODER_SINK,
	/*! statistics are fetched on all inputs of the previous encoder filter down the chain (towards the source)*/
	GF_STATS_ENCODER_SOURCE
} GF_FilterPidStatsLocation;

/*! Gets statistics for the PID
\param PID the target filter PID
\param stats the retrieved statistics
\param location indicates where to locate the filter to query stats on it.
\return error code if any
*/
GF_Err gf_filter_pid_get_statistics(GF_FilterPid *PID, GF_FilterPidStatistics *stats, GF_FilterPidStatsLocation location);

/*! Resets current properties of the PID
\param PID the target filter PID
\return error code if any
*/
GF_Err gf_filter_pid_reset_properties(GF_FilterPid *PID);


/*! Function protoype for filtering properties.
\param cbk callback data
\param prop_4cc the built-in property code
\param prop_name property name
\param src_prop the property value in the source packet
\return GF_TRUE if the property shall be merged, GF_FALSE otherwise
*/
typedef Bool (*gf_filter_prop_filter)(void *cbk, u32 prop_4cc, const char *prop_name, const GF_PropertyValue *src_prop);

/*! Push a new set of properties on destination PID using all properties from source PID. Old properties in destination will be lost (i.e. reset properties is always performed during copy properties)
\param dst_pid the destination filter PID
\param src_pid the source filter PID
\return error code if any
*/
GF_Err gf_filter_pid_copy_properties(GF_FilterPid *dst_pid, GF_FilterPid *src_pid);

/*! Push a new set of properties on destination PID, using all properties from source PID.
Old properties of the destination are first copied to the new property set before copying the ones from the source PID, potentially filtering them.
\param dst_pid the destination filter PID
\param src_pid the source filter PID
\param filter_prop callback filtering function
\param cbk callback data passed to the callback function
\return error code if any
*/
GF_Err gf_filter_pid_merge_properties(GF_FilterPid *dst_pid, GF_FilterPid *src_pid, gf_filter_prop_filter filter_prop, void *cbk );

/*! Gets a built-in property of the PID
Warning: properties are only valid until the next configure_pid is called. Attempting to use a property
value (either the pointer or one of the value) queried before the current configure_pid will result in
 unpredictable behaviour, potentially crashes.
\param PID the target filter PID
\param prop_4cc the code of the built-in property to retrieve
\return the property if found or NULL otherwise
*/
const GF_PropertyValue *gf_filter_pid_get_property(GF_FilterPid *PID, u32 prop_4cc);

/*! Gets a property of the PID - see \ref gf_filter_pid_get_property
\param PID the target filter PID
\param prop_name the name of the property to retrieve
\return the property if found or NULL otherwise
*/
const GF_PropertyValue *gf_filter_pid_get_property_str(GF_FilterPid *PID, const char *prop_name);

/*! Enumerates properties of a PID - see \ref gf_filter_pid_get_property
\param PID the target filter PID
\param idx input/output index of the current property. 0 means first. Incremented by 1 upon success
\param prop_4cc set to the built-in code of the property if built-in
\param prop_name set to the name of the property if not built-in
\return the property if found or NULL otherwise (end of enumeration)
*/
const GF_PropertyValue *gf_filter_pid_enum_properties(GF_FilterPid *PID, u32 *idx, u32 *prop_4cc, const char **prop_name);

/*! Enumerates info of a PID
\param PID the target filter PID
\param idx input/output index of the current info. 0 means first. Incremented by 1 upon success
\param prop_4cc set to the built-in code of the info if built-in
\param prop_name set to the name of the info if not built-in
\return the property if found or NULL otherwise (end of enumeration)
*/
const GF_PropertyValue *gf_filter_pid_enum_info(GF_FilterPid *PID, u32 *idx, u32 *prop_4cc, const char **prop_name);

/*! Sets PID framing mode. filters can consume packets as they arrive, or may want to only process full frames/files
\param PID the target filter PID
\param requires_full_blocks if GF_TRUE, the packets on the PID will be reaggregated to form complete frame/files.
\return error code if any
*/
GF_Err gf_filter_pid_set_framing_mode(GF_FilterPid *PID, Bool requires_full_blocks);

/*! Gets cumulated buffer duration of PID (recursive until source)
\param PID the target filter PID
\param check_pid_full if GF_TRUE, returns 0 if the PID buffer is not yet full
\return the duration in us, or -1 if session is in final flush
*/
u64 gf_filter_pid_query_buffer_duration(GF_FilterPid *PID, Bool check_pid_full);

/*! Try to force a synchronous flush of the filter chain downwards this PID. If refetching a packet returns NULL, this failed.
\param PID the target filter PID
*/
void gf_filter_pid_try_pull(GF_FilterPid *PID);


/*! Looks for a built-in property value on a  PIDs. This is a recursive call on input chain
Info query is NOT threadsafe in gpac, you
Properties retrieved shall be released using \ref gf_filter_release_property. See \ref gf_filter_pid_get_info for more details.
\param PID the target filter PID to query
\param prop_4cc the code of the built-in property to fetch
\param propentry the property reference object for later release. See \ref gf_filter_pid_get_info for more details.
\return the property if found NULL otherwise
*/
const GF_PropertyValue *gf_filter_pid_get_info(GF_FilterPid *PID, u32 prop_4cc, GF_PropertyEntry **propentry);

/*! Looks for a property value on a  PIDs. This is a recursive call on both input and ouput chain
Properties retrieved shall be released using \ref gf_filter_release_property. See \ref gf_filter_pid_get_info for more details.
\param PID the target filter PID to query
\param prop_name the name of the property to fetch
\param propentry the property reference object for later release. See \ref gf_filter_pid_get_info for more details.
\return the property if found NULL otherwise
*/
const GF_PropertyValue *gf_filter_pid_get_info_str(GF_FilterPid *PID, const char *prop_name, GF_PropertyEntry **propentry);


/*! Signals end of stream on a PID. Each filter needs to call this when EOS is reached on a given stream since there is no explicit link between input PIDs and output PIDs
\param PID the target filter PID
*/
void gf_filter_pid_set_eos(GF_FilterPid *PID);

/*! Checks for end of stream signaling on a PID input chain.
This is a recursive call on input chain
\param PID the target filter PID
\return GF_TRUE if end of stream was signaled on the input chain
*/
Bool gf_filter_pid_has_seen_eos(GF_FilterPid *PID);

/*! Checks for end of stream signaling on a PID.
\param PID the target filter PID
\return GF_TRUE if end of stream was signaled on that PID
*/
Bool gf_filter_pid_is_eos(GF_FilterPid *PID);

/*! Checks if there is a packet ready on an input PID.
\param PID the target filter PID
\return GF_TRUE if no packet in buffers
*/
Bool gf_filter_pid_first_packet_is_empty(GF_FilterPid *PID);

/*! Gets the first packet in the input PID buffer.
This may trigger a reconfigure signal on the filter. If reconfigure is not OK, returns NULL and the PID passed to the filter NO LONGER EXISTS (implicit remove)
The packet is still present in the PID buffer until explicitly removed by \ref gf_filter_pid_drop_packet
\param PID the target filter PID
\return packet or NULL of empty or reconfigure error
*/
GF_FilterPacket * gf_filter_pid_get_packet(GF_FilterPid *PID);

/*! Fetches the CTS of the first packet in the input PID buffer.
This may trigger a reconfigure signal on the filter. If reconfigure is not OK, returns NULL and the PID passed to the filter NO LONGER EXISTS (implicit remove)
\param PID the target filter PID
\param cts set to the composition time of the first packet, in PID timescale
\return GF_TRUE if cts was fetched, GF_FALSE otherwise
*/
Bool gf_filter_pid_get_first_packet_cts(GF_FilterPid *PID, u64 *cts);

/*! Drops the first packet in the input PID buffer.
\param PID the target filter PID
*/
void gf_filter_pid_drop_packet(GF_FilterPid *PID);

/*! Gets the number of packets in input PID buffer.
\param PID the target filter PID
\return the number of packets
*/
u32 gf_filter_pid_get_packet_count(GF_FilterPid *PID);

/*! Checks the capability of the input PID match its destination filter.
\param PID the target filter PID
\return GF_TRUE if match , GF_FALSE otherwise
*/
Bool gf_filter_pid_check_caps(GF_FilterPid *PID);

/*! Checks if the PID would enter a blocking state if a new packet is sent.
This function should be called by eg demuxers to regulate the rate at which they send packets

Note: PIDs are never fully blocking in GPAC, a filter requesting an output packet should usually get one unless something goes wrong
\param PID the target filter PID
\return GF_TRUE if PID would enter blocking state , GF_FALSE otherwise
*/
Bool gf_filter_pid_would_block(GF_FilterPid *PID);

/*! Flags for argument update event*/
enum
{
	/*! the update event can be sent down the source chain*/
	GF_FILTER_UPDATE_DOWNSTREAM = 1<<1,
	/*! the update event can be sent up the filter chain*/
	GF_FILTER_UPDATE_UPSTREAM = 1<<2,
};

/*! Shortcut to access the timescale of the PID - faster than get property as the timescale is locally cached for buffer management
\param PID the target filter PID
\return the PID timescale
*/
u32 gf_filter_pid_get_timescale(GF_FilterPid *PID);

/*! Clears the end of stream flag on a PID.
Note: the end of stream is automatically cleared when a new packet is dispatched; This function is used to clear it asap, before next packet dispacth (period switch in dash for example).
\param PID the target filter PID
\param all_pids if sets, clear end oof stream for all PIDs coming from the same filter as the target PID
*/
void gf_filter_pid_clear_eos(GF_FilterPid *PID, Bool all_pids);

/*! Indicates how clock references (PCR of MPEG-2) should be handled.
By default these references are passed from input packets to output packets by the filter session (this assumes the filter doesn't modify composition timestamps).
This default can be changed with this function.
\param PID the target filter PID
\param filter_in_charge if set to GF_TRUE, clock references are not forwarded by the filter session and the filter is in charge of handling them
*/
void gf_filter_pid_set_clock_mode(GF_FilterPid *PID, Bool filter_in_charge);

/*! Resolves file template using PID properties and file index. Templates follow the DASH mechanism:

- $KEYWORD$ or $KEYWORD%0nd$ are replaced in the template with the resolved value,

- $$ is an escape for $

Supported KEYWORD (case insensitive):
- num: replaced by file_number (usually matches GF_PROP_PCK_FILENUM, but this property is not used in the solving mechanism)
- PID: ID of the source PID
- URL: URL of source file
- File: path on disk for source file
- p4cc=ABCD: uses PID property with 4CC ABCD
- pname=VAL: uses PID property with name VAL (either built-in prop name or other peroperty name)

\param PID the target filter PID
\param szTemplate source template to solve
\param szFinalName buffer for final name
\param file_number number of file to use
\param file_suffix if not null, will be appended after the value of the §File$ keyword if present
\return error if any
*/
GF_Err gf_filter_pid_resolve_file_template(GF_FilterPid *PID, char szTemplate[GF_MAX_PATH], char szFinalName[GF_MAX_PATH], u32 file_number, const char *file_suffix);


/*! Sets discard mode on or off on an input PID. When discard is on, all input packets for this PID are no longer dispatched.

This only affect the current PID, not the source filter(s) for that PID.

PID reconfigurations are still forwarded to the filter, so that a filter may decide to re-enable regular mode.

This is typically needed for filters that stop consuming data for a while (dash forced period duration for example) but may resume
consumption later on (stream moving from period 1 to period 2 for example).

\param PID the target filter PID
\param discard_on enables/disables discard
\return error if any
*/
GF_Err gf_filter_pid_set_discard(GF_FilterPid *PID, Bool discard_on);

/*! Discard blocking mode for PID on end of stream. The filter is blocked when all output PIDs are in end of stream, this function unblocks the filter.
This can be needed for playlist type filters dispatching end of stream at the end of each file but setting up next file in
the following process() call.

\param PID the target filter PID
*/
void gf_filter_pid_discard_block(GF_FilterPid *PID);

/*! Force a built-in cap props on the output PID.
A PID may hold one forced cap at most. When set it acts as a virtual property checked against filter caps.
This is currently only used by DASH segmenter to enforce loading muxers with dashing capability - likely to change in the near future.
\param PID the target filter PID
\param cap_4cc of built-in property to force in linked filters
\return error if any
*/
GF_Err gf_filter_pid_force_cap(GF_FilterPid *PID, u32 cap_4cc);

/*! Gets URL argument of first destination of PID if any - memory shall be freed by caller.
\param PID the target filter PID
\return destination URL string or NULL if error
*/
char *gf_filter_pid_get_destination(GF_FilterPid *PID);

/*! Gets URL  argument of first source of PID if any - memory shall be freed by caller.
\param PID the target filter PID
\return source URL string or NULL if error
*/
char *gf_filter_pid_get_source(GF_FilterPid *PID);

/*! Indicates that this output PID requires a sourceID on the destination filter to be present. This prevents trying to link to other filters with no source IDs but
accepting the PID
\param PID the target filter PID
\return error code if any
*/
GF_Err gf_filter_pid_require_source_id(GF_FilterPid *PID);

/*! Enables decoding time reconstruction on PID for packets with DTS not set. If not enabled (default), dts not set implies dts = cts
\param PID the target filter PID
\param do_recompute if set, dts will be recomputed when not set
*/
void gf_filter_pid_recompute_dts(GF_FilterPid *PID, Bool do_recompute);

/*! Queries minimum packet duration as computed from DTS/CTS info on the PID
\param PID the target filter PID
\return minimum packet duration computed
*/
u32 gf_filter_pid_get_min_pck_duration(GF_FilterPid *PID);


/*! Sends an event down the filter chain for input PID, or up the filter chain for output PIDs.
\param PID the target filter PID
\param evt the event to send
*/
void gf_filter_pid_send_event(GF_FilterPid *PID, GF_FilterEvent *evt);


/*! Helper function to init play events, that checks the PID \ref GF_FilterPidPlaybackMode and adjust start/speed accordingly. This does not send the event.
\param PID the target filter PID
\param evt the event to initialize
\param start playback start time of request
\param speed playback speed time of request
\param log_name name used for logs in case of capability mismatched
*/
void gf_filter_pid_init_play_event(GF_FilterPid *PID, GF_FilterEvent *evt, Double start, Double speed, const char *log_name);

/*! Check if the given PID is marked as playing or not.
\param PID the target filter PID
\return GF_TRUE if PID is currently playing, GF_FALSE otherwise
*/
Bool gf_filter_pid_is_playing(GF_FilterPid *PID);

/*! Enables direct dispatch of packets to connected filters. This mode is usefull when a filter may send a very large number of packets
in one process() call; this is for example the case of the isobmff muxer in interleave mode. Using this mode avoids overloading
the PID buffer with packets.
If the session is multi-threaded, this parameter has no effect.
\param PID the target filter PID
\return GF_TRUE if PID is currently playing, GF_FALSE otherwise
*/
GF_Err gf_filter_pid_allow_direct_dispatch(GF_FilterPid *PID);

/*! @} */


/*!
 *\addtogroup fs_pck Filter Packet
 *\ingroup filters_grp
 *\brief Filter data exchange

 Packets consist in block of data or reference to such blocks, passed from the source to the sink only.
Internally, a packet created by a filter (output packet) is different from an input packet to a filter (\ref gf_filter_pid_get_packet)
but the API has been designed to hide this, so that most packet functions can be called regardless of the input/output nature of the PID.

Packets have native attributes (timing, sap state, ...) but may also have any number of properties associated to them.

The generic design of the architecture is that each filter is free to decide how it handle PIDs and their packets. This implies that the filter session has no clue how
an output packet relates to an input packet.
Developpers must therefore manually copy packet properties that seem relevant, or more practically copy all properties from input packet to output packet and
reassign output packet properties changed by the filter.

In order to handle reordering of packets, it is possible to keep references to either packets (may block the filter chain), or packet properties.

Packets shall always be dispatched in their processing order (decode order). If reordering upon reception is needed, or AU interleaving is used, a filter SHALL do the reordering.
However, packets do not have to be send in their creation order: a created packet is not assigned to PID buffers until it is send.

 *	@{
 */


/*! Keeps a reference to the given input packet. The packet shall be released at some point using \ref gf_filter_pck_unref
\param pck the target input packet
\return error if any
*/
GF_Err gf_filter_pck_ref(GF_FilterPacket **pck);

/*! Remove a reference to the given input packet. The packet might be destroyed after that call.
\param pck the target input packet
*/
void gf_filter_pck_unref(GF_FilterPacket *pck);

/*! Creates a reference to the packet properties, but not to the data.
This is mostly usefull for encoders/decoders/filters with delay, where the input packet needs to be released before getting the corresponding output (frame reordering & co).
This allows merging back packet properties after some delay without blocking the filter chain.
\param pck the target input packet
\return error if any
*/
GF_Err gf_filter_pck_ref_props(GF_FilterPacket **pck);


/*! Allocates a new packet on the output PID with associated allocated data.
The packet has by default no DTS, no CTS, no duration framing set to full frame (start=end=1) and all other flags set to 0 (including SAP type).
\param PID the target output PID
\param data_size the desired size of the packet - can be changed later
\param data set to the writable buffer of the created packet
\return new packet or NULL if error
*/
GF_FilterPacket *gf_filter_pck_new_alloc(GF_FilterPid *PID, u32 data_size, u8 **data);


/*! Allocates a new packet on the output PID referencing internal data.
The packet has by default no DTS, no CTS, no duration framing set to full frame (start=end=1) and all other flags set to 0 (including SAP type).
\param PID the target output PID
\param data the data block to dispatch
\param data_size the size of the data block to dispatch
\param destruct the callback function used to destroy the packet when no longer used - may be NULL
\return new packet or NULL if error
*/
GF_FilterPacket *gf_filter_pck_new_shared(GF_FilterPid *PID, const u8 *data, u32 data_size, gf_fsess_packet_destructor destruct);

/*! Allocates a new packet on the output PID referencing data of some input packet.
The packet has by default no DTS, no CTS, no duration framing set to full frame (start=end=1) and all other flags set to 0 (including SAP type).
\param PID the target output PID
\param data the data block to dispatch - if NULL, the entire data of the source packet is used
\param data_size the size of the data block to dispatch - if 0, the entire data of the source packet is used
\param source_packet the source packet this data belongs to (at least from the filter point of view).
\return new packet or NULL if error
*/
GF_FilterPacket *gf_filter_pck_new_ref(GF_FilterPid *PID, const u8 *data, u32 data_size, GF_FilterPacket *source_packet);

/*! Allocates a new packet on the output PID with associated allocated data.
The packet has by default no DTS, no CTS, no duration framing set to full frame (start=end=1) and all other flags set to 0 (including SAP type).
\param PID the target output PID
\param data_size the desired size of the packet - can be changed later
\param data set to the writable buffer of the created packet
\param destruct the callback function used to destroy the packet when no longer used - may be NULL
\return new packet or NULL if error
*/
GF_FilterPacket *gf_filter_pck_new_alloc_destructor(GF_FilterPid *PID, u32 data_size, u8 **data, gf_fsess_packet_destructor destruct);

/*! Clones a new packet from a source packet.
If the source packet uses a frame interface object or has no associated data, returns NULL.
If the source packet is referenced more than once (ie more than just the caller), a new packet on the output PID is allocated with identical data (alloc and copy) than the source.
Otherwise, the source data is assigned to the output packet.
 This is typically called by filter wishing to perform in-place processing of input data.
\param PID the target output PID
\param pck_source the desired source packet to clone
\param data set to the writable buffer of the created packet
\return new packet or NULL if error
*/
GF_FilterPacket *gf_filter_pck_new_clone(GF_FilterPid *PID, GF_FilterPacket *pck_source, u8 **data);


/*! Marks memory of a shared packet as non-writable. By default \ref gf_filter_pck_new_shared and \ref gf_filter_pck_new_ref allow
write access to internal memory in case the packet can be cloned (single reference used). If your filter relies on the content of the shared
memory for its internal state, packet must be marked as read-only to avoid later state corruption.
Note that packets created with \ref gf_filter_pck_new_frame_interface are always treated as read-only packets
\param pck the target output packet to send
\return error if any
*/
GF_Err gf_filter_pck_set_readonly(GF_FilterPacket *pck);

/*! Sends the packet on its output PID. Packets SHALL be sent in processing order (eg, decoding order for video).
However, packets don't have to be sent in their allocation order.
\param pck the target output packet to send
*/
GF_Err gf_filter_pck_send(GF_FilterPacket *pck);

/*! Destructs a packet allocated but that cannot be sent. Shall not be used on packet references.
\param pck the target output packet to send
*/
void gf_filter_pck_discard(GF_FilterPacket *pck);

/*! Destructs a packet allocated but that cannot be sent. Shall not be used on packet references.
This is a shortcut to \ref gf_filter_pck_new_ref + \ref gf_filter_pck_merge_properties + \ref gf_filter_pck_send
\param reference the input packet to forward
\param PID the output PID to forward to
\return error code if any
*/
GF_Err gf_filter_pck_forward(GF_FilterPacket *reference, GF_FilterPid *PID);

/*! Gets data associated with the packet.
\param pck the target packet
\param size set to the packet data size
\return packet data if any, NULL if empty or if the packet uses a frame interface object. see \ref gf_filter_pck_get_frame_interface
*/
const u8 *gf_filter_pck_get_data(GF_FilterPacket *pck, u32 *size);

/*! Sets a built-in property of a packet
\param pck the target packet
\param prop_4cc the code of the built-in property to set
\param value the property value to set
\return error code if any
*/
GF_Err gf_filter_pck_set_property(GF_FilterPacket *pck, u32 prop_4cc, const GF_PropertyValue *value);

/*! Sets a property of a packet
\param pck the target packet
\param name the name of the property to set
\param value the property value to set
\return error code if any
*/
GF_Err gf_filter_pck_set_property_str(GF_FilterPacket *pck, const char *name, const GF_PropertyValue *value);

/*! Sets a property of a packet
\param pck the target packet
\param name the code of the property to set. The name will be copied to the property, and memory destruction performed by the filter session
\param value the property value to set
\return error code if any
*/
GF_Err gf_filter_pck_set_property_dyn(GF_FilterPacket *pck, char *name, const GF_PropertyValue *value);

/*! Merge properties of source packet into destination packet but does NOT reset destination packet properties
\param pck_src source packet
\param pck_dst destination packet
\return error code if any
*/
GF_Err gf_filter_pck_merge_properties(GF_FilterPacket *pck_src, GF_FilterPacket *pck_dst);

/*! Same as \ref gf_filter_pck_merge_properties but uses a filter callback to select properties to merge
\param pck_src source packet
\param pck_dst destination packet
\param filter_prop callback filtering function
\param cbk callback data passed to the callback function
\return error code if any
*/
GF_Err gf_filter_pck_merge_properties_filter(GF_FilterPacket *pck_src, GF_FilterPacket *pck_dst, gf_filter_prop_filter filter_prop, void *cbk);

/*! Gets built-in property of packet.
\param pck target packet
\param prop_4cc the code of the built-in property
\return the property if found, NULL otherwise
*/
const GF_PropertyValue *gf_filter_pck_get_property(GF_FilterPacket *pck, u32 prop_4cc);

/*! Gets property of packet.
\param pck target packet
\param prop_name the name of the property
\return the property if found, NULL otherwise
*/
const GF_PropertyValue *gf_filter_pck_get_property_str(GF_FilterPacket *pck, const char *prop_name);

/*! Enumerates properties on packets.
\param pck target packet
\param idx input/output index of the current property. 0 means first. Incremented by 1 upon success
\param prop_4cc set to the code of the built-in property
\param prop_name set to the name of the property
\return the property if found, NULL otherwise
*/
const GF_PropertyValue *gf_filter_pck_enum_properties(GF_FilterPacket *pck, u32 *idx, u32 *prop_4cc, const char **prop_name);


/*! Sets packet framing info. A full frame is a complete entity for the stream type, ie an access unit for media streams and a complete file for file streams
\param pck target packet
\param is_start packet is start of the frame
\param is_end packet is end of the frame
\return error code if any
*/
GF_Err gf_filter_pck_set_framing(GF_FilterPacket *pck, Bool is_start, Bool is_end);

/*! Gets packet framing info. A full frame is a complete entity for the stream type, ie an access unit for media streams and a complete file for file streams
\param pck target packet
\param is_start set to GF_TRUE if packet is start of the frame, to GF_FALSE otherwise
\param is_end set to GF_TRUE if packet is end of the frame, to GF_FALSE otherwise
\return error code if any
*/
GF_Err gf_filter_pck_get_framing(GF_FilterPacket *pck, Bool *is_start, Bool *is_end);

/*! Sets Decoding Timestamp (DTS) of the packet. Do not set if unknown - automatic packet duration is based on DTS diff if DTS is present, otherwise in CTS diff.
\param pck target packet
\param dts decoding timestamp of packet, in PID timescale units
\return error code if any
*/
GF_Err gf_filter_pck_set_dts(GF_FilterPacket *pck, u64 dts);

/*! Gets Decoding Timestamp (DTS) of the packet.
\param pck target packet
\return dts decoding timestamp of packet, in PID timescale units
*/
u64 gf_filter_pck_get_dts(GF_FilterPacket *pck);

/*! Sets Composition Timestamp (CTS) of the packet. Do not set if unknown - automatic packet duration is based on DTS diff if DTS is present, otherwise in CTS diff.
\param pck target packet
\param cts composition timestamp of packet, in PID timescale units
\return error code if any
*/
GF_Err gf_filter_pck_set_cts(GF_FilterPacket *pck, u64 cts);

/*! Gets Composition Timestamp (CTS) of the packet.
\param pck target packet
\return cts composition timestamp of packet, in PID timescale units
*/
u64 gf_filter_pck_get_cts(GF_FilterPacket *pck);

/*! Returns packet timescale (same as PID timescale)
\param pck target packet
\return packet timescale
*/
u32 gf_filter_pck_get_timescale(GF_FilterPacket *pck);

/*! Sets packet duration
\param pck target packet
\param duration duration of packet, in PID timescale units
\return error code if any
*/
GF_Err gf_filter_pck_set_duration(GF_FilterPacket *pck, u32 duration);

/*! Gets packet duration
\param pck target packet
\return duration duration of packet, in PID timescale units
*/
u32 gf_filter_pck_get_duration(GF_FilterPacket *pck);

/*! reallocates packet not yet sent. Returns data start and new range of data. This will reset byte offset information to not available.
\param pck target packet
\param nb_bytes_to_add bytes to add to packet
\param data_start realloc pointer of packet data start - shall not be NULL
\param new_range_start pointer to new (apppended space) data - shall not be NULL
\param new_size full size of allocated block. - shall not be NULL
\return error code if any
*/
GF_Err gf_filter_pck_expand(GF_FilterPacket *pck, u32 nb_bytes_to_add, u8 **data_start, u8 **new_range_start, u32 *new_size);

/*! Truncates packet not yet sent to given size
\param pck target packet
\param size new size to truncate to
\return error code if any
*/
GF_Err gf_filter_pck_truncate(GF_FilterPacket *pck, u32 size);

/*! SAP types as defined in annex I of ISOBMFF */
typedef enum
{
	/*! no SAP */
	GF_FILTER_SAP_NONE = 0,
	/*! closed gop no leading */
	GF_FILTER_SAP_1,
	/*! closed gop leading */
	GF_FILTER_SAP_2,
	/*! open gop */
	GF_FILTER_SAP_3,
	/*! GDR */
	GF_FILTER_SAP_4,
} GF_FilterSAPType;

/*! Sets packet SAP type
\param pck target packet
\param sap_type SAP type of the packet
\return error code if any
*/
GF_Err gf_filter_pck_set_sap(GF_FilterPacket *pck, GF_FilterSAPType sap_type);

/*! Sets packet SAP type
\param pck target packet
\return sap_type SAP type of the packet
*/
GF_FilterSAPType gf_filter_pck_get_sap(GF_FilterPacket *pck);


/*! Sets packet video interlacing flag
\param pck target packet
\param is_interlaced set to 0 if not interlaced, 1 for top field first/contains only top field, 2 for bottom field first/contains only bottom field.
\return error code if any
*/
GF_Err gf_filter_pck_set_interlaced(GF_FilterPacket *pck, u32 is_interlaced);

/*! Gets packet video interlacing flag
\param pck target packet
\return interlaced flag, set to 0 if not interlaced, 1 for top field first, 2 otherwise.
*/
u32 gf_filter_pck_get_interlaced(GF_FilterPacket *pck);

/*! Sets packet corrupted data flag
\param pck target packet
\param is_corrupted indicates if data in packet is corrupted
\return error code if any
*/
GF_Err gf_filter_pck_set_corrupted(GF_FilterPacket *pck, Bool is_corrupted);

/*! Gets packet corrupted data flag
\param pck target packet
\return GF_TRUE if data in packet is corrupted
*/
Bool gf_filter_pck_get_corrupted(GF_FilterPacket *pck);

/*! Sets seek flag of packet.
For PIDs of stream type FILE with GF_PROP_PID_DISABLE_PROGRESSIVE set, the seek flag indicates
that the packet is a PATCH packet, replacing bytes located at gf_filter_pck_get_byte_offset in file.
If the corrupted flag is set, this indicates the data will be replaced later on.
A seek packet is not meant to be displayed but need for decoding.
\param pck target packet
\param is_seek indicates packet is seek frame
\return error code if any
*/
GF_Err gf_filter_pck_set_seek_flag(GF_FilterPacket *pck, Bool is_seek);

/*! Gets packet seek data flag
\param pck target packet
\return GF_TRUE if packet is a seek packet
*/
Bool gf_filter_pck_get_seek_flag(GF_FilterPacket *pck);

/*! Sets packet byte offset in source.
byte offset should only be set if the data in the packet is exactly the same as the one at the given byte offset
\param pck target packet
\param byte_offset indicates the byte offset in the source. By default packets are created with no byte offset
\return error code if any
*/
GF_Err gf_filter_pck_set_byte_offset(GF_FilterPacket *pck, u64 byte_offset);

/*! Sets packet byte offset in source
\param pck target packet
\return  byte offset in the source
*/
u64 gf_filter_pck_get_byte_offset(GF_FilterPacket *pck);

/*! Sets packet roll info (number of packets to rewind/forward and decode to get a clean access point).
\param pck target packet
\param roll_count indicates the roll distance of this packet - only used for SAP 4 for now
\return error code if any
*/
GF_Err gf_filter_pck_set_roll_info(GF_FilterPacket *pck, s16 roll_count);

/*! Gets packet roll info.
\param pck target packet
\return roll distance of this packet
\return error code if any
*/
s16 gf_filter_pck_get_roll_info(GF_FilterPacket *pck);

/*! Crypt flags for packet */
#define GF_FILTER_PCK_CRYPT 1

/*! Sets packet crypt flags
\param pck target packet
\param crypt_flag packet crypt flag
\return error code if any
*/
GF_Err gf_filter_pck_set_crypt_flags(GF_FilterPacket *pck, u8 crypt_flag);

/*! Gets packet crypt flags
\param pck target packet
\return packet crypt flag
*/
u8 gf_filter_pck_get_crypt_flags(GF_FilterPacket *pck);

/*! Packet clock reference types - used for MPEG-2 TS*/
typedef enum
{
	/*! packet is not a clock reference */
	GF_FILTER_CLOCK_NONE=0,
	/*! packet is a PCR clock reference, expressed in PID timescale */
	GF_FILTER_CLOCK_PCR,
	/*! packet is a PCR clock discontinuity, expressed in PID timescale */
	GF_FILTER_CLOCK_PCR_DISC,
} GF_FilterClockType;

/*! Sets packet clock type
\param pck target packet
\param ctype packet clock flag
\return error code if any
*/
GF_Err gf_filter_pck_set_clock_type(GF_FilterPacket *pck, GF_FilterClockType ctype);

/*! Gets last clock type and clock value on PID. Always returns 0 if the source filter manages clock references internally cd \ref gf_filter_pid_set_clock_mode.
\param PID target PID to query for clock
\param clock_val last clock reference found
\param timescale last clock reference timescale
\return ctype packet clock flag
*/
GF_FilterClockType gf_filter_pid_get_clock_info(GF_FilterPid *PID, u64 *clock_val, u32 *timescale);

/*! Gets clock type of packet. Always returns 0 if the source filter does NOT manages clock references internally cd \ref gf_filter_pid_set_clock_mode.
\param pck target packet
\return ctype packet clock flag
*/
GF_FilterClockType gf_filter_pck_get_clock_type(GF_FilterPacket *pck);

/*! Sets packet carrousel info
\param pck target packet
\param version_number carrousel version number associated with this data chunk
\return error code if any
*/
GF_Err gf_filter_pck_set_carousel_version(GF_FilterPacket *pck, u8 version_number);

/*! Gets packet carrousel info
\param pck target packet
\return version_number carrousel version number associated with this data chunk
*/
u8 gf_filter_pck_get_carousel_version(GF_FilterPacket *pck);

/*! Sets packet sample dependency flags.

Sample dependency flags have the same semantics as ISOBMFF:\n
bit(2)is_leading bit(2)sample_depends_on (2)sample_is_depended_on (2)sample_has_redundancy\n
\n
is_leading takes one of the following four values:\n
0: the leading nature of this sample is unknown;\n
1: this sample is a leading sample that has a dependency before the referenced I-picture (and is therefore not decodable);\n
2: this sample is not a leading sample;\n
3: this sample is a leading sample that has no dependency before the referenced I-picture (and is therefore decodable);\n
sample_depends_on takes one of the following four values:\n
0: the dependency of this sample is unknown;\n
1: this sample does depend on others (not an I picture);\n
2: this sample does not depend on others (I picture);\n
3: reserved\n
sample_is_depended_on takes one of the following four values:\n
0: the dependency of other samples on this sample is unknown;\n
1: other samples may depend on this one (not disposable);\n
2: no other sample depends on this one (disposable);\n
3: reserved\n
sample_has_redundancy takes one of the following four values:\n
0: it is unknown whether there is redundant coding in this sample;\n
1: there is redundant coding in this sample;\n
2: there is no redundant coding in this sample;\n
3: reserved\n

\param pck target packet
\param dep_flags sample dependency flags
\return error code if any
*/
GF_Err gf_filter_pck_set_dependency_flags(GF_FilterPacket *pck, u8 dep_flags);

/*! Gets packet sample dependency flags.
\param pck target packet
\return dep_flags sample dependency flags - see \ref gf_filter_pck_set_dependency_flags for syntax.
*/
u8 gf_filter_pck_get_dependency_flags(GF_FilterPacket *pck);


/*! Sets packet sequence number. Shall only be used when a filter handles a PLAY request based on packet sequence number
\param pck target packet
\param seq_num sequence number of packet
\return error code if any
*/
GF_Err gf_filter_pck_set_seq_num(GF_FilterPacket *pck, u32 seq_num);

/*! Gets packet sequence number info
\param pck target packet
\return version_number carrousel version number associated with this data chunk
*/
u32 gf_filter_pck_get_seq_num(GF_FilterPacket *pck);

/*! Redefinition of GF_Matrix but without the maths.h include which breaks VideoToolBox on OSX/iOS */
typedef struct __matrix GF_Matrix_unexposed;

/*! Frame interface object

The frame interface object can be used to expose an interface between the packet generated by a filter and its consumers, when dispatching a regular data block is not possible.
This is typically used by decoders exposing the output image planes or by OpenGL filters outputing textures.
Currently only video frames use this interface object.
*/
typedef struct _gf_filter_frame_interface
{
	/*! get video frame plane
	\param frame interface object for the video frame
	\param plane_idx plane index, 0: Y or full plane, 1: U or UV plane, 2: V plane
	\param outPlane adress of target color plane
	\param outStride stride in bytes of target color plane
	\return error code if any
	*/
	GF_Err (*get_plane)(struct _gf_filter_frame_interface *frame, u32 plane_idx, const u8 **outPlane, u32 *outStride);

	/*! get video frame plane texture
	\param frame interface object for the video frame
	\param plane_idx plane index, 0: Y or full plane, 1: U or UV plane, 2: V plane
	\param gl_tex_format GL texture format used
	\param gl_tex_id GL texture ID used
	\param texcoordmatrix texture transform
	\return error code if any
	*/
	GF_Err (*get_gl_texture)(struct _gf_filter_frame_interface *frame, u32 plane_idx, u32 *gl_tex_format, u32 *gl_tex_id, GF_Matrix_unexposed * texcoordmatrix);

	/*! When set to true, indicates that the emitting filter will block until this frame is released.
	Consumers of such a packet shall drop the packet as soon as possible, since it blocks the emiting filter.*/
	Bool blocking;

	/*! private space for the emitting filter, consumers shall not modify this*/
	void *user_data;
} GF_FilterFrameInterface;


/*! Allocates a new packet holding a reference to a frame interface object.
The packet has by default no DTS, no CTS, no duration framing set to full frame (start=end=1) and all other flags set to 0 (including SAP type).
\param PID the target output PID
\param frame_ifce the associated frame interface object
\param destruct the destructor to be called upon packet release
\return new packet
*/
GF_FilterPacket *gf_filter_pck_new_frame_interface(GF_FilterPid *PID, GF_FilterFrameInterface *frame_ifce, gf_fsess_packet_destructor destruct);

/*! Gets a frame interface associated with a packet if any.

Consummers will typically first check if the packet has associated data using \ref gf_filter_pck_get_data.

\param pck the target packet
\return the associated frame interface object if any, or NULL otherwise
*/
GF_FilterFrameInterface *gf_filter_pck_get_frame_interface(GF_FilterPacket *pck);


/*! Checks if the packet is a blocking reference, i.e. a parent filter in the chain is waiting for its destruction to emit a new packet.
This is typically used by sink filters to decide if they can hold references to input packets without blocking the chain.
\param pck the target packet
\return GF_TRUE if the packet is blocking or is a reference to a blocking packet, GF_FALSE otherwise
*/
Bool gf_filter_pck_is_blocking_ref(GF_FilterPacket *pck);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif	//_GF_FILTERS_H_

