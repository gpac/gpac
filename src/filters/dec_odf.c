/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / OD decoder filter
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

#include <gpac/internal/compositor_dev.h>
#include <gpac/constants.h>
#include <gpac/compositor.h>

#ifndef GPAC_DISABLE_PLAYER

typedef struct
{
	GF_ObjectManager *odm;
	GF_Scene *scene;
	Bool is_playing;
	GF_FilterPid *out_pid;
} GF_ODFDecCtx;


GF_Err odf_dec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_ODFDecCtx *ctx = gf_filter_get_udta(filter);
	Bool in_iod = GF_FALSE;
	GF_FilterPid *out_pid;
	const GF_PropertyValue *prop;

	//we must have streamtype SCENE
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!prop || (prop->value.uint != GF_STREAM_OD)) {
		return GF_NOT_SUPPORTED;
	}
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!prop || ( (prop->value.uint != GF_CODECID_OD_V1) && (prop->value.uint != GF_CODECID_OD_V2)) ) {
		return GF_NOT_SUPPORTED;
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_IN_IOD);
	if (prop && prop->value.boolean) in_iod = GF_TRUE;
	if (! in_iod) {
		return GF_NOT_SUPPORTED;
	}

	if (is_remove) {
		out_pid = gf_filter_pid_get_udta(pid);
		if (out_pid==ctx->out_pid)
			ctx->out_pid = NULL;
		if (out_pid)
			gf_filter_pid_remove(out_pid);
		return GF_OK;
	}

	//this is a reconfigure
	if (gf_filter_pid_get_udta(pid)) {
		return GF_OK;
	}

	//check our namespace
	if (ctx->scene && ! gf_filter_pid_is_filter_in_parents(pid, ctx->scene->root_od->scene_ns->source_filter)) {
		return GF_REQUIRES_NEW_INSTANCE;
	}

	//declare a new output PID of type scene, codecid RAW
	out_pid = gf_filter_pid_new(filter);

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(out_pid, pid);
	gf_filter_pid_set_property(out_pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
	gf_filter_pid_set_udta(pid, out_pid);
	if (!ctx->out_pid)
		ctx->out_pid = out_pid;
	return GF_OK;
}

