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

#define GF_FILTER_NO_BO 0xFFFFFFFFFFFFFFFFUL
#define GF_FILTER_NO_TS 0xFFFFFFFFFFFFFFFFUL

typedef struct __gf_media_session GF_FilterSession;

typedef struct __gf_filter GF_Filter;
typedef GF_Err (*filter_callback)(GF_Filter *filter);

typedef struct __gf_filter_pid GF_FilterPid;

typedef struct __gf_filter_pck GF_FilterPacket;
typedef void (*packet_destructor)(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck);

typedef union __gf_filter_event GF_FilterEvent;

typedef struct __gf_fs_task GF_FSTask;
typedef void (*gf_fs_task_callback)(GF_FSTask *task);

void *gf_fs_task_get_udta(GF_FSTask *task);

typedef enum
{
	//the scheduler does not use locks for packet and property queues. Main task list is mutex-protected
	GF_FS_SCHEDULER_LOCK_FREE=0,
	//the scheduler uses locks for packet and property queues. Defaults to lock-free if no threads are used. Main task list is mutex-protected
	GF_FS_SCHEDULER_LOCK,
	//the scheduler does not use locks for packet and property queues, nor for the main task list
	GF_FS_SCHEDULER_LOCK_FREE_X,
	//the scheduler uses locks for packet and property queues even if single-threaded (test mode)
	GF_FS_SCHEDULER_LOCK_FORCE,
	//the scheduler uses direct dispatch and no threads, trying to nest task calls within task calls
	GF_FS_SCHEDULER_DIRECT,
} GF_FilterSchedulerType;

GF_FilterSession *gf_fs_new(u32 nb_threads, GF_FilterSchedulerType type, GF_User *user, Bool load_meta_filters, Bool disable_blocking, const char *blacklist);
void gf_fs_del(GF_FilterSession *ms);
GF_Filter *gf_fs_load_filter(GF_FilterSession *ms, const char *name);
GF_Err gf_fs_run(GF_FilterSession *ms);
void gf_fs_print_stats(GF_FilterSession *ms);
void gf_fs_print_connections(GF_FilterSession *fsess);
GF_Err gf_fs_set_separators(GF_FilterSession *session, char *separator_set);
GF_Err gf_fs_set_max_resolution_chain_length(GF_FilterSession *session, u32 max_chain_length);

u32 gf_fs_run_step(GF_FilterSession *fsess);

GF_Err gf_fs_stop(GF_FilterSession *fsess);

typedef struct
{
	u32 max_screen_width, max_screen_height, max_screen_bpp;
} GF_FilterSessionCaps;

typedef enum
{
	GF_PROP_FORBIDEN=0,
	GF_PROP_SINT,
	GF_PROP_UINT,
	GF_PROP_LSINT,
	GF_PROP_LUINT,
	GF_PROP_BOOL,
	GF_PROP_FRACTION,
	GF_PROP_FRACTION64,
	GF_PROP_FLOAT,
	GF_PROP_DOUBLE,
	GF_PROP_VEC2I,
	GF_PROP_VEC2,
	GF_PROP_VEC3I,
	GF_PROP_VEC3,
	GF_PROP_VEC4I,
	GF_PROP_VEC4,
	GF_PROP_PIXFMT,
	GF_PROP_PCMFMT,
	//string property, memory is duplicated when setting the property and managed internally
	GF_PROP_STRING,
	//string property, memory is NOT duplicated when setting the property but is then managed (and free) internally
	//only used when setting a property, the type then defaults to GF_PROP_STRING
	GF_PROP_STRING_NO_COPY,
	//data property, memory is duplicated when setting the property and managed internally
	GF_PROP_DATA,
	//const string property, memory is NOT duplicated when setting the property, stays user-managed
	GF_PROP_NAME,
	//data property, memory is NOT duplicated when setting the property but is then managed (and free) internally
	//only used when setting a property, the type then defaults to GF_PROP_DATA
	GF_PROP_DATA_NO_COPY,
	//const data property, memory is NOT duplicated when setting the property, stays user-managed
	GF_PROP_CONST_DATA,
	//user-managed pointer
	GF_PROP_POINTER,
	//string list: memory is ALWAYS duplicated
	GF_PROP_STRING_LIST,
	GF_PROP_UINT_LIST,
} GF_PropType;


typedef struct __gf_prop_val GF_PropertyValue;

typedef struct
{
	char *ptr;
	u32 size;
} GF_PropData;


typedef struct
{
	u32 *vals;
	u32 nb_items;
} GF_PropUIntList;

typedef struct
{
	s32 x;
	s32 y;
} GF_PropVec2i;

typedef struct
{
	Double x;
	Double y;
} GF_PropVec2;

typedef struct
{
	s32 x;
	s32 y;
	s32 z;
} GF_PropVec3i;

typedef struct
{
	Double x;
	Double y;
	Double z;
} GF_PropVec3;

typedef struct
{
	s32 x;
	s32 y;
	s32 z;
	s32 w;
} GF_PropVec4i;

typedef struct
{
	Double x;
	Double y;
	Double z;
	Double w;
} GF_PropVec4;

struct __gf_prop_val
{
	GF_PropType type;
	union {
		u64 longuint;
		s64 longsint;
		s32 sint;
		u32 uint;
		Bool boolean;
		GF_Fraction frac;
		GF_Fraction64 lfrac;
		Fixed fnumber;
		Double number;
		GF_PropVec2i vec2i;
		GF_PropVec2 vec2;
		GF_PropVec3i vec3i;
		GF_PropVec3 vec3;
		GF_PropVec4i vec4i;
		GF_PropVec4 vec4;
		//alloc/freed by filter
		GF_PropData data;
		//alloc/freed by filter if type is GF_PROP_STRING, otherwise const char *
		char *string;
		void *ptr;
		GF_List *string_list;
		GF_PropUIntList uint_list;
	} value;
};

const char *gf_props_get_type_name(u32 type);
GF_PropertyValue gf_props_parse_value(u32 type, const char *name, const char *value, const char *enum_values, char list_sep_char);

#define GF_PROP_DUMP_ARG_SIZE	4000
const char *gf_prop_dump_val(const GF_PropertyValue *att, char dump[GF_PROP_DUMP_ARG_SIZE], Bool dump_data);
const char *gf_prop_dump(u32 p4cc, const GF_PropertyValue *att, char dump[GF_PROP_DUMP_ARG_SIZE], Bool dump_data);

//prop aplies only to packets
#define GF_PROP_FLAG_PCK 1
//prop is optional for media processing
#define GF_PROP_FLAG_GSF_REM 1<<1

Bool gf_props_get_description(u32 prop_idx, u32 *type, const char **name, const char **description, u8 *prop_type, u8 *prop_flags);
u32 gf_props_get_id(const char *name);
u8 gf_props_4cc_get_flags(u32 prop_4cc);

//helper macros to set properties
#define PROP_SINT(_val) (GF_PropertyValue){.type=GF_PROP_SINT, .value.sint = _val}
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



typedef struct
{
	const char *arg_name;
	//offset of the argument in the structure, -1 means not stored in structure, in which case it is notified
	//through the update_arg function
	s32 offset_in_private;
	const char *arg_desc;
	u8 arg_type;
	const char *arg_default_val;
	const char *min_max_enum;
	Bool updatable;
	//used by meta filters (ffmpeg & co) to indicate the parsing is handled by the filter
	//in which case the type is overloaded to string
	Bool meta_arg;
} GF_FilterArgs;


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

