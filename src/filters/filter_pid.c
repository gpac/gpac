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
	if (ref_count_dec(&pcki->pck->reference_count) == 0) {
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

static GF_FilterPidInst *gf_filter_pid_inst_new(GF_Filter *filter, GF_FilterPid *pid)
{
	GF_FilterPidInst *pidinst;
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
	return pidinst;
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
		pidinst = gf_filter_pid_inst_new(filter, pid);
		new_pid_inst=GF_TRUE;
	}

	e = filter->freg->configure_pid(filter, (GF_FilterPid*) pidinst, state);

	if (e==GF_OK) {
		//if new, register the new pid instance, and the source pid as input to this filer
		if (new_pid_inst) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Connected filter %s PID to filter %s\n", pid->filter->name, filter->name));
			gf_list_add(pid->destinations, pidinst);

			if (!filter->input_pids) filter->input_pids = gf_list_new();
			assert(gf_list_find(filter->input_pids, pid)<0);
			gf_list_add(filter->input_pids, pid);
		}
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to connect filter %s PID to filter %s\n", pid->filter->name, filter->name));
		//error, if old pid remove from input
		if (!new_pid_inst) {
			gf_list_del_item(pid->destinations, pidinst);
			gf_list_del_item(filter->input_pids, pid);
		}
		gf_filter_pid_inst_del(pidinst);
	}

	if (state==GF_PID_CONFIG_CONNECT) {
		assert(pid->filter->pid_connection_pending);
		if ( (ref_count_dec(&pid->filter->pid_connection_pending)==0) ) {

			//source filters, start flushing data
			if (!pid->filter->input_pids) {
				gf_fs_post_task(filter->session, gf_filter_process_task, pid->filter, NULL, "process", NULL);
			}
			//other filters with packets ready in inputs, start processing
			else if (pid->filter->pending_packets) {
				gf_fs_post_task(filter->session, gf_filter_process_task, pid->filter, NULL, "process", NULL);
			}
		}
	}
	//TODO - once all pid have been (re)connected, update any internal caps
	return GF_FALSE;
}

Bool gf_filter_pid_connect_task(GF_FSTask *task)
{
	//destination filter
	GF_FilterPid *pid = task->pid->pid;
	GF_Filter *filter = (GF_Filter *) task->udta;

	return gf_filter_pid_configure(filter, pid, GF_PID_CONFIG_CONNECT);
}

Bool gf_filter_pid_reconfigure_task(GF_FSTask *task)
{
	//destination filter
	GF_FilterPid *pid = task->pid->pid;
	GF_Filter *filter = task->filter;
	return gf_filter_pid_configure(filter, pid, GF_PID_CONFIG_UPDATE);
}

/*
Bool gf_filter_pid_disconnect(GF_FSTask *task)
{
	//destination filter
	GF_FilterPid *pid = task->pid->pid;
	GF_Filter *filter = task->filter;
	return gf_filter_pid_configure(filter, pid, GF_PID_CONFIG_DISCONNECT);
}
*/

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


static Bool filter_source_id_match(GF_FilterPid *src_pid, const char *id, const char *source_ids)
{
	if (!source_ids)
		return GF_TRUE;
	if (!id)
		return GF_FALSE;
	while (source_ids) {
		u32 len, sublen;
		Bool last=GF_FALSE;
		char *sep = strchr(source_ids, ',');
		char *pid_name;
		if (sep) {
			len = sep - source_ids;
		} else {
			len = strlen(source_ids);
			last=GF_TRUE;
		}

		pid_name = strchr(source_ids, '#');
		if (pid_name > source_ids + len) pid_name = NULL;
		sublen = pid_name ? pid_name - source_ids : len;
		//skip #
		if (pid_name) pid_name++;

		//match id
		if (!strncmp(id, source_ids, sublen)) {
			const GF_PropertyValue *name;
			if (!pid_name) return GF_TRUE;

			//match pid name
			name = gf_filter_pid_get_property(src_pid, GF_FILTER_PID_NAME);
			if (name && !strcmp(name->value.string, pid_name)) return GF_TRUE;

			//TODO: match by PID type #audioX, #videoX
			if (!name) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Unsupported PID adressing #%s in filter %s\n", pid_name, src_pid->filter->name));
			}
		}


		source_ids += len;
		if (last) break;
	}
	return GF_FALSE;
}

