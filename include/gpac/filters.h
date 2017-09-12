/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
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
#include <gpac/events.h>
#include <gpac/user.h>

#include <gpac/download.h>

//for offsetof()
#include <stddef.h>

//atomic ref_count++ / ref_count--
#if defined(WIN32) || defined(_WIN32_WCE)

#define safe_int_inc(__v) InterlockedIncrement((int *) (__v))
#define safe_int_dec(__v) InterlockedDecrement((int *) (__v))

#define safe_int_add(__v, inc_val) InterlockedAdd((int *) (__v), inc_val)
#define safe_int_sub(__v, dec_val) InterlockedAdd((int *) (__v), -dec_val)

#else

#define safe_int_inc(__v) __sync_add_and_fetch((int *) (__v), 1)
#define safe_int_dec(__v) __sync_sub_and_fetch((int *) (__v), 1)

#define safe_int_add(__v, inc_val) __sync_add_and_fetch((int *) (__v), inc_val)
#define safe_int_sub(__v, dec_val) __sync_sub_and_fetch((int *) (__v), dec_val)

#endif

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

GF_FilterSession *gf_fs_new(u32 nb_threads, GF_FilterSchedulerType type, GF_User *user, Bool load_meta_filters);
void gf_fs_del(GF_FilterSession *ms);
GF_Filter *gf_fs_load_filter(GF_FilterSession *ms, const char *name);
GF_Err gf_fs_run(GF_FilterSession *ms);
void gf_fs_print_stats(GF_FilterSession *ms);

u32 gf_fs_run_step(GF_FilterSession *fsess);

GF_Err gf_fs_stop(GF_FilterSession *fsess);

typedef enum
{
	GF_PROP_FORBIDEN=0,
	GF_PROP_SINT,
	GF_PROP_UINT,
	GF_PROP_LSINT,
	GF_PROP_LUINT,
	GF_PROP_BOOL,
	GF_PROP_FRACTION,
	GF_PROP_FLOAT,
	GF_PROP_DOUBLE,
	//string property, memory is duplicated when setting the property and managed internally
	GF_PROP_STRING,
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
} GF_PropType;

typedef struct
{
	GF_PropType type;
	union {
		u64 longuint;
		s64 longsint;
		u32 sint;
		s32 uint;
		Bool boolean;
		GF_Fraction frac;
		Fixed fnumber;
		Double number;
		//alloc/freed by filter
		char *data;
		//alloc/freed by filter if type is GF_PROP_STRING, otherwise const char *
		char *string;
		void *ptr;
	} value;

	u32 data_len;
} GF_PropertyValue;

const char *gf_props_get_type_name(u32 type);
GF_PropertyValue gf_props_parse_value(u32 type, const char *name, const char *value, const char *enum_values);

//helper macros to set properties
#define PROP_SINT(_val) (GF_PropertyValue){.type=GF_PROP_SINT, .value.sint = _val}
#define PROP_UINT(_val) (GF_PropertyValue){.type=GF_PROP_UINT, .value.uint = _val}
#define PROP_LONGSINT(_val) (GF_PropertyValue){.type=GF_PROP_LSINT, .value.longsint = _val}
#define PROP_LONGUINT(_val) (GF_PropertyValue){.type=GF_PROP_LUINT, .value.longuint = _val}
#define PROP_BOOL(_val) (GF_PropertyValue){.type=GF_PROP_BOOL, .value.boolean = _val}
#define PROP_FIXED(_val) (GF_PropertyValue){.type=GF_PROP_FLOAT, .value.fnumber = _val}
#define PROP_FLOAT(_val) (GF_PropertyValue){.type=GF_PROP_FLOAT, .value.fnumber = FLT2FIX(_val)}
#define PROP_FRAC(_num, _den) (GF_PropertyValue){.type=GF_PROP_FRACTION, .value.frac.num = _num, .value.frac.den = _den}
#define PROP_DOUBLE(_val) (GF_PropertyValue){.type=GF_PROP_DOUBLE, .value.number = _val}
#define PROP_STRING(_val) (GF_PropertyValue){.type=GF_PROP_STRING, .value.string = _val}
#define PROP_STRING(_val) (GF_PropertyValue){.type=GF_PROP_STRING, .value.string = _val}
#define PROP_NAME(_val) (GF_PropertyValue){.type=GF_PROP_NAME, .value.string = _val}
#define PROP_DATA(_val, _len) (GF_PropertyValue){.type=GF_PROP_DATA, .value.data = _val, .data_len=_len}
#define PROP_DATA_NO_COPY(_val, _len) (GF_PropertyValue){.type=GF_PROP_DATA_NO_COPY, .value.data = _val, .data_len=_len}
#define PROP_CONST_DATA(_val, _len) (GF_PropertyValue){.type=GF_PROP_CONST_DATA, .value.data = _val, .data_len=_len}

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

