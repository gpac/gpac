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

} GF_PropertyEntry;

#define HASH_TABLE_SIZE 30

typedef struct
{
	GF_List *hash_table[HASH_TABLE_SIZE];
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

u32 gf_props_hash_djb2(u32 p4cc, const char *str);

GF_Err gf_props_merge_property(GF_PropertyMap *dst_props, GF_PropertyMap *src_props);

const GF_PropertyValue *gf_props_enum_property(GF_PropertyMap *props, u32 *io_idx, u32 *prop_4cc, const char **prop_name);

Bool gf_props_equal(const GF_PropertyValue *p1, const GF_PropertyValue *p2);
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
} GF_FilterPacketInstance;

struct __gf_filter_pck
{
	struct __gf_filter_pck *pck; //this object
	GF_FilterPid *pid;

	//nb references of this packet
	volatile u32 reference_count;
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
	u8 eos;
	u8 clock_discontinuity;
	u8 seek_flag;
	u8 carousel_version_number;
	u64 byte_offset;

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

	u32 schedule_next_time;


	gf_fs_task_callback run_task;
	GF_Filter *filter;
	GF_FilterPid *pid;
	const char *log_name;
	void *udta;
};

void gf_fs_post_task(GF_FilterSession *fsess, gf_fs_task_callback fun, GF_Filter *filter, GF_FilterPid *pid, const char *log_name, void *udta);

void gf_fs_send_update(GF_FilterSession *fsess, const char *fid, const char *name, const char *val);
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
	//reservoir for property maps hash table entries (GF_Lists) for PID and packets properties
	GF_FilterQueue *prop_maps_list_reservoir;
	//reservoir for property entries  - properties may be inherited between packets
	GF_FilterQueue *prop_maps_entry_reservoir;

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


	GF_List *event_listeners;
	GF_Mutex *evt_mx;
	u32 in_event_listener;

	GF_DownloadManager *download_manager;
	volatile u32 nb_dm_users;

};


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

	void (*on_setup_error)(GF_Filter *f, void *on_setup_error_udta, GF_Err e);
	void *on_setup_error_udta;

	//const pointer to the argument string
	const char *orig_args;
	//allocated pointer to the argument string for source filters
	char *src_args;

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

	volatile u32 pid_connection_pending;
	volatile u32 pending_packets;

	volatile u32 stream_reset_pending;

	GF_List *postponed_packets;

	//list of blacklisted filtered registries
	GF_List *blacklisted;

	//pointer to the original filter being cloned - only used for graph setup, reset after
	GF_Filter *cloned_from;

	//statistics
	//number of tasks executed by this filter
	u64 nb_tasks_done;
	//number of packets processed by this filter
	u64 nb_pck_processed;
	//number of bytes processed by this filter
	u64 nb_bytes_processed;
	//number of packets sent by this filter
	u64 nb_pck_sent;
	//number of bytes sent by this filter
	u64 nb_bytes_sent;
	//number of microseconds this filter was active
	u64 time_process;

	volatile u32 would_block; //concurrent inc/dec

	//finalized has been called
	Bool finalized;
	//filter is scheduled for removal: any filter connected to this filter will
	//not be checked for graph resolution
	Bool removed;
	//filter loaded to solve a filter chain
	Bool dynamic_filter;

	volatile u32 process_task_queued;

	Bool skip_process_trigger_on_tasks;

	u64 schedule_next_time;
};

typedef enum
{
	GF_FILTER_ARG_LOCAL = 0,
	GF_FILTER_ARG_GLOBAL,
	GF_FILTER_ARG_GLOBAL_SOURCE,
} GF_FilterArgType;

GF_Filter *gf_filter_new(GF_FilterSession *fsess, const GF_FilterRegister *registry, const char *args, GF_FilterArgType arg_type, GF_Err *err);
GF_Filter *gf_filter_clone(GF_Filter *filter);
void gf_filter_del(GF_Filter *filter);

void gf_filter_set_arg(GF_Filter *filter, const GF_FilterArgs *a, GF_PropertyValue *argv);
Bool gf_filter_swap_source_registry(GF_Filter *filter);

GF_Err gf_filter_new_finalize(GF_Filter *filter, const char *args, GF_FilterArgType arg_type);

GF_Filter *gf_fs_load_source_internal(GF_FilterSession *fsess, char *url, char *parent_url, GF_Err *err, GF_Filter *filter);

void gf_filter_pid_inst_delete_task(GF_FSTask *task);


typedef struct
{
	char *name;
	char *val;
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

	Bool discard_packets;

	//amount of media data in us in the packet queue - concurrent inc/dec
	volatile u64 buffer_duration;

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
	volatile u32 discard_input_packets;
	//set whenever an eos packet is dispatched, reset whenever a regular packet is dispatched
	Bool has_seen_eos;

	u32 max_buffer_unit;
	//max number of packets in each of the destination pids - concurrent inc/dec
	volatile u32 nb_buffer_unit;
	//times in us
	u32 max_buffer_time;
	//max buffered duration of packets in each of the destination pids - concurrent inc/dec
	u32 buffer_duration;

	volatile u32 would_block; // concurrent set
	
	Bool duration_init;
	u64 last_pck_dts, last_pck_cts;
	u32 min_pck_duration;

	u32 nb_pck_sent;
	//1000x speed value
	u32 playback_speed_scaler;

	void *udta;
};


void gf_filter_pid_del(GF_FilterPid *pid);

void gf_filter_packet_destroy(GF_FilterPacket *pck);

/*internal tasks definitions*/
void gf_filter_pid_init_task(GF_FSTask *task);
void gf_filter_pid_connect_task(GF_FSTask *task);
void gf_filter_pid_reconfigure_task(GF_FSTask *task);
void gf_filter_update_arg_task(GF_FSTask *task);
void gf_filter_pid_disconnect_task(GF_FSTask *task);
void gf_filter_remove_task(GF_FSTask *task);

#endif //_GF_FILTER_SESSION_H_




