/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2022
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
#include <gpac/module.h>

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

GF_EXPORT
void gf_fs_add_filter_register(GF_FilterSession *fsess, const GF_FilterRegister *freg)
{
	if (!freg || !fsess) return;

	if (!freg->name) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter missing name - ignoring\n"));
		return;
	}
#if 0
	if (strchr(freg->name, fsess->sep_args)	) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter name cannot contain argument separator %c - ignoring\n", fsess->sep_args));
		return;
	}
	if (strchr(freg->name, fsess->sep_name)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter name cannot contain argument value separator %c - ignoring\n", fsess->sep_name));
		return;
	}
#endif
	if (!freg->process) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter %s missing process function - ignoring\n", freg->name));
		return;
	}
	if (fsess->blacklist) {
		Bool match = GF_FALSE;
		const char *blacklist = fsess->blacklist;
		Bool is_whitelist=GF_FALSE;
		if (blacklist[0]=='-') {
			blacklist++;
			is_whitelist = GF_TRUE;
		}
		while (blacklist) {
			char *fname = strstr(blacklist, freg->name);
			if (!fname) break;
			u32 len = (u32) strlen(freg->name);
			if (!fname[len] || (fname[len] == fsess->sep_list)) {
				match = GF_TRUE;
				break;
			}
			blacklist = fname+len;
		}
		if (is_whitelist) match = !match;

		if (match) {
			if (freg->register_free) freg->register_free(fsess, (GF_FilterRegister *)freg);
			return;
		}
	}
	//except for meta filters, don't accept a filter with input caps but no output caps
	//meta filters do follow the same rule, however when expanding them for help we may have weird cases
	//where no config is set but inputs are listed to expose mime types or extensions (cf ffdmx)
	if (!(freg->flags & GF_FS_REG_META)
		&& !freg->configure_pid
		&& gf_filter_has_in_caps(freg->caps, freg->nb_caps)
	) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter %s missing configure_pid function but has input capabilities - ignoring\n", freg->name));
		if (freg->register_free) freg->register_free(fsess, (GF_FilterRegister *)freg);
		return;
	}

	gf_mx_p(fsess->filters_mx);
	gf_list_add(fsess->registry, (void *) freg);
	gf_mx_v(fsess->filters_mx);

	if (fsess->init_done && fsess->links && gf_list_count( fsess->links)) {
		gf_filter_sess_build_graph(fsess, freg);
	}
}



static Bool fs_default_event_proc(void *ptr, GF_Event *evt)
{
	GF_FilterSession *fs = (GF_FilterSession *)ptr;
	if (evt->type==GF_EVENT_QUIT) {
		gf_fs_abort(fs, (evt->message.error) ? GF_FS_FLUSH_NONE : GF_FS_FLUSH_FAST);
	}
	if (evt->type==GF_EVENT_MESSAGE) {
		if (evt->message.error) {
			if (evt->message.service) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Service %s %s: %s\n", evt->message.service, evt->message.message, gf_error_to_string(evt->message.error) ))
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("%s: %s\n", evt->message.message, gf_error_to_string(evt->message.error) ))
			}
		}
	}

#ifdef GPAC_HAS_QJS
	if (fs->on_evt_task && jsfs_on_event(fs, evt))
		return GF_TRUE;
#endif

	if (evt->type==GF_EVENT_AUTHORIZATION) {
#ifdef GPAC_HAS_QJS
		if (fs->on_auth_task && jsfs_on_auth(fs, evt))
			return GF_TRUE;
#endif

		if (gf_sys_is_test_mode()) {
			return GF_FALSE;
		}
		assert( evt->type == GF_EVENT_AUTHORIZATION);
		assert( evt->auth.user);
		assert( evt->auth.password);
		assert( evt->auth.site_url);

		fprintf(stderr, "**** Authorization required for site %s %s ****\n", evt->auth.site_url, evt->auth.secure ? "(secure)" : "- NOT SECURE");
		fprintf(stderr, "login   : ");
		if (!gf_read_line_input(evt->auth.user, 50, 1))
			return GF_FALSE;
		fprintf(stderr, "\npassword: ");
		if (!gf_read_line_input(evt->auth.password, 50, 0))
			return GF_FALSE;
		fprintf(stderr, "*********\n");

		if (evt->auth.on_usr_pass) {
			evt->auth.on_usr_pass(evt->auth.async_usr_data, evt->auth.user, evt->auth.password, GF_FALSE);
			evt->auth.password[0] = 0;
		}
		return GF_TRUE;
	}

	return GF_FALSE;
}


#ifdef GF_FS_ENABLE_LOCALES
static Bool fs_check_locales(void *__self, const char *locales_parent_path, const char *rel_path, char *relocated_path, char *localized_rel_path);
#endif


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
	//reverse implicit mode flag, we run by default in this mode
	if (flags & GF_FS_FLAG_NO_IMPLICIT)
		flags &= ~GF_FS_FLAG_IMPLICIT_MODE;
	else
		flags |= GF_FS_FLAG_IMPLICIT_MODE;

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

	if (flags & GF_FS_FLAG_NON_BLOCKING)
		fsess->non_blocking = 1;

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
		char szName[30];
		GF_SessionThread *sess_thread;
		GF_SAFEALLOC(sess_thread, GF_SessionThread);
		if (!sess_thread) continue;
#ifndef GPAC_DISABLE_REMOTERY
		sprintf(sess_thread->rmt_name, "FSThread%d", i+1);
#endif
		sprintf(szName, "gf_fs_th_%d", i+1);
		sess_thread->th = gf_th_new(szName);
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
	fsess->default_pid_buffer_max_us = gf_opts_get_int("core", "buffer-gen");
	fsess->decoder_pid_buffer_max_us = gf_opts_get_int("core", "buffer-dec");
	fsess->default_pid_buffer_max_units = gf_opts_get_int("core", "buffer-units");
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

	//parse all global filter options for argument tracking
	if (gf_sys_has_filter_global_args() || gf_sys_has_filter_global_meta_args()) {
		u32 nb_args = gf_sys_get_argc();
		for (i=0; i<nb_args; i++) {
			char *arg = (char *)gf_sys_get_arg(i);
			if (arg[0]!='-') continue;
			if ((arg[1]!='-') && (arg[1]!='+')) continue;
			char *sep = strchr(arg, '=');
			if (sep) sep[0] = 0;
			gf_fs_push_arg(fsess, arg+2, GF_FALSE, (arg[1]=='-') ? GF_ARGTYPE_GLOBAL : GF_ARGTYPE_META, NULL, NULL);

			//force indexing in reframers when dash template with bandwidth is used
			if (sep && !strcmp(arg+2, "template") && strstr(sep+1, "$Bandwidth$")) {
				gf_opts_set_key("temp", "force_indexing", "true");
			}

			if (sep) sep[0] = '=';
		}
	}

#ifdef GF_FS_ENABLE_LOCALES
	fsess->uri_relocators = gf_list_new();
	fsess->locales.relocate_uri = fs_check_locales;
	fsess->locales.sess = fsess;
	gf_list_add(fsess->uri_relocators, &fsess->locales);
#endif
	return fsess;
}

