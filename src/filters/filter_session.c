/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / gpac application
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

#include <gpac/list.h>
#include <gpac/thread.h>
#include <gpac/filters.h>

#define MEDIASESSION_LOCKFREE


const GF_FilterRegister *test_source_register();
const GF_FilterRegister *test_filter_register();
const GF_FilterRegister *test_sink_register();


/*
	TODO:
		- rewrite module API
		- add arguments for modules
*/



enum
{
	GF_FILTER_EVENT_INTERNAL=GF_FILTER_EVENT_CUSTOM+1,
};


GF_Err utfilter_load(GF_Filter *filter);
GF_Err utsource_load(GF_Filter *filter);
GF_Err utsink_load(GF_Filter *filter);

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


#define PID_IS_INPUT(__pid) ((__pid->pid==__pid) ? GF_FALSE : GF_TRUE)
#define PID_IS_OUTPUT(__pid) ((__pid->pid==__pid) ? GF_TRUE : GF_FALSE)
#define PCK_IS_INPUT(__pck) ((__pck->pck==__pck) ? GF_FALSE : GF_TRUE)
#define PCK_IS_OUTPUT(__pck) ((__pck->pck==__pck) ? GF_TRUE : GF_FALSE)


typedef struct __gf_filter_pid_inst GF_FilterPidInst;

#if defined(WIN32) || defined(_WIN32_WCE)

void ref_count_inc(volatile u32 *ref_count)
{
	InterlockedExchangeAdd((int *)ref_count, 1);
}

void ref_count_dec(volatile u32 *ref_count)
{
	InterlockedExchangeAdd((int *)ref_count, -1);
}

#else

void ref_count_inc(volatile u32 *ref_count)
{
	__sync_add_and_fetch((int *)ref_count, 1);
}

void ref_count_dec(volatile u32 *ref_count)
{
	__sync_sub_and_fetch((int *)ref_count, 1);
}

#endif



typedef struct __lf_item
{
	struct __lf_item *next;
	void *data;
} GF_LFQItem;

typedef struct
{
	//head element is dummy, never swaped
	GF_LFQItem *head;
	GF_LFQItem *tail;
} GF_LFQueue;

GF_LFQueue *gf_lfq_new()
{
	GF_LFQueue *q;
	GF_SAFEALLOC(q, GF_LFQueue);
	//create dummuy slot for head
	GF_SAFEALLOC(q->head, GF_LFQItem);
	q->tail = q->head;
	return q;
}

void gf_lfq_del(GF_LFQueue *q, void (*item_delete)(void *) )
{
	GF_LFQItem *it = q->head;
	//first item is dummy, doesn't hold a valid pointer
	it->data=NULL;
	while (it) {
		GF_LFQItem *ptr = it;
		it = it->next;
		if (ptr->data && item_delete) item_delete(ptr->data);
		gf_free(ptr);
	}
	gf_free(q);
}

void gf_lfq_add(GF_LFQueue *q, void *item)
{
	GF_LFQItem *it, *next, *tail;
	GF_SAFEALLOC(it, GF_LFQItem);
	it->data=item;

	while (1) {
		tail = q->tail;
		next = tail->next;
		if (next == tail->next) {
			if (next==NULL) {
				if (__sync_bool_compare_and_swap(&tail->next, next, it)) {
					break; // Enqueue is done.  Exit loop
				}
			} else {
				//tail not pointing at last node, move it
				__sync_bool_compare_and_swap(&q->tail, tail, next);
			}
		}
	}
	__sync_bool_compare_and_swap(&q->tail, tail, it);
}

void *gf_lfq_pop(GF_LFQueue *q)
{
	void *data=NULL;
	GF_LFQItem *tail, *next, *head;
	next = NULL;

	while (1 ) {
		head = q->head;
		tail = q->tail;
		next = head->next;
		//state seems OK
		if (head == q->head) {
			//head is at tail, we have an empty queue or some state issue
			if (head == tail) {
				//empty queue (first slot is dummy, we need to check next)
				if (next == NULL)
					return NULL;

				//swap back tail at next
				__sync_bool_compare_and_swap(&q->tail, tail, next);
			} else {
				data = next->data;
				//try to advance q->head to next
				if (__sync_bool_compare_and_swap(&q->head, head, next))
					break; //success!
			}
		}
	}
	free(head);
	return data;
}

void gf_lfq_del_item(GF_LFQueue *q, GF_LFQItem *cur_it)
{
}

void *gf_lfq_head(GF_LFQueue *q)
{
	return q->head->next ? q->head->next->data : NULL;
}

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


struct __gf_filter
{
	GF_FilterRegister *freg;

	//ID of the filter to setup connections
	char *id;
	//list of IDs of filters usable as sources for this filter. NULL means any source
	char *source_ids;

	//parent media session
	GF_FilterSession *session;
	//flag set when the filter functions are currently being called. Filters are not reentrant
	Bool in_process;
	//user data for the filter implementation
	void *filter_udta;

	//list of pids connected to this filter
	GF_List *input_pids;
	//list of pids created by this filter
	GF_List *output_pids;

#ifdef MEDIASESSION_LOCKFREE
	//reservoir for packets with allocated memory
	GF_LFQueue *pcks_alloc_reservoir;
	//reservoir for packets with shared memory
	GF_LFQueue *pcks_shared_reservoir;
	//reservoir for packets instances - the ones stored in the pid destination(s) with shared memory
	GF_LFQueue *pcks_inst_reservoir;


	//reservoir for property maps for PID and packets properties
	GF_LFQueue *prop_maps_reservoir;
	//reservoir for property maps hash table entries (GF_Lists) for PID and packets properties
	GF_LFQueue *prop_maps_list_reservoir;
	//reservoir for property entries  - properties may be inherited between packets
	GF_LFQueue *prop_maps_entry_reservoir;

#else
	GF_Mutex *pcks_mx;
	GF_List *pcks_alloc_reservoir;
	GF_List *pcks_shared_reservoir;
	GF_List *pcks_inst_reservoir;
	GF_Mutex *props_mx;
	GF_List *prop_maps_reservoir;
	GF_List *prop_maps_list_reservoir;
	GF_List *prop_maps_entry_reservoir;
#endif

	GF_FilterPidInst *pending_pid;
};

typedef struct __gf_fs_thread
{
	//NULL for main thread
	GF_Thread *th;
	struct __gf_media_session *ms;

	u64 nb_tasks;
	u64 run_time;
	u64 active_time;
} GF_SessionThread;

struct __gf_media_session
{
	GF_List *registry;
	GF_List *filters;
	GF_List *tasks;
	GF_LFQueue *tasks_reservoir;
	GF_List *threads;
	GF_SessionThread main_th;

	GF_Semaphore *semaphore;
	GF_Mutex *task_mx;
	u32 tasks_pending;

	Bool done;
};

//structure for input pids, in order to handle fan-outs of a pid into several filters
struct __gf_filter_pid_inst
{
	//first two fileds are the same as in GF_FilterPid for typecast
	struct __gf_filter_pid *pid; // source pid
	GF_Filter *filter;

