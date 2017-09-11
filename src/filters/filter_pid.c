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
	if (safe_int_dec(&pcki->pck->reference_count) == 0) {
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

GF_Filter *gf_filter_pid_resolve_link(GF_FilterPid *pid, GF_Filter *dst);

static void gf_filter_pid_update_caps(GF_FilterPid *pid, GF_Filter *dst_filter)
{
	u32 mtype=0, oti=0;
	const GF_PropertyValue *p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (p) mtype = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_OTI);
	if (p) oti = p->value.uint;

	if ((mtype==GF_STREAM_VISUAL) && (oti==GPAC_OTI_RAW_MEDIA_STREAM)) {
		pid->max_buffer_unit = 4;
	}
	else if ((mtype==GF_STREAM_AUDIO) && (oti==GPAC_OTI_RAW_MEDIA_STREAM)) {
		pid->max_buffer_unit = 8;
	}
	//output is a decoded stream: if some input has same type but different OTI this is a decoder
	//set input buffer size
	if (oti==GPAC_OTI_RAW_MEDIA_STREAM) {
		u32 i, count=pid->filter->num_input_pids;
		for (i=0; i<count; i++) {
			u32 i_oti, i_type=0;
			GF_FilterPidInst *pidi = gf_list_get(pid->filter->input_pids, i);

			p = gf_filter_pid_get_property(pidi->pid, GF_PROP_PID_STREAM_TYPE);
			if (p) i_type = p->value.uint;

			p = gf_filter_pid_get_property(pidi->pid, GF_PROP_PID_OTI);
			if (p) i_oti = p->value.uint;

			//same stream type but changing format type: this is a decoder
			//set buffer req
			if ((mtype==i_type) && (oti != i_oti)) {
				if (!pidi->pid->max_buffer_time)
					pidi->pid->max_buffer_time = 3000000;
			}
		}
	}
}

void gf_filter_pid_inst_delete_task(GF_FSTask *task)
{
	GF_FilterPid *pid = task->pid;
	GF_FilterPidInst *pidinst = task->udta;
	GF_Filter *filter = pid->filter;

	//reset in process
	if ((pidinst->filter && pidinst->discard_input_packets) || filter->stream_reset_pending) {
		task->requeue_request = GF_TRUE;
		return;
	}
	//WARNING at this point pidinst->filter may be destroyed
	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s pid instance %s destruction task\n",  filter->name, pid->name));
	gf_list_del_item(pid->destinations, pidinst);
	gf_filter_pid_inst_del(pidinst);

	//no more pids on filter, destroy it
	if (! gf_list_count(pid->destinations)) {
		gf_list_del_item(filter->output_pids, pid);
		gf_filter_pid_del(pid);
	}
	if (!gf_list_count(filter->output_pids) ) {
		filter->finalized = GF_TRUE;
		assert(!gf_list_count(filter->input_pids));
		gf_fs_post_task(filter->session, gf_filter_remove_task, filter, NULL, "filter_destroy", NULL);
	}
}

