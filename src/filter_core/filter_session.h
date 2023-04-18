/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2023
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

#ifdef GPAC_HAS_QJS
#include "../scenegraph/qjs_common.h"
#endif


#define GF_FILTER_SPEED_SCALER	1000

#define PID_IS_INPUT(__pid) ((__pid->pid==__pid) ? GF_FALSE : GF_TRUE)
#define PID_IS_OUTPUT(__pid) ((__pid->pid==__pid) ? GF_TRUE : GF_FALSE)
#define PCK_IS_INPUT(__pck) ((__pck->pck==__pck) ? GF_FALSE : GF_TRUE)
#define PCK_IS_OUTPUT(__pck) ((__pck->pck==__pck) ? GF_TRUE : GF_FALSE)

#define FSESS_CHECK_THREAD(__f) assert( !(__f)->process_th_id || ( (__f)->process_th_id == gf_th_id() ) );

struct __gf_prop_entry
{
	//parent filter session for property reservoir
	GF_FilterSession *session;
	volatile u32 reference_count;
	u32 p4cc;
	Bool name_alloc;
	char *pname;

	GF_PropertyValue prop;
	u32 alloc_size;
};

//we use the same value internally but with reverse meaning
#define GF_FS_FLAG_IMPLICIT_MODE	GF_FS_FLAG_NO_IMPLICIT


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
	//number of references hold by packet references - since these may be destroyed at the end of the referring filter
	//the pid might be dead. This is only used for pid props maps
	volatile u32 pckrefs_reference_count;
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

const GF_PropertyValue *gf_props_get_property(GF_PropertyMap *map, u32 prop_4cc, const char *name);

const GF_PropertyEntry *gf_props_get_property_entry(GF_PropertyMap *map, u32 prop_4cc, const char *name);

#if GF_PROPS_HASHTABLE_SIZE
u32 gf_props_hash_djb2(u32 p4cc, const char *str);
#else
#define gf_props_hash_djb2(_a, _b) 0
#endif

GF_Err gf_props_merge_property(GF_PropertyMap *dst_props, GF_PropertyMap *src_props, gf_filter_prop_filter filter_prop, void *cbk);

const GF_PropertyValue *gf_props_enum_property(GF_PropertyMap *props, u32 *io_idx, u32 *prop_4cc, const char **prop_name);

Bool gf_props_4cc_check_props();

void gf_props_del_property(GF_PropertyEntry *it);


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
void gf_fq_enum(GF_FilterQueue *fq, void (*enum_func)(void *udta1, void *item), void *udta);


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


/*URI relocators are used for containers like zip or ISO FF with file items. The relocator
is in charge of translating the URI, potentially extracting the associated resource and sending
back the new (local or not) URI. Only the interface is defined, URI translators are free to derive from them

relocate a URI - if NULL is returned, this relocator is not concerned with the URI
otherwise returns the translated URI
*/

#define GF_FS_URI_RELOCATOR	\
	Bool (*relocate_uri)(void *__self, const char *parent_uri, const char *uri, char *out_relocated_uri, char *out_localized_uri);		\

typedef struct __gf_uri_relocator GF_URIRelocator;

struct __gf_uri_relocator
{
	GF_FS_URI_RELOCATOR
};

typedef struct
{
	GF_FS_URI_RELOCATOR
	GF_FilterSession *sess;
	char *szAbsRelocatedPath;
} GF_FSLocales;


//packet flags
enum
{
	GF_PCKF_BLOCK_START = 0x80000000, //1<<31
	GF_PCKF_BLOCK_END = 1<<30,
	GF_PCKF_PROPS_CHANGED = 1<<29,
	GF_PCKF_INFO_CHANGED = 1<<28,
	GF_PCKF_CORRUPTED = 1<<27,
	GF_PCKF_SEEK = 1<<26,
	GF_PCKF_DUR_SET = 1<<25,
	GF_PCKF_PROPS_REFERENCE = 1<<24,
	//3 bits for SAP
	GF_PCK_SAP_POS = 21,
	GF_PCK_SAP_MASK = 0x7 << GF_PCK_SAP_POS,
	//2 bits for interlaced
	GF_PCK_ILACE_POS = 19,
	GF_PCK_ILACE_MASK = 0x3 << GF_PCK_ILACE_POS,
	//2 bits for clock type
	GF_PCK_CKTYPE_POS = 17,
	GF_PCK_CKTYPE_MASK = 0x3 << GF_PCK_CKTYPE_POS,
	//2 bits for crypt type
	GF_PCK_CRYPT_POS = 15,
	GF_PCK_CRYPT_MASK = 0x3 << GF_PCK_CRYPT_POS,
	//2 bits for crypt type
	GF_PCK_CMD_POS = 13,
	GF_PCK_CMD_MASK = 0x3 << GF_PCK_CMD_POS,
	GF_PCKF_FORCE_MAIN = 1<<12,
	//RESERVED bits [8,11]

