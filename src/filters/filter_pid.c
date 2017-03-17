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
#include <gpac/constants.h>

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

Bool gf_filter_pid_resolve_link(GF_FilterPid *pid, GF_FilterRegister *dst);

static Bool gf_filter_pid_configure(GF_Filter *filter, GF_FilterPid *pid, Bool is_connect, Bool is_remove)
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

	e = filter->freg->configure_pid(filter, (GF_FilterPid*) pidinst, is_remove);

	if (e==GF_OK) {
		//if new, register the new pid instance, and the source pid as input to this filer
		if (new_pid_inst) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Connected filter %s PID %s to filter %s\n", pid->filter->name,  pid->name, filter->name));
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

		if (e==GF_REQUIRES_NEW_INSTANCE) {
			//TODO: copy over args from current filter
			GF_Filter *new_filter = gf_filter_clone(filter);
			if (new_filter) {
				//watchout, we must post the tesk on the source filter, otherwise we may have concurrency
				//issues setting up the pid destinations if done in parallel. We therefore set udat to the target filter
				gf_fs_post_task(pid->filter->session, gf_filter_pid_connect_task, pid->filter, pid, "pid_connect", new_filter);
			} else {
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to connect filter %s PID %s to filter %s\n", pid->filter->name, pid->name, filter->name));

			if (filter->freg->output_caps) {
				//todo - try to load another filter to handle that connection
				gf_list_add(pid->filter->blacklisted, (void *) filter->freg);
				//gf_filter_pid_resolve_link(pid, filter->freg);
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to reconfigure input of sink %s, cannot rebuild graph\n", filter->name));
			}
		}
		return GF_FALSE;
	}

	//flush all pending pid init requests following the call to init
	while (gf_fq_count(filter->pending_pids)) {
		GF_FilterPid *pid=gf_fq_pop(filter->pending_pids);
		gf_fs_post_task(filter->session, gf_filter_pid_init_task, filter, pid, "pid_init", NULL);
	}

	if (is_connect) {
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

	return gf_filter_pid_configure(filter, pid, GF_TRUE, GF_FALSE);
}

Bool gf_filter_pid_reconfigure_task(GF_FSTask *task)
{
	//destination filter
	GF_FilterPid *pid = task->pid->pid;
	GF_Filter *filter = task->filter;
	return gf_filter_pid_configure(filter, pid, GF_FALSE, GF_FALSE);
}

/*
Bool gf_filter_pid_disconnect(GF_FSTask *task)
{
	//destination filter
	GF_FilterPid *pid = task->pid->pid;
	GF_Filter *filter = task->filter;
	return gf_filter_pid_configure(filter, pid, GF_FALSE, GF_TRUE);
}
*/

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
			if (!strcmp(src_pid->name, pid_name)) return GF_TRUE;

			//special case for stream types filters
			name = gf_filter_pid_get_property(src_pid, GF_PROP_PID_STREAM_TYPE);
			if (name) {
				u32 matched=0;
				u32 type=0;
				if (!strnicmp(pid_name, "audio", 5) && (name->value.uint==GF_STREAM_AUDIO)) {
					matched=5;
					type=GF_STREAM_AUDIO;
				} else if (!strnicmp(pid_name, "video", 5) && (name->value.uint==GF_STREAM_VISUAL)) {
					matched=5;
					type=GF_STREAM_VISUAL;
				} else if (!strnicmp(pid_name, "scene", 5) && (name->value.uint==GF_STREAM_SCENE)) {
					matched=5;
					type=GF_STREAM_SCENE;
				} else if (!strnicmp(pid_name, "font", 4) && (name->value.uint==GF_STREAM_FONT)) {
					matched=5;
					type=GF_STREAM_FONT;
				} else if (!strnicmp(pid_name, "text", 4) && (name->value.uint==GF_STREAM_TEXT)) {
					matched=5;
					type=GF_STREAM_TEXT;
				}

				if (matched) {
					u32 idx=0;
					u32 k, count_pid;
					if (strlen(pid_name)==matched) return GF_TRUE;
					idx = atoi(pid_name+matched);
					count_pid = gf_list_count(src_pid->filter->output_pids);
					for (k=0; k<count_pid; k++) {
						GF_FilterPid *p = gf_list_get(src_pid->filter->output_pids, k);
						name = gf_filter_pid_get_property(src_pid, GF_PROP_PID_STREAM_TYPE);
						if (name && name->value.uint==type) {
							idx--;
							if (!idx) {
								if (p==src_pid) return GF_TRUE;
								break;
							}
						}
					}
				}
			}

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
	//browse all parent PIDs
	count = gf_list_count(parent->input_pids);
	if (!count) return GF_FALSE;
	for (i=0; i<count; i++) {
		GF_FilterPidInst *pid = gf_list_get(parent->input_pids, i);
		if (filter_in_parent_chain(pid->filter, filter)) return GF_TRUE;
	}
	return GF_FALSE;
}

