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

#include "filter_session.h"
#include <gpac/constants.h>

void pcki_del(GF_FilterPacketInstance *pcki)
{
	if (safe_int_dec(&pcki->pck->reference_count) == 0) {
		gf_filter_packet_destroy(pcki->pck);
	}
	gf_free(pcki);
}

void gf_filter_pid_inst_reset(GF_FilterPidInst *pidinst)
{
	assert(pidinst);
	while (gf_fq_count(pidinst->packets)) {
		GF_FilterPacketInstance *pcki = gf_fq_pop(pidinst->packets);
		pcki_del(pcki);
	}

	while (gf_list_count(pidinst->pck_reassembly)) {
		GF_FilterPacketInstance *pcki = gf_list_pop_back(pidinst->pck_reassembly);
		pcki_del(pcki);
	}
}

void gf_filter_pid_inst_del(GF_FilterPidInst *pidinst)
{
	assert(pidinst);
	gf_filter_pid_inst_reset(pidinst);

	gf_fq_del(pidinst->packets, (gf_destruct_fun) pcki_del);
	gf_mx_del(pidinst->pck_mx);
	gf_list_del(pidinst->pck_reassembly);
	if (pidinst->props && safe_int_dec(&pidinst->props->reference_count) == 0)
		gf_props_del(pidinst->props);
	gf_free(pidinst);
}

static GF_FilterPidInst *gf_filter_pid_inst_new(GF_Filter *filter, GF_FilterPid *pid)
{
	GF_FilterPidInst *pidinst;
	GF_SAFEALLOC(pidinst, GF_FilterPidInst);
	pidinst->pid = pid;
	pidinst->filter = filter;

	if (filter->session->use_locks) {
		char szName[200];
		u32 pid_idx = 1 + gf_list_find(pid->filter->output_pids, pid);
		u32 dst_idx = 1 + pid->num_destinations;
		snprintf(szName, 200, "F%sPid%dDest%dPackets", filter->name, pid_idx, dst_idx);
		pidinst->pck_mx = gf_mx_new(szName);
	}

	pidinst->packets = gf_fq_new(pidinst->pck_mx);

	pidinst->pck_reassembly = gf_list_new();
	pidinst->last_block_ended = GF_TRUE;
	return pidinst;
}

static void gf_filter_pid_check_unblock(GF_FilterPid *pid)
{
	Bool unblock=GF_FALSE;


	//we block according to the number of dispatched units (decoder output) or to the requested buffer duration
	//for other streams - unblock accordingly
	if (pid->max_buffer_unit) {
		if (pid->nb_buffer_unit * GF_FILTER_SPEED_SCALER < pid->max_buffer_unit * pid->playback_speed_scaler) {
			unblock=GF_TRUE;
		}
	} else if (pid->buffer_duration * GF_FILTER_SPEED_SCALER < pid->max_buffer_time * pid->playback_speed_scaler) {
		unblock=GF_TRUE;
	}

	if (pid->would_block && unblock) {
		//todo needs Compare&Swap
		safe_int_dec(&pid->would_block);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s unblocked\n", pid->pid->filter->name, pid->pid->name));
		assert(pid->filter->would_block);
		safe_int_dec(&pid->filter->would_block);
		if (pid->filter->would_block < pid->filter->num_output_pids) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s has only %d / %d blocked pids, requesting process task\n", pid->filter->name, pid->filter->would_block, pid->filter->num_output_pids));
			//requeue task
			if (!pid->filter->skip_process_trigger_on_tasks)
				gf_filter_post_process_task(pid->filter);
		}
	}
}

static void gf_filter_pid_inst_check_dependencies(GF_FilterPidInst *pidi)
{
	const GF_PropertyValue *p;
	u32 i, dep_id = 0;
	GF_FilterPid *pid = pidi->pid;
	GF_Filter *filter = pid->filter;

	//check pid dependency
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
	if (p) dep_id = p->value.uint;

	if (!dep_id) return;

	for (i=0; i<filter->num_output_pids; i++) {
		u32 j;
		GF_FilterPid *a_pid = gf_list_get(filter->output_pids, i);
		if (a_pid==pid) continue;
		p = gf_filter_pid_get_property(a_pid, GF_PROP_PID_ID);
		if (!p) p = gf_filter_pid_get_property(a_pid, GF_PROP_PID_ESID);
		if (!p || (p->value.uint != dep_id)) continue;

		for (j=0; j<a_pid->num_destinations; j++) {
			GF_FilterPidInst *a_pidi = gf_list_get(a_pid->destinations, j);
			if (a_pidi == pidi) continue;
			if (! a_pidi->is_decoder_input) continue;

			if (a_pidi->filter == pidi->filter) continue;

			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Filter %s PID %s connected to decoder %s, but dependent stream %s connected to %s - switching pid destination\n", a_pid->filter->name, a_pid->name, a_pidi->filter->name, pidi->pid->name, pidi->filter->name));

			//disconnect this pid instance from its current decoder
			gf_fs_post_task(filter->session, gf_filter_pid_disconnect_task, a_pidi->filter, a_pid, "pidinst_disconnect", NULL);

			//reconnect this pid instance to the new decoder
			safe_int_inc(&pid->filter->out_pid_connection_pending);
			gf_filter_pid_post_connect_task(pidi->filter, a_pid);

		}
	}
}

static void gf_filter_pid_update_caps(GF_FilterPid *pid)
{
	u32 mtype=0, codecid=0;
	u32 i, count;
	const GF_PropertyValue *p;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (p) codecid = p->value.uint;

	//by default all buffers are 1ms max
	pid->max_buffer_time = pid->filter->session->default_pid_buffer_max_us;
	if (codecid!=GF_CODECID_RAW)
		return;
	pid->raw_media = GF_TRUE;

	if (pid->user_max_buffer_time) pid->max_buffer_time = pid->user_max_buffer_time;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (p) mtype = p->value.uint;

	//output is a decoded raw stream: if some input has same type but different codecid this is a decoder
	//set input buffer size
	count=pid->filter->num_input_pids;
	for (i=0; i<count; i++) {
		u32 i_codecid=0, i_type=0;
		GF_FilterPidInst *pidi = gf_list_get(pid->filter->input_pids, i);

		p = gf_filter_pid_get_property(pidi->pid, GF_PROP_PID_STREAM_TYPE);
		if (p) i_type = p->value.uint;

		p = gf_filter_pid_get_property(pidi->pid, GF_PROP_PID_CODECID);
		if (p) i_codecid = p->value.uint;

		//same stream type but changing format type: this is a decoder input pid, set buffer req
		if ((mtype==i_type) && (codecid != i_codecid)) {
			//default decoder buffer
			if (pidi->pid->user_max_buffer_time)
				pidi->pid->max_buffer_time = pidi->pid->user_max_buffer_time;
			else
				pidi->pid->max_buffer_time = pidi->pid->filter->session->decoder_pid_buffer_max_us;


			if (mtype==GF_STREAM_VISUAL) {
				pid->max_buffer_unit = 4;
			} else if (mtype==GF_STREAM_AUDIO) {
				pid->max_buffer_unit = 20;
			}

			if (!pidi->is_decoder_input) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s pid instance %s marked as decoder input\n",  pidi->pid->filter->name, pidi->pid->name));
				pidi->is_decoder_input = GF_TRUE;
				safe_int_inc(&pidi->pid->nb_decoder_inputs);

				if ((i_type == GF_STREAM_AUDIO) || (i_type == GF_STREAM_VISUAL))
					gf_filter_pid_inst_check_dependencies(pidi);
			}
		}
	}
}

#define TASK_REQUEUE(_t) \
	_t->requeue_request = GF_TRUE; \
	_t->schedule_next_time = gf_sys_clock_high_res() + 50; \

void gf_filter_pid_inst_delete_task(GF_FSTask *task)
{
	u64 dur;
	GF_FilterPid *pid = task->pid;
	GF_FilterPidInst *pidinst = task->udta;
	GF_Filter *filter = pid->filter;

	//reset in process
	if ((pidinst->filter && pidinst->discard_packets) || filter->stream_reset_pending) {
		TASK_REQUEUE(task)
		return;
	}
	//we still have packets out there!
	if (pidinst->pid->nb_shared_packets_out) {
		TASK_REQUEUE(task)
		return;
	}

	//WARNING at this point pidinst->filter may be destroyed
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s pid instance %s destruction\n",  filter->name, pid->name));
	gf_list_del_item(pid->destinations, pidinst);
	pid->num_destinations = gf_list_count(pid->destinations);

	dur = pidinst->buffer_duration;
	if (pidinst->is_decoder_input) {
		assert(pid->nb_decoder_inputs);
		safe_int_dec(&pid->nb_decoder_inputs);
	}
	gf_filter_pid_inst_del(pidinst);

	if (dur) {
		pid->buffer_duration = 0;
		//update blocking state
		if (pid->would_block)
			gf_filter_pid_check_unblock(pid);
		else
			gf_filter_pid_would_block(pid);
	}

	//if filter still has input pids, another filter is still connected to it so we cannot destroy the pid
	if (gf_list_count(filter->input_pids)) {
		return;
	}
	//no more pids on filter, destroy it
	if (! pid->num_destinations ) {
		gf_list_del_item(filter->output_pids, pid);
		filter->num_output_pids = gf_list_count(filter->output_pids);
		gf_filter_pid_del(pid);
	}
	if (!gf_list_count(filter->output_pids) && !gf_list_count(filter->input_pids)) {
		assert(!filter->finalized);
		filter->finalized = GF_TRUE;
		gf_fs_post_task(filter->session, gf_filter_remove_task, filter, NULL, "filter_destroy", NULL);
	}
}


typedef enum {
	GF_PID_CONF_CONNECT,
	GF_PID_CONF_RECONFIG,
	GF_PID_CONF_REMOVE,
} GF_PidConnectType;

static GF_Err gf_filter_pid_configure(GF_Filter *filter, GF_FilterPid *pid, GF_PidConnectType ctype)
{
	u32 i, count;
	GF_Err e;
	Bool new_pid_inst=GF_FALSE;
	GF_FilterPidInst *pidinst=NULL;

	assert(filter->freg->configure_pid);

	if (filter->detached_pid_inst) {
		count = gf_list_count(filter->detached_pid_inst);
		for (i=0; i<count; i++) {
			pidinst = gf_list_get(filter->detached_pid_inst, i);
			if (pidinst->filter==filter) {
				gf_list_rem(filter->detached_pid_inst, i);
				//reattach new filter and pid
				pidinst->filter = filter;
				pidinst->pid = pid;

				assert(!pidinst->props);

				//and treat as new pid inst
				new_pid_inst=GF_TRUE;
				break;
			}
			pidinst=NULL;
		}
		if (! gf_list_count(filter->detached_pid_inst)) {
			gf_list_del(filter->detached_pid_inst);
			filter->detached_pid_inst = NULL;
		}
	}
	count = pid->num_destinations;
	for (i=0; i<count; i++) {
		pidinst = gf_list_get(pid->destinations, i);
		if (pidinst->filter==filter) {
			break;
		}
		pidinst=NULL;
	}

	//first connection of this PID to this filter
	if (!pidinst) {
		if (ctype>=GF_PID_CONF_REMOVE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Trying to disconnect PID %s not found in filter %s inputs\n",  pid->name, filter->name));
			return GF_SERVICE_ERROR;
		}
		pidinst = gf_filter_pid_inst_new(filter, pid);
		new_pid_inst=GF_TRUE;
	}

	//if new, add the PID to input/output before calling configure
	if (new_pid_inst) {
		assert(pidinst);
		gf_list_add(pid->destinations, pidinst);
		pid->num_destinations = gf_list_count(pid->destinations);

		if (!filter->input_pids) filter->input_pids = gf_list_new();
		gf_list_add(filter->input_pids, pidinst);
		filter->num_input_pids = gf_list_count(filter->input_pids);
		//new connection, update caps in case we have events using caps (buffer req) being sent
		//while processing the configure (they would be dispatched on the source filter, not the dest one being
		//processed here)
		gf_filter_pid_update_caps(pid);
	}

	//commented out for now, due to audio thread pulling packets out of the pid but not in the compositor:process, which
	//could be called for video at the same time... FIXME
#ifdef FILTER_FIXME
	FSESS_CHECK_THREAD(filter)
#endif
	e = filter->freg->configure_pid(filter, (GF_FilterPid*) pidinst, (ctype==GF_PID_CONF_REMOVE) ? GF_TRUE : GF_FALSE);

#ifdef GPAC_MEMORY_TRACKING
	if (filter->session->check_allocs) {
		if (filter->nb_consecutive_process >= filter->max_nb_consecutive_process) {
			filter->max_nb_consecutive_process = filter->nb_consecutive_process;
			filter->max_nb_process = filter->nb_process_since_reset;
			filter->max_stats_nb_alloc = filter->stats_nb_alloc;
			filter->max_stats_nb_calloc = filter->stats_nb_calloc;
			filter->max_stats_nb_realloc = filter->stats_nb_realloc;
			filter->max_stats_nb_free = filter->stats_nb_free;
		}
		filter->stats_mem_allocated = 0;
		filter->stats_nb_alloc = filter->stats_nb_realloc = filter->stats_nb_free = 0;
		filter->nb_process_since_reset = filter->nb_consecutive_process = 0;
	}
#endif

	if (e==GF_OK) {
		//if new, register the new pid instance, and the source pid as input to this filer
		if (new_pid_inst) {
			GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Connected filter %s PID %s to filter %s\n", pid->filter->name,  pid->name, filter->name));
		}
	} else {
		//error,  remove from input
		gf_list_del_item(filter->input_pids, pidinst);
		gf_list_del_item(pidinst->pid->destinations, pidinst);
		pidinst->pid->num_destinations = gf_list_count(pidinst->pid->destinations);
		pidinst->filter = NULL;
		filter->num_input_pids = gf_list_count(filter->input_pids);
		
		//if connect and error, direct delete of pid
		if (new_pid_inst) {
			gf_list_del_item(pid->destinations, pidinst);
			pid->num_destinations = gf_list_count(pid->destinations);
			gf_filter_pid_inst_del(pidinst);
		}

		if (e==GF_REQUIRES_NEW_INSTANCE) {
			//TODO: copy over args from current filter
			GF_Filter *new_filter = gf_filter_clone(filter);
			if (new_filter) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Clone filter %s, new instance for pid %s\n", filter->name, pid->name));
				gf_filter_pid_post_connect_task(new_filter, pid);
				return GF_OK;
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to clone filter %s\n", filter->name));
				e = GF_OUT_OF_MEM;
			}
		}
		if (e && (ctype==GF_PID_CONF_REMOVE)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to disconnect filter %s PID %s from filter %s: %s\n", pid->filter->name, pid->name, filter->name, gf_error_to_string(e) ));
		}
		else if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to connect filter %s PID %s to filter %s: %s\n", pid->filter->name, pid->name, filter->name, gf_error_to_string(e) ));

			if (filter->has_out_caps) {
				Bool unload_filter = GF_TRUE;
				//try to load another filter to handle that connection
				//1-blacklist this filter
				gf_list_add(pid->filter->blacklisted, (void *) filter->freg);
				//2-disconnect all other inputs, and post a re-init
				while (gf_list_count(filter->input_pids)) {
					GF_FilterPidInst *a_pidinst = gf_list_pop_back(filter->input_pids);
					FSESS_CHECK_THREAD(filter)
					filter->freg->configure_pid(filter, (GF_FilterPid *) a_pidinst, GF_TRUE);

					gf_filter_pid_post_init_task(a_pidinst->pid->filter, a_pidinst->pid);

					gf_fs_post_task(filter->session, gf_filter_pid_inst_delete_task, a_pidinst->pid->filter, a_pidinst->pid, "pid_inst_delete", a_pidinst);

					unload_filter = GF_FALSE;
				}
				if (!filter->session->last_connect_error) filter->session->last_connect_error = e;
				if (ctype==GF_PID_CONF_CONNECT) {
					assert(pid->filter->out_pid_connection_pending);
					safe_int_dec(&pid->filter->out_pid_connection_pending);
				}
				//3- post a re-init on this pid
				gf_filter_pid_post_init_task(pid->filter, pid);

				if (unload_filter) {
					assert(!filter->finalized);
					filter->finalized = GF_TRUE;
					assert(!gf_list_count(filter->input_pids));
					gf_fs_post_task(filter->session, gf_filter_remove_task, filter, NULL, "filter_destroy", NULL);
				}
				return e;
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to reconfigure input of sink %s, cannot rebuild graph\n", filter->name));
			}
		} else {
			filter->session->last_connect_error = GF_OK;
		}

		//try to run filter no matter what
		if (filter->session->requires_solved_graph )
			return e;
	}

	//flush all pending pid init requests following the call to init
	if (filter->has_pending_pids) {
		filter->has_pending_pids = GF_FALSE;
		while (gf_fq_count(filter->pending_pids)) {
			GF_FilterPid *pid=gf_fq_pop(filter->pending_pids);
			gf_filter_pid_post_init_task(filter, pid);
		}
	}

	if (ctype==GF_PID_CONF_REMOVE) {
		gf_list_del_item(filter->input_pids, pidinst);
		gf_list_del_item(pidinst->pid->destinations, pidinst);
		pidinst->pid->num_destinations = gf_list_count(pidinst->pid->destinations);
		pidinst->filter = NULL;
		filter->num_input_pids = gf_list_count(filter->input_pids);

		//disconnected the last input, flag as removed
		if (!filter->num_input_pids && !filter->sticky)
			filter->removed = GF_TRUE;
		//post a pid_delete task to also trigger removal of the filter if needed
		gf_fs_post_task(filter->session, gf_filter_pid_inst_delete_task, pid->filter, pid, "pid_inst_delete", pidinst);

		return e;
	}

	if (ctype==GF_PID_CONF_CONNECT) {
		assert(pid->filter->out_pid_connection_pending);
		if ( (safe_int_dec(&pid->filter->out_pid_connection_pending)==0) ) {

			//postponed packets dispatched by source while setting up PID, flush through process()
			//pending packets (not yet consumed but in PID buffer), start processing
			if (pid->filter->postponed_packets || pid->filter->pending_packets || pid->filter->nb_caps_renegociate) {
				gf_filter_post_process_task(pid->filter);
			}
		}
	}
	//once all pid have been (re)connected, update any internal caps
	gf_filter_pid_update_caps(pid);
	return e;
}

