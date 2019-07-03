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

static void gf_filter_pck_reset_props(GF_FilterPacket *pck, GF_FilterPid *pid)
{
	memset(&pck->info, 0, sizeof(GF_FilterPckInfo));
	pck->info.dts = pck->info.cts = GF_FILTER_NO_TS;
	pck->info.byte_offset =  GF_FILTER_NO_BO;
	pck->info.flags = GF_PCKF_BLOCK_START | GF_PCKF_BLOCK_END;
	pck->pid = pid;
	pck->src_filter = pid->filter;
	pck->session = pid->filter->session;
}

GF_EXPORT
GF_Err gf_filter_pck_merge_properties_filter(GF_FilterPacket *pck_src, GF_FilterPacket *pck_dst, gf_filter_prop_filter filter_prop, void *cbk)
{
	if (PCK_IS_INPUT(pck_dst)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to set property on an input packet in filter %s\n", pck_dst->pid->filter->name));
		return GF_BAD_PARAM;
	}
	//we allow copying over properties from dest packets to dest packets
	//get true packet pointer
	pck_src=pck_src->pck;
	pck_dst=pck_dst->pck;

	pck_dst->info = pck_src->info;
	pck_dst->info.flags &= ~GF_PCKF_PROPS_REFERENCE;

	if (!pck_src->props) {
		return GF_OK;
	}
	if (!pck_dst->props) {
		pck_dst->props = gf_props_new(pck_dst->pid->filter);

		if (!pck_dst->props) return GF_OUT_OF_MEM;
	}
	return gf_props_merge_property(pck_dst->props, pck_src->props, filter_prop, cbk);
}

GF_EXPORT
GF_Err gf_filter_pck_merge_properties(GF_FilterPacket *pck_src, GF_FilterPacket *pck_dst)
{
	return gf_filter_pck_merge_properties_filter(pck_src, pck_dst, NULL, NULL);
}

static GF_FilterPacket *gf_filter_pck_new_alloc_internal(GF_FilterPid *pid, u32 data_size, u8 **data, Bool no_block_check)
{
	GF_FilterPacket *pck=NULL;
	GF_FilterPacket *closest=NULL;
	u32 i, count, max_reservoir_size;

	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to allocate a packet on an input PID in filter %s\n", pid->filter->name));
		return NULL;
	}
	if (!no_block_check && gf_filter_pid_would_block(pid))
		return NULL;

	count = gf_fq_count(pid->filter->pcks_alloc_reservoir);
	for (i=0; i<count; i++) {
		GF_FilterPacket *cur = gf_fq_get(pid->filter->pcks_alloc_reservoir, i);
		if (cur->alloc_size >= data_size) {
			if (!pck || (pck->alloc_size>cur->alloc_size)) {
				pck = cur;
			}
		}
		else if (!closest) closest = cur;
		//small data blocks, find smaller one
		else if (data_size<1000) {
			if (closest->alloc_size > cur->alloc_size) closest = cur;
		}
		//otherwise find largest one below our target size
		else if (closest->alloc_size < cur->alloc_size) closest = cur;
	}
	//stop allocating after a while - TODO we for sur can design a better algo...
	max_reservoir_size = pid->num_destinations ? 10 : 1;
	if (!pck && (count>=max_reservoir_size)) {
		assert(closest);
		closest->alloc_size = data_size;
		closest->data = gf_realloc(closest->data, closest->alloc_size);
		pck = closest;
#ifdef GPAC_MEMORY_TRACKING
		pid->filter->session->nb_realloc_pck++;
#endif
	}

	if (!pck) {
		GF_SAFEALLOC(pck, GF_FilterPacket);
		pck->data = gf_malloc(sizeof(char)*data_size);
		pck->alloc_size = data_size;
#ifdef GPAC_MEMORY_TRACKING
		pid->filter->session->nb_alloc_pck+=2;
#endif
	} else {
		//pop first item and swap pointers. We can safely do this since this filter
		//is the only one accessing the queue in pop mode, all others are just pushing to it
		//this may however imply that we don't get the best matching block size if new packets
		//were added to the list

		GF_FilterPacket *head_pck = gf_fq_pop(pid->filter->pcks_alloc_reservoir);
		char *data = pck->data;
		u32 alloc_size = pck->alloc_size;
		pck->data = head_pck->data;
		pck->alloc_size = head_pck->alloc_size;
		head_pck->data = data;
		head_pck->alloc_size = alloc_size;
		pck = head_pck;
	}

	pck->pck = pck;
	pck->data_length = data_size;
	if (data) *data = pck->data;
	pck->filter_owns_mem = 0;

	gf_filter_pck_reset_props(pck, pid);
	return pck;
}

GF_EXPORT
GF_FilterPacket *gf_filter_pck_new_alloc(GF_FilterPid *pid, u32 data_size, u8 **data)
{
	return gf_filter_pck_new_alloc_internal(pid, data_size, data, GF_TRUE);
}

GF_EXPORT
GF_FilterPacket *gf_filter_pck_new_clone(GF_FilterPid *pid, GF_FilterPacket *pck_source, u8 **data)
{
	GF_FilterPacket *dst, *ref;
	u32 max_ref = 0;
	GF_FilterPacketInstance *pcki;
	if (!pck_source) return NULL;

	pcki = (GF_FilterPacketInstance *) pck_source;
	if (pcki->pck->frame_ifce || !pcki->pck->data_length)
		return NULL;

	ref = pcki->pck;
	while (ref) {
		if (ref->filter_owns_mem==2) {
			max_ref = 2;
			break;
		}
		if (ref->reference_count>max_ref)
			max_ref = ref->reference_count;
		ref = ref->reference;
	}

	if (max_ref>1) {
		dst = gf_filter_pck_new_alloc_internal(pid, pcki->pck->data_length, data, GF_TRUE);
		if (dst && data) {
			memcpy(*data, pcki->pck->data, sizeof(char)*pcki->pck->data_length);
		}
		if (dst) gf_filter_pck_merge_properties(pck_source, dst);
		return dst;
	}
	dst = gf_filter_pck_new_ref(pid, NULL, 0, pck_source);
	if (dst) {
		gf_filter_pck_merge_properties(pck_source, dst);
		*data = dst->data;
	}
	return dst;
}

GF_EXPORT
GF_FilterPacket *gf_filter_pck_new_alloc_destructor(GF_FilterPid *pid, u32 data_size, u8 **data, gf_fsess_packet_destructor destruct)
{
	GF_FilterPacket *pck = gf_filter_pck_new_alloc_internal(pid, data_size, data, GF_TRUE);
	if (pck) pck->destructor = destruct;
	return pck;
}

