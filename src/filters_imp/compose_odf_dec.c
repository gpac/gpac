/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / OD decoder module
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

#include <gpac/internal/terminal_dev.h>
#include <gpac/constants.h>
#include <gpac/compositor.h>
#include <gpac/internal/terminal_dev.h>


GF_Err compose_odf_dec_config_input(GF_Scene *scene, GF_FilterPid *pid, u32 oti, Bool is_remove)
{
	const GF_PropertyValue *prop;
	Bool in_iod = GF_FALSE;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_IN_IOD);
	if (prop && prop->value.boolean) in_iod = GF_TRUE;

	if (! in_iod) {
		return GF_NOT_SUPPORTED;
	}

	//setup object (clock) and playback requests
	gf_scene_insert_pid(scene, scene->root_od->scene_ns, pid, GF_TRUE);
	return GF_OK;
}

static void ODS_SetupOD(GF_Scene *scene, GF_ObjectDescriptor *od)
{
	u32 i, j, count, nb_scene, nb_od, nb_esd;
	GF_ESD *esd;
	GF_ObjectManager *odm;
	odm = gf_scene_find_odm(scene, od->objectDescriptorID);
	/*remove the old OD*/
	if (odm) gf_odm_disconnect(odm, 1);

	if (od->URLString) {
		odm = gf_odm_new();
		odm->ID = od->objectDescriptorID;
		odm->parentscene = scene;
		odm->scene_ns = scene->root_od->scene_ns;
		gf_list_add(scene->resources, odm);
		gf_odm_setup_remote_object(odm, scene->root_od->scene_ns, od->URLString);
		return;
	}

	odm = NULL;
	nb_esd = gf_list_count(od->ESDescriptors);
	nb_scene = nb_od = 0;
	for (i=0; i<nb_esd; i++) {
		esd = gf_list_get(od->ESDescriptors, i);
		if (esd->decoderConfig && (esd->decoderConfig->streamType==GF_STREAM_SCENE)) nb_scene++;
		else if (esd->decoderConfig && (esd->decoderConfig->streamType==GF_STREAM_OD)) nb_od++;
	}

	for (j=0; j<nb_esd; j++) {
		GF_FilterPid *pid = NULL;
		esd = gf_list_get(od->ESDescriptors, j);
		count = gf_list_count(scene->resources);
		for (i=0; i<count; i++) {
			u32 k=0;
			GF_ODMExtraPid *xpid;
			odm = gf_list_get(scene->resources, i);
			assert(odm->pid);

			if (odm->pid_id == esd->ESID) {
				pid = odm->pid;
				break;
			}
			while ( (xpid = gf_list_enum(odm->extra_pids, &k))) {
				if (xpid->pid_id == esd->ESID) {
					pid = odm->pid;
					break;
				}
			}
			if (pid) break;
			odm = NULL;
		}
		if (!odm || !pid ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Cannot match OD ID %d to any PID in the service, ignoring OD\n", od->objectDescriptorID));
			return;
		}

		/*if there is BIFS and OD streams in the OD, we need an GF_Scene (except if we already
		have one, which means this is the first IOD)*/
		if (nb_scene && nb_od && !odm->subscene) {
			odm->subscene = gf_scene_new(NULL, odm->parentscene);
			odm->subscene->root_od = odm;
		}

		odm->ID = od->objectDescriptorID;
		odm->ServiceID = od->ServiceID;

		/*setup PID for this object */
		gf_odm_setup_object(odm, scene->root_od->scene_ns, pid);
	}
}

static GF_Err ODS_ODUpdate(GF_Scene *scene, GF_ODUpdate *odU)
{
	GF_ObjectDescriptor *od;
	u32 i, count;

	/*extract all our ODs and compare with what we already have...*/
	count = gf_list_count(odU->objectDescriptors);
	if (count > 255) return GF_ODF_INVALID_DESCRIPTOR;

	for (i=0; i<count; i++) {
		od = (GF_ObjectDescriptor *)gf_list_get(odU->objectDescriptors, i);
		ODS_SetupOD(scene, od);
	}
	return GF_OK;
}



static GF_Err ODS_RemoveOD(GF_Scene *scene, GF_ODRemove *odR)
{
	u32 i;
	for (i=0; i< odR->NbODs; i++) {
		GF_ObjectManager *odm = gf_scene_find_odm(scene, odR->OD_ID[i]);
		if (odm) gf_odm_disconnect(odm, 1);
	}
	return GF_OK;
}