static void gf_filter_pid_connect_task(GF_FSTask *task)
{
	GF_Filter *filter = task->filter;
	GF_FilterSession *fsess = filter->session;
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s pid %s connecting to %s\n", task->pid->pid->filter->name, task->pid->pid->name, task->filter->name));

	//filter will require a new instance, clone it
	if (filter->num_input_pids && (filter->max_extra_pids <= filter->num_input_pids - 1)) {
		GF_Filter *new_filter = gf_filter_clone(filter);
		if (new_filter) {
			filter = new_filter;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to clone filter %s\n", filter->name));
			safe_int_dec(&filter->in_pid_connection_pending);
			return;
		}
	}
	gf_filter_pid_configure(filter, task->pid->pid, GF_PID_CONF_CONNECT);
	//once connected, any set_property before the first packet dispatch will have to trigger a reconfigure
	task->pid->pid->request_property_map = GF_TRUE;

	//filter may now be the clone, decrement on original filter
	safe_int_dec(&task->filter->in_pid_connection_pending);

	gf_fs_cleanup_filters(fsess);

}

void gf_filter_pid_reconfigure_task(GF_FSTask *task)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s pid %s reconfigure to %s\n", task->pid->pid->filter->name, task->pid->pid->name, task->filter->name));

	gf_filter_pid_configure(task->filter, task->pid->pid, GF_PID_CONF_RECONFIG);
}

void gf_filter_pid_disconnect_task(GF_FSTask *task)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s pid %s disconnect from %s\n", task->pid->pid->filter->name, task->pid->pid->name, task->filter->name));
	gf_filter_pid_configure(task->filter, task->pid->pid, GF_PID_CONF_REMOVE);

	//if the filter has no more connected ins and outs, remove it
	if (task->filter->removed && !gf_list_count(task->filter->output_pids) && !gf_list_count(task->filter->input_pids)) {
		Bool direct_mode = task->filter->session->direct_mode;
		assert(!task->filter->finalized);
		task->filter->finalized = GF_TRUE;
		gf_fs_post_task(task->filter->session, gf_filter_remove_task, task->filter, NULL, "filter_destroy", NULL);
		if (direct_mode) task->filter = NULL;
	}
}

void gf_filter_pid_detach_task(GF_FSTask *task)
{
	u32 i, count;
	GF_Filter *filter = task->filter;
	GF_FilterPid *pid = task->pid->pid;
	GF_FilterPidInst *pidinst=NULL;

	//we may have concurrent reset (due to play/stop/seek) and caps renegotiation
	//wait for the pid to be reset before detaching
	if (pid->filter->stream_reset_pending) {
		TASK_REQUEUE(task)
		return;
	}
	assert(filter->sticky);
	assert(filter->freg->configure_pid);
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s pid %s disconnect from %s\n", task->pid->pid->filter->name, task->pid->pid->name, task->filter->name));

	count = pid->num_destinations;
	for (i=0; i<count; i++) {
		pidinst = gf_list_get(pid->destinations, i);
		if (pidinst->filter==filter) {
			break;
		}
		pidinst=NULL;
	}

	//first connection of this PID to this filter
	if (!pidinst) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Trying to detach PID %s not found in filter %s inputs\n",  pid->name, filter->name));
		return;
	}
	//detach props
	if (pidinst->props && (safe_int_dec(& pidinst->props->reference_count) == 0) ) {
		gf_list_del_item(pidinst->pid->properties, pidinst->props);
		gf_props_del(pidinst->props);
	}
	pidinst->props = NULL;

	//detach pid
	gf_filter_pid_inst_reset(pidinst);
	pidinst->pid = NULL;
	gf_list_del_item(pid->destinations, pidinst);
	pid->num_destinations = gf_list_count(pid->destinations);
	gf_list_del_item(filter->input_pids, pidinst);
	filter->num_input_pids = gf_list_count(filter->input_pids);

	if (!filter->detached_pid_inst) {
		filter->detached_pid_inst = gf_list_new();
	}
	gf_list_add(filter->detached_pid_inst, pidinst);
}


void gf_filter_pid_set_name(GF_FilterPid *pid, const char *name)
{
	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Attempt to assign name %s to input PID %s in filter %s - ignoring\n", name, pid->pid->name, pid->pid->filter->name));
	} else if (name) {
		if (pid->name) gf_free(pid->name);
		pid->name = gf_strdup(name);
	}
}
const char *gf_filter_pid_get_name(GF_FilterPid *pid)
{
	return pid->pid->name;
}
const char *gf_filter_pid_get_filter_name(GF_FilterPid *pid)
{
	if (PID_IS_INPUT(pid)) {
		return pid->pid->filter->name;
	}
	return pid->filter->name;
}
const char *gf_filter_pid_original_args(GF_FilterPid *pid)
{
	return pid->pid->filter->src_args ? pid->pid->filter->src_args : pid->pid->filter->orig_args;
}

void gf_filter_pid_get_buffer_occupancy(GF_FilterPid *pid, u32 *max_slots, u32 *nb_pck, u32 *max_duration, u32 *duration)
{
	if (max_slots) *max_slots = pid->pid->max_buffer_unit;
	if (nb_pck) *nb_pck = pid->pid->nb_buffer_unit;
	if (max_duration) *max_duration = pid->pid->max_buffer_time;
	if (duration) *duration = pid->pid->buffer_duration;
}

void gf_filter_pid_set_udta(GF_FilterPid *pid, void *udta)
{
	if (PID_IS_INPUT(pid)) {
		((GF_FilterPidInst *)pid)->udta = udta;
	} else {
		pid->udta = udta;
	}
}
void *gf_filter_pid_get_udta(GF_FilterPid *pid)
{
	if (PID_IS_INPUT(pid)) {
		return ((GF_FilterPidInst *)pid)->udta;
	} else {
		return pid->udta;
	}
}


static Bool filter_source_id_match(GF_FilterPid *src_pid, const char *id, const char *source_ids, Bool *pid_excluded)
{
	*pid_excluded = GF_FALSE;
	if (!source_ids)
		return GF_TRUE;
	if (!id)
		return GF_FALSE;
	while (source_ids) {
		u32 len, sublen;
		Bool last=GF_FALSE;
		char *sep = strchr(source_ids, src_pid->filter->session->sep_list);
		char *pid_name;
		if (sep) {
			len = sep - source_ids;
		} else {
			len = strlen(source_ids);
			last=GF_TRUE;
		}

		pid_name = strchr(source_ids, src_pid->filter->session->sep_frag);
		if (pid_name > source_ids + len) pid_name = NULL;
		sublen = pid_name ? pid_name - source_ids : len;
		//skip frag char
		if (pid_name) pid_name++;

		//match id
		if (!strncmp(id, source_ids, sublen)) {
			char *psep;
			u32 comp_type=0;
			const GF_PropertyValue *prop;
			if (!pid_name) return GF_TRUE;

			//match pid name
			if (!strcmp(src_pid->name, pid_name)) return GF_TRUE;

			//generic property adressing code(or builtin name)=val
			psep = strchr(pid_name, src_pid->filter->session->sep_name);
			if (!psep) {
				psep = strchr(pid_name, '-');
				if (psep) comp_type = 1;
				else {
					psep = strchr(pid_name, '+');
					if (psep) comp_type = 2;
				}
			}
			if (psep) {
				Bool is_equal = GF_FALSE;
				GF_PropertyValue prop_val;
				u32 p4cc = 0;
				char c=psep[0];
				psep[0] = 0;
				prop=NULL;
				p4cc = gf_props_get_id(pid_name);
				if (!p4cc && (strlen(pid_name)==4))
					p4cc = GF_4CC(pid_name[0], pid_name[1], pid_name[2], pid_name[3]);

				psep[0] = c;
				if (p4cc) {
					prop = gf_filter_pid_get_property(src_pid, p4cc);

					if (prop) {
						prop_val = gf_props_parse_value(prop->type, pid_name, psep+1, NULL);
						if (!comp_type) {
							is_equal = gf_props_equal(prop, &prop_val);
						} else {
							switch (prop_val.type) {
							case GF_PROP_SINT:
								if (prop->value.sint<prop_val.value.sint) is_equal = GF_TRUE;
								if (comp_type==2) is_equal = !is_equal;
								break;
							case GF_PROP_UINT:
								if (prop->value.uint<prop_val.value.uint) is_equal = GF_TRUE;
								if (comp_type==2) is_equal = !is_equal;
								break;
							case GF_PROP_LSINT:
								if (prop->value.longsint<prop_val.value.longsint) is_equal = GF_TRUE;
								if (comp_type==2) is_equal = !is_equal;
								break;
							case GF_PROP_LUINT:
								if (prop->value.longuint<prop_val.value.longuint) is_equal = GF_TRUE;
								if (comp_type==2) is_equal = !is_equal;
								break;
							case GF_PROP_FLOAT:
								if (prop->value.fnumber<prop_val.value.fnumber) is_equal = GF_TRUE;
								if (comp_type==2) is_equal = !is_equal;
								break;
							case GF_PROP_DOUBLE:
								if (prop->value.number<prop_val.value.number) is_equal = GF_TRUE;
								if (comp_type==2) is_equal = !is_equal;
								break;
							case GF_PROP_FRACTION:
								if (prop->value.frac.num * prop_val.value.frac.den < prop->value.frac.den * prop_val.value.frac.num) is_equal = GF_TRUE;
								if (comp_type==2) is_equal = !is_equal;
								break;
							default:
								GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("PID adressing uses \'%s\' comparison on property %s which is not a number, defaulting to equal\n", (comp_type==1) ? "less than" : "more than", gf_props_4cc_get_name(p4cc) ));
								is_equal = GF_TRUE;
								break;
							}
						}
						gf_props_reset_single(&prop_val);
						if (!is_equal) *pid_excluded = GF_TRUE;
					} else {
						//if the property is not found, we accept the connection
						is_equal = GF_TRUE;
					}
					return is_equal;
				}
			}

			//special case for stream types filters
			prop = gf_filter_pid_get_property(src_pid, GF_PROP_PID_STREAM_TYPE);
			if (prop) {
				u32 matched=0;
				u32 type=0;
				if (!strnicmp(pid_name, "audio", 5)) {
					matched=5;
					type=GF_STREAM_AUDIO;
				} else if (!strnicmp(pid_name, "video", 5)) {
					matched=5;
					type=GF_STREAM_VISUAL;
				} else if (!strnicmp(pid_name, "scene", 5)) {
					matched=5;
					type=GF_STREAM_SCENE;
				} else if (!strnicmp(pid_name, "font", 4)) {
					matched=4;
					type=GF_STREAM_FONT;
				} else if (!strnicmp(pid_name, "text", 4)) {
					matched=4;
					type=GF_STREAM_TEXT;
				}
				if (matched && (type != prop->value.uint)) {
					//special case: if we request a non-file stream but the pid is a file, we will need a demux to
					//move from file to A/V/... streams, so we accept any #MEDIA from file streams
					if (prop->value.uint == GF_STREAM_FILE) {
						return GF_TRUE;
					}
					matched = 0;
				}

				if (matched) {
					u32 idx=0;
					u32 k, count_pid;
					if (strlen(pid_name)==matched) return GF_TRUE;
					idx = atoi(pid_name+matched);
					count_pid = src_pid->filter->num_output_pids;
					for (k=0; k<count_pid; k++) {
						GF_FilterPid *p = gf_list_get(src_pid->filter->output_pids, k);
						prop = gf_filter_pid_get_property(src_pid, GF_PROP_PID_STREAM_TYPE);
						if (prop && prop->value.uint==type) {
							idx--;
							if (!idx) {
								if (p==src_pid) return GF_TRUE;
								break;
							}
						}
					}
					*pid_excluded = GF_TRUE;
				}
			}

			if (!prop) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Unsupported PID adressing %c%s in filter %s\n", src_pid->filter->session->sep_frag, pid_name, src_pid->filter->name));
			}
		}


		source_ids += len;
		if (last) break;
	}
	return GF_FALSE;
}

Bool filter_in_parent_chain(GF_Filter *parent, GF_Filter *filter)
{
	u32 i, count;
	if (parent == filter) return GF_TRUE;
	//browse all parent PIDs
	count = parent->num_input_pids;
	if (!count) return GF_FALSE;
	for (i=0; i<count; i++) {
		GF_FilterPidInst *pid = gf_list_get(parent->input_pids, i);
		if (filter_in_parent_chain(pid->pid->filter, filter)) return GF_TRUE;
	}
	return GF_FALSE;
}

static Bool filter_pid_caps_match(GF_FilterPid *src_pid, const GF_FilterRegister *freg, GF_Filter *filter_inst, u8 *priority, u32 *dst_bundle_idx, GF_Filter *dst_filter, s32 for_bundle_idx)
{
	u32 i=0;
	u32 cur_bundle_start = 0;
	u32 cap_bundle_idx = 0;
	u32 nb_subcaps=0;
	Bool skip_explicit_load = GF_FALSE;
	Bool all_caps_matched = GF_TRUE;
	const GF_FilterCapability *in_caps;
	u32 nb_in_caps;
	if (!freg) {
		assert(dst_filter);
		freg = dst_filter->freg;
		skip_explicit_load = GF_TRUE;
	}
	in_caps = freg->caps;
	nb_in_caps = freg->nb_caps;
	if (filter_inst && filter_inst->forced_caps && (filter_inst->freg==freg)) {
		in_caps = filter_inst->forced_caps;
		nb_in_caps = filter_inst->nb_forced_caps;
	}

	if (priority)
		(*priority) = freg->priority;

	if (dst_bundle_idx)
		(*dst_bundle_idx) = 0;

	//filters with no explicit input cap accept anything for now, this should be refined ...
	if (!in_caps)
		return GF_TRUE;

	//check all input caps of dst filter
	for (i=0; i<nb_in_caps; i++) {
		const GF_PropertyValue *pid_cap=NULL;
		const GF_FilterCapability *cap = &in_caps[i];

		if (i && !(cap->flags & GF_FILTER_CAPS_IN_BUNDLE) ) {
			if (all_caps_matched) {
				if (dst_bundle_idx)
					(*dst_bundle_idx) = cap_bundle_idx;
				return GF_TRUE;
			}
			all_caps_matched = GF_TRUE;
			nb_subcaps=0;
			cur_bundle_start = i;
			cap_bundle_idx++;
			if ((for_bundle_idx>=0) && (cap_bundle_idx>for_bundle_idx)) {
				break;
			}
			continue;
		}
		if ((for_bundle_idx>=0) && (cap_bundle_idx<for_bundle_idx)) {
			all_caps_matched = 0;
			continue;
		}

		//not an input cap
		if (! (cap->flags & GF_FILTER_CAPS_INPUT) ) continue;

		nb_subcaps++;
		//no match for this cap, go on until new one or end
		if (!all_caps_matched) continue;

		if (cap->code) {
			pid_cap = gf_filter_pid_get_property(src_pid, cap->code);
		}
		if (!pid_cap && cap->name) pid_cap = gf_filter_pid_get_property_str(src_pid, cap->name);

		//we found a property of that type and it is equal
		if (pid_cap) {
			u32 j;
			Bool prop_excluded = GF_FALSE;
			Bool prop_equal = GF_FALSE;

			//this could be optimized by not checking several times the same cap
			for (j=0; j<nb_in_caps; j++) {
				const GF_FilterCapability *a_cap = &in_caps[j];

				if ((j>cur_bundle_start) && ! (a_cap->flags & GF_FILTER_CAPS_IN_BUNDLE) ) {
					break;
				}
				//not an input cap
				if (! (a_cap->flags & GF_FILTER_CAPS_INPUT) ) continue;
				//not a static and not in bundle
				if (! (a_cap->flags & GF_FILTER_CAPS_STATIC)) {
					if (j<cur_bundle_start)
						continue;
				}

				if (cap->code) {
					if (cap->code!=a_cap->code) continue;
				} else if (!cap->name || !a_cap->name || strcmp(cap->name, a_cap->name)) {
					continue;
				}
				if (!skip_explicit_load && (a_cap->flags & GF_FILTER_CAPS_EXPLICIT) ) {
					if (!dst_filter || (dst_filter != src_pid->filter->dst_filter)) {
						prop_equal = GF_FALSE;
						break;
					}
					if (dst_filter->freg != freg) {
						prop_equal = GF_FALSE;
						break;
					}
				}

				if (!prop_equal) {
					prop_equal = gf_props_equal(pid_cap, &a_cap->val);
					//excluded cap: if value match, don't match this cap at all
					if (cap->flags & GF_FILTER_CAPS_EXCLUDED) {
						if (prop_equal) {
							prop_equal = GF_FALSE;
							prop_excluded = GF_FALSE;
							break;
						}
						prop_excluded = GF_TRUE;
					}
					if (prop_equal) break;
				}
			}
			if (!prop_equal && !prop_excluded) {
				all_caps_matched=GF_FALSE;
			} else if (priority && ( (*priority) < cap->priority) ) {
				(*priority) = cap->priority;
			}
		}
		else if (!(cap->flags & GF_FILTER_CAPS_EXCLUDED) ) {
			all_caps_matched=GF_FALSE;
		}
	}

	if (nb_subcaps && all_caps_matched) {
		if (dst_bundle_idx)
			(*dst_bundle_idx) = cap_bundle_idx;
		return GF_TRUE;
	}

	return GF_FALSE;
}

