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
 *  it under the terfsess of the GNU Lesser General Public License as published by
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

#ifndef _GF_FILTER_SESSION_H_
#define _GF_FILTER_SESSION_H_

#include <gpac/list.h>
#include <gpac/thread.h>
#include <gpac/filters.h>
#include <gpac/user.h>


 //atomic ref_count++ / ref_count--
#if defined(WIN32) || defined(_WIN32_WCE)
#include <Windows.h>
#include <WinBase.h>

#define safe_int_inc(__v) InterlockedIncrement((int *) (__v))
#define safe_int_dec(__v) InterlockedDecrement((int *) (__v))

#define safe_int_add(__v, inc_val) InterlockedAdd((int *) (__v), inc_val)
#define safe_int_sub(__v, dec_val) InterlockedAdd((int *) (__v), -dec_val)

#define safe_int64_add(__v, inc_val) InterlockedAdd64((LONG64 *) (__v), inc_val)
#define safe_int64_sub(__v, dec_val) InterlockedAdd64((LONG64 *) (__v), -dec_val)

#else

#define safe_int_inc(__v) __sync_add_and_fetch((int *) (__v), 1)
#define safe_int_dec(__v) __sync_sub_and_fetch((int *) (__v), 1)

#define safe_int_add(__v, inc_val) __sync_add_and_fetch((int *) (__v), inc_val)
#define safe_int_sub(__v, dec_val) __sync_sub_and_fetch((int *) (__v), dec_val)

#define safe_int64_add(__v, inc_val) __sync_add_and_fetch((int *) (__v), inc_val)
#define safe_int64_sub(__v, dec_val) __sync_sub_and_fetch((int *) (__v), dec_val)
#endif


#define GF_FILTER_SPEED_SCALER	1000

#define PID_IS_INPUT(__pid) ((__pid->pid==__pid) ? GF_FALSE : GF_TRUE)
#define PID_IS_OUTPUT(__pid) ((__pid->pid==__pid) ? GF_TRUE : GF_FALSE)
#define PCK_IS_INPUT(__pck) ((__pck->pck==__pck) ? GF_FALSE : GF_TRUE)
#define PCK_IS_OUTPUT(__pck) ((__pck->pck==__pck) ? GF_TRUE : GF_FALSE)

#define FSESS_CHECK_THREAD(__f) assert( !(__f)->process_th_id || ( (__f)->process_th_id == gf_th_id() ) );

typedef struct
{
	//parent filter session for property reservoir
	GF_FilterSession *session;
	volatile u32 reference_count;
	u32 p4cc;
	Bool name_alloc;
	char *pname;

	GF_PropertyValue prop;
	u32 alloc_size;

} GF_PropertyEntry;

#ifndef GF_PROPS_HASHTABLE_SIZE
#define GF_PROPS_HASHTABLE_SIZE 0
#endif

void gf_propmap_del(void *pmap);

typedef struct
{
#if GF_PROPS_HASHTABLE_SIZE
	GF_List *hash_table[GF_PROPS_HASHTABLE_SIZE];
#else
	GF_List *properties;
#endif
	volatile u32 reference_count;
	GF_FilterSession *session;
	//current timescale, cached for duration/buffer compute
	u32 timescale;
} GF_PropertyMap;

GF_PropertyMap * gf_props_new(GF_Filter *filter);
void gf_props_del(GF_PropertyMap *prop);
void gf_props_reset(GF_PropertyMap *prop);

GF_Err gf_props_set_property(GF_PropertyMap *map, u32 p4cc, const char *name, char *dyn_name, const GF_PropertyValue *value);
GF_Err gf_props_insert_property(GF_PropertyMap *map, u32 hash, u32 p4cc, const char *name, char *dyn_name, const GF_PropertyValue *value);

void gf_props_remove_property(GF_PropertyMap *map, u32 hash, u32 p4cc, const char *name);
GF_List *gf_props_get_list(GF_PropertyMap *map);

const GF_PropertyValue *gf_props_get_property(GF_PropertyMap *map, u32 prop_4cc, const char *name);

#if GF_PROPS_HASHTABLE_SIZE
u32 gf_props_hash_djb2(u32 p4cc, const char *str);
#else
#define gf_props_hash_djb2(_a, _b) 0
#endif