static Bool filter_in_parent_chain(GF_Filter *parent, GF_Filter *filter)
{
	u32 i, count;
	if (parent == filter) return GF_TRUE;
	count = gf_list_count(parent->input_pids);
	if (!count) return GF_FALSE;
	for (i=0; i<count; i++) {
		GF_FilterPidInst *pid = gf_list_get(parent->input_pids, i);
		if (filter_in_parent_chain(pid->filter, filter)) return GF_TRUE;
	}
	return GF_FALSE;
}

Bool gf_filter_pid_init_task(GF_FSTask *task)
{
	u32 i, count;

	//try to connect pid
	count = gf_list_count(task->filter->session->filters);
	for (i=0; i<count; i++) {
		GF_Filter *filter_dst = gf_list_get(task->filter->session->filters, i);
		//source filter
		if (!filter_dst->freg->configure_pid) continue;
		//walk up through the parent graph and check if this filter is already in. If so don't connect
		//since we don't allow re-entrant PIDs
		if (filter_in_parent_chain(task->filter, filter_dst) ) continue;

		if (!filter_source_id_match(task->pid, task->filter->id, filter_dst->source_ids)) continue;

		ref_count_inc(&task->filter->pid_connection_pending);
		//watchout, we must post the tesk on the source filter, otherwise we may have concurrency
		//issues setting up the pid destinations if done in parallel. We therefore set udat to the target filter
		gf_fs_post_task(filter_dst->session, gf_filter_pid_connect_task, task->filter, task->pid, "pid_connect", filter_dst);
	}
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
	gf_fs_post_task(filter->session, gf_filter_pid_init_task, filter, pid, "pid_init", NULL);

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
	GF_PropertyMap *map;

	if (!pid->request_property_map) {
		return old_map;
	}
	pid->request_property_map = 0;
	map = gf_props_new(pid->filter);
	gf_list_add(pid->properties, map);

	//when creating a new map, ref_count of old map is decremented
	if (old_map) {
		if (merge_props)
			gf_props_merge_property(map, old_map);
		assert(old_map->reference_count);
		if ( ref_count_dec(&old_map->reference_count) == 0) {
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
	//always merge properties
	map = check_new_pid_props(pid, GF_TRUE);
	if (!map) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("No properties for destination pid in filter %s, ignoring reset\n", pid->filter->name));
		return GF_OUT_OF_MEM;
	}
	return gf_props_set_property(map, prop_4cc, prop_name, dyn_name, value);
}

GF_Err gf_filter_pid_set_property(GF_FilterPid *pid, u32 prop_4cc, const GF_PropertyValue *value)
{
	if (!prop_4cc) return GF_BAD_PARAM;
	return gf_filter_pid_set_property_full(pid, prop_4cc, NULL, NULL, value);
}

GF_Err gf_filter_pid_set_property_str(GF_FilterPid *pid, const char *name, const GF_PropertyValue *value)
{
	if (!name) return GF_BAD_PARAM;
	return gf_filter_pid_set_property_full(pid, 0, name, NULL, value);
}

GF_Err gf_filter_pid_set_property_dyn(GF_FilterPid *pid, char *name, const GF_PropertyValue *value)
{
	if (!name) return GF_BAD_PARAM;
	return gf_filter_pid_set_property_full(pid, 0, NULL, name, value);
}

const GF_PropertyValue *gf_filter_pid_get_property(GF_FilterPid *pid, u32 prop_4cc)
{
	GF_PropertyMap *map;
	pid = pid->pid;
	map = gf_list_last(pid->properties);
	assert(map);
	return gf_props_get_property(map, prop_4cc, NULL);
}

const GF_PropertyValue *gf_filter_pid_get_property_str(GF_FilterPid *pid, const char *prop_name)
{
	GF_PropertyMap *map;
	pid = pid->pid;
	map = gf_list_last(pid->properties);
	assert(map);
	return gf_props_get_property(map, 0, prop_name);
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

	return gf_props_merge_property(dst_props, src_props);
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
	if (ref_count_dec(&pck->reference_count) == 0) {
		gf_filter_packet_destroy(pck);
	}
	//decrement number of pending packet on target filter
	ref_count_dec(&pidinst->filter->pending_packets);
}


