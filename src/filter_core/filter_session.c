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
#include <gpac/network.h>

static GFINLINE void gf_fs_sema_io(GF_FilterSession *fsess, Bool notify, Bool main)
{
	GF_Semaphore *sem = main ? fsess->semaphore_main : fsess->semaphore_other;
	if (sem) {
		if (notify) {
			if ( ! gf_sema_notify(sem, 1)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_SCHEDULER, ("Cannot notify scheduler of new task, semaphore failure\n"));
			}
		} else {
			if (gf_sema_wait(sem)) {
			}
		}
	}
}

void gf_fs_add_filter_registry(GF_FilterSession *fsess, const GF_FilterRegister *freg)
{
	if (!freg) return;

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



static Bool fs_default_event_proc(void *ptr, GF_Event *evt)
{
	if (evt->type==GF_EVENT_QUIT) {
		GF_FilterSession *fsess = (GF_FilterSession *)ptr;
		fsess->run_status = GF_EOS;
	}
	return 0;
}

GF_EXPORT
GF_FilterSession *gf_fs_new(u32 nb_threads, GF_FilterSchedulerType sched_type, GF_User *user, Bool load_meta_filters, Bool disable_blocking)
{
	u32 i;
	GF_FilterSession *fsess, *a_sess;

	if ( ! gf_props_4cc_check_props())
		return NULL;

	GF_SAFEALLOC(fsess, GF_FilterSession);
	if (!fsess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to alloc media session\n"));
		return NULL;
	}
	fsess->filters = gf_list_new();
	fsess->main_th.fsess = fsess;

	if (sched_type==GF_FS_SCHEDULER_DIRECT) {
		fsess->direct_mode = GF_TRUE;
		nb_threads=0;
	}
	if (nb_threads && (sched_type != GF_FS_SCHEDULER_LOCK_FREE_X)) {
		fsess->tasks_mx = gf_mx_new("TasksList");
	}

	//regardless of scheduler type, we don't use lock on the main task list
	fsess->tasks = gf_fq_new(fsess->tasks_mx);

	if (nb_threads>0) {
		fsess->main_thread_tasks = gf_fq_new(fsess->tasks_mx);
		fsess->filters_mx = gf_mx_new("Filters");
	} else {
		//otherwise use the same as the global task list
		fsess->main_thread_tasks = fsess->tasks;
	}

	fsess->tasks_reservoir = gf_fq_new(fsess->tasks_mx);

	if (nb_threads || (sched_type==GF_FS_SCHEDULER_LOCK_FORCE) ) {
		fsess->semaphore_main = fsess->semaphore_other = gf_sema_new(GF_UINT_MAX, 0);
		if (nb_threads>0)
			fsess->semaphore_other = gf_sema_new(GF_UINT_MAX, 0);

		//force testing of mutex queues
		//fsess->use_locks = GF_TRUE;
	}
	fsess->user = user;

	//setup our basic callbacks
	if (!user) {
		fsess->static_user.EventProc = fs_default_event_proc;
		fsess->static_user.opaque = fsess;
		fsess->user = &fsess->static_user;
	}

	if (fsess->user->init_flags & GF_TERM_NO_COMPOSITOR_THREAD)
		fsess->no_main_thread = GF_TRUE;

	if (fsess->user->init_flags & GF_TERM_NO_REGULATION)
		fsess->no_regulation = GF_TRUE;


	if (!fsess->semaphore_main)
		nb_threads=0;

	if (nb_threads) {
		fsess->threads = gf_list_new();
		if (!fsess->threads) {
			gf_sema_del(fsess->semaphore_main);
			fsess->semaphore_main=NULL;
			gf_sema_del(fsess->semaphore_other);
			fsess->semaphore_other=NULL;
			nb_threads=0;
		}
		fsess->use_locks = (sched_type==GF_FS_SCHEDULER_LOCK) ? GF_TRUE : GF_FALSE;
	} else {
#ifdef GPAC_MEMORY_TRACKING
		extern int gf_mem_track_enabled;
		fsess->check_allocs = gf_mem_track_enabled;
#endif

	}

	if (fsess->use_locks)
		fsess->props_mx = gf_mx_new("FilterSessionProps");

	fsess->prop_maps_list_reservoir = gf_fq_new(fsess->props_mx);
	fsess->prop_maps_reservoir = gf_fq_new(fsess->props_mx);
	fsess->prop_maps_entry_reservoir = gf_fq_new(fsess->props_mx);
	fsess->prop_maps_entry_data_alloc_reservoir = gf_fq_new(fsess->props_mx);



	if (!fsess->filters || !fsess->tasks || !fsess->tasks_reservoir) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to alloc media session\n"));
		fsess->run_status = GF_OUT_OF_MEM;
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

	a_sess = load_meta_filters ? fsess : NULL;
	gf_fs_reg_all(fsess, a_sess);

	//todo - find a way to handle events without mutex ...
	fsess->evt_mx = gf_mx_new("Event mutex");

	fsess->disable_blocking = disable_blocking;
	fsess->run_status = GF_EOS;
	fsess->nb_threads_stopped = 1+nb_threads;
	fsess->default_pid_buffer_max_us = 1000;
	fsess->decoder_pid_buffer_max_us = 1000000;
	return fsess;
}

void gf_fs_remove_filter_registry(GF_FilterSession *session, GF_FilterRegister *freg)
{
	gf_list_del_item(session->registry, freg);
}

void gf_propalloc_del(void *it)
{
	GF_PropertyEntry *pe = (GF_PropertyEntry *)it;
	if (pe->prop.value.data.ptr) gf_free(pe->prop.value.data.ptr);
	gf_free(pe);
}

GF_EXPORT
void gf_fs_del(GF_FilterSession *fsess)
{
	assert(fsess);

	gf_fs_stop(fsess);

	//temporary until we don't introduce fsess_stop
	assert(fsess->run_status != GF_OK);
	if (fsess->filters) {
		u32 i, count=gf_list_count(fsess->filters);
		//first pass: disconnect all filters, since some may have references to property maps or packets 
		for (i=0; i<count; i++) {
			GF_Filter *filter = gf_list_get(fsess->filters, i);
			filter->process_th_id = 0;

			if (filter->postponed_packets) {
				while (gf_list_count(filter->postponed_packets)) {
					GF_FilterPacket *pck = gf_list_pop_front(filter->postponed_packets);
					gf_filter_packet_destroy(pck);
				}
				gf_list_del(filter->postponed_packets);
				filter->postponed_packets = NULL;
			}

			if (filter->freg->finalize) {
				filter->finalized = GF_TRUE;
				FSESS_CHECK_THREAD(filter)
				filter->freg->finalize(filter);
			}
		}

		while (gf_list_count(fsess->filters)) {
			GF_Filter *filter = gf_list_pop_back(fsess->filters);
			gf_filter_del(filter);
		}
		gf_list_del(fsess->filters);
	}

	if (fsess->download_manager) gf_dm_del(fsess->download_manager);

	if (fsess->registry) {
		while (gf_list_count(fsess->registry)) {
			GF_FilterRegister *freg = gf_list_pop_back(fsess->registry);
			if (freg->registry_free) freg->registry_free(fsess, freg);
		}
		gf_list_del(fsess->registry);
	}

	if (fsess->tasks)
		gf_fq_del(fsess->tasks, gf_void_del);

	if (fsess->tasks_reservoir)
		gf_fq_del(fsess->tasks_reservoir, gf_void_del);

	if (fsess->threads) {
		if (fsess->main_thread_tasks)
			gf_fq_del(fsess->main_thread_tasks, gf_void_del);

		while (gf_list_count(fsess->threads)) {
			GF_SessionThread *sess_th = gf_list_pop_back(fsess->threads);
			gf_th_del(sess_th->th);
			gf_free(sess_th);
		}
		gf_list_del(fsess->threads);
	}

	gf_fq_del(fsess->prop_maps_reservoir, gf_void_del);
	gf_fq_del(fsess->prop_maps_list_reservoir, (gf_destruct_fun) gf_list_del);
	gf_fq_del(fsess->prop_maps_entry_reservoir, gf_void_del);
	gf_fq_del(fsess->prop_maps_entry_data_alloc_reservoir, gf_propalloc_del);

	if (fsess->props_mx)
		gf_mx_del(fsess->props_mx);

	if (fsess->semaphore_other && (fsess->semaphore_other != fsess->semaphore_main) )
		gf_sema_del(fsess->semaphore_other);

	if (fsess->semaphore_main)
		gf_sema_del(fsess->semaphore_main);

	if (fsess->tasks_mx)
		gf_mx_del(fsess->tasks_mx);

	if (fsess->filters_mx)
		gf_mx_del(fsess->filters_mx);

	if (fsess->static_user.modules) gf_modules_del(fsess->static_user.modules);
	if (fsess->static_user.config) gf_cfg_del(fsess->static_user.config);

	if (fsess->evt_mx) gf_mx_del(fsess->evt_mx);
	if (fsess->event_listeners) gf_list_del(fsess->event_listeners);
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

static void check_task_list(GF_FilterQueue *fq, GF_FSTask *task)
{
	u32 k, c = gf_fq_count(fq);
	for (k=0; k<c; k++) {
		GF_FSTask *a = gf_fq_get(fq, k);
		assert(a != task);
	}
}


void gf_fs_post_task(GF_FilterSession *fsess, gf_fs_task_callback task_fun, GF_Filter *filter, GF_FilterPid *pid, const char *log_name, void *udta)
{
	GF_FSTask *task;

	assert(fsess);
	assert(task_fun);

	//only flatten calls if in main thread (we still have some broken filters using threading
	//that could trigger tasks
	if (fsess->direct_mode && fsess->task_in_process && gf_th_id()==fsess->main_th.th_id) {
		GF_FSTask atask;
		u64 task_time = gf_sys_clock_high_res();
		memset(&atask, 0, sizeof(GF_FSTask));
		atask.filter = filter;
		atask.pid = pid;
		atask.run_task = task_fun;
		atask.log_name = log_name;
		atask.udta = udta;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread 0 task#%d %p executing Filter %s::%s (%d tasks pending)\n", fsess->main_th.nb_tasks, &atask, filter ? filter->name : "", log_name, fsess->tasks_pending));

		task_fun(&atask);
		filter = atask.filter;
		if (filter) {
			filter->time_process += gf_sys_clock_high_res() - task_time;
			filter->nb_tasks_done++;
		}
		if (!atask.requeue_request)
			return;
		//asked to requeue the task, post it
	}
	task = gf_fq_pop(fsess->tasks_reservoir);

	if (!task) {
		GF_SAFEALLOC(task, GF_FSTask);
		if (!task) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCHEDULER, ("No more memory to post new task\n"));
			return;
		}
	}
	task->filter = filter;
	task->pid = pid;
	task->run_task = task_fun;
	task->log_name = log_name;
	task->udta = udta;
	task->in_main_task_list_only = GF_FALSE;
	task->notified = GF_FALSE;

	if (filter) {
	
		gf_mx_p(filter->tasks_mx);
		if (! filter->scheduled_for_next_task && (gf_fq_count(filter->tasks) == 0)) {
			task->notified = GF_TRUE;
		}
		gf_fq_add(filter->tasks, task);
		gf_mx_v(filter->tasks_mx);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Posted task %p Filter %s::%s (%d pending) on %s\n", task, filter->name, task->log_name, fsess->tasks_pending, task->notified ? "main task list" : "filter task list"));
	} else {
		task->notified = GF_TRUE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Posted filter-less task %s (%d pending)\n", task->log_name, fsess->tasks_pending));
	}


	if (task->notified) {
		check_task_list(fsess->main_thread_tasks, task);

		//only notify/count tasks posted on the main task lists, the other ones don't use sema_wait
		safe_int_inc(&fsess->tasks_pending);
		if (filter && filter->freg->requires_main_thread) {
			gf_fq_add(fsess->main_thread_tasks, task);
			gf_fs_sema_io(fsess, GF_TRUE, GF_TRUE);
		} else {
			gf_fq_add(fsess->tasks, task);
			gf_fs_sema_io(fsess, GF_TRUE, GF_FALSE);
		}

	}
}