	GF_List *packets;
	GF_Mutex *mx;

	Bool requires_full_data_block;
	Bool last_block_ended;
};

struct __gf_filter_pid
{
	//first two fileds are the same as in GF_FilterPidInst for typecast
	struct __gf_filter_pid *pid; //self if output pid, or source pid if output
	GF_Filter *filter;

	GF_List *destinations;
	GF_List *properties;
	Bool request_property_map;
	Bool requires_full_blocks_dispatch;
};

typedef struct __gf_fs_task GF_MSTask;

typedef Bool (*task_callback)(GF_MSTask *task);

struct __gf_fs_task
{
	task_callback run_task;
	GF_Filter *filter;
	GF_FilterPid *pid;
	void *udta;
};

void gf_fs_post_task(GF_FilterSession *ms, task_callback fun, GF_Filter *filter, GF_FilterPid *pid);

//forward declarations
void gf_filter_pid_del(GF_FilterPid *pid);
static void gf_filter_packet_destroy(GF_FilterPacket *pck);

void gf_filter_set_name(GF_Filter *filter, const char *name)
{
	assert(filter);

	filter->freg->name=name;
}

void gf_filter_set_udta(GF_Filter *filter, void *udta)
{
	assert(filter);

	filter->filter_udta = udta;
}

void *gf_filter_get_udta(GF_Filter *filter)
{
	assert(filter);

	return filter->filter_udta;
}

void gf_filter_set_id(GF_Filter *filter, const char *ID)
{
	assert(filter);

	if (filter->id) gf_free(filter->id);
	filter->id = ID ? gf_strdup(ID) : NULL;
}
void gf_filter_set_sources(GF_Filter *filter, const char *sources_ID)
{
	assert(filter);

	if (filter->source_ids) gf_free(filter->source_ids);
	filter->source_ids = sources_ID ? gf_strdup(sources_ID) : NULL;
}

static Bool gf_filter_set_args(GF_MSTask *task)
{
	assert(task->filter);
	assert(task->filter->freg);
	assert(task->filter->freg->update_args);

	task->filter->freg->update_args(task->filter);
	return GF_FALSE;
}

GF_Filter *gf_filter_new(GF_FilterSession *ms, const GF_FilterRegister *registry)
{
	GF_Filter *filter;
	assert(ms);

	GF_SAFEALLOC(filter, GF_Filter);
	if (!filter) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to alloc filter for %s\n", registry->name));
		return NULL;
	}
	filter->freg = registry;
	filter->session = ms;
	if (filter->freg->construct) {
		GF_Err e = filter->freg->construct(filter);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Error %s while instantiating filter %s\n", gf_error_to_string(e), registry->name));
			gf_free(filter);
			return NULL;
		}
	}

#ifdef MEDIASESSION_LOCKFREE
	filter->pcks_shared_reservoir = gf_lfq_new();
	filter->pcks_alloc_reservoir = gf_lfq_new();
	filter->pcks_inst_reservoir = gf_lfq_new();

	filter->prop_maps_list_reservoir = gf_lfq_new();
	filter->prop_maps_reservoir = gf_lfq_new();
	filter->prop_maps_entry_reservoir = gf_lfq_new();
#else
	filter->pcks_shared_reservoir = gf_list_new();
	filter->pcks_alloc_reservoir = gf_list_new();
	filter->pcks_inst_reservoir = gf_list_new();

	filter->prop_maps_list_reservoir = gf_list_new();
	filter->prop_maps_reservoir = gf_list_new();
	filter->prop_maps_entry_reservoir = gf_list_new();
	if (ms->threads) {
		filter->pcks_mx = gf_mx_new("FilterPackets");
		filter->props_mx = gf_mx_new("FilterProps");
	}
#endif

	gf_mx_p(ms->task_mx);
	gf_list_add(ms->filters, filter);
	gf_mx_v(ms->task_mx);

	if (filter->freg->update_args)
		gf_fs_post_task(ms, gf_filter_set_args, filter, NULL);

	return filter;
}

void gf_void_del(void *p)
{
	gf_free(p);
}
void gf_filterpacket_del(void *p)
{
	GF_FilterPacket *pck=(GF_FilterPacket *)p;
	if (pck->data) gf_free(pck->data);
	gf_free(p);
}

void gf_list_destruct(GF_List *l, void (*item_delete)(void *) )
{
	while (1) {
		void *p = gf_list_pop_back(l);
		if (!p) break;
		item_delete(p);
	}
	gf_list_del(l);
}

void gf_filter_del(GF_Filter *filter)
{
	assert(filter);

	if (filter->freg->destruct) {
		filter->freg->destruct(filter);
	}
	while (gf_list_count(filter->output_pids)) {
		GF_FilterPid *pid = gf_list_pop_back(filter->output_pids);
		gf_filter_pid_del(pid);
	}
	gf_list_del(filter->output_pids);

	gf_list_del(filter->input_pids);

#ifdef MEDIASESSION_LOCKFREE
	gf_lfq_del(filter->pcks_shared_reservoir, gf_void_del);
	gf_lfq_del(filter->pcks_inst_reservoir, gf_void_del);
	gf_lfq_del(filter->pcks_alloc_reservoir, gf_filterpacket_del);

	gf_lfq_del(filter->prop_maps_reservoir, gf_void_del);
	gf_lfq_del(filter->prop_maps_list_reservoir, gf_list_del);
	gf_lfq_del(filter->prop_maps_entry_reservoir, gf_void_del);
#else

	gf_list_destruct(filter->pcks_shared_reservoir, gf_void_del);
	gf_list_destruct(filter->pcks_inst_reservoir, gf_void_del);
	gf_list_destruct(filter->pcks_alloc_reservoir, gf_filterpacket_del);

	gf_list_destruct(filter->prop_maps_reservoir, gf_void_del);
	gf_list_destruct(filter->prop_maps_list_reservoir, gf_list_del);
	gf_list_destruct(filter->prop_maps_entry_reservoir, gf_void_del);
	gf_mx_del(filter->pcks_mx);
	gf_mx_del(filter->props_mx);
#endif


	if (filter->id) gf_free(filter->id);
	if (filter->source_ids) gf_free(filter->source_ids);
	gf_free(filter);
}