enum
{
	//when not set, indicates the start of a new set of caps. Set by default by the generic GF_CAPS_*
	GF_FILTER_CAPS_IN_BUNDLE = 1,
	//! if set this is an input cap of the bundle
	GF_FILTER_CAPS_INPUT = 1<<1,
	//! if set this is an output cap of the bundle. A cap can be declared both as input and output
	//for example stream type is usually the same on both inputs and outputs
	GF_FILTER_CAPS_OUTPUT = 1<<2,
	//! when set, the cap is valid if the value does not match. If an excluded cap is not found
	//in the destination pid, it is assumed to match
	GF_FILTER_CAPS_EXCLUDED = 1<<3,
	//when set, the cap is validated only for filter loaded for this destination filter
	GF_FILTER_CAPS_EXPLICIT = 1<<4,
	//Only used for output caps, indicates that this cap applies to all bundles
	//This avoids repeating caps common to all bundles by setting them only in the first
	GF_FILTER_CAPS_STATIC = 1<<5,
	//Only used for input caps, indicates that this cap is optionnal in the input pid
	GF_FILTER_CAPS_OPTIONAL = 1<<6,
};

#define GF_CAPS_INPUT	(GF_FILTER_CAPS_IN_BUNDLE|GF_FILTER_CAPS_INPUT)
#define GF_CAPS_INPUT_OPT	(GF_FILTER_CAPS_IN_BUNDLE|GF_FILTER_CAPS_INPUT|GF_FILTER_CAPS_OPTIONAL)
#define GF_CAPS_INPUT_STATIC	(GF_FILTER_CAPS_IN_BUNDLE|GF_FILTER_CAPS_INPUT|GF_FILTER_CAPS_STATIC)
#define GF_CAPS_INPUT_STATIC_OPT	(GF_FILTER_CAPS_IN_BUNDLE|GF_FILTER_CAPS_INPUT|GF_FILTER_CAPS_STATIC|GF_FILTER_CAPS_OPTIONAL)
#define GF_CAPS_INPUT_EXCLUDED	(GF_FILTER_CAPS_IN_BUNDLE|GF_FILTER_CAPS_INPUT|GF_FILTER_CAPS_EXCLUDED)
#define GF_CAPS_INPUT_EXCPLICIT	(GF_FILTER_CAPS_IN_BUNDLE|GF_FILTER_CAPS_INPUT|GF_FILTER_CAPS_EXPLICIT)
#define GF_CAPS_OUTPUT	(GF_FILTER_CAPS_IN_BUNDLE|GF_FILTER_CAPS_OUTPUT)
#define GF_CAPS_OUTPUT_EXPLICIT	(GF_FILTER_CAPS_IN_BUNDLE|GF_FILTER_CAPS_OUTPUT|GF_FILTER_CAPS_EXPLICIT)
#define GF_CAPS_OUTPUT_EXCLUDED	(GF_FILTER_CAPS_IN_BUNDLE|GF_FILTER_CAPS_OUTPUT|GF_FILTER_CAPS_EXCLUDED)
#define GF_CAPS_OUTPUT_STATIC	(GF_FILTER_CAPS_IN_BUNDLE|GF_FILTER_CAPS_OUTPUT|GF_FILTER_CAPS_STATIC)
#define GF_CAPS_INPUT_OUTPUT	(GF_FILTER_CAPS_IN_BUNDLE|GF_FILTER_CAPS_INPUT|GF_FILTER_CAPS_OUTPUT)

typedef struct
{
	//4cc of the capability listed.
	u32 code;
	//default type and value of the capability listed
	GF_PropertyValue val;
	//name of the capability listed. the special value * is used to indicate that the capability is
	//solved at run time (the filter must be loaded)
	const char *name;
	//set of flags defined above
	u32 flags;

	//overrides the filter registry priority for this cap. Usually 0
	u8 priority;
} GF_FilterCapability;

void gf_filter_get_session_caps(GF_Filter *filter, GF_FilterSessionCaps *caps);

typedef enum
{
	//input is not supported
	GF_FPROBE_NOT_SUPPORTED = 0,
	//input is supported with potentially missing features
	GF_FPROBE_MAYBE_SUPPORTED,
	//input is supported
	GF_FPROBE_SUPPORTED
} GF_FilterProbeScore;

#define SETCAPS( __struct ) .caps = __struct, .nb_caps=sizeof(__struct)/sizeof(GF_FilterCapability)

typedef struct __gf_filter_register
{
	//mandatory - name of the filter as used when setting up filters, shall not contain any space
	const char *name;
	//optional - author of the filter
	const char *author;
	//mandatory - description of the filter
	const char *description;
	//optionnal - comment for the filter
	const char *comment;
	//optional - size of private stack structure. The structure is allocated by the framework and arguments are setup before calling any of the filter functions
	u32 private_size;
	//indicates all calls shall take place in fthe main thread (running GL output) - to be refined
	u8 requires_main_thread;
	//when set indicates the filter does not take part of dynamic filter chain resolution and can only be used
	//by explicitly loading the filter
	u8 explicit_only;
	//indicates the max number of additional input PIDs - muxers and scalable filters typically set this to (u32) -1
	u32 max_extra_pids;

	//list of pid capabilities
	const GF_FilterCapability *caps;
	u32 nb_caps;

	//optional - filter arguments if any
	const GF_FilterArgs *args;

	//mandatory - callback for filter processing
	GF_Err (*process)(GF_Filter *filter);

	//optional for sources, mandatory for filters and sinks - callback for PID update
	//may be called several times on the same pid if pid config is changed
	//may return GF_REQUIRES_NEW_INSTANCE to indicate the PID cannot be processed in this instance
	//but could be in a clone of the filter
	//since discontinuities may happen at any time, and a filter may fetch packets in burst,
	// this function may be called while the filter is calling gf_filter_pid_get_packet
	//
	//is_remove: indicates the input PID is removed
	GF_Err (*configure_pid)(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove);

	//optional - callback for filter init -  private stack of filter is allocated by framework)
	GF_Err (*initialize)(GF_Filter *filter);

	//optional - callback for filter desctruction -  private stack of filter is freed by framework
	void (*finalize)(GF_Filter *filter);

	//optional - callback for arguments update. If GF_OK is returned, the stack is updated accordingly
	//if function is NULL, all updatable arguments will be changed in the stack without the filter being notified
	GF_Err (*update_arg)(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *new_val);

	//optional - process a given event. Retruns TRUE if the event has to be canceled, FALSE otherwise
	Bool (*process_event)(GF_Filter *filter, const GF_FilterEvent *evt);

	//optional
	//Called whenever an output pid needs caps renegotiaition. If not set, a filter chain will be loaded to solve the cap negotiation
	GF_Err (*reconfigure_output)(GF_Filter *filter, GF_FilterPid *pid);

	//required for source filters (filters having an "src" argument and for destination filters
	// (filters having a "dst" argument) - probe the given URL, returning a score
	GF_FilterProbeScore (*probe_url)(const char *url, const char *mime);

	//optional, usually set by demuxers
	//probes the mime type of a data chunk
	const char * (*probe_data)(const u8 *data, u32 size);

	//for filters having the same match of input caps for a PID, the filter with priority at the lowest value will be used
	//scalable decoders should use high values, so that they are only selected when enhancement layers are present
	u8 priority;

	//optional for dynamic filter registries. Dynamic registries may declare any number of registries. The registry_free function will be called to cleanup any allocated memory
	void (*registry_free)(GF_FilterSession *session, struct __gf_filter_register *freg);
	void *udta;
} GF_FilterRegister;

u32 gf_fs_filters_registry_count(GF_FilterSession *session);
const GF_FilterRegister *gf_fs_get_filter_registry(GF_FilterSession *session, u32 idx);
void gf_fs_register_test_filters(GF_FilterSession *fsess);