GF_EXPORT
void *gf_fs_task_get_udta(GF_FSTask *task)
{
	return task->udta;
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
			filter = gf_filter_new(fsess, f_reg, args, GF_FILTER_ARG_LOCAL, NULL);
			if (filter) filter->orig_args = args;
			return filter;
		}
	}
	return NULL;
}

static u32 gf_fs_thread_proc(GF_SessionThread *sess_thread)
{
	GF_FilterSession *fsess = sess_thread->fsess;
	u32 i, th_count = fsess->threads ? gf_list_count(fsess->threads) : 0;
	u32 thid =  1 + gf_list_find(fsess->threads, sess_thread);
	u64 enter_time = gf_sys_clock_high_res();
	//main thread not using this thread proc, don't wait for notifications
	Bool do_use_sema = (!thid && fsess->no_main_thread) ? GF_FALSE : GF_TRUE;
	Bool use_main_sema = thid ? GF_FALSE : GF_TRUE;
	GF_Filter *current_filter = NULL;
	sess_thread->th_id = gf_th_id();

	while (1) {
		Bool notified;
		Bool requeue = GF_FALSE;
		Bool task_from_filter = GF_FALSE;
		u64 active_start, task_time;
		u32 pending_packets=0;
		GF_FSTask *task=NULL;
		GF_Filter *prev_current_filter = NULL;

		if (do_use_sema && (current_filter==NULL)) {
			//wait for something to be done
			gf_fs_sema_io(fsess, GF_FALSE, use_main_sema);
		}

		active_start = gf_sys_clock_high_res();

		if (current_filter==NULL) {
			//main thread
			if (thid==0) {
				task = gf_fq_pop(fsess->main_thread_tasks);
				if (!task) task = gf_fq_pop(fsess->tasks);
			} else {
				task = gf_fq_pop(fsess->tasks);
			}
			if (task) {
				assert( task->run_task );
				assert( task->notified );
			}
		} else {
			//keep task in filter list task until done
			task = gf_fq_head(current_filter->tasks);
			if (task) {
				assert( task->run_task );
				assert( ! task->in_main_task_list_only );
				assert( ! task->notified );
			}
			task_from_filter = GF_TRUE;
		}

		if (!task) {
			if (!fsess->tasks_pending && fsess->main_th.has_seen_eot) {
				//check all threads
				Bool all_done = GF_TRUE;
				//no more task and EOS signal
				if (fsess->run_status != GF_OK)
					break;

				for (i=0; i<th_count; i++) {
					GF_SessionThread *st = gf_list_get(fsess->threads, i);
					if (!st->has_seen_eot) {
						all_done = GF_FALSE;
						break;
					}
				}
				if (all_done)
					break;
			}
			if (current_filter) {
				current_filter->scheduled_for_next_task = GF_FALSE;
				current_filter->process_th_id = 0;
				assert(current_filter->in_process);
				current_filter->in_process = GF_FALSE;
			}
			current_filter = NULL;
			sess_thread->active_time += gf_sys_clock_high_res() - active_start;

			//no main thread, return
			if (!thid && fsess->no_main_thread) return 0;

			//no pending tasks and first time main task queue is empty, flush to detect if we
			//are indeed done
			if (!fsess->tasks_pending && !sess_thread->has_seen_eot && !gf_fq_count(fsess->tasks)) {

				if (thid || !gf_fq_count(fsess->main_thread_tasks)) {
					if (do_use_sema) {
						//maybe last task, force a notify to check if we are truly done
						sess_thread->has_seen_eot = GF_TRUE;
						gf_fs_sema_io(fsess, GF_TRUE, use_main_sema);
					}
				}
			}

			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %d: no task available\n", thid));

			gf_sleep(0);
			continue;
		}
		if (current_filter) {
			assert(current_filter==task->filter);
		}
		current_filter = task->filter;


		//this is a crude way of scheduling the next task, we should
		//1- have a way to make sure we will not repost after a time-consuming task
		//2- have a wait to wait for the given amount of time rather than just do a sema_wait/notify in loop
		if (task->schedule_next_time) {
			s64 now = gf_sys_clock_high_res();
			s64 diff = task->schedule_next_time;
			diff -= now;
			diff /= 1000;


			if (diff > 4) {
				GF_FSTask *next;

				if (diff>50) diff = 50;

				//no filter, just reschedule the task
				if (!current_filter) {
					next = gf_fq_head(fsess->tasks);
					gf_fq_add(fsess->tasks, task);
					if (next) {
						s64 ndiff;
						if (next->schedule_next_time <= now) {
							continue;
						}
						if (!fsess->no_regulation) {
							ndiff = next->schedule_next_time - now;
							if (ndiff<diff) diff = ndiff;
							gf_sleep(diff);
						}
					}
					if (do_use_sema) {
						gf_fs_sema_io(fsess, GF_TRUE, use_main_sema);
					}

					continue;
				}

				if (!task->filter->finalized) {
					//task was in the filter queue, drop it
					if (!task->in_main_task_list_only) {
						next = gf_fq_head(current_filter->tasks);
						assert(next == task);

						gf_fq_pop(current_filter->tasks);
						task->in_main_task_list_only = GF_TRUE;
					}

					check_task_list(current_filter->tasks, task);
					check_task_list(fsess->main_thread_tasks, task);

					//next in filter should be handled before this task, move task at the end of the filter task
					next = gf_fq_head(current_filter->tasks);
					if (next && next->schedule_next_time < task->schedule_next_time) {
						if (task->notified) {
							assert(fsess->tasks_pending);
							safe_int_dec(&fsess->tasks_pending);
							task->notified = GF_FALSE;
						}
						task->in_main_task_list_only = GF_FALSE;
						GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %d: task %s:%s reposted to filter task until task exec time is reached (%d us)\n", thid, current_filter->name, task->log_name, (s32) (task->schedule_next_time - next->schedule_next_time) ));
						gf_fq_add(current_filter->tasks, task);
						continue;
					}
					assert(task->in_main_task_list_only);

					//move task to main list
					if (!task->notified) {
						task->notified = GF_TRUE;
						safe_int_inc(&fsess->tasks_pending);
					}
					task->in_main_task_list_only = GF_TRUE;

					sess_thread->active_time += gf_sys_clock_high_res() - active_start;

					if (! fsess->no_regulation) {
						GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %d: task %s:%s postponed for %d ms\n", thid, current_filter->name, task->log_name, (s32) diff));

						if (th_count==0) {
							if ( gf_fq_count(fsess->tasks) )
								diff=5;
						}

						gf_sleep(diff);
						active_start = gf_sys_clock_high_res();
					}
					if (task->schedule_next_time >  gf_sys_clock_high_res() + 2000) {
						current_filter->in_process = GF_FALSE;
						if (current_filter->freg->requires_main_thread) {
							gf_fq_add(fsess->main_thread_tasks, task);
						} else {
							gf_fq_add(fsess->tasks, task);
						}
						current_filter = NULL;
						continue;
					}
				}
			}
		}

		if (current_filter) {
			current_filter->scheduled_for_next_task = GF_TRUE;
			assert(!current_filter->in_process);
			current_filter->in_process = GF_TRUE;
			current_filter->process_th_id = gf_th_id();
			pending_packets = current_filter->pending_packets;
		}

		sess_thread->nb_tasks++;
		sess_thread->has_seen_eot = GF_FALSE;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %d task#%d %p executing Filter %s::%s (%d tasks pending)\n", thid, sess_thread->nb_tasks, task, task->filter ? task->filter->name : "(none)", task->log_name, fsess->tasks_pending));

		fsess->task_in_process = GF_TRUE;
		assert( task->run_task );
		task_time = gf_sys_clock_high_res();

		task->requeue_request = GF_FALSE;
		task->run_task(task);
		requeue = task->requeue_request;

		task_time = gf_sys_clock_high_res() - task_time;
		fsess->task_in_process = GF_FALSE;

		//may now be NULL if task was a filter destruction task
		current_filter = task->filter;

		prev_current_filter = task->filter;

		//source task was current filter, pop the filter task list
		if (current_filter) {
			current_filter->nb_tasks_done++;
			current_filter->time_process += task_time;
			gf_mx_p(current_filter->tasks_mx);

			//drop task from filter task list if this was not a requeued task
			if (!task->in_main_task_list_only)
				gf_fq_pop(current_filter->tasks);

			//no more pending tasks for this filter
			if ((gf_fq_count(current_filter->tasks) == 0) || (requeue && current_filter->stream_reset_pending) ) {
//				assert (gf_fq_count(current_filter->tasks) == 0);

				current_filter->in_process = GF_FALSE;
				if (current_filter->stream_reset_pending)
					current_filter->in_process = GF_FALSE;

				if (requeue) {
					current_filter->process_th_id = 0;
				} else {
					//don't reset the flag if not requeued to make sure no other task posted from
					//another thread will post to main sched
					current_filter->scheduled_for_next_task = GF_FALSE;
				}

				gf_mx_v(current_filter->tasks_mx);
				current_filter = NULL;
			} else {
				gf_mx_v(current_filter->tasks_mx);
			}
		}

		notified = task->notified;
		if (requeue) {
			//if requeue on a filter active, use filter queue to avoid another thread grabing the task (we would have concurrent access to the filter)
			if (current_filter) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %d re-posted task Filter %s::%s in filter tasks (%d pending)\n", thid, task->filter->name, task->log_name, fsess->tasks_pending));
				task->notified = GF_FALSE;
				task->in_main_task_list_only = GF_FALSE;
				check_task_list(fsess->main_thread_tasks, task);
				gf_fq_add(current_filter->tasks, task);
				//keep this thread running on the current filter no signaling of semaphore
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %d re-posted task Filter %s::%s in main tasks (%d pending)\n", thid, task->filter->name, task->log_name, fsess->tasks_pending));

				task->notified = GF_TRUE;
				safe_int_inc(&fsess->tasks_pending);

				if (prev_current_filter) check_task_list(prev_current_filter->tasks, task);
				task->in_main_task_list_only = GF_TRUE;
				//main thread
				if (task->filter && task->filter->freg->requires_main_thread) {
					gf_fq_add(fsess->main_thread_tasks, task);
				} else {
					gf_fq_add(fsess->tasks, task);
				}
				if (do_use_sema)
					gf_fs_sema_io(fsess, GF_TRUE, use_main_sema);
			}
		} else {
			check_task_list(fsess->main_thread_tasks, task);
			if (prev_current_filter)
				check_task_list(prev_current_filter->tasks, task);

			{
				u32 k, c2 = gf_list_count(fsess->filters);
				for (k=0; k<c2; k++) {
					GF_Filter *af = gf_list_get(fsess->filters, k);
					check_task_list(af->tasks, task);
				}
			}

			memset(task, 0, sizeof(GF_FSTask));
			gf_fq_add(fsess->tasks_reservoir, task);
		}

		//decrement task counter
		if (notified) {
			assert(fsess->tasks_pending);
			safe_int_dec(&fsess->tasks_pending);
		}
		if (current_filter) {
			current_filter->in_process = GF_FALSE;
		}

		//not requeuing, no pending tasks and first time main task queue is empty, flush to detect if we
		//are indeed done
		if (!current_filter && !fsess->tasks_pending && !sess_thread->has_seen_eot && !gf_fq_count(fsess->tasks)) {
			if (thid || !gf_fq_count(fsess->main_thread_tasks) ) {
				//maybe last task, force a notify to check if we are truly done
				sess_thread->has_seen_eot = GF_TRUE;
				gf_fs_sema_io(fsess, GF_TRUE, use_main_sema);
			}
		}

		sess_thread->active_time += gf_sys_clock_high_res() - active_start;


		//no main thread, return
		if (!thid && fsess->no_main_thread && !current_filter) {
			return 0;
		}
	}
	//no main thread, return
	if (!thid && fsess->no_main_thread) {
		return 0;
	}
	sess_thread->run_time = gf_sys_clock_high_res() - enter_time;

	safe_int_inc(&fsess->nb_threads_stopped);

	if (!fsess->run_status)
		fsess->run_status = GF_EOS;

	for (i=0; i < th_count; i++) {
		gf_fs_sema_io(fsess, GF_TRUE, i ? GF_FALSE : GF_TRUE);
	}
	return 0;
}

