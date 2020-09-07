/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2020
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

#ifndef GPAC_DISABLE_3D
#include <gpac/modules/video_out.h>
#endif
//#define CHECK_TASK_LIST_INTEGRITY

#ifndef GPAC_DISABLE_PLAYER
struct _gf_ft_mgr *gf_font_manager_new();
void gf_font_manager_del(struct _gf_ft_mgr *fm);
#endif


static GFINLINE void gf_fs_sema_io(GF_FilterSession *fsess, Bool notify, Bool main)
{
	GF_Semaphore *sem = main ? fsess->semaphore_main : fsess->semaphore_other;
	if (sem) {
		if (notify) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u notify scheduler %s semaphore\n", gf_th_id(), main ? "main" : "secondary"));
			if ( ! gf_sema_notify(sem, 1)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_SCHEDULER, ("Cannot notify scheduler of new task, semaphore failure\n"));
			}
		} else {
			u32 nb_tasks;
			//if not main and no tasks in main list, this could be the last task to process.
			//if main thread is sleeping force a wake to take further actions (only the main thread decides the exit)
			//this also ensures that tha main thread will process tasks from secondary task lists if no
			//dedicated main thread tasks are present (eg no GL filters)
			if (!main && fsess->in_main_sem_wait && !gf_fq_count(fsess->main_thread_tasks)) {
				gf_fs_sema_io(fsess, GF_TRUE, GF_TRUE);
			}
			nb_tasks = 1;
			//no active threads, count number of tasks. If no posted tasks we are likely at the end of the session, don't block, rather use a sem_wait 
			if (!fsess->active_threads)
			 	nb_tasks = gf_fq_count(fsess->main_thread_tasks) + gf_fq_count(fsess->tasks);

			//if main semaphore, keep track that we are going to sleep
			if (main) {
				fsess->in_main_sem_wait = GF_TRUE;
				if (!nb_tasks) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("No tasks scheduled, waiting on main semaphore for at most 100 ms\n"));
					if (gf_sema_wait_for(sem, 100)) {
					}
				} else {
					if (gf_sema_wait(sem)) {
					}
				}
				fsess->in_main_sem_wait = GF_FALSE;
			} else {
				if (!nb_tasks) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("No tasks scheduled, waiting on secondary semaphore for at most 100 ms\n"));
					if (gf_sema_wait_for(sem, 100)) {
					}
				} else {
					if (gf_sema_wait(sem)) {
					}
				}
			}
		}
	}
}

void gf_fs_add_filter_register(GF_FilterSession *fsess, const GF_FilterRegister *freg)
{
	if (!freg) return;

	if (!freg->name) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter missing name - ignoring\n"));
		return;
	}
	if (!freg->process) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter %s missing process function - ignoring\n", freg->name));
		return;
	}
	if (fsess->blacklist) {
		char *fname = strstr(fsess->blacklist, freg->name);
		if (fname) {
			u32 len = (u32) strlen(freg->name);
			if (!fname[len] || (fname[len] == fsess->sep_list)) {
				return;
			}
		}
	}
	gf_list_add(fsess->registry, (void *) freg);

	if (fsess->init_done && fsess->links && gf_list_count( fsess->links)) {
		gf_filter_sess_build_graph(fsess, freg);
	}
}



static Bool fs_default_event_proc(void *ptr, GF_Event *evt)
{
	GF_FilterSession *fs = (GF_FilterSession *)ptr;
#ifdef GPAC_HAS_QJS
	if (fs->on_evt_task)
		return jsfs_on_event(fs, evt);
#endif

	if (evt->type==GF_EVENT_QUIT) {
		gf_fs_abort(fs, GF_FALSE);
	}
	return 0;
}

GF_EXPORT
GF_FilterSession *gf_fs_new(s32 nb_threads, GF_FilterSchedulerType sched_type, u32 flags, const char *blacklist)
{
	const char *opt;
	Bool gf_sys_has_filter_global_args();
	Bool gf_sys_has_filter_global_meta_args();

	u32 i, count;
	GF_FilterSession *fsess, *a_sess;

	//safety check: all built-in properties shall have unique 4CCs
	if ( ! gf_props_4cc_check_props())
		return NULL;

	GF_SAFEALLOC(fsess, GF_FilterSession);
	if (!fsess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to alloc media session\n"));
		return NULL;
	}
	fsess->flags = flags;

	fsess->filters = gf_list_new();
	fsess->main_th.fsess = fsess;

	if ((s32) nb_threads == -1) {
		GF_SystemRTInfo rti;
		memset(&rti, 0, sizeof(GF_SystemRTInfo));
		if (gf_sys_get_rti(0, &rti, 0)) {
			nb_threads = rti.nb_cores-1;
		}
		if ((s32)nb_threads<0) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Failed to query number of cores, disabling extra threads for session\n"));
			nb_threads=0;
		}
	}

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

	if (!(flags & GF_FS_FLAG_NO_RESERVOIR)) {
		fsess->tasks_reservoir = gf_fq_new(fsess->tasks_mx);
	}

	if (nb_threads || (sched_type==GF_FS_SCHEDULER_LOCK_FORCE) ) {
		fsess->semaphore_main = fsess->semaphore_other = gf_sema_new(GF_INT_MAX, 0);
		if (nb_threads>0)
			fsess->semaphore_other = gf_sema_new(GF_INT_MAX, 0);

		//force testing of mutex queues
		//fsess->use_locks = GF_TRUE;
	}
	fsess->ui_event_proc = fs_default_event_proc;
	fsess->ui_opaque = fsess;

	if (flags & GF_FS_FLAG_NO_MAIN_THREAD)
		fsess->no_main_thread = GF_TRUE;

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

	if (!(flags & GF_FS_FLAG_NO_RESERVOIR)) {
#if GF_PROPS_HASHTABLE_SIZE
		fsess->prop_maps_list_reservoir = gf_fq_new(fsess->props_mx);
#endif
		fsess->prop_maps_reservoir = gf_fq_new(fsess->props_mx);
		fsess->prop_maps_entry_reservoir = gf_fq_new(fsess->props_mx);
		fsess->prop_maps_entry_data_alloc_reservoir = gf_fq_new(fsess->props_mx);
		//we also use the props mutex for the this one
		fsess->pcks_refprops_reservoir = gf_fq_new(fsess->props_mx);
	}


#ifndef GPAC_DISABLE_REMOTERY
	sprintf(fsess->main_th.rmt_name, "FSThread0");
#endif

	if (!fsess->filters || !fsess->tasks) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to alloc media session\n"));
		fsess->run_status = GF_OUT_OF_MEM;
		gf_fs_del(fsess);
		return NULL;
	}

	if (nb_threads) {
		fsess->info_mx = gf_mx_new("FilterSessionInfo");
		fsess->ui_mx = gf_mx_new("FilterSessionUIProc");
	}

	for (i=0; i<(u32) nb_threads; i++) {
		GF_SessionThread *sess_thread;
		GF_SAFEALLOC(sess_thread, GF_SessionThread);
		if (!sess_thread) continue;
#ifndef GPAC_DISABLE_REMOTERY
		sprintf(sess_thread->rmt_name, "FSThread%d", i+1);
#endif
		sess_thread->th = gf_th_new("MediaSessionThread");
		if (!sess_thread->th) {
			gf_free(sess_thread);
			continue;
		}
		sess_thread->fsess = fsess;
		gf_list_add(fsess->threads, sess_thread);
	}

	gf_fs_set_separators(fsess, NULL);

	fsess->registry = gf_list_new();
	fsess->blacklist = blacklist;
	a_sess = (flags & GF_FS_FLAG_LOAD_META) ? fsess : NULL;
	gf_fs_reg_all(fsess, a_sess);

	//load external modules
	count = gf_modules_count();
	for (i=0; i<count; i++) {
		GF_FilterRegister *freg = (GF_FilterRegister *) gf_modules_load_filter(i, a_sess);
		if (freg) {
			freg->flags |= 0x80000000;
			gf_fs_add_filter_register(fsess, freg);
		}
	}
	fsess->blacklist = NULL;

	//todo - find a way to handle events without mutex ...
	fsess->evt_mx = gf_mx_new("Event mutex");

	fsess->blocking_mode = GF_FS_BLOCK_ALL;
	opt = gf_opts_get_key("core", "no-block");
	if (opt) {
		if (!strcmp(opt, "fanout")) {
			fsess->blocking_mode = GF_FS_NOBLOCK_FANOUT;
		}
		else if (!strcmp(opt, "all")) {
			fsess->blocking_mode = GF_FS_NOBLOCK;
		}
	}
	fsess->run_status = GF_EOS;
	fsess->nb_threads_stopped = 1+nb_threads;
	fsess->default_pid_buffer_max_us = 1000;
	fsess->decoder_pid_buffer_max_us = 1000000;
	fsess->default_pid_buffer_max_units = 1;
	fsess->max_resolve_chain_len = 6;
	fsess->auto_inc_nums = gf_list_new();

	if (nb_threads)
		fsess->links_mx = gf_mx_new("FilterRegistryGraph");
	fsess->links = gf_list_new();

#ifndef GPAC_DISABLE_3D
	fsess->gl_providers = gf_list_new();
#endif

	if (! (fsess->flags & GF_FS_FLAG_NO_GRAPH_CACHE))
		gf_filter_sess_build_graph(fsess, NULL);

	fsess->init_done = GF_TRUE;


	if (gf_sys_has_filter_global_args() || gf_sys_has_filter_global_meta_args()) {
		u32 nb_args = gf_sys_get_argc();
		for (i=0; i<nb_args; i++) {
			char *arg = (char *)gf_sys_get_arg(i);
			if (arg[0]!='-') continue;
			if ((arg[1]!='-') && (arg[1]!='+')) continue;
			char *sep = strchr(arg, '=');
			if (sep) sep[0] = 0;
			gf_fs_push_arg(fsess, arg+2, GF_FALSE, (arg[1]!='-') ? 2 : 1);
			if (sep) sep[0] = '=';
		}
	}

	return fsess;
}

void gf_fs_push_arg(GF_FilterSession *session, const char *szArg, Bool was_found, u32 type)
{
	if (session->flags & GF_FS_FLAG_NO_ARG_CHECK)
		return;

	if (!was_found) {
		Bool afound = GF_FALSE;
		u32 k, acount;
		if (!session->parsed_args) session->parsed_args = gf_list_new();
		acount = gf_list_count(session->parsed_args);
		for (k=0; k<acount; k++) {
			GF_FSArgItem *ai = gf_list_get(session->parsed_args, k);
			if (!strcmp(ai->argname, szArg)) {
				afound = GF_TRUE;
				if ((ai->type==2) && (type==2))
					ai->found = GF_FALSE;
				break;
			}
		}
		if (!afound) {
			GF_FSArgItem *ai;
			GF_SAFEALLOC(ai, GF_FSArgItem);
			if (ai) {
				ai->argname = gf_strdup(szArg);
				ai->type = type;
				gf_list_add(session->parsed_args, ai );
			}
		}
	} else {
		u32 k, acount;
		Bool found = GF_FALSE;
		if (!session->parsed_args) session->parsed_args = gf_list_new();
		acount = gf_list_count(session->parsed_args);
		for (k=0; k<acount; k++) {
			GF_FSArgItem *ai = gf_list_get(session->parsed_args, k);
			if (!strcmp(ai->argname, szArg)) {
				ai->found = GF_TRUE;
				found = GF_TRUE;
				break;
			}
		}
		if (!found) {
			GF_FSArgItem *ai;
			GF_SAFEALLOC(ai, GF_FSArgItem);
			if (ai) {
				ai->argname = gf_strdup(szArg);
				ai->type = type;
				ai->found = GF_TRUE;
				gf_list_add(session->parsed_args, ai );
			}
		}
	}
}


GF_EXPORT
GF_FilterSession *gf_fs_new_defaults(u32 inflags)
{
	GF_FilterSession *fsess;
	GF_FilterSchedulerType sched_type = GF_FS_SCHEDULER_LOCK_FREE;
	u32 flags = 0;
	s32 nb_threads = gf_opts_get_int("core", "threads");
	const char *blacklist = gf_opts_get_key("core", "blacklist");
	const char *opt = gf_opts_get_key("core", "sched");

	if (!opt) sched_type = GF_FS_SCHEDULER_LOCK_FREE;
	else if (!strcmp(opt, "lock")) sched_type = GF_FS_SCHEDULER_LOCK;
	else if (!strcmp(opt, "flock")) sched_type = GF_FS_SCHEDULER_LOCK_FORCE;
	else if (!strcmp(opt, "direct")) sched_type = GF_FS_SCHEDULER_DIRECT;
	else if (!strcmp(opt, "free")) sched_type = GF_FS_SCHEDULER_LOCK_FREE;
	else if (!strcmp(opt, "freex")) sched_type = GF_FS_SCHEDULER_LOCK_FREE_X;
	else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Unrecognized scheduler type %s\n", opt));
		return NULL;
	}
	if (inflags & GF_FS_FLAG_LOAD_META)
		flags |= GF_FS_FLAG_LOAD_META;

	if (inflags & GF_FS_FLAG_NO_MAIN_THREAD)
		flags |= GF_FS_FLAG_NO_MAIN_THREAD;

	if (inflags & GF_FS_FLAG_NO_GRAPH_CACHE)
		flags |= GF_FS_FLAG_NO_GRAPH_CACHE;

	if (gf_opts_get_bool("core", "dbg-edges"))
		flags |= GF_FS_FLAG_PRINT_CONNECTIONS;

	if (gf_opts_get_bool("core", "full-link"))
		flags |= GF_FS_FLAG_FULL_LINK;

	if (gf_opts_get_bool("core", "no-reg"))
		flags |= GF_FS_FLAG_NO_REGULATION;

	if (gf_opts_get_bool("core", "no-reassign"))
		flags |= GF_FS_FLAG_NO_REASSIGN;

	if (gf_opts_get_bool("core", "no-graph-cache"))
		flags |= GF_FS_FLAG_NO_GRAPH_CACHE;

	if (gf_opts_get_bool("core", "no-probe"))
		flags |= GF_FS_FLAG_NO_PROBE;

	if (gf_opts_get_bool("core", "no-argchk"))
		flags |= GF_FS_FLAG_NO_ARG_CHECK;

	if (gf_opts_get_bool("core", "no-reservoir"))
		flags |= GF_FS_FLAG_NO_RESERVOIR;


	fsess = gf_fs_new(nb_threads, sched_type, flags, blacklist);
	if (!fsess) return NULL;

	gf_fs_set_max_resolution_chain_length(fsess, gf_opts_get_int("core", "max-chain") );

	gf_fs_set_max_sleep_time(fsess, gf_opts_get_int("core", "max-sleep") );

	opt = gf_opts_get_key("core", "seps");
	if (opt)
		gf_fs_set_separators(fsess, opt);

	return fsess;
}