GF_FilterSession *gf_fs_new()
{
	u32 i, nb_threads=0;
	GF_FilterSession *ms;
	GF_SAFEALLOC(ms, GF_FilterSession);
	if (!ms) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to alloc media session\n"));
		return NULL;
	}
	ms->filters = gf_list_new();
	ms->tasks = gf_list_new();
	ms->tasks_reservoir = gf_lfq_new();
	ms->main_th.ms = ms;

	if (nb_threads) {
		ms->semaphore = gf_sema_new(GF_UINT_MAX, 0);
		ms->task_mx = gf_mx_new("MediaSessionTasks");
	}
	if (!ms->semaphore || !ms->task_mx)
		nb_threads=0;

	if (nb_threads) {
		ms->threads = gf_list_new();
		if (!ms->threads) {
			gf_mx_del(ms->task_mx);
			ms->task_mx=NULL;
			gf_sema_del(ms->semaphore);
			ms->semaphore=NULL;
			nb_threads=0;
		}
	}

	if (!ms->filters || !ms->tasks || !ms->tasks_reservoir) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to alloc media session\n"));
		ms->done=1;
		gf_fs_del(ms);
		return NULL;
	}

	for (i=0; i<nb_threads; i++) {
		GF_SessionThread *sess_thread;
		GF_SAFEALLOC(sess_thread, GF_SessionThread);
		if (!sess_thread) continue;
		sess_thread->th = gf_th_new("MediaSessionThread");
		if (!sess_thread->th) {
			gf_free(sess_thread);
			continue;
		}
		sess_thread->ms = ms;
		gf_list_add(ms->threads, sess_thread);
	}

	ms->registry = gf_list_new();
	gf_list_add(ms->registry, test_source_register());
	gf_list_add(ms->registry, test_filter_register());
	gf_list_add(ms->registry, test_sink_register());

	return ms;
}

void gf_fs_del(GF_FilterSession *ms)
{
	assert(ms);

	//temporary until we don't introduce ms_stop
	assert(ms->done);
	if (ms->filters) {
		while (gf_list_count(ms->filters)) {
			GF_Filter *filter = gf_list_pop_back(ms->filters);
			gf_filter_del(filter);
		}
		gf_list_del(ms->filters);
	}
	if (ms->tasks) {
		while (gf_list_count(ms->tasks)) {
			GF_MSTask *task = gf_list_pop_back(ms->tasks);
			gf_free(task);
		}
		gf_list_del(ms->tasks);
	}
	if (ms->tasks_reservoir)
		gf_lfq_del(ms->tasks_reservoir, gf_void_del);

	if (ms->threads) {
		while (gf_list_count(ms->threads)) {
			GF_SessionThread *sess_th = gf_list_pop_back(ms->threads);
			gf_th_del(sess_th->th);
			gf_free(sess_th);
		}
		gf_list_del(ms->threads);
	}
	if (ms->task_mx)
		gf_mx_del(ms->task_mx);
	if (ms->semaphore)
		gf_sema_del(ms->semaphore);
	gf_free(ms);
}

void gf_fs_post_task(GF_FilterSession *ms, task_callback task_fun, GF_Filter *filter, GF_FilterPid *pid)
{
	GF_MSTask *task;

	assert(ms);
	assert(filter);
	assert(task_fun);

	task = gf_lfq_pop(ms->tasks_reservoir);

	if (!task) {
		GF_SAFEALLOC(task, GF_MSTask);
		if (!task) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("No more memory to post new task\n"));
			return;
		}
	}
	task->filter = filter;
	task->pid = pid;
	task->run_task = task_fun;

	gf_mx_p(ms->task_mx);
	gf_list_add(ms->tasks, task);
	gf_mx_v(ms->task_mx);

	if (ms->semaphore && ! gf_sema_notify(ms->semaphore, 1)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Cannot notify scheduler of new task, semaphore failure\n"));
	}
}

GF_Filter *gf_fs_load_filter(GF_FilterSession *ms, const char *name)
{
	GF_Filter *filter = NULL;
	u32 i, count;
	count = gf_list_count(ms->registry);

	assert(ms);
	assert(name);

	for (i=0;i<count;i++) {
		const GF_FilterRegister *f_reg = gf_list_get(ms->registry, i);
		if (!strcmp(f_reg->name, name)) {
			filter = gf_filter_new(ms, f_reg);
			return filter;
		}
	}
	return NULL;
}

static Bool gf_filter_pid_process(GF_MSTask *task)
{
	GF_Err e;
	assert(task->filter);
	assert(task->filter->freg);
	assert(task->filter->freg->process);

	e = task->filter->freg->process(task->filter);

	//source filters, flush data if enough space available. If the sink  returns EOS, don't repost the task
	if ( !task->filter->input_pids && task->filter->output_pids && (e!=GF_EOS))
		return GF_TRUE;

	return GF_FALSE;
}

u32 gf_fs_thread_proc(GF_SessionThread *sess_thread)
{
	Bool is_main_thread = (sess_thread->th==NULL) ? GF_TRUE : GF_FALSE;
	GF_FilterSession *ms = sess_thread->ms;
	u32 thid =  1 + gf_list_find(ms->threads, sess_thread);
	u64 enter_time = gf_sys_clock_high_res();

	while (1) {
		u32 i, count;
		Bool requeue = GF_FALSE;
		u64 active_start;
		GF_Err e = GF_OK;
		GF_MSTask *task=NULL;

		//wait for something to be done
		gf_sema_wait(ms->semaphore);
		if (ms->done) break;

		gf_mx_p(ms->task_mx);

		active_start = gf_sys_clock_high_res();
		count = gf_list_count(ms->tasks);
		for (i=0; i<count; i++) {
			task = gf_list_get(ms->tasks, i);
			if (task && !task->filter->in_process) {
				gf_list_rem(ms->tasks, i);
				break;
			}
			task=NULL;
		}
		if (!task) {
			if (!ms->tasks_pending && is_main_thread) {
				gf_mx_v(ms->task_mx);
				break;
			}
			sess_thread->active_time += gf_sys_clock_high_res() - active_start;
			gf_sema_notify(ms->semaphore, 1);
			gf_mx_v(ms->task_mx);
			continue;
		} else {
			ms->tasks_pending++;
		}
		assert(task->filter->in_process == GF_FALSE);
		task->filter->in_process = GF_TRUE;
		gf_mx_v(ms->task_mx);


		if (task->run_task) {
			requeue = task->run_task(task);
		}

		gf_mx_p(ms->task_mx);
		task->filter->in_process = GF_FALSE;
		gf_mx_v(ms->task_mx);

		ms->tasks_pending--;
		sess_thread->nb_tasks++;

		if (requeue) {
			gf_mx_p(ms->task_mx);
			gf_list_add(ms->tasks, task);
			gf_mx_v(ms->task_mx);
			gf_sema_notify(ms->semaphore, 1);
		} else {
			memset(task, 0, sizeof(GF_MSTask));
			gf_lfq_add(ms->tasks_reservoir, task);
		}

		sess_thread->active_time += gf_sys_clock_high_res() - active_start;
	}
	sess_thread->run_time = gf_sys_clock_high_res() - enter_time;

	return 0;
}

GF_Err gf_fs_run(GF_FilterSession *ms)
{
	u32 i, nb_threads;
	assert(ms);

	nb_threads = gf_list_count(ms->threads);
	for (i=0;i<nb_threads; i++) {
		GF_SessionThread *sess_th = gf_list_get(ms->threads, i);
		gf_th_run(sess_th->th, gf_fs_thread_proc, sess_th);
	}
	ms->done = GF_FALSE;
	gf_fs_thread_proc(&ms->main_th);
	ms->done = GF_TRUE;
	for (i=0;i<nb_threads; i++) {
		gf_sema_notify(ms->semaphore, 1);
	}
	return GF_EOS;
}