void *gf_filter_get_udta(GF_Filter *filter);
void gf_filter_set_name(GF_Filter *filter, const char *name);
const char *gf_filter_get_name(GF_Filter *filter);

void gf_filter_make_sticky(GF_Filter *filter);

u32 gf_filter_get_num_events_queued(GF_Filter *filter);

GF_DownloadManager *gf_filter_get_download_manager(GF_Filter *filter);

void gf_filter_ask_rt_reschedule(GF_Filter *filter, u32 us_until_next);

void gf_filter_post_process_task(GF_Filter *filter);

void gf_filter_post_task(GF_Filter *filter, gf_fs_task_callback task_fun, void *udta, const char *task_name);

void gf_filter_set_setup_failure_callback(GF_Filter *filter, GF_Filter *source_filter, void (*on_setup_error)(GF_Filter *f, void *on_setup_error_udta, GF_Err e), void *udta);

void gf_filter_setup_failure(GF_Filter *filter, GF_Err reason);

void gf_filter_notification_failure(GF_Filter *filter, GF_Err reason, Bool force_disconnect);

void gf_filter_remove(GF_Filter *filter, GF_Filter *until_filter);

void gf_filter_sep_max_extra_input_pids(GF_Filter *filter, u32 max_extra_pids);
Bool gf_filter_block_enabled(GF_Filter *filter);

GF_FilterSession *gf_filter_get_session(GF_Filter *filter);
void gf_filter_session_abort(GF_FilterSession *fsess, GF_Err error_code);

GF_Filter *gf_fs_load_source(GF_FilterSession *fsess, const char *url, const char *args, const char *parent_url, GF_Err *err);

GF_Filter *gf_fs_load_destination(GF_FilterSession *fsess, const char *url, const char *args, const char *parent_url, GF_Err *err);

GF_User *gf_fs_get_user(GF_FilterSession *fsess);

GF_Err gf_fs_get_last_connect_error(GF_FilterSession *fsess);
GF_Err gf_fs_get_last_process_error(GF_FilterSession *fsess);

GF_Filter *gf_filter_connect_source(GF_Filter *filter, const char *url, const char *parent_url, GF_Err *err);
GF_Filter *gf_filter_connect_destination(GF_Filter *filter, const char *url, GF_Err *err);

u32 gf_filter_get_ipid_count(GF_Filter *filter);
GF_FilterPid *gf_filter_get_ipid(GF_Filter *filter, u32 idx);

u32 gf_filter_get_opid_count(GF_Filter *filter);
GF_FilterPid *gf_filter_get_opid(GF_Filter *filter, u32 idx);

//by default copy properties if only one input pid
GF_FilterPid *gf_filter_pid_new(GF_Filter *filter);
void gf_filter_pid_remove(GF_FilterPid *pid);

GF_FilterPid *gf_filter_pid_raw_new(GF_Filter *filter, const char *url, const char *local_file, const char *mime_type, const char *fext, char *probe_data, u32 probe_size, GF_Err *err);

void gf_filter_get_buffer_max(GF_Filter *filter, u32 *max_buf, u32 *max_playout_buf);

//set a new property to the pid. previous properties (ones set before last packet dispatch)
//will still be valid. You need to remove them one by one using set_property with NULL property, or reset the
//properties with gf_filter_pid_reset_properties. Setting a new property will trigger a PID reconfigure
GF_Err gf_filter_pid_set_property(GF_FilterPid *pid, u32 prop_4cc, const GF_PropertyValue *value);
GF_Err gf_filter_pid_set_property_str(GF_FilterPid *pid, const char *name, const GF_PropertyValue *value);
GF_Err gf_filter_pid_set_property_dyn(GF_FilterPid *pid, char *name, const GF_PropertyValue *value);

//set a new info on the pid. previous info (ones set before last packet dispatch)
//will still be valid. You need to remove them one by one using set_property with NULL property, or reset the
//properties with gf_filter_pid_reset_properties. Setting a new info will not trigger a PID reconfigure.
GF_Err gf_filter_pid_set_info(GF_FilterPid *pid, u32 prop_4cc, const GF_PropertyValue *value);
GF_Err gf_filter_pid_set_info_str(GF_FilterPid *pid, const char *name, const GF_PropertyValue *value);
GF_Err gf_filter_pid_set_info_dyn(GF_FilterPid *pid, char *name, const GF_PropertyValue *value);

void gf_filter_pid_set_udta(GF_FilterPid *pid, void *udta);
void *gf_filter_pid_get_udta(GF_FilterPid *pid);
void gf_filter_pid_set_name(GF_FilterPid *pid, const char *name);
const char *gf_filter_pid_get_name(GF_FilterPid *pid);
const char *gf_filter_pid_get_filter_name(GF_FilterPid *pid);
const char *gf_filter_pid_orig_src_args(GF_FilterPid *pid);

const char *gf_filter_pid_get_args(GF_FilterPid *pid);

void gf_filter_pid_set_max_buffer(GF_FilterPid *pid, u32 total_duration_us);
u32 gf_filter_pid_get_max_buffer(GF_FilterPid *pid);

Bool gf_filter_pid_is_filter_in_parents(GF_FilterPid *pid, GF_Filter *filter);

void gf_filter_pid_get_buffer_occupancy(GF_FilterPid *pid, u32 *max_slots, u32 *nb_pck, u32 *max_duration, u32 *duration);

void gf_filter_pid_set_loose_connect(GF_FilterPid *pid);


GF_Err gf_filter_pid_negociate_property(GF_FilterPid *pid, u32 prop_4cc, const GF_PropertyValue *value);
GF_Err gf_filter_pid_negociate_property_str(GF_FilterPid *pid, const char *name, const GF_PropertyValue *value);
GF_Err gf_filter_pid_negociate_property_dyn(GF_FilterPid *pid, char *name, const GF_PropertyValue *value);

const GF_PropertyValue *gf_filter_pid_caps_query(GF_FilterPid *pid, u32 prop_4cc);
const GF_PropertyValue *gf_filter_pid_caps_query_str(GF_FilterPid *pid, const char *prop_name);

typedef struct
{
	u32 average_process_rate;
	u32 max_process_rate;
	u32 avgerage_bitrate;
	u32 max_bitrate;
	u32 nb_processed;
	u32 max_process_time;
	u64 total_process_time;
	u64 first_process_time;
	u64 last_process_time;
	u32 min_frame_dur;
	u32 nb_saps;
	u32 max_sap_process_time;
	u64 total_sap_process_time;
} GF_FilterPidStatistics;

GF_Err gf_filter_pid_get_statistics(GF_FilterPid *pid, GF_FilterPidStatistics *stats);

//resets current properties of the pid
GF_Err gf_filter_pid_reset_properties(GF_FilterPid *pid);
//push a new set of properties for dst_pid using all properties from src_pid
//old props in dst_pid will be lost (i.e. reset properties is always during copy properties)
GF_Err gf_filter_pid_copy_properties(GF_FilterPid *dst_pid, GF_FilterPid *src_pid);

const GF_PropertyValue *gf_filter_pid_get_property(GF_FilterPid *pid, u32 prop_4cc);
const GF_PropertyValue *gf_filter_pid_get_property_str(GF_FilterPid *pid, const char *prop_name);

const GF_PropertyValue *gf_filter_pid_enum_properties(GF_FilterPid *pid, u32 *idx, u32 *prop_4cc, const char **prop_name);

GF_Err gf_filter_pid_set_framing_mode(GF_FilterPid *pid, Bool requires_full_blocks);

u64 gf_filter_pid_query_buffer_duration(GF_FilterPid *pid, Bool check_decoder_output);

void gf_filter_pid_try_pull(GF_FilterPid *pid);