GF_EXPORT
GF_FilterPacket *gf_filter_pck_new_shared_internal(GF_FilterPid *pid, const u8 *data, u32 data_size, gf_fsess_packet_destructor destruct, Bool intern_pck)
{
	GF_FilterPacket *pck;

	if (PID_IS_INPUT(pid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to allocate a packet on an input PID in filter %s\n", pid->filter->name));
		return NULL;
	}

	pck = gf_fq_pop(pid->filter->pcks_shared_reservoir);
	if (!pck) {
		GF_SAFEALLOC(pck, GF_FilterPacket);
		if (!pck)
			return NULL;
	}
	pck->pck = pck;
	pck->data = (char *) data;
	pck->data_length = data_size;
	pck->destructor = destruct;
	pck->filter_owns_mem = 1;
	if (!intern_pck) {
		safe_int_inc(&pid->nb_shared_packets_out);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s has %d shared packets out\n", pid->filter->name, pid->name, pid->nb_shared_packets_out));
	}
	gf_filter_pck_reset_props(pck, pid);

	assert(pck->pid);
	return pck;
}

GF_EXPORT
GF_FilterPacket *gf_filter_pck_new_shared(GF_FilterPid *pid, const u8 *data, u32 data_size, gf_fsess_packet_destructor destruct)
{
	return gf_filter_pck_new_shared_internal(pid, data, data_size, destruct, GF_FALSE);

}

GF_EXPORT
GF_FilterPacket *gf_filter_pck_new_ref(GF_FilterPid *pid, const u8 *data, u32 data_size, GF_FilterPacket *reference)
{
	GF_FilterPacket *pck;
	if (!reference) return NULL;
	reference=reference->pck;
	pck = gf_filter_pck_new_shared(pid, data, data_size, NULL);
	if (!pck) return NULL;
	pck->reference = reference;
	assert(reference->reference_count);
	safe_int_inc(&reference->reference_count);
	if (!data && !data_size) {
		pck->data = reference->data;
		pck->data_length = reference->data_length;
		pck->frame_ifce = reference->frame_ifce;
	}
	safe_int_inc(&reference->pid->nb_shared_packets_out);
	return pck;
}

GF_EXPORT
GF_Err gf_filter_pck_set_readonly(GF_FilterPacket *pck)
{
	if (PCK_IS_INPUT(pck)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to set readonly on an input packet in filter %s\n", pck->pid->filter->name));
		return GF_BAD_PARAM;
	}
	pck->filter_owns_mem = 2;
	return GF_OK;
}

GF_EXPORT
GF_FilterPacket *gf_filter_pck_new_frame_interface(GF_FilterPid *pid, GF_FilterFrameInterface *frame_ifce, gf_fsess_packet_destructor destruct)
{
	GF_FilterPacket *pck;
	if (!frame_ifce) return NULL;
	pck = gf_filter_pck_new_shared(pid, NULL, 0, NULL);
	if (!pck) return NULL;
	pck->destructor = destruct;
	pck->frame_ifce = frame_ifce;
	pck->filter_owns_mem = 2;
	return pck;
}

GF_EXPORT
GF_Err gf_filter_pck_forward(GF_FilterPacket *reference, GF_FilterPid *pid)
{
	GF_FilterPacket *pck;
	if (!reference) return GF_OUT_OF_MEM;
	reference=reference->pck;
	pck = gf_filter_pck_new_shared(pid, NULL, 0, NULL);
	if (!pck) return GF_OUT_OF_MEM;
	pck->reference = reference;
	assert(reference->reference_count);
	safe_int_inc(&reference->reference_count);
	safe_int_inc(&reference->pid->nb_shared_packets_out);

	gf_filter_pck_merge_properties(reference, pck);
	pck->data = reference->data;
	pck->data_length = reference->data_length;
	pck->frame_ifce = reference->frame_ifce;

	return gf_filter_pck_send(pck);
}

/*internal*/
void gf_filter_packet_destroy(GF_FilterPacket *pck)
{
	Bool is_filter_destroyed = GF_FALSE;
	GF_FilterPid *pid = pck->pid;
	Bool is_ref_props_packet = GF_FALSE;

	//this is a ref props packet, its destruction can happen at any time, included after destruction
	//of source filter/pid.  pck->src_filter or pck->pid shall not be trusted !
	if (pck->info.flags & GF_PCKF_PROPS_REFERENCE) {
		is_ref_props_packet = GF_TRUE;
		is_filter_destroyed = GF_TRUE;
		pck->src_filter = NULL;
		pck->pid = NULL;
		assert(!pck->destructor);
		assert(!pck->filter_owns_mem);
		assert(!pck->reference);

		if (pck->info.cts != GF_FILTER_NO_TS) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Destroying packet property reference CTS "LLU" size %d\n", pck->info.cts, pck->data_length));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Destroying packet property reference size %d\n", pck->data_length));
		}
	}
	//if not null, we discard a non-sent packet
	else if (pck->src_filter) {
		is_filter_destroyed = pck->src_filter->finalized;
	}

	if (!is_filter_destroyed) {
		assert(pck->pid);
		if (pck->pid->filter) {
			if (pck->info.cts != GF_FILTER_NO_TS) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s destroying packet CTS "LLU"\n", pck->pid->filter->name, pck->pid->name, pck->info.cts));
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s destroying packet\n", pck->pid->filter->name, pck->pid->name));
			}
		}
	}
	if (pck->destructor) pck->destructor(pid->filter, pid, pck);

	if (pck->pid_props) {
		GF_PropertyMap *props = pck->pid_props;
		pck->pid_props = NULL;

		if (is_ref_props_packet) {
			assert(props->pckrefs_reference_count);
			if (safe_int_dec(&props->pckrefs_reference_count) == 0) {
				gf_props_del(props);
			}
		} else {
			assert(props->reference_count);
			if (safe_int_dec(&props->reference_count) == 0) {
				if (!is_filter_destroyed) {
					//see \ref gf_filter_pid_merge_properties_internal for mutex
					gf_mx_p(pck->pid->filter->tasks_mx);
					gf_list_del_item(pck->pid->properties, props);
					gf_mx_v(pck->pid->filter->tasks_mx);
				}
				gf_props_del(props);
			}
		}
	}

	if (pck->props) {
		GF_PropertyMap *props = pck->props;
		pck->props=NULL;
		assert(props->reference_count);
		if (safe_int_dec(&props->reference_count) == 0) {
			gf_props_del(props);
		}
	}
	if (pck->filter_owns_mem && !(pck->info.flags & GF_PCK_CMD_MASK) ) {
		assert(pck->pid);
		assert(pck->pid->nb_shared_packets_out);
		safe_int_dec(&pck->pid->nb_shared_packets_out);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s has %d shared packets out\n", pck->pid->filter->name, pck->pid->name, pck->pid->nb_shared_packets_out));
	}

	pck->data_length = 0;
	pck->pid = NULL;

	if (pck->reference) {
		assert(pck->reference->pid->nb_shared_packets_out);
		safe_int_dec(&pck->reference->pid->nb_shared_packets_out);
		
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s has %d shared packets out\n", pck->reference->pid->filter->name, pck->reference->pid->name, pck->reference->pid->nb_shared_packets_out));
		assert(pck->reference->reference_count);
		if (safe_int_dec(&pck->reference->reference_count) == 0) {
			gf_filter_packet_destroy(pck->reference);
		}
		pck->reference = NULL;
	}
	/*this is a property reference packet, its destruction may happen at ANY time*/
	if (is_ref_props_packet) {
		if (pck->session->pcks_refprops_reservoir) {
			gf_fq_add(pck->session->pcks_refprops_reservoir, pck);
		} else {
			gf_free(pck);
		}
	} else if (is_filter_destroyed) {
		if (!pck->filter_owns_mem && pck->data) gf_free(pck->data);
		gf_free(pck);
	} else if (pck->filter_owns_mem ) {
		if (pid->filter->pcks_shared_reservoir) {
			gf_fq_add(pid->filter->pcks_shared_reservoir, pck);
		} else {
			gf_free(pck);
		}
	} else {
		if (pid->filter->pcks_alloc_reservoir) {
			gf_fq_add(pid->filter->pcks_alloc_reservoir, pck);
		} else {
			if (pck->data) gf_free(pck->data);
			gf_free(pck);
		}
	}
}