static void gf_fs_print_thread_stats(GF_SessionThread *s, u32 idx)
{
	fprintf(stderr, "\tThread %d: run_time "LLU" us active_time "LLU" us nb_tasks "LLU" us\n", idx, s->run_time, s->active_time, s->nb_tasks);
}

void gf_fs_print_stats(GF_FilterSession *ms)
{
	u64 run_time=0, active_time=0, nb_tasks=0;
	u32 i, count=gf_list_count(ms->threads);
	fprintf(stderr, "Session threads %d\n", 1+count);
	gf_fs_print_thread_stats(&ms->main_th, 1);
	run_time+=ms->main_th.run_time;
	active_time+=ms->main_th.active_time;
	nb_tasks+=ms->main_th.nb_tasks;

	for (i=0; i<count; i++) {
		GF_SessionThread *s = gf_list_get(ms->threads, i);
		gf_fs_print_thread_stats(s, i+1);
		run_time+=s->run_time;
		active_time+=s->active_time;
		nb_tasks+=s->nb_tasks;
	}
	fprintf(stderr, "\nTotal: run_time "LLU" us active_time "LLU" us nb_tasks "LLU" us\n", run_time, active_time, nb_tasks);
}


static Bool filter_source_id_match(const char *id, const char *source_ids)
{
	while (source_ids) {
		char *sep = strchr(source_ids, ',');
		if (sep) {
			u32 len = sep - source_ids;
			if (!strncmp(id, source_ids, len)) return GF_TRUE;
			source_ids=sep+1;
		} else {
			if (!strcmp(id, source_ids)) return GF_TRUE;
			source_ids=NULL;
		}
	}
	return GF_FALSE;
}

void gf_filter_pid_inst_del(GF_FilterPidInst *pidinst)
{
	assert(pidinst);
	while (gf_list_count(pidinst->packets)) {
		GF_FilterPacketInstance *pcki = gf_list_pop_back(pidinst->packets);

		ref_count_dec(&pcki->pck->reference);
		if (!pcki->pck->reference_count) {
			gf_filter_packet_destroy(pcki->pck);
		}
		gf_free(pcki);
	}
	gf_list_del(pidinst->packets);
	gf_mx_del(pidinst->mx);
	gf_free(pidinst);
}

static Bool gf_filter_pid_update_internal_caps(GF_FilterPid *pid)
{
	u32 i, count;
	//update intenal pid caps
	pid->requires_full_blocks_dispatch = GF_TRUE;
	count = gf_list_count(pid->destinations);
	for (i=0; i<count; i++) {
		GF_FilterPidInst *pidinst = gf_list_get(pid->destinations, i);
		if (!pidinst->requires_full_data_block)
			pid->requires_full_blocks_dispatch=GF_FALSE;
	}
	return GF_FALSE;
}

void gf_filter_pid_update(GF_MSTask *task)
{
	u32 i, count;
	GF_Err e;


	//try to connect pid
	count = gf_list_count(task->filter->session->filters);
	for (i=0; i<count; i++) {
		GF_Filter *filter = gf_list_get(task->filter->session->filters, i);
		//we don't allow re-entrant PIDs
		if (filter==task->filter) continue;

		if (!filter_source_id_match(task->filter->id, filter->source_ids)) continue;

		//might be wrong in multithread, we would have to wait for the filter to exit
		assert(!filter->in_process);

		if (filter->freg->configure_pid) {
			u32 k, dst_count=gf_list_count(task->pid->destinations);
			Bool new_pid_inst=GF_FALSE;
			GF_FilterPidInst *pidinst=NULL;

			for (k=0; k<dst_count; k++) {
				pidinst = gf_list_get(task->pid->destinations, k);
				if (pidinst->filter==filter) {
					break;
				}
				pidinst=NULL;
			}
			//first connection of this PID to this filter
			if (!pidinst) {
				GF_SAFEALLOC(pidinst, GF_FilterPidInst);
				pidinst->pid = task->pid;
				pidinst->filter = filter;
				pidinst->packets = gf_list_new();
				pidinst->requires_full_data_block = GF_TRUE;
				pidinst->last_block_ended = GF_TRUE;
				new_pid_inst=GF_TRUE;
				if (task->pid->filter->session->task_mx)
					pidinst->mx = gf_mx_new("PacketFIFOMutex");
			}

			gf_mx_p(filter->session->task_mx);
			filter->in_process = GF_TRUE;
			gf_mx_v(filter->session->task_mx);

			e = filter->freg->configure_pid(filter, pidinst);

			gf_mx_p(filter->session->task_mx);
			filter->in_process = GF_FALSE;
			gf_mx_v(filter->session->task_mx);

			if (e==GF_OK) {
				//if new, register the new pid instance, and the source pid as input to this filer
				if (new_pid_inst) {
					gf_list_add(task->pid->destinations, pidinst);

					if (!filter->input_pids) filter->input_pids = gf_list_new();
					assert(gf_list_find(filter->input_pids, task->pid)<0);
					gf_list_add(filter->input_pids, task->pid);
				}
			} else {
				//error, if old pid remove from input
				if (!new_pid_inst) {
					gf_list_del_item(task->pid->destinations, pidinst);
					gf_list_del_item(filter->input_pids, task->pid);
				}
				gf_filter_pid_inst_del(pidinst);
			}
		}
	}
	//update intenal pid caps
	gf_filter_pid_update_internal_caps(task->pid);
}

GF_Err gf_filter_pid_set_framing_mode(GF_FilterPid *pid, Bool requires_full_blocks)
{
	GF_FilterPidInst *pidinst = (GF_FilterPidInst *)pid;

	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to set framing info on an output pid in filter %s\n", pid->filter->freg->name));
		return GF_BAD_PARAM;
	}
	pidinst->requires_full_data_block = requires_full_blocks;
	return GF_OK;
}

GF_FilterPid *gf_filter_pid_new(GF_Filter *filter)
{
	GF_FilterPid *pid;
	GF_SAFEALLOC(pid, GF_FilterPid);
	pid->filter = filter;
	pid->destinations = gf_list_new();
	pid->properties = gf_list_new();
	pid->request_property_map = GF_TRUE;
	if (!filter->output_pids) filter->output_pids = gf_list_new();
	gf_list_add(filter->output_pids, pid);
	pid->pid = pid;
	gf_fs_post_task(filter->session, gf_filter_pid_update, filter, pid);

	//source filters, flush data if enough space available. If the sink  returns EOS, don't repost the task
	if ( !filter->input_pids)
		gf_fs_post_task(filter->session, gf_filter_pid_process, filter, NULL);

	return pid;
}

static GFINLINE u32 hash_djb2(u32 p4cc, unsigned char *str)
{
	u32 hash = 5381;

	if (p4cc) {
		hash = ((hash << 5) + hash) + ((p4cc>>24)&0xFF);
		hash = ((hash << 5) + hash) + ((p4cc>>16)&0xFF);
		hash = ((hash << 5) + hash) + ((p4cc>>8)&0xFF);
		hash = ((hash << 5) + hash) + (p4cc&0xFF);
	} else {
		int c;
		while (c = *str++)
			hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	}
	return (hash % HASH_TABLE_SIZE);
}