u32 gf_filter_caps_bundle_count(const GF_FilterCapability *caps, u32 nb_caps)
{
	u32 i, nb_bundles = nb_caps ? 1 : 0;
	for (i=0; i<nb_caps; i++) {
		const GF_FilterCapability *cap = &caps[i];
		if (! (cap->flags & GF_FILTER_CAPS_IN_BUNDLE)) {
			nb_bundles++;
			continue;
		}
	}
	return nb_bundles;
}

Bool gf_filter_has_out_caps(const GF_FilterRegister *freg)
{
	u32 i;
	//check all input caps of dst filter, count bundles
	for (i=0; i<freg->nb_caps; i++) {
		const GF_FilterCapability *out_cap = &freg->caps[i];
		if (out_cap->flags & GF_FILTER_CAPS_OUTPUT) {
			return GF_TRUE;
		}
	}
	return GF_FALSE;
}

u32 gf_filter_caps_to_caps_match(const GF_FilterRegister *src, u32 src_bundle_idx, const GF_FilterRegister *dst_reg, GF_Filter *dst_filter, u32 *dst_bundle_idx, s32 for_dst_bundle)
{
	u32 i=0;
	u32 cur_bundle_start = 0;
	u32 cur_bundle_idx = 0;
	u32 nb_matched=0;
	u32 nb_out_caps=0;
	u32 nb_in_bundles=0;
	u32 bundle_score = 0;
	Bool *bundles_in_ok = NULL;
	u32 *bundles_cap_found = NULL;
	u32 *bundles_in_scores = NULL;
	//initialize caps matched to true for first cap bundle
	Bool all_caps_matched = GF_TRUE;
	const GF_FilterCapability *dst_caps = dst_reg->caps;
	u32 nb_dst_caps = dst_reg->nb_caps;

	if (dst_filter && dst_filter->freg==dst_reg && dst_filter->forced_caps) {
		dst_caps = dst_filter->forced_caps;
		nb_dst_caps = dst_filter->nb_forced_caps;
	}

	//check all input caps of dst filter, count bundles
	if (! gf_filter_has_out_caps(src)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s has no output caps, cannot match filter %s inputs\n", src->name, dst_reg->name));
		return 0;
	}

	//check all input caps of dst filter, count bundles
	nb_in_bundles = gf_filter_caps_bundle_count(dst_caps, nb_dst_caps);
	if (!nb_in_bundles) {
		if (dst_reg->configure_pid) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s has no caps but pid configure possible, assuming possible connection\n", dst_reg->name));
			return 1;
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s has no caps and no pid configure, no possible connection\n", dst_reg->name));
		return 0;
	}
	bundles_in_ok = gf_malloc(sizeof(Bool) * nb_in_bundles);
	bundles_cap_found = gf_malloc(sizeof(u32) * nb_in_bundles);
	bundles_in_scores = gf_malloc(sizeof(u32) * nb_in_bundles);
	for (i=0; i<nb_in_bundles; i++) {
		bundles_in_ok[i] = GF_TRUE;
		bundles_cap_found[i] = 0;
		bundles_in_scores[i] = 0;
	}


	//check all output caps of src filter
	for (i=0; i<src->nb_caps; i++) {
		u32 j, k;
		Bool already_tested = GF_FALSE;
		const GF_FilterCapability *out_cap = &src->caps[i];

		if (!(out_cap->flags & GF_FILTER_CAPS_IN_BUNDLE) ) {
			all_caps_matched = GF_TRUE;
			cur_bundle_start = i+1;
			cur_bundle_idx++;
			if (src_bundle_idx < cur_bundle_idx)
				break;

			continue;
		}

		//not our selected output and not static cap
		if ((src_bundle_idx != cur_bundle_idx) && ! (out_cap->flags & GF_FILTER_CAPS_STATIC) ) {
			continue;
		}

		//not an output cap
		if (!(out_cap->flags & GF_FILTER_CAPS_OUTPUT) ) continue;

		//no match possible for this cap, wait until next cap start
		if (!all_caps_matched) continue;

		//check we didn't test a cap with same name/code before us
		for (k=cur_bundle_start; k<i; k++) {
			const GF_FilterCapability *an_out_cap = &src->caps[k];
			if (! (an_out_cap->flags & GF_FILTER_CAPS_IN_BUNDLE) ) {
				break;
			}
			if (! (an_out_cap->flags & GF_FILTER_CAPS_OUTPUT) ) {
				continue;
			}
			if (out_cap->code && (out_cap->code==an_out_cap->code) ) {
				already_tested = GF_TRUE;
				break;
			}
			if (out_cap->name && an_out_cap->name && !strcmp(out_cap->name, an_out_cap->name)) {
				already_tested = GF_TRUE;
				break;
			}
		}
		if (already_tested) {
			continue;
		}
		nb_out_caps++;

		//set cap as OK in all bundles
		for (k=0; k<nb_in_bundles; k++) {
			bundles_cap_found[k] = 0;
		}

		//check all output caps in this bundle with the same code/name, consider OK if one is matched
		for (k=cur_bundle_start; k<src->nb_caps; k++) {
			u32 cur_dst_bundle=0;
			Bool static_matched = GF_FALSE;
			u32 nb_caps_tested = 0;
			Bool matched=GF_FALSE;
			Bool exclude=GF_FALSE;
			Bool prop_found=GF_FALSE;
			const GF_FilterCapability *an_out_cap = &src->caps[k];
			if (! (an_out_cap->flags & GF_FILTER_CAPS_IN_BUNDLE) ) {
				break;
			}
			if (! (an_out_cap->flags & GF_FILTER_CAPS_OUTPUT) ) {
				continue;
			}
			if (out_cap->code && (out_cap->code!=an_out_cap->code) )
				continue;
			if (out_cap->name && (!an_out_cap->name || strcmp(out_cap->name, an_out_cap->name)))
				continue;

			//not our selected output and not static cap
			if ((src_bundle_idx != cur_bundle_idx) && ! (an_out_cap->flags & GF_FILTER_CAPS_STATIC) ) {
				continue;
			}

			nb_matched = 0;
			//check all input caps of dst filter, count ones that are matched
			for (j=0; j<nb_dst_caps; j++) {
				Bool prop_equal;
				const GF_FilterCapability *in_cap = &dst_caps[j];

				if (! (in_cap->flags & GF_FILTER_CAPS_IN_BUNDLE)) {
					//we found a prop, excluded but with != value hence acceptable, default matching to true
					if (!matched && prop_found) matched = GF_TRUE;
					//not match, flag this bundle as not ok
					if (matched) {
						bundles_cap_found[cur_dst_bundle] ++;
						nb_matched++;
					}

					matched = static_matched ? GF_TRUE : GF_FALSE;
					exclude = GF_FALSE;
					prop_found = GF_FALSE;
					nb_caps_tested = 0;
					cur_dst_bundle++;
					if ((for_dst_bundle>=0) && (cur_dst_bundle>for_dst_bundle))
						break;

					continue;
				}
				//not an input cap
				if (!(in_cap->flags & GF_FILTER_CAPS_INPUT) )
					continue;

				if ((for_dst_bundle>=0) && (cur_dst_bundle<for_dst_bundle) && !(in_cap->flags & GF_FILTER_CAPS_STATIC))
					continue;

				//prop was excluded, cannot match in bundle
				if (exclude) continue;
				//prop was matched, no need to check other caps in the current bundle
				if (matched) continue;

				if (out_cap->code && (out_cap->code!=in_cap->code) )
					continue;
				if (out_cap->name && (!in_cap->name || strcmp(out_cap->name, in_cap->name)))
					continue;

				nb_caps_tested++;
				//we found a property of that type , check if equal equal
				prop_equal = gf_props_equal(&in_cap->val, &an_out_cap->val);
				if ((in_cap->flags & GF_FILTER_CAPS_EXCLUDED) && !(an_out_cap->flags & GF_FILTER_CAPS_EXCLUDED) ) {
					//prop type matched, output includes it and input excludes it: no match, don't look any further
					if (prop_equal) {
						matched = GF_FALSE;
						exclude = GF_TRUE;
						prop_found = GF_FALSE;
					} else {
						//remember we found a prop of same type but excluded value
						// we will match unless we match an excluded value
						prop_found = GF_TRUE;
					}
				} else if (!(in_cap->flags & GF_FILTER_CAPS_EXCLUDED) && (an_out_cap->flags&GF_FILTER_CAPS_EXCLUDED) ) {
					//prop type matched, input includes it and output excludes it: no match, don't look any further
					if (prop_equal) {
						matched = GF_FALSE;
						exclude = GF_TRUE;
						prop_found = GF_FALSE;
					} else {
						//remember we found a prop of same type but excluded value
						//we will match unless we match an excluded value
						prop_found = GF_TRUE;
					}
				} else if (prop_equal) {
					matched = GF_TRUE;
//					if (an_out_cap->flags & GF_FILTER_CAPS_STATIC)
//						static_matched = GF_TRUE;
				}
			}
			if (nb_caps_tested) {
				//we found a prop, excluded but with != value hence acceptable, default matching to true
				if (!matched && prop_found) matched = GF_TRUE;
				//not match, flag this bundle as not ok
				if (matched) {
					bundles_cap_found[cur_dst_bundle] ++;
					nb_matched++;
				}
			} else if (!nb_dst_caps) {
				bundles_cap_found[cur_dst_bundle] ++;
				nb_matched++;
			}
		}
		//merge bundle cap
		nb_matched=0;
		for (k=0; k<nb_in_bundles; k++) {
			if (!bundles_cap_found[k])
				bundles_in_ok[k] = GF_FALSE;
			else {
				nb_matched += bundles_cap_found[k];
				//we matched this property, keep score for the bundle
				bundles_in_scores[k] ++;
			}
		}
		//not matched and not excluded, skip until next bundle
		if (!nb_matched && !(out_cap->flags & GF_FILTER_CAPS_EXCLUDED)) {
			all_caps_matched = GF_FALSE;
		}
	}

	//get bundle with highest score
	bundle_score = 0;
	nb_matched = 0;
	for (i=0; i<nb_in_bundles; i++) {
		if (bundles_in_ok[i]) {
			nb_matched++;
			if (bundle_score < bundles_in_scores[i]) {
				*dst_bundle_idx = i;
				bundle_score = bundles_in_scores[i];
			}
		}
	}
	if (!bundle_score) {
//		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s outputs cap bundle %d do not match filter %s inputs\n", src->name, src_bundle_idx, dst_reg->name));
	} else {
//		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s outputs cap bundle %d matches filter %s inputs caps bundle %d (%d total bundle matches, bundle matched %d/%d caps)\n", src->name, src_bundle_idx, dst_reg->name, *dst_bundle_idx, nb_matched, bundle_score, nb_out_caps));
	}
	gf_free(bundles_in_ok);
	gf_free(bundles_cap_found);
	gf_free(bundles_in_scores);
	return bundle_score;
}

Bool gf_filter_pid_check_caps(GF_FilterPid *pid)
{
	u8 priority;
	if (PID_IS_OUTPUT(pid)) return GF_FALSE;
	return filter_pid_caps_match(pid->pid, NULL, NULL, &priority, NULL, pid->filter, -1);
}


/*
	Check the PID output matches the filter registry input for the given caps bundle. This will
	recursevely be called until a registry input matches the PID output, or until the mac chain length
	is reached, in which case 0 is returned. The \original_filter_chain is filled, starting from the filter
	matching the destination filter, with:
	* even entries: the matched registry
	* even entries+1: the cap of the registry located at the index of the cap bundle in that registry.
	to get the cap bundle index, locate the index of that cap in the registry. This avoids allocating integers

\param fsess the filter session
\param filter_reg the filter registry to check the PID against
\param src_bundle_idx the caps bundle index in the registry to connect to
\param black_list the list of blacklisted filters for the source pid
\param original_filter_chain the filter chain description to fill
\param filter_stack the current stack of filters, used to break loops in recursion (eg imgdec->imgenc->imgdec->...
\param tested_filters the list of tested filters that should not be retested again
\param dst destination filter
\param rlevel recursion level, used to stop recursion when chains are longer than maximum chain
\param max_chain_len the maximum length of accepted chains, or 0 if not bound
\param pid the pid we are connecting
\return the weight of the path found (number of caps matched at each level)
*/

static u32 gf_filter_check_dst_caps(GF_FilterSession *fsess, const GF_FilterRegister *filter_reg, u32 filter_reg_bundle_idx, GF_List *black_list, GF_List *original_filter_chain, GF_List *filter_stack, GF_List *tested_filters, GF_Filter *dst, u32 rlevel, u32 max_chain_len, GF_FilterPid *pid)
{
	u32 nb_matched = 0;
	u32 bundle_idx;
	u32 subchain_len=0;
	const GF_FilterRegister *candidate = NULL;
	//browse all our registered filters
	u32 i, count=gf_list_count(fsess->registry);
	u32 count_at_input = gf_list_count(original_filter_chain);
	u8 priority;
	GF_List *current_filter_chain;

	//no match of pid caps for this filter
	if (filter_pid_caps_match(pid, filter_reg, dst, &priority, &bundle_idx, pid->filter->dst_filter, filter_reg_bundle_idx) ) {
		//add filter
		gf_list_add(original_filter_chain, (void *) filter_reg);
		//and indicate its matching cap bundle
		gf_list_add(original_filter_chain, (void *) &filter_reg->caps[filter_reg_bundle_idx]);
		return 1;
	}
	bundle_idx=0;


	if (max_chain_len && (rlevel+1>=max_chain_len) )
		return 0;

	//already being tested for this chain
	if (gf_list_find(tested_filters, (void *) filter_reg)>=0)
		return 0;

	//create new temp chain, init with this filter as root
	current_filter_chain = gf_list_new();
	gf_list_add(current_filter_chain, (void *) filter_reg);
	//and indicate its matching cap bundle
	gf_list_add(current_filter_chain, (void *) &filter_reg->caps[filter_reg_bundle_idx]);

	for (i=0; i<count; i++) {
		u32 path_weight=0;
		u32 sub_weight, orig_chain_len, cur_chain_len, k, nb_in_caps, dst_bundle_idx;
		const GF_FilterRegister *freg = gf_list_get(fsess->registry, i);
		if (freg==filter_reg) continue;
		//filter shall be explicitly loaded
		if (freg->explicit_only) continue;

		//freg already being tested for another chain
		if (gf_list_find(tested_filters, (void *) freg)>=0)
			continue;

		//freg already in our chain, break loop
		if (gf_list_find(filter_stack, (void *) freg)>=0)
			continue;

		//source filter, can't add pid
		if (!freg->configure_pid) continue;
		
		//blacklisted filter, can't add pid
		if (gf_list_find(black_list, (void *) freg)>=0)
			continue;

		//no output caps, don't resolve
		if (! gf_filter_has_out_caps(freg))
			continue;

		//resulting chain would be too big, don't look any further
		if (max_chain_len && (rlevel+2>=max_chain_len) )
			continue;

		//for each cap bundle check if we have a valid path
		nb_in_caps = gf_filter_caps_bundle_count(freg->caps, freg->nb_caps);
		for (k=0; k<nb_in_caps; k++) {
			path_weight = gf_filter_caps_to_caps_match(freg, k, (const GF_FilterRegister *) filter_reg, NULL, &dst_bundle_idx, filter_reg_bundle_idx);
			//we don't match our target bundle
			if (dst_bundle_idx != filter_reg_bundle_idx) continue;
			if (! path_weight) continue;

			bundle_idx = k;

			path_weight *= (255-freg->priority);

			//check this filter
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("L%d Filter %s outputs caps bundle %d match filter %s input caps bundle %d, checking filter chain\n", rlevel, freg->name, bundle_idx, filter_reg->name, filter_reg_bundle_idx));

			gf_list_add(filter_stack, (void *) freg);

			//we pass the original filter chain with the new root inserted to break loops
			//note that of the max_chain_length is not supported and we have a local solution, don't allow
			//longer chains than the local one
			sub_weight = gf_filter_check_dst_caps(fsess, freg, bundle_idx, black_list, current_filter_chain, filter_stack, tested_filters, NULL, rlevel+1, max_chain_len ? max_chain_len : subchain_len, pid);

			gf_list_rem_last(filter_stack);

			if (!sub_weight) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("L%d No valid shorter chain to filter %s input\n", rlevel, freg->name));
				gf_list_add(tested_filters, (void *) freg);
				continue;
			}
			path_weight += sub_weight;

			orig_chain_len = gf_list_count(original_filter_chain) - count_at_input;
			cur_chain_len = gf_list_count(current_filter_chain);
			//the filter chain contain for each filter the registry and the cap
			orig_chain_len/=2;
			cur_chain_len/=2;

			if (!orig_chain_len || (orig_chain_len>cur_chain_len)
				|| ((orig_chain_len==cur_chain_len) && (path_weight>nb_matched) )
			) {
				//remove all entries added in recursive gf_filter_check_dst_caps
				while (gf_list_count(original_filter_chain)>count_at_input) {
					gf_list_rem_last(original_filter_chain);
				}
				if (candidate) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("L%d Filter %s has lower priority/pid matching (%d) than filter %s (%d), replacing it in filter chain\n", rlevel, candidate->name, nb_matched, freg->name, path_weight));
				}
				nb_matched = path_weight;
				candidate = freg;

				//store our filter chain in original one
				gf_list_transfer(original_filter_chain, current_filter_chain);
				subchain_len = gf_list_count(original_filter_chain) - count_at_input;
				//the filter chain contain for each filter the registry and the cap
				subchain_len /= 2;

				//and reinit test chain
				gf_list_add(current_filter_chain, (void *) filter_reg);
				//and indicate its matching cap bundle
				gf_list_add(current_filter_chain, (void *) &filter_reg->caps[filter_reg_bundle_idx]);

			} else {
				assert(candidate);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("L%d Filter %s has lower priority/pid matching (%d) than filter %s (%d), ignoring\n", rlevel, freg->name, path_weight, candidate->name, nb_matched));

				//and reinit current chain
				gf_list_reset(current_filter_chain);
				gf_list_add(current_filter_chain, (void *) filter_reg);
				//and indicate its matching cap bundle
				gf_list_add(current_filter_chain, (void *) &filter_reg->caps[filter_reg_bundle_idx]);
			}

		}
	}
	gf_list_del(current_filter_chain);
	return nb_matched;
}