GF_User *gf_fs_get_user(GF_FilterSession *fsess)
{
	if (!fsess->user_init) {
		u32 count=0;
		fsess->user_init = GF_TRUE;

		if (!fsess->user->config) {
			assert(fsess->user == &fsess->static_user);

			fsess->static_user.config = gf_cfg_init(NULL, NULL);
			if (fsess->static_user.config)
				fsess->static_user.modules = gf_modules_new(NULL, fsess->static_user.config);
		}
		if (!fsess->user->config) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Error: no config file found - cannot load user object\n" ));
			return NULL;
		}

		if (fsess->user->modules) count = gf_modules_get_count(fsess->user->modules);
		if (!count) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Error: no modules found - cannot load user object\n" ));
			return NULL;
		}
	}
	return fsess->user;
}


GF_EXPORT
GF_Err gf_fs_run(GF_FilterSession *fsess)
{
	u32 i, nb_threads;
	assert(fsess);

	fsess->run_status = GF_OK;
	fsess->main_th.has_seen_eot = GF_FALSE;
	fsess->nb_threads_stopped = 0;

	nb_threads = gf_list_count(fsess->threads);
	for (i=0;i<nb_threads; i++) {
		GF_SessionThread *sess_th = gf_list_get(fsess->threads, i);
		gf_th_run(sess_th->th, (gf_thread_run) gf_fs_thread_proc, sess_th);
	}
	if (fsess->no_main_thread) return GF_OK;

	gf_fs_thread_proc(&fsess->main_th);

	//wait for all threads to be done
	while (nb_threads+1 != fsess->nb_threads_stopped) {
	}

	return fsess->run_status;
}