GF_PropertyMap * gf_property_map_new(GF_Filter *filter)
{
	GF_PropertyMap *map;
	Bool created=GF_FALSE;

#ifdef MEDIASESSION_LOCKFREE
	map = gf_lfq_pop(filter->prop_maps_reservoir);
#else
	gf_mx_p(filter->props_mx);
	map = gf_list_pop_back(filter->prop_maps_reservoir);
	gf_mx_v(filter->props_mx);
#endif
	if (!map) {
		GF_SAFEALLOC(map, GF_PropertyMap);
		map->filter = filter;
		created=GF_TRUE;
	}
	assert(!map->reference_count);
	map->reference_count = 1;
	return map;
}

void gf_property_map_del_property(GF_PropertyMap *prop, GF_PropertyEntry *it)
{
	assert(it->reference_count);
	ref_count_dec(&it->reference_count);
	if (!it->reference_count) {
		Bool res;
		if (it->pname && it->name_alloc) gf_free(it->pname);

		if (it->prop.type==GF_PROP_STRING) {
			gf_free(it->prop.value.string);
			it->prop.value.string = NULL;
		}
		else if (it->prop.type==GF_PROP_DATA) {
			gf_free(it->prop.value.data);
			it->prop.value.data = NULL;
		}
		it->prop.data_len = 0;

#ifdef MEDIASESSION_LOCKFREE
		gf_lfq_add(it->filter->prop_maps_entry_reservoir, it);
#else
		gf_mx_p(it->filter->props_mx);
		gf_list_add(it->filter->prop_maps_entry_reservoir, it);
		gf_mx_v(it->filter->props_mx);
#endif
	}
}

void gf_property_map_del(GF_PropertyMap *prop)
{
	u32 i;
	for (i=0; i<HASH_TABLE_SIZE; i++) {
		if (prop->hash_table[i]) {
			GF_List *l = prop->hash_table[i];
			while (gf_list_count(l)) {
				gf_property_map_del_property(prop, (GF_PropertyEntry *) gf_list_pop_back(l) );
			}
			prop->hash_table[i] = NULL;
#ifdef MEDIASESSION_LOCKFREE
			gf_lfq_add(prop->filter->prop_maps_list_reservoir, l);
#else
			gf_mx_p(prop->filter->props_mx);
			gf_list_add(prop->filter->prop_maps_list_reservoir, l);
			gf_mx_v(prop->filter->props_mx);
#endif
		}
	}
	prop->reference_count = 0;

#ifdef MEDIASESSION_LOCKFREE
	gf_lfq_add(prop->filter->prop_maps_reservoir, prop);
#else
	gf_mx_p(prop->filter->props_mx);
	gf_list_add(prop->filter->prop_maps_reservoir, prop);
	gf_mx_v(prop->filter->props_mx);
#endif

}

void gf_filter_pid_del(GF_FilterPid *pid)
{
	while (gf_list_count(pid->destinations)) {
		gf_filter_pid_inst_del( gf_list_pop_back(pid->destinations) );
	}
	gf_list_del(pid->destinations);

	while (gf_list_count(pid->properties)) {
		gf_property_map_del( gf_list_pop_back(pid->properties) );
	}
	gf_list_del(pid->properties);
	gf_free(pid);
}

//purge existing property of same name
void gf_property_map_remove_property(GF_PropertyMap *map, u32 hash, u32 p4cc, const char *name)
{
	if (map->hash_table[hash]) {
		u32 i, count = gf_list_count(map->hash_table[hash]);
		for (i=0; i<count; i++) {
			GF_PropertyEntry *prop = gf_list_get(map->hash_table[hash], i);
			if ((p4cc && (p4cc==prop->p4cc)) || (name && !strcmp(prop->pname, name)) ) {
				gf_list_rem(map->hash_table[hash], i);
				gf_property_map_del_property(map, prop);
				break;
			}
		}
	}
}

static GF_List *gf_property_map_get_list(GF_PropertyMap *map)
{
	GF_List *l;
#ifdef MEDIASESSION_LOCKFREE
	l = gf_lfq_pop(map->filter->prop_maps_list_reservoir);
#else
	gf_mx_p(map->filter->props_mx);
	l = gf_list_pop_back(map->filter->prop_maps_list_reservoir);
	gf_mx_v(map->filter->props_mx);
#endif

	if (!l) l = gf_list_new();
	return l;
}

GF_Err gf_property_map_insert_property(GF_PropertyMap *map, u32 hash, u32 p4cc, const char *name, char *dyn_name, const GF_PropertyValue *value)
{
	GF_PropertyEntry *prop;

	if (! map->hash_table[hash] ) {
		map->hash_table[hash] = gf_property_map_get_list(map);
		if (!map->hash_table[hash]) return GF_OUT_OF_MEM;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("PropertyMap hash collision - %d enries\n", 1+gf_list_count(map->hash_table[hash]) ));
	}

#ifdef MEDIASESSION_LOCKFREE
	prop = gf_lfq_pop(map->filter->prop_maps_entry_reservoir);
#else
	gf_mx_p(map->filter->props_mx);
	prop = gf_list_pop_back(map->filter->prop_maps_entry_reservoir);
	gf_mx_v(map->filter->props_mx);
#endif

	if (!prop) {
		GF_SAFEALLOC(prop, GF_PropertyEntry);
		if (!prop) return GF_OUT_OF_MEM;
		prop->filter = map->filter;
	}

	prop->reference_count = 1;
	prop->p4cc = p4cc;
	prop->pname = name;
	if (dyn_name) {
		prop->pname = gf_strdup(dyn_name);
		prop->name_alloc=GF_TRUE;
	}

	memcpy(&prop->prop, value, sizeof(GF_PropertyValue));
	if (prop->prop.type == GF_PROP_STRING)
		prop->prop.value.string = value->value.string ? gf_strdup(value->value.string) : NULL;

	if (prop->prop.type != GF_PROP_DATA)
		prop->prop.data_len = 0;

	return gf_list_add(map->hash_table[hash], prop);
}
GF_Err gf_property_map_set_property(GF_PropertyMap *map, u32 p4cc, const char *name, char *dyn_name, GF_PropertyValue *value)
{
	u32 hash;
	GF_PropertyEntry *prop;
	hash = hash_djb2(p4cc, name);
	gf_property_map_remove_property(map, hash, p4cc, name);
	return gf_property_map_insert_property(map, hash, p4cc, name, dyn_name, value);
}

static GF_Err gf_filter_pid_set_property_full(GF_FilterPid *pid, u32 prop_4cc, const char *prop_name, char *dyn_name, const GF_PropertyValue *value)
{
	GF_PropertyMap *map;

	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to write property on input PID in filter %s - ignoring\n", pid->filter->freg->name));
		return GF_BAD_PARAM;
	}
	if (pid->request_property_map) {
		GF_PropertyMap *old_map = gf_list_last(pid->properties);
		map = gf_property_map_new(pid->filter);
		gf_list_add(pid->properties, map);
		pid->request_property_map = 0;
		//when creating a new map, ref_count of old map is decremented
		if (old_map) {
			assert(old_map->reference_count);
			ref_count_dec(&old_map->reference_count);
			if (!old_map->reference_count) {
				gf_list_del_item(pid->properties, old_map);
				gf_property_map_del(old_map);
			}
		}
	} else {
		map = gf_list_last(pid->properties);
	}
	assert(map);
	return gf_property_map_set_property(map, prop_4cc, prop_name, dyn_name, value);
}