void gf_fs_push_arg(GF_FilterSession *session, const char *szArg, Bool was_found, GF_FSArgItemType type, GF_Filter *meta_filter, const char *sub_opt_name)
{
	u32 meta_len = meta_filter ? (u32) strlen(meta_filter->freg->name) : 0;
	Bool create_if_not_found = GF_TRUE;
	if (session->flags & GF_FS_FLAG_NO_ARG_CHECK)
		return;

	//ignore any meta argument reported (found or not) that is not already present
	//if sub_opt_name, we must create an entry
	if (!sub_opt_name && (type==GF_ARGTYPE_META_REPORTING)) {
		create_if_not_found = GF_FALSE;
	}
	if (!session->parsed_args) session->parsed_args = gf_list_new();

	u32 k, acount = gf_list_count(session->parsed_args);
	GF_FSArgItem *ai=NULL;
	for (k=0; k<acount; k++) {
		ai = gf_list_get(session->parsed_args, k);
		if (!strcmp(ai->argname, szArg))
			break;

		if (meta_len
			&& !strncmp(ai->argname, meta_filter->freg->name, meta_len)
			&& ((ai->argname[meta_len]==':') || (ai->argname[meta_len]=='@'))
			&& !strcmp(ai->argname+meta_len+1, szArg)
		) {
			break;
		}
		ai = NULL;
	}
	if (!ai && create_if_not_found) {
		GF_SAFEALLOC(ai, GF_FSArgItem);
		if (ai) {
			ai->argname = gf_strdup(szArg);
			ai->type = type;
			if ((type==GF_ARGTYPE_META_REPORTING) && meta_filter) {
				ai->meta_filter = meta_filter->freg->name;
				ai->meta_opt = sub_opt_name;
			}
			gf_list_add(session->parsed_args, ai );
		}
	}
	if (!ai) return;

	if (type==GF_ARGTYPE_META_REPORTING) {
		//meta option declared as true by default at init but notified as not found
		if (ai->meta_state==3) {
			ai->meta_state = 0;
			if (!was_found) ai->opt_found = 0;
		}
		if (!ai->meta_state) {
			ai->opt_found = 0;
			ai->meta_state = 1;
			if (!ai->meta_filter) {
				ai->meta_filter = meta_filter->freg->name;
				ai->meta_opt = sub_opt_name;
			}
		}
		if (was_found)
			ai->meta_state = 2;
	} else if (was_found && !ai->meta_state) {
		ai->opt_found = 1;
		//initial declaration from filter setup: meta args are declared as true by default
		if (type==GF_ARGTYPE_LOCAL) {
			//forbid further meta state change
			ai->meta_state = 1;
			//meta option, mark as not found
			if (meta_filter) ai->meta_state = 3;
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

	if (inflags & GF_FS_FLAG_NON_BLOCKING)
		flags |= GF_FS_FLAG_NON_BLOCKING;

	if (inflags & GF_FS_FLAG_NO_GRAPH_CACHE)
		flags |= GF_FS_FLAG_NO_GRAPH_CACHE;

	if (inflags & GF_FS_FLAG_PRINT_CONNECTIONS)
		flags |= GF_FS_FLAG_PRINT_CONNECTIONS;

	if (inflags & GF_FS_FLAG_NO_IMPLICIT)
		flags |= GF_FS_FLAG_NO_IMPLICIT;

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
	else if (inflags & GF_FS_FLAG_NO_PROBE)
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

GF_EXPORT
void gf_fs_remove_filter_register(GF_FilterSession *session, GF_FilterRegister *freg)
{
	if (!session || !freg) return;

	gf_mx_p(session->filters_mx);
	gf_list_del_item(session->registry, freg);
	gf_mx_v(session->filters_mx);
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
Bool gf_fs_enum_unmapped_options(GF_FilterSession *fsess, u32 *idx, const char **argname, u32 *argtype, const char **meta_filter, const char **meta_sub_opt)
{
	if (!fsess || !fsess->parsed_args) return GF_FALSE;
	u32 i, count = gf_list_count(fsess->parsed_args);

	for (i=*idx; i<count; i++) {
		GF_FSArgItem *ai = gf_list_get(fsess->parsed_args, i);
		(*idx)++;
		if (ai->opt_found || (ai->meta_state==2)) continue;
		if (argname) *argname = ai->argname;
		if (argtype) *argtype = ai->type;
		if (meta_filter) *meta_filter = ai->meta_filter;
		if (meta_sub_opt) *meta_sub_opt = ai->meta_opt;
		return GF_TRUE;
	}
	return GF_FALSE;
}

void task_canceled(GF_FSTask *task);
static void gf_task_del(void *p)
{
	GF_FSTask *t = (GF_FSTask *) p;
	task_canceled(t);
	gf_free(p);
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
		u32 i, pass, count=gf_list_count(fsess->filters);
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
			gf_mx_p(filter->tasks_mx);
			for (j=0; j<filter->num_input_pids; j++) {
				GF_FilterPidInst *pidi = gf_list_get(filter->input_pids, j);
				gf_filter_pid_inst_reset(pidi);
			}
			gf_mx_v(filter->tasks_mx);
			filter->scheduled_for_next_task = GF_FALSE;
		}
		//second pass, finalize all
		for (pass=0; pass<2; pass++) {
			Bool has_scripts = GF_FALSE;
			for (i=0; i<count; i++) {
				GF_Filter *filter = gf_list_get(fsess->filters, i);
				if (!pass && (filter->freg->flags & GF_FS_REG_SCRIPT)) {
					has_scripts = GF_TRUE;
					continue;
				}
				if (filter->freg->finalize && !filter->finalized) {
					filter->finalized = GF_TRUE;
					FSESS_CHECK_THREAD(filter)
					filter->freg->finalize(filter);
				}
			}
			if (!has_scripts) break;
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
		gf_fq_del(fsess->tasks, gf_task_del);

	if (fsess->tasks_reservoir)
		gf_fq_del(fsess->tasks_reservoir, gf_void_del);

	if (fsess->threads) {
		if (fsess->main_thread_tasks)
			gf_fq_del(fsess->main_thread_tasks, gf_task_del);

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

#ifdef GF_FS_ENABLE_LOCALES
	if (fsess->uri_relocators) gf_list_del(fsess->uri_relocators);
	if (fsess->locales.szAbsRelocatedPath) gf_free(fsess->locales.szAbsRelocatedPath);
#endif

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

void gf_fs_post_task_ex(GF_FilterSession *fsess, gf_fs_task_callback task_fun, GF_Filter *filter, GF_FilterPid *pid, const char *log_name, void *udta, Bool is_configure, Bool force_main_thread, Bool force_direct_call, u32 class_type)
{
	GF_FSTask *task;
	Bool notified = GF_FALSE;

	assert(fsess);
	assert(task_fun);

	//only flatten calls if in main thread (we still have some broken filters using threading that could trigger tasks)
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

	/*this was a gf_filter_process_task request but direct call could not be done or requeue is requested.
	process_task_queued was incremented by caller without checking for existing process task
		- If the task was not treated, dec / inc will give the same state, undo process_task_queued increment
		- If the task was requeued, dec will undo the increment done when requeing the task in gf_filter_check_pending_tasks

	In both cases, inc will redo the same logic as in gf_filter_post_process_task_internal, not creating task if gf_filter_process_task is
	already scheduled for the filter

	We must use safe_int_dec/safe_int_inc here for multi thread cases - cf issue #1778
	*/
	if (force_direct_call) {
		assert(filter);
		safe_int_dec(&filter->process_task_queued);
		if (safe_int_inc(&filter->process_task_queued) > 1) {
			return;
		}
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
	task->class_type = class_type;

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
				force_main_thread = (filter->nb_main_thread_forced || (filter->freg->flags & GF_FS_REG_MAIN_THREAD)) ? GF_TRUE : GF_FALSE;
		} else if (force_main_thread) {
#if 0
			force_main_thread = GF_FALSE;
			if (filter->process_th_id && (fsess->main_th.th_id != filter->process_th_id)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_SCHEDULER, ("Cannot post task to main thread, filter is already scheduled\n"));
			}
#endif
		}
		if (!force_main_thread)
			task->blocking = (filter->is_blocking_source) ? GF_TRUE : GF_FALSE;
		else
			task->force_main = GF_TRUE;

		gf_fq_add(filter->tasks, task);
		gf_mx_v(filter->tasks_mx);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u Posted task %p Filter %s::%s (%d (%d) pending, %d process tasks) on %s task list\n", gf_th_id(), task, filter->name, task->log_name, fsess->tasks_pending, gf_fq_count(filter->tasks), filter->process_task_queued, task->notified ? (force_main_thread ? "main" : "secondary") : "filter"));
	} else {
		task->notified = notified = GF_TRUE;
		task->force_main = force_main_thread;
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
		if (task->force_main) {
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
	gf_fs_post_task_ex(fsess, task_fun, filter, pid, log_name, udta, GF_FALSE, GF_FALSE, GF_FALSE, TASK_TYPE_NONE);
}

void gf_fs_post_task_class(GF_FilterSession *fsess, gf_fs_task_callback task_fun, GF_Filter *filter, GF_FilterPid *pid, const char *log_name, void *udta, u32 class_id)
{
	gf_fs_post_task_ex(fsess, task_fun, filter, pid, log_name, udta, GF_FALSE, GF_FALSE, GF_FALSE, class_id);
}

Bool gf_fs_check_filter_register_cap_ex(const GF_FilterRegister *f_reg, u32 incode, GF_PropertyValue *cap_input, u32 outcode, GF_PropertyValue *cap_output, Bool exact_match_only, Bool out_cap_excluded)
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
					if (out_cap_excluded) {
						exclude_cid_out = (cap->flags & GF_CAPS_OUTPUT_STATIC) ? 2 : 1;
					} else {
						has_cid_match = (cap->flags & GF_CAPS_OUTPUT_STATIC) ? 2 : 1;
					}
				}
			} else {
				Bool prop_equal = gf_props_equal(&cap->val, cap_output);
				if (out_cap_excluded)
					prop_equal = !prop_equal;

				if (prop_equal) {
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
GF_EXPORT
Bool gf_fs_check_filter_register_cap(const GF_FilterRegister *f_reg, u32 incode, GF_PropertyValue *cap_input, u32 outcode, GF_PropertyValue *cap_output, Bool exact_match_only)
{
	return gf_fs_check_filter_register_cap_ex(f_reg, incode, cap_input, outcode, cap_output, exact_match_only, GF_FALSE);
}

static GF_Filter *gf_fs_load_encoder(GF_FilterSession *fsess, const char *args)
{
	GF_Err e;
	char szCodec[3];
	char *cid, *sep;
	const GF_FilterRegister *candidate;
	u32 codecid=0;
	GF_Filter *filter;
	u32 i, count;
	GF_PropertyValue cap_in, cap_out;
	GF_List *blacklist = NULL;
	Bool ocap_excluded = GF_FALSE;
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

	codecid = gf_codecid_parse(cid+2);
#if 0
	if (codecid==GF_CODECID_NONE) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Unrecognized codec identifier in \"enc\" definition: %s\n", cid));
		if (sep) sep[0] = fsess->sep_args;
		return NULL;
	}
#endif
	if (sep) sep[0] = fsess->sep_args;

	cap_in.type = GF_PROP_UINT;
	cap_in.value.uint = GF_CODECID_RAW;
	cap_out.type = GF_PROP_UINT;
	if (codecid==GF_CODECID_NONE) {
		cap_out.value.uint = GF_CODECID_RAW;
		ocap_excluded = GF_TRUE;
	} else {
		cap_out.value.uint = codecid;
	}

retry:
	candidate = NULL;
	count = gf_list_count(fsess->registry);
	for (i=0; i<count; i++) {
		const GF_FilterRegister *f_reg = gf_list_get(fsess->registry, i);
		if (blacklist && (gf_list_find(blacklist, (void *) f_reg)>=0) )
			continue;

		if ( gf_fs_check_filter_register_cap_ex(f_reg, GF_PROP_PID_CODECID, &cap_in, GF_PROP_PID_CODECID, &cap_out, GF_FALSE, ocap_excluded)) {
			if (!candidate || (candidate->priority>f_reg->priority))
				candidate = f_reg;
		}
	}
	if (!candidate) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Cannot find any filter providing encoding for %s\n", cid));
		if (blacklist) gf_list_del(blacklist);
		return NULL;
	}
	filter = gf_filter_new(fsess, candidate, args, NULL, GF_FILTER_ARG_EXPLICIT, &e, NULL, GF_FALSE);
	if (!filter) {
		if (e==GF_NOT_SUPPORTED) {
			if (!blacklist) blacklist = gf_list_new();
			gf_list_add(blacklist, (void *) candidate);
			goto retry;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to load filter %s: %s\n", candidate->name, gf_error_to_string(e) ));
	} else {
		filter->encoder_stream_type = gf_codecid_type(codecid);
	}
	if (blacklist) gf_list_del(blacklist);
	return filter;
}

GF_EXPORT
Bool gf_fs_filter_exists(GF_FilterSession *fsess, const char *name)
{
	u32 i, count;

	if (!strcmp(name, "enc")) return GF_TRUE;
	if ((strlen(name)>2) && (name[0]=='c') && (name[1]==fsess->sep_name))
		return GF_TRUE;

	count = gf_list_count(fsess->registry);
	for (i=0;i<count;i++) {
		const GF_FilterRegister *f_reg = gf_list_get(fsess->registry, i);
		if (!strcmp(f_reg->name, name)) {
			return GF_TRUE;
		}
	}
	return GF_FALSE;
}

static Bool locate_js_script(char *path, const char *file_name, const char *file_ext)
{
	u32 len = (u32) strlen(path);
	u32 flen = 20 + (u32) strlen(file_name);

	char *apath = gf_malloc(sizeof(char) * (len+flen) );
	if (!apath) return GF_FALSE;
	strcpy(apath, path);

	strcat(apath, file_name);
	if (gf_file_exists(apath)) {
		strncpy(path, apath, GF_MAX_PATH-1);
		path[GF_MAX_PATH-1] = 0;
		gf_free(apath);
		return GF_TRUE;
	}

	if (!file_ext) {
		strcat(apath, ".js");
		if (gf_file_exists(apath)) {
			strncpy(path, apath, GF_MAX_PATH-1);
			path[GF_MAX_PATH-1] = 0;
			gf_free(apath);
			return GF_TRUE;
		}
	}
	apath[len] = 0;
	strcat(apath, file_name);
	strcat(apath, "/init.js");
	if (gf_file_exists(apath)) {
		strncpy(path, apath, GF_MAX_PATH-1);
		path[GF_MAX_PATH-1] = 0;
		gf_free(apath);
		return GF_TRUE;
	}
	gf_free(apath);
	return GF_FALSE;
}

Bool gf_fs_solve_js_script(char *szPath, const char *file_name, const char *file_ext)
{
	const char *js_dirs;
	if (gf_opts_default_shared_directory(szPath)) {
		strcat(szPath, "/scripts/jsf/");
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Trying JS filter %s\n", szPath));
		if (locate_js_script(szPath, file_name, file_ext)) {
			return GF_TRUE;
		}
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Failed to get default shared dir\n"));
	}
	js_dirs = gf_opts_get_key("core", "js-dirs");
	while (js_dirs && js_dirs[0]) {
		char *sep = strchr(js_dirs, ',');
		if (sep) {
			u32 cplen = (u32) (sep-js_dirs);
			if (cplen>=GF_MAX_PATH) cplen = GF_MAX_PATH-1;
			strncpy(szPath, js_dirs, cplen);
			szPath[cplen]=0;
			js_dirs = sep+1;
		} else {
			strcpy(szPath, js_dirs);
		}
		if (strcmp(szPath, "$GJS")) {
			u32 len = (u32) strlen(szPath);
			if (len && (szPath[len-1]!='/') && (szPath[len-1]!='\\'))
				strcat(szPath, "/");
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Trying JS filter in %s\n", szPath));
			if (locate_js_script(szPath, file_name, file_ext))
				return GF_TRUE;
		}
		if (!sep) break;
	}
	return GF_FALSE;
}

static GF_Filter *gf_fs_load_filter_internal(GF_FilterSession *fsess, const char *name, GF_Err *err_code, Bool *probe_only)
{
	const char *args=NULL;
	const char *sep, *file_ext;
	u32 i, len, count = gf_list_count(fsess->registry);
	Bool quiet = (err_code && (*err_code == GF_EOS)) ? GF_TRUE : GF_FALSE;

	assert(fsess);
	assert(name);
	if (err_code) *err_code = GF_OK;

	//the first string before any option sep MUST be the filter registry name, so we don't use gf_fs_path_escape_colon here
	sep = strchr(name, fsess->sep_args);
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
	if ((strlen(name)>2) && (name[0]=='c') && (name[1]==fsess->sep_name)) {
		return gf_fs_load_encoder(fsess, name);
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
				//check we have a src specified for the filter
				const char *src_url = strstr(name, "src");
				if (src_url && (src_url[3]==fsess->sep_name)) {
					const GF_FilterArgs *f_args = filter->instance_args ? filter->instance_args : f_reg->args;
					//check the filter has an src argument
					//we don't want to call process on a filter not acting as source until at least one input is connected
					i=0;
					while (f_args && f_args[i].arg_name) {
						if (!strcmp(f_args[i].arg_name, "src")) {
							gf_filter_post_process_task(filter);
							break;
						}
						i++;
					}
				}
			}
			return filter;
		}
	}
	/*check JS file*/
	file_ext = gf_file_ext_start(name);
	if (file_ext && (file_ext > sep) )
		file_ext = NULL;

	if (!file_ext || strstr(name, ".js") || strstr(name, ".jsf") || strstr(name, ".mjs") ) {
		Bool file_exists = GF_FALSE;
		char szName[10+GF_MAX_PATH];
		char szPath[10+GF_MAX_PATH];
		if (len>GF_MAX_PATH)
			return NULL;

		strncpy(szPath, name, len);
		szPath[len]=0;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Trying JS filter %s\n", szPath));
		if (gf_file_exists(szPath)) {
			file_exists = GF_TRUE;
		} else {
			strcpy(szName, szPath);
			file_exists = gf_fs_solve_js_script(szPath, szName, file_ext);
			if (!file_exists && !file_ext) {
				strcat(szName, ".js");
				if (gf_file_exists(szName)) {
					strncpy(szPath, name, len);
					szPath[len]=0;
					strcat(szPath, ".js");
					file_exists = GF_TRUE;
				}
			}
		}

		if (file_exists) {
			if (probe_only) {
				*probe_only = GF_TRUE;
				return NULL;
			}
			sprintf(szName, "jsf%cjs%c", fsess->sep_args, fsess->sep_name);
			strcat(szName, szPath);
			if (name[len])
				strcat(szName, name+len);
			return gf_fs_load_filter(fsess, szName, err_code);
		}
	}
	if (err_code) *err_code = GF_FILTER_NOT_FOUND;
	return NULL;
}

GF_EXPORT
GF_Filter *gf_fs_load_filter(GF_FilterSession *fsess, const char *name, GF_Err *err_code)
{
	return gf_fs_load_filter_internal(fsess, name, err_code, NULL);
}

//in mono thread mode, we cannot always sleep for the requested timeout in case there are more tasks to be processed
//this defines the number of pending tasks above which we limit sleep
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

	if (!thid && fsess->non_blocking) {
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
					//if task is blocking, don't use it, let a secondary thread deal with it
					if (task && task->blocking) {
						gf_fq_add(fsess->tasks, task);
						task = NULL;
						gf_fs_sema_io(fsess, GF_TRUE, GF_FALSE);
					}
				}
				force_secondary_tasks = GF_FALSE;
			} else {
				task = gf_fq_pop(fsess->tasks);
				if (task && (task->force_main || (task->filter && task->filter->nb_main_thread_forced) ) ) {
					//post to main
					gf_fq_add(fsess->main_thread_tasks, task);
					gf_fs_sema_io(fsess, GF_TRUE, GF_TRUE);
					continue;
				}
			}
			assert(!task || task->run_task );
			assert(!task || task->notified );
		} else {
			//keep task in filter tasks list until done
			task = gf_fq_head(current_filter->tasks);
			if (task) {
				assert( task->run_task );
				assert( ! task->notified );

				//task was requested for main thread
				if ((task->force_main || current_filter->nb_main_thread_forced) && thid) {
					//make task notified and increase pending tasks
					task->notified = GF_TRUE;
					safe_int_inc(&fsess->tasks_pending);

					//post to main
					gf_fq_add(fsess->main_thread_tasks, task);
					gf_fs_sema_io(fsess, GF_TRUE, GF_TRUE);
					//disable current filter
					current_filter = NULL;
					continue;
				}
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
			if (!thid && fsess->non_blocking) {
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

		if (current_filter && current_filter->restrict_th_idx && (thid != current_filter->restrict_th_idx)) {
			//reschedule task to secondary list
			if (!task->notified) {
				task->notified = GF_TRUE;
				safe_int_inc(&fsess->tasks_pending);
			}
			gf_fq_add(fsess->tasks, task);
			gf_fs_sema_io(fsess, GF_TRUE, GF_FALSE);
			current_filter = NULL;
			continue;
		}

		assert(!current_filter || !current_filter->restrict_th_idx || (thid == current_filter->restrict_th_idx));

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
					if (!thid && task->notified && (diff > MONOTH_MIN_SLEEP) ) {
						u32 idx=0;
						while (1) {
							next = gf_fq_get(fsess->tasks, idx);
							if (!next || next->blocking) break;
							idx++;

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
								GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Thread %u: forcing secondary task list on main - current task schedule time "LLU" (diff to now %d) vs next time secondary "LLU" (%s::%s)\n", sys_thid, task->schedule_next_time, (s32) diff, next_time_secondary, next->filter ? next->filter->freg->name : "", next->log_name));
								diff = 0;
								force_secondary_tasks = GF_TRUE;
								break;
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
		//remember the last time we scheduled this filter
		if (task->filter)
			task->filter->last_schedule_task_time = task_time;

		task->can_swap = 0;
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
			Bool last_task = GF_FALSE;
			current_filter->nb_tasks_done++;
			current_filter->time_process += task_time;
			consecutive_filter_tasks++;

			gf_mx_p(current_filter->tasks_mx);
			if (gf_fq_count(current_filter->tasks)==1) {
				//if task is set to immediate reschedule, don't consider this is the last task
				//and check consecutive_filter_tasks - this will keep the active filter running
				//when the last task is a process task and the filter is not blocking
				//FIXME: commented out as this breaks ssome tests, needs further checking
				//if (task->can_swap!=2)
				{
					last_task = GF_TRUE;
				}
			}
			//if last task
			if ( last_task
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

					//FIXME, we sometimes miss a sema notfiy resulting in secondary tasks being locked
					//until we find the cause, notify secondary sema if non-main-thread tasks are scheduled and we are the only task in main
					if (use_main_sema && (thid==0) && fsess->threads && (gf_fq_count(fsess->main_thread_tasks)==1) && gf_fq_count(fsess->tasks)) {
						gf_fs_sema_io(fsess, GF_TRUE, GF_FALSE);
					}
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
		if (!thid && fsess->non_blocking && !current_filter && !fsess->pid_connect_tasks_pending) {
			gf_rmt_end();
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Main thread proc exit\n"));
			return 0;
		}
	}

	gf_rmt_end();

	//no main thread, return
	if (!thid && fsess->non_blocking) {
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

	//non blocking mode and threads created, only run main thread proc
	if (fsess->non_blocking && (fsess->non_blocking==2) ) {
		gf_fs_thread_proc(&fsess->main_th);
		return fsess->run_status;
	}

	//run threads
	fsess->run_status = GF_OK;
	fsess->main_th.has_seen_eot = GF_FALSE;
	fsess->nb_threads_stopped = 0;

	nb_threads = gf_list_count(fsess->threads);
	for (i=0;i<nb_threads; i++) {
		GF_SessionThread *sess_th = gf_list_get(fsess->threads, i);
		gf_th_run(sess_th->th, (gf_thread_run) gf_fs_thread_proc, sess_th);
	}

	//run main thread
	gf_fs_thread_proc(&fsess->main_th);

	//non blocking mode init, don't wait for other threads
	if (fsess->non_blocking) {
		fsess->non_blocking = 2;
		return fsess->run_status;
	}

	//blocking mode, wait for all threads to be done
	while (nb_threads+1 != fsess->nb_threads_stopped) {
		gf_sleep(1);
	}

	return fsess->run_status;
}

static void filter_abort_task(GF_FSTask *task)
{
	GF_FilterEvent evt;
	GF_FEVT_INIT(evt, GF_FEVT_STOP, task->pid);

	task->pid->filter->freg->process_event(task->pid->filter, &evt);
	gf_filter_pid_set_eos(task->pid);
	task->pid->filter->disabled = GF_FILTER_DISABLED;
	safe_int_dec(&task->pid->filter->abort_pending);

}

GF_EXPORT
GF_Err gf_fs_abort(GF_FilterSession *fsess, GF_FSFlushType flush_type)
{
	u32 i, count;
	Bool threaded;
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Session abort from user, stopping sources\n"));
	if (!fsess) return GF_BAD_PARAM;
	threaded = (!fsess->filters_mx && (fsess->main_th.th_id==gf_th_id())) ? GF_FALSE : GF_TRUE;

	if (flush_type==GF_FS_FLUSH_NONE) {
		fsess->in_final_flush = GF_TRUE;
		fsess->run_status = GF_EOS;
		return GF_OK;
	}

	fsess->in_final_flush = GF_TRUE;

	gf_mx_p(fsess->filters_mx);
	count = gf_list_count(fsess->filters);
	//disable all sources
	for (i=0; i<count; i++) {
		GF_Filter *filter = gf_list_get(fsess->filters, i);
		//force end of session on all sources, and on all filters connected to these sources, and dispatch end of stream on all outputs pids of these filters
		//if we don't propagate on connected filters, we take the risk of not deactivating demuxers working from file
		//(eg ignoring input packets)
		//
		//we shortcut the thread execution state here by directly calling set_eos, we need to lock/unlock our filters carefully
		//to avoid deadlocks or crashes
		gf_mx_p(filter->tasks_mx);

		if (!filter->num_input_pids) {
			u32 j, k, l;
			if (!filter->disabled)
				filter->disabled = GF_FILTER_DISABLED;
			for (j=0; j<filter->num_output_pids; j++) {
				const GF_PropertyValue *p;
				GF_FilterPid *pid = gf_list_get(filter->output_pids, j);
				//unlock before forcing eos as this could trigger a post task on a filter waiting for this mutex to be unlocked
				gf_mx_v(filter->tasks_mx);
				gf_filter_pid_set_eos(pid);
				gf_mx_p(filter->tasks_mx);
				//if the PID has a codecid set (demuxed pid, e.g. ffavin or other grabbers), do not force STOP on its destinations
				p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
				if (p) continue;
				
				for (k=0; k<pid->num_destinations; k++) {
					Bool force_disable = GF_TRUE;
					GF_FilterPidInst *pidi = gf_list_get(pid->destinations, k);
					gf_mx_v(filter->tasks_mx);
					gf_mx_p(pidi->filter->tasks_mx);
					for (l=0; l<pidi->filter->num_output_pids; l++) {
						GF_FilterPid *opid = gf_list_get(pidi->filter->output_pids, l);
						//We cannot directly call process_event as this would make concurrent access to the filter
						//which we guarantee we will never do
						//but we don't want to send a regular stop event which will reset PID buffers, so:
						//- if called in main thread of session in single-thread mode we can safely force a STOP event
						//otherwise:
						//- post a task to the filter
						//- only disable the filter once the filter_abort has been called
						//- only move to EOS if no filter_abort is pending
						//
						if (opid->filter->freg->process_event) {
							if (threaded) {
								safe_int_inc(&opid->filter->abort_pending);
								gf_fs_post_task(opid->filter->session, filter_abort_task, opid->filter, opid, "filter_abort", NULL);
								force_disable = GF_FALSE;
							} else {
								GF_FilterEvent evt;
								GF_FEVT_INIT(evt, GF_FEVT_STOP, opid);

								opid->filter->freg->process_event(opid->filter, &evt);
								gf_filter_pid_set_eos(opid);
							}
						} else {
							gf_filter_pid_set_eos(opid);
						}
					}
					gf_mx_v(pidi->filter->tasks_mx);
					gf_mx_p(filter->tasks_mx);
					//no filter_abort pending, disable the filter
					if (force_disable)
						pidi->filter->disabled = GF_FILTER_DISABLED;
				}
			}
		}
		//fast flush and this is a sink: send a stop from all filters connected to the sink
		if ((flush_type==GF_FS_FLUSH_FAST) && !filter->num_output_pids) {
			u32 j;
			for (j=0; j<filter->num_input_pids; j++) {
				GF_FilterEvent evt;
				GF_FilterPidInst *pidi = gf_list_get(filter->input_pids, j);
				const GF_PropertyValue *p = gf_filter_pid_get_property((GF_FilterPid *) pidi, GF_PROP_PID_STREAM_TYPE);
				//if pid is of type FILE, we keep the last connections to the sink active so that muxers can still dispatch pending packets
				if (p && (p->value.uint==GF_STREAM_FILE)) {
					u32 k;
					gf_mx_p(pidi->pid->filter->tasks_mx);
					for (k=0; k<pidi->pid->filter->num_input_pids; k++) {
						GF_FilterPid *pid = gf_list_get(pidi->pid->filter->input_pids, k);
						GF_FEVT_INIT(evt, GF_FEVT_STOP, pid);
						gf_filter_pid_send_event(pid, &evt);
						//and force pid to be in eos, since we do a fast flush EOS may not be dispatched by input filter(s)
						gf_filter_pid_set_eos(pid->pid);
					}
					gf_mx_v(pidi->pid->filter->tasks_mx);
				}
				//otherwise send STOP right away
				else {
					GF_FEVT_INIT(evt, GF_FEVT_STOP, (GF_FilterPid *) pidi);
					gf_filter_pid_send_event((GF_FilterPid *) pidi, &evt);
					//and force pid to be in eos, since we do a fast flush EOS may not be dispatched by input filter(s)
					gf_filter_pid_set_eos(pidi->pid);
				}
			}
		}

		gf_mx_v(filter->tasks_mx);
	}
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

	if (!fsess->run_status) {
		fsess->in_final_flush = GF_TRUE;
		fsess->run_status = GF_EOS;
	}

	for (i=0; i < count; i++) {
		gf_fs_sema_io(fsess, GF_TRUE, GF_FALSE);
	}

	//wait for all threads to be done, we might still need flushing the main thread queue
	while (fsess->non_blocking) {
		gf_fs_thread_proc(&fsess->main_th);
		if (gf_fq_count(fsess->main_thread_tasks))
			continue;

		if (count && (count == fsess->nb_threads_stopped) && gf_fq_count(fsess->tasks) ) {
			continue;
		}
		break;
	}
	if (fsess->non_blocking) {
		safe_int_inc(&fsess->nb_threads_stopped);
		fsess->main_th.has_seen_eot = GF_TRUE;
	}

	while (count+1 != fsess->nb_threads_stopped) {
		for (i=0; i < count; i++) {
			gf_fs_sema_io(fsess, GF_TRUE, GF_FALSE);
		}
		gf_sleep(0);
		//we may have tasks in main task list posted by other threads
		if (fsess->non_blocking) {
			gf_fs_thread_proc(&fsess->main_th);
			fsess->main_th.has_seen_eot = GF_TRUE;
		}
	}
	return GF_OK;
}

static GFINLINE void print_filter_name(GF_Filter *f, Bool skip_id, Bool skip_args)
{
	if (f->freg->flags & GF_FS_REG_SCRIPT) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" \"%s\"", f->name));
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s", f->freg->name));
		if (strcmp(f->name, f->freg->name)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" \"%s\"", f->name));
		}
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
	gf_mx_p(fsess->filters_mx);

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

		gf_mx_p(f->tasks_mx);

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
			if (pid->requires_full_data_block && (pid->nb_reagg_pck != pid->pid->nb_pck_sent) ) {
				GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\t\t* input PID %s: %d frames (%d packets) received\n", pid->pid->name, pid->nb_reagg_pck, pid->pid->nb_pck_sent));
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\t\t* input PID %s: %d packets received\n", pid->pid->name, pid->pid->nb_pck_sent));
			}
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

		gf_mx_v(f->tasks_mx);
	}
	gf_mx_v(fsess->filters_mx);

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

static void gf_fs_print_filter_outputs(GF_Filter *f, GF_List *filters_done, u32 indent, GF_FilterPid *pid, GF_Filter *alias_for, u32 src_num_tiled_pids, Bool skip_print, s32 nb_recursion, u32 max_length)
{
	u32 i=0;

	if (!skip_print) {
		while (i<indent) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("-"));
			i++;
		}

		if (src_num_tiled_pids>1) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("(tilePID[%d]) ", src_num_tiled_pids));
		}
		else if (pid) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("(PID %s)", pid->name));
		}
		if (max_length) {
			u32 l;
			if (src_num_tiled_pids) {
				char szName[50];
				sprintf(szName, "PID[%d]", src_num_tiled_pids);
				l = (u32) strlen(szName);
			} else {
				l = (u32) strlen(pid->name);
			}
			while (l<max_length) {
				GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" "));
					l++;
			}
		}
		if (nb_recursion>0) {
			u32 k=0;
			while (k<(u32) nb_recursion-1) {
				k++;
				GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" "));
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\\\n"));
			return;
		}
		if (pid) {
			if (nb_recursion<0) {
				u32 k=(u32) -nb_recursion;
				while (k>1) {
					k--;
					GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("-"));
				}
				GF_LOG(GF_LOG_INFO, GF_LOG_APP, (">"));
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" "));
		}

		print_filter_name(f, GF_TRUE, GF_FALSE);

		GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" (%s=%u)\n", f->dynamic_filter ? "dyn_idx" : "idx", 1+gf_list_find(f->session->filters, f) ));
	}

	if (filters_done && (gf_list_find(filters_done, f)>=0))
		return;

	if (filters_done)
		gf_list_add(filters_done, f);
	if (alias_for && !skip_print) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" (<=> "));
		print_filter_name(alias_for, GF_TRUE, GF_TRUE);
		if (alias_for->id) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" ID=%s", alias_for->id));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" ptr=%p", alias_for));
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, (")\n"));
	}

	GF_List *dests = gf_list_new();
	for (i=0; i<f->num_output_pids; i++) {
		u32 j;
		GF_FilterPid *pidout = gf_list_get(f->output_pids, i);
		for (j=0; j<pidout->num_destinations; j++) {
			GF_FilterPidInst *pidi = gf_list_get(pidout->destinations, j);
			if (gf_list_find(dests, pidi->filter)<0)
				gf_list_add(dests, pidi->filter);
		}
	}

	while (gf_list_count(dests)) {
	GF_Filter *dest = gf_list_pop_front(dests);
	GF_List *pids = gf_list_new();
	u32 max_name_len=0;
	u32 num_tile_pids=0;
	for (i=0; i<f->num_output_pids; i++) {
		u32 j;
		GF_FilterPid *pidout = gf_list_get(f->output_pids, i);
		for (j=0; j<pidout->num_destinations; j++) {
			GF_FilterPidInst *pidi = gf_list_get(pidout->destinations, j);
			if (pidi->filter != dest) continue;
			gf_list_add(pids, pidi);
		}
		u32 plen = (u32) strlen(pidout->name);

		const GF_PropertyValue *p = gf_filter_pid_get_property(pidout, GF_PROP_PID_CODECID);
		if (p &&
			((p->value.uint==GF_CODECID_HEVC_TILES) || (p->value.uint==GF_CODECID_VVC_SUBPIC))
		) {
			plen = 0;
			if (!num_tile_pids) {
				for (j=0; j<f->num_output_pids; j++) {
					GF_FilterPid *apid = gf_list_get(f->output_pids, j);
					p = gf_filter_pid_get_property(apid, GF_PROP_PID_CODECID);
					if (p &&
						((p->value.uint==GF_CODECID_HEVC_TILES) || (p->value.uint==GF_CODECID_VVC_SUBPIC))
					) {
						num_tile_pids++;
					}
				}
				plen = 5;
				j=0;
				while (j<num_tile_pids) {
					plen+=1;
					j+=10;
				}
			}
		}
		if (plen>max_name_len) max_name_len = plen;
	}
	s32 nb_pids_print = gf_list_count(pids);
	if (nb_pids_print==1) nb_pids_print = 0;
	if (num_tile_pids) nb_pids_print -= num_tile_pids-1;
	s32 nb_final_pids = nb_pids_print;
	if (nb_pids_print) nb_pids_print++;
	Bool first_tile = GF_TRUE;

	for (i=0; i<f->num_output_pids; i++) {
		u32 j, k;
		Bool is_tiled = GF_FALSE;
		Bool skip_tiled = skip_print;

		GF_FilterPid *pidout = gf_list_get(f->output_pids, i);
		if (num_tile_pids) {
			const GF_PropertyValue *p = gf_filter_pid_get_property(pidout, GF_PROP_PID_CODECID);
			if (p &&
				((p->value.uint==GF_CODECID_HEVC_TILES) || (p->value.uint==GF_CODECID_VVC_SUBPIC))
			) {
				is_tiled = GF_TRUE;
			}
		}

		for (j=0; j<pidout->num_destinations; j++) {
			GF_Filter *alias = NULL;
			GF_FilterPidInst *pidi = gf_list_get(pidout->destinations, j);
			if (pidi->filter != dest) continue;

			gf_list_del_item(pids, pidi);
			if (nb_pids_print && !gf_list_count(pids))
				nb_pids_print = 0;
			else if (is_tiled) {
				if (!first_tile) continue;
				nb_pids_print = 0;
				first_tile = GF_FALSE;
			}

			for (k=0; k<gf_list_count(f->destination_filters); k++) {
				alias = gf_list_get(f->destination_filters, k);
				if (alias->multi_sink_target == pidi->filter)
					break;
				alias = NULL;
			}
			if (alias) {
				gf_fs_print_filter_outputs(alias, filters_done, indent+1, pidout, pidi->filter, is_tiled ? num_tile_pids : src_num_tiled_pids, skip_tiled, nb_pids_print-nb_final_pids, max_name_len);
			} else {
				gf_fs_print_filter_outputs(pidi->filter, filters_done, indent+1, pidout, NULL, is_tiled ? num_tile_pids : src_num_tiled_pids, skip_tiled, nb_pids_print-nb_final_pids, max_name_len);
			}
		}
		if (nb_pids_print) nb_pids_print++;
	}
	gf_list_del(pids);
	}
	gf_list_del(dests);
}