const GF_PropertyValue *gf_filter_get_info(GF_Filter *filter, u32 prop_4cc);
const GF_PropertyValue *gf_filter_get_info_str(GF_Filter *filter, const char *prop_name);
const GF_PropertyValue *gf_filter_pid_get_info(GF_FilterPid *pid, u32 prop_4cc);
const GF_PropertyValue *gf_filter_pid_get_info_str(GF_FilterPid *pid, const char *prop_name);


//signals EOS on a PID. Each filter needs to call this when EOS is reached on a given stream
//since there is no explicit link between input PIDs and output PIDs
void gf_filter_pid_set_eos(GF_FilterPid *pid);
//recursive
Bool gf_filter_pid_has_seen_eos(GF_FilterPid *pid);
Bool gf_filter_pid_is_eos(GF_FilterPid *pid);

Bool gf_filter_pid_first_packet_is_empty(GF_FilterPid *pid);

//may trigger a reconfigure signal to the filter. If reconfigure not OK, returns NULL and the pid passed to the filter NO LONGER EXISTS (implicit remove)
GF_FilterPacket * gf_filter_pid_get_packet(GF_FilterPid *pid);
Bool gf_filter_pid_get_first_packet_cts(GF_FilterPid *pid, u64 *cts);
void gf_filter_pid_drop_packet(GF_FilterPid *pid);
u32 gf_filter_pid_get_packet_count(GF_FilterPid *pid);
Bool gf_filter_pid_check_caps(GF_FilterPid *pid);

Bool gf_filter_pid_would_block(GF_FilterPid *pid);

enum
{
	GF_FILTER_UPDATE_DOWNSTREAM = 1<<1,
	GF_FILTER_UPDATE_UPSTREAM = 1<<2,
};

void gf_filter_send_update(GF_Filter *filter, const char *target_filter_id, const char *arg_name, const char *arg_val, u32 propagate_mask);

//keeps a reference to the given packet
GF_Err gf_filter_pck_ref(GF_FilterPacket **pck);
void gf_filter_pck_unref(GF_FilterPacket *pck);

//creates a reference to the packet properties, but not to the data
//this is mostly usefull for encoders/decoders/filters with delay, where the input packet needs to
//be released before getting the corresponding output (frame reordering & co)
GF_Err gf_filter_pck_ref_props(GF_FilterPacket **pck);

//packet allocators. the packet has by default no DTS, no CTS, no duration framing set to full frame (start=end=1) nd all other flags set to 0 (including SAP type)
GF_FilterPacket *gf_filter_pck_new_alloc(GF_FilterPid *pid, u32 data_size, char **data);
GF_FilterPacket *gf_filter_pck_new_shared(GF_FilterPid *pid, const char *data, u32 data_size, packet_destructor destruct);
GF_FilterPacket *gf_filter_pck_new_ref(GF_FilterPid *pid, const char *data, u32 data_size, GF_FilterPacket *reference);

GF_FilterPacket *gf_filter_pck_new_alloc_destructor(GF_FilterPid *pid, u32 data_size, char **data, packet_destructor destruct);

GF_Err gf_filter_pck_send(GF_FilterPacket *pck);

void gf_filter_pck_discard(GF_FilterPacket *pck);

GF_Err gf_filter_pck_forward(GF_FilterPacket *reference, GF_FilterPid *pid);

const char *gf_filter_pck_get_data(GF_FilterPacket *pck, u32 *size);

GF_Err gf_filter_pck_set_property(GF_FilterPacket *pck, u32 prop_4cc, const GF_PropertyValue *value);
GF_Err gf_filter_pck_set_property_str(GF_FilterPacket *pck, const char *name, const GF_PropertyValue *value);
GF_Err gf_filter_pck_set_property_dyn(GF_FilterPacket *pck, char *name, const GF_PropertyValue *value);

//by defaults packet do not share properties with each-other, this functions enable passing the properties of
//one packet to another
typedef Bool (*gf_filter_prop_filter)(void *cbk, u32 prop_4cc, const char *prop_name, const GF_PropertyValue *src_prop);

//merge properties of SRC into dst, does NOT reset dst properties
GF_Err gf_filter_pck_merge_properties(GF_FilterPacket *pck_src, GF_FilterPacket *pck_dst);
GF_Err gf_filter_pck_merge_properties_filter(GF_FilterPacket *pck_src, GF_FilterPacket *pck_dst, gf_filter_prop_filter filter_prop, void *cbk);
const GF_PropertyValue *gf_filter_pck_get_property(GF_FilterPacket *pck, u32 prop_4cc);
const GF_PropertyValue *gf_filter_pck_get_property_str(GF_FilterPacket *pck, const char *prop_name);

const GF_PropertyValue *gf_filter_pck_enum_properties(GF_FilterPacket *pck, u32 *idx, u32 *prop_4cc, const char **prop_name);


GF_Err gf_filter_pck_set_framing(GF_FilterPacket *pck, Bool is_start, Bool is_end);
GF_Err gf_filter_pck_get_framing(GF_FilterPacket *pck, Bool *is_start, Bool *is_end);

//sets DTS of the packet. Do not set if unknown - automatic packet duration is based on DTS diff if DTS is present, otherwise in CTS diff
GF_Err gf_filter_pck_set_dts(GF_FilterPacket *pck, u64 dts);
u64 gf_filter_pck_get_dts(GF_FilterPacket *pck);
GF_Err gf_filter_pck_set_cts(GF_FilterPacket *pck, u64 cts);
u64 gf_filter_pck_get_cts(GF_FilterPacket *pck);
u32 gf_filter_pck_get_timescale(GF_FilterPacket *pck);

GF_Err gf_filter_pck_set_duration(GF_FilterPacket *pck, u32 duration);
u32 gf_filter_pck_get_duration(GF_FilterPacket *pck);

//realloc packet not already sent. Returns data start and new range of data. This will reset byte offset information
GF_Err gf_filter_pck_expand(GF_FilterPacket *pck, u32 nb_bytes_to_add, char **data_start, char **new_range_start, u32 *new_size);

//truncate packet to given size
GF_Err gf_filter_pck_truncate(GF_FilterPacket *pck, u32 size);

//SAP types as defined in annex I of ISOBMFF
typedef enum
{
	//no SAP
	GF_FILTER_SAP_NONE = 0,
	//closed gop no leading
	GF_FILTER_SAP_1,
	//closed gop leading
	GF_FILTER_SAP_2,
	//open gop
	GF_FILTER_SAP_3,
	//GDR
	GF_FILTER_SAP_4,

	//redundant SAP1 for shadow sync / carousel
	GF_FILTER_SAP_REDUNDANT = 10
} GF_FilterSAPType;

GF_Err gf_filter_pck_set_sap(GF_FilterPacket *pck, GF_FilterSAPType sap_type);
GF_FilterSAPType gf_filter_pck_get_sap(GF_FilterPacket *pck);
GF_Err gf_filter_pck_set_interlaced(GF_FilterPacket *pck, u32 is_interlaced);
u32 gf_filter_pck_get_interlaced(GF_FilterPacket *pck);
GF_Err gf_filter_pck_set_corrupted(GF_FilterPacket *pck, Bool is_corrupted);
Bool gf_filter_pck_get_corrupted(GF_FilterPacket *pck);

//for PIDs of stream type FILE with GF_PROP_PID_DISABLE_PROGRESSIVE set, the seek flag indicates
//that the packet is a PATCH packet, replacing bytes located at gf_filter_pck_get_byte_offset in file
//if the corrupted flag is set, this indicates the data will be replaced later on
GF_Err gf_filter_pck_set_seek_flag(GF_FilterPacket *pck, Bool is_seek);
Bool gf_filter_pck_get_seek_flag(GF_FilterPacket *pck);