GF_Err gf_props_merge_property(GF_PropertyMap *dst_props, GF_PropertyMap *src_props, gf_filter_prop_filter filter_prop, void *cbk);

const GF_PropertyValue *gf_props_enum_property(GF_PropertyMap *props, u32 *io_idx, u32 *prop_4cc, const char **prop_name);

Bool gf_props_4cc_check_props();


typedef struct __gf_filter_queue GF_FilterQueue;
//constructs a new fifo queue. If mx is set all pop/add/head operations are protected by the mutex
//otherwise, a lock-free version of the fifo is used
GF_FilterQueue *gf_fq_new(const GF_Mutex *mx);
void gf_fq_del(GF_FilterQueue *fq, void (*item_delete)(void *) );
void gf_fq_add(GF_FilterQueue *fq, void *item);
void *gf_fq_pop(GF_FilterQueue *fq);
void *gf_fq_head(GF_FilterQueue *fq);
u32 gf_fq_count(GF_FilterQueue *fq);
void *gf_fq_get(GF_FilterQueue *fq, u32 idx);


typedef void (*gf_destruct_fun)(void *cbck);

//calls gf_free on p, used for LFQ / lists destructors
void gf_void_del(void *p);

typedef struct __gf_filter_pid_inst GF_FilterPidInst;

typedef struct __gf_filter_pck_inst
{
	struct __gf_filter_pck *pck; //source packet
	GF_FilterPidInst *pid;
	u8 pid_props_change_done;
	u8 pid_info_change_done;

	//DO NOT EXTEND UNLESS UPDATING CODE IN gf_filter_pck_send()
} GF_FilterPacketInstance;


typedef struct __gf_filter_pck_info
{
	//framing info is not set as a property but directly in packet
	Bool data_block_start;
	Bool data_block_end;

	Bool pid_props_changed;
	Bool pid_info_changed;

	//packet timing in pid_props->timescale units
	u64 dts, cts;
	u32 duration;

	u8 sap_type;
	u8 interlaced;
	u8 corrupted;
	u8 internal_command; //0: nothing (regular packet), 1: eos, 2: pid remove
	u8 clock_type;
	u8 seek_flag;
	u8 duration_set;
	u8 carousel_version_number;
	u8 dependency_flags;
	u8 crypt_flags;

	s16 roll;
	u64 byte_offset;

	Bool props_ref_only;
} GF_FilterPckInfo;

struct __gf_filter_pck
{
	struct __gf_filter_pck *pck; //this object
	GF_FilterPid *pid;
	//filter emiting the packet - this is only needed to handle destruction of packets never dispatched
	//by the filter upon destruction. Set to NULL once the packet is dispatched
	GF_Filter *src_filter;

	//nb references of this packet
	volatile u32 reference_count;

	GF_FilterPckInfo info;

	char *data;
	u32 data_length;

	//for allocated memory packets
	u32 alloc_size;
	//for shared memory packets
	Bool filter_owns_mem;
	packet_destructor destructor;
	//for packet reference  packets (sharing data from other packets)
	struct __gf_filter_pck *reference;

	GF_FilterHWFrame *hw_frame;
	
	// properties applying to this packet
	GF_PropertyMap *props;
	//pid properties applying to this packet
	GF_PropertyMap *pid_props;
};


struct __gf_fs_task
{
	//flag set for tasks registered with main task list, eg having incremented the task_pending counter.
	//some tasks may not increment that counter (eg when requeued to a filer), so this simplifies
	//decrementing the counter
	Bool notified;
	//set for tasks that have been requeued (i.e. no longer present in filter task list)
	Bool in_main_task_list_only;
	Bool requeue_request;
	Bool is_filter_process;

	u64 schedule_next_time;


	gf_fs_task_callback run_task;
	GF_Filter *filter;
	GF_FilterPid *pid;
	const char *log_name;
	void *udta;
};

void gf_fs_post_task(GF_FilterSession *fsess, gf_fs_task_callback fun, GF_Filter *filter, GF_FilterPid *pid, const char *log_name, void *udta);

