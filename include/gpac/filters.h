/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
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
 * This file contains all exported functions for filter managemeny of the GPAC framework.
*/

/*! \defgroup filters_grp Filter Management
 	\brief Filter Management of GPAC.

API Documentation of the filter managment system of GPAC.

The filter management in GPAC is built using the following core objects:
- \ref GF_FilterSession in charge of
 loading filters from regsitry, managing argument parsing and co
 resolving filter graphs to handle PID connection
 tracking data packets and properties exchanged on PIDs
 scheduling tasks between filters
 ensuring thread-safe filter state: a filter may be called from any thread in the session (unless explicitely asked not to), but only by a single thread at any time.
- \ref GF_FilterRegistry static structure describing possible entry points of the filter, possible arguments and input output pid capabilities.
	Each filter share the same API (registry definition) regardless of its type: source/sink, mux/demux, encode/decode, raw media processing, encoded media processing, ...
- \ref GF_Filter is an instance of the filter registry. A filter implementation typical tasks are:
 accepting new input PIDs (for non source filters)
 defining new output PIDs (for non sink filters), applying any property change due to filter processing
 consuming packets on the input PIDs
 dispatching packets on the output PIDs
- \ref GF_FilterPid handling the connections between two filters.
	pid natively supports fan-out (one filter pid connecting to multiple destinations).
	A pid is in charge of dispatching packets to possible destinations and storing pid properties in sync with dispatched packets.
	Whenever pid properties change, the next packet sent on that pid is associated with the new state, and the destinations filter will be called
	upon fetching the new packet. This is the ONLY reentrant code of a filter.
	When blocking mode is not disabled a the session filter, a pid is also in charge of managing its occupancy through either a number of packets or the
	cumulated duration of the packets it is holding.
	Whenever a PID holds too much data, it enters a blocking state. A filter with ALL its output pids in a blocked state won't be scheduled
	for processing. This is a semi-blocking design, which imply that if a filter has one of its bit in a non blocking state, it will be scheduled for processing.
	A pid is in charge of managing the packet references across filters, by performing memory management of allocated data packets
	 (avoid alloc/free at each packet but rather recycle the memory) and tracking shared packets references.
- \ref GF_FilterPacket holding data to dispatch from a filter on a given PID.
	Packets are always associated to a single output pid, ie it is not possible for a filter to send one packet to multiple pids, the data has to be cloned.
	Packets have default attributes such as timestamps, size, random access status, start/end frame, etc, as well as optionnal properties.
	All packets are reference counted.
	A packet can hold allocated block on the output PID, a pointer to some filter internal data,  a data reference to an single input packet, or a
	special structure called hardware frame currently used for decoder memory access (data or openGL textures).
	Packets holding data references rather than copy are notified back to their creators upon destruction.
- \ref GF_PropertyValue holding various properties for a PID or a packet
	Properties can be copied/merged between input and output pids, or input and output packets. These properties are reference counted.
	Two kinds of properties are defined, built-in ones wich use a 32 bit identifier (usually a four character code), and user properties identified by a string.
	pid properties are defined by the filter creating the pid. They can be overridden/added after being set by the filter by specifying fragment properties
	in the filter arguments. For example fin=src=myfile.foo:#FEXT=bar will override the file extension property (FEXT) foo to bar AFTER the pid is being defined.
- \ref GF_FilterEvent used to pass various events (play/stop/buffer requieremnts/...) up and dow the filter chain.
	This part of the API will likely change in the future, being merged with the global GF_Event of GPAC.

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
typedef struct __gf_media_session GF_FilterSession;

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
typedef void (*gf_fsess_packet_destructor)(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck);

/*!
 *	Filter Event object
 */
typedef union __gf_filter_event GF_FilterEvent;

/*!
 *	Filter Session Task object
 */
typedef struct __gf_fs_task GF_FSTask;
/*!
 *	Filter Session Task process function prototype
 */
typedef void (*gf_fs_task_callback)(GF_FSTask *task);

/*!
 *	Filter Registry object
 */
typedef struct __gf_filter_register GF_FilterRegister;
/*!
 *	Filter Property object
 */
typedef struct __gf_prop_val GF_PropertyValue;

/*!
 *\addtogroup fs_grp Filter Session
 *\ingroup filters_grp
 *\brief Filter Session

 The GPAC filter session object allows building media pipelines using multiple sources and destinations and arbitrary filter chains.

 Filters are descibed through a \ref GF_FilterRegister structure. A set of built-in filters are available, and user-defined filters can be added or removed at run time.

 The filter session keeps an internal graph representation of all available filters and their possible input connections, which is used when resolving connections between filters.

 The number of \ref GF_FilterCapability macthed between registries defines the weight of the connection.

 Paths from an instantiated filter are enabled/disabled based on the source pid capabilities.

 Paths to destination are recomputed for each destination, based on the instantiated destination filter capabilities.

 The graph edges are then enabled in the possible subgraphs allowed by the destination capabilities, and unused filter registries (without enabled input connections) are removed from the graph.

 The resulting weighted graph is then solved using Dijkstra's algorithm, using filter priority in case of weight equality.

 The filter session works by default in a semi-blocking state. Whenever output pid buffers on a filter are all full, the filter is marked as blocked and not scheduled for processing. Whenever one output pid buffer is not full, the filter unblocks.

 This implies that pid buffers may grow quite large if a filter is consuming data from a pid at a much faster rate than another filter consuming from that same pid.
 *	@{
 */


/*! filter session scheduler type */
typedef enum
{
	/*! In this mode, the scheduler does not use locks for packet and property queues. Main task list is mutex-protected */
	GF_FS_SCHEDULER_LOCK_FREE=0,
	/*! In this mode, the scheduler uses locks for packet and property queues. Defaults to lock-free if no threads are used. Main task list is mutex-protected */
	GF_FS_SCHEDULER_LOCK,
	/*! In this mode, the scheduler does not use locks for packet and property queues, nor for the main task list */
	GF_FS_SCHEDULER_LOCK_FREE_X,
	/* In this mode, the scheduler uses locks for packet and property queues even if single-threaded (test mode) */
	GF_FS_SCHEDULER_LOCK_FORCE,
	/*! In this mode, the scheduler uses direct dispatch and no threads, trying to nest task calls within task calls */
	GF_FS_SCHEDULER_DIRECT
} GF_FilterSchedulerType;

/*! Flag set to indicate meta filters should be loaded. A meta filter is a filter providing various subfilters. The subfilters are usually not exposed as filters, only the parent one is.
When set, all sub filters are exposed. This should only be set when inspecting filters help.*/
#define GF_FS_FLAG_LOAD_META	1<<1
/*! Flag set to disable the blocking mode of the filter session. The default is a semi-blocking mode, cf \ref gf_filter_pid_would_block*/
#define GF_FS_FLAG_NO_BLOCKING	1<<2
/*! Flag set to disable internal caching of filter graph connections. If diabled, the graph will be recomputed at each link resolution (less memory ungry but slower)*/
#define GF_FS_FLAG_NO_GRAPH_CACHE	1<<3

/*! Creates a new filter session. This will also load all available filters not blacklisted.
\param nb_threads number of extra threads to allocate
\param type scheduler type
\param user GPAC user for config, callback proc and flags. Can be NULL, see \ref gf_fs_get_user
\param flags set of above flags for the session. Modes set by flags cannot be changed at runtime
\param blacklist string containing comma-separated names of filters to disable
\return the created filter session
*/
GF_FilterSession *gf_fs_new(u32 nb_threads, GF_FilterSchedulerType type, GF_User *user, u32 flags, const char *blacklist);
/*! Destructs the filter session
\param session the filter session to destruct
*/
void gf_fs_del(GF_FilterSession *session);
/*! Load a given filter by its registry name. Filter are created using their registry name, with options appended as a list of colon-separated Name=Value pairs.
Value can be omitted for booleans, defaulting to true (eg :noedit). Using '!'before the name negates the result (eg :!moof_first).
Name can be omitted for enumerations (eg :disp=pbo is equivalent to :pbo), provided that filter developers pay attention to not reuse enum names in one filter!

\param session filter session
\param name name and arguments of the filter registry to instantiate.
\return created filter or NULL if filter registry cannot be found
*/
GF_Filter *gf_fs_load_filter(GF_FilterSession *session, const char *name);

/*! Runs the session
\param session filter session
\return error if any, or GF_EOS. The last errors can be retrieved using \ref gf_fs_get_last_connect_error and \ref gf_fs_get_last_process_error
*/
GF_Err gf_fs_run(GF_FilterSession *session);

/*! Sets the set of separators to use when parsin args
\param session filter session
\param separator_set filter session.
The first char is used to seperate argument names - default is ':'
The second char, if present, is used to seperate names and values - default is '='
The third char, if present, is used to seperate fragments for pid sources - default is '#'
The fourth char, if present, is used for list separators (sourceIDs, gfreg, ...) - default is ','
The fifth char, if present, is used for boolean negation - default is '!'
The sixth char, if present, is used for LINK directives - default is '@'
\return error if any
*/
GF_Err gf_fs_set_separators(GF_FilterSession *session, char *separator_set);

/*! prints all possible connections between filter registries to stderr
\param session filter session
\param  max_chain_length sets maximum chain length when resolving filter links.
Default value is 6 ([in ->] demux -> reframe -> decode -> encode -> reframe -> mux [-> out]
(filter chains loaded for adaptation (eg pixel format change) are loaded after the link resolution)
Setting the value to 0 disables dynamic link resolution. You will have to specify the entire chain manually
\return error if any
*/
GF_Err gf_fs_set_max_resolution_chain_length(GF_FilterSession *session, u32 max_chain_length);

/*! runs session in non blovking mode: process all tasks of oldest scheduled filter, process any pending pid connections and returns.
This can only be used if a user was secified at session creation time, with the flag GF_TERM_NO_COMPOSITOR_THREAD set.
\param session filter session
*/
void gf_fs_run_step(GF_FilterSession *session);

/*! stops the session, waiting for all additional threads to complete
\param session filter session
\return error if any
*/
GF_Err gf_fs_stop(GF_FilterSession *session);

/*! gets the number of available filter registries (not blacklisted)
\param session filter session
\return number of filter registries
*/
u32 gf_fs_filters_registry_count(GF_FilterSession *session);

/*! returns the registry at the given index
\param session filter session
\param idx index of registry, from 0 to \ref gf_fs_filters_registry_count
\return the registry object
*/
const GF_FilterRegister *gf_fs_get_filter_registry(GF_FilterSession *session, u32 idx);

/*! register the test filters used for unit tests
\param session filter session
*/
void gf_fs_register_test_filters(GF_FilterSession *session);

/*! load a source filter from a URL and arguments
\param session filter session
\param url URL of the source to load. Can ba a local file name, a full path (/.., \\...) or a full url with scheme (eg http://, tcp://)
\param args arguments for the filter, see \ref gf_fs_load_filter
\param parent_url parent URL of the source, or NULL if none
\param err if not NULL, is set to error code if any
\return the filter loaded or NULL if error
*/
GF_Filter *gf_fs_load_source(GF_FilterSession *session, const char *url, const char *args, const char *parent_url, GF_Err *err);

/*! load a destination filter from a URL and arguments
\param session filter session
\param url URL of the source to load. Can ba a local file name, a full path (/.., \\...) or a full url with scheme (eg http://, tcp://)
\param args arguments for the filter, see \ref gf_fs_load_filter
\param parent_url parent URL of the source, or NULL if none
\param err if not NULL, is set to error code if any
\return the filter loaded or NULL if error
*/
GF_Filter *gf_fs_load_destination(GF_FilterSession *session, const char *url, const char *args, const char *parent_url, GF_Err *err);