void gf_filter_pid_configure(GF_Filter *filter, GF_FilterPid *pid, Bool is_connect, Bool is_remove)
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
			GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Connected filter %s PID %s to filter %s\n", pid->filter->name,  pid->name, filter->name));
			gf_list_add(pid->destinations, pidinst);

			if (!filter->input_pids) filter->input_pids = gf_list_new();
			gf_list_add(filter->input_pids, pidinst);
			filter->num_input_pids = gf_list_count(filter->input_pids);
		}
	} else {
		//error, if old pid remove from input
		if (!new_pid_inst) {
			gf_list_del_item(filter->input_pids, pidinst);
			gf_list_del_item(pidinst->pid->destinations, pidinst);
			pidinst->filter = NULL;
			filter->num_input_pids = gf_list_count(filter->input_pids);
		}
		//if connect or reconfigure and error, direct delete of pid
		if (!is_remove) {
			gf_list_del_item(pid->destinations, pidinst);
			gf_filter_pid_inst_del(pidinst);
		}

		if (e==GF_REQUIRES_NEW_INSTANCE) {
			//TODO: copy over args from current filter
			GF_Filter *new_filter = gf_filter_clone(filter);
			if (new_filter) {
				gf_fs_post_task(filter->session, gf_filter_pid_connect_task, new_filter, pid, "pid_connect", NULL);
				e = GF_OK;
				return;
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to clone filter %s\n", filter->name));
				e = GF_OUT_OF_MEM;
			}
		}
		if (e && is_remove) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to disconnect filter %s PID %s from filter %s: %s\n", pid->filter->name, pid->name, filter->name, gf_error_to_string(e) ));
		}
		else if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to connect filter %s PID %s to filter %s: %s\n", pid->filter->name, pid->name, filter->name, gf_error_to_string(e) ));

			if (filter->freg->output_caps) {
				//todo - try to load another filter to handle that connection
				gf_list_add(pid->filter->blacklisted, (void *) filter->freg);
				//gf_filter_pid_resolve_link(pid, filter);
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to reconfigure input of sink %s, cannot rebuild graph\n", filter->name));
			}
		}
		//try to run filter no matter what
		if (filter->session->requires_solved_graph )
			return;
	}

	//flush all pending pid init requests following the call to init
	if (filter->has_pending_pids) {
		filter->has_pending_pids = GF_FALSE;
		while (gf_fq_count(filter->pending_pids)) {
			GF_FilterPid *pid=gf_fq_pop(filter->pending_pids);
			gf_fs_post_task(filter->session, gf_filter_pid_init_task, filter, pid, "pid_init", NULL);
		}
	}

	if (is_remove) {
		gf_list_del_item(filter->input_pids, pidinst);
		gf_list_del_item(pidinst->pid->destinations, pidinst);
		pidinst->filter = NULL;
		filter->num_input_pids = gf_list_count(filter->input_pids);
		//post a pid_delete task to also trigger removal of the filter if needed
		gf_fs_post_task(filter->session, gf_filter_pid_inst_delete_task, pid->filter, pid, "pid_inst_delete", pidinst);
		return;
	}

	if (is_connect) {
		assert(pid->filter->pid_connection_pending);
		if ( (safe_int_dec(&pid->filter->pid_connection_pending)==0) ) {

			//pending packets dispatched by source while setting up PID, flush through process()
			if (pid->pid->filter->postponed_packets) {
//				gf_filter_post_process_task(pid->pid->filter);
			}

			//source filters, start flushing data
			//commented for now, we may want an auto-start
			if (0 && !pid->filter->input_pids) {
				gf_filter_post_process_task(pid->filter);
			}
			//other filters with packets ready in inputs, start processing
			else if (pid->filter->pending_packets) {
				gf_filter_post_process_task(pid->filter);
			}
		}
	}
	//once all pid have been (re)connected, update any internal caps
	gf_filter_pid_update_caps(pid, filter);
}

void gf_filter_pid_connect_task(GF_FSTask *task)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s pid %s connect to %s task\n", task->pid->pid->filter->name, task->filter->name, task->pid->pid->name));
	gf_filter_pid_configure(task->filter, task->pid->pid, GF_TRUE, GF_FALSE);
}

void gf_filter_pid_reconfigure_task(GF_FSTask *task)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s pid %s reconfigure to %s task\n", task->pid->pid->filter->name, task->filter->name, task->pid->pid->name));
	gf_filter_pid_configure(task->filter, task->pid->pid, GF_FALSE, GF_FALSE);
}