static void attach_desc_to_odm(GF_ObjectManager *odm, GF_ObjectDescriptor *od)
{
	if (odm->OD) {
		gf_odf_desc_del((GF_Descriptor *)odm->OD);
		odm->OD = NULL;
	}
	if (gf_list_count(od->OCIDescriptors)) {
		char *url = od->URLString;
		od->URLString = NULL;
		gf_odf_desc_copy((GF_Descriptor *) od, (GF_Descriptor **) &odm->OD);
		od->URLString = url;
	}

}
void ODS_SetupOD(GF_Scene *scene, GF_ObjectDescriptor *od)
{
	u32 i, j, count, nb_scene, nb_od, nb_esd;
	GF_ESD *esd;
	GF_ObjectManager *odm;

	if (od->URLString) {
		odm = gf_odm_new();
		odm->ID = od->objectDescriptorID;
		odm->parentscene = scene;
		if (od->fake_remote)
			odm->ignore_sys = GF_TRUE;

		attach_desc_to_odm(odm, od);

		gf_list_add(scene->resources, odm);
		if (odm->ID != GF_MEDIA_EXTERNAL_ID) {
			count = gf_list_count(scene->scene_objects);
			for (i=0; i<count; i++) {
				GF_MediaObject *mo = gf_list_get(scene->scene_objects, i);
				if (mo->OD_ID != odm->ID) continue;
				if (mo->type == GF_MEDIA_OBJECT_SCENE) {
					odm->subscene = gf_scene_new(scene->compositor, scene);
					odm->subscene->is_dynamic_scene = GF_TRUE;
					odm->subscene->root_od = odm;
					odm->mo = mo;
					mo->odm = odm;
					break;
				}
			}
		}
		gf_odm_setup_remote_object(odm, scene->root_od->scene_ns, od->URLString, GF_FALSE);
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
		Bool skip_od = GF_FALSE;
		GF_FilterPid *pid = NULL;
		esd = gf_list_get(od->ESDescriptors, j);

		count = gf_list_count(scene->resources);
		for (i=0; i<count; i++) {
			u32 k=0;
			GF_ODMExtraPid *xpid;
			odm = gf_list_get(scene->resources, i);
			//can happen with interaction and scene streams
			if (!odm->pid) {
				if (odm->mo && odm->mo->OD_ID == od->objectDescriptorID) {
					odm->ServiceID = od->ServiceID;
					attach_desc_to_odm(odm, od);
					skip_od = GF_TRUE;
					break;
				}
				odm = NULL;
				continue;
			}
			
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
		if (skip_od)
			continue;

		//OCR streams and input sensors don't have PIDs associated for now (only local sensors supported)
		if ((esd->decoderConfig->streamType == GF_STREAM_INTERACT)
			|| (esd->decoderConfig->streamType == GF_STREAM_OCR)
		) {
#ifndef GPAC_DISABLE_VRML
			//first time we setup this stream, create an ODM
			if (!odm) {
				odm = gf_odm_new();
				odm->type = GF_STREAM_INTERACT;
				odm->parentscene = scene;
				odm->ID = od->objectDescriptorID;
				odm->pid_id = esd->ESID;
				odm->ck = scene->root_od->ck;
				odm->scene_ns = scene->root_od->scene_ns;
				odm->scene_ns->nb_odm_users++;
				gf_list_add(scene->resources, odm);
			}
			attach_desc_to_odm(odm, od);

			if (esd->decoderConfig->streamType == GF_STREAM_INTERACT) {
				gf_scene_setup_object(scene, odm);
				gf_input_sensor_setup_object(odm, esd);
			}
			else if (esd->decoderConfig->streamType == GF_STREAM_OCR) {
				odm->mo = gf_mo_new();
				odm->mo->odm = odm;
				odm->mo->OD_ID = od->objectDescriptorID;
				odm->mo->type = GF_MEDIA_OBJECT_UNDEF;
				gf_list_add(scene->scene_objects, odm->mo);

				gf_clock_set_time(odm->ck, 0, 1000);
			}
#endif
			return;
		} else if (!odm || !pid ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("Cannot match OD ID %d to any PID in the service, ignoring OD\n", od->objectDescriptorID));
			return;
		}

		/*if there is BIFS and OD streams in the OD, we need an GF_Scene (except if we already
		have one, which means this is the first IOD)*/
		if (nb_scene && nb_od && !odm->subscene) {
			odm->subscene = gf_scene_new(NULL, odm->parentscene);
			odm->subscene->root_od = odm;
		}

		odm->ID = od->objectDescriptorID;
		if (odm->mo && (odm->mo->OD_ID != odm->ID)) {
			odm->mo->OD_ID = odm->ID;

		}
		odm->ServiceID = od->ServiceID;
		attach_desc_to_odm(odm, od);

		/*setup PID for this object */
		gf_odm_setup_object(odm, scene->root_od->scene_ns, pid);

	}
}