void gf_fs_send_update(GF_FilterSession *fsess, const char *fid, GF_Filter *filter, const char *name, const char *val, u32 propagate_mask);
void gf_filter_pid_send_event_downstream(GF_FSTask *task);


typedef struct __gf_fs_thread
{
	//NULL for main thread
	GF_Thread *th;
	struct __gf_media_session *fsess;
	u32 th_id;
	
	Bool has_seen_eot; //set when no more tasks in global queue

	u64 nb_tasks;
	u64 run_time;
	u64 active_time;
} GF_SessionThread;

struct __gf_media_session
{
	Bool use_locks;
	Bool direct_mode;
	Bool task_in_process;
	Bool requires_solved_graph;
	Bool no_main_thread;
	Bool no_regulation;

	GF_List *registry;
	GF_List *filters;

	GF_FilterQueue *tasks;
	GF_FilterQueue *main_thread_tasks;
	GF_FilterQueue *tasks_reservoir;

	//if more than one thread, this mutex protects access to loaded filters list, to avoid concurrent calls to destruct and
	//filter testing (graph resolution, update sending, ...)
	GF_Mutex *filters_mx;

	//reservoir for property maps for PID and packets properties
	GF_FilterQueue *prop_maps_reservoir;
	//reservoir for property entries  - properties may be inherited between packets
	GF_FilterQueue *prop_maps_entry_reservoir;
	//reservoir for property entries with allocated data buffers - properties may be inherited between packets
	GF_FilterQueue *prop_maps_entry_data_alloc_reservoir;
#if GF_PROPS_HASHTABLE_SIZE
	//reservoir for property maps hash table entries (GF_Lists) for PID and packets properties
	GF_FilterQueue *prop_maps_list_reservoir;
#endif

	GF_Mutex *props_mx;

	GF_List *threads;
	GF_SessionThread main_th;

	//only used in forced lock mode
	GF_Mutex *tasks_mx;

	//we duplicate the semaphores for the main and other task list. This avoids having threads grabing tasks
	//posted for main thread, and re-notifying the sema which could then be grabbed by another thread
	//than the main, hence producing too many wake-ups
	//semaphore for tasks posted in main task list
	GF_Semaphore *semaphore_main;
	//semaphore for tasks posted in other task list
	GF_Semaphore *semaphore_other;

	volatile u32 tasks_pending;

	u32 nb_threads_stopped;
	GF_Err run_status;
	Bool disable_blocking;

	//FIXME, we should get rid of GF_User
	Bool user_init;
	GF_User static_user, *user;

	volatile u32 pid_connect_tasks_pending;

	GF_List *event_listeners;
	GF_Mutex *evt_mx;
	u32 in_event_listener;

	GF_DownloadManager *download_manager;
	volatile u32 nb_dm_users;

	u32 default_pid_buffer_max_us, decoder_pid_buffer_max_us;

#ifdef GPAC_MEMORY_TRACKING
	Bool check_allocs;
#endif
	GF_Err last_connect_error, last_process_error;

	GF_FilterSessionCaps caps;

	u64 hint_clock_us;
	Double hint_timestamp;

	char sep_args, sep_name, sep_frag, sep_list, sep_neg;
	const char *blacklist;
};

void gf_fs_reg_all(GF_FilterSession *fsess, GF_FilterSession *a_sess);

typedef enum
{
	GF_FILTER_ARG_EXPLICIT = 0,
	GF_FILTER_ARG_INHERIT,
	GF_FILTER_ARG_EXPLICIT_SOURCE,
	GF_FILTER_ARG_EXPLICIT_SINK,
} GF_FilterArgType;

struct __gf_filter
{
	const GF_FilterRegister *freg;

	//name of the filter - may be overiden by the filter instance
	char *name;

	//ID of the filter to setup connections
	char *id;
	//list of IDs of filters usable as sources for this filter. NULL means any source
	char *source_ids;

	//parent media session
	GF_FilterSession *session;

	//indicates the max number of additional input PIDs - muxers and scalable filters typically set this to (u32) -1
	u32 max_extra_pids;

	void (*on_setup_error)(GF_Filter *f, void *on_setup_error_udta, GF_Err e);
	void *on_setup_error_udta;
	GF_Filter *on_setup_error_filter;