u32 gf_fs_run_step(GF_FilterSession *fsess)
{
	gf_fs_thread_proc(&fsess->main_th);
	return 0;
}

GF_Err gf_fs_stop(GF_FilterSession *fsess)
{
	u32 i, count = fsess->threads ? gf_list_count(fsess->threads) : 0;

	if (count+1 == fsess->nb_threads_stopped) {
		return GF_OK;
	}

	if (!fsess->run_status)
		fsess->run_status = GF_EOS;

	for (i=0; i < count; i++) {
		gf_fs_sema_io(fsess, GF_TRUE, GF_FALSE);
	}

	//wait for all threads to be done, we might still need flushing the main thread queue
	while (fsess->no_main_thread) {
		gf_fs_thread_proc(&fsess->main_th);
		if (! gf_fq_count(fsess->main_thread_tasks))
			break;
	}
	if (fsess->no_main_thread) {
		safe_int_inc(&fsess->nb_threads_stopped);
		fsess->main_th.has_seen_eot = GF_TRUE;
	}

	while (count+1 != fsess->nb_threads_stopped) {
		for (i=0; i < count; i++) {
			gf_fs_sema_io(fsess, GF_TRUE, GF_FALSE);
		}
		gf_sleep(0);
		//we may have tasks in main task list posted by other threads
		if (fsess->no_main_thread) {
			gf_fs_thread_proc(&fsess->main_th);
			fsess->main_th.has_seen_eot = GF_TRUE;
		}
	}
	return GF_OK;
}