void gf_filter_pid_disconnect_task(GF_FSTask *task)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s pid %s disconnect from %s task\n", task->pid->pid->filter->name, task->filter->name, task->pid->pid->name));
	gf_filter_pid_configure(task->filter, task->pid->pid, GF_FALSE, GF_TRUE);
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
					count_pid = src_pid->filter->num_output_pids;
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
	count = parent->num_input_pids;
	if (!count) return GF_FALSE;
	for (i=0; i<count; i++) {
		GF_FilterPidInst *pid = gf_list_get(parent->input_pids, i);
		if (filter_in_parent_chain(pid->pid->filter, filter)) return GF_TRUE;
	}
	return GF_FALSE;
}

static Bool filter_pid_caps_match(GF_FilterPid *src_pid, const GF_FilterRegister *freg)
{
	u32 i=0;
	u32 nb_matched=0;
	u32 nb_subcaps=0;
	Bool all_caps_matched = GF_TRUE;

	//filters with no explicit input cap accept anything for now, this should be refined ...
	if (!freg->input_caps)
		return GF_TRUE;

	//check all input caps of dst filter
	while (freg->input_caps) {
		const GF_PropertyValue *pid_cap=NULL;
		const GF_FilterCapability *cap = &freg->input_caps[i];
		if (!cap || (!cap->code && !cap->name) ) {
			if (nb_subcaps && all_caps_matched)
				return GF_TRUE;
			break;
		}

		if (i && cap->start) {
			if (all_caps_matched) return GF_TRUE;
			all_caps_matched = GF_TRUE;
			nb_subcaps=0;
		}
		i++;
		nb_subcaps++;
		//no match for this cap, go on until new one or end
		if (!all_caps_matched) continue;

		if (cap->code) {
			pid_cap = gf_filter_pid_get_property(src_pid, cap->code);
		}
		if (!pid_cap && cap->name) pid_cap = gf_filter_pid_get_property_str(src_pid, cap->name);

		//we found a property of that type and it is equal
		if (pid_cap) {
			Bool prop_equal = gf_props_equal(pid_cap, &cap->val);
			if (cap->exclude) prop_equal = !prop_equal;

			if (!prop_equal) {
				all_caps_matched=GF_FALSE;
			}
		} else {
			all_caps_matched=GF_FALSE;
		}

	}
	return GF_FALSE;
}

static u32 filter_caps_to_caps_match(const GF_FilterRegister *src, const GF_FilterRegister *dst)
{
	u32 i=0;
	u32 nb_matched=0;
	u32 nb_subcaps=0;
	Bool all_caps_matched=GF_TRUE;

	//check all output caps of src filter
	i=0;
	while (src->output_caps) {
		u32 j=0;
		Bool matched=GF_FALSE;
		GF_PropertyValue capv;
		const GF_FilterCapability *out_cap = &src->output_caps[i];
		i++;
		if (!out_cap || (!out_cap->code && !out_cap->name) ) {
			if (nb_subcaps && all_caps_matched) nb_matched++;
			break;
		}

		if (out_cap->start) {
			if (all_caps_matched) nb_matched++;
			all_caps_matched = GF_TRUE;
			nb_subcaps=0;
		}
		nb_subcaps++;
		//no match possible for this cap, wait until next cap start
		if (!all_caps_matched) continue;

		//check all input caps of dst filter, count ones that are matched
		while (dst->input_caps) {
			Bool prop_equal;
			const GF_FilterCapability *in_cap = &dst->input_caps[j];
			j++;
			if (!in_cap || (!in_cap->code && !in_cap->name) ) break;
			if (out_cap->code && (out_cap->code!=in_cap->code) )
				continue;
			if (out_cap->name && (!in_cap->name || strcmp(out_cap->name, in_cap->name)))
				continue;

			//we found a property of that type and it is equal
			prop_equal = gf_props_equal(&in_cap->val, &out_cap->val);
			if (in_cap->exclude && !out_cap->exclude) prop_equal = !prop_equal;
			else if (!in_cap->exclude && out_cap->exclude) prop_equal = !prop_equal;

			if (prop_equal) {
				matched = GF_TRUE;
			}
		}
		if (!matched) {
			all_caps_matched = GF_FALSE;
		}
	}
	return nb_matched;
}

