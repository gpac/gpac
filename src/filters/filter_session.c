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

#include "filter_session.h"


static GFINLINE void gf_fs_sema_io(GF_FilterSession *fsess, Bool notify)
{
	if (fsess->semaphore) {
		if (notify) {
			ref_count_inc(&fsess->sema_count);
			if ( ! gf_sema_notify(fsess->semaphore, 1)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Cannot notify scheduler of new task, semaphore failure\n"));
			}
		} else {
			if (gf_sema_wait(fsess->semaphore)) {
				assert(fsess->sema_count);
				ref_count_dec(&fsess->sema_count);
			}
		}
	}
}

static void add_filter_reg(GF_FilterSession *fsess, const GF_FilterRegister *freg)
{
	if (!freg->name) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter missing name - ignoring\n"));
		return;
	}
	if (!freg->description) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter %s missing description - ignoring\n", freg->name));
		return;
	}
	if (!freg->process) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter %s missing process function - ignoring\n", freg->name));
		return;
	}
	gf_list_add(fsess->registry, (void *) freg);
}

GF_EXPORT
GF_FilterSession *gf_fs_new(u32 nb_threads, GF_FilterSchedulerType sched_type)
{
	u32 i;
	GF_FilterSession *fsess;

	GF_SAFEALLOC(fsess, GF_FilterSession);
	if (!fsess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to alloc media session\n"));
		return NULL;
	}
	fsess->filters = gf_list_new();
	fsess->main_th.fsess = fsess;

	//regardless of scheduler type, we don't use lock on the main task list
	fsess->tasks = gf_fq_new(NULL);
	fsess->tasks_reservoir = gf_fq_new(NULL);

	if (nb_threads || (sched_type==GF_FS_SCHEDULER_FORCE_LOCK) ) {
		fsess->semaphore = gf_sema_new(GF_UINT_MAX, 0);

		//force testing of mutex queues
		fsess->use_locks = GF_TRUE;
	}

	if (!fsess->semaphore)
		nb_threads=0;

	if (nb_threads) {
		fsess->threads = gf_list_new();
		if (!fsess->threads) {
			gf_sema_del(fsess->semaphore);
			fsess->semaphore=NULL;
			nb_threads=0;
		}
		fsess->use_locks = (sched_type==GF_FS_SCHEDULER_LOCK) ? GF_TRUE : GF_FALSE;
	}

	if (!fsess->filters || !fsess->tasks || !fsess->tasks_reservoir) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to alloc media session\n"));
		fsess->done=1;
		gf_fs_del(fsess);
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
		sess_thread->fsess = fsess;
		gf_list_add(fsess->threads, sess_thread);
	}

	fsess->registry = gf_list_new();

	add_filter_reg(fsess, ut_filter_register() );

	fsess->done=GF_TRUE;
	return fsess;
}

GF_EXPORT
void gf_fs_del(GF_FilterSession *fsess)
{
	assert(fsess);

	//temporary until we don't introduce fsess_stop
	assert(fsess->done);
	if (fsess->filters) {
		u32 i, count=gf_list_count(fsess->filters);
		//first pass: disconnect all filters, since some may have references to property maps or packets 
		for (i=0; i<count; i++) {
			GF_Filter *filter = gf_list_get(fsess->filters, i);
			if (filter->freg->finalize) {
				filter->freg->finalize(filter);
			}
		}

		while (gf_list_count(fsess->filters)) {
			GF_Filter *filter = gf_list_pop_back(fsess->filters);
			gf_filter_del(filter);
		}
		gf_list_del(fsess->filters);
	}

	if (fsess->registry) {
		while (gf_list_count(fsess->registry)) {
			/*GF_FilterRegister *freg = */gf_list_pop_back(fsess->registry);
			//gf_filter_del(freg);
		}
		gf_list_del(fsess->registry);
	}

	if (fsess->tasks)
		gf_fq_del(fsess->tasks, gf_void_del);

	if (fsess->tasks_reservoir)
		gf_fq_del(fsess->tasks_reservoir, gf_void_del);

	if (fsess->threads) {
		while (gf_list_count(fsess->threads)) {
			GF_SessionThread *sess_th = gf_list_pop_back(fsess->threads);
			gf_th_del(sess_th->th);
			gf_free(sess_th);
		}
		gf_list_del(fsess->threads);
	}

	if (fsess->semaphore)
		gf_sema_del(fsess->semaphore);

	gf_free(fsess);
}

GF_EXPORT
u32 gf_fs_filters_registry_count(GF_FilterSession *fsess)
{
	return fsess ? gf_list_count(fsess->registry) : 0;
}

GF_EXPORT
const GF_FilterRegister * gf_fs_get_filter_registry(GF_FilterSession *fsess, u32 idx)
{
	return gf_list_get(fsess->registry, idx);
}