static void concat_reg(GF_FilterSession *sess, char prefRegistry[1001], const char *reg_key, const char *args)
{
	u32 len;
	char *forced_reg, *sep;
	if (!args) return;
	forced_reg = strstr(args, reg_key);
	if (!forced_reg) return;
	forced_reg += 6;
	sep = strchr(forced_reg, sess->sep_args);
	len = sep ? sep-forced_reg : strlen(forced_reg);
	if (len+2+strlen(prefRegistry)>1000) {
		return;
	}
	if (prefRegistry[0]) {
		char szSepChar[2];
		szSepChar[0] = sess->sep_args;
		szSepChar[1] = 0;
		strcat(prefRegistry, szSepChar);
	}
	strncat(prefRegistry, forced_reg, len);
}

static Bool gf_filter_out_caps_solved_by_connection(const GF_FilterRegister *freg, u32 bundle_idx)
{
	u32 i, k, cur_bundle_idx = 0;
	for (i=0; i<freg->nb_caps; i++) {
		u32 nb_caps = 0;
		const GF_FilterCapability *cap = &freg->caps[i];
		if (!(cap->flags & GF_FILTER_CAPS_IN_BUNDLE)) {
			cur_bundle_idx++;
			if (cur_bundle_idx>bundle_idx) return GF_FALSE;
		}
		if (!(cap->flags & GF_FILTER_CAPS_STATIC) && (bundle_idx>cur_bundle_idx)) continue;
		if (!(cap->flags & GF_FILTER_CAPS_OUTPUT)) continue;
		for (k=0; k<freg->nb_caps; k++) {
			const GF_FilterCapability *acap = &freg->caps[k];
			if (!(acap->flags & GF_FILTER_CAPS_IN_BUNDLE)) break;
			if (!(acap->flags & GF_FILTER_CAPS_OUTPUT)) continue;
			if (!(acap->flags & GF_FILTER_CAPS_STATIC) && (k<i) ) continue;

			if (cap->code && (acap->code==cap->code)) {
				nb_caps++;
			} else if (cap->name && acap->name && !strcmp(cap->name, acap->name)) {
				nb_caps++;
			}
			//if more than one cap with same code in same bundle, consider the filter is undecided
			if (nb_caps>1)
				return GF_TRUE;
		}
	}
	return GF_FALSE;
}

/*
	!Resolves a link between a PID and a destination filter

\param pid source pid to connect
\param dst destination filter to connect source PID to
\param filter_reassigned indicates the filter has been destroyed and reassigned
\param reconfigurable_only indicates the chain should be loaded for reconfigurable filters
\return the first filter in the matching chain, or NULL if no match
*/
static GF_Filter *gf_filter_pid_resolve_link_internal(GF_FilterPid *pid, GF_Filter *dst, Bool *filter_reassigned, Bool reconfigurable_only)
{
	GF_Filter *chain_input = NULL;
	GF_FilterSession *fsess = pid->filter->session;
	GF_List *filter_chain = gf_list_new();
	GF_List *filter_stack = gf_list_new();
	GF_List *tested_filters = gf_list_new();
	u32 max_weight=0;
	u32 min_length=GF_INT_MAX;
	u32 i, count;
	const GF_FilterRegister *dst_filter = dst->freg;
	char *force_freg, prefRegistry[1001];
	char szForceReg[20];

	if (filter_reassigned)
		*filter_reassigned = GF_FALSE;

	if (!dst) return NULL;

	sprintf(szForceReg, "gfreg%c", pid->filter->session->sep_name);
	force_freg = NULL;
	prefRegistry[0]=0;
	//look for reg given in
	concat_reg(pid->filter->session, prefRegistry, szForceReg, pid->filter->orig_args ? pid->filter->orig_args : pid->filter->src_args);
	concat_reg(pid->filter->session, prefRegistry, szForceReg, pid->filter->dst_args);
	concat_reg(pid->filter->session, prefRegistry, szForceReg, dst->src_args);
	concat_reg(pid->filter->session, prefRegistry, szForceReg, dst->dst_args);

	gf_list_reset(filter_chain);
	gf_list_reset(filter_stack);
	gf_list_reset(tested_filters);
	gf_list_add(filter_stack, (void *)pid->filter->freg);

	max_weight=0;
	min_length=0;

	//browse all our registered filters
	count=gf_list_count(fsess->registry);
	for (i=0; i<count; i++) {
		u32 chain_priority=0;
		u32 path_weight=0;
		u32 path_len=0;
		u32 chain_len=0;
		u32 cap_bundle_idx=0;
		u32 bundle_idx=0;
		u32 k, nb_filters, nb_in_caps;
		Bool force_registry = GF_FALSE;
		const GF_FilterRegister *freg = gf_list_get(fsess->registry, i);

		//source filter, can't add pid
		if (!freg->configure_pid) continue;

		//freg shall be instantiated 
		if (freg->explicit_only)
			continue;

		//we only want reconfigurable output filters
		if (reconfigurable_only && !freg->reconfigure_output) continue;

		//we don't allow re-entrant filter registries (eg filter foo of type A output cannot connect to filter bar of type A)
		if (pid->filter->freg == freg)
			continue;

		//blacklisted filter, can't add pid
		if (gf_list_find(pid->filter->blacklisted, (void *) freg)>=0)
			continue;

		if (pid->adapters_blacklist && (gf_list_find(pid->adapters_blacklist, (void *) freg)>=0))
			continue;

		if (!gf_filter_has_out_caps(freg))
			continue;

		//check which cap of this filter matches ou destination
		nb_in_caps = gf_filter_caps_bundle_count(freg->caps, freg->nb_caps);
		for (k=0; k<nb_in_caps; k++) {
			path_weight = gf_filter_caps_to_caps_match(freg, k, (const GF_FilterRegister *) dst_filter, dst, &bundle_idx, -1);
			if (path_weight) break;
		}
		//no caps matched
		if (!path_weight) continue;
		//remember it
		cap_bundle_idx = k;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s output caps bundle %d match from %s input caps bundle %d checking filter chain\n", freg->name, cap_bundle_idx, dst_filter->name, bundle_idx));

		//TODO: handle user-defined priorities

		//we have a target destination filter match, keep solving filter until done
		path_len = gf_list_count(filter_chain);

		//min_length/2 because the filter chain contain for each filter the registry and the cap
		path_weight = gf_filter_check_dst_caps(fsess, freg, cap_bundle_idx, pid->filter->blacklisted, filter_chain, filter_stack, tested_filters, dst, 0, min_length/2, pid);

		//not our candidate, remove all added entries
		if (!path_weight) {
			while (gf_list_count(filter_chain) > path_len) {
				gf_list_rem_last(filter_chain);
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Cannot find a valid filter chain from filter %s to filter %s, ignoring filter\n", freg->name, dst_filter->name));
			//skip further attempts with this filter
			gf_list_add(tested_filters, (void *) freg);
			continue;
		}
		//we keep track of all filters in possible chain, whether we keep the chain or not
		//this allows calls to gf_filter_check_dst_caps to abort when trying to test an already tested filter
		//this might need refinements in case the filter was first tested in a longer chain ...
		chain_priority = 0;
		nb_filters = gf_list_count(filter_chain);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Possible %sfilter chain from filter %s PID %s to filter %s:\n", reconfigurable_only ? "adaptation " : "", pid->filter->name, pid->name, dst_filter->name));
		for (k=path_len; k<nb_filters; k++) {
			const GF_FilterRegister *areg;
			u32 for_bundle_idx = 0;
			u32 j;
			const GF_FilterCapability *cap;
			s32 idx = nb_filters+path_len - k-2;
			if (k%2) continue;
			assert(idx>=0);
			areg = gf_list_get(filter_chain, idx);
			cap = gf_list_get(filter_chain, idx + 1);

			//get the cap bundle index - the cap added to the list is the cap with the same index as the bundle start we want
			//(this avoids allocating integers to store the bundle)
			for (j=0; j<areg->nb_caps; j++) {
				if (&areg->caps[j]==cap) {
					for_bundle_idx = j;
					break;
				}
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("\t%s\n", areg->name));

			chain_priority += (255 - areg->priority);

			if (prefRegistry[0] && strstr(prefRegistry, areg->name)) {
				force_registry = GF_TRUE;
			}
			//if filter caps need connection to resolve, don't try loading this filter in another test
			if (gf_filter_out_caps_solved_by_connection(areg, for_bundle_idx)) {
				gf_list_add(tested_filters, (void *) areg);
			}
		}
		//
		chain_len = gf_list_count(filter_chain) - path_len;

		//if same chain length, use max path weight
 		if ( ( (chain_len==min_length) && (path_weight+chain_priority > max_weight) )
 		//otherwise use chorter chain
			|| (chain_len<min_length)
			|| !min_length || force_registry
		) {
			//remove initial entries
			while (path_len) {
				gf_list_rem(filter_chain, 0);
				path_len--;
			}
			max_weight = path_weight+chain_priority;
			min_length = gf_list_count(filter_chain);
		} else {
			//remove all added entries
			while (gf_list_count(filter_chain) > path_len) {
				gf_list_rem_last(filter_chain);
			}
		}
		if (force_registry) break;
	}
	count = gf_list_count(filter_chain);
	if (count==0) {
		Bool can_reassign = GF_TRUE;

		//reassign only for source filters
		if (pid->filter->num_input_pids) can_reassign = GF_FALSE;
		//sticky filters cannot be unloaded
		else if (pid->filter->sticky) can_reassign = GF_FALSE;
		//if we don't have pending PIDs to setup from the source
		else if (pid->filter->out_pid_connection_pending) can_reassign = GF_FALSE;
		//if we don't have pending PIDs to setup from the source
		else if (pid->filter->num_output_pids) {
			u32 k;
			for (k=0; k<pid->filter->num_output_pids; k++) {
				GF_FilterPid *apid = gf_list_get(pid->filter->output_pids, k);
				if (apid->num_destinations || apid->init_task_pending) {
					can_reassign = GF_FALSE;
					break;
				}
			}
		}
		//if source filter, try to load another filter - we should complete this with a cache of filter sources
		if (filter_reassigned && can_reassign) {
			if (! gf_filter_swap_source_registry(pid->filter) ) {
				//no filter found for this pid !
				GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("No suitable filter chain found\n"));
			}
			*filter_reassigned = GF_TRUE;
		} else if (!reconfigurable_only) {
			GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("No suitable filter found for pid %s from filter %s\n", pid->name, pid->filter->name));
		}
	} else if (reconfigurable_only && (count>2)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Cannot find filter chain with only one filter handling reconfigurable output for pid %s from filter %s - not supported\n", pid->name, pid->filter->name));
	} else {
		const char *dst_args = NULL;
		const char *args = pid->filter->src_args;
		GF_FilterPid *a_pid = pid;
		GF_Filter *prev_af;

		dst_args = dst->src_args ? dst->src_args : dst->orig_args;

		while (a_pid) {
			GF_FilterPidInst *pidi;
			args = a_pid->filter->src_args;
			if (!args) args = a_pid->filter->orig_args;
			if (args) break;
			pidi = gf_list_get(a_pid->filter->input_pids, 0);
			if (!pidi) break;
			a_pid = pidi->pid;
		}

#ifndef GPAC_DISABLE_LOGS
		if (gf_log_tool_level_on(GF_LOG_FILTER, GF_LOG_INFO)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Solved %sfilter chain from filter %s PID %s to filter %s - dumping chain:\n", reconfigurable_only ? "adaptation " : "", pid->filter->name, pid->name, dst_filter->name));
		}
#endif
		prev_af = NULL;
		for (i=0; i<count; i++) {
			GF_Filter *af;
			Bool load_first_only = GF_FALSE;
			s32 cap_idx = -1;
			const GF_FilterRegister *freg;
			const GF_FilterCapability *cap = NULL;
			u32 k, cur_bundle, bundle_idx=0;
			if (i%2) continue;
			freg = gf_list_get(filter_chain, count - i - 2);
			cap = gf_list_get(filter_chain, count - i - 1);
			//get the cap bundle index - the cap added to the list is the cap with the same index as the bundle start we want
			//(this avoids allocating integers to store the bundle)
			for (k=0; k<freg->nb_caps; k++) {
				if (&freg->caps[k]==cap) {
					bundle_idx = k;
					break;
				}
			}
			cur_bundle = 0;
			for (k=0; k<freg->nb_caps; k++) {
				cap = &freg->caps[k];
				if (cur_bundle==bundle_idx) {
					cap_idx = k;
					break;
				}
				if (!(cap->flags & GF_FILTER_CAPS_IN_BUNDLE)) {
					cur_bundle++;
				}
			}
			//if first filter has multiple possible outputs, don't bother loading the entire chain since it is likely wrong
			//(eg demuxers, we don't know yet what's in the file)
			if (!i && gf_filter_out_caps_solved_by_connection(freg, bundle_idx))
				load_first_only = GF_TRUE;

			GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("\t%s\n", freg->name));

			af = gf_filter_new(fsess, freg, args, dst_args, GF_FILTER_ARG_GLOBAL, NULL);
			if (!af) goto exit;

			//the other filters shouldn't need any specific init
			af->dynamic_filter = GF_TRUE;
			//remember our target cap on that filter
			af->cap_idx_at_resolution = cap_idx;

			//remember our target filter
			if (prev_af) prev_af->dst_filter = af;
			if (i+2==count) af->dst_filter = dst;

			//also remember our original target in case we got the link wrong
			af->target_filter = pid->filter->target_filter;

			prev_af = af;

			if (reconfigurable_only) af->is_pid_adaptation_filter = GF_TRUE;

			//remember the first in the chain
			if (!i) chain_input = af;

			if (load_first_only) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s needs to be connected to decide its outputs, not loading end of the chain\n", freg->name));
				break;
			}
		}
	}

exit:
	gf_list_del(filter_chain);
	gf_list_del(filter_stack);
	gf_list_del(tested_filters);
	return chain_input;
}

static GF_Filter *gf_filter_pid_resolve_link(GF_FilterPid *pid, GF_Filter *dst, Bool *filter_reassigned)
{
	return gf_filter_pid_resolve_link_internal(pid, dst, filter_reassigned, GF_FALSE);
}

GF_Filter *gf_filter_pid_resolve_link_for_caps(GF_FilterPid *pid, GF_Filter *dst)
{
	return gf_filter_pid_resolve_link_internal(pid, dst, NULL, GF_TRUE);
}

static const char *gf_filter_last_id_in_chain(GF_Filter *filter)
{
	u32 i;
	const char *id;
	if (filter->id) return filter->id;
	if (!filter->dynamic_filter) return NULL;

	for (i=0; i<filter->num_input_pids; i++) {
		GF_FilterPidInst *pidi = gf_list_get(filter->input_pids, i);
		if (pidi->pid->filter->id) return pidi->pid->filter->id;
		//stop at first non dyn filter
		if (!pidi->pid->filter->dynamic_filter) continue;
		id = gf_filter_last_id_in_chain(pidi->pid->filter);
		if (id) return id;
	}
	return NULL;
}

void gf_filter_pid_retry_caps_negotiate(GF_FilterPid *src_pid, GF_FilterPid *pid, GF_Filter *dst_filter)
{
	assert(dst_filter);
	src_pid->caps_negociate = pid->caps_negociate;
	pid->caps_negociate = NULL;
	src_pid->caps_dst_filter = dst_filter;
	//blacklist filter for adaptation
	if (!src_pid->adapters_blacklist) src_pid->adapters_blacklist = gf_list_new();
	gf_list_add(src_pid->adapters_blacklist, (void *) pid->filter->freg);
	//once != 0 will trigger reconfiguration, so set this once all vars have been set
	safe_int_inc(& src_pid->filter->nb_caps_renegociate );

	//disconnect source pid from filter - this will unload the filter itself
	gf_fs_post_task(src_pid->filter->session, gf_filter_pid_disconnect_task, pid->filter, src_pid, "pidinst_disconnect", NULL);
}