static void gf_fs_print_not_connected_filters(GF_FilterSession *fsess, GF_List *filters_done, Bool ignore_sinks)
{
	u32 i, count;
	Bool has_unconnected=GF_FALSE;
	count=gf_list_count(fsess->filters);
	for (i=0; i<count; i++) {
		GF_Filter *f = gf_list_get(fsess->filters, i);
		//only dump not connected ones
		if (f->num_input_pids || f->num_output_pids || f->multi_sink_target || f->nb_tasks_done) continue;
		if (f->disabled==GF_FILTER_DISABLED_HIDE) continue;

		if (ignore_sinks) {
			Bool has_outputs;
			if (f->forced_caps)
				has_outputs = gf_filter_has_out_caps(f->forced_caps, f->nb_forced_caps);
			else
				has_outputs = gf_filter_has_out_caps(f->freg->caps, f->freg->nb_caps);
			if (!has_outputs) continue;
		}

		if (!has_unconnected) {
			has_unconnected = GF_TRUE;
			GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("Filters not connected:\n"));
		}
		gf_fs_print_filter_outputs(f, filters_done, 0, NULL, NULL, 0, GF_FALSE, 0, 0);
	}
}

GF_EXPORT
void gf_fs_print_non_connected(GF_FilterSession *fsess)
{
	gf_fs_print_not_connected_filters(fsess, NULL, GF_FALSE);
}

