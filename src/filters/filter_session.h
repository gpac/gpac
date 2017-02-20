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


const GF_FilterRegister *ut_filter_register();


//atomic ref_count++ / ref_count--
#if defined(WIN32) || defined(_WIN32_WCE)

#define ref_count_inc(__v) InterlockedIncrement((int *) (__v))
#define ref_count_dec(__v) InterlockedDecrement((int *) (__v))

#else

#define ref_count_inc(__v) __sync_add_and_fetch((int *) (__v), 1)
#define ref_count_dec(__v) __sync_sub_and_fetch((int *) (__v), 1)

#endif

#define PID_IS_INPUT(__pid) ((__pid->pid==__pid) ? GF_FALSE : GF_TRUE)
#define PID_IS_OUTPUT(__pid) ((__pid->pid==__pid) ? GF_TRUE : GF_FALSE)
#define PCK_IS_INPUT(__pck) ((__pck->pck==__pck) ? GF_FALSE : GF_TRUE)
#define PCK_IS_OUTPUT(__pck) ((__pck->pck==__pck) ? GF_TRUE : GF_FALSE)

typedef struct
{
	//filter for which the property was allocated. Since properties may be passed down the filter chain
	//this ensures that we put the property in the source filter reservoir, otherwise we would accumulate
	//in the reservoir of the filter releasing the property last
	GF_Filter *filter;
	volatile u32 reference_count;
	u32 p4cc;
	Bool name_alloc;
	char *pname;

	GF_PropertyValue prop;

} GF_PropertyEntry;

#define HASH_TABLE_SIZE 255

typedef struct
{
	GF_List *hash_table[HASH_TABLE_SIZE];
	volatile u32 reference_count;
	GF_Filter *filter;
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



typedef struct __gf_filter_queue GF_FilterQueue;
//constructs a new fifo queue. If mx is set all pop/add/head operations are protected by the mutex
//otherwise, a lock-free version of the fifo is used
GF_FilterQueue *gf_fq_new(const GF_Mutex *mx);
void gf_fq_del(GF_FilterQueue *q, void (*item_delete)(void *) );
void gf_fq_add(GF_FilterQueue *q, void *item);
void *gf_fq_pop(GF_FilterQueue *q);
void *gf_fq_head(GF_FilterQueue *q);
u32 gf_fq_count(GF_FilterQueue *q);
void *gf_fq_get(GF_FilterQueue *fq, u32 idx);


typedef void (*gf_destruct_fun)(void *cbck);

//calls gf_free on p, used for LFQ / lists destructors
void gf_void_del(void *p);

typedef struct __gf_filter_pid_inst GF_FilterPidInst;

typedef struct __gf_filter_pck_inst
{
	struct __gf_filter_pck *pck; //source packet
	GF_FilterPidInst *pid;
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

	char *data;
	u32 data_length;

	//for allocated memory packets
	u32 alloc_size;
	//for shared memory packets
	Bool filter_owns_mem;
	packet_destructor destructor;
	//for packet reference  packets (sharing data from other packets)
	struct __gf_filter_pck *reference;

	// properties applying to this packet
	GF_PropertyMap *props;
	//pid properties applying to this packet
	GF_PropertyMap *pid_props;
};


typedef struct __gf_fs_task GF_FSTask;

typedef Bool (*task_callback)(GF_FSTask *task);

struct __gf_fs_task
{
	//flag set for tasks registered with main task list, eg having incremented the task_pending counter.
	//some tasks may not increment that counter (eg when requeued to a filer), so this simplifies
	//decrementing the counter
	Bool notified;
	task_callback run_task;
	GF_Filter *filter;
	GF_FilterPid *pid;
	const char *log_name;
	void *udta;
};

void gf_fs_post_task(GF_FilterSession *fsess, task_callback fun, GF_Filter *filter, GF_FilterPid *pid, const char *log_name, void *udta);

void gf_fs_send_update(GF_FilterSession *fsess, const char *fid, const char *name, const char *val);


typedef struct __gf_fs_thread
{
	//NULL for main thread
	GF_Thread *th;
	struct __gf_media_session *fsess;

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

	GF_List *registry;
	GF_List *filters;

	GF_FilterQueue *tasks;
	GF_FilterQueue *tasks_reservoir;

	GF_List *threads;
	GF_SessionThread main_th;

	GF_Semaphore *semaphore;

	volatile u32 sema_count;

	volatile u32 tasks_pending;

	Bool done;
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

	//tasks pending for this filter. The first task in this list is also present in the filter session
	//task list in order to avoid locking the main task list with a mutex
	GF_FilterQueue *tasks;
	volatile Bool scheduled_for_next_task;
	volatile Bool in_process;
	//user data for the filter implementation
	void *filter_udta;

	//list of pids connected to this filter
	GF_List *input_pids;
	//list of pids created by this filter
	GF_List *output_pids;

	//reservoir for packets with allocated memory
	GF_FilterQueue *pcks_alloc_reservoir;
	//reservoir for packets with shared memory
	GF_FilterQueue *pcks_shared_reservoir;
	//reservoir for packets instances - the ones stored in the pid destination(s) with shared memory
	GF_FilterQueue *pcks_inst_reservoir;

	//reservoir for property maps for PID and packets properties
	GF_FilterQueue *prop_maps_reservoir;
	//reservoir for property maps hash table entries (GF_Lists) for PID and packets properties
	GF_FilterQueue *prop_maps_list_reservoir;
	//reservoir for property entries  - properties may be inherited between packets
	GF_FilterQueue *prop_maps_entry_reservoir;

	GF_Mutex *pcks_mx;
	GF_Mutex *props_mx;
	GF_Mutex *tasks_mx;

	//reservoir for property entries  - properties may be inherited between packets
	GF_FilterQueue *pending_pids;

	volatile u32 pid_connection_pending;
	volatile u32 pending_packets;
};

GF_Filter *gf_filter_new(GF_FilterSession *fsess, const GF_FilterRegister *registry, const char *args);
void gf_filter_del(GF_Filter *filter);
Bool gf_filter_process_task(GF_FSTask *task);

typedef struct
{
	char *name;
	char *val;
} GF_FilterUpdate;

Bool gf_filter_update_arg_task(GF_FSTask *task);

//structure for input pids, in order to handle fan-outs of a pid into several filters
struct __gf_filter_pid_inst
{
	//first two fileds are the same as in GF_FilterPid for typecast
	struct __gf_filter_pid *pid; // source pid
	GF_Filter *filter;

	GF_FilterQueue *packets;

	GF_Mutex *pck_mx;

	GF_List *pck_reassembly;
	Bool requires_full_data_block;
	Bool last_block_ended;

	void *udta;
};

struct __gf_filter_pid
{
	//first two fileds are the same as in GF_FilterPidInst for typecast
	struct __gf_filter_pid *pid; //self if output pid, or source pid if output
	GF_Filter *filter;

	GF_List *destinations;
	GF_List *properties;
	Bool request_property_map;

	u32 nb_pck_sent;

	void *udta;
};


void gf_filter_pid_del(GF_FilterPid *pid);

void gf_filter_packet_destroy(GF_FilterPacket *pck);

Bool gf_filter_pid_init_task(GF_FSTask *task);

Bool gf_filter_pid_connect_task(GF_FSTask *task);
Bool gf_filter_pid_reconfigure_task(GF_FSTask *task);

#endif //_GF_FILTER_SESSION_H_