static void gf_filter_pid_init_task(GF_FSTask *task)
{
	u32 i, count;
	Bool found_dest=GF_FALSE;
	Bool first_pass=GF_TRUE;
	GF_List *loaded_filters = NULL;
	GF_Filter *filter = task->filter;
	GF_FilterPid *pid = task->pid;
	Bool filter_found_but_pid_excluded = GF_FALSE;
	const char *filter_id;

	if (pid->destroyed) {
		safe_int_dec(&pid->init_task_pending);
		return;
	}
	pid->props_changed_since_connect = GF_FALSE;

	if (filter->caps_negociate) {
		if (! gf_filter_reconf_output(filter, pid))
			return;
	}

	//since we may have inserted filters in the middle (demuxers typically), get the last explicitely
	//loaded ID in the chain
	filter_id = gf_filter_last_id_in_chain(filter);
	if (!filter_id && filter->cloned_from)
		filter_id = gf_filter_last_id_in_chain(filter->cloned_from);

restart:

	if (filter->session->filters_mx) gf_mx_p(filter->session->filters_mx);

	if (!first_pass)
		loaded_filters = gf_list_new();

	//try to connect pid to all running filters
	count = gf_list_count(filter->session->filters);
	for (i=0; i<count; i++) {
		GF_Filter *filter_dst = gf_list_get(filter->session->filters, i);
		//source filter
		if (!filter_dst->freg->configure_pid) continue;
		if (filter_dst->finalized || filter_dst->removed) continue;

		//if destination accepts only one input, and connected or connection pending
		if (!filter_dst->max_extra_pids && (filter_dst->num_input_pids || filter_dst->in_pid_connection_pending) ) {
			//not explicitly clonable, don't connect to it
			if (!filter_dst->clonable)
				continue;

			//explicitly clonable but caps don't match, don't connect to it
			if (!filter_pid_caps_match(pid, filter_dst->freg, filter_dst, NULL, NULL, pid->filter->dst_filter, -1))
				continue;
		}

		if (gf_list_find(pid->filter->blacklisted, (void *) filter_dst->freg)>=0) continue;

		//we don't allow re-entrant filter registries (eg filter foo of type A output cannot connect to filter bar of type A)
		if (pid->pid->filter->freg == filter_dst->freg) {
			continue;
		}
		//we try to load a filter chain, so don't test against filters loaded for another chain
		if (filter_dst->dynamic_filter && (filter_dst != pid->filter->dst_filter)) {
			//dst was explicitely set and does not match
			if (pid->filter->dst_filter) continue;
			//dst was not set, we may try to connect to this filter if it allows several input
			//this is typically the case for muxers instantiated dynamically
			if (!filter_dst->max_extra_pids) continue;
		}
		//pid->filter->dst_filter NULL and pid->filter->target_filter is not: we had a wrong resolved chain to target
		//so only attempt to reling the chain if dst_filter is the expected target
		if (!pid->filter->dst_filter && pid->filter->target_filter && (filter_dst != pid->filter->target_filter))
			continue;

		//dynamic filters only connect to their destination, unless explicit connections through sources
		//we could remove this but this highly complicates PID resolution
		if (!filter_dst->source_ids && pid->filter->dynamic_filter && pid->filter->dst_filter && (filter_dst!=pid->filter->dst_filter)) continue;

		//walk up through the parent graph and check if this filter is already in. If so don't connect
		//since we don't allow re-entrant PIDs
		if (filter_in_parent_chain(filter, filter_dst) ) continue;

		//if the original filter is in the parent chain of this PID's filter, don't connect (equivalent to re-entrant)
		if (filter_dst->cloned_from) {
			if (filter_in_parent_chain(filter, filter_dst->cloned_from) ) continue;
		}

		//if the filter is in the parent chain of this PID's original filter, don't connect (equivalent to re-entrant)
		if (filter->cloned_from) {
			if (filter_in_parent_chain(filter->cloned_from, filter_dst) ) continue;
		}

		//if we have sourceID info on the destination, check them
		if (filter_id) {
			if (filter_dst->source_ids) {
				Bool pid_excluded=GF_FALSE;
				if (!filter_source_id_match(pid, filter_id, filter_dst->source_ids, &pid_excluded)) {
					if (pid_excluded) filter_found_but_pid_excluded = GF_TRUE;
					continue;
				}
			}
			//if no source ID on the dst filter, this means the dst filter accepts any possible connections from out filter
		}
		//no filterID and dst expects only specific filters, continue
		else if (filter_dst->source_ids) {
			continue;
		}

		//we have a match, check if caps are OK
		if (!filter_pid_caps_match(pid, filter_dst->freg, filter_dst, NULL, NULL, pid->filter->dst_filter, -1)) {
			Bool reassigned=GF_FALSE;
			u32 j, nb_loaded;
			GF_Filter *new_f, *reuse_f=NULL;
			//we had a destination set during link resolve, and we don't match that filter, consider the link resolution wrong
			if (pid->filter->dst_filter && (filter_dst == pid->filter->dst_filter)) {
				GF_Filter *old_dst = pid->filter->dst_filter;
				pid->filter->dst_filter = NULL;
				//nobody using this filter, destroy
				if (old_dst->dynamic_filter
					&& !old_dst->has_pending_pids
					&& !old_dst->num_input_pids
					&& !old_dst->out_pid_connection_pending
				) {
					assert(!old_dst->finalized);
					old_dst->finalized = GF_TRUE;
					gf_fs_post_task(old_dst->session, gf_filter_remove_task, old_dst, NULL, "filter_destroy", NULL);
				}
			}
			if (first_pass) continue;

			nb_loaded = gf_list_count(loaded_filters);
			for (j=0; j<nb_loaded; j++) {
				u32 out_cap_idx=0;
				reuse_f = gf_list_get(loaded_filters, j);
				//check if we match caps with this loaded filter, in which case assume we can use that filter for connections
				//THIS MIGHT NEED REFINEMENT
				if (filter_pid_caps_match(pid, reuse_f->freg, reuse_f, NULL, &out_cap_idx, pid->filter->dst_filter, -1)) {
					break;
				}
				reuse_f = NULL;
			}
			if (reuse_f) {
				GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Skip link from %s:%s to %s because already connected to filter %s which can handle the connection\n", pid->filter->name, pid->name, filter_dst->name, reuse_f->name));
				continue;
			}

			new_f = gf_filter_pid_resolve_link(pid, filter_dst, &reassigned);
			//try to load filters
			if (! new_f) {
				//filter was reassigned (pid is destroyed), return
				if (reassigned) {
					if (filter->session->filters_mx) gf_mx_v(filter->session->filters_mx);
					safe_int_dec(&pid->init_task_pending);
					return;
				}
				//we might had it wrong solving the chain initially, break the chain
				if (filter_dst->dynamic_filter && filter_dst->dst_filter) {
					GF_Filter *new_dst = filter_dst;
					while (new_dst->dst_filter && new_dst->dynamic_filter) {
						GF_Filter *f = new_dst;
						new_dst = new_dst->dst_filter;
						if (!f->num_input_pids && !f->num_output_pids && !f->in_pid_connection_pending) {
							f->finalized = GF_TRUE;
							gf_fs_post_task(f->session, gf_filter_remove_task, f, NULL, "filter_destroy", NULL);
						}
					}
					
					pid->filter->dst_filter = NULL;
					new_f = gf_filter_pid_resolve_link(pid, new_dst, &reassigned);
					if (!new_f) {
						if (reassigned) {
							if (filter->session->filters_mx) gf_mx_v(filter->session->filters_mx);
							safe_int_dec(&pid->init_task_pending);
							return;
						} else {
							continue;
						}
					}
					//good to go !
				} else {
					continue;
				}
			}
			filter_dst = new_f;
			gf_list_add(loaded_filters, new_f);
		}
		assert(pid->pid->filter->freg != filter_dst->freg);

		safe_int_inc(&pid->filter->out_pid_connection_pending);
		gf_filter_pid_post_connect_task(filter_dst, pid);

		found_dest = GF_TRUE;
	}
	if (filter->session->filters_mx) gf_mx_v(filter->session->filters_mx);

	if (loaded_filters) gf_list_del(loaded_filters);

	//connection task posted, nothing left to do
	if (found_dest) {
		//we connected ourselves to that filter. In case this is a demux filter, we may have other pids
		//connecting from that filter, so reset the destination to allow for graph resolution
		pid->filter->dst_filter = NULL;
		assert(pid->init_task_pending);
		safe_int_dec(&pid->init_task_pending);
		return;
	}

	//nothing found, redo a pass, this time allowing for link resolve
	if (first_pass) {
		first_pass = GF_FALSE;
		goto restart;
	}
	if (filter_found_but_pid_excluded) {
		//PID was not included in explicit connection lists
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("PID %s in filter %s not connected to any loaded filter due to source directives\n", pid->name, pid->filter->name));
	} else {
		//no filter found for this pid !
		GF_LOG(pid->not_connected_ok ? GF_LOG_DEBUG : GF_LOG_WARNING, GF_LOG_FILTER, ("No filter chain found for PID %s in filter %s to any loaded filters - NOT CONNECTED\n", pid->name, pid->filter->name));

	}
	assert(pid->init_task_pending);
	safe_int_dec(&pid->init_task_pending);
	return;
}

void gf_filter_pid_post_connect_task(GF_Filter *filter, GF_FilterPid *pid)
{
	assert(pid->pid);
	assert(pid->filter != filter);
	assert(pid->filter->freg != filter->freg);
	assert(filter->freg->configure_pid);
	safe_int_inc(&filter->session->pid_connect_tasks_pending);
	safe_int_inc(&filter->in_pid_connection_pending);
	gf_fs_post_task(filter->session, gf_filter_pid_connect_task, filter, pid, "pid_init", NULL);
}

void gf_filter_pid_post_init_task(GF_Filter *filter, GF_FilterPid *pid)
{
	if (pid->init_task_pending) return;
	safe_int_inc(&pid->init_task_pending);
	gf_fs_post_task(filter->session, gf_filter_pid_init_task, filter, pid, "pid_init", NULL);
}

void gf_filter_reconnect_output(GF_Filter *filter)
{
	u32 i;
	for (i=0; i<filter->num_output_pids; i++) {
		GF_FilterPid *pid = gf_list_get(filter->output_pids, i);
		gf_filter_pid_post_init_task(filter, pid);
	}
}


GF_Err gf_filter_pid_set_framing_mode(GF_FilterPid *pid, Bool requires_full_blocks)
{
	GF_FilterPidInst *pidinst = (GF_FilterPidInst *)pid;

	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to set framing info on an output pid in filter %s\n", pid->filter->name));
		return GF_BAD_PARAM;
	}
	pidinst->requires_full_data_block = requires_full_blocks;
	return GF_OK;
}

GF_FilterPid *gf_filter_pid_new(GF_Filter *filter)
{
	char szName[30];
	GF_FilterPid *pid;
	GF_SAFEALLOC(pid, GF_FilterPid);
	pid->filter = filter;
	pid->destinations = gf_list_new();
	pid->properties = gf_list_new();
	pid->request_property_map = GF_TRUE;
	if (!filter->output_pids) filter->output_pids = gf_list_new();
	gf_list_add(filter->output_pids, pid);
	filter->num_output_pids = gf_list_count(filter->output_pids);
	pid->pid = pid;
	pid->playback_speed_scaler = GF_FILTER_SPEED_SCALER;
	
	sprintf(szName, "PID%d", filter->num_output_pids);
	pid->name = gf_strdup(szName);

	filter->has_pending_pids = GF_TRUE;
	gf_fq_add(filter->pending_pids, pid);
	return pid;
}

void gf_filter_pid_del(GF_FilterPid *pid)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s pid %s destruction\n", pid->filter->name, pid->name));
	while (gf_list_count(pid->destinations)) {
		gf_filter_pid_inst_del( gf_list_pop_back(pid->destinations) );
	}
	gf_list_del(pid->destinations);

	while (gf_list_count(pid->properties)) {
		GF_PropertyMap *prop = gf_list_pop_back(pid->properties);
		if (safe_int_dec(&prop->reference_count)==0) {
			gf_props_del(prop);
		}
	}
	gf_list_del(pid->properties);

	//this one is NOT reference counted
	if(pid->caps_negociate && (safe_int_dec(&pid->caps_negociate->reference_count)==0) )
		gf_props_del(pid->caps_negociate);

	if (pid->adapters_blacklist)
		gf_list_del(pid->adapters_blacklist);

	if (pid->name) gf_free(pid->name);
	gf_free(pid);
}

void gf_filter_pid_del_task(GF_FSTask *task)
{
	gf_filter_pid_del(task->pid);
}

static GF_PropertyMap *check_new_pid_props(GF_FilterPid *pid, Bool merge_props)
{
	u32 i, nb_recf;
	GF_PropertyMap *old_map = gf_list_last(pid->properties);
	GF_PropertyMap *map;

	pid->props_changed_since_connect = GF_TRUE;
	if (!pid->request_property_map) {
		return old_map;
	}
	pid->request_property_map = GF_FALSE;
	pid->pid_info_changed = GF_TRUE;
	map = gf_props_new(pid->filter);
	if (!map) return NULL;
	gf_list_add(pid->properties, map);

	//when creating a new map, ref_count of old map is decremented
	if (old_map) {
		if (merge_props)
			gf_props_merge_property(map, old_map, NULL, NULL);
		assert(old_map->reference_count);
		if ( safe_int_dec(&old_map->reference_count) == 0) {
			gf_list_del_item(pid->properties, old_map);
			gf_props_del(old_map);
		}
	}

	//trick here: we may be reconfigured before any packet is being dispatched
	//so we need to manually trigger reconfigure of outputs
	nb_recf = 0;
	for (i=0; i<pid->num_destinations; i++) {
		GF_FilterPidInst *pidi = gf_list_get(pid->destinations, i);
		if (!pidi->filter->process_task_queued) {
			//remember the pid prop map to use
			pidi->reconfig_pid_props = map;
			nb_recf++;
		}
	}
	if (nb_recf)
		pid->filter->reconfigure_outputs = GF_TRUE;
	return map;
}

static GF_Err gf_filter_pid_set_property_full(GF_FilterPid *pid, u32 prop_4cc, const char *prop_name, char *dyn_name, const GF_PropertyValue *value, Bool is_info)
{
	GF_PropertyMap *map;

	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to write property on input PID in filter %s - ignoring\n", pid->filter->name));
		return GF_BAD_PARAM;
	}
	//info property, do not request a new property map
	if (is_info) {
		map = gf_list_last(pid->properties);
		if (!map) {
			map = gf_props_new(pid->filter);
			if (map) gf_list_add(pid->properties, map);
		}
	} else {
		//always merge properties
		map = check_new_pid_props(pid, GF_TRUE);
	}
	if (!map) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("No properties for destination pid in filter %s, ignoring reset\n", pid->filter->name));
		return GF_OUT_OF_MEM;
	}
	if (prop_4cc==GF_PROP_PID_TIMESCALE) map->timescale = value->value.uint;

	if (value && (prop_4cc == GF_PROP_PID_ID)) {
		char szName[100];
		sprintf(szName, "PID%d", value->value.uint);
		gf_filter_pid_set_name(pid, szName);
	}
	return gf_props_set_property(map, prop_4cc, prop_name, dyn_name, value);
}

GF_EXPORT
GF_Err gf_filter_pid_set_property(GF_FilterPid *pid, u32 prop_4cc, const GF_PropertyValue *value)
{
	if (!prop_4cc) return GF_BAD_PARAM;
	return gf_filter_pid_set_property_full(pid, prop_4cc, NULL, NULL, value, GF_FALSE);
}

GF_EXPORT
GF_Err gf_filter_pid_set_property_str(GF_FilterPid *pid, const char *name, const GF_PropertyValue *value)
{
	if (!name) return GF_BAD_PARAM;
	return gf_filter_pid_set_property_full(pid, 0, name, NULL, value, GF_FALSE);
}

GF_EXPORT
GF_Err gf_filter_pid_set_property_dyn(GF_FilterPid *pid, char *name, const GF_PropertyValue *value)
{
	if (!name) return GF_BAD_PARAM;
	return gf_filter_pid_set_property_full(pid, 0, NULL, name, value, GF_FALSE);
}

GF_EXPORT
GF_Err gf_filter_pid_set_info(GF_FilterPid *pid, u32 prop_4cc, const GF_PropertyValue *value)
{
	if (!prop_4cc) return GF_BAD_PARAM;
	return gf_filter_pid_set_property_full(pid, prop_4cc, NULL, NULL, value, GF_TRUE);
}

GF_EXPORT
GF_Err gf_filter_pid_set_info_str(GF_FilterPid *pid, const char *name, const GF_PropertyValue *value)
{
	if (!name) return GF_BAD_PARAM;
	return gf_filter_pid_set_property_full(pid, 0, name, NULL, value, GF_TRUE);
}

GF_EXPORT
GF_Err gf_filter_pid_set_info_dyn(GF_FilterPid *pid, char *name, const GF_PropertyValue *value)
{
	if (!name) return GF_BAD_PARAM;
	return gf_filter_pid_set_property_full(pid, 0, NULL, name, value, GF_TRUE);
}

static GF_Err gf_filter_pid_negociate_property_full(GF_FilterPid *pid, u32 prop_4cc, const char *prop_name, char *dyn_name, const GF_PropertyValue *value)
{
	GF_FilterPidInst *pidi = (GF_FilterPidInst *) pid;
	if (!prop_4cc) return GF_BAD_PARAM;

	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to negociate property on output PID in filter %s - ignoring\n", pid->filter->name));
		return GF_BAD_PARAM;
	}
	pid = pid->pid;
	if (!pid->caps_negociate) {
		assert(!pid->caps_negociate_pidi);
		pid->caps_negociate = gf_props_new(pid->filter);
		pid->caps_negociate_pidi = pidi;
		//we start a new caps negotiation step, reset any blacklist on pid
		if (pid->adapters_blacklist) {
			gf_list_del(pid->adapters_blacklist);
			pid->adapters_blacklist = NULL;
		}
		safe_int_inc(&pid->filter->nb_caps_renegociate);
	}
	return gf_props_set_property(pid->caps_negociate, prop_4cc, prop_name, dyn_name, value);
}

GF_EXPORT
GF_Err gf_filter_pid_negociate_property(GF_FilterPid *pid, u32 prop_4cc, const GF_PropertyValue *value)
{
	if (!prop_4cc) return GF_BAD_PARAM;
	return gf_filter_pid_negociate_property_full(pid, prop_4cc, NULL, NULL, value);
}

GF_EXPORT
GF_Err gf_filter_pid_negociate_property_str(GF_FilterPid *pid, const char *name, const GF_PropertyValue *value)
{
	if (!name) return GF_BAD_PARAM;
	return gf_filter_pid_negociate_property_full(pid, 0, name, NULL, value);
}

GF_EXPORT
GF_Err gf_filter_pid_negociate_property_dyn(GF_FilterPid *pid, char *name, const GF_PropertyValue *value)
{
	if (!name) return GF_BAD_PARAM;
	return gf_filter_pid_negociate_property_full(pid, 0, NULL, name, value);
}