static GF_Err ODS_UpdateESD(GF_Scene *scene, GF_ESDUpdate *ESDs)
{
#if FILTER_FIXME
	GF_ESD *esd, *prev;
	GF_ObjectManager *odm;
	u32 count, i;

	odm = gf_scene_find_odm(priv->scene, ESDs->ODID);
	/*spec: "ignore"*/
	if (!odm) return GF_OK;

	count = gf_list_count(ESDs->ESDescriptors);

	while (count) {
		esd = (GF_ESD*)gf_list_get(ESDs->ESDescriptors, 0);
		/*spec: "ES_Descriptors with ES_IDs that have already been received within the same name scope shall be ignored."*/
		prev = NULL;
		i=0;
		while ((prev = (GF_ESD*)gf_list_enum(odm->OD->ESDescriptors, &i))) {
			if (prev->ESID == esd->ESID) break;
		}
		if (prev) {
			gf_odf_desc_del((GF_Descriptor *)esd);
		} else {
			/*and register new stream*/
			gf_list_add(odm->OD->ESDescriptors, esd);
			gf_odm_setup_es(odm, esd, odm->net_service, NULL);
		}

		/*remove the desc from the AU*/
		gf_list_rem(ESDs->ESDescriptors, 0);
		count--;
	}
	/*resetup object since a new ES has been inserted
	(typically an empty object first sent, then a stream added - cf how ogg demuxer works)*/
	gf_scene_setup_object(priv->scene, odm);
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}


static GF_Err ODS_RemoveESD(GF_Scene *scene, GF_ESDRemove *ESDs)
{
	GF_ObjectManager *odm = gf_scene_find_odm(scene, ESDs->ODID);
	/*spec: "ignore"*/
	if (!odm) return GF_OK;

	return GF_NOT_SUPPORTED;
	//todo one day (never used before in MPEG-4 systems): find the stream with the given ESID
	//in the parent filter, and remove it
}

GF_Err compose_odf_dec_process(GF_Scene *scene, GF_FilterPid *pid)
{
	GF_Err e;
	GF_ODCom *com;
	GF_ODCodec *oddec;
	Double ts_offset;
	u64 cts, now;
	u32 obj_time;
	const char *data;
	u32 size, ESID=0;
	const GF_PropertyValue *prop;
	GF_ObjectManager *odm = gf_filter_pid_get_udta(pid);

	GF_FilterPacket *pck = gf_filter_pid_get_packet(pid);
	if (!pck) return GF_OK;

	data = gf_filter_pck_get_data(pck, &size);
	if (!data) {
		Bool is_eos = gf_filter_pck_get_eos(pck);
		//for now we retain the last packet to keep the filter active, but this must be controlled
		//player mode vs rip mode
		//gf_filter_pid_drop_packet(pid);
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
	if (ts_offset * 1000 > obj_time)
		return GF_OK;

	now = gf_sys_clock_high_res();
	oddec = gf_odf_codec_new();

	e = gf_odf_codec_set_au(oddec, data, size);
	if (!e) e = gf_odf_codec_decode(oddec);

	gf_filter_pid_drop_packet(pid);

	if (e) goto err_exit;

	//3- process all the commands in this AU, in order
	while (1) {
		com = gf_odf_codec_get_com(oddec);
		if (!com) break;

		//ok, we have a command
		switch (com->tag) {
		case GF_ODF_OD_UPDATE_TAG:
			e = ODS_ODUpdate(scene, (GF_ODUpdate *) com);
			break;
		case GF_ODF_OD_REMOVE_TAG:
			e = ODS_RemoveOD(scene, (GF_ODRemove *) com);
			break;
		case GF_ODF_ESD_UPDATE_TAG:
			e = ODS_UpdateESD(scene, (GF_ESDUpdate *)com);
			break;
		case GF_ODF_ESD_REMOVE_TAG:
			e = ODS_RemoveESD(scene, (GF_ESDRemove *)com);
			break;
		case GF_ODF_IPMP_UPDATE_TAG:
#if 0
		{
			GF_IPMPUpdate *ipmpU = (GF_IPMPUpdate *)com;
			while (gf_list_count(ipmpU->IPMPDescList)) {
				GF_IPMP_Descriptor *ipmp = gf_list_get(ipmpU->IPMPDescList, 0);
				gf_list_rem(ipmpU->IPMPDescList, 0);
				IS_UpdateIPMP(priv->scene, ipmp);
			}
			e = GF_OK;
		}
#else
		e = GF_OK;
#endif
		break;
		case GF_ODF_IPMP_REMOVE_TAG:
			e = GF_NOT_SUPPORTED;
			break;
		/*should NEVER exist outside the file format*/
		case GF_ODF_ESD_REMOVE_REF_TAG:
			e = GF_NON_COMPLIANT_BITSTREAM;
			break;
		default:
			if (com->tag >= GF_ODF_COM_ISO_BEGIN_TAG && com->tag <= GF_ODF_COM_ISO_END_TAG) {
				e = GF_ODF_FORBIDDEN_DESCRIPTOR;
			} else {
				/*we don't process user commands*/
				e = GF_OK;
			}
			break;
		}
		gf_odf_com_del(&com);
		if (e) break;
	}

err_exit:
	gf_odf_codec_del(oddec);

	now = gf_sys_clock_high_res() - now;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[ODF] ODM%d #CH%d at %d decoded AU TS %u in "LLU" us\n", odm->ID, ESID, obj_time, cts, now));


	return e;
}