GF_EXPORT
GF_Err gf_fs_set_separators(GF_FilterSession *session, const char *separator_set)
{
	if (!session) return GF_BAD_PARAM;
	if (separator_set && (strlen(separator_set)<5)) return GF_BAD_PARAM;

	if (separator_set) {
		session->sep_args = separator_set[0];
		session->sep_name = separator_set[1];
		session->sep_frag = separator_set[2];
		session->sep_list = separator_set[3];
		session->sep_neg = separator_set[4];
	} else {
		session->sep_args = ':';
		session->sep_name = '=';
		session->sep_frag = '#';
		session->sep_list = ',';
		session->sep_neg = '!';
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_fs_set_max_resolution_chain_length(GF_FilterSession *session, u32 max_chain_length)
{
	if (!session) return GF_BAD_PARAM;
	session->max_resolve_chain_len = max_chain_length;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_fs_set_max_sleep_time(GF_FilterSession *session, u32 max_sleep)
{
	if (!session) return GF_BAD_PARAM;
	session->max_sleep = max_sleep;
	return GF_OK;
}

GF_EXPORT
u32 gf_fs_get_max_resolution_chain_length(GF_FilterSession *session)
{
	if (!session) return 0;
	return session->max_resolve_chain_len;
}

void gf_fs_remove_filter_register(GF_FilterSession *session, GF_FilterRegister *freg)
{
	gf_list_del_item(session->registry, freg);
	gf_filter_sess_reset_graph(session, freg);
}

GF_EXPORT
void gf_fs_set_ui_callback(GF_FilterSession *fs, Bool (*ui_event_proc)(void *opaque, GF_Event *event), void *cbk_udta)
{
	if (fs) {
		fs->ui_event_proc = ui_event_proc;
		fs->ui_opaque = cbk_udta;
		if (!fs->ui_event_proc) {
			fs->ui_event_proc = fs_default_event_proc;
			fs->ui_opaque = fs;
		}
	}
}

void gf_propalloc_del(void *it)
{
	GF_PropertyEntry *pe = (GF_PropertyEntry *)it;
	if (pe->prop.value.data.ptr) gf_free(pe->prop.value.data.ptr);
	gf_free(pe);
}


GF_EXPORT
Bool gf_fs_enum_unmapped_options(GF_FilterSession *fsess, u32 *idx, char **argname, u32 *argtype)
{
	if (!fsess || !fsess->parsed_args) return GF_FALSE;
	u32 i, count = gf_list_count(fsess->parsed_args);

	for (i=*idx; i<count; i++) {
		GF_FSArgItem *ai = gf_list_get(fsess->parsed_args, i);
		(*idx)++;
		if (ai->found) continue;
		if (argname) *argname = ai->argname;
		if (argtype) *argtype = ai->type;
		return GF_TRUE;
	}
	return GF_FALSE;
}


GF_EXPORT
void gf_fs_del(GF_FilterSession *fsess)
{
	assert(fsess);

	gf_fs_stop(fsess);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Session destroy begin\n"));

	if (fsess->parsed_args) {
		while (gf_list_count(fsess->parsed_args)) {
			GF_FSArgItem *ai = gf_list_pop_back(fsess->parsed_args);
			gf_free(ai->argname);
			gf_free(ai);
		}
		gf_list_del(fsess->parsed_args);
	}

	//temporary until we don't introduce fsess_stop
	assert(fsess->run_status != GF_OK);
	if (fsess->filters) {
		u32 i, count=gf_list_count(fsess->filters);
		//first pass: disconnect all filters, since some may have references to property maps or packets 
		for (i=0; i<count; i++) {
			u32 j;
			GF_Filter *filter = gf_list_get(fsess->filters, i);
			filter->process_th_id = 0;
			filter->scheduled_for_next_task = GF_TRUE;

			if (filter->detached_pid_inst) {
				while (gf_list_count(filter->detached_pid_inst)) {
					GF_FilterPidInst *pidi = gf_list_pop_front(filter->detached_pid_inst);
					gf_filter_pid_inst_del(pidi);
				}
				gf_list_del(filter->detached_pid_inst);
				filter->detached_pid_inst = NULL;
			}

			if (filter->postponed_packets) {
				while (gf_list_count(filter->postponed_packets)) {
					GF_FilterPacket *pck = gf_list_pop_front(filter->postponed_packets);
					gf_filter_packet_destroy(pck);
				}
				gf_list_del(filter->postponed_packets);
				filter->postponed_packets = NULL;
			}
			for (j=0; j<filter->num_input_pids; j++) {
				GF_FilterPidInst *pidi = gf_list_get(filter->input_pids, j);
				gf_filter_pid_inst_reset(pidi);
			}
			filter->scheduled_for_next_task = GF_FALSE;
		}
		//second pass, finalize all
		for (i=0; i<count; i++) {
			GF_Filter *filter = gf_list_get(fsess->filters, i);
			if (filter->freg->finalize && !filter->finalized) {
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
		fsess->filters = NULL;
	}

	gf_fs_unload_script(fsess, NULL);

	if (fsess->download_manager) gf_dm_del(fsess->download_manager);
#ifndef GPAC_DISABLE_PLAYER
	if (fsess->font_manager) gf_font_manager_del(fsess->font_manager);
#endif

	if (fsess->registry) {
		while (gf_list_count(fsess->registry)) {
			GF_FilterRegister *freg = gf_list_pop_back(fsess->registry);
			if (freg->register_free) freg->register_free(fsess, freg);
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

	if (fsess->prop_maps_reservoir)
		gf_fq_del(fsess->prop_maps_reservoir, gf_propmap_del);
#if GF_PROPS_HASHTABLE_SIZE
	if (fsess->prop_maps_list_reservoir)
		gf_fq_del(fsess->prop_maps_list_reservoir, (gf_destruct_fun) gf_list_del);
#endif
	if (fsess->prop_maps_entry_reservoir)
		gf_fq_del(fsess->prop_maps_entry_reservoir, gf_void_del);
	if (fsess->prop_maps_entry_data_alloc_reservoir)
		gf_fq_del(fsess->prop_maps_entry_data_alloc_reservoir, gf_propalloc_del);
	if (fsess->pcks_refprops_reservoir)
		gf_fq_del(fsess->pcks_refprops_reservoir, gf_void_del);


	if (fsess->props_mx)
		gf_mx_del(fsess->props_mx);

	if (fsess->info_mx)
		gf_mx_del(fsess->info_mx);

	if (fsess->ui_mx)
		gf_mx_del(fsess->ui_mx);

	if (fsess->semaphore_other && (fsess->semaphore_other != fsess->semaphore_main) )
		gf_sema_del(fsess->semaphore_other);

	if (fsess->semaphore_main)
		gf_sema_del(fsess->semaphore_main);

	if (fsess->tasks_mx)
		gf_mx_del(fsess->tasks_mx);

	if (fsess->filters_mx)
		gf_mx_del(fsess->filters_mx);

	if (fsess->evt_mx) gf_mx_del(fsess->evt_mx);
	if (fsess->event_listeners) gf_list_del(fsess->event_listeners);

	if (fsess->links) {
		gf_filter_sess_reset_graph(fsess, NULL);
		gf_list_del(fsess->links);
	}
	if (fsess->links_mx) gf_mx_del(fsess->links_mx);

#ifndef GPAC_DISABLE_3D
	gf_list_del(fsess->gl_providers);
	if (fsess->gl_driver) {
		fsess->gl_driver->Shutdown(fsess->gl_driver);
		gf_modules_close_interface((GF_BaseInterface *)fsess->gl_driver);
	}
#endif

	if (fsess->auto_inc_nums) {
		while(gf_list_count(fsess->auto_inc_nums)) {
			GF_FSAutoIncNum *aint = gf_list_pop_back(fsess->auto_inc_nums);
			gf_free(aint);
		}
		gf_list_del(fsess->auto_inc_nums);
	}

	gf_free(fsess);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Session destroyed\n"));
}

GF_EXPORT
u32 gf_fs_filters_registers_count(GF_FilterSession *fsess)
{
	return fsess ? gf_list_count(fsess->registry) : 0;
}

GF_EXPORT
const GF_FilterRegister * gf_fs_get_filter_register(GF_FilterSession *fsess, u32 idx)
{
	return gf_list_get(fsess->registry, idx);
}

#ifdef CHECK_TASK_LIST_INTEGRITY
static Bool check_task_list_enum(void *udta, void *item)
{
	assert(udta != item);
	return GF_FALSE;
}
static void check_task_list(GF_FilterQueue *fq, GF_FSTask *task)
{
	if (fq) {
		gf_fq_enum(fq, check_task_list_enum, task);
	}
}
#endif


void gf_fs_post_task_ex(GF_FilterSession *fsess, gf_fs_task_callback task_fun, GF_Filter *filter, GF_FilterPid *pid, const char *log_name, void *udta, Bool is_configure, Bool force_direct_call)
{
	GF_FSTask *task;
	Bool force_main_thread = GF_FALSE;
	Bool notified = GF_FALSE;

	assert(fsess);
	assert(task_fun);

	//only flatten calls if in main thread (we still have some broken filters using threading
	//that could trigger tasks
	if ((force_direct_call || fsess->direct_mode)
		&& (!filter || !filter->in_process)
		&& fsess->tasks_in_process
		&& (gf_th_id()==fsess->main_th.th_id)
	) {
		GF_FSTask atask;
		u64 task_time = gf_sys_clock_high_res();
		memset(&atask, 0, sizeof(GF_FSTask));
		atask.filter = filter;
		atask.pid = pid;
		atask.run_task = task_fun;
		atask.log_name = log_name;
		atask.udta = udta;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread 0 task#%d %p executing Filter %s::%s (%d tasks pending)\n", fsess->main_th.nb_tasks, &atask, filter ? filter->name : "none", log_name, fsess->tasks_pending));
		if (filter)
			filter->scheduled_for_next_task = GF_TRUE;
		task_fun(&atask);
		filter = atask.filter;
		if (filter) {
			filter->time_process += gf_sys_clock_high_res() - task_time;
			filter->scheduled_for_next_task = GF_FALSE;
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

	if (filter && is_configure) {
		if (filter->freg->flags & GF_FS_REG_CONFIGURE_MAIN_THREAD)
			force_main_thread = GF_TRUE;
	}

	if (filter) {
		gf_mx_p(filter->tasks_mx);
		//no tasks and not scheduled
		if (! filter->scheduled_for_next_task && !gf_fq_count(filter->tasks)) {
			notified = task->notified = GF_TRUE;

			if (!force_main_thread)
				force_main_thread = (filter->main_thread_forced || (filter->freg->flags & GF_FS_REG_MAIN_THREAD)) ? GF_TRUE : GF_FALSE;
		} else if (force_main_thread) {
			force_main_thread = GF_FALSE;
			if (filter->process_th_id && (fsess->main_th.th_id != filter->process_th_id)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_SCHEDULER, ("Cannot post task to main thread, filter is already scheduled\n"));
			}
		}
		if (!force_main_thread)
			task->blocking = (filter->freg->flags & GF_FS_REG_BLOCKING) ? GF_TRUE : GF_FALSE;

		gf_fq_add(filter->tasks, task);
		gf_mx_v(filter->tasks_mx);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u Posted task %p Filter %s::%s (%d (%d) pending, %d process tasks) on %s task list\n", gf_th_id(), task, filter->name, task->log_name, fsess->tasks_pending, gf_fq_count(filter->tasks), filter->process_task_queued, task->notified ? (force_main_thread ? "main" : "secondary") : "filter"));
	} else {
		task->notified = notified = GF_TRUE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u Posted filter-less task %s (%d pending) on secondary task list\n", gf_th_id(), task->log_name, fsess->tasks_pending));
	}

	//WARNING, do not use task->notified since the task may have been posted to the filter task list and may already have been swapped
	//with a different value !
	if (notified) {
#ifdef CHECK_TASK_LIST_INTEGRITY
		check_task_list(fsess->main_thread_tasks, task);
		check_task_list(fsess->tasks, task);
		check_task_list(fsess->tasks_reservoir, task);
#endif
		assert(task->run_task);
		if (filter) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u posting filter task, scheduled_for_next_task %d\n", gf_th_id(), filter->scheduled_for_next_task));
			assert(!filter->scheduled_for_next_task);
		}

		//notify/count tasks posted on the main task or regular task lists
		safe_int_inc(&fsess->tasks_pending);
		if (filter && force_main_thread) {
			gf_fq_add(fsess->main_thread_tasks, task);
			gf_fs_sema_io(fsess, GF_TRUE, GF_TRUE);
		} else {
			assert(task->run_task);
			gf_fq_add(fsess->tasks, task);
			gf_fs_sema_io(fsess, GF_TRUE, GF_FALSE);
		}
	}
}

void gf_fs_post_task(GF_FilterSession *fsess, gf_fs_task_callback task_fun, GF_Filter *filter, GF_FilterPid *pid, const char *log_name, void *udta)
{
	gf_fs_post_task_ex(fsess, task_fun, filter, pid, log_name, udta, GF_FALSE, GF_FALSE);
}

GF_EXPORT
Bool gf_fs_check_filter_register_cap(const GF_FilterRegister *f_reg, u32 incode, GF_PropertyValue *cap_input, u32 outcode, GF_PropertyValue *cap_output, Bool exact_match_only)
{
	u32 j;
	u32 has_raw_in = 0;
	u32 has_cid_match = 0;
	u32 exclude_cid_out = 0;
	u32 has_exclude_cid_out = 0;
	for (j=0; j<f_reg->nb_caps; j++) {
		const GF_FilterCapability *cap = &f_reg->caps[j];
		if (!(cap->flags & GF_CAPFLAG_IN_BUNDLE)) {
			//CID not excluded, raw in present and CID explicit match or not included in excluded set
			if (!exclude_cid_out && has_raw_in && (has_cid_match || (!exact_match_only && has_exclude_cid_out) ) ) {
				return GF_TRUE;
			}

			if (has_raw_in != 2) has_raw_in = 0;
			if (has_cid_match != 2) has_cid_match = 0;
			if (exclude_cid_out != 2) exclude_cid_out = 0;
			if (has_exclude_cid_out != 2) has_exclude_cid_out = 0;

			continue;
		}

		if ( (cap->flags & GF_CAPFLAG_INPUT) && (cap->code == incode) ) {
			if (! (cap->flags & GF_CAPFLAG_EXCLUDED) && gf_props_equal(&cap->val, cap_input) ) {
				has_raw_in = (cap->flags & GF_CAPS_INPUT_STATIC) ? 2 : 1;
			}
		}
		if ( (cap->flags & GF_CAPFLAG_OUTPUT) && (cap->code == outcode) ) {
			if (! (cap->flags & GF_CAPFLAG_EXCLUDED)) {
				if (gf_props_equal(&cap->val, cap_output) ) {
					has_cid_match = (cap->flags & GF_CAPS_OUTPUT_STATIC) ? 2 : 1;
				}
			} else {
				if (gf_props_equal(&cap->val, cap_output) ) {
					exclude_cid_out = (cap->flags & GF_CAPS_OUTPUT_STATIC) ? 2 : 1;
				} else {
					has_exclude_cid_out = (cap->flags & GF_CAPS_OUTPUT_STATIC) ? 2 : 1;
				}
			}
		}
	}
	//CID not excluded, raw in present and CID explicit match or not included in excluded set
	if (!exclude_cid_out && has_raw_in && (has_cid_match || (!exact_match_only && has_exclude_cid_out) ) ) {
		return GF_TRUE;
	}
	return GF_FALSE;
}
static GF_Filter *gf_fs_load_encoder(GF_FilterSession *fsess, const char *args)
{
	GF_Err e;
	char szCodec[3];
	char *cid, *sep;
	const GF_FilterRegister *candidate = NULL;
	u32 codecid=0;
	GF_Filter *filter;
	u32 i, count;
	GF_PropertyValue cap_in, cap_out;
	szCodec[0] = 'c';
	szCodec[1] = fsess->sep_name;
	szCodec[2] = 0;

	cid = args ? strstr(args, szCodec) : NULL;
	if (!cid) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Missing codec identifier in \"enc\" definition: %s\n", args ? args : "no arguments"));
		return NULL;
	}
	sep = strchr(cid, fsess->sep_args);
	if (sep) sep[0] = 0;

	codecid = gf_codec_parse(cid+2);
	if (codecid==GF_CODECID_NONE) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Unrecognized codec identifier in \"enc\" definition: %s\n", cid));
		if (sep) sep[0] = fsess->sep_args;
		return NULL;
	}
	if (sep) sep[0] = fsess->sep_args;

	cap_in.type = GF_PROP_UINT;
	cap_in.value.uint = GF_CODECID_RAW;
	cap_out.type = GF_PROP_UINT;
	cap_out.value.uint = codecid;

	count = gf_list_count(fsess->registry);
	for (i=0; i<count; i++) {
		const GF_FilterRegister *f_reg = gf_list_get(fsess->registry, i);

		if ( gf_fs_check_filter_register_cap(f_reg, GF_PROP_PID_CODECID, &cap_in, GF_PROP_PID_CODECID, &cap_out, GF_FALSE)) {
			if (!candidate || (candidate->priority>f_reg->priority))
				candidate = f_reg;
		}
	}
	if (!candidate) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Cannot find any filter providing encoding for %s\n", cid));
		return NULL;
	}
	filter = gf_filter_new(fsess, candidate, args, NULL, GF_FILTER_ARG_EXPLICIT, &e, NULL, GF_FALSE);
	if (!filter) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to load filter %s: %s\n", candidate->name, gf_error_to_string(e) ));
	} else {
		filter->encoder_stream_type = gf_codecid_type(codecid);
	}
	return filter;
}