	//2 bits for is_leading
	GF_PCK_ISLEADING_POS = 6,
	GF_PCK_ISLEADING_MASK = 0x3 << GF_PCK_ISLEADING_POS,
	//2 bits for depends_on
	GF_PCK_DEPENDS_ON_POS = 4,
	GF_PCK_DEPENDS_ON_MASK = 0x3 << GF_PCK_DEPENDS_ON_POS,
	//2 bits for depended_on
	GF_PCK_DEPENDED_ON_POS = 2,
	GF_PCK_DEPENDED_ON_MASK = 0x3 << GF_PCK_DEPENDED_ON_POS,
	//2 bits for redundant
	GF_PCK_REDUNDANT_MASK = 0x3,

	//quick defs
	GF_PCK_CMD_NONE = 0<<GF_PCK_CMD_POS,
	GF_PCK_CMD_PID_EOS = 1<<GF_PCK_CMD_POS,
	GF_PCK_CMD_PID_REM = 2<<GF_PCK_CMD_POS,
};

typedef struct __gf_filter_pck_info
{
	/*! packet flags */
	u32 flags;

	//packet timing in pid_props->timescale units
	u64 dts, cts;
	u32 duration;

	u64 byte_offset;
	u32 seq_num;
	s16 roll;
	u8 carousel_version_number;

} GF_FilterPckInfo;

struct __gf_filter_pck
{
	struct __gf_filter_pck *pck; //this object
	GF_FilterPid *pid;
	//filter emiting the packet - this is only needed to handle destruction of packets never dispatched
	//by the filter upon destruction. Set to NULL once the packet is dispatched
	GF_Filter *src_filter;

	//parent session, used to collect packet props references
	GF_FilterSession *session;

	//nb references of this packet
	volatile u32 reference_count;

	GF_FilterPckInfo info;

	char *data;
	u32 data_length;

	//for allocated memory packets
	u32 alloc_size;
	gf_fsess_packet_destructor destructor;
	//for packet reference  packets (sharing data from other packets)
	struct __gf_filter_pck *reference;

	GF_FilterFrameInterface *frame_ifce;
	
	// properties applying to this packet
	GF_PropertyMap *props;
	//pid properties applying to this packet
	GF_PropertyMap *pid_props;

	//for shared memory packets: 0: cloned mem, 1: read/write mem from source filter, 2: read-only mem from filter
	//note that packets with frame_ifce are always considered as read-only memory
	u8 filter_owns_mem;
	u8 is_dangling;

};

/*!
 *	Filter Session Task process function prototype
 */
typedef void (*gf_fs_task_callback)(GF_FSTask *task);

struct __gf_fs_task
{
	//flag set for tasks registered with main task list, eg having incremented the task_pending counter.
	//some tasks may not increment that counter (eg when requeued to a filer), so this simplifies
	//decrementing the counter
	Bool notified;
	Bool requeue_request;
	//if set, task can be pushed back in filter task list. If set to 2, filter is kept for scheduling event if mast task
	u32 can_swap;
	Bool blocking;
	Bool force_main;

	u64 schedule_next_time;


	gf_fs_task_callback run_task;
	GF_Filter *filter;
	GF_FilterPid *pid;
	const char *log_name;
	void *udta;
	u32 class_type;
};

void gf_fs_post_task(GF_FilterSession *fsess, gf_fs_task_callback fun, GF_Filter *filter, GF_FilterPid *pid, const char *log_name, void *udta);

//task type used to free up resources when a filter task is being canceled (configure error)
typedef enum
{
	//no free required
	TASK_TYPE_NONE=0,
	//task udta is a GF_FilterEvent
	TASK_TYPE_EVENT,
	//task udta is a struct _gf_filter_setup_failure (simple free needed)
	TASK_TYPE_SETUP,
	//task udta is a GF_UserTask structure (simple free needed), and task logname shall be freed
	TASK_TYPE_USER,
} GF_TaskClassType;


/* extended version of gf_fs_post_task
force_direct_call shall only be true for gf_filter_process_task
*/
void gf_fs_post_task_ex(GF_FilterSession *fsess, gf_fs_task_callback task_fun, GF_Filter *filter, GF_FilterPid *pid, const char *log_name, void *udta, Bool is_configure, Bool force_main_thread, Bool force_direct_call, GF_TaskClassType class_type);

void gf_fs_post_task_class(GF_FilterSession *fsess, gf_fs_task_callback task_fun, GF_Filter *filter, GF_FilterPid *pid, const char *log_name, void *udta, GF_TaskClassType class_type);

void gf_filter_pid_send_event_downstream(GF_FSTask *task);


typedef struct __gf_fs_thread
{
	//NULL for main thread
	GF_Thread *th;
	struct __gf_filter_session *fsess;
	u32 th_id;
	u32 nb_filters_pinned;

	Bool has_seen_eot; //set when no more tasks in global queue

	u64 nb_tasks;
	u64 run_time;
	u64 active_time;

#ifndef GPAC_DISABLE_REMOTERY
	u32 rmt_tasks;
	char rmt_name[20];
#endif

} GF_SessionThread;