Bool gf_filter_pid_check_caps(GF_FilterPid *pid)
{
	const GF_FilterRegister *freg = pid->filter->freg;
	if (PID_IS_OUTPUT(pid)) return GF_FALSE;
	return filter_pid_caps_match(pid->pid, freg);
}


u32 gf_filter_check_dst_caps(GF_FilterSession *fsess, const GF_FilterRegister *filter_reg, GF_List *black_list, GF_List *filter_chain, const GF_FilterRegister *dst_filter)
{
	u32 nb_matched = 0;
	const GF_FilterRegister *candidate = NULL;
	//browse all our registered filters
	u32 i, count=gf_list_count(fsess->registry);
	u32 count_at_input = gf_list_count(filter_chain);

	for (i=0; i<count; i++) {
		u32 path_weight=0;
		const GF_FilterRegister *freg = gf_list_get(fsess->registry, i);
		if (freg==filter_reg) continue;

		//source filter, can't add pid
		if (!freg->configure_pid) continue;
		
		//blacklisted filter, can't add pid
		if (gf_list_find(black_list, (void *) freg)>=0)
			continue;

		path_weight = filter_caps_to_caps_match(filter_reg, freg);
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
	if (candidate) gf_list_add(filter_chain, (void *) candidate);
	return nb_matched;
}

GF_Filter *gf_filter_pid_resolve_link(GF_FilterPid *pid, GF_Filter *dst)
{
	GF_Filter *chain_input = NULL;
	GF_FilterSession *fsess = pid->filter->session;
	GF_List *filter_chain = gf_list_new();
	u32 max_weight=0;
	const GF_FilterRegister *dst_filter = dst->freg;

	//browse all our registered filters
	u32 i, count=gf_list_count(fsess->registry);
	for (i=0; i<count; i++) {
		u32 freg_weight=0;
		u32 path_weight=0;
		u32 path_len=0;
		const GF_FilterRegister *freg = gf_list_get(fsess->registry, i);

		//source filter, can't add pid
		if (!freg->configure_pid) continue;

		//blacklisted filter, can't add pid
		if (gf_list_find(pid->filter->blacklisted, (void *) freg)>=0)
			continue;

		//no match of pid caps for this filter
		freg_weight = filter_pid_caps_match(pid, freg) ? 1 : 0;
		if (!freg_weight) continue;

		//we have a target destination filter match, keep solving filter until done
		path_len = gf_list_count(filter_chain);

		gf_list_add(filter_chain, (void *) freg);
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
	count = gf_list_count(filter_chain);
	if (count==0) {
		//no filter found for this pid !
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("No suitable filter found for PID in filter %s - NOT CONNECTED\n", pid->filter->name));
	} else {
		//no filter found for this pid !
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Solved filter chain from filter %s PID %s to filter %s - dumping chain:\n", pid->filter->name, pid->name, dst_filter->name));

		for (i=0; i<count; i++) {
			GF_Filter *af;
			const GF_FilterRegister *freg = gf_list_get(filter_chain, i);
			GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("\t%s\n", freg->name));

			//todo - we could forward the src arguments to the new filters
			af = gf_filter_new(fsess, freg, NULL);
			if (!af) goto exit;
			//remember the first load one
			if (!i) chain_input = af;
			//the other filters shouldn't need any specific init
			af->dynamic_filter = GF_TRUE;
		}
	}

exit:
	gf_list_del(filter_chain);
	return chain_input;
}