GF_Err gf_filter_pid_set_property(GF_FilterPid *pid, u32 prop_4cc, const GF_PropertyValue *value)
{
	return gf_filter_pid_set_property_full(pid, prop_4cc, NULL, NULL, value);
}

GF_Err gf_filter_pid_set_property_str(GF_FilterPid *pid, const char *name, const GF_PropertyValue *value)
{
	return gf_filter_pid_set_property_full(pid, 0, name, NULL, value);
}

GF_Err gf_filter_pid_set_property_dyn(GF_FilterPid *pid, char *name, const GF_PropertyValue *value)
{
	return gf_filter_pid_set_property_full(pid, 0, NULL, name, value);
}

const GF_PropertyValue *gf_property_map_get_property(GF_PropertyMap *map, u32 prop_4cc, const char *name)
{
	u32 i, count, hash;
	hash = hash_djb2(prop_4cc, name);
	if (map->hash_table[hash] ) {
		count = gf_list_count(map->hash_table[hash]);
		for (i=0; i<count; i++) {
			GF_PropertyEntry *p = gf_list_get(map->hash_table[hash], i);

			if ((prop_4cc && (p->p4cc==prop_4cc)) || (p->pname && name && !strcmp(p->pname, name)) )
				return &p->prop;
		}
	}
	return NULL;
}

const GF_PropertyValue *gf_filter_pid_get_property(GF_FilterPid *pid, u32 prop_4cc, const char *prop_name)
{
	GF_PropertyMap *map;
	pid = pid->pid;
	map = gf_list_last(pid->properties);
	assert(map);
	return gf_property_map_get_property(map, prop_4cc, prop_name);
}


GF_FilterPacket *gf_filter_pck_new_alloc(GF_FilterPid *pid, u32 data_size, char **data)
{
	GF_FilterPacket *pck=NULL;
#ifdef MEDIASESSION_LOCKFREE
	GF_LFQItem *cur_it, *pck_it=NULL;
#else
	u32 i, count;
#endif

	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to allocate a packet on an input PID in filter %s\n", pid->filter->freg->name));
		return NULL;
	}

#ifdef MEDIASESSION_LOCKFREE
	cur_it=pid->filter->pcks_alloc_reservoir->head->next;
	while (cur_it) {
		GF_FilterPacket *cur=cur_it->data;
		if (cur->alloc_size >= data_size) {
			if (!pck || (pck->alloc_size > cur->alloc_size)) {
				pck = cur;
				pck_it = cur_it;
			}
		}
		cur_it = cur_it->next;
	}
#else
	count = gf_list_count(pid->filter->pcks_alloc_reservoir);
	for (i=0; i<count; i++) {
		GF_FilterPacket *cur = gf_list_get(pid->filter->pcks_alloc_reservoir, i);
		if (cur->alloc_size >= data_size) {
			if (!pck || (pck->alloc_size>cur->alloc_size)) {
				pck = cur;
			}
		}
	}
#endif

	//TODO - stop allocating after a while...
	if (!pck) {
		GF_SAFEALLOC(pck, GF_FilterPacket);
		pck->data = gf_malloc(sizeof(char)*data_size);
		pck->alloc_size = data_size;
	} else {
#ifdef MEDIASESSION_LOCKFREE
		//pop first item and swap pointers. We can safely do this since this filter
		//is the only one accessing the queue in pop mode, all others are just pushing to it
		//this may however imply that we don't get the best matching block size if new packets
		//were added to the list
		GF_FilterPacket *head_pck = gf_lfq_pop(pid->filter->pcks_alloc_reservoir);
		char *data = pck->data;
		u32 alloc_size = pck->alloc_size;
		pck->data = head_pck->data;
		pck->alloc_size = head_pck->alloc_size;
		head_pck->data = data;
		head_pck->alloc_size = alloc_size;
		pck = head_pck;
#else
		gf_mx_p(pid->filter->pcks_mx);
		gf_list_del_item(pid->filter->pcks_alloc_reservoir, pck);
		gf_mx_v(pid->filter->pcks_mx);
#endif

	}

	pck->pck = pck;
	pck->data_length = data_size;
	pck->pid = pid;
	if (data) *data = pck->data;
	pck->data_block_start = pck->data_block_end = GF_TRUE;

	//pid properties applying to this packet are the last defined ones
	pck->pid_props = gf_list_last(pid->properties);
	if (pck->pid_props) ref_count_inc(&pck->pid_props->reference_count);
	return pck;
}

GF_FilterPacket *gf_filter_pck_new_shared(GF_FilterPid *pid, const char *data, u32 data_size, packet_destructor destruct)
{
	GF_FilterPacket *pck;

	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to allocate a packet on an input PID in filter %s\n", pid->filter->freg->name));
		return NULL;
	}

#ifdef MEDIASESSION_LOCKFREE
	pck = gf_lfq_pop(pid->filter->pcks_shared_reservoir);
#else
	gf_mx_p(pid->filter->pcks_mx);
	pck = gf_list_pop_back(pid->filter->pcks_shared_reservoir);
	gf_mx_v(pid->filter->pcks_mx);
#endif

	if (!pck) {
		GF_SAFEALLOC(pck, GF_FilterPacket);
		if (!pck) return NULL;
	}
	pck->pck = pck;
	pck->pid = pid;
	pck->data = data;
	pck->data_length = data_size;
	pck->filter_owns_mem = GF_TRUE;
	pck->destructor = destruct;
	pck->data_block_start = pck->data_block_end = GF_TRUE;

	//pid properties applying to this packet are the last defined ones
	pck->pid_props = gf_list_last(pid->properties);
	if (pck->pid_props) ref_count_inc(&pck->pid_props->reference_count);

	return pck;
}

GF_FilterPacket *gf_filter_pck_new_ref(GF_FilterPid *pid, const char *data, u32 data_size, GF_FilterPacket *reference)
{
	GF_FilterPacket *pck;
	if (!reference) return NULL;
	reference=reference->pck;
	pck = gf_filter_pck_new_shared(pid, data, data_size, NULL);
	pck->reference = reference;
	assert(reference->reference_count);
	ref_count_inc(&reference->reference_count);
	if (!data || !data_size) {
		pck->data = reference->data;
		pck->data_length = reference->data_length;
	}
	return pck;
}