typedef enum {
	GF_ARGTYPE_LOCAL = 0, //:arg syntax
	GF_ARGTYPE_GLOBAL, //--arg syntax
	GF_ARGTYPE_META, //old -+arg syntax
	GF_ARGTYPE_META_REPORTING,
} GF_FSArgItemType;

typedef struct
{
	char *argname;
	GF_FSArgItemType type;
	const char *meta_filter, *meta_opt;
	u8 opt_found;
	u8 meta_state;
} GF_FSArgItem;

void gf_fs_push_arg(GF_FilterSession *session, const char *szArg, Bool was_found, GF_FSArgItemType type, GF_Filter *meta_filter, const char *sub_opt_name);

enum
{
	GF_FS_BLOCK_ALL=0,
	GF_FS_NOBLOCK_FANOUT,
	GF_FS_NOBLOCK
};

//#define GF_FS_ENABLE_LOCALES


struct __gf_filter_session
{
	u32 flags;
	Bool use_locks;
	Bool direct_mode;
	volatile u32 tasks_in_process;
	Bool requires_solved_graph;
	//non blocking session mode:
	//0: session is blocking
	//1: session is non-blocking and first call to gf_fs_run (extra threads not started)
	//2: session is non-blocking and not first call to gf_fs_run (extra threads started)
	u32 non_blocking;

	GF_List *registry;
	GF_List *filters;

	GF_FilterQueue *tasks;
	GF_FilterQueue *main_thread_tasks;
	GF_FilterQueue *tasks_reservoir;
	volatile Bool in_main_sem_wait;
	volatile u32 active_threads;

	//if more than one thread, this mutex protects access to loaded filters list, to avoid concurrent calls to destruct and
	//filter testing (graph resolution, update sending, non thread-safe graph traversal...)
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
	//reservoir for reference property packets - we mutualize at session level to collect them
	//it is not possible to do so at filter or pid level because a prop ref packet may be destroyed after the source
	//pid/packet is destroyed, and we don't want to track them per pid/filter
	GF_FilterQueue *pcks_refprops_reservoir;


	GF_Mutex *props_mx;

	GF_Mutex *info_mx;

	GF_Mutex *ui_mx;

#ifndef GPAC_DISABLE_THREADS
	GF_List *threads;
#endif
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
	u32 blocking_mode;
	Bool in_final_flush;

	Bool reporting_on;
	/*user defined callback for all functions - cannot be NULL*/
	void *ui_opaque;
	/*the event proc. Return value depend on the event type, usually 0
	cannot be NULL if os_window_handler is specified and dont_override_window_proc is set
	may be NULL otherwise*/
	Bool (*ui_event_proc)(void *opaque, GF_Event *event);

	volatile u32 pid_connect_tasks_pending;

	GF_List *event_listeners;
	GF_Mutex *evt_mx;
	u32 in_event_listener;

	GF_DownloadManager *download_manager;

#ifndef GPAC_DISABLE_FONTS
	struct _gf_ft_mgr *font_manager;
#endif

	u32 default_pid_buffer_max_us, decoder_pid_buffer_max_us;
	u32 default_pid_buffer_max_units;

#ifdef GPAC_MEMORY_TRACKING
	Bool check_allocs;
	u32 nb_alloc_pck, nb_realloc_pck;
#endif
	GF_Err last_connect_error, last_process_error;

	GF_FilterSessionCaps caps;

	u64 hint_clock_us;
	GF_Fraction64 hint_timestamp;

	//max filter chain allowed in the link resolution process
	u32 max_resolve_chain_len;
	//max sleep time
	u32 max_sleep;

	//protect access to link bank
	GF_Mutex *links_mx;
	GF_List *links;


	GF_List *parsed_args;

	char sep_args, sep_name, sep_frag, sep_list, sep_neg;
	char *blacklist;
	Bool init_done;

	GF_List *auto_inc_nums;
#ifndef GPAC_DISABLE_3D
	GF_List *gl_providers;
	volatile u32 nb_gl_filters;
#endif
	//internal video output to hidden window for GL context
	struct _video_out *gl_driver;

#ifdef GPAC_HAS_QJS
	struct JSContext *js_ctx;
	GF_List *jstasks;
	struct __jsfs_task *new_f_task, *del_f_task, *on_evt_task, *on_auth_task;
#endif

	gf_fs_on_filter_creation on_filter_create_destroy;
	void *rt_udta;
	Bool force_main_thread_tasks;

	void *ext_gl_udta;
	gf_fs_gl_activate ext_gl_callback;

#ifdef GF_FS_ENABLE_LOCALES
	GF_List *uri_relocators;
	GF_FSLocales locales;
#endif

#ifdef GPAC_CONFIG_EMSCRIPTEN
	Bool is_worker;
	volatile u32 pending_threads;
#endif
};