void gf_fs_post_task(GF_FilterSession *fsess, task_callback task_fun, GF_Filter *filter, GF_FilterPid *pid, const char *log_name, void *udta)
{
	GF_FSTask *task;
	assert(fsess);
	assert(filter);
	assert(task_fun);

	task = gf_fq_pop(fsess->tasks_reservoir);

	if (!task) {
		GF_SAFEALLOC(task, GF_FSTask);
		if (!task) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("No more memory to post new task\n"));
			return;
		}
	}
	task->filter = filter;
	task->pid = pid;
	task->run_task = task_fun;
	task->log_name = log_name;
	task->udta = udta;

	if (filter) {
	
		gf_mx_p(filter->tasks_mx);
		if (! filter->scheduled_for_next_task && (gf_fq_count(filter->tasks) == 0)) {
			task->notified = GF_TRUE;
		}
		gf_fq_add(filter->tasks, task);
		gf_mx_v(filter->tasks_mx);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Posted task Filter %s::%s (%d pending)\n", filter->name, task->log_name, fsess->tasks_pending));
	} else {
		task->notified = GF_TRUE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Posted filter-less task %s (%d pending)\n", task->log_name, fsess->tasks_pending));
	}


	if (task->notified) {
		//only notify/count tasks posted on the main task lists, the other ones don't use sema_wait
		ref_count_inc(&fsess->tasks_pending);
		gf_fq_add(fsess->tasks, task);

		gf_fs_sema_io(fsess, GF_TRUE);

	}
}

GF_EXPORT
GF_Filter *gf_fs_load_filter(GF_FilterSession *fsess, const char *name)
{
	GF_Filter *filter = NULL;
	const char *args=NULL;
	u32 i, len, count = gf_list_count(fsess->registry);
	char *sep;

	assert(fsess);
	assert(name);
	sep = strchr(name, ':');
	if (sep) {
		args = sep+1;
		len = sep - name;
	} else len = strlen(name);

	for (i=0;i<count;i++) {
		const GF_FilterRegister *f_reg = gf_list_get(fsess->registry, i);
		if (!strncmp(f_reg->name, name, len)) {
			filter = gf_filter_new(fsess, f_reg, args);
			return filter;
		}
	}
	return NULL;
}