GF_EXPORT
void gf_fs_print_stats(GF_FilterSession *fsess)
{
	u64 run_time=0, active_time=0, nb_tasks=0;
	u32 i, count;

	fprintf(stderr, "\n");
	if (fsess->filters_mx) gf_mx_p(fsess->filters_mx);

	count=gf_list_count(fsess->filters);
	fprintf(stderr, "Filter stats - %d filters\n", count);
	for (i=0; i<count; i++) {
		u32 k, ipids, opids;
		GF_Filter *f = gf_list_get(fsess->filters, i);
		ipids = f->num_input_pids;
		opids = f->num_output_pids;
		fprintf(stderr, "\tFilter %s: %d input pids %d output pids "LLU" tasks "LLU" us process time\n", f->name, ipids, opids, f->nb_tasks_done, f->time_process);

		if (ipids) {
			fprintf(stderr, "\t\t"LLU" packets processed "LLU" bytes processed", f->nb_pck_processed, f->nb_bytes_processed);
			if (f->time_process) {
				fprintf(stderr, " (%g pck/sec %g kbps)", (Double) f->nb_pck_processed*1000000/f->time_process, (Double) f->nb_bytes_processed*8000/f->time_process);
			}
			fprintf(stderr, "\n");
		}
		if (opids) {
			fprintf(stderr, "\t\t"LLU" packets sent "LLU" bytes sent", f->nb_pck_sent, f->nb_bytes_sent);
			if (f->time_process) {
				fprintf(stderr, " (%g pck/sec %g kbps)", (Double) f->nb_pck_sent*1000000/f->time_process, (Double) f->nb_bytes_sent*8000/f->time_process);
			}
			fprintf(stderr, "\n");
		}

		for (k=0; k<ipids; k++) {
			GF_FilterPidInst *pid = gf_list_get(f->input_pids, k);
			fprintf(stderr, "\t\t* input PID %s: %d packets received\n", pid->pid->name, pid->pid->nb_pck_sent);
		}
		for (k=0; k<opids; k++) {
			GF_FilterPid *pid = gf_list_get(f->output_pids, k);
			fprintf(stderr, "\t\t* output PID %s: %d packets sent\n", pid->name, pid->nb_pck_sent);
		}
	}
	if (fsess->filters_mx) gf_mx_v(fsess->filters_mx);

	count=gf_list_count(fsess->threads);
	fprintf(stderr, "Session stats - threads %d\n", 1+count);

	fprintf(stderr, "\tThread %d: run_time "LLU" us active_time "LLU" us nb_tasks "LLU"\n", 1, fsess->main_th.run_time, fsess->main_th.active_time, fsess->main_th.nb_tasks);

	run_time+=fsess->main_th.run_time;
	active_time+=fsess->main_th.active_time;
	nb_tasks+=fsess->main_th.nb_tasks;

	for (i=0; i<count; i++) {
		GF_SessionThread *s = gf_list_get(fsess->threads, i);

		fprintf(stderr, "\tThread %d: run_time "LLU" us active_time "LLU" us nb_tasks "LLU"\n", i+1, s->run_time, s->active_time, s->nb_tasks);

		run_time+=s->run_time;
		active_time+=s->active_time;
		nb_tasks+=s->nb_tasks;
	}
	fprintf(stderr, "\nTotal: run_time "LLU" us active_time "LLU" us nb_tasks "LLU"\n", run_time, active_time, nb_tasks);
}