GF_Err gf_filter_pck_set_byte_offset(GF_FilterPacket *pck, u64 byte_offset);
u64 gf_filter_pck_get_byte_offset(GF_FilterPacket *pck);

GF_Err gf_filter_pck_set_roll_info(GF_FilterPacket *pck, s16 roll_count);
s16 gf_filter_pck_get_roll_info(GF_FilterPacket *pck);


#define GF_FILTER_PCK_CRYPT 1
GF_Err gf_filter_pck_set_crypt_flags(GF_FilterPacket *pck, u8 crypt_flag);
u8 gf_filter_pck_get_crypt_flags(GF_FilterPacket *pck);

typedef enum
{
	GF_FILTER_CLOCK_NONE=0,
	GF_FILTER_CLOCK_PCR,
	GF_FILTER_CLOCK_PCR_DISC,
} GF_FilterClockType;

GF_Err gf_filter_pck_set_clock_type(GF_FilterPacket *pck, GF_FilterClockType ctype);
GF_FilterClockType gf_filter_pid_get_clock_info(GF_FilterPid *pid, u64 *clock_val, u32 *timescale);

GF_FilterClockType gf_filter_pck_get_clock_type(GF_FilterPacket *pck);

u32 gf_filter_pid_get_timescale(GF_FilterPid *pid);

GF_Err gf_filter_pck_set_carousel_version(GF_FilterPacket *pck, u8 version_number);
u8 gf_filter_pck_get_carousel_version(GF_FilterPacket *pck);

/* Sample dependency flags, same semantics as ISOBMFF: bit(2)is_leading bit(2)sample_depends_on (2)sample_is_depended_on (2)sample_has_redundancy

is_leading takes one of the following four values:
0: the leading nature of this sample is unknown;
1: this sample is a leading sample that has a dependency before the referenced I-picture (and is
therefore not decodable);
2: this sample is not a leading sample;
3: this sample is a leading sample that has no dependency before the referenced I-picture (and
is therefore decodable);
sample_depends_on takes one of the following four values:
0: the dependency of this sample is unknown;
1: this sample does depend on others (not an I picture);
2: this sample does not depend on others (I picture);
3: reserved
sample_is_depended_on takes one of the following four values:
0: the dependency of other samples on this sample is unknown;
1: other samples may depend on this one (not disposable);
2: no other sample depends on this one (disposable);
3: reserved
sample_has_redundancy takes one of the following four values:
0: it is unknown whether there is redundant coding in this sample;
1: there is redundant coding in this sample;
2: there is no redundant coding in this sample;
3: reserved
*/

GF_Err gf_filter_pck_set_dependency_flag(GF_FilterPacket *pck, u8 dep_flags);
u8 gf_filter_pck_get_dependency_flags(GF_FilterPacket *pck);

void gf_filter_pid_clear_eos(GF_FilterPid *pid);

//for user defined registries
void gf_fs_add_filter_registry(GF_FilterSession *fsess, const GF_FilterRegister *freg);
void gf_fs_remove_filter_registry(GF_FilterSession *session, GF_FilterRegister *freg);

void gf_filter_pid_set_clock_mode(GF_FilterPid *pid, Bool filter_in_charge);

enum
{
	GF_PLAYBACK_MODE_NONE=0,
	GF_PLAYBACK_MODE_SEEK,
	GF_PLAYBACK_MODE_FASTFORWARD,
	GF_PLAYBACK_MODE_REWIND
};

enum
{
	//(uint) PID ID
	GF_PROP_PID_ID = GF_4CC('P','I','D','I'),
	GF_PROP_PID_ESID = GF_4CC('E','S','I','D'),
	GF_PROP_PID_ITEM_ID = GF_4CC('I','T','I','D'),
	//(uint) ID of originating service
	GF_PROP_PID_SERVICE_ID = GF_4CC('P','S','I','D'),
	GF_PROP_PID_CLOCK_ID = GF_4CC('C','K','I','D'),
	GF_PROP_PID_DEPENDENCY_ID = GF_4CC('D','P','I','D'),
	GF_PROP_PID_SUBLAYER = GF_4CC('D','P','S','L'),
	//(bool) playback mode (rewind, fast forward, seek) capability of the pid
	GF_PROP_PID_PLAYBACK_MODE = GF_4CC('P','B','K','M'),
	//(bool) indicates single PID has scalable layers not signaled - TODO: change that to the actual number of layers
	GF_PROP_PID_SCALABLE = GF_4CC('S','C','A','L'),
	GF_PROP_PID_TILE_BASE = GF_4CC('S','A','B','T'),
	GF_PROP_PID_LANGUAGE = GF_4CC('L','A','N','G'),
	GF_PROP_PID_SERVICE_NAME = GF_4CC('S','N','A','M'),
	GF_PROP_PID_SERVICE_PROVIDER = GF_4CC('S','P','R','O'),
	//(uint) media stream type, matching gpac stream types
	GF_PROP_PID_STREAM_TYPE = GF_4CC('P','M','S','T'),
	//(uint) media subtype (auxiliary, picture, etc)
	GF_PROP_PID_SUBTYPE = GF_4CC('P','S','S','T'),
	//(uint) media 4cc used in isobmf
	GF_PROP_PID_ISOM_SUBTYPE = GF_4CC('P','I','S','T'),
	//(uint) media stream type before encryption
	GF_PROP_PID_ORIG_STREAM_TYPE = GF_4CC('P','O','S','T'),
	//(uint) object type indication , matching gpac codecid types
	GF_PROP_PID_CODECID = GF_4CC('P','O','T','I'),
	//(bool) indicates if PID is present in IOD
	GF_PROP_PID_IN_IOD = GF_4CC('P','I','O','D'),
	//(bool) indicates the PID is not framed (framed= one full packet <=> one compressed framed). Only used for compressed media, raw media shall only be framed
	GF_PROP_PID_UNFRAMED = GF_4CC('P','F','R','M'),
	//(rational) PID duration
	GF_PROP_PID_DURATION = GF_4CC('P','D','U','R'),
	//(uint) number of frames
	GF_PROP_PID_NB_FRAMES = GF_4CC('N','F','R','M'),
	//(uint) size of frames
	GF_PROP_PID_FRAME_SIZE = GF_4CC('C','F','R','S'),
	//(rational) PID timeshift depth
	GF_PROP_PID_TIMESHIFT = GF_4CC('P','T','S','H'),
	//(uint) timescale of pid
	GF_PROP_PID_TIMESCALE = GF_4CC('T','I','M','S'),
	//(uint) profile and level
	GF_PROP_PID_PROFILE_LEVEL = GF_4CC('P','R','P','L'),
	//(data) decoder config
	GF_PROP_PID_DECODER_CONFIG = GF_4CC('D','C','F','G'),
	//(data) decoder config for enhancement
	GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT = GF_4CC('E','C','F','G'),
	//(uint) sample rate
	GF_PROP_PID_SAMPLE_RATE = GF_4CC('A','U','S','R'),
	//(uint) nb samples per audio frame
	GF_PROP_PID_SAMPLES_PER_FRAME = GF_4CC('F','R','M','S'),
	//(uint) number of audio channels
	GF_PROP_PID_NUM_CHANNELS = GF_4CC('C','H','N','B'),
	//(uint) channel layout
	GF_PROP_PID_CHANNEL_LAYOUT = GF_4CC('C','H','L','O'),
	//(uint) audio format: u8|s16|s32|flt|dbl|u8p|s16p|s32p|fltp|dblp
	GF_PROP_PID_AUDIO_FORMAT = GF_4CC('A','F','M','T'),
	//(uint) audio playback speed - only used for audio out caps reconfig
	GF_PROP_PID_AUDIO_SPEED = GF_4CC('A','S','P','D'),
	//(sint) media delay
	GF_PROP_PID_DELAY = GF_4CC('M','D','L','Y'),
	//(sint) CTS shift to apply when negative ctts are used
	GF_PROP_PID_CTS_SHIFT = GF_4CC('M','D','T','S'),
	//(uint) frame width
	GF_PROP_PID_WIDTH = GF_4CC('W','I','D','T'),
	//(uint) frame height
	GF_PROP_PID_HEIGHT = GF_4CC('H','E','I','G'),
	//(uint) pixel format
	GF_PROP_PID_PIXFMT = GF_4CC('P','F','M','T'),
	//(uint) image or Y/alpha plane stride
	GF_PROP_PID_STRIDE = GF_4CC('V','S','T','Y'),
	//(uint) U/V plane stride
	GF_PROP_PID_STRIDE_UV = GF_4CC('V','S','T','C'),
	//(uint) bit depth of Y samples
	GF_PROP_PID_BIT_DEPTH_Y = GF_4CC('Y','B','P','S'),
	//(uint) bit depth of UV samples
	GF_PROP_PID_BIT_DEPTH_UV = GF_4CC('C','B','P','S'),
	//(fraction) video FPS
	GF_PROP_PID_FPS = GF_4CC('V','F','P','F'),
	//(boolean) video is interlaced
	GF_PROP_PID_INTERLACED = GF_4CC('V','I','L','C'),
	//(fraction) sample (ie pixel) aspect ratio
	GF_PROP_PID_SAR = GF_4CC('P','S','A','R'),
	//(fraction) picture aspect ratio
	GF_PROP_PID_PAR = GF_4CC('V','P','A','R'),
	//(uint) max frame width of all enhancement layers
	GF_PROP_PID_WIDTH_MAX = GF_4CC('M', 'W','I','D'),
	//(uint) max frame height of all enhancement layers
	GF_PROP_PID_HEIGHT_MAX = GF_4CC('M', 'H','E','I'),
	//(uint) Z-index of video pid
	GF_PROP_PID_ZORDER = GF_4CC('V', 'Z','I','X'),
	//(uint) frame width
	GF_PROP_PID_TRANS_X = GF_4CC('V','T','R','X'),
	//(uint) frame height
	GF_PROP_PID_TRANS_Y = GF_4CC('V','T','R','Y'),
	//(uint) frame source window
	GF_PROP_PID_CROP_POS = GF_4CC('V','C','X','Y'),
	GF_PROP_PID_ORIG_SIZE = GF_4CC('V','O','W','H'),