static u32 filter_pid_caps_match(GF_FilterPid *src_pid, const GF_FilterRegister *freg)
{
	u32 i=0;
	u32 nb_matched=0;

	//filters with no explicit input cap accept anything 
	if (!freg->input_caps) return 1;

	//check all input caps of dst filter
	while (freg->input_caps) {
		const GF_PropertyValue *pid_cap=NULL;
		const GF_FilterCapability *cap = &freg->input_caps[i];
		i++;
		if (!cap || (!cap->cap_code && !cap->cap_string) ) break;
		if (cap->cap_code) {
			pid_cap = gf_filter_pid_get_property(src_pid, cap->cap_code);
		}
		if (!pid_cap) pid_cap = gf_filter_pid_get_property_str(src_pid, cap->cap_string);

		//we found a property of that type and it is equal
		if (pid_cap) {
			if (gf_props_equal(pid_cap, &cap->p)) {
				nb_matched++;
			} else if (cap->mandatory) {
				return GF_FALSE;
			}
		}

	}
	return nb_matched;
}
#if 0
const GF_FilterCapability *filter_find_input_cap(GF_FilterRegister *freg, char *name)
{
	u32 i=0;
	Bool matched=GF_FALSE;
	//check all output caps of dst filter
	while (freg->output_caps) {
		GF_PropertyValue capv, *pid_cap=NULL;
		const GF_FilterCapability *cap = &freg->output_caps[i];
		i++;
		if (!cap || !cap->cap_name) break;
		if (!strcmp(name, cap->cap_name)) return cap;
	}
	return NULL;
}

static u32 filter_caps_match(const GF_FilterRegister *src, const GF_FilterRegister *dst)
{
	u32 i=0;
	u32 nb_matched=0;
	u32 nb_mandatory=0;

	//count all mandatory input caps of dst filter
	while (dst->input_caps) {
		const GF_FilterCapability *in_cap = &src->input_caps[i];
		i++;
		if (!in_cap || !in_cap->cap_name) break;
		if (in_cap->mandatory) nb_mandatory++;
	}

	//check all output caps of src filter
	i=0;
	while (src->output_caps) {
		u32 j=0;
		Bool matched=GF_FALSE;
		GF_PropertyValue capv, *pid_cap=NULL;
		const GF_FilterCapability *out_cap = &src->output_caps[i];
		i++;
		if (!out_cap || !out_cap->cap_name) break;

		//check all input caps of dst filter, count ones that are matched
		while (dst->input_caps) {
			const GF_FilterCapability *in_cap = &dst->input_caps[j];
			j++;
			if (!in_cap || !in_cap->cap_name) break;
			if (strcmp(out_cap->cap_name, in_cap->cap_name)) continue;

			//we found a property of that type and it is equal
			if (gf_props_equal(&in_cap->p, &out_cap->p)) {
				matched = GF_TRUE;
				if (in_cap->mandatory) nb_mandatory--;
			}
		}
		//output cap matching this input cap, increment counter
		if (matched) nb_matched++;
	}
	//some mandatory input caps not provided by the input filter, don't use
	if (nb_mandatory) return 0;
	return nb_matched;
}

u32 gf_filter_check_dst_caps(GF_FilterSession *fsess, GF_FilterRegister *filter, GF_List *black_list, GF_List *filter_chain, GF_FilterRegister *dst_filter)
{
	u32 nb_matched = 0;
	const GF_FilterRegister *candidate = NULL;
	//browse all our registered filters
	u32 i, count=gf_list_count(fsess->registry);
	u32 count_at_input = gf_list_count(filter_chain);

	for (i=0; i<count; i++) {
		u32 path_weight=0;
		GF_Filter *filter_dst;
		const GF_FilterRegister *freg = gf_list_get(fsess->registry, i);
		if (freg==filter) continue;

		//source filter, can't add pid
		if (!freg->configure_pid) continue;
		
		//blacklisted filter, can't add pid
		if (gf_list_find(black_list, freg)>=0)
			continue;

		path_weight = filter_caps_match(filter, freg);
		if (!path_weight) continue;

		//we found our target filter
		if (freg==dst_filter) {
			return path_weight;
		}
		//check this filter
		else {
			path_weight += gf_filter_check_dst_caps(fsess, freg, black_list, filter_chain, dst_filter);
		}
		if (path_weight>nb_matched) {
			//remove all entries added in recursive gf_filter_check_dst_caps
			while (gf_list_count(filter_chain)>count_at_input) {
				gf_list_rem_last(filter_chain);
			}
			nb_matched = path_weight;
			candidate = freg;
		}
	}
	if (candidate) gf_list_add(filter_chain, candidate);
	return nb_matched;
}