typedef struct
{
	//set to true to indicate the start of a new set of cap. The first cap is treated as cap_start=TRUE
	Bool start;
	//4cc of the capability listed.
	u32 code;

	GF_PropertyValue val;	//default type and value of the capability listed

	//when set to true the cap is valid if the value does not match
	Bool exclude;
	//name of the capability listed. the special value * is used to indicate that the capability is
	//solved at run time (the filter must be loaded)
	const char *name;
} GF_FilterCapability;

typedef enum
{
	//input is not supported
	GF_FPROBE_NOT_SUPPORTED = 0,
	//input is supported with potentially missing features
	GF_FPROBE_MAYBE_SUPPORTED,
	//input is supported
	GF_FPROBE_SUPPORTED
} GF_FilterProbeScore;

typedef struct __gf_filter_register
{
	//mandatory - name of the filter as used when setting up filters
	const char *name;
	//optional - author of the filter
	const char *author;
	//mandatory - description of the filter
	const char *description;
	//optional - size of private stack structure. The structure is allocated by the framework and arguments are setup before calling any of the filter functions
	u32 private_size;
	//indicates all calls shall take place in fthe main thread (running GL output) - to be refined
	Bool requires_main_thread;
	//indicates the max number of additional input PIDs - muxers and scalable filters typically set this to (u32) -1
	u32 max_extra_pids;

	//list of input capabilities
	const GF_FilterCapability *input_caps;
	//list of output capabilities
	const GF_FilterCapability *output_caps;

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
	//is_remove: indicates the input PID is removed (not yet implemented)
	GF_Err (*configure_pid)(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove);

	//optional - callback for filter init -  private stack of filter is allocated by framework)
	GF_Err (*initialize)(GF_Filter *filter);

	//optional - callback for filter desctruction -  private stack of filter is freed by framework
	void (*finalize)(GF_Filter *filter);

	//optional - callback for arguments update. If GF_OK is returned, the stack is updated accordingly
	//if function is NULL, all updatable arguments will be changed in the stack without the filter being notified
	GF_Err (*update_arg)(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *new_val);

	//optional - process a given event. Retruns TRUE if the event has to be canceled, FALSE otherwise
	Bool (*process_event)(GF_Filter *filter, GF_FilterEvent *evt);

	//required for source filters - probe the given URL, returning a score
	GF_FilterProbeScore (*probe_url)(const char *url, const char *mime);

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

GF_DownloadManager *gf_filter_get_download_manager(GF_Filter *filter);

void gf_filter_ask_rt_reschedule(GF_Filter *filter, u32 us_until_next);

void gf_filter_post_process_task(GF_Filter *filter);

void gf_filter_post_task(GF_Filter *filter, gf_fs_task_callback task_fun, void *udta, const char *task_name);

void gf_filter_set_setup_failure_callback(GF_Filter *filter, void (*on_setup_error)(GF_Filter *f, void *on_setup_error_udta, GF_Err e), void *udta);

void gf_filter_setup_failure(GF_Filter *filter, GF_Err reason);

void gf_filter_remove(GF_Filter *filter, GF_Filter *until_filter);

GF_FilterSession *gf_filter_get_session(GF_Filter *filter);
void gf_filter_session_abort(GF_FilterSession *fsess, GF_Err error_code);

GF_Filter *gf_fs_load_source(GF_FilterSession *fsess, char *url, char *parent_url);

GF_User *gf_fs_get_user(GF_FilterSession *fsess);

u32 gf_filter_get_ipid_count(GF_Filter *filter);
GF_FilterPid *gf_filter_get_ipid(GF_Filter *filter, u32 idx);

GF_FilterPid *gf_filter_pid_new(GF_Filter *filter);
void gf_filter_pid_remove(GF_FilterPid *pid);


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

Bool gf_filter_pid_is_filter_in_parents(GF_FilterPid *pid, GF_Filter *filter);


void gf_filter_pid_get_buffer_occupancy(GF_FilterPid *pid, u32 *max_slots, u32 *nb_pck, u32 *max_duration, u32 *duration);

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
GF_Err gf_filter_pid_copy_properties(GF_FilterPid *dst_pid, GF_FilterPid *src_pid);

const GF_PropertyValue *gf_filter_pid_get_property(GF_FilterPid *pid, u32 prop_4cc);
const GF_PropertyValue *gf_filter_pid_get_property_str(GF_FilterPid *pid, const char *prop_name);

const GF_PropertyValue *gf_filter_pid_enum_properties(GF_FilterPid *pid, u32 *idx, u32 *prop_4cc, const char **prop_name);

GF_Err gf_filter_pid_set_framing_mode(GF_FilterPid *pid, Bool requires_full_blocks);

u64 gf_filter_pid_query_buffer_duration(GF_FilterPid *pid);


const GF_PropertyValue *gf_filter_get_info(GF_Filter *filter, u32 prop_4cc);
const GF_PropertyValue *gf_filter_pid_get_info(GF_FilterPid *pid, u32 prop_4cc);


//signals EOS on a PID. Each filter needs to call this when EOS is reached on a given stream
//since there is no explicit link between input PIDs and output PIDs
void gf_filter_pid_set_eos(GF_FilterPid *pid);
Bool gf_filter_pid_has_seen_eos(GF_FilterPid *pid);

//may trigger a reconfigure signal to the filter. If reconfigure not OK, returns NULL and the pid passed to the filter NO LONGER EXISTS (implicit remove)
GF_FilterPacket * gf_filter_pid_get_packet(GF_FilterPid *pid);
void gf_filter_pid_drop_packet(GF_FilterPid *pid);
u32 gf_filter_pid_get_packet_count(GF_FilterPid *pid);
Bool gf_filter_pid_check_caps(GF_FilterPid *pid);

Bool gf_filter_pid_would_block(GF_FilterPid *pid);

void gf_filter_send_update(GF_Filter *filter, const char *filter_id, const char *arg_name, const char *arg_val);

//pck structure may be destroyed in this call, use the packet passed as return value 
GF_FilterPacket *gf_filter_pck_ref(GF_FilterPacket *pck);
void gf_filter_pck_unref(GF_FilterPacket *pck);

GF_FilterPacket *gf_filter_pck_new_alloc(GF_FilterPid *pid, u32 data_size, char **data);
GF_FilterPacket *gf_filter_pck_new_shared(GF_FilterPid *pid, const char *data, u32 data_size, packet_destructor destruct);
GF_FilterPacket *gf_filter_pck_new_ref(GF_FilterPid *pid, const char *data, u32 data_size, GF_FilterPacket *reference);
GF_Err gf_filter_pck_send(GF_FilterPacket *pck);

GF_Err gf_filter_pck_forward(GF_FilterPacket *reference, GF_FilterPid *pid);

const char *gf_filter_pck_get_data(GF_FilterPacket *pck, u32 *size);


GF_Err gf_filter_pck_set_property(GF_FilterPacket *pck, u32 prop_4cc, const GF_PropertyValue *value);
GF_Err gf_filter_pck_set_property_str(GF_FilterPacket *pck, const char *name, const GF_PropertyValue *value);
GF_Err gf_filter_pck_set_property_dyn(GF_FilterPacket *pck, char *name, const GF_PropertyValue *value);

//by defaults packet do not share properties with each-other, this functions enable passing the properties of
//one packet to another
GF_Err gf_filter_pck_merge_properties(GF_FilterPacket *pck_src, GF_FilterPacket *pck_dst);
const GF_PropertyValue *gf_filter_pck_get_property(GF_FilterPacket *pck, u32 prop_4cc);
const GF_PropertyValue *gf_filter_pck_get_property_str(GF_FilterPacket *pck, const char *prop_name);

const GF_PropertyValue *gf_filter_pck_enum_properties(GF_FilterPacket *pck, u32 *idx, u32 *prop_4cc, const char **prop_name);


GF_Err gf_filter_pck_set_framing(GF_FilterPacket *pck, Bool is_start, Bool is_end);
GF_Err gf_filter_pck_get_framing(GF_FilterPacket *pck, Bool *is_start, Bool *is_end);

GF_Err gf_filter_pck_set_dts(GF_FilterPacket *pck, u64 dts);
u64 gf_filter_pck_get_dts(GF_FilterPacket *pck);
GF_Err gf_filter_pck_set_cts(GF_FilterPacket *pck, u64 cts);
u64 gf_filter_pck_get_cts(GF_FilterPacket *pck);
u32 gf_filter_pck_get_timescale(GF_FilterPacket *pck);

GF_Err gf_filter_pck_set_duration(GF_FilterPacket *pck, u32 duration);
u32 gf_filter_pck_get_duration(GF_FilterPacket *pck);

GF_Err gf_filter_pck_set_sap(GF_FilterPacket *pck, u32 sap_type);
u32 gf_filter_pck_get_sap(GF_FilterPacket *pck);
GF_Err gf_filter_pck_set_interlaced(GF_FilterPacket *pck, u32 is_interlaced);
u32 gf_filter_pck_get_interlaced(GF_FilterPacket *pck);
GF_Err gf_filter_pck_set_corrupted(GF_FilterPacket *pck, Bool is_corrupted);
Bool gf_filter_pck_get_corrupted(GF_FilterPacket *pck);
GF_Err gf_filter_pck_set_eos(GF_FilterPacket *pck, Bool eos);
Bool gf_filter_pck_get_eos(GF_FilterPacket *pck);

GF_Err gf_filter_pck_set_seek_flag(GF_FilterPacket *pck, Bool is_seek);
Bool gf_filter_pck_get_seek_flag(GF_FilterPacket *pck);

void gf_fs_add_filter_registry(GF_FilterSession *fsess, const GF_FilterRegister *freg);
void gf_fs_remove_filter_registry(GF_FilterSession *session, GF_FilterRegister *freg);



void gf_fs_add_filter_registry(GF_FilterSession *fsess, const GF_FilterRegister *freg);
void gf_fs_remove_filter_registry(GF_FilterSession *session, GF_FilterRegister *freg);


enum
{
	//(uint) PID ID
	GF_PROP_PID_ID = GF_4CC('P','I','D','I'),
	GF_PROP_PID_ESID = GF_4CC('E','S','I','D'),

	//(uint) ID of originating service
	GF_PROP_PID_SERVICE_ID = GF_4CC('P','S','I','D'),
	GF_PROP_PID_CLOCK_ID = GF_4CC('C','K','I','D'),
	GF_PROP_PID_DEPENDENCY_ID = GF_4CC('D','P','I','D'),
	GF_PROP_PID_LANGUAGE = GF_4CC('P','L','A','N'),


	//(uint) media stream type, matching gpac stream types
	GF_PROP_PID_STREAM_TYPE = GF_4CC('P','M','S','T'),
	//(uint) object type indication , matching gpac OTI types
	GF_PROP_PID_OTI = GF_4CC('P','O','T','I'),
	//(bool) object type indication , matching gpac OTI types
	GF_PROP_PID_IN_IOD = GF_4CC('P','I','O','D'),

	//(rational) PID duration
	GF_PROP_PID_DURATION = GF_4CC('P','D','U','R'),

	//(uint) timescale of pid
	GF_PROP_PID_TIMESCALE = GF_4CC('T','I','M','S'),
	//(data) decoder config
	GF_PROP_PID_DECODER_CONFIG = GF_4CC('D','C','F','G'),
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

	//(rational) video FPS
	GF_PROP_PID_FPS = GF_4CC('V','F','P','F'),
	//(fraction) sample (ie pixel) aspect ratio
	GF_PROP_PID_SAR = GF_4CC('P','S','A','R'),
	//(fraction) picture aspect ratio
	GF_PROP_PID_PAR = GF_4CC('V','P','A','R'),
	//(uint) average bitrate
	GF_PROP_PID_BITRATE = GF_4CC('R','A','T','E'),

	//(string) URL of source file
	GF_PROP_PID_URL = GF_4CC('F','U','R','L'),
	//(string) URL of source file on the local file system, if any
	GF_PROP_PID_FILEPATH = GF_4CC('F','S','R','C'),
	//(string) mime type of source file if known
	GF_PROP_PID_MIME = GF_4CC('M','I','M','E'),
	//(string) file extension of source file if known
	GF_PROP_PID_FILE_EXT = GF_4CC('F','E','X','T'),
	//(uint) download rate in bits per second
	GF_PROP_PID_DOWN_RATE = GF_4CC('D','L','B','W'),

	//(longuint) NTP time stamp from sender
	GF_PROP_PCK_SENDER_NTP = GF_4CC('N','T','P','S'),
	//(longuint) byte offset in file of first byte in packet
	GF_PROP_PCK_BYTE_OFFSET = GF_4CC('B','O','F','F'),

};

const char *gf_props_4cc_get_name(u32 prop_4cc);


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
	GF_FEVT_ATTACH_SCENE,
	GF_FEVT_RESET_SCENE,
	GF_FEVT_QUALITY_SWITCH,
	GF_FEVT_VISIBILITY_HINT,
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

	/*params for GF_NET_CHAN_PLAY only: ranges in sec - if range is <0, then it is ignored (eg [2, -1] with speed>0 means 2 +oo) */
	Double start_range, end_range;
	/*params for GF_NET_CHAN_PLAY and GF_NET_CHAN_SPEED*/
	Double speed;

	/*params for GF_NET_CHAN_PLAY only: indicates this is the first PLAY on an element inserted from bcast*/
	u8 initial_broadcast_play;
	/*params for GF_NET_CHAN_PLAY only
		0: range is in media time
		1: range is in timesatmps
		2: range is in media time but timestamps should not be shifted (hybrid dash only for now)
	*/
	u8 timestamp_based;
} GF_FEVT_Play;

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


union __gf_filter_event
{
	GF_FEVT_Base base;
	GF_FEVT_Play play;
	GF_FEVT_AttachScene attach_scene;
	GF_FEVT_Event user_event;
	GF_FEVT_QualitySwitch quality_switch;
	GF_FEVT_VisibililityHint visibility_hint;
};



void gf_filter_pid_send_event(GF_FilterPid *pid, GF_FilterEvent *evt);



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

#ifdef __cplusplus
}
#endif

#endif	//_GF_FILTERS_H_