GF_EXPORT
Bool gf_fs_filter_exists(GF_FilterSession *fsess, const char *name)
{
	u32 i, count;

	if (!strcmp(name, "enc")) return GF_TRUE;

	count = gf_list_count(fsess->registry);
	for (i=0;i<count;i++) {
		const GF_FilterRegister *f_reg = gf_list_get(fsess->registry, i);
		if (!strcmp(f_reg->name, name)) {
			return GF_TRUE;
		}
	}
	return GF_FALSE;
}

GF_EXPORT
GF_Filter *gf_fs_load_filter(GF_FilterSession *fsess, const char *name, GF_Err *err_code)
{
	const char *args=NULL;
	const char *sep;
	u32 i, len, count = gf_list_count(fsess->registry);
	Bool quiet = (err_code && (*err_code == GF_EOS)) ? GF_TRUE : GF_FALSE;

	assert(fsess);
	assert(name);
	if (err_code) *err_code = GF_OK;

	sep = gf_fs_path_escape_colon(fsess, name);
	if (sep) {
		args = sep+1;
		len = (u32) (sep - name);
	} else len = (u32) strlen(name);

	if (!len) {
		if (!quiet) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Missing filter name in %s\n", name));
		}
		return NULL;
	}

	if (!strncmp(name, "enc", len)) {
		return gf_fs_load_encoder(fsess, args);
	}
	/*regular filter loading*/
	for (i=0;i<count;i++) {
		const GF_FilterRegister *f_reg = gf_list_get(fsess->registry, i);
		if ((strlen(f_reg->name)==len) && !strncmp(f_reg->name, name, len)) {
			GF_Filter *filter;
			GF_FilterArgType argtype = GF_FILTER_ARG_EXPLICIT;

			if ((f_reg->flags & GF_FS_REG_REQUIRES_RESOLVER) && !fsess->max_resolve_chain_len) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter %s requires graph resolver but it is disabled\n", name));
				if (err_code) *err_code = GF_BAD_PARAM;
				return NULL;
			}

			if (f_reg->flags & GF_FS_REG_ACT_AS_SOURCE) argtype = GF_FILTER_ARG_EXPLICIT_SOURCE;
			filter = gf_filter_new(fsess, f_reg, args, NULL, argtype, err_code, NULL, GF_FALSE);
			if (!filter) return NULL;
			if (!filter->num_output_pids) {
				const char *src_url = strstr(name, "src");
				if (src_url && (src_url[3]==fsess->sep_name))
					gf_filter_post_process_task(filter);
			}
			return filter;
		}
	}
	/*check JS file*/
	if (strstr(name, ".js") || strstr(name, ".jsf") || strstr(name, ".mjs") ) {
		char szPath[10+GF_MAX_PATH];
		if (len>GF_MAX_PATH)
			return NULL;
		strncpy(szPath, name, len);
		szPath[len]=0;
		if (gf_file_exists(szPath)) {
			sprintf(szPath, "jsf%cjs%c", fsess->sep_args, fsess->sep_name);
			strcat(szPath, name);
			return gf_fs_load_filter(fsess, szPath, err_code);
		}
	}

	if (!quiet) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to load filter %s: no such filter registry\n", name));
	}
	if (err_code) *err_code = GF_FILTER_NOT_FOUND;
	return NULL;
}

//in mono thread mode, we cannot always sleep for the requested timeout in case there are more tasks to be processed
//this defines the number of pending tasks above wich we limit sleep
#define MONOTH_MIN_TASKS	2
//this defines the sleep time for this case
#define MONOTH_MIN_SLEEP	5