void gf_fs_send_update(GF_FilterSession *fsess, const char *fid, const char *name, const char *val)
{
	GF_FilterUpdate *upd;
	GF_Filter *filter=NULL;
	u32 i, count;
	Bool removed = GF_FALSE;
	if (!fid || !name) return;

	if (fsess->filters_mx) gf_mx_p(fsess->filters_mx);

	count = gf_list_count(fsess->filters);
	for (i=0; i<count; i++) {
		filter = gf_list_get(fsess->filters, i);
		if (filter->id && !strcmp(filter->id, fid)) {
			break;
		}
	}
	removed = (!filter || filter->removed || filter->finalized) ? GF_TRUE : GF_FALSE;
	if (fsess->filters_mx) gf_mx_v(fsess->filters_mx);

	if (removed) return;

	GF_SAFEALLOC(upd, GF_FilterUpdate);
	upd->name = gf_strdup(name);
	upd->val = gf_strdup(val);
	gf_fs_post_task(fsess, gf_filter_update_arg_task, filter, NULL, "update_arg", upd);
}


GF_Filter *gf_fs_load_source_internal(GF_FilterSession *fsess, const char *url, const char *user_args, const char *parent_url, GF_Err *err, GF_Filter *filter, GF_Filter *dst_filter)
{
	GF_FilterProbeScore score = GF_FPROBE_NOT_SUPPORTED;
	GF_FilterRegister *candidate_freg=NULL;
	const GF_FilterArgs *src_arg=NULL;
	u32 i, count, user_args_len;
	GF_Err e;
	const char *force_freg = NULL;
	char *sURL, *mime_type, *args, *sep;
	char szExt[50];
	memset(szExt, 0, sizeof(szExt));

	if (err) *err = GF_OK;
	
	sURL = NULL;
	if (!url || !strncmp(url, "\\\\", 2) ) {
		return NULL;
	}
	if (filter) {
		sURL = (char *) url;
	} else {
		/*used by GUIs scripts to skip URL concatenation*/
		if (!strncmp(url, "gpac://", 7)) sURL = gf_strdup(url+7);
		/*opera-style localhost URLs*/
		else if (!strncmp(url, "file://localhost", 16)) sURL = gf_strdup(url+16);

		else if (parent_url) sURL = gf_url_concatenate(parent_url, url);

		/*path absolute*/
		if (!sURL) sURL = gf_strdup(url);

		if (gf_url_is_local(sURL))
			gf_url_to_fs_path(sURL);
	}
	sep = strstr(sURL, "://");
	if (sep) {
		sep = strchr(sep+3, '/');
		if (sep) sep = strstr(sep+1, ":gpac:");
	} else {
		sep = strchr(sURL, ':');
		if (sep && !strnicmp(sep, ":\\", 2)) sep = strstr(sep+2, ":gpac:");
	}
	if (sep) {
		sep[0] = 0;
		force_freg = strstr(sep+1, "filter=");
		if (force_freg)
			force_freg += 7;
	}

	//check all our registered filters
	count = gf_list_count(fsess->registry);
	for (i=0; i<count; i++) {
		u32 j;
		GF_FilterProbeScore s;
		GF_FilterRegister *freg = gf_list_get(fsess->registry, i);
		if (! freg->probe_url) continue;
		if (force_freg && strncmp(force_freg, freg->name, strlen(freg->name))) continue;
		if (! freg->args) continue;
		if (filter && (gf_list_find(filter->blacklisted, freg) >=0)) continue;

		j=0;
		while (freg->args) {
			src_arg = &freg->args[j];
			if (!src_arg || !src_arg->arg_name) {
				src_arg=NULL;
				break;
			}
			if (!strcmp(src_arg->arg_name, "src")) break;
			src_arg = NULL;
			j++;
		}
		if (!src_arg)
			continue;

		s = freg->probe_url(sURL, mime_type);
		if (s > score) {
			candidate_freg = freg;
			score = s;
		}
	}
	if (!candidate_freg) {
		gf_free(sURL);
		if (err) *err = GF_NOT_SUPPORTED;
		return NULL;
	}
	if (sep) sep[0] = ':';

	user_args_len = user_args ? strlen(user_args) : 0;
	args = gf_malloc(sizeof(char)*(5+strlen(sURL) + (user_args_len ? user_args_len + 1  :0) ) );
	strcpy(args, "src=");
	strcat(args, sURL);
	if (user_args_len) {
		strcat(args, ":gpac:");
		strcat(args, user_args);
	}

	e = GF_OK;
	if (!filter) {
		filter = gf_filter_new(fsess, candidate_freg, args, GF_FILTER_ARG_GLOBAL_SOURCE, err);
	} else {
		filter->freg = candidate_freg;
		e = gf_filter_new_finalize(filter, args, GF_FILTER_ARG_GLOBAL_SOURCE);
		if (err) *err = e;
	}
	
	if (!e && filter && !filter->num_output_pids)
		gf_filter_post_process_task(filter);

	gf_free(sURL);
	if (filter) {
		if (filter->src_args) gf_free(filter->src_args);
		filter->src_args = args;
		filter->dst_filter = dst_filter;
	} else {
		gf_free(args);
	}
	return filter;
}