	//internal copy of argument string
	char *orig_args;
	//remember how the args were set, either by explicit loading of filters or during filter chain
	GF_FilterArgType arg_type;
	//allocated pointer to the argument string for source filters
	char *src_args;
	//pointer to the argument string of the destingation filter for filters dynamically loaded
	const char *dst_args;

	//tasks pending for this filter. The first task in this list is also present in the filter session
	//task list in order to avoid locking the main task list with a mutex
	GF_FilterQueue *tasks;
	//set to true when the filter is present or to be added in the main task list
	//this variable is unset in a zone protected by task_mx
	volatile Bool scheduled_for_next_task;
	//set to true when the filter is being processed by a thread
	volatile Bool in_process;
	u32 process_th_id;
	//user data for the filter implementation
	void *filter_udta;

	Bool has_out_caps;
	
	//list of pids connected to this filter
	GF_List *input_pids;
	u32 num_input_pids;
	//list of pids created by this filter
	GF_List *output_pids;
	u32 num_output_pids;
	
	//reservoir for packets with allocated memory
	GF_FilterQueue *pcks_alloc_reservoir;
	//reservoir for packets with shared memory
	GF_FilterQueue *pcks_shared_reservoir;
	//reservoir for packets instances - the ones stored in the pid destination(s) with shared memory
	GF_FilterQueue *pcks_inst_reservoir;

	GF_Mutex *pcks_mx;
	GF_Mutex *tasks_mx;

	//list of output pids to be configured
	Bool has_pending_pids;
	GF_FilterQueue *pending_pids;

	volatile u32 in_pid_connection_pending;
	volatile u32 out_pid_connection_pending;
	volatile u32 pending_packets;

	volatile u32 stream_reset_pending;
	volatile u32 num_events_queued;

	GF_List *postponed_packets;

	//list of blacklisted filtered registries
	GF_List *blacklisted;

	//pointer to the original filter being cloned - only used for graph setup, reset after
	GF_Filter *cloned_from;

	//pointer to the destination filter to connect to next in the chain - only used for graph setup, reset after
	GF_Filter *dst_filter;
	//pointer to the target filter to connect to
	GF_Filter *target_filter;

	//statistics
	//number of tasks executed by this filter
	u64 nb_tasks_done;
	//number of packets processed by this filter
	u64 nb_pck_processed;
	//number of bytes processed by this filter
	u64 nb_bytes_processed;
	//number of packets sent by this filter
	u64 nb_pck_sent;
	//number of hardware frames packets sent by this filter
	u64 nb_hw_pck_sent;

	//number of bytes sent by this filter
	u64 nb_bytes_sent;
	//number of microseconds this filter was active
	u64 time_process;

#ifdef GPAC_MEMORY_TRACKING
	u64 stats_mem_allocated;
	u32 stats_nb_alloc, stats_nb_realloc, stats_nb_calloc, stats_nb_free, nb_alloc_pck, nb_realloc_pck;
	u32 nb_process_since_reset, nb_consecutive_process;
	u32 max_stats_nb_alloc, max_stats_nb_realloc, max_stats_nb_calloc, max_stats_nb_free;
	u32 max_nb_consecutive_process, max_nb_process;
#endif

	volatile u32 would_block; //concurrent inc/dec

	//filter destroy task has been posted
	Bool finalized;
	//filter is scheduled for removal: any filter connected to this filter will
	//not be checked for graph resolution - destroy has not yet been posted
	Bool removed;
	//setup has been notified
	Bool setup_notified;
	//filter loaded to solve a filter chain
	Bool dynamic_filter;
	//sticky filters won't unload if all inputs are deconnected. Usefull for sink filters
	//2 means temporary sticky, used when reconfiguring filter chain
	u32 sticky;
	//explicitly loaded filters are usually not cloned, except if this flag is set
	Bool clonable;
	//one of the output PID needs reconfiguration
	volatile u32 nb_caps_renegociate;

	volatile u32 process_task_queued;

	Bool skip_process_trigger_on_tasks;

	u64 schedule_next_time;

	u64 next_clock_dispatch;
	u32 next_clock_dispatch_timescale;
	GF_FilterClockType next_clock_dispatch_type;

	GF_PropertyMap *caps_negociate;
	Bool is_pid_adaptation_filter;
	GF_FilterPidInst *swap_pidinst;
	Bool swap_needs_init;