static GF_Err ODS_ODUpdate(GF_Scene *scene, GF_ODUpdate *odU)
{
	u32 i, count;

	/*extract all our ODs and compare with what we already have...*/
	count = gf_list_count(odU->objectDescriptors);
	if (count > 255) return GF_ODF_INVALID_DESCRIPTOR;

	for (i=0; i<count; i++) {
		GF_ObjectDescriptor *od = (GF_ObjectDescriptor *)gf_list_get(odU->objectDescriptors, i);
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
	//no more support for ESD update in new arch (never used anyway)
	//following is kept for the case we need to reintroduce it one day
#if 0
	GF_ObjectManager *odm;
	u32 count, i;

	odm = gf_scene_find_odm(priv->scene, ESDs->ODID);
	/*spec: "ignore"*/
	if (!odm) return GF_OK;

	count = gf_list_count(ESDs->ESDescriptors);

	while (count) {
		GF_ESD *esd, *prev;
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

GF_Err odf_dec_process(GF_Filter *filter)
{
	GF_Err e;
	GF_ODCom *com;
	GF_ODCodec *oddec;
	u64 cts, now;
	u32 count, i;
	const char *data;
	u32 size, ESID=0;
	const GF_PropertyValue *prop;
	GF_ODFDecCtx *ctx = gf_filter_get_udta(filter);

	if (!ctx->scene) {
		if (ctx->is_playing) {
			gf_filter_pid_set_eos(ctx->out_pid);
			return GF_EOS;
		}
		return GF_OK;
	}

	count = gf_filter_get_ipid_count(filter);
	for (i=0; i<count; i++) {
		GF_Scene *scene;
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		GF_FilterPid *opid = gf_filter_pid_get_udta(pid);
		GF_ObjectManager *odm = gf_filter_pid_get_udta(opid);
		if (!odm) continue;

		GF_FilterPacket *pck = gf_filter_pid_get_packet(pid);
		if (!pck) {
			Bool is_eos = gf_filter_pid_is_eos(pid);
			if (is_eos)
				gf_filter_pid_set_eos(opid);
			continue;
		}
		data = gf_filter_pck_get_data(pck, &size);
		if (!data) {
			gf_filter_pid_drop_packet(pid);
			continue;
		}
		scene = odm->subscene;

		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (prop) ESID = prop->value.uint;

		cts = gf_filter_pck_get_cts( pck );
		cts = gf_timestamp_to_clocktime(cts, gf_filter_pck_get_timescale(pck));

		if (!gf_sc_check_sys_frame(scene, odm, pid, filter, cts))
			continue;

		now = gf_sys_clock_high_res();
		oddec = gf_odf_codec_new();

		e = gf_odf_codec_set_au(oddec, data, size);
		if (!e) e = gf_odf_codec_decode(oddec);

		gf_filter_pid_drop_packet(pid);

		//3- process all the commands in this AU, in order
		while (e == GF_OK) {
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
		}

		gf_odf_codec_del(oddec);

		now = gf_sys_clock_high_res() - now;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[ODF] ODM%d #CH%d decoded AU TS %u in "LLU" us\n", odm->ID, ESID, cts, now));
	}

	return GF_OK;
}



static Bool odf_dec_process_event(GF_Filter *filter, const GF_FilterEvent *com)
{
	u32 count, i;
	GF_ODFDecCtx *ctx = gf_filter_get_udta(filter);

	switch (com->base.type) {
	case GF_FEVT_ATTACH_SCENE:
		break;
	case GF_FEVT_PLAY:
		ctx->is_playing = GF_TRUE;
		return GF_FALSE;
	default:
		return GF_FALSE;
	}
	if (!com->attach_scene.on_pid) return GF_TRUE;

	count = gf_filter_get_ipid_count(filter);
	//attach inline scenes
	for (i=0; i<count; i++) {
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		GF_FilterPid *opid = gf_filter_pid_get_udta(ipid);
		//we found our pid, set it up
		if (opid == com->attach_scene.on_pid) {
			if (!ctx->odm) {
				ctx->odm = com->attach_scene.object_manager;
				ctx->scene = ctx->odm->subscene ? ctx->odm->subscene : ctx->odm->parentscene;
			}
			gf_filter_pid_set_udta(opid, com->attach_scene.object_manager);
			return GF_TRUE;
		}
	}

	return GF_TRUE;
}

static const GF_FilterCapability ODFDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_OD_V1),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_OD_V2),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister ODFDecRegister = {
	.name = "odfdec",
	GF_FS_SET_DESCRIPTION("MPEG-4 OD decoder")
	GF_FS_SET_HELP("This filter decodes MPEG-4 OD binary frames directly into the scene manager of the compositor.\n"
	"Note: This filter cannot be used to dump OD content to text or xml, use `MP4Box` for that.")
	.private_size = sizeof(GF_ODFDecCtx),
	.flags = GF_FS_REG_MAIN_THREAD,
	.priority = 1,
	SETCAPS(ODFDecCaps),
	.process = odf_dec_process,
	.configure_pid = odf_dec_configure_pid,
	.process_event = odf_dec_process_event,
};

const GF_FilterRegister *odf_dec_register(GF_FilterSession *session)
{
	return &ODFDecRegister;
}
#else
const GF_FilterRegister *odf_dec_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_PLAYER