#ifdef GPAC_HAS_QJS
void jsfs_on_filter_created(GF_Filter *new_filter);
void jsfs_on_filter_destroyed(GF_Filter *del_filter);
Bool jsfs_on_event(GF_FilterSession *session, GF_Event *evt);
Bool jsfs_on_auth(GF_FilterSession *session, GF_Event *evt);
#endif

void gf_fs_reg_all(GF_FilterSession *fsess, GF_FilterSession *a_sess);

typedef struct
{
	u32 crc;
	s32 inc_val;
	GF_Filter *filter;
} GF_FSAutoIncNum;

#ifndef GPAC_DISABLE_3D
GF_Err gf_fs_check_gl_provider(GF_FilterSession *session);
GF_Err gf_fs_set_gl(GF_FilterSession *session, Bool do_activate);
#endif

typedef enum
{
	GF_FILTER_ARG_EXPLICIT = 0,
	GF_FILTER_ARG_INHERIT,
	GF_FILTER_ARG_INHERIT_SOURCE_ONLY,
	GF_FILTER_ARG_EXPLICIT_SOURCE,
	GF_FILTER_ARG_EXPLICIT_SOURCE_NO_DST_INHERIT,
	GF_FILTER_ARG_EXPLICIT_SINK,
} GF_FilterArgType;

typedef enum
{
	//filter cannot be cloned
	GF_FILTER_NO_CLONE=0,
	//filter can be cloned
	GF_FILTER_CLONE,
	//filter may be cloned in implicit link session mode only
	GF_FILTER_CLONE_PROBE,
} GF_FilterCloneType;


typedef enum
{
	GF_FILTER_ENABLED = 0,
	GF_FILTER_DISABLED,
	GF_FILTER_DISABLED_HIDE,
} GF_FilterDisableType;

//#define DEBUG_BLOCKMODE

struct __gf_filter
{
	const GF_FilterRegister *freg;

	//name of the filter - may be overiden by the filter instance
	char *name;

	//ID of the filter to setup connections
	char *id;
	//list of IDs of filters usable as sources for this filter. NULL means any source
	char *source_ids;
	//original source IDs in case we use dynamic clone. If present, the source_ids contains resolved identifiers (eg ServiceID=234)
	//but dynamic_source_ids still contains the unresolved pattern (eg ServiceID=*)
	char *dynamic_source_ids;

	char *restricted_source_id;
	
	//parent media session
	GF_FilterSession *session;

	//indicates the max number of additional input PIDs - muxers and scalable filters typically set this to (u32) -1
	u32 max_extra_pids;
	volatile u32 nb_sparse_pids;

	u32 subsession_id, subsource_id;

	Bool (*on_setup_error)(GF_Filter *f, void *on_setup_error_udta, GF_Err e);
	void *on_setup_error_udta;
	GF_Filter *on_setup_error_filter;

	//internal copy of argument string
	char *orig_args;
	//remember how the args were set, either by explicit loading of filters or during filter chain
	GF_FilterArgType arg_type;
	//allocated pointer to the argument string for source filters
	char *src_args;
	//allocated argument string of the destingation filter for filters dynamically loaded, if any
	char *dst_args;
	//filter tag
	char *tag;
	//filter itag
	char *itag;

	//tasks pending for this filter. The first task in this list is also present in the filter session
	//task list in order to avoid locking the main task list with a mutex
	GF_FilterQueue *tasks;
	//set to true when the filter is present or to be added in the main task list
	//this variable is unset in a zone protected by task_mx
	volatile Bool scheduled_for_next_task;
	//set to true when the filter is being processed by a thread
	volatile Bool in_process;
	u32 process_th_id, restrict_th_idx;
	//user data for the filter implementation
	void *filter_udta;

	Bool has_out_caps;
	Bool in_force_flush;

	GF_FilterDisableType disabled;
	//set to true before calling filter process() callback, and reset to false right after
	Bool in_process_callback;
	Bool no_probe;
	Bool no_inputs;
	Bool is_blocking_source;
	Bool force_demux;

	s32 nb_pids_playing;

	//list of pids connected to this filter
	GF_List *input_pids;
	u32 num_input_pids;
	//list of pids created by this filter
	GF_List *output_pids;
	u32 num_output_pids;
	u32 num_out_pids_not_connected;

	//reservoir for packets with allocated memory
	GF_FilterQueue *pcks_alloc_reservoir;
	//reservoir for packets with shared memory
	GF_FilterQueue *pcks_shared_reservoir;
	//reservoir for packets instances - the ones stored in the pid destination(s) with shared memory
	GF_FilterQueue *pcks_inst_reservoir;

	GF_Mutex *pcks_mx;

	//!this mutex protects:
	//- the filter task queue, when reordering tasks for later processing while other threads try to post to the filter task queue
	//- the list of input pid and output pid destinations, which can be added from different threads for a same pid (fan-out)
	//-the blocking state of the filter
	GF_Mutex *tasks_mx;