static GF_PropertyMap *filter_pid_get_prop_map(GF_FilterPid *pid)
{
	if (PID_IS_INPUT(pid)) {
		GF_FilterPidInst *pidi = (GF_FilterPidInst *) pid;
		//first time we access the props, use the first entry in the property list
		if (!pidi->props) {
			pidi->props = gf_list_get(pid->pid->properties, 0);
			assert(pidi->props);
			safe_int_inc(&pidi->props->reference_count);
		}
		return pidi->props;
	} else {
		pid = pid->pid;
		return gf_list_last(pid->properties);
	}
	return NULL;
}

const GF_PropertyValue *gf_filter_pid_get_property(GF_FilterPid *pid, u32 prop_4cc)
{
	GF_PropertyMap *map = filter_pid_get_prop_map(pid);
	if (!map)
		return NULL;
	return gf_props_get_property(map, prop_4cc, NULL);
}

const GF_PropertyValue *gf_filter_pid_get_property_str(GF_FilterPid *pid, const char *prop_name)
{
	GF_PropertyMap *map = filter_pid_get_prop_map(pid);
	assert(map);
	return gf_props_get_property(map, 0, prop_name);
}

static const GF_PropertyValue *gf_filter_pid_get_info_internal(GF_FilterPid *pid, u32 prop_4cc, const char *prop_name)
{
	u32 i, count;
	const GF_PropertyValue * prop;
	GF_PropertyMap *map = filter_pid_get_prop_map(pid);
	assert(map);
	prop = gf_props_get_property(map, prop_4cc, prop_name);
	if (prop) return prop;

	count = gf_list_count(pid->filter->input_pids);
	for (i=0; i<count; i++) {
		GF_FilterPid *pidinst = gf_list_get(pid->filter->input_pids, i);
		prop = gf_filter_pid_get_info_internal(pidinst->pid, prop_4cc, prop_name);
		if (prop) return prop;
	}
	return NULL;
}

const GF_PropertyValue *gf_filter_pid_get_info(GF_FilterPid *pid, u32 prop_4cc)
{
	return gf_filter_pid_get_info_internal(pid, prop_4cc, NULL);
}
const GF_PropertyValue *gf_filter_pid_get_info_str(GF_FilterPid *pid, const char *prop_name)
{
	return gf_filter_pid_get_info_internal(pid, 0, prop_name);
}

static const GF_PropertyValue *gf_filter_get_info_internal(GF_Filter *filter, u32 prop_4cc, const char *prop_name)
{
	u32 i, count;
	const GF_PropertyValue * prop;

	//TODO avoid doing back and forth ...
	count = gf_list_count(filter->output_pids);
	for (i=0; i<count; i++) {
		GF_FilterPid *pid = gf_list_get(filter->output_pids, i);
		prop = gf_filter_pid_get_info_internal(pid, prop_4cc, prop_name);
		if (prop) return prop;
	}
	count = gf_list_count(filter->input_pids);
	for (i=0; i<count; i++) {
		GF_FilterPidInst *pidinst = gf_list_get(filter->input_pids, i);
		prop = gf_filter_pid_get_info_internal(pidinst->pid, prop_4cc, prop_name);
		if (prop) return prop;
	}
	return NULL;
}

GF_EXPORT
const GF_PropertyValue *gf_filter_get_info(GF_Filter *filter, u32 prop_4cc)
{
	return gf_filter_get_info_internal(filter, prop_4cc, NULL);
}

GF_EXPORT
const GF_PropertyValue *gf_filter_get_info_str(GF_Filter *filter, const char *prop_name)
{
	return gf_filter_get_info_internal(filter, 0, prop_name);
}

GF_Err gf_filter_pid_reset_properties(GF_FilterPid *pid)
{
	GF_PropertyMap *map;

	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to reset all properties on input PID in filter %s - ignoring\n", pid->filter->name));
		return GF_BAD_PARAM;
	}
	//don't merge properties, we will reset them anyway
	map = check_new_pid_props(pid, GF_FALSE);

	if (!map) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("No properties for destination pid in filter %s, ignoring reset\n", pid->filter->name));
		return GF_OUT_OF_MEM;
	}
	gf_props_reset(map);
	return GF_OK;

}

GF_Err gf_filter_pid_copy_properties(GF_FilterPid *dst_pid, GF_FilterPid *src_pid)
{
	GF_PropertyMap *dst_props, *src_props;

	if (PID_IS_INPUT(dst_pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to reset all properties on input PID in filter %s - ignoring\n", dst_pid->filter->name));
		return GF_BAD_PARAM;
	}
	//don't merge properties with old state we merge with source pid
	dst_props = check_new_pid_props(dst_pid, GF_FALSE);

	if (!dst_props) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("No properties for destination pid in filter %s, ignoring reset\n", dst_pid->filter->name));
		return GF_OUT_OF_MEM;
	}
	src_pid = src_pid->pid;
	src_props = gf_list_last(src_pid->properties);
	if (!src_props) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("No properties for source pid in filter %s, ignoring merge\n", src_pid->filter->name));
		return GF_OK;
	}
	if (src_pid->name) gf_filter_pid_set_name(dst_pid, src_pid->name);
	
	gf_props_reset(dst_props);
	return gf_props_merge_property(dst_props, src_props, NULL, NULL);
}

u32 gf_filter_pid_get_packet_count(GF_FilterPid *pid)
{
	GF_FilterPidInst *pidinst = (GF_FilterPidInst *)pid;
	if (PID_IS_OUTPUT(pid)) {
		pidinst = gf_list_get(pid->destinations, 0);
		if (! pidinst) return 0;
		return gf_fq_count(pidinst->packets) - pidinst->nb_eos_signaled - pidinst->nb_clocks_signaled;

	} else {
		if (pidinst->discard_packets) return 0;
		return gf_fq_count(pidinst->packets) - pidinst->nb_eos_signaled - pidinst->nb_clocks_signaled;
	}
}

static Bool gf_filter_pid_filter_internal_packet(GF_FilterPidInst *pidi, GF_FilterPacketInstance *pcki)
{
	Bool is_internal = GF_FALSE;
	if (pcki->pck->info.eos_type==1) {
		pcki->pid->is_end_of_stream = pcki->pid->pid->has_seen_eos ? GF_TRUE : GF_FALSE;
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Found EOS packet in PID %s in filter %s - eos %d\n", pidi->pid->name, pidi->filter->name, pcki->pid->pid->has_seen_eos));
		safe_int_dec(&pcki->pid->nb_eos_signaled);
		is_internal = GF_TRUE;
	} else if (pcki->pck->info.eos_type==2) {
		gf_fs_post_task(pidi->filter->session, gf_filter_pid_disconnect_task, pidi->filter, pidi->pid, "pidinst_disconnect", NULL);

		is_internal = GF_TRUE;
	}
	if (pcki->pck->info.clock_type) {
		if (pcki->pid->handles_clock_references) return GF_FALSE;
		safe_int_dec(&pcki->pid->nb_clocks_signaled);
		//signal destination
		assert(!pcki->pid->filter->next_clock_dispatch_type || !pcki->pid->filter->num_output_pids);

		pcki->pid->filter->next_clock_dispatch = pcki->pck->info.cts;
		pcki->pid->filter->next_clock_dispatch_timescale = pcki->pck->pid_props->timescale;
		pcki->pid->filter->next_clock_dispatch_type = pcki->pck->info.clock_type;

		//keep clock values but only override clock type if no discontinuity is pending
		pcki->pid->last_clock_value = pcki->pck->info.cts;
		pcki->pid->last_clock_timescale = pcki->pck->pid_props->timescale;
		if (pcki->pid->last_clock_type != GF_FILTER_CLOCK_PCR_DISC)
			pcki->pid->last_clock_type = pcki->pck->info.clock_type;

		if (pcki->pck->info.clock_type == GF_FILTER_CLOCK_PCR_DISC) {
			assert(pcki->pid->last_clock_type == GF_FILTER_CLOCK_PCR_DISC);
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Internal clock reference packet filtered - PID %s clock ref "LLU"/%d - type %d\n", pcki->pid->pid->name, pcki->pid->last_clock_value, pcki->pid->last_clock_timescale, pcki->pid->last_clock_type));
		//the following call to drop_packet will trigger clock forwarding to all output pids
		is_internal = GF_TRUE;
	}

	if (is_internal) gf_filter_pid_drop_packet((GF_FilterPid *)pidi);
	return is_internal;
}

GF_FilterPacket *gf_filter_pid_get_packet(GF_FilterPid *pid)
{
	GF_FilterPacketInstance *pcki;
	GF_FilterPidInst *pidinst = (GF_FilterPidInst *)pid;
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to fetch a packet on an output PID in filter %s\n", pid->filter->name));
		return NULL;
	}
	if (pidinst->discard_packets) return NULL;

	pcki = (GF_FilterPacketInstance *)gf_fq_head(pidinst->packets);
	//no packets
	if (!pcki) {
		return NULL;
	}
	assert(pcki->pck);

	if (gf_filter_pid_filter_internal_packet(pidinst, pcki))  {
		return gf_filter_pid_get_packet(pid);
	}
	pcki->pid->is_end_of_stream = GF_FALSE;

	if (pcki->pck->info.pid_props_changed && !pcki->pid_props_change_done) {
		GF_Err e;
		Bool skip_props = GF_FALSE;

		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s PID %s property changed at this packet, triggering reconfigure\n", pidinst->pid->filter->name, pidinst->pid->name));
		pcki->pid_props_change_done = 1;

		//it may happen that:
		//- the props are not set when querying the first packet (no prop queries on pid)
		//- the new props are already set if filter_pid_get_property was queried before the first packet dispatch
		if (pidinst->props) {
			if (pidinst->props != pcki->pck->pid_props) {
				//unassign old property list and set the new one
				if (safe_int_dec(& pidinst->props->reference_count) == 0) {
					gf_list_del_item(pidinst->pid->properties, pidinst->props);
					gf_props_del(pidinst->props);
				}
				pidinst->props = pcki->pck->pid_props;
				safe_int_inc( & pidinst->props->reference_count );
			} else {
				//it may happen that pid_configure for destination was called after packet being dispatched, in
				//which case we are already properly configured
				skip_props = GF_TRUE;
			}
		}
		if (!skip_props) {
			assert(pidinst->filter->freg->configure_pid);
			e = gf_filter_pid_configure(pidinst->filter, pidinst->pid, GF_PID_CONF_RECONFIG);
			if (e != GF_OK) return NULL;
		}
	}
	if (pcki->pck->info.pid_info_changed && !pcki->pid_info_change_done && pidinst->filter->freg->process_event) {
		GF_FilterEvent evt;
		pcki->pid_info_change_done = 1;
		GF_FEVT_INIT(evt, GF_FEVT_INFO_UPDATE, pid);

		FSESS_CHECK_THREAD(pidinst->filter)
		pidinst->filter->freg->process_event(pidinst->filter, &evt);
	}
	pidinst->last_pck_fetch_time = gf_sys_clock_high_res();

	return (GF_FilterPacket *)pcki;
}

Bool gf_filter_pid_get_first_packet_cts(GF_FilterPid *pid, u64 *cts)
{
	GF_FilterPacketInstance *pcki;
	GF_FilterPidInst *pidinst = (GF_FilterPidInst *)pid;
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to read packet CTS on an output PID in filter %s\n", pid->filter->name));
		return GF_FALSE;
	}
	if (pidinst->discard_packets) return GF_FALSE;

	pcki = (GF_FilterPacketInstance *)gf_fq_head(pidinst->packets);
	//no packets
	if (!pcki) {
		return GF_FALSE;
	}
	assert(pcki->pck);

	if (gf_filter_pid_filter_internal_packet(pidinst, pcki))  {
		return gf_filter_pid_get_first_packet_cts(pid, cts);
	}

	if (pidinst->requires_full_data_block && !pcki->pck->info.data_block_end)
		return GF_FALSE;
	*cts = pcki->pck->info.cts;
	return GF_TRUE;
}

Bool gf_filter_pid_first_packet_is_empty(GF_FilterPid *pid)
{
	GF_FilterPacketInstance *pcki;
	GF_FilterPidInst *pidinst = (GF_FilterPidInst *)pid;
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to read packet CTS on an output PID in filter %s\n", pid->filter->name));
		return GF_TRUE;
	}
	if (pidinst->discard_packets) return GF_TRUE;

	pcki = (GF_FilterPacketInstance *)gf_fq_head(pidinst->packets);
	//no packets
	if (!pcki) {
		return GF_TRUE;
	}
	assert(pcki->pck);

	if (pcki->pck->info.eos_type || pcki->pck->info.clock_type) {
		return GF_TRUE;
	}
	if (pidinst->requires_full_data_block && !pcki->pck->info.data_block_end)
		return GF_TRUE;
	return (pcki->pck->data_length || pcki->pck->hw_frame) ? GF_FALSE : GF_TRUE;
}


static void gf_filter_pidinst_update_stats(GF_FilterPidInst *pidi, GF_FilterPacket *pck)
{
	u64 now = gf_sys_clock_high_res();
	u64 dec_time = now - pidi->last_pck_fetch_time;
	if (pck->info.eos_type) return;
	if (pidi->pid->filter->removed) return;
	
	pidi->filter->nb_pck_processed++;
	pidi->filter->nb_bytes_processed += pck->data_length;

	pidi->total_process_time += dec_time;
	if (!pidi->nb_processed) {
		pidi->first_frame_time = pidi->last_pck_fetch_time;
	}

	pidi->nb_processed++;
	if (pck->info.sap_type) {
		pidi->nb_sap_processed ++;
		if (dec_time > pidi->max_sap_process_time) pidi->max_sap_process_time = dec_time;
		pidi->total_sap_process_time += dec_time;
	}

	if (dec_time > pidi->max_process_time) pidi->max_process_time = dec_time;

	if (pck->data_length) {
		u64 ts = (pck->info.dts != GF_FILTER_NO_TS) ? pck->info.dts : pck->info.cts;
		if (pck->pid_props->timescale) {
			ts *= 1000000;
			ts /= pck->pid_props->timescale;
		}
		
		if (!pidi->cur_bit_size || (pidi->stats_start_ts > ts)) {
			pidi->stats_start_ts = ts;
			pidi->stats_start_us = now;
			pidi->cur_bit_size = 8*pck->data_length;
		} else {
			if (pidi->stats_start_ts + 1000000 >= ts) {
				pidi->avg_bit_rate = (u32) (pidi->cur_bit_size * (1000000.0 / (ts - pidi->stats_start_ts) ) );
				if (pidi->avg_bit_rate > pidi->max_bit_rate) pidi->max_bit_rate = pidi->avg_bit_rate;

				pidi->avg_process_rate = (u32) (pidi->cur_bit_size * (1000000.0 / (now - pidi->stats_start_us) ) );
				if (pidi->avg_process_rate > pidi->max_process_rate) pidi->max_process_rate = pidi->avg_process_rate;

				pidi->stats_start_ts = ts;
				pidi->cur_bit_size = 0;
			}
			pidi->cur_bit_size += 8*pck->data_length;
		}
	}
}

static void gf_filter_pidinst_reset_stats(GF_FilterPidInst *pidi)
{
	pidi->last_pck_fetch_time = 0;
	pidi->stats_start_ts = 0;
	pidi->stats_start_us = 0;
	pidi->cur_bit_size = 0;
	pidi->avg_bit_rate = 0;
	pidi->max_bit_rate = 0;
	pidi->avg_process_rate = 0;
	pidi->max_process_rate = 0;
	pidi->nb_processed = 0;
	pidi->nb_sap_processed = 0;
	pidi->total_process_time = 0;
	pidi->total_sap_process_time = 0;
	pidi->max_process_time = 0;
	pidi->max_sap_process_time = 0;
	pidi->first_frame_time = 0;
}

void gf_filter_pid_drop_packet(GF_FilterPid *pid)
{
	u32 nb_pck=0;
	GF_FilterPacket *pck=NULL;
	GF_FilterPacketInstance *pcki;
	GF_FilterPidInst *pidinst = (GF_FilterPidInst *)pid;

	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to discard a packet on an output PID in filter %s\n", pid->filter->name));
		return;
	}
	//remove pck instance
	pcki = gf_fq_pop(pidinst->packets);

	if (!pcki) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Attempt to discard a packet already discarded in filter %s\n", pid->filter->name));
		return;
	}
	pck = pcki->pck;
	//move to source pid
	pid = pid->pid;

	nb_pck=gf_fq_count(pidinst->packets);

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_FILTER, GF_LOG_DEBUG)) {
		if ((pck->info.dts != GF_FILTER_NO_TS) && (pck->info.cts != GF_FILTER_NO_TS) ) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s (%s) drop packet DTS "LLU" CTS "LLU" SAP %d Seek %d - %d packets remaining buffer "LLU" us\n", pidinst->filter ? pidinst->filter->name : "disconnected", pid->name, pid->filter->name, pck->info.dts, pck->info.cts, pck->info.sap_type, pck->info.seek_flag, nb_pck, pidinst->buffer_duration));
		} else if ((pck->info.cts != GF_FILTER_NO_TS) ) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s (%s) drop packet CTS "LLU" SAP %d Seek %d - %d packets remaining buffer "LLU" us\n", pidinst->filter ? pidinst->filter->name : "disconnected", pid->name, pid->filter->name, pck->info.cts, pck->info.sap_type, pck->info.seek_flag, nb_pck, pidinst->buffer_duration));
		} else if ((pck->info.dts != GF_FILTER_NO_TS) ) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s (%s) drop packet DTS "LLU" SAP %d Seek %d - %d packets remaining buffer "LLU" us\n", pidinst->filter ? pidinst->filter->name : "disconnected", pid->name, pid->filter->name, pck->info.dts, pck->info.sap_type, pck->info.seek_flag, nb_pck, pidinst->buffer_duration));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s (%s) drop packet SAP %d Seek %d - %d packets remaining buffer "LLU" us\n", pidinst->filter ? pidinst->filter->name : "disconnected", pid->name, pid->filter->name, pck->info.sap_type, pck->info.seek_flag, nb_pck, pidinst->buffer_duration));
		}
	}