static void gf_filter_packet_destroy(GF_FilterPacket *pck)
{
	GF_FilterPid *pid = pck->pid;
	assert(pck->pid);

	if (pck->destructor) pck->destructor(pid->filter, pid, pck);

	if (pck->pid_props) {
		GF_PropertyMap *props = pck->pid_props;
		pck->pid_props = NULL;
		assert(props->reference_count);
		ref_count_dec(&props->reference_count);
		if (!props->reference_count) {
			gf_list_del_item(pck->pid->properties, props);
			gf_property_map_del(props);
		}
	}

	if (pck->props) {
		GF_PropertyMap *props = pck->props;
		pck->props=NULL;
		gf_property_map_del(props);
	}
	pck->data_length = 0;
	pck->pid = NULL;

	if (pck->filter_owns_mem) {

		if (pck->reference) {
			assert(pck->reference->reference_count);
			ref_count_dec(&pck->reference->reference_count);
			if (!pck->reference->reference_count) {
				gf_filter_packet_destroy(pck->reference);
			}
			pck->reference = NULL;
		}

#ifdef MEDIASESSION_LOCKFREE
		gf_lfq_add(pid->filter->pcks_shared_reservoir, pck);
#else
		gf_mx_p(pid->filter->pcks_mx);
		gf_list_add(pid->filter->pcks_shared_reservoir, pck);
		gf_mx_v(pid->filter->pcks_mx);
#endif
	} else {
#ifdef MEDIASESSION_LOCKFREE
		gf_lfq_add(pid->filter->pcks_alloc_reservoir, pck);
#else
		gf_mx_p(pid->filter->pcks_mx);
		gf_list_add(pid->filter->pcks_alloc_reservoir, pck);
		gf_mx_v(pid->filter->pcks_mx);
#endif
	}
}

static Bool gf_filter_aggregate_packets(GF_FilterPidInst *dst)
{
	u32 size=0, pos=0;
	char *data;
	GF_FilterPacket *final;
	u32 i, count;

	gf_mx_p(dst->mx);
	count=gf_list_count(dst->packets);
	for (i=count; i>0; i--) {
		GF_FilterPacketInstance *pck = gf_list_get(dst->packets, i-1);
		//if packet has end flag set, stop aggregating - skip first packet in test since this is
		//the end of the block we try to reaggregate
		if ((i<count) && pck->pck->data_block_end) break;
		size+=pck->pck->data_length;
		pos++;
		if (pck->pck->data_block_start) {
			//a single start packet was found, set end flag
			if (pos==1) pck->pck->data_block_end = GF_TRUE;
			break;
		}
	}
	//no packet to reaggregate
	if (pos<=1) {
		gf_mx_v(dst->mx);
		return GF_FALSE;
	}
	if (!i) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Cannot find start of block for packet in filter %s, aggregating anyway\n", dst->pid->filter->freg->name));
	} else {
		i--;
	}
	final = gf_filter_pck_new_alloc(dst->pid, size, &data);
	pos=0;
	while (i<count) {
		GF_FilterPacketInstance *pcki = gf_list_get(dst->packets, i);
		GF_FilterPacket *pck = pcki->pck;
		memcpy(data+pos, pcki->pck->data, pcki->pck->data_length);
		pos+=pcki->pck->data_length;
		gf_filter_pck_copy_properties(pcki->pck, final);

		//destroy pcki
		if (i+1<count) {
			gf_list_rem(dst->packets, i);

			pcki->pck = NULL;
			pcki->pid = NULL;
#ifdef MEDIASESSION_LOCKFREE
			gf_lfq_add(pck->pid->filter->pcks_inst_reservoir, pcki);
#else
			gf_mx_p(pck->pid->filter->pcks_mx);
			gf_list_add(pck->pid->filter->pcks_inst_reservoir, pcki);
			gf_mx_v(pck->pid->filter->pcks_mx);
#endif
		} else {
			pcki->pck = final;
			ref_count_inc(&final->reference_count);
		}
		//unref pck
		ref_count_dec(&pck->reference_count);
		if (!pck->reference_count) {
			gf_filter_packet_destroy(pck);
		}

		count--;
	}
	gf_mx_v(dst->mx);
	return GF_TRUE;
}

GF_Err gf_filter_pck_send(GF_FilterPacket *pck)
{
	u32 i, count;
	GF_FilterPid *pid;
	assert(pck);
	pid = pck->pid;

	if (PCK_IS_INPUT(pck)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to dispatch input packet on output PID in filter %s\n", pck->pid->filter->freg->name));
		return NULL;
	}

	//todo - check amount of packets size/time to return a WOULD_BLOCK

	//todo - reaggregate packet if all destination require full blocks
	if (pck->pid->requires_full_blocks_dispatch) {
	}

	count = gf_list_count(pck->pid->destinations);
	for (i=0; i<count; i++) {
		GF_FilterPidInst *dst = gf_list_get(pck->pid->destinations, i);
		if (dst->filter->freg->process) {
			Bool post_task=GF_FALSE;
			GF_FilterPacketInstance *inst;

#ifdef MEDIASESSION_LOCKFREE
			inst = gf_lfq_pop(pck->pid->filter->pcks_inst_reservoir);
#else
			gf_mx_p(pck->pid->filter->pcks_mx);
			inst = gf_list_pop_back(pck->pid->filter->pcks_inst_reservoir);
			gf_mx_v(pck->pid->filter->pcks_mx);
#endif

			if (!inst) {
				GF_SAFEALLOC(inst, GF_FilterPacketInstance);
				if (!inst) return GF_OUT_OF_MEM;
			}
			inst->pck = pck;
			inst->pid = dst;

			ref_count_inc(&pck->reference_count);


			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Dispatching packet from filter %s to filter %s\n", pid->filter->freg->name, dst->filter->freg->name));

			if (dst->requires_full_data_block) {
				//protect packet from destruction during aggregation
				ref_count_inc(&pck->reference_count);

				//missed end of previous, aggregate all before excluding this packet
				if (pck->data_block_start && !dst->last_block_ended) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Filter %s: Missed end of block signaling but got start of block - reaggregating packet\n", pid->filter->freg->name, dst->filter->freg->name));

					//post process task if we have been reaggregating a packet
					post_task = gf_filter_aggregate_packets(dst);
					dst->last_block_ended = GF_TRUE;
				}

				//insert packet
				gf_mx_p(dst->mx);
				gf_list_add(dst->packets, inst);
				gf_mx_v(dst->mx);

				//block end, aggregat all before and including this packet
				if (pck->data_block_end) {
					//not starting at this packet
					if (!pck->data_block_start && !dst->last_block_ended) {
						gf_filter_aggregate_packets(dst);
					}
					dst->last_block_ended = GF_TRUE;
					post_task = GF_TRUE;
				}
				//new block start or continuation
				else if (pck->data_block_start) {
					dst->last_block_ended = GF_FALSE;
					//block not complete, don't post process task
				}

				//unprotect packet
				ref_count_dec(&pck->reference_count);

			} else {
				gf_mx_p(dst->mx);
				gf_list_add(dst->packets, inst);
				gf_mx_v(dst->mx);
				post_task = GF_TRUE;
			}
			if (post_task)
				gf_fs_post_task(pid->filter->session, gf_filter_pid_process, dst->filter, pid);
		}
	}
	if (!pck->reference_count) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("No PID destination on filter %s for packet - discarding\n", pid->filter->freg->name));
		gf_filter_packet_destroy(pck);
	}
	return GF_OK;
}