Bool gf_filter_pid_resolve_link(GF_FilterPid *pid, GF_FilterRegister *dst_filter)
{
	GF_FilterSession *fsess = pid->filter->session;
	GF_List *filter_chain = gf_list_new();
	u32 max_weight=0;
	//browse all our registered filters
	u32 i, count=gf_list_count(fsess->registry);
	for (i=0; i<count; i++) {
		u32 freg_weight=0;
		u32 path_weight=0;
		u32 path_len=0;
		GF_Filter *filter_dst;
		const GF_FilterRegister *freg = gf_list_get(fsess->registry, i);

		//source filter, can't add pid
		if (!freg->configure_pid) continue;

		//blacklisted filter, can't add pid
		if (gf_list_find(pid->filter->blacklisted, freg)>=0)
			continue;

		//no match of pid caps for this filter
		freg_weight = filter_pid_caps_match(pid, freg);
		if (!freg_weight) continue;

		//we have a target destination filter match, keep solving filter until done
		path_len = gf_list_count(filter_chain);

		gf_list_add(filter_chain, freg);
		path_weight = gf_filter_check_dst_caps(fsess, freg, pid->filter->blacklisted, filter_chain, dst_filter);

		//not our candidate, remove all added entries
		if (!path_weight) {
			while (gf_list_count(filter_chain) > path_len) {
				gf_list_rem_last(filter_chain);
			}
			continue;
		}
		//
		if (path_weight+freg_weight > max_weight) {
			max_weight = path_weight+freg_weight;
			//remove initial entries
			while (path_len) {
				gf_list_rem(filter_chain, 0);
				path_len--;
			}
		} else {
			//remove all added entries
			while (gf_list_count(filter_chain) > path_len) {
				gf_list_rem_last(filter_chain);
			}
		}
	}

	if (gf_list_count(filter_chain)==0) {
		//no filter found for this pid !
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("No suitable filter found for PID in filter %s - NOT CONNECTED\n", pid->filter->name));
	} else {

	}
	gf_list_del(filter_chain);
}

#endif

Bool gf_filter_pid_init_task(GF_FSTask *task)
{
	u32 i, count;
	Bool found_dest=GF_FALSE;

	//try to connect pid to all running filters
	count = gf_list_count(task->filter->session->filters);
	for (i=0; i<count; i++) {
		GF_Filter *filter_dst = gf_list_get(task->filter->session->filters, i);
		//source filter
		if (!filter_dst->freg->configure_pid) continue;
		//walk up through the parent graph and check if this filter is already in. If so don't connect
		//since we don't allow re-entrant PIDs
		if (filter_in_parent_chain(task->filter, filter_dst) ) continue;

		//if the original filter is in the parent chain of this PID's filter, don't connect (equivalent to re-entrant)
		if (filter_dst->cloned_from) {
			if (filter_in_parent_chain(task->filter, filter_dst->cloned_from) ) continue;
		}
		//if the filter is in the parent chain of this PID's original filter, don't connect (equivalent to re-entrant)
		if (task->filter->cloned_from) {
			if (filter_in_parent_chain(task->filter->cloned_from, filter_dst) ) continue;
		}
		//if we have sourceID info on the pid, check them
		if (!filter_source_id_match(task->pid, task->filter->id, filter_dst->source_ids)) continue;

		//we have a match, check if caps are OK
		if (!filter_pid_caps_match(task->pid, filter_dst->freg)) continue;

		ref_count_inc(&task->filter->pid_connection_pending);
		//watchout, we must post the tesk on the source filter, otherwise we may have concurrency
		//issues setting up the pid destinations if done in parallel. We therefore set udat to the target filter
		gf_fs_post_task(filter_dst->session, gf_filter_pid_connect_task, task->filter, task->pid, "pid_connect", filter_dst);

		found_dest = GF_TRUE;
	}
	
	//connection in proces, do nothing
	if (found_dest) return GF_FALSE;

	//try to load filters ? for now disabled
//	gf_filter_pid_resolve_link(task->pid, GF_TRUE);

	//no filter found for this pid !
	GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("No filter found for PID %s in filter %s - NOT CONNECTED\n", task->pid->name, task->pid->filter->name));
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
	char szName[30];
	GF_FilterPid *pid;
	GF_SAFEALLOC(pid, GF_FilterPid);
	pid->filter = filter;
	pid->destinations = gf_list_new();
	pid->properties = gf_list_new();
	pid->request_property_map = GF_TRUE;
	if (!filter->output_pids) filter->output_pids = gf_list_new();
	gf_list_add(filter->output_pids, pid);
	pid->pid = pid;

	sprintf(szName, "PID%d", gf_list_count(filter->output_pids) );
	pid->name = gf_strdup(szName);

	gf_fq_add(filter->pending_pids, pid);

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
	if (pid->name) gf_free(pid->name);
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

	if (pck->data_length) {
		pidinst->filter->nb_pck_processed++;
		pidinst->filter->nb_bytes_processed += pck->data_length;
	}

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


void gf_filter_pid_set_eos(GF_FilterPid *pid)
{
	GF_FilterPacket *pck;
	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to signal EOS on inpt PID %s in filter %s\n", pid->pid->name, pid->filter->name));
		return;
	}
	pck = gf_filter_pck_new_shared(pid, NULL, 0, NULL);
	gf_filter_pck_set_property(pck, GF_PROP_PCK_EOS, &PROP_BOOL(GF_TRUE) );
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