static u32 gf_fs_thread_proc(GF_SessionThread *sess_thread)
{
	GF_FilterSession *fsess = sess_thread->fsess;
	u32 i, th_count = fsess->threads ? gf_list_count(fsess->threads) : 0;
	u32 thid =  1 + gf_list_find(fsess->threads, sess_thread);
	u64 enter_time = gf_sys_clock_high_res();
	Bool use_main_sema = thid ? GF_FALSE : GF_TRUE;
#ifndef GPAC_DISABLE_LOG
	u32 sys_thid = gf_th_id();
#endif
	u64 next_task_schedule_time = 0;
	Bool do_regulate = (fsess->flags & GF_FS_FLAG_NO_REGULATION) ? GF_FALSE : GF_TRUE;
	u32 consecutive_filter_tasks=0;
	Bool force_secondary_tasks = GF_FALSE;
	Bool skip_next_sema_wait = GF_FALSE;

	GF_Filter *current_filter = NULL;
	sess_thread->th_id = gf_th_id();

#ifndef GPAC_DISABLE_REMOTERY
	sess_thread->rmt_tasks=40;
	gf_rmt_set_thread_name(sess_thread->rmt_name);
#endif

	gf_rmt_begin(fs_thread, 0);

	safe_int_inc(&fsess->active_threads);

	if (!thid && fsess->no_main_thread) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Main thread proc enter\n"));
	}

	while (1) {
		Bool notified;
		Bool requeue = GF_FALSE;
		u64 active_start, task_time;
		GF_FSTask *task=NULL;
#ifdef CHECK_TASK_LIST_INTEGRITY
		GF_Filter *prev_current_filter = NULL;
		Bool skip_filter_task_check = GF_FALSE;
#endif

#ifndef GPAC_DISABLE_REMOTERY
		sess_thread->rmt_tasks--;
		if (!sess_thread->rmt_tasks) {
			gf_rmt_end();
			gf_rmt_begin(fs_thread, 0);
			sess_thread->rmt_tasks=40;
		}
#endif

		safe_int_dec(&fsess->active_threads);

		if (!skip_next_sema_wait && (current_filter==NULL)) {
			gf_rmt_begin(sema_wait, GF_RMT_AGGREGATE);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u Waiting scheduler %s semaphore\n", sys_thid, use_main_sema ? "main" : "secondary"));
			//wait for something to be done
			gf_fs_sema_io(fsess, GF_FALSE, use_main_sema);
			consecutive_filter_tasks = 0;
			gf_rmt_end();
		}
		safe_int_inc(&fsess->active_threads);
		skip_next_sema_wait = GF_FALSE;

		active_start = gf_sys_clock_high_res();

		if (current_filter==NULL) {
			//main thread
			if (thid==0) {
				if (!force_secondary_tasks) {
					task = gf_fq_pop(fsess->main_thread_tasks);
				}
				if (!task) {
					task = gf_fq_pop(fsess->tasks);
					if (task && task->blocking) {
						gf_fq_add(fsess->tasks, task);
						task = NULL;
						gf_fs_sema_io(fsess, GF_TRUE, GF_FALSE);
					}
				}
				force_secondary_tasks = GF_FALSE;
			} else {
				task = gf_fq_pop(fsess->tasks);
			}
			if (task) {
				assert( task->run_task );
				assert( task->notified );
			}
		} else {
			//keep task in filter tasks list until done
			task = gf_fq_head(current_filter->tasks);
			if (task) {
				assert( task->run_task );
				assert( ! task->notified );
			}
		}

		if (!task) {
			u32 force_nb_notif = 0;
			next_task_schedule_time = 0;
			//no more task and EOS signal
			if (fsess->run_status != GF_OK)
				break;

			if (!fsess->tasks_pending && fsess->main_th.has_seen_eot) {
				//check all threads
				Bool all_done = GF_TRUE;

				for (i=0; i<th_count; i++) {
					GF_SessionThread *st = gf_list_get(fsess->threads, i);
					if (!st->has_seen_eot) {
						all_done = GF_FALSE;
						force_nb_notif++;
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


			//no pending tasks and first time main task queue is empty, flush to detect if we
			//are indeed done
			if (!fsess->tasks_pending && !fsess->tasks_in_process && !sess_thread->has_seen_eot && !gf_fq_count(fsess->tasks)) {
				//maybe last task, force a notify to check if we are truly done
				sess_thread->has_seen_eot = GF_TRUE;
				//not main thread and some tasks pending on main, notify only ourselves
				if (thid && gf_fq_count(fsess->main_thread_tasks)) {
					gf_fs_sema_io(fsess, GF_TRUE, use_main_sema);
				}
				//main thread exit probing, send a notify to main sema (for this thread), and N for the secondary one
				else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u notify scheduler main semaphore\n", gf_th_id()));
					gf_sema_notify(fsess->semaphore_main, 1);
					GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u notify scheduler secondary semaphore %d\n", gf_th_id(), th_count));
					gf_sema_notify(fsess->semaphore_other, th_count);
				}
			}
			//this thread and the main thread are done but we still have unfinished threads, re-notify everyone
			else if (!fsess->tasks_pending && fsess->main_th.has_seen_eot && force_nb_notif) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u notify scheduler main semaphore\n", gf_th_id()));
				gf_sema_notify(fsess->semaphore_main, 1);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u notify scheduler secondary semaphore %d\n", gf_th_id(), th_count));
				gf_sema_notify(fsess->semaphore_other, th_count);
			}

			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u: no task available\n", sys_thid));

			//no main thread, return
			if (!thid && fsess->no_main_thread) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Main thread proc exit\n"));
				return 0;
			}

			if (do_regulate) {
				gf_sleep(0);
			}
			continue;
		}
#ifdef CHECK_TASK_LIST_INTEGRITY
		check_task_list(fsess->main_thread_tasks, task);
		check_task_list(fsess->tasks, task);
#endif
		if (current_filter) {
			assert(current_filter==task->filter);
		}
		current_filter = task->filter;

		//this is a crude way of scheduling the next task, we should
		//1- have a way to make sure we will not repost after a time-consuming task
		//2- have a way to wait for the given amount of time rather than just do a sema_wait/notify in loop
		if (task->schedule_next_time) {
			s64 now = gf_sys_clock_high_res();
			s64 diff = task->schedule_next_time;
			diff -= now;
			diff /= 1000;


			if (diff > 0) {
				GF_FSTask *next;
				s64 tdiff = diff;
				s64 ndiff = 0;

				//no filter, just reschedule the task
				if (!current_filter) {
#ifndef GPAC_DISABLE_LOG
					const char *task_log_name = task->log_name;
#endif
					next = gf_fq_head(fsess->tasks);
					next_task_schedule_time = task->schedule_next_time;
					assert(task->run_task);
#ifdef CHECK_TASK_LIST_INTEGRITY
					check_task_list(fsess->main_thread_tasks, task);
					check_task_list(fsess->tasks, task);
					check_task_list(fsess->tasks_reservoir, task);
#endif
					//tasks without filter are currently only posted to the secondary task list
					gf_fq_add(fsess->tasks, task);
					if (next) {
						if (next->schedule_next_time <= (u64) now) {
							GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u: task %s reposted, next task time ready for execution\n", sys_thid, task_log_name));

							skip_next_sema_wait = GF_TRUE;
							continue;
						}
						ndiff = next->schedule_next_time;
						ndiff -= now;
						ndiff /= 1000;
						if (ndiff<diff) {
							GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u: task %s scheduled after next task %s:%s (in %d ms vs %d ms)\n", sys_thid, task_log_name, next->log_name, next->filter ? next->filter->name : "", (s32) diff, (s32) ndiff));
							diff = ndiff;
						}
					}

					if (!do_regulate) {
						diff = 0;
					}

					if (diff && do_regulate) {
						if (diff > fsess->max_sleep)
							diff = fsess->max_sleep;
						if (th_count==0) {
							if ( gf_fq_count(fsess->tasks) > MONOTH_MIN_TASKS)
								diff = MONOTH_MIN_SLEEP;
						}
						GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u: task %s reposted, %s task scheduled after this task, sleeping for %d ms (task diff %d - next diff %d)\n", sys_thid, task_log_name, next ? "next" : "no", diff, tdiff, ndiff));
						gf_sleep((u32) diff);
					} else {
						GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u: task %s reposted, next task scheduled after this task, rerun\n", sys_thid, task_log_name));
					}
					skip_next_sema_wait = GF_TRUE;
					continue;
				}

				if (!task->filter->finalized) {
#ifdef CHECK_TASK_LIST_INTEGRITY
					next = gf_fq_head(current_filter->tasks);
					assert(next == task);
					check_task_list(fsess->main_thread_tasks, task);
					check_task_list(fsess->tasks_reservoir, task);
#endif

					//next in filter should be handled before this task, move task at the end of the filter task
					next = gf_fq_get(current_filter->tasks, 1);
					if (next && next->schedule_next_time < task->schedule_next_time) {
						if (task->notified) {
							assert(fsess->tasks_pending);
							safe_int_dec(&fsess->tasks_pending);
							task->notified = GF_FALSE;
						}
						GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u: task %s:%s reposted to filter task until task exec time is reached (%d us)\n", sys_thid, current_filter->name, task->log_name, (s32) (task->schedule_next_time - next->schedule_next_time) ));
						//remove task
						gf_fq_pop(current_filter->tasks);
						//and queue it after the next one
						gf_fq_add(current_filter->tasks, task);
						//and continue with the same filter
						continue;
					}
					//little optim here: if this is the main thread and we have other tasks pending
					//check the timing of tasks in the secondary list. If a task is present with smaller time than
					//the head of the main task, force a temporary swap to the secondary task list
					if (!thid && task->notified && diff>10) {
						next = gf_fq_head(fsess->tasks);
						if (next && !next->blocking) {
							u64 next_time_main = task->schedule_next_time;
							u64 next_time_secondary = next->schedule_next_time;
							//if we have several threads, also check the next task on the main task list
							// (different from secondary tasks in multithread case)
							if (th_count) {
								GF_FSTask *next_main = gf_fq_head(fsess->main_thread_tasks);
								if (next_main && (next_time_main > next_main->schedule_next_time))
									next_time_main = next_main->schedule_next_time;
							}
							
							if (next_time_secondary<next_time_main) {
								GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u: forcing secondary task list on main - current task schedule time "LLU" (diff to now %d) vs next time secondary "LLU" (%s::%s)\n", sys_thid, task->schedule_next_time, (s32) diff, next_time_secondary, next->filter->freg->name, next->log_name));
								diff = 0;
								force_secondary_tasks = GF_TRUE;
							}
						}
					}

					//move task to main list
					if (!task->notified) {
						task->notified = GF_TRUE;
						safe_int_inc(&fsess->tasks_pending);
					}

					sess_thread->active_time += gf_sys_clock_high_res() - active_start;

					if (next_task_schedule_time && (next_task_schedule_time <= task->schedule_next_time)) {
						tdiff = next_task_schedule_time;
						tdiff -= now;
						if (tdiff < 0) tdiff=0;
						if (tdiff<diff) {
							GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u: next task has earlier exec time than current task %s:%s, adjusting sleep (old %d - new %d)\n", sys_thid, current_filter->name, task->log_name, (s32) diff, (s32) tdiff));
							diff = tdiff;
						} else {
							GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u: next task has earlier exec time#2 than current task %s:%s, adjusting sleep (old %d - new %d)\n", sys_thid, current_filter->name, task->log_name, (s32) diff, (s32) tdiff));

						}
					}

					if (do_regulate && diff) {
						if (diff > fsess->max_sleep)
							diff = fsess->max_sleep;
						if (th_count==0) {
							if ( gf_fq_count(fsess->tasks) > MONOTH_MIN_TASKS)
								diff = MONOTH_MIN_SLEEP;
						}
						GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u: task %s:%s postponed for %d ms (scheduled time "LLU" us, next task schedule "LLU" us)\n", sys_thid, current_filter->name, task->log_name, (s32) diff, task->schedule_next_time, next_task_schedule_time));

						gf_sleep((u32) diff);
						active_start = gf_sys_clock_high_res();
					}
					diff = (s64)task->schedule_next_time;
					diff -= (s64) gf_sys_clock_high_res();
					if (diff > 100 ) {
						Bool use_main = (current_filter->freg->flags & GF_FS_REG_MAIN_THREAD) ? GF_TRUE : GF_FALSE;
						GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u: releasing current filter %s, exec time due in "LLD" us\n", sys_thid, current_filter->name, diff));
						current_filter->process_th_id = 0;
						current_filter->in_process = GF_FALSE;
						//don't touch the current filter tasks, just repost the task to the main/secondary list
						assert(gf_fq_count(current_filter->tasks));
						current_filter = NULL;

#ifdef CHECK_TASK_LIST_INTEGRITY
						check_task_list(fsess->main_thread_tasks, task);
						check_task_list(fsess->tasks, task);
						check_task_list(fsess->tasks_reservoir, task);
						assert(task->run_task);
#endif

						if (use_main) {
							gf_fq_add(fsess->main_thread_tasks, task);
							//we are the main thread and reposting to the main task list, don't notify/wait for the sema, just retry
							//we are sure to get a task from main list at next iteration
							if (use_main_sema) {
								skip_next_sema_wait = GF_TRUE;
							} else {
								gf_fs_sema_io(fsess, GF_TRUE, GF_TRUE);
							}
						} else {
							gf_fq_add(fsess->tasks, task);
							//we are not the main thread and we are reposting to the secondary task list, don't notify/wait for the sema, just retry
							//we are not sure to get a task from secondary list at next iteration, but the end of thread check will make
							//sure we renotify secondary sema if some tasks are still pending
							if (!use_main_sema) {
								skip_next_sema_wait = GF_TRUE;
							} else {
								gf_fs_sema_io(fsess, GF_TRUE, GF_FALSE);
							}
						}
						//we temporary force the main thread to fetch a task from the secondary list
						//because the first main task was not yet due for execution
						//it is likely that the execution of the next task will not wake up the main thread
						//but we must reevaluate the previous main task timing, so we force a notification of the main sema
						if (force_secondary_tasks)
							gf_fs_sema_io(fsess, GF_TRUE, GF_TRUE);

						continue;
					}
					force_secondary_tasks=GF_FALSE;
				}
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u: task %s:%s schedule time "LLU" us reached (diff %d ms)\n", sys_thid, current_filter ? current_filter->name : "", task->log_name, task->schedule_next_time, (s32) diff));

		}
		next_task_schedule_time = 0;

		if (current_filter) {
			current_filter->scheduled_for_next_task = GF_TRUE;
			assert(!current_filter->in_process);
			current_filter->in_process = GF_TRUE;
			current_filter->process_th_id = gf_th_id();
		}

		sess_thread->nb_tasks++;
		sess_thread->has_seen_eot = GF_FALSE;
		if (task->filter) {
			assert(gf_fq_count(task->filter->tasks));
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u task#%d %p executing Filter %s::%s (%d tasks pending, %d(%d) process task queued)\n", sys_thid, sess_thread->nb_tasks, task, task->filter ? task->filter->name : "none", task->log_name, fsess->tasks_pending, task->filter ? task->filter->process_task_queued : 0, task->filter ? gf_fq_count(task->filter->tasks) : 0));

		safe_int_inc(& fsess->tasks_in_process );
		assert( task->run_task );
		task_time = gf_sys_clock_high_res();

		task->can_swap = GF_FALSE;
		task->requeue_request = GF_FALSE;
		task->run_task(task);
		requeue = task->requeue_request;

		task_time = gf_sys_clock_high_res() - task_time;
		safe_int_dec(& fsess->tasks_in_process );

		//may now be NULL if task was a filter destruction task
		current_filter = task->filter;

#ifdef CHECK_TASK_LIST_INTEGRITY
		prev_current_filter = task->filter;
#endif

		//source task was current filter, pop the filter task list
		if (current_filter) {
			current_filter->nb_tasks_done++;
			current_filter->time_process += task_time;
			consecutive_filter_tasks++;

			gf_mx_p(current_filter->tasks_mx);
			//if last task
			if ( (gf_fq_count(current_filter->tasks)==1)
				//if requeue request and stream reset pending (we must exit the filter task loop for the reset task to pe processed)
				|| (requeue && current_filter->stream_reset_pending)
				//or requeue request and pid swap pending (we must exit the filter task loop for the swap task to pe processed)
				|| (requeue && (current_filter->swap_pidinst_src ||  current_filter->swap_pidinst_dst) )
				//or requeue request and pid detach / cap negotiate pending
				|| (requeue && (current_filter->out_pid_connection_pending || current_filter->detached_pid_inst || current_filter->caps_negociate) )

				//or requeue request and we have been running on that filter for more than 10 times, abort
				|| (requeue && (consecutive_filter_tasks>10))
			) {

				if (requeue) {
					//filter task can be pushed back the queue of tasks
					if (task->can_swap) {
						GF_FSTask *next_task;

						//drop task from filter task list
						gf_fq_pop(current_filter->tasks);

						next_task = gf_fq_head(current_filter->tasks);
						//if first task was notified, swap the flag
						if (next_task) {
							//see note in post_task_ex for caution about this !!
							next_task->notified = task->notified;
							task->notified = GF_FALSE;
						}
						//requeue task
						gf_fq_add(current_filter->tasks, task);

						//ans swap task for later requeing
						if (next_task) task = next_task;
					}
					//otherwise (can't swap) keep task first in the list

					//don't reset scheduled_for_next_task flag if requeued to make sure no other task posted from
					//another thread will post to main sched

#ifdef CHECK_TASK_LIST_INTEGRITY
					skip_filter_task_check = GF_TRUE;
#endif
				} else {
					//no requeue, filter no longer scheduled and drop task
					current_filter->scheduled_for_next_task = GF_FALSE;

					//drop task from filter task list
					gf_fq_pop(current_filter->tasks);
				}

				current_filter->process_th_id = 0;
				current_filter->in_process = GF_FALSE;

				//unlock once we modified in_process, otherwise this will make our assert fail
				gf_mx_v(current_filter->tasks_mx);
#ifdef CHECK_TASK_LIST_INTEGRITY
				if (requeue && !skip_filter_task_check) check_task_list(current_filter->tasks, task);
#endif
				current_filter = NULL;
			} else {
				//drop task from filter task list
				gf_fq_pop(current_filter->tasks);

				//not requeued, no more tasks, deactivate filter
				if (!requeue && !gf_fq_count(current_filter->tasks)) {
					current_filter->process_th_id = 0;
					current_filter->in_process = GF_FALSE;
					current_filter->scheduled_for_next_task = GF_FALSE;
					gf_mx_v(current_filter->tasks_mx);
					current_filter = NULL;
				} else {
#ifdef CHECK_TASK_LIST_INTEGRITY
					check_task_list(fsess->main_thread_tasks, task);
					check_task_list(fsess->tasks, task);
					check_task_list(fsess->tasks_reservoir, task);
#endif

					//requeue task in current filter
					if (requeue)
						gf_fq_add(current_filter->tasks, task);

					gf_mx_v(current_filter->tasks_mx);
				}
			}
		}
		//do not touch the filter task list after this, it has to be mutex protected to ensure proper posting of tasks

		notified = task->notified;
		if (requeue) {
			//if requeue on a filter active, use filter queue to avoid another thread grabing the task (we would have concurrent access to the filter)
			if (current_filter) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u re-posted task Filter %s::%s in filter tasks (%d pending)\n", sys_thid, task->filter->name, task->log_name, fsess->tasks_pending));
				task->notified = GF_FALSE;
				//keep this thread running on the current filter no signaling of semaphore
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u re-posted task Filter %s::%s in %s tasks (%d pending)\n", sys_thid, task->filter ? task->filter->name : "none", task->log_name, (task->filter && (task->filter->freg->flags & GF_FS_REG_MAIN_THREAD)) ? "main" : "secondary", fsess->tasks_pending));

				task->notified = GF_TRUE;
				safe_int_inc(&fsess->tasks_pending);

#ifdef CHECK_TASK_LIST_INTEGRITY
				check_task_list(fsess->main_thread_tasks, task);
				check_task_list(fsess->tasks, task);
				check_task_list(fsess->tasks_reservoir, task);
				if (prev_current_filter && !skip_filter_task_check) check_task_list(prev_current_filter->tasks, task);
#endif

				//main thread
				if (task->filter && (task->filter->freg->flags & GF_FS_REG_MAIN_THREAD)) {
					gf_fq_add(fsess->main_thread_tasks, task);
				} else {
					gf_fq_add(fsess->tasks, task);
				}
				gf_fs_sema_io(fsess, GF_TRUE, use_main_sema);
			}
		} else {
#ifdef CHECK_TASK_LIST_INTEGRITY
			check_task_list(fsess->main_thread_tasks, task);
			check_task_list(fsess->tasks, task);
			check_task_list(fsess->tasks_reservoir, task);
			if (prev_current_filter)
				check_task_list(prev_current_filter->tasks, task);

			{
				gf_mx_p(fsess->filters_mx);
				u32 k, c2 = gf_list_count(fsess->filters);
				for (k=0; k<c2; k++) {
					GF_Filter *af = gf_list_get(fsess->filters, k);
					check_task_list(af->tasks, task);
				}
				gf_mx_v(fsess->filters_mx);
			}
#endif
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u task#%d %p pushed to reservoir\n", sys_thid, sess_thread->nb_tasks, task));

			if (fsess->tasks_reservoir) {
				memset(task, 0, sizeof(GF_FSTask));
				gf_fq_add(fsess->tasks_reservoir, task);
			} else {
				gf_free(task);
			}
		}

		//decrement task counter
		if (notified) {
			assert(fsess->tasks_pending);
			safe_int_dec(&fsess->tasks_pending);
		}
		if (current_filter) {
			current_filter->process_th_id = 0;
			current_filter->in_process = GF_FALSE;
		}
		//not requeuing and first time we have an empty task queue, flush to detect if we are indeed done
		if (!current_filter && !fsess->tasks_pending && !sess_thread->has_seen_eot && !gf_fq_count(fsess->tasks)) {
			//if not the main thread, or if main thread and task list is empty, enter end of session probing mode
			if (thid || !gf_fq_count(fsess->main_thread_tasks) ) {
				//maybe last task, force a notify to check if we are truly done. We only tag "session done" for the non-main
				//threads, in order to enter the end-of session signaling above
				if (thid) sess_thread->has_seen_eot = GF_TRUE;
				gf_fs_sema_io(fsess, GF_TRUE, use_main_sema);
			}
		}

		sess_thread->active_time += gf_sys_clock_high_res() - active_start;


		//no main thread, return
		if (!thid && fsess->no_main_thread && !current_filter && !fsess->pid_connect_tasks_pending) {
			gf_rmt_end();
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Main thread proc exit\n"));
			return 0;
		}
	}

	gf_rmt_end();

	//no main thread, return
	if (!thid && fsess->no_main_thread) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Main thread proc exit\n"));
		return 0;
	}
	sess_thread->run_time = gf_sys_clock_high_res() - enter_time;

	safe_int_inc(&fsess->nb_threads_stopped);

	if (!fsess->run_status)
		fsess->run_status = GF_EOS;

	// thread exit, notify the semaphores
	if (fsess->semaphore_main && ! gf_sema_notify(fsess->semaphore_main, 1)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCHEDULER, ("Failed to notify main semaphore, might hang up !!\n"));
	}
	if (fsess->semaphore_other && ! gf_sema_notify(fsess->semaphore_other, th_count)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCHEDULER, ("Failed to notify secondary semaphore, might hang up !!\n"));
	}

	return 0;
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
		gf_sleep(1);
	}

	return fsess->run_status;
}