GF_FilterPacket *gf_filter_pid_get_packet(GF_FilterPid *pid)
{
	GF_FilterPacket *pck;
	GF_FilterPidInst *pidinst = (GF_FilterPidInst *)pid;
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to fetch a packet on an output PID in filter %s\n", pid->filter->freg->name));
		return NULL;
	}

	gf_mx_p(pidinst->mx);
	pck = (GF_FilterPacket *)gf_list_get(pidinst->packets, 0);
	gf_mx_v(pidinst->mx);
	if (!pck) return NULL;
	
	if (pidinst->requires_full_data_block && !pck->pck->data_block_end)
		return NULL;
	return pck;
}

void gf_filter_pck_drop(GF_FilterPacket *pck)
{
	s32 res;
	GF_FilterPacketInstance *pcki = (GF_FilterPacketInstance *)pck;

	if (PCK_IS_OUTPUT(pck)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to discard a packet on an output PID in filter %s\n", pck->pid->filter->freg->name));
		return;
	}
	//get true packet pointer
	pck = pck->pck;

	//remove pck instance
	gf_mx_p(pcki->pid->mx);
	res = gf_list_del_item(pcki->pid->packets, pcki);
	gf_mx_v(pcki->pid->mx);

	if (res<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to discard a packet from another pid in filter %s\n", pck->pid->filter->freg->name));
		return;
	}
	//destroy pcki
	pcki->pck = NULL;
	pcki->pid = NULL;

#ifdef MEDIASESSION_LOCKFREE
	gf_lfq_add(pck->pid->filter->pcks_inst_reservoir, pcki);
#else
	gf_mx_p(pck->pid->filter->pcks_mx);
	gf_list_add(pck->pid->filter->pcks_inst_reservoir, pcki);
	gf_mx_v(pck->pid->filter->pcks_mx);
#endif

	//unref pck
	ref_count_dec(&pck->reference_count);
	if (!pck->reference_count) {
		gf_filter_packet_destroy(pck);
	}
}

void gf_filter_pck_ref(GF_FilterPacket *pck)
{
	assert(pck);
	pck=pck->pck;
	ref_count_inc(&pck->reference_count);
}

void gf_filter_pck_unref(GF_FilterPacket *pck)
{
	assert(pck);
	pck=pck->pck;
	ref_count_dec(&pck->reference_count);
	if (!pck->reference_count) {
		gf_filter_packet_destroy(pck);
	}
}

const char *gf_filter_pck_get_data(GF_FilterPacket *pck, u32 *size)
{
	assert(pck);
	assert(size);
	//get true packet pointer
	pck=pck->pck;
	*size = pck->data_length;
	return (const char *)pck->data;
}

static GF_Err gf_filter_pck_set_property_full(GF_FilterPacket *pck, u32 prop_4cc, const char *prop_name, char *dyn_name, const GF_PropertyValue *value)
{
	u32 hash;
	assert(pck);
	assert(pck->pid);
	if (PCK_IS_INPUT(pck)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to set property on an input packet in filter %s\n", pck->pid->filter->freg->name));
		return GF_BAD_PARAM;
	}
	//get true packet pointer
	pck=pck->pck;
	hash = hash_djb2(prop_4cc, prop_name);

	if (!pck->props) {
		pck->props = gf_property_map_new(pck->pid->filter);
	} else {
		gf_property_map_remove_property(pck->props, hash, prop_4cc, prop_name);
	}
	return gf_property_map_insert_property(pck->props, hash, prop_4cc, prop_name, dyn_name, value);
}

GF_Err gf_filter_pck_set_property(GF_FilterPacket *pck, u32 prop_4cc, const GF_PropertyValue *value)
{
	return gf_filter_pck_set_property_full(pck, prop_4cc, NULL, NULL, value);
}

GF_Err gf_filter_pck_set_property_str(GF_FilterPacket *pck, const char *name, const GF_PropertyValue *value)
{
	return gf_filter_pck_set_property_full(pck, 0, name, NULL, value);
}

GF_Err gf_filter_pck_set_property_dyn(GF_FilterPacket *pck, char *name, const GF_PropertyValue *value)
{
	return gf_filter_pck_set_property_full(pck, 0, NULL, name, value);
}

GF_Err gf_filter_pck_copy_properties(GF_FilterPacket *pck_src, GF_FilterPacket *pck_dst)
{
	u32 i, count, idx;

	if (PCK_IS_INPUT(pck_dst)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to set property on an input packet in filter %s\n", pck_dst->pid->filter->freg->name));
		return GF_BAD_PARAM;
	}
	//we allow copying over properties from dest packets to dest packets

	//get true packet pointer
	pck_src=pck_src->pck;
	pck_dst=pck_dst->pck;

	if (!pck_src->props) {
		if (pck_dst->props) {
			gf_property_map_del(pck_dst->props);
			pck_dst->props=NULL;
		}
		return GF_OK;
	}

	if (!pck_dst->props) {
		pck_dst->props = gf_property_map_new(pck_dst->pid->filter);

		if (!pck_dst->props) return GF_OUT_OF_MEM;
	}
	for (idx=0; idx<HASH_TABLE_SIZE; idx++) {
		if (pck_src->props->hash_table[idx]) {
			count = gf_list_count(pck_src->props->hash_table[idx] );
			if (!pck_dst->props->hash_table[idx]) {
				pck_dst->props->hash_table[idx] = gf_property_map_get_list(pck_dst->props);
				if (!pck_dst->props->hash_table[idx]) return GF_OUT_OF_MEM;
			}

			for (i=0; i<count; i++) {
				GF_PropertyEntry *prop = gf_list_get(pck_src->props->hash_table[idx], i);
				assert(prop->reference_count);
				ref_count_inc(&prop->reference_count);
				gf_list_add(pck_dst->props->hash_table[idx], prop);
			}
		}
	}
	return GF_OK;
}

const GF_PropertyValue *gf_filter_pck_get_property(GF_FilterPacket *pck, u32 prop_4cc, const char *prop_name)
{
	//get true packet pointer
	pck = pck->pck;
	if (!pck->props) return NULL;
	return gf_property_map_get_property(pck->props, prop_4cc, prop_name);
}

GF_Err gf_filter_pck_set_framing(GF_FilterPacket *pck, Bool is_start, Bool is_end)
{
	if (PCK_IS_INPUT(pck)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to set framing info on an input packet in filter %s\n", pck->pid->filter->freg->name));
		return GF_BAD_PARAM;
	}
	//get true packet pointer
	pck=pck->pck;

	pck->data_block_start = is_start;
	pck->data_block_end = is_end;
	return GF_OK;
}

GF_Err gf_filter_pck_get_framing(GF_FilterPacket *pck, Bool *is_start, Bool *is_end)
{
	//get true packet pointer
	pck=pck->pck;

	if (is_start) *is_start = pck->data_block_start;
	if (is_end) *is_end = pck->data_block_end;
	return GF_OK;
}