#endif

	gf_filter_pidinst_update_stats(pidinst, pck);

	if (nb_pck<pid->nb_buffer_unit) {
		//todo needs Compare&Swap
		pid->nb_buffer_unit = nb_pck;
	}

	if (!nb_pck) {
		safe_int_sub(&pidinst->buffer_duration, pidinst->buffer_duration);
	} else if (pck->info.duration && pck->info.data_block_start && pck->pid_props->timescale) {
		s64 d = ((u64)pck->info.duration) * 1000000;
		d /= pck->pid_props->timescale;
		assert(d <= pidinst->buffer_duration);
		safe_int_sub(&pidinst->buffer_duration, d);
	}

	if (!pid->buffer_duration || (pidinst->buffer_duration < pid->buffer_duration)) {
		//todo needs Compare&Swap
		pid->buffer_duration = pidinst->buffer_duration;
	}

	gf_filter_pid_check_unblock(pid);

	//destroy pcki
	pcki->pck = NULL;
	pcki->pid = NULL;

	gf_fq_add(pid->filter->pcks_inst_reservoir, pcki);

	//unref pck
	if (safe_int_dec(&pck->reference_count) == 0) {
		gf_filter_packet_destroy(pck);
	}
	//decrement number of pending packet on target filter if this is not a destroy
	if (pidinst->filter)
		safe_int_dec(&pidinst->filter->pending_packets);

	if (pidinst->filter)
		gf_filter_forward_clock(pidinst->filter);
}

Bool gf_filter_pid_is_eos(GF_FilterPid *pid)
{
	GF_FilterPacketInstance *pcki;
	GF_FilterPidInst *pidi = (GF_FilterPidInst *)pid;

	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to query EOS on output PID %s in filter %s\n", pid->pid->name, pid->filter->name));
		return GF_FALSE;
	}
	if (!pid->pid->has_seen_eos) {
		((GF_FilterPidInst *)pid)->is_end_of_stream = GF_FALSE;
		return GF_FALSE;
	}
	//peek next for eos
	pcki = (GF_FilterPacketInstance *)gf_fq_head(pidi->packets);
	if (pcki)
		gf_filter_pid_filter_internal_packet(pidi, pcki);

	return ((GF_FilterPidInst *)pid)->is_end_of_stream;
}

void gf_filter_pid_set_eos(GF_FilterPid *pid)
{
	GF_FilterPacket *pck;
	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to signal EOS on input PID %s in filter %s\n", pid->pid->name, pid->filter->name));
		return;
	}
	if (pid->pid->has_seen_eos) return;

	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("EOS signaled on PID %s in filter %s\n", pid->name, pid->filter->name));
	//we create a fake packet for eos signaling
	pck = gf_filter_pck_new_shared(pid, NULL, 0, NULL);
	gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);
	pck->pck->info.eos_type = 1;
	pid->pid->has_seen_eos = GF_TRUE;
	gf_filter_pck_send(pck);
}

const GF_PropertyValue *gf_filter_pid_enum_properties(GF_FilterPid *pid, u32 *idx, u32 *prop_4cc, const char **prop_name)
{
	GF_PropertyMap *props;

	if (PID_IS_INPUT(pid)) {
		props = gf_list_last(pid->pid->properties);
	} else {
		props = check_new_pid_props(pid, GF_FALSE);
	}
	if (!props) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("No properties for pid in filter %s, ignoring enum\n", pid->filter->name));
		*idx = 0xFFFFFFFF;
		return NULL;
	}
	return gf_props_enum_property(props, idx, prop_4cc, prop_name);
}

Bool gf_filter_pid_would_block(GF_FilterPid *pid)
{
	Bool would_block=GF_FALSE;

	if (PID_IS_INPUT(pid)) {
		return GF_FALSE;
	}

	if (pid->filter->session->disable_blocking)
		return GF_FALSE;

	//either block according to the number of dispatched units (decoder output) or to the requested buffer duration
	if (pid->max_buffer_unit) {
		if (pid->nb_buffer_unit * GF_FILTER_SPEED_SCALER >= pid->max_buffer_unit * pid->playback_speed_scaler) {
			would_block = GF_TRUE;
		}
	} else if (pid->max_buffer_time && (pid->buffer_duration * GF_FILTER_SPEED_SCALER > pid->max_buffer_time * pid->playback_speed_scaler) ) {
		would_block = GF_TRUE;
	}
	if (would_block && !pid->would_block) {
		safe_int_inc(&pid->would_block);
		safe_int_inc(&pid->filter->would_block);

#ifndef GPAC_DISABLE_LOG
		if (gf_log_tool_level_on(GF_LOG_FILTER, GF_LOG_DEBUG)) {
			if (pid->max_buffer_unit) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s blocked (%d units vs %d max units) - %d filter PIDs blocked\n", pid->pid->filter->name, pid->pid->name, pid->nb_buffer_unit, pid->max_buffer_unit, pid->filter->would_block));
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s blocked ("LLU" us vs "LLU" max buffer) - %d filter PIDs blocked\n", pid->pid->filter->name, pid->pid->name, pid->buffer_duration, pid->max_buffer_time, pid->filter->would_block));
			}
		}
#endif
	}
	return would_block;
}

u64 gf_filter_pid_query_buffer_duration(GF_FilterPid *pid, Bool check_decoder_output)
{
	u32 count, i, j;
	u64 duration=0;
	if (PID_IS_INPUT(pid)) {
		GF_Filter *filter;
		GF_FilterPidInst *pidinst = (GF_FilterPidInst *)pid;
		filter = pidinst->pid->filter;
		if (check_decoder_output && pidinst->pid->max_buffer_unit && (pidinst->pid->max_buffer_unit>pidinst->pid->nb_buffer_unit))
			return 0;
		count = filter->num_input_pids;
		for (i=0; i<count; i++) {
			u64 dur = gf_filter_pid_query_buffer_duration( gf_list_get(filter->input_pids, i), check_decoder_output);
			if (dur > duration)
				duration = dur;
		}
		duration += pidinst->buffer_duration;
		return duration;
	} else {
		u32 count2;
		u64 max_dur=0;
		if (check_decoder_output && pid->max_buffer_unit && (pid->max_buffer_unit > pid->nb_buffer_unit))
			return 0;
		count = pid->num_destinations;
		for (i=0; i<count; i++) {
			GF_FilterPidInst *pidinst = gf_list_get(pid->destinations, i);

			count2 = pidinst->filter->num_output_pids;
			for (j=0; j<count2; j++) {
				GF_FilterPid *pid_n = gf_list_get(pidinst->filter->output_pids, i);
				u64 dur = gf_filter_pid_query_buffer_duration(pid_n, check_decoder_output);
				if (dur > max_dur ) max_dur = dur;
			}
		}
		duration += max_dur;
	}
	return duration;
}


Bool gf_filter_pid_has_seen_eos(GF_FilterPid *pid)
{
	u32 i;
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to query EOS on output PID %s in filter %s\n", pid->pid->name, pid->filter->name));
		return GF_FALSE;
	}
	if (pid->pid->has_seen_eos) return GF_TRUE;

	for (i=0; i<pid->pid->filter->num_input_pids; i++) {
		GF_FilterPidInst *pidi = gf_list_get(pid->pid->filter->input_pids, i);
		if (gf_filter_pid_has_seen_eos((GF_FilterPid *) pidi)) return GF_TRUE;
	}
	return GF_FALSE;
}

const char *gf_filter_event_name(u32 type)
{
	switch (type) {
	case GF_FEVT_PLAY: return "PLAY";
	case GF_FEVT_SET_SPEED: return "SET_SPEED";
	case GF_FEVT_STOP: return "STOP";
	case GF_FEVT_SOURCE_SEEK: return "SOURCE_SEEK";
	case GF_FEVT_ATTACH_SCENE: return "ATTACH_SCENE";
	case GF_FEVT_RESET_SCENE: return "RESET_SCENE";
	case GF_FEVT_PAUSE: return "PAUSE";
	case GF_FEVT_RESUME: return "RESUME";
	case GF_FEVT_QUALITY_SWITCH: return "QUALITY_SWITCH";
	case GF_FEVT_VISIBILITY_HINT: return "VISIBILITY_HINT";
	case GF_FEVT_INFO_UPDATE: return "INFO_UPDATE";
	case GF_FEVT_BUFFER_REQ: return "BUFFER_REQ";
	case GF_FEVT_MOUSE: return "MOUSE";
	default: return "UNKNOWN";
	}
}

static void gf_filter_pid_reset_task(GF_FSTask *task)
{
	GF_FilterPidInst *pidi = (GF_FilterPidInst *)task->udta;
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s input PID %s (from %s) reseting buffer\n", task->filter->name, pidi->pid->name, pidi->pid->filter->name ));

	while (gf_fq_count(pidi->packets)) {
		gf_filter_pid_drop_packet((GF_FilterPid *) pidi);
	}
	while (gf_list_count(pidi->pck_reassembly)) {
		GF_FilterPacketInstance *pcki = gf_list_pop_back(pidi->pck_reassembly);
		pcki_del(pcki);
	}
	gf_filter_pidinst_reset_stats(pidi);

	pidi->discard_packets = GF_FALSE;
	pidi->last_block_ended = GF_TRUE;
	pidi->first_block_started = GF_FALSE;
	pidi->is_end_of_stream = GF_FALSE;
	pidi->buffer_duration = 0;
	pidi->nb_eos_signaled = 0;
	pidi->pid->has_seen_eos = GF_FALSE;

	safe_int_dec(& pidi->pid->filter->stream_reset_pending );

	pidi->pid->nb_buffer_unit = 0;
	pidi->pid->nb_buffer_unit = 0;

	assert(pidi->pid->discard_input_packets);
	safe_int_dec(& pidi->pid->discard_input_packets );
}

void gf_filter_pid_send_event_downstream(GF_FSTask *task)
{
	u32 i, count;
	Bool canceled = GF_FALSE;
	GF_FilterEvent *evt = task->udta;
	GF_Filter *f = task->filter;
	GF_List *dispatched_filters = NULL;

	//if stream reset task is posted, wait for it before processing this event
	if (f->stream_reset_pending) {
		TASK_REQUEUE(task)
		return;
	}
	//if some pids are still detached, wait for the connection before processing this event
	if (f->detached_pid_inst) {
		TASK_REQUEUE(task)
		return;
	}

	if (evt->base.on_pid) {
		assert(evt->base.on_pid->filter->num_events_queued);
		safe_int_dec(&evt->base.on_pid->filter->num_events_queued);
	}

	if (evt->base.type == GF_FEVT_BUFFER_REQ) {
		if (!evt->base.on_pid) {
			gf_free(evt);
			return;
		}
		if (evt->base.on_pid->nb_decoder_inputs || evt->base.on_pid->raw_media) {
			evt->base.on_pid->max_buffer_time = evt->base.on_pid->user_max_buffer_time = evt->buffer_req.max_buffer_us;
			evt->base.on_pid->user_max_playout_time = evt->buffer_req.max_playout_us;
			//update blocking state
			if (evt->base.on_pid->would_block)
				gf_filter_pid_check_unblock(evt->base.on_pid);
			else
				gf_filter_pid_would_block(evt->base.on_pid);
			canceled = GF_TRUE;
		}
	} else if (evt->base.on_pid && (evt->base.type == GF_FEVT_PLAY) && evt->base.on_pid->pid->is_playing) {
		gf_free(evt);
		return;
	} else if (evt->base.on_pid && (evt->base.type == GF_FEVT_STOP) && !evt->base.on_pid->pid->is_playing) {
		gf_free(evt);
		return;
	} else if (f->freg->process_event) {
		FSESS_CHECK_THREAD(f)
		canceled = f->freg->process_event(f, evt);
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s PID %s processed event %s - canceled %s\n", f->name, evt->base.on_pid ? evt->base.on_pid->name : "none", gf_filter_event_name(evt->base.type), canceled ? "yes" : "no" ));

	if (evt->base.on_pid && ((evt->base.type == GF_FEVT_STOP) || (evt->base.type==GF_FEVT_SOURCE_SEEK) || (evt->base.type==GF_FEVT_PLAY)) ) {
		u32 i;
		Bool do_reset = GF_TRUE;
		Bool is_play_reset = GF_FALSE;
		GF_FilterPidInst *p = (GF_FilterPidInst *) evt->base.on_pid;
		GF_FilterPid *pid = p->pid;
		//we need to force a PID reset when the first PLAY is > 0, since some filters may have dispatched packets during the initialization
		//phase
		if (evt->base.type==GF_FEVT_PLAY) {
			pid->is_playing = GF_TRUE;
			if (pid->initial_play_done) {
				do_reset = GF_FALSE;
			} else {
				pid->initial_play_done = GF_TRUE;
				is_play_reset = GF_TRUE;
				if (evt->play.start_range < 0.1)
					do_reset = GF_FALSE;
			}
		} else if (evt->base.type==GF_FEVT_STOP) {
			pid->is_playing = GF_FALSE;
		}
		for (i=0; i<pid->num_destinations && do_reset; i++) {
			GF_FilterPidInst *pidi = gf_list_get(pid->destinations, i);
			pidi->discard_packets = GF_TRUE;
			if (is_play_reset)
				safe_int_inc(& pid->discard_input_packets );
				
			safe_int_inc(& pid->filter->stream_reset_pending );
			//post task on destination filter
			gf_fs_post_task(pidi->filter->session, gf_filter_pid_reset_task, pidi->filter, NULL, "reset_pid", pidi);
		}
		pid->nb_reaggregation_pending = 0;
	}
	//after  play or seek, request a process task for source filters or filters having pending packets
	if (!f->input_pids || f->pending_packets) {
		if ((evt->base.type==GF_FEVT_PLAY) || (evt->base.type==GF_FEVT_SOURCE_SEEK)) {
			gf_filter_post_process_task(f);
		}
	}
	if ((evt->base.type==GF_FEVT_PLAY) || (evt->base.type==GF_FEVT_SET_SPEED)) {
		if (evt->base.on_pid) {
			u32 scaler = (u32)  ( (evt->play.speed<0) ? -evt->play.speed : evt->play.speed ) * GF_FILTER_SPEED_SCALER;
			if (!scaler) scaler = GF_FILTER_SPEED_SCALER;
			if (scaler != evt->base.on_pid->playback_speed_scaler) {
				u32 prev_scaler = evt->base.on_pid->playback_speed_scaler;
				evt->base.on_pid->playback_speed_scaler = scaler;
				//lowering speed, we may need to trigger blocking
				if (scaler<prev_scaler)
					gf_filter_pid_would_block(evt->base.on_pid);
				//increasing speed, we may want to unblock
				else
					gf_filter_pid_check_unblock(evt->base.on_pid);
			}
		}
	}

	//no more input pids
	count = f->num_input_pids;
	if (count==0) canceled = GF_TRUE;

	if (canceled) {
		gf_free(evt);
		return;
	}
	if (!task->pid) dispatched_filters = gf_list_new();

	//otherwise forward event to each input PID
	for (i=0; i<count; i++) {
		GF_FilterEvent *an_evt;
		GF_FilterPidInst *pid_inst = gf_list_get(f->input_pids, i);
		GF_FilterPid *pid = pid_inst->pid;
		if (!pid) continue;

		if (dispatched_filters) {
			if (gf_list_find(dispatched_filters, pid_inst->pid->filter) >=0 )
				continue;

			gf_list_add(dispatched_filters, pid_inst->pid->filter);
		}

		//mark pid instance as about to be reset to avoid processing PID destroy task before
		if ((evt->base.type == GF_FEVT_STOP) || (evt->base.type==GF_FEVT_SOURCE_SEEK)) {
			pid_inst->discard_packets = GF_TRUE;
			safe_int_inc(& pid_inst->pid->discard_input_packets );
		}
		//allocate a copy except for the last PID where we use the one from the input
		if (i+1<count) {
			an_evt = gf_malloc(sizeof(GF_FilterEvent));
			memcpy(an_evt, evt, sizeof(GF_FilterEvent));
		} else {
			an_evt = evt;
		}
		an_evt->base.on_pid = task->pid ? pid : NULL;

		safe_int_inc(&pid->filter->num_events_queued);
		
		gf_fs_post_task(pid->filter->session, gf_filter_pid_send_event_downstream, pid->filter, task->pid ? pid : NULL, "downstream_event", an_evt);
	}
	if (dispatched_filters) gf_list_del(dispatched_filters);
	return;
}

void gf_filter_pid_send_event_upstream(GF_FSTask *task)
{
	u32 i, j;
	Bool canceled = GF_FALSE;
	GF_FilterEvent *evt = task->udta;
	GF_Filter *f = task->filter;

	if (f->stream_reset_pending) {
		TASK_REQUEUE(task)
		return;
	}

	assert(! evt->base.on_pid);

	canceled = f->freg->process_event ? f->freg->process_event(f, evt) : GF_TRUE;
	if (!canceled) {
		for (i=0; i<f->num_output_pids; i++) {
			GF_FilterPid *apid = gf_list_get(f->output_pids, i);
			for (j=0; j<apid->num_destinations; j++) {
				GF_FilterEvent *dup_evt;
				GF_FilterPidInst *pidi = gf_list_get(apid->destinations, j);

				dup_evt = gf_malloc(sizeof(GF_FilterEvent));
				memcpy(dup_evt, evt, sizeof(GF_FilterEvent));
				dup_evt->base.on_pid = NULL;
				gf_fs_post_task(pidi->filter->session, gf_filter_pid_send_event_upstream, pidi->filter, NULL, "upstream_event", dup_evt);
			}
		}
	}
	gf_free(evt);
}

void gf_filter_pid_send_event(GF_FilterPid *pid, GF_FilterEvent *evt)
{
	GF_FilterEvent *dup_evt;
	GF_FilterPid *target_pid=NULL;
	Bool upstream=GF_FALSE;
	if (!pid) {
		pid = evt->base.on_pid;
		if (!pid) return;
	}
	//filter is being shut down, prevent any event posting
	if (pid->filter->finalized) return;

	if (PID_IS_OUTPUT(pid)) {
		upstream = GF_TRUE;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s PID %s queuing %s event %s\n", pid->pid->filter->name, pid->pid->name, upstream ? "upstream" : "downstream", gf_filter_event_name(evt->base.type) ));

	if (upstream) {
		u32 i, j;
		for (i=0; i<pid->filter->num_output_pids; i++) {
			GF_FilterPid *apid = gf_list_get(pid->filter->output_pids, i);
			if (evt->base.on_pid && (apid != evt->base.on_pid)) continue;
			for (j=0; j<apid->num_destinations; j++) {
				GF_FilterPidInst *pidi = gf_list_get(apid->destinations, j);
				dup_evt = gf_malloc(sizeof(GF_FilterEvent));
				memcpy(dup_evt, evt, sizeof(GF_FilterEvent));
				dup_evt->base.on_pid = NULL;
				gf_fs_post_task(pidi->filter->session, gf_filter_pid_send_event_upstream, pidi->filter, NULL, "upstream_event", dup_evt);
			}
		}
		return;
	}


	if ((evt->base.type == GF_FEVT_STOP) || (evt->base.type == GF_FEVT_PLAY) || (evt->base.type==GF_FEVT_SOURCE_SEEK)) {
		u32 i, count = pid->pid->num_destinations;
		for (i=0; i<count; i++) {
			GF_FilterPidInst *pidi = gf_list_get(pid->pid->destinations, i);
			if (evt->base.type == GF_FEVT_PLAY) {
				pidi->is_end_of_stream = GF_FALSE;
			} else {
				//flag pid instance to discard all packets
				pidi->discard_packets = GF_TRUE;
				safe_int_inc(& pidi->pid->discard_input_packets );
			}
		}
	}

	dup_evt = gf_malloc(sizeof(GF_FilterEvent));
	memcpy(dup_evt, evt, sizeof(GF_FilterEvent));
	if (evt->base.on_pid) {
		target_pid = evt->base.on_pid->pid;
		dup_evt->base.on_pid = target_pid;
		safe_int_inc(&target_pid->filter->num_events_queued);
	}
	gf_fs_post_task(pid->pid->filter->session, gf_filter_pid_send_event_downstream, pid->pid->filter, target_pid, "downstream_event", dup_evt);

}


void gf_filter_send_event(GF_Filter *filter, GF_FilterEvent *evt)
{
	GF_FilterEvent *dup_evt;
	//filter is being shut down, prevent any event posting
	if (filter->finalized) return;

	if (evt->base.on_pid && PID_IS_OUTPUT(evt->base.on_pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Sending filter events upstream not yet implemented (PID %s in filter %s)\n", evt->base.on_pid->pid->name, filter->name));
		return;
	}

	dup_evt = gf_malloc(sizeof(GF_FilterEvent));
	memcpy(dup_evt, evt, sizeof(GF_FilterEvent));

	if (evt->base.on_pid) {
		safe_int_inc(&evt->base.on_pid->filter->num_events_queued);
	}

	gf_fs_post_task(filter->session, gf_filter_pid_send_event_downstream, filter, evt->base.on_pid, "downstream_event", dup_evt);
}


void gf_filter_pid_exec_event(GF_FilterPid *pid, GF_FilterEvent *evt)
{
	//filter is being shut down, prevent any event posting
	if (pid->pid->filter->finalized) return;
	assert (pid->pid->filter->freg->requires_main_thread);

	if (pid->pid->filter->freg->process_event) {
		if (evt->base.on_pid) evt->base.on_pid = evt->base.on_pid->pid;
		FSESS_CHECK_THREAD(pid->pid->filter)
		pid->pid->filter->freg->process_event(pid->pid->filter, evt);
	}
}


Bool gf_filter_pid_is_filter_in_parents(GF_FilterPid *pid, GF_Filter *filter)
{
	if (!pid || !filter) return GF_FALSE;
	pid = pid->pid;
	return filter_in_parent_chain(pid->pid->filter, filter);
}


GF_Err gf_filter_pid_get_statistics(GF_FilterPid *pid, GF_FilterPidStatistics *stats)
{
	GF_FilterPidInst *pidi = (GF_FilterPidInst *)pid;
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Sending filter events upstream not yet implemented (PID %s in filter %s)\n", pid->pid->name, pid->filter->name));
		return GF_BAD_PARAM;
	}
	memset(stats, 0, sizeof(GF_FilterPidStatistics) );
	stats->avgerage_bitrate = pidi->avg_bit_rate;
	stats->first_process_time = pidi->first_frame_time;
	stats->last_process_time = pidi->last_pck_fetch_time;
	stats->max_bitrate = pidi->max_bit_rate;
	stats->max_process_time = pidi->max_process_time;
	stats->max_sap_process_time = pidi->max_sap_process_time;
	stats->min_frame_dur = pidi->pid->min_pck_duration;
	stats->nb_processed = pidi->nb_processed;
	stats->nb_saps = pidi->nb_sap_processed;
	stats->total_process_time = pidi->total_process_time;
	stats->total_sap_process_time = pidi->total_sap_process_time;

	stats->average_process_rate = pidi->avg_process_rate;
	stats->max_process_rate = pidi->max_process_rate;

	return GF_OK;
}

void gf_filter_pid_remove(GF_FilterPid *pid)
{
	GF_FilterPacket *pck;
	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Removing PID input filter (%s:%s) not allowed\n", pid->filter->name, pid->pid->name));
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s removed request output PID %s\n", pid->filter->name, pid->pid->name));

	if (pid->filter->removed) {
		return;
	}
	if (pid->removed) {
		return;
	}
	pid->removed = GF_TRUE;

	//we create a fake packet for eos signaling
	pck = gf_filter_pck_new_shared(pid, NULL, 0, NULL);
	gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);
	pck->pck->info.eos_type = 2;
	gf_filter_pck_send(pck);
}

void gf_filter_pid_try_pull(GF_FilterPid *pid)
{
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to pull from output PID %s in filter %s\n", pid->pid->name, pid->filter->name));
		return;
	}
	pid = pid->pid;
	if (pid->filter->session->threads) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Filter pull in multithread mode not yet implementing - defaulting to 1 ms sleep\n", pid->pid->name, pid->filter->name));
		gf_sleep(1);
		return;
	}

	gf_filter_process_inline(pid->filter);
}