	const GF_FilterCapability *forced_caps;
	u32 nb_forced_caps;
	//valid when a pid inst is waiting for a reconnection, NULL otherwise
	GF_List *detached_pid_inst;

	s32 cap_idx_at_resolution;

	Bool reconfigure_outputs;

	Bool user_pid_props;
};

GF_Filter *gf_filter_new(GF_FilterSession *fsess, const GF_FilterRegister *registry, const char *args, const char *dst_args, GF_FilterArgType arg_type, GF_Err *err);
GF_Filter *gf_filter_clone(GF_Filter *filter);
void gf_filter_del(GF_Filter *filter);

Bool gf_filter_swap_source_registry(GF_Filter *filter);

GF_Err gf_filter_new_finalize(GF_Filter *filter, const char *args, GF_FilterArgType arg_type);

GF_Filter *gf_fs_load_source_dest_internal(GF_FilterSession *fsess, const char *url, const char *args, const char *parent_url, GF_Err *err, GF_Filter *filter, GF_Filter *dst_filter, Bool for_source);

void gf_filter_pid_inst_delete_task(GF_FSTask *task);

void gf_filter_pid_inst_reset(GF_FilterPidInst *pidinst);
void gf_filter_pid_inst_del(GF_FilterPidInst *pidinst);

void gf_filter_forward_clock(GF_Filter *filter);

void gf_filter_process_inline(GF_Filter *filter);

void gf_filter_pid_retry_caps_negotiate(GF_FilterPid *src_pid, GF_FilterPid *pid, GF_Filter *dst_filter);

typedef struct
{
	char *name;
	char *val;
	//0: only on filter, 1: forward downstream, 2: forward upstream
	u32 recursive;
} GF_FilterUpdate;

//structure for input pids, in order to handle fan-outs of a pid into several filters
struct __gf_filter_pid_inst
{
	//first two fileds are the same as in GF_FilterPid for typecast
	struct __gf_filter_pid *pid; // source pid
	GF_Filter *filter;

	//current properties for the pid - we cannot use pid->properties since there could be
	//multiple consumers running at different speed
	GF_PropertyMap *props;

	GF_FilterQueue *packets;

	GF_Mutex *pck_mx;

	GF_List *pck_reassembly;
	Bool requires_full_data_block;
	Bool last_block_ended;
	Bool first_block_started;
	//set during play/stop/reset phases
	Bool discard_packets;

	//set by filter
	Bool discard_inputs;

	//amount of media data in us in the packet queue - concurrent inc/dec
	volatile s64 buffer_duration;

	volatile s32 detach_pending;

	void *udta;

	//statistics per pid instance
	u64 last_pck_fetch_time;
	u64 stats_start_ts, stats_start_us;
	u32 cur_bit_size, avg_bit_rate, max_bit_rate, avg_process_rate, max_process_rate;
	u32 nb_processed, nb_sap_processed;
	//all times in us
	u64 total_process_time, total_sap_process_time;
	u64 max_process_time, max_sap_process_time;
	u64 first_frame_time;
	Bool is_end_of_stream;
	volatile u32 nb_eos_signaled;

	Bool is_decoder_input;
	GF_PropertyMap *reconfig_pid_props;
	
	//clock handling by the consumer: the clock values are not automatically dispatched to the output pids and are kept
	//available as regular packets in the input pid
	Bool handles_clock_references;
	//if clocks are handled internally:
	//number of clock packets present
	volatile u32 nb_clocks_signaled;
	//last clock value found
	u64 last_clock_value;
	//last clock value's timescale found
	u32 last_clock_timescale;
	//last clock type found
	GF_FilterClockType last_clock_type;
};

struct __gf_filter_pid
{
	//first two fileds are the same as in GF_FilterPidInst for typecast
	struct __gf_filter_pid *pid; //self if output pid, or source pid if output
	GF_Filter *filter;

	char *name;
	GF_List *destinations;
	u32 num_destinations;
	GF_List *properties;
	Bool request_property_map;
	Bool pid_info_changed;
	Bool destroyed;
	Bool not_connected_ok;
	Bool removed;
	volatile u32 discard_input_packets;
	volatile u32 init_task_pending;
	volatile Bool props_changed_since_connect;
	//number of shared packets (shared, hw frames or reference) still out there
	volatile u32 nb_shared_packets_out;