void gf_fs_run_step(GF_FilterSession *fsess)
{
	gf_fs_thread_proc(&fsess->main_th);
}

GF_EXPORT
GF_Err gf_fs_abort(GF_FilterSession *fsess, Bool do_flush)
{
	u32 i, count;
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Session abort from user, stoping sources\n"));
	if (!fsess) return GF_BAD_PARAM;

	if (!do_flush) {
		fsess->in_final_flush = GF_TRUE;
		fsess->run_status = GF_EOS;
		return GF_OK;
	}

	gf_mx_p(fsess->filters_mx);
	count = gf_list_count(fsess->filters);
	//disable all sources
	for (i=0; i<count; i++) {
		GF_Filter *filter = gf_list_get(fsess->filters, i);
		//force end of session on all sources, and on all filters connected to these sources, and dispatch end of stream on all outputs pids of these filters
		//if we don't propagate on connected filters, we take the risk of not deactivating demuxers working from file
		//(eg ignoring input packets)
		if (!filter->num_input_pids) {
			u32 j, k, l;
			filter->disabled = GF_TRUE;
			for (j=0; j<filter->num_output_pids; j++) {
				GF_FilterPid *pid = gf_list_get(filter->output_pids, j);
				gf_filter_pid_set_eos(pid);
				for (k=0; k<pid->num_destinations; k++) {
					GF_FilterPidInst *pidi = gf_list_get(pid->destinations, k);
					pidi->filter->disabled = GF_TRUE;
					for (l=0; l<pidi->filter->num_output_pids; l++) {
						GF_FilterPid *opid = gf_list_get(pidi->filter->output_pids, l);
						if (opid->filter->freg->process_event) {
							GF_FilterEvent evt;
							GF_FEVT_INIT(evt, GF_FEVT_STOP, opid);
							opid->filter->freg->process_event(opid->filter, &evt);
						}
						gf_filter_pid_set_eos(opid);
					}
				}
			}
		}
	}
	fsess->in_final_flush = GF_TRUE;

	gf_mx_v(fsess->filters_mx);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_fs_stop(GF_FilterSession *fsess)
{
	u32 i, count = fsess->threads ? gf_list_count(fsess->threads) : 0;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Session stop\n"));
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
		if (gf_fq_count(fsess->main_thread_tasks))
			continue;

		if (count && (count == fsess->nb_threads_stopped) && gf_fq_count(fsess->tasks) ) {
			continue;
		}
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

static GFINLINE void print_filter_name(GF_Filter *f, Bool skip_id, Bool skip_args)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s", f->freg->name));
	if (strcmp(f->name, f->freg->name)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" \"%s\"", f->name));
	}
	if (!skip_id && f->id) GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" ID %s", f->id));
	if (f->dynamic_filter || skip_args) return;

	if (!f->src_args && !f->orig_args && !f->dst_args && !f->dynamic_source_ids) return;
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" ("));
	if (f->src_args) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s", f->src_args));
	}
	else if (f->orig_args) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s", f->orig_args));
	}
	else if (f->dst_args) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s", f->dst_args));
	}

	if (f->dynamic_source_ids) GF_LOG(GF_LOG_INFO, GF_LOG_APP, (",resolved SID:%s", f->source_ids));
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, (")"));
}

GF_EXPORT
void gf_fs_print_stats(GF_FilterSession *fsess)
{
	u64 run_time=0, active_time=0, nb_tasks=0, nb_filters=0;
	u32 i, count;

	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\n"));
	if (fsess->filters_mx) gf_mx_p(fsess->filters_mx);

	count=gf_list_count(fsess->filters);
	for (i=0; i<count; i++) {
		GF_Filter *f = gf_list_get(fsess->filters, i);
		if (f->multi_sink_target) continue;
		nb_filters++;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Filter stats - %d filters\n", nb_filters));
	for (i=0; i<count; i++) {
		u32 k, ipids, opids;
		GF_Filter *f = gf_list_get(fsess->filters, i);
		if (f->multi_sink_target) continue;

		ipids = f->num_input_pids;
		opids = f->num_output_pids;
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\tFilter "));
		print_filter_name(f, GF_FALSE, GF_FALSE);
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" : %d input pids %d output pids "LLU" tasks "LLU" us process time\n", ipids, opids, f->nb_tasks_done, f->time_process));

		if (ipids) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\t\t"LLU" packets processed "LLU" bytes processed", f->nb_pck_processed, f->nb_bytes_processed));
			if (f->time_process) {
				GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" (%g pck/sec %g mbps)", (Double) f->nb_pck_processed*1000000/f->time_process, (Double) f->nb_bytes_processed*8/f->time_process));
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\n"));
		}
		if (opids) {
			if (f->nb_hw_pck_sent) {
				GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\t\t"LLU" hardware frames sent", f->nb_hw_pck_sent));
				if (f->time_process) {
					GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" (%g pck/sec)", (Double) f->nb_hw_pck_sent*1000000/f->time_process));
				}

			} else if (f->nb_pck_sent) {
				GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\t\t"LLU" packets sent "LLU" bytes sent", f->nb_pck_sent, f->nb_bytes_sent));
				if (f->time_process) {
					GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" (%g pck/sec %g mbps)", (Double) f->nb_pck_sent*1000000/f->time_process, (Double) f->nb_bytes_sent*8/f->time_process));
				}
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\n"));
		}

		for (k=0; k<ipids; k++) {
			GF_FilterPidInst *pid = gf_list_get(f->input_pids, k);
			if (!pid->pid) continue;
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\t\t* input PID %s: %d packets received\n", pid->pid->name, pid->pid->nb_pck_sent));
		}
#ifndef GPAC_DISABLE_LOG
		for (k=0; k<opids; k++) {
			GF_FilterPid *pid = gf_list_get(f->output_pids, k);
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\t\t* output PID %s: %d packets sent\n", pid->name, pid->nb_pck_sent));
		}
		if (f->nb_errors) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\t\t%d errors while processing\n", f->nb_errors));
		}
#endif

	}
	if (fsess->filters_mx) gf_mx_v(fsess->filters_mx);

	count=gf_list_count(fsess->threads);
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Session stats - threads %d\n", 1+count));

	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\tThread %u: run_time "LLU" us active_time "LLU" us nb_tasks "LLU"\n", 1, fsess->main_th.run_time, fsess->main_th.active_time, fsess->main_th.nb_tasks));

	run_time+=fsess->main_th.run_time;
	active_time+=fsess->main_th.active_time;
	nb_tasks+=fsess->main_th.nb_tasks;

	for (i=0; i<count; i++) {
		GF_SessionThread *s = gf_list_get(fsess->threads, i);

		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\tThread %u: run_time "LLU" us active_time "LLU" us nb_tasks "LLU"\n", i+2, s->run_time, s->active_time, s->nb_tasks));

		run_time+=s->run_time;
		active_time+=s->active_time;
		nb_tasks+=s->nb_tasks;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\nTotal: run_time "LLU" us active_time "LLU" us nb_tasks "LLU"\n", run_time, active_time, nb_tasks));
}