/*! returns the GPAC user object associated with the session. If no user was assigned during session creation, a default user object will be created
\param session filter session
\return the user object
*/
GF_User *gf_fs_get_user(GF_FilterSession *session);

/*! returns the last error which happened during a pid connection
\param session filter session
\return the error code if any
*/
GF_Err gf_fs_get_last_connect_error(GF_FilterSession *session);

/*! returns the last error which happened during a filter process
\param session filter session
\return the error code if any
*/
GF_Err gf_fs_get_last_process_error(GF_FilterSession *session);

/*! Adds a user-defined registry to the session
\param session filter session
\param freg filter registry to add
*/
void gf_fs_add_filter_registry(GF_FilterSession *session, const GF_FilterRegister *freg);

/*! Removes a user-defined registry from the session
\param session filter session
\param freg filter registry to add
*/
void gf_fs_remove_filter_registry(GF_FilterSession *session, GF_FilterRegister *freg);

/*! Posts a user task to the session
\param session filter session
\param task_execute the callback function for the task. The callback can return GF_TRUE to reschedule the task, in which case the task will be rescheduled
immediately or after reschedule_ms. The
\param udta_callback callback user data passed back to the task_execute function
\param log_name log name of the task. If NULL, default is "user_task"
\return the error code if any
*/
GF_Err gf_fs_post_user_task(GF_FilterSession *session, Bool (*task_execute) (GF_FilterSession *fsess, void *callback, u32 *reschedule_ms), void *udta_callback, const char *log_name);

/*! Aborts the session. This can be called within a callback task to stop the session. Do NOT use \ref gf_fs_stop from within a user task callback, this will deadlock the session
\param session filter session
\return the error code if any
*/
GF_Err gf_fs_abort(GF_FilterSession *session);
/*! Check if the session is processing its last task. This can be called within a callback task to check if this is the last task, in order to avoid rescheduling the task
\param session filter session
\return GF_TRUE if no more task, GF_FALSE otherwise
*/
Bool gf_fs_is_last_task(GF_FilterSession *session);

/*! prints stats to stderr
\param session filter session
*/
void gf_fs_print_stats(GF_FilterSession *session);

/*! prints connections between loaded filters in the session
\param session filter session
*/
void gf_fs_print_connections(GF_FilterSession *session);

/*! prints all possible connections between filter registries to stderr
\param session filter session
*/
void gf_fs_print_all_connections(GF_FilterSession *session);

/*! checks the presence of an input capability and an output capability in a target registry. The caps are matched only if they belong to the same bundle
\param filter_reg filter registry to check
\param in_cap_code capabiility code (property type) of input cap to check
\param in_cap capabiility value of input cap to check
\param out_cap_code capabiility code (property type) of output cap to check
\param out_cap capabiility value of output cap to check
\return GF_TRUE if filter registry has such a match, GF_FALSE otherwise
*/
Bool gf_fs_check_registry_cap(const GF_FilterRegister *filter_reg, u32 in_cap_code, GF_PropertyValue *in_cap, u32 out_cap_code, GF_PropertyValue *out_cap);


/*! @} */


/*!
 *\addtogroup fs_props Filter Properties
 *\ingroup filters_grp
 *\brief PID and filter properties
 *
 *Documents the property object used for PID and packets.
 *	@{
 */

/*! defined property types*/
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
	/*! 32 bit / 32 bit fraction */
	GF_PROP_FRACTION,
	/*! 64 bit / 64 bit fraction */
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
	/*! string property, memory is duplicated when setting the property and managed internally */
	GF_PROP_STRING,
	/*! string property, memory is NOT duplicated when setting the property but is then managed (and free) internally
	only used when setting a property, the type then defaults to GF_PROP_STRING*/
	GF_PROP_STRING_NO_COPY,
	/* !data property, memory is duplicated when setting the property and managed internally*/
	GF_PROP_DATA,
	/* !const string property, memory is NOT duplicated when setting the property, stays user-managed */
	GF_PROP_NAME,
	/*! data property, memory is NOT duplicated when setting the property but is then managed (and free) internally
	//only used when setting a property, the type then defaults to GF_PROP_DATA */
	GF_PROP_DATA_NO_COPY,
	/*! const data property, memory is NOT duplicated when setting the property, stays user-managed */
	GF_PROP_CONST_DATA,
	/*! user-managed pointer */
	GF_PROP_POINTER,
	/*! string list: memory is ALWAYS duplicated */
	GF_PROP_STRING_LIST,
	/*! signed 32 bit integer list: memory is ALWAYS duplicated */
	GF_PROP_UINT_LIST,
} GF_PropType;

/*! data property*/
typedef struct
{
	/*! data pointer */
	char *ptr;
	/*! data size */
	u32 size;
} GF_PropData;

/*! list of signed int property*/
typedef struct
{
	/*! array of signed integers */
	u32 *vals;
	/*! number of ites in array */
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

/*! property value used by pids and packets*/
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
		/*! string value of property. For non const string / names, the memory is freed by filter session, otherwise const char * */
		char *string;
		/*! pointer value of property */
		void *ptr;
		/*! string list value of property - memory is handled by filter session (always copy)*/
		GF_List *string_list;
		/*! integer list value of property - memory is handled by filter session (always copy)*/
		GF_PropUIntList uint_list;
	} value;
};

/*! playback mode type supported on pid*/
typedef enum
{
	/*! simplest playback mode, can play from 0 at speed=1 only*/
	GF_PLAYBACK_MODE_NONE=0,
	/*! seek playback mode, can play from any position at speed=1 only*/
	GF_PLAYBACK_MODE_SEEK,
	/*! fast forward playback mode, can play from any position at speed=N>=0 only*/
	GF_PLAYBACK_MODE_FASTFORWARD,
	/*! rewind playback mode, can play from any position at speed=N, n positive or negative*/
	GF_PLAYBACK_MODE_REWIND
} GF_FilterPidPlaybackMode;

/*! Builtin property types
See gpac help (gpac -h props) for codes, types, format and and meaning
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
	GF_PROP_PID_DURATION = GF_4CC('P','D','U','R'),
	GF_PROP_PID_NB_FRAMES = GF_4CC('N','F','R','M'),
	GF_PROP_PID_FRAME_SIZE = GF_4CC('C','F','R','S'),
	GF_PROP_PID_TIMESHIFT = GF_4CC('P','T','S','H'),
	GF_PROP_PID_TIMESCALE = GF_4CC('T','I','M','S'),
	GF_PROP_PID_PROFILE_LEVEL = GF_4CC('P','R','P','L'),
	GF_PROP_PID_DECODER_CONFIG = GF_4CC('D','C','F','G'),
	GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT = GF_4CC('E','C','F','G'),
	GF_PROP_PID_SAMPLE_RATE = GF_4CC('A','U','S','R'),
	GF_PROP_PID_SAMPLES_PER_FRAME = GF_4CC('F','R','M','S'),
	GF_PROP_PID_NUM_CHANNELS = GF_4CC('C','H','N','B'),
	GF_PROP_PID_CHANNEL_LAYOUT = GF_4CC('C','H','L','O'),
	GF_PROP_PID_AUDIO_FORMAT = GF_4CC('A','F','M','T'),
	GF_PROP_PID_AUDIO_SPEED = GF_4CC('A','S','P','D'),
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
	GF_PROP_PID_CROP_POS = GF_4CC('V','C','X','Y'),
	GF_PROP_PID_ORIG_SIZE = GF_4CC('V','O','W','H'),
	GF_PROP_PID_BITRATE = GF_4CC('R','A','T','E'),
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
	GF_PROP_PCK_ISMA_BSO = GF_4CC('I','B','S','O'),
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
	GF_PROP_PCK_EODS = GF_4CC('E','O','D','S'),
	GF_PROP_PID_MAX_FRAME_SIZE = GF_4CC('M','F','R','S'),
	GF_PROP_PID_ISOM_TRACK_TEMPLATE = GF_4CC('I','T','K','T'),
	GF_PROP_PID_ISOM_UDTA = GF_4CC('I','M','U','D'),
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
	GF_PROP_PID_UDP = GF_4CC('P','U','D','P'),
};

/*! gets readable name of builtin property
\param prop_4cc property built-in 4cc
\return readable name
*/
const char *gf_props_4cc_get_name(u32 prop_4cc);

/*! gets property type of builtin property
\param prop_4cc property built-in 4cc
\return property name
*/
u32 gf_props_4cc_get_type(u32 prop_4cc);

/*! checks if two properties equal
\param p1 first property to compare
\param p2 second property to compare
\return GF_TRUE if properties equal, GF_FALSE otherwise
*/
Bool gf_props_equal(const GF_PropertyValue *p1, const GF_PropertyValue *p2);

/*! gets the readable name for a property type
\param type property type
\return readable  name
*/
const char *gf_props_get_type_name(u32 type);

/*! checks if two properties equal
\param type property type to parse
\param name property name to parse (for logs)
\param value string containing the value to parse
\param enum_values string containig enum_values, or NULL. enum_values are used for unisgned int propertues, take the form "a|b|c" and resolve to 0|1|2.
\param list_sep_char value of the list seperator character to use
\return the parsed property value
*/
GF_PropertyValue gf_props_parse_value(u32 type, const char *name, const char *value, const char *enum_values, char list_sep_char);

/*! maximum string size to use when dumping a propery*/
#define GF_PROP_DUMP_ARG_SIZE	100

/*! dumps a property value to string
\param att property value
\param dump buffer holding the resulting value for types requiring string conversions (integers, ...)
\param dump_data if set data will be dumped in hexa. Otherwise, data buffer is not dumped
\return readable  name
*/
const char *gf_prop_dump_val(const GF_PropertyValue *att, char dump[GF_PROP_DUMP_ARG_SIZE], Bool dump_data);

/*! dumps a property value to string, resolving any builtin types (pix formats, codec id, ...)
\param p4cc property 4CC
\param att property value
\param dump buffer holding the resulting value for types requiring string conversions (integers, ...)
\param dump_data if set data will be dumped in hexa. Otherwise, data buffer is not dumped
\return readable  name
*/
const char *gf_prop_dump(u32 p4cc, const GF_PropertyValue *att, char dump[GF_PROP_DUMP_ARG_SIZE], Bool dump_data);

/* ! property aplies only to packets */
#define GF_PROP_FLAG_PCK 1
/* ! property is optional for GPAC GSF serialization (not transmitted over network when property thining is enabled) */
#define GF_PROP_FLAG_GSF_REM 1<<1

/*! gets property description and falgs
\param prop_idx built-in property index, starting from 0
\param name pointer for property name - optional
\param description pointer for property description - optional
\param prop_type pointer for property type - optional
\param prop_flags pointer for property flags - optional
\return GF_TRUE if property was found, GF_FALSE otherwise
*/
Bool gf_props_get_description(u32 prop_idx, u32 *type, const char **name, const char **description, u8 *prop_type, u8 *prop_flags);

/*! gets built-in property 4CC from name
\param name built-in property name
\return built-in property prop_4cc or 0 if not found
*/
u32 gf_props_get_id(const char *name);

/*! gets flags of built-in property
\param prop_4cc built-in property 4cc
\return built-in property flags, 0 if not found
*/
u8 gf_props_4cc_get_flags(u32 prop_4cc);