static Bool gf_filter_aggregate_packets(GF_FilterPidInst *dst)
{
	u32 size=0, pos=0;
	u64 byte_offset = 0;
	u64 first_offset = 0;
	u8 *data;
	GF_FilterPacket *final;
	u32 i, count;
	GF_FilterPckInfo info;

	//no need to lock the packet list since only the dispatch thread operates on it

	count=gf_list_count(dst->pck_reassembly);
	//no packet to reaggregate
	if (!count) return GF_FALSE;
	//single packet, update PID buffer and dispatch to packet queue
	if (count==1) {
		GF_FilterPacketInstance *pcki = gf_list_pop_back(dst->pck_reassembly);
		safe_int_inc(&dst->filter->pending_packets);
		if (pcki->pck->info.duration && pcki->pck->pid_props->timescale) {
			s64 duration = ((u64)pcki->pck->info.duration) * 1000000;
			duration /= pcki->pck->pid_props->timescale;
			safe_int64_add(&dst->buffer_duration, duration);
		}
		pcki->pck->info.flags |= GF_PCKF_BLOCK_START | GF_PCKF_BLOCK_END;
 		gf_fq_add(dst->packets, pcki);
		return GF_TRUE;
	}

	for (i=0; i<count; i++) {
		GF_FilterPacketInstance *pck = gf_list_get(dst->pck_reassembly, i);
		assert(pck);
		assert(! (pck->pck->info.flags & GF_PCKF_BLOCK_START) || ! (pck->pck->info.flags & GF_PCKF_BLOCK_END) );
		size += pck->pck->data_length;
		if (!i) {
			first_offset = byte_offset = pck->pck->info.byte_offset;
			if (byte_offset != GF_FILTER_NO_BO) byte_offset += pck->pck->data_length;
		}else if (byte_offset == pck->pck->info.byte_offset) {
			byte_offset += pck->pck->data_length;
		} else {
			byte_offset = GF_FILTER_NO_BO;
		}
	}

	final = gf_filter_pck_new_alloc(dst->pid, size, &data);
	pos=0;

	for (i=0; i<count; i++) {
		GF_FilterPacket *pck;
		GF_FilterPacketInstance *pcki = gf_list_get(dst->pck_reassembly, i);
		assert(pcki);
		pck = pcki->pck;

		if (!pos) {
			info = pck->info;
		} else {
			if (pcki->pck->info.duration > info.duration)
				info.duration = pcki->pck->info.duration;
			if ((pcki->pck->info.dts != GF_FILTER_NO_TS) && pcki->pck->info.dts > info.dts)
				info.dts = pcki->pck->info.dts;
			if ((pcki->pck->info.cts != GF_FILTER_NO_TS) && pcki->pck->info.cts > info.cts)
				info.cts = pcki->pck->info.cts;

			info.flags |= pcki->pck->info.flags;
			if (pcki->pck->info.carousel_version_number > info.carousel_version_number)
				info.carousel_version_number = pcki->pck->info.carousel_version_number;
		}
		memcpy(data+pos, pcki->pck->data, pcki->pck->data_length);
		pos += pcki->pck->data_length;

		gf_filter_pck_merge_properties(pcki->pck, final);

		//copy the first pid_props non null
		if (pcki->pck->pid_props && !final->pid_props) {
			final->pid_props = pcki->pck->pid_props;
			safe_int_inc(&final->pid_props->reference_count);
		}

		gf_list_rem(dst->pck_reassembly, i);

		//destroy pcki
		if (i+1<count) {
			pcki->pck = NULL;
			pcki->pid = NULL;

			if (pck->pid->filter->pcks_inst_reservoir) {
				gf_fq_add(pck->pid->filter->pcks_inst_reservoir, pcki);
			} else {
				gf_free(pcki);
			}
		} else {
			pcki->pck = final;
			safe_int_inc(&final->reference_count);
			final->info = info;
			final->info.flags |= GF_PCKF_BLOCK_START | GF_PCKF_BLOCK_END;

			safe_int_inc(&dst->filter->pending_packets);

			if (info.duration && pck->pid_props && pck->pid_props->timescale) {
				s64 duration = ((u64) info.duration) * 1000000;
				duration /= pck->pid_props->timescale;
				safe_int64_add(&dst->buffer_duration, duration);
			}
			//not continous set of bytes reaggregated
			if (byte_offset == GF_FILTER_NO_BO) final->info.byte_offset = GF_FILTER_NO_BO;
			else final->info.byte_offset = first_offset;

			gf_fq_add(dst->packets, pcki);

		}
		//unref pck
		assert(pck->reference_count);
		if (safe_int_dec(&pck->reference_count) == 0) {
			gf_filter_packet_destroy(pck);
		}

		count--;
		i--;
	}
	return GF_TRUE;
}

GF_EXPORT
void gf_filter_pck_discard(GF_FilterPacket *pck)
{
	if (PCK_IS_INPUT(pck)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to discard input packet on output PID in filter %s\n", pck->pid->filter->name));
		return;
	}
	//function is only used to discard packets allocated but not dispatched, eg with no reference count
	//so don't decrement the counter
	if (pck->reference_count == 0) {
		gf_filter_packet_destroy(pck);
	}
}