	//(uint) average bitrate
	GF_PROP_PID_BITRATE = GF_4CC('R','A','T','E'),
	//(luint) data size of media
	GF_PROP_PID_MEDIA_DATA_SIZE = GF_4CC('M','D','S','Z'),
	//(bool) data ref is possible
	GF_PROP_PID_CAN_DATAREF = GF_4CC('D','R','E','F'),
	//(string) URL of source file
	GF_PROP_PID_URL = GF_4CC('F','U','R','L'),
	//(string) remote URL where stream data is available - NOT YET SUPPORTED
	GF_PROP_PID_REMOTE_URL = GF_4CC('R','U','R','L'),
	//(string) URL of source file on the local file system, if any
	GF_PROP_PID_FILEPATH = GF_4CC('F','S','R','C'),
	//(string) mime type of file if known
	GF_PROP_PID_MIME = GF_4CC('M','I','M','E'),
	//(string) file extension of source file if known
	GF_PROP_PID_FILE_EXT = GF_4CC('F','E','X','T'),
	//(string) path of output file on the local file system, if any
	GF_PROP_PID_OUTPATH = GF_4CC('F','D','S','T'),
	//(bool) indicates the file is completely cached
	GF_PROP_PID_FILE_CACHED = GF_4CC('C','A','C','H'),
	//(uint) download rate in bits per second
	GF_PROP_PID_DOWN_RATE = GF_4CC('D','L','B','W'),
	//(uint) total download size in bytes if known
	GF_PROP_PID_DOWN_SIZE = GF_4CC('D','L','S','Z'),
	//(uint) total downloaded bytes if known
	GF_PROP_PID_DOWN_BYTES = GF_4CC('D','L','B','D'),
	//(fraction) byte range for the file
	GF_PROP_PID_FILE_RANGE = GF_4CC('F','B','R','A'),
	//(boolean) disables progressive sending of file
	GF_PROP_PID_DISABLE_PROGRESSIVE = GF_4CC('N','P','R','G'),

	//(uint) display width of service
	GF_PROP_SERVICE_WIDTH = GF_4CC('D','W','D','T'),
	//(uint) display  height of service
	GF_PROP_SERVICE_HEIGHT = GF_4CC('D','H','G','T'),

	//(uint) repeat rate in ms
	GF_PROP_PID_CAROUSEL_RATE = GF_4CC('C','A','R','A'),

	//thses two may need refinements when handling clock discontinuities
	//(longuint) UTC date and time of PID
	GF_PROP_PID_UTC_TIME = GF_4CC('U','T','C','D'),
	//(longuint) timestamp corresponding to UTC date and time of PID
	GF_PROP_PID_UTC_TIMESTAMP = GF_4CC('U','T','C','T'),
	//(uint) (info) volume
	GF_PROP_PID_AUDIO_VOLUME = GF_4CC('A','V','O','L'),
	//(uint) (info) pan
	GF_PROP_PID_AUDIO_PAN = GF_4CC('A','P','A','N'),
	//(uint) (info) thread priority
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

	//(longuint) NTP time stamp from sender
	GF_PROP_PCK_SENDER_NTP = GF_4CC('N','T','P','S'),
	//(longuint) ISMA BSO
	GF_PROP_PCK_ISMA_BSO = GF_4CC('I','B','S','O'),
	GF_PROP_PID_ADOBE_CRYPT_META = GF_4CC('A','M','E','T'),
	//(boolean) packets are protected
	GF_PROP_PID_ENCRYPTED = GF_4CC('E','P','C','K'),
	//(long uint)
	GF_PROP_PID_OMA_PREVIEW_RANGE = GF_4CC('O','D','P','R'),
	//(data) binary blob containing (u32)N [(bin128)SystemID(u32)version(u32)KID_count[(bin128)keyID](u32)priv_size(char*priv_size)priv_data]
	GF_PROP_PID_CENC_PSSH = GF_4CC('P','S','S','H'),
	//ptr to raw CENC subsample info
	GF_PROP_PCK_CENC_SAI = GF_4CC('S','A','I','S'),
	//(uint) KID, used on PID
	GF_PROP_PID_KID = GF_4CC('S','K','I','D'),
	//(uint) IV size, used on PID
	GF_PROP_PID_CENC_IV_SIZE = GF_4CC('S','A','I','V'),
	//(data) constant IV
	GF_PROP_PID_CENC_IV_CONST = GF_4CC('C','B','I','V'),
	//(fraction) CENC pattern, skip as num crypt as den
	GF_PROP_PID_CENC_PATTERN = GF_4CC('C','P','T','R'),
	//(uint) senc/piff
	GF_PROP_PID_CENC_STORE = GF_4CC('C','S','T','R'),


	//(uint)
	GF_PROP_PID_AMR_MODE_SET = GF_4CC('A','M','S','T'),
	//(data) binary blob containing N [(u32)flags(u32)size(u32)reserved(u8)priority(u8) discardable]
	GF_PROP_PCK_SUBS = GF_4CC('S','U','B','S'),
	//(uint)
	GF_PROP_PID_MAX_NALU_SIZE = GF_4CC('N','A','L','S'),
	//(uint)
	GF_PROP_PCK_FILENUM = GF_4CC('F','N','U','M'),
	//(uint)
	GF_PROP_PCK_FILENAME = GF_4CC('F','N','A','M'),
	//(bool)
	GF_PROP_PCK_EODS = GF_4CC('E','O','D','S'),