/* !helper macro to set signed int properties */
#define PROP_SINT(_val) (GF_PropertyValue){.type=GF_PROP_SINT, .value.sint = _val}
/* !helper macro to set unsigned int properties */
#define PROP_UINT(_val) (GF_PropertyValue){.type=GF_PROP_UINT, .value.uint = _val}
#define PROP_LONGSINT(_val) (GF_PropertyValue){.type=GF_PROP_LSINT, .value.longsint = _val}
#define PROP_LONGUINT(_val) (GF_PropertyValue){.type=GF_PROP_LUINT, .value.longuint = _val}
#define PROP_BOOL(_val) (GF_PropertyValue){.type=GF_PROP_BOOL, .value.boolean = _val}
#define PROP_FIXED(_val) (GF_PropertyValue){.type=GF_PROP_FLOAT, .value.fnumber = _val}
#define PROP_FLOAT(_val) (GF_PropertyValue){.type=GF_PROP_FLOAT, .value.fnumber = FLT2FIX(_val)}
#define PROP_FRAC_INT(_num, _den) (GF_PropertyValue){.type=GF_PROP_FRACTION, .value.frac.num = _num, .value.frac.den = _den}
#define PROP_FRAC(_val) (GF_PropertyValue){.type=GF_PROP_FRACTION, .value.frac = _val}
#define PROP_FRAC64(_val) (GF_PropertyValue){.type=GF_PROP_FRACTION, .value.lfrac = _val}
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