GF_EXPORT
void gf_fs_print_non_connected_ex(GF_FilterSession *fsess, Bool ignore_sinks)
{
	gf_fs_print_not_connected_filters(fsess, NULL, ignore_sinks);
}

GF_EXPORT
void gf_fs_print_connections(GF_FilterSession *fsess)
{
	u32 i, count;
	Bool has_undefined=GF_FALSE;
	Bool has_connected=GF_FALSE;
	GF_List *filters_done;
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\n"));
	gf_mx_p(fsess->filters_mx);

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
		gf_fs_print_filter_outputs(f, filters_done, 0, NULL, NULL, 0, GF_FALSE, 0, 0);
	}

	gf_fs_print_not_connected_filters(fsess, filters_done, GF_FALSE);

	for (i=0; i<count; i++) {
		GF_Filter *f = gf_list_get(fsess->filters, i);
		if (f->multi_sink_target) continue;
		if (gf_list_find(filters_done, f)>=0) continue;
		if (f->disabled==GF_FILTER_DISABLED_HIDE) continue;
		if (!has_undefined) {
			has_undefined = GF_TRUE;
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Filters in unknown connection state:\n"));
		}
		gf_fs_print_filter_outputs(f, filters_done, 0, NULL, NULL, 0, GF_FALSE, 0, 0);
	}

	gf_mx_v(fsess->filters_mx);
	gf_list_del(filters_done);
}