void gf_filter_pid_init_task(GF_FSTask *task)
{
	u32 i, count;
	Bool found_dest=GF_FALSE;
	Bool first_pass=GF_TRUE;

restart:
	//try to connect pid to all running filters
	count = gf_list_count(task->filter->session->filters);
	for (i=0; i<count; i++) {
		GF_Filter *filter_dst = gf_list_get(task->filter->session->filters, i);
		//source filter
		if (!filter_dst->freg->configure_pid) continue;
		if (filter_dst->finalized || filter_dst->removed) continue;

		//second pass, we try to load a filter chain, so don't test against filters
		//loaded for another chain
		if (!first_pass && filter_dst->dynamic_filter) continue;

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
		if (!filter_pid_caps_match(task->pid, filter_dst->freg)) {
			if (first_pass) continue;

			GF_Filter *new_f = gf_filter_pid_resolve_link(task->pid, filter_dst);
			//try to load filters
			if (! new_f) {
				continue;
			}
			filter_dst = new_f;
		}

		safe_int_inc(&task->pid->filter->pid_connection_pending);
		gf_fs_post_task(filter_dst->session, gf_filter_pid_connect_task, filter_dst, task->pid, "pid_connect", NULL);

		found_dest = GF_TRUE;
	}

	//connection in proces, do nothing
	if (found_dest) return;

	//nothing found, redo a pass, this time allowing for link resolve
	if (first_pass) {
		first_pass = GF_FALSE;
		goto restart;
	}

	//no filter found for this pid !
	GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("No filter found for PID %s in filter %s - NOT CONNECTED\n", task->pid->name, task->pid->filter->name));
	return;
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

	sprintf(szName, "PID%d", filter->num_output_pids);
	pid->name = gf_strdup(szName);

	filter->has_pending_pids = GF_TRUE;
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
	pid->request_property_map = GF_FALSE;
	map = gf_props_new(pid->filter);
	if (!map) return NULL;
	gf_list_add(pid->properties, map);

	//when creating a new map, ref_count of old map is decremented
	if (old_map) {
		if (merge_props)
			gf_props_merge_property(map, old_map);
		assert(old_map->reference_count);
		if ( safe_int_dec(&old_map->reference_count) == 0) {
			gf_list_del_item(pid->properties, old_map);
			gf_props_del(old_map);
		}
	}
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

u32 gf_filter_pid_get_packet_count(GF_FilterPid *pid)
{
	GF_FilterPacket *pck;
	GF_FilterPidInst *pidinst = (GF_FilterPidInst *)pid;
	if (PID_IS_OUTPUT(pid)) {
		pidinst = gf_list_get(pid->destinations, 0);
		if (! pidinst) return 0;
		return gf_fq_count(pidinst->packets);

	} else {
		if (pidinst->discard_input_packets) return 0;
		return gf_fq_count(pidinst->packets);
	}
}

GF_FilterPacket *gf_filter_pid_get_packet(GF_FilterPid *pid)
{
	GF_FilterPacketInstance *pcki;
	GF_FilterPidInst *pidinst = (GF_FilterPidInst *)pid;
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to fetch a packet on an output PID in filter %s\n", pid->filter->name));
		return NULL;
	}
	if (pidinst->discard_input_packets) return NULL;

	pcki = (GF_FilterPacketInstance *)gf_fq_head(pidinst->packets);

	if (!pcki) {
		//it may happen that no task is currently scheduled for the source pid (bug to investigate)
		//we force a task if not EOS to force flushing the parent
		if (!pidinst->pid->has_seen_eos && !pidinst->pid->filter->would_block) {
//			gf_filter_post_process_task(pidinst->pid->filter);
		}
		return NULL;
	}
	
	if (pidinst->requires_full_data_block && !pcki->pck->data_block_end)
		return NULL;

	assert(pcki->pck);
	if (pcki->pck->pid_props_changed && !pcki->pid_props_change_done) {
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s PID %s property changed at this packet, triggering reconfigure\n", pidinst->pid->filter->name, pidinst->pid->name));
		pcki->pid_props_change_done = GF_TRUE;
		assert(pidinst->filter->freg->configure_pid);
		gf_filter_pid_configure(pidinst->filter, pidinst->pid, GF_FALSE, GF_FALSE);
	}
	pidinst->last_pck_fetch_time = gf_sys_clock_high_res();

	return (GF_FilterPacket *)pcki;
}