	//set whenever an eos packet is dispatched, reset whenever a regular packet is dispatched
	Bool has_seen_eos;
	u32 nb_reaggregation_pending;

	//only valid for decoder output pids
	u32 max_buffer_unit;
	//max number of packets in each of the destination pids - concurrent inc/dec
	volatile u32 nb_buffer_unit;

	//times in us
	u64 max_buffer_time;
	u32 user_max_buffer_time, user_max_playout_time;
	//max buffered duration of packets in each of the destination pids - concurrent inc/dec
	u64 buffer_duration;
	//true if the pid carries raw media
	Bool raw_media;

	volatile u32 would_block; // concurrent set
	volatile u32 nb_decoder_inputs;

	Bool duration_init;
	u64 last_pck_dts, last_pck_cts;
	u32 min_pck_duration;

	u32 nb_pck_sent;
	//1000x speed value
	u32 playback_speed_scaler;

	Bool initial_play_done;
	Bool is_playing;
	void *udta;

	GF_PropertyMap *caps_negociate;
	GF_FilterPidInst *caps_negociate_pidi;
	GF_List *adapters_blacklist;
	GF_Filter *caps_dst_filter;

	u32 forced_cap;
};


void gf_filter_pid_del(GF_FilterPid *pid);
void gf_filter_pid_del_task(GF_FSTask *task);

void gf_filter_packet_destroy(GF_FilterPacket *pck);

void gf_fs_cleanup_filters(GF_FilterSession *fsess);

/*specific task posting*/
void gf_filter_pid_post_init_task(GF_Filter *filter, GF_FilterPid *pid);
void gf_filter_pid_post_connect_task(GF_Filter *filter, GF_FilterPid *pid);

/*internal tasks definitions*/
void gf_filter_pid_reconfigure_task(GF_FSTask *task);
void gf_filter_update_arg_task(GF_FSTask *task);
void gf_filter_pid_disconnect_task(GF_FSTask *task);
void gf_filter_remove_task(GF_FSTask *task);
void gf_filter_pid_detach_task(GF_FSTask *task);

Bool filter_in_parent_chain(GF_Filter *parent, GF_Filter *filter);

u32 gf_filter_caps_bundle_count(const GF_FilterCapability *caps, u32 nb_caps);
u32 gf_filter_caps_to_caps_match(const GF_FilterRegister *src, u32 src_bundle_idx, const GF_FilterRegister *dst, GF_Filter *dst_filter, u32 *dst_bundle_idx, s32 for_dst_bundle);
Bool gf_filter_has_out_caps(const GF_FilterRegister *freg);

void gf_filter_check_output_reconfig(GF_Filter *filter);
Bool gf_filter_reconf_output(GF_Filter *filter, GF_FilterPid *pid);

void gf_filter_renegociate_output_dst(GF_FilterPid *pid, GF_Filter *filter, GF_Filter *filter_dst, GF_FilterPidInst *pidi, Bool reconfig_only);

GF_Filter *gf_filter_pid_resolve_link(GF_FilterPid *pid, GF_Filter *dst, Bool *filter_reassigned);
GF_Filter *gf_filter_pid_resolve_link_for_caps(GF_FilterPid *pid, GF_Filter *dst);
u32 gf_filter_pid_resolve_link_length(GF_FilterPid *pid, GF_Filter *dst);

Bool gf_filter_pid_caps_match(GF_FilterPid *src_pid, const GF_FilterRegister *freg, GF_Filter *filter_inst, u8 *priority, u32 *dst_bundle_idx, GF_Filter *dst_filter, s32 for_bundle_idx);

void gf_filter_relink_dst(GF_FilterPidInst *pidinst);

void gf_filter_remove_internal(GF_Filter *filter, GF_Filter *until_filter, Bool keep_end_connections);

GF_FilterPacket *gf_filter_pck_new_shared_internal(GF_FilterPid *pid, const char *data, u32 data_size, packet_destructor destruct, Bool intern_pck);

#endif //_GF_FILTER_SESSION_H_