GF_EXPORT
void gf_fs_print_unused_args(GF_FilterSession *fsess, const char *ignore_args)
{
	u32 idx = 0;
	const char *argname;
	u32 argtype;

	while (1) {
		Bool found = GF_FALSE;
		const char *loc_arg;
		if (gf_fs_enum_unmapped_options(fsess, &idx, &argname, &argtype, NULL, NULL)==GF_FALSE)
			break;

		loc_arg = ignore_args;
		while (loc_arg) {
			u32 len;
			char *sep;
			char *match = strstr(loc_arg, argname);
			if (!match) break;
			len = (u32) strlen(argname);
			if (!match[len] || (match[len]==',')) {
				found = GF_TRUE;
				break;
			}
			sep = strchr(loc_arg, ',');
			if (!sep) break;
			loc_arg = sep+1;
		}
		if (found) continue;

		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Arg %s set but not used\n", argname));
	}
}

GF_EXPORT
void gf_fs_send_update(GF_FilterSession *fsess, const char *fid, GF_Filter *filter, const char *name, const char *val, GF_EventPropagateType propagate_mask)
{
	GF_FilterUpdate *upd;
	u32 i, count;
	char *sep = NULL;
	Bool removed = GF_FALSE;
	if ((!fid && !filter) || !name) return;
	if (!fsess) {
		if (!filter) return;
		fsess = filter->session;
	}

	gf_mx_p(fsess->filters_mx);

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
	gf_mx_v(fsess->filters_mx);

	if (removed) return;

	if (!val) {
		sep = strchr(name, fsess->sep_name);
		if (sep) sep[0] = 0;
	}

	//find arg and check if it is only a sync update - if so do it now
	i=0;
	while (filter->freg->args) {
		const GF_FilterArgs *a = &filter->freg->args[i];
		i++;
		if (!a || !a->arg_name) break;

		if ((a->flags & GF_FS_ARG_META) && !strcmp(a->arg_name, "*")) {
			continue;
		} else if (strcmp(a->arg_name, name)) {
			continue;
		}

		if (a->flags & GF_FS_ARG_UPDATE_SYNC) {
			gf_mx_p(filter->tasks_mx);
			gf_filter_update_arg_apply(filter, name, sep ? sep+1 : val, GF_TRUE);
			gf_mx_v(filter->tasks_mx);
			if (sep) sep[0] = fsess->sep_name;
			return;
		}
		break;
	}

	GF_SAFEALLOC(upd, GF_FilterUpdate);
	if (!val) {
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
		if (ext_arg && (ext_arg>fargs) && (ext_arg[-1] != fsess->sep_name))
			ext_arg = NULL;

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
			if (!strcmp(dst_arg->arg_name, "dst") && !(dst_arg->flags&GF_FS_ARG_SINK_ALIAS)) break;
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

static GF_Filter *locate_alias_sink(GF_Filter *filter, const char *url, const char *mime_type)
{
	u32 i;
	for (i=0; i<filter->num_output_pids; i++) {
		u32 j;
		GF_FilterPid *pid = gf_list_get(filter->output_pids, i);
		for (j=0; j<pid->num_destinations; j++) {
			GF_Filter *f;
			GF_FilterPidInst *pidi = gf_list_get(pid->destinations, j);
			if (!pidi->filter) continue;
			if (pidi->filter->act_as_sink && pidi->filter->freg->use_alias
				&& pidi->filter->freg->use_alias(pidi->filter, url, mime_type)
			) {
				return pidi->filter;
			}
			//recursovely walk towards the sink
			f = locate_alias_sink(pidi->filter, url, mime_type);
			if (f) return f;
		}
	}
	return NULL;
}

Bool filter_solve_gdocs(const char *url, char szPath[GF_MAX_PATH]);

GF_Filter *gf_fs_load_source_dest_internal(GF_FilterSession *fsess, const char *url, const char *user_args, const char *parent_url, GF_Err *err, GF_Filter *filter, GF_Filter *dst_filter, Bool for_source, Bool no_args_inherit, Bool *probe_only, const GF_FilterRegister **probe_reg)
{
	GF_FilterProbeScore score = GF_FPROBE_NOT_SUPPORTED;
	GF_FilterRegister *candidate_freg=NULL;
	GF_Filter *alias_for_filter = NULL;
	const GF_FilterArgs *src_dst_arg=NULL;
	u32 i, count, user_args_len, arg_type;
	char szForceReg[20];
	char szMime[50];
	char szForceExt[20];
	GF_Err e;
	const char *force_freg = NULL;
	char *sURL, *mime_type, *args, *sep;
	char szExt[50];
	Bool free_url=GF_FALSE;
	memset(szExt, 0, sizeof(szExt));

	if (!url) {
		if (err) *err = GF_BAD_PARAM;
		return NULL;
	}
	if (err) *err = GF_OK;

	szForceExt[0] = 0;
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
		sprintf(szForceExt, "%cext=", fsess->sep_args);
		char *ext = strstr(url, szForceExt);
		if (ext) {
			ext+=5;
			snprintf(szForceExt, 19, "test.%s", ext);
			szForceExt[19]=0;
			ext = strchr(szForceExt, fsess->sep_args);
			if (ext) ext[0] = 0;
		}
	}
	sURL = NULL;
	if (filter) {
		sURL = (char *) url;
	} else {
		char szSolvedPath[GF_MAX_PATH];
		Bool is_local;

		if (!strncmp(url, "$GDOCS", 6)) {
			if (filter_solve_gdocs(url, szSolvedPath))
				url = szSolvedPath;
		}
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
		//remove any filter arguments in URL before checking if it is local
		//not doing so will lead wrong result if one argument is a URL (eg ":#BUrl=http://")
		sep = (char *) gf_fs_path_escape_colon(fsess, sURL);
		if (sep) sep[0] = 0;
		is_local = gf_url_is_local(sURL);
		if (sep) sep[0] = fsess->sep_args;

		if (for_source && is_local && !strstr(sURL, "isobmff://")) {
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

			if (strcmp(sURL, "null") && strncmp(sURL, "rand", 4) && strcmp(sURL, "-") && strcmp(sURL, "stdin") && ! gf_file_exists(sURL)) {
				char szPath[GF_MAX_PATH];
				Bool try_js = gf_fs_solve_js_script(szPath, sURL, NULL);
				if (sep) sep[0] = fsess->sep_args;
				if (frag_par) frag_par[0] = f_c;
				gf_free(sURL);

				if (try_js) {
					if (!strncmp(url, "gpac://", 7)) url += 7;
					filter = gf_fs_load_filter_internal(fsess, url, err, probe_only);
					if (probe_only) return NULL;

					if (filter) {
						//for link resolution
						if (dst_filter)	{
							if (gf_list_find(filter->destination_links, dst_filter)<0)
								gf_list_add(filter->destination_links, dst_filter);
							//to remember our connection target
							filter->target_filter = dst_filter;
						}
					}
					return filter;
				}

				if (err) *err = GF_URL_ERROR;
				return NULL;
			}
			if (frag_par) frag_par[0] = f_c;
			if (sep) sep[0] = fsess->sep_args;
		}
	}
	Bool needs_escape;
	sep = (char *)gf_fs_path_escape_colon_ex(fsess, sURL, &needs_escape, for_source);

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


	if (!for_source && dst_filter) {
		alias_for_filter = locate_alias_sink(dst_filter, sURL, mime_type);
		if (alias_for_filter) {
			candidate_freg = (GF_FilterRegister *) alias_for_filter->freg;
		}
	}

restart:
	//check all our registered filters
	count = gf_list_count(fsess->registry);
	if (candidate_freg) count = 0;
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
			else if (!for_source && !strcmp(src_dst_arg->arg_name, "dst") && !(src_dst_arg->flags&GF_FS_ARG_SINK_ALIAS)) break;
			src_dst_arg = NULL;
			j++;
		}
		if (!src_dst_arg)
			continue;

		s = freg->probe_url(sURL, mime_type);
		if (szForceExt[0] && (s==GF_FPROBE_NOT_SUPPORTED))
			s = freg->probe_url(szForceExt, mime_type);
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
		if (probe_reg) *probe_reg = candidate_freg;

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
	args = gf_malloc(sizeof(char)*5);
	
	sprintf(args, "%s%c", for_source ? "src" : "dst", fsess->sep_name);
	//path is using ':' and has options specified, inject :gpac before first option
	if (sep && needs_escape) {
		sep[0]=0;
		gf_dynstrcat(&args, sURL, NULL);
		gf_dynstrcat(&args, ":gpac", NULL);
		sep[0] = fsess->sep_args;
		gf_dynstrcat(&args, sep, NULL);
	} else {
		gf_dynstrcat(&args, sURL, NULL);
	}
	//path is using ':' and has no specified, inject :gpac: at end
	if (needs_escape && !sep && !user_args_len)
		gf_dynstrcat(&args, ":gpac:", NULL);

	if (user_args_len) {
		if (fsess->sep_args==':')
			gf_dynstrcat(&args, ":gpac:", NULL);
		else {
			char szSep[2];
			szSep[0] = fsess->sep_args;
			szSep[1] = 0;
			gf_dynstrcat(&args, szSep, NULL);
		}
		gf_dynstrcat(&args, user_args, NULL);
	}

	e = GF_OK;
	arg_type = GF_FILTER_ARG_EXPLICIT_SINK;
	if (for_source) {
		if (no_args_inherit) arg_type = GF_FILTER_ARG_EXPLICIT_SOURCE_NO_DST_INHERIT;
		else arg_type = GF_FILTER_ARG_EXPLICIT_SOURCE;
	}

	if (!for_source && !alias_for_filter && candidate_freg->use_alias) {
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
        //destroy underlying JS object - gf_filter_new_finalize always reassign it to JS_UNDEFINED
#ifdef GPAC_HAS_QJS
        jsfs_on_filter_destroyed(filter);
#endif
		if (filter->session->on_filter_create_destroy)
			filter->session->on_filter_create_destroy(filter->session->rt_udta, filter, GF_TRUE);
			
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
		if (dst_filter)
			filter->subsession_id = dst_filter->subsession_id;
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
	return gf_fs_load_source_dest_internal(fsess, url, args, parent_url, err, NULL, NULL, GF_TRUE, GF_FALSE, NULL, NULL);
}

GF_EXPORT
GF_Filter *gf_fs_load_destination(GF_FilterSession *fsess, const char *url, const char *args, const char *parent_url, GF_Err *err)
{
	return gf_fs_load_source_dest_internal(fsess, url, args, parent_url, err, NULL, NULL, GF_FALSE, GF_FALSE, NULL, NULL);
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
Bool gf_fs_forward_gf_event(GF_FilterSession *fsess, GF_Event *evt, Bool consumed, Bool skip_user)
{
	if (!fsess || fsess->in_final_flush) return GF_FALSE;

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

	if (!skip_user && !consumed && fsess->ui_event_proc) {
		Bool res;
		res = gf_fs_ui_event(fsess, evt);
		return res;
	}
	return GF_FALSE;
}

GF_EXPORT
Bool gf_filter_forward_gf_event(GF_Filter *filter, GF_Event *evt, Bool consumed, Bool skip_user)
{
	if (!filter) return GF_FALSE;
	return gf_fs_forward_gf_event(filter->session, evt, consumed, skip_user);

}

GF_EXPORT
Bool gf_filter_send_gf_event(GF_Filter *filter, GF_Event *evt)
{
	return gf_filter_forward_gf_event(filter, evt, GF_FALSE, GF_FALSE);
}


static void gf_fs_print_jsf_connection(GF_FilterSession *session, char *filter_name, GF_Filter *js_filter, void (*print_fn)(FILE *output, GF_SysPrintArgFlags flags, const char *fmt, ...) )
{
	GF_CapsBundleStore capstore;
	const char *js_name = NULL;
	GF_Err e=GF_OK;
	u32 i, j, count, nb_js_caps;
	GF_List *sources, *sinks;
	GF_FilterRegister loaded_freg;
	Bool has_output, has_input;

	if (!js_filter) {
		if (!filter_name) return;
		js_filter = gf_fs_load_filter(session, filter_name, &e);
		if (!js_filter) return;
	} else {
		if (!filter_name) filter_name = (char *) js_filter->freg->name;
	}

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
		char *type = i ? "sinks" : "sources";

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
	u32 i, j, k, count;
	u32 llev = gf_log_get_tool_level(GF_LOG_FILTER);

	gf_log_set_tool_level(GF_LOG_FILTER, GF_LOG_INFO);
	//load JS to inspect its connections
	if (filter_name && strstr(filter_name, ".js")) {
		gf_fs_print_jsf_connection(session, filter_name, NULL, print_fn);
		gf_log_set_tool_level(GF_LOG_FILTER, llev);
		return;
	}
	done = gf_list_new();
	count = gf_list_count(session->links);

	for (i=0; i<count; i++) {
		const GF_FilterRegDesc *src = gf_list_get(session->links, i);
		if (filter_name && strcmp(src->freg->name, filter_name))
			continue;

		found = GF_TRUE;
		if (print_fn)
			print_fn(stderr, 1, "%s sources:%s", src->freg->name, src->nb_edges ? "" : " none");
		else {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s sources:%s", src->freg->name, src->nb_edges ? "" : " none"));
		}
		if (!src->nb_edges)
			continue;

		for (j=0; j<src->nb_edges; j++) {
			if (gf_list_find(done, (void *) src->edges[j].src_reg->freg->name)<0) {
				if (print_fn)
					print_fn(stderr, 0, " %s", src->edges[j].src_reg->freg->name);
				else {
					GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" %s", src->edges[j].src_reg->freg->name));
				}
				gf_list_add(done, (void *) src->edges[j].src_reg->freg->name);
				if (session->flags & GF_FS_FLAG_PRINT_CONNECTIONS) {
					if (print_fn)
						print_fn(stderr, 0, " ( ", src->edges[j].src_reg->freg->name);
					else {
						GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" ( ", src->edges[j].src_reg->freg->name));
					}
					for (k=0; k<src->nb_edges; k++) {
						if (src->edges[j].src_reg != src->edges[k].src_reg) continue;;

						if (print_fn)
							print_fn(stderr, 0, "%d->%d ", src->edges[k].src_cap_idx, src->edges[k].dst_cap_idx);
						else {
							GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%d->%d ", src->edges[k].src_cap_idx, src->edges[k].dst_cap_idx));
						}

					}
					if (print_fn)
						print_fn(stderr, 0, ")", src->edges[j].src_reg->freg->name);
					else {
						GF_LOG(GF_LOG_INFO, GF_LOG_APP, (")", src->edges[j].src_reg->freg->name));
					}
				}
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
		u32 nb_sinks=0;
		if (print_fn)
			print_fn(stderr, 1, "%s sinks:", filter_name);
		else {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s sinks:", filter_name));
		}
		count = gf_list_count(session->links);
		for (i=0; i<count; i++) {
			const GF_FilterRegDesc *src = gf_list_get(session->links, i);
			if (!strcmp(src->freg->name, filter_name)) {
				if (!(src->freg->flags & GF_FS_REG_EXPLICIT_ONLY) || !(src->freg->flags & GF_FS_REG_ALLOW_CYCLIC))
					continue;
			}

			for (j=0; j<src->nb_edges; j++) {
				if (strcmp(src->edges[j].src_reg->freg->name, filter_name)) continue;

				if (gf_list_find(done, (void *) src->freg->name)<0) {
					nb_sinks++;
					if (print_fn)
						print_fn(stderr, 0, " %s", src->freg->name);
					else {
						GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" %s", src->freg->name));
					}
					gf_list_add(done, (void *) src->freg->name);
					if (session->flags & GF_FS_FLAG_PRINT_CONNECTIONS) {
						if (print_fn)
							print_fn(stderr, 0, " ( ", src->edges[j].src_reg->freg->name);
						else {
							GF_LOG(GF_LOG_INFO, GF_LOG_APP, (" ( ", src->edges[j].src_reg->freg->name));
						}
						for (k=0; k<src->nb_edges; k++) {
							if (src->edges[j].src_reg != src->edges[k].src_reg) continue;

							if (print_fn)
								print_fn(stderr, 0, "%d->%d ", src->edges[k].src_cap_idx, src->edges[k].dst_cap_idx);
							else {
								GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%d->%d ", src->edges[k].src_cap_idx, src->edges[k].dst_cap_idx));
							}
						}
						if (print_fn)
							print_fn(stderr, 0, ")", src->edges[j].src_reg->freg->name);
						else {
							GF_LOG(GF_LOG_INFO, GF_LOG_APP, (")", src->edges[j].src_reg->freg->name));
						}
				}
				}
			}
			gf_list_reset(done);
		}
		if (print_fn)
			print_fn(stderr, 1, "%s\n", nb_sinks ? " " : " none");
		else {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s\n", nb_sinks ? " " : " none"));
		}
	}

	if (!found && filter_name) {
		GF_Err e = GF_OK;
		GF_Filter *f = gf_fs_load_filter(session, filter_name, &e);
		if (f) {
			gf_fs_print_jsf_connection(session, filter_name, f, print_fn);
		}
		else if (print_fn)
			print_fn(stderr, 1, "%s filter not found\n", filter_name);
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("%s filter not found\n", filter_name));
		}
	}
	gf_list_del(done);
	gf_log_set_tool_level(GF_LOG_FILTER, llev);
}