	//list of output pids to be configured
	Bool has_pending_pids;
	GF_FilterQueue *pending_pids;

	volatile u32 in_pid_connection_pending;
	volatile u32 out_pid_connection_pending;
	volatile u32 pending_packets;
	volatile u32 nb_ref_packets;

	volatile u32 stream_reset_pending;
	volatile u32 num_events_queued;
	volatile u32 detach_pid_tasks_pending;
	volatile u32 nb_shared_packets_out;
	volatile u32 abort_pending;
	GF_List *postponed_packets;

	//list of blacklisted filtered registries
	GF_List *blacklisted;

	//pointer to the original filter being cloned - only used for graph setup, reset after
	GF_Filter *cloned_from;

	//pointer to the current destination filter to connect to next in the chain - only used for graph setup, reset after
	GF_Filter *dst_filter;
	//pointer to the target filter to connect to
	GF_Filter *target_filter;

	//list of direct destination filters for this filter during graph resolution
	GF_List *destination_filters;
	//list of potential destination for which we need to perform link resolution
	GF_List *destination_links;
	//temp list of input PIDs during graph resolution, added during pid_init task and removed at pid_configure task
	GF_List *temp_input_pids;

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
	//number of processing errors in the lifetime of the filter
	u32 nb_errors;

	//number of bytes sent by this filter
	u64 nb_bytes_sent;
	//number of microseconds this filter was active
	u64 time_process;

#ifdef GPAC_MEMORY_TRACKING
	//various stats in mem tracking mode, mostly used to detect heavy alloc/free usage by the filter
	u64 stats_mem_allocated;
	u32 stats_nb_alloc, stats_nb_realloc, stats_nb_calloc, stats_nb_free;
	u32 nb_process_since_reset, nb_consecutive_process;
	u32 max_stats_nb_alloc, max_stats_nb_realloc, max_stats_nb_calloc, max_stats_nb_free;
	u32 max_nb_consecutive_process, max_nb_process;
#endif

	volatile u32 would_block; //concurrent inc/dec
#ifdef DEBUG_BLOCKMODE
	//sets once broken blocking mode has been detected
	Bool blockmode_broken;
#endif

	//per-filter buffer options
	u32 pid_buffer_max_us, pid_buffer_max_units, pid_decode_buffer_max_us;

	//requested by a filter to disable blocking
	Bool prevent_blocking;

	//filter destroy task has been posted
	Bool finalized;
	//filter is scheduled for removal: any filter connected to this filter will
	//not be checked for graph resolution - destroy has not yet been posted
	//if value is 2, packets are still dispatched to this PID instance (pid reconfig)
	u32 removed;
	//setup has been notified
	Bool setup_notified;
	//filter loaded to solve a filter chain - special value 2 is for dummy reframer when forcing demux
	u32 dynamic_filter;
	//filter block EOS queries
	Bool block_eos;
	//set when one input pid of the filter has been marked for removal through gf_filter_remove_src
	//this prevents dispatching pid_remove as packets that would no longer be consumed
	Bool marked_for_removal;
	//sticky filters won't unload if all inputs are deconnected. Useful for sink filters
	//2 means temporary sticky, used when reconfiguring filter chain
	u32 sticky;
	//explicitly loaded filters are usually not cloned, except if this flag is set
	GF_FilterCloneType clonable;
	//set to true during pid link resolution for filters accepting a single pid
	Bool in_link_resolution;
	//one of the output PID needs reconfiguration
	volatile u32 nb_caps_renegociate;

	//number of process tasks queued. There is only one "process" task allocated for the filter, but it is automatically reposted based on this value
	volatile u32 process_task_queued;

	//time in system clock at which the process should be called, used for real-time regulation of some filters
	u64 schedule_next_time;

	//clock (PCR) dispatch info
	u64 next_clock_dispatch;
	u32 next_clock_dispatch_timescale;
	GF_FilterClockType next_clock_dispatch_type;

	GF_Filter *cap_dst_filter;
	//capability negotiation for the input pid
	GF_PropertyMap *caps_negociate;
	//set to true of this filter was instantiated to resolve a capability negotiation between two filters
	Bool is_pid_adaptation_filter;
	/*destination pid instance we are swapping*/
	GF_FilterPidInst *swap_pidinst_dst;
	/*source pid instance we are swapping*/
	GF_FilterPidInst *swap_pidinst_src;
	Bool swap_needs_init;
	Bool swap_pending;

	//overloaded caps of the filter
	const GF_FilterCapability *forced_caps;
	u32 nb_forced_caps, nb_forced_bundles;
	//valid when a pid inst is waiting for a reconnection, NULL otherwise
	GF_List *detached_pid_inst;

	//index of the bundle input for dynamic filters
	s32 bundle_idx_at_resolution;
	//index of the cap bundle input for adaptation filters
	s32 cap_idx_at_resolution;

	//set to signal output pids should be reconfigured
	Bool reconfigure_outputs;
	//when set, indicates the filter uses PID property overwrite in its arguments, needed to rewrite the props at pid init time
	Bool user_pid_props;