static void gf_fs_print_filter_outputs(GF_Filter *f, GF_List *filters_done, u32 indent, GF_FilterPid *pid, GF_Filter *alias_for)
{
	u32 i=0;

	while (i<indent) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("-"));
		i++;
	}

	if (pid) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("(PID %s) ", pid->name));
	}
	print_filter_name(f, GF_TRUE, GF_FALSE);
	if (f->id) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" (ID=%s)\n", f->id));
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" (ptr=%p)\n", f));
	}
	if (gf_list_find(filters_done, f)>=0)
		return;

	gf_list_add(filters_done, f);
	if (alias_for) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" (<=> "));
		print_filter_name(alias_for, GF_TRUE, GF_TRUE);
		if (alias_for->id) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" ID=%s", alias_for->id));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" ptr=%p", alias_for));
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, (")\n"));
	}

	for (i=0; i<f->num_output_pids; i++) {
		u32 j, k;
		GF_FilterPid *pidout = gf_list_get(f->output_pids, i);
		for (j=0; j<pidout->num_destinations; j++) {
			GF_FilterPidInst *pidi = gf_list_get(pidout->destinations, j);
			GF_Filter *alias = NULL;
			for (k=0; k<gf_list_count(f->destination_filters); k++) {
				alias = gf_list_get(f->destination_filters, k);
				if (alias->multi_sink_target == pidi->filter)
					break;
				alias = NULL;
			}
			if (alias) {

				gf_fs_print_filter_outputs(alias, filters_done, indent+1, pidout, pidi->filter);
			} else {
				gf_fs_print_filter_outputs(pidi->filter, filters_done, indent+1, pidout, NULL);
			}
		}
	}

}
GF_EXPORT
void gf_fs_print_connections(GF_FilterSession *fsess)
{
	u32 i, count;
	Bool has_undefined=GF_FALSE;
	Bool has_connected=GF_FALSE;
	Bool has_unconnected=GF_FALSE;
	GF_List *filters_done;
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\n"));
	if (fsess->filters_mx) gf_mx_p(fsess->filters_mx);

	filters_done = gf_list_new();

	count=gf_list_count(fsess->filters);
	for (i=0; i<count; i++) {
		GF_Filter *f = gf_list_get(fsess->filters, i);
		//only dump sources
		if (f->num_input_pids) continue;
		if (!f->num_output_pids) continue;
		if (!has_connected) {
			has_connected = GF_TRUE;
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Filters connected:\n"));
		}
		gf_fs_print_filter_outputs(f, filters_done, 0, NULL, NULL);
	}
	for (i=0; i<count; i++) {
		GF_Filter *f = gf_list_get(fsess->filters, i);
		//only dump not connected ones
		if (f->num_input_pids || f->num_output_pids || f->multi_sink_target) continue;
		if (!has_unconnected) {
			has_unconnected = GF_TRUE;
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Filters not connected:\n"));
		}
		gf_fs_print_filter_outputs(f, filters_done, 0, NULL, NULL);
	}
	for (i=0; i<count; i++) {
		GF_Filter *f = gf_list_get(fsess->filters, i);
		if (f->multi_sink_target) continue;
		if (gf_list_find(filters_done, f)>=0) continue;
		if (!has_undefined) {
			has_undefined = GF_TRUE;
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Filters in unknown connection state:\n"));
		}
		gf_fs_print_filter_outputs(f, filters_done, 0, NULL, NULL);
	}

	if (fsess->filters_mx) gf_mx_v(fsess->filters_mx);
	gf_list_del(filters_done);
}


GF_EXPORT
void gf_fs_send_update(GF_FilterSession *fsess, const char *fid, GF_Filter *filter, const char *name, const char *val, GF_EventPropagateType propagate_mask)
{
	GF_FilterUpdate *upd;
	u32 i, count;
	Bool removed = GF_FALSE;
	if ((!fid && !filter) || !name) return;

	if (fsess->filters_mx) gf_mx_p(fsess->filters_mx);

	if (!filter) {
		GF_Filter *reg_filter = NULL;
		count = gf_list_count(fsess->filters);
		for (i=0; i<count; i++) {
			filter = gf_list_get(fsess->filters, i);
			if (filter->id && !strcmp(filter->id, fid)) {
				break;
			}
			if (filter->name && !strcmp(filter->name, fid)) {
				break;
			}
			if (!reg_filter && !strcmp(filter->freg->name, fid))
				reg_filter = filter;
			filter = NULL;
		}
		if (!filter)
			filter = reg_filter;
	}

	if (filter && filter->multi_sink_target)
		filter = filter->multi_sink_target;

	removed = (!filter || filter->removed || filter->finalized) ? GF_TRUE : GF_FALSE;
	if (fsess->filters_mx) gf_mx_v(fsess->filters_mx);

	if (removed) return;

	GF_SAFEALLOC(upd, GF_FilterUpdate);
	if (!val) {
		char *sep = strchr(name, fsess->sep_name);
		if (sep) sep[0] = 0;
		upd->name = gf_strdup(name);
		upd->val = sep ? gf_strdup(sep+1) : NULL;
		if (sep) sep[0] = fsess->sep_name;
	} else {
		upd->name = gf_strdup(name);
		upd->val = gf_strdup(val);
	}
	upd->recursive = propagate_mask;
	gf_fs_post_task(fsess, gf_filter_update_arg_task, filter, NULL, "update_arg", upd);
}

static GF_FilterProbeScore probe_meta_check_builtin_format(GF_FilterSession *fsess, GF_FilterRegister *freg, const char *url, const char *mime, char *fargs)
{
	char szExt[100];
	const char *ext = gf_file_ext_start(url);
	u32 len=0, i, j, count = gf_list_count(fsess->registry);
	if (ext) {
		ext++;
		len = (u32) strlen(ext);
	}
	//check in filter args if we have a format set, in which case replace URL ext by the given format
	if (fargs) {
		char szExtN[10];
		char *ext_arg;
		sprintf(szExtN, "ext%c", fsess->sep_name);
		ext_arg = strstr(fargs, szExtN);
		if (ext_arg) {
			char *next_arg;
			ext_arg+=4;
			next_arg = strchr(ext_arg, fsess->sep_args);
			if (next_arg) {
				len = (u32) (next_arg - ext_arg);
			} else {
				len = (u32) strlen(ext_arg);
			}
			if (len>99) len=99;
			strncpy(szExt, ext_arg, len);
			szExt[len] = 0;
			ext = szExt;
		}
	}

	if (mime) {
		if (strstr(mime, "/mp4")) return GF_FPROBE_MAYBE_SUPPORTED;
	}

	for (i=0; i<count; i++) {
		const GF_FilterArgs *dst_arg=NULL;
		GF_FilterRegister *reg = gf_list_get(fsess->registry, i);
		if (reg==freg) continue;
		if (reg->flags & GF_FS_REG_META) continue;

		j=0;
		while (reg->args) {
			dst_arg = &reg->args[j];
			if (!dst_arg || !dst_arg->arg_name) {
				dst_arg=NULL;
				break;
			}
			if (!strcmp(dst_arg->arg_name, "dst")) break;
			dst_arg = NULL;
			j++;
		}
		/*check prober*/
		if (dst_arg) {
			if (reg->probe_url) {
				GF_FilterProbeScore s = reg->probe_url(url, mime);
				if (s==GF_FPROBE_SUPPORTED)
					return GF_FPROBE_MAYBE_SUPPORTED;
			}
			continue;
		}

		/* check muxers*/
		for (j=0; j<reg->nb_caps; j++) {
			char *value=NULL;
			const char *pattern = NULL;
			const GF_FilterCapability *cap = &reg->caps[j];
			if (! (cap->flags & GF_CAPFLAG_OUTPUT) )
				continue;
			if (cap->flags & GF_CAPFLAG_EXCLUDED)
				continue;

			if (cap->code==GF_PROP_PID_FILE_EXT) {
				if (ext) {
					value = cap->val.value.string;
					pattern = ext;
				}
			} else if (cap->code==GF_PROP_PID_MIME) {
				if (mime) {
					value = cap->val.value.string;
					pattern = mime;
				}
			}
			while (value) {
				char *match = strstr(value, pattern);
				if (!match) break;
				if (!match[len] || match[len]=='|')
					return GF_FPROBE_MAYBE_SUPPORTED;
				value = match+1;
			}
		}
	}
	return GF_FPROBE_SUPPORTED;
}


GF_Filter *gf_fs_load_source_dest_internal(GF_FilterSession *fsess, const char *url, const char *user_args, const char *parent_url, GF_Err *err, GF_Filter *filter, GF_Filter *dst_filter, Bool for_source, Bool no_args_inherit, Bool *probe_only)
{
	GF_FilterProbeScore score = GF_FPROBE_NOT_SUPPORTED;
	GF_FilterRegister *candidate_freg=NULL;
	GF_Filter *alias_for_filter = NULL;
	const GF_FilterArgs *src_dst_arg=NULL;
	u32 i, count, user_args_len, arg_type;
	char szForceReg[20];
	char szMime[50];
	GF_Err e;
	const char *force_freg = NULL;
	char *sURL, *mime_type, *args, *sep;
	char szExt[50];
	Bool free_url=GF_FALSE;
	memset(szExt, 0, sizeof(szExt));

	if (err) *err = GF_OK;

	mime_type = NULL;
	//destination, extract mime from arguments
	if (!for_source) {
		sprintf(szMime, "%cmime=", fsess->sep_args);
		mime_type = strstr(url, szMime);
		if (!mime_type && user_args)
			mime_type = strstr(user_args, szMime);

		if (mime_type) {
			strncpy(szMime, mime_type+6, 49);
			szMime[49]=0;
			sep = strchr(szMime, fsess->sep_args);
			if (sep) sep[0] = 0;
			mime_type = szMime;
		}
	}
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
		free_url=GF_TRUE;

		if (!strncmp(sURL, "gpac://", 7)) {
			u32 ulen = (u32) strlen(sURL+7);
			memmove(sURL, sURL+7, ulen);
			sURL[ulen]=0;
		}

		if (for_source && gf_url_is_local(sURL)) {
			char *frag_par, *cgi, *ext_start;
			char f_c=0;
			gf_url_to_fs_path(sURL);
			sep = (char *)gf_fs_path_escape_colon(fsess, sURL);
			if (sep) sep[0] = 0;

			ext_start = gf_file_ext_start(sURL);
			if (!ext_start) ext_start = sURL;
			frag_par = strchr(ext_start, '#');
			cgi = strchr(ext_start, '?');
			if (frag_par && cgi && (cgi<frag_par))
				frag_par = cgi;

			if (frag_par) {
				f_c = frag_par[0];
				frag_par[0] = 0;
			}

			if (strcmp(sURL, "null") && strcmp(sURL, "-") && strcmp(sURL, "stdin") && ! gf_file_exists(sURL)) {
				if (sep) sep[0] = fsess->sep_args;
				if (frag_par) frag_par[0] = f_c;

				if (err) *err = GF_URL_ERROR;
				gf_free(sURL);
				return NULL;
			}
			if (frag_par) frag_par[0] = f_c;
			if (sep) sep[0] = fsess->sep_args;
		}
	}
	sep = (char *)gf_fs_path_escape_colon(fsess, sURL);

	sprintf(szForceReg, "gfreg%c", fsess->sep_name);
	force_freg = NULL;
	if (sep) {
		sep[0] = 0;
		force_freg = strstr(sep+1, szForceReg);
	}
	if (!force_freg && user_args) {
		force_freg = strstr(user_args, szForceReg);
	}
	if (force_freg)
		force_freg += 6;

restart:
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
			src_dst_arg = &freg->args[j];
			if (!src_dst_arg || !src_dst_arg->arg_name) {
				src_dst_arg=NULL;
				break;
			}
			if (for_source && !strcmp(src_dst_arg->arg_name, "src")) break;
			else if (!for_source && !strcmp(src_dst_arg->arg_name, "dst")) break;
			src_dst_arg = NULL;
			j++;
		}
		if (!src_dst_arg)
			continue;

		s = freg->probe_url(sURL, mime_type);
		/* destination meta filter: change GF_FPROBE_SUPPORTED to GF_FPROBE_MAYBE_SUPPORTED for internal mux formats
		in order to avoid always giving the hand to the meta filter*/
		if (!for_source && (s == GF_FPROBE_SUPPORTED) && (freg->flags & GF_FS_REG_META)) {
			s = probe_meta_check_builtin_format(fsess, freg, sURL, mime_type, sep ? sep+1 : NULL);
		}
		//higher score, use this new registry
		if (s > score) {
			candidate_freg = freg;
			score = s;
		}
		//same score and higher priority (0 being highest), use this new registry
		else if ((s == score) && candidate_freg && (freg->priority<candidate_freg->priority) ) {
			candidate_freg = freg;
			score = s;
		}
	}
	if (probe_only) {
		*probe_only = candidate_freg ? GF_TRUE : GF_FALSE;
		if (free_url)
			gf_free(sURL);
		if (err) *err = GF_OK;
		return NULL;
	}

	if (!candidate_freg) {
		if (force_freg) {
			GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("No source filter named %s found, retrying without forcing registry\n", force_freg));
			force_freg = NULL;
			goto restart;
		}
		if (free_url)
			gf_free(sURL);
		if (err) *err = GF_NOT_SUPPORTED;
		if (filter) filter->finalized = GF_TRUE;
		return NULL;
	}
	if (sep) sep[0] = fsess->sep_args;

	user_args_len = user_args ? (u32) strlen(user_args) : 0;
	args = gf_malloc(sizeof(char)*(5+strlen(sURL) + (user_args_len ? user_args_len + 8/*for potential :gpac: */  :0) ) );

	sprintf(args, "%s%c", for_source ? "src" : "dst", fsess->sep_name);
	strcat(args, sURL);
	if (user_args_len) {
		if (fsess->sep_args==':') strcat(args, ":gpac:");
		else {
			char szSep[2];
			szSep[0] = fsess->sep_args;
			szSep[1] = 0;
			strcat(args, szSep);
		}
		strcat(args, user_args);
	}

	e = GF_OK;
	arg_type = GF_FILTER_ARG_EXPLICIT_SINK;
	if (for_source) {
		if (no_args_inherit) arg_type = GF_FILTER_ARG_EXPLICIT_SOURCE_NO_DST_INHERIT;
		else arg_type = GF_FILTER_ARG_EXPLICIT_SOURCE;
	}

	if (!for_source && candidate_freg->use_alias) {
		u32 fcount = gf_list_count(fsess->filters);
		for (i=0; i<fcount; i++) {
			GF_Filter *f = gf_list_get(fsess->filters, i);
			if (f->freg != candidate_freg) continue;
			if (f->freg->use_alias(f, sURL, mime_type)) {
				alias_for_filter = f;
				break;
			}
		}
	}

	if (!filter) {
		filter = gf_filter_new(fsess, candidate_freg, args, NULL, arg_type, err, alias_for_filter, GF_FALSE);
	} else {
		filter->freg = candidate_freg;
		e = gf_filter_new_finalize(filter, args, arg_type);
		if (err) *err = e;
	}

	if (free_url)
		gf_free(sURL);

	if (filter) {
		if (filter->src_args) gf_free(filter->src_args);
		filter->src_args = args;
		//for link resolution
		if (dst_filter && for_source)	{
			if (gf_list_find(filter->destination_links, dst_filter)<0)
				gf_list_add(filter->destination_links, dst_filter);
			//to remember our connection target
			filter->target_filter = dst_filter;
		}
	} else {
		gf_free(args);
	}

	if (!e && filter && !filter->num_output_pids && for_source)
		gf_filter_post_process_task(filter);

	return filter;
}