GF_EXPORT
void gf_filter_print_all_connections(GF_Filter *filter, void (*print_fn)(FILE *output, GF_SysPrintArgFlags flags, const char *fmt, ...) )
{
	if (!filter) return;
	if (filter->freg->flags & GF_FS_REG_CUSTOM) {
		gf_fs_print_jsf_connection(filter->session, NULL, filter, print_fn);
	} else {
		gf_fs_print_all_connections(filter->session, (char *) filter->freg->name, print_fn);
	}
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

static Bool gf_fsess_get_user_pass(void *usr_cbk, Bool secure, const char *site_url, char *usr_name, char *password, gf_dm_on_usr_pass async_pass, void *async_udta)
{
	GF_Event evt;
	GF_FilterSession *fsess = (GF_FilterSession *)usr_cbk;
	evt.type = GF_EVENT_AUTHORIZATION;
	evt.auth.secure = secure;
	evt.auth.site_url = site_url;
	evt.auth.user = usr_name;
	evt.auth.password = password;
	evt.auth.on_usr_pass = async_pass;
	evt.auth.async_usr_data = async_udta;
	return gf_fs_forward_gf_event(fsess, &evt, GF_FALSE, GF_FALSE);
}

static GF_DownloadManager *gf_fs_get_download_manager(GF_FilterSession *fs)
{
	if (!fs->download_manager) {
		fs->download_manager = gf_dm_new(fs);

		gf_dm_set_auth_callback(fs->download_manager, gf_fsess_get_user_pass, fs);
	}
	return fs->download_manager;
}

GF_EXPORT
GF_DownloadManager *gf_filter_get_download_manager(GF_Filter *filter)
{
	if (!filter) return NULL;
	return gf_fs_get_download_manager(filter->session);
}

GF_EXPORT
struct _gf_ft_mgr *gf_fs_get_font_manager(GF_FilterSession *fsess)
{
#ifdef GPAC_DISABLE_PLAYER
	return NULL;
#else
	if (!fsess->font_manager) {
		fsess->font_manager = gf_font_manager_new();
	}
	return fsess->font_manager;
#endif
}

GF_EXPORT
struct _gf_ft_mgr *gf_filter_get_font_manager(GF_Filter *filter)
{
	if (!filter) return NULL;
	return gf_fs_get_font_manager(filter->session);
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
		//we duplicated the name for user tasks
		gf_free((char *) task->log_name);
		task->requeue_request = GF_FALSE;
	} else {
		assert(task->requeue_request);
		task->schedule_next_time = gf_sys_clock_high_res() + 1000*reschedule_ms;
	}
}