	//for encoder filters, set to the corresponding stream type - used to discard filters during the resolution
	u32 encoder_stream_type;

	Bool act_as_sink;
	Bool require_source_id;
#ifndef GPAC_DISABLE_REMOTERY
	rmtU32 rmt_hash;
#endif

	//signals tha pid info has changed, to notify the filter chain
	Bool pid_info_changed;

	//set to 1 when one or more input pid to the filter is on end of state, set to 2 if the filter dispatch a packet while in this state
	u32 eos_probe_state;

	//error checking
	//number of packet release or created during a process() call
	u32 nb_pck_io;
	//number of consecutive errors during process()
	u32 nb_consecutive_errors;
	//system clock of first error
	u64 time_at_first_error;

	GF_Err in_connect_err;

	volatile u32 nb_main_thread_forced;
	Bool no_dst_arg_inherit;
	GF_List *source_filters;

	char *status_str;
	u32 status_str_alloc, status_percent;
	Bool report_updated;

	char *instance_description, *instance_version, *instance_author, *instance_help;
	GF_FilterArgs *instance_args;

	GF_Filter *multi_sink_target;

	Bool event_target;

	u64 last_schedule_task_time;

	//set to NULL, or to the only source filter for this filter
	//this is a helper for the graph resolver to avoid browing all input pids when checking
	//for cycles or fetching last defined ID.
	//typically helps for tiling case with hundreds of tiles
	GF_Filter *single_source;

	char *meta_instances;

#ifdef GPAC_HAS_QJS
	char *iname;
	JSValue jsval;
#endif
	//for external bindings
	void *rt_udta;
};

GF_Filter *gf_filter_new(GF_FilterSession *fsess, const GF_FilterRegister *freg, const char *args, const char *dst_args, GF_FilterArgType arg_type, GF_Err *err, GF_Filter *multi_sink_target, Bool dynamic_filter);
GF_Filter *gf_filter_clone(GF_Filter *filter, GF_Filter *source_filter);
void gf_filter_del(GF_Filter *filter);

Bool gf_filter_swap_source_register(GF_Filter *filter);

GF_Err gf_filter_new_finalize(GF_Filter *filter, const char *args, GF_FilterArgType arg_type);

GF_Filter *gf_fs_load_source_dest_internal(GF_FilterSession *fsess, const char *url, const char *args, const char *parent_url, GF_Err *err, GF_Filter *filter, GF_Filter *dst_filter, Bool for_source, Bool no_args_inherit, Bool *probe_only, const GF_FilterRegister **probe_reg);

void gf_filter_pid_inst_delete_task(GF_FSTask *task);

void gf_filter_pid_inst_reset(GF_FilterPidInst *pidinst);
void gf_filter_pid_inst_del(GF_FilterPidInst *pidinst);

void gf_filter_forward_clock(GF_Filter *filter);

void gf_filter_process_inline(GF_Filter *filter);

void gf_filter_pid_retry_caps_negotiate(GF_FilterPid *src_pid, GF_FilterPid *pid, GF_Filter *dst_filter);

void gf_filter_reset_pending_packets(GF_Filter *filter);

void gf_filter_instance_detach_pid(GF_FilterPidInst *pidi);

typedef struct
{
	char *name;
	char *val;
	//0: only on filter, 1: forward downstream, 2: forward upstream
	GF_EventPropagateType recursive;
} GF_FilterUpdate;


//structure for input pids, in order to handle fan-outs of a pid into several filters
struct __gf_filter_pid_inst
{
	//first two fields are the same as in GF_FilterPid for typecast
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
	volatile u32 discard_packets;

	Bool force_reconfig;

	//set by filter
	u32 discard_inputs;

	//amount of media data in us in the packet queue - concurrent inc/dec
	volatile s64 buffer_duration;

	volatile s32 detach_pending;
	Bool force_flush;

	void *udta;
	u32 udta_flags;

	//statistics per pid instance
	u64 last_pck_fetch_time;
	u64 stats_start_ts, stats_start_us;
	u32 cur_bit_size, avg_bit_rate, max_bit_rate, avg_process_rate, max_process_rate;
	u32 nb_processed, nb_sap_processed, nb_reagg_pck;
	//all times in us
	u64 total_process_time, total_sap_process_time;
	u64 max_process_time, max_sap_process_time;
	u64 first_frame_time;
	Bool is_end_of_stream;
	Bool is_playing, is_paused;
	u8 play_queued, stop_queued;
	
	volatile u32 nb_eos_signaled;

	Bool is_encoder_input;
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

	GF_Filter *alias_orig;

	GF_Fraction64 last_ts_drop;

	u64 last_buf_query_clock;
	u64 last_buf_query_dur;

	/*! last RT info update time - input pid only */
	u64 last_rt_report;
	/*! estimated round-trip time in ms - input pid only */
	u32 rtt;
	/*! estimated interarrival jitter in microseconds - input pid only */
	u32 jitter;
	/*! loss rate in per-thousand - input pid only */
	u32 loss_rate;

};

