/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / BIFS decoder module
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
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

#include <gpac/filters.h>
#include <gpac/bifs.h>
#include <gpac/constants.h>
#include <gpac/compositor.h>
#include <gpac/internal/terminal_dev.h>

#ifndef GPAC_DISABLE_BIFS

GF_Err compose_bifs_dec_config_input(GF_Scene *scene, GF_FilterPid *pid, u32 oti, Bool is_remove)
{
	GF_Err e;
	const GF_PropertyValue *prop;
	Bool in_iod = GF_FALSE;
	u32 es_id = 0;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	if (prop) es_id = prop->value.uint;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_IN_IOD);
	if (prop && prop->value.boolean) in_iod = GF_TRUE;

	//we must have a dsi
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!prop || !prop->value.data || !prop->data_len) {
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	if (in_iod && !scene->bifs_dec)
		scene->bifs_dec = gf_bifs_decoder_new(scene->graph, GF_FALSE);

	e = gf_bifs_decoder_configure_stream(scene->bifs_dec, es_id, prop->value.data, prop->data_len, oti);
	if (e) return e;

	if (in_iod) {
		gf_filter_pid_set_udta(pid, scene->root_od);

		//setup object (clock) and playback requests
		gf_odm_register_pid(scene->root_od, pid, GF_FALSE);
	}
	//animation stream object
	else {
		if (!scene->bifs_dec) {
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		gf_scene_insert_pid(scene, scene->root_od->scene_ns, pid);
	}
	return e;
}

GF_Err compose_bifs_dec_process(GF_Scene *scene, GF_FilterPid *pid)
{
	GF_Err e;
	Double ts_offset;
	u64 now, cts;
	u32 obj_time;
	const char *data;
	u32 size, ESID=0;
	const GF_PropertyValue *prop;
	GF_FilterPacket *pck;
	GF_ObjectManager *odm = gf_filter_pid_get_udta(pid);

	//object not yet setup - FILTER_FIXME: we should not need this test once play/stop requests
	//are done, but for now the streams play by default
	if (!odm->ck) return GF_OK;
	assert(odm->ck);

	pck = gf_filter_pid_get_packet(pid);
	if (!pck) return GF_OK;

	data = gf_filter_pck_get_data(pck, &size);
	if (!data) {
		Bool is_eos = gf_filter_pck_get_eos(pck);
		//for now we retain the last packet to keep the filter active, but this must be controlled
		//player mode vs rip mode
		gf_filter_pid_drop_packet(pid);
		return is_eos ? GF_EOS : GF_NON_COMPLIANT_BITSTREAM;
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	if (prop) ESID = prop->value.uint;

	cts = gf_filter_pck_get_cts( pck );
	ts_offset = (Double) cts;
	ts_offset /= gf_filter_pck_get_timescale(pck);

	gf_odm_check_buffering(odm, pid);


	//we still process any frame before our clock time even when buffering
	obj_time = gf_clock_time(odm->ck);
	if (ts_offset * 1000 > obj_time) {
		u32 wait_ms = (u32) (ts_offset * 1000 - obj_time);
		if (!scene->compositor->ms_until_next_frame || (wait_ms<scene->compositor->ms_until_next_frame))
			scene->compositor->ms_until_next_frame = wait_ms;
//		fprintf(stderr, "CTS "LLU" vs time %u - return\n", cts, obj_time);
		return GF_OK;
	}

	now = gf_sys_clock_high_res();
	e = gf_bifs_decode_au(scene->bifs_dec, ESID, data, size, ts_offset);
	now = gf_sys_clock_high_res() - now;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[BIFS] ODM%d #CH%d at %d decoded AU TS %u in "LLU" us\n", odm->ID, ESID, obj_time, cts, now));

	gf_filter_pid_drop_packet(pid);

	if (e) return e;
	gf_scene_attach_to_compositor(scene);

	return GF_OK;
}

#else
GF_Err compose_bifs_dec_config_input(GF_Scene *scene, GF_FilterPid *pid, u32 oti, Bool is_remove)
{
	return GF_NOT_SUPPORTED;
}
GF_Err compose_bifs_dec_process(GF_Scene *scene, GF_FilterPid *pid)
{
	return GF_NOT_SUPPORTED;
}

#endif /*GPAC_DISABLE_BIFS*/