GF_Filter *gf_fs_load_source(GF_FilterSession *fsess, const char *url, const char *args, const char *parent_url, GF_Err *err)
{
	return gf_fs_load_source_internal(fsess, url, args, parent_url, err, NULL, NULL);
}


GF_EXPORT
GF_Err gf_fs_add_event_listener(GF_FilterSession *fsess, GF_FSEventListener *el)
{
	GF_Err e;
	if (!fsess || !el || !el->on_event) return GF_BAD_PARAM;
	while (fsess->in_event_listener) gf_sleep(1);
	gf_mx_p(fsess->evt_mx);
	if (!fsess->event_listeners) {
		fsess->event_listeners = gf_list_new();
	}
	e = gf_list_add(fsess->event_listeners, el);
	gf_mx_v(fsess->evt_mx);
	return e;
}

GF_EXPORT
GF_Err gf_fs_remove_event_listener(GF_FilterSession *fsess, GF_FSEventListener *el)
{
	if (!fsess || !el || !fsess->event_listeners) return GF_BAD_PARAM;

	while (fsess->in_event_listener) gf_sleep(1);
	gf_mx_p(fsess->evt_mx);
	gf_list_del_item(fsess->event_listeners, el);
	if (!gf_list_count(fsess->event_listeners)) {
		gf_list_del(fsess->event_listeners);
		fsess->event_listeners=NULL;
	}
	gf_mx_v(fsess->evt_mx);
	return GF_OK;
}

GF_EXPORT
Bool gf_fs_forward_event(GF_FilterSession *fsess, GF_Event *evt, Bool consumed, Bool forward_only)
{
	if (!fsess) return GF_FALSE;

	if (fsess->event_listeners) {
		GF_FSEventListener *el;
		u32 i=0;

		gf_mx_p(fsess->evt_mx);
		fsess->in_event_listener ++;
		gf_mx_v(fsess->evt_mx);
		while ((el = gf_list_enum(fsess->event_listeners, &i))) {
			if (el->on_event(el->udta, evt, consumed)) {
				fsess->in_event_listener --;
				return GF_TRUE;
			}
		}
		fsess->in_event_listener --;
	}

	if (!forward_only && !consumed && fsess->user->EventProc) {
		Bool res;
//		term->nb_calls_in_event_proc++;
		res = fsess->user->EventProc(fsess->user->opaque, evt);
//		term->nb_calls_in_event_proc--;
		return res;
	}
	return GF_FALSE;
}

GF_EXPORT
Bool gf_fs_send_event(GF_FilterSession *fsess, GF_Event *evt)
{
	return gf_fs_forward_event(fsess, evt, 0, 0);
}

GF_EXPORT
void gf_filter_get_session_caps(GF_Filter *filter, GF_FilterSessionCaps *caps)
{
	if (caps) {
		if (filter) {
			(*caps) = filter->session->caps;
		} else {
			memset(caps, 0, sizeof(GF_FilterSessionCaps));
		}
	}
}