Events may flow downwards (towards the source, in which case they are commands, upwards (towards the sink) in which case they are informative event.

A filter not implementing a process_event will result in the event being forwarded down (resp. up) to all input (resp. output) PIDs

A filter may decide to cancel an event, in which case the event is no longer forwarded down/up the chain

GF_FEVT_PLAY, GF_FEVT_STOP and GF_FEVT_SOURCE_SEEK events will trigger a reset of pid buffers

A GF_FEVT_PLAY on a pid already playing is discarded

A GF_FEVT_STOP on a pid already stopped is discarded

GF_FEVT_PLAY and GF_FEVT_SET_SPEED will trigger larger (abs(speed)>1) or smaller (abs(speed)<1) pid buffer limit in blocking mode

GF_FEVT_STOP and GF_FEVT_SOURCE_SEEK events are filtered to reset the pid buffers

 *	@{
 */

/*! Filter event types */
typedef enum
{
	/*! pid control, usually trigger by sink - see \ref GF_FilterPidPlaybackMode*/
	GF_FEVT_PLAY = 1,
	/*! pid speed control, usually trigger by sink - see \ref GF_FilterPidPlaybackMode*/
	GF_FEVT_SET_SPEED,
	/*! pid control, usually trigger by sink - see \ref GF_FilterPidPlaybackMode*/
	GF_FEVT_STOP,
	/*! pid pause, usually trigger by sink - see \ref GF_FilterPidPlaybackMode*/
	GF_FEVT_PAUSE,
	/*! pid resume, usually trigger by sink - see \ref GF_FilterPidPlaybackMode*/
	GF_FEVT_RESUME,
	/*! pid source seek, allows seeking in bytes the source*/
	GF_FEVT_SOURCE_SEEK,
	/*! pid source switch, allows a source filter to switch its source URL for the same protocol*/
	GF_FEVT_SOURCE_SWITCH,
	/*! DASH segment size info, sent down from muxers to manifest generators*/
	GF_FEVT_SEGMENT_SIZE,
	/*! Scene attach event, sent down from compositor to filters (BIFS/OD/timedtext/any scene-related) to share the scene (resources and node graph)*/
	GF_FEVT_ATTACH_SCENE,
	/*! Scene reset event, sent down from compositor to filters (BIFS/OD/timedtext/any scene-related) to indicate scene reset (resources and node graph)
	THIS IS A DIRECT FILTER CALL NOT THREADSAFE, filters processing this event SHALL run on the main thread*/
	GF_FEVT_RESET_SCENE,
	/*! quality switching request event, helps filters decide how to adapt their processing*/
	GF_FEVT_QUALITY_SWITCH,
	/*! visibility hint event, helps filters decide how to adapt their processing*/
	GF_FEVT_VISIBILITY_HINT,
	/*! special event type sent to a filter whenever the pid info properties have been modified. Cannot be cancelled because not forwarded - cf \ref gf_filter_pid_set_info*/
	GF_FEVT_INFO_UPDATE,
	/*! buffer requirement event. This event is NOT sent to filters, it is internaly processed by the filter session. Filters may however send this
	event to indicate their buffereing preference (real-time sinks mostly)*/
	GF_FEVT_BUFFER_REQ,
	/*! filter session capability change, sent whenever globacl caps (max width, max heoght, ... ) are changed*/
	GF_FEVT_CAPS_CHANGE,
	/*mouse move event, send from compositor down to filters*/
	GF_FEVT_MOUSE,
} GF_FEventType;

/*! type: the type of the event*/
/*! on_pid: PID to which the event is targeted. If NULL the event is targeted at the whole filter */
#define FILTER_EVENT_BASE \
	GF_FEventType type; \
	GF_FilterPid *on_pid; \
	\


/*! macro helper for event structinr initializing*/
#define GF_FEVT_INIT(_a, _type, _on_pid)	{ memset(&_a, 0, sizeof(GF_FilterEvent)); _a.base.type = _type; _a.base.on_pid = _on_pid; }

/*! Base type of events. All events start withe the fields defined in \ref FILTER_EVENT_BASE*/
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
	/*!hint block size for source, might not be respected*/
	u32 hint_block_size;
} GF_FEVT_SourceSeek;

/*! Event structure for GF_FEVT_SEGMENT_SIZE*/
typedef struct
{
	FILTER_EVENT_BASE
	/*! URL of segment this info is for, or NULL if single file*/
	const char *seg_url;
	/* !global sidx is signaled using is_init=1 and range in idx range*/
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
	/*!  move to automatic switching quality*/
	Bool set_auto;
	/*! 0: current group, otherwise index of the depending_on group */
	u32 dependent_group_index;
	/*! or ID of the quality to switch, as indicated in query quality*/
	const char *ID;
	/*! 1+tile mode adaptation (doesn't change other selections) */
	u32 set_tile_mode_plus_one;
	/*! quality degradation hint, betwwen 0 (full quality) and 100 (lowest quality, stream not currently rendered)*/
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
	/*! gives min_max coords of the visible rectangle associated with channels. min_x may be greater than max_x in case of 360 videos */
	u32 min_x, max_x, min_y, max_y;
} GF_FEVT_VisibililityHint;


/*! Event structure for GF_FEVT_BUFFER_REQ*/
typedef struct
{
	FILTER_EVENT_BASE
	/*! indicates the max buffer to set on pid - the buffer is only activated on pids connected to decoders*/
	u32 max_buffer_us;
	/*! indicates the max playout buffer to set on pid (buffer level triggering playback)*/
	u32 max_playout_us;
	/*! for muxers: if set, only the pid target of the event will have the buffer req set;
	 otherwise, the buffer requirement event is passed down the chain until a raw media PID is found or a decoder is found*/
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


const char *gf_filter_event_name(u32 type);

/*! @} */


/*!
 *\addtogroup fs_filter Filter
 *\ingroup filters_grp
 *\brief Filter


The API for filters documents declaation of filters, their creation and  functions available for communication with the filter session.
The gf_filter_* functions shall only be called from within a given filter, this filter being the target of the function. This is not checked at runtime !

Calling these functions on other filters will result in unpredictable behaviour, very likely crashes in multi-threading conditions.

A filter is instanciated through a filter registry. The registry holds entry points to a filter, arguments of a filter (basically property values) and capabilities.

Capabilities are used to check if a filter can be connected to a given input PID, or if two filters can be directly connected (capability match).

Capabilities are organized in so-called bundles, gathering the caps that shall be present or not present in the pid / connecting filter. Typically a capability bundle
will contain a stream type for input a stream type for output, a codec id for input and a codec id for output.

Several capability bundles can be used if needed. A good example is the writegen filter in GPAC, which simply transforms a sequence of media frames into a raw file, hence converts
stream_type/codecID to file extension and mime type - cf \ref gpac/src/filters/write_generic.c

When resolving a chain, PID properties are checked against these capabilities. If a property of the same type exists in the pid than in the capability,
it must match the capability requirement (equal, excluded). If no property exist for a given non-optionnal cap type,
 the bundle is marked as not matching and the ext capability bundle in the filter is checked.
 A pid property not listed in any capabiilty of the filter does not impact the matching.

 *	@{
 */

/*! structure holding arguments for a filter*/
typedef struct
{
	/*! argument name*/
	const char *arg_name;
	/*! offset of the argument in the structure, -1 means not exposed/stored in structure, in which case it is notified through the update_arg function
	the offset is casted into the corresponding property value of the argument type*/
	s32 offset_in_private;
	/*! description of argument*/
	const char *arg_desc;
	/*! type of argument - this is a property type*/
	GF_PropType arg_type;
	/*! default value of argument, can be NULL (for number types, value 0 for each dimension of the type is assumed)*/
	const char *arg_default_val;
	/*! string describing an enum (unsigned integer type) or a min/max value.
		For min/max, the syntax is "min,max", with -I = -infinity and +I = +infinity
		For enum, the syntax is "a|b|...", resoling in a=0, b=1,... To skip a value insert '|' eg "|a|b" resolves to a=1, b=2, "a||b" resolves to a=0, b=2*/
	const char *min_max_enum;
	/*! inidcats if the argument is updatable. If so, the value will be changed if \ref offset_in_private is valid, and the update_args function will be called if not NULL*/
	Bool updatable;
	/*! used by meta filters (ffmpeg & co) to indicate the parsing is handled by the filter in which case the type is overloaded to string and passed to the update_args function*/
	Bool meta_arg;
} GF_FilterArgs;

/*! Some macros for assigning filter caps types quicly*/
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

/*! flags for filter capabilities*/
enum
{
	/*! when not set, indicates the start of a new set of caps. Set by default by the generic GF_CAPS_* */
	GF_CAPFLAG_IN_BUNDLE = 1,
	/*!  if set this is an input cap of the bundle*/
	GF_CAPFLAG_INPUT = 1<<1,
	/*!  if set this is an output cap of the bundle. A cap can be declared both as input and output. for example stream type is usually the same on both inputs and outputs*/
	GF_CAPFLAG_OUTPUT = 1<<2,
	/*!  when set, the cap is valid if the value does not match. If an excluded cap is not found in the destination pid, it is assumed to match*/
	GF_CAPFLAG_EXCLUDED = 1<<3,
	/*! when set, the cap is validated only for filter loaded for this destination filter*/
	GF_CAPFLAG_LOADED_FILTER = 1<<4,
	/*! Only used for output caps, indicates that this cap applies to all bundles This avoids repeating caps common to all bundles by setting them only in the first*/
	GF_CAPFLAG_STATIC = 1<<5,
	/*! Only used for input caps, indicates that this cap is optionnal in the input pid */
	GF_CAPFLAG_OPTIONAL = 1<<6,
};

/*! Some macros for assigning filter caps flags quicly*/
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
#define GF_CAPS_INPUT_OUTPUT	(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_OUTPUT)

/*! Filter capability description*/
typedef struct
{
	/*! 4cc of the capability listed. This shall be 0 or the type of a builtin property */
	u32 code;
	/*! default type and value of the capability listed*/
	GF_PropertyValue val;
	/*! name of the capability listed, NULL if code is set. The special value * is used to indicate that the capability is solved at run time (the filter must be loaded)*/
	const char *name;
	/*! Flags of the capability*/
	u32 flags;
	/*! overrides the filter registry priority for this cap. Usually 0*/
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

/*! gets filter session capability
\param filter source of the query - avoids fetching the filter session object
\param caps pointer filled with caps
*/
void gf_filter_get_session_caps(GF_Filter *filter, GF_FilterSessionCaps *caps);

/*! sets filter session capability
\param filter source of the query - avoids fetching the filter session object
\param caps session capability new values - completely replace the old ones
*/
void gf_filter_set_session_caps(GF_Filter *filter, GF_FilterSessionCaps *caps);

/*! filter probe score, used when probing a url/mime or when probing formats from data*/
typedef enum
{
	/*! input is not supported*/
	GF_FPROBE_NOT_SUPPORTED = 0,
	/*! input is supported with potentially missing features*/
	GF_FPROBE_MAYBE_SUPPORTED,
	/*! input is supported*/
	GF_FPROBE_SUPPORTED
} GF_FilterProbeScore;

/*! quick macro for assigning the caps object to the registry structure*/
#define SETCAPS( __struct ) .caps = __struct, .nb_caps=sizeof(__struct)/sizeof(GF_FilterCapability)

/*! The filter registry. Registries are loaded once at the start of the session and shall never be modified after that.
If caps need to be changed for a specific filter, use \ref gf_filter_override_caps*/
struct __gf_filter_register
{
	/*! mandatory - name of the filter as used when setting up filters, shall not contain any space*/
	const char *name;
	/*! optional - author of the filter*/
	const char *author;
	/*! mandatory - description of the filter*/
	const char *description;
	/*! optionnal - comment for the filter*/
	const char *comment;
	/*! optional - size of private stack structure. The structure is allocated by the framework and arguments are setup before calling any of the filter functions*/
	u32 private_size;
	/*! indicates all calls shall take place in fthe main thread (running GL output) - to be refined*/
	u8 requires_main_thread;
	/*! when set indicates the filter does not take part of dynamic filter chain resolution and can only be used by explicitly loading the filter*/
	u8 explicit_only;
	/*! indicates the max number of additional input PIDs - muxers and scalable filters typically set this to (u32) -1. A value of 0 implies the filter can only handle one PID*/
	u32 max_extra_pids;

	/*! list of pid capabilities*/
	const GF_FilterCapability *caps;
	/*! number of pid capabilities*/
	u32 nb_caps;

	/*! optional - filter arguments if any*/
	const GF_FilterArgs *args;

	/*! mandatory - callback for filter processing*/
	GF_Err (*process)(GF_Filter *filter);

	/*! optional for sources, mandatory for filters and sinks - callback for PID update may be called several times
	on the same pid if pid config is changed.
	Since discontinuities may happen at any time, and a filter may fetch packets in burst,
	this function may be called while the filter is calling \ref gf_filter_pid_get_packet (this is the only reentrant call for filters)

	\param filter the target filter
	\param pid the input pid to configure
	\param is_remove indicates the input PID is removed
	\return error if any. A filter returning an error will trigger a reconfigure of the chain to find another filter.
	a filter may return GF_REQUIRES_NEW_INSTANCE to indicate the PID cannot be processed
	in this instance but could be in a clone of the filter.
	*/
	GF_Err (*configure_pid)(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove);

	/*! optional - callback for filter init -  private stack of filter is allocated by framework)
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

	/*! optional - Called whenever an output pid needs caps renegotiaition. If not set, a filter chain will be loaded to solve the cap negotiation

	\param filter the target filter
	\param pid the filter output pid being reconfigured
	\return error code if any
	*/
	GF_Err (*reconfigure_output)(GF_Filter *filter, GF_FilterPid *pid);

	/*! required for source filters (filters having an "src" argument and for destination filters (filters having a "dst" argument) - probe the given URL, returning a score.
	This function is called before opening the source (no data received yet)

	\param url the url of the source to probe
	\param mime the MIME type of the source to probe
	\return probe score
	*/
	GF_FilterProbeScore (*probe_url)(const char *url, const char *mime);

	/*! optional, usually set by demuxers probes the mime type of a data chunk
	This function is called once the source is open

	\param data data to probe
	\param size size of the data to probe
	\param score set to the probe score
	\return mime type of content
	*/
	const char * (*probe_data)(const u8 *data, u32 size, GF_FilterProbeScore *score);

	/*! for filters having the same match of input caps for a PID, the filter with priority at the lowest value will be used
	scalable decoders should use high values, so that they are only selected when enhancement layers are present*/
	u8 priority;

	/*! optional for dynamic filter registries. Dynamic registries may declare any number of registries. The registry_free function will be called to cleanup any allocated memory

	\param session the filter session
	\param the filter registry to destroy
	*/
	void (*registry_free)(GF_FilterSession *session, struct __gf_filter_register *freg);
	/*! user data of registry loader, not inspected/modified by filter session*/
	void *udta;
};


/*! gets filter private stack - the stack is allocated and freed by the filter session, as are any arguments. The rest is up to the filter to delete it
\param filter target filter
\return filter private stack if any, NULL otherwise
*/
void *gf_filter_get_udta(GF_Filter *filter);

/*! sets filter name - mostly used for logging purposes
\param filter target filter
\param name new name to assign. If NULL, name is reset to registry name
*/
void gf_filter_set_name(GF_Filter *filter, const char *name);

/*! gets filter name
\param filter target filter
\return name of the filter
*/
const char *gf_filter_get_name(GF_Filter *filter);

/*! makes the filter sticky. A sticky filter is not removed when all its input pids are disconnected. Typically used by the player
\param filter target filter
*/
void gf_filter_make_sticky(GF_Filter *filter);

/*! return the number of queued events on the filter. Events are not aggregated, some filter may want to wait until all events are processed before taking actions
\param filter target filter
\return number of queued events
*/
u32 gf_filter_get_num_events_queued(GF_Filter *filter);

/*! returns the single instance of GPAC download manager. DO NOT DESTROY IT!!
\param filter target filter
\return the GPAC download manager
*/
GF_DownloadManager *gf_filter_get_download_manager(GF_Filter *filter);

/*! asks task reschedule for a given delay. There is no guarantee that the task will be recalled at exactly the desired delay
\param filter target filter
\param us_until_next number of microseconds to wait before recalling this task
*/
void gf_filter_ask_rt_reschedule(GF_Filter *filter, u32 us_until_next);

/*! posts a filter process task to the parent session. This is needed for some filters not having any input packets to process but still needing to work
such as decoder flushes, servers, etc... The filter session will ignore this call if the filter is already scheduled for processing
\param filter target filter
*/
void gf_filter_post_process_task(GF_Filter *filter);

/*! posts a task to the session scheduler. This task is not a process task but the filter will still be called by a single thread at any time (ie, might delay process)
\param filter target filter
\param task_fun the function to call
\param udta user data passed back to the task function
\param task_name name of the task for logging purposes
*/
void gf_filter_post_task(GF_Filter *filter, gf_fs_task_callback task_fun, void *udta, const char *task_name);

/*! sets callback function on source filter setup failure
\param filter target filter
\param source_filter the source filter to monitor
\param on_setup_error callback function to call upon source  setup error
\param udta user data passed back to the task function
*/
void gf_filter_set_setup_failure_callback(GF_Filter *filter, GF_Filter *source_filter, void (*on_setup_error)(GF_Filter *f, void *on_setup_error_udta, GF_Err e), void *udta);

/*! notify a filter setup error. This is typically called when a source filter or a filter having accepted input pids detects an issue.
For a source filter (no input pid), the failure callback will be called if any, and the filter will be removed.
For a connected filter, all input pids od the filter will be disconnected and the filter removed.
\param filter target filter
\param reason the failure reason code
*/
void gf_filter_setup_failure(GF_Filter *filter, GF_Err reason);

/*! notify a filter fatal error but not a connection error. Typically, a 404 in HTTP.
\param filter target filter
\param reason the failure reason code
\param force_disconnect indicates if the filter chain should be disconnected or not
*/
void gf_filter_notification_failure(GF_Filter *filter, GF_Err reason, Bool force_disconnect);

/*! disconnects a filter chain between two filters
\param filter the starting point of the chain to disconnect
\param until_filter the end point of the chain to disconnect. This filter is NOT deconnected
*/
void gf_filter_remove(GF_Filter *filter, GF_Filter *until_filter);

/*! sets the number of additional input pid a filter can accept. This overrides the default value of the filter registry
\param filter the target filter
\param max_extra_pids the number of additional pids this filter can accept
*/
void gf_filter_sep_max_extra_input_pids(GF_Filter *filter, u32 max_extra_pids);

/*! queries if blocking mode is enabled for the filter
\param filter the target filter
\return GF_TRUE if blocking mode is enabled, GF_FALSE otherwise
*/
Bool gf_filter_block_enabled(GF_Filter *filter);

/*! gets the user defined for the parent filter session. THIS SHALL NOT BE DESTROYED/STOP. Mostly used by compositor/audio/video output
\param filter the target filter
\return the global GPAC user, or NULL if failure
*/
GF_User *gf_filter_get_user(GF_Filter *filter);


/*! connects a source to this filter
\param filter the target filter
\param url url of source to connect to, with optional arguments.
\param parent_url url of parent if any
\param err return code - can be NULL
\return the new source filter instance or NULL if error
*/
GF_Filter *gf_filter_connect_source(GF_Filter *filter, const char *url, const char *parent_url, GF_Err *err);

/*! connects a destination to this filter
\param filter the target filter
\param url url of destination to connect to, with optional arguments.
\param err return code - can be NULL
\return the new destination filter instance or NULL if error
*/
GF_Filter *gf_filter_connect_destination(GF_Filter *filter, const char *url, GF_Err *err);

/*! gets the number of input pids connected to a filter
\param filter the target filter
\return number of input pids
*/
u32 gf_filter_get_ipid_count(GF_Filter *filter);

/*! gets an input pids connected to a filter
\param filter the target filter
\param idx index of the input pid to retrieve, between 0 and \ref gf_filter_get_ipid_count
\return the input pid, or NULL if not found
*/
GF_FilterPid *gf_filter_get_ipid(GF_Filter *filter, u32 idx);

/*! gets the number of output pids connected to a filter
\param filter the target filter
\return number of output pids
*/
u32 gf_filter_get_opid_count(GF_Filter *filter);

/*! gets an output pids connected to a filter
\param filter the target filter
\param idx index of the output pid to retrieve, between 0 and \ref gf_filter_get_opid_count
\return the output pid, or NULL if not found
*/
GF_FilterPid *gf_filter_get_opid(GF_Filter *filter, u32 idx);

/*! queries buffer max limits on a filter. This is the max of buffer limits on all its connected outputs
\param filter the target filter
\param max_buf will be set to the maximum buffer duration in microseconds
\param max_playout_buf will be set to the maximum playout buffer (the one triggering play) duration in microseconds
*/
void gf_filter_get_buffer_max(GF_Filter *filter, u32 *max_buf, u32 *max_playout_buf);

/*! sets a clock state at session level indicating the time / timestamp of the last rendered frame. This is used by basic audio output
\param filter the target filter
\param time_in_us the system time in us, see \ref gf_sys_clock_high_res
\param media_timestamp the media timestamp associated with this time.
*/
void gf_filter_hint_single_clock(GF_Filter *filter, u64 time_in_us, Double media_timestamp);

/*! retrieves the clock state at session level, as set by \ref gf_filter_hint_single_clock
\param filter the target filter
\param time_in_us will be set to the system time in us, see \ref gf_sys_clock_high_res
\param media_timestamp will be set to the media timestamp associated with this time.
*/
void gf_filter_get_clock_hint(GF_Filter *filter, u64 *time_in_us, Double *media_timestamp);

/*! explicietly assigns a source ID to a filter. This shall be called before connecting the link_from filter
If no ID is assigned to the linked filter, a dynamic one in the form of __gpac__%p__ using the linked filter memory adress will be used
\param filter the target filter
\param link_from the filter to link from
\param link_ext any link extensions allowed in link syntax:
#PIDNAME: accepts only pid(s) with name PIDNAME
#TYPE: accepts only pids of matching media type. TYPE can be 'audio' 'video' 'scene' 'text' 'font'
#TYPEN: accepts only Nth pid of matching type from source
#P4CC=VAL: accepts only pids with property matching VAL.
#PName=VAL: same as above, using the builtin name corresponding to the property.
#P4CC-VAL: accepts only pids with property strictly less than VAL (only for 1-dimension number properties).
#P4CC+VAL: accepts only pids with property strictly greater than VAL (only for 1-dimension number properties).
\return error code if any
*/
GF_Err gf_filter_set_source(GF_Filter *filter, GF_Filter *link_from, const char *link_ext);

/*! overrides the filter registry caps with new caps for this instance. Typically used when an option of the filter changes the capabilities
\param filter the target filter
\param caps the new set of capabilities to use for the filter. These are NOT copied and shall be valid for the lifetime of the filter
\param nb_caps number of capabilities set
\return error code if any
*/
GF_Err gf_filter_override_caps(GF_Filter *filter, const GF_FilterCapability *caps, u32 nb_caps );

/*! filter session separator set query*/
typedef enum
{
	/*! queries the character code used to separate between filter arguments*/
	GF_FS_SEP_ARGS=0,
	/*! queries the character code used to separate between argument name and value*/
	GF_FS_SEP_NAME,
	/*! queries the character code used to indicate fragment identifiers (source PIDs, pid properties)*/
	GF_FS_SEP_FRAG,
	/*! queries the character code used to separate items in a list*/
	GF_FS_SEP_LIST,
	/*! queries the character code used to negate values*/
	GF_FS_SEP_NEG,
} GF_FilterSessionSepType;

/*! queries the character code used as a given separator type in argument names. Used for formating arguments when loading sources and destinations from inside a filter
\param filter the target filter
\param sep_type the separator type to query
\return character code of the separator
*/
u8 gf_filter_get_sep(GF_Filter *filter, GF_FilterSessionSepType sep_type);

/*! queries the arguments of the destination. The first output pid connected to a filter with non NULL args will be used (this is a recursive check until end of chain)
\param filter the target filter
\return the argument string of the destination, NULL if none found
*/
const char *gf_filter_get_dst_args(GF_Filter *filter);

/*! sends an event on all input pids
\param filter the target filter
\param evt the event to send
*/
void gf_filter_send_event(GF_Filter *filter, GF_FilterEvent *evt);

/*! looks for a built-in property value on a filter on all pids (inputs and output)
This is a recursive call on both input and ouput chain
\param filter the target filter
\param prop_4cc the code of the built-in property to fetch
\return the property if found NULL otherwise
*/
const GF_PropertyValue *gf_filter_get_info(GF_Filter *filter, u32 prop_4cc);

/*! looks for a property value on a filter on all pids (inputs and output).
This is a recursive call on both input and ouput chain
\param filter the target filter
\param prop_name the name of the property to fetch
\return the property if found NULL otherwise
*/
const GF_PropertyValue *gf_filter_get_info_str(GF_Filter *filter, const char *prop_name);


/*! sends a filter argument update
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

/*! adds an event listener to the session
\param filter filter object
\param el the event listener to add
\return the error code if any
*/
GF_Err gf_filter_add_event_listener(GF_Filter *filter, GF_FSEventListener *el);

/*! removes an event listener from the session
\param filter filter object
\param el the event listener to add
\return the error code if any
*/
GF_Err gf_filter_remove_event_listener(GF_Filter *filter, GF_FSEventListener *el);

/*! forwards an event to the filter session
\param filter filter object
\param evt the event forwarded
\param consumed if set, indicates the event was already consummed/processed before forwarding
\param skip_user if set, indicates the event should only be dispatched to event listeners.
Otherwise, if a user is assigned to the session, the event is forwarded to the user
\return the error code if any
*/
Bool gf_filter_forward_gf_event(GF_Filter *filter, GF_Event *evt, Bool consumed, Bool skip_user);

/*! forwards an event to the filter session. This is a shortcut for \ref gf_filter_forward_gf_event(session, evt, GF_FALSE, GF_FALSE);
\param filter filter object
\param evt the event forwarded
\return the error code if any
*/
Bool gf_filter_send_gf_event(GF_Filter *filter, GF_Event *evt);

/*! @} */


/*!
 *\addtogroup fs_pid Filter PID
 *\ingroup filters_grp
 *\brief Filter Interconnection


A pid is a connection between two filters, holding packets to process. Internally, a pid created by a filter (output pid) is different from an input pid to a filter (configure_pid)
but the API has been designed to hide this, so that most pid functions can be called regardless of the input/output nature of the pid.

All setters functions (gf_filter_pid_set*) will fail on an input pid.

The generic design of the architecture is that each filter is free to decide how it handle PIDs and their packets. This implies that the filter session has no clue how
an output pid relates to an input pid (same goes for packets).
Developpers must therefore manually copy pid properties that seem relevant, or more practically copy all properties from input pid to output pid and
reassign output pid properties changed by the filter.

PIDs can be reconfigured multiple times, even potentially changing caps on the fly. The current architecture does not check for capability matching during reconfigure, it is up to
the filter to do so.

It is also up to filters to decide how to handle a an input PID removal: remove the output pid immediately, keep it open to flush internal data or keep generating data on the output.
The usual practice is to remove the output as soon as the input is removed.

Once an input pid has been notified to be removed, it shall no longer be used by the filter, as it may be discarded/freed (pid are NOT reference counted).

 *	@{
 */



/*! creates a new output pid for the filter. If the filter has a single input pid already connected, the pid properties are copied by default
\param filter the target filter
\return the new output pid
*/
GF_FilterPid *gf_filter_pid_new(GF_Filter *filter);

/*! removes an output pid from the filter. This will trigger removal of the pid upward in the chain
\param pid the target filter pid to remove
*/
void gf_filter_pid_remove(GF_FilterPid *pid);

/*! creates an output pid for a raw input filter (file, sockets, pipe, etc). This will assign file name, local name, mime and extension properties to the created pid
\param filter the target filter
\param url URL of the source, SHALL be set
\param local_file path on local host, can be NULL
\param mime_type mime type of the content. If none provided and propbe_data is not null, the data will be probed for mime type resolution
\param fext file extension of the content. If NULL, will be extracted from URL
\param probe_data data of the stream to probe in order to solve its mime type
\param probe_size size of the probe data
\param out_pid the output pid to create or update. If no referer pid, a new pid will be created otherwise the pid will be updated
\return error code if any
*/
GF_Err gf_filter_pid_raw_new(GF_Filter *filter, const char *url, const char *local_file, const char *mime_type, const char *fext, char *probe_data, u32 probe_size, GF_FilterPid **out_pid);

/*! set a new property on an output pid for built-in property names.
Previous properties (ones set before last packet dispatch) will still be valid.
You need to remove them one by one using set_property with NULL property, or reset the properties with \ref gf_filter_pid_reset_properties.
Setting a new property will trigger a PID reconfigure.

\param pid the target filter pid
\param prop_4cc the built-in property code to modify
\param value the new value to assign, or NULL if the property is to be removed
\return error code if any
*/
GF_Err gf_filter_pid_set_property(GF_FilterPid *pid, u32 prop_4cc, const GF_PropertyValue *value);

/*! set a new property on an output pid - see \ref gf_filter_pid_set_property.
\param pid the target filter pid
\param name the name of the property to modify
\param value the new value to assign, or NULL if the property is to be removed
\return error code if any
*/
GF_Err gf_filter_pid_set_property_str(GF_FilterPid *pid, const char *name, const GF_PropertyValue *value);

/*! set a new property on an output pid - see \ref gf_filter_pid_set_property.
\param pid the target filter pid
\param name the name of the property to modify. The name will be copied to the property, and memory destruction performed by the filter session
\param value the new value to assign, or NULL if the property is to be removed
\return error code if any
*/
GF_Err gf_filter_pid_set_property_dyn(GF_FilterPid *pid, char *name, const GF_PropertyValue *value);

//set a new info on the pid. previous info (ones set before last packet dispatch)
//will still be valid. You need to remove them one by one using set_property with NULL property, or reset the
//properties with gf_filter_pid_reset_properties. Setting a new info will not trigger a PID reconfigure.

/*! set a new info property on an output pid for built-in property names.
Same as \ref gf_filter_pid_set_property, but does not trigger pid reconfiguration
\param pid the target filter pid
\param prop_4cc the built-in property code to modify
\param value the new value to assign, or NULL if the property is to be removed
\return error code if any
*/
GF_Err gf_filter_pid_set_info(GF_FilterPid *pid, u32 prop_4cc, const GF_PropertyValue *value);

/*! set a new info property on an output pid
Same as \ref gf_filter_pid_set_property_str, but does not trigger pid reconfiguration
\param pid the target filter pid
\param name the name of the property to modify
\param value the new value to assign, or NULL if the property is to be removed
\return error code if any
*/
GF_Err gf_filter_pid_set_info_str(GF_FilterPid *pid, const char *name, const GF_PropertyValue *value);

/*! set a new property on an output pid.
Same as \ref gf_filter_pid_set_property_dyn, but does not trigger pid reconfiguration
\param pid the target filter pid
\param name the name of the property to modify. The name will be copied to the property, and memory destruction performed by the filter session
\param value the new value to assign, or NULL if the property is to be removed
\return error code if any
*/
GF_Err gf_filter_pid_set_info_dyn(GF_FilterPid *pid, char *name, const GF_PropertyValue *value);

/*! sets user data pointer for a pid
\param pid the target filter pid
\param udta user data pointer
*/
void gf_filter_pid_set_udta(GF_FilterPid *pid, void *udta);

/*! gets user data pointer for a pid
\param pid the target filter pid
\return udta user data pointer
*/
void *gf_filter_pid_get_udta(GF_FilterPid *pid);

/*! sets pid name. Mostly used for logging purposes
\param pid the target filter pid
\param name the new PID name. function ignored if NULL.
*/
void gf_filter_pid_set_name(GF_FilterPid *pid, const char *name);

/*! gets pid name
\param pid the target filter pid
\return pid name
*/
const char *gf_filter_pid_get_name(GF_FilterPid *pid);

/*! gets filter name of input pid
\param pid the target filter pid
\return name of the filter owning this input pid
*/
const char *gf_filter_pid_get_filter_name(GF_FilterPid *pid);

/*! gets the source arguments of the pid, walking down the chain until the source filter
\param pid the target filter pid
\return argument of the source filter
*/
const char *gf_filter_pid_orig_src_args(GF_FilterPid *pid);

/*! gets the arguments for the filter
\param pid the target filter pid
\return arguments of the source filter
*/
const char *gf_filter_pid_get_args(GF_FilterPid *pid);

/*! sets max buffer occupancy in microsenconds of the pid, and only that pid.
This is different from the GF_FEVT_BUFFER_REQ event which travels down the chain for seting up decoder buffer.
\param pid the target filter pid
\param total_duration_us buffer max occupancy in us
*/
void gf_filter_pid_set_max_buffer(GF_FilterPid *pid, u32 total_duration_us);

/*! returns max buffer occupancy of a pid.
\param pid the target filter pid
\return buffer max occupancy in us
*/
u32 gf_filter_pid_get_max_buffer(GF_FilterPid *pid);

/*! checks if a given filter is in the pid parent chain. This is used to identify sources (rather than checking URL/...)
\param pid the target filter pid
\param filter the source filter to check
\return GF_TRUE if filter is a source for that pid, GF_FALSE otherwise
*/
Bool gf_filter_pid_is_filter_in_parents(GF_FilterPid *pid, GF_Filter *filter);

/*! gets current buffer levels of the pid
\param pid the target filter pid
\param max_units maximum number of packets allowed - can be 0 if buffer is measured in time
\param nb_pck number of packets in pid
\param max_duration maximum buffer duration allowed in us - can be 0 if buffer is measured in units
\param duration buffer duration in us
*/
void gf_filter_pid_get_buffer_occupancy(GF_FilterPid *pid, u32 *max_units, u32 *nb_pck, u32 *max_duration, u32 *duration);

/*! sets loose connect for a pid, avoiding to throw an error if connection of the pid fails. Used by the compositor filter which acts as both sink and filter.
\param pid the target filter pid
*/
void gf_filter_pid_set_loose_connect(GF_FilterPid *pid);


/*! negotiate a given property on an input pid for built-in properties
Filters may accept some pid connection but may need an adaptaion chain to be able to process packets, eg change pixel format or sample rate
This function will trigger a reconfiguration of the filter chain to try to adapt this. If failing, the filter chain will disconnect
This process is asynchronous, the filter asking for a pid negociation will see the notification through a pid_reconfigure if success.
\param pid the target filter pid - this MUST be an input pid
\param prop_4cc the built-in property code to negotiate
\param value the new value to negotiate, SHALL NOT be NULL
\return error code if any
*/
GF_Err gf_filter_pid_negociate_property(GF_FilterPid *pid, u32 prop_4cc, const GF_PropertyValue *value);

/*! negotiate a given property on an input pid for regular properties
see \ref gf_filter_pid_negociate_property
\param pid the target filter pid - this MUST be an input pid
\param name name of the property to negotiate
\param value the new value to negotiate, SHALL NOT be NULL
\return error code if any
*/
GF_Err gf_filter_pid_negociate_property_str(GF_FilterPid *pid, const char *name, const GF_PropertyValue *value);

/*! negotiate a given property on an input pid for regular properties
see \ref gf_filter_pid_negociate_property
\param pid the target filter pid - this MUST be an input pid
\param name the name of the property to modify. The name will be copied to the property, and memory destruction performed by the filter session
\param value the new value to negotiate, SHALL NOT be NULL
\return error code if any
*/
GF_Err gf_filter_pid_negociate_property_dyn(GF_FilterPid *pid, char *name, const GF_PropertyValue *value);

/*! queries a negotiated built-in capability on an output pid
Filters may check if a property negotiation was done on an output pid, and check the property value.
This can be done on an output pid in a filter->reconfigure_output if the filter accpets caps negotiation
This can be done on an input pid in a generic reconfigure_pid

\param pid the target filter pid
\param prop_4cc the built-in property code to negotiate
\return the negociated property value
*/
const GF_PropertyValue *gf_filter_pid_caps_query(GF_FilterPid *pid, u32 prop_4cc);

/*! queries a negotiated capability on an output pid - see \ref gf_filter_pid_caps_query
\param pid the target filter pid
\param prop_name the property name to negotiate
\return the negociated property value
*/
const GF_PropertyValue *gf_filter_pid_caps_query_str(GF_FilterPid *pid, const char *prop_name);

/*! Statistics for pid*/
typedef struct
{
	/*! average process rate on that pid in bits per seconds*/
	u32 average_process_rate;
	/*! max process rate on that pid in bits per seconds*/
	u32 max_process_rate;
	/*! average bitrate for that pid*/
	u32 avgerage_bitrate;
	/*! max bitrate for that pid*/
	u32 max_bitrate;
	/*! number of packets processed on that pid*/
	u32 nb_processed;
	/*! max packet process time of the filter in us*/
	u32 max_process_time;
	/*! total process time of the filter in us*/
	u64 total_process_time;
	/*! process time of first packet of the filter in us*/
	u64 first_process_time;
	/*! process time of the last packet on that pid in us*/
	u64 last_process_time;
	/*! minimum frame duration on that pid in us*/
	u32 min_frame_dur;
	/*! number of saps 1/2/3 on that pid*/
	u32 nb_saps;
	/*! max process time of SAP packets on that pid in us*/
	u32 max_sap_process_time;
	/*! toal process time of SAP packets on that pid in us*/
	u64 total_sap_process_time;
} GF_FilterPidStatistics;

/*! gets statistics for the pid
\param pid the target filter pid
\param stats the retrieved statistics
\return error code if any
*/
GF_Err gf_filter_pid_get_statistics(GF_FilterPid *pid, GF_FilterPidStatistics *stats);

/*! resets current properties of the pid
\param pid the target filter pid
\return error code if any
*/
GF_Err gf_filter_pid_reset_properties(GF_FilterPid *pid);

/*! push a new set of properties on destination pid using all properties from source pid. Old properties in destination will be lost (i.e. reset properties is always performed during copy properties)
\param dst_pid the destination filter pid
\param src_pid the source filter pid
\return error code if any
*/
GF_Err gf_filter_pid_copy_properties(GF_FilterPid *dst_pid, GF_FilterPid *src_pid);

/*! gets a built-in property of the pid
\param pid the target filter pid
\param prop_4cc the code of the built-in property to retrieve
\return the property if found or NULL otherwise
*/
const GF_PropertyValue *gf_filter_pid_get_property(GF_FilterPid *pid, u32 prop_4cc);

/*! gets a property of the pid
\param pid the target filter pid
\param prop_name the name of the property to retrieve
\return the property if found or NULL otherwise
*/
const GF_PropertyValue *gf_filter_pid_get_property_str(GF_FilterPid *pid, const char *prop_name);

/*! enumerates proerties of a pid
\param pid the target filter pid
\param idx input/output index of the current property. 0 means first. Incremented by 1 upon success
\param prop_4cc set to the built-in code of the property if built-in
\param prop_name set to the name of the property if not built-in
\return the property if found or NULL otherwise
*/
const GF_PropertyValue *gf_filter_pid_enum_properties(GF_FilterPid *pid, u32 *idx, u32 *prop_4cc, const char **prop_name);

/*! sets pid framing mode. filters can consume packets as they arrive, or may want to only process full frames/files
\param pid the target filter pid
\param requires_full_blocks if GF_TRUE, the packets on the pid will be reaggregated to form complete frame/files.
\return error code if any
*/
GF_Err gf_filter_pid_set_framing_mode(GF_FilterPid *pid, Bool requires_full_blocks);

/*! gets cumulated buffer duration of pid (recursive until source)
\param pid the target filter pid
\param check_decoder_output if GF_TRUE, the duration includes decoded frames already output by decoder
\return the duration in us
*/
u64 gf_filter_pid_query_buffer_duration(GF_FilterPid *pid, Bool check_decoder_output);

/*! try to force a synchronous flush of the filter chain downwards this pid. If refetching a packet returns NULL, this failed.
\param pid the target filter pid
*/
void gf_filter_pid_try_pull(GF_FilterPid *pid);


/*! looks for a built-in property value on a  pids. This is a recursive call on both input and ouput chain
\param pid the target filter pid to query
\param prop_4cc the code of the built-in property to fetch
\return the property if found NULL otherwise
*/
const GF_PropertyValue *gf_filter_pid_get_info(GF_FilterPid *pid, u32 prop_4cc);

/*! looks for a property value on a  pids. This is a recursive call on both input and ouput chain
\param pid the target filter pid to query
\param prop_name the name of the property to fetch
\return the property if found NULL otherwise
*/
const GF_PropertyValue *gf_filter_pid_get_info_str(GF_FilterPid *pid, const char *prop_name);


/*! signals end of stream on a PID. Each filter needs to call this when EOS is reached on a given stream since there is no explicit link between input PIDs and output PIDs
\param pid the target filter pid
*/
void gf_filter_pid_set_eos(GF_FilterPid *pid);

/*! checks for end of stream signaling on a PID input chain.
This is a recursive call on input chain
\param pid the target filter pid
\return GF_TRUE if end of stream was signaled on the input chain
*/
Bool gf_filter_pid_has_seen_eos(GF_FilterPid *pid);

/*! checks for end of stream signaling on a PID.
\param pid the target filter pid
\return GF_TRUE if end of stream was signaled on that pid
*/
Bool gf_filter_pid_is_eos(GF_FilterPid *pid);

/*! checks if there is a packet ready on an input pid.
\param pid the target filter pid
\return GF_TRUE if no packet in buffers
*/
Bool gf_filter_pid_first_packet_is_empty(GF_FilterPid *pid);

/*! gets the first packet in the input pid buffer.
This may trigger a reconfigure signal on the filter. If reconfigure is not OK, returns NULL and the pid passed to the filter NO LONGER EXISTS (implicit remove)
The packet is still present in the pid buffer until explicitly removed by \ref gf_filter_pid_drop_packet
\param pid the target filter pid
\return packet or NULL of empty or reconfigure error
*/
GF_FilterPacket * gf_filter_pid_get_packet(GF_FilterPid *pid);

/*! fetches the CTS of the first packet in the input pid buffer.
This may trigger a reconfigure signal on the filter. If reconfigure is not OK, returns NULL and the pid passed to the filter NO LONGER EXISTS (implicit remove)
\param pid the target filter pid
\param cts set to the composition time of the first packet, in pid timescale
\return GF_TRUE if cts was fetched, GF_FALSE otherwise
*/
Bool gf_filter_pid_get_first_packet_cts(GF_FilterPid *pid, u64 *cts);

/*! drops the first packet in the input pid buffer.
\param pid the target filter pid
*/
void gf_filter_pid_drop_packet(GF_FilterPid *pid);

/*! gets the number of packets in input pid buffer.
\param pid the target filter pid
\return the number of packets
*/
u32 gf_filter_pid_get_packet_count(GF_FilterPid *pid);

/*! checks the capability of the input pid match its destination filter.
\param pid the target filter pid
\return GF_TRUE if match , GF_FALSE otherwise
*/
Bool gf_filter_pid_check_caps(GF_FilterPid *pid);

/*! checks if the pid would enter a blocking state if a new packet is sent.
This function should be called by eg demuxers to regulate the rate at which they send packets

Note: pids are never fully blocking in GPAC, a filter requesting an output packet should usually get one unless something goes wrong
\param pid the target filter pid
\return GF_TRUE if pid would enter blocking state , GF_FALSE otherwise
*/
Bool gf_filter_pid_would_block(GF_FilterPid *pid);

/*! flags for argument update event*/
enum
{
	/*! the update event can be sent down the source chain*/
	GF_FILTER_UPDATE_DOWNSTREAM = 1<<1,
	/*! the update event can be sent up the filter chain*/
	GF_FILTER_UPDATE_UPSTREAM = 1<<2,
};

/*! shortcut to access the timescale of the pid - faster than get property as the timescale is locally cached for buffer management
\param pid the target filter pid
\return the pid timescale
*/
u32 gf_filter_pid_get_timescale(GF_FilterPid *pid);

/*! clears the end of stream flag on a pid.
Note: the end of stream is automatically cleared when a new packet is dispatched; This function is used to clear it asap, before next packet dispacth (period switch in dash for example).
\param pid the target filter pid
*/
void gf_filter_pid_clear_eos(GF_FilterPid *pid);

/*! indicates how clock references (PCR of MPEG-2) should be handled.
By default these references are passed from input packets to output packets by the filter session (this assumes the filter doesn't modify composition timestamps).
This default can be changed with this function.
\param pid the target filter pid
\param filter_in_charge if set to GF_TRUE, clock references are not forwarded by the filter session and the filter is in charge of handling them
*/
void gf_filter_pid_set_clock_mode(GF_FilterPid *pid, Bool filter_in_charge);

/*! resolves file template using PID properties and file index. Templates follow the DASH mechanism:

- $KEYWORD$ or $KEYWORD%0nd$ are replaced in the template with the resolved value,

- $$ is an escape for $

Supported KEYWORD (case insensitive):
- num: replaced by file_number (usually matches GF_PROP_PCK_FILENUM, but this property is not used in the solving mechanism)
- PID: ID of the source pid
- URL: URL of source file
- File: path on disk for source file
- p4cc=ABCD: uses pid property with 4CC ABCD
- pname=VAL: uses pid property with name VAL (either built-in prop name or other peroperty name)

\param pid the target filter pid
\param szTemplate source template to solve
\param szFinalName buffer for final name
\param file_number number of file to use
\return error if any
*/
GF_Err gf_filter_pid_resolve_file_template(GF_FilterPid *pid, char szTemplate[GF_MAX_PATH], char szFinalName[GF_MAX_PATH], u32 file_number);


/*! sets discard mode on or off on an input pid. When discard is on, all input packets for this PID are no longer dispatched.

This only affect the current PID, not the source filter(s) for that pid.

PID reconfigurations are still forwarded to the filter, so that a filter may decide to re-enable regular mode.

This is typically needed for filters that stop consuming data for a while (dash forced period duration for example) but may resume
consumption later on (stream moving from period 1 to period 2 for example).

\param pid the target filter pid
\param discard_on enables/disables discard
\return error if any
*/
GF_Err gf_filter_pid_set_discard(GF_FilterPid *pid, Bool discard_on);


/*! force a builtin cap props on the output pid.
A pid may hold one forced cap at most. When set it acts as a virtual property checked against filter caps.
This is currently only used by DASH segmenter to enforce loading muxers with dashing capability - likely to change in the near future.
\param pid the target filter pid
\param cap_4cc of built-in property to force in linked filters
\return error if any
*/
GF_Err gf_filter_pid_force_cap(GF_FilterPid *pid, u32 cap_4cc);

/*! gets URL and argument of first destination of PID if any - memory shall be freed by caller.
\param pid the target filter pid
\return destination string or NULL if error
*/
char *gf_filter_pid_get_destination(GF_FilterPid *pid);

/*! sends an event down the filter chain for input pid, or up the filter chain for output pids.
\param pid the target filter pid
\param evt the event to send
*/
void gf_filter_pid_send_event(GF_FilterPid *pid, GF_FilterEvent *evt);


/*! helper function to init play events, that checks the pid \ref GF_FilterPidPlaybackMode and adjust start/speed accordingly. This does not send the event.
\param pid the target filter pid
\param evt the event to initialize
\param start playback start time of request
\param speed playback speed time of request
\param log_name name used for logs in case of capability mismatched
*/
void gf_filter_pid_init_play_event(GF_FilterPid *pid, GF_FilterEvent *evt, Double start, Double speed, const char *log_name);

/*! @} */


/*!
 *\addtogroup fs_pck Filter Packet
 *\ingroup filters_grp
 *\brief Filter data exchange

 Packets consist in block of data or reference to such blocks, passed from the source to the sink only.
Internally, a packet created by a filter (output packet) is different from an input packet to a filter (\ref gf_filter_pid_get_packet)
but the API has been designed to hide this, so that most packet functions can be called regardless of the input/output nature of the pid.

Packets have native attributes (timing, sap state, ...) but may also have any number of properties associated to them.

The generic design of the architecture is that each filter is free to decide how it handle PIDs and their packets. This implies that the filter session has no clue how
an output packet relates to an input packet.
Developpers must therefore manually copy packet properties that seem relevant, or more practically copy all properties from input packet to output packet and
reassign output packet properties changed by the filter.

In order to handle reordering of packets, it is possible to keep references to either packets (may block the filter chain), or packet properties.

Packets shall always be dispatched in their processing order (decode order). If reordering upon reception is needed, or AU interleaving is used, a filter SHALL do the reordering.
However, packets do not have to be send in their creation order: a created packet is not assigned to pid buffers until it is send.

 *	@{
 */


/*! keeps a reference to the given input packet. The packet shall be released at some point using \ref gf_filter_pck_unref
\param pck the target input packet
\return error if any
*/
GF_Err gf_filter_pck_ref(GF_FilterPacket **pck);

/*! remove a reference to the given input packet. The packet might be destroyed after that call.
\param pck the target input packet
*/
void gf_filter_pck_unref(GF_FilterPacket *pck);

/*! creates a reference to the packet properties, but not to the data.
This is mostly usefull for encoders/decoders/filters with delay, where the input packet needs to be released before getting the corresponding output (frame reordering & co).
This allows merging back packet properties after some delay without blocking the filter chain.
\param pck the target input packet
\return error if any
*/
GF_Err gf_filter_pck_ref_props(GF_FilterPacket **pck);


/*! allocates a new packet on the output pid with associated allocated data.
The packet has by default no DTS, no CTS, no duration framing set to full frame (start=end=1) and all other flags set to 0 (including SAP type).
\param pid the target output pid
\param data_size the desired size of the packet - can be changed later
\param data set to the writable buffer of the created packet
\return new packet or NULL if error
*/
GF_FilterPacket *gf_filter_pck_new_alloc(GF_FilterPid *pid, u32 data_size, char **data);


/*! allocates a new packet on the output pid referencing internal data.
The packet has by default no DTS, no CTS, no duration framing set to full frame (start=end=1) and all other flags set to 0 (including SAP type).
\param pid the target output pid
\param data the data block to dispatch
\param data_size the size of the data block to dispatch
\param destruct the callback function used to destroy the packet when no longer used - may be NULL
\return new packet or NULL if error
*/
GF_FilterPacket *gf_filter_pck_new_shared(GF_FilterPid *pid, const char *data, u32 data_size, gf_fsess_packet_destructor destruct);

/*! allocates a new packet on the output pid referencing data of some input packet.
The packet has by default no DTS, no CTS, no duration framing set to full frame (start=end=1) and all other flags set to 0 (including SAP type).
\param pid the target output pid
\param data the data block to dispatch - if NULL, the entire data of the source packet is used
\param data_size the size of the data block to dispatch - if 0, the entire data of the source packet is used
\param source_packet the source packet this data belongs to (at least from the filter point of view).
\return new packet or NULL if error
*/
GF_FilterPacket *gf_filter_pck_new_ref(GF_FilterPid *pid, const char *data, u32 data_size, GF_FilterPacket *source_packet);

/*! allocates a new packet on the output pid with associated allocated data.
The packet has by default no DTS, no CTS, no duration framing set to full frame (start=end=1) and all other flags set to 0 (including SAP type).
\param pid the target output pid
\param data_size the desired size of the packet - can be changed later
\param data set to the writable buffer of the created packet
\param destruct the callback function used to destroy the packet when no longer used - may be NULL
\return new packet or NULL if error
*/
GF_FilterPacket *gf_filter_pck_new_alloc_destructor(GF_FilterPid *pid, u32 data_size, char **data, gf_fsess_packet_destructor destruct);

/*! sends the packet on its output pid. Packets SHALL be sent in processing order (eg, decoding order for video).
However, packets don't have to be sent in their allocation order.
\param pck the target output packet to send
*/
GF_Err gf_filter_pck_send(GF_FilterPacket *pck);

/*! destructs a packet allocated but that cannot be sent. Shall not be used on packet references.
\param pck the target output packet to send
*/
void gf_filter_pck_discard(GF_FilterPacket *pck);

/*! destructs a packet allocated but that cannot be sent. Shall not be used on packet references.
This is a shortcut to \ref gf_filter_pck_new_ref + \ref gf_filter_pck_merge_properties + \ref gf_filter_pck_send
\param reference the input packet to forward
\param pid the output pid to forward to
\return error code if any
*/
GF_Err gf_filter_pck_forward(GF_FilterPacket *reference, GF_FilterPid *pid);

/*! returns data associated with the packet.
\param pck the target packet
\param size set to the packet data size
\return packet data if any, NULL if empty or hardware frame. see \ref gf_filter_pck_get_hw_frame
*/
const char *gf_filter_pck_get_data(GF_FilterPacket *pck, u32 *size);

/*! sets a built-in property of a packet
\param pck the target packet
\param prop_4cc the code of the built-in property to set
\param value the property value to set
\return error code if any
*/
GF_Err gf_filter_pck_set_property(GF_FilterPacket *pck, u32 prop_4cc, const GF_PropertyValue *value);

/*! sets a property of a packet
\param pck the target packet
\param name the name of the property to set
\param value the property value to set
\return error code if any
*/
GF_Err gf_filter_pck_set_property_str(GF_FilterPacket *pck, const char *name, const GF_PropertyValue *value);

/*! sets a property of a packet
\param pck the target packet
\param name the code of the property to set. The name will be copied to the property, and memory destruction performed by the filter session
\param value the property value to set
\return error code if any
*/
GF_Err gf_filter_pck_set_property_dyn(GF_FilterPacket *pck, char *name, const GF_PropertyValue *value);

/*! function protoype for filtering properties.
\param cbk callback data
\param prop_4cc the built-in property code
\param prop_name property name
\param src_prop the property value in the source packet
\return GF_TRUE if the property shall be merged, GF_FALSE otherwise
*/
typedef Bool (*gf_filter_prop_filter)(void *cbk, u32 prop_4cc, const char *prop_name, const GF_PropertyValue *src_prop);

/*! merge properties of source packet into destination packet but does NOT reset destination packet properties
\param pck_src source packet
\param pck_dst destination packet
\return error code if any
*/
GF_Err gf_filter_pck_merge_properties(GF_FilterPacket *pck_src, GF_FilterPacket *pck_dst);

/*! same as \ref gf_filter_pck_merge_properties but uses a filter callback to select properties to merge
\param pck_src source packet
\param pck_dst destination packet
\param filter_prop callback filtering function
\param cbk callback data passed to the callback function
\return error code if any
*/
GF_Err gf_filter_pck_merge_properties_filter(GF_FilterPacket *pck_src, GF_FilterPacket *pck_dst, gf_filter_prop_filter filter_prop, void *cbk);

/*! gets built-in property of packet.
\param pck target packet
\param prop_4cc the code of the built-in property
\return the property if found, NULL otherwise
*/
const GF_PropertyValue *gf_filter_pck_get_property(GF_FilterPacket *pck, u32 prop_4cc);

/*! gets property of packet.
\param pck target packet
\param prop_name the name of the property
\return the property if found, NULL otherwise
*/
const GF_PropertyValue *gf_filter_pck_get_property_str(GF_FilterPacket *pck, const char *prop_name);

/*! enumerates properties on packets.
\param pck target packet
\param idx input/output index of the current property. 0 means first. Incremented by 1 upon success
\param prop_4cc set to the code of the built-in property
\param prop_name set to the name of the property
\return the property if found, NULL otherwise
*/
const GF_PropertyValue *gf_filter_pck_enum_properties(GF_FilterPacket *pck, u32 *idx, u32 *prop_4cc, const char **prop_name);


/*! sets packet framing info. A full frame is a complete entity for the stream type, ie an access unit for media streams and a complete file for file streams
\param pck target packet
\param is_start packet is start of the frame
\param is_end packet is end of the frame
\return error code if any
*/
GF_Err gf_filter_pck_set_framing(GF_FilterPacket *pck, Bool is_start, Bool is_end);

/*! gets packet framing info. A full frame is a complete entity for the stream type, ie an access unit for media streams and a complete file for file streams
\param pck target packet
\param is_start set to GF_TRUE if packet is start of the frame, to GF_FALSE otherwise
\param is_end set to GF_TRUE if packet is end of the frame, to GF_FALSE otherwise
\return error code if any
*/
GF_Err gf_filter_pck_get_framing(GF_FilterPacket *pck, Bool *is_start, Bool *is_end);

/*! sets Decoding Timestamp (DTS) of the packet. Do not set if unknown - automatic packet duration is based on DTS diff if DTS is present, otherwise in CTS diff.
\param pck target packet
\param dts decoding timestamp of packet, in pid timescale units
\return error code if any
*/
GF_Err gf_filter_pck_set_dts(GF_FilterPacket *pck, u64 dts);

/*! gets Decoding Timestamp (DTS) of the packet.
\param pck target packet
\return dts decoding timestamp of packet, in pid timescale units
*/
u64 gf_filter_pck_get_dts(GF_FilterPacket *pck);

/*! sets Composition Timestamp (CTS) of the packet. Do not set if unknown - automatic packet duration is based on DTS diff if DTS is present, otherwise in CTS diff.
\param pck target packet
\param cts composition timestamp of packet, in pid timescale units
\return error code if any
*/
GF_Err gf_filter_pck_set_cts(GF_FilterPacket *pck, u64 cts);

/*! gets Composition Timestamp (CTS) of the packet.
\param pck target packet
\return cts composition timestamp of packet, in pid timescale units
*/
u64 gf_filter_pck_get_cts(GF_FilterPacket *pck);

/*! returns packet timescale (same as pid timescale)
\param pck target packet
\return packet timescale
*/
u32 gf_filter_pck_get_timescale(GF_FilterPacket *pck);

/*! sets packet duration
\param pck target packet
\param duration duration of packet, in pid timescale units
\return error code if any
*/
GF_Err gf_filter_pck_set_duration(GF_FilterPacket *pck, u32 duration);

/*! gets packet duration
\param pck target packet
\return duration duration of packet, in pid timescale units
*/
u32 gf_filter_pck_get_duration(GF_FilterPacket *pck);

/*! realloc packet not yet sent. Returns data start and new range of data. This will reset byte offset information to not available.
\param pck target packet
\param nb_bytes_to_add bytes to add to packet
\param data_start realloc pointer of packet data start - shall not be NULL
\param new_range_start pointer to new (apppended space) data - shall not be NULL
\param new_size full size of allocated block. - shall not be NULL
\return error code if any
*/
GF_Err gf_filter_pck_expand(GF_FilterPacket *pck, u32 nb_bytes_to_add, char **data_start, char **new_range_start, u32 *new_size);

/*! truncate packet not yet sent to given size
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

/*! sets packet SAP type
\param pck target packet
\param sap_type SAP type of the packet
\return error code if any
*/
GF_Err gf_filter_pck_set_sap(GF_FilterPacket *pck, GF_FilterSAPType sap_type);

/*! sets packet SAP type
\param pck target packet
\return sap_type SAP type of the packet
*/
GF_FilterSAPType gf_filter_pck_get_sap(GF_FilterPacket *pck);


/*! sets packet video interlacing flag
\param pck target packet
\param is_interlaced set to 0 if not interlaced, 1 for top field first, 2 otherwise.
\return error code if any
*/
GF_Err gf_filter_pck_set_interlaced(GF_FilterPacket *pck, u32 is_interlaced);

/*! gets packet video interlacing flag
\param pck target packet
\return interlaced flag, set to 0 if not interlaced, 1 for top field first, 2 otherwise.
*/
u32 gf_filter_pck_get_interlaced(GF_FilterPacket *pck);

/*! sets packet corrupted data flag
\param pck target packet
\param is_corrupted indicates if data in packet is corrupted
\return error code if any
*/
GF_Err gf_filter_pck_set_corrupted(GF_FilterPacket *pck, Bool is_corrupted);

/*! gets packet corrupted data flag
\param pck target packet
\return GF_TRUE if data in packet is corrupted
*/
Bool gf_filter_pck_get_corrupted(GF_FilterPacket *pck);

/*! sets seek flag of packet.
For PIDs of stream type FILE with GF_PROP_PID_DISABLE_PROGRESSIVE set, the seek flag indicates
that the packet is a PATCH packet, replacing bytes located at gf_filter_pck_get_byte_offset in file.
If the corrupted flag is set, this indicates the data will be replaced later on.
A seek packet is not meant to be displayed but need for decoding.
\param pck target packet
\param is_seek indicates packet is seek frame
\return error code if any
*/
GF_Err gf_filter_pck_set_seek_flag(GF_FilterPacket *pck, Bool is_seek);

/*! gets packet seek data flag
\param pck target packet
\return GF_TRUE if packet is a seek packet
*/
Bool gf_filter_pck_get_seek_flag(GF_FilterPacket *pck);

/*! sets packet byte offset in source.
byte offset should only be set if the data in the packet is exactly the same as the one at the given byte offset
\param pck target packet
\param byte_offset indicates the byte offset in the source. By default packets are created with no byte offset
\return error code if any
*/
GF_Err gf_filter_pck_set_byte_offset(GF_FilterPacket *pck, u64 byte_offset);

/*! sets packet byte offset in source
\param pck target packet
\return  byte offset in the source
*/
u64 gf_filter_pck_get_byte_offset(GF_FilterPacket *pck);

/*! sets packet roll info (number of packets to rewind/forward and decode to get a clean access point).
\param pck target packet
\param roll_count indicates the roll distance of this packet - only used for SAP 4 for now
\return error code if any
*/
GF_Err gf_filter_pck_set_roll_info(GF_FilterPacket *pck, s16 roll_count);

/*! gets packet roll info.
\param pck target packet
\return roll distance of this packet
\return error code if any
*/
s16 gf_filter_pck_get_roll_info(GF_FilterPacket *pck);

/*! crypt flags for packet */
#define GF_FILTER_PCK_CRYPT 1

/*! sets packet crypt flags
\param pck target packet
\param crypt_flag packet crypt flag
\return error code if any
*/
GF_Err gf_filter_pck_set_crypt_flags(GF_FilterPacket *pck, u8 crypt_flag);

/*! gets packet crypt flags
\param pck target packet
\return packet crypt flag
*/
u8 gf_filter_pck_get_crypt_flags(GF_FilterPacket *pck);

/*! packet clock reference types - used for MPEG-2 TS*/
typedef enum
{
	/*! packet is not a clock reference */
	GF_FILTER_CLOCK_NONE=0,
	/*! packet is a PCR clock reference, expressed in pid timescale */
	GF_FILTER_CLOCK_PCR,
	/*! packet is a PCR clock discontinuity, expressed in pid timescale */
	GF_FILTER_CLOCK_PCR_DISC,
} GF_FilterClockType;

/*! sets packet clock type
\param pck target packet
\param ctype packet clock flag
\return error code if any
*/
GF_Err gf_filter_pck_set_clock_type(GF_FilterPacket *pck, GF_FilterClockType ctype);

/*! gets last clock type and clock value on pid. Always returns 0 if the source filter manages clock references internally cd \ref gf_filter_pid_set_clock_mode.
\param pid target pid to query for clock
\param clock_val last clock reference found
\param timescale last clock reference timescale
\return ctype packet clock flag
*/
GF_FilterClockType gf_filter_pid_get_clock_info(GF_FilterPid *pid, u64 *clock_val, u32 *timescale);

/*! gets clock type of packet. Always returns 0 if the source filter does NOT manages clock references internally cd \ref gf_filter_pid_set_clock_mode.
\param pck target packet
\return ctype packet clock flag
*/
GF_FilterClockType gf_filter_pck_get_clock_type(GF_FilterPacket *pck);

/*! sets packet carrousel info
\param pck target packet
\param version_number carrousel version number associated with this data chunk
\return error code if any
*/
GF_Err gf_filter_pck_set_carousel_version(GF_FilterPacket *pck, u8 version_number);

/*! gets packet carrousel info
\param pck target packet
\return version_number carrousel version number associated with this data chunk
*/
u8 gf_filter_pck_get_carousel_version(GF_FilterPacket *pck);

/*! sets packet sample dependency flags.

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

/*! gets packet sample dependency flags.
\param pck target packet
\return dep_flags sample dependency flags - see \ref gf_filter_pck_set_dependency_flags for syntax.
*/
u8 gf_filter_pck_get_dependency_flags(GF_FilterPacket *pck);

/*! redefinition of GF_Matrix but without the maths.h include which breaks VideoToolBox on OSX/iOS */
typedef struct __matrix GF_Matrix_unexposed;

/*! hardware frame object*/
typedef struct _gf_filter_hw_frame
{
	/*! get media frame plane
	\param frame media frame pointer
	\param plane_idx plane index, 0: Y or full plane, 1: U or UV plane, 2: V plane
	\param outPlane address of target color plane
	\param outStride stride in bytes of target color plane
	\return error code if any
	*/
	GF_Err (*get_plane)(struct _gf_filter_hw_frame *frame, u32 plane_idx, const u8 **outPlane, u32 *outStride);

	/*! get media frame plane texture
	\param frame media frame pointer
	\param plane_idx plane index, 0: Y or full plane, 1: U or UV plane, 2: V plane
	\param gl_tex_format GL texture format used
	\param gl_tex_id GL texture ID used
	\param texcoordmatrix texture transform
	\return error code if any
	*/
	GF_Err (*get_gl_texture)(struct _gf_filter_hw_frame *frame, u32 plane_idx, u32 *gl_tex_format, u32 *gl_tex_id, GF_Matrix_unexposed * texcoordmatrix);

	/*! flag set to true if a hardware reset is pending after the consumption of this frame. This is used to force frame droping
	and unblock the filter chain.*/
	Bool hardware_reset_pending;

	/*! private space for the filter*/
	void *user_data;
} GF_FilterHWFrame;


/*! allocates a new packet holding a reference to a hardware frame.
The packet has by default no DTS, no CTS, no duration framing set to full frame (start=end=1) and all other flags set to 0 (including SAP type).
\param pid the target output pid
\param hw_frame the associated hardware frame object
\param destruct the destructor to be called upon packet release
\return new packet
*/
GF_FilterPacket *gf_filter_pck_new_hw_frame(GF_FilterPid *pid, GF_FilterHWFrame *hw_frame, gf_fsess_packet_destructor destruct);

/*! gets a hardware frame associated with a packet if any.
\param pck the target packet
\return hw_frame the associated hardware frame object if any, or NULL otherwise
*/
GF_FilterHWFrame *gf_filter_pck_get_hw_frame(GF_FilterPacket *pck);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif	//_GF_FILTERS_H_