GF_FilterClockType gf_filter_pid_get_clock_info(GF_FilterPid *pid, u64 *clock_time, u32 *timescale)
{
	GF_FilterPidInst *pidi = (GF_FilterPidInst *)pid;
	GF_FilterClockType res;
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Querying clock on output PID %s in filter %s\n", pid->pid->name, pid->filter->name));
		return GF_FILTER_CLOCK_NONE;
	}
	if (clock_time) *clock_time = pidi->last_clock_value;
	if (timescale) *timescale = pidi->last_clock_timescale;
	res = pidi->last_clock_type;
	pidi->last_clock_type = 0;
	return res;
}

u32 gf_filter_pid_get_timescale(GF_FilterPid *pid)
{
	GF_PropertyMap *map = pid ? gf_list_get(pid->pid->properties, 0) : 0;
	return map ? map->timescale : 0;
}

void gf_filter_pid_clear_eos(GF_FilterPid *pid)
{
	GF_FilterPidInst *pidi = (GF_FilterPidInst *)pid;
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Clearing EOS on output PID %s in filter %s\n", pid->pid->name, pid->filter->name));
		return;
	}
	pidi->is_end_of_stream = GF_FALSE;
}

void gf_filter_pid_set_clock_mode(GF_FilterPid *pid, Bool filter_in_charge)
{
	GF_FilterPidInst *pidi = (GF_FilterPidInst *)pid;
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Changing clock mode on output PID %s in filter %s\n", pid->pid->name, pid->filter->name));
		return;
	}
	pidi->handles_clock_references = filter_in_charge;
}

const char *gf_filter_pid_get_args(GF_FilterPid *pid)
{
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Querying args on output PID %s in filter %s\n", pid->pid->name, pid->filter->name));
		return NULL;
	}
	if (pid->pid->filter->src_args) return pid->pid->filter->src_args;
	return pid->pid->filter->orig_args;
}

void gf_filter_pid_set_max_buffer(GF_FilterPid *pid, u32 total_duration_us)
{
	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Setting max buffer on input PID %s in filter %s not allowed\n", pid->pid->name, pid->filter->name));
		return;
	}
	pid->max_buffer_time = pid->user_max_buffer_time = total_duration_us;
}

u32 gf_filter_pid_get_max_buffer(GF_FilterPid *pid)
{
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Querying max buffer on output PID %s in filter %s not allowed\n", pid->pid->name, pid->filter->name));
		return 0;
	}
	return pid->pid->user_max_buffer_time;
}


void gf_filter_pid_set_loose_connect(GF_FilterPid *pid)
{
	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Setting loose connect on input PID %s in filter %s not allowed\n", pid->pid->name, pid->filter->name));
		return;
	}
	pid->not_connected_ok = GF_TRUE;
}

const GF_PropertyValue *gf_filter_pid_caps_query(GF_FilterPid *pid, u32 prop_4cc)
{
	u32 i;
	GF_PropertyMap *map = pid->caps_negociate;
	if (PID_IS_INPUT(pid)) {
		u32 k;
		GF_Filter *dst;
		if (!pid->filter->dst_filter || (pid->filter->dst_filter->cap_idx_at_resolution<0) ) {
			GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Reconfig caps query on input PID %s in filter %s with no destination filter set\n", pid->pid->name, pid->filter->name));
			return NULL;
		}
		dst = pid->filter->dst_filter;
		for (k=dst->cap_idx_at_resolution; k<dst->freg->nb_caps; k++) {
			const GF_FilterCapability *cap = &dst->freg->caps[k];
			if (!(cap->flags & GF_FILTER_CAPS_IN_BUNDLE)) return NULL;

			if (!(cap->flags & GF_FILTER_CAPS_INPUT)) continue;
			if (cap->code == prop_4cc) return &cap->val;
		}
		return NULL;
	}
	if (map) return gf_props_get_property(map, prop_4cc, NULL);
	for (i=0; i<pid->num_destinations; i++) {
		u32 j;
		GF_FilterPidInst *pidi = gf_list_get(pid->destinations, i);
		for (j=0; j<pidi->filter->nb_forced_caps; j++) {
			if (pidi->filter->forced_caps[i].code==prop_4cc)
				return &pidi->filter->forced_caps[i].val;
		}
	}

	//trick here: we may not be connected yet (called during a configure_pid), use the target destination
	//of the filter as caps source
	if (pid->filter->dst_filter) {
		for (i=0; i<pid->filter->dst_filter->nb_forced_caps; i++) {
			if (pid->filter->dst_filter->forced_caps[i].code==prop_4cc)
				return &pid->filter->dst_filter->forced_caps[i].val;
		}
	}

	return NULL;
}

const GF_PropertyValue *gf_filter_pid_caps_query_str(GF_FilterPid *pid, const char *prop_name)
{
	GF_PropertyMap *map = pid->caps_negociate;
	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Reconfig caps query on input PID %s in filter %s not allowed\n", pid->pid->name, pid->filter->name));
		return NULL;
	}
	return map ? gf_props_get_property(map, 0, prop_name) : NULL;
}


GF_Err gf_filter_pid_resolve_file_template(GF_FilterPid *pid, char szTemplate[GF_MAX_PATH], char szFinalName[GF_MAX_PATH], u32 file_idx)
{
	u32 k;
	char szFormat[10], szTemplateVal[GF_MAX_PATH], szPropVal[100];
	char *name = szTemplate;
	k = 0;
	while (name[0]) {
		char *sep=NULL;
		char *fsep=NULL;
		const char *str_val = NULL;
		s64 value = 0;
		Bool is_ok = GF_TRUE;
		Bool has_val = GF_FALSE;
		Bool is_file_str = GF_FALSE;
		u32 prop_4cc = 0;
		GF_PropertyValue prop_val_patched;
		const GF_PropertyValue *prop_val = NULL;

		if (k+1==GF_MAX_PATH) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("[Filter] Not enough memory to solve file template %s\n", szTemplate));
			return GF_OUT_OF_MEM;
		}
		if (name[0] != '$') {
			szFinalName[k] = name[0];
			k++;
			name++;
			continue;
		}
		if (name[1]=='$') {
			szFinalName[k] = '$';
			name++;
			k++;
			continue;
		}
		sep = strchr(name+1, '$');
		if (!sep) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[Filter] broken file template expecting $KEYWORD$, couln't find second '$'\n", szTemplate));
			strcpy(szFinalName, szTemplate);
			return GF_BAD_PARAM;
		}
		szFinalName[k] = 0;
		name++;
		sep[0]=0;
		fsep = strchr(name, '%');
		if (fsep) fsep[0] = 0;
		if (!stricmp(name, "Number") || !stricmp(name, "num")) {
			u32 len = (!stricmp(name, "num")) ? 3 : 6;
			name += len;
			value = file_idx;
			has_val = GF_TRUE;
		} else if (!stricmp(name, "URL")) {
			prop_val = gf_filter_pid_get_property(pid, GF_PROP_PID_URL);
			is_file_str = GF_TRUE;
		} else if (!stricmp(name, "File")) {
			prop_val = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
			is_file_str = GF_TRUE;
		} else if (!stricmp(name, "PID")) {
			prop_val = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		} else if (!strnicmp(name, "p4cc=", 5)) {
			if (strlen(name) != 9) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[Filter] wrong length in 4CC template, expecting 4cc=ABCD\n", name));
				is_ok = GF_FALSE;
			} else {
				prop_4cc = GF_4CC(name[5],name[6],name[7],name[8]);
				prop_val = gf_filter_pid_get_property(pid, prop_4cc);
				if (!prop_val) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[Filter] no pid property of type %s\n", name+5));
					is_ok = GF_FALSE;
				}
			}
		} else if (!strnicmp(name, "pname=", 6)) {
			prop_val = gf_filter_pid_get_property_str(pid, name+6);
			if (!prop_val) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[Filter] no pid property named %s\n", name+6));
				is_ok = GF_FALSE;
			}
		} else {
			prop_4cc = gf_props_get_id(name);
			//not matching, try with name
			if (!prop_4cc) {
				prop_val = gf_filter_pid_get_property_str(pid, name);
				if (!prop_val) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[Filter] Unrecognized template %s\n", name));
					is_ok = GF_FALSE;
				}
			} else {
				prop_val = gf_filter_pid_get_property(pid, prop_4cc);
				if (!prop_val) {
					is_ok = GF_FALSE;
				}
			}
		}

		if (!is_ok && !prop_val && prop_4cc) {
			if (prop_4cc==GF_PROP_PID_CROP_POS) {
				prop_val_patched.type = GF_PROP_VEC2I;
				prop_val_patched.value.vec2i.x = 0;
				prop_val_patched.value.vec2i.y = 0;
				prop_val = &prop_val_patched;
				is_ok=GF_TRUE;
			}
			else if (prop_4cc==GF_PROP_PID_ORIG_SIZE) {
				prop_val_patched.type = GF_PROP_VEC2I;
				prop_val = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
				prop_val_patched.value.vec2i.x = prop_val ? prop_val->value.uint : 0;
				prop_val = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
				prop_val_patched.value.vec2i.y = prop_val ? prop_val->value.uint : 0;
				prop_val = &prop_val_patched;
				is_ok=GF_TRUE;
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[Filter] property %s not found for pid, cannot resolve template\n", name));
			}
		}

		if (!is_ok) {
			if (fsep) fsep[0] = '%';
			if (sep) sep[0] = '$';
			return GF_BAD_PARAM;
		}
		if (prop_val) {
			if ((prop_val->type==GF_PROP_UINT) || (prop_val->type==GF_PROP_SINT)) {
				value = prop_val->value.uint;
				has_val = GF_TRUE;
			} else {
				str_val = gf_prop_dump_val(prop_val, szPropVal, GF_FALSE);
			}
		}

		if (has_val) {
			if (fsep) {
				strcpy(szFormat, "%");
				strcat(szFormat, fsep+1);
			} else {
				strcpy(szFormat, "%d");
			}
			sprintf(szTemplateVal, szFormat, value);
			str_val = szTemplateVal;
		} else if (str_val) {
			if (is_file_str) {
				char *ext;
				char *sname = strrchr(str_val, '/');
				if (!sname) sname = strrchr(str_val, '\\');
				if (!sname) sname = (char *) str_val;
				else sname++;
				ext = strrchr(str_val, '.');

				if (ext) {
					u32 len = ext - sname;
					strncpy(szTemplateVal, sname, ext - sname);
					szTemplateVal[len] = 0;
				} else {
					strcpy(szTemplateVal, sname);
				}
			} else {
				strcpy(szTemplateVal, str_val);
			}
		}
		if (k + strlen(szTemplateVal) > GF_MAX_PATH) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("[Filter] Not enough memory to solve file template %s\n", szTemplate));
			return GF_OUT_OF_MEM;
		}

		strcat(szFinalName, szTemplateVal);
		k = strlen(szFinalName);

		if (fsep) fsep[0] = '%';
		if (!sep) break;
		sep[0] = '$';
		name = sep+1;
	}
	szFinalName[k] = 0;
	return GF_OK;
}
