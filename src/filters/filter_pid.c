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

void pcki_del(GF_FilterPacketInstance *pcki)
{
	ref_count_dec(&pcki->pck->reference_count);
	if (!pcki->pck->reference_count) {
		gf_filter_packet_destroy(pcki->pck);
	}
	gf_free(pcki);
}

void gf_filter_pid_inst_del(GF_FilterPidInst *pidinst)
{
	assert(pidinst);

	gf_fq_del(pidinst->packets, (gf_destruct_fun) pcki_del);
	gf_mx_del(pidinst->pck_mx);

	while (gf_list_count(pidinst->pck_reassembly)) {
		GF_FilterPacketInstance *pcki = gf_list_pop_back(pidinst->pck_reassembly);
		pcki_del(pcki);
	}
	gf_list_del(pidinst->pck_reassembly);

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

Bool gf_filter_pid_configure(GF_Filter *filter, GF_FilterPid *pid, GF_PID_ConfigState state)
{
	u32 i, count;
	GF_Err e;
	Bool new_pid_inst=GF_FALSE;
	GF_FilterPidInst *pidinst=NULL;

	assert(filter->freg->configure_pid);

	count = gf_list_count(pid->destinations);
	for (i=0; i<count; i++) {
		pidinst = gf_list_get(pid->destinations, i);
		if (pidinst->filter==filter) {
			break;
		}
		pidinst=NULL;
	}

	//first connection of this PID to this filter
	if (!pidinst) {
		GF_SAFEALLOC(pidinst, GF_FilterPidInst);
		pidinst->pid = pid;
		pidinst->filter = filter;

		if (filter->session->use_locks) {
			char szName[200];
			u32 pid_idx = 1 + gf_list_find(pid->filter->output_pids, pid);
			u32 dst_idx = 1 + gf_list_count(pid->destinations);
			snprintf(szName, 200, "F%sPid%dDest%dPackets", filter->name, pid_idx, dst_idx);
			pidinst->pck_mx = gf_mx_new(szName);
		}

		pidinst->packets = gf_fq_new(pidinst->pck_mx);

		pidinst->pck_reassembly = gf_list_new();
		pidinst->requires_full_data_block = GF_TRUE;
		pidinst->last_block_ended = GF_TRUE;
		new_pid_inst=GF_TRUE;
	}

	e = filter->freg->configure_pid(filter, (GF_FilterPid*) pidinst, state);

	if (e==GF_OK) {
		//if new, register the new pid instance, and the source pid as input to this filer
		if (new_pid_inst) {
			gf_list_add(pid->destinations, pidinst);

			if (!filter->input_pids) filter->input_pids = gf_list_new();
			assert(gf_list_find(filter->input_pids, pid)<0);
			gf_list_add(filter->input_pids, pid);
		}
	} else {
		//error, if old pid remove from input
		if (!new_pid_inst) {
			gf_list_del_item(pid->destinations, pidinst);
			gf_list_del_item(filter->input_pids, pid);
		}
		gf_filter_pid_inst_del(pidinst);
	}

	if (state==GF_PID_CONFIG_CONNECT) {
		assert(pid->filter->pid_connection_pending);
		ref_count_dec(&pid->filter->pid_connection_pending);
		if (!pid->filter->pid_connection_pending && !pid->filter->input_pids) {
			//source filters, flush data
			gf_fs_post_task(filter->session, gf_filter_process, pid->filter, NULL, "process");
		}
	}
	return GF_FALSE;
}

Bool gf_filter_pid_connect(GF_FSTask *task)
{
	//destination filter
	GF_FilterPid *pid = task->pid->pid;
	GF_Filter *filter = task->filter;
	return gf_filter_pid_configure(filter, pid, GF_PID_CONFIG_CONNECT);
}

Bool gf_filter_pid_reconfigure(GF_FSTask *task)
{
	//destination filter
	GF_FilterPid *pid = task->pid->pid;
	GF_Filter *filter = task->filter;
	return gf_filter_pid_configure(filter, pid, GF_PID_CONFIG_UPDATE);
}

Bool gf_filter_pid_disconnect(GF_FSTask *task)
{
	//destination filter
	GF_FilterPid *pid = task->pid->pid;
	GF_Filter *filter = task->filter;
	return gf_filter_pid_configure(filter, pid, GF_PID_CONFIG_DISCONNECT);
}


static Bool filter_source_id_match(const char *id, const char *source_ids)
{
	if (!source_ids) return GF_TRUE;
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


Bool gf_filter_pid_init(GF_FSTask *task)
{
	u32 i, count;

	task->filter->pid_connection_pending = 0;
	//try to connect pid
	count = gf_list_count(task->filter->session->filters);
	for (i=0; i<count; i++) {
		GF_Filter *filter = gf_list_get(task->filter->session->filters, i);
		//we don't allow re-entrant PIDs
		if (filter==task->filter) continue;

		if (!filter_source_id_match(task->filter->id, filter->source_ids)) continue;

		if (filter->freg->configure_pid) {
			ref_count_inc(&task->filter->pid_connection_pending);
			gf_fs_post_task(filter->session, gf_filter_pid_connect, filter, task->pid, "pid_connect");
		}
	}
	//update intenal pid caps
	gf_filter_pid_update_internal_caps(task->pid);
	return GF_FALSE;
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
	GF_FilterPid *pid;
	GF_SAFEALLOC(pid, GF_FilterPid);
	pid->filter = filter;
	pid->destinations = gf_list_new();
	pid->properties = gf_list_new();
	pid->request_property_map = GF_TRUE;
	if (!filter->output_pids) filter->output_pids = gf_list_new();
	gf_list_add(filter->output_pids, pid);
	pid->pid = pid;
	gf_fs_post_task(filter->session, gf_filter_pid_init, filter, pid, "pid_init");

	return pid;
}

void gf_filter_pid_del(GF_FilterPid *pid)
{
	while (gf_list_count(pid->destinations)) {
		gf_filter_pid_inst_del( gf_list_pop_back(pid->destinations) );
	}
	gf_list_del(pid->destinations);

	while (gf_list_count(pid->properties)) {
		gf_props_del( gf_list_pop_back(pid->properties) );
	}
	gf_list_del(pid->properties);
	gf_free(pid);
}

static GF_PropertyMap *check_new_pid_props(GF_FilterPid *pid, Bool merge_props)
{
	GF_PropertyMap *old_map = gf_list_last(pid->properties);
	GF_PropertyMap *map = gf_props_new(pid->filter);

	gf_list_add(pid->properties, map);
	pid->request_property_map = 0;

	//when creating a new map, ref_count of old map is decremented
	if (old_map) {
		if (merge_props)
			gf_props_merge_property(map, old_map);
		assert(old_map->reference_count);
		ref_count_dec(&old_map->reference_count);
		if (!old_map->reference_count) {
			gf_list_del_item(pid->properties, old_map);
			gf_props_del(old_map);
		}
	}
	return map;
}

static GF_Err gf_filter_pid_set_property_full(GF_FilterPid *pid, u32 prop_4cc, const char *prop_name, char *dyn_name, const GF_PropertyValue *value)
{
	GF_PropertyMap *map;

	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to write property on input PID in filter %s - ignoring\n", pid->filter->name));
		return GF_BAD_PARAM;
	}
	if (pid->request_property_map) {
		//always merge properties
		map = check_new_pid_props(pid, GF_TRUE);
		pid->request_property_map = 0;
	} else {
		map = gf_list_last(pid->properties);
	}
	assert(map);
	return gf_props_set_property(map, prop_4cc, prop_name, dyn_name, value);
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

const GF_PropertyValue *gf_filter_pid_get_property(GF_FilterPid *pid, u32 prop_4cc, const char *prop_name)
{
	GF_PropertyMap *map;
	pid = pid->pid;
	map = gf_list_last(pid->properties);
	assert(map);
	return gf_props_get_property(map, prop_4cc, prop_name);
}

GF_Err gf_filter_pid_reset_properties(GF_FilterPid *pid)
{
	GF_PropertyMap *map;

	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to reset all properties on input PID in filter %s - ignoring\n", pid->filter->name));
		return GF_BAD_PARAM;
	}
	if (pid->request_property_map) {
		//always merge properties
		map = check_new_pid_props(pid, GF_TRUE);
		pid->request_property_map = 0;
	} else {
		map = gf_list_last(pid->properties);
	}
	gf_props_reset(map);
	return GF_OK;

}


GF_FilterPacket *gf_filter_pid_get_packet(GF_FilterPid *pid)
{
	GF_FilterPacket *pck;
	GF_FilterPidInst *pidinst = (GF_FilterPidInst *)pid;
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to fetch a packet on an output PID in filter %s\n", pid->filter->name));
		return NULL;
	}

	pck = (GF_FilterPacket *)gf_fq_head(pidinst->packets);

	if (!pck)
		return NULL;
	
	if (pidinst->requires_full_data_block && !pck->pck->data_block_end)
		return NULL;

	assert(pck->pck);
	return pck;
}

void gf_filter_pid_drop_packet(GF_FilterPid *pid)
{
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
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to discard a packet from another pid in filter %s\n", pid->filter->name));
		return;
	}
	pck = pcki->pck;
	//move to source pid
	pid = pid->pid;

	//destroy pcki
	pcki->pck = NULL;
	pcki->pid = NULL;

	gf_fq_add(pid->filter->pcks_inst_reservoir, pcki);

	//unref pck
	ref_count_dec(&pck->reference_count);
	if (!pck->reference_count) {
		gf_filter_packet_destroy(pck);
	}
	//decrement number of pending packet on target filter
	ref_count_dec(&pidinst->filter->pending_packets);
}