	//(uint)
	GF_PROP_PID_MAX_FRAME_SIZE = GF_4CC('M','F','R','S'),

	//ISOBMFF specific properties
	//(data)
	GF_PROP_PID_ISOM_TRACK_TEMPLATE = GF_4CC('I','T','K','T'),
	//(data)
	GF_PROP_PID_ISOM_UDTA = GF_4CC('I','M','U','D'),


	//DASH/HAS specific properties
	//(string)
	GF_PROP_PID_PERIOD_ID = GF_4CC('P','E','I','D'),
	//(double)
	GF_PROP_PID_PERIOD_START = GF_4CC('P','E','S','T'),
	//(double)
	GF_PROP_PID_PERIOD_DUR = GF_4CC('P','E','D','U'),
	//(uint)
	GF_PROP_PID_REP_ID = GF_4CC('D','R','I','D'),
	//(string)
	GF_PROP_PID_AS_ID = GF_4CC('D','A','I','D'),
	//(string)
	GF_PROP_PID_MUX_SRC = GF_4CC('M','S','R','C'),
	//(uint)
	GF_PROP_PID_DASH_MODE = GF_4CC('D','M','O','D'),
	//(double)
	GF_PROP_PID_DASH_DUR = GF_4CC('D','D','U','R'),
	//(pointer)
	GF_PROP_PID_DASH_MULTI_PID = GF_4CC('D','M','S','D'),
	//(uint)
	GF_PROP_PID_DASH_MULTI_PID_IDX = GF_4CC('D','M','S','I'),
	//(pointer)
	GF_PROP_PID_DASH_MULTI_TRACK = GF_4CC('D','M','T','K'),
	//(stringlist)
	GF_PROP_PID_ROLE = GF_4CC('R','O','L','E'),
	//(stringlist)
	GF_PROP_PID_PERIOD_DESC = GF_4CC('P','D','E','S'),
	//(stringlist)
	GF_PROP_PID_AS_COND_DESC = GF_4CC('A','C','D','S'),
	//(stringlist)
	GF_PROP_PID_AS_ANY_DESC = GF_4CC('A','A','D','S'),
	//(stringlist)
	GF_PROP_PID_REP_DESC = GF_4CC('R','D','E','S'),
	//(stringlist)
	GF_PROP_PID_BASE_URL = GF_4CC('B','U','R','L'),
	//(string)
	GF_PROP_PID_TEMPLATE = GF_4CC('D','T','P','L'),
	//(uint)
	GF_PROP_PID_START_NUMBER = GF_4CC('D','R','S','N'),
	//(string)
	GF_PROP_PID_XLINK = GF_4CC('X','L','N','K'),
	//(double)
	GF_PROP_PID_CLAMP_DUR = GF_4CC('D','C','M','D'),
	//(string)
	GF_PROP_PID_HLS_PLAYLIST = GF_4CC('H','L','V','P'),
	//(string)
	GF_PROP_PID_DASH_CUE = GF_4CC('D','C','U','E'),
	//(bool)
	GF_PROP_PID_UDP = GF_4CC('P','U','D','P'),
};

const char *gf_props_4cc_get_name(u32 prop_4cc);

u32 gf_props_4cc_get_type(u32 prop_4cc);
void gf_props_reset_single(GF_PropertyValue *p);

Bool gf_props_equal(const GF_PropertyValue *p1, const GF_PropertyValue *p2);

//PID messaging: PIDs may receive commands and may emit messages using this system
//event may flow
// downwards (towards the source, in whcih case they are commands,
// upwards (towards the sink) in which case they are informative event.
//A filter not implementing a process_event will result in the event being forwarded (down/up) to all PIDs (input/output)
//A filter may decide to cancel an event, in which case the event is no longer forwarded

typedef enum
{
	/*channel control, app->module. Note that most modules don't need to handle pause/resume/set_speed*/
	GF_FEVT_PLAY = 1,
	GF_FEVT_SET_SPEED,
	GF_FEVT_STOP,	//no associated event structure
	GF_FEVT_PAUSE,	//no associated event structure
	GF_FEVT_RESUME,	//no associated event structure
	GF_FEVT_SOURCE_SEEK,
	GF_FEVT_SOURCE_SWITCH,
	GF_FEVT_SEGMENT_SIZE,
	GF_FEVT_ATTACH_SCENE,
	GF_FEVT_RESET_SCENE,
	GF_FEVT_QUALITY_SWITCH,
	GF_FEVT_VISIBILITY_HINT,
	GF_FEVT_INFO_UPDATE,
	GF_FEVT_BUFFER_REQ,
	GF_FEVT_CAPS_CHANGE,
	GF_FEVT_MOUSE,
} GF_FEventType;

/*command type: the type of the event*/
/*on_pid: PID to which the event is targeted. If NULL the event is targeted at the whole filter */
#define FILTER_EVENT_BASE \
	GF_FEventType type; \
	GF_FilterPid *on_pid; \


#define GF_FEVT_INIT(_a, _type, _on_pid)	{ memset(&_a, 0, sizeof(GF_FilterEvent)); _a.base.type = _type; _a.base.on_pid = _on_pid; }


typedef struct
{
	FILTER_EVENT_BASE
} GF_FEVT_Base;

/*GF_FEVT_PLAY, GF_FEVT_SET_SPEED*/
typedef struct
{
	FILTER_EVENT_BASE

	/*params for : ranges in sec - if range is <0, it means end of file (eg [2, -1] with speed>0 means 2 +oo) */
	Double start_range, end_range;
	/*params for GF_NET_CHAN_PLAY and GF_NET_CHAN_SPEED*/
	Double speed;

	/* set when PLAY event is sent upstream to audio out, indicates HW buffer reset*/
	u8 hw_buffer_reset;
	/*params for GF_FEVT_PLAY only: indicates this is the first PLAY on an element inserted from bcast*/
	u8 initial_broadcast_play;
	/*params for GF_NET_CHAN_PLAY only
		0: range is in media time
		1: range is in timesatmps
		2: range is in media time but timestamps should not be shifted (hybrid dash only for now)
	*/
	u8 timestamp_based;
	//indicates the consumer only cares for the full file, not packets
	u8 full_file_only;

	u8 forced_dash_segment_switch;
} GF_FEVT_Play;

/*GF_FEVT_SOURCE_SEEK*/
typedef struct
{
	FILTER_EVENT_BASE
	u64 start_offset;
	u64 end_offset;
	const char *source_switch;
	u8 previous_is_init_segment;
	u8 skip_cache_expiration;
	//!hint block size for source, might not be respected
	u32 hint_block_size;
} GF_FEVT_SourceSeek;

/*GF_FEVT_SOURCE_SWITCH*/
typedef struct
{
	FILTER_EVENT_BASE
	const char *queue_url;
	u64 start_offset;
	u64 end_offset;

	const char *switch_url;
	u64 switch_start_offset;
	u64 switch_end_offset;
} GF_FEVT_SourceSwitch;

/*GF_FEVT_SEGMENT_SIZE*/
typedef struct
{
	FILTER_EVENT_BASE
	const char *seg_url;
	//global sidx is signaled using is_init=1 and range in idx range
	Bool is_init;
	u64 media_range_start;
	u64 media_range_end;
	u64 idx_range_start;
	u64 idx_range_end;
} GF_FEVT_SegmentSize;

/*GF_FEVT_ATTACH_SCENE and GF_FEVT_RESET_SCENE*/
typedef struct
{
	FILTER_EVENT_BASE
	void *object_manager;
} GF_FEVT_AttachScene;