GF_Err gf_filter_pck_send_internal(GF_FilterPacket *pck, Bool from_filter)
{
	u32 i, count, nb_dispatch=0, nb_discard=0;
	GF_FilterPid *pid;
	s64 duration=0;
	u32 timescale=0;
	GF_FilterClockType cktype;
#ifdef GPAC_MEMORY_TRACKING
	u32 nb_allocs=0, nb_reallocs=0, prev_nb_allocs=0, prev_nb_reallocs=0;
#endif


	if (!pck->src_filter) return GF_BAD_PARAM;

#ifdef GPAC_MEMORY_TRACKING
	if (pck->pid->filter->nb_process_since_reset)
		gf_mem_get_stats(&prev_nb_allocs, NULL, &prev_nb_reallocs, NULL);
#endif

	assert(pck);
	assert(pck->pid);
	pid = pck->pid;

	if (PCK_IS_INPUT(pck)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to dispatch input packet on output PID in filter %s\n", pck->pid->filter->name));
		return GF_BAD_PARAM;
	}

	if (pid->discard_input_packets) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s reset pending, discarding input packet\n", pid->filter->name, pid->name));
		safe_int_inc(&pck->reference_count);
		assert(pck->reference_count);
		if (safe_int_dec(&pck->reference_count) == 0) {
			gf_filter_packet_destroy(pck);
		}
		return GF_OK;
	}

	//special case for source filters (no input pids), mark as playing once we have a packet sent
	if (!pid->filter->num_input_pids && !pid->initial_play_done && !pid->is_playing) {
		pid->initial_play_done = GF_TRUE;
		pid->is_playing = GF_TRUE;
		pid->filter->nb_pids_playing++;
	}

	if (pid->filter->eos_probe_state)
		pid->filter->eos_probe_state = 2;

	pid->filter->nb_pck_io++;

	gf_rmt_begin(pck_send, GF_RMT_AGGREGATE);

	//send from filter, update flags
	if (from_filter) {
		Bool is_cmd = (pck->info.flags & GF_PCK_CKTYPE_MASK) ? GF_TRUE : GF_FALSE;
		if (! is_cmd ) {
			gf_filter_forward_clock(pck->pid->filter);
		}
		if ( (pck->info.flags & GF_PCK_CMD_MASK) == GF_PCK_CMD_PID_EOS) {
			if (!pid->has_seen_eos) {
				pid->has_seen_eos = GF_TRUE;
				GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s PID %s EOS detected\n", pck->pid->filter->name, pck->pid->name));
			}
		} else if (pid->has_seen_eos && !is_cmd) {
			pid->has_seen_eos = GF_FALSE;
		}


		if (pid->filter->pid_info_changed) {
			pid->filter->pid_info_changed = GF_FALSE;
			pid->pid_info_changed = GF_TRUE;
		}

		//a new property map was created -  flag the packet; don't do this if first packet dispatched on pid
		pck->info.flags &= ~GF_PCKF_PROPS_CHANGED;

		if (!pid->request_property_map && !(pck->info.flags & GF_PCK_CMD_MASK) && (pid->nb_pck_sent || pid->props_changed_since_connect) ) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s properties modified, marking packet\n", pck->pid->filter->name, pck->pid->name));

			pck->info.flags |= GF_PCKF_PROPS_CHANGED;
		}
		//any new pid_set_property after this packet will trigger a new property map
		if (! (pck->info.flags & GF_PCK_CMD_MASK)) {
			pid->request_property_map = GF_TRUE;
			pid->props_changed_since_connect = GF_FALSE;
		}
		if (pid->pid_info_changed) {
			pck->info.flags |= GF_PCKF_INFO_CHANGED;
			pid->pid_info_changed = GF_FALSE;
		}
	}

	if (pck->pid_props) {
		timescale = pck->pid_props->timescale;
	} else {
		//pid properties applying to this packet are the last defined ones
		pck->pid_props = gf_list_last(pid->properties);
		if (pck->pid_props) {
			safe_int_inc(&pck->pid_props->reference_count);
			timescale = pck->pid_props->timescale;
		}
	}

	if (pid->filter->out_pid_connection_pending || pid->filter->has_pending_pids || pid->init_task_pending) {
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s PID %s connection pending, queuing packet\n", pck->pid->filter->name, pck->pid->name));
		if (!pid->filter->postponed_packets) pid->filter->postponed_packets = gf_list_new();
		gf_list_add(pid->filter->postponed_packets, pck);

#ifdef GPAC_MEMORY_TRACKING
		if (pck->pid->filter->session->check_allocs) {
			gf_mem_get_stats(&nb_allocs, NULL, &nb_reallocs, NULL);
			pck->pid->filter->session->nb_alloc_pck += (nb_allocs - prev_nb_allocs);
			pck->pid->filter->session->nb_realloc_pck += (nb_reallocs - prev_nb_reallocs);
		}
#endif
		gf_rmt_end();
		return GF_PENDING_PACKET;
	}
	//now dispatched
	pck->src_filter = NULL;

	assert(pck->pid);
	if (! (pck->info.flags & GF_PCK_CMD_MASK) ) {
		pid->nb_pck_sent++;
		if (pck->data_length) {
			pid->filter->nb_pck_sent++;
			pid->filter->nb_bytes_sent += pck->data_length;
		} else if (pck->frame_ifce) {
			pid->filter->nb_hw_pck_sent++;
		}
	}

	cktype = ( pck->info.flags & GF_PCK_CKTYPE_MASK) >> GF_PCK_CKTYPE_POS;

	if (cktype == GF_FILTER_CLOCK_PCR_DISC) {
		pid->duration_init = GF_FALSE;
		pid->min_pck_cts = pid->max_pck_cts = 0;
		pid->nb_unreliable_dts = 0;
	}

	if (!cktype) {
		Bool unreliable_dts = GF_FALSE;
		if (pck->info.dts==GF_FILTER_NO_TS) {
			pck->info.dts = pck->info.cts;

			if (pid->recompute_dts) {
				if (pck->info.cts == pid->last_pck_cts) {
					pck->info.dts = pid->last_pck_dts;
				} else {
					s64 min_dur = pck->info.cts;
					min_dur -= (s64) pid->min_pck_cts;
					if (min_dur<0) min_dur*= -1;
					if ((u64) min_dur > pid->min_pck_duration) min_dur = pid->min_pck_duration;
					if (!min_dur) {
						min_dur = 1;
						unreliable_dts = GF_TRUE;
						pid->nb_unreliable_dts++;
					} else if (pid->nb_unreliable_dts) {
						pid->last_pck_dts -= pid->nb_unreliable_dts;
						pid->last_pck_dts += min_dur * pid->nb_unreliable_dts;
						pid->nb_unreliable_dts = 0;
						if (pid->last_pck_dts + min_dur > pck->info.cts) {
							if ((s64) pck->info.cts > min_dur)
 								pid->last_pck_dts = pck->info.cts - min_dur;
							else
								pid->last_pck_dts = 0;
						}
					}
					if (pid->last_pck_dts)
						pck->info.dts = pid->last_pck_dts + min_dur;
				}
			}
		}
		else if (pck->info.cts==GF_FILTER_NO_TS)
			pck->info.cts = pck->info.dts;

		if (pck->info.cts != GF_FILTER_NO_TS) {
			if (! pid->duration_init) {
				pid->last_pck_dts = pck->info.dts;
				pid->last_pck_cts = pck->info.cts;
				pid->max_pck_cts = pid->min_pck_cts = pck->info.cts;
				pid->duration_init = GF_TRUE;
			} else if (!pck->info.duration && !(pck->info.flags & GF_PCKF_DUR_SET) ) {
				if (!unreliable_dts && (pck->info.dts!=GF_FILTER_NO_TS)) {
					duration = pck->info.dts - pid->last_pck_dts;
					if (duration<0) duration = -duration;
				} else if (pck->info.cts!=GF_FILTER_NO_TS) {
					duration = pck->info.cts - pid->last_pck_cts;
					if (duration<0) duration = -duration;
				}

				if (pid->recompute_dts) {
					if (pck->info.cts > pid->max_pck_cts)
						pid->max_pck_cts = pck->info.cts;
					if (pck->info.cts < pid->max_pck_cts) {
						if (pck->info.cts <= pid->min_pck_cts) {
							pid->min_pck_cts = pck->info.cts;
						} else if (pck->info.cts>pid->last_pck_cts) {
							pid->min_pck_cts = pck->info.cts;
						}
					}
				}

				pid->last_pck_dts = pck->info.dts;
				pid->last_pck_cts = pck->info.cts;

			} else {
				duration = pck->info.duration;
				pid->last_pck_dts = pck->info.dts;
				pid->last_pck_cts = pck->info.cts;
			}
		} else {
			duration = pck->info.duration;
		}

		if (duration) {
			if (!pid->min_pck_duration) pid->min_pck_duration = (u32) duration;
			else if ((u32) duration < pid->min_pck_duration) pid->min_pck_duration = (u32) duration;
		}
		if (!pck->info.duration && pid->min_pck_duration)
			pck->info.duration = (u32) duration;

		//may happen if we don't have DTS, only CTS signaled and B-frames
		if ((s32) pck->info.duration < 0) {
			pck->info.duration = 0;
			duration = 0;
		}

#ifndef GPAC_DISABLE_LOG
		if (gf_log_tool_level_on(GF_LOG_FILTER, GF_LOG_DEBUG)) {
			u8 sap_type = (pck->info.flags & GF_PCK_SAP_MASK) >> GF_PCK_SAP_POS;
			u8 seek = (pck->info.flags & GF_PCKF_SEEK) ? 1 : 0;
			u8 bstart = (pck->info.flags & GF_PCKF_BLOCK_START) ? 1 : 0;
			u8 bend = (pck->info.flags & GF_PCKF_BLOCK_END) ? 1 : 0;

			if ((pck->info.dts != GF_FILTER_NO_TS) && (pck->info.cts != GF_FILTER_NO_TS) ) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s sent packet DTS "LLU" CTS "LLU" SAP %d seek %d duration %d S/E %d/%d\n", pck->pid->filter->name, pck->pid->name, pck->info.dts, pck->info.cts, sap_type, seek, pck->info.duration, bstart, bend));
			}
			else if ((pck->info.cts != GF_FILTER_NO_TS) ) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s sent packet CTS "LLU" SAP %d seek %d duration %d S/E %d/%d\n", pck->pid->filter->name, pck->pid->name, pck->info.cts, sap_type, seek, pck->info.duration, bstart, bend));
			}
			else if ((pck->info.dts != GF_FILTER_NO_TS) ) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s sent packet DTS "LLU" SAP %d seek %d duration %d S/E %d/%d\n", pck->pid->filter->name, pck->pid->name, pck->info.dts, sap_type, seek, pck->info.duration, bstart, bend));
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s sent packet no DTS/PTS SAP %d seek %d duration %d S/E %d/%d\n", pck->pid->filter->name, pck->pid->name, sap_type, seek, pck->info.duration, bstart, bend));
			}
		}