static void gf_filter_pidinst_update_stats(GF_FilterPidInst *pidi, GF_FilterPacket *pck)
{
	u64 now = gf_sys_clock_high_res();
	u64 dec_time = now - pidi->last_pck_fetch_time;
	if (pck->eos) return;

	pidi->filter->nb_pck_processed++;
	pidi->filter->nb_bytes_processed += pck->data_length;

	pidi->total_process_time += dec_time;
	if (!pidi->nb_processed) {
		pidi->first_frame_time = pidi->last_pck_fetch_time;
	}

	pidi->nb_processed++;
	if (pck->sap_type) {
		pidi->nb_sap_processed ++;
		if (dec_time > pidi->max_sap_process_time) pidi->max_sap_process_time = dec_time;
		pidi->total_sap_process_time += dec_time;
	}

	if (dec_time > pidi->max_process_time) pidi->max_process_time = dec_time;

	if (pck->data_length) {
		u64 ts = pck->dts ? pck->dts : pck->cts;
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
	Bool unblock=GF_FALSE;
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

	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s (%s) drop packet DTS "LLU" CTS "LLU" SAP %d Seek %d - %d packets remaining\n", pidinst->filter->name, pid->name, pid->filter->name, pck->dts, pck->cts, pck->sap_type, pck->seek_flag, gf_fq_count(pidinst->packets) ));

	gf_filter_pidinst_update_stats(pidinst, pck);

	nb_pck=gf_fq_count(pidinst->packets);
	if (nb_pck<pid->nb_buffer_unit) {
		//todo needs Compare&Swap
		pid->nb_buffer_unit = nb_pck;
		if (pid->nb_buffer_unit < pid->max_buffer_unit) {
			unblock=GF_TRUE;
		}
	}

	if (pck->duration && pck->pid_props->timescale) {
		s64 d = ((u64)pck->duration) * 1000000;
		d /= pck->pid_props->timescale;
		assert(d <= pidinst->buffer_duration);
		safe_int_sub(&pidinst->buffer_duration, d);

		if (pidinst->buffer_duration < pid->buffer_duration) {
			//todo needs Compare&Swap
			pid->buffer_duration = pidinst->buffer_duration;
			if (pid->buffer_duration < pid->max_buffer_time) {
				unblock=GF_TRUE;
			}
		}
	}

	if (pid->would_block && unblock) {
		//todo needs Compare&Swap
		safe_int_dec(&pid->would_block);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s unblocked\n", pid->pid->filter->name, pid->pid->name));
		assert(pid->filter->would_block);
		safe_int_dec(&pid->filter->would_block);
		if (!pid->filter->would_block) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s unblocked, requesting process task\n", pid->filter->name));
			//requeue task
			if (!pid->filter->skip_process_trigger_on_tasks)
				gf_filter_post_process_task(pid->filter);
		}
	}

	//destroy pcki
	pcki->pck = NULL;
	pcki->pid = NULL;

	gf_fq_add(pid->filter->pcks_inst_reservoir, pcki);

	//unref pck
	if (safe_int_dec(&pck->reference_count) == 0) {
		gf_filter_packet_destroy(pck);
	}
	//decrement number of pending packet on target filter
	safe_int_dec(&pidinst->filter->pending_packets);
}


void gf_filter_pid_set_eos(GF_FilterPid *pid)
{
	GF_FilterPacket *pck;
	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to signal EOS on input PID %s in filter %s\n", pid->pid->name, pid->filter->name));
		return;
	}
	pck = gf_filter_pck_new_shared(pid, NULL, 0, NULL);
	gf_filter_pck_set_eos(pck, GF_TRUE );
	gf_filter_pck_send(pck);
	pid->pid->has_seen_eos = GF_TRUE;
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

	if (pid->max_buffer_unit && (pid->nb_buffer_unit >= pid->max_buffer_unit)) {
		would_block = GF_TRUE;
	}

	if (pid->max_buffer_time && pid->buffer_duration > pid->max_buffer_time) {
		would_block = GF_TRUE;
	}
	if (would_block && !pid->would_block) {
		safe_int_inc(&pid->would_block);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s blocked\n", pid->pid->filter->name, pid->pid->name));
		safe_int_inc(&pid->filter->would_block);
	}
	return would_block;
}