struct __gf_filter_pid
{
	//first two fields are the same as in GF_FilterPidInst for typecast
	struct __gf_filter_pid *pid; //self if output pid, or source pid if output
	GF_Filter *filter;

	char *name;
	GF_List *destinations;
	u32 num_destinations;
	GF_List *properties;
	Bool request_property_map;
	Bool pid_info_changed;
	Bool destroyed;
	u32 not_connected;
	Bool not_connected_ok;
	Bool removed;
	Bool direct_dispatch;
	volatile u32 init_task_pending;
	volatile Bool props_changed_since_connect;
	//number of shared packets (shared, frame interfaces or reference) still out there
	volatile u32 nb_shared_packets_out;

	GF_PropertyMap *infos;
	
	//set whenever an eos packet is dispatched, reset whenever a regular packet is dispatched
	Bool has_seen_eos;
	u32 nb_reaggregation_pending;

	//only valid for decoder output pids
	u32 max_buffer_unit;
	//max number of packets in each of the destination pids - concurrent inc/dec
	volatile u32 nb_buffer_unit;

	//times in us
	u64 max_buffer_time;
	u32 user_max_buffer_time, user_max_playout_time, user_min_playout_time;
	//max buffered duration of packets in each of the destination pids - concurrent inc/dec
	u64 buffer_duration;
	//true if the pid carries raw media
	Bool raw_media;
	//true if pid is sparse (may not have data for a long time)
	//a sparse pid always triggers blocking if parent filter has more than one output
	Bool is_sparse;
	//for stats only
	u32 stream_type, codecid;

	volatile u32 would_block; // concurrent set
	volatile u32 nb_decoder_inputs;

	Bool duration_init;
	u64 last_pck_dts, last_pck_cts, min_pck_cts, max_pck_cts;
	u32 min_pck_duration, nb_unreliable_dts, last_pck_dur;
	Bool recompute_dts;
	Bool ignore_blocking;

	u32 nb_pck_sent;
	//1000x speed value
	u32 playback_speed_scaler;

	GF_Fraction64 last_ts_sent;
	
	Bool initial_play_done;
	Bool is_playing;
	void *udta;
	u32 udta_flags;

	GF_PropertyMap *caps_negociate;
	Bool caps_negociate_direct;
	GF_List *caps_negociate_pidi_list;
	GF_List *adapters_blacklist;
	GF_Filter *caps_dst_filter;

	Bool ext_not_trusted;
	Bool user_buffer_forced;