#endif
	} else {
		pck->info.duration = 0;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s sent clock reference "LLU"%s\n", pck->pid->filter->name, pck->pid->name, pck->info.cts, (cktype==GF_FILTER_CLOCK_PCR_DISC) ? " - discontinuity detected" : ""));
	}



	//protect packet from destruction - this could happen
	//1) during aggregation of packets
	//2) after dispatching to the packet queue of the next filter, that packet may be consumed
	//by its destination before we are done adding to the other destination
	safe_int_inc(&pck->reference_count);

	assert(pck->pid);
	count = pck->pid->num_destinations;
	for (i=0; i<count; i++) {
		Bool post_task=GF_FALSE;
		GF_FilterPacketInstance *inst;
		GF_FilterPidInst *dst = gf_list_get(pck->pid->destinations, i);
		if (!dst->filter || dst->filter->finalized || dst->filter->removed || !dst->filter->freg->process) continue;

		if (dst->discard_inputs) {
			//in discard input mode, we drop all input packets but trigger reconfigure as they happen
			if ((pck->info.flags & GF_PCKF_PROPS_CHANGED) && (dst->props != pck->pid_props)) {
				//unassign old property list and set the new one
				if (dst->props) {
					assert(dst->props->reference_count);
					if (safe_int_dec(& dst->props->reference_count) == 0) {
						//see \ref gf_filter_pid_merge_properties_internal for mutex
						gf_mx_p(dst->pid->filter->tasks_mx);
						gf_list_del_item(dst->pid->properties, dst->props);
						gf_mx_v(dst->pid->filter->tasks_mx);
						gf_props_del(dst->props);
					}
				}
				dst->props = pck->pid_props;
				safe_int_inc( & dst->props->reference_count);

				assert(dst->filter->freg->configure_pid);
				//reset the blacklist whenever reconfiguring, since we may need to reload a new filter chain
				//in which a previously blacklisted filter (failing (re)configure for previous state) could
				//now work, eg moving from formatA to formatB then back to formatA
				gf_list_reset(dst->filter->blacklisted);
				//and post a reconfigure task
				gf_fs_post_task(dst->filter->session, gf_filter_pid_reconfigure_task, dst->filter, dst->pid, "pidinst_reconfigure", NULL);
			}
			nb_discard++;
			continue;
		}

		inst = gf_fq_pop(pck->pid->filter->pcks_inst_reservoir);
		if (!inst) {
			GF_SAFEALLOC(inst, GF_FilterPacketInstance);
			if (!inst) return GF_OUT_OF_MEM;
		}
		inst->pck = pck;
		inst->pid = dst;
		inst->pid_props_change_done = 0;
		inst->pid_info_change_done = 0;
		//if packet is an openGL interface, force scheduling on main thread for the destination
		if (pck->frame_ifce&&pck->frame_ifce->get_gl_texture)
			dst->filter->main_thread_forced = GF_TRUE;

		if ((inst->pck->info.flags & GF_PCK_CMD_MASK) == GF_PCK_CMD_PID_EOS)  {
			safe_int_inc(&inst->pid->nb_eos_signaled);
		}

		if (!inst->pid->handles_clock_references && cktype) {
			safe_int_inc(&inst->pid->nb_clocks_signaled);
		}

		safe_int_inc(&pck->reference_count);
		nb_dispatch++;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Dispatching packet from filter %s to filter %s - %d packet in PID %s buffer ("LLU" us buffer)\n", pid->filter->name, dst->filter->name, gf_fq_count(dst->packets), pid->name, dst->buffer_duration ));

		if (cktype) {
			safe_int_inc(&dst->filter->pending_packets);
			gf_fq_add(dst->packets, inst);
			post_task = GF_TRUE;
		} else if (dst->requires_full_data_block) {
			if (pck->info.flags & GF_PCKF_BLOCK_START) {
				//missed end of previous, aggregate all before excluding this packet
				if (!dst->last_block_ended) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s: Missed end of block signaling but got start of block - queuing for potential reaggregation\n", pid->filter->name));

					//post process task if we have been reaggregating a packet
					post_task = gf_filter_aggregate_packets(dst);
					if (post_task && pid->nb_reaggregation_pending) pid->nb_reaggregation_pending--;
				}
				dst->last_block_ended = GF_TRUE;
			}
			//block end, aggregate all before and including this packet
			if (pck->info.flags & GF_PCKF_BLOCK_END) {
				//not starting at this packet, append and aggregate
				if (!(pck->info.flags & GF_PCKF_BLOCK_START) && gf_list_count(dst->pck_reassembly) ) {
					//insert packet into reassembly (no need to lock here)
					gf_list_add(dst->pck_reassembly, inst);

					gf_filter_aggregate_packets(dst);
					if (pid->nb_reaggregation_pending) pid->nb_reaggregation_pending--;
				}
				//single block packet, direct dispatch in packet queue (aggregation done before)
				else {
					assert(dst->last_block_ended);

					if (pck->info.duration && timescale) {
						duration = ((u64)pck->info.duration) * 1000000;
						duration /= timescale;
						safe_int64_add(&dst->buffer_duration, duration);
					}
					inst->pck->info.flags |= GF_PCKF_BLOCK_START;

					safe_int_inc(&dst->filter->pending_packets);
					gf_fq_add(dst->packets, inst);
				}
				dst->last_block_ended = GF_TRUE;
				post_task = GF_TRUE;
			}
			//new block start or continuation
			else {
				if (pck->info.flags & GF_PCKF_BLOCK_START) {
					pid->nb_reaggregation_pending++;
				}

				//if packet mem is hold by filter we must copy the packet since it is no longer
				//consumable until end of block is received, and source might be waiting for this packet to be freed to dispatch further packets
				if (inst->pck->filter_owns_mem) {
					u8 *data;
					u32 alloc_size;
					inst->pck = gf_filter_pck_new_alloc_internal(pck->pid, pck->data_length, &data, GF_TRUE);
					alloc_size = inst->pck->alloc_size;
					memcpy(inst->pck, pck, sizeof(GF_FilterPacket));
					inst->pck->pck = inst->pck;
					inst->pck->data = data;
					memcpy(inst->pck->data, pck->data, pck->data_length);
					inst->pck->alloc_size = alloc_size;
					inst->pck->filter_owns_mem = 0;
					inst->pck->reference_count = 0;
					inst->pck->reference = NULL;
					inst->pck->destructor = NULL;
					inst->pck->frame_ifce = NULL;
					if (pck->props) {
						GF_Err e;
						inst->pck->props = gf_props_new(pck->pid->filter);
						if (inst->pck->props) {
							e = gf_props_merge_property(inst->pck->props, pck->props, NULL, NULL);
						} else {
							e = GF_OUT_OF_MEM;
						}
						if (e) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter %s: failed to copy properties for cloned packet: %s\n", pid->filter->name, gf_error_to_string(e) ));
						}
					}
					if (inst->pck->pid_props) {
						safe_int_inc(&inst->pck->pid_props->reference_count);
					}
					
					assert(pck->reference_count);
					safe_int_dec(&pck->reference_count);
					safe_int_inc(&inst->pck->reference_count);
				}
				//insert packet into reassembly (no need to lock here)
				gf_list_add(dst->pck_reassembly, inst);

				dst->last_block_ended = GF_FALSE;
				//block not complete, don't post process task
			}

		} else {
			duration=0;
			//store start of block info
			if (pck->info.flags & GF_PCKF_BLOCK_START) {
				dst->first_block_started = GF_TRUE;
				duration = pck->info.duration;
			}
			if (pck->info.flags & GF_PCKF_BLOCK_END) {
				//we didn't get a start for this end of block use the packet duration
				if (!dst->first_block_started) {
					duration=pck->info.duration;
				}
				dst->first_block_started = GF_FALSE;
			}

			if (duration && timescale) {
				duration *= 1000000;
				duration /= timescale;
				safe_int64_add(&dst->buffer_duration, duration);
			}

			safe_int_inc(&dst->filter->pending_packets);
//
			gf_fq_add(dst->packets, inst);
			post_task = GF_TRUE;
		}
		if (post_task) {

			//make sure we lock the tasks mutex before getting the packet count, otherwise we might end up with a wrong number of packets
			//if one thread consumes one packet while the dispatching thread  (the caller here) is still upddating the state for that pid
			gf_mx_p(pid->filter->tasks_mx);
			u32 nb_pck = gf_fq_count(dst->packets);
			//update buffer occupancy before dispatching the task - if target pid is processed before we are done disptching his packet, pid buffer occupancy
			//will be updated during packet drop of target
			if (pid->nb_buffer_unit < nb_pck) pid->nb_buffer_unit = nb_pck;
			if ((s64) pid->buffer_duration < dst->buffer_duration) pid->buffer_duration = dst->buffer_duration;
			gf_mx_v(pid->filter->tasks_mx);

			//post process task
			gf_filter_post_process_task_internal(dst->filter, pid->direct_dispatch);
		}
	}