u64 gf_filter_pid_query_buffer_duration(GF_FilterPid *pid)
{
	u32 count, i, j;
	u64 duration=0;
	if (PID_IS_INPUT(pid)) {
		GF_Filter *filter;
		GF_FilterPidInst *pidinst = (GF_FilterPidInst *)pid;
		filter = pidinst->pid->filter;
		count = filter->num_input_pids;
		for (i=0; i<count; i++) {
			u64 dur = gf_filter_pid_query_buffer_duration( gf_list_get(filter->input_pids, i) );
			if (dur > duration)
				duration = dur;
		}
		duration += pidinst->buffer_duration;
		return duration;
	} else {
		u32 count2;
		u64 max_dur=0;
		count = gf_list_count(pid->destinations);
		for (i=0; i<count; i++) {
			GF_FilterPidInst *pidinst = gf_list_get(pid->destinations, i);
			if (pidinst->buffer_duration > duration) duration = pidinst->buffer_duration;

			count2 = pidinst->filter->num_output_pids;
			for (j=0; j<count2; j++) {
				GF_FilterPid *pid_n = gf_list_get(pidinst->filter->output_pids, i);
				u64 dur = gf_filter_pid_query_buffer_duration(pid_n);
				if (dur > max_dur ) max_dur = dur;
			}
		}
		duration += max_dur;
	}
	return duration;
}


Bool gf_filter_pid_has_seen_eos(GF_FilterPid *pid)
{
	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to query EOS on output PID %s in filter %s\n", pid->pid->name, pid->filter->name));
		return GF_FALSE;
	}
	return pid->pid->has_seen_eos;
}

static const char *get_fevt_name(u32 type)
{
	switch (type) {
	case GF_FEVT_PLAY: return "PLAY";
	case GF_FEVT_SET_SPEED: return "SET_SPEED";
	case GF_FEVT_STOP: return "STOP";
	case GF_FEVT_ATTACH_SCENE: return "ATTACH_SCENE";
	case GF_FEVT_PAUSE: return "PAUSE";
	case GF_FEVT_RESUME: return "RESUME";
	default: return "UNKNOWN";
	}
}

static void gf_filter_pid_reset(GF_FSTask *task)
{
	GF_FilterPidInst *pidi = (GF_FilterPidInst *)task->udta;
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s input PID %s (from %s) reseting buffer\n", task->filter->name, pidi->pid->name, pidi->pid->filter->name ));

	while (gf_fq_count(pidi->packets)) {
		gf_filter_pid_drop_packet((GF_FilterPid *) pidi);
	}
	while (gf_list_count(pidi->pck_reassembly)) {
		GF_FilterPacket *pck = gf_list_pop_back(pidi->pck_reassembly);
		gf_filter_pck_unref(pck);
	}
	gf_filter_pidinst_reset_stats(pidi);

	pidi->discard_input_packets = GF_FALSE;
	pidi->last_block_ended = GF_TRUE;
	pidi->first_block_started = GF_FALSE;
	safe_int_dec(& pidi->pid->filter->stream_reset_pending );
}