u32 gf_fs_thread_proc(GF_SessionThread *sess_thread)
{
	GF_FilterSession *fsess = sess_thread->fsess;
	u32 i, count = fsess->threads ? gf_list_count(fsess->threads) : 0;
	u32 thid =  1 + gf_list_find(fsess->threads, sess_thread);
	u64 enter_time = gf_sys_clock_high_res();
	GF_Filter *current_filter = NULL;
	while (1) {
		Bool notified;
		Bool requeue = GF_FALSE;
		u64 active_start;
		u32 pending_packets=0;
		GF_FSTask *task=NULL;

		if (current_filter==NULL) {
			//wait for something to be done
			gf_fs_sema_io(fsess, GF_FALSE);
		}

		if (fsess->done) break;

		active_start = gf_sys_clock_high_res();

		if (current_filter==NULL) {
			task = gf_fq_pop(fsess->tasks);
		} else {
			//keep task in filter list task until done
			task = gf_fq_head(current_filter->tasks);
		}

		if (!task) {
			if (!fsess->tasks_pending && fsess->main_th.has_seen_eot) {
				//check all threads
				Bool all_done = GF_TRUE;
				for (i=0; i<count; i++) {
					GF_SessionThread *st = gf_list_get(fsess->threads, i);
					if (!st->has_seen_eot) {
						all_done = GF_FALSE;
						break;
					}
				}
				if (all_done)
					break;
			}
			if (current_filter) current_filter->scheduled_for_next_task = GF_FALSE;
			current_filter = NULL;
			sess_thread->active_time += gf_sys_clock_high_res() - active_start;

			//no pending tasks and first time main task queue is empty, flush to detect if we
			//are indeed done
			if (!fsess->tasks_pending && !sess_thread->has_seen_eot && (gf_fq_count(fsess->tasks)==0)) {
				//maybe last task, force a notify to check if we are truly done
				sess_thread->has_seen_eot = GF_TRUE;
				gf_fs_sema_io(fsess, GF_TRUE);
			}
			if (fsess->tasks_pending) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Thread %d: no task available but still %d pending tasks, renotifying main semaphore\n", thid, fsess->tasks_pending));
				gf_fs_sema_io(fsess, GF_TRUE);
			}

			continue;
		}
		current_filter = task->filter;
		if (current_filter) {
			current_filter->scheduled_for_next_task = GF_TRUE;
			assert(!current_filter->in_process);
			current_filter->in_process=GF_TRUE;
			pending_packets = current_filter->pending_packets;
		}

		sess_thread->nb_tasks++;
		sess_thread->has_seen_eot = GF_FALSE;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Thread %d task#%d executing Filter %s::%s (%d tasks pending)\n", thid, sess_thread->nb_tasks, task->filter->name, task->log_name, fsess->tasks_pending));


		assert( task->run_task );
		requeue = task->run_task(task);

		//source task was current filter, pop the filter task list
		if (current_filter) {
			gf_mx_p(current_filter->tasks_mx);

			gf_fq_pop(current_filter->tasks);

			//no more pending tasks for this filter
			if (gf_fq_count(current_filter->tasks) == 0) {
				//no task for filter but pending packets, requeue
				if (task->filter->pending_packets != pending_packets) {
					requeue = GF_TRUE;
					gf_mx_v(current_filter->tasks_mx);
				} else {
					current_filter->scheduled_for_next_task = GF_FALSE;
					assert (gf_fq_count(current_filter->tasks) == 0);

					current_filter->in_process=GF_FALSE;
					gf_mx_v(current_filter->tasks_mx);
					current_filter = NULL;
				}
			} else {
				gf_mx_v(current_filter->tasks_mx);
			}
		}

		notified = task->notified;
		if (requeue) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Re-posted task Filter %s::%s (%d pending)\n", task->filter->name, task->log_name, fsess->tasks_pending));

			if (current_filter) {
				task->notified=GF_FALSE;
				gf_fq_add(current_filter->tasks, task);
				//keep this thread running on the current filter no signaling of semaphore
			} else {
				task->notified=GF_TRUE;
				ref_count_inc(&fsess->tasks_pending);
				gf_fq_add(fsess->tasks, task);

				gf_fs_sema_io(fsess, GF_TRUE);
			}
		} else {
			memset(task, 0, sizeof(GF_FSTask));
			gf_fq_add(fsess->tasks_reservoir, task);
		}

		//decrement task counter
		if (notified) {
			assert(fsess->tasks_pending);
			ref_count_dec(&fsess->tasks_pending);
		}
		if (current_filter) {
			current_filter->in_process=GF_FALSE;
		}

		//not requeuing, no pending tasks and first time main task queue is empty, flush to detect if we
		//are indeed done
		if (!requeue && !fsess->tasks_pending && !sess_thread->has_seen_eot && (gf_fq_count(fsess->tasks)==0)) {
			//maybe last task, force a notify to check if we are truly done
			sess_thread->has_seen_eot = GF_TRUE;
			gf_fs_sema_io(fsess, GF_TRUE);
		}

		sess_thread->active_time += gf_sys_clock_high_res() - active_start;
	}
	sess_thread->run_time = gf_sys_clock_high_res() - enter_time;

	fsess->done = GF_TRUE;
	for (i=0; i < count; i++) {
		gf_fs_sema_io(fsess, GF_TRUE);
		//gf_sema_notify(fsess->semaphore, 1);
	}
	return 0;
}

GF_EXPORT
GF_Err gf_fs_run(GF_FilterSession *fsess)
{
	u32 i, nb_threads;
	assert(fsess);

	fsess->done = GF_FALSE;
	nb_threads = gf_list_count(fsess->threads);
	for (i=0;i<nb_threads; i++) {
		GF_SessionThread *sess_th = gf_list_get(fsess->threads, i);
		gf_th_run(sess_th->th, (gf_thread_run) gf_fs_thread_proc, sess_th);
	}

	gf_fs_thread_proc(&fsess->main_th);

	return GF_EOS;
}

GF_EXPORT
void gf_fs_print_stats(GF_FilterSession *fsess)
{
	u64 run_time=0, active_time=0, nb_tasks=0;
	u32 i, count=gf_list_count(fsess->threads);
	fprintf(stderr, "Session threads %d\n", 1+count);

	fprintf(stderr, "\tThread %d: run_time "LLU" us active_time "LLU" us nb_tasks "LLU" us\n", 1, fsess->main_th.run_time, fsess->main_th.active_time, fsess->main_th.nb_tasks);

	run_time+=fsess->main_th.run_time;
	active_time+=fsess->main_th.active_time;
	nb_tasks+=fsess->main_th.nb_tasks;

	for (i=0; i<count; i++) {
		GF_SessionThread *s = gf_list_get(fsess->threads, i);

		fprintf(stderr, "\tThread %d: run_time "LLU" us active_time "LLU" us nb_tasks "LLU" us\n", i+1, s->run_time, s->active_time, s->nb_tasks);

		run_time+=s->run_time;
		active_time+=s->active_time;
		nb_tasks+=s->nb_tasks;
	}
	fprintf(stderr, "\nTotal: run_time "LLU" us active_time "LLU" us nb_tasks "LLU" us\n", run_time, active_time, nb_tasks);
}