	Bool require_source_id;
	//only used in filter_check_caps
	GF_PropertyMap *local_props;
	volatile u32 num_pidinst_del_pending;
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
void gf_filter_pid_reconfigure_task_discard(GF_FSTask *task);
void gf_filter_update_arg_task(GF_FSTask *task);
void gf_filter_pid_disconnect_task(GF_FSTask *task);
void gf_filter_remove_task(GF_FSTask *task);
void gf_filter_pid_detach_task(GF_FSTask *task);
void gf_filter_pid_detach_task_no_flush(GF_FSTask *task);

u32 gf_filter_caps_bundle_count(const GF_FilterCapability *caps, u32 nb_caps);

void gf_filter_post_remove(GF_Filter *filter);

typedef struct
{
	u32 *bundles_in_ok;
	u32 *bundles_cap_found;
	u32 *bundles_in_scores;
	u32 nb_allocs;
} GF_CapsBundleStore;

#define CAP_MATCH_LOADED_INPUT_ONLY		1
#define CAP_MATCH_LOADED_OUTPUT_ONLY	1<<1

u32 gf_filter_caps_to_caps_match(const GF_FilterRegister *src, u32 src_bundle_idx, const GF_FilterRegister *dst, u32 nb_in_bundles, GF_Filter *dst_filter, u32 *dst_bundle_idx, u32 for_dst_bundle, u32 *loaded_filter_flags, GF_CapsBundleStore *capstore);
Bool gf_filter_has_out_caps(const GF_FilterCapability *caps, u32 nb_caps);
Bool gf_filter_has_in_caps(const GF_FilterCapability *caps, u32 nb_caps);

void gf_filter_check_output_reconfig(GF_Filter *filter);
Bool gf_filter_reconf_output(GF_Filter *filter, GF_FilterPid *pid);

void gf_filter_renegociate_output_dst(GF_FilterPid *pid, GF_Filter *filter, GF_Filter *filter_dst, GF_FilterPidInst *dst_pidi, GF_FilterPidInst *src_pidi);

GF_Filter *gf_filter_pid_resolve_link(GF_FilterPid *pid, GF_Filter *dst, Bool *filter_reassigned);
GF_Filter *gf_filter_pid_resolve_link_check_loaded(GF_FilterPid *pid, GF_Filter *dst, Bool *filter_reassigned, GF_List *skip_if_in_filter_list, Bool *skipped);

GF_Filter *gf_filter_pid_resolve_link_for_caps(GF_FilterPid *pid, GF_Filter *dst, Bool check_reconfig_only);
u32 gf_filter_pid_resolve_link_length(GF_FilterPid *pid, GF_Filter *dst);

Bool gf_filter_pid_caps_match(GF_FilterPid *src_pid, const GF_FilterRegister *freg, GF_Filter *filter_inst, u8 *priority, u32 *dst_bundle_idx, GF_Filter *dst_filter, s32 for_bundle_idx);

void gf_filter_relink_dst(GF_FilterPidInst *pidinst, GF_Err reason);

void gf_filter_remove_internal(GF_Filter *filter, GF_Filter *until_filter, Bool keep_end_connections);

GF_FilterPacket *gf_filter_pck_new_shared_internal(GF_FilterPid *pid, const u8 *data, u32 data_size, gf_fsess_packet_destructor destruct, Bool intern_pck);

void gf_filter_sess_build_graph(GF_FilterSession *fsess, const GF_FilterRegister *freg);
void gf_filter_sess_reset_graph(GF_FilterSession *fsess, const GF_FilterRegister *freg);

Bool gf_fs_ui_event(GF_FilterSession *session, GF_Event *uievt);

GF_Err gf_filter_pck_send_internal(GF_FilterPacket *pck, Bool from_filter);

void gf_filter_pid_send_event_internal(GF_FilterPid *pid, GF_FilterEvent *evt, Bool force_downstream);

const GF_PropertyEntry *gf_filter_pid_get_property_entry(GF_FilterPid *pid, u32 prop_4cc);
const GF_PropertyEntry *gf_filter_pid_get_property_entry_str(GF_FilterPid *pid, const char *prop_name);

const GF_PropertyValue *gf_filter_pid_get_property_first(GF_FilterPid *pid, u32 prop_4cc);
const GF_PropertyValue *gf_filter_pid_get_property_str_first(GF_FilterPid *pid, const char *prop_name);

void gf_filter_pid_set_args(GF_Filter *filter, GF_FilterPid *pid);

Bool gf_filter_aggregate_packets(GF_FilterPidInst *dst);
enum
{
	EDGE_STATUS_NONE=0,
	EDGE_STATUS_ENABLED,
	EDGE_STATUS_DISABLED,
};

#define EDGE_LOADED_SOURCE_ONLY (1)
#define EDGE_LOADED_DEST_ONLY (1<<1)

typedef struct
{
	struct __freg_desc *src_reg;
	u16 src_cap_idx;
	u16 dst_cap_idx;
	u8 weight;
	u8 status;
	u8 priority;
	u8 loaded_filter_only;
	u32 disabled_depth;
	//stream type of the output cap of src. Might be:
	// -1 if multiple stream types are defined in the cap (demuxers, encoders/decoders bundles)
	// 0 if not specified
	// or a valid GF_STREAM_*
	s32 src_stream_type;
} GF_FilterRegEdge;

typedef struct __freg_desc
{
	const GF_FilterRegister *freg;
	u32 nb_edges, nb_alloc_edges, nb_bundles;
	GF_FilterRegEdge *edges;
	u32 dist;
	struct __freg_desc *destination;
	u32 cap_idx;
	u8 priority;
	u8 in_edges_enabling;
} GF_FilterRegDesc;

#ifdef GPAC_MEMORY_TRACKING
size_t gf_mem_get_stats(unsigned int *nb_allocs, unsigned int *nb_callocs, unsigned int *nb_reallocs, unsigned int *nb_free);
#endif

void gf_filter_post_process_task_internal(GF_Filter *filter, Bool use_direct_dispatch);

//get next option after path, NULL if not found
const char *gf_fs_path_escape_colon(GF_FilterSession *sess, const char *path);
const char *gf_fs_path_escape_colon_ex(GF_FilterSession *sess, const char *path, Bool *needs_escape, Bool for_source);

void gf_fs_check_graph_load(GF_FilterSession *fsess, Bool for_load);

void gf_filter_renegociate_output_task(GF_FSTask *task);

void gf_fs_unload_script(GF_FilterSession *fs, void *js_ctx);


Bool gf_fs_check_filter_register_cap_ex(const GF_FilterRegister *f_reg, u32 incode, GF_PropertyValue *cap_input, u32 outcode, GF_PropertyValue *cap_output, Bool exact_match_only, Bool out_cap_excluded);

Bool gf_filter_update_arg_apply(GF_Filter *filter, const char *arg_name, const char *arg_value, Bool is_sync_call);


GF_List *gf_filter_pid_compute_link(GF_FilterPid *pid, GF_Filter *dst);

GF_PropertyValue gf_filter_parse_prop_solve_env_var(GF_FilterSession *fs, GF_Filter *f, u32 type, const char *name, const char *value, const char *enum_values);
#endif //_GF_FILTER_SESSION_H_