GF_EXPORT
GF_DownloadManager *gf_filter_get_download_manager(GF_Filter *filter)
{
	GF_FilterSession *fsess;
	if (!filter) return NULL;
	fsess = filter->session;

	if (!fsess->download_manager) {
		fsess->download_manager = gf_dm_new(NULL);
	}
	safe_int_inc(&fsess->nb_dm_users);
	return fsess->download_manager;
}

void gf_fs_cleanup_filters_task(GF_FSTask *task)
{
	GF_FilterSession *fsess = task->udta;
	u32 i, count = gf_list_count(fsess->filters);
	for (i=0; i<count; i++) {
		GF_Filter *filter = gf_list_get(fsess->filters, i);
		//dynamic filter with no connections, remove it
		if (filter->dynamic_filter && !filter->num_input_pids && !filter->num_output_pids) {
			gf_list_rem(fsess->filters, i);
			i--;
			count--;

			filter->removed = GF_TRUE;
			filter->finalized = GF_TRUE;
			gf_filter_del(filter);
		}
	}
}

void gf_fs_cleanup_filters(GF_FilterSession *fsess)
{
#if 0
	if (fsess->filters_mx) gf_mx_p(fsess->filters_mx);
	if ( safe_int_dec(&fsess->pid_connect_tasks_pending) == 0) {
		//we need a task for cleanup, otherwise we may destroy the filter and the task of the task currently scheduled !!
		gf_fs_post_task(fsess, gf_fs_cleanup_filters_task, NULL, NULL, "filters_cleanup", fsess);
	}
	if (fsess->filters_mx) gf_mx_v(fsess->filters_mx);
#endif
}

GF_EXPORT
GF_Err gf_fs_get_last_connect_error(GF_FilterSession *fs)
{
	GF_Err e;
	if (!fs) return GF_BAD_PARAM;
	e = fs->last_connect_error;
	fs->last_connect_error = GF_OK;
	return e;
}

GF_EXPORT
GF_Err gf_fs_get_last_process_error(GF_FilterSession *fs)
{
	GF_Err e;
	if (!fs) return GF_BAD_PARAM;
	e = fs->last_process_error;
	fs->last_process_error = GF_OK;
	return e;
}


#ifdef FILTER_FIXME


static Bool term_find_res(GF_TermLocales *loc, char *parent, char *path, char *relocated_path, char *localized_rel_path)
{
	FILE *f;

	if (loc->szAbsRelocatedPath) gf_free(loc->szAbsRelocatedPath);
	loc->szAbsRelocatedPath = gf_url_concatenate(parent, path);
	if (!loc->szAbsRelocatedPath) loc->szAbsRelocatedPath = gf_strdup(path);

	f = gf_fopen(loc->szAbsRelocatedPath, "rb");
	if (f) {
		gf_fclose(f);
		strcpy(localized_rel_path, path);
		strcpy(relocated_path, loc->szAbsRelocatedPath);
		return 1;
	}
	return 0;
}

/* Checks if, for a given relative path, there exists a localized version in an given folder
   if this is the case, it returns the absolute localized path, otherwise it returns null.
   if the resource was localized, the last parameter is set to the localized relative path.
*/
static Bool term_check_locales(void *__self, const char *locales_parent_path, const char *rel_path, char *relocated_path, char *localized_rel_path)
{
	char path[GF_MAX_PATH];
	const char *opt;

	GF_TermLocales *loc = (GF_TermLocales*)__self;

	/* Checks if the rel_path argument really contains a relative path (no ':', no '/' at the beginning) */
	if (strstr(rel_path, "://") || (rel_path[0]=='/') || strstr(rel_path, ":\\") || !strncmp(rel_path, "\\\\", 2)) {
		return 0;
	}

	/*Checks if the absolute path is really absolute and points to a local file (no http or else) */
	if (!locales_parent_path ||
	        (locales_parent_path && (locales_parent_path[0] != '/') && strstr(locales_parent_path, "://") && strnicmp(locales_parent_path, "file://", 7))) {
		return 0;
	}
	opt = gf_cfg_get_key(loc->term->user->config, "Systems", "Language2CC");
	if (opt) {
		if (!strcmp(opt, "*") || !strcmp(opt, "un") )
			opt = NULL;
	}

	while (opt) {
		char lan[100];
		char *sep;
		char *sep_lang = strchr(opt, ';');
		if (sep_lang) sep_lang[0] = 0;

		while (strchr(" \t", opt[0]))
			opt++;

		strcpy(lan, opt);

		if (sep_lang) {
			sep_lang[0] = ';';
			opt = sep_lang+1;
		} else {
			opt = NULL;
		}

		while (1) {
			sep = strstr(lan, "-*");
			if (!sep) break;
			strncpy(sep, sep+2, strlen(sep)-2);
		}

		sprintf(path, "locales/%s/%s", lan, rel_path);
		if (term_find_res(loc, (char *) locales_parent_path, (char *) path, relocated_path, localized_rel_path))
			return 1;

		/*recursively remove region (sub)tags*/
		while (1) {
			sep = strrchr(lan, '-');
			if (!sep) break;
			sep[0] = 0;
			sprintf(path, "locales/%s/%s", lan, rel_path);
			if (term_find_res(loc, (char *) locales_parent_path, (char *) path, relocated_path, localized_rel_path))
				return 1;
		}
	}

	if (term_find_res(loc, (char *) locales_parent_path, (char *) rel_path, relocated_path, localized_rel_path))
		return 1;
	/* if we did not find the localized file, both the relocated and localized strings are NULL */
	strcpy(localized_rel_path, "");
	strcpy(relocated_path, "");
	return 0;
}

#endif