void gf_filter_pid_send_event_downstream(GF_FSTask *task)
{
	u32 i, count;
	Bool canceled = GF_FALSE;
	GF_FilterEvent *evt = task->udta;
	GF_Filter *f = task->filter;

	if (f->stream_reset_pending) {
		task->requeue_request = GF_TRUE;
		return;
	}

	if (f->freg->process_event) {
		canceled = f->freg->process_event(f, evt);

		//source filters and play command, request a process task
		if (!f->input_pids) {
			if (evt->base.type==GF_FEVT_PLAY) {
				gf_filter_post_process_task(f);
			}
		}
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s PID %s processed event %s - canceled %s\n", f->name, evt->base.on_pid ? evt->base.on_pid->name : "none", get_fevt_name(evt->base.type), canceled ? "yes" : "no" ));

	if (evt->base.type == GF_FEVT_STOP) {
		u32 i;
		GF_FilterPidInst *p = (GF_FilterPidInst *) evt->base.on_pid;
		GF_FilterPid *pid = p->pid;
		for (i=0; i<gf_list_count(pid->destinations); i++) {
			GF_FilterPidInst *pidi = gf_list_get(pid->destinations, i);
			pidi->discard_input_packets = GF_TRUE;
			safe_int_inc(& pid->filter->stream_reset_pending );
			gf_fs_post_task(pidi->filter->session, gf_filter_pid_reset, pidi->filter, NULL, "reset_pid", pidi);
		}
	}

	//no more input pids
	count = f->num_input_pids;
	if (count==0) canceled = GF_TRUE;

	if (canceled) {
		gf_free(evt);
		return;
	}
	//otherwise forward event to each input PID
	for (i=0; i<count; i++) {
		GF_FilterEvent *an_evt;
		GF_FilterPidInst *pid_inst = gf_list_get(f->input_pids, i);
		GF_FilterPid *pid = pid_inst->pid;

		//mark pid instance as about to be reset to avoid processing PID destroy task before
		if (evt->base.type == GF_FEVT_STOP) {
			pid_inst->discard_input_packets = GF_TRUE;
		}
		//allocate a copy except for the last PID where we use the one from the input
		if (i+1<count) {
			an_evt = gf_malloc(sizeof(GF_FilterEvent));
			memcpy(an_evt, evt, sizeof(GF_FilterEvent));
		} else {
			an_evt = evt;
		}
		an_evt->base.on_pid = pid;
		gf_fs_post_task(pid->filter->session, gf_filter_pid_send_event_downstream, pid->filter, pid, "downstream_event", an_evt);
	}
	return;
}

void gf_filter_pid_send_event(GF_FilterPid *pid, GF_FilterEvent *evt)
{
	GF_FilterEvent *dup_evt;

	//filter is being shut down, prevent any event posting
	if (pid->filter->finalized) return;

	if (PID_IS_OUTPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Sending filter events upstream not yet implemented (PID %s in filter %s)\n", pid->pid->name, pid->filter->name));
		return;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s PID %s queuing event %s\n", pid->pid->filter->name, pid->pid->name, get_fevt_name(evt->base.type) ));

	dup_evt = gf_malloc(sizeof(GF_FilterEvent));
	memcpy(dup_evt, evt, sizeof(GF_FilterEvent));
	dup_evt->base.on_pid = pid->pid;

	gf_fs_post_task(pid->pid->filter->session, gf_filter_pid_send_event_downstream, pid->pid->filter, pid->pid, "downstream_event", dup_evt);
}

void gf_filter_pid_exec_event(GF_FilterPid *pid, GF_FilterEvent *evt)
{
	GF_FilterEvent *dup_evt;

	//filter is being shut down, prevent any event posting
	if (pid->pid->filter->finalized) return;
	assert (pid->pid->filter->freg->requires_main_thread);

	if (pid->pid->filter->freg->process_event)
		pid->pid->filter->freg->process_event(pid->pid->filter, evt);
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
	u32 j, count;
	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Removing PID input filter (%s:%s) not allowed\n", pid->filter->name, pid->pid->name));
	}
	count = gf_list_count(pid->destinations);
	for (j=0; j<count; j++) {
		GF_FilterPidInst *pidi = gf_list_get(pid->destinations, j);
		gf_fs_post_task(pid->filter->session, gf_filter_pid_disconnect_task, pidi->filter, pid, "pidinst_disconnect", NULL);
	}
}