GF_EXPORT
GF_Filter *gf_fs_load_source(GF_FilterSession *fsess, const char *url, const char *args, const char *parent_url, GF_Err *err)
{
	return gf_fs_load_source_dest_internal(fsess, url, args, parent_url, err, NULL, NULL, GF_TRUE, GF_FALSE, NULL);
}

GF_EXPORT
GF_Filter *gf_fs_load_destination(GF_FilterSession *fsess, const char *url, const char *args, const char *parent_url, GF_Err *err)
{
	return gf_fs_load_source_dest_internal(fsess, url, args, parent_url, err, NULL, NULL, GF_FALSE, GF_FALSE, NULL);
}


GF_EXPORT
GF_Err gf_filter_add_event_listener(GF_Filter *filter, GF_FSEventListener *el)
{
	GF_Err e;
	if (!filter || !filter->session || !el || !el->on_event) return GF_BAD_PARAM;
	while (filter->session->in_event_listener) gf_sleep(1);
	gf_mx_p(filter->session->evt_mx);
	if (!filter->session->event_listeners) {
		filter->session->event_listeners = gf_list_new();
	}
	e = gf_list_add(filter->session->event_listeners, el);
	gf_mx_v(filter->session->evt_mx);
	return e;
}

GF_EXPORT
GF_Err gf_filter_remove_event_listener(GF_Filter *filter, GF_FSEventListener *el)
{
	if (!filter || !filter->session || !el || !filter->session->event_listeners) return GF_BAD_PARAM;

	while (filter->session->in_event_listener) gf_sleep(1);
	gf_mx_p(filter->session->evt_mx);
	gf_list_del_item(filter->session->event_listeners, el);
	if (!gf_list_count(filter->session->event_listeners)) {
		gf_list_del(filter->session->event_listeners);
		filter->session->event_listeners=NULL;
	}
	gf_mx_v(filter->session->evt_mx);
	return GF_OK;
}

GF_EXPORT
Bool gf_filter_forward_gf_event(GF_Filter *filter, GF_Event *evt, Bool consumed, Bool skip_user)
{
	if (!filter || !filter->session || filter->session->in_final_flush) return GF_FALSE;

	if (filter->session->event_listeners) {
		GF_FSEventListener *el;
		u32 i=0;

		gf_mx_p(filter->session->evt_mx);
		filter->session->in_event_listener ++;
		gf_mx_v(filter->session->evt_mx);
		while ((el = gf_list_enum(filter->session->event_listeners, &i))) {
			if (el->on_event(el->udta, evt, consumed)) {
				filter->session->in_event_listener --;
				return GF_TRUE;
			}
		}
		filter->session->in_event_listener --;
	}

	if (!skip_user && !consumed && filter->session->ui_event_proc) {
		Bool res;
//		term->nb_calls_in_event_proc++;
		res = gf_fs_ui_event(filter->session, evt);
//		term->nb_calls_in_event_proc--;
		return res;
	}
	return GF_FALSE;
}

GF_EXPORT
Bool gf_filter_send_gf_event(GF_Filter *filter, GF_Event *evt)
{
	return gf_filter_forward_gf_event(filter, evt, GF_FALSE, GF_FALSE);
}


static void gf_fs_print_jsf_connection(GF_FilterSession *session, char *filter_name, void (*print_fn)(FILE *output, GF_SysPrintArgFlags flags, const char *fmt, ...) )
{
	GF_CapsBundleStore capstore;
	GF_Filter *js_filter;
	const char *js_name = NULL;
	GF_Err e=GF_OK;
	u32 i, j, count, nb_js_caps;
	GF_List *sources, *sinks;
	GF_FilterRegister loaded_freg;
	Bool has_output, has_input;

	js_filter = gf_fs_load_filter(session, filter_name, &e);
	if (!js_filter) return;

	js_name = strrchr(filter_name, '/');
	if (!js_name) js_name = strrchr(filter_name, '\\');
	if (js_name) js_name++;
	else js_name = filter_name;

	nb_js_caps = gf_filter_caps_bundle_count(js_filter->forced_caps, js_filter->nb_forced_caps);

	//fake a new register with only the caps set
	memset(&loaded_freg, 0, sizeof(GF_FilterRegister));
	loaded_freg.caps = js_filter->forced_caps;
	loaded_freg.nb_caps = js_filter->nb_forced_caps;

	has_output = gf_filter_has_out_caps(js_filter->forced_caps, js_filter->nb_forced_caps);
	has_input = gf_filter_has_in_caps(js_filter->forced_caps, js_filter->nb_forced_caps);

	memset(&capstore, 0, sizeof(GF_CapsBundleStore));
	sources = gf_list_new();
	sinks = gf_list_new();
	//edges for JS are for the unloaded JSF (eg accept anything, output anything).
	//we need to do a manual check
	count = gf_list_count(session->links);
	for (i=0; i<count; i++) {
		u32 nb_src_caps, k, l;
		Bool src_match = GF_FALSE;
		Bool sink_match = GF_FALSE;
		GF_FilterRegDesc *a_reg = gf_list_get(session->links, i);
		if (a_reg->freg == js_filter->freg) continue;

		//check which cap of this filter matches our destination
		nb_src_caps = gf_filter_caps_bundle_count(a_reg->freg->caps, a_reg->freg->nb_caps);
		for (k=0; k<nb_src_caps; k++) {
			for (l=0; l<nb_js_caps; l++) {
				s32 bundle_idx;
				u32 loaded_filter_only_flags = 0;
				u32 path_weight;
				if (has_input && !src_match) {
					path_weight = gf_filter_caps_to_caps_match(a_reg->freg, k, (const GF_FilterRegister *) &loaded_freg, NULL, &bundle_idx, l, &loaded_filter_only_flags, &capstore);
					if (path_weight && (bundle_idx == l))
						src_match = GF_TRUE;
				}
				if (has_output && !sink_match) {
					loaded_filter_only_flags = 0;
					path_weight = gf_filter_caps_to_caps_match(&loaded_freg, l, a_reg->freg, NULL, &bundle_idx, k, &loaded_filter_only_flags, &capstore);

					if (path_weight && (bundle_idx == k))
						sink_match = GF_TRUE;
				}
			}
			if (src_match && sink_match)
				break;
		}
		if (src_match) gf_list_add(sources, (void *) a_reg->freg);
		if (sink_match) gf_list_add(sinks, (void *) a_reg->freg);
	}

	for (i=0; i<2; i++) {
		GF_List *from = i ? sinks : sources;
		char *type = i ? "sources" : "sinks";

		count = gf_list_count(from);
		if (!count) {
			if (print_fn)
				print_fn(stderr, 1, "%s: no %s\n", js_name, type);
			else {
				GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s: no %s\n", type));
			}
			continue;
		}

		if (print_fn)
			print_fn(stderr, 1, "%s %s:", js_name, type);
		else {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s %s:", js_name, type));
		}
		for (j=0; j<count; j++) {
			GF_FilterRegister *a_reg = gf_list_get(from, j);
			if (print_fn)
				print_fn(stderr, 0, " %s", a_reg->name);
			else {
				GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" %s", a_reg->name));
			}
		}
		if (print_fn)
			print_fn(stderr, 0, "\n");
		else {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\n"));
		}
	}

	if (capstore.bundles_cap_found) gf_free(capstore.bundles_cap_found);
	if (capstore.bundles_in_ok) gf_free(capstore.bundles_in_ok);
	if (capstore.bundles_in_scores) gf_free(capstore.bundles_in_scores);
	gf_list_del(sources);
	gf_list_del(sinks);
}

GF_EXPORT
void gf_fs_print_all_connections(GF_FilterSession *session, char *filter_name, void (*print_fn)(FILE *output, GF_SysPrintArgFlags flags, const char *fmt, ...) )
{
	Bool found = GF_FALSE;
	GF_List *done;
	u32 i, j, count;
	u32 llev = gf_log_get_tool_level(GF_LOG_FILTER);

	gf_log_set_tool_level(GF_LOG_FILTER, GF_LOG_INFO);
	//load JS to inspect its connections
	if (filter_name && strstr(filter_name, ".js")) {
		gf_fs_print_jsf_connection(session, filter_name, print_fn);
		gf_log_set_tool_level(GF_LOG_FILTER, llev);
		return;
	}
	done = gf_list_new();
	count = gf_list_count(session->links);

	for (i=0; i<count; i++) {
		const GF_FilterRegDesc *src = gf_list_get(session->links, i);
		if (filter_name && strcmp(src->freg->name, filter_name))
			continue;

		if (!src->nb_edges) {
			if (print_fn)
				print_fn(stderr, 1, "%s: no sources\n", src->freg->name);
			else {
				GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s: no sources\n", src->freg->name));
			}
			continue;
		}
		found = GF_TRUE;
		if (print_fn)
			print_fn(stderr, 1, "%s sources:", src->freg->name);
		else {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s sources:", src->freg->name));
		}

		for (j=0; j<src->nb_edges; j++) {
			if (gf_list_find(done, (void *) src->edges[j].src_reg->freg->name)<0) {
				if (print_fn)
					print_fn(stderr, 0, " %s", src->edges[j].src_reg->freg->name);
				else {
					GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" %s", src->edges[j].src_reg->freg->name));
				}
				gf_list_add(done, (void *) src->edges[j].src_reg->freg->name);
			}
		}
		if (print_fn)
			print_fn(stderr, 0, "\n");
		else {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\n"));
		}
		gf_list_reset(done);
	}

	if (found && filter_name) {
		if (print_fn)
			print_fn(stderr, 1, "%s sinks:", filter_name);
		else {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s sinks:", filter_name));
		}
		count = gf_list_count(session->links);
		for (i=0; i<count; i++) {
			const GF_FilterRegDesc *src = gf_list_get(session->links, i);
			if (!strcmp(src->freg->name, filter_name)) continue;

			for (j=0; j<src->nb_edges; j++) {
				if (strcmp(src->edges[j].src_reg->freg->name, filter_name)) continue;

				if (gf_list_find(done, (void *) src->freg->name)<0) {
					if (print_fn)
						print_fn(stderr, 0, " %s", src->freg->name);
					else {
						GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" %s", src->freg->name));
					}
					gf_list_add(done, (void *) src->freg->name);
				}
			}
			gf_list_reset(done);
		}
		if (print_fn)
			print_fn(stderr, 1, " \n");
		else {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" \n"));
		}
	}

	if (!found && filter_name) {
		if (print_fn)
			print_fn(stderr, 1, "%s filter not found\n", filter_name);
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("%s filter not found\n", filter_name));
		}
	}
	gf_list_del(done);
	gf_log_set_tool_level(GF_LOG_FILTER, llev);
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
void gf_filter_set_session_caps(GF_Filter *filter, GF_FilterSessionCaps *caps)
{
	if (caps && filter) {
		filter->session->caps = (*caps);
		//TODO fire event
	}
}

GF_EXPORT
u8 gf_filter_get_sep(GF_Filter *filter, GF_FilterSessionSepType sep_type)
{
	switch (sep_type) {
	case GF_FS_SEP_ARGS: return filter->session->sep_args;
	case GF_FS_SEP_NAME: return filter->session->sep_name;
	case GF_FS_SEP_FRAG: return filter->session->sep_frag;
	case GF_FS_SEP_LIST: return filter->session->sep_list;
	case GF_FS_SEP_NEG: return filter->session->sep_neg;
	default: return 0;
	}
}

GF_EXPORT
GF_DownloadManager *gf_filter_get_download_manager(GF_Filter *filter)
{
	GF_FilterSession *fsess;
	if (!filter) return NULL;
	fsess = filter->session;

	if (!fsess->download_manager) {
		fsess->download_manager = gf_dm_new(fsess);
	}
	return fsess->download_manager;
}

GF_EXPORT
struct _gf_ft_mgr *gf_filter_get_font_manager(GF_Filter *filter)
{
#ifdef GPAC_DISABLE_PLAYER
	return NULL;
#else
	GF_FilterSession *fsess;
	if (!filter) return NULL;
	fsess = filter->session;

	if (!fsess->font_manager) {
		fsess->font_manager = gf_font_manager_new();
	}
	return fsess->font_manager;
#endif
}