#ifdef GPAC_MEMORY_TRACKING
	if (pck->pid->filter->session->check_allocs) {
		gf_mem_get_stats(&nb_allocs, NULL, &nb_reallocs, NULL);
		pck->pid->filter->session->nb_alloc_pck += (nb_allocs - prev_nb_allocs);
		pck->pid->filter->session->nb_realloc_pck += (nb_reallocs - prev_nb_reallocs);
	}
#endif

	gf_filter_pid_would_block(pid);

	//unprotect the packet now that it is safely dispatched
	assert(pck->reference_count);
	if (safe_int_dec(&pck->reference_count) == 0) {
		if (!nb_dispatch) {
			if (nb_discard) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("All PID destinations on filter %s are in discard mode - discarding\n", pid->filter->name));
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("No PID destination on filter %s for packet - discarding\n", pid->filter->name));
			}
		}
		gf_filter_packet_destroy(pck);
	}
	gf_rmt_end();
	return GF_OK;
}

GF_EXPORT
GF_Err gf_filter_pck_send(GF_FilterPacket *pck)
{
	assert(pck->pid);
	return gf_filter_pck_send_internal(pck, GF_TRUE);
}

GF_EXPORT
GF_Err gf_filter_pck_ref(GF_FilterPacket **pck)
{
	if (! pck ) return GF_BAD_PARAM;
	if (! *pck ) return GF_OK;

	(*pck) = (*pck)->pck;
	safe_int_inc(& (*pck)->reference_count);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_filter_pck_ref_props(GF_FilterPacket **pck)
{
	GF_FilterPacket *npck, *srcpck;
	GF_FilterPid *pid;
	if (! pck ) return GF_BAD_PARAM;
	if (! *pck ) return GF_OK;

	srcpck = (*pck)->pck;
	pid = srcpck->pid;

	npck = gf_fq_pop( pid->filter->session->pcks_refprops_reservoir);
	if (!npck) {
		GF_SAFEALLOC(npck, GF_FilterPacket);
		if (!npck) return GF_OUT_OF_MEM;
	}
	npck->pck = npck;
	npck->data = NULL;
	npck->data_length = 0;
	npck->filter_owns_mem = 0;
	npck->destructor = NULL;
	gf_filter_pck_reset_props(npck, pid);
	npck->info = srcpck->info;
	npck->info.flags |= GF_PCKF_PROPS_REFERENCE;

	if (srcpck->props) {
		npck->props = srcpck->props;
		safe_int_inc(& npck->props->reference_count);
	}
	if (srcpck->pid_props) {
		npck->pid_props = srcpck->pid_props;
		safe_int_inc(& npck->pid_props->pckrefs_reference_count);
	}

	safe_int_inc(& npck->reference_count);
	*pck = npck;
	return GF_OK;
}

GF_EXPORT
void gf_filter_pck_unref(GF_FilterPacket *pck)
{
	assert(pck);
	pck=pck->pck;

	assert(pck->reference_count);
	if (safe_int_dec(&pck->reference_count) == 0) {
		gf_filter_packet_destroy(pck);
	}
}

GF_EXPORT
const u8 *gf_filter_pck_get_data(GF_FilterPacket *pck, u32 *size)
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
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to set property on an input packet in filter %s\n", pck->pid->filter->name));
		return GF_BAD_PARAM;
	}
	//get true packet pointer
	pck=pck->pck;
	hash = gf_props_hash_djb2(prop_4cc, prop_name ? prop_name : dyn_name);

	if (!pck->props) {
		pck->props = gf_props_new(pck->pid->filter);
	} else {
		gf_props_remove_property(pck->props, hash, prop_4cc, prop_name ? prop_name : dyn_name);
	}
	if (!value) return GF_OK;
	
	return gf_props_insert_property(pck->props, hash, prop_4cc, prop_name, dyn_name, value);
}