static GF_Err gf_fs_post_user_task_internal(GF_FilterSession *fsess, Bool (*task_execute) (GF_FilterSession *fsess, void *callback, u32 *reschedule_ms), void *udta_callback, const char *log_name, Bool force_main)
{
	GF_UserTask *utask;
	char *_log_name;
	if (!fsess || !task_execute) return GF_BAD_PARAM;
	GF_SAFEALLOC(utask, GF_UserTask);
	if (!utask) return GF_OUT_OF_MEM;
	utask->fsess = fsess;
	utask->callback = udta_callback;
	utask->task_execute = task_execute;
	//dup mem for user task
	_log_name = gf_strdup(log_name ? log_name : "user_task");
	gf_fs_post_task_ex(fsess, gf_fs_user_task, NULL, NULL, _log_name, utask, GF_FALSE, force_main, GF_FALSE, TASK_TYPE_USER);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_fs_post_user_task(GF_FilterSession *fsess, Bool (*task_execute) (GF_FilterSession *fsess, void *callback, u32 *reschedule_ms), void *udta_callback, const char *log_name)
{
	return gf_fs_post_user_task_internal(fsess, task_execute, udta_callback, log_name, fsess->force_main_thread_tasks);
}

GF_EXPORT
GF_Err gf_fs_post_user_task_main(GF_FilterSession *fsess, Bool (*task_execute) (GF_FilterSession *fsess, void *callback, u32 *reschedule_ms), void *udta_callback, const char *log_name)
{
	return gf_fs_post_user_task_internal(fsess, task_execute, udta_callback, log_name, GF_TRUE);
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
	char *_log_name=NULL;
	if (task_name) {
		_log_name = gf_strdup(task_name);
	} else {
		gf_dynstrcat(&_log_name, filter->name, NULL);
		gf_dynstrcat(&_log_name, "_task", NULL);
	}

	gf_fs_post_task_class(filter->session, gf_fs_user_task, filter, NULL, _log_name, utask, TASK_TYPE_USER);
	return GF_OK;
}


GF_EXPORT
Bool gf_fs_is_last_task(GF_FilterSession *fsess)
{
	if (!fsess) return GF_TRUE;
	if (fsess->tasks_pending>1) return GF_FALSE;
	if (gf_fq_count(fsess->main_thread_tasks)) return GF_FALSE;
	if (gf_fq_count(fsess->tasks)) return GF_FALSE;
	if (fsess->non_blocking && fsess->tasks_in_process) return GF_FALSE;
	return GF_TRUE;
}

GF_EXPORT
Bool gf_fs_in_final_flush(GF_FilterSession *fsess)
{
	if (!fsess) return GF_TRUE;
	return fsess->in_final_flush;
}
GF_EXPORT
Bool gf_fs_is_supported_mime(GF_FilterSession *fsess, const char *mime)
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
GF_Filter *gf_fs_get_filter(GF_FilterSession *session, u32 idx)
{
	return session ? gf_list_get(session->filters, idx) : NULL;
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
	gf_mx_p(f->tasks_mx);
	stats->nb_pid_in = f->num_input_pids;
	for (i=0; i<f->num_input_pids; i++) {
		GF_FilterPidInst *pidi = gf_list_get(f->input_pids, i);
		stats->nb_in_pck += pidi->nb_processed;
		if (pidi->is_end_of_stream) stats->in_eos = GF_TRUE;

		if (pidi->is_decoder_input) stats->type = GF_FS_STATS_FILTER_DECODE;
		else if (pidi->is_encoder_input) stats->type = GF_FS_STATS_FILTER_ENCODE;

		if (pidi->pid && (pidi->pid->stream_type==GF_STREAM_FILE))
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
	gf_mx_v(f->tasks_mx);
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

GF_EXPORT
GF_Filter *gf_fs_new_filter(GF_FilterSession *fsess, const char *name, u32 flags, GF_Err *e)
{
	GF_Filter *f;
	char szRegName[25];
	GF_FilterRegister *reg;

	GF_SAFEALLOC(reg, GF_FilterRegister);
	if (!reg) {
		*e = GF_OUT_OF_MEM;
		return NULL;
	}

	reg->flags = 0;
#ifndef GPAC_DISABLE_DOC
	reg->author = "custom";
	reg->description = "custom";
	reg->help = "custom";
#endif
	reg->version = "custom";
	sprintf(szRegName, "custom%p", reg);
	reg->name = gf_strdup(name ? name : szRegName);
	reg->flags = GF_FS_REG_CUSTOM | GF_FS_REG_EXPLICIT_ONLY;

	if (flags & GF_FS_REG_MAIN_THREAD)
		reg->flags |= GF_FS_REG_MAIN_THREAD;

	f = gf_filter_new(fsess, reg, NULL, NULL, 0, e, NULL, GF_FALSE);
	if (!f) return NULL;
	if (name)
		gf_filter_set_name(f, name);
	return f;
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

	if (session->ext_gl_callback) {
		e = session->ext_gl_callback(session->ext_gl_udta, GF_TRUE);
		if (e==GF_OK) gf_opengl_init();
		return e;
	}

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
	sOpt = gf_opts_get_key("temp", "window-display");
	if (sOpt) sscanf(sOpt, "%p", &os_disp_handler);

	e = session->gl_driver->Setup(session->gl_driver, NULL, os_disp_handler, GF_VOUT_INIT_HIDE);
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

GF_Err gf_fs_set_gl(GF_FilterSession *session, Bool do_activate)
{
	GF_Event evt;
	if (session->ext_gl_callback) {
		return session->ext_gl_callback(session->ext_gl_udta, do_activate);
	}

	if (!session->gl_driver) return GF_BAD_PARAM;
	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_SET_GL;
	return session->gl_driver->ProcessEvent(session->gl_driver, &evt);
}

GF_VideoOutput *gf_filter_claim_opengl_provider(GF_Filter *filter)
{
	if (!filter || !filter->session || !filter->session->gl_driver) return NULL;
	if (filter->session->ext_gl_callback) return NULL;

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
	if (filter->session->ext_gl_callback) return GF_FALSE;

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


GF_EXPORT
u32 gf_fs_get_http_max_rate(GF_FilterSession *fs)
{
	if (!fs->download_manager) {
		gf_fs_get_download_manager(fs);
		if (!fs->download_manager) return 0;
	}
	return gf_dm_get_data_rate(fs->download_manager);
}

GF_EXPORT
GF_Err gf_fs_set_http_max_rate(GF_FilterSession *fs, u32 rate)
{
	if (!fs) return GF_OK;
	if (!fs->download_manager) {
		gf_fs_get_download_manager(fs);
		if (!fs->download_manager) return GF_OUT_OF_MEM;
	}
	gf_dm_set_data_rate(fs->download_manager, rate);
	return GF_OK;
}

GF_EXPORT
u32 gf_fs_get_http_rate(GF_FilterSession *fs)
{
	if (!fs->download_manager) {
		gf_fs_get_download_manager(fs);
		if (!fs->download_manager) return 0;
	}
	return gf_dm_get_global_rate(fs->download_manager);
}

GF_EXPORT
Bool gf_fs_is_supported_source(GF_FilterSession *session, const char *url, const char *parent_url)
{
	GF_Err e;
	Bool is_supported = GF_FALSE;
	gf_fs_load_source_dest_internal(session, url, NULL, parent_url, &e, NULL, NULL, GF_TRUE, GF_TRUE, &is_supported, NULL);
	return is_supported;
}


GF_EXPORT
Bool gf_fs_fire_event(GF_FilterSession *fs, GF_Filter *f, GF_FilterEvent *evt, Bool upstream)
{
	Bool ret = GF_FALSE;
	if (!fs || !evt) return GF_FALSE;

	GF_FilterPid *on_pid = evt->base.on_pid;
	evt->base.on_pid = NULL;
	if (f) {
		if (evt->base.type==GF_FEVT_USER) {
			if (f->freg->process_event && f->event_target) {
				gf_mx_p(f->tasks_mx);
				f->freg->process_event(f, evt);
				gf_mx_v(f->tasks_mx);
				ret = GF_TRUE;
			}
		}
		if (!ret) {
			gf_mx_p(f->tasks_mx);
			if (f->num_output_pids && upstream) ret = GF_TRUE;
			else if (f->num_input_pids && !upstream) ret = GF_TRUE;
			if (ret)
				gf_filter_send_event(f, evt, upstream);
			gf_mx_v(f->tasks_mx);
		}
	} else {
		u32 i, count;
		gf_fs_lock_filters(fs, GF_TRUE);
		count = gf_list_count(fs->filters);
		for (i=0; i<count; i++) {
			Bool canceled;
			f = gf_list_get(fs->filters, i);
			if (f->disabled || f->removed) continue;
			if (f->multi_sink_target) continue;
			if (!f->freg->process_event) continue;
			if (!f->event_target) continue;

			gf_mx_p(f->tasks_mx);
			canceled = f->freg->process_event(f, evt);
			gf_mx_v(f->tasks_mx);
			ret = GF_TRUE;
			if (canceled) break;
		}
		gf_fs_lock_filters(fs, GF_FALSE);
	}
	evt->base.on_pid = on_pid;
	return ret;
}

GF_EXPORT
GF_Err gf_fs_set_filter_creation_callback(GF_FilterSession *session, gf_fs_on_filter_creation on_create_destroy, void *udta, Bool force_sync)
{
	if (!session) return GF_BAD_PARAM;
	session->rt_udta = udta;
	session->on_filter_create_destroy = on_create_destroy;
	session->force_main_thread_tasks = force_sync;
	return GF_OK;
}

GF_EXPORT
void *gf_fs_get_rt_udta(GF_FilterSession *session)
{
	if (!session) return NULL;
	return session->rt_udta;
}

GF_EXPORT
GF_Err gf_fs_set_external_gl_provider(GF_FilterSession *session, gf_fs_gl_activate on_gl_activate, void *udta)
{
	if (!session || !on_gl_activate || session->ext_gl_callback) return GF_BAD_PARAM;
	if (gf_list_count(session->filters)) return GF_BAD_PARAM;
	session->ext_gl_udta = udta;
	session->ext_gl_callback = on_gl_activate;
	return GF_OK;
}
#ifdef GF_FS_ENABLE_LOCALES

static Bool fsess_find_res(GF_FSLocales *loc, char *parent, char *path, char *relocated_path, char *localized_rel_path)
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
static Bool fs_check_locales(void *__self, const char *locales_parent_path, const char *rel_path, char *relocated_path, char *localized_rel_path)
{
	char path[GF_MAX_PATH];
	const char *opt;

	GF_FSLocales *loc = (GF_FSLocales*)__self;

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
		if (fsess_find_res(loc, (char *) locales_parent_path, (char *) path, relocated_path, localized_rel_path))
			return 1;

		/*recursively remove region (sub)tags*/
		while (1) {
			sep = strrchr(lan, '-');
			if (!sep) break;
			sep[0] = 0;
			sprintf(path, "locales/%s/%s", lan, rel_path);
			if (fsess_find_res(loc, (char *) locales_parent_path, (char *) path, relocated_path, localized_rel_path))
				return 1;
		}
	}

	if (fsess_find_res(loc, (char *) locales_parent_path, (char *) rel_path, relocated_path, localized_rel_path))
		return 1;
	/* if we did not find the localized file, both the relocated and localized strings are NULL */
	strcpy(localized_rel_path, "");
	strcpy(relocated_path, "");
	return 0;
}
#endif

static Bool gf_fs_relocate_url(GF_FilterSession *session, const char *service_url, const char *parent_url, char *out_relocated_url, char *out_localized_url)
{
#ifdef GF_FS_ENABLE_LOCALES
	u32 i, count;

	count = gf_list_count(session->uri_relocators);
	for (i=0; i<count; i++) {
		Bool result;
		GF_URIRelocator *uri_relocator = gf_list_get(session->uri_relocators, i);
		result = uri_relocator->relocate_uri(uri_relocator, parent_url, service_url, out_relocated_url, out_localized_url);
		if (result) return 1;
	}
#endif
	return 0;
}

GF_EXPORT
Bool gf_filter_relocate_url(GF_Filter *filter, const char *service_url, const char *parent_url, char *out_relocated_url, char *out_localized_url)
{
	if (!filter) return 0;
	return gf_fs_relocate_url(filter->session, service_url, parent_url, out_relocated_url, out_localized_url);
}