void gf_fs_cleanup_filters(GF_FilterSession *fsess)
{
	assert(fsess->pid_connect_tasks_pending);
	safe_int_dec(&fsess->pid_connect_tasks_pending);
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

typedef struct
{
	GF_FilterSession *fsess;
	void *callback;
	Bool (*task_execute) (GF_FilterSession *fsess, void *callback, u32 *reschedule_ms);
	Bool (*task_execute_filter) (GF_Filter *filter, void *callback, u32 *reschedule_ms);
#ifndef GPAC_DISABLE_REMOTERY
	rmtU32 rmt_hash;
#endif
} GF_UserTask;

static void gf_fs_user_task(GF_FSTask *task)
{
	u32 reschedule_ms=0;
	GF_UserTask *utask = (GF_UserTask *)task->udta;
	task->schedule_next_time = 0;

#ifndef GPAC_DISABLE_REMOTERY
	gf_rmt_begin_hash(task->log_name, GF_RMT_AGGREGATE, &utask->rmt_hash);
#endif
	if (utask->task_execute) {
		task->requeue_request = utask->task_execute(utask->fsess, utask->callback, &reschedule_ms);
	} else if (task->filter) {
		task->requeue_request = utask->task_execute_filter(task->filter, utask->callback, &reschedule_ms);
	} else {
		task->requeue_request = 0;
	}
	gf_rmt_end();
	//if no requeue request or if we are in final flush, don't re-execute
	if (!task->requeue_request || utask->fsess->in_final_flush) {
		gf_free(utask);
		task->udta = NULL;
		task->requeue_request = GF_FALSE;
	} else {
		task->schedule_next_time = gf_sys_clock_high_res() + 1000*reschedule_ms;
	}
}

GF_EXPORT
GF_Err gf_fs_post_user_task(GF_FilterSession *fsess, Bool (*task_execute) (GF_FilterSession *fsess, void *callback, u32 *reschedule_ms), void *udta_callback, const char *log_name)
{
	GF_UserTask *utask;
	if (!fsess || !task_execute) return GF_BAD_PARAM;
	GF_SAFEALLOC(utask, GF_UserTask);
	if (!utask) return GF_OUT_OF_MEM;
	utask->fsess = fsess;
	utask->callback = udta_callback;
	utask->task_execute = task_execute;
	gf_fs_post_task(fsess, gf_fs_user_task, NULL, NULL, log_name ? log_name : "user_task", utask);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_filter_post_task(GF_Filter *filter, Bool (*task_execute) (GF_Filter *filter, void *callback, u32 *reschedule_ms), void *udta, const char *task_name)
{
	GF_UserTask *utask;
	if (!filter || !task_execute) return GF_BAD_PARAM;
	GF_SAFEALLOC(utask, GF_UserTask);
	if (!utask) return GF_OUT_OF_MEM;
	utask->callback = udta;
	utask->task_execute_filter = task_execute;
	utask->fsess = filter->session;
	gf_fs_post_task(filter->session, gf_fs_user_task, filter, NULL, task_name ? task_name : "user_task", utask);
	return GF_OK;
}


GF_EXPORT
Bool gf_fs_is_last_task(GF_FilterSession *fsess)
{
	if (!fsess) return GF_TRUE;
	if (fsess->tasks_pending>1) return GF_FALSE;
	if (gf_fq_count(fsess->main_thread_tasks)) return GF_FALSE;
	if (gf_fq_count(fsess->tasks)) return GF_FALSE;
	return GF_TRUE;
}

GF_EXPORT
Bool gf_fs_mime_supported(GF_FilterSession *fsess, const char *mime)
{
	u32 i, count;
	//first pass on explicit mimes
	count = gf_list_count(fsess->registry);
	for (i=0; i<count; i++) {
		u32 j;
		const GF_FilterRegister *freg = gf_list_get(fsess->registry, i);
		for (j=0; j<freg->nb_caps; j++) {
			const GF_FilterCapability *acap = &freg->caps[j];
			if (!(acap->flags & GF_CAPFLAG_INPUT)) continue;
			if (acap->code == GF_PROP_PID_MIME) {
				if (acap->val.value.string && strstr(acap->val.value.string, mime)) return GF_TRUE;
			}
		}
	}
	return GF_FALSE;
}


GF_EXPORT
void gf_fs_enable_reporting(GF_FilterSession *session, Bool reporting_on)
{
	if (session) session->reporting_on = reporting_on;
}

GF_EXPORT
void gf_fs_lock_filters(GF_FilterSession *session, Bool do_lock)
{
	if (!session || !session->filters_mx) return;
	if (do_lock) gf_mx_p(session->filters_mx);
	else gf_mx_v(session->filters_mx);
}

GF_EXPORT
u32 gf_fs_get_filters_count(GF_FilterSession *session)
{
	return session ? gf_list_count(session->filters) : 0;
}

GF_EXPORT
GF_Err gf_filter_get_stats(GF_Filter *f, GF_FilterStats *stats)
{
	u32 i;
	Bool set_name=GF_FALSE;
	if (!stats || !f) return GF_BAD_PARAM;
	memset(stats, 0, sizeof(GF_FilterStats));
	stats->filter = f;
	stats->filter_alias = f->multi_sink_target;
	if (f->multi_sink_target) return GF_OK;
	
	stats->percent = f->status_percent>10000 ? -1 : (s32) f->status_percent;
	stats->status = f->status_str;
	stats->nb_pck_processed = f->nb_pck_processed;
	stats->nb_bytes_processed = f->nb_bytes_processed;
	stats->time_process = f->time_process;
	stats->nb_hw_pck_sent = f->nb_hw_pck_sent;
	stats->nb_pck_sent = f->nb_pck_sent;
	stats->nb_bytes_sent = f->nb_bytes_sent;
	stats->nb_tasks_done = f->nb_tasks_done;
	stats->nb_errors = f->nb_errors;
	stats->name = f->name;
	stats->reg_name = f->freg->name;
	stats->filter_id = f->id;
	stats->done = f->removed || f->finalized;
	if (stats->name && !strcmp(stats->name, stats->reg_name)) {
		set_name=GF_TRUE;
	}
	stats->report_updated = f->report_updated;
	f->report_updated = GF_FALSE;


	if (!stats->nb_pid_out && stats->nb_pid_in) stats->type = GF_FS_STATS_FILTER_RAWOUT;
	else if (!stats->nb_pid_in && stats->nb_pid_out) stats->type = GF_FS_STATS_FILTER_RAWIN;

	stats->nb_pid_out = f->num_output_pids;
	for (i=0; i<f->num_output_pids; i++) {
		GF_FilterPid *pid = gf_list_get(f->output_pids, i);
		stats->nb_out_pck += pid->nb_pck_sent;
		if (pid->has_seen_eos) stats->in_eos = GF_TRUE;

		if (pid->last_ts_sent.num * stats->last_ts_sent.den >= stats->last_ts_sent.num * pid->last_ts_sent.den)
			stats->last_ts_sent = pid->last_ts_sent;

		if (f->num_output_pids!=1) continue;

		if (!stats->codecid)
			stats->codecid = pid->codecid;
		if (!stats->stream_type)
			stats->stream_type = pid->stream_type;

		//set name if PID name is not a default generated one
		if (set_name && strncmp(pid->name, "PID", 3)) {
			stats->name = pid->name;
			set_name = GF_FALSE;
		}
	}
	stats->nb_pid_in = f->num_input_pids;
	for (i=0; i<f->num_input_pids; i++) {
		GF_FilterPidInst *pidi = gf_list_get(f->input_pids, i);
		stats->nb_in_pck += pidi->nb_processed;
		if (pidi->is_end_of_stream) stats->in_eos = GF_TRUE;

		if (pidi->is_decoder_input) stats->type = GF_FS_STATS_FILTER_DECODE;
		else if (pidi->is_encoder_input) stats->type = GF_FS_STATS_FILTER_ENCODE;

		if (pidi->pid->stream_type==GF_STREAM_FILE)
			stats->type = GF_FS_STATS_FILTER_DEMUX;

		if (pidi->last_ts_drop.num * stats->last_ts_drop.den >= stats->last_ts_drop.num * pidi->last_ts_drop.den)
			stats->last_ts_drop = pidi->last_ts_drop;

		if ((f->num_input_pids!=1) && f->num_output_pids)
			continue;

		if (!stats->codecid)
			stats->codecid = pidi->pid->codecid;
		if (!stats->stream_type)
			stats->stream_type = pidi->pid->stream_type;

		if (set_name) {
			stats->name = pidi->pid->name;
			set_name = GF_FALSE;
		}
	}
	if (!stats->type && stats->codecid) {
		if (!stats->nb_pid_out) {
			stats->type = GF_FS_STATS_FILTER_MEDIA_SINK;
		} else if (!stats->nb_pid_in) {
			stats->type = GF_FS_STATS_FILTER_MEDIA_SOURCE;
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_fs_get_filter_stats(GF_FilterSession *session, u32 idx, GF_FilterStats *stats)
{
	if (!stats || !session) return GF_BAD_PARAM;
	return gf_filter_get_stats(gf_list_get(session->filters, idx), stats);
}

Bool gf_fs_ui_event(GF_FilterSession *session, GF_Event *uievt)
{
	Bool ret;
	gf_mx_p(session->ui_mx);
	ret = session->ui_event_proc(session->ui_opaque, uievt);
	gf_mx_v(session->ui_mx);
	return ret;
}

void gf_fs_check_graph_load(GF_FilterSession *fsess, Bool for_load)
{
	if (for_load) {
		if (!fsess->links || ! gf_list_count( fsess->links))
			gf_filter_sess_build_graph(fsess, NULL);
	} else {
		if (fsess->flags & GF_FS_FLAG_NO_GRAPH_CACHE)
			gf_filter_sess_reset_graph(fsess, NULL);
	}
}

#ifndef GPAC_DISABLE_3D


#define GF_VIDEO_HW_INTERNAL	(1<<29)
#define GF_VIDEO_HW_ATTACHED	(1<<30)

static Bool fsess_on_event(void *cbk, GF_Event *evt)
{
	return GF_TRUE;
}

GF_Err gf_fs_check_gl_provider(GF_FilterSession *session)
{
	GF_Event evt;
	GF_Err e;
	const char *sOpt;
	void *os_disp_handler;

	if (!session->nb_gl_filters) return GF_OK;
	if (gf_list_count(session->gl_providers)) return GF_OK;

	if (session->gl_driver) return GF_OK;


	session->gl_driver = (GF_VideoOutput *) gf_module_load(GF_VIDEO_OUTPUT_INTERFACE, gf_opts_get_key("core", "video-output") );

	if (!session->gl_driver) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to load a video output for OpenGL context support !\n"));
		return GF_IO_ERR;
	}
	if (!gf_opts_get_key("core", "video-output")) {
		gf_opts_set_key("core", "video-output", session->gl_driver->module_name);
	}
	session->gl_driver->hw_caps |= GF_VIDEO_HW_INTERNAL;
	session->gl_driver->on_event = fsess_on_event;
	session->gl_driver->evt_cbk_hdl = session;

	os_disp_handler = NULL;
	sOpt = gf_opts_get_key("Temp", "OSDisp");
	if (sOpt) sscanf(sOpt, "%p", &os_disp_handler);

	e = session->gl_driver->Setup(session->gl_driver, NULL, os_disp_handler, GF_TERM_INIT_HIDE);
	if (e!=GF_OK) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Failed to setup Video Driver %s!\n", session->gl_driver->module_name));
		gf_modules_close_interface((GF_BaseInterface *)session->gl_driver);
		session->gl_driver = NULL;
		return e;
	}

	//and initialize GL context
	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_VIDEO_SETUP;
	evt.setup.width = 128;
	evt.setup.height = 128;
	evt.setup.use_opengl = GF_TRUE;
	evt.setup.back_buffer = 1;
	//we anyway should'nt call swapBuffer/flush on this object
	evt.setup.disable_vsync = GF_TRUE;
	session->gl_driver->ProcessEvent(session->gl_driver, &evt);

	if (evt.setup.use_opengl) {
		gf_opengl_init();
	}
	return GF_OK;
}

GF_Err gf_fs_set_gl(GF_FilterSession *session)
{
	GF_Event evt;
	if (!session->gl_driver) return GF_BAD_PARAM;
	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_SET_GL;
	return session->gl_driver->ProcessEvent(session->gl_driver, &evt);
}

GF_VideoOutput *gf_filter_claim_opengl_provider(GF_Filter *filter)
{
	if (!filter || !filter->session || !filter->session->gl_driver) return NULL;

	if (! (filter->session->gl_driver->hw_caps & GF_VIDEO_HW_INTERNAL))
		return NULL;
	if (filter->session->gl_driver->hw_caps & GF_VIDEO_HW_ATTACHED)
		return NULL;

	filter->session->gl_driver->hw_caps |= GF_VIDEO_HW_ATTACHED;
	return filter->session->gl_driver;
}

Bool gf_filter_unclaim_opengl_provider(GF_Filter *filter, GF_VideoOutput * video_out)
{
	if (!filter || !video_out) return GF_FALSE;

	if (! (video_out->hw_caps & GF_VIDEO_HW_INTERNAL))
		return GF_FALSE;
	if (video_out != filter->session->gl_driver)
		return GF_FALSE;

	if (filter->session->gl_driver->hw_caps & GF_VIDEO_HW_ATTACHED) {
		filter->session->gl_driver->hw_caps &= ~GF_VIDEO_HW_ATTACHED;
		filter->session->gl_driver->on_event = fsess_on_event;
		filter->session->gl_driver->evt_cbk_hdl = filter->session;
		return GF_TRUE;
	}
	return GF_FALSE;
}

#else
void *gf_filter_claim_opengl_provider(GF_Filter *filter)
{
	return NULL;
}
Bool gf_filter_unclaim_opengl_provider(GF_Filter *filter, void *vout)
{
	return GF_FALSE;
}

#endif


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
	opt = gf_opts_get_key("core", "lang");
	if (opt && (!strcmp(opt, "*") || !strcmp(opt, "un") ) ) {
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