/*GF_FEVT_QUALITY_SWITCH*/
typedef struct
{
	FILTER_EVENT_BASE

	//switch quality up or down
	Bool up;

	Bool set_auto;
	//0: current group, otherwise index of the depending_on group
	u32 dependent_group_index;
	//or ID of the quality to switch, as indicated in query quality
	const char *ID;
	//1+tile mode adaptation (doesn't change other selections)
	u32 set_tile_mode_plus_one;
	
	u32 quality_degradation;
} GF_FEVT_QualitySwitch;

/*GF_FEVT_MOUSE*/
typedef struct
{
	FILTER_EVENT_BASE
	GF_Event event;
} GF_FEVT_Event;


/*GF_FEVT_VISIBILITY_HINT*/
typedef struct
{
	FILTER_EVENT_BASE
	//gives min_max coords of the visible rectangle associated with channels.
	//min_x may be greater than max_x in case of 360 videos
	u32 min_x, max_x, min_y, max_y;
} GF_FEVT_VisibililityHint;


/*GF_FEVT_VISIBILITY_HINT*/
typedef struct
{
	FILTER_EVENT_BASE
	//indicates the max buffer to set on pid
	//the buffer is only activated on pids connected to decoders
	u32 max_buffer_us;
	u32 max_playout_us;
	//for muxers: if set, only the pid target of the event will have the buffer req set
	//otherwise, the buffer requirement event is passed down the chain until
	//a raw media PID is found or a decoder is found
	Bool pid_only;
} GF_FEVT_BufferRequirement;

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

void gf_filter_pid_send_event(GF_FilterPid *pid, GF_FilterEvent *evt);

void gf_filter_send_event(GF_Filter *filter, GF_FilterEvent *evt);

typedef struct
{
	void *udta;
	/* called when an event should be filtered */
	Bool (*on_event)(void *udta, GF_Event *evt, Bool consumed_by_compositor);
} GF_FSEventListener;

GF_Err gf_fs_add_event_listener(GF_FilterSession *fsess, GF_FSEventListener *el);
GF_Err gf_fs_remove_event_listener(GF_FilterSession *fsess, GF_FSEventListener *el);

Bool gf_fs_forward_event(GF_FilterSession *fsess, GF_Event *evt, Bool consumed, Bool forward_only);
Bool gf_fs_send_event(GF_FilterSession *fsess, GF_Event *evt);

//redefinition of GF_Matrix but without the maths.h include which breaks VideoToolBox on OSX/iOS
typedef struct __matrix GF_Matrix_unexposed;

typedef struct _gf_filter_hw_frame
{
	//get media frame plane
	// @frame: media frame pointer
	// @plane_idx: plane index, 0: Y or full plane, 1: U or UV plane, 2: V plane
	// @outPlane: adress of target color plane
	// @outStride: stride in bytes of target color plane
	GF_Err (*get_plane)(struct _gf_filter_hw_frame *frame, u32 plane_idx, const u8 **outPlane, u32 *outStride);

	//get media frame plane texture
	// @frame: media frame pointer
	// @plane_idx: plane index, 0: Y or full plane, 1: U or UV plane, 2: V plane
	// @gl_tex_format: GL texture format used
	// @gl_tex_id: GL texture ID used
	// @texcoordmatrix: texture transform
	GF_Err (*get_gl_texture)(struct _gf_filter_hw_frame *frame, u32 plane_idx, u32 *gl_tex_format, u32 *gl_tex_id, GF_Matrix_unexposed * texcoordmatrix);

	//set to true if a hardware reset is pending after the consumption of this frame
	Bool hardware_reset_pending;

	//allocated space by the filter
	void *user_data;
} GF_FilterHWFrame;


GF_FilterPacket *gf_filter_pck_new_hw_frame(GF_FilterPid *pid, GF_FilterHWFrame *hw_frame, packet_destructor destruct);
GF_FilterHWFrame *gf_filter_pck_get_hw_frame(GF_FilterPacket *pck);

typedef struct
{
	bin128 SystemID;
	u32 version;
	u32 KID_count;
	bin128 *KIDs;
	u32 private_data_size;
	u8 *private_data;
} GF_CENCPSSHSysInfo;


/*! Gets the stream type name based on stream type
 \param streamType stream type GF_STREAM_XXX as defined in constants.h
 \return NULL if unknown, otherwise value
 */
const char *gf_stream_type_name(u32 streamType);

GF_Err gf_fs_post_user_task(GF_FilterSession *fsess, Bool (*task_execute) (GF_FilterSession *fsess, void *callback, u32 *reschedule_ms), void *udta_callback, const char *log_name);

GF_Err gf_fs_abort(GF_FilterSession *fsess);
Bool gf_fs_is_last_task(GF_FilterSession *fsess);

void gf_filter_hint_single_clock(GF_Filter *filter, u64 time_in_us, Double media_timestamp);
void gf_filter_get_clock_hint(GF_Filter *filter, u64 *time_in_us, Double *media_timestamp);

GF_Err gf_filter_set_source(GF_Filter *filter, GF_Filter *link_from, const char *link_ext);

GF_Err gf_filter_override_caps(GF_Filter *filter, const GF_FilterCapability *caps, u32 nb_caps );

void gf_fs_filter_print_possible_connections(GF_FilterSession *session);

/*resolve file template using PID properties and file index. Templates follow the DASH mechanism:
-  $KEYWORD$ or $KEYWORD%0nd$ are replaced in the template with the resolved value,
- $$ is an escape for $

Supported KEYWORD (case insensitive):
- num: replaced by \file_number (usually matches GF_PROP_PCK_FILENUM)
- PID: ID of the source pid
- URL: URL of source file
- File: path on disk for source file
- p4cc=ABCD: uses pid property with 4CC ABCD
- pname=VAL: uses pid property with name VAL
*/
GF_Err gf_filter_pid_resolve_file_template(GF_FilterPid *pid, char szTemplate[GF_MAX_PATH], char szFinalName[GF_MAX_PATH], u32 file_number);

//helper function
void gf_filter_init_play_event(GF_FilterPid *pid, GF_FilterEvent *evt, Double start, Double speed, const char *log_name);

Bool gf_fs_check_registry_cap(const GF_FilterRegister *f_reg, u32 incode, GF_PropertyValue *cap_input, u32 outcode, GF_PropertyValue *cap_output);

typedef enum
{
	GF_FS_SEP_ARGS=0,
	GF_FS_SEP_NAME,
	GF_FS_SEP_FRAG,
	GF_FS_SEP_LIST,
	GF_FS_SEP_NEG,
} GF_FilterSessionSepType;

/*0: args separator, 1: value separator, 2: fragment, 3: list, 4: negator*/
u8 gf_filter_get_sep(GF_Filter *filter, GF_FilterSessionSepType sep_type);

const char *gf_filter_get_dst_args(GF_Filter *filter);

//setdiscard mode on or off. When discard is on, all nput packets for this PID are no longer dispatched
//this only affect the current PID, not the source filter(s) for that pid
//PID reconfigurations are still forwarded to the filter, so that a filter may decide to re-enable regular mode
//this is typically needed for filters that stop consuming data for a while (dash periods for example) but may resume
//consumption later on (stream moving from period 1 to period 2 for example)
GF_Err gf_filter_pid_set_discard(GF_FilterPid *pid, Bool discard_on);


GF_Err gf_filter_pid_force_cap(GF_FilterPid *pid, u32 cap4cc);

//query first destination of PID if any - memory shall be freed by caller
char *gf_filter_pid_get_destination(GF_FilterPid *pid);

#ifdef __cplusplus
}
#endif

#endif	//_GF_FILTERS_H_