GF_EXPORT
GF_Err gf_filter_pck_set_property(GF_FilterPacket *pck, u32 prop_4cc, const GF_PropertyValue *value)
{
	return gf_filter_pck_set_property_full(pck, prop_4cc, NULL, NULL, value);
}

GF_EXPORT
GF_Err gf_filter_pck_set_property_str(GF_FilterPacket *pck, const char *name, const GF_PropertyValue *value)
{
	return gf_filter_pck_set_property_full(pck, 0, name, NULL, value);
}

GF_EXPORT
GF_Err gf_filter_pck_set_property_dyn(GF_FilterPacket *pck, char *name, const GF_PropertyValue *value)
{
	return gf_filter_pck_set_property_full(pck, 0, NULL, name, value);
}

GF_EXPORT
const GF_PropertyValue *gf_filter_pck_get_property(GF_FilterPacket *pck, u32 prop_4cc)
{
	//get true packet pointer
	pck = pck->pck;
	if (!pck->props) return NULL;
	return gf_props_get_property(pck->props, prop_4cc, NULL);
}

GF_EXPORT
const GF_PropertyValue *gf_filter_pck_get_property_str(GF_FilterPacket *pck, const char *prop_name)
{
	//get true packet pointer
	pck = pck->pck;
	if (!pck->props) return NULL;
	return gf_props_get_property(pck->props, 0, prop_name);
}

GF_EXPORT
const GF_PropertyValue *gf_filter_pck_enum_properties(GF_FilterPacket *pck, u32 *idx, u32 *prop_4cc, const char **prop_name)
{
	if (!pck->pck->props) return NULL;
	return gf_props_enum_property(pck->pck->props, idx, prop_4cc, prop_name);
}

#define PCK_SETTER_CHECK(_pname) \
	if (PCK_IS_INPUT(pck)) { \
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to set %s on an input packet in filter %s\n", _pname, pck->pid->filter->name));\
		return GF_BAD_PARAM; \
	} \


GF_EXPORT
GF_Err gf_filter_pck_set_framing(GF_FilterPacket *pck, Bool is_start, Bool is_end)
{
	PCK_SETTER_CHECK("framing info")

	if (is_start) pck->info.flags |= GF_PCKF_BLOCK_START;
	else pck->info.flags &= ~GF_PCKF_BLOCK_START;

	if (is_end) pck->info.flags |= GF_PCKF_BLOCK_END;
	else pck->info.flags &= ~GF_PCKF_BLOCK_END;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_filter_pck_get_framing(GF_FilterPacket *pck, Bool *is_start, Bool *is_end)
{
	assert(pck);
	//get true packet pointer
	pck=pck->pck;
	if (is_start) *is_start = (pck->info.flags & GF_PCKF_BLOCK_START) ? GF_TRUE : GF_FALSE;
	if (is_end) *is_end = (pck->info.flags & GF_PCKF_BLOCK_END) ? GF_TRUE : GF_FALSE;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_filter_pck_set_dts(GF_FilterPacket *pck, u64 dts)
{
	PCK_SETTER_CHECK("DTS")
	pck->info.dts = dts;
	return GF_OK;
}

GF_EXPORT
u64 gf_filter_pck_get_dts(GF_FilterPacket *pck)
{
	assert(pck);
	//get true packet pointer
	return pck->pck->info.dts;
}

GF_EXPORT
GF_Err gf_filter_pck_set_cts(GF_FilterPacket *pck, u64 cts)
{
	PCK_SETTER_CHECK("CTS")
	pck->info.cts = cts;
	return GF_OK;
}

GF_EXPORT
u64 gf_filter_pck_get_cts(GF_FilterPacket *pck)
{
	assert(pck);
	//get true packet pointer
	return pck->pck->info.cts;
}

GF_EXPORT
u32 gf_filter_pck_get_timescale(GF_FilterPacket *pck)
{
	assert(pck);
	//get true packet pointer
	return pck->pck->pid_props->timescale ? pck->pck->pid_props->timescale : 1000;
}

GF_EXPORT
GF_Err gf_filter_pck_set_sap(GF_FilterPacket *pck, GF_FilterSAPType sap_type)
{
	PCK_SETTER_CHECK("SAP")
	pck->info.flags &= ~GF_PCK_SAP_MASK;
	pck->info.flags |= (sap_type)<<GF_PCK_SAP_POS;

	return GF_OK;
}

GF_EXPORT
GF_FilterSAPType gf_filter_pck_get_sap(GF_FilterPacket *pck)
{
	assert(pck);
	//get true packet pointer
	return (GF_FilterSAPType) ( (pck->pck->info.flags & GF_PCK_SAP_MASK) >> GF_PCK_SAP_POS);
}

GF_EXPORT
GF_Err gf_filter_pck_set_roll_info(GF_FilterPacket *pck, s16 roll_count)
{
	PCK_SETTER_CHECK("ROLL")
	pck->info.roll = roll_count;
	return GF_OK;
}

GF_EXPORT
s16 gf_filter_pck_get_roll_info(GF_FilterPacket *pck)
{
	assert(pck);
	//get true packet pointer
	return pck->pck->info.roll;
}

GF_EXPORT
GF_Err gf_filter_pck_set_interlaced(GF_FilterPacket *pck, u32 is_interlaced)
{
	PCK_SETTER_CHECK("interlaced")
	pck->info.flags &= ~GF_PCK_ILACE_MASK;
	if (is_interlaced)  pck->info.flags |= is_interlaced<<GF_PCK_ILACE_POS;
	return GF_OK;
}

GF_EXPORT
u32 gf_filter_pck_get_interlaced(GF_FilterPacket *pck)
{
	assert(pck);
	//get true packet pointer
	return (pck->pck->info.flags & GF_PCK_ILACE_MASK) >> GF_PCK_ILACE_POS;
}

GF_EXPORT
GF_Err gf_filter_pck_set_corrupted(GF_FilterPacket *pck, Bool is_corrupted)
{
	PCK_SETTER_CHECK("corrupted")
	pck->info.flags &= ~GF_PCKF_CORRUPTED;
	if (is_corrupted) pck->info.flags |= GF_PCKF_CORRUPTED;
	return GF_OK;
}

GF_EXPORT
Bool gf_filter_pck_get_corrupted(GF_FilterPacket *pck)
{
	assert(pck);
	//get true packet pointer
	return pck->pck->info.flags & GF_PCKF_CORRUPTED ? GF_TRUE : GF_FALSE;
}

GF_EXPORT
GF_Err gf_filter_pck_set_duration(GF_FilterPacket *pck, u32 duration)
{
	PCK_SETTER_CHECK("dur")
	pck->info.duration = duration;
	pck->info.flags |= GF_PCKF_DUR_SET;
	return GF_OK;
}

GF_EXPORT
u32 gf_filter_pck_get_duration(GF_FilterPacket *pck)
{
	assert(pck);
	//get true packet pointer
	return pck->pck->info.duration;
}

GF_EXPORT
GF_Err gf_filter_pck_set_seek_flag(GF_FilterPacket *pck, Bool is_seek)
{
	PCK_SETTER_CHECK("seek")
	pck->info.flags &= ~GF_PCKF_SEEK;
	if (is_seek) pck->info.flags |= GF_PCKF_SEEK;
	return GF_OK;
}

GF_EXPORT
Bool gf_filter_pck_get_seek_flag(GF_FilterPacket *pck)
{
	assert(pck);
	//get true packet pointer
	return (pck->pck->info.flags & GF_PCKF_SEEK) ? GF_TRUE : GF_FALSE;
}

GF_EXPORT
GF_Err gf_filter_pck_set_dependency_flags(GF_FilterPacket *pck, u8 dep_flags)
{
	PCK_SETTER_CHECK("dependency_flags")
	pck->info.flags &= ~0xFF;
	pck->info.flags |= dep_flags;
	return GF_OK;
}

GF_EXPORT
u8 gf_filter_pck_get_dependency_flags(GF_FilterPacket *pck)
{
	assert(pck);
	//get true packet pointer
	return pck->pck->info.flags & 0xFF;
}

GF_EXPORT
GF_Err gf_filter_pck_set_carousel_version(GF_FilterPacket *pck, u8 version_number)
{
	PCK_SETTER_CHECK("carousel_version")
	pck->info.carousel_version_number = version_number;
	return GF_OK;
}

GF_EXPORT
u8 gf_filter_pck_get_carousel_version(GF_FilterPacket *pck)
{
	assert(pck);
	//get true packet pointer
	return pck->pck->info.carousel_version_number;
}

GF_EXPORT
GF_Err gf_filter_pck_set_byte_offset(GF_FilterPacket *pck, u64 byte_offset)
{
	PCK_SETTER_CHECK("byteOffset")
	pck->info.byte_offset = byte_offset;
	return GF_OK;
}

GF_EXPORT
u64 gf_filter_pck_get_byte_offset(GF_FilterPacket *pck)
{
	assert(pck);
	//get true packet pointer
	return pck->pck->info.byte_offset;
}

GF_EXPORT
GF_Err gf_filter_pck_set_crypt_flags(GF_FilterPacket *pck, u8 crypt_flag)
{
	PCK_SETTER_CHECK("byteOffset")
	pck->info.flags &= ~GF_PCK_CRYPT_MASK;
	pck->info.flags |= crypt_flag << GF_PCK_CRYPT_POS;
	return GF_OK;
}

GF_EXPORT
u8 gf_filter_pck_get_crypt_flags(GF_FilterPacket *pck)
{
	//get true packet pointer
	return (pck->pck->info.flags & GF_PCK_CRYPT_MASK) >> GF_PCK_CRYPT_POS;
}

GF_EXPORT
GF_Err gf_filter_pck_set_seq_num(GF_FilterPacket *pck, u32 seq_num)
{
	PCK_SETTER_CHECK("seqNum")
	pck->info.seq_num = seq_num;
	return GF_OK;
}

GF_EXPORT
u32 gf_filter_pck_get_seq_num(GF_FilterPacket *pck)
{
	//get true packet pointer
	return pck->pck->info.seq_num;
}

GF_EXPORT
GF_Err gf_filter_pck_set_clock_type(GF_FilterPacket *pck, GF_FilterClockType ctype)
{
	PCK_SETTER_CHECK("clock_type")
	pck->info.flags &= ~GF_PCK_CKTYPE_MASK;
	pck->info.flags |= ctype << GF_PCK_CKTYPE_POS;
	return GF_OK;
}

GF_EXPORT
GF_FilterClockType gf_filter_pck_get_clock_type(GF_FilterPacket *pck)
{
	//get true packet pointer
	return (pck->pck->info.flags & GF_PCK_CKTYPE_MASK) >> GF_PCK_CKTYPE_POS;
}

GF_EXPORT
GF_FilterFrameInterface *gf_filter_pck_get_frame_interface(GF_FilterPacket *pck)
{
	assert(pck);
	return pck->pck->frame_ifce;
}

GF_EXPORT
GF_Err gf_filter_pck_expand(GF_FilterPacket *pck, u32 nb_bytes_to_add, u8 **data_start, u8 **new_range_start, u32 *new_size)
{
	assert(pck);
	if (PCK_IS_INPUT(pck)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to reallocate input packet on output PID in filter %s\n", pck->pid->filter->name));
		return GF_BAD_PARAM;
	}
	if (! pck->src_filter) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to reallocate an already sent packet in filter %s\n", pck->pid->filter->name));
		return GF_BAD_PARAM;
	}
	if (pck->filter_owns_mem) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to reallocate a shared memory packet in filter %s\n", pck->pid->filter->name));
		return GF_BAD_PARAM;
	}
	if (pck->data_length + nb_bytes_to_add > pck->alloc_size) {
		pck->alloc_size = pck->data_length + nb_bytes_to_add;
		pck->data = gf_realloc(pck->data, pck->alloc_size);
#ifdef GPAC_MEMORY_TRACKING
		pck->pid->filter->session->nb_realloc_pck++;
#endif
	}
	pck->info.byte_offset = GF_FILTER_NO_BO;
	*data_start = pck->data;
	*new_range_start = pck->data + pck->data_length;
	pck->data_length += nb_bytes_to_add;
	*new_size = pck->data_length;


	return GF_OK;
}

GF_EXPORT
GF_Err gf_filter_pck_truncate(GF_FilterPacket *pck, u32 size)
{
	assert(pck);
	if (PCK_IS_INPUT(pck)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to truncate input packet on output PID in filter %s\n", pck->pid->filter->name));
		return GF_BAD_PARAM;
	}
	if (! pck->src_filter) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt to truncate an already sent packet in filter %s\n", pck->pid->filter->name));
		return GF_BAD_PARAM;
	}
	if (pck->data_length > size) pck->data_length = size;
	return GF_OK;
}

GF_EXPORT
Bool gf_filter_pck_is_blocking_ref(GF_FilterPacket *pck)
{
	pck = pck->pck;

	while (pck) {
		if (pck->destructor && pck->filter_owns_mem) return GF_TRUE;
		if (pck->frame_ifce && pck->frame_ifce->blocking) return GF_TRUE;
		pck = pck->reference;
	}
	return GF_FALSE;
}

